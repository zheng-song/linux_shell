/*
 * function.c
 *
 *  Created on: 2017-6-8
 *      Author: sfh
 */

#include "ttyusb.h"

/*typematic definitions*/
#define TYPEMATIC_NAME		 	"tUsbtty"
#define USB_TTY_CLIENT_NAME 	"usbttyLib"    /* our USBD client name */

LOCAL UINT16 initCount = 0;	/* Count of init nesting */
LOCAL LIST_HEAD sioList;	/* linked list of USB_KBD_SIO_CHAN */
LOCAL LIST_HEAD reqList;	/* Attach callback request list */
LOCAL MUTEX_HANDLE usbttyMutex;    /* mutex used to protect internal structs */
LOCAL USBD_CLIENT_HANDLE usbdHandle; /* our USBD client handle */
LOCAL THREAD_HANDLE typematicHandle;/* task used to generate typematic repeat */
LOCAL BOOL killTypematic;	/* TRUE when typematic thread should exit */
LOCAL BOOL typematicExit;	/* TRUE when typematic thread exits */


typedef struct attach_request{
	LINK reqLink;						/*linked list of requests*/
	USB_TTY_ATTACH_CALLBACK callback; 	/*client callback routine*/
	pVOID callbackArg;  				/*client callback argument*/
}ATTACH_REQUEST,*pATTACH_REQUEST;


typedef struct usb_tty_sio_chan{
	SIO_CHAN sioChan;		/* must be first field */
	LINK sioLink;	        /* linked list of keyboard structs */
	UINT16 lockCount;		/* Count of times structure locked */
	USBD_NODE_ID nodeId;	/* keyboard node Id */
	UINT16 configuration;	/* configuration/interface reported as */
	UINT16 interface;		/* a keyboard by this device */
	BOOL connected;	        /* TRUE if keyboard currently connected */
	USBD_PIPE_HANDLE pipeHandle;/* USBD pipe handle for interrupt pipe */
	USB_IRP irp;	        /* IRP to monitor interrupt pipe */
	BOOL irpInUse;	        /* TRUE while IRP is outstanding */
	pHID_KBD_BOOT_REPORT pBootReport;/* Keyboard boot report fetched thru pipe */
//	char inQueue [KBD_Q_DEPTH]; /* Circular queue for keyboard input */
	UINT16 inQueueCount;	/* count of characters in input queue */
	UINT16 inQueueIn;		/* next location in queue */
	UINT16 inQueueOut;		/* next character to fetch */

	UINT32 typematicTime;	/* time current typematic period started */
	UINT32 typematicCount;	/* count of keys injected */
	UINT16 typematicChar; 	/* current character to repeat */

	int mode;		        /* SIO_MODE_INT or SIO_MODE_POLL */

	STATUS (*getTxCharCallback) (); /* tx callback */
	void *getTxCharArg; 	/* tx callback argument */

	STATUS (*putRxCharCallback) (); /* rx callback */
	void *putRxCharArg; 	/* rx callback argument */

	/* Following variables used to emulate certain ioctl functions. */
	int baudRate;	        /* has no meaning in USB */
	/* Following variables maintain keyboard state */
	BOOL capsLock;	        /* TRUE if CAPLOCK in effect */
	BOOL scrLock;	        /* TRUE if SCRLOCK in effect */
	BOOL numLock;	        /* TRUE if NUMLOCK in effect */
//	UINT16 activeScanCodes [BOOT_RPT_KEYCOUNT];
//	int scanMode;		/* raw or ascii */
}USB_TTY_SIO_CHAN, *pUSB_TTY_SIO_CHAN;

/*LOCAL VOID updateTypematic(pUSB_TTY_SIO_CHAN pSioChan)
{
	UINT32 diffTime;
	UINT32 repeatCount;
	 If the given channel is active and a typematic character is
	 * indicated, then update the typematic state.

	if (pSioChan->connected && pSioChan->typematicChar != 0){
		diffTime = OSS_TIME () - pSioChan->typematicTime;
		 If the typematic delay has passed, then it is time to start
		 * injecting characters into the queue.

		if (diffTime >= TYPEMATIC_DELAY){
			diffTime -= TYPEMATIC_DELAY;
			repeatCount = diffTime / TYPEMATIC_PERIOD + 1;

		 Inject characters into the queue.  If the queue is
		 * full, putInChar() dumps the character, but we increment
		 * the typematicCount anyway.  This keeps the queue from
		 * getting too far ahead of the user.

			while (repeatCount > pSioChan->typematicCount){
				if( ISEXTENDEDKEYCODE(pSioChan->typematicChar) ){
					if(pSioChan->inQueueCount < KBD_Q_DEPTH-1){
						putInChar (pSioChan, (char) 0);
						putInChar (pSioChan, (char) pSioChan->typematicChar & 0xFF);
					}
				}else{
					putInChar (pSioChan, pSioChan->typematicChar);
				}
				pSioChan->typematicCount++;
			}
 invoke receive callback
			while (pSioChan->inQueueCount > 0 &&
					pSioChan->putRxCharCallback != NULL &&
					pSioChan->mode == SIO_MODE_INT){
				(*pSioChan->putRxCharCallback) (pSioChan->putRxCharArg,
						nextInChar (pSioChan));
			}
		}
	}
}*/
unsigned int TYPEMATIC_DELAY  = 500;    /* 500 msec delay */
unsigned int TYPEMATIC_PERIOD = 66;     /* 66 msec = approx 15 char/sec */

LOCAL VOID typematicThread(
		pVOID param/*param not used by this thread*/
){
	pUSB_TTY_SIO_CHAN pSioChan;
	while(!killTypematic){
		OSS_MUTEX_TAKE(usbttyMutex,OSS_BLOCK);
		pSioChan = usbListFirst(&sioList);

		while(pSioChan != NULL){
//			updateTypematic(pSioChan);
			pSioChan = usbListNext(&pSioChan->sioLink);
		}

		OSS_MUTEX_RELEASE(usbttyMutex);
		OSS_THREAD_SLEEP(TYPEMATIC_PERIOD);
	}
	typematicExit = TRUE;
}


/*LOCAL STATUS doShutdown(int errCode)
{
	pATTACH_REQUEST pRequest;
	pUSB_TTY_SIO_CHAN pSioChan;

	Kill typematic thread
	if(typematicThread != NULL){
		killTypeMatic = TRUE;
	}
}*/

//========================================================================================//
LOCAL pUSB_TTY_SIO_CHAN findSioChan(USBD_NODE_ID nodeId)
{
	pUSB_TTY_SIO_CHAN pSioChan = usbListFirst(&sioList);

	while(pSioChan != NULL){
		if(pSioChan->nodeId == nodeId) break;

		pSioChan = usbListNext(&pSioChan->sioLink);
	}
	return pSioChan;
}
//========================================================================================//
LOCAL BOOL configureSioChan(pUSB_TTY_SIO_CHAN pSioChan)
{
	pUSB_CONFIG_DESCR pCfgDescr;
	pUSB_INTERFACE_DESCR pIfDescr;
	pUSB_ENDPOINT_DESCR pEpDescr;
	UINT8 * pBfr;
	UINT8 * pScratchBfr;
	UINT16 actLen;
	UINT16 ifNo;
//	UINT16 maxPacketSize;

	if((pBfr = OSS_MALLOC(USB_MAX_DESCR_LEN))== NULL)
		return FALSE;

	/* Read the configuration descriptor to get the configuration selection
	 * value and to determine the device's power requirements.*/
	if (usbdDescriptorGet (usbdHandle, pSioChan->nodeId,USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,
			 0, 0,USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
		OSS_FREE (pBfr);
		return FALSE;
	}

	if ((pCfgDescr = usbDescrParse (pBfr, actLen, USB_DESCR_CONFIGURATION))== NULL){
		OSS_FREE (pBfr);
		return FALSE;
	}

	/* Look for the interface indicated in the pSioChan structure. */
	ifNo = 0;

	/*  usbDescrParseSkip() modifies the value of the pointer it recieves
	 * so we pass it a copy of our buffer pointer */
	pScratchBfr = pBfr;
	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,USB_DESCR_INTERFACE))!= NULL){
		if (ifNo == pSioChan->interface) break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		OSS_FREE (pBfr);
		return FALSE;
	}

	/* Retrieve the endpoint descriptor following the identified interface descriptor.*/
	if ((pEpDescr = usbDescrParseSkip (&pScratchBfr,&actLen,USB_DESCR_ENDPOINT))== NULL){
		OSS_FREE (pBfr);
	   	return FALSE;
	}

	printf("configurationValue is :%d",pCfgDescr->configurationValue);
	/* Select the configuration. */
	if (usbdConfigurationSet (usbdHandle,pSioChan->nodeId,
			pCfgDescr->configurationValue,pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT)!= OK){
		OSS_FREE (pBfr);
		return FALSE;
	}

	printf("interface ");
	/* Select interface
	 * NOTE: Some devices may reject this command, and this does not represent
	 * a fatal error.  Therefore, we ignore the return status.*/
	if(usbdInterfaceSet (usbdHandle,pSioChan->nodeId,pSioChan->interface,
			pIfDescr->alternateSetting) != OK)
		printf("usbdInterfaceSet Failed\n");
	printf("interface alternate setting is %d",pIfDescr->alternateSetting);

	/* Select the keyboard boot protocol. */
/*	if (usbHidProtocolSet (usbdHandle,pSioChan->nodeId,
			pSioChan->interface,USB_HID_PROTOCOL_BOOT)!= OK){
		OSS_FREE (pBfr);
		return FALSE;
	}*/

	/* Set the keyboard idle time to infinite. */
/*	if (usbHidIdleSet (usbdHandle,pSioChan->nodeId,pSioChan->interface,
			0  no report ID ,0  infinite )!= OK){
		OSS_FREE (pBfr);
		return FALSE;
	}*/

	/* Turn off LEDs. */
/*	setLedReport (pSioChan, 0);*/

	/* Create a pipe to monitor input reports from the keyboard. */

/*	maxPacketSize = *((pUINT8) &pEpDescr->maxPacketSize) |
			(*(((pUINT8) &pEpDescr->maxPacketSize) + 1) << 8);

	if (usbdPipeCreate (usbdHandle,pSioChan->nodeId,pEpDescr->endpointAddress,
			pCfgDescr->configurationValue,pSioChan->interface,USB_XFRTYPE_INTERRUPT,
			USB_DIR_IN,maxPacketSize,sizeof (HID_KBD_BOOT_REPORT),pEpDescr->interval,
			&pSioChan->pipeHandle)  != OK){
		OSS_FREE (pBfr);
		return FALSE;
	}*/

/* Initiate IRP to listen for input on interrupt pipe */
/*	if (!initKbdIrp (pSioChan)){
		OSS_FREE (pBfr);
		return FALSE;
	}*/

	OSS_FREE (pBfr);
	return TRUE;
}

//========================================================================================//
LOCAL VOID destroyAttachRequest(pATTACH_REQUEST pRequest){
	/*Unlink request*/
	usbListUnlinkProt(&pRequest->reqLink,usbttyMutex);
	/*Dispose request*/
	OSS_FREE(pRequest);
}



//================================================================================================//
LOCAL VOID destorySioChan(pUSB_TTY_SIO_CHAN pSioChan)
{
	/*Unlink the structure*/
	usbListUnlinkProt(&pSioChan->sioLink,usbttyMutex);

	/*Release pipe if one has been allocated.Wait for the IRP to be cancelled if necessary*/
	if(pSioChan->pipeHandle != NULL)
		usbdPipeDestroy(usbdHandle,pSioChan->pipeHandle);

	/* The following block is commented out to address the nonblocking
	 * issue of destroySioChan when the keyboard is removed and a read is
	 * in progress SPR #98731 */

	/*
	OSS_MUTEX_RELEASE (kbdMutex);

	while (pSioChan->irpInUse)
	OSS_THREAD_SLEEP (1);

	OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);
	*/

	/* Release structure. */
	if(!pSioChan->irpInUse){
		if(pSioChan->pBootReport != NULL)
			OSS_FREE(pSioChan->pBootReport);

		OSS_FREE(pSioChan);
	}
}

//================================================================================================//
LOCAL pUSB_TTY_SIO_CHAN createSioChan(USBD_NODE_ID nodeId,UINT16 configuration,UINT16 interface)
{
	pUSB_TTY_SIO_CHAN pSioChan;
//	UINT16 i;

	/* Try to allocate space for a new keyboard struct */
	if ((pSioChan = OSS_CALLOC(sizeof(*pSioChan))) == NULL)
		return NULL;

	if ((pSioChan->pBootReport= OSS_CALLOC(sizeof(*pSioChan->pBootReport)))== NULL){
		OSS_FREE (pSioChan);
		return NULL;
	}

//	pSioChan->sioChan.pDrvFuncs = &usbttySioDrvFuncs;
	pSioChan->nodeId = nodeId;
	pSioChan->connected = TRUE;
	/*pSioChan->mode = SIO_MODE_POLL;
	pSioChan->scanMode = SIO_KYBD_MODE_ASCII;*/

	pSioChan->configuration = configuration;
	pSioChan->interface = interface;

	/*for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
		pSioChan->activeScanCodes [i] = NOTKEY;*/

	/* Try to configure the usb_tty device. */
	if (!configureSioChan (pSioChan))
	{
	destorySioChan (pSioChan);
	return NULL;
	}


	/* Link the newly created structure. */

	usbListLinkProt (&sioList, pSioChan, &pSioChan->sioLink, LINK_TAIL,usbttyMutex);

	return pSioChan;

}

//===============================================================================================//
LOCAL VOID notifyAttach(pUSB_TTY_SIO_CHAN pSioChan,UINT16 attachCode)
{
	pATTACH_REQUEST pRequest = usbListFirst(&reqList);

    while (pRequest != NULL){
	(*pRequest->callback) (pRequest->callbackArg, (SIO_CHAN *) pSioChan, attachCode);
	pRequest = usbListNext (&pRequest->reqLink);
    }
}



//===============================================================================================//
LOCAL VOID usbttyAttachCallback( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,UINT16 interface,
		UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
	pUSB_TTY_SIO_CHAN pSioChan;
	OSS_MUTEX_TAKE(usbttyMutex,OSS_BLOCK);
	switch(attachAction){
	case USBD_DYNA_ATTACH:
		/* A new device is being attached.  Check if we already  have a structure for this device.*/
		if(findSioChan(nodeId)!= NULL) break;

	    /* Create a new structure to manage this device.  If there's an error,
	     * there's nothing we can do about it, so skip the device and return immediately. */
		if((pSioChan = createSioChan(nodeId,configuration,interface)) == NULL) break;

		/* Notify registered callers that a new keyboard has been added and a new channel created.*/
		notifyAttach (pSioChan, USB_TTY_ATTACH);
		break;

	case USBD_DYNA_REMOVE:
		 /* A device is being detached.	Check if we have any structures to manage this device.*/
		if ((pSioChan = findSioChan (nodeId)) == NULL) break;

		/* The device has been disconnected. */
		pSioChan->connected = FALSE;

		/* Notify registered callers that the keyboard has been removed and the channel disabled.
		 * NOTE: We temporarily increment the channel's lock count to prevent usbttySioChanUnlock() from destroying the
		 * structure while we're still using it.*/
		pSioChan->lockCount++;
		notifyAttach (pSioChan, USB_TTY_REMOVE);
		pSioChan->lockCount--;

		/* If no callers have the channel structure locked, destroy it now.
		 * If it is locked, it will be destroyed later during a call to usbttyUnlock().*/
		if (pSioChan->lockCount == 0)
		destorySioChan (pSioChan);
		break;
	}
	OSS_MUTEX_RELEASE(usbttyMutex);
}

//===============================================================================================//
STATUS usbttyDevInit (void)
{
/* If not already initialized,then initialize internal structures
 * and connection to USBD
 * */
	if(initCount == 0){
		memset(&sioList,0,sizeof(sioList));
		memset(&reqList,0,sizeof(reqList));
		usbttyMutex = NULL;
		usbdHandle = NULL;
		killTypematic  = FALSE;
		typematicExit = FALSE;
		if (OSS_MUTEX_CREATE (&usbttyMutex) != OK)
			return -1;
//			return doShutdown (S_usbKeyboardLib_OUT_OF_RESOURCES);

		/* Initialize typematic repeat thread */
		if (OSS_THREAD_CREATE (typematicThread, NULL, OSS_PRIORITY_LOW,
				TYPEMATIC_NAME, &typematicHandle) != OK)
			return -1;
//			return doShutdown (S_usbKeyboardLib_OUT_OF_RESOURCES);

			if (usbdClientRegister (USB_TTY_CLIENT_NAME, &usbdHandle) != OK || usbdDynamicAttachRegister (usbdHandle,
				USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbttyAttachCallback)!= OK)
				printf("usbd register failed!\n");
//				return doShutdown (S_usbKeyboardLib_USBD_FAULT);
			}
	initCount++;
	return OK;
}

//===============================================================================================//
STATUS usbttyDynamicAttachRegister(
		USB_TTY_ATTACH_CALLBACK callback,	/* new callback to be registered */
	    pVOID arg		    				/* user-defined arg to callback */
){
	pATTACH_REQUEST pRequest;
	pUSB_TTY_SIO_CHAN pSioChan;
	int status = OK;

	if (callback == NULL)
		return ERROR;
	OSS_MUTEX_TAKE (usbttyMutex, OSS_BLOCK);

	/* Create a new request structure to track this callback request. */
	if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
		status = ERROR;else{
			pRequest->callback = callback;
			pRequest->callbackArg = arg;
			usbListLinkProt (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL,usbttyMutex);
	/* Perform an initial notification of all currrently attached devices.*/
			pSioChan = usbListFirst (&sioList);
			while (pSioChan != NULL)
			{
				if (pSioChan->connected)(*callback) (arg, (SIO_CHAN *) pSioChan, USB_TTY_ATTACH);
				pSioChan = usbListNext (&pSioChan->sioLink);
			}
		}
	OSS_MUTEX_RELEASE (usbttyMutex);
	return ossStatus (status);
}

//===============================================================================================//
STATUS usbttyDynamicAttachUnRegister(
		USB_TTY_ATTACH_CALLBACK callback,	/* callback to be unregistered */
		pVOID arg		    				/* user-defined arg to callback */
){
    pATTACH_REQUEST pRequest;
    pUSB_TTY_SIO_CHAN pSioChan = NULL;
    int status = ERROR;

    OSS_MUTEX_TAKE (usbttyMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL){
	if (callback == pRequest->callback && arg == pRequest->callbackArg){
    	/* Perform a notification of all currrently attached devices.*/
	    pSioChan = usbListFirst (&sioList);

    	while (pSioChan != NULL){
	        if (pSioChan->connected)
		       (*callback) (arg, (SIO_CHAN *) pSioChan, USB_TTY_REMOVE);
  	        pSioChan = usbListNext (&pSioChan->sioLink);
    	}

	    /* We found a matching notification request. */
    	destroyAttachRequest (pRequest);
	    status = OK;
	    break;
	    }
	pRequest = usbListNext (&pRequest->reqLink);
    }

    OSS_MUTEX_RELEASE (usbttyMutex);

    return status;
    }

//===============================================================================================//
STATUS usbttySioChanLock(SIO_CHAN *pChan	/* SIO_CHAN to be marked as in use */)
{
	pUSB_TTY_SIO_CHAN pSioChan = (pUSB_TTY_SIO_CHAN) pChan;
	pSioChan->lockCount++;
    return OK;
}

//===============================================================================================//
STATUS usbttySioChanUnlock(SIO_CHAN *pChan	/* SIO_CHAN to be marked as unused */)
{
    pUSB_TTY_SIO_CHAN pSioChan = (pUSB_TTY_SIO_CHAN) pChan;
    int status = OK;

    OSS_MUTEX_TAKE (usbttyMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0)
    	status = ERROR;
    else{
	/* If this is the last lock and the underlying USB keyboard is
	 * no longer connected, then dispose of the keyboard.
	 */
	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
	    destorySioChan (pSioChan);
    }
    OSS_MUTEX_RELEASE (usbttyMutex);
    return ossStatus (status);
}



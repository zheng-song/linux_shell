/*
 * cp210xLib.c
 *
 *  Created on: 2017-6-12
 *      Author: sfh
 */
#include "cp210xLib.h"

#define CP210X_CLIENT_NAME 	"cp210xLib"    	/* our USBD client name */

/*globals*/
USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */
USBD_NODE_ID 	cp210xNodeId;			/*our USBD node ID*/

/*locals*/
LOCAL UINT16 		initCount = 0;			/* Count of init nesting */
LOCAL LIST_HEAD 	cp210xDevList;			/* linked list of CP210X_SIO_CHAN */
LOCAL LIST_HEAD 	reqList;				/* Attach callback request list */

LOCAL MUTEX_HANDLE 	cp210xMutex;   			/* mutex used to protect internal structs */

LOCAL UINT32 		usbCp210xIrpTimeOut = CP210X_IRP_TIME_OUT;

LOCAL UINT16 cp210xAdapterList[][2] = {
		{ 0x045B, 0x0053 },
		{ 0x0471, 0x066A },
		{ 0x0489, 0xE000 },
		{ 0x0489, 0xE003 },
		{ 0x10C4, 0x80F6 },
		{ 0x10C4, 0x8115 },
		{ 0x10C4, 0x813D },
		{ 0x10C4, 0x813F },
		{ 0x10C4, 0x814A },
		{ 0x10C4, 0x814B },
		{ 0x2405, 0x0003 },
		{ 0x10C4, 0xEA60 },
};

typedef struct attach_request{
	LINK reqLink;						/*linked list of requests*/
	CP210X_ATTACH_CALLBACK callback; 	/*client callback routine*/
	pVOID callbackArg;  				/*client callback argument*/
}ATTACH_REQUEST,*pATTACH_REQUEST;

//============================================================================//
LOCAL int cp210xTxStartUp(SIO_CHAN *pSioChan);

LOCAL int cp210xCallbackInstall(SIO_CHAN *pSioChan,int CallbackType,
		STATUS(*callback)(void *,...),void *callbackArg);

LOCAL int cp210xPollOutput(SIO_CHAN *pSioChan,char outChar);

LOCAL int cp210xPollInput(SIO_CHAN *pSioChan,char *thisChar);

LOCAL int cp210xIoctl(SIO_CHAN *pSioChan,int request,void *arg);

LOCAL STATUS cp210xChangeBaudRate(CP210X_SIO_CHAN *pSioChan,UINT32 baud);


/*channel function table.*/
LOCAL SIO_DRV_FUNCS cp210xSioDrvFuncs ={
		cp210xIoctl,
		cp210xTxStartUp,
		cp210xCallbackInstall,
		cp210xPollInput,
		cp210xPollOutput
};



/*
 * cp210x_quantise_baudrate
 * Quantises the baud rate as per AN205 Table 1
 */
LOCAL UINT32 cp210x_quantise_baudrate(UINT32 baud)
{
	if (baud <= 300)
		baud = 300;
	else if (baud <= 600)      baud = 600;
	else if (baud <= 1200)     baud = 1200;
	else if (baud <= 1800)     baud = 1800;
	else if (baud <= 2400)     baud = 2400;
	else if (baud <= 4000)     baud = 4000;
	else if (baud <= 4803)     baud = 4800;
	else if (baud <= 7207)     baud = 7200;
	else if (baud <= 9612)     baud = 9600;
	else if (baud <= 14428)    baud = 14400;
	else if (baud <= 16062)    baud = 16000;
	else if (baud <= 19250)    baud = 19200;
	else if (baud <= 28912)    baud = 28800;
	else if (baud <= 38601)    baud = 38400;
	else if (baud <= 51558)    baud = 51200;
	else if (baud <= 56280)    baud = 56000;
	else if (baud <= 58053)    baud = 57600;
	else if (baud <= 64111)    baud = 64000;
	else if (baud <= 77608)    baud = 76800;
	else if (baud <= 117028)   baud = 115200;
	else if (baud <= 129347)   baud = 128000;
	else if (baud <= 156868)   baud = 153600;
	else if (baud <= 237832)   baud = 230400;
	else if (baud <= 254234)   baud = 250000;
	else if (baud <= 273066)   baud = 256000;
	else if (baud <= 491520)   baud = 460800;
	else if (baud <= 567138)   baud = 500000;
	else if (baud <= 670254)   baud = 576000;
	else if (baud < 1000000)
		baud = 921600;
	else if (baud > 2000000)
		baud = 2000000;
	return baud;
}





/***************************************************************************
*
* findSioChan - Searches for a CP210X_SIO_CHAN for indicated node ID
*
* This fucntion searches for the pointer of CP210X_SIO_CHAN  structure
* for the indicated <nodeId>.
*
* RETURNS: pointer to matching CP210X_SIO_CHAN or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/
LOCAL CP210X_SIO_CHAN * findSioChan(USBD_NODE_ID nodeId)
{
	CP210X_SIO_CHAN * pSioChan = usbListFirst(&cp210xDevList);

	while(pSioChan != NULL){
		if(pSioChan->nodeId == nodeId)
			break;

		pSioChan = usbListNext(&pSioChan->cp210xSioLink);
	}
	return pSioChan;
}

//***************************************************************************
/*
* notifyAttach - notifies registered callers of attachment/removal
*
* This function notifies the registered clients about the device attachment
* and removal
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/
LOCAL VOID notifyAttach(CP210X_SIO_CHAN * pSioChan,UINT16 attachCode)
{
	pATTACH_REQUEST pRequest = usbListFirst(&reqList);

    while (pRequest != NULL){
    	(*pRequest->callback) (pRequest->callbackArg,
    			(SIO_CHAN *)pSioChan, attachCode);
    	pRequest = usbListNext (&pRequest->reqLink);
    }
}

/***************************************************************************
*
* cp210xOutIrpInUse - determines if any of the output IRP's are in use
*
* This function determines if any of the output IRP's are in use and returns
* the status information
*
* RETURNS: TRUE if any of the IRP's are in use, FALSE otherwise.
*
* ERRNO: none
*/

/*BOOL cp210xOutIrpInUse(CP210X_SIO_CHAN * pSioChan)
{
	BOOL inUse = FALSE;
    int i;

    for (i=0;i<pSioChan->noOfOutIrps;i++){
        if (pSioChan->outIrpInUse){
            inUse = TRUE;
            break;
        }
    }
    return(inUse);
}*/


/***************************************************************************
*
* destorySioChan - disposes of a CP210X_SIO_CHAN structure
*
* Unlinks the indicated CP210X_SIO_CHAN structure and de-allocates
* resources associated with the channel.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL void destorySioChan(CP210X_SIO_CHAN * pSioChan)
{
	printf("IN destorySioChan\n");
	if (pSioChan != NULL){
		/* Unlink the structure. */
		usbListUnlink (&pSioChan->cp210xSioLink);

		/* Release pipes and wait for IRPs to be cancelled if necessary. */
		if (pSioChan->outPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->outPipeHandle);

		if (pSioChan->inPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->inPipeHandle);

		/* The outIrpInUse or inIrpInUse can be set to FALSE only when the mutex
		 * is released. So releasing the mutex here
		 */
		OSS_MUTEX_RELEASE (cp210xMutex);

		while (pSioChan->outIrpInUse || pSioChan->inIrpInUse)
			    OSS_THREAD_SLEEP (1);

		/* Acquire the mutex again */
		OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

		/* release buffers */
		if (pSioChan->pOutBfr != NULL)
		    OSS_FREE (pSioChan->pOutBfr);

		if (pSioChan->pInBfr != NULL)
			OSS_FREE (pSioChan->pInBfr);

		/* Release structure. */
		OSS_FREE (pSioChan);

		/* Release structure. */
		/* This constitutes a memory leak, however leaving it in
		 *  causes failure.
		if (pSioChan !=NULL)
			OSS_FREE (pSioChan);
		*/
	}
}

/***************************************************************************
*
* findEndpoint - searches for a BULK endpoint of the indicated direction.
*
* This function searches for the endpoint of indicated direction,
* used in createDevStructure().
*
* RETURNS: a pointer to matching endpoint descriptor or NULL if not found.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_ENDPOINT_DESCR findEndpoint
    (
    pUINT8 pBfr,		/* buffer to search for */
    UINT16 bfrLen,		/* buffer length */
    UINT16 direction		/* end point direction */
    )
    {
    pUSB_ENDPOINT_DESCR pEp;

    while ((pEp = (pUSB_ENDPOINT_DESCR)
	    usbDescrParseSkip (&pBfr, &bfrLen, USB_DESCR_ENDPOINT))
		!= NULL)
	{
	if ((pEp->attributes & USB_ATTR_EPTYPE_MASK) == USB_ATTR_BULK &&
	    (pEp->endpointAddress & USB_ENDPOINT_DIR_MASK) == direction)
	    break;
	}

    return pEp;
    }

/***************************************************************************
*
* initOutIrp - initiates data transmission to device
*
* If the output IRP is not already in use, this function fills the output
* buffer by invoking the "tx char callback".  If at least one character is
* available for output, the function then initiates the output IRP.
*
* RETURNS: OK, or ERROR if unable to initiate transmission
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS initOutIrp( CP210X_SIO_CHAN *pSioChan)
{
	pUSB_IRP pIrp = &pSioChan->outIrp;
	UINT16 count;

	/* Return immediately if the output IRP is already in use. */
	if (pSioChan->outIrpInUse){
		sleep(1);
	}

	if(!pSioChan->outIrpInUse){
		/* If there is no tx callback, return an error */
		if (pSioChan->getTxCharCallback == NULL)
		return ERROR;

		/* Fill the output buffer until it is full or until the tx callback
		 * has no more data.  Return if there is no data available.
		 */
		count = 0;

		while (count < pSioChan->outBfrLen &&
		   (*pSioChan->getTxCharCallback) (pSioChan->getTxCharArg,
				   &pSioChan->pOutBfr [count])== OK){
			count++;
		}

		if (count == 0)
		return OK;

		/* Initialize IRP */
		pIrp->irpLen = sizeof (pSioChan->outIrp);
		pSioChan->outIrp.transferLen = count;

		pSioChan->outIrp.bfrCount = 1;
		pSioChan->outIrp.bfrList[0].pid = USB_PID_OUT;
		pSioChan->outIrp.bfrList[0].pBfr = (UINT8 *)pSioChan->pOutBfr;
		pSioChan->outIrp.bfrList[0].bfrLen = count;

		/* Submit IRP */
		if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK)
		return ERROR;

		pSioChan->outIrpInUse = TRUE;
		return OK;
	}

	return ERROR;
}

/***************************************************************************
*
* listenForInput - Initialize IRP to listen for input from device
*
* This function initialize IRP to listen for input from device.
*
* RETURNS: OK, or ERROR if unable to submit IRP to listen for input
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS listenForInput(CP210X_SIO_CHAN *pSioChan)
{
    USB_IRP *pIrp = &pSioChan->inIrp;

    /* Initialize IRP */
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->transferLen = pSioChan->inBfrLen;

    pIrp->bfrCount = 1;
    pIrp->bfrList[0].pid = USB_PID_IN;

    pIrp->bfrList[0].pBfr = (UINT8 *) pSioChan->pInBfr;

    pIrp->bfrList[0].bfrLen = pSioChan->inBfrLen;

    /* Submit IRP */
    if (usbdTransfer (cp210xHandle, pSioChan->inPipeHandle, pIrp) != OK)
	return ERROR;

    pSioChan->inIrpInUse = TRUE;

    return OK;
}



/***************************************************************************
*
* cp210xIrpCallback - Invoked upon IRP completion
*
* Examines the status of the IRP submitted.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL void cp210xIrpCallback(
		pVOID p//pointer to the IRP submitted
		){

	printf("IN cp210xIrpCallback\n");
	USB_IRP	*pIrp = (USB_IRP *)p;
	CP210X_SIO_CHAN *pSioChan = pIrp->userPtr;

	UINT16 count;

	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	/* Determine if we're dealing with the output or input IRP. */
	if (pIrp == &pSioChan->outIrp){
		/* Output IRP completed */
		pSioChan->outIrpInUse = FALSE;

		if (pIrp->result != OK)
			pSioChan->outErrors++;
		/* Unless the IRP was cancelled - implying the channel is being
		 * torn down, see if there's any more data to send.
		 */

		if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
			initOutIrp (pSioChan);

	}else{

		/* Input IRP completed */
		pSioChan->inIrpInUse = FALSE;

		if (pIrp->result != OK)
			pSioChan->inErrors++;

		/* If the IRP was successful, and if an "rx callback" has been
		 * installed, then pass the data back to the client.
		 */
		if (pIrp->result == OK && pSioChan->putRxCharCallback != NULL){

			for (count = 0; count < pIrp->bfrList [0].actLen; count++)
				(*pSioChan->putRxCharCallback) (pSioChan->putRxCharArg,
						pIrp->bfrList[0].pBfr[count]);
		}

		/* Unless the IRP was cancelled - implying the channel is being
		 * torn down, re-initiate the "in" IRP to listen for more data from
		 * the printer.
		 */

		if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
			listenForInput (pSioChan);
	}


	OSS_MUTEX_RELEASE (cp210xMutex);


/*
    // check whether the IRP was for bulk out/ bulk in / status transport
    // 检查这个IRP是否是用于BULK OUT/BULK IN的状态报告的。
    if (pIrp == &(pSioChan->outIrp) ){
    	// check the result of IRP
        if (pIrp->result == OK){
        	printf("cp210xIrpCallback: Num of Bytes transferred on "\
        			"out pipe is:%d\n",pIrp->bfrList[0].actLen);
        	pSioChan->outIrpInUse = FALSE;
        }else{
        	printf("cp210xIrpCallback: Irp failed on Bulk Out %x \n",pIrp->result);
        	pSioChan->outIrpInUse = FALSE;

             Clear HALT Feature on Bulk out Endpoint
            if ((usbdFeatureClear (cp210xHandle,cp210xNodeId,USB_RT_ENDPOINT,
            		USB_FSEL_DEV_ENDPOINT_HALT,(pSioChan->outEpAddr & 0xFF)))
				 != OK){
                printf("cp210xIrpCallback: Failed to clear HALT "\
                              "feauture on bulk out Endpoint\n");
            }
        }
    }else if (pIrp == &(pSioChan->statusIrp))     if status block IRP
    {
    	// check the result of the IRP
    	if (pIrp->result == OK){
    		printf("cp210xIrpCallback : Num of Status Bytes \
                            read  =%d \n", pIrp->bfrList[0].actLen);
    	}else{
    		// status IRP failed
    		printf("cp210xIrpCallback: Status Irp failed on Bulk in "\
                          "%x\n", pIrp->result);
    	}
    }else{
    	// IRP for bulk_in data
    	if (pIrp->result == OK){
            printf("cp210xIrpCallback: Num of Bytes read from Bulk "\
                            "In =%d\n", pIrp->bfrList[0].actLen);
    	}else{
    		// IRP on BULK IN failed
            printf("cp210xIrpCallback : Irp failed on Bulk in ,%x\n",
                            pIrp->result);

            // Clear HALT Feature on Bulk in Endpoint
            if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
            		USB_FSEL_DEV_ENDPOINT_HALT,(pSioChan->inEpAddr & 0xFF)))
            		!= OK){
            	printf ("cp210xIrpCallback: Failed to clear HALT "\
                              "feature on bulk in Endpoint %x\n");
            }
    	}
    }*/

    OSS_SEM_GIVE (pSioChan->cp210xIrpSem);
}




//=========================================================================
LOCAL STATUS cp210xSetTermiosPort(CP210X_SIO_CHAN *pSioChan)
{
	UINT16 length = 0;
	UINT16 actLength;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_IFC_ENABLE,UART_ENABLE,
			pSioChan->interface,0,NULL,&actLength) != OK)
		return ERROR;

	/*获取波特率*/
	/*UINT32 baud;
	length = sizeof(UINT32);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baud,&actLength)!= OK)
		return ERROR;*/

	/*set default baudrate to 115200*/
	UINT32 baud = 115200;
	if(cp210xChangeBaudRate(pSioChan,baud) != OK){
		printf("set default baudrate to 115200 failed!\n");
		return ERROR;
	}


	/*if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pSioChan->interface,length,(pUINT8)&baud,&actLength) != OK){
		return ERROR;
	}*/

/*再次获取波特率，确认是否写入*/
	/*UINT32 baudNow = 0;
	length = sizeof(UINT32);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baudNow,&actLength)!= OK){
		return ERROR;
	}else{
		printf("the baudrate is :%d\n",baudNow);
	}*/

//设置为8个数据位，1个停止位，没有奇偶校验，没有流控。
	UINT16 bits;
	length = sizeof(UINT16);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bits,&actLength)!= OK)
		return ERROR;

	bits &= ~(BITS_DATA_MASK | BITS_PARITY_MASK | BITS_STOP_MASK);
	bits |= (BITS_DATA_8 | BITS_PARITY_NONE | BITS_STOP_1);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pSioChan->interface,0,NULL,&actLength) != OK)
		return ERROR;

/*重新查看当前bits设置*/
	UINT16 bitsNow;
	length = sizeof(bitsNow);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bitsNow,&actLength)!= OK)
		return ERROR;

	switch(bitsNow & BITS_DATA_MASK){
	case BITS_DATA_5:
		printf("data bits = 5\t");
		break;

	case BITS_DATA_6:
		printf("data bits = 6\t");
		break;

	case BITS_DATA_7:
		printf("data bits = 7\t");
		break;

	case BITS_DATA_8:
		printf("data bits = 8\t");
		break;

	default:
		printf("unknow number of data bits.using 8\n");
		return ERROR;
//		break;
	}

	switch(bitsNow & BITS_PARITY_MASK){
	case BITS_PARITY_NONE:
		printf("patiry = NONE\t");
		break;

	case BITS_PARITY_ODD:
		printf("patiry = ODD\t");
		break;

	case BITS_PARITY_EVEN:
		printf("patiry = EVEN\t");
		break;

	case BITS_PARITY_MASK:
		printf("patiry = MASK\t");
		break;

	case BITS_PARITY_SPACE:
		printf("patiry = SPACE\t");
		break;

	default :
		printf("Unknow parity mode.disabling patity\n");
		return ERROR;
//		break;
	}

	switch (bitsNow & BITS_STOP_MASK){
	case BITS_STOP_1:
		printf("stop bits = 1\t");
		break;

	case BITS_STOP_1_5:
		printf("stop bits = 1.5\t");
		break;

	case BITS_STOP_2:
		printf("stop bits = 2\t");
		break;

	default:
		printf("Unknown number of stop bitsNow,using 1 stop bit\n");
		return ERROR;
	}
	printf("\n");
	return OK;
}




/***************************************************************************
*
* configSioChan - configure cp210x for operation
*
* Selects the configuration/interface specified in the <pSioChan>
* structure.  These values come from the USBD dynamic attach callback,
* which in turn retrieved them from the configuration/interface
* descriptors which reported the device to be a printer.
*
* RETURNS: OK if successful, else ERROR if failed to configure channel
*
* ERRNO: none
*
* \NOMANUAL
*/

STATUS configSioChan(CP210X_SIO_CHAN *pSioChan)
{
	USB_CONFIG_DESCR 	* pCfgDescr;
	USB_INTERFACE_DESCR * pIfDescr;
	USB_ENDPOINT_DESCR 	* pOutEp;
	USB_ENDPOINT_DESCR 	* pInEp;
	UINT8 * pBfr;
	UINT8 * pScratchBfr;
	UINT16 actLen;
	UINT16 maxPacketSize;

    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
    	return ERROR;

    /* Read the configuration descriptor to get the configuration selection
     * value and to determine the device's power requirements.
     * Configuration index is assumed to be one less than config'n value
     */

    if (usbdDescriptorGet (cp210xHandle, pSioChan->nodeId,
    			USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,
    			0, 0, USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
    	OSS_FREE (pBfr);
    	return ERROR;
    }

	if ((pCfgDescr = usbDescrParse (pBfr, actLen,
			USB_DESCR_CONFIGURATION)) == NULL){
        OSS_FREE (pBfr);
        return ERROR;
	}

	pSioChan->configuration = pCfgDescr->configurationValue;

    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */
	UINT16 ifNo = 0;
	pScratchBfr = pBfr;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,
			USB_DESCR_INTERFACE))!= NULL){
		/*look for the interface indicated in the pSioChan structure*/
		if (ifNo == pSioChan->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pSioChan->interfaceAltSetting = pIfDescr->alternateSetting;
//	pSioChan->interface = pIfDescr->interfaceNumber;

	/*if ((pIfDescr = usbDescrParseSkip (&pScratchBfr, &actLen,
			USB_DESCR_INTERFACE)) == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}*/


    /* Retrieve the endpoint descriptor(s) following the identified interface
     * descriptor.
     */
	if ((pOutEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_OUT)) == NULL){
		OSS_FREE (pBfr);
        return ERROR;
	}

	if ((pInEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_IN)) == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pSioChan->outEpAddr = pOutEp->endpointAddress;
	pSioChan->inEpAddr = pInEp->endpointAddress;


    /* Select the configuration. */
	if (usbdConfigurationSet (cp210xHandle, cp210xNodeId,
				pCfgDescr->configurationValue,
				pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) != OK){
        OSS_FREE (pBfr);
        return ERROR;
    }

    /* Select interface
     *
     * NOTE: Some devices may reject this command, and this does not represent
     * a fatal error.  Therefore, we ignore the return status.
     */

    usbdInterfaceSet(cp210xHandle,cp210xNodeId,pSioChan->interface,\
			pIfDescr->alternateSetting);

    /* Create a pipe for output . */
	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) |
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,
			0,0,&pSioChan->outPipeHandle)!= OK){
		OSS_FREE (pBfr);
		return ERROR;
	}

	/*Create a pipe for input*/
	maxPacketSize = *((pUINT8)&pInEp->maxPacketSize) |
			(*(((pUINT8)&pInEp->maxPacketSize)+1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pInEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_IN,maxPacketSize,
			0,0,&pSioChan->inPipeHandle)!= OK){
		OSS_FREE (pBfr);
		return ERROR;
	}

	if(cp210xSetTermiosPort(pSioChan) != OK){
		OSS_FREE(pBfr);
		return ERROR;
	}

	/* Clear HALT feauture on the endpoints */
	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT, (pOutEp->endpointAddress & 0xFF)))
			!= OK){
		OSS_FREE(pBfr);
		return ERROR;
	}

	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT,(pInEp->endpointAddress & 0xFF)))
			!= OK){
		OSS_FREE(pBfr);
		return ERROR;
	}

/*	char e[8]="0303030";
	pSioChan->outIrp.transferLen = sizeof (e);
	pSioChan->outIrp.result = -1;
	pSioChan->outIrp.bfrCount = 1;
	pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
	pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)e;
	pSioChan->outIrp.bfrList [0].bfrLen = sizeof (e);
	if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
	printf("usbdtransfer returned false\n");
	}

	sleep(2);

	char f[]="hello world!";
	pSioChan->outIrp.transferLen = sizeof (f);
	pSioChan->outIrp.result = -1;
	pSioChan->outIrp.bfrCount = 1;
	pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
	pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)f;
	pSioChan->outIrp.bfrList [0].bfrLen = sizeof (f);
	if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
	printf("usbdtransfer returned false\n");
	}

	sleep(2);
   char g[]="你好";
   pSioChan->outIrp.transferLen = sizeof (g);
   pSioChan->outIrp.result = -1;
   pSioChan->outIrp.bfrCount = 1;
   pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
   pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)g;
   pSioChan->outIrp.bfrList [0].bfrLen = sizeof (g);
   if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
	printf("usbdtransfer returned false\n");
   }
   sleep(2);*/

			/*if (listenForInput (pSioChan) != OK)
            {
            OSS_FREE (pBfr);
            return ERROR;
            }*/
//	pSioChan->inIrpInUse = FALSE;

	if (listenForInput (pSioChan) != OK){
		OSS_FREE (pBfr);
		return ERROR;
	}

    OSS_FREE (pBfr);
    return OK;
}



/***************************************************************************
*
* createSioChan - creates a new CP210X_SIO_CHAN structure
*
* Creates a new CP210X_SIO_CHAN structure for the indicated CP210_DEV.
* If successful, the new structure is linked into the sioList upon
* return.
*
* <configuration> and <interface> identify the configuration/interface
* that first reported itself as a printer for this device.
*
* RETURNS: pointer to newly created structure, or NULL if failure
*
* ERRNO: none
*
* \NOMANUAL
*/
STATUS createSioChan(CP210X_SIO_CHAN *pSioChan)
{
	 /*Try to allocate space for the new structure's parameter*/
	if ((pSioChan->pOutBfr = OSS_CALLOC(CP210X_OUT_BFR_SIZE))== NULL )
		return ERROR;

	/*we don't use IN PIPE*/
	/*if((pSioChan->InBfr = OSS_CALLOC(CP210X_IN_BFR_SIZE)) == NULL){
		destorySioChan(pSioChan);
		return NULL;
	}*/

	pSioChan->txIrpIndex = 0;
	pSioChan->txStall = FALSE;
	pSioChan->txActive = FALSE;

	pSioChan->outBfrLen = CP210X_OUT_BFR_SIZE;
//	pSioChan->inBfrLen = CP210X_IN_BFR_SIZE;

	pSioChan->sioChan.pDrvFuncs = &cp210xSioDrvFuncs;
	pSioChan->mode = SIO_MODE_POLL;    /*always be SIO_MODE_POLL for cp210x*/

	/*initial outIrp*/
	pSioChan->outIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->outIrp.userCallback		= cp210xIrpCallback;
	pSioChan->outIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->outIrp.bfrCount          = 0x01;
	pSioChan->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pSioChan->outIrp.userPtr           = pSioChan;

	//初始化Device的InIrp
/*	pSioChan->inIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->inIrp.userCallback		= cp210xIrpCallback;
	pSioChan->inIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->inIrp.bfrCount          = 0x01;
	pSioChan->inIrp.bfrList[0].pid    = USB_PID_IN;
	pSioChan->inIrp.userPtr           = pSioChan;*/

	/*Try to configure the cp210x device*/
	if(configSioChan(pSioChan) != OK)
			return ERROR;

	if (OSS_SEM_CREATE( 1, 1, &pSioChan->cp210xIrpSem) != OK){
		return ERROR;
//        return(cp210xShutdown(S_cp210xLib_OUT_OF_RESOURCES));
	}
	return OK;
}

/***************************************************************************
*
* cp210xLibAttachCallback - gets called for attachment/detachment of devices
*
* The USBD will invoke this callback when a USB to RS232 device is
* attached to or removed from the system.
* <nodeId> is the USBD_NODE_ID of the node being attached or removed.
* <attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* Communication device functionality resides at the interface level, with
* the exception of being the definition of the Communication device class
* code.so <configuration> and <interface> will indicate the configuration
* or interface that reports itself as a USB to RS232 device.
* <deviceClass> and <deviceSubClass> will match the class/subclass for
* which we registered.
* <deviceProtocol> doesn't have meaning for the USB to RS232 devices so we
* ignore this field.
*
* NOTE
* The USBD invokes this function once for each configuration or
* interface which reports itself as a USB to RS232 device.
* So, it is possible that a single device insertion or removal may trigger
* multiple callbacks.  We ignore all callbacks except the first for a
* given device.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS cp210xLibAttachCallback ( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,
		UINT16 interface,UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
	CP210X_SIO_CHAN *pSioChan;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int noOfSupportedDevices =(sizeof(cp210xAdapterList)/(2*sizeof(UINT16))) ;
    int index = 0;

    if((pBfr = OSS_MALLOC(USB_MAX_DESCR_LEN)) == NULL)
    	return ERROR;

    OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	switch(attachAction){
	case USBD_DYNA_ATTACH:
		/*
		 * a new device is being attached
		 * Check if we already  have a structure for this device.
		 */
		if(findSioChan(nodeId)!= NULL)
			break;


		/*Now,we have to ensure that its a cp210x device*/
		if (usbdDescriptorGet (cp210xHandle, nodeId, USB_RT_STANDARD | USB_RT_DEVICE,
				USB_DESCR_DEVICE, 0, 0, 36, pBfr, &actLen) != OK)
	    	  break;


		vendorId = (((pUSB_DEVICE_DESCR) pBfr)->vendor);
		productId = (((pUSB_DEVICE_DESCR) pBfr)->product);

		/*
		 * 查找支持的设备列表， 确定所接入的设备
		 * 的vendorId 和ProductId是否在支持的设备在列表当中
		 */
		for (index = 0; index < noOfSupportedDevices; index++)
			if (vendorId == cp210xAdapterList[index][0])
				if (productId == cp210xAdapterList[index][1])
					break;

		if (index == noOfSupportedDevices ){
			/* device not supported */
			printf( " Unsupported device find vId %0x; pId %0x!!!\n",\
				  vendorId, productId);
			break;
		}else{
			printf("Find device vId %0x; pId %0x!!!\n",vendorId,productId);
		}

		/*
		 * Now create a structure for the newly found device and add
		 * it to the linked list
		 * */
		if((pSioChan = OSS_CALLOC(sizeof(CP210X_SIO_CHAN))) == NULL )
	    	  break;

	    pSioChan->nodeId = nodeId;
	    pSioChan->configuration = configuration;
	    pSioChan->interface = interface;
	    pSioChan->vendorId = vendorId;
	    pSioChan->productId = productId;

	    /*Save a global copy of NodeID*/
	    cp210xNodeId = nodeId;

	    /* Continue fill the newly allocate structure,If there's an error,
		 * there's nothing we can do about it, so skip the device and return immediately.
		 */
		if((createSioChan(pSioChan)) != OK){
			destorySioChan(pSioChan);
			break;
		}

	    /*ADD this structure to the linked list tail*/
		usbListLink (&cp210xDevList, pSioChan, &pSioChan->cp210xSioLink,LINK_TAIL);

	      /*将设备标记为已连接*/
	    pSioChan->connected = TRUE;

	      /* Notify registered callers that a new cp210x has been added
	       * and a new channel created.*/
	    notifyAttach (pSioChan, CP210X_ATTACH);
	    break;



	case USBD_DYNA_REMOVE:
		 /* A device is being detached.
		  * Check if we have any structures to manage this device.*/
		if ((pSioChan = findSioChan (nodeId)) == NULL)
			break;

		/* The device has been disconnected. */
		if(pSioChan->connected == FALSE)
			break;

		pSioChan->connected = FALSE;

		/* Notify registered callers that the cp210x has been removed
		 * and the channel disabled.
		 *
		 * NOTE: We temporarily increment the channel's lock count to
		 * prevent cp210xSioChanUnlock() from destroying the
		 * structure while we're still using it.*/
		pSioChan->lockCount++;
		notifyAttach (pSioChan, CP210X_REMOVE);

		pSioChan->lockCount--;

		/* If no callers have the channel structure locked,
		 * destroy it now.If it is locked, it will be destroyed
		 * later during a call to cp210xSioChanUnlock().*/
		if (pSioChan->lockCount == 0)
			destorySioChan (pSioChan);

		break;
	}

	OSS_FREE(pBfr);
	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}

/***************************************************************************
*
* destroyAttachRequest - disposes of an ATTACH_REQUEST structure
*
* This functon disposes of an ATTACH_REQUEST structure
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID destroyAttachRequest(pATTACH_REQUEST pRequest)
{
    /* Unlink request */
    usbListUnlink (&pRequest->reqLink);

    /* Dispose of structure */
    OSS_FREE (pRequest);
}



/***************************************************************************
*
* cp210xShutdown - shuts down cp210x SIO driver
*
* <errCode> should be OK or S_cp210xLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: depends on the error code <errCode>
*
* \NOMANUAL
*/

LOCAL STATUS cp210xShutdown(int errCode)
{
    CP210X_SIO_CHAN * pSioChan;
    ATTACH_REQUEST 	*pRequest;

    /*Dispose of any outstanding notification requests*/
    while((pRequest = usbListFirst(&reqList)) != NULL)
    	destroyAttachRequest(pRequest);


    /* Dispose of any open connections. */
    while ((pSioChan = usbListFirst (&cp210xDevList)) != NULL)
    	destorySioChan (pSioChan);

    /*
     * Release our connection to the USBD.  The USBD automatically
     * releases any outstanding dynamic attach requests when a client
     * unregisters.
     */
    if (cp210xHandle != NULL){
    	usbdClientUnregister (cp210xHandle);
    	cp210xHandle = NULL;
	}

    /* Release resources. */
    if (cp210xMutex != NULL){
    	OSS_MUTEX_DESTROY (cp210xMutex);
    	cp210xMutex = NULL;
	}

    return ossStatus (errCode);

}


/***************************************************************************
*
* cp210xDevInit - initializes the cp210x library
*
* Initizes the cp210x library. The library maintains an initialization
* count so that the calls to this function might be nested.
*
* This function initializes the system resources required for the library
* initializes the linked list for the devices found.
* This function reegisters the library as a client for the usbd calls and
* registers for dynamic attachment notification of usb communication device
* class and Ethernet sub class of devices.
*
* RETURNS : OK if successful, ERROR if failure
*
* ERRNO:
* \is
* \i S_cp210xLib_OUT_OF_RESOURCES
* Sufficient Resources not Available
*
* \i S_cp210xLib_USBD_FAULT
* Fault in the USBD Layer
* \ie
*/
STATUS cp210xDevInit (void)
{
	/*
	 * if not already initialized then initialize internal structures
	 * and connection to USBD
	 * */
	if(initCount == 0){
		memset(&cp210xDevList,0,sizeof(cp210xDevList));
		memset(&reqList,0,sizeof(reqList));

		cp210xMutex		 	= NULL;
		cp210xHandle		= NULL;

		if (OSS_MUTEX_CREATE (&cp210xMutex) != OK)
			return cp210xShutdown (S_cp210xLib_OUT_OF_RESOURCES);

		if (usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK ||
				usbdDynamicAttachRegister (cp210xHandle,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,TRUE,
						(USBD_ATTACH_CALLBACK)cp210xLibAttachCallback)!= OK)
			return cp210xShutdown (S_cp210xLib_USBD_FAULT);

	}
	initCount++;
	return OK;
}

/***************************************************************************
*
* cp210xDevShutdown - shuts down cp210x SIO driver
*
* This function shutdowns the cp210x SIO driver when <initCount> becomes 0
*
* RETURNS: OK, or ERROR if unable to shutdown.
*
* ERRNO:
* \is
* \i S_cp210xLib_NOT_INITIALIZED
* Printer not initialized
* \ie
*/

STATUS cp210xDevShutdown (void) {
    /* Shut down the cp210x SIO driver if the initCount goes to 0. */
    if (initCount == 0)
	return ossStatus (S_cp210xLib_NOT_INITIALIZED);

    if (--initCount == 0)
	return cp210xShutdown (OK);

    return OK;
}

/***************************************************************************
*
* cp210xDynamicAttachRegister - register cp210x device attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*CP210X_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     CP210X_SIO_CHAN * pSioChan,
*     UINT16 attachCode
*     );
* \ce
*
* cp210xDevLib will invoke <callback> each time a PEGASUS device
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also  pass the structure of the device
* being created/destroyed and an attach code of CP210X_ATTACH or
* CP210X_REMOVE.
*
* NOTE
* The user callback routine should not invoke any driver function that
* submits IRPs.  Further processing must be done from a different task context.
* As the driver routines wait for IRP completion, they cannot be invoked from
* USBD client task's context created for this driver.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_cp210xLib_BAD_PARAM
* Bad Parameter received
*
* \i S_cp210xLib_OUT_OF_MEMORY
* Sufficient memory no available
* \ie
*/
STATUS cp210xDynamicAttachRegister(
		CP210X_ATTACH_CALLBACK callback,	/* new callback to be registered */
	    pVOID arg		    				/* user-defined arg to callback */
){
	printf("IN cp210xDynamicAttachRegister\n");

	pATTACH_REQUEST pRequest;
	CP210X_SIO_CHAN * pSioChan;
	int status = OK;

	if (callback == NULL)
		return (ossStatus (S_cp210xLib_BAD_PARAM));
	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	/* Create a new request structure to track this callback request.*/
	if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL){
		status =  ossStatus (S_cp210xLib_OUT_OF_MEMORY);;
	}else{
			pRequest->callback = callback;
			pRequest->callbackArg = arg;

			usbListLink(&reqList, pRequest, &pRequest->reqLink, LINK_TAIL);

			/* Perform an initial notification of all*/
			/* currrently attached devices.*/
			pSioChan = usbListFirst (&cp210xDevList);
			while (pSioChan != NULL){
				if (pSioChan->connected)
					(*callback) (arg,(SIO_CHAN *)pSioChan,CP210X_ATTACH);

				pSioChan = usbListNext (&pSioChan->cp210xSioLink);
			}
		}

	OSS_MUTEX_RELEASE (cp210xMutex);
	return ossStatus (status);
}

/***************************************************************************
*
* cp210xDynamicAttachUnregister - unregisters cp210x attach callback
*
* This function cancels a previous request to be dynamically notified for
* attachment and removal.  The <callback> and <arg> paramters
* must exactly match those passed in a previous call to
* cp210xDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister the callback.
*
* ERRNO:
* \is
* \i S_cp210xLib_NOT_REGISTERED
* Could not regsiter the attachment callback
* \ie
*/


STATUS cp210xDynamicAttachUnRegister(
		CP210X_ATTACH_CALLBACK callback,	/* callback to be unregistered.*/
		pVOID arg		    				/* user-defined arg to callback.*/
){
	printf("IN cp210xDynamicAttachUnRegister\n");

    pATTACH_REQUEST pRequest;
    int status = S_cp210xLib_NOT_REGISTERED;
    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);
    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL){
    	if ((callback == pRequest->callback) && (arg == pRequest->callbackArg)){
    		/*we found a matching notification request*/
    		destroyAttachRequest(pRequest);
    		status = OK;
    		break;
    	}

    	pRequest = usbListNext (&pRequest->reqLink);
    }
    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}


/***************************************************************************
*
* cp210xSioChanLock - marks SIO_CHAN structure as in use
*
* A caller uses cp210xSioChanLock() to notify cp210xLib that
* it is using the indicated SIO_CHAN structure.  cp210xLib maintains
* a count of callers using a particular SIO_CHAN structure
* so that it knows when it is safe to dispose of a structure
* when the underlying cp210x device is removed from the system.
* So long as the "lock count" is greater than zero,
* cp210xLib will not dispose of an SIO_CHAN structure.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure in use.
*
* ERRNO: none
*/

STATUS cp210xSioChanLock(SIO_CHAN *pChan)
{
    if ( pChan == NULL)
    	return ERROR;

    CP210X_SIO_CHAN *pSioChan = (CP210X_SIO_CHAN *)pChan;
    pSioChan->lockCount++;
    return OK;
}


/***************************************************************************
*
* cp210xSioChanUnlock - marks SIO_CHAN structure as unused
*
* This function releases a lock placed on an SIO_CHAN structure.  When a
* caller no longer needs an SIO_CHAN structure for which it has previously
* called cp210xSioChanLock(), then it should call this function to
* release the lock.
*
* NOTE
* If the underlying cp210x device has already been removed
* from the system, then this function will automatically dispose of the
* SIO_CHAN structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the SIO_CHAN structure again after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure unused.
*
* ERRNO:
* \is
* \i S_cp210xLib_NOT_LOCKED
* No lock to Unlock
* \ie
*/
STATUS cp210xSioChanUnlock(SIO_CHAN *pChan)
{
    int status = OK;

    if(pChan == NULL)
    	return ERROR;
    CP210X_SIO_CHAN *pSioChan = (CP210X_SIO_CHAN *)pChan;

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0){
    	status = S_cp210xLib_NOT_LOCKED;
    }else{
    	/* If this is the last lock and the underlying cp210x device
    	 * is no longer connected, then dispose of the device.
    	 */
    	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
    		destorySioChan ((CP210X_SIO_CHAN *)pSioChan);
    }

    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}

/***************************************************************************
*
* cp210xTxStartup - start the interrupt transmitter
*
* This function will be called when characters are available for transmission
* to the device.
*
* RETURNS: OK, or EIO if unable to start transmission to device
*
* ERRNO: none
*
* \NOMANUAL
*/
LOCAL int cp210xTxStartUp(SIO_CHAN *pChan)
{
	printf("IN cp210xTxStartUp\n");

	CP210X_SIO_CHAN *pSioChan = (CP210X_SIO_CHAN *)pChan;
	int status = OK;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);
	if(initOutIrp(pSioChan)!= OK)
		status = EIO;

	OSS_MUTEX_RELEASE(cp210xMutex);

	return status;
}



/***************************************************************************
*
* cp210xCallbackInstall - install ISR callbacks to get/put chars
* 安装高层协议的回调函数
*
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.=
*
* RETURNS: OK on success, or ENOSYS for an unsupported callback type.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int cp210xCallbackInstall(SIO_CHAN *pChan,int callbackType,
		STATUS(*callback)(void *tmp,...),void *callbackArg)
{
	printf("IN cp210xCallbackInstall now\n");

	CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *)pChan;

	switch (callbackType){

	case SIO_CALLBACK_GET_TX_CHAR:
		pSioChan->getTxCharCallback = (STATUS (*)()) (callback);
		pSioChan->getTxCharArg = callbackArg;
		return OK;

	case SIO_CALLBACK_PUT_RCV_CHAR:
		pSioChan->putRxCharCallback = (STATUS (*)()) (callback);
		pSioChan->putRxCharArg = callbackArg;
		return OK;

	default:
		return ENOSYS;
	}
}


/***************************************************************************
*
* cp210xPollOutput - output a character in polled mode
* 查询模式的输出函数
* .
*
* RETURNS: OK
*
* ERRNO: none
*
*\NOMANUAL
*/
LOCAL int cp210xPollOutput(SIO_CHAN *pChan,char outChar)
{
//	printf("IN cp210xPollOutput");

	CP210X_SIO_CHAN *pSioChan = (CP210X_SIO_CHAN *)pChan;
//	printf("%c\n",outChar);
	if(initOutIrp(pSioChan)!= OK)
		return ERROR;

	return OK;
}

/***************************************************************************
*
* nextInChar - returns next character from input queue
*
* Returns the next character from the channel's input queue and updates
* the queue pointers.  The caller must ensure that at least one character
* is in the queue prior to calling this function.
*
* RETURNS: next char in queue
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL char nextInChar( CP210X_SIO_CHAN *pSioChan )
{
    char inChar = pSioChan->inQueue [pSioChan->inQueueOut];

    if (++pSioChan->inQueueOut == CP210X_Q_DEPTH)
	pSioChan->inQueueOut = 0;

    pSioChan->inQueueCount--;

    return inChar;
}

/***************************************************************************
*
* cp210xPollInput - poll the device for input
*
* This function polls the cp210x device for input.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
* if the input buffer if empty, ENOSYS if the device is interrupt-only.
*
* ERRNO: none
*
*\NOMANUAL
*/
LOCAL int cp210xPollInput(SIO_CHAN *pChan,char *thisChar)
{
	printf("IN cp210xPollInput\n");

	CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *)pChan;
	int status = OK;

	/*validate parameters*/
	if(thisChar == NULL)
		return EIO;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	/*Check if the input queue is empty.*/

	if (pSioChan->inQueueCount == 0)
		status = EAGAIN;
	else{
		/*Return a character from the input queue.*/
		*thisChar = nextInChar (pSioChan);
	}
	OSS_MUTEX_RELEASE (cp210xMutex);
	return status;
}


/**************************************************************************
 * cp210xChangeBaudRate - used to change baudRate of the device.
 *
 * RETURNS: OK on success,ERROR on failed!
 *
 * ERRNO: none
 *
 * \NOMANUAL
 */
LOCAL STATUS cp210xChangeBaudRate(CP210X_SIO_CHAN *pSioChan,UINT32 baud)
{
	baud = cp210x_quantise_baudrate(baud);

	UINT16 length = sizeof(UINT32);

	UINT16 actLength;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pSioChan->interface,length,(pUINT8)&baud,&actLength) != OK){
		return ERROR;
	}

	/*confirm the data has been written */
	UINT32 baudNow = 0;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baudNow,&actLength)!= OK){
		return ERROR;
	}

	if(baudNow == baud){
		printf("the baudrate set to:%d\n",baud);
		return OK;
	}else{
		return ERROR;
	}
}


/***************************************************************************
*
* cp210xIoctl - special device control
*
* This routine can used to change baudRate for the cp210x device.  The only ioctls
* which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
*
* RETURNS: OK on success, ENOSYS on unsupported request, EIO on failed request
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int cp210xIoctl
    (
    SIO_CHAN *pChan,	    /* device to control */
    int request,	/* request code */
    void *someArg	/* some argument */
    ){

	printf("IN cp210xIoctl\n");
	CP210X_SIO_CHAN *pSioChan = (CP210X_SIO_CHAN *) pChan;
    int arg = (int) someArg;

    switch (request){

	case SIO_BAUD_SET:

		if( cp210xChangeBaudRate(pSioChan,*((int *)arg)) != OK){
			printf("can not change to this baudrate\n");
			return ERROR ;
		}
		break;


	case SIO_BAUD_GET:
	{
		UINT32 baud = 0;
		UINT16 length = sizeof(UINT32);

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
				CP210X_GET_BAUDRATE,0,pSioChan->interface,
				length,(pUINT8)&baud,NULL)!= OK){
			return ERROR;
		}

	    *((int *) arg) = baud;
	    return OK;
	}

	case SIO_MODE_SET:

	    /* Set driver operating mode: for cp210x device is always be poll
	     * 设置中断或者轮询模式(INT or POLL)
	     * cp210x设备中并没有与SIO_MODE_SET相对应的厂商指令,cp210x
	     * 中没有中断端点。是不是不可以设置为中断模式。*/
	    if ( *(int *)arg != SIO_MODE_POLL || *(int *)arg != SIO_MODE_INT)
		return EIO;

	    pSioChan->mode = *(int *)arg;


	    return OK;


	case SIO_MODE_GET:

	    /* Return current driver operating mode for channel
	     * 返回系统当前的工作模式(INT or POLL)*/


	    *((int *) arg) = pSioChan->mode;
	    return OK;


	case SIO_AVAIL_MODES_GET:

	    /* Return modes supported by driver. */

	    *((int *) arg) = SIO_MODE_INT | SIO_MODE_POLL;
	    return OK;


	case SIO_OPEN:

	    /* Channel is always open. */
	    return OK;


	/*case SIO_KYBD_MODE_SET:
		switch (arg)
		 {
		 case SIO_KYBD_MODE_RAW:
		 case SIO_KYBD_MODE_ASCII:
			 break;

		 case SIO_KYBD_MODE_UNICODE:
			 return ENOSYS;  usb doesn't support unicode
		 }
		pSioChan->scanMode = arg;
		return OK;*/


/*
	case SIO_KYBD_MODE_GET:
		*(int *)someArg = pSioChan->scanMode;
		return OK;
*/

/*
	 case SIO_KYBD_LED_SET:
	{
	UINT8 ledReport;

	  update the channel's information about the LED state

	pSioChan->numLock = (arg & SIO_KYBD_LED_NUM) ? SIO_KYBD_LED_NUM : 0;

	pSioChan->capsLock = (arg & SIO_KYBD_LED_CAP) ?
				SIO_KYBD_LED_CAP : 0;
	pSioChan->scrLock = (arg & SIO_KYBD_LED_SCR) ?
				SIO_KYBD_LED_SCR : 0;


	     * We are relying on the SIO_KYBD_LED_X macros matching the USB
	     * LED equivelants.


	    ledReport = arg;

	     set the LED's

	    setLedReport (pSioChan, ledReport);

	    return OK;
	    }*/



     /*    case SIO_KYBD_LED_GET:
	     {
	     int tempArg;

    	     tempArg = (pSioChan->capsLock) ? SIO_KYBD_LED_CAP : 0;
    	     tempArg |= (pSioChan->scrLock) ? SIO_KYBD_LED_SCR : 0;
    	     tempArg |= (pSioChan->numLock) ? SIO_KYBD_LED_NUM : 0;

	     *(int *) someArg = tempArg;

             return OK;

	     }*/

	case SIO_HW_OPTS_SET:   /* optional, not supported */
	case SIO_HW_OPTS_GET:   /* optional, not supported */
	case SIO_HUP:	/* hang up is not supported */
	default:	    /* unknown/unsupported command. */

		return ENOSYS;
    }

    return OK;
}


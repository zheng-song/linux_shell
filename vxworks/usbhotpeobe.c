//=============================2017.06.20 18:30 BEGIN    solve usbdtranfer()fasongluma.==========================//

#include "cp210xLib.h"

/*typematic definitions*/
#define TYPEMATIC_NAME		 	"tUsbtty"
#define CP210X_CLIENT_NAME 	"cp210xLib"    	/* our USBD client name */

/*globals*/
USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */
USBD_NODE_ID 	cp210xNodeId;			/*our USBD node ID*/

/*locals*/
LOCAL UINT16 		initCount = 0;			/* Count of init nesting */
LOCAL LIST_HEAD 	cp210xDevList;			/* linked list of CP210X_SIO_CHAN */
LOCAL LIST_HEAD 	reqList;				/* Attach callback request list */

LOCAL MUTEX_HANDLE 	cp210xMutex;   			/* mutex used to protect internal structs */
//LOCAL MUTEX_HANDLE	cp210xTxMutex;			/*用来保护内部数据结构 to protect internal structs*/
//LOCAL MUTEX_HANDLE 	cp210xRxMutex;			/*用来保护内部数据结构*/

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


//LOCAL THREAD_HANDLE typematicHandle;		/* task used to generate typematic repeat */
//LOCAL BOOL killTypematic;	/* TRUE when typematic thread should exit */
//LOCAL BOOL typematicExit;	/* TRUE when typematic thread exits */

unsigned int TYPEMATIC_DELAY  = 500;    /* 500 msec delay */
unsigned int TYPEMATIC_PERIOD = 66;     /* 66 msec = approx 15 char/sec */

typedef struct attach_request{
	LINK reqLink;						/*linked list of requests*/
	CP210X_ATTACH_CALLBACK callback; 	/*client callback routine*/
	pVOID callbackArg;  				/*client callback argument*/
}ATTACH_REQUEST,*pATTACH_REQUEST;

//============================================================================//
//LOCAL int cp210xTxStartUp(SIO_CHAN *pSioChan);
//LOCAL int cp210xCallbackInstall(SIO_CHAN *pSioChan,int CallbackType,
//		STATUS(*callback)(void *,...),void *callbackArg);
//LOCAL int cp210xPollOutput(SIO_CHAN *pSioChan,char outChar);
//LOCAL int cp210xPollInput(SIO_CHAN *pSioChan,char *thisChar);
//LOCAL int cp210xIoctl(SIO_CHAN *pSioChan,int request,void *arg);




/*channel function table.*/
/*LOCAL SIO_DRV_FUNCS cp210xSioDrvFuncs ={
		cp210xIoctl,
		cp210xTxStartUp,
		cp210xCallbackInstall,
		cp210xPollInput,
		cp210xPollOutput
};*/


/***************************************************************************
*
* cp210xDevFind - Searches for a CP210X_SIO_CHAN for indicated node ID
*
* This fucntion searches for the pointer of CP210X_SIO_CHAN for the indicated
* <nodeId>. If the matching pointer is not found it returns NULL.
*
* RETURNS: pointer to matching CP210X_SIO_CHAN or NULL if not found
*
* ERRNO: none
*/
LOCAL CP210X_SIO_CHAN * cp210xFindDev(USBD_NODE_ID nodeId)
{
	printf("IN cp210xFindDev\n");
	CP210X_SIO_CHAN * pSioChan = usbListFirst(&cp210xDevList);

	while(pSioChan != NULL){
		if(pSioChan->nodeId == nodeId)
			break;

		pSioChan = usbListNext(&pSioChan->cp210xDevLink);
	}
	return pSioChan;
}

//========================================================================================//

/*LOCAL VOID destroyAttachRequest(pATTACH_REQUEST pRequest){
//	Unlink request
	usbListUnlinkProt(&pRequest->reqLink,cp210xMutex);
//	Dispose request
	OSS_FREE(pRequest);
}*/



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
/*LOCAL VOID notifyAttach(CP210X_SIO_CHAN * pSioChan,UINT16 attachCode)
{
	printf("IN notifyAttach\n");

	pATTACH_REQUEST pRequest = usbListFirst(&reqList);

    while (pRequest != NULL){
    	(*pRequest->callback) (pRequest->callbackArg,pSioChan, attachCode);
    	pRequest = usbListNext (&pRequest->reqLink);
    }
}*/

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

BOOL cp210xOutIrpInUse(CP210X_SIO_CHAN * pSioChan)
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
}


/***************************************************************************
*
* cp210xDestroyDevice - disposes of a CP210X_SIO_CHAN structure
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

LOCAL void cp210xDestroyDevice(CP210X_SIO_CHAN * pSioChan)
{
	printf("IN cp210xDestroyDevice\n");
	int index;

	if (pSioChan != NULL){
		/* Unlink the structure. */
		usbListUnlink (&pSioChan->cp210xDevLink);

		/* Release pipes and wait for IRPs to be cancelled if necessary. */
		if (pSioChan->outPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->outPipeHandle);

		if (pSioChan->inPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->inPipeHandle);

		while (cp210xOutIrpInUse(pSioChan) || pSioChan->inIrpInUse)
			OSS_THREAD_SLEEP (1);

		for (index=0; index < pSioChan->noOfInBfrs; index++)
			OSS_FREE (pSioChan->InBfr);

		if (pSioChan->InBfr !=NULL)
			OSS_FREE(pSioChan->InBfr);

		if ( pSioChan->outBfr != NULL)
			OSS_FREE(pSioChan->outBfr);

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
	CP210X_SIO_CHAN *pPhysDev = pIrp->userPtr;

    // check whether the IRP was for bulk out/ bulk in / status transport
    // 检查这个IRP是否是用于BULK OUT/BULK IN的状态报告的。
    if (pIrp == &(pPhysDev->outIrp) ){
    	// check the result of IRP
        if (pIrp->result == OK){
        	printf("cp210xIrpCallback: Num of Bytes transferred on "\
        			"out pipe is:%d\n",pIrp->bfrList[0].actLen);
        }else{
        	printf("cp210xIrpCallback: Irp failed on Bulk Out %x \n",pIrp->result);

            /* Clear HALT Feature on Bulk out Endpoint */
            if ((usbdFeatureClear (cp210xHandle,cp210xNodeId,USB_RT_ENDPOINT,
            		USB_FSEL_DEV_ENDPOINT_HALT,(pPhysDev->outEpAddr & 0xFF)))
				 != OK){
                printf("cp210xIrpCallback: Failed to clear HALT "\
                              "feauture on bulk out Endpoint\n");
            }
        }
    }else if (pIrp == &(pPhysDev->statusIrp))    /* if status block IRP */
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
            		USB_FSEL_DEV_ENDPOINT_HALT,(pPhysDev->inEpAddr & 0xFF)))
            		!= OK){
            	printf ("cp210xIrpCallback: Failed to clear HALT "\
                              "feature on bulk in Endpoint %x\n");
            }
    	}
    }

    OSS_SEM_GIVE (pPhysDev->cp210xIrpSem);
}
//=========================================================================
void cp210x_get_termios_port(CP210X_SIO_CHAN *pSioChan)
{
	/*
	* usbdVendorSpecific允许一个客户发送厂商指定的USB请求，特殊的设备可能需要执行特定的
	* 厂商请求，函数原型为：
	* STATUS usbdVendorSpecific(
	USBD_CLIENT_HANDLE clientHandle,	// Client handle
	USBD_NODE_ID nodeId,		// Node Id of device/hub
	UINT8 requestType,			// bmRequestType in USB spec.
	UINT8 request,			// bRequest in USB spec.
	UINT16 value,			// wValue in USB spec.
	UINT16 index,			// wIndex in USB spec.
	UINT16 length,			// wLength in USB spec.
	pUINT8 pBfr,			// ptr to data buffer
	pUINT16 pActLen			// actual length of IN
	)
	* requestType，request，value，index，length分别对应于USB Specfication中的
	* bmRequestType, bRequest, wValue, wIndex,wLength域。如果length比0要大，
	* 那么pBfr必须是一个指向数据缓冲区的非空指针，这个缓冲区用来接收数据或者
	* 存储要发送的数据(依据于传输的方向)
	*
	* 通过这个函数发送的厂商指定请求会直接发给NodeId指定的设备的控制管道。这个函数根据
	* 参数来形成和发送一个Setup packet，如果提供了一个非空pBfr，那么Additional IN或者
	* OUT传输将会跟随在这和Setup packet之后。传输的方向由requestType参数的方向比特域指定。
	*
	* 对于IN传输，如果pActLen参数非空的话，传入数据的实际长度会存储在pActLen当中。
	*
	* 返回值是OK或者是 ERROR(如果不能执行vendor-specific请求)
	* */

//	UINT8 partnum;
//	int length = sizeof(partnum)+1;
	int length = 0;
	UINT16 actLength;

/*	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_DEVICE_TO_HOST,
			CP210X_VENDOR_SPECIFIC,CP210X_GET_PARTNUM,
			pSioChan->interface,length,(pUINT8)&partnum,&actLength)!= OK){
		printf("Failed to send vendor specific \n");
	}else{
//		printf("createSioChan: Success to send vendor specific \n");
		printf("partnum:%d,len=%d\n",partnum,actLength);
	}*/


	/*if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_IFC_ENABLE,UART_ENABLE,
			pSioChan->interface,0,NULL,&actLength) != OK){
		printf("Failed to set CP210X_IFC_ENABLE\n");
	}else{
		printf("success to set CP210X_IFC_ENABLE.actLength:%d\n",actLength);
	}*/

	/*获取波特率*/
	UINT32 baud;
	length = sizeof(UINT32)+1;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baud,&actLength)!= OK){
		printf("Failed to send vendor specific \n");
	}else{
		printf("Success CP210X_GET_BAUDRATE baud is:%d,length:%d,actLength:%d\n",baud,length,actLength);
	}

	UINT16 bits;
	length = sizeof(UINT16)+1;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bits,&actLength)!= OK){
		printf("Failed to CP210X_GET_LINE_CTL,actLength != length \n");
	}else{
		printf("Success CP210X_GET_LINE_CTL bits:0x%x,length:%d,actLength:%d\n",bits,length,actLength);
	}

//bits = 0x0800  BITS_DATA_MASK = 0x0f00  BITS_DATA_5 = 0x0500
	switch(bits & BITS_DATA_MASK){
	case BITS_DATA_5:
		printf("data bits = 5\n");
		break;

	case BITS_DATA_6:
		printf("data bits = 6\n");
		break;

	case BITS_DATA_7:
		printf("data bits = 7\n");
		break;

	case BITS_DATA_8:
		printf("data bits = 8\n");
		break;

	case BITS_DATA_9:
		printf("data bits = 9(not supported,using 8 data bits)\n");
	/*	bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bits,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set to 8 data\n");*/
		break;

	default:
		printf("unknow number of data bits.using 8\n");
	/*	bits &= ~BITS_DATA_MASK;
		bits |= BITS_DATA_8;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bits,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set to 8 data\n");*/
		break;
	}

	switch(bits & BITS_PARITY_MASK){
	case BITS_PARITY_NONE:
		printf("patiry = NONE\n");
		break;

	case BITS_PARITY_ODD:
		printf("patiry = ODD\n");
		break;

	case BITS_PARITY_EVEN:
		printf("patiry = EVEN\n");
		break;

	case BITS_PARITY_MASK:
		printf("patiry = MASK\n");
		break;

	case BITS_PARITY_SPACE:
		printf("patiry = SPACE\n");
		break;

	default :
		printf("Unknow parity mode.disabling patity\n");
	/*	bits &= ~BITS_PARITY_MASK;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bits,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set parity to NONE\n");*/
		break;
	}

	switch (bits & BITS_STOP_MASK){
	case BITS_STOP_1:
		printf("stop bits = 1\n");
		break;

	case BITS_STOP_1_5:
		printf("stop bits = 1.5\n");
		break;

	case BITS_STOP_2:
		printf("stop bits = 2\n");
		break;

	default:
		printf("Unknown number of stop bits,using 1 stop bit\n");
		bits &= ~BITS_STOP_MASK;
/*
		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bits,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set STOP_BIT to 1\n");*/
		break;
	}



	CP210X_FLOW_CTL flow_ctl;
	length = sizeof(CP210X_FLOW_CTL)+1;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_FLOW,0,pSioChan->interface,
			length,(pUINT8)&flow_ctl,&actLength)!= OK){
		printf("Failed to CP210X_GET_FLOW \n");
	}else{
		printf("Success to CP210X_GET_FLOW \n");
		if(flow_ctl.ulControlHandshake & CP210X_SERIAL_CTS_HANDSHAKE){
			printf("flow control = CRTSCTS\n");
		}else{
			printf("flow control = NONE\n");
		}
		/*printf("flow_ctl.ulControlHandshake:%d,flow_ctl.ulFlowReplace:%d,\n"\
				"flow_ctl.ulXoffLimit:%d,flow_ctl.ulXonLimit:%d\n",\
				flow_ctl.ulControlHandshake,flow_ctl.ulFlowReplace,flow_ctl.ulXoffLimit,flow_ctl.ulXonLimit);*/
	}


	/*设置波特率*/
	baud = 115200;
	length = sizeof(baud);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pSioChan->interface,length,(pUINT8)&baud,&actLength) != OK){
		printf("Failed CP210X_SET_BAUDRATE \n");
	}else{
		printf("Success CP210X_SET_BAUDRATE length:%d,actLength:%d\n",length,actLength);
	}

/*再次获取波特率，确认是否写入*/
	UINT32 baudNow = 0;
	length = sizeof(UINT32)+1;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baudNow,&actLength)!= OK){
		printf("createSioChan: Failed to  CP210X_GET_BAUDRATE\n");
	}else{
		printf("Success CP210X_GET_BAUDRATE baudNow:%d,length:%d,actLength:%d\n",baudNow,length,actLength);
	}

//设置为7个数据位
	usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
				CP210X_GET_LINE_CTL,0,pSioChan->interface,
				length,(pUINT8)&bits,&actLength);

	printf("original:bits:%x",bits);
	bits &= ~BITS_DATA_MASK;
	bits |= BITS_DATA_8;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to SET_LINE_CONTROL\n");
	}else{
		printf("Success SET_LINE_CONTROL DATA_BIT,actLength:%d\n",actLength);
	}

//设置奇偶校验为EVEN
	usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
				CP210X_GET_LINE_CTL,0,pSioChan->interface,
				length,(pUINT8)&bits,&actLength);
	printf("after set DATA_BIT.bits:%x",bits);
	bits &= ~BITS_PARITY_MASK;
	bits |= BITS_PARITY_NONE;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pSioChan->interface,0,NULL,&actLength) != OK){
			printf("Failed to SET_LINE_CONTROL\n");
		}else{
			printf("Success SET_LINE_CONTROL_PARITY,actLength:%d\n",actLength);
		}

//设置停止位为2
	usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
				CP210X_GET_LINE_CTL,0,pSioChan->interface,
				length,(pUINT8)&bits,&actLength);
	printf("after set PARITY_BIT.bits:%x",bits);
	bits &= ~BITS_STOP_MASK;
	bits |= BITS_STOP_1;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pSioChan->interface,0,NULL,&actLength) != OK){
		printf("Failed to SET_LINE_CONTROL\n");
	}else{
		printf("Success to SET_LINE_CONTROL STOP_BIT,actLength:%d\n",actLength);
	}


/*重新查看当前bits设置*/
	UINT16 bitsNow;
	length = sizeof(bitsNow)+1;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bitsNow,&actLength)!= OK){
		printf("Failed to CP210X_GET_LINE_CTL,actLength != length \n");
	}else{
		printf("Success CP210X_GET_LINE_CTL bitsNow:0x%x,length:%d,actLength:%d\n",bitsNow,length,actLength);
	}

//bitsNow = 0x0800  BITS_DATA_MASK = 0x0f00  BITS_DATA_5 = 0x0500
	switch(bitsNow & BITS_DATA_MASK){
	case BITS_DATA_5:
		printf("data bitsNow = 5\n");
		break;

	case BITS_DATA_6:
		printf("data bitsNow = 6\n");
		break;

	case BITS_DATA_7:
		printf("data bitsNow = 7\n");
		break;

	case BITS_DATA_8:
		printf("data bitsNow = 8\n");
		break;

	case BITS_DATA_9:
		printf("data bitsNow = 9(not supported,using 8 data bitsNow)\n");
		bitsNow &= ~BITS_DATA_MASK;
		bitsNow |= BITS_DATA_8;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bitsNow,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set to 8 data\n");
		break;

	default:
		printf("unknow number of data bitsNow.using 8\n");
		bitsNow &= ~BITS_DATA_MASK;
		bitsNow |= BITS_DATA_8;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bitsNow,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set to 8 data\n");
		break;
	}

	switch(bitsNow & BITS_PARITY_MASK){
	case BITS_PARITY_NONE:
		printf("patiry = NONE\n");
		break;

	case BITS_PARITY_ODD:
		printf("patiry = ODD\n");
		break;

	case BITS_PARITY_EVEN:
		printf("patiry = EVEN\n");
		break;

	case BITS_PARITY_MASK:
		printf("patiry = MASK\n");
		break;

	case BITS_PARITY_SPACE:
		printf("patiry = SPACE\n");
		break;

	default :
		printf("Unknow parity mode.disabling patity\n");
	/*	bitsNow &= ~BITS_PARITY_MASK;

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bitsNow,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set parity to NONE\n");*/
		break;
	}

	switch (bitsNow & BITS_STOP_MASK){
	case BITS_STOP_1:
		printf("stop bitsNow = 1\n");
		break;

	case BITS_STOP_1_5:
		printf("stop bitsNow = 1.5\n");
		break;

	case BITS_STOP_2:
		printf("stop bitsNow = 2\n");
		break;

	default:
		printf("Unknown number of stop bitsNow,using 1 stop bit\n");
		bitsNow &= ~BITS_STOP_MASK;

	/*	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
				CP210X_SET_LINE_CTL,bitsNow,pSioChan->interface,
				0,NULL,&actLength) != OK)
			printf("can not set STOP_BIT to 1\n");*/
		break;
	}

	sleep(2);

/*设置流控*/
//	UINT32 ctl_hs;
//	UINT32 flow_repl;
//
//	ctl_hs = flow_ctl.ulControlHandshake;
//	flow_repl = flow_ctl.ulFlowReplace;
//	printf("read ulControlHandshake:0x%08x,ulFlowReplace:0x%08x\n");
//	ctl_hs &= ~CP210X_SERIAL_DSR_HANDSHAKE;
//	ctl_hs &= ~CP210X_SERIAL_DCD_HANDSHAKE;
//	ctl_hs &= ~CP210X_SERIAL_DSR_SENSITIVITY;
//	ctl_hs &= ~CP210X_SERIAL_DTR_MASK;
//	ctl_hs &= ~CP210X_SERIAL_DTR_SHIFT(CP210X_SERIAL_DTR_ACTIVE);
//
//	ctl_hs |= CP210X_SERIAL_CTS_HANDSHAKE;
//
//	flow_repl &= ~CP210X_SERIAL_RTS_MASK;
//	flow_repl |= CP210X_SERIAL_RTS_SHIFT(CP210X_SERIAL_RTS_FLOW_CTL);
//	printf("flow control = CRTSCTS");
//	printf("write ulControlHandleShake=0x%08x.ulFlowReplace=0x%08x\n",ctl_hs,flow_repl);
//
//	flow_ctl.ulControlHandshake = ctl_hs;
//	flow_ctl.ulFlowReplace = flow_repl;
//	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_FLOW,0,pSioChan->interface,0,NULL,&actLength)!=OK){
//		printf("createSioChan: Failed to send CP210X_SET_FLOW \n");
//	}else{
//		printf("createSioChan: Success to send CP210X_SET_FLOW \n");
//	}


}




/***************************************************************************
*
* configureSioChan - configure USB printer for operation
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

LOCAL STATUS configureSioChan(CP210X_SIO_CHAN *pSioChan){
	USB_CONFIG_DESCR 	* pCfgDescr;//配置描述符指针
	USB_INTERFACE_DESCR * pIfDescr;	//接口描述符指针
	USB_ENDPOINT_DESCR 	* pOutEp;	//OUT ENDPOINT 指针
	USB_ENDPOINT_DESCR 	* pInEp;	//IN ENDPOINT指针
	UINT8 * pBfr;                	//指向描述符存储区域的指针
	UINT8 * pScratchBfr;			// another pointer to the above store
	UINT16 actLen;					//描述符的实际长度
	UINT16 maxPacketSize;			//端点的最大数据包长

    //	为存储描述符分配空间
    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	return ERROR;

    /* Read the configuration descriptor to get the configuration selection
     * value and to determine the device's power requirements.
     * Configuration index is assumed to be one less than config'n value
     */

    if (usbdDescriptorGet (cp210xHandle, pSioChan->nodeId,
    			USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,
    			pSioChan->configuration, 0, USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
    	OSS_FREE (pBfr);
    	return ERROR;
    }

	if ((pCfgDescr = usbDescrParse (pBfr, actLen,
			USB_DESCR_CONFIGURATION)) == NULL){
        OSS_FREE (pBfr);
        return ERROR;
	}

//	printf("pSioChan->configuration is %d\n",pSioChan->configuration);
//	printf("pCfgDescr->configurationValue is:%d\n",pCfgDescr->configurationValue);
	pSioChan->configuration = pCfgDescr->configurationValue;



	//	 * 由于使用了NOTIFY_ALL来注册设备，注册返回函数的配置和接口数量没有
	//	 * 任何意义，
	//	 * 	参考芯片的文档发现它只提供了一个接口0.所以第一个接口就
	//	 * 	是我们需要的接口

	//	 *usbdDescrParseSkip()修改它所收到的指针的值，所以我们将其
	//	 *保存一份。

    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */


	UINT16 ifNo = 0;

	pScratchBfr = pBfr;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,
			USB_DESCR_INTERFACE))!= NULL){
		if (ifNo == pSioChan->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pSioChan->interface = pIfDescr->interfaceNumber;

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
		printf("usbdConfigurationSet() return ERROR\n");
        OSS_FREE (pBfr);
        return ERROR;
    }
//	printf("usbdConfigurationSet() return OK\n");

    /* Select interface
     *
     * NOTE: Some devices may reject this command, and this does not represent
     * a fatal error.  Therefore, we ignore the return status.
     */

	//	pSioChan->interfaceAltSetting = pIfDescr->alternateSetting;
  /*  if(usbdInterfaceSet(cp210xHandle,cp210xNodeId,pSioChan->interface,\
			pIfDescr->alternateSetting) != OK){
    	printf("configureSioChan: usbdInterfaceSet return ERROR,we just ignore it.\n");
	}
	printf("configureSioChan: usbdInterfaceSet return OK.\n");*/




	cp210x_get_termios_port(pSioChan);


    /* Create a pipe for output . */
	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) |
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,
			0,0,&pSioChan->outPipeHandle)!= OK){
		printf("out pipe create failed\n");
		OSS_FREE (pBfr);
		return ERROR;
	}
//	printf("create out pipe OK\n");


	maxPacketSize = *((pUINT8)&pInEp->maxPacketSize) |
			(*(((pUINT8)&pInEp->maxPacketSize)+1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pInEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_IN,maxPacketSize,
			0,0,&pSioChan->inPipeHandle)!= OK){
		printf("in pipe create failed\n");
		OSS_FREE (pBfr);
		return ERROR;
	}
//	printf("create in pipe OK\n");

#if 1
	/*usb spec中规定，对于设备的bulk端点，每当设备在reset之后，需要清除halt这个feature，然后端点
	 * 才能够正常的工作，所以在reset相关的函数当中都会调用需要调用清楚halt的函数。
	 *
	 * USB spec中规定对于使用了data toggle的endpoint，不管其halt feature是否被设置，只要调用了clear
	 * feature，那么其data toggle总是会被初始化为DATA0
	 *
	 * 中断端点和Bulk端点有一个HALT特征，其实质是寄存器中的某一个比特位，若该比特位
	 * 设置为1，就表示设置了HALT特征，那么这个端点就不能正常工作，要使这个端点正常工作
	 * 则应该清除这个bit位*/
	/* Clear HALT feauture on the endpoints */
	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT, (pOutEp->endpointAddress & 0xFF)))
			!= OK){
		printf("configureSioChan: Failed to clear HALT feauture "\
					  "on bulk out Endpoint \n");
	}
	printf("configureSioChan: Success to clear HALT feauture "\
						  "on bulk out Endpoint \n");

	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT,(pInEp->endpointAddress & 0xFF)))
			!= OK){
		printf("configureSioChan: Failed to clear HALT feauture "\
					  "on bulk in Endpoint \n");
	}
	printf("configureSioChan: Success to clear HALT feauture "\
						  "on bulk in Endpoint \n");
#endif



	//cp210x_get_termios_port(pSioChan);

	char a[8]="0000000";
    /* Initialize IRP */

    pSioChan->outIrp.transferLen = sizeof (a);
    pSioChan->outIrp.result = -1;
//    pSioChan->outIrp.dataToggle = USB_DATA0;

    pSioChan->outIrp.bfrCount = 1;
    pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
    pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)a;
    pSioChan->outIrp.bfrList [0].bfrLen = sizeof (a);

    /* Submit IRP */
    if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
    	printf("usbdtransfer returned false\n");
//    	return FALSE;
    }
//    printf("result:%d",pSioChan->outIrp.result);

sleep(2);

        char b[8]="0101010";
        /* Initialize IRP */

        pSioChan->outIrp.transferLen = sizeof (b);
        pSioChan->outIrp.result = -1;
    //    pSioChan->outIrp.dataToggle = USB_DATA0;

        pSioChan->outIrp.bfrCount = 1;
        pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
        pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)b;
        pSioChan->outIrp.bfrList [0].bfrLen = sizeof (b);

        /* Submit IRP */
        if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
        	printf("usbdtransfer returned false\n");
    //    	return FALSE;
        }

sleep(2);
        		char c[8]="BBBBBBB";
               /* Initialize IRP */

               pSioChan->outIrp.transferLen = sizeof (c);
               pSioChan->outIrp.result = -1;
           //    pSioChan->outIrp.dataToggle = USB_DATA0;

               pSioChan->outIrp.bfrCount = 1;
               pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
               pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)c;
               pSioChan->outIrp.bfrList [0].bfrLen = sizeof (c);

               /* Submit IRP */
               if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
               	printf("usbdtransfer returned false\n");
           //    	return FALSE;
               }
               sleep(2);

               	   char d[8]="AAAAAAA";
                   pSioChan->outIrp.transferLen = sizeof (d);
                   pSioChan->outIrp.result = -1;
                   pSioChan->outIrp.bfrCount = 1;
                   pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
                   pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)d;
                   pSioChan->outIrp.bfrList [0].bfrLen = sizeof (d);
                   if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
                   	printf("usbdtransfer returned false\n");
                   }

                   sleep(2);

					char e[8]="0303030";
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

				   char f[8]={0,0,0,0,0,0,0,0};
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

			/*if (listenForInput (pSioChan) != OK)
            {
            OSS_FREE (pBfr);
            return ERROR;
            }*/
//	pSioChan->inIrpInUse = FALSE;
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
LOCAL CP210X_SIO_CHAN * createSioChan(CP210X_SIO_CHAN *pSioChan)
{
	printf("IN createSioChan\n");

//	 Try to allocate space for a new keyboard struct
	if ((pSioChan->outBfr = OSS_CALLOC(CP210X_OUT_BFR_SIZE))== NULL ){
//		destorySioChan(pSioChan);
		return NULL;
	}

	if((pSioChan->InBfr = OSS_CALLOC(CP210X_IN_BFR_SIZE)) == NULL){
//		destorySioChan(pSioChan);
		return NULL;
	}

	pSioChan->txIrpIndex = 0;
	pSioChan->txStall = FALSE;
	pSioChan->txActive = FALSE;

	pSioChan->outBfrLen = CP210X_OUT_BFR_SIZE;
	pSioChan->inBfrLen = CP210X_IN_BFR_SIZE;

//	pSioChan->sioChan.pDrvFuncs = &cp210xSioDrvFuncs;
	pSioChan->mode = SIO_MODE_POLL;

	//初始化Device的OutIrp
	pSioChan->outIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->outIrp.userCallback		= cp210xIrpCallback;
	pSioChan->outIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->outIrp.bfrCount          = 0x01;
	pSioChan->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pSioChan->outIrp.userPtr           = pSioChan;

	//初始化Device的InIrp
	pSioChan->inIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->inIrp.userCallback		= cp210xIrpCallback;
	pSioChan->inIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->inIrp.bfrCount          = 0x01;
	pSioChan->inIrp.bfrList[0].pid    = USB_PID_IN;
	pSioChan->inIrp.userPtr           = pSioChan;

	//初始化Device的StatusIrp
	pSioChan->statusIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->statusIrp.userCallback		= cp210xIrpCallback;
	pSioChan->statusIrp.timeout          	= usbCp210xIrpTimeOut;
	pSioChan->statusIrp.transferLen      	= USB_CSW_LENGTH;  //USB_CSW_LENGTH 是什么用途？Length of CSW
	pSioChan->statusIrp.bfrCount         	= 0x01;
	pSioChan->statusIrp.bfrList[0].pid    	= USB_PID_IN;
	pSioChan->statusIrp.bfrList[0].pBfr    = (UINT8 *)(&pSioChan->bulkCsw);
	pSioChan->statusIrp.bfrList[0].bfrLen 	= USB_CSW_LENGTH;
	pSioChan->statusIrp.userPtr           	= pSioChan;

	if (OSS_SEM_CREATE( 1, 1, &pSioChan->cp210xIrpSem) != OK){
		return NULL;
//        return(cp210xShutdown(S_cp210xLib_OUT_OF_RESOURCES));
	}

	if(configureSioChan(pSioChan) != OK){
//		destorySioChan(pSioChan);
		return NULL;
	}

	return pSioChan;
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
	);
每次attachCallback被调用的时候，USBD会传递该设备的nodeid和attachAction给这个回调函数。
当attachAction是USBD_DYNA_REMOVE的时候，表明指向的nodeid不再有效，此时client应该查询这个nodeid的
永久数据结构并且删除任何和这个nodeid相关联的结构，如果client有明显的对这个nodeid的请求，
例如：数据传输请求，那么USBD就会在调用attachCallback通知client这个设备被移除之前设置这些显式的请求为无效。
通常与一个被移除的设备相关的数据传输请求在attachCallback被调用之前就应该被小心的对待。
	client可能会为多个通知登记重复使用一个attachCallback，为了方便起见，
USBD也会在每个设备attach/remove的时候传递该设备的class/subclass/protocol给attachCallback。
最后需要注意的是不是所有的设备都会在设备级来报告class信息，一些设备在interface-to-interface
的基础上报告class信息。当设备在设备级报告class信息时，USBD会在attachCallback中传递configuration的值为0，
并且每一个设备只会调用一次attachCallback。当设备在interface级报告class信息时，USBD会为每一个接口唤醒一次
attachCallback，只要这个接口适配client的class/subclass/protocol，
在这种情况下，USBD也会在每一次调用attachCallback时传递对应的configuration和interface的值
*/

LOCAL STATUS cp210xLibAttachCallback( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,UINT16 interface,
		UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
	printf("IN cp210xLibAttachCallback\n");

	CP210X_SIO_CHAN *pSioChan;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int noOfSupportedDevices =(sizeof(cp210xAdapterList)/(2*sizeof(UINT16))) ;
    printf("noOfSupportedDevices is:%d\n",noOfSupportedDevices);
    int index = 0;

    if((pBfr = OSS_MALLOC(USB_MAX_DESCR_LEN)) == NULL)
    	return ERROR;

    OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	switch(attachAction){
	case USBD_DYNA_ATTACH:
		/* 发现新设备
		 *  Check if we already  have a structure for this device.*/
		if(cp210xFindDev(nodeId)!= NULL){
			printf("device already exists\n");
			break;
		}

		/*确保这是一个cp210x设备*/
	      if (usbdDescriptorGet (cp210xHandle, nodeId, USB_RT_STANDARD | USB_RT_DEVICE,
	    		  USB_DESCR_DEVICE, 0, 0, 36, pBfr, &actLen) != OK){
	    	  printf("usbdDescriptorGet() function failed!\n");
	    	  break;
	      }

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
	   		/* device not supported，不支持该设备 */
	    	  printf( " Unsupported device found vId %0x; pId %0x!!! \n",\
	    			  vendorId, productId);
	    	  break;
	   		}else{
	   			printf("Found device vId %0x; pId %0x!!!\n",vendorId,productId);
	   		}


	      //为新设备创建结构体，并将结构体加入设备列表
	      if((pSioChan = OSS_CALLOC(sizeof(CP210X_SIO_CHAN))) == NULL ){
	    	  break;
	      }
	      pSioChan->nodeId = nodeId;
	      pSioChan->configuration = configuration;
	      pSioChan->interface = interface;
	      pSioChan->vendorId = vendorId;
	      pSioChan->productId = productId;

	      /*保存一个nodeId的全局变量,*/
	      cp210xNodeId = nodeId;

	      /* Create a new structure to manage this device.  If there's an error,
		   * there's nothing we can do about it, so skip the device and return immediately.
		   */
		  if((createSioChan(pSioChan)) == NULL)
			  break;

	      /*将设备加入设备链表尾部*/
	      usbListLink (&cp210xDevList, pSioChan, &pSioChan->cp210xDevLink,LINK_TAIL);

	      /*将设备标记为已连接*/
	      pSioChan->connected = TRUE;

	      /* Notify registered callers that a new cp210x has been added
	       * and a new channel created.*/
//	      notifyAttach (pSioChan, CP210X_ATTACH);
	      break;



	case USBD_DYNA_REMOVE:
		 /* A device is being detached.
		  * Check if we have any structures to manage this device.*/
		if ((pSioChan = cp210xFindDev (nodeId)) == NULL){
			printf("Device not found\n");
			break;
		}

		/* The device has been disconnected. */
		if(pSioChan->connected == FALSE){
			printf("device not connected\n");
			break;
		}

		pSioChan->connected = FALSE;

		/* Notify registered callers that the keyboard has been removed and the channel disabled.
		 * NOTE: We temporarily increment the channel's lock count to prevent usbttySioChanUnlock() from destroying the
		 * structure while we're still using it.*/
		pSioChan->lockCount++;
//		notifyAttach (pSioChan, CP210X_REMOVE);

		pSioChan->lockCount--;

		/* If no callers have the channel structure locked, destroy it now.
		 * If it is locked, it will be destroyed later during a call to usbttyUnlock().*/
		if (pSioChan->lockCount == 0)
			cp210xDestroyDevice (pSioChan);

		break;
	}

	OSS_FREE(pBfr);
	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}

/***************************************************************************
*
* cp210xShutdown - shuts down USB EnetLib
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

STATUS cp210xShutdown(int errCode)
{
	printf("IN cp210xShutdown\n");
    CP210X_SIO_CHAN * pSioChan;

    /* Dispose of any open connections. */
    while ((pSioChan = usbListFirst (&cp210xDevList)) != NULL)
	cp210xDestroyDevice (pSioChan);

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

//    if (cp210xTxMutex != NULL){
//    	OSS_MUTEX_DESTROY (cp210xTxMutex);
//    	cp210xTxMutex = NULL;
//	}
//
//    if (cp210xRxMutex != NULL){
//    	OSS_MUTEX_DESTROY (cp210xRxMutex);
//    	cp210xRxMutex = NULL;
//	}

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
	printf("IN cp210xDevInit\n");
	if(initCount == 0){
		memset(&cp210xDevList,0,sizeof(cp210xDevList));
		memset(&reqList,0,sizeof(reqList));

		cp210xMutex		 	= NULL;
		cp210xHandle		= NULL;

		if (OSS_MUTEX_CREATE (&cp210xMutex) != OK)
			return cp210xShutdown (S_cp210xLib_OUT_OF_RESOURCES);

/*		if (usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK || usbdDynamicAttachRegister (cp210xHandle,
			CP210X_CLASS,CP210X_SUB_CLASS,CP210X_DRIVER_PROTOCOL,TRUE,cp210xLibAttachCallback)!= OK)*/
		if (usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK ||
				usbdDynamicAttachRegister (cp210xHandle,CP210X_CLASS,CP210X_SUB_CLASS,CP210X_DRIVER_PROTOCOL,TRUE,
						(USBD_ATTACH_CALLBACK)cp210xLibAttachCallback)!= OK){
			printf("usbd register failed!\n");
			return cp210xShutdown (S_cp210xLib_USBD_FAULT);
		}

		printf("usbdClientRegister() returned OK,usbdDynamicAttachRegister() returned OK\n");
	}
	initCount++;
	return OK;
}

/***************************************************************************
*
* cp210xDynamicAttachRegister - register USB-RS232 device attach callback
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
/*STATUS cp210xDynamicAttachRegister(
		CP210X_ATTACH_CALLBACK callback,	// new callback to be registered
	    pVOID arg		    				// user-defined arg to callback
){
	printf("IN cp210xDynamicAttachRegister\n");

	pATTACH_REQUEST pRequest;
	CP210X_SIO_CHAN * pSioChan;
	int status = OK;

	if (callback == NULL)
		return (ossStatus (S_cp210xLib_BAD_PARAM));
	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	// Create a new request structure to track this callback request.
	if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL){
		status = ERROR;
	}else{
			pRequest->callback = callback;
			pRequest->callbackArg = arg;

			usbListLinkProt (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL,cp210xMutex);

//			 * Perform an initial notification of all
//			 * currrently attached devices.
			pSioChan = usbListFirst (&cp210xDevList);
			while (pSioChan != NULL){
				if (pSioChan->connected)
					(*callback) (arg,pSioChan,CP210X_ATTACH);

				pSioChan = usbListNext (&pSioChan->cp210xDevLink);
			}
		}

	OSS_MUTEX_RELEASE (cp210xMutex);
	return ossStatus (status);
}*/

/***************************************************************************
*
* cp210xDynamicAttachUnregister - unregisters USB-RS232 attach callbackx
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

/*
STATUS cp210xDynamicAttachUnRegister(
		CP210X_ATTACH_CALLBACK callback,	// callback to be unregistered
		pVOID arg		    				// user-defined arg to callback
){
	printf("IN cp210xDynamicAttachUnRegister\n");

    pATTACH_REQUEST pRequest;
//    CP210X_SIO_CHAN * pSioChan = NULL;
    int status = S_cp210xLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL){
    	if (callback == pRequest->callback && arg == pRequest->callbackArg){
    		//Perform a notification of all currrently attached devices.

    		//发现一个匹配的请求
    		usbListUnlink(&pRequest->reqLink);

			//删除结构体
			OSS_FREE(pRequest);
			status = OK;
			// We found a matching notification request.
			//destroyAttachRequest (pRequest);
			//status = OK;
			break;
    	}
    	pRequest = usbListNext (&pRequest->reqLink);
    }
    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}
*/

/***************************************************************************
*
* cp210xDevLock - marks CP210X_SIO_CHAN structure as in use
*
* A caller uses cp210xDevLock() to notify cp210xDevLib that
* it is using the indicated USB-RS232 device structure.  cp210xDevLib maintains
* a count of callers using a particular Cp210x Device structure so that it
* knows when it is safe to dispose of a structure when the underlying
* Cp210x Device is removed from the system.  So long as the "lock count"
* is greater than zero, cp210xDevLib will not dispose of an Cp210x
* structure.
*
* RETURNS: OK, or ERROR if unable to mark Cp210x structure in use.
*
* ERRNO: none
*/

/*STATUS cp210xDevLock(USBD_NODE_ID nodeId ){
	printf("IN cp210xDevLock\n");
    CP210X_SIO_CHAN * pCp210xDev = cp210xFindDevice (nodeId);

    if ( pCp210xDev == NULL)
    	return (ERROR);

    pCp210xDev->lockCount++;
    return (OK);
}*/


/***************************************************************************
*
* cp210xDevUnlock - marks USB_PEGASUS_DEV structure as unused
*
* This function releases a lock placed on an Pegasus Device structure.  When a
* caller no longer needs an Pegasus Device structure for which it has previously
* called cp210xDevLock(), then it should call this function to
* release the lock.
*
* NOTE
* If the underlying Pegasus device has already been removed
* from the system, then this function will automatically dispose of the
* Pegasus Device structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the Pegasus Device structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark Pegasus Device structure unused.
*
* ERRNO:
* \is
* \i S_cp210xLib_NOT_LOCKED
* No lock to Unlock
* \ie
*/
/*STATUS cp210xDevUnlock(USBD_NODE_ID nodeId)
{
	printf("IN cp210xDevUnlock,just return OK\n");
    int status = OK;
    CP210X_SIO_CHAN * pSioChan = cp210xFindDevice (nodeId);

    if(pSioChan == NULL){
    	return ERROR;
    }

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0){
    	status = S_cp210xLib_NOT_LOCKED;
    }else{
//	 If this is the last lock and the underlying USB-RS232 device
//	 is no longer connected, then dispose of the keyboard.
    	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
    		cp210xDestroyDevice ((CP210X_SIO_CHAN *)pSioChan->pCp210xDevice);
    	printf("USB to RS232 device destoryed in DevUnlock!\n");
    }
    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}*/
//===============================================================================================//
/* start a output transport
 * */
/*LOCAL int cp210xTxStartUp(SIO_CHAN *pChan)
{
	printf("IN cp210xTxStartUp\n");
//TODO
	return EIO;
}*/



//===============================================================================================//
/*cp210xCallbackInstall - install ISR callbacks to get/put chars
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.
 * */

/*LOCAL int cp210xCallbackInstall(SIO_CHAN *pChan,int callbackType,
		STATUS(*callback)(void *tmp,...),void *callbackArg)
{
	printf("IN cp210xCallbackInstall now\n");
				return OK;
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
}*/


//===============================================================================================//
/*
 * output a character in polled mode.
 * */
/*LOCAL int cp210xPollOutput(SIO_CHAN *pChan,char outChar)
{
	printf("IN cp210xPollOutput");
	//TODO
	return OK;
}*/


//===============================================================================================//
/*
 * poll the device for input
 * */
/*LOCAL int cp210xPollInput(SIO_CHAN *pChan,char *thisChar)
{
	printf("IN cp210xPollInput\n");
	return OK;
	CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *)pChan;
	int status = OK;

	if(thisChar == NULL)
		return EIO;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);
//	 Check if the input queue is empty.

	if (pSioChan->inQueueCount == 0)
		status = EAGAIN;
	else{
//	 Return a character from the input queue.
		*thisChar = nextInChar (pSioChan);
	}

//TODO

	OSS_MUTEX_RELEASE (cp210xMutex);
	return status;
}*/

//===============================================================================================//
/*
 * cp210xIoctl - special device control
 * this routine is largely a no-op for the function.c the only ioctls
 * which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
 *
 *
 * */
/*LOCAL int cp210xIoctl(SIO_CHAN *pChan,	     //device to control
		int request,	 //request code
		void *someArg	 //some argument
		){
	printf("IN cp210xIoctl now\n");
	return OK;
}*/



//=============================2017.06.20 18:30 END      ============================================//








//=============================2017.06.20 10:30 BEGIN    A totally new part==========================//


#include "cp210xLib.h"

typedef struct usb_tty_node{
	NODE 	node;
	struct usb_tty_dev *pusbttyDev;
}USB_TTY_NODE;

typedef struct usb_tty_dev
{
	/*I/O子系统在调用底层驱动函数时，将使用设备结构作为函数调用的第一个参数，这个设备结构是底层驱动自定义的，
	 * 用以保存底层硬件设备的关键参数。对于自定义的设备结构必须将内核结构DEV_HDR作为自定义设备结构的第一个参数。
	 * 该结构被I/O子系统使用，统一表示系统内的所有设备，即I/O子系统将所有的驱动自定义的设备结构作为DEV_HDR使用，
	 * DEV_HDR之后的字段由各自具体的驱动进行解释和使用，内核并不关心这些字段的含义。*/
	DEV_HDR 		usbttyDev;
	SIO_CHAN 		*pSioChan;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	USB_TTY_NODE *          pusbttyNode;
	SEL_WAKEUP_LIST         selWakeupList;   /*为解决任务列表*/
	SEM_ID                  usbttySelectSem;

	USBD_NODE_ID	usbttyNodeid;
	UINT16 			numOpened;
	UINT16 			outEndpointAddress;		/*Bulk out EP address*/
	UINT16 			inEndpointAddress;		/*BUlk in EP address*/
	UINT16          epMaxPacketSize;		/**/
	UINT8   		maxPower;
	USBD_PIPE_HANDLE outPipeHandle;			/*pipe handle for Bulk out EP*/
	USBD_PIPE_HANDLE inPipeHandle;			/*pipe handle for Bulk in EP*/
	USBD_PIPE_HANDLE ctlPipeHandle;			/*pipe handle for control EP*/
	USB_IRP			inIrp;					/*IRP used for in data*/
	USB_IRP 		outIrp;					/*IRP used for out data*/
	USB_IRP 		statusIrp; 				/*IRP used for reading status*/
	UINT8 			*usbInData;				/*Pointer for USB-in data*/
	UINT8 			*usbOutData;			/*pointer for USB-out data*/
	/*USB-in-data用于指向存储被读取数据的缓冲区，*/
//TODO
}USB_TTY_DEV;




//#define     DESC_PRINT
#define 	__DEBUG__

#ifdef 		__DEBUG__
	#include <D:\LambdaPRO615\target\deltaos\include\debug\utils\stdarg.h>
	void DEBUG(const char *fmt,...)
	{
		va_list ap;
//		va_list (ap,fmt);
		vprintf(fmt,ap);
		va_end(ap);
	}
#else
	void DEBUG(const char *fmt,...){}
#endif


#define USB_TTY_DEVICE_NUM			5
#define USB_TTY_NAME 				"/usbtty/"
#define USB_TTY_NAME_LEN_MAX 		50

STATUS status;
LOCAL SIO_CHAN 				*pusbttyDevArray[USB_TTY_DEVICE_NUM];
USB_TTY_DEV 				*pusbttyDev = NULL;
LOCAL int 					cp210xDrvNum = -1; 	//驱动注册的驱动号
LOCAL LIST					usbttyList;			//系统所有的USBTTY设备
LOCAL SEM_ID 				cp210xListMutex;  	//保护列表互斥信号量
LOCAL SEM_ID				cp210xMutex;			//互斥信号量





#define USB_TTY_LIST_SEM_TAKE(tmout)	semTake(cp210xListMutex,(int)(tmout))
#define	USB_TTY_LIST_SEM_GIVE			semGive(cp210xListMutex)


/*int usb_control_msg(USB_TTY_DEV *pusbttyDev,UINT8 request,\
		 UINT8 requestType,UINT16 value,UINT16 index,void *data,UINT16 length,UINT32 timeout);*/
//STATUS getAllConfiguration(void);

int usbttyDevRead(USB_TTY_DEV *pusbttyDev,  char *buffer,  UINT32 nBytes);
int usbttyDevWrite(USB_TTY_DEV *pusbttyDev, char *buffer,UINT32 nBytes);
int usbttyDevOpen(USB_TTY_DEV *pusbttyDev,  char *name,  int flags,int mode);
int usbttyDevClose(USB_TTY_DEV *pusbttyDev);
int usbttyDevIoctl(USB_TTY_DEV *pusbttyDev,  int request,  void *arg);
STATUS usbttyDevDelete(USB_TTY_DEV *pusbttyDev);
STATUS cp201x_set_config(unsigned char request,unsigned int *data,int size);
LOCAL STATUS usbttyDevCreate(char *,SIO_CHAN *);

//==============================================================================//
/*LOCAL int getusbttyNum(SIO_CHAN *pChan)
{
	int index;
	for(index =0;index<USB_TTY_DEVICE_NUM;index++){
		if(pusbttyDevArray[index] == 0){
			pusbttyDevArray[index] = pChan;
			return (index);
		}
	}
	printf("Out of avilable USB_TTY number.Check USB_TTY_DEVICE＿NUM\n");
	return (-1);
}*/

//==============================================================================//
/*LOCAL int freeusbttyNum(SIO_CHAN *pChan)
{
	int index;
	for (index=0; index < USB_TTY_DEVICE_NUM; index++)
	{
		if (pusbttyDevArray[index] == pChan){
			pusbttyDevArray[index] = NULL;
			return (index);
		}
	}
	printf("Unable to locate USB_TTY pointed to by channel 0x%X.\n",pChan);
	return(-1);
}*/

//==============================================================================//
/*为SIO通道查找USB_TTY_DEV设备*/
/*LOCAL STATUS usbttyDevFind(SIO_CHAN *pChan,USB_TTY_DEV **ppusbttyDev)
{
	USB_TTY_NODE 	*pusbttyNode = NULL ;	指向USB设备节点
	USB_TTY_DEV		*pTempDev;				指向USB_TTY_DEV

	if(pChan == NULL)
		return ERROR;

	防止其他模块干扰
	USB_TTY_LIST_SEM_TAKE(WAIT_FOREVER);

	遍历所有设备
	for(pusbttyNode = (USB_TTY_NODE *) lstFirst (&usbttyList);
			pusbttyNode != NULL;
	        pusbttyNode = (USB_TTY_NODE *) lstNext ((NODE *) pusbttyNode)){
		pTempDev = pusbttyNode->pusbttyDev;
		找到即返回
		if (pTempDev->pSioChan == pChan){
			* (UINT32 *) ppusbttyDev = (UINT32) pTempDev;
			USB_TTY_LIST_SEM_GIVE;
			return (OK);
		}
	}
	USB_TTY_LIST_SEM_GIVE;
	return (ERROR);
}*/

//==============================================================================//
/*void usbttyRcvCallback(){
	printf("IN usbttyRcvCallback()");
}*/


//==============================================================================//
LOCAL void cp210xAttachCallback(void *arg,CP210X_DEV * pDev,UINT16 attachCode)
{
	printf("IN cp210xAttachCallback\n");
//	int usbttyUintNum;
//	USB_TTY_DEV *pusbttyDev = NULL;
//	char cp210xName[USB_TTY_NAME_LEN_MAX];

	/*if(attachCode == CP210X_ATTACH){
		printf("device detected success!\n");
		if(usbttySioChanLock(pChan) != OK){
			printf("usbttySioChanLock() returned ERROR\n");
		}else{
			usbttyUintNum = getusbttyNum(pChan);
			if(usbttyUintNum >= 0){
				sprintf(cp210xName,"%s%d",USB_TTY_NAME,usbttyUintNum);

				创建USB-TTY设备
				if(usbttyDevCreate(cp210xName,pChan) != OK){
					printf("usbttyDevCreate() returned ERROR\n");
					return ;
				}
				printf("usbttyDevCreate() returned OK\n");
			}else{
				printf("Excessive usbttys-check USB_TTY_DEVICE_NUM");
				return;
			}

			if(usbttyDevFind(pChan,&pusbttyDev) != OK){
				printf("usbttyDevFind() returned ERROR\n");
				return;
			}

			if((*pChan->pDrvFuncs->callbackInstall)(pChan,SIO_CALLBACK_PUT_RCV_CHAR,
					(STATUS (*)(void *,...))usbttyRcvCallback,
					(void *)pusbttyDev)!=OK){
				printf("usbKbdRcvCallback() failed to install\n");
				return;
			}

			printf("USB_TTY attach as %s\n",cp210xName);
		}
	}*/

	/*if(attachCode == USB_TTY_REMOVE){
		查找关联设备
		if (usbttyDevFind (pChan, &pusbttyDev) != OK){
			printf ("usbttyDevFind could not find channel 0x%d", (UINT32)pChan);
			return;
			}

		删除设备
		usbttyDevDelete (pusbttyDev);
		sprintf (cp210xName, "%s%d", USB_TTY_NAME, freeusbttyNum(pChan));

		if (usbttySioChanUnlock (pChan) != OK){
			printf("usbttySioChanUnlock () returned ERROR\n");
			return;
		}
		printf ("USB-TTY %s removed\n", cp210xName);
	}*/
}


/*创建USBTTY系统设备，底层驱动xxxxDevCreate进行设备创建的时候，在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS usbttyDevCreate(char *name,SIO_CHAN * pSioChan )
{
	printf("IN usbttyDevCreate\n");
	USB_TTY_NODE 	*pusbttyNode 	= NULL;
	USB_TTY_DEV		*pusbttyDev 	= NULL;
	STATUS 			status 			= ERROR;

	if(pSioChan == (SIO_CHAN *)ERROR){
		printf("pSioChan is ERROR\n");
		return ERROR;
	}

	if((pusbttyDev = (USB_TTY_DEV *)calloc(1,sizeof(USB_TTY_DEV))) == NULL){
		printf("calloc returned NULL - out of memory\n");
		return ERROR;
	}
	pusbttyDev->pSioChan = pSioChan;

//	为节点分配内存并输入数据
	pusbttyNode = (USB_TTY_NODE *) calloc (1, sizeof (USB_TTY_NODE));

//	读取有用信息
	pusbttyNode->pusbttyDev = pusbttyDev;
	pusbttyDev->pusbttyNode = pusbttyNode;

//	将设备添加到系统的设备列表当中
	if((status = iosDevAdd (&pusbttyDev->usbttyDev,name,cp210xDrvNum)) != OK){
		free(pusbttyDev);
//		usbttySioChanLock(pSioChan);
		printf("Unable to create ttyusb device.\n");
		return status;
	}

	printf("iosDevAdd success\n");

//	初始化列表（记录驱动上未解决的任务列表）
	selWakeupListInit(&pusbttyDev->selWakeupList);

//	为列表增加节点

	USB_TTY_LIST_SEM_TAKE (WAIT_FOREVER);
	lstAdd (&usbttyList, (NODE *) pusbttyNode);
	USB_TTY_LIST_SEM_GIVE;

//	以支持方式启动设备
//	sioIoctl (pusbttyDev->pSioChan, SIO_MODE_SET, (void *) SIO_MODE_INT);
	return OK;
}




/*初始化USBTTY驱动*/
STATUS cp210xDrvInit(void)
{
	if(cp210xDrvNum > 0){
		printf("cp210x already initialized.\n");
		return OK;
	}

	cp210xMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);
	cp210xListMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);

	if((cp210xMutex == NULL) || (cp210xListMutex == NULL)){
		printf("Resource Allocation Failure.\n");
		goto ERROR_HANDLER;
	}

	/*初始化链表*/
	lstInit(&usbttyList);

	cp210xDrvNum = iosDrvInstall(NULL,usbttyDevDelete,usbttyDevOpen,usbttyDevClose,\
			usbttyDevRead,usbttyDevWrite,usbttyDevIoctl);


/*检查是否为驱动安装空间*/
	if(cp210xDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		goto ERROR_HANDLER;
	}

	if(cp210xDevInit() == OK){
		printf("cp210xDevInit() returned OK\n");
		if(cp210xDynamicAttachRegister(cp210xAttachCallback,(void *) NULL) != OK){
			 printf ("cp210xDynamicAttachRegister() returned ERROR\n");
//			 usbttyDevShutdown();
			 goto ERROR_HANDLER;
		}
	}else{
		printf("cp210xDevInit() returned ERROR\n");
		goto ERROR_HANDLER;
	}

	int i;
	for(i=0;i< USB_TTY_DEVICE_NUM;++i){
		pusbttyDevArray[i]=NULL;
	}
	return OK;

ERROR_HANDLER:

	if(cp210xDrvNum){
		iosDrvRemove (cp210xDrvNum, 1);
		cp210xDrvNum = 0;
		}
	if (cp210xMutex != NULL)
		semDelete(cp210xMutex);

	if(cp210xListMutex != NULL)
		semDelete(cp210xListMutex);

	cp210xMutex = NULL;
	cp210xListMutex = NULL;
	return ERROR;
}



/*
 * Writes any 16-bits CP210X_register(req) whose value
 * is passed entirely in the wValue field of the USB request
 * */
/*LOCAL int cp210x_write_u16_reg(USB_TTY_DEV *pusbttyDev,UINT8 request,UINT16 value)
{
	status = usb_control_msg(pusbttyDev,request,REQTYPE_HOST_TO_INTERFACE,value,\
			pusbttyDev->interface,NULL,0,USB_TIMEOUT_DEFAULT);
	if(status != OK){
		printf("control message send failed!\n");
	}
	return status;
}*/



/*写USBTTY设备*/
int usbttyDevWrite(USB_TTY_DEV *pusbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
//	status = CreateTransfer(pusbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}

/*打开USBTTY串行设备，在此处对设备进行设置*/
int usbttyDevOpen(USB_TTY_DEV *pusbttyDev, char *name, int flags,int mode)
{
	printf("OK here im in usbttyDevOpen!\n");

//	status = cp210x_write_u16_reg(pusbttyDev,CP210X_IFC_ENABLE,UART_ENABLE);
	if(status != OK){
		printf("set device failed!\n");
		return status;
	}

	return OK;
}


/*关闭USBTTY串行设备*/
int usbttyDevClose(USB_TTY_DEV *pusbttyDev)
{
	if(!(pusbttyDev->numOpened)){
		sioIoctl(pusbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pusbttyDev);
}


int usbttyDevIoctl(USB_TTY_DEV *pusbttyDev,//读取的设备
		int request,
		void *arg)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:		//调用IO控制函数
		status = (sioIoctl(pusbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
int usbttyDevRead(USB_TTY_DEV *pusbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

//	如果字节无效，返回ERROR
	if(nBytes == 0){
		return ERROR;
	}

//	设置缓冲区
	memset(buffer,0,nBytes);
	sioIoctl(pusbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
//		return pusbttyDev->pSioChan->pDrvFuncs->pollInput(&pusbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
//	返回读取的字节数
	return bytesToBeRead;
}

/*为USBTTY删除道系统设备*/
STATUS usbttyDevDelete(USB_TTY_DEV *pusbttyDev)
{
	if(cp210xDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


//	 *终止并释放选择的唤醒列表，调用selWakeUpAll，以防有数据。
//	 *需删除所有在驱动上等待的任务。

	selWakeupAll (&pusbttyDev->selWakeupList, SELREAD);

//	创建信号量
	if ((pusbttyDev->usbttySelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pusbttyDev->selWakeupList) > 0){
//		列表上有未解决的任务，等待信号在终结列表前释放任务
//		等待信号
		semTake (pusbttyDev->usbttySelectSem, WAIT_FOREVER);
	}

//	删除信号量

	semDelete (pusbttyDev->usbttySelectSem);

	selWakeupListTerm(&pusbttyDev->selWakeupList);

//	从I/O系统删除设备
	iosDevDelete(&pusbttyDev->usbttyDev);

//	从列表删除
	USB_TTY_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&usbttyList, (NODE *) pusbttyDev->pusbttyNode);
	USB_TTY_LIST_SEM_GIVE;

//	设备释放内存
	free(pusbttyDev->pusbttyNode);
	free(pusbttyDev);
	return OK;
}



































#ifndef CP210XLIB_H_
#define CP210XLIB_H_

#include <vxWorks.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <sioLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <logLib.h>
#include <selectLib.h>
#include <lstLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbListLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>


#define htole16(x) (x)
#define cpu_to_le16(x) htole16(x)


/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
//0x41 = 0x 0(方向为主机到设备) 10(类型为厂商自定义) 00001(接收方为接口)
#define REQTYPE_INTERFACE_TO_HOST	0xc1  //1100 0001
// 0xc1 = 0x 1(设备到主机) 10(类型为厂商自定义) 00001(接收方为接口???主机的接口??)
#define REQTYPE_HOST_TO_DEVICE	0x40
//0x41 = 0x 0(主机到设备) 10(类型为厂商自定义) 00000(接收方为设备)
#define REQTYPE_DEVICE_TO_HOST	0xc0
//0xc0 = 0x 1(设备到主机) 10(厂商自定义) 00000(接收方为设备)

/* Config request codes */
/*厂商自定义的设备请求，不是标准的设备请求*/
#define CP210X_IFC_ENABLE	0x00
#define CP210X_SET_BAUDDIV	0x01
#define CP210X_GET_BAUDDIV	0x02
#define CP210X_SET_LINE_CTL	0x03
#define CP210X_GET_LINE_CTL	0x04
#define CP210X_SET_BREAK	0x05
#define CP210X_IMM_CHAR		0x06
#define CP210X_SET_MHS		0x07
#define CP210X_GET_MDMSTS	0x08
#define CP210X_SET_XON		0x09
#define CP210X_SET_XOFF		0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR		0x0D
#define CP210X_GET_CHARS	0x0E
#define CP210X_GET_PROPS	0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET		0x11
#define CP210X_PURGE		0x12
#define CP210X_SET_FLOW		0x13
#define CP210X_GET_FLOW		0x14
#define CP210X_EMBED_EVENTS	0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS	0x19
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_SET_BAUDRATE	0x1E
#define CP210X_VENDOR_SPECIFIC	0xFF

/* CP210X_IFC_ENABLE */
#define UART_ENABLE		0x0001
#define UART_DISABLE		0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ	0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK		0X0f00
#define BITS_DATA_5		0X0500
#define BITS_DATA_6		0X0600
#define BITS_DATA_7		0X0700
#define BITS_DATA_8		0X0800
#define BITS_DATA_9		0X0900

#define BITS_PARITY_MASK	0x00f0
#define BITS_PARITY_NONE	0x0000
#define BITS_PARITY_ODD		0x0010
#define BITS_PARITY_EVEN	0x0020
#define BITS_PARITY_MARK	0x0030
#define BITS_PARITY_SPACE	0x0040

#define BITS_STOP_MASK		0x000f
#define BITS_STOP_1		0x0000
#define BITS_STOP_1_5		0x0001
#define BITS_STOP_2		0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON		0x0001
#define BREAK_OFF		0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR		0x0001
#define CONTROL_RTS		0x0002
#define CONTROL_CTS		0x0010
#define CONTROL_DSR		0x0020
#define CONTROL_RING		0x0040
#define CONTROL_DCD		0x0080
#define CONTROL_WRITE_DTR	0x0100
#define CONTROL_WRITE_RTS	0x0200

/* CP210X_VENDOR_SPECIFIC values */
#define CP210X_READ_LATCH	0x00C2
#define CP210X_GET_PARTNUM	0x370B
#define CP210X_GET_PORTCONFIG	0x370C
#define CP210X_GET_DEVICEMODE	0x3711
#define CP210X_WRITE_LATCH	0x37E1

/* Part number definitions */
#define CP210X_PARTNUM_CP2101	0x01
#define CP210X_PARTNUM_CP2102	0x02
#define CP210X_PARTNUM_CP2103	0x03
#define CP210X_PARTNUM_CP2104	0x04
#define CP210X_PARTNUM_CP2105	0x05
#define CP210X_PARTNUM_CP2108	0x08


#define CP210X_IN_BFR_SIZE		64    	//size of input buffer
#define CP210X_OUT_BFR_SIZE		4096	//size of output buffer

/*IRP Time out in millisecs */
#define CP210X_IRP_TIME_OUT 	5000

/* defines */

/* Error Numbers as set the usbEnetLib  */

/* usbEnetLib error values */

/*
 * USB errnos are defined as being part of the USB host Module, as are all
 * vxWorks module numbers, but the USB Module number is further divided into
 * sub-modules.  Each sub-module has upto 255 values for its own error codes
 */

#define CP210X_SUB_MODULE  	14

#define M_cp210xLib 	( (CP210X_SUB_MODULE << 8) | M_usbHostLib )

#define cp210xErr(x)	(M_cp210xLib | (x))

#define S_cp210xLib_NOT_INITIALIZED		cp210xErr (1)
#define S_cp210xLib_BAD_PARAM			cp210xErr (2)
#define S_cp210xLib_OUT_OF_MEMORY		cp210xErr (3)
#define S_cp210xLib_OUT_OF_RESOURCES	cp210xErr (4)
#define S_cp210xLib_GENERAL_FAULT		cp210xErr (5)
#define S_cp210xLib_QUEUE_FULL	    	cp210xErr (6)
#define S_cp210xLib_QUEUE_EMPTY			cp210xErr (7)
#define S_cp210xLib_NOT_IMPLEMENTED		cp210xErr (8)
#define S_cp210xLib_USBD_FAULT	    	cp210xErr (9)
#define S_cp210xLib_NOT_REGISTERED		cp210xErr (10)
#define S_cp210xLib_NOT_LOCKED	    	cp210xErr (11)




#define CP210X_CLASS 				0x10C4		//4292
#define CP210X_SUB_CLASS			0xEA60		//6000
#define CP210X_DRIVER_PROTOCOL	 	0x0100 		//256


//#define CP210X_SWAP_16(x)			((LSB(x) << 8)|MSB(x))


#define CP210X_ATTACH 		0
#define CP210X_REMOVE		1

#define USB_CSW_LENGTH		0x0D	//Length of CSW
#define USB_CBW_MAX_CBLEN	0x10	//Max length of command block



typedef struct cp210x_flow_ctl{
	UINT32 ulControlHandshake;
	UINT32 ulFlowReplace;
	UINT32 ulXonLimit;
	UINT32 ulXoffLimit;
}CP210X_FLOW_CTL;

#define BITS_PER_LONG (__CHAR_BIT__ * __SIZEOF_LONG__)
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define BIT(nr)			(1UL << (nr))
/* cp210x_flow_ctl::ulControlHandshake */
#define CP210X_SERIAL_DTR_MASK		GENMASK(1, 0)
#define CP210X_SERIAL_DTR_SHIFT(_mode)	(_mode)
#define CP210X_SERIAL_CTS_HANDSHAKE	BIT(3)
#define CP210X_SERIAL_DSR_HANDSHAKE	BIT(4)
#define CP210X_SERIAL_DCD_HANDSHAKE	BIT(5)
#define CP210X_SERIAL_DSR_SENSITIVITY	BIT(6)

/* values for cp210x_flow_ctl::ulControlHandshake::CP210X_SERIAL_DTR_MASK */
#define CP210X_SERIAL_DTR_INACTIVE	0
#define CP210X_SERIAL_DTR_ACTIVE	1
#define CP210X_SERIAL_DTR_FLOW_CTL	2

/* cp210x_flow_ctl::ulFlowReplace */
#define CP210X_SERIAL_AUTO_TRANSMIT	BIT(0)
#define CP210X_SERIAL_AUTO_RECEIVE	BIT(1)
#define CP210X_SERIAL_ERROR_CHAR	BIT(2)
#define CP210X_SERIAL_NULL_STRIPPING	BIT(3)
#define CP210X_SERIAL_BREAK_CHAR	BIT(4)
#define CP210X_SERIAL_RTS_MASK		GENMASK(7, 6)
#define CP210X_SERIAL_RTS_SHIFT(_mode)	(_mode << 6)
#define CP210X_SERIAL_XOFF_CONTINUE	BIT(31)

/* values for cp210x_flow_ctl::ulFlowReplace::CP210X_SERIAL_RTS_MASK */
#define CP210X_SERIAL_RTS_INACTIVE	0
#define CP210X_SERIAL_RTS_ACTIVE	1
#define CP210X_SERIAL_RTS_FLOW_CTL	2


/* command block wrapper */

typedef struct usbBulkCbw
    {
    UINT32	signature;              /* CBW Signature */
    UINT32	tag;                    /* Tag field     */
    UINT32	dataXferLength;         /* Size of data (bytes) */
    UINT8	direction;              /* direction bit */
    UINT8	lun;                    /* Logical Unit Number */
    UINT8	length;                 /* Length of command block */
    UINT8	CBD [USB_CBW_MAX_CBLEN];/* buffer for command block */
    } WRS_PACK_ALIGN(1) USB_BULK_CBW, *pUSB_BULK_CBW;


typedef struct usbBulkCsw
    {
    UINT32	signature;              /* CBW Signature */
    UINT32	tag;                    /* Tag field  */
    UINT32	dataResidue;            /* Size of residual data(bytes) */
    UINT8	status;                 /* buffer for command block */
    } WRS_PACK_ALIGN(1) USB_BULK_CSW, *pUSB_BULK_CSW;


/*
 * USB设备结构： 这个结构是用在动态注册回调函数中的主结构
 * 之后当最后的end_obj结构被创建之后，这个结构将被链接
 * 到end_obj结构体。
 * */
typedef struct cp210x_dev{
	LINK 			cp210xDevLink;	/*设备结构体的链表*/
	USBD_NODE_ID	nodeId;			/*设备的NODE ID*/
	UINT16 			configuration;	/*设备的配置*/
	UINT16			interface;		/*设备的接口*/

	UINT16 			vendorId;		/*设备的厂商ID */
	UINT16 			productId;		/*设备的产品ID*/
	UINT16 			lockCount;     	/*设备结构体上锁的次数*/
	BOOL   			connected;		/*是否已连接*/

	VOID			*pCp210xDevice;
}CP210X_DEV,*pCp210xListDev;





typedef struct cp210_sio_chan{
//	END_OBJ endObj;				/*Enet device structure, must be first field */
/*上面的结构体没有，还是和USB键盘中一样的定义一个SIO_CHAN结构体*/
	SIO_CHAN sioChan;			/*serial device struct,must be the first*/

	LINK 			cp210xDevLink;	/*设备结构体的链表*/
	USBD_NODE_ID	nodeId;			/*设备的NODE ID*/
	UINT16 			configuration;	/*设备的配置*/
	UINT16			interface;		/*设备的接口*/

	UINT16 			vendorId;		/*设备的厂商ID */
	UINT16 			productId;		/*设备的产品ID*/
	UINT16 			lockCount;     	/*设备结构体上锁的次数*/
	BOOL   			connected;		/*是否已连接*/

	CP210X_DEV * pDev;			/* the device info */

	UINT16 			interfaceAltSetting;/*设备接口的可变配置*/
	UINT8 communicateOk;	    /* TRUE after Starting and FALSE if stopped */

	int noOfOutIrps;		    /* no of Irps */
	int	 txIrpIndex;		    /* What the last submitted IRP is */

	USBD_PIPE_HANDLE outPipeHandle; /* USBD pipe handle for bulk OUT pipe */
	USB_IRP	outIrp;				/*IRP to monitor output to device*/
	BOOL outIrpInUse;			/*TRUE while IRP is outstanding*/
	char * outBfr;				/* pointer to output buffer */
	UINT16 outBfrLen;		    /* size of output buffer */
	UINT32 outErrors;		    /* count of IRP failures */

	USBD_PIPE_HANDLE inPipeHandle;  /* USBD pipe handle for bulk IN pipe */
	USB_IRP inIrp;				/* IRP to monitor input from printer */
	BOOL inIrpInUse;		    /* TRUE while IRP is outstanding */
	int	noOfInBfrs;	     		/* no of input buffers*/
	char * InBfr;				/* pointer to input buffers */
	UINT16 inBfrLen;		    /* size of input buffer */
	UINT32 inErrors;		    /* count of IRP failures */

	UINT16 	inEpAddr;
	UINT16  outEpAddr;

	USB_IRP 		statusIrp;
	SEM_HANDLE cp210xIrpSem;

	int mode;
	USB_BULK_CBW	bulkCbw;
	USB_BULK_CSW	bulkCsw;

	BOOL txStall;   /* Indicates we ran out of TX IRP's */
	BOOL txActive;  /* Indicates there is a TX IRP in process with the USB Stack */

} CP210X_SIO_CHAN, *pCp210xSioChan;



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

/******************************************************************************
 * CP210X_ATTACH_CALLBACK defines a callback routine which will be
 * invoked by cp210xLib.c when the attachment or removal of a
 * USB-RS232 device is detected.  When the callback is invoked with an attach
 * code of CP210X_ATTACH, the nodeId represents the ID of newly added device.
 * When the attach code is CP210X_REMOVE, nodeId points to the device
 * which is no longer attached.
 */

typedef VOID (* CP210X_ATTACH_CALLBACK)(
	pVOID arg,			/* caller-defined argument */
	CP210X_DEV *pDev,	/* pointer to affected SIO_CHAN */
	UINT16 attachCode	/* defined as USB_TTY_xxxx */
);

/*function prototypes*/
STATUS cp210xDevInit (void);
STATUS cp210xShutdown(int errCode);

STATUS cp210xDynamicAttachRegister(
	CP210X_ATTACH_CALLBACK callback,	/* new callback to be registered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS cp210xDynamicAttachUnRegister(
	CP210X_ATTACH_CALLBACK callback,	/* callback to be unregistered */
	pVOID arg							/* user-defined arg to callback */
);

/*STATUS usbttySioChanLock(
    SIO_CHAN *pChan
);*/

STATUS cp210xDevLock(USBD_NODE_ID nodeId );

STATUS usbttySioChanUnlock(
    SIO_CHAN *pChan		    /* SIO_CHAN to be marked as unused */
);



#endif /* CP210XLIB_H_ */





























#include "cp210xLib.h"

/*typematic definitions*/
#define TYPEMATIC_NAME		 	"tUsbtty"
#define CP210X_CLIENT_NAME 	"cp210xLib"    	/* our USBD client name */

/*globals*/
USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */
USBD_NODE_ID 	cp210xNodeId;			/*our USBD node ID*/

/*locals*/
LOCAL UINT16 		initCount = 0;			/* Count of init nesting */
LOCAL LIST_HEAD 	cp210xDevList;			/* linked list of CP210X_SIO_CHAN */
LOCAL LIST_HEAD 	reqList;				/* Attach callback request list */

LOCAL MUTEX_HANDLE 	cp210xMutex;   			/* mutex used to protect internal structs */
//LOCAL MUTEX_HANDLE	cp210xTxMutex;			/*用来保护内部数据结构 to protect internal structs*/
//LOCAL MUTEX_HANDLE 	cp210xRxMutex;			/*用来保护内部数据结构*/

LOCAL UINT32 		usbCp210xIrpTimeOut = CP210X_IRP_TIME_OUT;


//LOCAL THREAD_HANDLE typematicHandle;		/* task used to generate typematic repeat */
//LOCAL BOOL killTypematic;	/* TRUE when typematic thread should exit */
//LOCAL BOOL typematicExit;	/* TRUE when typematic thread exits */

unsigned int TYPEMATIC_DELAY  = 500;    /* 500 msec delay */
unsigned int TYPEMATIC_PERIOD = 66;     /* 66 msec = approx 15 char/sec */

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




/*channel function table.*/
LOCAL SIO_DRV_FUNCS cp210xSioDrvFuncs ={
		cp210xIoctl,
		cp210xTxStartUp,
		cp210xCallbackInstall,
		cp210xPollInput,
		cp210xPollOutput
};

/***************************************************************************
*
* cp210xDevFind - Searches for a CP210X_SIO_CHAN for indicated node ID
*
* This fucntion searches for the pointer of CP210X_SIO_CHAN for the indicated
* <nodeId>. If the matching pointer is not found it returns NULL.
*
* RETURNS: pointer to matching CP210X_SIO_CHAN or NULL if not found
*
* ERRNO: none
*/
LOCAL CP210X_SIO_CHAN * cp210xFindDev(USBD_NODE_ID nodeId)
{
	printf("IN cp210xFindDev\n");
	CP210X_SIO_CHAN * pSioChan = usbListFirst(&cp210xDevList);

	while(pSioChan != NULL){
		if(pSioChan->nodeId == nodeId)
			break;

		pSioChan = usbListNext(&pSioChan->cp210xDevLink);
	}
	return pSioChan;
}

//========================================================================================//
LOCAL VOID destroyAttachRequest(pATTACH_REQUEST pRequest){
	/*Unlink request*/
	usbListUnlinkProt(&pRequest->reqLink,cp210xMutex);
	/*Dispose request*/
	OSS_FREE(pRequest);
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
	printf("IN notifyAttach\n");

	pATTACH_REQUEST pRequest = usbListFirst(&reqList);

    while (pRequest != NULL){
    	(*pRequest->callback) (pRequest->callbackArg,pSioChan, attachCode);
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

BOOL cp210xOutIrpInUse(CP210X_SIO_CHAN * pSioChan)
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
}


/***************************************************************************
*
* cp210xDestroyDevice - disposes of a CP210X_SIO_CHAN structure
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

LOCAL void cp210xDestroyDevice(CP210X_SIO_CHAN * pSioChan)
{
	printf("IN cp210xDestroyDevice\n");
	int index;

	if (pSioChan != NULL){
		/* Unlink the structure. */
		usbListUnlink (&pSioChan->cp210xDevLink);

		/* Release pipes and wait for IRPs to be cancelled if necessary. */
		if (pSioChan->outPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->outPipeHandle);

		if (pSioChan->inPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pSioChan->inPipeHandle);

		while (cp210xOutIrpInUse(pSioChan) || pSioChan->inIrpInUse)
			OSS_THREAD_SLEEP (1);

		for (index=0; index < pSioChan->noOfInBfrs; index++)
			OSS_FREE (pSioChan->InBfr);

		if (pSioChan->InBfr !=NULL)
			OSS_FREE(pSioChan->InBfr);

		if ( pSioChan->outBfr != NULL)
			OSS_FREE(pSioChan->outBfr);

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
	CP210X_SIO_CHAN *pPhysDev = pIrp->userPtr;

    // check whether the IRP was for bulk out/ bulk in / status transport
    // 检查这个IRP是否是用于BULK OUT/BULK IN的状态报告的。
    if (pIrp == &(pPhysDev->outIrp) ){
    	// check the result of IRP
        if (pIrp->result == OK){
        	printf("cp210xIrpCallback: Num of Bytes transferred on "\
        			"out pipe is:%d\n",pIrp->bfrList[0].actLen);
        }else{
        	printf("cp210xIrpCallback: Irp failed on Bulk Out %x \n",pIrp->result);

            /* Clear HALT Feature on Bulk out Endpoint */
            if ((usbdFeatureClear (cp210xHandle,cp210xNodeId,USB_RT_ENDPOINT,
            		USB_FSEL_DEV_ENDPOINT_HALT,(pPhysDev->outEpAddr & 0xFF)))
				 != OK){
                printf("cp210xIrpCallback: Failed to clear HALT "\
                              "feauture on bulk out Endpoint\n");
            }
        }
    }else if (pIrp == &(pPhysDev->statusIrp))    /* if status block IRP */
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
            		USB_FSEL_DEV_ENDPOINT_HALT,(pPhysDev->inEpAddr & 0xFF)))
            		!= OK){
            	printf ("cp210xIrpCallback: Failed to clear HALT "\
                              "feature on bulk in Endpoint %x\n");
            }
    	}
    }

    OSS_SEM_GIVE (pPhysDev->cp210xIrpSem);
}
//=========================================================================
void cp210x_get_termios_port(CP210X_SIO_CHAN *pSioChan)
{
	/*
	* usbdVendorSpecific允许一个客户发送厂商指定的USB请求，特殊的设备可能需要执行特定的
	* 厂商请求，函数原型为：
	* STATUS usbdVendorSpecific(
	USBD_CLIENT_HANDLE clientHandle,	// Client handle
	USBD_NODE_ID nodeId,		// Node Id of device/hub
	UINT8 requestType,			// bmRequestType in USB spec.
	UINT8 request,			// bRequest in USB spec.
	UINT16 value,			// wValue in USB spec.
	UINT16 index,			// wIndex in USB spec.
	UINT16 length,			// wLength in USB spec.
	pUINT8 pBfr,			// ptr to data buffer
	pUINT16 pActLen			// actual length of IN
	)
	* requestType，request，value，index，length分别对应于USB Specfication中的
	* bmRequestType, bRequest, wValue, wIndex,wLength域。如果length比0要大，
	* 那么pBfr必须是一个指向数据缓冲区的非空指针，这个缓冲区用来接收数据或者
	* 存储要发送的数据(依据于传输的方向)
	*
	* 通过这个函数发送的厂商指定请求会直接发给NodeId指定的设备的控制管道。这个函数根据
	* 参数来形成和发送一个Setup packet，如果提供了一个非空pBfr，那么Additional IN或者
	* OUT传输将会跟随在这和Setup packet之后。传输的方向由requestType参数的方向比特域指定。
	*
	* 对于IN传输，如果pActLen参数非空的话，传入数据的实际长度会存储在pActLen当中。
	*
	* 返回值是OK或者是 ERROR(如果不能执行vendor-specific请求)
	* */



	UINT8 partnum;
	int length = sizeof(partnum);
	UINT16 actLength;

/*
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,UART_ENABLE,
			CP210X_IFC_ENABLE,pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to set CP210X_IFC_ENABLE\n");
	}else{
		printf("createSioChan: success to set CP210X_IFC_ENABLE.\n");
	}
*/

/*
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_DEVICE_TO_HOST,
			CP210X_VENDOR_SPECIFIC,CP210X_GET_PARTNUM,pSioChan->interface,
			length,(pUINT8)&partnum,&actLength)!= OK){
		printf("createSioChan: Failed to send vendor specific \n");
	}else{
//		printf("createSioChan: Success to send vendor specific \n");
//		printf("partnum:%d,len=%d\n",partnum,actLength);
	}*/

	/*获取波特率*/
	UINT32 baud;
	length = sizeof(baud);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baud,&actLength)!= OK){
		printf("createSioChan: Failed to send vendor specific \n");
	}else{
		printf("createSioChan: Success to CP210X_GET_BAUDRATE baudrate is:%d\n",baud);
//		printf("baud:%d,len=%d\n",baud,actLength);
	}



/*	UINT32 baudrate = 57600;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_BAUDRATE,\
			0,pSioChan->interface,sizeof(int),(pUINT8)&baudrate,&actLength) != OK){
		printf("createSioChan: Failed to send CP210X_SET_BAUDRATE \n");
	}else{
		printf("createSioChan: Success to send CP210X_SET_BAUDRATE \n");
	}*/


/*	UINT32 baud;
	length = sizeof(baud);*/

/*	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pSioChan->interface,
			length,(pUINT8)&baud,&actLength)!= OK){
		printf("createSioChan: Failed to  CP210X_GET_BAUDRATE\n");
	}else{
		printf("createSioChan: Success to CP210X_GET_BAUDRATE baud:%d\n",baud);
	}*/




	UINT16 bits;
	length = sizeof(bits);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bits,&actLength)!= OK){
		printf("createSioChan: Failed to CP210X_GET_LINE_CTL \n");
	}else{
		printf("createSioChan: Success to CP210X_GET_LINE_CTL bits:0x%x\n",bits);
	}

//bits = 0x0800  BITS_DATA_MASK = 0x0f00  BITS_DATA_5 = 0x0500
	switch(bits & BITS_DATA_MASK){
	case BITS_DATA_5:
		printf("data bits = 5");
		break;

	case BITS_DATA_6:
		printf("data bits = 6");
		break;

	case BITS_DATA_7:
		printf("data bits = 7");
		break;

	case BITS_DATA_8:
		printf("data bits = 8\n");
		break;

	case BITS_DATA_9:
		printf("data bits = 9");
		break;

	default:
		printf("unknow number of data bits.using 8\n");
	//TODO
//		bits &= ~BITS_DATA_MASK;
		break;
	}


	switch(bits & BITS_PARITY_MASK){
	case BITS_PARITY_NONE:
		printf("patiry = NONE\n");
		break;

	case BITS_PARITY_ODD:
		printf("patiry = ODD\n");
		break;

	case BITS_PARITY_EVEN:
		printf("patiry = EVEN\n");
		break;

	case BITS_PARITY_MASK:
		printf("patiry = MASK\n");
		break;

	case BITS_PARITY_SPACE:
		printf("patiry = SPACE\n");
		break;

	default :
		printf("Unknow parity mode.disabling patity\n");
		//TODO
		break;
	}

	switch (bits & BITS_STOP_MASK){
	case BITS_STOP_1:
		printf("stop bits = 1\n");
		break;

	case BITS_STOP_1_5:
		printf("stop bits = 1.5\n");
		break;

	case BITS_STOP_2:
		printf("stop bits = 2\n");
		break;

	default:
		printf("Unknown number of stop bits,using 1 stop bit\n");
		//TODO
		break;
	}

/*	UINT16 bitsSet;
	bitsSet = bits;

	bitsSet &= ~BITS_DATA_MASK;
	bitsSet |= BITS_DATA_7;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_LINE_CTL,\
			bitsSet,pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to SET_LINE_CONTROL\n");
	}else{
		printf("createSioChan: Success to SET_LINE_CONTROL DATA_BIT\n");
	}*/

//	bitsSet = bits;
//	bitsSet &= ~BITS_PARITY_MASK;
//	bitsSet |= BITS_PARITY_ODD;
/*	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_LINE_CTL,\
			bitsSet,pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to SET_LINE_CONTROL\n");
	}else{
		printf("createSioChan: Success to SET_LINE_CONTROL ODD \n");
	}*/

//	bitsSet = bits;
//	bitsSet &= ~BITS_STOP_MASK;
//	bitsSet |= BITS_STOP_2;
/*
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_LINE_CTL,\
			bitsSet,pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to SET_LINE_CONTROL\n");
	}else{
		printf("createSioChan: Success to SET_LINE_CONTROL STOP_BIT 1 \n");
	}
*/

	/*bitsSet = bits & ~(BITS_DATA_MASK|BITS_STOP_MASK|BITS_PARITY_MASK);
	bitsSet |= BITS_DATA_8|BITS_STOP_2|BITS_PARITY_NONE;   //0x0712;
	printf("createSioChan: bitsSet = 0x%x\n", bitsSet);*/

//	bitsSet = 0x712;

/*	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_LINE_CTL,\
			bitsSet,pSioChan->interface,0,NULL,&actLength) != OK){
		printf("createSioChan: Failed to SET_LINE_CONTROL\n");
	}else{
		printf("createSioChan: Success to SET_LINE_CONTROL 0712 \n");
	}*/





	/*UINT16 bitsNow;
	length = sizeof(bitsNow);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pSioChan->interface,
			length,(pUINT8)&bitsNow,&actLength)!= OK){
		printf("createSioChan: Failed to CP210X_GET_LINE_CTL \n");
	}else{
		printf("createSioChan: Success to CP210X_GET_LINE_CTL bitsNow:0x%x\n",bitsNow);
	}

	switch(bitsNow & BITS_DATA_MASK){
	case BITS_DATA_5:
		printf("data bitsNow = 5");
		break;

	case BITS_DATA_6:
		printf("data bitsNow = 6");
		break;

	case BITS_DATA_7:
		printf("data bitsNow = 7");
		break;

	case BITS_DATA_8:
		printf("data bitsNow = 8\n");
		break;

	case BITS_DATA_9:
		printf("data bitsNow = 9");
		break;

	default:
		printf("unknow number of data bitsNow.using 8\n");
	//TODO
//		bitsNow &= ~BITS_DATA_MASK;
		break;
	}


	switch(bitsNow & BITS_PARITY_MASK){
	case BITS_PARITY_NONE:
		printf("patiry = NONE\n");
		break;

	case BITS_PARITY_ODD:
		printf("patiry = ODD\n");
		break;

	case BITS_PARITY_EVEN:
		printf("patiry = EVEN\n");
		break;

	case BITS_PARITY_MASK:
		printf("patiry = MASK\n");
		break;

	case BITS_PARITY_SPACE:
		printf("patiry = SPACE\n");
		break;

	default :
		printf("Unknow parity mode.disabling patity\n");
		//TODO
		break;
	}

	switch (bitsNow & BITS_STOP_MASK){
	case BITS_STOP_1:
		printf("stop bitsNow = 1\n");
		break;

	case BITS_STOP_1_5:
		printf("stop bitsNow = 1.5\n");
		break;

	case BITS_STOP_2:
		printf("stop bitsNow = 2\n");
		break;

	default:
		printf("Unknown number of stop bitsNow,using 1 stop bit\n");
		//TODO
		break;
	}*/




/*	CP210X_FLOW_CTL flow_ctl;
	length = sizeof(CP210X_FLOW_CTL);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_FLOW,0,pSioChan->interface,
			length,(pUINT8)&flow_ctl,&actLength)!= OK){
		printf("createSioChan: Failed to CP210X_GET_FLOW \n");
	}else{
		printf("createSioChan: Success to CP210X_GET_FLOW \n");
		printf("flow_ctl.ulControlHandshake:%d,flow_ctl.ulFlowReplace:%d,\n"
				"flow_ctl.ulXoffLimit:%d,flow_ctl.ulXonLimit:%d\n",\
				flow_ctl.ulControlHandshake,flow_ctl.ulFlowReplace,flow_ctl.ulXoffLimit,flow_ctl.ulXonLimit);
	}*/


//	UINT32 ctl_hs;
//	UINT32 flow_repl;
//
//	ctl_hs = flow_ctl.ulControlHandshake;
//	flow_repl = flow_ctl.ulFlowReplace;
//	printf("read ulControlHandshake:0x%08x,ulFlowReplace:0x%08x\n");
//	ctl_hs &= ~CP210X_SERIAL_DSR_HANDSHAKE;
//	ctl_hs &= ~CP210X_SERIAL_DCD_HANDSHAKE;
//	ctl_hs &= ~CP210X_SERIAL_DSR_SENSITIVITY;
//	ctl_hs &= ~CP210X_SERIAL_DTR_MASK;
//	ctl_hs &= ~CP210X_SERIAL_DTR_SHIFT(CP210X_SERIAL_DTR_ACTIVE);
//
//	ctl_hs |= CP210X_SERIAL_CTS_HANDSHAKE;
//
//	flow_repl &= ~CP210X_SERIAL_RTS_MASK;
//	flow_repl |= CP210X_SERIAL_RTS_SHIFT(CP210X_SERIAL_RTS_FLOW_CTL);
//	printf("flow control = CRTSCTS");
//	printf("write ulControlHandleShake=0x%08x.ulFlowReplace=0x%08x\n",ctl_hs,flow_repl);
//
//	flow_ctl.ulControlHandshake = ctl_hs;
//	flow_ctl.ulFlowReplace = flow_repl;
//	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,CP210X_SET_FLOW,0,pSioChan->interface,0,NULL,&actLength)!=OK){
//		printf("createSioChan: Failed to send CP210X_SET_FLOW \n");
//	}else{
//		printf("createSioChan: Success to send CP210X_SET_FLOW \n");
//	}





}




/***************************************************************************
*
* configureSioChan - configure USB printer for operation
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

LOCAL STATUS configureSioChan(CP210X_SIO_CHAN *pSioChan){
	USB_CONFIG_DESCR 	* pCfgDescr;//配置描述符指针
	USB_INTERFACE_DESCR * pIfDescr;	//接口描述符指针
	USB_ENDPOINT_DESCR 	* pOutEp;	//OUT ENDPOINT 指针
	USB_ENDPOINT_DESCR 	* pInEp;	//IN ENDPOINT指针
	UINT8 * pBfr;                	//指向描述符存储区域的指针
	UINT8 * pScratchBfr;			// another pointer to the above store
	UINT16 actLen;					//描述符的实际长度
	UINT16 maxPacketSize;			//端点的最大数据包长

    //	为存储描述符分配空间
    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	return ERROR;

    /* Read the configuration descriptor to get the configuration selection
     * value and to determine the device's power requirements.
     * Configuration index is assumed to be one less than config'n value
     */

    if (usbdDescriptorGet (cp210xHandle, pSioChan->nodeId,
    			USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,
    			pSioChan->configuration, 0, USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
    	OSS_FREE (pBfr);
    	return ERROR;
    }

	if ((pCfgDescr = usbDescrParse (pBfr, actLen,
			USB_DESCR_CONFIGURATION)) == NULL){
        OSS_FREE (pBfr);
        return ERROR;
	}

//	printf("pSioChan->configuration is %d\n",pSioChan->configuration);
//	printf("pCfgDescr->configurationValue is:%d\n",pCfgDescr->configurationValue);
	pSioChan->configuration = pCfgDescr->configurationValue;



	//	 * 由于使用了NOTIFY_ALL来注册设备，注册返回函数的配置和接口数量没有
	//	 * 任何意义，
	//	 * 	参考芯片的文档发现它只提供了一个接口0.所以第一个接口就
	//	 * 	是我们需要的接口

	//	 *usbdDescrParseSkip()修改它所收到的指针的值，所以我们将其
	//	 *保存一份。

    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */


	UINT16 ifNo = 0;

	pScratchBfr = pBfr;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,
			USB_DESCR_INTERFACE))!= NULL){
		if (ifNo == pSioChan->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pSioChan->interface = pIfDescr->interfaceNumber;

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
		printf("usbdConfigurationSet() return ERROR\n");
        OSS_FREE (pBfr);
        return ERROR;
    }
//	printf("usbdConfigurationSet() return OK\n");

    /* Select interface
     *
     * NOTE: Some devices may reject this command, and this does not represent
     * a fatal error.  Therefore, we ignore the return status.
     */

	//	pSioChan->interfaceAltSetting = pIfDescr->alternateSetting;
  /*  if(usbdInterfaceSet(cp210xHandle,cp210xNodeId,pSioChan->interface,\
			pIfDescr->alternateSetting) != OK){
    	printf("configureSioChan: usbdInterfaceSet return ERROR,we just ignore it.\n");
	}
	printf("configureSioChan: usbdInterfaceSet return OK.\n");
*/



	cp210x_get_termios_port(pSioChan);


    /* Create a pipe for output to the printer. */
	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) |
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,
			0,0,&pSioChan->outPipeHandle)!= OK){
		printf("out pipe create failed\n");
		OSS_FREE (pBfr);
		return ERROR;
	}
//	printf("create out pipe OK\n");


	maxPacketSize = *((pUINT8)&pInEp->maxPacketSize) |
			(*(((pUINT8)&pInEp->maxPacketSize)+1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pInEp->endpointAddress,\
			pCfgDescr->configurationValue,pSioChan->interface,\
			USB_XFRTYPE_BULK,USB_DIR_IN,maxPacketSize,
			0,0,&pSioChan->inPipeHandle)!= OK){
		printf("in pipe create failed\n");
		OSS_FREE (pBfr);
		return ERROR;
	}
//	printf("create in pipe OK\n");

#if 1
	/*usb spec中规定，对于设备的bulk端点，每当设备在reset之后，需要清除halt这个feature，然后端点
	 * 才能够正常的工作，所以在reset相关的函数当中都会调用需要调用清楚halt的函数。
	 *
	 * USB spec中规定对于使用了data toggle的endpoint，不管其halt feature是否被设置，只要调用了clear
	 * feature，那么其data toggle总是会被初始化为DATA0
	 *
	 * 中断端点和Bulk端点有一个HALT特征，其实质是寄存器中的某一个比特位，若该比特位
	 * 设置为1，就表示设置了HALT特征，那么这个端点就不能正常工作，要使这个端点正常工作
	 * 则应该清除这个bit位*/
	/* Clear HALT feauture on the endpoints */
	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT, (pOutEp->endpointAddress & 0xFF)))
			!= OK){
		printf("configureSioChan: Failed to clear HALT feauture "\
					  "on bulk out Endpoint \n");
	}
	printf("configureSioChan: Success to clear HALT feauture "\
						  "on bulk out Endpoint \n");

	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,
			USB_FSEL_DEV_ENDPOINT_HALT,(pInEp->endpointAddress & 0xFF)))
			!= OK){
		printf("configureSioChan: Failed to clear HALT feauture "\
					  "on bulk in Endpoint \n");
	}
	printf("configureSioChan: Success to clear HALT feauture "\
						  "on bulk in Endpoint \n");
#endif



	//cp210x_get_termios_port(pSioChan);



	char a[13]="hello world!";
    /* Initialize IRP */

    pSioChan->outIrp.transferLen = sizeof (a);
    pSioChan->outIrp.result = -1;
//    pSioChan->outIrp.dataToggle = USB_DATA0;

    pSioChan->outIrp.bfrCount = 1;
    pSioChan->outIrp.bfrList [0].pid = USB_PID_OUT;
    pSioChan->outIrp.bfrList [0].pBfr = (pUINT8)a;
    pSioChan->outIrp.bfrList [0].bfrLen = sizeof (a);

    /* Submit IRP */
    if (usbdTransfer (cp210xHandle, pSioChan->outPipeHandle, &pSioChan->outIrp) != OK){
    	printf("usbdtransfer returned false\n");
//    	return FALSE;
    }
    printf("result:%d",pSioChan->outIrp.result);




	/*if (listenForInput (pSioChan) != OK)
            {
            OSS_FREE (pBfr);
            return ERROR;
            }*/
//	pSioChan->inIrpInUse = FALSE;
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
LOCAL CP210X_SIO_CHAN * createSioChan(CP210X_SIO_CHAN *pSioChan)
{
	printf("IN createSioChan\n");

//	 Try to allocate space for a new keyboard struct
	if ((pSioChan->outBfr = OSS_CALLOC(CP210X_OUT_BFR_SIZE))== NULL ){
//		destorySioChan(pSioChan);
		return NULL;
	}

	if((pSioChan->InBfr = OSS_CALLOC(CP210X_IN_BFR_SIZE)) == NULL){
//		destorySioChan(pSioChan);
		return NULL;
	}

	pSioChan->txIrpIndex = 0;
	pSioChan->txStall = FALSE;
	pSioChan->txActive = FALSE;

	pSioChan->outBfrLen = CP210X_OUT_BFR_SIZE;
	pSioChan->inBfrLen = CP210X_IN_BFR_SIZE;

	pSioChan->sioChan.pDrvFuncs = &cp210xSioDrvFuncs;
	pSioChan->mode = SIO_MODE_POLL;

	//初始化Device的OutIrp
	pSioChan->outIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->outIrp.userCallback		= cp210xIrpCallback;
	pSioChan->outIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->outIrp.bfrCount          = 0x01;
	pSioChan->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pSioChan->outIrp.userPtr           = pSioChan;

	//初始化Device的InIrp
	pSioChan->inIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->inIrp.userCallback		= cp210xIrpCallback;
	pSioChan->inIrp.timeout            = usbCp210xIrpTimeOut;
	pSioChan->inIrp.bfrCount          = 0x01;
	pSioChan->inIrp.bfrList[0].pid    = USB_PID_IN;
	pSioChan->inIrp.userPtr           = pSioChan;

	//初始化Device的StatusIrp
	pSioChan->statusIrp.irpLen				= sizeof (USB_IRP);
	pSioChan->statusIrp.userCallback		= cp210xIrpCallback;
	pSioChan->statusIrp.timeout          	= usbCp210xIrpTimeOut;
	pSioChan->statusIrp.transferLen      	= USB_CSW_LENGTH;  //USB_CSW_LENGTH 是什么用途？Length of CSW
	pSioChan->statusIrp.bfrCount         	= 0x01;
	pSioChan->statusIrp.bfrList[0].pid    	= USB_PID_IN;
	pSioChan->statusIrp.bfrList[0].pBfr    = (UINT8 *)(&pSioChan->bulkCsw);
	pSioChan->statusIrp.bfrList[0].bfrLen 	= USB_CSW_LENGTH;
	pSioChan->statusIrp.userPtr           	= pSioChan;

	if (OSS_SEM_CREATE( 1, 1, &pSioChan->cp210xIrpSem) != OK){
		return NULL;
//        return(cp210xShutdown(S_cp210xLib_OUT_OF_RESOURCES));
	}

	if(configureSioChan(pSioChan) != OK){
//		destorySioChan(pSioChan);
		return NULL;
	}

	return pSioChan;
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
	);
每次attachCallback被调用的时候，USBD会传递该设备的nodeid和attachAction给这个回调函数。
当attachAction是USBD_DYNA_REMOVE的时候，表明指向的nodeid不再有效，此时client应该查询这个nodeid的
永久数据结构并且删除任何和这个nodeid相关联的结构，如果client有明显的对这个nodeid的请求，
例如：数据传输请求，那么USBD就会在调用attachCallback通知client这个设备被移除之前设置这些显式的请求为无效。
通常与一个被移除的设备相关的数据传输请求在attachCallback被调用之前就应该被小心的对待。
	client可能会为多个通知登记重复使用一个attachCallback，为了方便起见，
USBD也会在每个设备attach/remove的时候传递该设备的class/subclass/protocol给attachCallback。
最后需要注意的是不是所有的设备都会在设备级来报告class信息，一些设备在interface-to-interface
的基础上报告class信息。当设备在设备级报告class信息时，USBD会在attachCallback中传递configuration的值为0，
并且每一个设备只会调用一次attachCallback。当设备在interface级报告class信息时，USBD会为每一个接口唤醒一次
attachCallback，只要这个接口适配client的class/subclass/protocol，
在这种情况下，USBD也会在每一次调用attachCallback时传递对应的configuration和interface的值
*/

LOCAL STATUS cp210xLibAttachCallback( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,UINT16 interface,
		UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
	printf("IN cp210xLibAttachCallback\n");

	CP210X_SIO_CHAN *pSioChan;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int noOfSupportedDevices =(sizeof(cp210xAdapterList)/(2*sizeof(UINT16))) ;
    printf("noOfSupportedDevices is:%d\n",noOfSupportedDevices);
    int index = 0;

    if((pBfr = OSS_MALLOC(USB_MAX_DESCR_LEN)) == NULL)
    	return ERROR;

    OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	switch(attachAction){
	case USBD_DYNA_ATTACH:
		/* 发现新设备
		 *  Check if we already  have a structure for this device.*/
		if(cp210xFindDev(nodeId)!= NULL){
			printf("device already exists\n");
			break;
		}

		/*确保这是一个cp210x设备*/
	      if (usbdDescriptorGet (cp210xHandle, nodeId, USB_RT_STANDARD | USB_RT_DEVICE,
	    		  USB_DESCR_DEVICE, 0, 0, 36, pBfr, &actLen) != OK){
	    	  printf("usbdDescriptorGet() function failed!\n");
	    	  break;
	      }

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
	   		/* device not supported，不支持该设备 */
	    	  printf( " Unsupported device found vId %0x; pId %0x!!! \n",\
	    			  vendorId, productId);
	    	  break;
	   		}else{
	   			printf("Found device vId %0x; pId %0x!!!\n",vendorId,productId);
	   		}


	      //为新设备创建结构体，并将结构体加入设备列表
	      if((pSioChan = OSS_CALLOC(sizeof(CP210X_SIO_CHAN))) == NULL ){
	    	  break;
	      }
	      pSioChan->nodeId = nodeId;
	      pSioChan->configuration = configuration;
	      pSioChan->interface = interface;
	      pSioChan->vendorId = vendorId;
	      pSioChan->productId = productId;

	      /*保存一个nodeId的全局变量,*/
	      cp210xNodeId = nodeId;

	      /* Create a new structure to manage this device.  If there's an error,
		   * there's nothing we can do about it, so skip the device and return immediately.
		   */
		  if((createSioChan(pSioChan)) == NULL)
			  break;

	      /*将设备加入设备链表尾部*/
	      usbListLink (&cp210xDevList, pSioChan, &pSioChan->cp210xDevLink,LINK_TAIL);

	      /*将设备标记为已连接*/
	      pSioChan->connected = TRUE;

	      /* Notify registered callers that a new cp210x has been added
	       * and a new channel created.*/
	      notifyAttach (pSioChan, CP210X_ATTACH);
	      break;



	case USBD_DYNA_REMOVE:
		 /* A device is being detached.
		  * Check if we have any structures to manage this device.*/
		if ((pSioChan = cp210xFindDev (nodeId)) == NULL){
			printf("Device not found\n");
			break;
		}

		/* The device has been disconnected. */
		if(pSioChan->connected == FALSE){
			printf("device not connected\n");
			break;
		}

		pSioChan->connected = FALSE;

		/* Notify registered callers that the keyboard has been removed and the channel disabled.
		 * NOTE: We temporarily increment the channel's lock count to prevent usbttySioChanUnlock() from destroying the
		 * structure while we're still using it.*/
		pSioChan->lockCount++;
		notifyAttach (pSioChan, CP210X_REMOVE);

		pSioChan->lockCount--;

		/* If no callers have the channel structure locked, destroy it now.
		 * If it is locked, it will be destroyed later during a call to usbttyUnlock().*/
		if (pSioChan->lockCount == 0)
			cp210xDestroyDevice (pSioChan);

		break;
	}

	OSS_FREE(pBfr);
	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}

/***************************************************************************
*
* cp210xShutdown - shuts down USB EnetLib
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

STATUS cp210xShutdown(int errCode)
{
	printf("IN cp210xShutdown\n");
    CP210X_SIO_CHAN * pSioChan;

    /* Dispose of any open connections. */
    while ((pSioChan = usbListFirst (&cp210xDevList)) != NULL)
	cp210xDestroyDevice (pSioChan);

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

//    if (cp210xTxMutex != NULL){
//    	OSS_MUTEX_DESTROY (cp210xTxMutex);
//    	cp210xTxMutex = NULL;
//	}
//
//    if (cp210xRxMutex != NULL){
//    	OSS_MUTEX_DESTROY (cp210xRxMutex);
//    	cp210xRxMutex = NULL;
//	}

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
	printf("IN cp210xDevInit\n");
	if(initCount == 0){
		memset(&cp210xDevList,0,sizeof(cp210xDevList));
		memset(&reqList,0,sizeof(reqList));

		cp210xMutex		 	= NULL;
		cp210xHandle		= NULL;

		if (OSS_MUTEX_CREATE (&cp210xMutex) != OK)
			return cp210xShutdown (S_cp210xLib_OUT_OF_RESOURCES);

/*		if (usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK || usbdDynamicAttachRegister (cp210xHandle,
			CP210X_CLASS,CP210X_SUB_CLASS,CP210X_DRIVER_PROTOCOL,TRUE,cp210xLibAttachCallback)!= OK)*/
		if (usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK ||
				usbdDynamicAttachRegister (cp210xHandle,CP210X_CLASS,CP210X_SUB_CLASS,CP210X_DRIVER_PROTOCOL,TRUE,
						(USBD_ATTACH_CALLBACK)cp210xLibAttachCallback)!= OK){
			printf("usbd register failed!\n");
			return cp210xShutdown (S_cp210xLib_USBD_FAULT);
		}

		printf("usbdClientRegister() returned OK,usbdDynamicAttachRegister() returned OK\n");
	}
	initCount++;
	return OK;
}

/***************************************************************************
*
* cp210xDynamicAttachRegister - register USB-RS232 device attach callback
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

	/* Create a new request structure to track this callback request. */
	if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL){
		status = ERROR;
	}else{
			pRequest->callback = callback;
			pRequest->callbackArg = arg;

			usbListLinkProt (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL,cp210xMutex);

			/*
			 * Perform an initial notification of all
			 * currrently attached devices.
			 * */
			pSioChan = usbListFirst (&cp210xDevList);
			while (pSioChan != NULL){
				if (pSioChan->connected)
					(*callback) (arg,pSioChan,CP210X_ATTACH);

				pSioChan = usbListNext (&pSioChan->cp210xDevLink);
			}
		}

	OSS_MUTEX_RELEASE (cp210xMutex);
	return ossStatus (status);
}

/***************************************************************************
*
* cp210xDynamicAttachUnregister - unregisters USB-RS232 attach callbackx
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

STATUS usbttyDynamicAttachUnRegister(
		CP210X_ATTACH_CALLBACK callback,	/* callback to be unregistered */
		pVOID arg		    				/* user-defined arg to callback */
){
	printf("IN usbttyDynamicAttachUnRegister\n");

    pATTACH_REQUEST pRequest;
//    CP210X_SIO_CHAN * pSioChan = NULL;
    int status = S_cp210xLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL){
    	if (callback == pRequest->callback && arg == pRequest->callbackArg){
    		/* Perform a notification of all currrently attached devices.*/

    		/*发现一个匹配的请求*/
    		usbListUnlink(&pRequest->reqLink);

			/*删除结构体*/
			OSS_FREE(pRequest);
			status = OK;
			/* We found a matching notification request. */
	/*    	destroyAttachRequest (pRequest);
			status = OK;*/
			break;
    	}
    	pRequest = usbListNext (&pRequest->reqLink);
    }
    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}

/***************************************************************************
*
* cp210xDevLock - marks CP210X_SIO_CHAN structure as in use
*
* A caller uses cp210xDevLock() to notify cp210xDevLib that
* it is using the indicated USB-RS232 device structure.  cp210xDevLib maintains
* a count of callers using a particular Cp210x Device structure so that it
* knows when it is safe to dispose of a structure when the underlying
* Cp210x Device is removed from the system.  So long as the "lock count"
* is greater than zero, cp210xDevLib will not dispose of an Cp210x
* structure.
*
* RETURNS: OK, or ERROR if unable to mark Cp210x structure in use.
*
* ERRNO: none
*/

STATUS cp210xDevLock(USBD_NODE_ID nodeId ){
	printf("IN cp210xDevLock\n");
   /* CP210X_SIO_CHAN * pCp210xDev = cp210xFindDevice (nodeId);

    if ( pCp210xDev == NULL)
    	return (ERROR);

    pCp210xDev->lockCount++;*/
    return (OK);
}


/***************************************************************************
*
* cp210xDevUnlock - marks USB_PEGASUS_DEV structure as unused
*
* This function releases a lock placed on an Pegasus Device structure.  When a
* caller no longer needs an Pegasus Device structure for which it has previously
* called cp210xDevLock(), then it should call this function to
* release the lock.
*
* NOTE
* If the underlying Pegasus device has already been removed
* from the system, then this function will automatically dispose of the
* Pegasus Device structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the Pegasus Device structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark Pegasus Device structure unused.
*
* ERRNO:
* \is
* \i S_cp210xLib_NOT_LOCKED
* No lock to Unlock
* \ie
*/
STATUS cp210xDevUnlock(USBD_NODE_ID nodeId)
{
	printf("IN cp210xDevUnlock,just return OK\n");
    int status = OK;
   /* CP210X_SIO_CHAN * pSioChan = cp210xFindDevice (nodeId);

    if(pSioChan == NULL){
    	return ERROR;
    }

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0){
    	status = S_cp210xLib_NOT_LOCKED;
    }else{
//	 If this is the last lock and the underlying USB-RS232 device
//	 is no longer connected, then dispose of the keyboard.
    	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
    		cp210xDestroyDevice ((CP210X_SIO_CHAN *)pSioChan->pCp210xDevice);
    	printf("USB to RS232 device destoryed in DevUnlock!\n");
    }
    OSS_MUTEX_RELEASE (cp210xMutex);*/
    return (ossStatus(status));
}
//===============================================================================================//
/* start a output transport
 * */
/*LOCAL int cp210xTxStartUp(SIO_CHAN *pChan)
{
	printf("IN cp210xTxStartUp\n");
//TODO
	return EIO;
}*/



//===============================================================================================//
/*cp210xCallbackInstall - install ISR callbacks to get/put chars
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.
 * */

LOCAL int cp210xCallbackInstall(SIO_CHAN *pChan,int callbackType,
		STATUS(*callback)(void *tmp,...),void *callbackArg)
{
	printf("IN cp210xCallbackInstall now\n");
				return OK;
/*	CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *)pChan;
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
	}*/
}


//===============================================================================================//
/*
 * output a character in polled mode.
 * */
LOCAL int cp210xPollOutput(SIO_CHAN *pChan,char outChar)
{
	printf("IN cp210xPollOutput");
	//TODO
	return OK;
}


//===============================================================================================//
/*
 * poll the device for input
 * */
LOCAL int cp210xPollInput(SIO_CHAN *pChan,char *thisChar)
{
	printf("IN cp210xPollInput\n");
	return OK;
	/*CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *)pChan;
	int status = OK;

	if(thisChar == NULL)
		return EIO;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);
//	 Check if the input queue is empty.

	if (pSioChan->inQueueCount == 0)
		status = EAGAIN;
	else{
//	 Return a character from the input queue.
		*thisChar = nextInChar (pSioChan);
	}

//TODO

	OSS_MUTEX_RELEASE (cp210xMutex);
	return status;*/
}

//===============================================================================================//
/*
 * cp210xIoctl - special device control
 * this routine is largely a no-op for the function.c the only ioctls
 * which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
 *
 *
 * */
LOCAL int cp210xIoctl(SIO_CHAN *pChan,	     //device to control
		int request,	 //request code
		void *someArg	 //some argument
		){
	printf("IN cp210xIoctl now\n");
	return OK;
/*	CP210X_SIO_CHAN * pSioChan = (CP210X_SIO_CHAN *) pChan;
	int arg = (int) someArg;

	switch (request){

	case SIO_BAUD_SET:
//		 baud rate has no meaning for USB.  We store the desired
//		 * baud rate value and return OK.

		pSioChan->baudRate = arg;
		return OK;


	case SIO_BAUD_GET:

//		 Return baud rate to caller
		*((int *) arg) = pSioChan->baudRate;
		return OK;

	case SIO_MODE_SET:

//		 Set driver operating mode: interrupt or polled
		if (arg != SIO_MODE_POLL && arg != SIO_MODE_INT)
		return EIO;

		pSioChan->mode = arg;
		return OK;


	case SIO_MODE_GET:

//		 Return current driver operating mode for channel

		*((int *) arg) = pSioChan->mode;
		return OK;


	case SIO_AVAIL_MODES_GET:

//		 Return modes supported by driver.

		*((int *) arg) = SIO_MODE_INT | SIO_MODE_POLL;
		return OK;


	case SIO_OPEN:

//		 Channel is always open.
		return OK;

//TODO

	case SIO_HW_OPTS_SET:    //optional, not supported
	case SIO_HW_OPTS_GET:    //optional, not supported
	case SIO_HUP:	 //hang up is not supported
	default:	     //unknown/unsupported command.
		return ENOSYS;
	}*/
}


//=============================2017.06.09 10:30 END    A totally new part==========================//




































//=============================2017.06.09 10:30 BEGIN    A totally new part==========================//
/*ttyusb.h*/
#ifndef __USBTTY__H
#define __USBTTY_H
#include <vxWorks.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <sioLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <logLib.h>
#include <selectLib.h>
#include <lstLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbListLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>

#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 		//256

#define USB_TTY_ATTACH 		0
#define USB_TTY_REMOVE		1

typedef VOID (*USB_TTY_ATTACH_CALLBACK)(
	pVOID arg,			/* caller-defined argument */
	SIO_CHAN *pChan,	/* pointer to affected SIO_CHAN */
	UINT16 attachCode	/* defined as USB_TTY_xxxx */
);

/*function prototypes*/
STATUS usbttyDevInit (void);
STATUS usbttyDevShutDown(void);

STATUS usbttyDynamicAttachRegister(
	USB_TTY_ATTACH_CALLBACK callback,	/* new callback to be registered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS usbttyDynamicAttachUnRegister(
	USB_TTY_ATTACH_CALLBACK callback,	/* callback to be unregistered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS usbttySioChanLock(
    SIO_CHAN *pChan		    /* SIO_CHAN to be marked as in use */
);

STATUS usbttySioChanUnlock(
    SIO_CHAN *pChan		    /* SIO_CHAN to be marked as unused */
);


#endif




/*cp210x.c*/
#include "ttyusb.h"

/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
//0x41 = 0x 0(方向为主机到设备) 10(类型为厂商自定义) 00001(接收方为接口)
#define REQTYPE_INTERFACE_TO_HOST	0xc1
// 0xc1 = 0x 1(设备到主机) 10(类型为厂商自定义) 00001(接收方为接口???主机的接口??)
#define REQTYPE_HOST_TO_DEVICE	0x40
//0x41 = 0x 0(主机到设备) 10(类型为厂商自定义) 00000(接收方为设备)
#define REQTYPE_DEVICE_TO_HOST	0xc0
//0xc0 = 0x 1(设备到主机) 10(厂商自定义) 00000(接收方为设备)

/* Config request codes */
/*厂商自定义的设备请求，不是标准的设备请求*/
#define CP210X_IFC_ENABLE	0x00
#define CP210X_SET_BAUDDIV	0x01
#define CP210X_GET_BAUDDIV	0x02
#define CP210X_SET_LINE_CTL	0x03
#define CP210X_GET_LINE_CTL	0x04
#define CP210X_SET_BREAK	0x05
#define CP210X_IMM_CHAR		0x06
#define CP210X_SET_MHS		0x07
#define CP210X_GET_MDMSTS	0x08
#define CP210X_SET_XON		0x09
#define CP210X_SET_XOFF		0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR		0x0D
#define CP210X_GET_CHARS	0x0E
#define CP210X_GET_PROPS	0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET		0x11
#define CP210X_PURGE		0x12
#define CP210X_SET_FLOW		0x13
#define CP210X_GET_FLOW		0x14
#define CP210X_EMBED_EVENTS	0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS	0x19
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_SET_BAUDRATE	0x1E
#define CP210X_VENDOR_SPECIFIC	0xFF

/* CP210X_IFC_ENABLE */
#define UART_ENABLE		0x0001
#define UART_DISABLE		0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ	0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK		0X0f00
#define BITS_DATA_5		0X0500
#define BITS_DATA_6		0X0600
#define BITS_DATA_7		0X0700
#define BITS_DATA_8		0X0800
#define BITS_DATA_9		0X0900

#define BITS_PARITY_MASK	0x00f0
#define BITS_PARITY_NONE	0x0000
#define BITS_PARITY_ODD		0x0010
#define BITS_PARITY_EVEN	0x0020
#define BITS_PARITY_MARK	0x0030
#define BITS_PARITY_SPACE	0x0040

#define BITS_STOP_MASK		0x000f
#define BITS_STOP_1		0x0000
#define BITS_STOP_1_5		0x0001
#define BITS_STOP_2		0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON		0x0001
#define BREAK_OFF		0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR		0x0001
#define CONTROL_RTS		0x0002
#define CONTROL_CTS		0x0010
#define CONTROL_DSR		0x0020
#define CONTROL_RING		0x0040
#define CONTROL_DCD		0x0080
#define CONTROL_WRITE_DTR	0x0100
#define CONTROL_WRITE_RTS	0x0200

/* CP210X_VENDOR_SPECIFIC values */
#define CP210X_READ_LATCH	0x00C2
#define CP210X_GET_PARTNUM	0x370B
#define CP210X_GET_PORTCONFIG	0x370C
#define CP210X_GET_DEVICEMODE	0x3711
#define CP210X_WRITE_LATCH	0x37E1

/* Part number definitions */
#define CP210X_PARTNUM_CP2101	0x01
#define CP210X_PARTNUM_CP2102	0x02
#define CP210X_PARTNUM_CP2103	0x03
#define CP210X_PARTNUM_CP2104	0x04
#define CP210X_PARTNUM_CP2105	0x05
#define CP210X_PARTNUM_CP2108	0x08



typedef struct usb_tty_node{
	NODE 	node;
	struct usb_tty_dev *pusbttyDev;
}USB_TTY_NODE;

typedef struct usb_tty_dev
{
	/*I/O子系统在调用底层驱动函数时，将使用设备结构作为函数调用的第一个参数，这个设备结构是底层驱动自定义的，
	 * 用以保存底层硬件设备的关键参数。对于自定义的设备结构必须将内核结构DEV_HDR作为自定义设备结构的第一个参数。
	 * 该结构被I/O子系统使用，统一表示系统内的所有设备，即I/O子系统将所有的驱动自定义的设备结构作为DEV_HDR使用，
	 * DEV_HDR之后的字段由各自具体的驱动进行解释和使用，内核并不关心这些字段的含义。*/
	DEV_HDR 		usbttyDev;
	SIO_CHAN 		*pSioChan;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	USB_TTY_NODE *          pusbttyNode;
	SEL_WAKEUP_LIST         selWakeupList;   /*为解决任务列表*/
	SEM_ID                  usbttySelectSem;

	USBD_CLIENT_HANDLE usb_ttyusbdClientHandle;
	USBD_NODE_ID	usbttyNodeid;
	UINT16 			numOpened;
	UINT16 			configuration;			/*configuration number(index)*/
	UINT16 			configurationValue;		/*Configuration value*/
	UINT16 			interface; 				/*interface */
	UINT16 			interfaceNumber;         /*interface number*/
	UINT16			altSetting;				/*Alternate setting of interface*/
	UINT16 			outEndpointAddress;		/*Bulk out EP address*/
	UINT16 			inEndpointAddress;		/*BUlk in EP address*/
	UINT16          epMaxPacketSize;		/**/
	UINT8   		maxPower;
	USBD_PIPE_HANDLE outPipeHandle;			/*pipe handle for Bulk out EP*/
	USBD_PIPE_HANDLE inPipeHandle;			/*pipe handle for Bulk in EP*/
	USBD_PIPE_HANDLE ctlPipeHandle;			/*pipe handle for control EP*/
	USB_IRP			inIrp;					/*IRP used for in data*/
	USB_IRP 		outIrp;					/*IRP used for out data*/
	USB_IRP 		statusIrp; 				/*IRP used for reading status*/
	UINT8 			*usbInData;				/*Pointer for USB-in data*/
	UINT8 			*usbOutData;			/*pointer for USB-out data*/
	/*USB-in-data用于指向存储被读取数据的缓冲区，*/
//TODO
}USB_TTY_DEV;




//#define     DESC_PRINT
#define 	__DEBUG__

#ifdef 		__DEBUG__
	#include <D:\LambdaPRO615\target\deltaos\include\debug\utils\stdarg.h>
	void DEBUG(const char *fmt,...)
	{
		va_list ap;
//		va_list (ap,fmt);
		vprintf(fmt,ap);
		va_end(ap);
	}
#else
	void DEBUG(const char *fmt,...){}
#endif

#define USB_TTY_DEVICE_NUM			5
#define USB_TTY_NAME 				"/usbtty/"
#define USB_TTY_NAME_LEN_MAX 		50

STATUS status;
LOCAL SIO_CHAN 				*pusbttyDevArray[USB_TTY_DEVICE_NUM];
USB_TTY_DEV 				*pusbttyDev = NULL;
LOCAL int 					usbttyDrvNum = -1; 	//驱动注册的驱动号
LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle = NULL;
LOCAL LIST					usbttyList;			//系统所有的USBTTY设备
LOCAL SEM_ID 				usbttyListMutex;  	//保护列表互斥信号量
LOCAL SEM_ID				usbttyMutex;			//互斥信号量





#define USB_TTY_LIST_SEM_TAKE(tmout)	semTake(usbttyListMutex,(int)(tmout))
#define	USB_TTY_LIST_SEM_GIVE			semGive(usbttyListMutex)


int usb_control_msg(USB_TTY_DEV *pusbttyDev,UINT8 request,\
		 UINT8 requestType,UINT16 value,UINT16 index,void *data,UINT16 length,UINT32 timeout);
STATUS getAllConfiguration(void);

 int usbttyDevRead(USB_TTY_DEV *pusbttyDev,  char *buffer,  UINT32 nBytes);
int usbttyDevWrite(USB_TTY_DEV *pusbttyDev, char *buffer,UINT32 nBytes);
int usbttyDevOpen(USB_TTY_DEV *pusbttyDev,  char *name,  int flags,int mode);
int usbttyDevClose(USB_TTY_DEV *pusbttyDev);
int usbttyDevIoctl(USB_TTY_DEV *pusbttyDev,  int request,  void *arg);
STATUS usbttyDevDelete(USB_TTY_DEV *pusbttyDev);
STATUS cp201x_set_config(unsigned char request,unsigned int *data,int size);
LOCAL STATUS usbttyDevCreate(char *,SIO_CHAN *);

//==============================================================================//
LOCAL int getusbttyNum(SIO_CHAN *pChan)
{
	int index;
	for(index =0;index<USB_TTY_DEVICE_NUM;index++){
		if(pusbttyDevArray[index] == 0){
			pusbttyDevArray[index] = pChan;
			return (index);
		}
	}
	printf("Out of avilable USB_TTY number.Check USB_TTY_DEVICE＿NUM\n");
	return (-1);
}

//==============================================================================//
LOCAL int freeusbttyNum(SIO_CHAN *pChan)
{
	int index;
	for (index=0; index < USB_TTY_DEVICE_NUM; index++)
	{
		if (pusbttyDevArray[index] == pChan){
			pusbttyDevArray[index] = NULL;
			return (index);
		}
	}
	printf("Unable to locate USB_TTY pointed to by channel 0x%X.\n",pChan);
	return(-1);
}

//==============================================================================//
/*为SIO通道查找USB_TTY_DEV设备*/
LOCAL STATUS usbttyDevFind(SIO_CHAN *pChan,USB_TTY_DEV **ppusbttyDev)
{
	USB_TTY_NODE 	*pusbttyNode = NULL ;	/*指向USB设备节点*/
	USB_TTY_DEV		*pTempDev;				/*指向USB_TTY_DEV*/

	if(pChan == NULL)
		return ERROR;

	/*防止其他模块干扰*/
	USB_TTY_LIST_SEM_TAKE(WAIT_FOREVER);

	/*遍历所有设备*/
	for(pusbttyNode = (USB_TTY_NODE *) lstFirst (&usbttyList);
			pusbttyNode != NULL;
	        pusbttyNode = (USB_TTY_NODE *) lstNext ((NODE *) pusbttyNode)){
		pTempDev = pusbttyNode->pusbttyDev;
		/*找到即返回*/
		if (pTempDev->pSioChan == pChan){
			* (UINT32 *) ppusbttyDev = (UINT32) pTempDev;
			USB_TTY_LIST_SEM_GIVE;
			return (OK);
		}
	}
	USB_TTY_LIST_SEM_GIVE;
	return (ERROR);
}

//==============================================================================//
void usbttyRcvCallback(){
	printf("IN usbttyRcvCallback()");
}


//==============================================================================//
LOCAL void usbttyDrvAttachCallback(void *arg,SIO_CHAN *pChan,UINT16 attachCode)
{
	int usbttyUintNum;
	USB_TTY_DEV *pusbttyDev = NULL;
	char usbttyName[USB_TTY_NAME_LEN_MAX];

	if(attachCode == USB_TTY_ATTACH){
		printf("device detected success!\n");
		if(usbttySioChanLock(pChan) != OK){
			printf("usbttySioChanLock() returned ERROR\n");
		}else{
			usbttyUintNum = getusbttyNum(pChan);
			if(usbttyUintNum >= 0){
				sprintf(usbttyName,"%s%d",USB_TTY_NAME,usbttyUintNum);
				/*创建USB-TTY设备*/
				if(usbttyDevCreate(usbttyName,pChan) != OK){
					printf("usbttyDevCreate() returned ERROR\n");
					return ;
				}
			}else{
				printf("Excessive usbttys-check USB_TTY_DEVICE_NUM");
			}
			return;
		}

		if(usbttyDevFind(pChan,&pusbttyDev) != OK){
			printf("usbttyDevFind() returned ERROR\n");
			return;
		}

		if((*pChan->pDrvFuncs->callbackInstall)(pChan,SIO_CALLBACK_PUT_RCV_CHAR,
				(STATUS (*)(void *,...))usbttyRcvCallback,
				(void *)pusbttyDev)!=OK){
			printf("usbKbdRcvCallback() failed to install\n");
			return;
		}
		printf("USB_TTY attach as %s\n",usbttyName);

/*		//创建该设备的的结构体并将其初始化
		pusbttyDev = (USB_TTY_DEV *)calloc(1,sizeof(USB_TTY_DEV));
		if(pusbttyDev == NULL){
			printf("have no enough room\n");
			return ;
		}
		pusbttyDev->usbttyNodeid = nodeid;
		pusbttyDev->usb_ttyusbdClientHandle = usb_ttyusbdClientHandle;
		status = getAllConfiguration();
		if(status == OK){
			printf("get all configuration ok!\n");
		}else{
			printf("get configuration failed!\n");
			free(pusbttyDev);
			return;
		}

创建UBSTTY设备
		if(usbttyDevCreate(usbttyName)== OK){
			printf("create /usbtty/0 device OK!\n");
		}else{
			printf("create /usbtty/0 device failed\n");
			free(pusbttyDev);
			return;
		}*/

	}

	if(attachCode == USB_TTY_REMOVE){
		/*查找关联设备*/
		if (usbttyDevFind (pChan, &pusbttyDev) != OK){
			printf ("usbttyDevFind could not find channel 0x%d", (UINT32)pChan);
			return;
			}

		/*删除设备*/
		usbttyDevDelete (pusbttyDev);
		sprintf (usbttyName, "%s%d", USB_TTY_NAME, freeusbttyNum(pChan));

		if (usbttySioChanUnlock (pChan) != OK){
			printf("usbttySioChanUnlock () returned ERROR\n");
			return;
		}
		printf ("USB-TTY %s removed\n", usbttyName);
	}
}


/*创建USBTTY系统设备，底层驱动xxxxDevCreate进行设备创建的时候，在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS usbttyDevCreate(char *name,SIO_CHAN * pSioChan )
{
	/*status = usbdConfigurationSet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,pusbttyDev->configurationValue,100);
	if(status == OK){
		printf("set configuration ok\n");
	}else{
		printf("set configuration failed\n");
	}

	status = usbdInterfaceSet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,0,0);
	if(status != OK){
		printf("set interface failed.\n");
		return ERROR;
	}else{
		printf("set interface OK.\n");
	}

	USBD_PIPE_HANDLE inPipeHandle=NULL;
	USBD_PIPE_HANDLE outPipeHandle=NULL;
	USBD_PIPE_HANDLE ctlPipeHandle=NULL;
	UINT16 endPointIndex=1;

	status = usbdPipeCreate(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,endPointIndex,pusbttyDev->configuration,pusbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_IN,pusbttyDev->epMaxPacketSize,0,0,&inPipeHandle);
	if(status == OK){
		printf("create IN pipe for endpoint 1 OK!\n");
		pusbttyDev->inPipeHandle = inPipeHandle;
	}else{
		printf("create IN pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,endPointIndex,pusbttyDev->configuration,pusbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_OUT,pusbttyDev->epMaxPacketSize,0,0,&outPipeHandle);
	if(status == OK){
		printf("create OUT pipe for endpoint 2 OK!\n");
		pusbttyDev->outPipeHandle= outPipeHandle;

	}else{
		printf("create OUT pipe for endpoint 2 failed;status is %d;\n",status);
	}

	endPointIndex = 0;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,endPointIndex,pusbttyDev->configuration,pusbttyDev->interface,USB_XFRTYPE_CONTROL,USB_DIR_INOUT,pusbttyDev->epMaxPacketSize,0,0,&ctlPipeHandle);
	if(status == OK){
		printf("create control pipe in endpoint 0 ok!\n");
		pusbttyDev->ctlPipeHandle = ctlPipeHandle;
	}else{
		printf("create control pipe in endpoint 0 failed!\n");
	}


	UINT16 confValue;
	status =  usbdConfigurationGet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,&confValue);
	if(status != OK){
		printf("get configuration failed!\n");
	}else{
		printf("confVlaue is:%d",confValue);
	}*/


	/*将设备添加到系统的设备列表当中*/
	if((status = iosDevAdd (&pusbttyDev->usbttyDev,name,usbttyDrvNum)) != OK){
		free(pusbttyDev);
		printf("Unable to create ttyusb device.");
		return status;
	}
	return OK;
}




/*初始化USBTTY驱动*/
STATUS usbttyDrvInit(void)
{
	if(usbttyDrvNum > 0){
		printf("USBTTY already initialized.\n");
		return OK;
	}

	usbttyMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);
	usbttyListMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);

	if((usbttyMutex == NULL) || (usbttyListMutex == NULL)){
		printf("Resource Allocation Failure.\n");
		goto ERROR_HANDLER;
	}

	/*初始化链表*/
	lstInit(&usbttyList);

	usbttyDrvNum = iosDrvInstall(NULL,usbttyDevDelete,usbttyDevOpen,usbttyDevClose,\
			usbttyDevRead,usbttyDevWrite,usbttyDevIoctl);


/*检查是否为驱动安装空间*/
	if(usbttyDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		goto ERROR_HANDLER;
	}

	if(usbttyDevInit() == OK){
		printf("usbttyDevInit() returned OK\n");
		if(usbttyDynamicAttachRegister(usbttyDrvAttachCallback,(void *) NULL) != OK){
			 printf ("usbKeyboardDynamicAttachRegister() returned ERROR\n");
//			 usbttyDevShutdown();
			 goto ERROR_HANDLER;
		}
	}else{
		printf("usbttyDevInit() returned ERROR\n");
		goto ERROR_HANDLER;
	}

	int i;
	for(i=0;i< USB_TTY_DEVICE_NUM;++i){
		pusbttyDevArray[i]=NULL;
	}
	return OK;

ERROR_HANDLER:

	if(usbttyDrvNum){
		iosDrvRemove (usbttyDrvNum, 1);
		usbttyDrvNum = 0;
		}
	if (usbttyMutex != NULL)
		semDelete(usbttyMutex);

	if(usbttyListMutex != NULL)
		semDelete(usbttyListMutex);

	usbttyMutex = NULL;
	usbttyListMutex = NULL;
	return ERROR;
}



/*
 * Writes any 16-bits CP210X_register(req) whose value
 * is passed entirely in the wValue field of the USB request
 * */
LOCAL int cp210x_write_u16_reg(USB_TTY_DEV *pusbttyDev,UINT8 request,UINT16 value)
{
	status = usb_control_msg(pusbttyDev,request,REQTYPE_HOST_TO_INTERFACE,value,\
			pusbttyDev->interface,NULL,0,USB_TIMEOUT_DEFAULT);
	if(status != OK){
		printf("control message send failed!\n");
	}
	return status;
}



/*写USBTTY设备*/
int usbttyDevWrite(USB_TTY_DEV *pusbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
//	status = CreateTransfer(pusbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}

/*打开USBTTY串行设备，在此处对设备进行设置*/
int usbttyDevOpen(USB_TTY_DEV *pusbttyDev, char *name, int flags,int mode)
{
	printf("OK here im in usbttyDevOpen!\n");

	status = cp210x_write_u16_reg(pusbttyDev,CP210X_IFC_ENABLE,UART_ENABLE);
	if(status != OK){
		printf("set device failed!\n");
		return status;
	}

	/*configure the termios structure*/


	return OK;
}


/*关闭USBTTY串行设备*/
int usbttyDevClose(USB_TTY_DEV *pusbttyDev)
{
	if(!(pusbttyDev->numOpened)){
		sioIoctl(pusbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pusbttyDev);
}


int usbttyDevIoctl(USB_TTY_DEV *pusbttyDev,/*读取的设备*/
		int request/*请求编码*/,
		void *arg/*参数*/)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:	/*	调用IO控制函数*/
		status = (sioIoctl(pusbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
int usbttyDevRead(USB_TTY_DEV *pusbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*设置缓冲区*/
	memset(buffer,0,nBytes);
	sioIoctl(pusbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
//		return pusbttyDev->pSioChan->pDrvFuncs->pollInput(&pusbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
	/*返回读取的字节数*/
	return bytesToBeRead;
}





/*为USBTTY删除道系统设备*/
STATUS usbttyDevDelete(USB_TTY_DEV *pusbttyDev)
{
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}

	/*
	 *终止并释放选择的唤醒列表，调用selWakeUpAll，以防有数据。
	 *需删除所有在驱动上等待的任务。
	 */
	selWakeupAll (&pusbttyDev->selWakeupList, SELREAD);

	/*创建信号量*/
	if ((pusbttyDev->usbttySelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pusbttyDev->selWakeupList) > 0){
		/*列表上有未解决的任务，等待信号在终结列表前释放任务*/
		/*等待信号*/
		semTake (pusbttyDev->usbttySelectSem, WAIT_FOREVER);
	}

	/*删除信号量*/

	semDelete (pusbttyDev->usbttySelectSem);

	selWakeupListTerm(&pusbttyDev->selWakeupList);

	/*从I/O系统删除设备*/
	iosDevDelete(&pusbttyDev->usbttyDev);

	/*从列表删除*/
	USB_TTY_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&usbttyList, (NODE *) pusbttyDev->pusbttyNode);
	USB_TTY_LIST_SEM_GIVE;

	/*设备释放内存*/
	free(pusbttyDev->pusbttyNode);
	free(pusbttyDev);
	return OK;
}





void usbBulkIrpCallback(pVOID pIrp)
{
	printf("I'm in tranferCallback now\n");
	return ;
}

LOCAL int usb_internal_control_msg(USB_TTY_DEV *pusbttyDev,USB_SETUP *cmd,void *data,int len,int timeout)
{
	USB_IRP *pIrp;
	pIrp = (USB_IRP *)calloc(1,sizeof(USB_IRP));
	if(pIrp == NULL){
		printf("have no enough room for IRP\n");
		return ERROR;
	}else{
		printf("malloc IRP room OK\n");
	}
	pIrp->bfrList[0].pid = USB_PID_SETUP;
//	pIrp->bfrList[0].pBfr = (pUINT8)cmd;
//	pIrp->bfrList[0].bfrLen = sizeof(USB_SETUP);

	pIrp->flags  = USB_FLAG_SHORT_OK;
	pIrp->timeout = timeout;
	pIrp->dataToggle = USB_DATA0;
	pIrp->irpLen  = sizeof(USB_IRP);
//	pIrp->transferLen = sizeof(USB_SETUP);
//	pIrp->bfrCount = 1;
	pIrp->userCallback = usbBulkIrpCallback;
	pIrp->userPtr = pusbttyDev;

	status = usbdTransfer(pusbttyDev->usb_ttyusbdClientHandle,pusbttyDev->ctlPipeHandle,pIrp);
	if(status != OK){
		printf("only SETUP send !usbdTransfer failed status value is:%d\n",status);
		return status;
	}else{
		printf("usbdTransfer success\n");
		return status;
	}


}

 int usb_control_msg(USB_TTY_DEV *pusbttyDev,UINT8 request,\
		 UINT8 requestType,UINT16 value,UINT16 index,void *data,UINT16 length,UINT32 timeout)
// request = CP210X_IFC_ENABLE ,requestType = REQTYPE_HOST_TO_INTERFACE,value = UART_ENABLE,
 // index = pusbttyDev->interfaceNumber  , data = NULL, length = 0, timeout = USB_CTRL_SET_TIMEOUT
 {
	 USB_SETUP *dr;
	 int ret;

	 dr = (USB_SETUP *)calloc(1,sizeof(USB_SETUP));
	 if(dr == NULL){
		 printf("there have no enough room for USB_SETUP packet\n");
		 return ERROR;
	 }
	 dr->requestType 	= requestType;
	 dr->request 	 	= request;
	 dr->value			= value;
	 dr->length 		= length;
	 dr->index 			= index;
//	 dr->index 			= 0;



	 ret = usb_internal_control_msg(pusbttyDev,dr,data,length,timeout);
	 if(ret != OK){
		 return ERROR;
	 }
	 free(dr);

	return ret;
 }


STATUS getAllConfiguration(void)
{
	UINT16 ActLen;
	pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
	UINT16 bfrLen = 38;
	pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
	pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

#ifdef DESC_PRINT
	USB_DEVICE_DESCR usb_ttyDeviceDescr;
	status = usbdDescriptorGet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
	if(status == OK){
		printf("\nDevice Descriptor:\n");
		printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
		printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
				usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
				usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
				usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
	}else{
		printf("get descriptor failed!\n");
	}
#endif
// bLength:18(设备描述符所占的字节为18个字节)。     bDescriptorType:1(设备描述符类型为1，表示这是什么？？？)       bcdUSB:0x110(表示遵循USB1.1规范)
// bdeviceClass:0(若为0，则每一个接口都要说明他的信息类型)  bdeviceSubClass:0(由于deviceClass为0，该域必须为0)   bdeviceProtocol:0(为0说明不是使用的基于一个设备的某一个类型的协议)
// bMaxPacketSize:64(用于端点零的最大分组尺寸)   idVendor:0x10c4(供应商ID由USB分配)   idProduct:0xea60(产品ID由厂家分配)
// bcdDevice： 0x100(二进制编码小数形式的设备发行号)    iMancufacturer:1(用于描述厂商的字符串描述符的索引) iProduct:2(用于描述产品的字符串描述符的索引)  iSerial:3(用于描述设备序列号的字符串描述符的索引)
// bNumConfiguration:1(可能的配置数)


	status = usbdDescriptorGet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
	if(status == OK){
#ifdef DESC_PRINT
				printf("\nConfiguration:\n");
				printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
				printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
						pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
						pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
#endif
// actual length:32(返回的数据的总长度为32字节);     buffer length is:38(提供的空间为38个字节);   bLength:9(配置描述符所占的长度为9 );  DescriptorType:2(表示该描述符指向的是一个设备）;
// wTotalLength:32(表示该配置+接口+端点描述符的总长度)   bNumInterface:1(该配置下接口数为1个)；          bConfigurationValue:1(该配置的值为1);   iConfiguration：0(用于描述该配置的字符串描述符的索引)
// bmAttributes:0x80(D7为1表示总线供电，D6为0表示不是自供电，D5为0表示不支持远程唤醒，D4-D0保留);    MaxPower:50(电流大小，以2mA为单位，50表示100mA)
				pusbttyDev->configuration      = 0;
				pusbttyDev->configurationValue = pusb_ttyConfigDescr->configurationValue;
				pusbttyDev->maxPower		   = pusb_ttyConfigDescr->maxPower;

#ifdef DESC_PRINT
		//打印Interface描述符
			printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
						pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
						pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
#endif
// Length:9(接口描述符的总长度为9);  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
// numEndpoint:2(该接口所支持的端点数，不包括0端点)  interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；
// interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)
				pusbttyDev->altSetting = pusb_ttyInterfaceDescr->alternateSetting;
				pusbttyDev->interface  = 0;
				pusbttyDev->interfaceNumber = pusb_ttyInterfaceDescr->interfaceNumber;

#ifdef DESC_PRINT
		//打印EndPoint 1 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
						pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点1为输入端点
				pusbttyDev->inEndpointAddress = pusb_ttyEndpointDescr1->endpointAddress ;
				pusbttyDev->epMaxPacketSize   = pusb_ttyEndpointDescr1->maxPacketSize;


#ifdef DESC_PRINT
		//打印EndPoint 2 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
						pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点2为输出端点

				pusbttyDev->outEndpointAddress = pusb_ttyEndpointDescr2->endpointAddress;

				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
	}else{
				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
			}

	return status;
}



/*cp210xLib.c*/
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

unsigned int TYPEMATIC_DELAY  = 500;    /* 500 msec delay */
unsigned int TYPEMATIC_PERIOD = 66;     /* 66 msec = approx 15 char/sec */

typedef struct attach_request{
	LINK reqLink;						/*linked list of requests*/
	USB_TTY_ATTACH_CALLBACK callback; 	/*client callback routine*/
	pVOID callbackArg;  				/*client callback argument*/
}ATTACH_REQUEST,*pATTACH_REQUEST;


typedef struct usb_tty_sio_chan{
	SIO_CHAN sioChan;		/* must be first field */
	LINK sioLink;	        /* linked list of usb_tty structs */
	UINT16 lockCount;		/* Count of times structure locked */
	USBD_NODE_ID nodeId;	/* usb_tty node Id */
	UINT16 configuration;	/* configuration/interface reported as */
	UINT16 interface;		/* a usb_tty interface in this device */
	BOOL connected;	        /* TRUE if usb_tty currently connected */

	USBD_PIPE_HANDLE 	inPipeHandle;  		/*cp210x pipe handle for in data*/
	USBD_PIPE_HANDLE 	outPipeHandle;		/*cp210x pipe handle for out data*/
	USBD_PIPE_HANDLE 	ctlPipeHandle;		/*cp210x pipe handle for control data*/
	USB_IRP 			inIrp;				/*cp210x IRP to monitor in pipe*/
	USB_IRP 			outIrp;				/*cp210x IRP to monitor out pipe*/
	USB_IRP				ctlIrp;				/*cp210x IRP to monitor control pipe*/


	USBD_PIPE_HANDLE pipeHandle;/* USBD pipe handle for interrupt pipe */
	USB_IRP irp;	        /* IRP to monitor interrupt pipe */
	BOOL irpInUse;	        /* TRUE while IRP is outstanding */
	pHID_KBD_BOOT_REPORT pBootReport;/* Keyboard boot report fetched thru pipe */
//	char inQueue [KBD_Q_DEPTH]; /* Circular queue for keyboard input */
//	UINT16 inQueueCount;	/* count of characters in input queue */
//	UINT16 inQueueIn;		/* next location in queue */
//	UINT16 inQueueOut;		/* next character to fetch */

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

//============================================================================//
LOCAL int usbttyTxStartUp(SIO_CHAN *pSioChan);
LOCAL int usbttyCallbackInstall(SIO_CHAN *pSioChan,int CallbackType,
		STATUS(*callback)(void *,...),void *callbackArg);
LOCAL int usbttyPollOutput(SIO_CHAN *pSioChan,char outChar);
LOCAL int usbttyPollInput(SIO_CHAN *pSioChan,char *thisChar);
LOCAL int usbttyIoctl(SIO_CHAN *pSioChan,int request,void *arg);

LOCAL VOID usbttyIrpCallback(pVOID p);


//=================================================================================================//
LOCAL SIO_DRV_FUNCS usbttySioDrvFuncs ={
		usbttyIoctl,
		usbttyTxStartUp,
		usbttyCallbackInstall,
		usbttyPollInput,
		usbttyPollOutput
};


//=================================================================================================//

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

	printf("configurationValue is :%d\n",pCfgDescr->configurationValue);
	/* Select the configuration. */
	if (usbdConfigurationSet (usbdHandle,pSioChan->nodeId,
			pCfgDescr->configurationValue,pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT)!= OK){
		OSS_FREE (pBfr);
		return FALSE;
	}

	/* Select interface
	 * NOTE: Some devices may reject this command, and this does not represent
	 * a fatal error.  Therefore, we ignore the return status.*/
	if(usbdInterfaceSet (usbdHandle,pSioChan->nodeId,pSioChan->interface,
			pIfDescr->alternateSetting) != OK)
		printf("usbdInterfaceSet Failed\n");
	printf("interface alternate setting is %d\n",pIfDescr->alternateSetting);

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

/* Create a pipe to monitor input data from cp210x*/
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

/*	if ((pSioChan->pBootReport= OSS_CALLOC(sizeof(*pSioChan->pBootReport)))== NULL){
		OSS_FREE (pSioChan);
		return NULL;
	}*/

	pSioChan->sioChan.pDrvFuncs = &usbttySioDrvFuncs;
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
				USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbttyAttachCallback)!= OK){
				printf("usbd register failed!\n");
//				return doShutdown (S_usbKeyboardLib_USBD_FAULT);
			}else{
				printf("usbdClientRegister() returned OK,usbdDynamicAttachRegister() returned OK\n");
			}
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
//===============================================================================================//
/* start a output transport
 * */
LOCAL int usbttyTxStartUp(SIO_CHAN *pChan)
{
	printf("IN usbttyTxStartUp\n");
//TODO
	return EIO;
}



//===============================================================================================//
/*usbttyCallbackInstall - install ISR callbacks to get/put chars
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.
 * */

LOCAL int usbttyCallbackInstall(SIO_CHAN *pChan,int callbackType,
		STATUS(*callback)(void *tmp,...),void *callbackArg)
{
	pUSB_TTY_SIO_CHAN pSioChan = (pUSB_TTY_SIO_CHAN)pChan;
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


//===============================================================================================//
/*
 * output a character in polled mode.
 * */
LOCAL int usbttyPollOutput(SIO_CHAN *pChan,char outChar)
{
	printf("IN usbttyPollOutput");
	//TODO
	return OK;
}


//===============================================================================================//
/*
 * poll the device for input
 * */
LOCAL int usbttyPollInput(SIO_CHAN *pChan,char *thisChar)
{
	printf("IN usbttyPollInput\n");
	pUSB_TTY_SIO_CHAN pSioChan = (pUSB_TTY_SIO_CHAN)pChan;
	int status = OK;

	if(thisChar == NULL)
		return EIO;

	OSS_MUTEX_TAKE(usbttyMutex,OSS_BLOCK);
	/* Check if the input queue is empty. */

/*	if (pSioChan->inQueueCount == 0)
		status = EAGAIN;
	else{
	 Return a character from the input queue.
		*thisChar = nextInChar (pSioChan);
	}*/

//TODO

	OSS_MUTEX_RELEASE (usbttyMutex);
	return status;
}

//===============================================================================================//
/*
 * usbttyIoctl - special device control
 * this routine is largely a no-op for the function.c the only ioctls
 * which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
 *
 *
 * */
LOCAL int usbttyIoctl(SIO_CHAN *pChan,	    /* device to control */
		int request,	/* request code */
		void *someArg	/* some argument */
		){
	pUSB_TTY_SIO_CHAN pSioChan = (pUSB_TTY_SIO_CHAN) pChan;
	int arg = (int) someArg;

	switch (request){

	case SIO_BAUD_SET:
		/* baud rate has no meaning for USB.  We store the desired
		 * baud rate value and return OK.
		 */
		pSioChan->baudRate = arg;
		return OK;


	case SIO_BAUD_GET:

		/* Return baud rate to caller */
		*((int *) arg) = pSioChan->baudRate;
		return OK;

	case SIO_MODE_SET:

		/* Set driver operating mode: interrupt or polled */
		if (arg != SIO_MODE_POLL && arg != SIO_MODE_INT)
		return EIO;

		pSioChan->mode = arg;
		return OK;


	case SIO_MODE_GET:

		/* Return current driver operating mode for channel */

		*((int *) arg) = pSioChan->mode;
		return OK;


	case SIO_AVAIL_MODES_GET:

		/* Return modes supported by driver. */

		*((int *) arg) = SIO_MODE_INT | SIO_MODE_POLL;
		return OK;


	case SIO_OPEN:

		/* Channel is always open. */
		return OK;

//TODO

	/*case SIO_KYBD_MODE_SET:

		switch (arg){
		case SIO_KYBD_MODE_RAW:

		case SIO_KYBD_MODE_ASCII:
			break;

		case SIO_KYBD_MODE_UNICODE:
			return ENOSYS;  usb doesn't support unicode
		}

		pSioChan->scanMode = arg;
			 return OK;


	case SIO_KYBD_MODE_GET:

		*(int *)someArg = pSioChan->scanMode;
		return OK;

	case SIO_KYBD_LED_SET:{
		UINT8 ledReport;

		  update the channel's information about the LED state
		pSioChan->numLock = (arg & SIO_KYBD_LED_NUM) ? SIO_KYBD_LED_NUM : 0;

		pSioChan->capsLock = (arg & SIO_KYBD_LED_CAP) ? SIO_KYBD_LED_CAP : 0;

		pSioChan->scrLock = (arg & SIO_KYBD_LED_SCR) ? SIO_KYBD_LED_SCR : 0;


		 * We are relying on the SIO_KYBD_LED_X macros matching the USB
		 * LED equivelants.


		ledReport = arg;

		 set the LED's

		setLedReport (pSioChan, ledReport);

		return OK;
	}


	case SIO_KYBD_LED_GET:{
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
}


//===============================================================================================//
/*
 * Initialize IRP to transfer for control on control pipe
 * */
LOCAL BOOL initusbttyIrp(pUSB_TTY_SIO_CHAN pSioChan)
{
	pUSB_IRP pIrp = &pSioChan->ctlIrp;

	/* Initialize IRP */
	memset (pIrp, 0, sizeof (*pIrp));

	pIrp->userPtr = pSioChan;
	pIrp->irpLen = sizeof (*pIrp);
	pIrp->userCallback = usbttyIrpCallback;
	pIrp->timeout = USB_TIMEOUT_NONE;
	pIrp->transferLen = sizeof (HID_KBD_BOOT_REPORT);

	pIrp->bfrCount = 1;
	pIrp->bfrList [0].pid = USB_PID_IN;
	pIrp->bfrList [0].pBfr = (pUINT8) pSioChan->pBootReport;
	pIrp->bfrList [0].bfrLen = sizeof (HID_KBD_BOOT_REPORT);


	/* Submit IRP */
	if (usbdTransfer (usbdHandle, pSioChan->pipeHandle, pIrp) != OK){
		printf("usbdTransfer() failed!\n");
		return FALSE;
	}

	pSioChan->irpInUse = TRUE;

	return TRUE;
}

//===============================================================================================//
LOCAL VOID usbttyIrpCallback(pVOID p)
{
	pUSB_IRP pIrp =(pUSB_IRP)p;
	pUSB_TTY_SIO_CHAN pSioChan = pIrp->userPtr ;

	OSS_MUTEX_TAKE(usbttyMutex,OSS_BLOCK);

	/* Was the IRP successful? */

	if (pIrp->result == OK){
		printf("IRP send OK\n");
		/* Interpret the keyboard report */
		/*interpKbdReport (pSioChan);*/
	}

	/* Re-submit the IRP unless it was canceled - which would happen only
	 * during pipe shutdown (e.g., the disappearance of the device).
	 */

	pSioChan->irpInUse = FALSE;
	if (pIrp->result != S_usbHcdLib_IRP_CANCELED){
		initusbttyIrp (pSioChan);
	}else {
		if (!pSioChan->connected){
			/* Release structure. */
			if (pSioChan->pBootReport != NULL)
				OSS_FREE (pSioChan->pBootReport);

		OSS_FREE (pSioChan);
		}
	}
	OSS_MUTEX_RELEASE (usbttyMutex);
}







//=============================2017.06.09 10:30 end    A totally new part==========================//


















































//===========================================2017.05.31 20:02 BEGIN===============================================//

#include <iosLib.h>
#include "usbhotprobe.h"

//#define     DESC_PRINT
#define 	__DEBUG__

#ifdef 		__DEBUG__
	#include <D:\LambdaPRO615\target\deltaos\include\debug\utils\stdarg.h>
	void DEBUG(const char *fmt,...)
	{
		va_list ap;
//		va_list (ap,fmt);
		vprintf(fmt,ap);
		va_end(ap);
	}
#else
	void DEBUG(const char *fmt,...){}
#endif

#define usbttyDevNamePrefix "/usbtty"

/* Config request types */
#define REQTYPE_HOST_TO_DEVICE	0x41
#define REQTYPE_DEVICE_TO_HOST	0xc1

/* Config request codes */
#define CP210X_IFC_ENABLE		0x00
#define CP210X_SET_BAUDDIV		0x01
#define CP210X_GET_BAUDDIV		0x02
#define CP210X_SET_LINE_CTL		0x03
#define CP210X_GET_LINE_CTL		0x04
#define CP210X_SET_BREAK		0x05
#define CP210X_IMM_CHAR			0x06
#define CP210X_SET_MHS			0x07
#define CP210X_GET_MDMSTS		0x08
#define CP210X_SET_XON			0x09
#define CP210X_SET_XOFF			0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR			0x0D
#define CP210X_GET_CHARS		0x0E
#define CP210X_GET_PROPS		0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET			0x11
#define CP210X_PURGE			0x12
#define CP210X_SET_FLOW			0x13
#define CP210X_GET_FLOW			0x14
#define CP210X_EMBED_EVENTS		0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS		0x19

/* CP210X_IFC_ENABLE */
#define UART_ENABLE				0x0001
#define UART_DISABLE			0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ		0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK			0X0f00
#define BITS_DATA_5				0X0500
#define BITS_DATA_6				0X0600
#define BITS_DATA_7				0X0700
#define BITS_DATA_8				0X0800
#define BITS_DATA_9				0X0900

#define BITS_PARITY_MASK		0x00f0
#define BITS_PARITY_NONE		0x0000
#define BITS_PARITY_ODD			0x0010
#define BITS_PARITY_EVEN		0x0020
#define BITS_PARITY_MARK		0x0030
#define BITS_PARITY_SPACE		0x0040

#define BITS_STOP_MASK			0x000f
#define BITS_STOP_1				0x0000
#define BITS_STOP_1_5			0x0001
#define BITS_STOP_2				0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON				0x0001
#define BREAK_OFF				0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR				0x0001
#define CONTROL_RTS				0x0002
#define CONTROL_CTS				0x0010
#define CONTROL_DSR				0x0020
#define CONTROL_RING			0x0040
#define CONTROL_DCD				0x0080
#define CONTROL_WRITE_DTR		0x0100
#define CONTROL_WRITE_RTS		0x0200



STATUS status;
USB_TTY_DEV 				*pUsbttyDev = NULL;
LOCAL int 					usbttyDrvNum = -1; 	//驱动的数目
LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle = NULL;
LOCAL LIST					usbttyList;			//系统所有的USBTTY设备

STATUS getAllConfiguration(void);
LOCAL STATUS usbttyDevCreate(char *name);
 int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,  char *buffer,  UINT32 nBytes);
int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes);
int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev,  char *name,  int flags,int mode);
int usbttyDevClose(USB_TTY_DEV *pUsbttyDev);
int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,  int request,  void *arg);
STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev);
STATUS cp201x_set_config(unsigned char request,unsigned int *data,int size);


LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{

	if(attachAction == USBD_DYNA_ATTACH){
		printf("device detected success!\n");
		DEBUG("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);
//callback nodeid is : 0x103(地址);configuration is: 0;   interface is 0;

		//创建该设备的的结构体并将其初始化
		pUsbttyDev = (USB_TTY_DEV *)calloc(1,sizeof(USB_TTY_DEV));
		if(pUsbttyDev == NULL){
			printf("have no enough room\n");
			return ;
		}
		pUsbttyDev->usbttyNodeid = nodeid;

		status = getAllConfiguration();
		if(status == OK){
			printf("get all configuration ok!\n");
		}else{
			printf("get configuration failed!\n");
			free(pUsbttyDev);
			return;
		}


/*创建UBSTTY设备*/
		if(usbttyDevCreate(usbttyDevNamePrefix)== OK){
			printf("create /usbtty/0 device OK!\n");
		}else{
			printf("create /usbtty/0 device failed\n");
			free(pUsbttyDev);
			return;
		}


	}

	if(attachAction == USBD_DYNA_REMOVE){
		if(usbttyDevDelete(pUsbttyDev)!= OK){
			printf("delete device failed!\n");
		}else{
			printf("delete device success!\n");
		}
		printf("device remove!");
		return ;
	}


}



void transferCallback(pVOID pIrp)
{
	printf("I'm in tranferCallback now\n");
	return ;
}


STATUS CreateTransfer(USBD_PIPE_HANDLE Pipe)
{

	char p[20]= "hello world.";
	USB_IRP Irp;

	Irp.bfrList[0].bfrLen = 64;
	Irp.bfrList[0].pBfr = (pUINT8)&p;
	Irp.bfrList[0].pid = USB_PID_OUT;
	Irp.bfrList[0].actLen = 13;


	Irp.flags = USB_FLAG_SHORT_OK;
	Irp.startFrame = 0;
	Irp.dataBlockSize = 0;
	Irp.timeout = USB_TIMEOUT_DEFAULT;
	Irp.userCallback = transferCallback;
	Irp.irpLen =sizeof(USB_IRP);
	Irp.bfrCount = 1;

	status = usbdTransfer(usb_ttyusbdClientHandle,Pipe,&Irp);
	if(status != OK){
		printf("transfer data failed!\n");
	}else{
		printf("transfer data OK\n");
	}

	return status;

}


/*创建USBTTY系统设备，底层驱动xxxxDevCreate进行设备创建的时候，在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS usbttyDevCreate(char *name)
{
	char devname[256];
	sprintf(devname,"%s/%d",name,0);

	status = usbdConfigurationSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,pUsbttyDev->configurationValue,100);
	if(status == OK){
		printf("set configuration ok\n");
	}else{
		printf("set configuration failed\n");
	}

	status = usbdInterfaceSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,0,0);
	if(status != OK){
		printf("set interface failed.\n");
		return ERROR;
	}else{
		printf("set interface OK.\n");
	}

	USBD_PIPE_HANDLE inPipeHandle=NULL;
	USBD_PIPE_HANDLE outPipeHandle=NULL;
	USBD_PIPE_HANDLE ctlPipeHandle=NULL;
	UINT16 endPointIndex=1;

	status = usbdPipeCreate(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,endPointIndex,pUsbttyDev->configuration,pUsbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_IN,pUsbttyDev->epMaxPacketSize,0,0,&inPipeHandle);
	if(status == OK){
		printf("create IN pipe for endpoint 1 OK!\n");
		pUsbttyDev->inPipeHandle = inPipeHandle;
	}else{
		printf("create IN pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,endPointIndex,pUsbttyDev->configuration,pUsbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_OUT,pUsbttyDev->epMaxPacketSize,0,0,&outPipeHandle);
	if(status == OK){
		printf("create OUT pipe for endpoint 2 OK!\n");
		pUsbttyDev->outPipeHandle= outPipeHandle;

	}else{
		printf("create OUT pipe for endpoint 2 failed;status is %d;\n",status);
	}

	endPointIndex = 0;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,endPointIndex,pUsbttyDev->configuration,pUsbttyDev->interface,USB_XFRTYPE_CONTROL,USB_DIR_INOUT,pUsbttyDev->epMaxPacketSize,0,0,&ctlPipeHandle);
	if(status == OK){
		printf("create control pipe in endpoint 0 ok!\n");
		pUsbttyDev->ctlPipeHandle = ctlPipeHandle;
	}else{
		printf("create control pipe in endpoint 0 failed!\n");
	}


	UINT16 confValue;
	status =  usbdConfigurationGet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,&confValue);
	if(status != OK){
		printf("get configuration failed!\n");
	}else{
		printf("confVlaue is:%d",confValue);
	}


	/*将设备添加到系统的设备列表当中*/
	if((status = iosDevAdd (&pUsbttyDev->usbttyDev,devname,usbttyDrvNum)) != OK){
		free(pUsbttyDev);
		printf("Unable to create ttyusb device.");
		return status;
	}
	return OK;
}




/*初始化USBTTY驱动*/
STATUS usrUsbttyDevInit(void)
{
//	int index = 0;

/*检查驱动是否已经安装,如果已经安装就正常退出*/
	if(usbttyDrvNum > 0){
		printf("USBTTY already initialized.\n");
		return OK;
	}

/*创建互斥的信号量*/
	SEM_ID usbttyMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	SEM_ID usbttyListMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	if(usbttyMutex == NULL || usbttyListMutex == NULL){
		printf("Resource Allocation Failure!\n");
//		goto ERROR_HANDLE;
	}
/*初始化链表*/
	lstInit(&usbttyList);


/*安装合适的程序到驱动列表,iosDrvInstall向I/O子系统注册驱动本身，此时这个驱动被保存在系统的驱动表当中，
 * 等待设备与其进行衔接，衔接的工作由xxxxDevCreate()函数来完成，在iosDevCreate()函数中调用iosDevAdd()
 * 函数添加设备时，需要指定设备对应的驱动程序的驱动号，就是xxxxDrvNum中存储的驱动号。从而将将设备和底层驱动进行衔接。
 * 当用户对该设备进行操作的时候，可以调用此处定义的底层驱动函数*/
	usbttyDrvNum = iosDrvInstall((FUNCPTR)NULL,(FUNCPTR)usbttyDevDelete, (FUNCPTR) usbttyDevOpen,
			(FUNCPTR) usbttyDevClose,(FUNCPTR) usbttyDevRead,(FUNCPTR) usbttyDevWrite,(FUNCPTR) usbttyDevIoctl);


/*检查是否为驱动安装空间*/
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		return ERROR;
//		goto ERROR_HANDLER;
	}

//	s=usbdInitialize();
//	printf("usbdInitialize returned %d\n",s);

	status =usbdClientRegister("/usbtty",&usb_ttyusbdClientHandle);
	if (status == OK){
		printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
	}else{
		printf("client register failed!\n");
		return  ERROR;
	}

	status =usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,&usbTestDevAttachCallback);
			if(status == OK){
				printf("usbdDynamicAttachRegister success!\n");
			}else{
				printf("usbdDynamicAttachRegister failed\n");
				return ERROR;
			}
	return OK;
}


STATUS cp201x_set_config(unsigned char request,unsigned int *data,int size)
{
	unsigned int *buf;
	int result,i,length;

	/*Number of intergers required to contian the array*/
	length = (((size-1) | 3) + 1)/4; //length = ((1 | 3)+1)/4 = 1
	buf = (unsigned int *)calloc(length , sizeof(unsigned int));
	if(buf == NULL){
		printf("there have no enough romm!\n");
		return ERROR;
	}

	/*array of integers into bytes*/
	for(i=0;i<length;i++){
		buf[i] = data[i];    //即 *buf = *data
	}

	/*if(size > 2){
		result = usb_control_msg();
	}*/

}



/*写USBTTY设备*/
int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
	status = CreateTransfer(pUsbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}

/*打开USBTTY串行设备，在此处对设备进行设置*/
int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev, char *name, int flags,int mode)
{
	printf("OK here im in usbttyDevOpen!\n");
	unsigned char request = 0x00;
	int data = 0x0001;
	status = cp201x_set_config(request,&data,2);
	if(status != OK){
		printf("set device failed!\n");
		return ERROR;
	}


//	status = usbdConfigurationSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,1,);


//	pUsbttyDev->numOpened++;
//	sioIoctl(&pUsbttyDev->pSioChan,SIO_OPEN,NULL);
//	return ((int)pUsbttyDev);
	return OK;
}


/*关闭USBTTY串行设备*/
int usbttyDevClose(USB_TTY_DEV *pUsbttyDev)
{
	if(!(pUsbttyDev->numOpened)){
		sioIoctl(&pUsbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pUsbttyDev);
}


int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,/*读取的设备*/
		int request/*请求编码*/,
		void *arg/*参数*/)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:	/*	调用IO控制函数*/
		status = (sioIoctl(&pUsbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*设置缓冲区*/
	memset(buffer,0,nBytes);
	sioIoctl(&pUsbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
//		return pUsbttyDev->pSioChan->pDrvFuncs->pollInput(&pUsbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
	/*返回读取的字节数*/
	return bytesToBeRead;
}





/*为USBTTY删除道系统设备*/
STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev)
{
	if(usbttyDrvNum < 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


	/*从I/O系统删除设备*/
	iosDevDelete(&pUsbttyDev->usbttyDev);

	/*从列表删除*/
	//TODO

	/*设备释放内存*/
//	free(pUsbttyDev->pUsbttyNode);
	free(pUsbttyDev);
	return OK;
}


STATUS getAllConfiguration(void)
{
	UINT16 ActLen;
	pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
	UINT16 bfrLen = 38;
	pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
	pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

	status = usbdDescriptorGet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
	if(status == OK){
#ifdef DESC_PRINT
				printf("\nConfiguration:\n");
				printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
				printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
						pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
						pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
#endif
// actual length:32(返回的数据的总长度为32字节);     buffer length is:38(提供的空间为38个字节);   bLength:9(配置描述符所占的长度为9 );  DescriptorType:2(表示该描述符指向的是一个设备）;
// wTotalLength:32(表示该配置+接口+端点描述符的总长度)   bNumInterface:1(该配置下接口数为1个)；          bConfigurationValue:1(该配置的值为1);   iConfiguration：0(用于描述该配置的字符串描述符的索引)
// bmAttributes:0x80(D7为1表示总线供电，D6为0表示不是自供电，D5为0表示不支持远程唤醒，D4-D0保留);    MaxPower:50(电流大小，以2mA为单位，50表示100mA)
				pUsbttyDev->configuration      = 0;
				pUsbttyDev->configurationValue = pusb_ttyConfigDescr->configurationValue;
				pUsbttyDev->maxPower		   = pusb_ttyConfigDescr->maxPower;

#ifdef DESC_PRINT
		//打印Interface描述符
			printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
						pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
						pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
#endif
// Length:9(接口描述符的总长度为9);  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
// numEndpoint:2(该接口所支持的端点数，不包括0端点)  interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；
// interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)
				pUsbttyDev->altSetting = pusb_ttyInterfaceDescr->alternateSetting;
				pUsbttyDev->interface  = 0;

#ifdef DESC_PRINT
		//打印EndPoint 1 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
						pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点1为输入端点
				pUsbttyDev->inEndpointAddress = pusb_ttyEndpointDescr1->endpointAddress ;
				pUsbttyDev->epMaxPacketSize   = pusb_ttyEndpointDescr1->maxPacketSize;


#ifdef DESC_PRINT
		//打印EndPoint 2 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
						pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点2为输出端点

				pUsbttyDev->outEndpointAddress = pusb_ttyEndpointDescr2->endpointAddress;

				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
	}else{
				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
			}

	return status;
}

//===========================================2017.05.31 20:02 END=================================================


//===========================================2017.05.26 19:25 BEGIN===============================================
#include <iosLib.h>
#include "usbhotprobe.h"

#define 	__DEBUG__
#ifdef 		__DEBUG__
	#include <D:\LambdaPRO615\target\deltaos\include\debug\utils\stdarg.h>
	void DEBUG(const char *fmt,...)
	{
		va_list ap;
//		va_list (ap,fmt);
		vprintf(fmt,ap);
		va_end(ap);
	}
#else
	void DEBUG(const char *fmt,...){}
#endif

#define usbttyDevNamePrefix "/usbtty"

STATUS status;
USB_TTY_DEV 				*pUsbttyDev = NULL;
LOCAL int 					usbttyDrvNum = -1; 	//驱动的数目
LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle = NULL;
LOCAL LIST					usbttyList;			//系统所有的USBTTY设备

STATUS getAllConfiguration(void);
LOCAL STATUS usbttyDevCreate(char *name);
 int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,  char *buffer,  UINT32 nBytes);
int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes);
int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev,  char *name,  int flags,int mode);
int usbttyDevClose(USB_TTY_DEV *pUsbttyDev);
int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,  int request,  void *arg);
STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev);



LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{

	if(attachAction == USBD_DYNA_ATTACH){
		printf("device detected success!\n");
		DEBUG("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);
//callback nodeid is : 0x103(地址);configuration is: 0;   interface is 0;

		//创建该设备的的结构体并将其初始化
		pUsbttyDev = (USB_TTY_DEV *)calloc(1,sizeof(USB_TTY_DEV));
		if(pUsbttyDev == NULL){
			printf("have no enough room\n");
			return ;
		}
		pUsbttyDev->usbttyNodeid = nodeid;

		status = getAllConfiguration();
		if(status == OK){
			printf("get all configuration ok!\n");
		}else{
			printf("get configuration failed!\n");
			free(pUsbttyDev);
			return;
		}


/*创建UBSTTY设备*/
		if(usbttyDevCreate(usbttyDevNamePrefix)== OK){
			printf("create /usbtty/0 device OK!\n");
		}else{
			printf("create /usbtty/0 device failed\n");
			free(pUsbttyDev);
			return;
		}
//		ttyDevCreate();









//	usbdInterfaceSet(usb_ttyusbdClientHandle,nodeid,0,0);





	}

	if(attachAction == USBD_DYNA_REMOVE){
		if(usbttyDevDelete(pUsbttyDev)!= OK){
			printf("delete device failed!\n");
		}else{
			printf("delete device success!\n");
		}
		printf("device remove!");
		return ;
	}


}



void transferCallback(pVOID pIrp)
{
	printf("I'm in tranferCallback now\n");
	return ;
}


STATUS CreateTransfer(USBD_PIPE_HANDLE Pipe)
{

	char p[20]= "hello world.";
	USB_IRP Irp;

	Irp.bfrList[0].bfrLen = 64;
	Irp.bfrList[0].pBfr = (pUINT8)&p;
	Irp.bfrList[0].pid = USB_PID_OUT;
	Irp.bfrList[0].actLen = 13;


	Irp.flags = USB_FLAG_SHORT_OK;
	Irp.startFrame = 0;
	Irp.dataBlockSize = 0;
	Irp.timeout = USB_TIMEOUT_DEFAULT;
	Irp.userCallback = transferCallback;
	Irp.irpLen =sizeof(USB_IRP);
	Irp.bfrCount = 1;

	status = usbdTransfer(usb_ttyusbdClientHandle,Pipe,&Irp);
	if(status != OK){
		printf("transfer data failed!\n");
	}else{
		printf("transfer data OK\n");
	}

	return status;

}


/*创建USBTTY系统设备，底层驱动xxxxDevCreate进行设备创建的时候，在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS usbttyDevCreate(char *name)
{
	char devname[256];
	sprintf(devname,"%s/%d",name,0);

	status = usbdConfigurationSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,pUsbttyDev->configurationValue,100);
	if(status == OK){
		printf("set configuration ok\n");
	}else{
		printf("set configuration failed\n");
	}

	status = usbdInterfaceSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,0,0);
	if(status != OK){
		printf("set interface failed.\n");
		return ERROR;
	}else{
		printf("set interface OK.\n");
	}

	USBD_PIPE_HANDLE inPipeHandle=NULL;
	USBD_PIPE_HANDLE outPipeHandle=NULL;
	UINT16 endPointIndex=1;

	status = usbdPipeCreate(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,endPointIndex,pUsbttyDev->configuration,pUsbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_IN,pUsbttyDev->epMaxPacketSize,0,0,&inPipeHandle);
	if(status == OK){
		printf("create IN pipe for endpoint 1 OK!\n");
	}else{
		printf("create IN pipe for endpoint 1 failed;status is %d;\n",status);
	}
	pUsbttyDev->inPipeHandle = inPipeHandle;

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,endPointIndex,pUsbttyDev->configuration,pUsbttyDev->interface,USB_XFRTYPE_BULK,USB_DIR_OUT,pUsbttyDev->epMaxPacketSize,0,0,&outPipeHandle);
		if(status == OK){
			printf("create OUT pipe for endpoint 2 OK!\n");
		}else{
			printf("create OUT pipe for endpoint 2 failed;status is %d;\n",status);
		}
	pUsbttyDev->outPipeHandle= outPipeHandle;


	/*将设备添加到系统的设备列表当中*/
	if((status = iosDevAdd (&pUsbttyDev->usbttyDev,devname,usbttyDrvNum)) != OK){
		free(pUsbttyDev);
		printf("Unable to create ttyusb device.");
		return status;
	}
	return OK;
}




/*初始化USBTTY驱动*/
STATUS usrUsbttyDevInit(void)
{
//	int index = 0;

/*检查驱动是否已经安装,如果已经安装就正常退出*/
	if(usbttyDrvNum > 0){
		printf("USBTTY already initialized.\n");
		return OK;
	}

/*创建互斥的信号量*/
	SEM_ID usbttyMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	SEM_ID usbttyListMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	if(usbttyMutex == NULL || usbttyListMutex == NULL){
		printf("Resource Allocation Failure!\n");
//		goto ERROR_HANDLE;
	}
/*初始化链表*/
	lstInit(&usbttyList);


/*安装合适的程序到驱动列表,iosDrvInstall向I/O子系统注册驱动本身，此时这个驱动被保存在系统的驱动表当中，
 * 等待设备与其进行衔接，衔接的工作由xxxxDevCreate()函数来完成，在iosDevCreate()函数中调用iosDevAdd()
 * 函数添加设备时，需要指定设备对应的驱动程序的驱动号，就是xxxxDrvNum中存储的驱动号。从而将将设备和底层驱动进行衔接。
 * 当用户对该设备进行操作的时候，可以调用此处定义的底层驱动函数*/
	usbttyDrvNum = iosDrvInstall((FUNCPTR)NULL,(FUNCPTR)usbttyDevDelete, (FUNCPTR) usbttyDevOpen,
			(FUNCPTR) usbttyDevClose,(FUNCPTR) usbttyDevRead,(FUNCPTR) usbttyDevWrite,(FUNCPTR) usbttyDevIoctl);


/*检查是否为驱动安装空间*/
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		return ERROR;
//		goto ERROR_HANDLER;
	}

//	s=usbdInitialize();
//	printf("usbdInitialize returned %d\n",s);

	status =usbdClientRegister("/usbtty",&usb_ttyusbdClientHandle);
	if (status == OK){
		printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
	}else{
		printf("client register failed!\n");
		return  ERROR;
	}

	status =usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,&usbTestDevAttachCallback);
			if(status == OK){
				printf("usbdDynamicAttachRegister success!\n");
			}else{
				printf("usbdDynamicAttachRegister failed\n");
				return ERROR;
			}
	return OK;
}



/*写USBTTY设备*/
int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
	status = CreateTransfer(pUsbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}

/*打开USBTTY串行设备，在此处对设备进行设置*/
int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev, char *name, int flags,int mode)
{
	printf("OK here im in usbttyDevOpen!\n");
//	status = usbdConfigurationSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,1,);


//	pUsbttyDev->numOpened++;
//	sioIoctl(&pUsbttyDev->pSioChan,SIO_OPEN,NULL);
//	return ((int)pUsbttyDev);
	return OK;
}


/*关闭USBTTY串行设备*/
int usbttyDevClose(USB_TTY_DEV *pUsbttyDev)
{
	if(!(pUsbttyDev->numOpened)){
		sioIoctl(&pUsbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pUsbttyDev);
}


int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,/*读取的设备*/
		int request/*请求编码*/,
		void *arg/*参数*/)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:	/*	调用IO控制函数*/
		status = (sioIoctl(&pUsbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*设置缓冲区*/
	memset(buffer,0,nBytes);
	sioIoctl(&pUsbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
//		return pUsbttyDev->pSioChan->pDrvFuncs->pollInput(&pUsbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
	/*返回读取的字节数*/
	return bytesToBeRead;
}





/*为USBTTY删除道系统设备*/
STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev)
{
	if(usbttyDrvNum < 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


	/*从I/O系统删除设备*/
	iosDevDelete(&pUsbttyDev->usbttyDev);

	/*从列表删除*/
	//TODO

	/*设备释放内存*/
//	free(pUsbttyDev->pUsbttyNode);
	free(pUsbttyDev);
	return OK;
}


STATUS getAllConfiguration(void)
{
	UINT16 ActLen;
	pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
	UINT16 bfrLen = 38;
	pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
	pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

	status = usbdDescriptorGet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
	if(status == OK){
				printf("\nConfiguration:\n");
				printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
				printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
						pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
						pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
// actual length:32(返回的数据的总长度为32字节);     buffer length is:38(提供的空间为38个字节);   bLength:9(配置描述符所占的长度为9 );  DescriptorType:2(表示该描述符指向的是一个设备）;
// wTotalLength:32(表示该配置+接口+端点描述符的总长度)   bNumInterface:1(该配置下接口数为1个)；          bConfigurationValue:1(该配置的值为1);   iConfiguration：0(用于描述该配置的字符串描述符的索引)
// bmAttributes:0x80(D7为1表示总线供电，D6为0表示不是自供电，D5为0表示不支持远程唤醒，D4-D0保留);    MaxPower:50(电流大小，以2mA为单位，50表示100mA)
				pUsbttyDev->configuration      = 0;
				pUsbttyDev->configurationValue = pusb_ttyConfigDescr->configurationValue;
				pUsbttyDev->maxPower		   = pusb_ttyConfigDescr->maxPower;


		//打印Interface描述符
			printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
						pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
						pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
// Length:9(接口描述符的总长度为9);  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
// numEndpoint:2(该接口所支持的端点数，不包括0端点)  interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；
// interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)
				pUsbttyDev->altSetting = pusb_ttyInterfaceDescr->alternateSetting;
				pUsbttyDev->interface  = 0;


		//打印EndPoint 1 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
						pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点1为输入端点
				pUsbttyDev->inEndpointAddress = pusb_ttyEndpointDescr1->endpointAddress ;
				pUsbttyDev->epMaxPacketSize   = pusb_ttyEndpointDescr1->maxPacketSize;



		//打印EndPoint 2 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
						pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点2为输出端点

				pUsbttyDev->outEndpointAddress = pusb_ttyEndpointDescr2->endpointAddress;

				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
	}else{
				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
			}

	return status;
}


//===========================================2017.05.26 19:25 END===============================================




//===========================================2017.05.26 BEGIN===============================================
/*
 * usbhotprobe.c
 *
 *  Created on: 2017-5-5
 *      Author: sfh
 */

/*
#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>
*/

#include <iosLib.h>
#include "usbhotprobe.h"

#define 	__DEBUG__
#ifdef 		__DEBUG__
	#include <D:\LambdaPRO615\target\deltaos\include\debug\utils\stdarg.h>
	void DEBUG(const char *fmt,...)
	{
		va_list ap;
//		va_list (ap,fmt);
		vprintf(fmt,ap);
		va_end(ap);
	}
#else
	void DEBUG(const char *fmt,...){}
#endif

#define usbttyDevNamePrefix "/usbtty"
#define DEBUG_DESCRIPTOR
#define DEBUG_PIPE
#define ERROR_HANDLER

STATUS status;
USB_TTY_DEV *pUsbttyDev = NULL;
LOCAL int 					usbttyDrvNum = -1; 	//驱动的数目
LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle = NULL;
LOCAL LIST					usbttyList;			//系统所有的USBTTY设备

void getAllConfiguration(void);
LOCAL int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,  char *buffer,  UINT32 nBytes);
LOCAL int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes);
LOCAL int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev,  char *name,  int flags,int mode);
LOCAL int usbttyDevClose(USB_TTY_DEV *pUsbttyDev);
LOCAL int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,  int request,  void *arg);
LOCAL STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev);
LOCAL STATUS usbttyDevCreate(char *name,USBD_NODE_ID nodeid,UINT16 configuration,UINT16 interface,UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol);


LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,
	UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{

	if(attachAction == USBD_DYNA_ATTACH){
		printf("device detected success!\n");
		DEBUG("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);
//callback nodeid is : 0x103(地址);configuration is: 0;   interface is 0;

/*创建UBSTTY设备*/
		if(usbttyDevCreate(usbttyDevNamePrefix,nodeid,configuration,interface,deviceClass,deviceSubClass,deviceProtocol)== OK){
			printf("create /USBTTY/0 device OK!\n");
		}else{
			printf("create /USBTTY/0 device failed\n");
		}
//		ttyDevCreate();



#ifdef DEBUG_DESCRIPTOR
		getAllConfiguration()
#endif



//	usbdInterfaceSet(usb_ttyusbdClientHandle,nodeid,0,0);





	}

	if(attachAction == USBD_DYNA_REMOVE){
		if(usbttyDevDelete(pUsbttyDev)!= OK){
			printf("delete device failed!\n");
		}else{
			printf("delete device success!\n");
		}
		printf("device remove!");
		return ;
	}


}












/*初始化USBTTY驱动*/
STATUS usrUsbttyDevInit(void)
{
	STATUS s = ERROR;
//	int index = 0;

/*检查驱动是否已经安装,如果已经安装就正常退出*/
	if(usbttyDrvNum > 0){
		printf("USBTTY already initialized.\n");
		return OK;
	}

/*创建互斥的信号量*/
	SEM_ID usbttyMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	SEM_ID usbttyListMutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	if(usbttyMutex == NULL || usbttyListMutex == NULL){
		printf("Resource Allocation Failure!\n");
//		goto ERROR_HANDLE;
	}
/*初始化链表*/
	lstInit(&usbttyList);


/*安装合适的程序到驱动列表,iosDrvInstall向I/O子系统注册驱动本身，此时这个驱动被保存在系统的驱动表当中，
 * 等待设备与其进行衔接，衔接的工作由xxxxDevCreate()函数来完成，在iosDevCreate()函数中调用iosDevAdd()
 * 函数添加设备时，需要指定设备对应的驱动程序的驱动号，就是xxxxDrvNum中存储的驱动号。从而将将设备和底层驱动进行衔接。
 * 当用户对该设备进行操作的时候，可以调用此处定义的底层驱动函数*/
	usbttyDrvNum = iosDrvInstall((FUNCPTR)NULL,(FUNCPTR)usbttyDevDelete, (FUNCPTR) usbttyDevOpen,
			(FUNCPTR) usbttyDevClose,(FUNCPTR) usbttyDevRead,(FUNCPTR) usbttyDevWrite,(FUNCPTR) usbttyDevIoctl);


/*检查是否为驱动安装空间*/
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		return ERROR;
//		goto ERROR_HANDLER;
	}

//	s=usbdInitialize();
//	printf("usbdInitialize returned %d\n",s);

	s=usbdClientRegister("/usbtty",&usb_ttyusbdClientHandle);
	if (s == OK){
		printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
	}else{
		printf("client register failed!\n");
		return  ERROR;
	}

	s=usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,&usbTestDevAttachCallback);
			if(s == OK){
				printf("usbdDynamicAttachRegister success!\n");
			}else{
				printf("usbdDynamicAttachRegister failed\n");
			}
//	}else{
//		printf("Initialized failed!\n");
//	}
	return OK;

/*ERROR_HANDLER:
	if(usbttyDrvNum){
		iosDrvRemove(usbttyDrvNum,1);
		usbttyDrvNum = 0;
	}

	if(usbttyMutex != NULL){
		semDelete(usbttyMutex);
	}

	if(usbttyListMutex != NULL)
		semDelete(usbttyListMutex);

	usbttyMutex  	= NULL;
	usbttyListMutex = NULL;
	return ERROR;*/

}

void transferCallback(pVOID pIrp)
{
	printf("I'm in tranferCallback now\n");
	return ;
}


STATUS CreateTransfer(USBD_PIPE_HANDLE Pipe)
{

	char p[20]= "hello world.";
	STATUS s;
	USB_IRP Irp;

	Irp.bfrList[0].bfrLen = 64;
	Irp.bfrList[0].pBfr = (pUINT8)&p;
	Irp.bfrList[0].pid = USB_PID_OUT;
	Irp.bfrList[0].actLen = 13;


	Irp.flags = USB_FLAG_SHORT_OK;
	Irp.startFrame = 0;
	Irp.dataBlockSize = 0;
	Irp.timeout = USB_TIMEOUT_DEFAULT;
	Irp.userCallback = transferCallback;
	Irp.irpLen =sizeof(USB_IRP);
	Irp.bfrCount = 1;

	s = usbdTransfer(usb_ttyusbdClientHandle,Pipe,&Irp);
	if(s != OK){
		printf("transfer data failed!\n");
	}else{
		printf("transfer data OK\n");
	}

	return s;

}







/*创建USBTTY系统设备，底层驱动xxxxDevCreate进行设备创建的时候，在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS usbttyDevCreate(char *name,USBD_NODE_ID nodeid,UINT16 configuration,UINT16 interface,UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
//LOCAL STATUS usbttyDevCreate(char *name	/*,SIO_CHAN *pSioChan*/,)
{
	STATUS s = ERROR;
	char devname[256];
	sprintf(devname,"%s/%d",usbttyDevNamePrefix,0);
//	USB_TTY_DEV usbttyDev; 是不是不能这样直接声明为一个变量，因为这个结构是要在整个程序的运行过程中保存的，
//而定义为变量的话出了这个函数这个结构就被销毁了。
	pUsbttyDev = (USB_TTY_DEV *)malloc(sizeof(USB_TTY_DEV));//指向USB设备
	if(pUsbttyDev == NULL){
		return ERROR;
	}
	memset(pUsbttyDev,0,sizeof(USB_TTY_DEV));
//申请了这个结构体之后就对其进行初始化，初始化完成以后再使用isoDevAdd()将其注册
	//TODO  初始化此结构体。


	s = usbdInterfaceSet(usb_ttyusbdClientHandle,nodeid,0,0);

#ifdef DEBUG_PIPE
	USBD_PIPE_HANDLE PipeHandle1=NULL;
	USBD_PIPE_HANDLE PipeHandle2=NULL;
//	USBD_PIPE_HANDLE PipeHandle3=NULL;
	UINT16 endPointIndex=1;
//	UINT16 configuration=0;
//	UINT16 interface=0;
	UINT16 maxPayload=0x40;

	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle1);
	if(status == OK){
		printf("create IN pipe for endpoint 1 OK!\n");
	}else{
		printf("create IN pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_OUT,maxPayload,0,0,&PipeHandle2);
		if(status == OK){
			printf("create OUT pipe for endpoint 2 OK!\n");
		}else{
			printf("create OUT pipe for endpoint 2 failed;status is %d;\n",status);
		}

	/*endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle3);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
		}*/
#endif



	pUsbttyDev->altSetting = 0;
	pUsbttyDev->configuration = configuration;
	pUsbttyDev->inEndpointAddress = 0x81;
	pUsbttyDev->inPipeHandle = PipeHandle1;
	pUsbttyDev->outPipeHandle = PipeHandle2;
	pUsbttyDev->interface = interface;
	pUsbttyDev->outEndpointAddress =0x1 ;
	pUsbttyDev->usbttyNodeid = nodeid;

	/*为节点分配内存并输入数据*/
/*	USB_TTY_NODE *pUsbttyNode = (USB_TTY_NODE *)calloc(1,sizeof(USB_TTY_NODE));//指向设备的节点
	if(pUsbttyNode == NULL){
		printf("calloc returned NULL - out of memory\n");
		return ERROR;
	}
	pUsbttyNode->pUsbttyDev = pUsbttyDev;
	pUsbttyDev->pUsbttyNode = pUsbttyNode;*/




	/*将设备添加到系统的设备列表当中*/
	if((s = iosDevAdd (&pUsbttyDev->usbttyDev,devname,usbttyDrvNum)) != OK){
		free(pUsbttyDev);
//		free(pUsbttyNode);
		printf("Unable to create ttyusb device.");
		return s;
	}
	return OK;
}




/*打开USBTTY串行设备，在此处对设备进行设置*/
LOCAL int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev, char *name, int flags,int mode)
{
	printf("OK here im in usbttyDevOpen!\n");
//	status = usbdConfigurationSet(usb_ttyusbdClientHandle,pUsbttyDev->usbttyNodeid,1,);


//	pUsbttyDev->numOpened++;
//	sioIoctl(&pUsbttyDev->pSioChan,SIO_OPEN,NULL);
//	return ((int)pUsbttyDev);
	return OK;
}


/*关闭USBTTY串行设备*/
LOCAL int usbttyDevClose(USB_TTY_DEV *pUsbttyDev)
{
	if(!(pUsbttyDev->numOpened)){
		sioIoctl(&pUsbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pUsbttyDev);
}


LOCAL int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,/*读取的设备*/
		int request/*请求编码*/,
		void *arg/*参数*/)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:	/*	调用IO控制函数*/
		status = (sioIoctl(&pUsbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
LOCAL int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*设置缓冲区*/
	memset(buffer,0,nBytes);
	sioIoctl(&pUsbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
//		return pUsbttyDev->pSioChan->pDrvFuncs->pollInput(&pUsbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
	/*返回读取的字节数*/
	return bytesToBeRead;
}


/*写USBTTY设备*/
LOCAL int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
	STATUS s;
	s = CreateTransfer(pUsbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}


/*为USBTTY删除道系统设备*/
LOCAL STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev)
{
	if(usbttyDrvNum < 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


	/*从I/O系统删除设备*/
	iosDevDelete(&pUsbttyDev->usbttyDev);

	/*从列表删除*/
	//TODO

	/*设备释放内存*/
//	free(pUsbttyDev->pUsbttyNode);
	free(pUsbttyDev);
	return OK;
}




void getAllConfiguration(void)
{
	UINT16 ActLen;
	pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
	UINT16 bfrLen = 38;
	pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
	pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
	pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

	status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
	if(status == OK){
				printf("\nConfiguration:\n");
				printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
				printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
						pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
						pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
// actual length:32(返回的数据的总长度为32字节);     buffer length is:38(提供的空间为38个字节);   bLength:9(配置描述符所占的长度为9 );  DescriptorType:2(表示该描述符指向的是一个设备）;
// wTotalLength:32(表示该配置+接口+端点描述符的总长度)   bNumInterface:1(该配置下接口数为1个)；          bConfigurationValue:1(该配置的值为1);   iConfiguration：0(用于描述该配置的字符串描述符的索引)
// bmAttributes:0x80(D7为1表示总线供电，D6为0表示不是自供电，D5为0表示不支持远程唤醒，D4-D0保留);    MaxPower:50(电流大小，以2mA为单位，50表示100mA)



		//打印Interface描述符
				printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
						pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
						pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
// Length:9(接口描述符的总长度为9);  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
// numEndpoint:2(该接口所支持的端点数，不包括0端点)  interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；
// interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)


		//打印EndPoint 1 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
						pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点1为输入端点
		//打印EndPoint 2 的描述符
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
						pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)
//端点2为输出端点

				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
	}else{
				printf("get configuration descriptor failed!\n");
				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
			}


}



//===========================================2017.05.26 END=================================================








//===========================================2017.05.19 BEGIN===============================================
#include "usbhotprobe.h"
LOCAL int usbttyDrvNum = 0; /*驱动的数目*/

LOCAL USBD_CLIENT_HANDLE usb_ttyusbdClientHandle = NULL;

LOCAL int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,  char *buffer,  UINT32 nBytes);
LOCAL int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes);
LOCAL int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev,  char *name,  int flags,int mode);
LOCAL int usbttyDevClose(USB_TTY_DEV *pUsbttyDev);
LOCAL int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,  int request,  void *arg);
LOCAL STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev);

LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,
	UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{

	STATUS status;
	if(attachAction == USBD_DYNA_ATTACH){
		printf("device detected success!\n");
		printf("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);	//0x103;0;0

//获取USB的主机控制器的总个数，每个主机控制器都有一个root hub,获得了主机控制器的总个数之后可以获得每一个root hub的nodeid。
		UINT16 busCount = 0;
		status = usbdBusCountGet(usb_ttyusbdClientHandle,&busCount);
		if(status == OK){
			printf("get bus count OK; the total number is:%d\n",busCount);//7
		}else{
			printf("get bus count failed!\n");
		}


//利用之前获得的host controller的总个数来获得root nodeid
		USBD_NODE_ID rootId;
		int i=0;
		while(i < busCount){
			status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,busCount-1,&rootId);
			if(status == OK){
//					printf("get rootId of root %d OK!the id is:%p\n",i,rootId);
			}else{
				printf("get rootId of root %d failed\n",i);
			}
			i++;
		}


//获取设备的node info
		USBD_NODE_INFO nodeInfo;
		UINT16 infoLen = 8;
		status = usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,&nodeInfo,infoLen);
		if(status == OK){
			printf("\nnode info get OK! nodeType:%d;  nodeSpeed:%d;\nparentHubId:%d;  parentHubport:%d;  rootId:%d;\n",nodeInfo.nodeType,nodeInfo.nodeSpeed,nodeInfo.parentHubId,nodeInfo.parentHubPort,nodeInfo.rootId);
/*nodeType:2(表示是设备),nodeapeed:0(表示全速)，parentHubId:257;parentHubPort:0;rootId:257*/
		}else{
			printf("get node information failed!\n");
		}

//获得设备的nodeid NodeType's type code as:USB_NODETYPE_NONE 0 / USB_NODETYPE_HUB 1 / USB_NODETYPE_DEVICE 2
//第2、3个参数使用上面获得的。
		UINT16 NodeType = -1;
		USBD_NODE_ID nodeId;
		status = usbdNodeIdGet(usb_ttyusbdClientHandle,nodeInfo.parentHubId,nodeInfo.parentHubPort,&NodeType,&nodeId);
		if(status == OK){
			printf("\nnodeType:%d;  nodeId:%p\n",NodeType,nodeId);/*node type:2(表示设备);nodId:0x102*/
		}else{
			printf("get nodeid failed\n");
		}

//获得设备的配置，该值可以在设备的configuration描述符中得到。
//		UINT16 devConfig;
//		status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&devConfig);
//		if(status == OK){
//			printf("\nthe devConfig value is:%d\n",devConfig); //值为1
//		}else{
//			printf("get device configuration failed!\n");
//		}

//获得接口的alternate setting。
		int interfaceIndex = 0;
		UINT16 alternateSetting;
		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
		if(status == OK){
			printf("\nget interface 0 alternate setting is:%d\n",alternateSetting);
		}else{
			printf("get interface 0 alternate setting failed!\n");
		}


/*		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("\nDevice Descriptor:\n");
			printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}*/


		UINT16 ActLen;
		pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
		UINT16 bfrLen = 32;
		pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
		pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
		if(status == OK){
					printf("\nConfiguration:\n");
					printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
					printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
							pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
							pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
//bLength:9;  DescriptorType:2(表示指向的是设备) bNumInterface:1(该配置下接口数为1)； bConfigurationValue:1(该配置的值为1);



			//打印Interface描述符
					printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
							pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
							pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
//Length:9;  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
//interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；  interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)


			//打印EndPoint 1 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
							pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)

			//打印EndPoint 2 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
							pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)


					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
		}else{
					printf("get configuration descriptor failed!\n");
					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
				}


/*		USB_CONFIG_DESCR usb_ttyConfigDescr;s
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("actual length is:%d;  buffer ength is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);*/
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
/*		}else{
			printf("get configuration descriptor failed!\n");
		}*/

//

	USBD_PIPE_HANDLE PipeHandle1=NULL;
	USBD_PIPE_HANDLE PipeHandle2=NULL;
	USBD_PIPE_HANDLE PipeHandle3=NULL;
	UINT16 endPointIndex=1;
	UINT16 configuration=0;
	UINT16 interface=0;
	UINT16 maxPayload=0x40;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_OUT,maxPayload,0,0,&PipeHandle1);
	if(status == OK){
		printf("create OUT pipe for endpoint 1 OK!\n");
	}else{
		printf("create OUT pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle2);
		if(status == OK){
			printf("create IN pipe for endpoint 2 OK!\n");
		}else{
			printf("create IN pipe for endpoint 2 failed;status is %d;\n",status);
		}

	endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle3);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
		}


/*	STATUS usbdStatusGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,UINT16 requestType,UINT16 index,UINT16 bfrLen,pUINT8 pBfr,pUINT16 pActLen)
 * this function retrieves the current status from the device indicated by nodeId. requestType indicates the nature of the desired status
 * The status word is returned in pBfr. The meaning of the status varies depending on whether it was queried
 * from the device, an interface, or an endpoint, class-specific function, etc. as described in the USB Specification.
 */
//	UINT16 StatusIndex = 0;  //StatusIndex的值该从哪里获得？
//	UINT16 bfrStatusLen = 2;
//	pUINT8 pStatusBfr;
//	pStatusBfr = (pUINT8)malloc(sizeof(bfrStatusLen));
//	pUINT16 pActStatusLen;
//	status = usbdStatusGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,StatusIndex,bfrStatusLen,pStatusBfr,pActStatusLen);
//	if(status == OK){
//		printf("get status OK\n buffer length is :%d\nactual length is:%d\n",bfrStatusLen,*pActStatusLen);
//		printf("the first byte of Buffer is 0x%x,the second byte of the buffer is 0x%x",*pStatusBfr,*(pStatusBfr+1));
////buffer length is 2,the actual length is 2
////the content of the buffer is 0
//	}else{
//		printf("get status failed!\n");
//	}


	}
	if(attachAction == USBD_DYNA_REMOVE){
		printf("device remove!");
	}
}


/*初始化USBTTY驱动*/
STATUS usrUsbttyDevInit(void)
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;

/*检查驱动是否已经安装*/
	if(usbttyDrvNum > 0){
		printf("USBTTY already initialized.\n");
	}

/*安装合适的程序到驱动列表*/
	usbttyDrvNum = iosDrvInstall((FUNCPTR)NULL,(FUNCPTR)usbttyDevDelete, (FUNCPTR) usbttyDevOpen,
			(FUNCPTR) usbttyDevClose,(FUNCPTR) usbttyDevRead,(FUNCPTR) usbttyDevWrite,(FUNCPTR) usbttyDevIoctl);

/*检查是否为驱动安装空间*/
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		goto ERROR_HANDLER;
	}






//	s=usbdInitialize();
//	printf("usbdInitialize returned %d\n",s);
//	if (s == OK){
		s=usbdClientRegister("usb/tty",&usb_ttyusbdClientHandle);
//		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
//				printf("usbd initilized OK!\n");
			}
		}


		s=usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback);
				if(s == OK){
					printf("usbdDynamicAttachRegister success!\n");
				}else{
					printf("usbdDynamicAttachRegister failed\n");
				}
//	}else{
//		printf("Initialized failed!\n");
//	}
	return OK;

ERROR_HANDLER:
	if(usbttyDrvNum){
		iosDrvRemove(usbttyDrvNum,1);
		usbttyDrvNum = 0;
	}


	return ERROR;
}



/*打开USBTTY串行设备*/
LOCAL int usbttyDevOpen(USB_TTY_DEV *pUsbttyDev, char *name, int flags,int mode)
{

	return OK;
}


/*关闭USBTTY串行设备*/
LOCAL int usbttyDevClose(USB_TTY_DEV *pUsbttyDev)
{
	if(!(pUsbttyDev->numOpen)){
		sioIoctl(pUsbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pUsbttyDev);
}


LOCAL int usbttyDevIoctl(USB_TTY_DEV *pUsbttyDev,/*读取的设备*/
		int request/*请求编码*/,
		void *arg/*参数*/)
{
	int status = OK;
	switch(request){
	case FIOSELECT:
		//TODO
		break;

	case FIOUNSELECT:
		//TODO
		break;

	case FIONREAD:
		//TODO
		break;

	default:	/*	调用IO控制函数*/
		status = (sioIoctl(pUsbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*读取数据*/
LOCAL int usbttyDevRead(USB_TTY_DEV *pUsbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*设置缓冲区*/
	memset(buffer,0,nBytes);
	sioIoctl(pUsbttyDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
		return pUsbttyDev->pSioChan->pDrvFuncs->pollInput(pUsbttyDev->pSioChan,buffer);
	}else if(arg == SIO_MODE_INT){
		//TODO

		return ERROR;
	}else{
		logMsg("Unsupported Mode\n",0,0,0,0,0,0);
		return ERROR;
	}
	/*返回读取的字节数*/
	return bytesToBeRead;
}


/*写USBTTY设备*/
LOCAL int usbttyDevWrite(USB_TTY_DEV *pUsbttyDev, char *buffer,UINT32 nBytes)
{
	int nByteWrited = 0;
	//TODO
	return nByteWrited;
}


/*为USBTTY删除道系统设备*/
LOCAL STATUS usbttyDevDelete(USB_TTY_DEV *pUsbttyDev)
{
	if(usbttyDrvNum < 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


	/*从I/O系统删除设备*/
	iosDevDelete(&pUsbttyDev->ioDev);

	/*从列表删除*/
	//TODO

	/*设备释放内存*/
	free(pUsbttyDev->pUsbttyNode);
	free(pUsbttyDev);
	return OK;
}
//===========================================2017.05.19 END=================================================




//===========================================2017.05.18 BEGIN===============================================
#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
//#define USB_TEST_CLASS USBD_NOTIFY_ALL
//#define USB_TEST_SUB_CLASS USBD_NOTIFY_ALL
//#define USB_TEST_DRIVE_PROTOCOL USBD_NOTIFY_ALL
#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 	//256

LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle=NULL;

LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,
	UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{
	printf("device detected success!\n");
	STATUS status;
	if(attachAction == USBD_DYNA_ATTACH){
		printf("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);	//0x103;0;0

//获取USB的主机控制器的总个数，每个主机控制器都有一个root hub,获得了主机控制器的总个数之后可以获得每一个root hub的nodeid。
		UINT16 busCount = 0;
		status = usbdBusCountGet(usb_ttyusbdClientHandle,&busCount);
		if(status == OK){
			printf("get bus count OK; the total number is:%d\n",busCount);//7
		}else{
			printf("get bus count failed!\n");
		}


//利用之前获得的host controller的总个数来获得root nodeid
		USBD_NODE_ID rootId;
		int i=0;
		while(i < busCount){
			status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,busCount-1,&rootId);
			if(status == OK){
//					printf("get rootId of root %d OK!the id is:%p\n",i,rootId);
			}else{
				printf("get rootId of root %d failed\n",i);
			}
			i++;
		}


//获取设备的node info
		USBD_NODE_INFO nodeInfo;
		UINT16 infoLen = 8;
		status = usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,&nodeInfo,infoLen);
		if(status == OK){
			printf("\nnode info get OK! nodeType:%d;  nodeSpeed:%d;\nparentHubId:%d;  parentHubport:%d;  rootId:%d;\n",nodeInfo.nodeType,nodeInfo.nodeSpeed,nodeInfo.parentHubId,nodeInfo.parentHubPort,nodeInfo.rootId);
/*nodeType:2(表示是设备),nodeapeed:0(表示全速)，parentHubId:257;parentHubPort:0;rootId:257*/
		}else{
			printf("get node information failed!\n");
		}

//获得设备的nodeid NodeType's type code as:USB_NODETYPE_NONE 0 / USB_NODETYPE_HUB 1 / USB_NODETYPE_DEVICE 2
//第2、3个参数使用上面获得的。
		UINT16 NodeType = -1;
		USBD_NODE_ID nodeId;
		status = usbdNodeIdGet(usb_ttyusbdClientHandle,nodeInfo.parentHubId,nodeInfo.parentHubPort,&NodeType,&nodeId);
		if(status == OK){
			printf("\nnodeType:%d;  nodeId:%p\n",NodeType,nodeId);/*node type:2(表示设备);nodId:0x102*/
		}else{
			printf("get nodeid failed\n");
		}

//获得设备的配置，该值可以在设备的configuration描述符中得到。
//		UINT16 devConfig;
//		status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&devConfig);
//		if(status == OK){
//			printf("\nthe devConfig value is:%d\n",devConfig); //值为1
//		}else{
//			printf("get device configuration failed!\n");
//		}

//获得接口的alternate setting。
		int interfaceIndex = 0;
		UINT16 alternateSetting;
		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
		if(status == OK){
			printf("\nget interface 2 alternate setting is:%d\n",alternateSetting);
		}else{
			printf("get interface 2 alternate setting failed!\n");
		}



/*		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("\nDevice Descriptor:\n");
			printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}*/


		UINT16 ActLen;
		pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
		UINT16 bfrLen = 35;
		pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
		pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
		if(status == OK){
					printf("\nConfiguration:\n");
					printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
					printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
							pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
							pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
//bLength:9;  DescriptorType:2(表示指向的是设备) bNumInterface:1(该配置下接口数为1)； bConfigurationValue:1(该配置的值为1);



			//打印Interface描述符
					printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
							pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
							pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
//Length:9;  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
//interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；  interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)


			//打印EndPoint 1 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
							pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)

			//打印EndPoint 2 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
							pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)


					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
		}else{
					printf("get configuration descriptor failed!\n");
					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
				}


/*		USB_CONFIG_DESCR usb_ttyConfigDescr;s
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("actual length is:%d;  buffer ength is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);*/
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
/*		}else{
			printf("get configuration descriptor failed!\n");
		}*/




//
	USBD_PIPE_HANDLE PipeHandle1=NULL;
	USBD_PIPE_HANDLE PipeHandle2=NULL;
	USBD_PIPE_HANDLE PipeHandle3=NULL;
	UINT16 endPointIndex=1;
	UINT16 configuration=0;
	UINT16 interface=0;
	UINT16 maxPayload=0x40;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_OUT,maxPayload,0,0,&PipeHandle1);
	if(status == OK){
		printf("create OUT pipe for endpoint 1 OK!\n");
	}else{
		printf("create OUT pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle2);
		if(status == OK){
			printf("create IN pipe for endpoint 2 OK!\n");
		}else{
			printf("create IN pipe for endpoint 2 failed;status is %d;\n",status);
		}

	endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle3);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
		}




/*	STATUS usbdStatusGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,UINT16 requestType,UINT16 index,UINT16 bfrLen,pUINT8 pBfr,pUINT16 pActLen)
 * this function retrieves the current status from the device indicated by nodeId. requestType indicates the nature of the desired status
 * The status word is returned in pBfr. The meaning of the status varies depending on whether it was queried
 * from the device, an interface, or an endpoint, class-specific function, etc. as described in the USB Specification.
 */
//	UINT16 StatusIndex = 0;  //StatusIndex的值该从哪里获得？
//	UINT16 bfrStatusLen = 2;
//	pUINT8 pStatusBfr;
//	pStatusBfr = (pUINT8)malloc(sizeof(bfrStatusLen));
//	pUINT16 pActStatusLen;
//	status = usbdStatusGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,StatusIndex,bfrStatusLen,pStatusBfr,pActStatusLen);
//	if(status == OK){
//		printf("get status OK\n buffer length is :%d\nactual length is:%d\n",bfrStatusLen,*pActStatusLen);
//		printf("the first byte of Buffer is 0x%x,the second byte of the buffer is 0x%x",*pStatusBfr,*(pStatusBfr+1));
////buffer length is 2,the actual length is 2
////the content of the buffer is 0
//	}else{
//		printf("get status failed!\n");
//	}





	}
	if(attachAction == USBD_DYNA_REMOVE){
		printf("device remove!");
	}
}


UINT16 usbInit()/*Initialize USBD*/
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;
	s=usbdInitialize();
	printf("usbdInitialize returned %d\n",s);
	if (s == OK){

		s=usbdClientRegister("usb/tty",&usb_ttyusbdClientHandle);
//		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
				printf("usbd initilized OK!\n");
			}
		}


		s=usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback);
				if(s == OK){
					printf("usbdDynamicAttachRegister success!\n");
				}else{
					printf("usbdDynamicAttachRegister failed\n");
				}
	}else{
		printf("Initialized failed!\n");
	}
	return OK;
}

//===========================================2017.05.18 END=================================================














//==========================================2017.05.17 BEGIN=================================================
#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 	//256

LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle=NULL;

LOCAL void usbTestDevAttachCallback(USBD_NODE_ID nodeid,UINT16 attachAction,UINT16 configuration,UINT16 interface,
	UINT16 deviceClass,UINT16 deviceSubClass,UINT16 deviceProtocol)
{
	printf("device detected success!\n");
	STATUS status;
	if(attachAction == USBD_DYNA_ATTACH){
		printf("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);	//0x103;0;0

//获取USB的主机控制器的总个数，每个主机控制器都有一个root hub,获得了主机控制器的总个数之后可以获得每一个root hub的nodeid。
		UINT16 busCount = 0;
		status = usbdBusCountGet(usb_ttyusbdClientHandle,&busCount);
		if(status == OK){
			printf("get bus count OK; the total number is:%d\n",busCount);//7
		}else{
			printf("get bus count failed!\n");
		}


//利用之前获得的host controller的总个数来获得root nodeid
		USBD_NODE_ID rootId;
		int i=0;
		while(i < busCount){
			status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,busCount-1,&rootId);
			if(status == OK){
//					printf("get rootId of root %d OK!the id is:%p\n",i,rootId);
			}else{
				printf("get rootId of root %d failed\n",i);
			}
			i++;
		}


//获取设备的node info
		USBD_NODE_INFO nodeInfo;
		UINT16 infoLen = 8;
		status = usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,&nodeInfo,infoLen);
		if(status == OK){
			printf("\nnode info get OK! nodeType:%d;  nodeSpeed:%d;\nparentHubId:%d;  parentHubport:%d;  rootId:%d;\n",nodeInfo.nodeType,nodeInfo.nodeSpeed,nodeInfo.parentHubId,nodeInfo.parentHubPort,nodeInfo.rootId);
/*nodeType:2(表示是设备),nodeapeed:0(表示全速)，parentHubId:257;parentHubPort:0;rootId:257*/
		}else{
			printf("get node information failed!\n");
		}

//获得设备的nodeid NodeType's type code as:USB_NODETYPE_NONE 0 / USB_NODETYPE_HUB 1 / USB_NODETYPE_DEVICE 2
//第2、3个参数使用上面获得的。
		UINT16 NodeType = -1;
		USBD_NODE_ID nodeId;
		status = usbdNodeIdGet(usb_ttyusbdClientHandle,nodeInfo.parentHubId,nodeInfo.parentHubPort,&NodeType,&nodeId);
		if(status == OK){
			printf("\nnodeType:%d;  nodeId:%p\n",NodeType,nodeId);/*node type:2(表示设备);nodId:0x102*/
		}else{
			printf("get nodeid failed\n");
		}

//获得设备的配置
		UINT16 devConfig;
		status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&devConfig);
		if(status == OK){
			printf("\nthe devConfig value is:%d\n",devConfig);
		}else{
			printf("get device configuration failed!\n");
		}

//获得接口的alternate setting。
		int interfaceIndex = 0;
		UINT16 alternateSetting;
		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
		if(status == OK){
			printf("\nget interface 0 alternate setting is:%d\n",alternateSetting);
		}else{
			printf("get interface 0 alternate setting failed!\n");
		}
//将接口index设置为1无效
//		interfaceIndex = 1;
//		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
//		if(status == OK){
//			printf("get interface 1 alternate setting is:%d\n",alternateSetting);
//		}else{
//			printf("get interface 1 alternate setting failed!\n");
//		}






/*		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("\nDevice Descriptor:\n");
			printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}*/


		UINT16 ActLen;
		pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
		UINT16 bfrLen = 35;
		pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
		pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
		if(status == OK){
					printf("\nConfiguration:\n");
					printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
					printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
							pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
							pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
			//打印Interface描述符
					printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
							pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
							pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);

			//打印EndPoint 1 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:%d;\n  attributes:%d;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
							pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);

			//打印EndPoint 2 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:%d;\n  attributes:%d;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
							pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);

		}else{
					printf("get configuration descriptor failed!\n");
				}


/*		USB_CONFIG_DESCR usb_ttyConfigDescr;s
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("actual length is:%d;  buffer ength is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);*/
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
/*		}else{
			printf("get configuration descriptor failed!\n");
		}*/




//
	pUSBD_PIPE_HANDLE pPipeHandle;
	UINT16 endPointIndex=1;
	UINT16 configuration=0;
	UINT16 interface=0;
	UINT16 maxPayload=0x40;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_OUT,maxPayload,0,0,pPipeHandle);
	if(status == OK){
		printf("create OUT pipe for endpoint 1 OK!\n");
	}else{
		printf("create OUT pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,pPipeHandle);
		if(status == OK){
			printf("create IN pipe for endpoint 2 OK!\n");
		}else{
			printf("create IN pipe for endpoint 2 failed;status is %d;\n",status);
		}

	endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,pPipeHandle);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
		}




/*	STATUS usbdStatusGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,UINT16 requestType,UINT16 index,UINT16 bfrLen,pUINT8 pBfr,pUINT16 pActLen)
 * this function retrieves the current status from the device indicated by nodeId. requestType indicates the nature of the desired status
 * The status word is returned in pBfr. The meaning of the status varies depending on whether it was queried
 * from the device, an interface, or an endpoint, class-specific function, etc. as described in the USB Specification.
 */
//	UINT16 StatusIndex = 0;  //StatusIndex的值该从哪里获得？
//	UINT16 bfrStatusLen = 2;
//	pUINT8 pStatusBfr;
//	pStatusBfr = (pUINT8)malloc(sizeof(bfrStatusLen));
//	pUINT16 pActStatusLen;
//	status = usbdStatusGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,StatusIndex,bfrStatusLen,pStatusBfr,pActStatusLen);
//	if(status == OK){
//		printf("get status OK\n buffer length is :%d\nactual length is:%d\n",bfrStatusLen,*pActStatusLen);
//		printf("the first byte of Buffer is 0x%x,the second byte of the buffer is 0x%x",*pStatusBfr,*(pStatusBfr+1));
////buffer length is 2,the actual length is 2
////the content of the buffer is 0
//	}else{
//		printf("get status failed!\n");
//	}





	}
	if(attachAction == USBD_DYNA_REMOVE){
		printf("device remove!");
	}
}


UINT16 usbInit()/*Initialize USBD*/
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;
	s=usbdInitialize();
	printf("usbdInitialize returned %d\n",s);
	if (s == OK){

		s=usbdClientRegister("usb/tty",&usb_ttyusbdClientHandle);
//		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
				printf("usbd initilized OK!\n");
			}
		}


		s=usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback);
				if(s == OK){
					printf("usbdDynamicAttachRegister success!\n");
				}else{
					printf("usbdDynamicAttachRegister failed\n");
				}
	}else{
		printf("Initialized failed!\n");
	}
	return OK;
}
//==========================================2017.05.17 END=================================================










/*
 * usbhotprobe.c
 *
 *  Created on: 2017-5-5
 *      Author: sfh
 */

#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <vxWorksCommon.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>

//#include <D:/LambdaPRO/target/deltaos/include/vxworks/usb/usbHst.h>
//#include <D:/LambdaPRO/target/deltaos/src/usr/h/ioLib.h>
//#include <D:/LambdaPRO/target/deltaos/include/vxworks/usb/usbHubInitialization.h>

//#define USB_TEST_CLASS USBD_NOTIFY_ALL
//#define USB_TEST_SUB_CLASS USBD_NOTIFY_ALL
//#define USB_TEST_DRIVE_PROTOCOL USBD_NOTIFY_ALL
#define USB_TEST_CLASS 0x10C4
#define USB_TEST_SUB_CLASS 0xEA60
#define USB_TEST_DRIVE_PROTOCOL 0x0100

LOCAL USBD_CLIENT_HANDLE usb_ttyusbdClientHandle=NULL;
LOCAL USBD_NODE_ID 	usb_ttyHubId;
LOCAL UINT16 		usb_ttyHubPort;
LOCAL USBD_NODE_ID 	usb_ttyrootId;

LOCAL void usbTestDevAttachCallback(
	USBD_NODE_ID nodeid,
	UINT16 attachAction,
	UINT16 configuration,
	UINT16 interface,
	UINT16 deviceClass,
	UINT16 deviceSubClass,
	UINT16 deviceProtocol
	)
{
	printf("device detected success!\n");
	UINT16 status;
	if(attachAction == USBD_DYNA_ATTACH){
		printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
		printf("the attachCallBack nodeid is:%p\n",nodeid);
		printf("the attachCallBack attachAction is:%d\n",attachAction);
		printf("the attachCallBack configuration is:%d\n",configuration);
		printf("the attachCallBack interface is:%d\n",interface);
		printf("the attachCallBack deviceClass is:%x\n",deviceClass);		//4292  0x10C4
		printf("the attachCallBack deviceSubClass is:%x\n",deviceSubClass);	//60000	0xEA60
		printf("the attachCallBack deviceProtocol is:%x\n",deviceProtocol);	//256	0x0100
/*
 * 		usbdNodeInfoGet() returns information about a USB node,this function retrieves information about the USB device specified by nodeId,
 * 	 the USBD copies node information into the pNodeInfo structure provided by the caller,this structure is of the form USBD_NODEINFO as shown below:
 * 	 		typedef struct usbd_nodeinfo{
 * 	 		UINT16 nodeType; UINT16 nodeSpeed;USBD_NODE_ID parentHubId; UINT16 parentHubPort;
 * 	 		USBD_NODE_ID rootId;
 * 	 		} USBD_NODEINFO, *pUSBD_NODEINFO;
 *
 * 	 		USB_NODETYPE_NONE	0;	USB_NODETYPE_HUB	1; 	USB_NODETYPE_DEVICE	2
 * 	 		USB_SPEED_FULL		0   * 12 mbit device *
 *		    USB_SPEED_LOW		1   * low speed device (1.5 mbit)*
 */
		pUSBD_NODE_INFO pNodeInfo;
		UINT16 infoLen;
		status=usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,pNodeInfo,infoLen);
		if(status == OK){
			printf("get node info successful!\n");
			printf("nodetype:0x%x\n",pNodeInfo->nodeType);
			printf("nodespeed:0x%x\n",pNodeInfo->nodeSpeed);
			printf("parentHubId is :%p\n",pNodeInfo->parentHubId);
			usb_ttyHubId=pNodeInfo->parentHubId;
			printf("parentHubPort is :%d\n",pNodeInfo->parentHubPort);
			usb_ttyHubPort = pNodeInfo->parentHubPort;
			printf("rootId is :%p\n",pNodeInfo->rootId);
			usb_ttyrootId = pNodeInfo->rootId;
			printf("Len of bfr allocated by client is :%d\n",infoLen);
		}else{
			printf("get node info failed!\n");
		}


		/* usbdBusCountGet() returned total number of usb host controllers in the system.each host controller has its own
		* root hub as required by usb specification;and clients planning to enumerate USB devices using
		* the Bus Enumeration Functions need to know the total number of the host controller in order to
		* retrieve the Node Ids for each root hub
		*  NOTE:the number of USB host controller is not constant,Bus controllers can be added by calling
		*  usbdHcdAttach() and removed by calling usbdHcdDetach(). **the Dynamic Attach Function deal with
		*  these situations automatically, and are preferred mechanism by which most clients should be
		*  informed of device attachment and removal
		*/
				pUINT16 pBusCount;
				status=usbdBusCountGet(usb_ttyusbdClientHandle, pBusCount);
				if( status == OK){
					printf("usbdCountGet() returned OK!,the pBusCount is %d\n",*pBusCount);
				}else{
					printf("usbdCountGet() Failed\n");
				}

		/*	usbdRootNodeIdGet() returns the Node ID for the root hub for the specified USB controller.busIndex is the index
		 * of the desired USB host controller.the first host Controller is index 0 and the last controller's index is the
		 * total number of USB host controllers - as returned by usbdBusCountGet()-minus 1.<pRootId>must be point to a
		 * USBD_NODE_ID variable  in which the node ID if the root hub will be stored.
		 */
//				pUSBD_NODE_ID pRootId;
//				status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,BusIndex, pRootId);
//				if( status == OK){
//					printf("usbdRootNodeGet success!,pRootId is:%p\n",pRootId);
//				}else{
//					printf("usbdRootNodeGet failed!\n");
//				}



		/*	usbdHubPortCountget()returns number of ports connected to a hub ,provides clients with a convenient mechanism to
		 * retrieve the number of downstream ports provided by the specified hub.Clients can also retrieve this information by
		 * retrieve configuration descriptors from the hub using the Configuration Functions describe in a following section
		 * 			STATUS usbdHubPortCountGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID hubId, pUINT16 pPortCOunt)
		 */
				pUINT16 pPortCount;
				status=usbdHubPortCountGet(usb_ttyusbdClientHandle,usb_ttyHubId,pPortCount);
				if(status == OK){
					printf("prot count is :%d\n",*pPortCount);
				}else{
					printf("PortCount get failed!\nq");
				}


		/*
		 *   usbdNodeIdGet() get the id of the node connected to a hub port,client use this function to retrieve the Node Id
		 *   for devices attach to each of a hub port.hubId and portIndex identify the hub to which a device may be attached.
		 *   pNodeType must point to a UINT16 variable to receive a type code.
		 *   	STATUS usbdNodeIdGet(
		 *   	USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID hubId,
		 *   	UINT16 portIndex, pUINT16 pNodeType,pUSBD_NODE_ID pNodeId)
		 */
				pUINT16 pNodeType;
				pUSBD_NODE_ID pNodeId;
				status=usbdNodeIdGet(usb_ttyusbdClientHandle,usb_ttyHubId,usb_ttyHubPort,pNodeType,pNodeId);
				if( status == OK){
					printf("get node id successful!\n");
					printf("node type is:%d\n",*pNodeType);
					printf("Node Id is :%p\n",*pNodeId);
				}else{
					printf("get node id failed!\n");
				}

/*
 * 		usbdInterfaceget() used to retrieve a device's current interface
 *   this function allows a client to query the current alternate setting for a given device's interface,nodeId and interfaceIndex specify the device and interface to
 *   be queried,respectively.pAlternateSetting points to a UINT16 variable in which the alternate setting will be stored upon return
 *   		STATUS usbdInterfaceget(
 *   		USBD_CLIENT_HANDLE clientHandle, USBD_NODE_ID nodeId,
 *   		UINT16 interfaceIndex,pUINT16 pAlternateSetting)
 */
//			pUINT16 pAlternateSetting;
//			status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,1,pAlternateSetting);
//			if(status == OK){
//				printf("usbdInterfaceGet OK returned point is :%d\n",*pAlternateSetting);
//			}else{
//				printf("usbdInterfaceGet failed!");
//			}

/*
 * 		usbdStatusGet() retrieves USB status from a device/interface/etc
 * 	this function retrieves the current status from the device indicated by nodeId.
 * 	requestType indicates the nature of desired status as documented for the usbdFeatureClear() function
 * 	the status word is returned in pBfr.the meaning of the status varies depending on whether it was queried from
 * 	the device,an interface,or an endpoint,class-specific function,etc.as described in the USB Specification.
 * 			STATUS = usbdStatusget(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,
 * 			UINT16 requestType, UINT16 index,UINT16 bfrLen, pUINT16 pBfr, pUINT16 pActLen)
 */




/*
 * 		usbdAddressGet() used to get USB address for a given device
 *
 */
			pUINT16 pDeviceAddress;
			status = usbdAddressGet(usb_ttyusbdClientHandle,nodeid,pDeviceAddress);
			if(status == OK){
				printf("usbdAddressGet OK ,the address is :%d\n",*pDeviceAddress);
			}else{
				printf("usbdAddressGet failed!\n");
			}

/*
 * 		usbd VendorSpecific() allows client to issue vendor-specific USB requests
 * 			STATUS usbdVendorSpecific(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,
 * 			UINT8 requestType, UINT8 request, UINT16 value, UINT16 index, UINT16 length,
 * 			pUINT8 pBfr, pUINT16 pActLen)
 */








	}
	if(attachAction == USBD_DYNA_REMOVE){
		printf("device remove!");
	}

}

UINT16 cmdUsbInit()/*Initialize USBD*/
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;
	s=usbdInitialize();
	printf("usbdInitialize returned %d\n",s);
	if (s == OK){
		s=usbdClientRegister("usb/tty",&usb_ttyusbdClientHandle);
		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
				printf("usbd initilized OK!\n");
			}
		}
/*
 * 	 usbdDynamicAttachRegister() used to registers client for dynamic attach notification
 *		Clients call this function to indicate to the USBD that they wish to be notified whenever a device of the indicated class/sub-class/protocol is attach
 *	or removed from the USB. A client may specify that it wants to receive notification for an entire device clsss or only for specific sub-classes within the calss
 *	deviceClass,deviceSubClass,and deviceProtocol must specify a USB class/subclass/protocol combination according to the USB specification.for the client convenience,
 *	usbdLib.h automatically includes usb.h which defines a numbers of USB device class as USB_CLASS_XXXX and USB_SUBCLASS_XXXX.A value of USBD_NOTIFY_ALL in any/all of these
 *	parameters acts like a wildcard and matches any value reported by the device for the corresponding filed.
 */
		s=usbdDynamicAttachRegister(usb_ttyusbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback);
				if(s == OK){
					printf("usbdDynamicAttachRegister success!\n");
				}else{
					printf("usbdDynamicAttachRegister failed\n");
				}
	}else{
		printf("Initialized failed!\n");
	}
	return OK;
}








































//
//int testUSBInit (void)
//{
//	 printf("hello USB test\n");

//	 if (usbInit() != OK){
//	   	printf("usbInit failed!\n" );
//	   	return ERROR;
//	   }else{
//	   	printf("usb init OK\n");
//	   }
//	   if (usbdInitialize() != OK){
//	   	printf("Failed to initialize USBD\n");
//	   	return ERROR;
//	   }else{
//	   	printf("USBD initialize success\n");
//	   	}

//	   //挂接过程
//	      	UINT8 busNo;
//	      	UINT8 deviceNo;
//	      	UINT8 funcNo;
//	      	PCI_CFG_HEADER pciCfgHdr;
//	      	GENERIC_HANDLE uhciAttachToken;
//	      	if (!usbPciClassFind(UHCI_CLASS,UHCI_SUBCLASS,UHCI_PGMIF,0,&busNo,&deviceNo,&funcNo)){
//	      		return ERROR;
//	      	}
//	      	printf("usbPciClassFind success!\n");
//
//	      	usbPciConfigHeaderGet(busNo,deviceNo,funcNo,&pciCfgHdr);
//
//	      	/* Attach the UHCI HCD to the USBD.The function usbHcdUhci() is exported by usbHcdUhciLib.*/
//	      	HCD_EXEC_FUNC usbHcdUhciExec;
//	      	if (usbdHcdAttach(usbHcdUhciExec,&pciCfgHdr,&uhciAttachToken) != OK){
//	      		//USBD failed to attach to UHCI HCD
//	      		return ERROR;
//	      	}
//	      	printf("usbHcdAttach OK!\n");

	   // STATUS usbdclientregister(pCHAR pClientName, pUSBD_CLIENT_HANDLE pClientHandle);
//	      pUSBD_CLIENT_HANDLE pUSB_TTYHandle;
//	     if (usbdClientRegister("USB/TTY",pUSB_TTYHandle) != OK){
//	     	printf("usbdclientregister failed\n");
//	     	return ERROR;
//	     }
//	     printf("usbdclientregister OK!\n");
//
//	      if (usbdDynamicAttachRegister(pUSB_TTYHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback) != OK){
//	      printf("usbdDynamicAttachRegister failed !\n");
//	      return ERROR;
//	      }
//	      printf("usbdDynamicAttachRegister success!\n");
//	      return OK;
//}



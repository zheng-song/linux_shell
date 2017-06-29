/*
 * xInitBak.c
 *
 *  Created on: 2017-6-29
 *      Author: sfh
 */

#include "xLib.h"

#define CP210X_CLIENT_NAME 	"cp210xLib"    	/* our USBD client name */
#define BUFFER_SIZE 100
#define MAX_WAIT_CYCLES 100

USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */
USBD_NODE_ID 	cp210xNodeId;			/*our USBD node ID*/
LOCAL UINT16 		initCount = 0;

LOCAL LIST_HEAD 	cp210xDevList;			/* linked list of CP210X_DEV */
LOCAL LIST_HEAD 	reqList;				/* Attach callback request list */

LOCAL MUTEX_HANDLE 	cp210xMutex;   			/* mutex used to protect internal structs */

LOCAL UINT32 		usbCp210xIrpTimeOut = CP210X_IRP_TIME_OUT;

LOCAL char buf[BUFFER_SIZE];
LOCAL int front = 0;
LOCAL int rear = 0;

#define ADD_POS(pos, offset) \
do {\
	pos += offset;\
	if(pos >= BUFFER_SIZE) pos -= BUFFER_SIZE;\
} while(0);

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

typedef struct cp210x_node{
	NODE 	node;
	struct cp210x_dev *pLibDev;
}CP210X_NODE;



typedef struct cp210x_dev
{
	DEV_HDR 		cp210xDevHdr;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	CP210X_NODE *          pCp210xNode;
	SEL_WAKEUP_LIST         selWakeupList;   //为解决任务列表
	SEM_ID                  cp210xSelectSem;


	LINK cp210xLibDevLink;	/* linked list of cp210x structs */

	USBD_NODE_ID nodeId;			/*device nodeID*/
	UINT16 configuration;	/*configuration/interface reported as*/
	UINT16 interface;		/*a interface of this device*/
	UINT16 interfaceAltSetting;

	UINT16 vendorId;		/*厂商ID */
	UINT16 productId;		/*设备的产品ID*/

	BOOL connected;		/*是否已连接*/
	UINT16 lockCount;				/* Count of times structure locked */

	USBD_PIPE_HANDLE outPipeHandle; /* USBD pipe handle for bulk OUT pipe */
	USB_IRP	outIrp;					/*IRP to monitor output to device*/
	BOOL outIrpInUse;				/*TRUE while IRP is outstanding*/
	char  *pOutBfr;					/* pointer to output buffer */
	UINT16 outBfrLen;		    	/* size of output buffer */
	UINT32 outErrors;		    	/* count of IRP failures */

	int trans_len;
	int trans_buf[64];

	USBD_PIPE_HANDLE inPipeHandle;

	UINT16 	inEpAddr;
	UINT16  outEpAddr;

	SEM_HANDLE cp210xIrpSem;

}CP210X_DEV;

#define CP210X_DEVICE_NUM			10
#define CP210X_NAME 				"/COM"
#define CP210X_NAME_LEN_MAX 		50

STATUS status;
LOCAL CP210X_DEV 				*pLibDevArray[CP210X_DEVICE_NUM];

LOCAL int 					cp210xDrvNum = -1; 	//驱动注册的驱动号
LOCAL LIST					cp210xList;			//系统所有的USBTTY设备
LOCAL SEM_ID 				cp210xListMutex;  	//保护列表互斥信号量
LOCAL SEM_ID				cp210xInitMutex;			//互斥信号量





#define CP210X_LIST_SEM_TAKE(tmout)	semTake(cp210xListMutex,(int)(tmout))
#define	CP210X_LIST_SEM_GIVE			semGive(cp210xListMutex)

/**************************函数前向声明**********************************************/
//LOCAL int cp210xDevRead(CP210X_DEV *pLibDev,  char *buffer,  UINT32 nBytes);
LOCAL int cp210xDevWrite(CP210X_DEV *pLibDev, char *buffer,UINT32 nBytes);
LOCAL int cp210xDevOpen(CP210X_DEV *pLibDev,  char *name,  int flags,int mode);
LOCAL int cp210xDevClose(CP210X_DEV *pLibDev);
//LOCAL int cp210xDevIoctl(CP210X_DEV *pLibDev,  int request,  void *arg);

LOCAL STATUS cp210xDevDelete(CP210X_DEV *pLibDev);

LOCAL STATUS cp210xDevCreate(char *,CP210X_DEV *);


/*为设备获取下一个单元编号*/
LOCAL int getCp210xNum(CP210X_DEV *pLibDev)
{
	int index;
	for(index =0;index<CP210X_DEVICE_NUM;index++){
		if(pLibDevArray[index] == 0){
			pLibDevArray[index] = pLibDev;
			return (index);
		}
	}
	printf("Out of avilable USB_TTY number.Check CP210X_DEVICE＿NUM\n");
	return (-1);
}

/*为设备释放下一个单元编号*/
LOCAL int freeCp210xNum(CP210X_DEV *pLibDev)
{
	int index;
	for (index=0; index < CP210X_DEVICE_NUM; index++)
	{
		if (pLibDevArray[index] == pLibDev){
			pLibDevArray[index] = NULL;
			return (index);
		}
	}
	printf("Unable to locate USB_TTY pointed to by channel 0x%X.\n",pLibDev);
	return(-1);
}


LOCAL STATUS cp210xDevCreate(char *name,CP210X_DEV * pLibDev )
{
	printf("IN cp210xDevCreate\n");
	CP210X_NODE 	*pCp210xNode 	= NULL;

	STATUS 			status 			= ERROR;

	if(pLibDev == (CP210X_DEV *)ERROR){
		printf("pLibDev is ERROR\n");
		return ERROR;
	}

	if((pLibDev = (CP210X_DEV *)calloc(1,sizeof(CP210X_DEV))) == NULL){
		printf("calloc returned NULL - out of memory\n");
		return ERROR;
	}


//	为节点分配内存并输入数据
	pCp210xNode = (CP210X_NODE *) calloc (1, sizeof (CP210X_NODE));

//	读取有用信息
	pCp210xNode->pLibDev = pLibDev;
	pLibDev->pCp210xNode = pCp210xNode;

//	将设备添加到系统的设备列表当中
	if((status = iosDevAdd (&pLibDev->cp210xDevHdr,name,cp210xDrvNum)) != OK){
		free(pLibDev);
		printf("Unable to create COM device.\n");
		return status;
	}

	printf("iosDevAdd success\n");

//	初始化列表（记录驱动上未解决的任务列表）
	selWakeupListInit(&pLibDev->selWakeupList);

//	为列表增加节点

	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstAdd (&cp210xList, (NODE *) pCp210xNode);
	CP210X_LIST_SEM_GIVE;

	return OK;
}

/*为cp210x删除道系统设备*/

STATUS cp210xDevDelete(CP210X_DEV *pLibDev)
{
	if(cp210xDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}

	selWakeupAll (&pLibDev->selWakeupList, SELREAD);

	if ((pLibDev->cp210xSelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pLibDev->selWakeupList) > 0){

		semTake (pLibDev->cp210xSelectSem, WAIT_FOREVER);
	}

	semDelete (pLibDev->cp210xSelectSem);

	selWakeupListTerm(&pLibDev->selWakeupList);


	iosDevDelete(&pLibDev->cp210xDevHdr);

	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&cp210xList, (NODE *) pLibDev->pCp210xNode);
	CP210X_LIST_SEM_GIVE;

	free(pLibDev->pCp210xNode);
	free(pLibDev);
	return OK;
}

LOCAL CP210X_DEV * findDev(USBD_NODE_ID nodeId)
{
	CP210X_DEV * pLibDev = usbListFirst(&cp210xDevList);

	while(pLibDev != NULL){
		if(pLibDev->nodeId == nodeId)
			break;

		pLibDev = usbListNext(&pLibDev->cp210xLibDevLink);
	}
	return pLibDev;
}


LOCAL STATUS cp210xChangeBaudRate(CP210X_DEV *pLibDev,UINT32 baud)
{
	UINT16 length = sizeof(UINT32);
	UINT16 actLength;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pLibDev->interface,length,(pUINT8)&baud,&actLength) != OK){
		return ERROR;
	}

	/*confirm the data has been written */
	UINT32 baudNow = 0;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pLibDev->interface,
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

LOCAL pUSB_ENDPOINT_DESCR findEndpoint( pUINT8 pBfr,UINT16 bfrLen,UINT16 direction )
{
	pUSB_ENDPOINT_DESCR pEp;

	while ((pEp = (pUSB_ENDPOINT_DESCR)usbDescrParseSkip (&pBfr,
			&bfrLen, USB_DESCR_ENDPOINT))!= NULL){

		if ((pEp->attributes & USB_ATTR_EPTYPE_MASK) == USB_ATTR_BULK &&
				(pEp->endpointAddress & USB_ENDPOINT_DIR_MASK) == direction)

			break;
	}

	return pEp;
}


LOCAL STATUS cp210xSetTermiosPort(CP210X_DEV *pLibDev)
{
	UINT16 length = 0;
	UINT16 actLength;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_IFC_ENABLE,UART_ENABLE,
			pLibDev->interface,0,NULL,&actLength) != OK)
		return ERROR;


	/*set default baudrate to 115200*/
	UINT32 baud = 115200;
	if(cp210xChangeBaudRate(pLibDev,baud) != OK){
		printf("set default baudrate to 115200 failed!\n");
		return ERROR;
	}

//设置为8个数据位，1个停止位，没有奇偶校验，没有流控。
	UINT16 bits;
	length = sizeof(UINT16);

	bits &= ~(BITS_DATA_MASK | BITS_PARITY_MASK | BITS_STOP_MASK);
	bits |= (BITS_DATA_8 | BITS_PARITY_NONE | BITS_STOP_1);

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pLibDev->interface,0,NULL,&actLength) != OK)
		return ERROR;

/*显示当前bits设置*/
	UINT16 bitsNow;
	length = sizeof(bitsNow);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pLibDev->interface,
			length,(pUINT8)&bitsNow,&actLength)!= OK)
		return ERROR;

	switch(bitsNow & BITS_DATA_MASK){
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

	default:
		printf("unknow number of data bits.using 8\n");
		return ERROR;
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
		return ERROR;
	}

	switch (bitsNow & BITS_STOP_MASK){
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
		printf("Unknown number of stop bitsNow,using 1 stop bit\n");
		return ERROR;
	}
	return OK;
}


STATUS configLibDev(CP210X_DEV *pLibDev)
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

    if (usbdDescriptorGet (cp210xHandle, pLibDev->nodeId,
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

	pLibDev->configuration = pCfgDescr->configurationValue;

	UINT16 ifNo = 0;
	pScratchBfr = pBfr;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,
			USB_DESCR_INTERFACE))!= NULL){
		/*look for the interface indicated in the pLibDev structure*/
		if (ifNo == pLibDev->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pLibDev->interfaceAltSetting = pIfDescr->alternateSetting;

	if ((pOutEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_OUT)) == NULL){
		OSS_FREE (pBfr);
        return ERROR;
	}

	if ((pInEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_IN)) == NULL){
		OSS_FREE (pBfr);
		return ERROR;
	}

	pLibDev->outEpAddr = pOutEp->endpointAddress;
	pLibDev->inEpAddr = pInEp->endpointAddress;

	if (usbdConfigurationSet (cp210xHandle, cp210xNodeId,
				pCfgDescr->configurationValue,
				pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) != OK){
        OSS_FREE (pBfr);
        return ERROR;
    }

    usbdInterfaceSet(cp210xHandle,cp210xNodeId,pLibDev->interface,\
			pIfDescr->alternateSetting);

	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) |
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pLibDev->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,
			0,0,&pLibDev->outPipeHandle)!= OK){
		OSS_FREE (pBfr);
		return ERROR;
	}

	maxPacketSize = *((pUINT8)&pInEp->maxPacketSize) |
			(*(((pUINT8)&pInEp->maxPacketSize)+1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pInEp->endpointAddress,\
			pCfgDescr->configurationValue,pLibDev->interface,\
			USB_XFRTYPE_BULK,USB_DIR_IN,maxPacketSize,
			0,0,&pLibDev->inPipeHandle)!= OK){
		OSS_FREE (pBfr);
		return ERROR;
	}

	if(cp210xSetTermiosPort(pLibDev) != OK){
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


	/*char f[]="hello world!";
		pLibDev->outIrp.transferLen = sizeof (f);
		pLibDev->outIrp.result = -1;
		pLibDev->outIrp.bfrCount = 1;
		pLibDev->outIrp.bfrList [0].pid = USB_PID_OUT;
		pLibDev->outIrp.bfrList [0].pBfr = (pUINT8)f;
		pLibDev->outIrp.bfrList [0].bfrLen = sizeof (f);
		if (usbdTransfer (cp210xHandle, pLibDev->outPipeHandle, &pLibDev->outIrp) != OK){
			printf("usbdtransfer returned false\n");
		}

		sleep(2);*/

    OSS_FREE (pBfr);
    return OK;
}



LOCAL int copy_buf(CP210X_DEV *pLibDev)
{
	int n, i;
	n = (front-rear);
	if(n == 0) {
		pLibDev->trans_len = 0;
		return 0;
	}
	if(n < 0) n += BUFFER_SIZE;
	if(n > 64) n = 64;

	for(i=0; i<n; ++i){
		if(rear == BUFFER_SIZE) rear=0;
		pLibDev->trans_buf[i] = buf[rear++];
	}
	pLibDev->trans_len = n;
	return n;
}

LOCAL STATUS initOutIrp( CP210X_DEV *pLibDev)
{
	pUSB_IRP pIrp = &pLibDev->outIrp;
	if(pLibDev->trans_len == 0){
		printf("initOutIrp len==0\n");
		return OK;
	}
	pIrp->irpLen = sizeof (pLibDev->outIrp);
	pLibDev->outIrp.transferLen = front-rear;

	pLibDev->outIrp.bfrCount = 1;
	pLibDev->outIrp.bfrList[0].pBfr = (pUINT8)rear;
	pLibDev->outIrp.bfrList[0].bfrLen = front-rear;

	/* Submit IRP */
	if (usbdTransfer (cp210xHandle, pLibDev->outPipeHandle, &pLibDev->outIrp) != OK){
		printf("usbdTransfer error\n");
		return ERROR;
	}
	return OK;

	/*
	pUSB_IRP pIrp = &pLibDev->outIrp;
	UINT16 count;
	// Return immediately if the output IRP is already in use.
	if(!pLibDev->outIrpInUse){
		// Initialize IRP
		pIrp->irpLen = sizeof (pLibDev->outIrp);
		pLibDev->outIrp.transferLen = count;

		pLibDev->outIrp.bfrCount = 1;
		pLibDev->outIrp.bfrList[0].pid = USB_PID_OUT;
		pLibDev->outIrp.bfrList[0].pBfr = (UINT8 *)pLibDev->pOutBfr;
		pLibDev->outIrp.bfrList[0].bfrLen = count;

		// Submit IRP
		if (usbdTransfer (cp210xHandle, pLibDev->outPipeHandle, &pLibDev->outIrp) != OK)
		return ERROR;

		pLibDev->outIrpInUse = TRUE;
		return OK;
	}*/

	return ERROR;
}


LOCAL void cp210xIrpCallback(pVOID p)
{
	printf("IN cp210xIrpCallback\n");
	USB_IRP	*pIrp = (USB_IRP *)p;
	CP210X_DEV *pLibDev = pIrp->userPtr;

	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	/* Determine if we're dealing with the output or input IRP. */
	if (pIrp == &pLibDev->outIrp)
	{
		if (pIrp->result != OK) {
			pLibDev->outErrors++;
			if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
				initOutIrp (pLibDev);
			return;
		}

	    //LOCK(m);
	    if(copy_buf(pLibDev)) {
	    	initOutIrp(pLibDev);
	    } else {
	    	pLibDev->outIrpInUse = FALSE;
	    }
	    //UNLOCK(m);
	}

	OSS_MUTEX_RELEASE (cp210xMutex);
    OSS_SEM_GIVE (pLibDev->cp210xIrpSem);

}





STATUS createLibDev(CP210X_DEV *pLibDev)
{
	 /*Try to allocate space for the new structure's parameter*/
	if ((pLibDev->pOutBfr = OSS_CALLOC(CP210X_OUT_BFR_SIZE))== NULL )
		return ERROR;

	pLibDev->outBfrLen = CP210X_OUT_BFR_SIZE;
	/*initial outIrp*/
	pLibDev->outIrp.irpLen				= sizeof (USB_IRP);
	pLibDev->outIrp.userCallback		= cp210xIrpCallback;
	pLibDev->outIrp.timeout            = usbCp210xIrpTimeOut;
	pLibDev->outIrp.bfrCount          = 0x01;
	pLibDev->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pLibDev->outIrp.userPtr           = pLibDev;

	/*Try to configure the cp210x device*/
	if(configLibDev(pLibDev) != OK)
			return ERROR;

	if (OSS_SEM_CREATE( 1, 1, &pLibDev->cp210xIrpSem) != OK){
		return ERROR;
//        return(cp210xShutdown(S_cp210xLib_OUT_OF_RESOURCES));
	}
	return OK;
}



LOCAL void destoryLibDev(CP210X_DEV * pLibDev)
{
	if (pLibDev != NULL){
		/* Unlink the structure. */
		usbListUnlink (&pLibDev->cp210xLibDevLink);

		/* Release pipes and wait for IRPs to be cancelled if necessary. */
		if (pLibDev->outPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pLibDev->outPipeHandle);

		if (pLibDev->inPipeHandle != NULL)
			usbdPipeDestroy (cp210xHandle, pLibDev->inPipeHandle);

		/* The outIrpInUse or inIrpInUse can be set to FALSE only when the mutex
		 * is released. So releasing the mutex here
		 */
		OSS_MUTEX_RELEASE (cp210xMutex);

		while ( pLibDev->outIrpInUse )
			    OSS_THREAD_SLEEP (1);

		/* Acquire the mutex again */
		OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

		/* release buffers */
		if (pLibDev->pOutBfr != NULL)
		    OSS_FREE (pLibDev->pOutBfr);

		/* Release structure. */
		OSS_FREE (pLibDev);
	}
}


LOCAL STATUS cp210xLibAttachCallback ( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,
		UINT16 interface,UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
	CP210X_DEV *pLibDev;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int cp210xUnitNum;

	char cp210xName[CP210X_NAME_LEN_MAX];

    int noOfSupportedDevices =(sizeof(cp210xAdapterList)/(2*sizeof(UINT16))) ;
    int index = 0;

    if((pBfr = OSS_MALLOC(USB_MAX_DESCR_LEN)) == NULL)
    	return ERROR;

    OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	switch(attachAction){
	case USBD_DYNA_ATTACH:

		if(findDev(nodeId)!= NULL)
			break;

		if (usbdDescriptorGet (cp210xHandle, nodeId, USB_RT_STANDARD | USB_RT_DEVICE,
				USB_DESCR_DEVICE, 0, 0, 36, pBfr, &actLen) != OK)
	    	  break;

		vendorId = (((pUSB_DEVICE_DESCR) pBfr)->vendor);
		productId = (((pUSB_DEVICE_DESCR) pBfr)->product);

		for (index = 0; index < noOfSupportedDevices; index++)
			if (vendorId == cp210xAdapterList[index][0])
				if (productId == cp210xAdapterList[index][1])
					break;

		if (index == noOfSupportedDevices ){
			printf( " Unsupported device find vId %0x; pId %0x!!!\n",\
				  vendorId, productId);
			break;
		}else{
			printf("Find device vId %0x; pId %0x!!!\n",vendorId,productId);
		}

		if((pLibDev = OSS_CALLOC(sizeof(CP210X_DEV))) == NULL )
	    	  break;

	    pLibDev->nodeId = nodeId;
	    pLibDev->configuration = configuration;
	    pLibDev->interface = interface;
	    pLibDev->vendorId = vendorId;
	    pLibDev->productId = productId;

	    /*Save a global copy of NodeID*/
	    cp210xNodeId = nodeId;

	    /* Continue fill the newly allocate structure,If there's an error,
		 * there's nothing we can do about it, so skip the device and return immediately.
		 */
		if((createLibDev(pLibDev)) != OK){
			destoryLibDev(pLibDev);
			break;
		}

	    /*ADD this structure to the linked list tail*/
		usbListLink (&cp210xDevList, pLibDev, &pLibDev->cp210xLibDevLink,LINK_TAIL);

	      /*将设备标记为已连接*/
	    pLibDev->connected = TRUE;

	    cp210xUnitNum = getCp210xNum(pLibDev);
		if(cp210xUnitNum >= 0){
			sprintf(cp210xName,"%s%d",CP210X_NAME,cp210xUnitNum);

			if(cp210xDevCreate(cp210xName,pLibDev) != OK){
				printf("cp210xDevCreate() returned ERROR\n");
				return ERROR;
			}
			printf("cp210xDevCreate() returned OK\n");
		}else{
			printf("Excessive!! check CP210X_DEVICE_NUM");
			return ERROR;
		}

		printf("cp210x attached as %s\n",cp210xName);
	    break;


	case USBD_DYNA_REMOVE:

		if ((pLibDev = findDev (nodeId)) == NULL)
			break;

		/* The device has been disconnected. */
		if(pLibDev->connected == FALSE)
			break;

		pLibDev->connected = FALSE;

		pLibDev->lockCount++;
//		notifyAttach (pLibDev, CP210X_REMOVE);

		cp210xDevDelete (pLibDev);
		sprintf (cp210xName, "%s%d", CP210X_NAME, freeCp210xNum(pLibDev));
		printf ("%s removed\n", cp210xName);

		pLibDev->lockCount--;

		/* If no callers have the channel structure locked,
		 * destroy it now.If it is locked, it will be destroyed
		 * later during a call to cp210xLibDevUnlock().*/
		if (pLibDev->lockCount == 0)
			destoryLibDev (pLibDev);

		break;
	}

	OSS_FREE(pBfr);
	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}


LOCAL STATUS cp210xShutdown(int errCode)
{
    CP210X_DEV * pLibDev;

    /* Dispose of any open connections. */
    while ((pLibDev = usbListFirst (&cp210xDevList)) != NULL)
    	destoryLibDev (pLibDev);

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



STATUS cp210xDevInit (void)
{
	if(initCount == 0){
		memset(&cp210xDevList,0,sizeof(cp210xDevList));
		memset(&reqList,0,sizeof(reqList));

		cp210xMutex = NULL;
		cp210xHandle = NULL;

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

STATUS cp210xDevShutdown (void) {
    if (initCount == 0)
	return ossStatus (S_cp210xLib_NOT_INITIALIZED);

    if (--initCount == 0)
	return cp210xShutdown (OK);

    return OK;
}

/*初始化cp210x驱动*/
STATUS cp210xDrvInit(void)
{
	if(cp210xDrvNum > 0){
		printf("cp210x already initialized.\n");
		return ERROR;
	}

	cp210xInitMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);
	cp210xListMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
	                  SEM_INVERSION_SAFE);

	if((cp210xInitMutex == NULL) || (cp210xListMutex == NULL)){
		printf("Resource Allocation Failure.\n");
		goto ERROR_HANDLER;
	}

	/*初始化链表*/
	lstInit(&cp210xList);

	/*cp210xDrvNum = iosDrvInstall(NULL,cp210xDevDelete,cp210xDevOpen,cp210xDevClose,
			cp210xDevRead,cp210xDevWrite,cp210xDevIoctl);*/
	cp210xDrvNum = iosDrvInstall(NULL,NULL,cp210xDevOpen,cp210xDevClose,
				NULL,cp210xDevWrite,NULL);


/*检查是否为驱动安装空间*/
	if(cp210xDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		goto ERROR_HANDLER;
	}

	if(cp210xDevInit() == OK){
		printf("cp210xDevInit() returned OK\n");
	}else{
		printf("cp210xDevInit() returned ERROR\n");
		cp210xDevShutdown();
		goto ERROR_HANDLER;
	}

	int i;
	for(i=0;i< CP210X_DEVICE_NUM;++i){
		pLibDevArray[i]=NULL;
	}
	return OK;

ERROR_HANDLER:

	if(cp210xDrvNum){
		iosDrvRemove (cp210xDrvNum, 1);
		cp210xDrvNum = 0;
		}
	if (cp210xInitMutex != NULL)
		semDelete(cp210xInitMutex);

	if(cp210xListMutex != NULL)
		semDelete(cp210xListMutex);

	cp210xInitMutex = NULL;
	cp210xListMutex = NULL;
	return ERROR;
}

/*关闭cp210x 设备驱动*/

STATUS cp210xDrvUnInit (void)
{
    if (!cp210xDrvNum)
    	return (OK);

    /* unregister */
    if (usbdDynamicAttachUnRegister (cp210xHandle,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,
			(USBD_ATTACH_CALLBACK)cp210xLibAttachCallback)!= OK){
    	printf("cp210xDynamicAttachUnRegister () returned ERROR\n");
        return (ERROR);
    }

    /*删除驱动*/
    if (iosDrvRemove (cp210xDrvNum, TRUE) != OK){
    	printf("iosDrvRemove () returned ERROR\n");
        return (ERROR);
    }

    cp210xDrvNum = -1;

    /*删除互斥信号量*/
    if (semDelete (cp210xInitMutex) == ERROR){
        printf("semDelete (cp210xInitMutex) returned ERROR\n");
        return (ERROR);
    }

    if (semDelete (cp210xListMutex) == ERROR){
        printf("semDelete (cp210xListMutex) returned ERROR\n");
        return (ERROR);
    }


    /*关闭*/

    if (cp210xDevShutdown () != OK){
        printf("cp210xDevShutDown() returned ERROR\n");
        return (ERROR);
    }

    return (OK);
}

/*******************************************************************
 * 写USBTTY设备
 *  在每一次写入操作之前，必须检查上一次的数据是否传输完成，
 *  即是否CP210X_DEV->outIrpInUse == FALSE,另外，为了避免UART端出现问题，对于
 *  状态位的检查不是无限制的，代码中设置一个循环等待次数（1000次），若在等待了
 *  这么多次数以后，上次的数据仍然没有发送完，则退出此次写操作，返回当前已处理的
 *  的字节数。
 * */

LOCAL int cp210xDevWrite(CP210X_DEV *pLibDev, char *buffer,UINT32 nBytes)
{
	int n;
	printf("nBytes is:%d",nBytes);
	if((nBytes <=0)||(nBytes > BUFFER_SIZE-1)) return -1;

//	LOCK(m);
	n = (front-rear);
	if(n < 0) n += BUFFER_SIZE;

	n = BUFFER_SIZE - n - 1;

	if(n < nBytes) {
		ADD_POS(rear, nBytes-n);
	}

	for(n=0; n<nBytes; ++n){
		if(front == BUFFER_SIZE) front=0;
		buf[front++] = buffer[n];
	}
	//ADD_POS(front, nBytes);

/*	if(pLibDev->outIrpInUse == FALSE) {
		pLibDev->outIrpInUse = TRUE;
		copy_buf();
		initOutIrp(pLibDev);
	}*/

//	UNLOCK(m);
	//数据已存入缓冲区，开始发送。

	if(front == rear){
	}
	if(front > rear){
		while(front > rear){
			printf("%c",buf[rear]);
			rear++;
		}
	}

	if(front < rear){
		while(rear < BUFFER_SIZE+1){
			printf("%c",buf[rear]);
			rear++;
		}

		if(rear == BUFFER_SIZE + 1){
			rear = 0;
		}

		while(rear < front){
			printf("%c",buf[rear]);
			rear++;
		}
	}

	printf("\n");

	return n;
}


/*打开cp210x设备，*/
LOCAL int cp210xDevOpen(CP210X_DEV *pLibDev, char *name, int flags,int mode)
{
	printf("OK here im in cp210xDevOpen!\n");
	(pLibDev->numOpen)++; /*增加打开路径数*/

	return ((int) pLibDev);
}


/*关闭cp210x设备*/
LOCAL int cp210xDevClose(CP210X_DEV *pLibDev)
{
	/*是否没有开放的通道*/
	--(pLibDev->numOpen);
	return ((int)pLibDev);
}


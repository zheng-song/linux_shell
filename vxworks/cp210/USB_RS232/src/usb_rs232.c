/*
 * usb_rs232.c
 *
 *  Created on: 2017-6-28
 *      Author: sfh
 */
#include "xLib.h"

#define CP210X_CLIENT_NAME 	"cp210xClient"    	/* our USBD client name */
#define BUFFER_SIZE 4096

USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */
USBD_NODE_ID 	cp210xNodeId;			/*our USBD node ID*/

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

typedef struct cp210x_dev
{
	DEV_HDR 		cp210xDevHdr;

	UINT16                  numOpen;
//	SEL_WAKEUP_LIST         selWakeupList;   //为解决任务列表
	SEM_ID                  cp210xSelectSem;


	LINK cp210xDevLink;	/* linked list of cp210x structs */

	USBD_NODE_ID nodeId;			/*device nodeID*/
	UINT16 configuration;	/*configuration/interface reported as*/
	UINT16 interface;		/*a interface of this device*/
	UINT16 interfaceAltSetting;

	UINT16 vendorId;		/*厂商ID */
	UINT16 productId;		/*设备的产品ID*/

	BOOL connected;		/*是否已连接*/

	USBD_PIPE_HANDLE outPipeHandle; /* USBD pipe handle for bulk OUT pipe */
	USB_IRP	outIrp;					/*IRP to monitor output to device*/
	BOOL outIrpInUse;				/*TRUE while IRP is outstanding*/
	UINT32 outErrors;		    	/* count of IRP failures */

	int trans_len;
	UINT8 trans_buf[64];

	USBD_PIPE_HANDLE inPipeHandle;

	UINT16 	inEpAddr;
	UINT16  outEpAddr;

//	SEM_HANDLE cp210xIrpSem;

} CP210X_DEV;

LOCAL CP210X_DEV one_dev;
LOCAL CP210X_DEV *pDev = NULL;

LOCAL int 					cp210xDrvNum = -1; 	//驱动注册的驱动号

#define CP210X_NAME 				"/hust_usb_serial"

/**************************函数前向声明**********************************************/
//LOCAL int cp210xDevRead(CP210X_DEV *pDev,  char *buffer,  UINT32 nBytes);
LOCAL int cp210xDevWrite(CP210X_DEV *pDev, char *buffer,UINT32 nBytes);
LOCAL int cp210xDevOpen(CP210X_DEV *pDev,  char *name,  int flags,int mode);
LOCAL int cp210xDevClose(CP210X_DEV *pDev);
//LOCAL int cp210xDevIoctl(CP210X_DEV *pDev,  int request,  void *arg);

LOCAL void cp210xIrpCallback(pVOID p);
LOCAL STATUS createDev(CP210X_DEV *pDev);

LOCAL void copy_buf(void)
{
	int n, i;
	n = (front-rear);
	if(n == 0) {
		pDev->trans_len = 0;
		return;
	}
	if(n < 0) n += BUFFER_SIZE;
	if(n > 64) n = 64;

	for(i=0; i<n; ++i){
		if(rear == BUFFER_SIZE) rear=0;
		pDev->trans_buf[i] = buf[rear++];
	}
	pDev->trans_len = n;
	return;
}

LOCAL STATUS initOutIrp()
{
	if(pDev->trans_len == 0){
		pDev->outIrpInUse = FALSE;
		return OK;
	}

	/*initial outIrp*/
	pDev->outIrp.irpLen				= sizeof (USB_IRP);
	pDev->outIrp.userCallback		= cp210xIrpCallback;
	pDev->outIrp.timeout            = usbCp210xIrpTimeOut;
	pDev->outIrp.bfrCount          = 0x01;
	pDev->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pDev->outIrp.userPtr           = pDev;

	pDev->outIrp.irpLen = sizeof (pDev->outIrp);
	pDev->outIrp.transferLen = pDev->trans_len;
	pDev->outIrp.bfrCount = 1;
	pDev->outIrp.bfrList[0].pBfr = pDev->trans_buf;
	pDev->outIrp.bfrList[0].bfrLen = pDev->trans_len;

	/* Submit IRP */
	if (usbdTransfer (cp210xHandle, pDev->outPipeHandle, &pDev->outIrp) != OK){
		printf("usbdTransfer error\n");
		return ERROR;
	}

	pDev->outIrpInUse = TRUE;
	return OK;
}


LOCAL void cp210xIrpCallback(pVOID p)
{
	printf("IN cp210xIrpCallback\n");
	USB_IRP	*pIrp = (USB_IRP *)p;
	CP210X_DEV *pDev = pIrp->userPtr;

	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	/* Determine if we're dealing with the output or input IRP. */
	if (pIrp == &pDev->outIrp)
	{
		if (pIrp->result != OK) {
			pDev->outErrors++;
			if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
				initOutIrp (pDev);
			return;
		}

		copy_buf();
    	initOutIrp();
	}

	OSS_MUTEX_RELEASE (cp210xMutex);
}


LOCAL void destoryDev(CP210X_DEV *pDev)
{
	/* Release pipes and wait for IRPs to be cancelled if necessary. */
	if (pDev->outPipeHandle != NULL)
		usbdPipeDestroy (cp210xHandle, pDev->outPipeHandle);

	if (pDev->inPipeHandle != NULL)
		usbdPipeDestroy (cp210xHandle, pDev->inPipeHandle);
}

LOCAL STATUS cp210xAttachCallback ( USBD_NODE_ID nodeId, UINT16 attachAction, UINT16 configuration,
		UINT16 interface,UINT16 deviceClass, UINT16 deviceSubClass, UINT16 deviceProtocol)
{
    UINT8 pBfr[USB_MAX_DESCR_LEN];
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int noOfSupportedDevices =(sizeof(cp210xAdapterList)/(2*sizeof(UINT16))) ;
    int index = 0;

    OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	switch(attachAction){
	case USBD_DYNA_ATTACH:

		if(pDev->connected == TRUE) break;

		if (usbdDescriptorGet (cp210xHandle, nodeId, USB_RT_STANDARD | USB_RT_DEVICE,
				USB_DESCR_DEVICE, 0, 0, sizeof(pBfr), pBfr, &actLen) != OK)
	    	  break;

		vendorId = (((pUSB_DEVICE_DESCR) pBfr)->vendor);
		productId = (((pUSB_DEVICE_DESCR) pBfr)->product);

		for (index = 0; index < noOfSupportedDevices; index++)
			if (vendorId == cp210xAdapterList[index][0])
				if (productId == cp210xAdapterList[index][1])
					break;

		if (index == noOfSupportedDevices ){
			break;
		}else{
			printf("Find device vId %0x; pId %0x!!!\n",vendorId,productId);
		}

	    /*Save a global copy of NodeID*/
	    cp210xNodeId = nodeId;

	    pDev->nodeId = nodeId;
	    pDev->configuration = configuration;
	    pDev->interface = interface;
	    pDev->vendorId = vendorId;
	    pDev->productId = productId;

	    /* Continue fill the newly allocate structure,If there's an error,
		 * there's nothing we can do about it, so skip the device and return immediately.
		 */
		if((createDev(pDev)) != OK){
			break;
		}

		pDev->connected = TRUE; /*将设备标记为已连接*/
		copy_buf();
		initOutIrp();
	    break;

	case USBD_DYNA_REMOVE:

		if(pDev->connected == FALSE) break;
		if(pDev->nodeId != nodeId) break;
		pDev->connected = FALSE;
		destoryDev (pDev);
		break;
	}

	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}

/*初始化cp210x驱动*/
STATUS cp210xDrvInit(void)
{
	STATUS status;

	if(cp210xDrvNum > 0){
		printf("cp210x already initialized.\n");
		return ERROR;
	}

	pDev = &one_dev;

//	cp210xInitMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
//	                  SEM_INVERSION_SAFE);
//	cp210xListMutex = semMCreate (SEM_Q_PRIORITY | SEM_DELETE_SAFE |
//	                  SEM_INVERSION_SAFE);

//	if((cp210xInitMutex == NULL) || (cp210xListMutex == NULL)){
//		printf("Resource Allocation Failure.\n");
//		goto ERROR_HANDLER;
//	}

	cp210xDrvNum = iosDrvInstall(NULL,NULL,cp210xDevOpen,cp210xDevClose,
				NULL,cp210xDevWrite,NULL);

/*检查是否为驱动安装空间*/
	if(cp210xDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		return ERROR;
	}

	if((status =iosDevAdd(&pDev->cp210xDevHdr,CP210X_NAME,cp210xDrvNum)) != OK){
		printf("Unable to create COM device.\n");
		return status;
	}

	cp210xMutex = NULL;
	cp210xHandle = NULL;

	if(OSS_MUTEX_CREATE(&cp210xMutex) != OK) {
		printf("Unable to OSS_MUTEX_CREATE\n");
		return ERROR;
	}

	if(usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK) {
		printf("Unable to usbdClientRegister\n");
		return ERROR;
	}

	if(usbdDynamicAttachRegister(cp210xHandle,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,TRUE,
					(USBD_ATTACH_CALLBACK)cp210xAttachCallback)!= OK)
	{
		printf("Unable to usbdDynamicAttachRegister\n");
		return ERROR;
	}

	return OK;
}

/*关闭cp210x 设备驱动*/

STATUS cp210xDrvUnInit (void)
{
#if 0
    if (!cp210xDrvNum)
    	return (OK);

    /* unregister */
    if (usbdDynamicAttachUnRegister (cp210xHandle,USBD_NOTIFY_ALL,\
    		USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,
			(USBD_ATTACH_CALLBACK)cp210xAttachCallback)\
			!= OK){
    	printf("usbdDynamicAttachUnRegister () returned ERROR\n");
        return (ERROR);
    }

    /*删除驱动*/
    if (iosDrvRemove (cp210xDrvNum, TRUE) != OK){
    	printf("iosDrvRemove () returned ERROR\n");
        return (ERROR);
    }

    cp210xDrvNum = -1;

	destoryDev ();
	usbdClientUnregister (cp210xHandle);

    /*删除互斥信号量*/
//    if (semDelete (cp210xInitMutex) == ERROR){
//        printf("semDelete (cp210xInitMutex) returned ERROR\n");
//        return (ERROR);
//    }
//
//    if (semDelete (cp210xListMutex) == ERROR){
//        printf("semDelete (cp210xListMutex) returned ERROR\n");
//        return (ERROR);
//    }


    /*关闭*/
#endif
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

LOCAL int cp210xDevWrite(CP210X_DEV *pDev, char *buffer,UINT32 nBytes)
{
	int n;
	printf("nBytes is:%d",nBytes);
	if((nBytes <=0)||(nBytes > BUFFER_SIZE-1)) return -1;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

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

	if(pDev->connected == FALSE) {
		OSS_MUTEX_RELEASE(cp210xMutex);
		return n;
	}

	if(pDev->outIrpInUse == FALSE) {
		pDev->outIrpInUse = TRUE;
		copy_buf();
		initOutIrp(pDev);
	}

	OSS_MUTEX_RELEASE(cp210xMutex);
	return n;
}

/*打开cp210x设备，*/
LOCAL int cp210xDevOpen(CP210X_DEV *pDev, char *name, int flags,int mode)
{
	(pDev->numOpen)++;
	return ((int) pDev);
}

/*关闭cp210x设备*/
LOCAL int cp210xDevClose(CP210X_DEV *pDev)
{
	--(pDev->numOpen);
	return ((int)pDev);
}

LOCAL STATUS cp210xChangeBaudRate(CP210X_DEV *pDev,UINT32 baud)
{
	UINT16 length = sizeof(UINT32);
	UINT16 actLength;
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pDev->interface,length,(pUINT8)&baud,&actLength) != OK){
		return ERROR;
	}

	/*confirm the data has been written */
	UINT32 baudNow = 0;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pDev->interface,
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


LOCAL STATUS cp210xSetTermiosPort(CP210X_DEV *pDev)
{
	UINT16 length = 0;
	UINT16 actLength;

	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_IFC_ENABLE,UART_ENABLE,
			pDev->interface,0,NULL,&actLength) != OK)
		return ERROR;

	/*set default baudrate to 115200*/
	UINT32 baud = 115200;
	if(cp210xChangeBaudRate(pDev,baud) != OK){
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
			pDev->interface,0,NULL,&actLength) != OK)
		return ERROR;

/*显示当前bits设置*/
	UINT16 bitsNow;
	length = sizeof(bitsNow);
	if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pDev->interface,
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

STATUS createDev(CP210X_DEV *pDev)
{
	USB_CONFIG_DESCR 	* pCfgDescr;
	USB_INTERFACE_DESCR * pIfDescr;
	USB_ENDPOINT_DESCR 	* pOutEp;
	USB_ENDPOINT_DESCR 	* pInEp;
	UINT8 pBfr[USB_MAX_DESCR_LEN];
	UINT8 * pScratchBfr;
	UINT16 actLen;
	UINT16 maxPacketSize;

	if (usbdDescriptorGet (cp210xHandle, pDev->nodeId,\
				USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,\
				0, 0, USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
		return ERROR;
	}

	if ((pCfgDescr = usbDescrParse (pBfr, actLen,\
			USB_DESCR_CONFIGURATION)) == NULL){
		return ERROR;
	}

	pDev->configuration = pCfgDescr->configurationValue;

	UINT16 ifNo = 0;
	pScratchBfr = pBfr;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,\
			USB_DESCR_INTERFACE))!= NULL){
		/*look for the interface indicated in the pDev structure*/
		if (ifNo == pDev->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		return ERROR;
	}

	pDev->interfaceAltSetting = pIfDescr->alternateSetting;

	if ((pOutEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_OUT))\
			== NULL){
		return ERROR;
	}

	if ((pInEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_IN))\
			== NULL){
		return ERROR;
	}

	pDev->outEpAddr = pOutEp->endpointAddress;
	pDev->inEpAddr = pInEp->endpointAddress;

	if (usbdConfigurationSet (cp210xHandle, cp210xNodeId,\
				pCfgDescr->configurationValue,\
				pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT)\
				!= OK){
		return ERROR;
	}

	usbdInterfaceSet(cp210xHandle,cp210xNodeId,pDev->interface,\
			pIfDescr->alternateSetting);

	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) | \
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pDev->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,\
			0,0,&pDev->outPipeHandle)!= OK){
		return ERROR;
	}

	maxPacketSize = *((pUINT8)&pInEp->maxPacketSize) |
			(*(((pUINT8)&pInEp->maxPacketSize)+1) << 8);

	if(usbdPipeCreate(cp210xHandle,cp210xNodeId,pInEp->endpointAddress,\
			pCfgDescr->configurationValue,pDev->interface,\
			USB_XFRTYPE_BULK,USB_DIR_IN,maxPacketSize,\
			0,0,&pDev->inPipeHandle)!= OK){
		return ERROR;
	}

	if(cp210xSetTermiosPort(pDev) != OK){
		return ERROR;
	}

	/* Clear HALT feauture on the endpoints */
	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,\
			USB_FSEL_DEV_ENDPOINT_HALT, (pOutEp->endpointAddress & 0xFF)))\
			!= OK){
		return ERROR;
	}

	if ((usbdFeatureClear (cp210xHandle, cp210xNodeId, USB_RT_ENDPOINT,\
			USB_FSEL_DEV_ENDPOINT_HALT,(pInEp->endpointAddress & 0xFF)))\
			!= OK){
		return ERROR;
	}

	/*char f[]="hello world!";
		pDev->outIrp.transferLen = sizeof (f);
		pDev->outIrp.result = -1;
		pDev->outIrp.bfrCount = 1;
		pDev->outIrp.bfrList [0].pid = USB_PID_OUT;
		pDev->outIrp.bfrList [0].pBfr = (pUINT8)f;
		pDev->outIrp.bfrList [0].bfrLen = sizeof (f);
		if (usbdTransfer (cp210xHandle, pDev->outPipeHandle, &pDev->outIrp) != OK){
			printf("usbdtransfer returned false\n");
		}

		sleep(2);*/

	return OK;
}



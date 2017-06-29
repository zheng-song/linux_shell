/*
 * usb_rs232.c
 *
 *  Created on: 2017-6-28
 *      Author: zhengsong
 */
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
#include <usb/usbLib.h>
#include <usb/usbdLib.h>


/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
#define REQTYPE_INTERFACE_TO_HOST	0xc1
#define REQTYPE_HOST_TO_DEVICE	0x40
#define REQTYPE_DEVICE_TO_HOST	0xc0

/* Config request codes */
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





/*IRP Time out in millisecs */
#define CP210X_IRP_TIME_OUT 	5000

#define CP210X_CLIENT_NAME 	"cp210xClient"    	/* our USBD client name */
#define CP210X_NAME  "/hust_usb_serial"

#define BUFFER_SIZE 4096

USBD_CLIENT_HANDLE  cp210xHandle; 	/* our USBD client handle */

LOCAL MUTEX_HANDLE 	cp210xMutex;   			/* mutex used to protect internal structs */

LOCAL UINT32 usbCp210xIrpTimeOut = CP210X_IRP_TIME_OUT;

LOCAL char buf[BUFFER_SIZE];
LOCAL int front = 0;
LOCAL int rear = 0;

LOCAL int  cp210xDrvNum = -1; 	//驱动注册的驱动号

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

	int trans_len;
	UINT8 trans_buf[64];

//	USBD_PIPE_HANDLE inPipeHandle;

//	UINT16 	inEpAddr;
	UINT16  outEpAddr;
} CP210X_DEV;

LOCAL CP210X_DEV one_dev;
LOCAL CP210X_DEV *pCp210xDev = NULL;

/**************************函数前向声明**********************************************/
//LOCAL int cp210xDevRead(CP210X_DEV *pCp210xDev,  char *buffer,  UINT32 nBytes);
LOCAL int cp210xDevWrite(CP210X_DEV *pCp210xDev, char *buffer,UINT32 nBytes);
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev,  char *name,  int flags,int mode);
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev);
//LOCAL int cp210xDevIoctl(CP210X_DEV *pCp210xDev,  int request,  void *arg);

LOCAL void cp210xIrpCallback(pVOID p);
LOCAL STATUS createDev(CP210X_DEV *pCp210xDev);

LOCAL void copy_buf(void)
{
	int n, i;
	n = (front-rear);
	if(n == 0) {
		pCp210xDev->trans_len = 0;
		return;
	}
	if(n < 0) n += BUFFER_SIZE;
	if(n > 64) n = 64;

	for(i=0; i<n; ++i){
		if(rear == BUFFER_SIZE) rear=0;
		pCp210xDev->trans_buf[i] = buf[rear++];
	}
	pCp210xDev->trans_len = n;
	return;
}

LOCAL STATUS initOutIrp(void)
{
	if(pCp210xDev->trans_len == 0){
		pCp210xDev->outIrpInUse = FALSE;
		return OK;
	}

	/*initial outIrp*/
	pCp210xDev->outIrp.irpLen				= sizeof (USB_IRP);
	pCp210xDev->outIrp.userCallback		= cp210xIrpCallback;
	pCp210xDev->outIrp.timeout            = usbCp210xIrpTimeOut;
	pCp210xDev->outIrp.bfrCount          = 0x01;
	pCp210xDev->outIrp.bfrList[0].pid    = USB_PID_OUT;
	pCp210xDev->outIrp.userPtr           = pCp210xDev;

	pCp210xDev->outIrp.irpLen = sizeof (pCp210xDev->outIrp);
	pCp210xDev->outIrp.transferLen = pCp210xDev->trans_len;
	pCp210xDev->outIrp.bfrCount = 1;
	pCp210xDev->outIrp.bfrList[0].pBfr = pCp210xDev->trans_buf;
	pCp210xDev->outIrp.bfrList[0].bfrLen = pCp210xDev->trans_len;

	/* Submit IRP */
	if (usbdTransfer (cp210xHandle, pCp210xDev->outPipeHandle, &pCp210xDev->outIrp) != OK){
		printf("usbdTransfer error\n");
		return ERROR;
	}

	pCp210xDev->outIrpInUse = TRUE;
	return OK;
}


LOCAL void cp210xIrpCallback(pVOID p)
{
	USB_IRP	*pIrp = (USB_IRP *)p;
	CP210X_DEV *pCp210xDev = pIrp->userPtr;

	OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

	if (pIrp == &pCp210xDev->outIrp)
	{
		if (pIrp->result != OK) {
			if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
				initOutIrp ();

			return;
		}

		copy_buf();
    	initOutIrp();
	}
	OSS_MUTEX_RELEASE (cp210xMutex);
}


LOCAL void destoryDev(CP210X_DEV *pCp210xDev)
{
	/* Release pipes and wait for IRPs to be cancelled if necessary. */
	if (pCp210xDev->outPipeHandle != NULL){
		usbdPipeDestroy (cp210xHandle, pCp210xDev->outPipeHandle);
		pCp210xDev->outPipeHandle = NULL;
	}
	return ;
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
		printf("attach");
		if(pCp210xDev->connected == TRUE) break;

		if (usbdDescriptorGet (cp210xHandle, nodeId, \
				USB_RT_STANDARD | USB_RT_DEVICE,USB_DESCR_DEVICE,\
				0, 0, sizeof(pBfr), pBfr, &actLen) != OK)
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

	    pCp210xDev->nodeId = nodeId;
	    pCp210xDev->configuration = configuration;
	    pCp210xDev->interface = interface;
	    pCp210xDev->vendorId = vendorId;
	    pCp210xDev->productId = productId;

	    /* Continue fill the newly allocate structure,If there's an error,
		 * there's nothing we can do about it, so skip the device and return immediately.
		 */
		if((createDev(pCp210xDev)) != OK){
			break;
		}

		pCp210xDev->connected = TRUE; /*将设备标记为已连接*/
		copy_buf();
		initOutIrp();
	    break;

	case USBD_DYNA_REMOVE:

		if(pCp210xDev->connected == FALSE) break;
		if(pCp210xDev->nodeId != nodeId) break;
		pCp210xDev->connected = FALSE;
		destoryDev (pCp210xDev);
		break;
	}

	OSS_MUTEX_RELEASE(cp210xMutex);
	return OK;
}

/*初始化cp210x驱动*/
STATUS cp210xDrvInit(void)
{
	if(cp210xDrvNum > 0){
		printf("cp210x already initialized.\n");
		return ERROR;
	}

	pCp210xDev = &one_dev;

	cp210xDrvNum = iosDrvInstall(NULL,NULL,cp210xDevOpen,cp210xDevClose,
				NULL,cp210xDevWrite,NULL);

/*检查是否为驱动安装空间*/
	if(cp210xDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		return ERROR;
	}

	if( iosDevAdd(&pCp210xDev->cp210xDevHdr,CP210X_NAME,cp210xDrvNum) != OK){
		printf("Unable to create COM device.\n");
		iosDrvRemove (cp210xDrvNum, TRUE);
		cp210xDrvNum = -1;
		return ERROR;
	}

	cp210xMutex = NULL;
	cp210xHandle = NULL;

	if(OSS_MUTEX_CREATE(&cp210xMutex) != OK) {
		iosDrvRemove (cp210xDrvNum, TRUE);
		cp210xDrvNum = -1;
		return ERROR;
	}

	if(usbdClientRegister (CP210X_CLIENT_NAME, &cp210xHandle) != OK) {
		printf("Unable to usbdClientRegister\n");
		iosDrvRemove (cp210xDrvNum, TRUE);
		cp210xDrvNum = -1;
		return ERROR;
	}

	if(usbdDynamicAttachRegister(cp210xHandle,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,TRUE,
					(USBD_ATTACH_CALLBACK)cp210xAttachCallback)!= OK)
	{
		printf("Unable to usbdDynamicAttachRegister\n");
		iosDrvRemove (cp210xDrvNum, TRUE);
		cp210xDrvNum = -1;
		usbdClientUnregister(cp210xHandle);
		return ERROR;
	}

	return OK;
}

/*关闭cp210x 设备驱动*/

STATUS cp210xDrvUnInit (void)
{
    if (!cp210xDrvNum)
    	return OK;

    /* unregister */
    if (usbdDynamicAttachUnRegister (cp210xHandle,USBD_NOTIFY_ALL,\
    		USBD_NOTIFY_ALL,USBD_NOTIFY_ALL,
			(USBD_ATTACH_CALLBACK)cp210xAttachCallback)\
			!= OK){
    	printf("usbdDynamicAttachUnRegister () returned ERROR\n");
        return ERROR;
    }

    if(usbdClientUnregister(cp210xHandle) != OK){
    	printf("usbdClientUnregister() returned ERROR\n");
    	return ERROR;
    }

    if(iosDevDelete((DEV_HDR *)pCp210xDev) != OK){
    	printf("iosDevDelete() returned ERROR\n");
    	return ERROR;
    }

    /*删除驱动*/
    if (iosDrvRemove (cp210xDrvNum, TRUE) != OK){
    	printf("iosDrvRemove () returned ERROR\n");
        return ERROR;
    }

    cp210xDrvNum = -1;

	destoryDev (pCp210xDev);
    return (OK);
}


LOCAL int cp210xDevWrite(CP210X_DEV *pCp210xDev, char *buffer,UINT32 nBytes)
{
	int n;

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

	if(pCp210xDev->connected == FALSE) {
		OSS_MUTEX_RELEASE(cp210xMutex);
		return n;
	}

	if(pCp210xDev->outIrpInUse == FALSE) {
		pCp210xDev->outIrpInUse = TRUE;
		copy_buf();
		initOutIrp();
	}

	OSS_MUTEX_RELEASE(cp210xMutex);
	return n;
}

/*打开cp210x设备，*/
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev, char *name, int flags,int mode)
{
	(pCp210xDev->numOpen)++;
	return ((int) pCp210xDev);
}

/*关闭cp210x设备*/
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev)
{
	--(pCp210xDev->numOpen);
	return ((int)pCp210xDev);
}

LOCAL STATUS cp210xChangeBaudRate(CP210X_DEV *pCp210xDev,UINT32 baud)
{
	UINT16 length = sizeof(UINT32);
	UINT16 actLength;
	if(usbdVendorSpecific(cp210xHandle,pCp210xDev->nodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_BAUDRATE,0,pCp210xDev->interface,length,(pUINT8)&baud,&actLength) != OK){
		return ERROR;
	}

	UINT32 baudNow = 0;

	if(usbdVendorSpecific(cp210xHandle,pCp210xDev->nodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_BAUDRATE,0,pCp210xDev->interface,
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


LOCAL STATUS cp210xSetTermiosPort(CP210X_DEV *pCp210xDev)
{
	UINT16 length = 0;
	UINT16 actLength;

	if(usbdVendorSpecific(cp210xHandle,pCp210xDev->nodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_IFC_ENABLE,UART_ENABLE,
			pCp210xDev->interface,0,NULL,&actLength) != OK)
		return ERROR;

	/*set default baudrate to 115200*/
	UINT32 baud = 115200;

	if(cp210xChangeBaudRate(pCp210xDev,baud) != OK){
		printf("set default baudrate to 115200 failed!\n");
		return ERROR;
	}

//设置为8个数据位，1个停止位，没有奇偶校验，没有流控。
	UINT16 bits;
	length = sizeof(UINT16);

	bits &= ~(BITS_DATA_MASK | BITS_PARITY_MASK | BITS_STOP_MASK);
	bits |= (BITS_DATA_8 | BITS_PARITY_NONE | BITS_STOP_1);

	if(usbdVendorSpecific(cp210xHandle,pCp210xDev->nodeId,REQTYPE_HOST_TO_INTERFACE,
			CP210X_SET_LINE_CTL,bits,
			pCp210xDev->interface,0,NULL,&actLength) != OK)
		return ERROR;

/*显示当前bits设置*/
	UINT16 bitsNow;
	length = sizeof(bitsNow);
	if(usbdVendorSpecific(cp210xHandle,pCp210xDev->nodeId,REQTYPE_INTERFACE_TO_HOST,
			CP210X_GET_LINE_CTL,0,pCp210xDev->interface,
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

STATUS createDev(CP210X_DEV *pCp210xDev)
{
	USB_CONFIG_DESCR 	* pCfgDescr;
	USB_INTERFACE_DESCR * pIfDescr;
	USB_ENDPOINT_DESCR 	* pOutEp;
	UINT8 pBfr[USB_MAX_DESCR_LEN];
	UINT8 * pScratchBfr;
	UINT16 actLen;
	UINT16 maxPacketSize;
	UINT16 ifNo = 0;
	pScratchBfr = pBfr;

	if (usbdDescriptorGet (cp210xHandle, pCp210xDev->nodeId,\
				USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION,\
				0, 0, USB_MAX_DESCR_LEN, pBfr, &actLen) != OK){
		return ERROR;
	}

	if ((pCfgDescr = usbDescrParse (pBfr, actLen,\
			USB_DESCR_CONFIGURATION)) == NULL){
		return ERROR;
	}

	pCp210xDev->configuration = pCfgDescr->configurationValue;

	while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,&actLen,\
			USB_DESCR_INTERFACE))!= NULL){
		/*look for the interface indicated in the pCp210xDev structure*/
		if (ifNo == pCp210xDev->interface)
			break;
		ifNo++;
	}

	if (pIfDescr == NULL){
		return ERROR;
	}

	pCp210xDev->interfaceAltSetting = pIfDescr->alternateSetting;

	if ((pOutEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_OUT))\
			== NULL){
		return ERROR;
	}

	pCp210xDev->outEpAddr = pOutEp->endpointAddress;

	if (usbdConfigurationSet (cp210xHandle, pCp210xDev->nodeId,\
				pCfgDescr->configurationValue,\
				pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT)\
				!= OK){
		return ERROR;
	}

	usbdInterfaceSet(cp210xHandle,pCp210xDev->nodeId,pCp210xDev->interface,\
			pIfDescr->alternateSetting);

	maxPacketSize = *((pUINT8) &pOutEp->maxPacketSize) | \
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

	if(usbdPipeCreate(cp210xHandle,pCp210xDev->nodeId,pOutEp->endpointAddress,\
			pCfgDescr->configurationValue,pCp210xDev->interface,\
			USB_XFRTYPE_BULK,USB_DIR_OUT,maxPacketSize,\
			0,0,&pCp210xDev->outPipeHandle)!= OK){
		return ERROR;
	}

	if(cp210xSetTermiosPort(pCp210xDev) != OK){
		return ERROR;
	}

	/* Clear HALT feauture on the endpoints */
	if ((usbdFeatureClear (cp210xHandle, pCp210xDev->nodeId, USB_RT_ENDPOINT,\
			USB_FSEL_DEV_ENDPOINT_HALT, (pOutEp->endpointAddress & 0xFF)))\
			!= OK){
		return ERROR;
	}

	return OK;
}



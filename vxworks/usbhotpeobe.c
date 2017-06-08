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



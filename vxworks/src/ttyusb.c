#include "ttyusb.h"

/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
//0x41 = 0x 0(����Ϊ�������豸) 10(����Ϊ�����Զ���) 00001(���շ�Ϊ�ӿ�)
#define REQTYPE_INTERFACE_TO_HOST	0xc1
// 0xc1 = 0x 1(�豸������) 10(����Ϊ�����Զ���) 00001(���շ�Ϊ�ӿ�???�����Ľӿ�??)
#define REQTYPE_HOST_TO_DEVICE	0x40
//0x41 = 0x 0(�������豸) 10(����Ϊ�����Զ���) 00000(���շ�Ϊ�豸)
#define REQTYPE_DEVICE_TO_HOST	0xc0
//0xc0 = 0x 1(�豸������) 10(�����Զ���) 00000(���շ�Ϊ�豸)

/* Config request codes */
/*�����Զ�����豸���󣬲��Ǳ�׼���豸����*/
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
	/*I/O��ϵͳ�ڵ��õײ���������ʱ����ʹ���豸�ṹ��Ϊ�������õĵ�һ������������豸�ṹ�ǵײ������Զ���ģ�
	 * ���Ա���ײ�Ӳ���豸�Ĺؼ������������Զ�����豸�ṹ���뽫�ں˽ṹDEV_HDR��Ϊ�Զ����豸�ṹ�ĵ�һ��������
	 * �ýṹ��I/O��ϵͳʹ�ã�ͳһ��ʾϵͳ�ڵ������豸����I/O��ϵͳ�����е������Զ�����豸�ṹ��ΪDEV_HDRʹ�ã�
	 * DEV_HDR֮����ֶ��ɸ��Ծ�����������н��ͺ�ʹ�ã��ں˲���������Щ�ֶεĺ��塣*/
	DEV_HDR 		usbttyDev;
	SIO_CHAN 		*pSioChan;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	USB_TTY_NODE *          pusbttyNode;
	SEL_WAKEUP_LIST         selWakeupList;   /*Ϊ��������б�*/
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
	/*USB-in-data����ָ��洢����ȡ���ݵĻ�������*/
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
LOCAL int 					usbttyDrvNum = -1; 	//����ע���������
LOCAL USBD_CLIENT_HANDLE 	usb_ttyusbdClientHandle = NULL;
LOCAL LIST					usbttyList;			//ϵͳ���е�USBTTY�豸
LOCAL SEM_ID 				usbttyListMutex;  	//�����б����ź���
LOCAL SEM_ID				usbttyMutex;			//�����ź���





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
	printf("Out of avilable USB_TTY number.Check USB_TTY_DEVICE��NUM\n");
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
/*ΪSIOͨ������USB_TTY_DEV�豸*/
LOCAL STATUS usbttyDevFind(SIO_CHAN *pChan,USB_TTY_DEV **ppusbttyDev)
{
	USB_TTY_NODE 	*pusbttyNode = NULL ;	/*ָ��USB�豸�ڵ�*/
	USB_TTY_DEV		*pTempDev;				/*ָ��USB_TTY_DEV*/

	if(pChan == NULL)
		return ERROR;

	/*��ֹ����ģ�����*/
	USB_TTY_LIST_SEM_TAKE(WAIT_FOREVER);

	/*���������豸*/
	for(pusbttyNode = (USB_TTY_NODE *) lstFirst (&usbttyList);
			pusbttyNode != NULL;
	        pusbttyNode = (USB_TTY_NODE *) lstNext ((NODE *) pusbttyNode)){
		pTempDev = pusbttyNode->pusbttyDev;
		/*�ҵ�������*/
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
				/*����USB-TTY�豸*/
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

/*		//�������豸�ĵĽṹ�岢�����ʼ��
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

����UBSTTY�豸
		if(usbttyDevCreate(usbttyName)== OK){
			printf("create /usbtty/0 device OK!\n");
		}else{
			printf("create /usbtty/0 device failed\n");
			free(pusbttyDev);
			return;
		}*/

	}

	if(attachCode == USB_TTY_REMOVE){
		/*���ҹ����豸*/
		if (usbttyDevFind (pChan, &pusbttyDev) != OK){
			printf ("usbttyDevFind could not find channel 0x%d", (UINT32)pChan);
			return;
			}

		/*ɾ���豸*/
		usbttyDevDelete (pusbttyDev);
		sprintf (usbttyName, "%s%d", USB_TTY_NAME, freeusbttyNum(pChan));

		if (usbttySioChanUnlock (pChan) != OK){
			printf("usbttySioChanUnlock () returned ERROR\n");
			return;
		}
		printf ("USB-TTY %s removed\n", usbttyName);
	}
}


/*����USBTTYϵͳ�豸���ײ�����xxxxDevCreate�����豸������ʱ����ÿһ���豸�ṹ�ж�Ҫ�洢���豸��������(iosDrvInstall�ķ���ֵ),
 * I/O��ϵͳ���Ը����豸�б��е��豸�ṹ��ֱ�Ӳ�ѯ�����豸��Ӧ����������*/
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


	/*���豸��ӵ�ϵͳ���豸�б���*/
	if((status = iosDevAdd (&pusbttyDev->usbttyDev,name,usbttyDrvNum)) != OK){
		free(pusbttyDev);
		printf("Unable to create ttyusb device.");
		return status;
	}
	return OK;
}




/*��ʼ��USBTTY����*/
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

	/*��ʼ������*/
	lstInit(&usbttyList);

	usbttyDrvNum = iosDrvInstall(NULL,usbttyDevDelete,usbttyDevOpen,usbttyDevClose,\
			usbttyDevRead,usbttyDevWrite,usbttyDevIoctl);


/*����Ƿ�Ϊ������װ�ռ�*/
	if(usbttyDrvNum <= 0){
		errnoSet (S_ioLib_NO_DRIVER);
		printf("There is no more room in the driver table\n");
		goto ERROR_HANDLER;
	}

	if(usbttyDevInit() == OK){
		printf("usbttyDevInit() returned OK\n");
		if(usbttyDynamicAttachRegister(usbttyDrvAttachCallback,(void *) NULL) != OK){
			 printf ("usbKeyboardDynamicAttachRegister() returned ERROR\n");
			 usbttyDevShutdown();
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



/*дUSBTTY�豸*/
int usbttyDevWrite(USB_TTY_DEV *pusbttyDev, char *buffer,UINT32 nBytes)
{
	printf("here is in usbttyDevWrite function\n");
	int nByteWrited = 2;
//	status = CreateTransfer(pusbttyDev->outPipeHandle);
	//TODO
	return nByteWrited;
}

/*��USBTTY�����豸���ڴ˴����豸��������*/
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


/*�ر�USBTTY�����豸*/
int usbttyDevClose(USB_TTY_DEV *pusbttyDev)
{
	if(!(pusbttyDev->numOpened)){
		sioIoctl(pusbttyDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pusbttyDev);
}


int usbttyDevIoctl(USB_TTY_DEV *pusbttyDev,/*��ȡ���豸*/
		int request/*�������*/,
		void *arg/*����*/)
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

	default:	/*	����IO���ƺ���*/
		status = (sioIoctl(pusbttyDev->pSioChan,request,arg));
	}
	return status;
}



/*��ȡ����*/
int usbttyDevRead(USB_TTY_DEV *pusbttyDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//�洢���е������ֽ�
	UINT32 		bytesToBeRead  	=0;		//��ȡ�ֽ���
//	UINT32 		i = 0;					//����ֵ

	/*����ֽ���Ч������ERROR*/
	if(nBytes == 0){
		return ERROR;
	}

	/*���û�����*/
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
	/*���ض�ȡ���ֽ���*/
	return bytesToBeRead;
}





/*ΪUSBTTYɾ����ϵͳ�豸*/
STATUS usbttyDevDelete(USB_TTY_DEV *pusbttyDev)
{
	if(usbttyDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}

	/*
	 *��ֹ���ͷ�ѡ��Ļ����б�����selWakeUpAll���Է������ݡ�
	 *��ɾ�������������ϵȴ�������
	 */
	selWakeupAll (&pusbttyDev->selWakeupList, SELREAD);

	/*�����ź���*/
	if ((pusbttyDev->usbttySelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pusbttyDev->selWakeupList) > 0){
		/*�б�����δ��������񣬵ȴ��ź����ս��б�ǰ�ͷ�����*/
		/*�ȴ��ź�*/
		semTake (pusbttyDev->usbttySelectSem, WAIT_FOREVER);
	}

	/*ɾ���ź���*/

	semDelete (pusbttyDev->usbttySelectSem);

	selWakeupListTerm(&pusbttyDev->selWakeupList);

	/*��I/Oϵͳɾ���豸*/
	iosDevDelete(&pusbttyDev->usbttyDev);

	/*���б�ɾ��*/
	USB_TTY_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&usbttyList, (NODE *) pusbttyDev->pusbttyNode);
	USB_TTY_LIST_SEM_GIVE;

	/*�豸�ͷ��ڴ�*/
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
// bLength:18(�豸��������ռ���ֽ�Ϊ18���ֽ�)��     bDescriptorType:1(�豸����������Ϊ1����ʾ����ʲô������)       bcdUSB:0x110(��ʾ��ѭUSB1.1�淶)
// bdeviceClass:0(��Ϊ0����ÿһ���ӿڶ�Ҫ˵��������Ϣ����)  bdeviceSubClass:0(����deviceClassΪ0���������Ϊ0)   bdeviceProtocol:0(Ϊ0˵������ʹ�õĻ���һ���豸��ĳһ�����͵�Э��)
// bMaxPacketSize:64(���ڶ˵����������ߴ�)   idVendor:0x10c4(��Ӧ��ID��USB����)   idProduct:0xea60(��ƷID�ɳ��ҷ���)
// bcdDevice�� 0x100(�����Ʊ���С����ʽ���豸���к�)    iMancufacturer:1(�����������̵��ַ���������������) iProduct:2(����������Ʒ���ַ���������������)  iSerial:3(���������豸���кŵ��ַ���������������)
// bNumConfiguration:1(���ܵ�������)


	status = usbdDescriptorGet(usb_ttyusbdClientHandle,pusbttyDev->usbttyNodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
	if(status == OK){
#ifdef DESC_PRINT
				printf("\nConfiguration:\n");
				printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
				printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
						pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
						pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
#endif
// actual length:32(���ص����ݵ��ܳ���Ϊ32�ֽ�);     buffer length is:38(�ṩ�Ŀռ�Ϊ38���ֽ�);   bLength:9(������������ռ�ĳ���Ϊ9 );  DescriptorType:2(��ʾ��������ָ�����һ���豸��;
// wTotalLength:32(��ʾ������+�ӿ�+�˵����������ܳ���)   bNumInterface:1(�������½ӿ���Ϊ1��)��          bConfigurationValue:1(�����õ�ֵΪ1);   iConfiguration��0(�������������õ��ַ���������������)
// bmAttributes:0x80(D7Ϊ1��ʾ���߹��磬D6Ϊ0��ʾ�����Թ��磬D5Ϊ0��ʾ��֧��Զ�̻��ѣ�D4-D0����);    MaxPower:50(������С����2mAΪ��λ��50��ʾ100mA)
				pusbttyDev->configuration      = 0;
				pusbttyDev->configurationValue = pusb_ttyConfigDescr->configurationValue;
				pusbttyDev->maxPower		   = pusb_ttyConfigDescr->maxPower;

#ifdef DESC_PRINT
		//��ӡInterface������
			printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
						pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
						pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
#endif
// Length:9(�ӿ����������ܳ���Ϊ9);  descriptorType:4(��ʾָ����ǽӿ�)��   interfaceNumber��0(��ʾ�ӿڵ���Ŀ����0ֵָ���ڸ�������ͬʱ֧�ֵĽӿ������е�����)��  alternateSetting��0(����Ϊ��һ��������ʶ���Ľӿ�ѡ��ɹ��滻������)
// numEndpoint:2(�ýӿ���֧�ֵĶ˵�����������0�˵�)  interfaceClass:0xFF(����λΪ0xFF���ӿ����;��ɹ�Ӧ�����ض����������е���ֵ�ͱ�������USB���з���)��  interfaceSubClass:0x0(��ʾ��ֵ��USB���з���)��
// interfaceProtocol:0x0(��ʾ�ýӿ���û��ʹ��һ���ض����͵�Э��)��  interfaceIndex:2(��ֵ���������ӿڵ��ַ�����������������)
				pusbttyDev->altSetting = pusb_ttyInterfaceDescr->alternateSetting;
				pusbttyDev->interface  = 0;
				pusbttyDev->interfaceNumber = pusb_ttyInterfaceDescr->interfaceNumber;

#ifdef DESC_PRINT
		//��ӡEndPoint 1 ��������
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
						pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:������Ӧ��λΪ0��bit0..3:�˵��)��  Attributes:0x2(bit0..1:��������0x10��ʾ��������);  MaxPacketSize:64(������ߴ�)��  Interval:0(������ѯ�����������Ϳ��ƴ���ӦΪ0)
//�˵�1Ϊ����˵�
				pusbttyDev->inEndpointAddress = pusb_ttyEndpointDescr1->endpointAddress ;
				pusbttyDev->epMaxPacketSize   = pusb_ttyEndpointDescr1->maxPacketSize;


#ifdef DESC_PRINT
		//��ӡEndPoint 2 ��������
				printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
						pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
						pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
#endif
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:������Ӧ��λΪ0��bit0..3:�˵��)��  Attributes:0x2(bit0..1:��������0x10��ʾ��������);  MaxPacketSize:64(������ߴ�)��  Interval:0(������ѯ�����������Ϳ��ƴ���ӦΪ0)
//�˵�2Ϊ����˵�

				pusbttyDev->outEndpointAddress = pusb_ttyEndpointDescr2->endpointAddress;

				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
	}else{
				free(pBfrReceiveData);
				pBfrReceiveData = NULL;
			}

	return status;
}

/*
*usbkeyboard init
*/
//================ global define============
#define TX_BUF_NUM 			0x10000
#define USB_KBD_MUTEX_TAKE(tmout)		semTake(usbKbdMutex,(int)(tmout))
//usbKbdMutex 是一个互斥信号量。
#define USB_KBD_MUTEX_GIVE(tmout)		semGive(usbKbdMutex)
#define USB_KBD_LIST_SEM_TAKE(tmout)	semTake(usbKbdListMutx,(int)(tmout))
#define USB_KBD_LIST_SEM_GIVE(tmout)	semGive(usbKbdListMutx)
#define KBD_NAME_LEN_MAX				100
#define USB_KBD_NAME					"/usb/Kb"
#define CTRL_Z							26	//^Z的ASCII码
#define QUEUE_SIZE						8   //循环队列的大小
#ifndef USB_MAX_KEYBOARDS
#define USB_MAX_KEYBOARDS 				5
#endif
/*
USB键盘连接道系统shell或bootAppShell宏检查，
INCLUDE_USB_KEYBOARD_SHELLATTACH和INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTCH是互斥的
*/
#ifdef INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH

#ifndef INCLUDE_PC_CONSOLE
#error INCLLUDE_PC_CONSOLE must be defined in USB bootAppShell keyboard attachmode
#endif //include PC_CONSOLE

#ifdef INCLUDE_USB_KEYBOARD_SHELL_ATTACH
#error INCLUDE_USB_KEYBOARD_SHELL_ATTACH must not be defined in USB bootAppShell attach mode
#endif

#endif

#ifdef INCLUDE_USB_KEYBOARD_SHELL_ATTACH

#ifdef INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH
#error INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH must not be defined in USB target shell attach mode
#endif

#endif

//==========================类型定义====================
typedef struct usb_kbd_node
{
	NODE 				node;
	struct usb_kbd_dev	*pUsbKbdDev
}USB_KBD_NODE;

typedef struct usb_kbd_dev
{
	DEV_HDR		ioDev;
	SIO_CHAN	*pSioChan;
	UINT16		numOpen;
	UINT32		bufSize;
	UCHAR		*buff;
	USB_KBD_NODE *pUsbKbdNode;
	SEL_WAKEUP_LIST selWakeupList //未解决任务列表
	SEM_ID		kbdSelectSem;	//选则处理程序信号量
#ifdef INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH
	TY_DEV		*pTyDev		//USB键盘TTY设备
#endif
}USB_KBD_DEV,*pUSB_KBD_DEV;

typedef struct kbdQueue
{
	char 		queueData[QUEUE_SIZE];
	INT8		queueFront;
	INT8		queueRear;
	SEM_ID		queueSem;
} KBD_QUEUE,*pKBD_QUEUE;

//==========================外部声明=====================
//==========================模块变量=====================
LOCAL SIO_CHAN	*pKbdDevArray[USB_MAX_KEYBOARDS];
LOCAL SEM_ID	usbKbdMutex;	//互斥信号量
LOCAL SEM_ID	usbKbdListMutx; //保护列表互斥信号量
LOCAL LIST 		usbKbdList;		//系统所有USB键盘
LOCAL int 		usbKBdDrvNum =0;//驱动数目

#ifdef INCLUDE_USB_KEYBOARD_SHELL_ATTACH
LOCAL int 		usbOrigFd;		//原始shell软驱
LOCAL int 		usbShellFd;		//usb shell 键盘软驱
LOCAL char 		*usbShellKeyboard = NULL;//usb shell 键盘名称
#else
LOCAL BOOL 		enterPressed;
#endif

LOCAL KBD_QUEUE kbdQueueData;//键盘发送数据库

#ifdef INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH//为USB键盘输入PC控制台函数
IMPORT TY_DEV	*pcConDevBind(int,FUNCPTR,void *);
#endif

//===========================前向声明======================
LOCAL void 		usbKbdDrvAttachCallback(void *arg,SIO_CHAN *pChan, UINT16 attachCode);
LOCAL int 		usbKbdRead(USB_KBD_DEV *pUsbKbdDev,char *buffer,UINT32 nBytes);
LOCAL int 		usbKbdClose(USB_KBD_DEV *pUsbKbdDev);
LOCAL int 		usbKbdWrite(USB_KBD_DEV *pUsbKbdDev,char *buffer,UINT32 nBytes);
LOCAL int 		usbKbdOpen(USB_KBD_DEV *pUsbKbdDev,char *name, int flags, int mode);
LOCAL int 		usbKbdIoctl(USB_KBD_DEV *pUsbKbdDev, int request,void *arg);
LOCAL STATUS 	usbKbdDevFind(SIO_CHAN *pChan, USB_KBD_DEV **ppUsbKbdDev);
LOCAL STATUS 	usbKbdDevDelete(USB_KBD_DEV *pUsbKbdDev);
LOCAL STATUS 	usbKbdRcvCalback(void *putRxCharArg, char nextChar);
LOCAL VOID 		putDataInQueue(char data);
LOCAL char 		getDataFromQueue(void);
LOCAL BOOL 		isDataQueueEmpty(void);
LOCAL UINT16	queueBytesCount(void);

//===========================全局变量＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝＝
//===========================实现============================
#ifdef INCLUDE_USB_KEYBOARD_SHELL_ATTACH
LOCAL STATUS 	usbKbdDevFind(SIO_CHAN *pChan, USB_KBD_DEV **ppUsbKbdDev);
LOCAL SIO_CHAN 	*findUsbKeyboard(void);

LOCAL SIO_CHAN	*findUsbKeyboard(void) //查找可用ＵＳＢ键盘
{
	int index;
	for (index = 0; index < USB_MAX_KEYBOARDS; ++index){
		if (pKbdDevArray[index] !=  NULL){
			return (pKbdDevArray[index]);
		}
	}
	return (NULL);
}

LOCAL STATUS redirectShellInput(USB_KBD_DEV *pDev) //目标机ｓｈｅｌｌ输入更改到ＵＳＢ键盘
{		//由于只有一个ｓｈｅｌｌ实例，如果键盘已经连接到ｓｈｅｌｌ，跳过。
	if (usbShellKeyboard == NULL){
		printf("\nOpening %s for console device",pDev->ioDev.name);
		usbShellFd = open(pDev->ioDev.name, 0 ,0);
		if (usbShellFd <= 0){
			printf("Could not open fd for shell redirection\n");
			return FALSE;
		}

		usbOrigFd = ioGlobalStdGet(0);
		//关闭控制台键盘
		shellTerminate(shellFirst(0));//????????是什么
		if (OK != shellGnericInit(SHELL_FIRST_CONFIG, SHELL_STACK_SIZE,
			NULL,NULL,TRUE,SHELL_SECURE,usbShellFd,usbOrigFd,usbOrigFd)){
			printf("Could not start a new shell\n");
			return FALSE;			
		}else{
			usbShellKeyboard = (char *)malloc(strlen(pDev->ioDev.name)+1);
			if (usbShellKeyboard == NULL){
				printf("\nMemory allocation failure - shell kbd undefined.");
			}else{
				strcpy(usbShellKeyboard,pDev->ioDev.name);
			}
		}
		return TRUE;
	}
	return TRUE;
}

//目标机ｓｈｅｌｌ输入存储到原始设备，检查移除设备是否连接到ｓｈｅｌｌ，如果是，将ｓｈｅｌｌ输入存储到原始默认设备
LOCAL STATUS restoreShellInput(USB_KBD_DEV *pDev)
{
	if (strcmp(pDev->ioDev.name,usbShellKeyboard) == 0){
		//关闭控制台键盘
		shellTerminate(shellFirst(0));
		if(OK != shellGnericInit(SHELL_FIRST_CONFIG, SHELL_STACK_SIZE,NULL, NULL, TRUE,
			SHELL_SECURE,usbOrigFd,usbOrigFd,usbOrigFd)){
			printf("could not start a new shell\n");
			return FALSE;
		}
		free(usbShellKeyboard);
		usbShellKeyboard = NULL;
	}
	return TRUE;
}
#endif


//为ＵＳＢ键盘创建道系统设备
STATUS usbKbdDevCreate(char *name,SIO_CHAN *pSioChan)
{
	USB_KBD_NODE 	*pUsbKbdNode = NULL;//指向设备节点
	USB_KBD_DEV 	*pUsbKbdDev  = NULL;//指向ＵＳＢ设备
	STATUS 			status 		 = ERROR;

	if (pSioChan == (SIO_CHAN *)ERROR){
		printf("pSioChan is error\n");
		return ERROR;
	}
	//为设备分配内存
	if ((pUsbKbdDev = (USB_KBD_DEV *)calloc(1,sizeof(USB_KBD_DEV))) == NULL){
		printf("calloc returned NULL - out of memory\n");
		return ERROR;
	}
	pUsbKbdDev->pSioChan = pSioChan
	//为节点分配内存病输入数据
	pUsbKbdNode = (USB_KBD_NODE *)calloc(1,sizeof(USB_KBD_NODE));
	//读取有用的信息
	pUsbKbdNode->pUsbKbdDev = pUsbKbdDev;
	pUsbKbdDev->pUsbKbdNode = pUsbKbdNode;

	//在ｂｏｏｔｒｏｍ中绑定键盘到ｐｃ控制台。
#ifdef INCLUDE_USB_KEYBOARD_BOOTSHELL_ATTACH
	pUsbKbdDev->pTyDev = pcConDevBind(PC_CONSOLE,NULL,NULL);
#endif

	//添加设备到ＩＯ系统
	if ((status = iosDevAdd()))
	{
		/* code */
	}


}

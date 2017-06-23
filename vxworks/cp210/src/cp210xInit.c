/*
 * cp210xInit.c
 *
 *  Created on: 2017-6-12
 *      Author: sfh
 */

#include "cp210xLib.h"

typedef struct cp210x_node{
	NODE 	node;
	struct cp210x_dev *pCp210xDev;
}CP210X_NODE;

typedef struct cp210x_dev
{
	//I/O子系统在调用底层驱动函数时，将使用设备结构作为函数调用的第一个参数，这个设备结构是底层驱动自定义的，
	// * 用以保存底层硬件设备的关键参数。对于自定义的设备结构必须将内核结构DEV_HDR作为自定义设备结构的第一个参数。
	// * 该结构被I/O子系统使用，统一表示系统内的所有设备，即I/O子系统将所有的驱动自定义的设备结构作为DEV_HDR使用，
	// * DEV_HDR之后的字段由各自具体的驱动进行解释和使用，内核并不关心这些字段的含义。
	DEV_HDR 		cp210xDevHdr;
	SIO_CHAN 		*pSioChan;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	CP210X_NODE *          pCp210xNode;
	SEL_WAKEUP_LIST         selWakeupList;   //为解决任务列表
	SEM_ID                  cp210xSelectSem;
//TODO
}CP210X_DEV;




//#define     DESC_PRINT
/*
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
*/


#define CP210X_DEVICE_NUM			10
#define CP210X_NAME 				"/COM"
#define CP210X_NAME_LEN_MAX 		50

STATUS status;
LOCAL SIO_CHAN 				*pCp210xDevArray[CP210X_DEVICE_NUM];
//CP210X_DEV 				*pCp210xDev = NULL;
LOCAL int 					cp210xDrvNum = -1; 	//驱动注册的驱动号
LOCAL LIST					cp210xList;			//系统所有的USBTTY设备
LOCAL SEM_ID 				cp210xListMutex;  	//保护列表互斥信号量
LOCAL SEM_ID				cp210xInitMutex;			//互斥信号量





#define CP210X_LIST_SEM_TAKE(tmout)	semTake(cp210xListMutex,(int)(tmout))
#define	CP210X_LIST_SEM_GIVE			semGive(cp210xListMutex)

/**************************函数前向声明**********************************************/
LOCAL int cp210xDevRead(CP210X_DEV *pCp210xDev,  char *buffer,  UINT32 nBytes);
LOCAL int cp210xDevWrite(CP210X_DEV *pCp210xDev, char *buffer,UINT32 nBytes);
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev,  char *name,  int flags,int mode);
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev);
LOCAL int cp210xDevIoctl(CP210X_DEV *pCp210xDev,  int request,  void *arg);

LOCAL STATUS cp210xDevDelete(CP210X_DEV *pCp210xDev);
LOCAL STATUS cp210xDevFind(SIO_CHAN *pChan,CP210X_DEV **ppCp210xDev);

LOCAL STATUS cp210xDevCreate(char *,SIO_CHAN *);


/*为设备获取下一个单元编号*/
LOCAL int getCp210xNum(SIO_CHAN *pChan)
{
	int index;
	for(index =0;index<CP210X_DEVICE_NUM;index++){
		if(pCp210xDevArray[index] == 0){
			pCp210xDevArray[index] = pChan;
			return (index);
		}
	}
	printf("Out of avilable USB_TTY number.Check CP210X_DEVICE＿NUM\n");
	return (-1);
}

/*为设备释放下一个单元编号*/
LOCAL int freeCp210xNum(SIO_CHAN *pChan)
{
	int index;
	for (index=0; index < CP210X_DEVICE_NUM; index++)
	{
		if (pCp210xDevArray[index] == pChan){
			pCp210xDevArray[index] = NULL;
			return (index);
		}
	}
	printf("Unable to locate USB_TTY pointed to by channel 0x%X.\n",pChan);
	return(-1);
}

//==============================================================================//
/*为SIO通道查找CP210X_DEV设备*/
LOCAL STATUS cp210xDevFind(SIO_CHAN *pChan,CP210X_DEV **ppCp210xDev)
{
	CP210X_NODE 	*pCp210xNode = NULL ;
	CP210X_DEV		*pTempDev;

	if(pChan == NULL)
		return ERROR;


	CP210X_LIST_SEM_TAKE(WAIT_FOREVER);

	for(pCp210xNode = (CP210X_NODE *) lstFirst (&cp210xList);
			pCp210xNode != NULL;
	        pCp210xNode = (CP210X_NODE *) lstNext ((NODE *) pCp210xNode)){
		pTempDev = pCp210xNode->pCp210xDev;

		if (pTempDev->pSioChan == pChan){
			* (UINT32 *) ppCp210xDev = (UINT32) pTempDev;
			CP210X_LIST_SEM_GIVE;
			return (OK);
		}
	}
	CP210X_LIST_SEM_GIVE;
	return (ERROR);
}

//==============================================================================//
void cp210xRcvCallback(){
	printf("IN cp210xRcvCallback()");
}


/*接收来自SIO驱动的回叫信号，不管设备是否链接，都将调用该函数*/
LOCAL void cp210xAttachCallback(void *arg,SIO_CHAN *pChan,UINT16 attachCode)
{
	int cp210xUnitNum;
	CP210X_DEV *pCp210xDev = NULL;
	char cp210xName[CP210X_NAME_LEN_MAX];

	if(attachCode == CP210X_ATTACH){
		printf("device detected success!\n");
		if(cp210xSioChanLock(pChan) != OK){
			printf("cp210xSioChanLock() returned ERROR\n");
		}else{
			cp210xUnitNum = getCp210xNum(pChan);
			if(cp210xUnitNum >= 0){
				sprintf(cp210xName,"%s%d",CP210X_NAME,cp210xUnitNum);

				if(cp210xDevCreate(cp210xName,pChan) != OK){
					printf("cp210xDevCreate() returned ERROR\n");
					return ;
				}
				printf("cp210xDevCreate() returned OK\n");
			}else{
				printf("Excessive!! check CP210X_DEVICE_NUM");
				return;
			}

			if(cp210xDevFind(pChan,&pCp210xDev) != OK){
				printf("cp210xDevFind() returned ERROR\n");
				return;
			}

			if((*pChan->pDrvFuncs->callbackInstall)(pChan,SIO_CALLBACK_PUT_RCV_CHAR,
					(STATUS (*)(void *,...))cp210xRcvCallback,
					(void *)pCp210xDev)!=OK){
				printf("usbKbdRcvCallback() failed to install\n");
				return;
			}

			printf("cp210x attached as %s\n",cp210xName);
		}
	}

	if(attachCode == CP210X_REMOVE){
//		查找关联设备
		if (cp210xDevFind (pChan, &pCp210xDev) != OK){
			printf ("cp210xDevFind could not find channel 0x%d", (UINT32)pChan);
			return;
		}

//		删除设备
		cp210xDevDelete (pCp210xDev);
		sprintf (cp210xName, "%s%d", CP210X_NAME, freeCp210xNum(pChan));

		if (cp210xSioChanUnlock (pChan) != OK){
			printf("cp210xSioChanUnlock () returned ERROR\n");
			return;
		}
		printf ("%s removed\n", cp210xName);
	}
}


/*创建cp210x系统设备，底层驱动xxxxDevCreate进行设备创建的时候，
 * 在每一个设备结构中都要存储该设备的驱动号(iosDrvInstall的返回值),
 * I/O子系统可以根据设备列表中的设备结构体直接查询到该设备对应的驱动程序*/
LOCAL STATUS cp210xDevCreate(char *name,SIO_CHAN * pSioChan )
{
	printf("IN cp210xDevCreate\n");
	CP210X_NODE 	*pCp210xNode 	= NULL;
	CP210X_DEV		*pCp210xDev 	= NULL;
	STATUS 			status 			= ERROR;

	if(pSioChan == (SIO_CHAN *)ERROR){
		printf("pSioChan is ERROR\n");
		return ERROR;
	}

	if((pCp210xDev = (CP210X_DEV *)calloc(1,sizeof(CP210X_DEV))) == NULL){
		printf("calloc returned NULL - out of memory\n");
		return ERROR;
	}
	pCp210xDev->pSioChan = pSioChan;

//	为节点分配内存并输入数据
	pCp210xNode = (CP210X_NODE *) calloc (1, sizeof (CP210X_NODE));

//	读取有用信息
	pCp210xNode->pCp210xDev = pCp210xDev;
	pCp210xDev->pCp210xNode = pCp210xNode;

//	将设备添加到系统的设备列表当中
	if((status = iosDevAdd (&pCp210xDev->cp210xDevHdr,name,cp210xDrvNum)) != OK){
		free(pCp210xDev);
		cp210xSioChanUnlock(pSioChan);
		printf("Unable to create COM device.\n");
		return status;
	}

	printf("iosDevAdd success\n");

//	初始化列表（记录驱动上未解决的任务列表）
	selWakeupListInit(&pCp210xDev->selWakeupList);

//	为列表增加节点

	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstAdd (&cp210xList, (NODE *) pCp210xNode);
	CP210X_LIST_SEM_GIVE;

//	以支持方式启动设备
//	sioIoctl (pCp210xDev->pSioChan, SIO_MODE_SET, (void *) SIO_MODE_INT);
	return OK;
}


/*为cp210x删除道系统设备*/

STATUS cp210xDevDelete(CP210X_DEV *pCp210xDev)
{
	if(cp210xDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


//	 *终止并释放选择的唤醒列表，调用selWakeUpAll，以防有数据。
//	 *需删除所有在驱动上等待的任务。

	selWakeupAll (&pCp210xDev->selWakeupList, SELREAD);

//	创建信号量
	if ((pCp210xDev->cp210xSelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pCp210xDev->selWakeupList) > 0){
//		列表上有未解决的任务，等待信号在终结列表前释放任务
//		等待信号
		semTake (pCp210xDev->cp210xSelectSem, WAIT_FOREVER);
	}

//	删除信号量

	semDelete (pCp210xDev->cp210xSelectSem);

	selWakeupListTerm(&pCp210xDev->selWakeupList);

//	从I/O系统删除设备
	iosDevDelete(&pCp210xDev->cp210xDevHdr);

//	从列表删除
	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&cp210xList, (NODE *) pCp210xDev->pCp210xNode);
	CP210X_LIST_SEM_GIVE;

//	设备释放内存
	free(pCp210xDev->pCp210xNode);
	free(pCp210xDev);
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

	cp210xDrvNum = iosDrvInstall(NULL,cp210xDevDelete,cp210xDevOpen,cp210xDevClose,
			cp210xDevRead,cp210xDevWrite,cp210xDevIoctl);


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
			 cp210xDevShutdown();
			 goto ERROR_HANDLER;
		}
	}else{
		printf("cp210xDevInit() returned ERROR\n");
		goto ERROR_HANDLER;
	}

	int i;
	for(i=0;i< CP210X_DEVICE_NUM;++i){
		pCp210xDevArray[i]=NULL;
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
    if (cp210xDynamicAttachUnRegister (cp210xAttachCallback,
    		NULL) != OK){
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





/*写USBTTY设备*/
LOCAL int cp210xDevWrite(CP210X_DEV *pCp210xDev, char *buffer,UINT32 nBytes)
{
	printf("here is in cp210xDevWrite function,nBytes is:%d\n",nBytes);
	int nByteWrited =0;

	while(buffer[nByteWrited] != '\0' && nByteWrited < nBytes){
		pCp210xDev->pSioChan->pDrvFuncs->pollOutput(pCp210xDev->pSioChan,buffer[nByteWrited]);
		nByteWrited++;
	}

	printf("nByteWrited:%d",nByteWrited);

	return nByteWrited;
}

/*读取数据*/
LOCAL int cp210xDevRead(CP210X_DEV *pCp210xDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//存储队列的所有字节
	UINT32 		bytesToBeRead  	=0;		//读取字节数
//	UINT32 		i = 0;					//计数值

	/*如果字节无效，返回ERROR*/
	if(nBytes < 1){
		return ERROR;
	}

//	设置缓冲区
	memset(buffer,0,nBytes);
	sioIoctl(pCp210xDev->pSioChan,SIO_MODE_GET,&arg);

	if(arg == SIO_MODE_POLL){
		return pCp210xDev->pSioChan->pDrvFuncs->pollInput(pCp210xDev->pSioChan,buffer);
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

/*打开cp210x设备，*/
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev, char *name, int flags,int mode)
{
	printf("OK here im in cp210xDevOpen!\n");
	pCp210xDev->numOpen++; /*增加打开路径数*/
	sioIoctl(pCp210xDev->pSioChan,SIO_OPEN,NULL);

	/*从循环缓冲区删除数据*/
	return ((int) pCp210xDev);
}


/*关闭cp210x设备*/
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev)
{
	/*是否没有开放的通道*/
	if(!(--pCp210xDev->numOpen)){
		sioIoctl(pCp210xDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pCp210xDev);
}


LOCAL int cp210xDevIoctl(CP210X_DEV *pCp210xDev,//读取的设备
		int request,
		void *arg)
{
	printf("IN cp210xDevIoctl.request is:%d, arg is :%d\n",request,*(int *)arg);
	int status = OK;
	switch(request){

	case FIOBAUDRATE:

		status = sioIoctl(pCp210xDev->pSioChan,SIO_BAUD_SET,arg);
		break;

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
		status = (sioIoctl(pCp210xDev->pSioChan,request,arg));
	}
	return status;
}



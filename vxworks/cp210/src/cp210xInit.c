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
	//I/O��ϵͳ�ڵ��õײ���������ʱ����ʹ���豸�ṹ��Ϊ�������õĵ�һ������������豸�ṹ�ǵײ������Զ���ģ�
	// * ���Ա���ײ�Ӳ���豸�Ĺؼ������������Զ�����豸�ṹ���뽫�ں˽ṹDEV_HDR��Ϊ�Զ����豸�ṹ�ĵ�һ��������
	// * �ýṹ��I/O��ϵͳʹ�ã�ͳһ��ʾϵͳ�ڵ������豸����I/O��ϵͳ�����е������Զ�����豸�ṹ��ΪDEV_HDRʹ�ã�
	// * DEV_HDR֮����ֶ��ɸ��Ծ�����������н��ͺ�ʹ�ã��ں˲���������Щ�ֶεĺ��塣
	DEV_HDR 		cp210xDevHdr;
	SIO_CHAN 		*pSioChan;

	UINT16                  numOpen;
	UINT32                  bufSize;
	UCHAR *                 buff;
	CP210X_NODE *          pCp210xNode;
	SEL_WAKEUP_LIST         selWakeupList;   //Ϊ��������б�
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
LOCAL int 					cp210xDrvNum = -1; 	//����ע���������
LOCAL LIST					cp210xList;			//ϵͳ���е�USBTTY�豸
LOCAL SEM_ID 				cp210xListMutex;  	//�����б����ź���
LOCAL SEM_ID				cp210xInitMutex;			//�����ź���





#define CP210X_LIST_SEM_TAKE(tmout)	semTake(cp210xListMutex,(int)(tmout))
#define	CP210X_LIST_SEM_GIVE			semGive(cp210xListMutex)

/**************************����ǰ������**********************************************/
LOCAL int cp210xDevRead(CP210X_DEV *pCp210xDev,  char *buffer,  UINT32 nBytes);
LOCAL int cp210xDevWrite(CP210X_DEV *pCp210xDev, char *buffer,UINT32 nBytes);
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev,  char *name,  int flags,int mode);
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev);
LOCAL int cp210xDevIoctl(CP210X_DEV *pCp210xDev,  int request,  void *arg);

LOCAL STATUS cp210xDevDelete(CP210X_DEV *pCp210xDev);
LOCAL STATUS cp210xDevFind(SIO_CHAN *pChan,CP210X_DEV **ppCp210xDev);

LOCAL STATUS cp210xDevCreate(char *,SIO_CHAN *);


/*Ϊ�豸��ȡ��һ����Ԫ���*/
LOCAL int getCp210xNum(SIO_CHAN *pChan)
{
	int index;
	for(index =0;index<CP210X_DEVICE_NUM;index++){
		if(pCp210xDevArray[index] == 0){
			pCp210xDevArray[index] = pChan;
			return (index);
		}
	}
	printf("Out of avilable USB_TTY number.Check CP210X_DEVICE��NUM\n");
	return (-1);
}

/*Ϊ�豸�ͷ���һ����Ԫ���*/
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
/*ΪSIOͨ������CP210X_DEV�豸*/
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


/*��������SIO�����Ļؽ��źţ������豸�Ƿ����ӣ��������øú���*/
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
//		���ҹ����豸
		if (cp210xDevFind (pChan, &pCp210xDev) != OK){
			printf ("cp210xDevFind could not find channel 0x%d", (UINT32)pChan);
			return;
		}

//		ɾ���豸
		cp210xDevDelete (pCp210xDev);
		sprintf (cp210xName, "%s%d", CP210X_NAME, freeCp210xNum(pChan));

		if (cp210xSioChanUnlock (pChan) != OK){
			printf("cp210xSioChanUnlock () returned ERROR\n");
			return;
		}
		printf ("%s removed\n", cp210xName);
	}
}


/*����cp210xϵͳ�豸���ײ�����xxxxDevCreate�����豸������ʱ��
 * ��ÿһ���豸�ṹ�ж�Ҫ�洢���豸��������(iosDrvInstall�ķ���ֵ),
 * I/O��ϵͳ���Ը����豸�б��е��豸�ṹ��ֱ�Ӳ�ѯ�����豸��Ӧ����������*/
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

//	Ϊ�ڵ�����ڴ沢��������
	pCp210xNode = (CP210X_NODE *) calloc (1, sizeof (CP210X_NODE));

//	��ȡ������Ϣ
	pCp210xNode->pCp210xDev = pCp210xDev;
	pCp210xDev->pCp210xNode = pCp210xNode;

//	���豸��ӵ�ϵͳ���豸�б���
	if((status = iosDevAdd (&pCp210xDev->cp210xDevHdr,name,cp210xDrvNum)) != OK){
		free(pCp210xDev);
		cp210xSioChanUnlock(pSioChan);
		printf("Unable to create COM device.\n");
		return status;
	}

	printf("iosDevAdd success\n");

//	��ʼ���б���¼������δ����������б�
	selWakeupListInit(&pCp210xDev->selWakeupList);

//	Ϊ�б����ӽڵ�

	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstAdd (&cp210xList, (NODE *) pCp210xNode);
	CP210X_LIST_SEM_GIVE;

//	��֧�ַ�ʽ�����豸
//	sioIoctl (pCp210xDev->pSioChan, SIO_MODE_SET, (void *) SIO_MODE_INT);
	return OK;
}


/*Ϊcp210xɾ����ϵͳ�豸*/

STATUS cp210xDevDelete(CP210X_DEV *pCp210xDev)
{
	if(cp210xDrvNum <= 0){
		errnoSet(S_ioLib_NO_DRIVER);
		return ERROR;
	}


//	 *��ֹ���ͷ�ѡ��Ļ����б�����selWakeUpAll���Է������ݡ�
//	 *��ɾ�������������ϵȴ�������

	selWakeupAll (&pCp210xDev->selWakeupList, SELREAD);

//	�����ź���
	if ((pCp210xDev->cp210xSelectSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY)) == NULL)
		return ERROR;

	if (selWakeupListLen (&pCp210xDev->selWakeupList) > 0){
//		�б�����δ��������񣬵ȴ��ź����ս��б�ǰ�ͷ�����
//		�ȴ��ź�
		semTake (pCp210xDev->cp210xSelectSem, WAIT_FOREVER);
	}

//	ɾ���ź���

	semDelete (pCp210xDev->cp210xSelectSem);

	selWakeupListTerm(&pCp210xDev->selWakeupList);

//	��I/Oϵͳɾ���豸
	iosDevDelete(&pCp210xDev->cp210xDevHdr);

//	���б�ɾ��
	CP210X_LIST_SEM_TAKE (WAIT_FOREVER);
	lstDelete (&cp210xList, (NODE *) pCp210xDev->pCp210xNode);
	CP210X_LIST_SEM_GIVE;

//	�豸�ͷ��ڴ�
	free(pCp210xDev->pCp210xNode);
	free(pCp210xDev);
	return OK;
}



/*��ʼ��cp210x����*/
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

	/*��ʼ������*/
	lstInit(&cp210xList);

	cp210xDrvNum = iosDrvInstall(NULL,cp210xDevDelete,cp210xDevOpen,cp210xDevClose,
			cp210xDevRead,cp210xDevWrite,cp210xDevIoctl);


/*����Ƿ�Ϊ������װ�ռ�*/
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

/*�ر�cp210x �豸����*/

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

    /*ɾ������*/
    if (iosDrvRemove (cp210xDrvNum, TRUE) != OK){
    	printf("iosDrvRemove () returned ERROR\n");
        return (ERROR);
    }

    cp210xDrvNum = -1;

    /*ɾ�������ź���*/
    if (semDelete (cp210xInitMutex) == ERROR){
        printf("semDelete (cp210xInitMutex) returned ERROR\n");
        return (ERROR);
    }

    if (semDelete (cp210xListMutex) == ERROR){
        printf("semDelete (cp210xListMutex) returned ERROR\n");
        return (ERROR);
    }


    /*�ر�*/

    if (cp210xDevShutdown () != OK){
        printf("cp210xDevShutDown() returned ERROR\n");
        return (ERROR);
    }

    return (OK);
}





/*дUSBTTY�豸*/
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

/*��ȡ����*/
LOCAL int cp210xDevRead(CP210X_DEV *pCp210xDev,char *buffer, UINT32 nBytes)
{
	int 		arg = 0;
//	UINT32  	bytesAvailable	=0; 	//�洢���е������ֽ�
	UINT32 		bytesToBeRead  	=0;		//��ȡ�ֽ���
//	UINT32 		i = 0;					//����ֵ

	/*����ֽ���Ч������ERROR*/
	if(nBytes < 1){
		return ERROR;
	}

//	���û�����
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
//	���ض�ȡ���ֽ���
	return bytesToBeRead;
}

/*��cp210x�豸��*/
LOCAL int cp210xDevOpen(CP210X_DEV *pCp210xDev, char *name, int flags,int mode)
{
	printf("OK here im in cp210xDevOpen!\n");
	pCp210xDev->numOpen++; /*���Ӵ�·����*/
	sioIoctl(pCp210xDev->pSioChan,SIO_OPEN,NULL);

	/*��ѭ��������ɾ������*/
	return ((int) pCp210xDev);
}


/*�ر�cp210x�豸*/
LOCAL int cp210xDevClose(CP210X_DEV *pCp210xDev)
{
	/*�Ƿ�û�п��ŵ�ͨ��*/
	if(!(--pCp210xDev->numOpen)){
		sioIoctl(pCp210xDev->pSioChan,SIO_HUP,NULL);
	}

	return ((int)pCp210xDev);
}


LOCAL int cp210xDevIoctl(CP210X_DEV *pCp210xDev,//��ȡ���豸
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

	default:		//����IO���ƺ���
		status = (sioIoctl(pCp210xDev->pSioChan,request,arg));
	}
	return status;
}



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
LOCAL UINT16 initCount = 0;			/* Count of init nesting */
LOCAL LIST_HEAD cp210xDevList;			/* linked list of CP210X_LIB_DEV */
LOCAL LIST_HEAD reqList;				/* Attach callback request list */

LOCAL MUTEX_HANDLE cp210xMutex;   			/* mutex used to protect internal structs */

LOCAL UINT32 usbCp210xIrpTimeOut = CP210X_IRP_TIME_OUT;

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




STATUS cp210xSioChanLock(CP210X_LIB_DEV *pChan)
{
    if ( pChan == NULL)
    	return ERROR;

    CP210X_LIB_DEV *pLibDev = (CP210X_LIB_DEV *)pChan;
    pLibDev->lockCount++;
    return OK;
}



STATUS cp210xLibDevUnlock(CP210X_LIB_DEV *pChan)
{
    int status = OK;

    if(pChan == NULL)
    	return ERROR;
    CP210X_LIB_DEV *pLibDev = (CP210X_LIB_DEV *)pChan;

    OSS_MUTEX_TAKE (cp210xMutex, OSS_BLOCK);

    if (pLibDev->lockCount == 0){
    	status = S_cp210xLib_NOT_LOCKED;
    }else{
    	/* If this is the last lock and the underlying cp210x device
    	 * is no longer connected, then dispose of the device.
    	 */
    	if (--pLibDev->lockCount == 0 && !pLibDev->connected)
    		destoryLibDev ((CP210X_LIB_DEV *)pLibDev);
    }

    OSS_MUTEX_RELEASE (cp210xMutex);
    return (ossStatus(status));
}


LOCAL int cp210xTxStartUp(CP210X_LIB_DEV *pChan)
{
	printf("IN cp210xTxStartUp\n");

	CP210X_LIB_DEV *pLibDev = (CP210X_LIB_DEV *)pChan;
	int status = OK;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);
	if(initOutIrp(pLibDev)!= OK)
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

LOCAL int cp210xCallbackInstall(CP210X_LIB_DEV *pChan,int callbackType,
		STATUS(*callback)(void *tmp,...),void *callbackArg)
{
	printf("IN cp210xCallbackInstall now\n");

	CP210X_LIB_DEV * pLibDev = (CP210X_LIB_DEV *)pChan;

	switch (callbackType){

	case SIO_CALLBACK_GET_TX_CHAR:
		pLibDev->getTxCharCallback = (STATUS (*)()) (callback);
		pLibDev->getTxCharArg = callbackArg;
		return OK;

	case SIO_CALLBACK_PUT_RCV_CHAR:
		pLibDev->putRxCharCallback = (STATUS (*)()) (callback);
		pLibDev->putRxCharArg = callbackArg;
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
LOCAL int cp210xPollOutput(CP210X_LIB_DEV *pChan,char outChar)
{
//	printf("IN cp210xPollOutput");

	CP210X_LIB_DEV *pLibDev = (CP210X_LIB_DEV *)pChan;
//	printf("%c\n",outChar);
	if(initOutIrp(pLibDev)!= OK)
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

LOCAL char nextInChar( CP210X_LIB_DEV *pLibDev )
{
    char inChar = pLibDev->inQueue [pLibDev->inQueueOut];

    if (++pLibDev->inQueueOut == CP210X_Q_DEPTH)
	pLibDev->inQueueOut = 0;

    pLibDev->inQueueCount--;

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
LOCAL int cp210xPollInput(CP210X_LIB_DEV *pChan,char *thisChar)
{
	printf("IN cp210xPollInput\n");

	CP210X_LIB_DEV * pLibDev = (CP210X_LIB_DEV *)pChan;
	int status = OK;

	/*validate parameters*/
	if(thisChar == NULL)
		return EIO;

	OSS_MUTEX_TAKE(cp210xMutex,OSS_BLOCK);

	/*Check if the input queue is empty.*/

	if (pLibDev->inQueueCount == 0)
		status = EAGAIN;
	else{
		/*Return a character from the input queue.*/
		*thisChar = nextInChar (pLibDev);
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
    CP210X_LIB_DEV *pChan,	    /* device to control */
    int request,	/* request code */
    void *someArg	/* some argument */
    ){

	printf("IN cp210xIoctl\n");
	CP210X_LIB_DEV *pLibDev = (CP210X_LIB_DEV *) pChan;
    int arg = (int) someArg;

    switch (request){

	case SIO_BAUD_SET:

		if( cp210xChangeBaudRate(pLibDev,*((int *)arg)) != OK){
			printf("can not change to this baudrate\n");
			return ERROR ;
		}
		break;


	case SIO_BAUD_GET:
	{
		UINT32 baud = 0;
		UINT16 length = sizeof(UINT32);

		if(usbdVendorSpecific(cp210xHandle,cp210xNodeId,REQTYPE_INTERFACE_TO_HOST,
				CP210X_GET_BAUDRATE,0,pLibDev->interface,
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

	    pLibDev->mode = *(int *)arg;


	    return OK;


	case SIO_MODE_GET:

	    /* Return current driver operating mode for channel
	     * 返回系统当前的工作模式(INT or POLL)*/


	    *((int *) arg) = pLibDev->mode;
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
		pLibDev->scanMode = arg;
		return OK;*/


/*
	case SIO_KYBD_MODE_GET:
		*(int *)someArg = pLibDev->scanMode;
		return OK;
*/

/*
	 case SIO_KYBD_LED_SET:
	{
	UINT8 ledReport;

	  update the channel's information about the LED state

	pLibDev->numLock = (arg & SIO_KYBD_LED_NUM) ? SIO_KYBD_LED_NUM : 0;

	pLibDev->capsLock = (arg & SIO_KYBD_LED_CAP) ?
				SIO_KYBD_LED_CAP : 0;
	pLibDev->scrLock = (arg & SIO_KYBD_LED_SCR) ?
				SIO_KYBD_LED_SCR : 0;


	     * We are relying on the SIO_KYBD_LED_X macros matching the USB
	     * LED equivelants.


	    ledReport = arg;

	     set the LED's

	    setLedReport (pLibDev, ledReport);

	    return OK;
	    }*/



     /*    case SIO_KYBD_LED_GET:
	     {
	     int tempArg;

    	     tempArg = (pLibDev->capsLock) ? SIO_KYBD_LED_CAP : 0;
    	     tempArg |= (pLibDev->scrLock) ? SIO_KYBD_LED_SCR : 0;
    	     tempArg |= (pLibDev->numLock) ? SIO_KYBD_LED_NUM : 0;

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


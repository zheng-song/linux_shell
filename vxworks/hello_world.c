/*
 * helloworld.c
 *
 *  Created on: 2017-4-17
 *      Author: sfh
 */
#include <vxWorks.h>
#include <tasklib.h>
#include <syslib.h>
#include <logLib.h>
#include <vmlib.h>
#include <sioLib.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <selectLib.h>
#include <string.h>

#define USB_TEST_CLASS 0xff
#define USB_TEST_SUB_CLASS 0x00
#define USB_TEST_DRIVE_PROTOCOL 0xff

LOCAL void usbTestDevAttachCallback(
	USBD_NODE_ID nodeid,
	UINT16 attachAction,
	UINT16 configuration
	UINT16 interface,
	UINT16 deviceClass
	UINT16 deviceSubClass
	UINT16 deviceProtocol)

LOCAL　STATUS usbTestPhysDevCreate(
	USB_NODE_ID nodeid,
	UINT16 configuration,
	UINT16 interface)

void testUSBInit (void)
{
    printf("hello USB test");
    if(usbdInit() != USBHST_SUCCESS){
    	exit(ERROR);
    }
    printf("usbdInit successful\n");

    if (usbHubInit() != 0){
    	exit(ERROR);
    }
    printf("usbdHubInit successful\n");

    






    if(usbdInitialize() != OK)
    {
    	printf("can not init usbd\n");
    	exit(0);
    }

    if(usbdClientRegister("USB_TEST"，&usbdClientHandle) != OK || 
    	usbdDynamicAttachRegister(usbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,
    		USB_TEST_DRIVE_PROTOCOL,usbTestDevAttachCallback) != OK){

    }

}










//  STATUS ghDrv(int arg);
//  STATUS ghDevCreate(char *devName,int arg);
//  int ghOpen(void);
//  int ghRead(void);
//  int ghWrite(void);
//  int ghIoctl(void);
//  int ghClose(void);

//  typedef struct
//  {
//  	DEV_HDR devHdr;
//  	int ix;
//  	BOOL Opened;
//  	BOOL RdReady;
//  	BOOL WrReady;
//  }gh_DEV;

//  LOCAL int ghDrvNum = -1;

//  STATUS ghDrv(int arg)
//  {
//  	if (ghDrvNum != -1){
//  		printf ("Keyboard already initilaized.\n");
//  		return (OK);
//  	}

//  	printf("\n ghDrv: #1.Initial hardware,use arg:%d\n",arg);
//  	if ((ghDrvNum = iosDrvInstall(ghOpen,NULL,ghOpen,ghClose,ghRead,ghWrite,ghIoctl)) == ERROR){
//  		return (ERROR);
//  	}
// //===================================================




// //===================================================
//  	printf("#1.3 install programs ok, DrvNum=%d\n",ghDrvNum);
//  	return (OK);
//  }


//  STATUS ghDevCreate(char *devName,int arg)
//  {
//  	gh_DEV *pghDev;
//  	if (ghDrvNum < 0){
//  		errno = S_ioLib_NO_DRIVER;
//  		return (ERROR);
//  	}
//  	pghDev = (gh_DEV *)malloc(sizeof(gh_DEV));
//  	if (pghDev == NULL){
//  		return ERROR;
//  	}
//  // 	bzero(pghDev,sizeof(gh_DEV));
//  	memset(pghDev, 0, sizeof(gh_DEV));
//   	pghDev->WrReady=1; //Initial gh_dev
//  	printf("\n ghDevCreate:device initial,arg=%d\n",arg);
//  	if (iosDevAdd(&pghDev->devHdr,devName,ghDrvNum) == ERROR){
//   		free((char *)pghDev);
//  		free((gh_DEV *)pghDev);
//  		return (ERROR);
//  	}

//  	return (OK);
//  }

//  int ghOpen(void)
//  {
//  	logMsg("\n 3.1ghOpen has entered\n",1,2,3,4,5,6);
//  	printf("\nghOpen:initial when opened\n");
//  	return (OK);
//  }

//  int ghRead()
//  {
//  	printf("\n4.Read Function\n");
//  	return (OK);
//  }

//  int ghWrite()
//  {
//  	printf("\n5.write function\n");
//  	return (OK);
//  }

//  int ghIoctl()
//  {
//  	printf("\n6.Ioctl function\n");
//  	return (OK);
//  }

//  int ghClose()
//  {
//  	printf("\n7.close function\n");
//  	return (OK);
//  }
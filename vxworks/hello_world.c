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
   printf("hello USB test\n");
//==========================================================

   // if (usbInit() != OK){
   // 	printf("usbInit failed!\n" );
   // 	return ERROR;
   // }else{
   // 	printf("usb init OK\n");
   // }
//不需要这一步，usbdInitialize()中已经包含了usbInit()这一步。
//==========================================================

   if (usbdInitialize() != OK){
   	printf("Failed to initialize USBD\n");
   	return ERROR;
   }else{
   	printf("USBD initialize success\n");
   	}

//挂接过程
   	UINT8 busNO;
   	UINT8 deviceNo;
   	UINT8 funcNo;
   	PCI_CFG_HEADER pciCfgHdr;
   	GENERIC_HANDLE uhciAttachToken;
   	HCD_EXEC_FUNC hcdExecFunc;

   	if (!usbPciClassFind(UHCI_CLASS,UHCI_SUBCLASS,UHCI_PGMIF,0,&busNo,&deviceNo,&funcNo)){
   		return ERROR;
   	}
   	printf("usbPciClassFind success!\n");

   	usbPciConfigHeaderGet(busNo,deviceNo,funcNo,&pciCfgHdr);
   	/* Attach the UHCI HCD to the USBD.The function usbHcdUhci() is exported by usbHcdUhciLib.*/
   	if (usbHcdAttach(usbHcdUhciExec,&pciCfgHdr,&uhciAttachToken) != OK){
   		return ERROR;
   	}
   	printf("usbHcdAttach OK!\n");


// STATUS usbdHcdAttach(HCD_EXEC_FUNC hcdExecFunc,pVOID param, pGENERIC_HANDLE pattachToken);
   // usbHcdAttach()

// STATUS usbdclientregister(pCHAR pClientName, 
//		pUSBD_CLIENT_HANDLE pClientHandle /*Client hdl returned by USBD*/ );
   pUSBD_CLIENT_HANDLE pUSB_TTYHandle;
  if (usbdClientRegister("USB/TTY",&pUSB_TTYHandle) != OK){
  	printf("usbdclientregister failed\n");
  	return ERROR;
  }
  printf("usbdclientregister OK!\n");

   if (usbdDynamicAttachRegister(pUSB_TTYHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,
   	USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback ) != OK){
   printf("usbdDynamicAttachRegister failed !\n");   	
   return ERROR;
   }
   printf("usbdDynamicAttachRegister success!\n");



    // if(usbdInitialize() != OK)
    // {
    // 	printf("can not init usbd\n");
    // 	exit(0);
    // }

    // if(usbdClientRegister("USB_TEST"，&usbdClientHandle) != OK || 
    // 	usbdDynamicAttachRegister(usbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,
    // 		USB_TEST_DRIVE_PROTOCOL,usbTestDevAttachCallback) != OK){

    // }

}



//==========================USB INIT

UINT16 cmdUsbInit()/*Initialize USBD*/
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;
	/*if alreadly initialized,just show a warning*/
	if (initialized){
		printf("Already initialized.\n");
		return RET_CONTINUE;
	}
	/*Initialize the USBD*/
	s=usbdInitialize();
	printf("usbdInitialize returned %d\n",s);
	if (s == OK){
		s=usbdClientRegister(PGM_NAME,&usbdClientHandle);
		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usbdClientHandle = 0x%x\n",(UINT32)usbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
			}
			if (s == OK)
				initialized = TRUE;
		}
	}
	if (s!=OK){
		printf("Initialized failed!\n");
	}
	return RET_CONTINUE;
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
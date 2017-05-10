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
//
//extern STATUS usbHcdInit(UINT8 hcdFlag);

//IMPORT STATUS usbInit (void);

#define USB_TEST_CLASS USBD_NOTIFY_ALL
#define USB_TEST_SUB_CLASS USBD_NOTIFY_ALL
#define USB_TEST_DRIVE_PROTOCOL USBD_NOTIFY_ALL

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

}

UINT16 cmdUsbInit()/*Initialize USBD*/
{
	UINT16 usbdVersion;
	char usbdMfg[USBD_NAME_LEN+1];
	UINT16 s;
	USBD_CLIENT_HANDLE usbdClientHandle=NULL;
	/*if alreadly initialized,just show a warning*/
//	if (initialized){
//		printf("Already initialized.\n");
//		return RET_CONTINUE;
//	}
	/*Initialize the USBD*/
	s=usbdInitialize();
	printf("usbdInitialize returned %d\n",s);
	if (s == OK){
		s=usbdClientRegister("usb/tty",&usbdClientHandle);
		printf("usbdClientRegister returned %d\n",s);
		if (s == OK){
			printf("usbdClientHandle = 0x%x\n",(UINT32)usbdClientHandle);
			/*Display the USBD version*/
			if ((s=usbdVersionGet(&usbdVersion,usbdMfg)) != OK){
				printf("usbdVersionGet() returned %d\n",s);
			}else{
				printf("USBD version=0x%5.4x\n",usbdVersion);
				printf("USBD mfg='%s'\n",usbdMfg);
				printf("usbd initilized OK!\n");
			}
		}

		s=usbdDynamicAttachRegister(usbdClientHandle,USB_TEST_CLASS,USB_TEST_SUB_CLASS,USB_TEST_DRIVE_PROTOCOL,TRUE,usbTestDevAttachCallback);
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


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



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
#include <String.h>
//#include <malloc.h>
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
//		printf("usb_ttyusbdClientHandle = 0x%x\n",(UINT32)usb_ttyusbdClientHandle); // 0xa2c400
//		printf("the attachCallBack nodeid is:%p\n",nodeid);							//0x103
		printf("the attachCallBack configuration is:%d\n",configuration);			//0
		printf("the attachCallBack interface is:%d\n",interface);					//0

/*	usbdDescriptorGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeID, UINT8 requestType,
 * 				UNIT8 descriptorType, UINT8 descriptorIndex, UINT16 languageId,UINT16 bfrLen,
 * 				pUINT8 pBfr, pUINT16 pActLen)----a client uses this function to retrieve a descriptor
 * 	from USB device identified by nodeID, requestType specifies whether the feature is related to the device,
 * 	to an interface,or to an endpoint as follow: USB_RT_DEVICE(device)/USB_RT_INTERFACE(interface)/USB_RT_ENDPOINT(endpoint)
 * 	reauestType also specify if the request is standard,class-specific,etc..,as: USB_RT_STANDARD(standard)/USB_RT_CLASS(class-specific)/USB_RT_VENDOR(vendor-specific)
 * 	For example,USB_RT_STANDARD|USB_RT_DEVICE in requestType specifies a standard request.
 * 		the descriptorType specifies the type of the descriptor to be retrieved and must be one of the following values:
 * 		USB_DESCR_DEVICE(specifies the device descriptor)/USB_DESCR_CONFIG(configuration desc)/USB_DESCR_STRING(string desc)/
 * 		USB_DESCR_INTERFACE(interface desc)/USB_DESCR_ENDPOINT(endpoint desc)
 *怎么知道descriptorIndex是多少？--只能为0
 *
 */
		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("Device Descriptor:\n");
			printf("the actual length of usb_ttyDeviceDescr is:%d;  the buffer Len is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}


		USB_CONFIG_DESCR usb_ttyConfigDescr;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("the actual length of usb_ttyConfiguratiuon is:%d;  the buffer Len is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
		}else{
			printf("get configuration descriptor failed!\n");
		}


/*
 * usbdConfigurationSet()对配置进行设置。
 */
//		UINT16 configValue=0x0001;
//		UINT16 maxPower = 50;
//		status = usbdConfigurationSet(usb_ttyusbdClientHandle,nodeid,configValue,maxPower);
//		if(status == OK){
//			printf("set configuration success!\n");
//		}else{
//			printf("set configuration failed!status is %d\n",status);
//		}


//		status = usbdInterfaceSet(usb_ttyusbdClientHandle,nodeid,0,0);
//		if(status == OK){
//			printf("interface 0 set OK\n");
//		}else{
//			printf("interface 0 set failed,status is:%d\n",status);
//		}


/*
 * get interface descriptor
 */
		USB_INTERFACE_DESCR usb_ttyInterfaceDescr;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_INTERFACE,USB_DESCR_INTERFACE,0,0,USB_INTERFACE_DESCR_LEN,(pUINT8)&usb_ttyInterfaceDescr,&ActLen);
		if(status == OK){
			printf("\ninterface descriptor:\n  the descriptor type is %d;the interface number is %d;\nthe alternateSetting is %x;the number of endpoint is %d; the interface index is %d\n",\
					usb_ttyInterfaceDescr.descriptorType,usb_ttyInterfaceDescr.interfaceNumber,usb_ttyInterfaceDescr.alternateSetting,usb_ttyInterfaceDescr.numEndpoints,usb_ttyInterfaceDescr.interfaceIndex);
		}else{
			printf("get interface descriptor failed!status is %d\n",status);
		}

//		USB_ENDPOINT_DESCR usb_ttyEndPointDescr;
//		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_ENDPOINT,USB_DESCR_ENDPOINT,0,0,USB_ENDPOINT_DESCR_LEN,(pUINT8)&usb_ttyEndPointDescr,&ActLen);
//		if(status == OK){
//			printf("  EndpointDescriptor 0:\n");
//
//		}else{
//			printf("get EndPoint descriptor failed!\n");
//		}
//
//		memset(&usb_ttyEndPointDescr,0,sizeof(USB_ENDPOINT_DESCR));
//		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_ENDPOINT,USB_DESCR_ENDPOINT,1,0,USB_ENDPOINT_DESCR_LEN,(pUINT8)&usb_ttyEndPointDescr,&ActLen);
//		if(status == OK){
//			printf("  EndpointDescriptor 1:\n");
//		}else{
//			printf("get EndPoint descriptor failed!\n");
//		}
/*	usbdConfigurationGet()--Gets USB configuration for a device
 *  	STATUS usbdConfigurationGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeID,pUINT16 pConfiguration)
 *  ----this function returns the currently selected configuration for the device or hub indicated by nodeID
 *  the current configuration value is returned in the low byte of pConfiguration,the high byte is currently
 *  reserved 0
 */
	UINT16 Configuration;
	status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&Configuration);
	if(status == OK){
		printf("get usb configuration OK,the configuration is:0x%x\n",Configuration); //0x1
		//0x1 表示的是什么意思？
	}else{
		printf("get usb configuration failed!\n");
	}


/*	STATUS usbdInterfaceGet(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,UINT16 interfaceIndex,pUINT16 pAlternateSetting)
 * this function allows a client to query the current alternate setting for a device’s interface,nodeId and interfaceIndex specify
 * the device and interface to be queried.respectively.pAlternateSetting points to a UINT16 variable in which the alternate setting will be stored upon return.
 *	?如何知道interfaceIndex是多少？
 */
	UINT16 interfaceIndex=0;
	pUINT16 pAlternateSetting=NULL;
	status 	= usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,pAlternateSetting);
	if(status == OK){
		printf("get interface set OK\nthe value of *pAlternateSetting is :%d\n",*pAlternateSetting);//the value is 19456
	}else{
		printf("get interface set failed!\n");
	}



/*
 * Create pipe -- STATUS usbdPipeCreate(USBD_CLIENT_HANDLE clientHandle,USBD_NODE_ID nodeId,UINT16 endpoint,UINT16 configuration,UINT16 interface,\
 *  UINT16 transferType,UINT16 direction,UINT16 maxPayload,UINT32 bandwidth,UINT16 serviceInterval，pUSBD_PIPE_HANDLE pPipeHandle)
 * 该函数用来建立一个client和设备的endpoint之间进行交换数据的管道，nodeid和endpoint用来标识相互连接的设备和端点，configuration和interface用来
 * 标识和该管道相关的配置和接口。（usbd使用这个信息来保持对这个管道相关的配置事件的跟踪）
 * 	管道会用到的数据传输类型为以下几种：
 * 	1.USB_XFRTYPE_CONTROL（控制管道（信息流））2.USB_XFRTYPE_ISOCH(isochronous 传输管道(数据流))3.USB_XFRTYPE_INTERRUPT(中断传输管道(数据流))4.USB_XFRTYPE_BLUK(块传输管道(数据流))
 *
 * 		方向为以下几种：1.USB_DIR_IN(从设备到host)2.USB_DIR_OUT(host到device)3.USB_DIR_INOUT(Data moves bidirectionally (message pies only))
 * 	maxPayload指定此端点的最大数据负载，通常USB设备会在每一个端点的描述符中声明最大负载。client通常获取这些描述然后取得合适的负载值
 * 	bandwidth指定管道的带宽需求，对于control和bulk管道，这个参数是应该为0，对于中断管道，这个参数应该传递每个被传输的帧的byte数量。对于isochronous管道，这个参数应该传递每秒传输的byte数量
 * 	serviceInterval指定管道每毫秒最大的潜能？？？这个参数只用于中断管道，对于其他管道为0
 * 	成功创建管道会返回一个管道句柄，client需要使用管道句柄来标识这个管道以调用USBD的Transfer函数。如果没有足够的带宽来创建管道（对于isochronous和interrupt管道而言），将会在句柄中返回一个错误和空指针
 */
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

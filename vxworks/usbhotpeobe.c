//===========================================2017.05.18 BEGIN===============================================
#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
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
		printf("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);	//0x103;0;0

//获取USB的主机控制器的总个数，每个主机控制器都有一个root hub,获得了主机控制器的总个数之后可以获得每一个root hub的nodeid。
		UINT16 busCount = 0;
		status = usbdBusCountGet(usb_ttyusbdClientHandle,&busCount);
		if(status == OK){
			printf("get bus count OK; the total number is:%d\n",busCount);//7
		}else{
			printf("get bus count failed!\n");
		}


//利用之前获得的host controller的总个数来获得root nodeid
		USBD_NODE_ID rootId;
		int i=0;
		while(i < busCount){
			status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,busCount-1,&rootId);
			if(status == OK){
//					printf("get rootId of root %d OK!the id is:%p\n",i,rootId);
			}else{
				printf("get rootId of root %d failed\n",i);
			}
			i++;
		}


//获取设备的node info
		USBD_NODE_INFO nodeInfo;
		UINT16 infoLen = 8;
		status = usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,&nodeInfo,infoLen);
		if(status == OK){
			printf("\nnode info get OK! nodeType:%d;  nodeSpeed:%d;\nparentHubId:%d;  parentHubport:%d;  rootId:%d;\n",nodeInfo.nodeType,nodeInfo.nodeSpeed,nodeInfo.parentHubId,nodeInfo.parentHubPort,nodeInfo.rootId);
/*nodeType:2(表示是设备),nodeapeed:0(表示全速)，parentHubId:257;parentHubPort:0;rootId:257*/
		}else{
			printf("get node information failed!\n");
		}

//获得设备的nodeid NodeType's type code as:USB_NODETYPE_NONE 0 / USB_NODETYPE_HUB 1 / USB_NODETYPE_DEVICE 2
//第2、3个参数使用上面获得的。
		UINT16 NodeType = -1;
		USBD_NODE_ID nodeId;
		status = usbdNodeIdGet(usb_ttyusbdClientHandle,nodeInfo.parentHubId,nodeInfo.parentHubPort,&NodeType,&nodeId);
		if(status == OK){
			printf("\nnodeType:%d;  nodeId:%p\n",NodeType,nodeId);/*node type:2(表示设备);nodId:0x102*/
		}else{
			printf("get nodeid failed\n");
		}

//获得设备的配置，该值可以在设备的configuration描述符中得到。
//		UINT16 devConfig;
//		status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&devConfig);
//		if(status == OK){
//			printf("\nthe devConfig value is:%d\n",devConfig); //值为1
//		}else{
//			printf("get device configuration failed!\n");
//		}

//获得接口的alternate setting。
		int interfaceIndex = 0;
		UINT16 alternateSetting;
		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
		if(status == OK){
			printf("\nget interface 2 alternate setting is:%d\n",alternateSetting);
		}else{
			printf("get interface 2 alternate setting failed!\n");
		}



/*		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("\nDevice Descriptor:\n");
			printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}*/


		UINT16 ActLen;
		pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
		UINT16 bfrLen = 35;
		pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
		pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
		if(status == OK){
					printf("\nConfiguration:\n");
					printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
					printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
							pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
							pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
//bLength:9;  DescriptorType:2(表示指向的是设备) bNumInterface:1(该配置下接口数为1)； bConfigurationValue:1(该配置的值为1);



			//打印Interface描述符
					printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
							pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
							pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);
//Length:9;  descriptorType:4(表示指向的是接口)；   interfaceNumber：0(表示接口的数目，非0值指出在该配置所同时支持的接口阵列中的索引)；  alternateSetting：0(用于为上一个域所标识出的接口选择可供替换的设置)
//interfaceClass:0xFF(若该位为0xFF，接口类型就由供应商所特定。其他所有的数值就保留而由USB进行分配)。  interfaceSubClass:0x0(表示数值由USB进行分配)；  interfaceProtocol:0x0(表示该接口上没有使用一个特定类型的协议)。  interfaceIndex:2(该值用于描述接口的字符串描述符的索引。)


			//打印EndPoint 1 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
							pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);
//length:7;  descriptorType:5  endpointAddress:0x81(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)

			//打印EndPoint 2 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:0x%x;\n  attributes:0x%x;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
							pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);
//length:7;  descriptorType:5  endpointAddress:0x1(bit7: 0(OUT),1(IN) bit4..6:保留，应复位为0，bit0..3:端点号)；  Attributes:0x2(bit0..1:传输类型0x10表示批量传输);  MaxPacketSize:64(最大分组尺寸)；  Interval:0(用于轮询，对于批量和控制传输应为0)


					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
		}else{
					printf("get configuration descriptor failed!\n");
					free(pBfrReceiveData);
					pBfrReceiveData = NULL;
				}


/*		USB_CONFIG_DESCR usb_ttyConfigDescr;s
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("actual length is:%d;  buffer ength is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);*/
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
/*		}else{
			printf("get configuration descriptor failed!\n");
		}*/




//
	USBD_PIPE_HANDLE PipeHandle1=NULL;
	USBD_PIPE_HANDLE PipeHandle2=NULL;
	USBD_PIPE_HANDLE PipeHandle3=NULL;
	UINT16 endPointIndex=1;
	UINT16 configuration=0;
	UINT16 interface=0;
	UINT16 maxPayload=0x40;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_OUT,maxPayload,0,0,&PipeHandle1);
	if(status == OK){
		printf("create OUT pipe for endpoint 1 OK!\n");
	}else{
		printf("create OUT pipe for endpoint 1 failed;status is %d;\n",status);
	}

	endPointIndex = 2;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle2);
		if(status == OK){
			printf("create IN pipe for endpoint 2 OK!\n");
		}else{
			printf("create IN pipe for endpoint 2 failed;status is %d;\n",status);
		}

	endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,&PipeHandle3);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
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

//===========================================2017.05.18 END=================================================














//==========================================2017.05.17 BEGIN=================================================
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
		printf("CallBack nodeid is:%p; configuration is:%d; interface is %d\n",nodeid,configuration,interface);	//0x103;0;0

//获取USB的主机控制器的总个数，每个主机控制器都有一个root hub,获得了主机控制器的总个数之后可以获得每一个root hub的nodeid。
		UINT16 busCount = 0;
		status = usbdBusCountGet(usb_ttyusbdClientHandle,&busCount);
		if(status == OK){
			printf("get bus count OK; the total number is:%d\n",busCount);//7
		}else{
			printf("get bus count failed!\n");
		}


//利用之前获得的host controller的总个数来获得root nodeid
		USBD_NODE_ID rootId;
		int i=0;
		while(i < busCount){
			status = usbdRootNodeIdGet(usb_ttyusbdClientHandle,busCount-1,&rootId);
			if(status == OK){
//					printf("get rootId of root %d OK!the id is:%p\n",i,rootId);
			}else{
				printf("get rootId of root %d failed\n",i);
			}
			i++;
		}


//获取设备的node info
		USBD_NODE_INFO nodeInfo;
		UINT16 infoLen = 8;
		status = usbdNodeInfoGet(usb_ttyusbdClientHandle,nodeid,&nodeInfo,infoLen);
		if(status == OK){
			printf("\nnode info get OK! nodeType:%d;  nodeSpeed:%d;\nparentHubId:%d;  parentHubport:%d;  rootId:%d;\n",nodeInfo.nodeType,nodeInfo.nodeSpeed,nodeInfo.parentHubId,nodeInfo.parentHubPort,nodeInfo.rootId);
/*nodeType:2(表示是设备),nodeapeed:0(表示全速)，parentHubId:257;parentHubPort:0;rootId:257*/
		}else{
			printf("get node information failed!\n");
		}

//获得设备的nodeid NodeType's type code as:USB_NODETYPE_NONE 0 / USB_NODETYPE_HUB 1 / USB_NODETYPE_DEVICE 2
//第2、3个参数使用上面获得的。
		UINT16 NodeType = -1;
		USBD_NODE_ID nodeId;
		status = usbdNodeIdGet(usb_ttyusbdClientHandle,nodeInfo.parentHubId,nodeInfo.parentHubPort,&NodeType,&nodeId);
		if(status == OK){
			printf("\nnodeType:%d;  nodeId:%p\n",NodeType,nodeId);/*node type:2(表示设备);nodId:0x102*/
		}else{
			printf("get nodeid failed\n");
		}

//获得设备的配置
		UINT16 devConfig;
		status = usbdConfigurationGet(usb_ttyusbdClientHandle,nodeid,&devConfig);
		if(status == OK){
			printf("\nthe devConfig value is:%d\n",devConfig);
		}else{
			printf("get device configuration failed!\n");
		}

//获得接口的alternate setting。
		int interfaceIndex = 0;
		UINT16 alternateSetting;
		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
		if(status == OK){
			printf("\nget interface 0 alternate setting is:%d\n",alternateSetting);
		}else{
			printf("get interface 0 alternate setting failed!\n");
		}
//将接口index设置为1无效
//		interfaceIndex = 1;
//		status = usbdInterfaceGet(usb_ttyusbdClientHandle,nodeid,interfaceIndex,&alternateSetting);
//		if(status == OK){
//			printf("get interface 1 alternate setting is:%d\n",alternateSetting);
//		}else{
//			printf("get interface 1 alternate setting failed!\n");
//		}






/*		USB_DEVICE_DESCR usb_ttyDeviceDescr;
		UINT16 ActLen;
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_DEVICE,USB_DESCR_DEVICE,0,0,USB_DEVICE_DESCR_LEN,(pUINT8)&usb_ttyDeviceDescr,&ActLen);
		if(status == OK){
			printf("\nDevice Descriptor:\n");
			printf("actual length is:%d; buffer length is:%d\n;",ActLen,USB_DEVICE_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  bcdUSB:0x%x;  bDeviceClass:%d;\n  bDeviceSubClass:%d;  bDeviceProtocol:%d;  bMaxPacketSize:%d;  idVendor:0x%x;\n  idProduct:0x%x;  bcdDevice:0x%x;  iMancufacturer:%d;  iProduct:%d;\n  iSerial:%d;  bNumConfigurations:%d;\n",\
					usb_ttyDeviceDescr.length,usb_ttyDeviceDescr.descriptorType,usb_ttyDeviceDescr.bcdUsb,usb_ttyDeviceDescr.deviceClass,usb_ttyDeviceDescr.deviceSubClass,usb_ttyDeviceDescr.deviceProtocol,\
					usb_ttyDeviceDescr.maxPacketSize0,usb_ttyDeviceDescr.vendor,usb_ttyDeviceDescr.product,usb_ttyDeviceDescr.bcdDevice,usb_ttyDeviceDescr.manufacturerIndex,usb_ttyDeviceDescr.productIndex,\
					usb_ttyDeviceDescr.serialNumberIndex,usb_ttyDeviceDescr.numConfigurations);
//numConfigurations is 1;
		}else{
			printf("get descriptor failed!\n");
		}*/


		UINT16 ActLen;
		pUINT8 pBfrReceiveData = (pUINT8)malloc(32*sizeof(char));
		UINT16 bfrLen = 35;
		pUSB_CONFIG_DESCR pusb_ttyConfigDescr = (pUSB_CONFIG_DESCR)(pBfrReceiveData);
		pUSB_INTERFACE_DESCR pusb_ttyInterfaceDescr = (pUSB_INTERFACE_DESCR)(pBfrReceiveData+9);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr1 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+18);
		pUSB_ENDPOINT_DESCR pusb_ttyEndpointDescr2 = (pUSB_ENDPOINT_DESCR)(pBfrReceiveData+25);

		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,bfrLen,pBfrReceiveData,&ActLen);
		if(status == OK){
					printf("\nConfiguration:\n");
					printf("actual length is:%d;  buffer length is:%d;\n",ActLen,bfrLen);
					printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
							pusb_ttyConfigDescr->length,pusb_ttyConfigDescr->descriptorType,pusb_ttyConfigDescr->totalLength,pusb_ttyConfigDescr->numInterfaces,pusb_ttyConfigDescr->configurationValue,\
							pusb_ttyConfigDescr->configurationIndex,pusb_ttyConfigDescr->attributes,pusb_ttyConfigDescr->maxPower);
			//打印Interface描述符
					printf("\nInterface:\n  length:%d;  descriptorType:%d;  interfaceNumber:%d;\n  alternateSetting:%d;  numEndpoints:%d;  interfaceClass:0x%x;\n  interfaceSubClass:0x%x  interfaceProtocol:0x%x;  interfaceIndex:%d\n",\
							pusb_ttyInterfaceDescr->length,pusb_ttyInterfaceDescr->descriptorType,pusb_ttyInterfaceDescr->interfaceNumber,pusb_ttyInterfaceDescr->alternateSetting,pusb_ttyInterfaceDescr->numEndpoints,\
							pusb_ttyInterfaceDescr->interfaceClass,pusb_ttyInterfaceDescr->interfaceSubClass,pusb_ttyInterfaceDescr->interfaceProtocol,pusb_ttyInterfaceDescr->interfaceIndex);

			//打印EndPoint 1 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:%d;\n  attributes:%d;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr1->length,pusb_ttyEndpointDescr1->descriptorType,pusb_ttyEndpointDescr1->endpointAddress,pusb_ttyEndpointDescr1->attributes,\
							pusb_ttyEndpointDescr1->maxPacketSize,pusb_ttyEndpointDescr1->interval);

			//打印EndPoint 2 的描述符
					printf("\nEndPoint:\n  length:%d;  descriptorType:%d;  endpointAddress:%d;\n  attributes:%d;  MaxPacketSize:%d;  interval:%d\n",\
							pusb_ttyEndpointDescr2->length,pusb_ttyEndpointDescr2->descriptorType,pusb_ttyEndpointDescr2->endpointAddress,pusb_ttyEndpointDescr2->attributes,\
							pusb_ttyEndpointDescr2->maxPacketSize,pusb_ttyEndpointDescr2->interval);

		}else{
					printf("get configuration descriptor failed!\n");
				}


/*		USB_CONFIG_DESCR usb_ttyConfigDescr;s
		status = usbdDescriptorGet(usb_ttyusbdClientHandle,nodeid,USB_RT_STANDARD|USB_RT_OTHER,USB_DESCR_CONFIGURATION,0,0,USB_CONFIG_DESCR_LEN,(pUINT8)&usb_ttyConfigDescr,&ActLen);
		if(status == OK){
			printf("\nconfiguration descriptor:\n");
			printf("actual length is:%d;  buffer ength is:%d;\n",ActLen,USB_CONFIG_DESCR_LEN);
			printf("  bLength:%d;  bDescriptorType:%d;  wTotalLength:%d;  bNumInterface:%d;\n  bConfigurationValue:%d;  iConfiguration:%d;  bmAttributes:0x%x;  MaxPower:%d;\n",\
					usb_ttyConfigDescr.length,usb_ttyConfigDescr.descriptorType,usb_ttyConfigDescr.totalLength,usb_ttyConfigDescr.numInterfaces,usb_ttyConfigDescr.configurationValue,\
					usb_ttyConfigDescr.configurationIndex,usb_ttyConfigDescr.attributes,usb_ttyConfigDescr.maxPower);*/
//wTotalLength:32  bNumInterface:1  bConfigurationValue:1  iConfiguration:0;  bmAttributes:0x80;  MaxPower:50
//wTotalLength 是为该配置所返回的数据的整个长度。其中包括为该配置所返回的所有描述符（配置、接口、端点和类型或具体的供应商）的联合长度。
//bNumInterface是该配置所支持的接口数量 ，为1表示该配置下只支持一个接口
//bConfigurationValue：是作为一个用于设备配置的自变量而使用的数值，以选择这一配置。
//iConfiguration：用于描述该配置的字符串索引描述符
//bmAttributes：配置供电和唤醒特性
//MaxPower：电流大小
/*		}else{
			printf("get configuration descriptor failed!\n");
		}*/




//
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

	endPointIndex = 3;
	status = usbdPipeCreate(usb_ttyusbdClientHandle,nodeid,endPointIndex,configuration,interface,USB_XFRTYPE_BULK,USB_DIR_IN,maxPayload,0,0,pPipeHandle);
		if(status == OK){
			printf("create IN pipe for endpoint 3 OK!\n");
		}else{
			printf("create IN pipe for endpoint 3 failed;status is %d;\n",status);
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
//==========================================2017.05.17 END=================================================










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



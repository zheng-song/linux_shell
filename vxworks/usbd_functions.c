http://www.vxdev.com/docs/vx55man/vxworks/ref/usb/usbdLib.html#usbdInterfaceSet

在调usbdInitialize()之后，通常会至少调用

1.usbdInitialize()
//在使用USBD提供的其他函数之前，该函数必须要被调用至少一次，该函数为USBD处理URBs做好准备工作，调用usbdInitialize（）可以是嵌套的，允许多个clients独立的调用。

2.usbdShotDown()------这个函数在每一次成功调用usbdInitialize()之后都应该被调用，这个函数的功能是释放USBD使用的内存和其他的资源。

3.UsbdClientRegister(pCHAR pClientName,pUSBD_CLIENT_HANDLE pClientHandle)---------这个函数用来注册一个新的client到USBD，pClientName用来指向一个长度不大于USBD_NAME_LEN的字符串
这个字符串为该注册的应用程序的名称，USBD用它来区分不同的应用程序，必须保证其唯一性。

3.2.usbdClientUnregister(pUSBD_CLIENT_HANDLE pClientHandle);--------该函数用来取消在USBD中的注册以节约资源，当一个应用程序不再使用USB总线，
应当及时调用usbdClientUnregister()取消注册，此时USBD将会释放为其分配的资源，并删除其回调任务。不需要client来调用此函数。


4.STATUS usbdMngmtCallbackSet(USBD_CLIENT_HANDLE clientHandle, USBD_MNGMT_CALLBACK mngmtCallback, pVOID mngmtCallbackParam)------当USB上有
asynchronous management events发生时， Management callbacks为USBD提供一个机制来通知clients。例如：如果USB处于SUSPEND状态并且一个USB设备驱动RESUME signalling，
这个event能够通过management callback来报告给client。Clients并不被要求注册一个management callback例程，clients确实需要注册的话，对每一个USBD_CLIENT_HANDLE也只能
被允许注册最多一个management callback。


5.usbdBusStateSet(clientHandle，nodeId，UINT16 busStatus)--------这个函数允许一个client设置bus的state，期望busStatus被定义为USBD_BUS_xxxx.
通常一个client使用这个函数来设置bus的状态为SUSPEND或者是RESUME。Clients必须小心使用这个函数，因为这会影响连接在这条总线上的所有设备。并且因此影响所有的跟
这些设备进行通信的clients。当设置bus的状态为SUSPEND状态时，要注意USBD不会自动的将bus的状态转换回RESUME状态。Client必须要显式的使用这个函数来将bus转换为RESUME状态。
当考虑到USB的“远程唤醒”特性时这一点尤其重要。“远程唤醒”特性允许一个远程设备发送RESUME信号到bus上。然而识别这个信号却是client的职责，识别通过使用
management callback监视management events来完成。当一个USBD_MNGMT_RESUME event被检测到，client可RESUME或不 RESUME bus。


6.usbdBusCountGet(clientHandle，pUINT16 pBusCount)--------这个函数返回系统中的USB host controllers的总个数。每一个主机控制器都有自己的root hub。
Client试图使用bus枚举函数枚举USB设备时，那么它需要知道host controllers的总数，这样就可以获得每一个root hub的node Ids。USB主机控制器的数量会被存放在pBusCount
指向的地址处。


7.usbdRootNodeIdGet(clientHandle,UINT16 busIndex/*BUS index*/,pUSBD_NODE_ID pRootId/*bfr to receive Root Id*/);--------这个函数返回指定的USB host controller的根hub
的Node Id。busIndex是期望的USB host controller的索引。第一个主机控制器的索引是0，最后一个控制器的索引是主机控制器的总数(usbdBusCountGet()的返回值)减一。
<pRootId>必须要指向一个USBD_NODE_ID型的变量，Node Id将会存储在这个变量当中。


8.usbdHubPortCountGet(clientHandle,USBD_NODE_ID hubId, pUINT16 pPortCount);--------这个函数给client提供一个便捷的机制来获得指定的hub的下行端口的数量，clients也可以
使用配置函数从hub获得配置描述符(configuration descriptors)来获取这一信息。
hubId必须要是期望的USB hub的Node Id，如果hubId没有指向一个hub，那么会返回一个错误，
pPortCount必须要指向一个UINT16类型的变量，在这个变量中会存储指定hub的port的总数。

9.usbdNodeIdGet(clientHandle,USBD_NODE_ID hubId, UINT16 portIndex, pUINT16 pNodeType, pUSBD_NODE_ID pNodeId);
client使用这个函数来获得连接在hub的每一个端口的设备的Node Id。
hubId和portIndex指定要获得NodeId的设备所连接在的hub和port
pNodeType必须指向一个UINT16类型的变量，这个变量用来接收一个类型代码。代码如下：
USBD_NODETYPE_NONE（表示没有设备连接在指定的port上） USB_NODETYPE_HUB（指定的port上连接的是一个hub而不是设备）
USBD_NODETYPE_DEVICE（指定port上连接的是设备）
如果node类型是USBD_NODETYPE_NONE，那么就不会返回nodeid,在pNodeId中的值就是不确定的。若类型是hub或者device，那么pNodeId将会包含hub或者是device的nodeid



10.usbdNodeInfoGet(clientHandle, USBD_NODE_ID nodeId/*Node id of device/hub*/, pUSBD_NODE_ID pNodeInfo,/*structure to receive node info*/,\
		 UINT16 infoLen/*Len of bfr allocated by client*/);
这个函数获得指定nodeid的USB设备的信息，USBD会复制设备的信息到一个由调用者提供的pNodeInfo结构体当中，这个结构体如下所示：
typedef struct usbd_nodeinfo{
	UINT16 			nodeType; 		//nodeType定义为 USB_NODETYPE_xxxx的形式
	UINT16 			nodeSpeed；		//nodeSpeed定义为USB_SPEED_XXXX的形式
	USBD_NODE_ID 	parentHubId；	//parentHUbid和parentHubPort指定设备所连接在的port和hub
	UINT16 			parentHubPort；	//若nodeId指向的是一个根hub，那么parentHubId和parentHubPort就会是0
	USBD_NODE_ID 	rootId; 		//
} USBD_NODE_INFO,*pUSBD_NODE_INFO;
可预见的是这个结构可能会随着时间的推移而变大。为了提供向后兼容性，client必须要传递一个USBD_NODEINFO类型的结构体的大小的变量在infoLen中。
USBD只会复制调用者所提供的大小到这个结构体当中。



11.usbdDynamicAttachRegister(clientHandle, deviceClass, deviceSubClass, deviceProtocol, attachCallback);
client调用这个函数来告知USBD，当一个指定的class/subclass/protocol设备连接或者是移除的时候，client希望得到这个通知。
一个client可能会指定它希望接受整个设备类或者是这个类当中某一个特殊的subclass的通知。
attachCallback必须是一个由client提供的callback例程的非空指针，类型如下：
typedef VOID (*USBD_ATTACH_CALLBACK)(USBD_NODE_ID nodeid,
	UINT16 		attachAction,
	UINT16 		configuration,
	UINT16 		interface,
	UINT16 		deviceClass,
	UINT16 		deviceSubClass,
	UINT16 		deviceProtocol
	);
每次attachCallback被调用的时候，USBD会传递该设备的nodeid和attachAction给这个回调函数。当attachAction是USBD_DYNA_REMOVE的时候，表明指向的nodeid不再有效，
此时client应该查询这个nodeid的永久数据结构并且删除任何和这个nodeid相关联的结构，如果client有明显的对这个nodeid的请求，例如：数据传输请求，那么USBD就会
在调用attachCallback通知client这个设备被移除之前设置这些显式的请求为无效。通常与一个被移除的设备相关的数据传输请求在attachCallback被调用之前就应该被小心的对待。
	client可能会为多个通知登记重复使用一个attachCallback，为了方便起见，USBD也会在每个设备attach/remove的时候传递该设备的class/subclass/protocol给attachCallback。
最后需要注意的是不是所有的设备都会在设备级来报告class信息，一些设备在interface-to-interface的基础上报告class信息。
当设备在设备级报告class信息时，USBD会在attachCallback中传递configuration的值为0，并且每一个设备只会调用一次attachCallback。
当设备在interface级报告class信息时，USBD会为每一个接口唤醒一次attachCallback，只要这个接口适配client的class/subclass/protocol，
在这种情况下，USBD也会在每一次调用attachCallback时传递对应的configuration和interface的值


12.uabdDynamicAttachUnRegister(clientHandle, deviceClass, deviceSubClass, deviceProtocol, USBD_ATTACH_CALLBACK attachCallback);
这个函数用来忽略一个设备之前为特定class/subclass/protocol的热插拔请求，



13.usbdFeatureClear(clientHandle, nodeid/*nodeid of device/hub*/, UINT16 requestType/*select request type*/, UINT16 feature/*feature selector*/, \
		UINT16 index/*interface/endpoint index*/);
这个函数的功能是允许一个client来清除USB的特性，nodeid指定对象是一个device还是hub，requestType指定这个特性是对于一个device、interface还是endpoint。
requestType类型有：USB_RT_DEVICE/USB_RT_INTERFACE/USB_RT_ENDPOINT，同时requestType还指定这个请求时标准的，class-specific还是Vendor-specific的
如下：USB_RT_STANDARD/UAB_RT_CLASS/USB_RT_VENDOR，例如USB_RT_STANDARD|USB_RT_DEVICE表示一个标准设备请求。
	client必须在feature中传递device选择的特性。如果featureType指定是interface或者endpoint，那么index就是interface或endpoint的index，如果是device那么index就是0.



14.usbdFeatureSet(clientHandle, nodeid, requestType, feature, index);
这个函数允许client设置一个USB的特性，nodeid指定要设置的对象。requestType类型和usbdfeatureClear()中的一样，
	client必须要在feature传递一个设备的特性值，若requestType为device，那么index值为0.


15.usbdConfigurationGet(clientHandle, nodeid/*nodeid of device/hub*/,pUINT16 pConfiguration/*bfr to receive config value*/)
	该函数返回现在选择的由nodeid指定的device或hub的configuration，当前configuration值返回在pConfiguration的低字节中，高字节目前保持为0。


16.usbdConfigurationSet(clientHandle, nodeid/*nodeid of device/hub*/, UINT16 configuration, UINT16 maxPower);
	该函数用来设置当前由nodeid指定的设备的configuration.client在configuration的低字节中传递所需的configuration值，高字节为0.
	client应该传递最大的电流值给该配置。



17.usbdDescriptorGet(clientHandle,nodeId/*Nodeid of device/hub*/,UINT8 requestType,UINT8 descriptorType
	UINT8 descriptorIndex, UINT16 languageId, UINT16 bfrLen, UINT8 pBfr, pUINT16 pActLen)
client使用这个函数来从nodeid指定的USB设备获取描述符，requestType和usbdFeatureClear()中定义的一样。descriptorType指定要获取的
描述符的类型，必须为以下的类型之一：USB_DESCR_DEVICE、USB_DESCR_CONFIG、USB_DESCR_STRING、USB_DESCR_INTERFACE、USB_DESCR_ENDPOINT。
	descriptorIndex是所需的描述符的索引。对于device和configuration描述符，languageId应该是0。(那interface的languageId是什么？？？？)




18.usbdDescriptorSet(clientHandle, nodeid, requestType, descriptorType，descriptorIndex，bfrLen, pUINT8 pBfr).
	client使用这个函数设置nodeid指定的USB设备的描述符，pBfr是一个指向长度为bfrLen的buffer的指针，在这个buffer中存储了发送给设备的描述符数据。



19.usbdInterfaceGet(clientHandle, nodeId/*nodeid of device/hub*/,UINT16 interfaceIndex/*index of interface*/,pUINT16 pAlternateSetting/*current alternate setting*/)
	这个函数允许一个client询问一个给定设备接口的当前的alternate setting。nodeid和interfaceIndex指定要询问的设备的interface。pAlternateSetting指向一个UINT16类型的变量，
这个变量用来存储返回值。


20.usbdInterfaceSet(clientHandle, nodeid, interfaceIndex, alternateSetting);
	该函数允许一个client为给定的设备的接口选择一个alternate setting


21.usbdStatusGet(clientHandle, nodeId, requestType/*selects device/interface/endpoint*/,UINT16 index/*interface/endpoint index*/, UINT16 bfrLen,\
		 pUINT8 pBfr, pUINT16 pActLen);
	这个函数获得nodeid指定的设备的当前状态，status word返回在pBfr中，状态的含义依据所询问的是device/interface/endpoint而不同。


22.usbdVendorSpecific(clientHandle, nodeId,UINT8 requestType, UINT8 request, UINT16 value, UINT16 index, UINT16 length, pUINT8 pBfr, pUINT16 pActLen);
	特殊的设备可能会执行vendor-specific的USB请求，这一请求无法使用其他地方的标准函数来生成，而这个函数允许一个client为USB控制管道请求直接指定准确的参数。
requestType/*bmRequestType in USB spec*/，request/*bRequest in USB spec*/,value/*wValue in USB spec*/,index/*wIndex in USB spec*/,length/*wLength in USB spec*/
如果长度大于零，那么pBfr必须是一个非空的指针，这个指针指向一个数据buffer，这个buffer用来提供还是接收数据要根据传输的方向而定。
	使用这个函数的厂商指定请求通常直接送到nodeid指定的设备的控制管道。这个函数根据提供的参数来格式化并且发送一个Setup packet。如果提供了一个非空pBfr，那么附加
的IN或OUT传输将会在Setup packet之后执行。传输的方向参考requestType参数中的方向比特位。对于IN传输，如果pActLen非空，那么实际的数据传输长度将会存储在pActLen中。



23.usbdPipeCreate(clientHandle, nodeid, endpoint, configuration, interface, transferType, direction, maxPayload, bandwidth, serviceInterval,\
		pUSBD_PIPE_HANDLE pPipehandle);
	这个函数用来建立一个管道，该管道随后被用来在client和USB的endpoint之间交换数据。nodeid 和endpoint各自指明管道连接的设备和设备的端点。
configuration和interface各自指明管道关联的configuration和interface(usbd使用这个信息来保持对这个管道相关的配置事件的跟踪）
  	管道会用到的数据传输类型为以下几种：
1.USB_XFRTYPE_CONTROL（控制管道（信息流））2.USB_XFRTYPE_ISOCH(isochronous 传输管道(数据流))3.USB_XFRTYPE_INTERRUPT(中断传输管道(数据流))
4.USB_XFRTYPE_BLUK(块传输管道(数据流))
	方向为以下几种：1.USB_DIR_IN(从设备到host)2.USB_DIR_OUT(host到device)3.USB_DIR_INOUT(Data moves bidirectionally (message pies only))
	maxPayload指定此端点的最大数据负载，通常USB设备会在每一个端点的描述符中声明最大负载。client通常获取这些描述然后取得合适的负载值
	bandwidth指定管道的带宽需求，对于control和bulk管道，这个参数是应该为0，对于中断管道，这个参数应该传递每个被传输的帧的byte数量。\
对于isochronous管道，这个参数应该传递每秒传输的byte数量
	serviceInterval指定管道每毫秒最大的潜能？？？这个参数只用于中断管道，对于其他管道为0
	成功创建管道会返回一个管道句柄，client需要使用管道句柄来标识这个管道以调用USBD的Transfer函数。如果没有足够的带宽来创建管道（对于isochronous和interrupt管道而言）,\
将会在句柄中返回一个错误和空指针
 


24.usbdPipeDestory(clientHandle, pipeHandle);
	该函数摧毁一个先前使用usbdPipeCerate()创建的管道，调用者要传递管道句柄参数。如果管道中有显著的传输请求，USBD会在摧毁管道之前终止传输。



25.usbdtransfer(clientHandle, pipeHandle, pIrp)
	client使用这个函数来初始化在一个pipeHandle指定的管道传输，传输由IRP来描述，或者是I/O请求包，调用者必须在使用usbdTransfer()之前对其进行配置和初始化。
USB_IRP结构体定义在usb.h中。如下：
typedef struct usb_bfr_list{
	UINT16 		pid;
	pUINT16 	pBfr;
	UINT16 		bfrLen;
	UINT16 		actLen;
} USB_BFR_LIST;

typedef struct usb_irp{
	LINK 	usbdLink;	//used by USBD
	pVOID 	usbdPtr;	//used by USBD
	LINK 	hcdLink;	//used by HCD
	pVOID 	hcdLink;	//used by HCD
	pVOID 	userPtr;	//ptr field used by client
	UINT16 	irpLen;		//total length of IRP structure
	int 	result;		//IRP completion result:S_usbHcdLib_xxxx
	IRP_CALLBACK usbdCallback;	/* USBD completion callback routine */
    IRP_CALLBACK userCallback;	/* client's completion callback routine */
    UINT16 	dataToggle;		/* IRP should start with DATA0/DATA1. */
    UINT16 	flags;		/* Defines other IRP processing options */
    UINT32 	timeout;		/* IRP timeout in milliseconds */
    UINT16 	startFrame;		/* Start frame for isoch transfer */
    UINT16 	dataBlockSize;	/* Data granularity for isoch transfer */
    UINT32 	transferLen; 	/* Total length of data to be transferred */
    UINT16 	bfrCount;		/* Indicates count of buffers in BfrList */
    USB_BFR_LIST bfrList [1];
    } USB_IRP, *pUSB_IRP;




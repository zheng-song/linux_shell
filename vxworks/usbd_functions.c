http://www.vxdev.com/docs/vx55man/vxworks/ref/usb/usbdLib.html#usbdInterfaceSet

在调usbdInitialize()之后，通常会至少调用

1.usbdInitialize()
//在使用USBD提供的其他函数之前，该函数必须要被调用至少一次，该函数为USBD处理URBs做好准备工作，调用usbdInitialize（）可以是嵌套的，允许多个clients独立的调用。

usbdShotDown()------这个函数在每一次成功调用usbdInitialize()之后都应该被调用，这个函数的功能是释放USBD使用的内存和其他的资源。

UsbdClientRegister()---------这个函数用来注册一个新的client到USBD，


STATUS usbdMngmtCallbackSet(USBD_CLIENT_HANDLE clientHandle, USBD_MNGMT_CALLBACK mngmtCallback, pVOID mngmtCallbackParam
)------当USB上有asynchronous management events发生时， Management callbacks为USBD提供一个机制来通知clients。例如：如果USB处于SUSPEND状态并且一个USB设备驱动RESUME signalling，这个event能够通过management callback来报告给client。Clients并不被要求注册一个management callback例程，clients确实需要注册的话，对每一个USBD_CLIENT_HANDLE也只能被允许注册最多一个management callback。


usbdBusStateSet（clientHandle，nodeId，UINT16 busStatus）--------这个函数允许一个client设置bus的state，期望busStatus被定义为USBD_BUS_xxxx.通常一个client使用这个函数来设置bus的状态为SUSPEND或者是RESUME。Clients必须小心使用这个函数，因为这会影响连接在这条总线上的所有设备。并且因此影响所有的跟这些设备进行通信的clients。当设置bus的状态为SUSPEND状态时，要注意USBD不会自动的将bus的状态转换回RESUME状态。Client必须要显式的使用这个函数来将bus转换为RESUME状态。当考虑到USB的“远程唤醒”特性时这一点尤其重要。“远程唤醒”特性允许一个远程设备发送RESUME信号到bus上。然而识别这个信号却是client的职责，识别通过使用management callback监视management events来完成。当一个USBD_MNGMT_RESUME event被检测到，client可RESUME或不 RESUME bus。


usbdBusCountGet（clientHandle，pUINT16 pBusCount）--------这个函数返回系统中的USB host controllers的总个数。每一个主机控制器都有自己的root hub。Client试图使用bus枚举函数枚举USB设备，那么它需要知道host controllers的总数，这样就可以获得每一个root hub的node Ids。USB主机控制器的数量会被存放在pBusCount指向的地址处。
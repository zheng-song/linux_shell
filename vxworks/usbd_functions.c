http://www.vxdev.com/docs/vx55man/vxworks/ref/usb/usbdLib.html#usbdInterfaceSet
1.usbdInitialize()
//在使用USBD提供的其他函数之前，该函数必须要被调用至少一次，该函数为USBD处理URBs做好准备工作，调用usbdInitialize（）可以是嵌套的，允许多个clients独立的调用。

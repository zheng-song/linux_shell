//===========================================2017.05.26 19:27 BEGIN===============================================
#include <lstLib.h>
#include <ioLib.h>
#include <usb/usbHid.h>
#include <vxWorks.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <sioLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <logLib.h>
#include <selectLib.h>
#include <tyLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>

#ifndef USBHOTPROBE_H_
#define USBHOTPROBE_H_

#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 	//256

typedef struct usb_tty_node{
	NODE 	node;
	struct usb_tty_dev *pUsbttyDev;
}USB_TTY_NODE;

typedef struct _usb_tty_dev
{
	/*I/O子系统在调用底层驱动函数时，将使用设备结构作为函数调用的第一个参数，这个设备结构是底层驱动自定义的，
	 * 用以保存底层硬件设备的关键参数。对于自定义的设备结构必须将内核结构DEV_HDR作为自定义设备结构的第一个参数。
	 * 该结构被I/O子系统使用，统一表示系统内的所有设备，即I/O子系统将所有的驱动自定义的设备结构作为DEV_HDR使用，
	 * DEV_HDR之后的字段由各自具体的驱动进行解释和使用，内核并不关心这些字段的含义。*/
	DEV_HDR 		usbttyDev;
	SIO_CHAN 		pSioChan;
	USBD_NODE_ID	usbttyNodeid;
	UINT16 			numOpened;
	UINT16 			configuration;
	UINT16 			configurationValue;
	UINT16 			interface;
	UINT16			altSetting;
	UINT16 			outEndpointAddress;
	UINT16 			inEndpointAddress;
	UINT16          epMaxPacketSize;
	UINT8   		maxPower;
	USBD_PIPE_HANDLE outPipeHandle;
	USBD_PIPE_HANDLE inPipeHandle;
	USB_IRP			inIrp;		//IRP used for in data
	USB_IRP 		outIrp;		//IRP used for out data
	USB_IRP 		statusIrp; 	//IRP used for reading status
	UINT8 			*usbInData;		//Pointer for USB-in data
	UINT8 			*usbOutData;	//pointer for USB-out data
	/*USB-in-data用于指向存储被读取数据的缓冲区，*/
//TODO
}USB_TTY_DEV;



#endif /* USBHOTPROBE_H_ */
//===========================================2017.05.26 19:27 END===============================================



//===========================================2017.05.26 BEGIN===============================================
/*
 * usbhotprobe.h
 *
 *  Created on: 2017-5-19
 *      Author: sfh
 */
#include <lstLib.h>
#include <ioLib.h>
#include <usb/usbHid.h>
#include <vxWorks.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <sioLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <logLib.h>
#include <selectLib.h>
#include <tyLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>

#ifndef USBHOTPROBE_H_
#define USBHOTPROBE_H_

#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 	//256

typedef struct usb_tty_node{
	NODE 	node;
	struct usb_tty_dev *pUsbttyDev;
}USB_TTY_NODE;

typedef struct _usb_tty_dev
{
	/*I/O子系统在调用底层驱动函数时，将使用设备结构作为函数调用的第一个参数，这个设备结构是底层驱动自定义的，
	 * 用以保存底层硬件设备的关键参数。对于自定义的设备结构必须将内核结构DEV_HDR作为自定义设备结构的第一个参数。
	 * 该结构被I/O子系统使用，统一表示系统内的所有设备，即I/O子系统将所有的驱动自定义的设备结构作为DEV_HDR使用，
	 * DEV_HDR之后的字段由各自具体的驱动进行解释和使用，内核并不关心这些字段的含义。*/
	DEV_HDR 		usbttyDev;
	SIO_CHAN 		pSioChan;
	USBD_NODE_ID	usbttyNodeid;
	UINT16 			numOpened;
	UINT16 			configuration;
	UINT16 			interface;
	UINT16			altSetting;
	UINT16 			outEndpointAddress;
	UINT16 			inEndpointAddress;
	USBD_PIPE_HANDLE outPipeHandle;
	USBD_PIPE_HANDLE inPipeHandle;
	USB_IRP			inIrp;		//IRP used for in data
	USB_IRP 		outIrp;		//IRP used for out data
	USB_IRP 		statusIrp; 	//IRP used for reading status
	UINT8 			*usbInData;		//Pointer for USB-in data
	UINT8 			*usbOutData;	//pointer for USB-out data
	/*USB-in-data用于指向存储被读取数据的缓冲区，*/
//TODO
}USB_TTY_DEV;



#endif /* USBHOTPROBE_H_ */



//===========================================2017.05.26 END=================================================







#include <lstLib.h>
#include <ioLib.h>
#include <usb/usbHid.h>
#include <vxWorks.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <sioLib.h>
#include <stdlib.h>
#include <stdio.h>
#include <logLib.h>
#include <selectLib.h>
#include <tyLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>

#ifndef USBHOTPROBE_H_
#define USBHOTPROBE_H_

#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 	//256
#define USB_TTY_DEVVICE_NAME 		"USBTTY/0"

typedef struct usb_tty_node{
	NODE 	node;
	struct usb_tty_dev *pUsbttyDev;
}USB_TTY_NODE;

typedef struct usb_tty_dev{
	DEV_HDR 		ioDev;
	SIO_CHAN *		pSioChan;
	UINT16			numOpen;
	UINT32			bufSize;
	UCHAR *			buff;
	USB_TTY_NODE *	pUsbttyNode;
}USB_TTY_DEV,*pUSB_TTY_DEV;



#endif /* USBHOTPROBE_H_ */
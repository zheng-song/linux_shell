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
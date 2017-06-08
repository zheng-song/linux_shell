/*
 * ttyusb.h
 *
 *  Created on: 2017-6-8
 *      Author: sfh
 */


#ifndef __USBTTY__H
#define __USBTTY_H
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
#include <lstLib.h>
#include <vxWorksCommon.h>
#include <String.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbListLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbdLib.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\pciConstants.h>
#include <D:\LambdaPRO615\target\deltaos\include\vxworks\usb\usbPciLib.h>

#define USB_TEST_CLASS 				0x10C4		//4292
#define USB_TEST_SUB_CLASS			0xEA60		//6000
#define USB_TEST_DRIVE_PROTOCOL	 	0x0100 		//256

#define USB_TTY_ATTACH 		0
#define USB_TTY_REMOVE		1

typedef VOID (*USB_TTY_ATTACH_CALLBACK)(
	pVOID arg,			/* caller-defined argument */
	SIO_CHAN *pChan,	/* pointer to affected SIO_CHAN */
	UINT16 attachCode	/* defined as USB_TTY_xxxx */
);

/*function prototypes*/
STATUS usbttyDevInit (void);
STATUS usbttyDevShutDown(void);

STATUS usbttyDynamicAttachRegister(
	USB_TTY_ATTACH_CALLBACK callback,	/* new callback to be registered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS usbttyDynamicAttachUnRegister(
	USB_TTY_ATTACH_CALLBACK callback,	/* callback to be unregistered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS usbttySioChanLock(
    SIO_CHAN *pChan		    /* SIO_CHAN to be marked as in use */
);

STATUS usbttySioChanUnlock(
    SIO_CHAN *pChan		    /* SIO_CHAN to be marked as unused */
);


#endif


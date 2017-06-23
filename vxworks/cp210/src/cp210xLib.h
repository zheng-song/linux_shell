/*
 * cp210xLib.h
 *
 *  Created on: 2017-6-12
 *      Author: sfh
 */

#ifndef CP210XLIB_H_
#define CP210XLIB_H_

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


#define htole16(x) (x)
#define cpu_to_le16(x) htole16(x)


/* Config request types */
#define REQTYPE_HOST_TO_INTERFACE	0x41
//0x41 = 0x 0(方向为主机到设备) 10(类型为厂商自定义) 00001(接收方为接口)
#define REQTYPE_INTERFACE_TO_HOST	0xc1  //1100 0001
// 0xc1 = 0x 1(设备到主机) 10(类型为厂商自定义) 00001(接收方为接口???主机的接口??)
#define REQTYPE_HOST_TO_DEVICE	0x40
//0x41 = 0x 0(主机到设备) 10(类型为厂商自定义) 00000(接收方为设备)
#define REQTYPE_DEVICE_TO_HOST	0xc0
//0xc0 = 0x 1(设备到主机) 10(厂商自定义) 00000(接收方为设备)

/* Config request codes */
/*厂商自定义的设备请求，不是标准的设备请求*/
#define CP210X_IFC_ENABLE	0x00
#define CP210X_SET_BAUDDIV	0x01
#define CP210X_GET_BAUDDIV	0x02
#define CP210X_SET_LINE_CTL	0x03
#define CP210X_GET_LINE_CTL	0x04
#define CP210X_SET_BREAK	0x05
#define CP210X_IMM_CHAR		0x06
#define CP210X_SET_MHS		0x07
#define CP210X_GET_MDMSTS	0x08
#define CP210X_SET_XON		0x09
#define CP210X_SET_XOFF		0x0A
#define CP210X_SET_EVENTMASK	0x0B
#define CP210X_GET_EVENTMASK	0x0C
#define CP210X_SET_CHAR		0x0D
#define CP210X_GET_CHARS	0x0E
#define CP210X_GET_PROPS	0x0F
#define CP210X_GET_COMM_STATUS	0x10
#define CP210X_RESET		0x11
#define CP210X_PURGE		0x12
#define CP210X_SET_FLOW		0x13
#define CP210X_GET_FLOW		0x14
#define CP210X_EMBED_EVENTS	0x15
#define CP210X_GET_EVENTSTATE	0x16
#define CP210X_SET_CHARS	0x19
#define CP210X_GET_BAUDRATE	0x1D
#define CP210X_SET_BAUDRATE	0x1E
#define CP210X_VENDOR_SPECIFIC	0xFF

/* CP210X_IFC_ENABLE */
#define UART_ENABLE		0x0001
#define UART_DISABLE		0x0000

/* CP210X_(SET|GET)_BAUDDIV */
#define BAUD_RATE_GEN_FREQ	0x384000

/* CP210X_(SET|GET)_LINE_CTL */
#define BITS_DATA_MASK		0X0f00
#define BITS_DATA_5		0X0500
#define BITS_DATA_6		0X0600
#define BITS_DATA_7		0X0700
#define BITS_DATA_8		0X0800
#define BITS_DATA_9		0X0900

#define BITS_PARITY_MASK	0x00f0
#define BITS_PARITY_NONE	0x0000
#define BITS_PARITY_ODD		0x0010
#define BITS_PARITY_EVEN	0x0020
#define BITS_PARITY_MARK	0x0030
#define BITS_PARITY_SPACE	0x0040

#define BITS_STOP_MASK		0x000f
#define BITS_STOP_1		0x0000
#define BITS_STOP_1_5		0x0001
#define BITS_STOP_2		0x0002

/* CP210X_SET_BREAK */
#define BREAK_ON		0x0001
#define BREAK_OFF		0x0000

/* CP210X_(SET_MHS|GET_MDMSTS) */
#define CONTROL_DTR		0x0001
#define CONTROL_RTS		0x0002
#define CONTROL_CTS		0x0010
#define CONTROL_DSR		0x0020
#define CONTROL_RING		0x0040
#define CONTROL_DCD		0x0080
#define CONTROL_WRITE_DTR	0x0100
#define CONTROL_WRITE_RTS	0x0200

/* CP210X_VENDOR_SPECIFIC values */
#define CP210X_READ_LATCH	0x00C2
#define CP210X_GET_PARTNUM	0x370B
#define CP210X_GET_PORTCONFIG	0x370C
#define CP210X_GET_DEVICEMODE	0x3711
#define CP210X_WRITE_LATCH	0x37E1

/* Part number definitions */
#define CP210X_PARTNUM_CP2101	0x01
#define CP210X_PARTNUM_CP2102	0x02
#define CP210X_PARTNUM_CP2103	0x03
#define CP210X_PARTNUM_CP2104	0x04
#define CP210X_PARTNUM_CP2105	0x05
#define CP210X_PARTNUM_CP2108	0x08


#define CP210X_IN_BFR_SIZE		64    	//size of input buffer
#define CP210X_OUT_BFR_SIZE		4096	//size of output buffer

/*IRP Time out in millisecs */
#define CP210X_IRP_TIME_OUT 	5000

/* defines */

/* Error Numbers as set the usbEnetLib  */

/* usbEnetLib error values */

/*
 * USB errnos are defined as being part of the USB host Module, as are all
 * vxWorks module numbers, but the USB Module number is further divided into
 * sub-modules.  Each sub-module has upto 255 values for its own error codes
 */

#define CP210X_SUB_MODULE  	14

#define M_cp210xLib 	( (CP210X_SUB_MODULE << 8) | M_usbHostLib )

#define cp210xErr(x)	(M_cp210xLib | (x))

#define S_cp210xLib_NOT_INITIALIZED		cp210xErr (1)
#define S_cp210xLib_BAD_PARAM			cp210xErr (2)
#define S_cp210xLib_OUT_OF_MEMORY		cp210xErr (3)
#define S_cp210xLib_OUT_OF_RESOURCES	cp210xErr (4)
#define S_cp210xLib_GENERAL_FAULT		cp210xErr (5)
#define S_cp210xLib_QUEUE_FULL	    	cp210xErr (6)
#define S_cp210xLib_QUEUE_EMPTY			cp210xErr (7)
#define S_cp210xLib_NOT_IMPLEMENTED		cp210xErr (8)
#define S_cp210xLib_USBD_FAULT	    	cp210xErr (9)
#define S_cp210xLib_NOT_REGISTERED		cp210xErr (10)
#define S_cp210xLib_NOT_LOCKED	    	cp210xErr (11)


#define CP210X_ATTACH 		0
#define CP210X_REMOVE		1

#define USB_CSW_LENGTH		0x0D	//Length of CSW
#define USB_CBW_MAX_CBLEN	0x10	//Max length of command block



typedef struct cp210x_flow_ctl{
	UINT32 ulControlHandshake;
	UINT32 ulFlowReplace;
	UINT32 ulXonLimit;
	UINT32 ulXoffLimit;
}CP210X_FLOW_CTL;

#define BITS_PER_LONG (__CHAR_BIT__ * __SIZEOF_LONG__)
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define BIT(nr)			(1UL << (nr))
/* cp210x_flow_ctl::ulControlHandshake */
#define CP210X_SERIAL_DTR_MASK		GENMASK(1, 0)
#define CP210X_SERIAL_DTR_SHIFT(_mode)	(_mode)
#define CP210X_SERIAL_CTS_HANDSHAKE	BIT(3)
#define CP210X_SERIAL_DSR_HANDSHAKE	BIT(4)
#define CP210X_SERIAL_DCD_HANDSHAKE	BIT(5)
#define CP210X_SERIAL_DSR_SENSITIVITY	BIT(6)

/* values for cp210x_flow_ctl::ulControlHandshake::CP210X_SERIAL_DTR_MASK */
#define CP210X_SERIAL_DTR_INACTIVE	0
#define CP210X_SERIAL_DTR_ACTIVE	1
#define CP210X_SERIAL_DTR_FLOW_CTL	2

/* cp210x_flow_ctl::ulFlowReplace */
#define CP210X_SERIAL_AUTO_TRANSMIT	BIT(0)
#define CP210X_SERIAL_AUTO_RECEIVE	BIT(1)
#define CP210X_SERIAL_ERROR_CHAR	BIT(2)
#define CP210X_SERIAL_NULL_STRIPPING	BIT(3)
#define CP210X_SERIAL_BREAK_CHAR	BIT(4)
#define CP210X_SERIAL_RTS_MASK		GENMASK(7, 6)
#define CP210X_SERIAL_RTS_SHIFT(_mode)	(_mode << 6)
#define CP210X_SERIAL_XOFF_CONTINUE	BIT(31)

/* values for cp210x_flow_ctl::ulFlowReplace::CP210X_SERIAL_RTS_MASK */
#define CP210X_SERIAL_RTS_INACTIVE	0
#define CP210X_SERIAL_RTS_ACTIVE	1
#define CP210X_SERIAL_RTS_FLOW_CTL	2


#define CP210X_Q_DEPTH  8 /*Max characters in cp210x device queue*/

/*
 * USB设备结构： 这个结构是用在动态注册回调函数中的主结构
 * 之后当最后的end_obj结构被创建之后，这个结构将被链接
 * 到end_obj结构体。
 * */
//typedef struct cp210x_dev{
//	LINK 			cp210xDevLink;	/*设备结构体的链表*/
//	USBD_NODE_ID	nodeId;			/*设备的NODE ID*/
//	UINT16 			configuration;	/*设备的配置*/
//	UINT16			interface;		/*设备的接口*/
//
//	UINT16 			vendorId;		/*设备的厂商ID */
//	UINT16 			productId;		/*设备的产品ID*/
//	UINT16 			lockCount;     	/*设备结构体上锁的次数*/
//	BOOL   			connected;		/*是否已连接*/
//
//	VOID			*pCp210xDevice;
//}CP210X_DEV,*pCp210xListDev;





typedef struct cp210x_sio_chan{
	SIO_CHAN sioChan;		/*serial device struct,must be the first*/
	LINK cp210xSioLink;	/* linked list of cp210x structs */

	USBD_NODE_ID nodeId;			/*device nodeID*/
	UINT16 configuration;	/*configuration/interface reported as*/
	UINT16 interface;		/*a interface of this device*/
	UINT16 interfaceAltSetting;

	UINT16 vendorId;		/*厂商ID */
	UINT16 productId;		/*设备的产品ID*/

	BOOL connected;		/*是否已连接*/
	UINT16 lockCount;				/* Count of times structure locked */

	UINT8 communicateOk;	    /* TRUE after Starting and FALSE if stopped */

	int noOfOutIrps;		    /* no of Irps */
	int	 txIrpIndex;		    /* What the last submitted IRP is */

	USBD_PIPE_HANDLE outPipeHandle; /* USBD pipe handle for bulk OUT pipe */
	USB_IRP	outIrp;					/*IRP to monitor output to device*/
	BOOL outIrpInUse;				/*TRUE while IRP is outstanding*/
	char  *pOutBfr;					/* pointer to output buffer */
	UINT16 outBfrLen;		    	/* size of output buffer */
	UINT32 outErrors;		    	/* count of IRP failures */

	/*
	 * since we don't use inIrp in this driver ,so we ust define here
	 * but not actually use it
	 * */
	USBD_PIPE_HANDLE inPipeHandle;  /* USBD pipe handle for bulk IN pipe */
	USB_IRP inIrp;				/* IRP to monitor input from printer */
	BOOL inIrpInUse;		    /* TRUE while IRP is outstanding */
	int	noOfInBfrs;	     		/* no of input buffers*/
	char * pInBfr;				/* pointer to input buffers */
	UINT16 inBfrLen;		    /* size of input buffer */
	UINT32 inErrors;		    /* count of IRP failures */

	UINT16 	inEpAddr;
	UINT16  outEpAddr;

	USB_IRP 		statusIrp;
	SEM_HANDLE cp210xIrpSem;


	char inQueue[CP210X_Q_DEPTH]; /* Circular queue for cp210x input*/
	UINT16 inQueueCount; /*count of characters in the queue*/
	UINT16 inQueueIn;   /*next location in queue*/
	UINT16 inQueueOut; /*next character to fetch*/

	int mode;		/*always SIO_MODE_POLL*/

	STATUS(*getTxCharCallback)();/*tx callback*/
	void *getTxCharArg;	/*tx callback argument*/

	STATUS(*putRxCharCallback)();/*rx callback*/
	void *putRxCharArg;/*rx callback argument*/

	BOOL txStall;   /* Indicates we ran out of TX IRP's */
	BOOL txActive;  /* Indicates there is a TX IRP in process with the USB Stack */

} CP210X_SIO_CHAN, *pCp210xSioChan;



/******************************************************************************
 * CP210X_ATTACH_CALLBACK defines a callback routine which will be
 * invoked by cp210xLib.c when the attachment or removal of a
 * USB-RS232 device is detected.  When the callback is invoked with an attach
 * code of CP210X_ATTACH, the nodeId represents the ID of newly added device.
 * When the attach code is CP210X_REMOVE, nodeId points to the device
 * which is no longer attached.
 */

typedef VOID (* CP210X_ATTACH_CALLBACK)(
	pVOID arg,			/* caller-defined argument */
	SIO_CHAN *pSioChan,	/* pointer to affected SIO_CHAN */
	UINT16 attachCode	/* defined as USB_TTY_xxxx */
);

/*function prototypes*/
STATUS cp210xDevInit (void);
STATUS cp210xDevShutdown (void);

STATUS cp210xDynamicAttachRegister(
	CP210X_ATTACH_CALLBACK callback,	/* new callback to be registered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS cp210xDynamicAttachUnRegister(
	CP210X_ATTACH_CALLBACK callback,	/* callback to be unregistered */
	pVOID arg							/* user-defined arg to callback */
);

STATUS cp210xSioChanLock(SIO_CHAN *pChan);

STATUS cp210xSioChanUnlock(SIO_CHAN *pChan);

#endif /* CP210XLIB_H_ */

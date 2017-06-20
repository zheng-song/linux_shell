/* usbPegasusEnd.c - USB Ethernet driver for the Pegasus USB-Ethernet adapter */

/* Copyright 2004 Wind River Systems, Inc. */

/*
modification history
--------------------
01u,26oct04,ami  Debug Messages Changes
01t,20oct04,ami  Fix for discoonection of Pegaus Device while loading
01s,15oct04,ami  Apigen Changes
01s,07oct04,hch  SPR #98211 98965 fix
01r,06oct04,ami  SPR #94684 Fix
01q,05oct04,pdg  SPR 75866 fix
01p,03aug04,pdg  Fixed coverity errors
01o,21jul04,hch  Fix SPR #99761
01n,07jun04,hch  Fix SPR #98112 for Pegasus multicast support
01n,07jun04,dgp  clean up formatting to correct doc build errors
01m,02dec03,nrv  fix diab compiler warning
01m,18dec03,cfc  Fix compiler warning error
01l,14oct03,nrv  complete fix for MAC endian issue
01k,08oct03,cfc  Fix for send buf alignment and queueing 
01j,07oct03,cfc  Fix for MAC Address ENDIAN issue
01i,27aug03,nrv  making mods for pegasus2
01h,06mar02,wef  remove more automatic buffers identify memory leak.
01g,14nov01,wef  remove automatic buffer creations and repalce with OSS_xALLOC
                 remove more warnings
01f,08aug01,dat  Removing warnings
01e,08aug01,dat  Removing compiler warnings
01d,09may01,wef  changed placement of dev structure connected flag being set
01c,01may01,wef  changed USB_DEV USB_PEGASUS_DEV
01b,28mar01,bri  Dynamic input and output Irp memory allocation added,
		 provsion for multiple device added.
01a,28jul00,bri  Created
*/

/*
DESCRIPTION

This module is the USB communication class, Ethernet Sub class driver 
for the vxWorks operating system. This module presents an interface which 
becomes an underlying layer of the vxWorks END (Enhanced Network Driver) 
model. It also adds certain APIs that are necessary for some additional 
 features supported by an usb - Ethernet adapter.
 
USB - Ethernet adapter devices are described in the USB Communication 
Devices class definitions. The USB - Ethernet adapter falls under the 
Ethernet Control model under the communications device class specification.
This driver is meant for the usb-ethernet adapters built around the 
Pegasus-ADM Tek AN986 chip.
  
 
DEVICE FUNCTIONALITY
 
The Pegasus USB to ethernet adapter chip ASIC provides bridge from USB 
to 10/100 MII and USB to 1M HomePNA network.The Pegasus Chip, is compliant 
with supports USB 1.0 and 1.1 specifications. This device supports 4 End 
Points. The first,is the default end point which is of control type (with 
max 8 byte packet). The Second and the Third are BULK IN (Max 64 Byte 
packet) and BULK OUT (Max 64 Byte Packet) end points for transfering the 
data into the Host and from the Host respectively. The Fourth End Point, 
is an Interrupt end point (Max 8 bytes) that is not currently used.
设备支持4个端点，首先是默认的控制端口（最大8字节）。第二个和第三个分别是bulk in
和bulk out端点（都是最大64byte）还有一个中断端点（最大8字节），中断并不常用。

This device supports One configuration which contains One Interface. This 
interface contains the 3 end points i.e. the Bulk IN/Out and interrupt
end points. 
该设备支持一个配置，该配置下有一个接口和3个端点。Bulk IN/OUT 和interrupt 端点。
 
Apart from the traditional commands, the device supports 3 Vendor specific 
commands. These commands are described in the Pegasus specification manual.
The device supports interface to EEPROM for storing the Ethernet MAC address 
and other configuration details. It also supports interface to SRAM for 
storing the packets received and to be transmitted.
除去通常的commands，该设备还支持3个Vendor specific commands。这些commands
在Pegasus指定的手册当中描述。
	该设备支持在接口的EEPROM中存储Ethernet的MAC地址和其他的配置细节，
还支持Interface到SRAM存储接收到的和传送的数据包

Packets are passed between the chip and host via bulk transfers.
There is an interrupt endpoint mentioned in the specification manual. However
it was not used. This device can work in 10Mbps half and Full duplex 
and 100 Mbps half and Full Duplex modes. The MAC supports a 64 entry
multicast filter. This device is IEEE 802.3 MII compliant and supports 
IEEE 802.3x flow control. It also supports for configurable threshold for 
transmitting PAUSE frame. Supports Wakeup frame, Link status change and
magic packet frame.

数据通过Bulk传输，Interrupt端点在实现中并没有使用。设备可以工作在10Mbps/100Mbps
的双工/半双工模式，MAC支持64条多播过滤。它还支持可配置的传输暂停帧阈值。
支持Wakeup 帧，链路状态转变。

The device supports the following (vendor specific)commands :

\is
\i 'USB_REQ_REG_GET'
Retrieves the Contents of the specified register from the device.

\i 'USB_REQ_REG_SET_SINGLE'
Sets the contents of the specified register (Single) in the device

\i 'USB_REQ_REG_SET_MULTIPLE'
Sets the contents of the specified register (Multiple) in the device
\ie

DRIVER FUNCTIONALITY
The function usbPegasusEndInit() is called at the time of usb system 
initialization. It registers as a client with the USBD. This function also
registers for the dynamic attachment and removal of the usb devices.
Ideally we should be registering for a specific Class ID and a Subclass
Id..but since the device doesn't support these parameters in the Device
descriptor, we register for ALL kinds of devices. 
We maintain a linked list of the ethernet devices on USB in 
a linked list "pegasusDevList". This list is created and maintained using 
the linked list library provided as a part of the USBD. Useful API calls 
are provided to find if the device exists in the list, by taking either
the device "nodeId" or the vendorId and productId as the parameters.
The Callback function registered for the dynamic attachment/removal, 
pegasusAttachCallback() will be called if any device is found on/removed
from the USB. This function first checks whether the device already exists 
in the List. If not, it will parse through the device descriptor, findout 
the Vendor Id and Product Id. If they match with Pegasus Ids, the device 
will be added to the list of ethernet devices found on the USB.

pegasusDevInit() does most of the device structure initialization 
afterwards. This routine checks if the device corresponding to the 
vendorId and productId matches to any of the devices in the "List". If yes
a pointer structure on the list will be assigned to one of the device
structure parameters. After this the driver will parse through the 
configuration descriptor, interface descriptor to findout the InPut and OutPut
end point details. Once we find these end point descriptors we create input 
and output Pipes and assign them to the corresponding structure. 
It then resets the device.

This driver, is a Polled mode driver as such. It keeps listening on the 
input pipe by calling "pegasusListenToInput" all the time, from the first
time it is called by pegasusStart(). This acquires a buffer from the 
endLayer and uses it in the IRP. Unless the IRP is cancelled (by 
pegasusStop()), it will be submitted again and again. If cancelled, it
will again start listening only if pegasusStart() is called. If there is 
data (IRP successfull), then it will be passed on to END by calling
pegasusEndRecv().

Rest of the functionality of the driver is straight forward and most of 
the places achieved by sending a vendor specific command from the list
described above, to the device.

INCLUDE FILES: 
end.h, endLib.h, lstLib.h, etherMultiLib.h, usb/usbPlatform.h, usb/usb.h, 
usb/usbListLib.h, usb/usbdLib.h, usb/usbLib.h, drv/usb/usbPegasusEnd.h

SEE ALSO:
muxLib, endLib,  usbLib, usbdLib, ossLib

\tb "Writing and Enhanced Network Driver" and
\tb "USB Developer's Kit User's Guide"

*/

/* includes */

#include "vxWorks.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "cacheLib.h"
#include "intLib.h"
#include "end.h"		/* Common END structures. */
#include "endLib.h"
#include "lstLib.h"		/* Needed to maintain protocol list. */
#include "wdLib.h"
#include "iv.h"
#include "semLib.h"
#include "etherLib.h"
#include "logLib.h"
#include "netLib.h"
#include "stdio.h"
#include "sysLib.h"
#include "errno.h"
#include "errnoLib.h"
#include "memLib.h"
#include "iosLib.h"
#include "etherMultiLib.h"		/* multicast stuff. */

#include "net/mbuf.h"
#include "net/unixLib.h"
#include "net/protosw.h"
#include "net/systm.h"
#include "net/if_subr.h"
#include "net/route.h"

#include "sys/socket.h"
#include "sys/ioctl.h"
#include "sys/times.h"

#include "usb/usbPlatform.h"
#include "usb/ossLib.h" 	/* operations system srvcs */
#include "usb/usb.h"		/* general USB definitions */
#include "usb/usbListLib.h"	/* linked list functions */
#include "usb/usbdLib.h"	/* USBD interface */
#include "usb/usbLib.h" 	/* USB utility functions */

#include "drv/usb/usbPegasusEnd.h"

/* defines */

/* for debugging */

#define PEGASUS_DBG

#ifdef	PEGASUS_DBG

#define PEGASUS_DBG_OFF			0x0000
#define PEGASUS_DBG_RX			0x0001
#define	PEGASUS_DBG_TX			0x0002
#define PEGASUS_DBG_MCAST		0x0004
#define	PEGASUS_DBG_ATTACH		0x0008
#define	PEGASUS_DBG_INIT		0x0010
#define	PEGASUS_DBG_START		0x0020
#define	PEGASUS_DBG_STOP		0x0040
#define	PEGASUS_DBG_RESET		0x0080
#define	PEGASUS_DBG_MAC			0x0100
#define	PEGASUS_DBG_POLL_RX		0x0200
#define	PEGASUS_DBG_POLL_TX		0x0400
#define	PEGASUS_DBG_LOAD		0x0800
#define	PEGASUS_DBG_IOCTL		0x1000
#define PEGASUS_DBG_DNLD		0x2000
#define PEGASUS_DBG_TX_STALL		0x4000

int	pegasusDebug = (0x0000);	

extern void usbLogMsg (char *,int,int,int,int,int,int);

#define PEGASUS_LOG(FLG, X0, X1, X2, X3, X4, X5, X6)                  \
        if (pegasusDebug & FLG)   \
            usbLogMsg(X0, X1, X2, X3, X4, X5, X6)


#define PEGASUS_PRINT(FLG,X)                             \
	if (pegasusDebug & FLG) printf X;

#else /*PEGASUS_DBG*/

#define PEGASUS_LOG(FLG, X0, X1, X2, X3, X4, X5, X6)

#define PEGASUS_PRINT(DBG_SW,X)

#endif /*PEGASUS_DBG*/


#define PEGASUS_CLIENT_NAME	"usbPegasusLib"

#define PEGASUS_BUFSIZ  	ETHERMTU + EH_SIZE + 6

#define EH_SIZE			(14)

#define END_SPEED_10M		10000000	/* 10Mbs */

#define PEGASUS_SPEED   	END_SPEED_10M

/* Constants for computing the Pegasus multicast hash table value */

#define PEGASUS_POLY        0xEDB88320
#define PEGASUS_BITS        6

/* A shortcut for getting the hardware address from the MIB II stuff. */

#define END_HADDR(pEnd)	\
		((pEnd)->mib2Tbl.ifPhysAddress.phyAddress)

#define END_HADDR_LEN(pEnd) \
		((pEnd)->mib2Tbl.ifPhysAddress.addrLength)


#define PEGASUS_MIN_FBUF	(1514)	/* min first buffer size */

#define USB_PEGASUS_BUF_SIZE    (1550)	/* transfer buffer size */

#define DELAY(i)\
	taskDelay(sysClkRateGet()*i);;

/* typedefs */

/*
 * This will only work if there is only a single unit, for multiple
 * unit device drivers these should be integrated into the PEGASUS_DEVICE
 * structure.
 */

M_CL_CONFIG pegasusMclBlkConfig = 	/* network mbuf configuration table */
    {
    /* 
    no. mBlks		no. clBlks	memArea		memSize
    -----------		----------	-------		-------
    */
    0, 			0, 		NULL, 		0
    };

CL_DESC pegasusClDescTbl [] = 	/* network cluster pool configuration table */
    {
    /* 
    clusterSize			num	memArea		memSize
    -----------			----	-------		-------
    */
    {PEGASUS_BUFSIZ,		0,	NULL,		0}
    }; 

int pegasusClDescTblNumEnt = (NELEMENTS(pegasusClDescTbl));

/* globals */

BOOL usbPegasusEndMemoryIsNotSetup = TRUE; /* memory setup flag */
NET_POOL * pUsbPegasusNetPool;		/* netPool pointer */
USBD_CLIENT_HANDLE pegasusHandle; 	/* our USBD client handle */
USBD_NODE_ID pegasusNodeID; 		/* our USBD node ID */

/* Locals */

LOCAL UINT16 		initCount = 0;	/* Count of init nesting */

LOCAL MUTEX_HANDLE 	pegasusMutex;	/* to protect internal structs */
LOCAL LIST_HEAD		pegasusDevList;	/* linked list of Device Structs */

LOCAL MUTEX_HANDLE 	pegasusTxMutex;	/* to protect internal structs */
LOCAL MUTEX_HANDLE 	pegasusRxMutex;	/* to protect internal structs */

LOCAL LIST_HEAD    reqList;           /* Attach callback request list */

/* forward declarartions */

/* END Specific Externally imported interfaces. */

IMPORT	int 	endMultiLstCnt 	(END_OBJ* pEnd);

IMPORT STATUS   taskDelay(int);

/* Externally visible interfaces. */

END_OBJ * 	usbPegasusEndLoad (char * initString);
STATUS 		usbPegasusEndInit 	(void);

/*  LOCAL functions */

LOCAL int	pegasusEndIoctl	  (PEGASUS_DEVICE * pDrvCtrl, 
				  int cmd, caddr_t data);

LOCAL STATUS	usbPegasusEndUnload (PEGASUS_DEVICE * pDrvCtrl);
LOCAL STATUS	pegasusEndSend	    (PEGASUS_DEVICE * pDrvCtrl, M_BLK_ID pBuf);
			  
LOCAL STATUS	pegasusEndMCastAdd (PEGASUS_DEVICE * pDrvCtrl, char * pAddress);
LOCAL STATUS	pegasusEndMCastDel (PEGASUS_DEVICE * pDrvCtrl, char * pAddress);
LOCAL STATUS	pegasusEndMCastGet (PEGASUS_DEVICE * pDrvCtrl, 
				   MULTI_TABLE * pTable);
LOCAL STATUS	pegasusEndPollSend (PEGASUS_DEVICE * pDrvCtrl, M_BLK_ID pBuf);
LOCAL STATUS	pegasusEndPollRcv  (PEGASUS_DEVICE * pDrvCtrl, M_BLK_ID pBuf);
LOCAL STATUS	pegasusEndParse	(PEGASUS_DEVICE * pDrvCtrl, char * initString,
				UINT16 * pVendorId, UINT16 * pProductId );
LOCAL STATUS	pegasusEndMemInit  (PEGASUS_DEVICE * pDrvCtrl);
LOCAL STATUS	pegasusEndConfig   (PEGASUS_DEVICE * pDrvCtrl);
LOCAL STATUS 	pegasusDevInit 	   (PEGASUS_DEVICE * pDevCtrl, UINT16 vendorId, 
			  	    UINT16 productId);

LOCAL STATUS 	pegasusEndStart	(PEGASUS_DEVICE * pDevCtrl);
LOCAL STATUS 	pegasusEndStop	(PEGASUS_DEVICE * pDevCtrl);
LOCAL void 	pegasusTxCallback	(pVOID p);
LOCAL void 	pegasusRxCallback	(pVOID p);
LOCAL STATUS 	pegasusShutdown 	(int errCode);
LOCAL STATUS 	pegasusSend	(PEGASUS_DEVICE * pDevCtrl,UINT8* pBfr,	
				UINT32 size);

LOCAL STATUS 	pegasusListenForInput   (PEGASUS_DEVICE * pDevCtrl);
LOCAL USB_PEGASUS_DEV * pegasusFindDevice	(USBD_NODE_ID nodeId);
LOCAL USB_PEGASUS_DEV * pegasusEndFindDevice 	(UINT16 vendorId, 
						 UINT16 productId);
LOCAL VOID 	pegasusDestroyDevice 	(PEGASUS_DEVICE * pDevCtrl);
LOCAL STATUS 	pegasusMCastFilterSet	(PEGASUS_DEVICE * pDevCtrl);

LOCAL STATUS 	pegasusAttachCallback (USBD_NODE_ID nodeId, UINT16 attachAction, 
	    			      UINT16 configuration, UINT16 interface,
	    			      UINT16 deviceClass, UINT16 deviceSubClass, 
	    			      UINT16 deviceProtocol );

LOCAL STATUS 	pegasusEndRecv    (PEGASUS_DEVICE * pDrvCtrl, UINT8 *  pData,              
	    			  UINT32  len );

LOCAL STATUS usbPegasusInit	(USBD_NODE_ID devId, UINT8 * macAdrs);
LOCAL STATUS usbPegasusReadPhy  (USBD_NODE_ID devId, UINT8 offSet, 
                                UINT16 * phyWord);
LOCAL STATUS usbPegasusWritePhy (USBD_NODE_ID devId, UINT8 direction, 
                                UINT8 offSet, UINT16 phyWord);

LOCAL STATUS usbPegasusReadSrom (USBD_NODE_ID devId, UINT8 offSet,
                                UINT16 * phyWord);

LOCAL VOID notifyAttach        (USB_PEGASUS_DEV * pDev, UINT16 attachCode);
BOOL pegasusOutputIrpInUse     (PEGASUS_DEVICE *pDevCtrl);

LOCAL int pegasusMchash        (UINT8 * addr); 

/*
 * Our driver function table.  This is static across all driver
 * instances.
 */

LOCAL NET_FUNCS pegasusEndFuncTable =
    {
    (FUNCPTR) pegasusEndStart,	   /* Function to start the device. */
    (FUNCPTR) pegasusEndStop,      /* Function to stop the device. */
    (FUNCPTR) usbPegasusEndUnload, /* Unloading function for the driver. */
    (FUNCPTR) pegasusEndIoctl,	   /* Ioctl function for the driver. */
    (FUNCPTR) pegasusEndSend,      /* Send function for the driver. */
    (FUNCPTR) pegasusEndMCastAdd, /* Multicast add function for the */
				  /* driver. */
    (FUNCPTR) pegasusEndMCastDel, /* Multicast delete function for */
				  /* the driver. */
    (FUNCPTR) pegasusEndMCastGet, /* Multicast retrieve function for */
				  /* the driver. */
    (FUNCPTR) pegasusEndPollSend, /* Polling send function */
    (FUNCPTR) pegasusEndPollRcv,  /* Polling receive function */
    endEtherAddressForm,	/* put address info into a NET_BUFFER */
    endEtherPacketDataGet,	/* get pointer to data in NET_BUFFER */
    endEtherPacketAddrGet  	/* Get packet addresses. */
    };

#define ADDR_LOGGING 0
#if ADDR_LOGGING

#define ADDR_BUF_SIZE 4096

UINT32 addrBuf [ADDR_BUF_SIZE]; 
UINT32 valueBuf [ADDR_BUF_SIZE]; 
UINT16 bufferCounter = 0;

void logAddr 
    (
    UINT32 value,
    UINT8 * addr 
    )
    {

    if (bufferCounter == ADDR_BUF_SIZE) 
	bufferCounter = 0;	

    addrBuf [bufferCounter] = addr; 
    valueBuf [bufferCounter++] = value; 

    }

void displayAddr (void)
    {
    UINT16 i, j=0;

    for (i=0; i<bufferCounter; i++)
	{
	printf ("%d", valueBuf [i]);
	printf (" - 0x%x ", addrBuf [i]);
	j++;
	if (j == 4)
	    {
	    printf ("\n");
	    j=0;
	    }
	}
    printf ("\n");

    }

#endif

/***************************************************************************
*
* pegasusAttachCallback - gets called for attachment/detachment of devices
*
* The USBD will invoke this callback when a USB ethernet device is 
* attached to or removed from the system.  
* <nodeId> is the USBD_NODE_ID of the node being attached or removed.	
* <attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* Communication device functionality resides at the interface level, with
* the exception of being the definition of the Communication device class
* code.so <configuration> and <interface> will indicate the configuration
* or interface that reports itself as a network device.
* <deviceClass> and <deviceSubClass> will match the class/subclass for 
* which we registered.  
* <deviceProtocol> doesn't have meaning for the ethernet devices so we 
* ignore this field.
*
* NOTE
* The USBD invokes this function once for each configuration or
* interface which reports itself as a network device.
* So, it is possible that a single device insertion or removal may trigger 
* multiple callbacks.  We ignore all callbacks except the first for a 
* given device.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS  pegasusAttachCallback
    (
    USBD_NODE_ID nodeId, 
    UINT16 attachAction, 
    UINT16 configuration,
    UINT16 interface,
    UINT16 deviceClass, 
    UINT16 deviceSubClass, 
    UINT16 deviceProtocol
    )
    {
    USB_PEGASUS_DEV * pNewDev;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT16 vendorId;
    UINT16 productId;

    int noOfSupportedDevices = (sizeof (pegasusAdapterList) /
				 (2 * sizeof (UINT16)));
    int index = 0;

    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_ATTACH, "pegasusAttachCallback: could not"
		    "allocate pBfr\n", 0, 0, 0, 0, 0, 0);
	
	return ERROR;
	}

    OSS_MUTEX_TAKE (pegasusMutex, OSS_BLOCK);

    switch (attachAction)
	{
	case USBD_DYNA_ATTACH :
	
	    /* a new device is found */

	    PEGASUS_LOG (PEGASUS_DBG_ATTACH, "New Device found : %0x Class "
			"%0x Subclass %0x Protocol %0x Configuration "
			"%0x Interface  %0x nodeId \n", deviceClass,
			deviceSubClass, deviceProtocol, configuration,
			interface, (UINT)nodeId);

	    /* First Ensure that this device is not already on the list */
	
	    if (pegasusFindDevice (nodeId) != NULL)
		{
		PEGASUS_LOG (PEGASUS_DBG_ATTACH, "Device already exists, \n",
		    0, 0, 0, 0, 0, 0);
	        break;
		}

	    /* Now, we have to ensure that its a PEGASUS device */
	
            if (usbdDescriptorGet (pegasusHandle, 	
				   nodeId, 
				   USB_RT_STANDARD | USB_RT_DEVICE, 
				   USB_DESCR_DEVICE, 
				   0, 
				   0, 
				   30, 
				   pBfr, 
				   &actLen) 
				!= OK)
	        {
		break;
            	}

            vendorId = USB_PEGASUS_SWAP_16 (((pUSB_DEVICE_DESCR) pBfr)->vendor);

	    productId = \
		USB_PEGASUS_SWAP_16 (((pUSB_DEVICE_DESCR) pBfr)->product);		
            for (index = 0; index < noOfSupportedDevices; index++)
		if (vendorId == pegasusAdapterList[index][0])
		    if (productId == pegasusAdapterList[index][1])
			break;
            
	    if (index == noOfSupportedDevices )
		{

		/* device not supported */

		PEGASUS_LOG (PEGASUS_DBG_ATTACH, " Unsupported "
			     "device found vId %0x; pId %0x!!! \n", 
			     vendorId, productId, 0, 0, 0, 0);
		break;
		}
		
	      PEGASUS_LOG (PEGASUS_DBG_ATTACH, " Found a PEGASUS "
		"Adapter!!!  %0x  %0x\n", vendorId, productId, 0, 0, 0, 0);
	
	    /* 
	     * Now create a strcture for the newly found device and add 
	     * it to the linked list 
	     */

            /* Try to allocate space for a new device struct */

	    if ((pNewDev = OSS_CALLOC (sizeof (USB_PEGASUS_DEV))) == NULL)
 	    	{
		break;	    	
		}

	    /* Fill in the device structure */
	
	    pNewDev->nodeId = nodeId;
	    pNewDev->configuration = configuration;
	    pNewDev->interface = interface;
	    pNewDev->vendorId = vendorId;
	    pNewDev->productId = productId;

            /* Save a global copy of NodeID used by DIAGs */
            pegasusNodeID = nodeId;

	    /* Add this device to the linked list */
	
	    usbListLink (&pegasusDevList, pNewDev, &pNewDev->devLink, 
			LINK_TAIL);

	    /* Mark the device as being connected */

	    pNewDev->connected = TRUE;

            /* Notify registered callers that a PEGASUS has been added */

	    notifyAttach (pNewDev, USB_PEGASUS_ATTACH); 	
	    
	    PEGASUS_LOG (PEGASUS_DBG_ATTACH, " notify attach got called \n",
		          0, 0, 0, 0, 0, 0);

	    break;

	case USBD_DYNA_REMOVE:
	
            PEGASUS_LOG (PEGASUS_DBG_ATTACH, " pegasusAttachCallback "
			"USBD_DYNA_REMOVE case  \n", 0, 0, 0, 0, 0, 0);
	

	    /* First Ensure that this device is on the list */
	
	    if ((pNewDev = pegasusFindDevice (nodeId)) == NULL)
	    	{
	        PEGASUS_LOG (PEGASUS_DBG_ATTACH, "Device not found \n",
			 0, 0, 0, 0, 0, 0);
	        break;
	        }

	    if (pNewDev->connected == FALSE)
		{
         	PEGASUS_LOG (PEGASUS_DBG_ATTACH, "Device not connected \n",
			 0, 0, 0, 0, 0, 0);
	        
                break;    
		}
	

	    pNewDev->connected = FALSE;

	    /*
	     * we need not check for the vendor id/product id as if 
             * the device is on the list, then it is our device only.
	     */

            pNewDev->lockCount++;

            notifyAttach (pNewDev, USB_PEGASUS_REMOVE); 
       

            pNewDev->lockCount--; 

            /* De allocate Memory for the device */

	    if (pNewDev->lockCount == 0)
		pegasusDestroyDevice((PEGASUS_DEVICE *)pNewDev->pPegasusDev);	

	    break;	    

	}

    OSS_FREE (pBfr);

    OSS_MUTEX_RELEASE (pegasusMutex);

    return OK;

    }
		
/***************************************************************************
*
* usbPegasusEndInit - initializes the pegasus library
*
* Initizes the pegasus library. The library maintains an initialization
* count so that the calls to this function might be nested.
*
* This function initializes the system resources required for the library
* initializes the linked list for the ethernet devices found.
* This function reegisters the library as a client for the usbd calls and 
* registers for dynamic attachment notification of usb communication device
* class and Ethernet sub class of devices.
*
* RETURNS : OK if successful, ERROR if failure 
*
* ERRNO: 
* \is
* \i S_usbPegasusLib_OUT_OF_RESOURCES
* Sufficient Resources not Available
*
* \i S_usbPegasusLib_USBD_FAULT
* Fault in the USBD Layer
* \ie
*/

STATUS usbPegasusEndInit(void)
    {
    
    /* see if already initialized. if not, initialize the library */

    initCount++;

    if(initCount != 1)		/* if its the first time */
	return OK;

    memset (&pegasusDevList, 0, sizeof (pegasusDevList));
    
    pegasusMutex = NULL;
    pegasusTxMutex = NULL;
    pegasusRxMutex = NULL;
    pegasusHandle = NULL;

    /* create the mutex */

    if (OSS_MUTEX_CREATE (&pegasusMutex) != OK)
	return pegasusShutdown (S_usbPegasusLib_OUT_OF_RESOURCES);
    
    if (OSS_MUTEX_CREATE (&pegasusTxMutex) != OK)
	return pegasusShutdown (S_usbPegasusLib_OUT_OF_RESOURCES);
    
    if (OSS_MUTEX_CREATE (&pegasusRxMutex) != OK)
	return pegasusShutdown (S_usbPegasusLib_OUT_OF_RESOURCES);

    /* 
     * Register the Library as a Client and register for 
     * dynamic attachment callback.
     */

        if((usbdClientRegister (PEGASUS_CLIENT_NAME, &pegasusHandle) != OK) ||
	(usbdDynamicAttachRegister (pegasusHandle, 0, 0, 0,
			(USBD_ATTACH_CALLBACK) pegasusAttachCallback) != OK))
	{
	logMsg(" Registration Failed..\n", 0, 0, 0, 0, 0, 0);
    	return pegasusShutdown (S_usbPegasusLib_USBD_FAULT);
	}
    
    return OK;
    }

/***************************************************************************
*
* findEndpoint - searches for a BULK endpoint of the indicated direction.
*
* This function searches for the endpoint of indicated direction
*
* RETURNS: a pointer to matching endpoint descriptor or NULL if not found.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_ENDPOINT_DESCR findEndpoint
    (
    pUINT8 pBfr,		/* buffer to search for */
    UINT16 bfrLen,		/* buffer length */
    UINT16 direction		/* end point direction */
    )
    {
    pUSB_ENDPOINT_DESCR pEp;

    while ((pEp = (pUSB_ENDPOINT_DESCR) 
	    usbDescrParseSkip (&pBfr, &bfrLen, USB_DESCR_ENDPOINT)) 
		!= NULL)
	{
	if ((pEp->attributes & USB_ATTR_EPTYPE_MASK) == USB_ATTR_BULK &&
	    (pEp->endpointAddress & USB_ENDPOINT_DIR_MASK) == direction)
	    break;
	}

    return pEp;
    }

/***************************************************************************
*
* pegasusDevInit - initializes the pegasus Device structure.
*
* This function initializes the usb ethernet device. It is called by 
* usbPegasusEndLoad() as part of the end driver initialzation. 
* usbPegasusEndLoad() expects this routine to perform all the device and usb 
* specific initialization and fill in the corresponding member fields in the 
* PEGASUS_DEVICE structure.
*
* This function first checks to see if the device corresponding to
* <vendorId> and <productId> exits in the linkedlist pegasusDevList.
* It allocates memory for the input and output buffers. The device 
* descriptors will be retrieved and are used to findout the IN and OUT
* bulk end points. Once the end points are found, the corresponding
* pipes are constructed. The Pipe handles are stored in the device 
* structure <pDevCtrl>. The device's Ethernet Address (MAC address) 
* will be retrieved and the corresponding field in the device structure
* will be updated. This is followed by setting up of the parameters 
* like Multicast address filter list, Packet Filter bitmap etc.
*
* RETURNS : OK if every thing succeds and ERROR if any thing fails.
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL STATUS pegasusDevInit
    (
    PEGASUS_DEVICE * pDevCtrl,	/* the device structure to be updated */
    UINT16 vendorId,		/* manufacturer id of the device */
    UINT16 productId    	/* product id of the device */
    )
    {

    USB_PEGASUS_DEV * pNewDev;
    USB_CONFIG_DESCR * pCfgDescr;	
    USB_INTERFACE_DESCR * pIfDescr;
    USB_ENDPOINT_DESCR * pOutEp;
    USB_ENDPOINT_DESCR * pInEp;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    PEGASUS_ENET_IRP* pIrpBfrs;
    UINT8** pInBfr;
    UINT16 actLen;
    int index, otherIndex = 0;
    UINT8 ** pOutBfr;

    if(pDevCtrl == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Null Device \n", 0, 0, 0, 0, 0, 0);
	return ERROR;
	}

    /* Find if the device is in the found devices list */
   
    if ((pNewDev = pegasusEndFindDevice (vendorId,productId)) == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Could not find device..\n",
		     0, 0, 0, 0, 0, 0);
	return ERROR;
	}

    /* Link the End Structure and the device that is found */

    pDevCtrl->pDev = pNewDev;

    pNewDev->pPegasusDev = pDevCtrl;
    
    /* Allocate memory for the input and output buffers..*/

    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Could not allocate memory for Get"
		     "descriptor.\n", 0, 0, 0, 0, 0, 0);

	return ERROR;
	}

    if ((pIrpBfrs = OSS_MALLOC (pDevCtrl->noOfIrps * \
			         sizeof (PEGASUS_ENET_IRP)))  \
			     == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Could not allocate memory for IRPs.\n",
		     0, 0, 0, 0, 0, 0);

	OSS_FREE (pBfr);

	return ERROR;
	}

    if ((pInBfr = OSS_MALLOC (pDevCtrl->noOfInBfrs * sizeof (char *))) ==NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Could Not align Memory for pInBfrs ...\n"
		      , 0, 0, 0, 0, 0, 0);

	OSS_FREE (pBfr);

	OSS_FREE (pIrpBfrs);

	return ERROR;
        }

    for (index=0;index<pDevCtrl->noOfInBfrs;index++)
	{
	if((pInBfr[index] = OSS_MALLOC (PEGASUS_IN_BFR_SIZE+8)) == NULL)
	    {
	    PEGASUS_LOG (PEGASUS_DBG_INIT,"Could Not align Memory "
                        "for InBfrs  %d...\n", index, 0, 0, 0, 0, 0);

   	    OSS_FREE (pBfr);

	    OSS_FREE (pIrpBfrs);

	    for (otherIndex=0; otherIndex<index; otherIndex++)
		OSS_FREE (pInBfr[otherIndex]);
 
	    OSS_FREE (pInBfr);

	    return ERROR;
	    }
	}

    if ((pOutBfr = OSS_MALLOC (pDevCtrl->noOfIrps * sizeof (char *))) ==NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT,"Could Not align Memory for pOutBfrs ...\n"
		      , 0, 0, 0, 0, 0, 0);

	OSS_FREE (pBfr);

	OSS_FREE (pIrpBfrs);
    
    for (index=0; index<pDevCtrl->noOfInBfrs; index++)
    OSS_FREE (pInBfr[index]);

    OSS_FREE (pInBfr);

	return ERROR;
        }

    for (index=0;index<pDevCtrl->noOfIrps;index++)
	{
	if((pOutBfr[index] = OSS_MALLOC (USB_PEGASUS_BUF_SIZE+8)) == NULL)
	    {
	    PEGASUS_LOG (PEGASUS_DBG_INIT,"Could Not align Memory "
                        "for OutBfrs  %d...\n", index, 0, 0, 0, 0, 0);

   	    OSS_FREE (pBfr);

	    OSS_FREE (pIrpBfrs);

	    for (otherIndex=0; otherIndex<index; otherIndex++)
		OSS_FREE (pOutBfr[otherIndex]);
 
	    OSS_FREE (pOutBfr);
        
        for (otherIndex=0; otherIndex<pDevCtrl->noOfInBfrs; otherIndex++)
        OSS_FREE (pInBfr[index]);

        OSS_FREE (pInBfr);

	    return ERROR;
	    }
	}


    pDevCtrl->pEnetIrp = pIrpBfrs;	
    pDevCtrl->pInBfrArray = pInBfr;
    pDevCtrl->pOutBfrArray = pOutBfr;

    for (index = 0; index < pDevCtrl->noOfIrps; index++)
	{
	pIrpBfrs->outIrpInUse = FALSE;
	pIrpBfrs ++;
	}

    pDevCtrl->rxIndex = 0;    
    pDevCtrl->txIrpIndex = 0;
    pDevCtrl->txStall = FALSE;
    pDevCtrl->txActive = FALSE;

    pDevCtrl->outBfrLen = PEGASUS_OUT_BFR_SIZE;
    pDevCtrl->inBfrLen = PEGASUS_IN_BFR_SIZE;


    /* 
     * Now we decifer the descriptors provided by the device ..
     * we try finding out what end point is what..
     * that doesn't mean we won't assume any thing, we assume certain
     * things though..like there are no any alternate settings for interfaces
     * etc..
     */

    /* To start with, get the configuration descriptor ..*/

    if (usbdDescriptorGet (pegasusHandle, 
			   pNewDev->nodeId, 
			   USB_RT_STANDARD | USB_RT_DEVICE, 
			   USB_DESCR_CONFIGURATION, 
			   0, 
			   0, 
			   USB_MAX_DESCR_LEN, 
			   pBfr, 
			   &actLen) 
			!= OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Could not GET Desc...\n",
		     0, 0, 0, 0, 0, 0);

	goto errorExit;
	}

    if ((pCfgDescr = usbDescrParse (pBfr, 	
				    actLen, 	
				    USB_DESCR_CONFIGURATION)) 	
				  == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Could not find Config. Desc...\n",
		     0, 0, 0, 0, 0, 0);

	goto errorExit;
	}

    /*
     * Since we registered for NOTIFY_ALL for attachment of devices,
     * the configuration no and interface number as reported by the 
     * call back function doesn't have any meanning.
     * we refer to the PEGASUS document and it says, it has only one 
     * interface with number 0. So the first (only) interface we find 
     * is the interface we want. 
     * If there are more interfaces and one of them meet our requirement
     * (as reported by callback funtction), then we need to parse
     * until we find our one..
     */
        
    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */

    pScratchBfr = pBfr;	    

    if ((pIfDescr = usbDescrParseSkip (&pScratchBfr, 	
				       &actLen, 
				       USB_DESCR_INTERFACE)) 
				    == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Could not find Intrface Desc.\n",
		     0, 0, 0, 0, 0, 0);

	goto errorExit;
	}
    
    /* Find out the output and input end points ..*/

    if ((pOutEp = findEndpoint (pScratchBfr, 
				actLen, 
				USB_ENDPOINT_OUT)) 
			    == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "No Output End Point \n",
		     0, 0, 0, 0, 0, 0);
 
        goto errorExit;
	}

    if ((pInEp = findEndpoint (pScratchBfr, 
			       actLen, 
			       USB_ENDPOINT_IN)) 
			     == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "No Input End Point \n",
		     0, 0, 0, 0, 0, 0);
 
        goto errorExit;
	}

    pDevCtrl->maxPower = pCfgDescr->maxPower; 

    /*
     * Now, set the configuration.
     */

    if (usbdConfigurationSet (pegasusHandle, pNewDev->nodeId,
	pCfgDescr->configurationValue, 			
	pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) != OK)
	{	 
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Could not set configutation. \n",
		     0, 0, 0, 0, 0, 0);

        goto errorExit;
	}

    /* Now go off and initialize the chip and read the mac address */

    if (usbPegasusInit (pNewDev->nodeId, (UINT8 *) pBfr)  != OK)
        {
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Could not initialize \n",
		     0, 0, 0, 0, 0, 0);

        goto errorExit;
        }

    pDevCtrl->macAdrs[0] = *pBfr;
    pDevCtrl->macAdrs[1] = *(pBfr + 1);
    pDevCtrl->macAdrs[2] = *(pBfr + 2);
    pDevCtrl->macAdrs[3] = *(pBfr + 3);
    pDevCtrl->macAdrs[4] = *(pBfr + 4);
    pDevCtrl->macAdrs[5] = *(pBfr + 5);

    PEGASUS_LOG (PEGASUS_DBG_INIT, "EthernetID %02x %02x %02x "
		    "%02x %02x %02x\n", *pBfr, *(pBfr + 1), 
		    *(pBfr + 2), *(pBfr + 3), *(pBfr + 4), *(pBfr + 5));

 
    /* Now, Create the Pipes.. */

    if (usbdPipeCreate (pegasusHandle, 
			pNewDev->nodeId, 
			pOutEp->endpointAddress, 
			pCfgDescr->configurationValue, 
			pNewDev->interface, 
			USB_XFRTYPE_BULK, 
			USB_DIR_OUT, 
			FROM_LITTLEW (pOutEp->maxPacketSize), 
			0, 
			0, 
			&pDevCtrl->outPipeHandle) 
		!= OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Pipe O/P coud not be created \n",
		     0, 0, 0, 0, 0, 0);
 
        goto errorExit;
	}
    
    if (usbdPipeCreate (pegasusHandle, 
			pNewDev->nodeId, 
			pInEp->endpointAddress, 
			pCfgDescr->configurationValue, 
			pNewDev->interface, 
			USB_XFRTYPE_BULK, 
			USB_DIR_IN, 
			FROM_LITTLEW (pInEp->maxPacketSize), 
			0, 
			0, 
			&pDevCtrl->inPipeHandle) 
		!= OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_INIT, "Pipe I/P coud not be created \n",
	     	     0, 0, 0, 0, 0, 0);
 
        goto errorExit;
	}

    pDevCtrl->inIrpInUse = FALSE;


    OSS_FREE (pBfr);

    return OK;

errorExit:

    OSS_FREE (pBfr);

    if (pDevCtrl->pEnetIrp != NULL)
        OSS_FREE (pDevCtrl->pEnetIrp);

    for (index=0; index<pDevCtrl->noOfInBfrs; index++)
        OSS_FREE (pInBfr[index]);

    OSS_FREE (pInBfr);

    return ERROR;

    }

/***************************************************************************
*
* pegasusEndStart - starts communications over Ethernet via usb (device)
*
* Since we don't have any interrupt, we have to prevent the device from 
* communicating, in some other way. Here, since the reception is based 
* on polling, and we do the pegasusListenForInput() for such polling,
* we can delay listening to any packet coming in, by having
* this function called in pegasusEndStart()... This way, we try to mimic the
* traditional "interrupt enabling". This is for packet reception.
*
* For packet transmission, we use the communicateOk flag. We transmit data 
* only if this flag is true. This flag will be set to TRUE in pegasusEndStart().
* and will be reset to FALSE in pegasusEndStop().
*
* RETURNS : OK or ERROR
*
* ERRNO: NONE
*
*\NOMANUAL
*/

LOCAL STATUS pegasusEndStart
    (
    PEGASUS_DEVICE * pDevCtrl	        /* Device to be started */
    )
    {

    PEGASUS_LOG (PEGASUS_DBG_START, "pegasusEndStart:..entered.\n",
		 0, 0, 0, 0, 0, 0);
    
    /*
     * what we do here is that we start listening on the 
     * BULK input pipe..for any ethernet packet coming in.
     * This is just simulating "connecting the interrupt" in End Model.
     */

    pDevCtrl->communicateOk = TRUE;

    PEGASUS_LOG (PEGASUS_DBG_START, "pegasusListenForInput:..entered.\n",
		 0, 0, 0, 0, 0, 0);
    if (pegasusListenForInput (pDevCtrl) != OK)
	{
        pDevCtrl->communicateOk = FALSE;
        PEGASUS_LOG (PEGASUS_DBG_START, "pegasusEndStart:..Unable to listen "
		     "for input.\n",0, 0, 0, 0, 0, 0);
	return ERROR;
	}

    /* 
     * The above will effectively preempt any possibility of packet reception,
     * before the pegasusEndStart is called.
     * for preempting such possiblity for transmission, we use the 
     * communicateOk flag. Ideally we should use a semaphore here but
     * i thought flag will suffice.
     */

    return OK;

    }

/***************************************************************************
*
* pegasusEndStop - disables communication over Ethernet via usb (device)
* 
* The pipes will be aborted. If there are any pending transfers on these
* Pipes, usbd will see to it that theu are also cancelled. The IRPs for
* these transfers will be returned a S_usbHcdLib_IRP_CANCELED error code.
* It waits for the IRPs to be informed of their Cancelled Status and then
* the function returns.
* 
* RETURNS: OK or ERROR
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndStop
    (
    PEGASUS_DEVICE * pDevCtrl	    /* Device to be Stopped */
    )
    {
    pATTACH_REQUEST  pRequest; 	

    PEGASUS_LOG (PEGASUS_DBG_STOP, "pegasusEndStop:..entered.\n",
		 0, 0, 0, 0, 0, 0);

    /* mark the interface as down */

    END_FLAGS_CLR (&pDevCtrl->endObj, (IFF_UP | IFF_RUNNING));

    /* 
     * We will Abort the transfers the input and output Pipes..
     * once we issue such requests, usbd will take care of aborting the 
     * outstanding transfers, if any, associated with the Pipes.
     */

    pDevCtrl->communicateOk = FALSE;

    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
	{
	usbListUnlink (&pRequest->reqLink);
	OSS_FREE (pRequest); 
	}

    return OK;

    }

/***************************************************************************
*
* pegasusListenForInput - listens for data on the ethernet (Bulk In Pipe) 
*
* Input IRP will be initialized to listen on the BULK input pipe and will
* be submitted to the usbd.
*
* RETURNS : OK if successful or ERROR on failure.
*
* ERRNO: none
*
* \NOMANUAL
*/
     
LOCAL STATUS pegasusListenForInput
    (
    PEGASUS_DEVICE * pDevCtrl		/* device to receive from */
    )
    {

    

    pUSB_IRP pIrp = NULL;

    if (pDevCtrl == NULL)
	return ERROR;

    pIrp = &pDevCtrl->inIrp;

    /* Initialize IRP */

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pDevCtrl;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = pegasusRxCallback;
    pIrp->timeout = USB_TIMEOUT_NONE;
    pIrp->transferLen = pDevCtrl->inBfrLen; 
    pIrp->flags = USB_FLAG_SHORT_OK;

    pIrp->bfrCount = 1;

    pIrp->bfrList[0].pid = USB_PID_IN;
    pIrp->bfrList[0].bfrLen = pDevCtrl->inBfrLen; 
    
    pIrp->bfrList[0].pBfr = (pUINT8)pDevCtrl->pInBfrArray[pDevCtrl->rxIndex];  
    
    /* Submit IRP */

    if (usbdTransfer (pegasusHandle, pDevCtrl->inPipeHandle, pIrp) != OK)
	return ERROR;

    pDevCtrl->inIrpInUse = TRUE;

    return OK;
    }

/***************************************************************************
*
* pegasusSend - initiates data transmission to the device
*
* This function initiates transmission on the ethernet.
*
* RETURNS: OK, or ERROR if unable to initiate transmission
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusSend
    (
    PEGASUS_DEVICE * pDevCtrl,		/* device to send to */
    UINT8 * pBfr,			/* data to send */
    UINT32 size				/* data size */
    )
    {

    pUSB_IRP pIrp;
    PEGASUS_ENET_IRP* pIrpBfr;	
    int index;

    PEGASUS_LOG (PEGASUS_DBG_TX, "pegasusSend:..entered. %d bytes\n", 
		size, 0, 0, 0, 0, 0); 

    if ((pDevCtrl == NULL) || (pBfr == NULL))
	{
	return ERROR;
	}

    if (size == 0)
	return ERROR;

    END_TX_SEM_TAKE(&pDevCtrl->endObj,WAIT_FOREVER);

    pIrpBfr = pDevCtrl->pEnetIrp + pDevCtrl->txIrpIndex;
    if (pIrpBfr->outIrpInUse) 
        {
        pDevCtrl->txStall = TRUE;
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, "pegasusSend: No TX IRP's available\n", 
            0, 0, 0, 0, 0, 0); 
        END_TX_SEM_GIVE(&pDevCtrl->endObj);
        return(END_ERR_BLOCK);
        }


    pIrp = &pIrpBfr -> outIrp;
    pIrpBfr->outIrpInUse = TRUE;

    PEGASUS_LOG (PEGASUS_DBG_TX, "pegasusSend: %d Irp put to use\n", 
		pDevCtrl->txIrpIndex, 0, 0, 0, 0, 0); 
    index = pDevCtrl->txIrpIndex;
    pDevCtrl->txIrpIndex++;
    pDevCtrl->txIrpIndex %= pDevCtrl->noOfIrps;

    /* Initialize IRP */
    if ((size % 64) == 0)
        size++;

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pDevCtrl;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = pegasusTxCallback;
    pIrp->timeout = USB_TIMEOUT_NONE;
    pIrp->transferLen = size;

    pIrp->bfrCount = 1;
    pIrp->bfrList [0].pid = USB_PID_OUT;
    pIrp->bfrList [0].pBfr = pBfr;
    pIrp->bfrList [0].bfrLen = size;

    /* Submit IRP */

    if (!pDevCtrl->txActive) 
        {
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, 
            "pegasusSend: Submitting 1st IRP %d %x %d\n", 
            index, (int)pIrp, size, 0, 0, 0); 
        if (usbdTransfer (pegasusHandle, pDevCtrl->outPipeHandle, pIrp) != OK)
            {
            END_TX_SEM_GIVE(&pDevCtrl->endObj);
            return ERROR;
            }
        pDevCtrl->txActive = TRUE;
        }
    else
        {
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, "pegasusSend: Queueing IRP %d\n", 
            index, 0, 0, 0, 0, 0); 
        }
    
    END_TX_SEM_GIVE(&pDevCtrl->endObj);

    return OK;
    }

/*******************************************************************************
* pegasusMuxTxRestart - place muxTxRestart on netJobRing
*
* This function places the muxTxRestart on netJobRing
*
* RETURNS: N/A
*
* ERRNO: none
*/

void pegasusMuxTxRestart
    (
    END_OBJ *  pEndObj	/* pointer to DRV_CTRL structure */
    )
    {
        muxTxRestart(pEndObj);
        return;
    }

/***************************************************************************
*
* pegasusTxCallback - Invoked upon Transmit IRP completion/cancellation
*
* This function is invoked upon Transmit IRP completion/cancellation
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/


LOCAL VOID pegasusTxCallback
    (
    pVOID p			/* completed IRP */
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;

    int index = 0;
    int nextIndex;
    PEGASUS_ENET_IRP* pIrpBfr;

    PEGASUS_DEVICE * pDevCtrl = pIrp->userPtr;

    /* Output IRP completed */

    for (index = 0; index < pDevCtrl->noOfIrps; index++)
	{
	pIrpBfr = pDevCtrl->pEnetIrp + index;	
	if (pIrp == &pIrpBfr->outIrp)
	    {
	    break;
	    }
	}
   
    if (index == pDevCtrl->noOfIrps)
	return;
 
    PEGASUS_LOG (PEGASUS_DBG_TX, "Tx Callback for  %d IRP.\n",
	    index, 0, 0, 0, 0, 0);
	
    END_TX_SEM_TAKE(&pDevCtrl->endObj,WAIT_FOREVER);

    pIrpBfr = pDevCtrl->pEnetIrp + index;
    pIrpBfr->outIrpInUse = FALSE;

    if (pDevCtrl->txStall) 
        {
        pDevCtrl->txStall = FALSE;
        /* Notify higher layers we can restart */
        netJobAdd ((FUNCPTR) pegasusMuxTxRestart, (int) &pDevCtrl->endObj, 0, 0, 0, 0);
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, 
                     "pegasusTxCallback: TX IRP available - Called MuxTxRestart() \n", 
                     0, 0, 0, 0, 0, 0); 
        }

    if (pIrp->result != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_TX, "Tx error %x.\n",
	    pIrp->result, 0, 0, 0, 0, 0);

	if (pIrp->result == S_usbHcdLib_STALLED)
	    {
	    if (usbdFeatureClear (pegasusHandle, pDevCtrl->pDev->nodeId,
		    USB_RT_STANDARD | USB_RT_ENDPOINT, 0, 1) == ERROR)
		{
		PEGASUS_LOG (PEGASUS_DBG_TX, "Could not clear STALL.\n",
		  pIrp->result, 0, 0, 0, 0, 0);
		}
	    }
  
	pDevCtrl->outErrors++;	/* Should also Update MIB */
	}
    else
	{
	PEGASUS_LOG (PEGASUS_DBG_TX, "Tx finished.\n",
		  0, 0, 0, 0, 0, 0);
	}
    nextIndex = (index+1) % pDevCtrl->noOfIrps;
	pIrpBfr = pDevCtrl->pEnetIrp + nextIndex;	
    if (pIrpBfr->outIrpInUse)
        {
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, "pegasusSend: Submitting queued IRP %d %x %d\n", 
            nextIndex, (int)(&pIrpBfr->outIrp), pIrpBfr->outIrp.transferLen, 0, 0, 0); 
        usbdTransfer(pegasusHandle, pDevCtrl->outPipeHandle, &pIrpBfr->outIrp);
        }
    else
        {
        PEGASUS_LOG (PEGASUS_DBG_TX_STALL, "pegasusSend: No More IRP's\n", 
            0, 0, 0, 0, 0, 0); 
        pDevCtrl->txActive = FALSE;
        }

    END_TX_SEM_GIVE(&pDevCtrl->endObj);
    }

/***************************************************************************
*
* pegasusRxCallback - Invoked when a Packet is received.
*
* This function is invoked wheb a packet is received
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID pegasusRxCallback
    (
    pVOID p			/* completed IRP */
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;

    PEGASUS_DEVICE * pDevCtrl = pIrp->userPtr;

    /* Input IRP completed */

     pDevCtrl->inIrpInUse = FALSE; 
     

    /*
     * If the IRP was successful then pass the data back to the client.
     * Note that the netJobAdd() is not necessary here as the function 
     * is not getting executed in isr context.
     */

    if (pIrp->result != OK)
	{
	pDevCtrl->inErrors++;	/* Should also update MIB */
  
	if(pIrp->result == S_usbHcdLib_STALLED)
	    {
	    if (usbdFeatureClear (pegasusHandle,pDevCtrl->pDev->nodeId,
		  USB_RT_STANDARD | USB_RT_ENDPOINT, 0, 0) != OK)
                return;
	    }
	}
    else
	{
	 if( pIrp->bfrList [0].actLen >= 2) 
	    {
	    pegasusEndRecv (pDevCtrl,pIrp->bfrList [0].pBfr, 
			  pIrp->bfrList [0].actLen); 
	    pDevCtrl->rxIndex++;
	    pDevCtrl->rxIndex %= pDevCtrl->noOfInBfrs;    
	    }
	}     

    /*
     * Unless the IRP was cancelled - implying the channel is being
     * torn down, re-initiate the "in" IRP to listen for more data from
     * the printer.
     */

    if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
	pegasusListenForInput (pDevCtrl);

    }

/***************************************************************************
*
* pegasusMchash - computing the Pegasus multicast hash table value.
*
* The Pegasus has a 64-bit multicast hash table filter. The filter is
* accessed via register 0x08 to 0x0F. The bits in the filter are set by 
* running the multicast address you want to receive through a little-endian 
* CRC-32 and then masking off all but the lower 6 bits. The resulting value 
* tells which bit in the 64 bit table to set.  
*
* RETURNS : The hash table value for the multicast address
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int pegasusMchash
    (
    UINT8 * addr
    )
    {
    UINT32       crc;
    int          idx, bit;
    UINT8        data;

    /* Compute CRC for the address value. */
    crc = 0xFFFFFFFF; /* initial value */

    /* the little-endian CRC-32 algorithm */  
    for (idx = 0; idx < 6; idx++)
        {
        for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
        crc = (crc >> 1) ^ (((crc ^ data) & 1) ? PEGASUS_POLY : 0);
        }

    /* masking off all but the lower 6 bits */
    return (crc & ((1 << PEGASUS_BITS) - 1));
    }

/***************************************************************************
*
* pegasusMCastFilterSet - sets a Multicast Address Filter for the device
*
* Even if the host wishes to change a single multicast filter in the device,
* it must reprogram the entire list of filters using this function. 
* <pAddress> shall contain a pointer to this list of multicast addresses
* and <noOfFilters> shall hold the number of multicast address filters
* being programmed to the device.
*
* RETURNS : OK or ERROR if <noOfFilters> is BIG or if the device NAKs
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusMCastFilterSet
    (
    PEGASUS_DEVICE * pDevCtrl	/* device to add the mcast filters */
    )
    {
    ETHER_MULTI *       pCurr;
    UINT8               table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    int                 hash, ix;

    PEGASUS_LOG (PEGASUS_DBG_MCAST, "pegasusMCastFilterSet:..entered.\n",
		 0, 0, 0, 0, 0, 0);

    if (pDevCtrl == NULL)
	return ERROR;

    pCurr = END_MULTI_LST_FIRST (&pDevCtrl->endObj);

    /* Go through the multicast list and compute the hash table value */

    while (pCurr != NULL)
    {
    hash = pegasusMchash((UINT8 *)&pCurr->addr);
    table[hash >> 3] = 1 << (hash & 0x7);
    pCurr = END_MULTI_LST_NEXT(pCurr);
    }

    /* Set the 64 bit multicast hash table filter 
     * The filter must be accessed by using 8 bit register 
     * read/write command
     */ 

    for (ix = 0; ix < 8; ix++)
        {
        if (usbdVendorSpecific (pegasusHandle,
                                pDevCtrl->pDev->nodeId,
                                USB_PEGASUS_SET_BMREQ,
                                USB_PEGASUS_SET_BREQ,
                                table[ix],
                                0x08 + ix,
                                0,
                                NULL,
                                NULL) != OK)
            {
            PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting Registers\n",
                         0, 0, 0, 0, 0, 0);
            return ERROR;
            }
        }

    return OK;

    }


/***************************************************************************
*
* pegasusFindDevice - searches for a usb enet device for indicated <nodeId>
*
* This function searches for a usb enet device for indicated <nodeId>
*
* RETURNS: pointer to matching dev struct or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL USB_PEGASUS_DEV * pegasusFindDevice
    (
    USBD_NODE_ID nodeId		/* Node Id to find */
    )

    {

    USB_PEGASUS_DEV * pDev = usbListFirst (&pegasusDevList);

    while (pDev != NULL)
	{
	if (pDev->nodeId == nodeId)
	    break;

	pDev = usbListNext (&pDev->devLink);
	}

    return pDev;
    }

/***************************************************************************
*
* pegasusEndFindDevice - searches for a usb enet device for a given <productId> and <vendorId>.
*
* This function searches for a usb enet device for a given <productId> and <vendorId>
*
* RETURNS: pointer to matching dev struct or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL USB_PEGASUS_DEV * pegasusEndFindDevice
    (
    UINT16 vendorId,		/* Vendor Id to search for */
    UINT16 productId		/* Product Id to search for */
    )

    {
    USB_PEGASUS_DEV * pDev = usbListFirst (&pegasusDevList);

    while (pDev != NULL)
	{
	if ((pDev->vendorId == vendorId) && (pDev->productId == productId))
	    break;

	pDev = usbListNext (&pDev->devLink);
	}

    return pDev;
    }


/***************************************************************************
*
* pegasusShutdown - shuts down USB EnetLib
*
* <errCode> should be OK or S_pegasusLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: depends on the error code <errCode>
*
* \NOMANUAL
*/

LOCAL STATUS pegasusShutdown
    (
    int errCode
    )

    {
   
    PEGASUS_DEVICE * pDev;

    /* Dispose of any open connections. */

    while ((pDev = usbListFirst (&pegasusDevList)) != NULL)
	pegasusDestroyDevice (pDev);
	
    /*	
     * Release our connection to the USBD.  The USBD automatically 
     * releases any outstanding dynamic attach requests when a client
     * unregisters.
     */

    if (pegasusHandle != NULL)
	{
	usbdClientUnregister (pegasusHandle);
	pegasusHandle = NULL;
	}

    /* Release resources. */

    if (pegasusMutex != NULL)
	{
	OSS_MUTEX_DESTROY (pegasusMutex);
	pegasusMutex = NULL;
	}
    
    if (pegasusTxMutex != NULL)
	{
	OSS_MUTEX_DESTROY (pegasusTxMutex);
	pegasusTxMutex = NULL;
	}
    
    if (pegasusRxMutex != NULL)
	{
	OSS_MUTEX_DESTROY (pegasusRxMutex);
	pegasusRxMutex = NULL;
	}

    return ossStatus (errCode);
    }

/***************************************************************************
*
* pegasusOutIrpInUse - determines if any of the output IRP's are in use
*
* This function determines if any of the output IRP's are in use and returns 
* the status information
* 
* RETURNS: TRUE if any of the IRP's are in use, FALSE otherwise.
*
* ERRNO: none
*/

BOOL pegasusOutIrpInUse
    (
    PEGASUS_DEVICE * pDevCtrl
    )
    {
    BOOL inUse = FALSE;
    int i;

    for (i=0;i<pDevCtrl->noOfIrps;i++) 
        {
        if (pDevCtrl->pEnetIrp[i].outIrpInUse) 
            {
            inUse = TRUE;
            break;
            }
        }

    return(inUse);
    }

/***************************************************************************
*
* pegasusDestroyDevice - disposes of a PEGASUS_DEVICE structure
*
* Unlinks the indicated PEGASUS_DEVICE structure and de-allocates
* resources associated with the channel.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL void pegasusDestroyDevice
    (
    PEGASUS_DEVICE * pDevCtrl
    )
    {
    USB_PEGASUS_DEV * pDev;
    int index;

    if (pDevCtrl != NULL)
	{

	pDev = pDevCtrl->pDev;

	/* Unlink the structure. */

	usbListUnlink (&pDev->devLink);
	

	/* Release pipes and wait for IRPs to be cancelled if necessary. */

	if (pDevCtrl->outPipeHandle != NULL)
	    usbdPipeDestroy (pegasusHandle, pDevCtrl->outPipeHandle);

	if (pDevCtrl->inPipeHandle != NULL)
	    usbdPipeDestroy (pegasusHandle, pDevCtrl->inPipeHandle);


	while (pegasusOutIrpInUse(pDevCtrl) || pDevCtrl->inIrpInUse)
	   OSS_THREAD_SLEEP (1);

	for (index=0; index < pDevCtrl->noOfInBfrs; index++)
            OSS_FREE (pDevCtrl->pInBfrArray[index]);

	if (pDevCtrl->pInBfrArray !=NULL)
	    OSS_FREE(pDevCtrl->pInBfrArray);

	if ( pDevCtrl->pEnetIrp != NULL)
	    OSS_FREE(pDevCtrl->pEnetIrp);

	/* Release structure. */

	/* This constitutes a memory leak, however leaving it in 
	 * causes failure.

	if (pDev !=NULL)
	    OSS_FREE (pDev);
	 */
	}
    }


/***************************************************************************
*
* usbPegasusEndLoad - initialize the driver and device
*
* This routine initializes the driver and the device to the operational state.
* All of the device specific parameters are passed in the initString.
* 这个例程将driver和device初始化到操作状态，
所有的设备指定的参数都通过initString来传递。

* This function first extracts the vendorId and productId of the device 
* from the initialization string using the pegasusEndParse() function. It then 
* passes these parametsrs and its control strcuture to the pegasusDevInit()
* function. pegasusDevInit() does most of the device specific initialization
* and brings the device to the operational state. Please refer to pegasusLib.c
* for more details about usbenetDevInit(). This driver will be attached to MUX
* and then the memory initialization of the device is carriedout using
* pegasusEndMemInit(). 
*
这个函数首先提取vendorId和productId (使用initialization string从pegasusEndParse()函数中提取)
然后将这些参数和控制结构传递给pegasusDevInit()函数
peagsusDevInit()函数完成大多数的设备指定的初始化将设备设置到可操作的状态。

关于usbenetDevInit()更多的细节请参考PegasusLib.c，这个驱动将会被attach到MUX上然后
设备的memory初始化被使用PegasusEndMemInit()函数来执行

* This function doesn't do any thing device specific. Instead, it delegates
* such initialization to pegasusDevInit(). This routine handles the other part
* of the driver initialization as required by MUX.

这个函数并不执行任何设备指定的初始化，设备指定参数的初始化在
PegasusDevInit()函数中执行。这个函数处理的是MUX要求的驱动初始化的其他部分。
*
* muxDevLoad calls this function twice. First time this function is called, 
* initialization string will be NULL . We are required to fill in the device 
* name ("usb") in the string and return. The next time this function is called
* the intilization string will be proper.

muxDevLoad()函数调用这个函数两次，第一次initString为NULL,我们
*
* <initString> will be in the following format :
* "unit:vendorId:productId:noOfInBfrs:noOfIrps"
*
* PARAMETERS
*
* \is
* \i <initString>
* The device initialization string.
* \ie
*
* RETURNS: An END object pointer or NULL on error.
*
* ERRNO: none
*/

END_OBJ * usbPegasusEndLoad
    (
    char * initString	                            /* initialization string */
    )
    {
    PEGASUS_DEVICE * pDrvCtrl;                      /* driver structure */

    UINT16 vendorId;                                /* vendor information */
    UINT16 productId;                               /* product information */

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Loading usb end...\n", 1, 2, 3, 4, 5, 6);

    if (initString == NULL)
	return (NULL);
    
    if (initString[0] == EOS)
	{

	/* Fill in the device name and return peacefully */

	bcopy ((char *)PEGASUS_NAME, (void *)initString, PEGASUS_NAME_LEN);
	return (0);
	}

    /* allocate the device structure */

    pDrvCtrl = (PEGASUS_DEVICE *) OSS_CALLOC (sizeof (PEGASUS_DEVICE));

    if (pDrvCtrl == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "No Memory!!...\n", 1, 2, 3, 4, 5, 6);
	goto errorExit;
	}

    /* parse the init string, filling in the device structure */

    if (pegasusEndParse (pDrvCtrl, initString, &vendorId, &productId) == ERROR)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse Failed.\n", 1, 2, 3, 4, 5, 6);
	goto errorExit;
	}

    /* Ask the pegasusLib to do the necessary initilization. */

    if (pegasusDevInit(pDrvCtrl,vendorId,productId) == ERROR)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "EnetDevInitFailed.\n", 
		    1, 2, 3, 4, 5, 6);
	goto errorExit;
	}

    /* initialize the END and MIB2 parts of the structure */

    if (END_OBJ_INIT (&pDrvCtrl->endObj, 
		      (DEV_OBJ *)pDrvCtrl, 
		      PEGASUS_NAME, 
		      pDrvCtrl->unit, 
		      &pegasusEndFuncTable,
		      PEGASUS_DESCRIPTION) 
		  == ERROR || 
	 END_MIB_INIT (&pDrvCtrl->endObj, 
		       M2_ifType_ethernet_csmacd, 
		       &pDrvCtrl->macAdrs[0], 
		       6, 
		       ETHERMTU, 
		       PEGASUS_SPEED) 
		  == ERROR)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "END MACROS FAILED...\n", 
		1, 2, 3, 4, 5, 6);
	goto errorExit;
	}

    /* Perform memory allocation/distribution */

    if (pegasusEndMemInit (pDrvCtrl) == ERROR)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "endMemInit() Failed...\n", 
		    1, 2, 3, 4, 5, 6);	
	goto errorExit;
	}
 
    /* set the flags to indicate readiness */

    END_OBJ_READY (&pDrvCtrl->endObj,
		    IFF_UP | IFF_RUNNING | IFF_NOTRAILERS | IFF_BROADCAST
		    | IFF_MULTICAST);

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Done loading usb end..\n", 
	1, 2, 3, 4, 5, 6);
	
    return (&pDrvCtrl->endObj);

errorExit:

    if (pDrvCtrl != NULL)
        {
                  
      	/* Unlink the structure. */

	usbListUnlink (&pDrvCtrl->pDev->devLink);
       
        OSS_FREE ((char *)pDrvCtrl);

        } 

    return NULL;
    }


/***************************************************************************
*
* pegasusEndParse - parse the init string
*
* Parse the input string.  Fill in values in the driver control structure.
*
* The muxLib.o module automatically prepends the unit number to the user's
* initialization string from the BSP (configNet.h).
* 
* This function parses the input string and fills in the places pointed
* to by <pVendorId> and <pProductId>. Unit Number of the string will be
* be stored in the device structure pointed to by <pDrvCtrl>.
* \is
* \i <pDrvCtrl>
* Pointer to the device structure.
* \i <initString>
* Initialization string for the device. It will be of the following format :
* "unit:vendorId:productId"
* Device unit number, a small integer.
* \i <pVendorId>
* Pointer to the place holder of the device vendor id.
* \i <pProductId>
*  Pointer to the place holder of the device product id.
* \ie
*
* RETURNS: OK or ERROR for invalid arguments.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndParse
    (
    PEGASUS_DEVICE * pDrvCtrl,	/* device pointer */
    char * initString,		/* information string */
    UINT16 * pVendorId,		
    UINT16 * pProductId
    )
    {

    char *	tok;
    char *	pHolder = NULL;
    
    /* Parse the initString */

    /* Unit number. (from muxLib.o) */

    tok = strtok_r (initString, ":", &pHolder);

    if (tok == NULL)
	return ERROR

    pDrvCtrl->unit = atoi (tok);

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: Unit : %d..\n", 
		pDrvCtrl->unit, 2, 3, 4, 5, 6); 

    /* Vendor Id. */

    tok = strtok_r (NULL, ":", &pHolder);

    if (tok == NULL)
	return ERROR;

    *pVendorId = atoi (tok);

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: VendorId : 0x%x..\n", 
		*pVendorId, 2, 3, 4, 5, 6); 

    /* Product Id. */

    tok = strtok_r (NULL, ":", &pHolder);

    if (tok == NULL)
	return ERROR;

    *pProductId = atoi (tok);
  
    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: ProductId : 0x%x..\n", 
		*pProductId, 2, 3, 4, 5, 6); 

    /* we have paresed the inbfrs and outirps here 
     * these vars are to be passed to pegasusEndParse from usbPegasusEndLoad. 
     */

    /* no of in buffers */

    tok = strtok_r (NULL, ":", &pHolder);

    if (tok == NULL)
	return ERROR;


    pDrvCtrl->noOfInBfrs  = atoi (tok);
  
    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: NoInBfrs : %d..\n", 
		pDrvCtrl->noOfInBfrs, 2, 3, 4, 5, 6); 
	
    /* no of out IRPs */

    tok = strtok_r (NULL, ":", &pHolder);

    if (tok == NULL)
	return ERROR;

    pDrvCtrl->noOfIrps = atoi (tok);
  
    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: NoOutIrps : %d..\n", 
		 pDrvCtrl->noOfIrps, 2, 3, 4, 5, 6);

    /*here ends the extra two parsing blocks */


    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Parse: Processed all arugments\n", 
		1, 2, 3, 4, 5, 6);

    return OK;
    }


/***************************************************************************
*
* pegasusEndSend - the driver send routine
*
* This routine takes a M_BLK_ID sends off the data in the M_BLK_ID. We copy
* the data contained in the MBlks to a character buffer and hand over the 
* buffer to pegasusSend().The buffer must already have the addressing 
* information properly installed in it.  This is done by a higher layer.   
* The device requires that the first two bytes of the data sent to it (for 
* transmission over ethernet) contain the length of the data. So we add this
* here. 
*
* During the course of our testing the driver, we found that if we send the 
* exact length of the data as handed over by MUX, the device is corrupting
* some bytes of the packet. This is resulting in packet not being received
* by the addresses destination, packet checksum errors etc. The remedy is to
* padup few bytes to the data packet. This, we found, solved the problem.
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndSend
    (
    PEGASUS_DEVICE * pDrvCtrl,	/* device ptr */
    M_BLK_ID     pMblk		/* data to send */
    )
    {

    UINT8 *      pBuf; 		/* buffer to hold the data */
    UINT32	noOfBytes;      /* noOfBytes to be transmitted */
    STATUS  status;

    
    PEGASUS_LOG (PEGASUS_DBG_TX, "pegasusEndSend: Entered.\n",
		0, 0, 0, 0, 0, 0);  


    if ((pDrvCtrl == NULL) || (pMblk == NULL))
	return ERROR;

    /* Point to the one of the pre-allocated output buffers */
    pBuf = pDrvCtrl->pOutBfrArray[pDrvCtrl->txIrpIndex];

    if (pBuf == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_TX," pegasusEndSend : "
		    "Could not allocate memory \n", 0, 0, 0, 0, 0, 0);
	return ERROR;
	}

    /* copy the MBlk chain to a buffer */

    noOfBytes = netMblkToBufCopy(pMblk,(char *)pBuf+2,NULL); 

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "pegasusEndSend: %d bytes to be sent.\n", 
		noOfBytes, 0, 0, 0, 0, 0);

    if (noOfBytes == 0) 
	return ERROR;

    /* 
     * Padding : how much to pad is decided by trial and error.
     * Note that we need not add any extra bytes in the buffer.
     * since we are not using those bytes, they can be any junk 
     * whick is already in the buffer.
     * We are just interested in the count.
     */
    
    if (noOfBytes < 60)
	noOfBytes = 60;

    /* (Required by the device) Fill in the Length in the first Two Bytes */

    *(UINT16 *)pBuf = TO_LITTLEW(noOfBytes);

    /* Transmit the data */

    if ((status = pegasusSend (pDrvCtrl, pBuf, noOfBytes+2)) != OK)
        {
	netMblkClChainFree(pMblk);
	return ERROR;
        }

    PEGASUS_LOG (PEGASUS_DBG_TX, "pegasusEndSend: Pkt submitted for tx.\n", 
		0, 0, 0, 0, 0, 0);
 
    /* Bump the statistic counter. */

    END_ERR_ADD (&pDrvCtrl->endObj, MIB2_OUT_UCAST, +1);

    /*
     * Cleanup.  The driver frees the packet now.
     */

    netMblkClChainFree (pMblk);

    return (OK);
    }


/***************************************************************************
*
* usbPegasusFreeRtn - pegasus Cluster Free Routine
*
* The pegasus driver does not use the netBufLib to free the cluster blocks.
* We do nothing when netBufLib is done with them because we allocate the 
* our buffers as IRP buffers.  The USB stack manages these buffers instead
* of netBufLib.
*
* RETURNS: N/A.
*
* ERRNO: none
*
* \NOMANUAL
*/

void usbPegasusFreeRtn (void)
    {

    ;	 /* We do nothing here.  */

    }

/***************************************************************************
*
* pegasusEndRecv - process the next incoming packet
*
* pegasusRecv is called by the pegasusIRPCallBack() upon successful execution
* of an input IRP. This means we got some proper data. This function will be 
* called with the pointer to be buffer and the length of data.
* What we do here is to construct an MBlk strcuture with the data received 
* and pass it onto the upper layer.
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndRecv
    (
    PEGASUS_DEVICE * pDrvCtrl,	/* device structure */
    UINT8 *  pData,              /* pointer to data buffer */
    UINT32  len                 /* length of data */
    )
    {
    char *      pNewCluster;    /* Clusuter to store the data */
    CL_BLK_ID	pClBlk;         /* Control block to "control" the cluster */
    M_BLK_ID 	pMblk;          /* and an MBlk to complete a MBlk contruct */

    PEGASUS_LOG (PEGASUS_DBG_RX, "pegasusEndRecv called...Entered len=%d \n",
		len, 0, 0, 0, 0, 0);

    /* Add one to our unicast data. */

    END_ERR_ADD (&pDrvCtrl->endObj, MIB2_IN_UCAST, +1);

    pNewCluster = (char *)pData; 

    /* Grab a cluster block to marry to the cluster we received. */

    if ((pClBlk = netClBlkGet (pDrvCtrl->endObj.pNetPool, M_DONTWAIT)) == NULL)
	{
	netClFree (pDrvCtrl->endObj.pNetPool, (UCHAR *)pData);
	PEGASUS_LOG (PEGASUS_DBG_RX, "Out of Cluster Blocks!\n", 
		    1, 2, 3, 4, 5, 6);

	END_ERR_ADD (&pDrvCtrl->endObj, MIB2_IN_ERRS, +1);

	PEGASUS_LOG (PEGASUS_DBG_RX, "netClBlkGet...Error\n",
		    0, 0, 0, 0, 0, 0);    

	goto cleanRXD;
	}

    /*
     * Let's get an M_BLK_ID and marry it to the one in the ring.
     */

    if ((pMblk = mBlkGet (pDrvCtrl->endObj.pNetPool, M_DONTWAIT, MT_DATA)) 
	== NULL)
	{
	netClBlkFree (pDrvCtrl->endObj.pNetPool, pClBlk); 
	netClFree (pDrvCtrl->endObj.pNetPool, (UCHAR *)pData);
	PEGASUS_LOG (PEGASUS_DBG_RX, "Out of M Blocks!\n", 
		    1, 2, 3, 4, 5, 6);
	END_ERR_ADD (&pDrvCtrl->endObj, MIB2_IN_ERRS, +1);
	PEGASUS_LOG (PEGASUS_DBG_RX, "mBlkGet...Error\n",
		    0, 0, 0, 0, 0, 0);    

	goto cleanRXD;
	}

    END_ERR_ADD (&pDrvCtrl->endObj, MIB2_IN_UCAST, +1);
    
    /* Join the cluster to the MBlock */

    if(netClBlkJoin (pClBlk, 
		     (char *)pData, 
		     len, 
		     (FUNCPTR) usbPegasusFreeRtn, 
		     0, 
		     0, 
		     0) 
		  == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_MCAST, "netClBlkJoin Failed...Error\n",
		    0, 0, 0, 0, 0, 0);    
	}


    if(netMblkClJoin (pMblk, pClBlk) == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_RX, "mMblkClJoin...Error\n",
		    0, 0, 0, 0, 0, 0);    
	}


    bcopy ((char *)pData, (char *)(pData+2), (int)(len & ~0xc000));

    pMblk->mBlkHdr.mData += 2;

    pMblk->mBlkHdr.mLen = len;
    pMblk->mBlkHdr.mFlags |= M_PKTHDR;
    pMblk->mBlkPktHdr.len = len;

    /* Call the upper layer's receive routine. */

    END_RCV_RTN_CALL(&pDrvCtrl->endObj, pMblk); 


cleanRXD:

    return OK;
    }

/***************************************************************************
*
* pegasusEndMemInit - initialize memory for the device
*
* Setup the END's Network memory pool.  The memory allocation is done once
* when the device is first inserted.  Cluster block and M-Block memory areas
* are created and sorted in global structures: pegasusMclBlkConfig[] and 
* pegasusClDescTbl[]. The netPool is created and stored in the global pointer
* pUsbPegasusNetPool.
* 
* All subsequent insertions reuse these memory areas.  
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndMemInit
    (
    PEGASUS_DEVICE * pDrvCtrl	/* device to be initialized */
    )
    {

    if (usbPegasusEndMemoryIsNotSetup)
	{
	pegasusMclBlkConfig.mBlkNum = PEGASUS_M_BLK_NUM;
	pegasusClDescTbl[0].clNum = PEGASUS_CL_NUM;
	pegasusMclBlkConfig.clBlkNum = pegasusClDescTbl[0].clNum;

	/* Calulate the memory size for the mBlks and cBlks.	 */

	pegasusMclBlkConfig.memSize = 
	    (pegasusMclBlkConfig.mBlkNum * (MSIZE + sizeof (long))) +
	    (pegasusMclBlkConfig.clBlkNum * (CL_BLK_SZ + sizeof (long)));

	/* Allocate the memory */

	pegasusMclBlkConfig.memArea = \
		    OSS_MALLOC (pegasusMclBlkConfig.memSize);

	if (pegasusMclBlkConfig.memArea == NULL)
	    {

	    PEGASUS_LOG (PEGASUS_DBG_LOAD,"pegasusEndMemInit:system memory "
		    "unavailable for mBlk clBlk area\n", 1, 2, 3, 4, 5, 6);
	    return (ERROR);

	    }

	/* Calulate the memory size for the clusters. */

	pegasusClDescTbl[0].memSize = 
	    (pegasusClDescTbl[0].clNum * (PEGASUS_BUFSIZ+ 8 + sizeof(int)));

	/* Allocate the memory */

	pegasusClDescTbl[0].memArea = OSS_MALLOC (pegasusClDescTbl[0].memSize);

	if (pegasusClDescTbl[0].memArea == NULL)
	    {

	    /* Fatal, free the previously alloced area and return */
	    
	    PEGASUS_LOG (PEGASUS_DBG_LOAD,"pegasusEndMemInit:system memory "
		    "unavailable for cluster area\n", 1, 2, 3, 4, 5, 6);

	    OSS_FREE (pegasusMclBlkConfig.memArea);

	    return (ERROR);

	    }

	/* 
	 * If we've made it here both memory area's have been succesfully
	 * alloc'd, now initialize the memory pool
	 */

	/* Allocate an END netPool */
    
	pUsbPegasusNetPool = OSS_MALLOC (sizeof (NET_POOL));

	if (pUsbPegasusNetPool == NULL)
	    {

	    /* Fatal, free the previously alloced areas and return */

	    PEGASUS_LOG (PEGASUS_DBG_LOAD,"pegasusEndMemInit:system memory "
		    "unavailable for netPool\n", 1, 2, 3, 4, 5, 6);

	    OSS_FREE (pegasusMclBlkConfig.memArea);

	    OSS_FREE (pegasusClDescTbl[0].memArea);

	    return (ERROR);

	    }

	/* Now initialize the pool */

	if (netPoolInit (pUsbPegasusNetPool, 
			 &pegasusMclBlkConfig, 
			 &pegasusClDescTbl[0], 
			 pegasusClDescTblNumEnt, 
			 NULL) 
		      == ERROR)
	    {
	    /* Fatal, free the previously alloced areas and return */

	    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Could not init buffering\n",
		    1, 2, 3, 4, 5, 6);

	    OSS_FREE (pegasusMclBlkConfig.memArea);

	    OSS_FREE (pegasusClDescTbl[0].memArea);

	    return (ERROR);
	    }
    
	/* 
	 * Mark the memory setup flag as false so we don't try to 
	 * setup memory again.
	 */

	usbPegasusEndMemoryIsNotSetup = FALSE;

	}  /* end of initial memory setup */

    pDrvCtrl->endObj.pNetPool = pUsbPegasusNetPool;

    if ((pDrvCtrl->pClPoolId = netClPoolIdGet (pDrvCtrl->endObj.pNetPool, 
					       PEGASUS_BUFSIZ, 
					       FALSE)) 
					    == NULL)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "netClPoolIdGet() not successfull \n",
		    1, 2, 3, 4, 5, 6);
	return (ERROR);
    
	}

    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Memory setup complete\n", 
		1, 2, 3, 4, 5, 6);

    return OK;
    }


/***************************************************************************
*
* pegasusEndConfig - reconfigure the interface under us
*
* Reconfigure the interface setting promiscuous/ broadcast etc modes, and 
* changing the multicast interface list.
*
* RETURNS: OK or ERROR
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndConfig
    (
    PEGASUS_DEVICE * pDrvCtrl	/* device to be re-configured */
    )
    {
    UINT8 ctl0=0, ctl2=0;

    /* Reading Pegasus Ethernet control_0 register at offset 0x0. */

    if (usbdVendorSpecific (pegasusHandle, pDrvCtrl->pDev->nodeId,
                           USB_PEGASUS_GET_BMREQ,
                           USB_PEGASUS_GET_BREQ,
                           ctl0, 0x0, 0x0, NULL, NULL) != OK)
        {
        PEGASUS_LOG (PEGASUS_DBG_INIT, "Error Reading ctl0 register\n",
                    0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    /* Reading Pegasus Ethernet control_2 register at offset 0x2. */

    if (usbdVendorSpecific (pegasusHandle, pDrvCtrl->pDev->nodeId,
                           USB_PEGASUS_GET_BMREQ,
                           USB_PEGASUS_GET_BREQ,
                           ctl2, 0x2, 0x0, NULL, NULL) != OK)
        {
        PEGASUS_LOG (PEGASUS_DBG_INIT, "Error Reading ctl2 register\n",
                    0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    /* Set the modes asked for. */

    if (END_FLAGS_GET(&pDrvCtrl->endObj) & IFF_PROMISC)
	{
	PEGASUS_LOG (PEGASUS_DBG_IOCTL, "Setting Promiscuous mode on!\n",
		1, 2, 3, 4, 5, 6);
	ctl2 |= PEGASUS_CTL2_RX_PROMISC;
	}
    else
	ctl2 &= ~PEGASUS_CTL2_RX_PROMISC;

    if (END_FLAGS_GET(&pDrvCtrl->endObj) & IFF_ALLMULTI)
	{
	PEGASUS_LOG (PEGASUS_DBG_IOCTL, "Setting ALLMULTI mode On!\n",
		1, 2, 3, 4, 5, 6);
        ctl0 |= PEGASUS_CTL0_ALLMULTI;
	}
    else
        ctl0 &= ~PEGASUS_CTL0_ALLMULTI;

    /* Setting Pegasus Ethernet control_0 register at offset 0x0. */

    if (usbdVendorSpecific (pegasusHandle, pDrvCtrl->pDev->nodeId,
                           USB_PEGASUS_SET_BMREQ,
                           USB_PEGASUS_SET_BREQ,
                           ctl0, 0x0, 0x0, NULL, NULL) != OK)
        {
        PEGASUS_LOG (PEGASUS_DBG_INIT, "Error Setting ctl0 register\n",
                    0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    /* Setting Pegasus Ethernet control_2 register at offset 0x2. */

    if (usbdVendorSpecific (pegasusHandle, pDrvCtrl->pDev->nodeId,
                           USB_PEGASUS_SET_BMREQ,
                           USB_PEGASUS_SET_BREQ,
                           ctl2, 0x2, 0x0, NULL, NULL) != OK)
        {
        PEGASUS_LOG (PEGASUS_DBG_INIT, "Error Setting ctl2 register\n",
                    0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    /* Set up address filter for multicasting. */
	
    if (END_MULTI_LST_CNT(&pDrvCtrl->endObj) > 0)
        {	
	/* Set the Filter!!! */

	if (pegasusMCastFilterSet(pDrvCtrl) == ERROR)

	    return ERROR; 
	}

    return OK;
    }


/***************************************************************************
*
* pegasusEndMCastAdd - add a multicast address for the device
*
* This routine adds a multicast address to whatever the chip is already 
* listening for. The usb Ethernet device specifically requires that even 
* if we want a small modification (addtion or removal) of the filter list,
* we download the entire list to the device. We use the generic etherMultiLib
* functions and then call the pegasusMCastFilterSet() to achieve the
* functionality.
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndMCastAdd
    (
    PEGASUS_DEVICE * pDrvCtrl,		/* device pointer */
    char * pAddress			/* new address to add */
    )
    {
    
    if ((pDrvCtrl == NULL) || (pAddress == NULL))
	return ERROR;

    /* First, add this address to the local list */
    
    if (etherMultiAdd (&pDrvCtrl->endObj.multiList,
				pAddress) 
			   == ERROR)
	return ERROR;
  
    /* Set the Filter!!! */

    if (pegasusMCastFilterSet (pDrvCtrl) == ERROR)
	return ERROR;

    return (OK);
    }

/***************************************************************************
*
* pegasusEndMCastDel - delete a multicast address for the device
*
* This routine removes a multicast address from whatever the driver 
* is listening for. The usb Ethernet device specifically requires that even 
* if we want a small modification (addtion or removal) of the filter list,
* we download the entire list to the device. We use the generic etherMultiLib
* functions and then call the pegasusMCastFilterSet() to achieve the
* functionality.
*
* RETURNS: OK or ERROR
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndMCastDel
    (
    PEGASUS_DEVICE * pDrvCtrl,		/* device pointer */
    char * pAddress	/* new address to add */
    )
    {
  
    if ((pDrvCtrl == NULL) || (pAddress == NULL))
	return ERROR;

    /* First, del this address from the local list */
    
    if (etherMultiDel (&pDrvCtrl->endObj.multiList, pAddress) == ERROR)
	return ERROR;
     
    if (pegasusMCastFilterSet (pDrvCtrl) == ERROR)
        return ERROR;
        
    return (OK);
    }


/***************************************************************************
*
* pegasusEndMCastGet - get the multicast address list for the device
*
* This routine gets the multicast list of whatever the driver
* is already listening for.
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*/

LOCAL STATUS pegasusEndMCastGet
    (
    PEGASUS_DEVICE * pDrvCtrl,		/* device pointer */
    MULTI_TABLE * pTable		/* address table to be filled in */
    )
    {
    return (etherMultiGet (&pDrvCtrl->endObj.multiList, pTable));
    }


/***************************************************************************
*
* pegasusEndIoctl - the driver I/O control routine
*
* This routine processes an ioctl request.
*
* RETURNS: A command specific response, usually OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int pegasusEndIoctl
    (
    PEGASUS_DEVICE * pDrvCtrl,	/* device receiving command */
    int cmd,			/* ioctl command code */
    caddr_t data		/* command argument */
    )
    {

    int error = 0;
    long value;

    switch ((UINT)cmd)
	{
	case EIOCSADDR	:			/* Set Device Address */

	    if (data == NULL)
		return (EINVAL);

	    bcopy ((char *)data, 
		   (char *) END_HADDR (&pDrvCtrl->endObj),
		   END_HADDR_LEN (&pDrvCtrl->endObj));

	    break;

	case EIOCGADDR	:			/* Get Device Address */

	    if (data == NULL)
		return (EINVAL);

	    bcopy ((char *)END_HADDR (&pDrvCtrl->endObj), (char *)data,
		    END_HADDR_LEN (&pDrvCtrl->endObj));

	    break;


	case EIOCSFLAGS	:			/* Set Device Flags */
	  
	    value = (long)data;
	    if (value < 0)
		{
		value = -value;
		value--;
		END_FLAGS_CLR (&pDrvCtrl->endObj, value);
		}
	    else
		{
		END_FLAGS_SET (&pDrvCtrl->endObj, value);
		}

	    pegasusEndConfig (pDrvCtrl);
	    break;


        case EIOCGFLAGS:			/* Get Device Flags */

	    *(int *)data = END_FLAGS_GET(&pDrvCtrl->endObj);
            break;


	case EIOCPOLLSTART :		/* Begin polled operation */

	    return EINVAL;		/* Not supported */


	case EIOCPOLLSTOP :		/* End polled operation */

	    return EINVAL;		/* Not supported */


        case EIOCGMIB2	:		/* return MIB information */

            if (data == NULL)
                return (EINVAL);

            bcopy ((char *)&pDrvCtrl->endObj.mib2Tbl, 
	 	   (char *)data, 
	 	   sizeof(pDrvCtrl->endObj.mib2Tbl));

            break;


        case EIOCGFBUF	:	/* return minimum First Buffer for chaining */

            if (data == NULL)
                return (EINVAL);

            *(int *)data = PEGASUS_MIN_FBUF;

            break;


	case EIOCMULTIADD : 	/* Add a Multicast Address */

	    if (data == NULL)
		return (EINVAL);

	    if (pegasusEndMCastAdd (pDrvCtrl, (char *)data) == ERROR)
                return ERROR;

	    break;


	case EIOCMULTIDEL : 	/* Delete a Multicast Address */

	    if (data == NULL)
		return (EINVAL);

	    if (pegasusEndMCastDel (pDrvCtrl, (char *)data) == ERROR)
		return ERROR;

	    break;


	case EIOCMULTIGET : 	/* Get the Multicast List */

	    if (data == NULL)
		return (EINVAL);

	    if (pegasusEndMCastGet (pDrvCtrl, (MULTI_TABLE *)data) == ERROR)
		return ERROR;

	    break;

       default:
            error = EINVAL;
	
        }

    return (error);
    }


/***************************************************************************
*
* usbPegasusEndUnload - unload a driver from the system
*
* This function first brings down the device, and then frees any
* stuff that was allocated by the driver in the load function.
*
* RETURNS: OK or ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPegasusEndUnload
    (
    PEGASUS_DEVICE * pDrvCtrl	/* device to be unloaded */
    )
    {

    PEGASUS_LOG (PEGASUS_DBG_STOP, "UnloadEnd...\n ", 1, 2, 3, 4, 5, 6);


    END_OBJECT_UNLOAD (&pDrvCtrl->endObj);
  
    if ((pDrvCtrl->pDev->lockCount == 0) && (!pDrvCtrl->pDev->connected))
       {	
	
        PEGASUS_LOG (PEGASUS_DBG_STOP, "Lock count is 0 and No device"
		     "connected\n", 1, 2, 3, 4, 5, 6);

	}
    else
	PEGASUS_LOG (PEGASUS_DBG_STOP, "Device lockCount = %x "
                    "connected =%x\n",pDrvCtrl->pDev->lockCount,
                    pDrvCtrl->pDev->connected,3,4,5,6); 	

    PEGASUS_LOG (PEGASUS_DBG_STOP,"Unloading End...Done!!!\n",1,2,3,4,5,6);

    return (OK);
    }


/***************************************************************************
*
* pegasusEndPollRcv - routine to receive a packet in polled mode
*
* This routine is NOT supported
*
* RETURNS: ERROR Always
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndPollRcv
    (
    PEGASUS_DEVICE * pDrvCtrl,	/* device to be polled */
    M_BLK_ID      pMblk		/* ptr to buffer */
    )
    {
    
    PEGASUS_LOG (PEGASUS_DBG_POLL_RX, "Poll Recv: NOT SUPPORTED", 
	1, 2, 3, 4, 5, 6);

    return ERROR;

    }

/***************************************************************************
*
* pegasusEndPollSend - routine to send a packet in polled mode
*
* This routine is NOT SUPPORTED
*
* RETURNS: ERROR always
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS pegasusEndPollSend
    (
    PEGASUS_DEVICE * 	pDrvCtrl,	/* device to be polled */
    M_BLK_ID    pMblk			/* packet to send */
    )
    {
    
    PEGASUS_LOG (PEGASUS_DBG_POLL_TX, "Poll Send : NOT SUPPORTED", 
	1, 2, 3, 4, 5, 6);

    return ERROR;
    }


/***************************************************************************
*
* notifyAttach - notifies registered callers of attachment/removal
*
* This fucntion notifies the registered clients about the device attachment 
* and removal
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    USB_PEGASUS_DEV * pDev,
    UINT16 attachCode
    )

    {
    pATTACH_REQUEST pRequest = usbListFirst (&reqList);
    
    PEGASUS_LOG (PEGASUS_DBG_ATTACH, "In notify attach, \n",
		    		0, 0, 0, 0, 0, 0);    	

    while (pRequest != NULL)
    	{
    	(*pRequest->callback) (pRequest->callbackArg, 
	                       pDev, 
			       attachCode);

	PEGASUS_LOG (PEGASUS_DBG_ATTACH, "Callback executed for node Id %x \n",
		    		(ULONG)pDev->nodeId, 0, 0, 0, 0, 0);    	

    	pRequest = usbListNext (&pRequest->reqLink);
        
    
    	}

    }

/***************************************************************************
*
* usbPegasusDynamicAttachRegister - register PEGASUS device attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_PEGASUS_ATTACH_CALLBACK) 
*     (
*     pVOID arg,
*     USB_PEGASUS_DEV * pDev,
*     UINT16 attachCode
*     );
* \ce
*
* usbPegasusDevLib will invoke <callback> each time a PEGASUS device
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also  pass the structure of the device 
* being created/destroyed and an attach code of USB_PEGASUS_ATTACH or 
* USB_PEGASUS_REMOVE.
*
* NOTE
* The user callback routine should not invoke any driver function that
* submits IRPs.  Further processing must be done from a different task context.
* As the driver routines wait for IRP completion, they cannot be invoked from
* USBD client task's context created for this driver.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbPegasusLib_BAD_PARAM
* Bad Parameter received
*
* \i S_usbPegasusLib_OUT_OF_MEMORY
* Sufficient memory no available
* \ie
*/

STATUS usbPegasusDynamicAttachRegister
    (
    USB_PEGASUS_ATTACH_CALLBACK callback, /* new callback to be registered */
    pVOID arg                           /* user-defined arg to callback  */
    )

    {
    pATTACH_REQUEST   pRequest;
    USB_PEGASUS_DEV  *	      pPegasusDev;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
        return (ossStatus (S_usbPegasusLib_BAD_PARAM));

    OSS_MUTEX_TAKE (pegasusMutex, OSS_BLOCK);

    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
        {
        status = ossStatus (S_usbPegasusLib_OUT_OF_MEMORY);
        }
    else
        {
        pRequest->callback    = callback;
        pRequest->callbackArg = arg;

        usbListLink (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL);
    
       /* 
        * Perform an initial notification of all currrently attached
        * PEGASUS devices.
        */

        pPegasusDev = usbListFirst (&pegasusDevList);

        while (pPegasusDev != NULL)
	    {
	    if (pPegasusDev->connected)
                (*callback) (arg, pPegasusDev, USB_PEGASUS_ATTACH);
                
	    PEGASUS_LOG (PEGASUS_DBG_ATTACH, "Callback executed for device"
			 " nodeid %0x, \n", (ULONG)pPegasusDev->nodeId, 
			 0, 0, 0, 0, 0);    	
		    
	    pPegasusDev = usbListNext (&pPegasusDev->devLink);
	    
	    }

        }

    OSS_MUTEX_RELEASE (pegasusMutex);

    return (ossStatus (status));
    }


/***************************************************************************
*
* usbPegasusDynamicAttachUnregister - unregisters PEGASUS attach callbackx
*
* This function cancels a previous request to be dynamically notified for
* attachment and removal.  The <callback> and <arg> paramters 
* must exactly match those passed in a previous call to 
* usbPegasusDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister the callback.
*
* ERRNO:
* \is
* \i S_usbPegasusLib_NOT_REGISTERED
* Could not regsiter the attachment callback
* \ie
*/

STATUS usbPegasusDynamicAttachUnregister
    (
    USB_PEGASUS_ATTACH_CALLBACK callback, /* callback to be unregistered  */
    pVOID arg                          /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    int status = S_usbPegasusLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (pegasusMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
        {
        if ((callback == pRequest->callback) && (arg == pRequest->callbackArg))
	    {
	    /* Found a matching notification request. */

	    usbListUnlink (&pRequest->reqLink);

            /* Dispose of structure */

            OSS_FREE (pRequest);
	    status = OK;

	    break;
	    }
        pRequest = usbListNext (&pRequest->reqLink);
	}

    OSS_MUTEX_RELEASE (pegasusMutex);

    return (ossStatus (status));
    }


/***************************************************************************
*
* usbPegasusDevLock - marks USB_PEGASUS_DEV structure as in use
*
* A caller uses usbPegasusDevLock() to notify usbPegasusDevLib that
* it is using the indicated PEGASUS device structure.  usbPegasusDevLib maintains
* a count of callers using a particular Pegasus Device structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* Pegasus Device is removed from the system.  So long as the "lock count"
* is greater than zero, usbPegasusDevLib will not dispose of an Pegasus
* structure.
*
* RETURNS: OK, or ERROR if unable to mark Pegasus structure in use.
*
* ERRNO: none
*/

STATUS usbPegasusDevLock
    (
    USBD_NODE_ID nodeId    /* NodeId of the USB_PEGASUS_DEV */
			   /* to be marked as in use        */
    )

    {
    USB_PEGASUS_DEV * pPegasusDev = pegasusFindDevice (nodeId);

    if ( pPegasusDev == NULL)
        return (ERROR);

    pPegasusDev->lockCount++;

    return (OK);
    }


/***************************************************************************
*
* usbPegasusDevUnlock - marks USB_PEGASUS_DEV structure as unused
*
* This function releases a lock placed on an Pegasus Device structure.  When a
* caller no longer needs an Pegasus Device structure for which it has previously
* called usbPegasusDevLock(), then it should call this function to
* release the lock.
*
* NOTE
* If the underlying Pegasus device has already been removed
* from the system, then this function will automatically dispose of the
* Pegasus Device structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the Pegasus Device structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark Pegasus Device structure unused.
*
* ERRNO:
* \is
* \i S_usbPegasusLib_NOT_LOCKED
* No lock to Unlock
* \ie
*/

STATUS usbPegasusDevUnlock
    (
    USBD_NODE_ID nodeId    /* NodeId of the BLK_DEV to be marked as unused */
    )

    {
    int status = OK;
    USB_PEGASUS_DEV * pPegasusDev = pegasusFindDevice (nodeId);


    if ( pPegasusDev == NULL)
  	{
        return (ERROR);
	}

    OSS_MUTEX_TAKE (pegasusMutex, OSS_BLOCK);

    if (pPegasusDev->lockCount == 0)
        {
        PEGASUS_LOG (PEGASUS_DBG_STOP,"usbPegasusDevUnlock..Not Locked!\n",
                    1,2,3,4,5,6);
        status = S_usbPegasusLib_NOT_LOCKED;
        }
    else
    {

    /* If this is the last lock and the underlying PEGASUS device is
     * no longer connected, then dispose of the device.
     */

    if ((--pPegasusDev->lockCount == 0) && (!pPegasusDev->connected))
        {
	pegasusDestroyDevice ((PEGASUS_DEVICE *)pPegasusDev->pPegasusDev);
      	PEGASUS_LOG (PEGASUS_DBG_STOP,"Pegasus Device Destroyed inDevUnlock\n",
                    1,2,3,4,5,6);

        }
	
    }

    OSS_MUTEX_RELEASE (pegasusMutex);

    return (ossStatus (status));
    }



/***************************************************************************
*
* usbPegasusInit - chip Initialization for the Pegasus Chip
*
* This function initializes the device specific registers for the PEGASUS
* chip. This function will reset the MAC and loads the Ethernet ID read from
* EEPROM.
*
* RETURNS: OK on success and ERROR on failure
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPegasusInit
    (
    USBD_NODE_ID devId,         /* Id of the Pegasus Device to be initialized */
    UINT8 * macAdrs             /* Ethernet ID to be returned */
    )
    {

    UINT16 phyWord;
    UINT8 * pBfr;
    UINT16 actLen;
    UINT8 * pEepromData;
    UINT8 tempIndex;
    UINT8 tempIndex2=0;
    int i;

    /* Create buffers for data transfer */

    pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN);

    if (pBfr == NULL)
	{
	printf ("usbPegasusInit() could not allocate memory\n");
	return ERROR;
	}

    pEepromData = OSS_MALLOC (255);

    if (pEepromData == NULL)
	{
	printf ("usbPegasusInit() could not allocate memory\n");

	OSS_FREE (pBfr);

	return ERROR;
	}

    /* Read the Ethernet ID registers from the device */

      if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_GET_BMREQ, 
                           USB_PEGASUS_GET_BREQ, 
                           0, 0x10, 0x6, pEepromData, &actLen) != OK) 
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Reading Ethernet ID \n",
		    0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Set USB test registers */

    *pBfr = 0xa5;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            0, 0x1d, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x1d register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }

    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            1, 0x7b, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x7b register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }
     
    taskDelay(100);

    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            2, 0x7b, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x7b register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }
    
    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            0xc0, 0x80, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x80 register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }
 
    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            0xff, 0x83, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x83 register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }  

     if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            0x01, 0x84, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x80 register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }  

      if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ,
                            USB_PEGASUS_SET_BREQ,
                            2, 0x81, 1, pBfr, &actLen) != OK)
       {
       PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error setting 0x81 register\n",
                       0, 0, 0, 0, 0, 0);
       goto errorExit;
       }               
                            
    /* Read the contents of EEPROM */

    for(tempIndex =0; tempIndex<16; tempIndex++)
	{
	if(usbPegasusReadSrom (devId, tempIndex, &phyWord) != OK)
	    {
	    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error retreiving Ethernet "
			 "descriptor \n", 0, 0, 0, 0, 0, 0);
	    goto errorExit;
	    }

	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Read %04x \n", phyWord, 0, 0, 0, 0, 0);

        /*
         * The first six bytes (or 3 words) of the EEPROM data is the MAC
         * address and should not be byte swapped
         */
     	if (tempIndex < 3)
            {
#if (_BYTE_ORDER == _BIG_ENDIAN) 
  
            *((UINT16 *)(pEepromData + tempIndex2)) = TO_BIGW(phyWord);

#else

            *((UINT16 *)(pEepromData + tempIndex2)) = TO_LITTLEW(phyWord);

#endif
            tempIndex2 += 2;
	    }

       else
           {

           *(pEepromData + tempIndex2++) = phyWord & 0x00ff;
           *(pEepromData + tempIndex2++) = phyWord >> 8;

           }
       }


    for(tempIndex = 0; tempIndex<6; tempIndex++)
	{
  	macAdrs[tempIndex] = *(pEepromData + tempIndex);
	}

    /* Set the Ethernet ID in the PEGASUS registers */

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x10, 0x6, pEepromData,   &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting Ethernet ID \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Enable the GPIO registers */

    *pBfr = 0x24;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x7e, 2, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 2 Registers \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    *pBfr = 0x26;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x7e, 2, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 3 Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
    
    /* Read the PHY Identifier */

    if(usbPegasusReadPhy (devId, 0x2, &phyWord) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Reading PHY Regs\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    i=2;
    DELAY(i)
    DELAY(i)

    /* Read the Status of the Link */

    if(usbPegasusReadPhy (devId, 0x1, &phyWord) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Reading PHY Regs\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Enable GPIO outputs */

    *pBfr = 0x26;
    *(pBfr + 1) = 0x11;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x7f, 2, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 4 Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Read the status of the Link */

    if(usbPegasusReadPhy (devId, 0x1, &phyWord) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Reading PHY Regs \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Enable the USB test pins */

    *pBfr = 0xc0;
    *(pBfr + 1) = 0x00;

    if(usbPegasusReadPhy (devId, 0x1, &phyWord) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Reading PHY Regs \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
	 
    *pBfr = 0x39;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
			   USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x0, 2, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 6 Registers \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
	 
    /* GPIO Output */

    if (usbdVendorSpecific (pegasusHandle, devId,
			   USB_PEGASUS_GET_BMREQ, 
                           USB_PEGASUS_GET_BREQ, 
                           0, 0x7a, 0x2, pBfr,   &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 7 Registers \n", 
		    0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
	 
    *pBfr = 0x08;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x01, 2, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 8 Registers \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
	 
    phyWord = 0x2000;

    if(usbPegasusWritePhy (devId, 0x2, 0x0, phyWord))
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting PHY Registers 0x02 \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Set Link partners Autonegatiation capability */

    phyWord = 0x0021;

    if(usbPegasusWritePhy (devId, 0x1, 0x5, phyWord))
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting PHY Registers 0x05 \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
  
    phyWord = 0x0400;

    if(usbPegasusWritePhy (devId, 0x2, 0x0, phyWord))
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting PHY Registers 0x02 \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
	 
    *pBfr = 0xc4;
    *(pBfr + 1) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x78, 2, pBfr,  &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting GPIO Regs \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}
    /* Enable the Ethernet Receive and Transmit */	 

    *pBfr = 0xfa;	
    *(pBfr + 1) = 0x30;
    *(pBfr + 2) = 0x00;
    *(pBfr + 3) = 0x00;

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x00, 4, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting Ethernet Control Regs \n",
				0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    OSS_FREE (pBfr);
    OSS_FREE (pEepromData);

    return OK;

errorExit:

    OSS_FREE (pBfr);
    OSS_FREE (pEepromData);

    return ERROR;
    }    

/***************************************************************************
*
* usbPegasusReadPHY - read the contents of the PHY registers
*
* Reads the register contents of PHY a device.
*
* RETURNS: OK if successful or ERROR on failure
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPegasusReadPhy
    (
    USBD_NODE_ID devId, 
    UINT8 offSet,
    UINT16 * phyWord
    )
    {
    UINT16 actLen=0;
    UINT8 * pBfr;

    pBfr = OSS_MALLOC (255);
 
    /* Create buffers for data transfer */

    if (pBfr == NULL)
	{
	printf ("usbPegasusReadPhy() could not allocate memory\n");
	return ERROR;
	}
 
    /* Write the off set into the PHy access registers */

    *pBfr = 1;
    *(pBfr + 1) = 0x0;
    *(pBfr + 2) = 0x0;
    *(pBfr + 3) = 0x40|(offSet&0x1f);

    if (usbdVendorSpecific (pegasusHandle, devId,
       			   USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x25, 4, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 9 Registers \n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Read the contents read from PHY */

    if (usbdVendorSpecific (pegasusHandle, devId,
			   USB_PEGASUS_GET_BMREQ, 
                           USB_PEGASUS_GET_BREQ, 
                           0, 0x26, 3, pBfr,   &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error reading Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* If operation successful return the data */

    if (*(pBfr + 2) & 0x80) 
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Read Registers %d %02x, %02x  %02x \n", 
		    offSet, *pBfr, *(pBfr + 1), *(pBfr + 2), 0, 0);
	*phyWord = *(UINT16 *)pBfr;
	}
    else
	goto errorExit;

    OSS_FREE (pBfr);
    return OK;

errorExit:

    OSS_FREE (pBfr);
    return ERROR;

    }



/***************************************************************************
*
* usbPegasusReadReg - read contents of specified and print 
*
* This function reads the register contents of Pegasus and prints them for 
* debugging purposes.
*
* RETURNS: OK if successful or ERROR on failure
*
* ERRNO: none
*/

STATUS usbPegasusReadReg
    (
    USBD_NODE_ID devId,        /* pointer to  device */
    UINT8 offSet,	       /* Offset of the registers */
    UINT8 noOfRegs	       /* No of registers to be read */
    )
    {

    UINT16 actLen=0; 
    UINT8 * pBfr;
    int i;

    /* Create buffers for data transfer */

    pBfr = OSS_MALLOC (10);
 
    if (pBfr == NULL)
	{
	printf ("usbPegasusReadReg() could not allocate memory\n");
	return ERROR;
	}
 
    /* read the registers */

    for(i=offSet; i<(offSet+noOfRegs); i++)
	{
	*pBfr = 0;

	if (usbdVendorSpecific (pegasusHandle, devId,
	                        USB_PEGASUS_GET_BMREQ, 
                                USB_PEGASUS_GET_BREQ, 
                                0, i, 1, pBfr,   &actLen) != OK)
	    {
	    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 10 Registers\n",
			0, 0, 0, 0, 0, 0);

	    OSS_FREE (pBfr);
	    return ERROR;
	    }
	    PEGASUS_LOG (PEGASUS_DBG_LOAD, "Registers offSet %d %02x %d \n",
			i, (UINT32)pBfr, actLen, 0, 0, 0);
	}

    OSS_FREE (pBfr);
    return OK;
    }

/***************************************************************************
*
* usbPegasusWritePHY - write the data into the PHY registers 
*
* This routine writes the data into the registers of the PHY device.
*
* RETURNS: OK if successful or ERROR on failure.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPegasusWritePhy
    (
    USBD_NODE_ID devId,        /* pointer to  device */
    UINT8 direction,		/* PHY Identifier */
    UINT8 offSet,		/* Register off set */
    UINT16 phyWord		/* Data to be written */
    )
    {
    UINT16 actLen=0;
    UINT8 * pBfr;

    /* Create buffers for data transfer */

    pBfr = OSS_MALLOC (8);
 
    if (pBfr == NULL)
	{
	printf ("usbPegasusWritePhy() could not allocate memory\n");
	return ERROR;
	}
 
    /* Fill the Registers for accessing the PHY */

    *pBfr = direction;
    *(pBfr + 1) = (UINT8)(phyWord&0x00ff);
    *(pBfr + 2) = (UINT8)(phyWord>>8);
    *(pBfr + 3) = 0x20|(offSet&0x1f);
 
    /* Write into the control Registers */

    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_SET_BMREQ, 
                            USB_PEGASUS_SET_BREQ, 
                            0, 0x25, 4, pBfr,   &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 11 Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Read the PHY contents back */

    if (usbdVendorSpecific (pegasusHandle, devId,
                            USB_PEGASUS_GET_BMREQ, 
                            USB_PEGASUS_GET_BREQ, 
                            0, 0x28, 1, pBfr,   &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Writing Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Check whether the opearation is succesful */

    if (*pBfr & 0x80) 
	{
	OSS_FREE (pBfr);
	return OK;
	}

errorExit:

    OSS_FREE (pBfr);
    return ERROR;

    }

/***************************************************************************
*
* usbPegasusReadSrom - read the contents of Serial EEPROM
*
* This function reads the Ethernet ID and other data from EEPROM.
*
* RETURNS: OK if successful or ERROR on failure.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPegasusReadSrom
    (
    USBD_NODE_ID devId, 	/* Pegasus Device ID */
    UINT8 offSet,		/* Location Offset */
    UINT16 * phyWord		/* Data word */
    )
    {

    UINT16 actLen=0;
    UINT8 * pBfr;

    /* Create buffers for data transfer */

    pBfr = OSS_MALLOC (255);
 
    if (pBfr == NULL)
	{
	printf ("usbPegasusReadSrom() could not allocate memory\n");
	return ERROR;
	}
 
    *pBfr = offSet;
    *(pBfr + 1) = 0x0;
    *(pBfr + 2) = 0x0;
    *(pBfr + 3) = 0x2;
    
    /* Write into the control registers for accessing the EEPROM */

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_SET_BMREQ, 
                           USB_PEGASUS_SET_BREQ, 
                           0, 0x20, 4, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error Setting 12 Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Read the Contents back */

    if (usbdVendorSpecific (pegasusHandle, devId,
                           USB_PEGASUS_GET_BMREQ, 
                           USB_PEGASUS_GET_BREQ, 
                           0, 0x21, 3, pBfr, &actLen) != OK)
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Error reading Registers\n",
			0, 0, 0, 0, 0, 0);
	goto errorExit;
	}

    /* Check whether the opeartion is successful */

    if (*(pBfr + 2) & 0x4) 
	{
	PEGASUS_LOG (PEGASUS_DBG_LOAD, "Read Registers %d %02x, %02xn",
			offSet, *(pBfr +0), *(pBfr + 1), 0, 0, 0);
	*phyWord = *(UINT16 *)pBfr;

	OSS_FREE (pBfr);
	return OK;
	}

errorExit:

    OSS_FREE (pBfr);
    return ERROR;

    }


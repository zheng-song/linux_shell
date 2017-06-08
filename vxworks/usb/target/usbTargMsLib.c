/* usbTargMsLib.c - Mass Storage routine library */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification History
--------------------
01e,02aug04,mta  Merge changes from integration branch to development branch
01d,29jul04,pdg  Fixed coverity error
01c,23jul04,hch  change vxworks.h to vxWorks.h
01b,01jul04,mta isp1582 changes
01a,15mar04,jac written.
*/

/* 
DESCRIPTION

This module defines those routines directly referenced by the USB peripheral
stack; namely, the routines that intialize the USB_TARG_CALLBACK_TABLE data
structure. Additional routines are also provided which are specific to the
mass storage driver.

INCLUDES: vxWorks.h, stdio.h, errnoLib.h, logLib.h, string.h, blkIo.h,
          usb/usbPlatform.h, usb/usb.h, usb/usbDescrCopyLib.h, usb/usbLib.h,
          usb/target/usbTargLib.h, drv/usb/usbBulkDevLib.h,
          drv/usb/target/usbTargMsLib.h, drv/usb/target/usbTargRbcLib.h

*/


/* includes */

#include "vxWorks.h"
#include "stdio.h"
#include "errnoLib.h"
#include "logLib.h"
#include "string.h"
#include "blkIo.h"
#include "usb/usbPlatform.h"
#include "usb/usb.h"
#include "usb/usbDescrCopyLib.h"
#include "usb/usbLib.h"
#include "usb/target/usbTargLib.h"
#include "drv/usb/usbBulkDevLib.h"
#include "drv/usb/target/usbTargMsLib.h"
#include "drv/usb/target/usbTargRbcCmd.h"
#include "drv/usb/target/usbTargRbcLib.h"

/* defines */

#ifdef USB_DEBUG_PRINT			/* debug macros */
    #define BUF_SIZE        255
    #define DEBUG_PRINT(a)  logMsg(a,0,0,0,0,0,0)
#endif

/*
 * The USE_MS_TEST_DATA macro allows the bulk-in and bulk-out pipes to be
 * tested using dummy data. The host computer must employ a special purpose
 * driver that invokes the vendor specific device request with the request
 * value set to either MS_BULK_IN_TX_TEST or MS_BULK_IN_RX_TEST. If set to
 * MS_BULK_IN_TX_TEST, the device transmits dummy data to the host;
 * alternatively, if set to MS_BULK_IN_RX_TEST, the device receives dummy
 * data from the host.
 */

#undef USE_MS_TEST_DATA

#ifdef USE_MS_TEST_DATA
    #define MS_TEST_DATA_SIZE   32	/* test data size */
    #define MS_BULK_IN_TX_TEST  1	/* transmit buffer test */
    #define MS_BULK_OUT_RX_TEST 2	/* receive buffer test */
#endif

#ifdef USE_RBC_SUBCLASS
    #define USB_MS_SUBCLASS         0x01 /* RBC command block */
#elif defined(USE_SCSI_SUBCLASS)
    #define USB_MS_SUBCLASS         USB_SUBCLASS_SCSI_COMMAND_SET  
					/* SCSI command block */
#else
    #error USB_MS_SUBCLASS undefined
#endif
    
/*
 * Define USE_DMA_ENDPOINT to direct the bulk I/O data to the 
 * Philips PDIUSBD12 "main IN" (endpoint number 5) and "main OUT"
 * (endpoint number 4) which use DMA.  Un-define USE_DMA_ENDPOINT
 * to direct data to the generic bulk I/O endpoint numbers which use
 * programmed-IO: 3 for bulk In, and 2 for bulk Out. 
 */

#undef USE_DMA_ENDPOINT 

#define MS_NUM_ENDPOINTS	2	/* mass storage endpoints */

#ifdef MS_USE_DMA_ENDPOINT

#define MS_BULK_IN_ENDPOINT_NUM		0x82	/* BULK IN endpoint */
#define MS_BULK_OUT_ENDPOINT_NUM	0x2	/* BULK OUT endpoint */

#else

#define MS_BULK_IN_ENDPOINT_NUM		0x81	/* BULK IN endpoint */
#define MS_BULK_OUT_ENDPOINT_NUM	0x1	/* BULK OUT endpoint */ 	
#endif

#ifdef	USE_DMA_ENDPOINT		
    #define MS_BULK_IN_ENDPOINT_ID	5	/* DMA - Bulk IN endpoint ID */	
#else
    #define MS_BULK_IN_ENDPOINT_ID	3	/* PIO - Bulk IN endpoint ID */
#endif

#ifdef	USE_DMA_ENDPOINT
    #define MS_BULK_OUT_ENDPOINT_ID	4	/* DMA - Bulk OUT endpoint ID */
#else
    #define MS_BULK_OUT_ENDPOINT_ID	2	/* PIO - Bulk OUT endpoint ID */ 
#endif

/* string identifiers and indexes for string descriptors */

#define UNICODE_ENGLISH		0x409		/* unicode */

#define ID_STR_MFG		1		/* manufacture's id */
#define ID_STR_MFG_VAL		"Wind River Systems"  /* manufacture's name */

#define ID_STR_PROD		2		/* product id */
#define ID_STR_PROD_VAL		"USB mass storage emulator"/* product name */

#define MS_USB_HIGH_SPEED	0

#define MS_USB_FULL_SPEED_VERSION	0x0100	/* USB full speed */	
#define MS_USB_HIGH_SPEED_VERSION	0x0200	/* USB high speed */ 

#if (MS_USB_HIGH_SPEED == 1)
#define MS_USB_VERSION  MS_USB_HIGH_SPEED_VERSION
#else
#define MS_USB_VERSION  MS_USB_FULL_SPEED_VERSION
#endif

/* mass storage configuration */

#define MS_NUM_CONFIG			1	/* number of configuration */
#define MS_CONFIG_VALUE			1	/* configuration value */

/* mass storage interface */

#define MS_NUM_INTERFACES		1	/* number of interfaces */
#define MS_INTERFACE_NUM		0	/* interface number */
#define MS_INTERFACE_ALT_SETTING	0	/* alternate setting */

#define MS_ID_VENDOR			0x0781	/* mass storage vendor ID */	
						/* Use a SanDisk ID so it */
						/* works on Windows */
#ifdef MS_USE_DMA_ENDPOINT

#define MS_BULK_MAX_PACKETSIZE		8	/* bulk max packet size */
#else

#define MS_BULK_MAX_PACKETSIZE		0x10	/* bulk max packet size */

#endif

#define MS_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE  0x40 /* Maximum packet size for */
                                                    /* the default control */
                                                    /* endpoint, if the device*/
                                                    /* is operating at high */
                                                    /* speed */


/* globals */

extern BOOL g_bulkInStallFlag;			/* bulk IN flag */
extern BOOL g_bulkOutStallFlag;			/* bulk OUT flag */

/* locals */

LOCAL char	*g_pStrMfg   = ID_STR_MFG_VAL;	/* manufacture's name */	
LOCAL char	*g_pStrProd  = ID_STR_PROD_VAL;	/* product name */

LOCAL UINT8	g_configuration = 0x0;	/* configuration value */ 
					/* zero means device unconfigured */
LOCAL UINT8	g_ifAltSetting  = 0x0;	/* alternate setting */

LOCAL UINT32    g_uDeviceFeature        = 0;	/* device feature */
LOCAL UINT32    g_uEndpointNumberBitmap = 0;	/* endpoint bitmap */

#ifndef USB1_1
LOCAL UINT32    g_uSpeed = USB_TCD_FULL_SPEED;	/* device operating speed */
#endif
LOCAL UINT8     g_uDeviceStatus = 0x01;		/* device status */

#ifdef USE_MS_TEST_DATA
LOCAL UINT8     g_usbMsTestData[MS_TEST_DATA_SIZE];	/* test data */
#endif

#ifdef USB_DEBUG_PRINT
LOCAL char      g_buf[BUF_SIZE];		/* buffer to hold data */
LOCAL BOOL      g_usbDbgPrint = FALSE;		/* debug flag */
#endif

#ifdef USB1_1
LOCAL UINT16	g_numEndpoints;			/* number of endpoints */
LOCAL pUSB_TARG_ENDPOINT_INFO g_pEndpoints;	/* USB_TARG_ENDPOINT_INFO */
#endif

LOCAL USB_BULK_CBW g_cbw = 			/* command block wrapper */
    {
    USB_BULK_SWAP_32 (USB_CBW_SIGNATURE),	/* USB signature */
    0,
    0,
    0,
    0,
    0,
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };

LOCAL USB_BULK_CSW g_csw = 			/* command status wrapper */
    {
    USB_BULK_SWAP_32 (USB_CSW_SIGNATURE),
    0,
    0,
    0
    };

/* bulk-In endpoint variables */
LOCAL USB_ERP       g_bulkInErp;          /* bulk-In ERP */
LOCAL BOOL          g_bulkInInUse;        /* bulk-In ERP in use flag */
LOCAL BOOL          g_bulkInBfrValid;     /* bulk-In ERP buffer valid flag */
LOCAL USB_TARG_PIPE g_bulkInPipeHandle;   /* bulk-In ERP pipe handle */
LOCAL BOOL          g_bulkInStallStatus = FALSE; /*bulk-in stall status */

/* bulk-Out endpoint variables */
LOCAL USB_ERP       g_bulkOutErp;         /* bulk-out ERP */
LOCAL BOOL          g_bulkOutInUse;       /* bulk-out ERP in use flag */
LOCAL BOOL          g_bulkOutBfrValid;    /* bulk-out ERP buffer valid flag */
LOCAL USB_TARG_PIPE g_bulkOutPipeHandle;  /* bulk-out ERP pipe handle */
LOCAL BOOL          g_bulkOutStallStatus = FALSE; /* bulk-out stall status */

LOCAL USB_TARG_CHANNEL g_targChannel;     /* the target channel */
LOCAL UINT16           g_deviceAddr;      /* device address */

#ifdef USB1_1
LOCAL BOOL             g_remoteDevWakeup = FALSE; /* remote wakeup flag */
#endif

/* descriptor definitions */

LOCAL USB_LANGUAGE_DESCR g_langDescr =	/* language descriptor */ 
    {
    sizeof (USB_LANGUAGE_DESCR),	/* bLength */ 
    USB_DESCR_STRING,			/* string descriptor */ 
    {TO_LITTLEW (UNICODE_ENGLISH)}	/* unicode */
    };

LOCAL USB_DEVICE_DESCR g_devDescr =	/* device descriptor */
    {
    USB_DEVICE_DESCR_LEN,           /* bLength */
    USB_DESCR_DEVICE,               /* bDescriptorType */
    TO_LITTLEW (MS_USB_VERSION),    /* bcdUsb */
    0,                              /* bDeviceClass */
    0,                              /* bDeviceSubclass */
    0,                              /* bDeviceProtocol */
    USB_MIN_CTRL_PACKET_SIZE,       /* maxPacketSize0 */
    MS_ID_VENDOR,                   /* idVendor */
    0,                              /* idProduct */
    0,                              /* bcdDevice */
    ID_STR_MFG,                     /* iManufacturer */
    ID_STR_PROD,                    /* iProduct */
    0,                              /* iSerialNumber */
    MS_NUM_CONFIG                   /* bNumConfigurations */
    };

LOCAL USB_CONFIG_DESCR g_configDescr =	/* configuration descriptor */
    {
    USB_CONFIG_DESCR_LEN,			/* bLength */
    USB_DESCR_CONFIGURATION,			/* bDescriptorType */
    TO_LITTLEW (USB_CONFIG_DESCR_LEN +
                USB_INTERFACE_DESCR_LEN +
                2*USB_ENDPOINT_DESCR_LEN),	/* wTotalLength */
    MS_NUM_INTERFACES,				/* bNumInterfaces */
    MS_CONFIG_VALUE,		                /* bConfigurationValue */
    0,						/* iConfiguration */
    0x80 | USB_ATTR_SELF_POWERED,	        /* bmAttributes */
    0						/* MaxPower */
    };

LOCAL USB_INTERFACE_DESCR g_ifDescr =	/* interface descriptor */ 
    {
    USB_INTERFACE_DESCR_LEN,            /* bLength */
    USB_DESCR_INTERFACE,                /* bDescriptorType */
    MS_INTERFACE_NUM,                   /* bInterfaceNumber */
    MS_INTERFACE_ALT_SETTING,           /* bAlternateSetting */
    MS_NUM_ENDPOINTS,                   /* bNumEndpoints */
    USB_CLASS_MASS_STORAGE,             /* bInterfaceClass */
    USB_MS_SUBCLASS,                    /* bInterfaceSubClass */
    USB_INTERFACE_PROTOCOL_BULK_ONLY,   /* bInterfaceProtocol */
    0                                   /* iInterface */
    };

LOCAL USB_ENDPOINT_DESCR g_bulkOutEpDescr =	/* OUT endpoint descriptor */
    {
    USB_ENDPOINT_DESCR_LEN,     	/* bLength */
    USB_DESCR_ENDPOINT,         	/* bDescriptorType */
    MS_BULK_OUT_ENDPOINT_NUM,   	/* bEndpointAddress */
    USB_ATTR_BULK,              	/* bmAttributes */
    TO_LITTLEW (MS_BULK_MAX_PACKETSIZE),/* max packet size */
    0                           	/* bInterval */
    };

LOCAL USB_ENDPOINT_DESCR g_bulkInEpDescr =	/* IN endpoint descriptor */
    {
    USB_ENDPOINT_DESCR_LEN,     	/* bLength */
    USB_DESCR_ENDPOINT,         	/* bDescriptorType */
    MS_BULK_IN_ENDPOINT_NUM,    	/* bEndpointAddress */
    USB_ATTR_BULK,              	/* bmAttributes */
#ifdef MS_USE_DMA_ENDPOINT
    TO_LITTLEW (0x10),			/* max packet size */
#else
    TO_LITTLEW (MS_BULK_MAX_PACKETSIZE),/* max packet size */
#endif
    0                          		/* bInterval */
    };

#ifndef USB1_1	
LOCAL USB_DEVICE_QUALIFIER_DESCR g_usbDevQualDescr =	/* device qualifier */
    {
    USB_DEVICE_QUALIFIER_DESCR_LEN,	/* size of descriptor */
    USB_DESCR_DEVICE_QUALIFIER,		/* DEVICE_QUALIFIER type */
    TO_LITTLEW(MS_USB_VERSION),		/* USB spec. (0x0200) */
    0,					/* class code */
    0,					/* subclass code */
    0,					/* protocol code */
    USB_MIN_CTRL_PACKET_SIZE,		/* other speed packet size */
    0x1,				/* number of other speed configs */
    0					/* reserved */
    };
#endif


/* forward declarations */

#ifdef USE_MS_TEST_DATA

void usbMsTestTxCallback (pVOID p);
void usbMsTestRxCallback (pVOID p);

#endif

/* The USB1_1 macro is for backward compatibility with the USB 1.1 stack */

#ifdef USB1_1		/* USB1.1 */

LOCAL STATUS mngmtFunc (pVOID param, USB_TARG_CHANNEL targChannel,
                        UINT16 mngmtCode);

#else			/* USB2.0 */	

LOCAL STATUS mngmtFunc (pVOID param, USB_TARG_CHANNEL targChannel,
                        UINT16 mngmtCode, pVOID pContext);
#endif

LOCAL STATUS featureClear (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT8 requestType, UINT16 feature, UINT16 index);

LOCAL STATUS featureSet (pVOID param,USB_TARG_CHANNEL targChannel,
                         UINT8 requestType, UINT16 feature, UINT16 index);

#ifdef USB1_1		/* USB1.1 */

LOCAL STATUS statusGet (pVOID param, USB_TARG_CHANNEL targChannel, 
                        UINT8 requestType, UINT16 index, UINT16 length,
                        pUINT8 pBfr, pUINT16 pActLen);

#else			/* USB2.0 */

LOCAL STATUS statusGet (pVOID param, USB_TARG_CHANNEL targChannel,
                        UINT16 requestType, UINT16 index, UINT16 length,
                        pUINT8 pBfr);
#endif

LOCAL STATUS addressSet (pVOID param, USB_TARG_CHANNEL targChannel,
                         UINT16 deviceAddress);

LOCAL STATUS descriptorGet (pVOID param, USB_TARG_CHANNEL targChannel,
                            UINT8 requestType, UINT8 descriptorType,
                            UINT8 descriptorIndex, UINT16 languageId,
                            UINT16 length, pUINT8 pBfr,
                            pUINT16 pActLen);

LOCAL STATUS configurationGet (pVOID param, USB_TARG_CHANNEL targChannel,
                               pUINT8 pConfiguration);

LOCAL STATUS configurationSet (pVOID param, USB_TARG_CHANNEL targChannel,
                               UINT8 configuration);

LOCAL STATUS interfaceGet (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT16 interfaceIndex,pUINT8 pAlternateSetting);

LOCAL STATUS interfaceSet (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT16 interfaceIndex, UINT8 alternateSetting);

LOCAL STATUS vendorSpecific (pVOID param, USB_TARG_CHANNEL targChannel,
                             UINT8 requestType, UINT8 request, UINT16 value,
                             UINT16 index, UINT16 length);

void usbMsTargError (void);

LOCAL USB_TARG_CALLBACK_TABLE usbTargMsCallbackTable = /* callback table */
    {
    mngmtFunc,          		/* mngmtFunc */
    featureClear,       		/* featureClear */
    featureSet,         		/* featureSet */
    configurationGet,   		/* configurationGet */
    configurationSet,   		/* configurationSet */
    descriptorGet,      		/* descriptorGet */
    NULL,               		/* descriptorSet */
    interfaceGet,       		/* interfaceGet */
    interfaceSet,       		/* interfaceSet */
    statusGet,          		/* statusGet */
    addressSet,         		/* addressSet */
    NULL,               		/* synchFrameGet */
    vendorSpecific      		/* vendorSpecific */
    };

/*******************************************************************************
*
* usbMsDevInit - initialize the mass storage driver
*
* This routine initializes the mass storage driver.
*
* RETURNS: OK or error.
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS usbMsDevInit (void)
    {

    /* initialize the RBC block I/O device */

    STATUS retVal = usbTargRbcBlockDevCreate();

    return(retVal);
    }

/*******************************************************************************
*
* usbMsCBWGet - get the last mass storage CBW received
*
* This routine retrieves the last CBW received on the bulk-out pipe.
*
* RETURNS: USB_BULK_CBW
*
* ERRNO:
*  none.
*/

USB_BULK_CBW *usbMsCBWGet (void)
    {

    /* return instance of last saved CBW data structure */

    return(&g_cbw);
    }

/*******************************************************************************
*
* usbMsCBWInit - initialize the mass storage CBW
*
* This routine initializes the CBW by resetting all fields to their default
* value.
*
* RETURNS: USB_BULK_CBW
*
* ERRNO:
*  none.
*/

USB_BULK_CBW *usbMsCBWInit (void)
    {
    
    /* init CBW and set signature */
    
    memset(&g_cbw, 0, sizeof(USB_BULK_CBW));
    g_cbw.signature = USB_BULK_SWAP_32 (USB_CBW_SIGNATURE);

    return(&g_cbw);
    }

/******************************************************************************
*
* usbMsCSWGet - get the current CSW
*
* This routine retrieves the current CSW.
*
* RETURNS: USB_BULK_CSW
*
* ERRNO:
*  none.
*/

USB_BULK_CSW *usbMsCSWGet (void)
    {

    /* return instance of last saved CSW data structure */

    return(&g_csw);
    }

/*******************************************************************************
*
* usbMsCSWInit - initialize the CSW
*
* This routine initializes the CSW.
*
* RETURNS: USB_BULK_CSW
*
* ERRNO:
*  none
*/

USB_BULK_CSW *usbMsCSWInit (void)
    {
    memset(&g_csw, 0, sizeof(USB_BULK_CSW));
    g_csw.signature = USB_BULK_SWAP_32 (USB_CSW_SIGNATURE);

    return(&g_csw);
    }


/*******************************************************************************
*
* usbMsBulkInStall - stall the bulk-in pipe
*
* This routine stalls the bulk-in pipe.
*
* RETURNS: OK or ERROR if not able to stall the bulk IN endpoint.
*
* ERRNO: 
*  none
*/

STATUS usbMsBulkInStall (void)
    {
    STATUS	retVal = ERROR;

#ifdef USB1_1
    retVal = usbTargPipeStatusSet(g_targChannel,
                                  g_bulkInPipeHandle,
                                  TCD_ENDPOINT_STALL);
#else
    retVal = usbTargPipeStatusSet(g_bulkInPipeHandle,
                                  TCD_ENDPOINT_STALL);
#endif

    if (retVal == OK)
        g_bulkInStallStatus = TRUE;
    return(retVal);
    }


/*******************************************************************************
*
* usbMsBulkInUnStall - unstall the bulk-in pipe
*
* This routine unstalls the bulk-in pipe.
*
* RETURNS: OK or ERROR if not able to un-stall the bulk IN endpoint.
*
* ERRNO: 
*  none
*/

STATUS usbMsBulkInUnStall (void)
    {
    STATUS	retVal = ERROR;

    /* Unstall the endpoint */

#ifdef USB1_1

    retVal = usbTargPipeStatusSet(g_targChannel,
                                  g_bulkInPipeHandle,
                                  TCD_ENDPOINT_UNSTALL);
#else
    retVal = usbTargPipeStatusSet(g_bulkInPipeHandle,
                                  TCD_ENDPOINT_UNSTALL);
#endif

    if (retVal == OK)
        g_bulkInStallStatus = FALSE;

    return(retVal);
    }

/*******************************************************************************
*
* usbMsBulkOutStall - stall the bulk-out pipe
*
* This routine stalls the bulk-out pipe.
*
* RETURNS: OK or ERROR in unable to stall the bulk OUT endpoints.
*
* ERRNO:
*  none.
*/

STATUS usbMsBulkOutStall (void)
    {
    STATUS	retVal = ERROR;

    /* Stall the bulk OUT endpoint */

#ifdef USB1_1

    retVal = usbTargPipeStatusSet(g_targChannel,
                                  g_bulkOutPipeHandle,
                                  TCD_ENDPOINT_STALL);
#else
    retVal = usbTargPipeStatusSet(g_bulkOutPipeHandle,
                                  TCD_ENDPOINT_STALL);
#endif
    if (retVal == OK)
        g_bulkOutStallStatus = TRUE;

    return(retVal);
    }

/*******************************************************************************
*
* usbMsBulkOutUnStall - unstall the bulk-out pipe
*
* This routine unstalls the bulk-out pipe.
*
* RETURNS: OK or ERROR if not able to unstall the bulk out endpoints
*
* ERRNO: 
*  none
*/

STATUS usbMsBulkOutUnStall (void)
    {
    STATUS	retVal = ERROR;

    /* un-stall the bulk-out endpoint */ 

#ifdef USB1_1
    retVal = usbTargPipeStatusSet(g_targChannel,
                                  g_bulkOutPipeHandle,
                                  TCD_ENDPOINT_UNSTALL);
#else
    retVal = usbTargPipeStatusSet(g_bulkOutPipeHandle,
                                  TCD_ENDPOINT_UNSTALL);
#endif

    if (retVal == OK)
        g_bulkOutStallStatus = FALSE;

    return(retVal);
    }


/******************************************************************************
*
* featureClear - clear the specified feature
*
* This routine implements the clear feature standard device request.
*
* RETURNS: OK or ERROR if not able to clear the feature 
*
* ERRNO:
*  none. 
*/

LOCAL STATUS featureClear
    (
    pVOID		param,		/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8		requestType,	/* request type */
    UINT16		feature,	/* feature to clear */
    UINT16		index		/* index */
    )
    {
    STATUS		status = ERROR;	/* status value */
    UINT8		* pData;	/* to hold CSW */
    UINT32		dSize;		/* size of USB_BULK_CSW */
    struct usbBulkCsw	* pCsw;		/* Command Status Wrapper */
    BOOL		CSWsend = FALSE;

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /* This request is not accepted when the device is in the default state */

    if (g_deviceAddr == 0)
        return ERROR;

    /* this request must be standard request from the host */

    if (((requestType & USB_RT_DIRECTION_MASK) != USB_RT_HOST_TO_DEV) ||
        ((requestType & USB_RT_CATEGORY_MASK) != USB_RT_STANDARD))
        return(ERROR);

    requestType &= ~(USB_RT_DIRECTION_MASK | USB_RT_CATEGORY_MASK);

    if (requestType == USB_RT_ENDPOINT)
        {
        if (feature == USB_FSEL_DEV_ENDPOINT_HALT)
            {
            if (index == MS_BULK_IN_ENDPOINT_NUM)
                {
                status = usbMsBulkInUnStall();

                if ((status == OK) && (g_bulkInStallFlag == TRUE))
                    {
                    g_bulkInStallFlag = FALSE;
                    CSWsend = TRUE;
                    }
                }
            else if (index == MS_BULK_OUT_ENDPOINT_NUM)
                {
                status = usbMsBulkOutUnStall();

                if ((status == OK) && (g_bulkOutStallFlag == TRUE))
                    {
                    g_bulkOutStallFlag = FALSE;
                    CSWsend = TRUE;
                    }
                }
            else
                status = ERROR;

            if (CSWsend == TRUE)
                {
                pCsw = usbMsCSWGet();
                pData = (UINT8 *)pCsw;
                dSize = sizeof(USB_BULK_CSW);
                if (usbMsBulkInErpInit(pData, dSize,
                                   bulkInErpCallbackCSW, NULL) != OK)
                    return(ERROR);                                   
                }

            return(status);
            }
        else
            return(ERROR);
        }
    else if (requestType == USB_RT_DEVICE)
        {
        if (feature == USB_FSEL_DEV_REMOTE_WAKEUP)
            {

#ifdef USB1_1

            /* set remote device wakeup flag to TRUE */

            g_remoteDevWakeup = TRUE;

            return(OK);

#else

            /* 
             * If the device supports remote wakeup, call the function
             * to clear the remote wakeup feature
             */

            if ((g_uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                {
                status = usbTargDeviceFeatureClear(targChannel, feature);

                /* Clear the global status */

                if (status == OK)
                    g_uDeviceStatus &= ~0x02;
                }
            else
                status = ERROR;

            return(status);
#endif
            }
        else
            return(ERROR);
        }
    else if (requestType == USB_RT_INTERFACE)
        return(ERROR);
    else
        return(ERROR);
    }


/*******************************************************************************
*
* featureSet - set the specified feature
*
* This routine implements the set feature standard device request.
*
* RETURNS: OK or ERORR if not able to set the feature.
*
* ERRNO:
*  none.
*/
LOCAL STATUS featureSet
    (
    pVOID		param,		/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */	
    UINT8		requestType,	/* request type */
    UINT16		feature,	/* feature to set */
    UINT16		index		/* wIndex */
    )
    {
    STATUS		status = ERROR;

    /* only one target */
	
    if (targChannel != g_targChannel) 
        return(ERROR);

    /*
     * This request is not accepted when the the device is in the default state
     * and the feature to be set is not TEST_MODE
     */

    if ((g_deviceAddr == 0) && (feature != USB_FSEL_DEV_TEST_MODE))
        return ERROR;

    /* this request must be standard request from the host */

    if (((requestType & USB_RT_DIRECTION_MASK) != USB_RT_HOST_TO_DEV) ||
        ((requestType & USB_RT_CATEGORY_MASK) != USB_RT_STANDARD))
        return(ERROR);

    requestType &= ~(USB_RT_DIRECTION_MASK | USB_RT_CATEGORY_MASK);

    if (requestType == USB_RT_ENDPOINT)
        {
        if (feature == USB_FSEL_DEV_ENDPOINT_HALT)
            {
            if (index == g_bulkInEpDescr.endpointAddress)
                status = usbMsBulkInStall();
            else if (index == g_bulkOutEpDescr.endpointAddress)
                status = usbMsBulkOutStall();
            else
                status = ERROR;

            return(status);
            }
        else
            return(ERROR);
        }
    else if (requestType == USB_RT_DEVICE)
        {
        if (feature == USB_FSEL_DEV_REMOTE_WAKEUP)
            {
#ifdef USB1_1

            /* set remote device wakeup flag to TRUE */

            g_remoteDevWakeup = TRUE;
            return(OK);
#else
            /*
             * If the device supports remote wakeup, call the function
             * to set the remote wakeup feature
             */

            if ((g_uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                {
                status = usbTargDeviceFeatureSet(targChannel, feature, index);

                /* Set the global status */

                if (status == OK)
                    g_uDeviceStatus |= 0x02;
                }
            else
                status = ERROR;

            return(status);
#endif
            }
        else if (feature == USB_FSEL_DEV_TEST_MODE) /* TEST_MODE */
            {
#ifdef USB1_1

#if(MS_USB_HIGH_SPEED == 1)

            /*
             * NOTE: device can only go into test mode until after this
             * request is acknowledged in the status phase
             */

             return(ERROR);
#else
             return(ERROR);
#endif
#else

            /*
             * If the device supports remote wakeup, call the function
             * to set the test mode feature
             */

            if ((g_uDeviceFeature & USB_FEATURE_TEST_MODE) != 0)
                {
                status = usbTargDeviceFeatureSet(targChannel,
                                                 feature,
                                                 ((index & 0xFF00) << 8));
                }

            return(status);
#endif
            }
        else
            return(ERROR);
        }
    else if (requestType == USB_RT_INTERFACE)
        return(ERROR);
    else
        return(ERROR);
    }

#ifdef USB1_1

/*******************************************************************************
*
* statusGet - get the specified status
*
* This routine implements the get status standard device request.
*
* RETURNS: OK or ERROR if not able to set the status
*
* ERRNO:
*  none
*/

LOCAL STATUS statusGet
    (
    pVOID		param,		/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8		requestType,	/* request type */
    UINT16		index,		/* wIndex */
    UINT16		length,		/* length */
    pUINT8		pBfr,		/* to hold status */
    pUINT16		pActLen		/* actual length */
    )
    {
    UINT8		data[2] = {0,0};/* To hold status information */
    UINT16		epStatus;	/* endpoint status */

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /* this request must be standard request to the host */

    if (((requestType & USB_RT_DIRECTION_MASK) != USB_RT_DEV_TO_HOST) ||
        ((requestType & USB_RT_CATEGORY_MASK) != USB_RT_STANDARD))
        return(ERROR);

    requestType &= ~(USB_RT_DIRECTION_MASK | USB_RT_CATEGORY_MASK);

    if (requestType == USB_RT_DEVICE)
        {

        /* self powered */

        data[1] = 0x1; 
        
        if (g_remoteDevWakeup == TRUE)
            {
            data[1] |= 0x2;
            }
        }
    else if (requestType == USB_RT_INTERFACE)
        {
        /* nothing to do */
        }
    else if (requestType == USB_RT_ENDPOINT)
        {
        if (index == MS_BULK_IN_ENDPOINT_NUM)
            {
            if (g_bulkInStallStatus == TRUE)
                epStatus = USB_ENDPOINT_STS_HALT;
            else
                epStatus = 0x0;
            }
        else if (index == MS_BULK_OUT_ENDPOINT_NUM)
            {
            if (g_bulkOutStallStatus == TRUE)
                epStatus = USB_ENDPOINT_STS_HALT;
            else
                epStatus = 0x0;
            }
        else
            return(ERROR);

        if (epStatus == USB_ENDPOINT_STS_HALT)
            data[0] = 0x1;
        }
    else
        return(ERROR);

    memcpy(pBfr,data,2);
    *pActLen = 2;

    return(OK);
    }
#else

/*******************************************************************************
*
* statusGet - get the specified status
*
* This routine implements the get status standard device request.
*
* RETURNS: OK or ERROR if not able to set the status
*
* ERRNO:
*  none
*/

LOCAL STATUS statusGet
    (
    pVOID		param,		/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16		requestType,	/* request type */
    UINT16		index,		/* wIndex */
    UINT16		length,		/* wLength */
    pUINT8		pBfr		/* to hold status */
    )
    {
    STATUS		status = ERROR;

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /* This is an invalid request if received in default state */

    if (g_deviceAddr == 0)
        return ERROR;

    /* this request must be standard request to the host */

    if (((requestType & USB_RT_DIRECTION_MASK) != USB_RT_DEV_TO_HOST) ||
        ((requestType & USB_RT_CATEGORY_MASK) != USB_RT_STANDARD))
        return(ERROR);

    requestType &= ~(USB_RT_DIRECTION_MASK | USB_RT_CATEGORY_MASK);

    if (requestType == USB_RT_DEVICE)
        {
        pBfr[0] = g_uDeviceStatus;
        status = OK;
        }
    else if (requestType == USB_RT_INTERFACE)
        {
        /* nothing to do */
        }
    else if (requestType == USB_RT_ENDPOINT)
        {
        if (index == g_bulkInEpDescr.endpointAddress)
            status = usbTargPipeStatusGet(g_bulkInPipeHandle, pBfr);

        else if (index == g_bulkOutEpDescr.endpointAddress)
            status = usbTargPipeStatusGet(g_bulkOutPipeHandle, pBfr);
        else
            return(ERROR);

        }
    else
        return(ERROR);

    return(status);
    }

#endif


/*******************************************************************************
*
* addressSet - set the specified address
*
* This routine implements the set address standard device request.
*
* RETURNS: ERROR or OK
*
* ERRNO: N/A
*/

LOCAL STATUS addressSet
    (
    pVOID		param,		/* TCD specific parameter */ 
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16		deviceAddress	/* device channel */
    )
    {

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /* The device cannot accept a set address request after configuration */

    if (g_configuration != 0)
        return ERROR;

    g_deviceAddr = deviceAddress;

    return(OK);
    }


/*******************************************************************************
*
* descriptorGet - get the specified descriptor
*
* This routine implements the get descriptor standard device request.
*
* RETURNS: OK or ERROR if not able to get the descriptor value
*
* ERRNO:
*  none
*/

LOCAL STATUS descriptorGet
    (
    pVOID		param,		/*  TCD specific parameter */		
    USB_TARG_CHANNEL	targChannel,	/* target chennel */
    UINT8		requestType,	/* request type */
    UINT8		descriptorType,	/* descriptor type */
    UINT8		descriptorIndex,/* descriptor index */
    UINT16		languageId,	/* language id */
    UINT16		length,		/* length of descriptor */
    pUINT8		pBfr,		/* buffer to hold the descriptor */
    pUINT16		pActLen		/* actual length */
    )
    {
    UINT8		bfr[USB_MAX_DESCR_LEN];	/* buffer to hold descriptor */
    UINT16		actLen;			/* actual lenght */

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /* this request must be standard request from the host */

    if (((requestType & USB_RT_DIRECTION_MASK) != USB_RT_DEV_TO_HOST) ||
        ((requestType & USB_RT_CATEGORY_MASK) != USB_RT_STANDARD) ||
        ((requestType & USB_RT_RECIPIENT_MASK) != USB_RT_DEVICE))
        return(ERROR);

    switch(descriptorType)
    {
    case USB_DESCR_DEVICE:

        /* copy device descriptor to pBfr and set pActLen */

        usbDbgPrint("descriptorGet: USB_DESCR_DEVICE\n");
        usbDescrCopy (pBfr, &g_devDescr, length, pActLen);
        break;

    case USB_DESCR_OTHER_SPEED_CONFIGURATION:
        #if(MS_USB_HIGH_SPEED == 0)
            return(ERROR);
        #endif
    case USB_DESCR_CONFIGURATION:

        /* 
         * copy configuration, interface, and endpoint descriptors
         * to pBfr and set pActLen 
         */

        usbDbgPrint("descriptorGet: USB_DESCR_CONFIGURATION\n");

        memcpy (bfr, &g_configDescr, USB_CONFIG_DESCR_LEN);

        memcpy (&bfr[USB_CONFIG_DESCR_LEN], &g_ifDescr,
                USB_INTERFACE_DESCR_LEN);
        memcpy (&bfr[USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN],
                &g_bulkInEpDescr, USB_ENDPOINT_DESCR_LEN);

        memcpy (&bfr[USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN +
                     USB_ENDPOINT_DESCR_LEN],
                &g_bulkOutEpDescr, USB_ENDPOINT_DESCR_LEN);

        actLen = min (length, USB_CONFIG_DESCR_LEN +
                      USB_INTERFACE_DESCR_LEN + 2*USB_ENDPOINT_DESCR_LEN);

        memcpy (pBfr, bfr, actLen);
        *pActLen = actLen;

        break;

#if(0) 
    case USB_DESCR_INTERFACE:

        /* copy interface descriptor to pBfr and set pActLen */

        usbDescrCopy (pBfr, &g_ifDescr, length, pActLen);
        break;

    case USB_DESCR_ENDPOINT:

        /* copy endpoint descriptor to pBfr and set pActLen */

        usbDescrCopy (pBfr, &g_ifDescr, length, pActLen);
        break;
#endif

    case USB_DESCR_STRING:
        switch(descriptorIndex)
        {
        case 0:

            /* copy language descriptor to pBfr and set pActLen */

            usbDescrCopy (pBfr, &g_langDescr, length, pActLen);
            break;

        case ID_STR_MFG:
            usbDescrStrCopy (pBfr, g_pStrMfg, length, pActLen);
            break;

        case ID_STR_PROD:
            usbDescrStrCopy (pBfr, g_pStrProd, length, pActLen);
            break;

        /*
         * test other cases based on values set for configuration,
         * interface, and endpoint descriptors 
         */

        default:
            return(ERROR);
        }
        break;

    case USB_DESCR_DEVICE_QUALIFIER: /* DEVICE_QUALIFIER */

        /* copy device qualifier descriptor to pBfr and set pActLen */

#if(MS_USB_HIGH_SPEED == 1)
            g_usbDevQualDescr.maxPacketSize0 = g_devDescr.maxPacketSize0;
            usbDescrCopy (pBfr, &g_usbDevQualDescr, length, pActLen);
            break;
#else
            return(ERROR);
#endif

    case USB_DESCR_INTERFACE_POWER: /* INTERFACE_POWER */

        /* copy interface power descriptor to pBfr and set pActLen */

        return(ERROR);
    default:
        return(ERROR);
    }

    return(OK);
    }

/*******************************************************************************
*
* configurationGet - get the specified configuration
*
* This function is used to get the current configuration of the device.
* <pConfiguration> is set with the current configuration and sent to the
* host.
*
* RETURNS: OK, or ERROR if unable to return configuration setting
*
* ERRNO:
*  none
*/

LOCAL STATUS configurationGet
    (
    pVOID		param,		/*  TCD specific parameter */		
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    pUINT8		pConfiguration	/* configuration value */
    )
    {
    if (targChannel != g_targChannel) /* only one target */
        return(ERROR);

    /* This request is not accepted when the device is in the default state */

    if (g_deviceAddr == 0)
        return ERROR;

   *pConfiguration = g_configuration;

    return(OK);
    }

/*******************************************************************************
*
* configurationSet - set the specified configuration
*
* This function is used to set the current configuration to the configuration
* value sent by host. <configuration> consists of the value to set.
*
* RETURNS: OK, or ERROR if unable to set specified configuration
*
* ERRNO:
*  none.
*/

LOCAL STATUS configurationSet
    (
    pVOID		param,		/*  TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */	
    UINT8		configuration	/* configuration value to set */
    )
    {
    USB_BULK_CBW	* pCbw;		/* Command Block Wrapper */
    UINT8		* pData;	
    UINT32		size;

    if (targChannel != g_targChannel) 
        return(ERROR);

    /*
     * This request is invalid if received in a default state
     * or the configuration value is not expected
     */

    if ((g_deviceAddr == 0) || (configuration > MS_CONFIG_VALUE))
        return ERROR;

    /* set current configuration global static variable */

    if (configuration == MS_CONFIG_VALUE)
        {
        usbDbgPrint("configurationSet: configuration = MS_CONFIG_VALUE\n"); 
        g_configuration = configuration;

        if (g_bulkInPipeHandle == NULL)
            {

#ifdef USB1_1

            /* Create bulk-in pipe */

            if (usbTargPipeCreate (targChannel, MS_BULK_IN_ENDPOINT_ID, 0,
		                   MS_BULK_IN_ENDPOINT_NUM, configuration,
                                   MS_INTERFACE_NUM, USB_XFRTYPE_BULK,
                                   USB_DIR_IN, &g_bulkInPipeHandle) != OK)
#else
            if (usbTargPipeCreate (targChannel, &g_bulkInEpDescr, configuration,
                                   MS_INTERFACE_NUM, g_ifAltSetting,
                                   &g_bulkInPipeHandle) != OK)
#endif
                return ERROR;
            }

        if (g_bulkOutPipeHandle == NULL)
            {
#ifdef USB1_1

            /* Create bulk-out pipe */

            if (usbTargPipeCreate (targChannel, MS_BULK_OUT_ENDPOINT_ID, 0,
                                   MS_BULK_OUT_ENDPOINT_NUM, configuration,
                                   MS_INTERFACE_NUM, USB_XFRTYPE_BULK,
                                   USB_DIR_OUT, &g_bulkOutPipeHandle) != OK)
#else
            if (usbTargPipeCreate (targChannel, &g_bulkOutEpDescr, configuration,
                                   MS_INTERFACE_NUM, g_ifAltSetting,
                                   &g_bulkOutPipeHandle) != OK)
#endif

                return ERROR;

#ifndef USE_MS_TEST_DATA

            /* Initialize ERP to listen for data */
            /* on reset setup to receive a new CBW */

            pCbw    = (USB_BULK_CBW *)usbMsCBWInit();
            pData   = (UINT8 *)pCbw;
            size    = sizeof(USB_BULK_CBW);

            if (usbMsDevInit() != OK)
                return(ERROR);

            if (usbMsBulkOutErpInit(pData, size,
                                    bulkOutErpCallbackCBW, NULL) != OK)
                return(ERROR);
#endif
	        }
        usbDbgPrint("configurationSet: Exiting...\n"); 
        }
    else if (configuration == 0)
        {
        usbDbgPrint("configurationSet: configuration = 0\n"); 

        g_configuration = configuration;

        if (g_bulkInPipeHandle != NULL)
            {
            usbTargPipeDestroy (g_bulkInPipeHandle);
            g_bulkInPipeHandle = NULL;
            }

        if (g_bulkOutPipeHandle != NULL)
            {
            usbTargPipeDestroy (g_bulkOutPipeHandle);
            g_bulkOutPipeHandle = NULL;
            }
        }
    else
        return(ERROR);


    return(OK);
    }

/*******************************************************************************
*
* interfaceGet - get the specified interface
*
* This function is used to get the selected alternate setting of the
* specified interface.
*
* RETURNS: OK, or ERROR if unable to return interface setting
*
* ERRNO:
*  none
*/

LOCAL STATUS interfaceGet
    (
    pVOID		param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT16		interfaceIndex,		/* interface index */
    pUINT8		pAlternateSetting	/* alternate setting */
    )
    {

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    if ((g_deviceAddr == 0) ||(g_configuration == 0))
        return ERROR;

    *pAlternateSetting = g_ifAltSetting;

    return(OK);
    }


/*******************************************************************************
*
* interfaceSet - set the specified interface
*
* This function is used to select the alternate setting of he specified
* interface.
*
* RETURNS: OK, or ERROR if unable to set specified interface
*
* ERRNO:
*  none.
*/

LOCAL STATUS interfaceSet
    (
    pVOID		param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT16		interfaceIndex,		/* interface index */
    UINT8		alternateSetting	/* alternate setting */
    )
    {

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    /*
     * This is an invalid request if the device is in default/addressed state
     * or if the alternate setting value does not match
     */

    if ((g_deviceAddr == 0) || (g_configuration == 0))
        return ERROR;

    if (alternateSetting == MS_INTERFACE_ALT_SETTING)
        {
        g_ifAltSetting = alternateSetting;
        return OK;
        }
    else
        return(ERROR);
    }


/*******************************************************************************
*
* vendorSpecific - invoke the VENDOR_SPECIFIC request
*
* This routine implements the vendor specific standard device request
*
* RETURNS: OK, or ERROR if unable to process vendor-specific request
*
* ERRNO:
*  none
*/

LOCAL STATUS vendorSpecific
    (
    pVOID		param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT8		requestType,		/* request type */
    UINT8		request,		/* request name */
    UINT16		value,			/* wValue */	
    UINT16		index,			/* wIndex */
    UINT16		length			/* wLength */
    )
    {
    STATUS		retVal = ERROR;
    LOCAL UINT8		maxLun = 0x0;		/* device has no LUNs */
    USB_BULK_CBW	* pCbw;			/* CBW */
    UINT8		* pData;
    UINT32		size;

    /* only one target */

    if (targChannel != g_targChannel) 
        return(ERROR);

    if (requestType == (USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_INTERFACE))
        {

        /* mass storage reset */

        if (request == USB_BULK_RESET)  
            {
            usbDbgPrint("vendorSpecific: Mass Storage reset request\n");

            /* if the bulk in pipe is in use, try to clear it */

            if (g_bulkInInUse == TRUE)
                if(usbTargTransferAbort(g_bulkInPipeHandle, &g_bulkInErp) != OK)
                    return(ERROR);

            /* if the bulk out pipe is in use, try to clear it */

            if (g_bulkOutInUse == TRUE)
               if(usbTargTransferAbort(g_bulkOutPipeHandle, &g_bulkOutErp) != OK)
                   return(ERROR);

            /* setup to receive a new CBW */

            pCbw    = usbMsCBWInit();
            pData   = (UINT8 *)pCbw;
            size    = sizeof(USB_BULK_CBW);

        if (usbMsBulkOutErpInit(pData, size, bulkOutErpCallbackCBW, NULL) == OK)
            {
            retVal = usbTargControlStatusSend (targChannel);    
            }
        else
            return(ERROR);
        }
    else
        return(ERROR);
    }
    else if (requestType == (USB_RT_DEV_TO_HOST | USB_RT_CLASS |
                             USB_RT_INTERFACE))
        {

        /* getMaxLUN */ 

        if (request == USB_BULK_GET_MAX_LUN) 
            {
            usbDbgPrint("vendorSpecific: Get Max LUN request\n");
            retVal = usbTargControlResponseSend(targChannel, 1, &maxLun);       
            }
        else
            return(ERROR);
        }

#ifdef USE_MS_TEST_DATA
    else if (requestType == (USB_RT_DEV_TO_HOST | USB_RT_VENDOR |
                             USB_RT_INTERFACE))
        {
        if (request == MS_BULK_IN_TX_TEST)
            {
            usbDbgPrint("vendorSpecific: sending test data to host\n");

            {
            int i;
            for (i = 0; i < MS_TEST_DATA_SIZE; i++)
                g_usbMsTestData[i] = i;
            }

            if (usbMsBulkInErpInit(g_usbMsTestData, MS_TEST_DATA_SIZE,
                                   usbMsTestTxCallback,NULL) == OK)
                {
                retVal = usbTargControlResponseSend(targChannel, 0, NULL);
                }
            else
                return(ERROR);
            }
        else
            return(ERROR);
        }
    else if (requestType == (USB_RT_VENDOR | USB_RT_INTERFACE))
        {
        if (request == MS_BULK_OUT_RX_TEST)
            {

            usbDbgPrint("vendorSpecific: receiving test data from host\n");

            memset (&g_usbMsTestData, 0, MS_TEST_DATA_SIZE);

            if (usbMsBulkOutErpInit(g_usbMsTestData, MS_TEST_DATA_SIZE,
                                    usbMsTestRxCallback,NULL) == OK)
                {
                retVal = usbTargControlResponseSend(targChannel, 0, NULL);          
                }
            else
                return(ERROR);
            }
        else
            return(ERROR);
        }
#endif
    else
        return(ERROR);

    return(retVal);
    }

#ifdef USB1_1
/****************************************************************************
*
* mngmtFunc - invoke the connection management function
*
* This function handles various management related events. <mngmtCode>
* consist of the management event function code that is reported by the
* TargLib layer. <pContext> is the argument sent for the management event to
* be handled.
*
* RETURNS: OK if able to handle event, or ERROR if unable to handle event
*
* ERRNO:
*  none.
*/

LOCAL STATUS mngmtFunc
    (
    pVOID		param,			/* TCD Specic parameter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT16		mngmtCode		/* management code */
    )

    {
    switch (mngmtCode)
	{
	case TCD_MNGMT_ATTACH:

            /* Initialize local static data */

            g_targChannel = targChannel;
            usbTargEndpointInfoGet(targChannel, &g_numEndpoints, &g_pEndpoints);

            g_configuration     = 0;
            g_ifAltSetting      = 0;
            g_bulkOutPipeHandle = NULL;
            g_bulkInPipeHandle  = NULL;

            /* Initialize control pipe maxPacketSize. */

            g_devDescr.maxPacketSize0 = g_pEndpoints[0].maxPacketSize;

            /* Initialize bulk endpoint max packet size. */

            g_bulkOutEpDescr.maxPacketSize =
            g_pEndpoints [MS_BULK_OUT_ENDPOINT_ID].bulkOutMaxPacketSize;

            g_bulkInEpDescr.maxPacketSize =
            g_pEndpoints [MS_BULK_IN_ENDPOINT_ID].bulkInMaxPacketSize;

            break;

        case TCD_MNGMT_DETACH:

            break;

        case TCD_MNGMT_BUS_RESET:

            usbDbgPrint("mngmtFunc: Bus Reset\n");

        case TCD_MNGMT_VBUS_LOST:

            /* revert to power-ON configuration */

            configurationSet (param, targChannel, 0);
            break;

        default:
            break;
        }

    return OK;
    }
#else
/*******************************************************************************
*
* mngmtFunc - invoke the connection management function
*
* This function handles various management related events. <mngmtCode>
* consist of the management event function code that is reported by the
* TargLib layer. <pContext> is the argument sent for the management event to
* be handled.
*
* RETURNS: OK if able to handle event, or ERROR if unable to handle event
*
* ERRNO:
*  none.
*/

LOCAL STATUS mngmtFunc
    (
    pVOID		param,			/* TCD specific paramter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT16		mngmtCode,		/* management code */
    pVOID		pContext		/* Context value */
    )
    {
    pUSB_APPLN_DEVICE_INFO	pDeviceInfo = NULL;/* USB_APPLN_DEVICE INFO */
    switch (mngmtCode)
    	{
    	case TARG_MNGMT_ATTACH:
            if (pContext == NULL)
                return ERROR;

            g_targChannel       = targChannel;
            g_configuration     = 0;
            g_ifAltSetting      = 0;
            g_bulkOutPipeHandle = NULL;
            g_bulkInPipeHandle  = NULL;

            /* Retrieve the pointer to the device info data structure */

            pDeviceInfo = (pUSB_APPLN_DEVICE_INFO)pContext;

    	    /* Initialize global data */

            g_uDeviceFeature = pDeviceInfo->uDeviceFeature;
            g_uEndpointNumberBitmap = pDeviceInfo->uEndpointNumberBitmap;

            /*
             * If the device is USB 2.0, initialize the bcdUSB field of
             * the device descriptor and the device qualifier descriptor.
             */

            if ((g_uDeviceFeature & USB_FEATURE_USB20) != 0)
                {
                g_devDescr.bcdUsb = TO_LITTLEW(MS_USB_HIGH_SPEED_VERSION);
                g_usbDevQualDescr.bcdUsb = TO_LITTLEW(MS_USB_HIGH_SPEED_VERSION);
                }
            else
                {
                g_devDescr.bcdUsb = TO_LITTLEW(MS_USB_FULL_SPEED_VERSION);
                g_usbDevQualDescr.bcdUsb = TO_LITTLEW(MS_USB_FULL_SPEED_VERSION);
                }

            /*
             * If the device supports remote wakeup, modify the configuration
             * descriptor accordingly.
             */

            if ((g_uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                g_configDescr.attributes |= USB_ATTR_REMOTE_WAKEUP;

            /*
             * Check if the endpoint number is supported.
             * The shift value is directly taken as the endpoint address
             * as the application is specifically written for keyboard and it
             * supports only the interrupt IN endpoint.
             */

            if ((g_uEndpointNumberBitmap >>
                (16 + (USB_ENDPOINT_MASK & g_bulkInEpDescr.endpointAddress))) == 0)
                {

                /* Search through the bitmap and arrive at an endpoint address */

                UINT32 uIndex = 1;

                for (uIndex = 16; uIndex < 32; uIndex++)
                    {
                    if ((g_uEndpointNumberBitmap >> uIndex) != 0)
                        {
                        g_bulkInEpDescr.endpointAddress = USB_ENDPOINT_IN | (uIndex - 16);
                        break;
                        }
                    }
                if (uIndex == 32)
                    return ERROR;
                }

            if ((g_uEndpointNumberBitmap >> g_bulkOutEpDescr.endpointAddress) == 0)
                {

                /* Search through the bitmap and arrive at an endpoint address */

                UINT32 uIndex = 1;

                for (uIndex = 1; uIndex < 16; uIndex++)
                    {
                    if ((g_uEndpointNumberBitmap >> uIndex) != 0)
                        {
                        g_bulkOutEpDescr.endpointAddress  = uIndex;
                        break;
                        }
                    }
                if (uIndex == 16)
                    return ERROR;
                }

    	    break;

    	case TARG_MNGMT_DETACH:

            /* Reset the globals */

            g_targChannel = 0;
            g_uDeviceFeature = 0;
            g_uEndpointNumberBitmap = 0;

            /* Reset the device and device qualifier descriptors' bcusb field */

            g_devDescr.bcdUsb = TO_LITTLEW(MS_USB_VERSION);
            g_usbDevQualDescr.bcdUsb = TO_LITTLEW(MS_USB_VERSION);

            g_bulkInEpDescr.endpointAddress = MS_BULK_IN_ENDPOINT_NUM;
            g_bulkOutEpDescr.endpointAddress = MS_BULK_OUT_ENDPOINT_NUM;

    	    break;

    	case TARG_MNGMT_BUS_RESET:

            /* Copy the operating speed of the device */

    	    g_uSpeed = (UINT32)pContext;
    	    g_deviceAddr = 0;

#if 1
            g_configuration     = 0;
            g_ifAltSetting      = 0;
#endif
    	    if (g_bulkInPipeHandle != NULL)
                {
                usbTargPipeDestroy (g_bulkInPipeHandle);
                g_bulkInPipeHandle = NULL;
                }

            if (g_bulkOutPipeHandle != NULL)
                {
                usbTargPipeDestroy (g_bulkOutPipeHandle);
                g_bulkOutPipeHandle = NULL;
                }

            /* Reset the device status to indicate that it is self powered */

            g_uDeviceStatus = 0x01;

            if (g_uSpeed == USB_TCD_HIGH_SPEED)
                {
                g_devDescr.maxPacketSize0 = 
                                      MS_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE;
                g_usbDevQualDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;

               /* Update the max packet size for bulk in and out endpoints */

                g_bulkOutEpDescr.maxPacketSize = 
                                       TO_LITTLEW(USB_MAX_HIGH_SPEED_BULK_SIZE);
                g_bulkInEpDescr.maxPacketSize = 
                                       TO_LITTLEW(USB_MAX_HIGH_SPEED_BULK_SIZE);
                }

            break;

    	case TARG_MNGMT_DISCONNECT:
            configurationSet (param, targChannel, 0);
     	    break;

        case TARG_MNGMT_SUSPEND:
        case TARG_MNGMT_RESUME:
    	default:
    	    break;
    	}

    return OK;
    }

#endif

/*******************************************************************************
*
* usbTargMsCallbackInfo - returns usbTargPrnLib callback table
* 
* This function returns the callback table pointer .
*
* RETURNS: N/A
*
* ERRNO:
*  none
*/

VOID usbTargMsCallbackInfo
    (
    struct usbTargCallbackTable ** ppCallbacks,	/*USB_TARG_CALLBACK_TABLE */
    pVOID			* pCallbackParam /* Callback Parameter */
    )
    {
    if (ppCallbacks != NULL)
        *ppCallbacks = &usbTargMsCallbackTable;

    if (pCallbackParam != NULL)
        *pCallbackParam = NULL;
    }

/*******************************************************************************
*
* usbMsBulkInErpInit - initialize the bulk-in ERP
*
* This function initializes the Bulk In ERP.
*
* RETURNS: OK, or ERROR if unable to submit ERP.
*
* ERRNO:
*  none 
*/

STATUS usbMsBulkInErpInit
    (
    UINT8		* pData,	/* pointer to data */
    UINT32		size,		/* size of data */
    ERP_CALLBACK	erpCallback,	/* erp callback */
    pVOID		usrPtr		/* user pointer */
    )
    {
    if (pData == NULL)
        return ERROR;

    if (g_bulkInInUse)
        return ERROR;

    memset (&g_bulkInErp, 0, sizeof (USB_ERP));

    g_bulkInErp.erpLen = sizeof (USB_ERP);
    g_bulkInErp.userCallback = erpCallback;
    g_bulkInErp.bfrCount = 1;
    g_bulkInErp.userPtr = usrPtr;

    g_bulkInErp.bfrList [0].pid = USB_PID_IN;
    g_bulkInErp.bfrList [0].pBfr = pData;
    g_bulkInErp.bfrList [0].bfrLen = size;

    g_bulkInInUse = TRUE;
    g_bulkInBfrValid = FALSE;

    if (usbTargTransfer (g_bulkInPipeHandle, &g_bulkInErp) != OK)
        {
        g_bulkInInUse = FALSE;
        return ERROR;
        }

    return OK;
    }


/***************************************************************************
*
* usbMsBulkOutErpInit - initialize the bulk-Out ERP
*
* This function initializes the bulk Out ERP.
*
* RETURNS: OK, or ERROR if unable to submit ERP.
*
* ERRNO: N/A
*/

STATUS usbMsBulkOutErpInit 
    (
    UINT8           * pData,		/* pointer to buffer */
    UINT32          size,		/* size of data */ 
    ERP_CALLBACK    erpCallback,	/* IRP_CALLBACK */
    pVOID           usrPtr		/* user pointer */
    )
    {
    if (pData == NULL)
        return ERROR;

    if (g_bulkOutInUse)
	    return ERROR;

    /* Initialize bulk ERP */

    memset (&g_bulkOutErp, 0, sizeof (USB_ERP));

    g_bulkOutErp.erpLen = sizeof (USB_ERP);
    g_bulkOutErp.userCallback = erpCallback;
    g_bulkOutErp.bfrCount = 1;
    g_bulkOutErp.userPtr = usrPtr;

    g_bulkOutErp.bfrList [0].pid = USB_PID_OUT;
    g_bulkOutErp.bfrList [0].pBfr = pData;
    g_bulkOutErp.bfrList [0].bfrLen = size;

    g_bulkOutInUse = TRUE;
    g_bulkOutBfrValid = FALSE;

    if (usbTargTransfer (g_bulkOutPipeHandle, &g_bulkOutErp) != OK)
        {
	g_bulkOutInUse = FALSE;
	return ERROR;
	}

    return OK;
    }

/*******************************************************************************
*
* usbMsIsConfigured - test if the device is configured
*
* This function checks whether the device is configured or not.
*
* RETURNS: TRUE or FALSE
*
* ERRNO:
*  none
*/

BOOL usbMsIsConfigured (void)
    {
    BOOL	retVal;

    retVal = (g_configuration == MS_CONFIG_VALUE)?TRUE:FALSE;

    return(retVal);
    }

/*******************************************************************************
*
* usbMsBulkInErpInUseFlagGet - get the Bulk-in ERP inuse flag
*
* This function is used to get the state of the Bulk-In ERP.
*
* RETURNS: TRUE or FALSE
*
* ERRNO:
*  none
*/

BOOL usbMsBulkInErpInUseFlagGet (void)
    {
    BOOL retVal = g_bulkInInUse;
    return(retVal);
    }

/*******************************************************************************
*
* usbMsBulkOutErpInUseFlagGet - get the Bulk-Out ERP inuse flag
*
* This function is used to get the state of the Bulk-OUT ERP.
*
* RETURNS: OK, or ERROR if unable to submit ERP.
*
* ERRNO:
*  none
*/

BOOL usbMsBulkOutErpInUseFlagGet (void)
    {
    BOOL	retVal = g_bulkOutInUse;
    return(retVal);
    }

/*******************************************************************************
*
* usbMsBulkInErpInUseFlagSet - set the Bulk-In ERP inuse flag
*
* This function is used to set the state of Bulk - IN ERP flag. <state> is the
* state to set.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*/
void usbMsBulkInErpInUseFlagSet
    (
    BOOL state
    )
    {
    g_bulkInInUse = state;
    return;
    }


/*******************************************************************************
*
* usbMsBulkOutErpInUseFlagSet - set the Bulk-Out ERP inuse flag
*
* This function is used to set the state of Bulk - OUT ERP flag. <state> is the
* state to set.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*/
void usbMsBulkOutErpInUseFlagSet
    (
    BOOL	state		/* State to set */
    )
    {
    g_bulkOutInUse = state;
    return;
    }

#ifdef USE_MS_TEST_DATA
/*******************************************************************************
*
* usbMsTestTxCallback - invoked after test data transmitted
*
* This function is invoked after the Bulk IN test data is transmitted. It sets
* the bulk IN flag to <false>.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*/
void usbMsTestTxCallback
    (
    pVOID	p
    )
    {
    g_bulkInInUse = FALSE;
    return;
    }


/***************************************************************************
*
* usbMsTestRxCallback - invoked after test data is received
*
* This function is invoked after the Bulk OUT test data is transmitted. It sets
* the bulk OUT flag to <false>.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/
void usbMsTestRxCallback
    (
    pVOID	p
    )
    {
    g_bulkOutInUse = FALSE;
    return;
    }
#endif

#ifdef USB_DEBUG_PRINT
/***************************************************************************
*
* usbDbgPrintOn - switch on debug printing
*
* This function is used to switch on the debug printing. This function is
* used only for degugging purpose.
*
* RETURNS: N/A
*
* ERRNO: N/A
*
*\NOMANUAL
*/

void usbDbgPrintOn (void)
    {
    g_usbDbgPrint = TRUE;
    return;
    }


/***************************************************************************
*
* usbDbgPrintOff - switch off debug printing
*
* This function is used to switch off the debug printing. 
*
* RETURNS: N/A
*
* ERRNO: N/A
*
*\NOMANUAL
*/

void usbDbgPrintOff
    (
    void
    )
    {
    g_usbDbgPrint = FALSE;
    return;
    }


/***************************************************************************
*
* usbDbgPrint - print the formatted debug statement
*
* This function implements the code for printign the debug messages
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
*\NOMANUAL
*/

void usbDbgPrint
    (
    char *fmt,
    ...
    )
    {
    va_list	ap;
    char	* p;
    char	* pBuf;
    int		val =0;
    int		cnt = 0;

    if (g_usbDbgPrint == FALSE)
        return;

    va_start(ap, fmt);
    memset(&g_buf[0],0,BUF_SIZE);

    pBuf = &g_buf[0];

    for (p = fmt; *p != '\0'; p++)
        {
        if (*p != '%')
            {
            if (cnt < BUF_SIZE)
                {
                *pBuf++ = *p;
                cnt++;
                continue;
                }
            else
                goto EXIT;
            }

        switch(*++p)
            {
            case 'd':
            case 'x':
                val = va_arg(ap, int);
                if ((cnt + 2*sizeof(int)) < BUF_SIZE)
                    {
                    sprintf(pBuf,"%x",val);
                    while(*pBuf != '\0')
                        {
                        pBuf++;
                        cnt++;
                        }
                    }
                else
                    goto EXIT;
                break;
            default:
                break;
            }
        }

EXIT:
    va_end(ap);
    DEBUG_PRINT(g_buf);
    return;
    }

/*******************************************************************************
*
* usbMsTargError - print the error message
*
* This function is called whenver any error occurs. It is used to print
* appropiate error messaged dependign on the error status set. 
*
* RETURNS: N/A
*
* ERRNO: N/A
*
*\NOMANUAL
*/

void usbMsTargError (void)
    {

    int	error = errnoGet();

    switch(error)
        {
        case S_usbTcdLib_BAD_PARAM:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_BAD_PARAM\n");
            break;
        case S_usbTcdLib_BAD_HANDLE:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_BAD_HANDLE\n");
            break;
        case S_usbTcdLib_OUT_OF_MEMORY:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_OUT_OF_MEMORY\n");
            break;
        case S_usbTcdLib_OUT_OF_RESOURCES: 
            usbDbgPrint("usbMsTargError: S_usbTcdLib_OUT_OF_RESOURCES\n");
            break;
        case S_usbTcdLib_NOT_IMPLEMENTED:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_NOT_IMPLEMENTED\n");
            break;
        case S_usbTcdLib_GENERAL_FAULT:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_GENERAL_FAULT\n");
            break;
        case S_usbTcdLib_NOT_INITIALIZED:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_NOT_INITIALIZED\n");
            break;
        case S_usbTcdLib_INT_HOOK_FAILED:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_INT_HOOK_FAILED\n");
            break;
        case S_usbTcdLib_HW_NOT_READY:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_HW_NOT_READY\n");
            break;
        case S_usbTcdLib_NOT_SUPPORTED:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_NOT_SUPPORTED\n");
            break;
        case S_usbTcdLib_ERP_CANCELED:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_ERP_CANCELED\n");
            break;
        case S_usbTcdLib_CANNOT_CANCEL:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_CANNOT_CANCEL\n");
            break;
        case S_usbTcdLib_SHUTDOWN:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_SHUTDOWN\n");
            break;
        case S_usbTcdLib_DATA_TOGGLE_FAULT:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_DATA_TOGGLE_FAULT\n");
            break;
        case S_usbTcdLib_PID_MISMATCH:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_PID_MISMATCH\n");
            break;
        case S_usbTcdLib_COMM_FAULT:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_COMM_FAULT\n");
            break;
        case S_usbTcdLib_NEW_SETUP_PACKET:
            usbDbgPrint("usbMsTargError: S_usbTcdLib_NEW_SETUP_PACKET\n");
            break;
        default:
            usbDbgPrint("usbMsTargError: Unknown error\n");
            break;
        }
    return;
    }
#endif


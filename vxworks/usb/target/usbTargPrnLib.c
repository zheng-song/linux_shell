/* usbTargPrnLib.c - USB printer target exerciser/demonstration */

/* Copyright 1999-2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
02e,02aug04,mta  Modification History Changes
02d,23jul04,ami  Apigen Changes
02c,12jul04,ami  Coding Convention Changes
02b,04may04,pdg  Support for performance calculation
02a,02apr04,pdg  modifications for USB 2.0 stack.
01b,23nov99,rcb  Change #include ../xxx references to lower case.
01a,30aug99,rcb  First.
*/

/*
DESCRIPTION

This module contains code to exercise the usbTargLib by emulating a rudimentary
USB printer.  This module will generally be invoked by usbTool or a similar USB
test/exerciser application.

It is the caller's responsibility to initialize usbTargLib and attach a USB TCD 
to it.	When attaching a TCD to usbTargLib, the caller must pass a pointer to a
table of callbacks required by usbTargLib.  The address of this table and the
"callback parameter" required by these callbacks may be obtained by calling
usbTargPrnCallbackInfo().  It is not necessary to initialize the usbTartPrnLib 
or to shut it down.  It performs all of its operations in response to callbacks 
from usbTargLib.

This module also exports a function, usbTargPrnBfrInfo(), which allows a test
application to retrieve the current status of the bulk output buffer.

INCLUDE FILES: usb/usbPlatform.h, string.h, usb/usb.h, usb/usbPrinter.h,
               usb/usbDescrCopyLib.h, usb/target/usbTargLib.h,
               drv/usb/target/usbTargPrnLib.h, usb/target/usbHalCommon.h
*/


/* includes */

#include "usb/usbPlatform.h"                
#include "string.h"                         
#include "usb/usb.h"                        
#include "usb/usbPrinter.h"		            
#include "usb/usbDescrCopyLib.h"
#include "usb/target/usbTargLib.h"
#include "drv/usb/target/usbTargPrnLib.h"
#include "usb/target/usbHalCommon.h"        

/* defines */

/* Define USE_DMA_ENDPOINT to direct printer data to the Philips PDIUSBD12
 * "main" endpoint (#2) which uses DMA.  Un-define USE_DMA_ENDPOINT to direct
 * printer data to the generic endpoint (#1) which uses programmed-IO.
 */

#if 0
#define USE_DMA_ENDPOINT
#endif


/* string identifiers */

#define UNICODE_ENGLISH		0x409                  /* unicode coding */

#define ID_STR_MFG		1                      /* manufacuturer id */
#define ID_STR_MFG_VAL		"Wind River Systems"   /* manufacturer name */

#define ID_STR_PROD		2                      /* product id */
#define ID_STR_PROD_VAL		"USB printer emulator" /* product name */

#define PRN_CAPS	"mfg:WRS;model=emulator;" /* printer */
						  /* "capabilities" string */

#define PRN_USB10_VERSION	0x0100		/* full speed version */
#define PRN_USB20_VERSION	0x0200		/* high speed version */
#define PRN_NUM_CONFIG		1		/* number of configurations */


/* printer configuration */

#define PRN_CONFIG_VALUE	1	/* Printer configuration value */
#define PRN_NUM_INTERFACES	1	/* number of interface */

/* printer interface */

#define PRN_INTERFACE_NUM		0	/* interface number */
#define PRN_INTERFACE_ALT_SETTING	0	/* alternate setting */
#define PRN_NUM_ENDPOINTS		1	/* number of endpoints */


/* printer BULK OUT endpoint */

#ifdef	USE_DMA_ENDPOINT
#define PRN_BULK_OUT_ENDPOINT_NUM	0x02    /* dma endpoint number */
#define PRN_BULK_OUT_MAX_PACKETSIZE	64      /* max packet size */
#else
#define PRN_BULK_OUT_ENDPOINT_NUM	0x01    /* PIO mode endpoint number */
#define PRN_BULK_OUT_MAX_PACKETSIZE	16      /* max packet size-FS */
#endif

#define BULK_BFR_LEN			4096    /* total bulk buffer length */

#define PRN_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE  0x40 /* maximum packet size for
                                                      * the default control
                                                      * endpoint, if the device
                                                      * is operating at high
                                                      * speed
                                                      */

LOCAL USB_TARG_CHANNEL	channel;		/* target channel */
LOCAL UINT8		curConfiguration;	/* curr config */
LOCAL UINT8		curAlternateSetting;	/* current alternate setting */
LOCAL USB_TARG_PIPE	bulkPipeHandle;		/* bulk pipe handle */
LOCAL USB_ERP		bulkErp;		/* bulk ERP */
LOCAL BOOL		bulkInUse;		/* states whether bulk ERP */ 
						/* is in use. */
LOCAL pUINT8		bulkBfr;		/* buffer for bulk data */
LOCAL BOOL		bulkBfrValid;
LOCAL UINT32		uDeviceFeature = 0;	/* device feature */
LOCAL UINT32		uEndpointNumberBitmap = 0; /* bitmaps for the enpoints*/
                                                   /* supported by Target */
                                                   /* Controller */
LOCAL UINT32		uSpeed = USB_TCD_FULL_SPEED;  /* USB Speed */
LOCAL UINT8		uDeviceStatus = 0x01;	/* status of the device */
LOCAL UINT8		uDeviceAddress = 0;	/* Device Address */

LOCAL char capsString [] = PRN_CAPS;

/* Printer Port Status */
LOCAL USB_PRINTER_PORT_STATUS portStatus = {USB_PRN_STS_SELECTED |
                                            USB_PRN_STS_NOT_ERROR};

/* Language Descriptor */
LOCAL USB_LANGUAGE_DESCR langDescr = {sizeof (USB_LANGUAGE_DESCR),
                                      USB_DESCR_STRING,
	                              {TO_LITTLEW (UNICODE_ENGLISH)}};

LOCAL char	*pStrMfg = ID_STR_MFG_VAL;    /* Manufacturer Name */
LOCAL char	*pStrProd = ID_STR_PROD_VAL;  /* Product Name */

LOCAL USB_DEVICE_DESCR devDescr =	/* Device Descriptor */
    {
    USB_DEVICE_DESCR_LEN,		/* bLength */
    USB_DESCR_DEVICE,			/* bDescriptorType */
    TO_LITTLEW (PRN_USB10_VERSION),	/* bcdUsb */
    0,					/* bDeviceClass */
    0,					/* bDeviceSubclass */
    0,					/* bDeviceProtocol */
    USB_MIN_CTRL_PACKET_SIZE,		/* maxPacketSize0 */
    0,					/* idVendor */
    0,					/* idProduct */
    0,					/* bcdDevice */
    ID_STR_MFG,				/* iManufacturer */
    ID_STR_PROD,			/* iProduct */
    0,					/* iSerialNumber */
    PRN_NUM_CONFIG			/* bNumConfigurations */
    };

LOCAL USB_DEVICE_QUALIFIER_DESCR devQualifierDescr =  /* Device Qualifier */
    {
    USB_DEVICE_QUALIFIER_DESCR_LEN,	/* bLength */
    USB_DESCR_DEVICE_QUALIFIER,		/* bDescriptorType */
    TO_LITTLEW (PRN_USB20_VERSION),	/* bcdUsb */
    0,					/* bDeviceClass */
    0,					/* bDeviceSubclass */
    0,					/* bDeviceProtocol */
    USB_MIN_CTRL_PACKET_SIZE,		/* maxPacketSize0 */
    PRN_NUM_CONFIG,			/* bNumConfigurations */
    0					/* Reserved */
    };

LOCAL USB_CONFIG_DESCR configDescr =	/* Configuration Descriptor */
    {
    USB_CONFIG_DESCR_LEN,	    /* bLength */
    USB_DESCR_CONFIGURATION,	    /* bDescriptorType */
    TO_LITTLEW (USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN +
    USB_ENDPOINT_DESCR_LEN),	    /* wTotalLength */
    PRN_NUM_INTERFACES, 	    /* bNumInterfaces */
    PRN_CONFIG_VALUE,		    /* bConfigurationValue */
    0,				    /* iConfiguration */
    USB_ATTR_SELF_POWERED,	    /* bmAttributes */
    0				    /* MaxPower */
    };

LOCAL USB_CONFIG_DESCR otherSpeedConfigDescr =	/* Other Speed */
						/* Configuration Descriptor */

    {
    USB_CONFIG_DESCR_LEN,		/* bLength */
    USB_DESCR_OTHER_SPEED_CONFIGURATION,	/* bDescriptorType */
    TO_LITTLEW (USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN +
    USB_ENDPOINT_DESCR_LEN),		/* wTotalLength */
    PRN_NUM_INTERFACES,			/* bNumInterfaces */
    PRN_CONFIG_VALUE,			/* bConfigurationValue */
    0,					/* iConfiguration */
    USB_ATTR_SELF_POWERED,		/* bmAttributes */
    0					/* MaxPower */
    };

LOCAL USB_INTERFACE_DESCR ifDescr =	/* Interface Descriptor */
    {
    USB_INTERFACE_DESCR_LEN,		/* bLength */
    USB_DESCR_INTERFACE,		/* bDescriptorType */
    PRN_INTERFACE_NUM,			/* bInterfaceNumber */
    PRN_INTERFACE_ALT_SETTING,		/* bAlternateSetting */
    PRN_NUM_ENDPOINTS,			/* bNumEndpoints */
    USB_CLASS_PRINTER,			/* bInterfaceClass */
    USB_SUBCLASS_PRINTER,		/* bInterfaceSubClass */
    USB_PROTOCOL_PRINTER_UNIDIR,	/* bInterfaceProtocol */
    0					/* iInterface */
    };

LOCAL USB_ENDPOINT_DESCR epDescr =	/* Endpoint Descriptor */
    {
    USB_ENDPOINT_DESCR_LEN,		/* bLength */
    USB_DESCR_ENDPOINT,			/* bDescriptorType */
    PRN_BULK_OUT_ENDPOINT_NUM,		/* bEndpointAddress */
    USB_ATTR_BULK,			/* bmAttributes */
    TO_LITTLEW(PRN_BULK_OUT_MAX_PACKETSIZE),	/* maxPacketSize */
    0					/* bInterval */
    };

LOCAL USB_ENDPOINT_DESCR otherSpeedEpDescr =    /* Other Speed Endpoint */
						/* Descriptor */
    {
    USB_ENDPOINT_DESCR_LEN,		/* bLength */
    USB_DESCR_ENDPOINT,			/* bDescriptorType */
    PRN_BULK_OUT_ENDPOINT_NUM,		/* bEndpointAddress */
    USB_ATTR_BULK,			/* bmAttributes */
    TO_LITTLEW(PRN_BULK_OUT_MAX_PACKETSIZE),	/* maxPacketSize */
    0					/* bInterval */
    };

/* forward declarations */

LOCAL STATUS mngmtFunc (pVOID param, USB_TARG_CHANNEL targChannel, 
                        UINT16 mngmtCode, pVOID pContext);

LOCAL STATUS featureClear (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT8 requestType, UINT16 feature,UINT16 index);

LOCAL STATUS featureSet (pVOID param, USB_TARG_CHANNEL targChannel,
                         UINT8 requestType, UINT16 feature, UINT16 index);

LOCAL STATUS configurationGet (pVOID param, USB_TARG_CHANNEL targChannel,
                               pUINT8 pConfiguration);

LOCAL STATUS configurationSet (pVOID param, USB_TARG_CHANNEL targChannel,
                               UINT8 configuration);

LOCAL STATUS descriptorGet (pVOID param, USB_TARG_CHANNEL targChannel,
                            UINT8 requestType, UINT8 descriptorType,
                            UINT8 descriptorIndex, UINT16 languageId,
                            UINT16 length, pUINT8 pBfr, pUINT16 pActLen);

LOCAL STATUS interfaceGet (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT16 interfaceIndex, pUINT8 pAlternateSetting);

LOCAL STATUS interfaceSet (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT16 interfaceIndex, UINT8 alternateSetting);

LOCAL STATUS statusGet (pVOID param, USB_TARG_CHANNEL targChannel,
                        UINT16 requestType, UINT16 index, UINT16 length,
                        pUINT8 pBfr);

LOCAL STATUS addressSet (pVOID param, USB_TARG_CHANNEL targChannel,
                         UINT16 deviceAddress);

LOCAL STATUS vendorSpecific (pVOID param, USB_TARG_CHANNEL targChannel,
                             UINT8 requestType, UINT8 request, UINT16 value,
                             UINT16 index, UINT16 length);

LOCAL STATUS initBulkOutErp (void);

LOCAL USB_TARG_CALLBACK_TABLE usbTargPrnCallbackTable =  /* Callback Table */
    {
    mngmtFunc,		/* mngmtFunc */
    featureClear,       /* featureClear */
    featureSet,		/* featureSet */
    configurationGet,	/* configurationGet */
    configurationSet,	/* configurationSet */
    descriptorGet,	/* descriptorGet */
    NULL,		/* descriptorSet */
    interfaceGet,	/* interfaceGet */
    interfaceSet,	/* interfaceSet */
    statusGet,		/* statusGet */
    addressSet,		/* addressSet */
    NULL,		/* synchFrameGet */
    vendorSpecific	/* vendorSpecific */
    };


/* functions */

/******************************************************************************
*
* bulkErpCallback - called when report ERP terminates
*
* This callback is invoked when an Endpoint Request Packet terminates.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL VOID bulkErpCallback
    (
    pVOID	p		/* pointer to ERP */
    )

    {
    bulkInUse = FALSE;
    bulkBfrValid = TRUE;
#ifdef TEST_PRINT_PERFORMANCE
    initBulkOutErp();
#endif
    }


/******************************************************************************
*
* initBulkOutErp - listen for output printer data
*
* This function initializes the Bulk Out ERP and listens to the data
* sent by the Printer.
*
* RETURNS: OK, or ERROR if unable to submit ERP.
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS initBulkOutErp (void)
    {
    if (bulkBfr == NULL)
	return ERROR;

    if (bulkInUse)
	return OK;

    /* Initialize bulk ERP */

    memset (&bulkErp, 0, sizeof (bulkErp));

    bulkErp.erpLen = sizeof (bulkErp);
    bulkErp.userCallback = bulkErpCallback;
    bulkErp.bfrCount = 1;

    bulkErp.bfrList [0].pid = USB_PID_OUT;
    bulkErp.bfrList [0].pBfr = bulkBfr;
    bulkErp.bfrList [0].bfrLen = BULK_BFR_LEN;

    bulkInUse = TRUE;
    bulkBfrValid = FALSE;

    if (usbTargTransfer (bulkPipeHandle, &bulkErp) != OK)
	{
	bulkInUse = FALSE;
	return ERROR;
	}

    return OK;
    }


/******************************************************************************
*
* usbTargPrnCallbackInfo - returns usbTargPrnLib callback table
*
* This function is called by the initialization routine. It returns the 
* information about the callback table.
*
* RETURNS: N/A
* 
* ERRNO:
*  none
*/

VOID usbTargPrnCallbackInfo
    (
    pUSB_TARG_CALLBACK_TABLE	*ppCallbacks,	/* Pointer to callback */
						/* table */
    pVOID	*pCallbackParam			/* target app-specific */
						/* parameter */
    )

    {
    if (ppCallbacks != NULL)
	*ppCallbacks = &usbTargPrnCallbackTable;

    if (pCallbackParam != NULL)
	*pCallbackParam = NULL;
    }

/*******************************************************************************
*
* usbTargPrnDataInfo - returns buffer status/info
*
* This function returns the status the bulk buffer which consist of the 
* data sent by the printer. <pActLen> will consist of the actual length of
* data to be printed.
*
* RETURNS: OK if buffer has valid data, else ERROR
*
* ERRNO:
*  none.
*/

STATUS usbTargPrnDataInfo
    (
    pUINT8	* ppBfr,	/* Pointer to the buffer address */
    pUINT16	pActLen		/* Actual length of the data */
    )

    {
    if (!bulkBfrValid)
	return ERROR;

    if (ppBfr != NULL)
	*ppBfr = bulkBfr;

    if (pActLen != NULL)
	*pActLen = bulkErp.bfrList [0].actLen;

    return OK;
    }

/*******************************************************************************
*
* usbTargPrnDataRestart - restarts listening ERP
*
* This function restarts the listening of ERP on Bulk Out Pipe.
*
* RETURNS: OK, or ERROR if unable to re-initiate ERP
*
* ERRNO:
*  none
*/

STATUS usbTargPrnDataRestart (void)
    {
    return initBulkOutErp ();
    }


/*******************************************************************************
*
* mngmtFunc - invoked by usbTargLib for connection management events
*
* This function handles various management related events. <mngmtCode>
* consist of the management event function code that is reported by the
* TargLib layer. <pContext> is the argument sent for the management event to
* be handled.
*
* RETURNS: OK if able to handle event, or ERROR if unable to handle event
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS mngmtFunc
    (
    pVOID	param,				/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,		/* target channel */
    UINT16	mngmtCode,			/* management code */
    pVOID	pContext			/* context value */
    )
    {
    pUSB_APPLN_DEVICE_INFO	pDeviceInfo = NULL;/* USB_APPLN_DEVICE_INFO */
    switch (mngmtCode)
    	{
    	case TARG_MNGMT_ATTACH:
            if (pContext == NULL)
                return ERROR;

            /* Retrieve the pointer to the device info data structure */

            pDeviceInfo = (pUSB_APPLN_DEVICE_INFO)pContext;

    	    /* Initialize global data */

            uDeviceFeature = pDeviceInfo->uDeviceFeature;
            uEndpointNumberBitmap = pDeviceInfo->uEndpointNumberBitmap;
    	    channel = targChannel;

    	    /* Allocate buffer */

    	    if ((bulkBfr = OSS_MALLOC (BULK_BFR_LEN)) == NULL)
                return ERROR;

            /* 
             * If the device is USB 2.0, initialize the bcdUSB field of
             * the device descriptor and the device qualifier descriptor.
             */

            if ((uDeviceFeature & USB_FEATURE_USB20) != 0)
                {
                devDescr.bcdUsb = TO_LITTLEW(PRN_USB20_VERSION);
                devQualifierDescr.bcdUsb = TO_LITTLEW(PRN_USB20_VERSION);
                }

            /*
             * If the device supports remote wakeup, modify the configuration
             * descriptor accordingly.
             */

            if ((uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                {
                configDescr.attributes |= USB_ATTR_REMOTE_WAKEUP;
                if ((uDeviceFeature & USB_FEATURE_USB20) != 0)
                    otherSpeedConfigDescr.attributes |= USB_ATTR_REMOTE_WAKEUP;
                }

            /* 
             * Check if the endpoint number is supported.
             * The shift value is directly taken as the endpoint address
             * as the application is specifically written for printer and it
             * supports only the bulk OUT endpoint.
             */

            if ((uEndpointNumberBitmap >> epDescr.endpointAddress) == 0)
                {
                /* 
                 * Search through the bitmap and arrive at an endpoint address
                 */
                UINT32 uIndex = 1;

                for (uIndex = 1; uIndex < 16; uIndex++)
                    {
                    if ((uEndpointNumberBitmap >> uIndex) != 0)
                        {
                        epDescr.endpointAddress = uIndex;
                        otherSpeedEpDescr.endpointAddress = uIndex;
                        break;
                        }
                    }
                if (uIndex == 16)
                    return ERROR;
                }
    	    break;

    	case TARG_MNGMT_DETACH:

    	    /* Call the function to delete the bulk pipe created */

    	    if (bulkPipeHandle != NULL)
    	        {
    	        usbTargPipeDestroy(bulkPipeHandle);
    	        bulkPipeHandle = NULL;
    	        }

    	    /* De-allocate buffer */

    	    if (bulkBfr != NULL)
                {
                OSS_FREE (bulkBfr);
                bulkBfr = NULL;
                }

            /* Reset the globals */

            channel = 0;
            uDeviceFeature = 0;
            uEndpointNumberBitmap = 0;

            /* 
             * Reset the device and device qualifier descriptors'
             * bcusb field
             */

            devDescr.bcdUsb = TO_LITTLEW(PRN_USB10_VERSION);
            devQualifierDescr.bcdUsb = TO_LITTLEW(PRN_USB10_VERSION);

            /*
             * Reset the device and device qualifier descriptors'
             * max packet size field
             */

            devDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;
            devQualifierDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;

            /* Reset the endpoint numbers */

            epDescr.endpointAddress = PRN_BULK_OUT_ENDPOINT_NUM;
            otherSpeedEpDescr.endpointAddress = PRN_BULK_OUT_ENDPOINT_NUM;

    	    break;

    	case TARG_MNGMT_BUS_RESET:

            /* Copy the operating speed of the device */

    	    uSpeed = (UINT32)pContext;
    	    curConfiguration = 0;
    	    bulkInUse = FALSE;
    	    bulkBfrValid = FALSE;
    	    uDeviceAddress = 0;

            /* Reset the device status to indicate that it is self powered */

            uDeviceStatus = 0x01;

    	    /* Call the function to delete the bulk pipe created */

    	    if (bulkPipeHandle != NULL)
    	        {
    	        usbTargPipeDestroy(bulkPipeHandle);
    	        bulkPipeHandle = NULL;
    	        }

    	    /* Initialize the descriptor values */

    	    if (uSpeed == USB_TCD_HIGH_SPEED)
    	        {
    	       
                /* 
                 * Initialize the max packet sizes for the default control
                 * pipe
                 */

                devDescr.maxPacketSize0 = PRN_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE;
                devQualifierDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;

                /* Initialize the max packet size for the bulk pipe */

                epDescr.maxPacketSize = TO_LITTLEW(USB_MAX_HIGH_SPEED_BULK_SIZE);
		otherSpeedEpDescr.maxPacketSize =
                                        TO_LITTLEW(USB_MAX_CTRL_PACKET_SIZE);

                /* Update the maximum NAK rate */

                epDescr.interval = USB_MAX_BULK_OUT_NAK_RATE;
                otherSpeedEpDescr.interval = 0;
                }
            else
    	        {
                devDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;

        	/* 
                 * If the device is a USB 2.0 device, then set the device 
                 * qualifier descriptor's maximum packet size
        	 */

        	if ((uDeviceFeature & USB_FEATURE_USB20) != 0)
        	    {
        	    devQualifierDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;
        	    otherSpeedEpDescr.maxPacketSize =
                                       TO_LITTLEW(USB_MAX_HIGH_SPEED_BULK_SIZE);
                    otherSpeedEpDescr.interval = USB_MAX_BULK_OUT_NAK_RATE;
        	    }

                /* Initialize the max packet size for the bulk pipe */

                epDescr.maxPacketSize = TO_LITTLEW(PRN_BULK_OUT_MAX_PACKETSIZE);

                /* Update the bInterval field */

                epDescr.interval = 0;
                }
             break;

    	case TARG_MNGMT_DISCONNECT:

    	    curConfiguration = 0;

    	    /* Call the function to delete the bulk pipe created */

    	    if (bulkPipeHandle != NULL)
    	        {
    	        usbTargPipeDestroy(bulkPipeHandle);
    	        bulkPipeHandle = NULL;
    	        }

    	    break;

        case TARG_MNGMT_SUSPEND:
        case TARG_MNGMT_RESUME:
    	default:
    	    break;
    	}

    return OK;
    }

/******************************************************************************
*
* featureClear - invoked by usbTargLib for CLEAR_FEATURE request
*
* This function is called to clear device or endpoint specific features.
*
* RETURNS: OK, or ERROR if unable to clear the feature
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS featureClear
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8	requestType,		/* standard request type  */
    UINT16	feature,		/* feature to be cleared */
    UINT16	index			/* 0, interface or endpoint */
    )
    {
    STATUS	status = ERROR;		/* variable to hold error status */

    /* This request is not accepted when the device is in the default state */

    if (uDeviceAddress == 0)
        return ERROR;

    /* Switch based on the feature which needs to be cleared */

    switch(feature)
        {
        case USB_FSEL_DEV_REMOTE_WAKEUP:

            /* 
             * If the device supports remote wakeup, call the function
             * to clear the remote wakeup feature
             */

            if ((uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                {
                status = usbTargDeviceFeatureClear(targChannel, feature);

                /* Clear the global status */

                if (status == OK)
                    uDeviceStatus &= ~0x02;
                }
            break;

        case USB_FSEL_DEV_ENDPOINT_HALT:

            /* Check whether the endpoint address is valid */

            if ((index == epDescr.endpointAddress) && (bulkPipeHandle != NULL))
                {

                /* Call the function to clear the endoint halt feature */

                status = usbTargPipeStatusSet(bulkPipeHandle,
                                              TCD_ENDPOINT_UNSTALL);
                }
            break;
        default:
            break;
        }
        return status;
    }

/******************************************************************************
*
* featureSet - invoked by usbTargLib for SET_FEATURE request
*
* This function is used to set device or endpoint specific features.
* <feature> consists of the feature to set.
*
* RETURNS: OK, or ERROR if unable to set the feature
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS featureSet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL targChannel,	/* target channel */
    UINT8	requestType,		/* request type */
    UINT16	feature,		/* feature to be set */
    UINT16	index			/* 0, interface or endpoint */
    )
    {
    STATUS	status = ERROR;		/* variable to hold status */

    /* 
     * This request is not accepted when the the device is in the default state
     * and the feature to be set is not TEST_MODE
     */

    if ((uDeviceAddress == 0) && (feature != USB_FSEL_DEV_TEST_MODE))
        return ERROR;

    /* Switch based on the feature which needs to be set */

    switch(feature)
        {
        case USB_FSEL_DEV_REMOTE_WAKEUP:
            
            /*
             * If the device supports remote wakeup, call the function
             * to set the remote wakeup feature
             */

            if ((uDeviceFeature & USB_FEATURE_DEVICE_REMOTE_WAKEUP) != 0)
                {
                status = usbTargDeviceFeatureSet(targChannel, feature, index);

                /* Set the global status */

                if (status == OK)
                    uDeviceStatus |= 0x02;
                }
            break;

        case USB_FSEL_DEV_TEST_MODE:

            /*
             * If the device supports remote wakeup, call the function
             * to set the test mode feature
             */

            if ((uDeviceFeature & USB_FEATURE_TEST_MODE) != 0)
                {
                status = usbTargDeviceFeatureSet(targChannel,
                                                 feature,
                                                 ((index & 0xFF00) << 8));
                }
            break;

        case USB_FSEL_DEV_ENDPOINT_HALT:

            /* Check whether the endpoint address is valid */

            if ((index == epDescr.endpointAddress) && (bulkPipeHandle != NULL))
                {

                /* Call the function to stall the endoint */

                status = usbTargPipeStatusSet(bulkPipeHandle,
                                              TCD_ENDPOINT_STALL);
                }
            break;

        default:
            break;
        }
    return status;
    }

/*******************************************************************************
*
* configurationGet - invoked by usbTargLib for GET_CONFIGURATION request
*
* This function is used to get the current configuration of the device.
* <pConfiguration> is set with the current configuration and sent to the
* host.
*
* RETURNS: OK, or ERROR if unable to return configuration setting
*
* ERRNO:
*   none.
*
* \NOMANUAL
*/

LOCAL STATUS configurationGet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    pUINT8	pConfiguration		/* data to be sent */
    )
    {

    /* This request is not accepted when the device is in the default state */

    if (uDeviceAddress == 0)
        return ERROR;

    *pConfiguration = curConfiguration;
    return OK;
    }


/******************************************************************************
*
* configurationSet - invoked by usbTargLib for SET_CONFIGURATION request
*
* This function is used to set the current configuration to the 
* configuration value sent by host. <configuration> consist of the value 
* to set.
*
* RETURNS: OK, or ERROR if unable to set specified configuration
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS configurationSet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8	configuration		/* onfiguration value */
    )
    {

    /* 
     * This request is invalid if received in a default state
     * or the configuration value is not expected
     */

    if ((uDeviceAddress == 0) || (configuration > PRN_CONFIG_VALUE))
        return ERROR;

    /* Check if the configuration value is the expected value */

    if (configuration == PRN_CONFIG_VALUE)
    	{

    	/* Create the bulk pipe if it is not created */

    	if (bulkPipeHandle == NULL)
    	    {

    	    /* Create a bulk OUT pipe */

    	    if (usbTargPipeCreate (targChannel,
                                   &epDescr,
                                   configuration,
                                   PRN_INTERFACE_NUM,
                                   PRN_INTERFACE_ALT_SETTING,
                                   &bulkPipeHandle) != OK)
        	    return ERROR;

    	    /* Initialize ERP to listen for data */

    	    initBulkOutErp ();
    	    }
    	curConfiguration = configuration;
    	}
    else
    	{

    	/* Delete the bulk pipe if created */

    	if (bulkPipeHandle != NULL)
    	    {
    	    usbTargPipeDestroy (bulkPipeHandle);
    	    bulkPipeHandle = NULL;
    	    }
    	curConfiguration = configuration;
    	}
    return OK;
    }

/*************************************************************************
*
* descriptorGet - invoked by usbTargLib for GET_DESCRIPTOR request
*
* This function is used to get the descriptor value. The type of 
* descriptor to get is specified in <descriptorType>.
*
* RETURNS: OK, or ERROR if unable to return requested descriptor
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS descriptorGet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT8	requestType,            /* Request Type */
    UINT8	descriptorType,         /* Descriptor Type */
    UINT8	descriptorIndex,        /* Index of the descriptor */
    UINT16	languageId,             /* Lang. ID for string descriptors */
    UINT16	length,                 /* Length of the descriptor */
    pUINT8	pBfr,                   /* buffer in which data is to be sent */
    pUINT16	pActLen                 /* Length of data in the buffer */
    )
    {
    UINT8 bfr [USB_MAX_DESCR_LEN];	/* buffer to hold descriptor value */
    UINT16 actLen;			/* actual length */

    /* Determine type of descriptor being requested. */

    if (requestType == (USB_RT_DEV_TO_HOST | USB_RT_STANDARD | USB_RT_DEVICE))
    	{
        switch (descriptorType)
            {
    	    case USB_DESCR_DEVICE:
                usbDescrCopy (pBfr, &devDescr, length, pActLen);
                break;

    	    case USB_DESCR_DEVICE_QUALIFIER:
                usbDescrCopy (pBfr, &devQualifierDescr, length, pActLen);
                break;

    	    case USB_DESCR_CONFIGURATION:
                memcpy (bfr, &configDescr, USB_CONFIG_DESCR_LEN);
                memcpy (&bfr [USB_CONFIG_DESCR_LEN], &ifDescr,
                        USB_INTERFACE_DESCR_LEN);
                memcpy (&bfr [USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN],
                        &epDescr, USB_ENDPOINT_DESCR_LEN);

                actLen = min (length, USB_CONFIG_DESCR_LEN +
        		      USB_INTERFACE_DESCR_LEN + USB_ENDPOINT_DESCR_LEN);

                memcpy (pBfr, bfr, actLen);
                *pActLen = actLen;
                break;

    	    case USB_DESCR_OTHER_SPEED_CONFIGURATION:
                memcpy (bfr, &otherSpeedConfigDescr, USB_CONFIG_DESCR_LEN);
                memcpy (&bfr [USB_CONFIG_DESCR_LEN], &ifDescr,
        	        USB_INTERFACE_DESCR_LEN);
                memcpy (&bfr [USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN],
                        &otherSpeedEpDescr, USB_ENDPOINT_DESCR_LEN);

                actLen = min (length, USB_CONFIG_DESCR_LEN +
                              USB_INTERFACE_DESCR_LEN + USB_ENDPOINT_DESCR_LEN);

                 memcpy (pBfr, bfr, actLen);
                 *pActLen = actLen;
                 break;

    	    case USB_DESCR_STRING:
                switch (descriptorIndex)
                    {
                    case 0: /* language descriptor */
                        usbDescrCopy (pBfr, &langDescr, length, pActLen);
            		break;

                    case ID_STR_MFG:
                        usbDescrStrCopy (pBfr, pStrMfg, length, pActLen);
            		break;

                    case ID_STR_PROD:
                        usbDescrStrCopy (pBfr, pStrProd, length, pActLen);
                        break;

                    default:
                        return ERROR;
                     }
                break;

    	       default:
    		    return ERROR;
    	    }
    	}
    else
    	{
    	return ERROR;
    	}

    return OK;
    }

/******************************************************************************
*
* interfaceGet - invoked by usbTargLib for GET_INTERFACE request
*
* This function is used to get the selected alternate setting of the
* specified interface.
*
* RETURNS: OK, or ERROR if unable to return interface setting
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS interfaceGet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16	interfaceIndex,		/* index of the specified interface */
    pUINT8	pAlternateSetting	/* value of alternate setting sent */
    )
    {

    /* 
     * This is an invalid request if the device is in
     * default or addressed state
     */

    if ((uDeviceAddress == 0) ||(curConfiguration == 0))
        return ERROR;

    *pAlternateSetting = PRN_INTERFACE_ALT_SETTING;
    return OK;
    }

/******************************************************************************
*
* interfaceSet - invoked by usbTargLib for SET_INTERFACE request
*
* This function is used to select the alternate setting of he specified
* interface.
*
* RETURNS: OK, or ERROR if unable to set specified interface
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS interfaceSet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16	interfaceIndex,		/* interface index */
    UINT8	alternateSetting	/* alternate setting to be set */
    )
    {

    /*
     * This is an invalid request if the device is in default/addressed state
     * or if the alternate setting value does not match
     */

    if ((uDeviceAddress == 0) ||
        (curConfiguration == 0) ||
        (alternateSetting != PRN_INTERFACE_ALT_SETTING))
        return ERROR;

    curAlternateSetting = alternateSetting;
    return OK;
    }

/******************************************************************************
*
* statusGet - invoked by usbTargLib for GET_STATUS request
*
* This function is used to get the status of the recipient specified in
* <index>. The status is sent in <pBfr> to the host.
*
* RETURNS: OK, or ERROR if unable to get the status of the recepient.
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS statusGet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16	requestType,            /* type of request */
    UINT16	index,                  /* recipient */
    UINT16	length,                 /* wLength */
    pUINT8	pBfr                    /* status of the specified recipient */
    )
    {

    STATUS	status = ERROR;		

    /* This is an invalid request if received in default state */

    if (uDeviceAddress == 0)
        return ERROR;

    /* Switch based on the recipient value specified */

    switch(requestType & USB_RT_RECIPIENT_MASK)
        {
        case USB_RT_DEVICE:
            pBfr[0] = uDeviceStatus;
            status = OK;
            break;
        case USB_RT_ENDPOINT:
            if ((index == epDescr.endpointAddress) && (bulkPipeHandle != NULL))
                status = usbTargPipeStatusGet(bulkPipeHandle, pBfr);
            break;
        default:
            break;
        }
    return status;
    }

/******************************************************************************
*
* addressSet - invoked by usbTargLib for SET_ADDRESS request
*
* This function is used to set the address of the device. <deviceAddress>
* consist of the address to set.
*
* RETURNS: OK, or ERROR if unable to set the address
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS addressSet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16	deviceAddress		/* device address to set */
    )
    {

    /* The device cannot accept a set address request after configuration */

    if (curConfiguration != 0)
        return ERROR;

    /* Copy the address to the global */

    uDeviceAddress = deviceAddress;
    return OK;
    }

/******************************************************************************
*
* vendorSpecific - invoked by usbTargLib for VENDOR_SPECIFIC request
*
* This fucntion is called when any vendor specific request comes from host.
*
* RETURNS: OK, or ERROR if unable to process vendor-specific request
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS vendorSpecific
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8	requestType,		/* characteristics of request */
    UINT8	request,		/* request type */
    UINT16	value,                  /* wValue */
    UINT16	index,                  /* wIndex */
    UINT16	length                  /* length to transfer */
    )
    {
    UINT8	bfr [USB_MAX_DESCR_LEN]; /* Buffer to hold data */	
    pUSB_PRINTER_CAPABILITIES pCaps = (pUSB_PRINTER_CAPABILITIES) bfr;
    UINT16	capsLen = strlen (capsString) + 2;

    if (requestType == (USB_RT_DEV_TO_HOST | USB_RT_CLASS | USB_RT_INTERFACE))
    	{
    	switch (request)
    	    {
    	    case USB_REQ_PRN_GET_DEVICE_ID:

                /* Send the IEEE-1284-style "device id" string. */

                pCaps->length = TO_BIGW (capsLen);
                memcpy (pCaps->caps, capsString, strlen (capsString));

                usbTargControlResponseSend (targChannel, capsLen, 
                                            (pUINT8) pCaps);
                 return OK;

    	    case USB_REQ_PRN_GET_PORT_STATUS:

                /* Initiate the data phase for sending the port status */

                 usbTargControlResponseSend (targChannel, sizeof (portStatus), 
                                             (pUINT8) &portStatus);

                  return OK;

    	    default:
        	break;
    	    }
    	}

    else if (requestType == (USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_OTHER))
    	{
    	switch (request)
    	    {
    	    case USB_REQ_PRN_SOFT_RESET:

                /* We accept the SOFT_RESET and initiate the status phase*/

                usbTargControlResponseSend (targChannel, 0, NULL);

                return OK;

    	    default:
                break;
    	    }
    	}
    return ERROR;
    }
   

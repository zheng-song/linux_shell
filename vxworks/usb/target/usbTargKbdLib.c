/* usbTargKbdLib.c - USB keyboard target exerciser/demonstration */

/* Copyright 1999-2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
02e,02aug04,mta  Modification History Changes
02d,23jul04,ami  Apigen Changes
02c,19jul04,ami  Coding Convention Changes
02b,30jun04,pdg  Bug fixes - isp1582 full speed testing
02a,05apr04,pdg  modifications for USB 2.0 stack.
01b,23nov99,rcb  Change #include ../xxx references to lower case.
01a,18aug99,rcb  First.
*/

/*
DESCRIPTION

This module contains code to exercise the usbTargLib by emulating a rudimentary
USB keyboard.  This module will generally be invoked by usbTool or a similar
USB test/exerciser application.

It is the caller's responsibility to initialize usbTargLib and attach a USB TCD
to it.	When attaching a TCD to usbTargLib, the caller must pass a pointer to a
table of callbacks required by usbTargLib.  The address of this table and the
"callback parameter" required by these callbacks may be obtained by calling
usbTargKbdCallbackInfo().  It is not necessary to initialize the usbTartKbdLib 
or to shut it down.  It performs all of its operations in response to callbacks 
from  usbTargLib.

This module also exports a function called usbTargKbdInjectReport().  This 
function allows the caller to inject a "boot report" into the interrupt pipe.
This allows for rudimentary emulation of keystrokes.

INCLUDE FILES: usb/usbPlatform.h, string.h, usb/usb.h, usb/usbHid.h,
               usb/usbDescrCopyLib.h, usb/target/usbTargLib.h,
               drv/usb/target/usbTargKbdLib.h

*/


/* includes */

#include "usb/usbPlatform.h"               
#include "string.h"                         
#include "usb/usb.h"                        
#include "usb/usbHid.h" 		    
#include "usb/usbDescrCopyLib.h"            
#include "usb/target/usbTargLib.h"	    
#include "drv/usb/target/usbTargKbdLib.h"

/* defines */

/* string identifiers */

#define UNICODE_ENGLISH		0x409		/* unicode */		

#define ID_STR_MFG		1		/* manufacture's index */
#define ID_STR_MFG_VAL		"Wind River Systems" /* manufacture's string */

#define ID_STR_PROD		2		/* product id */
#define ID_STR_PROD_VAL		"USB keyboard emulator" /* product name */


/* keyboard device */

#define KBD_USB10_VERSION	0x0100	/* full speed version */
#define KBD_USB20_VERSION	0x0200	/* high speed version */
#define KBD_NUM_CONFIG		1	/* number of configurations */

/* keyboard configuration */

#define KBD_CONFIG_VALUE	1	/* keyboard configuraion value */
#define KBD_NUM_INTERFACES	1	/* number of interfaces */

/* keyboard interface */

#define KBD_INTERFACE_NUM		0	/* interface number */
#define KBD_INTERFACE_ALT_SETTING	0	/* alternate setting Index */
#define KBD_NUM_ENDPOINTS		1	/* number of endpoints */

/* keyboard interrupt endpoint */

#define KBD_INTERRUPT_ENDPOINT_NUM	0x81    /* interrupt endpoint index */
#define KBD_INTERRUPT_ENDPOINT_INTERVAL 20      /* 20 milliseconds */
#define KBD_HIGH_SPEED_POLLING_INTERVAL	6

#define KBD_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE  0x40 /* maximum packet size */
						     /* for the default */
                                                     /* control endpoint, if */
						     /* the device is */
                                                     /* operating at high */
						     /* speed */
/* locals */

LOCAL	USB_TARG_CHANNEL	channel;	     /* target channel */
LOCAL	UINT8			curConfiguration;    /* current configuration */
LOCAL	UINT8			curAlternateSetting; /* current alternate */
						     /* setting */
LOCAL	USB_TARG_PIPE		intPipeHandle;	/* interrupt pipe handler*/
LOCAL	USB_ERP			reportErp;	/* boot report erp */
LOCAL	BOOL			reportInUse;	/* boot report in use */
LOCAL	HID_KBD_BOOT_REPORT 	reportBfr;	/* HID class boot report */
LOCAL	UINT32			uDeviceFeature = 0;  /* features supported by */
						     /* device */
LOCAL	UINT32			uEndpointNumberBitmap = 0;	
						/* bitmap to show the */
						/* endpoints supported by */
						/* the device. */
LOCAL	UINT32			uSpeed = USB_TCD_FULL_SPEED;
						/* operation speed */
LOCAL	UINT8			uDeviceStatus = 0x01;	/* curent status */
LOCAL	UINT8			uDeviceAddress = 0;	/* device address */

LOCAL USB_LANGUAGE_DESCR langDescr = {sizeof (USB_LANGUAGE_DESCR),
                                      USB_DESCR_STRING,
                                      {TO_LITTLEW (UNICODE_ENGLISH)}};

LOCAL	char	*pStrMfg  = ID_STR_MFG_VAL;	/* manufacturer id */
LOCAL	char	*pStrProd = ID_STR_PROD_VAL;	/* product id */

LOCAL	UCHAR	pKeyBoardBuf[2];

LOCAL USB_DEVICE_DESCR devDescr =	/* Device Descriptor */
    {
    USB_DEVICE_DESCR_LEN,		/* bLength */
    USB_DESCR_DEVICE,			/* bDescriptorType */
    TO_LITTLEW (KBD_USB10_VERSION),	/* bcdUsb */
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
    KBD_NUM_CONFIG			/* bNumConfigurations */
    };

LOCAL USB_DEVICE_QUALIFIER_DESCR devQualifierDescr =	/* Device Qualifier */
    {
    USB_DEVICE_QUALIFIER_DESCR_LEN,	/* bLength */
    USB_DESCR_DEVICE_QUALIFIER,		/* bDescriptorType */
    TO_LITTLEW (KBD_USB20_VERSION),	/* bcdUsb */
    0,					/* bDeviceClass */
    0,					/* bDeviceSubclass */
    0,					/* bDeviceProtocol */
    USB_MIN_CTRL_PACKET_SIZE,		/* maxPacketSize0 */
    KBD_NUM_CONFIG,			/* bNumConfigurations */
    0					/* Reserved */
    };

LOCAL USB_CONFIG_DESCR configDescr =	/* Configuration Descriptor */
    {
    USB_CONFIG_DESCR_LEN,		/* bLength */
    USB_DESCR_CONFIGURATION,		/* bDescriptorType */
    TO_LITTLEW (USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN +
                USB_ENDPOINT_DESCR_LEN),/* wTotalLength */
    KBD_NUM_INTERFACES,			/* bNumInterfaces */
    KBD_CONFIG_VALUE,			/* bConfigurationValue */
    0,					/* iConfiguration */
    USB_ATTR_SELF_POWERED,		/* bmAttributes */
    0					/* MaxPower */
    };

LOCAL USB_CONFIG_DESCR otherSpeedConfigDescr =	/* Other Speed Descriptor */
    {
    USB_CONFIG_DESCR_LEN,		/* bLength */
    USB_DESCR_OTHER_SPEED_CONFIGURATION,/* bDescriptorType */
    TO_LITTLEW (USB_CONFIG_DESCR_LEN + USB_INTERFACE_DESCR_LEN +
                USB_ENDPOINT_DESCR_LEN),/* wTotalLength */
    KBD_NUM_INTERFACES,			/* bNumInterfaces */
    KBD_CONFIG_VALUE,			/* bConfigurationValue */
    0,					/* iConfiguration */
    USB_ATTR_SELF_POWERED,		/* bmAttributes */
    0					/* MaxPower */
    };

LOCAL USB_INTERFACE_DESCR ifDescr =	/* Interface Descriptor */
    {
    USB_INTERFACE_DESCR_LEN,		/* bLength */
    USB_DESCR_INTERFACE,		/* bDescriptorType */
    KBD_INTERFACE_NUM,			/* bInterfaceNumber */
    KBD_INTERFACE_ALT_SETTING,		/* bAlternateSetting */
    KBD_NUM_ENDPOINTS,			/* bNumEndpoints */
    USB_CLASS_HID,			/* bInterfaceClass */
    USB_SUBCLASS_HID_BOOT,		/* bInterfaceSubClass */
    USB_PROTOCOL_HID_BOOT_KEYBOARD,	/* bInterfaceProtocol */
    0					/* iInterface */
    };

LOCAL USB_ENDPOINT_DESCR epDescr =	/* Endpoint Descriptor */
    {
    USB_ENDPOINT_DESCR_LEN,		/* bLength */
    USB_DESCR_ENDPOINT,			/* bDescriptorType */
    (USB_ENDPOINT_DIR_MASK | KBD_INTERRUPT_ENDPOINT_NUM),
					/* bEndpointAddress */
    USB_ATTR_INTERRUPT,			/* bmAttributes */
    TO_LITTLEW(sizeof (HID_KBD_BOOT_REPORT)),
					/* maxPacketSize */
    KBD_INTERRUPT_ENDPOINT_INTERVAL	/* bInterval */
    };

/* forward declarations */

LOCAL STATUS mngmtFunc (pVOID param, USB_TARG_CHANNEL targChannel, 
                        UINT16 mngmtCode, pVOID pContext);

LOCAL STATUS featureClear (pVOID param, USB_TARG_CHANNEL targChannel,
                           UINT8 requestType, UINT16 feature, UINT16 index);

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

LOCAL VOID usbkbdSetReportCallback (pVOID pErp);

LOCAL USB_TARG_CALLBACK_TABLE usbTargKbdCallbackTable =	/* Callback Table */
    {
    mngmtFunc,		/* mngmtFunc */
    featureClear,	/* featureClear */
    featureSet, 	/* featureSet */
    configurationGet,	/* configurationGet */
    configurationSet,	/* configurationSet */
    descriptorGet,	/* descriptorGet */
    NULL,		/* descriptorSet */
    interfaceGet,	/* interfaceGet */
    interfaceSet,	/* interfaceSet */
    statusGet,		/* statusGet */
    addressSet,		/* addressSet */
    NULL,		/* synchFrameGet */
    vendorSpecific,	/* vendorSpecific */
    };

/******************************************************************************
*
* usbTargKbdCallbackInfo - returns usbTargKbdLib callback table
*
* This function is called by the initialization rountine. It returns the
* callback table information.
*
* RETURNS: N/A
*
* ERRNO:
*  none.
*/

VOID usbTargKbdCallbackInfo
    (
    struct usbTargCallbackTable ** ppCallbacks,  /* Callback table pointer */
    pVOID *pCallbackParam                  /* target app-specific parameter */
    )
    {
    if (ppCallbacks != NULL)
	*ppCallbacks = &usbTargKbdCallbackTable;

    if (pCallbackParam != NULL)
	*pCallbackParam = NULL;
    }


/******************************************************************************
*
* reportErpCallback - called when report ERP terminates
*
* This callback is invoked when an Endpoint Request Packet terminates. It
* states that no more ERP reports are pending.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL VOID reportErpCallback
    (
    pVOID p				/* pointer to ERP */
    )
    {
    reportInUse = FALSE;
    }


/******************************************************************************
*
* usbTargKbdInjectReport - injects a "boot report"
*
* This function injects the boot report into the interrupt pipe. <pReport>
* is the pointer to the boot report ot be injected. <reportErpCallback> is
* called after the boot report is successfully sent to the host.
*
* RETURNS: OK, or ERROR if unable to inject report
*
* ERRNO:
*  none.
*/

STATUS usbTargKbdInjectReport
    (
    pHID_KBD_BOOT_REPORT pReport,	/* Boot Report to be injected */
    UINT16 reportLen			/* Length of the boot report */
    )
    {
    
    /* If the report pipe isn't enabled, return an error. */

    if (intPipeHandle == NULL)
	return ERROR;

    /* If a report is already queued, return an error. */

    while (reportInUse)
        OSS_THREAD_SLEEP(1);

    reportInUse = TRUE;

    /* Copy the report and set up the transfer. */

    reportLen = min (sizeof (reportBfr), reportLen);
    memcpy (&reportBfr, pReport, reportLen);

    memset (&reportErp, 0, sizeof (reportErp));

    reportErp.erpLen = sizeof (reportErp);
    reportErp.userCallback = reportErpCallback;
    reportErp.bfrCount = 1;
    reportErp.bfrList [0].pid = USB_PID_IN;
    reportErp.bfrList [0].pBfr = (pUINT8) &reportBfr;
    reportErp.bfrList [0].bfrLen = reportLen;

    return usbTargTransfer (intPipeHandle, &reportErp);
    }


/******************************************************************************
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
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS mngmtFunc
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT16	mngmtCode,		/* management code */
    pVOID	pContext		/* Context value */
    )

    {
    pUSB_APPLN_DEVICE_INFO	pDeviceInfo = NULL; /* Pointer to */
						    /* USB_APPLN_DEVICE_INFO */

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

            /* 
             * If the device is USB 2.0, initialize the bcdUSB field of
             * the device descriptor and the device qualifier descriptor.
             */

            if ((uDeviceFeature & USB_FEATURE_USB20) != 0)
                {
                devDescr.bcdUsb = TO_LITTLEW(KBD_USB20_VERSION);
                devQualifierDescr.bcdUsb = TO_LITTLEW(KBD_USB20_VERSION);
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
             * as the application is specifically written for keyboard and it
             * supports only the interrupt IN endpoint.
             */

            if ((uEndpointNumberBitmap >>
                (16 + (USB_ENDPOINT_MASK & epDescr.endpointAddress))) == 0)
                {

                /* Search through the bitmap and arrive at an endpoint address*/

                UINT32 uIndex = 1;

                for (uIndex = 16; uIndex < 32; uIndex++)
                    {
                    if ((uEndpointNumberBitmap >> uIndex) != 0)
                        {
                        epDescr.endpointAddress = USB_ENDPOINT_IN | (uIndex - 16);
                        break;
                        }
                    }
                if (uIndex == 32)
                    return ERROR;
                }

    	    break;

    	case TARG_MNGMT_DETACH:

            /* Reset the globals */

            channel = 0;
            uDeviceFeature = 0;
            uEndpointNumberBitmap = 0;

            /* Reset the device and device qualifier descriptors' bcusb field */

            devDescr.bcdUsb = TO_LITTLEW(KBD_USB10_VERSION);
            devQualifierDescr.bcdUsb = TO_LITTLEW(KBD_USB10_VERSION);


            epDescr.endpointAddress = KBD_INTERRUPT_ENDPOINT_NUM;

    	    break;

    	case TARG_MNGMT_BUS_RESET:

            /* Copy the operating speed of the device */

    	    uSpeed = (UINT32)pContext;

    	    curConfiguration = 0;
    	    uDeviceAddress = 0;

            /* Reset the device status to indicate that it is self powered */

            uDeviceStatus = 0x01;

            /* Delete the interrupt pipe if created */

    	    if (intPipeHandle != NULL)
    	        {
    	        usbTargPipeDestroy (intPipeHandle);
    	        intPipeHandle = NULL;
    	        }

    	    /* Initialize the descriptor values */

    	    if (uSpeed == USB_TCD_HIGH_SPEED)
    	        {

    	        /*Initialize the max packet sizes for the default control pipe*/

        	devDescr.maxPacketSize0 = KBD_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE;
                devQualifierDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;
                epDescr.interval = KBD_HIGH_SPEED_POLLING_INTERVAL;
                }
       	     else
       	        {
                devDescr.bcdUsb = TO_LITTLEW(KBD_USB10_VERSION);
                devQualifierDescr.bcdUsb = TO_LITTLEW(KBD_USB10_VERSION);
        	devDescr.maxPacketSize0 = USB_MIN_CTRL_PACKET_SIZE;
                epDescr.interval = KBD_INTERRUPT_ENDPOINT_INTERVAL;

       	        /* 
                 * If the device is a USB 2.0 device, then set the device
                 * qualifier descriptor's maximum packet size
        	 */

        	if ((uDeviceFeature & USB_FEATURE_USB20) != 0)
        	    {
        	        devQualifierDescr.maxPacketSize0 =
                                         KBD_HIGH_SPEED_CONTROL_MAX_PACKET_SIZE;
        	    }
                }

            break;

    	case TARG_MNGMT_DISCONNECT:

    	    curConfiguration = 0;

            /* Delete the interrupt pipe if created */
    	    
            if (intPipeHandle != NULL)
    	        {
    	        usbTargPipeDestroy (intPipeHandle);
    	        intPipeHandle = NULL;
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
* RETURNS: OK, or ERROR if unable to clear requested feature
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS featureClear
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT8	requestType,		/* Type of request */
    UINT16	feature,		/* Feature to be cleared */
    UINT16	index			/* 0, interface or endpoint */
    )
    {
    STATUS status = ERROR;

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

            if ((index == epDescr.endpointAddress) && (intPipeHandle != NULL))
                {

                /* Call the function to clear the endoint halt feature */

                status = usbTargPipeStatusSet(intPipeHandle,
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
* RETURNS: OK, or ERROR if unable to set requested feature
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS featureSet
    (
    pVOID	param,			/* TCD Specific Parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT8	requestType,		/* Type of Request */
    UINT16	feature,		/* Feature to set */
    UINT16	index			/* 0, interface or endpoint */
    )
    {
    STATUS	status = ERROR;		/* varaiable to hold error status */

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

            if ((index == epDescr.endpointAddress) && (intPipeHandle != NULL))
                {

                /* Call the function to stall the endoint */

                status = usbTargPipeStatusSet(intPipeHandle,
                                              TCD_ENDPOINT_STALL);
                }
            break;
        
        default:
            break;
        }
    return status;
    }

/******************************************************************************
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
*  none
*
* \NOMANUAL
*/

LOCAL STATUS configurationGet
    (
    pVOID	param,			/* TCD Specific Parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    pUINT8	pConfiguration		/* Configuration value to sent */
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
*
* This function is used to set the current configuration to the configuration
* value sent by host. <configuration> consist of the value to set.
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
    pVOID	param,			/* TCD Specific Parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT8	configuration		/* Configuration value to set */
    )
    {

    /* 
     * This request is invalid if received in a default state
     * or the configuration value is not expected
     */

    if ((uDeviceAddress == 0) || (configuration > KBD_CONFIG_VALUE))
        return ERROR;

    /* Check if the configuration value is the expected value */

    if (configuration == KBD_CONFIG_VALUE)
    	{

    	/* Create the interrupt pipe if it is not created */

    	if (intPipeHandle == NULL)
    	    {

    	    /* Create a bulk OUT pipe */

     	    if (usbTargPipeCreate (targChannel,
                                   &epDescr,
                                   configuration,
                                   KBD_INTERFACE_NUM,
                                   KBD_INTERFACE_ALT_SETTING,
                                   &intPipeHandle) != OK)
                return ERROR;

       	    reportInUse = FALSE;
    	    }
    	curConfiguration = configuration;
    	}
    else
    	{

    	/* Delete the interrupt pipe if created */

    	if (intPipeHandle != NULL)
    	    {
    	    usbTargPipeDestroy (intPipeHandle);
    	    intPipeHandle = NULL;
    	    }
    	curConfiguration = configuration;
    	}
    return OK;
    }

/******************************************************************************
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
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT8	requestType,            /* request type */
    UINT8	descriptorType,         /* type of descriptor */
    UINT8	descriptorIndex,        /* descriptor index */
    UINT16	languageId,		/* lang. ID for string descriptors */
    UINT16	length,			/* length of the descriptor */
    pUINT8	pBfr,			/* buffer send data */
    pUINT16	pActLen			/* length of data copied */
    )
    {
    UINT8	bfr [USB_MAX_DESCR_LEN];/* buffer to hold descriptor values */
    UINT16	actLen;			/* total length */

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
                        &epDescr, USB_ENDPOINT_DESCR_LEN);

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
*/

LOCAL STATUS interfaceGet
    (
    pVOID	param,			/* TCD specific parameter */
    USB_TARG_CHANNEL	targChannel,	/* target channel */
    UINT16	interfaceIndex,		/* index of the specified interface */
    pUINT8	pAlternateSetting	/* value of alternate setting*/
    )
    {

    /* 
     * This is an invalid request if the device is in
     * default or addressed state
     */

    if ((uDeviceAddress == 0) ||(curConfiguration == 0))
        return ERROR;

    *pAlternateSetting = KBD_INTERFACE_ALT_SETTING;
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
    UINT16	interfaceIndex,		/* index of the specified interface */
    UINT8	alternateSetting	/* alternate Setting to be set */
    )
    {
    
    /* 
     * This is an invalid request if the device is in default/addressed state
     * or if the alternate setting value does not match
     */

    if ((uDeviceAddress == 0) ||
        (curConfiguration == 0) ||
        (alternateSetting != KBD_INTERFACE_ALT_SETTING))
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
    UINT16	requestType,		/* type of request */
    UINT16	index,			/* recipient */
    UINT16	length,			/* wlength */
    pUINT8	pBfr			/* status of the specified recipient */
    )
    {
    STATUS status = ERROR;

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
            if ((index == epDescr.endpointAddress) && (intPipeHandle != NULL))
                status = usbTargPipeStatusGet(intPipeHandle, pBfr);
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
    pVOID	param,			/* TCD Specific Parameter */
    USB_TARG_CHANNEL	targChannel,	/* Target Channel */
    UINT16	deviceAddress		/* Device Address to Set */
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
* usbkbdSetReportCallback - invoked on  USB_REQ_HID_SET_REPORT request 
*
* This function is invoked when USB_REQ_HID_SET_REPORT vendor specific
* request is received from the host. It acts as a dummy.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*/

LOCAL VOID usbkbdSetReportCallback
    (
    pVOID	pErp			/* Pointer to ERP structure */
    )
    {
    return;
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
    if (requestType == (USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_INTERFACE))
    	{
    	switch (request)
    	    {
    	    case USB_REQ_HID_SET_PROTOCOL:
    	    case USB_REQ_HID_SET_IDLE:

                /* 
                 * This emulator simply acknowledges these HID requests...no
        	 * processing is required.
        	 */

        	usbTargControlStatusSend (targChannel);
        	return OK;

    	    case USB_REQ_HID_SET_REPORT:

                /* Send the request to receive the data from the host */

        	usbTargControlPayloadRcv (targChannel, 1, pKeyBoardBuf, 
                                          usbkbdSetReportCallback);
                return OK;

    	    default:
        	break;
    	    }
    	}

    return ERROR;
    }

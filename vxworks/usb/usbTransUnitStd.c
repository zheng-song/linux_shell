/* usbTransUnitStd.c - translation unit standard requests interfaces */

/* Copyright 2003 Wind River Systems, Inc. */

/*
Modification history
--------------------
01g,15oct04,ami  Apigen Changes
01f,11oct04,ami  Apigen changes
01h,16sep04,hch  Fix diab compiler error on strncpy
01g,12apr04,cfc  Apigen corrections
01f,02dec03,cfc  Merge for isochronous USB speaker support
01e,17sep03,cfc  Remove direct calls to wvEvent
01d,10jul03,mat     Set Configuration Fix
01c,16jun03,mat  prefixed "usb" to file name
01b,06jun03,mat  Wind View Instrumentation.
01a,06jun03,mat  Wind River Coding Convention  API changes.
*/

/*
DESCRIPTION

Implements the Translation Unit Standard Requests Interfaces.

INCLUDE FILES: drv/usb/usbTransUnit.h, usb2/usbHcdInstr.h

*/

/* includes */

#include "drv/usb/usbTransUnit.h"
#include "usb2/usbHcdInstr.h"

/* defines */

#define USBTUSTD_RECIPIENT_MASK		0x03	
#define USBTUSTD_STATUS_BUFFER_SIZE	2




/*******************************************************************************
*
* usbdFeatureClear - clears a USB feature
*
* This function allows a client to "clear" a USB feature.  <nodeId> specifies
* the Node Id of the desired device and <requestType> specifies whether the
* feature is related to the device, to an interface, or to an endpoint as:
*
* \is
* \i 'USB_RT_DEVICE'
* Device
*
* \i 'USB_RT_INTERFACE'
* Interface
*
* \i 'USB_RT_ENDPOINT'
* Endpoint
* \ie
*
* <requestType> also specifies if the request is standard, class-specific,
* etc., as:
*
* \is
* \i 'USB_RT_STANDARD'
* Standard
*
* \i 'USB_RT_CLASS'
* Class-specific
*
* \i 'USB_RT_VENDOR'
* Vendor-specific
* \ie
*
* For example, USB_RT_STANDARD | USB_RT_DEVICE in <requestType> specifies a
* standard device request.
*
* The client must pass the device’s feature selector in <feature>.  If
* <featureType> specifies an interface or endpoint, then <index> must contain
* the interface or endpoint index.  <index> should be zero when <featureType>
* is USB_SELECT_DEVICE.
*
* RETURNS: OK, or ERROR if unable to clear feature.
*
* ERRNO: none
*/

STATUS usbdFeatureClear
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 requestType, 		/* Selects request type */
    UINT16 feature,			/* Feature selector */
    UINT16 index			/* Interface/endpoint index */
    )

    {
    USBHST_STATUS	status;
    UINT8		recipient;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;



    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Feature Clear "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdFeatureClear entered \n ");

    /* get the recipient */

    recipient = (UINT8)(requestType & USBTUSTD_RECIPIENT_MASK);

    status = usbHstClearFeature( (UINT32) nodeId, recipient, index, feature);

    if ( status != USBHST_SUCCESS )
    {
        USBTU_LOG( "usbdFeatureClear returns ERROR \n ");
        return ERROR;
    }
    else
        {
        USBTU_LOG( "usbdFeatureClear returns OK \n ");
        return OK;
        }

    }


/*******************************************************************************
*
* usbdFeatureSet - sets a USB feature
*
* This function allows a client to "set" a USB feature.  <nodeId> specifies
* the Node Id of the desired device and <requestType> specifies the nature
* of the feature feature as defined for the usbdFeatureClear() function.
*
* The client must pass the device’s feature selector in <feature>.  If
* <requestType> specifies an interface or endpoint, then <index> must contain
* the interface or endpoint index.  <index> should be zero when <requestType>
* includes USB_SELECT_DEVICE.
*
* RETURNS: OK, or ERROR if unable to set feature.
*
* ERRNO: none
*/

STATUS usbdFeatureSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 requestType, 		/* Selects request type */
    UINT16 feature,			/* Feature selector */
    UINT16 index			/* Interface/endpoint index */
    )

    {
    USBHST_STATUS status;
    UINT8 recipient;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Feature Set "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdFeatureSet entered \n ");

    /* get the recipient */

    recipient = (UINT8)(requestType & USBTUSTD_RECIPIENT_MASK);

    status = usbHstSetFeature( (UINT32) nodeId, recipient, index, feature, 0);

    if ( status != USBHST_SUCCESS )
        {
        USBTU_LOG( "usbdFeatureSet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        USBTU_LOG( "usbdFeatureSet returns OK \n ");
        return OK;
        }
    }

/*******************************************************************************
*
* usbdConfigurationGet - gets USB configuration for a device
*
* This function returns the currently selected configuration for the device
* or hub indicated by <nodeId>.  The current configuration value is returned
* in the low byte of <pConfiguration>.	The high byte is currently reserved
* and will be 0.
*
* RETURNS: OK, or ERROR if unable to get configuration.
*
* ERRNO: none
*/

STATUS usbdConfigurationGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    pUINT16 pConfiguration		/* bfr to receive config value */
    )

    {
    USBHST_STATUS status;
    UCHAR  configuration;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Get Configuration  "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdConfigurationGet entered \n ");

    status = usbHstGetConfiguration( (UINT32) nodeId, &configuration);

    if (status != USBHST_SUCCESS)
        {
        USBTU_LOG( "usbdConfigurationGet returns ERROR \n ");
        return ERROR;
        }

    /* Return result */

    if (pConfiguration != NULL)
        *pConfiguration = configuration;

    USBTU_LOG( "usbdConfigurationGet returns OK \n ");
    return OK;
    }


/*******************************************************************************
*
* usbdConfigurationSet - sets USB configuration for a device
*
* This function sets the current configuration for the device identified
* by <nodeId>.	The client should pass the desired configuration value in
* the low byte of <configuration>.  The high byte is currently reserved and
* should be 0.
*
* The client must also pass the maximum current which will be used by this
* configuration in <maxPower>
*
* RETURNS: OK, or ERROR if unable to set configuration.
*
* ERRNO: none
*/

STATUS usbdConfigurationSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 configuration,		/* New configuration to be set */
    UINT16 maxPower			/* max power this config will draw */
    )

    {
    USBHST_STATUS status;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    pUSB_DEVICE_DESCR  pDevDescr; /* pointer to device descriptor  */
    UINT8    bfr[255];           /* store for config descriptor */
    UINT8    bfr1[255];
    UINT8  * pBfr = bfr;         /* pointer to the above store  */
    UINT8  * pBfr1 = bfr1; 
    UINT16   actLength;          /* actual length of descriptor */
    UINT16   actLength1;
    UINT8    numCfg;              /* number of configuration       */
    UINT8    index;

    pUSB_CONFIG_DESCR pCfgDescr;    /* pointer to config'n descriptor   */



    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN );
        strcat(evLog, " : Set Configuration ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }


    USBTU_LOG ( "usbdConfigurationSet entered \n ");

    /* get the device descriptor */

    if (usbdDescriptorGet (clientHandle, nodeId,
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_DEVICE, 0, 0, 20, bfr, &actLength) != OK)
        return (ERROR);

    if ((pDevDescr = usbDescrParse ((pUINT8 )pBfr, actLength, USB_DESCR_DEVICE)) == NULL)
         return (ERROR);

    /* retrieve the num of configurations */

    numCfg       = pDevDescr->numConfigurations;

    for (index = 0; index < numCfg; index++)
    {

    /* get the config descriptor */

    if (usbdDescriptorGet (clientHandle, nodeId,
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_CONFIGURATION, index, 0,
                           255, bfr1, &actLength1) != OK)
        return (ERROR);


    if ((pCfgDescr = usbDescrParseSkip ((pUINT8* )& pBfr1, &actLength1,
                                        USB_DESCR_CONFIGURATION)) == NULL)
        return (ERROR);

    /* determine if config value in descriptor matches the parameter passed */

    if (pCfgDescr->configurationValue == configuration)
        break;
    }

    if (index == numCfg)
        return (ERROR);

    status = usbHstSetConfiguration( (UINT32) nodeId, index);


    if (status != USBHST_SUCCESS)
        {
        USBTU_LOG( "usbdConfigurationSet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        USBTU_LOG( "usbdConfigurationSet returns OK \n ");
        return OK;
        }

    }


/*******************************************************************************
*
* usbdDescriptorGet - retrieves a USB descriptor
*
* A client uses this function to retrieve a descriptor from the USB device
* identified by <nodeId>.  <requestType> is defined as documented for the
* usbdFeatureClear() function.	<descriptorType> specifies the type of the
* descriptor to be retrieved and must be one of the following values:
*
* \is
* \i USB_DESCR_DEVICE
* Specifies the DEVICE descriptor.
*
* \i USB_DESCR_CONFIG
* Specifies the CONFIGURATION descriptor.
*
* \i USB_DESCR_STRING
* Specifies a STRING descriptor.
*
* \i USB_DESCR_INTERFACE
* Specifies an INTERFACE descriptor.
*
* \i USB_DESCR_ENDPOINT
* Specifies an ENDPOINT descriptor.
* \ie
*
* <descriptorIndex> is the index of the desired descriptor.
*
* For string descriptors the <languageId> should specify the desired
* language for the string.  According to the USB Specification, strings
* descriptors are returned in UNICODE format and the <languageId> should
* be the "sixteen-bit language ID (LANGID) defined by Microsoft for
* Windows as described in .I "Developing International Software for Windows
* 95 and Windows NT."  Please refer to Section 9.6.5 of revision 1.1 of the
* USB Specification for more detail.  For device and configuration
* descriptors, <languageId> should be 0.
*
* The caller must provide a buffer to receive the descriptor data.  <pBfr>
* is a pointer to a caller-supplied buffer of length <bfrLen>.	If the
* descriptor is too long to fit in the buffer provided, the descriptor will
* be truncated.  If a non-NULL pointer is passed in <pActLen>, the actual
* length of the data transferred will be stored in <pActLen> upon return.
*
* RETURNS: OK, or ERROR if unable to get descriptor.
*
* ERRNO: none
*/

STATUS usbdDescriptorGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT8 requestType,			/* specifies type of request */
    UINT8 descriptorType,		/* Type of descriptor */
    UINT8 descriptorIndex,		/* Index of descriptor */
    UINT16 languageId,			/* Language ID */
    UINT16 bfrLen,			/* Max length of data to be returned */
    pUINT8 pBfr,			/* Pointer to bfr to receive data */
    pUINT16 pActLen			/* bfr to receive actual length */
    )

    {
    USBHST_STATUS status;
    UINT32  len = bfrLen;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        if ( pDriver != NULL)
            strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        else
            strncpy ((char*)evLog, "Translation unit thread" ,
                      strlen("Translation unit thread") +1);
        strcat(evLog, " : Get Descriptor "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  



    USBTU_LOG ( "usbdDescriptorGet entered \n ");

    status = usbHstGetDescriptor( (UINT32) nodeId, descriptorType,
                                    descriptorIndex, languageId, &len, pBfr);

    if ( status != USBHST_SUCCESS)
        {

        USBTU_LOG( "usbdDescriptorGet returns ERROR \n ");
        return ERROR;

        }
    else
        {
        if (pActLen != NULL)
            *pActLen = len;
        }
    USBTU_LOG( "usbdDescriptorGet returns OK \n ");
    return OK;

    }

/*******************************************************************************
*
* usbdDescriptorSet - sets a USB descriptor
*
* A client uses this function to set a descriptor on the USB device identified
* by <nodeId>.	The parameters <requestType>, <descriptorType>,
* <descriptorIndex>, and <languageId> are the same as those described for the
* usbdDescriptorGet() function.  <pBfr> is a pointer to a buffer of length
* <bfrLen> which contains the descriptor data to be sent to the device.
*
* RETURNS: OK, or ERROR if unable to set descriptor.
* 
* ERRNO: none
*/

STATUS usbdDescriptorSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT8 requestType,			/* selects request type */
    UINT8 descriptorType,		/* Type of descriptor */
    UINT8 descriptorIndex,		/* Index of descriptor */
    UINT16 languageId,			/* Language ID */
    UINT16 bfrLen,			/* Max length of data to be returned */
    pUINT8 pBfr 			/* Pointer to bfr to receive data */
    )

    {
    USBHST_STATUS status;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Set Descriptor "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdDescriptorSet entered \n ");

    status = usbHstSetDescriptor( (UINT32) nodeId, descriptorType,
                                    descriptorIndex, languageId, pBfr, bfrLen);
    if ( status != USBHST_SUCCESS)
        {
        USBTU_LOG( "usbdDescriptorSet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        USBTU_LOG( "usbdDescriptorSet returns OK \n ");
        return OK;
        }

    }


/*******************************************************************************
*
* usbdInterfaceGet - retrieves a device's current interface
*
* This function allows a client to query the current alternate setting for
* a given device’s interface.  <nodeId> and <interfaceIndex> specify the
* device and interface to be queried, respectively.  <pAlternateSetting>
* points to a UINT16 variable in which the alternate setting will be stored
* upon return.
*
* RETURNS: OK, or ERROR if unable to get interface.
*
* ERRNO: none
*/

STATUS usbdInterfaceGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 interfaceIndex,		/* Index of interface */
    pUINT16 pAlternateSetting		/* Current alternate setting */
    )

    {
    USBHST_STATUS status;
    UCHAR   alternateSetting;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Get Interface "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdInterfaceGet entered \n ");

    status = usbHstGetInterface( (UINT32) nodeId, interfaceIndex,
                                   &alternateSetting );

    if ( status != USBHST_SUCCESS)
        {
        USBTU_LOG( "usbdInterfaceGet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        if (pAlternateSetting != NULL)
        *pAlternateSetting = alternateSetting;
        }

    USBTU_LOG( "usbdInterfaceGet returns OK \n ");
    return OK;

    }


/*******************************************************************************
*
* usbdInterfaceSet - sets a device's current interface
*
* This function allows a client to select an alternate setting for a given
* device’s interface.  <nodeId> and <interfaceIndex> specify the device and
* interface to be modified, respectively.  <alternateSetting> specifies the
* new alternate setting.
*
* RETURNS: OK, or ERROR if unable to set interface.
*
* ERRNO: none
*/

STATUS usbdInterfaceSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 interfaceIndex,		/* Index of interface */
    UINT16 alternateSetting		/* Alternate setting */
    )

    {
    USBHST_STATUS status;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Set interface "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdInterfaceSet entered \n ");

    status = usbHstSetInterface( (UINT32) nodeId, interfaceIndex,
                                  alternateSetting );

    if ( status != USBHST_SUCCESS)
        {
        USBTU_LOG( "usbdInterfaceSet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        USBTU_LOG( "usbdInterfaceSet returns OK \n ");
        return OK;
        }
    }

/*******************************************************************************
*
* usbdStatusGet - Retrieves USB status from a device/interface/etc.
*
* This function retrieves the current status from the device indicated
* by <nodeId>.	<requestType> indicates the nature of the desired status
* as documented for the usbdFeatureClear() function.
*
* The status word is returned in <pBfr>.  The meaning of the status
* varies depending on whether it was queried from the device, an interface,
* or an endpoint, class-specific function, etc. as described in the USB
* Specification.
*
* RETURNS: OK, or ERROR if unable to get status.
*
* ERRNO: none
*/

STATUS usbdStatusGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 requestType, 		/* Selects device/interface/endpoint */
    UINT16 index,			/* Interface/endpoint index */
    UINT16 bfrLen,			/* length of bfr */
    pUINT8 pBfr,			/* bfr to receive status */
    pUINT16 pActLen			/* bfr to receive act len xfr'd */
    )

    {
    USBHST_STATUS status;
    UINT8 recipient;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Get Status "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdStatusGet entered \n ");
    /* check buffer size */

    if ( bfrLen < USBTUSTD_STATUS_BUFFER_SIZE )
        {
        USBTU_LOG ( "usbdStatusGet returns ERROR : bfrLen less \n");
        return ERROR;
        }

    /* get the recipient */

    recipient = (UINT8)(requestType & USBTUSTD_RECIPIENT_MASK);

    status = usbHstGetStatus( (UINT32) nodeId, recipient, index, pBfr);

    if ( status != USBHST_SUCCESS )
        {
        USBTU_LOG ( "usbdStatusGet returns ERROR \n");
        return ERROR;
        }
    else
        {
        if (pActLen != NULL)
            *pActLen = USBTUSTD_STATUS_BUFFER_SIZE;
        }

    USBTU_LOG ( "usbdStatusGet returns OK \n");
    return OK;

    }

/*******************************************************************************
*
* usbdSynchFrameGet - Returns a device's isochronous synch. frame
*
* It is sometimes necessary for clients to re-synchronize with devices when
* the two are exchanging data isochronously.  This function allows a client
* to query a reference frame number maintained by the device.  Please refer
* to the USB Specification for more detail.
*
* <nodeId> specifies the node to query and <endpoint> specifies the endpoint
* on that device.  Upon return the device’s frame number for the specified
* endpoint is returned in <pFrameNo>.
*
* RETURNS: OK, or ERROR if unable to retrieve synch. frame.
*
* ERRNO: none
*/

STATUS usbdSynchFrameGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client Handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 endpoint,			/* Endpoint to be queried */
    pUINT16 pFrameNo			/* Frame number returned by device */
    )

    {
    USBHST_STATUS status;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    UCHAR  bfr; 


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN ); 
        strcat(evLog, " : Get Synch Frame "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_STD, evLog, USBTU_WV_FILTER); 
        }  


    USBTU_LOG ( "usbdSynchFrameGet entered \n ");

    status = usbHstSetSynchFrame( (UINT32) nodeId, endpoint,&bfr);

    *pFrameNo = bfr;

    if ( status != USBHST_SUCCESS )
        {
        USBTU_LOG( "usbdSynchFrameGet returns ERROR \n ");
        return ERROR;
        }
    else
        {
        USBTU_LOG( "usbdSynchFrameGet returns OK \n ");
        return OK;
        }
    }


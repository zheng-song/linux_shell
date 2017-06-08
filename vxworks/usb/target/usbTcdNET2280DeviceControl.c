/* usbTcdNET2280DeviceControl.c - defines modules for device control features*/

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01e,22sep04,pdg  Fix for setting the address
01d,21sep04,pdg  OURADDR values changed
01c,17sep04,ami  After Control, Interrupt IN and Bulk OUT Testing
01b,08sep04,ami  Code Review Comments Incorporated
01a,2sep04,gpd Written

*/

/*
DESCRIPTION

This module implements the hardware dependent device control and status
functionalities of the NET2280.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usb.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbNET2280.h,
               drv/usb/target/usbNET2280Tcd.h, usb/target/usbPeriphInstr.h
*/
/* includes */

#include "usb/usbPlatform.h"	             
#include "usb/ossLib.h" 		     
#include "usb/usb.h"                         
#include "usb/target/usbHalCommon.h"         
#include "usb/target/usbTcd.h"               
#include "drv/usb/target/usbNET2280.h"       
#include "drv/usb/target/usbNET2280Tcd.h"   
#include "drv/usb/target/usbTcdNET2280Debug.h" 
#include "usb/target/usbPeriphInstr.h"

/******************************************************************************
*
* usbTcdNET2280FncAddressSet - implements function code TCD_FNC_ADDRESS_SET
*
* This function is used to set the address register with the specified
* address.
*
* RETURNS: OK or ERROR, if not able to set the specified address.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncAddressSet
    (
    pTRB_ADDRESS_SET	pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	addrData = 0;			/* temporary variable */
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_DEVICE_CONTROL,
    "usbTcdNET2280FncAddressSet entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncAddressSet : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {
        	
        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncAddressSet exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncAddressSet : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Extract the pointer to the USB_TCD_NET2280_TARGET from the handle */

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Read the contents of OURADDR register */
    addrData = NET2280_CFG_READ (pTarget,NET2280_OURADDR_REG);

    /* Clear the address field */
    addrData &= ~NET2280_OURADDR_REG_MASK;

    /* Not doing anything here. Everything is from interrupt context */

    /* Write to OURADDR register */
    NET2280_CFG_WRITE(pTarget, NET2280_OURADDR_REG, addrData);


    USB_NET2280_DEBUG ("usbTcdNET2280FncAddressSet : Exiting...\n",0,0,0,0,0,0);

    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280FncSignalResume - implements TCD_FNC_SIGNAL_RESUME.
*
* This function implements the TCD_FNC_SIGNAL_RESUME function code. This
* function is used to signal a resume on the USB.
*
* RETURNS : OK or ERROR, if any.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncSignalResume
    (
    pTRB_SIGNAL_RESUME	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;/* USB_TCD_NET2280_TARGET */
    UINT32	statusData = 0;
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
    "usbTcdNET2280FncSignalResume entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncSignalResume : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncSignalResume exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncSignalResume : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Extract the pointer to the USB_TCD_NET2280_TARGET from the handle */

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Read the contents of the USBSTAT register */

    statusData = NET2280_CFG_READ(pTarget, 
                                   NET2280_USBSTAT_REG);

    /* 
     * No need to clear the Device Remote Wakeup field as a read returs
     * a zero in this field. So simply OR with the required mask value.
     */
    statusData |= NET2280_USBSTAT_REG_GENDEVREMWKUP;

    /* Write to the Device Remote Wakeup field of USBSTAT register */
    NET2280_CFG_WRITE(pTarget, 
                      NET2280_USBSTAT_REG,
                      statusData);

    USB_NET2280_DEBUG ("usbTcdNET2280FncSignalResume : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdNET2280FncCurrentFrameGet - implements TCD_FNC_CURRENT_FRAME_GET
*
* This utility function is used to implement the function code
* TCD_FNC_CURRENT_FRAME_GET. The function is used to get the current 
* frame (as encoded in the USB SOF Packet).
*
* RETURNS: OK or ERROR if any.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncCurrentFrameGet
    (
    pTRB_CURRENT_FRAME_GET	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb; /* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	frameNumberRead0 = 0;		/* Frame number read 1st time */
    UINT32	frameNumberRead1 = 0;		/* Frame number read 2nd time */
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
    "usbTcdNET2280FncCurrentFrameGet entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncCurrentFrameGet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncCurrentFrameGet exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncCurrentFrameGet:Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Extract the pointer to the USB_TCD_NET2280_TARGET from the handle */
    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Retrieve the frame number information */

    /* 
     * The address of frame number
     * register should be written to IDXADDR register. Then the IDXDATA
     * register should be read to retrieve the frame number 
     */
    NET2280_CFG_WRITE(pTarget, 
                      NET2280_IDXADDR_REG,
                      NET2280_FRAME_IDX);

    frameNumberRead0 = NET2280_CFG_READ(pTarget,
                                    NET2280_IDXDATA_REG);

    /* 
     * While the data is being read by the TCD, the TC can update the 
     * framenumber. In this case, the frame number which is read will
     * not be valid. in order to avoid this, wait till 2 consecutive reads
     * return the same value.
     * WARNING: This has to be verified by testing. High chances of the read
     * becoming very slow in the order of ms in which case, this will be an
     * infinite loop.
     */

    while (1)
        {
        frameNumberRead1 = NET2280_CFG_READ(pTarget,
                                    NET2280_IDXDATA_REG);

        if (frameNumberRead0 == frameNumberRead1)
            break;
        frameNumberRead0 = frameNumberRead1;
        }


    /* Copy the frame number into the TRB */
    pTrb->frameNo = (UINT16)frameNumberRead0;

    USB_NET2280_DEBUG ("usbTcdNET2280FncCurrentFrameGet : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdNET2280FncDeviceFeatureSet - implements TCD_FNC_DEVICE_FEATURE_SET
*
* This utility function is used to implement the function code
* TCD_FNC_DEVICE_FEATURE_SET.
*
* RETURNS: OK or ERROR if not able to set the Feature.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncDeviceFeatureSet
    (
    pTRB_DEVICE_FEATURE_SET_CLEAR	pTrb	/* Trb to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb; /* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */		
    UINT32 	registerData = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
    "usbTcdNET2280FncDeviceFeatureSet entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncDeviceFeatureSet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncDeviceFeatureSet exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncDeviceFeatureSet:Bad Parameters..\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Extract the pointer to the USB_TCD_NET2280_TARGET from the handle */

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* feature to be set is TEST_MODE */

    if (pTrb->uFeatureSelector == USB_FSEL_DEV_TEST_MODE)
        {
        switch (pTrb->uTestSelector)
            {
            case USB_TEST_MODE_J_STATE :
            case USB_TEST_MODE_K_STATE :
            case USB_TEST_MODE_SE0_ACK :
            case USB_TEST_MODE_TEST_PACKET:
            case USB_TEST_MODE_TEST_FORCE_ENABLE:
                /* Read the contents of the XCVRDIAG register */
                registerData =  NET2280_CFG_READ(pTarget,
                        NET2280_XCVRDIAG_REG);

                /* Clear the current contents of the test selector */
                registerData &= ~NET2280_XCVRDIAG_TEST_MASK;

                /* Update the test selector value */
                registerData |=
                         NET2280_XCVRDIAG_TEST_MODE_SET(pTrb->uTestSelector);
		
                /* Write to the XCVRDIAG register, the test selector value */
                NET2280_CFG_WRITE(pTarget, NET2280_XCVRDIAG_REG, registerData);
                break;
            default :

                /* WindView Instrumentation */

                USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
                "usbTcdNET2280FncDeviceFeatureSet exiting...Wrong Test Mode  \
                Feature to set",USB_TCD_NET2280_WV_FILTER);

                USB_NET2280_ERROR ("usbTcdNET2280FncDeviceFeatureSet:\
                                  Bad Parameters...\n",0,0,0,0,0,0);
                return ossStatus (S_usbTcdLib_BAD_PARAM);
            }
        }

    /* Feature to be set is device remote wakeup */
    else if (pTrb->uFeatureSelector == USB_FSEL_DEV_REMOTE_WAKEUP)
        {
        /* Read the contents of USBCTL register */
        registerData = NET2280_CFG_READ(pTarget, NET2280_USBCTL_REG);

        /* 
         * It is not required to clear the field here as this is a function
         * to set the feature. Even if this bit is set already, it does not
         * make a difference.
         */

        /* Update the remote wakeup enable field */
        registerData |= NET2280_USBCTL_REG_DRWUE;

        /* Write to the USBCTL register */
        NET2280_CFG_WRITE(pTarget,NET2280_USBCTL_REG, registerData);
        }
    /* Unidentified request */
    else
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncDeviceFeatureSet exiting...Unidentified Request",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncDeviceFeatureSet:\
                                 Unidentified request...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    USB_NET2280_DEBUG ("usbTcdNET2280FncDeviceFeatureSet : Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdNET2280FncDeviceFeatureClear - implements TCD_FNC_DEVICE_FEATURE_CLEAR
*
* This utility function is used to implement the function code
* TCD_FNC_DEVICE_FEATURE_CLEAR.
*
* RETURNS: OK or ERROR if not able to set the Test Mode Feature.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncDeviceFeatureClear
    (
    pTRB_DEVICE_FEATURE_SET_CLEAR	pTrb	/* Trb to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb; /* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */		
    UINT32 	controlRegData = 0;

    USB_NET2280_DEBUG ("usbTcdNET2280FncDeviceFeatureClear : Entered...\n",
                        0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncDeviceFeatureClear exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncDeviceFeatureClear:Bad Parameters..\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Extract the pointer to the USB_TCD_NET2280_TARGET from the handle */

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Feature to be cleared is device remote wakeup */

    if (pTrb->uFeatureSelector == USB_FSEL_DEV_REMOTE_WAKEUP)
        {
        /* Read the contents of USBCTL register */
        controlRegData = NET2280_CFG_READ(pTarget, NET2280_USBCTL_REG);

        /* Clear the remote wakeup enable field */
        controlRegData &= ~NET2280_USBCTL_REG_DRWUE;

        /* Write to the USBCTL register */
        NET2280_CFG_WRITE(pTarget,NET2280_USBCTL_REG, controlRegData);
        }
    /* Unidentified request */
    else
        {
        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_DEVICE_CONTROL,
        "usbTcdNET2280FncDeviceFeatureClear exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncDeviceFeatureClear:Bad Parameters..\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    USB_NET2280_DEBUG ("usbTcdNET2280FncDeviceFeatureSet : Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

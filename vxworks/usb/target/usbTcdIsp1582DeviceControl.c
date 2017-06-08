/* usbTcdIsp1582DeviceControl.c - Defines modules for Device Control Features*/

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01c,17sep04,ami  WindView Instrumentation Changes
01b,19jul04,ami Coding Convention Changes
01a,21apr04,ami First

*/

/*
DESCRIPTION

This module implements the hardware dependent device control and status
functionalities of the TCD.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usbPciLib.h, usb/usb.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbIsp1582Eval.h,
               drv/usb/target/usbIsp1582Tcd.h, usb/target/usbPeriphInstr.h
*/

/* includes */

#include "usb/usbPlatform.h"	             
#include "usb/ossLib.h" 		     
#include "usb/usbPciLib.h"	             
#include "usb/usb.h"                         
#include "usb/target/usbHalCommon.h"         
#include "usb/target/usbTcd.h"               
#include "drv/usb/target/usbIsp1582.h"       
#include "drv/usb/target/usbIsp1582Eval.h"   
#include "drv/usb/target/usbIsp1582Tcd.h"    
#include "usb/target/usbPeriphInstr.h"

/* defines */

#define USB_TEST_MODE_J_STATE		0x01	/* Test_J */
#define USB_TEST_MODE_K_STATE		0x02    /* Test_K */
#define USB_TEST_MODE_SE0_ACK		0x03    /* Test_SE0 */

/* functions */

/******************************************************************************
*
* usbTcdIsp1582FncAddressSet - implements function code TCD_FNC_ADDRESS_SET
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

LOCAL STATUS usbTcdIsp1582FncAddressSet
    (
    pTRB_ADDRESS_SET	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT8	data8 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncAddressSet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncAddressSet:  Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
        "usbTcdIsp1582FncAddressSet exiting...Bad Parameters received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncAddressSet : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Form the address byte */

    data8 = pTrb->deviceAddress | ISP1582_ADRS_REG_ENABLE;

    /* Write the byte into the address register */

    isp1582Write8 (pTarget, ISP1582_ADDRESS_REG, data8);

    /* Store the device address into the USB_TCD_ISP1582_TARGET structure */

    pTarget->deviceAddress = pTrb->deviceAddress;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncAddressSet exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncAddressSet : Exiting...\n",0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncSignalResume - implements TCD_FNC_SIGNAL_RESUME.
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

LOCAL STATUS usbTcdIsp1582FncSignalResume
    (
    pTRB_SIGNAL_RESUME	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;/* USB_TCD_ISP1582_TARGET */
    UINT16	data16 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncSignalResume entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncSignalResume : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
        "usbTcdIsp1582FncSignalResume exiting...Bad Parameters received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncSignalResume : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /*
     * Resume signal occurs by writing 1 followed by writing 0 into the
     * MODE register.
     */

    /* Read the mode register */

    data16 = isp1582Read16 ( pTarget , ISP1582_MODE_REG);

    data16 &= ISP1582_MODE_REG_MASK;

    /* Write Logic 1 to the SNDRSU bit of the Mode Register */

    isp1582Write16 (pTarget ,ISP1582_MODE_REG, data16 | ISP1582_MODE_REG_SNDRSU);

    /* Write Logic 0 to the SNDRSU bit of the Mode Register */

    data16 &= ~ISP1582_MODE_REG_SNDRSU;

    isp1582Write16 (pTarget , ISP1582_MODE_REG, data16 & ISP1582_MODE_REG_MASK);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncSignalResume exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncSignalResume : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncCurrentFrameGet - implements TCD_FNC_CURRENT_FRAME_GET
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

LOCAL STATUS usbTcdIsp1582FncCurrentFrameGet
    (
    pTRB_CURRENT_FRAME_GET	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb; /* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* pUSB_TCD_ISP1582_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncCurrentFrameGet entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncCurrentFrameGet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
        "usbTcdIsp1582FncCurrentFrameGet exiting...Bad Parameters received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncCurrentFrameGet:Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /*
     * Get the current frame from the Current Frame Register and update frameNo
     * member of the TRB_CURRENT_FRAME_GET structure. Only 0 - 10 bits of the
     * frame register gives the frame number.
     */

    pTrb->frameNo = isp1582Read16 (pTarget , ISP1582_FRM_NUM_REG) &
                    ISP1582_FRM_NUM_REG_SOFR_MASK;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncCurrentFrameGet exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncCurrentFrameGet : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncDeviceFeatureSet - implements TCD_FNC_DEVICE_FEATURE_SET
*
* This utility function is used to implement the function code
* TCD_FNC_DEVICE_FEATURE_SET.
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

LOCAL STATUS usbTcdIsp1582FncDeviceFeatureSet
    (
    pTRB_DEVICE_FEATURE_SET_CLEAR	pTrb	/* Trb to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb; /* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */		
    UINT8 	data8 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncDeviceFeatureSet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncDeviceFeatureSet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
        "usbTcdIsp1582FncDeviceFeatureSet exiting...Bad Parameters Recieved", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncDeviceFeatureSet:Bad Parameters..\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    if (pTrb->uFeatureSelector !=  USB_FSEL_DEV_TEST_MODE)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
        "usbTcdIsp1582FncDeviceFeatureSet exiting...Bad Parameters Recieved", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncDeviceFeatureSet:Bad Parameters..\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    switch (pTrb->uTestSelector)
        {
        case USB_TEST_MODE_J_STATE :

            /*
             * If Test Selector value is Test_J set J-State bit of TEST MODE
             * register. This set pins DP & DM of ISP 1582 to J state.
             */

            data8 = ISP1582_TEST_MODE_REG_JSTATE;
            break;

        case USB_TEST_MODE_K_STATE :

            /*
             * If Test Selector value is Test_K set K-State bit of TEST MODE
             * register. This set pins DP & DM of ISP 1582 to K state.
             */

            data8 = ISP1582_TEST_MODE_REG_KSTATE;
            break;

        case USB_TEST_MODE_SE0_ACK :

            /*
             * If Test Selector value is Test_SE0_NAK, set SE0_NAK bit of
             * TEST MODE register. This set pins DP & DM of ISP 1582 to HS
             * quienscent state.
             */

            data8 = ISP1582_TEST_MODE_REG_SE0_ACK;
            break;

        default :

        /* WindView Instrumentation */

            USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
            "usbTcdIsp1582FncDeviceFeatureSet exiting...Wrong Test Mode Feature \
            received",  USB_TCD_ISP582_WV_FILTER);   

            USBISP1582_ERROR ("usbTcdIsp1582FncDeviceFeatureSet:Bad Parameters \
            ...\n",0,0,0,0,0,0);
            return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Write the byte into the TEST Register */

    isp1582Write8 (pTarget , ISP1582_TEST_MODE_REG , data8 );

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_ISP1582_DEVICE_CONTROL,
    "usbTcdIsp1582FncDeviceFeatureSet exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncDeviceFeatureSet : Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }    

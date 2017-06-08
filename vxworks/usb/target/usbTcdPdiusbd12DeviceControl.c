/* usbTcdPdiusbd12DeviceControl.c - Defines modules for Device Control Features*/

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01c,17sep04,ami  WindView Instrumentation Changes
01b,19jul04,ami  Coding Convention Changes
01a,15mar04,ami  First

*/

/*
DESCRIPTION
This module implements the hardware dependent device control and status
functionalities of the TCD.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/target/usbIsaLib.h,
               drv/usb/target/usbPdiusbd12Eval.h, 
               drv/usb/target/usbTcdPdiusbd12EvalLib.h,
               drv/usb/target/usbPdiusbd12Tcd.h, 
               drv/usb/target/usbPdiusbd12Debug.h,
               usb/target/usbPeriphInstr.h
*/

/* includes */

#include "usb/usbPlatform.h"		        
#include "usb/ossLib.h" 		        
#include "drv/usb/target/usbPdiusbd12Eval.h"	
#include "drv/usb/target/usbTcdPdiusbd12EvalLib.h"  
#include "drv/usb/target/usbPdiusbd12Tcd.h"     
#include "drv/usb/target/usbPdiusbd12Debug.h"  
#include "usb/target/usbPeriphInstr.h"     

/* functions */

/*******************************************************************************
*
* usbTcdPdiusbd12FncAddressSet - implements TCD_FNC_ADDRESS_SET
*
* This function implements the TCD_FNC_SET_ADDRESS function code. It
* writes the given address into the register and enables it.
*
* RETURNS : OK or ERROR, if any
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncAddressSet
    (
    pTRB_ADDRESS_SET	pTrb		/* Trb to be executed */
    )
    {
    UINT8	byte = 0;			
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL; /* USB_TCD_PDIUSBD12_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncAddressSet entered ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAddressSet : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate Parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
        "usbTcdPdiusbd12FncAddressSet exiting: Bad Parameters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncAddressSet : \
        Invalid parameters ...\n",0,0,0,0,0,0 );
        return ERROR;
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* 
     * Lower 7 bits for the device address and the 8th bit for enabling
     * device address
     */

    byte = pTrb->deviceAddress & D12_CMD_SA_ADRS_MASK;
    byte |= D12_CMD_SA_ENABLE;

    /* giving the address command */

    OUT_D12_CMD (pTarget, D12_CMD_SET_ADDRESS);
    OUT_D12_DATA (pTarget, byte);
    pTarget->deviceAddress = pTrb->deviceAddress;

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAddressSet : Exiting \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncAddressSet exiting ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncCurrentFrameGet -  implements TCD_FNC_CURRENT_FRAME_GET.
*
* This function implements the function code TCD_FNC_CURRENT_FRAME_GET. It
* gets the current frame by giving proper command followed by a word read.
*
* RETURNS : OK or ERROR, if any
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncCurrentFrameGet
    (
    pTRB_CURRENT_FRAME_GET	pTrb			/* Trb to be executed */
    )
    {

    UINT8	firstByte = 0;
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncCurrentFrameGet : Entered \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncCurrentFrameGet entered ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    /* Validate Parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))

        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
        "usbTcdPdiusbd12FncCurrentFrameGet exiting: Bad Parameters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncCurrentFrameGet : Invalid \
        parameters ...\n",0,0,0,0,0,0 );
        return ERROR;
        }

     pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* Sending Frame Get Command */

    OUT_D12_CMD (pTarget , D12_CMD_READ_CURRENT_FRAME_NO);
    firstByte = IN_D12_DATA (pTarget);
    pTrb->frameNo = (firstByte || ( IN_D12_DATA (pTarget) << 8));

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncCurrentFrameGet : Exiting \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncCurrentFrameGet exiting ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncSignalResume - implements TCD_FNC_SIGNAL_RESUME.
*
* This function implements the TCD_FNC_SIGNAL_RESUME function code. This 
* function is used to signal a resume on the USB.
*
* RETURNS : OK or ERROR, if any
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncSignalResume
    (
    pTRB_SIGNAL_RESUME	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncSignalResume entered ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncSignalResume : Entered \
    ...\n",0,0,0,0,0,0);

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {


        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
        "usbTcdPdiusbd12FncSignalResume exiting: Bad Parameters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   


	USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncSignalResume : Invalid \
        parameters ...\n",0,0,0,0,0,0 );
        return ERROR;
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* Sending resume command */

    OUT_D12_CMD ( pTarget , D12_CMD_SEND_RESUME );

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncSignalResume : Exiting \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_DEVCE_CONTROL,
    "usbTcdPdiusbd12FncSignalResume exiting ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    return OK;
    }
    

/* usbTcdIsp1582InitExit.c - Initialization/uninitialization for ISP 1582 TCD */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01f,17sep04,ami  WindView Instrumentation Changes
01e,02aug04,mta  Modification History Changes
01d,19jul04,ami  Coding Convention Changes
01c,16jul04,pdg  Removed usbPciIntEnable and usbPciIntDisable
01b,21jun04,ami  Warm Reboot changes
01a,21apr04,ami  First.
*/

/*
DESCRIPTION

This file implements the initialization and uninitialization modules of TCD
(Target Controller Driver) for the Philips ISP 1582.

This module exports a single entry point, usbTcdIsp1582EvalExec().  This is
the USB_TCD_EXEC_FUNC for this TCD.  The caller passes requests to the TCD by
constructing TRBs, or Target Request Blocks, and passing them to this entry
point.

TCDs are initialized by invoking the TCD_FNC_ATTACH function.  In response to
this function, the TCD returns information about the target controller,
including its USB speed, the number of endpoints it supports etc.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usbPciLib.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbIsp1582Eval.h,
               drv/usb/target/usbTcdIsp1582EvalLib.h,
               drv/usb/target/usbIsp1582Tcd.h,
               drv/usb/target/usbIsp1582Debug.h, rebootLib.h
               usb/target/usbPeriphInstr.h   

*/

/* includes */

#include "usb/usbPlatform.h"	                
#include "usb/ossLib.h" 		             
#include "usb/usbPciLib.h"	                  
#include "usb/target/usbHalCommon.h"           
#include "usb/target/usbTcd.h"                 
#include "drv/usb/target/usbIsp1582.h"           
#include "drv/usb/target/usbIsp1582Eval.h"	    
#include "drv/usb/target/usbTcdIsp1582EvalLib.h"  
#include "drv/usb/target/usbIsp1582Tcd.h"         
#include "drv/usb/target/usbIsp1582Debug.h"   
#include "rebootLib.h"                  
#include "usb/target/usbPeriphInstr.h"     

#include "usbTcdIsp1582Util.c"                   
#include "usbTcdIsp1582DeviceControl.c"       
#include "usbTcdIsp1582Endpoint.c"          
#include "usbTcdIsp1582Interrupt.c"           


/* defines */

#define MAX_TARGET_CONTROLLER	5		/* maximum target controllers */

/* globals */

UINT32	usbIsp1582Debug = 0;			/* for debugging */

long ioBase [MAX_TARGET_CONTROLLER];		/* to hold base addresses */

#ifdef ISP1582_POLLING

/* forward declaration */

LOCAL VOID usbTcdIsp1582PollingIsr (pVOID param);
#endif

/* functions */

/******************************************************************************
*
* usbIsp1582Exit - function to be called on a reboot
*
* This function clears the Soft-Connect bit on a reboot.
*
* RETURNS: None.
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

LOCAL VOID usbIsp1582Exit
    (
    int	startType
    )
    {
    UINT8	i = 0;
    
    /* For all ISP 1582 target controllers attached, reset the mode register */

    for (i = 0 ; i < MAX_TARGET_CONTROLLER ; i++)
        {
        if (ioBase [i] != 0)
            {
            USB_PCI_WORD_OUT (ioBase [i] + ISP1582EVAL_ADDRESS_PORT ,
            ISP1582_MODE_REG);
            USB_PCI_WORD_OUT (ioBase [i] + ISP1582EVAL_DATA_PORT , 0);
            ioBase [i] = 0;
            }
        }
    }

/******************************************************************************
*
* usbTcdIsp1582Attach - function implementing function code TCD_FNC_ATTACH
*
* The purpose of this function is to initialize the Target Controller
* for USB operation.
*
* RETURNS: OK or ERROR if failed to initialize the Target Controller.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad parameter is passed.
* 
* \i S_usbTcdLib_OUT_OF_MEMORY
* No more memory to allocate variables.
*
* \i S_usbTcdLib_HW_NOT_READY
* Hardware is not ready.
*
* \i  S_usbTcdLib_GENERAL_FAULT
* Fault occured in upper software layers.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncAttach
    (
    pTRB_ATTACH	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    pUSB_TCD_ISP1582_PARAMS pParams = NULL;	/* USB_TCD_ISP1582_PARAMS */
    UINT8	data8 = 0;
    UINT16	data16 = 0;
    UINT32	data32 = 0;
    UINT8	i = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncAttach entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncAttach : Entered...\n",0,0,0,0,0,0);

    /* Validate Parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncAttach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    if ( (pTrb->tcdParam == NULL ) || (pTrb->usbHalIsr == NULL ) ||
         (pTrb->pHalDeviceInfo == NULL) || (pTrb->pDeviceInfo == NULL)  )
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncAttach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pParams = (pUSB_TCD_ISP1582_PARAMS)pTrb->tcdParam;

    /* Create a USB_TCD_ISP1582_TARGET structure to manage controller. */

    if ((pTarget = OSS_CALLOC (sizeof (*pTarget))) == NULL )
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Memory Allocation Error...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncAttach : Memory Allocation Error \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_OUT_OF_MEMORY);
        }

    /* Store the user supplied parameters */

    pTarget->ioBase = pParams->ioBase;

    pTarget->irq = pParams->irq;

    pTarget->dma = pParams->dma;

    /*
     * Store the ioBase address into the global ioBase which is to be used
     * during warm reboot
     */
    for ( i = 0 ; i < MAX_TARGET_CONTROLLER ; i++)
        {
        if (ioBase [i] == 0)
            {
            ioBase [i] = pParams->ioBase;
            break;
            }
        }

    /* Read the Chip ID and confirm that its ISP 1582 */

    data32 = (isp1582Read32 (pTarget , ISP1582_CHIP_ID_REG) &
              ISP1582_CHIP_ID_MASK);
 
    if (data32 != ISP1582_CHIP_ID)
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Wrong Chip Id...",
        USB_TCD_ISP582_WV_FILTER);   


        USBISP1582_ERROR ("usbTcdIsp1582FncAttach : Chip ID Mismatch... \n",
        0,0,0,0,0,0);

        /* Free the memory allocated */

        OSS_FREE (pTarget);
        return ossStatus (S_usbTcdLib_HW_NOT_READY);
        }

     /* Hook the function which needs to be called on a reboot */

     if(ERROR == rebootHookAdd((FUNCPTR)usbIsp1582Exit))
	{

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Enable to hook function for reboot...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ( " usbTcdIsp1582FncAttach: Not able \
        to hook a function on reboot ", 0,0,0,0,0,0);

        OSS_FREE (pTarget);
		return ERROR;
        }

    /* Unlock the registers */

    isp1582Write16 (pTarget, ISP1582_UNLOCK_DEV_REG,
                    ISP1582_UNLOCK_DEV_REG_CODE);

    /* perform a soft reset */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);

    data16 |=ISP1582_MODE_REG_SFRESET;

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Give a delay of 1 micro sec */

    OSS_THREAD_SLEEP (1);

    data16 &=~(ISP1582_MODE_REG_SFRESET);

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

     /* Give a delay of 1 micro sec */

    OSS_THREAD_SLEEP (1);

    /*
     * a soft reset does not clear the soft connect bit
     * clear the soft connect bit 
     */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);

    data16 &= ~(ISP1582_MODE_REG_SOFTCT );

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Set bit 7 in Control Port to 0 */

    USB_PCI_WORD_OUT(pTarget->ioBase + ISP1582EVAL_CNTL_PORT , ~(0x80));

    /* Clear the interrupt status register */

    isp1582Write32 (pTarget , ISP1582_INT_REG , ISP1582_INT_REG_CLEAR);

    /*
     * Clear the interrupt enable register to disable interrupts on all the
     * endpoints
     */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , 0);

    /* write 0 to OTG register */

    isp1582Write8(pTarget,ISP_1582_OTG_REG, 0);

    /* write wake up on chip select */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);
    data16 |= ISP1582_MODE_REG_WKUPCS;
    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* install the ISR */

    /* Store HAL parameters */

    pTarget->usbHalIsr = pTrb->usbHalIsr;
    pTarget->usbHalIsrParam = pTrb->usbHalIsrParam;

#ifndef   ISP1582_POLLING

    /* Set bit 7 in Control Port to 0 */

    USB_PCI_WORD_OUT((pTarget->ioBase + ISP1582EVAL_CNTL_PORT) , ~(0x80));

    /* Hook the ISR */

    if (usbPciIntConnect ((INT_HANDLER_PROTOTYPE)usbTcdIsp1582Isr,
        (pVOID)pTarget, pTarget->irq)!= OK)
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Error Hooking the ISR...",
        USB_TCD_ISP582_WV_FILTER);   

        OSS_FREE (pTarget);
        USBISP1582_ERROR ("usbTcdIsp1582FncAttach :Error Hooking the ISR...\n",
        0,0,0,0,0,0);
        return ERROR;
        }

    /* Set bit 7 in Control Port to 1 */

    USB_PCI_WORD_OUT((pTarget->ioBase + ISP1582EVAL_CNTL_PORT) , (0x80));

#else

    /* Set bit 7 in Control Port to 1 */

    USB_PCI_WORD_OUT(pTarget->ioBase + ISP1582EVAL_CNTL_PORT , (0x80));

    /* Create a thread for handling the interrupts */

    if (OSS_THREAD_CREATE((THREAD_PROTOTYPE)usbTcdIsp1582PollingIsr,
                          pTarget,
                          100,
                          "isp1582Thread",
                          &pTarget->threadId) != OK)
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncAttach exiting: Error in spawning of polling Thread...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ( "usbTcdIsp1582FncAttach : Spawning of polling \
		ISR Failed... " ,0,0,0,0,0,0);
        OSS_FREE (pTarget);
        return ERROR;
	}

#endif

    /* Decode the address to 0 */

    isp1582Write8 (pTarget ,ISP1582_ADDRESS_REG , ISP1582_ADRS_REG_ENABLE);

    /* Set Mode Register by setting bits GLINTENA , DMACLKON, CLKON */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);
    data16 |= ISP1582_MODE_REG_GLINTENA; 
    data16 |= ISP1582_MODE_REG_DMACLKON;
    data16 |= ISP1582_MODE_REG_CLKAON;
    data16 |= ISP1582_MODE_REG_POWRON;

    isp1582Write16 (pTarget , ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Form the byte to write into Interrupt Configration Register */

    data8 = ((ISP1582_INT_CONF_REG_CDBGMOD_SHIFT(
             ISP1582_INT_CONF_REG_CDBGMOD_ACK_ONLY)) |
            (ISP1582_INT_CONF_REG_DDBGMODIN_SHIFT(
             ISP1582_INT_CONF_REG_DDBGMODIN_ACK_ONLY)) |
            (ISP1582_INT_CONF_REG_DDBGMODOUT_SHIFT(
             ISP1582_INT_CONF_REG_DDBGMODOUT_ACK_NYET)));

    /*
     * Set the Interrupt Configuration Register to generate interrupts only
     * on ACKS
     */

    isp1582Write8 (pTarget , ISP1582_INT_CONFIG_REG , data8);

    /* Initialize Interrupt Enable Register */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG ,
                   (ISP1582_INT_ENABLE_REG_IEBRST |
                    ISP1582_INT_ENABLE_REG_IESUSP |
                    ISP1582_INT_ENABLE_REG_IERESM |
                    ISP1582_INT_ENABLE_REG_IEHS_STA));

    /* Initialize the DMA Hardware Register */
 
    data8 = ISP1582_DMA_HARDWARE_REG_EOT_POL | ISP1582_DMA_HARDWARE_REG_DREQ_POL;

    isp1582Write8 (pTarget, ISP1582_DMA_HARDWARE_REG , data8 );

    /* Initialize the DMA COnfiguration Register */

    isp1582Write16 (pTarget , ISP1582_DMA_CONFIG_REG ,
              ISP1582_DMA_CONFIG_REG_MODE_SET(ISP1582_DMA_CONFIG_REG_MODE_00)
              | ISP1582_DMA_CONFIG_REG_WIDTH_8);

    /* Return target information to caller */

    pTrb->pHalDeviceInfo->uNumberEndpoints = ISP1582_NUM_ENDPOINTS;

    /* supports remote wake up, test mode feature and is USB2 compliant */

    pTrb->pDeviceInfo->uDeviceFeature = USB_FEATURE_DEVICE_REMOTE_WAKEUP |
                                        USB_FEATURE_TEST_MODE |
                                        USB_FEATURE_USB20;

    /*
     * Set the bit maps for usEndpointNumbr Bitmap
     * Setting bits 0 to 7 indicates that ISP 1582 support 8 Out endpoints,
     * Setting bits 16 to 21 indicates that ISP 1582 support 8 In endpoints
     */

    pTrb->pDeviceInfo->uEndpointNumberBitmap = USB_ISP1582_TCD_OUT_ENDPOINT |
                                               USB_ISP1582_TCD_IN_ENDPOINT ;

    /* update handle */

    pHeader->handle = pTarget;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
   "usbTcdIsp1582FncAttach exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncAttach : Exiting...\n",0,0,0,0,0,0);

    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncDetach - implements TCD_FNC_DETACH
*
* The purpose of this function is to shutdown the Target Controller
*
* RETURNS: OK or ERROR, if TCD is not able to detach.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncDetach
    (
    pTRB_DETACH	pTrb				/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT16	data16 = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
   "usbTcdIsp1582FncDetach entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncDetach : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncDetach exiting: Bad Parameter received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncDetach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Disable the device address */

    /* perform a soft reset */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);
    data16 |=ISP1582_MODE_REG_SFRESET;
    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Give a delay of 1 micro sec */

    OSS_THREAD_SLEEP (1);

    data16 &= ~(ISP1582_MODE_REG_SFRESET);
    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Give a delay of 1 micro sec */

    OSS_THREAD_SLEEP (1);

    /* Reset the GINTENA */

    data16 &= ~(ISP1582_MODE_REG_SOFTCT | ISP1582_MODE_REG_GLINTENA);

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Un-hook the ISR */

#ifndef   ISP1582_POLLING

    usbPciIntRestore ((INT_HANDLER_PROTOTYPE)usbTcdIsp1582Isr, (pVOID)pTarget,
                       pTarget->irq);
   
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncDetach: ISR un-hooked successfully ...", 
    USB_TCD_ISP582_WV_FILTER);   


#else

    OSS_THREAD_DESTROY(pTarget->threadId);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncDetach: Polling Thread destroyed successfully ...", 
    USB_TCD_ISP582_WV_FILTER);   


#endif

    /* Release the USB_TCD_ISP1582_TARGET */

    OSS_FREE (pTarget);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
   "usbTcdIsp1582FncDetach exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncDetach : Exiting...\n",0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncEnable - implements TCD_FNC_ENABLE
*
* The purpose of this function is to enable the Target Controller
*
* RETURNS: OK or ERROR, if not able to enable the target controller.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncEnable
    (
    pTRB_ENABLE_DISABLE	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT16	data16 = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncEnable entered ...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEnable : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncEnable exiting: Bad Parameter received", 
        USB_TCD_ISP582_WV_FILTER);   


        USBISP1582_ERROR ("usbTcdIsp1582FncEnable : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Read the Mode Register */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);

    /* Set the soft connect bit */

    data16 |= ISP1582_MODE_REG_SOFTCT;

    /* Write to the Mode Register */

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncEnable exiting ...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEnable : Exiting...\n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncDisable - implements TCD_FNC_DISABLE
*
* The purpose of this function is to disable the Target Controller
*
* RETURNS: OK or ERROR, if not able to disable the target controller.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncDisable
    (
    pTRB_ENABLE_DISABLE	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT16	data16 = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncDisable entered ...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncDisable : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582FncDisable exiting: Bad Parameter received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncDisable : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Read the Mode Register */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);

    /* Reset the soft connect bit */

    data16 &= ~ISP1582_MODE_REG_SOFTCT;

    /* Write to the Mode Register */

    isp1582Write16 (pTarget, ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582FncDisable exiting ...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncDisable : Exiting...\n",0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582EvalExec - single Entry Point for ISP 1582 TCD
*
* This is the single entry point for the Philips ISP 1582
* USB TCD (Target Controller Driver).  The function qualifies the TRB passed
* by the caller and fans out to the appropriate TCD function handler.
*
* RETURNS: OK or ERROR if failed to execute TRB passed by caller.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad parameter is passed.
* \ie
*/

STATUS usbTcdIsp1582EvalExec
    (
    pVOID	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT32	status = OK;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582EvalExec entered ...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582EvalExec : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdIsp1582EvalExec exiting: Bad Paramters Received  ...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582EvalExec : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    if (pHeader->function != TCD_FNC_ATTACH)
        {
        if ((pTarget = (pUSB_TCD_ISP1582_TARGET) pHeader->handle) == NULL)
           {

           /* WindView Instrumentation */ 

           USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
           "usbTcdIsp1582EvalExec exiting: Handle value is NULL...",
           USB_TCD_ISP582_WV_FILTER);   

           USBISP1582_ERROR ("usbTcdIsp1582EvalExec : Bad Parameters...\n",
           0,0,0,0,0,0);
           return ossStatus (S_usbTcdLib_BAD_PARAM);
           }
        }

    USBISP1582_DEBUG ("usbTcdIsp1582EvalExec : Fucntion Code is %d...\n",
    pHeader->function,0,0,0,0,0);

    /* Fan-out to appropriate function processor */

    switch (pHeader->function)
        {

        /* Following functions codes are not implemented by ISP 1582 */

        case TCD_FNC_HANDLE_SUSPEND_INTERRUPT    :
        case TCD_FNC_HANDLE_DISCONNECT_INTERRUPT :
        case TCD_FNC_DEVICE_FEATURE_CLEAR        : break;

        /* initialization and uninitialization function codes */

        case TCD_FNC_ATTACH :
            status = usbTcdIsp1582FncAttach((pTRB_ATTACH) pHeader);
    	    break;

        case TCD_FNC_DETACH:
    	    status = usbTcdIsp1582FncDetach((pTRB_DETACH) pHeader);
            break;

        case TCD_FNC_ENABLE:
    	    status = usbTcdIsp1582FncEnable((pTRB_ENABLE_DISABLE) pHeader);
            break;

        case TCD_FNC_DISABLE:
    	    status = usbTcdIsp1582FncDisable((pTRB_ENABLE_DISABLE) pHeader);
            break;

        /* device control and status function codes */

        case TCD_FNC_ADDRESS_SET:
            status = usbTcdIsp1582FncAddressSet((pTRB_ADDRESS_SET) pHeader);
            break;

        case TCD_FNC_SIGNAL_RESUME:
    	    status = usbTcdIsp1582FncSignalResume((pTRB_SIGNAL_RESUME)pHeader);
            break;

        case TCD_FNC_CURRENT_FRAME_GET:
            status = usbTcdIsp1582FncCurrentFrameGet(
                      (pTRB_CURRENT_FRAME_GET)pHeader);
            break;

        case TCD_FNC_DEVICE_FEATURE_SET:
            status = usbTcdIsp1582FncDeviceFeatureSet(
                      (pTRB_DEVICE_FEATURE_SET_CLEAR)pHeader);
            break;

        /* endpoint related function codes */

        case TCD_FNC_ENDPOINT_ASSIGN:
            status = usbTcdIsp1582FncEndpointAssign(
                      (pTRB_ENDPOINT_ASSIGN) pHeader);
            break;

        case TCD_FNC_ENDPOINT_RELEASE:
            status = usbTcdIsp1582FncEndpointRelease(
                      (pTRB_ENDPOINT_RELEASE) pHeader);
            break;

        case TCD_FNC_ENDPOINT_STATE_SET:
            status = usbTcdIsp1582FncEndpointStateSet(
                      (pTRB_ENDPOINT_STATE_SET)pHeader);
            break;

        case TCD_FNC_ENDPOINT_STATUS_GET:
            status = usbTcdIsp1582FncEndpointStatusGet(
                      (pTRB_ENDPOINT_STATUS_GET)pHeader);
            break;

        case TCD_FNC_IS_BUFFER_EMPTY:
            status = usbTcdIsp1582FncIsBufferEmpty(
                      (pTRB_IS_BUFFER_EMPTY) pHeader);
            break;

        case TCD_FNC_COPY_DATA_FROM_EPBUF:
	        status = usbTcdIsp1582FncCopyDataFromEpBuf(
                      (pTRB_COPY_DATA_FROM_EPBUF) pHeader);
	        break;

        case TCD_FNC_COPY_DATA_TO_EPBUF:
            status = usbTcdIsp1582FncCopyDataToEpBuf(
                      (pTRB_COPY_DATA_TO_EPBUF) pHeader);
            break;

        /* interrupt related function codes */

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET:
            status = usbTcdIsp1582FncEndpointIntStatusGet(
                     (pTRB_ENDPOINT_INTERRUPT_STATUS_GET) pHeader);
            break;

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_CLEAR:
            status = usbTcdIsp1582FncEndpointIntStatusClear(
                      (pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR) pHeader);
            break;

        case TCD_FNC_INTERRUPT_STATUS_GET:
            status = usbTcdIsp1582FncInterruptStatusGet(
                      (pTRB_INTERRUPT_STATUS_GET_CLEAR) pHeader);
            break;

        case TCD_FNC_INTERRUPT_STATUS_CLEAR:
            status = usbTcdIsp1582FncInterruptStatusClear(
                      (pTRB_INTERRUPT_STATUS_GET_CLEAR) pHeader);
            break;

        case TCD_FNC_HANDLE_RESUME_INTERRUPT:
            status = usbTcdIsp1582FncHandleResumeInterrupt(
                      (pTRB_HANDLE_RESUME_INTERRUPT) pHeader);
            break;
        case TCD_FNC_HANDLE_RESET_INTERRUPT :
            status = usbTcdIsp1582FncHandleResetInterrupt(
                      (pTRB_HANDLE_RESET_INTERRUPT) pHeader);
            break;

        default:

            /* WindView Instrumentation */ 

            USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
            "usbTcdIsp1582EvalExec exiting: Wrong Function Code  ...",
            USB_TCD_ISP582_WV_FILTER);   

            USBISP1582_ERROR ("usbTcdIsp1582EvalExec : Bad Parameters...\n",
            0,0,0,0,0,0);
            status = ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
    "usbTcdIsp1582EvalExec exiting ...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582EvalExec : Exiting...\n",0,0,0,0,0,0);

    /* Return status */

    return status;
    }
    

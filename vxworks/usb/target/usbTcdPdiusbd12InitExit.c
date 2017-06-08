/* usbTcdPdiusbd12InitExit.c - Initialization/uninitialization for PDIUSBD12 TCD */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01e,17sep04,ami  WindView Instrumentation Changes
01d,02aug04,mta  Modification History Changes
01c,19jul04,ami  Coding Convention Changes
01b,04may04,pdg  Fix for warm reboot
01a,15mar04,mta First.
*/

/*
DESCRIPTION

This file implements the initialization and uninitialization modules of TCD
(Target Controller Driver) for the Philips PDIUSBD12.

This module exports a single entry point, usbTcdPdiusbd12EvalExec().  This is
the USB_TCD_EXEC_FUNC for this TCD.  The caller passes requests to the TCD by
constructing TRBs, or Target Request Blocks, and passing them to this entry
point.

TCDs are initialized by invoking the TCD_FNC_ATTACH function.  In response to
this function, the TCD returns information about the target controller,
including its USB speed, the number of endpoints it supports etc.

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
#include "usb/target/usbIsaLib.h"	      
#include "drv/usb/target/usbPdiusbd12Eval.h"	
#include "drv/usb/target/usbTcdPdiusbd12EvalLib.h"  
#include "drv/usb/target/usbPdiusbd12Tcd.h"     
#include "drv/usb/target/usbPdiusbd12Debug.h" 
#include "usb/target/usbPeriphInstr.h"     

/* include the .c files */

#include "usbTcdPdiusbd12Util.c"            
#include "usbTcdPdiusbd12DeviceControl.c"   
#include "usbTcdPdiusbd12Interrupt.c"       
#include "usbTcdPdiusbd12Endpoint.c"        
#include "rebootLib.h" 

/* globals */

UINT32	usbPdiusbd12Debug = 0;	/* Debug flag for usbPdiusbd12 */

/* forward declaration */

LOCAL VOID destroyTarget (pUSB_TCD_PDIUSBD12_TARGET pTarget);
LOCAL VOID usbTcdPdiusbd12Isr (pVOID param);
LOCAL STATUS usbTcdPdiusbd12FncAttach ( pTRB_ATTACH pTrb);
LOCAL STATUS usbTcdPdiusbd12FncDetach (pTRB_DETACH pTrb);
LOCAL STATUS usbTcdPdiusbd12FncEnable (pTRB_ENABLE_DISABLE pTrb);
LOCAL STATUS usbTcdPdiusbd12FncDisable (pTRB_ENABLE_DISABLE pTrb);

/* functions */

/******************************************************************************
*
* usbTcdPdiusbd12EvalExec - single entry point for PDIUSBD12 TCD
*
* This is the single entry point for the Philips PDIUSBD12 (ISA eval version)
* USB TCD (Target Controller Driver).  The function qualifies the TRB passed
* by the caller and fans out to the appropriate TCD function handler.
*
* RETURNS: OK or ERROR if failed to execute TRB passed by caller.
*
* ERRNO:
*   none.
*/

STATUS usbTcdPdiusbd12EvalExec
    (
    pVOID	pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    UINT32	status = OK;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12EvalExec entered ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12EvalExec: Entering...\n",0,0,0,0,0,0 );

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12EvalExec exiting: Bad Parameters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12EvalExec : Invalid parameters \
        ...\n",0,0,0,0,0,0 );
        return ERROR;
        }

    if (pHeader->function != TCD_FNC_ATTACH)
	{
	if ((pTarget = (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle) == NULL)
	    {

            /* WindView Instrumentation */
  
            USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
            "usbTcdPdiusbd12EvalExec exiting: Bad Parameters Received...",
            USB_TCD_PDIUSBD12_WV_FILTER);   

            USBPDIUSBD12_ERROR ("usbTcdPdiusbd12EvalExec: Invalid parameters \
            ... \n", 0,0,0,0,0,0);
           return ERROR;
	    }
        }

    /* Fan-out to appropriate function processor */

    switch (pHeader->function)
        {

        /* initialization and uninitialization function codes */

        case TCD_FNC_ATTACH:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncAttach \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncAttach((pTRB_ATTACH) pHeader);
	     break;

        case TCD_FNC_DETACH:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncDetach \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncDetach((pTRB_DETACH) pHeader);
             break;

        case TCD_FNC_ENABLE:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncEnable \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncEnable((pTRB_ENABLE_DISABLE) pHeader);
             break;

        case TCD_FNC_DISABLE:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncDisable \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncDisable((pTRB_ENABLE_DISABLE) pHeader);
             break;

        /* device control and status function codes */

        case TCD_FNC_ADDRESS_SET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncAddressSet \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncAddressSet((pTRB_ADDRESS_SET) pHeader);
             break;

        case TCD_FNC_SIGNAL_RESUME:
	    
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncSignalResume \n", 0,0,0,0,0,0 );
	     status=usbTcdPdiusbd12FncSignalResume((pTRB_SIGNAL_RESUME)pHeader);
             break;

        case TCD_FNC_CURRENT_FRAME_GET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncCurrentFrameGet \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncCurrentFrameGet
                                              ((pTRB_CURRENT_FRAME_GET)pHeader);
             break;

        /* endpoint related function codes */

        case TCD_FNC_ENDPOINT_ASSIGN:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncEndpointAssign \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncEndpointAssign(
                     (pTRB_ENDPOINT_ASSIGN) pHeader);
             break;

        case TCD_FNC_ENDPOINT_RELEASE:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncEndpointRelease \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncEndpointRelease(
                     (pTRB_ENDPOINT_RELEASE) pHeader);
             break;

        case TCD_FNC_ENDPOINT_STATE_SET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncEndpointStateSet \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncEndpointStateSet
                                            ((pTRB_ENDPOINT_STATE_SET)pHeader);
             break;

        case TCD_FNC_ENDPOINT_STATUS_GET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncStateGet \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncEndpointStatusGet
                                            ((pTRB_ENDPOINT_STATUS_GET)pHeader);
             break;

        case TCD_FNC_IS_BUFFER_EMPTY:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncIsBufferEmpty \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncIsBufferEmpty
                      ((pTRB_IS_BUFFER_EMPTY) pHeader);
             break;

        case TCD_FNC_COPY_DATA_FROM_EPBUF:
	     USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncCopyDataFromEpbuf \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncCopyDataFromEpBuf(
                      (pTRB_COPY_DATA_FROM_EPBUF) pHeader);
	     break;

        case TCD_FNC_COPY_DATA_TO_EPBUF:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncCopyDataToBuf \n", 0,0,0,0,0,0 );
             status = usbTcdPdiusbd12FncCopyDataToEpBuf(
                      (pTRB_COPY_DATA_TO_EPBUF) pHeader);
             break;

        /* interrupt related function codes */

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncEndpointIntStatusGet \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncEndpointIntStatusGet(
                     (pTRB_ENDPOINT_INTERRUPT_STATUS_GET) pHeader);
             break;

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_CLEAR:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncIntStatusClear \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncEndpointIntStatusClear(
                      (pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR) pHeader);
             break;

        case TCD_FNC_INTERRUPT_STATUS_GET:
             USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12EvalExec : Entering \
             usbTcdPdiusbd12FncGetInterruptStatus \n", 0,0,0,0,0,0 );
	     status = usbTcdPdiusbd12FncInterruptStatusGet(
                      (pTRB_INTERRUPT_STATUS_GET_CLEAR) pHeader);
             break;

        /* Following functions codes are not implemented by PDIUSB12 */

        case TCD_FNC_HANDLE_RESET_INTERRUPT      :
        case TCD_FNC_HANDLE_RESUME_INTERRUPT     :
        case TCD_FNC_HANDLE_SUSPEND_INTERRUPT    :
        case TCD_FNC_HANDLE_DISCONNECT_INTERRUPT :
        case TCD_FNC_INTERRUPT_STATUS_CLEAR      :
        case TCD_FNC_DEVICE_FEATURE_SET          :
	case TCD_FNC_DEVICE_FEATURE_CLEAR        : break;

        default:

            /* WindView Instrumentation */
  
            USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
            "usbTcdPdiusbd12EvalExec: Wrong Function Codes...",
            USB_TCD_PDIUSBD12_WV_FILTER);   

            USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12EvalExec : Functions \
            codes do not match...\n",0,0,0,0,0,0 );
            status = ERROR;
	    break;
	}

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12EvalExec exiting ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12EvalExec: Exiting...\n",0,0,0,0,0,0 );
    /* Return status */
    return status;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12Exit - function to be called on a reboot
*
* This function clears the Soft-Connect bit on a reboot.
*
* RETURNS: N/A
*
* ERRNO:
*   none.
*
* \NOMANUAL
*/

LOCAL VOID usbTcdPdiusbd12Exit
    (
    int	startType
    )
    {

    /* Disable the target controller */

    USB_ISA_BYTE_OUT ((volatile UINT32)
                    (D12EVAL_DEFAULT_IOBASE + D12EVAL_D12REG + D12_CMD_REG),
                    D12_CMD_SET_MODE);
    USB_ISA_BYTE_OUT ((volatile UINT32)
                    (D12EVAL_DEFAULT_IOBASE + D12EVAL_D12REG + D12_DATA_REG),
                     0);
    USB_ISA_BYTE_OUT ((volatile UINT32)
                    (D12EVAL_DEFAULT_IOBASE + D12EVAL_D12REG + D12_DATA_REG),
                     0);    
    }

/******************************************************************************
*
* usbTcdPdiusbd12FncAttach - function implementing function code TCD_FNC_ATTACH
*
* The purpose of this function is to initialize the Target Controller
* for Operation
*
* RETURNS: OK or ERROR if failed to initialize the Target Controller.
*
* ERRNO:
*   none.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncAttach
    (
    pTRB_ATTACH	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    pUSB_TCD_PDIUSBD12_PARAMS	pParams = NULL; /* USB_TCD_PDIUSBD12_PARAMS */
    UINT8	byte = 0 ;
    UINT8	i=0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncAttach entered ...", USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncAttach : Entered ...",
                          0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Bad Paramters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncAttach : Invalid Parameters...",
                             0,0,0,0,0,0 );
        return ERROR;
        }

    if ( (pTrb->tcdParam == NULL ) || (pTrb->usbHalIsr == NULL ) ||
         (pTrb->pHalDeviceInfo == NULL) || (pTrb->pDeviceInfo == NULL)  )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Bad Paramters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncAttach :Invalid Parameters...",
                              0,0,0,0,0,0 );
        return ERROR;
        }

    pParams = (pUSB_TCD_PDIUSBD12_PARAMS)pTrb->tcdParam;

    /* Create a USB_TCD_PDIUSBD12_TARGET structure to manage controller. */

    if ((pTarget = OSS_CALLOC (sizeof (*pTarget))) == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Fail to allocate memory...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncAttach : Could not allocate \
        memory ",0,0,0,0,0,0 );
        return ERROR;
        }

    /* Store the user supplied parameters */

    pTarget->ioBase = pParams->ioBase;
    pTarget->irq = pParams->irq;
    pTarget->dma = pParams->dma;

    /* Read the Chip ID and confirm that its PDIUSBD12 */

    if ((d12ReadChipId (pTarget) & D12_CHIP_ID_MASK) != D12_CHIP_ID)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Wrong Chip Id...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach: Chips Id \
        do not match ", 0,0,0,0,0,0);
        destroyTarget (pTarget);
        return ERROR;
        }

    /* disable address decoding */

    OUT_EVAL_GOUT (pTarget,0);		/* turn OFF LEDs & interrupts */

    /* Hook the function which needs to be called on a reboot */

    if(ERROR == rebootHookAdd((FUNCPTR)usbTcdPdiusbd12Exit))
       {

       /* WindView Instrumentation */

       USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
       "usbTcdPdiusbd12FncAttach exiting: Error hooking function for reboot...",
       USB_TCD_PDIUSBD12_WV_FILTER);   

       USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach: Not able \
       to hook a function on reboot ", 0,0,0,0,0,0);
       destroyTarget (pTarget);
       return ERROR;
       }

    /* Set Address to Zero */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Setting Device address \
    to 0 \n ",0,0,0,0,0,0);

    pTarget->deviceAddress = 0;

    OUT_D12_CMD (pTarget,D12_CMD_SET_ADDRESS);
    OUT_D12_DATA (pTarget,byte);

    /* Disable endpoints 1 and 2 */

    d12SetEndpointEnable (pTarget, 0);

    /* Clear Endpoint Fifo Buffers and endpoint status. */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Clearing Endpoints \n ",
    0,0,0,0,0,0);

    for (i = 0; i < D12_NUM_ENDPOINTS; i++)
	{
	d12SelectEndpoint (pTarget, i);
	d12ClearBfr (pTarget);
	d12ReadLastTransStatusByte (pTarget, i);
	}

    /* Clear the Interrupt Status Register  */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Clearing Interrupt \
    Status Register ...", 0,0,0,0,0,0);

    d12ReadIntReg (pTarget);

    /* Store HAL parameters */

    pTarget->usbHalIsr = pTrb->usbHalIsr;
    pTarget->usbHalIsrParam = pTrb->usbHalIsrParam;

    /* Hook the ISR */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Hooking \
    the ISR...\n",0,0,0,0,0,0);

#ifndef PDIUSBD12_POLLING
    if (USB_ISA_INT_CONNECT (usbTcdPdiusbd12Isr, pTarget, pTarget->irq)!= OK)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Error Hooking ISR...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncAttach : Hooking of ISR \
        Failed... " ,0,0,0,0,0,0);
        destroyTarget (pTarget);
        return ERROR;
        }
#else

    /* Create a thread for handling the interrupts */

    if (OSS_THREAD_CREATE((THREAD_PROTOTYPE)usbTcdPdiusbd12PollingIsr,
                          pTarget,
                          100,
                          "d12Thread",
                          &pTarget->threadId) != OK)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncAttach exiting: Error spawning the thread...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncAttach : Spawning of polling \
		ISR Failed... " ,0,0,0,0,0,0);
        destroyTarget (pTarget);
        return ERROR;
		}

#endif

    /* continue initializing hardware */

    /*
     * set basic operating mode
     *
     * NOTE: Setting the "clock running" bit keeps the chip alive even during
     * suspend in order to facilitate debugging.  In a production environment
     * where power device power consumption may be an issue, this bit should
     * not be set.
     */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Setting the Mode \
    Register...\n",0,0,0,0,0,0);

    pTarget->configByte = D12_CMD_SM_CFG_NO_LAZYCLOCK |
	D12_CMD_SM_CFG_CLOCK_RUNNING | D12_CMD_SM_CFG_MODE0_NON_ISO;

    pTarget->clkDivByte = D12_CMD_SM_CLK_DIV_DEFAULT |
	D12_CMD_SM_CLK_SET_TO_ONE;

    d12SetMode (pTarget);

    /*
     * Set default DMA mode
     *
     * NOTE: Originally when writing this code I set the PDIUSBD12 for
     * single-cycle DMA mode.  However, I noticed that the D12 would stop
     * asserting DRQ mid-transfer.  In examining the Philips evaluation code,
     * I noticed that they only use "burst 16" DMA mode, and that appears
     * to work correct.  -rcb
     */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Setting the \
    DMA Register ...\n",0,0,0,0,0,0);

    pTarget->dmaByte = D12_CMD_SD_DMA_BURST_16 |
	D12_CMD_SD_ENDPT_2_OUT_INTRPT | D12_CMD_SD_ENDPT_2_IN_INTRPT;

    d12SetDma (pTarget);

    /*
     * The following command enables interrupts.  Since we're using
     * an evaluation board with some debug LEDs, we also turn on an "LED"
     * to indicate that the board is configured.
     */

    pTarget->goutByte = ATTACH_LED | D12EVAL_GOUT_INTENB;
    OUT_EVAL_GOUT (pTarget,pTarget->goutByte);

    /* Return target information to caller */

    pTrb->pHalDeviceInfo->uNumberEndpoints = D12_NUM_ENDPOINTS;

    /* supports remote wake up */

    pTrb->pDeviceInfo->uDeviceFeature = USB_FEATURE_DEVICE_REMOTE_WAKEUP;

    /*
     * Setting the bit maps for usEndpointNumbr Bitmap
     * bits 0,1,2 are to be set for out endpoint,
     * bits 16,17,18 are to be set for in endpoint
     */

    pTrb->pDeviceInfo->uEndpointNumberBitmap = USB_PDIUSBD12_TCD_OUT_ENDPOINT |
                                               USB_PDIUSBD12_TCD_IN_ENDPOINT ;

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Updating the \
    Handle...\n",0, 0,0,0,0,0);

    /* update handle */

    pHeader->handle = pTarget;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncAttach exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncAttach : Exiting ...\
    \n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* destroyTarget - function releases target structure
*
* This function releases the resources allocated for a target controller
*
* RETURNS: N/A
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL VOID destroyTarget
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* USB_TCD_PDIUSBD12_TARGET */
    )
    {

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "destroyTarget entered...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("destroyTarget: Entering destroyTarget...\n",
                         0,0,0,0,0,0);

    if (pTarget)
        {

        /* Disable interrupts, turn off LEDs */

        if (pTarget->ioBase != 0)
            OUT_EVAL_GOUT (pTarget,0);

        if (pTarget->usbHalIsr)

#ifndef PDIUSBD12_POLLING 
            USB_ISA_INT_RESTORE (usbTcdPdiusbd12Isr, pTarget,pTarget->irq);
#else
            OSS_THREAD_DESTROY(pTarget->threadId);
#endif

        OSS_FREE (pTarget);
        }

    USBPDIUSBD12_DEBUG ("destroyTarget: Exiting \
    destroyTarget...\n", 0,0,0,0,0,0);
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncDetach - implements TCD_FNC_DETACH
*
* The purpose of this function is to shutdown the Target Controller
*
* RETURNS: OK or ERROR, if TCD is not able to detach.
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncDetach
    (
    pTRB_DETACH	pTrb			/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncDetach entered...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncDetach: Entered ...\n",
                         0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncDetach exiting: Bad Paramters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG (" usbTcdPdiusbd12FncDetach: Parameter Validation \
        is unsuccessful",0,0,0,0,0,0);
        return ERROR;
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* Disable the target controller */

    pTarget->configByte &= ~D12_CMD_SM_CFG_SOFTCONNECT;
    d12SetMode (pTarget);

    pTarget->goutByte &= ~ENABLE_LED;
    OUT_EVAL_GOUT (pTarget,pTarget->goutByte);

    destroyTarget(pTarget);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncDetach exiting...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncDetach: Exiting...\n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncEnable - implements TCD_FNC_ENABLE
*
* The purpose of this function is to enable the Target Controller
*
* RETURNS: OK or ERROR, if not able to enable the target controller.
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncEnable
    (
    pTRB_ENABLE_DISABLE	pTrb			/* TRB to executed */
    )
    {
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncEnable entered...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEnable: Entered...",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncEnable exiting: Bad Paramters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEnable: Parameter Validation \
        is unsuccessful",0,0,0,0,0,0);

        return ERROR;
        }

    /* Extract the target pointer from the handle */

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* enable soft connect */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncEnable : \
    Enabling Soft Connect...",0, 0,0,0,0,0);

    pTarget->configByte |= D12_CMD_SM_CFG_SOFTCONNECT;
    d12SetMode (pTarget);

    /* light LED indicating that target is enabled */

    pTarget->goutByte |= ENABLE_LED;
    OUT_EVAL_GOUT (pTarget,pTarget->goutByte);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncEnable exiting...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEnable: Exiting \
    ...",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncDisable - implements TCD_FNC_DISABLE
*
* The purpose of this function is to disable the Target Controller
*
* RETURNS: OK or ERROR, if not able to disable the target controller.
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncDisable
    (
    pTRB_ENABLE_DISABLE	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncDisable entered...",USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12FncDisable:Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL) )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
        "usbTcdPdiusbd12FncDisable exiting: Bad Paramters Received...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncDisable: Parameter Validation \
        is unsuccessful",0,0,0,0,0,0);
        return ERROR;
        }

    /* Extract the target pointer from the handle */

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    /* Disable controller */

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncDisable : Disabling the Soft \
    Connect...\n",0,0,0,0,0,0);

    pTarget->configByte &= ~D12_CMD_SM_CFG_SOFTCONNECT;
    d12SetMode (pTarget);

    pTarget->goutByte &= ~ENABLE_LED;
    OUT_EVAL_GOUT (pTarget,pTarget->goutByte);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INIT_EXIT,
    "usbTcdPdiusbd12FncDisable exiting...",USB_TCD_PDIUSBD12_WV_FILTER);   


    USBPDIUSBD12_DEBUG("usbTcdPdiusbd12FncDisable: Exiting \
    ...\n",0,0,0,0,0,0);

    return OK;
    }
    

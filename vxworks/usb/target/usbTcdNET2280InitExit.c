/* usbTcdNET2280InitExit.c - initialization/uninitialization for NET2280 TCD */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01h,19oct04,ami  #if 0 removed in usbTcdNet2280FncAttach function
01g,30sep04,pdg  Removed usbTcdNET2280Util.c
01f,29sep04,pdg  Include filename changed
01e,29sep04,ami  Changes for Mips
01d,20sep04,ami  NET2280 tested in High Speed
01c,17sep04,ami  After Control, Interrupt IN and Bulk OUT Testing
01b,08sep04,ami  Code Review Comments Incorporated
01a,04sep04,ami  First.
*/

/*
DESCRIPTION

This file implements the initialization and uninitialization modules of TCD
(Target Controller Driver) for the Netchip NET2280.

This module exports a single entry point, usbTcdNET2280Exec().  This is
the USB_TCD_EXEC_FUNC for this TCD.  The caller passes requests to the TCD by
constructing TRBs, or Target Request Blocks, and passing them to this entry
point.

TCDs are initialized by invoking the TCD_FNC_ATTACH function.  In response to
this function, the TCD returns information about the target controller,
including its USB speed, the number of endpoints it supports etc.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usbPciLib.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbNET2280.h,
               drv/usb/target/usbNET2280Tcd.h,
               drv/usb/target/usbTcdNET2280Lib.h,
               drv/usb/target/usbTcdNET2280Debug.h, rebootLib.h,
               usb/target/usbPeriphInstr.h

*/

/* includes */

#include "usb/usbPlatform.h"
#include "usb/ossLib.h"
#include "usb/usbPciLib.h"
#include "usb/target/usbHalCommon.h"
#include "usb/target/usbTcd.h"
#include "drv/usb/target/usbNET2280.h"
#include "drv/usb/target/usbNET2280Tcd.h"
#include "drv/usb/target/usbTcdNET2280Lib.h"
#include "drv/usb/target/usbTcdNET2280Debug.h"
#include "usb/target/usbPeriphInstr.h"
#include "rebootLib.h"

#include "usbTcdNET2280DeviceControl.c"
#include "usbTcdNET2280Endpoint.c"
#include "usbTcdNET2280Interrupt.c"

/* defines */
#define MAX_NET2280_CONTROLLERS 5

/* globals */

UINT32 usbNET2280Debug = 0;

UINT32 baseNET2280Addr[MAX_NET2280_CONTROLLERS] = {0,0,0,0,0};   

/******************************************************************************
*
* usbNET2280Exit - function to be called on a reboot
*
* This function clears the USBCTL register bit on a reboot.
*
* RETURNS: None.
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

LOCAL VOID usbNET2280Exit
    (
    int	startType
    )
    {
    
    UINT8	i = 0;
    for (i = 0; i < MAX_NET2280_CONTROLLERS; i++)
        {
         
        /* for all NET2280 controllers, reset the USB_CTL register */
 
        if (baseNET2280Addr [i] != 0)  

            *(volatile UINT32 *)(baseNET2280Addr [i] + NET2280_USBCTL_REG) = 0; 
        }
    } 


/******************************************************************************
*
* usbTcdNET2280FncAttach - function implementing function code TCD_FNC_ATTACH
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

LOCAL STATUS usbTcdNET2280FncAttach
    (
    pTRB_ATTACH		pTrb		/* TRB to be executed */
    )

    {

    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_PARAMS	pParams = NULL;	/* USB_TCD_NET2280_PARAMS */
    UINT32	data32 = 0;			/* temporary variable */
    UINT8	i = 0 ;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
    "usbTcdNET2280FncAttach entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncAttach : Entered...\n",0,0,0,0,0,0);

    /* Validate Parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncAttach exiting: Bad Parameter Received...",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncAttach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    if ( (pTrb->tcdParam == NULL ) || (pTrb->usbHalIsr == NULL ) ||
         (pTrb->pHalDeviceInfo == NULL) || (pTrb->pDeviceInfo == NULL)  )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncAttach exiting: Bad Parameter Received...",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncAttach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pParams = (pUSB_TCD_NET2280_PARAMS)pTrb->tcdParam;


    /*
     * Check if the base address[0] is valid.If not, the device is not located,
     * return an error in this case
     */

    if (pParams->ioBase[0] == 0)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncAttach exiting: Device not located...",
        USB_TCD_NET2280_WV_FILTER);

        
        USB_NET2280_ERROR ("usbTcdNET2280FncAttach : Device not located... \n",
        0,0,0,0,0,0);

        return ossStatus (S_usbTcdLib_HW_NOT_READY);
        }


    /* Create a pUSB_TCD_NET2280_TARGET structure to manage controller. */

    if ((pTarget = OSS_CALLOC (sizeof (*pTarget))) == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncAttach exiting: Memory Allocation Error...",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncAttach : Memory Allocation Error \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_OUT_OF_MEMORY);
        }


    /* Store the user supplied parameters */

    for (i = 0 ; i < NET2280_NO_OF_PCI_BADDR ; i++)
        pTarget->ioBase [i] = pParams->ioBase [i];

    pTarget->irq = pParams->irq;

    /* 
     * Store the base addresses in a vacant slot in the the global variable.
     * It is used to reset the USB_CTL register during warm reboot.
     */

    for (i = 0; i < MAX_NET2280_CONTROLLERS; i++)
        {
        if (baseNET2280Addr [i] == 0)
            
            baseNET2280Addr [i] = pTarget->ioBase [0];

        }  
        
     /* Hook the function which needs to be called on a reboot */

     if(ERROR == rebootHookAdd((FUNCPTR)usbNET2280Exit))
	{

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncAttach exiting: Enable to hook function for reboot...",
        USB_TCD_NET2280_WV_FILTER);   

        USB_NET2280_ERROR ( " usbTcdNET2280FncAttach: Not able \
        to hook a function on reboot ", 0,0,0,0,0,0);

        OSS_FREE (pTarget);
		return ERROR;
        }


    /* Initialize the DEVINIT regsiter. */    

    data32 = NET2280_DEVINIT_CLK_FREQ;

    data32 |= NET2280_DEVINIT_PCIEN | NET2280_DEVINIT_8051_RESET;

    NET2280_CFG_WRITE (pTarget, NET2280_DEVINIT_REG, data32);

    /* Reset the FIFO */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DEVINIT_REG);

    data32 |= NET2280_DEVINIT_FIFO_RESET | NET2280_DEVINIT_USB_RESET;
   
    NET2280_CFG_WRITE (pTarget, NET2280_DEVINIT_REG, data32);

    /* Disable auto-enumeration by setting all bits of STDRSP register to 0 */

    NET2280_CFG_WRITE (pTarget, NET2280_STDRSP_REG, 0);

    /* clear the interrupt status register */

    NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT0_REG, NET2280_IRQENB0_SETUP);

    NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG, NET2280_IRQENB1_REG_MASK);

 
    /*
     * Enable the interrupts for resume, suspend, root port reset, VBUS status
     * change, power state status change, parity error interrupt & PCI
     * Interrupt Enable
     */

    /* Suspend Interrupt needs to be handled */

    data32 = NET2280_IRQENB1_RESM | 
             NET2280_IRQENB1_RPRESET | NET2280_IRQENB1_VBUS |
             NET2280_IRQENB1_PSCINTEN | NET2280_IRQENB1_PCIPARITYERR |
             NET2280_IRQENB1_INTEN;


    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

 
    /*
     * Set the Bit USB Root Port Wakeup Eanble of USBCTL register to enable the
     * root part wake up condition to be detected 
     */


    data32 = NET2280_USBCTL_REG_RPWE;

    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG, data32);

    /* Store HAL parameters */

    pTarget->usbHalIsr = pTrb->usbHalIsr;
    pTarget->usbHalIsrParam = pTrb->usbHalIsrParam;

    /* Return target information to caller */

    pTrb->pHalDeviceInfo->uNumberEndpoints = NET2280_NUM_ENDPOINTS;

    /* supports remote wake up, test mode feature and is USB2 compliant */

    pTrb->pDeviceInfo->uDeviceFeature = USB_FEATURE_DEVICE_REMOTE_WAKEUP |
                                        USB_FEATURE_TEST_MODE |
                                        USB_FEATURE_USB20;
    /* Reset the dmaEot */
    pTarget->dmaEot = 0;

    /* supported endpoint bitmap */

    pTrb->pDeviceInfo->uEndpointNumberBitmap = USB_NET2280_TCD_ENDPOINT_BITMAP;


    /* Hook the ISR */

    if (usbPciIntConnect ((INT_HANDLER_PROTOTYPE)usbTcdNET2280Isr,
        (pVOID)pTarget, pTarget->irq)!= OK)
        {
        OSS_FREE (pTarget);
        USB_NET2280_ERROR ("usbTcdNET2280FncAttach :Error Hooking the ISR...\n",
        0,0,0,0,0,0);
        return ERROR;
        }

    /* update handle */

    pHeader->handle = pTarget;

    USB_NET2280_DEBUG ("usbTcdNET2280FncAttach : Exiting...\n",0,0,0,0,0,0);

    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280FncDetach - implements TCD_FNC_DETACH
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

LOCAL STATUS usbTcdNET2280FncDetach
    (
    pTRB_DETACH		pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* pUSB_TCD_NET2280_TARGET */
    UINT32	data32 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
   "usbTcdNET2280FncDetach entered...", USB_TCD_NET2280_WV_FILTER);


    USB_NET2280_DEBUG ("usbTcdNET2280FncDetach : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280FncDetach exiting: Bad Parameter received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncDetach : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Disable the interrupts by reset in PCI Interrupt Enable of PCIIRQENB1 */

    data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

    data32 &= ~NET2280_XIRQENB1_INTEN;

    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

    /* flush all the FIFO */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DEVINIT_REG);

    data32 |= NET2280_DEVINIT_FIFO_RESET;

    NET2280_CFG_WRITE (pTarget, NET2280_DEVINIT_REG, data32);

    /* Reset the USBCTL regsiter */

    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG,0);

    /* Un-hook the ISR */

    usbPciIntRestore ((INT_HANDLER_PROTOTYPE)usbTcdNET2280Isr, (pVOID)pTarget,
                       pTarget->irq);

    /* Release the USB_TCD_NET2280_TARGET */

    OSS_FREE (pTarget);

    USB_NET2280_DEBUG ("usbTcdNET2280FncDetach : Exiting...\n",0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280FncEnable - implements TCD_FNC_ENABLE
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

LOCAL STATUS usbTcdNET2280FncEnable
    (
    pTRB_ENABLE_DISABLE	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT32	data32 = 0;			/* temporary variable */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
    "usbTcdNET2280FncEnable entered ...", USB_TCD_NET2280_WV_FILTER);


    USB_NET2280_DEBUG ("usbTcdNET2280FncEnable : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdNET2280FncEnable exiting: Bad Parameter received",
        USB_TCD_NET2280_WV_FILTER);


        USB_NET2280_ERROR ("usbTcdNET2280FncEnable : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Set the USB DETECT ENABLE bit of USBCTL register */

    data32 = NET2280_CFG_READ (pTarget, NET2280_USBCTL_REG);

    data32 |= NET2280_USBCTL_REG_USBDE;

    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG, data32);

    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280FncDisable - implements TCD_FNC_DISBLE
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

LOCAL STATUS usbTcdNET2280FncDisable
    (
    pTRB_ENABLE_DISABLE	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT32	data32 = 0;			/* temporary variable */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
    "usbTcdNET2280FncDisable entered ...", USB_TCD_NET2280_WV_FILTER);


    USB_NET2280_DEBUG ("usbTcdNET2280FncDisable : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INIT_EXIT,
        "usbTcdNET2280FncDisable exiting: Bad Parameter received",
        USB_TCD_NET2280_WV_FILTER);


        USB_NET2280_ERROR ("usbTcdNET2280FncDisable : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;

    /* Reset the USB DETECT ENABLE bit of USBCTL register */

    data32 = NET2280_CFG_READ (pTarget, NET2280_USBCTL_REG);

    data32 &= ~NET2280_USBCTL_REG_USBDE;

    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG, data32);

    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280Exec - single Entry Point for NETCHIP 2280 TCD
*
* This is the single entry point for the NETCHIP 2280
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

STATUS usbTcdNET2280Exec
    (
    pVOID	pTrb			/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	status = OK;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
    "usbTcdNET2280Exec entered ...", USB_TCD_ISP582_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280Exec : Entered...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if (pHeader == NULL || pHeader->trbLength < sizeof (TRB_HEADER))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
        "usbTcdNET2280Exec exiting: Bad Paramters Received  ...",
        USB_TCD_ISP582_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280Exec : Bad Parameters...\n",
        0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    if (pHeader->function != TCD_FNC_ATTACH)
        {
        if ((pTarget = (pUSB_TCD_NET2280_TARGET) pHeader->handle) == NULL)
           {

           /* WindView Instrumentation */

           USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
           "usbTcdNET2280Exec exiting: Handle value is NULL...",
           USB_TCD_ISP582_WV_FILTER);

           USB_NET2280_ERROR ("usbTcdNET2280Exec : Bad Parameters...\n",
           0,0,0,0,0,0);
           return ossStatus (S_usbTcdLib_BAD_PARAM);
           }
        }

    USB_NET2280_DEBUG ("usbTcdNET2280Exec : Fucntion Code is %d...\n",
    pHeader->function,0,0,0,0,0);

    /* Fan-out to appropriate function processor */

    switch (pHeader->function)
        {

        /* Following functions codes are not implemented by ISP 1582 */

        case TCD_FNC_HANDLE_SUSPEND_INTERRUPT    :
        case TCD_FNC_HANDLE_RESUME_INTERRUPT :  break;

        /* initialization and uninitialization function codes */

        case TCD_FNC_ATTACH :
            status = usbTcdNET2280FncAttach((pTRB_ATTACH) pHeader);
    	    break;

        case TCD_FNC_DETACH:
    	    status = usbTcdNET2280FncDetach((pTRB_DETACH) pHeader);
            break;

        case TCD_FNC_ENABLE:
    	    status = usbTcdNET2280FncEnable((pTRB_ENABLE_DISABLE) pHeader);
            break;

        case TCD_FNC_DISABLE:
    	    status = usbTcdNET2280FncDisable((pTRB_ENABLE_DISABLE) pHeader);
            break;

        /* device control and status function codes */

        case TCD_FNC_ADDRESS_SET:
            status = usbTcdNET2280FncAddressSet((pTRB_ADDRESS_SET) pHeader);
            break;

        case TCD_FNC_SIGNAL_RESUME:
    	    status = usbTcdNET2280FncSignalResume((pTRB_SIGNAL_RESUME)pHeader);
            break;

        case TCD_FNC_CURRENT_FRAME_GET:
            status = usbTcdNET2280FncCurrentFrameGet(
                      (pTRB_CURRENT_FRAME_GET)pHeader);
            break;

        case TCD_FNC_DEVICE_FEATURE_SET:
            status = usbTcdNET2280FncDeviceFeatureSet(
                      (pTRB_DEVICE_FEATURE_SET_CLEAR)pHeader);
            break;

        case TCD_FNC_DEVICE_FEATURE_CLEAR :
            status = usbTcdNET2280FncDeviceFeatureClear(
                      (pTRB_DEVICE_FEATURE_SET_CLEAR) pHeader);
            break;


        /* endpoint related function codes */

        case TCD_FNC_ENDPOINT_ASSIGN:
            status = usbTcdNET2280FncEndpointAssign(
                      (pTRB_ENDPOINT_ASSIGN) pHeader);
            break;

        case TCD_FNC_ENDPOINT_RELEASE:
            status = usbTcdNET2280FncEndpointRelease(
                      (pTRB_ENDPOINT_RELEASE) pHeader);
            break;

        case TCD_FNC_ENDPOINT_STATE_SET:
            status = usbTcdNET2280FncEndpointStateSet(
                      (pTRB_ENDPOINT_STATE_SET)pHeader);
            break;

        case TCD_FNC_ENDPOINT_STATUS_GET:
            status = usbTcdNET2280FncEndpointStatusGet(
                      (pTRB_ENDPOINT_STATUS_GET)pHeader);
            break;

        case TCD_FNC_IS_BUFFER_EMPTY:
            status = usbTcdNET2280FncIsBufferEmpty(
                      (pTRB_IS_BUFFER_EMPTY) pHeader);
            break;

        case TCD_FNC_COPY_DATA_FROM_EPBUF:
	        status = usbTcdNET2280FncCopyDataFromEpBuf(
                      (pTRB_COPY_DATA_FROM_EPBUF) pHeader);
	        break;

        case TCD_FNC_COPY_DATA_TO_EPBUF:
            status = usbTcdNET2280FncCopyDataToEpBuf(
                      (pTRB_COPY_DATA_TO_EPBUF) pHeader);
            break;

        /* interrupt related function codes */

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET:
            status = usbTcdNET2280FncEndpointIntStatusGet(
                     (pTRB_ENDPOINT_INTERRUPT_STATUS_GET) pHeader);
            break;

        case TCD_FNC_ENDPOINT_INTERRUPT_STATUS_CLEAR:
            status = usbTcdNET2280FncEndpointIntStatusClear(
                      (pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR) pHeader);
            break;

        case TCD_FNC_INTERRUPT_STATUS_GET:
            status = usbTcdNET2280FncInterruptStatusGet(
                      (pTRB_INTERRUPT_STATUS_GET_CLEAR) pHeader);
            break;

        case TCD_FNC_INTERRUPT_STATUS_CLEAR:
            status = usbTcdNET2280FncInterruptStatusClear(
                      (pTRB_INTERRUPT_STATUS_GET_CLEAR) pHeader);
            break;

        case TCD_FNC_HANDLE_RESET_INTERRUPT :
            status = usbTcdNET2280FncHandleResetInterrupt(
                      (pTRB_HANDLE_RESET_INTERRUPT) pHeader);
            break;

        case TCD_FNC_HANDLE_DISCONNECT_INTERRUPT :
            status = usbTcdNET2280FncHandleDisconnectInterrupt(
                      (pTRB_HANDLE_DISCONNECT_INTERRUPT) pHeader);
            break;

        default:

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
            "usbTcdNET2280Exec exiting: Wrong Function Code  ...",
            USB_TCD_ISP582_WV_FILTER);

            USB_NET2280_ERROR ("usbTcdNET2280Exec : Bad Parameters...\n",
            0,0,0,0,0,0);
            status = ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INIT_EXIT,
    "usbTcdNET2280Exec exiting ...", USB_TCD_ISP582_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280Exec : Exiting...\n",0,0,0,0,0,0);

    /* Return status */

    return status;
    }





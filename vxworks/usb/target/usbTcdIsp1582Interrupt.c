/* usbTcdIsp1582Interrupt.c - defines modules for interrupt handling */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01g,17sep04,ami  WindView Instrumentation Changes
01f,02aug04,mta  Modification History Changes
01e,19jul04,ami Coding Convention Changes
01d,09jul04,mta Fixes based on pentium full speed testing
01c,30jun04,pdg Bug fixes - isp1582 full speed testing
01b,01jun04,mta Changes for Interrupt Mode
01a,21apr04,ami First
*/

/*
DESCRIPTION

This file implements the interrupt related functionalities of TCD
(Target Controller Driver) for the Philips ISP 1582.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usbPciLib.h, usb/usb.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbisp1582.h,
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

/*******************************************************************************
*
* usbTcdIsp1582Isr - isr of Isp 1582 TCD
*
* This is the ISR for the Isp 1582 TCD.
*
* RETURNS: N/A
*
* ERRNO:
*   none
*
* \NOMANUAL
*/

VOID usbTcdIsp1582Isr
    (
    pVOID	param			/* ISR Paramter */	
    )
    {
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */ 
    UINT16			data16 = 0 ;

    USBISP1582_DEBUG ("usbTcdIsp1582Isr: Entered...\n",0,0,0,0,0,0);

    if ( param == NULL )
        {
        USBISP1582_ERROR ("usbTcdIsp1582Isr : Bad Parameters...\n",0,0,0,0,0,0);
        return;
        }

    pTarget = (pUSB_TCD_ISP1582_TARGET) param;

    /* Disable the Control Port */

    USB_PCI_WORD_OUT((pTarget->ioBase + ISP1582EVAL_CNTL_PORT), ~(0x80));

    /* Disable all interrupts by reseting bit GLINTENA of mode register */

    /* Read the mode regsiter and then reset GLINTENA bit */

    data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);

    data16 &= ~ISP1582_MODE_REG_GLINTENA;

    /* Write to mode register to disable interrupts */

    isp1582Write16 (pTarget , ISP1582_MODE_REG ,data16 & ISP1582_MODE_REG_MASK);

    /* Call the HAL ISR */

    (*pTarget->usbHalIsr)(pTarget->usbHalIsrParam);

    /* Enable Interrupts */

    data16 |= ISP1582_MODE_REG_GLINTENA;

    isp1582Write16 (pTarget ,ISP1582_MODE_REG , data16 & ISP1582_MODE_REG_MASK);

    /* Enable the Control Port */

    USB_PCI_WORD_OUT((pTarget->ioBase + ISP1582EVAL_CNTL_PORT) , 0x80);

    USBISP1582_DEBUG ("usbTcdIsp1582Isr: Exiting...\n",0,0,0,0,0,0);
    return;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncInterruptStatusGet - to get the interrupt status
*
* This function returns the interrupt status i.e whether the reset interrupt,
* suspend interrupt, resume interrupt, disconnect interrupt or endpoint
* related interrupt is pending.
*
* RETURNS : OK or ERROR, if the interrupt status is not retrieved successfully.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncInterruptStatusGet
    (
    pTRB_INTERRUPT_STATUS_GET_CLEAR	pTrb	/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */	
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT32	data32 = 0;			
    UINT16	data16 = 0;
    UINT8	data8 = 0;
    UINT32	endpointIntMask = 0;		/* endpoint interrupt mask */

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncInterruptStatusGet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Entered...\n",
    0,0,0,0,0,0);


    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncInterruptStatusGet : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Reset uInterruptStatus */

    pTrb->uInterruptStatus = 0;

    /* Read the Interrupt Status Register */

    data32 = isp1582Read32 ( pTarget , ISP1582_INT_REG);

    /*
     * While reading interrupt register always and it with the interrupt
     * Enable Register.
     */

    data32 &= isp1582Read32 ( pTarget , ISP_1582_INT_ENABLE_REG);

    /* If bit 0 is set then its a Bus Reset Interrupt. */

    if (( data32 & ISP1582_INT_REG_BRST) != 0)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Reset Interrupt \
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: Reset Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   


        /* Reinilialize the registers */

        /* Mode Register */

        data16 = isp1582Read16 (pTarget , ISP1582_MODE_REG);
        data16 |= ISP1582_MODE_REG_CLKAON;
        data16 |= ISP1582_MODE_REG_POWRON;
        data16 |= ISP1582_MODE_REG_DMACLKON;
        data16 |= ISP1582_MODE_REG_GLINTENA;
        
        isp1582Write16 (pTarget , ISP1582_MODE_REG , data16);

        /* write 0 to OTG register */

        isp1582Write8(pTarget,ISP_1582_OTG_REG, 0);

        /* Interrupt Configuration Register */

        data8 = ((ISP1582_INT_CONF_REG_CDBGMOD_SHIFT(
                  ISP1582_INT_CONF_REG_CDBGMOD_ACK_ONLY)) |
                 (ISP1582_INT_CONF_REG_DDBGMODIN_SHIFT(
                  ISP1582_INT_CONF_REG_DDBGMODIN_ACK_ONLY)) |
                 (ISP1582_INT_CONF_REG_DDBGMODOUT_SHIFT(
                  ISP1582_INT_CONF_REG_DDBGMODOUT_ACK_NYET)));

        isp1582Write8 (pTarget , ISP1582_INT_CONFIG_REG , data8);

        /* Interrupt Enable Register */

        isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG ,
                       (ISP1582_INT_ENABLE_REG_IEBRST | 
                        ISP1582_INT_ENABLE_REG_IESUSP |
              ISP1582_INT_ENABLE_REG_IERESM ));

        pTrb->uInterruptStatus |= USBTCD_RESET_INTERRUPT_MASK;
        }

    /* If bit 3 is set then Suspend event has occured */

    if (( data32 & ISP1582_INT_REG_SUSP) != 0)
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: Suspend Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Suspend Interrupt\
        ...\n",0,0,0,0,0,0);
        pTrb->uInterruptStatus |= USBTCD_SUSPEND_INTERRUPT_MASK;
        }

    /* If bit 4 is set then Resume event has occured */

    if (( data32 & ISP1582_INT_REG_RESM) != 0)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Resume Interrupt\
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: Resume Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   


        pTrb->uInterruptStatus |= USBTCD_RESUME_INTERRUPT_MASK;
        }

    /* if bit 6 is set then DMA related event has occured */

    if (( data32 & ISP1582_INT_REG_DMA) != 0)
        {
        UINT8 endpointIndex = 0;
        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: DMA Interrupt \
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: DMA Related Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   

        pTarget->dmaEot = TRUE;
        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;
        endpointIndex = pTarget->dmaEndpointId;
        pTarget->endptIntPending |= (1 << endpointIndex);
        }

    /*
     * If bit 8 is set then setup interrupt has occured. Set setupIntPending
     * to TRUE
     */

    if (( data32 & ISP1582_INT_REG_EP0SETUP) != 0)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Setup Interrupt \
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: Setup Packet Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   

        pTarget->setupIntPending = TRUE;
        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;
        }

    /*
     * If any of bit from 10 to 25 is set, endpoint related intrrupt has
     * occured.
     */

    if ((endpointIntMask = (data32 & ISP1582_INT_ENDPT_MASK)) != 0)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Endpoint \
        Interrupt...\n",0,0,0,0,0,0);
 
        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusGet: Endpoint Related Event Occured...",
        USB_TCD_ISP582_WV_FILTER);   

        /* Update endptPending accordingly */

        pTarget->endptIntPending |= (endpointIntMask >>
                                     ISP1582_INT_REG_ENDPT_SFT_VAL);
        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncInterruptStatusGet exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncInterruptStatusClear - implements TCD_FNC_INTERRUPT_CLEAR
*
* This function clears the interrupt status bits.
*
* RETURNS : OK or ERROR, if the interrupt status is not cleared successfully.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncInterruptStatusClear
    (
    pTRB_INTERRUPT_STATUS_GET_CLEAR	pTrb	/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL; /* USB_TCD_ISP1582_TARGET */
    UINT32	data32 = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncInterruptStatusClear entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusClear: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncInterruptStatusClear exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncInterruptStatusClear : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;


    /*
     * Check whether interrupt is pending on reset event, suspend event,
     * resume event or endpoint. If so, clear appropiate bits in interrupt
     * register. If the interrupt is pending on any endpoint, clear for all
     * the endpoints on which interrupts are pending. Also clears the
     * dma and set interrutps.
     */

    if ((pTrb->uInterruptStatus & USBTCD_RESET_INTERRUPT_MASK) != 0)
        {

        /* Reset event has occured. Clear bit 0 on interrupt register */

        data32 |= ISP1582_INT_REG_BRST;
        }

    if ((pTrb->uInterruptStatus & USBTCD_SUSPEND_INTERRUPT_MASK) != 0)
        {

        /* Suspend event has occured. Clear bit 3 on interrupt register */

        data32 |= ISP1582_INT_REG_SUSP;
        }

    if ((pTrb->uInterruptStatus & USBTCD_RESUME_INTERRUPT_MASK) != 0)
        {

        /* Resume event has occured. Clear bit 4 on interrupt register */

        data32 |= ISP1582_INT_REG_RESM;

        }

    if ((pTrb->uInterruptStatus & USBTCD_ENDPOINT_INTERRUPT_MASK) != 0)
        {

        /* Check whether endpoint Interppt is pending */

        if (pTarget->setupIntPending)
            data32 |= ISP1582_INT_REG_EP0SETUP;

        /* Check whether endpoint Interrupt is pending */

        if (pTarget->dmaEot)
            data32 |= ISP1582_INT_REG_DMA;

        /* 
         * Endpoint event has occured. Reset inturrupts for endpoint which
         * are captured in endptPending member
         */

        if (pTarget->endptIntPending != 0)
            data32 |= ISP1582_INT_REG_ENDPT_SET (pTarget->endptIntPending);

        }

    USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusGet: Interrupt to be \
    cleared is %d...\n",data32,0,0,0,0,0);

  if (data32 != 0)
        {
    	isp1582Write32 (pTarget , ISP1582_INT_REG , data32);
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncInterruptStatusClear exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncInterruptStatusClear: Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncEndpointIntStatusGet - implements TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET
*
* This function returns the interrupt status on an endpoint. It also checks
* whether the particular endpoint on which interrupt occured is stalled or not.
*
* RETURNS: OK or ERROR, if not able to get the endpoint interrupt status.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncEndpointIntStatusGet
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_GET	pTrb		/* Trb to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;		/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	    /* USB_TCD_ISP1582_TARGET */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    UINT8	endpointIndex = 0;			/* endpoint index */
    UINT8	direction = 0;				/* direction */
    BOOL	endptPending = FALSE;	     /* endpoint interrupt is pending */
    
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncEndpointIntStatusGet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointIntStatusGet: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle ==0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncEndpointIntStatusGet exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointIntStatusGet : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;
    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;

    /* Determine the endpoint index and direction */

    endpointIndex = pEndpointInfo->endpointIndex;
    direction = pEndpointInfo->direction;

    /* Determine if setup interrupt is pending on the control OUT endpoint */

    if ((pTarget->endptIntPending & (1 << endpointIndex)) != 0)
        {
        if ((pEndpointInfo->isDoubleBufSup))
            {

            /* Initialize the endpoint index register */

            isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

            if (((direction == USB_ENDPOINT_IN) &&
                ((isp1582Read8 (pTarget,ISP1582_BUF_STATUS_REG) &
                  ISP1582_BUF_STATUS_REG_MASK) == 
                  ISP1582_BUF_STATUS_REG_BOTH_EMPT)) ||
               ((direction == USB_ENDPOINT_OUT) &&
                ((isp1582Read8 (pTarget ,ISP1582_BUF_STATUS_REG) &
                  ISP1582_BUF_STATUS_REG_MASK) == 
                  ISP1582_BUF_STATUS_REG_ONE_FILL)))

                /* Interrupt is pending on that endpoint. Update endptPending */

                pTarget->endptIntPending &= ~(1 << endpointIndex);
            }    
        else

            /* Interrupt is pending on that endpoint. Update endptPending */

            pTarget->endptIntPending &= ~(1 << endpointIndex);

        /* Interrupt is pending on that endpoint */

        endptPending = TRUE;
        }

    else if ((endpointIndex == ISP1582_ENDPT_0_RX) &&
        (pTarget->setupIntPending))
        endptPending = TRUE;

    if (endptPending)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointIntStatusGet: Interrupt \
        Pending on Endpoint %d...\n",endpointIndex,0,0,0,0,0);

        /*
         * Interrupt is pending on the endpoint. Set bit 0 of
         * uEndptInterruptStatus saying that endpoint interrupt is pending.
         */

        pTrb->uEndptInterruptStatus = USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK;

        /* Determine the direction and update bits 2 & 3 accordingly */

        if (direction == USB_ENDPOINT_OUT)
            {
            if ((pTarget->setupIntPending) && 
                (endpointIndex == ISP1582_ENDPT_0_RX))

                /* Setup Interrupt is pending, update bit 1  */

                pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_SETUP_PID_MASK;
            else
                pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_OUT_PID_MASK;
            }
        else
            {
            pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_IN_PID_MASK;

            /*
             * If this is the last IN interrupt for the data stage and the
             * status stage is to be initiated, then initiate a status stage
             */
            
            if ((endpointIndex == ISP1582_ENDPT_0_TX) &&
                pTarget->controlOUTStatusPending)
                { 
                UINT8 data = 0;

                /* Select OUT endpoint for initiating a status stage */

                isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG ,
                                                         ISP1582_ENDPT_0_RX);

                /* Initiate the status stage  */

                data = isp1582Read8 (pTarget , ISP1582_CNTL_FUNC_REG);
                isp1582Write8 (pTarget , ISP1582_CNTL_FUNC_REG ,
                                          data | ISP1582_CNTL_FUNC_REG_STATUS);

                pTarget->controlOUTStatusPending = FALSE;
                }         
            }

        /*
         * Determine whether the endpoint is stalled or not by reading the
         * Control Function Register. If so update bit 17 of
         * uEndptInterruptStatus.
         */

        /* Initialize the endpoint index register */

        isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

        /* Read endpoint control function register */

        if ((isp1582Read8 (pTarget , ISP1582_CNTL_FUNC_REG) &
                                             ISP1582_CNTL_FUNC_REG_STALL) != 0)
            {

            /* Endpoint is stalled */

            pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_STALL_ERROR;
            }
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncEndpointIntStatusGet exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointIntStatusGet: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncEndpointIntStatusClear - implements TCD_FNC_ENDPOINT_INTERRUPT_STATUS_CLEAR
*
* This function clears the interrupt on an endpoint.
*
* RETURNS: OK or ERROR, if not able to clear the endpoint interrupt status.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
*  Bad paramter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncEndpointIntStatusClear
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR	pTrb	/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;		/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	direction = 0;			/* direction */
    
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncEndpointIntStatusClear entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointIntStatusClear: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle ==0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncEndpointIntStatusClear exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointIntStatusClear : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;
    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;

    /* Determine the endpoint index and direction */

    endpointIndex = pEndpointInfo->endpointIndex;
    direction = pEndpointInfo->direction;

    /* Determine if setup interrupt is pending on the control OUT endpoint */

    if ((endpointIndex == ISP1582_ENDPT_0_RX) &&
        (pTarget->setupIntPending))
        {
        
        /* Setup Interrupt is pending. Update setupIntPending */

        pTarget->setupIntPending = FALSE;
        }

        /* Check whether there is a DMA Eot for the current endpoint */

        if ((pTarget->dmaEndpointId == endpointIndex) &&
            (pTarget->dmaEot) && (pTarget->dmaInUse))
            {

            /* If the transfer type is IN, disable the DMA controller */

            if (direction == USB_ENDPOINT_IN)
                {
                UINT32 data32 = 0;
                UINT8 i = 0;

                /* Reset the DMA Interrupt Enable Register */

                isp1582Write16 (pTarget , ISP1582_DMA_INT_ENBL_REG , 0);

                /* Read the interrupt enable register */

                data32 = isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG);

                /*
                 * Enable that endpoint by writing into interrupt enable
                 * register
                 */
                
                data32 |=  ISP1582_INT_ENABLE_REG_ENDPT_SET(endpointIndex);
                isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG ,data32);

                /* Initialize the DMA Endpoint Register to endpoint not used. */

                isp1582Write8 (pTarget , ISP1582_DMA_ENDPT_REG ,
                                                     pTarget->dmaEndptNotInUse);

                /* Set dmaEot & dmaInUse to False */

                pTarget->dmaEot = FALSE;
                pTarget->dmaInUse = FALSE;

                /* Set dmaEndpointId of USB_TCD-ISP1582_TARGET structure */

                pTarget->dmaEndpointId = i;

                /* Clear the dma interrupt reason register */

                isp1582Write16 (pTarget , ISP1582_DMA_INT_RESN_REG ,
                                ISP1582_DMA_INT_RESN_MASK);
                }
            }

     if ((endpointIndex == ISP1582_ENDPT_0_RX) &&
        pTarget->controlINStatusPending)
        {
        UINT8 data = 0;

        /* Select IN endpoint for initiating a status stage */

        isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG ,
                                 ISP1582_ENDPT_0_TX);

        /* Initiate the status stage  */

        data = isp1582Read8 (pTarget , ISP1582_CNTL_FUNC_REG);
        isp1582Write8 (pTarget , ISP1582_CNTL_FUNC_REG ,
                                             data | ISP1582_CNTL_FUNC_REG_STATUS);

        pTarget->controlINStatusPending = FALSE;
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncEndpointIntStatusClear exiting...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointIntStatusClear: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncHandleResumeInterrupt - implements TCD_FNC_HANDLE_RESUME_INTERRUPT
*
* This fucntion is called whenever resume interrupt has occured.
*
* RETURNS: OK or ERROR if any.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad paramter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncHandleResumeInterrupt
    (
    pTRB_HANDLE_RESUME_INTERRUPT	pTrb	/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */ 

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncHandleResumeInterrupt entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncHandleResumeInterrupt: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) || 
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncHandleResumeInterrupt exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncHandleResumeInterrupt : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Unlock the device address by writing code 0xAA37 */

    isp1582Write16 (pTarget,ISP1582_UNLOCK_DEV_REG,ISP1582_UNLOCK_DEV_REG_CODE);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncHandleResumeInterrupt exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncHandleResumeInterrupt: Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncHandleResetInterrupt - implements TCD_FNC_HANDLE_RESET_INTERRUPT
*
* This fucntion is called whenever reset interrupt has occured.
*
* RETURNS: OK or ERROR if any.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad paramter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdIsp1582FncHandleResetInterrupt
    (
    pTRB_HANDLE_RESET_INTERRUPT	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT32	data32 = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncHandleResetInterrupt entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncHandleResetInterrupt: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncHandleResetInterrupt exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncHandleResetInterrupt : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget = (pUSB_TCD_ISP1582_TARGET) pHeader->handle;

    /* Read the interrupt enable register */

    data32 = isp1582Read32(pTarget , ISP_1582_INT_ENABLE_REG);

    /* Enable the setup interrupt */

    data32 |= ISP1582_INT_ENABLE_REG_IEP0SETUP;

    /* Set appropiate bit of Interrupt Enable Register */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , data32);

    /* Delay for High Speed Status Set */

    OSS_THREAD_SLEEP (1);

    /*
     * Read the interrupt status register. If bit HS_STAT is set, device
     * is operating at High Speed.
     */

    if ((isp1582Read32 (pTarget , ISP1582_INT_REG) & ISP1582_INT_REG_HS_STA) != 0)
        {

        pTrb->uSpeed =  USB_TCD_HIGH_SPEED;

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncHandleResetInterrupt: High Speed Detected...",
        USB_TCD_ISP582_WV_FILTER);   


        /* Clear the High Speed Status Change */

        isp1582Write32 (pTarget , ISP1582_INT_REG , ISP1582_INT_REG_HS_STA);
        }
    else
        {
        pTrb->uSpeed = USB_TCD_FULL_SPEED;

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
        "usbTcdIsp1582FncHandleResetInterrupt: Full Speed Detected...",
        USB_TCD_ISP582_WV_FILTER);   

        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_INTERRUPT,
    "usbTcdIsp1582FncHandleResetInterrupt exiting...", USB_TCD_ISP582_WV_FILTER);   

    return OK;
    }


#ifdef ISP1582_POLLING
/******************************************************************************
*
* usbTcdIsp1582PollingIsr - polling ISR of ISP 1582 TCD
*
* This is the ISR for the ISP 1582 TCD which runs in polling mode
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL VOID usbTcdIsp1582PollingIsr
    (
    pVOID	param			/* polling isr paramter */
    )
    {
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */ 

    pTarget = (pUSB_TCD_ISP1582_TARGET)param;

    while(1)
        {
        if ((isp1582Read32 (pTarget , ISP1582_INT_REG) &
            (isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG)))!= 0)
            usbTcdIsp1582Isr(param);
        OSS_THREAD_SLEEP (1);
        }
    return;
    }
#endif


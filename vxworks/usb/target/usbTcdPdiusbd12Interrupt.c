/* usbTcdPdiusbd12Interrupt.c - Defines modules for interrupt handling */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01a,17sep04,ami  WindView Instrumentation Changes
01b,11may04,hch  merge after D12 driver testing.
01b,19jul04,ami  Coding Convnetion Changes
01a,15mar04,ami  First

*/

/*
DESCRIPTION

This module implements the various hardware dependent features of PDIUSBD12
which are related to interrupt handling.

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

/*******************************************************************************
*
* usbTcdPdiusbd12Isr - isr of pdiusbd12 TCD
*
* This is the ISR for the pdiusbd12 TCD
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL VOID usbTcdPdiusbd12Isr
    (
    pVOID	param			/* ISR Parameter */
    )
    {
    pUSB_TCD_PDIUSBD12_TARGET pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */

    if ( param == NULL )
        {
        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12Isr : Invalid Parameter...\n",
                            0,0,0,0,0,0);
        return;
        }

    pTarget = (pUSB_TCD_PDIUSBD12_TARGET) param;

    /* 
     * Is there an interrupt pending ?
     *
     * NOTE: INT_N is an active low signal.  The interrupt is asserted
     * when INT_N == 0.
     */

    USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12Isr : Entered the ISR...\n",0,0,0,
    0,0,0);

    if ((IN_EVAL_GIN (pTarget) & D12EVAL_INTN) == 0)
        {

        /* A interrupt is pending. Disable interrupts  */

        USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12Isr : Disabling Interrupts...\n",
        0,0,0,0,0,0);

	pTarget->goutByte &= ~D12EVAL_GOUT_INTENB;
	OUT_EVAL_GOUT (pTarget,pTarget->goutByte);

	/* Call the HAL ISR callback */

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12Isr : Calling the HAL Isr... \n",
        0,0,0,0,0,0 );
	
	(*pTarget->usbHalIsr)(pTarget->usbHalIsrParam);

        /* Enable the interrupts */
        
        USBPDIUSBD12_DEBUG ( " usbTcdPdiusbd12Isr : Enabling  Interrupts...\n",
        0,0,0,0,0,0);

        pTarget->goutByte |= D12EVAL_GOUT_INTENB;
	OUT_EVAL_GOUT (pTarget,pTarget->goutByte);
        }

     return;
     }

#ifdef PDIUSBD12_POLLING
/*******************************************************************************
*
* usbTcdPdiusbd12PollingIsr - polling ISR of pdiusbd12 TCD
*
* This is the ISR for the pdiusbd12 TCD which runs in polling mode.
*
* RETURNS: N/A
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL VOID usbTcdPdiusbd12PollingIsr
    (
    pVOID	param			/* ISR Polling Parameter */
    )
    {
    while(1)
        {
        usbTcdPdiusbd12Isr(param);
        OSS_THREAD_SLEEP (1);
        }
    return;
    }
#endif


/*******************************************************************************
*
* usbTcdPdiusbd12FncEndpointIntStatusGet -  implements TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET
*
* This function returns the interrupt status of an endpoint.
*
* RETURNS : OK or ERROR, if not able to get the interrupt status
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncEndpointIntStatusGet
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_GET	pTrb	/* Trb to be executed */
    )
    {
    BOOL	endptIntPending = FALSE;    /* endpoint interrupt is pending */	
    UINT8	readLastStatusByte = 0;	    /* last status byte */		
    UINT8	readErrorByte = 0;	    /* error byte */			
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */ 
    pUSB_TCD_PDIUSBD12_ENDPOINT pEndpoint = NULL;
						/* USB_TCD_PDIUSBD12_ENDPOINT */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncEndpointIntStatusGet entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet: Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncEndpointIntStatusGet exiting:Bad Parameters Received",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncEndpointIntStatusGet : \
        Invalid parameters ...\n",0,0,0,0,0,0 );
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;
    pEndpoint = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;

    /* Detemine whether interrupt is pending on this endpoint */

    if (pTarget->endptIntPending != 0)
        {
        if(((pTarget->endptIntPending >> pEndpoint->endpointIndex) & 0x01) != 0)
            {
            /* Clear the interrupt pending bit of the endpoint */ 
            pTarget->endptIntPending &= ~(0x01 << pEndpoint->endpointIndex);
            endptIntPending = TRUE;
            }
        }

    /* Interrupt is pending on that endpoint */

    if (endptIntPending)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncEndpointIntStatusGet Interrupt pending on Endpoint",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        /* Updating uEndptInterruptStatus flag */

        pTrb->uEndptInterruptStatus = 0;
        pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK;

        /* Reading Last Status Byte Register */

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet: Reading \
        Last Status Byte Register...\n",0,0,0,0,0,0);

        /* Read the value which is stored in the data structure */

        readLastStatusByte = 
                      pTarget->uLastTransactionStatus[pEndpoint->endpointIndex];

        /* Clear the value in the data structure */

        pTarget->uLastTransactionStatus[pEndpoint->endpointIndex] &= 
                                                            ~readLastStatusByte;

        /* No Error */ 

        if ((readLastStatusByte & D12_CMD_RLTS_DATA_SUCCESS) != 0)
            {

            /* check whether setup packet is received */

            if ((readLastStatusByte & D12_CMD_RLTS_SETUP_PACKET) != 0)
                {

                /* WindView Instrumentation */

                USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
                "usbTcdPdiusbd12FncEndpointIntStatusGet: Setup Packet Received",
                USB_TCD_PDIUSBD12_WV_FILTER);   

                USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet: \
                Setup Packet is received \n",0,0,0,0,0,0);

                /* update uEndptInterruptStatus flag */

                pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_SETUP_PID_MASK;

                /* Acknowledge the setup token */

                USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet: \
                Acknowledge the Setup Packet received \n",0,0,0,0,0,0);

                /* Set the flag indicating that the setup ERP is pending */

                pTarget->setupErpPending = TRUE; 
                }

            else
                {
                if ( pEndpoint->direction == USB_ENDPOINT_OUT )
                    pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_OUT_PID_MASK ;
                else
                    pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_IN_PID_MASK ;
                }

            pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_TRANSFER_SUCCESS;
            }
        else
            {

            /*
             * To read the error codes --
             * if any, or with pTrb->uEndptInterruptStatus
             */

            /* Masking and then shifting the bits to retrieve the error codes */

    	    readErrorByte = D12_CMD_RLTS_ERROR_CODE (readLastStatusByte) ;

    	    switch (readErrorByte)
                {
                case D12_CMD_RLTS_ERROR_DATA_TOGGLE  :
                case D12_CMD_RLTS_ERROR_PID_ENCODE   :
                case D12_CMD_RLTS_ERROR_PID_UNKNOWN  :
                case D12_CMD_RLTS_ERROR_EXPECTED_PKT :

                    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    PID Mismatch Error...\n",0,0,0,0,0,0);

            		pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_PID_MISMATCH ;
          		    break;

                case D12_CMD_RLTS_ERROR_TOKEN_CRC :
                case D12_CMD_RLTS_ERROR_DATA_CRC  :

                    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    Token CRC Error...\n",0,0,0,0,0,0);

            		pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_CRC_ERROR;
                    break;

                case D12_CMD_RLTS_ERROR_TIME_OUT :

            		USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet:\
                            Time Out Error...\n",0,0,0,0,0,0);
            		pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_TIMEOUT_ERROR;
                    break;

                case D12_CMD_RLTS_ERROR_END_OF_PKT :

                    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    End of  Packet Error...\n",0,0,0,0,0,0);
            		pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_COMMN_FAULT;
                    break;

                case D12_CMD_RLTS_ERROR_STALL :

                    USBPDIUSBD12_DEBUG("usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    Endpoint Stall Error...\n",0,0,0,0,0,0);
            		pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_STALL_ERROR;
                    break;

                case D12_CMD_RLTS_ERROR_OVERFLOW :

                    USBPDIUSBD12_DEBUG("usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    OverFlow Error...\n",0,0,0,0,0,0);

                    pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_DATA_OVERRUN;
                    break;

                case D12_CMD_RLTS_ERROR_BITSTUFF :

                    USBPDIUSBD12_DEBUG("usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    Bit Stuff Error...\n",0,0,0,0,0,0);

                    pTrb->uEndptInterruptStatus |=
                                                USBTCD_ENDPOINT_BIT_STUFF_ERROR;
                    break;

                default :
            		USBPDIUSBD12_ERROR("usbTcdPdiusbd12FncEndpointIntStatusGet:\
                    Wrong Error Code received...\n",0,0,0,0,0,0);
                }
            }
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncEndpointIntStatusGet exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   


    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointIntStatusGet: Exiting \
    ...\n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncEndpointIntStatusClear - to clear the endpoint interrupt
*
* This function clear the interrupt status of the endpoint.
*
* RETURNS : OK or ERROR, if not able to clear the endpoint interrupt status.
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncEndpointIntStatusClear
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR	pTrb	/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;		/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;   /* USB_TCD_PDIUSBD12_TARGET */
    pUSB_TCD_PDIUSBD12_ENDPOINT pEndpoint = NULL;/*USB_TCD_PDIUSBD12_ENDPOINT */

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointIntStatusClear: Entered \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncEndpointIntStatusClear entered...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncEndpointIntStatusClear exiting:Bad Parameters \
        Received", USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncEndpointIntStatusClear : \
        Invalid parameters ...\n",0,0,0,0,0,0 );
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;
    pEndpoint = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle ;

    /* Check for DMA EOT */

    if (pTarget->dmaEot && pTarget->dmaInUse &&
       (pEndpoint->endpointIndex == pTarget->dmaEndpointId))
        {
        if (pEndpoint->direction == USB_ENDPOINT_IN) /* Check this */
            {

            /* To disable the DMA  */

            pTarget->dmaByte &= ~D12_CMD_SD_DMA_ENABLE;

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointIntStatusClear: \
            Disabling the DMA...\n",0,0,0,0,0,0);

            d12SetDma (pTarget);
            }

        pTarget->dmaEot = FALSE;
       	pTarget->dmaInUse = FALSE;

        /* Reseting the endpoint id */

        pTarget->dmaEndpointId = 0;
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncEndpointIntStatusClear exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointIntStatusClear: \
    Exiting ...",0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncInterruptStatusGet - to get the interrupt status
*
* This function returns the interrupt status i.e whether the reset interrupt,
* suspend interrupt, resume interrupt, disconnect interrupt or endpoint
* related interrupt is pending.
*
* RETURNS : OK or ERROR, if the interrupt status is not retrieved successfully.
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncInterruptStatusGet
    (
    pTRB_INTERRUPT_STATUS_GET_CLEAR	pTrb	/* TRB to be executed */
    )
    {
    UINT16	readIntByte = 0;		/* Interrupt byte */
    pTRB_HEADER pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    UINT8	ginByte = 0;			
    UINT8	uIndex = 0;


    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncGetInterruptStatus entered...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncInterruptStatusGet exiting:Bad Parameters \
        Received", USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( " usbTcdPdiusbd12FncGetInterruptStatus : \
        Invalid parameters ...\n",0,0,0,0,0,0 );
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_PDIUSBD12_TARGET) pHeader->handle;

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : Reading the \
    Interrupt Status Register ...\n",0,0,0,0,0,0);

    /* read Interrupt Status Register */

    readIntByte = d12ReadIntReg (pTarget);

    /* Reseting the uInterruptStatus flag */

    pTrb->uInterruptStatus = 0;

    /* Checking for the type of interrupt occured */

    if ((readIntByte & D12_CMD_RIR_ENDPOINT_INT_MASK) != 0 )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncInterruptStatusGet: Endpoint related event occured",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : \
        Endpoint Interrupt has occured...\n",0,0,0,0,0,0);

        /* Store the endpoint whcih generated the interrupt */

        pTarget->endptIntPending |= (readIntByte & D12_CMD_RIR_ENDPOINT_INT_MASK);

        /* Update uInterruptStatus */

        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK ;

        /*
         * Read the last transaction status of the endpoints on which interrupts
         * are pending. This is done as the interrupt status bits are cleared
         * only on reading the last transaction status. The reason for storing
         * is because the contents are lost once the last transaction
         * status is read.
         */ 

        for (uIndex = 0; uIndex < D12_NUM_ENDPOINTS; uIndex++)
            {
            if ((readIntByte & (0x01 << uIndex)) != 0)
                pTarget->uLastTransactionStatus[uIndex] |= 
                                   d12ReadLastTransStatusByte(pTarget , uIndex);
            }
        }

    if ((readIntByte & D12_CMD_RIR_BUS_RESET) != 0)
        {
        ginByte = IN_EVAL_GIN (pTarget);

        /* Follow-up by determining if Vbus is present or not */

        if ((ginByte & D12EVAL_BUS_POWER) == 0)
            {
            pTrb->uInterruptStatus |= USBTCD_DISCONNECT_INTERRUPT_MASK;
     	    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : \
      	          Device is disconnected...\n",0,0,0,0,0,0);
            }
        else
            {
    	    pTrb->uInterruptStatus |= USBTCD_RESET_INTERRUPT_MASK ;
     	    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : Bus \
      	          Reset has occured...\n",0,0,0,0,0,0);
            }

        }

    /*  Suspend/Resume event has occured */

    if ((readIntByte & D12_CMD_RIR_SUSPEND) != 0)
        {
        UINT8  gInByte = IN_EVAL_GIN (pTarget);

        if ((gInByte & D12EVAL_SUSPEND) == 0) 
            {

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
            "usbTcdPdiusbd12FncInterruptStatusGet: Resume Event",
            USB_TCD_PDIUSBD12_WV_FILTER);   

	    /* Resume event has occured */	

            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : \
            Resume Event has occured...\n",0,0,0,0,0,0);
            pTrb->uInterruptStatus |= USBTCD_RESUME_INTERRUPT_MASK;
            }
        else              
            {

            /* Suspend event has occured */

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
            "usbTcdPdiusbd12FncInterruptStatusGet: Suspend Event",
            USB_TCD_PDIUSBD12_WV_FILTER);   

            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : \
            Suspend Event has occured...\n",0,0,0,0,0,0);
            pTrb->uInterruptStatus |= USBTCD_SUSPEND_INTERRUPT_MASK;
            }
        }

    /* Check for Dma Event */

    if ((readIntByte & D12_CMD_RIR_DMA_EOT) != 0)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
        "usbTcdPdiusbd12FncInterruptStatusGet: DMA Related Event",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus : DMA \
        Event has occured...\n",0,0,0,0,0,0);
        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;
        pTarget->dmaEot = TRUE;

        /* 
         * Set the bit indicating that an interrupt is pending on 
         * the endpoint.
         */   

        pTarget->endptIntPending |= (0x01 << pTarget->dmaEndpointId); 

        /*
         * Set the bit indicating that the dma transfer is 
         * success on the endpoint.
         */

        pTarget->uLastTransactionStatus[pTarget->dmaEndpointId] |= 
                                                 D12_CMD_RLTS_DATA_SUCCESS;

        d12ReadLastTransStatusByte(pTarget , pTarget->dmaEndpointId);
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_INTERRUPT,
    "usbTcdPdiusbd12FncGetInterruptStatus exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncGetInterruptStatus :Exiting \
    ...\n",0,0,0,0,0,0);

    return OK;
    }
    

/* usbTcdNET2280Interrupt.c - defines modules for interrupt handling */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01i,30sep04,pdg  DMA testing fixes
01h,29sep04,ami  Mips related Changes
01g,23sep04,pdg  Fix for short packet handling and not losing a setup
                 interrupt.
01f,22sep04,pdg  Fix for setting the address
01e,21sep04,pdg  Full speed testing fixes
01d,20sep04,ami  NET2280 tested for High Speed
01c,17sep04,ami  After Control, Interrupt IN and Bulk OUT Testing   
01b,08sep04,ami  Code Review Comments Incorporated
01a,03sep04,ami Written
*/

/*
DESCRIPTION

This file implements the interrupt related functionalities of TCD
(Target Controller Driver) for the Netchip NET2280

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usb.h,
               usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbNET2280.h,
               drv/usb/target/usbNET2280Tcd.h,
               drv/usb/target/usbTcdNET2280Debug.h
               usb/target/usbPeriphInstr.h
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

/*******************************************************************************
*
* usbTcdNET2280Isr - isr of NET2280 TCD
*
* This is the ISR for the NET2280 TCD.
*
* RETURNS: N/A
*
* ERRNO:
*   none
*
* \NOMANUAL
*/

LOCAL VOID usbTcdNET2280Isr
    (
    pUSB_TCD_NET2280_TARGET	pTarget		/* ISR Parameters */
    )

    {
    UINT32	data32 = 0 ;			/* temporary register */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280Isr entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280Isr: Entered...\n",0,0,0,0,0,0);

    if ( pTarget == NULL )
        {
        USB_NET2280_ERROR("usbTcdNET2280Isr : Bad Parameters...\n",0,0,0,0,0,0);
        return;
        }

    /* Disable the Interrupts. Reset bit 31 of PCIIRQENB1 register */

    data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

    data32 &= ~NET2280_XIRQENB1_INTEN;

    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);


    /* Call the HAL ISR */

    (*pTarget->usbHalIsr)(pTarget->usbHalIsrParam);

    /* Enable the Interrupts. Set bit 31 of PCIIRQENB1 register */

    data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

    data32 |= NET2280_XIRQENB1_INTEN;

    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

    USB_NET2280_DEBUG ("usbTcdNET2280Isr: Exiting...\n",0,0,0,0,0,0);

    return;
    }

/*******************************************************************************
*
* usbTcdNET2280FncInterruptStatusGet - to get the interrupt status
*
* This function returns the interrupt status i.e whether the reset interrupt,
* suspend interrupt, resume interrupt, disconnect interrupt or endpoint
* related interrupt is pending. It also carries out the handling of certain
* interrpts which are not intimated to the HAL.
*
* RETURNS: OK or ERROR, if the interrupt status is not retrieved successfully
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncInterruptStatusGet
    (
    pTRB_INTERRUPT_STATUS_GET_CLEAR pTrb	/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	data32 = 0;			/* temporary variable */
    UINT8	i = 0;

#ifdef NET2280_DMA_SUPPORTED

    UINT32	tempData = 0;			/* Temporary variable */
#endif
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncInterruptStatusGet entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Entered...\n",
    0,0,0,0,0,0);


    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet exiting: Bad Parameter Received...",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncInterruptStatusGet: \
        Bad Parameters...\n",0,0,0,0,0,0);

        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;


    /* Reset uInterruptStatus */

    pTrb->uInterruptStatus = 0;

    /*
     * Read IRQSTAT0 register to determine the whether any endpoint related
     * interrupts have occured
     */

    data32 = NET2280_CFG_READ (pTarget, NET2280_IRQSTAT0_REG);

    /* Mask the unwanted interrupts */

    data32 &= NET2280_CFG_READ(pTarget,NET2280_PCIIRQENB0_REG);
  
    /* If bit 7 is set, setup interrupt has occured */

    if ((data32 & NET2280_IRQENB0_SETUP) != 0)
        {

        /* To store the value of interruptEnable register */

        UINT32 intEnableValue = 0;

        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Setup Interrupt\
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet: Setup Event Occured...",
        USB_TCD_NET2280_WV_FILTER);

        pTarget->setupIntPending = TRUE;

        /*
         * Disable the setup interrupt, till we handle the setup. This will
         * be enabled in the endpoint interrupt status clear.
         */
        intEnableValue = NET2280_CFG_READ(pTarget,NET2280_PCIIRQENB0_REG);

        intEnableValue &= ~NET2280_IRQENB0_SETUP;

        NET2280_CFG_WRITE (pTarget,NET2280_PCIIRQENB0_REG, intEnableValue);

        /* Set the endpoint interrupt mask value */

        pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;

        }

    /* Check if any endpoint specific interrupts have occured */
 
    if ((data32 & NET2280_IRQENB0_EPMASK) != 0) 

        {

        /* If bits 0:6 are set, endpoint related interrupt has occured */

        for (i = NET2280_ENDPT_0_OUT; i <= NET2280_ENDPT_F; i++)
            {

            if ((data32 & NET2280_IRQENB0_EP(i)) != 0)
                { 

	        UINT32	dataEpStat = 0; 

                /* set bit 4 if TRB :: uInterruptStatus */

                pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;


                /*
                 * Read the EP_STAT regsiter for the endpoint and determine the 
                 * type of interrupt that has occured
                 */

                dataEpStat = NET2280_CFG_READ (pTarget, 
                             NET2280_EP_STAT_OFFSET(i));

                /*
                 * If the endpoint is not NET2280_ENDPT_0, update endptIntPending and 
                 * clear the particular bit in EP_STAT regsiter,
                 * else determine the type of interrupt on endpoint 0, update 
                 * endptIntPending accordingle and clear the EP_STAT register
                 */ 

                if (i != NET2280_ENDPT_0_OUT)

                  {
               
                  pTarget->endptIntPending |= (1 << i);

                  /* Clear the EP_STAT register */

                  dataEpStat = (NET2280_EP_STAT_DPT | NET2280_EP_STAT_DPR |
                                                      NET2280_EP_STAT_SPOD);

                  NET2280_CFG_WRITE (pTarget,  NET2280_EP_STAT_OFFSET(i), 
                                   dataEpStat);                               
                  }
   
                else   
                
                    {

                    if ((dataEpStat & NET2280_EP_STAT_DPT) != 0)
                        {
                        /* 
                         * Check if the address is to be set. If this
                         * condition succeeds, then the data packet
                         * transmitted interrupt has occured on the
                         * status stage completion of the setAddress
                         * standard request. So we write to OURADDR register
                         * to set the new address.
                         */
                        if (pTarget->addressTobeSet != 0)
                            {
                           
                            NET2280_CFG_WRITE(pTarget, NET2280_OURADDR_REG, 
                                             pTarget->addressTobeSet);
                           
                            /* Reset the addressTobeSet value */
                            pTarget->addressTobeSet = 0;

                            } 

                        /* Control In Interrupt. Set bit 7 of endptIntPending */
 
                        pTarget->endptIntPending |= (1 << NET2280_ENDPT_0_IN);

                        /* Clear the EP_STAT regsiter */

                        NET2280_CFG_WRITE (pTarget,  NET2280_EP_STAT_OFFSET(i), 
                                   NET2280_EP_STAT_DPT);
                        }

                    if ((dataEpStat & NET2280_EP_STAT_DPR) != 0)
                        {

                        if (pTarget->setupIntPending == FALSE)
                            {

                            /* Clear the EP_STAT regsiter */

                            NET2280_CFG_WRITE (pTarget,  NET2280_EP_STAT_OFFSET(i), 
                                               dataEpStat & NET2280_EP_STAT_DPR);

                            }
                        else

                            {

                            UINT32 dataTemp = 0;  

                            dataTemp = NET2280_CFG_READ (pTarget, 
                                                   NET2280_EP_IRQENB_OFFSET(i));

                            dataTemp &= ~NET2280_EP_STAT_DPR;

                            NET2280_CFG_WRITE(pTarget,
                                          NET2280_EP_IRQENB_OFFSET(i),dataTemp);                          
                            }


                        /*
                         * Disable the control status interrupt. We enable this in 
                         * usbTcdNET2280FncCopyDataToEpbuf, when the callback calls this 
                         * function for status stage. This ensures that we have handled 
                         * Data OUT stage before handling the Control Status stage
                         */
                                  
                        dataEpStat = NET2280_CFG_READ (pTarget, 
                                                       NET2280_PCIIRQENB1_REG);

                        dataEpStat &= ~NET2280_XIRQENB1_CS;

                        NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, 
                                           dataEpStat);                                     

               		pTarget->statusINPending = TRUE;	

                        /* Control Out Interrupt. Set bit 0 of endptIntPending */

                        pTarget->endptIntPending |= (1 << NET2280_ENDPT_0_OUT);

                        }
                    }
                }
            }
    	}


    /* Read IRQSTAT1 register */

    data32 = NET2280_CFG_READ (pTarget, NET2280_IRQSTAT1_REG);

    /* Mask the unwanted interrupts */

    data32 &= NET2280_CFG_READ(pTarget,NET2280_PCIIRQENB1_REG );

    /* reset interrupt */

    if ((data32 & NET2280_IRQENB1_RPRESET) != 0)
        {


        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Reset Interrupt\
        ...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet: Reset Event Occured...",
        USB_TCD_NET2280_WV_FILTER);

    	/* set bit 1 of TRB :: uInterruptStatus */

	pTrb->uInterruptStatus |= USBTCD_RESET_INTERRUPT_MASK;
        }

    /* disconnect interrupt */

    if ((data32 & NET2280_IRQENB1_VBUS) != 0)
        {

        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Change in VBUS \
        occured...\n",0,0,0,0,0,0);

        /* Read the USBCTL register and determine VBUS status */

        if ((NET2280_CFG_READ (pTarget, NET2280_USBCTL_REG) &
             NET2280_USBCTL_REG_VBUSPIN) == 0)

            {

            USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Disconnect \
            Interrupt...\n",0,0,0,0,0,0);

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
            "usbTcdNET2280FncInterruptStatusGet: Disconnect Event Occured...",
            USB_TCD_NET2280_WV_FILTER);

            /* set bit 0 of TRB :: uInterruptStatus */

     	    pTrb->uInterruptStatus |= USBTCD_DISCONNECT_INTERRUPT_MASK;

            }

        else
            {


            /* Reset the VBUS Interrupt */

            NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG,
                               NET2280_IRQENB1_VBUS);
            }
        }

    /* control status interrupt */


    if ((data32 & NET2280_IRQENB1_CS) != 0)
        {

        /* To temporarily store the setupData */
 
        UINT32 setupData = 0;

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet: Control Status Stage:.",
        USB_TCD_NET2280_WV_FILTER);   


        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Control Status \
        Interrupt...\n",0,0,0,0,0,0);

        /* Read the first 4 bytes of the setup packet */

        setupData = NET2280_CFG_READ (pTarget, NET2280_SETUP0123_REG);

        /* Check if it is for set address request */

        if ((setupData & 0xFF00) == 0x0500)
            {

            /* The most significant 16 bits give the address value to be set */            
            setupData >>= 16;

            /*
             * This value will be written on a status stage completion.
             * So set the Force Immediate bit to update the address value
             * immediately.
             */
            setupData |= NET2280_OURADDR_REG_FI;

            /* Store the data in pTarget data structure */

            pTarget->addressTobeSet = setupData;
            }  

        /*
         * control status interrupt has occured. We need to clear the Control
         * Status handshake bit of Endpoint Response Register for endpoint 0.
         * This bit is automatically set a SETUP Packet is detected. Clearing
         * it will result a proper response returned to the host
         */

        if ((NET2280_CFG_READ (pTarget, NET2280_EP_RSP_OFFSET (NET2280_ENDPT_0_OUT)) 
             & NET2280_EP_RSP_CSPH) != 0) 
  
             {

             /* Clear bit 3 of Endpoint Response Register */

             NET2280_CFG_WRITE (pTarget, NET2280_EP_RSP_OFFSET (NET2280_ENDPT_0_OUT),
                           NET2280_EP_RSP_CSPH);

             }

        /* Clear the Control Interrupt Status bit */

        NET2280_CFG_WRITE (pTarget,NET2280_IRQSTAT1_REG, NET2280_IRQENB1_CS);
        }

    /* suspend interrupt */

    if ((data32 &  NET2280_IRQENB1_SUSP) != 0)
        {

        /* SUSPEND EVENT HANDLING IS AN OPEN ISSUE .. NEED to BE HANDLED */

        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Suspend \
        Interrupt...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet: Suspend Event Occured...",
        USB_TCD_NET2280_WV_FILTER);

        /* Set bit 2 of TRB :: uInterruptStatus */

        pTrb->uInterruptStatus |= USBTCD_SUSPEND_INTERRUPT_MASK;
        }

    /* resume event */

    if ((data32 & NET2280_IRQENB1_RESM) != 0)

        {

        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Resume \
        Interrupt...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusGet: Resume Event Occured...",
        USB_TCD_NET2280_WV_FILTER);

        /* Set bit 2 of TRB :: uInterruptStatus */

        pTrb->uInterruptStatus |= USBTCD_RESUME_INTERRUPT_MASK;
        }

#ifdef NET2280_DMA_SUPPORTED

    /* dma related event */

    for (i = NET2280_ENDPT_A; i <= NET2280_ENDPT_D; i++)
        {
    	if ((data32 & NET2280_IRQENB1_DMA(i)) != 0)
            {

            /* Read the contents of EP_CFG register */

            tempData = NET2280_CFG_READ (pTarget, 
                                          NET2280_EP_CFG_OFFSET(i));

            /* Check if the dma is completed on an IN endpoint */

            if ((tempData & NET2280_EP_CFG_DIRECTION) != 0)
                {

                /* Clear the data packet transmitted interrupt */

                NET2280_CFG_WRITE (pTarget,
                                   NET2280_EP_STAT_OFFSET(i), 
                                   NET2280_EP_STAT_DPT);        

 
                /* Read the contents of EP_STAT register */
 
                tempData =  NET2280_CFG_READ (pTarget, 
                                          NET2280_EP_STAT_OFFSET(i));

                /* 
                 * There are chances that the EOT is happenned, but the
                 * FIFO still has some data to be sent to the host.
                 * If this is the case, then this cannot be treated as 
                 * a completion of IN transfer. If FIFO is empty, then
                 * IN transfer is completed. So, set the flag indicating 
                 * a transfer completion
                 */
                if ((tempData & NET2280_EP_STAT_FIFO_EMPTY) != 0)
                    {
                    /* Set appropiate bit in pTarget :: dmaEot */

                    pTarget->dmaEot |= (1 << i);

                    /* set bit 4 if TRB :: uInterruptStatus */

                    pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;
 
                    }
                 else
                     /*
                      * The transfer is yet to be completed. The handling should be
                      * endpoint interrupt handling section.
                      */
                    {
                    /* Enable back the data packet transmitted interrupt */

                    NET2280_CFG_WRITE (pTarget,
                                       NET2280_EP_IRQENB_OFFSET(i), 
                                       NET2280_EP_IRQENB_DPT);        
                    
                    }
                }   

            else
                {
                /* Set appropiate bit in pTarget :: dmaEot */

                pTarget->dmaEot |= (1 << i);

                /* set bit 4 if TRB :: uInterruptStatus */

                pTrb->uInterruptStatus |= USBTCD_ENDPOINT_INTERRUPT_MASK;

                }

            /* Clear the dma interrupt */

            NET2280_CFG_WRITE (pTarget, 
                  NET2280_DMASTAT_OFFSET(i),
                  NET2280_DMASTAT_TD_INT);
            }
        }

#endif
    /* Power State Change Mode */

    if ((data32 & NET2280_IRQENB1_PSCINTEN) != 0)
        {

        /* Clear this bit in IRQSTAT1 register */

        NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG,
                           NET2280_IRQENB1_PSCINTEN);

        }

    /* PCI Parity Error */

    if ((data32 & NET2280_IRQENB1_PCIPARITYERR) != 0)
        {
        /* To be implemented */

        /* Clear this bit in IRQSTAT1 register */

        NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG,
                           NET2280_IRQENB1_PCIPARITYERR);

        }

    USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusGet: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/******************************************************************************
*
* usbTcdNET2280FncInterruptStatusClear - implements TCD_FNC_INTERRUPT_CLEAR
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

LOCAL STATUS usbTcdNET2280FncInterruptStatusClear
    (
    pTRB_INTERRUPT_STATUS_GET_CLEAR pTrb	/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL; /* USB_TCD_NET2280_TARGET */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT (USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncInterruptStatusClear entered.",USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncInterruptStatusClear exiting:Bad Param Received..\n",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncInterruptStatusClear : \
        Bad Parameters...\n",0,0,0,0,0,0);

        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;


    /*
     * Check whether interrupt is pending on disconnect event, reset event,
     * suspend event, resume event or endpoint. If so, clear appropiate bits
     * in interrupt register. If the interrupt is pending on any endpoint,
     * clear for all the endpoints on which interrupts are pending.
     * Also clears the dma and set interrutps.
     */

    /* disconncet event */

    if ((pTrb->uInterruptStatus & USBTCD_DISCONNECT_INTERRUPT_MASK) != 0)
        {

         USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear:Clear VBUS \n",
         0,0,0,0,0,0);

        /* clear VBUS Interrupt */

        NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG, NET2280_IRQENB1_VBUS);
        }

    /* reset event */

    if ((pTrb->uInterruptStatus & USBTCD_RESET_INTERRUPT_MASK) != 0)
        {

        USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear:Clearing \
        resetBit \n", 0,0,0,0,0,0);

        /* clear Root Port Reset Interrupt Interrupt */

        NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG,
                           NET2280_IRQENB1_RPRESET);
        }

    /* suspend event */

    if ((pTrb->uInterruptStatus & USBTCD_SUSPEND_INTERRUPT_MASK) != 0)
        {

	/* open issue */

        }

    /* resume event */

    if ((pTrb->uInterruptStatus & USBTCD_RESUME_INTERRUPT_MASK) != 0)
        {

     	/* clear resume interrupt bit */

         USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear: \
         Clearing Resume Interrupt bit\n", 0,0,0,0,0,0);


         NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT1_REG,
                            NET2280_IRQENB1_RESM);
        }


    /* endpoint related event */

    if ((pTrb->uInterruptStatus & USBTCD_ENDPOINT_INTERRUPT_MASK) != 0)
        {

        if (pTarget->setupIntPending)
            {

    	    /* Setup Interrupt Pending */

            USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear: Clearing \
             Setup Interrupt bit\n", 0,0,0,0,0,0);


            NET2280_CFG_WRITE (pTarget, NET2280_IRQSTAT0_REG,
                               NET2280_IRQENB0_SETUP);
            }


        if ((pTarget->endptIntPending) != 0)
            {

            /*
             * Endpoint related event is pending on some endpoint. Clear all
             * endpoint interrupt on which endpoint related events are pending
             */

            }
        }

    USB_NET2280_DEBUG ("usbTcdNET2280FncInterruptStatusClear: Exiting...\n",
                        0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdNET2280FncEndpointIntStatusGet - implements TCD_FNC_ENDPOINT_INTERRUPT_STATUS_GET
*
* This function returns the interrupt status on an endpoint.
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


LOCAL STATUS usbTcdNET2280FncEndpointIntStatusGet
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_GET pTrb		/* TRB to be executed */
    )

    {

    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;		/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	    /* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    UINT8	endpointIndex = 0;			/* endpoint index */
    UINT8	direction = 0;				/* direction */
    UINT32      data32 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncEndpointIntStatusGet entered.", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointIntStatusGet: Entered...\n",
    0,0,0,0,0,0);


    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncEndpointIntStatusGet exiting:Bad Parameter Received.",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointIntStatusGet: \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;
    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;

    /* Determine the endpoint index and direction */

    endpointIndex = pEndpointInfo->endpointIndex;
    direction = pEndpointInfo->direction;

    /* update TRB :: uEndptInterruptStatus to 0 */

    pTrb->uEndptInterruptStatus = 0;

    if ((endpointIndex == NET2280_ENDPT_0_OUT) && 
        (pTarget->setupIntPending))
        {

        /* setup interrupt */
        /* Set bits 0 and bit 1 of TRB :: uEndptInterruptStatus ot 1 */

        pTrb->uEndptInterruptStatus |=  USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                                        USBTCD_ENDPOINT_SETUP_PID_MASK;
        return OK;

        }

#ifdef NET2280_DMA_SUPPORTED

    /* Check if an interrupt has occured for dma endpoints A, B, C or D  */
 
    if ((endpointIndex == NET2280_ENDPT_A) ||
         (endpointIndex == NET2280_ENDPT_B) ||
         (endpointIndex == NET2280_ENDPT_C) ||
         (endpointIndex == NET2280_ENDPT_D))
        {
        /* Check if there is a dma eot interrupt for this endpoint */

        if ((pTarget->dmaEot & (1 << endpointIndex)) != 0)

            {

            /* read the direction bit of DMACNT register */

            if ((NET2280_CFG_READ (pTarget, 
                                   NET2280_DMACOUNT_OFFSET (endpointIndex))
                                   & NET2280_DMACOUNT_DIR) != 0)
                {

                /*
                 * DMA Transfer occured on IN endpoint, update
                 * TRB :: uEndptInterruptStatus accordingly
                 */

                pTrb->uEndptInterruptStatus |=
                                        USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                                        USBTCD_ENDPOINT_IN_PID_MASK;
                }

            else
                {

                 /*
                  * DMA Transfer occured on OUT endpoint, update
                  * TRB :: uEndptInterruptStatus accordingly
                  */

                 pTrb->uEndptInterruptStatus |=
                                            USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                                            USBTCD_ENDPOINT_OUT_PID_MASK;
                }
            return OK;
            }
        /* 
         * This condition will be successful in the following cases
         * DMA OUT : short packet is received.
         * DMA IN : Dma transfer completed before USB data transfer.
         */
        else if (((pTarget->endptIntPending & (1 << endpointIndex)) != 0) &&
                  pEndpointInfo->dmaInUse)
            {

            if (direction == USB_TCD_ENDPT_OUT)
                {
                pTrb->uEndptInterruptStatus = 
                USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                USBTCD_ENDPOINT_OUT_PID_MASK |
                USBTCD_ENDPOINT_DATA_UNDERRUN;

                pTarget->dmaEot |= (1 << endpointIndex); 

                pTarget->endptIntPending &= ~(1 << endpointIndex); 

                /* Read the endpoint interrupt status */

                data32 = NET2280_CFG_READ (pTarget, 
                              NET2280_EP_STAT_OFFSET(endpointIndex));  

                /* Short packet will be cleared in copydatafrom() */
 
                data32 &= ~NET2280_EP_STAT_NAKOUT;

                /* Clear all the interrupts */

                NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex),
                                        data32);  
                }
            else /* Endpoint direction is IN */
                {

                pTrb->uEndptInterruptStatus = 
                    USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                    USBTCD_ENDPOINT_IN_PID_MASK;

                pTarget->dmaEot |= (1 << endpointIndex); 

                pTarget->endptIntPending &= ~(1 << endpointIndex); 

                }

            return OK;
            }

       }

#endif
    if ((pTarget->endptIntPending & (1 << endpointIndex)) != 0)
        {


        /* 
         * Check for direction and update TRB :: uEndptInterruptStatus with
         * proper mask 
         */

        if (direction == USB_TCD_ENDPT_IN)
            {

            /* IN INTERRUPT on the endpoint */

            pTrb->uEndptInterruptStatus |=
                                       USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                                       USBTCD_ENDPOINT_IN_PID_MASK;

            /* Clear the endptIntPending bit */
            pTarget->endptIntPending &= ~(1 << endpointIndex); 

            /*
             * Endpoint 0 IN and OUT use the same register. 
             * So update the index to be used for register access.
             */ 
              
            if (endpointIndex == NET2280_ENDPT_0_IN)

                endpointIndex = 0;      

            /*
             * Read the contents of the EP_STAT register.
             * Some specific short packet handling is required for OUT endpoints.
             * This is the reason for reading this register contents here.
             */ 
        
            data32 = NET2280_CFG_READ (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex));

            /* Clear the data packet received and transmitted interrupts */

            data32 &= ~(NET2280_EP_STAT_DPT | NET2280_EP_STAT_DPR);

            /* Clear all the interrupts */

            NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex), data32);  

            } 
             

        else if (direction == USB_TCD_ENDPT_OUT)
            {

            /* To store temporarily some register values */

            UINT32 dataTemp = 0;

            /* OUT INTERRUPT of the endpoint */

            pTrb->uEndptInterruptStatus |=
                                        USBTCD_ENDPOINT_INTERRUPT_PENDING_MASK |
                                        USBTCD_ENDPOINT_OUT_PID_MASK;

            /* Clear the endptIntPending bit */

            pTarget->endptIntPending &= ~(1 << endpointIndex); 

            /*
             * Read the contents of the EP_STAT register.
             * Some specific short packet handling is required for OUT endpoints.
             * This is the reason for reading this register contents here.
             */ 

            data32 = NET2280_CFG_READ (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex));


            /*
             * Clear the interrupts other than data packet received
             * and transmitted interrupts
             */

            data32 &= ~(NET2280_EP_STAT_DPT | NET2280_EP_STAT_DPR);

#ifdef NET2280_DMA_SUPPORTED

            /* Clear all the interrupts other than short packet interrupt */

            NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex), 
                               (data32 & ~NET2280_EP_STAT_NAKOUT));  

#else
            /* Clear all the interrupts */

            NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex), data32);  
#endif

            /* Read EP_AVAIL register */

            dataTemp = NET2280_CFG_READ (pTarget,
                                   NET2280_EP_AVAIL_OFFSET (endpointIndex));

            /*
             * Copy the data in the FIFO which is to be copied
             * to the HAL buffer
             */
            pEndpointInfo->dataLength = dataTemp;


            /* 
             * There are high chances that between the duration of reading
             * the EP_STAT register and EP_AVAIL register, there is a short
             * packet detected. If we do not do the following handling, 
             * there is a chance that we have copied the
             * entire data, but the HAL does not know that it is the end of
             * transfer. The following piece of code handles this case.
             */

            /* 
             * We need not indicate to HAL a short packet, if the
             * data available is less than max packetsize.
             */ 
            if (pEndpointInfo->dataLength > pEndpointInfo->maxPacketSize)
                {

                /*
                 * Check if a short packet is already detected. ie, the EP_STAT
                 * value read earlier indicates a short packet.
                 * Note:NET2280_EP_STAT_NAKOUT mask indicates a short packet
                 * occurance. Once this bit is set, the target controller
                 * cannot accept more data. Only if this bit is cleared by
                 * TCD, it can accept more data. If this bit is set here, 
                 * we have already read the complete data length.
                 * If it is not set, read the EP_STAT register again to check if
                 * there is any short packet. 
                 */

                if ((data32 & NET2280_EP_STAT_NAKOUT) == 0)
                    {
                    dataTemp = NET2280_CFG_READ (pTarget, 
                               NET2280_EP_STAT_OFFSET(endpointIndex));

                    /* 
                     * If the NAKOUT bit is set, then we need to copy the
                     * complete size of data for the transfer.
                     */
                    if ((dataTemp & NET2280_EP_STAT_NAKOUT)!= 0)
                        {
                        pEndpointInfo->dataLength = NET2280_CFG_READ (pTarget,
                                   NET2280_EP_AVAIL_OFFSET (endpointIndex));
                        }

                    /* 
                     * Clear the interrupts other than data packet transmitted 
                     * and received interrupts.
                     */
              
                    dataTemp &= ~(NET2280_EP_STAT_DPT | NET2280_EP_STAT_DPR);

#ifdef NET2280_DMA_SUPPORTED
                    /* Clear the contents other than short packet */

                    NET2280_CFG_WRITE (pTarget, 
                    NET2280_EP_STAT_OFFSET(endpointIndex), 
                    (dataTemp & ~NET2280_EP_STAT_NAKOUT)); 
#else
                    /* Clear the contents of the EP_STAT register */

                    NET2280_CFG_WRITE (pTarget, 
                    NET2280_EP_STAT_OFFSET(endpointIndex), dataTemp); 

#endif

                    /* 
                     * As data32 will be used further down the line,
                     * update the data32 value
                     */
                    data32 = dataTemp;

                    }
               }

               /*
                * If the transfer length is 0, then 
                * do not notify an endpoint interrupt. 
                * If we notify an interrupt, then HAL assumes that it is an
                * end of transfer and calls the callback function. 
                */ 
               if (pEndpointInfo->dataLength == 0)
                   pTrb->uEndptInterruptStatus = 0;    

            }

        /* if bit 21 is set, report TIMEOUT error to HAL */

        if ((data32 & NET2280_EP_STAT_TIMEOUT) != 0)
            {

            pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_TIMEOUT_ERROR;

            }

        /* 
         * If NAKOUT mode is set, then there is a short packet detected.
         * We need not give a short packet on IN, as it is the HAL which has
         * initiated a short packet transfer
         */
        if (((data32 & NET2280_EP_STAT_NAKOUT) != 0) && 
            (direction == USB_TCD_ENDPT_OUT))
            {

            pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_DATA_UNDERRUN;

#ifdef NET2280_DMA_SUPPORTED

            /* Set the flag indicating that a short packet is detected */

            pEndpointInfo->shortPacket = TRUE; 
#endif

            }

        /*
         * read the EP_RSP register and determine whether the corresponding
         * endpoint is stalled or not
         */

        if ((NET2280_CFG_READ (pTarget, NET2280_EP_RSP_OFFSET (endpointIndex))
                               & NET2280_EP_RSP_STALL) != 0)

           {
           /* endpoints are stalled */

           pTrb->uEndptInterruptStatus |= USBTCD_ENDPOINT_STALL_ERROR;

           }

        }

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointIntStatusGet: Exiting...\n",
                        0,0,0,0,0,0);

    return OK;

    }


/*******************************************************************************
*
* usbTcdNET2280FncEndpointIntStatusClear - implements TCD_FNC_ENDPOINT_INTERRUPT_STATUS_CLEAR
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

LOCAL STATUS usbTcdNET2280FncEndpointIntStatusClear
    (
    pTRB_ENDPOINT_INTERRUPT_STATUS_CLEAR	pTrb	/* Trb to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpoint = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	direction = 0;			/* direction */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncEndpointIntStatusClear entered...",
    USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointIntStatusClear: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle ==0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncEndpointIntStatusClear exiting:Bad Parameter Received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointIntStatusClear: \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget =  (pUSB_TCD_NET2280_TARGET) pHeader->handle;
    pEndpoint = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;

    /* Determine the endpoint index and direction */

    endpointIndex = pEndpoint->endpointIndex;
    direction = pEndpoint->direction;

    /* If setup interrupt is pending, clear the member in pTarget structure */

    if ((endpointIndex == NET2280_ENDPT_0_OUT) && 
        (pTarget->setupIntPending))

        {

        /* To temporarily store the register contents */

        UINT32 dataTemp = 0;  

        /* Set setupIntPending to FALSE */

        pTarget->setupIntPending = FALSE;

        /*
         * Disable the setup interrupt, till we handle the setup. This will
         * be enabled in the endpoint interrupt status clear.
         */
        dataTemp = NET2280_CFG_READ(pTarget,NET2280_PCIIRQENB0_REG);

        dataTemp |= NET2280_IRQENB0_SETUP;

        NET2280_CFG_WRITE(pTarget,NET2280_PCIIRQENB0_REG, dataTemp);

        /* 
         * If there is an OUT interrupt pending on endpoint 0,
         * enable the interrupts
         */
        if ((pTarget->endptIntPending & (1 << NET2280_ENDPT_0_OUT)) != 0)
            {

            dataTemp = NET2280_CFG_READ (pTarget, 
                         NET2280_EP_IRQENB_OFFSET(NET2280_ENDPT_0_OUT));

            dataTemp |= NET2280_EP_STAT_DPR;

            NET2280_CFG_WRITE(pTarget,
                              NET2280_EP_IRQENB_OFFSET(NET2280_ENDPT_0_OUT),
                              dataTemp); 
            }     

        }

    /* Check if direction is IN and DMA interrupt is pending on that endpoint */

    else
        {
#ifdef NET2280_DMA_SUPPORTED
        switch (endpointIndex)

            {

             case NET2280_ENDPT_0_OUT:
             case NET2280_ENDPT_0_IN:
             case NET2280_ENDPT_E:
             case NET2280_ENDPT_F:
                 /* dma transfers cannot happen on these endpoints */
                 break;

             default :

                 if ((direction == USB_ENDPOINT_IN) &&
                     ((pTarget->dmaEot & (1 << endpointIndex)) != 0))
                    disableDma (pTarget, pEndpoint);
                }
#endif
            }

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointIntStatusClear: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }


/******************************************************************************
*
* usbTcdNET2280FncHandleResetInterrupt - implements TCD_FNC_HANDLE_RESET_INTERRUPT
*
* This fucntion is called whenever reset interrupt has occured. It determine
* the operating speed of the device.
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

LOCAL STATUS usbTcdNET2280FncHandleResetInterrupt
    (
    pTRB_HANDLE_RESET_INTERRUPT	pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	data32 = 0;


    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncHandleResetInterrupt entered.",USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncHandleResetInterrupt: Entered...\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncHandleResetInterrupt exiting:Bad Parameter Received.",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_DEBUG ("usbTcdNET2280FncHandleResetInterrupt : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget = (pUSB_TCD_NET2280_TARGET) pHeader->handle;


    OSS_THREAD_SLEEP(1);

    /* read the USB STATUS register and determine the speed */

    data32 = NET2280_CFG_READ (pTarget, NET2280_USBSTAT_REG);


    if ((data32 & NET2280_USBSTAT_FS) != 0)
        {

        /* Device is operating in Full Speed, update pTrb :: speed */

        pTrb->uSpeed = USB_TCD_FULL_SPEED;
        pTarget->speed = USB_TCD_FULL_SPEED;
        }

    else if ((data32 & NET2280_USBSTAT_HS) != 0)
        {
        /* Device is operating in High Speed, update pTrb :: speed */

        pTrb->uSpeed = USB_TCD_HIGH_SPEED;
        pTarget->speed = USB_TCD_HIGH_SPEED;

        }

    else
         return ERROR;

    USB_NET2280_DEBUG ("usbTcdNET2280FncHandleResetInterrupt: Exiting...\n",
    0,0,0,0,0,0);

    return OK;

    }


/******************************************************************************
*
* usbTcdNET2280FncHandleDisconnectInterrupt - implements TCD_FNC_HANDLE_DISCONNECT_INTERRUPT
*
* This fucntion is called whenever disconnect interrupt has occured. It flushes
* the FIFO buffer.
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

LOCAL STATUS usbTcdNET2280FncHandleDisconnectInterrupt
    (
    pTRB_HANDLE_DISCONNECT_INTERRUPT pTrb
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT32	data32 = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
    "usbTcdNET2280FncHandleDisconnectInterrupt entered...",
    USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncHandleDisconnectInterrupt: Entered.\n",
    0,0,0,0,0,0);

    /* Validate Parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_NET2280_INTERRUPT,
        "usbTcdNET2280FncHandleDisconnectInterrupt exiting: Bad Parameter \
        Received...", USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_DEBUG ("usbTcdNET2280FncHandleDisconnectInterrupt : \
        Bad Parameters...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget = (pUSB_TCD_NET2280_TARGET) pHeader->handle;

  
    /* read the DEVINIT regsiter */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DEVINIT_REG);

    /* Set bit 4 of DEVINIT register to flush the FIFO */

    data32 |= NET2280_DEVINIT_FIFO_RESET |NET2280_DEVINIT_USB_RESET ;


    NET2280_CFG_WRITE (pTarget, NET2280_DEVINIT_REG, data32);

    /* Reset the USB CTL register */

    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG, 0);
  
    /* Set the USB Detect Enable  bit if USB CTL register to 1 */   
  
    NET2280_CFG_WRITE (pTarget, NET2280_USBCTL_REG, NET2280_USBCTL_REG_USBDE);


    USB_NET2280_DEBUG ("usbTcdNET2280FncHandleDisconnectInterrupt: Exiting.\n",
    0,0,0,0,0,0);

    return OK;

    }



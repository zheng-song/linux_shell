/* usbTcdNET2280Endpoint.c - Endpoint Related Routines */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01j,19oct04,ami  Fixes in usbTcdNET2280EndpointRelease() identified during SH
                 testing
01i,30sep04,pdg  DMA testing fixes
01h,29sep04,ami  Cleared merge error
01g,29sep04,ami  Mips Related Changes
01f,23sep04,pdg  Fix for short packet handling
01e,21sep04,pdg  Full speed testing fixes
01d,20sep04,ami  NET2280 tested for High Speed
01c,17sep04,ami  After Control, Interrupt IN and Bulk OUT Testing
01b,08sep04,ami  Code Review Comments Incorporated
01a,10sep04,ami  First
*/

/*
DESCRIPTION

This file implements the endpoint related functionalities of TCD
(Target Controller Driver) for the Netchip NET2280

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usb.h,
               string.h, usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbNET2280.h,
               drv/usb/target/usbTcdNET2280Debug.h,
               drv/usb/target/usbNET2280Tcd.h, usb/target/usbPeriphInstr.h
               

*/

/* includes */

#include "usb/usbPlatform.h"
#include "usb/ossLib.h"
#include "usb/usb.h"
#include "string.h"
#include "usb/target/usbHalCommon.h"
#include "usb/target/usbTcd.h"               
#include "drv/usb/target/usbNET2280.h"       
#include "drv/usb/target/usbNET2280Tcd.h"   
#include "drv/usb/target/usbTcdNET2280Debug.h" 
#include "usb/target/usbPeriphInstr.h"

/* defines */

#define NET2280_E_F_MAXPACKET		0x40

#ifdef NET2280_DMA_SUPPORTED

#define NET2280_DMA_TRANS_SIZE		16000000 

/* forward declaration */

LOCAL UINT32 initializeDma
    (pUSB_TCD_NET2280_TARGET  pTarget,pUSB_TCD_NET2280_ENDPOINT pEndpointInfo);

LOCAL VOID disableDma
    (
    pUSB_TCD_NET2280_TARGET  pTarget,pUSB_TCD_NET2280_ENDPOINT pEndpointInfo
    );

#endif


/*******************************************************************************
*
* usbTcdNET2280FncEndpointAssign - implements TCD_FNC_ENDPOINT_ASSIGN
*
* This utility function is called to assign an endpoint depending on the
* endpoint descriptor value.
*
* RETURNS: OK or ERROR, if not able to assign the endpoint.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM
* Bad Parameter is passed.
*
* \i S_usbTcdLib_HW_NOT_READY
* Hardware is not ready.
*
* \i S_usbTcdLib_GENERAL_FAULT
* Fault in the upper software layer.
*
* \i S_usbTcdLib_OUT_OF_MEMORY
* Heap is out of memory.
* \ie
*
* \NOMANUAL
*/
    

LOCAL STATUS usbTcdNET2280FncEndpointAssign
    (
    pTRB_ENDPOINT_ASSIGN pTrb			/* TRB to be executed */
    )
  
    {

    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;
						/* USB_TCD_NET2280_ENDPOINT */
    pUSB_ENDPOINT_DESCR		pEndptDescr = NULL;  /* USB_ENDPOINT_DESCR */
    UINT8	transferType = 0;		/* trasfer type of endpoint */
    UINT8	direction = 0;			/* direction 1-IN, 0-OUT */
    UINT16	maxPacketSize = 0;		/* max packet size */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	i = 0;
    UINT8	nTrans = 0;
    UINT32	data32 = 0;			
    UINT8	endpointNum = 0;		/* endpoint number */
    UINT32	fifoConfMode = 0;		/* fifo config value */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncEndpointAssign entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointAssign : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pEndpointDesc == NULL))
    	{

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointAssign exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

    	USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }
    
    /* Get the endpoint descriptor */

    pEndptDescr = pTrb->pEndpointDesc;

    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;

    /* Determine direction */

    if ((pEndptDescr->endpointAddress & USB_ENDPOINT_DIR_MASK) != 0)
        direction = USB_TCD_ENDPT_IN;
    else
        direction = USB_TCD_ENDPT_OUT;

    /* Determine the endpoint number */

    endpointNum = (pEndptDescr->endpointAddress & USB_ENDPOINT_MASK);

    /* Determine the transfer type */

    transferType = pEndptDescr->attributes & USB_ATTR_EPTYPE_MASK;

    /*
     * Determine the Max Packet Size. According to USB Specification only
     * first 10 bits signifies max packet size and bits 11 & 12 states
     * number of transaction per frame.
     */

    maxPacketSize = FROM_LITTLEW (pEndptDescr->maxPacketSize) & 
                    USB_MAX_PACKET_SIZE_MASK;

    /* Determine the number of transaction per frame and validate it. */

    nTrans =  (FROM_LITTLEW(pEndptDescr->maxPacketSize) & 
                USB_NUMBER_OF_TRANSACTIONS_MASK)
                 >> USB_NUMBER_OF_TRANSACTIONS_BITPOSITION;

    if (transferType == USB_ATTR_CONTROL)
        {

        /* Transfer Type is Control. Verify endpoint number and maxpacket size */

        if (endpointNum != USB_ENDPOINT_CONTROL) 
            {

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
            "usbTcdNET2280FncEndpointAssign exiting...Bad Parameters received",
            USB_TCD_NET2280_WV_FILTER);

            USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign : Bad Parameters\
            ...\n",0,0,0,0,0,0);
            return ossStatus (S_usbTcdLib_BAD_PARAM);
            }

        if (maxPacketSize > USB_MAX_CTRL_PACKET_SIZE)
            {

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
            "usbTcdNET2280FncEndpointAssign exiting...Bad Parameters received",
            USB_TCD_NET2280_WV_FILTER);

            USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign : Bad Parameters\
            ...\n",0,0,0,0,0,0);
            return ossStatus (S_usbTcdLib_BAD_PARAM);
            }

 
        /* 
         * If request comes form upper layer to allocate more than one control 
         * OUT or control IN, return ERROR 
         */

        if (((direction == USB_TCD_ENDPT_OUT) && 
            ((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_0_OUT)) != 0)) 
            || 
            ((direction == USB_TCD_ENDPT_IN) && ((pTarget->endpointIndexInUse 
            & (1 << NET2280_ENDPT_0_IN)) != 0)))

            {

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
            "usbTcdNET2280FncEndpointAssign exiting...Bad Parameters received",
            USB_TCD_NET2280_WV_FILTER);

            USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign : Bad Parameters\
            ...\n",0,0,0,0,0,0);

            return ossStatus (S_usbTcdLib_BAD_PARAM);

            } 

        /* Check if endpoint 0 OUT is not alloted */  
       
        if (direction == USB_TCD_ENDPT_OUT)
            {

            /* Endpoint 0 OUT is not assigned */
           
	    /* allot endpoint Index */   
  	
            endpointIndex = NET2280_ENDPT_0_OUT; 
 
            /*
             * Set Setup Packet Interrupt Enable & Endpoint 0 Interrupt Enable 
             * bits of PCIIRQENB0 register
             */

             data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB0_REG);

             data32 |= NET2280_XIRQENB0_SETUP | (1 << NET2280_ENDPT_0_OUT);

            /* Write into PCIIRQENB0 regsiter */

            NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB0_REG, data32);

            /* Set Control Status Interrupt bit of PCIIRQENB1 register */

            data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

            data32 |= NET2280_XIRQENB1_CS;

            NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

            }
        else

	    /* allot endpoint Index */   
  	
            endpointIndex = NET2280_ENDPT_0_IN; 
    
     
       }
    
    else
        /* Any Generic Transfer type. Determine a valid endpoint Index */
        {
        /* 
         * If the maximum packet size is less than or equal to 64,
         * check if endpoint E or F is available
         */
        if (maxPacketSize <= NET2280_E_F_MAXPACKET)
            {
            if ((pTarget->endpointIndexInUse & (0x01 << NET2280_ENDPT_E)) == 0)
                endpointIndex = NET2280_ENDPT_E;
            else if ((pTarget->endpointIndexInUse & (0x01 << NET2280_ENDPT_F)) == 0)
                endpointIndex = NET2280_ENDPT_F;
            else
                endpointIndex = 0;
            }

        /* 
         * endpointIndex is 0 due to one of these reasons
         * 1. max packet size is <= 64 and the endpoint indices E and F are
         *    already occupied.
         * 2. max packet size is > 64.
         */
        if (endpointIndex == 0)
            {
            /*
             * If the maximum packet size is greater than 
             * USB_MAX_HIGH_SPEED_BULK_SIZE, then this is an isochronous
             * endpoint. Double buffering can be used in this case. A and B
             * support double buffering.
             * Based on the endpoints which are configured earlier, decide
             * on the FIFO configuration.
             */
            if (maxPacketSize > USB_MAX_HIGH_SPEED_BULK_SIZE)
                {
                for (i = NET2280_ENDPT_A; i <= NET2280_ENDPT_D; i++)
                    {
                    if ((pTarget->endpointIndexInUse & (1 << i)) == 0)
                        {
		        /*
 		         * The particular endpoint index is free. 
                         * Valid endpoint found
                         */
		        endpointIndex = i;
      
                        break;
                        }
                    }     

                 /* If the endpoint index cannot be allotted, return an error */
                 if ( endpointIndex == 0 )

                    /* Valid Index not found */
                    return ossStatus (S_usbTcdLib_GENERAL_FAULT);

                /* Set the default configuration to 0 */

                fifoConfMode = NET2280_FIFOCTL_CFG0;
                /* 
                 * Decide the FIFO configuration based on the current
                 * index and the earlier endpoints occupied
                 */

                switch(endpointIndex)
                    {

                    case NET2280_ENDPT_A:
                    case NET2280_ENDPT_B: 

                        /* 
                         * If C and D are not configured, Configuration 2 
                         * can be set
                         */

                        if ((pTarget->endpointIndexInUse & 
                            (0x3 << NET2280_ENDPT_C)) == 0)
                            fifoConfMode = NET2280_FIFOCTL_CFG2;
                        /* 
                         * If D is already configured, only Configuration 0 
                         * can be set
                         */

                        else if ((pTarget->endpointIndexInUse & 
                            (0x1 << NET2280_ENDPT_D)) != 0)
                            fifoConfMode = NET2280_FIFOCTL_CFG0;
                        /* 
                         * If only C is configured, Configuration 1 
                         * can be set
                         */

                        else
                            fifoConfMode = NET2280_FIFOCTL_CFG1;

                        break;

                    case NET2280_ENDPT_C: 
                        /* 
                         * If D is not already configured, Configuration 1 
                         * can be set
                         */

                        if ((pTarget->endpointIndexInUse & 
                            (0x1 << NET2280_ENDPT_D)) == 0)
                            fifoConfMode = NET2280_FIFOCTL_CFG1;
                        break;
                    default:
                        fifoConfMode = NET2280_FIFOCTL_CFG0;

                        break;
                    }
                }
             else
                {

                /* Set the default configuration mode */
                fifoConfMode = NET2280_FIFOCTL_CFG0;

                /* 
                 * Fill up the elements in this order ie C, B, A and finally D.
                 * This will help in using configuration 1 also. 
                 */

                /* Check if endpoint C is available */

                if ((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_C)) == 0)
                    endpointIndex = NET2280_ENDPT_C;

                /* Check if endpoint B is available */

                else if ((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_B)) == 0)
                    endpointIndex = NET2280_ENDPT_B;

                /* Check if endpoint A is available */

                else if ((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_A)) == 0)
                    endpointIndex = NET2280_ENDPT_A;

                /* Check if endpoint D is available */

                else if ((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_D)) == 0)
                    endpointIndex = NET2280_ENDPT_D;

                else
                    endpointIndex = 0;	

                /* If the endpoint index cannot be allotted, return an error */

                if ( endpointIndex == 0 )

                    /* Valid Index not found */
                    return ossStatus (S_usbTcdLib_GENERAL_FAULT);
                /* 
                 * Check if D is not configured. If it is not configured,
                 * then configuration 1 or 2 can be used.
                 */
                if (((pTarget->endpointIndexInUse & (1 << NET2280_ENDPT_D))
                      == 0) && (endpointIndex != NET2280_ENDPT_D)) 
                    fifoConfMode = NET2280_FIFOCTL_CFG2;    
                }  
            }
        }      

    /* Allocate USB_TCD_ISP1582_ENDPOINT structure */

    if ((pEndpointInfo = OSS_CALLOC (sizeof (USB_TCD_ISP1582_ENDPOINT)))== NULL)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointAssign exiting...Memory Allocation Error",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign: Bad Parameters \
        ...\n",0,0,0,0,0,0);

        return ossStatus (S_usbTcdLib_OUT_OF_MEMORY);
        }

    /*
     * Store Direction, transfer type, max packet size and endpoint index in
     * the USB_TCD_ISP1582_ENDPOINT.
     */

    pEndpointInfo->transferType = transferType;
    pEndpointInfo->direction = direction;
    pEndpointInfo->maxPacketSize = maxPacketSize;
    pEndpointInfo->endpointIndex = endpointIndex;

    /*
     * endpoint index is used to access the endpoint specific hardware and 
     * as there is only one control endpoint, set endpoint Index to 0
     */ 

    if ((transferType == USB_ATTR_CONTROL) && (direction == USB_TCD_ENDPT_IN))
 
        endpointIndex = 0;
        

    /* set corresponding bit of pTarget->endpointIndexInUse */

    pTarget->endpointIndexInUse |= (1 << pEndpointInfo->endpointIndex);


#ifdef NET2280_DMA_SUPPORTED

    pEndpointInfo->dmaInUse = FALSE;

    /* Only endpoint A,B,C & D supports DMA */

    switch (endpointIndex)
        {
 
        case NET2280_ENDPT_A:
        case NET2280_ENDPT_B:
	case NET2280_ENDPT_C: 
        case NET2280_ENDPT_D: 
            pEndpointInfo->isDmaSupported = TRUE;

            break; 
        default : pEndpointInfo->isDmaSupported = FALSE;

        }
#endif

    /* Update the configuration if the endpoint indices are not E, F & 0 */

    if ((endpointIndex != NET2280_ENDPT_E) && 
        (endpointIndex != NET2280_ENDPT_F) && (endpointIndex != NET2280_ENDPT_0_OUT))
        {
        /* Read the contents of the FIFOCTL register */
        data32 = NET2280_CFG_READ (pTarget, NET2280_FIFOCTL_REG);

        /* Clear the configuration mode */
        data32 &= ~NET2280_FIFOCTL_CFG_MASK;

        /* Write to the FIFOCTL register to set the FIFO configuration */

        NET2280_CFG_WRITE (pTarget, NET2280_FIFOCTL_REG, data32 | fifoConfMode);
        }


    /* If it is not a control endpoint, configure the endpoint */

    if (transferType != USB_ATTR_CONTROL)

        {
        /*
         * Configure the Endpoint Configuration Register and enable it.  
         * We donot enable Endpoint 0. It is always enabled 
         */

        data32 = 0;

        /* Reset the configuration register for the endpoint */

        NET2280_CFG_WRITE (pTarget, NET2280_EP_CFG_OFFSET(endpointIndex),0);

        /*
         * Set the Configuration Register with endpoint number, transfer type
         * & endpoint direction
         */

        /* endpoint number */

        data32 = endpointNum;
        
        /* direction */    
      
        data32 |= (direction << NET2280_EP_CFG_DIR_SHIFT);

        /* transfer type */

        switch (transferType)
            {

            case USB_ATTR_BULK:  data32 |= NET2280_EP_CFG_TYPE_BULK;
                break;

            case USB_ATTR_ISOCH: data32 |= NET2280_EP_CFG_TYPE_ISO; 
                break;

            case USB_ATTR_INTERRUPT: data32 |= NET2280_EP_CFG_TYPE_INT;

                break;

            default : USB_NET2280_ERROR ("usbTcdNET2280FncEndpointAssign: \
                      Wrong Transfer Type...\n",0,0,0,0,0,0);

                      OSS_FREE (pEndpointInfo);

                      /* WindView Instrumentation */

                      USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
                      "usbTcdNET2280FncEndpointAssign exiting...Wrong Transfer \
                      Type", USB_TCD_NET2280_WV_FILTER);

                      return ossStatus (S_usbTcdLib_GENERAL_FAULT);

           }

        /* Write into configuration regsiter */
    
        NET2280_CFG_WRITE (pTarget, 
                           NET2280_EP_CFG_OFFSET (endpointIndex),
                           data32);

        /* Update the max packet size determining on the speed */
 
        if (pTarget->speed == USB_TCD_HIGH_SPEED)

            data32 =  NET2280_EP_X_HS_MAXPKT_IDX(endpointIndex);

        else
            data32 =  NET2280_EP_X_FS_MAXPKT_IDX(endpointIndex);
              
        /*
         * Write the max packet size in the high speed max packet size 
         * register of the corresponding endpoint
         */
           
       /* Write the address in the index register */
            
       NET2280_CFG_WRITE (pTarget, NET2280_IDXADDR_REG, data32);

       /* Write the max packet size in the index data register */

       if (pTarget->speed == USB_TCD_HIGH_SPEED)

           {   

           data32 =  (nTrans << NET2280_NTRANS_SHIFT) | maxPacketSize;

           }   

       else
           
           data32 = maxPacketSize;         

       NET2280_CFG_WRITE (pTarget, NET2280_IDXDATA_REG, data32);

       /* Unstall the endpoint by writing into EP_RSP register */

       NET2280_CFG_WRITE (pTarget, NET2280_EP_RSP_OFFSET (endpointIndex), 
                          NET2280_EP_RSP_CLEAR (NET2280_EP_RSP_STALL));        

       /* Enable the interrupts for the correponding endpoint */

       data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB0_REG);

       data32 |= NET2280_IRQENB0_EP(endpointIndex);

       NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB0_REG, data32);   

 

       }     
  
       /* Reset the EP_STAT register */ 

       NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET (endpointIndex),
                          0xFFFF); 


       /*
        * Depending on the direction update Data Packet Received Interrupt 
        * and Data Packet Transmitted Interupt
        */ 
       
      if (direction == USB_TCD_ENDPT_IN)

          {

          data32 = NET2280_CFG_READ (pTarget, 
                                     NET2280_EP_IRQENB_OFFSET(endpointIndex));

          data32 |= NET2280_EP_IRQENB_DPT;


          NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex),
                             data32);             

          }
     
      else
          {

          data32 = NET2280_CFG_READ (pTarget, 
                   NET2280_EP_IRQENB_OFFSET(endpointIndex)); 

          data32 |= NET2280_EP_IRQENB_DPR;

          NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex),
                             data32);             

          }

        data32 = NET2280_CFG_READ (pTarget, NET2280_EP_CFG_OFFSET (endpointIndex));

        /* Enable the endpoint for USB Transfer */

        data32 |= NET2280_EP_CFG_ENABLE;

        /* Write into configuration regsiter */
    
        NET2280_CFG_WRITE (pTarget, 
                           NET2280_EP_CFG_OFFSET (endpointIndex),
                           data32);

    /* Store the handle */

    pTrb->pipeHandle = (UINT32)pEndpointInfo;

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointAssign : Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }


/*******************************************************************************
*
* usbTcdNET2280FncEndpointRelease - implements TCD_FNC_ENDPOINT_RELEASE
*
* This function releases an endpoint.
*
* RETURNS: OK or ERROR if failed to unconfigure the endpoint
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncEndpointRelease
    (
    pTRB_ENDPOINT_RELEASE	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;
                                                /*USB_TCD_NET2280_ENDPOINT*/
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	direction = 0;			/* direction */
    UINT8	transferType = 0;		/* Transfer type */
    UINT32	data32 = 0;			/* temporary variable */
    UINT32	fifoConfMode = 0;		/* fifo config value */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncEndpointRelease entered...", USB_TCD_NET2280_WV_FILTER);


    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointRelease : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointRelease exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointRelease : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;
    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;

    endpointIndex = pEndpointInfo->endpointIndex;
    direction = pEndpointInfo->direction;
    transferType = pEndpointInfo->transferType;
    
    if (transferType == USB_ATTR_CONTROL)
        {

        if (direction ==  USB_TCD_ENDPT_OUT)
        
            {

            /*
             * Disable the setup packet interrupt and endpoint 0 interupt in 
             * PCIIRQENB0 register 
             */

            data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB0_REG);

            if ((data32 & (NET2280_XIRQENB0_SETUP | 
                 NET2280_XIRQENB0_EP(NET2280_ENDPT_0_OUT))) != 0)
  
                {

                data32 &= ~(NET2280_XIRQENB0_SETUP | 
			NET2280_XIRQENB0_EP(endpointIndex));

                NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB0_REG, 
                               data32 & NET2280_XIRQENB0_MASK); 

                }

            /*
             * Disable the control status interrupt. The disabling of this bit
             * can be done for control endpoint IN also. We are doing some 
             * interrupt specific handling for endpoint 0 here, hence we are
             * handling this interrupt also here.     
             */

            if (((data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG)) &
                  NET2280_XIRQENB1_CS) != 0)

                {

                data32 &= ~NET2280_XIRQENB1_CS;
 
                NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, 
                               data32 & NET2280_XIRQENB1_REG_MASK); 

                }


            } 

        else

            {   

            /*
             * endpoint index is used to access the endpoint specific hardware and 
             * as there is only one control endpoint, set endpoint Index to 0
             */ 

            endpointIndex = 0;
            }   


        }

     else

        {

        /* Disable the Endpoint by writing into EP_CFG register */

        NET2280_CFG_WRITE (pTarget, NET2280_EP_CFG_OFFSET (endpointIndex), 0);

        /* Disable the interupts on the endpoint */

        data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB0_REG);
  
        data32 &= ~NET2280_XIRQENB0_EP(endpointIndex);
                              
        NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB0_REG, 
                           data32 & NET2280_XIRQENB0_MASK); 

           
        }

    /* Flush the respective FIFO buffer for the endpoint */

    NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET (endpointIndex),
                       NET2280_EP_STAT_FIFO_FLUSH);

    if (direction == USB_TCD_ENDPT_IN)

        {

        data32 = NET2280_CFG_READ (pTarget, 
                                   NET2280_EP_IRQENB_OFFSET(endpointIndex));

        data32 &= ~NET2280_EP_IRQENB_DPT;

        NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex),
                           data32 & NET2280_EP_IRQEBN_MASK);             

        }
     
    else
        {

        data32 = NET2280_CFG_READ (pTarget, 
                                   NET2280_EP_IRQENB_OFFSET(endpointIndex)); 

        data32 &= ~NET2280_EP_IRQENB_DPR;

        NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex),
                           data32 & NET2280_EP_IRQEBN_MASK);             

        }

    /* Write all 1s to clear the interrupt status */

    NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex),
                       0xFFFFFFFF);             


    /* Clear the contents of the EP RESPONSE register */

    NET2280_CFG_WRITE (pTarget, NET2280_EP_RSP_OFFSET(endpointIndex),
                       NET2280_EP_RSP_CLEAR(NET2280_EP_RSP_NAKOUT));

    /* 
     * Based on the current endpoint configuration,
     * reconfigure the FIFO. endpoints E and F if unconfigured,
     * no FIFO reconfiguration is required.
     */
    
    if ((pEndpointInfo->endpointIndex != NET2280_ENDPT_E) &&
        (pEndpointInfo->endpointIndex != NET2280_ENDPT_F))
        {

        /* If endpoint D is being used, then only config 1 can be set */
 
        if ((pTarget->endpointIndexInUse & (0x1 << NET2280_ENDPT_D)) != 0)
            fifoConfMode = NET2280_FIFOCTL_CFG0;

        else if ((pTarget->endpointIndexInUse & (0x1 << NET2280_ENDPT_C)) != 0)
            fifoConfMode = NET2280_FIFOCTL_CFG1;

        else
            fifoConfMode = NET2280_FIFOCTL_CFG2; 

        /* Read the contents of the FIFOCTL register */
        data32 = NET2280_CFG_READ (pTarget, NET2280_FIFOCTL_REG);

        /* Clear the configuration mode */
        data32 &= ~NET2280_FIFOCTL_CFG_MASK;

        /* Write to the FIFOCTL register to set the FIFO configuration */

        NET2280_CFG_WRITE (pTarget, NET2280_FIFOCTL_REG, data32 | fifoConfMode);

        }

    /* Reset the corresponding endpoint bit in endpointIndexInUse */

    pTarget->endpointIndexInUse &= ~(1 << pEndpointInfo->endpointIndex); 

#ifdef NET2280_DMA_SUPPORTED
    /* 
     * If the endpoint supports dma, then
     * stop the dma transfers
     */ 
    switch (pEndpointInfo->endpointIndex )
        {
        case	NET2280_ENDPT_A:
        case	NET2280_ENDPT_B:
        case	NET2280_ENDPT_C:
        case	NET2280_ENDPT_D:	
            disableDma(pTarget,pEndpointInfo);
            break;
        default: break;
        }
#endif
    /* Release the endpoint */

    OSS_FREE(pEndpointInfo);
  
    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointRelease : Exiting...\n",
    0,0,0,0,0,0);
    
    return OK;
    }


/******************************************************************************
*
* usbTcdNET2280FncEndpointStatusGet - implements TCD_FNC_ENDPOINT_STATUS_GET
*
* This function returns the status of an endpoint ie whether it is STALLED
* or not.
*
* RETURNS: OK or ERROR, it not able to get the status of the endpoint.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncEndpointStatusGet 
    (
    pTRB_ENDPOINT_STATUS_GET pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;
                                                /*USB_TCD_NET2280_ENDPOINT*/
    UINT8	endpointIndex = 0;		/* endpoint index */
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncEndpointStatusGet entered...", USB_TCD_NET2280_WV_FILTER);


    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStatusGet: Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0) || (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointStatusGet exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointStatusGet: Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /*
     * If transfer type is Control and dirction is IN set endpointIndex to 0. 
     * Endpoint index is used to access the endpoint specific hardware and 
     * as there is only one control endpoint, set endpoint Index to 0
     */ 


    if (endpointIndex == NET2280_ENDPT_0_IN)
        
        endpointIndex = 0;


    /* 
     * Read the Endpoint Response register and determine the status of the 
     * endpoint 
     */  
      
    if ((NET2280_CFG_READ (pTarget, NET2280_EP_RSP_OFFSET(endpointIndex))
        & NET2280_EP_RSP_STALL) == NET2280_EP_RSP_STALL)
        {


        /* Endpoint is stalled */

        *(pTrb->pStatus) = USB_ENDPOINT_STS_HALT;
        }

    else

        /* Endpoint is not stalled. Update pStatus to 0 */

        *(pTrb->pStatus) = 0;

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStatusGet : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }


/******************************************************************************
*
* usbTcdNET2280FncEndpointStateSet - implements TCD_FNC_ENDPOINT_STATE_SET
*
* This function sets endpoint state as stalled or un-stalled.
*
* RETURNS: OK or ERROR, it not able to set the state of the endpoint
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncEndpointStateSet 
    (
    pTRB_ENDPOINT_STATE_SET pTrb		/* TRB to be executed */	
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncEndpointStateSet entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStateSet: Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0) || (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointStateSet exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncEndpointStateSet : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /*
     * If transfer type is Control and dirction is IN set endpointIndex to 0. 
     * Endpoint index is used to access the endpoint specific hardware and 
     * as there is only one control endpoint, set endpoint Index to 0
     */ 


    if (endpointIndex == NET2280_ENDPT_0_IN)
        
        endpointIndex = 0;


    if (pTrb->state == TCD_ENDPOINT_STALL)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointStateSet exiting...Stalling Endpoint",
        USB_TCD_NET2280_WV_FILTER);

        /*
         * Stall the endpoint by setting the Endpoint Halt (bit 8) of EP_RSP 
         * register of corresponding endpoint
         */
    
        NET2280_CFG_WRITE (pTarget, NET2280_EP_RSP_OFFSET (endpointIndex), 
                           NET2280_EP_RSP_SET (NET2280_EP_RSP_STALL));   

        USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStateSet : Stalling...\n",
        0,0,0,0,0,0);

               
        }

    else if (pTrb->state == TCD_ENDPOINT_UNSTALL)
        {


        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncEndpointStateSet exiting...Un-stalling Endpoint",
        USB_TCD_NET2280_WV_FILTER);

        /*
         * Un-stall the endpoint by setting the Endpoint Halt (bit 0) of EP_RSP 
         * register of corresponding endpoint
         */
    
        NET2280_CFG_WRITE (pTarget, NET2280_EP_RSP_OFFSET (endpointIndex), 
                           NET2280_EP_RSP_CLEAR (NET2280_EP_RSP_STALL));   


        USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStateSet : Un-stalling...\n",
        0,0,0,0,0,0);
        }
    
    USB_NET2280_DEBUG ("usbTcdNET2280FncEndpointStateSet: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdNET2280FncIsBufferEmpty - implements TCD_FNC_IS_BUFFER_EMPTY
*
* This utility function is used to check whether the FIFO buffer related
* with the associated endpoint is empty or not.
*
* RETURNS: OK or ERROR, it not able to get the status of the buffer.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncIsBufferEmpty 
    (
    pTRB_IS_BUFFER_EMPTY pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncIsBufferEmpty entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncIsBufferEmpty: Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncIsBufferEmpty exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncIsBufferEmpty : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /*
     * If transfer type is Control and dirction is IN set endpointIndex to 0. 
     * Endpoint index is used to access the endpoint specific hardware and 
     * as there is only one control endpoint, set endpoint Index to 0
     */ 


    if (endpointIndex == NET2280_ENDPT_0_IN)
        
        endpointIndex = 0;


    /*
     * Read the bit 10 of EP_STAT register and determine if the FIFO 
     * is empty
     */
   
    if ((NET2280_CFG_READ (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex))
                           & NET2280_EP_STAT_FIFO_EMPTY) != 0)

        {

        /* FIFO is empty */ 

        pTrb->bufferEmpty = TRUE;
 
        }

    else
        {

        /* FIFO is not empty */ 
  
        pTrb->bufferEmpty = FALSE;
 
        }
    
    USB_NET2280_DEBUG ("usbTcdNET2280FncIsBufferEmpty : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncCopyDataFromEpBuf - implements TCD_FNC_COPY_DATA_FROM_EPBUF
*
* This function copies the data from the endpoint FIFO buffer into the
* buffer that is passed. The Setup data on Control Endpoint is read from the 
* setup register. If the endpoint supports the DMA transfer we initiate the 
* DMA transfer on the corressponding endpoint. 
*
* RETURNS: OK or ERROR if not able to read data from the enpoint FIFO.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncCopyDataFromEpBuf 
    (
    pTRB_COPY_DATA_FROM_EPBUF pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	transferType = 0;		/* transfer type */
    UINT32	sizeToCopy = 0;			/* size to data to copy */
    unsigned char  * pBuf = pTrb->pBuffer;	/* pointer to buffer */
    UINT32	data32 = 0;			/* temporary variable */
    UINT8       i = 0;


    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncCopyDataFromEpBuf entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncCopyDataFromEpBuf: Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncCopyDataFromEpBuf exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncCopyDataFromEpBuf: Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;
    endpointIndex = pEndpointInfo->endpointIndex;
    transferType = pEndpointInfo->transferType;

#ifdef NET2280_DMA_SUPPORTED

    /*
     * DMA transfer is used only when the
     * the request length is a multiple of 4.
     * If there is a short packet detected, the HAL
     * will call the callback immediately after returning
     * from this function. If dma is used in this case,
     * the HAL will not have any data in the buffer which is
     * incorrect handling. So dma should not be used on a short
     * packet detection. 
     */

    if (pEndpointInfo->isDmaSupported &&
        ((pTrb->uActLength % 4) == 0) &&
        !pEndpointInfo->shortPacket)
        {
        /* Check if dma is being in use */
 
        if (pEndpointInfo->dmaInUse)
            {         
   
            /* Check if dmaEot is detected for the corresponding endpoint*/
 
            if ((pTarget->dmaEot & (1 << endpointIndex)) != 0)

                {
                /* Reset the corresponding bit in pTarget->dmaEot */

                pTarget->dmaEot &= ~(1 << endpointIndex);

                /* Calculate the size of data actually received */

                sizeToCopy = pEndpointInfo->dmaTransferLength - 
                                (NET2280_CFG_READ (pTarget, 
                                        NET2280_DMACOUNT_OFFSET(endpointIndex))
                                        & NET2280_DMACOUNT_BC);

                /* Invalidate DMA buffer */

                USB_PCI_MEM_INVALIDATE ((char *) pEndpointInfo->dmaBuffer,
                                            sizeToCopy);

                /* Read the EP_AVAIL register contents */

                data32 = NET2280_CFG_READ (pTarget,
                                   NET2280_EP_AVAIL_OFFSET (endpointIndex));

                /*
                 * Check if there is a short packet and there is some more
                 * data in the FIFO which is yet to be copied.
                 * This condition can happen when the data received is not a
                 * multiple of 4.
                 */

                if ((pEndpointInfo->dmaTransferLength != sizeToCopy) &&
                    (data32 > 0) && (data32 < 4))
                    {

                    /* Update pBuf pointer */

                    pBuf =  pTrb->pBuffer + sizeToCopy;

                    /* Update the size to copy */

                    sizeToCopy += data32;

                    /* Copy the size of data available */
  
                    i = data32;

                    /* Read EP_DATA register for the corresponding endpoint */

                    data32 = NET2280_CFG_READ (pTarget, 
                            NET2280_EP_DATA_OFFSET (endpointIndex));

                    /* Update TRB::pBuffer */

                    for (; i > 0; i--)

                        {

                        *pBuf = (char) (data32 & 0xFF);
          
                        data32 >>= 8;

                        pBuf++;
      
                        }

                    }                 

                /*
                 * Clear the NAK OUT mode bit which enables further 
                 * reception of packets.
                 */
 
                NET2280_CFG_WRITE (pTarget, 
                                   NET2280_EP_STAT_OFFSET(endpointIndex), 
                                   NET2280_EP_STAT_NAKOUT);
                /*
                 * If the size of data requested is more than the 
                 * dma transfer length, initiate the dma again
                 */
                if ((sizeToCopy == pEndpointInfo->dmaTransferLength) &&
                    (pTrb->uActLength > pEndpointInfo->dmaTransferLength))
                    {
                    /* Store the return value temporarily, need not use it */
                    data32 = initializeDma(pTarget, pEndpointInfo);
                    } 
                else /* Request is completed */
                    {

                    USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
                    "usbTcdNET2280FncCopyDataFromEpBuf...DMA \
                    Transfer Completed Successfully... Disabling DMA ",
                    USB_TCD_NET2280_WV_FILTER);

                    /* Disable the DMA */

                    disableDma (pTarget, pEndpointInfo);
                    }

                /* update uActLength with the size copied */
  
                pTrb->uActLength = sizeToCopy;

                return OK;

                } 

            else 
                {
                pTrb->uActLength = 0;

                /* DMA is in USE */
  
                return OK;  
                }
                   
            }

        else

            {

            /*
             * Determine the size of data which can be transmitted
             * in one dma transfer
             */

            pEndpointInfo->dmaTransferLength = (pTrb->uActLength < 
            NET2280_DMA_TRANS_SIZE) ? pTrb->uActLength : 
                                      NET2280_DMA_TRANS_SIZE;

            /* Copy the buffer pointer */

            pEndpointInfo->dmaBuffer = pTrb->pBuffer; 

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
            "usbTcdNET2280FncCopyDataFromEpBuf: Intiating DMA Activity... ",
            USB_TCD_NET2280_WV_FILTER);


            /* Initiate DMA activity on that endpoint */

            pTrb->uActLength = initializeDma (pTarget, pEndpointInfo);

            }  

        }     

    else

        {

#endif


        /* Determine if transfer type is CONTROL */

        if (transferType == USB_ATTR_CONTROL)

            {
            if (pTarget->statusOUTPending &&
               (pTrb->uActLength == 0))

                {

                pTarget->statusOUTPending = FALSE;

                /*
                 * Status Stage. The zero length packet will be sent when 
                 * Control Trnafere Handshake bit is  cleared. This handled 
                 * in Interrupt routines
                 */

                return OK;
                }      

            if (pTarget->setupIntPending)

                {

                /* Reset the flags */

                pTarget->statusOUTPending = FALSE;
                pTarget->statusINPending = FALSE;

                /* Setup Data has come from host. read the setup register */

                /* Update TRB::uActLength to 8 */

                pTrb->uActLength = 8; 

                /* Read the first setup regsiter */

                data32 = NET2280_CFG_READ (pTarget, NET2280_SETUP0123_REG);

                for (i = 0; i < 4; i++)

                    {

                    *(pBuf + i) = (char) (data32 & 0xFF);
          
                    data32 >>= 0x8;

                    }

                pBuf = pBuf + 4;

                /* Read the second setup regsiter */

                data32 = NET2280_CFG_READ (pTarget, NET2280_SETUP4567_REG);

                for (i = 0; i < 4; i++)

                    {

                    *(pBuf + i) = (char) (data32 & 0xFF);
          
                    data32 >>= 0x8;

                    }
  
                /* If there is no data stage, update the status pending flag */

                if ((pTrb->pBuffer[6] | (pTrb->pBuffer[7] << 8)) == 0)
                    pTarget->statusINPending = TRUE;

                return OK;
                }

            } 
#ifdef NET2280_DMA_SUPPORTED

        /* 
         * If a short packet is detected, 
         * clear the NAKOUT bit so that further packets can
         * be accepted.
         */
        if (pEndpointInfo->shortPacket)
            {

            /* Clear the short packet detected flag in EP_STAT register */

            NET2280_CFG_WRITE (pTarget, 
                    NET2280_EP_STAT_OFFSET(endpointIndex),NET2280_EP_STAT_NAKOUT);

            /* Reset the short packet detected flag */

            pEndpointInfo->shortPacket = FALSE;
            }

#endif
        /* Retrieve the size to be copied from the dataLength field */

        sizeToCopy = pEndpointInfo->dataLength;

        /* Determine the size to copy */

        if (pTrb->uActLength < sizeToCopy)

            {
            sizeToCopy = pTrb->uActLength;
            }

        /* 
         * If it is the last packet which is being copied, set
         * the statuspending flag to TRUE.
         */
        if ((transferType == USB_ATTR_CONTROL) &&
           (sizeToCopy == pTrb->uActLength))

            pTarget->statusINPending = TRUE;

        /* Update TRB::uActLength to sizeToCppy */

        pTrb->uActLength = sizeToCopy;

        while (sizeToCopy >= 4)

            {
            
            /* Read ET_DATA register for the corresponding endpoint */

            data32 = NET2280_CFG_READ (pTarget, 
                     NET2280_EP_DATA_OFFSET (endpointIndex));

 
            /* Update TRB::pBuffer */

            for (i = 0; i < 4; i++)

                {

                *(pBuf + i) = (char) (data32 & 0xFF);
          
                data32 >>= 0x8;

                }

            pBuf = pBuf + 4;  

            /* decrement size to copy by 4 */

            sizeToCopy -= 4;

            }

  
        if (sizeToCopy != 0)
                     
            {
             
            /*
             * There is a short packet received. Read the data from the FIFO
             * buffer and clear the NAK OUT Packet bit of endpoint response 
             * regsiter 
             */


            data32 = NET2280_CFG_READ (pTarget, 
                     NET2280_EP_DATA_OFFSET (endpointIndex));
                       

  
            for (i = 0; i < sizeToCopy; i++)

                {

                *(pBuf)++ = (char) (data32 & 0xFF); 

                data32 >>= 8;

                }
            }

#ifdef NET2280_DMA_SUPPORTED

        }

#endif
 
    USB_NET2280_DEBUG ("usbTcdNET2280FncCopyDataFromEpBuf: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
                 
    }                   
                        

/*******************************************************************************
*
* usbTcdNET2280FncCopyDataToEpBuf - implements TCD_FNC_COPY_DATA_TO_EPBUF
*
* This function copies the data into the endpoint FIFO buffer from the buffer
* that is passed.
*
* RETURNS: OK or ERROR if not able to read data from the enpoint fifo.
*
* ERRNO:
* \is
* \i S_usbTcdLib_BAD_PARAM.
* Bad Parameter is passed.
* \ie
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdNET2280FncCopyDataToEpBuf 
    (
    pTRB_COPY_DATA_TO_EPBUF pTrb		/* TRB to be executed */
    )

    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_NET2280_ENDPOINT*/
    pUSB_TCD_NET2280_TARGET	pTarget = NULL;	/* USB_TCD_NET2280_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	transferType = 0;		/* transfer type */
    UINT32	sizeToCopy = 0;			/* size to data to copy */
    unsigned char  * pBuf = pTrb->pBuffer;	/* pointer to buffer */
    UINT32	data32 = 0;			/* temporary variable */
    UINT8       i = 0;
    UINT16	maxPacket = 0; 

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_NET2280_ENDPOINT,
    "usbTcdNET2280FncCopyDataToEpBuf entered...", USB_TCD_NET2280_WV_FILTER);

    USB_NET2280_DEBUG ("usbTcdNET2280FncCopyDataToEpBuf: Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
        "usbTcdNET2280FncCopyDataToEpBuf exiting...Bad Parameters received",
        USB_TCD_NET2280_WV_FILTER);

        USB_NET2280_ERROR ("usbTcdNET2280FncCopyDataToEpBuf: Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_NET2280_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_NET2280_TARGET)pHeader->handle;
    endpointIndex = pEndpointInfo->endpointIndex;
    transferType = pEndpointInfo->transferType;

    /*
     * If transfer type is Control and dirction is IN set endpointIndex to 0. 
     * Endpoint index is used to access the endpoint specific hardware and 
     * as there is only one control endpoint, set endpoint Index to 0
     */ 


    if (endpointIndex == NET2280_ENDPT_0_IN)
        
        endpointIndex = 0;

                          

#ifdef NET2280_DMA_SUPPORTED

    /* 
     * Check if the endpoint supports DMA and the request length is a
     * multiple of maxpacket size.
     */

    if (pEndpointInfo->isDmaSupported &&
        ((pTrb->uActLength %4) == 0))
        {
        /* Check if a dma transfer is already in use */

        if (pEndpointInfo->dmaInUse)
            {         
   
            /* Check if dmaEot is detected for the corresponding endpoint*/
 
            if ((pTarget->dmaEot & (1 << endpointIndex)) != 0)

                {
                /* Reset the corresponding bit in pTarget->dmaEot */

                pTarget->dmaEot &= ~(1 << endpointIndex);

                /* 
                 * Initiate another DMA transfer 
                 * if the actLength is non-zero.
                 */
                if (pTrb->uActLength != 0)
                    {

                    /* Update the dma transfer length */

                    pEndpointInfo->dmaTransferLength = (pTrb->uActLength <
                                 NET2280_DMA_TRANS_SIZE) ? 
                    pTrb->uActLength : NET2280_DMA_TRANS_SIZE;

                    /* Copy the buffer pointer */

                    pEndpointInfo->dmaBuffer = pTrb->pBuffer;

                    /* WindView Instrumentation */

                    USB_TCD_LOG_EVENT (USB_TCD_NET2280_ENDPOINT,
                        "usbTcdNET2280FncCopyDataToEpBuf: Intiating DMA \
                        Activity... ",USB_TCD_NET2280_WV_FILTER);

             
                    /* Initiate DMA activity on that endpoint */

                    pTrb->uActLength = 
                              initializeDma (pTarget, pEndpointInfo);
                    }
                 else
                    /* Disable the DMA */
                    disableDma (pTarget, pEndpointInfo);

                 return OK;

                } 

            else 

                /* DMA is in USE */
  
                return OK;  
                   
            }

        else

            {

            /* Find the length of dma transfer */

            pEndpointInfo->dmaTransferLength = (pTrb->uActLength < 
            NET2280_DMA_TRANS_SIZE) ? 
            pTrb->uActLength : NET2280_DMA_TRANS_SIZE;

            /* Copy the buffer pointer */

            pEndpointInfo->dmaBuffer = pTrb->pBuffer;

            /* Initiate DMA activity on that endpoint */

            pTrb->uActLength = initializeDma (pTarget, pEndpointInfo);

            } 

        }     

    else

        {

#endif

        if (transferType == USB_ATTR_CONTROL)

            {
            if (pTarget->statusINPending &&
                (pTrb->uActLength == 0))
 
                {
                /*
                 * Status Stage. The zero length packet will be sent when 
                 * Control Transfer Handshake bit is  cleared. Thisis  handled
                 * in the Interrupt routines
                 */

	     	UINT32	cntEnb = 0;

                /* Enable the Control Interrupt Status bit. This was disabled
                 * whle handling DATA OUT interrupt in 
                 * usbTcdNET2280InterruptStatusGet 
                 */

                cntEnb = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

                cntEnb |= NET2280_XIRQENB1_CS;
 
                NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, cntEnb);                                     

                pTarget->statusINPending = FALSE;

                return OK;
                }  

            }

        /* Update sizeToCopy */

        sizeToCopy = (pTrb->uActLength < pEndpointInfo->maxPacketSize) ?
        	      pTrb->uActLength : pEndpointInfo->maxPacketSize;


        /* Update the statusOUT pending flag */
        if ((transferType == USB_ATTR_CONTROL) &&
            (pTrb->uActLength == sizeToCopy)) 
            pTarget->statusOUTPending = FALSE;

        /* update size to copy */ 

        pTrb->uActLength = sizeToCopy;

        /* 
         * Initially the byte count feild of the endpoint configuration register 
         * should be set to some value greater than 3, other wise it would be
         * treated as a short packet
         */ 

        data32 = NET2280_CFG_READ (pTarget, 
                NET2280_EP_CFG_OFFSET (endpointIndex));                     

        data32 |= 4 << NET2280_EP_CFG_BYTE_CNT_SHIFT;
 

        /* Write the value in the EP_CFG register */

        NET2280_CFG_WRITE (pTarget, NET2280_EP_CFG_OFFSET (endpointIndex), data32);

        while (sizeToCopy >= 4)

            {
            data32 = 0;

            /* Write 4 byte of data into the Endpoint FIFO */

            for (i = 0; i < 4; i++)

                {

                data32 |= (*(pBuf + i) << (i * 8));
 
                }                       



            /* Write the data in the Endpoint FIFO */

            NET2280_CFG_WRITE (pTarget, NET2280_EP_DATA_OFFSET (endpointIndex),
                               data32);

            /* update pBuf */

            pBuf += 4;

            /* Decrement size to copy */
            sizeToCopy -= 4;

            }

        if (transferType == USB_ATTR_CONTROL)
           
            maxPacket = 64;    
        else

            maxPacket = pEndpointInfo->maxPacketSize;
        /* 
         * If the data size is less than max packet size, write to the
         * EP_CFG register
         */
        if (pTrb->uActLength < maxPacket)

            {

            /* 
             * Update byte count feild of endpoint configuration register with
             * sizeToCopy
             */                   
               
            data32 = NET2280_CFG_READ (pTarget, 
                     NET2280_EP_CFG_OFFSET (endpointIndex));                     

           /* Mask off the bits for Byte Count */

           data32 &= ~NET2280_EP_CFG_EBC;
           data32  &= NET2280_EP_CFG_REG_MASK;

           data32 |= sizeToCopy << NET2280_EP_CFG_BYTE_CNT_SHIFT;
 

           /* Write the value in the EP_CFG register */

           NET2280_CFG_WRITE (pTarget, NET2280_EP_CFG_OFFSET (endpointIndex), data32);


           /* Write the data from TRB Buffer in the FIFO */

           data32 = 0;

           for (i = 0 ; i < sizeToCopy; i++ )

                data32 |= (*(pBuf + i) << (i * 8));

           /* Write the data in the Endpoint FIFO */

           NET2280_CFG_WRITE (pTarget, NET2280_EP_DATA_OFFSET (endpointIndex),
                              data32);
           }

#ifdef NET2280_DMA_SUPPORTED
        }
#endif
 
  
    USB_NET2280_DEBUG ("usbTcdNET2280FncCopyDataToEpBuf: Exiting...\n",
    0,0,0,0,0,0);

    return OK;
                 
    }   
               

#ifdef NET2280_DMA_SUPPORTED

/*******************************************************************************
*
* initializeDma - initializes the DMA
*
* This is a utility function which is used to initilize the DMA for DMA
* operations.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/

LOCAL UINT32 initializeDma
    (
    pUSB_TCD_NET2280_TARGET	pTarget,	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo	/* USB_TCD_NET2280_ENDPOINT */
    )

    {
    
    UINT8	endpointIndex = 0;
    UINT32	sizeCopied = 0;	
    UINT32	data32 = 0; 

    if ((pTarget == NULL) || (pEndpointInfo == NULL))
        {
        USB_NET2280_ERROR ("initializeDma: Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    endpointIndex = pEndpointInfo->endpointIndex;

    /* Write the address to the DMA Address Regsiter */

    NET2280_CFG_WRITE (pTarget, NET2280_DMAADDR_OFFSET (endpointIndex),
                       (UINT32) USB_MEM_TO_PCI (pEndpointInfo->dmaBuffer));

    if (pEndpointInfo->direction == USB_TCD_ENDPT_IN)

        {

        NET2280_CFG_WRITE (pTarget, NET2280_DMACOUNT_OFFSET(endpointIndex),
                           NET2280_DMACOUNT_DIR);

        /* Flush the DMA Buffer */

        USB_PCI_MEM_FLUSH ((pVOID)pEndpointInfo->dmaBuffer,
                            pEndpointInfo->dmaTransferLength);

        }

    else

        {                    

        NET2280_CFG_WRITE (pTarget, NET2280_DMACOUNT_OFFSET(endpointIndex),
                           pEndpointInfo->dmaTransferLength);

        }
     

    /* 
     * Disable the endpoint interrupts for the corresponding endpoint.
     * Enable short packet OUT done interrupt.
     */

    data32 = NET2280_EP_IRQENB_SPOD;

    NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex), 
                      data32);        

    /* clear the endpoint interrupt */

    pTarget->endptIntPending &= ~(1 << endpointIndex); 

    /* Clear all the interrupts for this endpoint */
    data32 = NET2280_CFG_READ (pTarget, 
                             NET2280_EP_STAT_OFFSET(endpointIndex));

    data32 &= ~NET2280_EP_STAT_SPOD; 

    NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex),data32);
 
    /*
     * Set the DMA Enable bit of DMA Control Register.
     * If it is an IN endpoint and the transfer length is not a
     * multiple of maxpacket size,  then set 
     * the bit which validates the FIFO after a dma transfer.
     */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DMACTL_OFFSET(endpointIndex));

    if ((pEndpointInfo->direction == USB_TCD_ENDPT_IN) &&
        ((pEndpointInfo->dmaTransferLength % pEndpointInfo->maxPacketSize) != 0))
        data32 |= (NET2280_DMACTL_EN | NET2280_DMACTL_FIFOVAL);    
    else
        {
        data32 &= ~NET2280_DMACTL_FIFOVAL;
        data32 |= NET2280_DMACTL_EN;
        }

    NET2280_CFG_WRITE (pTarget, NET2280_DMACTL_OFFSET (endpointIndex),
                       data32);

    /* Set the DMA Done Interrupt Enable of DMA Count Regsiter */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DMACOUNT_OFFSET(endpointIndex));

    NET2280_CFG_WRITE (pTarget, NET2280_DMACOUNT_OFFSET (endpointIndex),
                       (data32 | NET2280_DMACOUNT_DIE));

    /* Update DMA Transfer Length in DMA Count Register */
 
    data32 = NET2280_CFG_READ (pTarget, NET2280_DMACOUNT_OFFSET(endpointIndex));

    /* Clear the byte count fields */

    data32 &= ~NET2280_DMACOUNT_BC;

    NET2280_CFG_WRITE (pTarget, NET2280_DMACOUNT_OFFSET (endpointIndex),
                       (data32 | pEndpointInfo->dmaTransferLength));
    
    /* Reset DMA EOT for the Endpoint */

    pTarget->dmaEot &= ~(1 << endpointIndex);

    /* Set dmaInUse to TRUE */

    pEndpointInfo->dmaInUse = TRUE;

    /* Update the size which has to be updated in the TRB */

    if (pEndpointInfo->direction == USB_TCD_ENDPT_IN)

        sizeCopied = pEndpointInfo->dmaTransferLength;
  
    else
         
        sizeCopied = 0;

    /* Enable the dma interrupt for this endpoint */

    data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

    data32 |= NET2280_IRQENB1_DMA(endpointIndex);

    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

    /* Start the DMA Transfer */

    data32 = NET2280_CFG_READ (pTarget, NET2280_DMASTAT_OFFSET (endpointIndex));
  
    NET2280_CFG_WRITE (pTarget, NET2280_DMASTAT_OFFSET (endpointIndex),
                       (data32 | NET2280_DMASTAT_START));

    /* Return the size of data which has to be copied */

    return sizeCopied;
     
    }  

    
/*******************************************************************************
*
* disableDma - disableDma the DMA
*
* This is a utility function which is used to disable the DMA
* operations.
*
* RETURNS: N/A
*
* ERRNO:
*  none
*
* \NOMANUAL
*/


LOCAL VOID disableDma 
    (
    pUSB_TCD_NET2280_TARGET	pTarget,	/* USB_TCD_NET2280_TARGET */
    pUSB_TCD_NET2280_ENDPOINT	pEndpointInfo	/* USB_TCD_NET2280_ENDPOINT */
    )

    {

    UINT32	data32 = 0;			/* temporary variable */
    UINT8	endpointIndex = 0;

    if ((pTarget == NULL) || (pEndpointInfo == NULL))
        {
        USB_NET2280_ERROR ("disableDma: Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return;
        }
    
    endpointIndex = pEndpointInfo->endpointIndex;

    /* reset the usbTcdNet2280Target :: dmaEot */

    pTarget->dmaEot &= ~(1 << endpointIndex);

    /* clear the endpoint interrupt */

    pTarget->endptIntPending &= ~(1 << endpointIndex); 

    /* Set USB_TCD_NET2280_ENDPOINT :: dmaInUse to FALSE */
        
    pEndpointInfo->dmaInUse = FALSE;
  
    /* Disable the DMA */

    /*
     * disable the DMA INTERRUPT ENABLE for the endpoint from 
     * PCIIRQENB1
     */
                     
    data32 = NET2280_CFG_READ (pTarget, NET2280_PCIIRQENB1_REG);

    data32 &= ~NET2280_IRQENB1_DMA(endpointIndex);

    NET2280_CFG_WRITE (pTarget, NET2280_PCIIRQENB1_REG, data32);

    /* Reset the DMA Enable bit of DMACTL register */

    data32 = NET2280_CFG_READ (pTarget, 
             NET2280_DMACTL_OFFSET (endpointIndex));

    data32 &= ~NET2280_DMACTL_EN;

    NET2280_CFG_WRITE (pTarget, NET2280_DMACTL_OFFSET (endpointIndex),
                       data32);

    /* Reset the dma count field */

    NET2280_CFG_WRITE (pTarget, NET2280_DMACOUNT_OFFSET (endpointIndex), 0);

    /* Reset the dma address field */

    NET2280_CFG_WRITE (pTarget, NET2280_DMAADDR_OFFSET (endpointIndex), 0);

    /* 
     * Abort the dma transfer and clear the TD done interrupt.
     * This is effective only when there is a short packet detected
     * on a OUT endpoint.
     */
 
    NET2280_CFG_WRITE (pTarget, 
                       NET2280_DMASTAT_OFFSET (endpointIndex),
                       (NET2280_DMASTAT_ABORT | NET2280_DMASTAT_TD_INT));

    /* Update the dma buffer pointer */

    pEndpointInfo->dmaBuffer = NULL;

    /* Enable the interrupt for the endpoint */


    /* Clear the interrupt status */

    data32 = (NET2280_EP_STAT_DPT | NET2280_EP_STAT_DPR);

    NET2280_CFG_WRITE (pTarget, NET2280_EP_STAT_OFFSET(endpointIndex), 
                      data32);

    /* Reenable the interrupts */

    data32 = (NET2280_EP_IRQENB_DPT | NET2280_EP_IRQENB_DPR);

    NET2280_CFG_WRITE (pTarget, NET2280_EP_IRQENB_OFFSET(endpointIndex), 
                      data32);        

    return;
    }    
#endif

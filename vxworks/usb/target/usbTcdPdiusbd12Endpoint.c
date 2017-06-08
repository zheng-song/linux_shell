/* usbTcdPdiusbd12Endpoint.c - Endpoint related functionalities of PDIUSBD12 */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01g,17sep04,ami  WindView Instrumentation Changes
01f,02aug04,ami  Warning Messages Removed
01e,29jul04,pdg  Diab warnings fixed
01d,19jul04,ami  Coding Convention Changes
01c,03may04,pdg  Fix for DMA bugs
01b,27apr04,pdg  Testing bug fixes
01a,15mar04,mat  First.
*/

/*
DESCRIPTION
This file implements the endpoint related functionalities of TCD
(Target Controller Driver) for the Philips PDIUSBD12.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/target/usbIsaLib.h,
               string.h, drv/usb/target/usbPdiusbd12Eval.h,
               drv/usb/target/usbTcdPdiusbd12EvalLib.h,
               drv/usb/target/usbPdiusbd12Tcd.h,
               drv/usb/target/usbPdiusbd12Debug.h,
               usb/target/usbPeriphInstr.h
*/

/* includes */

#include "usb/usbPlatform.h"                    
#include "usb/ossLib.h" 		     
#include "string.h"                          
#include "drv/usb/target/usbPdiusbd12Eval.h"	
#include "drv/usb/target/usbTcdPdiusbd12EvalLib.h"
#include "drv/usb/target/usbPdiusbd12Tcd.h"     
#include "drv/usb/target/usbPdiusbd12Debug.h"
#include "usb/target/usbPeriphInstr.h"     
  

/* globals */

/*  In order to demonstrate the proper operation of the Philips
 * PDIUSBD12 with DMA, we use these buffers for DMA transfers.
 */

IMPORT UINT	sysFdBuf;		/* physical address of DMA bfr */
IMPORT UINT	sysFdBufSize;		/* size of DMA buffer */

/* functions */

/***************************************************************************
*
* usbTcdPdiusbd12FncEndpointAssign - implements TCD_FNC_ENDPOINT_ASSIGN
*
* This function assigns an endpoint for a specific kind of transfer.
*
* RETURNS: OK or ERROR if failed to configure the endpoint
*
* ERRNO:
*  None
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncEndpointAssign
    (
    pTRB_ENDPOINT_ASSIGN	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    UINT8	transferType = 0;		/* transfer type */
    UINT8	direction = 0;			/* direction */
    UINT16	maxPacketSize = 0;		/* max packet size */
    pUSB_TCD_PDIUSBD12_ENDPOINT	pEndpointInfo = NULL; 
						/* USB_TCD_PDIUSBD12_ENDPOINT*/
    pUSB_ENDPOINT_DESCR		pEndptDescr = NULL;
						/* pUSB_ENDPOINT_DESCR */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    UINT8	endpointNo = 0;			/* endpoint number */
    UINT8	endpointIndex = 0;		/* endpoint index */
    BOOL	indexFound= FALSE;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointAssign entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   


    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pEndpointDesc == NULL))
	{

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   


        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Invalid  \
        Parameter...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* Get the endpoint descriptor */

    pEndptDescr = pTrb->pEndpointDesc;

    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* determine the transfer type */

    transferType = pEndptDescr->attributes & USB_ATTR_EPTYPE_MASK;

    /* determine the direction */

    if ((pEndptDescr->endpointAddress & USB_ENDPOINT_DIR_MASK) != 0)
        direction = USB_ENDPOINT_IN;
    else
        direction = USB_ENDPOINT_OUT;

    /* determine the max packet size */

    maxPacketSize = pEndptDescr->maxPacketSize;

    /* Extract the endpoint number */

    endpointNo = pEndptDescr->endpointAddress & USB_ENDPOINT_MASK;

    /* determine if the maxpacket size can be supported */

    if (((transferType == USB_ATTR_CONTROL) && 
         (maxPacketSize >D12_MAX_PKT_CONTROL))||
        ((transferType == USB_ATTR_BULK) &&  
         (endpointNo == D12_ENDPOINT_NO_1) && 
         (maxPacketSize > D12_MAX_PKT_ENDPOINT_1))||
        ((transferType == USB_ATTR_BULK) &&  (endpointNo == D12_ENDPOINT_NO_2) && 
         (maxPacketSize > D12_MAX_PKT_ENDPOINT_2_NON_ISO))||
        ((transferType == USB_ATTR_INTERRUPT) &&  
         (endpointNo == D12_ENDPOINT_NO_1) &&
         (maxPacketSize > D12_MAX_PKT_ENDPOINT_1))||
        ((transferType == USB_ATTR_INTERRUPT) && 
         (endpointNo == D12_ENDPOINT_NO_2) &&
         (maxPacketSize > D12_MAX_PKT_ENDPOINT_2_NON_ISO))||
        ((transferType == USB_ATTR_ISOCH) && 
                         (maxPacketSize > D12_MAX_PKT_ENDPOINT_2_ISO_IO)))
        {
        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Max Packet \
        Size not supported...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Max Packet Size not supported...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        return ERROR;
        }

    /* determine a valid endpoint number and index*/

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Determining \
    Valid endpoint number and Index...\n",0,0,0,0,0,0);

    if(transferType == USB_ATTR_CONTROL && direction == USB_ENDPOINT_OUT)
        {
        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Control and Direction is Out...",0,0,0,0,0,0);

        /* detemine if endpoint index 0 is free */

        if((pTarget->endpointIndexInUse & (0x1<<D12_ENDPOINT_CONTROL_OUT))==0)
            {
            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : \
            Endpoint Index  0 alloted...\n",0,0,0,0,0,0);
            endpointIndex = D12_ENDPOINT_CONTROL_OUT;
            indexFound = TRUE;
            }
        }

    else if(transferType == USB_ATTR_CONTROL && direction == USB_ENDPOINT_IN)
        {
        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Control and Direction is In...",0,0,0,0,0,0);

        /* determine if endpoint index 1 is free */

        if((pTarget->endpointIndexInUse & ( 0x1<<D12_ENDPOINT_CONTROL_IN))==0)
            {
            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : \
            Endpoint Index 1 alloted...\n",0,0,0,0,0,0);
            endpointIndex = D12_ENDPOINT_CONTROL_IN;
            indexFound = TRUE;
            }
        }

    else if((transferType == USB_ATTR_BULK ||
            (transferType ==USB_ATTR_INTERRUPT)) && 
            (direction == USB_ENDPOINT_OUT))
        {
        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Bulk / Interrupt and Direction is Out...",0,0,0,0,0,0);

        /* If the endpoint number is 1, check if endpoint 1 is free */

        if ((endpointNo == D12_ENDPOINT_NO_1) && 
            ((pTarget->endpointIndexInUse & ( 0x1<<D12_ENDPOINT_1_OUT))==0))
            {
            endpointIndex = D12_ENDPOINT_1_OUT;
            indexFound = TRUE;
            }
        
        /* If the endpoint number is 2, check if endpoint 2 is free */

        else if ((endpointNo == D12_ENDPOINT_NO_2) && 
            ((pTarget->endpointIndexInUse & ( 0x1<<D12_ENDPOINT_2_OUT))==0))  
            {
            endpointIndex = D12_ENDPOINT_2_OUT;
            indexFound = TRUE;
            }
        }
    
    else if((transferType == USB_ATTR_BULK ||
           (transferType ==USB_ATTR_INTERRUPT)) && direction == USB_ENDPOINT_IN)
        {

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Bulk / Interrupt and Direction is In...",0,0,0,0,0,0);

        /* If the endpoint number is 1, check if endpoint 1 is free */

        if ((endpointNo == D12_ENDPOINT_NO_1) && 
            ((pTarget->endpointIndexInUse & ( 0x1<<D12_ENDPOINT_1_IN))==0))
            {
            endpointIndex = D12_ENDPOINT_1_IN;
            indexFound = TRUE;
            }

        /* If the endpoint number is 2, check if endpoint 2 is free */

        else if ((endpointNo == D12_ENDPOINT_NO_2) && 
            ((pTarget->endpointIndexInUse & ( 0x1<<D12_ENDPOINT_2_IN))==0))  
            {
            endpointIndex = D12_ENDPOINT_2_IN;
            indexFound = TRUE;
            }
        }

    else if(transferType == USB_ATTR_ISOCH && direction == USB_ENDPOINT_OUT)
        {

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Isochronous and Direction is Out...",0,0,0,0,0,0);

        /* determine if endpoint index 4 is free */

        if(((pTarget->endpointIndexInUse & ( 0x1 << D12_ENDPOINT_2_OUT)) == 0) &&
           (endpointNo == D12_ENDPOINT_NO_2))
            {
            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : \
            Endpoint 4 is free ...\n",0,0,0,0,0,0);
            endpointIndex = D12_ENDPOINT_2_OUT;
            indexFound = TRUE;
            }
        }

    else if(transferType == USB_ATTR_ISOCH && direction == USB_ENDPOINT_IN)
        {

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Transfer \
        Type is Isochronous and Direction is In...",0,0,0,0,0,0);

        /* determine if endpoint index 5 is free */

        if(((pTarget->endpointIndexInUse & ( 0x1 << D12_ENDPOINT_2_IN) )== 0) &&
           (endpointNo == D12_ENDPOINT_NO_2))
            {
            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : \
            Endpoint 5 is free ...\n",0,0,0,0,0,0);

            endpointIndex = D12_ENDPOINT_2_IN;
            indexFound = TRUE;
            }
        }
    else
        return ERROR;

    /* If index is not found, return ERROR */

    if ( indexFound == FALSE)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Invalid endpoint index obtained...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        /* No valid endpoint index determined */

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : No valid \
        endpoint index determined...\n ",0,0,0,0,0,0);
        return ERROR;
        }

    /* Validate the endpoint number */

    if (endpointNo != (pEndptDescr->endpointAddress & USB_ENDPOINT_MASK))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Endpoint number is Invalid...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Endpoint \
        number is invalid...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* Create a USB_TCD_PDIUSBD12_ENDPOINT structure to manage the endpoint */

    if ((pEndpointInfo = OSS_CALLOC (sizeof (USB_TCD_PDIUSBD12_ENDPOINT))) == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointAssign exiting: Failed to allocate memory...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Could not \
        allocate memory...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /*
     * Store direction, transfer type, maxpacket size, endpoint number
     * endpoint Index
     */

    pEndpointInfo->transferType = transferType;
    pEndpointInfo->direction = direction;
    pEndpointInfo->maxPacketSize = maxPacketSize;
    pEndpointInfo->endpointNo = endpointNo;
    pEndpointInfo->endpointIndex = endpointIndex;

    /* if main endpoint*/

    if (endpointNo == D12_ENDPOINT_NO_2)
        {
        
    /*
     * Check whether its a non-isochronous transfer type across the main
     * endpoint and increment epMainGenericCount
     */
        
    if ((transferType == USB_ATTR_BULK) ||
        (transferType ==USB_ATTR_INTERRUPT))
        pTarget->epMainGenericCount++;

        USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Main \
        Endpoint in use...\n",0,0,0,0,0,0);

        if (transferType == USB_ATTR_ISOCH )
            {
            
            /*
             * Check whether the main endpoint supports anny generic transfer,
             * if so retunr error
             */
            
            if (pTarget->epMainGenericCount != 0)
                {
                USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign: \
                Mode not supported...\n",0,0,0,0,0,0);

                /* Free the memory allocated */

                OSS_FREE (pEndpointInfo);

                return ERROR;
                }

            /* Clear the current mode of operation */

	    pTarget->configByte &= ~D12_CMD_SM_CFG_MODE_MASK;
	    switch (direction)
	        {
	        case USB_DIR_OUT:
                    if((pTarget->endpointIndexInUse &
                        ( 0x1 << D12_ENDPOINT_2_IN)) != 0)
                        pTarget->configByte |= D12_CMD_SM_CFG_MODE3_ISO_IO;
                    else
                       pTarget->configByte |= D12_CMD_SM_CFG_MODE1_ISO_OUT;
                    break;

		case USB_DIR_IN:
                    if((pTarget->endpointIndexInUse &
                       (0x1 << D12_ENDPOINT_2_OUT)) != 0)
                       pTarget->configByte |= D12_CMD_SM_CFG_MODE3_ISO_IO;
                    else
		         pTarget->configByte |= D12_CMD_SM_CFG_MODE2_ISO_IN;
                    break;

    	        default:
                    return ERROR;
  		 }

            USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign : Giving \
            Set Mode Command ...\n",0,0,0,0,0,0);

            d12SetMode (pTarget);
            }

            /* Increment the main endpoint count */

            pTarget->epMainCount++;
        }

    if ((endpointNo == D12_ENDPOINT_NO_2) || (endpointNo == D12_ENDPOINT_NO_1))
        {
        if (++pTarget->epOneAndTwoCount == 1)
	    d12SetEndpointEnable (pTarget, TRUE);
        }

    /* update endpointIndexInUse */

     pTarget->endpointIndexInUse |= (0x1 << endpointIndex);

    /* Enable the endpoint as necessary. */

    d12SetEndpointStatus (pTarget, endpointIndex, 0);    /* reset to DATA0 */

    /* Clear the buffer if it is an OUT endpoint */

    if (direction == USB_ENDPOINT_OUT)
        {
        d12SelectEndpoint(pTarget,endpointIndex);	
        d12ClearBfr(pTarget);
        } 

    /* update the handle */

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign: update the \
    handle... \n",0,0,0,0,0,0);

    /* Store the handle */

    pTrb->pipeHandle = (UINT32)pEndpointInfo;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointAssign exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointAssign: Exiting \
    ...\n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncEndpointRelease - implements TCD_FNC_ENDPOINT_RELEASE
*
* This function assigns an endpoint for a specific kind of transfer.
*
* RETURNS: OK or ERROR if failed to unconfigure the endpoint
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/


LOCAL STATUS usbTcdPdiusbd12FncEndpointRelease
    (
    pTRB_ENDPOINT_RELEASE	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER		pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    pUSB_TCD_PDIUSBD12_ENDPOINT pEndpointInfo = NULL;
						/* USB_TCD_PDIUSBD12_ENDPOINT */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointRelease entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointRelease: Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointRelease exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncEndpointRelease : Invalid \
        parameters...\n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointRelease exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointAssign : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* update endpointIndexInUse */

    pTarget->endpointIndexInUse &= ~(0x1 << pEndpointInfo->endpointIndex);

    /* if main endpoint */

    if ( pEndpointInfo->endpointNo == D12_ENDPOINT_NO_2  )
        {
        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointRelease : Releasing \
        the main endpoint...\n",0,0,0,0,0,0);

        pTarget->epMainCount-- ;

        if ((pEndpointInfo->transferType == USB_ATTR_BULK) ||
           (pEndpointInfo->transferType ==USB_ATTR_INTERRUPT))
            if ( pTarget->epMainGenericCount != 0)
                pTarget->epMainGenericCount--;

        /* Check if the transfer type is isochronous */

        if (pEndpointInfo->transferType == USB_ATTR_ISOCH )
	    {
            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointRelease : Setting \
            the mode of operation ...\n",0,0,0,0,0,0);

	    /* Clear the current mode of operation */

            pTarget->configByte &= ~D12_CMD_SM_CFG_MODE_MASK;

            /* Switch based on the direction of endpoint */

	    switch (pEndpointInfo->direction)
	        {
	        case USB_DIR_OUT:
	            if((pTarget->endpointIndexInUse &
                       ( 0x1 << D12_ENDPOINT_2_IN)) != 0)
                        pTarget->configByte |= D12_CMD_SM_CFG_MODE2_ISO_IN;
                    else
		        pTarget->configByte |= D12_CMD_SM_CFG_MODE0_NON_ISO;
   	            break;

	        case USB_DIR_IN:
	            if((pTarget->endpointIndexInUse &
                       ( 0x1 << D12_ENDPOINT_2_OUT)) != 0)
                        pTarget->configByte |= D12_CMD_SM_CFG_MODE1_ISO_OUT;
                    else
	                pTarget->configByte |= D12_CMD_SM_CFG_MODE0_NON_ISO;
	            break;

                default:
                    return ERROR;
                }

            d12SetMode (pTarget);
            }
	}

    if ((pEndpointInfo->endpointNo == D12_ENDPOINT_NO_2) ||
         (pEndpointInfo->endpointNo == D12_ENDPOINT_NO_1))
        {
        if (--pTarget->epOneAndTwoCount == 0)
            d12SetEndpointEnable (pTarget, FALSE);
        }

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointRelease : Releasing the \
    resource alloted endpoint...\n",0,0,0,0,0,0);

    OSS_FREE(pEndpointInfo);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointRelease exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointRelease : Exiting \
    ...\n",0,0,0,0,0,0);

    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncEndpointStateSet - implements TCD_FNC_ENDPOINT_STATE_SET
*
* This function sets endpoint state as stalled or un-stalled.
*
* RETURNS: OK or ERROR, it not able to set the state of the endpoint
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/


LOCAL STATUS usbTcdPdiusbd12FncEndpointStateSet
    (
    pTRB_ENDPOINT_STATE_SET	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER		pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_ENDPOINT	pEndpointInfo = NULL;
					/* USB_TCD_PDIUSBD12_ENDPOINT */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */ 

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointStateSet entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ( "usbTcdPdiusbd12FncEndpointStateSet : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStateSet exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncEndpointStateSet: Invalid \
        Parameters ...\n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStateSet exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointStateSet : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    if(pTrb->state == TCD_ENDPOINT_STALL)
        {
        
        /* stall the endpoint */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStateSet: Stalling \
        the Endpoint ...\n",0,0,0,0,0,0);
        d12SetEndpointStatus(pTarget,
                           pEndpointInfo->endpointIndex,D12_CMD_SES_STALLED);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStateSet: Endpoint Stalled ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        }
    else if (pTrb->state == TCD_ENDPOINT_UNSTALL)
        {
        
         /* unstall the endpoint */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStateSet: Un-stalling \
        the Endpoint ...\n",0,0,0,0,0,0);
        d12SetEndpointStatus(pTarget,
                         pEndpointInfo->endpointIndex ,D12_CMD_SES_UNSTALLED);

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStateSet: Endpoint Un-Stalled ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        }
    else
        {
        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStateSet: Invalid state\
        ...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointStatusSet exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStateSet: Exiting \
    ...\n",0,0,0,0,0,0);
    
    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncEndpointStatusGet - implements TCD_FNC_ENDPOINT_STATUS_GET
*
* This function returns the status of an endpoint ie whether it is STALLED or
* not.
*
* RETURNS: OK or ERROR, it not able to get the state of the endpoint
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

STATUS usbTcdPdiusbd12FncEndpointStatusGet
    (
    pTRB_ENDPOINT_STATUS_GET	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_ENDPOINT	pEndpointInfo = NULL; 
						/* USB_TCD_PDIUSBD12_ENDPOINT */	
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL; /* USB_TCD_PDIUSBD12_TARGET */
    UINT8	byte = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointStatusGet entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStatusGet: Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0) || (pTrb->pStatus == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStatusGet exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncEndpointStatusGet: Invalid \
        Parameters ... \n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncEndpointStatusGet exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncEndpointStatusGet : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* select endpoint command */

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStatusGet: Selecting \
    Endpoint ...\n",0,0,0,0,0,0);

    byte = d12SelectEndpoint (pTarget , pEndpointInfo->endpointIndex);

    if ((byte & D12_CMD_SE_STALL) != 0)
        {
        
        /* endpoint is stalled */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStatusGet: Endpoint \
        is stalled ...\n",0,0,0,0,0,0);
        *(pTrb->pStatus) = 1;
        }
    else
        {

        /* endpoint is not in stalled state */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStatusGet: Endpoint \
        is un-stalled ...\n",0,0,0,0,0,0);
        *(pTrb->pStatus) = 0;
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncEndpointStatusGet exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   


    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncEndpointStatusGet: Exiting \
    ...\n",0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12FncIsBufferEmpty - implements TCD_FNC_IS_BUFFER_EMPTY
*
* This function returns TRUE if the FIFO buffer associated with the endpoint
* is empty..
*
* RETURNS: OK or ERROR, if there is an error in checking the buffer status.
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncIsBufferEmpty
    (
    pTRB_IS_BUFFER_EMPTY	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_ENDPOINT	pEndpointInfo = NULL;
					/* USB_TCD_PDIUSBD12_ENDPOINT */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL; /* USB_TCD_PDIUSBD12_TARGET */ 
    UINT8	byte = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncIsBufferEmpty entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncIsBufferEmpty : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncIsBufferEmpty exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncIsBufferEmpty : Invalid \
        Parameter...\n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncIsBufferEmpty exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncIsBufferEmpty : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* select endpoint command */

    byte = d12SelectEndpoint (pTarget , pEndpointInfo->endpointIndex);

    if ((byte & D12_CMD_SE_FULL_EMPTY) == 0)
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncIsBufferEmpty: Buffer is empty...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        
        /* buffer is empty */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncIsBufferEmpty : Buffer is \
        empty ...\n",0,0,0,0,0,0);
        pTrb->bufferEmpty = TRUE;
        }
    else
        {
        
        /* buffer is full */

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncIsBufferEmpty: Buffer is full...",
        USB_TCD_PDIUSBD12_WV_FILTER);   


        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncIsBufferEmpty : Buffer is \
        full ...\n",0,0,0,0,0,0);
        pTrb->bufferEmpty = FALSE;

        }
    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncIsBufferEmpty : Exiting \
    ...\n",0,0,0,0,0,0);

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncIsBufferEmpty exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   


    return OK;
    }

/*******************************************************************************
*
* usbTcdPdiusbd12CopyDMABufferData - To copy data to/from the DMA buffer
*
* The data which is copied to/from the DMA buffer is to be copied in
* the sequence as follows
* 1st packet as the 0th packet.
* 0th packet as the 1st packet.
* 3rd packet as the 2nd packet.
* 2nd packet as the 3rd packet and so on.
* This function copies data from source to destination in the above-mentioned
* sequence
*
* RETURNS: N/A
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

LOCAL VOID usbTcdPdiusbd12CopyDMABufferData
    (
    char	* pDestination,			/* Destination Address */
    char	* pSource,			/* Source to copy */
    UINT32	sizeToCopy,			/* Size of data to copy */
    UINT16	maxPacketSize                   /* Maximum packet size */
    )
    {
    UINT32 	tempSize = 0;
    UINT16	sizeCopied = 0;

    /* This loop continues till the size becomes 0 */

    while (sizeToCopy >0)
        {
        sizeCopied = 0; 

        /*
         * Check if the size to be copied is more than the
         * maximum packet size
         */ 
        if (sizeToCopy > maxPacketSize)
            {
            if (sizeToCopy < (UINT32)(2 * maxPacketSize))  
                tempSize = sizeToCopy - maxPacketSize;
            else
                tempSize = maxPacketSize;

            /* Copy the data from the 2nd packet */

            memcpy(pDestination, pSource + maxPacketSize, tempSize); 

            /* Update the destination buffer */

            pDestination += tempSize; 

            /* Decrement the size remaining to be copied */

            sizeToCopy -= tempSize;

            /* Update the size of data copied */

            sizeCopied = tempSize;

            /* Update the size of data to be copied next */ 

            tempSize = maxPacketSize; 
            }
        else
            tempSize = sizeToCopy;

        /* Copy the 1st packet */            

        memcpy(pDestination,pSource, tempSize); 

        /* Update the source pointer */ 

        pSource += (tempSize + sizeCopied);

        /* Update the destination pointer */

        pDestination += tempSize;

        /* Decrement the number of bytes remaining to be copied */

        sizeToCopy -= tempSize;
        }
    return;
    } 

/*******************************************************************************
*
* usbTcdPdiusbd12FncCopyDataFromEpBuf - implements TCD_FNC_COPY_DATA_FROM _EPBUF
*
* This function copies data from endpoint FIFO buffer into the buffer that is
* passed.
*
* RETURNS: OK  if successfully copied, ERROR otherwise.
*
* ERRNO:
*   None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncCopyDataFromEpBuf
    (
    pTRB_COPY_DATA_FROM_EPBUF	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_ENDPOINT pEndpointInfo = NULL; 
					/* USB_TCD_PDIUSBD12_ENDPOINT */
    pUSB_TCD_PDIUSBD12_TARGET pTarget = NULL;	/* pUSB_TCD_PDIUSBD12_TARGET */
    UINT32 	sizeToCopy = 0;			/* size to copy */

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncCopyDataFromEpBuf entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncCopyDataFromEpBuf exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   


        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Bad \
        Parameter...\n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncCopyDataFromEpBuf exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncCopyDataFromEpBuf : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /* Check for main endpoint */

    if (pEndpointInfo->endpointNo == D12_ENDPOINT_NO_2)
        {

        /* endpoint supports DMA */

        /* Endpoint using DMA and DMA EOT */

        if ((pTarget->dmaEndpointId == pEndpointInfo->endpointIndex) &&
            (pTarget->dmaInUse) && (pTarget->dmaEot))

            {

            /* WindView Instrumentation */

            USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
            "usbTcdPdiusbd12FncCopyDataFromEpBuf: end of DMA transfer detected...",
            USB_TCD_PDIUSBD12_WV_FILTER);   

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Endpoint supports DMA and DMA is inuse...\n",0,0,0,0,0,0);

            /* Determine the size to copy */
            if (pTrb->uActLength < pEndpointInfo->dmaXfrLen)
                sizeToCopy = pTrb->uActLength;
            else
                sizeToCopy = pEndpointInfo->dmaXfrLen;

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Invalidating DMA buffer ...\n",0,0,0,0,0,0);

            /* Invalidate DMA buffer */

            USB_ISA_MEM_INVALIDATE ((char *) sysFdBuf, sizeToCopy);

            /* Copy the data from the DMA buffer */ 

            usbTcdPdiusbd12CopyDMABufferData((char *)pTrb->pBuffer,
                                             (char *) sysFdBuf,
                                             sizeToCopy,
                                             pEndpointInfo->maxPacketSize);

            /* update size copied */

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Size to copy = %d...\n", sizeToCopy ,0,0,0,0,0);

            pTrb->uActLength = sizeToCopy;

            /* Disable the D12's DMA and re-enable interrupts for endpoint */

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Disabling the DMA operation...\n",0,0,0,0,0,0);

	    pTarget->dmaByte &= ~D12_CMD_SD_DMA_ENABLE;

            pTarget->dmaByte |= D12_CMD_SD_ENDPT_2_OUT_INTRPT;

	    d12SetDma (pTarget);

            /* reset DMA related variables */

            pTarget->dmaInUse = FALSE;
            pTarget->dmaEndpointId = 0;
            pTarget->dmaEot = FALSE;
            pEndpointInfo->dmaXfrLen = 0;
            }

        /* endpoint using DMA and no DMA EOT */

        else if ((pTarget->dmaInUse) &&
                 (pTarget->dmaEndpointId == pEndpointInfo->endpointIndex) &&
                 (pTarget->dmaEot == FALSE))
            {

            USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Returning ok...\n",0,0,0,0,0,0);

            /*
             * This condition can happen when there is a delay between 
             * initiating the dma and the next OUT token
             * So return an OK.
             */
              
            pTrb->uActLength = 0;
            return OK;
            }

        /* dma channel not used */

        else if(pTarget->dmaInUse == FALSE)
            {

            /* update dmaXfrLen */

            if (pTrb->uActLength < sysFdBufSize)
                pEndpointInfo->dmaXfrLen =  pTrb->uActLength;
            else
                pEndpointInfo->dmaXfrLen = sysFdBufSize;


            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : \
            Initialize D12 for DMA operation...\n",0,0,0,0,0,0);

            /* Initialize D12 for DMA operation */

            USB_ISA_DMA_SETUP (DMA_MEM_WRITE, sysFdBuf, pEndpointInfo->dmaXfrLen,
                               pTarget->dma);

            pTarget->dmaByte &= ~D12_CMD_SD_DMA_DIRECTION_MASK;

            pTarget->dmaByte &= ~D12_CMD_SD_ENDPT_2_OUT_INTRPT;
	    pTarget->dmaByte |= D12_CMD_SD_DMA_DIRECTION_READ;

	    pTarget->dmaByte |= D12_CMD_SD_DMA_ENABLE;

	    d12SetDma (pTarget);

	    /* initialize DMA related variables */

            pTarget->dmaInUse = TRUE;
            pTarget->dmaEndpointId = pEndpointInfo->endpointIndex;
            pTarget->dmaEot = FALSE;
            pTrb->uActLength = 0;
            }
        }
    else
        {
        char * pBuf = (char *)pTrb->pBuffer;

        /* Select the endpoint */

        d12SelectEndpoint (pTarget, pEndpointInfo->endpointIndex);

        /* Read the number of bytes in the buffer. */

        OUT_D12_CMD (pTarget,D12_CMD_READ_BUFFER);
        
        /* throw away first byte */

        sizeToCopy = IN_D12_DATA (pTarget);	    
        sizeToCopy = IN_D12_DATA (pTarget);

        if (sizeToCopy > pTrb->uActLength)
            sizeToCopy = pTrb->uActLength;

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Size to \
        copy is %d...\n",sizeToCopy,0,0,0,0,0);

        /* update  uActLength */

        pTrb->uActLength = sizeToCopy;

        /* Read buffer data */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Reading \
        buffer data...\n",0,0,0,0,0,0);

        while (sizeToCopy > 0 )
            {
            *pBuf  = IN_D12_DATA (pTarget);
            pBuf++;
            --sizeToCopy;
            }

        /* Issue command to flush (clear) buffer */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Clearing \
        buffer...\n",0,0,0,0,0,0);


        /*
         * If a setup ERP is pending for the control OUT endpoint, then
         * acknowledge the setup ERP to enable the IN and the OUT buffers
         */

        if ((pTarget->setupErpPending) && 
            (pEndpointInfo->endpointIndex == D12_ENDPOINT_CONTROL_OUT))
            {
            d12AckSetup ( pTarget , D12_ENDPOINT_CONTROL_IN);
            d12AckSetup ( pTarget , D12_ENDPOINT_CONTROL_OUT );
            pTarget->setupErpPending = FALSE;
            } 

        /* Clear the buffer */

        d12ClearBfr (pTarget);
        }
    
    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncCopyDataFromEpBuf exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataFromEpBuf : Exiting \
    ...\n",0,0,0,0,0,0);
    return OK;
	}

/******************************************************************************
*
* usbTcdPdiusbd12FncCopyDataToEpBuf - implements TCD_FNC_COPY_DATA_TO _EPBUF
*
* This function copies data from user buffer to endpoint FIFO buffer.
*
* RETURNS: OK  if successfully copied, ERROR otherwise.
* 
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL STATUS usbTcdPdiusbd12FncCopyDataToEpBuf
    (
    pTRB_COPY_DATA_TO_EPBUF	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_PDIUSBD12_ENDPOINT	pEndpointInfo = NULL;
					/* USB_TCD_PDIUSBD12_ENDPOINT */
    pUSB_TCD_PDIUSBD12_TARGET	pTarget = NULL;	/* USB_TCD_PDIUSBD12_TARGET */
    UINT32	sizeToCopy = 0;			/* size to copy */
    UINT16	maxPacketSize = 0;		/* max packet size */
    UINT8	i = 0;

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncCopyDataToEpBuf entered ...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : Entered \
    ...\n",0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0))
        
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncCopyDataToEpBuf exiting: Bad Paramter Received ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncCopyDataToEpBuf : Invalid \
        Parameters...\n",0,0,0,0,0,0);
        return ERROR;
        }

    pEndpointInfo = (pUSB_TCD_PDIUSBD12_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_PDIUSBD12_TARGET)pHeader->handle;

    /* Check if target pointer is valid */

    if ( pTarget == NULL )
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
        "usbTcdPdiusbd12FncCopyDataToEpBuf exiting: Error assigning pTarget ...",
        USB_TCD_PDIUSBD12_WV_FILTER);   

        USBPDIUSBD12_ERROR ( "usbTcdPdiusbd12FncCopyDataToEpBuf : Error \
        assigning pTarget...\n",0,0,0,0,0,0);
        return ERROR;
        }

    /*
     * check if the endpoint number is 2, and the type is not interrupt.
     * Then only, DMA can be used
     */

    if ((pEndpointInfo->endpointNo == D12_ENDPOINT_NO_2) &&
        (pEndpointInfo->transferType != USB_ATTR_INTERRUPT))
  	{

    	/* Endpoint using DMA and DMA EOT */

    	if ((pTarget->dmaInUse) &&
            (pTarget->dmaEndpointId == pEndpointInfo->endpointIndex) &&
                 (pTarget->dmaEot == FALSE))
            {
            USBPDIUSBD12_ERROR ("usbTcdPdiusbd12FncCopyDataToEpBuf :  \
            returns ok...\n",0,0,0,0,0,0);

            /*
             * This condition can happen when there is a delay between 
             * initiating the dma and the next IN token
             * So return an OK.
             */
              
            pTrb->uActLength = 0;
            return OK;
            }

        /* Endpoint is not using DMA and DMA channel is unused */

        else if (pTarget->dmaInUse == FALSE)
            {

            /* update dmaXferlen */

            if (pTrb->uActLength < sysFdBufSize)
                pEndpointInfo->dmaXfrLen = pTrb->uActLength;
            else
                pEndpointInfo->dmaXfrLen = sysFdBufSize;

            memcpy ((char *)sysFdBuf , pTrb->pBuffer , pEndpointInfo->dmaXfrLen );

            /* Flush the DMA buffer */

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : \
            Flushing DMA buffer ...\n",0,0,0,0,0,0);

            USB_ISA_MEM_FLUSH ((pVOID)sysFdBuf ,pEndpointInfo->dmaXfrLen );

            USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : \
            Initialize D12 for DMA operation...\n",0,0,0,0,0,0);

            /* Initialize D12 for DMA operation */

            USB_ISA_DMA_SETUP (DMA_MEM_READ, sysFdBuf, pEndpointInfo->dmaXfrLen,
                               pTarget->dma);

            pTarget->dmaByte &= ~D12_CMD_SD_DMA_DIRECTION_MASK;

            pTarget->dmaByte &= ~D12_CMD_SD_ENDPT_2_IN_INTRPT;
            pTarget->dmaByte |= D12_CMD_SD_DMA_DIRECTION_WRITE;

            pTarget->dmaByte |= D12_CMD_SD_DMA_ENABLE;

	    d12SetDma (pTarget);

            /* initialize DMA related variables */

            pTarget->dmaInUse = TRUE;
            pTarget->dmaEndpointId = pEndpointInfo->endpointIndex;
            pTarget->dmaEot = FALSE;
            pTrb->uActLength = pEndpointInfo->dmaXfrLen ;
            }
        }
    else
        {

        /* Give Select command */

        d12SelectEndpoint (pTarget, pEndpointInfo->endpointIndex);

        /* Determine the max packet size of the endpint */

        maxPacketSize = pEndpointInfo->maxPacketSize ;

        if (pTrb->uActLength < maxPacketSize )
            sizeToCopy = pTrb->uActLength;
        else
            sizeToCopy = maxPacketSize;

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : Size to \
        write is %d...\n", sizeToCopy, 0,0,0,0,0);

        /* Give Write Command */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : Giving \
        Write Command ...\n",0,0,0,0,0,0);

        OUT_D12_CMD (pTarget,D12_CMD_WRITE_BUFFER);

        /* Write the first byte as 0 */

        OUT_D12_DATA (pTarget , 0 );

        /* Write the second byte as the size of the data to write */

        OUT_D12_DATA (pTarget , sizeToCopy );

        /* Write Buffer Data */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : Writing \
        Buffer...\n",0,0,0,0,0,0);

        for ( i = 0 ; i< sizeToCopy ; i++ )
            OUT_D12_DATA ( pTarget , pTrb->pBuffer[i]);

	pTrb->uActLength = sizeToCopy ;

        /* Validate the endpoint FIFO buffer */

        USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf : \
        Validating Buffer...\n",0,0,0,0,0,0);

	d12ValidateBfr (pTarget );
        }

    /* WindView Instrumentation */

    USB_TCD_LOG_EVENT(USB_TCD_PDIUSBD12_ENDPOINT,
    "usbTcdPdiusbd12FncCopyDataToEpBuf exiting...",
    USB_TCD_PDIUSBD12_WV_FILTER);   

    USBPDIUSBD12_DEBUG ("usbTcdPdiusbd12FncCopyDataToEpBuf: Exiting ...\n",
    0,0,0,0,0,0);
    return OK;
    }
    

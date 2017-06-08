/* usbTcdIsp1582Endpoint.c - Endpoint Related Routines */

/* Copyright 2004 Wind River Systems, Inc. */

/*
Modification history
--------------------
01g,17sep04,ami  WindView Instrumentation Changes
01f,02aug04,mta  Modification History Changes
01e,19jul04,ami  Coding Convention Changes
01d,14jul04,mta  ISP1582 and Mass Storage Functionality Changes
01c,08jul04,mta  ISP1582 with mass storage on SH changes
01b,30jun04,pdg  Bug fixes - isp1582 full speed testing
01a,21apr04,ami  First
*/

/*
DESCRIPTION

This file implements the endpoint related functionalities of TCD
(Target Controller Driver) for the Philips ISP 1582.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/usbPciLib.h, usb/usb.h,
               string.h, usb/target/usbHalCommon.h, usb/target/usbTcd.h
               drv/usb/target/usbisp1582.h,
               drv/usb/target/usbIsp1582Eval.h,
               drv/usb/target/usbIsp1582Tcd.h

*/

/* includes */

#include "usb/usbPlatform.h"	   
#include "usb/ossLib.h" 		     
#include "usb/usbPciLib.h"	                 
#include "usb/usb.h"                       
#include "string.h"                          
#include "usb/target/usbHalCommon.h"            
#include "usb/target/usbTcd.h"                 
#include "drv/usb/target/usbIsp1582.h"         
#include "drv/usb/target/usbIsp1582Eval.h"	  
#include "drv/usb/target/usbIsp1582Tcd.h"      
#include "usb/target/usbPeriphInstr.h"

/* defines */

#define MAX_PACK_SIZE_MASK	0x07FF       /* max packet size mask */
#define NTRANS_SIZE_MASK	0x1800       /* number of transaction per */
                                             /* frame mask */
#define NTRANS_SHIFT		0xB	     /* ntrans shift value */	

#undef DMA_SUPPORTED

/* globals */

#ifdef DMA_SUPPORTED

/* 
 *In order to demonstrate the proper operation of the Philips
 * PDIUSBD12 with DMA, we use these buffers for DMA transfers.
 */

IMPORT UINT	sysFdBuf;		/* physical address of DMA bfr */
IMPORT UINT	sysFdBufSize;		/* size of DMA buffer */

/* forward declaration */

LOCAL VOID disableDma (pUSB_TCD_ISP1582_TARGET pTarget, UINT8 endpointIndex);

LOCAL VOID initializeDma (pUSB_TCD_ISP1582_TARGET pTarget, UINT8 endpointIndex,
                          UINT8 command);

#endif

/* functions */

/*******************************************************************************
*
* usbTcdIsp1582FncEndpointAssign - implements TCD_FNC_ENDPOINT_ASSIGN
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

LOCAL STATUS usbTcdIsp1582FncEndpointAssign
    (
    pTRB_ENDPOINT_ASSIGN	pTrb		/* Trb to be executed */
    )
    {

    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_ENDPOINT_DESCR		pEndptDescr = NULL;  /* pUSB_ENDPOINT_DESCR */	
    UINT8	transferType = 0;		/* trasfer type of endpoint */
    UINT8	direction = 0;			/* direction 1-IN, 0-OUT */
    UINT16	maxPacketSize = 0;		/* max packet size */
    BOOL	indexFound = FALSE;		
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	i = 0;
    UINT8	nTrans = 0;
    UINT16	data16 = 0;
    UINT32	data32 = 0;			
    UINT8	endpointNum = 0;		/* endpoint number */

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointAssign entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointAssign : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pEndpointDesc == NULL))
    	{

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointAssign exiting...Bad Parameters Received", 
        USB_TCD_ISP582_WV_FILTER);   

    	USBISP1582_ERROR ("usbTcdIsp1582FncEndpointAssign : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    /* Get the endpoint descriptor */

    pEndptDescr = pTrb->pEndpointDesc;

    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;

    /* Determine direction */

    if ((pEndptDescr->endpointAddress & USB_ENDPOINT_DIR_MASK) != 0)
        direction = USB_ENDPOINT_IN;
    else
        direction = USB_ENDPOINT_OUT;

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
                    MAX_PACK_SIZE_MASK;

    /* Determine the number of transaction per frame and validate it. */
   
    nTrans =  (FROM_LITTLEW(pEndptDescr->maxPacketSize) & NTRANS_SIZE_MASK)
              >> NTRANS_SHIFT;
    
    /*
     * Determine if Max Packet size is supported by the hardware (Total FIFO
     * Size should not exceed 8K). If not, return ERROR.
     */

    if (pTarget->bufSize + maxPacketSize > ISP1582_FIFO_SIZE)
        {
        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointAssign exiting...ISP1582 FIFO full",
        USB_TCD_ISP582_WV_FILTER);   

        return ossStatus (S_usbTcdLib_HW_NOT_READY);
        } 

    if (transferType == USB_ATTR_CONTROL)
        {
        if (maxPacketSize > ISP1582_MAXPSIZE_64)
            {
            /* WindView Instrumentation */ 

            USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
            "usbTcdIsp1582FncEndpointAssign exiting...wrong control max packet size",
            USB_TCD_ISP582_WV_FILTER);   

            return ossStatus (S_usbTcdLib_HW_NOT_READY);
            }

        else
            maxPacketSize = ISP1582_MAXPSIZE_64;
        }

    if ((transferType == USB_ATTR_CONTROL) || (transferType == USB_ATTR_BULK))
        {
        if (nTrans != ISP1582_NTRANS_1)
            return ossStatus (S_usbTcdLib_HW_NOT_READY);
        }

    /* Determine a valid endpoint index */

    /* Control Transfer */

    if (transferType == USB_ATTR_CONTROL)
        {
        if (direction == USB_ENDPOINT_OUT)
            {

            /* 
             * Control Out Transfer.
             * If Bit 0 of endpointIndexInUse is 0, then the corresponding
             * endpoint is not in use, allocate it.
             */

            if ((pTarget->endpointIndexInUse & ( 0x1 << ISP1582_ENDPT_0_RX))==0)
                {
                indexFound = TRUE;
                endpointIndex = ISP1582_ENDPT_0_RX;
                }
            }
        else if (direction == USB_ENDPOINT_IN)
            {

            /* Control In Transfer. */

            if ((pTarget->endpointIndexInUse & ( 0x1 << ISP1582_ENDPT_0_TX))==0)
                {
                indexFound = TRUE;
                endpointIndex = ISP1582_ENDPT_0_TX;
                }
            }
        }
    else
        {

        /*
         * Generic Transfer type. Any endpoint other then control in and out
         * can be alloted. Hence we need to check only the direction and
         * allocated endpoints accordingly.
         */

        if (direction == USB_ENDPOINT_OUT)
            {
            i = endpointNum * 2;
            if ((pTarget->endpointIndexInUse & ( 0x1 << i)) == 0)
                {
                indexFound = TRUE;
                endpointIndex = i;
                }
            }
        else if (direction == USB_ENDPOINT_IN)
            {
            i = endpointNum * 2 + 1;
            if ((pTarget->endpointIndexInUse & ( 0x1 << i)) == 0)
                {
                indexFound = TRUE;
                endpointIndex = i;
                }
            }
        }

    if ( indexFound == FALSE )

        {
        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointAssign exiting...endpoint index not found",
        USB_TCD_ISP582_WV_FILTER);   

        /* Valid Index not found */

        return ossStatus (S_usbTcdLib_GENERAL_FAULT);
        } 

    /* Allocate USB_TCD_ISP1582_ENDPOINT structure */

    if ((pEndpointInfo = OSS_CALLOC (sizeof (USB_TCD_ISP1582_ENDPOINT)))== NULL)
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointAssign exiting...memory not free",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointAssign : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_OUT_OF_MEMORY);
        }

    /*
     * Store Direction, transfer type, max packet size and endpoint index in
     * the USB_TCD_ISP1582_ENDPOINT.
     */

    pEndpointInfo->transferType = transferType;
    pEndpointInfo->direction = direction;
    pEndpointInfo->maxPacketSize = FROM_LITTLEW (pEndptDescr->maxPacketSize) & 
                                   MAX_PACK_SIZE_MASK;
    pEndpointInfo->endpointIndex = endpointIndex;
    pEndpointInfo->isDoubleBufSup = FALSE;

    /*
     * If transfer type is bulk or isochronous, endpoint may support double
     * buffering.
     */

#ifdef DOUBLEBUF_SUPPORT

    if (((transferType == USB_ATTR_BULK) || (transferType == USB_ATTR_ISOCH))
          && (pTarget->bufSize + 2 * maxPacketSize <= ISP1582_FIFO_SIZE))
        {

        pTarget->bufSize += (2 * maxPacketSize);
        pEndpointInfo->isDoubleBufSup = TRUE;

        }

    else
        pTarget->bufSize += maxPacketSize;
#else
        pTarget->bufSize += maxPacketSize;
#endif


    /* Update endpointIndexInUse */

    pTarget->endpointIndexInUse |=  (1 << endpointIndex);

    /*
     * If this endpoint is alloted to the DMA as an un-used endpoint, alot any
     * other un-used endpoint to the DMA and write into the DMA endpoint
     * register.
     */

    if (endpointIndex == pTarget->dmaEndptNotInUse)
        {
        for (i = ISP1582_ENDPT_7_TX ; i >= ISP1582_ENDPT_1_RX ; i--)
            {
            if ((pTarget->endpointIndexInUse & (0x1 << i)) == 0)
                break;
            }
        if ( i < ISP1582_ENDPT_1_RX)
            {

            /* WindView Instrumentation */ 

            USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
            "usbTcdIsp1582FncEndpointAssign exiting...no unused endpoint",
            USB_TCD_ISP582_WV_FILTER);   


            USBISP1582_ERROR ("usbTcdIsp1582FncEndpointAssign : No Unused  \
            endpoint...\n",0,0,0,0,0,0);
            return ossStatus (S_usbTcdLib_HW_NOT_READY);
            }

        /* Initialize the DMA Endpoint Register to endpoint not used. */

        isp1582Write8 (pTarget , ISP1582_DMA_ENDPT_REG , i);
        pTarget->dmaEndptNotInUse = i;
        }

    /* Initialize the endpoint index register */

    isp1582Write8 ( pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    /* Initialize Max Packet Size Register */

    isp1582Write16 (pTarget , ISP1582_ENDPT_MAXPSIZE_REG ,
              ISP1582_ENDPT_MAXPSIZE_REG_SHIFT_NTRANS (nTrans) | maxPacketSize);

    /* Initialize Buffer Length Register */
    isp1582Write16 (pTarget , ISP1582_BUF_LEN_REG ,maxPacketSize);


    /* Form the value to be written into endpoint type register */

    /* Determine transfer type */

    switch (transferType)
        {
        case USB_ATTR_ISOCH:
            data16 = ISP1582_ENDPT_TYPE_REG_EPTYPE_ISO |
                     ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE;
            if (pEndpointInfo->isDoubleBufSup)
                data16 |= ISP1582_ENDPT_TYPE_REG_DOUBLE_BUF;
            break;

        case USB_ATTR_BULK :
            data16 = ISP1582_ENDPT_TYPE_REG_EPTYPE_BULK |
                     ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE;
            
            if (pEndpointInfo->isDoubleBufSup)
                data16 |= ISP1582_ENDPT_TYPE_REG_DOUBLE_BUF;
            break;

        case USB_ATTR_INTERRUPT :
            data16 = ISP1582_ENDPT_TYPE_REG_EPTYPE_INTP |
                     ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE;
            break;

        case USB_ATTR_CONTROL :
            data16 = ISP1582_ENDPT_TYPE_REG_EPTYPE_NOT_USED |
                     ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE;
            break;

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointAssign exiting... Wrong Transfer Type", 
        USB_TCD_ISP582_WV_FILTER);   

        default : return ERROR;
        }


    /* Initialize endpoint type register */

    isp1582Write16 (pTarget , ISP1582_ENDPT_TYPE_REG ,
             data16 & ISP1582_ENDPT_TYPE_REG_MASK );

    /*
     * Clear the buffer by setting bit CLBUF of Control Function Register
     * if it is a OUT transfer type
     */

    if ( direction == USB_ENDPOINT_OUT)
        isp1582Write8(pTarget, ISP1582_CNTL_FUNC_REG,
                      ISP1582_CNTL_FUNC_REG_CLBUF);

    /* Read Interrupt Enable Regsiter */
    data32 = isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG);

    /*
     * Form the data to write into Interrupt Enable Register. If control
     * endpoint,set EP0SETUP bit also.
     */

    if ( endpointIndex == ISP1582_ENDPT_0_RX ||
         endpointIndex == ISP1582_ENDPT_0_TX)
        data32 |= (ISP1582_INT_ENABLE_REG_IEP0SETUP |
                 ISP1582_INT_ENABLE_REG_ENDPT_SET (endpointIndex));
    else
        data32 |= ISP1582_INT_ENABLE_REG_ENDPT_SET (endpointIndex);

    /* Set appropiate bit of Interrupt Enable Register */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , data32);

    /* Store the handle */

    pTrb->pipeHandle = (UINT32)pEndpointInfo;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointAssign exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointAssign : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncEndpointRelease - implements TCD_FNC_ENDPOINT_RELEASE
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

LOCAL STATUS usbTcdIsp1582FncEndpointRelease
    (
    pTRB_ENDPOINT_RELEASE	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	direction = 0;			/* direction */
    UINT32	data32 = 0;		/* variable to read write in register */

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointRelease entered...", USB_TCD_ISP582_WV_FILTER);   

    
    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointRelease : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointRelease exiting...Bad Parameters Received",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointRelease : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus (S_usbTcdLib_BAD_PARAM);
        }

    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;
    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;

    endpointIndex = pEndpointInfo->endpointIndex;
    direction = pEndpointInfo->direction;

    /* Initialize endpoint Index regsiter */

    isp1582Write8 ( pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    /* Disable the endpoint by writing Logic 0 into endpoint type register */

    isp1582Write16 ( pTarget , ISP1582_ENDPT_TYPE_REG , 0);

    /* update endpointIndexInUse member indicating that the endpoint is free */

    pTarget->endpointIndexInUse &= ~(0x1 << endpointIndex);

    /*
     * Decrement the bufSize member of TARGET structure. If transfer type is
     * bulk or isochronous decrement with twice the maxpacket size.
     */

    if (pEndpointInfo->isDoubleBufSup)
        pTarget->bufSize -= (2 * pEndpointInfo->maxPacketSize);
    else
        pTarget->bufSize -= pEndpointInfo->maxPacketSize;

    /*
     * Get the data from interrupt enable register and reset corresponding
     * endpoint bit from it. If its a control endpoint reset EP0SET bit also.
     */

    /* Read Interrupt Enable Regsiter */

    data32 = isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG);

    /*
     * Form the data to write into Interrupt Enable Register. If control
     * endpoint,reset EP0SETUP bit also.
     */

    if ( endpointIndex == ISP1582_ENDPT_0_RX ||
         endpointIndex == ISP1582_ENDPT_0_TX)
        data32 &= ~(ISP1582_INT_ENABLE_REG_IEP0SETUP |
                    ISP1582_INT_ENABLE_REG_ENDPT_SET (endpointIndex));
    else
        data32 &= ~ISP1582_INT_ENABLE_REG_ENDPT_SET (endpointIndex);

    /* Set appropiate bit of Interrupt Enable Register */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , data32);


    /* Release the USB_TCD_ISP1582_ENDPOINT structure */

    OSS_FREE (pEndpointInfo);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointRelease exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointRelease : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncEndpointStateSet - implements TCD_FNC_ENDPOINT_STATE_SET
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

LOCAL STATUS usbTcdIsp1582FncEndpointStateSet
    (
    pTRB_ENDPOINT_STATE_SET	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	data8 = 0;			
    UINT16	data16 = 0;
    
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointStateSet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStateSet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pTrb->pipeHandle == 0) || (pHeader->handle == NULL))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointStateSet exiting...Bad Parameters Received",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointStateSet : Bad Parameters \
        ...\n",0,0,0,0,0,0);

        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /* Set the Endpoint Index Register */

    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , 
                                  endpointIndex & ISP1582_ENDPT_INDEX_REG_MASK);

    /* Read the Control Function Register */

    data8 = isp1582Read8 (pTarget , ISP1582_CNTL_FUNC_REG) &
            ISP1582_CNTL_FUNC_REG_MASK;

    if (pTrb->state == TCD_ENDPOINT_STALL)
        {

        /*
         * Stall the endpoint by writing Control Function Register with
         * Stall bit set.
         */

         USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStateSet : Stalling...\n",
         0,0,0,0,0,0);

         isp1582Write8 (pTarget , ISP1582_CNTL_FUNC_REG ,
                                           data8 | ISP1582_CNTL_FUNC_REG_STALL);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointStateSet: endpoints stalled",
        USB_TCD_ISP582_WV_FILTER);   

        }

    else if (pTrb->state == TCD_ENDPOINT_UNSTALL)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStateSet : Un-stalling...\n",
         0,0,0,0,0,0);

        /*
         * Un-Stall the endpoint by writing Control Function Register with
         * Stall bit reset.
         */

        data8 &= ~ISP1582_CNTL_FUNC_REG_STALL;
        isp1582Write8 (pTarget , ISP1582_CNTL_FUNC_REG ,
                       data8 & ISP1582_CNTL_FUNC_REG_MASK);


        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointStateSet: Endpoint Un-stall",
        USB_TCD_ISP582_WV_FILTER);   

        }
 
    /* Reset data toggle */

    /* Read the Endpoint Type Register */

    data16 = isp1582Read16 (pTarget , ISP1582_ENDPT_TYPE_REG) &
             ISP1582_ENDPT_TYPE_REG_MASK;

    /* Reset bit Enable and write into the register */

    data16 &= ~ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE;
    isp1582Write16 (pTarget , ISP1582_ENDPT_TYPE_REG ,
                    data16 & ISP1582_ENDPT_TYPE_REG_MASK);

    /* Set bit Enable and write into the register */

    isp1582Write16 (pTarget, ISP1582_ENDPT_TYPE_REG ,
                   (data16 | ISP1582_ENDPT_TYPE_REG_ENDPT_ENABLE) &
                   ISP1582_ENDPT_TYPE_REG_MASK);

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointStateSet exiting...",USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStateSet : Exiting...\n",
    0,0,0,0,0,0);

    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncEndpointStatusGet - implements TCD_FNC_ENDPOINT_STATUS_GET
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

LOCAL STATUS usbTcdIsp1582FncEndpointStatusGet
    (
    pTRB_ENDPOINT_STATUS_GET	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */	
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	data8 = 0;
    
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointStatusGet entered...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStatusGet : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncEndpointStatusGet exiting...Bad Parameters Received",
        USB_TCD_ISP582_WV_FILTER);   


        USBISP1582_ERROR ("usbTcdIsp1582FncEndpointStatusGet : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /* Set the Endpoint Index Register */

    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    /* Read the Control Function Register */

    data8 = isp1582Read8 ( pTarget , ISP1582_CNTL_FUNC_REG) &
            ISP1582_CNTL_FUNC_REG_MASK;

    if ((data8 & ISP1582_CNTL_FUNC_REG_STALL) != 0)

        /* Endpoint is stalled */

        *(pTrb->pStatus) = USB_ENDPOINT_STS_HALT;

    else

        /* Endpoint is not stalled. Update pStatus to 0 */

        *(pTrb->pStatus) = 0;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncEndpointStatusGet exiting...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncEndpointStatusGet : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncIsBufferEmpty - implements TCD_FNC_IS_BUFFER_EMPTY
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

LOCAL STATUS usbTcdIsp1582FncIsBufferEmpty
    (
    pTRB_IS_BUFFER_EMPTY	pTrb		/* Trb to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */	
    
    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncIsBufferEmpty entering...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncIsBufferEmpty : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncIsBufferEmpty: Bad Parameter Received", 
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncIsBufferEmpty : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;

    /* Set the Endpoint Index Register */

    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    /* Read the buffer register */

    if ((isp1582Read8 (pTarget ,ISP1582_BUF_STATUS_REG) &
         ISP1582_BUF_STATUS_REG_MASK) == ISP1582_BUF_STATUS_REG_BOTH_EMPT)
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncIsBufferEmpty : Both Buffers are \
        Empty...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncIsBufferEmpty: Both buffers are empty...",
        USB_TCD_ISP582_WV_FILTER);   


        /* Both the buffers are empty */

        pTrb->bufferEmpty = TRUE;
        }
    else
        {
        USBISP1582_DEBUG ("usbTcdIsp1582FncIsBufferEmpty : Either of the \
        Buffers is Filles...\n",0,0,0,0,0,0);

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncIsBufferEmpty: Either of buffer is filled...",
        USB_TCD_ISP582_WV_FILTER);   

        /* Atleast one of the buffer is filled */

        pTrb->bufferEmpty = FALSE;
        }

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncIsBufferEmpty exiting...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncIsBufferEmpty : Exiting...\n",
    0,0,0,0,0,0);
    return OK;
    }

/******************************************************************************
*
* usbTcdIsp1582FncCopyDataFromEpBuf - implements TCD_FNC_COPY_DATA_FROM_EPBUF
*
* This function copies the data from the endpoint FIFO buffer into the 
* buffer that is passed. If transfer types are BULK or ISOCHRONOUS and DMA 
* is not being used by any other endpoint, DMA channel is used to carry out 
* the transfer.
* Otherwise we read from the DataPort Register, 16 bits at a time.
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

LOCAL STATUS usbTcdIsp1582FncCopyDataFromEpBuf
    (
    pTRB_COPY_DATA_FROM_EPBUF	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_ENDPOINT	pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	transferType = 0;		/* transfer type */
#ifdef DMA_SUPPORTED
    UINT16	data16 = 0;
#endif
    UINT32	sizeToCopy = 0;			/* size to data to copy */
    STATUS	status = ERROR;			/* variable to hold status */
    unsigned char  * pBuf = pTrb->pBuffer;	/* pointer to buffer */
    UINT16	data = 0;			
    BOOL	bStatusNext = FALSE;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncCopyDataFromEpBuf entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0) ||
        (pTrb->pBuffer == NULL))
        {

        /* WindView Instrumentation */

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncCopyDataFromEpBuf exiting: Bad Parameter Received",
        USB_TCD_ISP582_WV_FILTER);   

        USBISP1582_ERROR ("usbTcdIsp1582FncCopyDataFromEpBuf : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;
    endpointIndex = pEndpointInfo->endpointIndex;
    transferType = pEndpointInfo->transferType;

#ifdef DMA_SUPPORTED

    /*
     * Check if the endpoint supports DMA, i.e. the transfer types are
     * bulk or isochronous.
     */

    if ((transferType == USB_ATTR_ISOCH || transferType == USB_ATTR_BULK))
        {

        /*
         * Check whether DMA Channel is in use and whether DMA transfer has
         * completed succesfully.
         */

        if ((pTarget->dmaEndpointId == endpointIndex) &&
            (pTarget->dmaInUse) && (pTarget->dmaEot))
            {
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf : DMA EOT has \
            happened...\n",0,0,0,0,0,0);

            data16 = isp1582Read16 (pTarget,ISP1582_DMA_INT_RESN_REG) &
                     isp1582Read16 (pTarget , ISP1582_DMA_INT_ENBL_REG);

            if (( data16 & ISP1582_DMA_INT_RESN_REG_DMA_XFER_OK) !=  0)
                {

                /* DMA Transfer is completed succesfully */

                /* Determine the size to copy */

                if (pTrb->uActLength < pTarget->dmaXferLen)
                    sizeToCopy = pTrb->uActLength;
                else
                    sizeToCopy = pTarget->dmaXferLen;

                /* Invalidate DMA buffer */

                USB_PCI_MEM_INVALIDATE ((char *) sysFdBuf, sizeToCopy);

                /* copy data to user suppiled buffer */

                memcpy (pTrb->pBuffer,(char *) sysFdBuf, sizeToCopy);

                /* update uActLength with the size copied */

                pTrb->uActLength = sizeToCopy;
                status = OK;
                }
            else
                {
                /* DMA has not completed normally. */
                status = ERROR;
                }
            /* Clear DMA Interrupt Reason Register */
            isp1582Write16 (pTarget , ISP1582_DMA_INT_RESN_REG ,
                                                    ISP1582_DMA_INT_RESN_MASK);

            /* Disable the DMA controller */
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf : Disabling \
            the DMA...\n",0,0,0,0,0,0);

            disableDma (pTarget , endpointIndex);
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf :Exiting...\n",
            0,0,0,0,0,0);
            return status;
            }

        else if ((pTarget->dmaEndpointId == endpointIndex) &&
                 (pTarget->dmaInUse) && (pTarget->dmaEot != TRUE))
            return ERROR;

        else if (pTarget->dmaInUse == FALSE)
            {

            /*
             * If the dma channel is unused. initialize the DMA Controller
             * to carry out the DMA operation.
             */

            /* Update dmaXferLen */

            if (pTrb->uActLength < sysFdBufSize)
                pTarget->dmaXferLen = pTrb->uActLength;
            else
                pTarget->dmaXferLen = sysFdBufSize;

            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf :Initializing \
            the DMA...\n",0,0,0,0,0,0);

            /* Initialize the DMA Controller */

            initializeDma (pTarget , endpointIndex ,
                                           ISP1582_DMA_COMMAND_REG_GDMA_WRITE);

            /* Update uActLength */

            pTrb->uActLength = 0;
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf :Exiting...\n",
            0,0,0,0,0,0);
            return OK;
            }
        }

    /* Endpoint does not support DMA or the dma channel is already in use */

#endif

    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf : Working in PIO Mode \
    ...\n",0,0,0,0,0,0);

    /* Initialize the endpoint index register */

    if ((endpointIndex == ISP1582_ENDPT_0_RX)  && 
        (pTarget->setupIntPending))

        {  
        pTarget->controlOUTStatusPending = FALSE;
        pTarget->controlINStatusPending = FALSE;
        isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG ,
                                           ISP1582_ENDPT_INDEX_REG_EP0SETUP);
        } 
    else
        isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    /*
     * If it is a control endpoint and uActLength is 0, then set bit STATUS of
     * Control Function Register.
     */

    /* Read the buffer length register */

    sizeToCopy = isp1582Read16 (pTarget , ISP1582_BUF_LEN_REG);

    /*
     * If the trbLength is the same as the size to copy, 
     * then the status stage follows next
     */

    if ((endpointIndex == ISP1582_ENDPT_0_RX) &&
        (pTrb->uActLength == sizeToCopy) &&
        (!pTarget->setupIntPending))
        bStatusNext = TRUE;    

    /* Update uActLength to size to copy */

    pTrb->uActLength = sizeToCopy;

    /*
     * Read Data Port Regsiter. It is a 16 bit access, hence sizeToCopy
     * should be decremented by 2 and pBuf should be incremented by 2
     */

    while (sizeToCopy > 0)
        {
        data = isp1582Read16( pTarget , ISP1582_DATA_PORT_REG);
        *pBuf = (char)(data & 0xFF);
        data >>= 0x08;
        sizeToCopy--;

        if (sizeToCopy > 0)
            {
            pBuf += 1;
            *pBuf = (char)(data);
            sizeToCopy--;
            if (sizeToCopy > 0)
                pBuf += 1;
            }
        }


    /*
     * Set DSEN bit if its a control endpoint and there is a data phase
     * after the setup phase.
     */

    if (endpointIndex == ISP1582_ENDPT_0_RX)
        {
        if (pTarget->setupIntPending)
            {

            /* Reuse sizeToCopy to retrieve the size of the data stage */

            sizeToCopy = pTrb->pBuffer[6] | (pTrb->pBuffer[7] << 8);

            /* If there is a data stage followed by setup stage */

            if (sizeToCopy != 0)
                {

                /*
                 * If this is an IN data stage change endpoint index
                 * to Control IN.
                 */

                if ((pTrb->pBuffer[0] & USB_ENDPOINT_DIR_MASK) != 0)
                    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG ,
                                                          ISP1582_ENDPT_0_TX);
                else
                    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG ,
                                                          ISP1582_ENDPT_0_RX);

                /* Initiate the data stage  */

                data = isp1582Read8 (pTarget , ISP1582_CNTL_FUNC_REG);
                isp1582Write8 (pTarget , ISP1582_CNTL_FUNC_REG ,
                                            data | ISP1582_CNTL_FUNC_REG_DSEN);
                }
            else
                {
                pTarget->controlINStatusPending = TRUE;
                }
            }
        else
            {

            /* If the status stage is pending, set the flag */

            if (bStatusNext)
                {
                pTarget->controlINStatusPending = TRUE;    
                }  
            }  
        }
    status = OK;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncCopyDataFromEpBuf exiting...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataFromEpBuf : Exiting...\n",
    0,0,0,0,0,0);
    return status;
    }

/*******************************************************************************
*
* usbTcdIsp1582FncCopyDataToEpBuf - implements TCD_FNC_COPY_DATA_TO_EPBUF
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

LOCAL STATUS usbTcdIsp1582FncCopyDataToEpBuf
    (
    pTRB_COPY_DATA_TO_EPBUF	pTrb		/* TRB to be executed */
    )
    {
    pTRB_HEADER	pHeader = (pTRB_HEADER) pTrb;	/* TRB_HEADER */
    pUSB_TCD_ISP1582_ENDPOINT pEndpointInfo = NULL;/*USB_TCD_ISP1582_ENDPOINT*/
    pUSB_TCD_ISP1582_TARGET	pTarget = NULL;	/* USB_TCD_ISP1582_TARGET */
    UINT8	endpointIndex = 0;		/* endpoint index */
    UINT8	transferType = 0;		/* transfer type */
#ifdef DMA_SUPPORTED
    UINT16	data16 = 0;
#endif
    UINT32	sizeToCopy = 0;			/* size of data to copy */
    STATUS	status = ERROR;
    unsigned char * pBuf = pTrb->pBuffer;	/* pointer to buffer */
    UINT16	data = 0 ;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncCopyDataToEpBuf entered...", USB_TCD_ISP582_WV_FILTER);   


    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : Entered...\n",
    0,0,0,0,0,0);

    /* Validate parameters */

    if ((pHeader == NULL) || (pHeader->trbLength < sizeof (TRB_HEADER)) ||
        (pHeader->handle == NULL) || (pTrb->pipeHandle == 0))
        {

        /* WindView Instrumentation */ 

        USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
        "usbTcdIsp1582FncCopyDataToEpBuf exiting: Bad Parameter Received...",
        USB_TCD_ISP582_WV_FILTER);   


        USBISP1582_ERROR ("usbTcdIsp1582FncCopyDataToEpBuf : Bad Parameters \
        ...\n",0,0,0,0,0,0);
        return ossStatus(S_usbTcdLib_BAD_PARAM);
        }

    pEndpointInfo = (pUSB_TCD_ISP1582_ENDPOINT)pTrb->pipeHandle;
    pTarget = (pUSB_TCD_ISP1582_TARGET)pHeader->handle;

    endpointIndex = pEndpointInfo->endpointIndex;
    transferType = pEndpointInfo->transferType;
    
#ifdef DMA_SUPPORTED

    /*
     * Check if the endpoint supports DMA, i.e. the transfer types are
     * bulk or isochronous.
     */

    if ((transferType == USB_ATTR_ISOCH || transferType == USB_ATTR_BULK))
        {

        /*
         * Check whether DMA Channel is in use and whether DMA transfer has
         * completed succesfully.
         */

        if ((pTarget->dmaEndpointId == endpointIndex) &&
            (pTarget->dmaInUse) && (pTarget->dmaEot))
            {

            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : DMA EOT has \
            happened...\n",0,0,0,0,0,0);

            data16 = isp1582Read16 (pTarget,ISP1582_DMA_INT_RESN_REG) &
                     isp1582Read16 (pTarget , ISP1582_DMA_INT_ENBL_REG);

            if (( data16 & ISP1582_DMA_INT_RESN_REG_DMA_XFER_OK) !=  0)
                {

                /* DMA Transfer is completed succesfully */

                /* Determine the size to copy */

                if (pTrb->uActLength < sysFdBufSize)
                    pTarget->dmaXferLen = pTrb->uActLength;
                else
                    pTarget->dmaXferLen = sysFdBufSize;

                /* Copy data from pBuffer to the DMA Buffer */

                memcpy ((char *)sysFdBuf , pTrb->pBuffer , pTarget->dmaXferLen );

                /* Flush the DMA Buffer */

                USB_PCI_MEM_FLUSH ((pVOID)sysFdBuf ,pTarget->dmaXferLen );

                /* Update uActLength */

                pTrb->uActLength = pTarget->dmaXferLen;

                /* Clear the Dma Interrupt Register */

                isp1582Write16 ( pTarget , ISP1582_DMA_INT_RESN_REG ,
                                 ISP1582_DMA_INT_RESN_MASK);

                /* Initialize the DMA Controller for another operation */

                /*
                 * Initailize the Dma Transfer Counter Register with the
                 * length of data
                 */

                isp1582Write32 (pTarget , ISP1582_DMA_TRANS_CNT_REG ,
                                pTrb->uActLength);

                /* Intialize the DMA Command Regsiter */

                isp1582Write8 (pTarget , ISP1582_DMA_COMMAND_REG ,
                               ISP1582_DMA_COMMAND_REG_GDMA_READ);

                status = OK;
                }

            else
                {

                /* DMA Transfer didn't complete normally */

                /* Clear the Dma Interrupt Register */

                isp1582Write16 ( pTarget , ISP1582_DMA_INT_RESN_REG ,
                                 ISP1582_DMA_INT_RESN_MASK);

                USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : \
                Disabling the DMA...\n",0,0,0,0,0,0);

                /* Disable DMA */

                disableDma (pTarget , endpointIndex);
                status = ERROR;
                }
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : Exiting...\n",
            0,0,0,0,0,0);
            return status;
            }

        else if ((pTarget->dmaEndpointId == endpointIndex) &&
                 (pTarget->dmaInUse) && (pTarget->dmaEot != TRUE))
            return ERROR;

        else if (pTarget->dmaInUse == FALSE)
            {

            /*
             * If the dma channel is unused. initialize the DMA Controller
             * to carry out the DMA operation.
             */

            /* Determine the size to copy */

            if (pTrb->uActLength < sysFdBufSize)
                pTarget->dmaXferLen = pTrb->uActLength;
            else
                pTarget->dmaXferLen = sysFdBufSize;

            /* Copy data from pBuffer to the DMA Buffer */

            memcpy ((char *)sysFdBuf , pTrb->pBuffer , pTarget->dmaXferLen );

            /* Flush the DMA Buffer */

            USB_PCI_MEM_FLUSH ((pVOID)sysFdBuf ,pTarget->dmaXferLen );

            pTarget->dmaEot = FALSE;

            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf :Initializing \
            the DMA for the next operation...\n",0,0,0,0,0,0);

            /* Initialize the DMA */

            initializeDma (pTarget , endpointIndex ,
                           ISP1582_DMA_COMMAND_REG_GDMA_READ);

            /* Update uActLength */

            pTrb->uActLength = pTarget->dmaXferLen;
            USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : Exiting...\n",
            0,0,0,0,0,0);
            return OK;
            }
        }

    /* Endpoint does not support DMA or the dma channel is already in use */

#endif

    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : Working in PIO Mode \
    ...\n",0,0,0,0,0,0);

    /* Initialize the endpoint index register */

    isp1582Write8 (pTarget , ISP1582_ENDPT_INDEX_REG , endpointIndex);

    if ( pTrb->uActLength < pEndpointInfo->maxPacketSize)
        sizeToCopy = pTrb->uActLength;
    else
        sizeToCopy = pEndpointInfo->maxPacketSize;

    /*
     * Write to the buffer length register, the size of data which is to be 
     * sent to the host
     */              

    if (sizeToCopy < pEndpointInfo->maxPacketSize)
        isp1582Write16 (pTarget , ISP1582_BUF_LEN_REG ,sizeToCopy);
    else 
        isp1582Write16 (pTarget , ISP1582_BUF_LEN_REG ,pEndpointInfo->maxPacketSize);

    /*
     * If this function is called for the endpoint 0 IN endpoint,
     * initiate a status stage after copying the data
     */
    if ((endpointIndex == ISP1582_ENDPT_0_TX) && (pTrb->uActLength == sizeToCopy)
        && (pTrb->zeroLengthPacket == FALSE))
        pTarget->controlOUTStatusPending = TRUE;

    /* Update uActLength */

    pTrb->uActLength = sizeToCopy;

    /*
     * Write Data Port Regsiter. It is a 16 bit access, hence sizeToCopy
     * should be decremented by 2 and pBuf should be incremented by 2
     */

    while (sizeToCopy > 0)
        {
        data = *pBuf;
        sizeToCopy--;
        if (sizeToCopy > 0)
            {
            pBuf += 1;
            sizeToCopy--;
            data |= (*pBuf << 8);
            if (sizeToCopy > 0)
                pBuf += 1;
            }
        isp1582Write16( pTarget , ISP1582_DATA_PORT_REG , data);
        }

    status = OK;

    /* WindView Instrumentation */ 

    USB_TCD_LOG_EVENT(USB_TCD_ISP1582_ENDPOINT,
    "usbTcdIsp1582FncCopyDataToEpBuf exited...", USB_TCD_ISP582_WV_FILTER);   

    USBISP1582_DEBUG ("usbTcdIsp1582FncCopyDataToEpBuf : Exiting...\n",
    0,0,0,0,0,0);
    return status;
    }

#ifdef DMA_SUPPORTED

/*******************************************************************************
*
* disableDma - Disable the DMA
*
* This is a utility function to disable the DMA.
*
* RETURNS: N/A.
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL VOID disableDma
    (
    pUSB_TCD_ISP1582_TARGET	pTarget,   	/* USB_TCD_ISP1582_TARGET */
    UINT8			endpointIndex   /* endpoint Index */
    )
    {
    UINT32 data32 = 0;

    /* Reset the DMA Interrupt Enable Regsiter */

    isp1582Write16 (pTarget , ISP1582_DMA_INT_ENBL_REG , 0);

    /* Enable the endpoint by writing into Interrupt Enable register.*/

    data32 = isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG);

    /*
     * Enable that endpoint by writing into endpoint enable
     * register. Also reset the but IE_DMA
     */

    data32 &= ~ISP1582_INT_ENABLE_REG_IEDMA;

    data32 |=  ISP1582_INT_ENABLE_REG_ENDPT_SET(endpointIndex);
    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , data32);

    /* Initialize the DMA Endpoint Register to endpoint not used. */

     isp1582Write8 (pTarget , ISP1582_DMA_ENDPT_REG ,pTarget->dmaEndptNotInUse);

    /*
     * Set dmaInUse and dmaEot member of structure USB_TCD_ISP1582_TARGET
     * to False.
     */

    pTarget->dmaInUse = FALSE;
    pTarget->dmaEot = FALSE;

    /* Reset the dmaEndpointId and dmaXferLen */

    pTarget->dmaEndpointId = 0;
    pTarget->dmaXferLen = 0;
    return;
    }

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

LOCAL VOID initializeDma
    (
    pUSB_TCD_ISP1582_TARGET	pTarget,	/* USB_TCD_ISP1582_TARGET */
    UINT8			endpointIndex,	/* endpoint index */
    UINT8			command		/* DMA command */
    )
    {
    UINT32	data32 = 0;
    UINT8	data8 = 0;

    /* Give Reset DMA Command */

    isp1582Write8 (pTarget , ISP1582_DMA_COMMAND_REG ,
                   ISP1582_DMA_COMMAND_REG_DMA_RESET);

    /* Initialize the DMA Configuration Register */

    isp1582Write16 (pTarget , ISP1582_DMA_CONFIG_REG ,
                  ISP1582_DMA_CONFIG_REG_MODE_SET(ISP1582_DMA_CONFIG_REG_MODE_00)
                  | ISP1582_DMA_CONFIG_REG_WIDTH_8);

    /* Initialize the DMA Hardware Register */

    data8 = ISP1582_DMA_HARDWARE_REG_DREQ_POL|ISP1582_DMA_HARDWARE_REG_EOT_POL;
    isp1582Write8 (pTarget, ISP1582_DMA_HARDWARE_REG , data8 );

    /*
     * In default Burstmode is set to 0. The Burstmode are set to
     * define the number of DIOR/DIOW strobes to be detected before
     * de-asserting DREQ. As an example, Burstmode '000' will mean that
     * the DREQ will stay asserted until the end of transfer OR maximum
     * packet size is reached.
     */

    /* Set the DMA Burst Counter Regsiter to default value */

    isp1582Write16 ( pTarget , ISP1582_DMA_BURST_COUNT_REG , 0);

    /* Initailize the Dma Transfer Counter Register with the length of data */

    isp1582Write32 (pTarget , ISP1582_DMA_TRANS_CNT_REG , pTarget->dmaXferLen);

    /*
     * Initialize Dma Interrupt Enable Register to generate interrupt only
     * when the DMA transfer counter reaches 0 i.e set bit IE_DMA_XFER_OK.
     */

    isp1582Write16 (pTarget , ISP1582_DMA_INT_ENBL_REG ,
                                       ISP1582_DMA_INT_ENBL_REG_IE_DMA_XFER_OK);

    /*
     * Disable the interrupt for endpoint in Interrupt Enalbe Register
     * and enable the bit IE_DMA.
     */

    data32 = isp1582Read32 (pTarget , ISP_1582_INT_ENABLE_REG);

    /* Disable the interrupt for endpoint */

    data32 &= ~(ISP1582_INT_ENABLE_REG_ENDPT_SET(endpointIndex));

    /* Enable the bit for the DMA interupt */

    data32 |= ISP1582_INT_ENABLE_REG_IEDMA;

    /* Write into interrupt enable regsiter */

    isp1582Write32 (pTarget , ISP_1582_INT_ENABLE_REG , data32);

    /* Initialize the DMA endpoint register */

    isp1582Write8 (pTarget , ISP1582_DMA_ENDPT_REG , endpointIndex);

    /* Set member dmaInUse and reset member dmaEot */

    pTarget->dmaInUse = TRUE;
    pTarget->dmaEot = FALSE;

    /* Update dmaEndpointId to endpointIndex */

    pTarget->dmaEndpointId = endpointIndex;

    /* Give the DMA Command by writing into the DMA Regsiter */

    isp1582Write8 (pTarget , ISP1582_DMA_COMMAND_REG , command);

    return;
    }
#endif


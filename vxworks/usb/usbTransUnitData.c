/* usbTransUnitData.c - Translation Unit Data Transfer Interfaces */

/* Copyright 2003 Wind River Systems, Inc. */

/*
Modification history
--------------------
01c,15oct04,ami  Apigen Changes
01o,06oct04,ami  Removal of warning messages for SPR #94684 Fix
01n,15sep04,hch  Fix diab compiler warning
01m,24aug04,hch  Removed big static arrays in usbdTransfer function
01l,03aug04,ami  Warning Messages Removed
01k,05jul04,???  Short packet flag enabled for vendor specific data transfer
01j,12apr04,cfc  Apigen fixes
01i,17nov03,???  correcting refgen errors.
01h,15oct03,cfc  Merge from Bangalore, check for NULL ptr
01g,30sep03,cfc  Fix for dual class driver devices from ODC
01f,17sep03,cfc  Remove direct calls to wvEvent
01e,08sep03,cfc  URB callbacks with msgQSend
01d,16Jul03,gpd added the status updation and actual transfer length updation
                of IRP in the callback function.
01c,16jun03,mathew  prefixed "usb" to file name, and callback routine names.
01b,06jun03,mathew  Wind View Instrumentation.
01a,06jun03,mathew  Wind River Coding Convention  API changes.
*/

/*
DESCRIPTION

Implements the Translation Unit Data Transfer Interfaces.

INCLUDE FILES: 
usbTransUnit.h, usbHcdInstr.h
*/


/* includes */

#include "drv/usb/usbTransUnit.h"
#include "usb2/usbHcdInstr.h"

/* forward declarations */

USBHST_STATUS usbtuDataUrbCompleteCallback ( pUSBHST_URB urbPtr );
USBHST_STATUS usbtuDataVendorSpecificCallback (pUSBHST_URB urbPtr );

/* mutex of Translation Unit */

extern MUTEX_HANDLE  usbtuMutex;

/***************************************************************************
*
* usbdPipeCreate - Creates a USB pipe for subsequent transfers
*
* This function establishes a pipe which can subsequently be used by a
* client to exchange data with a USB device endpoint.
*
* <nodeId> and <endpoint> identify the device and device endpoint,
* respectively, to which the pipe should be "connected."  <configuration>
* and <interface> specify the configuration and interface, respectively,
* with which the pipe is associated.
*
* <transferType> specifies the type of data transfers for which this pipe
* will be used:
*
* \is
* \i 'USB_XFRTYPE_CONTROL'
* Control transfer pipe (message)
*
* \i 'USB_XFRTYPE_ISOCH'
* Isochronous transfer pipe (stream)
*
* \i 'USB_XFRTYPE_INTERRUPT'
* Interrupt transfer pipe (stream)
*
* \i 'USB_XFRTYPE_BULK'
* Bulk transfer pipe (stream)
* \ie
*
* <direction> specifies the direction of the pipe as:
*
* \is
* \i 'USB_DIR_IN'
* Data moves from device to host
*
* \i 'USB_DIR_OUT'
* Data moves from host to device
*
* \i 'USB_DIR_INOUT'
* Data moves bidirectionally (message pipes only)
* \ie
*
* If the <direction> is specified as USB_DIR_INOUT, the USBD assumes that
* both the IN and OUT endpoints identified by endpoint will be used by
* this pipe (see the discussion of message pipes in Chapter 5 of the USB
* Specification).  USB_DIR_INOUT may be specified only for Control pipes.
*
* <maxPayload> specifies the largest data payload supported by this endpoint.
* Normally a USB device will declare the maximum payload size it supports on
* each endpoint in its configuration descriptors.  The client will typically
* read these descriptors using the USBD Configuration Functions and then
* parse the descriptors to retrieve the appropriate maximum payload value.
*
* <bandwidth> specifies the bandwidth required for this pipe.  For control
* and bulk pipes, this parameter should be 0.  For interrupt pipes, this
* parameter should express the number of bytes per frame to be transferred.
* for isochronous pipes, this parameter should express the number of bytes
* per second to be transferred.
*
* <serviceInterval> specifies the maximum latency for the pipe in
* milliseconds.  So, if a pipe needs to be serviced, for example, at least
* every 20 milliseconds, then the <serviceInterval> value should be 20.  The
* <serviceInterval> parameter is required only for interrupt pipes.  For
* other types of pipes, <serviceInterval> should be 0.
*
* If the USBD succeeds in creating the pipe it returns a pipe handle in
* <pPipeHandle>.  The client must use the pipe handle to identify the pipe
* in subsequent calls to the USBD Transfer Functions.  If there is
* insufficient bus bandwidth available to create the pipe (as might happen
* for an isochronous or interrupt pipe), then the USBD will return an error
* and a NULL handle in <pPipeHandle>.
*
* RETURNS: OK, or ERROR if pipe could not be create
*
* ERRNO: N/A
*/

STATUS usbdPipeCreate
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 endpoint,			/* Endpoint address */
    UINT16 configuration,		/* config w/which pipe associated */
    UINT16 interface,			/* interface w/which pipe associated */
    UINT16 transferType,		/* Type of transfer: control, bulk... */
    UINT16 direction,			/* Specifies IN or OUT endpoint */
    UINT16 maxPayload,			/* Maximum data payload per packet */
    UINT32 bandwidth,			/* Bandwidth required for pipe */
    UINT16 serviceInterval,		/* Required service interval */
    pUSBD_PIPE_HANDLE pPipeHandle	/* pipe handle returned by USBD */
    )

    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUSB_INTERFACE_DESCR pIfDescr;
    pUSB_ENDPOINT_DESCR pEpDescr;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    UINT16 actLen;
    UINT16 ifNo;
    pUSBTU_PIPE_INFO pipeInfo;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)pDriver->clientName,USBD_NAME_LEN ); 
        strcat(evLog, " : Pipe Create "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_TRANSFER, evLog, USBTU_WV_FILTER);
        }  

    


    USBTU_LOG ( "usbdPipeCreate entered \n ");

    if ((pBfr = OSS_CALLOC ( USB_MAX_DESCR_LEN)) == NULL)
        {
        USBTU_LOG ( "usbdPipeCreate returns ERROR : malloc failed \n ");
        return ERROR;
        }

    /* Read the configuration descriptor */
    
    if (usbdDescriptorGet (clientHandle,
  			    nodeId,
		            USB_RT_DEVICE,
                            USB_DESCR_CONFIGURATION,
			    0,
			    0,
			    USB_MAX_DESCR_LEN,
			    pBfr,
			    &actLen)
			    != OK)
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR: usbdDescriptorGet failed \n");
        return ERROR;
        }

    if ((pCfgDescr = usbDescrParse (pBfr,
                                     actLen,
				     USB_DESCR_CONFIGURATION))
				     == NULL)
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR : usbDescrParse failed \n ");
        return ERROR;
        }



    /* Look for the interface */

    ifNo = 0;

    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */

    pScratchBfr = pBfr;

    while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,
                                           &actLen,
					   USB_DESCR_INTERFACE))
					   != NULL)
        {
        if (ifNo == interface)
	    break;
	    ifNo++;
        }

    if (pIfDescr == NULL)
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR : usbDescrParseSkip failed\n");
        return ERROR;
        }

    /* get the endpoint descriptor */

    

    while ((pEpDescr = usbDescrParseSkip (&pScratchBfr,
                                           &actLen,
					   USB_DESCR_ENDPOINT))
					   != NULL)
        {
        if (pEpDescr->endpointAddress == endpoint)
            break;
	
        }

    if (pEpDescr == NULL)
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR : usbDescrParseSkip1 failed\n");
        return ERROR;
        }

    /* Check to make sure the max packet size is in the byte order endian */
    pEpDescr->maxPacketSize = OS_UINT16_LE_TO_CPU(pEpDescr->maxPacketSize);


    /* check the maximum payload */ 
    if (pEpDescr->maxPacketSize > maxPayload)
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR : bandwidth not available\n ");
        return ERROR;
        }

    /* allocate a structure for the pipe  */

    if ( !(pipeInfo = OSS_CALLOC( sizeof (USBTU_PIPE_INFO))))
        {
        OSS_FREE (pBfr);
        USBTU_LOG ( "usbdPipeCreate returns ERROR: malloc1 failed \n ");
        return ERROR;
        }

    /* initialize the pipe structure */

    pipeInfo->hDevice = (UINT32)nodeId;
    pipeInfo->endpointAddress = pEpDescr->endpointAddress;
    pipeInfo->transferType = transferType;
    pipeInfo->irpList = NULL;
    pipeInfo->markedForDeletion = 0;
    pipeInfo->uMaxPacketSize = maxPayload;
    pipeInfo->bandwidth = bandwidth;


    /* Return result */

    if (pPipeHandle != NULL)
        *pPipeHandle = pipeInfo;


    OSS_FREE (pBfr);
    USBTU_LOG ( "usbdPipeCreate returns OK \n ");
    return OK;

    }

/***************************************************************************
*
* usbdPipeDestroy - Destroys a USB data transfer pipe
*
* This function destroys a pipe previous created by calling usbdPipeCreate().
* The caller must pass the <pipeHandle> originally returned by usbdPipeCreate().
*
*
* RETURNS: OK, or ERROR if unable to destroy pipe.
*
* ERRNO: N/A
*/

STATUS usbdPipeDestroy
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_PIPE_HANDLE pipeHandle 	/* pipe handle */
    )

    {
    pUSBTU_PIPE_INFO pipeInfo = (pUSBTU_PIPE_INFO) pipeHandle;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    STATUS status;

    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog,(char *)pDriver->clientName,USBD_NAME_LEN );
        strcat(evLog, " : Pipe Destroy ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_TRANSFER, evLog, USBTU_WV_FILTER);
        }


    USBTU_LOG ( "usbdPipeDestroy entered \n ");


    /* release the pipe structure */

    if (pipeHandle == NULL)
        {
        USBTU_LOG ( "usbdPipeDestroy returns ERROR \n ");
        return ERROR;
        }

    /* Access the mutex */ 
    OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

    /* Indicate that pipe is marked for deletion
     * This is to ensure that IRP callback does not resubmit IRP while pipe
     * is being deleted
     */

    pipeInfo->markedForDeletion = 1;


    /* If there are requests queued for the pipe, create the semaphore
     * which will be signalled by the callback
     * if the cancellation is not successful
     */

    if (pipeInfo->irpList)
        {
        pipeInfo->PipeDelete = semBCreate (SEM_Q_FIFO,SEM_EMPTY);

        /* Check if sem creation is successful */
        if (pipeInfo->PipeDelete == NULL)
            {
            USBTU_LOG("usbdPipeDestroy-Pipe deletion sem not created \n ");
            OSS_MUTEX_RELEASE (usbtuMutex);
            return ERROR;
            }
        }

    /* Search through the list of IRPs and cancel all of them */
    while(pipeInfo->irpList != NULL)
        {
        /* Call the function to cancel the IRP */ 
        status = usbdTransferAbort( clientHandle, pipeHandle ,pipeInfo->irpList->pIrp);

        /* The status can be an error
         * 1. if the parameters are not valid
         *    or
         * 2. if the transfer is already completed.
         * We ignore the first case, as it is ensured that the
         * parameters are valid. 
         * This check will be pass only if the transfer is already completed.
         */
        if (status == ERROR)
            {
            /* Release the mutex. This is done as the mutex will be
             * taken by the completion routine also
             */
            OSS_MUTEX_RELEASE (usbtuMutex);

            /* Wait for the pipe deletion semaphore to be signalled from the
             * completion routine
             */  
            semTake(pipeInfo->PipeDelete ,WAIT_FOREVER);

            /* Release the mutex */  
            OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);
            }

        }

    /* Delete the semaphore created */

    if(pipeInfo->PipeDelete)
        semDelete(pipeInfo->PipeDelete);

    /* Release the acquired mutex */

    OSS_MUTEX_RELEASE (usbtuMutex);

    OSS_FREE( pipeHandle);
    USBTU_LOG ( "usbdPipeDestroy returns OK \n ");
    return OK;
    }


/***************************************************************************
*
* usbdTransfer - Initiates a transfer on a USB pipe
*
* A client uses this function to initiate an transfer on the pipe indicated
* by <pipeHandle>.  The transfer is described by an IRP, or I/O request
* packet, which must be allocated and initialized by the caller prior to
* invoking usbdTransfer().
*
* The USB_IRP structure is defined in usb.h as:
*
* \cs
* typedef struct usb_bfr_list
*     {
*     UINT16 pid;
*     pUINT8 pBfr;
*     UINT16 bfrLen;
*     UINT16 actLen;
*     } USB_BFR_LIST;
*
* typedef struct usb_irp
*     {
*     LINK usbdLink;		    // used by USBD
*     pVOID usbdPtr;		    // used by USBD
*     LINK hcdLink;		    // used by HCD
*     pVOID hcdPtr;		    // used by HCD
*     pVOID userPtr;
*     UINT16 irpLen;
*     int result;		    // returned by USBD/HCD
*     IRP_CALLBACK usbdCallback;    // used by USBD
*     IRP_CALLBACK userCallback;
*     UINT16 dataToggle;	    // filled in by USBD
*     UINT16 flags;
*     UINT32 timeout;		    // defaults to 5 seconds if zero
*     UINT16 startFrame;
*     UINT16 transferLen;
*     UINT16 dataBlockSize;
*     UINT16 bfrCount;
*     USB_BFR_LIST bfrList [1];
*     } USB_IRP, *pUSB_IRP;
* \ce
*
* The length of the USB_IRP structure must be stored in <irpLen> and varies
* depending on the number of <bfrList> elements allocated at the end of the
* structure.  By default, the default structure contains a single <bfrList>
* element, but clients may allocate a longer structure to accommodate a larger
* number of <bfrList> elements.
*
* <flags> define additional transfer options.  The currently defined flags are:
*
* \is
* \i 'USB_FLAG_SHORT_OK'
* Treats receive (IN) data underrun as OK
*
* \i 'USB_FLAG_SHORT_FAIL'
* Treats receive (IN) data underrun as error
*
* \i 'USB_FLAG_ISO_ASAP'
* Start an isochronous transfer immediately
* \ie
*
* When the USB is transferring data from a device to the host the data may
* "underrun".  That is, the device may transmit less data than anticipated by
* the host.  This may or may not indicate an error condition depending on the
* design of the device.  For many devices, the underrun is completely normal
* and indicates the end of data stream from the device.  For other devices,
* the underrun indicates a transfer failure.  By default, the USBD and
* underlying USB HCD (Host Controller Driver) treat underrun as the end-of-data
* indicator and do not declare an error.  If the USB_FLAG_SHORT_FAIL flag is
* set, then the USBD/HCD will instead treat underrun as an error condition.
*
* For isochronous transfers the USB_FLAG_ISO_ASAP specifies that the
* isochronous transfer should begin as soon as possible.  If USB_FLAG_ISO_ASAP
* is not specified, then <startFrame> must specify the starting frame number
* for the transfer.  The usbdCurrentFrameGet() function allows a client to
* retrieve the current frame number and a value called the frame scheduling
* window for the underlying USB host controller.  The frame window specifies
* the maximum number of frames into the future (relative to the current frame
* number) which may be specified by <startFrame>.  <startFrame> should be
* specified only for isochronous transfers.
*
* <dataBlockSize> may also be specified for isochronous transfers.  If non-0,
* the <dataBlockSize> defines the granularity of isochronous data being sent.
* When the underlying Host Controller Driver (HCD) breaks up the transfer into
* individual frames, it will ensure that the amount of data transferred in
* each frame is a multiple of this value.
*
* <timeout> specifies the IRP timeout in milliseconds.	If the caller passes
* a value of zero, then the USBD sets a default timeout of USB_TIMEOUT_DEFAULT.
* If no timeout is desired, then <timeout> should be set to USB_TIMEOUT_NONE.
* Timeouts apply only to control and bulk transfers.  Isochronous and
* interrupt transfers do not time out.
*
* <bfrList> is an array of buffer descriptors which describe data buffers to
* be associated with this IRP.	If more than the one <bfrList> element is
* required then the caller must allocate the IRP by calculating the size as
*
* \cs
* irpLen = sizeof (USB_IRP) + (sizeof (USB_BFR_DESCR) * (bfrCount - 1))
* \ce
*
* <transferLen> must be the total length of data to be transferred.  In other
* words, transferLen is the sum of all <bfrLen> entries in the <bfrList>.
*
* <pid> specifies the packet type to use for the indicated buffer and is
* specified as USB_PID_xxxx.
*
* The IRP <userCallback> routine must point to a client-supplied IRP_CALLBACK
* routine.  The usbdTransfer() function returns as soon as the IRP has been
* successfully enqueued.  If there is a failure in delivering the IRP to the
* HCD, then usbdTransfer() returns an error.  The actual result of the IRP
* should be checked after the <userCallback> routine has been invoked.
*
* RETURNS: OK, or ERROR if unable to submit IRP for transfer.
*
* ERRNO: N/A
*/

STATUS usbdTransfer
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_PIPE_HANDLE pipeHandle,	/* Pipe handle */
    pUSB_IRP pIrp			/* ptr to I/O request packet */
    )

    {
    pUSBTU_IRPCONTEXT pIrpContext;
    pUSBTU_PIPE_INFO pipeInfo = (pUSBTU_PIPE_INFO) pipeHandle;
    pUSBTU_DEVICE_DRIVER clientInfo = (pUSBTU_DEVICE_DRIVER) clientHandle;
    pUSBHST_ISO_PACKET_DESC pIsoPacketDesc;
    pUSBHST_URB pUrb;
    int no;
    int i;
    int bufferlength;
    int bufferIndex;
    int transferflag = 0;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    pUSBTU_IRP tempIrp = NULL;
    UINT32 frameCount = 0;
    UINT32 bytesThroughFrameCount = 0;
    UINT32 maxLen= 0;
    UINT32 bytesSoFar = 0;

    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)pDriver->clientName,USBD_NAME_LEN ); 
        strcat(evLog, " : USBD Transfer "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_TRANSFER, evLog, USBTU_WV_FILTER);
        }  



    USBTU_LOG ( "usbdTransfer entered \n ");

    /* control transfer type not handled by usbdTransfer */

    if (pipeInfo->transferType == USB_XFRTYPE_CONTROL)
        {
        USBTU_LOG ( "usbdTransfer returns ERROR : control transfer  \n ");
        return ERROR;
        }

    /* If pipe is marked for deletion return ERROR */

    if(pipeInfo->markedForDeletion)
        {
        USBTU_LOG ( "usbdTransfer returns ERROR : pipe marked for deletion  \n ");
        return ERROR;
        }


    /* allocate structure for IRP context */

    if ( !(pIrpContext = OSS_CALLOC ( sizeof (USBTU_IRPCONTEXT))))
        {
        USBTU_LOG ( "usbdTransfer returns ERROR : malloc  \n ");
        return ERROR;
        }

    /* allocate an array of URB structures one for each buffer */



    if (!(pIrpContext->urbPtr =
            OSS_CALLOC ( pIrp->bfrCount * sizeof(USBHST_URB))))
        {
         OSS_FREE (pIrpContext);
         USBTU_LOG ( "usbdTransfer returns ERROR : malloc1 failed  \n ");
         return ERROR;
        }

    /* set the array index */

    pIrpContext->urbIndex = 0;

    pIrpContext->pipeInfo = pipeInfo;


    pIrpContext->urbCompleteCount = 0;


    /* store the IRP context structure in IRP  */

    pIrp->usbdPtr = pIrpContext;

    /* set the initial status of IRP to OK */

    pIrp->result = OK;

    /* get the message queue id of client */

    pIrpContext->msgQid = clientInfo->msgQidIrpComplete;

    /* determine the transfer flag */

    if (pIrp->flags == USB_FLAG_SHORT_OK)
        transferflag = USBHST_SHORT_TRANSFER_OK;
    else
        {
        if ( pIrp->flags & USB_FLAG_ISO_ASAP)
            transferflag = USBHST_START_ISOCHRONOUS_TRANSFER_ASAP;
        }

    /* Allocate memory for irp link list element */

    if (!(tempIrp = OSS_CALLOC ( sizeof (USBTU_IRP))))
        {
         OSS_FREE(pIrpContext->urbPtr);
         OSS_FREE (pIrpContext);
         USBTU_LOG ( "usbdTransfer returns ERROR : malloc1 failed  \n ");
         return ERROR;
        }

    tempIrp->pIrp = pIrp;
    tempIrp->nextIrp = NULL;
    tempIrp->prevIrp = NULL;

    /* Take the mutex before accessing the list */
    OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

    /* Add the request to the list of requests for 
     * the pipe
     */  
    if(pipeInfo->irpList == NULL)
       	pipeInfo->irpList = tempIrp;
    else
        {
           tempIrp->nextIrp = pipeInfo->irpList;
           pipeInfo->irpList->prevIrp = tempIrp;
           pipeInfo->irpList = tempIrp;
        }

    /* Release the mutex */
    OSS_MUTEX_RELEASE (usbtuMutex);

    switch ( pipeInfo->transferType)
        {
        case USB_XFRTYPE_ISOCH:

            /* isochronous transfer type */

            for ( bufferIndex= 0; bufferIndex < pIrp->bfrCount ; bufferIndex++)
                {

                /* get the length of the buffer */

                bufferlength = (pIrp->bfrList[bufferIndex]).bfrLen;

                /* determine the number of isochronous packet
                 * descriptors required
                 */
                if (pipeInfo->bandwidth == 0)
                    { 
                if ( bufferlength % pipeInfo->uMaxPacketSize)
                    no = ( bufferlength / pipeInfo->uMaxPacketSize ) + 1;
                else
                    no = ( bufferlength / pipeInfo->uMaxPacketSize);

                pIsoPacketDesc = NULL;

                /* allocate ischronous packet descriptors */

                if (!(pIsoPacketDesc =
                        OSS_CALLOC( no * sizeof (USBHST_ISO_PACKET_DESC))))
                    {

                    /* cancel already submitted URB's */

                    usbdTransferAbort(clientHandle, pipeHandle, pIrp);
                    USBTU_LOG ( "usbdTransfer returns ERROR : malloc2  \n ");
                    return ERROR;

                    }

                /* initialize the isochronous packet descriptors  */

                for ( i= 0 ; i< no ; i++)
                   {

                   /* update the length */

                   if ( i == (no - 1) )
                       {
                       pIsoPacketDesc[i].uLength =
                                (bufferlength % pipeInfo->uMaxPacketSize);
                       }
                   else
                       pIsoPacketDesc[i].uLength = pipeInfo->uMaxPacketSize;

                   /* update the offset */

                   pIsoPacketDesc[i].uOffset =pipeInfo->uMaxPacketSize * i;

                   }
                    }
                else
                    {
                    /* Determine the number of packets based on pipeInfo->bandwidth */

                    bytesSoFar = 0;
                    frameCount = 0;
                    while ((int) bytesSoFar < bufferlength)
                        {
               
                        frameCount++;

          	        bytesThroughFrameCount = (frameCount * pipeInfo->bandwidth) / 1000L;

                        maxLen = min (bytesThroughFrameCount - bytesSoFar,
	                          bufferlength - bytesSoFar);


                        maxLen = min (maxLen, pipeInfo->uMaxPacketSize);


                        if (pIrp->dataBlockSize != 0 && maxLen > pIrp->dataBlockSize)
		            maxLen = (maxLen / pIrp->dataBlockSize) * pIrp->dataBlockSize;

                        bytesSoFar += maxLen ;

                        }

                    no = frameCount;
                    pIsoPacketDesc = NULL;

                    /* allocate ischronous packet descriptors */

                    if (!(pIsoPacketDesc =
                        OSS_CALLOC( no * sizeof (USBHST_ISO_PACKET_DESC))))
                        {

                        /* cancel already submitted URB's */

                        usbdTransferAbort(clientHandle, pipeHandle, pIrp);
                        USBTU_LOG ( "usbdTransfer returns ERROR : malloc2  \n ");
                        return ERROR;
 
                        }

                    /* initialize the isochronous packet descriptors  */

                    bytesSoFar = 0;
                    frameCount = 0;
                    while ((int) bytesSoFar < bufferlength)
                       {

                        frameCount++;

          	        bytesThroughFrameCount = (frameCount * pipeInfo->bandwidth) / 1000L;

                        maxLen = min (bytesThroughFrameCount - bytesSoFar,
	                          bufferlength - bytesSoFar);


                        maxLen = min (maxLen, pipeInfo->uMaxPacketSize);


                        if (pIrp->dataBlockSize != 0 && maxLen > pIrp->dataBlockSize)
		            maxLen = (maxLen / pIrp->dataBlockSize) * pIrp->dataBlockSize;

                       /* update the offset */
                       pIsoPacketDesc[frameCount - 1].uOffset =bytesSoFar;
                       /* update the length */
                       pIsoPacketDesc[frameCount - 1].uLength = maxLen;


                       bytesSoFar += maxLen ;

                       }
                  
                    }

                /* get the URB structure to be filled from array of URB structures */

                pUrb = &(pIrpContext->urbPtr[pIrpContext->urbIndex]);

                /* initialize the URB structure */

#if 1

                USBHST_FILL_ISOCHRONOUS_URB(pUrb,
                                            pipeInfo->hDevice,
                                            pipeInfo->endpointAddress,
                                            (pIrp->bfrList[bufferIndex]).pBfr,
                                            (pIrp->bfrList[bufferIndex]).bfrLen,
                                            transferflag,
                                            pIrp->startFrame,
                                            no,
                                            pIsoPacketDesc,
                                            usbtuDataUrbCompleteCallback,
                                            (PVOID)pIrp,
                                            USBHST_SUCCESS);
#else
	/* WARNING**************Just a patch */
		transferflag |= USBHST_START_ISOCHRONOUS_TRANSFER_ASAP;

                USBHST_FILL_ISOCHRONOUS_URB(pUrb,
                                            pipeInfo->hDevice,
                                            pipeInfo->endpointAddress,
                                            (pIrp->bfrList[bufferIndex]).pBfr,
                                            (pIrp->bfrList[bufferIndex]).bfrLen,
                                            transferflag,
                                            0,
                                            no,
                                            pIsoPacketDesc,
                                            usbtuDataUrbCompleteCallback,
                                            (PVOID)pIrp,
                                            USBHST_SUCCESS);

#endif

                /* increment the URB array index */

                (pIrpContext->urbIndex)++;

                /* submit the URB */

                if ( usbHstURBSubmit (pUrb) != USBHST_SUCCESS)
                    {
                    /* cancel already submitted URB's */

                    USBTU_LOG("usbdTransfer returns ERROR : SubmitURB failed\n");

                    usbdTransferAbort(clientHandle, pipeHandle, pIrp);

                    return ERROR;
                    }
                }
             break;

        case USB_XFRTYPE_BULK :

            /* Bulk Transfer Type */

            for ( bufferIndex= 0; bufferIndex < pIrp->bfrCount ; bufferIndex++)
                {
                /* get the URB structure to be filled */

                pUrb = &(pIrpContext->urbPtr[ pIrpContext->urbIndex]);

                /* initialize the URB structure */

                USBHST_FILL_BULK_URB(pUrb,
                                     pipeInfo->hDevice,
                                     pipeInfo->endpointAddress,
                                     (pIrp->bfrList[bufferIndex]).pBfr,
                                     (pIrp->bfrList[bufferIndex]).bfrLen,
                                     transferflag,
                                     usbtuDataUrbCompleteCallback,
                                     (PVOID)pIrp,
                                     USBHST_SUCCESS
                                     );
                /* increment the URB array index */

                (pIrpContext->urbIndex)++;

                /* submit the URB */

                if ( usbHstURBSubmit(pUrb) != USBHST_SUCCESS)
                    {

                    /* cancel already submitted URB's */

                    USBTU_LOG("usbdTransfer returns ERROR : SubmitURB failed\n");

                    usbdTransferAbort(clientHandle, pipeHandle, pIrp);

                    return ERROR;
                    }
                }
            break;
        case USB_XFRTYPE_INTERRUPT:

            /* Interrupt Transfer Type */

            for ( bufferIndex= 0; bufferIndex < pIrp->bfrCount ; bufferIndex++)
                {

                /* Get the URB structure to be filled */

                pUrb = &(pIrpContext->urbPtr[ pIrpContext->urbIndex]);

                /* initialize the URB structure */

                USBHST_FILL_INTERRUPT_URB(pUrb,
                                        pipeInfo->hDevice,
                                        pipeInfo->endpointAddress,
                                        (pIrp->bfrList[bufferIndex]).pBfr,
                                        (pIrp->bfrList[bufferIndex]).bfrLen,
                                        transferflag,
                                        usbtuDataUrbCompleteCallback,
                                        (PVOID)pIrp,
                                        USBHST_SUCCESS
                                        );

                /* increment the URB array index */

                (pIrpContext->urbIndex)++;

                /* submit the URB */

                if ( usbHstURBSubmit(pUrb) != OK)
                    {

                    /* cancel already submitted URB's */

                    USBTU_LOG("usbdTransfer returns ERROR : SubmitURB failed\n");

                    usbdTransferAbort(clientHandle, pipeHandle, pIrp);

                    return ERROR;
                    }
                }
            break;
        default: break;
        }

     USBTU_LOG(" usbdTransfer returns OK \n ");
     return OK;
    }


/***************************************************************************
*
* usbdTransferAbort - Aborts a transfer
*
* This function aborts an IRP which was previously submitted through
* a call to usbdTransfer().
*
* RETURNS: OK, or ERROR if unable to abort transfer.
*
* ERRNO: N/A
*/

STATUS usbdTransferAbort
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_PIPE_HANDLE pipeHandle,	    /* Pipe handle */
    pUSB_IRP pIrp			/* ptr to I/O to abort */
    )

    {
    int i;
    pUSBTU_IRPCONTEXT pIrpContext; 
    pUSBHST_URB pUrb;
    STATUS s = OK;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    USBHST_STATUS status;
    pUSBTU_PIPE_INFO pipeInfo = (pUSBTU_PIPE_INFO) pipeHandle;
    pUSBTU_IRP tempIrp;
    pUSBTU_IRP prevIrp;

    if(pIrp == NULL)
       return ERROR;

    /* Check if the pipe handle is valid */
    if (pipeInfo == NULL)
       return ERROR;

    /* remove the IRP from the list maintained - start */

    /* Take the mutex before accessing the list */
    OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

    tempIrp = pipeInfo->irpList;
    prevIrp = pipeInfo->irpList;

    while(tempIrp != NULL )
        {
      	if(tempIrp->pIrp == pIrp )
            break;
        else
           {
           prevIrp =  tempIrp;
           tempIrp = tempIrp->nextIrp;
           }
        }

    if (tempIrp == NULL)
        {
        OSS_MUTEX_RELEASE (usbtuMutex);
	    return ERROR;	          
        }

    if(tempIrp == prevIrp)
       pipeInfo->irpList = pipeInfo->irpList->nextIrp;
    else
       {
       prevIrp->nextIrp = tempIrp->nextIrp;
       if(tempIrp->nextIrp != NULL)
            tempIrp->nextIrp->prevIrp = prevIrp;
       }

    OSS_FREE (tempIrp);

    /* remove the IRP from the list maintained - End */

    /* Release the mutex after accessing the list */
    OSS_MUTEX_RELEASE (usbtuMutex);

    pIrpContext = pIrp->usbdPtr;


    /* Check for NULL Pointer */
    if (pIrpContext == NULL)
    {
       USBTU_LOG ("IRP context is NULL\n");
       return ERROR;
    }


    if(pIrpContext->urbPtr == NULL)
       return ERROR;

    pUrb = pIrpContext->urbPtr;
    

    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)pDriver->clientName,USBD_NAME_LEN ); 
        strcat(evLog, " : USBD Transfer Abort "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_TRANSFER, evLog, USBTU_WV_FILTER);
        }  


    USBTU_LOG ( "usbdTransferAbort entered \n ");

    /* if all the URB's are completed , there is nothing to abort */

    if (pIrpContext->urbCompleteCount == pIrp->bfrCount)
        {
        USBTU_LOG ( "Transfer Completed! \n ");
        return OK;
        }

    /* abort the URB's and release URB specific resources */

    for ( i = 0 ; i < pIrp->bfrCount ; i++ )
        {

        status = usbHstURBCancel( &(pUrb[i]));

        if ((status == USBHST_TRANSFER_COMPLETED) ||
            (status == USBHST_INVALID_PARAMETER) || 
             (status == USBHST_INVALID_REQUEST))
      
            {
            USBTU_LOG ( "usbdTransferAbort CancelURB failed \n ");
            s = ERROR;
            break;
            }

        /* This will be freed by the callback */
        if ( pUrb[i].pTransferSpecificData)
            OSS_FREE (pUrb[i].pTransferSpecificData);
        }

    /* Call callback if the cancelling is successful */
     
     if ( s!= ERROR)
         {
     pIrp->result = S_usbHcdLib_IRP_CANCELED;

    (*pIrp->userCallback)(pIrp);


    /* Release resources */

    OSS_FREE(pUrb);

    OSS_FREE(pIrpContext);
    pIrp->usbdPtr = NULL;

         }

    if ( s == OK)
        {
        USBTU_LOG ( "usbdTransferAbort return OK \n ");
        }
    else
        {
        USBTU_LOG ( "usbdTransferAbort return ERROR \n ");
        }

    return s;

   }


/***************************************************************************
*
* usbdVendorSpecific - Allows clients to issue vendor-specific USB requests
*
* Certain devices may implement vendor-specific USB requests which cannot
* be generated using the standard functions described elsewhere.  This
* function allows a client to specify directly the exact parameters for a
* USB control pipe request.
*
* <requestType>, <request>, <value>, <index>, and <length> correspond
* exactly to the bmRequestType, bRequest, wValue, wIndex, and wLength fields
* defined by the USB Specfication.  If <length> is greater than zero, then
* <pBfr> must be a non-NULL pointer to a data buffer which will provide or
* accept data, depending on the direction of the transfer.
*
* Vendor specific requests issued through this function are always directed
* to the control pipe of the device specified by <nodeId>.  This function
* formats and sends a Setup packet based on the parameters provided.  If a
* non-NULL <pBfr> is also provided, then additional IN or OUT transfers
* will be performed following the Setup packet.  The direction of these
* transfers is inferred from the direction bit in the <requestType> param.
* For IN transfers, the actual length of the data transferred will be
* stored in <pActLen> if <pActLen> is not NULL.
*
* RETURNS: OK, or ERROR if unable to execute vendor-specific request.
*
* ERRNO: N/A
*/

STATUS usbdVendorSpecific
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT8 requestType,			/* bmRequestType in USB spec. */
    UINT8 request,			/* bRequest in USB spec. */
    UINT16 value,			/* wValue in USB spec. */
    UINT16 index,			/* wIndex in USB spec. */
    UINT16 length,			/* wLength in USB spec. */
    pUINT8 pBfr,			/* ptr to data buffer */
    pUINT16 pActLen			/* actual length of IN */
    )

    {
    pUSBHST_URB pUrb;
    pUSBHST_SETUP_PACKET pSetupPacket;
    SEM_ID semMutex;
    STATUS s = OK;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog,(char *)pDriver->clientName,USBD_NAME_LEN ); 
        strcat(evLog, " : Vendor Specific Transfer "); 
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_TRANSFER, evLog, USBTU_WV_FILTER);
        }  


    USBTU_LOG ( "usbdVendorSpecific entered \n ");

    if (pActLen != NULL)
        *pActLen = 0;

    /* allocate a URB structure */

    if ( !(pUrb =  OSS_CALLOC ( sizeof (USBHST_URB ))))
        {
        USBTU_LOG ( "usbdVendorSpecific returns ERROR: malloc failed \n ");
        return ERROR;
        }

    /* allocate a setup packet structure */

    if ( !(pSetupPacket = OSS_CALLOC ( sizeof (USBHST_SETUP_PACKET))))
        {
        OSS_FREE (pUrb);
        USBTU_LOG ( "usbdVendorSpecific returns ERROR: malloc1 failed \n ");
        return ERROR;
        }

    /* initialize the setup packet structure */

    USBHST_FILL_SETUP_PACKET(pSetupPacket,requestType,request,value,
                             index,length);

    /* since the call has to block, create a semaphore */

    if ( !(semMutex = semBCreate (SEM_Q_FIFO,SEM_EMPTY)))
        {
        OSS_FREE (pUrb);
        OSS_FREE (pSetupPacket);
        USBTU_LOG ( "usbdVendorSpecific returns ERROR: sem create failed \n ");
        return ERROR;
        }

    /* initialize the URB structure */

    USBHST_FILL_CONTROL_URB (pUrb,
                             (UINT32)nodeId,
                             0,
                             pBfr,
                             length,
                             USBHST_SHORT_TRANSFER_OK,
                             (PVOID)pSetupPacket,
                             usbtuDataVendorSpecificCallback,
                             (PVOID)semMutex,
                             USBHST_SUCCESS
                             );



    /* submit the URB */



    if ( usbHstURBSubmit (pUrb) != USBHST_SUCCESS)
       {
       /* failed to submit the URB */

       OSS_FREE (pUrb);
       OSS_FREE (pSetupPacket);
       semDelete (semMutex);
       USBTU_LOG ( "usbdVendorSpecific returns ERROR: SubmitURB failed \n ");
       return ERROR;
       }

    /* block till vendor specific callback releases the semaphore */

    USBTU_LOG(" waiting for transfer to complete \n ");


    semTake (semMutex, WAIT_FOREVER);

    USBTU_LOG(" transfer completed \n ");

    /* URB completed, callback released the semaphore */

    if (pUrb->nStatus != USBHST_SUCCESS )
       {
       USBTU_LOG("usbdVendorSpecific returns ERROR:URB status!=USBHST_SUCCESS\n");
       s = ERROR;
       }

    /* update length fo data transfer */

    if (pActLen != NULL)
        *pActLen = pUrb->uTransferLength;

    /* release resources allocated */

    OSS_FREE (pUrb);
    OSS_FREE (pSetupPacket);
    semDelete (semMutex);

    /* return status */

    if ( s == OK)
        {
        USBTU_LOG ( "usbdVendorSpecific returns OK \n ");
        }
    else
        {
        USBTU_LOG ( "usbdVendorSpecific returns ERROR \n ");
        }

    return s;

    }

/***************************************************************************
*
* usbtuDataUrbCompleteCallback - Callback called on URB completeion
*
* This function is called from interrupt context by the USBD on a
* URB completion.
* RETURNS:  USBHST_SUCCESS , or USBHST_FAILURE on failure
*
* ERRNO: N/A
*/
USBHST_STATUS usbtuDataUrbCompleteCallback
    (
    pUSBHST_URB urbPtr       /* URB pointer */
    )

    {
    pUSBHST_URB pUrb = (pUSBHST_URB) urbPtr;
    pUSB_IRP pIrp = (pUSB_IRP)pUrb->pContext;
    pUSBTU_IRPCONTEXT pIrpContext = pIrp->usbdPtr;
    MSG_Q_ID msgQid;
    pUSBTU_IRP tempIrp;
    pUSBTU_IRP prevIrp;

    USBTU_LOG ( "usbtuDataUrbCompleteCallback entered \n ");

    /* map the error codes */

    if ( pUrb->nStatus != USBHST_SUCCESS )
        {

        switch (pUrb->nStatus)
            {
            case USBHST_TRANSFER_CANCELLED :
                pIrp->result = S_usbHcdLib_IRP_CANCELED;
                break;
             case USBHST_DATA_UNDERRUN_ERROR :
                pIrp->result = S_usbHcdLib_DATA_BFR_FAULT;
                break;
             case USBHST_TIMEOUT :
                pIrp->result = S_usbHcdLib_CRC_TIMEOUT;
                break;
             case USBHST_STALL_ERROR :
                pIrp->result = S_usbHcdLib_STALLED;
                break;
             case USBHST_DEVICE_NOT_RESPONDING_ERROR :
                pIrp->result = S_usbHcdLib_IRP_CANCELED;
                break;
             default:
              pIrp->result = ERROR;

             }
         }
    else
        pIrp->result = OK;

    /* release URB specific resources */

    if (pUrb->pTransferSpecificData)
        OSS_FREE(pUrb->pTransferSpecificData);

    /* Update the number of bytes transferred */
    pIrp->bfrList[pIrpContext->urbCompleteCount].actLen  = pUrb->uTransferLength;



    /* increment the URB completion count */

    (pIrpContext->urbCompleteCount)++;

    /* if all URB's have completed , call the callback  */

    if ( pIrpContext->urbCompleteCount == pIrp->bfrCount)
        {
        /* get the message queue id */
        msgQid = pIrpContext->msgQid;
        

      if ( msgQSend(msgQid,(char *) &pIrp ,
                         sizeof(char *) , NO_WAIT, MSG_PRI_NORMAL)
                         != OK)
                {
                USBTU_LOG ( "usbtuDataUrbCompleteCallback msgQSend failed \n ");
                }
        /* Take the mutex before accessing the list */
        OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);        

        tempIrp =pIrpContext->pipeInfo->irpList;
        prevIrp =pIrpContext->pipeInfo->irpList;

        /* unlink the irp from pipe info structure */

        while(tempIrp != NULL )
            {
            	if(tempIrp->pIrp == pIrp )
            	   break;
                else
                   {
                    prevIrp =  tempIrp;
                    tempIrp = tempIrp->nextIrp;
                   }
            }

       /* If the IRP is present, unlink it from the pipe's 
        * request list
        */ 
       if (tempIrp != NULL)
           {
       if(tempIrp == prevIrp)
       pIrpContext->pipeInfo->irpList = pIrpContext->pipeInfo->irpList->nextIrp;
       else
       {
       	 prevIrp->nextIrp = tempIrp->nextIrp;
       	 if( tempIrp->nextIrp)
            tempIrp->nextIrp->prevIrp = prevIrp;
       }
       OSS_FREE (tempIrp);
           }

        /* If the pipe is marked for deletion and the list is empty,
         * signal the semaphore so that the pipe can be safely deleted
         */
        if ((pIrpContext->pipeInfo->markedForDeletion == 1) &&
        (pIrpContext->pipeInfo->irpList == NULL)
         && (pIrpContext->pipeInfo->PipeDelete != NULL))
           {
           semGive(pIrpContext->pipeInfo->PipeDelete);
           }

        /* Release the semaphore */
        OSS_MUTEX_RELEASE (usbtuMutex);

        /* release IRP context resources */
        OSS_FREE ( pIrpContext->urbPtr);
        OSS_FREE ( pIrpContext);


        }

    USBTU_LOG ( "usbtuDataUrbCompleteCallback left \n ");
    return USBHST_SUCCESS;
    }

/***************************************************************************
*
* usbtuDataVendorSpecificCallback - Callback called on Vendor Specific Request
* completeion
*
* This function is called from interrupt context by the USBD on a
* Vendor Specific request completion.
*
* RETURNS:  USBHST_SUCCESS
*
* ERRNO: N/A
*/

USBHST_STATUS usbtuDataVendorSpecificCallback
    (
    pUSBHST_URB urbPtr   	/* URB pointer */
    )

    {
    pUSBHST_URB pUrb = (pUSBHST_URB) urbPtr;

    USBTU_LOG ( "usbtuDataVendorSpecificCallback entered \n ");

    /* release the semaphore */

    
    semGive (pUrb->pContext);
    
    USBTU_LOG ( "usbtuDataVendorSpecificCallback left \n ");
    return USBHST_SUCCESS;

    }

/* End of file. */



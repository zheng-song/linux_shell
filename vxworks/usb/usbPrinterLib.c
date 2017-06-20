/* usbPrinterLib.c - USB printer class drive with vxWorks SIO interface */

/* Copyright 2000-2002 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,15oct04,ami  Apigen Changes
01p,07oct04,hch  Merge from Bangalore develpment branch for SPR fixes.
01o,06oct04,mta  SPR95236
01n,06oct04,mta  SPR 95236:usbPrinterIoctl call is not protected my any mutex
01m,05may04,???  Race condition when device is disconnected while a file is
                 being printed. Fix for this race condition is provided here.
01l,04may04,???  Support for cancelling a print job
01k,18dec03,cfc  Fix compiler warning
01j,26apr02,wef  evaluating SPR #71793
01i,11mar02,wef  remove previous change.  
01h,04feb02,wef  Fix bug in listenForInput().  IRP bfr was incorrectly set to
		 be the address of the bfr ptr and not the bfr ptr SPR # 71793.
01g,30oct01,wef  Remove automatic buffer creations and repalce with OSS_MALLOC.
01f,18sep01,wef  merge from wrs.tor2_0.usb1_1-f for veloce
01e,01feb01,wef  Fixed tab stops
01d,12apr00,wef  Fixed uninitialized variable warning: pInEp in 
		 configureSioChan() 
01c,20mar00,rcb  Re-write code to fetch maxPacketSize from endpoint descriptor
		 on machines which don't support non-aligned word access.
01b,23nov99,rcb  Change return statement in usbPrinterTxStartup() to return 
		 status (formerly always returned OK).
01a,01sep99,rcb  written.
*/

/*
DESCRIPTION

This module implements the USB printer class driver for the vxWorks operating
system.  This module presents an interface which is a superset of the vxWorks
SIO (serial IO) driver model.  That is, this driver presents the external APIs
which would be expected of a standard "multi-mode serial (SIO) driver" and
adds certain extensions which are needed to address adequately the requirements
of the hot-plugging USB environment.

USB printers are described in the USB Printer Class definition.  This class
driver specification presents two kinds of printer: uni-directional printers
(output only) and bi-directional printers (capable of both output and input).
This class driver is capable of handling both kinds of printers.  If a printer
is uni-directional, then the SIO driver interface only allows characters to be
written to the printer.  If the printer is bi-directional, then the SIO interface
allows both output and input streams to be written/read.

Unlike most SIO drivers, the number of channels supported by this driver is not
fixed.	Rather, USB printers may be added or removed from the system at any
time.  This creates a situation in which the number of channels is dynamic, and
clients of usbPrinterLib.c need to be made aware of the appearance and 
disappearance of channels.  Therefore, this driver adds an additional set of
functions which allows clients to register for notification upon the insertion
and removal of USB printers, and hence the creation and deletion of channels.

This module itself is a client of the Universal Serial Bus Driver (USBD).  All
interaction with the USB buses and devices is handled through the USBD.


INITIALIZATION

As with standard SIO drivers, this driver must be initialized by calling
usbPrinterDevInit().  usbPrinterDevInit() in turn initializes its 
connection to the USBD and other internal resources needed for operation.  
Unlike some SIO drivers, there are no usbPrinterLib.c data structures which need 
to be initialized prior to calling usbPrinterDevInit().

Prior to calling usbPrinterDevInit(), the caller must ensure that the USBD
has been properly initialized by calling - at a minimum - usbdInitialize().
It is also the caller's responsibility to ensure that at least one USB HCD
(USB Host Controller Driver) is attached to the USBD - using the USBD function
usbdHcdAttach() - before printer operation can begin.  However, it is not 
necessary for usbdHcdAttach() to be alled prior to initializating usbPrinterLib.c.
usbPrinterLib.c uses the USBD dynamic attach services and is capable of 
recognizing USB printer attachment and removal on the fly.  Therefore, it is 
possible for USB HCDs to be attached to or detached from the USBD at run time
- as may be required, for example, in systems supporting hot swapping of
hardware.

usbPrinterLib.c does not export entry points for transmit, receive, and error
interrupt entry points like traditional SIO drivers.  All "interrupt" driven
behavior is managed by the underlying USBD and USB HCD(s), so there is no
need for a caller (or BSP) to connect interrupts on behalf of usbPrinterLib.c.
For the same reason, there is no post-interrupt-connect initialization code
and usbPrinterLib.c therefore also omits the "devInit2" entry point.


OTHER FUNCTIONS

usbPrinterLib.c also supports the SIO ioctl interface.	However, attempts to
set parameters like baud rates and start/stop bits have no meaning in the USB
environment and will be treated as no-ops.  

Additional ioctl functions have been added to allow the caller to retrieve the
USB printer's "device ID" string, the type of printer (uni- or bi-directional),
and the current printer status.  The "device ID" string is discussed in more
detail in the USB printer class specification and is based on the IEEE-1284
"device ID" string used by most 1284-compliant printers.  The printer status
function can be used to determine if the printer is selected, out of paper, or
has an error condition.


DATA FLOW

For each USB printer connected to the system, usbPrinterLib.c sets up a USB pipe
to output bulk data to the printer.  This is the pipe through which printer
control and page description data will be sent to the printer.	Additionally,
if the printer is bi-directional, usbPrinterLib.c also sets up a USB pipe to
receive bulk input data from the printer.  The meaining of data received from
a bi-directional printer depends on the specific make/model of printer.

The USB printer SIO driver supports only the SIO "interrupt" mode of operation
- SIO_MODE_INT.  Any attempt to place the driver in the polled mode will return
an error.

INCLUDE FILES: sioLib.h, usbPrinterLib.h
*/


/* includes */

#include "vxWorks.h"
#include "string.h"
#include "sioLib.h"
#include "errno.h"
#include "ctype.h"

#include "usb/usbPlatform.h"
#include "usb/ossLib.h" 	/* operations system srvcs */
#include "usb/usb.h"		/* general USB definitions */
#include "usb/usbPrinter.h"	/* USB printer definitions */
#include "usb/usbListLib.h"	/* linked list functions */
#include "usb/usbdLib.h"	/* USBD interface */
#include "usb/usbLib.h" 	/* USB utility functions */
#include "drv/usb/usbPrinterLib.h"  /* our API */


/* defines */

#define PRN_CLIENT_NAME     "usbPrinterLib" /* our USBD client name */

#define PRN_OUT_BFR_SIZE    4096	/* size of output bfr */
#define PRN_IN_BFR_SIZE     64		/* size of input bfr */

#define PRN_OUTPUT_TIMEOUT  30000	/* 30 second timeout */


/* IS_BIDIR() returns TRUE if deviceProtocol indicates device is bi-dir. */

#define IS_BIDIR(p) ((p) == USB_PROTOCOL_PRINTER_BIDIR)


/* typedefs */

/*
 * ATTACH_REQUEST
 */

typedef struct attach_request
    {
    LINK reqLink;			/* linked list of requests */
    USB_PRN_ATTACH_CALLBACK callback;	/* client callback routine */
    pVOID callbackArg;			/* client callback argument */
    } ATTACH_REQUEST, *pATTACH_REQUEST;


/* 
USB_PRN_SIO_CHAN is the internal data structure we use to track each USB
 * printer.
 */

typedef struct usb_prn_sio_chan
    {
    SIO_CHAN sioChan;		/* must be first field */

    LINK sioLink;		/* linked list of printer structs */

    UINT16 lockCount;		/* Count of times structure locked */

    USBD_NODE_ID nodeId;	/* printer node Id */
    UINT16 configuration;	/* configuration/interface reported as */
    UINT16 interface;		/* a printer by this device */
    UINT8 alternateSetting;

    UINT16 protocol;		/* protocol reported by device */

    BOOL connected;		/* TRUE if printer currently connected */

    USBD_PIPE_HANDLE outPipeHandle; /* USBD pipe handle for bulk OUT pipe */
    USB_IRP outIrp;		/* IRP to transmit output data */
    BOOL outIrpInUse;		/* TRUE while IRP is outstanding */
    char *outBfr;		/* pointer to output buffer */
    UINT16 outBfrLen;		/* size of output buffer */
    UINT32 outErrors;		/* count of IRP failures */

    USBD_PIPE_HANDLE inPipeHandle;  /* USBD pipe handle for bulk IN pipe */
    USB_IRP inIrp;		/* IRP to monitor input from printer */
    BOOL inIrpInUse;		/* TRUE while IRP is outstanding */
    char *inBfr;		/* pointer to input buffer */
    UINT16 inBfrLen;		/* size of input buffer */
    UINT32 inErrors;		/* count of IRP failures */

    int mode;			/* always SIO_MODE_INT */

    STATUS (*getTxCharCallback) (); /* tx callback */
    void *getTxCharArg; 	/* tx callback argument */

    STATUS (*putRxCharCallback) (); /* rx callback */
    void *putRxCharArg; 	/* rx callback argument */

    } USB_PRN_SIO_CHAN, *pUSB_PRN_SIO_CHAN;


/* forward static declarations */

LOCAL int usbPrinterTxStartup (SIO_CHAN * pSioChan);
LOCAL int usbPrinterCallbackInstall (SIO_CHAN *pSioChan, int callbackType,
				     STATUS (*callback)(void *, ...), void *callbackArg);
LOCAL int usbPrinterPollOutput (SIO_CHAN *pSioChan, char    outChar);
LOCAL int usbPrinterPollInput (SIO_CHAN *pSioChan, char *thisChar);
LOCAL int usbPrinterIoctl (SIO_CHAN *pSioChan, int request, void *arg);

LOCAL VOID usbPrinterIrpCallback (pVOID p);


/* locals */

LOCAL UINT16 initCount = 0;	/* Count of init nesting */

LOCAL MUTEX_HANDLE prnMutex;	/* mutex used to protect internal structs */

LOCAL LIST_HEAD sioList;	/* linked list of USB_PRN_SIO_CHAN */
LOCAL LIST_HEAD reqList;	/* Attach callback request list */

LOCAL USBD_CLIENT_HANDLE usbdHandle; /* our USBD client handle */


/* Channel function table. */

LOCAL SIO_DRV_FUNCS usbPrinterSioDrvFuncs =
    {
    usbPrinterIoctl,
    usbPrinterTxStartup,
    usbPrinterCallbackInstall,
    usbPrinterPollInput,
    usbPrinterPollOutput
    };


/***************************************************************************
*
* initiateOutput - initiates data transmission to printer
*
* If the output IRP is not already in use, this function fills the output
* buffer by invoking the "tx char callback".  If at least one character is
* available for output, the function then initiates the output IRP.
*
* RETURNS: OK, or ERROR if unable to initiate transmission
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS initiateOutput
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    pUSB_IRP pIrp = &pSioChan->outIrp;
    UINT16 count;


    /* Return immediately if the output IRP is already in use. */

    if (pSioChan->outIrpInUse)
	return OK;


    /* If there is no tx callback, return an error */

    if (pSioChan->getTxCharCallback == NULL)
	return ERROR;


    /* Fill the output buffer until it is full or until the tx callback
     * has no more data.  Return if there is no data available.
     */

    count = 0;

    while (count < pSioChan->outBfrLen &&
	   (*pSioChan->getTxCharCallback) (pSioChan->getTxCharArg,
					   &pSioChan->outBfr [count]) 
					 == OK)
	{
	count++;
	}

    if (count == 0)
	return OK;


    /* Initialize IRP */

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pSioChan;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = usbPrinterIrpCallback;
    pIrp->timeout = PRN_OUTPUT_TIMEOUT;
    pIrp->transferLen = count;

    pIrp->bfrCount = 1;
    pIrp->bfrList [0].pid = USB_PID_OUT;
    pIrp->bfrList [0].pBfr = (UINT8 *) pSioChan->outBfr;
    pIrp->bfrList [0].bfrLen = count;


    /* Submit IRP */

    if (usbdTransfer (usbdHandle, pSioChan->outPipeHandle, pIrp) != OK)
	return ERROR;

    pSioChan->outIrpInUse = TRUE;

    return OK;
    }

/*****************************************************************************
*
* cancelPrint - Cancels the printing operation
* This function is used to abort the erp which is submitted for printing
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrnCancelPrint
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    if (pSioChan->outIrpInUse)
        {
        if (usbdTransferAbort ( usbdHandle, pSioChan->outPipeHandle,
                               &(pSioChan->outIrp)) == OK )
            return OK;
        }
    return ERROR;
    }

/***************************************************************************
*
* listenForInput - Initialize IRP to listen for input from printer
*
* This function initialize IRP to listen for input from printer.
*
* RETURNS: OK, or ERROR if unable to submit IRP to listen for input
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS listenForInput
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    pUSB_IRP pIrp = &pSioChan->inIrp;

    /* Initialize IRP */

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pSioChan;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = usbPrinterIrpCallback;
    pIrp->timeout = USB_TIMEOUT_NONE;
    pIrp->transferLen = pSioChan->inBfrLen;

    pIrp->bfrCount = 1;
    pIrp->bfrList [0].pid = USB_PID_IN;

    pIrp->bfrList [0].pBfr = (UINT8 *) pSioChan->inBfr;

    pIrp->bfrList [0].bfrLen = pSioChan->inBfrLen;


    /* Submit IRP */

    if (usbdTransfer (usbdHandle, pSioChan->inPipeHandle, pIrp) != OK)
	return ERROR;

    pSioChan->inIrpInUse = TRUE;

    return OK;
    }


/***************************************************************************
*
* usbPrinterIrpCallback - Invoked upon IRP completion/cancellation
*
* Examines the cause of the IRP completion.  If completion was successful,
* interprets the USB printer's boot report and re-submits the IRP.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID usbPrinterIrpCallback
    (
    pVOID p	    /* completed IRP */
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;
    pUSB_PRN_SIO_CHAN pSioChan = pIrp->userPtr;
    UINT16 count;


    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);


    /* Determine if we're dealing with the output or input IRP. */

    if (pIrp == &pSioChan->outIrp)
	{
	/* Output IRP completed */

	pSioChan->outIrpInUse = FALSE;

	if (pIrp->result != OK)
	    pSioChan->outErrors++;

	/* Unless the IRP was cancelled - implying the channel is being
	 * torn down, see if there's any more data to send.
	 */

	if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
	    initiateOutput (pSioChan);
	}
    else
	{
	/* Input IRP completed */

	pSioChan->inIrpInUse = FALSE;

	if (pIrp->result != OK)
	    pSioChan->inErrors++;

	/* If the IRP was successful, and if an "rx callback" has been
	 * installed, then pass the data back to the client.
	 */

	if (pIrp->result == OK && 
	    pSioChan->putRxCharCallback != NULL)
	    {
	    for (count = 0; count < pIrp->bfrList [0].actLen; count++)

		(*pSioChan->putRxCharCallback) (pSioChan->putRxCharArg,
						pIrp->bfrList[0].pBfr[count]);
	    }

	/* Unless the IRP was cancelled - implying the channel is being
	 * torn down, re-initiate the "in" IRP to listen for more data from
	 * the printer.
	 */

	if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
	    listenForInput (pSioChan);
	}


    OSS_MUTEX_RELEASE (prnMutex);
    }


/***************************************************************************
*
* findEndpoint - Searches for a BULK endpoint of the indicated direction.
*
* This function searches for a bulk endpoitn of the indicated <direction>
*
* RETURNS: pointer to matching endpoint descriptor or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_ENDPOINT_DESCR findEndpoint
    (
    pUINT8 pBfr,
    UINT16 bfrLen,
    UINT16 direction
    )

    {
    pUSB_ENDPOINT_DESCR pEp;

    while ((pEp = usbDescrParseSkip (&pBfr, 
				     &bfrLen, 
				     USB_DESCR_ENDPOINT)) 
				  != NULL)
	{
	if ((pEp->attributes & USB_ATTR_EPTYPE_MASK) == USB_ATTR_BULK &&
	    (pEp->endpointAddress & USB_ENDPOINT_DIR_MASK) == direction)
	    break;
	}

    return pEp;
    }


/***************************************************************************
*
* configureSioChan - configure USB printer for operation
*
* Selects the configuration/interface specified in the <pSioChan>
* structure.  These values come from the USBD dynamic attach callback,
* which in turn retrieved them from the configuration/interface
* descriptors which reported the device to be a printer.
*
* RETURNS: OK if successful, else ERROR if failed to configure channel
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS configureSioChan
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUSB_INTERFACE_DESCR pIfDescr;
    pUSB_ENDPOINT_DESCR pOutEp;
    pUSB_ENDPOINT_DESCR pInEp = NULL;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    UINT16 actLen;
    UINT16 ifNo;
    UINT16 maxPacketSizeIn;
    UINT16 maxPacketSizeOut;


    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	return ERROR;

    /* Read the configuration descriptor to get the configuration selection
     * value and to determine the device's power requirements.
     */

    if (usbdDescriptorGet (usbdHandle, 
			   pSioChan->nodeId,
		   USB_RT_STANDARD | USB_RT_DEVICE, USB_DESCR_CONFIGURATION, 
			   0, 
			   0,
			   USB_MAX_DESCR_LEN,
			   pBfr, 
			   &actLen) 
			 != OK)
    	{
	OSS_FREE (pBfr);
	return ERROR;
	}

    if ((pCfgDescr = usbDescrParse (pBfr, 
				    actLen, 
				    USB_DESCR_CONFIGURATION)) 
				  == NULL)
        {
        OSS_FREE (pBfr);
        return ERROR;
        }



    /* Look for the interface indicated in the pSioChan structure. */

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
	if (ifNo == pSioChan->interface)
	    break;
	ifNo++;
	}

    if (pIfDescr == NULL)
        {
        OSS_FREE (pBfr);
        return ERROR;
        }



    /* Retrieve the endpoint descriptor(s) following the identified interface
     * descriptor.
     */

    if ((pOutEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_OUT)) == NULL)
        {
        OSS_FREE (pBfr);
        return ERROR;
        }


    if (IS_BIDIR (pSioChan->protocol))
	if ((pInEp = findEndpoint (pScratchBfr, actLen, USB_ENDPOINT_IN)) == NULL)
            {
            OSS_FREE (pBfr);
            return ERROR;
            }



    /* Select the configuration. */

    if (usbdConfigurationSet (usbdHandle, 
			      pSioChan->nodeId,
			      pCfgDescr->configurationValue, 
			      pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) 
			   != OK)
        {
        OSS_FREE (pBfr);
        return ERROR;
        }



    /* Select interface 
     * 
     * NOTE: Some devices may reject this command, and this does not represent
     * a fatal error.  Therefore, we ignore the return status.
     */

    pSioChan->alternateSetting = pIfDescr->alternateSetting;

    usbdInterfaceSet (usbdHandle, pSioChan->nodeId,
    pSioChan->interface, pIfDescr->alternateSetting);


    /* Create a pipe for output to the printer. */

    maxPacketSizeOut = *((pUINT8) &pOutEp->maxPacketSize) |
			(*(((pUINT8) &pOutEp->maxPacketSize) + 1) << 8);

    if (usbdPipeCreate (usbdHandle, 
			pSioChan->nodeId, 
			pOutEp->endpointAddress, 
			pCfgDescr->configurationValue,
			pSioChan->interface, 
			USB_XFRTYPE_BULK, 
			USB_DIR_OUT,
			maxPacketSizeOut, 
			0, 
			0, 
			&pSioChan->outPipeHandle) 
		      != OK)
        {
        OSS_FREE (pBfr);
        return ERROR;
        }


    /* If the device is bi-direction, create a pipe to listen for input from
     * the device and submit an IRP to listen for input.
     */

    if (IS_BIDIR (pSioChan->protocol))
	{
	maxPacketSizeIn = *((pUINT8) &pInEp->maxPacketSize) |
			    (*(((pUINT8) &pInEp->maxPacketSize) + 1) << 8);

	if (usbdPipeCreate (usbdHandle, 
			    pSioChan->nodeId, 
			    pInEp->endpointAddress, 
			    pCfgDescr->configurationValue,
			    pSioChan->interface, 
			    USB_XFRTYPE_BULK, 
			    USB_DIR_IN,
			    maxPacketSizeIn, 
			    0, 
			    0, 
			    &pSioChan->inPipeHandle) 
			  != OK)
	    {
            OSS_FREE (pBfr);
            return ERROR;
            }

	if (listenForInput (pSioChan) != OK)
            {
            OSS_FREE (pBfr);
            return ERROR;
            }

	}

    OSS_FREE (pBfr);

    return OK;
    }


/***************************************************************************
*
* destroyAttachRequest - disposes of an ATTACH_REQUEST structure
*
* This functon disposes of an ATTACH_REQUEST structure
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID destroyAttachRequest
    (
    pATTACH_REQUEST pRequest
    )

    {
    /* Unlink request */

    usbListUnlink (&pRequest->reqLink);

    /* Dispose of structure */

    OSS_FREE (pRequest);
    }


/***************************************************************************
*
* destroySioChan - disposes of a USB_PRN_SIO_CHAN structure
*
* Unlinks the indicated USB_PRN_SIO_CHAN structure and de-allocates
* resources associated with the channel.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID destroySioChan
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    if (pSioChan != NULL)
	{
	/* Unlink the structure. */

	usbListUnlink (&pSioChan->sioLink);


	/* Release pipes and wait for IRPs to be cancelled if necessary. */

	if (pSioChan->outPipeHandle != NULL)
	    usbdPipeDestroy (usbdHandle, pSioChan->outPipeHandle);

	if (pSioChan->inPipeHandle != NULL)
	    usbdPipeDestroy (usbdHandle, pSioChan->inPipeHandle);

    /* The outIrpInUse or inIrpInUse can be set to FALSE only when the mutex
     * is released. So releasing the mutex here
     */  
    OSS_MUTEX_RELEASE (prnMutex);

	while (pSioChan->outIrpInUse || pSioChan->inIrpInUse)
	    OSS_THREAD_SLEEP (1);

    /* Acquire the mutex again */ 
    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);

	/* release buffers */

	if (pSioChan->outBfr != NULL)
	    OSS_FREE (pSioChan->outBfr);

	if (pSioChan->inBfr != NULL)
	    OSS_FREE (pSioChan->inBfr);


	/* Release structure. */

	OSS_FREE (pSioChan);
	}
    }


/***************************************************************************
*
* createSioChan - creates a new USB_PRN_SIO_CHAN structure
*
* Creates a new USB_PRN_SIO_CHAN structure for the indicated <nodeId>.
* If successful, the new structure is linked into the sioList upon 
* return.
*
* <configuration> and <interface> identify the configuration/interface
* that first reported itself as a printer for this device.
*
* RETURNS: pointer to newly created structure, or NULL if failure
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_PRN_SIO_CHAN createSioChan
    (
    USBD_NODE_ID nodeId,
    UINT16 configuration,
    UINT16 interface,
    UINT16 deviceProtocol
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan;


    /* Try to allocate space for a new printer struct */

    if ((pSioChan = OSS_CALLOC (sizeof (*pSioChan))) == NULL ||
	(pSioChan->outBfr = OSS_MALLOC (PRN_OUT_BFR_SIZE)) == NULL)
	{
	destroySioChan (pSioChan);
	return NULL;
	}

    if (IS_BIDIR (deviceProtocol))
	if ((pSioChan->inBfr = OSS_MALLOC (PRN_IN_BFR_SIZE)) == NULL)
	    {
	    destroySioChan (pSioChan);
	    return NULL;
	    }

    pSioChan->outBfrLen = PRN_OUT_BFR_SIZE;
    pSioChan->inBfrLen = PRN_IN_BFR_SIZE;

    pSioChan->sioChan.pDrvFuncs = &usbPrinterSioDrvFuncs;
    pSioChan->nodeId = nodeId;
    pSioChan->connected = TRUE;
    pSioChan->mode = SIO_MODE_INT;

    pSioChan->configuration = configuration;
    pSioChan->interface = interface;

    pSioChan->protocol = deviceProtocol;


    /* Try to configure the printer. */

    if (configureSioChan (pSioChan) != OK)
	{
	destroySioChan (pSioChan);
	return NULL;
	}


    /* Link the newly created structure. */

    usbListLink (&sioList, pSioChan, &pSioChan->sioLink, LINK_TAIL);

    return pSioChan;
    }


/***************************************************************************
*
* findSioChan - Searches for a USB_PRN_SIO_CHAN for indicated node ID
*
* This function searches for the pointer to USB_PRN_SIO_CHAN structure for
* specified <nodeId>.
* 
* RETURNS: pointer to matching USB_PRN_SIO_CHAN or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_PRN_SIO_CHAN findSioChan
    (
    USBD_NODE_ID nodeId
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = usbListFirst (&sioList);

    while (pSioChan != NULL)
	{
	if (pSioChan->nodeId == nodeId)
	    break;

	pSioChan = usbListNext (&pSioChan->sioLink);
	}

    return pSioChan;
    }


/***************************************************************************
*
* notifyAttach - Notifies registered callers of attachment/removal
*
* This function notifies the regsitered clients about the attachment and removal
* of device
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    pUSB_PRN_SIO_CHAN pSioChan,
    UINT16 attachCode
    )

    {
    pATTACH_REQUEST pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
	{
	(*pRequest->callback) (pRequest->callbackArg, 
			       (SIO_CHAN *) pSioChan, 
			       attachCode);

	pRequest = usbListNext (&pRequest->reqLink);
	}
    }


/***************************************************************************
*
* usbPrinterAttachCallback - called by USBD when printer attached/removed
*
* The USBD will invoke this callback when a USB printer is attached to or
* removed from the system.  <nodeId> is the USBD_NODE_ID of the node being
* attached or removed.	<attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* printers generally report their class information at the interface level,
* so <configuration> and <interface> will indicate the configuratin/interface
* that reports itself as a printer.  <deviceClass> and <deviceSubClass> 
* will match the class/subclass for which we registered.  <deviceProtocol>
* will tell us if this is a uni- or bi-directional device.
*
* NOTE: The USBD will invoke this function once for each configuration/
* interface which reports itself as a printer.	So, it is possible that
* a single device insertion/removal may trigger multiple callbacks.  We
* ignore all callbacks except the first for a given device.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID usbPrinterAttachCallback
    (
    USBD_NODE_ID nodeId, 
    UINT16 attachAction, 
    UINT16 configuration,
    UINT16 interface,
    UINT16 deviceClass, 
    UINT16 deviceSubClass, 
    UINT16 deviceProtocol
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan;


    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);

    /* Depending on the attach code, add a new printer or disable one
     * that's already been created.
     */

    switch (attachAction)
	{
	case USBD_DYNA_ATTACH:

	    /* A new device is being attached.  Check if we already 
	     * have a structure for this device.
	     */

	    if (findSioChan (nodeId) != NULL)
		break;

	    /* Create a new structure to manage this device.  If there's
	     * an error, there's nothing we can do about it, so skip the
	     * device and return immediately. 
	     */

	    if ((pSioChan = createSioChan (nodeId, 
					   configuration, 
					   interface,
					   deviceProtocol)) 
					 == NULL)
		break;

	    /* Notify registered callers that a new printer has been
	     * added and a new channel created.
	     */

	    notifyAttach (pSioChan, USB_PRN_ATTACH);

	    break;


	case USBD_DYNA_REMOVE:

	    /* A device is being detached.	Check if we have any
	     * structures to manage this device.
	     */

	    if ((pSioChan = findSioChan (nodeId)) == NULL)
		break;

	    /* The device has been disconnected. */

	    pSioChan->connected = FALSE;

	    /* Notify registered callers that the printer has been
	     * removed and the channel disabled. 
	     *
	     * NOTE: We temporarily increment the channel's lock count
	     * to prevent usbPrinterSioChanUnlock() from destroying the
	     * structure while we're still using it.
	     */

	    pSioChan->lockCount++;

	    notifyAttach (pSioChan, USB_PRN_REMOVE);

	    pSioChan->lockCount--;

	    /* If no callers have the channel structure locked, destroy
	     * it now.  If it is locked, it will be destroyed later during
	     * a call to usbPrinterUnlock().
	     */

	    if (pSioChan->lockCount == 0)
		destroySioChan (pSioChan);

	    break;
	}

    OSS_MUTEX_RELEASE (prnMutex);
    }


/***************************************************************************
*
* doShutdown - shuts down USB printer SIO driver
*
* <errCode> should be OK or S_usbPrinterLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: appropiate errorcode
*
* \NOMANUAL
*/

LOCAL STATUS doShutdown
    (
    int errCode
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_PRN_SIO_CHAN pSioChan;


    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
	destroyAttachRequest (pRequest);


    /* Dispose of any open printer connections. */

    while ((pSioChan = usbListFirst (&sioList)) != NULL)
	destroySioChan (pSioChan);
    

    /* Release our connection to the USBD.  The USBD automatically 
     * releases any outstanding dynamic attach requests when a client
     * unregisters.
     */

    if (usbdHandle != NULL)
	{
	usbdClientUnregister (usbdHandle);
	usbdHandle = NULL;
	}


    /* Release resources. */

    if (prnMutex != NULL)
	{
	OSS_MUTEX_DESTROY (prnMutex);
	prnMutex = NULL;
	}


    return ossStatus (errCode);
    }


/***************************************************************************
*
* usbPrnDeviceIdGet - Reads USB printer device ID
*
* This function reads the USB printer device ID
*
* RETURNS: OK, or ERROR if unable to read device ID.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPrnDeviceIdGet
    (
    pUSB_PRN_SIO_CHAN pSioChan,
    pUSB_PRINTER_CAPABILITIES pBfr  
    )

    {
    UINT16 actLen;

    if (usbdVendorSpecific (usbdHandle, 
			    pSioChan->nodeId,
			    USB_RT_DEV_TO_HOST | USB_RT_CLASS | USB_RT_INTERFACE,
			    USB_REQ_PRN_GET_DEVICE_ID, 
			    pSioChan->configuration,
			    (pSioChan->interface << 8) | pSioChan->alternateSetting, 
			    USB_PRN_MAX_DEVICE_ID_LEN, 
			    (pUINT8) pBfr, 
			    &actLen) 
			  != OK 
	|| actLen < sizeof (UINT16) /* size of 1284 device ID length prefix */)
	{
	return ERROR;
	}

    return OK;
    }


/***************************************************************************
*
* usbPrnPortStatusGet - Reads USB printer status
*
* This function reads the printer status.
*
* RETURNS: OK, or ERROR if unable to read printer status.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPrnPortStatusGet
    (
    pUSB_PRN_SIO_CHAN pSioChan,
    pUSB_PRINTER_PORT_STATUS pBfr
    )

    {
    UINT16 actLen;

    if (usbdVendorSpecific (usbdHandle, 
			    pSioChan->nodeId,
			    USB_RT_DEV_TO_HOST | USB_RT_CLASS | USB_RT_INTERFACE,
			    USB_REQ_PRN_GET_PORT_STATUS, 
			    0, 
			    pSioChan->interface,
			    sizeof (*pBfr), 
			    (pUINT8) pBfr, 
			    &actLen) 
			   != OK 
	|| actLen < sizeof (*pBfr))
	{
	return ERROR;
	}

    return OK;
    }


/***************************************************************************
*
* usbPrnSoftReset - Issues soft reset to printer device
*
* This function issues a soft reset to the printer device as a vendor specific
* request
*
* RETURNS: OK, or ERROR if unable to issue soft reset.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbPrnSoftReset
    (
    pUSB_PRN_SIO_CHAN pSioChan
    )

    {
    if (usbdVendorSpecific (usbdHandle, 
			    pSioChan->nodeId,
			    USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_OTHER,
			    USB_REQ_PRN_SOFT_RESET, 
			    0, 
			    pSioChan->interface, 
			    0, 
			    NULL, 
			    NULL) 
			  != OK)
	{
	return ERROR;
	}

    return OK;
    }


/***************************************************************************
*
* usbPrinterIoctl - special device control
*
* usbPrinterLib supports the following IOCTLs:
*
* SIO_MODE_GET - returns current SIO driver mode (always SIO_MODE_INT).
* SIO_MODE_SET - sets current SIO driver mode (must be SIO_MODE_INT).
* SIO_AVAIL_MODES_GET - returns available SIO driver modes (SIO_MODE_INT).
* SIO_OPEN - always returns OK.
* SIO_USB_PRN_DEVICE_ID_GET - returns USB printer device ID
* SIO_USB_PRN_PROTOCOL_GET - returns USB printer protocol (uni- or bi-dir)
* SIO_USB_PRN_STATUS_GET - returns USB printer status
* SIO_USB_PRN_SOFT_RESET - issues soft reset to printer
*
* For SIO_USB_PRN_DEVICE_ID_GET, <someArg> must be a pointer a buffer of
* at least USB_PRN_MAX_DEVICE_ID_LEN bytes in length in which the device
* ID string for the device will be stored.
*
* For SIO_USB_PRN_PROTOCOL_GET, <someArg> must be a pointer to a UINT8
* variable in which the device's "protocol" byte will be stored.  The
* device generally reports its protocol as USB_PROTOCOL_PRINTER_UNIDIR or
* USB_PROTOCOL_PRINTER_BIDIR.  If the device reports that it is bi-
* directional, then usbPrinterLib will allow input to be read from the
* device.
*
* For SIO_USB_PRN_STATUS_GET, <someArg> must be a pointer to a
* USB_PRINTER_PORT_STATUS structure in which the device's current status
* will be stored.  The format of the status structure is defined in
* usbPrinter.h.
*
* SIO_USB_PRN_SOFT_RESET does not take an argument in <someArg>.
*
* RETURNS: OK on success, ENOSYS on unsupported request, EIO on failed
* request.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrinterIoctl
    (
    SIO_CHAN *pChan,	    /* device to control */
    int request,	/* request code */
    void *someArg	/* some argument */
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = (pUSB_PRN_SIO_CHAN) pChan;
    int arg = (int) someArg;
    int status = 0;

    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);


    switch (request)
	{
	case SIO_MODE_SET:

	    /* Set driver operating mode: interrupt or polled.
	     *
	     * NOTE: This driver supports only SIO_MODE_INT.
	     */

	    if (arg != SIO_MODE_INT)
		    status = EIO;
            else
                {
	        pSioChan->mode = arg;
	        status = OK;
                }
            break;


	case SIO_MODE_GET:

	    /* Return current driver operating mode for channel */

	    *((int *) arg) = pSioChan->mode;
	    status = OK;

            break;

	case SIO_AVAIL_MODES_GET:

	    /* Return modes supported by driver. */

	    *((int *) arg) = SIO_MODE_INT;
	    status = OK;

            break;

	case SIO_OPEN:

	    /* Channel is always open. */

	    status = OK;
            break;

	case SIO_USB_PRN_DEVICE_ID_GET:

	    if (usbPrnDeviceIdGet (pSioChan,
				   (pUSB_PRINTER_CAPABILITIES) arg)
				 != OK)
	        status = EIO;
            else
	        status = OK;
            break;

	case SIO_USB_PRN_PROTOCOL_GET:

	    /* return protocol byte reported by printer. */

	    *((pUINT8) arg) = pSioChan->protocol;
	    status = OK;
            break;

	case SIO_USB_PRN_STATUS_GET:

	    if (usbPrnPortStatusGet (pSioChan,
				     (pUSB_PRINTER_PORT_STATUS) arg)
				   != OK)
                status = EIO;
            else
	        status = OK;
            break;

	case SIO_USB_PRN_SOFT_RESET:

	    if (usbPrnSoftReset (pSioChan) != OK)
                status = EIO;
            else
	        status = OK;
            break;

    case SIO_USB_PRN_CANCEL:
         if ( usbPrnCancelPrint (pSioChan) != OK)
             status =EIO;
         else
             status = OK;
         break;

	case SIO_BAUD_SET:	/* set baud rate */
	case SIO_BAUD_GET:	/* get baud rate */
	case SIO_HW_OPTS_SET:   /* optional, not supported */
	case SIO_HW_OPTS_GET:   /* optional, not supported */
	case SIO_HUP:	/* hang up is not supported */
	default:	    /* unknown/unsupported command. */

	    status = ENOSYS;
	}

    OSS_MUTEX_RELEASE (prnMutex);
    return status;

    }


/***************************************************************************
*
* usbPrinterTxStartup - start the interrupt transmitter
*
* This function will be called when characters are available for transmission
* to the printer.  
*
* RETURNS: OK, or EIO if unable to start transmission to printer
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrinterTxStartup
    (
    SIO_CHAN *pChan	/* channel to start */
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = (pUSB_PRN_SIO_CHAN) pChan;
    int status = OK;


    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);

    if (initiateOutput (pSioChan) != OK)
	status = EIO;

    OSS_MUTEX_RELEASE (prnMutex);

    return status;
    }


/***************************************************************************
*
* usbPrinterCallbackInstall - install ISR callbacks to get/put chars
*
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.=
*
* RETURNS: OK on success, or ENOSYS for an unsupported callback type.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrinterCallbackInstall
    (
    SIO_CHAN *pChan,	    /* channel */
    int callbackType,	    /* type of callback */
    STATUS (*callback) (void *tmp, ...),  /* callback */
    void *callbackArg	    /* parameter to callback */
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = (pUSB_PRN_SIO_CHAN) pChan;

    switch (callbackType)
	{
	case SIO_CALLBACK_GET_TX_CHAR:
	    pSioChan->getTxCharCallback = (STATUS (*)()) (callback);
	    pSioChan->getTxCharArg = callbackArg;
	    return OK;

	case SIO_CALLBACK_PUT_RCV_CHAR:
	    pSioChan->putRxCharCallback = (STATUS (*)()) (callback);
	    pSioChan->putRxCharArg = callbackArg;
	    return OK;

	default:
	    return ENOSYS;
	}
    }


/***************************************************************************
*
* usbPrinterPollOutput - output a character in polled mode
*
* The USB printer driver supports only interrupt-mode operation.  Therefore,
* this function always returns the error ENOSYS.
*
* RETURNS: ENOSYS
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrinterPollOutput
    (
    SIO_CHAN *pChan,
    char outChar
    )

    {
    return ENOSYS;
    }


/***************************************************************************
*
* usbPrinterPollInput - poll the device for input
*
* The USB printer driver supports only interrupt-mode operation.  Therefore,
* this function always returns the error ENOSYS.
*
* RETURNS: ENOSYS
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL int usbPrinterPollInput
    (
    SIO_CHAN *pChan,
    char *thisChar
    )

    {
    return ENOSYS;
    }


/***************************************************************************
*
* usbPrinterDevInit - initialize USB printer SIO driver
*
* Initializes the USB printer SIO driver.  The USB printer SIO driver
* maintains an initialization count, so calls to this function may be
* nested.
*
* RETURNS: OK, or ERROR if unable to initialize.
*
* ERRNO:
* \is
* \i S_usbPrinterLib_OUT_OF_RESOURCES
* Sufficient resources not available
*
* \i S_usbPrinterLib_USBD_FAULT
* Error in USBD layer
* \ie
*/

STATUS usbPrinterDevInit (void)
    {
    /* If not already initialized, then initialize internal structures
     * and connection to USBD.
     */

    if (initCount == 0)
	{
	/* Initialize lists, structures, resources. */

	memset (&sioList, 0, sizeof (sioList));
	memset (&reqList, 0, sizeof (reqList));
	prnMutex = NULL;
	usbdHandle = NULL;

	if (OSS_MUTEX_CREATE (&prnMutex) != OK)
	    return doShutdown (S_usbPrinterLib_OUT_OF_RESOURCES);


	/* Establish connection to USBD */

	if (usbdClientRegister (PRN_CLIENT_NAME, 
				&usbdHandle) 
			      != OK ||
	    usbdDynamicAttachRegister (usbdHandle,
				       USB_CLASS_PRINTER,
				       USB_SUBCLASS_PRINTER, 
				       USBD_NOTIFY_ALL,
				       usbPrinterAttachCallback) 
				    != OK)
		{
		return doShutdown (S_usbPrinterLib_USBD_FAULT);
		}
	}

    initCount++;

    return OK;
    }


/***************************************************************************
*
* usbPrinterDevShutdown - shuts down printer SIO driver
*
* This function shutdowns the printer SIO driver when <initCount> becomes 0
*
* RETURNS: OK, or ERROR if unable to shutdown.
*
* ERRNO:
* \is
* \i S_usbPrinterLib_NOT_INITIALIZED
* Printer not initialized
* \ie
*/

STATUS usbPrinterDevShutdown (void)
    {
    /* Shut down the USB printer SIO driver if the initCount goes to 0. */

    if (initCount == 0)
	return ossStatus (S_usbPrinterLib_NOT_INITIALIZED);

    if (--initCount == 0)
	return doShutdown (OK);

    return OK;
    }


/***************************************************************************
*
* usbPrinterDynamicAttachRegister - Register printer attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_PRN_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     SIO_CHAN *pSioChan,
*     UINT16 attachCode
*     );
* \ce
*
* usbPrinterLib will invoke <callback> each time a USB printer
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also be passed a pointer to the 
* SIO_CHAN structure for the channel being created/destroyed and
* an attach code of USB_PRN_ATTACH or USB_PRN_REMOVE.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbPrinterLib_BAD_PARAM
* Bad Parameters received
*
* \i S_usbPrinterLib_OUT_OF_MEMORY
* System out of memory
* \ie
*/

STATUS usbPrinterDynamicAttachRegister
    (
    USB_PRN_ATTACH_CALLBACK callback,	/* new callback to be registered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_PRN_SIO_CHAN pSioChan;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
	return ossStatus (S_usbPrinterLib_BAD_PARAM);


    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);


    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
	status = S_usbPrinterLib_OUT_OF_MEMORY;
    else
	{
	pRequest->callback = callback;
	pRequest->callbackArg = arg;

	usbListLink (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL);

	
	/* Perform an initial notification of all currrently attached
	 * printer devices.
	 */

	pSioChan = usbListFirst (&sioList);

	while (pSioChan != NULL)
	    {
	    if (pSioChan->connected)
		(*callback) (arg, (SIO_CHAN *) pSioChan, USB_PRN_ATTACH);

	    pSioChan = usbListNext (&pSioChan->sioLink);
	    }
	}

    OSS_MUTEX_RELEASE (prnMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbPrinterDynamicAttachUnregister - Unregisters printer attach callback
*
* This function cancels a previous request to be dynamically notified for
* printer attachment and removal.  The <callback> and <arg> paramters must
* exactly match those passed in a previous call to 
* usbPrinterDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister callback
*
* ERRNO:
* \is
* \i  S_usbPrinterLib_NOT_REGISTERED
* Could not register the attachment callback
* \ie
*/

STATUS usbPrinterDynamicAttachUnRegister
    (
    USB_PRN_ATTACH_CALLBACK callback,	/* callback to be unregistered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    int status = S_usbPrinterLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
	{
	if (callback == pRequest->callback && arg == pRequest->callbackArg)
	    {
	    /* We found a matching notification request. */

	    destroyAttachRequest (pRequest);
	    status = OK;
	    break;
	    }

	pRequest = usbListNext (&pRequest->reqLink);
	}

    OSS_MUTEX_RELEASE (prnMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbPrinterSioChanLock - Marks SIO_CHAN structure as in use
*
* A caller uses usbPrinterSioChanLock() to notify usbPrinterLib that
* it is using the indicated SIO_CHAN structure.  usbPrinterLib maintains
* a count of callers using a particular SIO_CHAN structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* USB printer is removed from the system.  So long as the "lock count"
* is greater than zero, usbPrinterLib will not dispose of an SIO_CHAN
* structure.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure in use.
*
* ERRNO: none
*/

STATUS usbPrinterSioChanLock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as in use */
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = (pUSB_PRN_SIO_CHAN) pChan;
    pSioChan->lockCount++;

    return OK;
    }


/***************************************************************************
*
* usbPrinterSioChanUnlock - Marks SIO_CHAN structure as unused
*
* This function releases a lock placed on an SIO_CHAN structure.  When a
* caller no longer needs an SIO_CHAN structure for which it has previously
* called usbPrinterSioChanLock(), then it should call this function to
* release the lock.
*
* NOTE: If the underlying USB printer device has already been removed
* from the system, then this function will automatically dispose of the
* SIO_CHAN structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the SIO_CHAN again structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure unused
*
* ERRNO:
* \is
* \i S_usbPrinterLib_NOT_LOCKED
* No lock to unclock
* \ie
*/

STATUS usbPrinterSioChanUnlock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as unused */
    )

    {
    pUSB_PRN_SIO_CHAN pSioChan = (pUSB_PRN_SIO_CHAN) pChan;
    int status = OK;


    OSS_MUTEX_TAKE (prnMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0)
	status = S_usbPrinterLib_NOT_LOCKED;
    else
	{
	/* If this is the last lock and the underlying USB printer is
	 * no longer connected, then dispose of the printer.
	 */

	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
	    destroySioChan (pSioChan);
	}

    OSS_MUTEX_RELEASE (prnMutex);

    return ossStatus (status);
    }


/* end of file. */


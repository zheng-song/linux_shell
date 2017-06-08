/* usbMouseLib.c - USB mouse class drive with vxWorks SIO interface */

/* Copyright 2000-2002 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,30oct04,hch  SPR #101127 and 98731
01f,15oct04,ami  Apigen Changes
01e,03aug04,ami  Warning Messages Removed
01e,12jan04,cfc  merge synchronization fix
01d,23sep03,cfc  Fixed Idle timeout issue
01c,29oct01,wef  Remove automatic buffer creations and repalce with OSS_MALLOC.
01b,20mar00,rcb  Re-write code to fetch maxPacketSize from endpoint descriptor
	 	 on machines which don't support non-aligned word access.
	 	 Allocate "report" structure separately from SIO_CHAN in order
	 	 to avoid cache problems on MIPS platform.
01a,07oct99,rcb  written.
*/

/*
DESCRIPTION

This module implements the USB mouse class driver for the vxWorks operating
system.  This module presents an interface which is a superset of the vxWorks
SIO (serial IO) driver model.  That is, this driver presents the external APIs
which would be expected of a standard "multi-mode serial (SIO) driver" and
adds certain extensions which are needed to address adequately the requirements
of the hot-plugging USB environment.

USB mice are described as part of the USB "human interface device" class
specification and related documents.  This driver concerns itself only with USB
devices which claim to be mouses as set forth in the USB HID specification
and ignores other types of human interface devices (i.e., keyboard).  USB
mice can operate according to either a "boot protocol" or to a "report
protocol".  This driver enables mouses for operation using the boot
protocol.

Unlike most SIO drivers, the number of channels supported by this driver is not
fixed.	Rather, USB mice may be added or removed from the system at any
time.  This creates a situation in which the number of channels is dynamic, and
clients of usbMouseLib.c need to be made aware of the appearance and 
disappearance of channels.  Therefore, this driver adds an additional set of
functions which allows clients to register for notification upon the insertion
and removal of USB mice, and hence the creation and deletion of channels.

This module itself is a client of the Universal Serial Bus Driver (USBD).  All
interaction with the USB buses and devices is handled through the USBD.


INITIALIZATION

As with standard SIO drivers, this driver must be initialized by calling
usbMouseDevInit().  usbMouseDevInit() in turn initializes its 
connection to the USBD and other internal resources needed for operation.  
Unlike some SIO drivers, there are no usbMouseLib.c data structures which need 
to be initialized prior to calling usbMouseDevInit().

Prior to calling usbMouseDevInit(), the caller must ensure that the USBD
has been properly initialized by calling - at a minimum - usbdInitialize().
It is also the caller's responsibility to ensure that at least one USB HCD
(USB Host Controller Driver) is attached to the USBD - using the USBD function
usbdHcdAttach() - before mouse operation can begin.  However, it is not 
necessary for usbdHcdAttach() to be alled prior to initializating usbMouseLib.c.
usbMouseLib.c uses the USBD dynamic attach services and is capable of 
recognizing USB keboard attachment and removal on the fly.  Therefore, it is 
possible for USB HCDs to be attached to or detached from the USBD at run time
- as may be required, for example, in systems supporting hot swapping of
hardware.

usbMouseLib.c does not export entry points for transmit, receive, and error
interrupt entry points like traditional SIO drivers.  All "interrupt" driven
behavior is managed by the underlying USBD and USB HCD(s), so there is no
need for a caller (or BSP) to connect interrupts on behalf of usbMouseLib.c.
For the same reason, there is no post-interrupt-connect initialization code
and usbKeboardLib.c therefore also omits the "devInit2" entry point.


OTHER FUNCTIONS

usbMouseLib.c also supports the SIO ioctl interface.  However, attempts to
set parameters like baud rates and start/stop bits have no meaning in the USB
environment and will be treated as no-ops.  


DATA FLOW

For each USB mouse connected to the system, usbMouseLib.c sets up a
USB pipe to monitor input from the mouse.  usbMouseLib.c supports only the
SIO "interrupt" mode of operation.  In this mode, the application must install 
a "report callback" through the driver's callbackInstall() function.  This
callback is of the form:

\cs
typedef STATUS (*REPORT_CALLBACK)
    (
    void *arg,
    pHID_MSE_BOOT_REPORT pReport
    );
\ce

usbMouseLib.c will invoke this callback for each report received.  The STATUS
returned by the callback is ignored by usbMouseLib.c.  If the application is
unable to accept a report, the report is discarded.  The report structure is
defined in usbHid.h, which is included automatically by usbMouseLib.h.

usbMouseLib.c does not support output to the mouse.  Therefore, calls to
the txStartup() and pollOutput() functions will fail.  

INCLUDE FILES: sioLib.h, usbMouseLib.h
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
#include "usb/usbListLib.h"	/* linked list functions */
#include "usb/usbdLib.h"	/* USBD interface */
#include "usb/usbLib.h" 	/* USB utility functions */
#include "usb/usbHid.h" 	/* USB HID definitions */
#include "drv/usb/usbMouseLib.h"    /* our API */


/* defines */

#define MSE_CLIENT_NAME "usbMouseLib"	    /* our USBD client name */


/* If your hardware platform has problems sharing cache lines, then define
 * CACHE_LINE_SIZE below so that critical buffers can be allocated within
 * their own cache lines.
 */

#define CACHE_LINE_SIZE     16


/* typedefs */

/*
 * ATTACH_REQUEST
 */

typedef struct attach_request
    {
    LINK reqLink;	    /* linked list of requests */
    USB_MSE_ATTACH_CALLBACK callback;	/* client callback routine */
    pVOID callbackArg;		/* client callback argument */
    } ATTACH_REQUEST, *pATTACH_REQUEST;


/* USB_MSE_SIO_CHAN is the internal data structure we use to track each USB
 * mouse.
 */

typedef struct usb_mse_sio_chan
    {
    SIO_CHAN sioChan;		/* must be first field */

    LINK sioLink;	    /* linked list of mouse structs */

    UINT16 lockCount;		/* Count of times structure locked */

    USBD_NODE_ID nodeId;	/* mouse node Id */
    UINT16 configuration;	/* configuration/interface reported as */
    UINT16 interface;		/* a mouse by this device */

    BOOL connected;	    /* TRUE if mouse currently connected */

    UINT16 intMaxPacketSize;	    /* max pkt size for interrupt pipe */
    USBD_PIPE_HANDLE pipeHandle;    /* USBD pipe handle for interrupt pipe */
    USB_IRP irp;	    /* IRP to monitor interrupt pipe */
    BOOL irpInUse;	    /* TRUE while IRP is outstanding */
    pUINT8 pIrpBfr;	    /* bfr for boot report */
    pHID_MSE_BOOT_REPORT pReport;   /* points to pIrpBfr */

    int mode;		    /* SIO_MODE_INT or SIO_MODE_POLL */

    STATUS (*getTxCharCallback) (void *,...); /* tx callback */
    void *getTxCharArg; 	/* tx callback argument */

    STATUS (*putRxCharCallback) (void *, ...); /* rx callback */
    void *putRxCharArg; 	/* rx callback argument */

    STATUS (*putReportCallback) (void *,...); /* report callback */
    void *putReportArg; 	/* report callback argument */

    } USB_MSE_SIO_CHAN, *pUSB_MSE_SIO_CHAN;


/* forward static declarations */

LOCAL int usbMouseTxStartup (SIO_CHAN * pSioChan);
LOCAL int usbMouseCallbackInstall (SIO_CHAN *pSioChan, int callbackType,
    STATUS (*callback)(void *, ...), void *callbackArg);
LOCAL int usbMousePollOutput (SIO_CHAN *pSioChan, char	outChar);
LOCAL int usbMousePollInput (SIO_CHAN *pSioChan, char *thisChar);
LOCAL int usbMouseIoctl (SIO_CHAN *pSioChan, int request, void *arg);

LOCAL VOID usbMouseIrpCallback (pVOID p);


/* locals */

LOCAL UINT16 initCount = 0;	/* Count of init nesting */

LOCAL MUTEX_HANDLE mseMutex;	    /* mutex used to protect internal structs */

LOCAL LIST_HEAD sioList;	/* linked list of USB_MSE_SIO_CHAN */
LOCAL LIST_HEAD reqList;	/* Attach callback request list */

LOCAL USBD_CLIENT_HANDLE usbdHandle; /* our USBD client handle */


/* Channel function table. */

LOCAL SIO_DRV_FUNCS usbMouseSioDrvFuncs =
    {
    usbMouseIoctl,
    usbMouseTxStartup,
    usbMouseCallbackInstall,
    usbMousePollInput,
    usbMousePollOutput
    };


/***************************************************************************
*
* interpMseReport - interprets USB mouse BOOT report
*
* Interprets a mouse boot report and updates channel state as
* appropriate.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID interpMseReport
    (
    pUSB_MSE_SIO_CHAN pSioChan
    )

    {
    pHID_MSE_BOOT_REPORT pReport = pSioChan->pReport;


    /* Validate report */

    if (pSioChan->irp.bfrList [0].actLen >= sizeof (HID_MSE_BOOT_REPORT))
    {
    /* invoke receive callback */

    if (pSioChan->putReportCallback != NULL)
	{
	(*pSioChan->putReportCallback) (pSioChan->putReportArg, pReport);
	}
    }
    }


/***************************************************************************
*
* usbMouseIoctl - special device control
*
* This routine is largely a no-op for the usbMouseLib.	The only ioctls
* which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
*
* RETURNS: OK on success, ENOSYS on unsupported request, EIO on failed
* request.
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbMouseIoctl
    (
    SIO_CHAN *pChan,	    /* device to control */
    int request,	/* request code */
    void *someArg	/* some argument */
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan = (pUSB_MSE_SIO_CHAN) pChan;
    int arg = (int) someArg;


    switch (request)
    {
    case SIO_MODE_SET:

	/* Set driver operating mode: must be interrupt */

	if (arg != SIO_MODE_INT)
	return EIO;

	pSioChan->mode = arg;
	return OK;


    case SIO_MODE_GET:

	/* Return current driver operating mode for channel */

	*((int *) arg) = pSioChan->mode;
	return OK;


    case SIO_AVAIL_MODES_GET:

	/* Return modes supported by driver. */

	*((int *) arg) = SIO_MODE_INT;
	return OK;


    case SIO_OPEN:

	/* Channel is always open. */

	return OK;


    case SIO_BAUD_SET:
    case SIO_BAUD_GET:
    case SIO_HW_OPTS_SET:   /* optional, not supported */
    case SIO_HW_OPTS_GET:   /* optional, not supported */
    case SIO_HUP:	/* hang up is not supported */
    default:	    /* unknown/unsupported command. */

	return ENOSYS;
    }
    }


/***************************************************************************
*
* usbMouseTxStartup - start the interrupt transmitter
*
* The USB mouse SIO driver does not support output to the mouse.
*
* RETURNS: EIO
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbMouseTxStartup
    (
    SIO_CHAN *pChan	/* channel to start */
    )

    {
    return EIO;
    }


/***************************************************************************
*
* usbMouseCallbackInstall - install ISR callbacks to get/put chars
*
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.=
*
* RETURNS: OK on success, or ENOSYS for an unsupported callback type.
*
* ERRNO: none
*
*\NOMANUAL
*/ 

LOCAL int usbMouseCallbackInstall
    (
    SIO_CHAN *pChan,	    /* channel */
    int callbackType,	    /* type of callback */
    STATUS (*callback) (void *, ...),  /* callback */
    void *callbackArg	    /* parameter to callback */
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan = (pUSB_MSE_SIO_CHAN) pChan;

    switch (callbackType)
    {
    case SIO_CALLBACK_GET_TX_CHAR:
	pSioChan->getTxCharCallback = callback;
	pSioChan->getTxCharArg = callbackArg;
	return OK;

    case SIO_CALLBACK_PUT_RCV_CHAR:
	pSioChan->putRxCharCallback = callback;
	pSioChan->putRxCharArg = callbackArg;
	return OK;

    case SIO_CALLBACK_PUT_MOUSE_REPORT:
	pSioChan->putReportCallback = callback;
	pSioChan->putReportArg = callbackArg;
	return OK;

    default:
	return ENOSYS;
    }
    }


/***************************************************************************
*
* usbMousePollOutput - output a character in polled mode
*
* The USB mouse SIO driver does not support output to the mouse.
*
* RETURNS: EIO
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbMousePollOutput
    (
    SIO_CHAN *pChan,
    char outChar
    )

    {
    return EIO;
    }


/***************************************************************************
*
* usbMousePollInput - poll the device for input
*
* This function polls the mouse device for input
*
* RETURNS: ENOSYS
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbMousePollInput
    (
    SIO_CHAN *pChan,
    char *thisChar
    )

    {
    return ENOSYS;
    }


/***************************************************************************
*
* initMseIrp - Initialize IRP to listen for input on interrupt pipe
*
* This function initializes the IRP to listen tn interrupt pipe for input
*
* RETURNS: TRUE if able to submit IRP successfully, else FALSE
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL BOOL initMseIrp
    (
    pUSB_MSE_SIO_CHAN pSioChan
    )

    {
    pUSB_IRP pIrp = &pSioChan->irp;

    /* Initialize IRP */

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pSioChan;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = usbMouseIrpCallback;
    pIrp->timeout = USB_TIMEOUT_NONE;
    pIrp->transferLen = sizeof (HID_MSE_BOOT_REPORT);

    pIrp->bfrCount = 1;
    pIrp->bfrList [0].pid = USB_PID_IN;
    pIrp->bfrList [0].pBfr = pSioChan->pIrpBfr;
    pIrp->bfrList [0].bfrLen = 
    min (pSioChan->intMaxPacketSize, HID_BOOT_REPORT_MAX_LEN);


    /* Submit IRP */

    if (usbdTransfer (usbdHandle, pSioChan->pipeHandle, pIrp) != OK)
    return FALSE;

    pSioChan->irpInUse = TRUE;


    return TRUE;
    }


/***************************************************************************
*
* usbMouseIrpCallback - Invoked upon IRP completion/cancellation
*
* Examines the cause of the IRP completion.  If completion was successful,
* interprets the USB mouse's boot report and re-submits the IRP.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbMouseIrpCallback
    (
    pVOID p	    /* completed IRP */
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;
    pUSB_MSE_SIO_CHAN pSioChan = pIrp->userPtr;


    OSS_MUTEX_TAKE (mseMutex, OSS_BLOCK);

    /* Was the IRP successful? */

    if (pIrp->result == OK)
    {
    /* Interpret the mouse report */

    interpMseReport (pSioChan);
    }


    /* Re-submit the IRP unless it was canceled - which would happen only
     * during pipe shutdown (e.g., the disappearance of the device).
     */

    pSioChan->irpInUse = FALSE;

    if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
        { 
	initMseIrp (pSioChan);
	}
    else
	{
        if (!pSioChan->connected)
            {
            /* Release structure. */
            if (pSioChan->pIrpBfr != NULL)
	    OSS_FREE (pSioChan->pIrpBfr);

            OSS_FREE (pSioChan);
            }
        }

    OSS_MUTEX_RELEASE (mseMutex);
    }


/***************************************************************************
*
* configureSioChan - configure USB mouse for operation
*
* Selects the configuration/interface specified in the <pSioChan>
* structure.  These values come from the USBD dynamic attach callback,
* which in turn retrieved them from the configuration/interface
* descriptors which reported the device to be a mouse.
*
* RETURNS: TRUE if successful, else FALSE if failed to configure channel
*
* ERRNO: none
*
*\NOMANUALS
*/

LOCAL BOOL configureSioChan
    (
    pUSB_MSE_SIO_CHAN pSioChan
    )

    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUSB_INTERFACE_DESCR pIfDescr;
    pUSB_ENDPOINT_DESCR pEpDescr;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    UINT16 actLen;
    UINT16 ifNo;


    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
	return FALSE;

    /* Read the configuration descriptor to get the configuration selection
     * value and to determine the device's power requirements.
     */

    if (usbdDescriptorGet (usbdHandle, 
			   pSioChan->nodeId, 
			   USB_RT_STANDARD | USB_RT_DEVICE, 
			   USB_DESCR_CONFIGURATION, 
			   0, 
			   0, 
			   USB_MAX_DESCR_LEN,
			   pBfr, 
			   &actLen) 
			!= OK)
	{
	OSS_FREE (pBfr);
    	return FALSE;
	}

    if ((pCfgDescr = usbDescrParse (pBfr, 
				    actLen, 
				    USB_DESCR_CONFIGURATION)) 
    				== NULL)
        {
        OSS_FREE (pBfr);
        return FALSE;
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
        return FALSE;
        }



    /* Retrieve the endpoint descriptor following the identified interface
     * descriptor.
     */

    if ((pEpDescr = usbDescrParseSkip (&pScratchBfr, 
				       &actLen, 
				       USB_DESCR_ENDPOINT))
    				== NULL)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Select the configuration. */

    if (usbdConfigurationSet (usbdHandle, 
			      pSioChan->nodeId, 
			      pCfgDescr->configurationValue, 
			      pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) 
			   != OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Select interface 
     * 
     * NOTE: Some devices may reject this command, and this does not represent
     * a fatal error.  Therefore, we ignore the return status.
     */

    usbdInterfaceSet (usbdHandle, 
		      pSioChan->nodeId, 
		      pSioChan->interface, 
		      pIfDescr->alternateSetting);


    /* Select the mouse boot protocol. */

    if (usbHidProtocolSet (usbdHandle, 
			   pSioChan->nodeId, 
			   pSioChan->interface, 
			   USB_HID_PROTOCOL_BOOT) 
			!= OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Set the mouse idle time to infinite.
     * Note, some Mice (ie: Microsoft, Dell) don't recognize 
     * the command to set the Idle time, so ignore the result 
     */
    usbHidIdleSet (usbdHandle, 
		   pSioChan->nodeId, 
		   pSioChan->interface, 
		   0 /* no report ID */, 
		   0 /* infinite */);
 


    /* Create a pipe to monitor input reports from the mouse. */

    pSioChan->intMaxPacketSize = *((pUINT8) &pEpDescr->maxPacketSize) |
    			(*(((pUINT8) &pEpDescr->maxPacketSize) + 1) << 8);

    if (usbdPipeCreate (usbdHandle, 
			pSioChan->nodeId, 
			pEpDescr->endpointAddress, 
			pCfgDescr->configurationValue, 
			pSioChan->interface, 
			USB_XFRTYPE_INTERRUPT, 
			USB_DIR_IN, 
			pSioChan->intMaxPacketSize, 
			sizeof (HID_MSE_BOOT_REPORT), 
			pEpDescr->interval, 
			&pSioChan->pipeHandle) 
		    != OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Initiate IRP to listen for input on interrupt pipe */

    if (!initMseIrp (pSioChan))
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    OSS_FREE (pBfr);

    return TRUE;

    }


/***************************************************************************
*
* destroyAttachRequest - disposes of an ATTACH_REQUEST structure
*
* This function disposes of an ATTACH_REQUEST structure
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID destroyAttachRequest
    (
    pATTACH_REQUEST pRequest
    )

    {
    /* Unlink request */

    usbListUnlinkProt (&pRequest->reqLink,mseMutex);

    /* Dispose of structure */

    OSS_FREE (pRequest);
    }


/***************************************************************************
*
* destroySioChan - disposes of a USB_MSE_SIO_CHAN structure
*
* Unlinks the indicated USB_MSE_SIO_CHAN structure and de-allocates
* resources associated with the channel.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID destroySioChan
    (
    pUSB_MSE_SIO_CHAN pSioChan
    )

    {
    /* Unlink the structure. */

    usbListUnlinkProt (&pSioChan->sioLink,mseMutex);


    /* Release pipe if one has been allocated.	Wait for the IRP to be
     * cancelled if necessary.
     */

    if (pSioChan->pipeHandle != NULL)
    usbdPipeDestroy (usbdHandle, pSioChan->pipeHandle);

    /* The following block is commented out to address the nonblocking 
     * issue of destroySioChan when the mouse is removed and a read is 
     * in progress SPR #98731 */ 
    
    /*
    OSS_MUTEX_RELEASE (mseMutex);

    while (pSioChan->irpInUse)
    OSS_THREAD_SLEEP (1);

    OSS_MUTEX_TAKE(mseMutex, OSS_BLOCK);
    */

    /* Release structure. */
    if (!pSioChan->irpInUse)
        {
        if (pSioChan->pIrpBfr != NULL)
        OSS_FREE (pSioChan->pIrpBfr);

        OSS_FREE (pSioChan);
        }

    }

/***************************************************************************
*
* createSioChan - creates a new USB_MSE_SIO_CHAN structure
*
* Creates a new USB_MSE_SIO_CHAN structure for the indicated <nodeId>.
* If successful, the new structure is linked into the sioList upon 
* return.
*
* <configuration> and <interface> identify the configuration/interface
* that first reported itself as a mouse for this device.
*
* RETURNS: pointer to newly created structure, or NULL if failure
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_MSE_SIO_CHAN createSioChan
    (
    USBD_NODE_ID nodeId,
    UINT16 configuration,
    UINT16 interface
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan;


    /* Try to allocate space for a new mouse struct */

    if ((pSioChan = OSS_CALLOC (sizeof (*pSioChan))) == NULL)
    return NULL;

    if ((pSioChan->pIrpBfr = OSS_MALLOC (HID_BOOT_REPORT_MAX_LEN)) == NULL)
    	{
    	OSS_FREE (pSioChan);
    	return NULL;
        }

    pSioChan->sioChan.pDrvFuncs = &usbMouseSioDrvFuncs;
    pSioChan->nodeId = nodeId;
    pSioChan->connected = TRUE;
    pSioChan->mode = SIO_MODE_INT;

    pSioChan->configuration = configuration;
    pSioChan->interface = interface;


    pSioChan->pReport = (pHID_MSE_BOOT_REPORT) pSioChan->pIrpBfr;

    /* Try to configure the mouse. */

    if (!configureSioChan (pSioChan))
    {
    destroySioChan (pSioChan);
    return NULL;
    }


    /* Link the newly created structure. */

    usbListLinkProt (&sioList, pSioChan, &pSioChan->sioLink, LINK_TAIL,mseMutex);

    return pSioChan;
    }


/***************************************************************************
*
* findSioChan - Searches for a USB_MSE_SIO_CHAN for indicated node ID
*
* This function searches for a USB_MSE_SIO_CHAN for indicated <nodeId>
*
* RETURNS: pointer to matching USB_MSE_SIO_CHAN or NULL if not found
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_MSE_SIO_CHAN findSioChan
    (
    USBD_NODE_ID nodeId
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan = usbListFirst (&sioList);

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
* This function notifies the attachment/removal of device to registered client
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    pUSB_MSE_SIO_CHAN pSioChan,
    UINT16 attachCode
    )

    {
    pATTACH_REQUEST pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
    {
    (*pRequest->callback) (pRequest->callbackArg, 
	(SIO_CHAN *) pSioChan, attachCode);

    pRequest = usbListNext (&pRequest->reqLink);
    }
    }


/***************************************************************************
*
* usbMouseAttachCallback - called by USBD when mouse attached/removed
*
* The USBD will invoke this callback when a USB mouse is attached to or
* removed from the system.  <nodeId> is the USBD_NODE_ID of the node being
* attached or removed.	<attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* Mice generally report their class information at the interface level,
* so <configuration> and <interface> will indicate the configuratin/interface
* that reports itself as a mouse.  Finally, <deviceClass>, <deviceSubClass>,
* and <deviceProtocol> will identify a HID/BOOT/KEYBOARD device.
*
* NOTE: The USBD will invoke this function once for each configuration/
* interface which reports itself as a mouse.  So, it is possible that
* a single device insertion/removal may trigger multiple callbacks.  We
* ignore all callbacks except the first for a given device.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbMouseAttachCallback
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
    pUSB_MSE_SIO_CHAN pSioChan;


    OSS_MUTEX_TAKE (mseMutex, OSS_BLOCK);

    /* Depending on the attach code, add a new mouse or disabled one
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

	if ((pSioChan = createSioChan (nodeId, configuration, interface)) 
	== NULL)
	break;

	/* Notify registered callers that a new mouse has been
	 * added and a new channel created.
	 */

	notifyAttach (pSioChan, USB_MSE_ATTACH);

	break;


    case USBD_DYNA_REMOVE:

	/* A device is being detached.	Check if we have any
	 * structures to manage this device.
	 */

	if ((pSioChan = findSioChan (nodeId)) == NULL)
	break;

	/* The device has been disconnected. */

	pSioChan->connected = FALSE;

	/* Notify registered callers that the mouse has been
	 * removed and the channel disabled. 
	 *
	 * NOTE: We temporarily increment the channel's lock count
	 * to prevent usbMouseSioChanUnlock() from destroying the
	 * structure while we're still using it.
	 */

	pSioChan->lockCount++;

	notifyAttach (pSioChan, USB_MSE_REMOVE);

	pSioChan->lockCount--;

	/* If no callers have the channel structure locked, destroy
	 * it now.  If it is locked, it will be destroyed later during
	 * a call to usbMouseUnlock().
	 */

	if (pSioChan->lockCount == 0)
	destroySioChan (pSioChan);

	break;
    }

    OSS_MUTEX_RELEASE (mseMutex);
    }


/***************************************************************************
*
* doShutdown - shuts down USB mouse SIO driver
*
* <errCode> should be OK or S_usbMouseLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL STATUS doShutdown
    (
    int errCode
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_MSE_SIO_CHAN pSioChan;


    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
    destroyAttachRequest (pRequest);


    /* Dispose of any open mouse connections. */

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

    if (mseMutex != NULL)
    {
    OSS_MUTEX_DESTROY (mseMutex);
    mseMutex = NULL;
    }


    return ossStatus (errCode);
    }


/***************************************************************************
*
* usbMouseDevInit - initialize USB mouse SIO driver
*
* Initializes the USB mouse SIO driver.  The USB mouse SIO driver
* maintains an initialization count, so calls to this function may be
* nested.
*
* RETURNS: OK, or ERROR if unable to initialize.
*
* ERRNO:
* \is
* \i S_usbMouseLib_OUT_OF_RESOURCES
* Sufficient Resources are not available
*
* \i S_usbMouseLib_USBD_FAULT
* Error in USBD Layer
* \ie
*/

STATUS usbMouseDevInit (void)
    {
    /* If not already initialized, then initialize internal structures
     * and connection to USBD.
     */

    if (initCount == 0)
    {
    /* Initialize lists, structures, resources. */

    memset (&sioList, 0, sizeof (sioList));
    memset (&reqList, 0, sizeof (reqList));
    mseMutex = NULL;
    usbdHandle = NULL;


    if (OSS_MUTEX_CREATE (&mseMutex) != OK)
	return doShutdown (S_usbMouseLib_OUT_OF_RESOURCES);


    /* Establish connection to USBD */

    if (usbdClientRegister (MSE_CLIENT_NAME, &usbdHandle) != OK ||
	usbdDynamicAttachRegister (usbdHandle, USB_CLASS_HID,
	USB_SUBCLASS_HID_BOOT, USB_PROTOCOL_HID_BOOT_MOUSE,
	usbMouseAttachCallback) != OK)
	{
	return doShutdown (S_usbMouseLib_USBD_FAULT);
	}
    }

    initCount++;

    return OK;
    }


/***************************************************************************
*
* usbMouseDevShutdown - shuts down mouse SIO driver
*
* This function shutdowns the mouse SIO driver. If after decrementing 
* <initCount> is 0, SIO driver is uninitialized.
*
* RETURNS: OK, or ERROR if unable to shutdown.
*
* ERRNO:
* \is
* \i S_usbMouseLib_NOT_INITIALIZED
* SIO Driver is not initialized
* \ie
*/

STATUS usbMouseDevShutdown (void)
    {
    /* Shut down the USB mouse SIO driver if the initCount goes to 0. */

    if (initCount == 0)
    return ossStatus (S_usbMouseLib_NOT_INITIALIZED);

    if (--initCount == 0)
    return doShutdown (OK);

    return OK;
    }


/***************************************************************************
*
* usbMouseDynamicAttachRegister - Register mouse attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_MSE_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     SIO_CHAN *pSioChan,
*     UINT16 attachCode
*     );
* \ce
*
* usbMouseLib will invoke <callback> each time a USB mouse
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also be passed a pointer to the 
* SIO_CHAN structure for the channel being created/destroyed and
* an attach code of USB_MSE_ATTACH or USB_MSE_REMOVE.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbMouseLib_BAD_PARAM
* Bad Parameter is passed
*
* \i S_usbMouseLib_OUT_OF_MEMORY
* Not sufficient memory available
* \ie
*/

STATUS usbMouseDynamicAttachRegister
    (
    USB_MSE_ATTACH_CALLBACK callback,	/* new callback to be registered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_MSE_SIO_CHAN pSioChan;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
    return ossStatus (S_usbMouseLib_BAD_PARAM);


    OSS_MUTEX_TAKE (mseMutex, OSS_BLOCK);


    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
    status = S_usbMouseLib_OUT_OF_MEMORY;
    else
    {
    pRequest->callback = callback;
    pRequest->callbackArg = arg;

    usbListLinkProt (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL,mseMutex);

    
    /* Perform an initial notification of all currrently attached
     * mouse devices.
     */

    pSioChan = usbListFirst (&sioList);

    while (pSioChan != NULL)
	{
	if (pSioChan->connected)
	(*callback) (arg, (SIO_CHAN *) pSioChan, USB_MSE_ATTACH);

	pSioChan = usbListNext (&pSioChan->sioLink);
	}
    }

    OSS_MUTEX_RELEASE (mseMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbMouseDynamicAttachUnregister - Unregisters mouse attach callback
*
* This function cancels a previous request to be dynamically notified for
* mouse attachment and removal.  The <callback> and <arg> paramters must
* exactly match those passed in a previous call to 
* usbMouseDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister callback
*
* ERRNO:
* \is
* \i S_usbMouseLib_NOT_REGISTERED
* Could not register the callback
* \ie
*/

STATUS usbMouseDynamicAttachUnRegister
    (
    USB_MSE_ATTACH_CALLBACK callback,	/* callback to be unregistered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    int status = S_usbMouseLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (mseMutex, OSS_BLOCK);

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

    OSS_MUTEX_RELEASE (mseMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbMouseSioChanLock - Marks SIO_CHAN structure as in use
*
* A caller uses usbMouseSioChanLock() to notify usbMouseLib that
* it is using the indicated SIO_CHAN structure.  usbMouseLib maintains
* a count of callers using a particular SIO_CHAN structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* USB mouse is removed from the system.  So long as the "lock count"
* is greater than zero, usbMouseLib will not dispose of an SIO_CHAN
* structure.
*
* RETURNS: OK
*
* ERRNO: none
*/

STATUS usbMouseSioChanLock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as in use */
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan = (pUSB_MSE_SIO_CHAN) pChan;
    pSioChan->lockCount++;

    return OK;
    }


/***************************************************************************
*
* usbMouseSioChanUnlock - Marks SIO_CHAN structure as unused
*
* This function releases a lock placed on an SIO_CHAN structure.  When a
* caller no longer needs an SIO_CHAN structure for which it has previously
* called usbMouseSioChanLock(), then it should call this function to
* release the lock.
*
* NOTE: If the underlying USB mouse device has already been removed
* from the system, then this function will automatically dispose of the
* SIO_CHAN structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the SIO_CHAN again structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure unused
*
* ERRNO:
* \is
* \i S_usbMouseLib_NOT_LOCKED
* No lock to unlock
* \ie
*/

STATUS usbMouseSioChanUnlock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as unused */
    )

    {
    pUSB_MSE_SIO_CHAN pSioChan = (pUSB_MSE_SIO_CHAN) pChan;
    int status = OK;


    OSS_MUTEX_TAKE (mseMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0)
    status = S_usbMouseLib_NOT_LOCKED;
    else
    {
    /* If this is the last lock and the underlying USB mouse is
     * no longer connected, then dispose of the mouse.
     */

    if (--pSioChan->lockCount == 0 && !pSioChan->connected)
	destroySioChan (pSioChan);
    }

    OSS_MUTEX_RELEASE (mseMutex);

    return ossStatus (status);
    }



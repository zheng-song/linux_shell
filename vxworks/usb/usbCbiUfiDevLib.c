/* usbCbiUfiDevLib.c - USB CBI Mass Storage class driver for UFI sub-class */

/* Copyright 2000-2002 Wind River Systems, Inc. */

/*
modification history
--------------------
01r,26oct04,ami  Debug Message Changes
01q,21oct04,pdg  IRP timeout handling
01p,15oct04,ami  Apigen Changes
01o,07oct04,ami  SPR #91732 Fix
01n,07oct04,mta  SPR 94016:If device unplugged during a transfer, a corruption
                 (pagefault) may appear
01m,07oct04,mta  Merge from Integration branch
01l,06oct04,mta  SPR 97415:when addressing an endpoint, we omit the direction
                 bit
01k,06oct04,ami  SPR #94684 Fix
01j,26feb04,cfc  Merge fixes for SPR 93986 and 94016 from adh
01i,19sep03,cfc  Fix for creating USB Interrupt Pipe w/USB2 Host Stack
01h,30apr02,wef  fix little endian swap problem in usbCbiUfiPhysDevCreate
01g,05mar02,wef  fix bug - sizeof a macro
01f,19nov01,dee  fix continuation line syntax error
01e,06nov01,wef  Remove automatic buffer creations and repalce with OSS_xALLOC
                 remove more warnings, fix condition where no disk is in drive 
		 - SPR #69753.
01d,08aug01,dat  Removing warnings
01c,02sep00,bri  added support for multiple devices.
01b,04aug00,bri  updated as per review.
01a,26jun00,bri	 written.
*/

/*
DESCRIPTION

This module implements the USB Mass Storage class driver for the vxWorks 
operating system.  This module presents an interface which is a superset 
of the vxWorks Block Device driver model.  The driver implements external 
APIs which would be expected of a standard block device driver. 

This class driver restricts to Mass Storage class devices with UFI subclass,
that follow CBI (Control/Bulk/Interrupt) transport.  For CBI devices 
transport of command, data and status occurs via control, bulk and interrupt 
endpoints respectively. Interrupt endpoint is used to signal command completion.

The class driver is a client of the Universal Serial Bus Driver (USBD).  All
interaction with the USB buses and devices is handled through the USBD.

INITIALISATION

The driver initialisation routine usbCbiUfiDevInit() must be invoked first 
prior to any other driver routines.  It is assumed that USBD is already 
initialised and attached to atleast one USB Host Controller.  
usbCbiUfiDevInit() registers the class driver as a client module with USBD.  It 
also registers a callback routine to get notified whenever a USB MSC/UFI/CBI 
device is attached or removed from the system.  The callback routine
creates a USB_CBI_UFI_DEV structure to represent the USB device attached.  It
also sets device configuration, interface settings and creates pipes for 
BULK_IN, BULK_OUT and INTERRUPT transfers.

DATA FLOW

For every USB/CBI/UFI device detected, the device configuration is set to the
configuration that follows the CBI/UFI command set.  Pipes are created for bulk
in, bulk out and interrupt endpoints.  To initiate transactions, ADSC class 
specific request is used to send a command block on the control endpoint.  
Command blocks are formed as per the UFI command specifications.  If the 
command requires transport of data to/from the device, it is done via 
bulk-out/bulk-in pipes using IRPs.  This is followed by status transport via 
interrupt endpoint.


OTHER FUNCTIONS

Number of USB CBI_UFI devices supported by this driver is not fixed.  UFI 
devices may be added or removed from the USB system at any point of time.  The 
user of this client driver must be aware of the device attachment and removal.
To facilitate this, an user-specific callback routine may be registered, using
usbCbiUfiDynamicAttachRegister() routine. The USBD_NODE_ID assigned to the 
device being attached or removed, is passed on to the user callback routine. 
This unique ID may be used to create a block device using 
usbCbiUfiBlkDevCreate() and further launch file system.

NOTE : The user callback routine is invoked from the USBD client task created 
for this class driver. The callback routine should not invoke any class driver
function, which will further submit IRPs. For example,  usbCbiUfiBlkDevCreate() 
should not be invoked from the user's callback. 

Typically, the user may create a task, as a client of UFI driver, and invoke
the driver routines from the task's context.  The user callback routine may
be used to notify device attachment and removal to the task.
 

INCLUDE FILES: usbCbiUfiDevLib.h, blkIo.h

SEE ALSO:
  '<USB Mass Storage Class - Control/Bulk/Interrupt Transport Specification Revision 1.0>'
  '<USB Mass Storage Class - UFI Command specification Revision 1.0>'

*/

/* includes */

#include "vxWorks.h"
#include "string.h"
#include "errno.h"
#include "errnoLib.h"
#include "ioLib.h"
#include "blkIo.h"
#include "stdio.h"
#include "logLib.h"
#include "taskLib.h"
#include "drv/timer/timerDev.h"

#include "usb/usbPlatform.h"
#include "usb/ossLib.h"             /* operations system srvcs */
#include "usb/usb.h"                /* general USB definitions */
#include "usb/usbListLib.h"         /* linked list functions   */
#include "usb/usbdLib.h"            /* USBD interface          */
#include "usb/usbLib.h"             /* USB utility functions   */

#include "drv/usb/usbCbiUfiDevLib.h"


/* defines */

#define USB_DEBUG_MSG        0x01
#define USB_DEBUG_ERR        0x02

extern void usbLogMsg (char *,int,int,int,int,int,int);

#define USB_CBI_UFI_DEBUG                  \
        if (usbCbiUfiDebug & USB_DEBUG_MSG)   \
            usbLogMsg

#define USB_CBI_UFI_ERR                    \
        if (usbCbiUfiDebug & USB_DEBUG_ERR)   \
            usbLogMsg

#define CBI_TEST_READY_NOCHANGE  0   /* no media change detected           */
#define CBI_TEST_READY_CHANGED   1   /* media change detected              */
#define CBI_TEST_READY_FAILED    2   /* failed to checked ready status     */
#define CBI_TEST_READY_MXRETRIES 200 /* Retry count for testing unit ready */


/* typedefs */

typedef struct usbCbiUfiDev
    {
    BLK_DEV           blkDev;         /* Vxworks block device structure */
                                      /* Must be the first one          */
    USBD_NODE_ID      cbiUfiDevId;    /* USBD node ID of the device     */	 
    UINT16            configuration;  /* Configuration value            */    
    UINT16            interface;      /* Interface number               */
    UINT16            altSetting;     /* Alternate setting of interface */ 
    UINT16            outEpAddress;   /* Bulk out EP address            */   
    UINT16            inEpAddress;    /* Bulk in EP address             */
    UINT16            intrEpAddress;  /* Interrupt EP address           */
    USBD_PIPE_HANDLE  outPipeHandle;  /* Pipe handle for Bulk out EP    */
    USBD_PIPE_HANDLE  inPipeHandle;   /* Pipe handle for Bulk in EP     */
    USBD_PIPE_HANDLE  intrPipeHandle; /* Pipe handle for interrupt EP   */
    USB_IRP           inIrp;          /* IRP used for bulk-in data      */
    USB_IRP           outIrp;         /* IRP used for bulk-out data     */
    USB_IRP           statusIrp;      /* IRP used for status data       */   
    USB_UFI_CMD_BLOCK ufiCmdBlk;      /* Store for UFI Command block    */
    UINT8 *           bulkInData;     /* Pointer for bulk-in data       */
    UINT8 *           bulkOutData;    /* Pointer for bulk-out data      */   
    UINT8             intrStatus[2];  /* Store for Status bytes         */
    UINT16            lockCount;      /* Count of times structure locked*/
    UINT16            inEpAddressMaxPkt;  /* Max In Pipe Packet size       */
    UINT16            outEpAddressMaxPkt; /* Max Out Pipe Packet size      */
    BOOL              connected;      /* TRUE if CBI_UFI device connected  */    
    SEM_HANDLE        cbiUfiIrpSem;   /* Semaphore for IRP Synchronisation */
    LINK              cbiUfiDevLink;  /* Link to other USB_CBI_UFI devices */  
    } USB_CBI_UFI_DEV, *pUSB_CBI_UFI_DEV;

/* Attach request for user callback */

typedef struct attach_request
    {
    LINK reqLink;                       /* linked list of requests */
    USB_UFI_ATTACH_CALLBACK callback;   /* client callback routine */
    pVOID callbackArg;                  /* client callback argument*/
    } ATTACH_REQUEST, *pATTACH_REQUEST;

/* globals */

BOOL usbCbiUfiDebug = 0;

/* locals */

LOCAL UINT16 initCount = 0;           /* Count for UFI device initialisation */

LOCAL USBD_CLIENT_HANDLE usbdHandle;  /* Handle for this class driver */

LOCAL LIST_HEAD    cbiUfiDevList;     /* Linked list of USB_CBI_UFI_DEV */

LOCAL LIST_HEAD    reqList;           /* Attach callback request list */

LOCAL MUTEX_HANDLE cbiUfiDevMutex;    /* Mutex used to protect internal structs */



/* forward declarations */

LOCAL  STATUS usbCbiUfiDescShow (USBD_NODE_ID nodeId);
LOCAL  STATUS usbCbiUfiConfigDescShow  (USBD_NODE_ID nodeId, UINT8 index);
LOCAL  pUSB_CBI_UFI_DEV usbCbiUfiPhysDevCreate (USBD_NODE_ID nodeId, 
                                                UINT16 config, 
						UINT16 interface);
LOCAL  pUSB_CBI_UFI_DEV usbCbiUfiDevFind (USBD_NODE_ID nodeId);
LOCAL  STATUS usbCbiUfiDevBlkRd (BLK_DEV *blkDev,
				 UINT32 offset, 
				 UINT32 num, 
				 char * buf);
LOCAL  STATUS usbCbiUfiDevBlkWrt (BLK_DEV *blkDev, 
				  UINT32 offset, 
				  UINT32 num, 
				  char * buf);
LOCAL  STATUS usbCbiUfiDevStChk (BLK_DEV *blkDev);
LOCAL  STATUS usbCbiUfiDevReset (BLK_DEV *blkDev);
LOCAL  VOID   usbCbiUfiIrpCallback (pVOID p);
LOCAL  STATUS usbCbiUfiFormCmd (pUSB_CBI_UFI_DEV pCbiUfiDev, 
				UINT ufiCmd, 
				UINT cmdParam1, 
				UINT cmdParam2);
LOCAL  USB_COMMAND_STATUS usbCbiUfiCmdExecute (pUSB_CBI_UFI_DEV pCbiUfiDev);
LOCAL  VOID usbCbiUfiDevDestroy (pUSB_CBI_UFI_DEV pCbiUfiDev); 
LOCAL  VOID notifyAttach (USBD_NODE_ID nodeId, UINT16 attachCode);
LOCAL STATUS usbCbiUfiDevUnitReady ( BLK_DEV *);
LOCAL UINT8 usbCbiUfiDevTestReady ( BLK_DEV *);
 


/***************************************************************************
*
* usbCbiUfiAttachCallback - called by USBD when UFI device is attached/detached
*
* This function is invoked by the USBD driver when a mass storage device, 
* following CBI protocol and UFI command set, is attached or removed.  
* 
* <nodeId> is the USBD_NODE_ID of the node being attached or removed.	
* <attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* <configuration> and <interface> indicate the configuration/interface
* that reports itself as a MSC/CBI/UFI device.  
* <deviceClass>, <deviceSubClass>, and <deviceProtocol> will identify a 
* MSC/CBI/UFI device.
*
* NOTE: The USBD will invoke this function once for each configuration/
* interface which reports itself as a MSC/CBI/UFI.  So, it is possible 
* that a single device insertion/removal may trigger multiple callbacks. 
*
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbCbiUfiAttachCallback
    (
    USBD_NODE_ID nodeId,        /* USBD Node ID of the device attached       */
    UINT16 attachAction,        /* Whether device attached / detached        */
    UINT16 configuration,       /* Configur'n value for  MSC/CBI/UFI         */
    UINT16 interface,           /* Interface number for  MSC/CBI/UFI         */
    UINT16 deviceClass,         /* Interface class   - 0x8  for MSC          */
    UINT16 deviceSubClass,      /* Device sub-class  - 0x4  for UFI command  */    
    UINT16 deviceProtocol       /* Interfaceprotocol - 0x00 for CBI          */
    )
    {
    pUSB_CBI_UFI_DEV  pCbiUfiDev;  

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);
 
    switch (attachAction)
        { 
        case USBD_DYNA_ATTACH: 

            /* MSC/CBI/UFI Device attached */

            USB_CBI_UFI_DEBUG ("usbCbiUfiAttachCallback : New MSC/CBI/UFI device "\
                               "attached\n", 0, 0, 0, 0, 0, 0);

            /* Check out whether we already have a structure for this device */

            USB_CBI_UFI_DEBUG ("usbCbiUfiAttachCallback: Configuration = %d, " \
                               "Interface = %d, Node Id = %d \n", configuration,
                               interface, (UINT)nodeId, 0, 0, 0); 

            if (usbCbiUfiDevFind (nodeId) != NULL)
                break;

            /* create a USB_CBI_UFI_DEV structure for the device detected */
         
            if ((pCbiUfiDev = usbCbiUfiPhysDevCreate (nodeId,  configuration,  interface)) ==NULL )
                {
                USB_CBI_UFI_ERR ("usbCbiUfiAttachCallback : Error creating " \
                                 "MSC/CBI/UFI device\n", 0, 0, 0, 0, 0, 0);
                break;
                } 
             
            /* Notify registered callers that a CBI_UFI_DEV has been added */

	    notifyAttach (pCbiUfiDev->cbiUfiDevId, USB_UFI_ATTACH); 

            break;

        case USBD_DYNA_REMOVE:

            /* MSC/CBI/UFI Device detached */

            USB_CBI_UFI_DEBUG ("usbCbiUfiAttachCallback : MSC/CBI/UFI Mass storage "\
                               "device detached\n", 0, 0, 0, 0, 0, 0);

            if ((pCbiUfiDev = usbCbiUfiDevFind (nodeId)) == NULL)
                break;

            /* Check the connected flag  */

            if (pCbiUfiDev->connected == FALSE)
                break;
            
            pCbiUfiDev->connected = FALSE;

	    /* Notify registered callers that the CBI_UFI device has been
	     * removed 
	     *
	     * NOTE: We temporarily increment the device's lock count
	     * to prevent usbCbiUfiDevUnlock() from destroying the
	     * structure while we're still using it.
	     */

            pCbiUfiDev->lockCount++; 

            notifyAttach (pCbiUfiDev->cbiUfiDevId, USB_UFI_REMOVE); 

            pCbiUfiDev->lockCount--; 
            
            if (pCbiUfiDev->lockCount == 0) 
                usbCbiUfiDevDestroy (pCbiUfiDev); 

            USB_CBI_UFI_DEBUG ("usbCbiUfiDevAttachCallback : CbiUfi Mass \
			     storage device detached\n", 0, 0, 0, 0, 0, 0);                
            break;

        default :
            break; 
        }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex);  
    }


/***************************************************************************
*
* usbCbiUfiDevShutDown - shuts down the USB CBI mass storage class driver
* 
* This routine unregisters UFI driver from USBD and releases any resources 
* allocated for the devices.
*
* RETURNS: OK or ERROR.
*
* ERRNO:
* \is
* \i S_usbCbiUfiDevLib_NOT_INITIALIZED
* CBI Device is not initialized 
* \ie 
*/

STATUS usbCbiUfiDevShutDown 
    (
    int errCode                  /* Error code - reason for shutdown */
    )
    {

    pUSB_CBI_UFI_DEV pCbiUfiDev;
    pATTACH_REQUEST  pRequest;

    if (initCount == 0)
        return ossStatus (S_usbCbiUfiDevLib_NOT_INITIALIZED);

    /* release any UFI devices */

    while ((pCbiUfiDev = usbListFirst (&cbiUfiDevList)) != NULL)
        usbCbiUfiDevDestroy (pCbiUfiDev); 

    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
        {
      	usbListUnlink (&pRequest->reqLink);
        OSS_FREE (pRequest); 
        }

    /* 
     * Unregister with the USBD. USBD will automatically release any pending
     * IRPs or attach requests.
     */

    if (usbdHandle != NULL)
        {
        usbdClientUnregister (usbdHandle);
        usbdHandle = NULL;
        USB_CBI_UFI_DEBUG ("usbCbiUfiDevShutDown : CBI Mass storage class driver "\
                           "for UFI devices unregistered \n", 0, 0, 0, 0, 0, 0);
        }

    /* release resources */

    if (cbiUfiDevMutex != NULL)
        {
        OSS_MUTEX_DESTROY (cbiUfiDevMutex);
        cbiUfiDevMutex = NULL;
        }

    initCount--; 
   
    return ossStatus (errCode); 
    }



/***************************************************************************
*
* usbCbiUfiDevInit - registers USB CBI mass storage class driver for UFI devices
*
* This routine registers the CBI mass storage class driver for UFI devices.
* It also registers a callback routine to request notification whenever
* USB/MSC/CBI/UFI devices are attached or removed.    
*
* RETURNS: OK, or ERROR if unable to register with USBD.
*
* ERRNO:
* \is
* \i S_usbCbiUfiDevLib_OUT_OF_RESOURCES
* Resouces are not available
*
* \i S_usbCbiUfiDevLib_USBD_FAULT
* USBD Fault has occured
* \ie
*/

STATUS usbCbiUfiDevInit (VOID)
    {

    /* 
     * Check whether already initialised. If not, then initialise the required
     * structures and registers the class driver with USBD.
     */

    if (initCount == 0)
        {

        memset (&cbiUfiDevList, 0, sizeof (cbiUfiDevList));
        memset (&reqList, 0, sizeof (reqList));
        cbiUfiDevMutex = NULL;
        usbdHandle     = NULL;


        if (OSS_MUTEX_CREATE (&cbiUfiDevMutex) != OK)
            return (usbCbiUfiDevShutDown (S_usbCbiUfiDevLib_OUT_OF_RESOURCES));

        /* Establish connection to USBD and register for attach callback */

        if (usbdClientRegister ("CBI_UFI_CLASS", &usbdHandle) != OK ||
            usbdDynamicAttachRegister (usbdHandle, USB_CLASS_MASS_STORAGE,
                                       USB_SUBCLASS_UFI_COMMAND_SET, 
                                       USB_INTERFACE_PROTOCOL_CBI,
                                       usbCbiUfiAttachCallback) != OK)
            {
            USB_CBI_UFI_ERR ("usbCbiUfiDevInit: Client Registration Failed \n",
                             0, 0, 0, 0, 0, 0); 
            return (usbCbiUfiDevShutDown (S_usbCbiUfiDevLib_USBD_FAULT));
            }
        }

    initCount++;

    return (OK);
    }


/***************************************************************************
*
* usbCbiUfiDevIoctl - perform a device-specific control.
*
* Typically called by file system to invoke device-specific functions beyond 
* file handling.  The following control requests are supported
*
* \is
* \i 'FIODISKFORMAT (0x05)'
* Formats the entire disk with appropriate hardware track and sector marks.
* No file system is initialized on the disk by this request.  This control
* function is defined by the file system, but provided by the driver.
*
* \i 'USB UFI ALL DESCRIPTOR GET (0xF0)'
* Invokes show routine for displaying configuration, device and interface
* descriptors.
*
* \i 'USB UFI DEV RESET (0xF1)'
* Issues a command block reset and clears stall condition on bulk-in and
* bulk-out endpoints.
* \ie
*
* RETURNS: The status of the request, or ERROR if the request is unsupported.
*
* ERRNO: none
*/

STATUS usbCbiUfiDevIoctl
    (
    BLK_DEV * pBlkDev,           /* pointer to MSC/CBI/UFI device */
    UINT32 request,	         /* request type                  */
    UINT32 someArg		         /* arguments related to request  */
    )
    {

    /* get a pointer to the MSC/CBI/UFI  device */

    pUSB_CBI_UFI_DEV  pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;   

    if ( pCbiUfiDev == (pUSB_CBI_UFI_DEV)NULL )
        return (ERROR);

    /* Check whether the device exists or not */

    if (usbCbiUfiDevFind (pCbiUfiDev->cbiUfiDevId) != pCbiUfiDev)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevIoctl: MSC/CBI/UFI Device not found\n", 
                      0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    switch (request)
        {

        case FIODISKFORMAT: 

            /*  
             * This is the IO control function supported by file system,
             * but supplied by the device driver. Other IO control functions 
             * are directly handled by file system with out the use of this 
	     * routine.
             */


            if ( usbCbiUfiFormCmd (pCbiUfiDev, 
				   USB_UFI_FORMAT_UNIT, 
				   0, 
				   0) 
				!= OK)
 
                {
                USB_CBI_UFI_ERR ("usbCbiUfiDevIoctl: Error forming command\n",
                                 0, 0, 0, 0, 0, 0);
                return (ERROR); 
                } 

            if ( usbCbiUfiCmdExecute (pCbiUfiDev) != USB_COMMAND_SUCCESS )
                {
                USB_CBI_UFI_ERR ("usbCbiUfiDevIoctl: Error executing "\
                                 "USB_UFI_FORMAT_UNIT command\n", 
                                 0, 0, 0, 0, 0, 0);        
                return (ERROR);
                }

            break;

        case USB_UFI_ALL_DESCRIPTOR_GET:

            /* invoke routine to display all descriptors */

            return (usbCbiUfiDescShow (pCbiUfiDev->cbiUfiDevId));

        case USB_UFI_DEV_RESET:

            /* send a command block reset */
             
            return (usbCbiUfiDevReset ((BLK_DEV *)pCbiUfiDev));

        default:
            errnoSet (S_ioLib_UNKNOWN_REQUEST);
            USB_CBI_UFI_DEBUG ("usbCbiUfiDevIoctl: Unknown Request\n", 
                               0, 0, 0, 0, 0, 0);
            return (ERROR);
            break;

        }

    return (OK);
    }
 



/***************************************************************************
*
* usbCbiUfiCmdExecute - Executes an UFI command block. 
*
* This routine executes previously formed UFI Command as per the CBI protocol.
* First, the command block formed by usbCbiUfiFormCmd() is sent to the device
* via the control endpoint.  This is done using Accept Device Specific Command
* (ADSC) class specific request.  If the command block requires any data 
* transfer to/from the device, then an IRP is submitted to perform the data
* tranport via bulk in/out endpoints.  Finally, status bytes are read from the
* interrupt endpoint.
*
* RETURNS: USB_COMMAND_STATUS, command execution status
*
* ERRNO: 
* \is
* \i USB_COMMAND_FAILED
* Failed to execute the USB Command
*
* \i USB_INTERNAL_ERROR
* Internal error in USB Stack
*
* \i USB_BULK_IO_ERROR
* Bilk I/O Error
* \ie
*
*\NOMANUAL
*/

LOCAL USB_COMMAND_STATUS usbCbiUfiCmdExecute
    ( 
    pUSB_CBI_UFI_DEV pCbiUfiDev      /* pointer to cbi_ufi device  */
    )
    {

    UINT16 actLen = 0xFFFF;

    /* Send the UFI Command block along with the ADSC request */

    if ((usbdVendorSpecific (usbdHandle, pCbiUfiDev->cbiUfiDevId,
                USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_INTERFACE,
                0, 0, pCbiUfiDev->interface, USB_UFI_MAX_CMD_LEN, 
                (UINT8 *)&(pCbiUfiDev->ufiCmdBlk.cmd), &actLen )) != OK )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiCmdExecute: Failed to execute ADSC \n", 
                         0, 0, 0, 0, 0, 0);  
        return (USB_COMMAND_FAILED);
        }    

    if ( pCbiUfiDev->ufiCmdBlk.dataXferLen > 0 )
        {

        if ( OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, 
			   USB_CBI_IRP_TIME_OUT + 1000) 
			   == ERROR )
            {

            /* This should never occur unless the stack does'nt call 
	     * callback at all 
	     */

            USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Irp in Use \n", 
                               0, 0, 0, 0, 0, 0);
            return (USB_INTERNAL_ERROR);
            }

        if (pCbiUfiDev->ufiCmdBlk.direction == USB_UFI_DIR_IN)
            {
         
            /* data is expected from the device. read from BULK_IN pipe */

            memset (&(pCbiUfiDev->inIrp), 0, sizeof (USB_IRP));   

            /* form an IRP to read from BULK_IN pipe */ 

            pCbiUfiDev->inIrp.irpLen            = sizeof(USB_IRP);
            pCbiUfiDev->inIrp.userCallback      = usbCbiUfiIrpCallback; 
            pCbiUfiDev->inIrp.timeout           = USB_CBI_IRP_TIME_OUT;
            pCbiUfiDev->inIrp.transferLen       = pCbiUfiDev->ufiCmdBlk.dataXferLen;
            pCbiUfiDev->inIrp.bfrCount          = 0x01;  
            pCbiUfiDev->inIrp.bfrList[0].pid    = USB_PID_IN;
            pCbiUfiDev->inIrp.bfrList[0].pBfr   = pCbiUfiDev->bulkInData;
            pCbiUfiDev->inIrp.bfrList[0].bfrLen = pCbiUfiDev->ufiCmdBlk.dataXferLen;
            pCbiUfiDev->inIrp.userPtr           = pCbiUfiDev;            
 
            /* Submit IRP */
 
            if (usbdTransfer (usbdHandle, pCbiUfiDev->inPipeHandle, 
                              &(pCbiUfiDev->inIrp)) != OK)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiCmdExecute: Unable to submit IRP for "\
                                 "BULK_IN data transfer\n", 0, 0, 0, 0, 0, 0);
		OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);
                return (USB_INTERNAL_ERROR);
                }
            /* 
             * wait till the data transfer ends on bulk in pipe before reading
             * the command status 
             */


	    if ( OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, USB_CBI_IRP_TIME_OUT + 1000)  == ERROR )
		{
		USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Irp time out \n", 
                                   0, 0, 0, 0, 0, 0);
                /* Cancel the IRP */
                usbdTransferAbort(usbdHandle,
                                  pCbiUfiDev->inPipeHandle, 
                                  &(pCbiUfiDev->inIrp));

		return (USB_INTERNAL_ERROR);
		}
          
            /* Check whether data transfer was complete or not */

            if ( pCbiUfiDev->inIrp.result != OK)
                {
                USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Data transfer failed \n", 
                                   0, 0, 0, 0, 0, 0);
                OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);
                return (USB_COMMAND_FAILED); 
                }
            }
        else if (pCbiUfiDev->ufiCmdBlk.direction == USB_UFI_DIR_OUT)
            {

            /* device is expecting data over BULK_OUT pipe. send it */
        
            memset (&pCbiUfiDev->outIrp, 0, sizeof (USB_IRP));

            /* form an IRP to write to BULK_OUT pipe */ 

            pCbiUfiDev->outIrp.irpLen            = sizeof(USB_IRP);
            pCbiUfiDev->outIrp.userCallback      = usbCbiUfiIrpCallback; 
            pCbiUfiDev->outIrp.timeout           = USB_CBI_IRP_TIME_OUT;
            pCbiUfiDev->outIrp.transferLen       = pCbiUfiDev->ufiCmdBlk.dataXferLen;
            pCbiUfiDev->outIrp.bfrCount          = 0x01;  
            pCbiUfiDev->outIrp.bfrList[0].pid    = USB_PID_OUT;
            pCbiUfiDev->outIrp.bfrList[0].pBfr   = pCbiUfiDev->bulkOutData;
            pCbiUfiDev->outIrp.bfrList[0].bfrLen = pCbiUfiDev->ufiCmdBlk.dataXferLen;
            pCbiUfiDev->outIrp.userPtr           = pCbiUfiDev;

           /* Submit IRP */
 
            if (usbdTransfer (usbdHandle, pCbiUfiDev->outPipeHandle, 
                              &(pCbiUfiDev->outIrp)) != OK)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiCmdExecute: Unable to submit IRP for "\
                                 "BULK_OUT data transfer\n", 0, 0, 0, 0, 0, 0);

                OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);

                return (USB_INTERNAL_ERROR);
                }

            /* 
             * wait till the data transfer ends on bulk out pipe before reading
             * the command status 
             */

            if ( OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, USB_CBI_IRP_TIME_OUT + 1000)  == ERROR )
                {
                USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Irp time out \n", 
                                   0, 0, 0, 0, 0, 0);
                /* Cancel the IRP */
                usbdTransferAbort(usbdHandle,
                                  pCbiUfiDev->outPipeHandle, 
                                  &(pCbiUfiDev->outIrp));

                return (USB_INTERNAL_ERROR);
                }
          
            /* Check whether data transfer was complete or not */

            if (pCbiUfiDev->outIrp.result != OK)
                {
                USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Data transfer failed \n", 
                                   0, 0, 0, 0, 0, 0);
                OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);
                return (USB_COMMAND_FAILED); 
                }
            }

        OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);

        }

    if ( OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, USB_CBI_IRP_TIME_OUT + 1000)  == ERROR )
        {
        USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Irp time out \n", 
                           0, 0, 0, 0, 0, 0);
        OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);
        return (USB_INTERNAL_ERROR);
        }

    /* Read the status from the interrupt endpoint */

    memset (&(pCbiUfiDev->statusIrp), 0, sizeof (USB_IRP));   

    /* form an IRP to read status from interrupt endpoint */ 

    pCbiUfiDev->statusIrp.irpLen            = sizeof(USB_IRP);
    pCbiUfiDev->statusIrp.userCallback      = usbCbiUfiIrpCallback; 
    pCbiUfiDev->statusIrp.timeout           = USB_TIMEOUT_NONE;
    pCbiUfiDev->statusIrp.transferLen       = 0x02;
    pCbiUfiDev->statusIrp.bfrCount          = 0x01;  
    pCbiUfiDev->statusIrp.bfrList[0].pid    = USB_PID_IN;
    pCbiUfiDev->statusIrp.bfrList[0].pBfr   = pCbiUfiDev->intrStatus;
    pCbiUfiDev->statusIrp.bfrList[0].bfrLen = 0x02;
    pCbiUfiDev->statusIrp.userPtr           = pCbiUfiDev;            

    /* Submit IRP */
 
    if (usbdTransfer (usbdHandle, pCbiUfiDev->intrPipeHandle, 
                      &(pCbiUfiDev->statusIrp)) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiCmdExecute: Unable to submit IRP for "\
                         "Interrupt status\n", 0, 0, 0, 0, 0, 0);

        OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);

        return (USB_INTERNAL_ERROR);
        }

    /* wait till the status bytes are read */

    if ( OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, USB_CBI_IRP_TIME_OUT + 1000)  == ERROR )
        {
        USB_CBI_UFI_DEBUG ("usbCbiUfiCmdExecute: Irp time out \n", 
                           0, 0, 0, 0, 0, 0);

        /* Cancel the IRP */
        usbdTransferAbort(usbdHandle,
                          pCbiUfiDev->intrPipeHandle, 
                          &(pCbiUfiDev->statusIrp));

        return (USB_INTERNAL_ERROR);
        }

    OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);    

    /* check for the status bytes read..stalled condition */

    if (pCbiUfiDev->statusIrp.result != OK)
        {
        return (USB_BULK_IO_ERROR);
        } 
    else
        {
        return (USB_COMMAND_SUCCESS);
        }
    } 

/***************************************************************************
*
* usbCbiUfiFormCmd - forms a command block for requested UFI command. 
*
* This routine forms UFI command blocks as per the UFI command specifications.  
* The following are the input parameters 
*
* \is
* \i '<pCbiUfiDev>'
* pointer to USB/CBI/UFI device for which the command has to be formed.
*
* \i '<ufiCmd>'
* identifies the UFI command to be formed.
*
* \i '<cmdParam1>'
* parameter required to form a complete UFI comamnd, if any.
*
* \i '<cmdParam2>'
* parameter required to form a complete UFI comamnd, if any.
* \ie
*
* RETURNS: OK, or ERROR if the command is unsupported.
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiFormCmd 
    (
    pUSB_CBI_UFI_DEV pCbiUfiDev, /* pointer to cbi_ufi device  */
    UINT ufiCmd,                 /* UFI command                */ 
    UINT cmdParam1,              /* command parameter          */
    UINT cmdParam2               /* command parameter          */
    )
    {

    memset (&(pCbiUfiDev->ufiCmdBlk), 0, sizeof(USB_UFI_CMD_BLOCK));

    switch (ufiCmd)
        {
        case USB_UFI_FORMAT_UNIT:    /* UFI Format Unit Command */
      
            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0;
            pCbiUfiDev->ufiCmdBlk.direction    = 0;
      
            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_FORMAT_UNIT;
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = (USB_UFI_FORMAT_FMTDATA | 
                                             USB_UFI_FORMAT_FMT_DEFECT);

            pCbiUfiDev->ufiCmdBlk.cmd[2]  = 0;  /* Track Number              */
            pCbiUfiDev->ufiCmdBlk.cmd[3]  = 0;  /* Interleave factor MSB     */
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = 0;  /* Interleave factor LSB     */  
            pCbiUfiDev->ufiCmdBlk.cmd[7]  = 0;  /* Parameter List length MSB */
            pCbiUfiDev->ufiCmdBlk.cmd[8]  = 0x0C;	/* Parameter List 
							 * length LSB 
							 */
           
            break;

        case USB_UFI_INQUIRY:        /* UFI Inquiry command  */
  
             
            pCbiUfiDev->ufiCmdBlk.dataXferLen  = UFI_STD_INQUIRY_LEN;
            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_IN;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_INQUIRY;

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;   /* Enable Vital 
						  * Product data
						  */      
            pCbiUfiDev->ufiCmdBlk.cmd[2]  = 0;   /* Page Code 
						  * Field
						  */      
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = UFI_STD_INQUIRY_LEN;
  
            break;

        case USB_UFI_MODE_SELECT:   /* UFI Mode Select Command */

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_MODE_SELECT;

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = USB_UFI_MODE_SEL_PF;  
 
            /* TODO : Set the number of bytes as per the mode pages */ 
 
            pCbiUfiDev->ufiCmdBlk.cmd[7]  = 0; /* Parameter list length MSB */
            pCbiUfiDev->ufiCmdBlk.cmd[8]  = 0; /* Parameter list length LSB */       

            break;

        case USB_UFI_MODE_SENSE:     /* UFI Request Mode Sense command */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = USB_UFI_MS_HEADER_LEN;
            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_IN;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_MODE_SENSE;

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;  

            /* Check the basic header information only. */               

            pCbiUfiDev->ufiCmdBlk.cmd[2]  = 1;  
 
            pCbiUfiDev->ufiCmdBlk.cmd[7]  = 0; /* Parameter list length MSB */
            pCbiUfiDev->ufiCmdBlk.cmd[8]  = USB_UFI_MS_HEADER_LEN; /* Parameter list length LSB */       

            break;

        case USB_UFI_PREVENT_MEDIA_REMOVAL: /* Enable or disable media 
					     * removal 
					     */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0;
            pCbiUfiDev->ufiCmdBlk.direction    = 0;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_PREVENT_MEDIA_REMOVAL;            

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;  
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = USB_UFI_MEDIA_REMOVAL_BIT;
  
            break;
 
        case USB_UFI_READ10:         /* UFI 10 byte READ Command */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = ((cmdParam2) * 
                                                 (pCbiUfiDev->blkDev.bd_bytesPerBlk)); 

            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_IN;
 
            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_READ10;
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;

            /*
             * cmdParam1 for this command will indicate the logical block 
             * address at which the read operation begins.  cmdParam2 will
             * indicate the number of logical blocks to be read. 
             */ 

            pCbiUfiDev->ufiCmdBlk.cmd[2]  =  (cmdParam1 & 0xFF000000) >> 24;
            pCbiUfiDev->ufiCmdBlk.cmd[3]  =  (cmdParam1 & 0x00FF0000) >> 16;
            pCbiUfiDev->ufiCmdBlk.cmd[4]  =  (cmdParam1 & 0x0000FF00) >> 8;
            pCbiUfiDev->ufiCmdBlk.cmd[5]  =  (cmdParam1 & 0xFF);

            pCbiUfiDev->ufiCmdBlk.cmd[7]  =  (cmdParam2 & 0xFF00) >> 8;
            pCbiUfiDev->ufiCmdBlk.cmd[8]  =  (cmdParam2 & 0xFF);

            break;

    
        case USB_UFI_READ_CAPACITY:  /* UFI Read Capacity command */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0x8;
            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_IN;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  =  USB_UFI_READ_CAPACITY;

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;  /* RelAdr bit should be zero */
            pCbiUfiDev->ufiCmdBlk.cmd[2]  = 0;  /* Logical block address     */
            pCbiUfiDev->ufiCmdBlk.cmd[3]  = 0;  /* should be set to zero     */
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = 0;
            pCbiUfiDev->ufiCmdBlk.cmd[5]  = 0;
            pCbiUfiDev->ufiCmdBlk.cmd[8]  = 0;  /* PMI bit should be zero    */

            break;

        case USB_UFI_REQUEST_SENSE:  /* UFI Request Sense command */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0x12;
            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_IN;
 
            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_REQUEST_SENSE;
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = UFI_STD_REQ_SENSE_LEN;

            break;


        case USB_UFI_SEND_DIAGNOSTIC: /* UFI Send Diagnostics command */     

            /* Requests the device to perform self-test or reset */

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_SEND_DIAGNOSTIC;            

            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0x04; /* Perform default 
						   * self test 
						   */
        
            break;

 
        case USB_UFI_START_STOP_UNIT: /* UFI Start Stop unit command */   

            /* This command is basically used to update the media type and the
             * write protect status. Load eject bit is not supported by 
	     * USB-FDU.
             */ 

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0x0;
            pCbiUfiDev->ufiCmdBlk.direction    = 0;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_START_STOP_UNIT; 
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;
            pCbiUfiDev->ufiCmdBlk.cmd[4]  = 0x01;   /* Start the media */
                    
            break;

        case USB_UFI_TEST_UNIT_READY:  /* UFI Test Unit Ready Command */  

            /* This command checks if the device is ready or not */

            pCbiUfiDev->ufiCmdBlk.dataXferLen  = 0x0;
            pCbiUfiDev->ufiCmdBlk.direction    = 0;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_TEST_UNIT_READY; 
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0; 

            break;
    

        case USB_UFI_WRITE10:        /* UFI 10-byte Write Command */
            
            pCbiUfiDev->ufiCmdBlk.dataXferLen  = ((cmdParam2) * 
                                                 (pCbiUfiDev->blkDev.bd_bytesPerBlk)); 

            pCbiUfiDev->ufiCmdBlk.direction    = USB_UFI_DIR_OUT;

            pCbiUfiDev->ufiCmdBlk.cmd[0]  = USB_UFI_WRITE10;
            pCbiUfiDev->ufiCmdBlk.cmd[1]  = 0;

            /*
             * cmdParam1 for this command will indicate the logical block 
             * address at which the write operation begins.  cmdParam2 will
             * indicate the number of logical blocks to be written. 
             */ 

            pCbiUfiDev->ufiCmdBlk.cmd[2]  =  (cmdParam1 & 0xFF000000) >> 24;
            pCbiUfiDev->ufiCmdBlk.cmd[3]  =  (cmdParam1 & 0x00FF0000) >> 16;
            pCbiUfiDev->ufiCmdBlk.cmd[4]  =  (cmdParam1 & 0x0000FF00) >> 8;
            pCbiUfiDev->ufiCmdBlk.cmd[5]  =  (cmdParam1 & 0xFF);

            pCbiUfiDev->ufiCmdBlk.cmd[7]  =  (cmdParam2 & 0xFF00) >> 8;
            pCbiUfiDev->ufiCmdBlk.cmd[8]  =  (cmdParam2 & 0xFF);

            break;
    

        default: 
            return (ERROR);
        }
 
    return (OK);
 
    } 


/***************************************************************************
*
* usbCbiUfiReqSense - Executes an Request Sense command block. 
*
* Forms and executes Request Sense command on the specified device.
*
* RETURNS: UINT16 value specifying the sense key or ERROR
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL UINT16 usbCbiUfiReqSense
    (
    pUSB_CBI_UFI_DEV pCbiUfiDev      /* pointer to cbi_ufi device  */
    )
    {
    UINT8 * pReqSense;
    UINT16 status = 0xFFFF;

    if ((pReqSense = OSS_MALLOC (UFI_STD_REQ_SENSE_LEN)) == NULL)
	{
	return ERROR;
	}

    pCbiUfiDev->bulkInData  = pReqSense; 


    if ( usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_REQUEST_SENSE, 0, 0) != OK )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiReqSense: Error forming command\n",
                         0, 0, 0, 0, 0, 0);
        } 

    if ( usbCbiUfiCmdExecute (pCbiUfiDev) != USB_COMMAND_SUCCESS )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiReqSense: Error executing "\
                         "USB_UFI_REQUEST_SENSE command\n", 0, 0, 0, 0, 0, 0);        
        }
    else 
        {
        
         /* Request Sense command success. Check the sense key & qualifiers. */
        status = (*(pReqSense + 12)  << 8) & 0xFF00;
        status = status | *(pReqSense + 13);
        switch (*(pReqSense + USB_UFI_SENSE_KEY_OFFSET))
            {
            case USB_UFI_NO_SENSE:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: No Sense key, %d, %d\n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);
                break;

            case USB_UFI_RECOVERED_ERROR:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Recovered Error, %d, %d\n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);

                break;

            case USB_UFI_NOT_READY:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Device not ready, %d, %d \n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);
                break;

            case USB_UFI_MEDIUM_ERROR:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Medium Error, %d, %d\n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);
                break;

            case USB_UFI_HARDWARE_ERROR:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Hardware Error, %d, %d\n",
				    *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);

                break;

            case USB_UFI_ILL_REQUEST:
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Illegal Request, %d, %d\n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);

                break;

            case USB_UFI_UNIT_ATTN:

                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Unit Attention, %d, %d\n",
                                   *(pReqSense + USB_UFI_SENSE_ASC), 
                                   *(pReqSense + USB_UFI_SENSE_ASCQ), 0, 0, 0, 0);

                /* If Unit attention condition, check the ASC and ASCQ */  
                switch (status)
                    {
                    case USB_UFI_WRITE_PROTECT :

                        USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Media write "\
                                           "protected\n", 0, 0, 0, 0, 0 ,0);
                        break;
 
                    case USB_UFI_MEDIA_CHANGE :  
                        USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Media changed\n",
                                           0, 0, 0, 0, 0 ,0);
                        break;

                    case USB_UFI_POWER_ON_RESET :  
                        USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Power on reset"\
                                           " Bus device reset\n",0, 0, 0, 0, 0 ,0);
                        break;

                    case USB_UFI_COMMAND_SUCCESS :  
                        USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: No sense"\
                                           "Bus device reset\n",0, 0, 0, 0, 0 ,0);
                        break;
                    }
                break;

            case USB_UFI_DATA_PROTECT:  
                USB_CBI_UFI_DEBUG ("usbCbiUfiReqSense: Write Protect\n",
                                   0, 0, 0, 0, 0, 0);
                break;

            default :
                break;

            }
        }

    OSS_FREE (pReqSense);
	return status;
    }        

/***************************************************************************
*
* usbCbiUfiDevDestroy - releases USB_CBI_UFI_DEV structure and its links
*
* Unlinks the indicated USB_CBI_UFI_DEV structure and de-allocates
* resources associated with the device.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbCbiUfiDevDestroy
    (
    pUSB_CBI_UFI_DEV  pCbiUfiDev     /* pointer to MSC/CBI/UFI device   */
    )

    {
    if (pCbiUfiDev != NULL)
        {

        /* Unlink the structure. */

        usbListUnlink (&pCbiUfiDev->cbiUfiDevLink);

        /* Release pipes and wait for IRPs. */

        if (pCbiUfiDev->outPipeHandle != NULL)
            usbdPipeDestroy (usbdHandle, pCbiUfiDev->outPipeHandle);

        if (pCbiUfiDev->inPipeHandle != NULL)
            usbdPipeDestroy (usbdHandle, pCbiUfiDev->inPipeHandle);

        /* wait for any IRP to complete */

        OSS_SEM_TAKE (pCbiUfiDev->cbiUfiIrpSem, OSS_DONT_BLOCK); 

        OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);

        if (pCbiUfiDev->cbiUfiIrpSem)
            OSS_SEM_DESTROY(pCbiUfiDev->cbiUfiIrpSem);


        /* Release structure. */

        OSS_FREE (pCbiUfiDev);
        }
    }


/***************************************************************************
*
* usbCbiUfiDevFind - Searches for a USB_CBI_UFI_DEV for indicated node ID
*
* This fucntion searches the pointer to USB_CBI_UFI_DEV structure for 
* specified <nodeId>
*
* RETURNS: pointer to matching USB_CBI_UFI_DEV or NULL if not found
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_CBI_UFI_DEV usbCbiUfiDevFind
    (
    USBD_NODE_ID nodeId          /* node ID to be looked for */
    )

    {

    pUSB_CBI_UFI_DEV pCbiUfiDev = usbListFirst (&cbiUfiDevList);

    /* browse through the list */

    while (pCbiUfiDev != NULL)
        {
        if (pCbiUfiDev->cbiUfiDevId == nodeId)
            break;

        pCbiUfiDev = usbListNext (&pCbiUfiDev->cbiUfiDevLink);
        }

    return (pCbiUfiDev);

    }

/***************************************************************************
*
* findEndpoint - Searches for a BULK endpoint of the indicated direction
*
* This function searches for the Bulk endpoint for specified direction
*
* RETURNS: pointer to matching endpoint descriptor or NULL if not found
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_ENDPOINT_DESCR findEndpoint
    (
    pUINT8 pBfr,
    UINT16 bfrLen,
    UINT8  attribute, 
    UINT16 direction
    )

    {
    pUSB_ENDPOINT_DESCR pEp;

    while ((pEp = usbDescrParseSkip (&pBfr, &bfrLen, USB_DESCR_ENDPOINT)) 
           != NULL)
        {
        if ((pEp->attributes & USB_ATTR_EPTYPE_MASK) == attribute &&
            (pEp->endpointAddress & USB_ENDPOINT_DIR_MASK) == direction)
            break;
        }

    return (pEp);
    }


/***************************************************************************
*
* usbCbiUfiPhysDevCreate - create USB_CBI_UFI_DEV Structure for the device 
* 
* This function is invoked from the dynamic attach callback routine whenever 
* a USB_CBI_UFI_DEV device is attached.  It allocates memory for the structure,
* sets device configuration, and creates pipe for bulk-in,bulk-out and interrupt
* endpoints.  It also sets the device configuration so that it follows UFI
* command set.
* 
* RETURNS: pUSB_CBI_UFI_DEV on success, NULL if failed to create device
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_CBI_UFI_DEV usbCbiUfiPhysDevCreate
    (
    USBD_NODE_ID nodeId,         /* USBD Node Id ofthe device  */     
    UINT16       configuration,  /* Configuration value        */ 
    UINT16       interface       /* Interface Number           */ 
    )
    {
    UINT16   actLength;
    UINT8 * pBfr;           /* store for descriptors      */ 
    UINT8 * pScratchBfr;    /* pointer to the above store */ 
    UINT     ifNo;
  
    USB_CBI_UFI_DEV * pCbiUfiDev;
    USB_CONFIG_DESCR * pCfgDescr;
    USB_INTERFACE_DESCR * pIfDescr;
    USB_ENDPOINT_DESCR * pOutEp;
    USB_ENDPOINT_DESCR * pInEp;
    USB_ENDPOINT_DESCR * pIntrEp;

    /*
     * A new device is being attached.	Check if we already 
     * have a structure for this device.
     */

    if ((pCbiUfiDev = usbCbiUfiDevFind (nodeId)) != NULL)
        return (pCbiUfiDev);

    /* Allocate memory for a new structure to represent this device */

    if ((pBfr = OSS_MALLOC (USB_MAX_DESCR_LEN)) == NULL)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to allocate \
				memory:pBfr\n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }

    if ((pCbiUfiDev = OSS_CALLOC (sizeof (*pCbiUfiDev))) == NULL)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to allocate \
				memory:pCbiUfiDev\n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }

    pCbiUfiDev->cbiUfiDevId     = nodeId; 
    pCbiUfiDev->interface       = interface;
    pCbiUfiDev->connected       = TRUE;

    if (OSS_SEM_CREATE( 1, 1, &pCbiUfiDev->cbiUfiIrpSem) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to create irp semaphore "\
                         "descriptor\n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }

    /* Check out the device configuration */

    /* Configuration index is assumed to be one less than config'n value */

    if (usbdDescriptorGet (usbdHandle, 
			   pCbiUfiDev->cbiUfiDevId, 
			   USB_RT_STANDARD | USB_RT_DEVICE, 
			   USB_DESCR_CONFIGURATION, 
			   0, 
			   0, 
			   USB_MAX_DESCR_LEN, 
			   pBfr, 
			   &actLength) 
			!= OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to read configuration "\
                         "descriptor\n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }

    if ((pCfgDescr = usbDescrParse (pBfr, 
				    actLength, 
				    USB_DESCR_CONFIGURATION)) 
				  == NULL)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to find configuration "\
                         "descriptor\n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }

    /* Store the configuration value */
    pCbiUfiDev->configuration = pCfgDescr->configurationValue;
   
    /* Look for the interface representing the USB_CBI_UFI Device. */

    ifNo = 0;

    /*
     * usbDescrParseSkip() modifies the value of the pointer it recieves
     * so we pass it a copy of our buffer pointer
     */

    pScratchBfr = pBfr;

    while ((pIfDescr = usbDescrParseSkip (&pScratchBfr, 
					  &actLength, 
					  USB_DESCR_INTERFACE)) 
					!= NULL)
        {
        if (ifNo == pCbiUfiDev->interface)
            break;
        ifNo++;
        }

    if (pIfDescr == NULL)
        goto errorExit;

    pCbiUfiDev->altSetting = pIfDescr->alternateSetting;

    /* 
     * Retrieve the endpoint descriptor(s) following the identified interface
     * descriptor.
     */

    if ((pOutEp = findEndpoint (pScratchBfr, 
				actLength, 
				USB_ATTR_BULK, 
				USB_ENDPOINT_OUT)) 
			    == NULL)
        goto errorExit;

    if ((pInEp = findEndpoint (pScratchBfr, 	
			       actLength, 	
			       USB_ATTR_BULK, 	
			       USB_ENDPOINT_IN)) 
			   == NULL)
        goto errorExit;

    if ((pIntrEp = findEndpoint (pScratchBfr, 
				 actLength, 
				 USB_ATTR_INTERRUPT, 
				 USB_ENDPOINT_IN)) 
			      == NULL)
        goto errorExit; 

    pCbiUfiDev->outEpAddress = pOutEp->endpointAddress;
    pCbiUfiDev->inEpAddress  = pInEp->endpointAddress;
    pCbiUfiDev->intrEpAddress  = pIntrEp->endpointAddress;
    pCbiUfiDev->inEpAddressMaxPkt = pInEp->maxPacketSize;
    pCbiUfiDev->outEpAddressMaxPkt = pOutEp->maxPacketSize;

    /* Set the device configuration corresponding to USB_CBI_UFI_DEV */

    if ((usbdConfigurationSet (usbdHandle, pCbiUfiDev->cbiUfiDevId, 
                               pCbiUfiDev->configuration, 
                               pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT)) 
			      != OK )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Unable to set device "\
                         "configuration \n", 0, 0, 0, 0, 0, 0);
        goto errorExit;
        }
    else
        {
        USB_CBI_UFI_DEBUG ("usbCbiUfiPhysDevCreate: Configuration set to 0x%x \n",
                           pCbiUfiDev->configuration, 0, 0, 0, 0, 0);
        }
    
   /* Select interface 
    * 
    * NOTE: Some devices may reject this command, and this does not represent
    * a fatal error.  Therefore, we ignore the return status.
    */

    usbdInterfaceSet (usbdHandle, 
		      pCbiUfiDev->cbiUfiDevId, 
		      pCbiUfiDev->interface, 
		      pCbiUfiDev->altSetting);

    /* Create a Bulk-out pipe for the USB_CBI_UFI_DEV device */

    if (usbdPipeCreate (usbdHandle, 
			pCbiUfiDev->cbiUfiDevId, 
			pOutEp->endpointAddress, 
			pCbiUfiDev->configuration, 
			pCbiUfiDev->interface, 
			USB_XFRTYPE_BULK, 
			USB_DIR_OUT, 
			FROM_LITTLEW(pOutEp->maxPacketSize),
			0, 
			0, 
			&(pCbiUfiDev->outPipeHandle)) 
		!= OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Error creating bulk out pipe\n",
                         0, 0, 0, 0, 0, 0);
        goto errorExit;
        } 

    /* Create a Bulk-in pipe for the USB_CBI_UFI_DEV device */
       
    if (usbdPipeCreate (usbdHandle, pCbiUfiDev->cbiUfiDevId, 
                        pInEp->endpointAddress, pCbiUfiDev->configuration, 
                        pCbiUfiDev->interface, USB_XFRTYPE_BULK, USB_DIR_IN, 
                        FROM_LITTLEW(pInEp->maxPacketSize), 0, 0, 
                        &(pCbiUfiDev->inPipeHandle)) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Error creating bulk in pipe\n",
                         0, 0, 0, 0, 0, 0);
        goto errorExit;
        } 


    /* Create a Interrupt pipe for the USB_CBI_UFI_DEV device */

    if ((usbdPipeCreate (usbdHandle, pCbiUfiDev->cbiUfiDevId, 
                        pIntrEp->endpointAddress, 
			pCbiUfiDev->configuration, 
                        pCbiUfiDev->interface, 
			USB_XFRTYPE_INTERRUPT, 
			USB_DIR_IN, 
                        2, 
			2, 
			0x10, 
                        &(pCbiUfiDev->intrPipeHandle))) 
			!= OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Error creating interrupt pipe %x\n",
                         0, 0, 0, 0, 0, 0);
        goto errorExit;
        } 

    /* Clear HALT feauture on the endpoints */

    if ((usbdFeatureClear (usbdHandle, 
			   pCbiUfiDev->cbiUfiDevId, 
			   USB_RT_ENDPOINT, 
                           USB_FSEL_DEV_ENDPOINT_HALT, 
                           (pOutEp->endpointAddress & 0xFF))) 
			   != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Failed to clear HALT feauture "\
                         "on bulk out Endpoint %x\n", 0, 0, 0, 0, 0, 0);
        }  

    if ((usbdFeatureClear (usbdHandle, 
			   pCbiUfiDev->cbiUfiDevId, 
			   USB_RT_ENDPOINT, 
                           USB_FSEL_DEV_ENDPOINT_HALT, 
                           (pOutEp->endpointAddress & 0xFF))) 
			   != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiPhysDevCreate: Failed to clear HALT feauture "\
                         "on bulk in Endpoint %x\n", 0, 0, 0, 0, 0, 0);
        } 

    /* Link the newly created structure. */

    usbListLink (&cbiUfiDevList, pCbiUfiDev, 
                 &pCbiUfiDev->cbiUfiDevLink, LINK_TAIL);

    OSS_FREE (pBfr);

    return (pCbiUfiDev);

errorExit:

    /* Error in creating CBI UFI Device */

    usbCbiUfiDevDestroy (pCbiUfiDev);

    OSS_FREE (pBfr);

    return (NULL);

    }

/***************************************************************************
*
* usbCbiUfiBlkDevCreate - create a block device
*
* This routine  initializes a BLK_DEV structure, which describes a 
* logical partition on a USB_CBI_UFI_DEV device.  A logical partition is 
* an array of contiguously addressed blocks; it can be completely described 
* by the number of blocks and the address of the first block in the partition.  
*
* RETURNS: A pointer to the BLK_DEV, or NULL if no CBI/UFI device exists.
*
* ERRNO: none
*/

BLK_DEV * usbCbiUfiBlkDevCreate 
    (
    USBD_NODE_ID nodeId        /* Node Id of the CBI_UFI device */ 
    )
    {

    UINT8 * pInquiry;    /* store for INQUIRY data  */  
    UINT16 senseStatus = 0xffff;
 
    pUSB_CBI_UFI_DEV pCbiUfiDev = usbCbiUfiDevFind (nodeId);

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK); 

	if ((pInquiry = OSS_MALLOC (UFI_STD_INQUIRY_LEN)) == NULL)
	{
	  USB_CBI_UFI_ERR ("usbCbiUfiBlkDevCreate: Error allocating memory\n",
                        0, 0, 0, 0, 0, 0);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return NULL;
	}

    if (pCbiUfiDev == NULL)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiBlkDevCreate: No MSC/CBI/UFI found\n",
                         0, 0, 0, 0, 0, 0);
		OSS_FREE (pInquiry);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (NULL);
        }

    /* 
     * Initialize the standard block device structure for use with file 
     * systems.
     */

    pCbiUfiDev->blkDev.bd_blkRd        = (FUNCPTR) usbCbiUfiDevBlkRd;
    pCbiUfiDev->blkDev.bd_blkWrt       = (FUNCPTR) usbCbiUfiDevBlkWrt;
    pCbiUfiDev->blkDev.bd_ioctl        = (FUNCPTR) usbCbiUfiDevIoctl;
    pCbiUfiDev->blkDev.bd_reset        = (FUNCPTR) NULL; 
    pCbiUfiDev->blkDev.bd_statusChk    = (FUNCPTR) usbCbiUfiDevStChk;
    pCbiUfiDev->blkDev.bd_retry        = 1;
    pCbiUfiDev->blkDev.bd_mode         = O_RDWR;
    pCbiUfiDev->blkDev.bd_readyChanged = TRUE;
    pCbiUfiDev->blkDev.bd_nBlocks =  (UINT32) 0;
    pCbiUfiDev->blkDev.bd_bytesPerBlk = (UINT32)  2;

   /* 
     * Read out the standard INQUIRY information from the device, mainly to
     * check whether the media is removable or not.
     */
    pCbiUfiDev->bulkInData  = pInquiry;

    if ( usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_INQUIRY, 0, 0) != OK )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiBlkDevCreate: Error forming command\n",
                         0, 0, 0, 0, 0, 0);

			OSS_FREE (pInquiry);
		    OSS_MUTEX_RELEASE (cbiUfiDevMutex);
	        return (NULL); 
        } 


    if ( usbCbiUfiCmdExecute (pCbiUfiDev) != USB_COMMAND_SUCCESS)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiBlkDevCreate: Error executing "\
                         "USB_UFI_INQUIRY command\n", 0, 0, 0, 0, 0, 0);        

			OSS_FREE (pInquiry);
    	    OSS_MUTEX_RELEASE (cbiUfiDevMutex);
	    senseStatus = usbCbiUfiReqSense (pCbiUfiDev);
            return (NULL);  
        }

    /* Check the media type bit  */

    if (*(pInquiry + 1) & USB_UFI_INQUIRY_RMB_BIT)
        {
        pCbiUfiDev->blkDev.bd_removable = TRUE;
        }
    else
        {
        pCbiUfiDev->blkDev.bd_removable = FALSE;      
        }

    if (usbCbiUfiDevStChk(&pCbiUfiDev->blkDev) == ERROR)
         {
         OSS_FREE (pInquiry);
         OSS_MUTEX_RELEASE (cbiUfiDevMutex);
         pCbiUfiDev->connected = FALSE;
         return NULL;
         }     

    OSS_FREE (pInquiry);
    OSS_MUTEX_RELEASE (cbiUfiDevMutex);
    return (&pCbiUfiDev->blkDev);
   	}        

/***************************************************************************
*
* usbCbiUfiDevBlkRd - routine to read one or more blocks from the device.
*
* This routine reads the specified physical sector(s) from a specified
* physical device.  Typically called by file system when data is to be
* read from a particular device.
*
* RETURNS: OK on success, or ERROR if failed to read from device
*
* ERRNO: None
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiDevBlkRd
    (
    BLK_DEV * pBlkDev,           /* pointer to UFI  device   */ 
    UINT32       blkNum,            /* logical block number     */
    UINT32       numBlks,           /* number of blocks to read */
    char *    pBuf               /* store for data           */ 
    )
    {

    USB_COMMAND_STATUS s;
    pUSB_CBI_UFI_DEV  pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;   

    USB_CBI_UFI_DEBUG ("usbCbiUfiDevBlkRd: Reading from block number: %x\n",
                       blkNum, 0, 0, 0, 0, 0);
 
    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK); 

    /* initialise the pointer to fetch bulk out data */

    pCbiUfiDev->bulkInData = (UINT8 *)pBuf;

    if ( usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_READ10, 
                           blkNum, numBlks) != OK )
        {
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR);  
        }
  
    s = usbCbiUfiCmdExecute (pCbiUfiDev);
   
    if ( s != USB_COMMAND_SUCCESS )
        {
        if ( s == USB_COMMAND_FAILED)
            usbCbiUfiDevReset (pBlkDev); 
       
        usbCbiUfiReqSense (pCbiUfiDev);

        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR); 
        }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex); 
    return (OK);

    }

/***************************************************************************
*
* usbCbiUfiDevBlkWrt - routine to write one or more blocks to the device.
*
* This routine writes the specified physical sector(s) to a specified physical
* device.
*
* RETURNS: OK on success, or ERROR if failed to write to device
*
* ERRNO: None
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiDevBlkWrt
    (
    BLK_DEV * pBlkDev,           /* pointer to UFI device     */  
    UINT32       blkNum,            /* logical block number      */
    UINT32       numBlks,           /* number of blocks to write */ 
    char *    pBuf               /* data to be written        */ 
    )
    {
    USB_COMMAND_STATUS s;

    pUSB_CBI_UFI_DEV  pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;   

    USB_CBI_UFI_DEBUG ("usbCbiUfiDevBlkWrt: Writing from block number: %x\n",
                        blkNum, 0, 0, 0, 0, 0);

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK); 

    /* initialise the pointer to fetch bulk out data */

    pCbiUfiDev->bulkOutData = (UINT8 *)pBuf;

    if ( usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_WRITE10, blkNum, 
                           numBlks) != OK )
        {
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR); 
        }

    s = usbCbiUfiCmdExecute (pCbiUfiDev);

    /*
     * check for the status of the write operation. If failed while
     * transferring ADSC or any FATAL error, do a block reset.
     */   

    if ( s != USB_COMMAND_SUCCESS )
        {
        if ( s == USB_COMMAND_FAILED)
            {
            usbCbiUfiDevReset (pBlkDev); 
            }

        usbCbiUfiReqSense (pCbiUfiDev);   
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR); 
        }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex); 
    return (OK);

    }


/***************************************************************************
*
* usbCbiUfiIrpCallback - Invoked upon IRP completion
*
* Examines the status of the IRP submitted.  
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbCbiUfiIrpCallback
    (
    pVOID p                      /* pointer to the IRP submitted */
    )
    {
    pUSB_IRP      pIrp     = (pUSB_IRP) p;

    pUSB_CBI_UFI_DEV pCbiUfiDev = pIrp->userPtr;


    /* check whether the IRP was for bulk out/ bulk in / status transport */

    if (pIrp == &(pCbiUfiDev->outIrp))
        {  
        if (pIrp->result == OK)     /* check the result of IRP */
            {
            USB_CBI_UFI_DEBUG ("usbCbiUfiIrpCallback: Num of Bytes transferred on "\
                               "out pipe %d\n", pIrp->bfrList[0].actLen, 
                               0, 0, 0, 0, 0); 
            }
        else
            {
            USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback: Irp failed on Bulk Out %x \n",
                            pIrp->result, 0, 0, 0, 0, 0); 

            /* Clear HALT Feature on Bulk out Endpoint */ 

            if ((usbdFeatureClear (usbdHandle, pCbiUfiDev->cbiUfiDevId, 
                           USB_RT_ENDPOINT, USB_FSEL_DEV_ENDPOINT_HALT, 
                           (pCbiUfiDev->outEpAddress & 0xFF))) != OK)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback: Failed to clear HALT "\
                                 "feature on bulk out Endpoint %x\n", 0, 0, 0, 
                                 0, 0, 0);
                }
            }
        } 
     else  if (pIrp == &(pCbiUfiDev->inIrp))   /* IRP for bulk_in data */
        {
        if (pIrp->result == OK)
            {
            USB_CBI_UFI_DEBUG ("usbCbiUfiIrpCallback: Num of Bytes read from Bulk "\
                               "In =%d\n", pIrp->bfrList[0].actLen, 0, 0, 0, 0, 0); 
            }
        else   /* IRP on BULK IN failed */
            {
            USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback : Irp failed on Bulk in ,%x\n", 
                          pIrp->result, 0, 0, 0, 0, 0);

            /* Clear HALT Feature on Bulk in Endpoint */

            if ((usbdFeatureClear (usbdHandle, 
				   pCbiUfiDev->cbiUfiDevId, 
				   USB_RT_ENDPOINT, 
                                   USB_FSEL_DEV_ENDPOINT_HALT, 
                                   (pCbiUfiDev->inEpAddress & 0xFF))) 
				  != OK)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback: Failed to clear HALT "\
                        "feature on bulk in Endpoint %x\n", 0, 0, 0, 0, 0, 0);
                }
             }

        }
     else  if (pIrp == &(pCbiUfiDev->statusIrp))   /* IRP for bulk_in data */
        {

        if (pIrp->result == OK)
            {
            USB_CBI_UFI_DEBUG ("usbCbiUfiIrpCallback: Num of Bytes read from "\
                               "status pipe =%d, %d, %d\n", pIrp->bfrList[0].actLen, 
                                pCbiUfiDev->intrStatus[0], 
                                pCbiUfiDev->intrStatus[1], 0, 0, 0); 
            }
        else   /* IRP on BULK IN failed */
            {
            USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback : Irp failed on interrupt pipe ,%x\n", 
                          pIrp->result, 0, 0, 0, 0, 0);

            /* Clear HALT Feature on Bulk in Endpoint */

            if ((usbdFeatureClear (usbdHandle, 
				   pCbiUfiDev->cbiUfiDevId, 
				   USB_RT_ENDPOINT, 
                                   USB_FSEL_DEV_ENDPOINT_HALT, 
                                  (pCbiUfiDev->intrEpAddress & 0xFF))) 
				   != OK)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiIrpCallback: Failed to clear HALT "\
                        "feature on bulk in Endpoint %x\n", 0, 0, 0, 0, 0, 0);
                }
            }
        }
   
    OSS_SEM_GIVE (pCbiUfiDev->cbiUfiIrpSem);
    }   
  
/***************************************************************************
*
* usbCbiUfiDevStChk - routine to check device status
*
* Typically called by the file system before doing a read/write to the device.
* The routine issues a TEST_UNIT_READY command.  Also, if the device is not
* ready, then the sense data is read from the device to check for presence/
* change of media.  For removable devices, the ready change flag is set to 
* indicate media change.  File system checks this flag, and remounts the device
*
* RETURNS: OK if the device is ready for IO, or ERROR if device is busY
*
* ERRNO: None
*
*\NOMANUAL 
*/

LOCAL STATUS usbCbiUfiDevStChk
    (
    BLK_DEV * pBlkDev                /* pointer to MSC/CBI/UFI device */
    )
    {

    UINT8 * pSenseData;  /* store for SENSE data  */                 
    UINT8 * pInquiry;  /* store for SENSE data  */                 
    USB_COMMAND_STATUS s ;
    STATUS  rtnStatus = OK;
    STATUS  ready     = OK;
    UINT8   devStatus = OK;
    pUSB_CBI_UFI_DEV pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;
    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);

    if ((pSenseData = OSS_MALLOC (USB_UFI_MS_HEADER_LEN)) == NULL) {
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
	return ERROR;
	}

    if ((pInquiry = OSS_MALLOC (UFI_STD_INQUIRY_LEN)) == NULL) {
  	OSS_FREE (pSenseData);	
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return ERROR;
	}


    USB_CBI_UFI_DEBUG ("usbCbiUfiDevStChk: About to send TEST_UNIT_READY\n",
                       0, 0, 0, 0, 0, 0);    
    devStatus = usbCbiUfiDevTestReady((BLK_DEV*)pCbiUfiDev);
    if (devStatus == CBI_TEST_READY_FAILED)
        {
        /* Failed to check the device */
        if (pCbiUfiDev->blkDev.bd_removable == TRUE)
            pCbiUfiDev->blkDev.bd_readyChanged = TRUE;

        OSS_FREE (pInquiry);
        OSS_FREE (pSenseData);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return( ERROR );
        }
    else if (devStatus == CBI_TEST_READY_NOCHANGE)
        {
        /* No Media Change detected */
        OSS_FREE (pInquiry);
        OSS_FREE (pSenseData);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return( OK );
        }
    else
        {
        /* media change detected */
        if (pCbiUfiDev->blkDev.bd_removable == TRUE)
            pCbiUfiDev->blkDev.bd_readyChanged = TRUE;
        }

 	
    /* set pointer */
    pCbiUfiDev->bulkInData  = pInquiry;		
    if ( usbCbiUfiFormCmd (pCbiUfiDev, 
                           USB_UFI_READ_CAPACITY, 
                           0, 
                           0) 
                           != OK )
        {
        rtnStatus = ERROR; 
        USB_CBI_UFI_ERR ("***********error creating capacity cmd\n", 0,0,0,0,0,0);
        OSS_FREE (pInquiry);
        OSS_FREE (pSenseData);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return ERROR;
        } 
	
		
    rtnStatus = usbCbiUfiCmdExecute (pCbiUfiDev);
    if ( rtnStatus != USB_COMMAND_SUCCESS ) 
        {
        ready = usbCbiUfiReqSense (pCbiUfiDev);
        USB_CBI_UFI_ERR ("************failed sending capacity cmd cmd sense %x return error \n", ready, 0,0,0,0,0);
        OSS_FREE (pInquiry);
        OSS_FREE (pSenseData);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return ERROR;
        }

    /*
     * Response is in BIG endian format. Swap it to get correct 
     * value.  * Also, READ_CAPACITY returns the address of the 
     * last logical block, NOT the number of blocks.  Since the 
     * blkDev structure wants the number of blocks, we add 1.
     */
 
    pCbiUfiDev->blkDev.bd_nBlocks =  USB_SWAP_32 (*((UINT32 *) pInquiry)) + 1;
    pCbiUfiDev->blkDev.bd_bytesPerBlk = USB_SWAP_32 (*((UINT32 *) (pInquiry + 4)));
    USB_CBI_UFI_DEBUG ("\nDevice detected with %d blocks / %d bytes per block\n",
                        pCbiUfiDev->blkDev.bd_nBlocks,pCbiUfiDev->blkDev.bd_bytesPerBlk,0,0,0,0);
 

    /* Check the device again */
    devStatus = usbCbiUfiDevTestReady((BLK_DEV*)pCbiUfiDev);
    if ((devStatus == CBI_TEST_READY_FAILED) || (devStatus == CBI_TEST_READY_CHANGED))
        {
        /* Failed to check the device or there was a media change */
        if (pCbiUfiDev->blkDev.bd_removable == TRUE)
            pCbiUfiDev->blkDev.bd_readyChanged = TRUE;

        OSS_FREE (pInquiry);
        OSS_FREE (pSenseData);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return( ERROR );
        }
  
    /* if we are here, then TEST UNIT READY command returned device is READY */

    /* 
     * Based on the status of write-protect status bit update the
     * access mode of the device. Issue a Mode Sense for reading
     * basic header information.
     */

    pCbiUfiDev->bulkInData = pSenseData;

    if ( usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_MODE_SENSE, 0, 0) != OK )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevStChk: Error forming command\n",
                         0, 0, 0, 0, 0, 0);
        OSS_FREE (pSenseData);
        OSS_FREE (pInquiry);
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR); 
        } 

    if ( (s = usbCbiUfiCmdExecute (pCbiUfiDev)) != USB_COMMAND_SUCCESS )
        {
        usbCbiUfiReqSense (pCbiUfiDev);

        if ( s == USB_COMMAND_FAILED )
            usbCbiUfiDevReset (pBlkDev); 
 
        USB_CBI_UFI_ERR ("usbCbiUfiDevStChk: Error executing " \
                        "USB_UFI_MODE_SENSE command\n", 0, 0, 0, 0, 0, 0);        

        OSS_FREE (pSenseData);

        OSS_FREE (pInquiry);

        OSS_MUTEX_RELEASE (cbiUfiDevMutex);

        return (ERROR);  
        }
   
    if ((*(pSenseData + 3) & USB_UFI_WRITE_PROTECT_BIT) \
				== USB_UFI_WRITE_PROTECT_BIT)
        {
        USB_CBI_UFI_DEBUG ("usbCbiUfiDevStChk: Media Write protected\n",
                           0, 0, 0, 0, 0, 0);
        pCbiUfiDev->blkDev.bd_mode = O_RDONLY;
        }
    else  {
        USB_CBI_UFI_DEBUG ("media is rdwr \n",0, 0, 0, 0, 0, 0);
        pCbiUfiDev->blkDev.bd_mode = O_RDWR;
    }
    OSS_FREE (pSenseData);
    OSS_FREE (pInquiry);
    OSS_MUTEX_RELEASE (cbiUfiDevMutex);
    return (OK);

    }


/***************************************************************************
*
* usbCbiUfiDevReset -  routine to reset the MSC/CBI/UFI device.
*
* This routine issues a command block reset and followed by a port reset if 
* needed.  It also clears stall condition on bulk-in and bulk-out endpoints.
*
* RETURNS: OK if reset succcssful, or ERROR
*
* ERRNO: None
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiDevReset
    (
    BLK_DEV * pBlkDev                /* pointer to MSC/CBI/UFI device */
    )
    {

    USBD_NODE_INFO    ufiNodeInfo;
    pUSB_CBI_UFI_DEV pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;

    USB_CBI_UFI_DEBUG ("usbCbiUfiDevReset: Sending Command block reset\n",
                       0, 0, 0, 0, 0, 0);

    /* issue a command block reset as mentioned in CBI specification */
 	OSS_MUTEX_TAKE (cbiUfiDevMutex,OSS_BLOCK);
    pCbiUfiDev->ufiCmdBlk.dataXferLen = 0;
    pCbiUfiDev->ufiCmdBlk.direction   = 0;

    memset (&(pCbiUfiDev->ufiCmdBlk.cmd), 0xFF, USB_UFI_MAX_CMD_LEN);    
 
    pCbiUfiDev->ufiCmdBlk.cmd[0]      = 0x1D;     
    pCbiUfiDev->ufiCmdBlk.cmd[1]      = 0x04;


    /* Send the CBI Command block reset along with the ADSC request */

    if ( usbCbiUfiCmdExecute (pCbiUfiDev) != USB_COMMAND_SUCCESS )
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevReset: Error executing " \
                         "Class specific Reset command\n", 0, 0, 0, 0, 0, 0);

                
        if (usbdNodeInfoGet (usbdHandle, pCbiUfiDev->cbiUfiDevId, &ufiNodeInfo,
                             sizeof (USBD_NODE_INFO) ) != OK )
            {
            OSS_MUTEX_RELEASE (cbiUfiDevMutex);
            return (ERROR);  
            }

        /* Issue a port reset on the HUB port to which UFI device is attached */

        if ( usbdFeatureSet (usbdHandle, pCbiUfiDev->cbiUfiDevId,
                             USB_RT_CLASS | USB_RT_OTHER, 
                             USB_HUB_FSEL_PORT_RESET, 
                             ufiNodeInfo.parentHubPort) != OK)
            {
            OSS_MUTEX_RELEASE (cbiUfiDevMutex);
            return (ERROR);  
            } 
        }

    /* Clear STALL condition on bulk-in and bulk-out endpoints */

    if ((usbdFeatureClear (usbdHandle,  
			   pCbiUfiDev->cbiUfiDevId, 
			   USB_RT_ENDPOINT, 
                           USB_FSEL_DEV_ENDPOINT_HALT, 
                           (pCbiUfiDev->inEpAddress & 0xFF))) 
			   != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevReset: Failed to clear HALT feauture"\
                      " on bulk in Endpoint %x\n", 0, 0, 0, 0, 0, 0);
        }  

    if ((usbdFeatureClear (usbdHandle,  
			   pCbiUfiDev->cbiUfiDevId, 
			   USB_RT_ENDPOINT, 
                           USB_FSEL_DEV_ENDPOINT_HALT, 
                           (pCbiUfiDev->outEpAddress & 0xFF))) 
			   != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevReset: Failed to clear HALT feauture"\
                      " on bulk out Endpoint %x\n", 0, 0, 0, 0, 0, 0);
        } 
	OSS_MUTEX_RELEASE (cbiUfiDevMutex);
    return (OK);

    }

/***************************************************************************
*
* notifyAttach - Notifies registered callers of attachment/removal
*
* This function notifies of the device attachment and removal to the 
* registered clients.
*
* RETURNS: N/A
* 
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    USBD_NODE_ID cbiUfiDevId,
    UINT16 attachCode
    )

    {
    pATTACH_REQUEST pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
    {
    (*pRequest->callback) (pRequest->callbackArg, 
	                   cbiUfiDevId, attachCode);

    pRequest = usbListNext (&pRequest->reqLink);
    }
    }

/***************************************************************************
*
* usbCbiUfiDynamicAttachRegister - Register UFI device attach callback.
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_UFI_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     USBD_NODE_ID cbiUfiDevId,
*     UINT16 attachCode
*     );
* \ce
*
* usbCBiUfiDevLib will invoke <callback> each time a CBI_UFI device
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also be passed the nodeID of the device 
* being created/destroyed and an attach code of USB_UFI_ATTACH or 
* USB_UFI_REMOVE.
*
* NOTE: The user callback routine should not invoke any driver function that
* submits IRPs.  Further processing must be done from a different task context.
* As the driver routines wait for IRP completion, they cannot be invoked from
* USBD client task's context created for this driver.
*
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbCbiUfiDevLib_BAD_PARAM
* Bad Paramter passed
*
* \i S_usbCbiUfiDevLib_OUT_OF_MEMORY
* Sufficient memory not available
* \ie
*/

STATUS usbCbiUfiDynamicAttachRegister
    (
    USB_UFI_ATTACH_CALLBACK callback,	/* new callback to be registered */
    pVOID arg                           /* user-defined arg to callback  */
    )

    {
    pATTACH_REQUEST   pRequest;
    pUSB_CBI_UFI_DEV  pCbiUfiDev;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
        return (ossStatus (S_usbCbiUfiDevLib_BAD_PARAM));

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);

    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
        {
        status = S_usbCbiUfiDevLib_OUT_OF_MEMORY;
        }
    else
        {
        pRequest->callback    = callback;
        pRequest->callbackArg = arg;

        usbListLink (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL) ;
    
       /* 
        * Perform an initial notification of all currrently attached
        * CBI_UFI devices.
        */

        pCbiUfiDev = usbListFirst (&cbiUfiDevList);

        while (pCbiUfiDev != NULL)
	    {
            if (pCbiUfiDev->connected)
                (*callback) (arg, pCbiUfiDev->cbiUfiDevId, USB_UFI_ATTACH);

	    pCbiUfiDev = usbListNext (&pCbiUfiDev->cbiUfiDevLink);
	    }
        }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex);

    return (ossStatus (status));
    }


/***************************************************************************
*
* usbCbiUfiDynamicAttachUnregister - Unregisters CBI_UFI attach callback.
*
* This function cancels a previous request to be dynamically notified for
* CBI_UFI device attachment and removal.  The <callback> and <arg> paramters 
* must exactly match those passed in a previous call to 
* usbCbiUfiDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister callback
*
* ERRNO:
* \is
* \i S_usbCbiUfiDevLib_NOT_REGISTERED
* Could not register the callback 
* \ie
*/

STATUS usbCbiUfiDynamicAttachUnregister
    (
    USB_UFI_ATTACH_CALLBACK callback,  /* callback to be unregistered  */
    pVOID arg                          /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    int status = S_usbCbiUfiDevLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
        {
        if ((callback == pRequest->callback) && (arg == pRequest->callbackArg))
	    {
	    /* We found a matching notification request. */

	    usbListUnlink (&pRequest->reqLink);

            /* Dispose of structure */

            OSS_FREE (pRequest);
	    status = OK;
	    break;
	    }
        pRequest = usbListNext (&pRequest->reqLink);
        }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex);

    return (ossStatus (status));
    }


/***************************************************************************
*
* usbCbiUfiDevLock - Marks CBI_UFI_DEV structure as in use
*
* A caller uses usbCbiUfiDevLock() to notify usbCBiUfiDevLib that
* it is using the indicated CBI_UFI_DEV structure.  usbCBiUfiDevLib maintains
* a count of callers using a particular CBI_UFI_DEV structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* CBI_UFI_DEV is removed from the system.  So long as the "lock count"
* is greater than zero, usbCbiUfiDevLib will not dispose of an CBI_UFI_DEV
* structure.
*
* RETURNS: OK, or ERROR if unable to mark CBI_UFI_DEV structure in use
*
* ERRNO: none
*/

STATUS usbCbiUfiDevLock
    (
    USBD_NODE_ID nodeId    /* NodeId of the BLK_DEV to be marked as in use */
    )

    {
    pUSB_CBI_UFI_DEV pCbiUfiDev = usbCbiUfiDevFind (nodeId);

    if ( pCbiUfiDev == NULL)
        return (ERROR);

    pCbiUfiDev->lockCount++;

    return (OK);
    }


/***************************************************************************
*
* usbCbiUfiDevUnlock - Marks CBI_UFI_DEV structure as unused.
*
* This function releases a lock placed on an CBI_UFI_DEV structure.  When a
* caller no longer needs an CBI_UFI_DEV structure for which it has previously
* called usbCbiUfiDevLock(), then it should call this function to
* release the lock.
*
* NOTE: If the underlying CBI_UFI device has already been removed
* from the system, then this function will automatically dispose of the
* CBI_UFI_DEV structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the CBI_UFI_DEV structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark CBI_UFI_DEV structure unused
*
* ERRNO:
* \is
* \i S_usbCBiUfiDevLib_NOT_LOCKED
* No lock to Unlock
* \ie
*/

STATUS usbCbiUfiDevUnlock
    (
    USBD_NODE_ID nodeId    /* NodeId of the BLK_DEV to be marked as unused */
    )

    {
    int status = OK;
    pUSB_CBI_UFI_DEV pCbiUfiDev = usbCbiUfiDevFind (nodeId);
 
    if ( pCbiUfiDev == NULL)
        return (ERROR);

    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);

    if (pCbiUfiDev->lockCount == 0)
        {
        status = S_usbCBiUfiDevLib_NOT_LOCKED;
        }
    else
    {
    /* If this is the last lock and the underlying CBI_UFI device is
     * no longer connected, then dispose of the device.
     */

    if ((--pCbiUfiDev->lockCount == 0) && (!pCbiUfiDev->connected))
	usbCbiUfiDevDestroy (pCbiUfiDev);
    }

    OSS_MUTEX_RELEASE (cbiUfiDevMutex);

    return (ossStatus (status));
    }

/***************************************************************************
*
* usbCbiUfiDescShow - displays configuration, device and interface descriptors
*
* Displays all the descriptors for specifed <nodeId>
*
* RETURNS: OK on success or ERROR is unable to read descriptors
*
* ERRNO: none
*
*\NOMANUAL 
*/

LOCAL STATUS usbCbiUfiDescShow 
    (
    USBD_NODE_ID nodeId           /* node ID of the device        */
    )
    {

    UINT8    bfr[255];            /* store for descriptors         */
    UINT8    index;               
    UINT8    numCfg;              /* number of configuration       */
    UINT8    mfcIndex;            /* index for manufacturer string */
    UINT8    productIndex;        /* index for product string      */
    UINT16   actLength;           /* actual length of descriptor   */
    pUSB_DEVICE_DESCR  pDevDescr; /* pointer to device descriptor  */
    pUSB_STRING_DESCR  pStrDescr; /* pointer to string descriptor  */   

    /* Get the Device descriptor first */

    if (usbdDescriptorGet (usbdHandle, nodeId, 
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_DEVICE, 0, 0, 20, bfr, &actLength) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDescShow: Failed to read Device descriptor\n",
                         0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    if ((pDevDescr = usbDescrParse (bfr, actLength, USB_DESCR_DEVICE)) == NULL)
        {
         USB_CBI_UFI_ERR ("usbCbiUfiDescShow: No Device descriptor was found "\
                          "in the buffer \n", 0, 0, 0, 0, 0, 0);
         return (ERROR);
        }

    /* store the num. of configurations, string indices locally */

    numCfg       = pDevDescr->numConfigurations;
    mfcIndex     = pDevDescr->manufacturerIndex;
    productIndex = pDevDescr->productIndex;

    printf ("DEVICE DESCRIPTOR:\n");
    printf ("------------------\n");
    printf (" Length                    = 0x%x\n", pDevDescr->length);
    printf (" Descriptor Type           = 0x%x\n", pDevDescr->descriptorType);
    printf (" USB release in BCD        = 0x%x\n", pDevDescr->bcdUsb);
    printf (" Device Class              = 0x%x\n", pDevDescr->deviceClass);
    printf (" Device Sub-Class          = 0x%x\n", pDevDescr->deviceSubClass);
    printf (" Device Protocol           = 0x%x\n", pDevDescr->deviceProtocol);
    printf (" Max Packet Size           = 0x%x\n", pDevDescr->maxPacketSize0);
    printf (" Vendor ID                 = 0x%x\n", pDevDescr->vendor);
    printf (" Product ID                = 0x%x\n", pDevDescr->product);
    printf (" Dev release in BCD        = 0x%x\n", pDevDescr->bcdDevice);
    printf (" Manufacturer              = 0x%x\n", pDevDescr->manufacturerIndex);
    printf (" Product                   = 0x%x\n", pDevDescr->productIndex);
    printf (" Serial Number             = 0x%x\n", 
            pDevDescr->serialNumberIndex);
    printf (" Number of Configurations  = 0x%x\n\n", 
            pDevDescr->numConfigurations);

    /* get the manufacturer string descriptor  */     

    if (usbdDescriptorGet (usbdHandle, nodeId, 
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_STRING , mfcIndex,
                           0x0409, 100, bfr, &actLength) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDescShow: Failed to read String descriptor\n",
                         0, 0, 0, 0, 0, 0);
        }
    else
        {

        if ((pStrDescr = usbDescrParse (bfr, actLength, 
                                        USB_DESCR_STRING)) == NULL)
            {
            USB_CBI_UFI_ERR ("usbCbiUfiDescShow: No String descriptor was "\
                             "found in the buffer \n", 0, 0, 0, 0, 0, 0);
            }
        else
            {
            printf ("STRING DESCRIPTOR : %d\n",1);
            printf ("----------------------\n");
            printf (" Length              = 0x%x\n", pStrDescr->length);
            printf (" Descriptor Type     = 0x%x\n", pStrDescr->descriptorType); 
            printf (" Manufacturer String = ");  
            for (index = 0; index < (pStrDescr->length -2) ; index++)
                printf("%c", pStrDescr->string [index]);
            printf("\n\n");
            }
        }  

    /* get the product string descriptor  */

    if (usbdDescriptorGet (usbdHandle, nodeId, 
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_STRING , productIndex,
                           0x0409, 100, bfr, &actLength) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDescShow: Failed to read String descriptor\n", 
                         0, 0, 0, 0, 0, 0);
        }
    else
        {

        if ((pStrDescr = usbDescrParse (bfr, actLength, 
                                        USB_DESCR_STRING)) == NULL)
            {
            USB_CBI_UFI_ERR ("usbCbiUfiDescShow: No String descriptor was "\
                             "found in the buffer \n", 0, 0, 0, 0, 0, 0);
            }
        else
            {
            printf ("STRING DESCRIPTOR : %d\n",2);
            printf ("----------------------\n");
            printf (" Length              = 0x%x\n", pStrDescr->length);
            printf (" Descriptor Type     = 0x%x\n", pStrDescr->descriptorType); 
            printf (" Product String      = ");  

            for (index = 0; index < (pStrDescr->length -2) ; index++)
                printf("%c", pStrDescr->string [index]);

            printf("\n\n");
            }
        }

    /* get the configuration descriptors one by one for each configuration */

    for (index = 0; index < numCfg; index++) 
       if (usbCbiUfiConfigDescShow ( nodeId, index ) == ERROR)
          return (ERROR);

    return  (OK);

    }


/***************************************************************************
*
* usbCbiUfiConfigDescShow - shows configuration and its interface descriptors
*
* Displays the Configuration Descriptor for specified <nodeId> with 
* configuration index <cfgIndex>
*
* RETURNS: OK on success or ERROR is unable to read descriptors.
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiConfigDescShow 
    (
    USBD_NODE_ID nodeId,         /* node ID of the device       */
    UINT8 cfgIndex               /* index of configuration      */
    )
    {

    UINT8    bfr[255];           /* store for config descriptor */
    UINT8  * pBfr = bfr;         /* pointer to the above store  */
    UINT8    ifIndex;            /* index for interfaces        */
    UINT8    epIndex;            /* index for endpoints         */
    UINT16   actLength;          /* actual length of descriptor */

    pUSB_CONFIG_DESCR pCfgDescr;    /* pointer to config'n descriptor   */
    pUSB_INTERFACE_DESCR pIntDescr; /* pointer to interface descriptor  */ 
    pUSB_ENDPOINT_DESCR pEndptDescr;/* pointer to endpointer descriptor */ 
 
    /* get the configuration descriptor */

    if (usbdDescriptorGet (usbdHandle, nodeId, 
                           USB_RT_STANDARD | USB_RT_DEVICE,
                           USB_DESCR_CONFIGURATION, cfgIndex, 0, 
                           255, bfr, &actLength) != OK)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiConfigDescShow: Failed to read Config'n "\
                         "descriptor\n", 0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

        
    if ((pCfgDescr = usbDescrParseSkip (&pBfr, &actLength, 
                                        USB_DESCR_CONFIGURATION)) == NULL)
        {
        USB_CBI_UFI_ERR ("usbCbiUfiConfigDescShow: No Config'n descriptor was found "\
                         "in the buffer \n", 0, 0, 0, 0, 0, 0);
        return (ERROR);
        }

    printf ("CONFIGURATION DESCRIPTOR: %d\n", cfgIndex);
    printf ("----------------------------\n");
    printf (" Length                    = 0x%x\n", pCfgDescr->length);
    printf (" Descriptor Type           = 0x%x\n", pCfgDescr->descriptorType);
    printf (" Total Length              = 0x%x\n", pCfgDescr->totalLength);
    printf (" Number of Interfaces      = 0x%x\n", pCfgDescr->numInterfaces);
    printf (" Configuration Value       = 0x%x\n", pCfgDescr->configurationValue);
    printf (" Configuration Index       = 0x%x\n", pCfgDescr->configurationIndex);
    printf (" Attributes                = 0x%x\n", pCfgDescr->attributes);
    printf (" Maximum Power             = 0x%x\n\n", pCfgDescr->maxPower);

    /* Parse for the interface and its related endpoint descriptors */   

    for (ifIndex = 0; ifIndex < pCfgDescr->numInterfaces; ifIndex++)
        {

        if ((pIntDescr = usbDescrParseSkip (&pBfr, &actLength, 
                                            USB_DESCR_INTERFACE)) == NULL)
            {
            USB_CBI_UFI_ERR ("usbCbiUfiConfigDescShow: No Interface descriptor "\
                             "was found \n", 0, 0, 0, 0, 0, 0);
            return (ERROR);
            }

        printf ("INTERFACE DESCRIPTOR: %d\n", ifIndex);
        printf ("-----------------------\n");
        printf (" Length                    = 0x%x\n", pIntDescr->length);
        printf (" Descriptor Type           = 0x%x\n", pIntDescr->descriptorType);
        printf (" Interface Number          = 0x%x\n", pIntDescr->interfaceNumber);
        printf (" Alternate setting         = 0x%x\n", 
                pIntDescr->alternateSetting);
        printf (" Number of Endpoints       = 0x%x\n", pIntDescr->numEndpoints) ;
        printf (" Interface Class           = 0x%x\n", 
                pIntDescr->interfaceClass);
        printf (" Interface Sub-Class       = 0x%x\n", 
                pIntDescr->interfaceSubClass);
        printf (" Interface Protocol        = 0x%x\n", 
                pIntDescr->interfaceProtocol);
        printf (" Interface Index           = 0x%x\n\n", pIntDescr->interfaceIndex); 
        
        for ( epIndex = 0; epIndex < pIntDescr->numEndpoints; epIndex++)
            { 
            if ((pEndptDescr = usbDescrParseSkip (&pBfr, &actLength, 
                                                  USB_DESCR_ENDPOINT)) == NULL)
                {
                USB_CBI_UFI_ERR ("usbCbiUfiConfigDescShow: No Endpoint descriptor" \
                                 "was found \n", 0, 0, 0, 0, 0, 0);
                return (ERROR);
                }

            printf ("ENDPOINT DESCRIPTOR: %d\n", epIndex);
            printf ("-----------------------\n");
            printf (" Length                    = 0x%x\n", pEndptDescr->length);
            printf (" Descriptor Type           = 0x%x\n", 
                    pEndptDescr->descriptorType);
            printf (" Endpoint Address          = 0x%x\n", 
                    pEndptDescr->endpointAddress);
            printf (" Attributes                = 0x%x\n", 
                    pEndptDescr->attributes);
            printf (" Max Packet Size           = 0x%x\n", 
                    pEndptDescr->maxPacketSize);
            printf (" Interval                  = 0x%x\n\n", 
                    pEndptDescr->interval);
            }

        }
  
    return (OK);
    }

/***************************************************************************
*
* usbCbiUfiDevTestReady - function to check device status
*
* This function checks whether the device is ready
*
* RETURNS: OK if command successful, ERROR if failed 
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL STATUS usbCbiUfiDevUnitReady 
    ( 
    BLK_DEV * pBlkDev             /* pointer to bulk device */
    )
    {
    USB_COMMAND_STATUS status;
    UINT8              requestSense[20]; /* store for REQUEST SENSE data  */
    pUSB_CBI_UFI_DEV      pCbiUfiDev = (USB_CBI_UFI_DEV *)pBlkDev;    
    unsigned int usbCbiUfiIrpTimeOut = 0;
  
    OSS_MUTEX_TAKE (cbiUfiDevMutex, OSS_BLOCK);

    /* 
     * increase IRP timeout in case device changes 
     */

    usbCbiUfiIrpTimeOut = (USB_CBI_IRP_TIME_OUT * 4);

    /* make a SCSI Test Unit Ready command */ 
    if (usbCbiUfiFormCmd (pCbiUfiDev, USB_UFI_TEST_UNIT_READY, 
			    0, 0) != OK )
        {
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR); 
        } 

    status = usbCbiUfiCmdExecute (pCbiUfiDev);

    usbCbiUfiIrpTimeOut = USB_CBI_IRP_TIME_OUT;

    if (status == USB_COMMAND_FAILED )
        {
        /* get request sense data.*/
        pCbiUfiDev->bulkInData = requestSense;
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR);
        }
    else if (status != USB_COMMAND_SUCCESS)
        {
        OSS_MUTEX_RELEASE (cbiUfiDevMutex);
        return (ERROR);
        }   

    /* device is ready */
    OSS_MUTEX_RELEASE (cbiUfiDevMutex);

    return (OK);
    }



/***************************************************************************
*
* usbCbiUfiDevTestReady - function to check device status
*
* This function checks whether the device is ready
*
* RETURNS: 
*  CBI_TEST_READY_NOCHANGE if no media change detected
*  CBI_TEST_READY_CHANGED  if media change detected
*  CBI_TEST_READY_FAILED   if failed to checked ready status
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL UINT8 usbCbiUfiDevTestReady
    ( 
    BLK_DEV *pBlkDev
    )
    {
    pUSB_CBI_UFI_DEV pCbiUfiDev     = (USB_CBI_UFI_DEV *)pBlkDev;
    UINT8            status         = CBI_TEST_READY_CHANGED;
    STATUS           rtnStatus      = ERROR;
    int              retry          = 0;
    int              ready          = ERROR;
    int              changeDetected = FALSE;
    int              noMedium       = 0;
    int              unknownFlag    = 0;

    do 
        {
 	ready = 0;
	rtnStatus = usbCbiUfiDevUnitReady((BLK_DEV*)pCbiUfiDev);
	if (rtnStatus !=OK) 
            {
            USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady:Device still not ready  problen sending ready cmd \n", 0,0,0,0,0,0);	
	    }		

	ready = usbCbiUfiReqSense(pCbiUfiDev);
	if (ready == OK) 
            {
            USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady: Device is ready", 0,0,0,0,0,0);
            break;
            }

        switch (ready) 
            {
            case USB_UFI_INITIALIZATION_REQUIRED:
                changeDetected = TRUE;
                noMedium       = 0;
                USB_CBI_UFI_DEBUG("usbCbiUfiDevTestReady: initialization required... retrying\n", 0,0,0,0,0,0);
                break;
            case USB_UFI_FORMAT_IN_PROGRESS:
                noMedium       = 0;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:Format in progress... retrying\n",0,0,0,0,0,0);
                break;
            case USB_UFI_DEVICE_IS_BUSY:
                noMedium       = 0;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:Device is busy... retrying\n",0,0,0,0,0,0);
                break;
            case USB_UFI_MEDIUM_NOT_PRESENT:
                noMedium ++;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:medium not present... retrying\n",0,0,0,0,0,0);
                break;
            case USB_UFI_MEDIA_CHANGE:
                noMedium       = 0;
                changeDetected = TRUE;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady: medium changed... retrying\n",0,0,0,0,0,0);
                break;
            case USB_UFI_POWER_ON_RESET:
                noMedium       = 0;
                changeDetected = TRUE;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady: Powered up and reset... retrying\n",0,0,0,0,0,0);
                break;
            case USB_UFI_UNKNOWN_ERROR:
                noMedium       = 0;
                changeDetected = TRUE;
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:unknown sense key received 0x%x... retrying\n",ready,0,0,0,0,0);
                if (pCbiUfiDev->blkDev.bd_removable == TRUE)
                    pCbiUfiDev->blkDev.bd_readyChanged = TRUE;

                USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady:Here we have the error because we tried to access some inexistant node, or not yet existing node. this has to be corrected!\n",ready, 0,0,0,0,0);
                status = CBI_TEST_READY_FAILED;

                /* If we encounter this unknown error more than twice, then take drastic
                 * action and re-create the in-bound and out-bound pipes 
                 */
                unknownFlag++;
                if (unknownFlag > 2)
                    {
                    USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: Destroying Pipes n",0, 0,0,0,0,0);
                    noMedium = 11;

                    if ((pCbiUfiDev->inPipeHandle != NULL) && (usbdPipeDestroy (usbdHandle, pCbiUfiDev->inPipeHandle) == OK))
                        {

                        pCbiUfiDev->inPipeHandle = NULL;

                        /* Create a Bulk-in pipe for the USB_CBI_UFI_DEV device */

                        if (usbdPipeCreate (usbdHandle, pCbiUfiDev->cbiUfiDevId,
                                            pCbiUfiDev->inEpAddress, pCbiUfiDev->configuration,
                                            pCbiUfiDev->interface, USB_XFRTYPE_BULK, USB_DIR_IN,
                                            FROM_LITTLEW(pCbiUfiDev->inEpAddressMaxPkt), 0, 0,
                                            &(pCbiUfiDev->inPipeHandle)) != OK)
                            {
                            USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: Error creating bulk in pipe %x \n",
                                         (int)pCbiUfiDev->inPipeHandle, 0, 0, 0, 0, 0);
                            }
                        else
                            USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: Successfully created bulk in pipe\n",
                                         0, 0, 0, 0, 0, 0);
                        }

                    if ((pCbiUfiDev->outPipeHandle != NULL) && (usbdPipeDestroy (usbdHandle, pCbiUfiDev->outPipeHandle) == OK))
                        {
                        
                        pCbiUfiDev->outPipeHandle = NULL;
 
                        /* Create a Bulk-out pipe for the USB_CBI_UFI_DEV device */

                        if (usbdPipeCreate (usbdHandle, pCbiUfiDev->cbiUfiDevId,
                                            pCbiUfiDev->outEpAddress, pCbiUfiDev->configuration,
                                            pCbiUfiDev->interface, USB_XFRTYPE_BULK, USB_DIR_OUT,
                                            FROM_LITTLEW(pCbiUfiDev->outEpAddressMaxPkt), 0, 0,
                                            &(pCbiUfiDev->outPipeHandle)) != OK)
                            {  
                            USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: Error creating bulk out pipe %x\n",
                                         (int)pCbiUfiDev->outPipeHandle, 0, 0, 0, 0, 0);
                            }           
                        else
                            USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: Successfully created bulk out pipe\n",
                                         0, 0, 0, 0, 0, 0);
                        }

                    }
 
                break;
            default:
                USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:other sense key received 0x%x... retrying\n",ready,0,0,0,0,0);
		break;
            }

        if ( noMedium > 10 )
            retry = CBI_TEST_READY_MXRETRIES;


        retry++;
        taskDelay ((sysClkRateGet() * 20) / 200);
	} while (retry < CBI_TEST_READY_MXRETRIES);

    if (retry >= CBI_TEST_READY_MXRETRIES) 
        {
        USB_CBI_UFI_ERR ("usbCbiUfiDevTestReady: dev test failed", 
                         0, 0, 0, 0, 0, 0);
        if (pCbiUfiDev->blkDev.bd_removable == TRUE)
            pCbiUfiDev->blkDev.bd_readyChanged = TRUE;        
 
        USB_CBI_UFI_ERR ("\n returning error!",0,0,0,0,0,0);               
        status = CBI_TEST_READY_FAILED;
	}

    /* didn't detect a change anyway*/
    else if (changeDetected == FALSE) 
        {
        USB_CBI_UFI_DEBUG ("usbCbiUfiDevTestReady:No change detected, returning OK!\n",0,0,0,0,0,0);
        status = CBI_TEST_READY_NOCHANGE;
        }

    /* Otherwise, we've detected a change in the media */
    else
        {
        status = CBI_TEST_READY_CHANGED;
        }

    return(status);
    }



/* usbTransUnitInit.c - Translation Unit Initialization interfaces */

/* Copyright 2003 Wind River Systems, Inc. */

/*
Modification history
--------------------
01k,15oct04,ami  Apigen Changes
01l,07oct04,hch  Merge from Bangalore developement branch for SPR# 96484
01k,16sep04,hch  Fix diab compiler error
01j,06oct04,ami  Removal of warning messages related to SPR #94684 Fix
01i,14sep04,gpd  memory leak fixes
01h,12apr04,cfc  Apigen fixes
01g,02dec03,cfc  Merge to support isochronous USB speakers
01f,17nov03,sm   correcting refgen errors.
01e,06nov03,cfc  Modification to support the USBD_NOTIFY_ALL from class
                 drivers
01d,01oct03,cfc  Merge from Bangalore fix for dual class drivers
01c,21sep03,cfc  Fix compiler warnings
01b,17sep03,cfc  Remove direct calls to wvEvent()
01a,11sep03,cfc  Fix task creation priorities
o1d,10jul03,mat  Set Configuration Fix
01c,16jun03,mat  prefixed "usb" to file name ,global variables etc
01b,06jun03,mat  Wind View Instrumentation.
01a,06jun03,mat  Wind River Coding Convention  API changes.
*/

/*
DESCRIPTION

Implements the Translation Unit Initialization interfaces.


In order to use the USBD, it is first necessary to invoke usbdInitialize().
Multiple calls to usbdInitialize() may be nested so long as a corresponding
number of calls to usbdShutdown() are also made.  This allows multiple USBD
clients to be written independently and without concern for coordinating the
initialization of the independent clients.

Normal USBD clients  register with the USBD by calling usbdClientRegister().
In response to this call, the Translation Unit allocates per-client data
structures and a client callback task.
Callbacks for each client are invoked from this client-unique task.  This
improves the USBD's ability to shield clients from one-another and to help
ensure the real-time response for all clients.

After a client has registered, it will most often also register for dynamic
attachment notification using usbdDynamicAttachRegister().  This function
allows a special client callback routine to be invoked each time a USB device
is attached or removed from the system.  In this way, clients may discover the
real-time attachment and removal of devices.

INCLUDE FILES: usbTransUnit.h
*/


/* includes */

#include "drv/usb/usbTransUnit.h" 
#include "usb2/usbHcdInstr.h"

/* defines */

#define USBTUINIT_MAX_MESSAGES   100     /* Maximum messages queued in message q */


/* globals */

/* Wind View Events Filter variable
 * value of 0 disables event capture
 */

UINT32 usbtuInitWvFilter= 0x00;  

/* list of clients registered with USBD */

LIST_HEAD     usbtuClientList;   

/* list of devices attached to USB */

LIST_HEAD     usbtuDevList;      

/* thread id of Translation Unit thread */

THREAD_HANDLE usbtuThreadId;     

/* message queue id of Translation Unit */

MSG_Q_ID      usbtuMsgQid;       

/* mutex of Translation Unit */

MUTEX_HANDLE  usbtuMutex;        


/* locals */

LOCAL int usbtuInitCount = 0;		/* init nesting count */
LOCAL BOOL usbtuInitOssInit = FALSE;	/* TRUE if ossLib is initialized  */



/* forward declarations */

VOID usbtuInitThreadFn( pVOID param );
VOID usbtuInitClientThreadFn(pVOID driverParam);
VOID usbtuInitClientIrpCompleteThreadFn(pVOID driverParam);
USBHST_STATUS usbtuInitDeviceAdd(UINT32 hDevice, UINT8 interfaceNumber,
                              UINT8 uSpeed, void** ppDriverData);
VOID usbtuInitDeviceRemove(UINT32 hDevice, PVOID pDriverData);
VOID usbtuInitDeviceSuspend(UINT32 hDevice, PVOID ppSuspendData);
VOID usbtuInitDeviceResume(UINT32 hDevice, PVOID pSuspendData);

/***************************************************************************
*
* usbdInitialize - Initialize the USBD
*
* usbdInitialize() must be called at least once prior to calling other
* USBD functions.  usbdInitialize() prepares the USBD and Translation
* unit to process URBs.
* Calls to usbdInitialize() may be nested, allowing multiple USBD clients
* to be written independently.
*
* RETURNS: OK, or ERROR if initialization failed.
*
* ERRNO: N/A
*/

STATUS usbdInitialize (void)
    {
    
    
    USBTU_LOG ( "usbdInitialize entered \n ");
 
    
    
      
    /* If not already initialized... */

    if (++usbtuInitCount == 1)
        {

        /* Wind View Instrumentation */

        if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
            {
            pCHAR evLog = " USBD Initialization ";
            USB_HCD_LOG_EVENT(USBTU_WV_USBD_INIT, evLog, USBTU_WV_FILTER); 
            } 


        if (ossInitialize () != OK)
            {
            USBTU_LOG ("usbdInitialize returns ERROR:ossInitialize failed \n");
            return ERROR;
            } 
       
	    usbtuInitOssInit = TRUE;

       

        usbdInit();

        /* create the Translation Unit Mutex */

        if (OSS_MUTEX_CREATE (&usbtuMutex) != OK)
            {
            USBTU_LOG ( "usbdInitialize returns ERROR : mutex create failed \n ");
            return ERROR;
            }

        /* create the Translation Unit Message Queue */

        if (!(usbtuMsgQid = msgQCreate (USBTUINIT_MAX_MESSAGES,
                                        sizeof(PVOID), MSG_Q_FIFO )))
            {
            OSS_MUTEX_DESTROY(usbtuMutex);
            USBTU_LOG ( "usbdInitialize returns ERROR: msq q create failed\n ");              
            return ERROR;
            }  

        /* create the Translation Unit Thread */

        if (OSS_THREAD_CREATE (usbtuInitThreadFn, NULL, OSS_PRIORITY_TYPICAL,
                               NULL, &usbtuThreadId) !=OK)
            {
            OSS_MUTEX_DESTROY(usbtuMutex);
            msgQDelete (usbtuMsgQid);
            USBTU_LOG ( "usbdInitialize returns ERROR : thread create failed \n ");               
            return ERROR;
            } 
        }

    USBTU_LOG ( "usbdInitialize  returns OK\n ");
    
    return OK;
    }


/***************************************************************************
*
* usbdShutdown - Shuts down the USBD
*
* usbdShutdown() should be called once for every successful call to
* usbdInitialize().  This function frees memory and other resources used
* by the USBD and Translation Unit.
*
* RETURNS: OK, or ERROR if shutdown failed.
*
* ERRNO: N/A
*/

STATUS usbdShutdown (void)
    {
    
    STATUS s = OK;
    USBTU_LOG ( "usbdShutdown entered \n ");

    

    if (usbtuInitCount == 0)
        {

        /* not initialized */ 

        USBTU_LOG ( "usbdShutdown returns OK \n");
        return OK;
        }

    /* We've been initialized at least once. */

    if (usbtuInitCount == 1)
        {


        /* Wind View Instrumentation */

        if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
            {
            pCHAR evLog = " USBD Shutdown ";
            USB_HCD_LOG_EVENT(USBTU_WV_USBD_INIT, evLog, USBTU_WV_FILTER);            } 


        if (usbtuInitOssInit)
            {
            /* Shut down osServices library */

            ossShutdown ();
            usbtuInitOssInit = FALSE;
            }

        

        usbdExit();

        /* destroy Translation Unit Mutex */

        if ( usbtuMutex )
            {
            if ( OSS_MUTEX_DESTROY (usbtuMutex) != OK )
                {
                USBTU_LOG ("usbdShutdown  ERROR:mutex destoy failed\n");
                s = ERROR;
                }
            else
                usbtuMutex = NULL;
            }

        /* destroy Translation Unit thread */
        

        if ( usbtuThreadId )
            {
            if ( OSS_THREAD_DESTROY (usbtuThreadId) != OK )
                {
                USBTU_LOG ("usbdShutdown  ERROR:thread dest failed\n");
                s = ERROR;
                }
            else
                usbtuThreadId = NULL;
            }

        /* destroy Translation Unit message queue */

        if ( usbtuMsgQid )
            {
            if ( msgQDelete (usbtuMsgQid) != OK )
                {
                USBTU_LOG ("usbdShutdown  ERROR:message q dest failed\n");
                s = ERROR;
                }
            else
                usbtuMsgQid = NULL;
            }



        }

        --usbtuInitCount;

        if ( s == OK)
            {
            USBTU_LOG ( "usbdShutdown returns OK \n");
            }
        else
            {
            USBTU_LOG ( "usbdShutdown returns ERROR \n");
            }
        return s;
    }

/***************************************************************************
*
* usbdClientRegister - Registers a new client with the USBD
*
* This routine invokes the USBD function to register a new client.
* <pClientName> should point to a string of not more than USBD_NAME_LEN
* characters (excluding terminating NULL) which can be used to uniquely
* identify the client.	If successful, upon return the <pClientHandle>
* will be filled with a newly assigned USBD_CLIENT_HANDLE.
*
* RETURNS: OK, or ERROR if unable to register new client.
*
*
* ERRNO: N/A
*/

STATUS usbdClientRegister
    (
    pCHAR pClientName,			/* Client name */
    pUSBD_CLIENT_HANDLE pClientHandle	/* Client hdl returned by USBD */
    )

    {

    pUSBTU_DEVICE_DRIVER pDriverData;
    char pIrpCompleteThreadName[USBD_NAME_LEN];

    USBTU_LOG ( "usbdClientRegister entered \n");

    /* Zero out the IRP thread name */
    memset(pIrpCompleteThreadName,0,USBD_NAME_LEN);


    /* Wind View Instrumentation */
    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, pClientName,USBD_NAME_LEN );
        strcat(evLog, " : USBD Register ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
        }


    if ( !usbtuInitCount)
        {

        /* usbdInitialize() not called */

        USBTU_LOG("usbdClientRegister returns ERROR:usbdInitialize not called\n");
        return ERROR;
        }

    /* allocate structure for client */

    if ( !(pDriverData = OSS_CALLOC ( sizeof (USBTU_DEVICE_DRIVER))))
        {
        USBTU_LOG ( "usbdClientRegister returns ERROR : malloc failed \n");
        return ERROR;
        }



    /* store name */

    if (pClientName != NULL)
        {
        strncpy ((char *)(pDriverData->clientName), pClientName, USBD_NAME_LEN);
        sprintf(pIrpCompleteThreadName,"%s_IRP",pClientName);
        }



    /* create message queue for client */

    if (!(pDriverData->msgQid = msgQCreate (USBTUINIT_MAX_MESSAGES,
                                            sizeof(PVOID),
                                            MSG_Q_FIFO )))
        {
        OSS_FREE (pDriverData);
        USBTU_LOG("usbdClientRegister returns ERROR:message q create failed\n");
        return ERROR;
        }

    /* create thread for client */

    if (OSS_THREAD_CREATE (usbtuInitClientThreadFn, pDriverData,
                           OSS_PRIORITY_TYPICAL,
                           pClientName,
                           &(pDriverData->threadHandle)
                          )!= OK )
        {
        msgQDelete(pDriverData->msgQid);
        OSS_FREE (pDriverData);
        USBTU_LOG("usbdClientRegister returns ERROR:thread create failed \n");
        return ERROR ;
        }

     /* create message queue for IrpComplete */

    if (!(pDriverData->msgQidIrpComplete = msgQCreate (USBTUINIT_MAX_MESSAGES,
                                            sizeof(PVOID),
                                            MSG_Q_FIFO )))
        {
        	
        OSS_FREE (pDriverData);
        USBTU_LOG("usbdClientRegister returns ERROR:message q create failed\n");
        return ERROR;
        }

    /* create thread for IrpComplete */

    if (OSS_THREAD_CREATE (usbtuInitClientIrpCompleteThreadFn, pDriverData,
                           OSS_PRIORITY_TYPICAL,
                           pIrpCompleteThreadName,
                           &(pDriverData->irpThreadHandle)
                          )!= OK )
        {
        msgQDelete(pDriverData->msgQidIrpComplete);
        OSS_FREE (pDriverData);
        USBTU_LOG("usbdClientRegister returns ERROR:thread create failed \n");
        return ERROR ;
        }


    /* Link structure to global list */

    OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

    usbListLink (&usbtuClientList, pDriverData,
                    &(pDriverData->tuDriverLink), LINK_TAIL);

    OSS_MUTEX_RELEASE (usbtuMutex);

    /* store Return result */

    if (pClientHandle != NULL)
        *pClientHandle = pDriverData;

    USBTU_LOG ( "usbdClientRegister returns OK \n");

    return OK;

    }


/***************************************************************************
*
* usbdClientUnregister - Unregisters a USBD client
*
* A client invokes this function to release a previously assigned
* USBD_CLIENT_HANDLE.  The USBD will release all resources allocated to
* the client, aborting any outstanding URBs which may exist for the client.
*
* Once this function has been called with a given clientHandle, the client
* must not attempt to reuse the indicated <clientHandle>.
*
* RETURNS: OK, or ERROR if unable to unregister client.
*
* ERRNO: N/A
*/

STATUS usbdClientUnregister
    (
    USBD_CLIENT_HANDLE clientHandle	/* Client handle */
    )

    {
    USBHST_STATUS status = USBHST_SUCCESS;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER) clientHandle;
    STATUS s = OK;

    USBTU_LOG ( "usbdClientUnRegister entered \n");


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog, (char *)(pDriver->clientName),USBD_NAME_LEN );
        strcat(evLog, " : USBD Unregister ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
        }


    if ( !clientHandle)
        {
        USBTU_LOG("usbdClientUnRegister returns ERROR : NULL client handle \n");
        return ERROR;
        }

    /*  unregister the client */

    if (pDriver->pDriverData)
        {
        status = usbHstDriverDeregister (pDriver->pDriverData);
        OSS_FREE (pDriver->pDriverData);
        }

    /* destroy the client thread */

    if (pDriver->threadHandle)
        if (OSS_THREAD_DESTROY (pDriver->threadHandle) != OK)
            {
            USBTU_LOG("usbdClientUnRegister returns ERROR:thread destroy failed\n");
            s = ERROR;
            }

    /* destroy the client message queue */

    if (pDriver->msgQid)
        if (msgQDelete (pDriver->msgQid) != OK )
            {
            USBTU_LOG("usbdClientUnRegister returns ERROR : message queue \
                       destroy failed \n");
            s = ERROR;
            }

    /* destroy the irpComplete thread */
    if (pDriver->irpThreadHandle)
        if (OSS_THREAD_DESTROY (pDriver->irpThreadHandle) != OK)
            {
            USBTU_LOG("usbdClientUnRegister returns ERROR:thread destroy failed\n");
            s = ERROR;
            }

    /* destroy the irpComplete message queue */

    if (pDriver->msgQidIrpComplete)
        if (msgQDelete (pDriver->msgQidIrpComplete) != OK )
            {
            USBTU_LOG("usbdClientUnRegister returns ERROR : message queue \
                       destroy failed \n");
            s = ERROR;
            }


    /* Unlink client structure */

    OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

    usbListUnlink (&pDriver->tuDriverLink);

    OSS_MUTEX_RELEASE(usbtuMutex);

    /* free the client structure */

    OSS_FREE (pDriver);

    if ( status == USBHST_SUCCESS && s == OK )
        {
        USBTU_LOG ( "usbdClientUnRegister returns OK \n");
        return OK;
        }
    else
        {
        USBTU_LOG ( "usbdClientUnRegister returns ERROR \n");
        return ERROR;
        }

    }


/***************************************************************************
*
* usbdMngmtCallbackSet - sets management callback for a client
*
* Management callbacks provide a mechanism for the USBD to inform clients
* of asynchronous management events on the USB.  For example, if the USB
* is in the SUSPEND state - see usbdBusStateSet() - and a USB device
* drives RESUME signalling, that event can be reported to a client through
* its management callback.
*
* <clientHandle> is a client's registered handled with the USBD.
* <mngmtCallback> is the management callback routine of type
* USBD_MNGMT_CALLBACK which will be invoked by the USBD when management
* events are detected.	<mngmtCallbackParam> is a client-defined parameter
* which will be passed to the <mngmtCallback> each time it is invoked.
* Passing a <mngmtCallback> of NULL cancels management event callbacks.
*
* When the <mngmtCallback> is invoked, the USBD will also pass it the
* USBD_NODE_ID of the root node on the bus for which the management event
* has been detected and a code signifying the type of management event
* as USBD_MNGMT_xxxx.
*
* Clients are not required to register a management callback routine.
* Clients that do use a management callback are permitted to register at
* most one management callback per USBD_CLIENT_HANDLE.
*
* RETURNS: OK, or ERROR if unable to register management callback
*
* ERRNO: N/A
*/

STATUS usbdMngmtCallbackSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_MNGMT_CALLBACK mngmtCallback,	/* management callback */
    pVOID mngmtCallbackParam		/* client-defined parameter */
    )

    {
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER)clientHandle;


    USBTU_LOG ( "usbdMngmtCallbackSet entered \n");


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog,(char *)(pDriver->clientName),USBD_NAME_LEN );
        strcat(evLog, " : Management Callback Register ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
        }


    /* If management callback already registered return ERROR */

    if (pDriver->mngmtCallback && mngmtCallback != NULL)
        {
        USBTU_LOG("usbdMngmtCallbackSet returns ERROR : callback already set\n");
        return ERROR;
        }

    /* register management callback */

    pDriver->mngmtCallback = mngmtCallback;
    pDriver->mngmtCallbackParam = mngmtCallbackParam;

    USBTU_LOG ( "usbdMngmtCallbackSet returns OK \n");
    return OK;

    }


/***************************************************************************
*
* usbdBusStateSet - Sets bus state (e.g., suspend/resume)
*
* This function allows a client to set the state of the bus to which
* the specified <nodeId> is attached.  The desired <busState> is specified
* as USBD_BUS_xxxx.
*
* Typically, a client will use this function to set a bus to the SUSPEND
* or RESUME state.  Clients must use this capability with care, as it will
* affect all devices on a given bus - and hence all clients communicating
* with those devices.
*
*
* RETURNS: OK, or ERROR if unable to set specified bus state
*
* ERRNO: N/A
*/

STATUS usbdBusStateSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* node ID */
    UINT16 busState			/* new bus state: USBD_BUS_xxxx */
    )

    {
    /* This functionality is not yet provided */
#if 0
    USBHST_STATUS status;
    USBD_NODE_ID rootId;
    USBD_NODE_INFO   nodeInfo;

    USBTU_LOG ( "usbdBusStateSet entered \n");

    /* get the root id */

    usbdNodeInfoGet (clientHandle, nodeId, &nodeInfo, sizeof (USBD_NODE_INFO));
    rootId = nodeInfo.rootId;

    switch( busState )
        {
        case USBD_BUS_SUSPEND :
            status = usbHstSelectiveSuspend ((UINT32)rootId);
            break;
        case USBD_BUS_RESUME  :
            status = usbHstSelectiveResume ((UINT32)rootId);
            break;
        default :
            break;
        }

    if ( status != USBHST_SUCCESS)
        {
        USBTU_LOG ( "usbdBusStateSet returns ERROR \n");
        return ERROR;
        }
    else
        {
        USBTU_LOG ( "usbdBusStateSet returns OK \n");
        return OK;
        }
#endif
    return OK;
    }



/***************************************************************************
*
* usbdDynamicAttachRegister - Registers client for dynamic attach notification
*
* Clients call this function to indicate to the USBD that they wish to be
* notified whenever a device of the indicated class/sub-class/protocol is
* attached or removed from the USB.  A client may specify that it wants to
* receive notification for an entire device class or only for specific
* sub-classes within that class.
*
* <deviceClass>, <deviceSubClass>, and <deviceProtocol> must specify a USB
* class/sub-class/protocol combination according to the USB specification.  For
* the client’s convenience, usbdLib.h automatically includes usb.h which defines
* a number of USB device classes as USB_CLASS_xxxx and USB_SUBCLASS_xxxx.  A
* value of USBD_NOTIFY_ALL in any/all of these parameters acts like a wildcard
* and matches any value reported by the device for the corresponding field.
*
* <attachCallback> must be a non-NULL pointer to a client-supplied callback
* routine of the form USBD_ATTACH_CALLBACK:
*
* \cs
* typedef VOID (*USBD_ATTACH_CALLBACK)
*     (
*     USBD_NODE_ID nodeId,
*     UINT16 attachAction,
*     UINT16 configuration,
*     UINT16 interface,
*     UINT16 deviceClass,
*     UINT16 deviceSubClass,
*     UINT16 deviceProtocol
*     );
* \ce
*
* Immediately upon registration the client should expect that it may begin
* receiving calls to the <attachCallback> routine.  Upon registration,
* Translation Unit will call the <attachCallback> for each device of the
* specified class which is already attached to the system.  Thereafter, the
* Translation unit will call the <attachCallback> whenever a new device of the
* specified class is attached to the system or an already-attached device is
* removed.
*
* Each time the <attachCallback> is called, Translation Unit will pass the
* Node Id of the device in <nodeId> and an attach code in <attachAction> which
* explains the reason for the callback.  Attach codes are defined as:
*
* \is
* \i 'USBD_DYNA_ATTACH'
* USBD is notifying the client that nodeId is a device which is now attached
* to the system.
*
* \i 'USBD_DYNA_REMOVE'
* USBD is notifying the client that nodeId has been detached (removed) from
* the system.
* \ie
*
* When the <attachAction> is USBD_DYNA_REMOVE the <nodeId> refers to a Node Id
* which is no longer valid.  The client should interrogate its internal data
* structures and delete any references to the specified Node Id.  If the client
* had outstanding requests to the specified <nodeId>, such as data transfer
* requests, then the USBD will fail those outstanding requests prior to calling
* the <attachCallback> to notify the client that the device has been removed.
* In general, therefore, transfer requests related to removed devices should
* already be taken care of before the <attachCallback> is called.
*
* A client may re-use a single <attachCallback> for multiple notification
* registrations.  As a convenience to the <attachCallback> routine, the USBD
* also passes the <deviceClass>, <deviceSubClass>, and <deviceProtocol> of the
* attached/removed <nodeId> each time it calls the <attachCallback>.
*
* Finally, clients need to be aware that not all USB devices report their class
* information at the "device" level.  Rather, some devices report class types
* on an interface-by-interface basis.  When the device reports class information
* at the device level, then the USBD passes a <configuration> value of zero to
* the attach callback and calls the callback only a single time for each device.
* When the device reports class information at the interface level, then the
* Translation Unit invokes the attach callback once for each interface which
* matches the client's <deviceClass>/<deviceSubClass>/<deviceProtocol>
* specification.
* In this case, the USBD also passes the corresponding configuration & interface
* numbers in <configuration> and <interface> each time it invokes the callback.
*
* RETURNS: OK, or ERROR if unable to register for attach/removal notification.
*
* ERRNO: N/A
*/

STATUS usbdDynamicAttachRegister
    (

    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    UINT16 deviceClass, 		/* USB class code */
    UINT16 deviceSubClass,		/* USB sub-class code */
    UINT16 deviceProtocol,		/* USB device protocol code */
    USBD_ATTACH_CALLBACK attachCallback /* User-supplied callback  */
    )

    {
    pUSBHST_DEVICE_DRIVER pDriverData;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER)clientHandle;
    USBHST_STATUS status;

    USBTU_LOG ( "usbdDynamicAttachRegister entered \n");


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog,(char *)(pDriver->clientName),USBD_NAME_LEN );
        strcat(evLog, " : Dynamic Attach/Dettach Callback Register ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
        }


    /* allocate structure for driver specific data  */

    if ( !(pDriverData = OSS_CALLOC (sizeof (USBHST_DEVICE_DRIVER))))
        {
        USBTU_LOG ("usbdDynamicAttachRegister returns ERROR: alloc failed \n");
        return ERROR;
        }
    /* initialize structure with driver specific data */

    pDriverData->bFlagVendorSpecific = 0;
    pDriverData->uVendorIDorClass = deviceClass;
    pDriverData->uProductIDorSubClass = deviceSubClass;
    pDriverData->uBCDUSBorProtocol = deviceProtocol;
    pDriverData->addDevice = usbtuInitDeviceAdd;
    pDriverData->removeDevice = usbtuInitDeviceRemove;
    pDriverData->suspendDevice =  usbtuInitDeviceSuspend;
    pDriverData->resumeDevice = usbtuInitDeviceResume;

    /* register the client with USBD */

    status = usbHstDriverRegister (pDriverData, NULL);

    if (status != USBHST_SUCCESS)
        {

        /* release driver specific structure */

        OSS_FREE (pDriverData);
        USBTU_LOG ( "usbdDynamicAttachRegister returns ERROR \n");
        return ERROR;

        }

    /* initialize client structure */

    pDriver->class = deviceClass;
    pDriver->subclass = deviceSubClass;
    pDriver->protocol = deviceProtocol;
    pDriver->attachCallback = attachCallback;
    pDriver->pDriverData = pDriverData;

    USBTU_LOG ( "usbdDynamicAttachRegister returns OK \n");
    return OK;

    }


/***************************************************************************
*
* usbdDynamicAttachUnRegister - Unregisters client for attach notification
*
* This function cancels a client’s earlier request to be notified for the
* attachment and removal of devices within the specified class.  <deviceClass>,
* <deviceSubClass>, <deviceProtocol>, and <attachCallback> are defined as for
* the usbdDynamicAttachRegister() function and must match exactly the parameters
* passed in an earlier call to usbdDynamicAttachRegister.
*
* RETURNS: OK, or ERROR if unable to unregister for attach/removal notification.
*
* ERRNO: N/A
*/

STATUS usbdDynamicAttachUnRegister
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    UINT16 deviceClass, 		/* USB class code */
    UINT16 deviceSubClass,		/* USB sub-class code */
    UINT16 deviceProtocol,		/* USB device protocol code */
    USBD_ATTACH_CALLBACK attachCallback /* user-supplied callback routine */
    )

    {
    USBHST_STATUS status = USBHST_SUCCESS;
    pUSBTU_DEVICE_DRIVER pDriver = (pUSBTU_DEVICE_DRIVER)clientHandle;

    USBTU_LOG ( "usbdDynamicAttachUnRegister entered \n");


    /* Wind View Instrumentation */

    if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
        {
        char evLog[USBTU_WV_LOGSIZE];
        strncpy ((char*)evLog,(char *)(pDriver->clientName),USBD_NAME_LEN );
        strcat(evLog, " : Dynamic Attach/Dettach Callback Unregister ");
        USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
        }


    /* unregister */

    if ( pDriver->pDriverData )
        status = usbHstDriverDeregister (pDriver->pDriverData);

    if ( status != USBHST_SUCCESS)
        {
        USBTU_LOG ( "usbdDynamicAttachUnRegister returns ERROR \n");
        return ERROR;
        }
    else
        {
        /* reset client structure members  */

        pDriver->attachCallback = NULL;
        OSS_FREE (pDriver->pDriverData);
        pDriver->pDriverData = NULL;
        }

    USBTU_LOG ( "usbdDynamicAttachUnRegister returns OK \n");

    return OK;
    }

/***************************************************************************
*
* usbtuInitThreadFn - Translation Unit Thread Function
*
* This function is executed by the Translation Unit Thread.
* The thread waits on the  message queue created for the Translation Unit
* The message is of the type USBTU_TUMSG.
* Based on the USBTU_EVENTCODE in the message it performs appropriate actions
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

VOID usbtuInitThreadFn 
    ( 
    pVOID param                      /* User Parameter */
    )
    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUSB_INTERFACE_DESCR pIfDescr;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    UINT16 actLen;
    UINT16 ifNo;
    pUSBTU_NODE_INFO pNodeInfo;
    UINT32   message;
    pUSBTU_TUMSG pMessage;
    pUSBTU_CLIENTMSG pClientMessage;
    pUSBTU_DEVICE_DRIVER pDriverInfo;
    pUSBTU_NODE_INFO     pDevInfo;

    USBTU_LOG ( "Translation Unit Thread Function:usbtuInitThreadFn entered \n");

    while(1)
        {

        /* receive message */

        msgQReceive(usbtuMsgQid, (char*)&message,sizeof(UINT32), WAIT_FOREVER);


        /* get pointer to USBTU_TUMSG message structure */

        pMessage = (pUSBTU_TUMSG)message;

        switch (pMessage->eventCode)
            {
            case ADDDEVICE:

                USBTU_LOG ( "usbtuInitThreadFn ADDDEVICE message received\n");


                /* message indicates device attach event */

                /* allocate buffer to get configuration descriptor */

                if ((pBfr = OSS_CALLOC (USB_MAX_DESCR_LEN)) == NULL)
                    {
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn malloc failed \n");
	                continue;
	                }

                /* Get the configuration descriptor  */

                if (usbdDescriptorGet (NULL,
		                              (USBD_NODE_ID)pMessage->hDevice,
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
	                OSS_FREE (pMessage);
                    USBTU_LOG("usbtuInitThreadFn usbdDescriptorGet failed \n");
    	            continue;
	                }

                if ((pCfgDescr = usbDescrParse (pBfr,
                                      	        actLen,
                                                USB_DESCR_CONFIGURATION))== NULL)
                    {
                    OSS_FREE (pBfr);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn usbDescrParse failed \n");
                    continue;
                    }

                /* Look for required interface descriptor   */

                ifNo = 0;

                /*
                 * usbDescrParseSkip() modifies the value of the pointer
                 * it recieves.
                 * so we pass it a copy of our buffer pointer
                 */

                pScratchBfr = pBfr;

                while ((pIfDescr = usbDescrParseSkip (&pScratchBfr,
		               &actLen,
			           USB_DESCR_INTERFACE))
    			       != NULL)
    	            {
    	            if (ifNo == pMessage->interface)
	                    break;
    	            ifNo++;
    	            }

                if (pIfDescr == NULL)
                    {
                    OSS_FREE (pBfr);
                    OSS_FREE (pMessage);
                    USBTU_LOG("usbtuInitThreadFn usbDescrParseSkip failed\n");
                    continue;
                    }

                /* allocate structure for new device attached */

                if ( !(pNodeInfo = OSS_CALLOC( sizeof(USBTU_NODE_INFO))))
                    {
                    OSS_FREE (pBfr);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn malloc1 failed \n");
                    continue;
                    }

                /* initialize members of structure */

                pNodeInfo->hDevice = pMessage->hDevice;
                pNodeInfo->configuration = 0;
                pNodeInfo->interface = pMessage->interface;

                /* search for client that manages device attached */

                OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

                pDriverInfo = usbListFirst (&usbtuClientList);
                while (pDriverInfo != NULL)
                    {


                    if ((pDriverInfo->class == pIfDescr->interfaceClass) &&
                      (pDriverInfo->subclass == pIfDescr->interfaceSubClass) &&
                      (pDriverInfo->protocol == pIfDescr->interfaceProtocol))
	                    break;
                    
                    /* Check if driver only cared about class and subclass */
                    if ((pDriverInfo->class == pIfDescr->interfaceClass) &&
                      (pDriverInfo->subclass == pIfDescr->interfaceSubClass) &&
                      (pDriverInfo->protocol == USBD_NOTIFY_ALL))
	                    break;

                    /* Check if driver only cared about class */
                    if ((pDriverInfo->class == pIfDescr->interfaceClass) &&
                      (pDriverInfo->subclass == USBD_NOTIFY_ALL) &&
                      (pDriverInfo->protocol == USBD_NOTIFY_ALL))
	                    break;


                    pDriverInfo = usbListNext (&pDriverInfo->tuDriverLink);

                    }

                OSS_MUTEX_RELEASE(usbtuMutex);

                if (!(pDriverInfo))
                    {

                    /* no client registered for device type */

                    OSS_FREE (pNodeInfo);
                    OSS_FREE (pBfr);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn no client for device \n");
                    continue;
                    }

                /* store message queue id of client */

                pNodeInfo->msgQid = pDriverInfo->msgQid;

                /* allocate a message structure to be sent to client thread */

                if ( !(pClientMessage = OSS_CALLOC (sizeof (USBTU_CLIENTMSG))))
                    {
                    OSS_FREE (pNodeInfo);
                    OSS_FREE (pBfr);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn malloc2 failed \n");
                    continue;
                    }

                /* initialize message structure */

                pClientMessage->eventCode = ADDDEVICE;
                pClientMessage->hDevice = pNodeInfo->hDevice;
                pClientMessage->interface = pNodeInfo->interface;

                /* send the message to client thread */

                if ( msgQSend(pNodeInfo->msgQid,(char *) &pClientMessage,
                              sizeof(char *), NO_WAIT, MSG_PRI_URGENT )
                              != OK)
                    {

                    /* could not send message to client thread */

                    OSS_FREE (pNodeInfo);
                    OSS_FREE (pBfr);
                    OSS_FREE (pClientMessage);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn msgQSend failed \n");
                    continue;
                    }

                /* update driver data with pointer to message q id */

                *(pMessage->ppDriverData) = pNodeInfo->msgQid;

                /* Link device structure to global list */

                OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

                usbListLink (&usbtuDevList, pNodeInfo,
                             &(pNodeInfo->devLink), LINK_TAIL);

                OSS_MUTEX_RELEASE (usbtuMutex);

                /* Free the memory allocated for the buffer */

                OSS_FREE(pBfr);

                break;

            case REMOVEDEVICE:

                USBTU_LOG ( "usbtuInitThreadFn REMOVEDEVICE message received \n");

                /* message indicates device removal event */

                /* allocate a message structure to send to client thread */

                if ( !(pClientMessage = OSS_CALLOC( sizeof (USBTU_CLIENTMSG))))
                    {
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn malloc failed \n");
                    continue;
                    }

                /* initialize message structure */

                pClientMessage->eventCode = REMOVEDEVICE;
                pClientMessage->hDevice = pMessage->hDevice;

                /* send message */

                if ( msgQSend((MSG_Q_ID)pMessage->ppDriverData,
                              (char *) &pClientMessage ,
                              sizeof(char *), NO_WAIT, MSG_PRI_URGENT)
                              != OK)
                    {

                    /* could not send message to client thread  */

                    OSS_FREE ( pClientMessage);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn msgQSend failed \n");
                    continue;
                    }

                break;

             case SUSPENDDEVICE:

                 USBTU_LOG ( "usbtuInitThreadFn SUSPENDDEVICE message received\n");

                 /* message indicates device suspend  event */

                 /* search devList for device structure */

                 OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

                 pDevInfo = usbListFirst (&usbtuDevList);

                 while (pDevInfo != NULL)
                     {
                     if ( pDevInfo->hDevice == pMessage->hDevice )
	                     break;

                     pDevInfo = usbListNext (&pDevInfo->devLink);

                     }

                 OSS_MUTEX_RELEASE (usbtuMutex);

                 if (!(pDevInfo))
                     {
                     /* did not find device structure */

                     OSS_FREE (pMessage);
                     USBTU_LOG ( "usbtuInitThreadFn device not found \n");
                     continue;

                     }

                 /* allocate a message structure to send to client thread */

                 if ( !(pClientMessage = OSS_CALLOC(sizeof (USBTU_CLIENTMSG))))
                     {
                     OSS_FREE (pMessage);
                     USBTU_LOG ( "usbtuInitThreadFn malloc failed \n");
                     continue;
                     }

                 /* initialze message structure */

                 pClientMessage->eventCode = SUSPENDDEVICE;
                 pClientMessage->hDevice = pMessage->hDevice;

                 /* send message */

                 if ( msgQSend(pDevInfo->msgQid,(char *) &pClientMessage,
                               sizeof(char *), NO_WAIT, MSG_PRI_URGENT )
                               != OK)
                     {

                     /* could not send message to client */

                     OSS_FREE ( pClientMessage);
                     OSS_FREE (pMessage);
                     USBTU_LOG ( "usbtuInitThreadFn msgQSend failed \n");
                     continue;
                     }

                 /* update driver data */

                 *(pMessage->ppDriverData) = pDevInfo->msgQid;

                 break;

            case RESUMEDEVICE:

                USBTU_LOG ( "usbtuInitThreadFn RESUMEDEVICE  message received\n");

                /* message indicates device resume  event */

                /* allocate a message structure to be sent to client thread */


                if ( !(pClientMessage = OSS_CALLOC( sizeof (USBTU_CLIENTMSG))))
                    {
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn malloc failed \n");
                    continue;
                    }


                /* initialize message structure */

                pClientMessage->eventCode = RESUMEDEVICE;
                pClientMessage->hDevice = pMessage->hDevice;

                /* send the message to client  */

                if ( msgQSend((MSG_Q_ID)pMessage->ppDriverData,
                              (char *) &pClientMessage,
                               sizeof(char *),
                               NO_WAIT,
                               MSG_PRI_URGENT )
                               != OK )
                    {

                    /* could not send message to client */

                    OSS_FREE (pClientMessage);
                    OSS_FREE (pMessage);
                    USBTU_LOG ( "usbtuInitThreadFn msgQSend failed \n");
                    continue;
                    }

                break;
            default:
                break;
            }

        OSS_FREE(pMessage);

        USBTU_LOG("Translation Unit Thread Function:waiting for more messages\n");
        }
    }
/***************************************************************************
*
* usbtuInitClientThreadFn - Client Thread Function
*
* This function is executed by a client Thread.
* The thread waits on the  message queue created for the client
* The message is of the type USBTU_CLIENTMSG.
* Based on the USBTU_EVENTCODE in the message it performs the action.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/


VOID usbtuInitClientThreadFn
    (
    pVOID driverParam
    )

    {
    UINT32 message;
    pUSBTU_CLIENTMSG pMessage;
    pUSBTU_DEVICE_DRIVER pDriverInfo = (pUSBTU_DEVICE_DRIVER) driverParam;
    pUSBTU_NODE_INFO pDevInfo;

    USBTU_LOG ( " Client Thread Function : usbtuInitClientThreadFn entered \n");

    while(1)
        {

        /* wait for a message */

        msgQReceive(pDriverInfo->msgQid,
                    (char*)&message,
                    sizeof(UINT32),
                    WAIT_FOREVER);

        /* get the message pointer  */

        pMessage = (pUSBTU_CLIENTMSG)message;

        switch (pMessage->eventCode)
            {
            case ADDDEVICE:

                /* message indicates device add event */

                USBTU_LOG ( "usbtuInitClientThreadFn ADDDEVICE message received \n");

                /* Wind View Instrumentation */

                if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
                    {
                    char evLog[USBTU_WV_LOGSIZE];
                    strncpy ((char*)evLog, (char *)(pDriverInfo->clientName),USBD_NAME_LEN );
                    strcat(evLog, " : ADDDEVICE Event ");
                    USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
                    }

                /* call the user supplied callback */

                (*pDriverInfo->attachCallback)
                    (
                    (USBD_NODE_ID) pMessage->hDevice,USBD_DYNA_ATTACH,
                    0,
                    pMessage->interface,
                    pDriverInfo->class,
                    pDriverInfo->subclass,
                    pDriverInfo->protocol
                    );

                break;

            case REMOVEDEVICE:

                /* message indicates device removal event */

                USBTU_LOG("usbtuInitClientThreadFn REMOVEDEVICE message received\n");

                /* Wind View Instrumentation */

                if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
                    {
                    char evLog[USBTU_WV_LOGSIZE];
                    strncpy ((char*)evLog,(char *)(pDriverInfo->clientName),USBD_NAME_LEN );
                    strcat(evLog, " : REMOVEDEVICE Event ");
                    USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
                    }

                /* call the user supplied callback */

                 (*pDriverInfo->attachCallback)
                    (
                    (USBD_NODE_ID)pMessage->hDevice,
                    USBD_DYNA_REMOVE,
                    1,
                    pMessage->interface,
                    pDriverInfo->class,
                    pDriverInfo->subclass,
                    pDriverInfo->protocol);

                 /* remove the device structure from global list */

                 OSS_MUTEX_TAKE (usbtuMutex, OSS_BLOCK);

                 pDevInfo = usbListFirst (&usbtuDevList);

                 while (pDevInfo != NULL)
                     {
                     if ( pDevInfo->hDevice == pMessage->hDevice )
	                 break;

                     pDevInfo = usbListNext (&pDevInfo->devLink);

                     }

                 if (!(pDevInfo))
                     {
                     /* did not find device structure */

                     OSS_FREE (pMessage);
                     OSS_MUTEX_RELEASE (usbtuMutex);
                     USBTU_LOG ( "usbtuInitClientThreadFn device not found \n");
                     continue;

                     }

                /* unlink structure for device */

                usbListUnlink(&pDevInfo->devLink);

                OSS_MUTEX_RELEASE (usbtuMutex);

                /* Free the node structure */

                OSS_FREE(pDevInfo);

                break;

            case SUSPENDDEVICE:

                /* message indicates device suspend event */

                USBTU_LOG("usbtuInitClientThreadFn SUSPENDDEVICE message received\n");

                /* Wind View Instrumentation */

                if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
                    {
                    char evLog[USBTU_WV_LOGSIZE];
                    strncpy ((char*)evLog, (char *)(pDriverInfo->clientName),USBD_NAME_LEN );
                    strcat(evLog, " : SUSPENDDEVICE Event ");
                    USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
                    }

                /* no code defined for suspend event by Wind River,
                 * so no call back called
                 */

                break;

            case RESUMEDEVICE:

                /* message indicates device resume event */

                USBTU_LOG("usbtuInitClientThreadFn RESUMEDEVICE message received\n");

                /* Wind View Instrumentation */

                if ((usbtuInitWvFilter & USBTU_WV_FILTER) == TRUE)
                    {
                    char evLog[USBTU_WV_LOGSIZE];
                    strncpy ((char*)evLog, (char *)(pDriverInfo->clientName),USBD_NAME_LEN );
                    strcat(evLog, " : RESUMEDEVICE Event ");
                    USB_HCD_LOG_EVENT(USBTU_WV_CLIENT_INIT, evLog, USBTU_WV_FILTER);
                    }

                /* call the user supplied callback */

                 if ( pDriverInfo->mngmtCallback == NULL)
                     {
                     USBTU_LOG( " management callback NULL \n ");
                     }
                 else
                 {

                 (*(pDriverInfo->mngmtCallback))
                    (
                    pDriverInfo->mngmtCallbackParam,
                    (USBD_NODE_ID)pMessage->hDevice,
                    USBD_MNGMT_RESUME
                    );
                 }

                 break;

            default:
                break;
            }

            OSS_FREE (pMessage);
            USBTU_LOG("usbtuInitClientThreadFn continues to wait for more \
                       messages\n");
        }
    }


/***************************************************************************
*
* usbtuInitClientIrpCompleteThreadFn - Client Thread Function
*
* This function is executed by a client Thread.
* The thread waits on the  message queue created for the client
* The message is of the type USBTU_CLIENTMSG.
* Based on the USBTU_EVENTCODE in the message it performs the action.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/


VOID usbtuInitClientIrpCompleteThreadFn
    (
    pVOID driverParam
    )

    {
    UINT32 message;
    pUSB_IRP        pIrp;
    pUSBTU_DEVICE_DRIVER pDriverInfo = (pUSBTU_DEVICE_DRIVER) driverParam;
 
    USBTU_LOG ( " Client Thread Function : usbtuInitClientIrpCompleteThreadFn entered \n");

    while(1)
        {

        /* wait for a message */

        msgQReceive(pDriverInfo->msgQidIrpComplete,
                    (char*)&message,
                    sizeof(UINT32),
                    WAIT_FOREVER);

        /* get the message pointer  */
        pIrp =     (pUSB_IRP) message;


                /* message indicates IRP completion event */

                USBTU_LOG("usbtuInitClientIrpCompleteThreadFn IRPCOMPLETE message received\n");

                /* call the user supplied IRP completion callback */
        (*pIrp->userCallback)(pIrp);
            USBTU_LOG("usbtuInitClientIrpCompleteThreadFn continues to wait for more \
                       messages\n");
        }
    }

/***************************************************************************
*
* usbtuInitDeviceAdd - Device Attach Callback
*
* This function is called from interrupt context by USBD
* on a device attach.
*
* RETURNS: USBHST_SUCCESS, or USBHST_FAILURE on failure
*
* ERRNO: N/A
*/

USBHST_STATUS usbtuInitDeviceAdd
    (
    UINT32 hDevice,
    UINT8 interfaceNumber,
    UINT8 speed,
    void** ppDriverData
    )

    {
    pUSBTU_TUMSG   pTuMessage;

    USBTU_LOG ( " Device Attach Callback usbtuInitDeviceAdd() entered \n");

    /* allocate a message structure to send to Translation Unit thread */

    if ( !(pTuMessage = OSS_CALLOC( sizeof (USBTU_TUMSG))))
        {
        USBTU_LOG ( "usbtuInitDeviceAdd malloc failed \n");
        return USBHST_FAILURE;
        }
    else
        {

        /* initialize message structure */

        pTuMessage->eventCode = ADDDEVICE;
        pTuMessage->hDevice = hDevice;
        pTuMessage->interface = interfaceNumber;
        pTuMessage->ppDriverData = ppDriverData;

        /* send message ptr */

        if (msgQSend(usbtuMsgQid,(char *) &pTuMessage , sizeof(char *),
                     NO_WAIT, MSG_PRI_URGENT )
                     !=OK)
            USBTU_LOG ( "tuinitDeviceAdd msgQSend failed \n");
        }

    USBTU_LOG ( "usbtuInitDeviceAdd left \n");
    return USBHST_SUCCESS;

    }

/***************************************************************************
*
* usbtuInitDeviceRemove - Device Detach Callback
*
* This function is called from interrupt context by USBD
* on a device detach.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

VOID usbtuInitDeviceRemove
    (
    UINT32 hDevice,
    PVOID pDriverData
    )

    {
    pUSBTU_TUMSG   pTuMessage;

    USBTU_LOG ( " Device Detach Callback usbtuInitDeviceRemove() entered \n");

    /* allocate a message structure to send to Translation Unit thread */

    if ( !(pTuMessage = OSS_CALLOC( sizeof (USBTU_TUMSG))))
        {
        USBTU_LOG ( "usbtuInitDeviceRemove malloc failed \n");
        }
    else
        {

        /* initialize message structure */

        pTuMessage->eventCode = REMOVEDEVICE;
        pTuMessage->hDevice = hDevice;
        pTuMessage->ppDriverData = pDriverData;

        /* send message ptr */

        if ( msgQSend(usbtuMsgQid,(char *) &pTuMessage,
                      sizeof(char *) , NO_WAIT, MSG_PRI_URGENT )
                      != OK)

            USBTU_LOG ( "usbtuInitDeviceRemove msgQSend failed \n");
        }

    USBTU_LOG ( "usbtuInitDeviceRemove left \n");

    }

/***************************************************************************
*
* usbtuInitDeviceSuspend - Device Suspend Callback
*
* This function is called from interrupt context by USBD
* on a device suspend.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

VOID usbtuInitDeviceSuspend
    (
    UINT32 hDevice,
    PVOID ppSuspendData
    )

    {
    pUSBTU_TUMSG   pTuMessage;

    USBTU_LOG ( "Device Suspend Callback usbtuInitDeviceSuspend() entered \n");


    /* allocate a message structure to send to Translation Unit thread */

    if ( !(pTuMessage = OSS_CALLOC( sizeof (USBTU_TUMSG))))
        {
        USBTU_LOG ( "usbtuInitDeviceSuspend malloc failed \n");
        }
    else
        {

        /* initialize message structure */

        pTuMessage->eventCode = SUSPENDDEVICE;
        pTuMessage->hDevice = hDevice;
        pTuMessage->ppDriverData = ppSuspendData;

        /* send message ptr */

        if ( msgQSend(usbtuMsgQid,(char *) &pTuMessage,
                      sizeof(char *) , NO_WAIT, MSG_PRI_URGENT )
                      != OK)
            USBTU_LOG ( "usbtuInitDeviceSuspend msgQSend failed \n");
        }

    USBTU_LOG ( "usbtuInitDeviceSuspend left \n");

    }

/***************************************************************************
*
* usbtuInitDeviceResume - Device Resume Callback
*
* This function is called from interrupt context by USBD
* on a device resume.
*
* RETURNS: N/A
*
* ERRNO: N/A
*/

VOID usbtuInitDeviceResume
    (
    UINT32 hDevice,
    PVOID pSuspendData
    )

    {
    pUSBTU_TUMSG   pTuMessage;

    USBTU_LOG ( "Device Resume Callback usbtuInitDeviceResume() entered \n");

    /* allocate a message structure to send to Translation Unit thread */

    if (!(pTuMessage = OSS_CALLOC( sizeof (USBTU_TUMSG))))
        {
        USBTU_LOG ( "usbtuInitDeviceResume malloc failed \n");
        }
    else
        {

        /* initialize message structure */

        pTuMessage->eventCode = RESUMEDEVICE;
        pTuMessage->hDevice = hDevice;
        pTuMessage->ppDriverData = pSuspendData;

        /* send message ptr  */

        if ( msgQSend(usbtuMsgQid,(char *) &pTuMessage , sizeof(char *),
                      NO_WAIT, MSG_PRI_URGENT )
                      != OK )
            USBTU_LOG ( "usbtuInitDeviceResume msgQSend failed \n");
        }

    USBTU_LOG ( "usbtuInitDeviceResume left \n");

    }
/* End of file. */


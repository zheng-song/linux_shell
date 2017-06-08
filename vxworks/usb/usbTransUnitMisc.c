/* usbTransUnitMisc.c - translation unit miscellaneous functions */

/* Copyright 2003 Wind River Systems, Inc. */

/* 
Modification History
--------------------
01d,15oct04,ami  Apigen Changes
01e,28sep04,hch  Fix gnu compiler warning with usbdHcdAttach and usbdHcdDetach
01d,03aug04,hch  Fix compiler warning
01c,03aug04,mta  coverity error fixes
01b,03aug04,ami  Warning Messages Removed
01a,19sep03,mta  First
*/

/*
DESCRIPTION

Implements the Translation Unit Miscellaneous Interfaces.
These interfaces are used only by UsbTool and not by Class Drivers.
They interfaces are provided to Integrate Translation Unit with UsbTool

INCLUDE FILES: drv/usb/usbTransUnit.h, usb/pciConstants.h, usb2/usbHubMisc.h,
usb2/usbdMisc.h
*/

/* includes */

#include "drv/usb/usbTransUnit.h"
#include "usb/pciConstants.h"
#include "usb2/usbHubMisc.h"
#include "usb2/usbdMisc.h"

/* defines */

#define USBTUMISC_USBD_VERSION	0x0200		      /* USBD version in BCD */
#define USBTUMISC_USBD_MFG	  "Wind River Systems, Inc."  /* USBD Mfg */

/* Add these in compiler options
#define INCLUDE_OHCD
#define INCLUDE_UHCD
#define INCLUDE_EHCD*/


/* To hold the class code for UHCI Complaint USB Host Controllers */
#define USB_UHCI_HOST_CONTROLLER                            0x000C0300

/* To hold the class code for OHCI Complaint USB Host Controllers */
#define USB_OHCI_HOST_CONTROLLER                            0x000C0310

/* To hold the class code for EHCI Complaint USB Host Controllers */
#define USB_EHCI_HOST_CONTROLLER                            0x000C0320


#define MAX_NO_OF_OHCI_CONTROLLERS	5
#define MAX_NO_OF_UHCI_CONTROLLERS	5
#define MAX_NO_OF_EHCI_CONTROLLERS	5

#define TOKEN_FOR_OHCI	0x4
#define TOKEN_FOR_UHCI	0x5
#define TOKEN_FOR_EHCI	0x6

/* Defines for the return types for detected devices */

#define USBHUB_NODETYPE_HUB      (0x02)
#define USBHUB_NODETYPE_DEVICE   (0x01)
#define USBHUB_NODETYPE_NONE     (0x00)



/***************************************************************************
*
* usbdHcdAttach - attaches an HCD to the USBD
*
* The <hcdExecFunc> passed by the caller must point to an HCD’s primary
* entry point as defined below:
*
* \cs
* typedef UINT16 (*HCD_EXEC_FUNC) (pHRB_HEADER pHrb);
* \ce
*
* RETURNS: OK
*
* ERRNO: none
*/


STATUS usbdHcdAttach
    (
    HCD_EXEC_FUNC hcdExecFunc,		/* Ptr to HCD’s primary entry point */
    void * hcdPciCfgHdr,			/* HCD-specific parameter */
    pGENERIC_HANDLE pAttachToken	/* Token to identify HCD in future */
    )
    {
    UINT32 HCD_ID;
    PCI_CFG_HEADER *pciCfgHdr = (PCI_CFG_HEADER *)hcdPciCfgHdr;

    HCD_ID = ( (pciCfgHdr->pciClass << 16) | (pciCfgHdr->subClass << 8) | pciCfgHdr->pgmIf );

    switch(HCD_ID)
        {

#ifdef INCLUDE_EHCD
        case USB_EHCI_HOST_CONTROLLER :
            if(TRUE == usbEhcdInit())
                *pAttachToken = TOKEN_FOR_EHCI ;
            break;
#endif

#ifdef INCLUDE_OHCD
        case USB_OHCI_HOST_CONTROLLER :
            if(TRUE == usbOhciInit())
                *pAttachToken = TOKEN_FOR_OHCI;
            break;
#endif

#ifdef INCLUDE_UHCD
        case USB_UHCI_HOST_CONTROLLER :
            if(USBHST_SUCCESS == usbUhcdInit())
                *pAttachToken = TOKEN_FOR_UHCI;
            break;
#endif

        default:
            break;
        }

    return OK;
    }



/***************************************************************************
*
* usbdHcdDetach - Detaches an HCD from the USBD
*
* The <attachToken> must be the attach token originally returned by
* usbdHcdAttach() when it first attached to the HCD.
*
* RETURNS: OK
*
* ERRNO: none
*/

STATUS usbdHcdDetach
    (
    GENERIC_HANDLE attachToken		/* AttachToken returned */
    )
    {

UINT32 token = (UINT32)attachToken;

switch ( token )
	{

#ifdef INCLUDE_EHCD
	case TOKEN_FOR_EHCI:
        usbEhcdExit();
		break;
#endif

#ifdef INCLUDE_OHCD
	case TOKEN_FOR_OHCI:
		usbOhciExit();
		break;
#endif

#ifdef INCLUDE_UHCD
	case TOKEN_FOR_UHCI:
		usbUhcdExit();
		break;
#endif

	default:
            break;
	}
    return OK;
    }

/***************************************************************************
*
* usbdBusCountGet - get number of USBs attached to the host.
*
* This function returns the total number of USB host controllers in the
* system.  Each host controller has its own root hub as required by the USB
* specification; and clients planning to enumerate USB devices using the Bus
* Enumeration Functions need to know the total number of host controllers in
* order to retrieve the Node Ids for each root hub.
*
* <pBusCount> must point to a UINT16 variable in which the total number of 
* USB host controllers will be stored.
*
* Note: The number of USB host controllers is not constant.  Bus controllers 
* can be added by calling usbdHcdAttach() and removed by calling 
* usbdHcdDetach().  Again, the Dynamic Attach Functions deal with these 
* situations automatically, and are the preferred mechanism by which most 
* clients should be informed of device attachment and removal.
*
* RETURNS: OK, or ERROR if unable to retrieve bus count
*
* ERRNO: none
*/

STATUS usbdBusCountGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    pUINT16 pBusCount			/* Word bfr to receive bus count */
    )

    {
     
    if (pBusCount == NULL)
        return ERROR;
    *pBusCount = usbdBusCntGet();

    return OK;
    }


/***************************************************************************
*
* usbdRootNodeIdGet - returns root node for a specific USB
*
* This function returns the Node Id for the root hub for the specified 
* USB host controller.	<busIndex> is the index of the desired USB host 
* controller.  The first host controller is index 0 and the last host 
* controller's index is the total number of USB host controllers - as 
* returned by usbdBusCountGet() - minus 1. < pRootId> must point to a 
* USBD_NODE_ID variable in which the Node Id of the root hub will be stored.
* 
* RETURNS: OK, or ERROR if unable to get root node ID.
*
* ERRNO: none
*/

STATUS usbdRootNodeIdGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    UINT16 busIndex,			/* Bus index */
    pUSBD_NODE_ID pRootId		/* bfr to receive Root Id */
    )

    {
    UINT32 rootId;

    if (pRootId == NULL)
        return ERROR;
        
    usbdRootNodeIDGet(busIndex,&rootId);

    *pRootId = (USBD_NODE_ID)rootId;

    return OK;

    }



/*******************************************************************************
*
* usbdHubPortCountGet - returns number of ports connected to a hub
*
* usbdHubPortCountGet() provides clients with a convenient mechanism to
* retrieve the number of downstream ports provided by the specified hub.
* Clients can also retrieve this information by retrieving configuration
* descriptors from the hub using the Configuration Functions describe in
* a following section.
*
* <hubId> must be the Node Id for the desired USB hub.	An error will be
* returned if <hubId> does not refer to a hub.	<pPortCount> must point to
* a UINT16 variable in which the total number of ports on the specified
* hub will be stored.
*
* RETURNS: OK, or ERROR if unable to get hub port count.
*
* ERRNO: none
*/

STATUS usbdHubPortCountGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID hubId, 		/* Node Id for desired hub */
    pUINT16 pPortCount			/* bfr to receive port count */
    )

    {

    /* Support is required from USBD to retrieve actual data.
     * This is currently a stub.The count that is returned is equal to the
     * number of USB devices already attached to USB or 2 which ever is higher
     */

    USBHST_STATUS status;

    if(pPortCount == NULL)
        return ERROR;

    status = usbHubPortCntGet((UINT32)hubId,pPortCount);

    if (status != USBHST_SUCCESS)
        return ERROR;
    else
        return OK;

    }

/*******************************************************************************
*
* usbdNodeIdGet - gets the id of the node connected to a hub port
*
* Clients use this function to retrieve the Node Id for devices attached to
* each of a hub’s ports.  <hubId> and <portIndex> identify the hub and port
* to which a device may be attached.  <pNodeType> must point to a UINT16
* variable to receive a type code as follows:
*
* \is
* \i 'USB_NODETYPE_NONE'
* No device is attached to the specified port.
*
* \i 'USB_NODETYPE_HUB'
* A hub is attached to the specified port.
*
* \i 'USB_NODETYPE_DEVICE'
* A device (non-hub) is attached to the specified port.
* \ie
*
* If the node type is returned as USBD_NODE_TYPE_NONE, then a Node Id is
* not returned and the value returned in <pNodeId> is undefined.  If the
* node type indicates a hub or device is attached to the port, then
* <pNodeId> will contain that hub or device’s nodeId upon return.
*
* RETURNS: OK, or ERROR if unable to get node ID.
* 
* ERRNO: none
*/

STATUS usbdNodeIdGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID hubId, 		/* Node Id for desired hub */
    UINT16 portIndex,			/* Port index */
    pUINT16 pNodeType,			/* bfr to receive node type */
    pUSBD_NODE_ID pNodeId		/* bfr to receive Node Id */
    )

    {

    /* Support is required from USBD to retrieve actual data.
     * This is a currently a stub. The portIndex is used to chain through
     * the global device list to get to the appropriate device.
     */

    UINT16 nodeType;
    UINT32 nodeId; 
    USBHST_STATUS status;
    
    if ( pNodeType == NULL ||  pNodeId == NULL)
        return ERROR;


    status  = usbHubNodeIDGet((UINT32)hubId, portIndex, &nodeType, &nodeId);

    switch( nodeType)
        {
        case  USBHUB_NODETYPE_HUB :
                 *pNodeType = USB_NODETYPE_HUB;
                 *pNodeId = (USBD_NODE_ID) nodeId;
                 break;
        case  USBHUB_NODETYPE_DEVICE :
                *pNodeType = USB_NODETYPE_DEVICE;
                *pNodeId = (USBD_NODE_ID) nodeId;
                 break;
        case  USBHUB_NODETYPE_NONE :
                *pNodeType = USB_NODETYPE_NONE;
                *pNodeId = (USBD_NODE_ID) nodeId;
                 break;
        }


    if(status != USBHST_SUCCESS)
        return ERROR;
    else
        return OK;

    }

/*******************************************************************************
*
* usbdAddressGet - gets the USB address for a given device
*
* This function returns the USB address assigned to device specified by
* <nodeId>.
*
* RETURNS: OK, or ERROR
*
* ERRNO: none
*/

STATUS usbdAddressGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    pUINT16 pDeviceAddress		/* Currently assigned device address */
    )

    {
    /* This interface is not exposed by the Host Stack to Client Modules
     * This is a stub.
     */

    if (pDeviceAddress == NULL)
        return ERROR;

    *pDeviceAddress = 0;

    return OK;

    }


/***************************************************************************
*
* usbdAddressSet - sets the USB address for a given device
*
* This function sets the USB address at which a device will respond to future
* requests.  Upon return, the address of the device identified by <nodeId>
* will be changed to the value specified in <deviceAddress>.  <deviceAddress>
* must be in the range from 0..127.  The <deviceAddress> must also be unique
* within the scope of each USB host controller.
*
* The USBD manages USB device addresses automatically, and this function
* should never be called by normal USBD clients.  Changing a device address
* may cause serious problems, including device address conflicts, and may
* cause the USB to cease operation.
*
* RETURNS: OK, or ERROR 
*
* ERRNO: none
*/

STATUS usbdAddressSet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of device/hub */
    UINT16 deviceAddress		/* New device address */
    )

    {

    /* This interface is not exposed by the Host Stack to Client Modules.
     * This is a stub.
     */
    if (deviceAddress > 127)
        return ERROR;
    else
        return OK;

    }

/******************************************************************************
*
* usbdVersionGet - Returns USBD version information
*
* This function returns the USBD version.  If <pVersion> is not NULL, the
* USBD returns its version in BCD in <pVersion>.  For example, version
* "1.02" would be coded as 01h in the high byte and 02h in the low byte.
*
* If <pMfg> is not NULL it must point to a buffer of at least USBD_NAME_LEN
* bytes in length in which the USBD will store the NULL terminated name of
* the USBD manufacturer (e.g., "Wind River Systems" + \0).
*
* RETURNS: OK, or ERROR
*
* ERRNO: none
*/

STATUS usbdVersionGet
    (
    pUINT16 pVersion,			/* UINT16 bfr to receive version */
    pCHAR pMfg				/* bfr to receive USBD mfg string */
    )

    {

    if ( pVersion == NULL || pMfg == NULL)
        return ERROR;

	*pVersion = USBTUMISC_USBD_VERSION;
    strncpy (pMfg, (char *)USBTUMISC_USBD_MFG, USBD_NAME_LEN + 1);

	return OK;

    }
    
/*******************************************************************************
*
* usbdStatisticsGet - Retrieves USBD operating statistics
*
* This function returns operating statistics for the USB to which the
* specified <nodeId> is connected.
*
* The USBD copies the current operating statistics into the <pStatistics>
* structure provided by the caller.  This structure is defined as:
*
* \cs
* typedef struct usbd_stats
*     {
*     UINT16 totalTransfersIn;
*     UINT16 totalTransfersOut;
*     UINT16 totalReceiveErrors;
*     UINT16 totalTransmitErrors;
*     } USBD_STATS, *pUSBD_STATS;
* \ce
*
* It is anticipated that this structure may grow over time.  To provide
* backwards compatibility, the client must pass the total size of the
* USBD_STATS structure it has allocated in <statLen>.  The USBD will copy
* fields into this structure only up to the statLen indicated by the caller.
*
* RETURNS: OK
* 
* ERRNO: N/A
*/

STATUS usbdStatisticsGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of node on desired USB */
    pUSBD_STATS pStatistics,		/* Ptr to structure to receive stats */
    UINT16 statLen			/* Len of bfr provided by caller */
    )

    {

    /* Support is required from USBD to retrieve actual data.
     * This is a currently a stub. All members are filled with zeros
     */
    
    memset ( (char*)pStatistics, 0, statLen);

    return OK;

    }

/***************************************************************************
*
* usbdCurrentFrameGet - returns the current frame number for a USB
*
* It is sometimes necessary for clients to retrieve the current USB frame 
* number for a specified host controller.  This function allows a client to 
* retrieve the current USB frame number for the host controller to which
* <nodeId> is connected.  Upon return, the current frame number is stored 
* in <pFrameNo>.
*
* If <pFrameWindow> is not NULL, the USBD will also return the maximum frame 
* scheduling window for the indicated USB host controller.  The frame 
* scheduling window is essentially the number of unique frame numbers 
* tracked by the USB host controller.  Most USB host controllers maintain an 
* internal frame count which is a 10- or 11-bit number, allowing them to 
* track typically 1024 or 2048 unique frames.  When starting an isochronous 
* transfer, a client may wish to specify that the transfer will begin in a 
* specific USB frame.  For the given USB host controller, the starting frame 
* number can be no more than <frameWindow> frames from the current <frameNo>.
*
* Note: The USBD is capable of simultaneously managing multiple USB host 
* controllers, each of which operates independently.  Therefore, it is 
* important that the client specify the correct <nodeId> when retrieving the 
* current frame number.  Typically, a client will be interested in the 
* current frame number for the host controller to which a specific device is 
* attached.
*
* RETURNS: OK, or ERROR if unable to retrieve current frame number.
*
* ERRNO: none
*/

STATUS usbdCurrentFrameGet
    (
    USBD_CLIENT_HANDLE clientHandle,	/* Client handle */
    USBD_NODE_ID nodeId,		/* Node Id of node on desired USB */
    pUINT32 pFrameNo,			/* bfr to receive current frame no. */
    pUINT32 pFrameWindow		/* bfr to receive frame window */
    )

    {
    UINT16 frameno;
    USBHST_STATUS status;

    if (pFrameNo == NULL || pFrameWindow == NULL)
        return ERROR;

    status = usbHstGetFrameNumber((UINT32)nodeId, &frameno);
    if (status != USBHST_SUCCESS)
        return ERROR;
    *pFrameNo = frameno;
    *pFrameWindow = 1024;

    return OK;

    }




/***************************************************************************
*
* HubEnumerate - enumerate all ports on the specified hub
* 
* This routine enumerates all devices from the specified HubId down.
* <clientHandle> must be a valid USBD_CLIENT_HANDLE.  <hubId> specifies
* the Node Id of the first USB hub to be enumerated.
*
* RETURNS: OK, or ERROR if USBD returns an error.
*
* ERRNO: N/A
* 
*\NOMANUAL
*/
#define INDENT 2

LOCAL UINT16 HubEnumerate
    (
    USBD_CLIENT_HANDLE clientHandle, /* Caller’s USBD client handle */
    USBD_NODE_ID hubId, 			 /* Node Id for hub to enumerate */
    UINT16 indent,                   
    pUSBD_NODE_INFO pNodeInfo,		 /* Structure to receive node info */    
    USBD_NODE_ID nodeId_passed		 /* id of node being enumerated */    
    )
    {
    UINT16	portCount;			     /* Number of ports on this hub */
    UINT16	portIndex;			     /* current port index */
    UINT16	nodeType;			     /* type of node being enumerated */
    USBD_NODE_ID nodeId;		     /* id of node being enumerated */

    /* Retrieve the number of ports for this hub. */

    if ( usbdHubPortCountGet (clientHandle, hubId, &portCount) != OK )
		{
		return ERROR;
		}

    /* See if a device is attached to each of the hub’s ports. */

    for (portIndex = 0; portIndex < portCount; portIndex++) 
		{
		if ( usbdNodeIdGet (clientHandle, hubId, portIndex, 
							&nodeType, &nodeId) != OK)
		    {
		    return ERROR;
		    }
		if(nodeId==nodeId_passed)
			{
			pNodeInfo->nodeType = nodeType;
			pNodeInfo->parentHubId = hubId;
			pNodeInfo->parentHubPort = portIndex;
			}
		
		switch (nodeType)
		    {
		    case USB_NODETYPE_HUB:	/* Another hub found. */
				if (HubEnumerate (clientHandle, nodeId,  
								  indent+INDENT,pNodeInfo,nodeId_passed ) != OK)
				    return ERROR;
			break;
	
		    default:			/* Unknown node type code */
			break;
		    }
		}

    return OK;
    }

/*******************************************************************************
*
* usbdNodeInfoGet - returns information about a USB node
*
* This function retrieves information about the USB device specified by 
* <nodeId>.  The USBD copies node information into the <pNodeInfo> structure 
* provided by the caller.  This structure is of the form USBD_NODEINFO as
* shown below:
*
* \cs
* typedef struct usbd_nodeinfo
*     {
*     UINT16 nodeType;
*     UINT16 nodeSpeed;
*     USBD_NODE_ID parentHubId;
*     UINT16 parentHubPort;
*     USBD_NODE_ID rootId;
*     } USBD_NODEINFO, *pUSBD_NODEINFO;
* \ce
*
* <nodeType> specifies the type of node identified by <nodeId> and is defined 
* as USB_NODETYPE_xxxx.  <nodeSpeed> identifies the speed of the device and 
* is defined as USB_SPEED_xxxx, Currently this field is not updated.
* <parentHubId> and <parentHubPort> identify the Node Id and port of the hub 
* to which the indicated node is attached upstream.  
* If the indicated <nodeId> happens to be a root hub, then 
* <parentHubId> and <parentHubPort> will both be 0.  
*
* Similarly, <rootId> identifies the Node Id of the root hub for the USB to 
* which nodeId is attached.  If <nodeId> itself happens to be the root hub, 
* then the same value will be returned in <rootId>.
*
* It is anticipated that this structure may grow over time.  To provide 
* backwards compatibility, the client must pass the total size of the 
* USBD_NODEINFO structure it has allocated in <infoLen>.  The USBD will copy 
* fields into this structure only up to the <infoLen> indicated by the caller.
*
* RETURNS: OK, or ERROR if unable to retrieve node information.
*
* ERRNO: None
*/

STATUS usbdNodeInfoGet
    (
    USBD_CLIENT_HANDLE clientHandle,/* Client handle */
    USBD_NODE_ID nodeId,			/* Node Id of device/hub */
    pUSBD_NODE_INFO pNodeInfo,		/* Structure to receive node info */
    UINT16 infoLen					/* Len of bfr allocated by client */
    )
    {
    UINT16	busCount;				/* Number of USB host controllers */
    UINT16	busIndex;				/* current bus index */
    USBD_NODE_ID rootId;			/* Root hub id for current bus */

    /* Retrieve the number of USB host controllers in the system. */
	if( NULL == pNodeInfo )
		{
		return ERROR;
		}
	
    if ( usbdBusCountGet (clientHandle, &busCount) != OK)
		{
		return ERROR;
		}

    /* Retrieve the root hub id for each host controller and enumerate it. */
    for (busIndex = 0; busIndex < busCount; busIndex++)
		{
		
		if ( usbdRootNodeIdGet (clientHandle, busIndex, &rootId) != OK )
		    {
		    return ERROR;
		    }
		
		pNodeInfo->rootId = rootId;
		if (HubEnumerate (clientHandle, rootId,  INDENT,pNodeInfo ,nodeId ) != OK)
		    return ERROR;
		}
	return OK;
		
	}


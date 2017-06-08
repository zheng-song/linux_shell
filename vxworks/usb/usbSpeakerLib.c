/* usbSpeakerLib.c - USB speaker class drive with vxWorks SEQ_DEV interface */

/* Copyright 2000-2002 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,15oct04,ami  Apigen Changes
01g,29oct01,wef  Remove automatic buffer creations and repalce with OSS_MALLOC.
01f,18sep01,wef  merge from wrs.tor2_0.usb1_1-f for veloce
01e,08aug01,dat  Removing warnings
01d,31jan01,wef  Fixed tab stops
01c,12apr00,wef  Fixed uninitialized variable warning: controlSelector and
		 settingWidth in setChannelControl()
01b,20mar00,rcb  Re-write code which de-references potentiall non-aligned word 
		 fields to prevent faults on certain processors (e.g., MIPS).
01a,12jan00,rcb  written.
*/

/*
DESCRIPTION

This module implements the class driver for USB speaker devices.  USB speakers
are a subset of the USB audio class, and this module handles only those parts
of the USB audio class definition which are relevant to the operation of USB
speakers.  

This module presents a modified VxWorks SEQ_DEV interface to its callers.  The
SEQ_DEV interface was chosen because, of the existing VxWorks driver models, it
best supports the streaming data transfer model required by isochronous devices
such as USB speakers.  As with other VxWorks USB class drivers, the standard
driver interface has been expanded to support features unique to the USB and to
speakers in general.  Functions have been added to allow callers to recognize
the dynamic attachment and removal of speaker devices.	IOCTL functions have 
been added to retrieve and control additional settings related to speaker
operation.


INITIALIZATION

As with standard SEQ_DEV drivers, this driver must be initialized by calling
usbSpeakerDevInit().  usbSpeakerDevInit() in turn initializes its connection 
to the USBD and other internal resources needed for operation.	Unlike some 
SEQ_DEV drivers, there are no usbSpeakerLib.c data structures which need 
to be initialized prior to calling usbSpeakerDevInit().

Prior to calling usbSpeakerDevInit(), the caller must ensure that the USBD
has been properly initialized by calling - at a minimum - usbdInitialize().
It is also the caller's responsibility to ensure that at least one USB HCD
(USB Host Controller Driver) is attached to the USBD - using the USBD function
usbdHcdAttach() - before speaker operation can begin.  However, it is not 
necessary for usbdHcdAttach() to be alled prior to initializating usbSpeakerLib.
usbSpeakerLib.c uses the USBD dynamic attach services and is capable of 
recognizing USB speaker attachment and removal on the fly.  Therefore, it is 
possible for USB HCDs to be attached to or detached from the USBD at run time
- as may be required, for example, in systems supporting hot swapping of
hardware.


RECOGNIZING & HANDLING USB SPEAKERS

As noted earlier, the operation of USB speakers is defined in the USB Audio Class
Specification.	Speakers, loosely defined, are those USB audio devices which
provide an "Output Terminal".  For each USB audio device, usbSpeakerLib examines
the descriptors which enumerate the "units" and "terminals" contained within the
device.  These descriptors define both which kinds of units/terminals are present
and how they are connected.  

If an "Output Terminal" is found, usbSpeakerLib traces the device's internal
connections to determine which "Input Terminal" ultiminately provides the audio
stream for the "Output Terminal" and which, if any, Feature Unit is responsible
for controlling audio stream attributes like volume.  Once having built such an
internal "map" of the device, usbSpeakerLib configures the device and waits for
a caller to provide a stream of audio data.  If no "Output Terminal" is found,
usbSpeakerLib ignores the audio device.

After determining that the audio device contains an Output Terminal, 
usbSpeakerLib builds a list of the audio formats supported by the device. 
usbSpeakerLib supports only AudioStreaming interfaces (no MidiStreaming is 
supported).  

For each USB speaker attached to the system and properly recognized by 
usbSpeakerLib, usbSpeakerLib creates a SEQ_DEV structure to control the speaker.
Each speaker is uniquely identified by the pointer to its corresponding SEQ_DEV
structure.


DYNAMIC ATTACHMENT & REMOVAL OF SPEAKERS

As with other USB devices, USB speakers may be attached to or detached from the
system dynamically.  usbSpeakerLib uses the USBD's dynamic attach services in 
order to recognize these events.  Callers of usbSpeakerLib may, in turn, register
with usbSpeakerLib for notification when USB speakers are attached or removed
using the usbSpeakerDynamicAttachRegister() function.  When a USB speaker is
attached or removed, usbSpeakerLib invokes the attach notification callbacks 
for all registered callers.  The callback is passed the pointer to the affected
SEQ_DEV structure and a code indicated whether the speaker is being attached or
removed.

usbSpeakerLib maintains a usage count for each SEQ_DEV structure.  Callers can 
increment the usage count by calling usbSpeakSeqDevLock() and can decrement the 
usage count by calling usbSpeakerSeqDevUnlock().  When a USB speaker is removed
from the system and its usage count is 0, usbSpeakerLib automatically removes 
all data structures, including the SEQ_DEV structure itself, allocated on behalf
of the device.	Sometimes, however, callers rely on these data structures and 
must properly recognize the removal of the device before it is safe to destroy 
the underlying data structures.  The lock/unlock functions provide a mechanism
for callers to protect these data structures as needed.


DATA FLOW

Before sending audio data to a speaker device, the caller must specify the data
format (e.g., PCM, MPEG) using an IOCTL (see below).  The USB speaker itself
must support the indicated (or a similar) data format.

USB speakers rely on an uninterrupted, time-critical stream of audio data.  The
data is sent to the speaker through an isochronous pipe.  In order for the data
flow to continue uninterrupted, usbSpeakerLib internally uses a double-buffering
scheme.  When data is presented to usbSpeakerLib's sd_seqWrt() function by the 
caller, usbSpeakerLib copies the data into an internal buffer and immediately 
releases the caller's buffer.  The caller should immediately try to pass the next 
buffer to usbSpeakerLib.  When usbSpeakerLib's internal buffer is filled, it will 
block the caller until such time as it can accept the new data.  In this manner,
the caller and usbSpeakerLib work together to ensure that an adequate supply of
audio data will always be available to continue isochronous transmission 
uninterrupted.

Audio play begins after usbSpeakerLib has accepted half a second of audio data
or when the caller closes the audio stream, whichever happens first.  The caller
must use the IOCTLs to "open" and "close" each audio stream.  usbSpeakerLib
relies on these IOCTLs to manage its internal buffers correctly.


IOCTLs

usbSpeakerLib implements a number of IOCTLs unique to the handling of audio
data and devices.  usbSpeakerLib provides IOCTLs to set the following controls:
mute, volume, bass, mid-range, and treble.  usbSpeakerLib also provides IOCTLs
to be used by callers to interrogate a speaker's audio format capabilities or 
to specify the audio format for a subsequent data stream.


INCLUDE FILES: seqIo.h, usbAudio.h, usbSpeakerLib.h
*/


/* includes */

#include "vxWorks.h"
#include "string.h"
#include "ioLib.h"
#include "seqIo.h"
#include "errno.h"

#include "usb/usbPlatform.h"
#include "usb/ossLib.h" 	/* operations system srvcs */
#include "usb/usb.h"		/* general USB definitions */
#include "usb/usbListLib.h"	/* linked list functions */
#include "usb/usbdLib.h"	/* USBD interface */
#include "usb/usbLib.h" 	/* USB utility functions */
#include "usb/usbAudio.h"	/* USB audio class definitions */
#include "drv/usb/usbSpeakerLib.h"  /* our API */


/* defines */

#define SPKR_CLIENT_NAME	"usbSpeakerLib" /* our USBD client name */

#define A_REALLY_BIG_INTEGER	0x7fffffff  /* large positive integer */

#define MSEC_PER_SEC		1000L	/* number of msec per second */
#define FRAME_SKIP		20	/* frames to skip before play */
					/* (makes sure h/w doesn't run 
					 * ahead of us) 
					 */

#define IRP_COUNT	2		/* number of IRPs for transmit */


/* typedefs */

/* ATTACH_REQUEST */

typedef struct attach_request
    {
    LINK reqLink;			/* linked list of requests */
    USB_SPKR_ATTACH_CALLBACK callback;	/* client callback routine */
    pVOID callbackArg;			/* client callback argument */
    } ATTACH_REQUEST, *pATTACH_REQUEST;


/* USB_SPKR_SEQ_DEV is the internal data structure we use to track each USB
 * speaker.
 */

typedef struct usb_spkr_seq_dev
    {
    SEQ_DEV seqDev;		/* must be first field */
    BOOL reserved;		/* TRUE if device reserved */

    LINK devLink;		/* linked list of structs */

    UINT16 lockCount;		/* Count of times structure locked */

    USBD_NODE_ID nodeId;	/* speaker node Id */
    UINT16 configuration;	/* configuration/interface reported as */

    BOOL connected;		/* TRUE if speaker currently connected */

    UINT8 outputTerminalId;	/* ID of output terminal */
    UINT8 inputTerminalId;	/* ID of input terminal */
    UINT8 featureUnitId;	/* ID of feature unit (optional) */

    UINT8 channels;		/* count of channels */
    UINT16 channelConfig;	/* channel configuration */

    UINT16 capsLen;		/* size of following array */
    pUSB_SPKR_CHANNEL_CAPS pCaps;   /* Indicates feature support on each
				     * channel...channel 0 is the "master"
				     * and is global to all 
				     */

    UINT16 fmtCount;		/* count of audio formats */
    pUSB_SPKR_AUDIO_FORMAT pFmt;    /* indicates nature of each format */

    pUSB_SPKR_AUDIO_FORMAT pSelFmt; /* pointer to selected format */

    UINT32 sampleFrequency;	/* playback frequency */
    UINT32 sampleSize;		/* data always sent in blocks which are 
				 * multiples of this value 
				 */
    UINT32 bytesPerSec; 	/* playback bytes per second */

    BOOL open;			/* TRUE when audio stream is open */
    BOOL streamFault;		/* TRUE if stream fault detected */

    pUINT8 pAudioBfr;		/* bfr for audio data */
    UINT32 audioBfrLen; 	/* size of audio buffer */
    UINT32 audioBfrHalf;	/* one half the length of the buffer */
    UINT32 audioBfrCount;	/* count of data available */
    UINT32 audioBfrIn;		/* next location to receive data */
    UINT32 audioBfrOut; 	/* next location to transmit */
    UINT32 audioBfrPending;	/* nbr of bytes being transmitted */
    UINT32 audioBfrTotal;	/* total data passed through bfr */

    BOOL foregroundWaiting;	/* TRUE if foreground waiting for bfr */
    SEM_HANDLE bfrNotFullSem;	/* signalled when bfr goes from 
				 * full->not full 
				 */

    USBD_PIPE_HANDLE isochPipeHandle; /* USBD handle for isoch data pipe */

    UINT32 frameWindow; 	/* size of host controller frame ctr */
    UINT32 nextStartFrame;	/* next frame for isoch transfer */
    UINT32 blocksSent;		/* blocks sent for this stream */

    USB_IRP irp [IRP_COUNT];	/* IRPs to be used for transmit */
    UINT16 nextIrp;		/* index of next IRP to use */
    UINT16 nextDoneIrp; 	/* index of next IRP to complete */
    UINT16 irpsInUse;		/* count of IRPs in use */
    BOOL stopFlag;          /* Flag set on close of audio stream with param 1 */ 

    } USB_SPKR_SEQ_DEV, *pUSB_SPKR_SEQ_DEV;


/* forward function declarations */

LOCAL VOID usbSpkrIrpCallback 
    (
    pVOID p
    );


/* locals */

LOCAL UINT16 initCount = 0;	/* Count of init nesting */

LOCAL MUTEX_HANDLE speakerMutex;	/* mutex used to protect 
					 * internal structs 
					 */

LOCAL LIST_HEAD devList;	/* linked list of USB_SPKR_SEQ_DEV */
LOCAL LIST_HEAD reqList;	/* Attach callback request list */

LOCAL USBD_CLIENT_HANDLE usbdHandle;	/* our USBD client handle */


/***************************************************************************
*
* audioIfGetRequest - executes an audio-class-specific request
*
* Executes a request to an AudioControl or AudioStreaming interface.
* The actual length of data transferred must be equal to the <length> passed
* by the caller or an ERROR is returned.
*
* RETURNS: OK, else ERROR if not successful
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS audioIfRequest
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    UINT8 requestType,
    UINT8 request,
    UINT8 entityId,
    UINT16 value,
    UINT16 length,
    pVOID pBfr
    )
    
    {
    UINT16 actLen;

    if (usbdVendorSpecific (usbdHandle, 
			    pSeqDev->nodeId, 
			    requestType,
			    request, 
			    value, 
			    entityId << 8, 
			    length, 
			    (pUINT8) pBfr, 
			    &actLen) 
			  != OK 
	|| actLen != length)
	{
	return ERROR;
	}

    return OK;
    }


/***************************************************************************
*
* findAudioByUnit - seaches a buffer for the indicated audio descriptor
*
* This function searches a buffer for the indicated audio descriptor
*
* RETURNS: ptr to a matching descriptor or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pVOID findAudioDescrByUnit
    (
    pUINT8 pBfr,
    UINT16 bfrLen,
    UINT8 unitId
    )

    {
    pUSB_AUDIO_AC_COMMON pHdr;


    /* The remaining buffer length must be at least three bytes to accommodate
     * the USB_AUDIO_AC_COMMON structure. 
     */

    while (bfrLen >= sizeof (*pHdr))
	{
	pHdr = (pUSB_AUDIO_AC_COMMON) pBfr;

	if (pHdr->length == 0)
	    return NULL;	/* must be invalid */

	pBfr += min (pHdr->length, bfrLen);
	bfrLen -= min (pHdr->length, bfrLen);

	if (pHdr->descriptorType == USB_DESCR_AUDIO_INTERFACE &&
				    pHdr->unitId == unitId)
	    return (pVOID) pHdr;
	}

    return NULL;
    }


/***************************************************************************
*
* findAudioDescrSkip - seaches a buffer for the indicated audio descriptor
*
* Upon return, <ppBfr> points to the descriptor following the last searhed
* descriptor and <pBfrLen> is updated with the number of bytes remaining in 
* the bfr.
*
* RETURNS: ptr to a matching descriptor or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pVOID findAudioDescrSkip
    (
    pUINT8 *ppBfr,
    pUINT16 pBfrLen,
    UINT8 descriptorType,
    UINT8 descriptorSubType
    )

    {
    pUSB_DESCR_HDR_SUBHDR pHdr;


    /* The remaining buffer length must be at least three bytes to accommodate
     * the descriptor length, descriptorType, and descriptorSubType fields. 
     */

    while (*pBfrLen >= sizeof (*pHdr))
	{
	pHdr = (pUSB_DESCR_HDR_SUBHDR) *ppBfr;

	if (pHdr->length == 0)
	    return NULL;	/* must be invalid */

	*ppBfr += min (pHdr->length, *pBfrLen);
	*pBfrLen -= min (pHdr->length, *pBfrLen);

	if (pHdr->descriptorType == descriptorType &&
	    pHdr->descriptorSubType == descriptorSubType)
	    return (pVOID) pHdr;
	}

    return NULL;
    }


/***************************************************************************
*
* findAudioDescr - seaches a buffer for the indicated audio descriptor
*
* This function seaches a buffer for the indicated audio descriptor.
*
* RETURNS: ptr to a matching descriptor or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pVOID findAudioDescr
    (
    pUINT8 pBfr,
    UINT16 bfrLen,
    UINT8 descriptorType,
    UINT8 descriptorSubType
    )

    {
    return findAudioDescrSkip (&pBfr, 
			       &bfrLen, 
			       descriptorType, 
			       descriptorSubType);
    }


/***************************************************************************
*
* setChannelControl - Sets the specified channel control
*
* <funcCode> is the IOCTL code specifying the control to set.  <channel>
* is the channel number (channel 0 = master channel).  <value> is the
* new setting value.
*
* RETURNS: OK if successful, ENOSYS if not supported, else EIO if failure
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS setChannelControl
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    int funcCode,
    UINT16 channel,
    UINT16 value
    )

    {
    pUSB_SPKR_CHANNEL_CAPS pChannelCaps;
    pUSB_SPKR_CTL_CAPS pCtlCaps = NULL;
    UINT8 controlSelector = 0;
    UINT8 settingWidth = 0;
    UINT16 * pSetting;

    if ((pSetting = OSS_MALLOC (sizeof (UINT16))) == NULL)
	return EIO;

    /* validate channel number.  0 = master channel.  1..n = other channels. */

    if (channel > pSeqDev->channels)
	return EIO;

    pChannelCaps = &pSeqDev->pCaps [channel];


    /* Check the capabilities of the control */

    switch (funcCode)
	{
	case USB_SPKR_IOCTL_SET_MUTE:

	    pCtlCaps = NULL;
	    controlSelector = USB_AUDIO_FCS_VOLUME;
	    settingWidth = USB_AUDIO_MUTE_ATTR_WIDTH;
	    break;

	case USB_SPKR_IOCTL_SET_VOLUME:

	    pCtlCaps = &pChannelCaps->volume;
	    controlSelector = USB_AUDIO_FCS_VOLUME;
	    settingWidth = USB_AUDIO_VOLUME_ATTR_WIDTH;
	    break;

	case USB_SPKR_IOCTL_SET_BASS:

	    pCtlCaps = &pChannelCaps->bass;
	    controlSelector = USB_AUDIO_FCS_BASS;
	    settingWidth = USB_AUDIO_BASS_ATTR_WIDTH;
	    break;

	case USB_SPKR_IOCTL_SET_MID:

	    pCtlCaps = &pChannelCaps->mid;
	    controlSelector = USB_AUDIO_FCS_MID;
	    settingWidth = USB_AUDIO_MID_ATTR_WIDTH;
	    break;

	case USB_SPKR_IOCTL_SET_TREBLE:

	    pCtlCaps = &pChannelCaps->treble;
	    controlSelector = USB_AUDIO_FCS_TREBLE;
	    settingWidth = USB_AUDIO_TREBLE_ATTR_WIDTH;
	    break;
	}

    if (pCtlCaps != NULL)
	{
	if (!pCtlCaps->supported)
	    return ENOSYS;

	if (((INT16) value) < pCtlCaps->min || ((INT16) value) > pCtlCaps->max)
	    return EIO;
	}


    /* Set the control */

    *pSetting = TO_LITTLEW (value);

    if (audioIfRequest (pSeqDev,
			USB_RT_HOST_TO_DEV | USB_RT_CLASS | USB_RT_INTERFACE,
			USB_REQ_AUDIO_SET_CUR,
			pSeqDev->featureUnitId,
			controlSelector << 8 | channel,
			settingWidth,
			pSetting)
		      != OK)
	{
	OSS_FREE (pSetting);
	return EIO;
	}
    if(pCtlCaps != NULL)
        pCtlCaps->cur = value;

    OSS_FREE (pSetting);

    return OK;
    }


/***************************************************************************
*
* getControlSetting - read an individual setting for specified control
*
* <settingWidth> specifies the control value width as 1 (byte) or 2 (word).
* 
* RETURNS: OK if successfull, else ERROR
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS getControlSetting
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    UINT8 entityId,
    UINT8 channel,
    UINT8 controlSelector,
    UINT8 request,
    pUINT16 pAttribute,
    UINT16 settingWidth
    )

    {
    UINT16 * pAttributeBuf;

    if ((pAttributeBuf = OSS_CALLOC (sizeof (UINT16))) == NULL)
	return ERROR;

    if (audioIfRequest (pSeqDev,
			USB_RT_DEV_TO_HOST | USB_RT_CLASS | USB_RT_INTERFACE, 
			request, 
			entityId, 
			controlSelector << 8 | channel, 
			settingWidth, 
			pAttributeBuf) 
		      != OK)
	{
	OSS_FREE (pAttributeBuf);
	return ERROR;
	}

    *pAttribute = FROM_LITTLEW (*pAttributeBuf); 
    
    OSS_FREE (pAttributeBuf);

    return OK;
    }


/***************************************************************************
*
* getControlSettings - reads settings for the specified feature unit control
*
* If the information can be read successfully, <pCtlCaps>->supported is set 
* to TRUE.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID getControlSettings
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    UINT8 entityId,
    UINT8 channel,
    UINT8 controlSelector,
    pUSB_SPKR_CTL_CAPS pCtlCaps,
    UINT16 settingWidth
    )

    {
    /* Get the control's resolution. */

    if (getControlSetting (pSeqDev, 
			   entityId, 
			   channel, 
			   controlSelector,
			   USB_REQ_AUDIO_GET_RES, 
			   &pCtlCaps->res, 
			   settingWidth) 
			 != OK)
	return;

    /* Get the control's min setting */

    if (getControlSetting (pSeqDev, 
			   entityId, 
			   channel, 
			   controlSelector,
			   USB_REQ_AUDIO_GET_MIN, 
			   (UINT16 *)&pCtlCaps->min, 
			   settingWidth) 
			 != OK)
	return;

    /* Get the control's max setting */

    if (getControlSetting (pSeqDev, 
			   entityId, 
			   channel, 
			   controlSelector,
			   USB_REQ_AUDIO_GET_MAX, 
			   (UINT16 *)&pCtlCaps->max, 
			   settingWidth) 
			 != OK)
	return;
    
    /* Get the control's current setting */

    if (getControlSetting (pSeqDev, 
			   entityId, 
			   channel, 
			   controlSelector,
			   USB_REQ_AUDIO_GET_CUR, 
			   (UINT16 *)&pCtlCaps->cur, 
			   settingWidth) 
			 != OK)
	return;

    pCtlCaps->supported = TRUE;
    }


/***************************************************************************
*
* recordChannelsCaps - record capabilities for the specified channel
*
* This function records capabilities for the specified channel
* 
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID recordChannelCaps
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    UINT8 entityId,
    UINT8 channel,
    pUSB_SPKR_CHANNEL_CAPS pCaps,
    UINT8 controls
    )

    {
    if ((controls & USB_AUDIO_FCM_MUTE) != 0)
	pCaps->mute.supported = TRUE;

    if ((controls & USB_AUDIO_FCM_VOLUME) != 0)
	getControlSettings (pSeqDev, 
			    entityId, 
			    channel,
			    USB_AUDIO_FCS_VOLUME, 
			    &pCaps->volume, 
			    USB_AUDIO_VOLUME_ATTR_WIDTH);

    if ((controls & USB_AUDIO_FCM_BASS) != 0)
	getControlSettings (pSeqDev, 
			    entityId, 
			    channel,
			    USB_AUDIO_FCS_BASS, 
			    &pCaps->bass, 
			    USB_AUDIO_BASS_ATTR_WIDTH);

    if ((controls & USB_AUDIO_FCM_MID) != 0)
	getControlSettings (pSeqDev, 
			    entityId, 
			    channel,
			    USB_AUDIO_FCS_MID, 
			    &pCaps->mid, 
			    USB_AUDIO_MID_ATTR_WIDTH);

    if ((controls & USB_AUDIO_FCM_TREBLE) != 0)
	getControlSettings (pSeqDev, 
			    entityId, 
			    channel,	    
			    USB_AUDIO_FCS_TREBLE, 
			    &pCaps->treble, 
			    USB_AUDIO_TREBLE_ATTR_WIDTH);
    }


/***************************************************************************
*
* buildAudioMap - determine what capabilities the audio device has
*
* Search the provided <pCfgBfr> for an AudioClass (AC) descriptor and 
* from that descriptor determine what capabilities the device has. 
*
* RETURNS: OK if successful, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS buildAudioMap
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    pUINT8 pCfgBfr,
    UINT16 cfgLen
    )

    {
    pUSB_AUDIO_AC_DESCR pAcDescr;
    pUSB_AUDIO_AC_COMMON pAcCommon;
    pUSB_AUDIO_OUTPUT_TERM_DESCR pOutputTerm;
    pUSB_AUDIO_INPUT_TERM_DESCR pInputTerm;
    pUSB_AUDIO_FEATURE_UNIT_DESCR pFeatureUnit = NULL;
    pUINT8 pBfr;
    UINT16 bfrLen;
    UINT16 totalLength;
    UINT16 remLen;
    UINT8 prevId;
    pUINT8 pControl;
    UINT16 i;


    /* Find the first AudioControl (AC) descriptor */

    pBfr = pCfgBfr;
    bfrLen = cfgLen;

    if ((pAcDescr = findAudioDescrSkip (&pBfr, 
					&bfrLen, 
					USB_DESCR_AUDIO_INTERFACE, 
					USB_DESCR_AUDIO_AC_HEADER)) 
				      == NULL)
	return ERROR;


    /* Find an "Output Terminal".  Then, using the terminal/unit numbers as
     * links, work backwards from the output terminal to find the "Input 
     * Terminal" and "Feature Unit" (if any) associated with it.
     */

    totalLength = pAcDescr->totalLength [0] | (pAcDescr->totalLength [1] << 8);

    remLen = min (totalLength - sizeof (*pAcDescr), bfrLen);

    if ((pOutputTerm = findAudioDescr (pBfr, 
				       remLen, 
				       USB_DESCR_AUDIO_INTERFACE, 
				       USB_DESCR_AUDIO_AC_OUTPUT_TERMINAL)) 
				     == NULL)
	return ERROR;

    pSeqDev->outputTerminalId = pOutputTerm->terminalId;

    prevId = pOutputTerm->sourceId;

    while (prevId != 0 && pSeqDev->inputTerminalId == 0)
	{
	if ((pAcCommon = findAudioDescrByUnit (pBfr, remLen, prevId)) == NULL)
	    return FALSE;

	switch (pAcCommon->descriptorSubType)
	    {
	    case USB_DESCR_AUDIO_AC_INPUT_TERMINAL:

		/* Record characteristics of the input terminal */

		pInputTerm = (pUSB_AUDIO_INPUT_TERM_DESCR) pAcCommon;
		pSeqDev->inputTerminalId = pInputTerm->terminalId;
		pSeqDev->channels = pInputTerm->channels;
		pSeqDev->channelConfig = pInputTerm->channelConfig [0] 
					| (pInputTerm->channelConfig [1] << 8);
		prevId = 0;

		/* If there is a feature unit descriptor, record its 
		 * capabilities on a channel-by-channel basis. 
		 */

		if (pFeatureUnit != NULL)
		    {
		    /* Allocate space for info on a per-channel basis.
		     *
		     * NOTE: Array position 0 is for "master" (ie. 
		     * global) channel.
		     */

		    pSeqDev->capsLen = 
		    (pSeqDev->channels + 1) * sizeof (USB_SPKR_CHANNEL_CAPS);

		    if ((pSeqDev->pCaps = OSS_CALLOC (pSeqDev->capsLen)) 
						     == NULL)
			return ERROR;


		    /* Record capabilities */

		    for (i = 0; i <= pSeqDev->channels; i++)
			{
			pControl = &pFeatureUnit->controls[i * pFeatureUnit->controlSize];

			recordChannelCaps (pSeqDev, 
					   pFeatureUnit->unitId, 
					   i, 
					   &pSeqDev->pCaps [i], 
					   *pControl);
			}
		    }

		break;

	    case USB_DESCR_AUDIO_AC_MIXER_UNIT:

		prevId = ((pUSB_AUDIO_MIXER_UNIT_DESCR) pAcCommon)->sourceId [0];
		break;

	    case USB_DESCR_AUDIO_AC_SELECTOR_UNIT:

		prevId = ((pUSB_AUDIO_SELECTOR_UNIT_DESCR) pAcCommon)->sourceId [0];
		break;

	    case USB_DESCR_AUDIO_AC_FEATURE_UNIT:

		/* Record capabilities of the feature unit */

		pFeatureUnit = (pUSB_AUDIO_FEATURE_UNIT_DESCR) pAcCommon;
		pSeqDev->featureUnitId = pFeatureUnit->unitId;
		prevId = pFeatureUnit->sourceId;
		break;

	    case USB_DESCR_AUDIO_AC_PROCESSING_UNIT:

		prevId = ((pUSB_AUDIO_PROCESS_UNIT_DESCR) pAcCommon)->sourceId [0];
		break;

	    case USB_DESCR_AUDIO_AC_EXTENSION_UNIT:

		prevId = ((pUSB_AUDIO_EXT_UNIT_DESCR) pAcCommon)->sourceId [0];
		break;

	    case USB_DESCR_AUDIO_AC_UNDEFINED:
	    case USB_DESCR_AUDIO_AC_HEADER:
	    case USB_DESCR_AUDIO_AC_OUTPUT_TERMINAL:
	    default:

		/* These terminal/unit types should not be encountered at
		 * this point in the algorithm.
		 */

		return ERROR;
	    }
	}

    return OK;
    }


/***************************************************************************
*
* reallocateFormatList - increases room for format list by one entry
*
* This function increases room for format list by one entry
*
* RETURNS: OK, else ERROR if out of memory
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS reallocateFormatList
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    pUSB_SPKR_AUDIO_FORMAT pNewList;

    pSeqDev->fmtCount++;

    if ((pNewList = OSS_CALLOC (pSeqDev->fmtCount * 
				sizeof (USB_SPKR_AUDIO_FORMAT))) 
			       == NULL)
	return ERROR;

    if (pSeqDev->fmtCount > 1 && pSeqDev->pFmt != NULL)
	{
	memcpy (pNewList, 
		pSeqDev->pFmt, 
		(pSeqDev->fmtCount - 1) * sizeof (USB_SPKR_AUDIO_FORMAT));

	OSS_FREE (pSeqDev->pFmt);
	}

    pSeqDev->pFmt = pNewList;

    return OK;
    }


/***************************************************************************
*
* parseInterface - parse an interface descriptor, looking for audio formats
*
* NOTE: <cfgLen> should count bytes only through the end of this interface.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID parseInterface
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    pUSB_INTERFACE_DESCR pIfDescr,
    pUINT8 pCfgBfr,
    UINT16 cfgLen
    )

    {
    pUSB_AUDIO_AS_DESCR pAsDescr;
    pUSB_AUDIO_TYPE_DESCR pType;
    pUSB_AUDIO_STD_ISOCH_EP_DESCR pStdEp;
    pUSB_SPKR_AUDIO_FORMAT pFmt;
    USB_SPKR_AUDIO_FORMAT fmt = {0};


    /* If this interface doesn't contain a class-specific AS (AudioStreaming)
     * descriptor then ignore it.  If it does, but the AudioStream doesn't
     * link to the inputTerminalId, then ignore it anyway.
     */

    if ((pAsDescr = findAudioDescrSkip (&pCfgBfr, 
					&cfgLen, 
					USB_DESCR_AUDIO_INTERFACE, 
					USB_DESCR_AUDIO_AS_GENERAL)) 
				       == NULL)
	return;

    if (pAsDescr->terminalLink != pSeqDev->inputTerminalId)
	return;


    /* We must now find a format type descriptor and an endpoint descriptor. */

    if ((pType = findAudioDescr (pCfgBfr, 
				 cfgLen,
				 USB_DESCR_AUDIO_INTERFACE, 
				 USB_DESCR_AUDIO_AS_FORMAT_TYPE)) 
				== NULL)
	return;

    if ((pStdEp = usbDescrParse (pCfgBfr, cfgLen, USB_DESCR_ENDPOINT)) == NULL)
	return;


    /* Construct a format record from the accumulated data. */

    fmt.interface = pIfDescr->interfaceNumber;
    fmt.altSetting = pIfDescr->alternateSetting;
    fmt.delay = pAsDescr->delay;
    fmt.endpoint = pStdEp->endpointAddress;
    fmt.maxPacketSize = pStdEp->maxPacketSize [0] 
			| (pStdEp->maxPacketSize [1] << 8);
    fmt.formatTag = pAsDescr->formatTag [0] | (pAsDescr->formatTag [1] << 8);
    fmt.formatType = pType->formatType;

    switch (pType->formatType)
	{
	case USB_AUDIO_FORMAT_TYPE1:

	    fmt.channels = pType->ts.type1.nbrChannels;
	    fmt.subFrameSize = pType->ts.type1.subFrameSize;
	    fmt.bitRes = pType->ts.type1.bitResolution;
	    break;

	case USB_AUDIO_FORMAT_TYPE2:

	    fmt.maxBitRate = pType->ts.type2.maxBitRate [0] |
	    (pType->ts.type2.maxBitRate [1] << 8);
	    fmt.samplesPerFrame = pType->ts.type2.samplesPerFrame [0] 
				  | (pType->ts.type2.samplesPerFrame [1] << 8);
	    break;

	case USB_AUDIO_FORMAT_TYPE3:

	    fmt.channels = pType->ts.type3.nbrChannels;
	    fmt.subFrameSize = pType->ts.type3.subFrameSize;
	    fmt.bitRes = pType->ts.type3.bitResolution;
	    break;

	default:	/* We don't recognize this format */

	    return;
	}


    /* Copy new format record to SEQ_DEV structure. */

    if (reallocateFormatList (pSeqDev) != OK)
	return;

    pFmt = &pSeqDev->pFmt [pSeqDev->fmtCount - 1];
    *pFmt = fmt;
    }


/***************************************************************************
*
* buildFormatList - builds a list of formats supported by the speaker
*
* This function builds a list of formats supported by the speaker
* 
* RETURNS: OK if successful, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS buildFormatList
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    pUINT8 pCfgBfr,
    UINT16 cfgLen
    )

    {
    pUSB_INTERFACE_DESCR pIfDescr;
    pUINT8 pTmp;
    UINT16 tmpLen;
    UINT16 rangeLen;


    /* Find each interface descriptor of type AudioStreaming.  Determine the 
     * length of the descriptor, and then parse each format type supported by 
     * the interface.
     */

    while ((pIfDescr = usbDescrParseSkip (&pCfgBfr, 
					  &cfgLen, 
					  USB_DESCR_INTERFACE)) 
					!= NULL)
	{
	if (pIfDescr->interfaceClass == USB_CLASS_AUDIO &&
	    pIfDescr->interfaceSubClass == USB_SUBCLASS_AUDIO_AUDIOSTREAMING)
	    {
	    /* We found an AudioStreaming interface descriptor.  Now, determine 
	     * how many bytes until the next interface descriptor.	This 
	     * determines the range of the configuration descriptor we will 
	     * examine while parsing this interface.
	     */

	    pTmp = pCfgBfr;
	    tmpLen = cfgLen;

	    if (usbDescrParseSkip (&pTmp, 
				   &tmpLen, 
				   USB_DESCR_INTERFACE) 
				 != NULL)
		rangeLen = cfgLen - tmpLen;
	    else
		rangeLen = cfgLen;

	    parseInterface (pSeqDev, pIfDescr, pCfgBfr, rangeLen);
	    }
	}


    return OK;
    }


/***************************************************************************
*
* selectFormat - selects an audio format to match the caller's request
*
* <pReqFmt> points to a USB_SPEAKER_AUDIO_FORMAT structure in which the
* "formatTag" field has been initialized to the desired format as
* USB_AUDIO_TYPEn_xxxx and "formatType" has been initialized with the 
* format type as USB_AUDIO_FORMAT_TYPEn.  
*
* Depending on the specified format, the fields "channels", "subFrameSize", 
* "bitRes", "sampleFrequency", "maxBitRate", and "samplesPerFrame" may also 
* need to be specified to qualify the format.  The other fields of the 
* structure *do not* need to be specified.
*
* The field "sampleFrequency", which must be set for Type I & Type 3 formats
* must be set with the desired playback frequency.  The frequency is 
* expressed as samples per sec.  For example, a sampleFrequency of 88,500 
* is 88,500 samples per second per channel.
*
* This function will attempt to find an audio format supported by the
* speaker which is an exact match for the specified format.  If found, that
* format will be selected.  If no exact match can be found, an error will be
* returned.
*
* RETURNS: OK if successful, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS selectFormat
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    pUSB_SPKR_AUDIO_FORMAT pReqFmt
    )

    {
    pUSB_SPKR_AUDIO_FORMAT pFmt;
    UINT16 i;

    /* Set the format type to "unspecified". */

    pSeqDev->pSelFmt = NULL;

    pSeqDev->sampleFrequency = 0;
    pSeqDev->sampleSize = 0;


    /* Look for an exact match for the format */

    for (i = 0; i < pSeqDev->fmtCount; i++)
	{
	pFmt = &pSeqDev->pFmt [i];

	switch (pReqFmt->formatType)
	    {
	    case USB_AUDIO_FORMAT_TYPE1:
	    case USB_AUDIO_FORMAT_TYPE3:

		if (pReqFmt->formatTag == pFmt->formatTag &&
		    pReqFmt->channels == pFmt->channels &&
		    pReqFmt->subFrameSize == pFmt->subFrameSize &&
		    pReqFmt->bitRes == pFmt->bitRes)

		    {
		    pSeqDev->pSelFmt = pFmt;
		    pSeqDev->sampleFrequency = pReqFmt->sampleFrequency;
		    pSeqDev->sampleSize = pFmt->channels * pFmt->subFrameSize;
		    pSeqDev->bytesPerSec = 
		    pSeqDev->sampleFrequency * pSeqDev->sampleSize;
		    return OK;
		    }
		break;

	    case USB_AUDIO_FORMAT_TYPE2:

	    if (pReqFmt->formatTag == pFmt->formatTag &&
		pReqFmt->maxBitRate == pFmt->maxBitRate &&
		pReqFmt->samplesPerFrame == pFmt->samplesPerFrame)

		{
		pSeqDev->pSelFmt = pFmt;
		pSeqDev->sampleFrequency = pReqFmt->sampleFrequency;
		pSeqDev->sampleSize = 1;
		pSeqDev->bytesPerSec = 
		pSeqDev->sampleFrequency * pSeqDev->sampleSize;
		return OK;
		}
	    break;

	    default:    /* not supported */

	    return ERROR;
	    }
	}


    /* No compatible format was found. */

    return ERROR;
    }


/***************************************************************************
*
* parseAudioConfig - parse configuration descriptor for audio device
*
* Examine the configuration descriptor and build a map of the device's
* capabilities.  To be used by this speaker class driver, the device must
* have an "Output Terminal".  If it does, then determine which audio
* formats can be passed to the device.
*
* RETURNS: OK if successful, else ERROR
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS parseAudioConfig
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    pUINT8 pCfgBfr,
    UINT16 cfgLen,
    pUSB_CONFIG_DESCR pCfgDescr
    )

    {
    /* Find the Audio Control descriptor and build a map of the device */

    if (buildAudioMap (pSeqDev, pCfgBfr, cfgLen) != OK)
	return ERROR;

    if (pSeqDev->outputTerminalId == 0 || pSeqDev->inputTerminalId == 0)
	return ERROR;


    /* The audio device has the necessary Output Terminal.  Parse the
     * configuration descriptor and make a list of the supported audio
     * formats.
     */

    if (buildFormatList (pSeqDev, pCfgBfr, cfgLen) != OK)
	return ERROR;

    if (pSeqDev->fmtCount == 0)
	return ERROR;

    return OK;
    }


/***************************************************************************
*
* configureSeqDev - configure USB speaker for operation
*
* Examine the device's configuration & associated descriptors to determine
* the audio device's capability.  If the device is an audio device capable
* of output (e.g., it includes an "Output Terminal") then build a structure
* to describe the device and configure it.
*
* RETURNS: OK if successful, else ERROR if failed to configure channel
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS configureSeqDev
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUINT8 pCfgBfr;
    UINT16 cfgLen;
    STATUS retcode;


    /* Read the configuration descriptor so we can determine the device's
     * capabilities.
     */

    if (usbConfigDescrGet (usbdHandle, 
			   pSeqDev->nodeId, 
			   pSeqDev->configuration, 
			   &cfgLen, 
			   &pCfgBfr) 
			 != OK)
	return ERROR;

    if ((pCfgDescr = usbDescrParse (pCfgBfr, 
				    cfgLen, 
				    USB_DESCR_CONFIGURATION)) 
				  == NULL)
	{
	retcode = ERROR;
	}
    else
	{
	/* Parse the configuration descriptor to see if the device has the
	 * capabilities we need to use it as a speaker.
	 */

	retcode = parseAudioConfig (pSeqDev, pCfgBfr, cfgLen, pCfgDescr);
	}


    if (retcode == OK)
	{
	/* Select the configuration. */

	if (usbdConfigurationSet (usbdHandle, 
				  pSeqDev->nodeId,
				  pCfgDescr->configurationValue, 
				  pCfgDescr->maxPower * USB_POWER_MA_PER_UNIT) 
				!= OK)
	    {
	    retcode = ERROR;
	    }
	}


    /* Release the buffer allocated by usbConfigDescrGet(). */

    OSS_FREE (pCfgBfr);

    return retcode;
    }


/***************************************************************************
*
* destroyAudioStream - releases audio stream, bandwidth, resources
*
* Releases the resources allocated for an audio stream as long as play
* has completed.  If <unconditional> is TRUE, then the stream is 
* destroyed whether or not play has completed. 
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID destroyAudioStream
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    BOOL unconditional
    )

    {
    /* If still playing, return. */

    if (unconditional 
	|| pSeqDev->streamFault 
	|| (!pSeqDev->open && pSeqDev->audioBfrCount == 0)
    || (!pSeqDev->open && pSeqDev->stopFlag == TRUE) )
	{
	/* Destroy the isoch. pipe. */

	if (pSeqDev->isochPipeHandle != NULL)
	    {
	    usbdPipeDestroy (usbdHandle, pSeqDev->isochPipeHandle);
	    pSeqDev->isochPipeHandle = NULL;
	    }

	/* Release buffer-not-full semaphore */

	if (pSeqDev->bfrNotFullSem != NULL)
	    {
	    OSS_SEM_DESTROY (pSeqDev->bfrNotFullSem);
	    pSeqDev->bfrNotFullSem = NULL;
	    }

	/* Release the audio data buffer */

	if (pSeqDev->pAudioBfr != NULL)
	    {
	    OSS_FREE (pSeqDev->pAudioBfr);
	    pSeqDev->pAudioBfr = NULL;
	    }
	
    /* reset the audioBfrCount */
    pSeqDev->audioBfrCount = 0;

    /* Reset Stopflag */
    if(pSeqDev->stopFlag == TRUE)
       pSeqDev->stopFlag = FALSE;



	}
    }


/***************************************************************************
*
* openAudioStream - Sets up speaker to begin receiving audio data
*
* This function sets up speaker to begin receiving audio data
*
* RETURNS: OK if successful, else EIO if error.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS openAudioStream
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    pUSB_SPKR_AUDIO_FORMAT pFmt = pSeqDev->pSelFmt;
    UINT32 halfBfrLen;


    /* If the audio stream is already open or data is still in the bfr,
     * return an error.
     */

    if (pSeqDev->open || pSeqDev->audioBfrCount > 0)
	return EIO;

    /* Return an error if no audio format specified. */

    if (pFmt == NULL)
	return EIO;


    /* Select the audio interface/alt setting for the indicated format */

    if (usbdInterfaceSet (usbdHandle, 
			  pSeqDev->nodeId,
			  pFmt->interface, 
			  pFmt->altSetting)
			!= OK)
	return EIO;


    /* Calculate the size of the data buffer for the audio stream.  We 
     * calculate the buffer size to ensure one second of buffering capability
     * and to ensure that each half of the buffer accomodates an exactly
     * multiple of the sample size.
     */

    halfBfrLen = pSeqDev->bytesPerSec / 2;
    halfBfrLen = ((halfBfrLen + pSeqDev->sampleSize - 1) / pSeqDev->sampleSize)
			* pSeqDev->sampleSize;


    /* Allocate resources for the stream. */

    pSeqDev->streamFault = FALSE;

    if ((pSeqDev->pAudioBfr = OSS_MALLOC (halfBfrLen * 2)) == NULL)
	return EIO;

    pSeqDev->audioBfrLen = halfBfrLen * 2;
    pSeqDev->audioBfrHalf = halfBfrLen;
    pSeqDev->audioBfrCount = 0;
    pSeqDev->audioBfrIn = 0;
    pSeqDev->audioBfrOut = 0;
    pSeqDev->audioBfrPending = 0;
    pSeqDev->audioBfrTotal = 0;

    pSeqDev->foregroundWaiting = FALSE;

    if (OSS_SEM_CREATE (1, 0, &pSeqDev->bfrNotFullSem) != OK)
	{
	destroyAudioStream (pSeqDev, TRUE);
	return EIO;
	}

    pSeqDev->nextStartFrame = 0;
    pSeqDev->blocksSent = 0;

    pSeqDev->nextIrp = 0;
    pSeqDev->nextDoneIrp = 0;
    pSeqDev->irpsInUse = 0;


    /* Create the isochronous transmit pipe for the audio data. */

    if (usbdPipeCreate (usbdHandle, 
			pSeqDev->nodeId, 
			pFmt->endpoint,
			pSeqDev->configuration, 
			pFmt->interface, 
			USB_XFRTYPE_ISOCH,
			USB_DIR_OUT, 
			pFmt->maxPacketSize, 
			pSeqDev->bytesPerSec, 
			1, 
			&pSeqDev->isochPipeHandle) 
		      != OK)
	{
	destroyAudioStream (pSeqDev, TRUE);
	return EIO;
	}


    pSeqDev->open = TRUE;

    return OK;
    }


/***************************************************************************
*
* closeAudioStream - Marks end of audio stream.
*
* This function marks end of audio stream.
*
* RETURNS: OK if successful, else EIO if error.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS closeAudioStream
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    /* If we're not playing, return an error. */

    if (!pSeqDev->open)
	return EIO;


    /* tear down the isoch pipe, etc., if the audio transmission is 
     * complete...if still transmitting, the stream will be torn down
     * upon completion of the data transfer.
     */

    pSeqDev->open = FALSE;

    destroyAudioStream (pSeqDev, FALSE);

    return OK;
    }


/***************************************************************************
*
* updateTransmitter - starts/restarts double-buffered I/O
*
* NOTE: Caller must ensure speakerMutex is taken before calling this
* routine.
*
* RETURNS: OK if successful, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS updateTransmitter
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    pUSB_IRP pIrp;
    UINT32 currentFrame;
    UINT32 extLen;
    UINT32 frameCount;
    STATUS status = OK;


    /* If an IRP is available and there is data available but not yet
     * being transmitted, then send it.
     */

    while (!pSeqDev->streamFault 
	    && pSeqDev->irpsInUse < IRP_COUNT 
	    && ((pSeqDev->audioBfrCount - pSeqDev->audioBfrPending 
	         >= pSeqDev->audioBfrHalf) 
	    || !pSeqDev->open))
	{
	/* If this is the first block of data, initialize the starting
	 * frame number.
	 */

	if (pSeqDev->blocksSent == 0)
	    {
	    if (usbdCurrentFrameGet (usbdHandle, 
				     pSeqDev->nodeId,
				     &currentFrame, 
				     &pSeqDev->frameWindow) 
				   != OK)
		currentFrame = 0;

	    pSeqDev->nextStartFrame = (currentFrame + FRAME_SKIP) %
	    pSeqDev->frameWindow;
	    }


	/* Use the next IRP to transmit a block of data.  Each IRP always
	 * transfers half of the data buffer or the entire block of 
	 * remaining data if the stream has been closed.
	 */

	extLen = pSeqDev->audioBfrCount - pSeqDev->audioBfrPending;
	extLen = min (extLen, pSeqDev->audioBfrHalf);

	if (extLen < pSeqDev->audioBfrHalf && pSeqDev->open)
	    break;

	frameCount = ((extLen * MSEC_PER_SEC) + pSeqDev->bytesPerSec - 1) / 
	    pSeqDev->bytesPerSec;

	if (frameCount == 0)
	    break;


	/* Initialize the IRP. */

	pIrp = &pSeqDev->irp [pSeqDev->nextIrp];

	pSeqDev->irpsInUse++;

	if (++pSeqDev->nextIrp == IRP_COUNT)
	    pSeqDev->nextIrp = 0;

	memset (pIrp, 0, sizeof (*pIrp));
	
	pIrp->userPtr = pSeqDev;
	pIrp->irpLen = sizeof (*pIrp);
	pIrp->userCallback = usbSpkrIrpCallback;
	pIrp->timeout = USB_TIMEOUT_NONE;
	pIrp->startFrame = pSeqDev->nextStartFrame;
	pIrp->dataBlockSize = pSeqDev->sampleSize;

	pIrp->transferLen = extLen;
	pIrp->bfrCount = 1;
	pIrp->bfrList [0].pid = USB_PID_OUT;
	pIrp->bfrList [0].pBfr = &pSeqDev->pAudioBfr [pSeqDev->audioBfrOut];
	pIrp->bfrList [0].bfrLen = extLen;

	pSeqDev->audioBfrPending += extLen;
	
	if ((pSeqDev->audioBfrOut += extLen) == pSeqDev->audioBfrLen)
	    pSeqDev->audioBfrOut = 0;

	pSeqDev->nextStartFrame = 
	    (pSeqDev->nextStartFrame + frameCount) % pSeqDev->frameWindow;

	pSeqDev->blocksSent++;


	/* Send the data */
	
	if (usbdTransfer (usbdHandle, pSeqDev->isochPipeHandle, pIrp) != OK)
	    {
	    pSeqDev->streamFault = TRUE;
	    status = ERROR;
	    }
	}

    return status;
    }


/***************************************************************************
*
* usbSpkrIrpCallback - invoked when IRPs complete
*
* Examines the result of each IRP.  Attempts to initiate new transmission.
*
* NOTE: By convention, the userPtr field in each IRP is a pointer to the
* corresponding USB_SPKR_SEQ_DEV structure.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID usbSpkrIrpCallback 
    (
    pVOID p
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;
    pUSB_SPKR_SEQ_DEV pSeqDev = (pUSB_SPKR_SEQ_DEV) pIrp->userPtr;


    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);


    /* Look for out-of-order IRP completion */

    if (pIrp != &pSeqDev->irp [pSeqDev->nextDoneIrp])
	{
	pSeqDev->streamFault = TRUE;
	}
    else
	{
	/* Indicate an IRP has completed and update buffer pointers */

	pSeqDev->irpsInUse--;

	if (++pSeqDev->nextDoneIrp == IRP_COUNT)
	    pSeqDev->nextDoneIrp = 0;

	pSeqDev->audioBfrPending -= pIrp->transferLen;
	pSeqDev->audioBfrCount -= pIrp->transferLen;


	/* Examine the IRP result */

	if (pIrp->result != S_usbHcdLib_IRP_CANCELED)
	    updateTransmitter (pSeqDev);
	else
	    pSeqDev->streamFault = TRUE;
	}


    /* The completion of any IRP, successfully or otherwise, unblocks 
     * the foreground thread. 
     */

    if (pSeqDev->foregroundWaiting)
	{
	pSeqDev->foregroundWaiting = FALSE;
	OSS_SEM_GIVE (pSeqDev->bfrNotFullSem);
	}


    /* The following call to destroyAudioStream() doesn't do anything
     * unless the audio play is complete or there has been an error.
     */

    if(pSeqDev->stopFlag == FALSE)
    destroyAudioStream (pSeqDev, FALSE);


    OSS_MUTEX_RELEASE (speakerMutex);
    }


/***************************************************************************
*
* usbSpkrSeqRd - SEQ_DEV sequential read routine
*
* This function reads the SEQ_DEV seqential read routine. 
* 
* RETURNS: not supported, always returns ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrSeqRd
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int numBytes,	    /* number of bytes to read */
    char *buffer,	    /* pointer to buffer to receive data */
    BOOL fixed		    /* TRUE => fixed block size */
    )

    {
    return ERROR;
    }


/***************************************************************************
*
* usbSpkrSeqWt - SEQ_DEV sequential write routine
*
* This writes into the audio buffer
* 
* RETURNS: OK if transfer successful, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrSeqWrt
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int numBytes,	    /* number of bytes or blocks to be written */
    char *buffer,	    /* ptr to input data buffer */
    BOOL fixed		    /* TRUE => fixed block size */
    )

    {
    UINT32 extLen;


    /* If the audio stream isn't open, we don't accept input */

    if (!pSeqDev->open)
    return EIO;


    /* Copy audio data to our internal buffer. */

    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

    while (numBytes > 0 && !pSeqDev->streamFault)
	{
	/* If our internal buffer is full, wait for space to become avail. */

	if (pSeqDev->audioBfrCount == pSeqDev->audioBfrLen)
	    {
	    pSeqDev->foregroundWaiting = TRUE;

	    OSS_MUTEX_RELEASE (speakerMutex);
	    OSS_SEM_TAKE (pSeqDev->bfrNotFullSem, OSS_BLOCK);
	    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

	    if (pSeqDev->streamFault)
	    break;
	    }

	
	/* Calculate length of next contiguous extent in buffer */

	if (pSeqDev->audioBfrIn < pSeqDev->audioBfrOut)
	    extLen = pSeqDev->audioBfrOut - pSeqDev->audioBfrIn;
	else
	    extLen = pSeqDev->audioBfrLen - pSeqDev->audioBfrIn;

	extLen = min (extLen, pSeqDev->audioBfrLen - pSeqDev->audioBfrCount);


	/* Copy data to buffer */

	extLen = min (extLen, (unsigned)numBytes);

	memcpy (&pSeqDev->pAudioBfr [pSeqDev->audioBfrIn], buffer, extLen);

	buffer += extLen;
	numBytes -= extLen;

	pSeqDev->audioBfrCount += extLen;
	pSeqDev->audioBfrTotal += extLen;

	if ((pSeqDev->audioBfrIn += extLen) == pSeqDev->audioBfrLen)
	    pSeqDev->audioBfrIn = 0;


	/* Update transmitter. */

	if (updateTransmitter (pSeqDev) != OK)
        {
        OSS_MUTEX_RELEASE (speakerMutex);
	    return EIO;
        }
	}


    OSS_MUTEX_RELEASE (speakerMutex);


    if (numBytes == 0)
	return OK;
    else
	return EIO;
    }


/***************************************************************************
*
* usbSpkrIoctl - SEQ_DEV IOCTL routine
*
* This function handles the various ioctl functions
*
* RETURNS: OK if IOCTL handled successfully, ENOSYS if unsupported
* request, or EIO for other errors.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrIoctl
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int funcCode,	    /* ioctl() function code */
    int arg		/* function-specific argument */
    )

    {
    STATUS status = OK;


    /* Handle IOCTL */

    switch (funcCode)
    {
    case USB_SPKR_IOCTL_GET_FORMAT_COUNT:

	/* Return could of supported audio formats */

	*((pUINT16) arg) = pSeqDev->fmtCount;
	break;


    case USB_SPKR_IOCTL_GET_FORMAT_LIST:

	/* Return pointer to array of audio formats */

	*((pUSB_SPKR_AUDIO_FORMAT *) arg) = pSeqDev->pFmt;
	break;


    case USB_SPKR_IOCTL_GET_CHANNEL_COUNT:

	/* Return count of audio channels + 1 for master channel */

	*((pUINT16) arg) = pSeqDev->channels;
	break;


    case USB_SPKR_IOCTL_GET_CHANNEL_CONFIG:

	/* Return configuration of channels */

	*((pUINT16) arg) = pSeqDev->channelConfig;
	break;


    case USB_SPKR_IOCTL_GET_CHANNEL_CAPS:

	/* Return pointer to capabilities array for all channels */

	*((pUSB_SPKR_CHANNEL_CAPS *) arg) = pSeqDev->pCaps;
	break;


    case USB_SPKR_IOCTL_SET_AUDIO_FORMAT:

	/* Find a format matching the one indicated by the caller */

	if (selectFormat (pSeqDev, (pUSB_SPKR_AUDIO_FORMAT) arg) != OK)
	    status = ENOSYS;
	break;


    case USB_SPKR_IOCTL_OPEN_AUDIO_STREAM:

	/* Begin an audio stream. */

	status = openAudioStream (pSeqDev);

	break;


    case USB_SPKR_IOCTL_CLOSE_AUDIO_STREAM:

	/* mark the end of an audio stream */


   	if (arg)
	   pSeqDev->stopFlag = TRUE;

	status = closeAudioStream (pSeqDev);
	break;


    case USB_SPKR_IOCTL_AUDIO_STREAM_STATUS:

	/* report whether the audio device is still busy. */

	*((pUINT16) arg) = 0;

	if (pSeqDev->open)
	    *((pUINT16) arg) |= USB_SPKR_STATUS_OPEN;

	if (pSeqDev->audioBfrCount)
	    *((pUINT16) arg) |= USB_SPKR_STATUS_DATA_IN_BFR;

	break;


    case USB_SPKR_IOCTL_SET_MUTE:
    case USB_SPKR_IOCTL_SET_VOLUME:
    case USB_SPKR_IOCTL_SET_BASS:
    case USB_SPKR_IOCTL_SET_MID:
    case USB_SPKR_IOCTL_SET_TREBLE:

	/* Set indicated channel control.  MSW(arg) = channel,
	 * LSW(arg) = value.
	 */

	status = setChannelControl (pSeqDev, 
				    funcCode, 
				    MSW (arg), 
				    LSW (arg));
	break;


    default:
	status = ENOSYS;
	break;
    }

    return status;
    }	


/***************************************************************************
*
* usbSpkrSeqWrtFileMarks - SEQ_DEV sequential write file marks routine
*
* SEQ_DEV sequential write file marks routine
*
* RETURNS: not implemented.  Always returns OK for compatibility.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrSeqWrtFileMarks
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int numMarks,	    /* number of file marks to write */
    BOOL shortMark	    /* short or long file marks */
    )

    {
    return OK;
    }


/***************************************************************************
*
* usbSpkrRewind - SEQ_DEV rewind routine
*
* This function is not implemented
*
* RETURNS: not implemented.  Always returns OK for compatibility.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrRewind
    (
    pUSB_SPKR_SEQ_DEV pSeqDev	    /* pointer to device descriptor */
    )

    {
    return OK;
    }


/***************************************************************************
*
* usbSpkrReserve - SEQ_DEV reserve routine
*
* This function makes SEQ_DEV descriptor reserved
*
* RETURNS: OK if device can be reserved, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrReserve
    (
    pUSB_SPKR_SEQ_DEV pSeqDev	    /* pointer to device descriptor */
    )

    {
    STATUS status;

    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

    if (!pSeqDev->reserved)
	{
	pSeqDev->reserved = TRUE;
	status = OK;
	}
    else
	{
	status = ERROR;
	}

    OSS_MUTEX_RELEASE (speakerMutex);

    return status;
    }


/***************************************************************************
*
* usbSpkrRelease - SEQ_DEV release routine
*
* This function releases the SEQ_DEV descriptor structure
*
* RETURNS: OK if device can be released, else ERROR.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrRelease
    (
    pUSB_SPKR_SEQ_DEV pSeqDev	    /* pointer to device descriptor */
    )

    {
    STATUS status;

    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

    if (pSeqDev->reserved)
	{
	pSeqDev->reserved = FALSE;
	status = OK;
	}
    else
	{
	status = ERROR;
	}

    OSS_MUTEX_RELEASE (speakerMutex);

    return status;
    }


/***************************************************************************
*
* usbSpkrReadBlkLim - SEQ_DEV read block limit routine
*
* This function blocks the read block 
*
* RETURNS: always returns OK.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrReadBlkLim
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int *maxBlkLimit,		/* maximum block size for device */
    int *minBlkLimit		/* minimum block size for device */
    )

    {
    if (maxBlkLimit != NULL)
	*maxBlkLimit = A_REALLY_BIG_INTEGER;

    if (minBlkLimit != 0)
	*minBlkLimit = 0;

    return OK;
    }


/***************************************************************************
*
* usbSpkrLoad - SEQ_DEV load routine
*
* This function is not implemented
*
* RETURNS: not implemented, always returns OK for compatibility.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrLoad
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    BOOL load		    /* load or unload device */
    )

    {
    return OK;
    }


/***************************************************************************
*
* usbSpkrSpace - SEQ_DEV space routine
*
* This function is not implemented
*
* RETURNS: not implemented, always returns OK for compatibility.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrSpace
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,	    /* pointer to device descriptor */
    int count,		    /* number of spaces */
    int spaceCode	    /* type of space */
    )

    {
    return OK;
    }


/***************************************************************************
*
* usbSpkrErase - SEQ_DEV erase routine
*
* This function is not implemented
*
* RETURNS: not implemented, always returns OK for compatibility.
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL STATUS usbSpkrErase
    (
    pUSB_SPKR_SEQ_DEV pSeqDev	    /* pointer to device descriptor */
    )

    {
    return OK;
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
* destroySeqDev - disposes of a USB_SPKR_SEQ_DEV structure
*
* Unlinks the indicated USB_SPKR_SEQ_DEV structure and de-allocates
* resources associated with the device.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID destroySeqDev
    (
    pUSB_SPKR_SEQ_DEV pSeqDev
    )

    {
    /* Unlink the structure. */

    usbListUnlink (&pSeqDev->devLink);

    /* Release audio stream-related info */

    destroyAudioStream (pSeqDev, TRUE);

    /* Release per-channel data */

    if (pSeqDev->pCaps != NULL)
	OSS_FREE (pSeqDev->pCaps);

    /* Release audio format data */

    if (pSeqDev->pFmt != NULL)
	OSS_FREE (pSeqDev->pFmt);

    /* Release structure. */

    OSS_FREE (pSeqDev);
    }


/***************************************************************************
*
* createSeqDev - creates a new USB_SPKR_SEQ_DEV structure
*
* Creates a new USB_SPKR_SEQ_DEV structure for the indicated <nodeId>.
* If successful, the new structure is linked into the devList upon 
* return.
*
* <configuration> and <interface> identify the configuration/interface
* that first reported itself as an audio device.
*
* RETURNS: pointer to newly created structure, or NULL if failure
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_SPKR_SEQ_DEV createSeqDev
    (
    USBD_NODE_ID nodeId,
    UINT16 configuration,
    UINT16 interface
    )

    {
    pUSB_SPKR_SEQ_DEV pSeqDev;


    /* Try to allocate space for a new speaker struct */

    if ((pSeqDev = OSS_CALLOC (sizeof (*pSeqDev))) == NULL)
	return NULL;

    pSeqDev->seqDev.sd_seqRd		= usbSpkrSeqRd;
    pSeqDev->seqDev.sd_seqWrt		= usbSpkrSeqWrt;
    pSeqDev->seqDev.sd_ioctl		= usbSpkrIoctl;
    pSeqDev->seqDev.sd_seqWrtFileMarks	= usbSpkrSeqWrtFileMarks;
    pSeqDev->seqDev.sd_rewind		= usbSpkrRewind;
    pSeqDev->seqDev.sd_reserve		= usbSpkrReserve;
    pSeqDev->seqDev.sd_release		= usbSpkrRelease;
    pSeqDev->seqDev.sd_readBlkLim	= usbSpkrReadBlkLim;
    pSeqDev->seqDev.sd_load		= usbSpkrLoad;
    pSeqDev->seqDev.sd_space		= usbSpkrSpace;
    pSeqDev->seqDev.sd_erase		= usbSpkrErase;
    pSeqDev->seqDev.sd_reset		= NULL;
    pSeqDev->seqDev.sd_statusChk	= NULL;

    pSeqDev->seqDev.sd_blkSize	    = 0;
    pSeqDev->seqDev.sd_mode	= O_WRONLY;
    pSeqDev->seqDev.sd_readyChanged = TRUE;
    pSeqDev->seqDev.sd_maxVarBlockLimit = A_REALLY_BIG_INTEGER;
    pSeqDev->seqDev.sd_density	    = 0;

    pSeqDev->nodeId = nodeId;
    pSeqDev->configuration = configuration;

    pSeqDev->connected = TRUE;


    /* Try to configure the speaker. */

    if (configureSeqDev (pSeqDev) != OK)
	{
	destroySeqDev (pSeqDev);
	return NULL;
	}


    /* Link the newly created structure. */

    usbListLink (&devList, pSeqDev, &pSeqDev->devLink, LINK_TAIL);

    return pSeqDev;
    }


/***************************************************************************
*
* findSeqDev - Searches for a USB_SPKR_SEQ_DEV for indicated node ID
*
* This fucntion Searches for a USB_SPKR_SEQ_DEV for indicated <nodeId>
* 
* RETURNS: pointer to matching USB_SPKR_SEQ_DEV or NULL if not found
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL pUSB_SPKR_SEQ_DEV findSeqDev
    (
    USBD_NODE_ID nodeId
    )

    {
    pUSB_SPKR_SEQ_DEV pSeqDev = usbListFirst (&devList);

    while (pSeqDev != NULL)
	{
	if (pSeqDev->nodeId == nodeId)
	    break;

	pSeqDev = usbListNext (&pSeqDev->devLink);
	}

    return pSeqDev;
    }


/***************************************************************************
*
* notifyAttach - Notifies registered callers of attachment/removal
*
* This function notifies the registered client of the device attachment and
* its removal
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    pUSB_SPKR_SEQ_DEV pSeqDev,
    UINT16 attachCode
    )

    {
    pATTACH_REQUEST pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
	{
	(*pRequest->callback) (pRequest->callbackArg, 
			       (SEQ_DEV *) pSeqDev, 
			       attachCode);

	pRequest = usbListNext (&pRequest->reqLink);
	}
    }


/***************************************************************************
*
* usbSpeakerAttachCallback - called by USBD when speaker attached/removed
*
* The USBD will invoke this callback when a USB audio device is attached to or
* removed from the system.  <nodeId> is the USBD_NODE_ID of the node being
* attached or removed.	<attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* Audio devices report their class information at the interface level, so 
* <configuration> and <interface> will indicate the configuratin/interface that 
* reports itself as an audio device.  Finally, <deviceClass>, <deviceSubClass>,
* and <deviceProtocol> will identify the type of audio device (eg. a speaker).
*
* NOTE: The USBD will invoke this function once for each configuration/
* interface which reports itself as an audio device.  So, it is possible that
* a single device insertion/removal may trigger multiple callbacks.  We
* ignore all callbacks except the first for a given device.  We also ignore
* audio devices that are not speakers (eg., we ignore microphones and other
* input audio devices).
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID usbSpeakerAttachCallback
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
    pUSB_SPKR_SEQ_DEV pSeqDev;


    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

    /* Depending on the attach code, add a new speaker or disabled one
     * that's already been created.
     */

    switch (attachAction)
	{
	case USBD_DYNA_ATTACH:

	    /* A new device is being attached.  Check if we already 
	     * have a structure for this device.
	     */

	    if (findSeqDev (nodeId) != NULL)
		break;

	    /* Create a new structure to manage this device.  If there's
	     * an error, there's nothing we can do about it, so skip the
	     * device and return immediately. 
	     */

	    if ((pSeqDev = createSeqDev (nodeId, 
					 configuration, 
					 interface)) 
					== NULL)
		break;

	    /* Notify registered callers that a new speaker has been
	     * added and a new channel created.
	     */

	    notifyAttach (pSeqDev, USB_SPKR_ATTACH);

	    break;


	case USBD_DYNA_REMOVE:

	    /* A device is being detached.	Check if we have any
	     * structures to manage this device.
	     */

	    if ((pSeqDev = findSeqDev (nodeId)) == NULL)
		break;

	    /* The device has been disconnected. */

	    pSeqDev->connected = FALSE;

	    /* Notify registered callers that the speaker has been
	     * removed and the channel disabled. 
	     *
	     * NOTE: We temporarily increment the channel's lock count
	     * to prevent usbSpeakerSeqDevUnlock() from destroying the
	     * structure while we're still using it.
	     */

	    pSeqDev->lockCount++;

        /* Release the mutex so that the application
         * can access the speakerLib functions on notification
         */  
        OSS_MUTEX_RELEASE (speakerMutex);

	    notifyAttach (pSeqDev, USB_SPKR_REMOVE);

        /* Take the mutex again */
        OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

	    pSeqDev->lockCount--;

	    /* If no callers have the channel structure locked, destroy
	     * it now.  If it is locked, it will be destroyed later during
	     * a call to usbSpeakerUnlock().
	     */

	    if (pSeqDev->lockCount == 0)
		destroySeqDev (pSeqDev);

	    break;
	}

    OSS_MUTEX_RELEASE (speakerMutex);
    }


/***************************************************************************
*
* doShutdown - shuts down USB speaker SIO driver
*
* <errCode> should be OK or S_usbSpeakerLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: appropaite error code
*
* \NOMANUAL
*/

LOCAL STATUS doShutdown
    (
    int errCode
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_SPKR_SEQ_DEV pSeqDev;


    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
	destroyAttachRequest (pRequest);


    /* Dispose of any open speaker connections. */

    while ((pSeqDev = usbListFirst (&devList)) != NULL)
	destroySeqDev (pSeqDev);
    

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

    if (speakerMutex != NULL)
	{
	OSS_MUTEX_DESTROY (speakerMutex);
	speakerMutex = NULL;
	}


    return ossStatus (errCode);
    }


/***************************************************************************
*
* usbSpeakerDevInit - initialize USB speaker SIO driver
*
* Initializes the USB speaker SIO driver.  The USB speaker SIO driver
* maintains an initialization count, so calls to this function may be
* nested.
*
* RETURNS: OK, or ERROR if unable to initialize.
*
* ERRNO:
* \is
* \i S_usbSpeakerLib_OUT_OF_RESOURCES
* Sufficient resources not available
*
* \i S_usbSpeakerLib_USBD_FAULT
* Fault in the USBD Layer
* \ie
*/

STATUS usbSpeakerDevInit (void)
    {
    /* If not already initialized, then initialize internal structures
     * and connection to USBD.
     */

    if (initCount == 0)
    {
    /* Initialize lists, structures, resources. */

    memset (&devList, 0, sizeof (devList));
    memset (&reqList, 0, sizeof (reqList));
    speakerMutex = NULL;
    usbdHandle = NULL;


    if (OSS_MUTEX_CREATE (&speakerMutex) != OK)
	return doShutdown (S_usbSpeakerLib_OUT_OF_RESOURCES);


    /* Establish connection to USBD */

    if (usbdClientRegister (SPKR_CLIENT_NAME, &usbdHandle) != OK ||
	usbdDynamicAttachRegister (usbdHandle, 
				   USB_CLASS_AUDIO,
				   USBD_NOTIFY_ALL, 
				   USBD_NOTIFY_ALL,
				   usbSpeakerAttachCallback) 
			   != OK)
	{
	return doShutdown (S_usbSpeakerLib_USBD_FAULT);
	}
    }

    initCount++;

    return OK;
    }


/***************************************************************************
*
* usbSpeakerDevShutdown - shuts down speaker SIO driver
*
* This function shuts down speaker SIO driver depending on <initCount>. Every
* call to this function decrements the <initCount>, and when it turns 0,
* SIO speaker driver is shutdown.
*
* RETURNS: OK, or ERROR if unable to shutdown.
*
* ERRNO:
* \is
* \i S_usbSpeakerLib_NOT_INITIALIZED
* Speaker SIO Driver is not initialized
* \ie
*/

STATUS usbSpeakerDevShutdown (void)
    {
    /* Shut down the USB speaker SIO driver if the initCount goes to 0. */

    if (initCount == 0)
	return ossStatus (S_usbSpeakerLib_NOT_INITIALIZED);

    if (--initCount == 0)
	return doShutdown (OK);

    return OK;
    }


/***************************************************************************
*
* usbSpeakerDynamicAttachRegister - Register speaker attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_SPKR_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     SEQ_DEV *pSeqDev,
*     UINT16 attachCode
*     );
* \ce
*
* usbSpeakerLib will invoke <callback> each time a USB speaker
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also be passed a pointer to the 
* SEQ_DEV structure for the channel being created/destroyed and
* an attach code of USB_SPKR_ATTACH or USB_SPKR_REMOVE.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbSpeakerLib_BAD_PARAM
* Bad Parameter is recieved
*
* \i S_usbSpeakerLib_OUT_OF_MEMORY
* Sufficient memory is not available
* \ie
*/

STATUS usbSpeakerDynamicAttachRegister
    (
    USB_SPKR_ATTACH_CALLBACK callback,	/* new callback to be registered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_SPKR_SEQ_DEV pSeqDev;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
	return ossStatus (S_usbSpeakerLib_BAD_PARAM);


    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);


    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
	status = S_usbSpeakerLib_OUT_OF_MEMORY;
    else
	{
	pRequest->callback = callback;
	pRequest->callbackArg = arg;

	usbListLink (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL);

	
	/* Perform an initial notification of all currrently attached
	 * speaker devices.
	 */

	pSeqDev = usbListFirst (&devList);

	while (pSeqDev != NULL)
	    {
	    if (pSeqDev->connected)
		(*callback) (arg, (SEQ_DEV *) pSeqDev, USB_SPKR_ATTACH);

	    pSeqDev = usbListNext (&pSeqDev->devLink);
	    }
	}

    OSS_MUTEX_RELEASE (speakerMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbSpeakerDynamicAttachUnregister - Unregisters speaker attach callback
*
* This function cancels a previous request to be dynamically notified for
* speaker attachment and removal.  The <callback> and <arg> paramters must
* exactly match those passed in a previous call to 
* usbSpeakerDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister callback
*
* ERRNO:
* \is
* \i S_usbSpeakerLib_NOT_REGISTERED
* Could not regsiter the attachment callback function 
* \ie
*/

STATUS usbSpeakerDynamicAttachUnRegister
    (
    USB_SPKR_ATTACH_CALLBACK callback,	/* callback to be unregistered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    int status = S_usbSpeakerLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

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

    OSS_MUTEX_RELEASE (speakerMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbSpeakerSeqDevLock - Marks SEQ_DEV structure as in use
*
* A caller uses usbSpeakerSeqDevLock() to notify usbSpeakerLib that
* it is using the indicated SEQ_DEV structure.	usbSpeakerLib maintains
* a count of callers using a particular SEQ_DEV structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* USB speaker is removed from the system.  So long as the "lock count"
* is greater than zero, usbSpeakerLib will not dispose of an SEQ_DEV
* structure.
*
* RETURNS: OK, or ERROR if unable to mark SEQ_DEV structure in use.
*
* ERRNO: none
*/

STATUS usbSpeakerSeqDevLock
    (
    SEQ_DEV *pChan	/* SEQ_DEV to be marked as in use */
    )

    {
    pUSB_SPKR_SEQ_DEV pSeqDev = (pUSB_SPKR_SEQ_DEV) pChan;
    pSeqDev->lockCount++;

    return OK;
    }


/***************************************************************************
*
* usbSpeakerSeqDevUnlock - Marks SEQ_DEV structure as unused
*
* This function releases a lock placed on an SEQ_DEV structure.  When a
* caller no longer needs an SEQ_DEV structure for which it has previously
* called usbSpeakerSeqDevLock(), then it should call this function to
* release the lock.
*
* NOTE: If the underlying USB speaker device has already been removed
* from the system, then this function will automatically dispose of the
* SEQ_DEV structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the SEQ_DEV again structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark SEQ_DEV structure unused
*
* ERRNO:
* \is 
* \i S_usbSpeakerLib_NOT_LOCKED
* No lock to unlock
* \ie
*/

STATUS usbSpeakerSeqDevUnlock
    (
    SEQ_DEV *pChan	/* SEQ_DEV to be marked as unused */
    )

    {
    pUSB_SPKR_SEQ_DEV pSeqDev = (pUSB_SPKR_SEQ_DEV) pChan;
    int status = OK;


    OSS_MUTEX_TAKE (speakerMutex, OSS_BLOCK);

    if (pSeqDev->lockCount == 0)
	status = S_usbSpeakerLib_NOT_LOCKED;
    else
	{
	/* If this is the last lock and the underlying USB speaker is
	 * no longer connected, then dispose of the speaker.
	 */

	if (--pSeqDev->lockCount == 0 && !pSeqDev->connected)
	    destroySeqDev (pSeqDev);
	}

    OSS_MUTEX_RELEASE (speakerMutex);

    return ossStatus (status);
    }


/* end of file */







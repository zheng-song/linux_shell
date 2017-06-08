/* usbKeyboardLib.c - USB keyboard class drive with vxWorks SIO interface */

/* Copyright 2000-2002 Wind River Systems, Inc. */

/*
modification history
--------------------
01i,15oct04,ami  Apigen Changes
01h,18dec03,cfc  Fix compiler warning
01g,08may02,wef  Add ioctl to set keyboard mode to return raw scan codes - 
		 SPR #70988. 
01f,29oct01,wef  Remove automatic buffer creations and repalce with OSS_MALLOC.
01e,18sep01,wef  merge from wrs.tor2_0.usb1_1-f for veloce
01d,03jul01,jsv	 Re-wrote key tracking logic to fix issue with extra chars
                 when shift is released before the desired shifted key.
                 Made typematic setting globals so they can be modified.
                 Fixed typematic task, not calling callback after queueing
                 chars.
                 Added support of extended keycodes.
                 Fixed shifted key operation when CAPSLOCK is on.
01c,22jan01,wef  fix tab stops.
01b,20mar00,rcb  Re-write code to fetch maxPacketSize from endpoint descriptor
	 	 on machines which don't support non-aligned word access.
	 	 Allocate "report" structure separately from SIO_CHAN in order
 		 to avoid cache problems on MIPS platform.
01a,27jul99,rcb  written.
*/

/*
DESCRIPTION

This module implements the USB keyboard class driver for the vxWorks operating
system.  This module presents an interface which is a superset of the vxWorks
SIO (serial IO) driver model.  That is, this driver presents the external APIs
which would be expected of a standard "multi-mode serial (SIO) driver" and
adds certain extensions which are needed to address adequately the requirements
of the hot-plugging USB environment.

USB keyboards are described as part of the USB "human interface device" class
specification and related documents.  This driver concerns itself only with USB
devices which claim to be keyboards as set forth in the USB HID specification
and ignores other types of human interface devices (i.e., mouse).  USB
keyboards can operate according to either a "boot protocol" or to a "report
protocol".  This driver enables keyboards for operation using the boot
protocol.

As the SIO driver model presents a fairly limited, byte-stream oriented view of
a serial device, this driver maps USB keyboard scan codes into appropriate
ASCII codes.  Scan codes and combinations of scan codes which do not map to the
ASCII character set are suppressed.

Unlike most SIO drivers, the number of channels supported by this driver is not
fixed.	Rather, USB keyboards may be added or removed from the system at any
time.  This creates a situation in which the number of channels is dynamic, and
clients of usbKeyboardLib.c need to be made aware of the appearance and 
disappearance of channels.  Therefore, this driver adds an additional set of
functions which allows clients to register for notification upon the insertion
and removal of USB keyboards, and hence the creation and deletion of channels.

This module itself is a client of the Universal Serial Bus Driver (USBD).  All
interaction with the USB buses and devices is handled through the USBD.


INITIALIZATION

As with standard SIO drivers, this driver must be initialized by calling
usbKeyboardDevInit().  usbKeyboardDevInit() in turn initializes its 
connection to the USBD and other internal resources needed for operation.  
Unlike some SIO drivers, there are no usbKeyboardLib.c data structures which need 
to be initialized prior to calling usbKeyboardDevInit().

Prior to calling usbKeyboardDevInit(), the caller must ensure that the USBD
has been properly initialized by calling - at a minimum - usbdInitialize().
It is also the caller's responsibility to ensure that at least one USB HCD
(USB Host Controller Driver) is attached to the USBD - using the USBD function
usbdHcdAttach() - before keyboard operation can begin.	However, it is not 
necessary for usbdHcdAttach() to be alled prior to initializating usbKeyboardLib.c.
usbKeyboardLib.c uses the USBD dynamic attach services and is capable of 
recognizing USB keboard attachment and removal on the fly.  Therefore, it is 
possible for USB HCDs to be attached to or detached from the USBD at run time
- as may be required, for example, in systems supporting hot swapping of
hardware.

usbKeyboardLib.c does not export entry points for transmit, receive, and error
interrupt entry points like traditional SIO drivers.  All "interrupt" driven
behavior is managed by the underlying USBD and USB HCD(s), so there is no
need for a caller (or BSP) to connect interrupts on behalf of usbKeyboardLib.c.
For the same reason, there is no post-interrupt-connect initialization code
and usbKeboardLib.c therefore also omits the "devInit2" entry point.


OTHER FUNCTIONS

usbKeyboardLib.c also supports the SIO ioctl interface.  However, attempts to
set parameters like baud rates and start/stop bits have no meaning in the USB
environment and will be treated as no-ops.  


DATA FLOW

For each USB keyboard connected to the system, usbKeyboardLib.c sets up a
USB pipe to monitor input from the keyboard.  Input, in the form of scan codes,
is translated to ASCII codes and placed in an input queue.  If SIO callbacks
have been installed and usbKeyboardLib.c has been placed in the SIO "interrupt" 
mode of operation, then usbKeyboardLib.c will invoke the "character received"
callback for each character in the queue.  When usbKeyboardLib.c has been placed
in the "polled" mode of operation, callbacks will not be invoked and the 
caller will be responsible for fetching keyboard input using the driver's
pollInput() function.

usbKeyboardLib.c does not support output to the keyboard.  Therefore, calls to
the txStartup() and pollOutput() functions will fail.  The only "output" 
supported is the control of the keyboard LEDs, and this is handled internally
by usbKeyboardLib.c.

The caller needs to be aware that usbKeyboardLib.c is not capable of operating
in a true "polled mode" as the underlying USBD and USB HCD always operate in
an interrupt mode.  


TYPEMATIC REPEAT

USB keyboards do not implement typematic repeat, and it is the responsibility
of the host software to implement this feature.  For this purpose, this module
creates a task called typematicThread() which monitors all open channels and
injects repeated characters into input queues as appropriate.


INCLUDE FILES: sioLib.h, usbKeyboardLib.h
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
#include "drv/usb/usbKeyboardLib.h" /* our API */


/* defines */

#define KBD_CLIENT_NAME "usbKeyboardLib"    /* our USBD client name */

#define KBD_Q_DEPTH 8	    /* Max characters in keyboard queue */

#define ISEXTENDEDKEYCODE(keyCode)          (keyCode & 0xFF00)
#define ISALPHASCANCODE(scanCode)           ( (scanCode >= 0x04) && (scanCode <= 0x1D) )
#define ISNUMERICSCANCODE(scanCode)         ( (scanCode >= 0x1E) && (scanCode <= 0x27) )
#define ISKEYPADSCANCODE(scanCode)          ( (scanCode >= 0x53) && (scanCode <= 0x63) )
#define ISKEYPADEXTSCANCODE(scanCode)       ( (scanCode >= 0x59) && (scanCode <= 0x63) )
#define ISFUNCTIONSCANCODE(scanCode)        ( (scanCode >= 0x3A) && (scanCode <= 0x45) )
#define ISOTHEREXTENDEDSCANCODE(scanCode)   ( (scanCode >= 0x49) && (scanCode <= 0x52) )
#define ISEXTENDEDSCANCODE(scanCode, modifiers)                                 \
                ( (modifiers & MOD_KEY_ALT) && ISALPHASCANCODE(scanCode) )      \
                || ISFUNCTIONSCANCODE(scanCode)                                 \
                || ISOTHEREXTENDEDSCANCODE(scanCode)

/* If your hardware platform has problems sharing cache lines, then define
 * CACHE_LINE_SIZE below so that critical buffers can be allocated within
 * their own cache lines.
 */

#define CACHE_LINE_SIZE     16


/* typematic definitions */

#define TYPEMATIC_NAME	    "tUsbKbd"


/* typedefs */

/*
 * ATTACH_REQUEST
 */

typedef struct attach_request
    {
    LINK reqLink;	    /* linked list of requests */
    USB_KBD_ATTACH_CALLBACK callback;	/* client callback routine */
    pVOID callbackArg;		/* client callback argument */
    } ATTACH_REQUEST, *pATTACH_REQUEST;


/* USB_KBD_SIO_CHAN is the internal data structure we use to track each USB
 * keyboard.
 */

typedef struct usb_kbd_sio_chan
    {
    SIO_CHAN sioChan;		/* must be first field */

    LINK sioLink;	        /* linked list of keyboard structs */

    UINT16 lockCount;		/* Count of times structure locked */

    USBD_NODE_ID nodeId;	/* keyboard node Id */
    UINT16 configuration;	/* configuration/interface reported as */
    UINT16 interface;		/* a keyboard by this device */

    BOOL connected;	        /* TRUE if keyboard currently connected */

    USBD_PIPE_HANDLE pipeHandle;/* USBD pipe handle for interrupt pipe */
    USB_IRP irp;	        /* IRP to monitor interrupt pipe */
    BOOL irpInUse;	        /* TRUE while IRP is outstanding */
    pHID_KBD_BOOT_REPORT pBootReport;/* Keyboard boot report fetched thru pipe */

    char inQueue [KBD_Q_DEPTH]; /* Circular queue for keyboard input */
    UINT16 inQueueCount;	/* count of characters in input queue */
    UINT16 inQueueIn;		/* next location in queue */
    UINT16 inQueueOut;		/* next character to fetch */

    UINT32 typematicTime;	/* time current typematic period started */
    UINT32 typematicCount;	/* count of keys injected */
    UINT16 typematicChar; 	/* current character to repeat */

    int mode;		        /* SIO_MODE_INT or SIO_MODE_POLL */

    STATUS (*getTxCharCallback) (); /* tx callback */
    void *getTxCharArg; 	/* tx callback argument */

    STATUS (*putRxCharCallback) (); /* rx callback */
    void *putRxCharArg; 	/* rx callback argument */

    /* Following variables used to emulate certain ioctl functions. */

    int baudRate;	        /* has no meaning in USB */

    /* Following variables maintain keyboard state */

    BOOL capsLock;	        /* TRUE if CAPLOCK in effect */
    BOOL scrLock;	        /* TRUE if SCRLOCK in effect */
    BOOL numLock;	        /* TRUE if NUMLOCK in effect */

    UINT16 activeScanCodes [BOOT_RPT_KEYCOUNT];

    int scanMode;		/* raw or ascii */

    } USB_KBD_SIO_CHAN, *pUSB_KBD_SIO_CHAN;


/* forward static declarations */

LOCAL int usbKeyboardTxStartup (SIO_CHAN * pSioChan);
LOCAL int usbKeyboardCallbackInstall (SIO_CHAN *pSioChan, int callbackType,
    STATUS (*callback)(void *, ...), void *callbackArg);
LOCAL int usbKeyboardPollOutput (SIO_CHAN *pSioChan, char   outChar);
LOCAL int usbKeyboardPollInput (SIO_CHAN *pSioChan, char *thisChar);
LOCAL int usbKeyboardIoctl (SIO_CHAN *pSioChan, int request, void *arg);

LOCAL VOID usbKeyboardIrpCallback (pVOID p);


/* locals */

LOCAL UINT16 initCount = 0;	/* Count of init nesting */

LOCAL MUTEX_HANDLE kbdMutex;    /* mutex used to protect internal structs */

LOCAL LIST_HEAD sioList;	/* linked list of USB_KBD_SIO_CHAN */
LOCAL LIST_HEAD reqList;	/* Attach callback request list */

LOCAL USBD_CLIENT_HANDLE usbdHandle; /* our USBD client handle */

LOCAL THREAD_HANDLE typematicHandle;/* task used to generate typematic repeat */
LOCAL BOOL killTypematic;	/* TRUE when typematic thread should exit */
LOCAL BOOL typematicExit;	/* TRUE when typematic thread exits */


/* Channel function table. */

LOCAL SIO_DRV_FUNCS usbKeyboardSioDrvFuncs =
    {
    usbKeyboardIoctl,
    usbKeyboardTxStartup,
    usbKeyboardCallbackInstall,
    usbKeyboardPollInput,
    usbKeyboardPollOutput
    };


/* ASCI definitions */

#define SHIFT_CASE_OFFSET   ('a' - 'A')
#define CTRL_CASE_OFFSET    64		/* diff between 'A' and CTRL-A */

#define BS	    8		/* backspace, CTRL-H */
#define DEL	    BS		/* delete key */
#define TAB	    9		/* tab, CTRL-I */
#define CR	    13		/* carriage return, CTRL-M */
#define ESC	    27		/* escape */


/* Scan code lookup table and related constants. */

#define ASCII_MIN	        0x0000
#define ASCII_MAX	        0x007f

#define CAPLOCK 	        0x39
#define CAPLOCK_LOCKING     	0x82
#define NUMLOCK 	        0x53
#define NUMLOCK_LOCKING 	0x83
#define SCRLOCK 	        0x47
#define SCRLOCK_LOCKING 	0x84

#define NOTKEY		        0

#define isAscii(a)  ((a) <= ASCII_MAX)


/* NOTE: The following scan code tables are populated only so far as is useful
 * in order to map the ASCI character set.  Also, the following two tables must
 * remain exactly the same length.
 */

LOCAL UINT16 scanCodes [] =
    {
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, 'a',    'b',    'c',    'd',    /* 0 - 7 */
    'e',    'f',    'g',    'h',    'i',    'j',    'k',    'l',    /* 8 - 15 */
    'm',    'n',    'o',    'p',    'q',    'r',    's',    't',    /* 16 - 23 */
    'u',    'v',    'w',    'x',    'y',    'z',    '1',    '2',    /* 24 - 31 */
    '3',    '4',    '5',    '6',    '7',    '8',    '9',    '0',    /* 32 - 39 */
    CR,     ESC,    DEL,    TAB,    ' ',    '-',    '=',    '[',    /* 40 - 47 */
    ']',    '\\',   '#',    ';',    '\'',   '`',    ',',    '.',    /* 48 - 55 */
    '/',    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 56 - 63 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 64 - 71 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, DEL,    NOTKEY, NOTKEY, NOTKEY, /* 72 - 79 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, '/',    '*',    '-',    '+',    /* 80 - 87 */
    CR,     '1',    '2',    '3',    '4',    '5',    '6',    '7',    /* 88 - 95 */
    '8',    '9',    '0',    '.',    '|',    NOTKEY, NOTKEY, '=',    /* 96 - 103 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 104 - 111 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 112 - 119 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 120 - 127 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, ',',    '=', 	NOTKEY, /* 128 - 135 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 136 - 143 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 144 - 151 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 152 - 159 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 160 - 167 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 168 - 175 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 176 - 183 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 184 - 191 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 192 - 199 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 200 - 207 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 208 - 215 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 216 - 223 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 224 - 231 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 232 - 239 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 240 - 247 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY  /* 248 - 255 */
    };


LOCAL UINT16 scanCodesShift [] =
    {
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, 'A',    'B',    'C',    'D',    /* 0 - 7 */
    'E',    'F',    'G',    'H',    'I',    'J',    'K',    'L',    /* 8 - 15 */
    'M',    'N',    'O',    'P',    'Q',    'R',    'S',    'T',    /* 16 - 23 */
    'U',    'V',    'W',    'X',    'Y',    'Z',    '!',    '@',    /* 24 - 31 */
    '#',    '$',    '%',    '^',    '&',    '*',    '(',    ')',    /* 32 - 39 */
    CR,     ESC,    DEL,    TAB,    ' ',    '_',    '+',    '{',    /* 40 - 47 */
    '}',    '|',    '~',    ':',    '"',    '~',    '<',    '>',    /* 48 - 55 */
    '?',    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 56 - 63 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 64 - 71 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, DEL,    NOTKEY, NOTKEY, NOTKEY, /* 72 - 79 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, '/',    '*',    '-',    '+',    /* 80 - 87 */
    CR,     '1',    '2',    '3',    '4',    '5',    '6',    '7',    /* 88 - 95 */
    '8',    '9',    '0',    '.',    '|',    NOTKEY, NOTKEY, '=',    /* 96 - 103 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 104 - 111 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 112 - 119 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 120 - 127 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, ',',    '=', 	NOTKEY, /* 128 - 135 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 136 - 143 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 144 - 151 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 152 - 159 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 160 - 167 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 168 - 175 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 176 - 183 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 184 - 191 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 192 - 199 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 200 - 207 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 208 - 215 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 216 - 223 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 224 - 231 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 232 - 239 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, /* 240 - 247 */
    NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY, NOTKEY  /* 248 - 255 */
    };


#define SCAN_CODE_TBL_LEN   (sizeof (scanCodes) / sizeof (UINT16))

/*  IBM Extended keycodes.
    Char.     Hex Pair       Char.    Hex Pair
    ALT-A     (0x00,0x1e)    ALT-B    (0x00,0x30)
    ALT-C     (0x00,0x2e)    ALT-D    (0x00,0x20)
    ALT-E     (0x00,0x12)    ALT-F    (0x00,0x21)
    ALT-G     (0x00,0x22)    ALT-H    (0x00,0x23)
    ALT-I     (0x00,0x17)    ALT-J    (0x00,0x24)
    ALT-K     (0x00,0x25)    ALT-L    (0x00,0x26)
    ALT-M     (0x00,0x32)    ALT-N    (0x00,0x31)
    ALT-O     (0x00,0x18)    ALT-P    (0x00,0x19)
    ALT-Q     (0x00,0x10)    ALT-R    (0x00,0x13)
    ALT-S     (0x00,0x1a)    ALT-T    (0x00,0x14)
    ALT-U     (0x00,0x16)    ALT-V    (0x00,0x2f)
    ALT-W     (0x00,0x11)    ALT-X    (0x00,0x2d)
    ALT-Y     (0x00,0x15)    ALT-Z    (0x00,0x2c)
    PgUp      (0x00,0x49)    PgDn     (0x00,0x51)
    Home      (0x00,0x47)    End      (0x00,0x4f)
    UpArrw    (0x00,0x48)    DnArrw   (0x00,0x50)
    LftArrw   (0x00,0x4b)    RtArrw   (0x00,0x4d)
    F1        (0x00,0x3b)    F2       (0x00,0x3c)
    F3        (0x00,0x3d)    F4       (0x00,0x3e)
    F5        (0x00,0x3f)    F6       (0x00,0x40)
    F7        (0x00,0x41)    F8       (0x00,0x42)
    F9        (0x00,0x43)    F10      (0x00,0x44)
    F11       (0x00,0x85)    F12      (0x00,0x86)
    ALT-F1    (0x00,0x68)    ALT-F2   (0x00,0x69)
    ALT-F3    (0x00,0x6a)    ALT-F4   (0x00,0x6b)
    ALT-F5    (0x00,0x6c)    ALT-F6   (0x00,0x6d)
    ALT-F7    (0x00,0x6e)    ALT-F8   (0x00,0x6f)
    ALT-F9    (0x00,0x70)    ALT-F10  (0x00,0x71)
    ALT-F11   (0x00,0x8b)    ALT-F12  (0x00,0x8c)
*/

LOCAL UINT16 extendedAlphaKeyCodes [] =
    {
        0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23,
        0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19,
        0x10, 0x13, 0x1a, 0x14, 0x16, 0x2f, 0x11, 0x2d,
        0x15, 0x2c
    };

LOCAL UINT16 extendedOtherKeyCodes [] =
    {
        0x52, 0x47, 0x49, 0x53, 0x4f, 0x51, 0x4d ,0x4b, 0x50,
        0x48,
    };

LOCAL UINT16 extendedFunctionKeyCodes [] =
    {
        0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42,
        0x43, 0x44, 0x85, 0x86
    };

LOCAL UINT16 extendedAltFunctionKeyCodes [] =
    {
        0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x8b, 0x8c
    };

LOCAL UINT16 extendedKeypadKeyCodes [] =
    {
        0x4F, 0x50, 0x51, 0x4B, 0x00, 0x4D, 0x47, 0x48,
        0x49, 0x52, 0x53
    };

/* globals */

unsigned int TYPEMATIC_DELAY  = 500;    /* 500 msec delay */
unsigned int TYPEMATIC_PERIOD = 66;     /* 66 msec = approx 15 char/sec */


/***************************************************************************
*
* cvtScanCodeToKeyCode - converts scan code to key code if possible
*
* <scanCode> is the scan code to be interpreted and <modifiers> is the 
* current state of the keyboard modifies (e.g., SHIFT).  
*
* RETURNS: ASCII code if mapping exists, CAPLOCK, SCRLOCK, NUMLOCK, or
*          NOTKEY if no mapping.
*
* ERRNO: None
*
*\NOMANUAL
*/

LOCAL UINT16 cvtScanCodeToKeyCode
    (
    pUSB_KBD_SIO_CHAN pSioChan,
    UINT16 scanCode,
    UINT16 modifiers
    )

    {

    /* Translate keypad keys. */

    if( ISKEYPADSCANCODE(scanCode) )
	{

        if( !!(modifiers & MOD_KEY_SHIFT) ^ !!(pSioChan->numLock) )
            return( scanCodes [scanCode] );

        if( ISKEYPADEXTSCANCODE(scanCode) )
	    {

            /* If the table contains 0 (like entry for keypad '5') 
	     * return NOTKEY. 
	     */

            if(extendedKeypadKeyCodes[scanCode-0x59] == 0) 
		return(NOTKEY);

            return(0xFF00 | extendedKeypadKeyCodes[scanCode-0x59]);
	    }
        return( scanCodes [scanCode] );
	}

    /* Translate extended keys. */

    if( ISALPHASCANCODE(scanCode) && (modifiers & MOD_KEY_ALT) )
        return(0xFF00 | extendedAlphaKeyCodes[scanCode-0x04]);

    if( ISOTHEREXTENDEDSCANCODE(scanCode) )
        return(0xFF00 | extendedOtherKeyCodes[scanCode-0x49]);

    if( ISFUNCTIONSCANCODE(scanCode) && (modifiers & MOD_KEY_ALT) )
        return(0xFF00 | extendedAltFunctionKeyCodes[scanCode-0x3A]);

    if( ISFUNCTIONSCANCODE(scanCode))
        return(0xFF00 | extendedFunctionKeyCodes[scanCode-0x3A]);

    /* Translate the scan code into a preliminary ASCII code */

    if (scanCode < SCAN_CODE_TBL_LEN)
	{
        /* Translate alpha keys */

        if( ISALPHASCANCODE(scanCode) )
	    {
            if( modifiers & MOD_KEY_CTRL )
                return( scanCodesShift [scanCode] - CTRL_CASE_OFFSET);

            if( !!(modifiers & MOD_KEY_SHIFT) ^ !!(pSioChan->capsLock) )
                return( scanCodesShift [scanCode] );

            else
                return( scanCodes [scanCode] );
	    }

        /* Translate non-alpha keys */

        if ((modifiers & (MOD_KEY_SHIFT | MOD_KEY_CTRL)) != 0)
        	return( scanCodesShift [scanCode] );

        else
	        return( scanCodes [scanCode] );
	}

    return(NOTKEY);
    }


/***************************************************************************
*
* isKeyPresent - determines if a key is present in an array of keys
*
* This function determines whether the <key> is present in the array <pKeyArray>
*
* RETURNS: TRUE if <key> is present in the <keyArray>, else returns FALSE
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL BOOL isKeyPresent
    (
    pUINT16 pKeyArray,
    UINT16 key
    )

    {
    UINT16 i;

    for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
	if (key == pKeyArray [i])
	    return TRUE;

    return FALSE;
    }


/***************************************************************************
*
* setLedReport - Issues a SET_REPORT to change a keyboard's LEDs
*
* This function isses a <SET_REPORT> request to change keyboards LED
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID setLedReport
    (
    pUSB_KBD_SIO_CHAN pSioChan,
    UINT8 ledReport
    )

    {

    UINT8 * pLedReport = OSS_CALLOC (sizeof (UINT8));

    if (pLedReport == NULL)
	return;

    *pLedReport = ledReport;

    usbHidReportSet (usbdHandle, 
		     pSioChan->nodeId, 
		     pSioChan->interface, 
		     USB_HID_RPT_TYPE_OUTPUT,      
		     0, 
		     pLedReport, 
		     sizeof (ledReport));

    OSS_FREE (pLedReport);
    }


/***************************************************************************
*
* changeKeyState - changes keyboard state
*
* <key> is CAPLOCK, SCRLOCK, or NUMLOCK.  If <key> is not already 
* active, then, toggle the current keyboard state for the corresponding item.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID changeKbdState
    (
    pUSB_KBD_SIO_CHAN pSioChan,
    UINT16 scanCode, 			/* not used */
    pBOOL pKeyState
    )

    {
    UINT8 ledReport;

    /* The scancode is newly active, toggle the corresponding keyboard state. */

    *pKeyState = !(*pKeyState);


    /* Update the keyboard LEDs */

    ledReport = (pSioChan->capsLock) ? RPT_LED_CAPS_LOCK : 0;
    ledReport |= (pSioChan->scrLock) ? RPT_LED_SCROLL_LOCK : 0;
    ledReport |= (pSioChan->numLock) ? RPT_LED_NUM_LOCK : 0;

    setLedReport (pSioChan, ledReport);

    }

/***************************************************************************
*
* interpScanCode - interprets keyboard scan code
*
* Interprets the <scanCode> according to the <modifiers>.  This function
* handles any special requirements, such as turning an LED ON or OFF in
* response to a keypress.
*
* RETURNS: N/A.
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL void interpScanCode
    (
    pUSB_KBD_SIO_CHAN pSioChan,
    UINT16 scanCode,
    UINT16 modifiers
    )

    {

    /* If the key is already active, ignore it. */

    if (isKeyPresent (pSioChan->activeScanCodes, scanCode))
        return;

    /* Determine if special handling is required for the key */

    switch (scanCode)
	{
        case CAPLOCK:   /* key is CAPLOCK */
        case CAPLOCK_LOCKING:   /* key is CAPLOCK */

	        changeKbdState (pSioChan, scanCode, &pSioChan->capsLock);
	        break;

        case SCRLOCK:   /* key is SCRLOCK */
        case SCRLOCK_LOCKING:   /* key is SCRLOCK */

	        changeKbdState (pSioChan, scanCode, &pSioChan->scrLock);
	        break;

        case NUMLOCK:   /* key is NUMLOCK */
        case NUMLOCK_LOCKING:   /* key is NUMLOCK */

	        changeKbdState (pSioChan, scanCode, &pSioChan->numLock);
	        break;

        case NOTKEY:    /* no valid scan code mapping */
        default:	/* an ASCII character */

	    break;
	}
    }


/***************************************************************************
*
* putInChar - puts a character into channel's input queue
*
* This function puts character <putChar> in the queue
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID putInChar
    (
    pUSB_KBD_SIO_CHAN pSioChan,
    char putChar
    )

    {
    if (pSioChan->inQueueCount < KBD_Q_DEPTH)
	{
	pSioChan->inQueue [pSioChan->inQueueIn] = putChar;

	if (++pSioChan->inQueueIn == KBD_Q_DEPTH)
	    pSioChan->inQueueIn = 0;

	pSioChan->inQueueCount++;
	}
    }


/***************************************************************************
*
* nextInChar - returns next character from input queue
*
* Returns the next character from the channel's input queue and updates
* the queue pointers.  The caller must ensure that at least one character
* is in the queue prior to calling this function.
*
* RETURNS: next char in queue
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL char nextInChar
    (
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    char inChar = pSioChan->inQueue [pSioChan->inQueueOut];

    if (++pSioChan->inQueueOut == KBD_Q_DEPTH)
	pSioChan->inQueueOut = 0;

    pSioChan->inQueueCount--;

    return inChar;
    }


/***************************************************************************
*
* updateTypematic - generates typematic characters for channel if appropriate
*
* This function generates typematic characters for channel if appropriate
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID updateTypematic
    (
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    UINT32 diffTime;
    UINT32 repeatCount;


    /* If the given channel is active and a typematic character is
     * indicated, then update the typematic state.
     */


    if (pSioChan->connected && pSioChan->typematicChar != 0)
	{

        diffTime = OSS_TIME () - pSioChan->typematicTime;

        /* If the typematic delay has passed, then it is time to start
         * injecting characters into the queue.
         */

        if (diffTime >= TYPEMATIC_DELAY)
	    {
	    diffTime -= TYPEMATIC_DELAY;
	    repeatCount = diffTime / TYPEMATIC_PERIOD + 1;

	    /* Inject characters into the queue.  If the queue is
	     * full, putInChar() dumps the character, but we increment
	     * the typematicCount anyway.  This keeps the queue from
	     * getting too far ahead of the user. 
	     */

	    while (repeatCount > pSioChan->typematicCount)
		{
		if( ISEXTENDEDKEYCODE(pSioChan->typematicChar) )
		    {
		    if(pSioChan->inQueueCount < KBD_Q_DEPTH-1)
			{
			putInChar (pSioChan, (char) 0);
			putInChar (pSioChan, 
				   (char) pSioChan->typematicChar & 0xFF);
			}
		    }
		else
		    {
		    putInChar (pSioChan, pSioChan->typematicChar);
		    }
		pSioChan->typematicCount++;
		}

	    /* invoke receive callback */

	    while (pSioChan->inQueueCount > 0 &&
		    pSioChan->putRxCharCallback != NULL &&
		    pSioChan->mode == SIO_MODE_INT)
		{
                (*pSioChan->putRxCharCallback) (pSioChan->putRxCharArg, 
						nextInChar (pSioChan));
		}
	    }
	}
    }


/***************************************************************************
*
* interpKbdReport - interprets USB keyboard BOOT report
*
* Interprets a keyboard boot report and updates channel state as
* appropriate.  Operates in one of two modes: ASCII or RAW.  In ASCII mode
* it inserts the ascii character into the character buffer and implements
* typematic repeat.  In RAW mode it always inserts the modifier byte
* regardless of change, it inserts any keypresses that are currently active
* and it inserts a terminating byte of 0xff into the charater buffer.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID interpKbdReport
    (
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    pHID_KBD_BOOT_REPORT pReport = pSioChan->pBootReport;
    UINT16 keyCode;
    UINT16 newTypematicChar;
    UINT16 activeCount;
    UINT16 i;

    /* 
     * interpret each key position in a keyboard boot report 
     * (handles CAPS/SCROLL/NUM lock). 
     */

    for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
        interpScanCode (pSioChan, pReport->scanCodes [i], pReport->modifiers);

    /* Raw mode has been set, handle accordingly */

    if (pSioChan->scanMode == SIO_KYBD_MODE_RAW)
	{
	/* The first byte is any modifier keys, CTRL, SHIFT, ALT, GUI */

	putInChar (pSioChan, pReport->modifiers);
	for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
	    if (pReport->scanCodes [i])
		    /* Any depressed keys */

		    putInChar (pSioChan, pReport->scanCodes [i]);

	/* trailing byte */

	putInChar (pSioChan, 0xff);
    	}

    /* then pSioChan->scanMode must = SIO_KYBD_MODE_ASCII */

    else 
	{
	/* insert newly activated keys into the input queue for the keyboard */

	newTypematicChar = 0;
	activeCount = 0;

	for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
	    {
	    if (pReport->scanCodes [i])
		{
		keyCode = cvtScanCodeToKeyCode (pSioChan, 
						pReport->scanCodes [i], 
						pReport->modifiers);

		if (!isKeyPresent (pSioChan->activeScanCodes, 
				   pReport->scanCodes [i]))
		    {
		    /* If there is room in the input queue, enqueue the key,
		     * else discard it.
		     * For extended keyCodes, make sure there is room for two 
		     * chars - the 0 and the ext key. 
		     */
		    if( ISEXTENDEDKEYCODE(keyCode) )
			{
			if(pSioChan->inQueueCount < KBD_Q_DEPTH-1)
			    {
			    putInChar (pSioChan, (char) 0);
			    putInChar (pSioChan, (char) keyCode & 0xFF);
			    }
			}
		    else
			{
			if(keyCode)
			    putInChar (pSioChan, (char) keyCode & 0xFF);
			}
		    }
		newTypematicChar = keyCode;
		activeCount++;

		}
	    }

	    /* 
	     * If newTypematicChar is 0, then no keys were received in 
	     * this report - so no keys are being held down.  If 
	     * newTypematicChar matches the previous typematic char, 
	     * then allow the typematic timer to continue.  If 
	     * newTypematicChar is different (but non-zero), then start 
	     * a new timer.  In all cases, only one key may be active 
	     * for typematic repeat to be enabled. 
	     */

	    if (activeCount != 1)
		newTypematicChar = 0;

	    if (newTypematicChar != pSioChan->typematicChar)
		{
		pSioChan->typematicChar = newTypematicChar;

		if (newTypematicChar != 0)
		    {
		    pSioChan->typematicTime = OSS_TIME ();
		    pSioChan->typematicCount = 0;
		    }
		}
	    updateTypematic (pSioChan);

	}

    /* invoke receive callback */

    while (pSioChan->inQueueCount > 0 &&
           pSioChan->putRxCharCallback != NULL &&
           pSioChan->mode == SIO_MODE_INT)
	{
        (*pSioChan->putRxCharCallback) (pSioChan->putRxCharArg, 
					nextInChar (pSioChan));
	}

    /* 
     * Copy the current list of active keys to the channel 
     * structure, overwriting the previous list.
     */

    for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
	pSioChan->activeScanCodes [i] = pReport->scanCodes [i];

    }


/***************************************************************************
*
* usbKeyboardIoctl - special device control
*
* This routine is largely a no-op for the usbKeyboardLib.  The only ioctls
* which are used by this module are the SIO_AVAIL_MODES_GET and SIO_MODE_SET.
*
* RETURNS: OK on success, ENOSYS on unsupported request, EIO on failed request
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbKeyboardIoctl
    (
    SIO_CHAN *pChan,	    /* device to control */
    int request,	/* request code */
    void *someArg	/* some argument */
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = (pUSB_KBD_SIO_CHAN) pChan;
    int arg = (int) someArg;

    switch (request)
	{
	case SIO_BAUD_SET:

	    /* baud rate has no meaning for USB.  We store the desired 
	     * baud rate value and return OK.
	     */

	    pSioChan->baudRate = arg;
	    return OK;


	case SIO_BAUD_GET:

	    /* Return baud rate to caller */

	    *((int *) arg) = pSioChan->baudRate;
	    return OK;	


	case SIO_MODE_SET:

	    /* Set driver operating mode: interrupt or polled */

	    if (arg != SIO_MODE_POLL && arg != SIO_MODE_INT)
		return EIO;

	    pSioChan->mode = arg;
	    return OK;


	case SIO_MODE_GET:

	    /* Return current driver operating mode for channel */

	    *((int *) arg) = pSioChan->mode;
	    return OK;


	case SIO_AVAIL_MODES_GET:

	    /* Return modes supported by driver. */

	    *((int *) arg) = SIO_MODE_INT | SIO_MODE_POLL;
	    return OK;


	case SIO_OPEN:

	    /* Channel is always open. */

	    return OK;


         case SIO_KYBD_MODE_SET:
             switch (arg)
                 {
                 case SIO_KYBD_MODE_RAW:
                 case SIO_KYBD_MODE_ASCII:
                     break;

                 case SIO_KYBD_MODE_UNICODE:
                     return ENOSYS; /* usb doesn't support unicode */
                 }
             pSioChan->scanMode = arg;
             return OK;


         case SIO_KYBD_MODE_GET:
             *(int *)someArg = pSioChan->scanMode;
             return OK;


         case SIO_KYBD_LED_SET:
	    {
	    UINT8 ledReport;
 
	    /*  update the channel's information about the LED state */	

	    pSioChan->numLock = (arg & SIO_KYBD_LED_NUM) ? SIO_KYBD_LED_NUM : 0;

	    pSioChan->capsLock = (arg & SIO_KYBD_LED_CAP) ? 
					SIO_KYBD_LED_CAP : 0;
	    pSioChan->scrLock = (arg & SIO_KYBD_LED_SCR) ? 
					SIO_KYBD_LED_SCR : 0;

	    /* 
	     * We are relying on the SIO_KYBD_LED_X macros matching the USB
	     * LED equivelants.
	     */

	    ledReport = arg;

	    /* set the LED's */

	    setLedReport (pSioChan, ledReport);
	     
	    return OK;
	    }



         case SIO_KYBD_LED_GET:
	     {
	     int tempArg;	

    	     tempArg = (pSioChan->capsLock) ? SIO_KYBD_LED_CAP : 0;
    	     tempArg |= (pSioChan->scrLock) ? SIO_KYBD_LED_SCR : 0;
    	     tempArg |= (pSioChan->numLock) ? SIO_KYBD_LED_NUM : 0;

	     *(int *) someArg = tempArg;

             return OK;

	     }

	case SIO_HW_OPTS_SET:   /* optional, not supported */
	case SIO_HW_OPTS_GET:   /* optional, not supported */
	case SIO_HUP:	/* hang up is not supported */
	default:	    /* unknown/unsupported command. */

	    return ENOSYS;
	}
    }


/***************************************************************************
*
* usbKeyboardTxStartup - start the interrupt transmitter
*
* The USB keyboard SIO driver does not support output to the keyboard.
*
* RETURNS: EIO
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbKeyboardTxStartup
    (
    SIO_CHAN *pChan	/* channel to start */
    )

    {
    return EIO;
    }


/***************************************************************************
*
* usbKeyboardCallbackInstall - install ISR callbacks to get/put chars
*
* This driver allows interrupt callbacks for transmitting characters
* and receiving characters.
*
* RETURNS: OK on success, or ENOSYS for an unsupported callback type
*
* ERRNO: none
*
*\NOMANUAL
*/ 

LOCAL int usbKeyboardCallbackInstall
    (
    SIO_CHAN *pChan,	    /* channel */
    int callbackType,	    /* type of callback */
    STATUS (*callback) (void *tmp, ...),  /* callback */
    void *callbackArg	    /* parameter to callback */
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = (pUSB_KBD_SIO_CHAN) pChan;

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
* usbKeyboardPollOutput - output a character in polled mode
*
* The USB keyboard SIO driver does not support output to the keyboard.
*
* RETURNS: EIO
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL int usbKeyboardPollOutput
    (
    SIO_CHAN *pChan,
    char outChar
    )

    {
    return EIO;
    }


/***************************************************************************
*
* usbKeyboardPollInput - poll the device for input
*
* This function polls the keyboard device for input.
*
* RETURNS: OK if a character arrived, EIO on device error, EAGAIN
* if the input buffer if empty, ENOSYS if the device is interrupt-only.
*
* ERRNO: none
*
*\NOMANUAL 
*/

LOCAL int usbKeyboardPollInput
    (
    SIO_CHAN *pChan,
    char *thisChar
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = (pUSB_KBD_SIO_CHAN) pChan;
    int status = OK;


    /* validate parameters */

    if (thisChar == NULL)
        return EIO;


    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

    /* Check if the input queue is empty. */

    if (pSioChan->inQueueCount == 0)
        status = EAGAIN;
    else
	{
	/* Return a character from the input queue. */

	*thisChar = nextInChar (pSioChan);
	}

    OSS_MUTEX_RELEASE (kbdMutex);

    return status;
    }


/***************************************************************************
*
* initKbdIrp - Initialize IRP to listen for input on interrupt pipe
*
* This function intializes the IRP to listen on the interrupt pipe
*
* RETURNS: TRUE if able to submit IRP successfully, else FALSE
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL BOOL initKbdIrp
    (
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    pUSB_IRP pIrp = &pSioChan->irp;

    /* Initialize IRP */

    memset (pIrp, 0, sizeof (*pIrp));

    pIrp->userPtr = pSioChan;
    pIrp->irpLen = sizeof (*pIrp);
    pIrp->userCallback = usbKeyboardIrpCallback;
    pIrp->timeout = USB_TIMEOUT_NONE;
    pIrp->transferLen = sizeof (HID_KBD_BOOT_REPORT);

    pIrp->bfrCount = 1;
    pIrp->bfrList [0].pid = USB_PID_IN;
    pIrp->bfrList [0].pBfr = (pUINT8) pSioChan->pBootReport;
    pIrp->bfrList [0].bfrLen = sizeof (HID_KBD_BOOT_REPORT);


    /* Submit IRP */

    if (usbdTransfer (usbdHandle, pSioChan->pipeHandle, pIrp) != OK)
	return FALSE;

    pSioChan->irpInUse = TRUE;


    return TRUE;
    }


/***************************************************************************
*
* usbKeyboardIrpCallback - Invoked upon IRP completion/cancellation
*
* Examines the cause of the IRP completion.  If completion was successful,
* interprets the USB keyboard's boot report and re-submits the IRP.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbKeyboardIrpCallback
    (
    pVOID p	    /* completed IRP */
    )

    {
    pUSB_IRP pIrp = (pUSB_IRP) p;
    pUSB_KBD_SIO_CHAN pSioChan = pIrp->userPtr;


    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

    /* Was the IRP successful? */

    if (pIrp->result == OK)
	{
	/* Interpret the keyboard report */

	interpKbdReport (pSioChan);
	}


    /* Re-submit the IRP unless it was canceled - which would happen only
     * during pipe shutdown (e.g., the disappearance of the device).
     */

    pSioChan->irpInUse = FALSE;

    if (pIrp->result != S_usbHcdLib_IRP_CANCELED) 
	{
	initKbdIrp (pSioChan);
	}
    else 
        {
        if (!pSioChan->connected)
            {
            /* Release structure. */
            if (pSioChan->pBootReport != NULL)
                OSS_FREE (pSioChan->pBootReport);
	
	    OSS_FREE (pSioChan);
            }           
        }
    
    OSS_MUTEX_RELEASE (kbdMutex);
    }


/***************************************************************************
*
* typematicThread - Updates typematic state for each active channel
*
* Updates typematic state for each active channel
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID typematicThread
    (
    pVOID param 	/* param not used by this thread */
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan;


    while (!killTypematic)
	{
        OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

        /* Walk the list of open channels and update the typematic
         * state for each.
         */

        pSioChan = usbListFirst (&sioList);

        while (pSioChan != NULL)
	    {
	    updateTypematic (pSioChan);
	    pSioChan = usbListNext (&pSioChan->sioLink);
	    }

        OSS_MUTEX_RELEASE (kbdMutex);

        OSS_THREAD_SLEEP (TYPEMATIC_PERIOD);
	}

    typematicExit = TRUE;

    }


/***************************************************************************
*
* configureSioChan - configure USB keyboard for operation
*
* Selects the configuration/interface specified in the <pSioChan>
* structure.  These values come from the USBD dynamic attach callback,
* which in turn retrieved them from the configuration/interface
* descriptors which reported the device to be a keyboard.
*
* RETURNS: TRUE if successful, else FALSE if failed to configure channel
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL BOOL configureSioChan
    (
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    pUSB_CONFIG_DESCR pCfgDescr;
    pUSB_INTERFACE_DESCR pIfDescr;
    pUSB_ENDPOINT_DESCR pEpDescr;
    UINT8 * pBfr;
    UINT8 * pScratchBfr;
    UINT16 actLen;
    UINT16 ifNo;
    UINT16 maxPacketSize;

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


    /* Select the keyboard boot protocol. */

    if (usbHidProtocolSet (usbdHandle, 
			   pSioChan->nodeId,
			   pSioChan->interface, 
			   USB_HID_PROTOCOL_BOOT) 
			 != OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Set the keyboard idle time to infinite. */

    if (usbHidIdleSet (usbdHandle, 
		       pSioChan->nodeId,
		       pSioChan->interface, 
		       0 /* no report ID */, 
		       0 /* infinite */) 
		    != OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Turn off LEDs. */

    setLedReport (pSioChan, 0);


    /* Create a pipe to monitor input reports from the keyboard. */

    maxPacketSize = *((pUINT8) &pEpDescr->maxPacketSize) |
		    (*(((pUINT8) &pEpDescr->maxPacketSize) + 1) << 8);

    if (usbdPipeCreate (usbdHandle, 
			pSioChan->nodeId,     
			pEpDescr->endpointAddress, 
			pCfgDescr->configurationValue,    
			pSioChan->interface, 
			USB_XFRTYPE_INTERRUPT, 
			USB_DIR_IN,    
			maxPacketSize, 
			sizeof (HID_KBD_BOOT_REPORT), 
			pEpDescr->interval, 
			&pSioChan->pipeHandle) 
		     != OK)
        {
        OSS_FREE (pBfr);
        return FALSE;
        }



    /* Initiate IRP to listen for input on interrupt pipe */

    if (!initKbdIrp (pSioChan))
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
* This function disposes of an ATTACH_REQUEST structure.
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

    usbListUnlinkProt (&pRequest->reqLink,kbdMutex);

    /* Dispose of structure */

    OSS_FREE (pRequest);
    }


/***************************************************************************
*
* destroySioChan - disposes of a USB_KBD_SIO_CHAN structure
*
* Unlinks the indicated USB_KBD_SIO_CHAN structure and de-allocates
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
    pUSB_KBD_SIO_CHAN pSioChan
    )

    {
    /* Unlink the structure. */

    usbListUnlinkProt (&pSioChan->sioLink,kbdMutex);


    /* Release pipe if one has been allocated.	Wait for the IRP to be
     * cancelled if necessary.
     */

    if (pSioChan->pipeHandle != NULL)
	usbdPipeDestroy (usbdHandle, pSioChan->pipeHandle);

    /* The following block is commented out to address the nonblocking
     * issue of destroySioChan when the keyboard is removed and a read is
     * in progress SPR #98731 */

    /*
    OSS_MUTEX_RELEASE (kbdMutex);

    while (pSioChan->irpInUse)
    OSS_THREAD_SLEEP (1);
    
    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);
    */

    /* Release structure. */
    if (!pSioChan->irpInUse)
         {
         if (pSioChan->pBootReport != NULL)
	    OSS_FREE (pSioChan->pBootReport);

	    OSS_FREE (pSioChan);
         }

    }


/***************************************************************************
*
* createSioChan - creates a new USB_KBD_SIO_CHAN structure
*
* Creates a new USB_KBD_SIO_CHAN structure for the indicated <nodeId>.
* If successful, the new structure is linked into the sioList upon 
* return.
*
* <configuration> and <interface> identify the configuration/interface
* that first reported itself as a keyboard for this device.
*
* RETURNS: pointer to newly created structure, or NULL if failure
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_KBD_SIO_CHAN createSioChan
    (
    USBD_NODE_ID nodeId,
    UINT16 configuration,
    UINT16 interface
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan;
    UINT16 i;


    /* Try to allocate space for a new keyboard struct */

    if ((pSioChan = OSS_CALLOC (sizeof (*pSioChan))) == NULL)
    	return NULL;

    if ((pSioChan->pBootReport = OSS_CALLOC (sizeof (*pSioChan->pBootReport)))
				      == NULL)
	{
	OSS_FREE (pSioChan);
	return NULL;
	}

    pSioChan->sioChan.pDrvFuncs = &usbKeyboardSioDrvFuncs;
    pSioChan->nodeId = nodeId;
    pSioChan->connected = TRUE;
    pSioChan->mode = SIO_MODE_POLL;
    pSioChan->scanMode = SIO_KYBD_MODE_ASCII;

    pSioChan->configuration = configuration;
    pSioChan->interface = interface;

    for (i = 0; i < BOOT_RPT_KEYCOUNT; i++)
	pSioChan->activeScanCodes [i] = NOTKEY;


    /* Try to configure the keyboard. */

    if (!configureSioChan (pSioChan))
	{
	destroySioChan (pSioChan);
	return NULL;
	}


    /* Link the newly created structure. */

    usbListLinkProt (&sioList, pSioChan, &pSioChan->sioLink, LINK_TAIL,kbdMutex);

    return pSioChan;
    }


/***************************************************************************
*
* findSioChan - Searches for a USB_KBD_SIO_CHAN for indicated node ID
*
* Searches for a USB_KBD_SIO_CHAN for indicated <nodeId>.
*
* RETURNS: pointer to matching USB_KBD_SIO_CHAN or NULL if not found
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL pUSB_KBD_SIO_CHAN findSioChan
    (
    USBD_NODE_ID nodeId
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = usbListFirst (&sioList);

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
* This function notifies of the device attachment or removal to the 
* registered clients
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID notifyAttach
    (
    pUSB_KBD_SIO_CHAN pSioChan,
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
* usbKeyboardAttachCallback - called by USBD when keyboard attached/removed
*
* The USBD will invoke this callback when a USB keyboard is attached to or
* removed from the system.  <nodeId> is the USBD_NODE_ID of the node being
* attached or removed.	<attachAction> is USBD_DYNA_ATTACH or USBD_DYNA_REMOVE.
* Keyboards generally report their class information at the interface level,
* so <configuration> and <interface> will indicate the configuratin/interface
* that reports itself as a keyboard.  Finally, <deviceClass>, <deviceSubClass>,
* and <deviceProtocol> will identify a HID/BOOT/KEYBOARD device.
*
* NOTE: The USBD will invoke this function once for each configuration/
* interface which reports itself as a keyboard.  So, it is possible that
* a single device insertion/removal may trigger multiple callbacks.  We
* ignore all callbacks except the first for a given device.
*
* RETURNS: N/A
*
* ERRNO: none
*
*\NOMANUAL
*/

LOCAL VOID usbKeyboardAttachCallback
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
    pUSB_KBD_SIO_CHAN pSioChan;


    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

    /* Depending on the attach code, add a new keyboard or disabled one
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

	    if ((pSioChan = createSioChan (nodeId, configuration, interface)) == NULL)
		break;

	    /* Notify registered callers that a new keyboard has been
	     * added and a new channel created.
	     */

	    notifyAttach (pSioChan, USB_KBD_ATTACH);

	    break;


	case USBD_DYNA_REMOVE:

	    /* A device is being detached.	Check if we have any
	     * structures to manage this device.
	     */

	    if ((pSioChan = findSioChan (nodeId)) == NULL)
		break;

	    /* The device has been disconnected. */

	    pSioChan->connected = FALSE;

	    /* Notify registered callers that the keyboard has been
	     * removed and the channel disabled. 
	     *
	     * NOTE: We temporarily increment the channel's lock count
	     * to prevent usbKeyboardSioChanUnlock() from destroying the
	     * structure while we're still using it.
	     */

	    pSioChan->lockCount++;

	    notifyAttach (pSioChan, USB_KBD_REMOVE);

	    pSioChan->lockCount--;

	    /* If no callers have the channel structure locked, destroy
	     * it now.  If it is locked, it will be destroyed later during
	     * a call to usbKeyboardUnlock().
	     */

	    if (pSioChan->lockCount == 0)
		destroySioChan (pSioChan);

	    break;
	}

    OSS_MUTEX_RELEASE (kbdMutex);
    }


/***************************************************************************
*
* doShutdown - shuts down USB keyboard SIO driver
*
* <errCode> should be OK or S_usbKeyboardLib_xxxx.  This value will be
* passed to ossStatus() and the return value from ossStatus() is the
* return value of this function.
*
* RETURNS: OK, or ERROR per value of <errCode> passed by caller
*
* ERRNO: appropiate error code
*
*\NOMANUAL
*/

LOCAL STATUS doShutdown
    (
    int errCode
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_KBD_SIO_CHAN pSioChan;


    /* Kill typematic thread */

    if (typematicThread != NULL)
	{
	killTypematic = TRUE;

	while (!typematicExit)
	    OSS_THREAD_SLEEP (1);

	OSS_THREAD_DESTROY (typematicHandle);
	}


    /* Dispose of any outstanding notification requests */

    while ((pRequest = usbListFirst (&reqList)) != NULL)
	destroyAttachRequest (pRequest);


    /* Dispose of any open keyboard connections. */

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

    if (kbdMutex != NULL)
	{
	OSS_MUTEX_DESTROY (kbdMutex);
	kbdMutex = NULL;
	}


    return ossStatus (errCode);
    }


/***************************************************************************
*
* usbKeyboardDevInit - initialize USB keyboard SIO driver
*
* Initializes the USB keyboard SIO driver. The USB keyboard SIO driver
* maintains an initialization count, so calls to this function may be
* nested.
*
* RETURNS: OK, or ERROR if unable to initialize.
*
* ERRNO:
* \is
* \i S_usbKeyboardLib_OUT_OF_RESOURCES
* Sufficient resources are not available to create mutex
*
* \i S_usbKeyboardLib_USBD_FAULT
* Fault in the USBD Layer
* \ie
*/

STATUS usbKeyboardDevInit (void)
    {
    /* If not already initialized, then initialize internal structures
     * and connection to USBD.
     */

    if (initCount == 0)
	{
	/* Initialize lists, structures, resources. */

	memset (&sioList, 0, sizeof (sioList));
	memset (&reqList, 0, sizeof (reqList));
	kbdMutex = NULL;
	usbdHandle = NULL;
	typematicHandle = NULL;
	killTypematic = FALSE;
	typematicExit = FALSE;


	if (OSS_MUTEX_CREATE (&kbdMutex) != OK)
	    return doShutdown (S_usbKeyboardLib_OUT_OF_RESOURCES);


	/* Initialize typematic repeat thread */

	if (OSS_THREAD_CREATE (typematicThread, 
			       NULL, 
			       OSS_PRIORITY_LOW,
			       TYPEMATIC_NAME, 
			       &typematicHandle) 
			     != OK)
	    return doShutdown (S_usbKeyboardLib_OUT_OF_RESOURCES);
	

	/* Establish connection to USBD */

	if (usbdClientRegister (KBD_CLIENT_NAME, &usbdHandle) != OK ||
	    usbdDynamicAttachRegister (usbdHandle, 
				       USB_CLASS_HID,
				       USB_SUBCLASS_HID_BOOT, 
				       USB_PROTOCOL_HID_BOOT_KEYBOARD,
				       usbKeyboardAttachCallback) 
				     != OK)
		{
		return doShutdown (S_usbKeyboardLib_USBD_FAULT);
		}
	}

    initCount++;

    return OK;
    }


/***************************************************************************
*
* usbKeyboardDevShutdown - shuts down keyboard SIO driver
*
* This function shuts down the keyboard driver. The driver is shutdown only 
* if <initCount> after decrementing. If it is more the 0, it is decremented.
*
* RETURNS: OK, or ERROR if unable to shutdown.
*
* ERRNO:
* \is
* \i S_usbKeyboardLib_NOT_INITIALIZED
* Keyboard Driver not initialized
* \ie
*/

STATUS usbKeyboardDevShutdown (void)
    {
    /* Shut down the USB keyboard SIO driver if the initCount goes to 0. */

    if (initCount == 0)
	return ossStatus (S_usbKeyboardLib_NOT_INITIALIZED);

    if (--initCount == 0)
	return doShutdown (OK);

    return OK;
    }


/***************************************************************************
*
* usbKeyboardDynamicAttachRegister - Register keyboard attach callback
*
* <callback> is a caller-supplied function of the form:
*
* \cs
* typedef (*USB_KBD_ATTACH_CALLBACK)
*     (
*     pVOID arg,
*     SIO_CHAN *pSioChan,
*     UINT16 attachCode
*     );
* \ce
*
* usbKeyboardLib will invoke <callback> each time a USB keyboard
* is attached to or removed from the system.  <arg> is a caller-defined
* parameter which will be passed to the <callback> each time it is
* invoked.  The <callback> will also be passed a pointer to the 
* SIO_CHAN structure for the channel being created/destroyed and
* an attach code of USB_KBD_ATTACH or USB_KBD_REMOVE.
*
* RETURNS: OK, or ERROR if unable to register callback
*
* ERRNO:
* \is
* \i S_usbKeyboardLib_BAD_PARAM
* Bad Parameter are passed
*
* \i S_usbKeyboardLib_OUT_OF_MEMORY
* Not sufficient memory is available
* \ie
*/

STATUS usbKeyboardDynamicAttachRegister
    (
    USB_KBD_ATTACH_CALLBACK callback,	/* new callback to be registered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_KBD_SIO_CHAN pSioChan;
    int status = OK;


    /* Validate parameters */

    if (callback == NULL)
	return ossStatus (S_usbKeyboardLib_BAD_PARAM);


    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);


    /* Create a new request structure to track this callback request. */

    if ((pRequest = OSS_CALLOC (sizeof (*pRequest))) == NULL)
	status = S_usbKeyboardLib_OUT_OF_MEMORY;
    else
	{
	pRequest->callback = callback;
	pRequest->callbackArg = arg;

	usbListLinkProt (&reqList, pRequest, &pRequest->reqLink, LINK_TAIL,kbdMutex);
	
	/* Perform an initial notification of all currrently attached
	 * keyboard devices.
	 */

	pSioChan = usbListFirst (&sioList);

	while (pSioChan != NULL)
	    {
	    if (pSioChan->connected)
		(*callback) (arg, (SIO_CHAN *) pSioChan, USB_KBD_ATTACH);

	    pSioChan = usbListNext (&pSioChan->sioLink);
	    }
	}

    OSS_MUTEX_RELEASE (kbdMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbKeyboardDynamicAttachUnregister - Unregisters keyboard attach callback
*
* This function cancels a previous request to be dynamically notified for
* keyboard attachment and removal.  The <callback> and <arg> paramters must
* exactly match those passed in a previous call to 
* usbKeyboardDynamicAttachRegister().
*
* RETURNS: OK, or ERROR if unable to unregister callback
*
* ERRNO:
* \is
* \i S_usbKeyboardLib_NOT_REGISTERED
* Could not register the callback
* \ie
*/

STATUS usbKeyboardDynamicAttachUnRegister
    (
    USB_KBD_ATTACH_CALLBACK callback,	/* callback to be unregistered */
    pVOID arg		    /* user-defined arg to callback */
    )

    {
    pATTACH_REQUEST pRequest;
    pUSB_KBD_SIO_CHAN pSioChan = NULL;

    int status = S_usbKeyboardLib_NOT_REGISTERED;

    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

    pRequest = usbListFirst (&reqList);

    while (pRequest != NULL)
	{
	if (callback == pRequest->callback && arg == pRequest->callbackArg)
	    {
    	/* Perform a notification of all currrently attached
	     * keyboard devices.
	     */
	    pSioChan = usbListFirst (&sioList);

    	while (pSioChan != NULL)
	        {
	        if (pSioChan->connected)
		       (*callback) (arg, (SIO_CHAN *) pSioChan, USB_KBD_REMOVE);

  	        pSioChan = usbListNext (&pSioChan->sioLink);
	        }

	    /* We found a matching notification request. */

	    destroyAttachRequest (pRequest);
	    status = OK;
	    break;
	    }

	pRequest = usbListNext (&pRequest->reqLink);
	}

    OSS_MUTEX_RELEASE (kbdMutex);

    return ossStatus (status);
    }


/***************************************************************************
*
* usbKeyboardSioChanLock - Marks SIO_CHAN structure as in use
*
* A caller uses usbKeyboardSioChanLock() to notify usbKeyboardLib that
* it is using the indicated SIO_CHAN structure.  usbKeyboardLib maintains
* a count of callers using a particular SIO_CHAN structure so that it 
* knows when it is safe to dispose of a structure when the underlying
* USB keyboard is removed from the system.  So long as the "lock count"
* is greater than zero, usbKeyboardLib will not dispose of an SIO_CHAN
* structure.
*
* RETURNS: OK
*
* ERRNO: none.
*/

STATUS usbKeyboardSioChanLock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as in use */
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = (pUSB_KBD_SIO_CHAN) pChan;
    pSioChan->lockCount++;

    return OK;
    }


/***************************************************************************
*
* usbKeyboardSioChanUnlock - Marks SIO_CHAN structure as unused
*
* This function releases a lock placed on an SIO_CHAN structure.  When a
* caller no longer needs an SIO_CHAN structure for which it has previously
* called usbKeyboardSioChanLock(), then it should call this function to
* release the lock.
*
* NOTE: If the underlying USB keyboard device has already been removed
* from the system, then this function will automatically dispose of the
* SIO_CHAN structure if this call removes the last lock on the structure.
* Therefore, a caller must not reference the SIO_CHAN again structure after
* making this call.
*
* RETURNS: OK, or ERROR if unable to mark SIO_CHAN structure unused
*
* ERRNO:
* \is
* \i S_usbKeyboardLib_NOT_LOCKED
* No lock to unlock
* \ie
*/

STATUS usbKeyboardSioChanUnlock
    (
    SIO_CHAN *pChan	/* SIO_CHAN to be marked as unused */
    )

    {
    pUSB_KBD_SIO_CHAN pSioChan = (pUSB_KBD_SIO_CHAN) pChan;
    int status = OK;


    OSS_MUTEX_TAKE (kbdMutex, OSS_BLOCK);

    if (pSioChan->lockCount == 0)
	status = S_usbKeyboardLib_NOT_LOCKED;
    else
	{
	/* If this is the last lock and the underlying USB keyboard is
	 * no longer connected, then dispose of the keyboard.
	 */

	if (--pSioChan->lockCount == 0 && !pSioChan->connected)
	    destroySioChan (pSioChan);
	}

    OSS_MUTEX_RELEASE (kbdMutex);

    return ossStatus (status);
    }

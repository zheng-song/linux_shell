/* usbTcdIsp1582Util.c - This module contains ISP 1582 utility functions.*/

/*
Modification history
--------------------
01b,15jun04,hch  Merge after ISP1582 driver testing done on MIPS, SH.
01b,19jul04,ami  Coding Convention Changes
01a,21apr04,ami implementing the utility functions of ISP 1582

*/

/*
DESCRIPTION

Defines all ISP 1582 utility functions. These utility functions will be used by
the ISP 1582 functions to carry out 8-bits, 16-bits & 32-bits read/write access
into the registers.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, drv/usb/target/usbIsp1582.h,
               drv/usb/target/usbIsp1582Eval.h
*/

/* includes */

#include "usb/usbPlatform.h"		       
#include "usb/ossLib.h" 		           
#include "drv/usb/target/usbIsp1582.h"	   
#include "drv/usb/target/usbIsp1582Eval.h"  



/******************************************************************************
* isp1582Read8 - reads a 8 bit register
*
* This is a utility function used to read a 8 bit register. <regAddr> consists
* of the address of the register from where 8 bit read is to be carried out.
*
* RETURNS: 8 bit value
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL UINT8 isp1582Read8
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr                      /* Address of register */
    )

    {
    OUT_ISP1582_CMD (pTarget , regAddr);
    return  (IN_ISP1582_DATA (pTarget) & 0xFFFF);
    }

/******************************************************************************
* isp1582Write8 - writes a 8 bit register
*
* This is a utility function used to writes to a 8 bit register. <regAddr>
* consists of the address of the register to where 8 bit is to be written.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID isp1582Write8
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr,                     /* Address of register */
    UINT8 data                         /* Data to be written */
    )

    {
    OUT_ISP1582_CMD (pTarget , regAddr);
    OUT_ISP1582_DATA (pTarget , data);
    return;
    }

/******************************************************************************
* isp1582Read16 - reads a 16 bit register
*
* This is a utility function used to read a 16 bit register. <regAddr> consists
* of the address of the register from where 16 bit read is to be carried out.
*
* RETURNS: 16 bit value
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL UINT16 isp1582Read16
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr                      /* Address of register */
    )

    {
    OUT_ISP1582_CMD (pTarget , regAddr);
    return  IN_ISP1582_DATA (pTarget);
    }

/******************************************************************************
* isp1582Write16 - writes a 16 bit register
*
* This is a utility function used to write into a 16 bit register. <regAddr>
* consists of the address of the register to where 16 bit is to be written.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID isp1582Write16
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr,                     /* Address of register */
    UINT16 data                        /* Data to be written */
    )

    {
    OUT_ISP1582_CMD (pTarget , regAddr);
    OUT_ISP1582_DATA (pTarget , data);
    return;
    }


/******************************************************************************
* isp1582Read32 - reads a 32 bit register
*
* This is a utility function used to read a 32 bit register. <regAddr> consists
* of the address of the register from where 32 bit read is to be carried out.
*
* RETURNS: 32 bit value
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL UINT32 isp1582Read32
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr                      /* Address of register */
    )

    {
    UINT16 value = 0;
    UINT16 value1 = 0;
    UINT32 data = 0;
    
    OUT_ISP1582_CMD (pTarget , regAddr);

    /* ISP 1582 uses 16 bit bus access */

    value = IN_ISP1582_DATA (pTarget);

    OUT_ISP1582_CMD (pTarget , regAddr + 2);

    value1 = IN_ISP1582_DATA (pTarget);
    data = (value | (value1 << 0x10));
    return  data;
    }

/******************************************************************************
* isp1582Write32 - writes a 32 bit register
*
* This is a utility function used to write into a 32 bit register. <regAddr>
* consists of the address of the register to where 32 bit is to be written.
*
* RETURNS: N/A
*
* ERRNO: none
*
* \NOMANUAL
*/

LOCAL VOID isp1582Write32
    (
    pUSB_TCD_ISP1582_TARGET pTarget,   /* Pointer to USB_TCD_ISP1582_TARGET */
    UINT8 regAddr,                     /* Address of register */
    UINT32 data                        /* Data to be written */
    )

    {
    UINT16  value = 0;

    value = (UINT16)(data & 0x0000FFFF);

    OUT_ISP1582_CMD (pTarget , regAddr);

     /* ISP 1582 uses 16 bit bus access */

    OUT_ISP1582_DATA (pTarget , value);
    
    /* Obtain the most significant 16 bits for write */
    data >>= 0x10;

    OUT_ISP1582_CMD (pTarget , regAddr + 2);

    OUT_ISP1582_DATA (pTarget , (UINT16)data);

    return;
    }

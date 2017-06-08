/* usbTcdPdiusbd12Util.c - This module contains PDIUSBD12 utility functions.*/

/*
Modification history
--------------------
01b,11may04,hch  merge after D12 driver testing
01b,19jul04,ami Coding Convention Changes
01a,15mar04,ami implementing the utility functions of D12

*/

/*
DESCRIPTION

Defines all D12 utility functions. These utility functions will be used by
the PDIUSBD12 functions implemeting the various functions codes.

INCLUDE FILES: usb/usbPlatform.h, usb/ossLib.h, usb/target/usbIsaLib.h,
               drv/usb/target/usbPdiusbd12Eval.h,
               drv/usb/target/usbTcdPdiusbd12EvalLib.h,
               drv/usb/target/usbPdiusbd12Tcd.h,
               drv/usb/target/usbPdiusbd12Debug.h
*/

/* includes */

#include "usb/usbPlatform.h"		   
#include "usb/ossLib.h" 	
#include "drv/usb/target/usbPdiusbd12Eval.h"	  
#include "drv/usb/target/usbTcdPdiusbd12EvalLib.h" 
#include "drv/usb/target/usbPdiusbd12Tcd.h"   

/* functions */

/******************************************************************************
*
* d12ReadLastTransStatusByte - function to read the last status byte
*
* This function reads the last status byte on the <endpointIndex>.
*
* RETURNS: UINT8 value read after issuing command
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL UINT8 d12ReadLastTransStatusByte
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget,/* PDISUBD12 data structure */
    UINT8	endpointIndex		/* endpoint index */
    )
    {
    OUT_D12_CMD	(pTarget , D12_CMD_READ_LAST_TRANS_STATUS | endpointIndex );
    return IN_D12_DATA (pTarget);
    }


/*******************************************************************************
*
* d12ReadIntReg - function to read the interupt register
*
* This function reads the interrupt status register. The register is read
* twice and a 16-bit value is returened.
*
* RETURNS : 16 bit interupt status value
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL UINT16 d12ReadIntReg
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 data structure */
    )
    {
    UINT8	firstByte = 0;

    OUT_D12_CMD (pTarget , D12_CMD_READ_INTERRUPT_REG);
    firstByte = IN_D12_DATA (pTarget);
    return (firstByte | ( IN_D12_DATA (pTarget) << 8));
    }

/*******************************************************************************
*
* d12SetEndpointEnable - issues Set Endpoint Enable command to PDIUSBD12
*
* This function enable an endpoint for USB operations. <enable> consists
* of the status to set.
*
* RETURNS: N/A
*
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL VOID d12SetEndpointEnable
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget,	/* PDISUBD12 data structure */
    BOOL	enable				/* status to set */
    )
    {
    OUT_D12_CMD (pTarget , D12_CMD_SET_ENDPOINT_ENABLE);
    OUT_D12_DATA ( pTarget , (enable) ? D12_CMD_SEE_ENABLE : 0);
    }

/*******************************************************************************
*
* d12SetMode - issues Set Mode command to PDIUSBD12
*
* This function set the mode of operation of PDISUBD12 TC.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12SetMode
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 data structure */
    )

    {
    OUT_D12_CMD (pTarget , D12_CMD_SET_MODE);
    OUT_D12_DATA (pTarget , pTarget->configByte);
    OUT_D12_DATA (pTarget , pTarget->clkDivByte);
    }


/******************************************************************************
*
* d12SetDma - issues Set DMA command to PDIUSBD12
*
* This function writes in to the DMA register. It gives the command to 
* canrry out DMA operation in PDIUSBD12 TC.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12SetDma
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 data structure */
    )
    {
    OUT_D12_CMD (pTarget , D12_CMD_SET_DMA);
    OUT_D12_DATA (pTarget , pTarget->dmaByte);
    }

/******************************************************************************
*
* d12SelectEndpoint - issues Select Endpoint command to PDIUSBD12
*
* This function selects an endpoint for USB Operations. <endpoint> 
* parameter must be D12_ENDPOINT_xxxx.
*
* RETURNS: UINT8 value read after issuing select endpoint command
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL UINT8 d12SelectEndpoint
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget,	/* PDISUBD12 data structure */
    UINT8	endpoint			/* endpoint Index */
    )

    {
    OUT_D12_CMD (pTarget , D12_CMD_SELECT_ENDPOINT | endpoint);
    return IN_D12_DATA (pTarget);
    }


/*******************************************************************************
*
* d12ClearBfr - issues Clear Buffer command to PDIUSBD12
*
* This function explicitly clears the FIFO buffers of the endpoints.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12ClearBfr
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 Data Structure */
    )

    {
    OUT_D12_CMD (pTarget , D12_CMD_CLEAR_BUFFER);
    }


/*******************************************************************************
*
* d12ValidateBfr - Issues Validate Buffer command to PDIUSBD12
*
* This function validates the FIFO Buffers.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12ValidateBfr
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 Data Structure */
    )
    {
    OUT_D12_CMD (pTarget , D12_CMD_VALIDATE_BUFFER);
    }

/*******************************************************************************
*
* d12AckSetup - issues Acknowledge setup command to PDIUSBD12
*
* This function acknowledges the setup packet. It also re-enables the 
* validate buffer and clear buffer commands.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12AckSetup
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget,	/* PDISUBD12 Data Structure */
    UINT8	endpoint			/* endpoint index */
    )
    {
    d12SelectEndpoint (pTarget, endpoint);
    OUT_D12_CMD (pTarget , D12_CMD_ACK_SETUP);
    }

/******************************************************************************
*
* d12ReadChipId - issues Read Chip Id command
*
* This function reads the chip id of PDIUSBD12 peripheral controller.
*
* RETURNS: chip ID returned by PDIUSBD12
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL UINT16 d12ReadChipId
    (
    pUSB_TCD_PDIUSBD12_TARGET	pTarget		/* PDISUBD12 Data Structure */
    )
    {
    UINT8 firstByte = 0;

    OUT_D12_CMD (pTarget , D12_CMD_READ_CHIP_ID);
    firstByte = IN_D12_DATA (pTarget);
    return (firstByte | (IN_D12_DATA (pTarget) << 8));
    }

/******************************************************************************
*
* d12SetEndpointStatus - issues Set Endpoint Status command to PDIUSBD12
*
* This function sets the status of the endpoint for PDISUBD12.
*
* RETURNS: N/A
*
* ERRNO:
*  None.
*
* \NOMANUAL
*/

LOCAL VOID d12SetEndpointStatus
    (
    pUSB_TCD_PDIUSBD12_TARGET pTarget,		/* PDISUBD12 Data Structure */
    UINT8	endpoint,			/* Endpoint Index */
    UINT8	status				/* status to set */
    )

    {
    OUT_D12_CMD (pTarget , D12_CMD_SET_ENDPOINT_STATUS | endpoint);
    OUT_D12_DATA (pTarget , status);
    }

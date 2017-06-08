/* usbTargRbcLib.c - USB Reduced Block Command set routine library */

/* Copyright 2004 Wind River Systems, Inc. */

/*
DESCRIPTION

This module defines the USB_ERP callback routines directly used by the 
USB 2.0 mass storage driver. These callback routines invoke the 
routines defined in the file usbTargRbcCmd.c.

INCLUDES: vxWorks.h, ramDrv.h, cbioLib.h, logLib.h,
          usb/usbPlatform.h, usb/usb.h, usb/usbdLib.h,
          usb/target/usbTargLib.h, drv/usb/usbBulkDevLib.h,
          drv/usb/target/usbTargMsLib.h, drv/usb/target/usbTargRbcCmd.h

*/

/*
Modification History
--------------------
01f,02aug04,mta  Changes to modification history
01e,23jul04,hch  change vxworks.h to vxWorks.h
01d,23jul04,ami  Coding Convention Changes
01c,21jul04,ami  Cleaned the warning messages
01b,19jul04,hch  created the file element
01a,15mar04,jac  written.
*/

/* includes */
#include "vxWorks.h"
#include "ramDrv.h"
#include "cbioLib.h"
#include "usb/usbPlatform.h"
#include "usb/usb.h"
#include "usb/target/usbTargLib.h"

#include "usb/usbdLib.h"
#include "drv/usb/usbBulkDevLib.h"
#include "drv/usb/target/usbTargMsLib.h"
#include "drv/usb/target/usbTargRbcCmd.h"

/* defines */

/* RBC commands */

#define RBC_CMD_FORMAT			0x04	/* format command */
#define RBC_CMD_READ10			0x28	/* read (10) command */ 
#define RBC_CMD_READCAPACITY		0x25	/* read media capacity command*/
#define RBC_CMD_STARTSTOPUNIT		0x1B	/* start/stop file */
#define RBC_CMD_SYNCCACHE		0x35	/* sync. cache command */
#define RBC_CMD_VERIFY10		0x2F	/* verify (10) command */
#define RBC_CMD_WRITE10			0x2A	/* write (10) command */

/* SPC-2 commands */

#define RBC_CMD_INQUIRY			0x12	/* inquiry command */
#define RBC_CMD_MODESELECT6		0x15	/* mode select command */
#define RBC_CMD_MODESENSE6		0x1A	/* mode sense command */
#define RBC_CMD_PERSISTANTRESERVIN	0x5E	/* reserve IN */
#define RBC_CMD_PERSISTANTRESERVOUT	0x5F	/* reserve OUT */
#define RBC_CMD_PRVENTALLOWMEDIUMREMOVAL 0x1E	/* prevent/ allow medium */
						/* removal */
#define RBC_CMD_RELEASE6		0x17	/* release command */
#define RBC_CMD_REQUESTSENSE		0x03	/* request sense command */
#define RBC_CMD_RESERVE6		0x16	/* reserve command */
#define RBC_CMD_TESTUNITREADY		0x00	/* test unit ready command */
#define RBC_CMD_WRITEBUFFER		0x3B	/* write buffer */

/* vendor specific commands */

#define RBC_CMD_VENDORSPECIFIC_23	0x23	/* vendor specific command */
#define RBC_CMD_SIZE6			0x06	/* size 6 command */
#define RBC_CMD_SIZE10			0x0a	/* size 10 command */

#define CBW_DATA_IN		1		/* Command Block Data IN */	
#define CBW_DATA_OUT		0		/* Command Block Data Out */
#define RBC_LUN			0		/* Logical Unit Number */

/* globals */

BOOL	g_bulkInStallFlag = FALSE;		/* Bulk IN data */
BOOL	g_bulkOutStallFlag = FALSE;		/* Bulk OUT data */


/* forward declarations */

void bulkInErpCallbackCSW   (pVOID erp);
void bulkOutErpCallbackData (pVOID erp);
void bulkInErpCallbackData  (pVOID erp);
void bulkInErpCallbackCSW   (pVOID erp);
void bulkOutErpCallbackCBW  (pVOID erp);


/*******************************************************************************
*
* bulkOutErpCallbackCBW - process the CBW on bulk-out pipe
*
* This routine processes the the CBW (Command Block Wrapper) which is received
* on the bulk out pipe.
*
* RETURNS: N/A
* 
* ERRNO:
*  none
*/

void bulkOutErpCallbackCBW
    (
    pVOID	erp		/* USB_ERP endpoint request packet */
    )
    {
    USB_ERP   	    *pErp     = (USB_ERP *)erp; /* USB_ERP */        
    STATUS          rbcRetVal = ERROR;		/* return value */
    BOOL            rbcDataIn = FALSE;		/* flag for data in */
    BOOL            rbcDataOut = FALSE;		/* flag for data out */ 
    UINT8           * pData   = NULL;		/* data buffer */
    UINT8           **ppData  = &pData;
    UINT32          dSize     = 0;		/* size of data */
    BOOL            validCBW  = FALSE;		/* flag for CBW */
    BOOL            validReq  = TRUE;		/* flag for request */
    USB_BULK_CBW    *pCbw  = NULL;		/* Command Block Wrapper */
    USB_BULK_CSW    *pCsw  = NULL;		/* Command Block Status */
    UINT8           opCode    = 0;		/* opcode */
    UINT32          cbwDataXferLgth = 0;	/* transfer length */
    UINT8           cbwDataDir;			/* direction */
    ERP_CALLBACK    erpCallback = NULL;		/* ERP_CALLBACK pointer */

    /* debug print */

    usbDbgPrint("bulkOutErpCallbackCBW: Enter...\n");

    /* signal that bulk out ERP is complete */

    usbMsBulkOutErpInUseFlagSet (FALSE);

    /* if we had a reset condition, then we are no longer configured */

    if (usbMsIsConfigured() == FALSE)
        return;

    /* verify that the data is good */

    if (pErp->result != OK)
        return;

    /*
     * retrieve CBW from pErp and test if valid 
     * NOTE: USB_BULK_CBW has an odd size, so this struct must be byte 
     * aligned and packed 
     */

    if (pErp->bfrList[0].actLen == sizeof(USB_BULK_CBW))
        {
        pCbw = (USB_BULK_CBW *)(pErp->bfrList[0].pBfr);

        if (pCbw->signature == USB_BULK_SWAP_32(USB_CBW_SIGNATURE))
            {
            if (pCbw->lun == RBC_LUN) 

                /* must be this LUN */

                {
                if ((pCbw->length >= 1) && (pCbw->length <= USB_CBW_MAX_CBLEN))
                    validCBW = TRUE;
                }
            }
        }

    if (validCBW == TRUE)
        {

        /*
         * init CSW, set status to "command good",
         * set CSW tag to CBW tag 
         */

        pCsw = usbMsCSWInit();
        pCsw->status = USB_CSW_STATUS_PASS;
        pCsw->tag = pCbw->tag;
        pCsw->dataResidue = 0x0;
        
        /* get data transfer lgth, and correct endian */

        cbwDataXferLgth = USB_BULK_SWAP_32(pCbw->dataXferLength);

        /* set data direction flag from CBW */

        if(((pCbw->direction & USB_CBW_DIR_IN) >> 7) == 0x1)
            cbwDataDir = CBW_DATA_IN;
        else
            cbwDataDir = CBW_DATA_OUT;

        /* retrieve op code and execute RBC command */

        opCode = pCbw->CBD[0];

        switch(opCode)
            {

            /* RBC commands */

            case RBC_CMD_FORMAT:
                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcFormat(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_READ10:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcRead(&pCbw->CBD[0], ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_READCAPACITY:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcCapacityRead(&pCbw->CBD[0], 
                                                 ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_STARTSTOPUNIT:
                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcStartStop(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;


            case RBC_CMD_SYNCCACHE:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcCacheSync(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_VERIFY10:
                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcVerify(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_WRITE10:
                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcWrite(&pCbw->CBD[0], ppData, &dSize);
                    erpCallback = bulkOutErpCallbackData;
                    rbcDataOut = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            /* SPC-2 commands */

            case RBC_CMD_INQUIRY:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcInquiry(&pCbw->CBD[0], ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_MODESELECT6:
                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcModeSelect(&pCbw->CBD[0], 
                                               ppData, &dSize);
                    erpCallback = bulkOutErpCallbackData;
                    rbcDataOut = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_MODESENSE6:
                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcModeSense(&pCbw->CBD[0], 
                                               ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_PERSISTANTRESERVIN:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcPersistentReserveIn(&pCbw->CBD[0], 
                                               ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_PERSISTANTRESERVOUT:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcPersistentReserveOut(&pCbw->CBD[0], 
                                               ppData, &dSize);
                    erpCallback = bulkOutErpCallbackData;
                    rbcDataOut = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_PRVENTALLOWMEDIUMREMOVAL:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcPreventAllowRemoval(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_RELEASE6:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcRelease(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_REQUESTSENSE:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcRequestSense(&pCbw->CBD[0], 
                                                 ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_RESERVE6:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcReserve(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_TESTUNITREADY:

                if (pCbw->length == RBC_CMD_SIZE6)
                    {
                    rbcRetVal = usbTargRbcTestUnitReady(&pCbw->CBD[0]);
                    erpCallback = bulkInErpCallbackCSW;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_WRITEBUFFER:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcBufferWrite(&pCbw->CBD[0], 
                                                ppData, &dSize);
                    erpCallback = bulkOutErpCallbackData;
                    rbcDataOut = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            case RBC_CMD_VENDORSPECIFIC_23:

                if (pCbw->length == RBC_CMD_SIZE10)
                    {
                    rbcRetVal = usbTargRbcVendorSpecific(&pCbw->CBD[0], 
                                                   ppData, &dSize);
                    erpCallback = bulkInErpCallbackData;
                    rbcDataIn = TRUE;
                    }
                else
                    goto EXIT_ERROR;
                break;

            default:
                validReq = FALSE;
                goto EXIT_ERROR;
            }

        if (rbcRetVal == OK)
            {
            if (rbcDataIn == TRUE)
                {
                if (cbwDataXferLgth > 0)
                    {
                    if (cbwDataDir == CBW_DATA_IN)
                        {
                        if (cbwDataXferLgth == dSize)
                            {
                            pCsw->dataResidue = 0x0;

                            /* 
                             * init Data-In ERP on Bulk-In pipe and submit
                             * to TCD 
                             */

                            if (usbMsBulkInErpInit(pData,dSize,
                                                   erpCallback,NULL) != OK)
                                goto EXIT_ERROR;
                            }
                        else if (cbwDataXferLgth > dSize)
                            {
                            pCsw->dataResidue = cbwDataXferLgth - dSize;

                            /*
                             * init Data In ERP on Bulk-In pipe and submit
                             * to TCD 
                             */

                            if (usbMsBulkInErpInit(pData,dSize,
                                                   erpCallback,NULL) != OK)
                                goto EXIT_ERROR;
                            }
                        else
                            {

                            /* set CSW Status 0x2 (phase error) */

                            pCsw->status = USB_CSW_PHASE_ERROR;
                            pData = (UINT8 *)pCsw;
                            dSize = sizeof(USB_BULK_CSW);
                            erpCallback = bulkInErpCallbackCSW;

                            /* init CSW ERP on Bulk-in pipe and submit */

                            if (usbMsBulkInErpInit(pData, dSize,
                                                   erpCallback, NULL) != OK)
                                goto EXIT_ERROR;
                            }
                        }
                    else
                        {

                        /* set CSW Status 0x2 (phase error) */

                        pCsw->status = USB_CSW_PHASE_ERROR;
                        pData = (UINT8 *)pCsw;
                        dSize = sizeof(USB_BULK_CSW);
                        erpCallback = bulkInErpCallbackCSW;

                        /* init CSW ERP on Bulk-in pipe and submit */

                        if (usbMsBulkInErpInit(pData, dSize, erpCallback, NULL)
                            != OK)
                            goto EXIT_ERROR;
                        }
                    }
                else
                    {

                    /* set CSW Status 0x2 (phase error) */

                    pCsw->status = USB_CSW_PHASE_ERROR;
                    pData = (UINT8 *)pCsw;
                    dSize = sizeof(USB_BULK_CSW);
                    erpCallback = bulkInErpCallbackCSW;

                    /* init CSW ERP on Bulk-in pipe and submit */

                    if (usbMsBulkInErpInit(pData, dSize, erpCallback, NULL)
                        != OK)
                        goto EXIT_ERROR;
                    }
                }
            else if (rbcDataOut == TRUE)
                {
                if (cbwDataXferLgth > 0)
                    {
                    if (cbwDataDir == CBW_DATA_OUT)
                        {
                        if (cbwDataXferLgth == dSize)
                            {
                            pCsw->dataResidue = 0x0;


                            /* init Bulk-Out ERP and submit to TCD */

                            if (usbMsBulkOutErpInit(pData, dSize, erpCallback,NULL)
                                != OK)
                                goto EXIT_ERROR;
                            }
                        else if (cbwDataXferLgth > dSize)
                            {
                            pCsw->dataResidue = cbwDataXferLgth - dSize;

                            /* set stall flag on Bulk-Out Endpoint */

                            g_bulkOutStallFlag = TRUE;

                            /* init Data Out ERP on Bulk-Out Pipe and submit */

                            if (usbMsBulkOutErpInit(pData, dSize, erpCallback,
                                                    NULL) != OK)
                                goto EXIT_ERROR;
                            }
                        else
                            {

                            /* set CSW Status 0x2 (phase error) */

                            pCsw->status = USB_CSW_PHASE_ERROR;
                            pData = (UINT8 *)pCsw;
                            dSize = sizeof(USB_BULK_CSW);
                            erpCallback = bulkInErpCallbackCSW;

                            /* init CSW ERP on Bulk-in pipe and submit */

                            if (usbMsBulkInErpInit(pData, dSize, erpCallback, 
                                                   NULL) != OK)
                                goto EXIT_ERROR;
                            }
                        }
                    else
                        {

                        /* set CSW Status 0x2 (phase error) */

                        pCsw->status = USB_CSW_PHASE_ERROR;
                        pData = (UINT8 *)pCsw;
                        dSize = sizeof(USB_BULK_CSW);
                        erpCallback = bulkInErpCallbackCSW;

                        /* init CSW ERP on Bulk-in pipe and submit */

                        if (usbMsBulkInErpInit(pData, dSize, 
                                               erpCallback, NULL) != OK)
                            goto EXIT_ERROR;
                        }
                    }
                else
                    {

                    /* set CSW Status 0x2 (phase error) */

                    pCsw->status = USB_CSW_PHASE_ERROR;
                    pData = (UINT8 *)pCsw;
                    dSize = sizeof(USB_BULK_CSW);
                    erpCallback = bulkInErpCallbackCSW;

                    /* init CSW ERP on Bulk-in pipe and submit */

                    if (usbMsBulkInErpInit (pData, dSize, erpCallback, NULL)
                        != OK)
                        goto EXIT_ERROR;
                    }
                }
            else
                {
                if (cbwDataXferLgth == 0)
                    {

                    /* init CSW ERP on Bulk-in pipe submit ERP */

                    pCsw->dataResidue = 0x0;
                    pData = (UINT8 *)pCsw;
                    dSize = sizeof(USB_BULK_CSW);
                    erpCallback = bulkInErpCallbackCSW;

                    /* init CSW ERP on Bulk-in pipe and submit */

                    if (usbMsBulkInErpInit(pData, dSize, erpCallback, NULL)
                        != OK)
                        goto EXIT_ERROR;
                    }
                else
                    {
                    /* set CSW residue size, dSize = 0 */

                    pCsw->dataResidue = cbwDataXferLgth;

                    if (cbwDataDir == CBW_DATA_IN)
                        {

                        /* set stall flag on Bulk-In Pipe */

                        g_bulkInStallFlag = TRUE;

                        /* 
                         * init Data In ERP on Bulk-In pipe and submit
                         * to TCD 
                         */

                        if (usbMsBulkInErpInit(pData,dSize,
                                               erpCallback,NULL) != OK)
                            goto EXIT_ERROR;
                        }
                    else
                        {

                        /* set stall flag on Bulk-In Pipe */

                        g_bulkOutStallFlag = TRUE;

                        /* init Data In ERP on Bulk-In pipe and submit to TCD */

                        if (usbMsBulkOutErpInit(pData, dSize, erpCallback,NULL)
                            != OK)
                            goto EXIT_ERROR;
                        }
                    }
                }
            }
        else
            {

            /* set CSW Status 0x1 (command failed) */

            pCsw->status = USB_CSW_STATUS_FAIL;
            pData = (UINT8 *)pCsw;
            dSize = sizeof(USB_BULK_CSW);
            erpCallback = bulkInErpCallbackCSW;

            /* init CSW ERP on Bulk-in pipe and submit */

            if (usbMsBulkInErpInit(pData, dSize, erpCallback, NULL) != OK)
                goto EXIT_ERROR;
            }   
        }
    else
        {
        goto EXIT_ERROR;
        }

    return;

EXIT_ERROR:

    usbMsBulkInStall();
    usbMsBulkOutStall();

    return;
    }

/*******************************************************************************
*
* bulkInErpCallbackCSW - send the CSW on bulk-in pipe
*
* This routine sends the CSW (Command Status Wrapper) back to the host following
* execution of the CBW.
*
* RETURNS: N/A
* 
* ERRNO:
*  none
*/
void bulkInErpCallbackCSW
    (
    pVOID	erp		/* USB_ERP endpoint request packet */
    )
    {
    UINT8		*pData;			/* data buffer */
    UINT32		dSize;				/* size */
    USB_ERP		*pErp = (USB_ERP *)erp;	/* USB_ERP */

    usbDbgPrint("bulkInErpCallbackCSW: Enter...\n");

    /* signal that bulk in ERP is complete */

    usbMsBulkInErpInUseFlagSet (FALSE);

    /* if a reset ocurred, we are no longer configured */

    if (usbMsIsConfigured () == FALSE)
        return;

    /* verify data is valid */

    if (pErp->result != OK)
        return;

    pData = (UINT8 *)usbMsCBWInit();
    dSize = sizeof (USB_BULK_CBW);

    /* init bulk-out ERP w/bulkOutErpCallbackCBW() callback */

    if (usbMsBulkOutErpInit(pData, dSize, 
                        bulkOutErpCallbackCBW, NULL) != OK)
        usbMsBulkOutStall();
	return;
    }

/*******************************************************************************
*
* bulkInErpCallbackData - process end of data phase on bulk-in pipe
*
* This routine is invoked following a data IN phase to the host.
*
* RETURNS: N/A
* 
* ERRNO:
*  none
*/

void bulkInErpCallbackData
    (
    pVOID		erp		/* USB_ERP endpoint request packet */ 
    )
    {
    USB_ERP		*pErp = (USB_ERP *)erp;	/* USB_ERP */
    USB_BULK_CBW        *pCbw;			/* USB_BULK_CBW	*/
    USB_BULK_CSW	*pCsw;			/* USB_BULK_CSW	*/ 
    UINT8		 opCode;		/* operation code */
    UINT8		*pData;		/* pointer to buffer */
    UINT32		 dSize;		/* size */

    usbDbgPrint("bulkInErpCallbackData: Enter...\n"); /* FIXME_JAC */

    /* signal that bulk in ERP is complete */

    usbMsBulkInErpInUseFlagSet (FALSE);

    /* if a reset ocurred, we are no longer configured */

    if (usbMsIsConfigured() == FALSE)
        return;

    /* verify data is valid */

    if (pErp->result != OK)
        return;

    if (g_bulkInStallFlag == TRUE)
        usbMsBulkInStall();
    else
        {
        pCsw = usbMsCSWGet();
        pCbw = usbMsCBWGet();
        opCode = pCbw->CBD[0];

        switch(opCode)
            {

            /* place any user specific code here */

            case RBC_CMD_FORMAT:
            case RBC_CMD_READ10:
            case RBC_CMD_READCAPACITY:
            case RBC_CMD_STARTSTOPUNIT:
            case RBC_CMD_SYNCCACHE:
            case RBC_CMD_VERIFY10:
            case RBC_CMD_WRITE10:
            case RBC_CMD_INQUIRY:
            case RBC_CMD_MODESELECT6:
            case RBC_CMD_MODESENSE6:
            case RBC_CMD_PERSISTANTRESERVIN:
            case RBC_CMD_PERSISTANTRESERVOUT:
            case RBC_CMD_PRVENTALLOWMEDIUMREMOVAL:
            case RBC_CMD_RELEASE6:
            case RBC_CMD_REQUESTSENSE:
            case RBC_CMD_RESERVE6:
            case RBC_CMD_TESTUNITREADY:
            case RBC_CMD_WRITEBUFFER:
            default:
            break;
            }

        /* init CSW ERP on Bulk-in pipe and submit */

        pData = (UINT8 *)pCsw;
        dSize = sizeof(USB_BULK_CSW);

        /* init CSW ERP on Bulk-in pipe and submit */

        if (usbMsBulkInErpInit(pData, dSize, bulkInErpCallbackCSW, NULL) != OK)
            usbMsBulkInStall();

        }
    return;
    }

/*******************************************************************************
*
* bulkOutErpCallbackData - process end of data phase on bulk-out pipe
*
* This routine is invoked following a data OUT phase from the host.
*
* RETURNS: N/A
* 
* ERRNO:
*  none
*/
void bulkOutErpCallbackData
    (
    pVOID	erp		/* USB_ERP endpoint request packet */  
    )
    {
    USB_ERP		*pErp = (USB_ERP *)erp;	/* USB_ERP */	 
    USB_BULK_CBW	*pCbw;			/* USB_BULK_CBW */    	
    USB_BULK_CSW	*pCsw;			/* USB_BULK_CSW */     
    UINT8		opCode;			/* operation code */
    UINT8		*pData;			/* pointer to buffer */
    UINT32		dSize;			/* size */
    UINT32		blkNum;			/* block nubmer */
    UINT16		numBlks;		/* number of blocks */

    usbDbgPrint("Enter bulkOutErpCallbackData\n"); 

    /* signal that bulk out ERP is complete */

    usbMsBulkOutErpInUseFlagSet(FALSE);

    /* if a reset ocurred, we are no longer configured */

    if (usbMsIsConfigured() == FALSE)
        return;

    /* verify data is valid */

    if (pErp->result != OK)
        return;

    if (g_bulkOutStallFlag == TRUE)
        usbMsBulkOutStall();
    else
        {
        pCsw = usbMsCSWGet();
        pCbw = usbMsCBWGet();
        opCode = pCbw->CBD[0];

        switch(opCode)
            {

            /* place any user specific code here */

            case RBC_CMD_FORMAT:
                break;
            case RBC_CMD_READ10:
                break;
            case RBC_CMD_READCAPACITY:
                break;
            case RBC_CMD_STARTSTOPUNIT:
                break;
            case RBC_CMD_SYNCCACHE:
                break;
            case RBC_CMD_VERIFY10:
                break;
            case RBC_CMD_WRITE10:
                {
                CBIO_DEV_ID cbio;
                STATUS      retVal;
                cookie_t    cookie;

                cbio = (CBIO_DEV_ID)usbTargRbcBlockDevGet();


                /* get starting LBA from arg[2] - arg[5] of WRITE CBW */

                blkNum = (pCbw->CBD[2] << 24) |
                         (pCbw->CBD[3] << 16) |
                         (pCbw->CBD[4] << 8)  |
                         (pCbw->CBD[5]);

                /* get transfer length from WRITE CBW */

                numBlks = (pCbw->CBD[7] << 8) |
                          (pCbw->CBD[8]);

                retVal = cbioBlkRW (cbio, blkNum, numBlks, 
                                   (addr_t)pErp->bfrList [0].pBfr, CBIO_WRITE, 
                                    &cookie );

                if (retVal == ERROR)
                    {
                    usbMsBulkOutStall();
                    }
                }
                break;

            case RBC_CMD_INQUIRY:
            case RBC_CMD_MODESELECT6:
            case RBC_CMD_MODESENSE6:
            case RBC_CMD_PERSISTANTRESERVIN:
            case RBC_CMD_PERSISTANTRESERVOUT:
            case RBC_CMD_PRVENTALLOWMEDIUMREMOVAL:
            case RBC_CMD_RELEASE6:
            case RBC_CMD_REQUESTSENSE:
            case RBC_CMD_RESERVE6:
            case RBC_CMD_TESTUNITREADY:
            case RBC_CMD_WRITEBUFFER:
            default:
            break;
            }

        /* init CSW ERP on Bulk-in pipe and submit */

        pData = (UINT8 *)pCsw;
        dSize = sizeof(USB_BULK_CSW);

        /* init CSW ERP on Bulk-in pipe and submit */

        if (usbMsBulkInErpInit(pData,dSize, 
                               bulkInErpCallbackCSW, NULL) != OK)
            usbMsBulkInStall();
        }

    return;
    }


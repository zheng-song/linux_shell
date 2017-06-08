/* usbTargRbcCmd.c - Reduced Block Command set routine library */

/* Copyright 2004 Wind River Systems, Inc. */

/*
DESCRIPTION

This module implements a framework based on the RBC (Reduced Block Command)
set. These routines are invoked by the USB 2.0 mass storage driver based
on the contents of the USB CBW (command block wrapper).

INCLUDES: vxWorks.h, disFsLib.h, dcacheCbio.h, ramDrv.h, usrFdiskPartLib.h,
          usb/usbPlatform.h, usb/usb.h, usb/target/usbTargLib.h, 
          drv/usb/target/usbTargMsLib.h, drv/usb/target/usbTargRbcCmd.h

*/

/*
modification history
--------------------
01g,02aug04,mta  Merge from Integration Branch to development branch
01f,29jul04,pdg  Diab warnings fixed
01e,23jul04,hch  change vxworks.h to vxWorks.h
01d,23jul04,ami  Coding Convention Changes
01c,21jul04,pdg  Changes for windows host
01b,19jul04,hch  created the file element
01a,12mar04,jac  written.
*/

/* includes */

#include "vxWorks.h"
#include "dosFsLib.h"
#include "dcacheCbio.h"
#include "ramDrv.h"
#include "ramDiskCbio.h"
#include "usrFdiskPartLib.h"

#include "usb/usbPlatform.h"
#include "usb/usb.h"
#include "usb/usbLib.h"
#include "usb/target/usbTargLib.h"
#include "drv/usb/usbBulkDevLib.h"
#include "drv/usb/target/usbTargMsLib.h"
#include "drv/usb/target/usbTargRbcCmd.h"

/* defines */

#define DATA_BUFFER_LEN		BYTES_PER_BLOCK * NUM_BLOCKS /* data buffer */
#define RBC_MODE_PARAMETER_PAGE	0x06		/* RBC Mode Page */		
#define BLK_DEV_NAME		"/bd/0"		/* bulk device name */

#ifdef USE_RBC_SUBCLASS
    #define PERIPHERAL_DEVICE_TYPE	0xe  /* Simplified direct-access */ 
                                             /* device (RBC) */
#elif defined(USE_SCSI_SUBCLASS)
    #define PERIPHERAL_DEVICE_TYPE      0x0  /* Direct-access device */
#else
    #error PERIPHERAL_DEVICE_TYPE undefined
#endif

#define PERIPHERAL_DEVICE_QUAL          0x0	/* device qualifier */	

#define MIN_STD_INQUIRY_LGTH            36	/* inquiry length */
#define VPD_SUPPORTED_PAGE_CODE         0x0	/* VPD page code */
#define VPD_UNIT_SERIAL_NUM_PAGE_CODE   0x80	/* VPD serial page number */
#define VPD_DEVICE_ID_PAGE_CODE         0x83	/* VPD device id page code */
#define VENDOR_SPECIFIC_23_LENGTH	12	/* vendor specific length */


/* locals */

LOCAL VPD_SUPPORTED_PAGE g_vpdSupportedPage =
    {
    ((PERIPHERAL_DEVICE_QUAL & 0x7) << 5) | 
      PERIPHERAL_DEVICE_TYPE,       /* device qualifier and device type */
    0x0,                            /* this page has page code = 0 */
    0x0,                            /* reserved */
    0x3,                            /* page length is three */
    VPD_SUPPORTED_PAGE_CODE,        /* page zero is supported */
    VPD_UNIT_SERIAL_NUM_PAGE_CODE,  /* serial num page */
    VPD_DEVICE_ID_PAGE_CODE         /* device ID page */
    };

LOCAL VPD_DEVICE_ID_PAGE g_vpdDevIdPage = 
    {
    ((PERIPHERAL_DEVICE_QUAL & 0x7) << 5) | 
      PERIPHERAL_DEVICE_TYPE,   /* device qualifier and device type */
    VPD_DEVICE_ID_PAGE_CODE,    /* this page code */
    0x0,                        /* reserved */
    0x1,                        /* page length is one */
        {
        0x2,                    /* code set is ASCI */
        0x1,                    /* association is zero, ID type is Vendor ID */
        0x0,                    /* reserved */
        DEVICE_ID_LGTH,         /* device ID length */
        {DEVICE_ID}             /* the ASCI device ID */
        }
    };

LOCAL VPD_UNIT_SERIAL_NUM_PAGE g_vpdSerialNumPage = 
    {
    ((PERIPHERAL_DEVICE_QUAL & 0x7) << 5) | 
      PERIPHERAL_DEVICE_TYPE,       /* device qualifier and device type */
    VPD_UNIT_SERIAL_NUM_PAGE_CODE,  /* this page code */
    0x0,                            /* reserved */
    24,                             /* this page length (n - 3) */
        {
        '0',0, 	                    /* SerialNumber */
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0,
        '0',0
        }
    };


LOCAL COMMAND_DATA g_cmdData = 
    {
    ((PERIPHERAL_DEVICE_QUAL & 0x7) << 5) | 
      PERIPHERAL_DEVICE_TYPE,   /* device qualifier and device type */
    0x1,        /* The device server does not support the tested SCSI */
                /* operation code. All data after byte 1 is undefined */
        { 
        0x0,    /* reserved fields */ 
        0x0
        },    
    0x2,        /* cdbSize */
    0x0,        /* cdbOpCode */
    0x0         /* cdbUsageMap */
    };

LOCAL STD_INQUIRY_DATA g_stdInquiryData = 
    {
    ((PERIPHERAL_DEVICE_QUAL & 0x7) << 5) | 
      PERIPHERAL_DEVICE_TYPE,  /* device qualifier and device type */
    0x80,   /* media is removable */
    0x0,    /* The device does not claim conformance to any standard. */
    0x2,    /* data format corresponds to this standard */
    91,     /* n - 4 (95-4) */
    0x0,    /* sccs = 0, no embedded storage ctrl */
    0x0,    /* BQUE ENCSERV VS MULTIP MCHNGR Obsolete Obsolete ADDR16 */
    0x0,    /* RELADR Obsolete WBUS16 SYNC LINKED Obsolete CMDQUE VS */
        {
        'W','R','S',' ',    /* vendorId[8] */
        ' ',' ',' ',' '
        },
        {
        'U','S','B',' ',    /* productId[16] */
        'M','a','s','s',
        ' ','S','t','o',
        'r','a','g','e'
        },
        {
        '1','.','0','0'     /* productRevisionLevel[4] */
        },
        {
        0, 0, 0, 0,         /* vendorSpecific[20] */
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
        },
    0x0,                    /* Reserved CLOCKING QAS IUS */
    0x0,
        {		    /* UINT16 VersionDescriptor[8] */
        0x023C,             /* RBC ANSI NCITS.330:2000 */
        0x0260,             /* SPC-2 (no version claimed) */
        0, 0,         
        0, 0, 0, 0
        },
        {
        0, 0, 0, 0,         /* UINT8 Reserved3[22] */
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0
        }
    };


LOCAL MODE_PARAMETER_HEADER g_deviceHdr = 
    {
    sizeof(MODE_PARAMETER_LIST)-1,  /* data len */
    0x0,                            /* reqd. by RBC */
    0x0,                            /* reqd. by RBC */
    0x0                             /* reqd. by RBC */
    };

LOCAL MODE_PARAMETER_LIST g_deviceParamList = 
    {
    /* g_deviceHdr */
    {
    sizeof (MODE_PARAMETER_LIST)-1, /* data len */
    0x0,                            /* reqd. by RBC */
    0x0,                            /* reqd. by RBC */
    0x0                             /* reqd. by RBC */
    },
    /* g_deviceParams */
    {
    0x80 | RBC_MODE_PARAMETER_PAGE, /* bit 7 is page save (always 1), */
                                    /* page code is RBC (0x6) */
    0xb,                            /* page length */
    0x1,                            /* writeCacheDisable: return status */
                                    /*after a WRITE */
	{

        /*Logical block Size = 512 Bytes (0x200) */

        (BYTES_PER_BLOCK & 0xff00) >> 8,
        BYTES_PER_BLOCK & 0xff
	},
	{

	/*Number of logical blocks = 400 (0x190) */

        (NUM_BLOCKS & 0xff000000) >> 24,
        (NUM_BLOCKS & 0x00ff0000) >> 16,
        (NUM_BLOCKS & 0x0000ff00) >> 8,
        (NUM_BLOCKS & 0x000000ff),
        },
    0xFF,                           /* highest possible Power/Peformance */
    0x3,                            /* read and write access, media cannot */
                                    /*be formatted or locked */
    0x0                             /* reserved */
    }
    };

LOCAL MODE_PARAMETER_LIST g_deviceParamListMask = {
    /* g_deviceHdr */
    {
    sizeof (MODE_PARAMETER_LIST)-1, /* data len */
    0x0,                            /* reqd. by RBC */
    0x0,                            /* reqd. by RBC */
    0x0                             /* reqd. by RBC */
    },
    /* g_deviceParamsMask */
    {
    0x80 | RBC_MODE_PARAMETER_PAGE,  /* bit 7 is page save (always 1), */
                                     /* page code is RBC (0x6) */
    0x0,                             /* page length */
    0x0,                             /* write cached disabled bit */
        {

	/* Logical block Size */

        0x00,
        0x00
        },
        {
	
        /* Number of logical blocks */

        0x00,
        0x00,
        0x00,
        0x00,
        0x00
        },
    0x0,                             /* highest possible Power/Peformance */
    0x0,                             /* READD WRITED FORMATD LOCKD */
    0x0                              /* reserved */
    }
};


LOCAL SENSE_DATA g_senseData = 
    {
    0x70, 			/* VALID = 0, responseCode = current */
				/*sense data */
    0x0,  			/* obsolete */
    SCSI_SENSE_NO_SENSE,	/* senseKey = NO SENSE */
        {
        0x0,    		/* info fields all zero */
        0x0,
        0x0,
        0x0 
        },
    0xa,    			/* additionalSenseLgth always 0xa */
        {
        0x0, 			/* cmdSpecificInfo[4] depends on command */
        0x0,
        0x0,
        0x0
        },
    SCSI_ADSENSE_NO_SENSE,  	/* ASC = NO ADDITIONAL SENSE INFORMATION */
    0x0,                    	/* ASCQ = NO ADDITIONAL SENSE INFORMATION */
    0x0, 			/* fieldReplaceableUnitCode: 0 = no failures */
				/* or no data avail */
        {
        0x0, 			/* bit 7 = 0 (invalid) => sense key specific */
				/* data not defined */
        0x0,
        0x0
        }
    };

LOCAL CAPACITY_DATA g_capacityData = 
    {

    /* capacity data for the blocks */

    ((NUM_BLOCKS - 1) & 0xff000000) >> 24,
    ((NUM_BLOCKS - 1) & 0x00ff0000) >> 16,
    ((NUM_BLOCKS - 1) & 0x0000ff00) >> 8,
    (NUM_BLOCKS - 1) & 0x000000ff,
    (BYTES_PER_BLOCK & 0xff000000) >> 24,
    (BYTES_PER_BLOCK & 0x00ff0000) >> 16,
    (BYTES_PER_BLOCK & 0x0000ff00) >> 8,
    BYTES_PER_BLOCK & 0x000000ff
    };


LOCAL UINT8 g_VendorSpecificData[VENDOR_SPECIFIC_23_LENGTH] = 
      {
      0,
      0,
      0,
      0x08,
      0x02,
      0x54,
      0x29,
      0x7f,
      0,
      0,
      0x02,
      0
      };

LOCAL UINT8	g_dataInBfr[DATA_BUFFER_LEN];	/* Bulk In buffer */
LOCAL UINT8	g_dataOutBfr[DATA_BUFFER_LEN];	/* Bulk Out buffer */

LOCAL BOOL	g_mediaRemoved   = FALSE;	/* TRUE - if media removed */
LOCAL BOOL	g_mediaReady     = TRUE;	/* TRUE - if media ready */
LOCAL BOOL	g_mediaPrevent   = FALSE;	
LOCAL UINT8	g_pwrConditions  = 0;		/* power conditions */

pVOID g_rbcBlkDev = NULL;


/*******************************************************************************
*
* usbTargRbcSenseDataSet - set sense data
*
* This routine is used locally to set the sense key, additional sense code, 
* and additional sense code qualifier of the current sense data.
*
* RETURNS: OK
* 
* ERRNO:
*  none.
*
* \NOMANUAL
*/

LOCAL STATUS usbTargRbcSenseDataSet
    (
    UINT8	senseKey,	/* the SCSI sense key */
    UINT8	asc,		/* additional sense code */
    UINT8	ascq		/* additional sense code qualifier */
    )
    {
    g_senseData.senseKey = senseKey;
    g_senseData.asc = asc;
    g_senseData.ascq = ascq;

    return(OK);
    }


/********************************************************************************
*
* usbTargRbcRead - read data from the RBC device
*
* This routine reads data from the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO: none
*  none
*/

STATUS usbTargRbcRead
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* pointer to where data will be read by host */
    UINT32	* pSize		/* size of data to be read */
    )
    {
    UINT32      startBlk    = 0;	/* start block */
    UINT16      numBlks     = 0;	/* number of blocks */
    CBIO_DEV_ID cbio;			/* CBIO_DEV_ID */
    STATUS      retVal;			/* status */
    cookie_t    cookie;			/* cookie_t */

    cbio = (CBIO_DEV_ID)usbTargRbcBlockDevGet ();

    /* get starting blk number */

    startBlk = (arg[2] << 24) | (arg[3] << 16)| (arg[4] << 8) | arg[5];

    /* get number of blks */

    numBlks = (arg[7] << 8) | arg[8];

    /* perform blk read */

    retVal = cbioBlkRW (cbio, startBlk, numBlks, (addr_t)&g_dataInBfr[0],    
                        CBIO_READ, &cookie);

    *ppData = &g_dataInBfr[0];
    *pSize  = BYTES_PER_BLOCK * numBlks;

    if (retVal == ERROR)
        {
        usbDbgPrint ("usbTargRbcRead: cbioBlkRW returned ERROR\n");
        return(ERROR);
        }

    /* set sense data to no sense */

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE,SCSI_ADSENSE_NO_SENSE,0x0);
    return(OK);
    }


/******************************************************************************
*
* usbTargRbcCapacityRead - read the capacity of the RBC device
*
* This routine reads the capacity of the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO: 
*  none.
*/
STATUS usbTargRbcCapacityRead
    (
    UINT8	arg[10],	/* RBC command */
    UINT8	**ppData,	/* point to capacity data */
    UINT32	*pSize		/* size of capacity */
    )
    {
    if (!g_mediaRemoved)
        {
        *ppData = (UINT8 *)&g_capacityData;
        *pSize  = sizeof(CAPACITY_DATA);

        /* set sense data to no sense */
 
       usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE,SCSI_ADSENSE_NO_SENSE,0x0);
        }
    else
        {
        *ppData = NULL;
        *pSize = 0;

        /* 
         * set sense key to NOT READY (02h), and an ASC of LOGICAL UNIT 
         * NOT READY (04h). set ASCQ to cause not reportable (0x0)  
         */
        usbTargRbcSenseDataSet (SCSI_SENSE_NOT_READY, 
                                SCSI_ADSENSE_LUN_NOT_READY, 0x0);

        return(ERROR);
        }

    return(OK);
    }

/*******************************************************************************
*
* usbTargRbcStartStop - start or stop the RBC device
*
* This routine starts or stops the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcStartStop
     (
     UINT8	arg[6]		/* the RBC command */
     )
     {
     UINT8	pwrConditions = arg[4] & 0xf0;	/* power conditions */
     UINT8	loEj  = arg[4] & 0x2;		/* load eject */
     UINT8	start = arg[4] & 0x1;		/* enable or disable media */
						/* access operations */

    if (pwrConditions != 0)
        {
        switch (pwrConditions)
            {
            case 1:

                /* Place device in Active condition */

                break;

            case 2:

                /* Place device in Idle condition */

                break;

            case 3:

                /* Place device in Standby condition */

                break;

            case 5:

                /* Place device in Sleep condition */

                break;

            case 7: 

               /* optional control */

            default:
                break;
            }

        /*
         * Save power conditions:
         * NOTE: The device shall terminate any command received that requires
         * more power consumption than allowed by the START STOP UNIT command's
         * most recent power condition setting. Status shall be ERROR, the sense
         * key to ILLEGAL REQUEST (05h), and the ASC/ASCQ to LOW POWER CONDITION
         * ACTIVE (5Eh/00h). It is not an error to request a device be placed 
         * into the same power consumption level in which it currently resides.
         */

        g_pwrConditions = pwrConditions;
        }
    else
        {

        /*
         * If a removable medium device, in either PREVENT state 01b or 11b,
         * receives a START STOP UNIT command with the POWER CONDITIONS field
         * set to the Sleep state (5), this routine will return ERROR with
         * the sense key set to ILLEGAL REQUEST (05h), and the ASC/ASCQ to
         * ILLEGAL POWER CONDITION REQUEST (2Ch/05h). 
         */

        if (g_pwrConditions == 0x5)
            {
            if (g_mediaPrevent == 0x1 || g_mediaPrevent == 0x3)
                {
                usbTargRbcSenseDataSet(SCSI_SENSE_ILLEGAL_REQUEST,
                                       SCSI_ADSENSE_CMDSEQ_ERROR,
                                  SCSI_SENSEQ_ILLEGAL_POWER_CONDITION_REQUEST);
                return(ERROR);
                }
            }

        /* either load or eject the media */

        if (loEj == 1) 
            {
            if (start == 1)
                {

                /* load medium: set media removed flag to loaded */

                g_mediaRemoved = FALSE;
                }
            else
                {

                /* unload medium: set media removed flag to ejected */

                g_mediaRemoved = TRUE;
                }
            }
        else 
           
            /* get media ready for data transfer */
   
            {
            if (start == 1)

                /* media ready for data transfers */

                g_mediaReady = TRUE;
            else

                /* media inaccessible for data transfers */

                g_mediaReady = FALSE;
            }
        }

    /* set sense data to no sense */

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0);

    return(OK);
    }

/*******************************************************************************
*
* usbTargRbcPreventAllowRemoval - prevent or allow the removal of the RBC device
*
* This routine prevents or allows the removal of the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcPreventAllowRemoval
    (
    UINT8	arg[6]			/* the RBC command */
    )
    {
    UINT8	prevent = arg[4] & 0x3;	/* prevent media removal flag */

    switch (prevent)
        {
        case 0:
 
           /* 
            * Medium removal shall be allowed from both the data transport
            * element and the attached medium changer (if any) 
            */

            break;

        case 1:

            /*
             * Medium removal shall be prohibited from the data transport
             * element but allowed from the attached medium changer (if any) 
             */

            break;

        case 2:

            /*
             * Medium removal shall be allowed for the data transport element
             * but prohibited for the attached medium changer. 
             */

            break;

        case 3:

            /* 
             * Medium removal shall be prohibited for both the data transport
             * element and the attached medium changer.
             */

            break;

        default:

            /* 
             * set the sense key to ILLEGAL REQUEST (0x5) and set the ASC to
             * INVALID COMMAND OPERATION CODE (0x20) 
             */

            usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                                    SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
            return(ERROR);
        }

    /* save prevent/allow flag for use in usbTargRbcStartStop() cmd */

    g_mediaPrevent = prevent;

    /* set sense data to no sense */

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE,0);

    return(OK);
    }


/*******************************************************************************
*
* usbTargRbcVerify - verify the last data written to the RBC device
*
* This routine verifies the last data written to the RBC block I/O device.
*
* RETURNS: OK or ERROR.
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcVerify
    (
    UINT8	arg[10]		/* the RBC command */
    )
    {

    /* 
     * Assume last WRITE command was good.
     * set sense key to NO SENSE (0x0),
     * set additional sense code to NO ADDITIONAL SENSE INFORMATION 
     */

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE,0);

    return(OK);
    }

/*******************************************************************************
*
* usbTargRbcWrite - write to the RBC device
*
* This routine writes to the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcWrite
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* location where data will be written to device */
    UINT32	*pSize		/* size of location on device */ 
    )
    {
    UINT32	startLBA = 0;	/* logical block address */
    UINT16	xferLgth = 0;	/* total length to write */

    /* save starting LBA from arg[2] - arg[5]> save transfer length */

    startLBA = (arg[2] << 24) | (arg[3] << 16) | (arg[4] << 8) | arg[5];
    xferLgth = (arg[7] << 8) | arg[8];

    /* pointer to where data is to be written */

    *ppData = &g_dataOutBfr[0];

    if ((xferLgth * BYTES_PER_BLOCK) < DATA_BUFFER_LEN)
        *pSize = xferLgth * BYTES_PER_BLOCK;
    else
        *pSize = DATA_BUFFER_LEN;

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0x0);
    return(OK);
    }

/******************************************************************************
*
* usbTargRbcInquiry - retrieve inquiry data from the RBC device
*
* This routine retrieves inquiry data from the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcInquiry
    (
    UINT8	cmd[6],		/* the RBC command */
    UINT8	**ppData,	/* location of inquiry data on device */
    UINT32	*pSize		/* size of inquiry data on device */
    )
    {
    UINT8	evpd      = cmd[1] & 0x1; /* enable vital product data bit */
    UINT8	cmdDt     = cmd[1] & 0x2; /* command support data bit */
    UINT8	pageCode  = cmd[2];       /* page code */
    UINT8	allocLgth = cmd[4];	  /* number of bytes of inquiry */	

    *ppData = NULL;
    *pSize = 0;

    if (evpd == 1)
        {

        /* return vital product data */

        switch(pageCode)
            {
            case VPD_SUPPORTED_PAGE_CODE:

                /* return supported vital product data page codes: */

                *ppData = (UINT8 *)&g_vpdSupportedPage;
                *pSize  = sizeof(VPD_SUPPORTED_PAGE);
                break;

            case VPD_UNIT_SERIAL_NUM_PAGE_CODE:

                /* transmit vpdSerialNumPage */

                *ppData = (UINT8 *)&g_vpdSerialNumPage;
                *pSize  = sizeof(VPD_UNIT_SERIAL_NUM_PAGE);
                break;

            case VPD_DEVICE_ID_PAGE_CODE:

                /* transmit vpdDevIdPage */

                *ppData = (UINT8 *)&g_vpdDevIdPage;
                *pSize  = sizeof(VPD_DEVICE_ID_PAGE);
                break;

            default:

                /*
                 * return ERROR status; set the sense key set to 
                 * ILLEGAL REQUEST and set the additional sense code
                 * of INVALID FIELD IN CDB. 
                 */

                usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                                        SCSI_ADSENSE_INVALID_CDB,0);
                return(ERROR);            
            }
        }
    else if (cmdDt == 1)
        {

        /*
         * return command support data. This implementation is optional; 
         * therefore, the data returned will contain the peripheral qualifier, 
         * device type byte, and 001b entered in the SUPPORT field.
         */

        /* transmit cmdData */

        *ppData = (UINT8 *)&g_cmdData;
        *pSize  = sizeof(COMMAND_DATA);
        }
    else if (pageCode == 0)
        {

        /* return standard inquiry data must be at least 36 bytes */

        *ppData = (UINT8 *)&g_stdInquiryData;
        *pSize  = sizeof(STD_INQUIRY_DATA);
        }
    else
        {

        /*
         * return ERROR status with the sense key set to 
         * ILLEGAL REQUEST and an additional sense code of 
         * INVALID FIELD IN CDB. 
         */

        usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                                SCSI_ADSENSE_INVALID_CDB, 0);
        return(ERROR);
        }

    /* return only what was requested */

    if (allocLgth < *pSize)
        *pSize = allocLgth;

    usbTargRbcSenseDataSet (SCSI_SENSE_NO_SENSE, 
                            SCSI_ADSENSE_NO_SENSE, 0x0);
    
    return(OK);
    }


/*******************************************************************************
*
* usbTargRbcModeSelect - select the mode parameter page of the RBC device
*
* This routine selects the mode parameter page of the RBC block I/O device.
* For non-removable medium devices the SAVE PAGES (SP) bit shall be set to one. 
* This indicates that the device shall perform the specified MODE SELECT 
* operation and shall save, to a non-volatile vendor-specific location, all the
* changeable pages, including any sent with the command. Application clients 
* should issue MODE SENSE(6) prior to each MODE SELECT(6) to determine supported
* pages, page lengths, and other parameters.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcModeSelect
    (
    UINT8	arg[6],		/* the RBC command */
    UINT8	** ppData,	/* location of mode parameter data on device */
    UINT32	* pSize		/* size of mode parameter data on device */
    )
    {
    UINT8	pgfmt         = arg[0] & 0x10;	/* page format */
    UINT8	savePgs       = arg[0] & 0x1;	/* save pages */
    UINT8	paramLstLgth  = arg[4];		/* parameter list length */

    /*
     * The removable media bit (RMB) in the stdInquiry data
     * page is one; therefore, support for the save pages bit
     * is optional 
     */

    if (pgfmt == 1 && savePgs == 0)
        {
        *ppData = (UINT8 *)&g_deviceParamList;
        *pSize = sizeof(MODE_PARAMETER_LIST);

        /*
         * return ERROR if paramLstLgth truncates any of the mode 
         * parameter list (header and mode page). Set sense key to
         * ILLEGAL REQUEST, and ASC to PARAMETER LIST LENGTH ERROR.
         */

        if (paramLstLgth < *pSize)
            {
            usbTargRbcSenseDataSet(SCSI_SENSE_ILLEGAL_REQUEST,
                                   SCSI_PARAMETER_LIST_LENGTH_ERROR, 0);
            return(ERROR);
            }
        }
    else
        {

        /*
         * set sense key to ILLEGAL REQUEST (05h) and an ASC of INVALID FIELD 
         * IN CDB (24h).
         */

        usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST, 
                                SCSI_ADSENSE_INVALID_CDB, 0);
        return(ERROR);
        }


    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE,0);
    return(OK);
    };


/*******************************************************************************
*
* usbTargRbcModeSense - retrieve sense data from the RBC device
*
* This routine retrieves sense data from the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcModeSense
    (
    UINT8	arg[6],		/* the RBC command */
    UINT8	** ppData,	/* location mode parameter data on device */
    UINT32	* pSize		/* size of mode parameter data on device */
    )
    {

    UINT8	pgCtrl	   = arg[2] & 0xc;	/* page control feild */
    UINT8	pgCode     = arg[2] & 0x3f;	/* page code */
    UINT8	allocLgth  = arg[4];		/* allocation length */

    *ppData = NULL;
    *pSize = 0;

    if (pgCode == RBC_MODE_PARAMETER_PAGE)
        {
        switch (pgCtrl)
            {
            case 0: /* return current values */
            case 2: /* return default values */
                *ppData = (UINT8 *)&g_deviceParamList;
                *pSize = sizeof(MODE_PARAMETER_LIST);
                break;

            case 1: 
                /* return changeable values */

                /*
                 * return a mask where the bit fields of the mode parameters 
                 * that are changeable shall are one; otherwise, zero. 
                 */

                *ppData = (UINT8 *)&g_deviceParamListMask;
                *pSize = sizeof(MODE_PARAMETER_LIST);
                break;

            case 3:
                /* saved values are not implemented */
                /*
                 * termine with ERROR status, 
                 * set sense key set to ILLEGAL REQUEST
                 * set additional sense code set to SAVING PARAMETERS
                 * 
                 * NOT SUPPORTED. 
                 */

                usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                                        SCSI_ADSENSE_SAVE_ERROR, 0);
                return(ERROR);

            default: 
                /* error */
                /*
                 * terminate with ERROR status, 
                 * set sense key set to ILLEGAL REQUEST
                 * set additional sense code set to INVALID FIELD IN CDB. 
                 */

                usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                                        SCSI_ADSENSE_INVALID_CDB, 0);
                return(ERROR);
            }
        }
    else
        {

        /* If not an RBC device, return just the header with no page info */

        g_deviceHdr.dataLen = sizeof(MODE_PARAMETER_HEADER)-1;
        *ppData = (UINT8 *)&g_deviceHdr;
        *pSize = sizeof(MODE_PARAMETER_HEADER);
        }

    /* return only what was requested */

    if (allocLgth < *pSize)
        *pSize = allocLgth;

    /* set sense data */

    usbTargRbcSenseDataSet (SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0);

    return(OK);
    }


/*******************************************************************************
*
* usbTargRbcTestUnitReady - test if the RBC device is ready
*
* This routine tests whether the RBC block I/O device is ready.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcTestUnitReady
    (
    UINT8	arg[6]		/* the RBC command */
    )
    {

    /*
     * set sense key to NO SENSE (0x0) and set additional sense code
     * to NO ADDITIONAL SENSE INFORMATION 
     */

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE,SCSI_ADSENSE_NO_SENSE,0);

    return(OK);
    }


/******************************************************************************
*
* usbTargRbcBufferWrite - write micro-code to the RBC device
*
* This routine writes micro-code to the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcBufferWrite
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* micro-code location on device */
    UINT32	* pSize		/* size of micro-code location on device */
    )
    {
    UINT32	offset;		/* offset */
    UINT32	paramListLgth;	/* paramter list length */
    UINT8	mode;		/* mode */

    mode = arg[1] & 0x7;
    offset = (arg[3] << 16) | (arg[4] << 8) | arg[5];
    paramListLgth = (arg[6] << 16) | (arg[7] << 8) | arg[8];

    switch(mode)
        {
        case 5:

            /* 
             * vendor-specific Microcode or control information is 
             * transferred to the device and saved saved to 
             * non-volatile memory 
             */ 

            break;

        case 7:

            /*
             * vendor-specific microcode or control information is
             * transferred to the device with two or more WRITE BUFFER commands. 
             */

            break;

        default: 

            /* other modes optional for RBC devices and not implemented */

            /* 
             * set sense key to ILLEGAL REQUEST (05h) and an ASC of COMMAND
             * SEQUENCE ERROR (2Ch) 
             */

            usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST, 
                                    SCSI_ADSENSE_CMDSEQ_ERROR, 0x0);
            return(ERROR);
        }

    /*
     * The PARAMETER LIST LENGTH field specifies the maximum number of bytes 
     * that sent by the host to be stored in the specified buffer beginning
     * at the buffer offset. If the BUFFER OFFSET and PARAMETER LIST LENGTH
     * fields specify a transfer in excess of the buffer capacity, return 
     * ERROR and set the sense key to ILLEGAL REQUEST with an additional 
     * sense code of INVALID FIELD IN CDB. 
     */

    if (paramListLgth + offset > DATA_BUFFER_LEN)
        {
        usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST, 
                                SCSI_ADSENSE_INVALID_CDB, 0x0);
        return(ERROR);
        }

    usbTargRbcSenseDataSet(SCSI_SENSE_NO_SENSE, SCSI_ADSENSE_NO_SENSE, 0x0);

    *ppData = &g_dataOutBfr[0];
    *pSize = DATA_BUFFER_LEN;
    return(OK);
    }


/* optional routines */

/*******************************************************************************
*
* usbTargRbcFormat - format the RBC device
*
* This routine formats the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcFormat
    (
    UINT8	arg[6]		/* the RBC command */
    )

    {
    /* 
     * This routine should not be called because the mode sense data indicates
     * the unit cannot be formatted. If this routine is called, return a status
     * of ERROR, a sense key of MEDIA ERROR (03h), an ASC/ASCQ
     * of FORMAT COMMAND FAILED (31h /01h). 
     */
    usbTargRbcSenseDataSet (SCSI_SENSE_MEDIUM_ERROR, SCSI_ADSENSE_FORMAT_ERROR,
                            0x1);
    return(ERROR);
    }

/*******************************************************************************
*
* usbTargRbcPersistentReserveIn - send reserve data to the host
*
* This routine requests reserve data to be sent to the initiator.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcPersistentReserveIn
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* location of reserve data on device */
    UINT32	*pSize		/* size of reserve data */
    )
    {
    *ppData = NULL;
    *pSize  = 0;

    /*
     * This routine is optional and is not implemented. If this routine is called,
     * return a status of ERROR, a sense key of ILLEGAL REQUEST (05h),
     * and an ASC/ASCQ of INVALID COMMAND OPERATION CODE (20h). 
     */

    usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST, 
                            SCSI_ADSENSE_ILLEGAL_COMMAND, 0x0);

    return(ERROR);
    }

/*******************************************************************************
*
* usbTargRbcPersistentReserveOut - reserve resources on the RBC device
*
* This routine reserves resources on the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcPersistentReserveOut
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* location of reserve data on device */
    UINT32	*pSize		/* size of reserve data */
    )
    {
    *ppData = NULL;
    *pSize = 0;

    /*
     * This routine is optional and is not implemented. If this routine is 
     * called, return a status of ERROR, a sense key of ILLEGAL REQUEST (05h),
     * and an ASC/ASCQ of INVALID COMMAND OPERATION CODE (20h).
     */

    usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                            SCSI_ADSENSE_ILLEGAL_COMMAND, 0x0);

    return(ERROR);
    }

/*******************************************************************************
*
* usbTargRbcRelease - release a resource on the RBC device
*
* This routine releases a resource on the RBC block I/O device.
*
* RETURNS: OK or ERROR
*
* ERRNO:
*  none.
*/

STATUS usbTargRbcRelease
    (
    UINT8	arg[6]		/* the RBC command */
    )
    {

    /*
     * This routine is optional and is not implemented. If this routine is 
     * called, return a status of ERROR, a sense key of ILLEGAL REQUEST (05h),
     * and an ASC/ASCQ of INVALID COMMAND OPERATION CODE (20h).
     */

    usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                            SCSI_ADSENSE_ILLEGAL_COMMAND, 0x0);

    return(ERROR);
    }


/*******************************************************************************
*
* usbTargRbcRequestSense - request sense data from the RBC device
*
* This routine requests sense data from the RBC block I/O device.
*
* RETURNS: OK or ERROR
*
* ERRNO: N/A
*/

STATUS usbTargRbcRequestSense
    (
    UINT8	arg[6],		/* the RBC command */
    UINT8	** ppData,	/* location of sense data on device */
    UINT32	*pSize		/* size of sense data */
    )
    {
    UINT8	allocLgth = arg[4];	/* allocation length */

    *ppData = (UINT8 *)&g_senseData;
    *pSize  = sizeof(SENSE_DATA);

    if (allocLgth < *pSize)
        *pSize = allocLgth;

    return(OK);
    }

/*******************************************************************************
*
* usbTargRbcReserve - reserve a resource on the RBC device
*
* This routine reserves a resource on the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcReserve
    (
    UINT8	arg[6]		/* the RBC command */
    )
    {
    /*
     * set sense key to ILLEGAL REQUEST 0x5 and 
     * set additional sense code to INVALID COMMAND OPERATION CODE 0x20 
     */

    usbTargRbcSenseDataSet (SCSI_SENSE_ILLEGAL_REQUEST,
                            SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
    return(ERROR);
    }

/******************************************************************************
*
* usbTargRbcCacheSync - synchronize the cache of the RBC device
*
* This routine synchronizes the cache of the RBC block I/O device.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcCacheSync
    (
    UINT8	arg[10]		/* the RBC command */
    )
    {
    usbTargRbcSenseDataSet (SCSI_SENSE_NO_SENSE,
                            SCSI_ADSENSE_NO_SENSE, 0);
    return(OK);
    }

/*******************************************************************************
*
* usbTargRbcBlockDevGet - return opaque pointer to the RBC BLK I/O DEV device 
* structure.
*
* This routine returns an opaque pointer to the RBC BLK I/O DEV device 
* structure.
*
* RETURNS: Pointer to the RBC BLK I/O DEV structure
* 
* ERRNO:
*  none
*/

pVOID usbTargRbcBlockDevGet (void)
    {

    /* return current instance of the RBC BLK_DEV structure */

    return(g_rbcBlkDev);
    }


/*******************************************************************************
*
* usbTargRbcBlockDevSet - set the pointer to the RBC BLK I/O DEV device structure.
*
* This routine sets the RBC BLK_DEV pointer that is accessed by the 
* usbTargRbcBlockDevGet() routine.
*
* RETURNS: OK or ERROR
* 
* ERRNO:
*  none
*/

STATUS usbTargRbcBlockDevSet
    (
    pVOID	*blkDev		/* pointer to the BLK_DEV device */
    )
    {

    /* set the RBC BLK_DEV pointer to the value of the argument */

    g_rbcBlkDev = blkDev;
    return(OK);
    }


/******************************************************************************
*
* usbTargRbcBlockDevCreate - create an RBC BLK_DEV device.
*
* This routine creates an RBC BLK I/O device. The RAM driver will be used for
* the actual implementation.
*
* RETURNS: Pointer to BLK_DEV structure
* 
* ERRNO:
*  none.
*/

STATUS usbTargRbcBlockDevCreate (void)
    {
    CBIO_DEV_ID		cbio;	/* CBIO_DEV_ID */

    if (g_rbcBlkDev != NULL)
        return(OK);

#if (USB_MS_BLK_DRV == USB_MS_CBIO_DRV)

    /* create a RAM disk cache block I/O device (CBIO) */

    cbio = (CBIO_DEV_ID)ramDiskDevCreate
        (
        NULL,               /* where it is in memory (0 = malloc)     */
        BYTES_PER_BLOCK,    /* number of bytes per block              */
        BLKS_PER_TRACK,     /* number of blocks per track             */
        NUM_BLOCKS,         /* number of blocks on this device        */
        BLK_OFFSET          /* no. of blks to skip at start of device */
        );

    if (cbio == NULL)
        {
        usbDbgPrint("usbTargRbcBlockDevCreate: ramDiskDevCreate returned \
                     NULL\n");
        return(ERROR);
        }


    /* create the DOS file system */

    if (dosFsDevCreate (BLK_DEV_NAME, cbio, 0x20, NONE) == ERROR)
        {
        usbDbgPrint ("usbTargRbcFileSystemCreate: dosFsDevCreate \
                      returned ERROR\n");
        return ERROR;
        }

    /* Format the first partition */  

    if(dosFsVolFormat (BLK_DEV_NAME, 2,0) == ERROR)
        {
        usbDbgPrint ("usbTargRbcBlockDevCreate: dosFsVolFormat returned \
                      ERROR\n");
        return ERROR;
        }


#elif (USB_MS_BLK_DRV == USB_MS_RAM_DRV)

    /* create a RAM driver and use CBIO wrapper routines */        

    cbio = cbioWrapBlkDev (ramDevCreate (NULL, BYTES_PER_BLOCK, BLKS_PER_TRACK,	
                                         NUM_BLOCKS, BLK_OFFSET));
    if (cbio == NULL)
        {
        usbDbgPrint("usbTargRbcBlockDevCreate: ramDevCreate returned NULL\n");
        return(ERROR);
        }

    if(cbioIoctl(cbio,CBIO_RESET,NULL) != OK)
        {
        usbDbgPrint ("usbTargRbcBlockDevCreate: CBIO_RESET returned ERROR\n");
        return ERROR;
        }

#else

#error usbTargRbcBlockDevCreate: USB_MS_BLK_DRV is an unknown device

#endif

    /* reset the device */

    if(cbioIoctl(cbio,CBIO_RESET,NULL) != OK)
        {
        usbDbgPrint ("usbTargRbcBlockDevCreate: CBIO_RESET returned ERROR\n");
        return ERROR;
        }

    /* save the pointer */

    g_rbcBlkDev = (pVOID)cbio;

    return(OK);
    }


/******************************************************************************
*
* usbTargRbcVendorSpecific - vendor specific call
*
* This routine is a vendor specific call.
*
* RETURNS: OK
* 
* ERRNO:
*  none
*/
STATUS usbTargRbcVendorSpecific
    (
    UINT8	arg[10],	/* the RBC command */
    UINT8	** ppData,	/* location of sense data on device */
    UINT32	*  pSize	/* size of sense data */
    )
    {
    *ppData = g_VendorSpecificData;
    *pSize = VENDOR_SPECIFIC_23_LENGTH;

    return(OK);
    }

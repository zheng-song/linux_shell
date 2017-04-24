// the underlying serial driver registered with TTY middle-tier data structure is SIO_CHAN.
// the SIO_CHAN structure is defined in sioLib.h like below:
typedef struct sio_drv_funcs SIO_DRV_FUNCS;

typedef struct sio_chan
{
	SIO_DRV_FUNCS *pDrvFuncs;
	// device data
}SIO_CHAN;

struct sio_drv_funcs
{
	int (*ioctl)			(SIO_CHAN *pSioChan);
	int (*txStartUp)		(SIO_CHAN *pSioChan);
	int (*callbackInstall)	(SIO_CHAN *pSioChan,
		int callbackType,STATUS (*callback)(void *,...) (void *callbackArg));
	int (*pollInput)		(SIO_CHAN *pSioChan,char *inChar);
	int (*pollOutput)		(SIO_CHAN *pSioChan,char outChar);
};


typedef volatile unsigned char ARMREG
typedef struct _CSL_UART_S
{
	ARMREG RBR;
	ARMREG IER;
	ARMREG IIR;
	ARMREG LCR;
	ARMREG MCR;
	ARMREG LSR;
	ARMREG MSR;
	ARMREG SCR;
	ARMREG DLL;
	ARMREG DLH;
	ARMREG DLH;
	ARMREG PID1;
	ARMREG PID2;
	ARMREG PWREMU_MGMT;
}CSL_UART_S;

#define THR RBR;		//RBR & THR have same offset
#define FCR IIR;		// IIR and FCR have same offset

typedef struct _ARM926_CHAN
{
	SIO_CHAN 		sio;
	STATUS 			(*getTxChar)();
	STATUS 			(*putRcvChar)();
	void 			*getTxArg;
	void 			* putRcvArg;
	//register address,serial device hardware register base address
	CSL_UART_S 		*regs;
	UINT32 			level;   		// interrupt Level for this device
	UINT32 			clkdiv;
	UINT32 			options;		//hardware options
	int 			intrmode		//current mode(interrupt or poll)
	int 			frequency;		//input clock frequency
	UINT32			errcount;
}ARM926_CHAN;




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




// TTY initialization
#ifdef INCLUDE_TTY_DEV
if (NUM_TTY > 0){
	ttyDrv();
}

for (ix = 0; ix< NUM_TTY; ix++)
{
#if (defined(INCLUDE_WDB)  && (WDB_COMM_TYPE == WDB_COMM_SERIAL))	
	if (ix == WDB_TTY_CHANNEL)		//don't use WDBs channel
		continue;
#endif
	sprintf(tyName,"%s%d","tyCo/",ix);
	(void)ttyDevCreate(tyName,sysSerialChanGet(ix),512,512);
	if (ix == CONSOLE_TTY)		//init the tty console
	{
		strcpy(consoleName,tyName);
		consoleFd = open(consoleName,O_RDWR,0);
		(void)ioctl(consoleFd,FIOBAUDRATE,CONSOLE_BAUD_RATE);
		(void)ioctl(consoleFd,FIOSETOPTIONS,OPT_TERMINAL);
	}
}
#endif


STATUS ttyDevCreate(char * name, SIO_CHAN *pSioChan, int rdBufSize, int wrtBufSize);

/*sysSerial.c Template File*/
＃define MAX_SIOS
SIO_CHAN *sysSioChans[MAX_SIOS];
/*definitions*/
/*Defines for template SIO #0*/
#define TEMPL_SIO_BASE0		0x00100000		//base addr for device 0
#define TEMPL_SIO_VEC0		0x05			//base vector for device 0
#define TEMPL_SIO_LVL0		TEMPL_SIO_VEC0	//level 5
#define TEMPL_SIO_FREQ0		3000000			//clk freq is 3 MHz

void sysSerialHwInit(void){
	// TODO
}

void sysSerialHwInit2(void){
	SIO_CHAN *pChan;
	// TODO
	pChan = templateSioCreate(TEMPL_SIO_BASE0, TEMPL_SIO_VEC0,
		TEMPL_SIO_LVL0,TEMPL_SIO_FREQ0);
	sysSioChans[0] = pChan;
}

SIO_CHAN *sysSerialChanGet(int channel){
	if (channel < 0 || channel >= NELEMENTS(sysSioChans)){
		return (SIO_CHAN *) ERROR;
	}

	return sysSioChans[channel];
}

/*device initialization structure*/
typedef struct 
{
	UINT32		vector;
	CSL_UART_S 	*baseAdrs;
	UINT32		intLevel;
	UINT32		clkdiv;
}SYS_ARM926_CHAN_PARAS;

LOCAL SYS_ARM926_CHAN_PARAS devParas[]={
	{INUM_TO_IVEC(INT_UARTINT0),(CSL_UART_S *)UART0_BASE_ADDR,INT_UARTINT0,1},
	{INUM_TO_IVEC(INT_UARTINT1),(CSL_UART_S *)UART1_BASE_ADDR,INT_UARTINT1,1},
};

LOCAL ARM926_CHAN arm926UartChan[N_SIO_CHANNELS];

SIO_CHAN *sysSioChans[]={
	&arm926UartChan[0].sio,		/*　/tyCo/0　*/
	&arm926UartChan[1].sio,		/*　/tyCo/1 */	
}

void sysSerialHwInit(void)
{
	int i;
	for (i = 0; i < N_SIO_CHANNELS; ++i){
		arm926UartChan[i].regs = devParas[i].baseAdrs;
		arm926UartChan[i].baudRate = CONSOLE_BAUD_RATE;
		arm926UartChan[i].clkdiv = devParas[i].clkdiv;
		arm926UartChan[i].level = devParas[i].intLevel;
		arm926UartInit(&arm926UartChan[i]);
	}
}

void sysSerialHwInit2(void)
{
	for (int i = 0; i < N_SIO_CHANNELS; ++i){
		(void)intConnect(INUM_TO_IVEC(devParas[i].vector),
			arm926UartInt,(UINT32)&arm926UartChan[i]);
		intEnable(devParas[i].intLevel);
		arm926UartInit2(&arm926UartChan[i]);
	}
}

SIO_CHAN *sysSerialChanGet(int channel)
{
	if (channel < 0 || channel >= (int)(NELEMENTS(sysSioChans))){
		return (SIO_CHAN *)ERROR;
	}
	return sysSioChans[channel];
}

 
 LOCAL SIO_DRV_FUNCS arm926UartDrvFuncs={
 	arm926UartIoctl,
 	arm926UartTxStartUp,
 	arm926UartCallbackInstall,
 	arm926UartPollInput,
 	arm926UartPollOutput,
 };

 LOCAL STATUS dummyCallback(void)
 {
 	return (ERROR);
 }

 void arm926UartInit(ARM926_CHAN *pChan)
 {
 	UINT32		dummy;
 	//initialize each channel's driver function pointers
 	pChan->sio.pDrvFuncs = &arm926UartDrvFuncs;
 	//install dummy driver callback
 	pChan->getTxChar = dummyCallback;
 	pChan->putRcvArg = dummyCallback;
 	//reset the chip
 	CSL_uartReset(pChan);
 	CSL_uartConfig(pChan);
 	pChan->intrmode = FALSE;
 }

 void arm 926UartInit2(ARM926_CHAN *pChan)
 {
 	return ;
 }



 LOCAL int arm926UartCallbackInstall(SIO_CHAN *pSioChan,
 	int callbackType,STATUS (*callback)(),void *callbackArg)
 {
 	ARM926_CHAN *pChan = (ARM926_CHAN *)pSioChan;
 	switch(callbackType){
 		case SIO_CALLBACK_GET_TX_CHAR:
 		pChan->getTxChar = callback;
 		pChan->getTxArg = callbackArg;
 		return OK;

 		case SIO_CALLBACK_PUT_RCV_CHAR:
 		pChan->putRcvChar = callback;
 		pChan->putRcvArg = callbackArg;
 		return OK;

 		default:
 		return (ENOSYS);

 	}
 }


 LOCAL int arm926UartIoctl(SIO_CHAN *pSioChan,
 	int request, void *someArg)
 {
 	ARM926_CHAN *pChan=(ARM926_CHAN *)pSioChan;
 	int oldLevel;//current interrupt level mask
 	int arg =(int)someArg;

 	switch(request){
 		case SIO_BAUD_SET:
 		if (arg == 0){
 			return arm926UartHup(pChan);
 		//like unix a request of 0 is hangup
 		}
 		if (arg < ARM926UART_BAUD_MIN || arg > ARM926UART_BAUD_MAX){
 			return EIO;
 		}
 		//check the baud rate constant for the new baud rate
 		switch(arg){
 			case 1200:
 			case 2400:
 			case 4800:
 			case 9600:
 			case 19200:
 			case 38400:
 			case 57600:
 			case 115200:
 			case 230400:
 			case 460800:
 			return (arm926UartSetNewBaudRate(pChan,arg));
 			default:
 		}
 		return OK;

 		case SIO_BAUD_GET:
 		*(int *)arg = pChan->baudRate;
 		return OK;

 		case SIO_MODE_SET:
 		return (arm926UartModeSet(pChan,arg));

 		case SIO_MODE_GET:
 		if (pChan->intrmode){
 			*(int *)arg = SIO_MODE_INT;
 		}else{
 			*(int *)arg = SIO_MODE_POLL;
 		}
 		return OK;

 		case SIO_AVAIL_MODES_GET:
 		*(int *)arg = SIO_MODE_INT|SIO_MODE_POLL;
 		return OK;	

 		case SIO_HW_OPTS_SET:
 		case SIO_HW_OPTS_GET:
 		case SIO_OPEN:
 		case SIO_HUP:
 		default:
 		return (ENOSYS);
 	}
 	return (ENOSYS);
 }


 void arm926UartInt(ARM926_CHAN *pChan/*channel generating the interrupt*/)
 {
 	char 	rByte;
 	UINT32 	status;
 	CSL_UART_S 	*udev = pChan->regs;
 	status = udev->LSR;
 	//error interrupt
 	if (status&(CSL_UART_LSR_RXFIFOE_MASK|CSL_UART_LSR_FE_MASK|
 		CSL_UART_LSR_OE_MASK|CSL_UART_LSR_PE_MASK)){
 		pChan->errcount++;
 	CSL_uartReset(pChan);
 	CSL_uartConfig(pChan);
 	CSL_uartEnable(pChan);
 	}
// data receive interrupt
 	while(status&(CSL_UART_LSR_DR_MASK)){
 		CSL_FINSR(uartRegs->LCR,7,7,0);
 		rByte = CSL_FEXT(uartRegs->RBR,USRT_RBR_DATA);
 		(*pChan->putRcvChar)(pChan->putRcvArg,rByte);
 		status = udev->LSR;//get status again to check
 	}
 }
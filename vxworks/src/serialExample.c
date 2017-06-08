/*
 * usb2serial.c
 *
 *  Created on: 2017-4-18
 *      Author: sfh
 */

#include <vxWorks.h>
#include <tasklib.h>
#include <syslib.h>
#include <logLib.h>
#include <vmlib.h>
#include <sioLib.h>
#include <iv.h>
#include <ioLib.h>
#include <iosLib.h>
#include <tyLib.h>
#include <intLib.h>
#include <errnoLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <selectLib.h>


/*
typedef struct
{
	DEV_HDR		u2sDevHdr;
	UINT32 		u2sregBase; 	//设备寄存器基地址
	UINT32 		u2sDevbuf;  	//数据缓存区基地址
	BOOL 		u2sisOpen;      //设备已打开标志
	UINT8 		u2sintLv;		//设备中断号
	char *		u2sputData;		//内核回调函数指针，指向的函数向内核提供数据
	char *		u2sgetdata;		//内核回调函数指针，指向的函数从内核获取数据
	}u2s_DEV;


STATUS u2sDevCreate(char *devName ,int arg)
{
	u2s_DEV *
	}

*/


#define SIPSPD		（100000000）
GPIO_RED *gpioReg = (GPIOREG *)GPIO_REG_BASE;
CLKPW_REG *gCLKPWReg = (CLKPW_REG *)CLKPW_REG_BASE;


typedef struct
{
DEV_HDR 	pDevHdr;
UINT32		gpioRegBase;	//GPIO控制寄存器基地址
UINT8		channel;		//通道号：指示当前是对哪一个串口进行操作
UINT8		flags;			//保存特定通道的操作类型
}SPI_DEV;


void spi_gpio_init()
{
	//mosi,miso;mosi-output,miso-input
	gCLKPWReg->rCLKCON &=~(1<<18);
	gpioReg->rGPECON = (gpioReg->rGPECON &~(0x3F<<22))|(0x4<<22);
	gpioReg->rGPEUP |=(0x3<<11);		//use external pullup,disable gpio pullup
	gpioReg->rGPEDAT &= ~(0x3<<11);		//initial low
	//clk
	gpioReg->rGPGCON = (gpioReg->rGPGCON & ~(0x3<<4))|(0x1<<4);		//output
	gpioReg->rGPGUP |= (0x1<<2);		//use external pullup,disablegpio pullup
	gpioReg->rGPGDAT &= ~(0x1<<2);		//spi mode 0,sck inactive-high
	//cs
	gpioReg->rGPFCON = (gpioReg->rGPFCON & ~(0x3<<2))|(0x1<<2);		//output
	gpioReg->rGPFUP |= (0x1<<1);		//use external pullup,disable gpio pullup
	gpioReg->rGPFDAT |= (0x1<<1);		//chip select, inactive-high
}

inline void delay(unsigned int t)
{
	int i;
	while(t--){
		i=t-i;		//延时操作，此处的i并没有作用，知识为了不让那个CPU空转？？
	}
}

//GPIO F1用作片选SCS
void scs(int d)
{
	if (d){
		gpioReg->rGPFDAT |= (1<<1);
	}else
	gpioReg->rGPFDAT &= ~(1<<1);
}

//GPIO E12用作 MOSI
void mosi(int i)
{
	if (i){
		gpioReg->rGPEDAT |= (1<<12);
	}else
		gpioReg->rGPEDAT &= ~(1<<12);
}

// GPIO E11 用作MISO
void miso(int i)
{
	return(gpioReg->rGPEDAT >> 11)&0x1;
}

//GPIO G2用作时钟SCK
void clk(int i)
{
	if (i){
		gpioReg->rGPGDAT |= (0x1<<2);
	}else
		gpioReg->rGPGDAT &= ~(0x1<<2);
}

//SPI接口读写函数，每次16-bit
//底层驱动使用spi_bitbang函数对VK3224 SPI从接口进行读写，SPI接口使用两根数据线进行数据传输
//SPI主从接口之间数据的交互是全双工的，一方在写出数据的同时也在读入数据，主从双方构成一个环形寄存器。

unsigned short spi_bitbang(unsigned short dat)
{
	scs(0);
	for (int i = 0; i < 16; ++i){
		mosi(dat&0x8000);
		dat<<=1;
		delay(SPISPD);
		clk(1);
		dat |= miso();
		delay(SPISPD);
		clk(0);
	}
	scs(1);
	return dat;
}

LOCAL int spiDrvNum = -1;
STATUS spiDrv()
{
	if (spiDrvNum != -1){
		return (OK);
	}

	spiDrvNum = iosDrvInstall(spiOpen,spiDelete,spiOpen,spiClose,spiRead,spiWrite,spiIoctl);
	return(spiDrvNum == ERROR?ERROR:OK);
}


char *spiDevNamePrefix="/spiUart";
STATUS spiDevCreate(int channel)
{
	SPI_DEV		*pSpiDev;
	char		devName[256];
	memset(devName,0,sizof(char)*256);
	sprintf(devName,"%s/%d",spiDevNamePrefix, channel);
	pSpiDev = (SPI_DEV *)malloc(sizof(SPI_DEV));
	memset(pSpiDev, 0, sizeof(SPI_DEV));
	pSpiDev->channel = channel;
	pSpiDev->gpioRegBase = (UINT32)GPIO_RED_BASE;
	spi_gpio_init;			//配置GPIO模块 ，即初始化。
//=======================================================
/*
此处因该调用spi_bitbang函数通过SPI接口配置VK3224，初始化由channel指定的通道
*/
//=======================================================

	//将设备添加到系统中
	if (iosDevAdd(&pSpiDev->pDevHdr, devName, spiDrvNum) == ERROR){
		free((char *)pSpiDev);
		return (ERROR);
	}
	return (OK);
}



SPI_DEV *spiOpen(DEV_HDR *pDevHdr,char *name, int flags, int mode)
{
//====================================================================================
/*
	对于我们的SPI底层驱动而言，在主机端直接使用GPIO进行SPI时序的模拟，故我们没有使用中断
所以底层驱动打开函数spiOpen的实现只需要通过SPI接口配置对应的通道寄存器，使能通道工作即可。
而对于通道波特率等参数的配置在spiDrvCreate函数中已经完成。
	第一个参数由IO子系统提供，IO子系统在根据驱动号寻址到对应驱动函数时，其将系统设备列表
中存储的设备结构作为第一个参数调用spiOpen。
	第二个参数是设备名匹配后的剩余的部分，我们此处调用时输入的路径名与系统设备列表中的
设备名完全匹配，故此处的那么应为空字符串
	第三、第四个参数就是open调用时传入的第二、三个参数，IO子系统将其原样传给spiOpen函数
*/
//====================================================================================
	SPI_DEV		*pSpiDev;
	int status = OK;
	pSpiDev = (SPI_DEV *)pDevHdr;
	pSpiDev->flags = flags;

	switch(pSpiDev->channel){
		case 0:
		//调用spi_bitbang函数配置VK3224，使能通道1进行工作。
		break；

		case 1:
		//调用spi_bitbang函数配置VK3224，使能通道2进行工作。
		break；

		case 3:
		//调用spi_bitbang函数配置VK3224，使能通道3进行工作。
		break；
//spiOpen根据设备结构中的通道号（channel）决定对哪一个通道采取措施。底层驱动同时驱动四个通道，
//故SPI_DEV结构中专门定义了一个channel字段用以对各个通道进行分区。
		default:
		printErr("bad channel num:%d",pSpiDev->channel);
		status = ERROR;
		break;
	}
	if (status == OK){
		return pSpiDev;
	}else{
		return (SPI_DEV *)E RROR;
	}
}

#define MAX_WAIT_CYCLE		(1000)
int spiRead(SPI_DEV *pSpiDev,char *buffer,int nbyes)
{
	int dataRdy,cycles = 0;
	int bytesRead = 0;

	if (pSpiDev->flags == O_WRONLY){
		return ERROR;
	}

	switch(pSpiDev->channel){
		case 0:
		while(bytesRead<nbyes&&cycles<MAX_WAIT_CYCLES)
//虽然VK3224每次操作16-bit，但是每次只能读取8-bit有效数据，使用spi_bitbang函数从VK3224通道1读取数据
			dataRdy = spi_bitbang(CHN1_STATUS_RD<<8|0);//检查数据是否就绪。
		if (dataRdy & RD_RDY){//读就绪标志位有效，读取一字节数据
			buffer[bytesRead] = (char)spi_bitbang(CHN1_STATUS_RD<<8|0);
			bytesRead++;
			cycles = 0;
		}else{//尚未就绪，循环等待。
			cycles++;
		}
		break;

		case 1:
		while(bytesRead<nbyes&&cycles<MAX_WAIT_CYCLES)
			dataRdy = spi_bitbang(CHN2_STATUS_RD<<8|0);//检查数据是否就绪。
		if (dataRdy & RD_RDY){//读就绪标志位有效，读取一字节数据
			buffer[bytesRead] = (char)spi_bitbang(CHN2_STATUS_RD<<8|0);
			bytesRead++;
			cycles = 0;
		}else{//尚未就绪，循环等待。
			cycles++;
		}
		break;

		case 2:
		//.....

		case 3:
		//.....

		default:
		printErr("bad channel num:%d\n",pSpiDev->channel);
		bytesRead = -1;
		break;
	}

	return bytesRead;
}


int spiWrite(SPI_DEV *pSpiDevspi, char *buffer, int nbytes)
{
	int dataRdy,cycles = 0;
	int bytesWrite = 0;

	if (pSpiDev->flags == O_RDONLY){
		return ERROR;
	}

	switch(pSpiDev->channel){
		case 0:
		while(bytesWrite < nbytes && cycles < MAX_WAIT_CYCLES)
			dataRdy = spi_bitbang(CHN1_STATUS_RD<<8|0);
		if (dataRdy & WR_RDY){
			spi_bitbang(CHN1_DATA_WR<<8|buffer[bytesWrite]);
			bytesWrite ++;
			cycles = 0;
		}else{
			cycles++;
		}
		break;

		case 1:
		while(bytesWrite < nbytes && cycles < MAX_WAIT_CYCLES)
			dataRdy = spi_bitbang(CHN2_STATUS_RD<<8|0);
		if (dataRdy & WR_RDY){
			spi_bitbang(CHN2_DATA_WR<<8|buffer[bytesWrite]);
			bytesWrite ++;
			cycles = 0;
		}else{
			cycles++;
		}
		break;

		case 2:
		//....

		case 3:
		//.....

		default:
		printErr("bad channel num:%d\n",pSpiDev->channel);
		bytesWrite = -1;
		break;
	}

	return bytesWrite;
}
//=====================================================================================
/*
spiRead和spiWrite的实现非常的相似，只是更换了一下数据的传输方向。VK3224每次以16-bit为单位
进行操作，但是高8-bit都是状态位，只有低8-bit才是有效数据位，所以nbytes个字节需要nbytes次读写操作。
在每一次进行数据的读写之前，必须要检查相关的寄存器状态位，对于读操作而言，查看下一个数据是否准备好，
而对于写操作而言，检查前一个写入的数据是否已经得到处理。另外为了避免可能的UART端出现问题，对于状态位
的检查不是无限制的，代码中设备了一个循环等待次数（1000次），如果在检查了这么多次后数据依然没有准备好，
（对于读操作）或者没有被处理（对于写操作），则推出本次读写操作，返回当前已经处理的字节数。
这种行为是允许的，也是标准文件接口函数的正常行为，用户应该对此有相应的处理。
	VK3224对于每个通道都设置了独立的寄存器组用以进行各自的配置和控制操作，其中包括状态和数据寄存器。
上面代码中诸如CHNx_xxx_RD,CHNx_xxx_WR就是对每一个通道各自的状态和数据寄存器进行读写的命令，
这些命令以常量的形式在底层驱动当中定义。
*/


//=====================================================================================



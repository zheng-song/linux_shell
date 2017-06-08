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
	UINT32 		u2sregBase; 	//�豸�Ĵ�������ַ
	UINT32 		u2sDevbuf;  	//���ݻ���������ַ
	BOOL 		u2sisOpen;      //�豸�Ѵ򿪱�־
	UINT8 		u2sintLv;		//�豸�жϺ�
	char *		u2sputData;		//�ں˻ص�����ָ�룬ָ��ĺ������ں��ṩ����
	char *		u2sgetdata;		//�ں˻ص�����ָ�룬ָ��ĺ������ں˻�ȡ����
	}u2s_DEV;


STATUS u2sDevCreate(char *devName ,int arg)
{
	u2s_DEV *
	}

*/


#define SIPSPD		��100000000��
GPIO_RED *gpioReg = (GPIOREG *)GPIO_REG_BASE;
CLKPW_REG *gCLKPWReg = (CLKPW_REG *)CLKPW_REG_BASE;


typedef struct
{
DEV_HDR 	pDevHdr;
UINT32		gpioRegBase;	//GPIO���ƼĴ�������ַ
UINT8		channel;		//ͨ���ţ�ָʾ��ǰ�Ƕ���һ�����ڽ��в���
UINT8		flags;			//�����ض�ͨ���Ĳ�������
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
		i=t-i;		//��ʱ�������˴���i��û�����ã�֪ʶΪ�˲����Ǹ�CPU��ת����
	}
}

//GPIO F1����ƬѡSCS
void scs(int d)
{
	if (d){
		gpioReg->rGPFDAT |= (1<<1);
	}else
	gpioReg->rGPFDAT &= ~(1<<1);
}

//GPIO E12���� MOSI
void mosi(int i)
{
	if (i){
		gpioReg->rGPEDAT |= (1<<12);
	}else
		gpioReg->rGPEDAT &= ~(1<<12);
}

// GPIO E11 ����MISO
void miso(int i)
{
	return(gpioReg->rGPEDAT >> 11)&0x1;
}

//GPIO G2����ʱ��SCK
void clk(int i)
{
	if (i){
		gpioReg->rGPGDAT |= (0x1<<2);
	}else
		gpioReg->rGPGDAT &= ~(0x1<<2);
}

//SPI�ӿڶ�д������ÿ��16-bit
//�ײ�����ʹ��spi_bitbang������VK3224 SPI�ӽӿڽ��ж�д��SPI�ӿ�ʹ�����������߽������ݴ���
//SPI���ӽӿ�֮�����ݵĽ�����ȫ˫���ģ�һ����д�����ݵ�ͬʱҲ�ڶ������ݣ�����˫������һ�����μĴ�����

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
	spi_gpio_init;			//����GPIOģ�� ������ʼ����
//=======================================================
/*
�˴���õ���spi_bitbang����ͨ��SPI�ӿ�����VK3224����ʼ����channelָ����ͨ��
*/
//=======================================================

	//���豸��ӵ�ϵͳ��
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
	�������ǵ�SPI�ײ��������ԣ���������ֱ��ʹ��GPIO����SPIʱ���ģ�⣬������û��ʹ���ж�
���Եײ������򿪺���spiOpen��ʵ��ֻ��Ҫͨ��SPI�ӿ����ö�Ӧ��ͨ���Ĵ�����ʹ��ͨ���������ɡ�
������ͨ�������ʵȲ�����������spiDrvCreate�������Ѿ���ɡ�
	��һ��������IO��ϵͳ�ṩ��IO��ϵͳ�ڸ���������Ѱַ����Ӧ��������ʱ���佫ϵͳ�豸�б�
�д洢���豸�ṹ��Ϊ��һ����������spiOpen��
	�ڶ����������豸��ƥ����ʣ��Ĳ��֣����Ǵ˴�����ʱ�����·������ϵͳ�豸�б��е�
�豸����ȫƥ�䣬�ʴ˴�����ôӦΪ���ַ���
	���������ĸ���������open����ʱ����ĵڶ�������������IO��ϵͳ����ԭ������spiOpen����
*/
//====================================================================================
	SPI_DEV		*pSpiDev;
	int status = OK;
	pSpiDev = (SPI_DEV *)pDevHdr;
	pSpiDev->flags = flags;

	switch(pSpiDev->channel){
		case 0:
		//����spi_bitbang��������VK3224��ʹ��ͨ��1���й�����
		break��

		case 1:
		//����spi_bitbang��������VK3224��ʹ��ͨ��2���й�����
		break��

		case 3:
		//����spi_bitbang��������VK3224��ʹ��ͨ��3���й�����
		break��
//spiOpen�����豸�ṹ�е�ͨ���ţ�channel����������һ��ͨ����ȡ��ʩ���ײ�����ͬʱ�����ĸ�ͨ����
//��SPI_DEV�ṹ��ר�Ŷ�����һ��channel�ֶ����ԶԸ���ͨ�����з�����
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
//��ȻVK3224ÿ�β���16-bit������ÿ��ֻ�ܶ�ȡ8-bit��Ч���ݣ�ʹ��spi_bitbang������VK3224ͨ��1��ȡ����
			dataRdy = spi_bitbang(CHN1_STATUS_RD<<8|0);//��������Ƿ������
		if (dataRdy & RD_RDY){//��������־λ��Ч����ȡһ�ֽ�����
			buffer[bytesRead] = (char)spi_bitbang(CHN1_STATUS_RD<<8|0);
			bytesRead++;
			cycles = 0;
		}else{//��δ������ѭ���ȴ���
			cycles++;
		}
		break;

		case 1:
		while(bytesRead<nbyes&&cycles<MAX_WAIT_CYCLES)
			dataRdy = spi_bitbang(CHN2_STATUS_RD<<8|0);//��������Ƿ������
		if (dataRdy & RD_RDY){//��������־λ��Ч����ȡһ�ֽ�����
			buffer[bytesRead] = (char)spi_bitbang(CHN2_STATUS_RD<<8|0);
			bytesRead++;
			cycles = 0;
		}else{//��δ������ѭ���ȴ���
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
spiRead��spiWrite��ʵ�ַǳ������ƣ�ֻ�Ǹ�����һ�����ݵĴ��䷽��VK3224ÿ����16-bitΪ��λ
���в��������Ǹ�8-bit����״̬λ��ֻ�е�8-bit������Ч����λ������nbytes���ֽ���Ҫnbytes�ζ�д������
��ÿһ�ν������ݵĶ�д֮ǰ������Ҫ�����صļĴ���״̬λ�����ڶ��������ԣ��鿴��һ�������Ƿ�׼���ã�
������д�������ԣ����ǰһ��д��������Ƿ��Ѿ��õ���������Ϊ�˱�����ܵ�UART�˳������⣬����״̬λ
�ļ�鲻�������Ƶģ��������豸��һ��ѭ���ȴ�������1000�Σ�������ڼ������ô��κ�������Ȼû��׼���ã�
�����ڶ�����������û�б���������д�����������Ƴ����ζ�д���������ص�ǰ�Ѿ�������ֽ�����
������Ϊ������ģ�Ҳ�Ǳ�׼�ļ��ӿں�����������Ϊ���û�Ӧ�öԴ�����Ӧ�Ĵ���
	VK3224����ÿ��ͨ���������˶����ļĴ��������Խ��и��Ե����úͿ��Ʋ��������а���״̬�����ݼĴ�����
�������������CHNx_xxx_RD,CHNx_xxx_WR���Ƕ�ÿһ��ͨ�����Ե�״̬�����ݼĴ������ж�д�����
��Щ�����Գ�������ʽ�ڵײ��������ж��塣
*/


//=====================================================================================



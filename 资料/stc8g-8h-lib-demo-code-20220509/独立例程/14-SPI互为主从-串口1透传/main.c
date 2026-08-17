/*---------------------------------------------------------------------*/
/* --- STC MCU Limited ------------------------------------------------*/
/* --- STC 1T Series MCU Demo Programme -------------------------------*/
/* --- Mobile: (86)13922805190 ----------------------------------------*/
/* --- Fax: 86-0513-55012956,55012947,55012969 ------------------------*/
/* --- Tel: 86-0513-55012928,55012929,55012966 ------------------------*/
/* --- Web: www.STCMCU.com --------------------------------------------*/
/* --- Web: www.STCMCUDATA.com  ---------------------------------------*/
/* --- QQ:  800003751 -------------------------------------------------*/
/* 如果要在程序中使用此代码,请在程序中注明使用了STC的资料及程序            */
/*---------------------------------------------------------------------*/

#include	"config.h"
#include	"GPIO.h"
#include	"delay.h"
#include	"UART.h"
#include	"spi.h"

/*************	功能说明	**************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8G、STC8H系列芯片可通用参考.

通过串口发送数据给MCU1，MCU1将接收到的数据由SPI发送给MCU2，MCU2再通过串口发送出去.

设置方法 2：
两个设备初始化时都设置 SSIG 为 0，MSTR 设置为0，此时两个设备都是不忽略 SS 的从机模式。
当其中一个设备需要启动传输时，先检测 SS 管脚的电平，如果时候高电平，
就将自己设置成忽略 SS 的主模式，自己的 SS 脚输出低电平，拉低对方的 SS 脚，即可进行数据传输。

         MCU1                          MCU2
  |-----------------|           |-----------------|
  |            MISO |-----------| MISO            |
--| TX         MOSI |-----------| MOSI         TX |--
  |            SCLK |-----------| SCLK            |
--| RX           SS |-----------| SS           RX |--
  |-----------------|           |-----------------|


下载时, 选择时钟 22.1184MHz (可以在配置文件"config.h"中修改).

******************************************/

/*************	本地常量声明	**************/


/*************	本地变量声明	**************/


/*************	本地函数声明	**************/



/*************  外部函数和变量声明 *****************/



/******************** IO口配置 ********************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P2,&GPIO_InitStructure);	//初始化
}

/****************  串口初始化函数 *****************/
void	UART_config(void)
{
	COMx_InitDefine		COMx_InitStructure;					//结构定义
	COMx_InitStructure.UART_Mode      = UART_8bit_BRTx;	//模式, UART_ShiftRight,UART_8bit_BRTx,UART_9bit,UART_9bit_BRTx
	COMx_InitStructure.UART_BRT_Use   = BRT_Timer1;			//选择波特率发生器, BRT_Timer1, BRT_Timer2 (注意: 串口2固定使用BRT_Timer2)
	COMx_InitStructure.UART_BaudRate  = 115200ul;			//波特率, 一般 110 ~ 115200
	COMx_InitStructure.UART_RxEnable  = ENABLE;				//接收允许,   ENABLE或DISABLE
	COMx_InitStructure.BaudRateDouble = DISABLE;			//波特率加倍, ENABLE或DISABLE
	COMx_InitStructure.UART_Interrupt = ENABLE;				//中断允许,   ENABLE或DISABLE
	COMx_InitStructure.UART_Priority    = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	COMx_InitStructure.UART_P_SW      = UART1_SW_P30_P31;	//切换端口,   UART1_SW_P30_P31,UART1_SW_P36_P37,UART1_SW_P16_P17,UART1_SW_P43_P44
	UART_Configuration(UART1, &COMx_InitStructure);		//初始化串口1 UART1,UART2,UART3,UART4

	PrintString1("STC8 UART1与SPI透传程序\r\n");		//UART1发送一个字符串
}

/****************  SPI初始化函数 *****************/
void	SPI_config(void)
{
	SPI_InitTypeDef		SPI_InitStructure;
	SPI_InitStructure.SPI_Module    = ENABLE;						//SPI启动    ENABLE, DISABLE
	SPI_InitStructure.SPI_SSIG      = ENABLE;					//片选位     ENABLE, DISABLE
	SPI_InitStructure.SPI_FirstBit  = SPI_MSB;					//移位方向   SPI_MSB, SPI_LSB
	SPI_InitStructure.SPI_Mode      = SPI_Mode_Slave;	  //主从选择   SPI_Mode_Master, SPI_Mode_Slave
	SPI_InitStructure.SPI_CPOL      = SPI_CPOL_Low;    //时钟相位   SPI_CPOL_High,   SPI_CPOL_Low
	SPI_InitStructure.SPI_CPHA      = SPI_CPHA_2Edge;	  //数据边沿   SPI_CPHA_1Edge,  SPI_CPHA_2Edge
	SPI_InitStructure.SPI_Interrupt = ENABLE;			  //中断允许   ENABLE,DISABLE
	SPI_InitStructure.SPI_Speed     = SPI_Speed_4;		  //SPI速度    SPI_Speed_4, SPI_Speed_16, SPI_Speed_64, SPI_Speed_128
	SPI_InitStructure.SPI_IoUse     = SPI_P22_P23_P24_P25; //IO口切换   SPI_P12_P13_P14_P15, SPI_P22_P23_P24_P25, SPI_P54_P40_P41_P43, SPI_P35_P34_P33_P32
	SPI_Init(&SPI_InitStructure);
	
	SPI_SS_2 = 1;
}


/******************** task A **************************/
void main(void)
{
	u8	i;

	GPIO_config();
	UART_config();
	SPI_config();
	EA = 1;

	while (1)
	{
		delay_ms(1);
				
		if(COM1.RX_TimeOut > 0)
		{
			if(--COM1.RX_TimeOut == 0)
			{
				if(COM1.RX_Cnt > 0)
				{
					COM1.B_RX_OK = 1;
				}
			}
		}
		if((COM1.B_RX_OK) && (SPI_SS_2))
		{
			SPI_SS_2 = 0;     //拉低从机 SS 管脚
			SPI_SetMode(SPI_Mode_Master);
			for(i=0;i<COM1.RX_Cnt;i++)
			{
				SPI_WriteByte(RX1_Buffer[i]); //发送串口数据
			}
			SPI_SS_2 = 1;    //拉高从机的 SS 管脚
			SPI_SetMode(SPI_Mode_Slave);
			COM1.RX_Cnt = 0;	//COM1.B_RX_OK = 0;
			COM1.B_RX_OK = 0;
		}
		
		if(SPI_RxTimerOut > 0)
		{
			if(--SPI_RxTimerOut == 0)
			{
				if(SPI_RxCnt > 0)
				{
					for(i=0; i<SPI_RxCnt; i++)	TX1_write2buff(SPI_RxBuffer[i]);
				}
				SPI_RxCnt = 0;
			}
		}
	}
}




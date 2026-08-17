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

#include	"APP_INT_UART1.h"
#include	"STC8G_H_GPIO.h"
#include	"STC8G_H_Exti.h"
#include	"STC8G_H_UART.h"
#include	"STC8G_H_Delay.h"
#include	"STC8G_H_NVIC.h"

/*************	功能说明	**************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8G、STC8H系列芯片可通用参考.

演示INT0~INT4 5个唤醒源将MCU从休眠唤醒.

从串口输出唤醒源跟唤醒次数，115200,N,8,1.

用定时器做波特率发生器，建议使用1T模式(除非低波特率用12T)，并选择可被波特率整除的时钟频率，以提高精度。

下载时, 选择时钟 22.1184MHz (用户可在"config.h"修改频率).

******************************************/


//========================================================================
//                               本地常量声明	
//========================================================================


//========================================================================
//                               本地变量声明
//========================================================================

u8 WakeUpCnt;

//========================================================================
//                               本地函数声明
//========================================================================


//========================================================================
//                            外部函数和变量声明
//========================================================================


//========================================================================
// 函数: INTtoUART_init
// 描述: 用户初始化程序.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-28
//========================================================================
void INTtoUART_init(void)
{
	EXTI_InitTypeDef	Exti_InitStructure;							//结构定义
	COMx_InitDefine		COMx_InitStructure;					//结构定义

	COMx_InitStructure.UART_Mode      = UART_8bit_BRTx;	//模式, UART_ShiftRight,UART_8bit_BRTx,UART_9bit,UART_9bit_BRTx
	COMx_InitStructure.UART_BRT_Use   = BRT_Timer1;			//选择波特率发生器, BRT_Timer1, BRT_Timer2 (注意: 串口2固定使用BRT_Timer2)
	COMx_InitStructure.UART_BaudRate  = 115200ul;			//波特率, 一般 110 ~ 115200
	COMx_InitStructure.UART_RxEnable  = ENABLE;				//接收允许,   ENABLE或DISABLE
	COMx_InitStructure.BaudRateDouble = DISABLE;			//波特率加倍, ENABLE或DISABLE
	UART_Configuration(UART1, &COMx_InitStructure);		//初始化串口1 UART1,UART2,UART3,UART4
	NVIC_UART1_Init(ENABLE,Priority_1);		//中断使能, ENABLE/DISABLE; 优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	//------------------------------------------------
	Exti_InitStructure.EXTI_Mode      = EXT_MODE_Fall;//中断模式,   EXT_MODE_RiseFall,EXT_MODE_Fall
	Ext_Inilize(EXT_INT0,&Exti_InitStructure);				//初始化
	NVIC_INT0_Init(ENABLE,Priority_0);		//中断使能, ENABLE/DISABLE; 优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3

	Exti_InitStructure.EXTI_Mode      = EXT_MODE_Fall;//中断模式,   EXT_MODE_RiseFall,EXT_MODE_Fall
	Ext_Inilize(EXT_INT1,&Exti_InitStructure);				//初始化
	NVIC_INT1_Init(ENABLE,Priority_0);		//中断使能, ENABLE/DISABLE; 优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3

	NVIC_INT2_Init(ENABLE,NULL);		//中断使能, ENABLE/DISABLE; 无优先级
	NVIC_INT3_Init(ENABLE,NULL);		//中断使能, ENABLE/DISABLE; 无优先级
	NVIC_INT4_Init(ENABLE,NULL);		//中断使能, ENABLE/DISABLE; 无优先级

	P3_MODE_IO_PU(GPIO_Pin_HIGH);		//P3.4~P3.7 设置为准双向口
}

//========================================================================
// 函数: Sample_INTtoUART
// 描述: 用户应用程序.
// 参数: None.
// 返回: None.
// 版本: V1.0, 2020-09-24
//========================================================================
void Sample_INTtoUART(void)
{
	if(!INT0) return;	//等待外中断为高电平
	if(!INT1) return;	//等待外中断为高电平
	if(!INT2) return;	//等待外中断为高电平
	if(!INT3) return;	//等待外中断为高电平
	if(!INT4) return;	//等待外中断为高电平
	delay_ms(10);	//delay 10ms

	if(!INT0) return;	//等待外中断为高电平
	if(!INT1) return;	//等待外中断为高电平
	if(!INT2) return;	//等待外中断为高电平
	if(!INT3) return;	//等待外中断为高电平
	if(!INT4) return;	//等待外中断为高电平

	WakeUpSource = 0;

	MCU_POWER_DOWN();	//Sleep
	_nop_();
	_nop_();
	_nop_();
	delay_ms(1);	//delay 1ms
	
	if(WakeUpSource == 1)	PrintString1("外中断INT0唤醒  ");
	if(WakeUpSource == 2)	PrintString1("外中断INT1唤醒  ");
	if(WakeUpSource == 3)	PrintString1("外中断INT2唤醒  ");
	if(WakeUpSource == 4)	PrintString1("外中断INT3唤醒  ");
	if(WakeUpSource == 5)	PrintString1("外中断INT4唤醒  ");
	
	WakeUpCnt++;
	TX1_write2buff(WakeUpCnt/100+'0');
	TX1_write2buff(WakeUpCnt%100/10+'0');
	TX1_write2buff(WakeUpCnt%10+'0');
	PrintString1("次唤醒\r\n");
}


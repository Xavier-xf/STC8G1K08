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
#include	"PCA.h"
#include	"UART.h"
#include	"delay.h"

/*************   功能说明   ***************

本例程基于STC8G1K08-20PIN进行编写测试，STC8G系列芯片可通用参考.

PCA0  设置为8位PWM. P1.1输出变化的PWM信号, 类似"呼吸灯"的驱动.
PCA1  设置为捕捉. 可以连接到P1.1或者P3.7用来测试捕捉, 捕捉的时钟数从串口1输出. 也可以从外部输入一个信号来捕捉.
PCA2  设置为16位软件定时, 定时时间为15000个PCA时钟, 并且从P3.7输出这个信号,输出周期为30000个PCA时钟.

捕获脉宽时间=捕捉的时钟数/PCA时钟源, 比如 捕捉的时钟数 = 256, PCA时钟源 = 22.1184MHz(1T), 脉宽 = 256/22.1184MHz = 11.57us.

下载时, 选择时钟 22.1184MHz (用户可在"config.h"修改频率).

******************************************/

/*************	本地常量声明	**************/


/*************	本地变量声明	**************/

u8	cnt0;
u8	pwm0;		//pwm
bit	B_PWM0_Dir;	//方向, 0为+, 1为-.

/*************	本地函数声明	**************/


/*************  外部函数和变量声明 *****************/


/******************** IO口配置 ********************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_7;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);	//初始化
}

/***************  串口初始化函数 *****************/
void	UART_config(void)
{
	COMx_InitDefine		COMx_InitStructure;					//结构定义
	COMx_InitStructure.UART_Mode      = UART_8bit_BRTx;	//模式, UART_ShiftRight,UART_8bit_BRTx,UART_9bit,UART_9bit_BRTx
	COMx_InitStructure.UART_BRT_Use   = BRT_Timer1;			//选择波特率发生器, BRT_Timer1, BRT_Timer2 (注意: 串口2固定使用BRT_Timer2)
	COMx_InitStructure.UART_BaudRate  = 115200ul;			//波特率, 一般 110 ~ 115200
	COMx_InitStructure.UART_RxEnable  = ENABLE;				//接收允许,   ENABLE或DISABLE
	COMx_InitStructure.BaudRateDouble = DISABLE;			//波特率加倍, ENABLE或DISABLE
	COMx_InitStructure.UART_Interrupt = ENABLE;				//中断允许,   ENABLE或DISABLE
	COMx_InitStructure.UART_Priority  = Priority_0;		//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	COMx_InitStructure.UART_P_SW      = UART1_SW_P30_P31;	//切换端口,   UART1_SW_P30_P31,UART1_SW_P36_P37,UART1_SW_P16_P17,UART1_SW_P43_P44
	UART_Configuration(UART1, &COMx_InitStructure);		//初始化串口1 UART1,UART2,UART3,UART4

	PrintString1("STC8G1K08 PCA Test Programme!\r\n");	//UART1发送一个字符串
}

/******************** PCA配置 ********************/
void	PCA_config(void)
{
	PCA_InitTypeDef		PCA_InitStructure;

	PCA_InitStructure.PCA_Clock    = PCA_Clock_1T;		//PCA_Clock_1T, PCA_Clock_2T, PCA_Clock_4T, PCA_Clock_6T, PCA_Clock_8T, PCA_Clock_12T, PCA_Clock_Timer0_OF, PCA_Clock_ECI
	PCA_InitStructure.PCA_IoUse    = PCA_P12_P11_P10_P37;	//PCA_P12_P11_P10_P37, PCA_P34_P35_P36_P37, PCA_P24_P25_P26_P27
	PCA_InitStructure.PCA_Interrupt_Mode = DISABLE;		//ENABLE, DISABLE
	PCA_InitStructure.PCA_Priority = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	PCA_InitStructure.PCA_RUN      = DISABLE;					//ENABLE, DISABLE
	PCA_Init(PCA_Counter,&PCA_InitStructure);		//设置公用Counter

	PCA_InitStructure.PCA_Mode     = PCA_Mode_PWM;		//PCA_Mode_PWM, PCA_Mode_Capture, PCA_Mode_SoftTimer, PCA_Mode_HighPulseOutput
	PCA_InitStructure.PCA_PWM_Wide = PCA_PWM_8bit;		//PCA_PWM_8bit, PCA_PWM_7bit, PCA_PWM_6bit, PCA_PWM_10bit
	PCA_InitStructure.PCA_Interrupt_Mode = DISABLE;		//PCA_Rise_Active, PCA_Fall_Active, ENABLE, DISABLE
	PCA_InitStructure.PCA_Value    = 128 << 8;				//对于PWM,高8位为PWM占空比
	PCA_Init(PCA0,&PCA_InitStructure);

	PCA_InitStructure.PCA_Mode     = PCA_Mode_Capture;		//PCA_Mode_PWM, PCA_Mode_Capture, PCA_Mode_SoftTimer, PCA_Mode_HighPulseOutput
	PCA_InitStructure.PCA_PWM_Wide = 0;								//PCA_PWM_8bit, PCA_PWM_7bit, PCA_PWM_6bit, PCA_PWM_10bit
	PCA_InitStructure.PCA_Interrupt_Mode = PCA_Fall_Active | ENABLE;		//PCA_Rise_Active, PCA_Fall_Active, ENABLE, DISABLE
	PCA_InitStructure.PCA_Value    = 0;								//对于捕捉, 这个值没意义
	PCA_Init(PCA1,&PCA_InitStructure);

	PCA_InitStructure.PCA_Mode     = PCA_Mode_HighPulseOutput;		//PCA_Mode_PWM, PCA_Mode_Capture, PCA_Mode_SoftTimer, PCA_Mode_HighPulseOutput
	PCA_InitStructure.PCA_PWM_Wide = 0;								//PCA_PWM_8bit, PCA_PWM_7bit, PCA_PWM_6bit, PCA_PWM_10bit
	PCA_InitStructure.PCA_Interrupt_Mode = ENABLE;		//PCA_Rise_Active, PCA_Fall_Active, ENABLE, DISABLE
	PCA_InitStructure.PCA_Value    = 15000;						//对于软件定时, 为匹配比较值
	PCA_Init(PCA2,&PCA_InitStructure);

	CR = 1;							//启动PCA
}


/******************** task A **************************/
void main(void)
{
	GPIO_config();
	UART_config();
	PCA_config();
	EA = 1;

	pwm0 = 128;
	B_PWM0_Dir = 0;

	while (1)
	{
		delay_ms(1);

		if(B_Capture1)
		{
			B_Capture1 = 0;
			//捕获脉宽时间=CCAP1_tmp/PCA_Clock, 比如 CCAP1_tmp = 256, PCA_Clock = 22.1184MHz(1T), 脉宽 = 256/22.1184MHz = 11.57us
			TX1_write2buff(CCAP1_tmp/10000 + '0');
			TX1_write2buff(CCAP1_tmp%10000/1000 + '0');
			TX1_write2buff(CCAP1_tmp%1000/100 + '0');
			TX1_write2buff(CCAP1_tmp%100/10 + '0');
			TX1_write2buff(CCAP1_tmp%10 + '0');
			PrintString1("\r\n");
		}

		cnt0++;
		if((cnt0 & 0x07) == 0x07)	//8ms
		{
			if(B_PWM0_Dir)
			{
					if(--pwm0 <= 8)		B_PWM0_Dir = 0;
			}
			else	if(++pwm0 >= 248)	B_PWM0_Dir = 1;
			UpdatePwm(PCA0,pwm0);
		}
	}
}




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
#include	"timer.h"
#include	"GPIO.h"
#include	"PWM.h"

/*************	功能说明	**************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8H系列芯片可通用参考.

高级PWM定时器 PWM1P/PWM1N,PWM2P/PWM2N,PWM3P/PWM3N,PWM4P/PWM4N 每个通道都可独立实现PWM输出，或者两两互补对称输出.

8个通道PWM设置对应P6的8个端口.

通过P6口上连接的8个LED灯，利用PWM实现呼吸灯效果.

PWM周期和占空比可以根据需要自行设置，最高可达65535.

下载时, 选择时钟 24MHZ (用户可在"config.h"修改频率).

******************************************/

/*************	本地常量声明	**************/


/*************	本地变量声明	**************/

PWMx_Duty PWMA_Duty;

/*************	本地函数声明	**************/



/*************  外部函数和变量声明 *****************/



/************************ IO口配置 ****************************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义
	GPIO_InitStructure.Pin  = GPIO_Pin_0;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P4,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P6,&GPIO_InitStructure);	//初始化
}

/************************ 定时器配置 ****************************/
void	Timer_config(void)
{
	TIM_InitTypeDef		TIM_InitStructure;						//结构定义
	TIM_InitStructure.TIM_Mode      = TIM_16BitAutoReload;	//指定工作模式,   TIM_16BitAutoReload,TIM_16Bit,TIM_8BitAutoReload,TIM_16BitAutoReloadNoMask
	TIM_InitStructure.TIM_Priority    = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	TIM_InitStructure.TIM_Interrupt = ENABLE;					//中断是否允许,   ENABLE或DISABLE
	TIM_InitStructure.TIM_ClkSource = TIM_CLOCK_1T;		//指定时钟源,     TIM_CLOCK_1T,TIM_CLOCK_12T,TIM_CLOCK_Ext
	TIM_InitStructure.TIM_ClkOut    = DISABLE;				//是否输出高速脉冲, ENABLE或DISABLE
	TIM_InitStructure.TIM_Value     = 65536UL - (MAIN_Fosc / 1000);		//中断频率, 1000次/秒
	TIM_InitStructure.TIM_Run       = ENABLE;					//是否初始化后启动定时器, ENABLE或DISABLE
	Timer_Inilize(Timer0,&TIM_InitStructure);					//初始化Timer0	  Timer0,Timer1,Timer2,Timer3,Timer4
}

/***************  串口初始化函数 *****************/
void	PWM_config(void)
{
	PWMx_InitDefine		PWMx_InitStructure;
	
	PWMx_InitStructure.PWM1_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM2_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM3_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2
	PWMx_InitStructure.PWM4_Mode    =	CCMRn_PWM_MODE1;	//模式,		CCMRn_FREEZE,CCMRn_MATCH_VALID,CCMRn_MATCH_INVALID,CCMRn_ROLLOVER,CCMRn_FORCE_INVALID,CCMRn_FORCE_VALID,CCMRn_PWM_MODE1,CCMRn_PWM_MODE2

	PWMx_InitStructure.PWM1_SetPriority  = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	PWMx_InitStructure.PWM2_SetPriority  = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	PWMx_InitStructure.PWM3_SetPriority  = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	PWMx_InitStructure.PWM4_SetPriority  = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	
	PWMx_InitStructure.PWM_Period   = 2047;							//周期时间,   0~65535
	PWMx_InitStructure.PWM1_Duty    = PWMA_Duty.PWM1_Duty;	//PWM1占空比时间, 0~Period
	PWMx_InitStructure.PWM2_Duty    = PWMA_Duty.PWM2_Duty;	//PWM2占空比时间, 0~Period
	PWMx_InitStructure.PWM3_Duty    = PWMA_Duty.PWM3_Duty;	//PWM3占空比时间, 0~Period
	PWMx_InitStructure.PWM4_Duty    = PWMA_Duty.PWM4_Duty;	//PWM4占空比时间, 0~Period
	PWMx_InitStructure.PWM_DeadTime = 0;								//死区发生器设置, 0~255
	
	PWMx_InitStructure.PWM_EnoSelect   = ENO1P | ENO1N | ENO2P | ENO2N | ENO3P | ENO3N | ENO4P | ENO4N;	//输出通道选择,	ENO1P,ENO1N,ENO2P,ENO2N,ENO3P,ENO3N,ENO4P,ENO4N / ENO5P,ENO6P,ENO7P,ENO8P
	PWMx_InitStructure.PWM_PS_SW       = PWM1_SW_P60_P61| PWM2_SW_P62_P63 | PWM3_SW_P64_P65 | PWM4_SW_P66_P67;	//切换端口,		PWM1_SW_P10_P11,PWM1_SW_P20_P21,PWM1_SW_P60_P61
																											//						PWM2_SW_P12_P13,PWM2_SW_P22_P23,PWM2_SW_P62_P63
																											//						PWM3_SW_P14_P15,PWM3_SW_P24_P25,PWM3_SW_P64_P65
																											//						PWM4_SW_P16_P17,PWM4_SW_P26_P27,PWM4_SW_P66_P67,PWM4_SW_P34_P33

	PWMx_InitStructure.PWM_CC1Enable   = ENABLE;				//开启PWM1P输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC1NEnable  = ENABLE;				//开启PWM1N输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC2Enable   = ENABLE;				//开启PWM2P输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC2NEnable  = ENABLE;				//开启PWM2N输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC3Enable   = ENABLE;				//开启PWM3P输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC3NEnable  = ENABLE;				//开启PWM3N输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC4Enable   = ENABLE;				//开启PWM4P输入捕获/比较输出,  ENABLE,DISABLE
	PWMx_InitStructure.PWM_CC4NEnable  = ENABLE;				//开启PWM4N输入捕获/比较输出,  ENABLE,DISABLE
	
	PWMx_InitStructure.PWM_MainOutEnable= ENABLE;				//主输出使能, ENABLE,DISABLE
	PWMx_InitStructure.PWM_CEN_Enable   = ENABLE;				//使能计数器, ENABLE,DISABLE
	PWM_Configuration(PWMA, &PWMx_InitStructure);				//初始化PWM,  PWMA,PWMB
}

/******************** 主函数**************************/
void main(void)
{
	PWMA_Duty.PWM1_Duty = 128;
	PWMA_Duty.PWM2_Duty = 256;
	PWMA_Duty.PWM3_Duty = 512;
	PWMA_Duty.PWM4_Duty = 1024;
	
	GPIO_config();
	Timer_config();
	PWM_config();
	EA = 1;
	P40 = 0;		//打开LED电源

	while (1);
}




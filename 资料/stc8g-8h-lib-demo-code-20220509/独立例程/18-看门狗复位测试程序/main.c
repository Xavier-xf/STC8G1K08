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
#include	"WDT.h"
#include	"delay.h"

/*************   功能说明   ***************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8G、STC8H系列芯片可通用参考.

初始化翻转电平, 5秒后不喂狗, 等待看门狗复位.

下载时, 选择时钟 24MHz (用户可在"config.h"修改频率).

******************************************/

/*************	本地常量声明	**************/


/*************	本地变量声明	**************/
u16 ms_cnt;
u8  second;    //测试用的计数变量

/*************	本地函数声明	**************/


/*************  外部函数和变量声明 *****************/



/******************** IO口配置 ********************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_0;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);	//初始化
}

/******************** INT配置 ********************/
void	WDT_config(void)
{
	WDT_InitTypeDef	WDT_InitStructure;					//结构定义

	WDT_InitStructure.WDT_Enable     = ENABLE;					//中断使能   ENABLE或DISABLE
	WDT_InitStructure.WDT_IDLE_Mode  = WDT_IDLE_STOP;		//IDLE模式是否停止计数		WDT_IDLE_STOP,WDT_IDLE_RUN
	WDT_InitStructure.WDT_PS         = WDT_SCALE_16;		//看门狗定时器时钟分频系数		WDT_SCALE_2,WDT_SCALE_4,WDT_SCALE_8,WDT_SCALE_16,WDT_SCALE_32,WDT_SCALE_64,WDT_SCALE_128,WDT_SCALE_256
	WDT_Inilize(&WDT_InitStructure);					//初始化
}

/******************** 主函数***********************/
void main(void)
{
	GPIO_config();
	
	P10 = 0;
	delay_ms(100);
	P10 = 1;
	delay_ms(100);
	P10 = 0;
	delay_ms(100);
	P10 = 1;
	delay_ms(100);
	P10 = 0;
	
	WDT_config();

	while(1)
	{
		delay_ms(1);    //延时1ms
		if(second <= 5)    //5秒后不喂狗, 将复位,
			WDT_Clear();    // 喂狗

		if(++ms_cnt >= 1000)
		{
			ms_cnt = 0;
			second++;
		}
	}
}


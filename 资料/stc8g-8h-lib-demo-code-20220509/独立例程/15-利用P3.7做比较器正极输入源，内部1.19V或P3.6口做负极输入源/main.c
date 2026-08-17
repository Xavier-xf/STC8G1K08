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

/*************  功能说明    **************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8G、STC8H系列芯片可通用参考.

比较器的正极可以是 P3.7 端口或者 ADC 的模拟输入通道，

而负极可以是 P3.6 端口或者是内部 BandGap 经过 OP 后的 REFV 电压（1.19V内部固定比较电压）。

通过中断或者查询方式读取比较器比较结果，CMP+的电平低于CMP-的电平P47口输出低电平(LED10亮)，反之输出高电平(LED10灭)。

下载时, 选择时钟 24MHz (可以在配置文件"config.h"中修改).

******************************************/

#include	"config.h"
#include	"compare.h"
#include	"GPIO.h"

/*************	本地常量声明	**************/


/*************	本地变量声明	**************/


/*************	本地函数声明	**************/


/*************  外部函数和变量声明 *****************/


/************************ 比较器配置 ****************************/
void	CMP_config(void)
{
	CMP_InitDefine CMP_InitStructure;						//结构定义
	CMP_InitStructure.CMP_EN = ENABLE;								//允许比较器		ENABLE,DISABLE
	CMP_InitStructure.CMP_RiseInterruptEn = ENABLE;		//允许上升沿中断	ENABLE,DISABLE
	CMP_InitStructure.CMP_FallInterruptEn = ENABLE;		//允许下降沿中断	ENABLE,DISABLE
	CMP_InitStructure.CMP_P_Select     = CMP_P_P37;		//比较器输入正极性选择, CMP_P_P37: 选择内部P3.7做正输入, CMP_P_ADC: 由ADCIS[2:0]所选择的ADC输入端做正输入.
	CMP_InitStructure.CMP_N_Select     = CMP_N_GAP;		//比较器输入负极性选择, CMP_N_GAP: 选择内部BandGap电压BGv做负输入, CMP_N_P36: 选择外部P3.6做输入.
	CMP_InitStructure.CMP_InvCMPO      = DISABLE;			//比较器输出取反, 	ENABLE,DISABLE
	CMP_InitStructure.CMP_100nsFilter  = ENABLE;			//内部0.1uF滤波,  	ENABLE,DISABLE
	CMP_InitStructure.CMP_Outpt_En     = ENABLE;			//允许比较结果输出,ENABLE,DISABLE
	CMP_InitStructure.CMP_P_SW         = CMP_OUT_P41;	//选择P3.4/P4.1作为比较器输出脚,  CMP_OUT_P34,CMP_OUT_P41
	CMP_InitStructure.CMP_OutDelayDuty = 16;					//比较结果变化延时周期数, 0~63
	CMP_InitStructure.CMP_Priority	   = Priority_0;		//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
	CMP_Inilize(&CMP_InitStructure);				//初始化比较器
}


/************************ IO口配置 ****************************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义
	GPIO_InitStructure.Pin  = GPIO_Pin_6 | GPIO_Pin_7;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_HighZ;				//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_1 | GPIO_Pin_7;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P4,&GPIO_InitStructure);	//初始化
}

/************************ 定时器配置 ****************************/

/******************** task A **************************/
void main(void)
{
	GPIO_config();
	CMP_config();

	EA = 1;
	while (1);
}

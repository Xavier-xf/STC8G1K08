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
#include	"timer.h"
#include	"I2C.h"

/*************	功能说明	**************

本例程基于STC8H8K64U为主控芯片的实验箱8进行编写测试，STC8G、STC8H系列芯片可通用参考.

内部集成I2C串行总线控制器做从机模式，SCL->P3.2, SDA->P3.3;
IO口模拟I2C做主机模式，SCL->P0.0, SDA->P0.1;
通过外部飞线连接 P0.0->P3.2, P0.1->P3.3，实现I2C自发自收功能。

用STC的MCU的IO方式驱动8位数码管。
使用Timer0的16位自动重装来产生1ms节拍,程序运行于这个节拍下,用户修改MCU主时钟频率时,自动定时于1ms.
计数器每秒钟加1, 计数范围为0~9999.

显示效果为: 上电后主机每秒钟发送一次计数数据，并在左边4个数码管上显示发送内容；从机接收到数据后在右边4个数码管显示。

下载时, 选择时钟 24MHz (可以在配置文件"config.h"中修改).

******************************************/

/*************	本地常量声明	**************/
#define DIS_DOT     0x20
#define DIS_BLACK   0x10
#define DIS_        0x11

u8 code t_display[]={                       //标准字库
//   0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71,
//black  -     H    J    K    L    N    o   P    U     t    G    Q    r   M    y
    0x00,0x40,0x76,0x1E,0x70,0x38,0x37,0x5C,0x73,0x3E,0x78,0x3d,0x67,0x50,0x37,0x6e,
    0xBF,0x86,0xDB,0xCF,0xE6,0xED,0xFD,0x87,0xFF,0xEF,0x46};    //0. 1. 2. 3. 4. 5. 6. 7. 8. 9. -1

u8 code T_COM[]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};      //位码

/*************	本地变量声明	**************/
u8  LED8[8];        //显示缓冲
u8  display_index;  //显示位索引
bit DisplayFlag;
bit B_1ms;          //1ms标志

u16 msecond;
u16 second;   //测试用的秒计数变量
u8  tmp[4];     //通用数组

/*************	本地函数声明	**************/



/*************  外部函数和变量声明 *****************/



/******************** IO口配置 ********************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_0 | GPIO_Pin_1;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_3;				//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P3,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P6,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P7,&GPIO_InitStructure);	//初始化
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

/****************  I2C初始化函数 *****************/
void	I2C_config(void)
{
	I2C_InitTypeDef		I2C_InitStructure;
	I2C_InitStructure.I2C_Mode      = I2C_Mode_Slave;		//主从选择   I2C_Mode_Master, I2C_Mode_Slave
	I2C_InitStructure.I2C_Enable    = ENABLE;						//I2C功能使能,   ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_ESTAI  = ENABLE;						//从机接收START信号中断使能,  ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_ERXI   = ENABLE;						//从机接收1字节数据中断使能,  ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_ETXI   = ENABLE;						//从机发送1字节数据中断使能,  ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_ESTOI  = ENABLE;						//从机接收STOP信号中断使能,   ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_MA     = ENABLE;						//使能从机地址比较功能,   ENABLE, DISABLE
	I2C_InitStructure.I2C_SL_ADR    = 0x2d;							//从机设备地址,  0~127  (0x2d<<1 = 0x5a)
	I2C_InitStructure.I2C_IoUse     = I2C_P33_P32;			//IO口切换   I2C_P14_P15, I2C_P24_P25, I2C_P33_P32

	I2C_Init(&I2C_InitStructure);
}


/******************** task A **************************/
void main(void)
{
	u8  i;
	
	GPIO_config();
	Timer_config();
	I2C_config();
	EA = 1;

	display_index = 0;
	DisplayFlag = 0;
	
	for(i=0; i<8; i++)  LED8[i] = 0x10; //上电消隐

	while (1)
	{
		if(DisplayFlag)
		{
			DisplayFlag = 0;
			LED8[4] = I2C_Buffer[0];
			LED8[5] = I2C_Buffer[1];
			LED8[6] = I2C_Buffer[2];
			LED8[7] = I2C_Buffer[3];
		}
		
		if(B_1ms)
		{
			B_1ms = 0;
			
			if(++msecond >= 1000)   //1秒到
			{
				msecond = 0;        //清1000ms计数
				second++;         //秒计数+1
				if(second >= 10000)    second = 0;   //秒计数范围为0~9999

				tmp[0] = second / 1000;
				tmp[1] = (second % 1000) / 100;
				tmp[2] = (second % 100) / 10;
				tmp[3] = second % 10;
				LED8[0] = tmp[0];
				LED8[1] = tmp[1];
				LED8[2] = tmp[2];
				LED8[3] = tmp[3];

				WriteNbyte(0, tmp, 4);
			}
		}
	}
}

/********************** 显示扫描函数 ************************/
void DisplayScan(void)
{   
    P7 = ~T_COM[7-display_index];
    P6 = ~t_display[LED8[display_index]];
    if(++display_index >= 8)    display_index = 0;  //8位结束回0
}




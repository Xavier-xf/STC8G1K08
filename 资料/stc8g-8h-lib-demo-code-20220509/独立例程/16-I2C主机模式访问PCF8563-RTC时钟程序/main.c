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

读写RTC, IC为PCF8563.

用STC的MCU的IO方式驱动8位数码管。

使用Timer0的16位自动重装来产生1ms节拍,程序运行于这个节拍下, 用户修改MCU主时钟频率时,自动定时于1ms.

8位数码管显示时间(小时-分钟-秒).

行列扫描按键键码为17~32.

按键只支持单键按下, 不支持多键同时按下, 那样将会有不可预知的结果.

键按下超过1秒后,将以10键/秒的速度提供重键输出. 用户只需要检测KeyCode是否非0来判断键是否按下.

调整时间键:
键码17: 小时+.
键码18: 小时-.
键码19: 分钟+.
键码20: 分钟-.

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
bit B_1ms;          //1ms标志

u8  IO_KeyState, IO_KeyState1, IO_KeyHoldCnt;    //行列键盘变量
u8  KeyHoldCnt; //键按下计时
u8  KeyCode;    //给用户使用的键码
u8  cnt50ms;

u8  hour,minute,second; //RTC变量
u16 msecond;

/*************	本地函数声明	**************/

void IO_KeyScan(void);   //50ms call
void DisplayRTC(void);
void ReadRTC(void);
void WriteRTC(void);


/*************  外部函数和变量声明 *****************/



/******************** IO口配置 ********************/
void	GPIO_config(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;				//结构定义

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P0,&GPIO_InitStructure);	//初始化

	GPIO_InitStructure.Pin  = GPIO_Pin_All;			//指定要初始化的IO, GPIO_Pin_0 ~ GPIO_Pin_7, 或操作
	GPIO_InitStructure.Mode = GPIO_PullUp;			//指定IO的输入或输出方式,GPIO_PullUp,GPIO_HighZ,GPIO_OUT_OD,GPIO_OUT_PP
	GPIO_Inilize(GPIO_P1,&GPIO_InitStructure);	//初始化

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
	TIM_InitStructure.TIM_Priority  = Priority_0;			//指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3
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
	I2C_InitStructure.I2C_Mode      = I2C_Mode_Master;	//主从选择   I2C_Mode_Master, I2C_Mode_Slave
	I2C_InitStructure.I2C_Enable    = ENABLE;						//I2C功能使能,   ENABLE, DISABLE
	I2C_InitStructure.I2C_MS_WDTA   = DISABLE;					//主机使能自动发送,  ENABLE, DISABLE
	I2C_InitStructure.I2C_MS_Interrupt = DISABLE;				//使能主机模式中断,  ENABLE, DISABLE
	I2C_InitStructure.I2C_Speed     = 16;								//总线速度=Fosc/2/(Speed*2+4),      0~63
	I2C_InitStructure.I2C_IoUse     = I2C_P14_P15;			//IO口切换   I2C_P14_P15, I2C_P24_P25, I2C_P33_P32

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
	
	for(i=0; i<8; i++)  LED8[i] = 0x10; //上电消隐
    
	ReadRTC();
	F0 = 0;
	if(second >= 60)    F0 = 1; //错误
	if(minute >= 60)    F0 = 1; //错误
	if(hour   >= 24)    F0 = 1; //错误
	if(F0)  //有错误, 默认12:00:00
	{
		second = 0;
		minute = 0;
		hour  = 12;
		WriteRTC();
	}

	DisplayRTC();
	LED8[2] = DIS_;
	LED8[5] = DIS_;

	KeyHoldCnt = 0; //键按下计时
	KeyCode = 0;    //给用户使用的键码

	IO_KeyState = 0;
	IO_KeyState1 = 0;
	IO_KeyHoldCnt = 0;
	cnt50ms = 0;

	while (1)
	{
		if(B_1ms)
		{
			B_1ms = 0;
			if(++msecond >= 1000)   //1秒到
			{
				msecond = 0;
				ReadRTC();
				DisplayRTC();
			}

			if(++cnt50ms >= 50)     //50ms扫描一次行列键盘
			{
				cnt50ms = 0;
				IO_KeyScan();
			}
			
			if(KeyCode != 0)        //有键按下
			{
				if(KeyCode == 17)   //hour +1
				{
					if(++hour >= 24)    hour = 0;
					WriteRTC();
					DisplayRTC();
				}
				if(KeyCode == 18)   //hour -1
				{
					if(--hour >= 24)    hour = 23;
					WriteRTC();
					DisplayRTC();
				}
				if(KeyCode == 19)   //minute +1
				{
					second = 0;
					if(++minute >= 60)  minute = 0;
					WriteRTC();
					DisplayRTC();
				}
				if(KeyCode == 20)   //minute -1
				{
					second = 0;
					if(--minute >= 60)  minute = 59;
					WriteRTC();
					DisplayRTC();
				}

				KeyCode = 0;
			}
		}
	}
}

/********************** 显示时钟函数 ************************/
void DisplayRTC(void)
{
    if(hour >= 10)  LED8[0] = hour / 10;
    else            LED8[0] = DIS_BLACK;
    LED8[1] = hour % 10;
    LED8[2] = DIS_;
    LED8[3] = minute / 10;
    LED8[4] = minute % 10;
    LED8[6] = second / 10;
    LED8[7] = second % 10;
}

/********************** 读RTC函数 ************************/
void ReadRTC(void)
{
    u8  tmp[3];

    ReadNbyte(2, tmp, 3);
    second = ((tmp[0] >> 4) & 0x07) * 10 + (tmp[0] & 0x0f);
    minute = ((tmp[1] >> 4) & 0x07) * 10 + (tmp[1] & 0x0f);
    hour   = ((tmp[2] >> 4) & 0x03) * 10 + (tmp[2] & 0x0f);
}

/********************** 写RTC函数 ************************/
void WriteRTC(void)
{
    u8  tmp[3];

    tmp[0] = ((second / 10) << 4) + (second % 10);
    tmp[1] = ((minute / 10) << 4) + (minute % 10);
    tmp[2] = ((hour / 10) << 4) + (hour % 10);
    WriteNbyte(2, tmp, 3);
}


/*****************************************************
    行列键扫描程序
    使用XY查找4x4键的方法，只能单键，速度快

   Y     P04      P05      P06      P07
          |        |        |        |
X         |        |        |        |
P00 ---- K00 ---- K01 ---- K02 ---- K03 ----
          |        |        |        |
P01 ---- K04 ---- K05 ---- K06 ---- K07 ----
          |        |        |        |
P02 ---- K08 ---- K09 ---- K10 ---- K11 ----
          |        |        |        |
P03 ---- K12 ---- K13 ---- K14 ---- K15 ----
          |        |        |        |
******************************************************/


u8 code T_KeyTable[16] = {0,1,2,0,3,0,0,0,4,0,0,0,0,0,0,0};

void IO_KeyDelay(void)
{
    u8 i;
    i = 60;
    while(--i)  ;
}

void IO_KeyScan(void)    //50ms call
{
    u8  j;

    j = IO_KeyState1;   //保存上一次状态

    P0 = 0xf0;  //X低，读Y
    IO_KeyDelay();
    IO_KeyState1 = P0 & 0xf0;

    P0 = 0x0f;  //Y低，读X
    IO_KeyDelay();
    IO_KeyState1 |= (P0 & 0x0f);
    IO_KeyState1 ^= 0xff;   //取反
    
    if(j == IO_KeyState1)   //连续两次读相等
    {
        j = IO_KeyState;
        IO_KeyState = IO_KeyState1;
        if(IO_KeyState != 0)    //有键按下
        {
            F0 = 0;
            if(j == 0)  F0 = 1; //第一次按下
            else if(j == IO_KeyState)
            {
                if(++IO_KeyHoldCnt >= 20)   //1秒后重键
                {
                    IO_KeyHoldCnt = 18;
                    F0 = 1;
                }
            }
            if(F0)
            {
                j = T_KeyTable[IO_KeyState >> 4];
                if((j != 0) && (T_KeyTable[IO_KeyState& 0x0f] != 0)) 
                    KeyCode = (j - 1) * 4 + T_KeyTable[IO_KeyState & 0x0f] + 16;    //计算键码，17~32
            }
        }
        else    IO_KeyHoldCnt = 0;
    }
    P0 = 0xff;
}


/********************** 显示扫描函数 ************************/
void DisplayScan(void)
{   
    P7 = ~T_COM[7-display_index];
    P6 = ~t_display[LED8[display_index]];
    if(++display_index >= 8)    display_index = 0;  //8位结束回0
}




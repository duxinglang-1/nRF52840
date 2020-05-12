/****************************************Copyright (c)************************************************
**                                      [艾克姆科技]
**                                        IIKMSIK 
**                            官方店铺：https://acmemcu.taobao.com
**                            官方论坛：http://www.e930bbs.com
**                                   
**--------------File Info-----------------------------------------------------------------------------
** File name:			     main.c
** Last modified Date:          
** Last Version:		   
** Descriptions:		   使用的SDK版本-SDK_15.2
**						
**----------------------------------------------------------------------------------------------------
** Created by:			
** Created date:		2018-12-1
** Version:			    1.0
** Descriptions:		GPIO驱动LED指示灯实验
**---------------------------------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "nrf_delay.h"
#include "boards.h"
#include "app_uart.h"

#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UART_PRESENT)
#include "nrf_uarte.h"
#endif
#include "nrf_nvmc.h"

#include "lcd.h"
#include "nrf_drv_timer.h"
#include "datetime.h"
#include "font.h"
#include "img.h"

/* 试验需要用到IK-52840DK开发板中的LED指示灯D1、按键S1和UART,占用的nRF52840引脚资源如下
P0.13：输出：驱动指示灯D1

P0.11：输入：检测按键S1

P0.06：串口发送TXD
P0.08：串口接收RXD
P0.05：串口RTS：发送请求，硬件流控开启时有效
P0.07：串口CTS：发送允许，硬件流控开启时有效

不使用硬件流控时需要用跳线帽短接P0.06和P0.08 P0.13
使用硬件流控时需要用跳线帽短接P0.05 P0.06 P0.07 P0.08 P0.13
*/

#define UART_TX_BUF_SIZE 256       //串口发送缓存大小（字节数）
#define UART_RX_BUF_SIZE 256       //串口接收缓存大小（字节数）

#define PI 3.1415926

#if defined(LCD_LH096TIG11G_ST7735SV)
#define ANALOG_CLOCK_CIRCLE_R 	30		//表圈半径
#define ANALOG_CLOCK_NUMBER_R 	25		//表圈数字半径
#define ANALOG_CLOCK_SCALE_R	40		//表圈刻度半径
#define ANALOG_CLOCK_HOUR_R		10		//时针半径
#define ANALOG_CLOCK_MIN_R		15		//分针半径
#define ANALOG_CLOCK_SEC_R		20		//秒针半径

#elif defined(LCD_R154101_ST7796S)
#define ANALOG_CLOCK_CIRCLE_R 	150		//表圈半径
#define ANALOG_CLOCK_NUMBER_R 	140		//表圈数字半径
#define ANALOG_CLOCK_SCALE_R	160		//表圈刻度半径
#define ANALOG_CLOCK_HOUR_R		70		//时针半径
#define ANALOG_CLOCK_MIN_R		100		//分针半径
#define ANALOG_CLOCK_SEC_R		120		//秒针半径
#endif

#define APP_TIMER_PRESCALER                  0                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE              4                                          /**< Size of timer operation queues. */

#define BUTTON_DEBOUNCE_DELAY                50 // Delay from a GPIOTE event until a button is reported as pushed. 
#define APP_GPIOTE_MAX_USERS                 1  // Maximum number of users of the GPIOTE handler. 


static bool show_date_time_first = true;
static bool update_date_time = false;

static uint8_t show_pic_count = 0;//图片显示顺序
static sys_date_timer_t date_time = {0};
static sys_date_timer_t last_date_time = {0};

static uint8_t clock_mode = 0;//0:analog  1:digital
static uint8_t date_time_changed = 0;//通过位来判断日期时间是否有变化，从第6位算起，分表表示年月日时分秒

static nrfx_timer_t TIMER_CLOCK = NRFX_TIMER_INSTANCE(0);

static void test_show_analog_clock(void);
static void test_show_digital_clock(void);
static void idle_show_time(void);

/***************************************************************************
* 描  述 : 设置GPIO高电平时的输出电压为3.3V 
* 入  参 : 无 
* 返回值 : 无
**************************************************************************/
static void gpio_output_voltage_setup_3v3(void)
{
    //读取UICR_REGOUT0寄存器，判断当前GPIO输出电压设置的是不是3.3V，如果不是，设置成3.3V
    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) !=
        (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos))
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
                            (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

        //复位更新UICR寄存器
        NVIC_SystemReset();
    }
}

//Timer时间回调函数
void timer_clock_event_handler(nrf_timer_event_t event_type, void* p_context)
{
	switch(event_type)
	{
		//因为我们配置的是使用CC通道0，所以时间回调函数中判断NRF_TIMER_EVENT_COMPARE0事件
		case NRF_TIMER_EVENT_COMPARE0:
			update_date_time = true;
			//idle_show_time();
			break;
		default:
			break;
	}
}

//定时器初始化
void timer_init(void)
{
	uint32_t err_code = NRF_SUCCESS;
	uint32_t time_ms = 500;
	uint32_t time_ticks;
	
	//定义定时器配置结构体，并使用默认配置参数初始化结构体
	nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
	
	//初始化定时器，初始化时会注册timet_led_event_handler时间回调函数
	err_code = nrfx_timer_init(&TIMER_CLOCK, &timer_cfg, timer_clock_event_handler);
	
	APP_ERROR_CHECK(err_code);
	
	//时间(单位ms)转换为ticks
	time_ticks = nrfx_timer_ms_to_ticks(&TIMER_CLOCK, time_ms);
	//设置定时器捕获/比较通道及该通道的比较值，使能通道的比较中断
	nrfx_timer_extended_compare(
			&TIMER_CLOCK, NRF_TIMER_CC_CHANNEL0, time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
}

/***************************************************************************
* 描  述 : idle_show_digit_clock 待机界面显示数字时钟
* 入  参 : 无 
* 返回值 : 无
**************************************************************************/
void idle_show_digital_clock(void)
{
	uint16_t x,y,w,h;
	uint8_t str_time[20] = {0};
	uint8_t str_date[20] = {0};
	uint8_t str_week[20] = {0};
	uint8_t *week[7] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
	
	POINT_COLOR=WHITE;
	BACK_COLOR=BLACK;
#if defined(LCD_R154101_ST7796S)
	LCD_SetFontSize(FONT_SIZE_24);
#elif defined()
	LCD_SetFontSize(FONT_SIZE_16);
#endif

	sprintf((char*)str_time, "%02d:%02d:%02d", date_time.hour, date_time.minute, date_time.second);
	sprintf((char*)str_date, "%04d/%02d/%02d", date_time.year, date_time.month, date_time.day);
	strcpy((char*)str_week, (const char*)week[date_time.week]);
	
	LCD_MeasureString(str_time,&w,&h);
	x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
	y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2 : 0;
	LCD_ShowString(x,y,str_time);//时间
	
	if(show_date_time_first)
	{
		show_date_time_first = false;
		
		LCD_MeasureString(str_date,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+30 : 0;
		LCD_ShowString(x,y,str_date);//日期

		LCD_MeasureString(str_week,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+60 : 0;
		LCD_ShowString(x,y,str_week);//星期
	}
	else if((date_time_changed&0x38) != 0)//日期有改变
	{
		LCD_MeasureString(str_date,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+30 : 0;
		LCD_ShowString(x,y,str_date);//日期

		LCD_MeasureString(str_week,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+60 : 0;
		LCD_ShowString(x,y,str_week);//星期
		
		date_time_changed = date_time_changed&0xC7;//清空日期变化标志位
	}
}

void radraw_second_hand(int second, bool flag)
{
	double a_sec;       // 秒针的弧度值
	int x_sec, y_sec; 	//秒针的末端位置
	int x_sec1,y_sec1;	//秒针的开始位置

	POINT_COLOR = RED;

	//计算秒针的弧度值
	a_sec = second * 2 * PI / 60;

	// 计算秒针的首末端位置
	x_sec = LCD_WIDTH/2 + (int)(ANALOG_CLOCK_SEC_R * sin(a_sec));
	y_sec = LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_SEC_R * cos(a_sec));
	x_sec1= LCD_WIDTH/2 - (int)(15 * sin(a_sec));
	y_sec1= LCD_HEIGHT/2 + (int)(15 * cos(a_sec));

	// 画秒针
	if(flag)
		POINT_COLOR = BLACK;//清除以前的秒针
	LCD_DrawLine(x_sec1, y_sec1, x_sec, y_sec);//分针	
}

void radraw_minute_hand(int minute, bool flag)
{
	double a_min;       //分针的弧度值
	int x_min, y_min; 	//分针的末端位置
	int x_min1,y_min1;	//分针的开始位置

	POINT_COLOR = BLUE;

	//计算分针的弧度值
	a_min = minute * 2 * PI / 60;

	//计算分针的首末端位置
	x_min = LCD_WIDTH/2 + (int)(ANALOG_CLOCK_MIN_R * sin(a_min));
	y_min = LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_MIN_R * cos(a_min));
	x_min1= LCD_WIDTH/2 - (int)(10 * sin(a_min));
	y_min1= LCD_HEIGHT/2 + (int)(10 * cos(a_min));

	// 画分针
	if(flag)
		POINT_COLOR = BLACK;
	LCD_DrawLine(x_min1, y_min1, x_min, y_min);//分针	
}

void radraw_hour_hand(int hour, int minute, bool flag)
{
	double a_hour, a_min;   //时、分针的弧度值
	int x_hour, y_hour; 	//时针的末端位置
	int x_hour1,y_hour1;	//时针的开始位置

	POINT_COLOR = WHITE;

	//计算时针的弧度值
	a_min = minute * 2 * PI / 60;
	a_hour= hour * 2 * PI / 12 + a_min / 12;

	//计算时针的首末端位置
	x_hour= LCD_WIDTH/2 + (int)(ANALOG_CLOCK_HOUR_R * sin(a_hour));
	y_hour= LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_HOUR_R * cos(a_hour));
	x_hour1= LCD_WIDTH/2 - (int)(5 * sin(a_hour));
	y_hour1= LCD_HEIGHT/2 + (int)(5 * cos(a_hour));

	// 画时针
	if(flag)
		POINT_COLOR = BLACK;
	LCD_DrawLine(x_hour1, y_hour1, x_hour, y_hour);//时针
}

void Draw(int hour, int minute, int second)
{
	radraw_hour_hand(hour, minute, false);
	radraw_minute_hand(minute, false);
	radraw_second_hand(second, false);
}

/***************************************************************************
* 描  述 : idle_show_digit_clock 待机界面显示模拟时钟
* 入  参 : 无 
* 返回值 : 无
**************************************************************************/
void idle_show_analog_clock(void)
{
	uint8_t str_date[20] = {0};

#if defined(LCD_LH096TIG11G_ST7735SV)
	LCD_SetFontSize(FONT_SIZE_16);
#elif defined(LCD_R154101_ST7796S)
	LCD_SetFontSize(FONT_SIZE_24);
#endif

	sprintf((char*)str_date, "%04d/%02d/%02d", date_time.year, date_time.month, date_time.day);
	
	if(show_date_time_first)
	{
		show_date_time_first = false;
		
		Draw(date_time.hour, date_time.minute, date_time.second);    //画表针
		
		POINT_COLOR=WHITE;
		LCD_ShowString((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,str_date);//日期所示
	}
	else if(date_time_changed != 0)
	{
		//if(((date_time_changed&0x04) != 0) || ((date_time.hour%12 == 0) && (last_date_time.second == 0)))//时钟有变化
		{
			radraw_hour_hand(date_time.hour, date_time.minute, false);
			date_time_changed = date_time_changed&0xFB;
		}
		//if((date_time_changed&0x02) != 0 || date_time.minute == last_date_time.second)//分钟有变化
		{
			radraw_minute_hand(date_time.minute, false);
			date_time_changed = date_time_changed&0xFD;
		}
		//if((date_time_changed&0x01) != 0)//秒钟有改变
		{
			radraw_second_hand(date_time.second, false);
			date_time_changed = date_time_changed&0xFE;
		}
		
		//if((date_time_changed&0x38) != 0)//日期有改变
		{
			POINT_COLOR=WHITE;
			LCD_ShowString((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,str_date);//日期所示
		
			date_time_changed = date_time_changed&0xC7;//清空日期标志位
		}
	}
}

void idle_show_clock_background(void)
{
	int32_t x,y;

	LCD_Clear(BLACK);
	BACK_COLOR=BLACK;
	POINT_COLOR=WHITE;
	
	LCD_SetFontSize(FONT_SIZE_16);
	
	if(clock_mode == 0)
	{
		// 绘制一个简单的表盘
		LCD_Draw_Circle(LCD_WIDTH/2, LCD_HEIGHT/2, 3);
		LCD_Draw_Circle(LCD_WIDTH/2, LCD_HEIGHT/2, ANALOG_CLOCK_CIRCLE_R);
		
		for(int i=0;i<12;i++)
		{
			x=LCD_WIDTH/2+(int)(ANALOG_CLOCK_NUMBER_R*sin(30*i*2*PI/360));
			y=LCD_HEIGHT/2-(int)(ANALOG_CLOCK_NUMBER_R*cos(30*i*2*PI/360));
			switch(i)
			{
			case 0:LCD_ShowString(x-5,y-5,(uint8_t*)"12");break;
			case 1:LCD_ShowString(x-5,y-5,(uint8_t*)"1");break;
			case 2:LCD_ShowString(x-5,y-5,(uint8_t*)"2");break;
			case 3:LCD_ShowString(x-5,y-5,(uint8_t*)"3");break;
			case 4:LCD_ShowString(x-5,y-5,(uint8_t*)"4");break;
			case 5:LCD_ShowString(x-5,y-5,(uint8_t*)"5");break;
			case 6:LCD_ShowString(x-5,y-5,(uint8_t*)"6");break;
			case 7:LCD_ShowString(x-5,y-5,(uint8_t*)"7");break;
			case 8:LCD_ShowString(x-5,y-5,(uint8_t*)"8");break;
			case 9:LCD_ShowString(x-5,y-5,(uint8_t*)"9");break;
			case 10:LCD_ShowString(x-5,y-5,(uint8_t*)"10");break;
			case 11:LCD_ShowString(x-5,y-5,(uint8_t*)"11");break;
			}
		}
		
		//画刻度
		int a,b,a1,b1,n=0;
		for(n=0;n<60;n++)
		{
			a=LCD_WIDTH/2+(int)(ANALOG_CLOCK_SCALE_R * sin(n*2*PI/60));
			b=LCD_HEIGHT/2-(int)(ANALOG_CLOCK_SCALE_R * cos(n*2*PI/60));
			a1=LCD_WIDTH/2+(int)(ANALOG_CLOCK_CIRCLE_R * sin(n*2*PI/60));
			b1=LCD_HEIGHT/2-(int)(ANALOG_CLOCK_CIRCLE_R * cos(n*2*PI/60));
			if(n%5==0)
				POINT_COLOR=RED;
			else
				POINT_COLOR=WHITE;

			LCD_DrawLine(a1,b1,a,b);
		}
	}
}

/***************************************************************************
* 描  述 : idle_show_time 待机界面显示时间
* 入  参 : 无 
* 返回值 : 无
**************************************************************************/
void idle_show_time(void)
{	
	memcpy(&last_date_time, &date_time, sizeof(sys_date_timer_t));
	
	date_time.second++;
	if(date_time.second > 59)
	{
		date_time.second = 0;
		date_time.minute++;
		date_time_changed = date_time_changed|0x02;
		date_time_changed = date_time_changed|0x04;//分针在变动的同时，时针也会同步缓慢变动
		if(date_time.minute > 59)
		{
			date_time.minute = 0;
			date_time.hour++;
			if(date_time.hour > 23)
			{
				date_time.hour = 0;
				date_time.day++;
				date_time.week++;
				if(date_time.week > 6)
					date_time.week = 0;
				date_time_changed = date_time_changed|0x08;
				if(date_time.month == 1 \
				|| date_time.month == 3 \
				|| date_time.month == 5 \
				|| date_time.month == 7 \
				|| date_time.month == 8 \
				|| date_time.month == 10 \
				|| date_time.month == 12)
				{
					if(date_time.day > 31)
					{
						date_time.day = 1;
						date_time.month++;
						date_time_changed = date_time_changed|0x10;
						if(date_time.month > 12)
						{
							date_time.month = 1;
							date_time.year++;
							date_time_changed = date_time_changed|0x20;
						}
					}
				}
				else if(date_time.month == 4 \
					|| date_time.month == 6 \
					|| date_time.month == 9 \
					|| date_time.month == 11)
				{
					if(date_time.day > 30)
					{
						date_time.day = 1;
						date_time.month++;
						date_time_changed = date_time_changed|0x10;
						if(date_time.month > 12)
						{
							date_time.month = 1;
							date_time.year++;
							date_time_changed = date_time_changed|0x20;
						}
					}
				}
				else
				{
					uint8_t Leap = date_time.year%4;
					
					if(date_time.day > (28+Leap))
					{
						date_time.day = 1;
						date_time.month++;
						date_time_changed = date_time_changed|0x10;
						if(date_time.month > 12)
						{
							date_time.month = 1;
							date_time.year++;
							date_time_changed = date_time_changed|0x20;
						}
					}
				}
			}
		}
	}
	date_time_changed = date_time_changed|0x01;		

	if(clock_mode == 0)
	{
		if((date_time_changed&0x01) != 0)
		{
			radraw_second_hand(last_date_time.second, true);//擦除以前的秒针
		}
		if((date_time_changed&0x02) != 0)
		{
			radraw_minute_hand(last_date_time.minute, true);//擦除以前的分针
		}
		if((date_time_changed&0x04) != 0)
		{
			radraw_hour_hand(last_date_time.hour, last_date_time.minute, true);//擦除以前的时针
		}
		
		if((date_time_changed&0x38) != 0)
		{			
			BlockWrite((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,10*24,24);
		}
		
		POINT_COLOR=WHITE;
		LCD_Draw_Circle(LCD_WIDTH/2, LCD_HEIGHT/2, 3);//补画圆心
		
		idle_show_analog_clock();
	}
	else
	{
		if((date_time_changed&0x01) != 0)
		{
			date_time_changed = date_time_changed&0xFE;
			BlockWrite((LCD_WIDTH-8*8)/2+6*8,(LCD_HEIGHT-16)/2,2*8,16);//擦除以前的秒钟
		}
		if((date_time_changed&0x02) != 0)
		{
			date_time_changed = date_time_changed&0xFD;
			BlockWrite((LCD_WIDTH-8*8)/2+3*8,(LCD_HEIGHT-16)/2,2*8,16);//擦除以前的分钟
		}
		if((date_time_changed&0x04) != 0)
		{
			date_time_changed = date_time_changed&0xFB;
			BlockWrite((LCD_WIDTH-8*8)/2,(LCD_HEIGHT-16)/2,2*8,16);//擦除以前的时钟
		}
		
		if((date_time_changed&0x38) != 0)
		{			
			BlockWrite((LCD_WIDTH-10*8)/2,(LCD_HEIGHT-16)/2+30,10*8,16);
		}
		
		idle_show_digital_clock();
	}
}

void test_show_analog_clock(void)
{
	clock_mode = 0;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();
	
	timer_init();											//初始化定时器	
	nrfx_timer_enable(&TIMER_CLOCK);						//启动定时器
}

void test_show_digital_clock(void)
{
	clock_mode = 1;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();
	
	timer_init();											//初始化定时器	
	nrfx_timer_enable(&TIMER_CLOCK);						//启动定时器
}

void test_show_image(void)
{
	uint8_t i=0;
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);
	LCD_get_pic_size(peppa_pig_160X160, &w, &h);
	while(1)
	{
		switch(i)
		{
			case 0:
				LCD_dis_pic(w*0,h*0,peppa_pig_160X160);
				break;
			case 1:
				LCD_dis_pic(w*1,h*0,peppa_pig_160X160);
				break;
			case 2:
				LCD_dis_pic(w*1,h*1,peppa_pig_160X160);
				break;
			case 3:
				LCD_dis_pic(w*0,h*1,peppa_pig_160X160);
				break;
			case 4:
				LCD_Fill(w*0,h*0,w,h,BLACK);
				break;
			case 5:
				LCD_Fill(w*1,h*0,w,h,BLACK);
				break;
			case 6:
				LCD_Fill(w*1,h*1,w,h,BLACK);
				break;
			case 7:
				LCD_Fill(w*0,h*1,w,h,BLACK);
				break;
		}
		
		i++;
		if(i>=8)
			i=0;
		
		nrf_delay_ms(1000);								//软件延时1000ms
	}
}

void test_show_string(void)
{
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);								//清屏
	
	POINT_COLOR=WHITE;								//画笔颜色
	BACK_COLOR=BLACK;  								//背景色 
	
	LCD_SetFontSize(FONT_SIZE_16);					//设置字体大小
	LCD_MeasureString("深圳市奥科斯数码有限公司",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = 40;	
	LCD_ShowString(x,y,"深圳市奥科斯数码有限公司");
	
	LCD_MeasureString("2015-2020 August International Ltd. All Rights Reserved.",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2015-2020 August International Ltd. All Rights Reserved.");
	
	LCD_SetFontSize(FONT_SIZE_24);					//设置字体大小
	LCD_MeasureString("Rawmec Business Park, Plumpton Road, Hoddesdon",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"Rawmec Business Park, Plumpton Road, Hoddesdon");
	
	LCD_MeasureString("深圳市龙华观澜环观南路凯美广场A座",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"深圳市龙华观澜环观南路凯美广场A座");
	
	LCD_SetFontSize(FONT_SIZE_32);					//设置字体大小
	LCD_MeasureString("2020-01-03 16:30:45",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2020-01-03 16:30:45");
}

/**@brief Function for the Power Management.
 */
static void power_manage(void)
{
    // Use directly __WFE and __SEV macros since the SoftDevice is not available.

    // Wait for event.
    __WFE();

    // Clear Event Register.
    __SEV();
    __WFE();
}

/***************************************************************************
* 描  述 : main函数 
* 入  参 : 无 
* 返回值 : int 类型
**************************************************************************/
int main(void)
{
	uint32_t err_code;
	uint32_t addr;
	uint32_t *pdat;											//定义读取Flash的指针
	uint8_t t = 0;
	
	gpio_output_voltage_setup_3v3();						//设置GPIO输出电压为3.3V
	bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);		//初始化开发板上的4个LED，即将驱动LED的GPIO配置为输出，
	nrf_gpio_pin_clear(LED_1);   							//LED指示灯D1初始状态设置为点亮

	LCD_Init();												//初始化LCD
	
//	test_show_string();										//字符显示测试
//	test_show_image();										//图片显示测试
	test_show_analog_clock();								//指针时钟测试
//	test_show_digital_clock();								//数字时钟测试
	
	while(true)
	{
		if(update_date_time)
		{
			update_date_time = false;

			nrf_gpio_pin_toggle(LED_2);
			
			t++;
			if(t == 2)
			{
				t = 0;
				idle_show_time();
			}
		}

		power_manage();
	}
}
/********************************************END FILE**************************************/

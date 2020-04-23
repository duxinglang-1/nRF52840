/****************************************Copyright (c)************************************************
**                                      [����ķ�Ƽ�]
**                                        IIKMSIK 
**                            �ٷ����̣�https://acmemcu.taobao.com
**                            �ٷ���̳��http://www.e930bbs.com
**                                   
**--------------File Info-----------------------------------------------------------------------------
** File name:			     main.c
** Last modified Date:          
** Last Version:		   
** Descriptions:		   ʹ�õ�SDK�汾-SDK_15.2
**						
**----------------------------------------------------------------------------------------------------
** Created by:			
** Created date:		2018-12-1
** Version:			    1.0
** Descriptions:		GPIO����LEDָʾ��ʵ��
**---------------------------------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "nrf_delay.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_clock.h"
#include "boards.h"
#include "app_uart.h"
#include "app_button.h"
#include "app_gpiote.h"
#include "app_timer.h"

#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UART_PRESENT)
#include "nrf_uarte.h"
#endif
#include "nrf_nvmc.h"

#include "lcd.h"
#include "datetime.h"
#include "font.h"
#include "img.h"

/* ������Ҫ�õ�IK-52840DK�������е�LEDָʾ��D1������S1��UART,ռ�õ�nRF52840������Դ����
P0.13�����������ָʾ��D1

P0.11�����룺��ⰴ��S1

P0.06�����ڷ���TXD
P0.08�����ڽ���RXD
P0.05������RTS����������Ӳ�����ؿ���ʱ��Ч
P0.07������CTS����������Ӳ�����ؿ���ʱ��Ч

��ʹ��Ӳ������ʱ��Ҫ������ñ�̽�P0.06��P0.08 P0.13
ʹ��Ӳ������ʱ��Ҫ������ñ�̽�P0.05 P0.06 P0.07 P0.08 P0.13
*/

#define UART_TX_BUF_SIZE 256       //���ڷ��ͻ����С���ֽ�����
#define UART_RX_BUF_SIZE 256       //���ڽ��ջ����С���ֽ�����

#define PI 3.1415926

static bool show_date_time_first = true;
static bool update_date_time = false;

static uint8_t show_pic_count = 0;//ͼƬ��ʾ˳��
static sys_date_timer_t date_time = {0};
static sys_date_timer_t last_date_time = {0};

static uint8_t language_mode = 0;//0:chinese  1:english
static uint8_t clock_mode = 0;//0:analog  1:digital
static uint8_t date_time_changed = 0;//ͨ��λ���ж�����ʱ���Ƿ��б仯���ӵ�6λ���𣬷ֱ��ʾ������ʱ����

bool lcd_sleep_in = false;
bool lcd_sleep_out = false;

APP_TIMER_DEF(g_clock_timer_id);  /**< Polling timer id. */

static void test_show_analog_clock(void);
static void test_show_digital_clock(void);
static void idle_show_time(void);

/***************************************************************************
* ��  �� : ����GPIO�ߵ�ƽʱ�������ѹΪ3.3V 
* ��  �� : �� 
* ����ֵ : ��
**************************************************************************/
static void gpio_output_voltage_setup_3v3(void)
{
    //��ȡUICR_REGOUT0�Ĵ������жϵ�ǰGPIO�����ѹ���õ��ǲ���3.3V��������ǣ����ó�3.3V
    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) !=
        (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos))
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) |
                            (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

        //��λ����UICR�Ĵ���
        NVIC_SystemReset();
    }
}

//Timerʱ��ص�����
static void clock_timer_handler(void * p_context)
{
    update_date_time = true;
}

void ClearAnalogHourPic(int hour)
{
	uint16_t offset_x=4,offset_y=4;
	uint16_t hour_x,hour_y,hour_w,hour_h;

	LCD_get_pic_size(clock_hour_1_31X31,&hour_w,&hour_h);
	hour_x = (LCD_WIDTH)/2;
	hour_y = (LCD_HEIGHT)/2;

	if((hour%12) == 3)
		LCD_Fill(hour_x-offset_x,hour_y+offset_y-hour_h,hour_w,hour_h,BLACK);
	else if((hour%12) == 6)
		LCD_Fill(hour_x-offset_x,hour_y-offset_y,hour_w,hour_h,BLACK);
	else if((hour%12) == 9)
		LCD_Fill(hour_x+offset_x-hour_w,hour_y-offset_y,hour_w,hour_h,BLACK);
	else if((hour%12) == 0)
		LCD_Fill(hour_x+offset_x-hour_w,hour_y+offset_y-hour_h,hour_w,hour_h,BLACK);
}

void DrawAnalogHourPic(int hour)
{
	uint16_t offset_x=4,offset_y=4;
	uint16_t hour_x,hour_y,hour_w,hour_h;
	unsigned int *hour_pic[3] = {clock_hour_1_31X31,clock_hour_2_31X31,clock_hour_3_31X31};

	LCD_get_pic_size(clock_hour_1_31X31,&hour_w,&hour_h);
	hour_x = (LCD_WIDTH)/2;
	hour_y = (LCD_HEIGHT)/2;

	if((hour%12)<3)
		LCD_dis_pic_rotate(hour_x-offset_x,hour_y+offset_y-hour_h,hour_pic[hour%3],0);
	else if(((hour%12)>=3) && ((hour%12)<6))
		LCD_dis_pic_rotate(hour_x-offset_x,hour_y-offset_y,hour_pic[hour%3],90);
	else if(((hour%12)>=6) && ((hour%12)<9))
		LCD_dis_pic_rotate(hour_x+offset_x-hour_w,hour_y-offset_y,hour_pic[hour%3],180);
	else if(((hour%12)>=9) && ((hour%12)<12))
		LCD_dis_pic_rotate(hour_x+offset_x-hour_w,hour_y+offset_y-hour_h,hour_pic[hour%3],270);
}

void ClearAnalogMinPic(int minute)
{
	uint16_t offset_x=4,offset_y=4;
	uint16_t min_x,min_y,min_w,min_h;
	
	LCD_get_pic_size(clock_min_1_31X31,&min_w,&min_h);
	min_x = (LCD_WIDTH)/2;
	min_y = (LCD_HEIGHT)/2;

	if(minute == 15)
		LCD_Fill(min_x-offset_x,min_y+offset_y-min_h,min_w,min_h,BLACK);
	else if(minute == 30)
		LCD_Fill(min_x-offset_x,min_y-offset_y,min_w,min_h,BLACK);
	else if(minute == 45)
		LCD_Fill(min_x+offset_x-min_w,min_y-offset_y,min_w,min_h,BLACK);
	else if(minute == 0)
		LCD_Fill(min_x+offset_x-min_w,min_y+offset_y-min_h,min_w,min_h,BLACK);
}

void DrawAnalogMinPic(int hour, int minute)
{
	uint16_t offset_x=4,offset_y=4;
	uint16_t min_x,min_y,min_w,min_h;
	unsigned int *min_pic[15] = {clock_min_1_31X31,clock_min_2_31X31,clock_min_3_31X31,clock_min_4_31X31,clock_min_5_31X31,\
								 clock_min_6_31X31,clock_min_7_31X31,clock_min_8_31X31,clock_min_9_31X31,clock_min_10_31X31,\
								 clock_min_11_31X31,clock_min_12_31X31,clock_min_13_31X31,clock_min_14_31X31,clock_min_15_31X31};
	
	LCD_get_pic_size(clock_min_1_31X31,&min_w,&min_h);
	min_x = (LCD_WIDTH)/2;
	min_y = (LCD_HEIGHT)/2;

	if(minute<15)
	{
		if((hour%12)<3)							//����ʱ�����ص���͸����ʾ
		{
			DrawAnalogHourPic(hour);
			LCD_dis_trans_pic_rotate(min_x-offset_x,min_y+offset_y-min_h,min_pic[minute%15],BLACK,0);
		}
		else if((hour%12)==3)		//�ٽ�㣬���벻͸����ʾ�����ǲ����ڸ�ʱ��
		{
			LCD_dis_pic_rotate(min_x-offset_x,min_y+offset_y-min_h,min_pic[minute%15],0);
			DrawAnalogHourPic(hour);
		}
		else
			LCD_dis_pic_rotate(min_x-offset_x,min_y+offset_y-min_h,min_pic[minute%15],0);
	}
	else if((minute>=15) && (minute<30))
	{
		if(((hour%12)>=3) && ((hour%12)<6))	//����ʱ�����ص���͸����ʾ
		{
			DrawAnalogHourPic(hour);
			LCD_dis_trans_pic_rotate(min_x-offset_x,min_y-offset_y,min_pic[minute%15],BLACK,90);
		}
		else if((hour%12)==6)		//�ٽ�㣬���벻͸����ʾ�����ǲ����ڸ�ʱ��
		{
			LCD_dis_pic_rotate(min_x-offset_x,min_y-offset_y,min_pic[minute%15],90);
			DrawAnalogHourPic(hour);
		}
		else
			LCD_dis_pic_rotate(min_x-offset_x,min_y-offset_y,min_pic[minute%15],90);
	}
	else if((minute>=30) && (minute<45))
	{
		if(((hour%12)>=6) && ((hour%12)<9))	//����ʱ�����ص���͸����ʾ
		{
			DrawAnalogHourPic(hour);
			LCD_dis_trans_pic_rotate(min_x+offset_x-min_w,min_y-offset_y,min_pic[minute%15],BLACK,180);
		}
		else if((hour%12)==9)		//�ٽ�㣬���벻͸����ʾ�����ǲ����ڸ�ʱ��
		{
			LCD_dis_pic_rotate(min_x+offset_x-min_w,min_y-offset_y,min_pic[minute%15],180);
			DrawAnalogHourPic(hour);
		}
		else
			LCD_dis_pic_rotate(min_x+offset_x-min_w,min_y-offset_y,min_pic[minute%15],180);
	}
	else if((minute>=45) && (minute<60))
	{
		if((hour%12)>=9)	//����ʱ�����ص���͸����ʾ
		{
			DrawAnalogHourPic(hour);
			LCD_dis_trans_pic_rotate(min_x+offset_x-min_w,min_y+offset_y-min_h,min_pic[minute%15],BLACK,270);
		}
		else if((hour%12)==0)		//�ٽ�㣬���벻͸����ʾ�����ǲ����ڸ�ʱ��
		{
			LCD_dis_pic_rotate(min_x+offset_x-min_w,min_y+offset_y-min_h,min_pic[minute%15],270);
			DrawAnalogHourPic(hour);
		}
		else
			LCD_dis_pic_rotate(min_x+offset_x-min_w,min_y+offset_y-min_h,min_pic[minute%15],270);
	}
}

/***************************************************************************
* ��  �� : idle_show_digit_clock ����������ʾ����ʱ��
* ��  �� : �� 
* ����ֵ : ��
**************************************************************************/
void idle_show_digital_clock(void)
{
	uint16_t x,y,w,h;
	uint8_t str_time[20] = {0};
	uint8_t str_date[20] = {0};
	uint8_t str_week[20] = {0};
	uint8_t *week[7] = {"������", "����һ", "���ڶ�", "������", "������", "������", "������"};
	
	POINT_COLOR=WHITE;
	BACK_COLOR=BLACK;
	LCD_SetFontSize(FONT_SIZE_16);
	
	sprintf((char*)str_time, "%02d:%02d:%02d", date_time.hour, date_time.minute, date_time.second);
	sprintf((char*)str_date, "%04d/%02d/%02d", date_time.year, date_time.month, date_time.day);
	strcpy((char*)str_week, (const char*)week[date_time.week]);
	
	LCD_MeasureString(str_time,&w,&h);
	x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
	y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2 : 0;
	LCD_ShowString(x,y,str_time);//ʱ��
	
	if(show_date_time_first)
	{
		show_date_time_first = false;
		
		LCD_MeasureString(str_date,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+30 : 0;
		LCD_ShowString(x,y,str_date);//����

		LCD_MeasureString(str_week,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+60 : 0;
		LCD_ShowString(x,y,str_week);//����
	}
	else if((date_time_changed&0x38) != 0)//�����иı�
	{
		LCD_MeasureString(str_date,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+30 : 0;
		LCD_ShowString(x,y,str_date);//����

		LCD_MeasureString(str_week,&w,&h);
		x = (LCD_WIDTH > w) ? (LCD_WIDTH-w)/2 : 0;
		y = (LCD_HEIGHT > h) ? (LCD_HEIGHT-h)/2+60 : 0;
		LCD_ShowString(x,y,str_week);//����
		
		date_time_changed = date_time_changed&0xC7;//������ڱ仯��־λ
	}
}

/***************************************************************************
* ��  �� : idle_show_analog_clock ����������ʾģ��ʱ��
* ��  �� : �� 
* ����ֵ : ��
**************************************************************************/
void idle_show_analog_clock(void)
{
	uint8_t str_date[20] = {0};
	uint8_t str_week[20] = {0};
	uint8_t *week_cn[7] = {"����", "��һ", "�ܶ�", "����", "����", "����", "����"};
	uint8_t *week_en[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
	uint16_t str_w,str_h;

	POINT_COLOR=WHITE;								//������ɫ
	BACK_COLOR=BLACK;  								//����ɫ 
	
	LCD_SetFontSize(FONT_SIZE_16);

	sprintf((char*)str_date, "%02d/%02d", date_time.day,date_time.month);
	if(language_mode == 0)
		strcpy(str_week, week_cn[date_time.week]);
	else
		strcpy(str_week, week_en[date_time.week]);
	
	if(show_date_time_first)
	{
		show_date_time_first = false;
		DrawAnalogHourPic(date_time.hour);
		DrawAnalogMinPic(date_time.hour, date_time.minute);

		LCD_MeasureString(str_week, &str_w, &str_h);
		LCD_ShowString((LCD_WIDTH-str_w)/2, 15, str_week);

		LCD_MeasureString(str_date, &str_w, &str_h);
		LCD_ShowString((LCD_WIDTH-str_w)/2, 33, str_date);
	}
	else if(date_time_changed != 0)
	{
		if((date_time_changed&0x04) != 0)
		{
			DrawAnalogHourPic(date_time.hour);
			date_time_changed = date_time_changed&0xFB;
		}	
		if((date_time_changed&0x02) != 0)//�����б仯
		{
			DrawAnalogHourPic(date_time.hour);
			DrawAnalogMinPic(date_time.hour, date_time.minute);
			date_time_changed = date_time_changed&0xFD;
		}
		if((date_time_changed&0x38) != 0)
		{
			LCD_MeasureString(str_week, &str_w, &str_h);
			LCD_ShowString((LCD_WIDTH-str_w)/2, 15, str_week);

			LCD_MeasureString(str_date, &str_w, &str_h);
			LCD_ShowString((LCD_WIDTH-str_w)/2, 33, str_date);

			date_time_changed = date_time_changed&0xC7;//������ڱ仯��־λ
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
		LCD_dis_pic(0,0,clock_bg_80X160);
		LCD_ShowString(40,120,"88");
	}
}

/***************************************************************************
* ��  �� : idle_show_time ����������ʾʱ��
* ��  �� : �� 
* ����ֵ : ��
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
		//date_time_changed = date_time_changed|0x04;//�����ڱ䶯��ͬʱ��ʱ��Ҳ��ͬ�������䶯
		if(date_time.minute > 59)
		{
			date_time.minute = 0;
			date_time.hour++;
			date_time_changed = date_time_changed|0x04;
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

	//ÿ���ӱ���һ��ʱ��
	if((date_time_changed&0x02) != 0)
	{
		SetSystemDateTime(date_time);
	}

	if(clock_mode == 0)
	{
		if((date_time_changed&0x02) != 0)
		{
			ClearAnalogMinPic(date_time.minute);//������ǰ�ķ���
		}
		if((date_time_changed&0x04) != 0)
		{
			ClearAnalogHourPic(date_time.hour);//������ǰ��ʱ��
		}

		idle_show_analog_clock();
	}
	else
	{
		if((date_time_changed&0x01) != 0)
		{
			date_time_changed = date_time_changed&0xFE;
			BlockWrite((LCD_WIDTH-8*8)/2+6*8,(LCD_HEIGHT-16)/2,2*8,16);//������ǰ������
		}
		if((date_time_changed&0x02) != 0)
		{
			date_time_changed = date_time_changed&0xFD;
			BlockWrite((LCD_WIDTH-8*8)/2+3*8,(LCD_HEIGHT-16)/2,2*8,16);//������ǰ�ķ���
		}
		if((date_time_changed&0x04) != 0)
		{
			date_time_changed = date_time_changed&0xFB;
			BlockWrite((LCD_WIDTH-8*8)/2,(LCD_HEIGHT-16)/2,2*8,16);//������ǰ��ʱ��
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
	uint32_t err_code;
	
	clock_mode = 0;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();

	err_code = app_timer_create(&g_clock_timer_id,	APP_TIMER_MODE_REPEATED, clock_timer_handler);	
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(g_clock_timer_id,APP_TIMER_TICKS(1000),NULL);
	APP_ERROR_CHECK(err_code);
}

void test_show_digital_clock(void)
{
	uint32_t err_code;
	
	clock_mode = 1;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();

	err_code = app_timer_create(&g_clock_timer_id,	APP_TIMER_MODE_REPEATED, clock_timer_handler);	
	APP_ERROR_CHECK(err_code);
	err_code = app_timer_start(g_clock_timer_id,APP_TIMER_TICKS(1000),NULL);
	APP_ERROR_CHECK(err_code);
}

void test_show_image(void)
{
	uint8_t i=0;
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);
	LCD_get_pic_size(clock_bg_80X160, &w, &h);
	while(1)
	{
		switch(i)
		{
			case 0:
				LCD_dis_pic(w*0,h*0,clock_bg_80X160);
				break;
			case 1:
				LCD_dis_pic(w*1,h*0,clock_bg_80X160);
				break;
			case 2:
				LCD_dis_pic(w*1,h*1,clock_bg_80X160);
				break;
			case 3:
				LCD_dis_pic(w*0,h*1,clock_bg_80X160);
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
		
		nrf_delay_ms(1000);								//�����ʱ1000ms
	}
}

void test_show_string(void)
{
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);								//����
	
	POINT_COLOR=WHITE;								//������ɫ
	BACK_COLOR=BLACK;  								//����ɫ 

#ifdef FONT_16
	LCD_SetFontSize(FONT_SIZE_16);					//���������С
#endif
	LCD_MeasureString("�����а¿�˹�������޹�˾",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = 40;	
	LCD_ShowString(x,y,"�����а¿�˹�������޹�˾");
	
	LCD_MeasureString("2015-2020 August International Ltd. All Rights Reserved.",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2015-2020 August International Ltd. All Rights Reserved.");

#ifdef FONT_24
	LCD_SetFontSize(FONT_SIZE_24);					//���������С
#endif
	LCD_MeasureString("Rawmec Business Park, Plumpton Road, Hoddesdon",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"Rawmec Business Park, Plumpton Road, Hoddesdon");
	
	LCD_MeasureString("��������������������·�����㳡A��",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"��������������������·�����㳡A��");

#ifdef FONT_32
	LCD_SetFontSize(FONT_SIZE_32);					//���������С
#endif
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

void system_init(void)
{
	uint32_t err_code;

	err_code = nrf_drv_clock_init();
	APP_ERROR_CHECK(err_code);
	nrf_drv_clock_lfclk_request(NULL);

	gpio_output_voltage_setup_3v3();						//����GPIO�����ѹΪ3.3V
	bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);		//��ʼ���������ϵ�4��LED����������LED��GPIO����Ϊ�����

	//������ʼ��
	key_init();
	//LCD��ʼ��
	LCD_Init();	
}

/***************************************************************************
* ��  �� : main���� 
* ��  �� : �� 
* ����ֵ : int ����
**************************************************************************/
int main(void)
{
	uint32_t err_code;
	uint32_t addr;
	uint32_t *pdat;											//�����ȡFlash��ָ��
	uint8_t t = 0;

	system_init();

//	test_show_string();										//�ַ���ʾ����
//	test_show_image();										//ͼƬ��ʾ����
	test_show_analog_clock();								//ָ��ʱ�Ӳ���
//	test_show_digital_clock();								//����ʱ�Ӳ���
	
	while(true)
	{
		if(update_date_time)
		{
			update_date_time = false;
			idle_show_time();
		}

		if(lcd_sleep_in)
		{
			lcd_sleep_in = false;
			LCD_SleepIn();
		}

		if(lcd_sleep_out)
		{
			lcd_sleep_out = false;
			LCD_SleepOut();
		}
		
		power_manage();
	}
}
/********************************************END FILE**************************************/

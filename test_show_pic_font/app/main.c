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

#if defined(LCD_LH096TIG11G_ST7735SV)
#define ANALOG_CLOCK_CIRCLE_R 	30		//��Ȧ�뾶
#define ANALOG_CLOCK_NUMBER_R 	25		//��Ȧ���ְ뾶
#define ANALOG_CLOCK_SCALE_R	40		//��Ȧ�̶Ȱ뾶
#define ANALOG_CLOCK_HOUR_R		10		//ʱ��뾶
#define ANALOG_CLOCK_MIN_R		15		//����뾶
#define ANALOG_CLOCK_SEC_R		20		//����뾶

#elif defined(LCD_R154101_ST7796S)
#define ANALOG_CLOCK_CIRCLE_R 	150		//��Ȧ�뾶
#define ANALOG_CLOCK_NUMBER_R 	140		//��Ȧ���ְ뾶
#define ANALOG_CLOCK_SCALE_R	160		//��Ȧ�̶Ȱ뾶
#define ANALOG_CLOCK_HOUR_R		70		//ʱ��뾶
#define ANALOG_CLOCK_MIN_R		100		//����뾶
#define ANALOG_CLOCK_SEC_R		120		//����뾶
#endif

#define APP_TIMER_PRESCALER                  0                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE              4                                          /**< Size of timer operation queues. */

#define BUTTON_DEBOUNCE_DELAY                50 // Delay from a GPIOTE event until a button is reported as pushed. 
#define APP_GPIOTE_MAX_USERS                 1  // Maximum number of users of the GPIOTE handler. 


static bool show_date_time_first = true;
static bool update_date_time = false;

static uint8_t show_pic_count = 0;//ͼƬ��ʾ˳��
static sys_date_timer_t date_time = {0};
static sys_date_timer_t last_date_time = {0};

static uint8_t clock_mode = 0;//0:analog  1:digital
static uint8_t date_time_changed = 0;//ͨ��λ���ж�����ʱ���Ƿ��б仯���ӵ�6λ���𣬷ֱ��ʾ������ʱ����

static nrfx_timer_t TIMER_CLOCK = NRFX_TIMER_INSTANCE(0);

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
void timer_clock_event_handler(nrf_timer_event_t event_type, void* p_context)
{
	switch(event_type)
	{
		//��Ϊ�������õ���ʹ��CCͨ��0������ʱ��ص��������ж�NRF_TIMER_EVENT_COMPARE0�¼�
		case NRF_TIMER_EVENT_COMPARE0:
			update_date_time = true;
			//idle_show_time();
			break;
		default:
			break;
	}
}

//��ʱ����ʼ��
void timer_init(void)
{
	uint32_t err_code = NRF_SUCCESS;
	uint32_t time_ms = 500;
	uint32_t time_ticks;
	
	//���嶨ʱ�����ýṹ�壬��ʹ��Ĭ�����ò�����ʼ���ṹ��
	nrfx_timer_config_t timer_cfg = NRFX_TIMER_DEFAULT_CONFIG;
	
	//��ʼ����ʱ������ʼ��ʱ��ע��timet_led_event_handlerʱ��ص�����
	err_code = nrfx_timer_init(&TIMER_CLOCK, &timer_cfg, timer_clock_event_handler);
	
	APP_ERROR_CHECK(err_code);
	
	//ʱ��(��λms)ת��Ϊticks
	time_ticks = nrfx_timer_ms_to_ticks(&TIMER_CLOCK, time_ms);
	//���ö�ʱ������/�Ƚ�ͨ������ͨ���ıȽ�ֵ��ʹ��ͨ���ıȽ��ж�
	nrfx_timer_extended_compare(
			&TIMER_CLOCK, NRF_TIMER_CC_CHANNEL0, time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
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

void radraw_second_hand(int second, bool flag)
{
	double a_sec;       // ����Ļ���ֵ
	int x_sec, y_sec; 	//�����ĩ��λ��
	int x_sec1,y_sec1;	//����Ŀ�ʼλ��

	POINT_COLOR = RED;

	//��������Ļ���ֵ
	a_sec = second * 2 * PI / 60;

	// �����������ĩ��λ��
	x_sec = LCD_WIDTH/2 + (int)(ANALOG_CLOCK_SEC_R * sin(a_sec));
	y_sec = LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_SEC_R * cos(a_sec));
	x_sec1= LCD_WIDTH/2 - (int)(15 * sin(a_sec));
	y_sec1= LCD_HEIGHT/2 + (int)(15 * cos(a_sec));

	// ������
	if(flag)
		POINT_COLOR = BLACK;//�����ǰ������
	LCD_DrawLine(x_sec1, y_sec1, x_sec, y_sec);//����	
}

void radraw_minute_hand(int minute, bool flag)
{
	double a_min;       //����Ļ���ֵ
	int x_min, y_min; 	//�����ĩ��λ��
	int x_min1,y_min1;	//����Ŀ�ʼλ��

	POINT_COLOR = BLUE;

	//�������Ļ���ֵ
	a_min = minute * 2 * PI / 60;

	//����������ĩ��λ��
	x_min = LCD_WIDTH/2 + (int)(ANALOG_CLOCK_MIN_R * sin(a_min));
	y_min = LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_MIN_R * cos(a_min));
	x_min1= LCD_WIDTH/2 - (int)(10 * sin(a_min));
	y_min1= LCD_HEIGHT/2 + (int)(10 * cos(a_min));

	// ������
	if(flag)
		POINT_COLOR = BLACK;
	LCD_DrawLine(x_min1, y_min1, x_min, y_min);//����	
}

void radraw_hour_hand(int hour, int minute, bool flag)
{
	double a_hour, a_min;   //ʱ������Ļ���ֵ
	int x_hour, y_hour; 	//ʱ���ĩ��λ��
	int x_hour1,y_hour1;	//ʱ��Ŀ�ʼλ��

	POINT_COLOR = WHITE;

	//����ʱ��Ļ���ֵ
	a_min = minute * 2 * PI / 60;
	a_hour= hour * 2 * PI / 12 + a_min / 12;

	//����ʱ�����ĩ��λ��
	x_hour= LCD_WIDTH/2 + (int)(ANALOG_CLOCK_HOUR_R * sin(a_hour));
	y_hour= LCD_HEIGHT/2 - (int)(ANALOG_CLOCK_HOUR_R * cos(a_hour));
	x_hour1= LCD_WIDTH/2 - (int)(5 * sin(a_hour));
	y_hour1= LCD_HEIGHT/2 + (int)(5 * cos(a_hour));

	// ��ʱ��
	if(flag)
		POINT_COLOR = BLACK;
	LCD_DrawLine(x_hour1, y_hour1, x_hour, y_hour);//ʱ��
}

void Draw(int hour, int minute, int second)
{
	radraw_hour_hand(hour, minute, false);
	radraw_minute_hand(minute, false);
	radraw_second_hand(second, false);
}

/***************************************************************************
* ��  �� : idle_show_digit_clock ����������ʾģ��ʱ��
* ��  �� : �� 
* ����ֵ : ��
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
		
		Draw(date_time.hour, date_time.minute, date_time.second);    //������
		
		POINT_COLOR=WHITE;
		LCD_ShowString((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,str_date);//������ʾ
	}
	else if(date_time_changed != 0)
	{
		//if(((date_time_changed&0x04) != 0) || ((date_time.hour%12 == 0) && (last_date_time.second == 0)))//ʱ���б仯
		{
			radraw_hour_hand(date_time.hour, date_time.minute, false);
			date_time_changed = date_time_changed&0xFB;
		}
		//if((date_time_changed&0x02) != 0 || date_time.minute == last_date_time.second)//�����б仯
		{
			radraw_minute_hand(date_time.minute, false);
			date_time_changed = date_time_changed&0xFD;
		}
		//if((date_time_changed&0x01) != 0)//�����иı�
		{
			radraw_second_hand(date_time.second, false);
			date_time_changed = date_time_changed&0xFE;
		}
		
		//if((date_time_changed&0x38) != 0)//�����иı�
		{
			POINT_COLOR=WHITE;
			LCD_ShowString((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,str_date);//������ʾ
		
			date_time_changed = date_time_changed&0xC7;//������ڱ�־λ
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
		// ����һ���򵥵ı���
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
		
		//���̶�
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
		date_time_changed = date_time_changed|0x04;//�����ڱ䶯��ͬʱ��ʱ��Ҳ��ͬ�������䶯
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
			radraw_second_hand(last_date_time.second, true);//������ǰ������
		}
		if((date_time_changed&0x02) != 0)
		{
			radraw_minute_hand(last_date_time.minute, true);//������ǰ�ķ���
		}
		if((date_time_changed&0x04) != 0)
		{
			radraw_hour_hand(last_date_time.hour, last_date_time.minute, true);//������ǰ��ʱ��
		}
		
		if((date_time_changed&0x38) != 0)
		{			
			BlockWrite((LCD_WIDTH-10*12)/2,(LCD_HEIGHT-24)/2+80,10*24,24);
		}
		
		POINT_COLOR=WHITE;
		LCD_Draw_Circle(LCD_WIDTH/2, LCD_HEIGHT/2, 3);//����Բ��
		
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
	clock_mode = 0;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();
	
	timer_init();											//��ʼ����ʱ��	
	nrfx_timer_enable(&TIMER_CLOCK);						//������ʱ��
}

void test_show_digital_clock(void)
{
	clock_mode = 1;
	
	GetSystemDateTime(&date_time);
	
	idle_show_clock_background();
	idle_show_time();
	
	timer_init();											//��ʼ����ʱ��	
	nrfx_timer_enable(&TIMER_CLOCK);						//������ʱ��
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
		
		nrf_delay_ms(1000);								//�����ʱ1000ms
	}
}

void test_show_string(void)
{
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);								//����
	
	POINT_COLOR=WHITE;								//������ɫ
	BACK_COLOR=BLACK;  								//����ɫ 
	
	LCD_SetFontSize(FONT_SIZE_16);					//���������С
	LCD_MeasureString("�����а¿�˹�������޹�˾",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = 40;	
	LCD_ShowString(x,y,"�����а¿�˹�������޹�˾");
	
	LCD_MeasureString("2015-2020 August International Ltd. All Rights Reserved.",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2015-2020 August International Ltd. All Rights Reserved.");
	
	LCD_SetFontSize(FONT_SIZE_24);					//���������С
	LCD_MeasureString("Rawmec Business Park, Plumpton Road, Hoddesdon",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"Rawmec Business Park, Plumpton Road, Hoddesdon");
	
	LCD_MeasureString("��������������������·�����㳡A��",&w,&h);
	x = (w > LCD_WIDTH)? 0 : (LCD_WIDTH-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"��������������������·�����㳡A��");
	
	LCD_SetFontSize(FONT_SIZE_32);					//���������С
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
	
	gpio_output_voltage_setup_3v3();						//����GPIO�����ѹΪ3.3V
	bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);		//��ʼ���������ϵ�4��LED����������LED��GPIO����Ϊ�����
	nrf_gpio_pin_clear(LED_1);   							//LEDָʾ��D1��ʼ״̬����Ϊ����

	LCD_Init();												//��ʼ��LCD
	
//	test_show_string();										//�ַ���ʾ����
//	test_show_image();										//ͼƬ��ʾ����
	test_show_analog_clock();								//ָ��ʱ�Ӳ���
//	test_show_digital_clock();								//����ʱ�Ӳ���
	
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

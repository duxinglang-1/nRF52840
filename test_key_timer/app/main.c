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
#include "nrf_drv_timer.h"
#include "nrf_drv_clock.h"
#include "datetime.h"
#include "font.h"
#include "img.h"

#define APP_TIMER_PRESCALER                  0                                          /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_OP_QUEUE_SIZE              4                                          /**< Size of timer operation queues. */

static nrfx_timer_t TIMER_CLOCK = NRFX_TIMER_INSTANCE(0);

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

void test_show_string(void)
{
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);								//����
	
	POINT_COLOR=WHITE;								//������ɫ
	BACK_COLOR=BLACK;  								//����ɫ 
	
	LCD_SetFontSize(FONT_SIZE_16);					//���������С
	LCD_MeasureString("�����а¿�˹�������޹�˾",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = 40;	
	LCD_ShowString(x,y,"�����а¿�˹�������޹�˾");
	
	LCD_MeasureString("2015-2020 August International Ltd. All Rights Reserved.",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2015-2020 August International Ltd. All Rights Reserved.");
	
	LCD_SetFontSize(FONT_SIZE_24);					//���������С
	LCD_MeasureString("Rawmec Business Park, Plumpton Road, Hoddesdon",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"Rawmec Business Park, Plumpton Road, Hoddesdon");
	
	LCD_MeasureString("��������������������·�����㳡A��",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"��������������������·�����㳡A��");
	
	LCD_SetFontSize(FONT_SIZE_32);					//���������С
	LCD_MeasureString("2020-01-03 16:30:45",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
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

	//err_code = nrf_drv_clock_init();
	//APP_ERROR_CHECK(err_code);
	//nrf_drv_clock_lfclk_request(NULL);

	gpio_output_voltage_setup_3v3();						//����GPIO�����ѹΪ3.3V

	//������ʼ��
	//key_init();
	//LCD��ʼ��
	//LCD_Init();	
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

	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,6)); 							//����P0.6Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,8)); 							//����P0.8Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,13)); 							//����P0.13Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,24)); 							//����P0.24Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,26));							//����P0.26Ϊ���

	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,1));								//����P1.1Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,3));								//����P1.3Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,5));								//����P1.5Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,7));								//����P1.7Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,9));								//����P1.9Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,10));							//����P1.10Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,12));							//����P1.12Ϊ���
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,14));							//����P1.14Ϊ���
	
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,6));								//����P0.6Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,8));   								//����P0.8Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,26));   							//����P0.26Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,1));   								//����P1.1Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,3));								//����P1.3Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,5));								//����P1.5Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,7));								//����P1.7Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,9));								//����P1.9Ϊ����ߵ�ƽ
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,14));								//����P1.14Ϊ����ߵ�ƽ
	
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0,13));								//����P0.13Ϊ����͵�ƽ
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0,24));								//����P0.24Ϊ����͵�ƽ
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1,10));								//����P1.10Ϊ����͵�ƽ
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1,12));								//����P1.12Ϊ����͵�ƽ

	//bsp_board_init(BSP_INIT_LEDS|BSP_INIT_BUTTONS);
	
//	test_show_string();										//�ַ���ʾ����

	while(true)
	{
		power_manage();
	}

}
/********************************************END FILE**************************************/

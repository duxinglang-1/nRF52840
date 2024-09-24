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

void test_show_string(void)
{
	uint16_t x,y,w,h;
	
	LCD_Clear(BLACK);								//清屏
	
	POINT_COLOR=WHITE;								//画笔颜色
	BACK_COLOR=BLACK;  								//背景色 
	
	LCD_SetFontSize(FONT_SIZE_16);					//设置字体大小
	LCD_MeasureString("深圳市奥科斯数码有限公司",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = 40;	
	LCD_ShowString(x,y,"深圳市奥科斯数码有限公司");
	
	LCD_MeasureString("2015-2020 August International Ltd. All Rights Reserved.",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"2015-2020 August International Ltd. All Rights Reserved.");
	
	LCD_SetFontSize(FONT_SIZE_24);					//设置字体大小
	LCD_MeasureString("Rawmec Business Park, Plumpton Road, Hoddesdon",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"Rawmec Business Park, Plumpton Road, Hoddesdon");
	
	LCD_MeasureString("深圳市龙华观澜环观南路凯美广场A座",&w,&h);
	x = (w > COL)? 0 : (COL-w)/2;
	y = y + h + 10;	
	LCD_ShowString(x,y,"深圳市龙华观澜环观南路凯美广场A座");
	
	LCD_SetFontSize(FONT_SIZE_32);					//设置字体大小
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

	gpio_output_voltage_setup_3v3();						//设置GPIO输出电压为3.3V

	//按键初始化
	//key_init();
	//LCD初始化
	//LCD_Init();	
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

	system_init();

	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,6)); 							//配置P0.6为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,8)); 							//配置P0.8为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,13)); 							//配置P0.13为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,24)); 							//配置P0.24为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,26));							//配置P0.26为输出

	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,1));								//配置P1.1为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,3));								//配置P1.3为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,5));								//配置P1.5为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,7));								//配置P1.7为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,9));								//配置P1.9为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,10));							//配置P1.10为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,12));							//配置P1.12为输出
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(1,14));							//配置P1.14为输出
	
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,6));								//引脚P0.6为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,8));   								//引脚P0.8为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(0,26));   							//引脚P0.26为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,1));   								//引脚P1.1为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,3));								//引脚P1.3为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,5));								//引脚P1.5为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,7));								//引脚P1.7为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,9));								//引脚P1.9为输出高电平
	nrf_gpio_pin_set(NRF_GPIO_PIN_MAP(1,14));								//引脚P1.14为输出高电平
	
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0,13));								//引脚P0.13为输出低电平
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0,24));								//引脚P0.24为输出低电平
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1,10));								//引脚P1.10为输出低电平
	nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(1,12));								//引脚P1.12为输出低电平

	//bsp_board_init(BSP_INIT_LEDS|BSP_INIT_BUTTONS);
	
//	test_show_string();										//字符显示测试

	while(true)
	{
		power_manage();
	}

}
/********************************************END FILE**************************************/

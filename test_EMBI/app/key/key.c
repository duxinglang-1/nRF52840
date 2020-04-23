#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "boards.h"
#include "app_button.h"
#include "app_gpiote.h"
#include "app_timer.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_clock.h"
#include "key.h"

#define TIMER_FOR_LONG_PRESSED 500
#define BUTTON_DEBOUNCE_DELAY  50 // Delay from a GPIOTE event until a button is reported as pushed. 

#define KEY_DOWN 		APP_BUTTON_PUSH
#define KEY_UP 			APP_BUTTON_RELEASE
#define KEY_LONG_PRESS	2

#define KEY_1 NRF_GPIO_PIN_MAP(0,11)
#define KEY_2 NRF_GPIO_PIN_MAP(0,12)
#define KEY_3 NRF_GPIO_PIN_MAP(0,24)
#define KEY_4 NRF_GPIO_PIN_MAP(0,25)

extern bool lcd_sleep_in;
extern bool lcd_sleep_out;
extern bool lcd_is_sleeping;

static void button_handler(uint8_t pin_no, uint8_t button_action);

APP_TIMER_DEF(g_long_press_timer_id);  /**< Polling timer id. */

app_button_cfg_t p_button[] = 
	{
		{KEY_1, APP_BUTTON_ACTIVE_LOW, BUTTON_PULL, button_handler},
		{KEY_2, APP_BUTTON_ACTIVE_LOW, BUTTON_PULL, button_handler},
		{KEY_3, APP_BUTTON_ACTIVE_LOW, BUTTON_PULL, button_handler},
		{KEY_4, APP_BUTTON_ACTIVE_LOW, BUTTON_PULL, button_handler}
	};

static void key_event_handler(uint8_t key_code, uint8_t key_type)
{
	switch(key_code)
	{
	case KEY_1:
		switch(key_type)
		{
		case KEY_DOWN:
			nrf_gpio_pin_clear(LED_1);	
			break;
		case KEY_UP:
			nrf_gpio_pin_set(LED_1);
			break;
		case KEY_LONG_PRESS:
			break;
		}
		break;

	case KEY_2:
		switch(key_type)
		{
		case KEY_DOWN:
			nrf_gpio_pin_clear(LED_2);	
			break;
		case KEY_UP:
			nrf_gpio_pin_set(LED_2);
			break;
		case KEY_LONG_PRESS:
			break;
		}		
		break;

	case KEY_3:
		switch(key_type)
		{
		case KEY_DOWN:
			nrf_gpio_pin_clear(LED_3);	
			break;
		case KEY_UP:
			nrf_gpio_pin_set(LED_3);
			break;
		case KEY_LONG_PRESS:
			break;
		}		
		break;

	case KEY_4:
		switch(key_type)
		{
		case KEY_DOWN:
			nrf_gpio_pin_clear(LED_4);	
			break;
		case KEY_UP:
			nrf_gpio_pin_set(LED_4);
			break;
		case KEY_LONG_PRESS:
			break;
		}
		break;		
	}

	if(key_type == KEY_UP)
	{
		if(lcd_is_sleeping)
			lcd_sleep_out = 1;
		else
			lcd_sleep_in = 1;
	}
}

static void button_handler(uint8_t pin_no, uint8_t button_action)
{
	uint32_t err_code;

	switch(button_action)
	{
	case KEY_DOWN:
		err_code = app_timer_start(g_long_press_timer_id,APP_TIMER_TICKS(TIMER_FOR_LONG_PRESSED),&pin_no);
		APP_ERROR_CHECK(err_code);
		break;

	case KEY_UP:
		err_code = app_timer_stop(g_long_press_timer_id);
		APP_ERROR_CHECK(err_code);
		break;

	case KEY_LONG_PRESS:
		break;
		
	default:
		break;
	}

	key_event_handler(pin_no, button_action);
}

static void long_press_timer_handler(void * p_context)
{
    button_handler(*(uint8_t *)p_context, KEY_LONG_PRESS);
}


void key_init(void)
{
	uint32_t err_code;
	
	err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);
	
	err_code = app_button_init((app_button_cfg_t *)p_button, ARRAY_SIZE(p_button), APP_TIMER_TICKS(BUTTON_DEBOUNCE_DELAY));
	APP_ERROR_CHECK(err_code);
	
	err_code = app_button_enable();
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&g_long_press_timer_id,	APP_TIMER_MODE_SINGLE_SHOT,	long_press_timer_handler);	
	APP_ERROR_CHECK(err_code);
}



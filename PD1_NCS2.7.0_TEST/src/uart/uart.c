/*
* Copyright (c) 2019 Nordic Semiconductor ASA
*
* SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
*/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/types.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/pm/device.h>
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "transfer_cache.h"
#include "datetime.h"
#include "Settings.h"
#include "Uart.h"
#include "inner_flash.h"

//#define UART_DEBUG

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart0), okay)
#define LTE_DEV DT_NODELABEL(uart0)
#else
#error "uart0 devicetree node is disabled"
#define LTE_DEV	""
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
#define GPS_DEV DT_NODELABEL(uart1)
#else
#error "uart1 devicetree node is disabled"
#define GPS_DEV	""
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio0), okay)
#define LTE_PORT DT_NODELABEL(gpio0)
#else
#error "gpio0 devicetree node is disabled"
#define LTE_PORT	""
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
#define CTR_PORT DT_NODELABEL(gpio1)
#else
#error "gpio0 devicetree node is disabled"
#define CTR_PORT	""
#endif


#define LTE_INT_PIN			3
#define GPS_WIFI_SWITCH_PIN	4

#define BUF_MAXSIZE	2048


#ifdef CONFIG_PM_DEVICE
bool uart_lte_sleep_flag = false;
bool uart_lte_wake_flag = false;
bool uart_lte_is_waked = true;
#define UART_LTE_WAKE_HOLD_TIME_SEC		(60)

bool uart_gps_sleep_flag = false;
bool uart_gps_wake_flag = false;
bool uart_gps_is_waked = true;
#define UART_GPS_WAKE_HOLD_TIME_SEC		(5)
#endif

static bool uart_lte_send_data_flag = false;
static bool uart_lte_rece_data_flag = false;
static bool uart_lte_rece_frame_flag = false;
static bool uart_gps_send_data_flag = false;
static bool uart_gps_rece_data_flag = false;
static bool uart_gps_rece_frame_flag = false;
static bool uart_is_gps_data = false;		//gps和wifi共用一组uart，通过开关切换，ctr=0, wifi; ctr=1, gps

static CacheInfo uart_lte_send_cache = {0};
static CacheInfo uart_lte_rece_cache = {0};
static CacheInfo uart_gps_send_cache = {0};
static CacheInfo uart_gps_rece_cache = {0};

static uint32_t lte_rece_len=0;
static uint32_t lte_send_len=0;
static uint32_t gps_rece_len=0;
static uint32_t gps_send_len=0;

static uint8_t lte_rx_buf[BUF_MAXSIZE]={0};
static uint8_t gps_rx_buf[BUF_MAXSIZE]={0};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static struct device *uart_lte = NULL;
static struct device *uart_gps = NULL;

static struct device *gpio_lte = NULL;
static struct device *gpio_ctr = NULL;

static struct gpio_callback gpio_cb;

static struct uart_data_t
{
	void  *fifo_reserved;
	uint8_t    data[BUF_MAXSIZE];
	uint16_t   len;
};

static void UartLteSendDataCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_lte_send_data_timer, UartLteSendDataCallBack, NULL);
static void UartLteReceDataCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_lte_rece_data_timer, UartLteReceDataCallBack, NULL);
static void UartLteReceFrameCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_lte_rece_frame_timer, UartLteReceFrameCallBack, NULL);
static void UartGpsSendDataCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_gps_send_data_timer, UartGpsSendDataCallBack, NULL);
static void UartGpsReceDataCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_gps_rece_data_timer, UartGpsReceDataCallBack, NULL);
static void UartGpsReceFrameCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_gps_rece_frame_timer, UartGpsReceFrameCallBack, NULL);

#ifdef CONFIG_PM_DEVICE
static void UartLteSleepInCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_lte_sleep_in_timer, UartLteSleepInCallBack, NULL);
static void UartGpsSleepInCallBack(struct k_timer *timer_id);
K_TIMER_DEFINE(uart_gps_sleep_in_timer, UartGpsSleepInCallBack, NULL);
#endif

void UartSetGpsBaudRate(uint32_t baudrate)
{
	struct uart_config uart_cfg;

	uart_config_get(uart_gps, &uart_cfg);
	uart_cfg.baudrate = baudrate;
	uart_configure(uart_gps, &uart_cfg);
}

void UartSetLteBaudRate(uint32_t baudrate)
{
	int ret;
	struct uart_config uart_cfg;

	uart_config_get(uart_lte, &uart_cfg);
	uart_cfg.baudrate = baudrate;
	uart_configure(uart_lte, &uart_cfg);
}

void UartSwitchToGps(void)
{
#ifdef CONFIG_PM_DEVICE	
	if(k_timer_remaining_get(&uart_gps_sleep_in_timer) > 0)
		k_timer_stop(&uart_gps_sleep_in_timer);

	uart_sleep_out(uart_gps);
#endif

	//ZKW GNSS defaults to a baud rate of 9600
	UartSetGpsBaudRate(9600);

	gpio_pin_set(gpio_ctr, GPS_WIFI_SWITCH_PIN, UART_DATA_GPS);
	uart_is_gps_data = true;
}

void UartSwitchToWifi(void)
{
#ifdef CONFIG_PM_DEVICE	
	if(k_timer_remaining_get(&uart_gps_sleep_in_timer) > 0)
		k_timer_stop(&uart_gps_sleep_in_timer);

	uart_sleep_out(uart_gps);
#endif

	UartSetGpsBaudRate(115200);

	gpio_pin_set(gpio_ctr, GPS_WIFI_SWITCH_PIN, UART_DATA_WIFI);
	uart_is_gps_data = false;
}

static void uart_send_data_handle(struct device *dev, uint8_t *buffer, uint32_t datalen)
{
#ifdef CONFIG_PM_DEVICE
	uart_sleep_out(dev);
#endif

#ifdef UART_DEBUG
	LOGD("len:%d, data:%s", datalen, buffer);
#endif

	uart_fifo_fill(dev, buffer, datalen);
	uart_irq_tx_enable(dev); 	
}

static void uart_receive_data_handle(struct device *dev, uint8_t *data, uint32_t datalen)
{
	uint32_t data_len = 0;

	if(dev == uart_lte)
	{
	#ifdef UART_DEBUG
		LOGD("uart for lte!");
	#endif
		lte_receive_data_handle(data, datalen);
	}
	else if(dev == uart_gps)
	{
		if(uart_is_gps_data)
		{//gps data
		#ifdef UART_DEBUG
			LOGD("uart for gps!");
		#endif
			gps_receive_data_handle(data, datalen);
		}
		else
		{//wifi data
		#ifdef UART_DEBUG
			LOGD("uart for wifi!");
		#endif
			wifi_receive_data_handle(data, datalen);
		}
	}
}

void UartLteSendData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&uart_lte_send_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
		uart_send_data_handle(uart_lte, p_data, data_len);
		delete_data_from_cache(&uart_lte_send_cache);
		k_timer_start(&uart_lte_send_data_timer, K_MSEC(50), K_NO_WAIT);
	}
}

void UartLteSendDataStart(void)
{
	k_timer_start(&uart_lte_send_data_timer, K_MSEC(50), K_NO_WAIT);
}

bool LteSendCacheIsEmpty(void)
{
	if(cache_is_empty(&uart_lte_send_cache))
		return true;
	else
		return false;
}

void LteSendData(uint8_t *data, uint32_t datalen)
{
	int ret;

	ret = add_data_into_cache(&uart_lte_send_cache, data, datalen, DATA_TRANSFER);
	UartLteSendDataStart();
}

void UartLteReceData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&uart_lte_rece_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
		uart_receive_data_handle(uart_lte, p_data, data_len);
		delete_data_from_cache(&uart_lte_rece_cache);
		k_timer_start(&uart_lte_rece_data_timer, K_MSEC(50), K_NO_WAIT);
	}
}

void LteReceDataStart(void)
{
	k_timer_start(&uart_lte_rece_data_timer, K_MSEC(50), K_NO_WAIT);
}

bool LteReceCacheIsEmpty(void)
{
	if(cache_is_empty(&uart_lte_rece_cache))
		return true;
	else
		return false;
}

void LteReceData(uint8_t *data, uint32_t datalen)
{
	int ret;

	ret = add_data_into_cache(&uart_lte_rece_cache, data, datalen, DATA_TRANSFER);
	LteReceDataStart();
}

static void uart_lte_cb(struct device *x)
{
	uint8_t tmpbyte = 0;
	uint32_t len=0;

	uart_irq_update(x);

	if(uart_irq_rx_ready(x)) 
	{
		if(lte_rece_len >= BUF_MAXSIZE)
			lte_rece_len = 0;

		while((len = uart_fifo_read(x, &lte_rx_buf[lte_rece_len], BUF_MAXSIZE-lte_rece_len)) > 0)
		{
			lte_rece_len += len;
			k_timer_start(&uart_lte_rece_frame_timer, K_MSEC(20), K_NO_WAIT);
		}
	}
	
	if(uart_irq_tx_ready(x))
	{
		struct uart_data_t *buf;
		uint16_t written = 0;

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		/* Nothing in the FIFO, nothing to send */
		if(!buf)
		{
			uart_irq_tx_disable(x);
			return;
		}

		while(buf->len > written)
		{
			written += uart_fifo_fill(x, &buf->data[written], buf->len - written);
		}

		while (!uart_irq_tx_complete(x))
		{
			/* Wait for the last byte to get
			* shifted out of the module
			*/
		}

		if (k_fifo_is_empty(&fifo_uart_tx_data))
		{
			uart_irq_tx_disable(x);
		}

		k_free(buf);
	}
}

void UartGpsSendData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&uart_gps_send_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
	#ifdef UART_DEBUG
		LOGD("begin");
	#endif
		uart_send_data_handle(uart_gps, p_data, data_len);
		delete_data_from_cache(&uart_gps_send_cache);
		k_timer_start(&uart_gps_send_data_timer, K_MSEC(50), K_NO_WAIT);
	}
}

void UartGpsSendDataStart(void)
{
	k_timer_start(&uart_gps_send_data_timer, K_MSEC(50), K_NO_WAIT);
}

bool GpsSendCacheIsEmpty(void)
{
	if(cache_is_empty(&uart_gps_send_cache))
		return true;
	else
		return false;
}

void GpsSendData(uint8_t *data, uint32_t datalen)
{
	int ret;

	ret = add_data_into_cache(&uart_gps_send_cache, data, datalen, DATA_TRANSFER);
	UartGpsSendDataStart();
}

void UartGpsReceData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&uart_gps_rece_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
		uart_receive_data_handle(uart_gps, p_data, data_len);
		delete_data_from_cache(&uart_gps_rece_cache);
		k_timer_start(&uart_gps_rece_data_timer, K_MSEC(50), K_NO_WAIT);
	}
}

void GpsReceDataStart(void)
{
	k_timer_start(&uart_gps_rece_data_timer, K_MSEC(50), K_NO_WAIT);
}

bool GpsReceCacheIsEmpty(void)
{
	if(cache_is_empty(&uart_gps_rece_cache))
		return true;
	else
		return false;
}

void GpsReceData(uint8_t *data, uint32_t datalen)
{
	int ret;

	ret = add_data_into_cache(&uart_gps_rece_cache, data, datalen, DATA_TRANSFER);
	GpsReceDataStart();
}

static void uart_gps_cb(struct device *x)
{
	uint8_t tmpbyte = 0;
	uint32_t len=0;

	uart_irq_update(x);

	if(uart_irq_rx_ready(x)) 
	{
		if(gps_rece_len >= BUF_MAXSIZE)
			gps_rece_len = 0;

		while((len = uart_fifo_read(x, &gps_rx_buf[gps_rece_len], BUF_MAXSIZE-gps_rece_len)) > 0)
		{
			gps_rece_len += len;
			k_timer_start(&uart_gps_rece_frame_timer, K_MSEC(20), K_NO_WAIT);
		}
	}
	
	if(uart_irq_tx_ready(x))
	{
		struct uart_data_t *buf;
		uint16_t written = 0;

		buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
		/* Nothing in the FIFO, nothing to send */
		if(!buf)
		{
			uart_irq_tx_disable(x);
			return;
		}

		while(buf->len > written)
		{
			written += uart_fifo_fill(x, &buf->data[written], buf->len - written);
		}

		while (!uart_irq_tx_complete(x))
		{
			/* Wait for the last byte to get
			* shifted out of the module
			*/
		}

		if (k_fifo_is_empty(&fifo_uart_tx_data))
		{
			uart_irq_tx_disable(x);
		}

		k_free(buf);
	}
}

#ifdef CONFIG_PM_DEVICE
void uart_sleep_out(struct device *dev)
{
	if(dev == uart_lte)
	{
		if(k_timer_remaining_get(&uart_lte_sleep_in_timer) > 0)
			k_timer_stop(&uart_lte_sleep_in_timer);
		k_timer_start(&uart_lte_sleep_in_timer, K_SECONDS(UART_LTE_WAKE_HOLD_TIME_SEC), K_NO_WAIT);

		if(uart_lte_is_waked)
			return;
		uart_lte_is_waked = true;
	}
	else if(dev == uart_gps)
	{		
		if(uart_gps_is_waked)
			return;
		uart_gps_is_waked = true;
	}
	
	pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);

#ifdef UART_DEBUG
	LOGD("uart set active success!");
#endif
}

void uart_sleep_in(struct device *dev)
{	
	if(dev == uart_lte)
	{
		if(!uart_lte_is_waked)
			return;
		uart_lte_is_waked = false;
	}
	else if(dev == uart_gps)
	{
		if(!uart_gps_is_waked)
			return;
		uart_gps_is_waked = false;
	}
	
	pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);

#ifdef UART_DEBUG
	LOGD("uart set low power success!");
#endif
}

static void lte_interrupt_event(struct device *interrupt, struct gpio_callback *cb, uint32_t pins)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif
	uart_lte_wake_flag = true;
}

static void UartLteSleepInCallBack(struct k_timer *timer_id)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif
	uart_lte_sleep_flag = true;
}

static void UartGpsSleepInCallBack(struct k_timer *timer_id)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif
	uart_gps_sleep_flag = true;
}

#endif

static void UartLteSendDataCallBack(struct k_timer *timer)
{
	uart_lte_send_data_flag = true;
}

static void UartLteReceDataCallBack(struct k_timer *timer_id)
{
	uart_lte_rece_data_flag = true;
}

static void UartLteReceFrameCallBack(struct k_timer *timer_id)
{
	uart_lte_rece_frame_flag = true;
}

static void UartGpsSendDataCallBack(struct k_timer *timer)
{
	uart_gps_send_data_flag = true;
}

static void UartGpsReceDataCallBack(struct k_timer *timer_id)
{
	uart_gps_rece_data_flag = true;
}

static void UartGpsReceFrameCallBack(struct k_timer *timer_id)
{
	uart_gps_rece_frame_flag = true;
}

void UartGpsOff(void)
{
	if(uart_is_gps_data)
	{
		if(k_timer_remaining_get(&uart_gps_send_data_timer) > 0)
			k_timer_stop(&uart_gps_send_data_timer);
		delete_all_from_cache(&uart_gps_send_cache);
	}

#ifdef CONFIG_PM_DEVICE
	uart_sleep_in(uart_gps);
#endif
}

void UartWifiOff(void)
{
	if(!uart_is_gps_data)
	{
		if(k_timer_remaining_get(&uart_gps_send_data_timer) > 0)
			k_timer_stop(&uart_gps_send_data_timer);
		delete_all_from_cache(&uart_gps_send_cache);
	}
	
#ifdef CONFIG_PM_DEVICE
	uart_sleep_in(uart_gps);
#endif
}

void UartLteOff(void)
{
	if(k_timer_remaining_get(&uart_lte_send_data_timer) > 0)
		k_timer_stop(&uart_lte_send_data_timer);
	delete_all_from_cache(&uart_lte_send_cache);
	
#ifdef CONFIG_PM_DEVICE
	uart_sleep_in(uart_lte);
#endif
}

void uart_lte_init(void)
{
	gpio_flags_t flag = GPIO_INPUT|GPIO_PULL_UP;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	uart_lte = DEVICE_DT_GET(LTE_DEV);
	if(!uart_lte)
	{
	#ifdef UART_DEBUG
		LOGD("Could not get uart!");
	#endif
		return;
	}

	uart_irq_callback_set(uart_lte, uart_lte_cb);
	uart_irq_rx_enable(uart_lte);

	gpio_lte = DEVICE_DT_GET(LTE_PORT);
	if(!gpio_lte)
	{
	#ifdef UART_DEBUG
		LOGD("Could not get gpio!");
	#endif
		return;
	}	

#ifdef CONFIG_PM_DEVICE
	gpio_pin_configure(gpio_lte, LTE_INT_PIN, flag);
	gpio_pin_interrupt_configure(gpio_lte, LTE_INT_PIN, GPIO_INT_DISABLE);
	gpio_init_callback(&gpio_cb, lte_interrupt_event, BIT(LTE_INT_PIN));
	gpio_add_callback(gpio_lte, &gpio_cb);
	gpio_pin_interrupt_configure(gpio_lte, LTE_INT_PIN, GPIO_INT_ENABLE|GPIO_INT_EDGE_FALLING);	
	k_timer_start(&uart_lte_sleep_in_timer, K_SECONDS(UART_LTE_WAKE_HOLD_TIME_SEC), K_NO_WAIT);
#endif
}

void uart_gps_init(void)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif

	if(uart_gps == NULL)
	{
		uart_gps = DEVICE_DT_GET(GPS_DEV);
		uart_irq_callback_set(uart_gps, uart_gps_cb);
		uart_irq_rx_enable(uart_gps);

		gpio_ctr = DEVICE_DT_GET(CTR_PORT);
		gpio_pin_configure(gpio_ctr, GPS_WIFI_SWITCH_PIN, GPIO_OUTPUT);
		gpio_pin_set(gpio_ctr, GPS_WIFI_SWITCH_PIN, UART_DATA_WIFI);//default wifi 

	#ifdef CONFIG_PM_DEVICE
		k_timer_start(&uart_gps_sleep_in_timer, K_SECONDS(UART_GPS_WAKE_HOLD_TIME_SEC), K_NO_WAIT);
	#endif
	}
}

void uart_wifi_init(void)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif

	if(uart_gps == NULL)
	{
		uart_gps = DEVICE_DT_GET(GPS_DEV);
		uart_irq_callback_set(uart_gps, uart_gps_cb);
		uart_irq_rx_enable(uart_gps);

		gpio_ctr = DEVICE_DT_GET(CTR_PORT);
		gpio_pin_configure(gpio_ctr, GPS_WIFI_SWITCH_PIN, GPIO_OUTPUT);
		gpio_pin_set(gpio_ctr, GPS_WIFI_SWITCH_PIN, UART_DATA_WIFI);//default wifi 

	#ifdef CONFIG_PM_DEVICE
		k_timer_start(&uart_gps_sleep_in_timer, K_SECONDS(UART_GPS_WAKE_HOLD_TIME_SEC), K_NO_WAIT);
	#endif
	}
}

void UartMsgProc(void)
{
#ifdef CONFIG_PM_DEVICE
	if(uart_lte_wake_flag)
	{
		uart_lte_wake_flag = false;
		uart_sleep_out(uart_lte);
	}

	if(uart_lte_sleep_flag)
	{
		uart_lte_sleep_flag = false;
		uart_sleep_in(uart_lte);
	}

	if(uart_gps_wake_flag)
	{
		uart_gps_wake_flag = false;
		uart_sleep_out(uart_gps);
	}

	if(uart_gps_sleep_flag)
	{
		uart_gps_sleep_flag = false;
		uart_sleep_in(uart_gps);
	}
#endif/*CONFIG_PM_DEVICE*/

	if(uart_lte_send_data_flag)
	{
		UartLteSendData();
		uart_lte_send_data_flag = false;
	}

	if(uart_lte_rece_data_flag)
	{
		UartLteReceData();
		uart_lte_rece_data_flag = false;
	}
	
	if(uart_lte_rece_frame_flag)
	{
		LteReceData(lte_rx_buf, lte_rece_len);
		memset(lte_rx_buf, 0, sizeof(lte_rx_buf));
		lte_rece_len = 0;
		uart_lte_rece_frame_flag = false;
	}

	if(uart_gps_send_data_flag)
	{
		UartGpsSendData();
		uart_gps_send_data_flag = false;
	}

	if(uart_gps_rece_data_flag)
	{
		UartGpsReceData();
		uart_gps_rece_data_flag = false;
	}
	
	if(uart_gps_rece_frame_flag)
	{
		GpsReceData(gps_rx_buf, gps_rece_len);
		memset(gps_rx_buf, 0, sizeof(gps_rx_buf));
		gps_rece_len = 0;
		uart_gps_rece_frame_flag = false;
	}
}

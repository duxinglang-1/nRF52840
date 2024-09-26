/*
* Copyright (c) 2019 Nordic Semiconductor ASA
*
* SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>
#include <power/reboot.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <dk_buttons_and_leds.h>
#include "datetime.h"
#include "inner_flash.h"
#include "external_flash.h"
#include "uart_ble.h"
#include "settings.h"
#include "pmu.h"
#include "codetrans.h"
#ifdef CONFIG_WATCHDOG
#include "watchdog.h"
#endif
#include "logger.h"

static bool sys_pwron_completed_flag = false;

static void modem_init(void)
{
	//nrf_modem_lib_init(NORMAL_MODE);
	//boot_write_img_confirmed();
}

void system_init(void)
{
	k_sleep(K_MSEC(500));//xb test 2022-03-11 启动时候延迟0.5S,等待其他外设完全启动

	modem_init();

	InitSystemSettings();

#ifdef CONFIG_PPG_SUPPORT
	//PPG_i2c_off();
#endif
	pmu_init();
	key_init();
	//flash_init();
	
#ifdef CONFIG_PPG_SUPPORT	
	//PPG_init();
#endif
#ifdef CONFIG_ECG_SUPPORT
	//ECG_init();
#endif
	//ble_init();
	//LogInit();
}

void work_init(void)
{
	if(IS_ENABLED(CONFIG_WATCHDOG))
	{
		watchdog_init_and_start(&k_sys_work_q);
	}
}

bool system_is_completed(void)
{
	return sys_pwron_completed_flag;
}

void system_init_completed(void)
{
	if(!sys_pwron_completed_flag)
		sys_pwron_completed_flag = true;
}

/***************************************************************************
* 描  述 : main函数 
* 入  参 : 无 
* 返回值 : int 类型
**************************************************************************/
int main(void)
{
	LOGD("begin");

	work_init();
	system_init();
	
//	test_show_string();
//	test_show_image();
//	test_show_color();
//	test_show_stripe();
//	test_nvs();
//	test_flash();
//	test_uart_ble();
//	test_sensor();
//	test_show_digital_clock();
//	test_sensor();
//	test_pmu();
//	test_crypto();
//	test_imei_for_qr();
//	test_tp();
//	test_gps_on();
//	test_nb();
//	test_i2c();
//	test_bat_soc();
//	test_notify();
//	test_wifi();
//	LogInit();

	while(1)
	{
		KeyMsgProcess();
		TimeMsgProcess();
		PMUMsgProcess();
	#ifdef CONFIG_PPG_SUPPORT	
		PPGMsgProcess();
	#endif
	#ifdef CONFIG_ECG_SUPPORT
		ECGMsgProcess();
	#endif
		SettingsMsgPorcess();
		UartMsgProc();
	#ifdef CONFIG_FACTORY_TEST_SUPPORT
		FactoryTestProccess();
	#endif
		LogMsgProcess();
		system_init_completed();
		k_cpu_idle();
	}
}

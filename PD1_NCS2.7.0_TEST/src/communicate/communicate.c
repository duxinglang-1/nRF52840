/****************************************Copyright (c)************************************************
** File Name:			    communicate.c
** Descriptions:			communicate source file
** Created By:				xie biao
** Created Date:			2021-04-28
** Modified Date:      		2021-04-28 
** Version:			    	V1.0
******************************************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "settings.h"
#include "external_flash.h"
#include "uart_ble.h"
#include "datetime.h"
#include "communicate.h"
#include "logger.h"

extern uint16_t g_last_steps;

void TimeCheckSendWristOffData(void)
{
	uint8_t reply[8] = {0};

	if(CheckSCC())
		strcpy(reply, "1");
	else
		strcpy(reply, "0");
	
	//NBSendTimelyWristOffData(reply, strlen(reply));
}

/*****************************************************************************
 * FUNCTION
 *  SendPowerOnData
 * DESCRIPTION
 *  发送开机数据包
 * PARAMETERS
 *	Nothing
 * RETURNS
 *  Nothing
 *****************************************************************************/
void SendPowerOnData(void)
{
	uint8_t tmpbuf[10] = {0};
	uint8_t reply[256] = {0};

	//imsi
	strcpy(reply, ",");
	
	//iccid
	strcat(reply, ",");

	//nb rsrp
	strcat(reply, ",");
	
	//time zone
	strcat(reply, ",");
	
	//battery
	GetBatterySocString(tmpbuf);
	strcat(reply, tmpbuf);
	strcat(reply, ",");

	//mcu fw version
	strcat(reply, g_fw_version);	
	strcat(reply, ",");

	//modem fw version
	strcat(reply, ",");

	//ppg algo
#ifdef CONFIG_PPG_SUPPORT	
	strcat(reply, g_ppg_ver);
#endif
	strcat(reply, ",");

	//wifi version
	strcat(reply, ",");

	//wifi mac
	strcat(reply, ",");

	//ble version
	strcat(reply, &g_nrf52810_ver[15]);	
	strcat(reply, ",");

	//ble mac
	strcat(reply, g_ble_mac_addr);	

	//NBSendPowerOnInfor(reply, strlen(reply));
}

/*****************************************************************************
 * FUNCTION
 *  SendPowerOffData
 * DESCRIPTION
 *  发送关机数据包
 * PARAMETERS
 *	pwroff_mode			[IN]		关机模式 1:低电关机 2:按键关机 3:重启关机 
 * RETURNS
 *  Nothing
 *****************************************************************************/
void SendPowerOffData(uint8_t pwroff_mode)
{
	uint8_t tmpbuf[10] = {0};
	uint8_t reply[128] = {0};

	//pwr off mode
	sprintf(reply, "%d,", pwroff_mode);
	
	//nb rsrp
	sprintf(tmpbuf, "%d,", 0);
	strcat(reply, tmpbuf);
	
	//battery
	GetBatterySocString(tmpbuf);
	strcat(reply, tmpbuf);
			
	//NBSendPowerOffInfor(reply, strlen(reply));
}

/*****************************************************************************
 * FUNCTION
 *  SendSettingsData
 * DESCRIPTION
 *  发送终端设置项数据包
 * PARAMETERS
 *	Nothing
 * RETURNS
 *  Nothing
 *****************************************************************************/
void SendSettingsData(void)
{
	uint8_t tmpbuf[10] = {0};
	uint8_t reply[128] = {0};

	//temp uint
	sprintf(reply, "%d,", global_settings.temp_unit);
	
	//language
	switch(global_settings.language)
	{
#ifndef FW_FOR_CN
	case LANGUAGE_EN:	//English
		strcat(reply, "en");
		break;
	case LANGUAGE_DE:	//Deutsch
		strcat(reply, "de");
		break;
	case LANGUAGE_FR:	//French
		strcat(reply, "fr");
		break;
	case LANGUAGE_ITA:	//Italian
		strcat(reply, "it");
		break;
	case LANGUAGE_ES:	//Spanish
		strcat(reply, "es");
		break;
	case LANGUAGE_PT:	//Portuguese
		strcat(reply, "pt");
		break;
#else
	case LANGUAGE_CHN:	//Chinese
		strcat(reply, "zh");
		break;
	case LANGUAGE_EN:	//English
		strcat(reply, "en");
		break;
#endif	
	}
	
	//NBSendSettingsData(reply, strlen(reply));
}

/****************************************Copyright (c)************************************************
** File Name:			    ble.c
** Descriptions:			ble function main source file
** Created By:				xie biao
** Created Date:			2024-09-29
** Modified Date:      		2024-09-29
** Version:			    	V1.0
******************************************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include "ble.h"
#include "transfer_cache.h"
#include "datetime.h"
#include "Settings.h"
#include "pmu.h"
#include "inner_flash.h"
#include "logger.h"


#define BUF_MAXSIZE	2048

#define PACKET_HEAD	0xAB
#define PACKET_END	0x88

#define BLE_WORK_MODE_ID		0xFF10			//52810工作状态正常
#define HEART_RATE_ID			0xFF31			//心率
#define BLOOD_OXYGEN_ID			0xFF32			//血氧
#define BLOOD_PRESSURE_ID		0xFF33			//血压
#define TEMPERATURE_ID			0xFF62			//体温
#define	ONE_KEY_MEASURE_ID		0xFF34			//一键测量
#define	PULL_REFRESH_ID			0xFF35			//下拉刷新
#define	SLEEP_DETAILS_ID		0xFF36			//睡眠详情
#define	FIND_DEVICE_ID			0xFF37			//查找手环
#define SMART_NOTIFY_ID			0xFF38			//智能提醒
#define	ALARM_SETTING_ID		0xFF39			//闹钟设置
#define USER_INFOR_ID			0xFF40			//用户信息
#define	SEDENTARY_ID			0xFF41			//久坐提醒
#define	SHAKE_SCREEN_ID			0xFF42			//抬手亮屏
#define	MEASURE_HOURLY_ID		0xFF43			//整点测量设置
#define	SHAKE_PHOTO_ID			0xFF44			//摇一摇拍照
#define	LANGUAGE_SETTING_ID		0xFF45			//中英日文切换
#define	TIME_24_SETTING_ID		0xFF46			//12/24小时设置
#define	FIND_PHONE_ID			0xFF47			//查找手机回复
#define	WEATHER_INFOR_ID		0xFF48			//天气信息下发
#define	TIME_SYNC_ID			0xFF49			//时间同步
#define	TARGET_STEPS_ID			0xFF50			//目标步数
#define	BATTERY_LEVEL_ID		0xFF51			//电池电量
#define	FIRMWARE_INFOR_ID		0xFF52			//固件版本号
#define	FACTORY_RESET_ID		0xFF53			//清除手环数据
#define	ECG_ID					0xFF54			//心电
#define	LOCATION_ID				0xFF55			//获取定位信息
#define	DATE_FORMAT_ID			0xFF56			//年月日格式设置
#define NOTIFY_CONTENT_ID		0xFF57			//智能提醒内容
#define CHECK_WHITELIST_ID		0xFF58			//判断手机ID是否在手环白名单
#define INSERT_WHITELIST_ID`	0xFF59			//将手机ID插入白名单
#define DEVICE_SEND_128_RAND_ID	0xFF60			//手环发送随机的128位随机数
#define PHONE_SEND_128_AES_ID	0xFF61			//手机发送AES 128 CBC加密数据给手环

#define	BLE_CONNECT_ID			0xFFB0			//BLE断连提醒
#define	CTP_NOTIFY_ID			0xFFB1			//CTP触屏消息
#define GET_NRF52810_VER_ID		0xFFB2			//获取52810版本号
#define GET_BLE_MAC_ADDR_ID		0xFFB3			//获取BLE MAC地址
#define GET_BLE_STATUS_ID		0xFFB4			//获取BLE当前工作状态	0:关闭 1:休眠 2:广播 3:连接
#define SET_BEL_WORK_MODE_ID	0xFFB5			//设置BLE工作模式		0:关闭 1:打开 2:唤醒 3:休眠

bool blue_is_on = true;
bool get_ble_info_flag = false;
sys_date_timer_t refresh_time = {0};
static bool redraw_blt_status_flag = false;
static ENUM_BLE_WORK_MODE ble_work_mode=BLE_WORK_NORMAL;

bool g_ble_connected = false;

static bool reply_cur_data_flag = false;

uint8_t g_ble_mac_addr[20] = {0};
uint8_t g_nrf52810_ver[128] = {0};

ENUM_BLE_STATUS g_ble_status = BLE_STATUS_BROADCAST;
ENUM_BLE_MODE g_ble_mode = BLE_MODE_TURN_OFF;

extern bool app_find_device;

void BleSendData(uint8_t *buf, uint32_t len)
{
}

#ifdef CONFIG_BLE_SUPPORT
void ble_connect_or_disconnect_handle(uint8_t *buf, uint32_t len)
{
#ifdef UART_DEBUG
	LOGD("BLE status:%x", buf[6]);
#endif

	if(buf[6] == 0x01)				//查看control值
		g_ble_connected = true;
	else if(buf[6] == 0x00)
		g_ble_connected = false;
	else
		g_ble_connected = false;

	redraw_blt_status_flag = true;
}

#ifdef CONFIG_ALARM_SUPPORT
void APP_reply_find_phone(uint8_t *buf, uint32_t len)
{
	uint32_t i;

#ifdef UART_DEBUG
	LOGD("begin");
#endif
}

void APP_set_find_device(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG	
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (FIND_DEVICE_ID>>8);		
	reply[reply_len++] = (uint8_t)(FIND_DEVICE_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	app_find_device = true;	
}
#endif

void APP_set_language(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	if(buf[7] == 0x00)
		global_settings.language = LANGUAGE_CHN;
	else if(buf[7] == 0x01)
		global_settings.language = LANGUAGE_EN;
#ifndef FW_FOR_CN	
	else if(buf[7] == 0x02)
		global_settings.language = LANGUAGE_DE;
#endif	
	else
		global_settings.language = LANGUAGE_EN;
		
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (LANGUAGE_SETTING_ID>>8);		
	reply[reply_len++] = (uint8_t)(LANGUAGE_SETTING_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;
}

void APP_set_time_24_format(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	if(buf[7] == 0x00)
		global_settings.time_format = TIME_FORMAT_24;//24 format
	else if(buf[7] == 0x01)
		global_settings.time_format = TIME_FORMAT_12;//12 format
	else
		global_settings.time_format = TIME_FORMAT_24;//24 format

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (TIME_24_SETTING_ID>>8);
	reply[reply_len++] = (uint8_t)(TIME_24_SETTING_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;	
}


void APP_set_date_format(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	if(buf[7] == 0x00)
		global_settings.date_format = DATE_FORMAT_YYYYMMDD;
	else if(buf[7] == 0x01)
		global_settings.date_format = DATE_FORMAT_MMDDYYYY;
	else if(buf[7] == 0x02)
		global_settings.date_format = DATE_FORMAT_DDMMYYYY;
	else
		global_settings.date_format = DATE_FORMAT_YYYYMMDD;

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (DATE_FORMAT_ID>>8);		
	reply[reply_len++] = (uint8_t)(DATE_FORMAT_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;
}

void APP_set_date_time(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;
	sys_date_timer_t datetime = {0};

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	datetime.year = 256*buf[7]+buf[8];
	datetime.month = buf[9];
	datetime.day = buf[10];
	datetime.hour = buf[11];
	datetime.minute = buf[12];
	datetime.second = buf[13];
	
	if(CheckSystemDateTimeIsValid(datetime))
	{
		datetime.week = GetWeekDayByDate(datetime);
		memcpy(&date_time, &datetime, sizeof(sys_date_timer_t));

		need_save_time = true;
	}

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (TIME_SYNC_ID>>8);		
	reply[reply_len++] = (uint8_t)(TIME_SYNC_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);	
}

#ifdef CONFIG_ALARM_SUPPORT
void APP_set_alarm(uint8_t *buf, uint32_t len)
{
	uint8_t result=0,reply[128] = {0};
	uint32_t i,index,reply_len = 0;
	alarm_infor_t infor = {0};

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	index = buf[7];
	if(index <= 7)
	{
		infor.is_on = buf[8];	//on\off
		infor.hour = buf[9];	//hour
		infor.minute = buf[10];//minute
		infor.repeat = buf[11];//repeat from monday to sunday, for example:0x1111100 means repeat in workday

		if((buf[9]<=23)&&(buf[10]<=59)&&(buf[11]<=0x7f))
		{
			result = 0x80;
			memcpy((alarm_infor_t*)&global_settings.alarm[index], (alarm_infor_t*)&infor, sizeof(alarm_infor_t));
			need_save_settings = true;
		}
	}
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (ALARM_SETTING_ID>>8);		
	reply[reply_len++] = (uint8_t)(ALARM_SETTING_ID&0x00ff);
	//status
	reply[reply_len++] = result;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);	
}
#endif

void APP_set_PHD_interval(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("flag:%d, interval:%d", buf[6], buf[7]);
#endif

	global_settings.health_interval = buf[7];
	need_save_settings = true;
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (MEASURE_HOURLY_ID>>8);		
	reply[reply_len++] = (uint8_t)(MEASURE_HOURLY_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;	
}

void APP_set_wake_screen_by_wrist(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("flag:%d", buf[6]);
#endif

	if(buf[6] == 1)
		global_settings.wake_screen_by_wrist = true;
	else
		global_settings.wake_screen_by_wrist = false;
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (SHAKE_SCREEN_ID>>8);		
	reply[reply_len++] = (uint8_t)(SHAKE_SCREEN_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;
}

void APP_set_factory_reset(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (FACTORY_RESET_ID>>8);		
	reply[reply_len++] = (uint8_t)(FACTORY_RESET_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_reset_settings = true;
}

void APP_set_target_steps(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("steps:%d", buf[7]*100+buf[8]);
#endif

	global_settings.target_steps = buf[7]*100+buf[8];
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (TARGET_STEPS_ID>>8);		
	reply[reply_len++] = (uint8_t)(TARGET_STEPS_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);

	need_save_settings = true;
}

#ifdef CONFIG_PPG_SUPPORT
void APP_get_one_key_measure_data(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("setting:%d", buf[6]);
#endif

	if(buf[6] == 1)//开启
	{
		StartPPG(PPG_DATA_HR, TRIGGER_BY_APP_ONE_KEY);
	}
	else
	{
		//packet head
		reply[reply_len++] = PACKET_HEAD;
		//data_len
		reply[reply_len++] = 0x00;
		reply[reply_len++] = 0x0A;
		//data ID
		reply[reply_len++] = (ONE_KEY_MEASURE_ID>>8);		
		reply[reply_len++] = (uint8_t)(ONE_KEY_MEASURE_ID&0x00ff);
		//status
		reply[reply_len++] = 0x80;
		//control
		reply[reply_len++] = 0x00;
		//data hr
		reply[reply_len++] = 0x00;
		//data spo2
		reply[reply_len++] = 0x00;
		//data systolic
		reply[reply_len++] = 0x00;
		//data diastolic
		reply[reply_len++] = 0x00;
		//CRC
		reply[reply_len++] = 0x00;
		//packet end
		reply[reply_len++] = PACKET_END;

		for(i=0;i<(reply_len-2);i++)
			reply[reply_len-2] += reply[i];

		BleSendData(reply, reply_len);
	}
}
#endif

#ifdef CONFIG_PPG_SUPPORT
void MCU_reply_cur_hour_ppg(sys_date_timer_t time, PPG_DATA_TYPE type, uint8_t *data)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x0D;
	//data ID
	reply[reply_len++] = (PULL_REFRESH_ID>>8);		
	reply[reply_len++] = (uint8_t)(PULL_REFRESH_ID&0x00ff);
	//status
	switch(type)
	{
	case PPG_DATA_HR://hr
		reply[reply_len++] = 0x81;
		break;
	case PPG_DATA_SPO2://spo2
		reply[reply_len++] = 0x82;
		break;
	case PPG_DATA_BPT://bpt
		reply[reply_len++] = 0x83;
		break;
	}
	//control
	reply[reply_len++] = 0x00;
	//year
	reply[reply_len++] = (time.year-2000);
	//month
	reply[reply_len++] = time.month;
	//day
	reply[reply_len++] = time.day;
	//hour
	reply[reply_len++] = time.hour;
	//minute
	reply[reply_len++] = time.minute;
	//data
	switch(type)
	{
	case PPG_DATA_HR://hr
	#ifdef UART_DEBUG
		LOGD("hr:%d", data[0]);
	#endif
		reply[reply_len++] = data[0];
		break;
	case PPG_DATA_SPO2://spo2
	#ifdef UART_DEBUG
		LOGD("spo2:%d", data[0]);
	#endif
		reply[reply_len++] = data[0];
		break;
	case PPG_DATA_BPT://bpt
	#ifdef UART_DEBUG
		LOGD("bpt:%d\\%d", data[0],data[1]);
	#endif
		reply[reply_len++] = data[0];
		reply[reply_len++] = data[1];
		break;
	}
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}
#endif

#ifdef CONFIG_TEMP_SUPPORT
void MCU_reply_cur_hour_temp(sys_date_timer_t time, uint8_t *data)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x0D;
	//data ID
	reply[reply_len++] = (PULL_REFRESH_ID>>8);		
	reply[reply_len++] = (uint8_t)(PULL_REFRESH_ID&0x00ff);
	//status
	reply[reply_len++] = 0x86;
	//control
	reply[reply_len++] = 0x00;
	//year
	reply[reply_len++] = (time.year-2000);
	//month
	reply[reply_len++] = time.month;
	//day
	reply[reply_len++] = time.day;
	//hour
	reply[reply_len++] = time.hour;
	//minute
	reply[reply_len++] = time.minute;
	//data
#ifdef UART_DEBUG
	LOGD("temp:%d.%d", (256*data[0]+data[1])/10, (256*data[0]+data[1])%10);
#endif
	reply[reply_len++] = data[0];
	reply[reply_len++] = data[1];
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}
#endif

void APP_get_cur_hour_health(sys_date_timer_t ask_time)
{
	uint8_t hr = 0,spo2 = 0;
#ifdef CONFIG_PPG_SUPPORT	
	bpt_data bpt = {0};
#endif
	uint16_t temp = 0;
	uint8_t data[2] = {0};

#ifdef UART_DEBUG
	LOGD("begin");
#endif

#ifdef CONFIG_PPG_SUPPORT
	GetGivenTimeHrRecData(ask_time, &hr);
	GetGivenTimeSpo2RecData(ask_time, &spo2);
	GetGivenTimeBptRecData(ask_time, &bpt);
#endif
#ifdef CONFIG_TEMP_SUPPORT
	GetGivenTimeTempRecData(ask_time, &temp);
#endif

#ifdef CONFIG_PPG_SUPPORT
	MCU_reply_cur_hour_ppg(ask_time, PPG_DATA_HR, &hr);
	MCU_reply_cur_hour_ppg(ask_time, PPG_DATA_SPO2, &spo2);
	MCU_reply_cur_hour_ppg(ask_time, PPG_DATA_BPT, (uint8_t*)&bpt);
#endif

#ifdef CONFIG_TEMP_SUPPORT
	data[0] = temp>>8;
	data[1] = (uint8_t)(temp&0x00ff);
	MCU_reply_cur_hour_temp(ask_time, data);
#endif	
}

void APP_get_cur_hour_sport(sys_date_timer_t ask_time)
{
	uint8_t wake,reply[128] = {0};
	uint16_t steps=0,calorie=0,distance=0;
	uint16_t light_sleep=0,deep_sleep=0;	
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

#ifdef CONFIG_IMU_SUPPORT
#ifdef CONFIG_STEP_SUPPORT
	GetSportData(&steps, &calorie, &distance);
#endif
#ifdef CPNFIG_SLEEP_SUPPORT
	GetSleepTimeData(&deep_sleep, &light_sleep);
#endif
#endif
	
	wake = 8;
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x11;
	//data ID
	reply[reply_len++] = (PULL_REFRESH_ID>>8);		
	reply[reply_len++] = (uint8_t)(PULL_REFRESH_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//Step
	reply[reply_len++] = (steps>>8);
	reply[reply_len++] = (uint8_t)(steps&0x00ff);
	//Calorie
	reply[reply_len++] = (calorie>>8);
	reply[reply_len++] = (uint8_t)(calorie&0x00ff);
	//Distance
	reply[reply_len++] = (distance>>8);
	reply[reply_len++] = (uint8_t)(distance&0x00ff);
	//Shallow Sleep
	reply[reply_len++] = (light_sleep>>8);
	reply[reply_len++] = (uint8_t)(light_sleep&0x00ff);
	//Deep Sleep
	reply[reply_len++] = (deep_sleep>>8);
	reply[reply_len++] = (uint8_t)(deep_sleep&0x00ff);
	//Wake
	reply[reply_len++] = wake;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

void APP_get_cur_hour_data(uint8_t *buf, uint32_t len)
{
#ifdef UART_DEBUG
	LOGD("%04d/%02d/%02d %02d:%02d", 2000 + buf[7],buf[8],buf[9],buf[10],buf[11]);
#endif

	refresh_time.year = 2000 + buf[7];
	refresh_time.month = buf[8];
	refresh_time.day = buf[9];
	refresh_time.hour = buf[10];
	refresh_time.minute = buf[11];

	reply_cur_data_flag = true;
}

void APP_get_location_data(uint8_t *buf, uint32_t len)
{
#ifdef UART_DEBUG
	LOGD("begin");
#endif

#ifdef CONFIG_GPS_SUPPORT
#endif	
}

void APP_get_gps_data_reply(bool flag, void *gps_data)
{
	uint8_t tmpgps;
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;
	uint32_t tmp1;
	double tmp2;

	if(!flag)
		return;
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x17;
	//data ID
	reply[reply_len++] = (LOCATION_ID>>8);		
	reply[reply_len++] = (uint8_t)(LOCATION_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;

#if 0	//xb test 2024-09-29	
	//UTC data&time
	//year
	reply[reply_len++] = gps_data.datetime.year>>8;
	reply[reply_len++] = (uint8_t)(gps_data.datetime.year&0x00FF);
	//month
	reply[reply_len++] = gps_data.datetime.month;
	//day
	reply[reply_len++] = gps_data.datetime.day;
	//hour
	reply[reply_len++] = gps_data.datetime.hour;
	//minute
	reply[reply_len++] = gps_data.datetime.minute;
	//seconds
	reply[reply_len++] = gps_data.datetime.seconds;
	
	//longitude
	tmpgps = 'E';
	if(gps_data.longitude < 0)
	{
		tmpgps = 'W';
		gps_data.longitude = -gps_data.longitude;
	}
	//direction	E\W
	reply[reply_len++] = tmpgps;
	tmp1 = (uint32_t)(gps_data.longitude); //经度整数部分
	tmp2 = gps_data.longitude - tmp1;	//经度小数部分
	//degree int
	reply[reply_len++] = tmp1;//整数部分
	tmp1 = (uint32_t)(tmp2*1000000);
	//degree dot1~2
	reply[reply_len++] = (uint8_t)(tmp1/10000);
	tmp1 = tmp1%10000;
	//degree dot3~4
	reply[reply_len++] = (uint8_t)(tmp1/100);
	tmp1 = tmp1%100;
	//degree dot5~6
	reply[reply_len++] = (uint8_t)(tmp1);	
	//latitude
	tmpgps = 'N';
	if(gps_data.latitude < 0)
	{
		tmpgps = 'S';
		gps_data.latitude = -gps_data.latitude;
	}
	//direction N\S
	reply[reply_len++] = tmpgps;
	tmp1 = (uint32_t)(gps_data.latitude);	//经度整数部分
	tmp2 = gps_data.latitude - tmp1;	//经度小数部分
	//degree int
	reply[reply_len++] = tmp1;//整数部分
	tmp1 = (uint32_t)(tmp2*1000000);
	//degree dot1~2
	reply[reply_len++] = (uint8_t)(tmp1/10000);
	tmp1 = tmp1%10000;
	//degree dot3~4
	reply[reply_len++] = (uint8_t)(tmp1/100);
	tmp1 = tmp1%100;
	//degree dot5~6
	reply[reply_len++] = (uint8_t)(tmp1);
#endif
					
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

void APP_get_battery_level(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x07;
	//data ID
	reply[reply_len++] = (BATTERY_LEVEL_ID>>8);		
	reply[reply_len++] = (uint8_t)(BATTERY_LEVEL_ID&0x00ff);
	//status
	switch(g_chg_status)
	{
	case BAT_CHARGING_NO:
		reply[reply_len++] = 0x00;
		break;
		
	case BAT_CHARGING_PROGRESS:
	case BAT_CHARGING_FINISHED:
		reply[reply_len++] = 0x01;
		break;
	}
	//control
	reply[reply_len++] = 0x00;
	//bat level
	reply[reply_len++] = g_bat_soc;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

void APP_get_firmware_version(uint8_t *buf, uint32_t len)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x07;
	//data ID
	reply[reply_len++] = (FIRMWARE_INFOR_ID>>8);		
	reply[reply_len++] = (uint8_t)(FIRMWARE_INFOR_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//fm ver
	reply[reply_len++] = (0x02<<4)+0x00;	//V2.0
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

#ifdef CONFIG_PPG_SUPPORT
void APP_get_hr(uint8_t *buf, uint32_t len)
{
	uint8_t heart_rate,reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("type:%d, flag:%d", buf[5], buf[6]);
#endif

	switch(buf[6])
	{
	case 0://关闭传感器
		break;
	case 1://打开传感器
		break;
	}

	if(buf[6] == 0)
	{
		//packet head
		reply[reply_len++] = PACKET_HEAD;
		//data_len
		reply[reply_len++] = 0x00;
		reply[reply_len++] = 0x07;
		//data ID
		reply[reply_len++] = (HEART_RATE_ID>>8);		
		reply[reply_len++] = (uint8_t)(HEART_RATE_ID&0x00ff);
		//status
		reply[reply_len++] = buf[5];
		//control
		reply[reply_len++] = buf[6];
		//heart rate
		reply[reply_len++] = 0;
		//CRC
		reply[reply_len++] = 0x00;
		//packet end
		reply[reply_len++] = PACKET_END;

		for(i=0;i<(reply_len-2);i++)
			reply[reply_len-2] += reply[i];

		BleSendData(reply, reply_len);	
	}
	else
	{
		health_record_t last_data = {0};

		switch(buf[5])
		{
		case 0x01://实时测量心率
			StartPPG(PPG_DATA_HR, TRIGGER_BY_APP);;
			break;

		case 0x02://单次测量心率
			get_cur_health_from_record(&last_data);
			MCU_send_app_get_ppg_data(PPG_DATA_HR, &last_data.hr);
			break;
		}
	}
}

void APP_get_spo2(uint8_t *buf, uint32_t len)
{
	uint8_t heart_rate,reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("type:%d, flag:%d", buf[5], buf[6]);
#endif

	switch(buf[6])
	{
	case 0://关闭传感器
		break;
	case 1://打开传感器
		break;
	}

	if(buf[6] == 0)
	{
		//packet head
		reply[reply_len++] = PACKET_HEAD;
		//data_len
		reply[reply_len++] = 0x00;
		reply[reply_len++] = 0x07;
		//data ID
		reply[reply_len++] = (BLOOD_OXYGEN_ID>>8);		
		reply[reply_len++] = (uint8_t)(BLOOD_OXYGEN_ID&0x00ff);
		//status
		reply[reply_len++] = buf[5];
		//control
		reply[reply_len++] = buf[6];
		//heart rate
		reply[reply_len++] = 0;
		//CRC
		reply[reply_len++] = 0x00;
		//packet end
		reply[reply_len++] = PACKET_END;

		for(i=0;i<(reply_len-2);i++)
			reply[reply_len-2] += reply[i];

		BleSendData(reply, reply_len);	
	}
	else
	{
		health_record_t last_data = {0};

		switch(buf[5])
		{
		case 0x01://实时测量血氧
			StartPPG(PPG_DATA_SPO2, TRIGGER_BY_APP);;
			break;

		case 0x02://单次测量血氧
			get_cur_health_from_record(&last_data);
			MCU_send_app_get_ppg_data(PPG_DATA_SPO2, &last_data.spo2);
			break;
		}
	}
}

void APP_get_bpt(uint8_t *buf, uint32_t len)
{
	uint8_t heart_rate,reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("type:%d, flag:%d", buf[5], buf[6]);
#endif

	switch(buf[6])
	{
	case 0://关闭传感器
		break;
	case 1://打开传感器
		break;
	}

	if(buf[6] == 0)
	{
		//packet head
		reply[reply_len++] = PACKET_HEAD;
		//data_len
		reply[reply_len++] = 0x00;
		reply[reply_len++] = 0x07;
		//data ID
		reply[reply_len++] = (BLOOD_PRESSURE_ID>>8);		
		reply[reply_len++] = (uint8_t)(BLOOD_PRESSURE_ID&0x00ff);
		//status
		reply[reply_len++] = buf[5];
		//control
		reply[reply_len++] = buf[6];
		//heart rate
		reply[reply_len++] = 0;
		//CRC
		reply[reply_len++] = 0x00;
		//packet end
		reply[reply_len++] = PACKET_END;

		for(i=0;i<(reply_len-2);i++)
			reply[reply_len-2] += reply[i];

		BleSendData(reply, reply_len);	
	}
	else
	{
		uint8_t data[2] = {0};
		health_record_t last_data = {0};

	#ifdef CONFIG_PPG_SUPPORT
		switch(buf[5])
		{
		case 0x01://实时测量血压
			StartPPG(PPG_DATA_BPT, TRIGGER_BY_APP);;
			break;

		case 0x02://单次测量血压
			get_cur_health_from_record(&last_data);
			data[0] = last_data.systolic;
			data[1] = last_data.diastolic;
			MCU_send_app_get_ppg_data(PPG_DATA_BPT, data);
			break;
		}
	#endif	
	}
}
#endif

#ifdef CONFIG_TEMP_SUPPORT
void APP_get_temp(uint8_t *buf, uint32_t len)
{
	uint8_t heart_rate,reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("type:%d, flag:%d", buf[5], buf[6]);
#endif

	switch(buf[6])
	{
	case 0://关闭传感器
		break;
	case 1://打开传感器
		break;
	}

	if(buf[6] == 0)
	{
		//packet head
		reply[reply_len++] = PACKET_HEAD;
		//data_len
		reply[reply_len++] = 0x00;
		reply[reply_len++] = 0x07;
		//data ID
		reply[reply_len++] = (TEMPERATURE_ID>>8);		
		reply[reply_len++] = (uint8_t)(TEMPERATURE_ID&0x00ff);
		//status
		reply[reply_len++] = buf[5];
		//control
		reply[reply_len++] = buf[6];
		//heart rate
		reply[reply_len++] = 0;
		//CRC
		reply[reply_len++] = 0x00;
		//packet end
		reply[reply_len++] = PACKET_END;

		for(i=0;i<(reply_len-2);i++)
			reply[reply_len-2] += reply[i];

		BleSendData(reply, reply_len);	
	}
	else
	{
		uint8_t data[2] = {0};
		health_record_t last_data = {0};

		switch(buf[5])
		{
		case 0x01://实时测量体温
			APPStartTemp();
			break;

		case 0x02://单次测量体温
			get_cur_health_from_record(&last_data);
			data[0] = last_data.deca_temp>>8;
			data[1] = (uint8_t)(last_data.deca_temp&0x00ff);
			MCU_send_app_get_temp_data(data);
			break;
		}
	}
}
#endif

void MCU_get_ble_status(void)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (GET_BLE_STATUS_ID>>8);		
	reply[reply_len++] = (uint8_t)(GET_BLE_STATUS_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

//手环查找手机
void MCU_send_find_phone(void)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x07;
	//data ID
	reply[reply_len++] = (FIND_PHONE_ID>>8);		
	reply[reply_len++] = (uint8_t)(FIND_PHONE_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//data
	reply[reply_len++] = 0x01;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);	
}

//手环上报一键测量数据
void MCU_send_app_one_key_measure_data(void)
{
	uint8_t heart_rate,spo2,systolic,diastolic;
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	GetPPGData(&heart_rate, &spo2, &systolic, &diastolic);
	
	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x0A;
	//data ID
	reply[reply_len++] = (ONE_KEY_MEASURE_ID>>8);		
	reply[reply_len++] = (uint8_t)(ONE_KEY_MEASURE_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x01;
	//data hr
	reply[reply_len++] = heart_rate;
	//data spo2
	reply[reply_len++] = spo2;
	//data systolic
	reply[reply_len++] = systolic;
	//data diastolic
	reply[reply_len++] = diastolic;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

#ifdef CONFIG_PPG_SUPPORT
void MCU_send_app_get_ppg_data(PPG_DATA_TYPE flag, uint8_t *data)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x07;
	//data ID
	switch(flag)
	{
	case PPG_DATA_HR:
		reply[reply_len++] = (HEART_RATE_ID>>8);		
		reply[reply_len++] = (uint8_t)(HEART_RATE_ID&0x00ff);
		break;
	case PPG_DATA_SPO2:
		reply[reply_len++] = (BLOOD_OXYGEN_ID>>8);		
		reply[reply_len++] = (uint8_t)(BLOOD_OXYGEN_ID&0x00ff);
		break;
	case PPG_DATA_BPT:
		reply[reply_len++] = (BLOOD_PRESSURE_ID>>8);		
		reply[reply_len++] = (uint8_t)(BLOOD_PRESSURE_ID&0x00ff);
		break;
	}
	//status
	reply[reply_len++] = 0x02;
	//control
	reply[reply_len++] = 0x01;
	switch(flag)
	{
	case PPG_DATA_HR:
		reply[reply_len++] = data[0];
		break;
	case PPG_DATA_SPO2:
		reply[reply_len++] = data[0];
		break;
	case PPG_DATA_BPT:
		reply[reply_len++] = data[0];		
		reply[reply_len++] = data[1];
		break;
	}
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}
#endif

#ifdef CONFIG_TEMP_SUPPORT
void MCU_send_app_get_temp_data(uint8_t *data)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x07;
	//data ID
	reply[reply_len++] = (TEMPERATURE_ID>>8);		
	reply[reply_len++] = (uint8_t)(TEMPERATURE_ID&0x00ff);
	//status
	reply[reply_len++] = 0x02;
	//control
	reply[reply_len++] = 0x01;
	//tempdata
	reply[reply_len++] = data[0];		
	reply[reply_len++] = data[1];
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}
#endif

void nrf52810_report_work_mode(uint8_t *buf, uint32_t len)
{
#ifdef UART_DEBUG
	LOGD("mode:%d", buf[5]);
#endif

	if(buf[5] == 1)
	{
		ble_work_mode = BLE_WORK_NORMAL;
	}
	else
	{
		ble_work_mode = BLE_WORK_DFU;
		if(g_ble_connected)
		{
			g_ble_connected = false;
			redraw_blt_status_flag = true;
		}
	}
}

void get_ble_status_response(uint8_t *buf, uint32_t len)
{
#ifdef UART_DEBUG
	LOGD("BLE_status:%d", buf[7]);
#endif

	g_ble_status = buf[7];

	switch(g_ble_status)
	{
	case BLE_STATUS_OFF:
	case BLE_STATUS_SLEEP:
	case BLE_STATUS_BROADCAST:
		g_ble_connected = false;
		break;

	case BLE_STATUS_CONNECTED:
		g_ble_connected = true;
		redraw_blt_status_flag = true;
		break;
	}
}
#endif

void get_nrf52810_ver_response(uint8_t *buf, uint32_t len)
{
	uint32_t i;

	for(i=0;i<len-9;i++)
	{
		g_nrf52810_ver[i] = buf[7+i];
	}

#ifdef UART_DEBUG
	LOGD("nrf52810_ver:%s", g_nrf52810_ver);
#endif
}

void get_ble_mac_address_response(uint8_t *buf, uint32_t len)
{
	uint32_t i;
	uint8_t mac_addr[6] = {0};
	
	for(i=0;i<6;i++)
		mac_addr[i] = buf[7+i];

	sprintf(g_ble_mac_addr, "%02X:%02X:%02X:%02X:%02X:%02X", 
							mac_addr[0],
							mac_addr[1],
							mac_addr[2],
							mac_addr[3],
							mac_addr[4],
							mac_addr[5]);
#ifdef UART_DEBUG
	LOGD("ble_mac_addr:%s", g_ble_mac_addr);
#endif
}

void MCU_get_nrf52810_ver(void)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (GET_NRF52810_VER_ID>>8);		
	reply[reply_len++] = (uint8_t)(GET_NRF52810_VER_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

void MCU_get_ble_mac_address(void)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (GET_BLE_MAC_ADDR_ID>>8);		
	reply[reply_len++] = (uint8_t)(GET_BLE_MAC_ADDR_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = 0x00;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);
}

//设置BLE工作模式		0:关闭 1:打开 2:唤醒 3:休眠
void MCU_set_ble_work_mode(ENUM_BLE_MODE work_mode)
{
	uint8_t reply[128] = {0};
	uint32_t i,reply_len = 0;

#ifdef UART_DEBUG
	LOGD("begin");
#endif

	//packet head
	reply[reply_len++] = PACKET_HEAD;
	//data_len
	reply[reply_len++] = 0x00;
	reply[reply_len++] = 0x06;
	//data ID
	reply[reply_len++] = (SET_BEL_WORK_MODE_ID>>8);		
	reply[reply_len++] = (uint8_t)(SET_BEL_WORK_MODE_ID&0x00ff);
	//status
	reply[reply_len++] = 0x80;
	//control
	reply[reply_len++] = work_mode;
	//CRC
	reply[reply_len++] = 0x00;
	//packet end
	reply[reply_len++] = PACKET_END;

	for(i=0;i<(reply_len-2);i++)
		reply[reply_len-2] += reply[i];

	BleSendData(reply, reply_len);	
}

/**********************************************************************************
*Name: ble_receive_data_handle
*Function:  处理蓝牙接收到的数据
*Parameter: 
*			Input:
*				buf 接收到的数据
*				len 接收到的数据长度
*			Output:
*				none
*			Return:
*				void
*Description:
*	接收到的数据包的格式如下:
*	包头			数据长度		操作		状态类型	控制		数据1	数据…		校验		包尾
*	(StarFrame)		(Data length)	(ID)		(Status)	(Control)	(Data1)	(Data…)	(CRC8)		(EndFrame)
*	(1 bytes)		(2 byte)		(2 byte)	(1 byte)	(1 byte)	(可选)	(可选)		(1 bytes)	(1 bytes)
*
*	例子如下表所示：
*	Offset	Field		Size	Value(十六进制)		Description
*	0		StarFrame	1		0xAB				起始帧
*	1		Data length	2		0x0000-0xFFFF		数据长度,从ID开始一直到包尾
*	3		Data ID		2		0x0000-0xFFFF	    ID
*	5		Status		1		0x00-0xFF	        Status
*	6		Control		1		0x00-0x01			控制
*	7		Data0		1-14	0x00-0xFF			数据0
*	8+n		CRC8		1		0x00-0xFF			数据校验,从包头开始到CRC前一位
*	9+n		EndFrame	1		0x88				结束帧
**********************************************************************************/
void ble_receive_data_handle(uint8_t *buf, uint32_t len)
{
	uint8_t CRC_data=0,data_status;
	uint16_t data_len,data_ID;
	uint32_t i;

#ifdef UART_DEBUG
	//LOGD("receive:%s", buf);
#endif

	if((buf[0] != PACKET_HEAD) || (buf[len-1] != PACKET_END))	//format is error
	{
	#ifdef UART_DEBUG
		LOGD("format is error! HEAD:%x, END:%x", buf[0], buf[len-1]);
	#endif
		return;
	}

	for(i=0;i<len-2;i++)
		CRC_data = CRC_data+buf[i];

	if(CRC_data != buf[len-2])									//crc is error
	{
	#ifdef UART_DEBUG
		LOGD("CRC is error! data:%x, CRC:%x", buf[len-2], CRC_data);
	#endif
		return;
	}

	data_len = buf[1]*256+buf[2];
	data_ID = buf[3]*256+buf[4];

	switch(data_ID)
	{
	#ifdef CONFIG_BLE_SUPPORT
	case BLE_WORK_MODE_ID:
		nrf52810_report_work_mode(buf, len);//52810工作状态
		break;
#ifdef CONFIG_PPG_SUPPORT
	case HEART_RATE_ID:			//心率
		APP_get_hr(buf, len);
		break;
	case BLOOD_OXYGEN_ID:		//血氧
		APP_get_spo2(buf, len);
		break;
	case BLOOD_PRESSURE_ID:		//血压
		APP_get_bpt(buf, len);
		break;
	case ONE_KEY_MEASURE_ID:	//一键测量
		APP_get_one_key_measure_data(buf, len);
		break;
#endif
#ifdef CONFIG_TEMP_SUPPORT	
	case TEMPERATURE_ID:		//体温
		APP_get_temp(buf, len);
		break;
#endif	
	case PULL_REFRESH_ID:		//下拉刷新
		APP_get_cur_hour_data(buf, len);
		break;
	case SLEEP_DETAILS_ID:		//睡眠详情
		break;
	#ifdef CONFIG_ALARM_SUPPORT	
	case FIND_DEVICE_ID:		//查找手环
		APP_set_find_device(buf, len);
		break;
	#endif	
	case SMART_NOTIFY_ID:		//智能提醒
		break;
	#ifdef CONFIG_ALARM_SUPPORT	
	case ALARM_SETTING_ID:		//闹钟设置
		APP_set_alarm(buf, len);
		break;
	#endif	
	case USER_INFOR_ID:			//用户信息
		break;
	case SEDENTARY_ID:			//久坐提醒
		break;
	case SHAKE_SCREEN_ID:		//抬手亮屏
		APP_set_wake_screen_by_wrist(buf, len);
		break;
	case MEASURE_HOURLY_ID:		//整点测量设置
		APP_set_PHD_interval(buf, len);
		break;
	case SHAKE_PHOTO_ID:		//摇一摇拍照
		break;
	case LANGUAGE_SETTING_ID:	//中英日文切换
		APP_set_language(buf, len);
		break;
	case TIME_24_SETTING_ID:	//12/24小时设置
		APP_set_time_24_format(buf, len);
		break;
	#ifdef CONFIG_ALARM_SUPPORT	
	case FIND_PHONE_ID:			//查找手机回复
		APP_reply_find_phone(buf, len);
		break;
	#endif
	case WEATHER_INFOR_ID:		//天气信息下发
		break;
	case TIME_SYNC_ID:			//时间同步
		APP_set_date_time(buf, len);
		break;
	case TARGET_STEPS_ID:		//目标步数
		APP_set_target_steps(buf, len);
		break;
	case BATTERY_LEVEL_ID:		//电池电量
		APP_get_battery_level(buf, len);
		break;
	case FACTORY_RESET_ID:		//清除手环数据
		APP_set_factory_reset(buf, len);
		break;
	case ECG_ID:				//心电
		break;
	case LOCATION_ID:			//获取定位信息
		APP_get_location_data(buf, len);
		break;
	case DATE_FORMAT_ID:		//年月日格式
		APP_set_date_format(buf, len);
		break;
	case BLE_CONNECT_ID:		//BLE断连提醒
		ble_connect_or_disconnect_handle(buf, len);
		break;
	case GET_BLE_STATUS_ID:
		get_ble_status_response(buf, len);
		break;
	case SET_BEL_WORK_MODE_ID:
		break;	
	case FIRMWARE_INFOR_ID:		//固件版本号
		APP_get_firmware_version(buf, len);
		break;
	#endif/*CONFIG_BLE_SUPPORT*/

	#ifdef CONFIG_TOUCH_SUPPORT	
	case CTP_NOTIFY_ID:
		CTP_notify_handle(buf, len);
		break;
	#endif	
	case GET_NRF52810_VER_ID:
		get_nrf52810_ver_response(buf, len);
		break;
	case GET_BLE_MAC_ADDR_ID:
		get_ble_mac_address_response(buf, len);
		break;
	default:
	#ifdef UART_DEBUG	
		LOGD("data_id is unknown!");
	#endif
		break;
	}
}

void BLE_init(void)
{
}

void BleMsgProcess(void)
{
}

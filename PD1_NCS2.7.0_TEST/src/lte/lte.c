/****************************************Copyright (c)************************************************
** File Name:			    lte.c
** Descriptions:			lte function main source file
** Created By:				xie biao
** Created Date:			2024-09-29
** Modified Date:      		2024-09-29
** Version:			    	V1.0
******************************************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include "settings.h"
#include "datetime.h"
#include "transfer_cache.h"
#include "lte.h"
#include "logger.h"

#define LTE_DEBUG

static bool lte_connect_ok_flag = true;
static bool power_on_data_flag = true;

static bool lte_connected = false;
static bool mqtt_connected = false;

static bool lte_get_infor_flag = false;
static bool lte_get_ip_flag = false;
static bool lte_get_rsrp_flag = false;
static bool lte_turn_off_flag = false;
static bool lte_net_link_flag = false;
static bool lte_mqtt_link_flag = false;
static bool lte_mqtt_send_flag = false;

static bool server_has_timed_flag = false;

uint8_t g_imsi[IMSI_MAX_LEN+1] = {0};
uint8_t g_imei[IMEI_MAX_LEN+1] = {0};
uint8_t g_iccid[ICCID_MAX_LEN+1] = {0};

uint8_t g_prj_dir[128] = {0};
uint8_t g_new_fw_ver[64] = {0};
uint8_t g_new_modem_ver[64] = {0};
uint8_t g_new_ble_ver[64] = {0};
uint8_t g_new_wifi_ver[64] = {0};
uint8_t g_new_ui_ver[16] = {0};
uint8_t g_new_font_ver[16] = {0};
uint8_t g_new_ppg_ver[16] = {0};
uint8_t g_timezone[5] = {0};

uint8_t g_rsrp = 0;
uint16_t g_tau_time = 0;
uint16_t g_act_time = 0;
uint32_t g_tau_ext_time = 0;


static uint8_t lte_relink_count = 0;
static uint8_t mqtt_relink_count = 0;

static uint32_t mqtt_msg_id = 1;

static uint8_t broker_hostname[128] = CONFIG_MQTT_DOMESTIC_BROKER_HOSTNAME;
static uint32_t broker_port = CONFIG_MQTT_DOMESTIC_BROKER_PORT;

static CacheInfo mqtt_send_cache = {0};

static void lte_get_infor_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_get_infor_timer, lte_get_infor_timerout, NULL);
static void lte_get_lp_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_get_ip_timer, lte_get_lp_timerout, NULL);
static void lte_get_rsrp_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_get_rsrp_timer, lte_get_rsrp_timerout, NULL);
static void lte_turn_off_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_turn_off_timer, lte_turn_off_timerout, NULL);
static void lte_net_link_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_net_link_timer, lte_net_link_timerout, NULL);
static void lte_mqtt_link_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_mqtt_link_timer, lte_mqtt_link_timerout, NULL);
static void lte_mqtt_Send_timeout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_mqtt_send_timer, lte_mqtt_Send_timeout, NULL);


static void lte_get_infor_timerout(struct k_timer *timer_id)
{
	lte_get_infor_flag = true;
}

static void lte_get_lp_timerout(struct k_timer *timer_id)
{
	lte_get_ip_flag = true;
}

static void lte_turn_off_timerout(struct k_timer *timer_id)
{
	lte_turn_off_flag = true;
}

static void lte_get_rsrp_timerout(struct k_timer *timer_id)
{
	lte_get_rsrp_flag = true;
}

static void lte_net_link_timerout(struct k_timer *timer_id)
{
	lte_net_link_flag = true;
}

static void lte_mqtt_link_timerout(struct k_timer *timer_id)
{
	lte_mqtt_link_flag = true;
}

static void lte_mqtt_Send_timeout(struct k_timer *timer_id)
{
	lte_mqtt_send_flag = true;
}

void lte_mqtt_client_set_keepalive(void)
{
	uint8_t cmdbuf[256] = {0};
	
	//Set mqtt keepalive
	//AT+ECMTCFG="keepalive",<tcpconnectID>[,<keep-alive time>]
	sprintf(cmdbuf, "%s=\"%s\",0,%d\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE, CONFIG_MQTT_KEEPALIVE_TIME);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_client_set_session(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt session
	//AT+ECMTCFG="session",<tcpconnectID>[,<clean_session>] 
	//<clean_session> 整型。 配置会话类型。0：断开连接后，服务器必须存储客户端的订阅。1：服务器端丢弃之前为该客户端保留的任何信息，并将连接视为“clean（清除）”。
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,0\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_SESSION);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_client_set_timeout(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt msg send timeout
	//AT+ECMTCFG="timeout",<tcpconnectID>[,<pkt_timeout>[,<retry_times>][,<timeout_notice>]]
	//<pkt_timeout> 整型。数据包传递超时时间。 范围是 1-60。 默认值为 10。单位：秒。
	//<retry_times> 整型. (不支持)数据包传递超时的重试时间。 范围是 0-10。 预设值为 3。
	//<timeout_notice> 整型. (不支持)0：传输数据包时不报告超时消息。1：传输数据包时报告超时消息。
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,10,3,1\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_will(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt will
	//AT+ECMTCFG="will",<tcpconnectID>[,<will_fg>[,<will_qos>,<will_retain>,<"will_topic">,<"will_msg">]]
	//<will_fg> 整型。 配置 Will 标志。0：忽略 Will 标志配置。1：需要 Will 标志配置。
	//<will_qos> 整型。 消息传递的服务质量。0：最多一次。1：最少一次。2：正好一次。
	//<will_retain> 整型。Will 保留标志仅用于发布消息。0：当客户端向服务器发送发布消息时，服务器在将消息传递给当前订户后将不保留该消息。1：当客户端向服务器发送 PUBLISH 消息时，服务器在将消息传递给当前订户后应保留该消息。
	//<will_topic> 整型。Will 主题字符串，最大大小为 255 字节。
	//<will_msg> 整型。Will 消息定义了客户端意外断开连接时发布到 will 主题的消息的内容。它可以是零长度的消息。最大大小为 255 个字节。
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",1,0,1,0,\"%s\",\"\"\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_WILL, CONFIG_MQTT_SUB_TOPIC);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_version(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt version
	//AT+ECMTCFG="version",<tcpconnectID>[,<version>]
	//<version> 整型。 MQTT 协议的版本，默认为 MQTT v3.1.1
	//									3：MQTT v3.1 。
	//									4：MQTT v3.1.1。
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,4\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_VERSION);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_cloud(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt cloud type
	//AT+ECMTCFG="cloud",<tcpconnectID>,<cloudtype>,<data type>
	//<cloud type > 整型。
	//				0：mosquitto 平台。1：OneNet 平台。2：阿里云。
	//<data type> 整型，范围是 0-255
	//					OneNet 云平台定义如下
	//						1 ：OneNet 数据类型 1。
	//						2 ：OneNet 数据类型 2。
	//						3 ：OneNet 数据类型 3。
	//						4 ：OneNet 数据类型 4。
	//						5 ：OneNet 数据类型 5。
	//						6 ：OneNet 数据类型 6。
	//					阿里云定义如下
	//						1 ：Json 数据。
	//						2 ：字符串数据。
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,0,2\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_CLOUD);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_init(void)
{
	lte_mqtt_client_set_keepalive();
}

void lte_mqtt_open(void)
{
	uint8_t cmdbuf[256] = {0};

	//AT+ECMTOPEN=<tcpconnectID>,"<host_name>",<port>
	sprintf(cmdbuf, "%s=0,\"%s\",%d\r\n", LTE_SNS521S_SET_MQTT_OPEN, broker_hostname, broker_port);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_close(void)
{
	uint8_t cmdbuf[256] = {0};

	//AT+ECMTCLOSE=<tcpconnectID>
	sprintf(cmdbuf, "%s=0\r\n", LTE_SNS521S_SET_MQTT_CLOSE);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_connect(void)
{
	uint8_t cmdbuf[256] = {0};

	//AT+ECMTCONN=<tcpconnectID>,"<clientID>"[,"<username>"[,"<password>"]]
	sprintf(cmdbuf, "%s=0,\"%s\",\"%s\",\"%s\"\r\n", LTE_SNS521S_SET_MQTT_CONNECT, g_imei, CONFIG_MQTT_USER_NAME, CONFIG_MQTT_PASSWORD);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_disconnect(void)
{
	uint8_t cmdbuf[256] = {0};
	
	//AT+ECMTDISC=<tcpconnectID>
	sprintf(cmdbuf, "%s=0\r\n", LTE_SNS521S_SET_MQTT_DISCONNECT);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_subscribe(void)
{
	uint8_t cmdbuf[256] = {0};
	
	//AT+ECMTSUB=<tcpconnectID>,<msgID>,"<topic>",<qos>
	sprintf(cmdbuf, "%s=0,%d,\"%s\",1\r\n", LTE_SNS521S_SET_MQTT_SUB, mqtt_msg_id++, CONFIG_MQTT_SUB_TOPIC);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_unsubscribe(void)
{
	uint8_t cmdbuf[256] = {0};
	
	//AT+ECMTUNS=<tcpconnectID>,<msgID>,"<topic>"
	sprintf(cmdbuf, "%s=0,%d,\"%s\"\r\n", LTE_SNS521S_SET_MQTT_UNSUB, --mqtt_msg_id, CONFIG_MQTT_SUB_TOPIC);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_publish(uint8_t *data, uint32_t data_len)
{
	uint8_t cmdbuf[256] = {0};
		
	//AT+ECMTPUB=<tcpconnectID>,<msgID>,<qos>,<retain>,"<topic>","<payload>"
	sprintf(cmdbuf, "%s=0,%d,2,0,\"%s\",\"%s\"\r\n", LTE_SNS521S_SET_MQTT_PUBLISH, mqtt_msg_id++, CONFIG_MQTT_SUB_TOPIC, data);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_link(void)
{
	static bool init_flag = false;

	if(!init_flag)
	{
		init_flag = true;

		lte_mqtt_client_init();
	}
	else
	{
		lte_mqtt_open();
	}
	
	k_timer_start(&lte_mqtt_link_timer, K_SECONDS(2*60), K_NO_WAIT);
}

void LteMqttSendData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&mqtt_send_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
		LteSendData(p_data, data_len);
		delete_data_from_cache(&mqtt_send_cache);
		k_timer_start(&lte_mqtt_send_timer, K_MSEC(50), K_NO_WAIT);
	}
}

void MqttSendDataStart(void)
{
	k_timer_start(&lte_mqtt_send_timer, K_MSEC(50), K_NO_WAIT);
}

void MqttSendData(uint8_t *databuf, uint32_t len)
{
	add_data_into_cache(&mqtt_send_cache, databuf, len, DATA_TRANSFER);
	
	if(!lte_connected)
	{
		k_timer_start(&lte_net_link_timer, K_SECONDS(1), K_NO_WAIT);
	}
	else if(!mqtt_connected)
	{
		k_timer_start(&lte_mqtt_link_timer, K_SECONDS(1), K_NO_WAIT);
	}
	else
	{
		MqttSendDataStart();
	}
}

bool GetParaFromString(uint8_t *rece, uint32_t rece_len, uint8_t *cmd, uint8_t *data)
{
	uint8_t *p1,*p2;
	uint32_t i=0;
	
	if(rece == NULL || cmd == NULL || rece_len == 0)
		return false;

	p1 = rece;
	while((p1 != NULL) && ((p1-rece) < rece_len))
	{
		p2 = strstr(p1, ":");
		if(p2 != NULL)
		{
			i++;
			p1 = p2 + 1;
			if(i == 5)
				break;
		}
		else
		{
			return false;
		}
	}

	p2 = strstr(p1, ":");
	if(p2 != NULL)
	{
		memcpy(cmd, p1, (p2-p1));

		p1 = p2 + 1;
		p2 = strstr(p1, "}");
		if(p2 != NULL)
		{
			memcpy(data, p1, (p2-p1));
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

void ParseData(uint8_t *data, uint32_t datalen)
{
	bool ret = false;
	uint8_t strcmd[10] = {0};
	uint8_t strdata[256] = {0};
	
	if(data == NULL || datalen == 0)
		return;

	ret = GetParaFromString(data, datalen, strcmd, strdata);
#ifdef LTE_DEBUG
	LOGD("ret:%d, cmd:%s, data:%s", ret,strcmd,strdata);
#endif
	if(ret)
	{
		bool flag = false;
		
		if(strcmp(strcmd, "S7") == 0)
		{
			uint8_t *ptr,*ptr1;
			uint8_t strtmp[128] = {0};

			//后台下发定位上报间隔
			ptr = strstr(strdata, ",");
			if(ptr != NULL)
			{
				memcpy(strtmp, strdata, (ptr-strdata));
				global_settings.dot_interval.time = atoi(strtmp);

				ptr++;
				ptr1 = strstr(ptr, ",");
				if(ptr1 != NULL)
				{
					memset(strtmp, 0, sizeof(strtmp));
					memcpy(strtmp, ptr, ptr1-ptr);
					global_settings.dot_interval.steps = atoi(strtmp);

					ptr = ptr1+1;
					memset(strtmp, 0, sizeof(strtmp));
					strcpy(strtmp, ptr);
					global_settings.location_type = atoi(strtmp);
				}
			#ifdef LTE_DEBUG
				LOGD("time:%d, steps:%d, location_type:%d", global_settings.dot_interval.time,global_settings.dot_interval.steps,global_settings.location_type);
			#endif	
			}

			flag = true;
		}
		else if(strcmp(strcmd, "S8") == 0)
		{
			//后台下发健康检测间隔
			global_settings.health_interval = atoi(strdata);

			flag = true;
		}
		else if(strcmp(strcmd, "S9") == 0)
		{
			uint8_t tmpbuf[8] = {0};

			strcat(strdata, ",");
			
			//后台下发抬腕亮屏设置
			GetStringInforBySepa(strdata, ",", 1, tmpbuf);
			global_settings.wake_screen_by_wrist = atoi(tmpbuf);
			//后台下发脱腕检测设置
			GetStringInforBySepa(strdata, ",", 2, tmpbuf);
			global_settings.wrist_off_check = atoi(tmpbuf);
			//后台下发摔倒检测设置
			GetStringInforBySepa(strdata, ",", 3, tmpbuf);
			global_settings.fall_check = atoi(tmpbuf);
			
			flag = true;
		}
		else if(strcmp(strcmd, "S11") == 0)
		{
			uint8_t *ptr;
			uint8_t strtmp[128] = {0};
			
			//后台下发血压校准值
			ptr = strstr(strdata, ",");
			if(ptr != NULL)
			{
				bp_calibra_t bpt = {0};
				
				memcpy(strtmp, strdata, (ptr-strdata));
				bpt.systolic = atoi(strtmp);

				ptr++;
				memset(strtmp, 0, sizeof(strtmp));
				strcpy(strtmp, ptr);
				bpt.diastolic = atoi(strtmp);

			#ifdef CONFIG_PPG_SUPPORT
				if((global_settings.bp_calibra.systolic != bpt.systolic) || (global_settings.bp_calibra.diastolic != bpt.diastolic))
				{
					memcpy(&global_settings.bp_calibra, &bpt, sizeof(bp_calibra_t));
					sh_clear_bpt_cal_data();
					ppg_bpt_is_calbraed = false;
					ppg_bpt_cal_need_update = true;
				}
			#endif
			}

			flag = true;
		}
		else if(strcmp(strcmd, "S12") == 0)
		{
			uint8_t *ptr,*ptr1;
			uint8_t strtmp[256] = {0};
			uint32_t copylen = 0;

			//后台下发最新版本信息
			//project dir
			ptr = strstr(strdata, ",");
			if(ptr == NULL)
				return;
			copylen = (ptr-strdata) < sizeof(g_prj_dir) ? (ptr-strdata) : sizeof(g_prj_dir);
			memset(g_prj_dir, 0x00, sizeof(g_prj_dir));
			memcpy(g_prj_dir, strdata, copylen);

			//9160 fw ver
			ptr++;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_fw_ver) ? (ptr1-ptr) : sizeof(g_new_fw_ver);
			memset(g_new_fw_ver, 0x00, sizeof(g_new_fw_ver));
			memcpy(g_new_fw_ver, ptr, copylen);

			//9160 modem ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_modem_ver) ? (ptr1-ptr) : sizeof(g_new_modem_ver);
			memset(g_new_modem_ver, 0x00, sizeof(g_new_modem_ver));
			memcpy(g_new_modem_ver, ptr, copylen);

			//52810 fw ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_ble_ver) ? (ptr1-ptr) : sizeof(g_new_ble_ver);
			memset(g_new_ble_ver, 0x00, sizeof(g_new_ble_ver));
			memcpy(g_new_ble_ver, ptr, copylen);

			//ppg ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_ppg_ver) ? (ptr1-ptr) : sizeof(g_new_ppg_ver);
			memset(g_new_ppg_ver, 0x00, sizeof(g_new_ppg_ver));
			memcpy(g_new_ppg_ver, ptr, copylen);

			//wifi ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_wifi_ver) ? (ptr1-ptr) : sizeof(g_new_wifi_ver);
			memset(g_new_wifi_ver, 0x00, sizeof(g_new_wifi_ver));
			memcpy(g_new_wifi_ver, ptr, copylen);

			//ui ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(g_new_ui_ver) ? (ptr1-ptr) : sizeof(g_new_ui_ver);
			memset(g_new_ui_ver, 0x00, sizeof(g_new_ui_ver));
			memcpy(g_new_ui_ver, ptr, copylen);

			//font ver
			ptr = ptr1+1;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				copylen = (datalen-(ptr-strdata)) < sizeof(g_new_font_ver) ? (datalen-(ptr-strdata)) : sizeof(g_new_font_ver);
			else
				copylen = (ptr1-ptr) < sizeof(g_new_font_ver) ? (ptr1-ptr) : sizeof(g_new_font_ver);
			memset(g_new_font_ver, 0x00, sizeof(g_new_font_ver));
			memcpy(g_new_font_ver, ptr, copylen);

		#ifdef CONFIG_FOTA_DOWNLOAD
			VerCheckExit();
		#endif
		}
		else if(strcmp(strcmd, "S15") == 0)
		{
			uint8_t *ptr,*ptr1;
			uint8_t tz_count,tz_dir,tz_buf[4] = {0};
			uint8_t date_buf[8] = {0};
			uint8_t time_buf[6] = {0};
			uint8_t tmpbuf[16] = {0};
			sys_date_timer_t tmp_dt = {0};
			uint32_t copylen = 0;

			//后台下发校时指令
			//timezone
			ptr = strstr(strdata, ",");
			if(ptr == NULL)
				return;
			copylen = (ptr-strdata) < sizeof(tz_buf) ? (ptr-strdata) : sizeof(tz_buf);
			memcpy(tz_buf, strdata, copylen);
			if(tz_buf[0] == '+' || tz_buf[0] == '-')
			{
				if(tz_buf[0] == '+')
					tz_dir = 1;
				else
					tz_dir = 0;

				tz_count = atoi(&tz_buf[1]);
			}
			else
			{
				tz_dir = 1;
				tz_count = atoi(tz_buf);
			}

		#ifdef LTE_DEBUG
			LOGD("timezone:%c%d",  (tz_dir == 1 ? '+':'-'), tz_count);
		#endif

			//date
			ptr++;
			ptr1 = strstr(ptr, ",");
			if(ptr1 == NULL)
				return;
			copylen = (ptr1-ptr) < sizeof(date_buf) ? (ptr1-ptr) : sizeof(date_buf);
			memcpy(date_buf, ptr, copylen);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &date_buf[0], 4);
			tmp_dt.year = atoi(tmpbuf);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &date_buf[4], 2);
			tmp_dt.month = atoi(tmpbuf);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &date_buf[6], 2);
			tmp_dt.day = atoi(tmpbuf);

			//time
			ptr = ptr1+1;
			copylen = (datalen-(ptr-strdata)) < sizeof(time_buf) ? (datalen-(ptr-strdata)) : sizeof(time_buf);
			memcpy(time_buf, ptr, copylen);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &time_buf[0], 2);
			tmp_dt.hour = atoi(tmpbuf);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &time_buf[2], 2);
			tmp_dt.minute = atoi(tmpbuf);
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, &time_buf[4], 2);
			tmp_dt.second = atoi(tmpbuf);

			//if(tz_dir == 1)
			//{
			//	TimeIncrease(&tmp_dt, tz_count*15);
			//}
			//else
			//{
			//	TimeDecrease(&tmp_dt, tz_count*15);
			//}
			
		#ifdef LTE_DEBUG
			LOGD("%04d%02d%02d %02d:%02d:%02d", tmp_dt.year,tmp_dt.month,tmp_dt.day,tmp_dt.hour,tmp_dt.minute,tmp_dt.second);
		#endif

			if(CheckSystemDateTimeIsValid(tmp_dt))
			{
				tmp_dt.week = GetWeekDayByDate(tmp_dt);
				memcpy(&date_time, &tmp_dt, sizeof(sys_date_timer_t));
				SaveSystemDateTime();
				server_has_timed_flag = true;
				
			#ifdef CONFIG_IMU_SUPPORT
			#ifdef CONFIG_STEP_SUPPORT
				StepsDataInit(true);
			#endif
			#ifdef CONFIG_SLEEP_SUPPORT
				SleepDataInit(true);
			#endif
			#endif
			}

			flag = true;			
		}
		else if(strcmp(strcmd, "S29") == 0)
		{
			//后台下发位置信息
		#ifdef LTE_DEBUG
			LOGD("%s", strdata);
		#endif
			uint8_t *ptr,*ptr1;
			uint8_t strtmp[256] = {0};
		
			//后台下发定位上报间隔
			ptr = strstr(strdata, ",");
			if(ptr != NULL)
			{
				memcpy(strtmp, strdata, (ptr-strdata));
				switch(atoi(strtmp))
				{
				case 1://SOS Alarm
					SOSRecLocatNotify(ptr+1);
					break;
				case 2://Fall Alarm
					//FallRecLocatNotify(ptr+1);
					break;
				}
			}
		}

		SaveSystemSettings();

		if(flag)
		{			
			strcmd[0] = 'T';
			MqttSendData(strcmd, strlen(strcmd));
		}
	}
}

void DecodeNetWorkDateTime(uint8_t *buffer, uint32_t len)
{
	uint8_t tz_count,tz_dir[3] = {0};
	uint8_t *ptr,strbuf[8] = {0};
	sys_date_timer_t tmp_dt = {0};

	ptr = buffer;
	
	memcpy(strbuf, ptr, 2);
	tmp_dt.year = 2000+atoi(strbuf);

	ptr+=3;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);
	tmp_dt.month= atoi(strbuf);

	ptr+=3;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);
	tmp_dt.day = atoi(strbuf);

	ptr+=3;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);
	tmp_dt.hour = atoi(strbuf);

	ptr+=3;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);
	tmp_dt.minute = atoi(strbuf);

	ptr+=3;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);
	tmp_dt.second = atoi(strbuf);

	ptr+=2;
	memcpy(tz_dir, ptr, 1);

	ptr+=1;
	memset(strbuf, 0, sizeof(strbuf));
	memcpy(strbuf, ptr, 2);

	tz_count = atoi(strbuf);
	if(tz_dir[0] == '+')
	{
		TimeIncrease(&tmp_dt, tz_count*15);
	}
	else if(tz_dir[0] == '-')
	{
		TimeDecrease(&tmp_dt, tz_count*15);
	}

	strcpy(g_timezone, tz_dir);
	sprintf(strbuf, "%d", tz_count/4);
	strcat(g_timezone, strbuf);

	if(CheckSystemDateTimeIsValid(tmp_dt))
	{
		tmp_dt.week = GetWeekDayByDate(tmp_dt);
		memcpy(&date_time, &tmp_dt, sizeof(sys_date_timer_t));
		SaveSystemDateTime();

	#ifdef CONFIG_IMU_SUPPORT
	#ifdef CONFIG_STEP_SUPPORT
		StepsDataInit(true);
	#endif
	#ifdef CONFIG_SLEEP_SUPPORT
		SleepDataInit(true);
	#endif
	#endif	
	}

#ifdef LTE_DEBUG
	LOGD("%04d-%02d-%02d,%02d:%02d:%02d,%d,%s", date_time.year, date_time.month, date_time.day, 
												date_time.hour, date_time.minute, date_time.second, 
												date_time.week, 
												g_timezone);
#endif	
}

void SetNwtWorkMqttBroker(uint8_t *imsi_buf)
{
	if(strncmp(imsi_buf, "460", strlen("460")) == 0)
	{
		strcpy(broker_hostname, CONFIG_MQTT_DOMESTIC_BROKER_HOSTNAME);
		broker_port = CONFIG_MQTT_DOMESTIC_BROKER_PORT;
	}
	else
	{
		strcpy(broker_hostname, CONFIG_MQTT_FOREIGN_BROKER_HOSTNAME);
		broker_port = CONFIG_MQTT_FOREIGN_BROKER_PORT;
	}
}

void lte_receive_data_handle(uint8_t *data, uint32_t data_len)
{
	uint8_t *ptr1,*ptr2;
	uint8_t tmpbuf[256] = {0};
	
#ifdef LTE_DEBUG
	LOGD("len:%d, data:%s", data_len, data);
#endif

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_BOOT)) != NULL)
	{
	#ifdef LTE_DEBUG
		LOGD("REBOOT:%s", tmpbuf);
	#endif

		lte_net_link();
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_IP)) != NULL)
	{
		ptr1 += strlen(LTE_SNS521S_RECE_IP);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			//+CGPADDR: <cid>[,<PDP_addr_1>[,<PDP_addr_2>]]
			//<cid>取值范围：0-10
			//<PDP_addr_1> <PDP_addr_1>和<PDP_addr_2>：每个都是一个字符串类型，用于标识适用
			//<PDP_addr_2>	于 PDP 的地址空间中的 MT。 如果没有可用的，则省略<PDP_addr_1>和<PDP_addr_2> 。
			//				当同时分配了 IPv4 和 IPv6 地址时，将同时包含<PDP_addr_1>和<PDP_addr_2>，
			//				其中<PDP_addr_1>包含 IPv4 地址，而<PDP_addr_2>包含 IPv6 地址。字符串以点分隔的数值(0-255)形式给出:
			//				a1.a2.a3.a4 表示 IPv4
			//				a1.a2.a3.a4.a5.a6 表示 IPv6.
			uint8_t strbuf[128] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			if(strlen(strbuf) > 0)
			{
			#ifdef LTE_DEBUG
				LOGD("register to network! %s", tmpbuf);
			#endif

				lte_relink_count = 0;
				lte_connected = true;
				k_timer_stop(&lte_net_link_timer);

				lte_get_net_time();
				k_timer_start(&lte_get_rsrp_timer, K_SECONDS(1), K_SECONDS(60));

				lte_mqtt_link();
			}
			else
			{
				k_timer_start(&lte_get_ip_timer, K_SECONDS(1), K_NO_WAIT);
			}
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MANU_INFOR)) != NULL)
	{
		ptr1 += strlen(LTE_SNS521S_RECE_MANU_INFOR);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("MANU_INFOR:%s", tmpbuf);
		#endif
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MANU_MODE)) != NULL)
	{	
		ptr1 += strlen(LTE_SNS521S_RECE_MANU_MODE);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("MANU_MODE:%s", tmpbuf);
		#endif
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_HW_VER)) != NULL)
	{	
		ptr1 += strlen(LTE_SNS521S_RECE_HW_VER);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("HW_VER:%s", tmpbuf);
		#endif
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_SW_VER)) != NULL)
	{	
		ptr1 += strlen(LTE_SNS521S_RECE_SW_VER);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("SW_VER:%s", tmpbuf);
		#endif
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_GET_IMEI)) != NULL)
	{	
		//AT+CGSN
		//862451058674047
		ptr1 += strlen(LTE_SNS521S_GET_IMEI);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(g_imei, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("IMEI:%s", g_imei);
		#endif	
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_GET_IMSI)) != NULL)
	{	
		//AT+CIMI
		//460082515704673
		ptr1 += strlen(LTE_SNS521S_GET_IMSI);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(g_imsi, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("IMSI:%s", g_imsi);
		#endif

			SetNwtWorkMqttBroker(g_imsi);
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_ICCID)) != NULL)
	{	
		//+NCCID:898604A52121C0114673
		ptr1 += strlen(LTE_SNS521S_RECE_ICCID);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memcpy(g_iccid, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("ICCID:%s", g_iccid);
		#endif	
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_CESQ)) != NULL)
	{	
		//+CESQ:99,99,255,255,32,88
		ptr1 += strlen(LTE_SNS521S_RECE_CESQ);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			uint8_t strbuf[8] = {0};

			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 6, strbuf);
			g_rsrp = atoi(strbuf);
		#ifdef LTE_DEBUG
			LOGD("RSRP:%d", g_rsrp);
		#endif	
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_CCLK)) != NULL)
	{	
		//+CCLK:24/10/30,09:22:10+32
		ptr1 += strlen(LTE_SNS521S_RECE_CCLK);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("CCLK:%s", tmpbuf);
		#endif			
			DecodeNetWorkDateTime(tmpbuf, ptr2-ptr1);
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CILENT)) != NULL)
	{
		//+ECMTOPEN:<tcpconnectID>,<result>
		if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE);
			#endif

				lte_mqtt_client_set_session();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_SESSION)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_SESSION);
			#endif

				lte_mqtt_client_set_timeout();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_SESSION);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT);
			#endif

				lte_mqtt_client_set_version();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_WILL)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_WILL);
			#endif

				//lte_mqtt_client_set_version();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_WILL);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_VERSION)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_VERSION);
			#endif

				lte_mqtt_client_set_cloud();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_VERSION);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_ALIAUTH)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_ALIAUTH);
			#endif	
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_ALIAUTH);
			#endif
			}
		}
		else if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_CLOUD)) != NULL)
		{
			if((ptr2 = strstr(data, LTE_SNS521S_RECE_OK)) != NULL)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s sucsuccess!", LTE_SNS521S_SET_MQTT_CLIENT_CLOUD);
			#endif

				lte_mqtt_open();
			}
			else
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt cfg %s fail!", LTE_SNS521S_SET_MQTT_CLIENT_CLOUD);
			#endif
			}
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_OPEN)) != NULL)
	{
		//+ECMTOPEN:<tcpconnectID>,<result>
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_OPEN);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt open:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			ret = atoi(strbuf);
			if(ret == 0)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt open success!");
			#endif
				lte_mqtt_connect();
			}
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_CON)) != NULL)
	{
		//+ECMTCONN:<tcpconnectID>,<result>[,<ret_code>]
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_CON);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt connect:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			ret = atoi(strbuf);
			if(ret == 0 || ret == 1)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt connect success!");
			#endif
				lte_mqtt_subscribe();
			}

			memset(strbuf, 0, sizeof(strbuf));
			GetStringInforBySepa(tmpbuf, ",", 3, strbuf);
			ret = atoi(strbuf);
			switch(ret)
			{
			case 0:
				//连接服务器成功。
				LOGD("Connect success!");
				break;
			case 1:
				//连接服务器被拒绝 – 错误的协议版本。
				LOGD("Connect reject: version err.");
				break;
			case 2:
				//连接服务器被拒绝 – 错误的客户端 ID。
				LOGD("Connect reject: client err.");
				break;
			case 3:
				//连接服务器被拒绝 – 找不到服务器。
				LOGD("Connect reject: server not found.");
				break;
			case 4:
				//连接服务器被拒绝 – 用户名或者密码错误。
				LOGD("Connect reject: username or password err.");
				break;
			case 5:
				//连接服务器被拒绝 – 认证失败。
				LOGD("Connect reject: authentication failed.");
				break;
			case 6:
				//连接服务器失败。
				LOGD("Connect failed.");
				break;
			}
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_SUB)) != NULL)
	{
		//+ECMTSUB:<tcpconnectID>,<msgID>,<result>[,<value>]
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_SUB);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt subscribe:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 3, strbuf);
			ret = atoi(strbuf);
			if(ret == 0 || ret == 1)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt subscribe success!");
			#endif

				mqtt_relink_count = 0;
				mqtt_connected = true;
				k_timer_stop(&lte_mqtt_link_timer);

				if(power_on_data_flag)
				{
					power_on_data_flag = false;
					SendPowerOnData();
					SendSettingsData();
				}
				
				MqttSendDataStart();
			}
		}
	}
	
	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_STAT)) != NULL)
	{
		//+ECMTSTAT: <tcpconnectID>,<err_code>
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_STAT);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt stat:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			mqtt_connected = atoi(strbuf);
		#ifdef LTE_DEBUG
			LOGD("mqtt_connected:%d", mqtt_connected);
		#endif
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_DATA)) != NULL)
	{
		//+ECMTRECV:<tcpconnectID>,<msgID>,<topic>,<data>
		uint8_t i = 3;
		
	#ifdef LTE_DEBUG
		LOGD("mqtt rece:%s", ptr1);
	#endif
	
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_DATA);
		while(i-- > 0)
		{
			ptr1 = strstr(ptr1, ",");
			if(ptr1 != NULL)
				ptr1++;
		}

		ptr1++;
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			ParseData(ptr1, ptr2-ptr1);
		}
	}
}

void LteSendAlarmData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T0:");
	strcat(buf, data);
	strcat(buf, ",");
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG	
	LOGD("alarm data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendSosWifiData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T1:");
	strcat(buf, data);
	strcat(buf, ",");
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG	
	LOGD("sos wifi data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendSosGpsData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{T2,");
	strcat(buf, g_imei);
	strcat(buf, ",[");
	strcat(buf, data);
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "]}");
#ifdef LTE_DEBUG	
	LOGD("sos gps data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

#ifdef CONFIG_IMU_SUPPORT
void LteSendFallWifiData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T3:");
	strcat(buf, data);
	strcat(buf, ",");
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG	
	LOGD("fall wifi data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendFallGpsData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{T4,");
	strcat(buf, g_imei);
	strcat(buf, ",[");
	strcat(buf, data);
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "]}");
#ifdef LTE_DEBUG
	LOGD("fall gps data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}
#endif

void LteSendLocationData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T6:");
	strcat(buf, data);
	strcat(buf, ",");
	memset(tmpbuf, 0, sizeof(tmpbuf));
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG
	LOGD("location data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendTimelyWristOffData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[128] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T34:");
	strcat(buf, data);
	strcat(buf, ",");
	GetBatterySocString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, ",");
	memset(tmpbuf, 0, sizeof(tmpbuf));
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG
	LOGD("data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendSettingsData(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[32] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T19:");
	strcat(buf, data);
	strcat(buf, ",");
	memset(tmpbuf, 0, sizeof(tmpbuf));
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG
	LOGD("settings data:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendPowerOnInfor(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[20] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T12:");
	strcat(buf, data);
	strcat(buf, ",");
	memset(tmpbuf, 0, sizeof(tmpbuf));
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG
	LOGD("pwr on infor:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

void LteSendPowerOffInfor(uint8_t *data, uint32_t datalen)
{
	uint8_t buf[256] = {0};
	uint8_t tmpbuf[20] = {0};
	
	strcpy(buf, "{1:1:0:0:");
	strcat(buf, g_imei);
	strcat(buf, ":T13:");
	strcat(buf, data);
	strcat(buf, ",");
	memset(tmpbuf, 0, sizeof(tmpbuf));
	GetSystemTimeSecString(tmpbuf);
	strcat(buf, tmpbuf);
	strcat(buf, "}");
#ifdef LTE_DEBUG
	LOGD("pwr off infor:%s", buf);
#endif

	MqttSendData(buf, strlen(buf));
}

bool lte_is_connected(void)
{
	return lte_connected;
}

void lte_turn_off(void)
{
#ifdef LTE_DEBUG
	LOGD("begin");
#endif

	UartLteOff();
}

void lte_get_net_time(void)
{
	LteSendData(LTE_SNS521S_GET_CCLK, strlen(LTE_SNS521S_GET_CCLK));
}

void lte_get_rsrp(void)
{
	LteSendData(LTE_SNS521S_GET_CESQ, strlen(LTE_SNS521S_GET_CESQ));
}

void lte_get_ip(void)
{
	LteSendData(LTE_SNS521S_GET_IP, strlen(LTE_SNS521S_GET_IP));
}

void lte_ue_reboot(void)
{
	//Reboot UE
	LteSendData(LTE_SNS521S_SET_REBOOT, strlen(LTE_SNS521S_SET_REBOOT));
}

void lte_get_infor(void)
{
#ifdef LTE_DEBUG
	LOGD("begin");
#endif

	//Turn of cmd reshow
	LteSendData(LTE_SNS521S_SET_CMD_RESHOW_ON, strlen(LTE_SNS521S_SET_CMD_RESHOW_ON));
	//Get manufacturer infor
	LteSendData(LTE_SNS521S_GET_MANU_INFOR, strlen(LTE_SNS521S_GET_MANU_INFOR));
	//Get manufacturer mode
	LteSendData(LTE_SNS521S_GET_MANU_MODEL, strlen(LTE_SNS521S_GET_MANU_MODEL));
	//Get hw version
	LteSendData(LTE_SNS521S_GET_HW_VERSION, strlen(LTE_SNS521S_GET_HW_VERSION));
	//Get sw version
	LteSendData(LTE_SNS521S_GET_SW_VERSION, strlen(LTE_SNS521S_GET_SW_VERSION));
	//Get IMEI
	LteSendData(LTE_SNS521S_GET_IMEI, strlen(LTE_SNS521S_GET_IMEI));
	//Get IMSI
	LteSendData(LTE_SNS521S_GET_IMSI, strlen(LTE_SNS521S_GET_IMSI));
	//Get ICCID
	LteSendData(LTE_SNS521S_GET_ICCID, strlen(LTE_SNS521S_GET_ICCID));

	lte_ue_reboot();
}

void lte_get_err(int32_t err_id)
{
	switch(err_id)
	{
	case 1:
		//未连接
		LOGD("MT not connection MT");
		break;
	case 2:
		//链接保留
		LOGD("MT link reserved MT");
		break;
	case 3:
		//不允许操作
		LOGD("operation not allowed");
		break;
	case 4:
		//不支持操作
		LOGD("operation not supported");
		break;
	case 5:
		//需要PH-SIM PIN
		LOGD("PH-SIM PIN required");
		break;
	case 6:
		//需要PH-FSIM PIN
		LOGD("PH-FSIM PIN required");
		break;
	case 7:
		//需要PH-FSIM PUK
		LOGD("PH-FSIM PUK required");
		break;
	case 10:
		//未插入SIM 卡
		LOGD("SIM not inserted");
		break;
	case 11:
		//需要SIM PIN
		LOGD("SIM PIN required");
		break;
	case 12:
		//需要SIM PUK
		LOGD("SIM PUK required");
		break;
	case 13:
		//SIM卡失败
		LOGD("SIM failure");
		break;
	case 14:
		//SIM 卡忙碌
		LOGD("SIM busy");
		break;
	case 15:
		//SIM 卡错误
		LOGD("SIM wrong");
		break;
	case 16:
		//密码不正确
		LOGD("incorrect password");
		break;
	case 17:
		//需要 SIM PIN2
		LOGD("SIM PIN2 required");
		break;
	case 18:
		//需要 SIM PUK2
		LOGD("SIM PUK2 required");
		break;
	case 20:
		//内存已满
		LOGD("memory full");
		break;
	case 21:
		//无效索引
		LOGD("invalid index");
		break;
	case 22:
		//未发现
		LOGD("not found");
		break;
	case 23:
		//内存不足
		LOGD("memory failure");
		break;
	case 24:
		//文本字符串过长
		LOGD("text string too long");
		break;
	case 25:
		//无效文本字符串
		LOGD("invalid characters in text string");
		break;
	case 26:
		//拨号字符串过长
		LOGD("dial string too long");
		break;
	case 27:
		//无效拨号字符串
		LOGD("invalid characters in dial string");
		break;
	case 30:
		//无网络服务
		LOGD("no network service");
		break;
	case 31:
		//网络超时
		LOGD("network timeout");
		break;
	case 32:
		//网络不允许-仅紧急呼叫
		LOGD("network not allowed-emergency call only");
		break;
	case 40:
		//需要网络个性化 PIN
		LOGD("network personalization PIN required");
		break;
	case 41:
		//需要网络个性化 PUK
		LOGD("network personalization PUK required");
		break;
	case 42:
		//需要网络子集个性化 PIN 
		LOGD("network subset personalization PIN required");
		break;
	case 43:
		//需要网络子集个性化 PUK
		LOGD("network subset personalization PUK required");
		break;
	case 44:
		//需要服务提供商个性化 PIN
		LOGD("service provider personalization PIN required");
		break;
	case 45:
		//需要服务提供商个性化 PUK
		LOGD("service provider personalization PUK required");
		break;
	case 46:
		//需要企业个性化 PIN
		LOGD("corporate personalization PIN required");
		break;
	case 47:
		//需要企业个性化 PUK
		LOGD("corporate personalization PUK required");
		break;
	case 48:
		//需要隐藏密钥
		LOGD("hidden key required");
		break;
	case 49:
		//EAP 方法不支持
		LOGD("EAP method not support");
		break;
	case 50:
		//无效参数
		LOGD("incorrect Parameters");
		break;
	case 51:
		//命令已实现，但当前已禁用
		LOGD("command implemented but currently disabled");
		break;
	case 52:
		//命令被用户终止
		LOGD("command aborted by user");
		break;
	case 53:
		//由于 MT 功能受限，未注册到网络
		LOGD("not attached to network due to MT functionalityrestrictions");
		break;
	case 54:
		//不允许- MT 仅限于紧急呼叫
		LOGD("modem not allowed-MT restricted to emergencycalls onlyModem");
		break;
	case 55:
		//由于 MT 功能受限，操作不允许
		LOGD("operation not allowed because of MTfunctionality restrictions");
		break;
	case 56:
		//仅允许固定拨号-已拨号码非固话
		LOGD("fixed dial number only allowed – called number isnot a fixed");
		break;
	case 57:
		//由于其他 MT 使用，暂时停用
		LOGD("temporarily out of service due to other MT usage");
		break;
	case 58:
		//不支持的语言/字母
		LOGD("language/alphabet not supported");
		break;
	case 59:
		//数据值超出范围
		LOGD("unexpected data value");
		break;
	case 60:
		//系统发生问题
		LOGD("system failure");
		break;
	case 61:
		//数据丢失
		LOGD("data missing");
		break;
	case 62:
		//禁止通话
		LOGD("call barred");
		break;
	case 63:
		//消息等待指示订阅失败
		LOGD("message waiting indication subscription failure");
		break;
	case 100:
		//未知
		LOGD("unknown");
		break;
	case 103:
		//非法 MS
		LOGD("illegal MS");
		break;
	case 106:
		//非法 ME
		LOGD("illegal ME");
		break;
	case 107:
		//不允许的 GPRS 服务
		LOGD("GPRS services not allowed");
		break;
	case 108:
		//不允许的 GPRS 服务和非 GPRS 服务
		LOGD("GPRS services and non GPRS services not allowed");
		break;
	case 111:
		//不允许 PLMN
		LOGD("PLMN not allowed");
		break;
	case 112:
		//不允许的位置区域
		LOGD("location area not allowed");
		break;
	case 113:
		//在此位置区域内不允许漫游
		LOGD("roaming not allowed in this location area");
		break;
	case 114:
		//此 PLMN 不允许使用 GPRS 服务
		LOGD("GPRS services not allowed in this PLMN");
		break;
	case 115:
		//位置区域内没有合适的小区
		LOGD("No suitable cells in location area");
		break;
	case 122:
		//拥塞
		LOGD("Congestion");
		break;
	case 126:
		//资源不足
		LOGD("Insufficient resources");
		break;
	case 127:
		//任务或未知 APN
		LOGD("Mission or unknown APN");
		break;
	case 128:
		//未知 PDP 地址或 PDP 类型
		LOGD("Unknown PDP address or PDP type");
		break;
	case 129:
		//用户鉴权失败
		LOGD("User authentication failed");
		break;
	case 130:
		//GW 被 GGSN 服务 GW 或 PDN GW 主动拒绝
		LOGD("Active reject by GGSN services GW or PDN");
		break;
	case 131:
		//未指定主动拒绝
		LOGD("Active reject unspecified");
		break;
	case 132:
		//服务选项不支持
		LOGD("service option not supported");
		break;
	case 133:
		//未订阅请求服务选项
		LOGD("requested service option not subscribed");
		break;
	case 134:
		//服务选项暂时失灵
		LOGD("service option temporarily out of order");
		break;
	case 140:
		//不支持该功能
		LOGD("Feature not supported");
		break;
	case 141:
		//TFT 操作语义错误
		LOGD("Semantic errors in the TFT operation");
		break;
	case 142:
		//TFT 操作中的句法错误
		LOGD("Syntactical errors in the TFT operation");
		break;
	case 143:
		//未知 PDP 上下文
		LOGD("Unknown PDP context");
		break;
	case 144:
		//数据包过滤器中的语义错误
		LOGD("Semantic errors in packet filters");
		break;
	case 145:
		//数据包过滤器中的句法错误
		LOGD("Syntactical errors in packet filters");
		break;
	case 146:
		//没有激活 TFT 的 PDP 上下文
		LOGD("PDP context without TFT already activated");
		break;
	case 148:
		//未指定的 GPRS 错误
		LOGD("unspecified GPRS error");
		break;
	case 149:
		//PDP 认证失败
		LOGD("PDP authentication failure");
		break;
	case 150:
		//无效移动类
		LOGD("invalid mobile class");
		break;
	case 171:
		//不允许最后一个 PDN 断开连接
		LOGD("Last PDN disconnection not allowed");
		break;
	case 172:
		//消息语义不正确
		LOGD("Semantically incorrect message");
		break;
	case 173:
		//强制性信息元素错误
		LOGD("Mandatory information element error");
		break;
	case 174:
		//信息元素不存在或未实现
		LOGD("Information element not existent or notimplemented");
		break;
	case 175:
		//有条件 IE 错误
		LOGD("Conditional IE error");
		break;
	case 176:
		//未指定的协议错误
		LOGD("Protocol error unspecified");
		break;
	case 177:
		//运营商决定的限制
		LOGD("Operator determined barring");
		break;
	case 178:
		//达到 PDP 上下文的最大数量
		LOGD("Max number of PDP contexts reached");
		break;
	case 179:
		//当前的 RAT 和 PLMN 组合不支持请求的 APN
		LOGD("Requested APN not supported in current RAT and PLMN combination");
		break;
	case 180:
		//请求拒绝承载控制模式冲突
		LOGD("Request rejected bearer control mode violation");
		break;
	case 181:
		//不支持的 OCI 值
		LOGD("Unsupported OCI value");
		break;
	case 182:
		//通过控制平面的用户数据传输拥塞
		LOGD("User data transmission via control plane iscongested");
		break;
	case 301:
		//内部错误基础
		LOGD("Internal error base");
		break;
	case 302:
		//UE 繁忙
		LOGD("UE busy");
		break;
	case 303:
		//未开机
		LOGD("Not power on");
		break;
	case 304:
		//未激活 PDN
		LOGD("PDN not active");
		break;
	case 305:
		//无需 PDN
		LOGD("PDN not valid");
		break;
	case 306:
		//无需 PDN 类型
		LOGD("PDN invalid type");
		break;
	case 307:
		//PDN 无参数
		LOGD("PDN no parameter");
		break;
	case 308:
		//UE 失败
		LOGD("UE fail");
		break;
	case 309:
		//重复的 PDN 和 APN
		LOGD("PDN type and APN duplicate used");
		break;
	case 310:
		//PAP 和 EITF 不匹配
		LOGD("PAP and EITF not matched");
		break;
	case 311:
		//SIM 卡 PIN 码已禁用
		LOGD("SIM PIN disabled");
		break;
	case 312:
		//SIM 卡 PIN 码已启用
		LOGD("SIM PIN already enabled");
		break;
	case 313:
		//SIM PIN 格式错误
		LOGD("SIM PIN wrong format");
		break;
	case 512:
		//未配置必需的参数
		LOGD("Required parameter not configured");
		break;
	case 514:
		//AT 内部错误
		LOGD("AT internal error");
		break;
	case 515:
		//CID 已激活
		LOGD("CID is active");
		break;
	case 516:
		//命令状态错误
		LOGD("Incorrect state for command");
		break;
	case 517:
		//CID 无效
		LOGD("CID is invalid");
		break;
	case 518:
		//CID 未激活
		LOGD("CID is not active");
		break;
	case 520:
		//停用最后一个已激活的 CID
		LOGD("Deactivate the last active CID");
		break;
	case 521:
		//CID 未定义
		LOGD("CID is not defined");
		break;
	case 522:
		//奇偶校验错误
		LOGD("UART parity error UART");
		break;
	case 523:
		//UART 帧错误
		LOGD("UART frame error");
		break;
	case 524:
		//UE 处于最小功能模式
		LOGD("UE is in minimal function mode");
		break;
	case 525:
		//AT 命令中止：处理中
		LOGD("AT command aborted: in processing");
		break;
	case 526:
		//AT 命令中止：出错
		LOGD("AT command aborted: error");
		break;
	case 527:
		//命令中断
		LOGD("Command interrupted");
		break;
	case 528:
		//配置冲突
		LOGD("Configuration conflicts");
		break;
	case 529:
		//在 FOTA 升级中
		LOGD("During FOTA updating");
		break;
	case 530:
		//非 AT 分配的 Socket
		LOGD("Not the AT allocated socket");
		break;
	case 531:
		//USIM PIN 被阻止
		LOGD("USIM PIN is blocked");
		break;
	case 532:
		//USIM PUK 被阻止
		LOGD("USIM PUK is blocked");
		break;
	case 533:
		//非 mipi 模块
		LOGD("Not mipi module");
		break;
	case 534:
		//文件未找到
		LOGD("File not found");
		break;
	case 535:
		//使用条件不满足
		LOGD("Conditions of use not satisfied");
		break;
	case 536:
		//AT UART 缓冲区错误
		LOGD("AT UART buffer error");
		break;
	case 537:
		//退避计时器正在运行
		LOGD("Back off timer is running");
		break;
	}
}

void lte_net_clear_search(void)
{
#ifdef LTE_DEBUG
	LOGD("begin");
#endif

	//Turn off
	LteSendData(LTE_SNS521S_SET_FUN_OFF, strlen(LTE_SNS521S_SET_FUN_OFF));
	//Clear search records
	LteSendData(LTE_SNS521S_SET_CLEAR_SEARCH, strlen(LTE_SNS521S_SET_CLEAR_SEARCH));
}

void lte_net_link(void)
{
#ifdef LTE_DEBUG
	LOGD("begin");
#endif	
	
	//Turn on error report
	LteSendData(LTE_SNS521S_SET_ERR_REPORT, strlen(LTE_SNS521S_SET_ERR_REPORT));
	//Turn on RF
	LteSendData(LTE_SNS521S_SET_FUN_FULL, strlen(LTE_SNS521S_SET_FUN_FULL));
	//Attach request
	LteSendData(LTE_SNS521S_SET_ATTACH_NETWAOK, strlen(LTE_SNS521S_SET_ATTACH_NETWAOK));
	//Get PDP IP
	LteSendData(LTE_SNS521S_GET_IP, strlen(LTE_SNS521S_GET_IP));
	
	k_timer_start(&lte_net_link_timer, K_SECONDS(2*60), K_NO_WAIT);
}

void LTE_init(void)
{
	uart_lte_init();
	lte_get_infor();
}

void LteMsgProcess(void)
{
	if(lte_get_infor_flag)
	{
		lte_get_infor();
		lte_get_infor_flag = false;
	}

	if(lte_get_ip_flag)
	{
		lte_get_ip();
		lte_get_ip_flag = false;
	}

	if(lte_get_rsrp_flag)
	{
		lte_get_rsrp();
		lte_get_rsrp_flag = false;
	}
	
	if(lte_turn_off_flag)
	{
		lte_turn_off();
		lte_turn_off_flag = false;
	}

	if(lte_net_link_flag)
	{
		k_timer_stop(&lte_get_ip_timer);
		
		lte_net_clear_search();
		lte_relink_count++;
		if(lte_relink_count < LTE_NET_RELINK_MAX)
		{
			lte_ue_reboot();
		}
		else
		{
		}
		
		lte_net_link_flag = false;
	}

	if(lte_mqtt_link_flag)
	{
		mqtt_relink_count++;
		if(mqtt_relink_count < MQTT_RELINK_MAX)
		{
			lte_mqtt_link();
		}
		else
		{
		}
		
		lte_mqtt_link_flag = false;
	}

	if(lte_mqtt_send_flag)
	{
		LteMqttSendData();
		lte_mqtt_send_flag = false;
	}
}

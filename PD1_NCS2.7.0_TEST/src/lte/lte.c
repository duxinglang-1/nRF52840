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

//#define LTE_DEBUG

static bool lte_connect_ok_flag = true;
static bool power_on_data_flag = true;

static bool lte_connected = false;
static LTE_MQTT_STATUS mqtt_connected = LTE_MQTT_CLOSED;

static bool lte_get_infor_flag = false;
static bool lte_get_ip_flag = false;
static bool lte_get_rsrp_flag = false;
static bool lte_turn_off_flag = false;
static bool lte_net_link_flag = false;
static bool lte_mqtt_discon_flag = false;
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
static void lte_mqtt_disconnect_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(lte_mqtt_discon_timer, lte_mqtt_disconnect_timerout, NULL);
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

static void lte_mqtt_disconnect_timerout(struct k_timer *timer_id)
{
	lte_mqtt_discon_flag = true;
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
	//<clean_session> ���͡� ���ûỰ���͡�0���Ͽ����Ӻ󣬷���������洢�ͻ��˵Ķ��ġ�1���������˶���֮ǰΪ�ÿͻ��˱������κ���Ϣ������������Ϊ��clean�����������
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,0\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_SESSION);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_client_set_timeout(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt msg send timeout
	//AT+ECMTCFG="timeout",<tcpconnectID>[,<pkt_timeout>[,<retry_times>][,<timeout_notice>]]
	//<pkt_timeout> ���͡����ݰ����ݳ�ʱʱ�䡣 ��Χ�� 1-60�� Ĭ��ֵΪ 10����λ���롣
	//<retry_times> ����. (��֧��)���ݰ����ݳ�ʱ������ʱ�䡣 ��Χ�� 0-10�� Ԥ��ֵΪ 3��
	//<timeout_notice> ����. (��֧��)0���������ݰ�ʱ�����泬ʱ��Ϣ��1���������ݰ�ʱ���泬ʱ��Ϣ��
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,10,3,1\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_will(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt will
	//AT+ECMTCFG="will",<tcpconnectID>[,<will_fg>[,<will_qos>,<will_retain>,<"will_topic">,<"will_msg">]]
	//<will_fg> ���͡� ���� Will ��־��0������ Will ��־���á�1����Ҫ Will ��־���á�
	//<will_qos> ���͡� ��Ϣ���ݵķ���������0�����һ�Ρ�1������һ�Ρ�2������һ�Ρ�
	//<will_retain> ���͡�Will ������־�����ڷ�����Ϣ��0�����ͻ�������������ͷ�����Ϣʱ���������ڽ���Ϣ���ݸ���ǰ�����󽫲���������Ϣ��1�����ͻ�������������� PUBLISH ��Ϣʱ���������ڽ���Ϣ���ݸ���ǰ������Ӧ��������Ϣ��
	//<will_topic> ���͡�Will �����ַ���������СΪ 255 �ֽڡ�
	//<will_msg> ���͡�Will ��Ϣ�����˿ͻ�������Ͽ�����ʱ������ will �������Ϣ�����ݡ����������㳤�ȵ���Ϣ������СΪ 255 ���ֽڡ�
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",1,0,1,0,\"%s\",\"\"\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_WILL, CONFIG_MQTT_SUB_TOPIC);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_version(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt version
	//AT+ECMTCFG="version",<tcpconnectID>[,<version>]
	//<version> ���͡� MQTT Э��İ汾��Ĭ��Ϊ MQTT v3.1.1
	//									3��MQTT v3.1 ��
	//									4��MQTT v3.1.1��
	memset(cmdbuf, 0, sizeof(cmdbuf));
	sprintf(cmdbuf, "%s=\"%s\",0,4\r\n", LTE_SNS521S_SET_MQTT_CILENT, LTE_SNS521S_SET_MQTT_CLIENT_VERSION);
	LteSendData(cmdbuf, strlen(cmdbuf));	
}

void lte_mqtt_client_set_cloud(void)
{
	uint8_t cmdbuf[256] = {0};

	//Set mqtt cloud type
	//AT+ECMTCFG="cloud",<tcpconnectID>,<cloudtype>,<data type>
	//<cloud type > ���͡�
	//				0��mosquitto ƽ̨��1��OneNet ƽ̨��2�������ơ�
	//<data type> ���ͣ���Χ�� 0-255
	//					OneNet ��ƽ̨��������
	//						1 ��OneNet �������� 1��
	//						2 ��OneNet �������� 2��
	//						3 ��OneNet �������� 3��
	//						4 ��OneNet �������� 4��
	//						5 ��OneNet �������� 5��
	//						6 ��OneNet �������� 6��
	//					�����ƶ�������
	//						1 ��Json ���ݡ�
	//						2 ���ַ������ݡ�
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

	//Turn off reshow
	LteSendData(LTE_SNS521S_SET_CMD_RESHOW_OFF, strlen(LTE_SNS521S_SET_CMD_RESHOW_OFF));
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

	if(k_timer_remaining_get(&lte_mqtt_discon_timer) > 0)
		k_timer_stop(&lte_mqtt_discon_timer);
	if(k_timer_remaining_get(&lte_mqtt_link_timer) > 0)
		k_timer_stop(&lte_mqtt_link_timer);
	
	//AT+ECMTDISC=<tcpconnectID>
	sprintf(cmdbuf, "%s=0\r\n", LTE_SNS521S_SET_MQTT_DISCONNECT);
	LteSendData(cmdbuf, strlen(cmdbuf));
}

void lte_mqtt_subscribe(void)
{
	uint8_t cmdbuf[256] = {0};
	
	//AT+ECMTSUB=<tcpconnectID>,<msgID>,"<topic>",<qos>
	sprintf(cmdbuf, "%s=0,%d,\"%s%s\",1\r\n", LTE_SNS521S_SET_MQTT_SUB, mqtt_msg_id++, CONFIG_MQTT_SUB_TOPIC, g_imei);
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
	sprintf(cmdbuf, "%s=0,%d,2,0,\"%s\",\"%s\"\r\n", LTE_SNS521S_SET_MQTT_PUBLISH, mqtt_msg_id++, CONFIG_MQTT_PUB_TOPIC, data);
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
		switch(mqtt_connected)
		{
		case LTE_MQTT_CLOSED:
		case LTE_MQTT_OPENED:
			lte_mqtt_open();
			break;

		case LTE_MQTT_CONNECTED:
			break;
		}
	}
	
	k_timer_start(&lte_mqtt_link_timer, K_SECONDS(30), K_NO_WAIT);
}

void LteMqttSendData(void)
{
	uint8_t data_type,*p_data;
	uint32_t data_len;
	int ret;

	ret = get_data_from_cache(&mqtt_send_cache, &p_data, &data_len, &data_type);
	if(ret)
	{
		if(k_timer_remaining_get(&lte_mqtt_discon_timer) > 0)
			k_timer_stop(&lte_mqtt_discon_timer);
		k_timer_start(&lte_mqtt_discon_timer, K_SECONDS(CONFIG_MQTT_KEEPALIVE_TIME), K_NO_WAIT);
		
		lte_mqtt_publish(p_data, data_len);
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
	else if(mqtt_connected != LTE_MQTT_CONNECTED)
	{
		k_timer_start(&lte_mqtt_link_timer, K_SECONDS(1), K_NO_WAIT);
	}
	else
	{
		MqttSendDataStart();
	}
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

void ParseServerDownloadData(uint8_t *data, uint32_t datalen)
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

			//��̨�·���λ�ϱ����
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
			//��̨�·����������
			global_settings.health_interval = atoi(strdata);

			flag = true;
		}
		else if(strcmp(strcmd, "S9") == 0)
		{
			uint8_t tmpbuf[8] = {0};

			strcat(strdata, ",");
			
			//��̨�·�̧����������
			GetStringInforBySepa(strdata, ",", 1, tmpbuf);
			global_settings.wake_screen_by_wrist = atoi(tmpbuf);
			//��̨�·�����������
			GetStringInforBySepa(strdata, ",", 2, tmpbuf);
			global_settings.wrist_off_check = atoi(tmpbuf);
			//��̨�·�ˤ���������
			GetStringInforBySepa(strdata, ",", 3, tmpbuf);
			global_settings.fall_check = atoi(tmpbuf);
			
			flag = true;
		}
		else if(strcmp(strcmd, "S11") == 0)
		{
			uint8_t *ptr;
			uint8_t strtmp[128] = {0};
			
			//��̨�·�ѪѹУ׼ֵ
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

			//��̨�·����°汾��Ϣ
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

			//��̨�·�Уʱָ��
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
			//��̨�·�λ����Ϣ
		#ifdef LTE_DEBUG
			LOGD("%s", strdata);
		#endif
			uint8_t *ptr,*ptr1;
			uint8_t strtmp[256] = {0};
		
			//��̨�·���λ�ϱ����
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

void ParseNetWorkDateTime(uint8_t *buffer, uint32_t len)
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

void ParseRegisterStatusReport(uint8_t *data, uint32_t data_len)
{
	//+CEREG:2,0000,00000000,9,,,,
	//+CEREG: <stat>[,[<tac>],[<ci>],[<AcT>][,[<cause_type>],[<reject_cause>][,[<Active-Time>],[<Periodic-TAU>]]]]
	uint8_t tmpbuf[256]={0},strbuf[8]={0};
	uint8_t status,act,cause_type,reject_cause;
	uint8_t tac[5]={0},ci[9]={0},act_time[9]={0},tau_time[9]={0};

	if(data == NULL || data_len == 0)
		return;
	
	memcpy(tmpbuf, data, data_len);
	strcat(tmpbuf, ",");
	
	//status
	GetStringInforBySepa(tmpbuf, ",", 1, strbuf);
	status = atoi(strbuf);
	
	//tac
	GetStringInforBySepa(tmpbuf, ",", 2, tac);
#ifdef LTE_DEBUG
	LOGD("tac:%s", tac);
#endif	
	
	//ci
	GetStringInforBySepa(tmpbuf, ",", 3, ci);
#ifdef LTE_DEBUG
	LOGD("ci:%s", ci);
#endif

	//act
	memset(strbuf, 0, sizeof(strbuf));
	GetStringInforBySepa(tmpbuf, ",", 4, strbuf);
	act = atoi(strbuf);
	switch(act)
	{
	case 0:
		//GSM (������)
	#ifdef LTE_DEBUG
		LOGD("GSM (not applicable)");
	#endif	
		break;
	case 1:
		//GSM Compact (������)
	#ifdef LTE_DEBUG
		LOGD("GSM Compact (not applicable)");
	#endif	
		break;
	case 2:
		//UTRAN (������)
	#ifdef LTE_DEBUG
		LOGD("UTRAN (not applicable)");
	#endif	
		break;
	case 3:
		//GSM w/EGPRS (������)
	#ifdef LTE_DEBUG
		LOGD("GSM w/EGPRS (not applicable)");
	#endif	
		break;
	case 4:
		//UTRAN w/HSDPA (������)
	#ifdef LTE_DEBUG
		LOGD("UTRAN w/HSDPA (not applicable)");
	#endif	
		break;
	case 5:
		//UTRAN w/HSUPA (������)
	#ifdef LTE_DEBUG
		LOGD("UTRAN w/HSUPA (not applicable)");
	#endif	
		break;
	case 6:
		//UTRAN w/HSDPA �� HSUPA (������)
	#ifdef LTE_DEBUG
		LOGD("UTRAN w/HSDPA and HSUPA (not applicable)");
	#endif	
		break;
	case 7:
		//E-UTRAN (������)
	#ifdef LTE_DEBUG
		LOGD("E-UTRAN (not applicable)");
	#endif	
		break;
	case 8:
		//EC-GSM-IoT (A/Gb ģʽ) (������)
	#ifdef LTE_DEBUG
		LOGD("EC GSM IoT (A/Gb mode) (not applicable)");
	#endif	
		break;
	case 9:
		//E-UTRAN (NB-S1 ģʽ) 
	#ifdef LTE_DEBUG
		LOGD("E-UTRAN (NB-S1 mode)");
	#endif	
		break;
	}
	
	//cause_type
	memset(strbuf, 0, sizeof(strbuf));
	GetStringInforBySepa(tmpbuf, ",", 5, strbuf);
#ifdef LTE_DEBUG
	LOGD("cause_type:%s", strbuf);
#endif	
	if(strlen(strbuf) > 0)
	{
		cause_type = atoi(strbuf);
		switch(cause_type)
		{
		case 0:
			//ָʾ <reject_cause> ����һ�� EMM ԭ��ֵ
		#ifdef LTE_DEBUG
			LOGD("Indicate that<reject_cause>contains an EMM reason value");
		#endif	
			break;
		case 1:
			//ָʾ <reject_cause> �����������ض���ԭ��.
		#ifdef LTE_DEBUG
			LOGD("Indicate that<reject_cause>contains manufacturer specific reasons");
		#endif	
			break;
		}
	}
	
	//reject_cause
	memset(strbuf, 0, sizeof(strbuf));
	GetStringInforBySepa(tmpbuf, ",", 6, strbuf);
#ifdef LTE_DEBUG
	LOGD("reject_cause:%s", strbuf);
#endif
	if(strlen(strbuf) > 0)
	{
		reject_cause = atoi(strbuf);
	}
	
	//Active-Time
	GetStringInforBySepa(tmpbuf, ",", 7, act_time);
#ifdef LTE_DEBUG
	LOGD("act_time:%s", act_time);
#endif

	//Periodic-TAU
	GetStringInforBySepa(tmpbuf, ",", 8, tau_time);
#ifdef LTE_DEBUG
	LOGD("tau_time:%s", tau_time);
#endif

	switch(status)
	{
	case 0:
		//û��ע�����磬MT û�������µ�����
	#ifdef LTE_DEBUG
		LOGD("No registered network, MT did not search for new networks");
	#endif	
		break;
	case 1:
		//��ע�ᵽ��������
	#ifdef LTE_DEBUG
		LOGD("Registered to local network");
	#endif				
		break;
	case 2:
		//û��ע�����磬MT ���������µ�����
	#ifdef LTE_DEBUG
		LOGD("No registered network, MT is searching for new networks");
	#endif	
		break;
	case 3:
		//ע�ᱻ�ܾ�
	#ifdef LTE_DEBUG
		LOGD("Registration rejected");
	#endif	
		break;
	case 4:
		//δ֪(���� ���� E-UTRAN ���Ƿ�Χ)
	#ifdef LTE_DEBUG
		LOGD("Unknown (e.g. beyond the coverage range of E-UTRAN)");
	#endif	
		break;
	case 5:
		//�ɹ�ע����������
	#ifdef LTE_DEBUG
		LOGD("Successfully registered for roaming network");
	#endif	
		break;
	case 6:
		//��ע�ᵽ"SMS only"����(������)
	#ifdef LTE_DEBUG
		LOGD("Registered to the 'SMS only' network (not applicable)");
	#endif	
		break;
	case 7:
		//��ע�ᵽ"SMS only"��������(������)
	#ifdef LTE_DEBUG
		LOGD("Registered to the 'SMS only' roaming network (not applicable)");
	#endif	
		break;
	case 8:
		//�����ŵ��������з���(������)
	#ifdef LTE_DEBUG
		LOGD("Only attached to emergency call service (not applicable)");
	#endif	
		break;
	case 9:
		//��ע�ᵽ"CSFB not preferred"����(������)
	#ifdef LTE_DEBUG
		LOGD("Registered on the 'CSFB not preferred' network (not applicable)");
	#endif	
		break;
	case 10:
		//��ע�ᵽ"CSFB not preferred"��������(������)
	#ifdef LTE_DEBUG
		LOGD("Registered to the 'CSFB not preferred' roaming network (not applicable)");
	#endif	
		break;
	}

	if((status == 1) || (status == 5))
	{
		lte_relink_count = 0;
		lte_connected = true;
		k_timer_stop(&lte_net_link_timer);
		k_timer_start(&lte_get_rsrp_timer, K_SECONDS(5), K_SECONDS(60));
	}

	if((status == 1) || (status == 5)		//Registered to network
		|| (status == 6) || (status == 7)	//Registered to 'SMS only' network
		|| (status == 8)					//Registered to emergency
		|| (status == 9) || (status == 10)	//Registered to 'CSFB not preferred' network
		)
	{
		//Get PDP IP
		LteSendData(LTE_SNS521S_GET_IP, strlen(LTE_SNS521S_GET_IP));
	}
}

void ParseMqttClientInitCallBack(uint8_t *data, uint32_t data_len)
{
	uint8_t *ptr1,*ptr2;

	if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE)) != NULL)
	{
		//AT+ECMTCFG="keepalive",0,60
		//OK
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
		//AT+ECMTCFG="session",0,0
		//OK
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
		//AT+ECMTCFG="timeout",0,10,3,1
		//OK
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
		//AT+ECMTCFG="version",0,4
		//OK
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
		//
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
		//AT+ECMTCFG="cloud",0,0,2
		//OK
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
	
	if((ptr1 = strstr(data, LTE_SNS521S_RECE_CEREG)) != NULL)
	{
		ptr1 += strlen(LTE_SNS521S_RECE_CEREG);
		if((ptr2 = strstr(ptr1, "\r\n")) != NULL)
		{
			ParseRegisterStatusReport(ptr1, (ptr2-ptr1));
		}
	}
	
	if((ptr1 = strstr(data, LTE_SNS521S_RECE_IP)) != NULL)
	{
		ptr1 += strlen(LTE_SNS521S_RECE_IP);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			//+CGPADDR: <cid>[,<PDP_addr_1>[,<PDP_addr_2>]]
			//<cid>ȡֵ��Χ��0-10
			//<PDP_addr_1> <PDP_addr_1>��<PDP_addr_2>��ÿ������һ���ַ������ͣ����ڱ�ʶ����
			//<PDP_addr_2>	�� PDP �ĵ�ַ�ռ��е� MT�� ���û�п��õģ���ʡ��<PDP_addr_1>��<PDP_addr_2> ��
			//				��ͬʱ������ IPv4 �� IPv6 ��ַʱ����ͬʱ����<PDP_addr_1>��<PDP_addr_2>��
			//				����<PDP_addr_1>���� IPv4 ��ַ����<PDP_addr_2>���� IPv6 ��ַ���ַ����Ե�ָ�����ֵ(0-255)��ʽ����:
			//				a1.a2.a3.a4 ��ʾ IPv4
			//				a1.a2.a3.a4.a5.a6 ��ʾ IPv6.
			uint8_t strbuf[128] = {0};

			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			if(strlen(strbuf) > 0)
			{
			#ifdef LTE_DEBUG
				LOGD("IP:%s", tmpbuf);
			#endif	
			}

			lte_get_net_time();
			lte_mqtt_link();
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

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_CSCON)) != NULL)
	{	
		//+CSCON:<mode>
		//<mode> ����; ָʾ�ź�����״̬, 0:���� 1:������
		ptr1 += strlen(LTE_SNS521S_RECE_CSCON);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			uint8_t mode;
			uint8_t strbuf[8] = {0};

			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 1, strbuf);
			mode = atoi(strbuf);
		#ifdef LTE_DEBUG
			LOGD("CSCON:%d", mode);
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
			ParseNetWorkDateTime(tmpbuf, ptr2-ptr1);
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_SET_MQTT_CILENT)) != NULL)
	{
		ParseMqttClientInitCallBack(ptr1, data_len-(ptr1-data));
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_OPEN)) != NULL)
	{
		//+ECMTOPEN:<tcpconnectID>,<result>
		//<tcpconnectID> ���ͣ�MQTT �׽��ֱ�ʶ����ֵ�� 0
		//<result> ���ͣ�����ִ�н����-1��������ʧ�ܡ�0��������ɹ���
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

				mqtt_connected = LTE_MQTT_OPENED;
				lte_mqtt_connect();
			}
			else
			{
				lte_mqtt_close();
				lte_mqtt_open();
			}
		}
	}
	
	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_CLOSE)) != NULL)
	{
		//+ECMTCLOSE: <tcpconnectID>,<result>
		//<tcpconnectID> ���ͣ�MQTT �׽��ֱ�ʶ����ֵ�� 0��
		//<result> ���ͣ�����ִ�н����-1���ر� mqtt ʧ�ܡ�0���ر� mqtt �ɹ���
		//<err> ���ͣ�������
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_CLOSE);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt close:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			ret = atoi(strbuf);
			if(ret == 0)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt close success!");
			#endif

				mqtt_connected = LTE_MQTT_CLOSED;
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
			
				mqtt_relink_count = 0;
				mqtt_connected = LTE_MQTT_CONNECTED;
				k_timer_stop(&lte_mqtt_link_timer);
				
				lte_mqtt_subscribe();
			}

			memset(strbuf, 0, sizeof(strbuf));
			GetStringInforBySepa(tmpbuf, ",", 3, strbuf);
			ret = atoi(strbuf);
			switch(ret)
			{
			case 0:
				//���ӷ������ɹ���
				LOGD("Connect success!");
				break;
			case 1:
				//���ӷ��������ܾ� �C �����Э��汾��
				LOGD("Connect reject: version err.");
				break;
			case 2:
				//���ӷ��������ܾ� �C ����Ŀͻ��� ID��
				LOGD("Connect reject: client err.");
				break;
			case 3:
				//���ӷ��������ܾ� �C �Ҳ�����������
				LOGD("Connect reject: server not found.");
				break;
			case 4:
				//���ӷ��������ܾ� �C �û��������������
				LOGD("Connect reject: username or password err.");
				break;
			case 5:
				//���ӷ��������ܾ� �C ��֤ʧ�ܡ�
				LOGD("Connect reject: authentication failed.");
				break;
			case 6:
				//���ӷ�����ʧ�ܡ�
				LOGD("Connect failed.");
				break;
			}
		}
	}

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_DISCON)) != NULL)
	{
		//+ECMTDISC: <tcpconnectID>,<result>
		//<tcpconnectID> ���ͣ�MQTT �׽��ֱ�ʶ����ֵ�� 0��
		//<result> ���ͣ�����ִ�н����-1���Ͽ�����ʧ�ܡ�0���Ͽ����ӳɹ���
		//<err> ���ͣ�������
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_DISCON);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt disconnect:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			ret = atoi(strbuf);
			if(ret == 0)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt disconnect success!");
			#endif

				mqtt_connected = LTE_MQTT_OPENED;
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

	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_PUB)) != NULL)
	{
		//+ECMTPUB: <tcpconnectID>,<msgID>,<result>[,<value>]
		//<tcpconnectID> ���ͣ�MQTT �׽��ֱ�ʶ����ֵ�� 0��
		//<msgID> ���ͣ����ĵı��ı�ʶ�� ��Χ�� 0-65535�� ����<qos> = 0 ʱ����Ϊ 0��
		//<result> ���͡�0�����ͳɹ������յ� server �ظ���1�����ͳɹ��������յ��Ļظ�����2������ʧ�ܡ�
		//<value> ���͡������������ qos �ȼ���
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_PUB);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt publish:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 3, strbuf);
			ret = atoi(strbuf);
			if(ret == 0 || ret == 1)
			{
			#ifdef LTE_DEBUG
				LOGD("mqtt publish success!");
			#endif
			}
		}
	}
	
	if((ptr1 = strstr(data, LTE_SNS521S_RECE_MQTT_STAT)) != NULL)
	{
		//+ECMTSTAT: <tcpconnectID>,<err_code>
		//<tcpconnectID> ���ͣ�MQTT �׽��ֱ�ʶ����ֵ�� 0��
		//<err_code> ���͡�������롣1�������ѹرջ��ɶԵȷ����á�
		ptr1 += strlen(LTE_SNS521S_RECE_MQTT_STAT);
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			int ret;
			uint8_t strbuf[8] = {0};
			
			memset(tmpbuf, 0, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef LTE_DEBUG
			LOGD("mqtt stat:%s", tmpbuf);
		#endif			
			strcat(tmpbuf, ",");
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			ret = atoi(strbuf);
		#ifdef LTE_DEBUG
			LOGD("ret:%d", ret);
		#endif
			if(ret == 1)
			{
				mqtt_connected = LTE_MQTT_OPENED;
			}
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
			ParseServerDownloadData(ptr1, ptr2-ptr1);
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

void lte_set_cfg(void)
{
	//Disable lwm2m protocol
	LteSendData(LTE_SNS521S_SET_LWM2M_DISABLE, strlen(LTE_SNS521S_SET_LWM2M_DISABLE));
	//Set register mode
	LteSendData(LTE_SNS521S_SET_REGISTER_MODE, strlen(LTE_SNS521S_SET_REGISTER_MODE));
	//Set power save mode
	LteSendData(LTE_SNS521S_SET_PMU_CFG, strlen(LTE_SNS521S_SET_PMU_CFG));
}

void lte_get_infor(void)
{
#ifdef LTE_DEBUG
	LOGD("begin");
#endif

	lte_set_cfg();

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
		//δ����
		LOGD("MT not connection MT");
		break;
	case 2:
		//���ӱ���
		LOGD("MT link reserved MT");
		break;
	case 3:
		//���������
		LOGD("operation not allowed");
		break;
	case 4:
		//��֧�ֲ���
		LOGD("operation not supported");
		break;
	case 5:
		//��ҪPH-SIM PIN
		LOGD("PH-SIM PIN required");
		break;
	case 6:
		//��ҪPH-FSIM PIN
		LOGD("PH-FSIM PIN required");
		break;
	case 7:
		//��ҪPH-FSIM PUK
		LOGD("PH-FSIM PUK required");
		break;
	case 10:
		//δ����SIM ��
		LOGD("SIM not inserted");
		break;
	case 11:
		//��ҪSIM PIN
		LOGD("SIM PIN required");
		break;
	case 12:
		//��ҪSIM PUK
		LOGD("SIM PUK required");
		break;
	case 13:
		//SIM��ʧ��
		LOGD("SIM failure");
		break;
	case 14:
		//SIM ��æµ
		LOGD("SIM busy");
		break;
	case 15:
		//SIM ������
		LOGD("SIM wrong");
		break;
	case 16:
		//���벻��ȷ
		LOGD("incorrect password");
		break;
	case 17:
		//��Ҫ SIM PIN2
		LOGD("SIM PIN2 required");
		break;
	case 18:
		//��Ҫ SIM PUK2
		LOGD("SIM PUK2 required");
		break;
	case 20:
		//�ڴ�����
		LOGD("memory full");
		break;
	case 21:
		//��Ч����
		LOGD("invalid index");
		break;
	case 22:
		//δ����
		LOGD("not found");
		break;
	case 23:
		//�ڴ治��
		LOGD("memory failure");
		break;
	case 24:
		//�ı��ַ�������
		LOGD("text string too long");
		break;
	case 25:
		//��Ч�ı��ַ���
		LOGD("invalid characters in text string");
		break;
	case 26:
		//�����ַ�������
		LOGD("dial string too long");
		break;
	case 27:
		//��Ч�����ַ���
		LOGD("invalid characters in dial string");
		break;
	case 30:
		//���������
		LOGD("no network service");
		break;
	case 31:
		//���糬ʱ
		LOGD("network timeout");
		break;
	case 32:
		//���粻����-����������
		LOGD("network not allowed-emergency call only");
		break;
	case 40:
		//��Ҫ������Ի� PIN
		LOGD("network personalization PIN required");
		break;
	case 41:
		//��Ҫ������Ի� PUK
		LOGD("network personalization PUK required");
		break;
	case 42:
		//��Ҫ�����Ӽ����Ի� PIN 
		LOGD("network subset personalization PIN required");
		break;
	case 43:
		//��Ҫ�����Ӽ����Ի� PUK
		LOGD("network subset personalization PUK required");
		break;
	case 44:
		//��Ҫ�����ṩ�̸��Ի� PIN
		LOGD("service provider personalization PIN required");
		break;
	case 45:
		//��Ҫ�����ṩ�̸��Ի� PUK
		LOGD("service provider personalization PUK required");
		break;
	case 46:
		//��Ҫ��ҵ���Ի� PIN
		LOGD("corporate personalization PIN required");
		break;
	case 47:
		//��Ҫ��ҵ���Ի� PUK
		LOGD("corporate personalization PUK required");
		break;
	case 48:
		//��Ҫ������Կ
		LOGD("hidden key required");
		break;
	case 49:
		//EAP ������֧��
		LOGD("EAP method not support");
		break;
	case 50:
		//��Ч����
		LOGD("incorrect Parameters");
		break;
	case 51:
		//������ʵ�֣�����ǰ�ѽ���
		LOGD("command implemented but currently disabled");
		break;
	case 52:
		//����û���ֹ
		LOGD("command aborted by user");
		break;
	case 53:
		//���� MT �������ޣ�δע�ᵽ����
		LOGD("not attached to network due to MT functionalityrestrictions");
		break;
	case 54:
		//������- MT �����ڽ�������
		LOGD("modem not allowed-MT restricted to emergencycalls onlyModem");
		break;
	case 55:
		//���� MT �������ޣ�����������
		LOGD("operation not allowed because of MTfunctionality restrictions");
		break;
	case 56:
		//������̶�����-�Ѳ�����ǹ̻�
		LOGD("fixed dial number only allowed �C called number isnot a fixed");
		break;
	case 57:
		//�������� MT ʹ�ã���ʱͣ��
		LOGD("temporarily out of service due to other MT usage");
		break;
	case 58:
		//��֧�ֵ�����/��ĸ
		LOGD("language/alphabet not supported");
		break;
	case 59:
		//����ֵ������Χ
		LOGD("unexpected data value");
		break;
	case 60:
		//ϵͳ��������
		LOGD("system failure");
		break;
	case 61:
		//���ݶ�ʧ
		LOGD("data missing");
		break;
	case 62:
		//��ֹͨ��
		LOGD("call barred");
		break;
	case 63:
		//��Ϣ�ȴ�ָʾ����ʧ��
		LOGD("message waiting indication subscription failure");
		break;
	case 100:
		//δ֪
		LOGD("unknown");
		break;
	case 103:
		//�Ƿ� MS
		LOGD("illegal MS");
		break;
	case 106:
		//�Ƿ� ME
		LOGD("illegal ME");
		break;
	case 107:
		//������� GPRS ����
		LOGD("GPRS services not allowed");
		break;
	case 108:
		//������� GPRS ����ͷ� GPRS ����
		LOGD("GPRS services and non GPRS services not allowed");
		break;
	case 111:
		//������ PLMN
		LOGD("PLMN not allowed");
		break;
	case 112:
		//�������λ������
		LOGD("location area not allowed");
		break;
	case 113:
		//�ڴ�λ�������ڲ���������
		LOGD("roaming not allowed in this location area");
		break;
	case 114:
		//�� PLMN ������ʹ�� GPRS ����
		LOGD("GPRS services not allowed in this PLMN");
		break;
	case 115:
		//λ��������û�к��ʵ�С��
		LOGD("No suitable cells in location area");
		break;
	case 122:
		//ӵ��
		LOGD("Congestion");
		break;
	case 126:
		//��Դ����
		LOGD("Insufficient resources");
		break;
	case 127:
		//�����δ֪ APN
		LOGD("Mission or unknown APN");
		break;
	case 128:
		//δ֪ PDP ��ַ�� PDP ����
		LOGD("Unknown PDP address or PDP type");
		break;
	case 129:
		//�û���Ȩʧ��
		LOGD("User authentication failed");
		break;
	case 130:
		//GW �� GGSN ���� GW �� PDN GW �����ܾ�
		LOGD("Active reject by GGSN services GW or PDN");
		break;
	case 131:
		//δָ�������ܾ�
		LOGD("Active reject unspecified");
		break;
	case 132:
		//����ѡ�֧��
		LOGD("service option not supported");
		break;
	case 133:
		//δ�����������ѡ��
		LOGD("requested service option not subscribed");
		break;
	case 134:
		//����ѡ����ʱʧ��
		LOGD("service option temporarily out of order");
		break;
	case 140:
		//��֧�ָù���
		LOGD("Feature not supported");
		break;
	case 141:
		//TFT �����������
		LOGD("Semantic errors in the TFT operation");
		break;
	case 142:
		//TFT �����еľ䷨����
		LOGD("Syntactical errors in the TFT operation");
		break;
	case 143:
		//δ֪ PDP ������
		LOGD("Unknown PDP context");
		break;
	case 144:
		//���ݰ��������е��������
		LOGD("Semantic errors in packet filters");
		break;
	case 145:
		//���ݰ��������еľ䷨����
		LOGD("Syntactical errors in packet filters");
		break;
	case 146:
		//û�м��� TFT �� PDP ������
		LOGD("PDP context without TFT already activated");
		break;
	case 148:
		//δָ���� GPRS ����
		LOGD("unspecified GPRS error");
		break;
	case 149:
		//PDP ��֤ʧ��
		LOGD("PDP authentication failure");
		break;
	case 150:
		//��Ч�ƶ���
		LOGD("invalid mobile class");
		break;
	case 171:
		//���������һ�� PDN �Ͽ�����
		LOGD("Last PDN disconnection not allowed");
		break;
	case 172:
		//��Ϣ���岻��ȷ
		LOGD("Semantically incorrect message");
		break;
	case 173:
		//ǿ������ϢԪ�ش���
		LOGD("Mandatory information element error");
		break;
	case 174:
		//��ϢԪ�ز����ڻ�δʵ��
		LOGD("Information element not existent or notimplemented");
		break;
	case 175:
		//������ IE ����
		LOGD("Conditional IE error");
		break;
	case 176:
		//δָ����Э�����
		LOGD("Protocol error unspecified");
		break;
	case 177:
		//��Ӫ�̾���������
		LOGD("Operator determined barring");
		break;
	case 178:
		//�ﵽ PDP �����ĵ��������
		LOGD("Max number of PDP contexts reached");
		break;
	case 179:
		//��ǰ�� RAT �� PLMN ��ϲ�֧������� APN
		LOGD("Requested APN not supported in current RAT and PLMN combination");
		break;
	case 180:
		//����ܾ����ؿ���ģʽ��ͻ
		LOGD("Request rejected bearer control mode violation");
		break;
	case 181:
		//��֧�ֵ� OCI ֵ
		LOGD("Unsupported OCI value");
		break;
	case 182:
		//ͨ������ƽ����û����ݴ���ӵ��
		LOGD("User data transmission via control plane iscongested");
		break;
	case 301:
		//�ڲ��������
		LOGD("Internal error base");
		break;
	case 302:
		//UE ��æ
		LOGD("UE busy");
		break;
	case 303:
		//δ����
		LOGD("Not power on");
		break;
	case 304:
		//δ���� PDN
		LOGD("PDN not active");
		break;
	case 305:
		//���� PDN
		LOGD("PDN not valid");
		break;
	case 306:
		//���� PDN ����
		LOGD("PDN invalid type");
		break;
	case 307:
		//PDN �޲���
		LOGD("PDN no parameter");
		break;
	case 308:
		//UE ʧ��
		LOGD("UE fail");
		break;
	case 309:
		//�ظ��� PDN �� APN
		LOGD("PDN type and APN duplicate used");
		break;
	case 310:
		//PAP �� EITF ��ƥ��
		LOGD("PAP and EITF not matched");
		break;
	case 311:
		//SIM �� PIN ���ѽ���
		LOGD("SIM PIN disabled");
		break;
	case 312:
		//SIM �� PIN ��������
		LOGD("SIM PIN already enabled");
		break;
	case 313:
		//SIM PIN ��ʽ����
		LOGD("SIM PIN wrong format");
		break;
	case 512:
		//δ���ñ���Ĳ���
		LOGD("Required parameter not configured");
		break;
	case 514:
		//AT �ڲ�����
		LOGD("AT internal error");
		break;
	case 515:
		//CID �Ѽ���
		LOGD("CID is active");
		break;
	case 516:
		//����״̬����
		LOGD("Incorrect state for command");
		break;
	case 517:
		//CID ��Ч
		LOGD("CID is invalid");
		break;
	case 518:
		//CID δ����
		LOGD("CID is not active");
		break;
	case 520:
		//ͣ�����һ���Ѽ���� CID
		LOGD("Deactivate the last active CID");
		break;
	case 521:
		//CID δ����
		LOGD("CID is not defined");
		break;
	case 522:
		//��żУ�����
		LOGD("UART parity error UART");
		break;
	case 523:
		//UART ֡����
		LOGD("UART frame error");
		break;
	case 524:
		//UE ������С����ģʽ
		LOGD("UE is in minimal function mode");
		break;
	case 525:
		//AT ������ֹ��������
		LOGD("AT command aborted: in processing");
		break;
	case 526:
		//AT ������ֹ������
		LOGD("AT command aborted: error");
		break;
	case 527:
		//�����ж�
		LOGD("Command interrupted");
		break;
	case 528:
		//���ó�ͻ
		LOGD("Configuration conflicts");
		break;
	case 529:
		//�� FOTA ������
		LOGD("During FOTA updating");
		break;
	case 530:
		//�� AT ����� Socket
		LOGD("Not the AT allocated socket");
		break;
	case 531:
		//USIM PIN ����ֹ
		LOGD("USIM PIN is blocked");
		break;
	case 532:
		//USIM PUK ����ֹ
		LOGD("USIM PUK is blocked");
		break;
	case 533:
		//�� mipi ģ��
		LOGD("Not mipi module");
		break;
	case 534:
		//�ļ�δ�ҵ�
		LOGD("File not found");
		break;
	case 535:
		//ʹ������������
		LOGD("Conditions of use not satisfied");
		break;
	case 536:
		//AT UART ����������
		LOGD("AT UART buffer error");
		break;
	case 537:
		//�˱ܼ�ʱ����������
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

	//Turn of cmd reshow
	LteSendData(LTE_SNS521S_SET_CMD_RESHOW_ON, strlen(LTE_SNS521S_SET_CMD_RESHOW_ON));
	//Turn on register report
	LteSendData(LTE_SNS521S_SET_EREG_REPORT, strlen(LTE_SNS521S_SET_EREG_REPORT));
	//Turn on connect report
	LteSendData(LTE_SNS521S_SET_CON_REPORT, strlen(LTE_SNS521S_SET_CON_REPORT));
	//Turn on psm report
	LteSendData(LTE_SNS521S_SET_PSM_REPORT, strlen(LTE_SNS521S_SET_PSM_REPORT));
	//Turn on error report
	LteSendData(LTE_SNS521S_SET_ERR_REPORT, strlen(LTE_SNS521S_SET_ERR_REPORT));
	//Turn on RF
	LteSendData(LTE_SNS521S_SET_FUN_FULL, strlen(LTE_SNS521S_SET_FUN_FULL));
	//Attach request
	LteSendData(LTE_SNS521S_SET_ATTACH_NETWAOK, strlen(LTE_SNS521S_SET_ATTACH_NETWAOK));
	
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

	if(lte_mqtt_discon_flag)
	{
		if(LteSendCacheIsEmpty())
		{
			lte_mqtt_disconnect();
			lte_mqtt_close();
		}
		else
		{
			k_timer_start(&lte_mqtt_discon_timer, K_SECONDS(10), K_NO_WAIT);
		}

		lte_mqtt_discon_flag = false;
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

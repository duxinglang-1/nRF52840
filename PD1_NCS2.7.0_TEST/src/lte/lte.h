/****************************************Copyright (c)************************************************
** File Name:			    lte.h
** Descriptions:			lte function main head file
** Created By:				xie biao
** Created Date:			2024-09-29
** Modified Date:      		2024-09-29
** Version:			    	V1.0
******************************************************************************************************/
#ifndef __LTE_H__
#define __LTE_H__

#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>

#define LTE_SNS521S_SET_CMD_RESHOW_ON	"ATE1\r\n"
#define LTE_SNS521S_SET_CMD_RESHOW_OFF	"ATE0\r\n"
#define LTE_SNS521S_SET_FUN_OFF			"AT+CFUN=0\r\n"
#define LTE_SNS521S_SET_FUN_FULL		"AT+CFUN=1\r\n"
#define LTE_SNS521S_SET_FUN_RF_OFF		"AT+CFUN=4\r\n"
#define LTE_SNS521S_SET_ERR_REPORT		"AT+CMEE=2\r\n"
#define LTE_SNS521S_SET_REBOOT			"AT+NRB\r\n"
#define LTE_SNS521S_SET_ATTACH_NETWAOK	"AT+CGATT=1\r\n"
#define LTE_SNS521S_SET_CLEAR_SEARCH	"AT+NCSEARFCN\r\n"
#define LTE_SNS521S_SET_REG_REPORT		"AT+CREG=3\r\n"
#define LTE_SNS521S_SET_EREG_REPORT		"AT+CEREG=5\r\n"
#define LTE_SNS521S_SET_CON_REPORT		"AT+CSCON=1\r\n"
#define LTE_SNS521S_SET_PSM				"AT+CPSMS=1,,,"##CONFIG_LTE_PSM_REQ_RPTAU##","##CONFIG_LTE_PSM_REQ_RAT##"\r\n"
#define LTE_SNS521S_SET_PSM_REPORT		"AT+NPSMR=1\r\n"
#define LTE_SNS521S_SET_AUTO_CONNECT	"AT+NCONFIG=AUTOCONNECT,TRUE\r\n"
#define LTE_SNS521S_SET_TZ_REPORT		"AT+CTZR=2\r\n"
#define LTE_SNS521S_SET_IP_REPORT		"AT+NIPINFO=1\r\n"
#define LTE_SNS521S_SET_DNS				"AT+MDNS=1,"
#define LTE_SNS521S_SET_SOCKET_CREAT	"AT+NSOCR="
#define LTE_SNS521S_SET_SOCKET_CONNECT	"AT+NSOCO="
#define LTE_SNS521S_SET_SOCKET_SEND		"AT+NSOSD="
#define LTE_SNS521S_SET_SOCKET_CLOSE	"AT+NSOCL="
#define	LTE_SNS521S_SET_SOCKET_NOTIFY	"AT+NSONMI=2\r\n"
#define LTE_SNS521S_SET_RECEIVED_NOTIFY	"AT+NNMI=1\r\n"
#define LTE_SNS521S_SET_SENT_NOTIFY		"AT+NSMI=1\r\n"

#define LTE_SNS521S_SET_LWM2M_DISABLE	"AT+MLWM2MENABLE=0\r\n"
#define LTE_SNS521S_SET_REGISTER_MODE	"AT+MREGSWT=0\r\n"
#define LTE_SNS521S_SET_PMU_CFG			"AT+ECPMUCFG=1,2\r\n"

#define LTE_SNS521S_SET_MQTT_CILENT		"AT+ECMTCFG"
#define LTE_SNS521S_SET_MQTT_CLIENT_KEEPALIVE	"keepalive"
#define LTE_SNS521S_SET_MQTT_CLIENT_SESSION		"session"
#define LTE_SNS521S_SET_MQTT_CLIENT_TIMEOUT		"timeout"
#define LTE_SNS521S_SET_MQTT_CLIENT_WILL		"will"
#define LTE_SNS521S_SET_MQTT_CLIENT_VERSION		"version"
#define LTE_SNS521S_SET_MQTT_CLIENT_ALIAUTH		"aliauth"
#define LTE_SNS521S_SET_MQTT_CLIENT_CLOUD		"cloud"

#define	LTE_SNS521S_SET_MQTT_OPEN		"AT+ECMTOPEN"
#define LTE_SNS521S_SET_MQTT_CLOSE		"AT+ECMTCLOSE"
#define LTE_SNS521S_SET_MQTT_CONNECT	"AT+ECMTCONN"
#define LTE_SNS521S_SET_MQTT_DISCONNECT	"AT+ECMTDISC"
#define LTE_SNS521S_SET_MQTT_SUB		"AT+ECMTSUB"
#define LTE_SNS521S_SET_MQTT_UNSUB		"AT+ECMTUNS"
#define LTE_SNS521S_SET_MQTT_PUBLISH	"AT+ECMTPUB"

#define LTE_SNS521S_GET_MANU_INFOR		"AT+CGMI\r\n"
#define LTE_SNS521S_GET_MANU_MODEL		"AT+CGMM\r\n"
#define LTE_SNS521S_GET_HW_VERSION		"AT+WHVER\r\n"
#define LTE_SNS521S_GET_SW_VERSION		"AT+WGMR\r\n"
#define LTE_SNS521S_GET_IMEI			"AT+CGSN\r\n"
#define LTE_SNS521S_GET_IMSI			"AT+CIMI\r\n"
#define LTE_SNS521S_GET_ICCID			"AT+NCCID\r\n"
#define LTE_SNS521S_GET_IP				"AT+CGPADDR\r\n"
#define LTE_SNS521S_GET_CSQ				"AT+CSQ\r\n"
#define LTE_SNS521S_GET_CESQ			"AT+CESQ\r\n"
#define LTE_SNS521S_GET_CCLK			"AT+CCLK?\r\n"
#define LTE_SNS521S_GET_UE_INFOR		"AT+NUESTATS\r\n"
#define LTE_SNS521S_GET_RECEIVE_DATA	"AT+NMGR\r\n"

#define LTE_SNS521S_RECE_BOOT			"REBOOT_CAUSE_APPLICATION_SWRESET"
#define LTE_SNS521S_RECE_OK				"OK"
#define LTE_SNS521S_RECE_ERROR			"ERROR"

#define LTE_SNS521S_RECE_MANU_INFOR		"+CGMI:"
#define LTE_SNS521S_RECE_MANU_MODE		"+CGMM:"
#define LTE_SNS521S_RECE_HW_VER			"+WHVER:"
#define LTE_SNS521S_RECE_SW_VER			"+WGMR:"
#define LTE_SNS521S_RECE_ICCID			"+NCCID:"
#define LTE_SNS521S_RECE_CSQ			"+CSQ:"
#define LTE_SNS521S_RECE_CESQ			"+CESQ:"
#define LTE_SNS521S_RECE_CEREG			"+CEREG:"
#define LTE_SNS521S_RECE_CCLK			"+CCLK:"
#define LTE_SNS521S_RECE_IP				"+CGPADDR:"
#define LTE_SNS521S_RECE_CSCON			"+CSCON:"

#define LTE_SNS521S_RECE_MQTT_CFG		"+ECMTCFG:"
#define LTE_SNS521S_RECE_MQTT_OPEN		"+ECMTOPEN:"
#define LTE_SNS521S_RECE_MQTT_CLOSE		"+ECMTCLOSE:"
#define LTE_SNS521S_RECE_MQTT_CON		"+ECMTCONN:"
#define LTE_SNS521S_RECE_MQTT_DISCON	"+ECMTDISC:"
#define LTE_SNS521S_RECE_MQTT_SUB		"+ECMTSUB:"
#define LTE_SNS521S_RECE_MQTT_UNSUB		"+ECMTUNS:"
#define LTE_SNS521S_RECE_MQTT_PUB		"+ECMTPUB:"
#define LTE_SNS521S_RECE_MQTT_STAT		"+ECMTSTAT:"
#define LTE_SNS521S_RECE_MQTT_DATA		"+ECMTRECV:"

#define IMEI_MAX_LEN	(15)
#define IMSI_MAX_LEN	(15)
#define ICCID_MAX_LEN	(20)
#define MODEM_MAX_LEN   (17)

#define APN_MAX_LEN		(100)
#define	PLMN_MAX_LEN    (6)

#define LTE_NET_RELINK_MAX	5
#define MQTT_RELINK_MAX		5

typedef enum
{
	REBOOT_CAUSE_APPLICATION_UNKNOWN,
	REBOOT_CAUSE_APPLICATION_WATCHDOG,
	REBOOT_CAUSE_APPLICATION_ASSERT,
	REBOOT_CAUSE_APPLICATION_HARDFAULT,
	REBOOT_CAUSE_APPLICATION_AT,
	REBOOT_CAUSE_APPLICATION_POWER_ON_RESET,
	REBOOT_CAUSE_APPLICATION_SWRESET
}LTE_UE_REBOOT_CAUSE;

typedef enum
{
	LTE_MQTT_CLOSED,
	LTE_MQTT_OPENED,
	LTE_MQTT_CONNECTED,
	LTE_MQTT_MAX
}LTE_MQTT_STATUS;

extern uint8_t g_imsi[IMSI_MAX_LEN+1];
extern uint8_t g_imei[IMEI_MAX_LEN+1];
extern uint8_t g_iccid[ICCID_MAX_LEN+1];
extern uint8_t g_modem[MODEM_MAX_LEN+1];

extern uint8_t g_new_fw_ver[64];
extern uint8_t g_new_modem_ver[64];
extern uint8_t g_new_ble_ver[64];
extern uint8_t g_new_wifi_ver[64];
extern uint8_t g_new_ui_ver[16];
extern uint8_t g_new_font_ver[16];
extern uint8_t g_new_ppg_ver[16];
extern uint8_t g_timezone[5];
extern uint8_t g_prj_dir[128];
extern uint8_t g_rsrp;

extern void LTE_init(void);
extern void LteMsgProcess(void);

#endif/*__LTE_H__*/
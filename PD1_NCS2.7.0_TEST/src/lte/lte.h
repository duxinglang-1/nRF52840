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

#define IMEI_MAX_LEN	(15)
#define IMSI_MAX_LEN	(15)
#define ICCID_MAX_LEN	(20)
#define MODEM_MAX_LEN   (17)

#define APN_MAX_LEN		(100)
#define	PLMN_MAX_LEN    (6)


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
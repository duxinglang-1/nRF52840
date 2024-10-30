/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "gps.h"
#include "uart.h"
#include "logger.h"

//#define GPS_DEBUG

#define GPS_EN_PIN		9
#define GPS_RST_PIN		10

#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio0), okay)
#define GPS_PORT DT_NODELABEL(gpio0)
#else
#error "gpio0 devicetree node is disabled"
#define GPS_PORT	""
#endif

static void APP_Ask_GPS_Data_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(app_wait_gps_timer, APP_Ask_GPS_Data_timerout, NULL);
static void APP_Send_GPS_Data_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(app_send_gps_timer, APP_Send_GPS_Data_timerout, NULL);
static void GPS_get_infor_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(gps_get_infor_timer, GPS_get_infor_timerout, NULL);
static void GPS_turn_off_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(gps_turn_off_timer, GPS_turn_off_timerout, NULL);


static bool gps_is_on = false;

static int64_t gps_start_time=0,gps_fix_time=0,gps_local_time=0;

bool gps_on_flag = false;
bool gps_off_flag = false;
bool ble_wait_gps = false;
bool sos_wait_gps = false;
#ifdef CONFIG_FALL_DETECT_SUPPORT
bool fall_wait_gps = false;
#endif
bool location_wait_gps = false;
bool test_gps_flag = false;
bool gps_test_update_flag = false;
bool gps_send_data_flag = false;

uint8_t gps_test_info[256] = {0};

static bool gps_get_infor_flag = false;

static uint8_t test_gps_data[] = 
{
	"$GNGSA,A,3,10,12,18,23,24,25,28,32,195,199,,,1.3,0.8,1.0,1*36\r\n"
	"$GNGSA,A,3,23,32,37,38,39,40,41,,,,,,1.3,0.8,1.0,4*3B\r\n"
	"$GPGSV,3,1,12,10,68,335,25,12,33,068,22,18,13,181,22,23,65,133,27,0*61\r\n"
	"$GPGSV,3,2,12,24,12,045,12,25,52,120,31,28,44,252,23,31,14,235,,0*6C\r\n"
	"$GPGSV,3,3,12,32,40,327,25,194,,,24,195,22,166,19,199,60,149,20,0*65\r\n"
	"$BDGSV,4,1,15,06,66,289,,07,13,179,18,08,82,143,14,09,60,267,,0*7E\r\n"
	"$BDGSV,4,2,15,10,09,191,14,13,83,309,,16,69,313,,23,51,046,18,0*76\r\n"
	"$BDGSV,4,3,15,30,07,236,,32,61,025,15,37,70,195,22,38,69,154,30,0*78\r\n"
	"$BDGSV,4,4,15,39,66,330,12,40,06,169,21,41,31,103,23,0*42\r\n"
	"$GNRMC,040021.000,A,2240.06401,N,11401.52998,E,0.00,224.50,291024,,,A,V*0D\r\n"
	"$GNVTG,224.50,T,,M,0.00,N,0.00,K,A*22\r\n"
	"$GNZDA,040021.000,29,10,2024,00,00*41\r\n"
	"$GPTXT,01,01,01,ANTENNA OPEN*25\r\n"
	"$GPTXT,01,01,02,SW=URANUS5,V5.3.0.0*1D\r\n"
	"$GPTXT,01,01,02,IC=AT6558R-5N-32-1C580901*13\r\n"
	"$GNGGA,040022.000,2240.06398,N,11401.53003,E,1,17,0.8,95.6,M,-3.4,M,,*68\r\n"
	"$GNGLL,2240.06398,N,11401.53003,E,040022.000,A,A*43\r\n"
};
								 
static const char update_indicator[] = {'\\', '|', '/', '-'};

static struct device *gpio_gps = NULL;

static gnss_pvt_data_frame_t last_pvt = {0};

/*============================================================================
* Function Name  : Send_Cmd_To_AT6558
* Description    : 向AT6558送命令
* Input          : cmd:发送的命令字符串
* Output         : None
* Return         : None
* CALL           : 可被外部调用
==============================================================================*/
void Send_Cmd_To_AT6558(uint8_t *cmd, uint32_t len)
{
	uint8_t i,cs = 0;
	uint8_t cmdbuf[256] = {0};
	uint8_t csbuf[3] = {0};
	
	//校验和，$和*之间（不包括$和*）所有字符的异或结果
	cs = cmd[1];
	for(i=2;i<len;i++)
		cs = cs^cmd[i];
	sprintf(csbuf, "%02X", cs);
	
	strcpy(cmdbuf, cmd);
	strcat(cmdbuf, "*");
	strcat(cmdbuf, csbuf);
	strcat(cmdbuf, "\r\n");
	
	UartSwitchToGps();
	GpsSendData(cmdbuf, strlen(cmdbuf));
}

void GetStringInforBySepa(uint8_t *sour_buf, uint8_t *key_buf, uint8_t pos_index, uint8_t *outbuf)
{
	uint8_t *ptr1,*ptr2;
	uint8_t count=0;
	
	if(sour_buf == NULL || key_buf == NULL || outbuf == NULL || pos_index < 1)
		return;

	ptr1 = sour_buf;
	while(1)
	{
		ptr2 = strstr(ptr1, key_buf);
		if(ptr2)
		{
			count++;
			if(count == pos_index)
			{
				memcpy(outbuf, ptr1, (ptr2-ptr1));
				outbuf[ptr2-ptr1] = 0x00;
				return;
			}
			else
			{
				ptr1 = ptr2+1;
			}
		}
		else
		{
			return;
		}
	}
}

void gnss_data_analysis(uint8_t *data, uint32_t data_len)
{
	uint8_t *ptr1,*ptr2;
	uint8_t tmpbuf[256] = {0};
	uint32_t len;
	
	if((ptr1 = strstr(data, GNSS_GNTXT_FIELD)) != NULL)
	{
		//$GPTXT,01,01,02,MA=CASGN*24
		//$GPTXT,01,01,02,HW=AT6668,0012345612345*6E
		//$GPTXT,01,01,02,IC=AT6668-6X-32-00000917,CDS2A2J-P1-000236*39

		while(1)
		{
			ptr2 = strstr(ptr1, "\r\n");
			if(ptr2 != NULL)
			{
				uint8_t *ptr3,*ptr4;
				
				memset(tmpbuf, 0x00, sizeof(tmpbuf));
				memcpy(tmpbuf, ptr1, ptr2-ptr1);
			#ifdef GPS_DEBUG
				//LOGD("%s", tmpbuf);
			#endif

				//MA
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_MA);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_MA);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_MA, strbuf);
					#endif	
					}
				}

				//HW
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_HW);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_HW);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_HW, strbuf);
					#endif	
					}
				}

				//IC
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_IC);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_IC);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_IC, strbuf);
					#endif	
					}
				}

				//SW
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_SW);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_SW);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_SW, strbuf);
					#endif	
					}
				}

				//MO
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_MO);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_MO);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_MO, strbuf);
					#endif	
					}
				}

				//SM
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_SM);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_SM);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_SM, strbuf);
					#endif	
					}
				}

				//ANTENNA
				ptr3 = strstr(tmpbuf, GNSS_GNTXT_ANT);
				if(ptr3 != NULL)
				{
					ptr3 += strlen(GNSS_GNTXT_ANT);
					ptr4 = strstr(ptr3, GNSS_FIELD_CHECKSUM);
					if(ptr4 != NULL)
					{
						uint8_t strbuf[128] = {0};
						
						memcpy(strbuf, ptr3, ptr4-ptr3);
					#ifdef GPS_DEBUG
						LOGD("%s%s", GNSS_GNTXT_ANT, strbuf);
					#endif	
					}
				}

				ptr2 += strlen("\r\n");
				ptr1 = strstr(ptr2, GNSS_GNTXT_FIELD);
				if(ptr1 == NULL)
					break;
			}
			else
			{
				break;
			}
		}
	}

	if((ptr1 = strstr(data, GNSS_GPGSV_FIELD)) != NULL)
	{
		//$GPGSV,3,1,12,03,24,261,21,04,32,321,24,08,20,198,17,16,66,314,22,0*69\r\n
		//$GPGSV,3,2,12,26,49,020,23,27,47,175,28,28,30,079,08,31,48,043,22,0*64\r\n
		//$GPGSV,3,3,12,32,16,147,24,194,64,054,20,195,22,137,23,199,60,149,26,0*5F\r\n
		uint8_t i=0,count;
		uint8_t type,strgsv[10] = {0};

		memset(tmpbuf, 0x00, sizeof(tmpbuf));
		GetStringInforBySepa(ptr1, ",", 4, tmpbuf);
		count = atoi(tmpbuf);

		while(1)
		{
			ptr2 = strstr(ptr1, "\r\n");
			if(ptr2 != NULL)
			{
				uint8_t *ptr3,*ptr4;
				uint8_t strbuf[128] = {0};
				
				memset(tmpbuf, 0x00, sizeof(tmpbuf));
				memcpy(tmpbuf, ptr1, ptr2-ptr1);
			#ifdef GPS_DEBUG
				//LOGD("%s", tmpbuf);
			#endif
			
				last_pvt.sv[i].signal = GNSS_SIGNAL_GPS;
				GetStringInforBySepa(tmpbuf, ",", 5, strbuf);
				last_pvt.sv[i].sv = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 6, strbuf);
				last_pvt.sv[i].elevation = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 7, strbuf);
				last_pvt.sv[i].azimuth = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 8, strbuf);
				last_pvt.sv[i].cn0 = atoi(strbuf);
				
				i++;
				if((i >= count) || (i >= GNSS_MAX_SATELLITES))
					break;
	
				ptr2 += strlen("\r\n");
				ptr1 = strstr(ptr2, GNSS_GPGSV_FIELD);
				if(ptr1 == NULL)
					break;
			}
			else
			{
				break;
			}
		}
	}
	else if((ptr1 = strstr(data, GNSS_BDGSV_FIELD)) != NULL)
	{
		//$BDGSV,4,1,14,07,60,193,27,08,11,200,19,10,49,203,26,12,28,086,16,0*70
		//$BDGSV,4,2,14,14,27,178,18,25,50,344,24,33,45,202,27,34,36,119,27,0*7D
		//$BDGSV,4,3,14,38,09,188,18,40,63,168,29,41,46,282,24,42,06,163,23,0*75
		//$BDGSV,4,4,14,43,21,170,23,44,18,062,23,0*7E
		uint8_t i=0,count;
		uint8_t type,strgsv[10] = {0};

		memset(tmpbuf, 0x00, sizeof(tmpbuf));
		GetStringInforBySepa(ptr1, ",", 4, tmpbuf);
		count = atoi(tmpbuf);

		while(1)
		{
			ptr2 = strstr(ptr1, "\r\n");
			if(ptr2 != NULL)
			{
				uint8_t *ptr3,*ptr4;
				uint8_t strbuf[128] = {0};
				
				memset(tmpbuf, 0x00, sizeof(tmpbuf));
				memcpy(tmpbuf, ptr1, ptr2-ptr1);
			#ifdef GPS_DEBUG
				//LOGD("%s", tmpbuf);
			#endif

				last_pvt.sv[i].signal = GNSS_SIGNAL_BDS;
				GetStringInforBySepa(tmpbuf, ",", 5, strbuf);
				last_pvt.sv[i].sv = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 6, strbuf);
				last_pvt.sv[i].elevation = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 7, strbuf);
				last_pvt.sv[i].azimuth = atoi(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 8, strbuf);
				last_pvt.sv[i].cn0 = atoi(strbuf);
				
				i++;
				if((i >= count) || (i >= GNSS_MAX_SATELLITES))
					break;
	
				ptr2 += strlen("\r\n");
				ptr1 = strstr(ptr2, GNSS_BDGSV_FIELD);
				if(ptr1 == NULL)
					break;
			}
			else
			{
				break;
			}
		}
	}
	
	if((ptr1 = strstr(data, GNSS_GNGSA_FIELD)) != NULL)
	{
		//$GNGSA,A,3,03,04,08,16,26,27,31,32,194,195,199,,1.1,0.6,0.9,1*04
		//$GNGSA,A,3,07,08,10,12,14,25,33,34,38,40,41,42,1.1,0.6,0.9,4*3F
		while(1)
		{
			ptr2 = strstr(ptr1, "\r\n");
			if(ptr2 != NULL)
			{
				uint8_t *ptr3,*ptr4;
				uint8_t strbuf[128] = {0};
				
				memset(tmpbuf, 0x00, sizeof(tmpbuf));
				memcpy(tmpbuf, ptr1, ptr2-ptr1);
			#ifdef GPS_DEBUG
				//LOGD("%s", tmpbuf);
			#endif

				GetStringInforBySepa(tmpbuf, ",", 16, strbuf);
				last_pvt.pdop = atof(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 17, strbuf);
				last_pvt.hdop = atof(strbuf);
				GetStringInforBySepa(tmpbuf, ",", 18, strbuf);
				last_pvt.vdop = atof(strbuf);

				ptr2 += strlen("\r\n");
				ptr1 = strstr(ptr2, GNSS_GNGSA_FIELD);
				if(ptr1 == NULL)
					break;
			}
			else
			{
				break;
			}
		}
	}

	if((ptr1 = strstr(data, GNSS_GNGGA_FIELD)) != NULL)
	{
		//$GNGGA,085842.000,2240.05632,N,11401.51793,E,1,25,0.6,95.2,M,-3.4,M,,*6E
		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			uint8_t strbuf[128] = {0};
			
			memset(tmpbuf, 0x00, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef GPS_DEBUG
			//LOGD("%s", tmpbuf);
		#endif
		
			GetStringInforBySepa(tmpbuf, ",", 10, strbuf);
			last_pvt.altitude = atof(strbuf);
		}
	}
	
	if((ptr1 = strstr(data, GNSS_GNRMC_FIELD)) != NULL)
	{
		//$GNRMC,085842.000,A,2240.05632,N,11401.51793,E,0.00,295.02,251024,,,A,V*0F
		uint32_t time,date;
		double latitude = 0.0, longitude = 0.0;

		ptr2 = strstr(ptr1, "\r\n");
		if(ptr2 != NULL)
		{
			uint8_t strbuf[128] = {0};
			
			memset(tmpbuf, 0x00, sizeof(tmpbuf));
			memcpy(tmpbuf, ptr1, ptr2-ptr1);
		#ifdef GPS_DEBUG
			LOGD("%s", tmpbuf);
		#endif

			//time
			GetStringInforBySepa(tmpbuf, ",", 2, strbuf);
			time = atoi(strbuf);
			last_pvt.datetime.hour = time/10000;
			last_pvt.datetime.minute = (time%10000)/100;
			last_pvt.datetime.seconds = time%100;

			//latitude&longitude
			GetStringInforBySepa(tmpbuf, ",", 3, strbuf);
			if(strbuf[0] == 'A')
			{
				double tmp_f = 0.0;
				uint32_t tmp_i = 0;
				
				last_pvt.flags = GNSS_PVT_FLAG_FIX_VALID;

				memset(strbuf, 0x00, sizeof(strbuf));
				GetStringInforBySepa(tmpbuf, ",", 4, strbuf);
				tmp_f = atof(strbuf);
				tmp_i = tmp_f*100000;
				latitude = tmp_i/10000000.0 +  (tmp_i%10000000)/100000/60.0;

				memset(strbuf, 0x00, sizeof(strbuf));
				GetStringInforBySepa(tmpbuf, ",", 5, strbuf);
				if(strbuf[0] == 'S')
					latitude = -latitude;

				memset(strbuf, 0x00, sizeof(strbuf));
				GetStringInforBySepa(tmpbuf, ",", 6, strbuf);
				tmp_f = atof(strbuf);
				tmp_i = tmp_f*100000;
				longitude = tmp_i/10000000.0 +  (tmp_i%10000000)/100000/60.0;

				memset(strbuf, 0x00, sizeof(strbuf));
				GetStringInforBySepa(tmpbuf, ",", 7, strbuf);
				if(strbuf[0] == 'W')
					longitude = -longitude;
			}
			else
			{
				last_pvt.flags = GNSS_PVT_FLAG_FIX_UNVALID;
			}
			last_pvt.latitude = latitude;
			last_pvt.longitude = longitude;

			//speed
			memset(strbuf, 0x00, sizeof(strbuf));
			GetStringInforBySepa(tmpbuf, ",", 8, strbuf);
			last_pvt.speed = atof(strbuf);

			//heading
			memset(strbuf, 0x00, sizeof(strbuf));
			GetStringInforBySepa(tmpbuf, ",", 9, strbuf);
			last_pvt.heading = atof(strbuf);

			//date
			memset(strbuf, 0x00, sizeof(strbuf));
			GetStringInforBySepa(tmpbuf, ",", 10, strbuf);
			date = atoi(strbuf);
			last_pvt.datetime.day = date/10000;
			last_pvt.datetime.month = (date%10000)/100;
			last_pvt.datetime.year = 2000 + date%100;
		}
	}
}

static void print_fix_data(gnss_pvt_data_frame_t *pvt_data)
{
    uint8_t tmpbuf[256] = {0};

#ifdef GPS_DEBUG
    int32_t lon, lat;

    lon = pvt_data->longitude*1000000;
    lat = pvt_data->latitude*1000000;
    sprintf(tmpbuf, "Longitude:%d.%06d, Latitude:%d.%06d", lon/1000000, lon%1000000, lat/1000000, lat%1000000);
    LOGD("%s",tmpbuf);
	LOGD("Date:           %04u-%02u-%02u\n",
	       pvt_data->datetime.year,
	       pvt_data->datetime.month,
	       pvt_data->datetime.day);
	LOGD("Time (UTC):     %02u:%02u:%02u.%03u\n",
	       pvt_data->datetime.hour,
	       pvt_data->datetime.minute,
	       pvt_data->datetime.seconds,
	       pvt_data->datetime.ms);
#endif
}

void gps_receive_data_handle(uint8_t *data, uint32_t data_len)
{
#ifdef GPS_DEBUG
	LOGD("len:%d, data:%s", data_len, data);
#endif

	gnss_data_analysis(data, data_len);

	if(test_gps_flag)
	{
		uint8_t i,tracked = 0,flag = 0;
		uint8_t strbuf[256] = {0};
		int32_t lon,lat;
		
		memset(gps_test_info, 0x00, sizeof(gps_test_info));
		
		for(i=0;i<GNSS_MAX_SATELLITES;i++)
		{
			uint8_t buf[256] = {0};
			
			if(last_pvt.sv[i].sv > 0)
			{
				tracked++;
			#if defined(LCD_VGM068A4W01_SH1106G)||defined(LCD_VGM096064A6W01_SP5090)
				if(tracked<8)
				{
					if(flag == 0)
					{
						flag = 1;
						sprintf(buf, "%02d", last_pvt.sv[i].cn0);
					}
					else
					{
						sprintf(buf, "|%02d", last_pvt.sv[i].cn0);
					}
					
					strcat(strbuf, buf);
				}
			#else
			  #ifdef CONFIG_FACTORY_TEST_SUPPORT
				if(IsFTGPSTesting())
				{
					if(flag == 0)
					{
						flag = 1;
						sprintf(buf, "%02d", last_pvt.sv[i].cn0);
					}
					else
					{
						sprintf(buf, "|%02d", last_pvt.sv[i].cn0);
					}
				}
				else
			  #endif
				{
					if(flag == 0)
					{
						flag = 1;
						sprintf(buf, "%02d|%02d", last_pvt.sv[i].sv, last_pvt.sv[i].cn0);
					}
					else
					{
						sprintf(buf, ";%02d|%02d", last_pvt.sv[i].sv, last_pvt.sv[i].cn0);
					}
			  	}
				strcat(strbuf, buf);
			#endif
			}
		}

	#if defined(LCD_VGM068A4W01_SH1106G)||defined(LCD_VGM096064A6W01_SP5090)
		sprintf(gps_test_info, "%02d,", tracked);
	#else
		sprintf(gps_test_info, "%02d\n", tracked);
	#endif
		strcat(gps_test_info, strbuf);

	#ifdef CONFIG_FACTORY_TEST_SUPPORT
		FTGPSStatusUpdate(false);
	#endif

		gps_test_update_flag = true;
	}
	
	if(last_pvt.flags == GNSS_PVT_FLAG_FIX_VALID)
	{
	#ifdef GPS_DEBUG
		LOGD("NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID");
	#endif

		if(gps_fix_time == 0)
		{
			gps_fix_time = k_uptime_get();
			gps_local_time = gps_fix_time-gps_start_time;
		}

		if(!test_gps_flag)
		{
		#ifdef GPS_DEBUG
			LOGD("Position fix with NMEA data, fix time:%d", gps_local_time);
		#endif
			APP_Ask_GPS_off();

			if(k_timer_remaining_get(&app_wait_gps_timer) > 0)
				k_timer_stop(&app_wait_gps_timer);

			k_timer_start(&app_send_gps_timer, K_MSEC(1000), K_NO_WAIT);
		}
		else
		{
			uint8_t strbuf[256] = {0};
			int32_t lon,lat;
			
		#if defined(LCD_VGM068A4W01_SH1106G)||defined(LCD_VGM096064A6W01_SP5090)
			strcat(gps_test_info, "\n");	//2行没显示满，多换行一行
			if(gps_fix_time > 0)
			{
				sprintf(strbuf, "fix:%dS", gps_local_time/1000);
				strcat(gps_test_info, strbuf);
			}
		#else
			strcat(gps_test_info, "\n");
			
			lon = last_pvt.longitude*1000000;
			lat = last_pvt.latitude*1000000;
			sprintf(strbuf, "Lon:   %d.%06d\nLat:    %d.%06d\n", lon/1000000, abs(lon)%1000000, lat/1000000, abs(lat)%1000000);
			strcat(gps_test_info, strbuf);

		#ifdef CONFIG_FACTORY_TEST_SUPPORT
			if(IsFTGPSTesting())
			{
				//xb add 2023-03-15 ft gps test don't need show fix time
			}
			else
		#endif
			{
				if(gps_fix_time > 0)
				{
					sprintf(strbuf, "fix time:    %dS", gps_local_time/1000);
					strcat(gps_test_info, strbuf);
				}
			}
		#endif

		#ifdef CONFIG_FACTORY_TEST_SUPPORT
			FTGPSStatusUpdate(true);
		#endif

			gps_test_update_flag = true;
		}
		
	#ifdef GPS_DEBUG
		print_fix_data(&last_pvt);
	#endif
	}
	else
	{
	#ifdef GPS_DEBUG
		LOGD("Seconds since last fix: %d", (k_uptime_get() - gps_start_time) / 1000);
	#endif
	}
}

bool APP_GPS_data_send(bool fix_flag)
{
	bool ret = false;

#ifdef CONFIG_BLE_SUPPORT	
	if(ble_wait_gps)
	{
		APP_get_gps_data_reply(fix_flag, last_pvt);
		ble_wait_gps = false;
		ret = true;
	}
#endif

	if(sos_wait_gps)
	{
		sos_get_gps_data_reply(fix_flag, last_pvt);
		sos_wait_gps = false;
		ret = true;
	}

#if defined(CONFIG_IMU_SUPPORT)&&defined(CONFIG_FALL_DETECT_SUPPORT)
	if(fall_wait_gps)
	{
		fall_get_gps_data_reply(fix_flag, last_pvt);
		fall_wait_gps = false;
		ret = true;
	}
#endif

	if(location_wait_gps)
	{
		location_get_gps_data_reply(fix_flag, last_pvt);
		location_wait_gps = false;
		ret = true;
	}

	return ret;
}

void APP_Ask_GPS_Data_timerout(struct k_timer *timer)
{
	if(!test_gps_flag)
		gps_off_flag = true;

	gps_send_data_flag = true;
}

void APP_Ask_GPS_Data(void)
{
	static uint8_t time_init = false;

#if 1
	if(!gps_is_on)
	{
		gps_on_flag = true;
		k_timer_start(&app_wait_gps_timer, K_SECONDS(APP_WAIT_GPS_TIMEOUT), K_NO_WAIT);
	}
#else
	last_pvt.datetime.year = 2020;
	last_pvt.datetime.month = 11;
	last_pvt.datetime.day = 4;
	last_pvt.datetime.hour = 2;
	last_pvt.datetime.minute = 20;
	last_pvt.datetime.seconds = 40;
	last_pvt.longitude = 114.025254;
	last_pvt.latitude = 22.667808;
	gps_local_time = 1000;
	
	APP_Ask_GPS_Data_timerout(NULL);
#endif
}

void APP_Send_GPS_Data_timerout(struct k_timer *timer)
{
	gps_send_data_flag = true;
}

static void GPS_get_infor_timerout(struct k_timer *timer_id)
{
	gps_get_infor_flag = true;
}

static void GPS_turn_off_timerout(struct k_timer *timer_id)
{
	gps_off_flag = true;
}

void APP_Ask_GPS_off(void)
{
	if(!test_gps_flag)
		gps_off_flag = true;
}

bool gps_is_working(void)
{
#ifdef GPS_DEBUG
	LOGD("gps_is_on:%d", gps_is_on);
#endif
	return gps_is_on;
}

/*============================================================================
* Function Name  : gps_enable
* Description    : AT6558R_EN使能，高电平有效
* Input          : None
* Output         : None
* Return         : None
* CALL           : 可被外部调用
==============================================================================*/
void gps_enable(void)
{
	gpio_pin_set(gpio_gps, GPS_RST_PIN, 0);
	k_sleep(K_MSEC(20));
	gpio_pin_set(gpio_gps, GPS_RST_PIN, 1);
	
	gpio_pin_set(gpio_gps, GPS_EN_PIN, 1);
	k_sleep(K_MSEC(20));
}

/*============================================================================
* Function Name  : gps_disable
* Description    : AT6558R_EN使能禁止，低电平有效
* Input          : None
* Output         : None
* Return         : None
* CALL           : 可被外部调用
==============================================================================*/
void gps_disable(void)
{
	gpio_pin_set(gpio_gps, GPS_EN_PIN, 0);
}

void gps_turn_off(void)
{
#ifdef GPS_DEBUG
	LOGD("begin");
#endif
	
	gps_is_on = false;
	gps_disable();
	UartGpsOff();
}

void gps_off(void)
{
	if(!gps_is_on)
		return;

	gps_turn_off();
}

bool gps_turn_on(void)
{
	gps_is_on = true;
	gps_fix_time = 0;
	gps_local_time = 0;
	memset(&last_pvt, 0, sizeof(last_pvt));
	gps_start_time = k_uptime_get();

#ifdef GPS_DEBUG
	LOGD("begin");
#endif

	UartSwitchToGps();
	gps_enable();

	return true;
}

void gps_on(void)
{
	bool ret = false;
	
	if(gps_is_on)
		return;
	
	ret = gps_turn_on();
#ifdef CONFIG_FACTORY_TEST_SUPPORT
	if(!ret)
	{
		FTGPSStartFail();
	}
#endif
}

void gps_test_update(void)
{
	//if(screen_id == SCREEN_ID_GPS_TEST)
	//	scr_msg[screen_id].act = SCREEN_ACTION_UPDATE;
}

void MenuStartGPS(void)
{
	test_gps_flag = true;
	gps_on_flag = true;
}

void MenuStopGPS(void)
{
	gps_off_flag = true;
}

#ifdef CONFIG_FACTORY_TEST_SUPPORT
void FTStartGPS(void)
{
	test_gps_flag = true;
	gps_on();
}

void FTStopGPS(void)
{
	gps_off();
}
#endif

void gps_get_infor(void)
{
	static bool flag = false;

	Send_Cmd_To_AT6558(ZKW_GNSS_GET_SW_VERSION, strlen(ZKW_GNSS_GET_SW_VERSION));
	Send_Cmd_To_AT6558(ZKW_GNSS_GET_HW_VERSION, strlen(ZKW_GNSS_GET_HW_VERSION));
	Send_Cmd_To_AT6558(ZKW_GNSS_GET_MODE_VERSION, strlen(ZKW_GNSS_GET_MODE_VERSION));
	Send_Cmd_To_AT6558(ZKW_GNSS_GET_USEFUL_SIGNAL, strlen(ZKW_GNSS_GET_USEFUL_SIGNAL));
	Send_Cmd_To_AT6558(ZKW_GNSS_GET_IC_INFOR, strlen(ZKW_GNSS_GET_IC_INFOR));

	k_timer_start(&gps_turn_off_timer, K_SECONDS(5), K_NO_WAIT);	
}

void GPS_init(void)
{
	uart_gps_init();
	
	gpio_gps = DEVICE_DT_GET(GPS_PORT);
	if(!gpio_gps)
	{
	#ifdef GPS_DEBUG
		LOGD("Could not get gpio!");
	#endif
		return;
	}

	gpio_pin_configure(gpio_gps, GPS_RST_PIN, GPIO_OUTPUT);
	gpio_pin_configure(gpio_gps, GPS_EN_PIN, GPIO_OUTPUT);

	gpio_pin_set(gpio_gps, GPS_RST_PIN, 0);
	k_sleep(K_MSEC(20));
	gpio_pin_set(gpio_gps, GPS_RST_PIN, 1);
	
	gpio_pin_set(gpio_gps, GPS_EN_PIN, 1);
	k_sleep(K_MSEC(20));

	k_timer_start(&gps_get_infor_timer, K_SECONDS(5), K_NO_WAIT);
}

void GPSMsgProcess(void)
{
	if(gps_get_infor_flag)
	{
		gps_get_infor_flag = false;
		gps_get_infor();
	}

	if(gps_on_flag)
	{
		gps_on_flag = false;
		gps_on();
	}
	
	if(gps_off_flag)
	{
		gps_off_flag = false;
		test_gps_flag = false;
		gps_off();
	}

	if(gps_send_data_flag)
	{
		gps_send_data_flag = false;
		if(gps_local_time > 0)
		{
			APP_GPS_data_send(true);
		}
		else
		{
			APP_GPS_data_send(false);
		}
	}
	
	if(gps_test_update_flag)
	{
		gps_test_update_flag = false;
		gps_test_update();
	}
}

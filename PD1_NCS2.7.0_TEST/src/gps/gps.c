/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <date_time.h>
#include <logger.h>
#include "screen.h"
#include "gps.h"

//#define GPS_DEBUG

static struct k_work_q *app_work_q;
static struct k_work gps_data_get_work;
static void gps_data_get_work_fn(struct k_work *item);

static void APP_Ask_GPS_Data_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(app_wait_gps_timer, APP_Ask_GPS_Data_timerout, NULL);
static void APP_Send_GPS_Data_timerout(struct k_timer *timer_id);
K_TIMER_DEFINE(app_send_gps_timer, APP_Send_GPS_Data_timerout, NULL);

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

static const char update_indicator[] = {'\\', '|', '/', '-'};

static uint8_t last_pvt[512] = {0};

static void gps_data_get_work_fn(struct k_work *item)
{
	ARG_UNUSED(item);

	uint8_t cnt = 0;

#if 0	//xb test 2024-09-29
	for(;;)
	{
		(void)k_poll(events, 2, K_FOREVER);

		if(events[0].state == K_POLL_STATE_SEM_AVAILABLE 
			&& k_sem_take(events[0].sem, K_NO_WAIT) == 0)
		{
			/* New PVT data available */

			if(!IS_ENABLED(CONFIG_GNSS_SAMPLE_NMEA_ONLY))
			{
				//print_satellite_stats(&last_pvt);
			#ifdef GPS_DEBUG
				LOGD("pvt");
			#endif

				if(test_gps_flag)
				{
					uint8_t i,tracked = 0,flag = 0;
					uint8_t strbuf[256] = {0};
					int32_t lon,lat;
					
					memset(gps_test_info, 0x00, sizeof(gps_test_info));
					
					for(i=0;i<NRF_MODEM_GNSS_MAX_SATELLITES;i++)
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
									sprintf(buf, "%02d", last_pvt.sv[i].cn0/10);
								}
								else
								{
									sprintf(buf, "|%02d", last_pvt.sv[i].cn0/10);
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
									sprintf(buf, "%02d", last_pvt.sv[i].cn0/10);
								}
								else
								{
									sprintf(buf, "|%02d", last_pvt.sv[i].cn0/10);
								}
							}
							else
						  #endif
							{
								if(flag == 0)
								{
									flag = 1;
									sprintf(buf, "%02d|%02d", last_pvt.sv[i].sv, last_pvt.sv[i].cn0/10);
								}
								else
								{
									sprintf(buf, ";%02d|%02d", last_pvt.sv[i].sv, last_pvt.sv[i].cn0/10);
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

				if(last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED)
				{
				#ifdef GPS_DEBUG
					LOGD("GNSS operation blocked by LTE");
				#endif
				}
				
				if(last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_NOT_ENOUGH_WINDOW_TIME)
				{
				#ifdef GPS_DEBUG
					LOGD("Insufficient GNSS time windows");
				#endif
				}
				
				if(last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_SLEEP_BETWEEN_PVT)
				{
				#ifdef GPS_DEBUG
					LOGD("Sleep period(s) between PVT notifications");
				#endif
				}

				if(last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID)
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
					LOGD("Seconds since last fix: %lld", (k_uptime_get() - gps_start_time) / 1000);
				#endif
					cnt++;
				#ifdef GPS_DEBUG
					LOGD("Searching [%c]", update_indicator[cnt%4]);
				#endif
				}
			}
		#if defined(CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST)
			else
			{
				/* Calculate the time GNSS has been blocked by LTE. */
				if(last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_DEADLINE_MISSED)
				{
					time_blocked++;
				}
			}
		#endif/*CONFIG_GNSS_SAMPLE_MODE_TTFF_TEST*/
		}
		if(events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE 
			&& k_msgq_get(events[1].msgq, &nmea_data, K_NO_WAIT) == 0)
		{
			/* New NMEA data available */
		#ifdef GPS_DEBUG
			LOGD("NMEA:%s", nmea_data->nmea_str);
		#endif

			k_free(nmea_data);
		}

		events[0].state = K_POLL_STATE_NOT_READY;
		events[1].state = K_POLL_STATE_NOT_READY;

		if(!gps_is_on)
			break;
	}
#endif	
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

void gps_turn_off(void)
{
#ifdef GPS_DEBUG
	LOGD("begin");
#endif

	gps_is_on = false;
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
	if(screen_id == SCREEN_ID_GPS_TEST)
		scr_msg[screen_id].act = SCREEN_ACTION_UPDATE;
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

void GPS_init(struct k_work_q *work_q)
{
	app_work_q = work_q;
	gps_work_init();
}

void GPSTestInit(void)
{
}

void GPSTestUnInit(void)
{
}

void GPSMsgProcess(void)
{
	if(gps_on_flag)
	{
		gps_on_flag = false;
		if(test_gps_flag)
		{
			GPSTestInit();
		}
		gps_on();
	}
	
	if(gps_off_flag)
	{
		gps_off_flag = false;
		gps_off();
		if(test_gps_flag)
		{
			GPSTestUnInit();
			test_gps_flag = false;
		}
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

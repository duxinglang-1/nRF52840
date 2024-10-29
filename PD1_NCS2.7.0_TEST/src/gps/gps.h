/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr/kernel.h>
#include <stdio.h>

#define APP_WAIT_GPS_TIMEOUT	(3*60)
#define GNSS_MAX_SATELLITES		14
#define GNSS_NMEA_MAX_LEN 		83

#define ZKW_GNSS_SET_UART_BAUDRATE	"$PCAS01,5"	//0=4800bps,1=9600bps,2=19200bps,3=38400bps,4=57600bps,5=115200bps,6=230400bps,7=460800bps

#define ZKW_GNSS_SET_HOT_REBOOT		"$PCAS10,0"
#define ZKW_GNSS_SET_WARM_REBOOT	"$PCAS10,1"
#define ZKW_GNSS_SET_COLD_REBOOT	"$PCAS10,2"
#define ZKW_GNSS_SET_FACTORY_REBOOT	"$PCAS10,3"

#define ZKW_GNSS_GET_SW_VERSION		"$PCAS06,0"
#define ZKW_GNSS_GET_HW_VERSION		"$PCAS06,1"
#define ZKW_GNSS_GET_MODE_VERSION	"$PCAS06,2"
#define ZKW_GNSS_GET_USEFUL_SIGNAL	"$PCAS06,4"
#define ZKW_GNSS_GET_IC_INFOR		"$PCAS06,6"


//$GNGGA,025029.00,3011.16504,N,12009.38696,E,1,27,0.6,93.96,M,7.05,M,,*77
#define GNSS_GNGGA_FIELD	"GGA"

//$GNGLL,2240.05632,N,11401.51793,E,085842.000,A,A*4E
#define GNSS_GNGLL_FIELD	"GLL"

//$GNGSA,A,3,03,04,08,16,26,27,31,32,194,195,199,,1.1,0.6,0.9,1*04
//$GNGSA,A,3,07,08,10,12,14,25,33,34,38,40,41,42,1.1,0.6,0.9,4*3F
#define GNSS_GNGSA_FIELD	"GSA"

//$GPGSV,3,1,12,03,24,261,21,04,32,321,24,08,20,198,17,16,66,314,22,0*69
//$GPGSV,3,2,12,26,49,020,23,27,47,175,28,28,30,079,08,31,48,043,22,0*64
//$GPGSV,3,3,12,32,16,147,24,194,64,054,20,195,22,137,23,199,60,149,26,0*5F
#define GNSS_GPGSV_FIELD	"$GPGSV"

//$BDGSV,4,1,14,07,60,193,27,08,11,200,19,10,49,203,26,12,28,086,16,0*70
//$BDGSV,4,2,14,14,27,178,18,25,50,344,24,33,45,202,27,34,36,119,27,0*7D
//$BDGSV,4,3,14,38,09,188,18,40,63,168,29,41,46,282,24,42,06,163,23,0*75
//$BDGSV,4,4,14,43,21,170,23,44,18,062,23,0*7E
#define GNSS_BDGSV_FIELD	"$BDGSV"

//$GNRMC,085842.000,A,2240.05632,N,11401.51793,E,0.00,295.02,251024,,,A,V*0F
#define GNSS_GNRMC_FIELD	"RMC"

//$GPTXT,01,01,02,MA=CASGN*24
//$GPTXT,01,01,02,HW=AT6668,0012345612345*6E
//$GPTXT,01,01,02,IC=AT6668-6X-32-00000917,CDS2A2J-P1-000236*39
#define GNSS_GNTXT_FIELD	"$GPTXT"
#define GNSS_GNTXT_MA		"MA="
#define GNSS_GNTXT_HW		"HW="
#define GNSS_GNTXT_IC		"IC="
#define GNSS_GNTXT_SW		"SW="
#define GNSS_GNTXT_MO		"MO="
#define GNSS_GNTXT_SM		"SM="
#define GNSS_GNTXT_ANT		"ANTENNA "


#define GNSS_FIELD_END		"\r\n"
#define GNSS_FIELD_CHECKSUM	"*"

/** @defgroup nrf_modem_gnss_sv_flag_bitmask Satellite flags bitmask values
 *
 * @brief Bitmask values for the flags member in the SV elements in the PVT notification struct.
 * @{
 */
/** @brief The satellite is used in the position calculation. */
typedef enum
{
	GNSS_SV_FLAG_USED_IN_FIX,
	GNSS_SV_FLAG_UNHEALTHY,	
}GNSS_SV_FLAG;

typedef struct
{
	/** 4-digit representation (Gregorian calendar). */
	uint16_t year;
	/** 1...12 */
	uint8_t month;
	/** 1...31 */
	uint8_t day;
	/** 0...23 */
	uint8_t hour;
	/** 0...59 */
	uint8_t minute;
	/** 0...59 */
	uint8_t seconds;
	/** 0...999 */
	uint16_t ms;
}gnss_datetime_t;

typedef enum
{
	GNSS_SIGNAL_GPS,
	GNSS_SIGNAL_BDS,
}GNSS_SIGNAL_TYPE;

/** @brief Space Vehicle (SV) information. */
typedef struct
{
	/** Satellite ID.
	 *  1...32 for GPS
	 *  193...202 for QZSS.
	 */
	uint16_t sv;
	/** Signal type, see @ref GNSS_SIGNAL_TYPE. */
	GNSS_SIGNAL_TYPE signal;
	/** 0.1 dB/Hz. */
	uint16_t cn0;
	/** Satellite elevation angle in degrees. */
	int16_t elevation;
	/** Satellite azimuth angle in degrees. */
	int16_t azimuth;
	/** Satellite type. */
	GNSS_SV_FLAG flags;
}gnss_sv_t;

/** @defgroup nrf_modem_gnss_pvt_flag_bitmask Bitmask values for flags in the PVT notification
 *
 * @brief Bitmask values for the flags member in the #nrf_modem_gnss_pvt_data_frame.
 * @{
 */
typedef enum
{
	GNSS_PVT_FLAG_FIX_UNVALID,
	GNSS_PVT_FLAG_FIX_VALID	
}GNSS_PVT_FLAG;


/** @brief Position, Velocity and Time (PVT) data frame. */
typedef struct
{
	/** Latitude in degrees. */
	double latitude;
	/** Longitude in degrees. */
	double longitude;
	/** Altitude above WGS-84 ellipsoid in meters. */
	float altitude;
	/** Horizontal speed in m/s. */
	float speed;
	/** Heading of user movement in degrees. */
	float heading;
	/** Date and time. */
	gnss_datetime_t datetime;
	/** Position dilution of precision. */
	float pdop;
	/** Horizontal dilution of precision. */
	float hdop;
	/** Vertical dilution of precision. */
	float vdop;
	/** fix status. */
	GNSS_PVT_FLAG flags;
	/** Describes up to 14 of the satellites used for the measurement. */
	gnss_sv_t sv[GNSS_MAX_SATELLITES];
}gnss_pvt_data_frame_t;

/** @brief Single null-terminated NMEA sentence. */
typedef struct
{
	/** Null-terminated NMEA sentence. */
	char nmea_str[GNSS_NMEA_MAX_LEN];
}gnss_nmea_data_frame_t;

extern bool gps_on_flag;
extern bool gps_off_flag;

extern bool ble_wait_gps;
extern bool sos_wait_gps;
#ifdef CONFIG_FALL_DETECT_SUPPORT
extern bool fall_wait_gps;
#endif
extern bool location_wait_gps;
extern bool test_gps_flag;

extern uint8_t gps_test_info[256];

extern void test_gps_on(void);
extern void GPS_init(void);
extern void GPSMsgProcess(void);

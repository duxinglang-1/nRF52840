/****************************************Copyright (c)************************************************
** File Name:				transfer_cache.h
** Descriptions:			Data transfer cache pool head file
** Created By:				xie biao
** Created Date:			2021-03-25
** Modified Date:			2021-03-25 
** Version:					V1.0
******************************************************************************************************/
#ifndef __UART_BLE_H__
#define __UART_BLE_H__

typedef enum
{
	UART_DATA_WIFI,
	UART_DATA_GPS,
	UART_DATA_MAX
}UART_DATA_TYPE;

extern void UartSwitchToGps(void);
extern void UartSwitchToWifi(void);

#endif/*__UART_BLE_H__*/

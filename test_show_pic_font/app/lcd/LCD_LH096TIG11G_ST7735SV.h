#include "lcd.h"

#ifdef LCD_LH096TIG11G_ST7735SV

#include "boards.h"

//�ӿڶ���
//extern xdata unsigned char buffer[512];
//------------------------------------------------------
#define ROW 160			//��ʾ���С�
#define COL 80			//����

//LCD�Ļ�����ɫ�ͱ���ɫ	   
extern uint16_t  POINT_COLOR;//Ĭ�Ϻ�ɫ    
extern uint16_t  BACK_COLOR; //������ɫ.Ĭ��Ϊ��ɫ

//LCM
#define	CS 				NRF_GPIO_PIN_MAP(0,4)	
#define	RST				NRF_GPIO_PIN_MAP(0,28)
#define	RS				NRF_GPIO_PIN_MAP(0,29)
#define	SCL				NRF_GPIO_PIN_MAP(0,30)
#define	SDA				NRF_GPIO_PIN_MAP(0,31)

//TP 
#define TP_0			NRF_GPIO_PIN_MAP(1,12)
#define TP_1			NRF_GPIO_PIN_MAP(1,12)

//LEDK(LED����)
#define LEDK			NRF_GPIO_PIN_MAP(0,3)

#define X_min 0x0043		 //TP���Է�Χ��������
#define X_max 0x07AE
#define Y_min 0x00A1
#define Y_max 0x0759
//------------------------------------------------------

extern void BlockWrite(unsigned int x,unsigned int y,unsigned int w,unsigned int h);
extern void WriteOneDot(unsigned int color);
extern void Write_Data(uint8_t i);
extern void LCD_Clear(uint16_t color);
extern void LCD_Init(void);
extern void LCD_SleepIn(void);
extern void LCD_SleepOut(void);

#endif/*LCD_LH096TIG11G_ST7735SV*/

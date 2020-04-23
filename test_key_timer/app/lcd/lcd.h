#ifndef __LCD_H__
#define __LCD_H__
#include "boards.h"

//接口定义
//extern xdata unsigned char buffer[512];
//------------------------------------------------------
#define ROW 320			//显示的行、
#define COL 320			//列数

//LCD的画笔颜色和背景色	   
extern uint16_t  POINT_COLOR;//默认红色    
extern uint16_t  BACK_COLOR; //背景颜色.默认为白色

//LCM
#define	CS0 			NRF_GPIO_PIN_MAP(1,13)	
#define	RST				NRF_GPIO_PIN_MAP(1,8)
#define	RS				NRF_GPIO_PIN_MAP(1,14)
#define	WR0				NRF_GPIO_PIN_MAP(1,15)
#define	RD0				NRF_GPIO_PIN_MAP(0,02)

//DB0~7
#define DB0				NRF_GPIO_PIN_MAP(1,06)
#define DB1				NRF_GPIO_PIN_MAP(1,07)
#define DB2				NRF_GPIO_PIN_MAP(1,05)
#define DB3				NRF_GPIO_PIN_MAP(0,27)
#define DB4				NRF_GPIO_PIN_MAP(1,04)
#define DB5				NRF_GPIO_PIN_MAP(1,03)
#define DB6				NRF_GPIO_PIN_MAP(0,26)
#define DB7				NRF_GPIO_PIN_MAP(1,02)

#define DBS_LIST { DB0, DB1, DB2, DB3, DB4, DB5, DB6, DB7 }

//TP 
#define TP_CS			NRF_GPIO_PIN_MAP(1,12)

//LEDK(LED背光)
#define LEDK_1          NRF_GPIO_PIN_MAP(1,10)
#define LEDK_2          NRF_GPIO_PIN_MAP(1,11)


//------------------------------------------------------
#define PIC_WIDTH    160	 //预备向LCD显示区域填充的图片的大小
#define PIC_HEIGHT   160


//画笔颜色
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //棕色
#define BRRED 			 0XFC07 //棕红色
#define GRAY  			 0X8430 //灰色
//GUI颜色

#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  
#define GRAYBLUE       	 0X5458 //灰蓝色
//以上三色为PANEL的颜色 
 
#define LIGHTGREEN     	 0X841F //浅绿色
//#define LIGHTGRAY        0XEF5B //浅灰色(PANNEL)
#define LGRAY 			 0XC618 //浅灰色(PANNEL),窗体背景色

#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)



#define X_min 0x0043		 //TP测试范围常量定义
#define X_max 0x07AE
#define Y_min 0x00A1
#define Y_max 0x0759
//------------------------------------------------------

void BlockWrite(unsigned int Xstart,unsigned int Xend,unsigned int Ystart,unsigned int Yend);
void WriteOneDot(unsigned int color);
void LCD_Init(void);
void Write_Data(uint8_t i);
void LCD_Clear(uint16_t color);
void LCD_Fill(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color);
void LCD_Color_Fill(uint16_t x,uint16_t y,uint16_t w,uint16_t h,unsigned int *color);
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void LCD_Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r);
void LCD_ShowString(uint16_t x,uint16_t y,uint8_t *p);
void LCD_ShowStringInRect(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t *p);
void LCD_ShowNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len);
void LCD_ShowxNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t mode);
void LCD_SetFontSize(uint8_t font_size);
void LCD_MeasureString(uint8_t *p, uint16_t *width,uint16_t *height);

#endif

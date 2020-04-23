#include "lcd.h"
#include "stdlib.h"
#include "font.h" 

#ifdef LCD_R154101_ST7796S
#include "LCD_R154101_ST7796S.h"

uint8_t m_db_list[8] = DBS_LIST;	//������Ļ���ݽӿ�����

//LCD��ʱ����
void Delay(unsigned int dly)
{
    unsigned int i,j;

    for(i=0;i<dly;i++)
    	for(j=0;j<255;j++);
}

//���ݽӿں���
//i:8λ����
void Write_Data(uint8_t i) 
{
	uint8_t t;
	
	for(t = 0; t < 8; t++)
	{
		if(i & 0x01)						//������д�����ݽӿ�
			nrf_gpio_pin_set(m_db_list[t]);
		else
			nrf_gpio_pin_clear(m_db_list[t]);
		
		i >>= 1;							//��������һλ
	}
}

//----------------------------------------------------------------------
//д�Ĵ�������
//i:�Ĵ���ֵ
void WriteComm(unsigned int i)
{
	nrf_gpio_pin_clear(CS0);				//CS��0
	nrf_gpio_pin_set(RD0);					//RD��1
	nrf_gpio_pin_clear(RS);					//RS��0
		
	Write_Data(i);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);

	nrf_gpio_pin_set(CS0);
}

//дLCD����
//i:Ҫд���ֵ
void WriteData(unsigned int i)
{
	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RD0);
	nrf_gpio_pin_set(RS);
		
	Write_Data(i);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);

	nrf_gpio_pin_set(CS0);
}

void WriteDispData(unsigned char DataH,unsigned char DataL)
{
	Write_Data(DataH);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	 
	Write_Data(DataL);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
}

//LCD���㺯��
//color:Ҫ������ɫ
void WriteOneDot(unsigned int color)
{ 
	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RD0);
	nrf_gpio_pin_set(RS);

	Write_Data(color>>8);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);

	Write_Data(color);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);

	nrf_gpio_pin_set(CS0);
}

//LCD��ʼ������
void LCD_Init(void)
{
	//�˿ڳ�ʼ��
	nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0,02));			//���ö˿�Ϊ���
	nrf_gpio_range_cfg_output(NRF_GPIO_PIN_MAP(0,26),NRF_GPIO_PIN_MAP(0,27));
	nrf_gpio_range_cfg_output(NRF_GPIO_PIN_MAP(1,02),NRF_GPIO_PIN_MAP(1,8));
	nrf_gpio_range_cfg_output(NRF_GPIO_PIN_MAP(1,10),NRF_GPIO_PIN_MAP(1,15));	
		
	nrf_gpio_pin_set(RST);
	Delay(100);
	
	nrf_gpio_pin_clear(RST);
	Delay(800);

	nrf_gpio_pin_set(RST);
	Delay(800);

	WriteComm(0x36);
	WriteData(0x48);
	WriteComm(0x3a);
	WriteData(0x05);

	WriteComm(0xF0);
	WriteData(0xC3);
	WriteComm(0xF0);
	WriteData(0x96);

	WriteComm(0xb1);
	WriteData(0xa0);
	WriteData(0x10);

	WriteComm(0xb4);
	WriteData(0x00);

	WriteComm(0xb5);
	WriteData(0x40);
	WriteData(0x40);
	WriteData(0x00);
	WriteData(0x04);

	WriteComm(0xb6);
	WriteData(0x8a);
	WriteData(0x07);
	WriteData(0x27);

	WriteComm(0xb9);
	WriteData(0x02);

	WriteComm(0xc5);
	WriteData(0x2e);

	WriteComm(0xE8);
	WriteData(0x40);
	WriteData(0x8a);
	WriteData(0x00);
	WriteData(0x00);
	WriteData(0x29);
	WriteData(0x19);
	WriteData(0xa5);
	WriteData(0x93);

	WriteComm(0xe0);
	WriteData(0xf0);
	WriteData(0x07);
	WriteData(0x0e);
	WriteData(0x0a);
	WriteData(0x08);
	WriteData(0x25);
	WriteData(0x38);
	WriteData(0x43);
	WriteData(0x51);
	WriteData(0x38);
	WriteData(0x14);
	WriteData(0x12);
	WriteData(0x32);
	WriteData(0x3f);

	WriteComm(0xe1);
	WriteData(0xf0);
	WriteData(0x08);
	WriteData(0x0d);
	WriteData(0x09);
	WriteData(0x09);
	WriteData(0x26);
	WriteData(0x39);
	WriteData(0x45);
	WriteData(0x52);
	WriteData(0x07);
	WriteData(0x13);
	WriteData(0x16);
	WriteData(0x32);
	WriteData(0x3f);

	WriteComm(0xf0);
	WriteData(0x3c);
	WriteComm(0xf0);
	WriteData(0x69);
	WriteComm(0x11);
	Delay(150);  
	WriteComm(0x21);
	WriteComm(0x29);

	//��������
	nrf_gpio_pin_clear(LEDK_1);
	nrf_gpio_pin_clear(LEDK_2);
				
	LCD_Clear(WHITE);		//����Ϊ��ɫ
}

 
////////////////////////////////////////////////���Ժ���//////////////////////////////////////////
void BlockWrite(unsigned int x,unsigned int y,unsigned int w,unsigned int h) //reentrant
{
	//ST7796
	WriteComm(0x2A);             
	WriteData(x>>8);             
	WriteData(x);             
	WriteData((x+w)>>8);             
	WriteData((x+w));             
	
	WriteComm(0x2B);             
	WriteData(y>>8);             
	WriteData(y);             
	WriteData((y+h)>>8);//	WriteData((Yend+1)>>8);             
	WriteData((y+h));//	WriteData(Yend+1);   	

	WriteComm(0x2c);
}

void DispColor(unsigned int color)
{
	unsigned int i,j;

	BlockWrite(0,0,COL-1,ROW-1);

	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RS);
	nrf_gpio_pin_set(RD0);

	for(i=0;i<ROW;i++)
	{
	    for(j=0;j<COL;j++)
		{    
			Write_Data(color>>8);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);

			Write_Data(color);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);
		}
	}

	nrf_gpio_pin_set(CS0);
}

//���Ժ�������ʾRGB���ƣ�
void DispBand(void)	 
{
	unsigned int i,j,k;
	//unsigned int color[8]={0x001f,0x07e0,0xf800,0x07ff,0xf81f,0xffe0,0x0000,0xffff};
	unsigned int color[8]={0xf800,0xf800,0x07e0,0x07e0,0x001f,0x001f,0xffff,0xffff};//0x94B2
	//unsigned int gray16[]={0x0000,0x1082,0x2104,0x3186,0x42,0x08,0x528a,0x630c,0x738e,0x7bcf,0x9492,0xa514,0xb596,0xc618,0xd69a,0xe71c,0xffff};

   	BlockWrite(0,0,COL-1,ROW-1);
   
	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RD0);
	nrf_gpio_pin_set(RS);

	for(i=0;i<8;i++)
	{
		for(j=0;j<ROW/8;j++)
		{
	        for(k=0;k<COL;k++)
			{
				
				Write_Data(color[i]>>8);  
				nrf_gpio_pin_clear(WR0);
				nrf_gpio_pin_set(WR0);

				Write_Data(color[i]);  
				nrf_gpio_pin_clear(WR0);
				nrf_gpio_pin_set(WR0);
			} 
		}
	}
	for(j=0;j<(ROW%8);j++)
	{
		for(k=0;k<COL;k++)
		{
			Write_Data(color[7]>>8);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);

			Write_Data(color[7]);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);
		} 
	}
	
	nrf_gpio_pin_set(CS0);
}

//���Ժ��������߿�
void DispFrame(void)
{
	unsigned int i,j;
	
	BlockWrite(0,0,COL-1,ROW-1);

	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RD0);
	nrf_gpio_pin_set(RS);
		
	Write_Data(0xf8);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	
	Write_Data(0x00);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	for(i=0;i<COL-2;i++)
	{
		Write_Data(0xFF);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
		
		Write_Data(0xFF);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
	}
	Write_Data(0x00);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	
	Write_Data(0x1F);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);

	for(j=0;j<ROW-2;j++)
	{
		Write_Data(0xf8);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
		
		Write_Data(0x00);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
		for(i=0;i<COL-2;i++)
		{
			Write_Data(0x00);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);
			
			Write_Data(0x00);  
			nrf_gpio_pin_clear(WR0);
			nrf_gpio_pin_set(WR0);
		}
		Write_Data(0x00);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
		
		Write_Data(0x1f);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
	}

	Write_Data(0xf8);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	
	Write_Data(0x00);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	for(i=0;i<COL-2;i++)
	{
		Write_Data(0xff);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
		
		Write_Data(0xff);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
	}
	Write_Data(0x00);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	
	Write_Data(0x1f);  
	nrf_gpio_pin_clear(WR0);
	nrf_gpio_pin_set(WR0);
	
	nrf_gpio_pin_set(CS0);
}
//////////////////////////////////////////////////////////////////////////////////////////////

//��������
//color:Ҫ���������ɫ
void LCD_Clear(uint16_t color)
{
	uint32_t index=0;      
	uint32_t totalpoint=ROW;
	totalpoint*=COL; 			//�õ��ܵ���
	
	BlockWrite(0,0,COL-1,ROW-1);//��λ

	nrf_gpio_pin_clear(CS0);
	nrf_gpio_pin_set(RS);
	nrf_gpio_pin_set(RD0);

	for(index=0;index<totalpoint;index++)
	{
		Write_Data(color>>8);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);

		Write_Data(color);  
		nrf_gpio_pin_clear(WR0);
		nrf_gpio_pin_set(WR0);
	}
	nrf_gpio_pin_set(CS0);
} 

#endif

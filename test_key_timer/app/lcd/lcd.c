#include "lcd.h"
#include "stdlib.h"
#include "font.h" 

uint8_t m_db_list[8] = DBS_LIST;	//������Ļ���ݽӿ�����
//LCD�Ļ�����ɫ�ͱ���ɫ	   
uint16_t POINT_COLOR=WHITE;	//������ɫ
uint16_t BACK_COLOR=BLACK;  //����ɫ 

system_font_size system_font = FONT_SIZE_16;

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

#if 0
	WriteComm(0x11);
	Delay(150);
	WriteComm(0xF0);
	WriteData(0xC3);
	WriteComm(0xF0);
	WriteData(0x96);
	WriteComm(0xb6);
	WriteData(0x8A);
	WriteData(0x07);
	WriteData(0x3b);
	WriteComm(0xb5);
	WriteData(0x20);
	WriteData(0x20);
	WriteData(0x00);
	WriteData(0x20);
	WriteComm(0xb1);
	WriteData(0x80);
	WriteData(0x10);
	WriteComm(0x36);
	WriteData(0x48);
	WriteComm(0xb4);
	WriteData(0x00);
	WriteComm(0xc5);
	WriteData(0x28);
	WriteComm(0xc1);
	WriteData(0x04);
	WriteComm(0x21);
	WriteComm(0xE8);
	WriteData(0x40);
	WriteData(0x84);
	WriteData(0x1b);
	WriteData(0x1b);
	WriteData(0x10);
	WriteData(0x03);
	WriteData(0xb8);
	WriteData(0x33);
	WriteComm(0xe0);
	WriteData(0x00);
	WriteData(0x03);
	WriteData(0x07);
	WriteData(0x07);
	WriteData(0x07);
	WriteData(0x23);
	WriteData(0x2b);
	WriteData(0x33);
	WriteData(0x46);
	WriteData(0x1a);
	WriteData(0x19);
	WriteData(0x19);
	WriteData(0x27);
	WriteData(0x2f);
	WriteComm(0xe1);
	WriteData(0x00);
	WriteData(0x03);
	WriteData(0x06);
	WriteData(0x07);
	WriteData(0x04);
	WriteData(0x22);
	WriteData(0x2f);
	WriteData(0x54);
	WriteData(0x49);
	WriteData(0x1b);
	WriteData(0x17);
	WriteData(0x15);
	WriteData(0x25);
	WriteData(0x2d);
	WriteComm(0x12);
	WriteComm(0x30);
	WriteData(0x00);
	WriteData(0x00);
	WriteData(0x01);
	WriteData(0x3f);
	Delay(100);  
	WriteComm(0xe4);
	WriteData(0x31);
	WriteComm(0xF0);
	WriteData(0x3c);
	WriteComm(0xF0);
	WriteData(0x69);

	WriteComm(0x3a);
	WriteData(0x05);

	WriteComm(0x29);
#endif

#if  1
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
#endif 

	//��������
	nrf_gpio_pin_clear(LEDK_1);
	nrf_gpio_pin_clear(LEDK_2);
				
	LCD_Clear(WHITE);		//����Ϊ��ɫ
}

 

	
////////////////////////////////////////////////���Ժ���//////////////////////////////////////////
void BlockWrite(unsigned int Xstart,unsigned int Xend,unsigned int Ystart,unsigned int Yend) //reentrant
{
	//ILI9327
	WriteComm(0x2A);             
	WriteData(Xstart>>8);             
	WriteData(Xstart);             
	WriteData(Xend>>8);             
	WriteData(Xend);             
	
	WriteComm(0x2B);             
	WriteData(Ystart>>8);             
	WriteData(Ystart);             
	WriteData(Yend>>8);//	WriteData((Yend+1)>>8);             
	WriteData(Yend);//	WriteData(Yend+1);   	

	WriteComm(0x2c);
}

void DispColor(unsigned int color)
{
	unsigned int i,j;

	BlockWrite(0,COL-1,0,ROW-1);

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

   	BlockWrite(0,COL-1,0,ROW-1);
   
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
	for(j = 0;j < (ROW % 8);j++)
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
	
	BlockWrite(0,COL-1,0,ROW-1);

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
	
	BlockWrite(0,COL-1,0,ROW-1);//��λ

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

//���ٻ���
//x,y:����
//color:��ɫ
void LCD_Fast_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{	   
	BlockWrite(x,x + 1,y,y + 1);	//������
	WriteOneDot(color);				//���㺯��	
}	 

//��ָ����������䵥����ɫ
//(x,y),(w,h):�����ζԽ�����,�����СΪ:w*h   
//color:Ҫ������ɫ
void LCD_Fill(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color)
{          
	uint16_t i,j;

	for(i=y;i<=(y+h);i++)
	{
		BlockWrite(x,x+w,i,y+h);	  
		for(j=0;j<w;j++)
			WriteOneDot(color);	//��ʾ��ɫ 
	}	 
}  
//��ָ�����������ָ����ɫ��	(��ʾͼƬ)		 
//(x,y),(w,h):�����ζԽ�����,�����СΪ:w*h   
//color:Ҫ������ɫ
void LCD_Color_Fill(uint16_t x,uint16_t y,uint16_t w,uint16_t h,unsigned int *color)
{  
	uint16_t i,j;

 	for(i=0;i<h;i++)
	{
		BlockWrite(x,x+w,y+i,y+h);	  
		for(j=0;j<w;j++)
			WriteOneDot(color[i*w+j]);	//��ʾ��ɫ 
		
// 		LCD_SetCursor(sx,sy+i);   	//���ù��λ�� 
//		LCD_WriteRAM_Prepare();     //��ʼд��GRAM
//		for(j=0;j<width;j++)
//			LCD->LCD_RAM=color[i*width+j];//д������ 
	}		  
} 

//����
//x1,y1:�������
//x2,y2:�յ�����  
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	uint16_t t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance; 
	int incx,incy,uRow,uCol; 
	delta_x=x2-x1; //������������ 
	delta_y=y2-y1; 
	uRow=x1; 
	uCol=y1; 
	if(delta_x>0)incx=1; //���õ������� 
	else if(delta_x==0)incx=0;//��ֱ�� 
	else {incx=-1;delta_x=-delta_x;} 
	if(delta_y>0)incy=1; 
	else if(delta_y==0)incy=0;//ˮƽ�� 
	else{incy=-1;delta_y=-delta_y;} 
	if( delta_x>delta_y)distance=delta_x; //ѡȡ�������������� 
	else distance=delta_y; 
	for(t=0;t<=distance+1;t++ )//������� 
	{  
		LCD_Fast_DrawPoint(uRow,uCol,POINT_COLOR);//���� 
		xerr+=delta_x ; 
		yerr+=delta_y ; 
		if(xerr>distance) 
		{ 
			xerr-=distance; 
			uRow+=incx; 
		} 
		if(yerr>distance) 
		{ 
			yerr-=distance; 
			uCol+=incy; 
		} 
	}  
}    
//������	  
//(x1,y1),(x2,y2):���εĶԽ�����
void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	LCD_DrawLine(x1,y1,x2,y1);
	LCD_DrawLine(x1,y1,x1,y2);
	LCD_DrawLine(x1,y2,x2,y2);
	LCD_DrawLine(x2,y1,x2,y2);
}

//��ָ��λ�û�һ��ָ����С��Բ
//(x,y):���ĵ�
//r    :�뾶
void LCD_Draw_Circle(uint16_t x0,uint16_t y0,uint8_t r)
{
	int a,b;
	int di;
	a=0;b=r;	  
	di=3-(r<<1);             //�ж��¸���λ�õı�־
	while(a<=b)
	{
		LCD_Fast_DrawPoint(x0+a,y0-b,POINT_COLOR);//5
		LCD_Fast_DrawPoint(x0+b,y0-a,POINT_COLOR);//0 
		LCD_Fast_DrawPoint(x0+b,y0+a,POINT_COLOR);//4
		LCD_Fast_DrawPoint(x0+a,y0+b,POINT_COLOR);//6
		
		LCD_Fast_DrawPoint(x0-a,y0+b,POINT_COLOR);//1
		LCD_Fast_DrawPoint(x0-b,y0+a,POINT_COLOR);
		LCD_Fast_DrawPoint(x0-a,y0-b,POINT_COLOR);//2
		LCD_Fast_DrawPoint(x0-b,y0-a,POINT_COLOR);//7 
                	         
		a++;
		//ʹ��Bresenham�㷨��Բ     
		if(di<0)di +=4*a+6;	  
		else
		{
			di+=10+4*(a-b);   
			b--;
		} 						    
	}
} 

//��ָ��λ����ʾһ���ַ�
//x,y:��ʼ����
//num:Ҫ��ʾ���ַ�:" "--->"~"
//mode:���ӷ�ʽ(1)���Ƿǵ��ӷ�ʽ(0)
void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t num,uint8_t mode)
{  							  
    uint8_t temp,t1,t;
	uint16_t y0=y;
	uint8_t csize=(system_font/8+((system_font%8)?1:0))*(system_font/2);		//�õ�����һ���ַ���Ӧ������ռ���ֽ���	
 	
	num=num-' ';//�õ�ƫ�ƺ��ֵ��ASCII�ֿ��Ǵӿո�ʼȡģ������-' '���Ƕ�Ӧ�ַ����ֿ⣩
	for(t=0;t<csize;t++)
	{   
		switch(system_font)
		{
			case FONT_SIZE_16:
				temp=asc2_1608[num][t]; 	 	//����1608����
				break;
			case FONT_SIZE_24:
				temp=asc2_2412[num][t];			//����2412����
				break;
			case FONT_SIZE_32:
				temp=asc2_3216[num][t];			//����3216����
				break;
			default:
				return;							//û�е��ֿ�
		}
								
		for(t1=0;t1<8;t1++)
		{			    
			if(temp&0x80)LCD_Fast_DrawPoint(x,y,POINT_COLOR);
			else if(mode==0)LCD_Fast_DrawPoint(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=COL)return;		//��������
			if((y-y0)==system_font)
			{
				y=y0;
				x++;
				if(x>=ROW)return;	//��������
				break;
			}
		}  	 
	}  	    	   	 	  
}   

//��ָ��λ����ʾһ���ַ�
//x,y:��ʼ����
//num:Ҫ��ʾ���ַ�:" "--->"~"
//mode:���ӷ�ʽ(1)���Ƿǵ��ӷ�ʽ(0)
void LCD_ShowChineseChar(uint16_t x,uint16_t y,uint16_t num,uint8_t mode)
{  							  
	uint8_t temp,t1,t;
	uint16_t y0=y;
	uint16_t index=0;
	
	uint8_t csize=(system_font/8+((system_font%8)?1:0))*(system_font);		//�õ�����һ���ַ���Ӧ������ռ���ֽ���	
 	index=94*((num>>8)-0xa0-1)+1*((num&0x00ff)-0xa0-1);			//offset = (94*(����-1)+(λ��-1))*32
	for(t=0;t<csize;t++)
	{	
		switch(system_font)
		{
			case FONT_SIZE_16:
				temp=chinese_1616[index][t]; 	 	//����1616����
				break;
			case FONT_SIZE_24:
				temp=chinese_2424[index][t];		//����2424����
				break;
			case FONT_SIZE_32:
				//temp=chinese_3232[index][t];		//����3232����
				//break;
			default:
				return;								//û�е��ֿ�
		}	

		for(t1=0;t1<8;t1++)
		{			    
			if(temp&0x80)LCD_Fast_DrawPoint(x,y,POINT_COLOR);
			else if(mode==0)LCD_Fast_DrawPoint(x,y,BACK_COLOR);
			temp<<=1;
			y++;
			if(y>=COL)return;		//��������
			if((y-y0)==system_font)
			{
				y=y0;
				x++;
				if(x>=ROW)return;	//��������
				break;
			}
		} 
	}  	    	   	 	  
}   

//��ָ��������������ʾ��Ӣ���ַ���
//x,y:�������
//width,height:�����С  
//*p:�ַ�����ʼ��ַ	
void LCD_ShowStringInRect(uint16_t x,uint16_t y,uint16_t width,uint16_t height,uint8_t *p)
{
	uint8_t x0=x;
	uint16_t phz=0;
	
	width+=x;
	height+=y;
    while(*p)//((*p<='~')&&(*p>=' '))//�ж��ǲ��ǷǷ��ַ�!
    {       
        if(x>=width){x=x0;y+=system_font;}
        if(y>=height)break;//�˳�
		if(*p<0x80)
		{
			LCD_ShowChar(x,y,*p,0);
			x+=system_font/2;
			p++;
		}
		else if(*(p+1))
        {
			phz = *p<<8;
			phz += *(p+1);
			LCD_ShowChineseChar(x,y,phz,0);
			x+=system_font;
			p+=2;
		}        
    }
}

//��ʾ��Ӣ���ַ���
//x,y:�������
//*p:�ַ�����ʼ��ַ	
void LCD_ShowString(uint16_t x,uint16_t y,uint8_t *p)
{
	uint8_t x0=x;
	uint16_t phz=0;
	
    while(*p)
    {       
        if(x>=ROW)break;//�˳�
        if(y>=COL)break;//�˳�
		if(*p<0x80)
		{
			LCD_ShowChar(x,y,*p,0);
			x+=system_font/2;
			p++;
		}
		else if(*(p+1))
        {
			phz = *p<<8;
			phz += *(p+1);
			LCD_ShowChineseChar(x,y,phz,0);
			x+=system_font;
			p+=2;
		}        
    }
}

//m^n����
//����ֵ:m^n�η�.
uint32_t LCD_Pow(uint8_t m,uint8_t n)
{
	uint32_t result=1;	 
	while(n--)result*=m;    
	return result;
}			 
//��ʾ����,��λΪ0,����ʾ
//x,y :�������	 
//len :���ֵ�λ��
//color:��ɫ 
//num:��ֵ(0~4294967295);	 
void LCD_ShowNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len)
{         	
	uint8_t t,temp;
	uint8_t enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+(system_font/2)*t,y,' ',0);
				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+(system_font/2)*t,y,temp+'0',0); 
	}
} 
//��ʾ����,��λΪ0,������ʾ
//x,y:�������
//num:��ֵ(0~999999999);	 
//len:����(��Ҫ��ʾ��λ��)
//mode:
//[7]:0,�����;1,���0.
//[6:1]:����
//[0]:0,�ǵ�����ʾ;1,������ʾ.
void LCD_ShowxNum(uint16_t x,uint16_t y,uint32_t num,uint8_t len,uint8_t mode)
{  
	uint8_t t,temp;
	uint8_t enshow=0;						   
	for(t=0;t<len;t++)
	{
		temp=(num/LCD_Pow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				if(mode&0X80)LCD_ShowChar(x+(system_font/2)*t,y,'0',mode&0X01);  
				else LCD_ShowChar(x+(system_font/2)*t,y,' ',mode&0X01);  
 				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+(system_font/2)*t,y,temp+'0',mode&0X01); 
	}
} 

//������������ַ����ĳ��Ⱥ͸߶�
//p:�ַ���ָ��
//width,height:���ص��ַ�����Ⱥ͸߶ȱ�����ַ
void LCD_MeasureString(uint8_t *p, uint16_t *width,uint16_t *height)
{
	uint8_t font_size;
	
	*width = 0;
	*height = 0;
	
	if(p == NULL || strlen((const char *)p) == 0)
		return;
	
	(*height) = system_font;
	
	while(*p)
    {       
		if(*p<0x80)
		{
			(*width) += system_font/2;
			p++;
		}
		else if(*(p+1))
        {
			(*width) += system_font;
			p += 2;
		}        
    }  
}

//����ϵͳ����
//font_size:ö�������С
void LCD_SetFontSize(uint8_t font_size)
{
	if(font_size > FONT_SIZE_MIN && font_size < FONT_SIZE_MAX)
		system_font = font_size;
}

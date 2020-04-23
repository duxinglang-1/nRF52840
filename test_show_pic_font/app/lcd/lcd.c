#include "lcd.h"
#include "stdlib.h"
#include "font.h" 
#if defined(LCD_R154101_ST7796S)
#include "LCD_R154101_ST7796S.h"
#elif defined(LCD_LH096TIG11G_ST7735SV)
#include "LCD_LH096TIG11G_ST7735SV.h"
#endif 


//LCD��Ļ�ĸ߶ȺͿ��
uint16_t LCD_WIDTH = COL;
uint16_t LCD_HEIGHT = ROW;

//LCD�Ļ�����ɫ�ͱ���ɫ	   
uint16_t POINT_COLOR=WHITE;	//������ɫ
uint16_t BACK_COLOR=BLACK;  //����ɫ 

//Ĭ�������С
system_font_size system_font = FONT_SIZE_16;


//���ٻ���
//x,y:����
//color:��ɫ
void LCD_Fast_DrawPoint(uint16_t x,uint16_t y,uint16_t color)
{	   
	BlockWrite(x,y,1,1);	//������
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
		BlockWrite(x,i,w,y+h-i);	  
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
		BlockWrite(x,y+i,w,h-i);	  
		for(j=0;j<w;j++)
			WriteOneDot(color[i*w+j]);	//��ʾ��ɫ 
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

//��ȡͼƬ�ߴ�
//color:ͼƬ����ָ��
//width:��ȡ����ͼƬ��������ַ
//height:��ȡ����ͼƬ�߶������ַ
void LCD_get_pic_size(unsigned int *color, uint16_t *width, uint16_t *height)
{
	*width = 255*color[2]+color[3];
	*height = 255*color[4]+color[5];
}

//ָ��λ����ʾͼƬ
//color:ͼƬ����ָ��
//x:ͼƬ��ʾX����
//y:ͼƬ��ʾY����
void LCD_dis_pic(uint16_t x,uint16_t y,unsigned int *color)
{  
	uint16_t h,w;
	uint16_t i,j;
	
	w=255*color[2]+color[3]; 			//��ȡͼƬ���
	h=255*color[4]+color[5];			//��ȡͼƬ�߶�

 	for(i=0;i<h;i++)
	{
		BlockWrite(x,y+i,w,1);	  	//����ˢ��λ��
		for(j=0;j<w;j++)
			WriteOneDot(color[8+i*w+j]);	//��ʾ��ɫ 
	}		  
}

//ָ��λ����ת�Ƕ���ʾͼƬ
//color:ͼƬ����ָ��
//x:ͼƬ��ʾX����
//y:ͼƬ��ʾY����
//rotate:��ת�Ƕ�,0,90,180,270,
void LCD_dis_pic_offset_rotate(uint16_t x,uint16_t y,unsigned int *color,unsigned int rotate)
{
	uint16_t w,h;
	uint16_t i,j;
	
	w=255*color[2]+color[3]; 			//��ȡͼƬ���
	h=255*color[4]+color[5];			//��ȡͼƬ�߶�

	switch(rotate)
	{
	case 0:
		for(i=0;i<h;i++)
		{
			BlockWrite(x,y+i,w,1);	  	//����ˢ��λ��
			for(j=0;j<w;j++)
				WriteOneDot(color[8+i*w+j]);	//��ʾ��ɫ 
		}	
		break;
		
	case 90:
		for(i=0;i<w;i++)
		{
			BlockWrite(x,y+i,h,1);	  	//����ˢ��λ��
			for(j=0;j<h;j++)
				WriteOneDot(color[8+(i+w*(h-1)-j*w)]);	//��ʾ��ɫ 
		}
		break;
		
	case 180:
		for(i=0;i<h;i++)
		{
			BlockWrite(x,y+i,w,1);	  	//����ˢ��λ��
			for(j=0;j<w;j++)
				WriteOneDot(color[8+(w*h-1)-w*i-j]);	//��ʾ��ɫ 
		}		
		break;
		
	case 270:
		for(i=0;i<w;i++)
		{
			BlockWrite(x,y+i,h,1);	  	//����ˢ��λ��
			for(j=0;j<h;j++)
				WriteOneDot(color[8+(w-1-i+w*j)]);	//��ʾ��ɫ 
		}		
		break;
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
			if(y>=LCD_HEIGHT)return;		//��������
			if((y-y0)==system_font)
			{
				y=y0;
				x++;
				if(x>=LCD_WIDTH)return;	//��������
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
			if(y>=LCD_HEIGHT)return;		//��������
			if((y-y0)==system_font)
			{
				y=y0;
				x++;
				if(x>=LCD_WIDTH)return;	//��������
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
    while(*p)
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
        if(x>=LCD_WIDTH)break;//�˳�
        if(y>=LCD_HEIGHT)break;//�˳�
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

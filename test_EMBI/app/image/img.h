#ifndef __IMG_H__
#define __IMG_H__

/*Image2Lcd�����ͼ��������֯��ʽΪ��ͼ��ͷ����-��ɫ������-ͼ�����ݡ�
����ɫ/4��/16��/256ɫ����ͼ������ͷ���£�
typedef struct _HEADGRAY
{
   unsigned char scan;
   unsigned char gray;
   unsigned short w;
   unsigned short h;
}HEADGRAY;

scan: ɨ��ģʽ
Bit7: 0:��������ɨ�裬1:��������ɨ�衣 
Bit6: 0:�Զ�����ɨ�裬1:�Ե�����ɨ�衣 
Bit5: 0:�ֽ����������ݴӸ�λ����λ���У�1:�ֽ����������ݴӵ�λ����λ���С� 
Bit4: 0:WORD���͸ߵ�λ�ֽ�˳����PC��ͬ��1:WORD���͸ߵ�λ�ֽ�˳����PC�෴�� 
Bit3~2: ������ 
Bit1~0: [00]ˮƽɨ�裬[01]��ֱɨ�裬[10]����ˮƽ,�ֽڴ�ֱ��[11]���ݴ�ֱ,�ֽ�ˮƽ�� 

gray: �Ҷ�ֵ 
   �Ҷ�ֵ��1:��ɫ��2:�Ļң�4:ʮ���ң�8:256ɫ��12:4096ɫ��16:16λ��ɫ��24:24λ��ɫ��32:32λ��ɫ��

w: ͼ��Ŀ�ȡ�

h: ͼ��ĸ߶ȡ�

��4096ɫ/16λ���ɫ/18λ���ɫ/24λ���ɫ/32λ���ɫ����ͼ������ͷ���£�
typedef struct _HEADCOLOR
{
   unsigned char scan;
   unsigned char gray;
   unsigned short w;
   unsigned short h;
   unsigned char is565;
   unsigned char rgb;
}HEADCOLOR; 

scan��gray��w��h��HEADGRAY�ṹ�е�ͬ����Ա����������ͬ��

is565: ��4096ɫģʽ��Ϊ0��ʾʹ��[16bits(WORD)]��ʽ����ʱͼ��������ÿ��WORD��ʾһ�����أ�Ϊ1��ʾʹ��[12bits(�����ֽ���)]��ʽ����ʱ�������е�ÿ12Bits����һ�����ء�
��16λ��ɫģʽ��Ϊ0��ʾR G B��ɫ������ռ�õ�λ����Ϊ5Bits��Ϊ1��ʾR G B��ɫ������ռ�õ�λ���ֱ�Ϊ5Bits,6Bits,5Bits��
��18λ��ɫģʽ��Ϊ0��ʾ"6Bits in Low Byte"��Ϊ1��ʾ"6Bits in High Byte"��
��24λ��ɫ��32λ��ɫģʽ��is565��Ч��

rgb: ����R G B��ɫ����������˳��rgb��ÿ2Bits��ʾһ����ɫ������[00]��ʾ�հף�[01]��ʾRed��[10]��ʾGreen��[11]��ʾBlue��

��256ɫ���ĵ�ɫ�����ݽṹ���£�
typedef struct _PALENTRY
{
   unsigned char red;
   unsigned char green;
   unsigned char blue;
}PALENTRY;

typedef struct _PALETTE
{
   unsigned short palnum;
   PALENTRY palentry[palnum];
}PALETTE;

����256ɫģʽ�´��ڵ�ɫ�����ݽṹ,��ɫ�����ݽṹ���������ݽṹHEADGRAY֮��
*/

//extern unsigned int peppa_pig_160X160[];	//С������ͼƬ
//extern unsigned int peppa_pig_80X160[];	//С������ͼƬ
extern unsigned int clock_hour_1_31X31[];
extern unsigned int clock_hour_2_31X31[];
extern unsigned int clock_hour_3_31X31[];
extern unsigned int clock_hour_4_31X31[];
extern unsigned int clock_min_1_31X31[];
extern unsigned int clock_min_2_31X31[];
extern unsigned int clock_min_3_31X31[];
extern unsigned int clock_min_4_31X31[];
extern unsigned int clock_min_5_31X31[];
extern unsigned int clock_min_6_31X31[];
extern unsigned int clock_min_7_31X31[];
extern unsigned int clock_min_8_31X31[];
extern unsigned int clock_min_9_31X31[];
extern unsigned int clock_min_10_31X31[];
extern unsigned int clock_min_11_31X31[];
extern unsigned int clock_min_12_31X31[];
extern unsigned int clock_min_13_31X31[];
extern unsigned int clock_min_14_31X31[];
extern unsigned int clock_min_15_31X31[];
extern unsigned int clock_min_16_31X31[];
extern unsigned int clock_bg_80X160[];

#endif/*__FONT_H__*/

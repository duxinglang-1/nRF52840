#ifndef __FONT_H__
#define __FONT_H__

#define FONT_16
//#define FONT_24
//#define FONT_32

typedef enum
{
	FONT_SIZE_MIN,
#ifdef FONT_16
	FONT_SIZE_16 = 16,
#endif
#ifdef FONT_24
	FONT_SIZE_24 = 24,
#endif
#ifdef FONT_32
	FONT_SIZE_32 = 32,
#endif
	FONT_SIZE_MAX
}system_font_size;

//Ӣ���ֿ�
#ifdef FONT_16
extern const unsigned char asc2_1608[95][16];
#endif
#ifdef FONT_24
extern const unsigned char asc2_2412[95][36];
#endif
#ifdef FONT_32
extern const unsigned char asc2_3216[95][64];
#endif
//�����ֿ�
#ifdef FONT_16
extern const unsigned char chinese_1616[8178][32];
#endif
#ifdef FONT_24
extern const unsigned char chinese_2424[8178][72];
#endif
#ifdef FONT_32
extern const unsigned char chinese_3232[8178][128];
#endif

#endif/*__FONT_H__*/

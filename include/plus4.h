/*
 * plus4.h
 *
 * Ullrich von Bassewitz, 12.08.1998
 */



#ifndef _PLUS4_H
#define _PLUS4_H



/* Additional key defines */
#define CH_F1			133
#define CH_F2			137
#define CH_F3			134
#define CH_F4			138
#define CH_F5			135
#define CH_F6			139
#define CH_F7			136
#define CH_F8			140



/* Color attributes */
#define CATTR_LUMA0		0x00
#define CATTR_LUMA1		0x10
#define CATTR_LUMA2		0x20
#define CATTR_LUMA3		0x30
#define CATTR_LUMA4		0x40
#define CATTR_LUMA5		0x50
#define CATTR_LUMA6		0x60
#define CATTR_LUMA7		0x70
#define CATTR_BLINK		0x80

/* Base colors */
#define BCOLOR_BLACK		0x00
#define BCOLOR_WHITE		0x01
#define BCOLOR_RED		0x02
#define BCOLOR_CYAN    	       	0x03
#define BCOLOR_VIOLET		0x04
#define BCOLOR_GREEN		0x05
#define BCOLOR_BLUE		0x06
#define BCOLOR_YELLOW		0x07
#define BCOLOR_ORANGE		0x08
#define BCOLOR_BROWN		0x09
#define BCOLOR_LEMON		0x0A   	/* What's that color? */
#define BCOLOR_LIGHTVIOLET	0x0B
#define BCOLOR_BLUEGREEN	0x0C
#define BCOLOR_LIGHTBLUE	0x0D
#define BCOLOR_DARKBLUE		0x0E
#define BCOLOR_LIGHTGREEN	0x0F



/* Now try to mix up a C64/C128 compatible palette */
#define COLOR_BLACK  	       	(BCOLOR_BLACK)
#define COLOR_WHITE  	       	(BCOLOR_WHITE | CATTR_LUMA7)
#define COLOR_RED    	       	(BCOLOR_RED | CATTR_LUMA4)
#define COLOR_CYAN        	(BCOLOR_CYAN | CATTR_LUMA7)
#define COLOR_VIOLET 	       	(BCOLOR_VIOLET | CATTR_LUMA7)
#define COLOR_GREEN  	       	(BCOLOR_GREEN | CATTR_LUMA7)
#define COLOR_BLUE   	       	(BCOLOR_BLUE | CATTR_LUMA7)
#define COLOR_YELLOW 	       	(BCOLOR_YELLOW | CATTR_LUMA7)
#define COLOR_ORANGE 	       	(BCOLOR_ORANGE | CATTR_LUMA7)
#define COLOR_BROWN  	       	(BCOLOR_BROWN | CATTR_LUMA7)
#define COLOR_LIGHTRED       	(BCOLOR_RED | CATTR_LUMA7)
#define COLOR_GRAY1  	       	(BCOLOR_WHITE | CATTR_LUMA1)
#define COLOR_GRAY2  	       	(BCOLOR_WHITE | CATTR_LUMA3)
#define COLOR_LIGHTGREEN     	(BCOLOR_LIGHTGREEN | CATTR_LUMA7)
#define COLOR_LIGHTBLUE      	(BCOLOR_LIGHTBLUE | CATTR_LUMA7)
#define COLOR_GRAY3  	       	(BCOLOR_WHITE | CATTR_LUMA5)



/* End of plus4.h */
#endif




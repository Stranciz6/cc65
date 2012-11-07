/*****************************************************************************/
/*                                                                           */
/*                                  lynx.h                                   */
/*                                                                           */
/*                     Lynx system specific definitions                      */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2003      Shawn Jefferson                                             */
/*                                                                           */
/* Adapted with many changes Ullrich von Bassewitz, 2004-10-09               */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



#ifndef _LYNX_H
#define _LYNX_H



/* Check for errors */
#if !defined(__LYNX__)
#  error This module may only be used when compiling for the Lynx game console!
#endif



/*****************************************************************************/
/*	       	   	  	     Data				     */
/*****************************************************************************/



/* Color defines */
#define COLOR_BLACK             0x00
#define COLOR_RED               0x01
#define COLOR_PINK              0x02
#define COLOR_LIGHTGREY         0x03
#define COLOR_GREY              0x04
#define COLOR_DARKGREY          0x05
#define COLOR_BROWN             0x06
#define COLOR_PEACH             0x07
#define COLOR_YELLOW            0x08
#define COLOR_LIGHTGREEN        0x09
#define COLOR_GREEN             0x0A
#define COLOR_DARKBROWN         0x0B
#define COLOR_VIOLET            0x0C
#define COLOR_BLUE              0x0D
#define COLOR_LIGHTBLUE         0x0E
#define COLOR_WHITE             0x0F

/* TGI color defines (default palette) */
#define TGI_COLOR_BLACK         COLOR_BLACK
#define TGI_COLOR_RED           COLOR_RED
#define TGI_COLOR_PINK          COLOR_PINK
#define TGI_COLOR_LIGHTGREY     COLOR_LIGHTGREY
#define TGI_COLOR_GREY          COLOR_GREY
#define TGI_COLOR_DARKGREY      COLOR_DARKGREY
#define TGI_COLOR_BROWN         COLOR_BROWN
#define TGI_COLOR_PEACH         COLOR_PEACH
#define TGI_COLOR_YELLOW        COLOR_YELLOW
#define TGI_COLOR_LIGHTGREEN    COLOR_LIGHTGREEN
#define TGI_COLOR_GREEN         COLOR_GREEN
#define TGI_COLOR_DARKBROWN     COLOR_DARKBROWN
#define TGI_COLOR_VIOLET        COLOR_VIOLET
#define TGI_COLOR_BLUE          COLOR_BLUE
#define TGI_COLOR_LIGHTBLUE     COLOR_LIGHTBLUE
#define TGI_COLOR_WHITE         COLOR_WHITE

/* No support for dynamically loadable drivers */
#define DYN_DRV 0



/*****************************************************************************/
/*                                 Variables                                 */
/*****************************************************************************/



/* The addresses of the static drivers */
extern void lynx_stdjoy[];        /* Referred to by joy_static_stddrv[] */
extern void lynx_comlynx[];
extern void lynx_160_102_16[];    /* Referred to by tgi_static_stddrv[] */



/*****************************************************************************/
/*                           Accessing the cart                              */
/*****************************************************************************/



void __fastcall__ lynx_load (int fileno);
/* Load a file into ram. The first entry is fileno=0. */

void __fastcall__ lynx_exec (int fileno);
/* Load a file into ram and execute it. */



/*****************************************************************************/
/*                           Accessing the EEPROM                            */
/*****************************************************************************/



unsigned __fastcall__ lynx_eeprom_read (unsigned char cell);
/* Read a 16 bit word from the given address */

unsigned __fastcall__ lynx_eeprom_write (unsigned char cell, unsigned val);
/* Write the word at the given address */

void __fastcall__ lynx_eeprom_erase (unsigned char cell);
/* Clear the word at the given address */

unsigned __fastcall__ lynx_eeread (unsigned cell);
/* Read a 16 bit word from the given address 93C46 93C66 or 93C86*/

unsigned __fastcall__ lynx_eewrite (unsigned cell, unsigned val);
/* Write the word at the given address 93C46 93C66 or 93C86*/



/*****************************************************************************/
/*                           TGI extras                                      */
/*****************************************************************************/



#define tgi_sprite(spr) tgi_ioctl(0, spr)
#define tgi_flip() tgi_ioctl(1, (void*)0)
#define tgi_setbgcolor(bgcol) tgi_ioctl(2, (void*)(bgcol))
#define tgi_setframerate(rate) tgi_ioctl(3, (void*)(rate))
#define tgi_busy() tgi_ioctl(4, (void*)0)
#define tgi_updatedisplay() tgi_ioctl(4, (void*)1)
#define tgi_setcollisiondetection(active) tgi_ioctl(5, (void*)(active))



/*****************************************************************************/
/*                           Sprites                                         */
/*****************************************************************************/



#define SPRCTL0_1_bit_per_pixel 0
#define SPRCTL0_2_bits_per_pixel 0x40
#define SPRCTL0_3_bits_per_pixel 0x80
#define SPRCTL0_4_bits_per_pixel 0xc0
#define SPRCTL0_h_flip 0x20
#define SPRCTL0_v_flip 0x10
#define SPRCTL0_shadow 7
#define SPRCTL0_xor_shadow 6
#define SPRCTL0_non_collide 5
#define SPRCTL0_normal 4
#define SPRCTL0_boundary 3
#define SPRCTL0_boundary_shadow 2
#define SPRCTL0_background_no_coll 1
#define SPRCTL0_background_shadow 0

#define SPRCTL1_literal 0x80
#define SPRCTL1_hsize_vsize_strech_tilt 0x30
#define SPRCTL1_hsize_vsize_strech 0x20
#define SPRCTL1_hsize_vsize 0x10
#define SPRCTL1_no_palette 0x08
#define SPRCTL1_skip_sprite 0x04
#define SPRCTL1_start_drawing_up 0x02
#define SPRCTL1_start_drawing_left 0x01

#define SPRCOLL_dont_collide 0x20
#define SPRCOLL_number_0_3 0x00


/* Define Hardware */
#include <_mikey.h>
#define MIKEY (*(struct __mikey *)0xFD00)

#define _MIKEY_TIMERS (*(struct _mikey_all_timers *) 0xFD00)  // mikey_timers[8]
#define _HBL_TIMER (*(struct _mikey_timer *) 0xFD00)          // timer0 (HBL)
#define _VBL_TIMER (*(struct _mikey_timer *) 0xFD08)          // timer2 (VBL)
#define _UART_TIMER (*(struct _mikey_timer *) 0xFD14)         // timer4 (UART)
#define _VIDDMA (*(unsigned int *) 0xFD92)                    // dispctl/viddma

#include <_suzy.h>
#define SUZY        (*(struct __suzy*)0xFC00)


/* End of lynx.h */
#endif

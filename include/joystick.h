/*****************************************************************************/
/*                                                                           */
/*				  joystick.h				     */
/*                                                                           */
/*		 Read the joystick on systems that support it		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2002 Ullrich von Bassewitz                                       */
/*               Wacholderweg 14                                             */
/*               D-70597 Stuttgart                                           */
/* EMail:        uz@musoftware.de                                            */
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



#ifndef _JOYSTICK_H
#define _JOYSTICK_H



/*****************************************************************************/
/*                                  Definitions                              */
/*****************************************************************************/



/* Error codes */
#define JOY_ERR_OK              0       /* No error */
#define JOY_ERR_NO_DRIVER       1       /* No driver available */
#define JOY_ERR_CANNOT_LOAD     2       /* Error loading driver */
#define JOY_ERR_INV_DRIVER      3       /* Invalid driver */
#define JOY_ERR_NO_DEVICE       4       /* Device (hardware) not found */

/* Argument for the joy_read function */
#define JOY_1		0
#define JOY_2		1

/* The following codes are *indices* into the joy_masks array */
#define JOY_UP          0
#define JOY_DOWN        1
#define JOY_LEFT        2
#define JOY_RIGHT       3
#define JOY_FIRE        4

/* Array of masks used to check the return value of joy_read for a state */
extern const unsigned char joy_masks[8];



/* Result codes of the function. The actual code is a bitwise or
 * of one or more of the following values.
 */
#if 0
#if defined(__VIC20__)
#  define JOY_UP        0x02
#  define JOY_DOWN      0x04
#  define JOY_LEFT      0x08
#  define JOY_RIGHT     0x80
#  define JOY_FIRE      0x10
#else
#  define JOY_UP       	0x01
#  define JOY_DOWN	0x02
#  define JOY_LEFT	0x04
#  define JOY_RIGHT	0x08
#  define JOY_FIRE	0x10
#endif
#endif



/*****************************************************************************/
/* 	      	       	      	   Functions	       			     */
/*****************************************************************************/



unsigned char __fastcall__ joy_load_driver (const char* driver);
/* Load a joystick driver and return an error code */

unsigned char __fastcall__ joy_unload (void);
/* Unload the currently loaded driver. */

unsigned char __fastcall__ joy_count (void);
/* Return the number of joysticks supported by the driver */

unsigned char __fastcall__ joy_read (unsigned char joystick);
/* Read a particular joystick */



/* End of joystick.h */
#endif




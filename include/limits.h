/*****************************************************************************/
/*                                                                           */
/*				   limits.h				     */
/*                                                                           */
/*			    Sizes of integer types			     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2000 Ullrich von Bassewitz                                       */
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



#ifndef _LIMITS_H
#define _LIMITS_H



#define CHAR_BIT	8

#define SCHAR_MIN	(-128)
#define SCHAR_MAX	127

#define UCHAR_MAX	255

#define CHAR_MIN	0
#define CHAR_MAX	255

#define SHRT_MIN	(-32768)
#define SHRT_MAX	32767

#define USHRT_MAX	65535U

#define INT_MIN		(-32768)
#define INT_MAX		32767

#define UINT_MAX	65535U

#define LONG_MAX	2147483647L
#define LONG_MIN	(-2147483648L)

#define ULONG_MAX	4294967295UL



/* End of limits.h */
#endif




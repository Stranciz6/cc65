/*****************************************************************************/
/*                                                                           */
/*				     cbm.h			       	     */
/*                                                                           */
/*		        CBM system specific definitions			     */
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



#ifndef _CBM_H
#define _CBM_H



/* Load the system specific files here, if needed */
#ifdef __C64__
#ifndef _C64_H
#include <c64.h>
#endif
#endif

#ifdef __C128__
#ifndef _C128_H
#include <c128.h>
#endif
#endif

#ifdef __PLUS4__
#ifndef _PLUS4_H
#include <plus4.h>
#endif
#endif

#ifdef __CBM610__
#ifndef _CBM610_H
#include <cbm610.h>
#endif
#endif

#ifdef __PET__
#ifndef _PET_H
#include <pet.h>
#endif
#endif



/* Characters codes (CBM charset) */
#define CH_HLINE    		 96
#define CH_VLINE    		125
#define	CH_ULCORNER 		176
#define CH_URCORNER 		174
#define CH_LLCORNER 		173
#define CH_LRCORNER 		189
#define CH_TTEE	    		178
#define CH_RTEE	    		179
#define CH_BTEE	    		177
#define CH_LTEE	    		171
#define CH_CROSS    		123
#define CH_CURS_UP		145
#define CH_CURS_DOWN		 17
#define CH_CURS_LEFT		157
#define CH_CURS_RIGHT		 29
#define CH_PI			126
#define CH_DEL			 20
#define CH_INS			148
#define CH_ESC			 95



/* Kernel level functions */
void __fastcall__ cbm_k_setlfs (unsigned char LFN, unsigned char DEV,
                                unsigned char SA);
void __fastcall__ cbm_k_setnam (const char* Name);
unsigned char __fastcall__ cbm_k_load(unsigned char flag, unsigned addr);
unsigned char __fastcall__ cbm_k_save(unsigned int start, unsigned int end);
unsigned char __fastcall__ cbm_k_open (void);
void __fastcall__ cbm_k_close (unsigned char FN);
unsigned char __fastcall__ cbm_k_readst (void);
unsigned char __fastcall__ cbm_k_chkin (unsigned char FN);
unsigned char __fastcall__ cbm_k_basin (void);
void __fastcall__ cbm_k_clrch (void);

/* BASIC-like functions */
unsigned char cbm_load(const char* name, unsigned char device, unsigned int addr);
/* Loads file "name" from given device to given address or to the load address
 * of the file if addr is 0 (like load"name",8,1 in BASIC)
 * Returns 0 if loading was successful otherwise an errorcode (see table below).
 */

unsigned char cbm_save(const char* name, unsigned char device,
                      unsigned int start, unsigned int end);
/* Saves a memory area from start to end-1 to a file.
 * Returns 0 if saving was successful, otherwise an errorcode (see table below).
 */

unsigned char cbm_open(unsigned char lfn, unsigned char device,
                       unsigned char sec_addr, const char* name);
/* Opens a file. Works just like the BASIC command.
 * Returns 0 if opening was successful, otherwise an errorcode (see table below).
 */

void __fastcall__ cbm_close (unsigned char lfn);
/* Closes a file */

int cbm_read(unsigned char lfn, void* buffer, unsigned int size);

/* Errorcodes of load, save, open functions:
 *
 * errorcode	BASIC error
 *	1   =	too many files
 *	2   =	file open
 *	3   =	file not open
 *	4   =	file not found
 *	5   =	device not present
 *	6   =	not input file
 *	7   =	not output file
 *	8   =	missing filename
 *	9   =	illegal device number
 */

/* End of cbm.h */
#endif

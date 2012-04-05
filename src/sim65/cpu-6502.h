/*****************************************************************************/
/*                                                                           */
/*                                cpu-6502.h                                 */
/*                                                                           */
/*                        CPU core for the 6502 simulator                    */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2002-2012, Ullrich von Bassewitz                                      */
/*                Roemerstrasse 52                                           */
/*                D-70794 Filderstadt                                        */
/* EMail:         uz@cc65.org                                                */
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



#ifndef CPU_6502_H
#define CPU_6502_H



/* sim65 */
#include "cpuregs.h"



/*****************************************************************************/
/*  	   			     Data				     */
/*****************************************************************************/



/* Registers */
extern CPURegs Regs;



/*****************************************************************************/
/*  			   	     Code				     */
/*****************************************************************************/



void CPUInit (void);
/* Initialize the CPU */

void RESET (void);
/* Generate a CPU RESET */

void IRQRequest (void);
/* Generate an IRQ */

void NMIRequest (void);
/* Generate an NMI */

void Break (const char* Format, ...);
/* Stop running and display the given message */

void CPURun (void);
/* Run one CPU instruction */



/* End of cpu-6502.h */

#endif




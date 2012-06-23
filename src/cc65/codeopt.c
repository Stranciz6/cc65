/*****************************************************************************/
/*                                                                           */
/*				   codeopt.c				     */
/*                                                                           */
/*			     Optimizer subroutines			     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2001-2012, Ullrich von Bassewitz                                      */
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



#include <stdlib.h>
#include <string.h>

/* common */
#include "abend.h"
#include "chartype.h"
#include "cpu.h"
#include "debugflag.h"
#include "print.h"
#include "xmalloc.h"
#include "xsprintf.h"

/* cc65 */
#include "asmlabel.h"
#include "codeent.h"
#include "codeinfo.h"
#include "coptadd.h"
#include "coptc02.h"
#include "coptcmp.h"
#include "coptind.h"
#include "coptneg.h"
#include "coptptrload.h"
#include "coptptrstore.h"
#include "coptpush.h"
#include "coptsize.h"
#include "coptstop.h"
#include "coptstore.h"
#include "coptsub.h"
#include "copttest.h"
#include "error.h"
#include "global.h"
#include "codeopt.h"



/*****************************************************************************/
/*                                     Data                                  */
/*****************************************************************************/



/* Shift types */
enum {
    SHIFT_NONE,
    SHIFT_ASR_1,
    SHIFT_ASL_1,
    SHIFT_LSR_1,
    SHIFT_LSL_1
};



/*****************************************************************************/
/*				Optimize shifts                              */
/*****************************************************************************/



static unsigned OptShift1 (CodeSeg* S)
/* A call to the shlaxN routine may get replaced by one or more asl insns
 * if the value of X is not used later. If X is used later, but it is zero
 * on entry and it's a shift by one, it may get replaced by:
 *
 *      asl     a
 *      bcc     L1
 *      inx
 *  L1:
 *
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        CodeEntry* N;
        CodeEntry* X;
        CodeLabel* L;

      	/* Get next entry */
       	CodeEntry* E = CS_GetEntry (S, I);

     	/* Check for the sequence */
	if (E->OPC == OP65_JSR                       &&
       	    (strncmp (E->Arg, "shlax", 5) == 0 ||
	     strncmp (E->Arg, "aslax", 5) == 0)	     &&
	    strlen (E->Arg) == 6                     &&
	    IsDigit (E->Arg[5])) {

            if (!RegXUsed (S, I+1)) {

                /* Insert shift insns */
                unsigned Count = E->Arg[5] - '0';
                while (Count--) {
                    X = NewCodeEntry (OP65_ASL, AM65_ACC, "a", 0, E->LI);
                    CS_InsertEntry (S, X, I+1);
                }

                /* Delete the call to shlax */
                CS_DelEntry (S, I);

                /* Remember, we had changes */
                ++Changes;

            } else if (E->RI->In.RegX == 0              &&
                       E->Arg[5] == '1'                 &&
                       (N = CS_GetNextEntry (S, I)) != 0) {

                /* asl a */
                X = NewCodeEntry (OP65_ASL, AM65_ACC, "a", 0, E->LI);
                CS_InsertEntry (S, X, I+1);

                /* bcc L1 */
                L = CS_GenLabel (S, N);
                X = NewCodeEntry (OP65_BCC, AM65_BRA, L->Name, L, E->LI);
                CS_InsertEntry (S, X, I+2);

                /* inx */
                X = NewCodeEntry (OP65_INX, AM65_IMP, 0, 0, E->LI);
                CS_InsertEntry (S, X, I+3);

                /* Delete the call to shlax */
                CS_DelEntry (S, I);

                /* Remember, we had changes */
                ++Changes;
            }

	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned OptShift2(CodeSeg* S)
/* A call to the asrax1 routines may get replaced by something simpler, if
 * X is not used later:
 *
 *      cmp     #$80
 *      ror     a
 *
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        unsigned Count;

      	/* Get next entry */
       	CodeEntry* E = CS_GetEntry (S, I);

     	/* Check for the sequence */
	if (E->OPC == OP65_JSR                  &&
       	    strncmp (E->Arg, "asrax", 5) == 0   &&
	    strlen (E->Arg) == 6                &&
	    IsDigit (E->Arg[5])                 &&
            (Count = (E->Arg[5] - '0')) >= 1    &&
            Count * 100 <= S->CodeSizeFactor    &&
            !RegXUsed (S, I+1)) {

            CodeEntry* X;
            unsigned J = I+1;

            /* Generate the replacement sequence */
            while (Count--) {
                /* cmp #$80 */
                X = NewCodeEntry (OP65_CMP, AM65_IMM, "$80", 0, E->LI);
                CS_InsertEntry (S, X, J++);

                /* ror a */
                X = NewCodeEntry (OP65_ROR, AM65_ACC, "a", 0, E->LI);
                CS_InsertEntry (S, X, J++);
            }

            /* Delete the call to asrax */
            CS_DelEntry (S, I);

            /* Remember, we had changes */
            ++Changes;
    	}

    	/* Next entry */
    	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned OptShift3 (CodeSeg* S)
/* The sequence
 *
 *      bcc     L
 *  	inx
 * L:   jsr     shrax1
 *
 * may get replaced by
 *
 *      ror     a
 *
 * if X is zero on entry and unused later. For shift counts > 1, more
 *
 *      shr     a
 *
 * must be added.
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

	CodeEntry* L[3];

      	/* Get next entry */
       	L[0] = CS_GetEntry (S, I);

     	/* Check for the sequence */
       	if ((L[0]->OPC == OP65_BCC || L[0]->OPC == OP65_JCC)    &&
	    L[0]->JumpTo != 0                                   &&
            L[0]->RI->In.RegX == 0                              &&
       	    CS_GetEntries (S, L+1, I+1, 2)                      &&
	    L[1]->OPC == OP65_INX            	       	        &&
	    L[0]->JumpTo->Owner == L[2]                         &&
	    !CS_RangeHasLabel (S, I, 2)                         &&
            L[2]->OPC == OP65_JSR                               &&
            strlen (L[2]->Arg) == 6                             &&
            memcmp (L[2]->Arg, "shrax", 5) == 0                 &&
            IsDigit (L[2]->Arg[5])                              &&
	    !RegXUsed (S, I+3)) {

            unsigned ShiftCount = (L[2]->Arg[5] - '0');

            /* Add the replacement insn instead */
            CodeEntry* X = NewCodeEntry (OP65_ROR, AM65_ACC, "a", 0, L[2]->LI);
            CS_InsertEntry (S, X, I+3);
            while (--ShiftCount) {
                X = NewCodeEntry (OP65_LSR, AM65_ACC, "a", 0, L[2]->LI);
                CS_InsertEntry (S, X, I+4);
            }

	    /* Remove the bcs/dex/jsr */
	    CS_DelEntries (S, I, 3);

	    /* Remember, we had changes */
	    ++Changes;

	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned OptShift4 (CodeSeg* S)
/* Calls to the asraxN or shraxN routines may get replaced by one or more lsr
 * insns if the value of X is zero.
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

      	/* Get next entry */
       	CodeEntry* E = CS_GetEntry (S, I);

     	/* Check for the sequence */
	if (E->OPC == OP65_JSR                          &&
       	    (strncmp (E->Arg, "shrax", 5) == 0  ||
             strncmp (E->Arg, "asrax", 5) == 0)         &&
	    strlen (E->Arg) == 6                        &&
	    IsDigit (E->Arg[5])                         &&
       	    E->RI->In.RegX == 0) {

	    /* Insert shift insns */
	    unsigned Count = E->Arg[5] - '0';
	    while (Count--) {
	    	CodeEntry* X = NewCodeEntry (OP65_LSR, AM65_ACC, "a", 0, E->LI);
    	    	CS_InsertEntry (S, X, I+1);
	    }

	    /* Delete the call to shrax */
	    CS_DelEntry (S, I);

	    /* Remember, we had changes */
	    ++Changes;

	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned GetShiftType (const char* Sub)
/* Helper function for OptShift5 */
{
    if (*Sub == 'a') {
        if (strcmp (Sub+1, "slax1") == 0) {
            return SHIFT_ASL_1;
        } else if (strcmp (Sub+1, "srax1") == 0) {
            return SHIFT_ASR_1;
        }
    } else if (*Sub == 's') {
        if (strcmp (Sub+1, "hlax1") == 0) {
            return SHIFT_LSL_1;
        } else if (strcmp (Sub+1, "hrax1") == 0) {
            return SHIFT_LSR_1;
        }
    }
    return SHIFT_NONE;
}



static unsigned OptShift5 (CodeSeg* S)
/* Search for the sequence
 *
 *      lda     xxx
 *      ldx     yyy
 *      jsr     aslax1/asrax1/shlax1/shrax1
 *      sta     aaa
 *      stx     bbb
 *
 * and replace it by
 *
 *      lda     xxx
 *      asl     a
 *      sta     aaa
 *      lda     yyy
 *      rol     a
 *      sta     bbb
 *
 * or similar, provided that a/x is not used later
 */
{
    unsigned Changes = 0;

    /* Walk over the entries */
    unsigned I = 0;
    while (I < CS_GetEntryCount (S)) {

        unsigned ShiftType;
     	CodeEntry* L[5];

      	/* Get next entry */
       	L[0] = CS_GetEntry (S, I);

     	/* Check for the sequence */
       	if (L[0]->OPC == OP65_LDA                               &&
            (L[0]->AM == AM65_ABS || L[0]->AM == AM65_ZP)       &&
       	    CS_GetEntries (S, L+1, I+1, 4)     	                &&
            !CS_RangeHasLabel (S, I+1, 4)                       &&
            L[1]->OPC == OP65_LDX                               &&
            (L[1]->AM == AM65_ABS || L[1]->AM == AM65_ZP)       &&
            L[2]->OPC == OP65_JSR                               &&
            (ShiftType = GetShiftType (L[2]->Arg)) != SHIFT_NONE&&
       	    L[3]->OPC == OP65_STA                               &&
            (L[3]->AM == AM65_ABS || L[3]->AM == AM65_ZP)       &&
            L[4]->OPC == OP65_STX                               &&
            (L[4]->AM == AM65_ABS || L[4]->AM == AM65_ZP)       &&
            !RegAXUsed (S, I+5)) {

            CodeEntry* X;

            /* Handle the four shift types differently */
            switch (ShiftType) {

                case SHIFT_ASR_1:
                    X = NewCodeEntry (OP65_LDA, L[1]->AM, L[1]->Arg, 0, L[1]->LI);
                    CS_InsertEntry (S, X, I+5);
                    X = NewCodeEntry (OP65_CMP, AM65_IMM, "$80", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+6);
                    X = NewCodeEntry (OP65_ROR, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+7);
                    X = NewCodeEntry (OP65_STA, L[4]->AM, L[4]->Arg, 0, L[4]->LI);
                    CS_InsertEntry (S, X, I+8);
                    X = NewCodeEntry (OP65_LDA, L[0]->AM, L[0]->Arg, 0, L[0]->LI);
                    CS_InsertEntry (S, X, I+9);
                    X = NewCodeEntry (OP65_ROR, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+10);
                    X = NewCodeEntry (OP65_STA, L[3]->AM, L[3]->Arg, 0, L[3]->LI);
                    CS_InsertEntry (S, X, I+11);
                    CS_DelEntries (S, I, 5);
                    break;

                case SHIFT_LSR_1:
                    X = NewCodeEntry (OP65_LDA, L[1]->AM, L[1]->Arg, 0, L[1]->LI);
                    CS_InsertEntry (S, X, I+5);
                    X = NewCodeEntry (OP65_LSR, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+6);
                    X = NewCodeEntry (OP65_STA, L[4]->AM, L[4]->Arg, 0, L[4]->LI);
                    CS_InsertEntry (S, X, I+7);
                    X = NewCodeEntry (OP65_LDA, L[0]->AM, L[0]->Arg, 0, L[0]->LI);
                    CS_InsertEntry (S, X, I+8);
                    X = NewCodeEntry (OP65_ROR, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+9);
                    X = NewCodeEntry (OP65_STA, L[3]->AM, L[3]->Arg, 0, L[3]->LI);
                    CS_InsertEntry (S, X, I+10);
                    CS_DelEntries (S, I, 5);
                    break;

                case SHIFT_LSL_1:
                case SHIFT_ASL_1:
                    /* These two are identical */
                    X = NewCodeEntry (OP65_ASL, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+1);
                    X = NewCodeEntry (OP65_STA, L[3]->AM, L[3]->Arg, 0, L[3]->LI);
                    CS_InsertEntry (S, X, I+2);
                    X = NewCodeEntry (OP65_LDA, L[1]->AM, L[1]->Arg, 0, L[1]->LI);
                    CS_InsertEntry (S, X, I+3);
                    X = NewCodeEntry (OP65_ROL, AM65_ACC, "a", 0, L[2]->LI);
                    CS_InsertEntry (S, X, I+4);
                    X = NewCodeEntry (OP65_STA, L[4]->AM, L[4]->Arg, 0, L[4]->LI);
                    CS_InsertEntry (S, X, I+5);
                    CS_DelEntries (S, I+6, 4);
                    break;

            }

	    /* Remember, we had changes */
            ++Changes;

	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned OptShift6 (CodeSeg* S)
/* Inline the shift subroutines. */
{
    unsigned Changes = 0;

    /* Walk over the entries */
    unsigned I = 0;
    while (I < CS_GetEntryCount (S)) {

        CodeEntry* X;
        unsigned   IP;

      	/* Get next entry */
     	CodeEntry* E = CS_GetEntry (S, I);

     	/* Check for a call to one of the shift routine */
	if (E->OPC == OP65_JSR                          &&
       	    (strncmp (E->Arg, "shlax", 5) == 0  ||
             strncmp (E->Arg, "aslax", 5) == 0)         &&
	    strlen (E->Arg) == 6                        &&
	    IsDigit (E->Arg[5])) {

            /* Get number of shifts */
            unsigned ShiftCount = (E->Arg[5] - '0');

            /* Code is:
             *
             *      stx     tmp1
             *      asl     a
             *      rol     tmp1
             *      (repeat ShiftCount-1 times)
             *      ldx     tmp1
             *
             * which makes 4 + 3 * ShiftCount bytes, compared to the original
             * 3 bytes for the subroutine call. However, in most cases, the
             * final load of the X register gets merged with some other insn
             * and replaces a txa, so for a shift count of 1, we get a factor
             * of 200, which matches nicely the CodeSizeFactor enabled with -Oi
             */
            if (ShiftCount > 1 || S->CodeSizeFactor > 200) {
                unsigned Size = 4 + 3 * ShiftCount;
                if ((Size * 100 / 3) > S->CodeSizeFactor) {
                    /* Not acceptable */
                    goto NextEntry;
                }
            }

            /* Inline the code. Insertion point is behind the subroutine call */
            IP = (I + 1);

            /* stx tmp1 */
            X = NewCodeEntry (OP65_STX, AM65_ZP, "tmp1", 0, E->LI);
            CS_InsertEntry (S, X, IP++);

            while (ShiftCount--) {
                /* asl a */
                X = NewCodeEntry (OP65_ASL, AM65_ACC, "a", 0, E->LI);
                CS_InsertEntry (S, X, IP++);

                /* rol tmp1 */
                X = NewCodeEntry (OP65_ROL, AM65_ZP, "tmp1", 0, E->LI);
                CS_InsertEntry (S, X, IP++);
            }

            /* ldx tmp1 */
            X = NewCodeEntry (OP65_LDX, AM65_ZP, "tmp1", 0, E->LI);
            CS_InsertEntry (S, X, IP++);

            /* Remove the subroutine call */
            CS_DelEntry (S, I);

	    /* Remember, we had changes */
            ++Changes;
	}

NextEntry:
	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



/*****************************************************************************/
/*                              Optimize loads                               */
/*****************************************************************************/



static unsigned OptLoad1 (CodeSeg* S)
/* Search for a call to ldaxysp where X is not used later and replace it by
 * a load of just the A register.
 */
{
    unsigned I;
    unsigned Changes = 0;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

     	CodeEntry* E;

      	/* Get next entry */
       	E = CS_GetEntry (S, I);

     	/* Check for the sequence */
       	if (CE_IsCallTo (E, "ldaxysp")          &&
            RegValIsKnown (E->RI->In.RegY)      &&
            !RegXUsed (S, I+1)) {

            CodeEntry* X;

            /* Reload the Y register */
            const char* Arg = MakeHexArg (E->RI->In.RegY - 1);
            X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
            CS_InsertEntry (S, X, I+1);

            /* Load from stack */
            X = NewCodeEntry (OP65_LDA, AM65_ZP_INDY, "sp", 0, E->LI);
            CS_InsertEntry (S, X, I+2);

            /* Now remove the call to the subroutine */
	    CS_DelEntry (S, I);

	    /* Remember, we had changes */
            ++Changes;

	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



static unsigned OptLoad2 (CodeSeg* S)
/* Replace calls to ldaxysp by inline code */
{
    unsigned I;
    unsigned Changes = 0;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

     	CodeEntry* L[3];

      	/* Get next entry */
       	L[0] = CS_GetEntry (S, I);

     	/* Check for the sequence */
       	if (CE_IsCallTo (L[0], "ldaxysp")) {

            CodeEntry* X;

            /* Followed by sta abs/stx abs? */
            if (CS_GetEntries (S, L+1, I+1, 2)                  &&
                L[1]->OPC == OP65_STA                           &&
                L[2]->OPC == OP65_STX                           &&
                (L[1]->Arg == 0                         ||
                 L[2]->Arg == 0                         ||
                 strcmp (L[1]->Arg, L[2]->Arg) != 0)            &&
                !CS_RangeHasLabel (S, I+1, 2)                   &&
                !RegXUsed (S, I+3)) {

                /* A/X are stored into memory somewhere and X is not used
                 * later
                 */

                /* lda (sp),y */
                X = NewCodeEntry (OP65_LDA, AM65_ZP_INDY, "sp", 0, L[0]->LI);
                CS_InsertEntry (S, X, I+3);

                /* sta abs */
                X = NewCodeEntry (OP65_STA, L[2]->AM, L[2]->Arg, 0, L[2]->LI);
                CS_InsertEntry (S, X, I+4);

                /* dey */
                X = NewCodeEntry (OP65_DEY, AM65_IMP, 0, 0, L[0]->LI);
                CS_InsertEntry (S, X, I+5);

                /* lda (sp),y */
                X = NewCodeEntry (OP65_LDA, AM65_ZP_INDY, "sp", 0, L[0]->LI);
                CS_InsertEntry (S, X, I+6);

                /* sta abs */
                X = NewCodeEntry (OP65_STA, L[1]->AM, L[1]->Arg, 0, L[1]->LI);
                CS_InsertEntry (S, X, I+7);

                /* Now remove the call to the subroutine and the sta/stx */
                CS_DelEntries (S, I, 3);

            } else {

                /* Standard replacement */

                /* lda (sp),y */
                X = NewCodeEntry (OP65_LDA, AM65_ZP_INDY, "sp", 0, L[0]->LI);
                CS_InsertEntry (S, X, I+1);

                /* tax */
                X = NewCodeEntry (OP65_TAX, AM65_IMP, 0, 0, L[0]->LI);
                CS_InsertEntry (S, X, I+2);

                /* dey */
                X = NewCodeEntry (OP65_DEY, AM65_IMP, 0, 0, L[0]->LI);
                CS_InsertEntry (S, X, I+3);

                /* lda (sp),y */
                X = NewCodeEntry (OP65_LDA, AM65_ZP_INDY, "sp", 0, L[0]->LI);
                CS_InsertEntry (S, X, I+4);

                /* Now remove the call to the subroutine */
                CS_DelEntry (S, I);

            }

	    /* Remember, we had changes */
            ++Changes;

	}

	/* Next entry */
	++I;
    }

    /* Return the number of changes made */
    return Changes;
}



/*****************************************************************************/
/*			      Decouple operations                            */
/*****************************************************************************/



static unsigned OptDecouple (CodeSeg* S)
/* Decouple operations, that is, do the following replacements:
 *
 *   dex        -> ldx #imm
 *   inx        -> ldx #imm
 *   dey        -> ldy #imm
 *   iny        -> ldy #imm
 *   tax        -> ldx #imm
 *   txa        -> lda #imm
 *   tay        -> ldy #imm
 *   tya        -> lda #imm
 *   lda zp     -> lda #imm
 *   ldx zp     -> ldx #imm
 *   ldy zp	-> ldy #imm
 *
 * Provided that the register values are known of course.
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

	const char* Arg;

      	/* Get next entry and it's input register values */
       	CodeEntry* E = CS_GetEntry (S, I);
	const RegContents* In = &E->RI->In;

	/* Assume we have no replacement */
	CodeEntry* X = 0;

	/* Check the instruction */
	switch (E->OPC) {

	    case OP65_DEA:
	        if (RegValIsKnown (In->RegA)) {
	      	    Arg = MakeHexArg ((In->RegA - 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_DEX:
	        if (RegValIsKnown (In->RegX)) {
	      	    Arg = MakeHexArg ((In->RegX - 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_DEY:
	        if (RegValIsKnown (In->RegY)) {
	      	    Arg = MakeHexArg ((In->RegY - 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_INA:
	        if (RegValIsKnown (In->RegA)) {
	      	    Arg = MakeHexArg ((In->RegA + 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_INX:
	        if (RegValIsKnown (In->RegX)) {
	      	    Arg = MakeHexArg ((In->RegX + 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_INY:
	        if (RegValIsKnown (In->RegY)) {
	      	    Arg = MakeHexArg ((In->RegY + 1) & 0xFF);
	      	    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_LDA:
	        if (E->AM == AM65_ZP) {
		    switch (GetKnownReg (E->Use & REG_ZP, In)) {
			case REG_TMP1:
			    Arg = MakeHexArg (In->Tmp1);
			    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_LO:
			    Arg = MakeHexArg (In->Ptr1Lo);
	       		    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_HI:
			    Arg = MakeHexArg (In->Ptr1Hi);
			    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_LO:
			    Arg = MakeHexArg (In->SRegLo);
			    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_HI:
			    Arg = MakeHexArg (In->SRegHi);
			    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
			    break;
		    }
		}
		break;

	    case OP65_LDX:
	        if (E->AM == AM65_ZP) {
		    switch (GetKnownReg (E->Use & REG_ZP, In)) {
			case REG_TMP1:
			    Arg = MakeHexArg (In->Tmp1);
			    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_LO:
			    Arg = MakeHexArg (In->Ptr1Lo);
			    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_HI:
			    Arg = MakeHexArg (In->Ptr1Hi);
			    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_LO:
			    Arg = MakeHexArg (In->SRegLo);
			    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_HI:
			    Arg = MakeHexArg (In->SRegHi);
			    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
			    break;
		    }
		}
		break;

	    case OP65_LDY:
	        if (E->AM == AM65_ZP) {
		    switch (GetKnownReg (E->Use, In)) {
			case REG_TMP1:
			    Arg = MakeHexArg (In->Tmp1);
			    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_LO:
			    Arg = MakeHexArg (In->Ptr1Lo);
			    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_PTR1_HI:
			    Arg = MakeHexArg (In->Ptr1Hi);
			    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_LO:
			    Arg = MakeHexArg (In->SRegLo);
			    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
			    break;

			case REG_SREG_HI:
			    Arg = MakeHexArg (In->SRegHi);
			    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
			    break;
		    }
		}
		break;

	    case OP65_TAX:
	        if (E->RI->In.RegA >= 0) {
		    Arg = MakeHexArg (In->RegA);
	      	    X = NewCodeEntry (OP65_LDX, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_TAY:
	        if (E->RI->In.RegA >= 0) {
		    Arg = MakeHexArg (In->RegA);
	      	    X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_TXA:
	        if (E->RI->In.RegX >= 0) {
		    Arg = MakeHexArg (In->RegX);
	      	    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    case OP65_TYA:
	        if (E->RI->In.RegY >= 0) {
	       	    Arg = MakeHexArg (In->RegY);
	      	    X = NewCodeEntry (OP65_LDA, AM65_IMM, Arg, 0, E->LI);
	    	}
	        break;

	    default:
	        /* Avoid gcc warnings */
	        break;

	}

	/* Insert the replacement if we have one */
	if (X) {
       	    CS_InsertEntry (S, X, I+1);
	    CS_DelEntry (S, I);
	    ++Changes;
	}

	/* Next entry */
	++I;

    }

    /* Return the number of changes made */
    return Changes;
}



/*****************************************************************************/
/*                        Optimize stack pointer ops                         */
/*****************************************************************************/



static unsigned IsDecSP (const CodeEntry* E)
/* Check if this is an insn that decrements the stack pointer. If so, return
 * the decrement. If not, return zero.
 * The function expects E to be a subroutine call.
 */
{
    if (strncmp (E->Arg, "decsp", 5) == 0) {
        if (E->Arg[5] >= '1' && E->Arg[5] <= '8') {
            return (E->Arg[5] - '0');
        }
    } else if (strcmp (E->Arg, "subysp") == 0 && RegValIsKnown (E->RI->In.RegY)) {
        return E->RI->In.RegY;
    }

    /* If we come here, it's not a decsp op */
    return 0;
}



static unsigned OptStackPtrOps (CodeSeg* S)
/* Merge adjacent calls to decsp into one. NOTE: This function won't merge all
 * known cases!
 */
{
    unsigned Changes = 0;
    unsigned I;

    /* Walk over the entries */
    I = 0;
    while (I < CS_GetEntryCount (S)) {

        unsigned Dec1;
        unsigned Dec2;
        const CodeEntry* N;

        /* Get the next entry */
        const CodeEntry* E = CS_GetEntry (S, I);

        /* Check for decspn or subysp */
        if (E->OPC == OP65_JSR                          &&
            (Dec1 = IsDecSP (E)) > 0                    &&
            (N = CS_GetNextEntry (S, I)) != 0           &&
            (Dec2 = IsDecSP (N)) > 0                    &&
            (Dec1 += Dec2) <= 255                       &&
            !CE_HasLabel (N)) {

            CodeEntry* X;
            char Buf[20];

            /* We can combine the two */
            if (Dec1 <= 8) {
                /* Insert a call to decsp */
                xsprintf (Buf, sizeof (Buf), "decsp%u", Dec1);
                X = NewCodeEntry (OP65_JSR, AM65_ABS, Buf, 0, N->LI);
                CS_InsertEntry (S, X, I+2);
            } else {
                /* Insert a call to subysp */
                const char* Arg = MakeHexArg (Dec1);
                X = NewCodeEntry (OP65_LDY, AM65_IMM, Arg, 0, N->LI);
                CS_InsertEntry (S, X, I+2);
                X = NewCodeEntry (OP65_JSR, AM65_ABS, "subysp", 0, N->LI);
                CS_InsertEntry (S, X, I+3);
            }

            /* Delete the old code */
            CS_DelEntries (S, I, 2);

            /* Regenerate register info */
            CS_GenRegInfo (S);

            /* Remember we had changes */
	    ++Changes;

	} else {

            /* Next entry */
	    ++I;
        }

    }

    /* Return the number of changes made */
    return Changes;
}



/*****************************************************************************/
/*  	     	    		struct OptFunc                               */
/*****************************************************************************/



typedef struct OptFunc OptFunc;
struct OptFunc {
    unsigned       (*Func) (CodeSeg*);  /* Optimizer function */
    const char*    Name;                /* Name of the function/group */
    unsigned       CodeSizeFactor;      /* Code size factor for this opt func */
    unsigned long  TotalRuns;		/* Total number of runs */
    unsigned long  LastRuns;            /* Last number of runs */
    unsigned long  TotalChanges;        /* Total number of changes */
    unsigned long  LastChanges;         /* Last number of changes */
    char           Disabled;            /* True if function disabled */
};



/*****************************************************************************/
/*     	       	      	       	     Code	   	  		     */
/*****************************************************************************/



/* A list of all the function descriptions */
static OptFunc DOpt65C02BitOps  = { Opt65C02BitOps,  "Opt65C02BitOps",   66, 0, 0, 0, 0, 0 };
static OptFunc DOpt65C02Ind    	= { Opt65C02Ind,     "Opt65C02Ind",     100, 0, 0, 0, 0, 0 };
static OptFunc DOpt65C02Stores  = { Opt65C02Stores,  "Opt65C02Stores",  100, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd1	       	= { OptAdd1,   	     "OptAdd1",        	125, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd2	       	= { OptAdd2,   	     "OptAdd2",        	200, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd3	       	= { OptAdd3,   	     "OptAdd3",        	 65, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd4	       	= { OptAdd4,   	     "OptAdd4",        	 90, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd5	       	= { OptAdd5,   	     "OptAdd5",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptAdd6	       	= { OptAdd6,   	     "OptAdd6",        	 40, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegA1       = { OptBNegA1,       "OptBNegA1",       100, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegA2       = { OptBNegA2,       "OptBNegA2",       100, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegAX1      = { OptBNegAX1,      "OptBNegAX1",      100, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegAX2      = { OptBNegAX2,      "OptBNegAX2",      100, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegAX3      = { OptBNegAX3,      "OptBNegAX3",      100, 0, 0, 0, 0, 0 };
static OptFunc DOptBNegAX4      = { OptBNegAX4,      "OptBNegAX4",      100, 0, 0, 0, 0, 0 };
static OptFunc DOptBoolTrans    = { OptBoolTrans,    "OptBoolTrans",    100, 0, 0, 0, 0, 0 };
static OptFunc DOptBranchDist  	= { OptBranchDist,   "OptBranchDist",     0, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp1	       	= { OptCmp1,   	     "OptCmp1",        	 42, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp2	       	= { OptCmp2,   	     "OptCmp2",        	 85, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp3	       	= { OptCmp3,   	     "OptCmp3",        	 75, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp4	       	= { OptCmp4,   	     "OptCmp4",        	 75, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp5	       	= { OptCmp5,   	     "OptCmp5",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp6	       	= { OptCmp6,   	     "OptCmp6",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp7	       	= { OptCmp7,   	     "OptCmp7",        	 85, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp8	       	= { OptCmp8,   	     "OptCmp8",        	 50, 0, 0, 0, 0, 0 };
static OptFunc DOptCmp9	       	= { OptCmp9,   	     "OptCmp9",        	 85, 0, 0, 0, 0, 0 };
static OptFunc DOptCondBranches1= { OptCondBranches1,"OptCondBranches1", 80, 0, 0, 0, 0, 0 };
static OptFunc DOptCondBranches2= { OptCondBranches2,"OptCondBranches2",  0, 0, 0, 0, 0, 0 };
static OptFunc DOptDeadCode    	= { OptDeadCode,     "OptDeadCode",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptDeadJumps   	= { OptDeadJumps,    "OptDeadJumps",    100, 0, 0, 0, 0, 0 };
static OptFunc DOptDecouple     = { OptDecouple,     "OptDecouple",     100, 0, 0, 0, 0, 0 };
static OptFunc DOptDupLoads     = { OptDupLoads,     "OptDupLoads",       0, 0, 0, 0, 0, 0 };
static OptFunc DOptIndLoads1    = { OptIndLoads1,    "OptIndLoads1",      0, 0, 0, 0, 0, 0 };
static OptFunc DOptIndLoads2    = { OptIndLoads2,    "OptIndLoads2",      0, 0, 0, 0, 0, 0 };
static OptFunc DOptJumpCascades	= { OptJumpCascades, "OptJumpCascades", 100, 0, 0, 0, 0, 0 };
static OptFunc DOptJumpTarget1  = { OptJumpTarget1,  "OptJumpTarget1",  100, 0, 0, 0, 0, 0 };
static OptFunc DOptJumpTarget2  = { OptJumpTarget2,  "OptJumpTarget2",  100, 0, 0, 0, 0, 0 };
static OptFunc DOptJumpTarget3  = { OptJumpTarget3,  "OptJumpTarget3",  100, 0, 0, 0, 0, 0 };
static OptFunc DOptLoad1        = { OptLoad1,        "OptLoad1",        100, 0, 0, 0, 0, 0 };
static OptFunc DOptLoad2        = { OptLoad2,        "OptLoad2",        200, 0, 0, 0, 0, 0 };
static OptFunc DOptNegAX1       = { OptNegAX1,       "OptNegAX1",       165, 0, 0, 0, 0, 0 };
static OptFunc DOptNegAX2       = { OptNegAX2,       "OptNegAX2",       200, 0, 0, 0, 0, 0 };
static OptFunc DOptRTS 	       	= { OptRTS,    	     "OptRTS",         	100, 0, 0, 0, 0, 0 };
static OptFunc DOptRTSJumps1    = { OptRTSJumps1,    "OptRTSJumps1",   	100, 0, 0, 0, 0, 0 };
static OptFunc DOptRTSJumps2    = { OptRTSJumps2,    "OptRTSJumps2",   	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPrecalc      = { OptPrecalc,      "OptPrecalc",     	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad1    	= { OptPtrLoad1,     "OptPtrLoad1",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad2    	= { OptPtrLoad2,     "OptPtrLoad2",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad3    	= { OptPtrLoad3,     "OptPtrLoad3",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad4    	= { OptPtrLoad4,     "OptPtrLoad4",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad5    	= { OptPtrLoad5,     "OptPtrLoad5",    	 50, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad6    	= { OptPtrLoad6,     "OptPtrLoad6",    	 60, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad7    	= { OptPtrLoad7,     "OptPtrLoad7",    	140, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad11   	= { OptPtrLoad11,    "OptPtrLoad11",     92, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad12   	= { OptPtrLoad12,    "OptPtrLoad12",     50, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad13   	= { OptPtrLoad13,    "OptPtrLoad13",     65, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad14   	= { OptPtrLoad14,    "OptPtrLoad14",   	108, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad15   	= { OptPtrLoad15,    "OptPtrLoad15",   	 86, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad16   	= { OptPtrLoad16,    "OptPtrLoad16",   	100, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrLoad17   	= { OptPtrLoad17,    "OptPtrLoad17",    190, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrStore1   	= { OptPtrStore1,    "OptPtrStore1",     65, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrStore2   	= { OptPtrStore2,    "OptPtrStore2",     65, 0, 0, 0, 0, 0 };
static OptFunc DOptPtrStore3   	= { OptPtrStore3,    "OptPtrStore3",    100, 0, 0, 0, 0, 0 };
static OptFunc DOptPush1       	= { OptPush1,        "OptPush1",         65, 0, 0, 0, 0, 0 };
static OptFunc DOptPush2       	= { OptPush2,        "OptPush2",         50, 0, 0, 0, 0, 0 };
static OptFunc DOptPushPop      = { OptPushPop,      "OptPushPop",        0, 0, 0, 0, 0, 0 };
static OptFunc DOptShift1      	= { OptShift1,       "OptShift1",      	100, 0, 0, 0, 0, 0 };
static OptFunc DOptShift2      	= { OptShift2,       "OptShift2",      	100, 0, 0, 0, 0, 0 };
static OptFunc DOptShift3      	= { OptShift3,       "OptShift3",      	 17, 0, 0, 0, 0, 0 };
static OptFunc DOptShift4      	= { OptShift4,       "OptShift4",      	100, 0, 0, 0, 0, 0 };
static OptFunc DOptShift5      	= { OptShift5,       "OptShift5",      	110, 0, 0, 0, 0, 0 };
static OptFunc DOptShift6      	= { OptShift6,       "OptShift6",      	200, 0, 0, 0, 0, 0 };
static OptFunc DOptSize1        = { OptSize1,        "OptSize1",        100, 0, 0, 0, 0, 0 };
static OptFunc DOptSize2        = { OptSize2,        "OptSize2",        100, 0, 0, 0, 0, 0 };
static OptFunc DOptStackOps    	= { OptStackOps,     "OptStackOps",    	100, 0, 0, 0, 0, 0 };
static OptFunc DOptStackPtrOps  = { OptStackPtrOps,  "OptStackPtrOps",   50, 0, 0, 0, 0, 0 };
static OptFunc DOptStore1       = { OptStore1,       "OptStore1",        70, 0, 0, 0, 0, 0 };
static OptFunc DOptStore2       = { OptStore2,       "OptStore2",       115, 0, 0, 0, 0, 0 };
static OptFunc DOptStore3       = { OptStore3,       "OptStore3",       120, 0, 0, 0, 0, 0 };
static OptFunc DOptStore4       = { OptStore4,       "OptStore4",        50, 0, 0, 0, 0, 0 };
static OptFunc DOptStore5       = { OptStore5,       "OptStore5",       100, 0, 0, 0, 0, 0 };
static OptFunc DOptStoreLoad   	= { OptStoreLoad,    "OptStoreLoad",      0, 0, 0, 0, 0, 0 };
static OptFunc DOptSub1	       	= { OptSub1,   	     "OptSub1",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptSub2	       	= { OptSub2,   	     "OptSub2",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptSub3	       	= { OptSub3,   	     "OptSub3",        	100, 0, 0, 0, 0, 0 };
static OptFunc DOptTest1       	= { OptTest1,  	     "OptTest1",       	 65, 0, 0, 0, 0, 0 };
static OptFunc DOptTest2       	= { OptTest2,  	     "OptTest2",       	 50, 0, 0, 0, 0, 0 };
static OptFunc DOptTransfers1  	= { OptTransfers1,   "OptTransfers1",     0, 0, 0, 0, 0, 0 };
static OptFunc DOptTransfers2  	= { OptTransfers2,   "OptTransfers2",    60, 0, 0, 0, 0, 0 };
static OptFunc DOptTransfers3  	= { OptTransfers3,   "OptTransfers3",    65, 0, 0, 0, 0, 0 };
static OptFunc DOptTransfers4  	= { OptTransfers4,   "OptTransfers4",    65, 0, 0, 0, 0, 0 };
static OptFunc DOptUnusedLoads 	= { OptUnusedLoads,  "OptUnusedLoads",    0, 0, 0, 0, 0, 0 };
static OptFunc DOptUnusedStores	= { OptUnusedStores, "OptUnusedStores",   0, 0, 0, 0, 0, 0 };


/* Table containing all the steps in alphabetical order */
static OptFunc* OptFuncs[] = {
    &DOpt65C02BitOps,
    &DOpt65C02Ind,
    &DOpt65C02Stores,
    &DOptAdd1,
    &DOptAdd2,
    &DOptAdd3,
    &DOptAdd4,
    &DOptAdd5,
    &DOptAdd6,
    &DOptBNegA1,
    &DOptBNegA2,
    &DOptBNegAX1,
    &DOptBNegAX2,
    &DOptBNegAX3,
    &DOptBNegAX4,
    &DOptBoolTrans,
    &DOptBranchDist,
    &DOptCmp1,
    &DOptCmp2,
    &DOptCmp3,
    &DOptCmp4,
    &DOptCmp5,
    &DOptCmp6,
    &DOptCmp7,
    &DOptCmp8,
    &DOptCmp9,
    &DOptCondBranches1,
    &DOptCondBranches2,
    &DOptDeadCode,
    &DOptDeadJumps,
    &DOptDecouple,
    &DOptDupLoads,
    &DOptIndLoads1,
    &DOptIndLoads2,
    &DOptJumpCascades,
    &DOptJumpTarget1,
    &DOptJumpTarget2,
    &DOptJumpTarget3,
    &DOptLoad1,
    &DOptLoad2,
    &DOptNegAX1,
    &DOptNegAX2,
    &DOptPrecalc,
    &DOptPtrLoad1,
    &DOptPtrLoad11,
    &DOptPtrLoad12,
    &DOptPtrLoad13,
    &DOptPtrLoad14,
    &DOptPtrLoad15,
    &DOptPtrLoad16,
    &DOptPtrLoad17,
    &DOptPtrLoad2,
    &DOptPtrLoad3,
    &DOptPtrLoad4,
    &DOptPtrLoad5,
    &DOptPtrLoad6,
    &DOptPtrLoad7,
    &DOptPtrStore1,
    &DOptPtrStore2,
    &DOptPtrStore3,
    &DOptPush1,
    &DOptPush2,
    &DOptPushPop,
    &DOptRTS,
    &DOptRTSJumps1,
    &DOptRTSJumps2,
    &DOptShift1,
    &DOptShift2,
    &DOptShift3,
    &DOptShift4,
    &DOptShift5,
    &DOptShift6,
    &DOptSize1,
    &DOptSize2,
    &DOptStackOps,
    &DOptStackPtrOps,
    &DOptStore1,
    &DOptStore2,
    &DOptStore3,
    &DOptStore4,
    &DOptStore5,
    &DOptStoreLoad,
    &DOptSub1,
    &DOptSub2,
    &DOptSub3,
    &DOptTest1,
    &DOptTest2,
    &DOptTransfers1,
    &DOptTransfers2,
    &DOptTransfers3,
    &DOptTransfers4,
    &DOptUnusedLoads,
    &DOptUnusedStores,
};
#define OPTFUNC_COUNT  (sizeof(OptFuncs) / sizeof(OptFuncs[0]))



static int CmpOptStep (const void* Key, const void* Func)
/* Compare function for bsearch */
{
    return strcmp (Key, (*(const OptFunc**)Func)->Name);
}



static OptFunc* FindOptFunc (const char* Name)
/* Find an optimizer step by name in the table and return a pointer. Return
 * NULL if no such step is found.
 */
{
    /* Search for the function in the list */
    OptFunc** O = bsearch (Name, OptFuncs, OPTFUNC_COUNT, sizeof (OptFuncs[0]), CmpOptStep);
    return O? *O : 0;
}



static OptFunc* GetOptFunc (const char* Name)
/* Find an optimizer step by name in the table and return a pointer. Print an
 * error and call AbEnd if not found.
 */
{
    /* Search for the function in the list */
    OptFunc* F = FindOptFunc (Name);
    if (F == 0) {
	/* Not found */
	AbEnd ("Optimization step `%s' not found", Name);
    }
    return F;
}



void DisableOpt (const char* Name)
/* Disable the optimization with the given name */
{
    if (strcmp (Name, "any") == 0) {
	unsigned I;
       	for (I = 0; I < OPTFUNC_COUNT; ++I) {
       	    OptFuncs[I]->Disabled = 1;
    	}
    } else {
     	GetOptFunc(Name)->Disabled = 1;
    }
}



void EnableOpt (const char* Name)
/* Enable the optimization with the given name */
{
    if (strcmp (Name, "any") == 0) {
	unsigned I;
       	for (I = 0; I < OPTFUNC_COUNT; ++I) {
       	    OptFuncs[I]->Disabled = 0;
	}
    } else {
     	GetOptFunc(Name)->Disabled = 0;
    }
}



void ListOptSteps (FILE* F)
/* List all optimization steps */
{
    unsigned I;
    for (I = 0; I < OPTFUNC_COUNT; ++I) {
    	fprintf (F, "%s\n", OptFuncs[I]->Name);
    }
}



static void ReadOptStats (const char* Name)
/* Read the optimizer statistics file */
{
    char Buf [256];
    unsigned Lines;

    /* Try to open the file */
    FILE* F = fopen (Name, "r");
    if (F == 0) {
    	/* Ignore the error */
    	return;
    }

    /* Read and parse the lines */
    Lines = 0;
    while (fgets (Buf, sizeof (Buf), F) != 0) {

	char* B;
	unsigned Len;
	OptFunc* Func;

	/* Fields */
	char Name[32];
       	unsigned long  TotalRuns;
	unsigned long  TotalChanges;

	/* Count lines */
	++Lines;

	/* Remove trailing white space including the line terminator */
	B = Buf;
	Len = strlen (B);
	while (Len > 0 && IsSpace (B[Len-1])) {
	    --Len;
	}
    	B[Len] = '\0';

	/* Remove leading whitespace */
	while (IsSpace (*B)) {
	    ++B;
	}

	/* Check for empty and comment lines */
	if (*B == '\0' || *B == ';' || *B == '#') {
	    continue;
	}

	/* Parse the line */
       	if (sscanf (B, "%31s %lu %*u %lu %*u", Name, &TotalRuns, &TotalChanges) != 3) {
      	    /* Syntax error */
	    continue;
	}

	/* Search for the optimizer step. */
	Func = FindOptFunc (Name);
	if (Func == 0) {
	    /* Not found */
	    continue;
	}

	/* Found the step, set the fields */
	Func->TotalRuns    = TotalRuns;
	Func->TotalChanges = TotalChanges;

    }

    /* Close the file, ignore errors here. */
    fclose (F);
}



static void WriteOptStats (const char* Name)
/* Write the optimizer statistics file */
{
    unsigned I;

    /* Try to open the file */
    FILE* F = fopen (Name, "w");
    if (F == 0) {
    	/* Ignore the error */
    	return;
    }

    /* Write a header */
    fprintf (F,
	     "; Optimizer               Total      Last       Total      Last\n"
       	     ";   Step                  Runs       Runs        Chg       Chg\n");


    /* Write the data */
    for (I = 0; I < OPTFUNC_COUNT; ++I) {
    	const OptFunc* O = OptFuncs[I];
    	fprintf (F,
       	       	 "%-20s %10lu %10lu %10lu %10lu\n",
    		 O->Name,
   		 O->TotalRuns,
      	       	 O->LastRuns,
   	       	 O->TotalChanges,
   		 O->LastChanges);
    }

    /* Close the file, ignore errors here. */
    fclose (F);
}



static unsigned RunOptFunc (CodeSeg* S, OptFunc* F, unsigned Max)
/* Run one optimizer function Max times or until there are no more changes */
{
    unsigned Changes, C;

    /* Don't run the function if it is disabled or if it is prohibited by the
     * code size factor
     */
    if (F->Disabled || F->CodeSizeFactor > S->CodeSizeFactor) {
    	return 0;
    }

    /* Run this until there are no more changes */
    Changes = 0;
    do {

   	/* Run the function */
    	C = F->Func (S);
        if (Debug && C > 0) {
            printf ("Applied %s: %u changes\n", F->Name, C);
        }
    	Changes += C;

    	/* Do statistics */
    	++F->TotalRuns;
    	++F->LastRuns;
    	F->TotalChanges += C;
    	F->LastChanges  += C;

        /* If we had changes, regenerate register info */
        if (C) {
            CS_GenRegInfo (S);
        }

    } while (--Max && C > 0);

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup1 (CodeSeg* S)
/* Run the first group of optimization steps. These steps translate known
 * patterns emitted by the code generator into more optimal patterns. Order
 * of the steps is important, because some of the steps done earlier cover
 * the same patterns as later steps as subpatterns.
 */
{
    unsigned Changes = 0;

    Changes += RunOptFunc (S, &DOptStackPtrOps, 5);
    Changes += RunOptFunc (S, &DOptPtrStore1, 1);
    Changes += RunOptFunc (S, &DOptPtrStore2, 1);
    Changes += RunOptFunc (S, &DOptPtrStore3, 1);
    Changes += RunOptFunc (S, &DOptAdd3, 1);    /* Before OptPtrLoad5! */
    Changes += RunOptFunc (S, &DOptPtrLoad1, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad2, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad3, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad4, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad5, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad6, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad7, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad11, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad12, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad13, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad14, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad15, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad16, 1);
    Changes += RunOptFunc (S, &DOptPtrLoad17, 1);
    Changes += RunOptFunc (S, &DOptBNegAX1, 1);
    Changes += RunOptFunc (S, &DOptBNegAX2, 1);
    Changes += RunOptFunc (S, &DOptBNegAX3, 1);
    Changes += RunOptFunc (S, &DOptBNegAX4, 1);
    Changes += RunOptFunc (S, &DOptAdd1, 1);
    Changes += RunOptFunc (S, &DOptAdd2, 1);
    Changes += RunOptFunc (S, &DOptAdd4, 1);
    Changes += RunOptFunc (S, &DOptAdd5, 1);
    Changes += RunOptFunc (S, &DOptAdd6, 1);
    Changes += RunOptFunc (S, &DOptSub1, 1);
    Changes += RunOptFunc (S, &DOptSub3, 1);
    Changes += RunOptFunc (S, &DOptStore4, 1);
    Changes += RunOptFunc (S, &DOptStore5, 1);
    Changes += RunOptFunc (S, &DOptShift1, 1);
    Changes += RunOptFunc (S, &DOptShift2, 1);
    Changes += RunOptFunc (S, &DOptShift4, 1);
    Changes += RunOptFunc (S, &DOptShift5, 1);
    Changes += RunOptFunc (S, &DOptShift6, 1);
    Changes += RunOptFunc (S, &DOptStore1, 1);
    Changes += RunOptFunc (S, &DOptStore2, 5);
    Changes += RunOptFunc (S, &DOptStore3, 5);

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup2 (CodeSeg* S)
/* Run one group of optimization steps. This step involves just decoupling
 * instructions by replacing them by instructions that do not depend on
 * previous instructions. This makes it easier to find instructions that
 * aren't used.
 */
{
    unsigned Changes = 0;

    Changes += RunOptFunc (S, &DOptDecouple, 1);

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup3 (CodeSeg* S)
/* Run one group of optimization steps. These steps depend on each other,
 * that means that one step may allow another step to do additional work,
 * so we will repeat the steps as long as we see any changes.
 */
{
    unsigned Changes, C;

    Changes = 0;
    do {
       	C = 0;

       	C += RunOptFunc (S, &DOptBNegA1, 1);
       	C += RunOptFunc (S, &DOptBNegA2, 1);
        C += RunOptFunc (S, &DOptNegAX1, 1);
        C += RunOptFunc (S, &DOptNegAX2, 1);
       	C += RunOptFunc (S, &DOptStackOps, 1);
       	C += RunOptFunc (S, &DOptSub1, 1);
       	C += RunOptFunc (S, &DOptSub2, 1);
       	C += RunOptFunc (S, &DOptSub3, 1);
       	C += RunOptFunc (S, &DOptAdd5, 1);
       	C += RunOptFunc (S, &DOptAdd6, 1);
       	C += RunOptFunc (S, &DOptJumpCascades, 1);
       	C += RunOptFunc (S, &DOptDeadJumps, 1);
       	C += RunOptFunc (S, &DOptRTS, 1);
       	C += RunOptFunc (S, &DOptDeadCode, 1);
       	C += RunOptFunc (S, &DOptBoolTrans, 1);
       	C += RunOptFunc (S, &DOptJumpTarget1, 1);
       	C += RunOptFunc (S, &DOptJumpTarget2, 1);
       	C += RunOptFunc (S, &DOptCondBranches1, 1);
       	C += RunOptFunc (S, &DOptCondBranches2, 1);
       	C += RunOptFunc (S, &DOptRTSJumps1, 1);
       	C += RunOptFunc (S, &DOptCmp1, 1);
       	C += RunOptFunc (S, &DOptCmp2, 1);
       	C += RunOptFunc (S, &DOptCmp3, 1);
       	C += RunOptFunc (S, &DOptCmp4, 1);
       	C += RunOptFunc (S, &DOptCmp5, 1);
       	C += RunOptFunc (S, &DOptCmp6, 1);
       	C += RunOptFunc (S, &DOptCmp7, 1);
       	C += RunOptFunc (S, &DOptCmp8, 1);
       	C += RunOptFunc (S, &DOptCmp9, 1);
       	C += RunOptFunc (S, &DOptTest1, 1);
        C += RunOptFunc (S, &DOptLoad1, 1);
       	C += RunOptFunc (S, &DOptJumpTarget3, 1);       /* After OptCondBranches2 */
       	C += RunOptFunc (S, &DOptUnusedLoads, 1);
       	C += RunOptFunc (S, &DOptUnusedStores, 1);
       	C += RunOptFunc (S, &DOptDupLoads, 1);
       	C += RunOptFunc (S, &DOptStoreLoad, 1);
       	C += RunOptFunc (S, &DOptTransfers1, 1);
       	C += RunOptFunc (S, &DOptTransfers3, 1);
       	C += RunOptFunc (S, &DOptTransfers4, 1);
       	C += RunOptFunc (S, &DOptStore1, 1);
       	C += RunOptFunc (S, &DOptStore5, 1);
        C += RunOptFunc (S, &DOptPushPop, 1);
        C += RunOptFunc (S, &DOptPrecalc, 1);

	Changes += C;

    } while (C);

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup4 (CodeSeg* S)
/* Run another round of pattern replacements. These are done late, since there
 * may be better replacements before.
 */
{
    unsigned Changes = 0;

    /* Repeat some of the steps here */
    Changes += RunOptFunc (S, &DOptShift3, 1);
    Changes += RunOptFunc (S, &DOptPush1, 1);
    Changes += RunOptFunc (S, &DOptPush2, 1);
    Changes += RunOptFunc (S, &DOptUnusedLoads, 1);
    Changes += RunOptFunc (S, &DOptTest2, 1);
    Changes += RunOptFunc (S, &DOptTransfers2, 1);
    Changes += RunOptFunc (S, &DOptLoad2, 1);

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup5 (CodeSeg* S)
/* 65C02 specific optimizations. */
{
    unsigned Changes = 0;

    if (CPUIsets[CPU] & CPU_ISET_65SC02) {
        Changes += RunOptFunc (S, &DOpt65C02BitOps, 1);
    	Changes += RunOptFunc (S, &DOpt65C02Ind, 1);
        Changes += RunOptFunc (S, &DOpt65C02Stores, 1);
       	if (Changes) {
            /* The 65C02 replacement codes do often make the use of a register
             * value unnecessary, so if we have changes, run another load
             * removal pass.
             */
    	    Changes += RunOptFunc (S, &DOptUnusedLoads, 1);
    	}
    }

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup6 (CodeSeg* S)
/* This one is quite special. It tries to replace "lda (sp),y" by "lda (sp,x)".
 * The latter is ony cycle slower, but if we're able to remove the necessary
 * load of the Y register, because X is zero anyway, we gain 1 cycle and
 * shorten the code by one (transfer) or two bytes (load). So what we do is
 * to replace the insns, remove unused loads, and then change back all insns
 * where Y is still zero (meaning that the load has not been removed).
 */
{
    unsigned Changes = 0;

    /* This group will only run for a standard 6502, because the 65C02 has a
     * better addressing mode that covers this case.
     */
    if ((CPUIsets[CPU] & CPU_ISET_65SC02) == 0) {
        Changes += RunOptFunc (S, &DOptIndLoads1, 1);
        Changes += RunOptFunc (S, &DOptUnusedLoads, 1);
        Changes += RunOptFunc (S, &DOptIndLoads2, 1);
    }

    /* Return the number of changes */
    return Changes;
}



static unsigned RunOptGroup7 (CodeSeg* S)
/* The last group of optimization steps. Adjust branches, do size optimizations.
 */
{
    unsigned Changes = 0;
    unsigned C;

    /* Optimize for size, that is replace operations by shorter ones, even
     * if this does hinder further optimizations (no problem since we're
     * done soon).
     */
    C = RunOptFunc (S, &DOptSize1, 1);
    if (C) {
        Changes += C;
        /* Run some optimization passes again, since the size optimizations
         * may have opened new oportunities.
         */
        Changes += RunOptFunc (S, &DOptUnusedLoads, 1);
        Changes += RunOptFunc (S, &DOptUnusedStores, 1);
        Changes += RunOptFunc (S, &DOptJumpTarget1, 5);
        Changes += RunOptFunc (S, &DOptStore5, 1);
    }

    C = RunOptFunc (S, &DOptSize2, 1);
    if (C) {
        Changes += C;
        /* Run some optimization passes again, since the size optimizations
         * may have opened new oportunities.
         */
        Changes += RunOptFunc (S, &DOptUnusedLoads, 1);
        Changes += RunOptFunc (S, &DOptJumpTarget1, 5);
        Changes += RunOptFunc (S, &DOptStore5, 1);
        Changes += RunOptFunc (S, &DOptTransfers3, 1);
    }

    /* Adjust branch distances */
    Changes += RunOptFunc (S, &DOptBranchDist, 3);

    /* Replace conditional branches to RTS. If we had changes, we must run dead
     * code elimination again, since the change may have introduced dead code.
     */
    C = RunOptFunc (S, &DOptRTSJumps2, 1);
    Changes += C;
    if (C) {
      	Changes += RunOptFunc (S, &DOptDeadCode, 1);
    }

    /* Return the number of changes */
    return Changes;
}



void RunOpt (CodeSeg* S)
/* Run the optimizer */
{
    const char* StatFileName;

    /* If we shouldn't run the optimizer, bail out */
    if (!S->Optimize) {
      	return;
    }

    /* Check if we are requested to write optimizer statistics */
    StatFileName = getenv ("CC65_OPTSTATS");
    if (StatFileName) {
	ReadOptStats (StatFileName);
    }

    /* Print the name of the function we are working on */
    if (S->Func) {
     	Print (stdout, 1, "Running optimizer for function `%s'\n", S->Func->Name);
    } else {
     	Print (stdout, 1, "Running optimizer for global code segment\n");
    }

    /* Generate register info for all instructions */
    CS_GenRegInfo (S);

    /* Run groups of optimizations */
    RunOptGroup1 (S);
    RunOptGroup2 (S);
    RunOptGroup3 (S);
    RunOptGroup4 (S);
    RunOptGroup5 (S);
    RunOptGroup6 (S);
    RunOptGroup7 (S);

    /* Free register info */
    CS_FreeRegInfo (S);

    /* Write statistics */
    if (StatFileName) {
	WriteOptStats (StatFileName);
    }
}




/*****************************************************************************/
/*                                                                           */
/*				  codeinfo.c				     */
/*                                                                           */
/*		    Additional information about 6502 code		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2001-2002 Ullrich von Bassewitz                                       */
/*               Wacholderweg 14                                             */
/*               D-70597 Stuttgart                                           */
/* EMail:        uz@cc65.org                                                 */
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
#include "coll.h"

/* cc65 */
#include "codeent.h"
#include "codeseg.h"
#include "datatype.h"
#include "error.h"
#include "reginfo.h"
#include "symtab.h"
#include "codeinfo.h"



/*****************************************************************************/
/*     	       	      	  	     Data				     */
/*****************************************************************************/



/* Table with the compare suffixes */
static const char CmpSuffixTab [][4] = {
    "eq", "ne", "gt", "ge", "lt", "le", "ugt", "uge", "ult", "ule"
};

/* Table listing the function names and code info values for known internally
 * used functions. This table should get auto-generated in the future.
 */
typedef struct FuncInfo FuncInfo;
struct FuncInfo {
    const char*	    Name;	/* Function name */
    unsigned short  Use;	/* Register usage */
    unsigned short  Chg;   	/* Changed/destroyed registers */
};

static const FuncInfo FuncInfoTable[] = {
    { "addeq0sp",       REG_AX,               REG_AXY                        },
    { "addeqysp",       REG_AXY,              REG_AXY                        },
    { "addysp",	       	REG_Y, 	       	      REG_NONE	                     },
    { "aslax1",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "aslax2",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "aslax3",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "aslax4",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "bnega",          REG_A,                REG_AX			     },
    { "bnegax",         REG_AX,               REG_AX			     },
    { "bnegeax",        REG_EAX,              REG_EAX			     },
    { "booleq",	       	REG_NONE,      	      REG_AX			     },
    { "boolge",	       	REG_NONE,      	      REG_AX			     },
    { "boolgt",	       	REG_NONE,      	      REG_AX			     },
    { "boolle",	       	REG_NONE,      	      REG_AX			     },
    { "boollt",	       	REG_NONE,      	      REG_AX			     },
    { "boolne",	       	REG_NONE,      	      REG_AX			     },
    { "booluge",       	REG_NONE,      	      REG_AX			     },
    { "boolugt",       	REG_NONE,      	      REG_AX			     },
    { "boolule",       	REG_NONE,      	      REG_AX			     },
    { "boolult",       	REG_NONE,      	      REG_AX			     },
    { "complax",        REG_AX,               REG_AX			     },
    { "decax1",        	REG_AX,	       	      REG_AX			     },
    { "decax2",        	REG_AX,	       	      REG_AX			     },
    { "decax3",        	REG_AX,	       	      REG_AX			     },
    { "decax4",        	REG_AX,	       	      REG_AX			     },
    { "decax5",        	REG_AX,	       	      REG_AX			     },
    { "decax6",        	REG_AX,	       	      REG_AX			     },
    { "decax7",        	REG_AX,	       	      REG_AX			     },
    { "decax8",        	REG_AX,	       	      REG_AX			     },
    { "decaxy",	       	REG_AXY,       	      REG_AX | REG_TMP1		     },
    { "deceaxy",        REG_EAXY,             REG_EAX                        },
    { "decsp1",        	REG_NONE,      	      REG_Y			     },
    { "decsp2",        	REG_NONE,      	      REG_A			     },
    { "decsp3",        	REG_NONE,      	      REG_A			     },
    { "decsp4",        	REG_NONE,      	      REG_A			     },
    { "decsp5",        	REG_NONE,      	      REG_A			     },
    { "decsp6",        	REG_NONE,      	      REG_A			     },
    { "decsp7",        	REG_NONE,      	      REG_A			     },
    { "decsp8",        	REG_NONE,      	      REG_A			     },
    { "incax1",         REG_AX,               REG_AX			     },
    { "incax2",         REG_AX,               REG_AX			     },
    { "incsp1",	       	REG_NONE,      	      REG_NONE			     },
    { "incsp2",	       	REG_NONE,      	      REG_Y			     },
    { "incsp3",	       	REG_NONE,      	      REG_Y			     },
    { "incsp4",	       	REG_NONE,      	      REG_Y			     },
    { "incsp5",	       	REG_NONE,      	      REG_Y			     },
    { "incsp6",	       	REG_NONE,      	      REG_Y			     },
    { "incsp7",	       	REG_NONE,      	      REG_Y			     },
    { "incsp8",	       	REG_NONE,      	      REG_Y			     },
    { "laddeq",		REG_EAXY|REG_PTR1_LO, REG_EAXY | REG_PTR1_HI         },
    { "laddeq1",       	REG_Y | REG_PTR1_LO,  REG_EAXY | REG_PTR1_HI         },
    { "laddeqa",        REG_AY | REG_PTR1_LO, REG_EAXY | REG_PTR1_HI         },
    { "laddeq0sp",      REG_EAX,              REG_EAXY                       },
    { "laddeqysp",      REG_EAXY,             REG_EAXY                       },
    { "ldaidx",         REG_AXY,              REG_AX | REG_PTR1		     },
    { "ldauidx",        REG_AXY,              REG_AX | REG_PTR1		     },
    { "ldax0sp",       	REG_NONE,             REG_AXY                        },
    { "ldaxi",          REG_AX,               REG_AXY | REG_PTR1	     },
    { "ldaxidx",        REG_AXY,              REG_AXY | REG_PTR1       	     },
    { "ldaxysp",       	REG_Y, 	       	      REG_AXY  	       	       	     },
    { "ldeax0sp",       REG_NONE,             REG_EAXY                       },
    { "ldeaxi",         REG_AX,               REG_EAXY | REG_PTR1            },
    { "ldeaxidx",       REG_AXY,              REG_EAXY | REG_PTR1            },
    { "ldeaxysp",       REG_Y,                REG_EAXY                       },
    { "leaasp",         REG_A,                REG_AX			     },
    { "lsubeq",         REG_EAXY|REG_PTR1_LO, REG_EAXY | REG_PTR1_HI         },
    { "lsubeq0sp",      REG_EAX,              REG_EAXY                       },
    { "lsubeq1",        REG_Y | REG_PTR1_LO,  REG_EAXY | REG_PTR1_HI         },
    { "lsubeqa",        REG_AY | REG_PTR1_LO, REG_EAXY | REG_PTR1_HI         },
    { "lsubeqysp",      REG_EAXY,             REG_EAXY                       },
    { "lsubeq0sp",      REG_EAX,              REG_EAXY                       },
    { "negax",          REG_AX,               REG_AX			     },
    { "push0", 	       	REG_NONE,             REG_AXY			     },
    { "push1", 	       	REG_NONE,             REG_AXY			     },
    { "push2", 	       	REG_NONE,             REG_AXY			     },
    { "push3", 	       	REG_NONE,             REG_AXY			     },
    { "push4", 	       	REG_NONE,             REG_AXY			     },
    { "push5", 	       	REG_NONE,             REG_AXY			     },
    { "push6", 	       	REG_NONE,             REG_AXY			     },
    { "push7", 	       	REG_NONE,             REG_AXY	 		     },
    { "pusha", 	       	REG_A, 	       	      REG_Y			     },
    { "pusha0",	       	REG_A, 	       	      REG_XY			     },
    { "pushax",	       	REG_AX,	       	      REG_Y			     },
    { "pusha0sp",       REG_NONE,             REG_AY                         },
    { "pushaysp",       REG_Y,                REG_AY                         },
    { "pushc0",	       	REG_NONE,             REG_A | REG_Y                  },
    { "pushc1",	       	REG_NONE,             REG_A | REG_Y                  },
    { "pushc2",	       	REG_NONE,             REG_A | REG_Y                  },
    { "pusheax",        REG_EAX,              REG_Y			     },
    { "pushw0sp",      	REG_NONE,      	      REG_AXY			     },
    { "pushwysp",      	REG_Y, 	       	      REG_AXY			     },
    { "shlax1",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "shlax2",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "shlax3",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "shlax4",         REG_AX,	       	      REG_AX | REG_TMP1		     },
    { "shrax1",         REG_AX,               REG_AX | REG_TMP1		     },
    { "shrax2",         REG_AX,               REG_AX | REG_TMP1		     },
    { "shrax3",         REG_AX,               REG_AX | REG_TMP1		     },
    { "shrax4",         REG_AX,               REG_AX | REG_TMP1		     },
    { "shreax1",        REG_EAX,              REG_AX | REG_TMP1		     },
    { "shreax2",        REG_EAX,              REG_AX | REG_TMP1		     },
    { "shreax3",        REG_EAX,              REG_AX | REG_TMP1		     },
    { "shreax4",        REG_EAX,              REG_AX | REG_TMP1		     },
    { "staspidx",       REG_A | REG_Y,        REG_Y | REG_TMP1 | REG_PTR1    },
    { "stax0sp",        REG_AX,               REG_Y   			     },
    { "staxysp",        REG_AXY,              REG_Y   			     },
    { "steax0sp",       REG_EAX,              REG_Y                          },
    { "steaxysp",       REG_EAXY,             REG_Y                          },
    { "subeq0sp",       REG_AX,               REG_AXY                        },
    { "subeqysp",       REG_AXY,              REG_AXY                        },
    { "tosadda0",       REG_A,                REG_AXY                        },
    { "tosaddax",       REG_AX,               REG_AXY                        },
    { "tosanda0",       REG_A,                REG_AXY                        },
    { "tosandax",       REG_AX,               REG_AXY                        },
    { "tosdiva0",       REG_AY,       	      REG_ALL 			     },
    { "tosdivax",       REG_AXY,              REG_ALL 			     },
    { "tosdiveax",      REG_EAXY,             REG_ALL 			     },
    { "toseqeax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosgeeax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosgteax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosicmp",       	REG_AX,	       	      REG_AXY | REG_SREG	     },
    { "toslcmp",        REG_EAX,       	      REG_A | REG_Y | REG_PTR1       },
    { "tosleeax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "toslteax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosmula0",       REG_AX,	       	      REG_ALL 			     },
    { "tosmulax",       REG_AX,	       	      REG_ALL 			     },
    { "tosmuleax",      REG_EAX,       	      REG_ALL 			     },
    { "tosneeax",       REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosshreax",      REG_EAX,       	      REG_EAXY | REG_PTR1 | REG_PTR2 },
    { "tossuba0",       REG_A,                REG_AXY                        },
    { "tossubax",       REG_AX,               REG_AXY                        },
    { "tossubeax",      REG_EAX,              REG_EAXY                       },
    { "tosugeeax",      REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosugteax",      REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosuleeax",      REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosulteax",      REG_EAX,       	      REG_AXY | REG_PTR1             },
    { "tosumula0",      REG_AX,	       	      REG_ALL			     },
    { "tosumulax",      REG_AX,	       	      REG_ALL			     },
    { "tosumuleax",     REG_EAX,       	      REG_ALL			     },
    { "tsteax",         REG_EAX,              REG_Y                          },
    { "utsteax",        REG_EAX,              REG_Y                          },
};
#define FuncInfoCount	(sizeof(FuncInfoTable) / sizeof(FuncInfoTable[0]))

/* Table with names of zero page locations used by the compiler */
static const ZPInfo ZPInfoTable[] = {
    {  	0, "ptr1",      REG_PTR1_LO, 	REG_PTR1        },
    {   0, "ptr1+1",    REG_PTR1_HI, 	REG_PTR1        },
    {  	0, "ptr2",      REG_PTR2_LO, 	REG_PTR2        },
    {   0, "ptr2+1",    REG_PTR2_HI, 	REG_PTR2        },
    {  	4, "ptr3",      REG_NONE,    	REG_NONE        },
    {  	4, "ptr4",      REG_NONE,    	REG_NONE        },
    {   7, "regbank",   REG_NONE,    	REG_NONE        },
    {   0, "regsave",   REG_SAVE_LO, 	REG_SAVE        },
    {   0, "regsave+1", REG_SAVE_HI, 	REG_SAVE        },
    {   0, "sp",        REG_SP_LO,     	REG_SP          },
    {   0, "sp+1",      REG_SP_HI,      REG_SP          },
    {   0, "sreg",      REG_SREG_LO, 	REG_SREG        },
    {	0, "sreg+1",    REG_SREG_HI, 	REG_SREG        },
    {   0, "tmp1",      REG_TMP1,    	REG_TMP1        },
    {   0, "tmp2",      REG_NONE,    	REG_NONE        },
    {   0, "tmp3",      REG_NONE,    	REG_NONE        },
    {   0, "tmp4",      REG_NONE,    	REG_NONE        },
};
#define ZPInfoCount    	(sizeof(ZPInfoTable) / sizeof(ZPInfoTable[0]))



/*****************************************************************************/
/*     	       	      	  	     Code 	    	 	       	     */
/*****************************************************************************/



static int CompareFuncInfo (const void* Key, const void* Info)
/* Compare function for bsearch */
{
    return strcmp (Key, ((const	FuncInfo*) Info)->Name);
}



void GetFuncInfo (const char* Name, unsigned short* Use, unsigned short* Chg)
/* For the given function, lookup register information and store it into
 * the given variables. If the function is unknown, assume it will use and
 * load all registers.
 */
{
    /* If the function name starts with an underline, it is an external
     * function. Search for it in the symbol table. If the function does
     * not start with an underline, it may be a runtime support function.
     * Search for it in the list of builtin functions.
     */
    if (Name[0] == '_') {

     	/* Search in the symbol table, skip the leading underscore */
     	SymEntry* E = FindGlobalSym (Name+1);

     	/* Did we find it in the top level table? */
     	if (E && IsTypeFunc (E->Type)) {

     	    /* A function may use the A or A/X registers if it is a fastcall
     	     * function. If it is not a fastcall function but a variadic one,
	     * it will use the Y register (the parameter size is passed here).
	     * In all other cases, no registers are used. However, we assume
	     * that any function will destroy all registers.
     	     */
     	    FuncDesc* D = E->V.F.Func;
     	    if ((D->Flags & FD_FASTCALL) != 0 && D->ParamCount > 0) {
     		/* Will use registers depending on the last param */
     		SymEntry* LastParam = D->SymTab->SymTail;
                unsigned LastParamSize = CheckedSizeOf (LastParam->Type);
     		if (LastParamSize == 1) {
     		    *Use = REG_A;
     		} else if (LastParamSize == 2) {
     		    *Use = REG_AX;
     		} else {
                    *Use = REG_EAX;
                }
	    } else if ((D->Flags & FD_VARIADIC) != 0) {
		*Use = REG_Y;
     	    } else {
     		/* Will not use any registers */
     		*Use = REG_NONE;
     	    }

     	    /* Will destroy all registers */
     	    *Chg = REG_ALL;

     	    /* Done */
     	    return;
     	}

    } else {

     	/* Search for the function in the list of builtin functions */
	const FuncInfo* Info = bsearch (Name, FuncInfoTable, FuncInfoCount,
					sizeof(FuncInfo), CompareFuncInfo);

	/* Do we know the function? */
	if (Info) {
	    /* Use the information we have */
	    *Use = Info->Use;
	    *Chg = Info->Chg;
	    return;
	}
    }

    /* Function not found - assume that the primary register is input, and all
     * registers are changed
     */
    *Use = REG_EAXY;
    *Chg = REG_ALL;
}



static int CompareZPInfo (const void* Name, const void* Info)
/* Compare function for bsearch */
{
    /* Cast the pointers to the correct data type */
    const char* N   = (const char*) Name;
    const ZPInfo* E = (const ZPInfo*) Info;

    /* Do the compare. Be careful because of the length (Info may contain
     * more than just the zeropage name).
     */
    if (E->Len == 0) {
	/* Do a full compare */
	return strcmp (N, E->Name);
    } else {
	/* Only compare the first part */
	int Res = strncmp (N, E->Name, E->Len);
	if (Res == 0 && (N[E->Len] != '\0' && N[E->Len] != '+')) {
	    /* Name is actually longer than Info->Name */
	    Res = -1;
	}
	return Res;
    }
}



const ZPInfo* GetZPInfo (const char* Name)
/* If the given name is a zero page symbol, return a pointer to the info
 * struct for this symbol, otherwise return NULL.
 */
{
    /* Search for the zp location in the list */
    return bsearch (Name, ZPInfoTable, ZPInfoCount,
	       	    sizeof(ZPInfo), CompareZPInfo);
}



static unsigned GetRegInfo2 (CodeSeg* S,
		      	     CodeEntry* E,
		    	     int Index,
		     	     Collection* Visited,
		     	     unsigned Used,
		     	     unsigned Unused,
			     unsigned Wanted)
/* Recursively called subfunction for GetRegInfo. */
{
    /* Follow the instruction flow recording register usage. */
    while (1) {

	unsigned R;

	/* Check if we have already visited the current code entry. If so,
	 * bail out.
	 */
	if (CE_HasMark (E)) {
	    break;
	}

	/* Mark this entry as already visited */
	CE_SetMark (E);
	CollAppend (Visited, E);

	/* Evaluate the used registers */
	R = E->Use;
	if (E->OPC == OP65_RTS ||
	    ((E->Info & OF_BRA) != 0 && E->JumpTo == 0)) {
	    /* This instruction will leave the function */
	    R |= S->ExitRegs;
	}
       	if (R != REG_NONE) {
	    /* We are not interested in the use of any register that has been
	     * used before.
	     */
	    R &= ~Unused;
	    /* Remember the remaining registers */
	    Used |= R;
	}

	/* Evaluate the changed registers */
       	if ((R = E->Chg) != REG_NONE) {
	    /* We are not interested in the use of any register that has been
	     * used before.
	     */
	    R &= ~Used;
	    /* Remember the remaining registers */
	    Unused |= R;
	}

       	/* If we know about all registers now, bail out */
       	if (((Used | Unused) & Wanted) == Wanted) {
    	    break;
    	}

	/* If the instruction is an RTS or RTI, we're done */
       	if ((E->Info & OF_RET) != 0) {
	    break;
	}

	/* If we have an unconditional branch, follow this branch if possible,
	 * otherwise we're done.
	 */
	if ((E->Info & OF_UBRA) != 0) {

	    /* Does this jump have a valid target? */
	    if (E->JumpTo) {

	       	/* Unconditional jump */
 	       	E     = E->JumpTo->Owner;
		Index = -1;	     	/* Invalidate */

	    } else {
	       	/* Jump outside means we're done */
	       	break;
	    }

       	/* In case of conditional branches, follow the branch if possible and
	 * follow the normal flow (branch not taken) afterwards. If we cannot
	 * follow the branch, we're done.
	 */
	} else if ((E->Info & OF_CBRA) != 0) {

    	    if (E->JumpTo) {

	       	/* Recursively determine register usage at the branch target */
		unsigned U1;
		unsigned U2;

		U1 = GetRegInfo2 (S, E->JumpTo->Owner, -1, Visited, Used, Unused, Wanted);
		if (U1 == REG_ALL) {
		    /* All registers used, no need for second call */
		    return REG_AXY;
		}
		if (Index < 0) {
		    Index = CS_GetEntryIndex (S, E);
		}
       	       	if ((E = CS_GetEntry (S, ++Index)) == 0) {
		    Internal ("GetRegInfo2: No next entry!");
		}
       	       	U2 = GetRegInfo2 (S, E, Index, Visited, Used, Unused, Wanted);
	   	return U1 | U2;	       	/* Used in any of the branches */

	    } else {
	   	/* Jump to global symbol */
	  	break;
	    }

       	} else {

	    /* Just go to the next instruction */
	    if (Index < 0) {
	     	Index = CS_GetEntryIndex (S, E);
	    }
	    E = CS_GetEntry (S, ++Index);
	    if (E == 0) {
	     	/* No next entry */
	     	Internal ("GetRegInfo2: No next entry!");
	    }

	}

    }

    /* Return to the caller the complement of all unused registers */
    return Used;
}



static unsigned GetRegInfo1 (CodeSeg* S,
		      	     CodeEntry* E,
		   	     int Index,
		     	     Collection* Visited,
		     	     unsigned Used,
		     	     unsigned Unused,
			     unsigned Wanted)
/* Recursively called subfunction for GetRegInfo. */
{
    /* Remember the current count of the line collection */
    unsigned Count = CollCount (Visited);

    /* Call the worker routine */
    unsigned R = GetRegInfo2 (S, E, Index, Visited, Used, Unused, Wanted);

    /* Restore the old count, unmarking all new entries */
    unsigned NewCount = CollCount (Visited);
    while (NewCount-- > Count) {
	CodeEntry* E = CollAt (Visited, NewCount);
	CE_ResetMark (E);
	CollDelete (Visited, NewCount);
    }

    /* Return the registers used */
    return R;
}



unsigned GetRegInfo (struct CodeSeg* S, unsigned Index, unsigned Wanted)
/* Determine register usage information for the instructions starting at the
 * given index.
 */
{
    CodeEntry* 	    E;
    Collection 	    Visited;	/* Visited entries */
    unsigned        R;

    /* Get the code entry for the given index */
    if (Index >= CS_GetEntryCount (S)) {
	/* There is no such code entry */
	return REG_NONE;
    }
    E = CS_GetEntry (S, Index);

    /* Initialize the data structure used to collection information */
    InitCollection (&Visited);

    /* Call the recursive subfunction */
    R = GetRegInfo1 (S, E, Index, &Visited, REG_NONE, REG_NONE, Wanted);

    /* Delete the line collection */
    DoneCollection (&Visited);

    /* Return the registers used */
    return R;
}



int RegAUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in A is used. */
{
    return (GetRegInfo (S, Index, REG_A) & REG_A) != 0;
}



int RegXUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in X is used. */
{
    return (GetRegInfo (S, Index, REG_X) & REG_X) != 0;
}



int RegYUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in Y is used. */
{
    return (GetRegInfo (S, Index, REG_Y) & REG_Y) != 0;
}



int RegAXUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in A or(!) the value in X are used. */
{
    return (GetRegInfo (S, Index, REG_AX) & REG_AX) != 0;
}



int RegEAXUsed (struct CodeSeg* S, unsigned Index)
/* Check if any of the four bytes in EAX are used. */
{
    return (GetRegInfo (S, Index, REG_EAX) & REG_EAX) != 0;
}



unsigned GetKnownReg (unsigned Use, const RegContents* RC)
/* Return the register or zero page location from the set in Use, thats
 * contents are known. If Use does not contain any register, or if the
 * register in question does not have a known value, return REG_NONE.
 */
{
    if ((Use & REG_A) != 0) {
	return (RC == 0 || RC->RegA >= 0)? REG_A : REG_NONE;
    } else if ((Use & REG_X) != 0) {
	return (RC == 0 || RC->RegX >= 0)? REG_X : REG_NONE;
    } else if ((Use & REG_Y) != 0) {
	return (RC == 0 || RC->RegY >= 0)? REG_Y : REG_NONE;
    } else if ((Use & REG_TMP1) != 0) {
	return (RC == 0 || RC->Tmp1 >= 0)? REG_TMP1 : REG_NONE;
    } else if ((Use & REG_SREG_LO) != 0) {
	return (RC == 0 || RC->SRegLo >= 0)? REG_SREG_LO : REG_NONE;
    } else if ((Use & REG_SREG_HI) != 0) {
	return (RC == 0 || RC->SRegHi >= 0)? REG_SREG_HI : REG_NONE;
    } else {
	return REG_NONE;
    }
}



static cmp_t FindCmpCond (const char* Code, unsigned CodeLen)
/* Search for a compare condition by the given code using the given length */
{
    unsigned I;

    /* Linear search */
    for (I = 0; I < sizeof (CmpSuffixTab) / sizeof (CmpSuffixTab [0]); ++I) {
	if (strncmp (Code, CmpSuffixTab [I], CodeLen) == 0) {
	    /* Found */
	    return I;
	}
    }

    /* Not found */
    return CMP_INV;
}



cmp_t FindBoolCmpCond (const char* Name)
/* Check if the given string is the name of one of the boolean transformer
 * subroutine, and if so, return the condition that is evaluated by this
 * routine. Return CMP_INV if the condition is not recognised.
 */
{
    /* Check for the correct subroutine name */
    if (strncmp (Name, "bool", 4) == 0) {
	/* Name is ok, search for the code in the table */
	return FindCmpCond (Name+4, strlen(Name)-4);
    } else {
	/* Not found */
	return CMP_INV;
    }
}



cmp_t FindTosCmpCond (const char* Name)
/* Check if this is a call to one of the TOS compare functions (tosgtax).
 * Return the condition code or CMP_INV on failure.
 */
{
    unsigned Len = strlen (Name);

    /* Check for the correct subroutine name */
    if (strncmp (Name, "tos", 3) == 0 && strcmp (Name+Len-2, "ax") == 0) {
	/* Name is ok, search for the code in the table */
	return FindCmpCond (Name+3, Len-3-2);
    } else {
	/* Not found */
	return CMP_INV;
    }
}




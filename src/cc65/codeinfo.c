/*****************************************************************************/
/*                                                                           */
/*				  codeinfo.c				     */
/*                                                                           */
/*		    Additional information about 6502 code		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2001      Ullrich von Bassewitz                                       */
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
#include "error.h"
#include "codeinfo.h"



/*****************************************************************************/
/*     	       	      	  	     Data				     */
/*****************************************************************************/



/* Table listing the function names and code info values for known internally
 * used functions. This table should get auto-generated in the future.
 */
typedef struct FuncInfo FuncInfo;
struct FuncInfo {
    const char*	    Name;	/* Function name */
    unsigned char   Use;	/* Register usage */
    unsigned char   Chg;	/* Changed/destroyed registers */
};

static const FuncInfo FuncInfoTable[] = {
    { "addysp",	       	REG_Y, 	       	REG_NONE	},
    { "booleq",		REG_NONE,    	REG_AX		},
    { "boolge",		REG_NONE,	REG_AX		},
    { "boolgt",		REG_NONE,	REG_AX		},
    { "boolle",		REG_NONE,	REG_AX		},
    { "boollt",		REG_NONE,	REG_AX		},
    { "boolne",		REG_NONE,	REG_AX		},
    { "booluge",   	REG_NONE,	REG_AX		},
    { "boolugt",   	REG_NONE,	REG_AX		},
    { "boolule",   	REG_NONE,	REG_AX		},
    { "boolult",   	REG_NONE,	REG_AX		},
    { "decax1",     	REG_AX,		REG_AX  	},
    { "decax2",     	REG_AX,		REG_AX  	},
    { "decax3",     	REG_AX,		REG_AX  	},
    { "decax4",     	REG_AX,		REG_AX  	},
    { "decax5",     	REG_AX,		REG_AX  	},
    { "decax6",     	REG_AX,		REG_AX  	},
    { "decax7",     	REG_AX,		REG_AX  	},
    { "decax8",     	REG_AX,		REG_AX  	},
    { "decaxy",	   	REG_AXY,	REG_AX		},
    { "decsp2",    	REG_NONE,	REG_A		},
    { "decsp3",    	REG_NONE,	REG_A		},
    { "decsp4",    	REG_NONE,	REG_A		},
    { "decsp5",    	REG_NONE,	REG_A		},
    { "decsp6",     	REG_NONE,	REG_A		},
    { "decsp7",    	REG_NONE,	REG_A		},
    { "decsp8",      	REG_NONE,	REG_A		},
    { "incsp1",		REG_NONE,	REG_NONE	},
    { "incsp2",		REG_NONE,	REG_Y		},
    { "incsp3",		REG_NONE,	REG_Y		},
    { "incsp4",		REG_NONE,	REG_Y		},
    { "incsp5",		REG_NONE,	REG_Y		},
    { "incsp6",		REG_NONE,	REG_Y		},
    { "incsp7",		REG_NONE,	REG_Y		},
    { "incsp8",		REG_NONE,	REG_Y		},
    { "ldax0sp",   	REG_Y,		REG_AX		},
    { "ldaxysp",   	REG_Y,		REG_AX 		},
    { "pusha",	   	REG_A,		REG_Y		},
    { "pusha0",	       	REG_A,		REG_XY		},
    { "pushax",	   	REG_AX,		REG_Y		},
    { "pushw0sp",  	REG_NONE,	REG_AXY		},
    { "pushwysp",  	REG_Y,		REG_AXY		},
    { "tosicmp",   	REG_AX,		REG_AXY 	},
};
#define FuncInfoCount	(sizeof(FuncInfoTable) / sizeof(FuncInfoTable[0]))



/*****************************************************************************/
/*     	       	      	  	     Code 		 		     */
/*****************************************************************************/



static int CompareFuncInfo (const void* Key, const void* Info)
/* Compare function for bsearch */
{
    return strcmp (Key, ((const	FuncInfo*) Info)->Name);
}



void GetFuncInfo (const char* Name, unsigned char* Use, unsigned char* Chg)
/* For the given function, lookup register information and combine it with
 * the information already in place. If the function is unknown, assume it
 * will use all registers and load all registers.
 * See codeinfo.h for possible flags.
 */
{
    /* Search for the function */
    const FuncInfo* Info = bsearch (Name, FuncInfoTable, FuncInfoCount,
			  	    sizeof(FuncInfo), CompareFuncInfo);

    /* Do we know the function? */
    if (Info) {
	/* Use the information we have */
	*Use |= Info->Use;
	*Chg |= Info->Chg;
    } else {
	/* Assume all registers used */
	*Use |= REG_AXY;
	*Chg |= REG_AXY;
    }
}



static unsigned char GetRegInfo2 (CodeSeg* S,
		    		  CodeEntry* E,
		    		  int Index,
		     		  Collection* Visited,
		     		  unsigned char Used,
		     		  unsigned char Unused)
/* Recursively called subfunction for GetRegInfo. */
{
    /* Follow the instruction flow recording register usage. */
    while (1) {

	unsigned char R;

	/* Check if we have already visited the current code entry. If so,
	 * bail out.
	 */
	if (CodeEntryHasMark (E)) {
	    break;
	}

	/* Mark this entry as already visited */
	CodeEntrySetMark (E);
	CollAppend (Visited, E);

	/* Evaluate the used registers */
	if ((R = E->Use) != REG_NONE) {
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
       	if ((Used | Unused) == REG_AXY) {
    	    break;
    	}

	/* If the instruction is an RTS or RTI, we're done */
	if (E->OPC == OPC_RTS || E->OPC == OPC_RTI) {
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
		Index = -1;	  	/* Invalidate */

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
		unsigned char U1;
		unsigned char U2;

		U1 = GetRegInfo2 (S, E->JumpTo->Owner, -1, Visited, Used, Unused);
		if (U1 == REG_AXY) {
		    /* All registers used, no need for second call */
		    return REG_AXY;
		}
		if (Index < 0) {
		    Index = GetCodeEntryIndex (S, E);
		}
       	       	if ((E = GetCodeEntry (S, ++Index)) == 0) {
		    Internal ("GetRegInfo2: No next entry!");
		}
		U2 = GetRegInfo2 (S, E, Index, Visited, Used, Unused);
	   	return U1 | U2;	       	/* Used in any of the branches */

	    } else {
	   	/* Jump to global symbol */
	  	break;
	    }

       	} else {

	    /* Just go to the next instruction */
	    if (Index < 0) {
	     	Index = GetCodeEntryIndex (S, E);
	    }
	    E = GetCodeEntry (S, ++Index);
	    if (E == 0) {
	     	/* No next entry */
	     	Internal ("GetRegInfo2: No next entry!");
	    }

	}

    }

    /* Return to the caller the complement of all unused registers */
    return Used;
}



static unsigned char GetRegInfo1 (CodeSeg* S,
		  		  CodeEntry* E,
		  		  int Index,
		     		  Collection* Visited,
		     		  unsigned char Used,
		     		  unsigned char Unused)
/* Recursively called subfunction for GetRegInfo. */
{
    /* Remember the current count of the line collection */
    unsigned Count = CollCount (Visited);

    /* Call the worker routine */
    unsigned char R = GetRegInfo2 (S, E, Index, Visited, Used, Unused);

    /* Restore the old count, unmarking all new entries */
    unsigned NewCount = CollCount (Visited);
    while (NewCount-- > Count) {
	CodeEntry* E = CollAt (Visited, NewCount);
	CodeEntryResetMark (E);
	CollDelete (Visited, NewCount);
    }

    /* Return the registers used */
    return R;
}



unsigned char GetRegInfo (struct CodeSeg* S, unsigned Index)
/* Determine register usage information for the instructions starting at the
 * given index.
 */
{
    CodeEntry*	    E;
    Collection	    Visited;	/* Visited entries */
    unsigned char   R;

    /* Get the code entry for the given index */
    if (Index >= GetCodeEntryCount (S)) {
	/* There is no such code entry */
	return REG_NONE;
    }
    E = GetCodeEntry (S, Index);

    /* Initialize the data structure used to collection information */
    InitCollection (&Visited);

    /* Call the recursive subfunction */
    R = GetRegInfo1 (S, E, Index, &Visited, REG_NONE, REG_NONE);

    /* Delete the line collection */
    DoneCollection (&Visited);

    /* Return the registers used */
    return R;
}



int RegAUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in A is used. */
{
    return (GetRegInfo (S, Index) & REG_A) != 0;
}



int RegXUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in X is used. */
{
    return (GetRegInfo (S, Index) & REG_X) != 0;
}



int RegYUsed (struct CodeSeg* S, unsigned Index)
/* Check if the value in Y is used. */
{
    return (GetRegInfo (S, Index) & REG_Y) != 0;
}




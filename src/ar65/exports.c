/*****************************************************************************/
/*                                                                           */
/*				   exports.c				     */
/*                                                                           */
/*		Duplicate export checking for the ar65 archiver		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998     Ullrich von Bassewitz                                        */
/*              Wacholderweg 14                                              */
/*              D-70597 Stuttgart                                            */
/* EMail:       uz@musoftware.de                                             */
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



#include <string.h>

#include "../common/hashstr.h"

#include "mem.h"
#include "error.h"
#include "objdata.h"
#include "exports.h"



/*****************************************************************************/
/*     	      	    		     Data				     */
/*****************************************************************************/



/* A hash table entry */
typedef struct HashEntry_ HashEntry;
struct HashEntry_ {
    HashEntry* 		Next;		/* Next in list */
    unsigned		Module;	       	/* Module index */
    char		Name [1];	/* Name of identifier */
};

/* Hash table */
#define HASHTAB_SIZE	4783
static HashEntry*	HashTab [HASHTAB_SIZE];



/*****************************************************************************/
/*     	      	    		     Code  		  		     */
/*****************************************************************************/



static HashEntry* NewHashEntry (const char* Name, unsigned Module)
/* Create and return a new hash entry */
{
    /* Get the length of the name */
    unsigned Len = strlen (Name);

    /* Get memory for the struct */
    HashEntry* H = Xmalloc (sizeof (HashEntry) + Len);

    /* Initialize the fields and return it */
    H->Next	= 0;
    H->Module	= Module;
    memcpy (H->Name, Name, Len);
    H->Name [Len] = '\0';
    return H;
}



void ExpInsert (const char* Name, unsigned Module)
/* Insert an exported identifier and check if it's already in the list */
{
    HashEntry* L;

    /* Create a hash value for the given name */
    unsigned HashVal = HashStr (Name) % HASHTAB_SIZE;

    /* Create a new hash entry */
    HashEntry* H = NewHashEntry (Name, Module);

    /* Search through the list in that slot and print matching duplicates */
    if (HashTab [HashVal] == 0) {
    	/* The slot is empty */
    	HashTab [HashVal] = H;
    	return;
    }
    L = HashTab [HashVal];
    while (1) {
	if (strcmp (L->Name, Name) == 0) {
	    /* Duplicate entry */
	    Warning ("External symbol `%s' in module `%s' is duplicated in "
	       	     "module `%s'",
		     Name, GetObjName (L->Module), GetObjName (Module));
	}
     	if (L->Next == 0) {
     	    break;
     	} else {
     	    L = L->Next;
     	}
    }
    L->Next = H;
}



int ExpFind (const char* Name)
/* Check for an identifier in the list. Return -1 if not found, otherwise
 * return the number of the module, that exports the identifer.
 */
{
    /* Get a pointer to the list with the symbols hash value */
    HashEntry* L = HashTab [HashStr (Name) % HASHTAB_SIZE];
    while (L) {
        /* Search through the list in that slot */
	if (strcmp (L->Name, Name) == 0) {
	    /* Entry found */
	    return L->Module;
	}
	L = L->Next;
    }

    /* Not found */
    return -1;
}




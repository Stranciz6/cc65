/*****************************************************************************/
/*                                                                           */
/*			   	  symentry.c				     */
/*                                                                           */
/*	    Symbol table entry forward for the ca65 macroassembler	     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2003 Ullrich von Bassewitz                                       */
/*               Römerstraße 52                                              */
/*               D-70794 Filderstadt                                         */
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



#include <string.h>

/* common */
#include "addrsize.h"
#include "xmalloc.h"

/* ca65 */
#include "error.h"
#include "expr.h"
#include "global.h"
#include "scanner.h"
#include "segment.h"
#include "spool.h"
#include "symentry.h"
#include "symtab.h"



/*****************************************************************************/
/*     	      	    		     Data				     */
/*****************************************************************************/



/* List of all symbol table entries */
SymEntry* SymList = 0;

/* Pointer to last defined symbol */
SymEntry* SymLast = 0;



/*****************************************************************************/
/*     	       	   	  	     Code			   	     */
/*****************************************************************************/



int IsLocalName (const char* Name)
/* Return true if Name is the name of a local symbol */
{
    return (*Name == LocalStart);
}



int IsLocalNameId (unsigned Name)
/* Return true if Name is the name of a local symbol */
{
    return (*GetString (Name) == LocalStart);
}



static unsigned SymAddrSize (const SymEntry* S)
/* Get the default address size for a symbol. */
{
    /* Local symbols are always near (is this ok?) */
    if (IsLocalNameId (S->Name)) {
        return ADDR_SIZE_ABS;
    }

    /* Return the address size of the segment */
    return GetCurrentSegAddrSize ();
}



SymEntry* NewSymEntry (const char* Name)
/* Allocate a symbol table entry, initialize and return it */
{
    /* Allocate memory */
    SymEntry* S = xmalloc (sizeof (SymEntry));

    /* Initialize the entry */
    S->Left	  = 0;
    S->Right	  = 0;
    S->Locals	  = 0;
    S->SymTab	  = 0;
    S->Pos	  = CurPos;
    S->Flags	  = 0;
    S->V.Expr	  = 0;
    S->ExprRefs   = AUTO_COLLECTION_INITIALIZER;
    S->ExportSize = ADDR_SIZE_DEFAULT;
    S->AddrSize   = ADDR_SIZE_DEFAULT;
    memset (S->ConDesPrio, 0, sizeof (S->ConDesPrio));
    S->Name       = GetStringId (Name);

    /* Insert it into the list of all entries */
    S->List = SymList;
    SymList = S;

    /* Return the initialized entry */
    return S;
}



int SymSearchTree (SymEntry* T, const char* Name, SymEntry** E)
/* Search in the given tree for a name. If we find the symbol, the function
 * will return 0 and put the entry pointer into E. If we did not find the
 * symbol, and the tree is empty, E is set to NULL. If the tree is not empty,
 * E will be set to the last entry, and the result of the function is <0 if
 * the entry should be inserted on the left side, and >0 if it should get
 * inserted on the right side.
 */
{
    /* Is there a tree? */
    if (T == 0) {
      	*E = 0;
      	return 1;
    }

    /* We have a table, search it */
    while (1) {

        /* Get the symbol name */
        const char* SymName = GetString (T->Name);

      	/* Choose next entry */
        int Cmp = strcmp (Name, SymName);
       	if (Cmp < 0 && T->Left) {
	    T = T->Left;
	} else if (Cmp > 0&& T->Right) {
	    T = T->Right;
	} else {
     	    /* Found or end of search, return the result */
            *E = T;
            return Cmp;
       	}
    }
}



void SymRef (SymEntry* S)
/* Mark the given symbol as referenced */
{
    /* Mark the symbol as referenced */
    S->Flags |= SF_REFERENCED;
}



void SymDef (SymEntry* S, ExprNode* Expr, unsigned char AddrSize, unsigned Flags)
/* Define a new symbol */
{
    if (S->Flags & SF_IMPORT) {
       	/* Defined symbol is marked as imported external symbol */
       	Error ("Symbol `%s' is already an import", GetSymName (S));
       	return;
    }
    if (S->Flags & SF_DEFINED) {
       	/* Multiple definition */
       	Error ("Symbol `%s' is already defined", GetSymName (S));
       	S->Flags |= SF_MULTDEF;
       	return;
    }

    /* Map a default address size to a real value */
    if (AddrSize == ADDR_SIZE_DEFAULT) {
        long Val;
        if (IsConstExpr (Expr, &Val)) {
            if (IsByteRange (Val)) {
                AddrSize = ADDR_SIZE_ZP;
            } else if (IsWordRange (Val)) {
                AddrSize = ADDR_SIZE_ABS;
            } else if (IsFarRange (Val)) {
                AddrSize = ADDR_SIZE_FAR;
            } else {
                AddrSize = ADDR_SIZE_LONG;
            }
        } else {
            AddrSize = SymAddrSize (S);
        }
    }

    /* Set the symbol value */
    S->V.Expr = Expr;

    /* If the symbol is marked as global, export it */
    if (S->Flags & SF_GLOBAL) {
        S->ExportSize = S->AddrSize;
        S->Flags = (S->Flags & ~SF_GLOBAL) | SF_EXPORT;
    }

    /* Mark the symbol as defined and use the given address size */
    S->Flags |= (SF_DEFINED | Flags);
    S->AddrSize = AddrSize;

    /* If the symbol is exported, check the address sizes */
    if (S->Flags & SF_EXPORT) {
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            /* Use the real size of the symbol */
            S->ExportSize = S->AddrSize;
        } else if (S->AddrSize > S->ExportSize) {
            PWarning (GetSymPos (S), 1, "Symbol `%s' is %s but exported as %s",
                      GetSymName (S), AddrSizeToStr (S->AddrSize),
                      AddrSizeToStr (S->ExportSize));
        }
    }

    /* If the symbol is a ZP symbol, check if the value is in correct range */
    if (S->AddrSize == ADDR_SIZE_ZP) {
     	/* Already marked as ZP symbol by some means */
     	if (!IsByteExpr (Expr)) {
     	    Error ("Range error");
     	}
    }

    /* If this is not a local symbol, remember it as the last global one */
    if (!IsLocalNameId (S->Name)) {
       	SymLast = S;
    }
}



void SymImport (SymEntry* S, unsigned char AddrSize, unsigned Flags)
/* Mark the given symbol as an imported symbol */
{
    /* Don't accept local symbols */
    if (IsLocalNameId (S->Name)) {
     	Error ("Illegal use of a local symbol");
     	return;
    }

    if (S->Flags & SF_DEFINED) {
     	Error ("Symbol `%s' is already defined", GetSymName (S));
     	S->Flags |= SF_MULTDEF;
     	return;
    }
    if (S->Flags & SF_EXPORT) {
     	/* The symbol is already marked as exported symbol */
     	Error ("Cannot import exported symbol `%s'", GetSymName (S));
     	return;
    }

    /* Map a default address size to a real value */
    if (AddrSize == ADDR_SIZE_DEFAULT) {
        AddrSize = SymAddrSize (S);
    }

    /* If the symbol is marked as import or global, check the symbol flags,
     * then do silently remove the global flag
     */
    if (S->Flags & SF_IMPORT) {
     	if ((Flags & SF_FORCED) != (S->Flags & SF_FORCED)) {
       	    Error ("Redeclaration mismatch for symbol `%s'", GetSymName (S));
     	}
        if (AddrSize != S->AddrSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
    }
    if (S->Flags & SF_GLOBAL) {
        if (S->AddrSize != ADDR_SIZE_DEFAULT && S->AddrSize != AddrSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
     	}
        S->Flags &= ~SF_GLOBAL;
    }

    /* Set the symbol data */
    S->Flags |= (SF_IMPORT | Flags);
    S->AddrSize = AddrSize;
}



void SymExport (SymEntry* S, unsigned char AddrSize, unsigned Flags)
/* Mark the given symbol as an exported symbol */
{
    /* Don't accept local symbols */
    if (IsLocalNameId (S->Name)) {
     	Error ("Illegal use of a local symbol");
     	return;
    }

    /* Check if it's ok to export the symbol */
    if (S->Flags & SF_IMPORT) {
     	/* The symbol is already marked as imported external symbol */
     	Error ("Symbol `%s' is already an import", GetSymName (S));
     	return;
    }

    /* If the symbol was marked as global before, make it an export */
    if (S->Flags & SF_GLOBAL) {
        S->ExportSize = S->AddrSize;
        S->Flags &= ~SF_GLOBAL;
    }

    /* If the symbol was already marked as an export, check if this was done
     * specifiying the same address size. If the old spec had no explicit
     * address size, use the new one.
     */
    if (S->Flags & SF_EXPORT) {
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            S->ExportSize = AddrSize;
        } else if (AddrSize == ADDR_SIZE_DEFAULT) {
            AddrSize = S->ExportSize;
        }
        if (S->ExportSize != ADDR_SIZE_DEFAULT && S->ExportSize != AddrSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
    }
    S->ExportSize = AddrSize;

    /* If the symbol is already defined, check symbol size against the
     * exported size.
     */
    if (S->Flags & SF_DEFINED) {
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            /* No export size given, use the real size of the symbol */
            S->ExportSize = S->AddrSize;
        } else if (S->AddrSize > S->ExportSize) {
            Warning (1, "Symbol `%s' is %s but exported as %s",
                     GetSymName (S), AddrSizeToStr (S->AddrSize),
                     AddrSizeToStr (S->ExportSize));
        }
    }

    /* Set the symbol data */
    S->Flags |= (SF_EXPORT | SF_REFERENCED | Flags);
}



void SymGlobal (SymEntry* S, unsigned char AddrSize, unsigned Flags)
/* Mark the given symbol as a global symbol, that is, as a symbol that is
 * either imported or exported.
 */
{
    /* Don't accept local symbols */
    if (IsLocalNameId (S->Name)) {
     	Error ("Illegal use of a local symbol");
     	return;
    }

    /* If the symbol is already marked as import or export, check the
     * size of the definition, then bail out.
     */
    if (S->Flags & SF_IMPORT) {
        if (AddrSize != ADDR_SIZE_DEFAULT && AddrSize != S->AddrSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
        return;
    }
    if (S->Flags & SF_EXPORT) {
        /* If the old symbol had no explicit address size spec, use the
         * new one.
         */
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            S->ExportSize = AddrSize;
        }
        if (AddrSize != S->ExportSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
        return;
    }

    /* If the symbol is already defined, export it. Otherwise mark it as
     * global.
     */
    if (S->Flags & SF_DEFINED) {
        /* The symbol is defined, export it */
        S->ExportSize = AddrSize;
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            /* No export size given, use the real size of the symbol */
            S->ExportSize = S->AddrSize;
        } else if (S->AddrSize > S->ExportSize) {
            Warning (1, "Symbol `%s' is %s but exported as %s",
                     GetSymName (S), AddrSizeToStr (S->AddrSize),
                     AddrSizeToStr (S->ExportSize));
        }
        S->Flags |= (SF_EXPORT | Flags);
        S->ExportSize = AddrSize;
    } else {
        S->Flags |= (SF_GLOBAL | Flags);
        S->AddrSize = AddrSize;
    }
}



void SymConDes (SymEntry* S, unsigned char AddrSize, unsigned Type, unsigned Prio)
/* Mark the given symbol as a module constructor/destructor. This will also
 * mark the symbol as an export. Initializers may never be zero page symbols.
 */
{
    /* Check the parameters */
#if (CD_TYPE_MIN != 0)
    CHECK (Type >= CD_TYPE_MIN && Type <= CD_TYPE_MAX);
#else
    CHECK (Type <= CD_TYPE_MAX);
#endif
    CHECK (Prio >= CD_PRIO_MIN && Prio <= CD_PRIO_MAX);

    /* Don't accept local symbols */
    if (IsLocalNameId (S->Name)) {
       	Error ("Illegal use of a local symbol");
       	return;
    }

    /* Check for errors */
    if (S->Flags & SF_IMPORT) {
       	/* The symbol is already marked as imported external symbol */
       	Error ("Symbol `%s' is already an import", GetSymName (S));
       	return;
    }

    /* If the symbol was already marked as an export or global, check if
     * this was done specifiying the same address size. In case of a global
     * declaration, silently remove the global flag.
     */
    if (S->Flags & (SF_EXPORT | SF_GLOBAL)) {
        if (S->ExportSize != AddrSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
        S->Flags &= ~SF_GLOBAL;
    }
    S->ExportSize = AddrSize;

    /* If the symbol is already defined, check symbol size against the
     * exported size.
     */
    if (S->Flags & SF_DEFINED) {
        if (S->ExportSize == ADDR_SIZE_DEFAULT) {
            /* Use the real size of the symbol */
            S->ExportSize = S->AddrSize;
        } else if (S->AddrSize != S->ExportSize) {
            Error ("Address size mismatch for symbol `%s'", GetSymName (S));
        }
    }

    /* If the symbol was already declared as a condes, check if the new
     * priority value is the same as the old one.
     */
    if (S->ConDesPrio[Type] != CD_PRIO_NONE) {
	if (S->ConDesPrio[Type] != Prio) {
	    Error ("Redeclaration mismatch for symbol `%s'", GetSymName (S));
	}
    }
    S->ConDesPrio[Type] = Prio;

    /* Set the symbol data */
    S->Flags |= (SF_EXPORT | SF_REFERENCED);
}



int SymIsDef (const SymEntry* S)
/* Return true if the given symbol is already defined */
{
    return (S->Flags & SF_DEFINED) != 0;
}



int SymIsRef (const SymEntry* S)
/* Return true if the given symbol has been referenced */
{
    return (S->Flags & SF_REFERENCED) != 0;
}



int SymIsImport (const SymEntry* S)
/* Return true if the given symbol is marked as import */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    /* Check the import flag */
    return (S->Flags & SF_IMPORT) != 0;
}



int SymIsConst (SymEntry* S, long* Val)
/* Return true if the given symbol has a constant value. If Val is not NULL
 * and the symbol has a constant value, store it's value there.
 */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
    	S = S->V.Sym;
    }

    /* Check for constness */
    return (SymHasExpr (S) && IsConstExpr (S->V.Expr, Val));
}



int SymHasExpr (const SymEntry* S)
/* Return true if the given symbol has an associated expression */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    /* Check the expression */
    return ((S->Flags & (SF_DEFINED|SF_IMPORT)) == SF_DEFINED);
}



void SymMarkUser (SymEntry* S)
/* Set a user mark on the specified symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
       	S = S->V.Sym;
    }

    /* Set the bit */
    S->Flags |= SF_USER;
}



void SymUnmarkUser (SymEntry* S)
/* Remove a user mark from the specified symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    /* Reset the bit */
    S->Flags &= ~SF_USER;
}



int SymHasUserMark (SymEntry* S)
/* Return the state of the user mark for the specified symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    /* Check the bit */
    return (S->Flags & SF_USER) != 0;
}



struct ExprNode* GetSymExpr (SymEntry* S)
/* Get the expression for a non-const symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    PRECONDITION (S != 0 && SymHasExpr (S));
    return S->V.Expr;
}



const struct ExprNode* SymResolve (const SymEntry* S)
/* Helper function for DumpExpr. Resolves a symbol into an expression or return
 * NULL. Do not call in other contexts!
 */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }

    return SymHasExpr (S)? S->V.Expr : 0;
}



const char* GetSymName (const SymEntry* S)
/* Return the name of the symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }
    return GetString (S->Name);
}



unsigned char GetSymAddrSize (const SymEntry* S)
/* Return the address size of the symbol. Beware: This function will just
 * return the AddrSize member, it will not look at the expression!
 */
{
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }
    return S->AddrSize;
}



long GetSymVal (SymEntry* S)
/* Return the value of a symbol assuming it's constant. FAIL will be called
 * in case the symbol is undefined or not constant.
 */
{
    long Val;
    CHECK (S != 0 && SymHasExpr (S) && IsConstExpr (GetSymExpr (S), &Val));
    return Val;
}



unsigned GetSymIndex (const SymEntry* S)
/* Return the symbol index for the given symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }
    PRECONDITION (S != 0 && (S->Flags & SF_INDEXED) != 0);
    return S->Index;
}



const FilePos* GetSymPos (const SymEntry* S)
/* Return the position of first occurence in the source for the given symbol */
{
    /* Resolve trampoline entries */
    if (S->Flags & SF_TRAMPOLINE) {
	S = S->V.Sym;
    }
    PRECONDITION (S != 0);
    return &S->Pos;
}





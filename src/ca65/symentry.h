/*****************************************************************************/
/*                                                                           */
/*				  symentry.h				     */
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



#ifndef SYMENTRY_H
#define SYMENTRY_H



/* common */
#include "cddefs.h"
#include "coll.h"
#include "filepos.h"



/*****************************************************************************/
/*     	      	    		     Data				     */
/*****************************************************************************/



/* Bits for the Flags value in SymEntry */
#define SF_NONE         0x0000          /* Empty flag set */
#define SF_USER		0x0001	    	/* User bit */
#define SF_TRAMPOLINE  	0x0002	    	/* Trampoline entry */
#define SF_EXPORT      	0x0004	    	/* Export this symbol */
#define SF_IMPORT   	0x0008	    	/* Import this symbol */
#define SF_GLOBAL	0x0010	    	/* Global symbol */
#define SF_LABEL        0x0080          /* Used as a label */
#define SF_FORCED       0x0100          /* Forced import, SF_IMPORT also set */
#define SF_INDEXED	0x0800		/* Index is valid */
#define SF_MULTDEF     	0x2000		/* Multiply defined symbol */
#define	SF_DEFINED  	0x4000 	       	/* Defined */
#define SF_REFERENCED	0x8000 	       	/* Referenced */

/* Arguments for SymFind... */
#define SYM_FIND_EXISTING 	0
#define SYM_ALLOC_NEW		1

/* Structure of a symbol table entry */
typedef struct SymEntry SymEntry;
struct SymEntry {
    SymEntry*  	      	    Left;      	/* Lexically smaller entry */
    SymEntry*  	    	    Right; 	/* Lexically larger entry */
    SymEntry*  	    	    List;	/* List of all entries */
    SymEntry*  	       	    Locals;  	/* Root of subtree for local symbols */
    struct SymTable*	    SymTab;	/* Table this symbol is in, 0 for locals */
    FilePos    	       	    Pos;  	/* File position for this symbol */
    unsigned                Flags;	/* Symbol flags */
    unsigned	    	    Index;	/* Index of import/export entries */
    union {
        struct ExprNode*    Expr;      	/* Expression if CONST not set */
	long	    	    Val;  	/* Value (if CONST set) */
	SymEntry*  	    Sym;	/* Symbol (if trampoline entry) */
    } V;
    Collection              ExprRefs;   /* Expressions using this symbol */
    unsigned char           ExportSize; /* Export address size */
    unsigned char           AddrSize;   /* Address size of label */
    unsigned char      	    ConDesPrio[CD_TYPE_COUNT];	/* ConDes priorities... */
					/* ...actually value+1 (used as flag) */
    unsigned                Name;      	/* Name index in global string pool */
};

/* List of all symbol table entries */
extern SymEntry* SymList;

/* Pointer to last defined symbol */
extern SymEntry* SymLast;



/*****************************************************************************/
/*     	       	   	    	     Code	  		   	     */
/*****************************************************************************/



int IsLocalName (const char* Name);
/* Return true if Name is the name of a local symbol */

int IsLocalNameId (unsigned Name);
/* Return true if Name is the name of a local symbol */

SymEntry* NewSymEntry (const char* Name);
/* Allocate a symbol table entry, initialize and return it */

#if defined(HAVE_INLINE)
INLINE void SymAddExprRef (SymEntry* Sym, struct ExprNode* Expr)
/* Add an expression reference to this symbol */
{
    CollAppend (&Sym->ExprRefs, Expr);
}
#else
#define SymAddExprRef(Sym,Expr)     CollAppend (&(Sym)->ExprRefs, Expr)
#endif

#if defined(HAVE_INLINE)
INLINE void SymDelExprRef (SymEntry* Sym, struct ExprNode* Expr)
/* Delete an expression reference to this symbol */
{
    CollDeleteItem (&Sym->ExprRefs, Expr);
}
#else
#define SymDelExprRef(Sym,Expr)     CollDeleteItem (&(Sym)->ExprRefs, Expr)
#endif

void SymDef (SymEntry* Sym, ExprNode* Expr, unsigned char AddrSize, unsigned Flags);
/* Mark a symbol as defined */

void SymRef (SymEntry* Sym);
/* Mark the given symbol as referenced */

void SymImport (SymEntry* Sym, unsigned char AddrSize, unsigned Flags);
/* Mark the given symbol as an imported symbol */

void SymExport (SymEntry* Sym, unsigned char AddrSize, unsigned Flags);
/* Mark the given symbol as an exported symbol */

void SymGlobal (SymEntry* Sym, unsigned char AddrSize, unsigned Flags);
/* Mark the given symbol as a global symbol, that is, as a symbol that is
 * either imported or exported.
 */

void SymConDes (SymEntry* Sym, unsigned char AddrSize, unsigned Type, unsigned Prio);
/* Mark the given symbol as a module constructor/destructor. This will also
 * mark the symbol as an export. Initializers may never be zero page symbols.
 */

int SymIsDef (const SymEntry* Sym);
/* Return true if the given symbol is already defined */

int SymIsRef (const SymEntry* Sym);
/* Return true if the given symbol has been referenced */

int SymIsImport (const SymEntry* Sym);
/* Return true if the given symbol is marked as import */

int SymIsConst (SymEntry* Sym, long* Val);
/* Return true if the given symbol has a constant value. If Val is not NULL
 * and the symbol has a constant value, store it's value there.
 */

int SymHasExpr (const SymEntry* Sym);
/* Return true if the given symbol has an associated expression */

void SymMarkUser (SymEntry* Sym);
/* Set a user mark on the specified symbol */

void SymUnmarkUser (SymEntry* Sym);
/* Remove a user mark from the specified symbol */

int SymHasUserMark (SymEntry* Sym);
/* Return the state of the user mark for the specified symbol */

struct ExprNode* GetSymExpr (SymEntry* Sym);
/* Get the expression for a non-const symbol */

const struct ExprNode* SymResolve (const SymEntry* Sym);
/* Helper function for DumpExpr. Resolves a symbol into an expression or return
 * NULL. Do not call in other contexts!
 */

const char* GetSymName (const SymEntry* Sym);
/* Return the name of the symbol */

unsigned GetSymAddrSize (const SymEntry* Sym);
/* Return the address size of the symbol. Beware: This function will just
 * return the AddrSize member, it will not look at the expression!
 */

unsigned GetSymIndex (const SymEntry* Sym);
/* Return the symbol index for the given symbol */

const FilePos* GetSymPos (const SymEntry* Sym);
/* Return the position of first occurence in the source for the given symbol */




/* End of symentry.h */

#endif




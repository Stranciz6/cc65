/*****************************************************************************/
/*                                                                           */
/*				   symtab.c				     */
/*                                                                           */
/*		   Symbol table for the ca65 macroassembler		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2011, Ullrich von Bassewitz                                      */
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



#include <string.h>

/* common */
#include "addrsize.h"
#include "check.h"
#include "hashstr.h"
#include "mmodel.h"
#include "scopedefs.h"
#include "symdefs.h"
#include "xmalloc.h"

/* ca65 */
#include "global.h"
#include "error.h"
#include "expr.h"
#include "objfile.h"
#include "scanner.h"
#include "segment.h"
#include "sizeof.h"
#include "span.h"
#include "spool.h"
#include "studyexpr.h"
#include "symtab.h"



/*****************************************************************************/
/*	     	    	   	     Data				     */
/*****************************************************************************/



/* Combined symbol entry flags used within this module */
#define SF_UNDEFMASK	(SF_REFERENCED | SF_DEFINED | SF_IMPORT)
#define SF_UNDEFVAL	(SF_REFERENCED)
#define SF_DBGINFOMASK 	(SF_UNUSED | SF_DEFINED | SF_IMPORT)
#define SF_DBGINFOVAL 	(SF_DEFINED)

/* Symbol tables */
SymTable*      	    CurrentScope = 0;   /* Pointer to current symbol table */
SymTable*	    RootScope    = 0;   /* Root symbol table */
static SymTable*    LastScope    = 0;   /* Pointer to last scope in list */
static unsigned     ScopeCount   = 0;   /* Number of scopes */

/* Symbol table variables */
static unsigned     ImportCount = 0;    /* Counter for import symbols */
static unsigned     ExportCount = 0;    /* Counter for export symbols */



/*****************************************************************************/
/*     	       	       	   Internally used functions		 	     */
/*****************************************************************************/



static unsigned ScopeTableSize (unsigned Level)
/* Get the size of a table for the given lexical level */
{
    switch (Level) {
        case 0:         return 213;
        case 1:         return  53;
        default:        return  29;
    }
}



static SymTable* NewSymTable (SymTable* Parent, const StrBuf* Name)
/* Allocate a symbol table on the heap and return it */
{
    /* Determine the lexical level and the number of table slots */
    unsigned Level = Parent? Parent->Level + 1 : 0;
    unsigned Slots = ScopeTableSize (Level);

    /* Allocate memory */
    SymTable* S = xmalloc (sizeof (SymTable) + (Slots-1) * sizeof (SymEntry*));

    /* Set variables and clear hash table entries */
    S->Next         = 0;
    S->Left         = 0;
    S->Right        = 0;
    S->Childs       = 0;
    S->OwnerSym     = 0;
    S->Spans        = AUTO_COLLECTION_INITIALIZER;
    S->Id           = ScopeCount++;
    S->Flags        = ST_NONE;
    S->AddrSize     = ADDR_SIZE_DEFAULT;
    S->Type         = SCOPE_UNDEF;
    S->Level        = Level;
    S->TableSlots   = Slots;
    S->TableEntries = 0;
    S->Parent       = Parent;
    S->Name         = GetStrBufId (Name);
    while (Slots--) {
       	S->Table[Slots] = 0;
    }

    /* Insert the symbol table into the list of all symbol tables */
    if (RootScope == 0) {
        RootScope = S;
    } else {
        LastScope->Next = S;
    }
    LastScope = S;

    /* Insert the symbol table into the child tree of the parent */
    if (Parent) {
        SymTable* T = Parent->Childs;
        if (T == 0) {
            /* First entry */
            Parent->Childs = S;
        } else {
            while (1) {
                /* Choose next entry */
                int Cmp = SB_Compare (Name, GetStrBuf (T->Name));
                if (Cmp < 0) {
                    if (T->Left) {
                        T = T->Left;
                    } else {
                        T->Left = S;
                        break;
                    }
                } else if (Cmp > 0) {
                    if (T->Right) {
                        T = T->Right;
                    } else {
                        T->Right = S;
                        break;
                    }
                } else {
                    /* Duplicate scope name */
                    Internal ("Duplicate scope name: `%m%p'", Name);
                }
            }
        }
    }

    /* Return the prepared struct */
    return S;
}



/*****************************************************************************/
/*     	       	   	   	     Code		  	   	     */
/*****************************************************************************/



void SymEnterLevel (const StrBuf* ScopeName, unsigned char Type,
                    unsigned char AddrSize, SymEntry* OwnerSym)
/* Enter a new lexical level */
{
    /* Map a default address size to something real */
    if (AddrSize == ADDR_SIZE_DEFAULT) {
        /* Use the segment address size */
        AddrSize = GetCurrentSegAddrSize ();
    }

    /* If we have a current scope, search for the given name and create a
     * new one if it doesn't exist. If this is the root scope, just create it.
     */
    if (CurrentScope) {

        /* Search for the scope, create a new one */
        CurrentScope = SymFindScope (CurrentScope, ScopeName, SYM_ALLOC_NEW);

        /* Check if the scope has been defined before */
        if (CurrentScope->Flags & ST_DEFINED) {
            Error ("Duplicate scope `%m%p'", ScopeName);
        }

    } else {
        CurrentScope = RootScope = NewSymTable (0, ScopeName);
    }

    /* Mark the scope as defined and set type, address size and owner symbol */
    CurrentScope->Flags    |= ST_DEFINED;
    CurrentScope->AddrSize = AddrSize;
    CurrentScope->Type     = Type;
    CurrentScope->OwnerSym = OwnerSym;

    /* If this is a scope that allows to emit data into segments, add spans
     * for all currently existing segments. Doing this for just a few scope 
     * types is not really necessary but an optimization, because it does not 
     * allocate memory for useless data (unhandled types here don't occupy 
     * space in any segment).
     */
    if (CurrentScope->Type <= SCOPE_HAS_DATA) {
        AddSpans (&CurrentScope->Spans);
    }
}



void SymLeaveLevel (void)
/* Leave the current lexical level */
{
    /* Close the spans. We don't care about the scope type here, since types 
     * without spans will just have an empty list.
     */
    CloseSpans (&CurrentScope->Spans);

    /* If we have spans, the first one is the segment that was active, when the 
     * scope was opened. Set the size of the scope to the number of data bytes 
     * emitted into this segment. If we have an owner symbol set the size of 
     * this symbol, too.
     */
    if (CollCount (&CurrentScope->Spans) > 0) {
        const Span* S = CollAtUnchecked (&CurrentScope->Spans, 0);
        unsigned long Size = GetSpanSize (S);
        DefSizeOfScope (CurrentScope, Size);
        if (CurrentScope->OwnerSym) {
            DefSizeOfSymbol (CurrentScope->OwnerSym, Size);
        }                                  
    }

    /* Leave the scope */
    CurrentScope = CurrentScope->Parent;
}



SymTable* SymFindScope (SymTable* Parent, const StrBuf* Name, int AllocNew)
/* Find a scope in the given enclosing scope */
{
    SymTable** T = &Parent->Childs;
    while (*T) {
        int Cmp = SB_Compare (Name, GetStrBuf ((*T)->Name));
        if (Cmp < 0) {
            T = &(*T)->Left;
        } else if (Cmp > 0) {
            T = &(*T)->Right;
        } else {
            /* Found the scope */
            return *T;
        }
    }

    /* Create a new scope if requested and we didn't find one */
    if (*T == 0 && AllocNew) {
        *T = NewSymTable (Parent, Name);
    }

    /* Return the scope */
    return *T;
}



SymTable* SymFindAnyScope (SymTable* Parent, const StrBuf* Name)
/* Find a scope in the given or any of its parent scopes. The function will
 * never create a new symbol, since this can only be done in one specific
 * scope.
 */
{
    SymTable* Scope;
    do {
	/* Search in the current table */
	Scope = SymFindScope (Parent, Name, SYM_FIND_EXISTING);
       	if (Scope == 0) {
	    /* Not found, search in the parent scope, if we have one */
	    Parent = Parent->Parent;
	}
    } while (Scope == 0 && Parent != 0);

    return Scope;
}



SymEntry* SymFindLocal (SymEntry* Parent, const StrBuf* Name, int AllocNew)
/* Find a cheap local symbol. If AllocNew is given and the entry is not
 * found, create a new one. Return the entry found, or the new entry created,
 * or - in case AllocNew is zero - return 0.
 */
{
    SymEntry* S;
    int Cmp;

    /* Local symbol, get the table */
    if (!Parent) {
        /* No last global, so there's no local table */
        Error ("No preceeding global symbol");
        if (AllocNew) {
            return NewSymEntry (Name, SF_LOCAL);
        } else {
            return 0;
        }
    }

    /* Search for the symbol if we have a table */
    Cmp = SymSearchTree (Parent->Locals, Name, &S);

    /* If we found an entry, return it */
    if (Cmp == 0) {
        return S;
    }

    if (AllocNew) {

        /* Otherwise create a new entry, insert and return it */
        SymEntry* N = NewSymEntry (Name, SF_LOCAL);
        N->Sym.Entry = Parent;
        if (S == 0) {
            Parent->Locals = N;
        } else if (Cmp < 0) {
            S->Left = N;
        } else {
            S->Right = N;
        }
        return N;
    }

    /* We did not find the entry and AllocNew is false. */
    return 0;
}



SymEntry* SymFind (SymTable* Scope, const StrBuf* Name, int AllocNew)
/* Find a new symbol table entry in the given table. If AllocNew is given and
 * the entry is not found, create a new one. Return the entry found, or the
 * new entry created, or - in case AllocNew is zero - return 0.
 */
{
    SymEntry* S;

    /* Global symbol: Get the hash value for the name */
    unsigned Hash = HashBuf (Name) % Scope->TableSlots;

    /* Search for the entry */
    int Cmp = SymSearchTree (Scope->Table[Hash], Name, &S);

    /* If we found an entry, return it */
    if (Cmp == 0) {
        return S;
    }

    if (AllocNew) {

        /* Otherwise create a new entry, insert and return it */
        SymEntry* N = NewSymEntry (Name, SF_NONE);
        N->Sym.Tab = Scope;
        if (S == 0) {
            Scope->Table[Hash] = N;
        } else if (Cmp < 0) {
            S->Left = N;
        } else {
            S->Right = N;
        }
        ++Scope->TableEntries;
        return N;

    }

    /* We did not find the entry and AllocNew is false. */
    return 0;
}



SymEntry* SymFindAny (SymTable* Scope, const StrBuf* Name)
/* Find a symbol in the given or any of its parent scopes. The function will
 * never create a new symbol, since this can only be done in one specific
 * scope.
 */
{
    SymEntry* Sym;
    do {
	/* Search in the current table */
	Sym = SymFind (Scope, Name, SYM_FIND_EXISTING);
       	if (Sym) {
	    /* Found, return it */
	    break;
	}

        /* Not found, search in the parent scope, if we have one */
	Scope = Scope->Parent;

    } while (Sym == 0 && Scope != 0);

    /* Return the result */
    return Sym;
}



static void SymCheckUndefined (SymEntry* S)
/* Handle an undefined symbol */
{
    /* Undefined symbol. It may be...
     *
     *   - An undefined symbol in a nested lexical level. In this
     *     case, search for the symbol in the higher levels and
     * 	   make the entry a trampoline entry if we find one.
     *
     *   - If the symbol is not found, it is a real undefined symbol.
     *     If the AutoImport flag is set, make it an import. If the
     *     AutoImport flag is not set, it's an error.
     */
    SymEntry* Sym = 0;
    SymTable* Tab = GetSymParentScope (S);
    while (Tab) {
        Sym = SymFind (Tab, GetStrBuf (S->Name), SYM_FIND_EXISTING);
        if (Sym && (Sym->Flags & (SF_DEFINED | SF_IMPORT)) != 0) {
            /* We've found a symbol in a higher level that is
             * either defined in the source, or an import.
             */
             break;
        }
        /* No matching symbol found in this level. Look further */
        Tab = Tab->Parent;
    }

    if (Sym) {

        /* We found the symbol in a higher level. Transfer the flags and
         * address size from the local symbol to that in the higher level
         * and check for problems.
         */
        if (S->Flags & SF_EXPORT) {
	    if (Sym->Flags & SF_IMPORT) {
	       	/* The symbol is already marked as import */
       	       	LIError (&S->LineInfos,
                         "Symbol `%s' is already an import",
                         GetString (Sym->Name));
	    }
            if (Sym->Flags & SF_EXPORT) {
                /* The symbol is already marked as an export. */
                if (Sym->AddrSize > S->ExportSize) {
                    /* We're exporting a symbol smaller than it actually is */
                    LIWarning (&S->LineInfos, 1,
                               "Symbol `%m%p' is %s but exported %s",
                              GetSymName (Sym),
                              AddrSizeToStr (Sym->AddrSize),
                              AddrSizeToStr (S->ExportSize));
                }
            } else {
                /* Mark the symbol as an export */
                Sym->Flags |= SF_EXPORT;
                Sym->ExportSize = S->ExportSize;
                if (Sym->ExportSize == ADDR_SIZE_DEFAULT) {
                    /* Use the actual size of the symbol */
                    Sym->ExportSize = Sym->AddrSize;
                }
                if (Sym->AddrSize > Sym->ExportSize) {
                    /* We're exporting a symbol smaller than it actually is */
                    LIWarning (&S->LineInfos, 1,
                               "Symbol `%m%p' is %s but exported %s",
                               GetSymName (Sym),
                               AddrSizeToStr (Sym->AddrSize),
                               AddrSizeToStr (Sym->ExportSize));
                }
            }
        }
        Sym->Flags |= (S->Flags & SF_REFERENCED);

        /* Transfer all expression references */
        SymTransferExprRefs (S, Sym);

        /* Mark the symbol as unused removing all other flags */
        S->Flags = SF_UNUSED;

    } else {
	/* The symbol is definitely undefined */
	if (S->Flags & SF_EXPORT) {
	    /* We will not auto-import an export */
	    LIError (&S->LineInfos,
                     "Exported symbol `%m%p' was never defined",
                     GetSymName (S));
	} else {
	    if (AutoImport) {
	    	/* Mark as import, will be indexed later */
	    	S->Flags |= SF_IMPORT;
                /* Use the address size for code */
                S->AddrSize = CodeAddrSize;
	    } else {
	    	/* Error */
	        LIError (&S->LineInfos,
                         "Symbol `%m%p' is undefined",
                         GetSymName (S));
	    }
	}
    }
}



void SymCheck (void)
/* Run through all symbols and check for anomalies and errors */
{
    SymEntry* S;

    /* Check for open scopes */
    if (CurrentScope->Parent != 0) {
	Error ("Local scope was not closed");
    }

    /* First pass: Walk through all symbols, checking for undefined's and
     * changing them to trampoline symbols or make them imports.
     */
    S = SymList;
    while (S) {
	/* If the symbol is marked as global, mark it as export, if it is
	 * already defined, otherwise mark it as import.
	 */
	if (S->Flags & SF_GLOBAL) {
	    if (S->Flags & SF_DEFINED) {
	     	SymExportFromGlobal (S);
	    } else {
	     	SymImportFromGlobal (S);
	    }
	}

	/* Handle undefined symbols */
       	if ((S->Flags & SF_UNDEFMASK) == SF_UNDEFVAL) {
	    /* This is an undefined symbol. Handle it. */
	    SymCheckUndefined (S);
	}

	/* Next symbol */
	S = S->List;
    }

    /* Second pass: Walk again through the symbols. Count exports and imports
     * and set address sizes where this has not happened before. Ignore
     * undefined's, since we handled them in the last pass, and ignore unused
     * symbols, since we handled them in the last pass, too.
     */
    S = SymList;
    while (S) {
	if ((S->Flags & SF_UNUSED) == 0 &&
	    (S->Flags & SF_UNDEFMASK) != SF_UNDEFVAL) {

            /* Check for defined symbols that were never referenced */
	    if ((S->Flags & SF_DEFINED) != 0 && (S->Flags & SF_REFERENCED) == 0) {
                const StrBuf* Name = GetStrBuf (S->Name);
                if (SB_At (Name, 0) != '.') {           /* Ignore internals */
                    LIWarning (&S->LineInfos, 2,
                               "Symbol `%m%p' is defined but never used",
                               GetSymName (S));
                }
	    }

            /* Assign an index to all imports */
	    if (S->Flags & SF_IMPORT) {
	    	if ((S->Flags & (SF_REFERENCED | SF_FORCED)) == SF_NONE) {
	    	    /* Imported symbol is not referenced */
	    	    LIWarning (&S->LineInfos, 2,
                               "Symbol `%m%p' is imported but never used",
                               GetSymName (S));
	    	} else {
	    	    /* Give the import an id, count imports */
	    	    S->ImportId = ImportCount++;
	    	}
	    }

            /* Count exports */
	    if (S->Flags & SF_EXPORT) {
	    	++ExportCount;
	    }

            /* If the symbol is defined but has an unknown address size,
             * recalculate it.
             */
            if (SymHasExpr (S) && S->AddrSize == ADDR_SIZE_DEFAULT) {
                ExprDesc ED;
                ED_Init (&ED);
                StudyExpr (S->Expr, &ED);
                S->AddrSize = ED.AddrSize;
                if (SymIsExport (S)) {
                    if (S->ExportSize == ADDR_SIZE_DEFAULT) {
                        /* Use the real export size */
                        S->ExportSize = S->AddrSize;
                    } else if (S->AddrSize > S->ExportSize) {
                        /* We're exporting a symbol smaller than it actually is */
                        LIWarning (&S->LineInfos, 1,
                                   "Symbol `%m%p' is %s but exported %s",
                                   GetSymName (S),
                                   AddrSizeToStr (S->AddrSize),
                                   AddrSizeToStr (S->ExportSize));
                    }
                }
                ED_Done (&ED);
            }

            /* If the address size of the symbol was guessed, check the guess
             * against the actual address size and print a warning if the two
             * differ.
             */
            if (S->AddrSize != ADDR_SIZE_DEFAULT) {
                /* Do we have data for this address size? */
                if (S->AddrSize <= sizeof (S->GuessedUse) / sizeof (S->GuessedUse[0])) {
                    /* Get the file position where the symbol was used */
                    const FilePos* P = S->GuessedUse[S->AddrSize - 1];
                    if (P) {
                        PWarning (P, 0,
                                  "Didn't use %s addressing for `%m%p'",
                                  AddrSizeToStr (S->AddrSize),
                                  GetSymName (S));
                    }
                }
            }

	}

	/* Next symbol */
     	S = S->List;
    }
}



void SymDump (FILE* F)
/* Dump the symbol table */
{
    SymEntry* S = SymList;

    while (S) {
	/* Ignore unused symbols */
	if ((S->Flags & SF_UNUSED) != 0) {
	    fprintf (F,
		     "%m%-24p %s %s %s %s %s\n",
		     GetSymName (S),
		     (S->Flags & SF_DEFINED)? "DEF" : "---",
		     (S->Flags & SF_REFERENCED)? "REF" : "---",
		     (S->Flags & SF_IMPORT)? "IMP" : "---",
		     (S->Flags & SF_EXPORT)? "EXP" : "---",
                     AddrSizeToStr (S->AddrSize));
	}
	/* Next symbol */
	S = S->List;
    }
}



void WriteImports (void)
/* Write the imports list to the object file */
{
    SymEntry* S;

    /* Tell the object file module that we're about to start the imports */
    ObjStartImports ();

    /* Write the import count to the list */
    ObjWriteVar (ImportCount);

    /* Walk throught list and write all valid imports to the file. An import
     * is considered valid, if it is either referenced, or the forced bit is
     * set. Otherwise, the import is ignored (no need to link in something
     * that isn't used).
     */
    S = SymList;
    while (S) {
        if ((S->Flags & (SF_UNUSED | SF_IMPORT)) == SF_IMPORT &&
            (S->Flags & (SF_REFERENCED | SF_FORCED)) != 0) {

            ObjWrite8 (S->AddrSize);
       	    ObjWriteVar (S->Name);
     	    WriteLineInfo (&S->LineInfos);
     	}
     	S = S->List;
    }

    /* Done writing imports */
    ObjEndImports ();
}



void WriteExports (void)
/* Write the exports list to the object file */
{
    SymEntry* S;
    unsigned Type;

    /* Tell the object file module that we're about to start the exports */
    ObjStartExports ();

    /* Write the export count to the list */
    ObjWriteVar (ExportCount);

    /* Walk throught list and write all exports to the file */
    S = SymList;
    while (S) {
       	if ((S->Flags & (SF_UNUSED | SF_EXPORT)) == SF_EXPORT) {

	    /* Get the expression bits and the value */
            long ConstVal;
            unsigned ExprMask = GetSymInfoFlags (S, &ConstVal);

            /* Check if this symbol has a size. If so, remember it in the
             * flags.
             */
            long Size;
            SymEntry* SizeSym = FindSizeOfSymbol (S);
            if (SizeSym != 0 && SymIsConst (SizeSym, &Size)) {
                ExprMask |= SYM_SIZE;
            }

	    /* Count the number of ConDes types */
	    for (Type = 0; Type < CD_TYPE_COUNT; ++Type) {
	     	if (S->ConDesPrio[Type] != CD_PRIO_NONE) {
	     	    SYM_INC_CONDES_COUNT (ExprMask);
	     	}
	    }

	    /* Write the type and the export size */
	    ObjWriteVar (ExprMask);
            ObjWrite8 (S->ExportSize);

	    /* Write any ConDes declarations */
	    if (SYM_GET_CONDES_COUNT (ExprMask) > 0) {
	     	for (Type = 0; Type < CD_TYPE_COUNT; ++Type) {
	     	    unsigned char Prio = S->ConDesPrio[Type];
	     	    if (Prio != CD_PRIO_NONE) {
	     	     	ObjWrite8 (CD_BUILD (Type, Prio));
	     	    }
	     	}
	    }

	    /* Write the name */
       	    ObjWriteVar (S->Name);

	    /* Write the value */
	    if (SYM_IS_CONST (ExprMask)) {
	     	/* Constant value */
    	     	ObjWrite32 (ConstVal);
	    } else {
	     	/* Expression involved */
	        WriteExpr (S->Expr);
            }

            /* If the symbol has a size, write it to the file */
            if (SYM_HAS_SIZE (ExprMask)) {
                ObjWriteVar (Size);
            }

	    /* Write the line infos */
	    WriteLineInfo (&S->LineInfos);
	}
     	S = S->List;
    }

    /* Done writing exports */
    ObjEndExports ();
}



void WriteDbgSyms (void)
/* Write a list of all symbols to the object file */
{
    unsigned Count;
    SymEntry* S;

    /* Tell the object file module that we're about to start the debug info */
    ObjStartDbgSyms ();

    /* Check if debug info is requested */
    if (DbgSyms) {

    	/* Walk through the list, give each symbol an id and count them */
    	Count = 0;
    	S = SymList;
    	while (S) {
    	    if ((S->Flags & SF_DBGINFOMASK) == SF_DBGINFOVAL &&
                !IsSizeOfSymbol (S)) {
                S->DebugSymId = Count++;
    	    }
    	    S = S->List;
    	}

    	/* Write the symbol count to the list */
       	ObjWriteVar (Count);

       	/* Walk through list and write all symbols to the file. Ignore size
         * symbols.
         */
    	S = SymList;
    	while (S) {
    	    if ((S->Flags & SF_DBGINFOMASK) == SF_DBGINFOVAL &&
                !IsSizeOfSymbol (S)) {

                /* Get the expression bits and the value */
                long ConstVal;
                unsigned ExprMask = GetSymInfoFlags (S, &ConstVal);

                /* Check if this symbol has a size. If so, remember it in the
                 * flags.
                 */
                long Size;
                SymEntry* SizeSym = FindSizeOfSymbol (S);
                if (SizeSym != 0 && SymIsConst (SizeSym, &Size)) {
                    ExprMask |= SYM_SIZE;
                }

		/* Write the type */
		ObjWriteVar (ExprMask);

                /* Write the address size */
                ObjWrite8 (S->AddrSize);

                /* Write the id of the parent. For normal symbols, this is a
                 * scope (symbol table), for cheap locals, it's a symbol.
                 */
                if (SYM_IS_STD (ExprMask)) {
                    ObjWriteVar (S->Sym.Tab->Id);
                } else {
                    ObjWriteVar (S->Sym.Entry->DebugSymId);
                }

		/* Write the name */
       	       	ObjWriteVar (S->Name);

		/* Write the value */
		if (SYM_IS_CONST (ExprMask)) {
		    /* Constant value */
		    ObjWrite32 (ConstVal);
		} else {
		    /* Expression involved */
		    WriteExpr (S->Expr);
		}

                /* If the symbol has a size, write it to the file */
                if (SYM_HAS_SIZE (ExprMask)) {
                    ObjWriteVar (Size);
                }

		/* Write the line infos */
		WriteLineInfo (&S->LineInfos);
	    }
	    S = S->List;
	}

    } else {

	/* No debug symbols */
	ObjWriteVar (0);

    }

    /* Done writing debug symbols */
    ObjEndDbgSyms ();
}



void WriteScopes (void)
/* Write the scope table to the object file */
{
    /* Tell the object file module that we're about to start the scopes */
    ObjStartScopes ();

    /* We will write scopes only if debug symbols are requested */
    if (DbgSyms) {

        /* Get head of list */
        SymTable* S = RootScope;

        /* Write the scope count to the file */
        ObjWriteVar (ScopeCount);

        /* Walk through all scopes and write them to the file */
        while (S) {

            /* Flags for this scope */
            unsigned Flags = 0;

            /* Check if this scope has a size. If so, remember it in the
             * flags.
             */
            long Size;
            SymEntry* SizeSym = FindSizeOfScope (S);
            if (SizeSym != 0 && SymIsConst (SizeSym, &Size)) {
                Flags |= SCOPE_SIZE;
            }

            /* Scope must be defined */
            CHECK (S->Type != SCOPE_UNDEF);

            /* Id of parent scope */
            if (S->Parent) {
                ObjWriteVar (S->Parent->Id);
            } else {
                ObjWriteVar (0);
            }

            /* Lexical level */
            ObjWriteVar (S->Level);

            /* Scope flags */
            ObjWriteVar (Flags);

            /* Type of scope */
            ObjWriteVar (S->Type);

            /* Name of the scope */
            ObjWriteVar (S->Name);

            /* If the scope has a size, write it to the file */
            if (SCOPE_HAS_SIZE (Flags)) {
                ObjWriteVar (Size);
            }

            /* Spans for this scope */
            WriteSpans (&S->Spans);

            /* Next scope */
            S = S->Next;
        }

    } else {

        /* No debug info requested */
        ObjWriteVar (0);

    }

    /* Done writing the scopes */
    ObjEndScopes ();
}




/*****************************************************************************/
/*                                                                           */
/*                                studyexpr.c                                */
/*                                                                           */
/*                         Study an expression tree                          */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2003      Ullrich von Bassewitz                                       */
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
#include "check.h"
#include "print.h"
#include "shift.h"
#include "xmalloc.h"

/* ca65 */
#include "error.h"
#include "segment.h"
#include "studyexpr.h"
#include "symtab.h"
#include "ulabel.h"



/*****************************************************************************/
/*                              struct ExprDesc                              */
/*****************************************************************************/



ExprDesc* ED_Init (ExprDesc* ED)
/* Initialize an ExprDesc structure for use with StudyExpr */
{
    ED->Flags     = ED_OK;
    ED->AddrSize  = ADDR_SIZE_DEFAULT;
    ED->Val  	  = 0;
    ED->SymCount  = 0;
    ED->SymLimit  = 0;
    ED->SymRef    = 0;
    ED->SecCount  = 0;
    ED->SecLimit  = 0;
    ED->SecRef    = 0;
    return ED;
}



void ED_Done (ExprDesc* ED)
/* Delete allocated memory for an ExprDesc. */
{
    xfree (ED->SymRef);
    xfree (ED->SecRef);
}



int ED_IsConst (const ExprDesc* D)
/* Return true if the expression is constant */
{
    unsigned I;

    if (D->Flags & ED_TOO_COMPLEX) {
        return 0;
    }
    for (I = 0; I < D->SymCount; ++I) {
        if (D->SymRef[I].Count != 0) {
            return 0;
        }
    }
    for (I = 0; I < D->SecCount; ++I) {
        if (D->SecRef[I].Count != 0) {
            return 0;
        }
    }
    return 1;
}



static int ED_IsValid (const ExprDesc* D)
/* Return true if the expression is valid, that is, the TOO_COMPLEX flag is
 * not set
 */
{
    return ((D->Flags & ED_TOO_COMPLEX) == 0);
}



static void ED_Invalidate (ExprDesc* D)
/* Set the TOO_COMPLEX flag for D */
{
    D->Flags |= ED_TOO_COMPLEX;
}



static void ED_UpdateAddrSize (ExprDesc* ED, unsigned char AddrSize)
/* Update the address size of the expression */
{
    if (ED->AddrSize == ADDR_SIZE_DEFAULT || AddrSize > ED->AddrSize) {
        ED->AddrSize = AddrSize;
    }
}



static ED_SymRef* ED_FindSymRef (ExprDesc* ED, SymEntry* Sym)
/* Find a symbol reference and return it. Return NULL if the reference does
 * not exist.
 */
{
    unsigned I;
    ED_SymRef* SymRef;
    for (I = 0, SymRef = ED->SymRef; I < ED->SymCount; ++I, ++SymRef) {
        if (SymRef->Ref == Sym) {
            return SymRef;
        }
    }
    return 0;
}



static ED_SecRef* ED_FindSecRef (ExprDesc* ED, unsigned Sec)
/* Find a section reference and return it. Return NULL if the reference does
 * not exist.
 */
{
    unsigned I;
    ED_SecRef* SecRef;
    for (I = 0, SecRef = ED->SecRef; I < ED->SecCount; ++I, ++SecRef) {
        if (SecRef->Ref == Sec) {
            return SecRef;
        }
    }
    return 0;
}



static ED_SymRef* ED_AllocSymRef (ExprDesc* ED, SymEntry* Sym)
/* Allocate a new symbol reference and return it. The count of the new
 * reference will be set to zero, and the reference itself to Sym.
 */
{
    ED_SymRef* SymRef;

    /* Make sure we have enough SymRef slots */
    if (ED->SymCount >= ED->SymLimit) {
        ED->SymLimit *= 2;
        if (ED->SymLimit == 0) {
            ED->SymLimit = 2;
        }
        ED->SymRef = xrealloc (ED->SymRef, ED->SymLimit * sizeof (ED->SymRef[0]));
    }

    /* Allocate a new slot */
    SymRef = ED->SymRef + ED->SymCount++;

    /* Initialize the new struct and return it */
    SymRef->Count = 1;
    SymRef->Ref   = Sym;
    return SymRef;
}



static ED_SecRef* ED_AllocSecRef (ExprDesc* ED, unsigned Sec)
/* Allocate a new section reference and return it. The count of the new
 * reference will be set to zero, and the reference itself to Sec.
 */
{
    ED_SecRef* SecRef;

    /* Make sure we have enough SecRef slots */
    if (ED->SecCount >= ED->SecLimit) {
        ED->SecLimit *= 2;
        if (ED->SecLimit == 0) {
            ED->SecLimit = 2;
        }
        ED->SecRef = xrealloc (ED->SecRef, ED->SecLimit * sizeof (ED->SecRef[0]));
    }

    /* Allocate a new slot */
    SecRef = ED->SecRef + ED->SecCount++;

    /* Initialize the new struct and return it */
    SecRef->Count = 0;
    SecRef->Ref   = Sec;
    return SecRef;
}



static ED_SymRef* ED_GetSymRef (ExprDesc* ED, SymEntry* Sym)
/* Get a symbol reference and return it. If the symbol reference does not
 * exist, a new one is created and returned.
 */
{
    ED_SymRef* SymRef = ED_FindSymRef (ED, Sym);
    if (SymRef == 0) {
        SymRef = ED_AllocSymRef (ED, Sym);
    }
    return SymRef;
}



static ED_SecRef* ED_GetSecRef (ExprDesc* ED, unsigned Sec)
/* Get a section reference and return it. If the section reference does not
 * exist, a new one is created and returned.
 */
{
    ED_SecRef* SecRef = ED_FindSecRef (ED, Sec);
    if (SecRef == 0) {
        SecRef = ED_AllocSecRef (ED, Sec);
    }
    return SecRef;
}



static void ED_MergeSymRefs (ExprDesc* ED, const ExprDesc* New)
/* Merge the symbol references from New into ED */
{
    unsigned I;
    for (I = 0; I < New->SymCount; ++I) {

        /* Get a pointer to the SymRef entry */
        const ED_SymRef* NewRef = New->SymRef + I;

        /* Get the corresponding entry in ED */
        ED_SymRef* SymRef = ED_GetSymRef (ED, NewRef->Ref);

        /* Sum up the references */
        SymRef->Count += NewRef->Count;
    }
}



static void ED_MergeSecRefs (ExprDesc* ED, const ExprDesc* New)
/* Merge the section references from New into ED */
{
    unsigned I;
    for (I = 0; I < New->SecCount; ++I) {

        /* Get a pointer to the SymRef entry */
        const ED_SecRef* NewRef = New->SecRef + I;

        /* Get the corresponding entry in ED */
        ED_SecRef* SecRef = ED_GetSecRef (ED, NewRef->Ref);

        /* Sum up the references */
        SecRef->Count += NewRef->Count;
    }
}



static void ED_MergeRefs (ExprDesc* ED, const ExprDesc* New)
/* Merge all references from New into ED */
{
    ED_MergeSymRefs (ED, New);
    ED_MergeSecRefs (ED, New);
}



static void ED_Add (ExprDesc* ED, const ExprDesc* Right)
/* Calculate ED = ED + Right, update address size in ED */
{
    ED->Val += Right->Val;
    ED_MergeRefs (ED, Right);
    ED_UpdateAddrSize (ED, Right->AddrSize);
}



static void ED_Mul (ExprDesc* ED, const ExprDesc* Right)
/* Calculate ED = ED * Right, update address size in ED */
{
    unsigned I;

    ED->Val *= Right->Val;
    for (I = 0; I < ED->SymCount; ++I) {
        ED->SymRef[I].Count *= Right->Val;
    }
    for (I = 0; I < ED->SecCount; ++I) {
        ED->SecRef[I].Count *= Right->Val;
    }
    ED_UpdateAddrSize (ED, Right->AddrSize);
}



static void ED_Neg (ExprDesc* D)
/* Negate an expression */
{
    unsigned I;

    D->Val = -D->Val;
    for (I = 0; I < D->SymCount; ++I) {
        D->SymRef[I].Count = -D->SymRef[I].Count;
    }
    for (I = 0; I < D->SecCount; ++I) {
        D->SecRef[I].Count = -D->SecRef[I].Count;
    }
}



static void ED_Move (ExprDesc* From, ExprDesc* To)
/* Move the data from one ExprDesc to another. Old data is freed, and From
 * is prepared to that ED_Done may be called safely.
 */
{
    /* Delete old data */
    ED_Done (To);

    /* Move the data */
    *To = *From;

    /* Cleanup From */
    ED_Init (From);
}



/*****************************************************************************/
/*     	      	     	       	     Code	      			     */
/*****************************************************************************/



static void StudyExprInternal (ExprNode* Expr, ExprDesc* D);
/* Study an expression tree and place the contents into D */



static void StudyBinaryExpr (ExprNode* Expr, ExprDesc* D)
/* Study a binary expression subtree. This is a helper function for StudyExpr
 * used for operations that succeed when both operands are known and constant.
 * It evaluates the two subtrees and checks if they are constant. If they
 * aren't constant, it will set the TOO_COMPLEX flag, and merge references.
 * Otherwise the first value is returned in D->Val, the second one in D->Right,
 * so the actual operation can be done by the caller.
 */
{
    ExprDesc Right;

    /* Study the left side of the expression */
    StudyExprInternal (Expr->Left, D);

    /* Study the right side of the expression */
    ED_Init (&Right);
    StudyExprInternal (Expr->Right, &Right);

    /* Check if we can handle the operation */
    if (ED_IsConst (D) && ED_IsConst (&Right)) {

        /* Remember the constant value from Right */
        D->Right = Right.Val;

    } else {

        /* Cannot evaluate */
        ED_Invalidate (D);

        /* Merge references and update address size */
        ED_MergeRefs (D, &Right);
        ED_UpdateAddrSize (D, Right.AddrSize);

    }

    /* Cleanup Right */
    ED_Done (&Right);
}



static void StudyLiteral (ExprNode* Expr, ExprDesc* D)
/* Study a literal expression node */
{
    /* This one is easy */
    D->Val = Expr->V.Val;
}



static void StudySymbol (ExprNode* Expr, ExprDesc* D)
/* Study a symbol expression node */
{
    /* Get the symbol from the expression */
    SymEntry* Sym = Expr->V.Sym;

    /* If the symbol is defined somewhere, it has an expression associated.
     * In this case, just study the expression associated with the symbol,
     * but mark the symbol so if we encounter it twice, we know that we have
     * a circular reference.
     */
    if (SymHasExpr (Sym)) {
        if (SymHasUserMark (Sym)) {
            if (Verbosity > 0) {
                DumpExpr (Expr, SymResolve);
            }
            PError (GetSymPos (Sym),
                    "Circular reference in definition of symbol `%s'",
                    GetSymName (Sym));
            ED_Invalidate (D);
        } else {
            SymMarkUser (Sym);
            StudyExprInternal (GetSymExpr (Sym), D);
            SymUnmarkUser (Sym);
            ED_UpdateAddrSize (D, GetSymAddrSize (Sym));
        }
    } else {
        /* The symbol is either undefined or an import. In both cases, track
         * the symbols used and update the address size, but in case of an
         * undefined symbol also set the "too complex" flag, since we cannot
         * evaluate the final result.
         */
        ED_SymRef* SymRef = ED_GetSymRef (D, Sym);
        ++SymRef->Count;
        ED_UpdateAddrSize (D, GetSymAddrSize (Sym));
        if (!SymIsImport (Sym)) {
            /* Cannot handle */
            ED_Invalidate (D);
        }
    }
}



static void StudySection (ExprNode* Expr, ExprDesc* D)
/* Study a section expression node */
{
    /* Get the section reference */
    ED_SecRef* SecRef = ED_GetSecRef (D, Expr->V.SegNum);

    /* Update the data and the address size */
    ++SecRef->Count;
    ED_UpdateAddrSize (D, GetSegAddrSize (SecRef->Ref));
}



static void StudyULabel (ExprNode* Expr, ExprDesc* D)
/* Study an unnamed label expression node */
{
    /* If we can resolve the label, study the expression associated with it,
     * otherwise mark the expression as too complex to evaluate.
     */
    if (ULabCanResolve ()) {
        /* We can resolve the label */
        StudyExprInternal (ULabResolve (Expr->V.Val), D);
    } else {
        ED_Invalidate (D);
    }
}



static void StudyPlus (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_PLUS binary expression node */
{
    ExprDesc Right;

    /* Study the left side of the expression */
    StudyExprInternal (Expr->Left, D);

    /* Study the right side of the expression */
    ED_Init (&Right);
    StudyExprInternal (Expr->Right, &Right);

    /* Check if we can handle the operation */
    if (ED_IsValid (D) || ED_IsValid (&Right)) {

        /* Add both */
        ED_Add (D, &Right);

    } else {

        /* Cannot evaluate */
        ED_Invalidate (D);

        /* Merge references and update address size */
        ED_MergeRefs (D, &Right);
        ED_UpdateAddrSize (D, Right.AddrSize);

    }

    /* Done */
    ED_Done (&Right);
}



static void StudyMinus (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_MINUS binary expression node */
{
    ExprDesc Right;

    /* Study the left side of the expression */
    StudyExprInternal (Expr->Left, D);

    /* Study the right side of the expression */
    ED_Init (&Right);
    StudyExprInternal (Expr->Right, &Right);

    /* Check if we can handle the operation */
    if (ED_IsValid (D) || ED_IsValid (&Right)) {

        /* Subtract both */
        ED_Neg (&Right);
        ED_Add (D, &Right);

    } else {

        /* Cannot evaluate */
        ED_Invalidate (D);

        /* Merge references and update address size */
        ED_MergeRefs (D, &Right);
        ED_UpdateAddrSize (D, Right.AddrSize);

    }

    /* Done */
    ED_Done (&Right);
}



static void StudyMul (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_MUL binary expression node */
{
    ExprDesc Right;

    /* Study the left side of the expression */
    StudyExprInternal (Expr->Left, D);

    /* Study the right side of the expression */
    ED_Init (&Right);
    StudyExprInternal (Expr->Right, &Right);

    /* We can handle the operation if at least one of both operands is const
     * and the other one is valid.
     */
    if (ED_IsConst (D) && ED_IsValid (&Right)) {

        /* Multiplicate both, result goes into Right */
        ED_Mul (&Right, D);

        /* Move result into D */
        ED_Move (&Right, D);

    } else if (ED_IsConst (&Right) && ED_IsValid (D)) {

        /* Multiplicate both */
        ED_Mul (D, &Right);

    } else {

        /* Cannot handle this operation */
        ED_Invalidate (D);

    }

    /* If we could not handle the op, merge references and update address size */
    if (!ED_IsValid (D)) {
        ED_MergeRefs (D, &Right);
        ED_UpdateAddrSize (D, Right.AddrSize);
    }

    /* Done */
    ED_Done (&Right);
}



static void StudyDiv (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_DIV binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        if (D->Right == 0) {
            Error ("Division by zero");
            ED_Invalidate (D);
        } else {
            D->Val /= D->Right;
        }
    }
}



static void StudyMod (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_MOD binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        if (D->Right == 0) {
            Error ("Modulo operation with zero");
            ED_Invalidate (D);
        } else {
            D->Val %= D->Right;
        }
    }
}



static void StudyOr (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_OR binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val |= D->Right;
    }
}



static void StudyXor (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_XOR binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val ^= D->Right;
    }
}



static void StudyAnd (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_AND binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val &= D->Right;
    }
}



static void StudyShl (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_SHL binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = shl_l (D->Val, D->Right);
    }
}



static void StudyShr (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_SHR binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = shr_l (D->Val, D->Right);
    }
}



static void StudyEQ (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_EQ binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val == D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyNE (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_NE binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val != D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyLT (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_LT binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val < D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyGT (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_GT binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val > D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyLE (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_LE binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val <= D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyGE (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_GE binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val >= D->Right);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyBoolAnd (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BOOLAND binary expression node */
{
    StudyExprInternal (Expr->Left, D);
    if (ED_IsConst (D)) {
        if (D->Val != 0) {   /* Shortcut op */
            ED_Done (D);
            ED_Init (D);
            StudyExprInternal (Expr->Right, D);
            if (ED_IsConst (D)) {
                D->Val = (D->Val != 0);
            } else {
                ED_Invalidate (D);
            }
        }
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyBoolOr (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BOOLOR binary expression node */
{
    StudyExprInternal (Expr->Left, D);
    if (ED_IsConst (D)) {
        if (D->Val == 0) {   /* Shortcut op */
            ED_Done (D);
            ED_Init (D);
            StudyExprInternal (Expr->Right, D);
            if (ED_IsConst (D)) {
                D->Val = (D->Val != 0);
            } else {
                ED_Invalidate (D);
            }
        } else {
            D->Val = 1;
        }
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyBoolXor (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BOOLXOR binary expression node */
{
    /* Use helper function */
    StudyBinaryExpr (Expr, D);

    /* If the result is valid, apply the operation */
    if (ED_IsValid (D)) {
        D->Val = (D->Val != 0) ^ (D->Right != 0);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyUnaryMinus (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_UNARY_MINUS expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* If it is valid, negate it */
    if (ED_IsValid (D)) {
        ED_Neg (D);
    }
}



static void StudyNot (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_NOT expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = ~D->Val;
    } else {
        ED_Invalidate (D);
    }
}



static void StudySwap (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_SWAP expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val & ~0xFFFFUL) | ((D->Val >> 8) & 0xFF) | ((D->Val << 8) & 0xFF00);
    } else {
        ED_Invalidate (D);
    }
}



static void StudyBoolNot (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BOOLNOT expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val == 0);
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is 0 or 1 */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyByte0 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BYTE0 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val & 0xFF);
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is a zero page expression */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyByte1 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BYTE1 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val >> 8) & 0xFF;
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is a zero page expression */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyByte2 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BYTE2 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val >> 16) & 0xFF;
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is a zero page expression */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyByte3 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_BYTE3 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val >> 24) & 0xFF;
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is a zero page expression */
    D->AddrSize = ADDR_SIZE_ZP;
}



static void StudyWord0 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_WORD0 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val & 0xFFFFL);
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is an absolute expression */
    D->AddrSize = ADDR_SIZE_ABS;
}



static void StudyWord1 (ExprNode* Expr, ExprDesc* D)
/* Study an EXPR_WORD1 expression node */
{
    /* Study the expression */
    StudyExprInternal (Expr->Left, D);

    /* We can handle only const expressions */
    if (ED_IsConst (D)) {
        D->Val = (D->Val >> 16) & 0xFFFFL;
    } else {
        ED_Invalidate (D);
    }

    /* In any case, the result is an absolute expression */
    D->AddrSize = ADDR_SIZE_ABS;
}



static void StudyExprInternal (ExprNode* Expr, ExprDesc* D)
/* Study an expression tree and place the contents into D */
{
    /* Study this expression node */
    switch (Expr->Op) {

    	case EXPR_LITERAL:
            StudyLiteral (Expr, D);
    	    break;

    	case EXPR_SYMBOL:
            StudySymbol (Expr, D);
            break;

    	case EXPR_SECTION:
            StudySection (Expr, D);
    	    break;

	case EXPR_ULABEL:
            StudyULabel (Expr, D);
            break;

    	case EXPR_PLUS:
            StudyPlus (Expr, D);
    	    break;

    	case EXPR_MINUS:
       	    StudyMinus (Expr, D);
    	    break;

        case EXPR_MUL:
            StudyMul (Expr, D);
            break;

        case EXPR_DIV:
            StudyDiv (Expr, D);
            break;

        case EXPR_MOD:
            StudyMod (Expr, D);
            break;

        case EXPR_OR:
            StudyOr (Expr, D);
            break;

        case EXPR_XOR:
            StudyXor (Expr, D);
            break;

        case EXPR_AND:
            StudyAnd (Expr, D);
            break;

        case EXPR_SHL:
            StudyShl (Expr, D);
            break;

        case EXPR_SHR:
            StudyShr (Expr, D);
            break;

        case EXPR_EQ:
            StudyEQ (Expr, D);
            break;

        case EXPR_NE:
            StudyNE (Expr, D);
            break;

        case EXPR_LT:
            StudyLT (Expr, D);
            break;

        case EXPR_GT:
            StudyGT (Expr, D);
            break;

        case EXPR_LE:
            StudyLE (Expr, D);
            break;

        case EXPR_GE:
            StudyGE (Expr, D);
            break;

        case EXPR_BOOLAND:
            StudyBoolAnd (Expr, D);
            break;

        case EXPR_BOOLOR:
            StudyBoolOr (Expr, D);
            break;

        case EXPR_BOOLXOR:
            StudyBoolXor (Expr, D);
            break;

        case EXPR_UNARY_MINUS:
            StudyUnaryMinus (Expr, D);
            break;

        case EXPR_NOT:
            StudyNot (Expr, D);
            break;

        case EXPR_SWAP:
            StudySwap (Expr, D);
            break;

        case EXPR_BOOLNOT:
            StudyBoolNot (Expr, D);
            break;

        case EXPR_BYTE0:
            StudyByte0 (Expr, D);
            break;

        case EXPR_BYTE1:
            StudyByte1 (Expr, D);
            break;

        case EXPR_BYTE2:
            StudyByte2 (Expr, D);
            break;

        case EXPR_BYTE3:
            StudyByte3 (Expr, D);
            break;

        case EXPR_WORD0:
            StudyWord0 (Expr, D);
            break;

        case EXPR_WORD1:
            StudyWord1 (Expr, D);
            break;

        default:
	    Internal ("Unknown Op type: %u", Expr->Op);
    	    break;
    }
}



void StudyExpr (ExprNode* Expr, ExprDesc* D)
/* Study an expression tree and place the contents into D */
{
    unsigned I;

    /* Call the internal function */
    StudyExprInternal (Expr, D);

    /* Remove symbol references with count zero */
    I = 0;
    while (I < D->SymCount) {
        if (D->SymRef[I].Count == 0) {
            /* Delete the entry */
            --D->SymCount;
            memmove (D->SymRef + I, D->SymRef + I + 1,
                     (D->SymCount - I) * sizeof (D->SymRef[0]));
        } else {
            /* Next entry */
            ++I;
        }
    }

    /* Remove section references with count zero */
    I = 0;
    while (I < D->SecCount) {
        if (D->SecRef[I].Count == 0) {
            /* Delete the entry */
            --D->SecCount;
            memmove (D->SecRef + I, D->SecRef + I + 1,
                     (D->SecCount - I) * sizeof (D->SecRef[0]));
        } else {
            /* Next entry */
            ++I;
        }
    }

    /* If we don't have an address size, assign one of the expression is a
     * constant.
     */
    if (D->AddrSize == ADDR_SIZE_DEFAULT) {
        if (ED_IsConst (D)) {
            if ((D->Val & ~0xFFL) == 0) {
                D->AddrSize = ADDR_SIZE_ZP;
            } else if ((D->Val & ~0xFFFFL) == 0) {
                D->AddrSize = ADDR_SIZE_ABS;
            } else if ((D->Val & 0xFFFFFFL) == 0) {
                D->AddrSize = ADDR_SIZE_FAR;
            } else {
                D->AddrSize = ADDR_SIZE_LONG;
            }
        }
    }

#if 0
    printf ("StudyExpr: "); DumpExpr (Expr, SymResolve);
    if (!ED_IsValid (D)) {
        printf ("Invalid: %s\n", AddrSizeToStr (D->AddrSize));
    } else {
        printf ("Valid:   %s\n", AddrSizeToStr (D->AddrSize));
        printf ("%u symbols:\n", D->SymCount);
        printf ("%u sections:\n", D->SecCount);
    }
#endif
}




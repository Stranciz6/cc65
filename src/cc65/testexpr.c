/*****************************************************************************/
/*                                                                           */
/*                                testexpr.c                                 */
/*                                                                           */
/*                        Test an expression and jump                        */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2004      Ullrich von Bassewitz                                       */
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



#include "codegen.h"
#include "error.h"
#include "expr.h"
#include "scanner.h"
#include "testexpr.h"



/*****************************************************************************/
/*		      		     Code				     */
/*****************************************************************************/



unsigned Test (unsigned Label, int Invert)
/* Evaluate a boolean test expression and jump depending on the result of
 * the test and on Invert. The function returns one of the TESTEXPR_xx codes
 * defined above. If the jump is always true, a warning is output.
 */
{
    ExprDesc lval;
    unsigned Result;

    /* Evaluate the expression */
    expr (hie0, InitExprDesc (&lval));

    /* Check for a boolean expression */
    CheckBoolExpr (&lval);

    /* Check for a constant expression */
    if (ED_IsRVal (&lval) && lval.Flags == E_MCONST) {

        /* Result is constant, so we know the outcome */
        Result = (lval.ConstVal != 0);

      	/* Constant rvalue */
       	if (!Invert && lval.ConstVal == 0) {
      	    g_jump (Label);
     	    Warning ("Unreachable code");
     	} else if (Invert && lval.ConstVal != 0) {
 	    g_jump (Label);
      	}

    } else {

        /* Result is unknown */
        Result = TESTEXPR_UNKNOWN;

        /* If the expr hasn't set condition codes, set the force-test flag */
        if ((lval.Test & E_CC) == 0) {
            lval.Test |= E_FORCETEST;
        }

        /* Load the value into the primary register */
        ExprLoad (CF_FORCECHAR, &lval);

        /* Generate the jump */
        if (Invert) {
            g_truejump (CF_NONE, Label);
        } else {
            g_falsejump (CF_NONE, Label);
        }
    }

    /* Return the result */
    return Result;
}



unsigned TestInParens (unsigned Label, int Invert)
/* Evaluate a boolean test expression in parenthesis and jump depending on
 * the result of the test * and on Invert. The function returns one of the
 * TESTEXPR_xx codes defined above. If the jump is always true, a warning is
 * output.
 */
{
    unsigned Result;

    /* Eat the parenthesis */
    ConsumeLParen ();

    /* Do the test */
    Result = Test (Label, Invert);

    /* Check for the closing brace */
    ConsumeRParen ();

    /* Return the result of the expression */
    return Result;
}





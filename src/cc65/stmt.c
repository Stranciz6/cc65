/*
 * stmt.c
 *
 * Ullrich von Bassewitz, 06.08.1998
 *
 * Original by John R. Dunning - see copyleft.jrd
 */



#include <stdio.h>
#include <string.h>

#include "asmcode.h"
#include "asmlabel.h"
#include "codegen.h"
#include "datatype.h"
#include "error.h"
#include "expr.h"
#include "function.h"
#include "global.h"
#include "goto.h"
#include "litpool.h"
#include "locals.h"
#include "loop.h"
#include "mem.h"
#include "pragma.h"
#include "scanner.h"
#include "symtab.h"
#include "stmt.h"



/*****************************************************************************/
/*	  	  	       	     Data		     		     */
/*****************************************************************************/



/* Maximum count of cases */
#define CASE_MAX 	257



/*****************************************************************************/
/*	  	  		     Code		     		     */
/*****************************************************************************/



static int statement (void);
/* Forward decl */



static int doif (void)
/* Handle 'if' statement here */
{
    int flab1;
    int flab2;
    int gotbreak;

    /* Skip the if */
    gettok ();

    /* Generate a jump label and parse the condition */
    flab1 = GetLabel ();
    test (flab1, 0);

    /* Parse the if body */
    gotbreak = statement ();

    /* Else clause present? */
    if (curtok != ELSE) {

      	g_defloclabel (flab1);
     	/* Since there's no else clause, we're not sure, if the a break
     	 * statement is really executed.
     	 */
      	return 0;

    } else {

     	/* Skip the else */
     	gettok ();

     	/* If we had some sort of break statement at the end of the if clause,
     	 * there's no need to generate an additional jump around the else
     	 * clause, since the jump is never reached.
     	 */
     	if (!gotbreak) {
     	    flab2 = GetLabel ();
     	    g_jump (flab2);
     	} else {
     	    /* Mark the label as unused */
     	    flab2 = 0;
     	}
     	g_defloclabel (flab1);
     	gotbreak &= statement ();

     	/* Generate the label for the else clause */
     	if (flab2) {
     	    g_defloclabel (flab2);
     	}

     	/* Done */
     	return gotbreak;
    }
}



static void dowhile (char wtype)
/* Handle 'while' statement here */
{
    int loop;
    int lab;

    gettok ();
    loop = GetLabel ();
    lab = GetLabel ();
    addloop (oursp, loop, lab, 0, 0);
    g_defloclabel (loop);
    if (wtype == 'w') {

	/* While loop */
       	test (lab, 0);

	/* If the statement following the while loop is empty, that is, we have
	 * something like "while (1) ;", the test function ommitted the jump as
	 * an optimization. Since we know, the condition codes are set, we can
	 * do another small optimization here, and use a conditional jump
	 * instead an absolute one.
	 */
	if (curtok == SEMI) {
	    /* Shortcut */
	    gettok ();
	    /* Use a conditional jump */
	    g_truejump (CF_NONE, loop);
	} else {
	    /* There is code inside the while loop */
	    statement ();
	    g_jump (loop);
	    g_defloclabel (lab);
     	}

    } else {

	/* Do loop */
       	statement ();
	Consume (WHILE, ERR_WHILE_EXPECTED);
    	test (loop, 1);
    	ConsumeSemi ();
	g_defloclabel (lab);

    }
    delloop ();
}



static void doreturn (void)
/* Handle 'return' statement here */
{
    struct expent lval;
    unsigned etype = 0;		/* Type of return expression */
    int HaveVal = 0;		/* Do we have a return value in ax? */


    gettok ();
    if (curtok != SEMI) {
       	if (HasVoidReturn (CurrentFunc)) {
       	    Error (ERR_CANNOT_RETURN_VALUE);
       	}
       	if (evalexpr (CF_NONE, hie0, &lval) == 0) {
       	    /* Constant value */
       	    etype = CF_CONST;
       	} else {
	    /* Value in the primary register */
	    HaveVal = 1;
	}

	/* Convert the return value to the type of the function result */
	if (!HasVoidReturn (CurrentFunc)) {
       	    etype |= assignadjust (GetReturnType (CurrentFunc), &lval) & ~CF_CONST;
	}
    } else if (!HasVoidReturn (CurrentFunc)) {
       	Error (ERR_MUST_RETURN_VALUE);
    }
    RestoreRegVars (HaveVal);
    g_leave (etype, lval.e_const);
}



static void dobreak (void)
/* Handle 'break' statement here */
{
    struct loopdesc* l;

    gettok ();
    if ((l = currentloop ()) == 0) {
	/* Error: No current loop */
       	return;
    }
    g_space (oursp - l->sp);
    g_jump (l->label);
}



static void docontinue (void)
/* Handle 'continue' statement here */
{
    struct loopdesc* l;

    gettok ();
    if ((l = currentloop ()) == 0) {
	/* Error: Not in loop */
       	return;
    }
    do {
	if (l->loop) {
	    break;
	}
	l = l->next;
    } while (l);
    if (l == 0) {
       	Error (ERR_UNEXPECTED_CONTINUE);
       	return;
    }
    g_space (oursp - l->sp);
    if (l->linc) {
       	g_jump (l->linc);
    } else {
       	g_jump (l->loop);
    }
}



static void cascadeswitch (struct expent* eval)
/* Handle a switch statement for chars with a cmp cascade for the selector */
{
    unsigned exitlab;  		/* Exit label */
    unsigned nextlab;  		/* Next case label */
    unsigned codelab;		/* Label that starts the actual selector code */
    int havebreak;		/* Remember if we exited with break */
    int lcount;	       	       	/* Label count */
    unsigned flags;    		/* Code generator flags */
    struct expent lval;		/* Case label expression */
    long val;	       		/* Case label value */


    /* Create a loop so we may break out, init labels */
    exitlab = GetLabel ();
    addloop (oursp, 0, exitlab, 0, 0);

    /* Setup some variables needed in the loop  below */
    flags = TypeOf (eval->e_tptr) | CF_CONST | CF_FORCECHAR;
    codelab = nextlab = 0;
    havebreak = 1;

    /* Parse the labels */
    lcount = 0;
    while (curtok != RCURLY) {

	if (curtok == CASE || curtok == DEFAULT) {

	    /* If the code for the previous selector did not end with a
	     * break statement, we must jump over the next selector test.
	     */
	    if (!havebreak) {
		/* Define a label for the code */
		if (codelab == 0) {
		    codelab = GetLabel ();
		}
		g_jump (codelab);
	    }

	    /* If we have a cascade label, emit it */
	    if (nextlab) {
		g_defloclabel (nextlab);
		nextlab = 0;
	    }

	    while (curtok == CASE || curtok == DEFAULT) {

		/* Parse the selector */
		if (curtok == CASE) {

		    /* Count labels */
		    ++lcount;

		    /* Skip the "case" token */
		    gettok ();

		    /* Read the selector expression */
		    constexpr (&lval);
		    if (!IsInt (lval.e_tptr)) {
			Error (ERR_ILLEGAL_TYPE);
		    }

		    /* Check the range of the expression */
		    val = lval.e_const;
		    switch (*eval->e_tptr) {

			case T_CHAR:
			    /* Signed char */
			    if (val < -128 || val > 127) {
				Error (ERR_RANGE);
			    }
			    break;

			case T_UCHAR:
			    if (val < 0 || val > 255) {
				Error (ERR_RANGE);
			    }
			    break;

			case T_INT:
			    if (val < -32768 || val > 32767) {
				Error (ERR_RANGE);
			    }
			    break;

			case T_UINT:
			    if (val < 0 || val > 65535) {
				Error (ERR_RANGE);
			    }
			    break;

			default:
			    Internal ("Invalid type: %02X", *eval->e_tptr & 0xFF);
		    }

		    /* Skip the colon */
		    ConsumeColon ();

		    /* Emit a compare */
		    g_cmp (flags, val);

		    /* If another case follows, we will jump to the code if
		     * the condition is true.
		     */
		    if (curtok == CASE) {
			/* Create a code label if needed */
			if (codelab == 0) {
			    codelab = GetLabel ();
			}
			g_falsejump (CF_NONE, codelab);
		    } else if (curtok != DEFAULT) {
			/* No case follows, jump to next selector */
			if (nextlab == 0) {
			    nextlab = GetLabel ();
			}
			g_truejump (CF_NONE, nextlab);
		    }

		} else {

		    /* Default case */
		    gettok ();

		    /* Skip the colon */
		    ConsumeColon ();

		    /* Handle the pathologic case: DEFAULT followed by CASE */
		    if (curtok == CASE) {
			if (codelab == 0) {
			    codelab = GetLabel ();
			}
			g_jump (codelab);
		    }
		}

	    }

        }

	/* Emit a code label if we have one */
	if (codelab) {
	    g_defloclabel (codelab);
	    codelab = 0;
	}

	/* Parse statements */
	if (curtok != RCURLY) {
       	    havebreak = statement ();
	}
    }

    /* Check if we have any labels */
    if (lcount == 0) {
     	Warning (WARN_NO_CASE_LABELS);
    }

    /* Eat the closing curly brace */
    gettok ();

    /* Define the exit label and, if there's a next label left, create this
     * one, too.
     */
    if (nextlab) {
	g_defloclabel (nextlab);
    }
    g_defloclabel (exitlab);

    /* End the loop */
    delloop ();
}



static void tableswitch (struct expent* eval)
/* Handle a switch statement via table based selector */
{
    /* Entry for one case in a switch statement */
    struct swent {
    	long     sw_const;	/* selector value */
       	unsigned sw_lab; 	/* label for this selector */
    };

    int dlabel;	       	   	/* for default */
    int lab;   	       	   	/* exit label */
    int label; 	       	   	/* label for case */
    int lcase; 	       	       	/* label for compares */
    int lcount;	     	       	/* Label count */
    int havebreak;  		/* Last statement has a break */
    unsigned flags; 		/* Code generator flags */
    struct expent lval;		/* Case label expression */
    struct swent *p;
    struct swent *swtab;

    /* Allocate memory for the switch table */
    swtab = xmalloc (CASE_MAX * sizeof (struct swent));

    /* Create a look so we may break out, init labels */
    havebreak = 0;  		/* Keep gcc silent */
    dlabel = 0;	     	   	/* init */
    lab = GetLabel ();		/* get exit */
    p = swtab;
    addloop (oursp, 0, lab, 0, 0);

    /* Jump behind the code for the CASE labels */
    g_jump (lcase = GetLabel ());
    lcount = 0;
    while (curtok != RCURLY) {
    	if (curtok == CASE || curtok == DEFAULT) {
	    if (lcount >= CASE_MAX) {
       	       	Fatal (FAT_TOO_MANY_CASE_LABELS);
     	    }
    	    label = GetLabel ();
    	    do {
    	    	if (curtok == CASE) {
       	    	    gettok ();
    	    	    constexpr (&lval);
	    	    if (!IsInt (lval.e_tptr)) {
	    		Error (ERR_ILLEGAL_TYPE);
	      	    }
     	    	    p->sw_const = lval.e_const;
    	    	    p->sw_lab = label;
    	    	    ++p;
	    	    ++lcount;
    	    	} else {
    	    	    gettok ();
     	    	    dlabel = label;
    	    	}
    	    	ConsumeColon ();
    	    } while (curtok == CASE || curtok == DEFAULT);
    	    g_defloclabel (label);
	    havebreak = 0;
    	}
    	if (curtok != RCURLY) {
    	    havebreak = statement ();
    	}
    }

    /* Check if we have any labels */
    if (lcount == 0) {
     	Warning (WARN_NO_CASE_LABELS);
    }

    /* Eat the closing curly brace */
    gettok ();

    /* If the last statement doesn't have a break or return, add one */
    if (!havebreak) {
        g_jump (lab);
    }

    /* Actual selector code goes here */
    g_defloclabel (lcase);

    /* Create the call to the switch subroutine */
    flags = TypeOf (eval->e_tptr);
    g_switch (flags);

    /* First entry is negative of label count */
    g_defdata (CF_INT, -((int)lcount)-1, 0);

    /* Create the case selector table */
    AddCodeHint ("casetable");
    p = swtab;
    while (lcount) {
       	g_case (flags, p->sw_lab, p->sw_const);	/* Create one label */
	--lcount;
	++p;
    }

    if (dlabel) {
       	g_jump (dlabel);
    }
    g_defloclabel (lab);
    delloop ();

    /* Free the allocated space for the labels */
    xfree (swtab);
}



static void doswitch (void)
/* Handle 'switch' statement here */
{
    struct expent eval;		/* Switch statement expression */

    /* Eat the "switch" */
    gettok ();

    /* Read the switch expression */
    ConsumeLParen ();
    intexpr (&eval);
    ConsumeRParen ();

    /* result of expr is in P */
    ConsumeLCurly ();

    /* Now decide which sort of switch we will create: */
    if (IsChar (eval.e_tptr) || (FavourSize == 0 && IsInt (eval.e_tptr))) {
       	cascadeswitch (&eval);
    } else {
      	tableswitch (&eval);
    }
}



static void dofor (void)
/* Handle 'for' statement here */
{
    int loop;
    int lab;
    int linc;
    int lstat;
    struct expent lval1;
    struct expent lval2;
    struct expent lval3;

    gettok ();
    loop = GetLabel ();
    lab = GetLabel ();
    linc = GetLabel ();
    lstat = GetLabel ();
    addloop (oursp, loop, lab, linc, lstat);
    ConsumeLParen ();
    if (curtok != SEMI) {	/* exp1 */
	expression (&lval1);
    }
    ConsumeSemi ();
    g_defloclabel (loop);
    if (curtok != SEMI) { 	/* exp2 */
	boolexpr (&lval2);
	g_truejump (CF_NONE, lstat);
	g_jump (lab);
    } else {
	g_jump (lstat);
    }
    ConsumeSemi ();
    g_defloclabel (linc);
    if (curtok != RPAREN) {	/* exp3 */
    	expression (&lval3);
    }
    ConsumeRParen ();
    g_jump (loop);
    g_defloclabel (lstat);
    statement ();
    g_jump (linc);
    g_defloclabel (lab);
    delloop ();
}



static int statement (void)
/* Statement parser. Called whenever syntax requires a statement.
 * This routine performs that statement and returns 1 if it is a branch,
 * 0 otherwise
 */
{
    struct expent lval;

    /* */
    if (curtok == IDENT && nxttok == COLON) {

	/* Special handling for a label */
	DoLabel ();

    } else {

     	switch (curtok) {

     	    case LCURLY:
     		return compound ();

     	    case IF:
     		return doif ();

     	    case WHILE:
     		dowhile ('w');
		break;

	    case DO:
		dowhile ('d');
		break;

	    case SWITCH:
		doswitch ();
		break;

	    case RETURN:
		doreturn ();
		ConsumeSemi ();
		return 1;

	    case BREAK:
		dobreak ();
		ConsumeSemi ();
		return 1;

	    case CONTINUE:
		docontinue ();
		ConsumeSemi ();
		return 1;

	    case FOR:
		dofor ();
		break;

	    case GOTO:
		DoGoto ();
		ConsumeSemi ();
		return 1;

	    case SEMI:
		/* ignore it. */
		gettok ();
		break;

	    case PRAGMA:
		DoPragma ();
		break;

	    default:
    		AddCodeHint ("stmt:start");
		expression (&lval);
		AddCodeHint ("stmt:end");
		ConsumeSemi ();
	}
    }
    return 0;
}



int compound (void)
/* Compound statement. 	Allow any number of statements, inside braces. */
{
    static unsigned CurrentLevel = 0;

    int isbrk;
    int oldsp;

    /* eat LCURLY */
    gettok ();

    /* Remember the stack at block entry */
    oldsp = oursp;

    /* If we're not on function level, enter a new lexical level */
    if (CurrentLevel++ > 0) {
	/* A nested block */
    	EnterBlockLevel ();
    }

    /* Parse local variable declarations if any */
    DeclareLocals ();

    /* Now process statements in the function body */
    isbrk = 0;
    while (curtok != RCURLY) {
     	if (curtok == CEOF)
     	    break;
     	else {
     	    isbrk = statement ();
     	}
    }

    /* Emit references to imports/exports for this block */
    EmitExternals ();

    /* If this is not the top level compound statement, clean up the stack.
     * For a top level statement this will be done by the function exit code.
     */
    if (--CurrentLevel != 0) {
     	/* Some sort of nested block */
	LeaveBlockLevel ();
     	if (isbrk) {
     	    oursp = oldsp;
     	} else {
     	    g_space (oursp - oldsp);
     	    oursp = oldsp;
     	}
    }

    /* Eat closing brace */
    ConsumeRCurly ();

    return isbrk;
}




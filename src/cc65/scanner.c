/*****************************************************************************/
/*                                                                           */
/*				   scanner.c                                 */
/*                                                                           */
/*			Source file line info structure                      */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2003 Ullrich von Bassewitz                                       */
/*               Römerstrasse 52                                             */
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

/* common */
#include "chartype.h"
#include "tgttrans.h"

/* cc65 */
#include "datatype.h"
#include "error.h"
#include "function.h"
#include "global.h"
#include "hexval.h"
#include "ident.h"
#include "input.h"
#include "litpool.h"
#include "preproc.h"
#include "symtab.h"
#include "util.h"
#include "scanner.h"



/*****************************************************************************/
/*		  		     data				     */
/*****************************************************************************/



Token CurTok;		/* The current token */
Token NextTok;		/* The next token */



/* Token types */
#define TT_C 	0		/* ANSI C token */
#define TT_EXT	1 		/* cc65 extension */

/* Token table */
static const struct Keyword {
    char*    	    Key;    	/* Keyword name */
    unsigned char   Tok;    	/* The token */
    unsigned char   Type;      	/* Token type */
} Keywords [] = {
    { "_Pragma",        TOK_PRAGMA,     TT_C    },
    { "__AX__",	       	TOK_AX,		TT_C  	},
    { "__A__",	       	TOK_A,	       	TT_C   	},
    { "__EAX__",       	TOK_EAX,   	TT_C  	},
    { "__X__", 	       	TOK_X,		TT_C  	},
    { "__Y__", 	       	TOK_Y,		TT_C  	},
    { "__asm__",       	TOK_ASM,   	TT_C  	},
    { "__attribute__",	TOK_ATTRIBUTE,	TT_C  	},
    { "__far__",	TOK_FAR,	TT_C  	},
    { "__fastcall__",  	TOK_FASTCALL,   TT_C  	},
    { "__near__",      	TOK_NEAR,	TT_C  	},
    { "asm",   	       	TOK_ASM,   	TT_EXT 	},
    { "auto",  	       	TOK_AUTO,  	TT_C  	},
    { "break", 	       	TOK_BREAK, 	TT_C  	},
    { "case",  	       	TOK_CASE,  	TT_C  	},
    { "char",  	       	TOK_CHAR,  	TT_C  	},
    { "const", 	       	TOK_CONST, 	TT_C  	},
    { "continue",      	TOK_CONTINUE,   TT_C  	},
    { "default",       	TOK_DEFAULT,    TT_C  	},
    { "do",    	       	TOK_DO,    	TT_C  	},
    { "double",        	TOK_DOUBLE,	TT_C   	},
    { "else",  	       	TOK_ELSE,  	TT_C  	},
    { "enum",  	       	TOK_ENUM,  	TT_C  	},
    { "extern",        	TOK_EXTERN,	TT_C   	},
    { "far",		TOK_FAR,	TT_EXT	},
    { "fastcall",      	TOK_FASTCALL,	TT_EXT 	},
    { "float", 	       	TOK_FLOAT, 	TT_C  	},
    { "for",   	       	TOK_FOR,   	TT_C  	},
    { "goto",  	       	TOK_GOTO,  	TT_C  	},
    { "if",    	       	TOK_IF,    	TT_C  	},
    { "int",   	       	TOK_INT,   	TT_C  	},
    { "long",  	       	TOK_LONG,  	TT_C  	},
    { "near",          	TOK_NEAR,       TT_EXT 	},
    { "register",      	TOK_REGISTER,   TT_C  	},
    { "restrict",      	TOK_RESTRICT,   TT_C  	},
    { "return",        	TOK_RETURN,	TT_C  	},
    { "short", 	       	TOK_SHORT, 	TT_C  	},
    { "signed",	       	TOK_SIGNED,	TT_C  	},
    { "sizeof",        	TOK_SIZEOF,	TT_C  	},
    { "static",        	TOK_STATIC,	TT_C  	},
    { "struct",        	TOK_STRUCT,	TT_C  	},
    { "switch",        	TOK_SWITCH,	TT_C	},
    { "typedef",       	TOK_TYPEDEF,    TT_C	},
    { "union", 	       	TOK_UNION, 	TT_C	},
    { "unsigned",      	TOK_UNSIGNED,   TT_C	},
    { "void",  	       	TOK_VOID,  	TT_C	},
    { "volatile",      	TOK_VOLATILE,   TT_C	},
    { "while", 	       	TOK_WHILE, 	TT_C	},
};
#define KEY_COUNT	(sizeof (Keywords) / sizeof (Keywords [0]))



/* Stuff for determining the type of an integer constant */
#define IT_INT	 	0x01
#define IT_UINT	 	0x02
#define IT_LONG	 	0x04
#define IT_ULONG 	0x08



/*****************************************************************************/
/*		 		     code 				     */
/*****************************************************************************/



static int CmpKey (const void* Key, const void* Elem)
/* Compare function for bsearch */
{
    return strcmp ((const char*) Key, ((const struct Keyword*) Elem)->Key);
}



static int FindKey (const char* Key)
/* Find a keyword and return the token. Return IDENT if the token is not a
 * keyword.
 */
{
    struct Keyword* K;
    K = bsearch (Key, Keywords, KEY_COUNT, sizeof (Keywords [0]), CmpKey);
    if (K && (K->Type != TT_EXT || ANSI == 0)) {
	return K->Tok;
    } else {
	return TOK_IDENT;
    }
}



static int SkipWhite (void)
/* Skip white space in the input stream, reading and preprocessing new lines
 * if necessary. Return 0 if end of file is reached, return 1 otherwise.
 */
{
    while (1) {
       	while (CurC == 0) {
	    if (NextLine () == 0) {
	     	return 0;
     	    }
	    Preprocess ();
     	}
	if (IsSpace (CurC)) {
    	    NextChar ();
	} else {
    	    return 1;
	}
    }
}



void SymName (char* s)
/* Get symbol from input stream */
{
    unsigned k = 0;
    do {
       	if (k != MAX_IDENTLEN) {
       	    ++k;
       	    *s++ = CurC;
     	}
       	NextChar ();
    } while (IsIdent (CurC) || IsDigit (CurC));
    *s = '\0';
}



int IsSym (char *s)
/* Get symbol from input stream or return 0 if not a symbol. */
{
    if (IsIdent (CurC)) {
     	SymName (s);
     	return 1;
    } else {
     	return 0;
    }
}



static void UnknownChar (char C)
/* Error message for unknown character */
{
    Error ("Invalid input character with code %02X", C & 0xFF);
    NextChar (); 			/* Skip */
}



static void SetTok (int tok)
/* Set NextTok.Tok and bump line ptr */
{
    NextTok.Tok = tok;
    NextChar ();
}



static int ParseChar (void)
/* Parse a character. Converts \n into EOL, etc. */
{
    int I;
    unsigned Val;
    int C;

    /* Check for escape chars */
    if (CurC == '\\') {
	NextChar ();
	switch (CurC) {
	    case '?':
	       	C = '\?';
	      	break;
	    case 'a':
	       	C = '\a';
	      	break;
	    case 'b':
	       	C = '\b';
	      	break;
     	    case 'f':
	      	C = '\f';
	      	break;
	    case 'r':
	      	C = '\r';
	      	break;
	    case 'n':
	      	C = '\n';
	      	break;
	    case 't':
	      	C = '\t';
	      	break;
            case 'v':
                C = '\v';
                break;
	    case '\"':
	      	C = '\"';
	      	break;
	    case '\'':
	      	C = '\'';
	      	break;
	    case '\\':
	      	C = '\\';
	      	break;
	    case 'x':
	    case 'X':
	      	/* Hex character constant */
	      	NextChar ();
	       	Val = HexVal (CurC) << 4;
	      	NextChar ();
       	       	C = Val | HexVal (CurC); 	/* Do not translate */
	      	break;
	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
		/* Octal constant */
		I = 0;
       	       	Val = CurC - '0';
       	       	while (NextC >= '0' && NextC <= '7' && ++I <= 3) {
     	 	    NextChar ();
     	       	    Val = (Val << 3) | (CurC - '0');
     		}
                C = (int) Val;
                if (Val >= 256) {
                    Error ("Character constant out of range");
                    C = ' ';
                }
     		break;
     	    default:
     		Error ("Illegal character constant");
		C = ' ';
                /* Try to do error recovery, otherwise the compiler will spit
                 * out thousands of errors in this place and abort.
                 */
                if (CurC != '\'' && CurC != '\0') {
                    while (NextC != '\'' && NextC != '\"' && NextC != '\0') {
                        NextChar ();
                    }
                }
		break;
     	}
    } else {
     	C = CurC;
    }

    /* Skip the character read */
    NextChar ();

    /* Do correct sign extension */
    return SignExtendChar (C);
}



static void CharConst (void)
/* Parse a character constant. */
{
    int C;

    /* Skip the quote */
    NextChar ();

    /* Get character */
    C = ParseChar ();

    /* Check for closing quote */
    if (CurC != '\'') {
       	Error ("`\'' expected");
    } else {
	/* Skip the quote */
	NextChar ();
    }

    /* Setup values and attributes */
    NextTok.Tok  = TOK_CCONST;

    /* Translate into target charset */
    NextTok.IVal = SignExtendChar (TgtTranslateChar (C));

    /* Character constants have type int */
    NextTok.Type = type_int;
}



static void StringConst (void)
/* Parse a quoted string */
{
    NextTok.IVal = GetLiteralPoolOffs ();
    NextTok.Tok  = TOK_SCONST;

    /* Be sure to concatenate strings */
    while (CurC == '\"') {

	/* Skip the quote char */
	NextChar ();

	while (CurC != '\"') {
	    if (CurC == '\0') {
	     	Error ("Unexpected newline");
	     	break;
	    }
	    AddLiteralChar (ParseChar ());
	}

	/* Skip closing quote char if there was one */
     	NextChar ();

	/* Skip white space, read new input */
	SkipWhite ();

    }

    /* Terminate the string */
    AddLiteralChar ('\0');
}



static void NumericConst (void)
/* Parse a numeric constant */
{
    unsigned Base;              /* Temporary number base */
    unsigned Prefix;            /* Base according to prefix */
    StrBuf   S;
    int      IsFloat;
    char     C;
    unsigned DigitVal;
    unsigned long IVal;         /* Value */

    /* Check for a leading hex or octal prefix and determine the possible
     * integer types.
     */
    if (CurC == '0') {
        /* Gobble 0 and examine next char */
        NextChar ();
        if (toupper (CurC) == 'X') {
            Base = Prefix = 16;
            NextChar ();    	/* gobble "x" */
        } else {
            Base = 10;          /* Assume 10 for now - see below */
            Prefix = 8;         /* Actual prefix says octal */
        }
    } else {
        Base  = Prefix = 10;
    }

    /* Because floating point numbers don't have octal prefixes (a number
     * with a leading zero is decimal), we first have to read the number
     * before converting it, so we can determine if it's a float or an
     * integer.
     */
    InitStrBuf (&S);
    while (IsXDigit (CurC) && HexVal (CurC) < Base) {
        SB_AppendChar (&S, CurC);
        NextChar ();
    }
    SB_Terminate (&S);

    /* The following character tells us if we have an integer or floating
     * point constant.
     */
    IsFloat = (CurC == '.' ||
               (Base == 10 && toupper (CurC) == 'E') ||
               (Base == 16 && toupper (CurC) == 'P'));

    /* If we don't have a floating point type, an octal prefix results in an
     * octal base.
     */
    if (!IsFloat && Prefix == 8) {
        Base = 8;
    }

    /* Since we do now know the correct base, convert the remembered input
     * into a number.
     */
    SB_Reset (&S);
    IVal = 0;
    while ((C = SB_Get (&S)) != '\0') {
        DigitVal = HexVal (C);
        if (DigitVal >= Base) {
            Error ("Numeric constant contains digits beyond the radix");
        }
        IVal = (IVal * Base) + DigitVal;
    }

    /* We don't need the string buffer any longer */
    DoneStrBuf (&S);

    /* Distinguish between integer and floating point constants */
    if (!IsFloat) {

        unsigned Types;
        int      HaveSuffix;

        /* Check for a suffix and determine the possible types */
        HaveSuffix = 1;
        if (toupper (CurC) == 'U') {
            /* Unsigned type */
            NextChar ();
            if (toupper (CurC) != 'L') {
                Types = IT_UINT | IT_ULONG;
            } else {
                NextChar ();
                Types = IT_ULONG;
            }
        } else if (toupper (CurC) == 'L') {
            /* Long type */
            NextChar ();
            if (toupper (CurC) != 'U') {
                Types = IT_LONG | IT_ULONG;
            } else {
                NextChar ();
                Types = IT_ULONG;
            }
        } else {
            HaveSuffix = 0;
            if (Prefix == 10) {
                /* Decimal constants are of any type but uint */
                Types = IT_INT | IT_LONG | IT_ULONG;
            } else {
                /* Octal or hex constants are of any type */
                Types = IT_INT | IT_UINT | IT_LONG | IT_ULONG;
            }
        }

        /* Check the range to determine the type */
        if (IVal > 0x7FFF) {
            /* Out of range for int */
            Types &= ~IT_INT;
            /* If the value is in the range 0x8000..0xFFFF, unsigned int is not
             * allowed, and we don't have a type specifying suffix, emit a
             * warning, because the constant is of type long.
             */
            if (IVal <= 0xFFFF && (Types & IT_UINT) == 0 && !HaveSuffix) {
                Warning ("Constant is long");
            }
        }
        if (IVal > 0xFFFF) {
            /* Out of range for unsigned int */
            Types &= ~IT_UINT;
        }
        if (IVal > 0x7FFFFFFF) {
            /* Out of range for long int */
            Types &= ~IT_LONG;
        }

        /* Now set the type string to the smallest type in types */
        if (Types & IT_INT) {
            NextTok.Type = type_int;
        } else if (Types & IT_UINT) {
            NextTok.Type = type_uint;
        } else if (Types & IT_LONG) {
            NextTok.Type = type_long;
        } else {
            NextTok.Type = type_ulong;
        }

        /* Set the value and the token */
        NextTok.IVal = IVal;
        NextTok.Tok  = TOK_ICONST;

    } else {

        /* Float constant */
        double FVal = IVal;             /* Convert to float */

        /* Check for a fractional part and read it */
        if (CurC == '.') {

            unsigned Digits;
            unsigned long Frac;
            unsigned long Scale;

            /* Skip the dot */
            NextChar ();

            /* Read fractional digits. Since we support only 32 bit floats
             * with a maximum of 7 fractional digits, we read the fractional
             * part as integer with up to 8 digits and drop the remainder.
             * This avoids an overflow of Frac and Scale.
             */
            Digits = 0;
            Frac   = 0;
            Scale  = 1;
            while (IsXDigit (CurC) && (DigitVal = HexVal (CurC)) < Base) {
                if (Digits < 8) {
                    Frac = Frac * Base + DigitVal;
                    ++Digits;
                    Scale *= Base;
                }
                NextChar ();
            }

            /* Scale the fractional part and add it */
            if (Frac) {
                FVal += ((double) Frac) / ((double) Scale);
            }
        }

        /* Check for an exponent and read it */
        if ((Base == 16 && toupper (CurC) == 'F') ||
            (Base == 10 && toupper (CurC) == 'E')) {

            int Sign;
            unsigned Digits;
            unsigned Exp;

            /* Skip the exponent notifier */
            NextChar ();

            /* Read an optional sign */
            Sign = 1;
            if (CurC == '-') {
                Sign = -1;
                NextChar ();
            }

            /* Read exponent digits. Since we support only 32 bit floats
             * with a maximum exponent of +-/127, we read the exponent
             * part as integer with up to 3 digits and drop the remainder.
             * This avoids an overflow of Exp. The exponent is always
             * decimal, even for hex float consts.
             */
            Digits = 0;
            Exp    = 0;
            while (IsDigit (CurC)) {
                if (++Digits <= 3) {
                    Exp = Exp * 10 + HexVal (CurC);
                }
                NextChar ();
            }

            /* Check for errors: We must have exponent digits, and not more
             * than three.
             */
            if (Digits == 0) {
                Error ("Floating constant exponent has no digits");
            } else if (Digits > 3) {
                Warning ("Floating constant exponent is too large");
            }

            /* Scale the exponent and adjust the value accordingly */
            if (Exp) {
                FVal *= pow (10, Exp);
            }
        }

        /* Check for a suffix and determine the type of the constant */
        if (toupper (CurC) == 'F') {
            NextChar ();
            NextTok.Type = type_float;
        } else {
            NextTok.Type = type_double;
        }

        /* Set the value and the token */
        NextTok.FVal = FVal;
        NextTok.Tok  = TOK_FCONST;

    }
}



void NextToken (void)
/* Get next token from input stream */
{
    ident token;

    /* We have to skip white space here before shifting tokens, since the
     * tokens and the current line info is invalid at startup and will get
     * initialized by reading the first time from the file. Remember if
     * we were at end of input and handle that later.
     */
    int GotEOF = (SkipWhite() == 0);

    /* Current token is the lookahead token */
    if (CurTok.LI) {
	ReleaseLineInfo (CurTok.LI);
    }
    CurTok = NextTok;

    /* When reading the first time from the file, the line info in NextTok,
     * which was copied to CurTok is invalid. Since the information from
     * the token is used for error messages, we must make it valid.
     */
    if (CurTok.LI == 0) {
	CurTok.LI = UseLineInfo (GetCurLineInfo ());
    }

    /* Remember the starting position of the next token */
    NextTok.LI = UseLineInfo (GetCurLineInfo ());

    /* Now handle end of input. */
    if (GotEOF) {
	/* End of file reached */
	NextTok.Tok = TOK_CEOF;
	return;
    }

    /* Determine the next token from the lookahead */
    if (IsDigit (CurC)) {
     	/* A number */
        NumericConst ();
     	return;
    }

    if (IsSym (token)) {

     	/* Check for a keyword */
     	if ((NextTok.Tok = FindKey (token)) != TOK_IDENT) {
     	    /* Reserved word found */
     	    return;
     	}
     	/* No reserved word, check for special symbols */
     	if (token [0] == '_') {
     	    /* Special symbols */
            if (strcmp (token, "__FILE__") == 0) {
	       	NextTok.IVal = AddLiteral (GetCurrentFile());
	       	NextTok.Tok  = TOK_SCONST;
	       	return;
	    } else if (strcmp (token, "__LINE__") == 0) {
	       	NextTok.Tok  = TOK_ICONST;
    	       	NextTok.IVal = GetCurrentLine();
    	       	NextTok.Type = type_int;
    	       	return;
    	    } else if (strcmp (token, "__func__") == 0) {
	       	/* __func__ is only defined in functions */
	       	if (CurrentFunc) {
	       	    NextTok.IVal = AddLiteral (F_GetFuncName (CurrentFunc));
	       	    NextTok.Tok  = TOK_SCONST;
	       	    return;
	       	}
	    }
    	}

       	/* No reserved word but identifier */
	strcpy (NextTok.Ident, token);
     	NextTok.Tok = TOK_IDENT;
    	return;
    }

    /* Monstrous switch statement ahead... */
    switch (CurC) {

    	case '!':
	    NextChar ();
    	    if (CurC == '=') {
    		SetTok (TOK_NE);
    	    } else {
    		NextTok.Tok = TOK_BOOL_NOT;
    	    }
    	    break;

    	case '\"':
       	    StringConst ();
    	    break;

    	case '%':
	    NextChar ();
    	    if (CurC == '=') {
    		SetTok (TOK_MOD_ASSIGN);
    	    } else {
    		NextTok.Tok = TOK_MOD;
    	    }
    	    break;

    	case '&':
	    NextChar ();
    	    switch (CurC) {
    		case '&':
    		    SetTok (TOK_BOOL_AND);
    		    break;
    		case '=':
    		    SetTok (TOK_AND_ASSIGN);
    	      	    break;
    		default:
    		    NextTok.Tok = TOK_AND;
    	    }
    	    break;

    	case '\'':
    	    CharConst ();
    	    break;

    	case '(':
    	    SetTok (TOK_LPAREN);
    	    break;

    	case ')':
    	    SetTok (TOK_RPAREN);
    	    break;

    	case '*':
	    NextChar ();
    	    if (CurC == '=') {
    		SetTok (TOK_MUL_ASSIGN);
    	    } else {
    		NextTok.Tok = TOK_STAR;
    	    }
    	    break;

    	case '+':
	    NextChar ();
    	    switch (CurC) {
    	    	case '+':
    		    SetTok (TOK_INC);
    		    break;
    	     	case '=':
    		    SetTok (TOK_PLUS_ASSIGN);
    		    break;
    		default:
    		    NextTok.Tok = TOK_PLUS;
    	    }
    	    break;

    	case ',':
    	    SetTok (TOK_COMMA);
    	    break;

    	case '-':
	    NextChar ();
    	    switch (CurC) {
    	      	case '-':
    		    SetTok (TOK_DEC);
    		    break;
    		case '=':
    	    	    SetTok (TOK_MINUS_ASSIGN);
    		    break;
    		case '>':
    	    	    SetTok (TOK_PTR_REF);
    		    break;
    		default:
    		    NextTok.Tok = TOK_MINUS;
    	    }
    	    break;

    	case '.':     
            if (IsDigit (NextC)) {
                NumericConst ();
            } else {
                NextChar ();
                if (CurC == '.') {
                    NextChar ();
                    if (CurC == '.') {
                        SetTok (TOK_ELLIPSIS);
                    } else {
                        UnknownChar (CurC);
                    }
                } else {
                    NextTok.Tok = TOK_DOT;
                }
            }
    	    break;

    	case '/':
	    NextChar ();
    	    if (CurC == '=') {
    		SetTok (TOK_DIV_ASSIGN);
    	    } else {
    	     	NextTok.Tok = TOK_DIV;
    	    }
    	    break;

    	case ':':
    	    SetTok (TOK_COLON);
    	    break;

    	case ';':
    	    SetTok (TOK_SEMI);
    	    break;

    	case '<':
	    NextChar ();
    	    switch (CurC) {
    		case '=':
    	      	    SetTok (TOK_LE);
    	    	    break;
    		case '<':
		    NextChar ();
    		    if (CurC == '=') {
    		    	SetTok (TOK_SHL_ASSIGN);
    		    } else {
    		    	NextTok.Tok = TOK_SHL;
    	    	    }
    		    break;
    		default:
    		    NextTok.Tok = TOK_LT;
    	    }
    	    break;

    	case '=':
	    NextChar ();
       	    if (CurC == '=') {
    		SetTok (TOK_EQ);
    	    } else {
    		NextTok.Tok = TOK_ASSIGN;
    	    }
    	    break;

    	case '>':
	    NextChar ();
    	    switch (CurC) {
    		case '=':
    		    SetTok (TOK_GE);
    		    break;
    		case '>':
		    NextChar ();
    		    if (CurC == '=') {
    		    	SetTok (TOK_SHR_ASSIGN);
    		    } else {
    	     	    	NextTok.Tok = TOK_SHR;
    		    }
    		    break;
    		default:
    		    NextTok.Tok = TOK_GT;
    	    }
    	    break;

    	case '?':
    	    SetTok (TOK_QUEST);
    	    break;

    	case '[':
    	    SetTok (TOK_LBRACK);
    	    break;

    	case ']':
    	    SetTok (TOK_RBRACK);
    	    break;

    	case '^':
	    NextChar ();
    	    if (CurC == '=') {
    		SetTok (TOK_XOR_ASSIGN);
    	    } else {
    		NextTok.Tok = TOK_XOR;
    	    }
    	    break;

    	case '{':
    	    SetTok (TOK_LCURLY);
    	    break;

        case '|':
	    NextChar ();
    	    switch (CurC) {
    		case '|':
    		    SetTok (TOK_BOOL_OR);
    		    break;
    		case '=':
    		    SetTok (TOK_OR_ASSIGN);
    		    break;
    		default:
    		    NextTok.Tok = TOK_OR;
    	    }
    	    break;

    	case '}':
    	    SetTok (TOK_RCURLY);
    	    break;

    	case '~':
    	    SetTok (TOK_COMP);
    	    break;

    	default:
       	    UnknownChar (CurC);

    }

}



void SkipTokens (const token_t* TokenList, unsigned TokenCount)
/* Skip tokens until we reach TOK_CEOF or a token in the given token list.
 * This routine is used for error recovery.
 */
{
    while (CurTok.Tok != TOK_CEOF) {

    	/* Check if the current token is in the token list */
	unsigned I;
    	for (I = 0; I < TokenCount; ++I) {
    	    if (CurTok.Tok == TokenList[I]) {
    	    	/* Found a token in the list */
    	    	return;
    	    }
    	}

    	/* Not in the list: Skip it */
    	NextToken ();

    }
}



int Consume (token_t Token, const char* ErrorMsg)
/* Eat token if it is the next in the input stream, otherwise print an error
 * message. Returns true if the token was found and false otherwise.
 */
{
    if (CurTok.Tok == Token) {
	NextToken ();
        return 1;
    } else {
       	Error (ErrorMsg);
        return 0;
    }
}



int ConsumeColon (void)
/* Check for a colon and skip it. */
{
    return Consume (TOK_COLON, "`:' expected");
}



int ConsumeSemi (void)
/* Check for a semicolon and skip it. */
{
    /* Try do be smart about typos... */
    if (CurTok.Tok == TOK_SEMI) {
    	NextToken ();
        return 1;
    } else {
	Error ("`;' expected");
	if (CurTok.Tok == TOK_COLON || CurTok.Tok == TOK_COMMA) {
	    NextToken ();
	}
        return 0;
    }
}



int ConsumeComma (void)
/* Check for a comma and skip it. */
{
    /* Try do be smart about typos... */
    if (CurTok.Tok == TOK_COMMA) {
    	NextToken ();
        return 1;
    } else {
      	Error ("`,' expected");
	if (CurTok.Tok == TOK_SEMI) {
	    NextToken ();
	}
        return 0;
    }
}



int ConsumeLParen (void)
/* Check for a left parenthesis and skip it */
{
    return Consume (TOK_LPAREN, "`(' expected");
}



int ConsumeRParen (void)
/* Check for a right parenthesis and skip it */
{
    return Consume (TOK_RPAREN, "`)' expected");
}



int ConsumeLBrack (void)
/* Check for a left bracket and skip it */
{
    return Consume (TOK_LBRACK, "`[' expected");
}



int ConsumeRBrack (void)
/* Check for a right bracket and skip it */
{
    return Consume (TOK_RBRACK, "`]' expected");
}



int ConsumeLCurly (void)
/* Check for a left curly brace and skip it */
{
    return Consume (TOK_LCURLY, "`{' expected");
}



int ConsumeRCurly (void)
/* Check for a right curly brace and skip it */
{
    return Consume (TOK_RCURLY, "`}' expected");
}




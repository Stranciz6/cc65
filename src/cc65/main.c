/* CC65 main program */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "../common/version.h"

#include "asmcode.h"
#include "asmlabel.h"
#include "codegen.h"
#include "datatype.h"
#include "declare.h"
#include "error.h"
#include "expr.h"
#include "function.h"
#include "global.h"
#include "include.h"
#include "io.h"
#include "litpool.h"
#include "macrotab.h"
#include "mem.h"
#include "optimize.h"
#include "pragma.h"
#include "scanner.h"
#include "stmt.h"
#include "symtab.h"



/*****************************************************************************/
/*		  		     data				     */
/*****************************************************************************/



/* Names of the target systems sorted by target name */
static const char* TargetNames [] = {
    "none",
    "atari",
    "c64",
    "c128",
    "ace",
    "plus4",
    "cbm610",
    "pet",
    "nes",
    "apple2",
    "geos",
};



/*****************************************************************************/
/*				     code				     */
/*****************************************************************************/



static void usage (int ExitCode)
{
    fputs ("Usage: cc65 [options] file\n"
	   "\t-d\t\tDebug mode\n"
	   "\t-g\t\tAdd debug info to object files\n"
	   "\t-h\t\tPrint this help\n"
	   "\t-j\t\tDefault characters are signed\n"
	   "\t-o name\t\tName the output file\n"
	   "\t-tx\t\tSet target system x\n"
	   "\t-v\t\tVerbose mode\n"
	   "\t-A\t\tStrict ANSI mode\n"
	   "\t-Cl\t\tMake local variables static\n"
	   "\t-Dsym[=defn]\tDefine a symbol\n"
	   "\t-I path\t\tSet include directory\n"
	   "\t-O\t\tOptimize code\n"
	   "\t-Oi\t\tOptimize code, inline more code\n"
	   "\t-Or\t\tEnable register variables\n"
	   "\t-Os\t\tInline some known functions\n"
	   "\t-T\t\tInclude source as comment\n"
	   "\t-V\t\tPrint version number\n"
	   "\t-W\t\tSuppress warnings\n",
	   stderr);
    exit (ExitCode);
}



static char* GetArg (int* ArgNum, char* argv [], unsigned Len)
/* Get an option argument */
{
    char* Arg = argv [*ArgNum];
    if (Arg [Len] != '\0') {
	/* Argument appended */
	return Arg + Len;
    } else {
	/* Separate argument */
	Arg = argv [*ArgNum + 1];
	if (Arg == 0) {
	    /* End of arguments */
	    fprintf (stderr, "Option requires an argument: %s\n", argv [*ArgNum]);
	    exit (EXIT_FAILURE);
	}
	++(*ArgNum);
	return Arg;
    }
}



/* Define a CBM system */
static void cbmsys (const char* sys)
{
    AddNumericMacro ("__CBM__", 1);
    AddNumericMacro (sys, 1);
}



static int MapSys (const char* Name)
/* Map a target name to a system code. Return -1 in case of an error */
{
    unsigned I;

    /* Check for a numeric target */
    if (isdigit (*Name)) {
	int Target = atoi (Name);
	if (Target >= 0 && Target < TGT_COUNT) {
	    return Target;
	}
    }

    /* Check for a target string */
    for (I = 0; I < TGT_COUNT; ++I) {
	if (strcmp (TargetNames [I], Name) == 0) {
	    return I;
	}
    }
    /* Not found */
    return -1;
}



/* Define a target system */
static void SetSys (const char* Sys)
{
    switch (Target = MapSys (Sys)) {

	case TGT_NONE:
	    break;

	case TGT_ATARI:
	    AddNumericMacro ("__ATARI__", 1);
	    break;

	case TGT_C64:
	    cbmsys ("__C64__");
	    break;

	case TGT_C128:
	    cbmsys ("__C128__");
	    break;

	case TGT_ACE:
	    cbmsys ("__ACE__");
	    break;

	case TGT_PLUS4:
	    cbmsys ("__PLUS4__");
	    break;

	case TGT_CBM610:
	    cbmsys ("__CBM610__");
	    break;

	case TGT_PET:
	    cbmsys ("__PET__");
	    break;

	case TGT_NES:
	    AddNumericMacro ("__NES__", 1);
	    break;

	case TGT_APPLE2:
	    AddNumericMacro ("__APPLE2__", 1);
	    break;

	case TGT_GEOS:
	    /* Do not handle as a CBM system */
	    AddNumericMacro ("__GEOS__", 1);
	    break;

	default:
	    fputs ("Unknown system type\n", stderr);
	    usage (EXIT_FAILURE);
    }
}



static void InvSym (const char* Def)
/* Print an error about an invalid macro definition and die */
{
    fprintf (stderr, "Invalid macro definition: `%s'\n", Def);
    exit (EXIT_FAILURE);
}



static void DefineSym (const char* Def)
/* Define a symbol on the command line */
{
    const char* P = Def;

    /* The symbol must start with a character or underline */
    if (Def [0] != '_' && !isalpha (Def [0])) {
	InvSym (Def);
    }

    /* Check the symbol name */
    while (isalnum (*P) || *P == '_') {
	++P;
    }

    /* Do we have a value given? */
    if (*P != '=') {
	if (*P != '\0') {
	    InvSym (Def);
	}
	/* No value given. Define the macro with the value 1 */
	AddNumericMacro (Def, 1);
    } else {
	/* We have a value, P points to the '=' character. Since the argument
	 * is const, create a copy and replace the '=' in the copy by a zero
	 * terminator.
       	 */
	char* Q;
	unsigned Len = strlen (Def)+1;
	char* S = xmalloc (Len);
	memcpy (S, Def, Len);
	Q = S + (P - Def);
	*Q++ = '\0';

    	/* Define this as a macro */
    	AddTextMacro (S, Q);

	/* Release the allocated memory */
	xfree (S);
    }
}



static void Parse (void)
/* Process all input text.
 * At this level, only static declarations, defines, includes, and function
 * definitions are legal....
 */
{
    int comma;
    SymEntry* Entry;

    kill ();
    gettok ();		 	/* "prime" the pump */
    gettok ();
    while (curtok != CEOF) {

	DeclSpec 	Spec;
	Declaration 	Decl;
	int		NeedStorage;

	/* Check for an ASM statement (which is allowed also on global level) */
	if (curtok == ASM) {
	    doasm ();
	    ConsumeSemi ();
	    continue;
	}

	/* Check for a #pragma */
	if (curtok == PRAGMA) {
	    DoPragma ();
	    continue;
	}

       	/* Read variable defs and functions */
	ParseDeclSpec (&Spec, SC_EXTERN | SC_STATIC, T_INT);

	/* Don't accept illegal storage classes */
	if (Spec.StorageClass == SC_AUTO || Spec.StorageClass == SC_REGISTER) {
	    Error (ERR_ILLEGAL_STORAGE_CLASS);
	    Spec.StorageClass = SC_EXTERN | SC_STATIC;
	}

	/* Check if this is only a type declaration */
	if (curtok == SEMI) {
	    gettok ();
	    continue;
	}

       	/* Check if we must reserve storage for the variable. We do
	 * this if we don't had a storage class given ("int i") or
	 * if the storage class is explicitly specified as static.
	 * This means that "extern int i" will not get storage
	 * allocated.
	 */
	NeedStorage = (Spec.StorageClass & SC_TYPEDEF) == 0 &&
		      ((Spec.Flags & DS_DEF_STORAGE) != 0  ||
	   	      (Spec.StorageClass & (SC_STATIC | SC_EXTERN)) == SC_STATIC);

	/* Read declarations for this type */
	Entry = 0;
	comma = 0;
       	while (1) {

	    unsigned SymFlags;

	    /* Read the next declaration */
	    ParseDecl (&Spec, &Decl, DM_NEED_IDENT);
	    if (Decl.Ident[0] == '\0') {
	    	gettok ();
	    	break;
	    }

	    /* Get the symbol flags */
	    SymFlags = Spec.StorageClass;
	    if (IsFunc (Decl.Type)) {
		SymFlags |= SC_FUNC;
	    } else {
	    	if (NeedStorage) {
		    /* We will allocate storage, variable is defined */
		    SymFlags |= SC_STORAGE | SC_DEF;
		}
	    }

	    /* Add an entry to the symbol table */
	    Entry = AddGlobalSym (Decl.Ident, Decl.Type, SymFlags);

	    /* Reserve storage for the variable if we need to */
       	    if (SymFlags & SC_STORAGE) {

	     	/* Get the size of the variable */
	     	unsigned Size = SizeOf (Decl.Type);

	     	/* Allow initialization */
	     	if (curtok == ASGN) {

	     	    /* We cannot initialize types of unknown size, or
	     	     * void types in non ANSI mode.
	     	     */
       	       	    if (Size == 0) {
	     		if (!IsVoid (Decl.Type)) {
	     		    if (!IsArray (Decl.Type)) {
	     		      	/* Size is unknown and not an array */
	     		      	Error (ERR_UNKNOWN_SIZE);
	     		    }
	     		} else if (ANSI) {
	     		    /* We cannot declare variables of type void */
	     		    Error (ERR_ILLEGAL_TYPE);
	     		}
	     	    }

	     	    /* Switch to the data segment */
	     	    g_usedata ();

	     	    /* Define a label */
	     	    g_defgloblabel (Entry->Name);

	     	    /* Skip the '=' */
	     	    gettok ();

	     	    /* Parse the initialization */
	     	    ParseInit (Entry->Type);
	     	} else {

	     	    if (IsVoid (Decl.Type)) {
	     	    	/* We cannot declare variables of type void */
	     		Error (ERR_ILLEGAL_TYPE);
	     	    } else if (Size == 0) {
	     		/* Size is unknown */
	     		Error (ERR_UNKNOWN_SIZE);
	     	    }

	     	    /* Switch to the BSS segment */
	     	    g_usebss ();

	     	    /* Define a label */
	     	    g_defgloblabel (Entry->Name);

	     	    /* Allocate space for uninitialized variable */
	     	    g_res (SizeOf (Entry->Type));
	     	}

	    }

	    /* Check for end of declaration list */
	    if (curtok == COMMA) {
	    	gettok ();
	    	comma = 1;
	    } else {
	     	break;
	    }
	}

	/* Function declaration? */
	if (IsFunc (Decl.Type)) {

	    /* Function */
	    if (!comma) {

	     	if (curtok == SEMI) {

	     	    /* Prototype only */
	     	    gettok ();

	     	} else {
	     	    if (Entry) {
	     	        NewFunc (Entry);
	     	    }
	     	}
	    }

	} else {

	    /* Must be followed by a semicolon */
	    ConsumeSemi ();

	}
    }
}



static void Compile (void)
/* Compiler begins execution here. inp is input fd, output is output fd. */
{
    char* Path;


    /* Setup variables */
    filetab[0].f_iocb = inp;
    LiteralLabel = GetLabel ();

    /* Add some standard paths to the include search path */
    AddIncludePath ("", INC_USER);		/* Current directory */
    AddIncludePath ("include", INC_SYS);
#ifdef CC65_INC
    AddIncludePath (CC65_INC, INC_SYS);
#else
    AddIncludePath ("/usr/lib/cc65/include", INC_SYS);
#endif
    Path = getenv ("CC65_INC");
    if (Path) {
	AddIncludePath (Path, INC_SYS | INC_USER);
    }

    /* Add macros that are always defined */
    AddNumericMacro ("__CC65__", (VER_MAJOR * 0x100) + (VER_MINOR * 0x10) + VER_PATCH);

    /* Strict ANSI macro */
    if (ANSI) {
	AddNumericMacro ("__STRICT_ANSI__", 1);
    }

    /* Optimization macros */
    if (Optimize) {
	AddNumericMacro ("__OPT__", 1);
	if (FavourSize == 0) {
	    AddNumericMacro ("__OPT_i__", 1);
	}
	if (EnableRegVars) {
	    AddNumericMacro ("__OPT_r__", 1);
	}
	if (InlineStdFuncs) {
	    AddNumericMacro ("__OPT_s__", 1);
	}
    }

    /* Create the base lexical level */
    EnterGlobalLevel ();

    /* Generate the code generator preamble */
    g_preamble ();

    /* Ok, start the ball rolling... */
    Parse ();

    /* Dump literal pool. */
    DumpLiteralPool ();

    /* Write imported/exported symbols */
    EmitExternals ();

    if (Debug) {
	PrintLiteralStats (stdout);
	PrintMacroStats (stdout);
    }

    /* Leave the main lexical level */
    LeaveGlobalLevel ();

    /* Print an error report */
    ErrorReport ();
}



int main (int argc, char **argv)
{
    int i;
    char *argp;
    char out_name [256];
    char* p;

    /* Initialize the output file name */
    out_name [0] = '\0';

    fin = NULL;

    /* Parse the command line */
    for (i = 1; i < argc; i++) {
	if (*(argp = argv[i]) == '-') {
	    switch (argp[1]) {

		case 'd':	/* debug mode */
		    Debug = 1;
		    break;

		case 'h':
		case '?':
		    usage (EXIT_SUCCESS);
		    break;

	    	case 'g':
	    	    DebugInfo = 1;
	    	    break;

		case 'j':
		    SignedChars = 1;
		    break;

		case 'o':
		    strcpy (out_name, GetArg (&i, argv, 2));
		    break;

		case 't':
		    SetSys (GetArg (&i, argv, 2));
		    break;

		case 'v':
		    ++Verbose;
		    break;

		case 'A':
		    ANSI = 1;
		    break;

		case 'C':
		    p = argp + 2;
		    while (*p) {
			switch (*p++) {
			    case 'l':
			    	LocalsAreStatic = 1;
				break;
			}
		    }
		    break;

		case 'D':
		    DefineSym (GetArg (&i, argv, 2));
		    break;

		case 'I':
		    AddIncludePath (GetArg (&i, argv, 2), INC_SYS | INC_USER);
		    break;

		case 'O':
		    Optimize = 1;
		    p = argp + 2;
		    while (*p) {
			switch (*p++) {
			    case 'f':
			    	sscanf (p, "%lx", (long*) &OptDisable);
				break;
			    case 'i':
			    	FavourSize = 0;
			    	break;
			    case 'r':
				EnableRegVars = 1;
				break;
			    case 's':
				InlineStdFuncs = 1;
				break;
			}
		    }
		    break;

		case 'T':
		    IncSource = 1;
		    break;

		case 'V':
		    fprintf (stderr, "cc65 V%u.%u.%u\n",
		   	     VER_MAJOR, VER_MINOR, VER_PATCH);
		    break;

		case 'W':
		    NoWarn = 1;
		    break;

		default:
		    fprintf (stderr, "Invalid option %s\n", argp);
		    usage (EXIT_FAILURE);
	    }
	} else {
	    if (fin) {
		fprintf (stderr, "additional file specs ignored\n");
	    } else {
		fin = xstrdup (argp);
		inp = fopen (fin, "r");
		if (inp == 0) {
		    Fatal (FAT_CANNOT_OPEN_INPUT, strerror (errno));
		}
	    }
	}
    }
    if (!fin) {
	fprintf (stderr, "%s: No input files\n", argv [0]);
	exit (EXIT_FAILURE);
    }

    /* Create the output file name. We should really have
     * some checks for string overflow, but I'll drop this because of the
     * additional code size it would need (as in other places). Sigh.
     */
    if (out_name [0] == '\0') {
	/* No output name given, create default */
     	strcpy (out_name, fin);
     	if ((p = strrchr (out_name, '.'))) {
     	    *p = '\0';
     	}
     	strcat (out_name, ".s");
    }

    /* Go! */
    Compile ();

    /* Create the output file if we didn't had any errors */
    if (ErrorCount == 0 || Debug) {

	FILE* F;

	/* Optimize the output if requested */
	if (Optimize) {
	    OptDoOpt ();
	}

	/* Open the file */
	F = fopen (out_name, "w");
	if (F == 0) {
	    Fatal (FAT_CANNOT_OPEN_OUTPUT, strerror (errno));
	}

	/* Write the output to the file */
	WriteOutput (F);

	/* Close the file, check for errors */
	if (fclose (F) != 0) {
	    remove (out_name);
	    Fatal (FAT_CANNOT_WRITE_OUTPUT);
	}
    }

    /* Return an apropriate exit code */
    return (ErrorCount > 0)? EXIT_FAILURE : EXIT_SUCCESS;
}



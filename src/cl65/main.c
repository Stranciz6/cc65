/*****************************************************************************/
/*									     */
/*				    main.c				     */
/*									     */
/*	       Main module for the cl65 compile and link utility	     */
/*									     */
/*									     */
/*									     */
/* (C) 1999-2000 Ullrich von Bassewitz					     */
/*		 Wacholderweg 14					     */
/*		 D-70597 Stuttgart					     */
/* EMail:	 uz@musoftware.de					     */
/*									     */
/*									     */
/* This software is provided 'as-is', without any expressed or implied	     */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.				     */
/*									     */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:			     */
/*									     */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.					     */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.			     */
/* 3. This notice may not be removed or altered from any source		     */
/*    distribution.							     */
/*									     */
/*****************************************************************************/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef __WATCOMC__
#  include <process.h>		/* DOS, OS/2 and Windows */
#else
#  include "spawn.h"		/* All others */
#endif

#include "../common/version.h"

#include "global.h"
#include "error.h"
#include "mem.h"



/*****************************************************************************/
/*				     Data				     */
/*****************************************************************************/



/* Struct that describes a command */
typedef struct CmdDesc_ CmdDesc;
struct CmdDesc_ {
    char*	Name;		/* The command name */

    unsigned	ArgCount;	/* Count of arguments */
    unsigned	ArgMax;		/* Maximum count of arguments */
    char**	Args;		/* The arguments */

    unsigned	FileCount;	/* Count of files to translate */
    unsigned	FileMax;	/* Maximum count of files */
    char**	Files;		/* The files */
};

/* Command descriptors for the different programs */
static CmdDesc CC65 = { 0, 0, 0, 0, 0, 0, 0 };
static CmdDesc CA65 = { 0, 0, 0, 0, 0, 0, 0 };
static CmdDesc LD65 = { 0, 0, 0, 0, 0, 0, 0 };

/* File types */
enum {
    FILETYPE_UNKNOWN,
    FILETYPE_C,
    FILETYPE_ASM,
    FILETYPE_OBJ,
    FILETYPE_LIB
};

/* Default file type, used if type unknown */
static unsigned DefaultFileType = FILETYPE_UNKNOWN;

/* Variables controlling the steps we're doing */
static int DontLink	= 0;
static int DontAssemble = 0;

/* The name of the output file, NULL if none given */
static const char* OutputName = 0;

/* The name of the linker configuration file if given */
static const char* LinkerConfig = 0;

/* The name of the first input file. This will be used to construct the
 * executable file name if no explicit name is given.
 */
static const char* FirstInput = 0;

/* The target system */
enum {
    TGT_UNKNOWN = -1,
    TGT_NONE,
    TGT_FIRSTREAL,
    TGT_ATARI = TGT_FIRSTREAL,
    TGT_C64,
    TGT_C128,
    TGT_ACE,
    TGT_PLUS4,
    TGT_CBM610,
    TGT_PET,
    TGT_NES,
    TGT_APPLE2,
    TGT_GEOS,
    TGT_COUNT
} Target = TGT_UNKNOWN;

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

/* Name of the crt0 object file and the runtime library */
static char* TargetCRT0 = 0;
static char* TargetLib	= 0;



/*****************************************************************************/
/*	   			String handling				     */
/*****************************************************************************/



static const char* FindExt (const char* Name)
/* Return a pointer to the file extension in Name or NULL if there is none */
{
    const char* S;

    /* Get the length of the name */
    unsigned Len = strlen (Name);
    if (Len < 2) {
	return 0;
    }

    /* Get a pointer to the last character */
    S = Name + Len - 1;

    /* Search for the dot, beware of subdirectories */
    while (S >= Name && *S != '.' && *S != '\\' && *S != '/') {
	--S;
    }

    /* Did we find an extension? */
    if (*S == '.') {
	return S;
    } else {
	return 0;
    }
}



static char* ForceExt (const char* Name, const char* Ext)
/* Return a new filename with the new extension */
{
    char* Out;
    const char* P = FindExt (Name);
    if (P == 0) {
	/* No dot, add the extension */
	Out = Xmalloc (strlen (Name) + strlen (Ext) + 1);
	strcpy (Out, Name);
	strcat (Out, Ext);
    } else {
	Out = Xmalloc (P - Name + strlen (Ext) + 1);
	memcpy (Out, Name, P - Name);
	strcpy (Out + (P - Name), Ext);
    }
    return Out;
}



/*****************************************************************************/
/*			     Determine a file type			     */
/*****************************************************************************/



static unsigned GetFileType (const char* File)
/* Determine the type of the given file */
{
    /* Table mapping extensions to file types */
    static const struct {
	const char*	Ext;
	unsigned	Type;
    } FileTypes [] = {
	{   ".c",	FILETYPE_C	},
	{   ".s",	FILETYPE_ASM	},
	{   ".asm",	FILETYPE_ASM	},
	{   ".o",	FILETYPE_OBJ	},
	{   ".obj",	FILETYPE_OBJ	},
	{   ".a",	FILETYPE_LIB	},
	{   ".lib",	FILETYPE_LIB	},
    };

    unsigned I;

    /* Determine the file type by the extension */
    const char* Ext = FindExt (File);

    /* Do we have an extension? */
    if (Ext == 0) {
	return DefaultFileType;
    }

    /* Check for known extensions */
    for (I = 0; I < sizeof (FileTypes) / sizeof (FileTypes [0]); ++I) {
	if (strcmp (FileTypes [I].Ext, Ext) == 0) {
	    /* Found */
	    return FileTypes [I].Type;
	}
    }

    /* Not found, return the default */
    return DefaultFileType;
}



/*****************************************************************************/
/*			  Command structure handling			     */
/*****************************************************************************/



static void CmdAddArg (CmdDesc* Cmd, const char* Arg)
/* Add a new argument to the command */
{
    /* Expand the argument vector if needed */
    if (Cmd->ArgCount == Cmd->ArgMax) {
	unsigned NewMax  = Cmd->ArgMax + 10;
	char**	 NewArgs = Xmalloc (NewMax * sizeof (char*));
	memcpy (NewArgs, Cmd->Args, Cmd->ArgMax * sizeof (char*));
	Xfree (Cmd->Args);
	Cmd->Args   = NewArgs;
	Cmd->ArgMax = NewMax;
    }

    /* Add a copy of the new argument, allow a NULL pointer */
    if (Arg) {
	Cmd->Args [Cmd->ArgCount++] = StrDup (Arg);
    } else {
	Cmd->Args [Cmd->ArgCount++] = 0;
    }
}



static void CmdDelArgs (CmdDesc* Cmd, unsigned LastValid)
/* Remove all arguments with an index greater than LastValid */
{
    while (Cmd->ArgCount > LastValid) {
	Cmd->ArgCount--;
	Xfree (Cmd->Args [Cmd->ArgCount]);
	Cmd->Args [Cmd->ArgCount] = 0;
    }
}



static void CmdAddFile (CmdDesc* Cmd, const char* File)
/* Add a new file to the command */
{
    /* Expand the file vector if needed */
    if (Cmd->FileCount == Cmd->FileMax) {
	unsigned NewMax   = Cmd->FileMax + 10;
	char**	 NewFiles = Xmalloc (NewMax * sizeof (char*));
	memcpy (NewFiles, Cmd->Files, Cmd->FileMax * sizeof (char*));
	Xfree (Cmd->Files);
	Cmd->Files   = NewFiles;
	Cmd->FileMax = NewMax;
    }

    /* Add a copy of the file name, allow a NULL pointer */
    if (File) {
	Cmd->Files [Cmd->FileCount++] = StrDup (File);
    } else {
	Cmd->Files [Cmd->FileCount++] = 0;
    }
}



static void CmdInit (CmdDesc* Cmd, const char* Path)
/* Initialize the command using the given path to the executable */
{
    /* Remember the command */
    Cmd->Name = StrDup (Path);

    /* Use the command name as first argument */
    CmdAddArg (Cmd, Path);
}



static void CmdSetOutput (CmdDesc* Cmd, const char* File)
/* Set the output file in a command desc */
{
    CmdAddArg (Cmd, "-o");
    CmdAddArg (Cmd, File);
}



static void CmdSetTarget (CmdDesc* Cmd, int Target)
/* Set the output file in a command desc */
{
    if (Target == TGT_UNKNOWN) {
	/* Use C64 as default */
	Target = TGT_C64;
    }

    if (Target != TGT_NONE) {
	CmdAddArg (Cmd, "-t");
	CmdAddArg (Cmd, TargetNames[Target]);
    }
}



/*****************************************************************************/
/*	   			Target handling				     */
/*****************************************************************************/



static int MapTarget (const char* Name)
/* Map a target name to a system code. Abort on errors */
{
    int I;

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
    Error ("No such target system: `%s'", Name);
    return -1;	/* Not reached */
}



static void SetTargetFiles (void)
/* Set the target system files */
{
    /* Determine the names of the default startup and library file */
    if (Target >= TGT_FIRSTREAL) {

 	/* Get a pointer to the system name and its length */
 	const char* TargetName = TargetNames [Target];
 	unsigned    TargetNameLen = strlen (TargetName);

 	/* Set the startup file */
 	TargetCRT0 = Xmalloc (TargetNameLen + 2 + 1);
 	strcpy (TargetCRT0, TargetName);
 	strcat (TargetCRT0, ".o");

 	/* Set the library file */
 	TargetLib = Xmalloc (TargetNameLen + 4 + 1);
 	strcpy (TargetLib, TargetName);
 	strcat (TargetLib, ".lib");

    }
}



static void SetTargetByName (const char* Name)
/* Set the target system by name */
{
    Target = MapTarget (Name);
    SetTargetFiles ();
}



/*****************************************************************************/
/*	    		   	 Subprocesses				     */
/*****************************************************************************/



static void ExecProgram (CmdDesc* Cmd)
/* Execute a subprocess with the given name/parameters. Exit on errors. */
{
    /* Call the program */
    int Status = spawnvp (P_WAIT, Cmd->Name, Cmd->Args);

    /* Check the result code */
    if (Status < 0) {
	/* Error executing the program */
	Error ("Cannot execute `%s': %s", Cmd->Name, strerror (errno));
    } else if (Status != 0) {
	/* Called program had an error */
	exit (Status);
    }
}



static void Link (void)
/* Link the resulting executable */
{
    unsigned I;

    /* If we have a linker config file given, set the linker config file.
     * Otherwise set the target system.
     */
    if (LinkerConfig) {
       	CmdAddArg (&LD65, "-C");
	CmdAddArg (&LD65, LinkerConfig);
    } else {
	if (Target == TGT_UNKNOWN) {
	    /* Use c64 instead */
	    Target = TGT_C64;
	}
	SetTargetFiles ();
	CmdSetTarget (&LD65, Target);
    }

    /* Since linking is always the final step, if we have an output file name
     * given, set it here. If we don't have an explicit output name given,
     * try to build one from the name of the first input file.
     */
    if (OutputName) {

	CmdAddArg (&LD65, "-o");
	CmdAddArg (&LD65, OutputName);

    } else if (FirstInput && FindExt (FirstInput)) {  /* Only if ext present! */

	char* Output = ForceExt (FirstInput, "");
	CmdAddArg (&LD65, "-o");
	CmdAddArg (&LD65, Output);
	Xfree (Output);

    }

    /* If we have a startup file, add its name as a parameter */
    if (TargetCRT0) {
	CmdAddArg (&LD65, TargetCRT0);
    }

    /* Add all object files as parameters */
    for (I = 0; I < LD65.FileCount; ++I) {
	CmdAddArg (&LD65, LD65.Files [I]);
    }

    /* Add the system runtime library */
    if (TargetLib) {
	CmdAddArg (&LD65, TargetLib);
    }

    /* Terminate the argument list with a NULL pointer */
    CmdAddArg (&LD65, 0);

    /* Call the linker */
    ExecProgram (&LD65);
}



static void Assemble (const char* File)
/* Assemble the given file */
{
    /* Remember the current assembler argument count */
    unsigned ArgCount = CA65.ArgCount;

    /* If we won't link, this is the final step. In this case, set the
     * output name.
     */
    if (DontLink && OutputName) {
	CmdSetOutput (&CA65, OutputName);
    } else {
	/* The object file name will be the name of the source file
	 * with .s replaced by ".o". Add this file to the list of
	 * linker files.
	 */
	char* ObjName = ForceExt (File, ".o");
	CmdAddFile (&LD65, ObjName);
	Xfree (ObjName);
    }

    /* Add the file as argument for the assembler */
    CmdAddArg (&CA65, File);

    /* Add a NULL pointer to terminate the argument list */
    CmdAddArg (&CA65, 0);

    /* Run the assembler */
    ExecProgram (&CA65);

    /* Remove the excess arguments */
    CmdDelArgs (&CA65, ArgCount);
}



static void Compile (const char* File)
/* Compile the given file */
{
    char* AsmName = 0;

    /* Remember the current assembler argument count */
    unsigned ArgCount = CC65.ArgCount;

    /* Set the target system */
    CmdSetTarget (&CC65, Target);

    /* If we won't link, this is the final step. In this case, set the
     * output name.
     */
    if (DontAssemble && OutputName) {
	CmdSetOutput (&CC65, OutputName);
    } else {
	/* The assembler file name will be the name of the source file
	 * with .c replaced by ".s".
	 */
	AsmName = ForceExt (File, ".s");
    }

    /* Add the file as argument for the compiler */
    CmdAddArg (&CC65, File);

    /* Add a NULL pointer to terminate the argument list */
    CmdAddArg (&CC65, 0);

    /* Run the compiler */
    ExecProgram (&CC65);

    /* Remove the excess arguments */
    CmdDelArgs (&CC65, ArgCount);

    /* If this is not the final step, assemble the generated file, then
     * remove it
     */
    if (!DontAssemble) {
	Assemble (AsmName);
	if (remove (AsmName) < 0) {
	    Warning ("Cannot remove temporary file `%s': %s",
		     AsmName, strerror (errno));
	}
	Xfree (AsmName);
    }
}



/*****************************************************************************/
/*			       	     Code				     */
/*****************************************************************************/



static void Usage (void)
/* Print usage information and exit */
{
    fprintf (stderr,
	     "Usage: %s [options] file\n"
       	     "Short options:\n"
       	     "  -A\t\t\tStrict ANSI mode\n"
       	     "  -C name\t\tUse linker config file\n"
       	     "  -Cl\t\t\tMake local variables static\n"
       	     "  -D sym[=defn]\t\tDefine a preprocessor symbol\n"
       	     "  -I dir\t\tSet a compiler include directory path\n"
       	     "  -Ln name\t\tCreate a VICE label file\n"
       	     "  -O\t\t\tOptimize code\n"
       	     "  -Oi\t\t\tOptimize code, inline functions\n"
       	     "  -Or\t\t\tOptimize code, honour the register keyword\n"
       	     "  -Os\t\t\tOptimize code, inline known C funtions\n"
       	     "  -S\t\t\tCompile but don't assemble and link\n"
       	     "  -V\t\t\tPrint the version number\n"
       	     "  -W\t\t\tSuppress warnings\n"
       	     "  -c\t\t\tCompiler and assemble but don't link\n"
       	     "  -d\t\t\tDebug mode\n"
       	     "  -g\t\t\tAdd debug info\n"
       	     "  -h\t\t\tHelp (this text)\n"
       	     "  -m name\t\tCreate a map file\n"
       	     "  -o name\t\tName the output file\n"
       	     "  -t sys\t\tSet the target system\n"
       	     "  -v\t\t\tVerbose mode\n"
       	     "  -vm\t\t\tVerbose map file\n"
	     "\n"
	     "Long options:\n"
       	     "  --ansi\t\tStrict ANSI mode\n"
	     "  --asm-include-dir dir\tSet an assembler include directory\n"
       	     "  --debug\t\tDebug mode\n"
       	     "  --debug-info\t\tAdd debug info\n"
       	     "  --help\t\tHelp (this text)\n"
       	     "  --include-dir dir\tSet a compiler include directory path\n"
       	     "  --version\t\tPrint the version number\n"
       	     "  --target sys\t\tSet the target system\n"
       	     "  --verbose\t\tVerbose mode\n",
	     ProgName);
}



static const char* GetArg (int* ArgNum, char* argv [], unsigned Len)
/* Get an option argument */
{
    const char* Arg = argv [*ArgNum];
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



static void UnknownOption (const char* Arg)
/* Print an error about a wrong argument */
{
    Error ("Unknown option: `%s', use -h for help", Arg);
}



static void NeedArg (const char* Arg)
/* Print an error about a missing option argument and exit. */
{
    Error ("Option requires an argument: %s\n", Arg);
}



static void OptAnsi (const char* Opt)
/* Strict ANSI mode (compiler) */
{
    CmdAddArg (&CC65, "-A");
}



static void OptAsmIncludeDir (const char* Opt, const char* Arg)
/* Include directory (assembler) */
{
    if (Arg == 0) {
	NeedArg (Opt);
    }
    CmdAddArg (&CA65, "-I");
    CmdAddArg (&CA65, Arg);
}



static void OptDebug (const char* Opt)
/* Debug mode (compiler) */
{
    CmdAddArg (&CC65, "-d");
}



static void OptDebugInfo (const char* Opt)
/* Debug Info - add to compiler and assembler */
{
    CmdAddArg (&CC65, "-g");
    CmdAddArg (&CA65, "-g");
}



static void OptHelp (const char* Opt)
/* Print help - cl65 */
{
    Usage ();
    exit (EXIT_SUCCESS);
}



static void OptIncludeDir (const char* Opt, const char* Arg)
/* Include directory (compiler) */
{
    if (Arg == 0) {
	NeedArg (Opt);
    }
    CmdAddArg (&CC65, "-I");
    CmdAddArg (&CC65, Arg);
}



static void OptTarget (const char* Opt, const char* Arg)
/* Set the target system */
{
    if (Arg == 0) {
	NeedArg (Opt);
    }
    SetTargetByName (Arg);
}



static void OptVerbose (const char* Opt)
/* Verbose mode (compiler, assembler, linker) */
{
    CmdAddArg (&CC65, "-v");
    CmdAddArg (&CA65, "-v");
    CmdAddArg (&LD65, "-v");
}



static void OptVersion (const char* Opt)
/* Print version number */
{
    fprintf (stderr,
 	     "cl65 V%u.%u.%u - (C) Copyright 1998-2000 Ullrich von Bassewitz\n",
 	     VER_MAJOR, VER_MINOR, VER_PATCH);
}



static void LongOption (int* ArgNum, char* argv [])
/* Handle a long command line option */
{
    const char* Opt = argv [*ArgNum];
    const char* Arg = argv [*ArgNum+1];

    if (strcmp (Opt, "--ansi") == 0) {
	OptAnsi (Opt);
    } else if (strcmp (Opt, "--asm-include-dir") == 0) {
    	OptAsmIncludeDir (Opt, Arg);
    	++(*ArgNum);
    } else if (strcmp (Opt, "--debug") == 0) {
       	OptDebug (Opt);
    } else if (strcmp (Opt, "--debug-info") == 0) {
       	OptDebugInfo (Opt);
    } else if (strcmp (Opt, "--help") == 0) {
       	OptHelp (Opt);
    } else if (strcmp (Opt, "--include-dir") == 0) {
    	OptIncludeDir (Opt, Arg);
    	++(*ArgNum);
    } else if (strcmp (Opt, "--target") == 0) {
    	OptTarget (Opt, Arg);
    	++(*ArgNum);
    } else if (strcmp (Opt, "--verbose") == 0) {
       	OptVerbose (Opt);
    } else if (strcmp (Opt, "--version") == 0) {
       	OptVersion (Opt);
    } else {
        UnknownOption (Opt);
    }
}



int main (int argc, char* argv [])
/* Utility main program */
{
    int I;

    /* Initialize the command descriptors */
    CmdInit (&CC65, "cc65");
    CmdInit (&CA65, "ca65");
    CmdInit (&LD65, "ld65");

    /* Check the parameters */
    I = 1;
    while (I < argc) {

	/* Get the argument */
	const char* Arg = argv [I];

	/* Check for an option */
	if (Arg [0] == '-') {

	    switch (Arg [1]) {

		case '-':
		    LongOption (&I, argv);
		    break;

		case 'A':
		    /* Strict ANSI mode (compiler) */
		    OptAnsi (Arg);
		    break;

		case 'C':
	   	    if (Arg[2] == 'l' && Arg[3] == '\0') {
			/* Make local variables static */
			CmdAddArg (&CC65, "-Cl");
		    } else {
		     	/* Specify linker config file */
		     	LinkerConfig = GetArg (&I, argv, 2);
		    }
	     	    break;

		case 'D':
		    /* Define a preprocessor symbol (compiler) */
		    CmdAddArg (&CC65, "-D");
		    CmdAddArg (&CC65, GetArg (&I, argv, 2));
		    break;

		case 'I':
		    /* Include directory (compiler) */
		    OptIncludeDir (Arg, GetArg (&I, argv, 2));
		    break;

	   	case 'L':
		    if (Arg[2] == 'n') {
		      	/* VICE label file (linker) */
		      	CmdAddArg (&LD65, "-Ln");
		      	CmdAddArg (&LD65, GetArg (&I, argv, 3));
		    } else {
	    	      	UnknownOption (Arg);
		    }
	    	    break;

	    	case 'O':
	    	    /* Optimize code (compiler, also covers -Oi and others) */
	    	    CmdAddArg (&CC65, Arg);
	    	    break;

	    	case 'S':
	    	    /* Dont assemble and link the created files */
		    DontLink = DontAssemble = 1;
		    break;

	   	case 'T':
		    /* Include source as comment (compiler) */
		    CmdAddArg (&CC65, "-T");
		    break;

		case 'V':
		    /* Print version number */
	       	    OptVersion (Arg);
	   	    break;

	   	case 'W':
	   	    /* Suppress warnings - compiler and assembler */
	   	    CmdAddArg (&CC65, "-W");
	     	    CmdAddArg (&CA65, "-W");
	   	    CmdAddArg (&CA65, "0");
	   	    break;

	   	case 'c':
	   	    /* Don't link the resulting files */
	   	    DontLink = 1;
	   	    break;

		case 'd':
		    /* Debug mode (compiler) */
		    OptDebug (Arg);
		    break;

		case 'g':
		    /* Debugging - add to compiler and assembler */
		    OptDebugInfo (Arg);
		    break;

		case 'h':
		case '?':
		    /* Print help - cl65 */
	     	    OptHelp (Arg);
		    break;

	   	case 'm':
		    /* Create a map file (linker) */
		    CmdAddArg (&LD65, "-m");
		    CmdAddArg (&LD65, GetArg (&I, argv, 2));
		    break;

		case 'o':
		    /* Name the output file */
		    OutputName = GetArg (&I, argv, 2);
		    break;

		case 't':
		    /* Set target system - compiler and linker */
		    OptTarget (Arg, GetArg (&I, argv, 2));
		    break;

		case 'v':
		    if (Arg [2] == 'm') {
	     	     	/* Verbose map file (linker) */
		     	CmdAddArg (&LD65, "-vm");
	   	    } else {
	   	     	/* Verbose mode (compiler, assembler, linker) */
			OptVerbose (Arg);
	   	    }
	   	    break;

	     	default:
	     	    UnknownOption (Arg);
	    }
	} else {

	    /* Remember the first file name */
	    if (FirstInput == 0) {
	     	FirstInput = Arg;
	    }

	    /* Determine the file type by the extension */
	    switch (GetFileType (Arg)) {

	     	case FILETYPE_C:
	     	    /* Compile the file */
	     	    Compile (Arg);
	     	    break;

	     	case FILETYPE_ASM:
	     	    /* Assemble the file */
	     	    if (!DontAssemble) {
	     		Assemble (Arg);
	     	    }
	     	    break;

	     	case FILETYPE_OBJ:
	     	case FILETYPE_LIB:
	     	    /* Add to the linker files */
	     	    CmdAddFile (&LD65, Arg);
	     	    break;

	     	default:
	     	    Error ("Don't know what to do with `%s'", Arg);

	    }

	}

	/* Next argument */
	++I;
    }

    /* Check if we had any input files */
    if (FirstInput == 0) {
	Warning ("No input files");
    }

    /* Link the given files if requested and if we have any */
    if (DontLink == 0 && LD65.FileCount > 0) {
	Link ();
    }

    /* Return an apropriate exit code */
    return EXIT_SUCCESS;
}




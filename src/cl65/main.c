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
#if defined(__WATCOMC__) || defined(_MSC_VER)
#  include <process.h>		/* DOS, OS/2 and Windows */
#else
#  include "spawn.h"		/* All others */
#endif

/* common */
#include "cmdline.h"
#include "fname.h"
#include "target.h"
#include "version.h"
#include "xmalloc.h"

/* cl65 */
#include "global.h"
#include "error.h"



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
static CmdDesc GRC  = { 0, 0, 0, 0, 0, 0, 0 };

/* File types */
enum {
    FILETYPE_UNKNOWN,
    FILETYPE_C,
    FILETYPE_ASM,
    FILETYPE_OBJ,
    FILETYPE_LIB,
    FILETYPE_GR  		/* GEOS resource file */
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

/* Name of the crt0 object file and the runtime library */
static char* TargetCRT0 = 0;
static char* TargetLib	= 0;



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
	{   ".a65",	FILETYPE_ASM	},
	{   ".o",	FILETYPE_OBJ	},
	{   ".obj",	FILETYPE_OBJ	},
	{   ".a",	FILETYPE_LIB	},
	{   ".lib",	FILETYPE_LIB	},
	{   ".grc",	FILETYPE_GR	},
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
	char**	 NewArgs = xmalloc (NewMax * sizeof (char*));
	memcpy (NewArgs, Cmd->Args, Cmd->ArgMax * sizeof (char*));
	xfree (Cmd->Args);
	Cmd->Args   = NewArgs;
	Cmd->ArgMax = NewMax;
    }

    /* Add a copy of the new argument, allow a NULL pointer */
    if (Arg) {
	Cmd->Args [Cmd->ArgCount++] = xstrdup (Arg);
    } else {
	Cmd->Args [Cmd->ArgCount++] = 0;
    }
}



static void CmdDelArgs (CmdDesc* Cmd, unsigned LastValid)
/* Remove all arguments with an index greater than LastValid */
{
    while (Cmd->ArgCount > LastValid) {
	Cmd->ArgCount--;
	xfree (Cmd->Args [Cmd->ArgCount]);
	Cmd->Args [Cmd->ArgCount] = 0;
    }
}



static void CmdAddFile (CmdDesc* Cmd, const char* File)
/* Add a new file to the command */
{
    /* Expand the file vector if needed */
    if (Cmd->FileCount == Cmd->FileMax) {
	unsigned NewMax   = Cmd->FileMax + 10;
	char**	 NewFiles = xmalloc (NewMax * sizeof (char*));
	memcpy (NewFiles, Cmd->Files, Cmd->FileMax * sizeof (char*));
	xfree (Cmd->Files);
	Cmd->Files   = NewFiles;
	Cmd->FileMax = NewMax;
    }

    /* If the file name is not NULL (which is legal and is used to terminate
     * the file list), check if the file name does already exist in the file
     * list and print a warning if so. Regardless of the search result, add
     * the file.
     */
    if (File) {
	unsigned I;
	for (I = 0; I < Cmd->FileCount; ++I) {
	    if (strcmp (Cmd->Files[I], File) == 0) {
	     	/* Duplicate file */
		Warning ("Duplicate file in argument list: `%s'", File);
		/* No need to search further */
	     	break;
	    }
	}

	/* Add the file */
	Cmd->Files [Cmd->FileCount++] = xstrdup (File);
    } else {
	/* Add a NULL pointer */
	Cmd->Files [Cmd->FileCount++] = 0;
    }
}



static void CmdInit (CmdDesc* Cmd, const char* Path)
/* Initialize the command using the given path to the executable */
{
    /* Remember the command */
    Cmd->Name = xstrdup (Path);

    /* Use the command name as first argument */
    CmdAddArg (Cmd, Path);
}



static void CmdSetOutput (CmdDesc* Cmd, const char* File)
/* Set the output file in a command desc */
{
    CmdAddArg (Cmd, "-o");
    CmdAddArg (Cmd, File);
}



static void CmdSetTarget (CmdDesc* Cmd, target_t Target)
/* Set the output file in a command desc */
{
    CmdAddArg (Cmd, "-t");
    CmdAddArg (Cmd, TargetNames[Target]);
}



/*****************************************************************************/
/*	   			Target handling				     */
/*****************************************************************************/



static void SetTargetFiles (void)
/* Set the target system files */
{
    /* Determine the names of the default startup and library file */
    if (Target != TGT_NONE) {

 	/* Get a pointer to the system name and its length */
 	const char* TargetName = TargetNames [Target];
 	unsigned    TargetNameLen = strlen (TargetName);

 	/* Set the startup file */
 	TargetCRT0 = xmalloc (TargetNameLen + 2 + 1);
 	strcpy (TargetCRT0, TargetName);
 	strcat (TargetCRT0, ".o");

 	/* Set the library file */
 	TargetLib = xmalloc (TargetNameLen + 4 + 1);
 	strcpy (TargetLib, TargetName);
 	strcat (TargetLib, ".lib");

    }
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

    /* If we have a linker config file given, add it to the command line. 
     * Otherwise pass the target to the linker if we have one.
     */
    if (LinkerConfig) {
       	CmdAddArg (&LD65, "-C");
	CmdAddArg (&LD65, LinkerConfig);
    } else if (Target != TGT_NONE) {
	CmdSetTarget (&LD65, Target);
    }

    /* Determine which target libraries are needed */
    SetTargetFiles ();

    /* Since linking is always the final step, if we have an output file name
     * given, set it here. If we don't have an explicit output name given,
     * try to build one from the name of the first input file.
     */
    if (OutputName) {

	CmdAddArg (&LD65, "-o");
	CmdAddArg (&LD65, OutputName);

    } else if (FirstInput && FindExt (FirstInput)) {  /* Only if ext present! */

	char* Output = MakeFilename (FirstInput, "");
	CmdAddArg (&LD65, "-o");
	CmdAddArg (&LD65, Output);
	xfree (Output);

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

    /* Set the target system */
    CmdSetTarget (&CA65, Target);

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
	char* ObjName = MakeFilename (File, ".o");
	CmdAddFile (&LD65, ObjName);
	xfree (ObjName);
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
	AsmName = MakeFilename (File, ".s");
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
	xfree (AsmName);
    }
}



static void CompileRes (const char* File)
/* Compile the given geos resource file */
{
    char* AsmName = 0;

    /* Remember the current assembler argument count */
    unsigned ArgCount = GRC.ArgCount;

    /* The assembler file name will be the name of the source file
     * with .grc replaced by ".s".
     */
    AsmName = MakeFilename (File, ".s");

    /* Add the file as argument for the resource compiler */
    CmdAddArg (&GRC, File);

    /* Add a NULL pointer to terminate the argument list */
    CmdAddArg (&GRC, 0);

    /* Run the compiler */
    ExecProgram (&GRC);

    /* Remove the excess arguments */
    CmdDelArgs (&GRC, ArgCount);

    /* If this is not the final step, assemble the generated file, then
     * remove it
     */
    if (!DontAssemble) {
	Assemble (AsmName);
	if (remove (AsmName) < 0) {
	    Warning ("Cannot remove temporary file `%s': %s",
		     AsmName, strerror (errno));
	}
    }

    /* Free the assembler file name which was allocated from the heap */
    xfree (AsmName);
}



/*****************************************************************************/
/*		    	       	     Code				     */
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
       	     "  -l\t\t\tCreate an assembler listing\n"
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
	     "  --feature name\tSet an emulation feature\n"
       	     "  --help\t\tHelp (this text)\n"
       	     "  --include-dir dir\tSet a compiler include directory path\n"
       	     "  --listing\t\tCreate an assembler listing\n"
	     "  --mapfile name\tCreate a map file\n"
       	     "  --start-addr addr\tSet the default start address\n"
       	     "  --target sys\t\tSet the target system\n"
       	     "  --version\t\tPrint the version number\n"
       	     "  --verbose\t\tVerbose mode\n",
    	     ProgName);
}



static void OptAnsi (const char* Opt, const char* Arg)
/* Strict ANSI mode (compiler) */
{
    CmdAddArg (&CC65, "-A");
}



static void OptAsmIncludeDir (const char* Opt, const char* Arg)
/* Include directory (assembler) */
{
    CmdAddArg (&CA65, "-I");
    CmdAddArg (&CA65, Arg);
}



static void OptDebug (const char* Opt, const char* Arg)
/* Debug mode (compiler) */
{
    CmdAddArg (&CC65, "-d");
}



static void OptDebugInfo (const char* Opt, const char* Arg)
/* Debug Info - add to compiler and assembler */
{
    CmdAddArg (&CC65, "-g");
    CmdAddArg (&CA65, "-g");
}



static void OptFeature (const char* Opt, const char* Arg)
/* Emulation features for the assembler */
{
    CmdAddArg (&CA65, "--feature");
    CmdAddArg (&CA65, Arg);
}



static void OptHelp (const char* Opt, const char* Arg)
/* Print help - cl65 */
{
    Usage ();
    exit (EXIT_SUCCESS);
}



static void OptIncludeDir (const char* Opt, const char* Arg)
/* Include directory (compiler) */
{
    CmdAddArg (&CC65, "-I");
    CmdAddArg (&CC65, Arg);
}



static void OptListing (const char* Opt, const char* Arg)
/* Create an assembler listing */
{
    CmdAddArg (&CA65, "-l");
}



static void OptMapFile (const char* Opt, const char* Arg)
/* Create a map file */
{
    /* Create a map file (linker) */
    CmdAddArg (&LD65, "-m");
    CmdAddArg (&LD65, Arg);
}



static void OptStartAddr (const char* Opt, const char* Arg)
/* Set the default start address */
{
    CmdAddArg (&LD65, "-S");
    CmdAddArg (&LD65, Arg);
}



static void OptTarget (const char* Opt, const char* Arg)
/* Set the target system */
{
    Target = FindTarget (Arg);
    if (Target == TGT_UNKNOWN) {
	Error ("No such target system: `%s'", Arg);
    }
}



static void OptVerbose (const char* Opt, const char* Arg)
/* Verbose mode (compiler, assembler, linker) */
{
    CmdAddArg (&CC65, "-v");
    CmdAddArg (&CA65, "-v");
    CmdAddArg (&LD65, "-v");
}



static void OptVersion (const char* Opt, const char* Arg)
/* Print version number */
{
    fprintf (stderr,
 	     "cl65 V%u.%u.%u - (C) Copyright 1998-2000 Ullrich von Bassewitz\n",
 	     VER_MAJOR, VER_MINOR, VER_PATCH);
}



int main (int argc, char* argv [])
/* Utility main program */
{
    /* Program long options */
    static const LongOpt OptTab[] = {
	{ "--ansi",		0,	OptAnsi			},
	{ "--asm-include-dir",	1,	OptAsmIncludeDir	},
	{ "--debug",		0,	OptDebug		},
	{ "--debug-info",	0,	OptDebugInfo		},
	{ "--feature",	  	1,	OptFeature		},
	{ "--help",	  	0,	OptHelp			},
	{ "--include-dir",	1,	OptIncludeDir		},
	{ "--listing",		0,	OptListing		},
	{ "--mapfile",	  	1,	OptMapFile		},
	{ "--start-addr", 	1,	OptStartAddr		},
	{ "--target",	  	1,	OptTarget		},
	{ "--verbose",	  	0,	OptVerbose		},
	{ "--version",	  	0,	OptVersion		},
    };

    int I;

    /* Initialize the cmdline module */
    InitCmdLine (argc, argv, "cl65");

    /* Initialize the command descriptors */
    CmdInit (&CC65, "cc65");
    CmdInit (&CA65, "ca65");
    CmdInit (&LD65, "ld65");
    CmdInit (&GRC,  "grc");

    /* Our default target is the C64 instead of "none" */
    Target = TGT_C64;

    /* Check the parameters */
    I = 1;
    while (I < argc) {

	/* Get the argument */
	const char* Arg = argv [I];

	/* Check for an option */
	if (Arg [0] == '-') {

	    switch (Arg [1]) {

		case '-':
		    LongOption (&I, OptTab, sizeof(OptTab)/sizeof(OptTab[0]));
		    break;

		case 'A':
		    /* Strict ANSI mode (compiler) */
		    OptAnsi (Arg, 0);
		    break;

		case 'C':
	   	    if (Arg[2] == 'l' && Arg[3] == '\0') {
			/* Make local variables static */
			CmdAddArg (&CC65, "-Cl");
		    } else {
		     	/* Specify linker config file */
		     	LinkerConfig = GetArg (&I, 2);
		    }
	     	    break;

		case 'D':
		    /* Define a preprocessor symbol (compiler) */
		    CmdAddArg (&CC65, "-D");
		    CmdAddArg (&CC65, GetArg (&I, 2));
		    break;

		case 'I':
		    /* Include directory (compiler) */
		    OptIncludeDir (Arg, GetArg (&I, 2));
		    break;

	   	case 'L':
		    if (Arg[2] == 'n') {
		      	/* VICE label file (linker) */
		      	CmdAddArg (&LD65, "-Ln");
		      	CmdAddArg (&LD65, GetArg (&I, 3));
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
	       	    OptVersion (Arg, 0);
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
		    OptDebug (Arg, 0);
		    break;

		case 'g':
		    /* Debugging - add to compiler and assembler */
		    OptDebugInfo (Arg, 0);
		    break;

		case 'h':
		case '?':
		    /* Print help - cl65 */
	     	    OptHelp (Arg, 0);
		    break;

	   	case 'l':
		    /* Create an assembler listing */
		    OptListing (Arg, 0);
		    break;

	   	case 'm':
		    /* Create a map file (linker) */
		    OptMapFile (Arg, GetArg (&I, 2));
		    break;

		case 'o':
		    /* Name the output file */
		    OutputName = GetArg (&I, 2);
		    break;

		case 't':
		    /* Set target system - compiler, assembler and linker */
		    OptTarget (Arg, GetArg (&I, 2));
		    break;

		case 'v':
		    if (Arg [2] == 'm') {
	     	     	/* Verbose map file (linker) */
		     	CmdAddArg (&LD65, "-vm");
	   	    } else {
	   	     	/* Verbose mode (compiler, assembler, linker) */
			OptVerbose (Arg, 0);
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

		case FILETYPE_GR:
		    /* Add to the resource compiler files */
		    CompileRes (Arg);
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





/*****************************************************************************/
/*                                                                           */
/*				   config.c				     */
/*                                                                           */
/*		 Target configuration file for the ld65 linker		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2010, Ullrich von Bassewitz                                      */
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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* common */
#include "bitops.h"
#include "check.h"
#include "print.h"
#include "xmalloc.h"
#include "xsprintf.h"

/* ld65 */
#include "bin.h"
#include "binfmt.h"
#include "cfgexpr.h"
#include "condes.h"
#include "config.h"
#include "error.h"
#include "exports.h"
#include "global.h"
#include "o65.h"
#include "objdata.h"
#include "scanner.h"
#include "spool.h"



/*****************************************************************************/
/*     	      	    		     Data				     */
/*****************************************************************************/



/* Remember which sections we had encountered */
static enum {
    SE_NONE     = 0x0000,
    SE_MEMORY   = 0x0001,
    SE_SEGMENTS = 0x0002,
    SE_FEATURES = 0x0004,
    SE_FILES    = 0x0008,
    SE_FORMATS  = 0x0010,
    SE_SYMBOLS  = 0x0020
} SectionsEncountered = SE_NONE;



/* File list */
static Collection       FileList = STATIC_COLLECTION_INITIALIZER;

/* Memory list */
static Collection       MemoryList = STATIC_COLLECTION_INITIALIZER;

/* Memory attributes */
#define MA_START       	0x0001
#define MA_SIZE        	0x0002
#define MA_TYPE        	0x0004
#define MA_FILE        	0x0008
#define MA_DEFINE      	0x0010
#define MA_FILL	       	0x0020
#define MA_FILLVAL     	0x0040



/* Segment list */
SegDesc*	       	SegDescList;	/* Single linked list */
unsigned	       	SegDescCount;	/* Number of entries in list */

/* Segment attributes */
#define SA_TYPE	       	0x0001
#define SA_LOAD		0x0002
#define SA_RUN		0x0004
#define SA_ALIGN	0x0008
#define SA_ALIGN_LOAD   0x0010
#define SA_DEFINE	0x0020
#define SA_OFFSET	0x0040
#define SA_START	0x0080
#define SA_OPTIONAL     0x0100



/* Descriptor holding information about the binary formats */
static BinDesc*	BinFmtDesc	= 0;
static O65Desc* O65FmtDesc	= 0;



/*****************************************************************************/
/*				   Forwards				     */
/*****************************************************************************/



static File* NewFile (unsigned Name);
/* Create a new file descriptor and insert it into the list */



/*****************************************************************************/
/*				List management				     */
/*****************************************************************************/



static File* FindFile (unsigned Name)
/* Find a file with a given name. */
{
    unsigned I;
    for (I = 0; I < CollCount (&FileList); ++I) {
        File* F = CollAtUnchecked (&FileList, I);
       	if (F->Name == Name) {
       	    return F;
       	}
    }
    return 0;
}



static File* GetFile (unsigned Name)
/* Get a file entry with the given name. Create a new one if needed. */
{
    File* F = FindFile (Name);
    if (F == 0) {
	/* Create a new one */
	F = NewFile (Name);
    }
    return F;
}



static void FileInsert (File* F, Memory* M)
/* Insert the memory area into the files list */
{
    M->F = F;
    CollAppend (&F->MemList, M);
}



static Memory* CfgFindMemory (unsigned Name)
/* Find the memory are with the given name. Return NULL if not found */
{
    unsigned I;
    for (I = 0; I < CollCount (&MemoryList); ++I) {
        Memory* M = CollAt (&MemoryList, I);
       	if (M->Name == Name) {
       	    return M;
       	}
    }
    return 0;
}



static Memory* CfgGetMemory (unsigned Name)
/* Find the memory are with the given name. Print an error on an invalid name */
{
    Memory* M = CfgFindMemory (Name);
    if (M == 0) {
 	CfgError ("Invalid memory area `%s'", GetString (Name));
    }
    return M;
}



static SegDesc* CfgFindSegDesc (unsigned Name)
/* Find the segment descriptor with the given name, return NULL if not found. */
{
    SegDesc* S = SegDescList;
    while (S) {
       	if (S->Name == Name) {
    	    /* Found */
    	    return S;
       	}
     	S = S->Next;
    }

    /* Not found */
    return 0;
}



static void SegDescInsert (SegDesc* S)
/* Insert a segment descriptor into the list of segment descriptors */
{
    /* Insert the struct into the list */
    S->Next = SegDescList;
    SegDescList = S;
    ++SegDescCount;
}



static void MemoryInsert (Memory* M, SegDesc* S)
/* Insert the segment descriptor into the memory area list */
{
    /* Create a new node for the entry */
    MemListNode* N = xmalloc (sizeof (MemListNode));
    N->Seg  = S;
    N->Next = 0;

    if (M->SegLast == 0) {
       	/* First entry */
       	M->SegList = N;
    } else {
 	M->SegLast->Next = N;
    }
    M->SegLast = N;
}



/*****************************************************************************/
/*			   Constructors/Destructors			     */
/*****************************************************************************/



static File* NewFile (unsigned Name)
/* Create a new file descriptor and insert it into the list */
{
    /* Allocate memory */
    File* F = xmalloc (sizeof (File));

    /* Initialize the fields */
    F->Name    = Name;
    F->Flags   = 0;
    F->Format  = BINFMT_DEFAULT;
    InitCollection (&F->MemList);

    /* Insert the struct into the list */
    CollAppend (&FileList, F);

    /* ...and return it */
    return F;
}



static Memory* NewMemory (unsigned Name)
/* Create a new memory section and insert it into the list */
{
    /* Check for duplicate names */
    Memory* M =	CfgFindMemory (Name);
    if (M) {
	CfgError ("Memory area `%s' defined twice", GetString (Name));
    }

    /* Allocate memory */
    M = xmalloc (sizeof (Memory));

    /* Initialize the fields */
    M->Name        = Name;
    M->Attr        = 0;
    M->Flags       = 0;
    M->Start       = 0;
    M->Size        = 0;
    M->FillLevel   = 0;
    M->FillVal     = 0;
    M->Relocatable = 0;
    M->SegList     = 0;
    M->SegLast     = 0;
    M->F           = 0;

    /* Insert the struct into the list */
    CollAppend (&MemoryList, M);

    /* ...and return it */
    return M;
}



static SegDesc* NewSegDesc (unsigned Name)
/* Create a segment descriptor */
{
    Segment* Seg;

    /* Check for duplicate names */
    SegDesc* S = CfgFindSegDesc (Name);
    if (S) {
	CfgError ("Segment `%s' defined twice", GetString (Name));
    }

    /* Search for the actual segment in the input files. The function may
     * return NULL (no such segment), this is checked later.
     */
    Seg = SegFind (Name);

    /* Allocate memory */
    S = xmalloc (sizeof (SegDesc));

    /* Initialize the fields */
    S->Name    = Name;
    S->Next    = 0;
    S->Seg     = Seg;
    S->Attr    = 0;
    S->Flags   = 0;
    S->Align   = 0;

    /* ...and return it */
    return S;
}



static void FreeSegDesc (SegDesc* S)
/* Free a segment descriptor */
{
    xfree (S);
}



/*****************************************************************************/
/*     	       	       	      	     Code     				     */
/*****************************************************************************/



static void FlagAttr (unsigned* Flags, unsigned Mask, const char* Name)
/* Check if the item is already defined. Print an error if so. If not, set
 * the marker that we have a definition now.
 */
{
    if (*Flags & Mask) {
    	CfgError ("%s is already defined", Name);
    }
    *Flags |= Mask;
}



static void AttrCheck (unsigned Attr, unsigned Mask, const char* Name)
/* Check that a mandatory attribute was given */
{
    if ((Attr & Mask) == 0) {
	CfgError ("%s attribute is missing", Name);
    }
}



static void ParseMemory (void)
/* Parse a MEMORY section */
{
    static const IdentTok Attributes [] = {
       	{   "START",  	CFGTOK_START    },
	{   "SIZE", 	CFGTOK_SIZE     },
        {   "TYPE",     CFGTOK_TYPE     },
        {   "FILE",     CFGTOK_FILE     },
        {   "DEFINE",   CFGTOK_DEFINE   },
  	{   "FILL", 	CFGTOK_FILL     },
       	{   "FILLVAL", 	CFGTOK_FILLVAL  },
    };
    static const IdentTok Types [] = {
       	{   "RO",    	CFGTOK_RO       },
       	{   "RW",   	CFGTOK_RW       },
    };

    while (CfgTok == CFGTOK_IDENT) {

	/* Create a new entry on the heap */
       	Memory* M = NewMemory (GetStrBufId (&CfgSVal));

	/* Skip the name and the following colon */
	CfgNextTok ();
	CfgConsumeColon ();

       	/* Read the attributes */
	while (CfgTok == CFGTOK_IDENT) {

	    /* Map the identifier to a token */
	    cfgtok_t AttrTok;
	    CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	    AttrTok = CfgTok;

	    /* An optional assignment follows */
	    CfgNextTok ();
	    CfgOptionalAssign ();

	    /* Check which attribute was given */
	    switch (AttrTok) {

		case CFGTOK_START:
		    FlagAttr (&M->Attr, MA_START, "START");
                    M->Start = CfgIntExpr ();
		    break;

	      	case CFGTOK_SIZE:
	     	    FlagAttr (&M->Attr, MA_SIZE, "SIZE");
	      	    M->Size = CfgIntExpr ();
		    break;

		case CFGTOK_TYPE:
		    FlagAttr (&M->Attr, MA_TYPE, "TYPE");
      		    CfgSpecialToken (Types, ENTRY_COUNT (Types), "Type");
		    if (CfgTok == CFGTOK_RO) {
		    	M->Flags |= MF_RO;
		    }
                    CfgNextTok ();
		    break;

	        case CFGTOK_FILE:
		    FlagAttr (&M->Attr, MA_FILE, "FILE");
		    CfgAssureStr ();
       	       	    /* Get the file entry and insert the memory area */
	    	    FileInsert (GetFile (GetStrBufId (&CfgSVal)), M);
                    CfgNextTok ();
		    break;

	        case CFGTOK_DEFINE:
 		    FlagAttr (&M->Attr, MA_DEFINE, "DEFINE");
		    /* Map the token to a boolean */
		    CfgBoolToken ();
		    if (CfgTok == CFGTOK_TRUE) {
	  	    	M->Flags |= MF_DEFINE;
		    }
                    CfgNextTok ();
		    break;

	        case CFGTOK_FILL:
 		    FlagAttr (&M->Attr, MA_FILL, "FILL");
		    /* Map the token to a boolean */
		    CfgBoolToken ();
		    if (CfgTok == CFGTOK_TRUE) {
	  	    	M->Flags |= MF_FILL;
		    }
                    CfgNextTok ();
		    break;

	      	case CFGTOK_FILLVAL:
		    FlagAttr (&M->Attr, MA_FILLVAL, "FILLVAL");
	      	    M->FillVal = (unsigned char) CfgCheckedIntExpr (0, 0xFF);
		    break;

	     	default:
	       	    FAIL ("Unexpected attribute token");

	    }

	    /* Skip an optional comma */
	    CfgOptionalComma ();
	}

	/* Skip the semicolon */
	CfgConsumeSemi ();

	/* Check for mandatory parameters */
       	AttrCheck (M->Attr, MA_START, "START");
	AttrCheck (M->Attr, MA_SIZE, "SIZE");

	/* If we don't have a file name for output given, use the default
	 * file name.
	 */
	if ((M->Attr & MA_FILE) == 0) {
	    FileInsert (GetFile (GetStringId (OutputName)), M);
	}
    }

    /* Remember we had this section */
    SectionsEncountered |= SE_MEMORY;
}



static void ParseFiles (void)
/* Parse a FILES section */
{
    static const IdentTok Attributes [] = {
       	{   "FORMAT",  	CFGTOK_FORMAT   },
    };
    static const IdentTok Formats [] = {
       	{   "O65",     	CFGTOK_O65  	     },
       	{   "BIN",     	CFGTOK_BIN      },
       	{   "BINARY",   CFGTOK_BIN      },
    };


    /* The MEMORY section must preceed the FILES section */
    if ((SectionsEncountered & SE_MEMORY) == 0) {
        CfgError ("MEMORY must precede FILES");
    }

    /* Parse all files */
    while (CfgTok != CFGTOK_RCURLY) {

	File* F;

	/* We expect a string value here */
	CfgAssureStr ();

	/* Search for the file, it must exist */
       	F = FindFile (GetStrBufId (&CfgSVal));
	if (F == 0) {
       	    CfgError ("File `%s' not found in MEMORY section",
                      SB_GetConstBuf (&CfgSVal));
	}

	/* Skip the token and the following colon */
	CfgNextTok ();
	CfgConsumeColon ();

	/* Read the attributes */
	while (CfgTok == CFGTOK_IDENT) {

	    /* Map the identifier to a token */
	    cfgtok_t AttrTok;
	    CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	    AttrTok = CfgTok;

	    /* An optional assignment follows */
	    CfgNextTok ();
	    CfgOptionalAssign ();

	    /* Check which attribute was given */
	    switch (AttrTok) {

	    	case CFGTOK_FORMAT:
	    	    if (F->Format != BINFMT_DEFAULT) {
	    	  	/* We've set the format already! */
		  	Error ("Cannot set a file format twice");
		    }
		    /* Read the format token */
		    CfgSpecialToken (Formats, ENTRY_COUNT (Formats), "Format");
		    switch (CfgTok) {

			case CFGTOK_BIN:
			    F->Format = BINFMT_BINARY;
			    break;

			case CFGTOK_O65:
			    F->Format = BINFMT_O65;
			    break;

			default:
			    Error ("Unexpected format token");
		    }
      		    break;

	     	default:
	       	    FAIL ("Unexpected attribute token");

	    }

	    /* Skip the attribute value and an optional comma */
	    CfgNextTok ();
	    CfgOptionalComma ();
	}

	/* Skip the semicolon */
	CfgConsumeSemi ();

    }

    /* Remember we had this section */
    SectionsEncountered |= SE_FILES;
}



static void ParseSegments (void)
/* Parse a SEGMENTS section */
{
    static const IdentTok Attributes [] = {
        {   "ALIGN",            CFGTOK_ALIGN            },
        {   "ALIGN_LOAD",       CFGTOK_ALIGN_LOAD       },
        {   "DEFINE",           CFGTOK_DEFINE           },
       	{   "LOAD",    	        CFGTOK_LOAD             },
	{   "OFFSET",  	        CFGTOK_OFFSET           },
        {   "OPTIONAL",         CFGTOK_OPTIONAL         },
	{   "RUN",     	        CFGTOK_RUN              },
	{   "START",   	        CFGTOK_START            },
        {   "TYPE",             CFGTOK_TYPE             },
    };
    static const IdentTok Types [] = {
       	{   "RO",      	        CFGTOK_RO               },
       	{   "RW",      	        CFGTOK_RW               },
       	{   "BSS",     	        CFGTOK_BSS              },
	{   "ZP",      	        CFGTOK_ZP	        },
    };

    unsigned Count;
    long     Val;

    /* The MEMORY section must preceed the SEGMENTS section */
    if ((SectionsEncountered & SE_MEMORY) == 0) {
        CfgError ("MEMORY must precede SEGMENTS");
    }

    while (CfgTok == CFGTOK_IDENT) {

	SegDesc* S;

	/* Create a new entry on the heap */
       	S = NewSegDesc (GetStrBufId (&CfgSVal));

	/* Skip the name and the following colon */
	CfgNextTok ();
	CfgConsumeColon ();

       	/* Read the attributes */
	while (CfgTok == CFGTOK_IDENT) {

	    /* Map the identifier to a token */
	    cfgtok_t AttrTok;
	    CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	    AttrTok = CfgTok;

	    /* An optional assignment follows */
	    CfgNextTok ();
	    CfgOptionalAssign ();

	    /* Check which attribute was given */
	    switch (AttrTok) {

	        case CFGTOK_ALIGN:
	    	    FlagAttr (&S->Attr, SA_ALIGN, "ALIGN");
	    	    Val = CfgCheckedIntExpr (1, 0x10000);
	    	    S->Align = BitFind (Val);
	    	    if ((0x01L << S->Align) != Val) {
	    	     	CfgError ("Alignment must be a power of 2");
	    	    }
	    	    S->Flags |= SF_ALIGN;
	    	    break;

                case CFGTOK_ALIGN_LOAD:
	    	    FlagAttr (&S->Attr, SA_ALIGN_LOAD, "ALIGN_LOAD");
	    	    Val = CfgCheckedIntExpr (1, 0x10000);
       	       	    S->AlignLoad = BitFind (Val);
	    	    if ((0x01L << S->AlignLoad) != Val) {
	    	     	CfgError ("Alignment must be a power of 2");
	    	    }
	    	    S->Flags |= SF_ALIGN_LOAD;
	    	    break;

	        case CFGTOK_DEFINE:
 	    	    FlagAttr (&S->Attr, SA_DEFINE, "DEFINE");
	    	    /* Map the token to a boolean */
	    	    CfgBoolToken ();
	    	    if (CfgTok == CFGTOK_TRUE) {
	    	     	S->Flags |= SF_DEFINE;
	    	    }
                    CfgNextTok ();
	    	    break;

	    	case CFGTOK_LOAD:
	      	    FlagAttr (&S->Attr, SA_LOAD, "LOAD");
	    	    S->Load = CfgGetMemory (GetStrBufId (&CfgSVal));
                    CfgNextTok ();
	    	    break;

	        case CFGTOK_OFFSET:
	    	    FlagAttr (&S->Attr, SA_OFFSET, "OFFSET");
	    	    S->Addr   = CfgCheckedIntExpr (1, 0x1000000);
	    	    S->Flags |= SF_OFFSET;
	    	    break;

	        case CFGTOK_OPTIONAL:
	    	    FlagAttr (&S->Attr, SA_OPTIONAL, "OPTIONAL");
		    CfgBoolToken ();
		    if (CfgTok == CFGTOK_TRUE) {
	  	    	S->Flags |= SF_OPTIONAL;
		    }
                    CfgNextTok ();
	    	    break;

	    	case CFGTOK_RUN:
      	    	    FlagAttr (&S->Attr, SA_RUN, "RUN");
 	    	    S->Run = CfgGetMemory (GetStrBufId (&CfgSVal));
                    CfgNextTok ();
	    	    break;

	        case CFGTOK_START:
	    	    FlagAttr (&S->Attr, SA_START, "START");
	    	    S->Addr   = CfgCheckedIntExpr (1, 0x1000000);
	    	    S->Flags |= SF_START;
	    	    break;

	    	case CFGTOK_TYPE:
 	    	    FlagAttr (&S->Attr, SA_TYPE, "TYPE");
       	    	    CfgSpecialToken (Types, ENTRY_COUNT (Types), "Type");
	    	    switch (CfgTok) {
       	       	       	case CFGTOK_RO:	   S->Flags |= SF_RO;               break;
	    		case CFGTOK_RW:	   /* Default */		    break;
	    	     	case CFGTOK_BSS:   S->Flags |= SF_BSS;              break;
	    	     	case CFGTOK_ZP:	   S->Flags |= (SF_BSS | SF_ZP);    break;
	    	     	default:      	   Internal ("Unexpected token: %d", CfgTok);
	    	    }
                    CfgNextTok ();
	    	    break;

	    	default:
       	       	    FAIL ("Unexpected attribute token");

	    }

	    /* Skip an optional comma */
	    CfgOptionalComma ();
	}

	/* Check for mandatory parameters */
	AttrCheck (S->Attr, SA_LOAD, "LOAD");

 	/* Set defaults for stuff not given */
	if ((S->Attr & SA_RUN) == 0) {
	    S->Attr |= SA_RUN;
	    S->Run = S->Load;
	}

	/* If the segment is marked as BSS style, and if the segment exists
         * in any of the object file, check that there's no initialized data
         * in the segment.
	 */
	if ((S->Flags & SF_BSS) != 0 && S->Seg != 0 && !IsBSSType (S->Seg)) {
	    Warning ("%s(%u): Segment with type `bss' contains initialized data",
	    	     CfgGetName (), CfgErrorLine);
	}

        /* An attribute of ALIGN_LOAD doesn't make sense if there are no
         * separate run and load memory areas.
         */
        if ((S->Flags & SF_ALIGN_LOAD) != 0 && (S->Load == S->Run)) {
       	    Warning ("%s(%u): ALIGN_LOAD attribute specified, but no separate "
                     "LOAD and RUN memory areas assigned",
                     CfgGetName (), CfgErrorLine);
            /* Remove the flag */
            S->Flags &= ~SF_ALIGN_LOAD;
        }

        /* If the segment is marked as BSS style, it may not have separate
         * load and run memory areas, because it's is never written to disk.
         */
        if ((S->Flags & SF_BSS) != 0 && (S->Load != S->Run)) {
       	    Warning ("%s(%u): Segment with type `bss' has both LOAD and RUN "
                     "memory areas assigned", CfgGetName (), CfgErrorLine);
        }

      	/* Don't allow read/write data to be put into a readonly area */
      	if ((S->Flags & SF_RO) == 0) {
       	    if (S->Run->Flags & MF_RO) {
      	    	CfgError ("Cannot put r/w segment `%s' in r/o memory area `%s'",
      	    	     	  GetString (S->Name), GetString (S->Run->Name));
      	    }
      	}

      	/* Only one of ALIGN, START and OFFSET may be used */
       	Count = ((S->Flags & SF_ALIGN)  != 0) +
      	       	((S->Flags & SF_OFFSET) != 0) +
      	    	((S->Flags & SF_START)  != 0);
      	if (Count > 1) {
       	    CfgError ("Only one of ALIGN, START, OFFSET may be used");
      	}

      	/* If this segment does exist in any of the object files, insert the
      	 * descriptor into the list of segment descriptors. Otherwise print a
         * warning and discard it, because the segment pointer in the
         * descriptor is invalid.
      	 */
      	if (S->Seg != 0) {
	    /* Insert the descriptor into the list of all descriptors */
	    SegDescInsert (S);
      	    /* Insert the segment into the memory area list */
      	    MemoryInsert (S->Run, S);
      	    if (S->Load != S->Run) {
      	    	/* We have separate RUN and LOAD areas */
      	    	MemoryInsert (S->Load, S);
      	    }
      	} else {
            /* Print a warning if the segment is not optional */
            if ((S->Flags & SF_OPTIONAL) == 0) {
                CfgWarning ("Segment `%s' does not exist", GetString (S->Name));
            }
      	    /* Discard the descriptor */
      	    FreeSegDesc (S);
      	}

	/* Skip the semicolon */
	CfgConsumeSemi ();
    }

    /* Remember we had this section */
    SectionsEncountered |= SE_SEGMENTS;
}



static void ParseO65 (void)
/* Parse the o65 format section */
{
    static const IdentTok Attributes [] = {
       	{   "EXPORT",   CFGTOK_EXPORT   	},
	{   "IMPORT",	CFGTOK_IMPORT		},
        {   "TYPE",     CFGTOK_TYPE     	},
       	{   "OS",      	CFGTOK_OS       	},
       	{   "ID",      	CFGTOK_ID       	},
       	{   "VERSION",  CFGTOK_VERSION          },
    };
    static const IdentTok Types [] = {
       	{   "SMALL",   	CFGTOK_SMALL    	},
       	{   "LARGE",   	CFGTOK_LARGE    	},
    };
    static const IdentTok OperatingSystems [] = {
       	{   "LUNIX",   	CFGTOK_LUNIX     	},
       	{   "OSA65",   	CFGTOK_OSA65    	},
        {   "CC65",     CFGTOK_CC65             },
        {   "OPENCBM",  CFGTOK_OPENCBM          },
    };

    /* Bitmask to remember the attributes we got already */
    enum {
       	atNone		= 0x0000,
	atOS            = 0x0001,
        atOSVersion     = 0x0002,
	atType	       	= 0x0004,
	atImport        = 0x0008,
	atExport    	= 0x0010,
        atID            = 0x0020,
        atVersion       = 0x0040
    };
    unsigned AttrFlags = atNone;

    /* Remember the attributes read */
    unsigned CfgSValId;
    unsigned OS = 0;            /* Initialize to keep gcc happy */
    unsigned Version = 0;

    /* Read the attributes */
    while (CfgTok == CFGTOK_IDENT) {

	/* Map the identifier to a token */
	cfgtok_t AttrTok;
       	CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	AttrTok = CfgTok;

	/* An optional assignment follows */
	CfgNextTok ();
	CfgOptionalAssign ();

	/* Check which attribute was given */
	switch (AttrTok) {

	    case CFGTOK_EXPORT:
                /* Remember we had this token (maybe more than once) */
                AttrFlags |= atExport;
	      	/* We expect an identifier */
		CfgAssureIdent ();
                /* Convert the string into a string index */
                CfgSValId = GetStrBufId (&CfgSVal);
	        /* Check if the export symbol is also defined as an import. */
	       	if (O65GetImport (O65FmtDesc, CfgSValId) != 0) {
		    CfgError ("Exported symbol `%s' cannot be an import",
                              SB_GetConstBuf (&CfgSVal));
		}
      		/* Check if we have this symbol defined already. The entry
      	     	 * routine will check this also, but we get a more verbose
      		 * error message when checking it here.
      		 */
      	 	if (O65GetExport (O65FmtDesc, CfgSValId) != 0) {
      	  	    CfgError ("Duplicate exported symbol: `%s'",
                              SB_GetConstBuf (&CfgSVal));
      	 	}
		/* Insert the symbol into the table */
	  	O65SetExport (O65FmtDesc, CfgSValId);
                /* Eat the identifier token */
                CfgNextTok ();
	    	break;

	    case CFGTOK_IMPORT:
                /* Remember we had this token (maybe more than once) */
                AttrFlags |= atImport;
	      	/* We expect an identifier */
		CfgAssureIdent ();
                /* Convert the string into a string index */
                CfgSValId = GetStrBufId (&CfgSVal);
	        /* Check if the imported symbol is also defined as an export. */
	       	if (O65GetExport (O65FmtDesc, CfgSValId) != 0) {
		    CfgError ("Imported symbol `%s' cannot be an export",
                              SB_GetConstBuf (&CfgSVal));
		}
      	    	/* Check if we have this symbol defined already. The entry
      	    	 * routine will check this also, but we get a more verbose
      	    	 * error message when checking it here.
      	    	 */
      	    	if (O65GetImport (O65FmtDesc, CfgSValId) != 0) {
      	    	    CfgError ("Duplicate imported symbol: `%s'",
                              SB_GetConstBuf (&CfgSVal));
      	    	}
	    	/* Insert the symbol into the table */
	    	O65SetImport (O65FmtDesc, CfgSValId);
                /* Eat the identifier token */
                CfgNextTok ();
	    	break;

	    case CFGTOK_TYPE:
		/* Cannot have this attribute twice */
		FlagAttr (&AttrFlags, atType, "TYPE");
		/* Get the type of the executable */
		CfgSpecialToken (Types, ENTRY_COUNT (Types), "Type");
		switch (CfgTok) {

		    case CFGTOK_SMALL:
		        O65SetSmallModel (O65FmtDesc);
		     	break;

		    case CFGTOK_LARGE:
		    	O65SetLargeModel (O65FmtDesc);
	    	     	break;

	    	    default:
	    	     	CfgError ("Unexpected type token");
	    	}
                /* Eat the attribute token */
                CfgNextTok ();
	     	break;

	    case CFGTOK_OS:
	     	/* Cannot use this attribute twice */
	     	FlagAttr (&AttrFlags, atOS, "OS");
	     	/* Get the operating system. It may be specified as name or
                 * as a number in the range 1..255.
                 */
		if (CfgTok == CFGTOK_INTCON) {
		    CfgRangeCheck (O65OS_MIN, O65OS_MAX);
		    OS = (unsigned) CfgIVal;
		} else {
                    CfgSpecialToken (OperatingSystems, ENTRY_COUNT (OperatingSystems), "OS type");
                    switch (CfgTok) {
                        case CFGTOK_LUNIX:    OS = O65OS_LUNIX;     break;
                        case CFGTOK_OSA65:    OS = O65OS_OSA65;     break;
                        case CFGTOK_CC65:     OS = O65OS_CC65;      break;
                        case CFGTOK_OPENCBM:  OS = O65OS_OPENCBM;   break;
                        default:              CfgError ("Unexpected OS token");
                    }
                }
                CfgNextTok ();
	     	break;

            case CFGTOK_ID:
                /* Cannot have this attribute twice */
                FlagAttr (&AttrFlags, atID, "ID");
                /* We're expecting a number in the 0..$FFFF range*/
                ModuleId = (unsigned) CfgCheckedIntExpr (0, 0xFFFF);
                break;

            case CFGTOK_VERSION:
                /* Cannot have this attribute twice */
                FlagAttr (&AttrFlags, atVersion, "VERSION");
                /* We're expecting a number in byte range */
                Version = (unsigned) CfgCheckedIntExpr (0, 0xFF);
                break;

	    default:
		FAIL ("Unexpected attribute token");

	}

	/* Skip an optional comma */
	CfgOptionalComma ();
    }

    /* Check if we have all mandatory attributes */
    AttrCheck (AttrFlags, atOS, "OS");

    /* Check for attributes that may not be combined */
    if (OS == O65OS_CC65) {
        if ((AttrFlags & (atImport | atExport)) != 0 && ModuleId < 0x8000) {
            CfgError ("OS type CC65 may not have imports or exports for ids < $8000");
        }
    } else {
        if (AttrFlags & atID) {
            CfgError ("Operating system does not support the ID attribute");
        }
    }

    /* Set the O65 operating system to use */
    O65SetOS (O65FmtDesc, OS, Version, ModuleId);
}



static void ParseFormats (void)
/* Parse a target format section */
{
    static const IdentTok Formats [] = {
       	{   "O65",     	CFGTOK_O65      },
       	{   "BIN",     	CFGTOK_BIN      },
       	{   "BINARY",   CFGTOK_BIN      },
    };

    while (CfgTok == CFGTOK_IDENT) {

	/* Map the identifier to a token */
	cfgtok_t FormatTok;
       	CfgSpecialToken (Formats, ENTRY_COUNT (Formats), "Format");
	FormatTok = CfgTok;

	/* Skip the name and the following colon */
	CfgNextTok ();
	CfgConsumeColon ();

	/* Parse the format options */
	switch (FormatTok) {

	    case CFGTOK_O65:
		ParseO65 ();
		break;

	    case CFGTOK_BIN:
		/* No attribibutes available */
		break;

	    default:
		Error ("Unexpected format token");
	}

	/* Skip the semicolon */
	CfgConsumeSemi ();
    }


    /* Remember we had this section */
    SectionsEncountered |= SE_FORMATS;
}



static void ParseConDes (void)
/* Parse the CONDES feature */
{
    static const IdentTok Attributes [] = {
       	{   "SEGMENT",	   	CFGTOK_SEGMENT		},
	{   "LABEL",  	   	CFGTOK_LABEL  		},
	{   "COUNT",	   	CFGTOK_COUNT		},
	{   "TYPE",	   	CFGTOK_TYPE   		},
	{   "ORDER",	   	CFGTOK_ORDER		},
    };

    static const IdentTok Types [] = {
       	{   "CONSTRUCTOR",	CFGTOK_CONSTRUCTOR	},
	{   "DESTRUCTOR",      	CFGTOK_DESTRUCTOR	},
        {   "INTERRUPTOR",      CFGTOK_INTERRUPTOR      },
    };

    static const IdentTok Orders [] = {
	{   "DECREASING",      	CFGTOK_DECREASING	},
       	{   "INCREASING",      	CFGTOK_INCREASING	},
    };

    /* Attribute values. */
    unsigned SegName = INVALID_STRING_ID;
    unsigned Label   = INVALID_STRING_ID;
    unsigned Count   = INVALID_STRING_ID;
    /* Initialize to avoid gcc warnings: */
    int Type = -1;
    ConDesOrder Order = cdIncreasing;

    /* Bitmask to remember the attributes we got already */
    enum {
	atNone	   	= 0x0000,
	atSegName  	= 0x0001,
	atLabel	   	= 0x0002,
	atCount	   	= 0x0004,
	atType	   	= 0x0008,
	atOrder	   	= 0x0010
    };
    unsigned AttrFlags = atNone;

    /* Parse the attributes */
    while (1) {

	/* Map the identifier to a token */
	cfgtok_t AttrTok;
       	CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	AttrTok = CfgTok;

	/* An optional assignment follows */
	CfgNextTok ();
	CfgOptionalAssign ();

	/* Check which attribute was given */
	switch (AttrTok) {

	    case CFGTOK_SEGMENT:
	  	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atSegName, "SEGMENT");
	      	/* We expect an identifier */
		CfgAssureIdent ();
		/* Remember the value for later */
		SegName = GetStrBufId (&CfgSVal);
	    	break;

	    case CFGTOK_LABEL:
	       	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atLabel, "LABEL");
	      	/* We expect an identifier */
		CfgAssureIdent ();
		/* Remember the value for later */
		Label = GetStrBufId (&CfgSVal);
		break;

	    case CFGTOK_COUNT:
	       	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atCount, "COUNT");
	      	/* We expect an identifier */
		CfgAssureIdent ();
		/* Remember the value for later */
		Count = GetStrBufId (&CfgSVal);
		break;

	    case CFGTOK_TYPE:
	  	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atType, "TYPE");
		/* The type may be given as id or numerical */
		if (CfgTok == CFGTOK_INTCON) {
		    CfgRangeCheck (CD_TYPE_MIN, CD_TYPE_MAX);
		    Type = (int) CfgIVal;
		} else {
		    CfgSpecialToken (Types, ENTRY_COUNT (Types), "Type");
		    switch (CfgTok) {
		     	case CFGTOK_CONSTRUCTOR: Type = CD_TYPE_CON;	break;
		     	case CFGTOK_DESTRUCTOR:	 Type = CD_TYPE_DES;	break;
                        case CFGTOK_INTERRUPTOR: Type = CD_TYPE_INT;    break;
	     	     	default: FAIL ("Unexpected type token");
		    }
		}
		break;

	    case CFGTOK_ORDER:
	       	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atOrder, "ORDER");
		CfgSpecialToken (Orders, ENTRY_COUNT (Orders), "Order");
		switch (CfgTok) {
		    case CFGTOK_DECREASING: Order = cdDecreasing;	break;
		    case CFGTOK_INCREASING: Order = cdIncreasing;	break;
		    default: FAIL ("Unexpected order token");
		}
		break;

	    default:
		FAIL ("Unexpected attribute token");

	}

       	/* Skip the attribute value */
	CfgNextTok ();

	/* Semicolon ends the ConDes decl, otherwise accept an optional comma */
	if (CfgTok == CFGTOK_SEMI) {
	    break;
	} else if (CfgTok == CFGTOK_COMMA) {
	    CfgNextTok ();
	}
    }

    /* Check if we have all mandatory attributes */
    AttrCheck (AttrFlags, atSegName, "SEGMENT");
    AttrCheck (AttrFlags, atLabel, "LABEL");
    AttrCheck (AttrFlags, atType, "TYPE");

    /* Check if the condes has already attributes defined */
    if (ConDesHasSegName(Type) || ConDesHasLabel(Type)) {
	CfgError ("CONDES attributes for type %d are already defined", Type);
    }

    /* Define the attributes */
    ConDesSetSegName (Type, SegName);
    ConDesSetLabel (Type, Label);
    if (AttrFlags & atCount) {
	ConDesSetCountSym (Type, Count);
    }
    if (AttrFlags & atOrder) {
	ConDesSetOrder (Type, Order);
    }
}



static void ParseStartAddress (void)
/* Parse the STARTADDRESS feature */
{
    static const IdentTok Attributes [] = {
       	{   "DEFAULT",  CFGTOK_DEFAULT },
    };


    /* Attribute values. */
    unsigned long DefStartAddr = 0;

    /* Bitmask to remember the attributes we got already */
    enum {
	atNone		= 0x0000,
       	atDefault       = 0x0001
    };
    unsigned AttrFlags = atNone;

    /* Parse the attributes */
    while (1) {

	/* Map the identifier to a token */
	cfgtok_t AttrTok;
       	CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
	AttrTok = CfgTok;

	/* An optional assignment follows */
	CfgNextTok ();
	CfgOptionalAssign ();

	/* Check which attribute was given */
	switch (AttrTok) {

	    case CFGTOK_DEFAULT:
	  	/* Don't allow this twice */
		FlagAttr (&AttrFlags, atDefault, "DEFAULT");
	      	/* We expect a numeric expression */
                DefStartAddr = CfgCheckedIntExpr (0, 0xFFFFFF);
	    	break;

	    default:
		FAIL ("Unexpected attribute token");

	}

	/* Semicolon ends the ConDes decl, otherwise accept an optional comma */
	if (CfgTok == CFGTOK_SEMI) {
	    break;
	} else if (CfgTok == CFGTOK_COMMA) {
	    CfgNextTok ();
	}
    }

    /* Check if we have all mandatory attributes */
    AttrCheck (AttrFlags, atDefault, "DEFAULT");

    /* If no start address was given on the command line, use the one given
     * here
     */
    if (!HaveStartAddr) {
        StartAddr = DefStartAddr;
    }
}



static void ParseFeatures (void)
/* Parse a features section */
{
    static const IdentTok Features [] = {
       	{   "CONDES",  	    CFGTOK_CONDES	},
        {   "STARTADDRESS", CFGTOK_STARTADDRESS },
    };

    while (CfgTok == CFGTOK_IDENT) {

    	/* Map the identifier to a token */
    	cfgtok_t FeatureTok;
       	CfgSpecialToken (Features, ENTRY_COUNT (Features), "Feature");
       	FeatureTok = CfgTok;

    	/* Skip the name and the following colon */
    	CfgNextTok ();
    	CfgConsumeColon ();

    	/* Parse the format options */
    	switch (FeatureTok) {

    	    case CFGTOK_CONDES:
    	 	ParseConDes ();
    	 	break;

            case CFGTOK_STARTADDRESS:
                ParseStartAddress ();
                break;


    	    default:
       	       	FAIL ("Unexpected feature token");
    	}

    	/* Skip the semicolon */
    	CfgConsumeSemi ();
    }

    /* Remember we had this section */
    SectionsEncountered |= SE_FEATURES;
}



static void ParseSymbols (void)
/* Parse a symbols section */
{
    static const IdentTok Attributes[] = {
       	{   "VALUE",   	CFGTOK_VALUE    },
        {   "WEAK",     CFGTOK_WEAK     },
    };

    while (CfgTok == CFGTOK_IDENT) {

	long Val = 0L;
        int  Weak = 0;
        Export* E;

	/* Remember the name */
	unsigned Name = GetStrBufId (&CfgSVal);
	CfgNextTok ();

        /* Support both, old and new syntax here. New syntax is a colon
         * followed by an attribute list, old syntax is an optional equal
         * sign plus a value.
         */
        if (CfgTok != CFGTOK_COLON) {

            /* Old syntax */

            /* Allow an optional assignment */
            CfgOptionalAssign ();

            /* Make sure the next token is an integer expression, read and
             * skip it.
             */
            Val = CfgIntExpr ();

        } else {

            /* Bitmask to remember the attributes we got already */
            enum {
                atNone	     	= 0x0000,
                atValue         = 0x0001,
                atWeak          = 0x0002
            };
            unsigned AttrFlags = atNone;


            /* New syntax - skip the colon */
            CfgNextTok ();

            /* Parse the attributes */
            while (1) {

                /* Map the identifier to a token */
                cfgtok_t AttrTok;
                CfgSpecialToken (Attributes, ENTRY_COUNT (Attributes), "Attribute");
                AttrTok = CfgTok;

                /* Skip the attribute name */
                CfgNextTok ();

                /* An optional assignment follows */
                CfgOptionalAssign ();

                /* Check which attribute was given */
                switch (AttrTok) {

                    case CFGTOK_VALUE:
                        /* Don't allow this twice */
                        FlagAttr (&AttrFlags, atValue, "VALUE");
                        /* We expect a numeric expression */
                        Val = CfgIntExpr ();
                        break;

                    case CFGTOK_WEAK:
                        /* Don't allow this twice */
                        FlagAttr (&AttrFlags, atWeak, "WEAK");
                        CfgBoolToken ();
                        Weak = (CfgTok == CFGTOK_TRUE);
                        CfgNextTok ();
                        break;

                    default:
                        FAIL ("Unexpected attribute token");

                }

                /* Semicolon ends the decl, otherwise accept an optional comma */
                if (CfgTok == CFGTOK_SEMI) {
                    break;
                } else if (CfgTok == CFGTOK_COMMA) {
                    CfgNextTok ();
                }
            }

            /* Check if we have all mandatory attributes */
            AttrCheck (AttrFlags, atValue, "VALUE");

            /* Weak is optional, the default are non weak symbols */
            if ((AttrFlags & atWeak) == 0) {
                Weak = 0;
            }

        }

        /* Check if the symbol is already defined */
        if ((E = FindExport (Name)) != 0 && !IsUnresolvedExport (E)) {
            /* If the symbol is not marked as weak, this is an error.
             * Otherwise ignore the symbol from the config.
             */
            if (!Weak) {
                CfgError ("Symbol `%s' is already defined", GetString (Name));
            }
        } else {
            /* The symbol is undefined, generate an export */
            CreateConstExport (Name, Val);
        }

    	/* Skip the semicolon */
    	CfgConsumeSemi ();
    }

    /* Remember we had this section */
    SectionsEncountered |= SE_SYMBOLS;
}



static void ParseConfig (void)
/* Parse the config file */
{
    static const IdentTok BlockNames [] = {
	{   "MEMORY",  	CFGTOK_MEMORY	},
	{   "FILES",    CFGTOK_FILES    },
        {   "SEGMENTS", CFGTOK_SEGMENTS },
	{   "FORMATS", 	CFGTOK_FORMATS  },
	{   "FEATURES", CFGTOK_FEATURES	},
	{   "SYMBOLS",	CFGTOK_SYMBOLS 	},
    };
    cfgtok_t BlockTok;

    do {

	/* Read the block ident */
       	CfgSpecialToken (BlockNames, ENTRY_COUNT (BlockNames), "Block identifier");
	BlockTok = CfgTok;
	CfgNextTok ();

	/* Expected a curly brace */
	CfgConsume (CFGTOK_LCURLY, "`{' expected");

	/* Read the block */
	switch (BlockTok) {

	    case CFGTOK_MEMORY:
	     	ParseMemory ();
	     	break;

	    case CFGTOK_FILES:
	     	ParseFiles ();
	     	break;

	    case CFGTOK_SEGMENTS:
	     	ParseSegments ();
	     	break;

	    case CFGTOK_FORMATS:
	     	ParseFormats ();
	     	break;

	    case CFGTOK_FEATURES:
		ParseFeatures ();
		break;

	    case CFGTOK_SYMBOLS:
		ParseSymbols ();
		break;

	    default:
	     	FAIL ("Unexpected block token");

	}

	/* Skip closing brace */
	CfgConsume (CFGTOK_RCURLY, "`}' expected");

    } while (CfgTok != CFGTOK_EOF);
}



void CfgRead (void)
/* Read the configuration */
{
    /* Create the descriptors for the binary formats */
    BinFmtDesc = NewBinDesc ();
    O65FmtDesc = NewO65Desc ();

    /* If we have a config name given, open the file, otherwise we will read
     * from a buffer.
     */
    CfgOpenInput ();

    /* Parse the file */
    ParseConfig ();

    /* Close the input file */
    CfgCloseInput ();
}



static void CreateRunDefines (SegDesc* S, unsigned long SegAddr)
/* Create the defines for a RUN segment */
{
    StrBuf Buf = STATIC_STRBUF_INITIALIZER;

    SB_Printf (&Buf, "__%s_RUN__", GetString (S->Name));
    CreateMemoryExport (GetStrBufId (&Buf), S->Run, SegAddr - S->Run->Start);
    SB_Printf (&Buf, "__%s_SIZE__", GetString (S->Name));
    CreateConstExport (GetStrBufId (&Buf), S->Seg->Size);
    S->Flags |= SF_RUN_DEF;
    SB_Done (&Buf);
}



static void CreateLoadDefines (SegDesc* S, unsigned long SegAddr)
/* Create the defines for a LOAD segment */
{
    StrBuf Buf = STATIC_STRBUF_INITIALIZER;

    SB_Printf (&Buf, "__%s_LOAD__", GetString (S->Name));
    CreateMemoryExport (GetStrBufId (&Buf), S->Load, SegAddr - S->Load->Start);
    S->Flags |= SF_LOAD_DEF;
    SB_Done (&Buf);
}



unsigned CfgAssignSegments (void)
/* Assign segments, define linker symbols where requested. The function will
 * return the number of memory area overflows (so zero means anything went ok).
 * In case of overflows, a short mapfile can be generated later, to ease the
 * task of rearranging segments for the user.
 */
{
    unsigned Overflows = 0;

    /* Walk through each of the memory sections. Add up the sizes and check
     * for an overflow of the section. Assign the start addresses of the
     * segments while doing this.
     */
    unsigned I;
    for (I = 0; I < CollCount (&MemoryList); ++I) {

        MemListNode* N;

        /* Get this entry */
        Memory* M = CollAtUnchecked (&MemoryList, I);

     	/* Get the start address of this memory area */
     	unsigned long Addr = M->Start;

        /* Remember if this is a relocatable memory area */
        M->Relocatable = RelocatableBinFmt (M->F->Format);

     	/* Walk through the segments in this memory area */
     	N = M->SegList;
     	while (N) {

     	    /* Get the segment from the node */
     	    SegDesc* S = N->Seg;

            /* Some actions depend on wether this is the load or run memory
             * area.
             */
            if (S->Run == M) {

                /* This is the run (and maybe load) memory area. Handle
                 * alignment and explict start address and offset.
                 */
                if (S->Flags & SF_ALIGN) {
                    /* Align the address */
                    unsigned long Val = (0x01UL << S->Align) - 1;
                    Addr = (Addr + Val) & ~Val;
                } else if (S->Flags & (SF_OFFSET | SF_START)) {
                    /* Give the segment a fixed starting address */
                    unsigned long NewAddr = S->Addr;
                    if (S->Flags & SF_OFFSET) {
                        /* An offset was given, no address, make an address */
                        NewAddr += M->Start;
                    }
                    if (Addr > NewAddr) {
                        /* Offset already too large */
                        if (S->Flags & SF_OFFSET) {
                            Error ("Offset too small in `%s', segment `%s'",
                                   GetString (M->Name), GetString (S->Name));
                        } else {
                            Error ("Start address too low in `%s', segment `%s'",
                                   GetString (M->Name), GetString (S->Name));
                        }
                    }
                    Addr = NewAddr;
                }

                /* Set the start address of this segment, set the readonly flag
                 * in the segment and and remember if the segment is in a
                 * relocatable file or not.
                 */
                S->Seg->PC = Addr;
                S->Seg->ReadOnly = (S->Flags & SF_RO) != 0;
                S->Seg->Relocatable = M->Relocatable;

            } else if (S->Load == M) {

                /* This is the load memory area, *and* run and load are
                 * different (because of the "else" above). Handle alignment.
                 */
                if (S->Flags & SF_ALIGN_LOAD) {
                    /* Align the address */
                    unsigned long Val = (0x01UL << S->AlignLoad) - 1;
                    Addr = (Addr + Val) & ~Val;
                }

            }

     	    /* Increment the fill level of the memory area and check for an
     	     * overflow.
     	     */
     	    M->FillLevel = Addr + S->Seg->Size - M->Start;
       	    if (M->FillLevel > M->Size && (M->Flags & MF_OVERFLOW) == 0) {
                ++Overflows;
                M->Flags |= MF_OVERFLOW;
                Warning ("Memory area overflow in `%s', segment `%s' (%lu bytes)",
                         GetString (M->Name), GetString (S->Name),
                         M->FillLevel - M->Size);
     	    }

     	    /* If requested, define symbols for the start and size of the
     	     * segment.
     	     */
     	    if (S->Flags & SF_DEFINE) {
                if (S->Run == M && (S->Flags & SF_RUN_DEF) == 0) {
                    CreateRunDefines (S, Addr);
                }
                if (S->Load == M && (S->Flags & SF_LOAD_DEF) == 0) {
                    CreateLoadDefines (S, Addr);
                }
     	    }

     	    /* Calculate the new address */
     	    Addr += S->Seg->Size;

	    /* Next segment */
	    N = N->Next;
	}

	/* If requested, define symbols for start and size of the memory area */
	if (M->Flags & MF_DEFINE) {
	    StrBuf Buf = STATIC_STRBUF_INITIALIZER;
	    SB_Printf (&Buf, "__%s_START__", GetString (M->Name));
	    CreateMemoryExport (GetStrBufId (&Buf), M, 0);
	    SB_Printf (&Buf, "__%s_SIZE__", GetString (M->Name));
	    CreateConstExport (GetStrBufId (&Buf), M->Size);
	    SB_Printf (&Buf, "__%s_LAST__", GetString (M->Name));
	    CreateMemoryExport (GetStrBufId (&Buf), M, M->FillLevel);
            SB_Done (&Buf);
	}

    }

    /* Return the number of memory area overflows */
    return Overflows;
}



void CfgWriteTarget (void)
/* Write the target file(s) */
{
    unsigned I;

    /* Walk through the files list */
    for (I = 0; I < CollCount (&FileList); ++I) {

        /* Get this entry */
        File* F = CollAtUnchecked (&FileList, I);

    	/* We don't need to look at files with no memory areas */
  	if (CollCount (&F->MemList) > 0) {

  	    /* Is there an output file? */
  	    if (SB_GetLen (GetStrBuf (F->Name)) > 0) {

  		/* Assign a proper binary format */
  		if (F->Format == BINFMT_DEFAULT) {
  	    	    F->Format = DefaultBinFmt;
     		}

     		/* Call the apropriate routine for the binary format */
  		switch (F->Format) {

  		    case BINFMT_BINARY:
  		       	BinWriteTarget (BinFmtDesc, F);
  		       	break;

  		    case BINFMT_O65:
  		       	O65WriteTarget (O65FmtDesc, F);
  		       	break;

  		    default:
  		        Internal ("Invalid binary format: %u", F->Format);

  		}

  	    } else {

  		/* No output file. Walk through the list and mark all segments
       	       	 * loading into these memory areas in this file as dumped.
  		 */ 
                unsigned J;
                for (J = 0; J < CollCount (&F->MemList); ++J) {

		    MemListNode* N;

                    /* Get this entry */
  		    Memory* M = CollAtUnchecked (&F->MemList, J);

		    /* Debugging */
       	       	    Print (stdout, 2, "Skipping `%s'...\n", GetString (M->Name));

  		    /* Walk throught the segments */
  		    N = M->SegList;
  		    while (N) {
		       	if (N->Seg->Load == M) {
  		       	    /* Load area - mark the segment as dumped */
  		       	    N->Seg->Seg->Dumped = 1;
  		       	}

		       	/* Next segment node */
		       	N = N->Next;
		    }
		}
	    }
	}
    }
}




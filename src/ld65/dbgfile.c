/*****************************************************************************/
/*                                                                           */
/*				   dbgfile.c				     */
/*                                                                           */
/*                  Debug file creation for the ld65 linker                  */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2003-2011, Ullrich von Bassewitz                                      */
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
#include <string.h>
#include <errno.h>

/* ld65 */
#include "dbgfile.h"
#include "dbgsyms.h"
#include "error.h"
#include "fileinfo.h"
#include "global.h"
#include "library.h"
#include "lineinfo.h"
#include "scopes.h"
#include "segments.h"



/*****************************************************************************/
/*  	     	 		     Code   				     */
/*****************************************************************************/



static void AssignBaseIds (void)
/* Assign the base ids for debug info output. Within each module, many of the
 * items are addressed by ids which are actually the indices of the items in
 * the collections. To make them unique, we must assign a unique base to each
 * range.
 */
{
    unsigned I;

    /* Walk over all modules */
    unsigned FileBaseId  = 0;
    unsigned SymBaseId   = 0;
    unsigned ScopeBaseId = 0;
    for (I = 0; I < CollCount (&ObjDataList); ++I) {

        /* Get this module */
        ObjData* O = CollAt (&ObjDataList, I);

        /* Assign ids */
        O->FileBaseId  = FileBaseId;
        O->SymBaseId   = SymBaseId;
        O->ScopeBaseId = ScopeBaseId;

        /* Bump the base ids */
        FileBaseId    += CollCount (&O->Files);
        SymBaseId     += CollCount (&O->DbgSyms);
        ScopeBaseId   += CollCount (&O->Scopes);
    }
}



void CreateDbgFile (void)
/* Create a debug info file */
{
    unsigned I;

    /* Open the debug info file */
    FILE* F = fopen (DbgFileName, "w");
    if (F == 0) {
       	Error ("Cannot create debug file `%s': %s", DbgFileName, strerror (errno));
    }

    /* Output version information */
    fprintf (F, "version\tmajor=1,minor=2\n");

    /* Assign the base ids to the modules */
    AssignBaseIds ();

    /* Output libraries */
    PrintDbgLibraries (F);

    /* Output modules */
    for (I = 0; I < CollCount (&ObjDataList); ++I) {

        /* Get this object file */
        const ObjData* O = CollConstAt (&ObjDataList, I);

        /* The main source file is the one at index zero */
        const FileInfo* Source = CollConstAt (&O->Files, 0);

        /* Output the module line */
        fprintf (F,
                 "mod\tid=%u,name=\"%s\",file=%u",
                 I,
                 GetObjFileName (O),
                 Source->Id);

        /* Add library if any */
        if (O->Lib != 0) {
            fprintf (F, ",lib=%u", GetLibId (O->Lib));
        }

        /* Terminate the output line */
        fputc ('\n', F);
    }

    /* Output the segment info */
    PrintDbgSegments (F);

    /* Output files */
    PrintDbgFileInfo (F);

    /* Output line info */
    PrintDbgLineInfo (F);

    /* Output symbols */
    PrintDbgSyms (F);

    /* Output scopes */
    PrintDbgScopes (F);

    /* Close the file */
    if (fclose (F) != 0) {
	Error ("Error closing debug file `%s': %s", DbgFileName, strerror (errno));
    }
}




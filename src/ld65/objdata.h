/*****************************************************************************/
/*                                                                           */
/*				   objdata.h				     */
/*                                                                           */
/*		 Handling object file data for the ld65 linker		     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998     Ullrich von Bassewitz                                        */
/*              Wacholderweg 14                                              */
/*              D-70597 Stuttgart                                            */
/* EMail:       uz@musoftware.de                                             */
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



#ifndef OBJDATA_H
#define OBJDATA_H



/* common */
#include "objdefs.h"



/*****************************************************************************/
/*     	      	    	      	     Data		      		     */
/*****************************************************************************/



/* Values for the Flags field */
#define	OBJ_REF		0x0001 	       	/* We have a reference to this file */
#define OBJ_HAVEDATA	0x0002		/* We have this object file already */
#define OBJ_MARKED     	0x0004 		/* Generic marker bit */


/* Internal structure holding object file data */
typedef struct ObjData ObjData;
struct ObjData {
    ObjData*	     	Next; 		/* Linked list of all objects */
    char*   	     	Name; 		/* Module name */
    char*      	       	LibName;	/* Name of library */
    ObjHeader	 	Header;		/* Header of file */
    unsigned long	Start;		/* Start offset of data in library */
    unsigned 	     	Flags;
    unsigned 	 	FileCount;	/* Input file count */
    char**	 	Files;		/* List of input files */
    unsigned	 	SectionCount;	/* Count of sections in this object */
    struct Section**  	Sections;	/* List of all sections */
    unsigned	  	ExportCount;	/* Count of exports */
    struct Export**	Exports;       	/* List of all exports */
    unsigned	 	ImportCount;	/* Count of imports */
    struct Import**	Imports;	/* List of all imports */
    unsigned	 	DbgSymCount;	/* Count of debug symbols */
    struct DbgSym**   	DbgSyms;       	/* List of debug symbols */
};



/* Object data list management */
extern unsigned		ObjCount;	/* Count of files in the list */
extern ObjData*		ObjRoot;	/* List of object files */
extern ObjData*		ObjLast;	/* Last entry in list */



/*****************************************************************************/
/*     	      	    		     Code		  	   	     */
/*****************************************************************************/



ObjData* NewObjData (void);
/* Allocate a new structure on the heap, insert it into the list, return it */

void FreeObjData (ObjData* O);
/* Free a complete struct */



/* End of objdata.h */

#endif





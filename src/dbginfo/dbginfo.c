/*****************************************************************************/
/*                                                                           */
/*                                 dbginfo.h                                 */
/*                                                                           */
/*                         cc65 debug info handling                          */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2010,      Ullrich von Bassewitz                                      */
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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>

#include "dbginfo.h"



/*****************************************************************************/
/*     	       	       	 	     Data				     */
/*****************************************************************************/



/* Version numbers of the debug format we understand */
#define VER_MAJOR       1U
#define VER_MINOR       0U

/* Dynamic strings */
typedef struct StrBuf StrBuf;
struct StrBuf {
    char*       Buf;                    /* Pointer to buffer */
    unsigned    Len;                    /* Length of the string */
    unsigned    Allocated;              /* Size of allocated memory */
};

/* Initializer for a string buffer */
#define STRBUF_INITIALIZER      { 0, 0, 0 }

/* An array of pointers that grows if needed */
typedef struct Collection Collection;
struct Collection {
    unsigned   	       	Count;		/* Number of items in the list */
    unsigned   	       	Size;		/* Size of allocated array */
    void**	    	Items;		/* Array with dynamic size */
};

/* Initializer for static collections */
#define COLLECTION_INITIALIZER  { 0, 0, 0 }



/* Data structure containing information from the debug info file. A pointer
 * to this structure is passed as handle to callers from the outside.
 */
typedef struct DbgInfo DbgInfo;
struct DbgInfo {
    Collection  SegInfoByName;          /* Segment infos sorted by name */
    Collection  SegInfoById;            /* Segment infos sorted by id */
    Collection  FileInfoByName;         /* File infos sorted by name */
    Collection  FileInfoById;           /* File infos sorted by id */
    Collection  LineInfoByAddr;         /* Line information sorted by address */
    Collection  SymInfoByName;          /* Symbol information sorted by name */
};

/* Input tokens */
typedef enum {

    TOK_INVALID,                        /* Invalid token */
    TOK_EOF,                            /* End of file reached */

    TOK_INTCON,                         /* Integer constant */
    TOK_STRCON,                         /* String constant */

    TOK_EQUAL,                          /* = */
    TOK_COMMA,                          /* , */
    TOK_MINUS,                          /* - */
    TOK_PLUS,                           /* + */
    TOK_EOL,                            /* \n */

    TOK_ABSOLUTE,                       /* ABSOLUTE keyword */
    TOK_ADDRSIZE,                       /* ADDRSIZE keyword */
    TOK_EQUATE,                         /* EQUATE keyword */
    TOK_FILE,                           /* FILE keyword */
    TOK_ID,                             /* ID keyword */
    TOK_LABEL,                          /* LABEL keyword */
    TOK_LINE,                           /* LINE keyword */
    TOK_LONG,                           /* LONG_keyword */
    TOK_MAJOR,                          /* MAJOR keyword */
    TOK_MINOR,                          /* MINOR keyword */
    TOK_MTIME,                          /* MTIME keyword */
    TOK_NAME,                           /* NAME keyword */
    TOK_OUTPUTNAME,                     /* OUTPUTNAME keyword */
    TOK_OUTPUTOFFS,                     /* OUTPUTOFFS keyword */
    TOK_RANGE,                          /* RANGE keyword */
    TOK_RO,                             /* RO keyword */
    TOK_RW,                             /* RW keyword */
    TOK_SEGMENT,                        /* SEGMENT keyword */
    TOK_SIZE,                           /* SIZE keyword */
    TOK_START,                          /* START keyword */
    TOK_SYM,                            /* SYM keyword */
    TOK_TYPE,                           /* TYPE keyword */
    TOK_VALUE,                          /* VALUE keyword */
    TOK_VERSION,                        /* VERSION keyword */
    TOK_ZEROPAGE,                       /* ZEROPAGE keyword */

    TOK_IDENT,                          /* To catch unknown keywords */
} Token;

/* Data used when parsing the debug info file */
typedef struct InputData InputData;
struct InputData {
    const char*         FileName;       /* Name of input file */
    cc65_line           Line;           /* Current line number */
    unsigned            Col;            /* Current column number */
    cc65_line           SLine;          /* Line number at start of token */
    unsigned            SCol;           /* Column number at start of token */
    unsigned            Errors;         /* Number of errors */
    FILE*               F;              /* Input file */
    int                 C;              /* Input character */
    Token               Tok;            /* Token from input stream */
    unsigned long       IVal;           /* Integer constant */
    StrBuf              SVal;           /* String constant */
    cc65_errorfunc      Error;          /* Function called in case of errors */
    unsigned            MajorVersion;   /* Major version number */
    unsigned            MinorVersion;   /* Minor version number */
    DbgInfo*            Info;           /* Pointer to debug info */
};

/* Internally used segment info struct */
typedef struct SegInfo SegInfo;
struct SegInfo {
    unsigned            Id;             /* Id of segment */
    cc65_addr           Start;          /* Start address of segment */
    cc65_addr           Size;           /* Size of segment */
    char*               OutputName;     /* Name of output file */
    unsigned long       OutputOffs;     /* Offset in output file */
    char                SegName[1];     /* Name of segment */
};

/* Internally used file info struct */
typedef struct FileInfo FileInfo;
struct FileInfo {
    unsigned            Id;             /* Id of file */
    unsigned long       Size;           /* Size of file */
    unsigned long       MTime;          /* Modification time */
    cc65_addr           Start;          /* Start address of line infos */
    cc65_addr           End;            /* End address of line infos */
    Collection          LineInfoByAddr; /* Line infos sorted by address */
    Collection          LineInfoByLine; /* Line infos sorted by line */
    char                FileName[1];    /* Name of file with full path */
};

/* Internally used line info struct */
typedef struct LineInfo LineInfo;
struct LineInfo {
    cc65_addr           Start;          /* Start of data range */
    cc65_addr           End;            /* End of data range */
    cc65_line           Line;           /* Line number */
    union {
        unsigned        Id;             /* Id of file */
        FileInfo*       Info;           /* Pointer to file info */
    } File;
    union {
        unsigned        Id;             /* Id of segment */
        SegInfo*        Info;           /* Pointer to segment info */
    } Seg;
};

/* Internally used symbol info struct */
typedef struct SymInfo SymInfo;
struct SymInfo {
    cc65_symbol_type    Type;           /* Type of symbol */
    long                Value;          /* Value of symbol */
    char                SymName[1];     /* Name of symbol */
};



/*****************************************************************************/
/*                                 Forwards                                  */
/*****************************************************************************/



static void NextToken (InputData* D);
/* Read the next token from the input stream */



/*****************************************************************************/
/*                             Memory allocation                             */
/*****************************************************************************/



static void* xmalloc (size_t Size)
/* Allocate memory, check for out of memory condition. Do some debugging */
{
    void* P = 0;

    /* Allow zero sized requests and return NULL in this case */
    if (Size) {

        /* Allocate memory */
        P = malloc (Size);

        /* Check for errors */
        assert (P != 0);
    }

    /* Return a pointer to the block */
    return P;
}



static void* xrealloc (void* P, size_t Size)
/* Reallocate a memory block, check for out of memory */
{
    /* Reallocate the block */
    void* N = realloc (P, Size);

    /* Check for errors */
    assert (N != 0 || Size == 0);

    /* Return the pointer to the new block */
    return N;
}



static void xfree (void* Block)
/* Free the block, do some debugging */
{
    free (Block);
}



/*****************************************************************************/
/*                              Dynamic strings                              */
/*****************************************************************************/



static void SB_Done (StrBuf* B)
/* Free the data of a string buffer (but not the struct itself) */
{
    if (B->Allocated) {
        xfree (B->Buf);
    }
}



static void SB_Realloc (StrBuf* B, unsigned NewSize)
/* Reallocate the string buffer space, make sure at least NewSize bytes are
 * available.
 */
{
    /* Get the current size, use a minimum of 8 bytes */
    unsigned NewAllocated = B->Allocated;
    if (NewAllocated == 0) {
       	NewAllocated = 8;
    }

    /* Round up to the next power of two */
    while (NewAllocated < NewSize) {
       	NewAllocated *= 2;
    }

    /* Reallocate the buffer. Beware: The allocated size may be zero while the
     * length is not. This means that we have a buffer that wasn't allocated
     * on the heap.
     */
    if (B->Allocated) {
        /* Just reallocate the block */
        B->Buf   = xrealloc (B->Buf, NewAllocated);
    } else {
        /* Allocate a new block and copy */
        B->Buf   = memcpy (xmalloc (NewAllocated), B->Buf, B->Len);
    }

    /* Remember the new block size */
    B->Allocated = NewAllocated;
}



static void SB_CheapRealloc (StrBuf* B, unsigned NewSize)
/* Reallocate the string buffer space, make sure at least NewSize bytes are
 * available. This function won't copy the old buffer contents over to the new
 * buffer and may be used if the old contents are overwritten later.
 */
{
    /* Get the current size, use a minimum of 8 bytes */
    unsigned NewAllocated = B->Allocated;
    if (NewAllocated == 0) {
     	NewAllocated = 8;
    }

    /* Round up to the next power of two */
    while (NewAllocated < NewSize) {
     	NewAllocated *= 2;
    }

    /* Free the old buffer if there is one */
    if (B->Allocated) {
        xfree (B->Buf);
    }

    /* Allocate a fresh block */
    B->Buf = xmalloc (NewAllocated);

    /* Remember the new block size */
    B->Allocated = NewAllocated;
}



static unsigned SB_GetLen (const StrBuf* B)
/* Return the length of the buffer contents */
{
    return B->Len;
}



static const char* SB_GetConstBuf (const StrBuf* B)
/* Return a buffer pointer */
{
    return B->Buf;
}



static void SB_Terminate (StrBuf* B)
/* Zero terminate the given string buffer. NOTE: The terminating zero is not
 * accounted for in B->Len, if you want that, you have to use AppendChar!
 */
{
    unsigned NewLen = B->Len + 1;
    if (NewLen > B->Allocated) {
       	SB_Realloc (B, NewLen);
    }
    B->Buf[B->Len] = '\0';
}



static void SB_Clear (StrBuf* B)
/* Clear the string buffer (make it empty) */
{
    B->Len = 0;
}



static void SB_CopyBuf (StrBuf* Target, const char* Buf, unsigned Size)
/* Copy Buf to Target, discarding the old contents of Target */
{
    if (Size) {
        if (Target->Allocated < Size) {
            SB_CheapRealloc (Target, Size);
        }
        memcpy (Target->Buf, Buf, Size);
    }
    Target->Len = Size;
}



static void SB_Copy (StrBuf* Target, const StrBuf* Source)
/* Copy Source to Target, discarding the old contents of Target */
{
    SB_CopyBuf (Target, Source->Buf, Source->Len);
}



static void SB_AppendChar (StrBuf* B, int C)
/* Append a character to a string buffer */
{
    unsigned NewLen = B->Len + 1;
    if (NewLen > B->Allocated) {
       	SB_Realloc (B, NewLen);
    }
    B->Buf[B->Len] = (char) C;
    B->Len = NewLen;
}



static char* SB_StrDup (const StrBuf* B)
/* Return the contents of B as a dynamically allocated string. The string
 * will always be NUL terminated.
 */
{
    /* Allocate memory */
    char* S = xmalloc (B->Len + 1);

    /* Copy the string */
    memcpy (S, B->Buf, B->Len);

    /* Terminate it */
    S[B->Len] = '\0';

    /* And return the result */
    return S;
}



/*****************************************************************************/
/*                                Collections                                */
/*****************************************************************************/



static Collection* InitCollection (Collection* C)
/* Initialize a collection and return it. */
{
    /* Intialize the fields. */
    C->Count = 0;
    C->Size  = 0;
    C->Items = 0;

    /* Return the new struct */
    return C;
}



static void DoneCollection (Collection* C)
/* Free the data for a collection. This will not free the data contained in
 * the collection.
 */
{
    /* Free the pointer array */
    xfree (C->Items);
}



static unsigned CollCount (const Collection* C)
/* Return the number of items in the collection */
{
    return C->Count;
}



static void CollGrow (Collection* C, unsigned Size)
/* Grow the collection C so it is able to hold Size items without a resize
 * being necessary. This can be called for performance reasons if the number
 * of items to be placed in the collection is known in advance.
 */
{
    void** NewItems;

    /* Ignore the call if the collection is already large enough */
    if (Size <= C->Size) {
        return;
    }

    /* Grow the collection */
    C->Size = Size;
    NewItems = xmalloc (C->Size * sizeof (void*));
    memcpy (NewItems, C->Items, C->Count * sizeof (void*));
    xfree (C->Items);
    C->Items = NewItems;
}



static void CollInsert (Collection* C, void* Item, unsigned Index)
/* Insert the data at the given position in the collection */
{
    /* Check for invalid indices */
    assert (Index <= C->Count);

    /* Grow the array if necessary */
    if (C->Count >= C->Size) {
       	/* Must grow */
        CollGrow (C, (C->Size == 0)? 8 : C->Size * 2);
    }

    /* Move the existing elements if needed */
    if (C->Count != Index) {
       	memmove (C->Items+Index+1, C->Items+Index, (C->Count-Index) * sizeof (void*));
    }
    ++C->Count;

    /* Store the new item */
    C->Items[Index] = Item;
}



static void CollAppend (Collection* C, void* Item)
/* Append an item to the end of the collection */
{
    /* Insert the item at the end of the current list */
    CollInsert (C, Item, C->Count);
}



static void* CollAt (Collection* C, unsigned Index)
/* Return the item at the given index */
{
    /* Check the index */
    assert (Index < C->Count);

    /* Return the element */
    return C->Items[Index];
}



static void* CollFirst (Collection* C)
/* Return the first item in a collection */
{
    /* We must have at least one entry */
    assert (C->Count > 0);

    /* Return the element */
    return C->Items[0];
}



static void* CollLast (Collection* C)
/* Return the last item in a collection */
{
    /* We must have at least one entry */
    assert (C->Count > 0);

    /* Return the element */
    return C->Items[C->Count-1];
}



static void CollDelete (Collection* C, unsigned Index)
/* Remove the item with the given index from the collection. This will not
 * free the item itself, just the pointer. All items with higher indices
 * will get moved to a lower position.
 */
{
    /* Check the index */
    assert (Index < C->Count);

    /* Remove the item pointer */
    --C->Count;
    memmove (C->Items+Index, C->Items+Index+1, (C->Count-Index) * sizeof (void*));
}



static void CollQuickSort (Collection* C, int Lo, int Hi,
   	                   int (*Compare) (const void*, const void*))
/* Internal recursive sort function. */
{
    /* Get a pointer to the items */
    void** Items = C->Items;

    /* Quicksort */
    while (Hi > Lo) {
   	int I = Lo + 1;
   	int J = Hi;
   	while (I <= J) {
   	    while (I <= J && Compare (Items[Lo], Items[I]) >= 0) {
   	     	++I;
   	    }
   	    while (I <= J && Compare (Items[Lo], Items[J]) < 0) {
   	     	--J;
   	    }
   	    if (I <= J) {
		/* Swap I and J */
		void* Tmp = Items[I];
		Items[I]  = Items[J];
		Items[J]  = Tmp;
   	     	++I;
   	     	--J;
   	    }
      	}
   	if (J != Lo) {
	    /* Swap J and Lo */
	    void* Tmp = Items[J];
	    Items[J]  = Items[Lo];
	    Items[Lo] = Tmp;
   	}
	if (J > (Hi + Lo) / 2) {
	    CollQuickSort (C, J + 1, Hi, Compare);
	    Hi = J - 1;
	} else {
	    CollQuickSort (C, Lo, J - 1, Compare);
	    Lo = J + 1;
	}
    }
}



void CollSort (Collection* C, int (*Compare) (const void*, const void*))
/* Sort the collection using the given compare function. */
{
    if (C->Count > 1) {
        CollQuickSort (C, 0, C->Count-1, Compare);
    }
}



/*****************************************************************************/
/*                               Segment info                                */
/*****************************************************************************/



static SegInfo* NewSegInfo (const StrBuf* SegName, unsigned Id,
                            cc65_addr Start, cc65_addr Size,
                            const StrBuf* OutputName, unsigned long OutputOffs)
/* Create a new SegInfo struct and return it */
{
    /* Allocate memory */
    SegInfo* S = xmalloc (sizeof (SegInfo) + SB_GetLen (SegName));

    /* Initialize it */
    S->Id         = Id;
    S->Start      = Start;
    S->Size       = Size;
    if (SB_GetLen (OutputName) > 0) {
        /* Output file given */
        S->OutputName = SB_StrDup (OutputName);
        S->OutputOffs = OutputOffs;
    } else {
        /* No output file given */
        S->OutputName = 0;
        S->OutputOffs = 0;
    }
    memcpy (S->SegName, SB_GetConstBuf (SegName), SB_GetLen (SegName) + 1);

    /* Return it */
    return S;
}



static void FreeSegInfo (SegInfo* S)
/* Free a SegInfo struct */
{
    xfree (S->OutputName);
    xfree (S);
}



static int CompareSegInfoByName (const void* L, const void* R)
/* Helper function to sort segment infos in a collection by name */
{
    /* Sort by file name */
    return strcmp (((const SegInfo*) L)->SegName,
                   ((const SegInfo*) R)->SegName);
}



static int CompareSegInfoById (const void* L, const void* R)
/* Helper function to sort segment infos in a collection by id */
{
    if (((const SegInfo*) L)->Id > ((const SegInfo*) R)->Id) {
        return 1;
    } else if (((const SegInfo*) L)->Id < ((const SegInfo*) R)->Id) {
        return -1;
    } else {
        return 0;
    }
}



/*****************************************************************************/
/*                                 Line info                                 */
/*****************************************************************************/



static LineInfo* NewLineInfo (unsigned File, unsigned Seg, cc65_line Line,
                              cc65_addr Start, cc65_addr End)
/* Create a new LineInfo struct and return it */
{
    /* Allocate memory */
    LineInfo* L = xmalloc (sizeof (LineInfo));

    /* Initialize it */
    L->Start    = Start;
    L->End      = End;
    L->Line     = Line;
    L->File.Id  = File;
    L->Seg.Id   = Seg;

    /* Return it */
    return L;
}



static void FreeLineInfo (LineInfo* L)
/* Free a LineInfo struct */
{
    xfree (L);
}



static int CompareLineInfoByAddr (const void* L, const void* R)
/* Helper function to sort line infos in a collection by address */
{
    /* Sort by start of range */
    if (((const LineInfo*) L)->Start > ((const LineInfo*) R)->Start) {
        return 1;
    } else if (((const LineInfo*) L)->Start < ((const LineInfo*) R)->Start) {
        return -1;
    } else {
        return 0;
    }
}



static int CompareLineInfoByLine (const void* L, const void* R)
/* Helper function to sort line infos in a collection by line */
{
    if (((const LineInfo*) L)->Line > ((const LineInfo*) R)->Line) {
        return 1;
    } else if (((const LineInfo*) L)->Line < ((const LineInfo*) R)->Line) {
        return -1;
    } else {
        return 0;
    }
}



/*****************************************************************************/
/*                                 File info                                 */
/*****************************************************************************/



static FileInfo* NewFileInfo (const StrBuf* FileName, unsigned Id,
                              unsigned long Size, unsigned long MTime)
/* Create a new FileInfo struct and return it */
{
    /* Allocate memory */
    FileInfo* F = xmalloc (sizeof (FileInfo) + SB_GetLen (FileName));

    /* Initialize it */
    F->Id    = Id;
    F->Size  = Size;
    F->MTime = MTime;
    F->Start = ~(cc65_addr)0;
    F->End   = 0;
    InitCollection (&F->LineInfoByAddr);
    InitCollection (&F->LineInfoByLine);
    memcpy (F->FileName, SB_GetConstBuf (FileName), SB_GetLen (FileName) + 1);

    /* Return it */
    return F;
}



static void FreeFileInfo (FileInfo* F)
/* Free a FileInfo struct */
{
    unsigned I;

    /* Walk through the collection with line infos and delete them */
    for (I = 0; I < CollCount (&F->LineInfoByAddr); ++I) {
        FreeLineInfo (CollAt (&F->LineInfoByAddr, I));
    }
    DoneCollection (&F->LineInfoByAddr);
    DoneCollection (&F->LineInfoByLine);

    /* Free the file info structure itself */
    xfree (F);
}



static int CompareFileInfoByName (const void* L, const void* R)
/* Helper function to sort file infos in a collection by name */
{
    /* Sort by file name */
    return strcmp (((const FileInfo*) L)->FileName,
                   ((const FileInfo*) R)->FileName);
}



static int CompareFileInfoById (const void* L, const void* R)
/* Helper function to sort file infos in a collection by id */
{
    if (((const FileInfo*) L)->Id > ((const FileInfo*) R)->Id) {
        return 1;
    } else if (((const FileInfo*) L)->Id < ((const FileInfo*) R)->Id) {
        return -1;
    } else {
        return 0;
    }
}



/*****************************************************************************/
/*                                Symbol info                                */
/*****************************************************************************/



static SymInfo* NewSymInfo (const StrBuf* Name, long Val, cc65_symbol_type Type)
/* Create a new SymInfo struct, intialize and return it */
{
    /* Allocate memory */
    SymInfo* S = xmalloc (sizeof (SymInfo) + SB_GetLen (Name));

    /* Initialize it */
    S->Value = Val;
    S->Type  = Type;
    memcpy (S->SymName, SB_GetConstBuf (Name), SB_GetLen (Name) + 1);

    /* Return it */
    return S;
}



static void FreeSymInfo (SymInfo* S)
/* Free a SymInfo struct */
{
    xfree (S);
}



static int CompareSymInfoByName (const void* L, const void* R)
/* Helper function to sort symbol infos in a collection by name */
{
    /* Sort by file name */
    return strcmp (((const SymInfo*) L)->SymName,
                   ((const SymInfo*) R)->SymName);
}



/*****************************************************************************/
/*                                Debug info                                 */
/*****************************************************************************/



static DbgInfo* NewDbgInfo (void)
/* Create a new DbgInfo struct and return it */
{
    /* Allocate memory */
    DbgInfo* Info = xmalloc (sizeof (DbgInfo));

    /* Initialize it */
    InitCollection (&Info->SegInfoByName);
    InitCollection (&Info->SegInfoById);
    InitCollection (&Info->FileInfoByName);
    InitCollection (&Info->FileInfoById);
    InitCollection (&Info->LineInfoByAddr);
    InitCollection (&Info->SymInfoByName);

    /* Return it */
    return Info;
}



static void FreeDbgInfo (DbgInfo* Info)
/* Free a DbgInfo struct */
{
    unsigned I;

    /* Free segment info */
    for (I = 0; I < CollCount (&Info->SegInfoByName); ++I) {
        FreeSegInfo (CollAt (&Info->SegInfoByName, I));
    }
    DoneCollection (&Info->SegInfoByName);
    DoneCollection (&Info->SegInfoById);

    /* Free file info */
    for (I = 0; I < CollCount (&Info->FileInfoByName); ++I) {
        FreeFileInfo (CollAt (&Info->FileInfoByName, I));
    }
    DoneCollection (&Info->FileInfoByName);
    DoneCollection (&Info->FileInfoById);

    /* Free line info */
    DoneCollection (&Info->LineInfoByAddr);

    /* Free symbol info */
    for (I = 0; I < CollCount (&Info->SymInfoByName); ++I) {
        FreeSymInfo (CollAt (&Info->SymInfoByName, I));
    }
    DoneCollection (&Info->SymInfoByName);

    /* Free the structure itself */
    xfree (Info);
}



/*****************************************************************************/
/*                             Helper functions                              */
/*****************************************************************************/



static void CopyLineInfo (cc65_linedata* D, const LineInfo* L)
/* Copy data from a LineInfo struct to the cc65_linedata struct returned to
 * the caller.
 */
{
    D->source_name  = L->File.Info->FileName;
    D->source_size  = L->File.Info->Size;
    D->source_mtime = L->File.Info->MTime;
    D->source_line  = L->Line;
    D->line_start   = L->Start;
    D->line_end     = L->End;
    if (L->Seg.Info->OutputName) {
        D->output_name  = L->Seg.Info->OutputName;
        D->output_offs  = L->Seg.Info->OutputOffs + L->Start - L->Seg.Info->Start;
    } else {
        D->output_name  = 0;
        D->output_offs  = 0;
    }
}



static void ParseError (InputData* D, cc65_error_severity Type, const char* Msg, ...)
/* Call the user supplied parse error function */
{
    va_list             ap;
    int                 MsgSize;
    cc65_parseerror*    E;

    /* Test-format the error message so we know how much space to allocate */
    va_start (ap, Msg);
    MsgSize = vsnprintf (0, 0, Msg, ap);
    va_end (ap);

    /* Allocate memory */
    E = xmalloc (sizeof (*E) + MsgSize);

    /* Write data to E */
    E->type   = Type;
    E->name   = D->FileName;
    E->line   = D->SLine;
    E->column = D->SCol;
    va_start (ap, Msg);
    vsnprintf (E->errormsg, MsgSize+1, Msg, ap);
    va_end (ap);

    /* Call the caller:-) */
    D->Error (E);

    /* Free the data structure */
    xfree (E);

    /* Count errors */
    if (Type == CC65_ERROR) {
        ++D->Errors;
    }
}



static void SkipLine (InputData* D)
/* Error recovery routine. Skip tokens until EOL or EOF is reached */
{
    while (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        NextToken (D);
    }
}



static void UnexpectedToken (InputData* D)
/* Call ParseError with a message about an unexpected input token */
{
    ParseError (D, CC65_ERROR, "Unexpected input token %d", D->Tok);
    SkipLine (D);
}



static void UnknownKeyword (InputData* D)
/* Print a warning about an unknown keyword in the file. Try to do smart
 * recovery, so if later versions of the debug information add additional
 * keywords, this code may be able to at least ignore them.
 */
{
    /* Output a warning */
    ParseError (D, CC65_WARNING, "Unknown keyword \"%s\" - skipping",
                SB_GetConstBuf (&D->SVal));

    /* Skip the identifier */
    NextToken (D);

    /* If an equal sign follows, ignore anything up to the next line end
     * or comma. If a comma or line end follows, we're already done. If
     * we have none of both, we ignore the remainder of the line.
     */
    if (D->Tok == TOK_EQUAL) {
        NextToken (D);
        while (D->Tok != TOK_COMMA && D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
            NextToken (D);
        }
    } else if (D->Tok != TOK_COMMA && D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        SkipLine (D);
    }
}



/*****************************************************************************/
/*                            Scanner and parser                             */
/*****************************************************************************/



static int DigitVal (int C)
/* Return the value for a numeric digit. Return -1 if C is invalid */
{
    if (isdigit (C)) {
	return C - '0';
    } else if (isxdigit (C)) {
	return toupper (C) - 'A' + 10;
    } else {
        return -1;
    }
}



static void NextChar (InputData* D)
/* Read the next character from the input. Count lines and columns */
{
    /* Check if we've encountered EOF before */
    if (D->C >= 0) {
        D->C = fgetc (D->F);
        if (D->C == '\n') {
            ++D->Line;
            D->Col = 0;
        } else {
            ++D->Col;
        }
    }
}



static void NextToken (InputData* D)
/* Read the next token from the input stream */
{
    static const struct KeywordEntry  {
        const char      Keyword[16];
        Token           Tok;
    } KeywordTable[] = {
        { "absolute",   TOK_ABSOLUTE    },
        { "addrsize",   TOK_ADDRSIZE    },
        { "equate",     TOK_EQUATE      },
        { "file",       TOK_FILE        },
        { "id",         TOK_ID          },
        { "label",      TOK_LABEL       },
        { "line",       TOK_LINE        },
        { "long",       TOK_LONG        },
        { "major",      TOK_MAJOR       },
        { "minor",      TOK_MINOR       },
        { "mtime",      TOK_MTIME       },
        { "name",       TOK_NAME        },
        { "outputname", TOK_OUTPUTNAME  },
        { "outputoffs", TOK_OUTPUTOFFS  },
        { "range",      TOK_RANGE       },
        { "ro",         TOK_RO          },
        { "rw",         TOK_RW          },
        { "segment",    TOK_SEGMENT     },
        { "size",       TOK_SIZE        },
        { "start",      TOK_START       },
        { "sym",        TOK_SYM         },
        { "type",       TOK_TYPE        },
        { "value",      TOK_VALUE       },
        { "version",    TOK_VERSION     },
        { "zeropage",   TOK_ZEROPAGE    },
    };


    /* Skip whitespace */
    while (D->C == ' ' || D->C == '\t') {
     	NextChar (D);
    }

    /* Remember the current position as start of the next token */
    D->SLine = D->Line;
    D->SCol  = D->Col;

    /* Identifier? */
    if (D->C == '_' || isalpha (D->C)) {

        const struct KeywordEntry* Entry;

	/* Read the identifier */
        SB_Clear (&D->SVal);
	while (D->C == '_' || isalnum (D->C)) {
            SB_AppendChar (&D->SVal, D->C);
	    NextChar (D);
     	}
       	SB_Terminate (&D->SVal);

        /* Search the identifier in the keyword table */
        Entry = bsearch (SB_GetConstBuf (&D->SVal),
                         KeywordTable,
                         sizeof (KeywordTable) / sizeof (KeywordTable[0]),
                         sizeof (KeywordTable[0]),
                         (int (*)(const void*, const void*)) strcmp);
        if (Entry == 0) {
            D->Tok = TOK_IDENT;
        } else {
            D->Tok = Entry->Tok;
        }
	return;
    }

    /* Number? */
    if (isdigit (D->C)) {
        int Base = 10;
        int Val;
        if (D->C == '0') {
            NextChar (D);
            if (toupper (D->C) == 'X') {
                NextChar (D);
                Base = 16;
            } else {
                Base = 8;
            }
        } else {
            Base = 10;
        }
       	D->IVal = 0;
        while ((Val = DigitVal (D->C)) >= 0 && Val < Base) {
       	    D->IVal = D->IVal * Base + Val;
	    NextChar (D);
	}
	D->Tok = TOK_INTCON;
	return;
    }

    /* Other characters */
    switch (D->C) {

        case '-':
            NextChar (D);
            D->Tok = TOK_MINUS;
            break;

        case '+':
            NextChar (D);
            D->Tok = TOK_PLUS;
            break;

	case ',':
	    NextChar (D);
	    D->Tok = TOK_COMMA;
	    break;

	case '=':
	    NextChar (D);
	    D->Tok = TOK_EQUAL;
	    break;

        case '\"':
            SB_Clear (&D->SVal);
            NextChar (D);
            while (1) {
                if (D->C == '\n' || D->C == EOF) {
                    ParseError (D, CC65_ERROR, "Unterminated string constant");
                    break;
                }
                if (D->C == '\"') {
                    NextChar (D);
                    break;
                }
                SB_AppendChar (&D->SVal, D->C);
                NextChar (D);
            }
            SB_Terminate (&D->SVal);
            D->Tok = TOK_STRCON;
       	    break;

        case '\n':
            NextChar (D);
            D->Tok = TOK_EOL;
            break;

        case EOF:
       	    D->Tok = TOK_EOF;
	    break;

	default:
       	    ParseError (D, CC65_ERROR, "Invalid input character `%c'", D->C);

    }
}



static int TokenFollows (InputData* D, Token Tok, const char* Name)
/* Check for a comma */
{
    if (D->Tok != Tok) {
        ParseError (D, CC65_ERROR, "%s expected", Name);
        SkipLine (D);
        return 0;
    } else {
        return 1;
    }
}



static int IntConstFollows (InputData* D)
/* Check for an integer constant */
{
    return TokenFollows (D, TOK_INTCON, "Integer constant");
}



static int StrConstFollows (InputData* D)
/* Check for a string literal */
{
    return TokenFollows (D, TOK_STRCON, "String literal");
}



static int Consume (InputData* D, Token Tok, const char* Name)
/* Check for a token and consume it. Return true if the token was comsumed,
 * return false otherwise.
 */
{
    if (TokenFollows (D, Tok, Name)) {
        NextToken (D);
        return 1;
    } else {
        return 0;
    }
}



static int ConsumeEqual (InputData* D)
/* Consume an equal sign */
{
    return Consume (D, TOK_EQUAL, "'='");
}



static int ConsumeMinus (InputData* D)
/* Consume a minus sign */
{
    return Consume (D, TOK_MINUS, "'-'");
}



static void ConsumeEOL (InputData* D)
/* Consume an end-of-line token, if we aren't at end-of-file */
{
    if (D->Tok != TOK_EOF) {
        if (D->Tok != TOK_EOL) {
            ParseError (D, CC65_ERROR, "Extra tokens in line");
            SkipLine (D);
        }
        NextToken (D);
    }
}



static void ParseFile (InputData* D)
/* Parse a FILE line */
{
    unsigned      Id;
    unsigned long Size;
    unsigned long MTime;
    StrBuf        FileName = STRBUF_INITIALIZER;
    FileInfo*     F;
    enum {
        ibNone      = 0x00,
        ibId        = 0x01,
        ibFileName  = 0x02,
        ibSize      = 0x04,
        ibMTime     = 0x08,
        ibRequired  = ibId | ibFileName | ibSize | ibMTime,
    } InfoBits = ibNone;

    /* Skip the FILE token */
    NextToken (D);

    /* More stuff follows */
    while (1) {

        Token Tok;

        /* Check for an unknown keyword */
        if (D->Tok == TOK_IDENT) {
            UnknownKeyword (D);
            continue;
        }

        /* Something we know? */
        if (D->Tok != TOK_ID   && D->Tok != TOK_MTIME &&
            D->Tok != TOK_NAME && D->Tok != TOK_SIZE) {
            /* Done */
            break;
        }

        /* Remember the token, skip it, check for equal */
        Tok = D->Tok;
        NextToken (D);
        if (!ConsumeEqual (D)) {
            goto ErrorExit;
        }

        /* Check what the token was */
        switch (Tok) {

            case TOK_ID:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Id = D->IVal;
                InfoBits |= ibId;
                NextToken (D);
                break;

            case TOK_MTIME:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                MTime = D->IVal;
                NextToken (D);
                InfoBits |= ibMTime;
                break;

            case TOK_NAME:
                if (!StrConstFollows (D)) {
                    goto ErrorExit;
                }
                SB_Copy (&FileName, &D->SVal);
                SB_Terminate (&FileName);
                InfoBits |= ibFileName;
                NextToken (D);
                break;

            case TOK_SIZE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Size = D->IVal;
                NextToken (D);
                InfoBits |= ibSize;
                break;

            default:
                /* NOTREACHED */
                UnexpectedToken (D);
                goto ErrorExit;

        }

        /* Comma or done */
        if (D->Tok != TOK_COMMA) {
            break;
        }
        NextToken (D);
    }

    /* Check for end of line */
    if (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        UnexpectedToken (D);
        SkipLine (D);
        goto ErrorExit;
    }

    /* Check for required information */
    if ((InfoBits & ibRequired) != ibRequired) {
        ParseError (D, CC65_ERROR, "Required attributes missing");
        goto ErrorExit;
    }

    /* Create the file info and remember it */
    F = NewFileInfo (&FileName, Id, Size, MTime);
    CollAppend (&D->Info->FileInfoByName, F);

ErrorExit:
    /* Entry point in case of errors */
    SB_Done (&FileName);
    return;
}



static void ParseLine (InputData* D)
/* Parse a LINE line */
{
    unsigned    File;
    unsigned    Segment;
    cc65_line   Line;
    cc65_addr   Start;
    cc65_addr   End;
    LineInfo*   L;
    enum {
        ibNone      = 0x00,
        ibFile      = 0x01,
        ibSegment   = 0x02,
        ibLine      = 0x04,
        ibRange     = 0x08,
        ibRequired  = ibFile | ibSegment | ibLine | ibRange,
    } InfoBits = ibNone;

    /* Skip the LINE token */
    NextToken (D);

    /* More stuff follows */
    while (1) {

        Token Tok;

        /* Check for an unknown keyword */
        if (D->Tok == TOK_IDENT) {
            UnknownKeyword (D);
            continue;
        }

        /* Something we know? */
        if (D->Tok != TOK_FILE  && D->Tok != TOK_LINE     &&
            D->Tok != TOK_RANGE && D->Tok != TOK_SEGMENT) {
            /* Done */
            break;
        }

        /* Remember the token, skip it, check for equal */
        Tok = D->Tok;
        NextToken (D);
        if (!ConsumeEqual (D)) {
            goto ErrorExit;
        }

        /* Check what the token was */
        switch (Tok) {

            case TOK_FILE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                File = D->IVal;
                InfoBits |= ibFile;
                NextToken (D);
                break;

            case TOK_LINE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Line = (cc65_line) D->IVal;
                NextToken (D);
                InfoBits |= ibLine;
                break;

            case TOK_RANGE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Start = (cc65_addr) D->IVal;
                NextToken (D);
                if (!ConsumeMinus (D)) {
                    goto ErrorExit;
                }
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                End = (cc65_addr) D->IVal;
                NextToken (D);
                InfoBits |= ibRange;
                break;

            case TOK_SEGMENT:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Segment = D->IVal;
                InfoBits |= ibSegment;
                NextToken (D);
                break;

            default:
                /* NOTREACHED */
                UnexpectedToken (D);
                goto ErrorExit;

        }

        /* Comma or done */
        if (D->Tok != TOK_COMMA) {
            break;
        }
        NextToken (D);
    }

    /* Check for end of line */
    if (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        UnexpectedToken (D);
        SkipLine (D);
        goto ErrorExit;
    }

    /* Check for required information */
    if ((InfoBits & ibRequired) != ibRequired) {
        ParseError (D, CC65_ERROR, "Required attributes missing");
        goto ErrorExit;
    }

    /* Create the line info and remember it */
    L = NewLineInfo (File, Segment, Line, Start, End);
    CollAppend (&D->Info->LineInfoByAddr, L);

ErrorExit:
    /* Entry point in case of errors */
    return;
}



static void ParseSegment (InputData* D)
/* Parse a SEGMENT line */
{
    unsigned        Id;
    cc65_addr       Start;
    cc65_addr       Size;
    StrBuf          SegName = STRBUF_INITIALIZER;
    StrBuf          OutputName = STRBUF_INITIALIZER;
    unsigned long   OutputOffs;
    SegInfo*        S;
    enum {
        ibNone      = 0x00,
        ibId        = 0x01,
        ibSegName   = 0x02,
        ibStart     = 0x04,
        ibSize      = 0x08,
        ibAddrSize  = 0x10,
        ibType      = 0x20,
        ibOutputName= 0x40,
        ibOutputOffs= 0x80,
        ibRequired  = ibId | ibSegName | ibStart | ibSize | ibAddrSize | ibType,
    } InfoBits = ibNone;

    /* Skip the SEGMENT token */
    NextToken (D);

    /* More stuff follows */
    while (1) {

        Token Tok;

        /* Check for an unknown keyword */
        if (D->Tok == TOK_IDENT) {
            UnknownKeyword (D);
            continue;
        }

        /* Something we know? */
        if (D->Tok != TOK_ADDRSIZE      && D->Tok != TOK_ID         &&
            D->Tok != TOK_NAME          && D->Tok != TOK_OUTPUTNAME &&
            D->Tok != TOK_OUTPUTOFFS    && D->Tok != TOK_SIZE       &&
            D->Tok != TOK_START         && D->Tok != TOK_TYPE) {
            /* Done */
            break;
        }

        /* Remember the token, skip it, check for equal */
        Tok = D->Tok;
        NextToken (D);
        if (!ConsumeEqual (D)) {
            goto ErrorExit;
        }

        /* Check what the token was */
        switch (Tok) {

            case TOK_ADDRSIZE:
                NextToken (D);
                InfoBits |= ibAddrSize;
                break;

            case TOK_ID:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Id = D->IVal;
                InfoBits |= ibId;
                NextToken (D);
                break;

            case TOK_NAME:
                if (!StrConstFollows (D)) {
                    goto ErrorExit;
                }
                SB_Copy (&SegName, &D->SVal);
                SB_Terminate (&SegName);
                InfoBits |= ibSegName;
                NextToken (D);
                break;

            case TOK_OUTPUTNAME:
                if (!StrConstFollows (D)) {
                    goto ErrorExit;
                }
                SB_Copy (&OutputName, &D->SVal);
                SB_Terminate (&OutputName);
                InfoBits |= ibOutputName;
                NextToken (D);
                break;

            case TOK_OUTPUTOFFS:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                OutputOffs = D->IVal;
                NextToken (D);
                InfoBits |= ibOutputOffs;
                break;

            case TOK_SIZE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Size = D->IVal;
                NextToken (D);
                InfoBits |= ibSize;
                break;

            case TOK_START:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Start = (cc65_addr) D->IVal;
                NextToken (D);
                InfoBits |= ibStart;
                break;

            case TOK_TYPE:
                NextToken (D);
                InfoBits |= ibType;
                break;

            default:
                /* NOTREACHED */
                UnexpectedToken (D);
                goto ErrorExit;

        }

        /* Comma or done */
        if (D->Tok != TOK_COMMA) {
            break;
        }
        NextToken (D);
    }

    /* Check for end of line */
    if (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        UnexpectedToken (D);
        SkipLine (D);
        goto ErrorExit;
    }

    /* Check for required and/or matched information */
    if ((InfoBits & ibRequired) != ibRequired) {
        ParseError (D, CC65_ERROR, "Required attributes missing");
        goto ErrorExit;
    }
    InfoBits &= (ibOutputName | ibOutputOffs);
    if (InfoBits != ibNone && InfoBits != (ibOutputName | ibOutputOffs)) {
        ParseError (D, CC65_ERROR,
                    "Attributes \"outputname\" and \"outputoffs\" must be paired");
        goto ErrorExit;
    }

    /* Fix OutputOffs if not given */
    if (InfoBits == ibNone) {
        OutputOffs = 0;
    }

    /* Create the segment info and remember it */
    S = NewSegInfo (&SegName, Id, Start, Size, &OutputName, OutputOffs);
    CollAppend (&D->Info->SegInfoByName, S);

ErrorExit:
    /* Entry point in case of errors */
    SB_Done (&SegName);
    SB_Done (&OutputName);
    return;
}



static void ParseSym (InputData* D)
/* Parse a SYM line */
{
    cc65_symbol_type    Type;
    long                Value;
    StrBuf              SymName = STRBUF_INITIALIZER;
    SymInfo*            S;
    enum {
        ibNone          = 0x00,
        ibSymName       = 0x01,
        ibValue         = 0x02,
        ibAddrSize      = 0x04,
        ibType          = 0x08,
        ibRequired      = ibSymName | ibValue | ibAddrSize | ibType,
    } InfoBits = ibNone;

    /* Skip the SYM token */
    NextToken (D);

    /* More stuff follows */
    while (1) {

        Token Tok;

        /* Check for an unknown keyword */
        if (D->Tok == TOK_IDENT) {
            UnknownKeyword (D);
            continue;
        }

        /* Something we know? */
        if (D->Tok != TOK_ADDRSIZE      && D->Tok != TOK_NAME   &&
            D->Tok != TOK_TYPE          && D->Tok != TOK_VALUE) {
            /* Done */
            break;
        }

        /* Remember the token, skip it, check for equal */
        Tok = D->Tok;
        NextToken (D);
        if (!ConsumeEqual (D)) {
            goto ErrorExit;
        }

        /* Check what the token was */
        switch (Tok) {

            case TOK_ADDRSIZE:
                NextToken (D);
                InfoBits |= ibAddrSize;
                break;

            case TOK_NAME:
                if (!StrConstFollows (D)) {
                    goto ErrorExit;
                }
                SB_Copy (&SymName, &D->SVal);
                SB_Terminate (&SymName);
                InfoBits |= ibSymName;
                NextToken (D);
                break;

            case TOK_TYPE:
                switch (D->Tok) {
                    case TOK_EQUATE:
                        Type = CC65_SYM_EQUATE;
                        break;
                    case TOK_LABEL:
                        Type = CC65_SYM_LABEL;
                        break;
                    default:
                        ParseError (D, CC65_ERROR,
                                    "Unknown value for attribute \"type\"");
                        SkipLine (D);
                        goto ErrorExit;
                }
                NextToken (D);
                InfoBits |= ibType;
                break;

            case TOK_VALUE:
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                Value = D->IVal;
                InfoBits |= ibValue;
                NextToken (D);
                break;

            default:
                /* NOTREACHED */
                UnexpectedToken (D);
                goto ErrorExit;

        }

        /* Comma or done */
        if (D->Tok != TOK_COMMA) {
            break;
        }
        NextToken (D);
    }

    /* Check for end of line */
    if (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {
        UnexpectedToken (D);
        SkipLine (D);
        goto ErrorExit;
    }

    /* Check for required and/or matched information */
    if ((InfoBits & ibRequired) != ibRequired) {
        ParseError (D, CC65_ERROR, "Required attributes missing");
        goto ErrorExit;
    }

    /* Create the symbol info and remember it */
    S = NewSymInfo (&SymName, Value, Type);
    CollAppend (&D->Info->SymInfoByName, S);

ErrorExit:
    /* Entry point in case of errors */
    SB_Done (&SymName);
    return;
}



static void ParseVersion (InputData* D)
/* Parse a VERSION line */
{
    enum {
        ibNone      = 0x00,
        ibMajor     = 0x01,
        ibMinor     = 0x02,
        ibRequired  = ibMajor | ibMinor,
    } InfoBits = ibNone;

    /* Skip the VERSION token */
    NextToken (D);

    /* More stuff follows */
    while (D->Tok != TOK_EOL && D->Tok != TOK_EOF) {

        switch (D->Tok) {

            case TOK_MAJOR:
                NextToken (D);
                if (!ConsumeEqual (D)) {
                    goto ErrorExit;
                }
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                D->MajorVersion = D->IVal;
                NextToken (D);
                InfoBits |= ibMajor;
                break;

            case TOK_MINOR:
                NextToken (D);
                if (!ConsumeEqual (D)) {
                    goto ErrorExit;
                }
                if (!IntConstFollows (D)) {
                    goto ErrorExit;
                }
                D->MinorVersion = D->IVal;
                NextToken (D);
                InfoBits |= ibMinor;
                break;

            case TOK_IDENT:
                /* Try to skip unknown keywords that may have been added by
                 * a later version.
                 */
                UnknownKeyword (D);
                break;

            default:
                UnexpectedToken (D);
                SkipLine (D);
                goto ErrorExit;
        }

        /* Comma follows before next attribute */
        if (D->Tok == TOK_COMMA) {
            NextToken (D);
        } else if (D->Tok == TOK_EOL || D->Tok == TOK_EOF) {
            break;
        } else {
            UnexpectedToken (D);
            goto ErrorExit;
        }
    }

    /* Check for required information */
    if ((InfoBits & ibRequired) != ibRequired) {
        ParseError (D, CC65_ERROR, "Required attributes missing");
        goto ErrorExit;
    }

ErrorExit:
    /* Entry point in case of errors */
    return;
}



/*****************************************************************************/
/*                              Data processing                              */
/*****************************************************************************/



static SegInfo* FindSegInfoById (InputData* D, unsigned Id)
/* Find the SegInfo with a given Id */
{
    /* Get a pointer to the segment info collection */
    Collection* SegInfos = &D->Info->SegInfoById;

    /* Do a binary search */
    int Lo = 0;
    int Hi = (int) CollCount (SegInfos) - 1;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        SegInfo* CurItem = CollAt (SegInfos, Cur);

        /* Found? */
        if (Id > CurItem->Id) {
            Lo = Cur + 1;
        } else if (Id < CurItem->Id) {
            Hi = Cur - 1;
        } else {
            /* Found! */
            return CurItem;
        }
    }

    /* Not found */
    return 0;
}



static FileInfo* FindFileInfoByName (Collection* FileInfos, const char* FileName)
/* Find the FileInfo for a given file name */
{
    /* Do a binary search */
    int Lo = 0;
    int Hi = (int) CollCount (FileInfos) - 1;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        FileInfo* CurItem = CollAt (FileInfos, Cur);

        /* Compare */
        int Res = strcmp (CurItem->FileName, FileName);

        /* Found? */
        if (Res < 0) {
            Lo = Cur + 1;
        } else if (Res > 0) {
            Hi = Cur - 1;
        } else {
            /* Found! */
            return CurItem;
        }
    }

    /* Not found */
    return 0;
}



static FileInfo* FindFileInfoById (Collection* FileInfos, unsigned Id)
/* Find the FileInfo with a given Id */
{
    /* Do a binary search */
    int Lo = 0;
    int Hi = (int) CollCount (FileInfos) - 1;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        FileInfo* CurItem = CollAt (FileInfos, Cur);

        /* Found? */
        if (Id > CurItem->Id) {
            Lo = Cur + 1;
        } else if (Id < CurItem->Id) {
            Hi = Cur - 1;
        } else {
            /* Found! */
            return CurItem;
        }
    }

    /* Not found */
    return 0;
}



static void ProcessSegInfo (InputData* D)
/* Postprocess segment infos */
{
    unsigned I;

    /* Get pointers to the segment info collections */
    Collection* SegInfoByName = &D->Info->SegInfoByName;
    Collection* SegInfoById   = &D->Info->SegInfoById;

    /* Sort the segment infos by name */
    CollSort (SegInfoByName, CompareSegInfoByName);

    /* Copy all items over to the collection that will get sorted by id */
    for (I = 0; I < CollCount (SegInfoByName); ++I) {
        CollAppend (SegInfoById, CollAt (SegInfoByName, I));
    }

    /* Sort this collection */
    CollSort (SegInfoById, CompareSegInfoById);
}



static void ProcessFileInfo (InputData* D)
/* Postprocess file infos */
{
    /* Get pointers to the file info collections */
    Collection* FileInfoByName = &D->Info->FileInfoByName;
    Collection* FileInfoById   = &D->Info->FileInfoById;

    /* First, sort the file infos, so we can check for duplicates and do
     * binary search.
     */
    CollSort (FileInfoByName, CompareFileInfoByName);

    /* Cannot work on an empty collection */
    if (CollCount (FileInfoByName) > 0) {

        /* Walk through the file infos sorted by name and check for duplicates.
         * If we find some, warn and remove them, so the file infos are unique
         * after that step.
         */
        FileInfo* F = CollAt (FileInfoByName, 0);
        unsigned I = 1;
        while (I < CollCount (FileInfoByName)) {
            FileInfo* Next = CollAt (FileInfoByName, I);
            if (strcmp (F->FileName, Next->FileName) == 0) {
                /* Warn only if time stamp and/or size is different */
                if (F->Size != Next->Size || F->MTime != Next->MTime) {
                    ParseError (D,
                                CC65_WARNING,
                                "Duplicate file entry for \"%s\"",
                                F->FileName);
                }
                /* Remove the duplicate entry */
                FreeFileInfo (Next);
                CollDelete (FileInfoByName, I);
            } else {
                /* This one is ok, check the next entry */
                F = Next;
                ++I;
            }
        }

        /* Copy the file infos to another collection that will be sorted by id */
        for (I = 0; I < CollCount (FileInfoByName); ++I) {
            CollAppend (FileInfoById, CollAt (FileInfoByName, I));
        }

        /* Sort this collection */
        CollSort (FileInfoById, CompareFileInfoById);
    }
}



static void ProcessLineInfo (InputData* D)
/* Postprocess line infos */
{
    /* Get pointers to the collections */
    Collection* LineInfos = &D->Info->LineInfoByAddr;
    Collection* FileInfos = &D->Info->FileInfoByName;

    /* Walk over the line infos and replace the id numbers of file and segment
     * with pointers to the actual structs. Add the line info to each file
     * where it is defined.
     */
    unsigned I = 0;
    FileInfo* LastFileInfo = 0;
    SegInfo*  LastSegInfo  = 0;
    while (I < CollCount (LineInfos)) {

        FileInfo* F;
        SegInfo*  S;

        /* Get LineInfo struct */
        LineInfo* L = CollAt (LineInfos, I);

        /* Find the FileInfo that corresponds to Id. We cache the last file
         * info in LastFileInfo to speedup searching.
         */
        if (LastFileInfo && LastFileInfo->Id == L->File.Id) {
            F = LastFileInfo;
        } else {
            F = FindFileInfoById (&D->Info->FileInfoById, L->File.Id);

            /* If we have no corresponding file info, print a warning and
             * remove the line info.
             */
            if (F == 0) {
                ParseError (D,
                            CC65_ERROR,
                            "No file info for file with id %u",
                            L->File.Id);
                FreeLineInfo (L);
                CollDelete (LineInfos, I);
                continue;
            }

            /* Otherwise remember it for later */
            LastFileInfo = F;
        }

        /* Replace the file id by a pointer to the file info */
        L->File.Info = F;

        /* Find the SegInfo that corresponds to Id. We cache the last file
         * info in LastSegInfo to speedup searching.
         */
        if (LastSegInfo && LastSegInfo->Id == L->Seg.Id) {
            S = LastSegInfo;
        } else {
            S = FindSegInfoById (D, L->Seg.Id);

            /* If we have no corresponding segment info, print a warning and
             * remove the line info.
             */
            if (S == 0) {
                ParseError (D,
                            CC65_ERROR,
                            "No segment info for segment with id %u",
                            L->Seg.Id);
                FreeLineInfo (L);
                CollDelete (LineInfos, I);
                continue;
            }

            /* Otherwise remember it for later */
            LastSegInfo = S;
        }

        /* Replace the segment id by a pointer to the segment info */
        L->Seg.Info = S;

        /* Add this line info to the file where it is defined */
        CollAppend (&F->LineInfoByAddr, L);
        CollAppend (&F->LineInfoByLine, L);

        /* Next one */
        ++I;
    }

    /* Walk over all files and sort the line infos for each file so we can
     * do a binary search later.
     */
    for (I = 0; I < CollCount (FileInfos); ++I) {

        /* Get a pointer to this file info */
        FileInfo* F = CollAt (FileInfos, I);

        /* Sort the line infos for this file */
        CollSort (&F->LineInfoByAddr, CompareLineInfoByAddr);
        CollSort (&F->LineInfoByLine, CompareLineInfoByLine);

        /* If there are line info entries, place the first and last address
         * of into the FileInfo struct itself, so we can rule out a FileInfo
         * quickly when mapping an address to a line info.
         */
        if (CollCount (&F->LineInfoByAddr) > 0) {
            F->Start = ((const LineInfo*) CollFirst (&F->LineInfoByAddr))->Start;
            F->End   = ((const LineInfo*) CollLast (&F->LineInfoByAddr))->End;
        }
    }

    /* Sort the collection with all line infos by address */
    CollSort (LineInfos, CompareLineInfoByAddr);
}



static LineInfo* FindLineInfoByAddr (FileInfo* F, cc65_addr Addr)
/* Find the LineInfo for a given address */
{
    Collection* LineInfoByAddr;
    int         Hi;
    int         Lo;


    /* Each file info contains the first and last address for which line
     * info is available, so we can rule out non matching ones quickly.
     */
    if (Addr < F->Start || Addr > F->End) {
        return 0;
    }

    /* Get a pointer to the line info collection for this file */
    LineInfoByAddr = &F->LineInfoByAddr;

    /* Do a binary search */
    Lo = 0;
    Hi = (int) CollCount (LineInfoByAddr) - 1;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        LineInfo* CurItem = CollAt (LineInfoByAddr, Cur);

        /* Found? */
        if (Addr < CurItem->Start) {
            Hi = Cur - 1;
        } else if (Addr > CurItem->End) {
            Lo = Cur + 1;
        } else {
            /* Found! */
            return CurItem;
        }
    }

    /* Not found */
    return 0;
}



static LineInfo* FindLineInfoByLine (FileInfo* F, cc65_line Line)
/* Find the LineInfo for a given line number */
{
    int         Hi;
    int         Lo;


    /* Get a pointer to the line info collection for this file */
    Collection* LineInfoByLine = &F->LineInfoByLine;

    /* Do a binary search */
    Lo = 0;
    Hi = (int) CollCount (LineInfoByLine) - 1;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        LineInfo* CurItem = CollAt (LineInfoByLine, Cur);

        /* Found? */
        if (Line < CurItem->Line) {
            Hi = Cur - 1;
        } else if (Line > CurItem->Line) {
            Lo = Cur + 1;
        } else {
            /* Found! */
            return CurItem;
        }
    }

    /* Not found */
    return 0;
}



static void ProcessSymInfo (InputData* D)
/* Postprocess symbol infos */
{
    /* Get pointers to the symbol info collections */
    Collection* SymInfoByName = &D->Info->SymInfoByName;

    /* Sort the symbol infos by name */
    CollSort (SymInfoByName, CompareSymInfoByName);
}



static int FindSymInfoByName (Collection* SymInfos, const char* SymName, int* Index)
/* Find the SymInfo for a given file name. The function returns true if the
 * name was found. In this case, Index contains the index of the first item
 * that matches. If the item wasn't found, the function returns false and
 * Index contains the insert position for SymName.
 */
{
    /* Do a binary search */
    int Lo = 0;
    int Hi = (int) CollCount (SymInfos) - 1;
    int Found = 0;
    while (Lo <= Hi) {

        /* Mid of range */
        int Cur = (Lo + Hi) / 2;

        /* Get item */
        SymInfo* CurItem = CollAt (SymInfos, Cur);

        /* Compare */
        int Res = strcmp (CurItem->SymName, SymName);

        /* Found? */
        if (Res < 0) {
            Lo = Cur + 1;
        } else {
            Hi = Cur - 1;
            /* Since we may have duplicates, repeat the search until we've
             * the first item that has a match.
             */
            if (Res == 0) {
                Found = 1;
            }
        }
    }

    /* Pass back the index. This is also the insert position */
    *Index = Lo;
    return Found;
}



/*****************************************************************************/
/*     	      	       	      	     Code				     */
/*****************************************************************************/



cc65_dbginfo cc65_read_dbginfo (const char* FileName, cc65_errorfunc ErrFunc)
/* Parse the debug info file with the given name. On success, the function
 * will return a pointer to an opaque cc65_dbginfo structure, that must be
 * passed to the other functions in this module to retrieve information.
 * errorfunc is called in case of warnings and errors. If the file cannot be
 * read successfully, NULL is returned.
 */
{
    /* Data structure used to control scanning and parsing */
    InputData D = {
        0,                      /* Name of input file */
        1,                      /* Line number */
        0,                      /* Input file */
        0,                      /* Line at start of current token */
        0,                      /* Column at start of current token */
        0,                      /* Number of errors */
        0,                      /* Input file */
        ' ',                    /* Input character */
        TOK_INVALID,            /* Input token */
        0,                      /* Integer constant */
        STRBUF_INITIALIZER,     /* String constant */
        0,                      /* Function called in case of errors */
        0,                      /* Major version number */
        0,                      /* Minor version number */
        0,                      /* Pointer to debug info */
    };
    D.FileName = FileName;
    D.Error    = ErrFunc;

    /* Open the input file */
    D.F = fopen (D.FileName, "r");
    if (D.F == 0) {
        /* Cannot open */
        ParseError (&D, CC65_ERROR,
                    "Cannot open input file \"%s\": %s",
                     D.FileName, strerror (errno));
        return 0;
    }

    /* Create a new debug info struct */
    D.Info = NewDbgInfo ();

    /* Prime the pump */
    NextToken (&D);

    /* The first line in the file must specify version information */
    if (D.Tok != TOK_VERSION) {
        ParseError (&D, CC65_ERROR,
                    "\"version\" keyword missing in first line - this is not "
                    "a valid debug info file");
    } else {

        /* Parse the version directive and check the version */
        ParseVersion (&D);
        if (D.MajorVersion > VER_MAJOR) {
            ParseError (&D, CC65_WARNING,
                        "The format of this debug info file is newer than what we "
                        "know. Will proceed but probably fail. Version found = %u, "
                        "version supported = %u",
                        D.MajorVersion, VER_MAJOR);
        }
        ConsumeEOL (&D);

        /* Parse lines */
        while (D.Tok != TOK_EOF) {

            switch (D.Tok) {

                case TOK_FILE:
                    ParseFile (&D);
                    break;

                case TOK_LINE:
                    ParseLine (&D);
                    break;

                case TOK_SEGMENT:
                    ParseSegment (&D);
                    break;

                case TOK_SYM:
                    ParseSym (&D);
                    break;

                case TOK_IDENT:
                    /* Output a warning, then skip the line with the unknown
                     * keyword that may have been added by a later version.
                     */
                    ParseError (&D, CC65_WARNING,
                                "Unknown keyword \"%s\" - skipping",
                                SB_GetConstBuf (&D.SVal));

                    SkipLine (&D);
                    break;

                default:
                    UnexpectedToken (&D);

            }

            /* EOL or EOF must follow */
            ConsumeEOL (&D);
        }
    }

    /* Close the file */
    fclose (D.F);

    /* Free memory allocated for SVal */
    SB_Done (&D.SVal);

    /* In case of errors, delete the debug info already allocated and
     * return NULL
     */
    if (D.Errors > 0) {
        /* Free allocated stuff */
        unsigned I;
        for (I = 0; I < CollCount (&D.Info->LineInfoByAddr); ++I) {
            FreeLineInfo (CollAt (&D.Info->LineInfoByAddr, I));
        }
        DoneCollection (&D.Info->LineInfoByAddr);
        FreeDbgInfo (D.Info);
        return 0;
    }

    /* We do now have all the information from the input file. Do
     * postprocessing.
     */
    ProcessSegInfo (&D);
    ProcessFileInfo (&D);
    ProcessLineInfo (&D);
    ProcessSymInfo (&D);

    /* Return the debug info struct that was created */
    return D.Info;
}



void cc65_free_dbginfo (cc65_dbginfo Handle)
/* Free debug information read from a file */
{
    if (Handle) {
        FreeDbgInfo (Handle);
    }
}



cc65_lineinfo* cc65_lineinfo_byaddr (cc65_dbginfo Handle, unsigned long Addr)
/* Return line information for the given address. The function returns 0
 * if no line information was found.
 */
{
    unsigned        I;
    Collection*     FileInfoByName;
    cc65_lineinfo*  D = 0;

    /* We will place a list of line infos in a collection */
    Collection LineInfos = COLLECTION_INITIALIZER;

    /* Check the parameter */
    assert (Handle != 0);

    /* Walk over all files and search for matching line infos */
    FileInfoByName = &((DbgInfo*) Handle)->FileInfoByName;
    for (I = 0; I < CollCount (FileInfoByName); ++I) {
        /* Check if the file contains line info for this address */
        LineInfo* L = FindLineInfoByAddr (CollAt (FileInfoByName, I), Addr);
        if (L != 0) {
            CollAppend (&LineInfos, L);
        }
    }

    /* Do we have line infos? */
    if (CollCount (&LineInfos) > 0) {

        /* Prepare the struct we will return to the caller */
        D = xmalloc (sizeof (*D) +
                     (CollCount (&LineInfos) - 1) * sizeof (D->data[0]));
        D->count = CollCount (&LineInfos);
        for (I = 0; I < D->count; ++I) {
            /* Copy data */
            CopyLineInfo (D->data + I, CollAt (&LineInfos, I));
        }
    }

    /* Free the line info collection */
    DoneCollection (&LineInfos);

    /* Return the struct we've created */
    return D;
}



cc65_lineinfo* cc65_lineinfo_byname (cc65_dbginfo Handle, const char* FileName,
                                     cc65_line Line)
/* Return line information for a file/line number combination. The function
 * returns NULL if no line information was found.
 */
{
    DbgInfo*        Info;
    FileInfo*       F;
    LineInfo*       L;
    cc65_lineinfo*  D;

    /* Check the parameter */
    assert (Handle != 0);

    /* The handle is actually a pointer to a debug info struct */
    Info = (DbgInfo*) Handle;

    /* Get the file info */
    F = FindFileInfoByName (&Info->FileInfoByName, FileName);
    if (F == 0) {
        /* File not found */
        return 0;
    }

    /* Search in the file for the given line */
    L = FindLineInfoByLine (F, Line);
    if (L == 0) {
        /* Line not found */
        return 0;
    }

    /* Prepare the struct we will return to the caller */
    D = xmalloc (sizeof (*D));
    D->count = 1;

    /* Copy data */
    CopyLineInfo (D->data, L);

    /* Return the allocated struct */
    return D;
}



void cc65_free_lineinfo (cc65_dbginfo Handle, cc65_lineinfo* Info)
/* Free line info returned by one of the other functions */
{
    /* Just for completeness, check the handle */
    assert (Handle != 0);

    /* Just free the memory */
    xfree (Info);
}



cc65_sourceinfo* cc65_get_sourcelist (cc65_dbginfo Handle)
/* Return a list of all source files */
{
    DbgInfo*            Info;
    Collection*         FileInfoByName;
    cc65_sourceinfo*    D;
    unsigned            I;

    /* Check the parameter */
    assert (Handle != 0);

    /* The handle is actually a pointer to a debug info struct */
    Info = (DbgInfo*) Handle;

    /* Get a pointer to the file list */
    FileInfoByName = &Info->FileInfoByName;

    /* Allocate memory for the data structure returned to the caller */
    D = xmalloc (sizeof (*D) - sizeof (D->data[0]) +
                 CollCount (FileInfoByName) * sizeof (D->data[0]));

    /* Fill in the data */
    D->count = CollCount (FileInfoByName);
    for (I = 0; I < CollCount (FileInfoByName); ++I) {

        /* Get this item */
        const FileInfo* F = CollAt (FileInfoByName, I);

        /* Copy the data */
        D->data[I].source_name  = F->FileName;
        D->data[I].source_size  = F->Size;
        D->data[I].source_mtime = F->MTime;
    }

    /* Return the result */
    return D;
}



void cc65_free_sourceinfo (cc65_dbginfo Handle, cc65_sourceinfo* Info)
/* Free a source info record */
{
    /* Just for completeness, check the handle */
    assert (Handle != 0);

    /* Free the memory */
    xfree (Info);
}



cc65_segmentinfo* cc65_get_segmentlist (cc65_dbginfo Handle)
/* Return a list of all segments referenced in the debug information */
{
    DbgInfo*            Info;
    Collection*         SegInfoByName;
    cc65_segmentinfo*   D;
    unsigned            I;

    /* Check the parameter */
    assert (Handle != 0);

    /* The handle is actually a pointer to a debug info struct */
    Info = (DbgInfo*) Handle;

    /* Get a pointer to the segment list */
    SegInfoByName = &Info->SegInfoByName;

    /* Allocate memory for the data structure returned to the caller */
    D = xmalloc (sizeof (*D) - sizeof (D->data[0]) +
                 CollCount (SegInfoByName) * sizeof (D->data[0]));

    /* Fill in the data */
    D->count = CollCount (SegInfoByName);
    for (I = 0; I < CollCount (SegInfoByName); ++I) {

        /* Get this item */
        const SegInfo* S = CollAt (SegInfoByName, I);

        /* Copy the data */
        D->data[I].segment_name  = S->SegName;
        D->data[I].segment_start = S->Start;
        D->data[I].segment_size  = S->Size;
        D->data[I].output_name   = S->OutputName;
        D->data[I].output_offs   = S->OutputOffs;
    }

    /* Return the result */
    return D;
}



void cc65_free_segmentinfo (cc65_dbginfo Handle, cc65_segmentinfo* Info)
/* Free a segment info record */
{
    /* Just for completeness, check the handle */
    assert (Handle != 0);

    /* Free the memory */
    xfree (Info);
}



cc65_symbolinfo* cc65_symbol_byname (cc65_dbginfo Handle, const char* Name)
/* Return a list of symbols with a given name. The function returns NULL if
 * no symbol with this name was found.
 */
{
    DbgInfo*            Info;
    Collection*         SymInfoByName;
    cc65_symbolinfo*    D;
    unsigned            I;
    int                 Index;
    unsigned            Count;

    /* Check the parameter */
    assert (Handle != 0);

    /* The handle is actually a pointer to a debug info struct */
    Info = (DbgInfo*) Handle;

    /* Get a pointer to the symbol list */
    SymInfoByName = &Info->SymInfoByName;

    /* Search for the symbol */
    if (!FindSymInfoByName (SymInfoByName, Name, &Index)) {
        /* Not found */
        return 0;
    }

    /* Index contains the position. Count how many symbols with this name
     * we have. Skip the first one, since we have at least one.
     */
    Count = 1;
    while ((unsigned) Index + Count < CollCount (SymInfoByName)) {
        const SymInfo* S = CollAt (SymInfoByName, (unsigned) Index + Count);
        if (strcmp (S->SymName, Name) != 0) {
            break;
        }
        ++Count;
    }

    /* Allocate memory for the data structure returned to the caller */
    D = xmalloc (sizeof (*D) + (Count - 1) * sizeof (D->data[0]));

    /* Fill in the data */
    D->count = Count;
    while (Count--) {

        /* Get this item */
        const SymInfo* S = CollAt (SymInfoByName, Index);

        /* Copy the data */
        D->data[I].symbol_name  = S->SymName;
        D->data[I].symbol_type  = S->Type;
        D->data[I].symbol_value = S->Value;
    }

    /* Return the result */
    return D;
}



void cc65_free_symbolinfo (cc65_dbginfo Handle, cc65_symbolinfo* Info)
/* Free a symbol info record */
{
    /* Just for completeness, check the handle */
    assert (Handle != 0);

    /* Free the memory */
    xfree (Info);
}




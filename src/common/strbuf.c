/*****************************************************************************/
/*                                                                           */
/*		     		   strbuf.c                                  */
/*                                                                           */
/*			 Variable sized string buffers                       */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2001-2003 Ullrich von Bassewitz                                       */
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



#include <string.h>

/* common */
#include "xmalloc.h"
#include "strbuf.h"



/*****************************************************************************/
/*		     		     Data                                    */
/*****************************************************************************/



/* An empty string buf */
const StrBuf EmptyStrBuf = STATIC_STRBUF_INITIALIZER;



/*****************************************************************************/
/*	      			     Code	   	     		     */
/*****************************************************************************/



StrBuf* InitStrBuf (StrBuf* B)
/* Initialize a string buffer */
{
    B->Allocated = 0;
    B->Len       = 0;
    B->Index     = 0;
    B->Buf       = 0;
    return B;
}



void DoneStrBuf (StrBuf* B)
/* Free the data of a string buffer (but not the struct itself) */
{
    xfree (B->Buf);
}



StrBuf* NewStrBuf (void)
/* Allocate, initialize and return a new StrBuf */
{
    /* Allocate a new string buffer */
    StrBuf* B = xmalloc (sizeof (StrBuf));

    /* Initialize the struct... */
    InitStrBuf (B);

    /* ...and return it */
    return B;
}



void FreeStrBuf (StrBuf* B)
/* Free a string buffer */
{
    DoneStrBuf (B);
    xfree (B);
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

    /* Reallocate the buffer */
    B->Buf       = xrealloc (B->Buf, NewAllocated);
    B->Allocated = NewAllocated;
}



#if !defined(HAVE_INLINE)
char SB_At (const StrBuf* B, unsigned Index)
/* Get a character from the buffer */
{
    PRECONDITION (Index < B->Len);
    return B->Buf[Index];
}
#endif



void SB_Drop (StrBuf* B, unsigned Count)
/* Drop characters from the end of the string. */
{
    PRECONDITION (Count <= B->Len);
    B->Len -= Count;
    if (B->Index > B->Len) {
        B->Index = B->Len;
    }
}



void SB_Terminate (StrBuf* B)
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



void SB_CopyBuf (StrBuf* Target, const char* Buf, unsigned Size)
/* Copy Buf to Target, discarding the old contents of Target */
{
    if (Target->Allocated < Size) {
	SB_Realloc (Target, Size);
    }
    memcpy (Target->Buf, Buf, Size);
    Target->Len = Size;
}



#if !defined(HAVE_INLINE)
void SB_CopyStr (StrBuf* Target, const char* S)
/* Copy S to Target, discarding the old contents of Target */
{
    SB_CopyBuf (Target, S, strlen (S));
}
#endif



#if !defined(HAVE_INLINE)
void SB_Copy (StrBuf* Target, const StrBuf* Source)
/* Copy Source to Target, discarding the old contents of Target */
{
    SB_CopyBuf (Target, Source->Buf, Source->Len);
    Target->Index = Source->Index;
}
#endif



void SB_AppendChar (StrBuf* B, char C)
/* Append a character to a string buffer */
{
    unsigned NewLen = B->Len + 1;
    if (NewLen > B->Allocated) {
	SB_Realloc (B, NewLen);
    }
    B->Buf[B->Len] = C;
    B->Len = NewLen;
}



void SB_AppendBuf (StrBuf* B, const char* S, unsigned Size)
/* Append a character buffer to the end of the string buffer */
{
    unsigned NewLen = B->Len + Size;
    if (NewLen > B->Allocated) {
	SB_Realloc (B, NewLen);
    }
    memcpy (B->Buf + B->Len, S, Size);
    B->Len = NewLen;
}



#if !defined(HAVE_INLINE)
void SB_AppendStr (StrBuf* B, const char* S)
/* Append a string to the end of the string buffer */
{
    SB_AppendBuf (B, S, strlen (S));
}
#endif



#if !defined(HAVE_INLINE)
void SB_Append (StrBuf* Target, const StrBuf* Source)
/* Append the contents of Source to Target */
{
    SB_AppendBuf (Target, Source->Buf, Source->Len);
}
#endif



#if !defined(HAVE_INLINE)
void SB_Cut (StrBuf* B, unsigned Len)
/* Cut the contents of B at the given length. If the current length of the
 * buffer is smaller than Len, nothing will happen.
 */
{
    if (Len < B->Len) {
       	B->Len = Len;
    }
}
#endif



void SB_Slice (StrBuf* Target, const StrBuf* Source, unsigned Start, unsigned Len)
/* Copy a slice from Source into Target. The current contents of Target are
 * destroyed. If Start is greater than the length of Source, or if Len
 * characters aren't available, the result will be a buffer with less than Len
 * bytes.
 */
{
    /* Calculate the length of the resulting buffer */
    if (Start >= Source->Len) {
       	/* Target will be empty */
	SB_Clear (Target);
	return;
    } else if (Start + Len > Source->Len) {
       	Len = (Start + Len) - Source->Len;
    }

    /* Make sure we have enough room in the target string buffer */
    if (Len > Target->Allocated) {
	SB_Realloc (Target, Len);
    }

    /* Copy the slice */
    memcpy (Target->Buf, Source->Buf + Start, Len);
    Target->Len = Len;
}



void SB_Move (StrBuf* Target, StrBuf* Source)
/* Move the complete contents of Source to target. This will delete the old
 * contents of Target, and Source will be empty after the call.
 */                                          
{
    /* Free the target string */
    if (Target->Buf) {
        xfree (Target->Buf);
    }

    /* Move all data from Source to Target */
    *Target = *Source;

    /* Clear Source */
    InitStrBuf (Source);
}




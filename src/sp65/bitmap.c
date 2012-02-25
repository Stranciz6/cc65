/*****************************************************************************/
/*                                                                           */
/*                                 bitmap.c                                  */
/*                                                                           */
/*         Bitmap definition for the sp65 sprite and bitmap utility          */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2012,      Ullrich von Bassewitz                                      */
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



/* common */
#include "check.h"
#include "xmalloc.h"

/* sp65 */
#include "bitmap.h"
#include "error.h"



/*****************************************************************************/
/*                                   Code                                    */
/*****************************************************************************/



Bitmap* NewBitmap (unsigned Width, unsigned Height)
/* Create a new bitmap. The type is set to unknown and the palette to NULL */
{
    Bitmap* B;

    /* Calculate the size of the bitmap in pixels */
    unsigned long Size = (unsigned long) Width * Height;

    /* Some safety checks */
    PRECONDITION (Size > 0 && Size <= BM_MAX_SIZE);

    /* Allocate memory */
    B = xmalloc (sizeof (*B) + (Size - 1) * sizeof (B->Data[0]));

    /* Initialize the data */
    B->Type     = bmUnknown;
    SB_Init (&B->Name);
    B->Width    = Width;
    B->Height   = Height;
    B->Pal      = 0;

    /* Return the bitmap */
    return B;
}



void FreeBitmap (Bitmap* B)
/* Free a dynamically allocated bitmap */
{
    /* Free the palette */
    xfree (B->Pal);
}



int ValidBitmapSize (unsigned Width, unsigned Height)
/* Return true if this is a valid size for a bitmap */
{
    /* Calculate the size of the bitmap in pixels */
    unsigned long Size = (unsigned long) Width * Height;

    /* Check the size */
    return (Size > 0 && Size <= BM_MAX_SIZE);
}



Color GetPixelColor (const Bitmap* B, unsigned X, unsigned Y)
/* Get the color for a given pixel. For indexed bitmaps, the palette entry
 * is returned.
 */
{
    /* Get the pixel at the given coordinates */
    Pixel P = GetPixel (B, X, Y);

    /* If the bitmap has a palette, return the color from the palette. For
     * simplicity, we will only check the palette, not the type.
     */
    if (B->Pal) {
        if (P.Index >= B->Pal->Count) {
            /* Palette index is invalid */
            Error ("Invalid palette index %u at position %u/%u of \"%*s\"",
                   P.Index, X, Y, SB_GetLen (&B->Name),
                   SB_GetConstBuf (&B->Name));
        }
        return B->Pal->Entries[P.Index];
    } else {
        return P.C;
    }
}



Pixel GetPixel (const Bitmap* B, unsigned X, unsigned Y)
/* Return a pixel from the bitmap. The returned value may either be a color
 * or a palette index, depending on the type of the bitmap.
 */
{
    /* Check the coordinates */
    PRECONDITION (X < B->Width && Y < B->Height);

    /* Return the pixel */
    return B->Data[Y * B->Width + X];
}




/*****************************************************************************/
/*                                                                           */
/*				   attrtab.h				     */
/*                                                                           */
/*			 Disassembler attribute table			     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2000-2005 Ullrich von Bassewitz                                       */
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



#ifndef ATTRTAB_H
#define ATTRTAB_H



/*****************************************************************************/
/* 				     Data				     */
/*****************************************************************************/



typedef enum attr_t {

    /* Styles */
    atDefault	= 0x00,		/* Default style */
    atCode	= 0x01,
    atIllegal	= 0x02,
    atByteTab  	= 0x03,		/* Same as illegal */
    atDByteTab  = 0x04,
    atWordTab	= 0x05,
    atDWordTab	= 0x06,
    atAddrTab	= 0x07,
    atRtsTab	= 0x08,
    atTextTab   = 0x09,
    atSkip      = 0x0A,         /* Skip code completely */

    /* Label flags */
    atNoLabel	= 0x00,		/* No label for this address */
    atExtLabel	= 0x10,		/* External label */
    atIntLabel  = 0x20,		/* Internally generated label */
    atDepLabel 	= 0x40,		/* Dependent label */

    atStyleMask = 0x0F,		/* Output style */
    atLabelMask = 0x70		/* Label information */
} attr_t;



/*****************************************************************************/
/* 				     Code	   			     */
/*****************************************************************************/



unsigned GetGranularity (attr_t Style);
/* Get the granularity for the given style */

void MarkRange (unsigned Start, unsigned End, attr_t Attr);
/* Mark a range with the given attribute */

void MarkAddr (unsigned Addr, attr_t Attr);
/* Mark an address with an attribute */

void AddLabel (unsigned Addr, attr_t Attr, const char* Name, const char* Comment);
/* Add a label */

void AddIntLabel (unsigned Addr);
/* Add an internal label using the address to generate the name. */

void AddExtLabel (unsigned Addr, const char* Name, const char* Comment);
/* Add an external label */

void AddDepLabel (unsigned Addr, attr_t Attr, const char* BaseName, unsigned Offs);
/* Add a dependent label at the given address using "base name+Offs" as the new
 * name.
 */

void AddIntLabelRange (unsigned Addr, const char* Name, unsigned Count);
/* Add an internal label for a range. The first entry gets the label "Name"
 * while the others get "Name+offs".
 */

void AddExtLabelRange (unsigned Addr, const char* Name, const char* Comment, unsigned Count);
/* Add an external label for a range. The first entry gets the label "Name"
 * while the others get "Name+offs".
 */

int HaveLabel (unsigned Addr);
/* Check if there is a label for the given address */

int MustDefLabel (unsigned Addr);
/* Return true if we must define a label for this address, that is, if there
 * is a label at this address, and it is an external or internal label.
 */

const char* GetLabel (unsigned Addr);
/* Return the label for an address or NULL if there is none */

const char* GetComment (unsigned Addr);
/* Return the comment for an address */

unsigned char GetStyleAttr (unsigned Addr);
/* Return the style attribute for the given address */

unsigned char GetLabelAttr (unsigned Addr);
/* Return the label attribute for the given address */

void DefOutOfRangeLabels (void);
/* Output any labels that are out of the loaded code range */



/* End of attrtab.h */
#endif




/*****************************************************************************/
/*                                                                           */
/*                                  tgi-error.h                              */
/*                                                                           */
/*                                TGI error codes                            */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2002      Ullrich von Bassewitz                                       */
/*               Wacholderweg 14                                             */
/*               D-70597 Stuttgart                                           */
/* EMail:        uz@musoftware.de                                            */
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



#ifndef _TGI_ERROR_H
#define _TGI_ERROR_H



/*****************************************************************************/
/*				     Data                                    */
/*****************************************************************************/



#define TGI_ERR_OK              0       /* No error */
#define TGI_ERR_NO_DRIVER       1       /* No driver available */
#define TGI_ERR_LOAD_ERROR      2       /* Error loading driver */
#define TGI_ERR_INV_MODE        3       /* Mode not supported by driver */



/* End of tgi-error.h */
#endif




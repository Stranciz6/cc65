/*****************************************************************************/
/*                                                                           */
/*     	       	       	       	  exprdefs.h				     */
/*                                                                           */
/*			  Expression tree definitions			     */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 1998-2000 Ullrich von Bassewitz                                       */
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



#ifndef EXPRDEFS_H
#define EXPRDEFS_H



/*****************************************************************************/
/*     	      	    		     Data	   		       	     */
/*****************************************************************************/



/* Expression type masks */
#define EXPR_TYPEMASK    	0xC0
#define EXPR_BINARYNODE		0x00
#define EXPR_UNARYNODE		0x40
#define EXPR_LEAFNODE  		0x80

/* Type of expression nodes */
#define EXPR_NULL   	   	0x00	/* Internal error or NULL node */

/* Leaf node codes */
#define EXPR_LITERAL           	(EXPR_LEAFNODE | 0x01)
#define EXPR_SYMBOL            	(EXPR_LEAFNODE | 0x02)
#define EXPR_SEGMENT           	(EXPR_LEAFNODE | 0x03)
#define EXPR_MEMAREA		(EXPR_LEAFNODE | 0x04)	/* Linker only */
#define EXPR_ULABEL		(EXPR_LEAFNODE | 0x05) 	/* Assembler only */

/* Binary operations, left and right hand sides are valid */
#define EXPR_PLUS      	       	(EXPR_BINARYNODE | 0x01)
#define EXPR_MINUS             	(EXPR_BINARYNODE | 0x02)
#define EXPR_MUL       	       	(EXPR_BINARYNODE | 0x03)
#define EXPR_DIV       	       	(EXPR_BINARYNODE | 0x04)
#define EXPR_MOD       	       	(EXPR_BINARYNODE | 0x05)
#define EXPR_OR        	       	(EXPR_BINARYNODE | 0x06)
#define EXPR_XOR       	       	(EXPR_BINARYNODE | 0x07)
#define EXPR_AND       	       	(EXPR_BINARYNODE | 0x08)
#define EXPR_SHL       	       	(EXPR_BINARYNODE | 0x09)
#define EXPR_SHR       	       	(EXPR_BINARYNODE | 0x0A)
#define EXPR_EQ        	       	(EXPR_BINARYNODE | 0x0B)
#define EXPR_NE        	       	(EXPR_BINARYNODE | 0x0C)
#define EXPR_LT        	       	(EXPR_BINARYNODE | 0x0D)
#define EXPR_GT        	       	(EXPR_BINARYNODE | 0x0E)
#define EXPR_LE        	       	(EXPR_BINARYNODE | 0x0F)
#define EXPR_GE        	       	(EXPR_BINARYNODE | 0x10)
#define EXPR_BAND		(EXPR_BINARYNODE | 0x11)
#define EXPR_BOR		(EXPR_BINARYNODE | 0x12)
#define EXPR_BXOR		(EXPR_BINARYNODE | 0x13)

/* Unary operations, right hand side is empty */
#define EXPR_UNARY_MINUS       	(EXPR_UNARYNODE | 0x01)
#define EXPR_NOT       	       	(EXPR_UNARYNODE | 0x02)
#define EXPR_SWAP      	       	(EXPR_UNARYNODE | 0x03)
#define EXPR_BNOT		(EXPR_UNARYNODE | 0x04)
#define EXPR_FORCEWORD		(EXPR_UNARYNODE | 0x05)
#define EXPR_FORCEFAR		(EXPR_UNARYNODE | 0x06)

#define EXPR_BYTE0            	(EXPR_UNARYNODE | 0x08)
#define EXPR_BYTE1            	(EXPR_UNARYNODE | 0x09)
#define EXPR_BYTE2		(EXPR_UNARYNODE | 0x0A)
#define EXPR_BYTE3		(EXPR_UNARYNODE | 0x0B)
#define EXPR_WORD0		(EXPR_UNARYNODE | 0x0C)
#define EXPR_WORD1		(EXPR_UNARYNODE | 0x0D)



/* The expression node itself */
typedef struct ExprNode ExprNode;
struct ExprNode {
    unsigned char    	    Op;	    	/* Operand/Type */
    ExprNode*  	     	    Left;      	/* Left leaf */
    ExprNode*  	     	    Right;     	/* Right leaf */
    struct ObjData*	    Obj;	/* Object file reference (linker) */
    union {
       	long         	    Val;	/* If this is a value */
       	struct SymEntry*    Sym;	/* If this is a symbol */
	unsigned       	    SegNum;  	/* If this is a segment */
	unsigned	    ImpNum;	/* If this is an import */
	struct Memory*	    MemArea;	/* If this is a memory area */
    } V;
};



/* Macros to determine the expression type */
#define EXPR_IS_LEAF(Op)       	(((Op) & EXPR_TYPEMASK) == EXPR_LEAFNODE)
#define EXPR_IS_UNARY(Op)  	(((Op) & EXPR_TYPEMASK) == EXPR_UNARYNODE)
#define EXPR_IS_BINARY(OP)	(((Op) & EXPR_TYPEMASK) == EXPR_BINARYNODE)



/*****************************************************************************/
/*     	      	    		     Code	   		       	     */
/*****************************************************************************/



void DumpExpr (const ExprNode* Expr);
/* Dump an expression tree to stdout */



/* End of exprdefs.h */

#endif




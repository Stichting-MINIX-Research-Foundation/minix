/*	$NetBSD: varsyntax_calc1.tab.h,v 1.1.1.1 2015/01/03 22:58:24 christos Exp $	*/

#ifndef _varsyntax_calc1__defines_h_
#define _varsyntax_calc1__defines_h_

#define DREG 257
#define VREG 258
#define CONST 259
#define UMINUS 260
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union
{
	int ival;	/* dreg & vreg array index values*/
	double dval;	/* floating point values*/
	INTERVAL vval;	/* interval values*/
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
extern YYSTYPE varsyntax_calc1_lval;

#endif /* _varsyntax_calc1__defines_h_ */

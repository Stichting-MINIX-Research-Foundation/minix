/*	$NetBSD: btyacc_calc1.tab.h,v 1.1.1.1 2015/01/03 22:58:25 christos Exp $	*/

#ifndef _calc1__defines_h_
#define _calc1__defines_h_

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
	int ival;
	double dval;
	INTERVAL vval;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
extern YYSTYPE calc1_lval;

#endif /* _calc1__defines_h_ */

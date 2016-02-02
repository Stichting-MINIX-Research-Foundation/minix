/*	$NetBSD: btyacc_destroy2.tab.h,v 1.1.1.1 2015/01/03 22:58:24 christos Exp $	*/

#ifndef _destroy2__defines_h_
#define _destroy2__defines_h_

#define GLOBAL 257
#define LOCAL 258
#define REAL 259
#define INTEGER 260
#define NAME 261
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union
{
    class	cval;
    type	tval;
    namelist *	nlist;
    name	id;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
extern YYSTYPE destroy2_lval;

#endif /* _destroy2__defines_h_ */

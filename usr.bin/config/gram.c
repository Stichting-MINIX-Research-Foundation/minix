/* original parser id follows */
/* yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93" */
/* (use YYMAJOR/YYMINOR for ifdefs dependent on parser version) */

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)
#define YYENOMEM       (-2)
#define YYEOF          0
#undef YYBTYACC
#define YYBTYACC 0
#define YYDEBUGSTR YYPREFIX "debug"
#define YYPREFIX "yy"

#define YYPURE 0

#line 2 "gram.y"
/*	$NetBSD: gram.y,v 1.52 2015/09/01 13:42:48 uebayasi Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)gram.y	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: gram.y,v 1.52 2015/09/01 13:42:48 uebayasi Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "defs.h"
#include "sem.h"

#define	FORMAT(n) (((n).fmt == 8 && (n).val != 0) ? "0%llo" : \
    ((n).fmt == 16) ? "0x%llx" : "%lld")

#define	stop(s)	cfgerror(s), exit(1)

static	struct	config conf;	/* at most one active at a time */


/*
 * Allocation wrapper functions
 */
static void wrap_alloc(void *ptr, unsigned code);
static void wrap_continue(void);
static void wrap_cleanup(void);

/*
 * Allocation wrapper type codes
 */
#define WRAP_CODE_nvlist	1
#define WRAP_CODE_defoptlist	2
#define WRAP_CODE_loclist	3
#define WRAP_CODE_attrlist	4
#define WRAP_CODE_condexpr	5

/*
 * The allocation wrappers themselves
 */
#define DECL_ALLOCWRAP(t)	static struct t *wrap_mk_##t(struct t *arg)

DECL_ALLOCWRAP(nvlist);
DECL_ALLOCWRAP(defoptlist);
DECL_ALLOCWRAP(loclist);
DECL_ALLOCWRAP(attrlist);
DECL_ALLOCWRAP(condexpr);

/* allow shorter names */
#define wrap_mk_loc(p) wrap_mk_loclist(p)
#define wrap_mk_cx(p) wrap_mk_condexpr(p)

/*
 * Macros for allocating new objects
 */

/* old-style for struct nvlist */
#define	new0(n,s,p,i,x)	wrap_mk_nvlist(newnv(n, s, p, i, x))
#define	new_n(n)	new0(n, NULL, NULL, 0, NULL)
#define	new_nx(n, x)	new0(n, NULL, NULL, 0, x)
#define	new_ns(n, s)	new0(n, s, NULL, 0, NULL)
#define	new_si(s, i)	new0(NULL, s, NULL, i, NULL)
#define	new_nsi(n,s,i)	new0(n, s, NULL, i, NULL)
#define	new_np(n, p)	new0(n, NULL, p, 0, NULL)
#define	new_s(s)	new0(NULL, s, NULL, 0, NULL)
#define	new_p(p)	new0(NULL, NULL, p, 0, NULL)
#define	new_px(p, x)	new0(NULL, NULL, p, 0, x)
#define	new_sx(s, x)	new0(NULL, s, NULL, 0, x)
#define	new_nsx(n,s,x)	new0(n, s, NULL, 0, x)
#define	new_i(i)	new0(NULL, NULL, NULL, i, NULL)

/* new style, type-polymorphic; ordinary and for types with multiple flavors */
#define MK0(t)		wrap_mk_##t(mk_##t())
#define MK1(t, a0)	wrap_mk_##t(mk_##t(a0))
#define MK2(t, a0, a1)	wrap_mk_##t(mk_##t(a0, a1))
#define MK3(t, a0, a1, a2)	wrap_mk_##t(mk_##t(a0, a1, a2))

#define MKF0(t, f)		wrap_mk_##t(mk_##t##_##f())
#define MKF1(t, f, a0)		wrap_mk_##t(mk_##t##_##f(a0))
#define MKF2(t, f, a0, a1)	wrap_mk_##t(mk_##t##_##f(a0, a1))

/*
 * Data constructors
 */

static struct defoptlist *mk_defoptlist(const char *, const char *,
					const char *);
static struct loclist *mk_loc(const char *, const char *, long long);
static struct loclist *mk_loc_val(const char *, struct loclist *);
static struct attrlist *mk_attrlist(struct attrlist *, struct attr *);
static struct condexpr *mk_cx_atom(const char *);
static struct condexpr *mk_cx_not(struct condexpr *);
static struct condexpr *mk_cx_and(struct condexpr *, struct condexpr *);
static struct condexpr *mk_cx_or(struct condexpr *, struct condexpr *);

/*
 * Other private functions
 */

static	void	setmachine(const char *, const char *, struct nvlist *, int);
static	void	check_maxpart(void);

static struct loclist *present_loclist(struct loclist *ll);
static void app(struct loclist *, struct loclist *);
static struct loclist *locarray(const char *, int, struct loclist *, int);
static struct loclist *namelocvals(const char *, struct loclist *);

#line 153 "gram.y"
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union {
	struct	attr *attr;
	struct	devbase *devb;
	struct	deva *deva;
	struct	nvlist *list;
	struct defoptlist *defoptlist;
	struct loclist *loclist;
	struct attrlist *attrlist;
	struct condexpr *condexpr;
	const char *str;
	struct	numconst num;
	int64_t	val;
	u_char	flag;
	devmajor_t devmajor;
	int32_t i32;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
#line 197 "gram.c"

/* compatibility with bison */
#ifdef YYPARSE_PARAM
/* compatibility with FreeBSD */
# ifdef YYPARSE_PARAM_TYPE
#  define YYPARSE_DECL() yyparse(YYPARSE_PARAM_TYPE YYPARSE_PARAM)
# else
#  define YYPARSE_DECL() yyparse(void *YYPARSE_PARAM)
# endif
#else
# define YYPARSE_DECL() yyparse(void)
#endif

/* Parameters sent to lex. */
#ifdef YYLEX_PARAM
# define YYLEX_DECL() yylex(void *YYLEX_PARAM)
# define YYLEX yylex(YYLEX_PARAM)
#else
# define YYLEX_DECL() yylex(void)
# define YYLEX yylex()
#endif

/* Parameters sent to yyerror. */
#ifndef YYERROR_DECL
#define YYERROR_DECL() yyerror(const char *s)
#endif
#ifndef YYERROR_CALL
#define YYERROR_CALL(msg) yyerror(msg)
#endif

extern int YYPARSE_DECL();

#define AND 257
#define AT 258
#define ATTACH 259
#define BLOCK 260
#define BUILD 261
#define CHAR 262
#define COLONEQ 263
#define COMPILE_WITH 264
#define CONFIG 265
#define DEFFS 266
#define DEFINE 267
#define DEFOPT 268
#define DEFPARAM 269
#define DEFFLAG 270
#define DEFPSEUDO 271
#define DEFPSEUDODEV 272
#define DEVICE 273
#define DEVCLASS 274
#define DUMPS 275
#define DEVICE_MAJOR 276
#define ENDFILE 277
#define XFILE 278
#define FILE_SYSTEM 279
#define FLAGS 280
#define IDENT 281
#define IOCONF 282
#define LINKZERO 283
#define XMACHINE 284
#define MAJOR 285
#define MAKEOPTIONS 286
#define MAXUSERS 287
#define MAXPARTITIONS 288
#define MINOR 289
#define NEEDS_COUNT 290
#define NEEDS_FLAG 291
#define NO 292
#define XOBJECT 293
#define OBSOLETE 294
#define ON 295
#define OPTIONS 296
#define PACKAGE 297
#define PLUSEQ 298
#define PREFIX 299
#define BUILDPREFIX 300
#define PSEUDO_DEVICE 301
#define PSEUDO_ROOT 302
#define ROOT 303
#define SELECT 304
#define SINGLE 305
#define SOURCE 306
#define TYPE 307
#define VECTOR 308
#define VERSION 309
#define WITH 310
#define NUMBER 311
#define PATHNAME 312
#define QSTRING 313
#define WORD 314
#define EMPTYSTRING 315
#define ENDDEFS 316
#define YYERRCODE 256
typedef int YYINT;
static const YYINT yylhs[] = {                           -1,
    0,   57,   57,   61,   61,   61,   58,   58,   58,   58,
   58,   46,   46,   59,   62,   62,   62,   62,   62,   63,
   63,   63,   63,   63,   63,   63,   63,   63,   63,   63,
   63,   63,   63,   63,   63,   63,   63,   63,   63,   63,
   63,   64,   65,   66,   67,   67,   68,   68,   68,   69,
   70,   71,   72,   73,   74,   75,   76,   77,   78,   79,
   80,   81,   82,   83,   84,   85,    1,    1,    9,    9,
   10,   10,   13,   13,   11,   11,   12,   52,   52,   51,
   51,   53,   53,   53,   54,   54,   56,   56,   55,   39,
   39,   38,   18,   18,   18,   20,   20,   21,   21,   21,
   21,   21,   21,   49,   49,   22,   24,   25,   25,   26,
   26,   14,   44,   44,   43,   43,   42,   17,   17,   19,
   19,   41,   41,   40,   40,   40,   40,   86,   86,   88,
   15,   16,   16,   87,   87,   89,   35,   60,   90,   90,
   90,   90,   91,   91,   91,   91,   91,   91,   91,   91,
   91,   91,   91,   91,   91,   91,   91,   91,   91,   91,
   91,   91,   91,   92,   93,   94,   95,   96,   97,   98,
   99,  100,  101,  102,  103,  104,  105,  106,  107,  108,
  109,  110,  111,  113,  113,  121,  112,  112,  122,  115,
  115,  123,  123,  114,  114,  124,  117,  117,  125,  125,
  116,  116,  126,  118,  119,  119,   29,   29,   29,   33,
    8,    8,  120,  120,  128,   36,   36,   30,   30,   31,
   31,   31,   27,   27,   28,   28,   37,   37,    2,    4,
    4,    5,    5,    6,    7,    7,    7,    3,   50,   50,
   45,   45,   47,   47,   32,   32,   32,   32,   48,   48,
   23,   23,   34,   34,  127,  127,
};
static const YYINT yylen[] = {                            2,
    4,    0,    2,    1,    3,    3,    3,    4,    5,    3,
    1,    1,    2,    2,    0,    2,    3,    3,    2,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    5,    4,    6,    2,    1,    2,    2,    1,    2,
    3,    4,    4,    4,    4,    4,    4,    4,    6,    2,
    4,    2,    4,    4,    4,    2,    0,    1,    0,    2,
    1,    1,    0,    2,    0,    2,    1,    0,    2,    0,
    2,    0,    3,    1,    1,    3,    1,    3,    1,    1,
    2,    1,    0,    2,    3,    1,    3,    2,    1,    4,
    4,    5,    7,    1,    1,    2,    4,    0,    2,    1,
    3,    1,    0,    2,    1,    3,    1,    1,    3,    1,
    1,    1,    2,    1,    3,    3,    5,    1,    3,    4,
    1,    0,    2,    1,    3,    3,    1,    1,    0,    2,
    3,    3,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    2,    3,    3,    2,    3,    2,    3,
    2,    2,    2,    2,    4,    3,    3,    3,    2,    4,
    4,    2,    5,    1,    3,    1,    1,    3,    1,    1,
    3,    3,    3,    1,    3,    1,    1,    3,    1,    3,
    1,    3,    1,    1,    3,    4,    1,    1,    1,    4,
    2,    2,    0,    2,    3,    0,    1,    1,    2,    1,
    1,    2,    0,    2,    2,    2,    0,    2,    1,    1,
    3,    1,    3,    1,    1,    2,    3,    1,    1,    1,
    0,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    3,    1,    2,    0,    1,
};
static const YYINT yydefred[] = {                         2,
    0,    0,   11,    0,    0,    0,    0,    4,   15,    3,
  244,  243,    0,    0,    0,    0,  139,    0,    6,   10,
    0,    7,    5,    1,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   19,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   14,   16,
    0,   20,   21,   22,   23,   24,   25,   26,   27,   28,
   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,
   39,   40,   41,   12,    8,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  140,    0,
  143,    0,  144,  145,  146,  147,  148,  149,  150,  151,
  152,  153,  154,  155,  156,  157,  158,  159,  160,  161,
  162,  163,   18,  131,    0,   92,   90,    0,    0,    0,
  242,    0,    0,    0,    0,    0,   50,    0,    0,    0,
  238,    0,    0,    0,  235,    0,    0,  232,  234,    0,
  128,  137,    0,   60,    0,    0,    0,   45,   48,   47,
   66,   17,   13,    9,  142,  204,    0,  186,    0,  184,
  249,  250,  173,  239,    0,    0,    0,  190,    0,    0,
    0,    0,  174,    0,    0,    0,    0,    0,    0,    0,
  197,    0,  179,  164,  219,    0,  141,    0,    0,   91,
   51,    0,    0,    0,  122,    0,    0,    0,    0,    0,
    0,    0,    0,   69,   68,    0,    0,  134,  236,    0,
  240,    0,    0,    0,    0,    0,   75,    0,    0,    0,
  213,    0,    0,    0,    0,  176,    0,  189,    0,  187,
  196,    0,  194,  203,    0,  201,  177,  165,    0,    0,
    0,  217,  178,  220,    0,  223,  121,  120,    0,  118,
  117,  115,    0,  105,  104,   94,    0,    0,    0,    0,
    0,   52,    0,    0,  123,   53,   56,   54,   63,   64,
   58,   79,    0,    0,    0,    0,   65,    0,  237,    0,
    0,  233,  129,   61,    0,    0,    0,  256,    0,    0,
  185,  253,  245,  246,  247,    0,  193,  248,  192,  191,
  181,    0,    0,    0,  180,  200,  198,  222,    0,    0,
    0,    0,    0,    0,   95,    0,    0,    0,   98,  112,
  110,    0,  126,    0,   81,    0,    0,   71,   72,   70,
   42,  136,  135,  130,   77,   76,    0,  208,  207,    0,
  209,    0,  214,  254,  188,  195,  202,    0,    0,  224,
  183,  133,  119,   59,  116,    0,    0,   97,  106,    0,
    0,    0,   85,    0,   44,    0,   74,    0,    0,  206,
    0,  228,  225,  226,    0,    0,  100,    0,  111,  127,
    0,    0,    0,  212,  211,  215,    0,    0,    0,  102,
    0,   86,   89,   83,  210,  252,    0,    0,    0,  103,
    0,   88,  107,
};
#if defined(YYDESTRUCT_CALL) || defined(YYSTYPE_TOSTRING)
static const YYINT yystos[] = {                           0,
  318,  375,  256,  261,  282,  284,  306,   10,  376,  379,
  312,  313,  365,  314,  314,  365,  377,  380,   10,   10,
  314,   10,   10,  378,  408,  256,  259,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  276,  277,  278,  285,
  286,  287,  288,  293,  294,  299,  300,  309,  316,   10,
  381,  382,  383,  384,  385,  386,  387,  388,  389,  390,
  391,  392,  393,  394,  395,  396,  397,  398,  399,  400,
  401,  402,  403,  314,   10,  364,  256,  265,  279,  281,
  286,  287,  292,  296,  301,  302,  304,  314,   10,  348,
  381,  409,  410,  411,  412,  413,  414,  415,  416,  417,
  418,  419,  420,  421,  422,  423,  424,  425,  426,  427,
  428,  429,   10,  314,  333,  314,  356,  357,  314,  363,
  365,  363,  363,  333,  333,  333,  314,  314,  365,  123,
  314,   33,   40,  320,  321,  322,  323,  324,  325,  404,
  406,  311,  353,  353,  365,  269,  270,  365,  314,  365,
  353,   10,  314,   10,   10,  314,  436,  314,  431,  439,
  313,  314,  366,  313,  314,  368,  433,  441,  353,  265,
  273,  279,  281,  286,  296,  301,  304,  348,  314,  435,
  443,  314,  348,  314,   42,  258,   10,  258,   58,  356,
  362,  123,  336,  314,  358,  359,  359,  359,  336,  336,
  336,  262,  370,  319,  320,  333,  405,  407,  321,  320,
  314,  368,  124,   38,   44,  353,  319,  363,  363,  303,
  437,   44,  298,   61,   44,  314,  258,  314,  430,  440,
  314,  432,  442,  314,  434,  444,  314,  314,  258,   61,
   44,  353,  354,  303,  314,  349,  303,  314,  335,  337,
  314,  360,  361,  313,  314,  125,   91,  338,  339,  367,
   58,  343,  263,   61,  358,  362,  362,  362,  343,  343,
  343,  353,  260,  369,  327,   61,  125,   44,   41,  298,
  323,  324,  406,  353,  329,  359,  359,  295,  445,  438,
  439,  311,  313,  314,  315,   45,  350,  352,  350,  441,
  349,   44,   44,   44,  349,  350,  443,   63,  345,  310,
   44,  334,   44,  367,  125,   44,   61,   91,  340,  314,
  332,  344,  350,  350,  353,  319,  264,  290,  291,  328,
  331,  353,  407,  350,  291,  330,  285,  314,   63,  347,
  351,  275,  446,  311,  440,  442,  444,  280,  314,  346,
  355,  314,  337,  343,  360,   91,  340,  338,  350,  353,
   44,  263,  305,  308,  371,  372,  366,  311,  307,  326,
  445,  353,   63,  341,  350,  353,   93,   93,  332,  350,
   61,   44,  289,  314,   63,  347,   44,   93,   61,  342,
  311,  374,  283,  373,  311,  341,  342,  123,   58,   93,
  341,  311,  125,
};
#endif /* YYDESTRUCT_CALL || YYSTYPE_TOSTRING */
static const YYINT yydgoto[] = {                          1,
  204,  134,  135,  136,  137,  138,  139,  370,  275,  330,
  285,  336,  331,  321,  206,  312,  249,  193,  250,  258,
  259,  319,  374,  390,  262,  322,  309,  350,  340,   90,
  246,  375,  341,  298,  216,  243,  351,  117,  118,  195,
  196,  252,  253,  191,  120,   76,  121,  163,  260,  166,
  274,  203,  365,  366,  394,  392,    2,    9,   17,   24,
   10,   18,   51,   52,   53,   54,   55,   56,   57,   58,
   59,   60,   61,   62,   63,   64,   65,   66,   67,   68,
   69,   70,   71,   72,   73,  140,  207,  141,  208,   25,
   92,   93,   94,   95,   96,   97,   98,   99,  100,  101,
  102,  103,  104,  105,  106,  107,  108,  109,  110,  111,
  112,  229,  159,  232,  167,  235,  180,  157,  221,  290,
  160,  230,  168,  233,  181,  236,  289,  343,
};
static const YYINT yysindex[] = {                         0,
    0,  117,    0, -188, -247, -238, -188,    0,    0,    0,
    0,    0,   85,   95,   12,   99,    0,   -8,    0,    0,
   19,    0,    0,    0,   51,  101, -198, -193, -182, -188,
 -188, -188, -198, -198, -198, -179, -172,    0, -188,   22,
  -21, -164, -164, -188, -196, -188, -156, -164,    0,    0,
  141,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   26,  162, -141, -139, -184,
  -30, -164, -194, -138, -137, -136, -135,  138,    0,  -77,
    0,  172,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  -75,    0,    0,  -40,   62, -128,
    0, -128, -128,   62,   62,   62,    0,  -74,  -21, -198,
    0, -127,  -21, -175,    0,   65,  152,    0,    0,  148,
    0,    0, -164,    0,  -21, -188, -188,    0,    0,    0,
    0,    0,    0,    0,    0,    0, -110,    0,  150,    0,
    0,    0,    0,    0,    0,  -48,  154,    0, -164, -118,
  -59, -113,    0, -112, -111, -109, -108,  -54,  146,  164,
    0, -164,    0,    0,    0, -231,    0, -221, -105,    0,
    0,  -76,  153,  -36,    0,  -27,  -27,  -27,  153,  153,
  153, -164,  -50,    0,    0,  151,  -12,    0,    0,  173,
    0,  -85,  -21,  -21,  -21, -164,    0, -128, -128,  -80,
    0, -139,   89,   89, -175,    0, -231,    0,  174,    0,
    0,  175,    0,    0,  176,    0,    0,    0, -231,   89,
 -138,    0,    0,    0,  158,    0,    0,    0,  -28,    0,
    0,    0,  178,    0,    0,    0, -143,   91,  179,   -7,
  -97,    0,   89,   89,    0,    0,    0,    0,    0,    0,
    0,    0, -164,  -21, -173, -164,    0, -198,    0,   89,
  152,    0,    0,    0,  -67, -128, -128,    0,  -18,  -49,
    0,    0,    0,    0,    0,  -86,    0,    0,    0,    0,
    0, -113, -112, -111,    0,    0,    0,    0, -236,  -84,
 -221,  153, -105,    9,    0,  -71,   89, -164,    0,    0,
    0,  184,    0,  -34,    0, -239, -184,    0,    0,    0,
    0,    0,    0,    0,    0,    0,  -79,    0,    0,  -72,
    0,  -80,    0,    0,    0,    0,    0, -164,   83,    0,
    0,    0,    0,    0,    0, -164,  140,    0,    0,  143,
  -97,   89,    0,  180,    0,  195,    0,  -45,  -25,    0,
  -18,    0,    0,    0,  196,  156,    0,  185,    0,    0,
  -66,  -26,  -64,    0,    0,    0,   89,  185,  130,    0,
  198,    0,    0,    0,    0,    0,  161,   89,  -39,    0,
  165,    0,    0,
};
static const YYINT yyrindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,  273,    0,    0,    0,    0,  -15,
  -15,  -15,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  324,  331,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   -6,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  101,   31,    0,
    0,    0,    0,   31,   31,   31,    0,   -5,    7,    0,
    0,    0,    0,    0,    0,   78,   67,    0,    0,  338,
    0,    0,    0,    0,    4,  -15,  -15,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  344,    0,
    0,    0,    0,    0,   76,    0,  346,    0,  349,    0,
    0,    0,    0,    0,    0,    0,    0,  353,   50,  354,
    0,  356,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  357,   -2,    0,  101,  101,  101,  357,  357,
  357,    0,   -3,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -10,
    0,    0,    0,    0,    0,    0,    0,    0,  360,    0,
    0,  361,    0,    0,  367,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   14,    0,    0,    0,   32,    0,
    0,    0,  372,    0,    0,    0,    0,    0,  262,    6,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   41,  378,    0,    0,    0,    0,    0,
   71,    0,    0,    0,  383,  385,  395,    0,    0,  396,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,  397,    0,
    0,  357,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  398,    0,   -1,    0,  399,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   13,
    0,  -10,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,  400,    0,    0,    0,    0,
    0,    0,    0,    0,    1,    0,    0,    8,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   52,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,
};
#if YYBTYACC
static const YYINT yycindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,
};
#endif
static const YYINT yygindex[] = {                         0,
 -124,  -90,  279,    0,  199,  200,    0,    0,    0,    0,
    0,    0,    0,   54,  128,    0,    0,   40,  102,  100,
    0,  103, -301,   30, -153,    0,    0,    0,   48,   20,
 -159, -165,    0,    0,  -42,    0,    0,  302,    0, -133,
  -96,  108,    0,  -29,    2,    0,   97,   98,  167,  288,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,  401,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,  212,  155,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  206,  127,  205,  129,  190,  131,   92,    0,
};
#define YYTABLESIZE 435
static const YYINT yytable[] = {                        143,
  144,   50,  132,  218,   78,  151,   80,  124,  125,  133,
  251,  132,  224,   67,  257,  311,   67,  189,  133,  257,
  217,   22,  205,  221,  264,  197,  198,   78,   75,   80,
  189,  278,  122,  123,   78,  154,   80,  385,  205,  169,
   93,  132,  210,  348,  339,  269,  270,  271,  256,   99,
   67,  101,  255,  317,  205,  124,  125,  297,  299,  199,
   89,   87,  265,  265,  265,  363,   14,  301,  364,  317,
  170,  244,  146,  147,  306,   15,  230,  349,  171,  305,
  231,  247,  245,  318,  172,  396,  173,  229,   93,  132,
  327,  174,  248,  199,   19,   87,  401,  323,  324,  356,
   13,  175,  178,   16,   20,  183,  176,  230,   23,  177,
  113,  231,  277,  238,  334,  114,  328,  329,  229,   88,
  116,  286,  287,   11,   12,  251,    8,  296,  161,  162,
   99,  119,  101,  296,  127,  129,  240,  164,  211,  242,
  145,  128,  148,  150,  130,  373,  142,  218,  219,  326,
  152,  359,  265,  265,  115,   11,   12,  149,  354,  272,
  124,  125,  126,  199,  200,  201,  266,  267,  268,  254,
  255,  155,  156,  284,  158,  179,  182,   88,  184,  185,
  186,  187,  188,  205,  192,  194,  131,  202,  213,  214,
  230,  215,  220,  222,  231,  226,  380,  225,  227,  238,
  228,  231,  234,  239,  237,  238,  240,  241,  251,  273,
  261,  276,  280,  279,  288,  315,  320,  302,  303,  304,
  308,  313,  316,  335,  344,  342,  263,  361,  362,  352,
  325,  368,  377,  332,  369,  378,  254,  255,  382,  387,
  381,  254,  255,  383,  391,  389,  395,   26,  388,  223,
   27,  218,  398,  400,   78,  399,  393,   28,   29,   30,
   31,   32,   33,   34,   35,   36,  337,   37,   38,   39,
   67,  402,  138,  116,  255,  360,   40,   41,   42,   43,
  251,  310,  164,  165,   44,   45,  194,  205,  384,  403,
   46,   47,  131,  221,   67,  338,   67,   67,  241,   78,
   48,   80,   78,  255,   80,  372,   77,   49,   78,   27,
   80,  124,  125,  376,  251,   78,   28,   29,   30,   31,
   32,   33,   34,   35,   36,   21,   37,  221,   39,   79,
  230,   80,   74,   46,  231,   40,   81,   82,   43,  153,
   49,  229,   83,   44,   45,   67,   84,   62,   67,   46,
   47,   85,   86,  167,   87,  169,  230,  230,  172,   48,
  231,  231,  182,  171,   88,  216,  108,  229,  229,  166,
  168,  230,    3,  240,  230,  231,  170,    4,  231,  230,
  230,  114,  229,  231,  231,  229,   96,   73,  238,  238,
  229,  229,   43,  292,   57,  293,  294,  295,    5,  292,
    6,  293,  294,  295,   55,  175,  227,  109,   82,   84,
  209,  281,  353,  282,  379,  358,  357,  397,  386,  190,
  355,  212,    7,  314,  367,   91,  283,  291,  345,  300,
  307,  346,  333,  371,  347,
};
static const YYINT yycheck[] = {                         42,
   43,   10,   33,   10,   10,   48,   10,   10,   10,   40,
   10,   33,   61,   10,   91,   44,   10,   58,   40,   91,
  145,   10,   10,   10,   61,  122,  123,   33,   10,   33,
   58,   44,   31,   32,   40,   10,   40,   63,  129,   82,
   10,   10,  133,  280,   63,  199,  200,  201,  125,   44,
   10,   44,   63,   61,  145,   58,   58,  223,  224,   10,
   10,   10,  196,  197,  198,  305,  314,  227,  308,   61,
  265,  303,  269,  270,  240,  314,   10,  314,  273,  239,
   10,  303,  314,   91,  279,  387,  281,   10,   58,   58,
  264,  286,  314,   44,   10,   44,  398,  263,  264,   91,
    4,  296,   83,    7,   10,   86,  301,   41,   10,  304,
   10,   41,  125,   38,  280,  314,  290,  291,   41,  314,
  314,  218,  219,  312,  313,  125,   10,   45,  313,  314,
  125,  314,  125,   45,  314,   39,   61,  313,  314,  182,
   44,  314,   46,   47,  123,   63,  311,  146,  147,  274,
   10,  317,  286,  287,   27,  312,  313,  314,  312,  202,
   33,   34,   35,  124,  125,  126,  196,  197,  198,  313,
  314,   10,  314,  216,  314,  314,  314,  314,  314,   42,
  258,   10,  258,  274,  123,  314,  314,  262,  124,   38,
  124,   44,  303,   44,  124,  314,  362,   44,  258,  124,
  314,  314,  314,  258,  314,  314,   61,   44,  314,  260,
   58,   61,  298,   41,  295,  125,  314,   44,   44,   44,
   63,   44,   44,  291,  311,  275,  263,   44,  263,  314,
  273,  311,   93,  276,  307,   93,  313,  314,   44,   44,
   61,  313,  314,  289,  311,   61,  311,  256,   93,  298,
  259,  258,  123,   93,  260,   58,  283,  266,  267,  268,
  269,  270,  271,  272,  273,  274,  285,  276,  277,  278,
  264,  311,    0,  314,  285,  318,  285,  286,  287,  288,
  280,  310,  313,  314,  293,  294,  314,  275,  314,  125,
  299,  300,  314,  280,  291,  314,  290,  291,  314,  305,
  309,  305,  308,  314,  308,  348,  256,  316,  314,  259,
  314,  314,  314,  356,  314,  265,  266,  267,  268,  269,
  270,  271,  272,  273,  274,  314,  276,  314,  278,  279,
  264,  281,  314,   10,  264,  285,  286,  287,  288,  314,
   10,  264,  292,  293,  294,  305,  296,   10,  308,  299,
  300,  301,  302,   10,  304,   10,  290,  291,   10,  309,
  290,  291,   10,   10,  314,   10,   10,  290,  291,   10,
   10,  305,  256,  298,  308,  305,   10,  261,  308,  313,
  314,   10,  305,  313,  314,  308,  125,   10,  313,  314,
  313,  314,   10,  311,   10,  313,  314,  315,  282,  311,
  284,  313,  314,  315,   10,   10,   10,   10,   10,   10,
  132,  213,  311,  214,  361,  316,  314,  388,  371,  118,
  313,  134,  306,  257,  327,   25,  215,  222,  302,  225,
  241,  303,  278,  342,  304,
};
#if YYBTYACC
static const YYINT yyctable[] = {                        -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,
};
#endif
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 316
#define YYUNDFTOKEN 447
#define YYTRANSLATE(a) ((a) > YYMAXTOKEN ? YYUNDFTOKEN : (a))
#if YYDEBUG
static const char *const yyname[] = {

"$end",0,0,0,0,0,0,0,0,0,"'\\n'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'!'",0,0,0,0,"'&'",0,"'('","')'","'*'",0,"','","'-'",0,0,0,0,0,0,0,0,0,0,0,0,
"':'",0,0,"'='",0,"'?'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'['",0,"']'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",
"'|'","'}'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"error","AND","AT","ATTACH","BLOCK","BUILD",
"CHAR","COLONEQ","COMPILE_WITH","CONFIG","DEFFS","DEFINE","DEFOPT","DEFPARAM",
"DEFFLAG","DEFPSEUDO","DEFPSEUDODEV","DEVICE","DEVCLASS","DUMPS","DEVICE_MAJOR",
"ENDFILE","XFILE","FILE_SYSTEM","FLAGS","IDENT","IOCONF","LINKZERO","XMACHINE",
"MAJOR","MAKEOPTIONS","MAXUSERS","MAXPARTITIONS","MINOR","NEEDS_COUNT",
"NEEDS_FLAG","NO","XOBJECT","OBSOLETE","ON","OPTIONS","PACKAGE","PLUSEQ",
"PREFIX","BUILDPREFIX","PSEUDO_DEVICE","PSEUDO_ROOT","ROOT","SELECT","SINGLE",
"SOURCE","TYPE","VECTOR","VERSION","WITH","NUMBER","PATHNAME","QSTRING","WORD",
"EMPTYSTRING","ENDDEFS","$accept","configuration","fopts","condexpr","condatom",
"cond_or_expr","cond_and_expr","cond_prefix_expr","cond_base_expr","fs_spec",
"fflags","fflag","oflags","oflag","rule","depend","devbase","devattach_opt",
"atlist","interface_opt","atname","loclist","locdef","locdefault","values",
"locdefaults","depend_list","depends","locators","locator","dev_spec",
"device_instance","attachment","value","major_minor","signed_number","int32",
"npseudo","device_flags","deffs","deffses","defopt","defopts","optdepend",
"optdepends","optdepend_list","optfile_opt","subarches","filename",
"stringvalue","locname","mkvarname","device_major_block","device_major_char",
"devnodes","devnodetype","devnodeflags","devnode_dims","topthings",
"machine_spec","definition_part","selection_part","topthing","definitions",
"definition","define_file","define_object","define_device_major",
"define_prefix","define_buildprefix","define_devclass","define_filesystems",
"define_attribute","define_option","define_flag","define_obsolete_flag",
"define_param","define_obsolete_param","define_device",
"define_device_attachment","define_maxpartitions","define_maxusers",
"define_makeoptions","define_pseudo","define_pseudodev","define_major",
"define_version","condmkopt_list","majorlist","condmkoption","majordef",
"selections","selection","select_attr","select_no_attr","select_no_filesystems",
"select_filesystems","select_no_makeoptions","select_makeoptions",
"select_no_options","select_options","select_maxusers","select_ident",
"select_no_ident","select_config","select_no_config","select_no_pseudodev",
"select_pseudodev","select_pseudoroot","select_no_device_instance_attachment",
"select_no_device_attachment","select_no_device_instance",
"select_device_instance","no_fs_list","fs_list","no_mkopt_list","mkopt_list",
"no_opt_list","opt_list","conf","root_spec","sysparam_list","fsoption",
"no_fsoption","mkoption","no_mkoption","option","no_option","on_opt","sysparam",
"illegal-symbol",
};
static const char *const yyrule[] = {
"$accept : configuration",
"configuration : topthings machine_spec definition_part selection_part",
"topthings :",
"topthings : topthings topthing",
"topthing : '\\n'",
"topthing : SOURCE filename '\\n'",
"topthing : BUILD filename '\\n'",
"machine_spec : XMACHINE WORD '\\n'",
"machine_spec : XMACHINE WORD WORD '\\n'",
"machine_spec : XMACHINE WORD WORD subarches '\\n'",
"machine_spec : IOCONF WORD '\\n'",
"machine_spec : error",
"subarches : WORD",
"subarches : subarches WORD",
"definition_part : definitions ENDDEFS",
"definitions :",
"definitions : definitions '\\n'",
"definitions : definitions definition '\\n'",
"definitions : definitions error '\\n'",
"definitions : definitions ENDFILE",
"definition : define_file",
"definition : define_object",
"definition : define_device_major",
"definition : define_prefix",
"definition : define_buildprefix",
"definition : define_devclass",
"definition : define_filesystems",
"definition : define_attribute",
"definition : define_option",
"definition : define_flag",
"definition : define_obsolete_flag",
"definition : define_param",
"definition : define_obsolete_param",
"definition : define_device",
"definition : define_device_attachment",
"definition : define_maxpartitions",
"definition : define_maxusers",
"definition : define_makeoptions",
"definition : define_pseudo",
"definition : define_pseudodev",
"definition : define_major",
"definition : define_version",
"define_file : XFILE filename fopts fflags rule",
"define_object : XOBJECT filename fopts oflags",
"define_device_major : DEVICE_MAJOR WORD device_major_char device_major_block fopts devnodes",
"define_prefix : PREFIX filename",
"define_prefix : PREFIX",
"define_buildprefix : BUILDPREFIX filename",
"define_buildprefix : BUILDPREFIX WORD",
"define_buildprefix : BUILDPREFIX",
"define_devclass : DEVCLASS WORD",
"define_filesystems : DEFFS deffses optdepend_list",
"define_attribute : DEFINE WORD interface_opt depend_list",
"define_option : DEFOPT optfile_opt defopts optdepend_list",
"define_flag : DEFFLAG optfile_opt defopts optdepend_list",
"define_obsolete_flag : OBSOLETE DEFFLAG optfile_opt defopts",
"define_param : DEFPARAM optfile_opt defopts optdepend_list",
"define_obsolete_param : OBSOLETE DEFPARAM optfile_opt defopts",
"define_device : DEVICE devbase interface_opt depend_list",
"define_device_attachment : ATTACH devbase AT atlist devattach_opt depend_list",
"define_maxpartitions : MAXPARTITIONS int32",
"define_maxusers : MAXUSERS int32 int32 int32",
"define_makeoptions : MAKEOPTIONS condmkopt_list",
"define_pseudo : DEFPSEUDO devbase interface_opt depend_list",
"define_pseudodev : DEFPSEUDODEV devbase interface_opt depend_list",
"define_major : MAJOR '{' majorlist '}'",
"define_version : VERSION int32",
"fopts :",
"fopts : condexpr",
"fflags :",
"fflags : fflags fflag",
"fflag : NEEDS_COUNT",
"fflag : NEEDS_FLAG",
"rule :",
"rule : COMPILE_WITH stringvalue",
"oflags :",
"oflags : oflags oflag",
"oflag : NEEDS_FLAG",
"device_major_char :",
"device_major_char : CHAR int32",
"device_major_block :",
"device_major_block : BLOCK int32",
"devnodes :",
"devnodes : devnodetype ',' devnodeflags",
"devnodes : devnodetype",
"devnodetype : SINGLE",
"devnodetype : VECTOR '=' devnode_dims",
"devnode_dims : NUMBER",
"devnode_dims : NUMBER ':' NUMBER",
"devnodeflags : LINKZERO",
"deffses : deffs",
"deffses : deffses deffs",
"deffs : WORD",
"interface_opt :",
"interface_opt : '{' '}'",
"interface_opt : '{' loclist '}'",
"loclist : locdef",
"loclist : locdef ',' loclist",
"locdef : locname locdefault",
"locdef : locname",
"locdef : '[' locname locdefault ']'",
"locdef : locname '[' int32 ']'",
"locdef : locname '[' int32 ']' locdefaults",
"locdef : '[' locname '[' int32 ']' locdefaults ']'",
"locname : WORD",
"locname : QSTRING",
"locdefault : '=' value",
"locdefaults : '=' '{' values '}'",
"depend_list :",
"depend_list : ':' depends",
"depends : depend",
"depends : depends ',' depend",
"depend : WORD",
"optdepend_list :",
"optdepend_list : ':' optdepends",
"optdepends : optdepend",
"optdepends : optdepends ',' optdepend",
"optdepend : WORD",
"atlist : atname",
"atlist : atlist ',' atname",
"atname : WORD",
"atname : ROOT",
"defopts : defopt",
"defopts : defopts defopt",
"defopt : WORD",
"defopt : WORD '=' value",
"defopt : WORD COLONEQ value",
"defopt : WORD '=' value COLONEQ value",
"condmkopt_list : condmkoption",
"condmkopt_list : condmkopt_list ',' condmkoption",
"condmkoption : condexpr mkvarname PLUSEQ value",
"devbase : WORD",
"devattach_opt :",
"devattach_opt : WITH WORD",
"majorlist : majordef",
"majorlist : majorlist ',' majordef",
"majordef : devbase '=' int32",
"int32 : NUMBER",
"selection_part : selections",
"selections :",
"selections : selections '\\n'",
"selections : selections selection '\\n'",
"selections : selections error '\\n'",
"selection : definition",
"selection : select_attr",
"selection : select_no_attr",
"selection : select_no_filesystems",
"selection : select_filesystems",
"selection : select_no_makeoptions",
"selection : select_makeoptions",
"selection : select_no_options",
"selection : select_options",
"selection : select_maxusers",
"selection : select_ident",
"selection : select_no_ident",
"selection : select_config",
"selection : select_no_config",
"selection : select_no_pseudodev",
"selection : select_pseudodev",
"selection : select_pseudoroot",
"selection : select_no_device_instance_attachment",
"selection : select_no_device_attachment",
"selection : select_no_device_instance",
"selection : select_device_instance",
"select_attr : SELECT WORD",
"select_no_attr : NO SELECT WORD",
"select_no_filesystems : NO FILE_SYSTEM no_fs_list",
"select_filesystems : FILE_SYSTEM fs_list",
"select_no_makeoptions : NO MAKEOPTIONS no_mkopt_list",
"select_makeoptions : MAKEOPTIONS mkopt_list",
"select_no_options : NO OPTIONS no_opt_list",
"select_options : OPTIONS opt_list",
"select_maxusers : MAXUSERS int32",
"select_ident : IDENT stringvalue",
"select_no_ident : NO IDENT",
"select_config : CONFIG conf root_spec sysparam_list",
"select_no_config : NO CONFIG WORD",
"select_no_pseudodev : NO PSEUDO_DEVICE WORD",
"select_pseudodev : PSEUDO_DEVICE WORD npseudo",
"select_pseudoroot : PSEUDO_ROOT device_instance",
"select_no_device_instance_attachment : NO device_instance AT attachment",
"select_no_device_attachment : NO DEVICE AT attachment",
"select_no_device_instance : NO device_instance",
"select_device_instance : device_instance AT attachment locators device_flags",
"fs_list : fsoption",
"fs_list : fs_list ',' fsoption",
"fsoption : WORD",
"no_fs_list : no_fsoption",
"no_fs_list : no_fs_list ',' no_fsoption",
"no_fsoption : WORD",
"mkopt_list : mkoption",
"mkopt_list : mkopt_list ',' mkoption",
"mkoption : mkvarname '=' value",
"mkoption : mkvarname PLUSEQ value",
"no_mkopt_list : no_mkoption",
"no_mkopt_list : no_mkopt_list ',' no_mkoption",
"no_mkoption : WORD",
"opt_list : option",
"opt_list : opt_list ',' option",
"option : WORD",
"option : WORD '=' value",
"no_opt_list : no_option",
"no_opt_list : no_opt_list ',' no_option",
"no_option : WORD",
"conf : WORD",
"root_spec : ROOT on_opt dev_spec",
"root_spec : ROOT on_opt dev_spec fs_spec",
"dev_spec : '?'",
"dev_spec : WORD",
"dev_spec : major_minor",
"major_minor : MAJOR NUMBER MINOR NUMBER",
"fs_spec : TYPE '?'",
"fs_spec : TYPE WORD",
"sysparam_list :",
"sysparam_list : sysparam_list sysparam",
"sysparam : DUMPS on_opt dev_spec",
"npseudo :",
"npseudo : int32",
"device_instance : WORD",
"device_instance : WORD '*'",
"attachment : ROOT",
"attachment : WORD",
"attachment : WORD '?'",
"locators :",
"locators : locators locator",
"locator : WORD '?'",
"locator : WORD values",
"device_flags :",
"device_flags : FLAGS int32",
"condexpr : cond_or_expr",
"cond_or_expr : cond_and_expr",
"cond_or_expr : cond_or_expr '|' cond_and_expr",
"cond_and_expr : cond_prefix_expr",
"cond_and_expr : cond_and_expr '&' cond_prefix_expr",
"cond_prefix_expr : cond_base_expr",
"cond_base_expr : condatom",
"cond_base_expr : '!' condatom",
"cond_base_expr : '(' condexpr ')'",
"condatom : WORD",
"mkvarname : QSTRING",
"mkvarname : WORD",
"optfile_opt :",
"optfile_opt : filename",
"filename : QSTRING",
"filename : PATHNAME",
"value : QSTRING",
"value : WORD",
"value : EMPTYSTRING",
"value : signed_number",
"stringvalue : QSTRING",
"stringvalue : WORD",
"values : value",
"values : value ',' values",
"signed_number : NUMBER",
"signed_number : '-' NUMBER",
"on_opt :",
"on_opt : ON",

};
#endif

int      yydebug;
int      yynerrs;

int      yyerrflag;
int      yychar;
YYSTYPE  yyval;
YYSTYPE  yylval;

/* define the initial stack-sizes */
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH  YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH  10000
#endif
#endif

#define YYINITSTACKSIZE 200

typedef struct {
    unsigned stacksize;
    YYINT    *s_base;
    YYINT    *s_mark;
    YYINT    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 1083 "gram.y"

void
yyerror(const char *s)
{

	cfgerror("%s", s);
}

/************************************************************/

/*
 * Wrap allocations that live on the parser stack so that we can free
 * them again on error instead of leaking.
 */

#define MAX_WRAP 1000

struct wrap_entry {
	void *ptr;
	unsigned typecode;
};

static struct wrap_entry wrapstack[MAX_WRAP];
static unsigned wrap_depth;

/*
 * Remember pointer PTR with type-code CODE.
 */
static void
wrap_alloc(void *ptr, unsigned code)
{
	unsigned pos;

	if (wrap_depth >= MAX_WRAP) {
		panic("allocation wrapper stack overflow");
	}
	pos = wrap_depth++;
	wrapstack[pos].ptr = ptr;
	wrapstack[pos].typecode = code;
}

/*
 * We succeeded; commit to keeping everything that's been allocated so
 * far and clear the stack.
 */
static void
wrap_continue(void)
{
	wrap_depth = 0;
}

/*
 * We failed; destroy all the objects allocated.
 */
static void
wrap_cleanup(void)
{
	unsigned i;

	/*
	 * Destroy each item. Note that because everything allocated
	 * is entered on the list separately, lists and trees need to
	 * have their links blanked before being destroyed. Also note
	 * that strings are interned elsewhere and not handled by this
	 * mechanism.
	 */

	for (i=0; i<wrap_depth; i++) {
		switch (wrapstack[i].typecode) {
		    case WRAP_CODE_nvlist:
			nvfree(wrapstack[i].ptr);
			break;
		    case WRAP_CODE_defoptlist:
			{
				struct defoptlist *dl = wrapstack[i].ptr;

				dl->dl_next = NULL;
				defoptlist_destroy(dl);
			}
			break;
		    case WRAP_CODE_loclist:
			{
				struct loclist *ll = wrapstack[i].ptr;

				ll->ll_next = NULL;
				loclist_destroy(ll);
			}
			break;
		    case WRAP_CODE_attrlist:
			{
				struct attrlist *al = wrapstack[i].ptr;

				al->al_next = NULL;
				al->al_this = NULL;
				attrlist_destroy(al);
			}
			break;
		    case WRAP_CODE_condexpr:
			{
				struct condexpr *cx = wrapstack[i].ptr;

				cx->cx_type = CX_ATOM;
				cx->cx_atom = NULL;
				condexpr_destroy(cx);
			}
			break;
		    default:
			panic("invalid code %u on allocation wrapper stack",
			      wrapstack[i].typecode);
		}
	}

	wrap_depth = 0;
}

/*
 * Instantiate the wrapper functions.
 *
 * Each one calls wrap_alloc to save the pointer and then returns the
 * pointer again; these need to be generated with the preprocessor in
 * order to be typesafe.
 */
#define DEF_ALLOCWRAP(t) \
	static struct t *				\
	wrap_mk_##t(struct t *arg)			\
	{						\
		wrap_alloc(arg, WRAP_CODE_##t);		\
		return arg;				\
	}

DEF_ALLOCWRAP(nvlist);
DEF_ALLOCWRAP(defoptlist);
DEF_ALLOCWRAP(loclist);
DEF_ALLOCWRAP(attrlist);
DEF_ALLOCWRAP(condexpr);

/************************************************************/

/*
 * Data constructors
 *
 * (These are *beneath* the allocation wrappers.)
 */

static struct defoptlist *
mk_defoptlist(const char *name, const char *val, const char *lintval)
{
	return defoptlist_create(name, val, lintval);
}

static struct loclist *
mk_loc(const char *name, const char *str, long long num)
{
	return loclist_create(name, str, num);
}

static struct loclist *
mk_loc_val(const char *str, struct loclist *next)
{
	struct loclist *ll;

	ll = mk_loc(NULL, str, 0);
	ll->ll_next = next;
	return ll;
}

static struct attrlist *
mk_attrlist(struct attrlist *next, struct attr *a)
{
	return attrlist_cons(next, a);
}

static struct condexpr *
mk_cx_atom(const char *s)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_ATOM);
	cx->cx_atom = s;
	return cx;
}

static struct condexpr *
mk_cx_not(struct condexpr *sub)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_NOT);
	cx->cx_not = sub;
	return cx;
}

static struct condexpr *
mk_cx_and(struct condexpr *left, struct condexpr *right)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_AND);
	cx->cx_and.left = left;
	cx->cx_and.right = right;
	return cx;
}

static struct condexpr *
mk_cx_or(struct condexpr *left, struct condexpr *right)
{
	struct condexpr *cx;

	cx = condexpr_create(CX_OR);
	cx->cx_or.left = left;
	cx->cx_or.right = right;
	return cx;
}

/************************************************************/

static void
setmachine(const char *mch, const char *mcharch, struct nvlist *mchsubarches,
	int isioconf)
{
	char buf[MAXPATHLEN];
	struct nvlist *nv;

	if (isioconf) {
		if (include(_PATH_DEVNULL, ENDDEFS, 0, 0) != 0)
			exit(1);
		ioconfname = mch;
		return;
	}

	machine = mch;
	machinearch = mcharch;
	machinesubarches = mchsubarches;

	/*
	 * Define attributes for all the given names
	 */
	if (defattr(machine, NULL, NULL, 0) != 0 ||
	    (machinearch != NULL &&
	     defattr(machinearch, NULL, NULL, 0) != 0))
		exit(1);
	for (nv = machinesubarches; nv != NULL; nv = nv->nv_next) {
		if (defattr(nv->nv_name, NULL, NULL, 0) != 0)
			exit(1);
	}

	/*
	 * Set up the file inclusion stack.  This empty include tells
	 * the parser there are no more device definitions coming.
	 */
	if (include(_PATH_DEVNULL, ENDDEFS, 0, 0) != 0)
		exit(1);

	/* Include arch/${MACHINE}/conf/files.${MACHINE} */
	(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
	    machine, machine);
	if (include(buf, ENDFILE, 0, 0) != 0)
		exit(1);

	/* Include any arch/${MACHINE_SUBARCH}/conf/files.${MACHINE_SUBARCH} */
	for (nv = machinesubarches; nv != NULL; nv = nv->nv_next) {
		(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
		    nv->nv_name, nv->nv_name);
		if (include(buf, ENDFILE, 0, 0) != 0)
			exit(1);
	}

	/* Include any arch/${MACHINE_ARCH}/conf/files.${MACHINE_ARCH} */
	if (machinearch != NULL)
		(void)snprintf(buf, sizeof(buf), "arch/%s/conf/files.%s",
		    machinearch, machinearch);
	else
		strlcpy(buf, _PATH_DEVNULL, sizeof(buf));
	if (include(buf, ENDFILE, 0, 0) != 0)
		exit(1);

	/*
	 * Include the global conf/files.  As the last thing
	 * pushed on the stack, it will be processed first.
	 */
	if (include("conf/files", ENDFILE, 0, 0) != 0)
		exit(1);

	oktopackage = 1;
}

static void
check_maxpart(void)
{

	if (maxpartitions <= 0 && ioconfname == NULL) {
		stop("cannot proceed without maxpartitions specifier");
	}
}

static void
check_version(void)
{
	/*
	 * In essence, version is 0 and is not supported anymore
	 */
	if (version < CONFIG_MINVERSION)
		stop("your sources are out of date -- please update.");
}

/*
 * Prepend a blank entry to the locator definitions so the code in
 * sem.c can distinguish "empty locator list" from "no locator list".
 * XXX gross.
 */
static struct loclist *
present_loclist(struct loclist *ll)
{
	struct loclist *ret;

	ret = MK3(loc, "", NULL, 0);
	ret->ll_next = ll;
	return ret;
}

static void
app(struct loclist *p, struct loclist *q)
{
	while (p->ll_next)
		p = p->ll_next;
	p->ll_next = q;
}

static struct loclist *
locarray(const char *name, int count, struct loclist *adefs, int opt)
{
	struct loclist *defs = adefs;
	struct loclist **p;
	char buf[200];
	int i;

	if (count <= 0) {
		fprintf(stderr, "config: array with <= 0 size: %s\n", name);
		exit(1);
	}
	p = &defs;
	for(i = 0; i < count; i++) {
		if (*p == NULL)
			*p = MK3(loc, NULL, "0", 0);
		snprintf(buf, sizeof(buf), "%s%c%d", name, ARRCHR, i);
		(*p)->ll_name = i == 0 ? name : intern(buf);
		(*p)->ll_num = i > 0 || opt;
		p = &(*p)->ll_next;
	}
	*p = 0;
	return defs;
}


static struct loclist *
namelocvals(const char *name, struct loclist *vals)
{
	struct loclist *p;
	char buf[200];
	int i;

	for (i = 0, p = vals; p; i++, p = p->ll_next) {
		snprintf(buf, sizeof(buf), "%s%c%d", name, ARRCHR, i);
		p->ll_name = i == 0 ? name : intern(buf);
	}
	return vals;
}

#line 1456 "gram.c"

#if YYDEBUG
#include <stdio.h>		/* needed for printf */
#endif

#include <stdlib.h>	/* needed for malloc, etc */
#include <string.h>	/* needed for memset */

/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(YYSTACKDATA *data)
{
    int i;
    unsigned newsize;
    YYINT *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return YYENOMEM;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = (int) (data->s_mark - data->s_base);
    newss = (YYINT *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return YYENOMEM;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return YYENOMEM;

    data->l_base = newvs;
    data->l_mark = newvs + i;

    data->stacksize = newsize;
    data->s_last = data->s_base + newsize - 1;
    return 0;
}

#if YYPURE || defined(YY_NO_LEAKS)
static void yyfreestack(YYSTACKDATA *data)
{
    free(data->s_base);
    free(data->l_base);
    memset(data, 0, sizeof(*data));
}
#else
#define yyfreestack(data) /* nothing */
#endif

#define YYABORT  goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR  goto yyerrlab

int
YYPARSE_DECL()
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != 0)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = YYEMPTY;
    yystate = 0;

#if YYPURE
    memset(&yystack, 0, sizeof(yystack));
#endif

    if (yystack.s_base == NULL && yygrowstack(&yystack) == YYENOMEM) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = YYLEX) < 0) yychar = YYEOF;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
        {
            goto yyoverflow;
        }
        yystate = yytable[yyn];
        *++yystack.s_mark = yytable[yyn];
        *++yystack.l_mark = yylval;
        yychar = YYEMPTY;
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;

    YYERROR_CALL("syntax error");

    goto yyerrlab;

yyerrlab:
    ++yynerrs;

yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yystack.s_mark]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
                {
                    goto yyoverflow;
                }
                yystate = yytable[yyn];
                *++yystack.s_mark = yytable[yyn];
                *++yystack.l_mark = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yystack.s_mark);
#endif
                if (yystack.s_mark <= yystack.s_base) goto yyabort;
                --yystack.s_mark;
                --yystack.l_mark;
            }
        }
    }
    else
    {
        if (yychar == YYEOF) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = yyname[YYTRANSLATE(yychar)];
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = YYEMPTY;
        goto yyloop;
    }

yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    if (yym)
        yyval = yystack.l_mark[1-yym];
    else
        memset(&yyval, 0, sizeof yyval);
    switch (yyn)
    {
case 5:
#line 275 "gram.y"
	{ if (!srcdir) srcdir = yystack.l_mark[-1].str; }
break;
case 6:
#line 276 "gram.y"
	{ if (!builddir) builddir = yystack.l_mark[-1].str; }
break;
case 7:
#line 281 "gram.y"
	{ setmachine(yystack.l_mark[-1].str,NULL,NULL,0); }
break;
case 8:
#line 282 "gram.y"
	{ setmachine(yystack.l_mark[-2].str,yystack.l_mark[-1].str,NULL,0); }
break;
case 9:
#line 283 "gram.y"
	{ setmachine(yystack.l_mark[-3].str,yystack.l_mark[-2].str,yystack.l_mark[-1].list,0); }
break;
case 10:
#line 284 "gram.y"
	{ setmachine(yystack.l_mark[-1].str,NULL,NULL,1); }
break;
case 11:
#line 285 "gram.y"
	{ stop("cannot proceed without machine or ioconf specifier"); }
break;
case 12:
#line 290 "gram.y"
	{ yyval.list = new_n(yystack.l_mark[0].str); }
break;
case 13:
#line 291 "gram.y"
	{ yyval.list = new_nx(yystack.l_mark[0].str, yystack.l_mark[-1].list); }
break;
case 14:
#line 302 "gram.y"
	{ check_maxpart(); check_version(); }
break;
case 17:
#line 309 "gram.y"
	{ wrap_continue(); }
break;
case 18:
#line 310 "gram.y"
	{ wrap_cleanup(); }
break;
case 19:
#line 311 "gram.y"
	{ enddefs(); checkfiles(); }
break;
case 42:
#line 342 "gram.y"
	{ addfile(yystack.l_mark[-3].str, yystack.l_mark[-2].condexpr, yystack.l_mark[-1].flag, yystack.l_mark[0].str); }
break;
case 43:
#line 347 "gram.y"
	{ addfile(yystack.l_mark[-2].str, yystack.l_mark[-1].condexpr, yystack.l_mark[0].flag, NULL); }
break;
case 44:
#line 353 "gram.y"
	{
		adddevm(yystack.l_mark[-4].str, yystack.l_mark[-3].devmajor, yystack.l_mark[-2].devmajor, yystack.l_mark[-1].condexpr, yystack.l_mark[0].list);
		do_devsw = 1;
	}
break;
case 45:
#line 361 "gram.y"
	{ prefix_push(yystack.l_mark[0].str); }
break;
case 46:
#line 362 "gram.y"
	{ prefix_pop(); }
break;
case 47:
#line 366 "gram.y"
	{ buildprefix_push(yystack.l_mark[0].str); }
break;
case 48:
#line 367 "gram.y"
	{ buildprefix_push(yystack.l_mark[0].str); }
break;
case 49:
#line 368 "gram.y"
	{ buildprefix_pop(); }
break;
case 50:
#line 372 "gram.y"
	{ (void)defdevclass(yystack.l_mark[0].str, NULL, NULL, 1); }
break;
case 51:
#line 376 "gram.y"
	{ deffilesystem(yystack.l_mark[-1].list, yystack.l_mark[0].list); }
break;
case 52:
#line 381 "gram.y"
	{ (void)defattr0(yystack.l_mark[-2].str, yystack.l_mark[-1].loclist, yystack.l_mark[0].attrlist, 0); }
break;
case 53:
#line 386 "gram.y"
	{ defoption(yystack.l_mark[-2].str, yystack.l_mark[-1].defoptlist, yystack.l_mark[0].list); }
break;
case 54:
#line 391 "gram.y"
	{ defflag(yystack.l_mark[-2].str, yystack.l_mark[-1].defoptlist, yystack.l_mark[0].list, 0); }
break;
case 55:
#line 396 "gram.y"
	{ defflag(yystack.l_mark[-1].str, yystack.l_mark[0].defoptlist, NULL, 1); }
break;
case 56:
#line 401 "gram.y"
	{ defparam(yystack.l_mark[-2].str, yystack.l_mark[-1].defoptlist, yystack.l_mark[0].list, 0); }
break;
case 57:
#line 406 "gram.y"
	{ defparam(yystack.l_mark[-1].str, yystack.l_mark[0].defoptlist, NULL, 1); }
break;
case 58:
#line 411 "gram.y"
	{ defdev(yystack.l_mark[-2].devb, yystack.l_mark[-1].loclist, yystack.l_mark[0].attrlist, 0); }
break;
case 59:
#line 416 "gram.y"
	{ defdevattach(yystack.l_mark[-1].deva, yystack.l_mark[-4].devb, yystack.l_mark[-2].list, yystack.l_mark[0].attrlist); }
break;
case 60:
#line 420 "gram.y"
	{ maxpartitions = yystack.l_mark[0].i32; }
break;
case 61:
#line 425 "gram.y"
	{ setdefmaxusers(yystack.l_mark[-2].i32, yystack.l_mark[-1].i32, yystack.l_mark[0].i32); }
break;
case 63:
#line 435 "gram.y"
	{ defdev(yystack.l_mark[-2].devb, yystack.l_mark[-1].loclist, yystack.l_mark[0].attrlist, 1); }
break;
case 64:
#line 440 "gram.y"
	{ defdev(yystack.l_mark[-2].devb, yystack.l_mark[-1].loclist, yystack.l_mark[0].attrlist, 2); }
break;
case 66:
#line 448 "gram.y"
	{ setversion(yystack.l_mark[0].i32); }
break;
case 67:
#line 453 "gram.y"
	{ yyval.condexpr = NULL; }
break;
case 68:
#line 454 "gram.y"
	{ yyval.condexpr = yystack.l_mark[0].condexpr; }
break;
case 69:
#line 459 "gram.y"
	{ yyval.flag = 0; }
break;
case 70:
#line 460 "gram.y"
	{ yyval.flag = yystack.l_mark[-1].flag | yystack.l_mark[0].flag; }
break;
case 71:
#line 465 "gram.y"
	{ yyval.flag = FI_NEEDSCOUNT; }
break;
case 72:
#line 466 "gram.y"
	{ yyval.flag = FI_NEEDSFLAG; }
break;
case 73:
#line 471 "gram.y"
	{ yyval.str = NULL; }
break;
case 74:
#line 472 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 75:
#line 477 "gram.y"
	{ yyval.flag = 0; }
break;
case 76:
#line 478 "gram.y"
	{ yyval.flag = yystack.l_mark[-1].flag | yystack.l_mark[0].flag; }
break;
case 77:
#line 483 "gram.y"
	{ yyval.flag = FI_NEEDSFLAG; }
break;
case 78:
#line 488 "gram.y"
	{ yyval.devmajor = -1; }
break;
case 79:
#line 489 "gram.y"
	{ yyval.devmajor = yystack.l_mark[0].i32; }
break;
case 80:
#line 494 "gram.y"
	{ yyval.devmajor = -1; }
break;
case 81:
#line 495 "gram.y"
	{ yyval.devmajor = yystack.l_mark[0].i32; }
break;
case 82:
#line 500 "gram.y"
	{ yyval.list = new_s("DEVNODE_DONTBOTHER"); }
break;
case 83:
#line 501 "gram.y"
	{ yyval.list = nvcat(yystack.l_mark[-2].list, yystack.l_mark[0].list); }
break;
case 84:
#line 502 "gram.y"
	{ yyval.list = yystack.l_mark[0].list; }
break;
case 85:
#line 507 "gram.y"
	{ yyval.list = new_s("DEVNODE_SINGLE"); }
break;
case 86:
#line 508 "gram.y"
	{ yyval.list = nvcat(new_s("DEVNODE_VECTOR"), yystack.l_mark[0].list); }
break;
case 87:
#line 513 "gram.y"
	{ yyval.list = new_i(yystack.l_mark[0].num.val); }
break;
case 88:
#line 514 "gram.y"
	{
		struct nvlist *__nv1, *__nv2;

		__nv1 = new_i(yystack.l_mark[-2].num.val);
		__nv2 = new_i(yystack.l_mark[0].num.val);
		yyval.list = nvcat(__nv1, __nv2);
	  }
break;
case 89:
#line 525 "gram.y"
	{ yyval.list = new_s("DEVNODE_FLAG_LINKZERO");}
break;
case 90:
#line 530 "gram.y"
	{ yyval.list = new_n(yystack.l_mark[0].str); }
break;
case 91:
#line 531 "gram.y"
	{ yyval.list = new_nx(yystack.l_mark[0].str, yystack.l_mark[-1].list); }
break;
case 92:
#line 536 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 93:
#line 541 "gram.y"
	{ yyval.loclist = NULL; }
break;
case 94:
#line 542 "gram.y"
	{ yyval.loclist = present_loclist(NULL); }
break;
case 95:
#line 543 "gram.y"
	{ yyval.loclist = present_loclist(yystack.l_mark[-1].loclist); }
break;
case 96:
#line 553 "gram.y"
	{ yyval.loclist = yystack.l_mark[0].loclist; }
break;
case 97:
#line 554 "gram.y"
	{ yyval.loclist = yystack.l_mark[-2].loclist; app(yystack.l_mark[-2].loclist, yystack.l_mark[0].loclist); }
break;
case 98:
#line 563 "gram.y"
	{ yyval.loclist = MK3(loc, yystack.l_mark[-1].str, yystack.l_mark[0].str, 0); }
break;
case 99:
#line 564 "gram.y"
	{ yyval.loclist = MK3(loc, yystack.l_mark[0].str, NULL, 0); }
break;
case 100:
#line 565 "gram.y"
	{ yyval.loclist = MK3(loc, yystack.l_mark[-2].str, yystack.l_mark[-1].str, 1); }
break;
case 101:
#line 566 "gram.y"
	{ yyval.loclist = locarray(yystack.l_mark[-3].str, yystack.l_mark[-1].i32, NULL, 0); }
break;
case 102:
#line 568 "gram.y"
	{ yyval.loclist = locarray(yystack.l_mark[-4].str, yystack.l_mark[-2].i32, yystack.l_mark[0].loclist, 0); }
break;
case 103:
#line 570 "gram.y"
	{ yyval.loclist = locarray(yystack.l_mark[-5].str, yystack.l_mark[-3].i32, yystack.l_mark[-1].loclist, 1); }
break;
case 104:
#line 575 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 105:
#line 576 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 106:
#line 581 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 107:
#line 586 "gram.y"
	{ yyval.loclist = yystack.l_mark[-1].loclist; }
break;
case 108:
#line 591 "gram.y"
	{ yyval.attrlist = NULL; }
break;
case 109:
#line 592 "gram.y"
	{ yyval.attrlist = yystack.l_mark[0].attrlist; }
break;
case 110:
#line 597 "gram.y"
	{ yyval.attrlist = MK2(attrlist, NULL, yystack.l_mark[0].attr); }
break;
case 111:
#line 598 "gram.y"
	{ yyval.attrlist = MK2(attrlist, yystack.l_mark[-2].attrlist, yystack.l_mark[0].attr); }
break;
case 112:
#line 603 "gram.y"
	{ yyval.attr = refattr(yystack.l_mark[0].str); }
break;
case 113:
#line 608 "gram.y"
	{ yyval.list = NULL; }
break;
case 114:
#line 609 "gram.y"
	{ yyval.list = yystack.l_mark[0].list; }
break;
case 115:
#line 614 "gram.y"
	{ yyval.list = new_n(yystack.l_mark[0].str); }
break;
case 116:
#line 615 "gram.y"
	{ yyval.list = new_nx(yystack.l_mark[0].str, yystack.l_mark[-2].list); }
break;
case 117:
#line 620 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 118:
#line 626 "gram.y"
	{ yyval.list = new_n(yystack.l_mark[0].str); }
break;
case 119:
#line 627 "gram.y"
	{ yyval.list = new_nx(yystack.l_mark[0].str, yystack.l_mark[-2].list); }
break;
case 120:
#line 632 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 121:
#line 633 "gram.y"
	{ yyval.str = NULL; }
break;
case 122:
#line 638 "gram.y"
	{ yyval.defoptlist = yystack.l_mark[0].defoptlist; }
break;
case 123:
#line 639 "gram.y"
	{ yyval.defoptlist = defoptlist_append(yystack.l_mark[0].defoptlist, yystack.l_mark[-1].defoptlist); }
break;
case 124:
#line 644 "gram.y"
	{ yyval.defoptlist = MK3(defoptlist, yystack.l_mark[0].str, NULL, NULL); }
break;
case 125:
#line 645 "gram.y"
	{ yyval.defoptlist = MK3(defoptlist, yystack.l_mark[-2].str, yystack.l_mark[0].str, NULL); }
break;
case 126:
#line 646 "gram.y"
	{ yyval.defoptlist = MK3(defoptlist, yystack.l_mark[-2].str, NULL, yystack.l_mark[0].str); }
break;
case 127:
#line 647 "gram.y"
	{ yyval.defoptlist = MK3(defoptlist, yystack.l_mark[-4].str, yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 130:
#line 658 "gram.y"
	{ appendcondmkoption(yystack.l_mark[-3].condexpr, yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 131:
#line 663 "gram.y"
	{ yyval.devb = getdevbase(yystack.l_mark[0].str); }
break;
case 132:
#line 668 "gram.y"
	{ yyval.deva = NULL; }
break;
case 133:
#line 669 "gram.y"
	{ yyval.deva = getdevattach(yystack.l_mark[0].str); }
break;
case 136:
#line 681 "gram.y"
	{ setmajor(yystack.l_mark[-2].devb, yystack.l_mark[0].i32); }
break;
case 137:
#line 685 "gram.y"
	{
		if (yystack.l_mark[0].num.val > INT_MAX || yystack.l_mark[0].num.val < INT_MIN)
			cfgerror("overflow %" PRId64, yystack.l_mark[0].num.val);
		else
			yyval.i32 = (int32_t)yystack.l_mark[0].num.val;
	}
break;
case 141:
#line 708 "gram.y"
	{ wrap_continue(); }
break;
case 142:
#line 709 "gram.y"
	{ wrap_cleanup(); }
break;
case 164:
#line 738 "gram.y"
	{ addattr(yystack.l_mark[0].str); }
break;
case 165:
#line 742 "gram.y"
	{ delattr(yystack.l_mark[0].str); }
break;
case 172:
#line 770 "gram.y"
	{ setmaxusers(yystack.l_mark[0].i32); }
break;
case 173:
#line 774 "gram.y"
	{ setident(yystack.l_mark[0].str); }
break;
case 174:
#line 778 "gram.y"
	{ setident(NULL); }
break;
case 175:
#line 783 "gram.y"
	{ addconf(&conf); }
break;
case 176:
#line 787 "gram.y"
	{ delconf(yystack.l_mark[0].str); }
break;
case 177:
#line 791 "gram.y"
	{ delpseudo(yystack.l_mark[0].str); }
break;
case 178:
#line 795 "gram.y"
	{ addpseudo(yystack.l_mark[-1].str, yystack.l_mark[0].i32); }
break;
case 179:
#line 799 "gram.y"
	{ addpseudoroot(yystack.l_mark[0].str); }
break;
case 180:
#line 804 "gram.y"
	{ deldevi(yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 181:
#line 808 "gram.y"
	{ deldeva(yystack.l_mark[0].str); }
break;
case 182:
#line 812 "gram.y"
	{ deldev(yystack.l_mark[0].str); }
break;
case 183:
#line 817 "gram.y"
	{ adddev(yystack.l_mark[-4].str, yystack.l_mark[-2].str, yystack.l_mark[-1].loclist, yystack.l_mark[0].i32); }
break;
case 186:
#line 828 "gram.y"
	{ addfsoption(yystack.l_mark[0].str); }
break;
case 189:
#line 839 "gram.y"
	{ delfsoption(yystack.l_mark[0].str); }
break;
case 192:
#line 851 "gram.y"
	{ addmkoption(yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 193:
#line 852 "gram.y"
	{ appendmkoption(yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 196:
#line 864 "gram.y"
	{ delmkoption(yystack.l_mark[0].str); }
break;
case 199:
#line 875 "gram.y"
	{ addoption(yystack.l_mark[0].str, NULL); }
break;
case 200:
#line 876 "gram.y"
	{ addoption(yystack.l_mark[-2].str, yystack.l_mark[0].str); }
break;
case 203:
#line 887 "gram.y"
	{ deloption(yystack.l_mark[0].str); }
break;
case 204:
#line 892 "gram.y"
	{
		conf.cf_name = yystack.l_mark[0].str;
		conf.cf_lineno = currentline();
		conf.cf_fstype = NULL;
		conf.cf_root = NULL;
		conf.cf_dump = NULL;
	}
break;
case 205:
#line 903 "gram.y"
	{ setconf(&conf.cf_root, "root", yystack.l_mark[0].list); }
break;
case 206:
#line 904 "gram.y"
	{ setconf(&conf.cf_root, "root", yystack.l_mark[-1].list); }
break;
case 207:
#line 909 "gram.y"
	{ yyval.list = new_si(intern("?"),
					    (long long)NODEV); }
break;
case 208:
#line 911 "gram.y"
	{ yyval.list = new_si(yystack.l_mark[0].str,
					    (long long)NODEV); }
break;
case 209:
#line 913 "gram.y"
	{ yyval.list = new_si(NULL, yystack.l_mark[0].val); }
break;
case 210:
#line 918 "gram.y"
	{ yyval.val = (int64_t)makedev(yystack.l_mark[-2].num.val, yystack.l_mark[0].num.val); }
break;
case 211:
#line 923 "gram.y"
	{ setfstype(&conf.cf_fstype, intern("?")); }
break;
case 212:
#line 924 "gram.y"
	{ setfstype(&conf.cf_fstype, yystack.l_mark[0].str); }
break;
case 215:
#line 935 "gram.y"
	{ setconf(&conf.cf_dump, "dumps", yystack.l_mark[0].list); }
break;
case 216:
#line 940 "gram.y"
	{ yyval.i32 = 1; }
break;
case 217:
#line 941 "gram.y"
	{ yyval.i32 = yystack.l_mark[0].i32; }
break;
case 218:
#line 946 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 219:
#line 947 "gram.y"
	{ yyval.str = starref(yystack.l_mark[-1].str); }
break;
case 220:
#line 952 "gram.y"
	{ yyval.str = NULL; }
break;
case 221:
#line 953 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 222:
#line 954 "gram.y"
	{ yyval.str = wildref(yystack.l_mark[-1].str); }
break;
case 223:
#line 959 "gram.y"
	{ yyval.loclist = NULL; }
break;
case 224:
#line 960 "gram.y"
	{ yyval.loclist = yystack.l_mark[0].loclist; app(yystack.l_mark[0].loclist, yystack.l_mark[-1].loclist); }
break;
case 225:
#line 965 "gram.y"
	{ yyval.loclist = MK3(loc, yystack.l_mark[-1].str, NULL, 0); }
break;
case 226:
#line 966 "gram.y"
	{ yyval.loclist = namelocvals(yystack.l_mark[-1].str, yystack.l_mark[0].loclist); }
break;
case 227:
#line 971 "gram.y"
	{ yyval.i32 = 0; }
break;
case 228:
#line 972 "gram.y"
	{ yyval.i32 = yystack.l_mark[0].i32; }
break;
case 231:
#line 995 "gram.y"
	{ yyval.condexpr = MKF2(cx, or, yystack.l_mark[-2].condexpr, yystack.l_mark[0].condexpr); }
break;
case 233:
#line 1000 "gram.y"
	{ yyval.condexpr = MKF2(cx, and, yystack.l_mark[-2].condexpr, yystack.l_mark[0].condexpr); }
break;
case 235:
#line 1010 "gram.y"
	{ yyval.condexpr = yystack.l_mark[0].condexpr; }
break;
case 236:
#line 1011 "gram.y"
	{ yyval.condexpr = MKF1(cx, not, yystack.l_mark[0].condexpr); }
break;
case 237:
#line 1012 "gram.y"
	{ yyval.condexpr = yystack.l_mark[-1].condexpr; }
break;
case 238:
#line 1017 "gram.y"
	{ yyval.condexpr = MKF1(cx, atom, yystack.l_mark[0].str); }
break;
case 239:
#line 1028 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 240:
#line 1029 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 241:
#line 1034 "gram.y"
	{ yyval.str = NULL; }
break;
case 242:
#line 1035 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 243:
#line 1040 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 244:
#line 1041 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 245:
#line 1046 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 246:
#line 1047 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 247:
#line 1048 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 248:
#line 1049 "gram.y"
	{
		char bf[40];

		(void)snprintf(bf, sizeof(bf), FORMAT(yystack.l_mark[0].num), (long long)yystack.l_mark[0].num.val);
		yyval.str = intern(bf);
	  }
break;
case 249:
#line 1059 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 250:
#line 1060 "gram.y"
	{ yyval.str = yystack.l_mark[0].str; }
break;
case 251:
#line 1066 "gram.y"
	{ yyval.loclist = MKF2(loc, val, yystack.l_mark[0].str, NULL); }
break;
case 252:
#line 1067 "gram.y"
	{ yyval.loclist = MKF2(loc, val, yystack.l_mark[-2].str, yystack.l_mark[0].loclist); }
break;
case 253:
#line 1072 "gram.y"
	{ yyval.num = yystack.l_mark[0].num; }
break;
case 254:
#line 1073 "gram.y"
	{ yyval.num.fmt = yystack.l_mark[0].num.fmt; yyval.num.val = -yystack.l_mark[0].num.val; }
break;
#line 2373 "gram.c"
    }
    yystack.s_mark -= yym;
    yystate = *yystack.s_mark;
    yystack.l_mark -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yystack.s_mark = YYFINAL;
        *++yystack.l_mark = yyval;
        if (yychar < 0)
        {
            if ((yychar = YYLEX) < 0) yychar = YYEOF;
#if YYDEBUG
            if (yydebug)
            {
                yys = yyname[YYTRANSLATE(yychar)];
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == YYEOF) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack) == YYENOMEM)
    {
        goto yyoverflow;
    }
    *++yystack.s_mark = (YYINT) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    YYERROR_CALL("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}

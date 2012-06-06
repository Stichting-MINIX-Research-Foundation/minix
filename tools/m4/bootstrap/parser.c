#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif
#include <stdlib.h>
#ifndef lint
#if 0
static char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#else
#if defined(__NetBSD__) && defined(__IDSTRING)
__IDSTRING(yyrcsid, "NetBSD: skeleton.c,v 1.29 2008/07/18 14:25:37 drochner Exp ");
#endif /* __NetBSD__ && __IDSTRING */
#endif /* 0 */
#endif /* lint */
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING (yyerrflag!=0)
#define YYPREFIX "yy"
#line 2 "../../../usr.bin/m4/parser.y"
/* NetBSD: parser.y,v 1.2 2009/10/26 21:11:28 christos Exp  */
/* $OpenBSD: parser.y,v 1.6 2008/08/21 21:00:14 espie Exp $ */
/*
 * Copyright (c) 2004 Marc Espie <espie@cvs.openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif
__RCSID("NetBSD: parser.y,v 1.2 2009/10/26 21:11:28 christos Exp ");
#include <stdint.h>
#define YYSTYPE	int32_t
extern int32_t end_result;
extern int yylex(void);
extern int yyerror(const char *);
#line 48 "parser.c"
#define NUMBER 257
#define ERROR 258
#define LOR 259
#define LAND 260
#define EQ 261
#define NE 262
#define LE 263
#define GE 264
#define LSHIFT 265
#define RSHIFT 266
#define UMINUS 267
#define UPLUS 268
#define YYERRCODE 256
const short yylhs[] = {                                        -1,
    0,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    1,
};
const short yylen[] = {                                         2,
    1,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    3,    3,    3,    3,    3,    3,    3,    3,    3,    3,
    2,    2,    2,    2,    1,
};
const short yydefred[] = {                                      0,
   25,    0,    0,    0,    0,    0,    0,    0,   22,   21,
   23,   24,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   20,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    4,    5,    6,
};
const short yydgoto[] = {                                       7,
    8,
};
const short yysindex[] = {                                    -13,
    0,  -13,  -13,  -13,  -13,  -13,    0,  190,    0,    0,
    0,    0,  114,  -13,  -13,  -13,  -13,  -13,  -13,  -13,
  -13,  -13,  -13,  -13,  -13,  -13,  -13,  -13,  -13,  -13,
  -13,    0,  321,  347,  159,  397,  354,  -24,  -24,  -35,
  -35,  -35,  -35,  136,  136,  -31,  -31,    0,    0,    0,
};
const short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    3,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,   65,   56,   51,   48,   74,   60,   66,   34,
   40,   76,   83,   17,   26,    1,    9,    0,    0,    0,
};
const short yygindex[] = {                                      0,
  458,
};
#define YYTABLESIZE 663
const short yytable[] = {                                       0,
    2,   31,    1,    0,    0,   31,   29,   27,    3,   28,
   29,   30,   31,    0,    0,   30,    7,   29,   27,    4,
   28,    0,   30,    0,    0,    8,    6,    0,    0,    2,
    0,    3,    0,    9,    0,   21,    0,   23,    2,   11,
    0,    2,    0,    2,    0,    2,    3,   16,    0,    3,
   17,    3,    0,    3,    7,   18,    0,    7,    0,   13,
    2,    0,    2,    8,   19,   14,    8,    0,    3,    0,
    3,    9,    0,   15,    9,   10,    7,   11,    7,    0,
   11,    0,   12,    0,    0,    8,    0,    8,   16,    0,
    0,   17,    0,    9,    2,    9,   18,   13,    0,   11,
   13,   11,    3,   14,    0,   19,   14,    0,    0,    0,
    7,   15,    5,   10,   15,    0,   10,    0,    0,    8,
   12,    0,    0,   12,    2,    0,    0,    9,    0,    0,
    0,    0,    3,   11,    0,   10,    0,   10,    0,    0,
    7,   16,   12,    0,   12,    0,    0,    0,    0,    8,
   31,   18,    0,   13,   32,   29,   27,    9,   28,   14,
   30,    0,    0,   11,    0,    0,    0,   15,    0,   10,
    0,   16,   31,   21,   17,   23,   12,   29,   27,    0,
   28,    0,   30,   13,    0,    0,    0,    0,    0,   14,
    0,    0,    0,    0,    0,   31,   18,   15,    0,   10,
   29,   27,    0,   28,    0,   30,   12,   17,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   21,    0,
   23,    0,    0,    0,    0,    0,   31,   18,    0,   25,
   26,   29,   27,    0,   28,    0,   30,   16,   22,   24,
   25,   26,    0,    1,    0,    0,    0,    0,    0,   21,
    0,   23,   17,    0,    0,    0,    0,    0,    0,    2,
    2,    2,    2,    2,    2,    2,    2,    3,    3,    3,
    3,    3,    3,    3,    3,    7,    7,    7,    7,    7,
    7,    7,    7,   17,    8,    8,    8,    8,    8,    8,
    8,    8,    9,    9,    9,    9,    9,    9,   11,   11,
   11,   11,   11,   11,    0,    0,   16,   16,    0,   17,
   17,    0,    0,   16,   18,   18,    0,    0,   13,   13,
   13,   13,    0,   19,   14,   14,   14,   14,    0,    0,
    0,    0,   15,   15,   10,   10,   10,   10,   10,   10,
    0,   12,   12,   12,   12,   12,   12,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   31,   18,    0,
    0,    0,   29,   27,    0,   28,    0,   30,    0,    0,
    0,    0,   14,   15,   19,   20,   22,   24,   25,   26,
   21,    0,   23,   31,   18,    0,    0,    0,   29,   27,
   31,   28,    0,   30,    0,   29,   27,    0,   28,    0,
   30,    0,    0,    0,    0,    0,   21,    0,   23,    0,
    0,    0,    0,   21,   17,   23,    0,    0,    0,   19,
   20,   22,   24,   25,   26,    0,    0,    0,    0,    0,
    0,    0,    0,   31,   18,    0,    0,    0,   29,   27,
   17,   28,    0,   30,   16,    0,    0,    0,   14,   15,
   19,   20,   22,   24,   25,   26,   21,    0,   23,    9,
   10,   11,   12,   13,    0,    0,    0,    0,    0,    0,
   16,   33,   34,   35,   36,   37,   38,   39,   40,   41,
   42,   43,   44,   45,   46,   47,   48,   49,   50,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   15,   19,   20,   22,   24,   25,   26,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   19,   20,   22,
   24,   25,   26,    0,   19,   20,   22,   24,   25,   26,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,   19,   20,   22,
   24,   25,   26,
};
const short yycheck[] = {                                      -1,
    0,   37,    0,   -1,   -1,   37,   42,   43,    0,   45,
   42,   47,   37,   -1,   -1,   47,    0,   42,   43,   33,
   45,   -1,   47,   -1,   -1,    0,   40,   -1,   -1,   43,
   -1,   45,   -1,    0,   -1,   60,   -1,   62,   38,    0,
   -1,   41,   -1,   43,   -1,   45,   38,    0,   -1,   41,
    0,   43,   -1,   45,   38,    0,   -1,   41,   -1,    0,
   60,   -1,   62,   38,    0,    0,   41,   -1,   60,   -1,
   62,   38,   -1,    0,   41,    0,   60,   38,   62,   -1,
   41,   -1,    0,   -1,   -1,   60,   -1,   62,   41,   -1,
   -1,   41,   -1,   60,   94,   62,   41,   38,   -1,   60,
   41,   62,   94,   38,   -1,   41,   41,   -1,   -1,   -1,
   94,   38,  126,   38,   41,   -1,   41,   -1,   -1,   94,
   38,   -1,   -1,   41,  124,   -1,   -1,   94,   -1,   -1,
   -1,   -1,  124,   94,   -1,   60,   -1,   62,   -1,   -1,
  124,   94,   60,   -1,   62,   -1,   -1,   -1,   -1,  124,
   37,   38,   -1,   94,   41,   42,   43,  124,   45,   94,
   47,   -1,   -1,  124,   -1,   -1,   -1,   94,   -1,   94,
   -1,  124,   37,   60,  124,   62,   94,   42,   43,   -1,
   45,   -1,   47,  124,   -1,   -1,   -1,   -1,   -1,  124,
   -1,   -1,   -1,   -1,   -1,   37,   38,  124,   -1,  124,
   42,   43,   -1,   45,   -1,   47,  124,   94,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   60,   -1,
   62,   -1,   -1,   -1,   -1,   -1,   37,   38,   -1,  265,
  266,   42,   43,   -1,   45,   -1,   47,  124,  263,  264,
  265,  266,   -1,  257,   -1,   -1,   -1,   -1,   -1,   60,
   -1,   62,   94,   -1,   -1,   -1,   -1,   -1,   -1,  259,
  260,  261,  262,  263,  264,  265,  266,  259,  260,  261,
  262,  263,  264,  265,  266,  259,  260,  261,  262,  263,
  264,  265,  266,   94,  259,  260,  261,  262,  263,  264,
  265,  266,  259,  260,  261,  262,  263,  264,  259,  260,
  261,  262,  263,  264,   -1,   -1,  259,  260,   -1,  259,
  260,   -1,   -1,  124,  259,  260,   -1,   -1,  259,  260,
  261,  262,   -1,  259,  259,  260,  261,  262,   -1,   -1,
   -1,   -1,  259,  260,  259,  260,  261,  262,  263,  264,
   -1,  259,  260,  261,  262,  263,  264,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   37,   38,   -1,
   -1,   -1,   42,   43,   -1,   45,   -1,   47,   -1,   -1,
   -1,   -1,  259,  260,  261,  262,  263,  264,  265,  266,
   60,   -1,   62,   37,   38,   -1,   -1,   -1,   42,   43,
   37,   45,   -1,   47,   -1,   42,   43,   -1,   45,   -1,
   47,   -1,   -1,   -1,   -1,   -1,   60,   -1,   62,   -1,
   -1,   -1,   -1,   60,   94,   62,   -1,   -1,   -1,  261,
  262,  263,  264,  265,  266,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   37,   38,   -1,   -1,   -1,   42,   43,
   94,   45,   -1,   47,  124,   -1,   -1,   -1,  259,  260,
  261,  262,  263,  264,  265,  266,   60,   -1,   62,    2,
    3,    4,    5,    6,   -1,   -1,   -1,   -1,   -1,   -1,
  124,   14,   15,   16,   17,   18,   19,   20,   21,   22,
   23,   24,   25,   26,   27,   28,   29,   30,   31,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  260,  261,  262,  263,  264,  265,  266,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  261,  262,  263,
  264,  265,  266,   -1,  261,  262,  263,  264,  265,  266,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,  261,  262,  263,
  264,  265,  266,
};
#define YYFINAL 7
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 268
#if YYDEBUG
const char * const yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
"'!'",0,0,0,"'%'","'&'",0,"'('","')'","'*'","'+'",0,"'-'",0,"'/'",0,0,0,0,0,0,0,
0,0,0,0,0,"'<'",0,"'>'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,"'^'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'|'",0,
"'~'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,"NUMBER","ERROR","LOR","LAND","EQ","NE","LE","GE",
"LSHIFT","RSHIFT","UMINUS","UPLUS",
};
const char * const yyrule[] = {
"$accept : top",
"top : expr",
"expr : expr '+' expr",
"expr : expr '-' expr",
"expr : expr '*' expr",
"expr : expr '/' expr",
"expr : expr '%' expr",
"expr : expr LSHIFT expr",
"expr : expr RSHIFT expr",
"expr : expr '<' expr",
"expr : expr '>' expr",
"expr : expr LE expr",
"expr : expr GE expr",
"expr : expr EQ expr",
"expr : expr NE expr",
"expr : expr '&' expr",
"expr : expr '^' expr",
"expr : expr '|' expr",
"expr : expr LAND expr",
"expr : expr LOR expr",
"expr : '(' expr ')'",
"expr : '-' expr",
"expr : '+' expr",
"expr : '!' expr",
"expr : '~' expr",
"expr : NUMBER",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
static YYSTYPE yyvalzero;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
int yyparse(void);
#line 85 "../../../usr.bin/m4/parser.y"

#line 315 "parser.c"
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack(void);
static int yygrowstack(void)
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    if ((newss = (short *)realloc(yyss, newsize * sizeof *newss)) == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    if ((newvs = (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs)) == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse(void)
{
    int yym, yyn, yystate;
#if YYDEBUG
    const char *yys;

    if ((yys = getenv("YYDEBUG")) != NULL)
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
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
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
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
    goto yynewerror;
yynewerror:
    yyerror("syntax error");
    goto yyerrlab;
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
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
        yyval = yyvsp[1-yym];
    else
        yyval = yyvalzero;
    switch (yyn)
    {
case 1:
#line 45 "../../../usr.bin/m4/parser.y"
{ end_result = yyvsp[0]; }
break;
case 2:
#line 47 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] + yyvsp[0]; }
break;
case 3:
#line 48 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] - yyvsp[0]; }
break;
case 4:
#line 49 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] * yyvsp[0]; }
break;
case 5:
#line 50 "../../../usr.bin/m4/parser.y"
{
		if (yyvsp[0] == 0) {
			yyerror("division by zero");
			exit(1);
		}
		yyval = yyvsp[-2] / yyvsp[0];
	}
break;
case 6:
#line 57 "../../../usr.bin/m4/parser.y"
{ 
		if (yyvsp[0] == 0) {
			yyerror("modulo zero");
			exit(1);
		}
		yyval = yyvsp[-2] % yyvsp[0];
	}
break;
case 7:
#line 64 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] << yyvsp[0]; }
break;
case 8:
#line 65 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] >> yyvsp[0]; }
break;
case 9:
#line 66 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] < yyvsp[0]; }
break;
case 10:
#line 67 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] > yyvsp[0]; }
break;
case 11:
#line 68 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] <= yyvsp[0]; }
break;
case 12:
#line 69 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] >= yyvsp[0]; }
break;
case 13:
#line 70 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] == yyvsp[0]; }
break;
case 14:
#line 71 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] != yyvsp[0]; }
break;
case 15:
#line 72 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] & yyvsp[0]; }
break;
case 16:
#line 73 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] ^ yyvsp[0]; }
break;
case 17:
#line 74 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] | yyvsp[0]; }
break;
case 18:
#line 75 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] && yyvsp[0]; }
break;
case 19:
#line 76 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-2] || yyvsp[0]; }
break;
case 20:
#line 77 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[-1]; }
break;
case 21:
#line 78 "../../../usr.bin/m4/parser.y"
{ yyval = -yyvsp[0]; }
break;
case 22:
#line 79 "../../../usr.bin/m4/parser.y"
{ yyval = yyvsp[0]; }
break;
case 23:
#line 80 "../../../usr.bin/m4/parser.y"
{ yyval = !yyvsp[0]; }
break;
case 24:
#line 81 "../../../usr.bin/m4/parser.y"
{ yyval = ~yyvsp[0]; }
break;
#line 591 "parser.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
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
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}

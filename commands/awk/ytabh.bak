/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     FIRSTTOKEN = 258,
     PROGRAM = 259,
     PASTAT = 260,
     PASTAT2 = 261,
     XBEGIN = 262,
     XEND = 263,
     NL = 264,
     ARRAY = 265,
     MATCH = 266,
     NOTMATCH = 267,
     MATCHOP = 268,
     FINAL = 269,
     DOT = 270,
     ALL = 271,
     CCL = 272,
     NCCL = 273,
     CHAR = 274,
     OR = 275,
     STAR = 276,
     QUEST = 277,
     PLUS = 278,
     EMPTYRE = 279,
     AND = 280,
     BOR = 281,
     APPEND = 282,
     EQ = 283,
     GE = 284,
     GT = 285,
     LE = 286,
     LT = 287,
     NE = 288,
     IN = 289,
     ARG = 290,
     BLTIN = 291,
     BREAK = 292,
     CLOSE = 293,
     CONTINUE = 294,
     DELETE = 295,
     DO = 296,
     EXIT = 297,
     FOR = 298,
     FUNC = 299,
     SUB = 300,
     GSUB = 301,
     IF = 302,
     INDEX = 303,
     LSUBSTR = 304,
     MATCHFCN = 305,
     NEXT = 306,
     NEXTFILE = 307,
     ADD = 308,
     MINUS = 309,
     MULT = 310,
     DIVIDE = 311,
     MOD = 312,
     ASSIGN = 313,
     ASGNOP = 314,
     ADDEQ = 315,
     SUBEQ = 316,
     MULTEQ = 317,
     DIVEQ = 318,
     MODEQ = 319,
     POWEQ = 320,
     PRINT = 321,
     PRINTF = 322,
     SPRINTF = 323,
     ELSE = 324,
     INTEST = 325,
     CONDEXPR = 326,
     POSTINCR = 327,
     PREINCR = 328,
     POSTDECR = 329,
     PREDECR = 330,
     VAR = 331,
     IVAR = 332,
     VARNF = 333,
     CALL = 334,
     NUMBER = 335,
     STRING = 336,
     REGEXPR = 337,
     GETLINE = 338,
     SUBSTR = 339,
     SPLIT = 340,
     RETURN = 341,
     WHILE = 342,
     CAT = 343,
     UMINUS = 344,
     NOT = 345,
     POWER = 346,
     INCR = 347,
     DECR = 348,
     INDIRECT = 349,
     LASTTOKEN = 350
   };
#endif
/* Tokens.  */
#define FIRSTTOKEN 258
#define PROGRAM 259
#define PASTAT 260
#define PASTAT2 261
#define XBEGIN 262
#define XEND 263
#define NL 264
#define ARRAY 265
#define MATCH 266
#define NOTMATCH 267
#define MATCHOP 268
#define FINAL 269
#define DOT 270
#define ALL 271
#define CCL 272
#define NCCL 273
#define CHAR 274
#define OR 275
#define STAR 276
#define QUEST 277
#define PLUS 278
#define EMPTYRE 279
#define AND 280
#define BOR 281
#define APPEND 282
#define EQ 283
#define GE 284
#define GT 285
#define LE 286
#define LT 287
#define NE 288
#define IN 289
#define ARG 290
#define BLTIN 291
#define BREAK 292
#define CLOSE 293
#define CONTINUE 294
#define DELETE 295
#define DO 296
#define EXIT 297
#define FOR 298
#define FUNC 299
#define SUB 300
#define GSUB 301
#define IF 302
#define INDEX 303
#define LSUBSTR 304
#define MATCHFCN 305
#define NEXT 306
#define NEXTFILE 307
#define ADD 308
#define MINUS 309
#define MULT 310
#define DIVIDE 311
#define MOD 312
#define ASSIGN 313
#define ASGNOP 314
#define ADDEQ 315
#define SUBEQ 316
#define MULTEQ 317
#define DIVEQ 318
#define MODEQ 319
#define POWEQ 320
#define PRINT 321
#define PRINTF 322
#define SPRINTF 323
#define ELSE 324
#define INTEST 325
#define CONDEXPR 326
#define POSTINCR 327
#define PREINCR 328
#define POSTDECR 329
#define PREDECR 330
#define VAR 331
#define IVAR 332
#define VARNF 333
#define CALL 334
#define NUMBER 335
#define STRING 336
#define REGEXPR 337
#define GETLINE 338
#define SUBSTR 339
#define SPLIT 340
#define RETURN 341
#define WHILE 342
#define CAT 343
#define UMINUS 344
#define NOT 345
#define POWER 346
#define INCR 347
#define DECR 348
#define INDIRECT 349
#define LASTTOKEN 350




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 41 "awkgram.y"
{
	Node	*p;
	Cell	*cp;
	int	i;
	char	*s;
}
/* Line 1529 of yacc.c.  */
#line 246 "y.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;


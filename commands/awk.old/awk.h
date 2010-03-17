/*
 * a small awk clone
 *
 * (C) 1989 Saeko Hirabauashi & Kouichi Hirabayashi
 *
 * Absolutely no warranty. Use this software with your own risk.
 *
 * Permission to use, copy, modify and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright and disclaimer notice.
 *
 * This program was written to fit into 64K+64K memory of the Minix 1.2.
 */

/* lexical/parser tokens and executable statements */

#define FIRSTP	256
#define ARG	256
#define ARITH	257
#define ARRAY	258
#define ASSIGN	259
#define CALL	260
#define CAT	261
#define COND	262
#define DELETE	263
#define DO	264
#define ELEMENT	265
#define FIELD	266
#define FOR	267
#define FORIN	268
#define GETLINE	269
#define IF	270
#define IN	271
#define JUMP	272
#define MATHFUN	273
#define NULPROC	274
#define P1STAT	275
#define P2STAT	276
#define PRINT	277
#define PRINT0	278
#define STRFUN	279
#define SUBST	280
#define USRFUN	281
#define WHILE	282
#define LASTP	282
	/* lexical token */

#define ADD	300	/* + */
#define ADDEQ	301	/* += */
#define AND	302	/* && */
#define BEGIN	303	/* BEGIN */
#define BINAND	304	/* & */
#define BINOR	305	/* | */
#define BREAK	306	/* break */
#define CLOSE	307	/* close */
#define CONTIN	308	/* continue */
#define DEC	309	/* -- */
#define DIV	310	/* / */
#define DIVEQ	311	/* /= */
#define	ELSE	312	/* else */
#define END	313	/* END */
#define EOL	314	/* ; or '\n' */
#define EQ	315	/* == */
#define EXIT	316	/* exit */
#define FUNC	317	/* function */
#define GE	318	/* >= */
#define GT	319	/* > */
#define IDENT	320	/* identifier */
#define INC	321	/* ++ */
#define LE	322	/* <= */
#define LT	323	/* < */
#define MATCH	324	/* ~ */
#define MOD	325	/* % */
#define MODEQ	326	/* %= */
#define MULT	327	/* * */
#define MULTEQ	328	/* *= */
#define NE	329	/* != */
#define NEXT	330	/* next */
#define NOMATCH	331	/* !~ */
#define NOT	332	/* ! */
#define NUMBER	333	/* integer or floating number */
#define OR	334	/* || */
#define POWEQ	335	/* ^= */
#define POWER	336	/* ^ */
#define PRINTF	337	/* printf */
#define REGEXP	338	/* /REG/ */
#define RETURN	339	/* return */
#define SHIFTL	340	/* << */
#define SHIFTR	341	/* >> */
#define SPRINT	342	/* sprint */
#define SPRINTF	343	/* sprintf */
#define STRING	344	/* ".." */
#define SUB	345	/* - */
#define SUBEQ	346	/* -= */
#define SYSTEM	347	/* system */
#define UMINUS	348	/* - */

/* tokens in parser */

#define VALUE	400	/* value node */
#define INCDEC	401	/* ++, -- */
#define PRE	402	/* pre incre/decre */
#define POST	403	/* post incre/decre */

/* redirect in print(f) statement */

#define R_OUT	410	/* > */
#define R_APD	411	/* >> */
#define R_PIPE	412	/* | */
#define R_IN	413	/* < */
#define R_PIN	414	/* | getline */
#define R_POUT	415	/* print | */

/* function */

#define ATAN2	500	/* atan2 */
#define COS	501	/* cos */
#define EXP	502	/* exp */
#define INDEX	503	/* index */
#define INT	504	/* int */
#define LENGTH	505	/* length */
#define LOG	506	/* log */
#define RAND	507	/* rand */
#define RGSUB	508	/* gsub */
#define RMATCH	509	/* match */
#define RSUB	510	/* sub */
#define SIN	511	/* sin */
#define SPLIT	512	/* split */
#define SQRT	513	/* sqrt */
#define SRAND	514	/* srand */
#define SUBSTR	515	/* substr */

/* print(f) options */

#define FORMAT	1024	/* PRINTF, SPRINTF */
#define STROUT	2048	/* SPRINTF */
#define PRMASK	0x3ff	/* ~(FORMAT|STROUT) */

	/* node - used in parsed tree */

struct node {
  int n_type;			/* node type */
  struct node *n_next;		/* pointer to next node */
  struct node *n_arg[1];	/* argument (variable length) */
};

typedef struct node NODE;

	/* object cell */

struct cell {
  int c_type;		/* cell type */
  char *c_sval;		/* string value */
  double c_fval;	/* floating value */
};

typedef struct cell CELL;

	/* cell type */

#define UDF	0	/* pass parameter */
#define VAR	1	/* variable */
#define NUM	2	/* number */
#define ARR	4	/* array */
#define STR	8	/* string */
#define REC	16	/* record */
#define FLD	32	/* filed */
#define PAT	64	/* pattern (compiled REGEXPR) */
#define BRK	128	/* break */
#define CNT	256	/* continue */
#define NXT	512	/* next */
#define EXT	1024	/* exit */
#define RTN	2048	/* return */
#define TMP	4096	/* temp cell */
#define POS	8192	/* argument position */
#define FUN	16384	/* function */

	/* symbol cell - linked to symbol table */

struct symbol {
  char *s_name;
  CELL *s_val;
  struct symbol *s_next;
};

typedef struct symbol SYMBOL;

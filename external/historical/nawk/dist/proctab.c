#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <stdio.h>
#include "awk.h"
#include "awkgram.h"

static const char * const printname[94] = {
	"FIRSTTOKEN",	/* 57346 */
	"PROGRAM",	/* 57347 */
	"PASTAT",	/* 57348 */
	"PASTAT2",	/* 57349 */
	"XBEGIN",	/* 57350 */
	"XEND",	/* 57351 */
	"NL",	/* 57352 */
	"ARRAY",	/* 57353 */
	"MATCH",	/* 57354 */
	"NOTMATCH",	/* 57355 */
	"MATCHOP",	/* 57356 */
	"FINAL",	/* 57357 */
	"DOT",	/* 57358 */
	"ALL",	/* 57359 */
	"CCL",	/* 57360 */
	"NCCL",	/* 57361 */
	"CHAR",	/* 57362 */
	"OR",	/* 57363 */
	"STAR",	/* 57364 */
	"QUEST",	/* 57365 */
	"PLUS",	/* 57366 */
	"EMPTYRE",	/* 57367 */
	"AND",	/* 57368 */
	"BOR",	/* 57369 */
	"APPEND",	/* 57370 */
	"EQ",	/* 57371 */
	"GE",	/* 57372 */
	"GT",	/* 57373 */
	"LE",	/* 57374 */
	"LT",	/* 57375 */
	"NE",	/* 57376 */
	"IN",	/* 57377 */
	"ARG",	/* 57378 */
	"BLTIN",	/* 57379 */
	"BREAK",	/* 57380 */
	"CLOSE",	/* 57381 */
	"CONTINUE",	/* 57382 */
	"DELETE",	/* 57383 */
	"DO",	/* 57384 */
	"EXIT",	/* 57385 */
	"FOR",	/* 57386 */
	"FUNC",	/* 57387 */
	"SUB",	/* 57388 */
	"GSUB",	/* 57389 */
	"IF",	/* 57390 */
	"INDEX",	/* 57391 */
	"LSUBSTR",	/* 57392 */
	"MATCHFCN",	/* 57393 */
	"NEXT",	/* 57394 */
	"NEXTFILE",	/* 57395 */
	"ADD",	/* 57396 */
	"MINUS",	/* 57397 */
	"MULT",	/* 57398 */
	"DIVIDE",	/* 57399 */
	"MOD",	/* 57400 */
	"ASSIGN",	/* 57401 */
	"ASGNOP",	/* 57402 */
	"ADDEQ",	/* 57403 */
	"SUBEQ",	/* 57404 */
	"MULTEQ",	/* 57405 */
	"DIVEQ",	/* 57406 */
	"MODEQ",	/* 57407 */
	"POWEQ",	/* 57408 */
	"PRINT",	/* 57409 */
	"PRINTF",	/* 57410 */
	"SPRINTF",	/* 57411 */
	"ELSE",	/* 57412 */
	"INTEST",	/* 57413 */
	"CONDEXPR",	/* 57414 */
	"POSTINCR",	/* 57415 */
	"PREINCR",	/* 57416 */
	"POSTDECR",	/* 57417 */
	"PREDECR",	/* 57418 */
	"VAR",	/* 57419 */
	"IVAR",	/* 57420 */
	"VARNF",	/* 57421 */
	"CALL",	/* 57422 */
	"NUMBER",	/* 57423 */
	"STRING",	/* 57424 */
	"REGEXPR",	/* 57425 */
	"GETLINE",	/* 57426 */
	"GENSUB",	/* 57427 */
	"RETURN",	/* 57428 */
	"SPLIT",	/* 57429 */
	"SUBSTR",	/* 57430 */
	"WHILE",	/* 57431 */
	"CAT",	/* 57432 */
	"NOT",	/* 57433 */
	"UMINUS",	/* 57434 */
	"POWER",	/* 57435 */
	"DECR",	/* 57436 */
	"INCR",	/* 57437 */
	"INDIRECT",	/* 57438 */
	"LASTTOKEN",	/* 57439 */
};


Cell *(*proctab[94])(Node **, int) = {
	nullproc,	/* FIRSTTOKEN */
	program,	/* PROGRAM */
	pastat,	/* PASTAT */
	dopa2,	/* PASTAT2 */
	nullproc,	/* XBEGIN */
	nullproc,	/* XEND */
	nullproc,	/* NL */
	array,	/* ARRAY */
	matchop,	/* MATCH */
	matchop,	/* NOTMATCH */
	nullproc,	/* MATCHOP */
	nullproc,	/* FINAL */
	nullproc,	/* DOT */
	nullproc,	/* ALL */
	nullproc,	/* CCL */
	nullproc,	/* NCCL */
	nullproc,	/* CHAR */
	nullproc,	/* OR */
	nullproc,	/* STAR */
	nullproc,	/* QUEST */
	nullproc,	/* PLUS */
	nullproc,	/* EMPTYRE */
	boolop,	/* AND */
	boolop,	/* BOR */
	nullproc,	/* APPEND */
	relop,	/* EQ */
	relop,	/* GE */
	relop,	/* GT */
	relop,	/* LE */
	relop,	/* LT */
	relop,	/* NE */
	instat,	/* IN */
	arg,	/* ARG */
	bltin,	/* BLTIN */
	jump,	/* BREAK */
	closefile,	/* CLOSE */
	jump,	/* CONTINUE */
	awkdelete,	/* DELETE */
	dostat,	/* DO */
	jump,	/* EXIT */
	forstat,	/* FOR */
	nullproc,	/* FUNC */
	sub,	/* SUB */
	gsub,	/* GSUB */
	ifstat,	/* IF */
	sindex,	/* INDEX */
	nullproc,	/* LSUBSTR */
	matchop,	/* MATCHFCN */
	jump,	/* NEXT */
	jump,	/* NEXTFILE */
	arith,	/* ADD */
	arith,	/* MINUS */
	arith,	/* MULT */
	arith,	/* DIVIDE */
	arith,	/* MOD */
	assign,	/* ASSIGN */
	nullproc,	/* ASGNOP */
	assign,	/* ADDEQ */
	assign,	/* SUBEQ */
	assign,	/* MULTEQ */
	assign,	/* DIVEQ */
	assign,	/* MODEQ */
	assign,	/* POWEQ */
	printstat,	/* PRINT */
	awkprintf,	/* PRINTF */
	awksprintf,	/* SPRINTF */
	nullproc,	/* ELSE */
	intest,	/* INTEST */
	condexpr,	/* CONDEXPR */
	incrdecr,	/* POSTINCR */
	incrdecr,	/* PREINCR */
	incrdecr,	/* POSTDECR */
	incrdecr,	/* PREDECR */
	nullproc,	/* VAR */
	nullproc,	/* IVAR */
	getnf,	/* VARNF */
	call,	/* CALL */
	nullproc,	/* NUMBER */
	nullproc,	/* STRING */
	nullproc,	/* REGEXPR */
	awkgetline,	/* GETLINE */
	gensub,	/* GENSUB */
	jump,	/* RETURN */
	split,	/* SPLIT */
	substr,	/* SUBSTR */
	whilestat,	/* WHILE */
	cat,	/* CAT */
	boolop,	/* NOT */
	arith,	/* UMINUS */
	arith,	/* POWER */
	nullproc,	/* DECR */
	nullproc,	/* INCR */
	indirect,	/* INDIRECT */
	nullproc,	/* LASTTOKEN */
};

const char *tokname(int n)
{
	static char buf[100];

	if (n < FIRSTTOKEN || n > LASTTOKEN) {
		snprintf(buf, sizeof(buf), "token %d", n);
		return buf;
	}
	return printname[n-FIRSTTOKEN];
}

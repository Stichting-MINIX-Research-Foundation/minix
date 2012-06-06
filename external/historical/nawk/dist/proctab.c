#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <stdio.h>
#include "awk.h"
#include "awkgram.h"

static const char * const printname[94] = {
	"FIRSTTOKEN",	/* 257 */
	"PROGRAM",	/* 258 */
	"PASTAT",	/* 259 */
	"PASTAT2",	/* 260 */
	"XBEGIN",	/* 261 */
	"XEND",	/* 262 */
	"NL",	/* 263 */
	"ARRAY",	/* 264 */
	"MATCH",	/* 265 */
	"NOTMATCH",	/* 266 */
	"MATCHOP",	/* 267 */
	"FINAL",	/* 268 */
	"DOT",	/* 269 */
	"ALL",	/* 270 */
	"CCL",	/* 271 */
	"NCCL",	/* 272 */
	"CHAR",	/* 273 */
	"OR",	/* 274 */
	"STAR",	/* 275 */
	"QUEST",	/* 276 */
	"PLUS",	/* 277 */
	"EMPTYRE",	/* 278 */
	"AND",	/* 279 */
	"BOR",	/* 280 */
	"APPEND",	/* 281 */
	"EQ",	/* 282 */
	"GE",	/* 283 */
	"GT",	/* 284 */
	"LE",	/* 285 */
	"LT",	/* 286 */
	"NE",	/* 287 */
	"IN",	/* 288 */
	"ARG",	/* 289 */
	"BLTIN",	/* 290 */
	"BREAK",	/* 291 */
	"CLOSE",	/* 292 */
	"CONTINUE",	/* 293 */
	"DELETE",	/* 294 */
	"DO",	/* 295 */
	"EXIT",	/* 296 */
	"FOR",	/* 297 */
	"FUNC",	/* 298 */
	"SUB",	/* 299 */
	"GSUB",	/* 300 */
	"IF",	/* 301 */
	"INDEX",	/* 302 */
	"LSUBSTR",	/* 303 */
	"MATCHFCN",	/* 304 */
	"NEXT",	/* 305 */
	"NEXTFILE",	/* 306 */
	"ADD",	/* 307 */
	"MINUS",	/* 308 */
	"MULT",	/* 309 */
	"DIVIDE",	/* 310 */
	"MOD",	/* 311 */
	"ASSIGN",	/* 312 */
	"ASGNOP",	/* 313 */
	"ADDEQ",	/* 314 */
	"SUBEQ",	/* 315 */
	"MULTEQ",	/* 316 */
	"DIVEQ",	/* 317 */
	"MODEQ",	/* 318 */
	"POWEQ",	/* 319 */
	"PRINT",	/* 320 */
	"PRINTF",	/* 321 */
	"SPRINTF",	/* 322 */
	"ELSE",	/* 323 */
	"INTEST",	/* 324 */
	"CONDEXPR",	/* 325 */
	"POSTINCR",	/* 326 */
	"PREINCR",	/* 327 */
	"POSTDECR",	/* 328 */
	"PREDECR",	/* 329 */
	"VAR",	/* 330 */
	"IVAR",	/* 331 */
	"VARNF",	/* 332 */
	"CALL",	/* 333 */
	"NUMBER",	/* 334 */
	"STRING",	/* 335 */
	"REGEXPR",	/* 336 */
	"GETLINE",	/* 337 */
	"GENSUB",	/* 338 */
	"RETURN",	/* 339 */
	"SPLIT",	/* 340 */
	"SUBSTR",	/* 341 */
	"WHILE",	/* 342 */
	"CAT",	/* 343 */
	"NOT",	/* 344 */
	"UMINUS",	/* 345 */
	"POWER",	/* 346 */
	"DECR",	/* 347 */
	"INCR",	/* 348 */
	"INDIRECT",	/* 349 */
	"LASTTOKEN",	/* 350 */
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

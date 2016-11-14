#ifndef _yy_defines_h_
#define _yy_defines_h_

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
extern YYSTYPE yylval;

#endif /* _yy_defines_h_ */

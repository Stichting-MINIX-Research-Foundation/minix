#ifndef lint
static const char yysccsid[] = "@(#)yaccpar	1.9 (Berkeley) 02/21/93";
#endif

#ifdef _LIBC
#include "namespace.h"
#endif
#include <stdlib.h>
#include <string.h>

#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9

#define YYEMPTY        (-1)
#define yyclearin      (yychar = YYEMPTY)
#define yyerrok        (yyerrflag = 0)
#define YYRECOVERING() (yyerrflag != 0)

#define YYPREFIX "yy"

#define YYPURE 0

#line 69 "ftpcmd.y"
#include <sys/cdefs.h>

#ifndef lint
#if 0
static char sccsid[] = "@(#)ftpcmd.y	8.3 (Berkeley) 4/6/94";
#else
__RCSID("$NetBSD: ftpcmd.y,v 1.93 2011/09/16 16:13:17 plunky Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <netdb.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"
#include "version.h"

static	int cmd_type;
static	int cmd_form;
static	int cmd_bytesz;

char	cbuf[FTP_BUFLEN];
char	*cmdp;
char	*fromname;

extern int	epsvall;
struct tab	sitetab[];

static	int	check_write(const char *, int);
static	void	help(struct tab *, const char *);
static	void	port_check(const char *, int);
	int	yylex(void);

#line 124 "ftpcmd.y"
#ifdef YYSTYPE
#undef  YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
#endif
#ifndef YYSTYPE_IS_DECLARED
#define YYSTYPE_IS_DECLARED 1
typedef union {
	struct {
		LLT	ll;
		int	i;
	} u;
	char *s;
	const char *cs;
} YYSTYPE;
#endif /* !YYSTYPE_IS_DECLARED */
#line 94 "ftpcmd.c"

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
#define YYERROR_DECL() yyerror(const char *s)
#define YYERROR_CALL(msg) yyerror(msg)

extern int YYPARSE_DECL();


#define A 257
#define B 258
#define C 259
#define E 260
#define F 261
#define I 262
#define L 263
#define N 264
#define P 265
#define R 266
#define S 267
#define T 268
#define SP 269
#define CRLF 270
#define COMMA 271
#define ALL 272
#define USER 273
#define PASS 274
#define ACCT 275
#define CWD 276
#define CDUP 277
#define SMNT 278
#define QUIT 279
#define REIN 280
#define PORT 281
#define PASV 282
#define TYPE 283
#define STRU 284
#define MODE 285
#define RETR 286
#define STOR 287
#define STOU 288
#define APPE 289
#define ALLO 290
#define REST 291
#define RNFR 292
#define RNTO 293
#define ABOR 294
#define DELE 295
#define RMD 296
#define MKD 297
#define PWD 298
#define LIST 299
#define NLST 300
#define SITE 301
#define SYST 302
#define STAT 303
#define HELP 304
#define NOOP 305
#define AUTH 306
#define ADAT 307
#define PROT 308
#define PBSZ 309
#define CCC 310
#define MIC 311
#define CONF 312
#define ENC 313
#define FEAT 314
#define OPTS 315
#define SIZE 316
#define MDTM 317
#define MLST 318
#define MLSD 319
#define LPRT 320
#define LPSV 321
#define EPRT 322
#define EPSV 323
#define MAIL 324
#define MLFL 325
#define MRCP 326
#define MRSQ 327
#define MSAM 328
#define MSND 329
#define MSOM 330
#define CHMOD 331
#define IDLE 332
#define RATEGET 333
#define RATEPUT 334
#define UMASK 335
#define LEXERR 336
#define STRING 337
#define NUMBER 338
#define YYERRCODE 256
static const short yylhs[] = {                           -1,
    0,    0,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   16,   16,   16,   16,   16,   16,   16,   16,   16,   16,
   17,   17,   12,   11,   11,    3,   18,   19,   20,    7,
    7,    7,    6,    6,    6,    6,    6,    6,    6,    6,
    4,    4,    4,    5,    5,    5,   10,    9,    2,   13,
   14,   15,    8,    1,
};
static const short yylen[] = {                            2,
    1,    1,    4,    4,    3,    5,    3,    2,    5,    5,
    5,    5,    3,    3,    5,    5,    3,    5,    5,    5,
    5,    4,    4,    4,    5,    9,    4,    3,    4,    4,
    4,    3,    3,    5,    3,    5,    4,    8,    6,    5,
    7,    5,    7,    5,    7,    5,    7,    2,    5,    2,
    2,    4,    2,    4,    4,    4,    4,    2,    4,    4,
    4,    2,    4,    5,    5,    5,    3,    5,    3,    2,
    5,    4,    1,    0,    1,    1,   11,   17,   41,    1,
    1,    1,    1,    3,    1,    3,    1,    1,    3,    2,
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    0,
};
static const short yydefred[] = {                         0,
    0,    0,    0,  104,  104,    0,  104,  104,  104,  104,
  104,  104,    0,    0,    0,  104,  104,    0,    0,  104,
    0,    0,    0,  104,  104,  104,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,  104,  104,  104,  104,  104,  104,  104,  104,    0,
    1,    2,   70,    0,    0,    0,    0,    8,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,   48,
   50,    0,    0,   51,   53,    0,    0,    0,    0,   58,
    0,    0,    0,   62,    0,    0,    0,    0,    0,    0,
    0,    0,    0,   73,    0,   75,    0,    0,    5,    7,
    0,   13,    0,    0,    0,    0,   98,   97,    0,    0,
    0,    0,    0,    0,    0,   28,    0,    0,    0,   32,
    0,   33,    0,   35,    0,    0,  104,  104,  104,  104,
    0,    0,  100,    0,  101,    0,  102,    0,  103,    0,
    0,    0,    0,    0,    0,    0,    0,   67,    0,   69,
    0,   14,    0,    0,   17,    3,    4,    0,    0,    0,
    0,    0,   87,    0,    0,   91,   93,   92,    0,   95,
   96,   94,    0,    0,   22,   23,   24,    0,    0,   72,
   27,   29,   30,   31,    0,    0,    0,   37,    0,    0,
    0,    0,    0,    0,   52,   54,   55,   56,   57,   59,
   60,   61,   63,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    6,    0,    9,    0,    0,    0,   76,
   90,   18,   19,   20,   21,    0,   25,   71,   34,   36,
    0,   99,    0,    0,   40,    0,   42,    0,   44,    0,
   46,   49,   64,   65,   66,   68,    0,   10,   11,   12,
   16,   15,    0,   82,   80,   81,   84,   86,   89,    0,
   39,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   41,   43,   45,   47,    0,    0,    0,   38,    0,    0,
   26,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   77,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,   79,
};
static const short yydgoto[] = {                         50,
   56,  243,  231,  179,  183,  175,  267,  150,  118,  119,
  107,  105,  144,  146,  148,   51,   52,  170,  219,  220,
};
static const short yysindex[] = {                      -179,
 -260, -245, -239,    0,    0, -231,    0,    0,    0,    0,
    0,    0, -228, -225, -205,    0,    0, -203, -199,    0,
 -195, -181, -177,    0,    0,    0, -173, -194, -171, -257,
 -169, -124, -109, -108, -107, -106, -104, -103, -102, -101,
  -99,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0, -174, -166, -242,  -98,    0,  -96,  -95,
  -93,  -92,  -91,  -90, -163, -163, -163,  -89,  -88, -163,
 -163,  -87, -163, -163, -163,  -86, -233, -191, -272,    0,
    0,  -84, -155,    0,    0, -151, -150, -149, -170,    0,
 -150, -150, -150,    0, -148,  -79,  -78, -189, -187,  -77,
  -76,  -74, -185,    0,  -73,    0,  -72, -163,    0,    0,
 -145,    0, -217, -193, -236, -163,    0,    0,  -71,  -70,
  -69, -142, -136,  -67,  -65,    0,  -63,  -62,  -61,    0,
 -163,    0, -163,    0, -183,  -59,    0,    0,    0,    0,
 -163,  -58,    0,  -57,    0,  -56,    0,  -55,    0,  -54,
  -53,  -52,  -51,  -50, -163, -163, -163,    0, -163,    0,
 -134,    0, -126, -269,    0,    0,    0,  -49,  -48,  -46,
  -47,  -43,    0, -267,  -45,    0,    0,    0,  -42,    0,
    0,    0,  -41,  -40,    0,    0,    0, -119,  -39,    0,
    0,    0,    0,    0,  -38,  -37, -110,    0, -100, -117,
 -115, -113, -111,  -36,    0,    0,    0,    0,    0,    0,
    0,    0,    0,  -35,  -34,  -33,  -31,  -30,  -28,  -27,
  -26,  -25,  -24,    0,  -85,    0, -253, -253,  -83,    0,
    0,    0,    0,    0,    0,  -19,    0,    0,    0,    0,
  -22,    0,  -29,  -82,    0,  -80,    0,  -75,    0, -100,
    0,    0,    0,    0,    0,    0,  -68,    0,    0,    0,
    0,    0,  -21,    0,    0,    0,    0,    0,    0,  -20,
    0, -163,  -18,  -16,  -12,  -11,  -10,  -64,  -60,   -7,
    0,    0,    0,    0,  -32,   -6,   -4,    0,   -3,  -23,
    0,  -17,   -2,    1,  -15,  -14,    2,    4,  -13,   -9,
    0,    5,   -8,    6,   -5,    8,   -1,   10,    3,   11,
    7,   12,   13,   14,   15,   16,   17,   18,   19,   20,
   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,
   31,    0,
};
static const short yyrindex[] = {                         0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,   33,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,   34,    0,    0,    0,    0,    0,
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
   35, -169,    0,   37,    0,    0,    0,    0,    0,    0,
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
    0,    0,    0,    0,    0,    0,    0,   38,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,
};
static const short yygindex[] = {                         0,
    9,   36,   42,    0,    0,    0,   32,    0,    0,  -66,
    0,    0,    0,  -44,    0,    0,    0,    0,    0,    0,
};
#define YYTABLESIZE 369
static const short yytable[] = {                        120,
  121,  229,  222,  124,  125,  264,  127,  128,  129,   53,
  265,   83,   84,   57,  266,   59,   60,   61,   62,   63,
   64,  180,  181,   54,   68,   69,  108,  109,   72,   55,
  182,  135,   76,   77,   78,  131,  132,   82,   58,  171,
   65,  168,  172,   66,  173,  174,  151,  152,  153,  184,
   96,   97,   98,   99,  100,  101,  102,  103,  136,  137,
  138,  139,  140,   67,  195,   70,  196,  176,  223,   71,
  230,  177,  178,   73,  204,   80,    1,  133,  134,  157,
  158,  159,  160,  164,  165,  197,  198,   74,  214,  215,
  216,   75,  217,    2,    3,   79,    4,    5,   81,    6,
   85,    7,    8,    9,   10,   11,   12,   13,   14,   15,
   16,   17,   18,   19,   20,   21,   22,   23,   24,   25,
   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,
   36,   37,   38,   39,   40,   41,   42,   43,   44,   45,
   46,   47,   48,   49,   86,  200,  201,  202,  203,  236,
  237,  244,  245,  246,  247,  248,  249,  250,  251,   87,
   88,   89,  104,   90,   91,   92,   93,  149,   94,   95,
  106,  110,  111,  117,  112,  113,  114,  115,  116,  122,
  123,  142,  126,  130,  141,  143,  145,  147,  154,  155,
  156,  161,  169,  162,  163,  188,  166,  167,  185,  186,
  187,  189,  190,  218,  191,  280,  192,  193,  194,  199,
  221,  205,  206,  207,  208,  209,  210,  211,  212,  213,
  224,  227,  225,  226,  232,  228,  241,  233,  234,  235,
  238,  239,  240,  252,  253,  254,  255,  242,  256,  272,
  257,  258,  259,  260,  261,  262,  270,  271,  279,  278,
    0,  281,  263,  282,  230,  273,  274,  283,  284,  268,
  285,  275,  288,    0,  290,  291,    0,  292,  295,  277,
  269,  296,  299,  286,  300,  303,  305,  287,  307,    0,
  309,  311,  313,    0,  315,  276,  317,    0,  319,    0,
  321,    0,  323,    0,  325,    0,  327,    0,  329,    0,
  331,  104,    0,   74,   83,  289,   88,   78,    0,    0,
    0,    0,    0,    0,  293,    0,    0,    0,    0,    0,
  294,    0,  297,  298,  301,    0,    0,    0,  302,  304,
    0,    0,  306,    0,    0,    0,  308,    0,    0,    0,
  310,    0,    0,    0,  312,    0,    0,    0,    0,    0,
  314,    0,  316,    0,  318,    0,  320,    0,  322,    0,
  324,    0,  326,    0,  328,    0,  330,    0,  332,
};
static const short yycheck[] = {                         66,
   67,  269,  272,   70,   71,  259,   73,   74,   75,  270,
  264,  269,  270,    5,  268,    7,    8,    9,   10,   11,
   12,  258,  259,  269,   16,   17,  269,  270,   20,  269,
  267,  304,   24,   25,   26,  269,  270,   29,  270,  257,
  269,  108,  260,  269,  262,  263,   91,   92,   93,  116,
   42,   43,   44,   45,   46,   47,   48,   49,  331,  332,
  333,  334,  335,  269,  131,  269,  133,  261,  338,  269,
  338,  265,  266,  269,  141,  270,  256,  269,  270,  269,
  270,  269,  270,  269,  270,  269,  270,  269,  155,  156,
  157,  269,  159,  273,  274,  269,  276,  277,  270,  279,
  270,  281,  282,  283,  284,  285,  286,  287,  288,  289,
  290,  291,  292,  293,  294,  295,  296,  297,  298,  299,
  300,  301,  302,  303,  304,  305,  306,  307,  308,  309,
  310,  311,  312,  313,  314,  315,  316,  317,  318,  319,
  320,  321,  322,  323,  269,  137,  138,  139,  140,  269,
  270,  269,  270,  269,  270,  269,  270,  269,  270,  269,
  269,  269,  337,  270,  269,  269,  269,  338,  270,  269,
  337,  270,  269,  337,  270,  269,  269,  269,  269,  269,
  269,  337,  270,  270,  269,  337,  337,  337,  337,  269,
  269,  269,  338,  270,  269,  338,  270,  270,  270,  270,
  270,  338,  270,  338,  270,  272,  270,  270,  270,  269,
  337,  270,  270,  270,  270,  270,  270,  270,  270,  270,
  270,  269,  271,  270,  270,  269,  337,  270,  270,  270,
  270,  270,  270,  270,  270,  270,  270,  338,  270,  269,
  271,  270,  270,  270,  270,  270,  266,  270,  269,  271,
   -1,  270,  338,  270,  338,  338,  337,  270,  270,  228,
  271,  337,  270,   -1,  271,  270,   -1,  271,  271,  338,
  229,  271,  271,  338,  271,  271,  271,  338,  271,   -1,
  271,  271,  271,   -1,  271,  250,  271,   -1,  271,   -1,
  271,   -1,  271,   -1,  271,   -1,  271,   -1,  271,   -1,
  271,  269,   -1,  270,  270,  338,  270,  270,   -1,   -1,
   -1,   -1,   -1,   -1,  338,   -1,   -1,   -1,   -1,   -1,
  338,   -1,  338,  338,  338,   -1,   -1,   -1,  338,  338,
   -1,   -1,  338,   -1,   -1,   -1,  338,   -1,   -1,   -1,
  338,   -1,   -1,   -1,  338,   -1,   -1,   -1,   -1,   -1,
  338,   -1,  338,   -1,  338,   -1,  338,   -1,  338,   -1,
  338,   -1,  338,   -1,  338,   -1,  338,   -1,  338,
};
#define YYFINAL 50
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 338
#if YYDEBUG
static const char *yyname[] = {

"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"A","B","C","E","F","I","L","N",
"P","R","S","T","SP","CRLF","COMMA","ALL","USER","PASS","ACCT","CWD","CDUP",
"SMNT","QUIT","REIN","PORT","PASV","TYPE","STRU","MODE","RETR","STOR","STOU",
"APPE","ALLO","REST","RNFR","RNTO","ABOR","DELE","RMD","MKD","PWD","LIST",
"NLST","SITE","SYST","STAT","HELP","NOOP","AUTH","ADAT","PROT","PBSZ","CCC",
"MIC","CONF","ENC","FEAT","OPTS","SIZE","MDTM","MLST","MLSD","LPRT","LPSV",
"EPRT","EPSV","MAIL","MLFL","MRCP","MRSQ","MSAM","MSND","MSOM","CHMOD","IDLE",
"RATEGET","RATEPUT","UMASK","LEXERR","STRING","NUMBER",
};
static const char *yyrule[] = {
"$accept : cmd_sel",
"cmd_sel : cmd",
"cmd_sel : rcmd",
"cmd : USER SP username CRLF",
"cmd : PASS SP password CRLF",
"cmd : CWD check_login CRLF",
"cmd : CWD check_login SP pathname CRLF",
"cmd : CDUP check_login CRLF",
"cmd : QUIT CRLF",
"cmd : PORT check_login SP host_port CRLF",
"cmd : LPRT check_login SP host_long_port4 CRLF",
"cmd : LPRT check_login SP host_long_port6 CRLF",
"cmd : EPRT check_login SP STRING CRLF",
"cmd : PASV check_login CRLF",
"cmd : LPSV check_login CRLF",
"cmd : EPSV check_login SP NUMBER CRLF",
"cmd : EPSV check_login SP ALL CRLF",
"cmd : EPSV check_login CRLF",
"cmd : TYPE check_login SP type_code CRLF",
"cmd : STRU check_login SP struct_code CRLF",
"cmd : MODE check_login SP mode_code CRLF",
"cmd : RETR check_login SP pathname CRLF",
"cmd : STOR SP pathname CRLF",
"cmd : STOU SP pathname CRLF",
"cmd : APPE SP pathname CRLF",
"cmd : ALLO check_login SP NUMBER CRLF",
"cmd : ALLO check_login SP NUMBER SP R SP NUMBER CRLF",
"cmd : RNTO SP pathname CRLF",
"cmd : ABOR check_login CRLF",
"cmd : DELE SP pathname CRLF",
"cmd : RMD SP pathname CRLF",
"cmd : MKD SP pathname CRLF",
"cmd : PWD check_login CRLF",
"cmd : LIST check_login CRLF",
"cmd : LIST check_login SP pathname CRLF",
"cmd : NLST check_login CRLF",
"cmd : NLST check_login SP pathname CRLF",
"cmd : SITE SP HELP CRLF",
"cmd : SITE SP CHMOD SP octal_number SP pathname CRLF",
"cmd : SITE SP HELP SP STRING CRLF",
"cmd : SITE SP IDLE check_login CRLF",
"cmd : SITE SP IDLE check_login SP NUMBER CRLF",
"cmd : SITE SP RATEGET check_login CRLF",
"cmd : SITE SP RATEGET check_login SP STRING CRLF",
"cmd : SITE SP RATEPUT check_login CRLF",
"cmd : SITE SP RATEPUT check_login SP STRING CRLF",
"cmd : SITE SP UMASK check_login CRLF",
"cmd : SITE SP UMASK check_login SP octal_number CRLF",
"cmd : SYST CRLF",
"cmd : STAT check_login SP pathname CRLF",
"cmd : STAT CRLF",
"cmd : HELP CRLF",
"cmd : HELP SP STRING CRLF",
"cmd : NOOP CRLF",
"cmd : AUTH SP mechanism_name CRLF",
"cmd : ADAT SP base64data CRLF",
"cmd : PROT SP prot_code CRLF",
"cmd : PBSZ SP decimal_integer CRLF",
"cmd : CCC CRLF",
"cmd : MIC SP base64data CRLF",
"cmd : CONF SP base64data CRLF",
"cmd : ENC SP base64data CRLF",
"cmd : FEAT CRLF",
"cmd : OPTS SP STRING CRLF",
"cmd : SIZE check_login SP pathname CRLF",
"cmd : MDTM check_login SP pathname CRLF",
"cmd : MLST check_login SP pathname CRLF",
"cmd : MLST check_login CRLF",
"cmd : MLSD check_login SP pathname CRLF",
"cmd : MLSD check_login CRLF",
"cmd : error CRLF",
"rcmd : REST check_login SP NUMBER CRLF",
"rcmd : RNFR SP pathname CRLF",
"username : STRING",
"password :",
"password : STRING",
"byte_size : NUMBER",
"host_port : NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER",
"host_long_port4 : NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER",
"host_long_port6 : NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER COMMA NUMBER",
"form_code : N",
"form_code : T",
"form_code : C",
"type_code : A",
"type_code : A SP form_code",
"type_code : E",
"type_code : E SP form_code",
"type_code : I",
"type_code : L",
"type_code : L SP byte_size",
"type_code : L byte_size",
"struct_code : F",
"struct_code : R",
"struct_code : P",
"mode_code : S",
"mode_code : B",
"mode_code : C",
"pathname : pathstring",
"pathstring : STRING",
"octal_number : NUMBER",
"mechanism_name : STRING",
"base64data : STRING",
"prot_code : STRING",
"decimal_integer : NUMBER",
"check_login :",

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
#define YYSTACKSIZE 500
#define YYMAXDEPTH  500
#endif
#endif

#define YYINITSTACKSIZE 500

typedef struct {
    unsigned stacksize;
    short    *s_base;
    short    *s_mark;
    short    *s_last;
    YYSTYPE  *l_base;
    YYSTYPE  *l_mark;
} YYSTACKDATA;
/* variables for the parser stack */
static YYSTACKDATA yystack;
#line 1208 "ftpcmd.y"

#define	CMD	0	/* beginning of command */
#define	ARGS	1	/* expect miscellaneous arguments */
#define	STR1	2	/* expect SP followed by STRING */
#define	STR2	3	/* expect STRING */
#define	OSTR	4	/* optional SP then STRING */
#define	ZSTR1	5	/* SP then optional STRING */
#define	ZSTR2	6	/* optional STRING after SP */
#define	SITECMD	7	/* SITE command */
#define	NSTR	8	/* Number followed by a string */
#define NOARGS	9	/* No arguments allowed */
#define EOLN	10	/* End of line */

struct tab cmdtab[] = {
				/* From RFC 959, in order defined (5.3.1) */
	{ "USER", USER, STR1,	1,	"<sp> username", 0, },
	{ "PASS", PASS, ZSTR1,	1,	"<sp> password", 0, },
	{ "ACCT", ACCT, STR1,	0,	"(specify account)", 0, },
	{ "CWD",  CWD,  OSTR,	1,	"[ <sp> directory-name ]", 0, },
	{ "CDUP", CDUP, NOARGS,	1,	"(change to parent directory)", 0, },
	{ "SMNT", SMNT, ARGS,	0,	"(structure mount)", 0, },
	{ "QUIT", QUIT, NOARGS,	1,	"(terminate service)", 0, },
	{ "REIN", REIN, NOARGS,	0,	"(reinitialize server state)", 0, },
	{ "PORT", PORT, ARGS,	1,	"<sp> b0, b1, b2, b3, b4, b5", 0, },
	{ "LPRT", LPRT, ARGS,	1,	"<sp> af, hal, h1, h2, h3,..., pal, p1, p2...", 0, },
	{ "EPRT", EPRT, STR1,	1,	"<sp> |af|addr|port|", 0, },
	{ "PASV", PASV, NOARGS,	1,	"(set server in passive mode)", 0, },
	{ "LPSV", LPSV, ARGS,	1,	"(set server in passive mode)", 0, },
	{ "EPSV", EPSV, ARGS,	1,	"[<sp> af|ALL]", 0, },
	{ "TYPE", TYPE, ARGS,	1,	"<sp> [ A | E | I | L ]", 0, },
	{ "STRU", STRU, ARGS,	1,	"(specify file structure)", 0, },
	{ "MODE", MODE, ARGS,	1,	"(specify transfer mode)", 0, },
	{ "RETR", RETR, STR1,	1,	"<sp> file-name", 0, },
	{ "STOR", STOR, STR1,	1,	"<sp> file-name", 0, },
	{ "STOU", STOU, STR1,	1,	"<sp> file-name", 0, },
	{ "APPE", APPE, STR1,	1,	"<sp> file-name", 0, },
	{ "ALLO", ALLO, ARGS,	1,	"allocate storage (vacuously)", 0, },
	{ "REST", REST, ARGS,	1,	"<sp> offset (restart command)", 0, },
	{ "RNFR", RNFR, STR1,	1,	"<sp> file-name", 0, },
	{ "RNTO", RNTO, STR1,	1,	"<sp> file-name", 0, },
	{ "ABOR", ABOR, NOARGS,	4,	"(abort operation)", 0, },
	{ "DELE", DELE, STR1,	1,	"<sp> file-name", 0, },
	{ "RMD",  RMD,  STR1,	1,	"<sp> path-name", 0, },
	{ "MKD",  MKD,  STR1,	1,	"<sp> path-name", 0, },
	{ "PWD",  PWD,  NOARGS,	1,	"(return current directory)", 0, },
	{ "LIST", LIST, OSTR,	1,	"[ <sp> path-name ]", 0, },
	{ "NLST", NLST, OSTR,	1,	"[ <sp> path-name ]", 0, },
	{ "SITE", SITE, SITECMD, 1,	"site-cmd [ <sp> arguments ]", 0, },
	{ "SYST", SYST, NOARGS,	1,	"(get type of operating system)", 0, },
	{ "STAT", STAT, OSTR,	4,	"[ <sp> path-name ]", 0, },
	{ "HELP", HELP, OSTR,	1,	"[ <sp> <string> ]", 0, },
	{ "NOOP", NOOP, NOARGS,	2,	"", 0, },

				/* From RFC 2228, in order defined */
	{ "AUTH", AUTH, STR1,	1,	"<sp> mechanism-name", 0, },
	{ "ADAT", ADAT, STR1,	1,	"<sp> base-64-data", 0, },
	{ "PROT", PROT, STR1,	1,	"<sp> prot-code", 0, },
	{ "PBSZ", PBSZ, ARGS,	1,	"<sp> decimal-integer", 0, },
	{ "CCC",  CCC,  NOARGS,	1,	"(Disable data protection)", 0, },
	{ "MIC",  MIC,  STR1,	4,	"<sp> base64data", 0, },
	{ "CONF", CONF, STR1,	4,	"<sp> base64data", 0, },
	{ "ENC",  ENC,  STR1,	4,	"<sp> base64data", 0, },

				/* From RFC 2389, in order defined */
	{ "FEAT", FEAT, NOARGS,	1,	"(display extended features)", 0, },
	{ "OPTS", OPTS, STR1,	1,	"<sp> command [ <sp> options ]", 0, },

				/* From RFC 3659, in order defined */
	{ "MDTM", MDTM, OSTR,	1,	"<sp> path-name", 0, },
	{ "SIZE", SIZE, OSTR,	1,	"<sp> path-name", 0, },
	{ "MLST", MLST, OSTR,	2,	"[ <sp> path-name ]", 0, },
	{ "MLSD", MLSD, OSTR,	1,	"[ <sp> directory-name ]", 0, },

				/* obsolete commands */
	{ "MAIL", MAIL, OSTR,	0,	"(mail to user)", 0, },
	{ "MLFL", MLFL, OSTR,	0,	"(mail file)", 0, },
	{ "MRCP", MRCP, STR1,	0,	"(mail recipient)", 0, },
	{ "MRSQ", MRSQ, OSTR,	0,	"(mail recipient scheme question)", 0, },
	{ "MSAM", MSAM, OSTR,	0,	"(mail send to terminal and mailbox)", 0, },
	{ "MSND", MSND, OSTR,	0,	"(mail send to terminal)", 0, },
	{ "MSOM", MSOM, OSTR,	0,	"(mail send to terminal or mailbox)", 0, },
	{ "XCUP", CDUP, NOARGS,	1,	"(change to parent directory)", 0, },
	{ "XCWD", CWD,  OSTR,	1,	"[ <sp> directory-name ]", 0, },
	{ "XMKD", MKD,  STR1,	1,	"<sp> path-name", 0, },
	{ "XPWD", PWD,  NOARGS,	1,	"(return current directory)", 0, },
	{ "XRMD", RMD,  STR1,	1,	"<sp> path-name", 0, },

	{  NULL,  0,	0,	0,	0, 0, }
};

struct tab sitetab[] = {
	{ "CHMOD",	CHMOD,	NSTR,	1,	"<sp> mode <sp> file-name", 0, },
	{ "HELP",	HELP,	OSTR,	1,	"[ <sp> <string> ]", 0, },
	{ "IDLE",	IDLE,	ARGS,	1,	"[ <sp> maximum-idle-time ]", 0, },
	{ "RATEGET",	RATEGET,OSTR,	1,	"[ <sp> get-throttle-rate ]", 0, },
	{ "RATEPUT",	RATEPUT,OSTR,	1,	"[ <sp> put-throttle-rate ]", 0, },
	{ "UMASK",	UMASK,	ARGS,	1,	"[ <sp> umask ]", 0, },
	{ NULL,		0,	0,	0,	0, 0, }
};

/*
 * Check if a filename is allowed to be modified (isupload == 0) or
 * uploaded (isupload == 1), and if necessary, check the filename is `sane'.
 * If the filename is NULL, fail.
 * If the filename is "", don't do the sane name check.
 */
static int
check_write(const char *file, int isupload)
{
	if (file == NULL)
		return (0);
	if (! logged_in) {
		reply(530, "Please login with USER and PASS.");
		return (0);
	}
		/* checking modify */
	if (! isupload && ! CURCLASS_FLAGS_ISSET(modify)) {
		reply(502, "No permission to use this command.");
		return (0);
	}
		/* checking upload */
	if (isupload && ! CURCLASS_FLAGS_ISSET(upload)) {
		reply(502, "No permission to use this command.");
		return (0);
	}

		/* checking sanenames */
	if (file[0] != '\0' && CURCLASS_FLAGS_ISSET(sanenames)) {
		const char *p;

		if (file[0] == '.')
			goto insane_name;
		for (p = file; *p; p++) {
			if (isalnum((unsigned char)*p) || *p == '-' || *p == '+' ||
			    *p == ',' || *p == '.' || *p == '_')
				continue;
 insane_name:
			reply(553, "File name `%s' not allowed.", file);
			return (0);
		}
	}
	return (1);
}

struct tab *
lookup(struct tab *p, const char *cmd)
{

	for (; p->name != NULL; p++)
		if (strcasecmp(cmd, p->name) == 0)
			return (p);
	return (0);
}

#include <arpa/telnet.h>

/*
 * get_line - a hacked up version of fgets to ignore TELNET escape codes.
 *	`s' is the buffer to read into.
 *	`n' is the 1 less than the size of the buffer, to allow trailing NUL
 *	`iop' is the FILE to read from.
 *	Returns 0 on success, -1 on EOF, -2 if the command was too long.
 */
int
get_line(char *s, int n, FILE *iop)
{
	int c;
	char *cs;

	cs = s;
/* tmpline may contain saved command from urgent mode interruption */
	for (c = 0; tmpline[c] != '\0' && --n > 0; ++c) {
		*cs++ = tmpline[c];
		if (tmpline[c] == '\n') {
			*cs++ = '\0';
			if (ftpd_debug)
				syslog(LOG_DEBUG, "command: %s", s);
			tmpline[0] = '\0';
			return(0);
		}
		if (c == 0)
			tmpline[0] = '\0';
	}
	while ((c = getc(iop)) != EOF) {
		total_bytes++;
		total_bytes_in++;
		c &= 0377;
		if (c == IAC) {
		    if ((c = getc(iop)) != EOF) {
			total_bytes++;
			total_bytes_in++;
			c &= 0377;
			switch (c) {
			case WILL:
			case WONT:
				c = getc(iop);
				total_bytes++;
				total_bytes_in++;
				cprintf(stdout, "%c%c%c", IAC, DONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case DO:
			case DONT:
				c = getc(iop);
				total_bytes++;
				total_bytes_in++;
				cprintf(stdout, "%c%c%c", IAC, WONT, 0377&c);
				(void) fflush(stdout);
				continue;
			case IAC:
				break;
			default:
				continue;	/* ignore command */
			}
		    }
		}
		*cs++ = c;
		if (--n <= 0) {
			/*
			 * If command doesn't fit into buffer, discard the
			 * rest of the command and indicate truncation.
			 * This prevents the command to be split up into
			 * multiple commands.
			 */
			if (ftpd_debug)
				syslog(LOG_DEBUG,
				    "command too long, last char: %d", c);
			while (c != '\n' && (c = getc(iop)) != EOF)
				continue;
			return (-2);
		}
		if (c == '\n')
			break;
	}
	if (c == EOF && cs == s)
		return (-1);
	*cs++ = '\0';
	if (ftpd_debug) {
		if ((curclass.type != CLASS_GUEST &&
		    strncasecmp(s, "PASS ", 5) == 0) ||
		    strncasecmp(s, "ACCT ", 5) == 0) {
			/* Don't syslog passwords */
			syslog(LOG_DEBUG, "command: %.4s ???", s);
		} else {
			char *cp;
			int len;

			/* Don't syslog trailing CR-LF */
			len = strlen(s);
			cp = s + len - 1;
			while (cp >= s && (*cp == '\n' || *cp == '\r')) {
				--cp;
				--len;
			}
			syslog(LOG_DEBUG, "command: %.*s", len, s);
		}
	}
	return (0);
}

void
ftp_handle_line(char *cp)
{

	cmdp = cp;
	yyparse();
}

void
ftp_loop(void)
{
	int ret;

	while (1) {
		(void) alarm(curclass.timeout);
		ret = get_line(cbuf, sizeof(cbuf)-1, stdin);
		(void) alarm(0);
		if (ret == -1) {
			reply(221, "You could at least say goodbye.");
			dologout(0);
		} else if (ret == -2) {
			reply(500, "Command too long.");
		} else {
			ftp_handle_line(cbuf);
		}
	}
	/*NOTREACHED*/
}

int
yylex(void)
{
	static int cpos, state;
	char *cp, *cp2;
	struct tab *p;
	int n;
	char c;

	switch (state) {

	case CMD:
		hasyyerrored = 0;
		if ((cp = strchr(cmdp, '\r'))) {
			*cp = '\0';
#if defined(HAVE_SETPROCTITLE)
			if (strncasecmp(cmdp, "PASS", 4) != 0 &&
			    strncasecmp(cmdp, "ACCT", 4) != 0)
				setproctitle("%s: %s", proctitle, cmdp);
#endif /* defined(HAVE_SETPROCTITLE) */
			*cp++ = '\n';
			*cp = '\0';
		}
		if ((cp = strpbrk(cmdp, " \n")))
			cpos = cp - cmdp;
		if (cpos == 0)
			cpos = 4;
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		p = lookup(cmdtab, cmdp);
		cmdp[cpos] = c;
		if (p != NULL) {
			if (is_oob && ! CMD_OOB(p)) {
				/* command will be handled in-band */
				return (0);
			} else if (! CMD_IMPLEMENTED(p)) {
				reply(502, "%s command not implemented.",
				    p->name);
				hasyyerrored = 1;
				break;
			}
			state = p->state;
			yylval.cs = p->name;
			return (p->token);
		}
		break;

	case SITECMD:
		if (cmdp[cpos] == ' ') {
			cpos++;
			return (SP);
		}
		cp = &cmdp[cpos];
		if ((cp2 = strpbrk(cp, " \n")))
			cpos = cp2 - cmdp;
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		p = lookup(sitetab, cp);
		cmdp[cpos] = c;
		if (p != NULL) {
			if (!CMD_IMPLEMENTED(p)) {
				reply(502, "SITE %s command not implemented.",
				    p->name);
				hasyyerrored = 1;
				break;
			}
			state = p->state;
			yylval.cs = p->name;
			return (p->token);
		}
		break;

	case OSTR:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		/* FALLTHROUGH */

	case STR1:
	case ZSTR1:
	dostr1:
		if (cmdp[cpos] == ' ') {
			cpos++;
			state = state == OSTR ? STR2 : state+1;
			return (SP);
		}
		break;

	case ZSTR2:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		/* FALLTHROUGH */

	case STR2:
		cp = &cmdp[cpos];
		n = strlen(cp);
		cpos += n - 1;
		/*
		 * Make sure the string is nonempty and \n terminated.
		 */
		if (n > 1 && cmdp[cpos] == '\n') {
			cmdp[cpos] = '\0';
			yylval.s = ftpd_strdup(cp);
			cmdp[cpos] = '\n';
			state = ARGS;
			return (STRING);
		}
		break;

	case NSTR:
		if (cmdp[cpos] == ' ') {
			cpos++;
			return (SP);
		}
		if (isdigit((unsigned char)cmdp[cpos])) {
			cp = &cmdp[cpos];
			while (isdigit((unsigned char)cmdp[++cpos]))
				;
			c = cmdp[cpos];
			cmdp[cpos] = '\0';
			yylval.u.i = atoi(cp);
			cmdp[cpos] = c;
			state = STR1;
			return (NUMBER);
		}
		state = STR1;
		goto dostr1;

	case ARGS:
		if (isdigit((unsigned char)cmdp[cpos])) {
			cp = &cmdp[cpos];
			while (isdigit((unsigned char)cmdp[++cpos]))
				;
			c = cmdp[cpos];
			cmdp[cpos] = '\0';
			yylval.u.i = atoi(cp);
			yylval.u.ll = STRTOLL(cp, NULL, 10);
			cmdp[cpos] = c;
			return (NUMBER);
		}
		if (strncasecmp(&cmdp[cpos], "ALL", 3) == 0
		    && !isalnum((unsigned char)cmdp[cpos + 3])) {
			cpos += 3;
			return (ALL);
		}
		switch (cmdp[cpos++]) {

		case '\n':
			state = EOLN;
			return (CRLF);

		case ' ':
			return (SP);

		case ',':
			return (COMMA);

		case 'A':
		case 'a':
			return (A);

		case 'B':
		case 'b':
			return (B);

		case 'C':
		case 'c':
			return (C);

		case 'E':
		case 'e':
			return (E);

		case 'F':
		case 'f':
			return (F);

		case 'I':
		case 'i':
			return (I);

		case 'L':
		case 'l':
			return (L);

		case 'N':
		case 'n':
			return (N);

		case 'P':
		case 'p':
			return (P);

		case 'R':
		case 'r':
			return (R);

		case 'S':
		case 's':
			return (S);

		case 'T':
		case 't':
			return (T);

		}
		break;

	case NOARGS:
		if (cmdp[cpos] == '\n') {
			state = EOLN;
			return (CRLF);
		}
		c = cmdp[cpos];
		cmdp[cpos] = '\0';
		reply(501, "'%s' command does not take any arguments.", cmdp);
		hasyyerrored = 1;
		cmdp[cpos] = c;
		break;

	case EOLN:
		state = CMD;
		return (0);

	default:
		fatal("Unknown state in scanner.");
	}
	yyerror(NULL);
	state = CMD;
	return (0);
}

/* ARGSUSED */
void
yyerror(const char *s)
{
	char *cp;

	if (hasyyerrored || is_oob)
		return;
	if ((cp = strchr(cmdp,'\n')) != NULL)
		*cp = '\0';
	reply(500, "'%s': command not understood.", cmdp);
	hasyyerrored = 1;
}

static void
help(struct tab *ctab, const char *s)
{
	struct tab *c;
	int width, NCMDS;
	const char *htype;

	if (ctab == sitetab)
		htype = "SITE ";
	else
		htype = "";
	width = 0, NCMDS = 0;
	for (c = ctab; c->name != NULL; c++) {
		int len = strlen(c->name);

		if (len > width)
			width = len;
		NCMDS++;
	}
	width = (width + 8) &~ 7;
	if (s == 0) {
		int i, j, w;
		int columns, lines;

		reply(-214, "%s", "");
		reply(0, "The following %scommands are recognized.", htype);
		reply(0, "(`-' = not implemented, `+' = supports options)");
		columns = 76 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			cprintf(stdout, "    ");
			for (j = 0; j < columns; j++) {
				c = ctab + j * lines + i;
				cprintf(stdout, "%s", c->name);
				w = strlen(c->name);
				if (! CMD_IMPLEMENTED(c)) {
					CPUTC('-', stdout);
					w++;
				}
				if (CMD_HAS_OPTIONS(c)) {
					CPUTC('+', stdout);
					w++;
				}
				if (c + lines >= &ctab[NCMDS])
					break;
				while (w < width) {
					CPUTC(' ', stdout);
					w++;
				}
			}
			cprintf(stdout, "\r\n");
		}
		(void) fflush(stdout);
		reply(214, "Direct comments to ftp-bugs@%s.", hostname);
		return;
	}
	c = lookup(ctab, s);
	if (c == (struct tab *)0) {
		reply(502, "Unknown command '%s'.", s);
		return;
	}
	if (CMD_IMPLEMENTED(c))
		reply(214, "Syntax: %s%s %s", htype, c->name, c->help);
	else
		reply(504, "%s%-*s\t%s; not implemented.", htype, width,
		    c->name, c->help);
}

/*
 * Check that the structures used for a PORT, LPRT or EPRT command are
 * valid (data_dest, his_addr), and if necessary, detect ftp bounce attacks.
 * If family != -1 check that his_addr.su_family == family.
 */
static void
port_check(const char *cmd, int family)
{
	char h1[NI_MAXHOST], h2[NI_MAXHOST];
	char s1[NI_MAXHOST], s2[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV | NI_WITHSCOPEID;
#else
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
#endif

	if (epsvall) {
		reply(501, "%s disallowed after EPSV ALL", cmd);
		return;
	}

	if (family != -1 && his_addr.su_family != family) {
 port_check_fail:
		reply(500, "Illegal %s command rejected", cmd);
		return;
	}

	if (data_dest.su_family != his_addr.su_family)
		goto port_check_fail;

			/* be paranoid, if told so */
	if (CURCLASS_FLAGS_ISSET(checkportcmd)) {
#ifdef INET6
		/*
		 * be paranoid, there are getnameinfo implementation that does
		 * not present scopeid portion
		 */
		if (data_dest.su_family == AF_INET6 &&
		    data_dest.su_scope_id != his_addr.su_scope_id)
			goto port_check_fail;
#endif

		if (getnameinfo((struct sockaddr *)&data_dest, data_dest.su_len,
		    h1, sizeof(h1), s1, sizeof(s1), niflags))
			goto port_check_fail;
		if (getnameinfo((struct sockaddr *)&his_addr, his_addr.su_len,
		    h2, sizeof(h2), s2, sizeof(s2), niflags))
			goto port_check_fail;

		if (atoi(s1) < IPPORT_RESERVED || strcmp(h1, h2) != 0)
			goto port_check_fail;
	}

	usedefault = 0;
	if (pdata >= 0) {
		(void) close(pdata);
		pdata = -1;
	}
	reply(200, "%s command successful.", cmd);
}
#line 1263 "ftpcmd.c"

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
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = data->stacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;

    i = data->s_mark - data->s_base;
    newss = (short *)realloc(data->s_base, newsize * sizeof(*newss));
    if (newss == 0)
        return -1;

    data->s_base = newss;
    data->s_mark = newss + i;

    newvs = (YYSTYPE *)realloc(data->l_base, newsize * sizeof(*newvs));
    if (newvs == 0)
        return -1;

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

    if (yystack.s_base == NULL && yygrowstack(&yystack)) goto yyoverflow;
    yystack.s_mark = yystack.s_base;
    yystack.l_mark = yystack.l_base;
    yystate = 0;
    *yystack.s_mark = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = YYLEX) < 0) yychar = 0;
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
        if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
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
            if ((yyn = yysindex[*yystack.s_mark]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yystack.s_mark, yytable[yyn]);
#endif
                if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
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
case 1:
#line 176 "ftpcmd.y"
	{
			REASSIGN(fromname, NULL);
			restart_point = (off_t) 0;
		}
break;
case 3:
#line 188 "ftpcmd.y"
	{
			user(yystack.l_mark[-1].s);
			free(yystack.l_mark[-1].s);
		}
break;
case 4:
#line 194 "ftpcmd.y"
	{
			pass(yystack.l_mark[-1].s);
			memset(yystack.l_mark[-1].s, 0, strlen(yystack.l_mark[-1].s));
			free(yystack.l_mark[-1].s);
		}
break;
case 5:
#line 201 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i)
				cwd(homedir);
		}
break;
case 6:
#line 207 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				cwd(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 7:
#line 215 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i)
				cwd("..");
		}
break;
case 8:
#line 221 "ftpcmd.y"
	{
			if (logged_in) {
				reply(-221, "%s", "");
				reply(0,
 "Data traffic for this session was " LLF " byte%s in " LLF " file%s.",
				    (LLT)total_data, PLURAL(total_data),
				    (LLT)total_files, PLURAL(total_files));
				reply(0,
 "Total traffic for this session was " LLF " byte%s in " LLF " transfer%s.",
				    (LLT)total_bytes, PLURAL(total_bytes),
				    (LLT)total_xfers, PLURAL(total_xfers));
			}
			reply(221,
			    "Thank you for using the FTP service on %s.",
			    hostname);
			if (logged_in && logging) {
				syslog(LOG_INFO,
		"Data traffic: " LLF " byte%s in " LLF " file%s",
				    (LLT)total_data, PLURAL(total_data),
				    (LLT)total_files, PLURAL(total_files));
				syslog(LOG_INFO,
		"Total traffic: " LLF " byte%s in " LLF " transfer%s",
				    (LLT)total_bytes, PLURAL(total_bytes),
				    (LLT)total_xfers, PLURAL(total_xfers));
			}

			dologout(0);
		}
break;
case 9:
#line 251 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i)
				port_check("PORT", AF_INET);
		}
break;
case 10:
#line 257 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i)
				port_check("LPRT", AF_INET);
		}
break;
case 11:
#line 263 "ftpcmd.y"
	{
#ifdef INET6
			if (yystack.l_mark[-3].u.i)
				port_check("LPRT", AF_INET6);
#else
			reply(500, "IPv6 support not available.");
#endif
		}
break;
case 12:
#line 273 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				if (extended_port(yystack.l_mark[-1].s) == 0)
					port_check("EPRT", -1);
			}
			free(yystack.l_mark[-1].s);
		}
break;
case 13:
#line 282 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				if (CURCLASS_FLAGS_ISSET(passive))
					passive();
				else
					reply(500, "PASV mode not available.");
			}
		}
break;
case 14:
#line 292 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				if (CURCLASS_FLAGS_ISSET(passive)) {
					if (epsvall)
						reply(501,
						    "LPSV disallowed after EPSV ALL");
					else
						long_passive("LPSV", PF_UNSPEC);
				} else
					reply(500, "LPSV mode not available.");
			}
		}
break;
case 15:
#line 306 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				if (CURCLASS_FLAGS_ISSET(passive))
					long_passive("EPSV",
					    epsvproto2af(yystack.l_mark[-1].u.i));
				else
					reply(500, "EPSV mode not available.");
			}
		}
break;
case 16:
#line 317 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				if (CURCLASS_FLAGS_ISSET(passive)) {
					reply(200,
					    "EPSV ALL command successful.");
					epsvall++;
				} else
					reply(500, "EPSV mode not available.");
			}
		}
break;
case 17:
#line 329 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				if (CURCLASS_FLAGS_ISSET(passive))
					long_passive("EPSV", PF_UNSPEC);
				else
					reply(500, "EPSV mode not available.");
			}
		}
break;
case 18:
#line 339 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {

			switch (cmd_type) {

			case TYPE_A:
				if (cmd_form == FORM_N) {
					reply(200, "Type set to A.");
					type = cmd_type;
					form = cmd_form;
				} else
					reply(504, "Form must be N.");
				break;

			case TYPE_E:
				reply(504, "Type E not implemented.");
				break;

			case TYPE_I:
				reply(200, "Type set to I.");
				type = cmd_type;
				break;

			case TYPE_L:
#if NBBY == 8
				if (cmd_bytesz == 8) {
					reply(200,
					    "Type set to L (byte size 8).");
					type = cmd_type;
				} else
					reply(504, "Byte size must be 8.");
#else /* NBBY == 8 */
				UNIMPLEMENTED for NBBY != 8
#endif /* NBBY == 8 */
			}
			
			}
		}
break;
case 19:
#line 379 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				switch (yystack.l_mark[-1].u.i) {

				case STRU_F:
					reply(200, "STRU F ok.");
					break;

				default:
					reply(504, "Unimplemented STRU type.");
				}
			}
		}
break;
case 20:
#line 394 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				switch (yystack.l_mark[-1].u.i) {

				case MODE_S:
					reply(200, "MODE S ok.");
					break;

				default:
					reply(502, "Unimplemented MODE type.");
				}
			}
		}
break;
case 21:
#line 409 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				retrieve(NULL, yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 22:
#line 417 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 1))
				store(yystack.l_mark[-1].s, "w", 0);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 23:
#line 425 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 1))
				store(yystack.l_mark[-1].s, "w", 1);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 24:
#line 433 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 1))
				store(yystack.l_mark[-1].s, "a", 0);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 25:
#line 441 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i)
				reply(202, "ALLO command ignored.");
		}
break;
case 26:
#line 447 "ftpcmd.y"
	{
			if (yystack.l_mark[-7].u.i)
				reply(202, "ALLO command ignored.");
		}
break;
case 27:
#line 453 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 0)) {
				if (fromname) {
					renamecmd(fromname, yystack.l_mark[-1].s);
					REASSIGN(fromname, NULL);
				} else {
					reply(503, "Bad sequence of commands.");
				}
			}
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 28:
#line 467 "ftpcmd.y"
	{
			if (is_oob)
				abor();
			else if (yystack.l_mark[-1].u.i)
				reply(225, "ABOR command successful.");
		}
break;
case 29:
#line 475 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 0))
				delete(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 30:
#line 483 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 0))
				removedir(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 31:
#line 491 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 0))
				makedir(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 32:
#line 499 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i)
				pwd();
		}
break;
case 33:
#line 505 "ftpcmd.y"
	{
			const char *argv[] = { INTERNAL_LS, "-lgA", NULL };
			
			if (CURCLASS_FLAGS_ISSET(hidesymlinks))
				argv[1] = "-LlgA";
			if (yystack.l_mark[-1].u.i)
				retrieve(argv, "");
		}
break;
case 34:
#line 515 "ftpcmd.y"
	{
			const char *argv[] = { INTERNAL_LS, "-lgA", NULL, NULL };

			if (CURCLASS_FLAGS_ISSET(hidesymlinks))
				argv[1] = "-LlgA";
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL) {
				argv[2] = yystack.l_mark[-1].s;
				retrieve(argv, yystack.l_mark[-1].s);
			}
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 35:
#line 529 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i)
				send_file_list(".");
		}
break;
case 36:
#line 535 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i)
				send_file_list(yystack.l_mark[-1].s);
			free(yystack.l_mark[-1].s);
		}
break;
case 37:
#line 542 "ftpcmd.y"
	{
			help(sitetab, NULL);
		}
break;
case 38:
#line 547 "ftpcmd.y"
	{
			if (check_write(yystack.l_mark[-1].s, 0)) {
				if ((yystack.l_mark[-3].u.i == -1) || (yystack.l_mark[-3].u.i > 0777))
					reply(501,
				"CHMOD: Mode value must be between 0 and 0777");
				else if (chmod(yystack.l_mark[-1].s, yystack.l_mark[-3].u.i) < 0)
					perror_reply(550, yystack.l_mark[-1].s);
				else
					reply(200, "CHMOD command successful.");
			}
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 39:
#line 562 "ftpcmd.y"
	{
			help(sitetab, yystack.l_mark[-1].s);
			free(yystack.l_mark[-1].s);
		}
break;
case 40:
#line 568 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				reply(200,
				    "Current IDLE time limit is " LLF
				    " seconds; max " LLF,
				    (LLT)curclass.timeout,
				    (LLT)curclass.maxtimeout);
			}
		}
break;
case 41:
#line 579 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				if (yystack.l_mark[-1].u.i < 30 || yystack.l_mark[-1].u.i > curclass.maxtimeout) {
					reply(501,
				"IDLE time limit must be between 30 and "
					    LLF " seconds",
					    (LLT)curclass.maxtimeout);
				} else {
					curclass.timeout = yystack.l_mark[-1].u.i;
					(void) alarm(curclass.timeout);
					reply(200,
					    "IDLE time limit set to "
					    LLF " seconds",
					    (LLT)curclass.timeout);
				}
			}
		}
break;
case 42:
#line 598 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				reply(200,
				    "Current RATEGET is " LLF " bytes/sec",
				    (LLT)curclass.rateget);
			}
		}
break;
case 43:
#line 607 "ftpcmd.y"
	{
			char errbuf[100];
			char *p = yystack.l_mark[-1].s;
			LLT rate;

			if (yystack.l_mark[-3].u.i) {
				rate = strsuftollx("RATEGET", p, 0,
				    curclass.maxrateget
				    ? curclass.maxrateget
				    : LLTMAX, errbuf, sizeof(errbuf));
				if (errbuf[0])
					reply(501, "%s", errbuf);
				else {
					curclass.rateget = rate;
					reply(200,
					    "RATEGET set to " LLF " bytes/sec",
					    (LLT)curclass.rateget);
				}
			}
			free(yystack.l_mark[-1].s);
		}
break;
case 44:
#line 630 "ftpcmd.y"
	{
			if (yystack.l_mark[-1].u.i) {
				reply(200,
				    "Current RATEPUT is " LLF " bytes/sec",
				    (LLT)curclass.rateput);
			}
		}
break;
case 45:
#line 639 "ftpcmd.y"
	{
			char errbuf[100];
			char *p = yystack.l_mark[-1].s;
			LLT rate;

			if (yystack.l_mark[-3].u.i) {
				rate = strsuftollx("RATEPUT", p, 0,
				    curclass.maxrateput
				    ? curclass.maxrateput
				    : LLTMAX, errbuf, sizeof(errbuf));
				if (errbuf[0])
					reply(501, "%s", errbuf);
				else {
					curclass.rateput = rate;
					reply(200,
					    "RATEPUT set to " LLF " bytes/sec",
					    (LLT)curclass.rateput);
				}
			}
			free(yystack.l_mark[-1].s);
		}
break;
case 46:
#line 662 "ftpcmd.y"
	{
			int oldmask;

			if (yystack.l_mark[-1].u.i) {
				oldmask = umask(0);
				(void) umask(oldmask);
				reply(200, "Current UMASK is %03o", oldmask);
			}
		}
break;
case 47:
#line 673 "ftpcmd.y"
	{
			int oldmask;

			if (yystack.l_mark[-3].u.i && check_write("", 0)) {
				if ((yystack.l_mark[-1].u.i == -1) || (yystack.l_mark[-1].u.i > 0777)) {
					reply(501, "Bad UMASK value");
				} else {
					oldmask = umask(yystack.l_mark[-1].u.i);
					reply(200,
					    "UMASK set to %03o (was %03o)",
					    yystack.l_mark[-1].u.i, oldmask);
				}
			}
		}
break;
case 48:
#line 689 "ftpcmd.y"
	{
			if (EMPTYSTR(version))
				reply(215, "UNIX Type: L%d", NBBY);
			else
				reply(215, "UNIX Type: L%d Version: %s", NBBY,
				    version);
		}
break;
case 49:
#line 698 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				statfilecmd(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 50:
#line 706 "ftpcmd.y"
	{
			if (is_oob)
				statxfer();
			else
				statcmd();
		}
break;
case 51:
#line 714 "ftpcmd.y"
	{
			help(cmdtab, NULL);
		}
break;
case 52:
#line 719 "ftpcmd.y"
	{
			char *cp = yystack.l_mark[-1].s;

			if (strncasecmp(cp, "SITE", 4) == 0) {
				cp = yystack.l_mark[-1].s + 4;
				if (*cp == ' ')
					cp++;
				if (*cp)
					help(sitetab, cp);
				else
					help(sitetab, NULL);
			} else
				help(cmdtab, yystack.l_mark[-1].s);
			free(yystack.l_mark[-1].s);
		}
break;
case 53:
#line 736 "ftpcmd.y"
	{
			reply(200, "NOOP command successful.");
		}
break;
case 54:
#line 742 "ftpcmd.y"
	{
			reply(502, "RFC 2228 authentication not implemented.");
			free(yystack.l_mark[-1].s);
		}
break;
case 55:
#line 748 "ftpcmd.y"
	{
			reply(503,
			    "Please set authentication state with AUTH.");
			free(yystack.l_mark[-1].s);
		}
break;
case 56:
#line 755 "ftpcmd.y"
	{
			reply(503,
			    "Please set protection buffer size with PBSZ.");
			free(yystack.l_mark[-1].s);
		}
break;
case 57:
#line 762 "ftpcmd.y"
	{
			reply(503,
			    "Please set authentication state with AUTH.");
		}
break;
case 58:
#line 768 "ftpcmd.y"
	{
			reply(533, "No protection enabled.");
		}
break;
case 59:
#line 773 "ftpcmd.y"
	{
			reply(502, "RFC 2228 authentication not implemented.");
			free(yystack.l_mark[-1].s);
		}
break;
case 60:
#line 779 "ftpcmd.y"
	{
			reply(502, "RFC 2228 authentication not implemented.");
			free(yystack.l_mark[-1].s);
		}
break;
case 61:
#line 785 "ftpcmd.y"
	{
			reply(502, "RFC 2228 authentication not implemented.");
			free(yystack.l_mark[-1].s);
		}
break;
case 62:
#line 792 "ftpcmd.y"
	{

			feat();
		}
break;
case 63:
#line 798 "ftpcmd.y"
	{
			
			opts(yystack.l_mark[-1].s);
			free(yystack.l_mark[-1].s);
		}
break;
case 64:
#line 812 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				sizecmd(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 65:
#line 826 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL) {
				struct stat stbuf;
				if (stat(yystack.l_mark[-1].s, &stbuf) < 0)
					perror_reply(550, yystack.l_mark[-1].s);
				else if (!S_ISREG(stbuf.st_mode)) {
					reply(550, "%s: not a plain file.", yystack.l_mark[-1].s);
				} else {
					struct tm *t;

					t = gmtime(&stbuf.st_mtime);
					reply(213,
					    "%04d%02d%02d%02d%02d%02d",
					    TM_YEAR_BASE + t->tm_year,
					    t->tm_mon+1, t->tm_mday,
					    t->tm_hour, t->tm_min, t->tm_sec);
				}
			}
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 66:
#line 849 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				mlst(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 67:
#line 857 "ftpcmd.y"
	{
			mlst(NULL);
		}
break;
case 68:
#line 862 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i && yystack.l_mark[-1].s != NULL)
				mlsd(yystack.l_mark[-1].s);
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 69:
#line 870 "ftpcmd.y"
	{
			mlsd(NULL);
		}
break;
case 70:
#line 875 "ftpcmd.y"
	{
			yyerrok;
		}
break;
case 71:
#line 882 "ftpcmd.y"
	{
			if (yystack.l_mark[-3].u.i) {
				REASSIGN(fromname, NULL);
				restart_point = (off_t)yystack.l_mark[-1].u.ll;
				reply(350,
    "Restarting at " LLF ". Send STORE or RETRIEVE to initiate transfer.",
				    (LLT)restart_point);
			}
		}
break;
case 72:
#line 893 "ftpcmd.y"
	{
			restart_point = (off_t) 0;
			if (check_write(yystack.l_mark[-1].s, 0)) {
				REASSIGN(fromname, NULL);
				fromname = renamefrom(yystack.l_mark[-1].s);
			}
			if (yystack.l_mark[-1].s != NULL)
				free(yystack.l_mark[-1].s);
		}
break;
case 74:
#line 910 "ftpcmd.y"
	{
			yyval.s = (char *)calloc(1, sizeof(char));
		}
break;
case 76:
#line 919 "ftpcmd.y"
	{
			yyval.u.i = yystack.l_mark[0].u.i;
		}
break;
case 77:
#line 927 "ftpcmd.y"
	{
			char *a, *p;

			memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in);
			data_dest.su_family = AF_INET;
			p = (char *)&data_dest.su_port;
			p[0] = yystack.l_mark[-2].u.i; p[1] = yystack.l_mark[0].u.i;
			a = (char *)&data_dest.su_addr;
			a[0] = yystack.l_mark[-10].u.i; a[1] = yystack.l_mark[-8].u.i; a[2] = yystack.l_mark[-6].u.i; a[3] = yystack.l_mark[-4].u.i;
		}
break;
case 78:
#line 944 "ftpcmd.y"
	{
			char *a, *p;

			memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in);
			data_dest.su_family = AF_INET;
			p = (char *)&data_dest.su_port;
			p[0] = yystack.l_mark[-2].u.i; p[1] = yystack.l_mark[0].u.i;
			a = (char *)&data_dest.su_addr;
			a[0] = yystack.l_mark[-12].u.i; a[1] = yystack.l_mark[-10].u.i; a[2] = yystack.l_mark[-8].u.i; a[3] = yystack.l_mark[-6].u.i;

			/* reject invalid LPRT command */
			if (yystack.l_mark[-16].u.i != 4 || yystack.l_mark[-14].u.i != 4 || yystack.l_mark[-4].u.i != 2)
				memset(&data_dest, 0, sizeof(data_dest));
		}
break;
case 79:
#line 968 "ftpcmd.y"
	{
#ifdef INET6
			unsigned char buf[16];

			(void)memset(&data_dest, 0, sizeof(data_dest));
			data_dest.su_len = sizeof(struct sockaddr_in6);
			data_dest.su_family = AF_INET6;
			buf[0] = yystack.l_mark[-2].u.i; buf[1] = yystack.l_mark[0].u.i;
			(void)memcpy(&data_dest.su_port, buf,
			    sizeof(data_dest.su_port));
			buf[0] = yystack.l_mark[-36].u.i; buf[1] = yystack.l_mark[-34].u.i;
			buf[2] = yystack.l_mark[-32].u.i; buf[3] = yystack.l_mark[-30].u.i;
			buf[4] = yystack.l_mark[-28].u.i; buf[5] = yystack.l_mark[-26].u.i;
			buf[6] = yystack.l_mark[-24].u.i; buf[7] = yystack.l_mark[-22].u.i;
			buf[8] = yystack.l_mark[-20].u.i; buf[9] = yystack.l_mark[-18].u.i;
			buf[10] = yystack.l_mark[-16].u.i; buf[11] = yystack.l_mark[-14].u.i;
			buf[12] = yystack.l_mark[-12].u.i; buf[13] = yystack.l_mark[-10].u.i;
			buf[14] = yystack.l_mark[-8].u.i; buf[15] = yystack.l_mark[-6].u.i;
			(void)memcpy(&data_dest.si_su.su_sin6.sin6_addr,
			    buf, sizeof(data_dest.si_su.su_sin6.sin6_addr));
			if (his_addr.su_family == AF_INET6) {
				/* XXX: more sanity checks! */
				data_dest.su_scope_id = his_addr.su_scope_id;
			}
#else
			memset(&data_dest, 0, sizeof(data_dest));
#endif /* INET6 */
			/* reject invalid LPRT command */
			if (yystack.l_mark[-40].u.i != 6 || yystack.l_mark[-38].u.i != 16 || yystack.l_mark[-4].u.i != 2)
				memset(&data_dest, 0, sizeof(data_dest));
		}
break;
case 80:
#line 1003 "ftpcmd.y"
	{
			yyval.u.i = FORM_N;
		}
break;
case 81:
#line 1008 "ftpcmd.y"
	{
			yyval.u.i = FORM_T;
		}
break;
case 82:
#line 1013 "ftpcmd.y"
	{
			yyval.u.i = FORM_C;
		}
break;
case 83:
#line 1020 "ftpcmd.y"
	{
			cmd_type = TYPE_A;
			cmd_form = FORM_N;
		}
break;
case 84:
#line 1026 "ftpcmd.y"
	{
			cmd_type = TYPE_A;
			cmd_form = yystack.l_mark[0].u.i;
		}
break;
case 85:
#line 1032 "ftpcmd.y"
	{
			cmd_type = TYPE_E;
			cmd_form = FORM_N;
		}
break;
case 86:
#line 1038 "ftpcmd.y"
	{
			cmd_type = TYPE_E;
			cmd_form = yystack.l_mark[0].u.i;
		}
break;
case 87:
#line 1044 "ftpcmd.y"
	{
			cmd_type = TYPE_I;
		}
break;
case 88:
#line 1049 "ftpcmd.y"
	{
			cmd_type = TYPE_L;
			cmd_bytesz = NBBY;
		}
break;
case 89:
#line 1055 "ftpcmd.y"
	{
			cmd_type = TYPE_L;
			cmd_bytesz = yystack.l_mark[0].u.i;
		}
break;
case 90:
#line 1062 "ftpcmd.y"
	{
			cmd_type = TYPE_L;
			cmd_bytesz = yystack.l_mark[0].u.i;
		}
break;
case 91:
#line 1070 "ftpcmd.y"
	{
			yyval.u.i = STRU_F;
		}
break;
case 92:
#line 1075 "ftpcmd.y"
	{
			yyval.u.i = STRU_R;
		}
break;
case 93:
#line 1080 "ftpcmd.y"
	{
			yyval.u.i = STRU_P;
		}
break;
case 94:
#line 1087 "ftpcmd.y"
	{
			yyval.u.i = MODE_S;
		}
break;
case 95:
#line 1092 "ftpcmd.y"
	{
			yyval.u.i = MODE_B;
		}
break;
case 96:
#line 1097 "ftpcmd.y"
	{
			yyval.u.i = MODE_C;
		}
break;
case 97:
#line 1104 "ftpcmd.y"
	{
			/*
			 * Problem: this production is used for all pathname
			 * processing, but only gives a 550 error reply.
			 * This is a valid reply in some cases but not in
			 * others.
			 */
			if (logged_in && yystack.l_mark[0].s && *yystack.l_mark[0].s == '~') {
				char	*path, *home, *result;
				size_t	len;

				path = strchr(yystack.l_mark[0].s + 1, '/');
				if (path != NULL)
					*path++ = '\0';
				if (yystack.l_mark[0].s[1] == '\0')
					home = homedir;
				else {
					struct passwd	*hpw;

					if ((hpw = getpwnam(yystack.l_mark[0].s + 1)) != NULL)
						home = hpw->pw_dir;
					else
						home = yystack.l_mark[0].s;
				}
				len = strlen(home) + 1;
				if (path != NULL)
					len += strlen(path) + 1;
				if ((result = malloc(len)) == NULL)
					fatal("Local resource failure: malloc");
				strlcpy(result, home, len);
				if (path != NULL) {
					strlcat(result, "/", len);
					strlcat(result, path, len);
				}
				yyval.s = result;
				free(yystack.l_mark[0].s);
			} else
				yyval.s = yystack.l_mark[0].s;
		}
break;
case 99:
#line 1151 "ftpcmd.y"
	{
			int ret, dec, multby, digit;

			/*
			 * Convert a number that was read as decimal number
			 * to what it would be if it had been read as octal.
			 */
			dec = yystack.l_mark[0].u.i;
			multby = 1;
			ret = 0;
			while (dec) {
				digit = dec%10;
				if (digit > 7) {
					ret = -1;
					break;
				}
				ret += digit * multby;
				multby *= 8;
				dec /= 10;
			}
			yyval.u.i = ret;
		}
break;
case 103:
#line 1189 "ftpcmd.y"
	{
			yyval.u.i = yystack.l_mark[0].u.i;
		}
break;
case 104:
#line 1196 "ftpcmd.y"
	{
			if (logged_in)
				yyval.u.i = 1;
			else {
				reply(530, "Please login with USER and PASS.");
				yyval.u.i = 0;
				hasyyerrored = 1;
			}
		}
break;
#line 2517 "ftpcmd.c"
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
            if ((yychar = YYLEX) < 0) yychar = 0;
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
to state %d\n", YYPREFIX, *yystack.s_mark, yystate);
#endif
    if (yystack.s_mark >= yystack.s_last && yygrowstack(&yystack))
    {
        goto yyoverflow;
    }
    *++yystack.s_mark = (short) yystate;
    *++yystack.l_mark = yyval;
    goto yyloop;

yyoverflow:
    yyerror("yacc stack overflow");

yyabort:
    yyfreestack(&yystack);
    return (1);

yyaccept:
    yyfreestack(&yystack);
    return (0);
}

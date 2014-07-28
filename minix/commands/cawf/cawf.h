/*
 *	cawf.h - definitions for cawf(1)
 */

/*
 *	Copyright (c) 1991 Purdue University Research Foundation,
 *	West Lafayette, Indiana 47907.  All rights reserved.
 *
 *	Written by Victor A. Abell <abe@mace.cc.purdue.edu>,  Purdue
 *	University Computing Center.  Not derived from licensed software;
 *	derived from awf(1) by Henry Spencer of the University of Toronto.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to alter it and redistribute
 *	it freely, subject to the following restrictions:
 *
 *	1. The author is not responsible for any consequences of use of
 *	   this software, even if they arise from flaws in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *	   by explicit claim or by omission.  Credits must appear in the
 *	   documentation.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.  Credits must
 *	   appear in the documentation.
 *
 *	4. This notice may not be removed or altered.
 */

#include <stdio.h>
#ifdef	UNIX
#include <sys/types.h>
#else
#include <sys\types.h>
#endif
#include "regexp.h"
#include "cawflib.h"
#include "proto.h"

#define	DEVCONFIG	"device.cf"		/* device configuration file */
#define ESC		'\033'			/* ESCape character */
#define MAXEXP          30                      /* maximum expressions
						 * (and TABs) */
#define MAXFSTK		5			/* maximum file stack
						 * (for .so) */
#define MAXHYCH		10			/* maximum hyphen characters */
#define MAXLINE         512			/* maximum line length */
#define MAXMACRO        100			/* maximum number of macros */
#define MAXMTXT         1024			/* maximum macro text lines */
#define MAXNHNR		10			/* maximum ".NH" numbers
						 * (but 0 not used) */
#define MAXNR		50			/* maximum number reg */
#define MAXOLL		512			/* maximum output line length */
#define	MAXSCH		256			/* maximum special characters */
#define MAXSP		25			/* maximum stack pointer (for
						 * nesting of macro calls) */
#define MAXSTR		100			/* maximum ".ds" strings */

/*
 * Output line adjustment modes
 */

#define LEFTADJ		0
#define RIGHTADJ	1
#define BOTHADJ		2

/*
 * Error handling codes
 */

#define	FATAL		0			/* fatal error */
#define	LINE		0			/* display line */
#define	NOLINE		1			/* don't display line */
#define WARN		1			/* warning error */

/*
 * Padding directions
 */

#define	PADLEFT		0			/* pad from left */
#define PADRIGHT	1			/* pad from right */

/*
 * Pass 3 signal codes
 */

#define NOBREAK		-1
#define DOBREAK		-2
#define MESSAGE		-3

/*
 * Macro argument types
 */

#define	MANMACROS	1			/* -man */
#define MSMACROS	2			/* -ms */


struct fcode {
	unsigned char nm;		/* font name character */
	unsigned char status;		/* status */
};

struct fontstr {			/* font control strings */

	unsigned char *i;		/* font initialization string */
	int il;				/* length of *i */ 
	unsigned char *b;		/* bold */
	int bl;				/* length of *bb */
	unsigned char *it;		/* italic */
	int itl;			/* length of *itb */
	unsigned char *r;		/* roman string */
	int rl;				/* length of *r */
}; 

struct hytab {
	unsigned char font;		/* font name character */
	int len;			/* effective length */
	unsigned char *str;		/* value string */
};

struct macro {
        unsigned char name[2];		/* macro name */
        int bx;				/* beginning Macrotxt[] index */
	int ct;				/* index count */
};

struct nbr {
	unsigned char nm[2];		/* register name */
	int val;			/* value */
};

struct parms {
	char nm[2];			/* parameter name */
	char *cmd;			/* pass 3 command */
	int val;                        /* current value */
	int prev;                       /* previous value */
};

struct rx {
	char *re;			/* regular expression */
	struct regexp *pat;		/* compiled pattern */
};

struct scale {
	unsigned char nm;		/* scale factor name */
	double val;			/* value */
};

struct schtab {
	unsigned char nm[2];		/* character name */
	int len;			/* effective length */
	unsigned char *str;		/* value string */
};

struct str {
	unsigned char nm[2];		/* string name */
	unsigned char *str;		/* string value */
};

extern int Adj;				/* output line adjustment mode */
extern unsigned char *Aftnxt;		/* action after next line */
extern unsigned char *Args[];		/* macro arguments */
extern unsigned char *Argstack[];	/* stack for Expand()'s "args[]" */
extern int Backc;                       /* last word ended with '\\c' */
extern int Botmarg;			/* bottom margin */
extern int Centering;                   /* centering count */
extern int Condstack[];                 /* stack for Expand()'s "cond" */
extern unsigned char *Cont;		/* continue line append */
extern int Contlen;			/* continue line append length */
extern int Curmx;                 	/* current macro name */
extern char *Device;			/* output device name */
extern char *Devconf;			/* device configuration file path */
extern char *Devfont;			/* output device font */
extern int Divert;			/* diversion status */
extern FILE *Efs;			/* error file stream pointer */
extern unsigned char *Eol;		/* end of line information */
extern int Eollen;			/* end of line length */
extern int Err;                         /* error flag */
extern unsigned char *F;		/* field value */
extern struct fcode Fcode[];		/* font codes */
extern int Fill;			/* fill status */
extern unsigned char Font[];		/* current font */
extern int Fontctl;			/* output font control */
extern char Fontstat;			/* output font status */
extern int Fph;				/* first page header status */
extern int Fsp;                         /* files stack pointer (for .so) */
extern struct fontstr Fstr;		/* font control strings */
extern unsigned char *Ftc;		/* center footer */
extern unsigned char *Ftl;		/* left footer */
extern unsigned char *Ftr;		/* right footer */
extern unsigned char *Hdc;		/* center header */
extern int Hdft;			/* header/footer status */
extern unsigned char *Hdl;		/* left header */
extern unsigned char *Hdr;		/* right header */
extern FILE *Ifs;			/* input file stream */
extern FILE *Ifs_stk[];			/* Ifs stack */
extern int Ind;                         /* indentation amount */
extern unsigned char *Inname;		/* input file name */
extern unsigned char *Inn_stk[];	/* Inname stack */
extern struct hytab Hychar[];           /* hyphen characters */
extern int LL;				/* line length */
extern unsigned char Line[];		/* input line */
extern int Lockil;			/* pass 2 line number is locked
					 * (processing is inside macro) */
extern int Marg;                        /* macro argument - man, ms, etc. */
extern struct macro Macrotab[];         /* macro table */
extern int Macrostack[];                /* stack for Expand()'s "macro" */
extern unsigned char *Macrotxt[];	/* macro text */
extern int Mtx;                         /* macro text index */
extern int Mxstack[];                   /* stack for Expand()'s "mx" */
extern int Nhnr[];			/* ".NH" numbers */
extern int Nhy;                         /* number of Hychar[] entries */
extern int Nleftstack[];                /* stack for Expand()'s "nleft" */
extern int Nmac;                        /* number of macros */
extern int Nnr;                         /* number of Numb[] entries */
extern int Nospmode;			/* no space mode */
extern int Nparms;                      /* number of Parms[] entries */
extern int NR;                          /* number of record, ala awk */
extern int NR_stk[];			/* NR stack */
extern int Nsch;                        /* number of Schar[] entries */
extern int Nstr;                        /* number of entries in Str[] */
extern int Ntabs;			/* number of TAB positions */
extern struct nbr Numb[];		/* number registers */
extern int Nxtln;			/* next line number */
extern char *optarg;			/* getopt(3) argument pointer */
extern int optind;			/* getopt(3) index */
extern int Outll;			/* output line length */
extern unsigned char Outln[];		/* output line */
extern int Outlx;			/* output line index */
extern int P2il;                        /* pass 2 input line number */
extern unsigned char *P2name;		/* pass 2 input file name */
extern int P3fill;			/* pass 3 fill status */
extern int Padchar[];			/* padding character locations */
extern int Padfrom;			/* which end to pad from */
extern int Padx;			/* Padchar[] index */
extern struct parms Parms[];            /* parameter registers */
extern unsigned char Pass1ln[];		/* pass 1 output line */
extern unsigned char Pass2ln[];		/* pass 2 output line */
extern struct rx Pat[];			/* compiled regexp patterns */
extern int Pglen;			/* page length */
extern int Pgoff;			/* page offset */
extern char *Pname;			/* program name */
extern unsigned char Prevfont;		/* previous font */
extern int Ptrstack[];                  /* stack for Expand()'s "ptr" */
extern struct scale Scale[];		/* scaling factors */
extern double Scalen;                   /* 'n' scaling factor */
extern double Scaleu;                   /* 'u' scaling factor */
extern double Scalev;                   /* 'v' scaling factor */
extern struct schtab Schar[];           /* special characters */
extern int Sp;				/* stack pointer */
extern struct str Str[];		/* ".ds" strings */
extern int Sx;				/* string index */
extern int Tabs[];			/* TAB positions */
extern int Thispg;			/* this page number */
extern int Tind;			/* temporary indentation amount */
extern int Topmarg;			/* top margin */
extern unsigned char *Trtbl;		/* .tr table */
extern int Uhyph;			/* hyphen usage state */
extern int Vspace;                      /* vertical (inter-text-line) spacing */
extern unsigned char Word[];		/* pass 2 word buffer */
extern int Wordl;                       /* effective length of Word[] */
extern int Wordx;                       /* Word[] index */
extern int Dowarn;			/* Enables warnings when true */

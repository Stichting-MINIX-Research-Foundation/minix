/*
 *	store.c - cawf(1) storage areas
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

#include "cawf.h"

struct rx Pat[] = {
	{ "^[.'](i[ef]|el)",			 	NULL},	/* 0 */
	{ "^[.']i[ef] !",				NULL},  /* 1 */
	{ "^[.']i[ef] !?\\\\n\\(\\.\\$(>|>=|=|<|<=)[0-9] ",
							NULL},	/* 2 */
	{ "^[.']i[ef] !?'\\\\\\$[0-9]'[^']*' ",		NULL},	/* 3 */
	{ "^[.']i[ef] !?[nt] ",				NULL},  /* 4 */
	{ "\\\\\\$[0-9]",                               NULL},  /* 5 */
	{ "^[ \t]*$",					NULL},  /* 6 */
	{ "\\\\|\t|-|  ",				NULL},	/* 7 */
	{ "[.!?:][]\\)'\\\"\\*]*$",                     NULL},  /* 8 */
	{ ",fP",					NULL},	/* 9 */
	{ ",tP",					NULL},	/* 10 */
	{ "^(ta|ll|ls|in|ti|po|ne|sp|pl|nr)",           NULL},  /* 11 */
	{ "^(ll|ls|in|ti|po|pl)",                       NULL},  /* 12 */
	{ "[]\\)'\\\"\\*]",                             NULL},  /* 13 */
	{ "^(LH|CH|RH|LF|CF|RF)",			NULL},	/* 14 */
	{ "^[.']i[ef]",			 		NULL},	/* 15 */
	{ ",fR",					NULL},	/* 16 */
	{ NULL,                                         NULL}   /* END */
};

int Adj = BOTHADJ;			/* output line adjustment mode */
unsigned char *Aftnxt = NULL;		/* action after next line */
unsigned char *Args[] = { NULL, NULL,	/* 10 macro arguments */
			  NULL, NULL,
			  NULL, NULL,
			  NULL, NULL,
			  NULL, NULL
};
unsigned char *Argstack[10*MAXSP];	/* stack for Expand()'s "args[]" */
int Backc = 0;				/* last word ended with '\\c' */
int Botmarg = 5;			/* bottom margin */
int Centering = 0;			/* centering count */
int Condstack[MAXSP];                   /* stack for Expand()'s "cond" */
unsigned char *Cont = NULL;		/* continue line append */
int Contlen = 0;			/* continue line append length */
int Curmx = -1;				/* current macro index */
char *Device = NULL;			/* output device name */
char *Devconf = NULL;			/* device configuration file path */
char *Devfont = NULL;			/* output device font */
int Divert = 0;                         /* diversion status */
FILE *Efs = NULL;			/* error file stream */
unsigned char *Eol = NULL;		/* end of line information */
int Eollen = 0;				/* end of line length */
int Err = 0;                            /* error flag */
unsigned char *F = NULL;		/* field value */
struct fcode Fcode[] = {		/* font codes */
	{ 'B',  '\0'},			/* Bold */
	{ 'I',  '\0'},			/* Italic */
	{ 'R',  '\0'},			/* Roman */
	{ 'C',  '\0'},			/* Courier */
	{ '\0', '\0'}
};
int Fill = 0;				/* fill status */
unsigned char Font[] = { '\0', '\0' };	/* current font */
int Fontctl;				/* output font control */
char Fontstat = 'R';			/* output font status */
int Fph = 0;				/* first page header status */
int Fsp = 0;				/* files stack pointer (for .so) */
struct fontstr Fstr;			/* font control strings */
unsigned char *Ftc = NULL;		/* center footer */
unsigned char *Ftl = NULL;		/* left footer */
unsigned char *Ftr = NULL;		/* right footer */
unsigned char *Hdc = NULL;		/* center header */
int Hdft = 0;				/* header/footer status */
unsigned char *Hdl = NULL;		/* left header */
unsigned char *Hdr = NULL;		/* right header */
struct hytab Hychar[MAXHYCH];		/* hyphen characters */
FILE *Ifs = NULL;			/* input file stream */
FILE *Ifs_stk[MAXFSTK];                 /* Ifs stack */
int Ind = 0;				/* indentation amount */
unsigned char *Inname = NULL;		/* input file name */
unsigned char *Inn_stk[MAXFSTK];	/* Inname stack */
int LL = 78;				/* line length (default) */
unsigned char Line[MAXLINE];		/* input line */
int Lockil = 0;                      	/* pass 2 line number is locked
                                         * (processing is inside macro) */
int Marg = 0;				/* macro argument - man, ms, etc. */
struct macro Macrotab[MAXMACRO];        /* macro table */
unsigned char *Macrotxt[MAXMTXT];	/* macro text */
int Mtx = 0;                            /* macro text index */
int Mxstack[MAXSP];                     /* stack for Expand()'s "mx" */
int Nfc;				/* number of font codes */
int Nhnr[MAXNHNR];                      /* ".NH" numbers */
int Nhy = 0;				/* number of Hychar[] entries */
int Nleftstack[MAXSP];                  /* stack for Expand()'s "nleft" */
int Nmac = 0;                           /* number of macros */
int Nnr = 0;				/* number of Numb[] entries */
int Nospmode = 1;                    	/* no space mode */
int Nparms = 0;				/* number of Parms[] entries */
int NR = 0;                             /* number of record ala awk */
int NR_stk[MAXFSTK];                   	/* NR stack */
int Nsch = 0;				/* number of Schar[] entries */
int Nstr = 0;				/* number of entries in Str[] */
int Ntabs = 0;				/* number of TAB positions */
struct nbr Numb[MAXNR];			/* number registers */
int Nxtln = 1;				/* next line number */
int Outll = -1;				/* output line length */
unsigned char Outln[MAXOLL*2];		/* output line */
int Outlx = 0;				/* output line index */
int P2il = 0; 	                        /* pass 2 input line number */
unsigned char *P2name = NULL;		/* pass 2 input file name */
int P3fill = 1;				/* pass 3 fill status */
int Padchar[MAXOLL];			/* padding character locations */
int Padfrom = PADLEFT;			/* which end to pad from */
int Padx = 0;				/* Padchar[] index */
struct parms Parms[] = {                /* parameter registers */
	{ {'i', 'n'}, "indent", 0, 0      },
	{ {'l', 'l'}, "linelen", 0, 0     },
	{ {'l', 's'}, "vspace", 0, 0	  },
	{ {'t', 'i'}, "tempindent", 0, 0  },
	{ {'p', 'o'}, "pageoffset", 0, 0  },
	{ {'p', 'l'}, "pagelen", 0, 0     },
	{ {'\0', '\0'}, NULL, 0, 0        }
};
unsigned char Pass1ln[MAXLINE];		/* pass 1 output line */
unsigned char Pass2ln[MAXLINE];		/* pass 2 output line */
int Pglen = 66;				/* page length */
int Pgoff = 0;				/* page offset */
char *Pname = NULL;			/* program name */
unsigned char Prevfont = '\0';		/* previous font */
int Ptrstack[MAXSP];                    /* stack for Expand()'s "ptr" */
struct scale Scale[] = {		/* scaling factors */
	{ 'i',	(240.0)		 	},
	{ 'c',	((240.0 * 50.0)/127.0)	},
	{ 'P',	(240.0/6.0)		},
	{ 'p',	(240.0/72.0)		},
	{ 'u',  (1.0)                   },
	{ 'm',  (1.0)                   },
	{ 'n',  (1.0)                   },
	{ 'v',  (1.0)                   },
	{ '\0',	(0.0)			}
};
double Scalen = 0.0;			/* 'n' scaling factor */
double Scaleu = 0.0;			/* 'u' scaling factor */
double Scalev = 0.0;			/* 'v' scaling factor */
struct schtab Schar[MAXSCH];		/* special characters */
int Sp = -1;				/* stack pointer */
struct str Str[MAXSTR];                 /* ".ds" strings */
int Sx = -1;				/* string index */
int Tabs[MAXEXP+1];			/* TAB positions */
int Thispg = 1;				/* this page number */
int Tind = 0; 				/* temporary indentation amount */
int Topmarg = 5;			/* top margin */
unsigned char *Trtbl = NULL;		/* .tr table */
int Uhyph = 0;				/* hyphen usage state */
int Vspace = 1;				/* vertical (inter-text-line) spacing */
unsigned char Word[MAXLINE];		/* pass 2 word buffer */
int Wordl = 0;                          /* effective length of Word[] */
int Wordx = 0;                          /* Word[] index */
int Dowarn = 1;				/* Enable warnings if true */

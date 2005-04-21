/*
 * mdef.h
 * Facility: m4 macro processor
 * by: oz
 */


#define unix	1	/* (kjb) */

#ifndef unix
#define unix 0
#endif 

#ifndef vms
#define vms 0
#endif

#if vms

#include stdio
#include ctype
#include signal

#else 

#include <sys/types.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#endif

/*
 *
 * m4 constants..
 *
 */
 
#define MACRTYPE        1
#define DEFITYPE        2
#define EXPRTYPE        3
#define SUBSTYPE        4
#define IFELTYPE        5
#define LENGTYPE        6
#define CHNQTYPE        7
#define SYSCTYPE        8
#define UNDFTYPE        9
#define INCLTYPE        10
#define SINCTYPE        11
#define PASTTYPE        12
#define SPASTYPE        13
#define INCRTYPE        14
#define IFDFTYPE        15
#define PUSDTYPE        16
#define POPDTYPE        17
#define SHIFTYPE        18
#define DECRTYPE        19
#define DIVRTYPE        20
#define UNDVTYPE        21
#define DIVNTYPE        22
#define MKTMTYPE        23
#define ERRPTYPE        24
#define M4WRTYPE        25
#define TRNLTYPE        26
#define DNLNTYPE        27
#define DUMPTYPE        28
#define CHNCTYPE        29
#define INDXTYPE        30
#define SYSVTYPE        31
#define EXITTYPE        32
#define DEFNTYPE        33
 
#define STATIC          128

/*
 * m4 special characters
 */
 
#define ARGFLAG         '$'
#define LPAREN          '('
#define RPAREN          ')'
#define LQUOTE          '`'
#define RQUOTE          '\''
#define COMMA           ','
#define SCOMMT          '#'
#define ECOMMT          '\n'

/*
 * definitions of diversion files. If the name of
 * the file is changed, adjust UNIQUE to point to the
 * wildcard (*) character in the filename.
 */

#if unix
#define DIVNAM  "/tmp/m4*XXXXXX"        /* unix diversion files    */
#define UNIQUE          7               /* unique char location    */
#else
#if vms
#define DIVNAM  "sys$login:m4*XXXXXX"   /* vms diversion files     */
#define UNIQUE          12              /* unique char location    */
#else
#define DIVNAM	"\M4*XXXXXX"		/* msdos diversion files   */
#define	UNIQUE	    3			/* unique char location    */
#endif
#endif

/*
 * other important constants
 */

#define EOS             (char) 0
#define MAXINP          10              /* maximum include files   */
#define MAXOUT          10              /* maximum # of diversions */
#define MAXSTR          512             /* maximum size of string  */
#define BUFSIZE         4096            /* size of pushback buffer */
#define STACKMAX        1024            /* size of call stack      */
#define STRSPMAX        4096            /* size of string space    */
#define MAXTOK          MAXSTR          /* maximum chars in a tokn */
#define HASHSIZE        199             /* maximum size of hashtab */
 
#define ALL             1
#define TOP             0
 
#define TRUE            1
#define FALSE           0
#define cycle           for(;;)

#ifdef VOID
#define void            int             /* define if void is void. */
#endif

/*
 * m4 data structures
 */
 
typedef struct ndblock *ndptr;
 
struct ndblock {                /* hastable structure         */
        char    *name;          /* entry name..               */
        char    *defn;          /* definition..               */
        int     type;           /* type of the entry..        */
        ndptr   nxtptr;         /* link to next entry..       */
};
 
#define nil     ((ndptr) 0)
 
struct keyblk {
        char    *knam;          /* keyword name */
        int     ktyp;           /* keyword type */
};

typedef union {			/* stack structure */
	int	sfra;		/* frame entry  */
	char 	*sstr;		/* string entry */
} stae;

/*
 * macros for readibility and/or speed
 *
 *      gpbc()  - get a possibly pushed-back character
 *      min()   - select the minimum of two elements
 *      pushf() - push a call frame entry onto stack
 *      pushs() - push a string pointer onto stack
 */
#define gpbc() 	 (bp > buf) ? *--bp : getc(infile[ilevel])
#define min(x,y) ((x > y) ? y : x)
#define pushf(x) if (sp < STACKMAX) mstack[++sp].sfra = (x)
#define pushs(x) if (sp < STACKMAX) mstack[++sp].sstr = (x)

/*
 *	    .				   .
 *	|   .	|  <-- sp		|  .  |
 *	+-------+			+-----+
 *	| arg 3 ----------------------->| str |
 *	+-------+			|  .  |
 *	| arg 2 ---PREVEP-----+ 	   .
 *	+-------+	      |
 *	    .		      |		|     |
 *	+-------+	      | 	+-----+
 *	| plev	|  PARLEV     +-------->| str |
 *	+-------+			|  .  |
 *	| type	|  CALTYP		   .
 *	+-------+
 *	| prcf	---PREVFP--+
 *	+-------+  	   |
 *	|   .	|  PREVSP  |
 *	    .	   	   |
 *	+-------+	   |
 *	|	<----------+
 *	+-------+
 *
 */
#define PARLEV  (mstack[fp].sfra)
#define CALTYP  (mstack[fp-1].sfra)
#define PREVEP	(mstack[fp+3].sstr)
#define PREVSP	(fp-3)
#define PREVFP	(mstack[fp-2].sfra)

/* function prototypes */

/* eval.c */

_PROTOTYPE(void eval, (char *argv [], int argc, int td ));

/* expr.c */

_PROTOTYPE(int expr, (char *expbuf ));
_PROTOTYPE(int query, (void));
_PROTOTYPE(int lor, (void));
_PROTOTYPE(int land, (void));
_PROTOTYPE(int bor, (void));
_PROTOTYPE(int bxor, (void));
_PROTOTYPE(int band, (void));
_PROTOTYPE(int eql, (void));
_PROTOTYPE(int relat, (void));
_PROTOTYPE(int shift, (void));
_PROTOTYPE(int primary, (void));
_PROTOTYPE(int term, (void));
_PROTOTYPE(int unary, (void));
_PROTOTYPE(int factor, (void));
_PROTOTYPE(int constant, (void));
_PROTOTYPE(int num, (void));
_PROTOTYPE(int geteql, (void));
_PROTOTYPE(int getrel, (void));
_PROTOTYPE(int skipws, (void));
_PROTOTYPE(int experr, (char *msg ));

/* look.c */

_PROTOTYPE(int hash, (char *name ));
_PROTOTYPE(ndptr lookup, (char *name ));
_PROTOTYPE(ndptr addent, (char *name ));
_PROTOTYPE(void remhash, (char *name, int all ));
_PROTOTYPE(void freent, (ndptr p ));

/* main.c */

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void macro, (void));
_PROTOTYPE(ndptr inspect, (char *tp ));
_PROTOTYPE(void initm4, (void));
_PROTOTYPE(void initkwds, (void));

/* misc.c */

_PROTOTYPE(int indx, (char *s1, char *s2 ));
_PROTOTYPE(void putback, (int c ));
_PROTOTYPE(void pbstr, (char *s ));
_PROTOTYPE(void pbnum, (int n ));
_PROTOTYPE(void chrsave, (int c ));
_PROTOTYPE(void getdiv, (int ind ));
_PROTOTYPE(void error, (char *s ));
_PROTOTYPE(void onintr, (int s ));
_PROTOTYPE(void killdiv, (void));
_PROTOTYPE(char *strsave, (char *s ));
_PROTOTYPE(void usage, (void));

/* serv.c */

_PROTOTYPE(void expand, (char *argv [], int argc ));
_PROTOTYPE(void dodefine, (char *name, char *defn ));
_PROTOTYPE(void dodefn, (char *name ));
_PROTOTYPE(void dopushdef, (char *name, char *defn ));
_PROTOTYPE(void dodump, (char *argv [], int argc ));
_PROTOTYPE(void doifelse, (char *argv [], int argc ));
_PROTOTYPE(int doincl, (char *ifile ));
_PROTOTYPE(int dopaste, (char *pfile ));
_PROTOTYPE(void dochq, (char *argv [], int argc ));
_PROTOTYPE(void dochc, (char *argv [], int argc ));
_PROTOTYPE(void dodiv, (int n ));
_PROTOTYPE(void doundiv, (char *argv [], int argc ));
_PROTOTYPE(void dosub, (char *argv [], int argc ));
_PROTOTYPE(void map, (char *dest, char *src, char *from, char *to ));

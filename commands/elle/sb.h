/* SB - Copyright 1982 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.  In all cases
 *	the source code and any modifications thereto must remain
 *	available to any user.
 *
 *	This is part of the SB library package.
 *	Any software using the SB library must likewise be made
 *	quasi-public, with freely available sources.
 */

#ifdef COMMENT

The initials "SB" stand for "String Block" or "String Buffer".

SBBUFFER - A SB buffer containing a sbstring opened for editing.
SBFILE   - A structure holding file-specific information for all
		SDBLKs pointing to that file.
SBSTRING - A SB string; conceptually a single string, but actually
		a linked list of SDBLKs.  Unless opened by a SBBUFFER,
		only a few operations are allowed on SBSTRINGs (creating,
		copying, deleting).
SDBLK    - One of the linked nodes constituting a sbstring.  Each SDBLK
		node points to a continuous string either in memory or
		on disk, or both.
SBLK	 - Another name for SDBLK.
SMBLK	 - An allocated chunk of memory.  Also refers to the node structure
		maintained by the SBM memory management routines, which
		points to the actual chunk of memory.
SBM	 - Name of the memory management package.  SBM routines are used
		to allocate memory in general, and are not just for
		use by SB routines.

************ MACHINE DEPENDENT DEFINITIONS **********

	The following compile time definitions represent machine
dependent parameters which are intended mainly for use only by SBM and
SBSTR routines.  Other programs should use them with caution.  Note
that a great deal of code assumes that type "int" corresponds to a basic
machine word (as per C Reference Manual).

	The current definitions will only work for machines which have
1, 2, 4, or 8 "char" bytes in a machine word.  Any other size will
require some changes to the definitions and possibly to some places
using them.

WORD   - integer-type definition corresponding to machine word.
WDSIZE - # addressable char bytes in a machine word.		(1, 2, 4, 8)
WDBITS - # low order bits in an address, ie log2(WDSIZE).	(0, 1, 2, 3)
WDMASK - Mask for low order bits of address			(0, 1, 3, 7)
CHAR_MASK - If defined, machine does sign-extension on chars, and
	they must be masked with this value.

	Note that the macro for WDBITS has no mathematical significance
other than being an expression which happens to evaluate into the right
constant for the 4 allowed values of WDSIZE, and in fact it is this
crock which restricts WDSIZE!  If C had a base 2 logarithm expression
then any power of 2 could be used.

Values for machines
				WORD	WDSIZE	WDBITS	WDMASK
	PDP11, Z8000, I8086	int	2	1	01
	VAX11, M68000, PDP10	int	4	2	03

#endif /* COMMENT */

/* First try to define a few things in a semi-portable way
*/
#include "eesite.h"
#ifdef __STDC__		/* Implementation supports ANSI stuff? */
#include <limits.h>		/* Get sizes for char stuff */
#define _SBMUCHAR 1		/* Can use "unsigned char" */
#define _SBMCHARSIGN (CHAR_MIN < 0)	/* True if "char" is sign-extended */
#define CHAR_MASK (UCHAR_MAX)

#else	/* not ANSI */
#ifndef _SBMUCHAR		/* Default assumes no "unsigned char" */
#define _SBMUCHAR 0
#endif
#ifndef _SBMCHARSIGN		/* Default assumes "char" is sign-extended */
#define _SBMCHARSIGN 1
#endif
#ifndef CHAR_MASK		/* Default assumes "char" is 8 bits */
#define CHAR_MASK 0377
#endif
#endif	/* not ANSI */

/* Define "sb_uchartoint" as a macro which ensures that an unsigned
** character value is converted properly to an int value.
*/
#if (_SBMUCHAR || (_SBMCHARSIGN==0))
#define sb_uchartoint(a) (a)		/* No fear of sign extension */
#else
#define sb_uchartoint(a) ((a)&CHAR_MASK)	/* Bah, sign extension */
#endif


/* Defs for machines with a base-2 WDSIZE.  Yes, the (int) is indeed necessary
 * (to allow implicit conversion to long where needed - the PDP11 compiler
 * is known to lose without it, because sizeof is cast as "unsigned int"
 * which loses big in long masks!)
 */
#define WORD int
#define WDSIZE ((int)(sizeof(WORD)))
#define WDMASK (WDSIZE-1)
#define WDBITS ((WDSIZE>>2)+(1&WDMASK))

#define rnddiv(a) ((a)>>WDBITS)		/* # words, rounded down */
#define rndrem(a) ((a)&WDMASK)		/* # bytes remaining past wd bndary */
#define rnddwn(a) ((a)&~WDMASK)		/* Round down to word boundary */
#define rndup(a)  rnddwn((a)+WDSIZE-1)	/* Round up to word boundary */

#ifdef COMMENT	/* The following are for machines without a base-2 WDSIZE */
#define rnddiv(a) ((a)/WDSIZE)
#define rndrem(a) ((a)%WDSIZE)
#define rnddwn(a) ((a)-rndrem(a))
#define rndup(a)  rnddwn((a)+WDSIZE-1)
#undef WDMASK			/* These become meaningless and anything */
#undef WDBITS			/* which uses them should be changed! */
#endif /* COMMENT */

/* The following 3 definitions are somewhat machine-dependent,
 * but are specifically intended for general use and work for all
 * currently known C implementations.
 *	SBMO must be an integer-type object large enough to hold
 *	the largest difference in SBMA pointers, and must not be
 *	used in signed comparisons.
 */

typedef long chroff;		/* CHROFF - Char offset in disk/sbstr */
typedef unsigned int SBMO;	/* SBMO - Char offset in memory */
typedef
#if _SBMUCHAR
	unsigned
#endif
		char *SBMA;	/* SBMA - Pointer to char loc in memory */



/* The following definitions tend to be system-dependent.  Only the
 * SBM and SBSTR routines use them.
 */
#define SB_NFILES 32		/* # of open files we can hack.  Actually
				 * this is max FD value plus 1. */
#define SB_BUFSIZ 512		/* Optimal buffer size (system block size) */
#define SB_SLOP (16*WDSIZE)	/* # slop chars to tolerate for allocations */

#define SMNODES (20)		/* # SM or SD nodes to create when needed */
#define SMCHUNKSIZ (16*512)	/* # bytes of mem to create (via sbrk) " " */
#define MAXSBMO ((SBMO)-1)	/* Used in SBM only */
		/* MAXSBMO should be the largest possible SBMO value. */

#define EOF (-1)
#define SBFILE struct sbfile
#define SBBUF struct sbbuffer
#define SBSTR struct sdblk	/* Start of a sbstring */

struct sbfile {
	int sfflags;		/* Various flags */
	int sffd;		/* FD for file (-1 if none) */
	struct sdblk *sfptr1;	/* Ptr to 1st node in phys list */
	chroff sflen;		/* Original length of file FD is for */
};

	/* Definition of SBBUF string/buffer */
struct sbbuffer {
	SBMA sbiop;		/* I/O pointer into in-core text */
	int sbrleft;		/* # chars left for reading */
	int sbwleft;		/* # chars left for writing */
	int sbflags;		/* Various flags */
	chroff sbdot;		/* Logical pos for start of current sdblk */
	chroff sboff;		/* Offset into current sdblk (if no smblk)*/
	struct sdblk *sbcur;	/* Pointer to current SD block of string */
};
	/* Flags for "sbflags" */
#define SB_OVW	01	/* Over-write mode */
#define SB_WRIT 02	/* Written; smuse needs to be updated from sbiop */

	/* NOTE: An unused sbbuf structure should be completely zeroed.
	 *	This will cause routines to handle it properly
	 *	if they are accidentally pointed at it.
	 */

	/* Definition of SDBLK */
struct sdblk {
	struct sdblk *slforw;	/* Logical sequence forward link */
	struct sdblk *slback;	/* Logical sequence backward link */
	int sdflags;
	struct sdblk *sdforw;	/* Physical sequence (disk) */
	struct sdblk *sdback;	/* ditto - backptr for easy flushing */
	struct smblk *sdmem;	/* Mem pointer, 0 if no in-core version */
	SBFILE *sdfile;		/* File pointer, 0 if no disk version */
	chroff sdlen;		/* # chars in disk text */
	chroff sdaddr;		/* Disk address of text */
};
	/* Flags for "sdflags" */
#define SD_LOCK 0100000		/* Locked because opened by a SBBUF */
#define SD_LCK2	0040000		/* Locked for other reasons */
#define SD_MOD	0020000		/* Modified, mem blk is real stuff */
#define SD_NID	   0323		/* Node ID marks active (not on freelist) */
#define SD_LOCKS (SD_LOCK|SD_LCK2)

/* Note sdback is ONLY needed for fixing up phys list when a sdblk is
 * deleted (so as to find previous blk in phys list).  Perhaps it shd
 * be flushed (ie only use SDFORW)?  How to do deletions - use circular
 * list?  Sigh.
 */

	/* Definition of SMBLK (used by SBM routines) */
struct smblk {
	struct smblk *smforw;	/* Links to other mem blks, in phys order */
	struct smblk *smback;
	int smflags;		/* Type, in-use flags */
	SBMA smaddr;		/* Mem address of text */
	SBMO smlen;		/* # bytes in mem block */
	SBMO smuse;		/* # bytes "used" in block */
};
	/* Flags for "smflags" */
#define SM_USE	0100000		/* Block is in use (mem free if off) */
#define SM_NXM	 040000		/* Block mem is non-existent */
#define SM_EXT	 020000		/* Block mem owned by external (non-SBM) rtn*/
#define SM_MNODS 010000		/* Block holds SMBLK nodes */
#define SM_DNODS  04000		/* Block holds SDBLK nodes */
#define SM_NID	   0315		/* Node in-use identifier (low byte) */

/* Error handler type values */
#define SBMERR 0	/* Error in SBM package */
#define SBXERR 1	/* Error in SBSTR package */
#define SBFERR 2	/* "Error" - SBSTR package found a file overwritten.
			 *	Non-zero return will continue normally. */


/* Redefine certain external symbols to be unique in the first 6 chars
** to conform with ANSI requirements.
*/
#define sbm_nfre sbmnfre	/* SBM stuff */
#define sbm_nfor sbmnfor
#define sbm_nmov sbmnmov
#define sbm_ngc  sbmngc
#define sbx_ndget sbxndg	/* SBSTR stuff */
#define sbx_ndel  sbxnde
#define sbx_ndfre sbxndf
#define sbx_sdcpy sbxsdc
#define sbx_sdgc  sbxsdg
#define sbe_sdlist sbesls	/* SBERR stuff */
#define sbe_sdtab  sbestb
#define sbe_sds    sbesds
#define sbe_sbvfy  sbesbv
#define sbe_sbs    sbesbs

/* Forward declarations */
extern SBMA sbm_lowaddr;	/* For roundoff purposes */

extern SBFILE sbv_tf;		/* SBFILE for temp swapout file */
extern int (*sbv_debug)();	/* Error handler address */
extern off_t lseek();		/* For sbstr code mostly */
extern char *mktemp();
extern char *malloc();
extern char *calloc();
extern SBBUF *sb_open();
extern SBSTR *sb_close(), *sb_fduse(), *sbs_cpy(), *sbs_app(), *sb_cpyn(),
	*sb_killn();
extern struct sdblk *sbx_ready();
extern chroff sb_tell(), sb_ztell(), sbs_len();

/* Definition of SB_GETC, SB_PUTC, SB_BACKC macros */

#define sb_putc(s,c) (--((s)->sbwleft) >= 0 ? \
				(*(s)->sbiop++ = c) : sb_sputc(s,c))
#define sb_getc(s)   (--((s)->sbrleft) >= 0 ? \
				sb_uchartoint(*(s)->sbiop++) : sb_sgetc(s))
#define sb_peekc(s)  ((s)->sbrleft > 0 ? \
				sb_uchartoint(*(s)->sbiop)   : sb_speekc(s))

/* WARNING - sb_backc must ONLY be used if last operation was a
 * successful sb_getc!!  For slow but sure invocation use sb_rgetc.
 */
#define sb_backc(s) (++(s->sbrleft), --(s->sbiop))

#include "sbproto.h"	/* function prototypes */

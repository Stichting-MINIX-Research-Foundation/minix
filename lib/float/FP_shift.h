/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	include file for floating point package
*/

# define	CARRYBIT	0x80000000L
# define	NORMBIT		0x80000000L
# define	EXP_STORE	16


				/* parameters for Single Precision */
#define SGL_EXPSHIFT	7
#define SGL_M1LEFT	8
#define SGL_ZERO	0xffffff80L
#define SGL_EXACT	0xff
#define SGL_RUNPACK	SGL_M1LEFT

#define SGL_ROUNDUP	0x80
#define	SGL_CARRYOUT	0x01000000L
#define	SGL_MASK	0x007fffffL

				/* parameters for Double Precision */
				/* used in extend.c */

#define DBL_EXPSHIFT	4

#define DBL_M1LEFT	11

#define	DBL_RPACK	(32-DBL_M1LEFT)
#define	DBL_LPACK	DBL_M1LEFT

				/* used in compact.c */

#define DBL_ZERO	0xfffffd00L

#define DBL_EXACT	0x7ff

#define DBL_RUNPACK	DBL_M1LEFT
#define DBL_LUNPACK	(32-DBL_RUNPACK)

#define DBL_ROUNDUP	0x400
#define	DBL_CARRYOUT	0x00200000L
#define	DBL_MASK	0x000fffffL

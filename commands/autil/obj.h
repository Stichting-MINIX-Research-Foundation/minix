/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include <local.h>
#include <stdio.h>
#include <out.h>
#include <ranlib.h>
#include <arch.h>
#include "object.h"

#if ! defined(CHAR_UNSIGNED)
#define CHAR_UNSIGNED 0
#endif

#if CHAR_UNSIGNED
#define Xchar(ch)	(ch)
#else
#define Xchar(ch)	((ch) & 0377)
#endif

#if ! defined(BYTE_ORDER)
#define BYTE_ORDER 0x3210
#endif

#if (BYTE_ORDER == 0x3210 || BYTE_ORDER == 0x1032)
#define uget2(c)	(Xchar((c)[0]) | ((unsigned) Xchar((c)[1]) << 8))
#define Xput2(i, c)	(((c)[0] = (i)), ((c)[1] = (i) >> 8))
#define put2(i, c)	{ register int j = (i); Xput2(j, c); }
#else
#define uget2(c)	(* ((unsigned short *) (c)))
#define Xput2(i, c)	(* ((short *) (c)) = (i))
#define put2(i, c)	Xput2(i, c)
#endif

#define get2(c)		((short) uget2(c))

#if BYTE_ORDER != 0x0123
#define get4(c)		(uget2(c) | ((long) uget2((c)+2) << 16))
#define put4(l, c)	{ register long x=(l); \
			  Xput2((int)x,c); \
			  Xput2((int)(x>>16),(c)+2); \
			}
#else
#define get4(c)		(* ((long *) (c)))
#define put4(l, c)	(* ((long *) (c)) = (l))
#endif

#define SECTCNT	3	/* number of sections with own output buffer */
#if BIGMACHINE
#define WBUFSIZ	(8*BUFSIZ)
#else
#define WBUFSIZ	BUFSIZ
#endif

struct fil {
	int	cnt;
	char	*pnow;
	char	*pbegin;
	long	currpos;
	int	fd;
	char	pbuf[WBUFSIZ];
};

extern struct fil __parts[];

#define	PARTEMIT	0
#define	PARTRELO	(PARTEMIT+SECTCNT)
#define	PARTNAME	(PARTRELO+1)
#define	PARTCHAR	(PARTNAME+1)
#ifdef SYMDBUG
#define PARTDBUG	(PARTCHAR+1)
#else
#define PARTDBUG	(PARTCHAR+0)
#endif
#define	NPARTS		(PARTDBUG + 1)

#define getsect(s)      (PARTEMIT+((s)>=(SECTCNT-1)?(SECTCNT-1):(s)))

/*	$NetBSD: expand.h,v 1.4 2001/07/26 15:05:07 wiz Exp $	*/

/*
 * Expanding strings
 */
/* $Id: expand.h,v 1.4 2001/07/26 15:05:07 wiz Exp $ */

#define X_EXTRA		8	/* this many extra bytes in X string */

#if 0				/* Usage */
	XString xs;
	char *xp;

	Xinit(xs, xp, 128, ATEMP); /* allocate initial string */
	while ((c = generate()) {
		Xcheck(xs, xp);	/* expand string if necessary */
		Xput(xs, xp, c); /* add character */
	}
	return Xclose(xs, xp);	/* resize string */
/*
 * NOTE:
 *     The Xcheck and Xinit macros have a magic + X_EXTRA in the lengths.
 *     This is so that you can put up to X_EXTRA characters in a XString
 *     before calling Xcheck. (See yylex in lex.c)
 */
#endif /* 0 */

typedef struct XString {
	char   *end, *beg;	/* end, begin of string */
	size_t	len;		/* length */
	Area	*areap;		/* area to allocate/free from */
} XString;

typedef char * XStringP;

/* initialize expandable string */
#define	Xinit(xs, xp, length, area) do { \
			(xs).len = length; \
			(xs).areap = (area); \
			(xs).beg = alloc((xs).len + X_EXTRA, (xs).areap); \
			(xs).end = (xs).beg + (xs).len; \
			xp = (xs).beg; \
		} while (0)

/* stuff char into string */
#define	Xput(xs, xp, c)	(*xp++ = (c))

/* check if there are at least n bytes left */
#define	XcheckN(xs, xp, n) do { \
		    int more = ((xp) + (n)) - (xs).end; \
		    if (more > 0) \
			xp = Xcheck_grow_(&xs, xp, more); \
		} while (0)

/* check for overflow, expand string */
#define Xcheck(xs, xp)	XcheckN(xs, xp, 1)

/* free string */
#define	Xfree(xs, xp)	afree((void*) (xs).beg, (xs).areap)

/* close, return string */
#define	Xclose(xs, xp)	(char*) aresize((void*)(xs).beg, \
					(size_t)((xp) - (xs).beg), (xs).areap)
/* begin of string */
#define	Xstring(xs, xp)	((xs).beg)

#define Xnleft(xs, xp) ((xs).end - (xp))	/* may be less than 0 */
#define	Xlength(xs, xp) ((xp) - (xs).beg)
#define Xsize(xs, xp) ((xs).end - (xs).beg)
#define	Xsavepos(xs, xp) ((xp) - (xs).beg)
#define	Xrestpos(xs, xp, n) ((xs).beg + (n))

char *	Xcheck_grow_	ARGS((XString *xsp, char *xp, int more));

/*
 * expandable vector of generic pointers
 */

typedef struct XPtrV {
	void  **cur;		/* next avail pointer */
	void  **beg, **end;	/* begin, end of vector */
} XPtrV;

#define	XPinit(x, n) do { \
			register void **vp__; \
			vp__ = (void**) alloc(sizeofN(void*, n), ATEMP); \
			(x).cur = (x).beg = vp__; \
			(x).end = vp__ + n; \
		    } while (0)

#define	XPput(x, p) do { \
			if ((x).cur >= (x).end) { \
				int n = XPsize(x); \
				(x).beg = (void**) aresize((void*) (x).beg, \
						   sizeofN(void*, n*2), ATEMP); \
				(x).cur = (x).beg + n; \
				(x).end = (x).cur + n; \
			} \
			*(x).cur++ = (p); \
		} while (0)

#define	XPptrv(x)	((x).beg)
#define	XPsize(x)	((x).cur - (x).beg)

#define	XPclose(x)	(void**) aresize((void*)(x).beg, \
					 sizeofN(void*, XPsize(x)), ATEMP)

#define	XPfree(x)	afree((void*) (x).beg, ATEMP)

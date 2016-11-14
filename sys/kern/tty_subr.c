/*	$NetBSD: tty_subr.c,v 1.40 2011/09/24 00:05:38 christos Exp $	*/

/*
 * Copyright (c) 1993, 1994 Theo de Raadt
 * All rights reserved.
 *
 * Per Lindqvist <pgd@compuram.bbt.se> supplied an almost fully working
 * set of true clist functions that this is very loosely based on.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tty_subr.c,v 1.40 2011/09/24 00:05:38 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kmem.h>

/*
 * At compile time, choose:
 * There are two ways the TTY_QUOTE bit can be stored. If QBITS is
 * defined we allocate an array of bits -- 1/8th as much memory but
 * setbit(), clrbit(), and isset() take more CPU. If QBITS is
 * undefined, we just use an array of bytes.
 *
 * If TTY_QUOTE functionality isn't required by a line discipline,
 * it can free c_cq and set it to NULL. This speeds things up,
 * and also does not use any extra memory. This is useful for (say)
 * a SLIP line discipline that wants a 32K ring buffer for data
 * but doesn't need quoting.
 */
#define QBITS

#ifdef QBITS
#define QMEM(n)		((((n)-1)/NBBY)+1)
#else
#define QMEM(n)		(n)
#endif

#ifdef QBITS
static void	clrbits(u_char *, unsigned int, unsigned int);
#endif

/*
 * Initialize a particular clist. Ok, they are really ring buffers,
 * of the specified length, with/without quoting support.
 */
int
clalloc(struct clist *clp, int size, int quot)
{

	clp->c_cs = kmem_zalloc(size, KM_SLEEP);
	if (!clp->c_cs)
		return (-1);

	if (quot) {
		clp->c_cq = kmem_zalloc(QMEM(size), KM_SLEEP);
		if (!clp->c_cq) {
			kmem_free(clp->c_cs, size);
			return (-1);
		}
	} else
		clp->c_cq = NULL;

	clp->c_cf = clp->c_cl = NULL;
	clp->c_ce = clp->c_cs + size;
	clp->c_cn = size;
	clp->c_cc = 0;

	return (0);
}

void
clfree(struct clist *clp)
{
	if (clp->c_cs)
		kmem_free(clp->c_cs, clp->c_cn);
	if (clp->c_cq)
		kmem_free(clp->c_cq, QMEM(clp->c_cn));
	clp->c_cs = clp->c_cq = NULL;
}

/*
 * Get a character from a clist.
 */
int
getc(struct clist *clp)
{
	int c = -1;
	int s;

	s = spltty();
	if (clp->c_cc == 0)
		goto out;

	c = *clp->c_cf & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, clp->c_cf - clp->c_cs) )
			c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			c |= TTY_QUOTE;
#endif
	}
	*clp->c_cf = 0; /* wipe out to avoid information disclosure */
	if (++clp->c_cf == clp->c_ce)
		clp->c_cf = clp->c_cs;
	if (--clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
	return c;
}

/*
 * Copy clist to buffer.
 * Return number of bytes moved.
 */
int
q_to_b(struct clist *clp, u_char *cp, int count)
{
	int cc;
	u_char *p = cp;
	int s;

	s = spltty();
	/* optimize this while loop */
	while (count > 0 && clp->c_cc > 0) {
		cc = clp->c_cl - clp->c_cf;
		if (clp->c_cf >= clp->c_cl)
			cc = clp->c_ce - clp->c_cf;
		if (cc > count)
			cc = count;
		memcpy(p, clp->c_cf, cc);
		count -= cc;
		p += cc;
		clp->c_cc -= cc;
		clp->c_cf += cc;
		if (clp->c_cf == clp->c_ce)
			clp->c_cf = clp->c_cs;
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
	splx(s);
	return p - cp;
}

/*
 * Return count of contiguous characters in clist.
 * Stop counting if flag&character is non-null.
 */
int
ndqb(struct clist *clp, int flag)
{
	int count = 0;
	int i;
	int cc;
	int s;

	s = spltty();
	if ((cc = clp->c_cc) == 0)
		goto out;

	if (flag == 0) {
		count = clp->c_cl - clp->c_cf;
		if (count <= 0)
			count = clp->c_ce - clp->c_cf;
		goto out;
	}

	i = clp->c_cf - clp->c_cs;
	if (flag & TTY_QUOTE) {
		while (cc-- > 0 && !(clp->c_cs[i++] & (flag & ~TTY_QUOTE) ||
		    isset(clp->c_cq, i))) {
			count++;
			if (i == clp->c_cn)
				break;
		}
	} else {
		while (cc-- > 0 && !(clp->c_cs[i++] & flag)) {
			count++;
			if (i == clp->c_cn)
				break;
		}
	}
out:
	splx(s);
	return count;
}

/*
 * Flush count bytes from clist.
 */
void
ndflush(struct clist *clp, int count)
{
	int cc;
	int s;

	s = spltty();
	if (count == clp->c_cc) {
		clp->c_cc = 0;
		clp->c_cf = clp->c_cl = (u_char *)0;
		goto out;
	}
	/* optimize this while loop */
	while (count > 0 && clp->c_cc > 0) {
		cc = clp->c_cl - clp->c_cf;
		if (clp->c_cf >= clp->c_cl)
			cc = clp->c_ce - clp->c_cf;
		if (cc > count)
			cc = count;
		count -= cc;
		clp->c_cc -= cc;
		clp->c_cf += cc;
		if (clp->c_cf == clp->c_ce)
			clp->c_cf = clp->c_cs;
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
}

/*
 * Put a character into the output queue.
 */
int
putc(int c, struct clist *clp)
{
	int i;
	int s;

	s = spltty();
	if (clp->c_cc == clp->c_cn)
		goto out;

	if (clp->c_cc == 0) {
		if (!clp->c_cs) {
#if defined(DIAGNOSTIC) || 1
			printf("putc: required clalloc\n");
#endif
			if (clalloc(clp, clp->c_cn, 1)) {
out:
				splx(s);
				return -1;
			}
		}
		clp->c_cf = clp->c_cl = clp->c_cs;
	}

	*clp->c_cl = c & 0xff;
	i = clp->c_cl - clp->c_cs;
	if (clp->c_cq) {
#ifdef QBITS
		if (c & TTY_QUOTE)
			setbit(clp->c_cq, i);
		else
			clrbit(clp->c_cq, i);
#else
		q = clp->c_cq + i;
		*q = (c & TTY_QUOTE) ? 1 : 0;
#endif
	}
	clp->c_cc++;
	clp->c_cl++;
	if (clp->c_cl == clp->c_ce)
		clp->c_cl = clp->c_cs;
	splx(s);
	return 0;
}

#ifdef QBITS
/*
 * optimized version of
 *
 * for (i = 0; i < len; i++)
 *	clrbit(cp, off + len);
 */
static void
clrbits(u_char *cp, unsigned int off, unsigned int len)
{
	unsigned int sbi, ebi;
	u_char *scp, *ecp;
	unsigned int end;
	unsigned char mask;

	scp = cp + off / NBBY;
	sbi = off % NBBY;
	end = off + len + NBBY - 1;
	ecp = cp + end / NBBY - 1;
	ebi = end % NBBY + 1;
	if (scp >= ecp) {
		mask = ((1 << len) - 1) << sbi;
		*scp &= ~mask;
	} else {
		mask = (1 << sbi) - 1;
		*scp++ &= mask;

		mask = (1 << ebi) - 1;
		*ecp &= ~mask;

		while (scp < ecp)
			*scp++ = 0x00;
	}
}
#endif

/*
 * Copy buffer to clist.
 * Return number of bytes not transfered.
 */
int
b_to_q(const u_char *cp, int count, struct clist *clp)
{
	int cc;
	const u_char *p = cp;
	int s;

	if (count <= 0)
		return 0;

	s = spltty();
	if (clp->c_cc == clp->c_cn)
		goto out;

	if (clp->c_cc == 0) {
		if (!clp->c_cs) {
#if defined(DIAGNOSTIC) || 1
			printf("b_to_q: required clalloc\n");
#endif
			if (clalloc(clp, clp->c_cn, 1))
				goto out;
		}
		clp->c_cf = clp->c_cl = clp->c_cs;
	}

	/* optimize this while loop */
	while (count > 0 && clp->c_cc < clp->c_cn) {
		cc = clp->c_ce - clp->c_cl;
		if (clp->c_cf > clp->c_cl)
			cc = clp->c_cf - clp->c_cl;
		if (cc > count)
			cc = count;
		memcpy(clp->c_cl, p, cc);
		if (clp->c_cq) {
#ifdef QBITS
			clrbits(clp->c_cq, clp->c_cl - clp->c_cs, cc);
#else
			memset(clp->c_cl - clp->c_cs + clp->c_cq, 0, cc);
#endif
		}
		p += cc;
		count -= cc;
		clp->c_cc += cc;
		clp->c_cl += cc;
		if (clp->c_cl == clp->c_ce)
			clp->c_cl = clp->c_cs;
	}
out:
	splx(s);
	return count;
}

static int cc;

/*
 * Given a non-NULL pointer into the clist return the pointer
 * to the next character in the list or return NULL if no more chars.
 *
 * Callers must not allow getc's to happen between firstc's and getc's
 * so that the pointer becomes invalid.  Note that interrupts are NOT
 * masked.
 */
u_char *
nextc(struct clist *clp, u_char *cp, int *c)
{

	if (clp->c_cf == cp) {
		/*
		 * First time initialization.
		 */
		cc = clp->c_cc;
	}
	if (cc == 0 || cp == NULL)
		return NULL;
	if (--cc == 0)
		return NULL;
	if (++cp == clp->c_ce)
		cp = clp->c_cs;
	*c = *cp & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, cp - clp->c_cs))
			*c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			*c |= TTY_QUOTE;
#endif
	}
	return cp;
}

/*
 * Given a non-NULL pointer into the clist return the pointer
 * to the first character in the list or return NULL if no more chars.
 *
 * Callers must not allow getc's to happen between firstc's and getc's
 * so that the pointer becomes invalid.  Note that interrupts are NOT
 * masked.
 *
 * *c is set to the NEXT character
 */
u_char *
firstc(struct clist *clp, int *c)
{
	u_char *cp;

	cc = clp->c_cc;
	if (cc == 0)
		return NULL;
	cp = clp->c_cf;
	*c = *cp & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, cp - clp->c_cs))
			*c |= TTY_QUOTE;
#else
		if (*(cp - clp->c_cs + clp->c_cq))
			*c |= TTY_QUOTE;
#endif
	}
	return clp->c_cf;
}

/*
 * Remove the last character in the clist and return it.
 */
int
unputc(struct clist *clp)
{
	unsigned int c = -1;
	int s;

	s = spltty();
	if (clp->c_cc == 0)
		goto out;

	if (clp->c_cl == clp->c_cs)
		clp->c_cl = clp->c_ce - 1;
	else
		--clp->c_cl;
	clp->c_cc--;

	c = *clp->c_cl & 0xff;
	if (clp->c_cq) {
#ifdef QBITS
		if (isset(clp->c_cq, clp->c_cl - clp->c_cs))
			c |= TTY_QUOTE;
#else
		if (*(clp->c_cf - clp->c_cs + clp->c_cq))
			c |= TTY_QUOTE;
#endif
	}
	if (clp->c_cc == 0)
		clp->c_cf = clp->c_cl = (u_char *)0;
out:
	splx(s);
	return c;
}

/*
 * Put the chars in the from queue on the end of the to queue.
 */
void
catq(struct clist *from, struct clist *to)
{
	int c;

	while ((c = getc(from)) != -1)
		putc(c, to);
}

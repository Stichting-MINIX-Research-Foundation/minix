/*	$NetBSD: bt_subr.c,v 1.15 2010/11/13 13:52:11 uebayasi Exp $ */

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bt_subr.c	8.2 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bt_subr.c,v 1.15 2010/11/13 13:52:11 uebayasi Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <dev/sun/fbio.h>

#include <dev/sun/btreg.h>
#include <dev/sun/btvar.h>

/*
 * Common code for dealing with Brooktree video DACs.
 * (Contains some software-only code as well, since the colormap
 * ioctls are shared between the cgthree and cgsix drivers.)
 */

/*
 * Implement an FBIOGETCMAP-like ioctl.
 */
int
bt_getcmap(struct fbcmap *p, union bt_cmap *cm, int cmsize, int uspace)
{
	u_int i, start, count;
	int error = 0;
	u_char *cp, *r, *g, *b;
	u_char *cbuf = NULL;

	start = p->index;
	count = p->count;
	if (start >= cmsize || count > cmsize - start)
		return (EINVAL);

	if (uspace) {
		/* Allocate temporary buffer for color values */
		cbuf = malloc(3 * count * sizeof(char), M_TEMP, M_WAITOK);
		r = cbuf;
		g = r + count;
		b = g + count;
	} else {
		/* Direct access in kernel space */
		r = p->red;
		g = p->green;
		b = p->blue;
	}

	/* Copy colors from BT map to fbcmap */
	for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
		r[i] = cp[0];
		g[i] = cp[1];
		b[i] = cp[2];
	}

	if (uspace) {
		error = copyout(r, p->red, count);
		if (error)
			goto out;
		error = copyout(g, p->green, count);
		if (error)
			goto out;
		error = copyout(b, p->blue, count);
		if (error)
			goto out;
	}

out:
	if (cbuf != NULL)
		free(cbuf, M_TEMP);

	return (error);
}

/*
 * Implement the software portion of an FBIOPUTCMAP-like ioctl.
 */
int
bt_putcmap(struct fbcmap *p, union bt_cmap *cm, int cmsize, int uspace)
{
	u_int i, start, count;
	int error = 0;
	u_char *cp, *r, *g, *b;
	u_char *cbuf = NULL;

	start = p->index;
	count = p->count;
	if (start >= cmsize || count > cmsize - start)
		return (EINVAL);

	if (uspace) {
		/* Allocate temporary buffer for color values */
		cbuf = malloc(3 * count * sizeof(char), M_TEMP, M_WAITOK);
		r = cbuf;
		g = r + count;
		b = g + count;
		error = copyin(p->red, r, count);
		if (error)
			goto out;
		error = copyin(p->green, g, count);
		if (error)
			goto out;
		error = copyin(p->blue, b, count);
		if (error)
			goto out;
	} else {
		/* Direct access in kernel space */
		r = p->red;
		g = p->green;
		b = p->blue;
	}

	/* Copy colors from fbcmap to BT map */
	for (cp = &cm->cm_map[start][0], i = 0; i < count; cp += 3, i++) {
		cp[0] = r[i];
		cp[1] = g[i];
		cp[2] = b[i];
	}

out:
	if (cbuf != NULL)
		free(cbuf, M_TEMP);

	return (error);
}

/*
 * Initialize the color map to the default state:
 *
 *	- 0 is white			(PROM uses entry 0 for background)
 *	- all other entries are black	(PROM uses entry 255 for foreground)
 */
void
bt_initcmap(union bt_cmap *cm, int cmsize)
{
	int i;
	u_char *cp;

	cp = &cm->cm_map[0][0];
	cp[0] = cp[1] = cp[2] = 0xff;

	for (i = 1, cp = &cm->cm_map[i][0]; i < cmsize; cp += 3, i++)
		cp[0] = cp[1] = cp[2] = 0;

#ifdef RASTERCONSOLE
	if (cmsize > 16) {
		/*
		 * Setup an ANSI map at offset 1, for rasops;
		 * see dev/fb.c for usage (XXX - this should
		 * be replaced by more general colormap handling)
		 */
		extern u_char rasops_cmap[];
		memcpy(&cm->cm_map[1][0], rasops_cmap, 3*16);
	}
#endif
}

#if notyet
static void
bt_loadcmap_packed256(struct fbdevice *fb, volatile struct bt_regs *bt, int start, int ncolors)
{
	u_int v;
	int count, i;
	u_char *c[3], **p;
	struct cmap *cm = &fb->fb_cmap;

	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = &sc->sc_fbc->fbc_dac;
	bt->bt_addr = BT_D4M4(start);

	/*
	 * Figure out where to start in the RGB arrays
	 * See btreg.h for the way RGB triplets are packed into 4-byte words.
	 */
	c[0] = &cm->red[(4 * count) / 3];
	c[1] = &cm->green[(4 * count) / 3];
	c[2] = &cm->blue[(4 * count) / 3];
	p = &c[0];
	i = (4 * count) % 3;	/* This much of the last triplet is already in
				   the last packed word */
	while (i--) {
		c[1-i]++;
		p++;
	}


	while (--count >= 0) {
		u_int v = 0;

		/*
		 * Retrieve four colormap entries, pack them into
		 * a 32-bit word and write to the hardware register.
		 */
		for (i = 0; i < 4; i++) {
			u_char *cp = *p;
			v |= *cp++ << (8 * i);
			*p = cp;
			if (p++ == &c[2])
				/* Wrap around */
				p = &c[0];
		}

		bt->bt_cmap = v;
	}
}
#endif

/*	$NetBSD: pfour_subr.c,v 1.7 2009/03/14 15:36:21 dsl Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Support routines for pfour framebuffers.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pfour_subr.c,v 1.7 2009/03/14 15:36:21 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/sun/pfourreg.h>
#include <dev/sun/fbio.h>
#include <dev/sun/fbvar.h>

void
fb_setsize_pfour(struct fbdevice *fb)
{
#if defined(SUN4)
	volatile u_int32_t pfour;
	int width, height;

	/*
	 * Some pfour framebuffers, e.g. the
	 * cgsix, don't encode resolution the
	 * same, so the driver handles that.
	 * The driver can let us know that it
	 * needs to do this by not mapping in
	 * the pfour register by the time this
	 * routine is called.
	 */
	if (fb->fb_pfour == NULL)
		return;

	pfour = *fb->fb_pfour;

	/*
	 * Use the pfour register to determine
	 * the size.  Note that the cgsix and
	 * cgeight don't use this size encoding.
	 * In this case, we have to settle
	 * for the defaults we were provided
	 * with.
	 */
	if ((PFOUR_ID(pfour) == PFOUR_ID_COLOR24) ||
	    (PFOUR_ID(pfour) == PFOUR_ID_FASTCOLOR))
		return;

	switch (PFOUR_SIZE(pfour)) {
	case PFOUR_SIZE_1152X900:
		width = 1152;
		height = 900;
		break;

	case PFOUR_SIZE_1024X1024:
		width = 1024;
		height = 1024;
		break;

	case PFOUR_SIZE_1280X1024:
		width = 1280;
		height = 1024;
		break;

	case PFOUR_SIZE_1600X1280:
		width = 1600;
		height = 1280;
		break;

	case PFOUR_SIZE_1440X1440:
		width = 1440;
		height = 1440;
		break;

	case PFOUR_SIZE_640X480:
		width = 640;
		height = 480;
		break;

	default:

		/*
		 * Use the defaults already filled in by the generic fb code.
		 */

		return;
	}

	fb->fb_type.fb_width = width;
	fb->fb_type.fb_height = height;
#endif /* SUN4 */
}


/*
 * Probe for a pfour framebuffer.  Return values:
 *
 *	PFOUR_NOTPFOUR: framebuffer is not a pfour framebuffer
 *	otherwise returns pfour ID
 */
int
fb_pfour_id(volatile void *va)
{
#if defined(SUN4)
	volatile u_int32_t val, save, *pfour = va;

	/* Read the pfour register. */
	save = *pfour;

	/*
	 * Try to modify the type code.  If it changes, put the
	 * original value back, and notify the caller that it's
	 * not a pfour framebuffer.
	 */
	val = save & ~PFOUR_REG_RESET;
	*pfour = (val ^ PFOUR_FBTYPE_MASK);
	if ((*pfour ^ val) & PFOUR_FBTYPE_MASK) {
		*pfour = save;
		return (PFOUR_NOTPFOUR);
	}

	return (PFOUR_ID(val));
#else
	return (PFOUR_NOTPFOUR);
#endif /* SUN4 */
}

/*
 * Return the status of the video enable.
 */
int
fb_pfour_get_video(struct fbdevice *fb)
{

	return ((*fb->fb_pfour & PFOUR_REG_VIDEO) != 0);
}

/*
 * Enable or disable the framebuffer.
 */
void
fb_pfour_set_video(struct fbdevice *fb, int enable)
{
	volatile u_int32_t pfour;

	pfour = *fb->fb_pfour & ~(PFOUR_REG_INTCLR|PFOUR_REG_VIDEO);
	*fb->fb_pfour = pfour | (enable ? PFOUR_REG_VIDEO : 0);
}

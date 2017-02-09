/*	$NetBSD: video_subr.h,v 1.5 2008/04/28 20:23:48 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#define LEGAL_CLUT_INDEX(x)	((x) >= 0 && (x) <= 255)
#define RGB24(r, g, b)		((((r) << 16) & 0x00ff0000) |		\
				 (((g) << 8) & 0x0000ff00) |		\
				 (((b)) & 0x000000ff))

int cmap_work_alloc(u_int8_t **, u_int8_t **, u_int8_t **, u_int32_t **, int);
void cmap_work_free(u_int8_t *, u_int8_t *, u_int8_t *, u_int32_t *);
void rgb24_compose(u_int32_t *, u_int8_t *, u_int8_t *, u_int8_t *, int);
void rgb24_decompose(u_int32_t *, u_int8_t *, u_int8_t *, u_int8_t *, int);

/* debug function */
struct video_chip;
struct video_chip {
	void *vc_v; /* CPU/chipset dependent goo */

	vaddr_t vc_fbvaddr;
	paddr_t vc_fbpaddr;
	size_t vc_fbsize;
	int vc_fbdepth;
	int vc_fbwidth;
	int vc_fbheight;
	int vc_reverse;

	void (*vc_drawline)(struct video_chip *, int, int, int, int);
	void (*vc_drawdot)(struct video_chip *, int, int);
};

void video_attach_drawfunc(struct video_chip *);
void video_line(struct video_chip *, int, int, int, int);
void video_dot(struct video_chip *, int, int);
void video_calibration_pattern(struct video_chip *);
int video_reverse_color(void);

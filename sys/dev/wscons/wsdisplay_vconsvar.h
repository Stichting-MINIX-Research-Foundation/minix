/*	$NetBSD: wsdisplay_vconsvar.h,v 1.23 2014/03/18 18:20:42 riastradh Exp $ */

/*-
 * Copyright (c) 2005, 2006 Michael Lorenz
 * All rights reserved.
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

#ifndef _WSDISPLAY_VCONS_H_
#define _WSDISPLAY_VCONS_H_

#ifdef _KERNEL_OPT
#include "opt_wsdisplay_compat.h"
#include "opt_vcons.h"
#endif

struct vcons_data;

struct vcons_screen {
	struct rasops_info scr_ri;
	LIST_ENTRY(vcons_screen) next;
	void *scr_cookie;
	struct vcons_data *scr_vd;
	struct vcons_data *scr_origvd;
	const struct wsscreen_descr *scr_type;
	uint32_t *scr_chars;
	long *scr_attrs;
	long scr_defattr;
	/* static flags set by the driver */
	uint32_t scr_flags;
#define VCONS_NO_REDRAW		1	/* don't readraw in switch_screen */
#define	VCONS_SCREEN_IS_STATIC	2	/* don't free() this vcons_screen */
#define VCONS_SWITCH_NEEDS_POLLING 4	/* rasops can overlap so we need to
					 * poll the busy flag when switching
					 * - for drivers that use software
					 * drawing */
#define VCONS_DONT_DRAW		8	/* don't draw on this screen at all */
/*
 * the following flags are for drivers which either can't accelerate (all) copy 
 * operations or where drawing characters is faster than the blitter
 * for example, Sun's Creator boards can't accelerate copycols()
 */
#define VCONS_NO_COPYCOLS	0x10	/* use putchar() based copycols() */
#define VCONS_NO_COPYROWS	0x20	/* use putchar() based copyrows() */
#define VCONS_DONT_READ		0x30	/* avoid framebuffer reads */
	/* status flags used by vcons */
	uint32_t scr_status;
#define VCONS_IS_VISIBLE	1	/* this screen is currently visible */
	/* non zero when some rasops operation is in progress */
	int scr_busy;
#ifdef WSDISPLAY_SCROLLSUPPORT
	int scr_lines_in_buffer;
	int scr_current_line;
	int scr_line_wanted;
	int scr_offset_to_zero;
	int scr_current_offset;
#endif
#ifdef VCONS_DRAW_INTR
	unsigned int scr_dirty;
#endif
};

#define SCREEN_IS_VISIBLE(scr) (((scr)->scr_status & VCONS_IS_VISIBLE) != 0)
#define SCREEN_IS_BUSY(scr) ((scr)->scr_busy != 0)
#define SCREEN_CAN_DRAW(scr) (((scr)->scr_flags & VCONS_DONT_DRAW) == 0)
#define SCREEN_BUSY(scr) ((scr)->scr_busy = 1)
#define SCREEN_IDLE(scr) ((scr)->scr_busy = 0)
#define SCREEN_VISIBLE(scr) ((scr)->scr_status |= VCONS_IS_VISIBLE)
#define SCREEN_INVISIBLE(scr) ((scr)->scr_status &= ~VCONS_IS_VISIBLE)
#define SCREEN_DISABLE_DRAWING(scr) ((scr)->scr_flags |= VCONS_DONT_DRAW)
#define SCREEN_ENABLE_DRAWING(scr) ((scr)->scr_flags &= ~VCONS_DONT_DRAW)

struct vcons_data {
	/* usually the drivers softc */
	void *cookie;
	
	/*
	 * setup the rasops part of the passed vcons_screen, like
	 * geometry, framebuffer address, font, characters, acceleration.
	 * we pass the cookie as 1st parameter
	 */
	void (*init_screen)(void *, struct vcons_screen *, int, 
	    long *);

	/* accessops */
	int (*ioctl)(void *, void *, u_long, void *, int, struct lwp *);

	/* rasops */
	void (*copycols)(void *, int, int, int, int);
	void (*erasecols)(void *, int, int, int, long);
	void (*copyrows)(void *, int, int, int);
	void (*eraserows)(void *, int, int, long);
	void (*putchar)(void *, int, int, u_int, long);
	void (*cursor)(void *, int, int, int);
	/* called before vcons_redraw_screen */
	void (*show_screen_cb)(struct vcons_screen *);
	/* virtual screen management stuff */
	void (*switch_cb)(void *, int, int);
	void *switch_cb_arg;
	struct callout switch_callout;
	uint32_t switch_pending;
	LIST_HEAD(, vcons_screen) screens;
	struct vcons_screen *active, *wanted;
	const struct wsscreen_descr *currenttype;
	int switch_poll_count;
#ifdef VCONS_DRAW_INTR
	int cells;
	long *attrs;
	uint32_t *chars;
	int cursor_offset;
	callout_t intr;
	int intr_valid;
	void *intr_softint;
	int use_intr;		/* use intr drawing when non-zero */
#endif
};

int	vcons_init(struct vcons_data *, void *cookie, struct wsscreen_descr *,
    struct wsdisplay_accessops *);

int	vcons_init_screen(struct vcons_data *, struct vcons_screen *, int,
    long *);

/* completely redraw the screen, clear it if RI_FULLCLEAR is set */
void	vcons_redraw_screen(struct vcons_screen *);

#ifdef VCONS_DRAW_INTR
/* redraw all dirty character cells */
void	vcons_update_screen(struct vcons_screen *);
void	vcons_invalidate_cache(struct vcons_data *);
#else
#define vcons_update_screen vcons_redraw_screen
#endif

void	vcons_replay_msgbuf(struct vcons_screen *);

void	vcons_enable_polling(struct vcons_data *);
void	vcons_disable_polling(struct vcons_data *);
void	vcons_hard_switch(struct vcons_screen *);

#endif /* _WSDISPLAY_VCONS_H_ */

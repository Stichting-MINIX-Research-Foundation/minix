/*	$NetBSD: wsdisplay_vcons.c,v 1.39.4.1 2019/08/15 12:21:27 martin Exp $ */

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

#define _KERNTYPES
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: wsdisplay_vcons.c,v 1.39.4.1 2019/08/15 12:21:27 martin Exp $");

#include <sys/param.h>
//#include <sys/systm.h>
//#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
//#include <sys/malloc.h>
#include <sys/mman.h>
//#include <sys/tty.h>
#include <sys/termios.h>
#include <sys/conf.h>
#include <sys/proc.h>
//#include <sys/kthread.h>
//#include <sys/tprintf.h>
//#include <sys/atomic.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include "tty.h"

#include "wsdisplayvar.h"
#include "wsconsio.h"
#include "./wsfont/wsfont.h"
#include "./rasops/rasops.h"

#include "wsdisplay_vconsvar.h"

#ifdef VCONS_DEBUG
#define DPRINTF printf
#else
#define DPRINTF if (0) printf
#endif

static void vcons_dummy_init_screen(void *, struct vcons_screen *, int, 
	    long *);

static int  vcons_ioctl(void *, void *, u_long, void *, int);
static int  vcons_alloc_screen(void *, const struct wsscreen_descr *, void **, 
	    int *, int *, long *);
static void vcons_free_screen(void *, void *);
static int  vcons_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);
static int  vcons_load_font(void *, void *, struct wsdisplay_font *);

#ifdef WSDISPLAY_SCROLLSUPPORT
static void vcons_scroll(void *, void *, int);
static void vcons_do_scroll(struct vcons_screen *);
#endif

static void vcons_do_switch(void *);

/* methods that work only on text buffers */
static void vcons_copycols_buffer(void *, int, int, int, int);
static void vcons_erasecols_buffer(void *, int, int, int, long);
static void vcons_copyrows_buffer(void *, int, int, int);
static void vcons_eraserows_buffer(void *, int, int, long);
static void vcons_putchar_buffer(void *, int, int, u_int, long);

/*
 * actual wrapper methods which call both the _buffer ones above and the
 * driver supplied ones to do the drawing
 */
static void vcons_copycols(void *, int, int, int, int);
static void vcons_erasecols(void *, int, int, int, long);
static void vcons_copyrows(void *, int, int, int);
static void vcons_eraserows(void *, int, int, long);
static void vcons_putchar(void *, int, int, u_int, long);
static void vcons_cursor(void *, int, int, int);

/*
 * methods that avoid framebuffer reads
 */
static void vcons_copycols_noread(void *, int, int, int, int);
static void vcons_copyrows_noread(void *, int, int, int);


/* support for reading/writing text buffers. For wsmoused */
static int  vcons_putwschar(struct vcons_screen *, struct wsdisplay_char *);
static int  vcons_getwschar(struct vcons_screen *, struct wsdisplay_char *);

static void vcons_lock(struct vcons_screen *);
static void vcons_unlock(struct vcons_screen *);

int vcons_init(struct vcons_data *vd, void *cookie, struct wsscreen_descr *def,
    struct wsdisplay_accessops *ao)
{
	//ser_puts("vcons_init 1\n");
	/* zero out everything so we can rely on untouched fields being 0 */
	memset(vd, 0, sizeof(struct vcons_data));
	
	//ser_puts("vcons_init 2\n");
	vd->cookie = cookie;

	vd->init_screen = vcons_dummy_init_screen;
	vd->show_screen_cb = NULL;

	/* keep a copy of the accessops that we replace below with our
	 * own wrappers */
	vd->ioctl = ao->ioctl;

	/* configure the accessops */
	ao->ioctl = vcons_ioctl;
	ao->alloc_screen = vcons_alloc_screen;
	ao->free_screen = vcons_free_screen;
	ao->show_screen = vcons_show_screen;
	ao->load_font = vcons_load_font;
#ifdef WSDISPLAY_SCROLLSUPPORT
	ao->scroll = vcons_scroll;
#endif

	LIST_INIT(&vd->screens);
	vd->active = NULL;
	vd->wanted = NULL;
	vd->currenttype = def;
	vd->defaulttype = def;
	//callout_init(&vd->switch_callout, 0);
	//callout_setfunc(&vd->switch_callout, vcons_do_switch, vd);

	/*
	 * a lock to serialize access to the framebuffer.
	 * when switching screens we need to make sure there's no rasops
	 * operation in progress
	 */
#ifdef DIAGNOSTIC
	vd->switch_poll_count = 0;
#endif
	return 0;
}

static void vcons_lock(struct vcons_screen *scr)
{
	SCREEN_BUSY(scr);
}

static void vcons_unlock(struct vcons_screen *scr)
{
	SCREEN_IDLE(scr);
}

static void vcons_dummy_init_screen(void *cookie,
    struct vcons_screen *scr, int exists,
    long *defattr)
{

	/*
	 * default init_screen() method.
	 * Needs to be overwritten so we bitch and whine in case anyone ends
	 * up in here.
	 */
	printf("vcons_init_screen: dummy function called. Your driver is "
	       "supposed to supply a replacement for proper operation\n");
}

static int vcons_alloc_buffers(struct vcons_data *vd, struct vcons_screen *scr)
{
	struct rasops_info *ri = &scr->scr_ri;
	int cnt, i;

	/* 
	 * we allocate both chars and attributes in one chunk, attributes first 
	 * because they have the (potentially) bigger alignment 
	 */
#ifdef WSDISPLAY_SCROLLSUPPORT
	cnt = (ri->ri_rows + WSDISPLAY_SCROLLBACK_LINES) * ri->ri_cols;
	scr->scr_lines_in_buffer = WSDISPLAY_SCROLLBACK_LINES;
	scr->scr_current_line = 0;
	scr->scr_line_wanted = 0;
	scr->scr_offset_to_zero = ri->ri_cols * WSDISPLAY_SCROLLBACK_LINES;
	scr->scr_current_offset = scr->scr_offset_to_zero;
#else
	cnt = ri->ri_rows * ri->ri_cols;
#endif
	scr->scr_attrs = malloc(cnt * (sizeof(long) + 
	    sizeof(uint32_t)));
	if (scr->scr_attrs == NULL)
		return ENOMEM;

	scr->scr_chars = (uint32_t *)&scr->scr_attrs[cnt];

	/* 
	 * fill the attribute buffer with *defattr, chars with 0x20 
	 * since we don't know if the driver tries to mimic firmware output or
	 * reset everything we do nothing to VRAM here, any driver that feels
	 * the need to clear screen or something will have to do it on its own
	 * Additional screens will start out in the background anyway so
	 * cleaning or not only really affects the initial console screen
	 */
	for (i = 0; i < cnt; i++) {
		scr->scr_attrs[i] = scr->scr_defattr;
		scr->scr_chars[i] = 0x20;
	}

	return 0;
}

int vcons_init_screen(struct vcons_data *vd, struct vcons_screen *scr,
    int existing, long *defattr)
{
	struct rasops_info *ri = &scr->scr_ri;
	int i;

	//ser_puts("vcons_init_screen 1\n");
	scr->scr_cookie = vd->cookie;
	scr->scr_vd = scr->scr_origvd = vd;
	scr->scr_busy = 0;
	if (scr->scr_type == NULL)
		scr->scr_type = vd->defaulttype;
	
	/*
	 * call the driver-supplied init_screen function which is expected
	 * to set up rasops_info, override cursor() and probably others
	 */
	vd->init_screen(vd->cookie, scr, existing, defattr);
	//ser_puts("vcons_init_screen 2\n");

	/* 
	 * save the non virtual console aware rasops and replace them with 
	 * our wrappers
	 */
	vd->eraserows = ri->ri_ops.eraserows;
	vd->erasecols = ri->ri_ops.erasecols;
	scr->putchar   = ri->ri_ops.putchar;
	vd->cursor    = ri->ri_ops.cursor;

	if (scr->scr_flags & VCONS_NO_COPYCOLS) {
		vd->copycols  = vcons_copycols_noread;
	} else {
		vd->copycols = ri->ri_ops.copycols;
	}

	if (scr->scr_flags & VCONS_NO_COPYROWS) {
		vd->copyrows  = vcons_copyrows_noread;
	} else {
		vd->copyrows = ri->ri_ops.copyrows;
	}

	ri->ri_ops.eraserows = vcons_eraserows;	
	ri->ri_ops.erasecols = vcons_erasecols;	
	ri->ri_ops.putchar   = vcons_putchar;
	ri->ri_ops.cursor    = vcons_cursor;
	ri->ri_ops.copycols  = vcons_copycols;
	ri->ri_ops.copyrows  = vcons_copyrows;


	ri->ri_hw = scr;

	i = ri->ri_ops.allocattr(ri, WS_DEFAULT_FG, WS_DEFAULT_BG, 0, defattr);
	if (i != 0) {
#ifdef DIAGNOSTIC
		printf("vcons: error allocating attribute %d\n", i);
#endif
		scr->scr_defattr = 0;
	} else
		scr->scr_defattr = *defattr;

	vcons_alloc_buffers(vd, scr);

	if(vd->active == NULL) {
		vd->active = scr;
		SCREEN_VISIBLE(scr);
	}
	
	if (existing) {
		SCREEN_VISIBLE(scr);
		vd->active = scr;
	} else {
		SCREEN_INVISIBLE(scr);
	}
	
	LIST_INSERT_HEAD(&vd->screens, scr, next);
	return 0;
}

static int vcons_load_font(void *v, void *cookie, struct wsdisplay_font *f)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr = cookie;
	struct rasops_info *ri;
	struct wsdisplay_font *font;
	int flags = WSFONT_FIND_BITMAP, fcookie;

	/* see if we're asked to add a font or use it */
	if (scr == NULL)
		return 0;

	ri = &scr->scr_ri;

	/* see if the driver knows how to handle multiple fonts */
	if ((scr->scr_flags & VCONS_LOADFONT) == 0) {
		return EOPNOTSUPP;
	}

	/* now see what fonts we can use */
	if (ri->ri_flg & RI_ENABLE_ALPHA) {
		flags |= WSFONT_FIND_ALPHA;
	}

	fcookie = wsfont_find(f->name, 0, 0, 0, 0, 0, flags);
	if (fcookie == -1)
		return EINVAL;

	wsfont_lock(fcookie, &font);
	if (font == NULL)
		return EINVAL;

	/* ok, we got a font. Now clear the screen with the old parameters */
	if (SCREEN_IS_VISIBLE(scr))
		vd->eraserows(ri, 0, ri->ri_rows, scr->scr_defattr);

	vcons_lock(vd->active);
	/* set the new font and re-initialize things */
	ri->ri_font = font;
	wsfont_unlock(ri->ri_wsfcookie);
	ri->ri_wsfcookie = fcookie;

	vd->init_screen(vd->cookie, scr, 1, &scr->scr_defattr);
	DPRINTF("caps %x %x\n", scr->scr_type->capabilities, ri->ri_caps);
	if (scr->scr_type->capabilities & WSSCREEN_RESIZE) {
		scr->scr_type->nrows = ri->ri_rows;
		scr->scr_type->ncols = ri->ri_cols;
		DPRINTF("new size %d %d\n", ri->ri_rows, ri->ri_cols);
	}


	/* now, throw the old buffers away */
	if (scr->scr_attrs)
		free(scr->scr_attrs);
	/* allocate new buffers */
	vcons_alloc_buffers(vd, scr);
	
	/* save the potentially changed ri_ops */ 
	vd->eraserows = ri->ri_ops.eraserows;
	vd->erasecols = ri->ri_ops.erasecols;
	scr->putchar   = ri->ri_ops.putchar;
	vd->cursor    = ri->ri_ops.cursor;

	if (scr->scr_flags & VCONS_NO_COPYCOLS) {
		vd->copycols  = vcons_copycols_noread;
	} else {
		vd->copycols = ri->ri_ops.copycols;
	}

	if (scr->scr_flags & VCONS_NO_COPYROWS) {
		vd->copyrows  = vcons_copyrows_noread;
	} else {
		vd->copyrows = ri->ri_ops.copyrows;
	}

	/* and put our wrappers back */
	ri->ri_ops.eraserows = vcons_eraserows;	
	ri->ri_ops.erasecols = vcons_erasecols;	
	ri->ri_ops.putchar   = vcons_putchar;
	ri->ri_ops.cursor    = vcons_cursor;
	ri->ri_ops.copycols  = vcons_copycols;
	ri->ri_ops.copyrows  = vcons_copyrows;
	vcons_unlock(vd->active);

	/* notify things that we're about to redraw */
	if (vd->show_screen_cb != NULL)
		vd->show_screen_cb(scr, vd->show_screen_cookie);
	
	/* no need to draw anything, wsdisplay should reset the terminal */

	return 0;
}	

static void vcons_do_switch(void *arg)
{
	struct vcons_data *vd = arg;
	struct vcons_screen *scr, *oldscr;

	scr = vd->wanted;
	if (!scr) {
		printf("vcons_switch_screen: disappeared\n");
		vd->switch_cb(vd->switch_cb_arg, EIO, 0);
		return;
	}
	oldscr = vd->active; /* can be NULL! */

	/* 
	 * if there's an old, visible screen we mark it invisible and wait
	 * until it's not busy so we can safely switch 
	 */
	if (oldscr != NULL) {
		SCREEN_INVISIBLE(oldscr);
		if (SCREEN_IS_BUSY(oldscr)) {
			//callout_schedule(&vd->switch_callout, 1);
#ifdef DIAGNOSTIC
			/* bitch if we wait too long */
			vd->switch_poll_count++;
			if (vd->switch_poll_count > 100) {
				panic("vcons: screen still busy");
			}
#endif
			return;
		}
		/* invisible screen -> no visible cursor image */
		oldscr->scr_ri.ri_flg &= ~RI_CURSOR;
#ifdef DIAGNOSTIC
		vd->switch_poll_count = 0;
#endif
	}

	if (scr == oldscr)
		return;

#ifdef DIAGNOSTIC
	if (SCREEN_IS_VISIBLE(scr))
		printf("vcons_switch_screen: already active");
#endif

#ifdef notyet
	if (vd->currenttype != type) {
		vcons_set_screentype(vd, type);
		vd->currenttype = type;
	}
#endif

	SCREEN_VISIBLE(scr);
	vd->active = scr;
	vd->wanted = NULL;

	if (vd->show_screen_cb != NULL)
		vd->show_screen_cb(scr, vd->show_screen_cookie);

	if ((scr->scr_flags & VCONS_NO_REDRAW) == 0)
		vcons_redraw_screen(scr);

	if (vd->switch_cb)
		vd->switch_cb(vd->switch_cb_arg, 0, 0);
}

void vcons_redraw_screen(struct vcons_screen *scr)
{
	uint32_t *charptr = scr->scr_chars, c;
	long *attrptr = scr->scr_attrs, a, last_a = 0, mask, cmp, acmp;
	struct rasops_info *ri = &scr->scr_ri;
	struct vcons_data *vd = scr->scr_vd;
	int i, j, offset, boffset = 0, start = -1;

	mask = 0x00ff00ff;	/* background and flags */
	cmp = 0xffffffff;	/* never match anything */
	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {

		/*
		 * only clear the screen when RI_FULLCLEAR is set since we're
		 * going to overwrite every single character cell anyway
		 */
		if (ri->ri_flg & RI_FULLCLEAR) {
			vd->eraserows(ri, 0, ri->ri_rows,
			    scr->scr_defattr);
			cmp = scr->scr_defattr & mask;
		}

		/* redraw the screen */
#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		for (i = 0; i < ri->ri_rows; i++) {
			start = -1;
			for (j = 0; j < ri->ri_cols; j++) {
				/*
				 * no need to use the wrapper function - we 
				 * don't change any characters or attributes
				 * and we already made sure the screen we're
				 * working on is visible
				 */
				c = charptr[offset];
				a = attrptr[offset];
				acmp = a & mask;
				if (c == ' ') {
					/*
					 * if we already erased the background
					 * and this blank uses the same colour
					 * and flags we don't need to do
					 * anything here
					 */
					if (acmp == cmp)
						goto next;
					/*
					 * see if we can optimize things a
					 * little bit by drawing stretches of
					 * blanks using erasecols
					 */
					
					if (start == -1) {
						start = j;
						last_a = acmp;
					} else if (acmp != last_a) {
						/*
						 * different attr, need to
						 * flush & restart 
						 */
						vd->erasecols(ri, i, start,
						    j - start, last_a);
						start = j;
						last_a = acmp;
					}
				} else {
					if (start != -1) {
						vd->erasecols(ri, i, start,
						    j - start, last_a);
						start = -1;
					}
#undef putchar							
					scr->putchar(ri, i, j, c, a);
				}
next:
				offset++;
				boffset++;
			}
			/* end of the line - draw all defered blanks, if any */
			if (start != -1) {
				vd->erasecols(ri, i, start, j - start, last_a);
			}			
		}
		ri->ri_flg &= ~RI_CURSOR;
		scr->scr_vd->cursor(ri, 1, ri->ri_crow, ri->ri_ccol);
	}
	vcons_unlock(scr);
}

static int vcons_ioctl(void *v, void *vs, u_long cmd, void *data, int flag)
{
	struct vcons_data *vd = v;
	int error = 0;


	switch (cmd) {
	case WSDISPLAYIO_GETWSCHAR:
		error = vcons_getwschar((struct vcons_screen *)vs,
			(struct wsdisplay_char *)data);
		break;

	case WSDISPLAYIO_PUTWSCHAR:
		error = vcons_putwschar((struct vcons_screen *)vs,
			(struct wsdisplay_char *)data);
		break;

	case WSDISPLAYIO_SET_POLLING: {
		int poll = *(int *)data;

		/* first call the driver's ioctl handler */
		if (vd->ioctl != NULL)
			error = (*vd->ioctl)(v, vs, cmd, data, flag);
		if (poll) {
			vcons_enable_polling(vd);
			vcons_hard_switch(LIST_FIRST(&vd->screens));
		} else
			vcons_disable_polling(vd);
		}
		break;

	default:
		if (vd->ioctl != NULL)
			error = (*vd->ioctl)(v, vs, cmd, data, flag);
		else
			error = ENXIO;
	}

	return error;
}

static int vcons_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr;
	struct wsscreen_descr *t = __UNCONST(type);
	int ret;

	scr = malloc(sizeof(struct vcons_screen));
	if (scr == NULL)
		return ENOMEM;

	scr->scr_flags = 0;		
	scr->scr_status = 0;
	scr->scr_busy = 0;
	scr->scr_type = __UNCONST(type);

	ret = vcons_init_screen(vd, scr, 0, defattrp);
	if (ret != 0) {
		free(scr);
		return ret;
	}
	if (t->capabilities & WSSCREEN_RESIZE) {
		t->nrows = scr->scr_ri.ri_rows;
		t->ncols = scr->scr_ri.ri_cols;
	}

	if (vd->active == NULL) {
		SCREEN_VISIBLE(scr);
		vd->active = scr;
		vd->currenttype = type;
	}

	*cookiep = scr;
	*curxp = scr->scr_ri.ri_ccol;
	*curyp = scr->scr_ri.ri_crow;
	return 0;
}

static void vcons_free_screen(void *v, void *cookie)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr = cookie;

	vcons_lock(scr);
	/* there should be no rasops activity here */

	LIST_REMOVE(scr, next);

	if ((scr->scr_flags & VCONS_SCREEN_IS_STATIC) == 0) {
		free(scr->scr_attrs);
		free(scr);
	} else {
		/*
		 * maybe we should just restore the old rasops_info methods
		 * and free the character/attribute buffer here?
		 */
#ifdef VCONS_DEBUG
		panic("vcons_free_screen: console");
#else
		printf("vcons_free_screen: console\n");
#endif
	}

	if (vd->active == scr)
		vd->active = NULL;
}

static int vcons_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cb_arg)
{
	struct vcons_data *vd = v;
	struct vcons_screen *scr;

	scr = cookie;
	if (scr == vd->active)
		return 0;

	vd->wanted = scr;
	vd->switch_cb = cb;
	vd->switch_cb_arg = cb_arg;
	if (cb) {
		//callout_schedule(&vd->switch_callout, 0);
		return EAGAIN;
	}

	vcons_do_switch(vd);
	return 0;
}

/* wrappers for rasops_info methods */

static void vcons_copycols_buffer(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int from = srccol + row * ri->ri_cols;
	int to = dstcol + row * ri->ri_cols;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	memmove(&scr->scr_attrs[offset + to], &scr->scr_attrs[offset + from],
	    ncols * sizeof(long));
	memmove(&scr->scr_chars[offset + to], &scr->scr_chars[offset + from],
	    ncols * sizeof(uint32_t));
#else
	memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
	    ncols * sizeof(long));
	memmove(&scr->scr_chars[to], &scr->scr_chars[from],
	    ncols * sizeof(uint32_t));
#endif

}

static void vcons_copycols(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_copycols_buffer(cookie, row, srccol, dstcol, ncols);

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->scr_vd->copycols(cookie, row, srccol, dstcol, ncols);
	}
	vcons_unlock(scr);
}

static void vcons_copycols_noread(void *cookie, int row, int srccol, int dstcol, int ncols)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		int pos, c, offset, ppos;

#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		ppos = ri->ri_cols * row + dstcol;
		pos = ppos + offset;
		for (c = dstcol; c < (dstcol + ncols); c++) {
			scr->putchar(cookie, row, c, scr->scr_chars[pos],
			    scr->scr_attrs[pos]);
			pos++;
			ppos++;
		}
	}
	vcons_unlock(scr);
}

static void vcons_erasecols_buffer(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int start = startcol + row * ri->ri_cols;
	int end = start + ncols, i;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	for (i = start; i < end; i++) {
		scr->scr_attrs[offset + i] = fillattr;
		scr->scr_chars[offset + i] = 0x20;
	}
#else
	for (i = start; i < end; i++) {
		scr->scr_attrs[i] = fillattr;
		scr->scr_chars[i] = 0x20;
	}
#endif

}

static void vcons_erasecols(void *cookie, int row, int startcol, int ncols, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_erasecols_buffer(cookie, row, startcol, ncols, fillattr);

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->scr_vd->erasecols(cookie, row, startcol, ncols, fillattr);
	}
	vcons_unlock(scr);
}

static void vcons_copyrows_buffer(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int from, to, len;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	/* do we need to scroll the back buffer? */
	if (dstrow == 0) {
		from = ri->ri_cols * srcrow;
		to = ri->ri_cols * dstrow;

		memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
		    scr->scr_offset_to_zero * sizeof(long));
		memmove(&scr->scr_chars[to], &scr->scr_chars[from],
		    scr->scr_offset_to_zero * sizeof(uint32_t));
	}
	from = ri->ri_cols * srcrow + offset;
	to = ri->ri_cols * dstrow + offset;
	len = ri->ri_cols * nrows;		
		
#else
	from = ri->ri_cols * srcrow;
	to = ri->ri_cols * dstrow;
	len = ri->ri_cols * nrows;
#endif
	memmove(&scr->scr_attrs[to], &scr->scr_attrs[from],
	    len * sizeof(long));
	memmove(&scr->scr_chars[to], &scr->scr_chars[from],
	    len * sizeof(uint32_t));

}

static void vcons_copyrows(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_copyrows_buffer(cookie, srcrow, dstrow, nrows);

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->scr_vd->copyrows(cookie, srcrow, dstrow, nrows);
	}
	vcons_unlock(scr);
}

static void vcons_copyrows_noread(void *cookie, int srcrow, int dstrow, int nrows)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		int pos, l, c, offset, ppos;

#ifdef WSDISPLAY_SCROLLSUPPORT
		offset = scr->scr_current_offset;
#else
		offset = 0;
#endif
		ppos = ri->ri_cols * dstrow;
		pos = ppos + offset;
		for (l = dstrow; l < (dstrow + nrows); l++) {
			for (c = 0; c < ri->ri_cols; c++) {
				scr->putchar(cookie, l, c, scr->scr_chars[pos],
				    scr->scr_attrs[pos]);
				pos++;
				ppos++;
			}
		}
	}
	vcons_unlock(scr);
}

static void vcons_eraserows_buffer(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int start, end, i;

#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	start = ri->ri_cols * row + offset;
	end = ri->ri_cols * (row + nrows) + offset;
#else
	start = ri->ri_cols * row;
	end = ri->ri_cols * (row + nrows);
#endif

	for (i = start; i < end; i++) {
		scr->scr_attrs[i] = fillattr;
		scr->scr_chars[i] = 0x20;
	}
}


static void vcons_eraserows(void *cookie, int row, int nrows, long fillattr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_eraserows_buffer(cookie, row, nrows, fillattr);

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->scr_vd->eraserows(cookie, row, nrows, fillattr);
	}
	vcons_unlock(scr);
}

static void vcons_putchar_buffer(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	int pos;
	
#ifdef WSDISPLAY_SCROLLSUPPORT
	int offset;
	offset = scr->scr_offset_to_zero;

	if ((row >= 0) && (row < ri->ri_rows) && (col >= 0) && 
	     (col < ri->ri_cols)) {
		pos = col + row * ri->ri_cols;
		scr->scr_attrs[pos + offset] = attr;
		scr->scr_chars[pos + offset] = c;
	}
#else
	if ((row >= 0) && (row < ri->ri_rows) && (col >= 0) && 
	     (col < ri->ri_cols)) {
		pos = col + row * ri->ri_cols;
		scr->scr_attrs[pos] = attr;
		scr->scr_chars[pos] = c;
	}
#endif
}

static void vcons_putchar(void *cookie, int row, int col, u_int c, long attr)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;
	
	vcons_putchar_buffer(cookie, row, col, c, attr);

	vcons_lock(scr);
	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->putchar(cookie, row, col, c, attr);
	}
	vcons_unlock(scr);
}

static void vcons_cursor(void *cookie, int on, int row, int col)
{
	struct rasops_info *ri = cookie;
	struct vcons_screen *scr = ri->ri_hw;

	vcons_lock(scr);

	if (SCREEN_IS_VISIBLE(scr) && SCREEN_CAN_DRAW(scr)) {
		scr->scr_vd->cursor(cookie, on, row, col);
	} else {
		scr->scr_ri.ri_crow = row;
		scr->scr_ri.ri_ccol = col;
	}
	vcons_unlock(scr);
}

/* methods to read/write characters via ioctl() */

static int vcons_putwschar(struct vcons_screen *scr, struct wsdisplay_char *wsc)
{
	long attr;
	struct rasops_info *ri;
	int error;

	assert(scr != NULL && wsc != NULL);

	ri = &scr->scr_ri;

	if (__predict_false((unsigned int)wsc->col > ri->ri_cols ||
	    (unsigned int)wsc->row > ri->ri_rows))
			return (EINVAL);
	
	if ((wsc->row >= 0) && (wsc->row < ri->ri_rows) && (wsc->col >= 0) && 
	     (wsc->col < ri->ri_cols)) {

		error = ri->ri_ops.allocattr(ri, wsc->foreground,
		    wsc->background, wsc->flags, &attr);
		if (error)
			return error;
		vcons_putchar(ri, wsc->row, wsc->col, wsc->letter, attr);
#ifdef VCONS_DEBUG
		printf("vcons_putwschar(%d, %d, %x, %lx\n", wsc->row, wsc->col,
		    wsc->letter, attr);
#endif
		return 0;
	} else
		return EINVAL;
}

static int vcons_getwschar(struct vcons_screen *scr, struct wsdisplay_char *wsc)
{
	int offset;
	long attr;
	struct rasops_info *ri;

	assert(scr != NULL && wsc != NULL);

	ri = &scr->scr_ri;

	if ((wsc->row >= 0) && (wsc->row < ri->ri_rows) && (wsc->col >= 0) && 
	     (wsc->col < ri->ri_cols)) {

		offset = ri->ri_cols * wsc->row + wsc->col;
#ifdef WSDISPLAY_SCROLLSUPPORT
		offset += scr->scr_offset_to_zero;
#endif
		wsc->letter = scr->scr_chars[offset];
		attr = scr->scr_attrs[offset];

		/* 
		 * this is ugly. We need to break up an attribute into colours and
		 * flags but there's no rasops method to do that so we must rely on
		 * the 'canonical' encoding.
		 */
#ifdef VCONS_DEBUG
		printf("vcons_getwschar: %d, %d, %x, %lx\n", wsc->row,
		    wsc->col, wsc->letter, attr);
#endif
		wsc->foreground = (attr >> 24) & 0xff;
		wsc->background = (attr >> 16) & 0xff;
		wsc->flags      = attr & 0xff;
		return 0;
	} else
		return EINVAL;
}

#ifdef WSDISPLAY_SCROLLSUPPORT

static void vcons_scroll(void *cookie, void *vs, int where)
{
	struct vcons_screen *scr = vs;

	if (where == 0) {
		scr->scr_line_wanted = 0;
	} else {
		scr->scr_line_wanted = scr->scr_line_wanted - where;
		if (scr->scr_line_wanted < 0)
			scr->scr_line_wanted = 0;
		if (scr->scr_line_wanted > scr->scr_lines_in_buffer)
			scr->scr_line_wanted = scr->scr_lines_in_buffer;
	}

	if (scr->scr_line_wanted != scr->scr_current_line) {

		vcons_do_scroll(scr);
	}
}

static void vcons_do_scroll(struct vcons_screen *scr)
{
	int dist, from, to, num;
	int r_offset, r_start;
	int i, j;

	if (scr->scr_line_wanted == scr->scr_current_line)
		return;
	dist = scr->scr_line_wanted - scr->scr_current_line;
	scr->scr_current_line = scr->scr_line_wanted;
	scr->scr_current_offset = scr->scr_ri.ri_cols *
	    (scr->scr_lines_in_buffer - scr->scr_current_line);
	if (abs(dist) >= scr->scr_ri.ri_rows) {
		vcons_redraw_screen(scr);
		return;
	}
	/* scroll and redraw only what we really have to */
	if (dist > 0) {
		/* we scroll down */
		from = 0;
		to = dist;
		num = scr->scr_ri.ri_rows - dist;
		/* now the redraw parameters */
		r_offset = scr->scr_current_offset;
		r_start = 0;
	} else {
		/* scrolling up */
		to = 0;
		from = -dist;
		num = scr->scr_ri.ri_rows + dist;
		r_offset = scr->scr_current_offset + num * scr->scr_ri.ri_cols;
		r_start = num;
	}
	scr->scr_vd->copyrows(scr, from, to, num);
	for (i = 0; i < abs(dist); i++) {
		for (j = 0; j < scr->scr_ri.ri_cols; j++) {
#ifdef VCONS_DRAW_INTR
			vcons_putchar_cached(scr, i + r_start, j,
			    scr->scr_chars[r_offset],
			    scr->scr_attrs[r_offset]);
#else
			scr->putchar(scr, i + r_start, j,
			    scr->scr_chars[r_offset],
			    scr->scr_attrs[r_offset]);
#endif
			r_offset++;
		}
	}

	if (scr->scr_line_wanted == 0) {
		/* this was a reset - need to draw the cursor */
		scr->scr_ri.ri_flg &= ~RI_CURSOR;
		scr->scr_vd->cursor(scr, 1, scr->scr_ri.ri_crow,
		    scr->scr_ri.ri_ccol);
	}
}

#endif /* WSDISPLAY_SCROLLSUPPORT */

void vcons_enable_polling(struct vcons_data *vd)
{
	struct vcons_screen *scr = vd->active;

	if (scr && !SCREEN_IS_BUSY(scr)) {
		if ((scr->scr_flags & VCONS_NO_REDRAW) == 0)
			vcons_redraw_screen(scr);
	}
}

void vcons_disable_polling(struct vcons_data *vd)
{
}

void vcons_hard_switch(struct vcons_screen *scr)
{
	struct vcons_data *vd = scr->scr_vd;
	struct vcons_screen *oldscr = vd->active;

	if (oldscr) {
		SCREEN_INVISIBLE(oldscr);
		oldscr->scr_ri.ri_flg &= ~RI_CURSOR;
	}
	SCREEN_VISIBLE(scr);
	vd->active = scr;
	vd->wanted = NULL;

	if (vd->show_screen_cb != NULL)
		vd->show_screen_cb(scr, vd->show_screen_cookie);
}


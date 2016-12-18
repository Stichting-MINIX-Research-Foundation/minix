/*	$NetBSD: wsmuxvar.h,v 1.15 2013/11/23 20:56:41 christos Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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
 * A ws event source, i.e., wskbd, wsmouse, or wsmux.
 */
struct wsevsrc {
	device_t me_dv;
	const struct wssrcops *me_ops;	/* method pointers */
	struct wseventvar me_evar;	/* wseventvar opened directly */
	struct wseventvar *me_evp;	/* our wseventvar when open */
#if NWSDISPLAY > 0
	device_t me_dispdv;       /* our display if part of one */
#endif
#if NWSMUX > 0
	struct wsmux_softc *me_parent;	/* parent mux device */
	TAILQ_ENTRY(wsevsrc) me_next;	/* sibling pointers */
#endif
};

/*
 * Methods that can be performed on an events source.  Usually called
 * from a wsmux.
 */
struct wssrcops {
	int type;		/* device type: WSMUX_{MOUSE,KBD,MUX} */
	int (*dopen)(struct wsevsrc *, struct wseventvar *);
	int (*dclose)(struct wsevsrc *);
	int (*dioctl)(device_t, u_long, void *, int, struct lwp *);
	int (*ddispioctl)(device_t, u_long, void *, int, struct lwp *);
	int (*dsetdisplay)(device_t, struct wsevsrc *);
};

#define wsevsrc_open(me, evp) \
	((me)->me_ops->dopen((me), evp))
#define wsevsrc_close(me) \
	((me)->me_ops->dclose((me)))
#define wsevsrc_ioctl(me, cmd, data, flag, l) \
	((me)->me_ops->dioctl((me)->me_dv, cmd, (void *)data, flag, l))
#define wsevsrc_display_ioctl(me, cmd, data, flag, l) \
	((me)->me_ops->ddispioctl((me)->me_dv, cmd, (void *)data, flag, l))
#define wsevsrc_set_display(me, arg) \
	((me)->me_ops->dsetdisplay((me)->me_dv, arg))

#if NWSMUX > 0
struct wsmux_softc {
	struct wsevsrc sc_base;
	struct proc *sc_p;		/* open proc */
	TAILQ_HEAD(, wsevsrc) sc_cld;	/* list of children */
	u_int32_t sc_kbd_layout;	/* current layout of keyboard */
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int sc_rawkbd;		        /* A hack to remember the kbd mode */
#endif
};

struct	wsmux_softc *wsmux_getmux(int);
struct	wsmux_softc *wsmux_create(const char *, int);
int	wsmux_attach_sc(struct wsmux_softc *, struct wsevsrc *);
void	wsmux_detach_sc(struct wsevsrc *);
int	wsmux_set_display(struct wsmux_softc *, device_t);

int	wskbd_add_mux(int, struct wsmux_softc *);
int	wsmouse_add_mux(int, struct wsmux_softc *);

#endif /* NWSMUX > 0 */

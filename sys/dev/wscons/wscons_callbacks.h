/* $NetBSD: wscons_callbacks.h,v 1.19 2007/10/19 08:57:50 dogcow Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/device.h>

struct wsevsrc;

/*
 * Calls to the display interface from the glue code.
 */
void wsdisplay_set_console_kbd(struct wsevsrc *);

/*
 * Calls to the display interface from the keyboard interface.
 */
void	wsdisplay_kbdinput(device_t , keysym_t);
int	wsdisplay_switch(device_t, int, int);
enum wsdisplay_resetops {
	WSDISPLAY_RESETEMUL,
	WSDISPLAY_RESETCLOSE
};
void	wsdisplay_reset(device_t, enum wsdisplay_resetops);
void	wsdisplay_kbdholdscreen(device_t, int);

void	wsdisplay_set_cons_kbd(int (*get)(dev_t),
			       void (*poll)(dev_t, int),
			       void (*bell)(dev_t, u_int, u_int, u_int));
void	wsdisplay_unset_cons_kbd(void);
struct wsdisplay_param;
int	wsdisplay_param(device_t, u_long, struct wsdisplay_param *);

/*
 * Calls to the keyboard interface from the glue code.
 */
struct wsevsrc *wskbd_set_console_display(device_t, struct wsevsrc *);
int wskbd_pickfree(void);

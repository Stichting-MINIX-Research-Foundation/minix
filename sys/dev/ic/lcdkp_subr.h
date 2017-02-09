/* $NetBSD: lcdkp_subr.h,v 1.3 2011/05/14 02:58:27 rmind Exp $ */

/*
 * Copyright (c) 2002 Dennis I. Chernoivanov
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
 * 3. The name of the author may not be used to endorse or promote products
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

#ifndef _DEV_IC_LCDKP_SUBR_H_
#define _DEV_IC_LCDKP_SUBR_H_

#ifdef _KERNEL

/* Key code translation */
struct lcdkp_xlate {
	u_int8_t x_keycode;
	u_int8_t x_outcode;
};

/* Keypad control structure */
struct lcdkp_chip {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;	/* keypad i/o register */

#define LCDKP_HAS_BUF	0x1
	u_char sc_flags;
	u_int8_t sc_buf;		/* pending character */

	u_int8_t sc_knum;		/* number of buttons */
	struct lcdkp_xlate *sc_kpad;	/* recognized keycodes */

	u_int8_t (* sc_rread)(bus_space_tag_t, bus_space_handle_t);

	kmutex_t sc_lock;
};

#define lcdkp_dr_read(sc) \
	(sc)->sc_rread((sc)->sc_iot, (sc)->sc_ioh);

void lcdkp_attach_subr(struct lcdkp_chip *);
int  lcdkp_scankey(struct lcdkp_chip *);
int  lcdkp_readkey(struct lcdkp_chip *, u_int8_t *);

#endif /* _KERNEL */

#endif /* _DEV_IC_LCDKP_SUBR_H_ */

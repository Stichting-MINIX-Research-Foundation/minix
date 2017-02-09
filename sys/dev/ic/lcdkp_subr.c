/* $NetBSD: lcdkp_subr.c,v 1.7 2011/05/14 02:58:27 rmind Exp $ */

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

/*
 * Subroutines for simple one-port keypad.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lcdkp_subr.c,v 1.7 2011/05/14 02:58:27 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <machine/autoconf.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ic/lcdkp_subr.h>

#define HD_POLL_RATE	(hz / 10)

static int    lcdkp_poll(struct lcdkp_chip *);
static u_char lcdkp_scan(struct lcdkp_chip *, u_int8_t *);

/*
 * Initialization.
 */
void
lcdkp_attach_subr(struct lcdkp_chip *sc)
{

	sc->sc_flags = 0x0;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * Scan whether input is pending, don't block if not.
 */
int
lcdkp_scankey(struct lcdkp_chip *sc)
{
	int ret;

	if ((sc->sc_knum == 0) || (sc->sc_kpad == NULL))
		return 0;

	mutex_enter(&sc->sc_lock);
	if (!(sc->sc_flags & LCDKP_HAS_BUF)) {
		u_int8_t b;
		if (lcdkp_scan(sc, &b) != 0) {
			sc->sc_buf = b;
			sc->sc_flags |= LCDKP_HAS_BUF;
		}
	}
	ret = (sc->sc_flags & LCDKP_HAS_BUF);
	mutex_exit(&sc->sc_lock);

	return ret;
}

/*
 * Read new key code, block if needed.
 */
int
lcdkp_readkey(struct lcdkp_chip *sc, u_int8_t *result)
{
	int error;

	if ((sc->sc_knum == 0) || (sc->sc_kpad == NULL))
		return EIO;

	mutex_enter(&sc->sc_lock);
	if ( (error = lcdkp_poll(sc)) == 0) {
		*result = sc->sc_buf;
		sc->sc_flags &= ~LCDKP_HAS_BUF;
	}
	mutex_exit(&sc->sc_lock);

	return 0;
}

/*
 * Scan the keypad and translate the input.
 */
static u_char
lcdkp_scan(struct lcdkp_chip *sc, u_int8_t *b)
{
	u_int8_t i;
	u_int8_t c;

	c = lcdkp_dr_read(sc);
	for (i = 0; i < sc->sc_knum; i++) {
		if (sc->sc_kpad[i].x_keycode == c) {
			*b = sc->sc_kpad[i].x_outcode;
			return 1;
		}
	}
	return 0;
}

/*
 * Block until input is available.
 */
static int
lcdkp_poll(struct lcdkp_chip *sc)
{
	int error;
	uint8_t b;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (sc->sc_flags & LCDKP_HAS_BUF) {
		return 0;
	}
	while (lcdkp_scan(sc, &b) == 0) {
		error = mtsleep((void*)sc, PRIBIO | PCATCH, "kppoll",
		    HD_POLL_RATE, &sc->sc_lock);
		if (error != EWOULDBLOCK) {
			if (lcdkp_scan(sc, &b) != 0)
				break;
			return EINTR;
		}
	}
	sc->sc_buf = b;
	sc->sc_flags |= LCDKP_HAS_BUF;
	return 0;
}

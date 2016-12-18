/* $NetBSD: lk201_ws.c,v 1.9 2015/01/02 21:32:26 jklos Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lk201_ws.c,v 1.9 2015/01/02 21:32:26 jklos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>

#include <dev/wscons/wsconsio.h>

#include <dev/dec/lk201reg.h>
#include <dev/dec/lk201var.h>
#include <dev/dec/wskbdmap_lk201.h> /* for {MIN,MAX}_LK201_KEY */

#define send(lks, c) ((*((lks)->attmt.sendchar))((lks)->attmt.cookie, c))

void lk201_identify(void *);

static callout_t lkkbd_id;

static const char *lkkbd_descr[] = {
	"no keyboard",
	"LK-201 keyboard",
	"LK-401 keyboard",
};

int
lk201_init(struct lk201_state *lks)
{
	int i;

	lks->waitack = 0;

	send(lks, LK_LED_ENABLE);
	send(lks, LK_LED_ALL);

	/*
	 * set all keys to updown mode; autorepeat is
	 * done by wskbd software
	 */
	for (i = 1; i <= 14; i++)
		send(lks, LK_CMD_MODE(LK_UPDOWN, i));

	send(lks, LK_CL_ENABLE);
	send(lks, LK_PARAM_VOLUME(3));
	lks->kcvol = (8 - 3) * 100 / 8;

	lks->bellvol = -1; /* not yet set */

	for (i = 0; i < LK_KLL; i++)
		lks->down_keys_list[i] = -1;
	send(lks, LK_KBD_ENABLE);

	send(lks, LK_LED_DISABLE);
	send(lks, LK_LED_ALL);
	lks->leds_state = 0;

	callout_init(&lkkbd_id, 0);
	callout_setfunc(&lkkbd_id, lk201_identify, lks);
	callout_schedule(&lkkbd_id, 0);

	return (0);
}

void
lk201_identify(void *v)
{
	struct lk201_state *lks = v;
	int i;

	callout_destroy(&lkkbd_id);
	/*
	 * Swallow all the keyboard acknowledges from lk201_init().
	 * There should be 14 of them - one per LK_CMD_MODE command.
	 */
	for(;;) {
		lks->waitack = 1;
		for (i = 100; i != 0; i--) {
			DELAY(1000);
			if (lks->waitack == 0)
				break;
		}
		if (i == 0)
			break;
	}

	/*
	 * Try to set the keyboard in LK-401 mode.
	 * If we receive an error, this is an LK-201 keyboard.
	 */
	lks->waitack = 1;
	send(lks, LK_ENABLE_401);
	for (i = 100; i != 0; i--) {
		DELAY(1000);
		if (lks->waitack == 0)
			break;
	}
	if (lks->waitack != 0)
		lks->kbdtype = KBD_NONE;
	else {
		if (lks->ackdata == LK_INPUT_ERROR)
			lks->kbdtype = KBD_LK201;
		else
			lks->kbdtype = KBD_LK401;
	}
	lks->waitack = 0;

	printf("lkkbd0: %s\n", lkkbd_descr[lks->kbdtype]);
}

int
lk201_decode(struct lk201_state *lks, int wantmulti, int datain, u_int *type, int *dataout)
{
	int i, freeslot;

	if (lks->waitack != 0) {
		lks->ackdata = datain;
		lks->waitack = 0;
		return LKD_NODATA;
	}

	switch (datain) {
#if 0
	    case LK_KEY_UP:
		for (i = 0; i < LK_KLL; i++)
			lks->down_keys_list[i] = -1;
		*type = WSCONS_EVENT_ALL_KEYS_UP;
		return (1);
#endif
	    case LK_POWER_UP:
		printf("lk201_decode: powerup detected\n");
		lk201_init(lks);
		return (0);
	    case LK_KDOWN_ERROR:
	    case LK_POWER_ERROR:
	    case LK_OUTPUT_ERROR:
	    case LK_INPUT_ERROR:
		printf("lk201_decode: error %x\n", datain);
		/* FALLTHRU */
	    case LK_KEY_REPEAT: /* autorepeat handled by wskbd */
	    case LK_MODE_CHANGE: /* ignore silently */
		return (0);
	}


	if (datain == LK_KEY_UP) {
		if (wantmulti) {
			for (i = 0; i < LK_KLL; i++)
				if (lks->down_keys_list[i] != -1) {
					*type = WSCONS_EVENT_KEY_UP;
					*dataout = lks->down_keys_list[i] -
					    MIN_LK201_KEY;
					lks->down_keys_list[i] = -1;
					return (LKD_MORE);
				}
			return (LKD_NODATA);
		} else {
			for (i = 0; i < LK_KLL; i++)
				lks->down_keys_list[i] = -1;
			*type = WSCONS_EVENT_ALL_KEYS_UP;
			return (LKD_COMPLETE);
		}
	} else if (datain < MIN_LK201_KEY || datain > MAX_LK201_KEY) {
		printf("lk201_decode: %x\n", datain);
		return (0);
	}

	*dataout = datain - MIN_LK201_KEY;

	freeslot = -1;
	for (i = 0; i < LK_KLL; i++) {
		if (lks->down_keys_list[i] == datain) {
			*type = WSCONS_EVENT_KEY_UP;
			lks->down_keys_list[i] = -1;
			return (1);
		}
		if (lks->down_keys_list[i] == -1 && freeslot == -1)
			freeslot = i;
	}

	if (freeslot == -1) {
		printf("lk201_decode: down(%d) no free slot\n", datain);
		return (0);
	}

	*type = WSCONS_EVENT_KEY_DOWN;
	lks->down_keys_list[freeslot] = datain;
	return (1);
}

void
lk201_bell(struct lk201_state *lks, struct wskbd_bell_data *bell)
{
	unsigned int vol;

	if (bell->which & WSKBD_BELL_DOVOLUME) {
		vol = 8 - bell->volume * 8 / 100;
		if (vol > 7)
			vol = 7;
	} else
		vol = 3;

	if (vol != lks->bellvol) {
		send(lks, LK_BELL_ENABLE);
		send(lks, LK_PARAM_VOLUME(vol));
		lks->bellvol = vol;
	}
	send(lks, LK_RING_BELL);
}

void
lk201_set_leds(struct lk201_state *lks, int leds)
{
	int newleds;

	newleds = 0;
	if (leds & WSKBD_LED_SCROLL)
		newleds |= LK_LED_WAIT;
	if (leds & WSKBD_LED_CAPS)
		newleds |= LK_LED_LOCK;

	send(lks, LK_LED_DISABLE);
	send(lks, (0x80 | (~newleds & 0x0f)));

	send(lks, LK_LED_ENABLE);
	send(lks, (0x80 | (newleds & 0x0f)));

	lks->leds_state = leds;
}

void
lk201_set_keyclick(struct lk201_state *lks, int vol)
{
	unsigned int newvol;

	if (vol == 0)
		send(lks, LK_CL_DISABLE);
	else {
		newvol = 8 - vol * 8 / 100;
		if (newvol > 7)
			newvol = 7;

		send(lks, LK_CL_ENABLE);
		send(lks, LK_PARAM_VOLUME(newvol));
	}

	lks->kcvol = vol;
}

/*
 * Copyright (c) 2005 Jesse Off.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DEV_IC_MATRIXKPVAR_H_
#define _DEV_IC_MATRIXKPVAR_H_

#define MAXNKEYS	64
#define FOR_KEYS(x, y)	\
	for((x) = 0; (x) < ((MAXNKEYS + 31) / 32); (x)++) y

struct matrixkp_softc {
	int mxkp_nkeys;
	u_int32_t mxkp_pressed[(MAXNKEYS + 31) / 32];
	int poll_freq;
	u_int32_t debounce_stable_ms;
	void (*mxkp_scankeys)(struct matrixkp_softc *, u_int32_t *);
	void (*mxkp_event)(struct matrixkp_softc *, u_int32_t *, u_int32_t *);
#define	MXKP_NODEBOUNCE	0x1
	u_int32_t sc_flags;
	u_int32_t sc_enabled;
	device_t sc_dev;
	device_t sc_wskbddev;
	struct callout sc_callout;
};

void mxkp_attach(struct matrixkp_softc *);
void mxkp_poll(void *);
void mxkp_debounce(struct matrixkp_softc *, u_int32_t *, u_int32_t *);
void mxkp_wskbd_event(struct matrixkp_softc *, u_int32_t *, u_int32_t *);
int  mxkp_enable(void *, int);
void mxkp_set_leds(void *, int);
int  mxkp_ioctl(void *, u_long, void *, int, struct lwp *);

extern const struct wskbd_accessops mxkp_accessops;

#endif

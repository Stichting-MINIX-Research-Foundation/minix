/*	$NetBSD: efigetsecs.c,v 1.5 2021/10/06 10:13:19 jmcneill Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "efiboot.h"

#include <lib/libsa/net.h>

static EFI_EVENT getsecs_ev = 0;
static satime_t getsecs_val = 0;

static satime_t
getsecs_rtc(void)
{
	static const int daytab[][14] = {
	    { 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	    { 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	};
	EFI_TIME t;
	satime_t r;
	int y;
#define isleap(_y) (((_y) % 4) == 0 && (((_y) % 100) != 0 || ((_y) % 400) == 0))

	uefi_call_wrapper(RT->GetTime, 2, &t, NULL);

	/* Calc days from UNIX epoch */
	r = (t.Year - 1970) * 365;
	for (y = 1970; y < t.Year; y++) {
		if (isleap(y))
			r++;
	}
	r += daytab[isleap(t.Year) ? 1 : 0][t.Month] + t.Day;

	/* Calc secs */
	r *= 60 * 60 * 24;
	r += ((t.Hour * 60) + t.Minute) * 60 + t.Second;
	if (-24 * 60 < t.TimeZone && t.TimeZone < 24 * 60)
		r += t.TimeZone * 60;

	return r;
}

static EFIAPI void
getsecs_notify_func(EFI_EVENT ev, VOID *context)
{
	getsecs_val++;
}

satime_t
getsecs(void)
{
	EFI_STATUS status;

	if (getsecs_ev == 0) {
		status = uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
		    getsecs_notify_func, 0, &getsecs_ev);
		if (EFI_ERROR(status))
			panic("%s: couldn't create event timer: 0x%lx", __func__, (u_long)status);
		status = uefi_call_wrapper(BS->SetTimer, 3, getsecs_ev, TimerPeriodic, 10000000);	/* 1s in "100ns" units */
		if (EFI_ERROR(status))
			panic("%s: couldn't start event timer: 0x%lx", __func__, (u_long)status);
		getsecs_val = getsecs_rtc();
	}

	return getsecs_val;
}

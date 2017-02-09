/*	$NetBSD: ofrtc.c,v 1.23 2011/07/26 08:59:38 mrg Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ofrtc.c,v 1.23 2011/07/26 08:59:38 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/event.h>

#include <dev/clock_subr.h>
#include <dev/ofw/openfirm.h>

#define OFRTC_SEC 0
#define OFRTC_MIN 1
#define OFRTC_HR  2
#define OFRTC_DOM 3
#define OFRTC_MON 4
#define OFRTC_YR  5

struct ofrtc_softc {
	int sc_phandle;
	int sc_ihandle;
	struct todr_chip_handle sc_todr;
};

static int ofrtc_match(device_t, cfdata_t, void *);
static void ofrtc_attach(device_t, device_t, void *);
static int ofrtc_gettod(todr_chip_handle_t, struct clock_ymdhms *);
static int ofrtc_settod(todr_chip_handle_t, struct clock_ymdhms *);

CFATTACH_DECL_NEW(ofrtc, sizeof(struct ofrtc_softc),
    ofrtc_match, ofrtc_attach, NULL, NULL);

static int
ofrtc_match(device_t parent, cfdata_t match, void *aux)
{
	struct ofbus_attach_args *oba = aux;
	char type[8];
	int l;

	if (strcmp(oba->oba_busname, "ofw"))
		return (0);
	if ((l = OF_getprop(oba->oba_phandle, "device_type", type,
	    sizeof type - 1)) < 0 ||
	    l >= sizeof type)
		return 0;

	/* ensure null termination */
	type[l] = 0;
	return !strcmp(type, "rtc");
}

static void
ofrtc_attach(device_t parent, device_t self, void *aux)
{
	struct ofrtc_softc *of = device_private(self);
	struct ofbus_attach_args *oba = aux;
	char name[32];
	char path[256];
	int l;

	of->sc_phandle = oba->oba_phandle;
	of->sc_ihandle = 0;
	if ((l = OF_getprop(of->sc_phandle, "name", name,
	    sizeof name - 1)) < 0)
		panic("Device without name?");
	if (l >= sizeof name)
		l = sizeof name - 1;
	name[l] = 0;

	if (!of->sc_ihandle) {
		if ((l = OF_package_to_path(of->sc_phandle, path,
			 sizeof path - 1)) < 0 ||
		    l >= sizeof path) {
			aprint_error(": cannot determine package path\n");
 			return;
		}
		path[l] = 0;

		if (!(of->sc_ihandle = OF_open(path))) {
			aprint_error(": cannot open path\n");
 			return;
 		}
 	}

	of->sc_todr.todr_gettime_ymdhms = ofrtc_gettod;
	of->sc_todr.todr_settime_ymdhms = ofrtc_settod;
	of->sc_todr.cookie = of;
	todr_attach(&of->sc_todr);
	printf(": %s\n", name);
}

int
ofrtc_gettod(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct ofrtc_softc *of = tch->cookie;
	int date[6];

	/*
	 * We mostly ignore the suggested time and go for the RTC clock time
	 * stored in the CMOS RAM.  If the time can't be obtained from the
	 * CMOS, or if the time obtained from the CMOS is 5 or more years
	 * less than the suggested time, we used the suggested time.  (In
	 * the latter case, it's likely that the CMOS battery has died.)
	 */

	if (OF_call_method("get-time", of->sc_ihandle, 0, 6,
	    date, date + 1, date + 2, date + 3, date + 4, date + 5)) {
		return EIO;
	}

	dt->dt_sec = date[OFRTC_SEC];
	dt->dt_min = date[OFRTC_MIN];
	dt->dt_hour = date[OFRTC_HR];
	dt->dt_day = date[OFRTC_DOM];
	dt->dt_mon = date[OFRTC_MON];
	dt->dt_year = date[OFRTC_YR];

	return 0;
}

int
ofrtc_settod(todr_chip_handle_t tch, struct clock_ymdhms *dt)
{
	struct ofrtc_softc *of = tch->cookie;
	int sec, minute, hr, dom, mon, yr;

	sec = dt->dt_sec;
	minute = dt->dt_min;
	hr = dt->dt_hour;
	dom = dt->dt_day;
	mon = dt->dt_mon;
	yr = dt->dt_year;

	if (OF_call_method("set-time", of->sc_ihandle, 6, 0,
	     sec, minute, hr, dom, mon, yr))
		return EIO;

	return 0;
}

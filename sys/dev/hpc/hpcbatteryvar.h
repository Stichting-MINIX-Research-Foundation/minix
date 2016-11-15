/*	$NetBSD: hpcbatteryvar.h,v 1.4 2012/10/27 17:18:17 chs Exp $	*/

/*
 * Copyright (c) 2000-2001 SATO Kazumi
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
 *
 */

#ifndef _HPCBATTERY_VAR_H
#define _HPCBATTERY_VAR_H

#define HPCPM_MAX_BATTERY_VALUES	3
struct hpcbattery_values {
	int nextpoll;
	int n_values;
	int value[HPCPM_MAX_BATTERY_VALUES];
};

#ifdef notdef
typedef	enum {
	hpcbattery_init0,
	hpcbattery_init1,
	hpcbattery_unknown,
	hpcbattery_dc,
	hpcbattery_ac_charging,
	hpcbattery_ac_nocharging
} hpcbattery_ac_state;

typedef	enum {
	hpcbattery_batt_no_system_battery,
	hpcbattery_batt_unknown,
	hpcbattery_batt_100p,
	hpcbattery_batt_90p,
	hpcbattery_batt_80p,
	hpcbattery_batt_70p,
	hpcbattery_batt_60p,
	hpcbattery_batt_50p,
	hpcbattery_batt_40p,
	hpcbattery_batt_30p,
	hpcbattery_batt_20p,
	hpcbattery_batt_10p,
	hpcbattery_batt_critical,
} hpcbattery_batt_state;

struct hpcbattery_softc {
	/* current status */
	hpcbattery_ac_state sc_ac_status;
	hpcbattery_batt_state sc_batt_status;
	int sc_rate;
	int sc_minuits;
	int sc_v;
	int sc_v_h;
	int sc_v_l;
	/* last status change values */
	hpcbattery_ac_state sc_ac_status0;
	hpcbattery_batt_state sc_batt_status0;
	int sc_rate0;
	int sc_minuits0;
	int sc_v0;
	int sc_v0_h;
	int sc_v0_l;
};
#endif /* notdef */

struct hpcbattery_spec {
	int guess_charge;
	int guess_ac_dc;
	int main_port;		/* index of main battery port, -1 then ignore */
	int drift;		/* value drifts */
	int ac_bias;		/* DC->AC, AC->DC BIAS */
	int dc_100p;		/* DC: full value (100%) */
	int dc_80p;		/* DC: almost full value (80%) */
	int dc_50p;		/* DC: half value (50%) */
	int dc_20p;		/* DC: battery low value (20%) */
	int dc_critical;	/* DC: battery critical value (0%) */
	int ac_charge_100p;	/* AC: charge finished value */
	int ac_100p;		/* AC: full value (100%) */
	int ac_80p;		/* AC: almost full value (80%) */
	int ac_50p;		/* AC: half value (50%) */
	int ac_20p;		/* AC: battery low value (20%) */
	int ac_critical;	/* AC: battery critical value */
	int main_flag;		/* main battery value positive, negative flag. 1 or -1 */
	int backup_port;	/* index of backup battery port, -1 then ignore */
	int b_full;		/* backup battery full */
	int b_low;		/* backup battery low */
	int b_critical;		/* backup battery hight */
	int b_flag;		/* main battery value positive, negative flag. 1 or -1 */
	int nocharge_port;	/* nocharge/(charge or dc) z50, -1 then ignore*/
	int n_low;		/* if n_flag*value < n_flag*n_low, now nocharging... */
	int n_flag;		/* -1 or 1 */
	int dc_ac_port;		/* index of ac/dc port ,-1 then ignore */
	int da_low;		/* if da_flag*value < da_flag*da_low then dc else ac */
	int da_flag;		/* -1 or 1 */
	int charge_port;	/* charge/(no charge or dc)  ,-1 then ignore */
	int c_low;		/* if da_flag*value < da_flag*da_low then dc else ac */
	int c_flag;		/* -1 or 1 */
};
#endif /* _HPCBATTERY_VAR_H */
/* end */

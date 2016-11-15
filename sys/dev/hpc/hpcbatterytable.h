/*	$NetBSD: hpcbatterytable.h,v 1.6 2005/12/11 12:21:22 christos Exp $	*/

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
#ifdef hpcmips
/*
 * NEC MCR/430, MCR/530 parameters
 */
struct hpcbattery_spec hpcbattery_mcr530_spec =
{
	0,	/* guess_charge */
	0,	/* guess_ac_dc */
	0,	/* main_port */
	10,	/* drift */
	20,	/* ac_bias */
	840,	/* dc_100p (100) */
	810,	/* dc_80p (80) */
	780,	/* dc_50p (50) */
	720,	/* dc_20p (20) */
	630,	/* dc_critical (0) */
	875,	/* ac_charge_100p */
	860,	/* ac_100p */
	830,	/* ac_80p */
	800,	/* ac_50p */
	740,	/* ac_20p */
	640,	/* ac_critical */
	1,	/* main_flag */

	1,	/* backup_port */
	900,	/* b_full */
	720,	/* b_low */
	640,	/* b_critical */
	1,	/* b_flag */

	-1,	/* nocharge_port */
	-1,	/* n_low */
	0,	/* n_flag */

	-1,	/* dc_ac_port */
	-1,	/* da_low */
	0,	/* da_flag */

	-1,	/* c_ac_port */
	-1,	/* c_low */
	0	/* c_flag */
};

/*
 * DoCoMo sigmarion parameter
 */
struct hpcbattery_spec hpcbattery_sigmarion_spec =
{
	0,	/* guess_charge */
	0,	/* guess_ac_dc */
	0,	/* main_port */
	10,	/* drift */
	20,	/* ac_bias */
	840,	/* dc_100p */
	810,	/* dc_80p */
	780,	/* dc_50p */
	720,	/* dc_20p */
	630,	/* dc_critical */
	875,	/* ac_charge_100p */
	860,	/* ac_100p */
	830,	/* ac_80p */
	800,	/* ac_50p */
	740,	/* ac_20p */
	640,	/* ac_critical */
	1,	/* main_flag */

	1,	/* backup_port */
	900,	/* b_full */
	880,	/* b_low */
	860,	/* b_critical */
	1,	/* b_flag */

	-1,	/* nocharge_port */
	-1,	/* n_low */
	0,	/* n_flag */

	-1,	/* dc_ac_port */
	-1,	/* da_low */
	0,	/* da_flag */

	-1,	/* c_ac_port */
	-1,	/* c_low */
	0	/* c_flag */
};

/*
 * IBM WorkPad z50
 */
struct hpcbattery_spec hpcbattery_z50_spec =
{
	0,	/* guess_charge */
	0,	/* guess_ac_dc */
	0,	/* main_port */
	10,	/* drift */
	20,	/* ac_bias */
	945,	/* dc_100p */
	915,	/* dc_80p */
	880,	/* dc_50p */
	855,	/* dc_20p */
	820,	/* dc_critical */
	-1,	/* ac_charge_100p */
	-1,	/* ac_100p */
	-1,	/* ac_80p */
	-1,	/* ac_50p */
	969,	/* ac_20p */
	-1,	/* ac_critical */
	1,	/* main_flag */

	1,	/* backup_port */
	970,	/* b_full */
	900,	/* b_low */
	800,	/* b_critical */
	1,	/* b_flag */

	2,	/* nocharge_port */
	800,	/* n_low */
	1,	/* n_flag */

	-1,	/* dc_ac_port */
	-1,	/* da_low */
	0,	/* da_flag */

	-1,	/* c_ac_port */
	-1,	/* c_low */
	0	/* c_flag */
};

/*
 * NEC MC-R700/730 parameters
 */
struct hpcbattery_spec hpcbattery_mcr700_spec =
{
	0,	/* guess_charge */
	0,	/* guess_ac_dc */
	0,	/* main_port */
	10,	/* drift */
	20,	/* ac_bias */
	840,	/* dc_100p (100) */
	820,	/* dc_80p (80) */
	790,	/* dc_50p (50) */
	770,	/* dc_20p (20) */
	760,	/* dc_critical (0) */
	860,	/* ac_charge_100p */
	850,	/* ac_100p */
	840,	/* ac_80p */
	830,	/* ac_50p */
	810,	/* ac_20p */
	800,	/* ac_critical */
	1,	/* main_flag */

	1,	/* backup_port */
	900,	/* b_full */
	720,	/* b_low */
	640,	/* b_critical */
	1,	/* b_flag */

	-1,	/* nocharge_port */
	-1,	/* n_low */
	0,	/* n_flag */

	-1,	/* dc_ac_port */
	-1,	/* da_low */
	0,	/* da_flag */

	-1,	/* c_ac_port */
	-1,	/* c_low */
	0	/* c_flag */
};

#endif /* hpcmips */

/* parameter table */

struct platid_data hpcbattery_parameters[] = {
#ifdef hpcmips
	{ &platid_mask_MACH_NEC_MCR_510, &hpcbattery_mcr530_spec }, /* XXX */
	{ &platid_mask_MACH_NEC_MCR_520, &hpcbattery_mcr530_spec }, /* XXX */
	{ &platid_mask_MACH_NEC_MCR_520A, &hpcbattery_mcr530_spec }, /* XXX */
	{ &platid_mask_MACH_NEC_MCR_530, &hpcbattery_mcr530_spec },
	{ &platid_mask_MACH_NEC_MCR_530A, &hpcbattery_mcr530_spec },
	{ &platid_mask_MACH_NEC_MCR_SIGMARION, &hpcbattery_sigmarion_spec },
	{ &platid_mask_MACH_IBM_WORKPAD_Z50, &hpcbattery_z50_spec },
	{ &platid_mask_MACH_NEC_MCR_700, &hpcbattery_mcr700_spec },
	{ &platid_mask_MACH_NEC_MCR_700A, &hpcbattery_mcr700_spec }, /* XXX */
	{ &platid_mask_MACH_NEC_MCR_730, &hpcbattery_mcr700_spec }, /* XXX */
	{ &platid_mask_MACH_NEC_MCR_730A, &hpcbattery_mcr700_spec }, /* XXX */
#endif /* hpcmips */
	{ NULL, NULL }	/* terminator, don't delete */
};
/* end */

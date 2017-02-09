/* $NetBSD: universereg.h,v 1.1 2000/02/25 18:22:39 drochner Exp $ */

/*
 * Copyright (c) 1999
 * 	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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

#ifndef _DEV_IC_UNIVERSEREG_H_
#define _DEV_IC_UNIVERSEREG_H_

/*
 * Register layout of the Newbridge/Tundra Universe II PCI-VME
 * adapter chip (CA91C142). Looks the same from PCI and VME.
 */

struct universe_pcislvimg {
	u_int32_t lsi_ctl, lsi_bs, lsi_bd, lsi_to;
};

struct universe_vmeslvimg {
	u_int32_t vsi_ctl, vsi_bs, vsi_bd, vsi_to;
};

struct universereg {
	u_int32_t pci_id, pci_csr, pci_class, pci_misc0, pci_bs0, pci_bs1;
	u_int32_t _space1[9];
	u_int32_t pci_misc1;
	u_int32_t _space2[(0x100-0x40)/4];
	struct universe_pcislvimg pcislv0;
	u_int32_t _space3;
	struct universe_pcislvimg pcislv1;
	u_int32_t _space4;
	struct universe_pcislvimg pcislv2;
	u_int32_t _space5;
	struct universe_pcislvimg pcislv3;
	u_int32_t _space6[(0x170-0x14c)/4];
	u_int32_t scyc_ctl, scyc_addr, scyc_en, scyc_cmp, scyc_swp;
	u_int32_t lmisc;
	u_int32_t slsi;
	u_int32_t l_cmderr, laerr;
	u_int32_t _space7[(0x1a0-0x194)/4];
	struct universe_pcislvimg pcislv4;
	u_int32_t _space8;
	struct universe_pcislvimg pcislv5;
	u_int32_t _space9;
	struct universe_pcislvimg pcislv6;
	u_int32_t _space10;
	struct universe_pcislvimg pcislv7;
	u_int32_t _space11[(0x200-0x1ec)/4];
	u_int32_t dctl, dtbc, dla;
	u_int32_t _space12;
	u_int32_t dva;
	u_int32_t _space13;
	u_int32_t dcpp;
	u_int32_t _space14;
	u_int32_t dgcs, d_llue;
	u_int32_t _space15[(0x300-0x228)/4];
	u_int32_t lint_en, lint_stat, lint_map0, lint_map1;
	u_int32_t vint_en, vint_stat, vint_map0, vint_map1;
	u_int32_t statid, v_statid[7];
	u_int32_t lint_map2, vint_map2;
	u_int32_t mbox[4], sema[2];
	u_int32_t _space16[(0x400-0x360)/4];
	u_int32_t mast_ctl, misc_ctl, misc_stat, user_am;
	u_int32_t _space17[(0xf00-0x410)/4];
	struct universe_vmeslvimg vmeslv0;
	u_int32_t _space18;
	struct universe_vmeslvimg vmeslv1;
	u_int32_t _space19;
	struct universe_vmeslvimg vmeslv2;
	u_int32_t _space20;
	struct universe_vmeslvimg vmeslv3;
	u_int32_t _space21[(0xf64-0xf4c)/4];
	u_int32_t lm_ctl, lm_bs;
	u_int32_t _space22;
	u_int32_t vrai_ctl, vrai_bs;
	u_int32_t _space23[(0xf80-0xf78)/4];
	u_int32_t vcsr_ctl, vcsr_to;
	u_int32_t v_amerr, vaerr;
	struct universe_vmeslvimg vmeslv4;
	u_int32_t _space24;
	struct universe_vmeslvimg vmeslv5;
	u_int32_t _space25;
	struct universe_vmeslvimg vmeslv6;
	u_int32_t _space26;
	struct universe_vmeslvimg vmeslv7;
	u_int32_t _space27[(0xff0-0xfdc)/4];
	u_int32_t _space28;
	u_int32_t vcsr_clr, vcsr_set, vcsr_bs;
};

#endif /* _DEV_IC_UNIVERSEREG_H_ */

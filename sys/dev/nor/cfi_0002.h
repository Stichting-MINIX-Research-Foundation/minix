/*	$NetBSD: cfi_0002.h,v 1.2 2011/07/19 20:52:10 cliff Exp $	*/
/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Cliff Neighbors.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_NOR_CFI_0002_H_
#define	_DEV_NOR_CFI_0002_H_

/*
 * CFI Primary Vendor-specific Extended Query structure
 * AMD/Fujitsu Extended Command Set 0002
 */
struct cmdset_0002_query_data {
    uint8_t	pri[3];			/* { 'P', 'R', 'I' } */
    uint8_t	version_maj;		/* major version number (ASCII) */
    uint8_t	version_min;		/* minor version number (ASCII) */
    uint8_t	asupt;			/* Si rev., addr-sensitive unlock */
    uint8_t	erase_susp;		/* erase-suspend */
    uint8_t	sector_prot;		/* sector protect */
    uint8_t	tmp_sector_unprot;	/* temporary sector unprotect */
    uint8_t	sector_prot_scheme;	/* sector protect scheme */
    uint8_t	simul_op;		/* simultaneous operation */
    uint8_t	burst_mode_type;	/* burst mode type */
    uint8_t	page_mode_type;		/* page mode type */
    uint8_t	acc_min;		/* Acc supply min voltage */
    uint8_t	acc_max;		/* Acc supply max voltage */
    uint8_t	wp_prot;		/* WP# protection */
    uint8_t	prog_susp;		/* prpogram suspend */
    uint8_t	unlock_bypass;		/* unlock bypass */
    uint8_t	sss_size;		/* secured silicon sector size (1<<N) */
    uint8_t	soft_feat;		/* software features */
    uint8_t	page_size;		/* page size (1<<N) */
    uint8_t	erase_susp_time_max;	/* erase susp. timeout max, 1<<N usec */
    uint8_t	prog_susp_time_max;	/* prog. susp. timeout max, 1<<N usec */
    uint8_t	embhwrst_time_max;	/* emb hw rst timeout max, 1<<N usec */
    uint8_t	hwrst_time_max;		/* !emb hw rst timeout max, 1<<N usec */
};

/* forward references for prototype(s) */
struct nor_softc;
struct cfi;
struct nor_chip;
struct cfi_chip;

extern void cfi_0002_init(struct nor_softc * const, struct cfi * const,
	struct nor_chip * const);
extern void cfi_0002_print(device_t, struct cfi * const);

#endif	/* _DEV_NOR_CFI_0002_H_ */

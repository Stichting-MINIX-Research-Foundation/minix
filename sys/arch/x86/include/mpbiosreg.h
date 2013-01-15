/* 	$NetBSD: mpbiosreg.h,v 1.6 2010/04/18 23:47:51 jym Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks Inc.
 *
 * Author: Bill Sommerfeld
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

#ifndef _X86_MPBIOSREG_H_
#define _X86_MPBIOSREG_H_

#define BIOS_BASE		(0xf0000)
#define BIOS_SIZE		(0x10000)
#define BIOS_COUNT		(BIOS_SIZE)

/*
 * Multiprocessor config table entry types.
 */

#define MPS_MCT_CPU	0
#define MPS_MCT_BUS	1
#define MPS_MCT_IOAPIC	2
#define MPS_MCT_IOINT	3
#define MPS_MCT_LINT	4

#define MPS_MCT_NTYPES	5

/* MP Floating Pointer Structure */
struct mpbios_fps {
	uint32_t	signature;
/* string defined by the Intel MP Spec as identifying the MP table */
#define MP_FP_SIG		0x5f504d5f	/* _MP_ */
	
	uint32_t 	pap;
	uint8_t  	length;
	uint8_t  	spec_rev;
	uint8_t  	checksum;
	uint8_t  	mpfb1;	/* system configuration */
	uint8_t  	mpfb2;	/* flags */
#define MPFPS_FLAG_IMCR		0x80	/* IMCR present */
	uint8_t  	mpfb3;	/* unused */
	uint8_t  	mpfb4;	/* unused */
	uint8_t  	mpfb5;	/* unused */
};

/* MP Configuration Table Header */
struct mpbios_cth {
	uint32_t	signature;
#define MP_CT_SIG		0x504d4350 	/* PCMP */
	
	uint16_t 	base_len;
	uint8_t  	spec_rev;
	uint8_t  	checksum;
	uint8_t  	oem_id[8];
	uint8_t  	product_id[12];
	uint32_t	oem_table_pointer;
	uint16_t 	oem_table_size;
	uint16_t 	entry_count;
	uint32_t	apic_address;
	uint16_t	ext_len;
	uint8_t  	ext_cksum;
	uint8_t  	reserved;
};

struct mpbios_proc {
	uint8_t  type;
	uint8_t  apic_id;
	uint8_t  apic_version;
	uint8_t  cpu_flags;
#define PROCENTRY_FLAG_EN	0x01
#define PROCENTRY_FLAG_BP	0x02
	uint32_t  reserved1;
	uint32_t  reserved2;
};

struct mpbios_bus {
	uint8_t  type;
	uint8_t  bus_id;
	char    bus_type[6];
};

struct mpbios_ioapic {
	uint8_t  type;
	uint8_t  apic_id;
	uint8_t  apic_version;
	uint8_t  apic_flags;
#define IOAPICENTRY_FLAG_EN	0x01
	uint32_t   apic_address;
};

struct mpbios_int {
	uint8_t  type;
	uint8_t  int_type;
	uint16_t int_flags;
	uint8_t  src_bus_id;
	uint8_t  src_bus_irq;
	uint8_t  dst_apic_id;
#define MPS_ALL_APICS	0xff
	uint8_t  dst_apic_int;
};


#endif /* !_X86_MPBIOSREG_H_ */

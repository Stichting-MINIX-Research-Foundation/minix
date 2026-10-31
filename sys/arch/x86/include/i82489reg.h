/*	$NetBSD: i82489reg.h,v 1.19 2019/06/14 05:59:39 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Frank van der Linden.
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


/*
 * Registers and constants for the 82489DX and Pentium (and up) integrated
 * "local" APIC.
 */

#define	LAPIC_ID	0x020	/* ID. (xAPIC: RW, x2APIC: RO) */
#	define LAPIC_ID_MASK		0xff000000
#	define LAPIC_ID_SHIFT		24

#define LAPIC_VERS	0x030	/* Version. RO */
#	define LAPIC_VERSION_MASK	0x000000ff
#	define LAPIC_VERSION_LVT_MASK	0x00ff0000
#	define LAPIC_VERSION_LVT_SHIFT	16
#	define LAPIC_VERSION_DIRECTED_EOI 0x01000000
#	define LAPIC_VERSION_EXTAPIC_SPACE 0x80000000

#define LAPIC_TPRI	0x080	/* Task Prio. RW */
#	define LAPIC_TPRI_MASK		0x000000ff
#	define LAPIC_TPRI_INT_MASK	0x000000f0
#	define LAPIC_TPRI_SUB_MASK	0x0000000f

#define LAPIC_APRI	0x090	/* Arbitration prio (xAPIC: RO, x2APIC: NA) */
#	define LAPIC_APRI_MASK		0x000000ff

#define LAPIC_PPRI	0x0a0	/* Processor prio. RO */
#define LAPIC_EOI	0x0b0	/* End Int. W */
#define LAPIC_RRR	0x0c0	/* Remote read (xAPIC: RO, x2APIC: NA) */
#define LAPIC_LDR	0x0d0	/* Logical dest. (xAPIC: RW, x2APIC: RO) */

#define LAPIC_DFR	0x0e0	/* Dest. format (xAPIC: RW, x2APIC: NA) */
#	define LAPIC_DFR_MASK		0xf0000000
#	define LAPIC_DFR_FLAT		0xf0000000
#	define LAPIC_DFR_CLUSTER	0x00000000

#define LAPIC_SVR	0x0f0	/* Spurious intvec RW */
#	define LAPIC_SVR_VECTOR_MASK	0x000000ff
#	define LAPIC_SVR_VEC_FIX	0x0000000f
#	define LAPIC_SVR_VEC_PROG	0x000000f0
#	define LAPIC_SVR_ENABLE		0x00000100
#	define LAPIC_SVR_SWEN		0x00000100
#	define LAPIC_SVR_FOCUS		0x00000200
#	define LAPIC_SVR_FDIS		0x00000200
#	define LAPIC_SVR_EOI_BC_DIS	0x00001000

#define LAPIC_ISR	0x100	/* In-Service Status RO */
#define LAPIC_TMR	0x180	/* Trigger Mode RO */
#define LAPIC_IRR	0x200	/* Interrupt Req RO */
#define LAPIC_ESR	0x280	/* Err status. RW */

/* Common definitions for ICR, LVT and MSIDATA */
#define LAPIC_VECTOR_MASK    __BITS(7, 0)
#define LAPIC_DLMODE_MASK    __BITS(10, 8)	/* Delivery Mode */
#define LAPIC_DLMODE_FIXED   __SHIFTIN(0, LAPIC_DLMODE_MASK)
#define LAPIC_DLMODE_LOW     __SHIFTIN(1, LAPIC_DLMODE_MASK) /* NA in x2APIC */
#define LAPIC_DLMODE_SMI     __SHIFTIN(2, LAPIC_DLMODE_MASK)
#define LAPIC_DLMODE_NMI     __SHIFTIN(4, LAPIC_DLMODE_MASK)
#define LAPIC_DLMODE_INIT    __SHIFTIN(5, LAPIC_DLMODE_MASK)
#define LAPIC_DLMODE_STARTUP __SHIFTIN(6, LAPIC_DLMODE_MASK) /* NA in LVT,MSI*/
#define LAPIC_DLMODE_EXTINT  __SHIFTIN(7, LAPIC_DLMODE_MASK) /* NA in x2APIC */

#define LAPIC_DLSTAT_BUSY    __BIT(12)	/* NA in x2APIC nor MSI */
#define LAPIC_DLSTAT_IDLE    0x00000000

#define LAPIC_LEVEL_MASK     __BIT(14)	/* LAPIC_LVT_LINT_RIRR in LVT LINT */
#define LAPIC_LEVEL_ASSERT   LAPIC_LEVEL_MASK
#define LAPIC_LEVEL_DEASSERT 0x00000000

#define LAPIC_TRIGMODE_MASK   __BIT(15)
#define LAPIC_TRIGMODE_EDGE   0x00000000
#define LAPIC_TRIGMODE_LEVEL  LAPIC_TRIGMODE_MASK

/* Common definitions for LVT */
#define LAPIC_LVT_MASKED     __BIT(16)

#define LAPIC_LVT_CMCI	0x2f0	/* Loc.vec (CMCI) RW */
#define LAPIC_ICRLO	0x300	/* Int. cmd. (xAPIC: RW, x2APIC: RW64) */

#	define LAPIC_DSTMODE_MASK	__BIT(11)
#	define LAPIC_DSTMODE_PHYS	__SHIFTIN(0, LAPIC_DSTMODE_MASK)
#	define LAPIC_DSTMODE_LOG	__SHIFTIN(1, LAPIC_DSTMODE_MASK)

#	define LAPIC_DEST_MASK		__BITS(19, 18)
#	define LAPIC_DEST_DEFAULT	__SHIFTIN(0, LAPIC_DEST_MASK)
#	define LAPIC_DEST_SELF		__SHIFTIN(1, LAPIC_DEST_MASK)
#	define LAPIC_DEST_ALLINCL	__SHIFTIN(2, LAPIC_DEST_MASK)
#	define LAPIC_DEST_ALLEXCL	__SHIFTIN(3, LAPIC_DEST_MASK)

#define LAPIC_ICRHI	0x310	/* Int. cmd. (xAPIC: RW, x2APIC: NA) */

#define LAPIC_LVT_TIMER	0x320	/* Loc.vec.(timer) RW */
#	define LAPIC_LVT_TMM		__BITS(18, 17)
#	define LAPIC_LVT_TMM_ONESHOT	__SHIFTIN(0, LAPIC_LVT_TMM)
#	define LAPIC_LVT_TMM_PERIODIC	__SHIFTIN(1, LAPIC_LVT_TMM)
#	define LAPIC_LVT_TMM_TSCDLT	__SHIFTIN(2, LAPIC_LVT_TMM)

#define LAPIC_LVT_THERM	0x330	/* Loc.vec (Thermal) RW */
#define LAPIC_LVT_PCINT	0x340	/* Loc.vec (Perf Mon) RW */
#define LAPIC_LVT_LINT0	0x350	/* Loc.vec (LINT0) RW */
#	define LAPIC_LVT_LINT_INP_POL	__BIT(13)
#	define LAPIC_LVT_LINT_RIRR	__BIT(14)

#define LAPIC_LVT_LINT1	0x360	/* Loc.vec (LINT1) RW */
#define LAPIC_LVT_ERR	0x370	/* Loc.vec (ERROR) RW */
#define LAPIC_ICR_TIMER	0x380	/* Initial count RW */
#define LAPIC_CCR_TIMER	0x390	/* Current count RO */

#define LAPIC_DCR_TIMER	0x3e0	/* Divisor config RW */
#	define LAPIC_DCRT_DIV1		0x0b
#	define LAPIC_DCRT_DIV2		0x00
#	define LAPIC_DCRT_DIV4		0x01
#	define LAPIC_DCRT_DIV8		0x02
#	define LAPIC_DCRT_DIV16		0x03
#	define LAPIC_DCRT_DIV32		0x08
#	define LAPIC_DCRT_DIV64		0x09
#	define LAPIC_DCRT_DIV128	0x0a

#define LAPIC_SELF_IPI	0x3f0	/* SELF IPI (xAPIC: NA, x2APIC: W) */
#	define LAPIC_SELF_IPI_VEC_MASK	0x000000ff

#define LAPIC_MSIADDR_BASE	0xfee00000
#define	LAPIC_MSIADDR_DSTID_MASK	__BITS(19, 12)
#define	LAPIC_MSIADDR_RSVD0_MASK	__BITS(11, 4)
#define	LAPIC_MSIADDR_RH		__BIT(3)
#define	LAPIC_MSIADDR_DM		__BIT(2)
#define	LAPIC_MSIADDR_RSVD1_MASK	__BITS(1, 0)

#define LAPIC_BASE		0xfee00000

#define LAPIC_IRQ_MASK(i)	(1 << ((i) + 1))

/* Extended APIC registers, valid when CPUID features4 EAPIC is present */
#define LEAPIC_FR	0x400	/* Feature register */
#	define LEAPIC_FR_ELC		__BITS(23,16) /* Ext. Lvt Count RO */
#	define LEAPIC_FR_EIDCAP		__BIT(2)     /* Ext. Apic ID Cap. RO */
#	define LEAPIC_FR_SEIOCAP	__BIT(1)     /* Specific EOI Cap. RO */
#	define LEAPIC_FR_IERCAP		__BIT(0)     /* Intr. Enable Reg. RO */

#define LEAPIC_CR	0x410	/* Control Register */
#	define LEAPIC_CR_EID_ENABLE	__BIT(2)     /* Ext. Apic ID enable */
#	define LEAPIC_CR_SEOI_ENABLE	__BIT(1)     /* Specific EOI enable */
#	define LEAPIC_CR_IER_ENABLE	__BIT(0)     /* Enable writes to IER */

#define LEAPIC_SEOIR	0x420	/* Specific EOI Register */
#	define LEAPIC_SEOI_VEC	__BITS(7,0)

#define LEAPIC_IER_480	0x480	/* Interrupts 0-31 */
#define LEAPIC_IER_490	0x490	/* Interrupts 32-63 */
#define LEAPIC_IER_4B0	0x4B0	/* Interrupts 64-95 */
#define LEAPIC_IER_4C0	0x4C0	/* Interrupts 96-127 */
#define LEAPIC_IER_4D0	0x4D0	/* Interrupts 128-159 */
#define LEAPIC_IER_4E0	0x4E0	/* Interrupts 160-191 */
#define LEAPIC_IER_4F0	0x4F0	/* Interrupts 192-255 */

/* Extended Local Vector Table Entries */
#define LEAPIC_LVTR_500	0x500
#define LEAPIC_LVTR_504	0x504
#define LEAPIC_LVTR_508	0x508
#define LEAPIC_LVTR_50C	0x50C
#define LEAPIC_LVTR_510	0x510
#define LEAPIC_LVTR_514	0x514
#define LEAPIC_LVTR_518	0x518
#define LEAPIC_LVTR_51C	0x51C
#define LEAPIC_LVTR_520	0x520
#define LEAPIC_LVTR_524	0x524
#define LEAPIC_LVTR_528	0x528
#define LEAPIC_LVTR_52C	0x52C
#define LEAPIC_LVTR_530	0x530
#	define LEAPIC_LVTR_MASK		__BIT(16)     /* interrupt masked RW */
#	define LEAPIC_LVTR_DSTAT	__BIT(12)	/* delivery state RO */
#	define LEAPIC_LVTR_MSGTYPE	__BITS(10,8)	/* Message type */
#	define LEAPIC_LVTR_VEC		__BITS(7,0)	/* the intr. vector */

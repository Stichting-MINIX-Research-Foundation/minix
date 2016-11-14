/*	$NetBSD: disassem.c,v 1.33 2015/05/02 16:18:49 skrll Exp $	*/

/*
 * Copyright (c) 1996 Mark Brinicombe.
 * Copyright (c) 1996 Brini.
 *
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * db_disasm.c
 *
 * Kernel disassembler
 *
 * Created      : 10/02/96
 *
 * Structured after the sparc/sparc/db_disasm.c by David S. Miller &
 * Paul Kranenburg
 *
 * This code is not complete. Not all instructions are disassembled.
 */

#include <sys/param.h>

__KERNEL_RCSID(0, "$NetBSD: disassem.c,v 1.33 2015/05/02 16:18:49 skrll Exp $");

#include <sys/systm.h>

#include <arch/arm/arm/disassem.h>
#include <arm/armreg.h>

#ifndef _KERNEL
#include <stdio.h>
#endif

/*
 * General instruction format
 *
 *	insn[cc][mod]	[operands]
 *
 * Those fields with an uppercase format code indicate that the field
 * follows directly after the instruction before the separator i.e.
 * they modify the instruction rather than just being an operand to
 * the instruction. The only exception is the writeback flag which
 * follows a operand.
 *
 * !c - cps flags and mode
 * !d - debug option (bit 0-3)
 * !l - dmb/dsb limitation
 * !m - mode
 * 2 - print Operand 2 of a data processing instruction
 * a - address operand of ldr/str instruction
 * b - branch address
 * c - comment field bits(0-23)
 * d - destination register (bits 12-15)
 * e - address operand of ldrx/strx instruction
 * f - 1st fp operand (register) (bits 12-14)
 * g - 2nd fp operand (register) (bits 16-18)
 * h - 3rd fp operand (register/immediate) (bits 0-4)
 * i - lsb operand (bits 7-11)
 * j - msb operand (bits 6,7,12-14)
 * k - breakpoint comment (bits 0-3, 8-19)
 * l - register list for ldm/stm instruction
 * m - m register (bits 0-3)
 * n - n register (bits 16-19)
 * o - indirect register rn (bits 16-19) (used by swap)
 * p - saved or current status register
 * q - neon N register (7, 19-16)
 * r - width minus 1 (bits 16-20)
 * s - s register (bits 8-11)
 * t - thumb branch address (bits 24, 0-23)
 * u - neon M register (5, 3-0)
 * v - co-processor data transfer registers + addressing mode
 * w - neon D register (22, 15-12)
 * x - instruction in hex
 * y - co-processor data processing registers
 * z - co-processor register transfer registers
 * C - cps effect
 * D - destination-is-r15 (P) flag on TST, TEQ, CMP, CMN
 * F - PSR transfer fields
 * I - NEON operand size
 * L - co-processor transfer size
 * N - quad neon operand
 * P - fp precision
 * Q - fp precision (for ldf/stf)
 * R - fp rounding
 * S - set status flag
 * U - neon unsigned.
 * W - writeback flag
 * X - block transfer type
 * Y - block transfer type (r13 base)
 * Z - 16-bit value (movw,movt)
 * # - co-processor number
 */

struct arm32_insn {
	u_int mask;
	u_int pattern;
	const char* name;
	const char* format;
};

static const struct arm32_insn arm32_i[] = {
    /* A5.7 Unconditional instructions */
    /*
     * A5.7.1 Memory hints, Advanced SIMD instructions, and
     * miscellaneous instructions
     */
    { 0xfff10020, 0xf1000000, "cps",	"C!c" },
    { 0xfff100f0, 0xf1010000, "setend\tle", "" },
    { 0xfff102f0, 0xf1010200, "setend\tbe", "" },
/* pli */
/* pld */
    { 0xffffffff, 0xf57ff01f, "clrex",  "" },
    { 0xfffffff0, 0xf57ff040, "dsb",    "!l" },
    { 0xfffffff0, 0xf57ff050, "dmb",    "!l" },
    { 0xfffffff0, 0xf57ff060, "isb",    "" },
/* pli */
/* pld */

    //{ 0x0e100000, 0x08000000, "stm",	"XnWl" },
    { 0xfe5fffe0, 0xf84d0500, "srs",	"XnW!m" },
    { 0xfe50ffff, 0xf8100a00, "rfe",	"XnW" },
    { 0xfe000000, 0xfa000000, "blx",	"t" },		/* Before b and bl */
    { 0xfe100090, 0xfc000000, "stc2",	"L#v" },
    { 0x0e100090, 0x0c000000, "stc",	"L#v" },
    { 0xfe100090, 0xfc100000, "ldc2",	"L#v" },
    { 0x0e100090, 0x0c100000, "ldc",	"L#v" },
    { 0x0ff00000, 0x0c400000, "mcrr",	"#&" },
    { 0x0ff00000, 0x0c500000, "mrrc",	"#&" },
    { 0xff000010, 0xfe000000, "cdp2",	"#y" },
    { 0x0f000010, 0x0e000000, "cdp",	"#y" },
    { 0xff100010, 0xfe000010, "mcr2",	"#z" },
    { 0x0f100010, 0x0e000010, "mcr",	"#z" },
    { 0xff100010, 0xfe100010, "mrc2",	"#z" },
    { 0x0f100010, 0x0e100010, "mrc",	"#z" },

    /* A5.4 Media instructions  */
    { 0x0fe00070, 0x07c00050, "sbfx",	"dmir" },
    { 0x0fe0007f, 0x07c0001f, "bfc",    "dij" },
    { 0x0fe00070, 0x07c00010, "bfi",    "dmij" },
    { 0x0fe00070, 0x07e00050, "ubfx",	"dmir" },
    { 0xfff000f0, 0xe70000f0, "und",	"x" },		/* Special immediate? */

    { 0x06000010, 0x06000010, "und",	"x" },		/* Remove when done with media */

    { 0x0d700000, 0x04200000, "strt",	"daW" },
    { 0x0d700000, 0x04300000, "ldrt",	"daW" },
    { 0x0d700000, 0x04600000, "strbt",	"daW" },
    { 0x0d700000, 0x04700000, "ldrbt",	"daW" },

    { 0x0c500000, 0x04000000, "str",	"daW" },
    { 0x0c500000, 0x04100000, "ldr",	"daW" },
    { 0x0c500000, 0x04400000, "strb",	"daW" },
    { 0x0c500000, 0x04500000, "ldrb",	"daW" },


    /* A5.5 Branch, branch with link, and block data transfer */
    { 0x0fff0000, 0x092d0000, "push",	"l" },	/* separate out r13 base */
    { 0x0fff0000, 0x08bd0000, "pop",	"l" },	/* separate out r13 base */
    { 0x0e1f0000, 0x080d0000, "stm",	"YnWl" },/* separate out r13 base */
    { 0x0e1f0000, 0x081d0000, "ldm",	"YnWl" },/* separate out r13 base */
    { 0x0e100000, 0x08000000, "stm",	"XnWl" },
    { 0x0e100000, 0x08100000, "ldm",	"XnWl" },
    { 0x0f000000, 0x0a000000, "b",	"b" },
    { 0x0f000000, 0x0b000000, "bl",	"b" },

    { 0x0fffffff, 0x0ff00000, "imb",	"c" },		/* Before swi */
    { 0x0fffffff, 0x0ff00001, "imbrange", "c" },	/* Before swi */
    { 0x0f000000, 0x0f000000, "swi",	"c" },

    /*
     * A5.2 Data-process and miscellaneous instructions
     */

    /* A5.2 exceptions */

    /* A5.2.7 Halfword multiply and multiply accumulate */

    /* A5.2.8 Extra load/store instructions */

    { 0x0e1000f0, 0x000000b0, "strh",	"de" },
    { 0x0e1000f0, 0x001000b0, "ldrh",	"de" },

    { 0x0e5000f0, 0x000000d0, "ldrd",	"de" },
    { 0x0e5000f0, 0x001000d0, "ldrsb",	"de" },
    { 0x0e5000f0, 0x004000d0, "ldrd",	"de" },
    { 0x0e5000f0, 0x005000d0, "ldrsb",	"de" },

    { 0x0e1000f0, 0x000000f0, "ldrd",	"de" },
    { 0x0e1000f0, 0x001000f0, "ldrsb",	"de" },
    { 0x0e1000f0, 0x000000f0, "strd",	"de" },
    { 0x0e1000f0, 0x001000f0, "ldrsh",	"de" },

    /* A5.2.11 MSR (immediate), and hints */
    { 0x0fffffff, 0x0320f000, "nop",	"" },
    { 0x0fffffff, 0x0320f001, "yield",	"" },
    { 0x0fffffff, 0x0320f002, "wfe",	"" },
    { 0x0fffffff, 0x0320f003, "wfi",	"" },
    { 0x0fffffff, 0x0320f004, "sev",	"" },
    { 0x0ffffff0, 0x0320f0f0, "dbg",	"!d" },

    /* A5.2.12 Miscellaneous instructions - before data processing */

    { 0x0fbf0fff, 0x010f0000, "mrs",	"dp" },	/* A8.8.109, B9.3.8 */
    { 0x0fb00eff, 0x01000200, "mrs",	"c" },	/* XXXNH: B9.3.9 */
    { 0x0fb0fff0, 0x0120f000, "msr",	"pFm" },
    { 0x0fe0f000, 0x0320f000, "msr",	"pF2" },

    { 0x0ffffff0, 0x012fff10, "bx",	"m" },
    { 0x0fff0ff0, 0x016f0f10, "clz",	"dm" },
/* bxj */
    { 0x0ffffff0, 0x012fff30, "blx",	"m" },
/* saturating */
/* eret */
    { 0xfff000f0, 0xe1200070, "bkpt",	"k" },
/* hvc */
/* smc */

    { 0x0ff00000, 0x03000000, "movw", 	"dZ" },
    { 0x0ff00000, 0x03400000, "movt", 	"dZ" },

    /* A5.2.10 Synchronisation primitives */
    { 0x0ff00ff0, 0x01000090, "swp",	"dmo" },
    { 0x0ff00ff0, 0x01400090, "swpb",	"dmo" },
    { 0x0ff00fff, 0x01900f9f, "ldrex",	"da" },
    { 0x0ff00fff, 0x01b00f9f, "ldrexd",	"da" },
    { 0x0ff00fff, 0x01d00f9f, "ldrexb",	"da" },
    { 0x0ff00fff, 0x01f00f9f, "ldrexh",	"da" },
    { 0x0ff00ff0, 0x01800f90, "strex",	"dma" },
    { 0x0ff00ff0, 0x01a00f90, "strexd",	"dma" },
    { 0x0ff00ff0, 0x01c00f90, "strexb",	"dma" },
    { 0x0ff00ff0, 0x01e00f90, "strexh",	"dma" },

    /* A5.2 non-exceptions */

    /* A5.2.1, A5.2.2, and A5.2.3 Data-processing */
    { 0x0de00000, 0x00000000, "and",	"Sdn2" },
    { 0x0de00000, 0x00200000, "eor",	"Sdn2" },
    { 0x0de00000, 0x00400000, "sub",	"Sdn2" },
    { 0x0de00000, 0x00600000, "rsb",	"Sdn2" },
    { 0x0de00000, 0x00800000, "add",	"Sdn2" },
    { 0x0de00000, 0x00a00000, "adc",	"Sdn2" },
    { 0x0de00000, 0x00c00000, "sbc",	"Sdn2" },
    { 0x0de00000, 0x00e00000, "rsc",	"Sdn2" },
    { 0x0df00000, 0x01100000, "tst",	"Dn2" },
    { 0x0df00000, 0x01300000, "teq",	"Dn2" },
    { 0x0df00000, 0x01500000, "cmp",	"Dn2" },
    { 0x0df00000, 0x01700000, "cmn",	"Dn2" },
    { 0x0de00000, 0x01800000, "orr",	"Sdn2" },
    { 0x0de00000, 0x01a00000, "mov",	"Sd2" },
    { 0x0de00000, 0x01c00000, "bic",	"Sdn2" },
    { 0x0de00000, 0x01e00000, "mvn",	"Sd2" },

    /* A5.2.5 Multiply and multiply accumulate */
    { 0x0fe000f0, 0x00000090, "mul",	"Snms" },
    { 0x0fe000f0, 0x00200090, "mla",	"Snmsd" },
    { 0x0fe000f0, 0x00800090, "umull",	"Sdnms" },
    { 0x0fe000f0, 0x00c00090, "smull",	"Sdnms" },
    { 0x0fe000f0, 0x00a00090, "umlal",	"Sdnms" },
    { 0x0fe000f0, 0x00e00090, "smlal",	"Sdnms" },

    /* */
    { 0x0ff08f10, 0x0e000100, "adf",	"PRfgh" },
    { 0x0ff08f10, 0x0e100100, "muf",	"PRfgh" },
    { 0x0ff08f10, 0x0e200100, "suf",	"PRfgh" },
    { 0x0ff08f10, 0x0e300100, "rsf",	"PRfgh" },
    { 0x0ff08f10, 0x0e400100, "dvf",	"PRfgh" },
    { 0x0ff08f10, 0x0e500100, "rdf",	"PRfgh" },
    { 0x0ff08f10, 0x0e600100, "pow",	"PRfgh" },
    { 0x0ff08f10, 0x0e700100, "rpw",	"PRfgh" },
    { 0x0ff08f10, 0x0e800100, "rmf",	"PRfgh" },
    { 0x0ff08f10, 0x0e900100, "fml",	"PRfgh" },
    { 0x0ff08f10, 0x0ea00100, "fdv",	"PRfgh" },
    { 0x0ff08f10, 0x0eb00100, "frd",	"PRfgh" },
    { 0x0ff08f10, 0x0ec00100, "pol",	"PRfgh" },
    { 0x0f008f10, 0x0e000100, "fpbop",	"PRfgh" },
    { 0x0ff08f10, 0x0e008100, "mvf",	"PRfh" },
    { 0x0ff08f10, 0x0e108100, "mnf",	"PRfh" },
    { 0x0ff08f10, 0x0e208100, "abs",	"PRfh" },
    { 0x0ff08f10, 0x0e308100, "rnd",	"PRfh" },
    { 0x0ff08f10, 0x0e408100, "sqt",	"PRfh" },
    { 0x0ff08f10, 0x0e508100, "log",	"PRfh" },
    { 0x0ff08f10, 0x0e608100, "lgn",	"PRfh" },
    { 0x0ff08f10, 0x0e708100, "exp",	"PRfh" },
    { 0x0ff08f10, 0x0e808100, "sin",	"PRfh" },
    { 0x0ff08f10, 0x0e908100, "cos",	"PRfh" },
    { 0x0ff08f10, 0x0ea08100, "tan",	"PRfh" },
    { 0x0ff08f10, 0x0eb08100, "asn",	"PRfh" },
    { 0x0ff08f10, 0x0ec08100, "acs",	"PRfh" },
    { 0x0ff08f10, 0x0ed08100, "atn",	"PRfh" },
    { 0x0f008f10, 0x0e008100, "fpuop",	"PRfh" },
    { 0x0e100f00, 0x0c000100, "stf",	"QLv" },
    { 0x0e100f00, 0x0c100100, "ldf",	"QLv" },
    { 0x0ff00f10, 0x0e000110, "flt",	"PRgd" },
    { 0x0ff00f10, 0x0e100110, "fix",	"PRdh" },
    { 0x0ff00f10, 0x0e200110, "wfs",	"d" },
    { 0x0ff00f10, 0x0e300110, "rfs",	"d" },
    { 0x0ff00f10, 0x0e400110, "wfc",	"d" },
    { 0x0ff00f10, 0x0e500110, "rfc",	"d" },
    { 0x0ff0ff10, 0x0e90f110, "cmf",	"PRgh" },
    { 0x0ff0ff10, 0x0eb0f110, "cnf",	"PRgh" },
    { 0x0ff0ff10, 0x0ed0f110, "cmfe",	"PRgh" },
    { 0x0ff0ff10, 0x0ef0f110, "cnfe",	"PRgh" },

    { 0xffb00f10, 0xf2000110, "vand",	"Nuqw" },
    { 0xffb00f10, 0xf2100110, "vbic",	"Nuqw" },
    { 0xffb00f10, 0xf2200110, "vorr",	"Nuqw" },
    { 0xffb00f10, 0xf2300110, "vorn",	"Nuqw" },
    { 0xffb00f10, 0xf3000110, "veor",	"Nuqw" },
    { 0xffb00f10, 0xf3100110, "vbsl",	"Nuqw" },
    { 0xffb00f10, 0xf3200110, "vbit",	"Nuqw" },
    { 0xffb00f10, 0xf3300110, "vbif",	"Nuqw" },
    { 0xfe800f10, 0xf3000400, "vshl",	"SINuqw" },
    { 0xfe800f10, 0xf3000410, "vqshl",	"SINuqw" },
    { 0xfe800f10, 0xf3000500, "vrshl",	"SINuqw" },
    { 0xfe800f10, 0xf3000510, "vqrshl",	"SINuqw" },
    { 0xffb00f10, 0xf2000800, "vadd",	"INuqw" },
    { 0xffb00f10, 0xf2000810, "vtst",	"INuqw" },
    { 0xffb00f10, 0xf3000800, "vsub",	"INuqw" },
    { 0x00000000, 0x00000000, NULL,	NULL }
};

static char const arm32_insn_conditions[][4] = {
	"eq", "ne", "cs", "cc",
	"mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt",
	"gt", "le", "",   "nv"
};

static char const insn_block_transfers[][4] = {
	"da", "ia", "db", "ib"
};

static char const insn_stack_block_transfers[][4] = {
	"ed", "ea", "fd", "fa",	/* stm */
	"fa", "fd", "ea", "ed",	/* ldm */
};

static char const op_shifts[][4] = {
	"lsl", "lsr", "asr", "ror"
};

static char const *insn_barrier_limiation[] = {
	"",
	"",
	"oshst",	/* 0b0010 */
	"osh",		/* 0b0011 */
	"",
	"",
	"nshst",	/* 0b0110 */
	"nsh",		/* 0b0111 */
	"",
	"",
	"ishst",	/* 0b1010 */
	"ish",		/* 0b1011 */
	"",
	"",
	"st",		/* 0b1110 */
	"sy",		/* 0b1111 */
};

static char const insn_fpa_rounding[][2] = {
	"", "p", "m", "z"
};

static char const insn_fpa_precision[][2] = {
	"s", "d", "e", "p"
};

static char const insn_fpaconstants[][8] = {
	"0.0", "1.0", "2.0", "3.0",
	"4.0", "5.0", "0.5", "10.0"
};

#define insn_condition(x)	arm32_insn_conditions[(x >> 28) & 0x0f]
#define insn_blktrans(x)	insn_block_transfers[(x >> 23) & 3]
#define insn_stkblktrans(x)	insn_stack_block_transfers[((x >> (20 - 2)) & 4)|((x >> 23) & 3)]
#define insn_limitation(x)	insn_barrier_limiation[x & 0xf]
#define op2_shift(x)		op_shifts[(x >> 5) & 3]
#define insn_fparnd(x)		insn_fpa_rounding[(x >> 5) & 0x03]
#define insn_fpaprec(x)		insn_fpa_precision[(((x >> 18) & 2)|(x >> 7)) & 1]
#define insn_fpaprect(x)	insn_fpa_precision[(((x >> 21) & 2)|(x >> 15)) & 1]
#define insn_fpaimm(x)		insn_fpaconstants[x & 0x07]

/* Local prototypes */
static void disasm_cps(const disasm_interface_t *, u_int);
static void disasm_register_print(const disasm_interface_t *,u_int);
static void disasm_register_shift(const disasm_interface_t *, u_int);
static void disasm_print_reglist(const disasm_interface_t *, u_int);
static void disasm_insn_ldrstr(const disasm_interface_t *, u_int,
    u_int);
static void disasm_insn_ldrxstrx(const disasm_interface_t *, u_int,
    u_int);
static void disasm_insn_ldcstc(const disasm_interface_t *, u_int,
    u_int);
static u_int disassemble_readword(u_int);
static void disassemble_printaddr(u_int);

vaddr_t
disasm(const disasm_interface_t *di, vaddr_t loc, int altfmt)
{
	const struct arm32_insn *i_ptr = (const struct arm32_insn *)&arm32_i;

	u_int insn;
	int matchp;
	int branch;
	const char* f_ptr;
	int fmt;

	if (loc & 3) {
		/* Don't crash for now.  */
		di->di_printf("thumb insn\n");
		return (loc + THUMB_INSN_SIZE);
	}

	fmt = 0;
	matchp = 0;
	insn = di->di_readword(loc);
#if defined(__ARMEB__) && defined(CPU_ARMV7)
	insn = bswap32(insn);
#endif
	char neonfmt = 'd';
	char neonsign = 'u';

/*	di->di_printf("loc=%08x insn=%08x : ", loc, insn);*/

	while (i_ptr->name) {
		if ((insn & i_ptr->mask) ==  i_ptr->pattern) {
			matchp = 1;
			break;
		}
		i_ptr++;
	}

	if (!matchp) {
		di->di_printf("und%s\t%08x\n", insn_condition(insn), insn);
		return(loc + INSN_SIZE);
	}

	/* If instruction forces condition code, don't print it. */
	if ((i_ptr->mask & 0xf0000000) == 0xf0000000)
		di->di_printf("%s", i_ptr->name);
	else
		di->di_printf("%s%s", i_ptr->name, insn_condition(insn));

	f_ptr = i_ptr->format;

	/* Insert tab if there are no instruction modifiers */

	if (*(f_ptr) < 'A' || *(f_ptr) > 'Z') {
		++fmt;
		di->di_printf("\t");
	}

	while (*f_ptr) {
		switch (*f_ptr) {
		case '!':
			f_ptr++;
			switch (*f_ptr) {
			/* !c - cps flags and mode */
			case 'c':
				disasm_cps(di, insn);
				break;
			/* !d - debug option */
			case 'd':
				di->di_printf("#%d", insn & 0xf);
				break;
 			/* !l - barrier dmb/dsb limitation */
			case 'l':
				di->di_printf("%s", insn_limitation(insn));
				break;
			/* !m - mode */
			case 'm':
				di->di_printf(", #%d", insn & 0x1f);
				break;
			}
			break;
		/* 2 - print Operand 2 of a data processing instruction */
		case '2':
			if (insn & 0x02000000) {
				int rotate = ((insn >> 7) & 0x1e);
				int imm = (insn & 0xff) << (32 - rotate) |
					      (insn & 0xff) >> rotate;
				di->di_printf("#%d		; #0x%x",
					      imm, imm);
			} else {
				disasm_register_shift(di, insn);
			}
			break;
		/* d - destination register (bits 12-15) */
		case 'd':
			disasm_register_print(di, (insn >> 12) & 0x0f);
			break;
		/* u - neon destination register (bits 22, 12-15) */
		case 'u':
			di->di_printf("%c%d", neonfmt,
			    ((insn >> 18) & 0x10)|((insn >> 12) & 0x0f));
			break;
		/* D - insert 'p' if Rd is R15 */
		case 'D':
			if (((insn >> 12) & 0x0f) == 15)
				di->di_printf("p");
			break;
		/* n - n register (bits 16-19) */
		case 'n':
			disasm_register_print(di, (insn >> 16) & 0x0f);
			break;
		/* q - neon n register (bits 7, 16-19) */
		case 'q':
			di->di_printf("%c%d", neonfmt,
			    ((insn >> 3) & 0x10)|((insn >> 16) & 0x0f));
			break;
		/* s - s register (bits 8-11) */
		case 's':
			di->di_printf("r%d", ((insn >> 8) & 0x0f));
			break;
		/* o - indirect register rn (bits 16-19) (used by swap) */
		case 'o':
			di->di_printf("[r%d]", ((insn >> 16) & 0x0f));
			break;
		/* m - m register (bits 0-3) */
		case 'm':
			di->di_printf("r%d", ((insn >> 0) & 0x0f));
			break;
		/* w - neon m register (bits 5, 0-3) */
		case 'w':
			di->di_printf("%c%d", neonfmt,
			    ((insn >> 1) & 0x10)|(insn & 0x0f));
			break;
		/* a - address operand of ldr/str instruction */
		case 'a':
			disasm_insn_ldrstr(di, insn, loc);
			break;
		/* e - address operand of ldrx/strx instruction */
		case 'e':
			disasm_insn_ldrxstrx(di, insn, loc);
			break;
		/* l - register list for ldm/stm instruction */
		case 'l':
			disasm_print_reglist(di, insn);
			break;
		/* f - 1st fp operand (register) (bits 12-14) */
		case 'f':
			di->di_printf("f%d", (insn >> 12) & 7);
			break;
		/* g - 2nd fp operand (register) (bits 16-18) */
		case 'g':
			di->di_printf("f%d", (insn >> 16) & 7);
			break;
		/* h - 3rd fp operand (register/immediate) (bits 0-4) */
		case 'h':
			if (insn & (1 << 3))
				di->di_printf("#%s", insn_fpaimm(insn));
			else
				di->di_printf("f%d", insn & 7);
			break;
		/* i - lsb operand (bits 7-11) */
		case 'i':
			di->di_printf("#%d", (insn >> 7) & 0x1f);
			break;
		/* j - msb operand (bits 16-20) as width */
		case 'j':
			di->di_printf("#%d",
			    ((insn >> 16) & 0x1f) - ((insn >> 7) & 0x1f) + 1);
			break;
 		/* r - width minus 1 (bits 16-20) */
		case 'r':
			di->di_printf("#%d", ((insn >> 16) & 0x1f) + 1);
			break;
		/* b - branch address */
		case 'b':
			branch = ((insn << 2) & 0x03ffffff);
			if (branch & 0x02000000)
				branch |= 0xfc000000;
			di->di_printaddr(loc + 8 + branch);
			break;
		/* t - blx address */
		case 't':
			branch = ((insn << 2) & 0x03ffffff) |
			    (insn >> 23 & 0x00000002);
			if (branch & 0x02000000)
				branch |= 0xfc000000;
			di->di_printaddr(loc + 8 + branch);
			break;
		case 'N':
			if (insn & 0x40)
				neonfmt = 'q';
			break;
		case 'U':
			if (insn & (1 << 24))
				neonsign = 's';
			break;
		case 'I':
			di->di_printf(".%c%d", neonsign,
			    8 << ((insn >> 20) & 3));
			break;
		/* X - block transfer type */
		case 'X':
			di->di_printf("%s", insn_blktrans(insn));
			break;
		/* Y - block transfer type (r13 base) */
		case 'Y':
			di->di_printf("%s", insn_stkblktrans(insn));
			break;
		/* Z - print movw/movt argument */
		case 'Z':
			di->di_printf(", #0x%04x",
			    ((insn & 0xf0000) >> 4) | (insn & 0xfff));
			break;
		/* c - comment field bits(0-23) */
		case 'c':
			di->di_printf("0x%08x", (insn & 0x00ffffff));
			break;
		/* k - breakpoint comment (bits 0-3, 8-19) */
		case 'k':
			di->di_printf("0x%04x",
			    (insn & 0x000fff00) >> 4 | (insn & 0x0000000f));
			break;
		/* p - saved or current status register */
		case 'p':
			if (insn & 0x00400000)
				di->di_printf("spsr");
			else
				di->di_printf("cpsr");
			break;
		/* C - cps effect */
		case 'C':
			if ((insn & 0x000c0000) == 0x000c0000)
				di->di_printf("id");
			else if ((insn & 0x000c0000) == 0x00080000)
				di->di_printf("ie");
			break;
		/* F - PSR transfer fields */
		case 'F':
			di->di_printf("_");
			if (insn & (1 << 16))
				di->di_printf("c");
			if (insn & (1 << 17))
				di->di_printf("x");
			if (insn & (1 << 18))
				di->di_printf("s");
			if (insn & (1 << 19))
				di->di_printf("f");
			break;
		/* B - byte transfer flag */
		case 'B':
			if (insn & 0x00400000)
				di->di_printf("b");
			break;
		/* L - co-processor transfer size */
		case 'L':
			if (insn & (1 << 22))
				di->di_printf("l");
			break;
		/* S - set status flag */
		case 'S':
			if (insn & 0x00100000)
				di->di_printf("s");
			break;
		/* P - fp precision */
		case 'P':
			di->di_printf("%s", insn_fpaprec(insn));
			break;
		/* Q - fp precision (for ldf/stf) */
		case 'Q':
			break;
		/* R - fp rounding */
		case 'R':
			di->di_printf("%s", insn_fparnd(insn));
			break;
		/* W - writeback flag */
		case 'W':
			if (insn & (1 << 21))
				di->di_printf("!");
			break;
		/* # - co-processor number */
		case '#':
			di->di_printf("p%d", (insn >> 8) & 0x0f);
			break;
		/* v - co-processor data transfer registers+addressing mode */
		case 'v':
			disasm_insn_ldcstc(di, insn, loc);
			break;
		/* x - instruction in hex */
		case 'x':
			di->di_printf("0x%08x", insn);
			break;
		/* y - co-processor data processing registers */
		case 'y':
			di->di_printf("%d, ", (insn >> 20) & 0x0f);

			di->di_printf("c%d, c%d, c%d", (insn >> 12) & 0x0f,
			    (insn >> 16) & 0x0f, insn & 0x0f);

			di->di_printf(", %d", (insn >> 5) & 0x07);
			break;
		/* z - co-processor register transfer registers */
		case 'z':
			di->di_printf("%d, ", (insn >> 21) & 0x07);
			di->di_printf("r%d, c%d, c%d, %d",
			    (insn >> 12) & 0x0f, (insn >> 16) & 0x0f,
			    insn & 0x0f, (insn >> 5) & 0x07);

/*			if (((insn >> 5) & 0x07) != 0)
				di->di_printf(", %d", (insn >> 5) & 0x07);*/
			break;
		/* & - co-processor register range transfer registers */
		case '&':
			di->di_printf("%d, r%d, r%d, c%d",
			    (insn >> 4) & 0x0f, (insn >> 12) & 0x0f,
			    (insn >> 16) & 0x0f, insn & 0x0f);
			break;
		default:
			di->di_printf("[%c - unknown]", *f_ptr);
			break;
		}
		if (*(f_ptr+1) >= 'A' && *(f_ptr+1) <= 'Z')
			++f_ptr;
		else if (*(++f_ptr)) {
			++fmt;
			if (fmt == 1)
				di->di_printf("\t");
			else
				di->di_printf(", ");
		}
	};

	di->di_printf("\n");

	return(loc + INSN_SIZE);
}

static void
disasm_register_print(const disasm_interface_t *di, u_int r)
{
	switch (r) {
	case 13:
		di->di_printf("sp");
		break;
	case 14:
		di->di_printf("lr");
		break;
	case 15:
		di->di_printf("pc");
		break;
	default:
		di->di_printf("r%d", r);
		break;
	}
}

static void
disasm_cps(const disasm_interface_t *di, u_int insn)
{
	if ((insn & 0x000c0000) == 0x000c0000 ||
	    (insn & 0x000c0000) == 0x00080000) {
		if (insn & (1 << 8))
			di->di_printf("a");
		if (insn & (1 << 7))
			di->di_printf("i");
		if (insn & (1 << 6))
			di->di_printf("f");
		if ((insn & (1 << 17)) && ((insn & 0x1f) != 0))
			di->di_printf(", ");
	}
	if ((insn & (1 << 17)) && ((insn & 0x1f) != 0))
		di->di_printf("#%d", insn & 0x1f);
}

static void
disasm_register_shift(const disasm_interface_t *di, u_int insn)
{
	di->di_printf("r%d", (insn & 0x0f));
	if ((insn & 0x00000ff0) == 0)
		;
	else if ((insn & 0x00000ff0) == 0x00000060)
		di->di_printf(", rrx");
	else {
		if (insn & 0x10)
			di->di_printf(", %s r%d", op2_shift(insn),
			    (insn >> 8) & 0x0f);
		else
			di->di_printf(", %s #%d", op2_shift(insn),
			    (insn >> 7) & 0x1f);
	}
}


static void
disasm_print_reglist(const disasm_interface_t *di, u_int insn)
{
	int loop;
	int start;
	int comma;

	di->di_printf("{");
	start = -1;
	comma = 0;

	for (loop = 0; loop < 17; ++loop) {
		if (start != -1) {
			if (loop == 16 || !(insn & (1 << loop))) {
				if (comma)
					di->di_printf(", ");
				else
					comma = 1;
        			if (start == loop - 1)
        				di->di_printf("r%d", start);
        			else
        				di->di_printf("r%d-r%d", start, loop - 1);
        			start = -1;
        		}
        	} else {
        		if (insn & (1 << loop))
        			start = loop;
        	}
        }
	di->di_printf("}");

	if (insn & (1 << 22))
		di->di_printf("^");
}

static void
disasm_insn_ldrstr(const disasm_interface_t *di, u_int insn, u_int loc)
{
	int offset;

	offset = insn & 0xfff;
	if ((insn & 0x032f0000) == 0x010f0000) {
		/* rA = pc, immediate index */
		if (insn & 0x00800000)
			loc += offset;
		else
			loc -= offset;
		di->di_printaddr(loc + 8);
 	} else {
		di->di_printf("[r%d", (insn >> 16) & 0x0f);
		if ((insn & 0x03000fff) != 0x01000000
		    && (insn & 0x0f800ff0) != 0x01800f90) {
			di->di_printf("%s, ", (insn & (1 << 24)) ? "" : "]");
			if (!(insn & 0x00800000))
				di->di_printf("-");
			if (insn & (1 << 25))
				disasm_register_shift(di, insn);
			else
				di->di_printf("#0x%03x", offset);
		}
		if (insn & (1 << 24))
			di->di_printf("]");
	}
}

static void
disasm_insn_ldrxstrx(const disasm_interface_t *di, u_int insn, u_int loc)
{
	int offset;

	offset = ((insn & 0xf00) >> 4) | (insn & 0xf);
	if ((insn & 0x004f0000) == 0x004f0000) {
		/* rA = pc, immediate index */
		if (insn & 0x00800000)
			loc += offset;
		else
			loc -= offset;
		di->di_printaddr(loc + 8);
 	} else {
		di->di_printf("[r%d", (insn >> 16) & 0x0f);
		if ((insn & 0x01400f0f) != 0x01400000) {
			di->di_printf("%s, ", (insn & (1 << 24)) ? "" : "]");
			if (!(insn & 0x00800000))
				di->di_printf("-");
			if (insn & (1 << 22))
				di->di_printf("#0x%02x", offset);
			else
				di->di_printf("r%d", (insn & 0x0f));
		}
		if (insn & (1 << 24))
			di->di_printf("]");
	}
}

static void
disasm_insn_ldcstc(const disasm_interface_t *di, u_int insn, u_int loc)
{
	if (((insn >> 8) & 0xf) == 1)
		di->di_printf("f%d, ", (insn >> 12) & 0x07);
	else
		di->di_printf("c%d, ", (insn >> 12) & 0x0f);

	di->di_printf("[r%d", (insn >> 16) & 0x0f);

	di->di_printf("%s, ", (insn & (1 << 24)) ? "" : "]");

	if (!(insn & (1 << 23)))
		di->di_printf("-");

	di->di_printf("#0x%03x", (insn & 0xff) << 2);

	if (insn & (1 << 24))
		di->di_printf("]");

	if (insn & (1 << 21))
		di->di_printf("!");
}

static u_int
disassemble_readword(u_int address)
{
	return(*((u_int *)address));
}

static void
disassemble_printaddr(u_int address)
{
	printf("0x%08x", address);
}

static const disasm_interface_t disassemble_di = {
	disassemble_readword, disassemble_printaddr,
	(void (*)(const char *, ...))printf
};

void
disassemble(u_int address)
{

	(void)disasm(&disassemble_di, address, 0);
}

/* End of disassem.c */

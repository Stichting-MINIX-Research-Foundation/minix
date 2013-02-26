/*	$NetBSD: t_bpfjit.c,v 1.1 2012/11/11 17:37:34 alnsn Exp $ */

/*-
 * Copyright (c) 2011-2012 Alexander Nasonov.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: t_bpfjit.c,v 1.1 2012/11/11 17:37:34 alnsn Exp $");

#include <net/bpfjit.h>

#include <atf-c.h>
#include <stdint.h>
#include <string.h>

static uint8_t deadbeef_at_5[16] = {
	0, 0xf1, 2, 0xf3, 4, 0xde, 0xad, 0xbe, 0xef, 0xff
};

ATF_TC(bpfjit_empty);
ATF_TC_HEAD(bpfjit_empty, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that JIT compilation for an empty bpf program fails");
}

ATF_TC_BODY(bpfjit_empty, tc)
{
	struct bpf_insn dummy;

	ATF_CHECK(bpfjit_generate_code(&dummy, 0) == NULL);
}

ATF_TC(bpfjit_alu_add_k);
ATF_TC_HEAD(bpfjit_alu_add_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_ADD+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_add_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 3),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, 2),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 5);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_sub_k);
ATF_TC_HEAD(bpfjit_alu_sub_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_SUB+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_sub_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 1),
		BPF_STMT(BPF_ALU+BPF_SUB+BPF_K, 2),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_mul_k);
ATF_TC_HEAD(bpfjit_alu_mul_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_MUL+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_mul_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0xfffffffd));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div0_k);
ATF_TC_HEAD(bpfjit_alu_div0_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=0");
}

ATF_TC_BODY(bpfjit_alu_div0_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	//ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div1_k);
ATF_TC_HEAD(bpfjit_alu_div1_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=1");
}

ATF_TC_BODY(bpfjit_alu_div1_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 7),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 1),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 7);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div2_k);
ATF_TC_HEAD(bpfjit_alu_div2_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=2");
}

ATF_TC_BODY(bpfjit_alu_div2_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 7),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 2),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 3);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div4_k);
ATF_TC_HEAD(bpfjit_alu_div4_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=4");
}

ATF_TC_BODY(bpfjit_alu_div4_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0x3fffffff));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div10_k);
ATF_TC_HEAD(bpfjit_alu_div10_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=10");
}

ATF_TC_BODY(bpfjit_alu_div10_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294843849)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 10),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(429484384));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div10000_k);
ATF_TC_HEAD(bpfjit_alu_div10000_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=10000");
}

ATF_TC_BODY(bpfjit_alu_div10000_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294843849)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, 10000),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(429484));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div7609801_k);
ATF_TC_HEAD(bpfjit_alu_div7609801_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=7609801");
}

ATF_TC_BODY(bpfjit_alu_div7609801_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294967295)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, UINT32_C(7609801)),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 564);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div80000000_k);
ATF_TC_HEAD(bpfjit_alu_div80000000_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_K with k=0x80000000");
}

ATF_TC_BODY(bpfjit_alu_div80000000_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0xffffffde)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, UINT32_C(0x80000000)),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_and_k);
ATF_TC_HEAD(bpfjit_alu_and_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_AND+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_and_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdead),
		BPF_STMT(BPF_ALU+BPF_AND+BPF_K, 0xbeef),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == (0xdead&0xbeef));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_or_k);
ATF_TC_HEAD(bpfjit_alu_or_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_OR+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_or_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdead0000),
		BPF_STMT(BPF_ALU+BPF_OR+BPF_K, 0x0000beef),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_lsh_k);
ATF_TC_HEAD(bpfjit_alu_lsh_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_LSH+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_lsh_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_K, 16),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xbeef0000);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_lsh0_k);
ATF_TC_HEAD(bpfjit_alu_lsh0_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_LSH+BPF_K with k=0");
}

ATF_TC_BODY(bpfjit_alu_lsh0_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_K, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_rsh_k);
ATF_TC_HEAD(bpfjit_alu_rsh_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_RSH+BPF_K");
}

ATF_TC_BODY(bpfjit_alu_rsh_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_K, 16),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0x0000dead);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_rsh0_k);
ATF_TC_HEAD(bpfjit_alu_rsh0_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_RSH+BPF_K with k=0");
}

ATF_TC_BODY(bpfjit_alu_rsh0_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_K, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_modulo_k);
ATF_TC_HEAD(bpfjit_alu_modulo_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of modulo logic of BPF_ALU+BPF_K operations");
}

ATF_TC_BODY(bpfjit_alu_modulo_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0x7fffff77)),

		/* (7FFFFF77 * 0FFFFF77) = 07FFFFB2,F0004951 */
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_K, UINT32_C(0x0fffff77)),

		/* 07FFFFB2,F0004951 << 1 = 0FFFFF65,E00092A2 */
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_K, 1),

		/* 0FFFFF65,E00092A2 + DDDDDDDD = 0FFFFF66,BDDE707F */
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, UINT32_C(0xdddddddd)),

		/* 0FFFFF66,BDDE707F - FFFFFFFF = 0FFFFF65,BDDE7080 */
		BPF_STMT(BPF_ALU+BPF_SUB+BPF_K, UINT32_C(0xffffffff)),

		/* 0FFFFF65,BDDE7080 | 0000030C = 0FFFFF65,BDDE738C */
		BPF_STMT(BPF_ALU+BPF_OR+BPF_K, UINT32_C(0x0000030c)),

		/* -0FFFFF65,BDDE738C mod(2^64) = F000009A,42218C74 */
		BPF_STMT(BPF_ALU+BPF_NEG, 0),

		/* F000009A,42218C74 & FFFFFF0F = F000009A,42218C04 */
		BPF_STMT(BPF_ALU+BPF_AND+BPF_K, UINT32_C(0xffffff0f)),

		/* F000009A,42218C74 >> 3 = 1E000013,48443180 */
		/* 00000000,42218C74 >> 3 = 00000000,08443180 */
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_K, 3),

		/* 00000000,08443180 * 7FFFFF77 = 042218BB,93818280 */
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_K, UINT32_C(0x7fffff77)),

		/* 042218BB,93818280 / DEAD = 000004C0,71CBBBC3 */
		/* 00000000,93818280 / DEAD = 00000000,0000A994 */
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_K, UINT32_C(0xdead)),

		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) != UINT32_C(0x71cbbbc3));
	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0x0000a994));


	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_add_x);
ATF_TC_HEAD(bpfjit_alu_add_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_ADD+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_add_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 3),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 5);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_sub_x);
ATF_TC_HEAD(bpfjit_alu_sub_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_SUB+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_sub_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_STMT(BPF_ALU+BPF_SUB+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_mul_x);
ATF_TC_HEAD(bpfjit_alu_mul_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_MUL+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_mul_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0xfffffffd));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div0_x);
ATF_TC_HEAD(bpfjit_alu_div0_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=0");
}

ATF_TC_BODY(bpfjit_alu_div0_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div1_x);
ATF_TC_HEAD(bpfjit_alu_div1_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=1");
}

ATF_TC_BODY(bpfjit_alu_div1_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 7),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 1),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 7);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div2_x);
ATF_TC_HEAD(bpfjit_alu_div2_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=2");
}

ATF_TC_BODY(bpfjit_alu_div2_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 7),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 3);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div4_x);
ATF_TC_HEAD(bpfjit_alu_div4_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=4");
}

ATF_TC_BODY(bpfjit_alu_div4_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 4),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0x3fffffff));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div10_x);
ATF_TC_HEAD(bpfjit_alu_div10_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=10");
}

ATF_TC_BODY(bpfjit_alu_div10_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294843849)),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 10),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(429484384));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div10000_x);
ATF_TC_HEAD(bpfjit_alu_div10000_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=10000");
}

ATF_TC_BODY(bpfjit_alu_div10000_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294843849)),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 10000),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(429484));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div7609801_x);
ATF_TC_HEAD(bpfjit_alu_div7609801_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=7609801");
}

ATF_TC_BODY(bpfjit_alu_div7609801_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(4294967295)),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(7609801)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 564);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_div80000000_x);
ATF_TC_HEAD(bpfjit_alu_div80000000_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_DIV+BPF_X with X=0x80000000");
}

ATF_TC_BODY(bpfjit_alu_div80000000_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_MAX - 33),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0x80000000)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_and_x);
ATF_TC_HEAD(bpfjit_alu_and_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_AND+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_and_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdead),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0xbeef),
		BPF_STMT(BPF_ALU+BPF_AND+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == (0xdead&0xbeef));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_or_x);
ATF_TC_HEAD(bpfjit_alu_or_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_OR+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_or_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdead0000),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0x0000beef),
		BPF_STMT(BPF_ALU+BPF_OR+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_lsh_x);
ATF_TC_HEAD(bpfjit_alu_lsh_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_LSH+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_lsh_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 16),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xbeef0000);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_lsh0_x);
ATF_TC_HEAD(bpfjit_alu_lsh0_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_LSH+BPF_X with k=0");
}

ATF_TC_BODY(bpfjit_alu_lsh0_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_rsh_x);
ATF_TC_HEAD(bpfjit_alu_rsh_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_RSH+BPF_X");
}

ATF_TC_BODY(bpfjit_alu_rsh_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 16),
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0x0000dead);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_rsh0_x);
ATF_TC_HEAD(bpfjit_alu_rsh0_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_RSH+BPF_X with k=0");
}

ATF_TC_BODY(bpfjit_alu_rsh0_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 0xdeadbeef),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0xdeadbeef);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_modulo_x);
ATF_TC_HEAD(bpfjit_alu_modulo_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of modulo logic of BPF_ALU+BPF_X operations");
}

ATF_TC_BODY(bpfjit_alu_modulo_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0x7fffff77)),

		/* (7FFFFF77 * 0FFFFF77) = 07FFFFB2,F0004951 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0x0fffff77)),
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_X, 0),

		/* 07FFFFB2,F0004951 << 1 = 0FFFFF65,E00092A2 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, 1),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_X, 0),

		/* 0FFFFF65,E00092A2 + DDDDDDDD = 0FFFFF66,BDDE707F */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0xdddddddd)),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),

		/* 0FFFFF66,BDDE707F - FFFFFFFF = 0FFFFF65,BDDE7080 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_ALU+BPF_SUB+BPF_X, 0),

		/* 0FFFFF65,BDDE7080 | 0000030C = 0FFFFF65,BDDE738C */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0x0000030c)),
		BPF_STMT(BPF_ALU+BPF_OR+BPF_X, 0),

		/* -0FFFFF65,BDDE738C mod(2^64) = F000009A,42218C74 */
		BPF_STMT(BPF_ALU+BPF_NEG, 0),

		/* F000009A,42218C74 & FFFFFF0F = F000009A,42218C04 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0xffffff0f)),
		BPF_STMT(BPF_ALU+BPF_AND+BPF_X, 0),

		/* F000009A,42218C74 >> 3 = 1E000013,48443180 */
		/* 00000000,42218C74 >> 3 = 00000000,08443180 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, 3),
		BPF_STMT(BPF_ALU+BPF_RSH+BPF_X, 0),

		/* 00000000,08443180 * 7FFFFF77 = 042218BB,93818280 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0x7fffff77)),
		BPF_STMT(BPF_ALU+BPF_MUL+BPF_X, 0),

		/* 042218BB,93818280 / DEAD = 000004C0,71CBBBC3 */
		/* 00000000,93818280 / DEAD = 00000000,0000A994 */
		BPF_STMT(BPF_LDX+BPF_W+BPF_K, UINT32_C(0xdead)),
		BPF_STMT(BPF_ALU+BPF_DIV+BPF_X, 0),

		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) != UINT32_C(0x71cbbbc3));
	ATF_CHECK(code(pkt, 1, 1) == UINT32_C(0x0000a994));


	bpfjit_free_code(code);
}

ATF_TC(bpfjit_alu_neg);
ATF_TC_HEAD(bpfjit_alu_neg, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ALU+BPF_NEG");
}

ATF_TC_BODY(bpfjit_alu_neg, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 777),
		BPF_STMT(BPF_ALU+BPF_NEG, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0u-777u);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_ja);
ATF_TC_HEAD(bpfjit_jmp_ja, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JA");
}

ATF_TC_BODY(bpfjit_jmp_ja, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_JMP+BPF_JA, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_RET+BPF_K, 3),
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jgt_k);
ATF_TC_HEAD(bpfjit_jmp_jgt_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JGT+BPF_K");
}

ATF_TC_BODY(bpfjit_jmp_jgt_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 7, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 2, 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 9, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 4, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 6, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 5, 3, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 7);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jge_k);
ATF_TC_HEAD(bpfjit_jmp_jge_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JGE+BPF_K");
}

ATF_TC_BODY(bpfjit_jmp_jge_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 8, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 3, 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 9, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 5, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 7, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 6, 3, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, 1, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 7);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jeq_k);
ATF_TC_HEAD(bpfjit_jmp_jeq_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JEQ+BPF_K");
}

ATF_TC_BODY(bpfjit_jmp_jeq_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 8, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 3, 1, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 9, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 5, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 7, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 6, 3, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 1, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 7);
	ATF_CHECK(code(pkt, 2, 2) == 7);
	ATF_CHECK(code(pkt, 3, 3) == 1);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jset_k);
ATF_TC_HEAD(bpfjit_jmp_jset_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JSET+BPF_K");
}

ATF_TC_BODY(bpfjit_jmp_jset_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 8, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 4, 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 3, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 2, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 1, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 2, 3, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_K, 7, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 1);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 5);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_modulo_k);
ATF_TC_HEAD(bpfjit_jmp_modulo_k, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of modulo logic of BPF_JMP+BPF_K operations");
}

ATF_TC_BODY(bpfjit_jmp_modulo_k, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0x7fffff77)),
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_K, 4),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, UINT32_C(0xfffff770), 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, UINT32_C(0xfffff770), 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, UINT32_C(0xfffff771), 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, UINT32_C(0xfffff770), 0, 3),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, UINT32_C(0xfffff770), 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, UINT32_C(0xfffff771), 1, 0),
		BPF_STMT(BPF_JMP+BPF_JA, 1),
		BPF_STMT(BPF_RET+BPF_K, 3),

		/* FFFFF770+FFFFF770 = 00000001,FFFFEEE0 */
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, UINT32_C(0xfffff770)),

		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, UINT32_C(0xffffeee0), 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, UINT32_C(0xffffeee0), 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, UINT32_C(0xffffeee1), 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, UINT32_C(0xffffeee0), 0, 3),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_K, UINT32_C(0xffffeee0), 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_K, UINT32_C(0xffffeee1), 1, 0),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 7)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jgt_x);
ATF_TC_HEAD(bpfjit_jmp_jgt_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JGT+BPF_X");
}

ATF_TC_BODY(bpfjit_jmp_jgt_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 7),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 3, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 9),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 4),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 6),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 4, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 7);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jge_x);
ATF_TC_HEAD(bpfjit_jmp_jge_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JGE+BPF_X");
}

ATF_TC_BODY(bpfjit_jmp_jge_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 8),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 3, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 9),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 7),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 6),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 4, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 1),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 7);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jeq_x);
ATF_TC_HEAD(bpfjit_jmp_jeq_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JEQ+BPF_X");
}

ATF_TC_BODY(bpfjit_jmp_jeq_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 8),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 2, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 9),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 7),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 3, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 1, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 7);
	ATF_CHECK(code(pkt, 2, 2) == 7);
	ATF_CHECK(code(pkt, 3, 3) == 1);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 7);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_jset_x);
ATF_TC_HEAD(bpfjit_jmp_jset_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_JMP+BPF_JSET+BPF_X");
}

ATF_TC_BODY(bpfjit_jmp_jset_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 8),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 4),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 3, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 1, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 1),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 2, 3),
		BPF_STMT(BPF_RET+BPF_K, 3),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 2),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 4, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 7),
		BPF_JUMP(BPF_JMP+BPF_JSET+BPF_X, 0, 0, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, 8)
	};

	bpfjit_function_t code;
	uint8_t pkt[8]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 1);
	ATF_CHECK(code(pkt, 3, 3) == 1);
	ATF_CHECK(code(pkt, 4, 4) == 7);
	ATF_CHECK(code(pkt, 5, 5) == 5);
	ATF_CHECK(code(pkt, 6, 6) == 8);
	ATF_CHECK(code(pkt, 7, 7) == 5);
	ATF_CHECK(code(pkt, 8, 8) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_jmp_modulo_x);
ATF_TC_HEAD(bpfjit_jmp_modulo_x, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of modulo logic of BPF_JMP+BPF_X operations");
}

ATF_TC_BODY(bpfjit_jmp_modulo_x, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_C(0x7fffff77)),
		/* FFFFF770 << 4 = FFFFF770 */
		BPF_STMT(BPF_ALU+BPF_LSH+BPF_K, 4),

		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xfffff770)),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 0),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xfffff771)),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 2),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xfffff770)),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 0, 4),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 3, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xfffff771)),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_JMP+BPF_JA, 1),
		BPF_STMT(BPF_RET+BPF_K, 3),

		/* FFFFF770+FFFFF770 = 00000001,FFFFEEE0 */
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, UINT32_C(0xfffff770)),

		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xffffeee0)),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 4),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 5),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xffffeee1)),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, 6),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xffffeee0)),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 0, 4),
		BPF_JUMP(BPF_JMP+BPF_JGT+BPF_X, 0, 3, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_C(0xffffeee1)),
		BPF_JUMP(BPF_JMP+BPF_JGE+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 7)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ld_abs);
ATF_TC_HEAD(bpfjit_ld_abs, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_ABS");
}

ATF_TC_BODY(bpfjit_ld_abs, tc)
{
	static struct bpf_insn insns[3][2] = {
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 5),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 5),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 5),
			BPF_STMT(BPF_RET+BPF_A, 0)
		}
	};

	static size_t lengths[3] = { 1, 2, 4 };
	static unsigned int expected[3] = { 0xde, 0xdead, 0xdeadbeef };

	size_t i, l;
	uint8_t *pkt = deadbeef_at_5;
	size_t pktsize = sizeof(deadbeef_at_5);

	size_t insn_count = sizeof(insns[0]) / sizeof(insns[0][0]);

	for (i = 0; i < 3; i++) {
		bpfjit_function_t code;

		ATF_CHECK(bpf_validate(insns[i], insn_count));

		code = bpfjit_generate_code(insns[i], insn_count);
		ATF_REQUIRE(code != NULL);

		for (l = 0; l < 5 + lengths[i]; l++) {
			ATF_CHECK(code(pkt, l, l) == 0);
			ATF_CHECK(code(pkt, pktsize, l) == 0);
		}

		l = 5 + lengths[i];
		ATF_CHECK(code(pkt, l, l) == expected[i]);
		ATF_CHECK(code(pkt, pktsize, l) == expected[i]);

		l = pktsize;
		ATF_CHECK(code(pkt, l, l) == expected[i]);

		bpfjit_free_code(code);
	}
}

ATF_TC(bpfjit_ld_abs_k_overflow);
ATF_TC_HEAD(bpfjit_ld_abs_k_overflow, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_ABS with overflow in k+4");
}

ATF_TC_BODY(bpfjit_ld_abs_k_overflow, tc)
{
	static struct bpf_insn insns[12][3] = {
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_ABS, UINT32_MAX),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_ABS, UINT32_MAX - 1),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 1),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 2),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 3),
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_H+BPF_ABS, UINT32_MAX),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_H+BPF_ABS, UINT32_MAX - 1),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 1),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 2),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_B+BPF_ABS, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_ABS, UINT32_MAX - 3),
			BPF_STMT(BPF_RET+BPF_K, 1)
		}
	};

	int i;
	uint8_t pkt[8] = { 0 };

	size_t insn_count = sizeof(insns[0]) / sizeof(insns[0][0]);

	for (i = 0; i < 3; i++) {
		bpfjit_function_t code;

		ATF_CHECK(bpf_validate(insns[i], insn_count));

		code = bpfjit_generate_code(insns[i], insn_count);
		ATF_REQUIRE(code != NULL);

		ATF_CHECK(code(pkt, 8, 8) == 0);

		bpfjit_free_code(code);
	}
}

ATF_TC(bpfjit_ld_ind);
ATF_TC_HEAD(bpfjit_ld_ind, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_IND");
}

ATF_TC_BODY(bpfjit_ld_ind, tc)
{
	static struct bpf_insn insns[6][3] = {
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
			BPF_STMT(BPF_LD+BPF_B+BPF_IND, 2),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 2),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 3),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, 2),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
			BPF_STMT(BPF_LD+BPF_B+BPF_IND, 0),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 0),
			BPF_STMT(BPF_RET+BPF_A, 0)
		},
		{
			BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, 0),
			BPF_STMT(BPF_RET+BPF_A, 0)
		}
	};

	static size_t lengths[6] = { 1, 2, 4, 1, 2, 4 };

	static unsigned int expected[6] = {
		0xde, 0xdead, 0xdeadbeef,
		0xde, 0xdead, 0xdeadbeef
	};

	size_t i, l;
	uint8_t *pkt = deadbeef_at_5;
	size_t pktsize = sizeof(deadbeef_at_5);

	size_t insn_count = sizeof(insns[0]) / sizeof(insns[0][0]);

	for (i = 0; i < 3; i++) {
		bpfjit_function_t code;

		ATF_CHECK(bpf_validate(insns[i], insn_count));

		code = bpfjit_generate_code(insns[i], insn_count);
		ATF_REQUIRE(code != NULL);

		for (l = 0; l < 5 + lengths[i]; l++) {
			ATF_CHECK(code(pkt, l, l) == 0);
			ATF_CHECK(code(pkt, pktsize, l) == 0);
		}

		l = 5 + lengths[i];
		ATF_CHECK(code(pkt, l, l) == expected[i]);
		ATF_CHECK(code(pkt, pktsize, l) == expected[i]);

		l = pktsize;
		ATF_CHECK(code(pkt, l, l) == expected[i]);

		bpfjit_free_code(code);
	}
}

ATF_TC(bpfjit_ld_ind_k_overflow);
ATF_TC_HEAD(bpfjit_ld_ind_k_overflow, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_IND with overflow in k+4");
}

ATF_TC_BODY(bpfjit_ld_ind_k_overflow, tc)
{
	static struct bpf_insn insns[12][3] = {
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, UINT32_MAX),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, UINT32_MAX - 1),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 1),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 2),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 3),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, UINT32_MAX),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, UINT32_MAX - 1),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 1),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 2),
			BPF_STMT(BPF_RET+BPF_K, 1)
		},
		{
			BPF_STMT(BPF_LD+BPF_H+BPF_IND, 7),
			BPF_STMT(BPF_LD+BPF_W+BPF_IND, UINT32_MAX - 3),
			BPF_STMT(BPF_RET+BPF_K, 1)
		}
	};

	int i;
	uint8_t pkt[8] = { 0 };

	size_t insn_count = sizeof(insns[0]) / sizeof(insns[0][0]);

	for (i = 0; i < 3; i++) {
		bpfjit_function_t code;

		ATF_CHECK(bpf_validate(insns[i], insn_count));

		code = bpfjit_generate_code(insns[i], insn_count);
		ATF_REQUIRE(code != NULL);

		ATF_CHECK(code(pkt, 8, 8) == 0);

		bpfjit_free_code(code);
	}
}

ATF_TC(bpfjit_ld_ind_x_overflow1);
ATF_TC_HEAD(bpfjit_ld_ind_x_overflow1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_IND with overflow in X+4");
}

ATF_TC_BODY(bpfjit_ld_ind_x_overflow1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_LEN, 0),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_MISC+BPF_TAX, 0),
		BPF_STMT(BPF_LD+BPF_B+BPF_IND, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i <= sizeof(pkt); i++) {
		ATF_CHECK(bpf_filter(insns, pkt, i, i) == 10 * i);
		ATF_CHECK(code(pkt, i, i) == 10 * i);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ld_ind_x_overflow2);
ATF_TC_HEAD(bpfjit_ld_ind_x_overflow2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_IND with overflow in X+4");
}

ATF_TC_BODY(bpfjit_ld_ind_x_overflow2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_LEN, 0),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, UINT32_C(0xffffffff)),
		BPF_STMT(BPF_ST, 3),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 3),
		BPF_STMT(BPF_LD+BPF_B+BPF_IND, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i <= sizeof(pkt); i++) {
		ATF_CHECK(bpf_filter(insns, pkt, i, i) == 10 * i);
		ATF_CHECK(code(pkt, i, i) == 10 * i);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ld_len);
ATF_TC_HEAD(bpfjit_ld_len, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_W+BPF_LEN");
}

ATF_TC_BODY(bpfjit_ld_len, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[32]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < sizeof(pkt); i++)
		ATF_CHECK(code(pkt, i, 1) == i);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ld_imm);
ATF_TC_HEAD(bpfjit_ld_imm, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LD+BPF_IMM");
}

ATF_TC_BODY(bpfjit_ld_imm, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ldx_imm1);
ATF_TC_HEAD(bpfjit_ldx_imm1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LDX+BPF_IMM");
}

ATF_TC_BODY(bpfjit_ldx_imm1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, UINT32_MAX - 5),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX - 5);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ldx_imm2);
ATF_TC_HEAD(bpfjit_ldx_imm2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LDX+BPF_IMM");
}

ATF_TC_BODY(bpfjit_ldx_imm2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 5),
		BPF_STMT(BPF_LD+BPF_IMM, 5),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == UINT32_MAX);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ldx_len1);
ATF_TC_HEAD(bpfjit_ldx_len1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LDX+BPF_LEN");
}

ATF_TC_BODY(bpfjit_ldx_len1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[5]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i < sizeof(pkt); i++) {
		ATF_CHECK(code(pkt, i, 1) == i);
		ATF_CHECK(code(pkt, i + 1, i) == i + 1);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ldx_len2);
ATF_TC_HEAD(bpfjit_ldx_len2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LDX+BPF_LEN");
}

ATF_TC_BODY(bpfjit_ldx_len2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_LD+BPF_IMM, 5),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_X, 0, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, 7),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX)
	};

	bpfjit_function_t code;
	uint8_t pkt[5]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 5, 1) == UINT32_MAX);
	ATF_CHECK(code(pkt, 6, 5) == 7);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_ldx_msh);
ATF_TC_HEAD(bpfjit_ldx_msh, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_LDX+BPF_MSH");
}

ATF_TC_BODY(bpfjit_ldx_msh, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_B+BPF_MSH, 1),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[2] = { 0, 0x7a };

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 2, 2) == 40);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_misc_tax);
ATF_TC_HEAD(bpfjit_misc_tax, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_MISC+BPF_TAX");
}

ATF_TC_BODY(bpfjit_misc_tax, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_IMM, 3),
		BPF_STMT(BPF_MISC+BPF_TAX, 0),
		BPF_STMT(BPF_LD+BPF_B+BPF_IND, 2),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[] = { 0, 11, 22, 33, 44, 55 };

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, sizeof(pkt), sizeof(pkt)) == 55);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_misc_txa);
ATF_TC_HEAD(bpfjit_misc_txa, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_MISC+BPF_TXA");
}

ATF_TC_BODY(bpfjit_misc_txa, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 391),
		BPF_STMT(BPF_MISC+BPF_TXA, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 391);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_st1);
ATF_TC_HEAD(bpfjit_st1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ST");
}

ATF_TC_BODY(bpfjit_st1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_ST, 0),
		BPF_STMT(BPF_LD+BPF_MEM, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[16]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i <= sizeof(pkt); i++)
		ATF_CHECK(code(pkt, i, sizeof(pkt)) == i);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_st2);
ATF_TC_HEAD(bpfjit_st2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ST");
}

ATF_TC_BODY(bpfjit_st2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_ST, BPF_MEMWORDS-1),
		BPF_STMT(BPF_LD+BPF_MEM, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_st3);
ATF_TC_HEAD(bpfjit_st3, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ST");
}

ATF_TC_BODY(bpfjit_st3, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_ST, 0),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, 100),
		BPF_STMT(BPF_ST, BPF_MEMWORDS-1),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, 200),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 301, 2, 0),
		BPF_STMT(BPF_LD+BPF_MEM, BPF_MEMWORDS-1),
		BPF_STMT(BPF_RET+BPF_A, 0),
		BPF_STMT(BPF_LD+BPF_MEM, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[2]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_REQUIRE(BPF_MEMWORDS > 1);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 102);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_st4);
ATF_TC_HEAD(bpfjit_st4, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ST");
}

ATF_TC_BODY(bpfjit_st4, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_ST, 5),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, 100),
		BPF_STMT(BPF_ST, BPF_MEMWORDS-1),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_K, 200),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 301, 2, 0),
		BPF_STMT(BPF_LD+BPF_MEM, BPF_MEMWORDS-1),
		BPF_STMT(BPF_RET+BPF_A, 0),
		BPF_STMT(BPF_LD+BPF_MEM, 5),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[2]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_REQUIRE(BPF_MEMWORDS > 6);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 1);
	ATF_CHECK(code(pkt, 2, 2) == 102);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_st5);
ATF_TC_HEAD(bpfjit_st5, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_ST");
}

ATF_TC_BODY(bpfjit_st5, tc)
{
	struct bpf_insn insns[5*BPF_MEMWORDS+2];
	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	size_t k;
	bpfjit_function_t code;
	uint8_t pkt[BPF_MEMWORDS]; /* the program doesn't read any data */

	memset(insns, 0, sizeof(insns));

	/* for each k do M[k] = k */
	for (k = 0; k < BPF_MEMWORDS; k++) {
		insns[2*k].code   = BPF_LD+BPF_IMM;
		insns[2*k].k      = 3*k;
		insns[2*k+1].code = BPF_ST;
		insns[2*k+1].k    = k;
	}

	/* load wirelen into A */
	insns[2*BPF_MEMWORDS].code = BPF_LD+BPF_W+BPF_LEN;

	/* for each k, if (A == k + 1) return M[k] */
	for (k = 0; k < BPF_MEMWORDS; k++) {
		insns[2*BPF_MEMWORDS+3*k+1].code = BPF_JMP+BPF_JEQ+BPF_K;
		insns[2*BPF_MEMWORDS+3*k+1].k    = k+1;
		insns[2*BPF_MEMWORDS+3*k+1].jt   = 0;
		insns[2*BPF_MEMWORDS+3*k+1].jf   = 2;
		insns[2*BPF_MEMWORDS+3*k+2].code = BPF_LD+BPF_MEM;
		insns[2*BPF_MEMWORDS+3*k+2].k    = k;
		insns[2*BPF_MEMWORDS+3*k+3].code = BPF_RET+BPF_A;
		insns[2*BPF_MEMWORDS+3*k+3].k    = 0;
	}

	insns[5*BPF_MEMWORDS+1].code = BPF_RET+BPF_K;
	insns[5*BPF_MEMWORDS+1].k    = UINT32_MAX;

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (k = 1; k <= sizeof(pkt); k++)
		ATF_CHECK(code(pkt, k, k) == 3*(k-1));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_stx1);
ATF_TC_HEAD(bpfjit_stx1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_STX");
}

ATF_TC_BODY(bpfjit_stx1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_STX, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 0),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[16]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i <= sizeof(pkt); i++)
		ATF_CHECK(code(pkt, i, sizeof(pkt)) == i);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_stx2);
ATF_TC_HEAD(bpfjit_stx2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_STX");
}

ATF_TC_BODY(bpfjit_stx2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_STX, BPF_MEMWORDS-1),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 0),
		BPF_STMT(BPF_MISC+BPF_TXA, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	bpfjit_function_t code;
	uint8_t pkt[1]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	ATF_CHECK(code(pkt, 1, 1) == 0);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_stx3);
ATF_TC_HEAD(bpfjit_stx3, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_STX");
}

ATF_TC_BODY(bpfjit_stx3, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_LEN, 0),
		BPF_STMT(BPF_STX, 5),
		BPF_STMT(BPF_STX, 2),
		BPF_STMT(BPF_STX, 3),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 1),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 2),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 3),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 5),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_LDX+BPF_W+BPF_MEM, 6),
		BPF_STMT(BPF_ALU+BPF_ADD+BPF_X, 0),
		BPF_STMT(BPF_RET+BPF_A, 0)
	};

	size_t i;
	bpfjit_function_t code;
	uint8_t pkt[16]; /* the program doesn't read any data */

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 1; i <= sizeof(pkt); i++)
		ATF_CHECK(code(pkt, i, sizeof(pkt)) == 3 * i);

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_stx4);
ATF_TC_HEAD(bpfjit_stx4, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation of BPF_STX");
}

ATF_TC_BODY(bpfjit_stx4, tc)
{
	struct bpf_insn insns[5*BPF_MEMWORDS+2];
	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	size_t k;
	bpfjit_function_t code;
	uint8_t pkt[BPF_MEMWORDS]; /* the program doesn't read any data */

	memset(insns, 0, sizeof(insns));

	/* for each k do M[k] = k */
	for (k = 0; k < BPF_MEMWORDS; k++) {
		insns[2*k].code   = BPF_LDX+BPF_W+BPF_IMM;
		insns[2*k].k      = 3*k;
		insns[2*k+1].code = BPF_STX;
		insns[2*k+1].k    = k;
	}

	/* load wirelen into A */
	insns[2*BPF_MEMWORDS].code = BPF_LD+BPF_W+BPF_LEN;

	/* for each k, if (A == k + 1) return M[k] */
	for (k = 0; k < BPF_MEMWORDS; k++) {
		insns[2*BPF_MEMWORDS+3*k+1].code = BPF_JMP+BPF_JEQ+BPF_K;
		insns[2*BPF_MEMWORDS+3*k+1].k    = k+1;
		insns[2*BPF_MEMWORDS+3*k+1].jt   = 0;
		insns[2*BPF_MEMWORDS+3*k+1].jf   = 2;
		insns[2*BPF_MEMWORDS+3*k+2].code = BPF_LD+BPF_MEM;
		insns[2*BPF_MEMWORDS+3*k+2].k    = k;
		insns[2*BPF_MEMWORDS+3*k+3].code = BPF_RET+BPF_A;
		insns[2*BPF_MEMWORDS+3*k+3].k    = 0;
	}

	insns[5*BPF_MEMWORDS+1].code = BPF_RET+BPF_K;
	insns[5*BPF_MEMWORDS+1].k    = UINT32_MAX;

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (k = 1; k <= sizeof(pkt); k++)
		ATF_CHECK(code(pkt, k, k) == 3*(k-1));

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_abs_1);
ATF_TC_HEAD(bpfjit_opt_ld_abs_1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_ABS");
}

ATF_TC_BODY(bpfjit_opt_ld_abs_1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 8),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 26),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 3, 4),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 3),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_abs_2);
ATF_TC_HEAD(bpfjit_opt_ld_abs_2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_ABS");
}

ATF_TC_BODY(bpfjit_opt_ld_abs_2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 26),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 3, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 5),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 3),
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_abs_3);
ATF_TC_HEAD(bpfjit_opt_ld_abs_3, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_ABS");
}

ATF_TC_BODY(bpfjit_opt_ld_abs_3, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 26),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 3, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 5),
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 26),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 3),
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_ind_1);
ATF_TC_HEAD(bpfjit_opt_ld_ind_1, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_IND");
}

ATF_TC_BODY(bpfjit_opt_ld_ind_1, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 12),
		BPF_STMT(BPF_LD+BPF_H+BPF_IND, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 8),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 14),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 18),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 3, 4),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 3),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 18),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_ind_2);
ATF_TC_HEAD(bpfjit_opt_ld_ind_2, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_IND");
}

ATF_TC_BODY(bpfjit_opt_ld_ind_2, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 26),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 3, 6),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 5),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 30),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 3),
		BPF_STMT(BPF_LD+BPF_H+BPF_IND, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_ind_3);
ATF_TC_HEAD(bpfjit_opt_ld_ind_3, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_IND");
}

ATF_TC_BODY(bpfjit_opt_ld_ind_3, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 15),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 15),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 11),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 3, 7),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 6),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 11),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 4),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_LD+BPF_H+BPF_IND, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TC(bpfjit_opt_ld_ind_4);
ATF_TC_HEAD(bpfjit_opt_ld_ind_4, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test JIT compilation with length optimization "
	    "applied to BPF_LD+BPF_IND");
}

ATF_TC_BODY(bpfjit_opt_ld_ind_4, tc)
{
	static struct bpf_insn insns[] = {
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 11),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 19),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 2),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 15),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 3, 7),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x8003700f, 0, 6),
		BPF_STMT(BPF_LD+BPF_W+BPF_IND, 15),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x80037023, 0, 4),
		BPF_STMT(BPF_LDX+BPF_W+BPF_IMM, 0),
		BPF_STMT(BPF_LD+BPF_H+BPF_IND, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0x800, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, UINT32_MAX),
		BPF_STMT(BPF_RET+BPF_K, 0),
	};

	size_t i, j;
	bpfjit_function_t code;
	uint8_t pkt[2][34] = {
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x0f,
			0x80, 0x03, 0x70, 0x23
		},
		{
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0x08, 0x00,
			14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
			0x80, 0x03, 0x70, 0x23,
			0x80, 0x03, 0x70, 0x0f
		}
	};

	size_t insn_count = sizeof(insns) / sizeof(insns[0]);

	ATF_CHECK(bpf_validate(insns, insn_count));

	code = bpfjit_generate_code(insns, insn_count);
	ATF_REQUIRE(code != NULL);

	for (i = 0; i < 2; i++) {
		for (j = 1; j < sizeof(pkt[i]); j++)
			ATF_CHECK(code(pkt[i], j, j) == 0);
		ATF_CHECK(code(pkt[i], j, j) == UINT32_MAX);
	}

	bpfjit_free_code(code);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, bpfjit_empty);
	ATF_TP_ADD_TC(tp, bpfjit_alu_add_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_sub_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_mul_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div0_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div1_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div2_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div4_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div10_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div10000_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div7609801_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div80000000_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_and_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_or_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_lsh_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_lsh0_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_rsh_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_rsh0_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_modulo_k);
	ATF_TP_ADD_TC(tp, bpfjit_alu_add_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_sub_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_mul_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div0_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div1_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div2_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div4_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div10_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div10000_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div7609801_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_div80000000_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_and_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_or_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_lsh_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_lsh0_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_rsh_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_rsh0_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_modulo_x);
	ATF_TP_ADD_TC(tp, bpfjit_alu_neg);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_ja);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jgt_k);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jge_k);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jeq_k);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jset_k);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_modulo_k);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jgt_x);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jge_x);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jeq_x);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_jset_x);
	ATF_TP_ADD_TC(tp, bpfjit_jmp_modulo_x);
	ATF_TP_ADD_TC(tp, bpfjit_ld_abs);
	ATF_TP_ADD_TC(tp, bpfjit_ld_abs_k_overflow);
	ATF_TP_ADD_TC(tp, bpfjit_ld_ind);
	ATF_TP_ADD_TC(tp, bpfjit_ld_ind_k_overflow);
	ATF_TP_ADD_TC(tp, bpfjit_ld_ind_x_overflow1);
	ATF_TP_ADD_TC(tp, bpfjit_ld_ind_x_overflow2);
	ATF_TP_ADD_TC(tp, bpfjit_ld_len);
	ATF_TP_ADD_TC(tp, bpfjit_ld_imm);
	ATF_TP_ADD_TC(tp, bpfjit_ldx_imm1);
	ATF_TP_ADD_TC(tp, bpfjit_ldx_imm2);
	ATF_TP_ADD_TC(tp, bpfjit_ldx_len1);
	ATF_TP_ADD_TC(tp, bpfjit_ldx_len2);
	ATF_TP_ADD_TC(tp, bpfjit_ldx_msh);
	ATF_TP_ADD_TC(tp, bpfjit_misc_tax);
	ATF_TP_ADD_TC(tp, bpfjit_misc_txa);
	ATF_TP_ADD_TC(tp, bpfjit_st1);
	ATF_TP_ADD_TC(tp, bpfjit_st2);
	ATF_TP_ADD_TC(tp, bpfjit_st3);
	ATF_TP_ADD_TC(tp, bpfjit_st4);
	ATF_TP_ADD_TC(tp, bpfjit_st5);
	ATF_TP_ADD_TC(tp, bpfjit_stx1);
	ATF_TP_ADD_TC(tp, bpfjit_stx2);
	ATF_TP_ADD_TC(tp, bpfjit_stx3);
	ATF_TP_ADD_TC(tp, bpfjit_stx4);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_abs_1);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_abs_2);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_abs_3);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_ind_1);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_ind_2);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_ind_3);
	ATF_TP_ADD_TC(tp, bpfjit_opt_ld_ind_4);
	/* XXX: bpfjit_opt_ldx_msh */

	return atf_no_error();
}

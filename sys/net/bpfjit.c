/*	$NetBSD: bpfjit.c,v 1.43 2015/02/14 21:32:46 alnsn Exp $	*/

/*-
 * Copyright (c) 2011-2015 Alexander Nasonov.
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
#ifdef _KERNEL
__KERNEL_RCSID(0, "$NetBSD: bpfjit.c,v 1.43 2015/02/14 21:32:46 alnsn Exp $");
#else
__RCSID("$NetBSD: bpfjit.c,v 1.43 2015/02/14 21:32:46 alnsn Exp $");
#endif

#include <sys/types.h>
#include <sys/queue.h>

#ifndef _KERNEL
#include <assert.h>
#define BJ_ASSERT(c) assert(c)
#else
#define BJ_ASSERT(c) KASSERT(c)
#endif

#ifndef _KERNEL
#include <stdlib.h>
#define BJ_ALLOC(sz) malloc(sz)
#define BJ_FREE(p, sz) free(p)
#else
#include <sys/kmem.h>
#define BJ_ALLOC(sz) kmem_alloc(sz, KM_SLEEP)
#define BJ_FREE(p, sz) kmem_free(p, sz)
#endif

#ifndef _KERNEL
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#else
#include <sys/atomic.h>
#include <sys/module.h>
#endif

#define	__BPF_PRIVATE
#include <net/bpf.h>
#include <net/bpfjit.h>
#include <sljitLir.h>

#if !defined(_KERNEL) && defined(SLJIT_VERBOSE) && SLJIT_VERBOSE
#include <stdio.h> /* for stderr */
#endif

/*
 * Arguments of generated bpfjit_func_t.
 * The first argument is reassigned upon entry
 * to a more frequently used buf argument.
 */
#define BJ_CTX_ARG	SLJIT_SAVED_REG1
#define BJ_ARGS		SLJIT_SAVED_REG2

/*
 * Permanent register assignments.
 */
#define BJ_BUF		SLJIT_SAVED_REG1
//#define BJ_ARGS	SLJIT_SAVED_REG2
#define BJ_BUFLEN	SLJIT_SAVED_REG3
#define BJ_AREG		SLJIT_SCRATCH_REG1
#define BJ_TMP1REG	SLJIT_SCRATCH_REG2
#define BJ_TMP2REG	SLJIT_SCRATCH_REG3
#define BJ_XREG		SLJIT_TEMPORARY_EREG1
#define BJ_TMP3REG	SLJIT_TEMPORARY_EREG2

#ifdef _KERNEL
#define MAX_MEMWORDS BPF_MAX_MEMWORDS
#else
#define MAX_MEMWORDS BPF_MEMWORDS
#endif

#define BJ_INIT_NOBITS  ((bpf_memword_init_t)0)
#define BJ_INIT_MBIT(k) BPF_MEMWORD_INIT(k)
#define BJ_INIT_ABIT    BJ_INIT_MBIT(MAX_MEMWORDS)
#define BJ_INIT_XBIT    BJ_INIT_MBIT(MAX_MEMWORDS + 1)

/*
 * Get a number of memwords and external memwords from a bpf_ctx object.
 */
#define GET_EXTWORDS(bc) ((bc) ? (bc)->extwords : 0)
#define GET_MEMWORDS(bc) (GET_EXTWORDS(bc) ? GET_EXTWORDS(bc) : BPF_MEMWORDS)

/*
 * Optimization hints.
 */
typedef unsigned int bpfjit_hint_t;
#define BJ_HINT_ABS  0x01 /* packet read at absolute offset   */
#define BJ_HINT_IND  0x02 /* packet read at variable offset   */
#define BJ_HINT_MSH  0x04 /* BPF_MSH instruction              */
#define BJ_HINT_COP  0x08 /* BPF_COP or BPF_COPX instruction  */
#define BJ_HINT_COPX 0x10 /* BPF_COPX instruction             */
#define BJ_HINT_XREG 0x20 /* BJ_XREG is needed                */
#define BJ_HINT_LDX  0x40 /* BPF_LDX instruction              */
#define BJ_HINT_PKT  (BJ_HINT_ABS|BJ_HINT_IND|BJ_HINT_MSH)

/*
 * Datatype for Array Bounds Check Elimination (ABC) pass.
 */
typedef uint64_t bpfjit_abc_length_t;
#define MAX_ABC_LENGTH (UINT32_MAX + UINT64_C(4)) /* max. width is 4 */

struct bpfjit_stack
{
	bpf_ctx_t *ctx;
	uint32_t *extmem; /* pointer to external memory store */
	uint32_t reg; /* saved A or X register */
#ifdef _KERNEL
	int err; /* 3rd argument for m_xword/m_xhalf/m_xbyte function call */
#endif
	uint32_t mem[BPF_MEMWORDS]; /* internal memory store */
};

/*
 * Data for BPF_JMP instruction.
 * Forward declaration for struct bpfjit_jump.
 */
struct bpfjit_jump_data;

/*
 * Node of bjumps list.
 */
struct bpfjit_jump {
	struct sljit_jump *sjump;
	SLIST_ENTRY(bpfjit_jump) entries;
	struct bpfjit_jump_data *jdata;
};

/*
 * Data for BPF_JMP instruction.
 */
struct bpfjit_jump_data {
	/*
	 * These entries make up bjumps list:
	 * jtf[0] - when coming from jt path,
	 * jtf[1] - when coming from jf path.
	 */
	struct bpfjit_jump jtf[2];
	/*
	 * Length calculated by Array Bounds Check Elimination (ABC) pass.
	 */
	bpfjit_abc_length_t abc_length;
	/*
	 * Length checked by the last out-of-bounds check.
	 */
	bpfjit_abc_length_t checked_length;
};

/*
 * Data for "read from packet" instructions.
 * See also read_pkt_insn() function below.
 */
struct bpfjit_read_pkt_data {
	/*
	 * Length calculated by Array Bounds Check Elimination (ABC) pass.
	 */
	bpfjit_abc_length_t abc_length;
	/*
	 * If positive, emit "if (buflen < check_length) return 0"
	 * out-of-bounds check.
	 * Values greater than UINT32_MAX generate unconditional "return 0".
	 */
	bpfjit_abc_length_t check_length;
};

/*
 * Additional (optimization-related) data for bpf_insn.
 */
struct bpfjit_insn_data {
	/* List of jumps to this insn. */
	SLIST_HEAD(, bpfjit_jump) bjumps;

	union {
		struct bpfjit_jump_data     jdata;
		struct bpfjit_read_pkt_data rdata;
	} u;

	bpf_memword_init_t invalid;
	bool unreachable;
};

#ifdef _KERNEL

uint32_t m_xword(const struct mbuf *, uint32_t, int *);
uint32_t m_xhalf(const struct mbuf *, uint32_t, int *);
uint32_t m_xbyte(const struct mbuf *, uint32_t, int *);

MODULE(MODULE_CLASS_MISC, bpfjit, "sljit")

static int
bpfjit_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		bpfjit_module_ops.bj_free_code = &bpfjit_free_code;
		membar_producer();
		bpfjit_module_ops.bj_generate_code = &bpfjit_generate_code;
		membar_producer();
		return 0;

	case MODULE_CMD_FINI:
		return EOPNOTSUPP;

	default:
		return ENOTTY;
	}
}
#endif

/*
 * Return a number of scratch registers to pass
 * to sljit_emit_enter() function.
 */
static sljit_si
nscratches(bpfjit_hint_t hints)
{
	sljit_si rv = 2;

#ifdef _KERNEL
	if (hints & BJ_HINT_PKT)
		rv = 3; /* xcall with three arguments */
#endif

	if (hints & BJ_HINT_IND)
		rv = 3; /* uses BJ_TMP2REG */

	if (hints & BJ_HINT_COP)
		rv = 3; /* calls copfunc with three arguments */

	if (hints & BJ_HINT_XREG)
		rv = 4; /* uses BJ_XREG */

#ifdef _KERNEL
	if (hints & BJ_HINT_LDX)
		rv = 5; /* uses BJ_TMP3REG */
#endif

	if (hints & BJ_HINT_COPX)
		rv = 5; /* uses BJ_TMP3REG */

	return rv;
}

/*
 * Return a number of saved registers to pass
 * to sljit_emit_enter() function.
 */
static sljit_si
nsaveds(bpfjit_hint_t hints)
{
	sljit_si rv = 3;

	return rv;
}

static uint32_t
read_width(const struct bpf_insn *pc)
{

	switch (BPF_SIZE(pc->code)) {
	case BPF_W: return 4;
	case BPF_H: return 2;
	case BPF_B: return 1;
	default:    return 0;
	}
}

/*
 * Copy buf and buflen members of bpf_args from BJ_ARGS
 * pointer to BJ_BUF and BJ_BUFLEN registers.
 */
static int
load_buf_buflen(struct sljit_compiler *compiler)
{
	int status;

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_P,
	    BJ_BUF, 0,
	    SLJIT_MEM1(BJ_ARGS),
	    offsetof(struct bpf_args, pkt));
	if (status != SLJIT_SUCCESS)
		return status;

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV, /* size_t source */
	    BJ_BUFLEN, 0,
	    SLJIT_MEM1(BJ_ARGS),
	    offsetof(struct bpf_args, buflen));

	return status;
}

static bool
grow_jumps(struct sljit_jump ***jumps, size_t *size)
{
	struct sljit_jump **newptr;
	const size_t elemsz = sizeof(struct sljit_jump *);
	size_t old_size = *size;
	size_t new_size = 2 * old_size;

	if (new_size < old_size || new_size > SIZE_MAX / elemsz)
		return false;

	newptr = BJ_ALLOC(new_size * elemsz);
	if (newptr == NULL)
		return false;

	memcpy(newptr, *jumps, old_size * elemsz);
	BJ_FREE(*jumps, old_size * elemsz);

	*jumps = newptr;
	*size = new_size;
	return true;
}

static bool
append_jump(struct sljit_jump *jump, struct sljit_jump ***jumps,
    size_t *size, size_t *max_size)
{
	if (*size == *max_size && !grow_jumps(jumps, max_size))
		return false;

	(*jumps)[(*size)++] = jump;
	return true;
}

/*
 * Emit code for BPF_LD+BPF_B+BPF_ABS    A <- P[k:1].
 */
static int
emit_read8(struct sljit_compiler *compiler, sljit_si src, uint32_t k)
{

	return sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_AREG, 0,
	    SLJIT_MEM1(src), k);
}

/*
 * Emit code for BPF_LD+BPF_H+BPF_ABS    A <- P[k:2].
 */
static int
emit_read16(struct sljit_compiler *compiler, sljit_si src, uint32_t k)
{
	int status;

	BJ_ASSERT(k <= UINT32_MAX - 1);

	/* A = buf[k]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_AREG, 0,
	    SLJIT_MEM1(src), k);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = buf[k+1]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_TMP1REG, 0,
	    SLJIT_MEM1(src), k+1);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    BJ_TMP1REG, 0);
	return status;
}

/*
 * Emit code for BPF_LD+BPF_W+BPF_ABS    A <- P[k:4].
 */
static int
emit_read32(struct sljit_compiler *compiler, sljit_si src, uint32_t k)
{
	int status;

	BJ_ASSERT(k <= UINT32_MAX - 3);

	/* A = buf[k]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_AREG, 0,
	    SLJIT_MEM1(src), k);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = buf[k+1]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_TMP1REG, 0,
	    SLJIT_MEM1(src), k+1);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    BJ_TMP1REG, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = buf[k+2]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_TMP1REG, 0,
	    SLJIT_MEM1(src), k+2);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    BJ_TMP1REG, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	/* tmp1 = buf[k+3]; */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_TMP1REG, 0,
	    SLJIT_MEM1(src), k+3);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A << 8; */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    SLJIT_IMM, 8);
	if (status != SLJIT_SUCCESS)
		return status;

	/* A = A + tmp1; */
	status = sljit_emit_op2(compiler,
	    SLJIT_ADD,
	    BJ_AREG, 0,
	    BJ_AREG, 0,
	    BJ_TMP1REG, 0);
	return status;
}

#ifdef _KERNEL
/*
 * Emit code for m_xword/m_xhalf/m_xbyte call.
 *
 * @pc BPF_LD+BPF_W+BPF_ABS    A <- P[k:4]
 *     BPF_LD+BPF_H+BPF_ABS    A <- P[k:2]
 *     BPF_LD+BPF_B+BPF_ABS    A <- P[k:1]
 *     BPF_LD+BPF_W+BPF_IND    A <- P[X+k:4]
 *     BPF_LD+BPF_H+BPF_IND    A <- P[X+k:2]
 *     BPF_LD+BPF_B+BPF_IND    A <- P[X+k:1]
 *     BPF_LDX+BPF_B+BPF_MSH   X <- 4*(P[k:1]&0xf)
 */
static int
emit_xcall(struct sljit_compiler *compiler, bpfjit_hint_t hints,
    const struct bpf_insn *pc, int dst, struct sljit_jump ***ret0,
    size_t *ret0_size, size_t *ret0_maxsize,
    uint32_t (*fn)(const struct mbuf *, uint32_t, int *))
{
#if BJ_XREG == SLJIT_RETURN_REG   || \
    BJ_XREG == SLJIT_SCRATCH_REG1 || \
    BJ_XREG == SLJIT_SCRATCH_REG2 || \
    BJ_XREG == SLJIT_SCRATCH_REG3
#error "Not supported assignment of registers."
#endif
	struct sljit_jump *jump;
	sljit_si save_reg;
	int status;

	save_reg = (BPF_CLASS(pc->code) == BPF_LDX) ? BJ_AREG : BJ_XREG;

	if (save_reg == BJ_AREG || (hints & BJ_HINT_XREG)) {
		/* save A or X */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_UI, /* uint32_t destination */
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, reg),
		    save_reg, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/*
	 * Prepare registers for fn(mbuf, k, &err) call.
	 */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_SCRATCH_REG1, 0,
	    BJ_BUF, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	if (BPF_CLASS(pc->code) == BPF_LD && BPF_MODE(pc->code) == BPF_IND) {
		if (pc->k == 0) {
			/* k = X; */
			status = sljit_emit_op1(compiler,
			    SLJIT_MOV,
			    SLJIT_SCRATCH_REG2, 0,
			    BJ_XREG, 0);
			if (status != SLJIT_SUCCESS)
				return status;
		} else {
			/* if (X > UINT32_MAX - pc->k) return 0; */
			jump = sljit_emit_cmp(compiler,
			    SLJIT_C_GREATER,
			    BJ_XREG, 0,
			    SLJIT_IMM, UINT32_MAX - pc->k);
			if (jump == NULL)
				return SLJIT_ERR_ALLOC_FAILED;
			if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
				return SLJIT_ERR_ALLOC_FAILED;

			/* k = X + pc->k; */
			status = sljit_emit_op2(compiler,
			    SLJIT_ADD,
			    SLJIT_SCRATCH_REG2, 0,
			    BJ_XREG, 0,
			    SLJIT_IMM, (uint32_t)pc->k);
			if (status != SLJIT_SUCCESS)
				return status;
		}
	} else {
		/* k = pc->k */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    SLJIT_SCRATCH_REG2, 0,
		    SLJIT_IMM, (uint32_t)pc->k);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/*
	 * The third argument of fn is an address on stack.
	 */
	status = sljit_get_local_base(compiler,
	    SLJIT_SCRATCH_REG3, 0,
	    offsetof(struct bpfjit_stack, err));
	if (status != SLJIT_SUCCESS)
		return status;

	/* fn(buf, k, &err); */
	status = sljit_emit_ijump(compiler,
	    SLJIT_CALL3,
	    SLJIT_IMM, SLJIT_FUNC_OFFSET(fn));
	if (status != SLJIT_SUCCESS)
		return status;

	if (dst != SLJIT_RETURN_REG) {
		/* move return value to dst */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    dst, 0,
		    SLJIT_RETURN_REG, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/* if (*err != 0) return 0; */
	jump = sljit_emit_cmp(compiler,
	    SLJIT_C_NOT_EQUAL|SLJIT_INT_OP,
	    SLJIT_MEM1(SLJIT_LOCALS_REG),
	    offsetof(struct bpfjit_stack, err),
	    SLJIT_IMM, 0);
	if (jump == NULL)
		return SLJIT_ERR_ALLOC_FAILED;

	if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
		return SLJIT_ERR_ALLOC_FAILED;

	if (save_reg == BJ_AREG || (hints & BJ_HINT_XREG)) {
		/* restore A or X */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_UI, /* uint32_t source */
		    save_reg, 0,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, reg));
		if (status != SLJIT_SUCCESS)
			return status;
	}

	return SLJIT_SUCCESS;
}
#endif

/*
 * Emit code for BPF_COP and BPF_COPX instructions.
 */
static int
emit_cop(struct sljit_compiler *compiler, bpfjit_hint_t hints,
    const bpf_ctx_t *bc, const struct bpf_insn *pc,
    struct sljit_jump ***ret0, size_t *ret0_size, size_t *ret0_maxsize)
{
#if BJ_XREG    == SLJIT_RETURN_REG   || \
    BJ_XREG    == SLJIT_SCRATCH_REG1 || \
    BJ_XREG    == SLJIT_SCRATCH_REG2 || \
    BJ_XREG    == SLJIT_SCRATCH_REG3 || \
    BJ_TMP3REG == SLJIT_SCRATCH_REG1 || \
    BJ_TMP3REG == SLJIT_SCRATCH_REG2 || \
    BJ_TMP3REG == SLJIT_SCRATCH_REG3
#error "Not supported assignment of registers."
#endif

	struct sljit_jump *jump;
	sljit_si call_reg;
	sljit_sw call_off;
	int status;

	BJ_ASSERT(bc != NULL && bc->copfuncs != NULL);

	if (hints & BJ_HINT_LDX) {
		/* save X */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_UI, /* uint32_t destination */
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, reg),
		    BJ_XREG, 0);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	if (BPF_MISCOP(pc->code) == BPF_COP) {
		call_reg = SLJIT_IMM;
		call_off = SLJIT_FUNC_OFFSET(bc->copfuncs[pc->k]);
	} else {
		/* if (X >= bc->nfuncs) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_GREATER_EQUAL,
		    BJ_XREG, 0,
		    SLJIT_IMM, bc->nfuncs);
		if (jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
		if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
			return SLJIT_ERR_ALLOC_FAILED;

		/* tmp1 = ctx; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    BJ_TMP1REG, 0,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, ctx));
		if (status != SLJIT_SUCCESS)
			return status;

		/* tmp1 = ctx->copfuncs; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    BJ_TMP1REG, 0,
		    SLJIT_MEM1(BJ_TMP1REG),
		    offsetof(struct bpf_ctx, copfuncs));
		if (status != SLJIT_SUCCESS)
			return status;

		/* tmp2 = X; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BJ_TMP2REG, 0,
		    BJ_XREG, 0);
		if (status != SLJIT_SUCCESS)
			return status;

		/* tmp3 = ctx->copfuncs[tmp2]; */
		call_reg = BJ_TMP3REG;
		call_off = 0;
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    call_reg, call_off,
		    SLJIT_MEM2(BJ_TMP1REG, BJ_TMP2REG),
		    SLJIT_WORD_SHIFT);
		if (status != SLJIT_SUCCESS)
			return status;
	}

	/*
	 * Copy bpf_copfunc_t arguments to registers.
	 */
#if BJ_AREG != SLJIT_SCRATCH_REG3
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UI,
	    SLJIT_SCRATCH_REG3, 0,
	    BJ_AREG, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_P,
	    SLJIT_SCRATCH_REG1, 0,
	    SLJIT_MEM1(SLJIT_LOCALS_REG),
	    offsetof(struct bpfjit_stack, ctx));
	if (status != SLJIT_SUCCESS)
		return status;

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_P,
	    SLJIT_SCRATCH_REG2, 0,
	    BJ_ARGS, 0);
	if (status != SLJIT_SUCCESS)
		return status;

	status = sljit_emit_ijump(compiler,
	    SLJIT_CALL3, call_reg, call_off);
	if (status != SLJIT_SUCCESS)
		return status;

#if BJ_AREG != SLJIT_RETURN_REG
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    BJ_AREG, 0,
	    SLJIT_RETURN_REG, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif

	if (hints & BJ_HINT_LDX) {
		/* restore X */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_UI, /* uint32_t source */
		    BJ_XREG, 0,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, reg));
		if (status != SLJIT_SUCCESS)
			return status;
	}

	return SLJIT_SUCCESS;
}

/*
 * Generate code for
 * BPF_LD+BPF_W+BPF_ABS    A <- P[k:4]
 * BPF_LD+BPF_H+BPF_ABS    A <- P[k:2]
 * BPF_LD+BPF_B+BPF_ABS    A <- P[k:1]
 * BPF_LD+BPF_W+BPF_IND    A <- P[X+k:4]
 * BPF_LD+BPF_H+BPF_IND    A <- P[X+k:2]
 * BPF_LD+BPF_B+BPF_IND    A <- P[X+k:1]
 */
static int
emit_pkt_read(struct sljit_compiler *compiler, bpfjit_hint_t hints,
    const struct bpf_insn *pc, struct sljit_jump *to_mchain_jump,
    struct sljit_jump ***ret0, size_t *ret0_size, size_t *ret0_maxsize)
{
	int status = SLJIT_ERR_ALLOC_FAILED;
	uint32_t width;
	sljit_si ld_reg;
	struct sljit_jump *jump;
#ifdef _KERNEL
	struct sljit_label *label;
	struct sljit_jump *over_mchain_jump;
	const bool check_zero_buflen = (to_mchain_jump != NULL);
#endif
	const uint32_t k = pc->k;

#ifdef _KERNEL
	if (to_mchain_jump == NULL) {
		to_mchain_jump = sljit_emit_cmp(compiler,
		    SLJIT_C_EQUAL,
		    BJ_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (to_mchain_jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
	}
#endif

	ld_reg = BJ_BUF;
	width = read_width(pc);
	if (width == 0)
		return SLJIT_ERR_ALLOC_FAILED;

	if (BPF_MODE(pc->code) == BPF_IND) {
		/* tmp1 = buflen - (pc->k + width); */
		status = sljit_emit_op2(compiler,
		    SLJIT_SUB,
		    BJ_TMP1REG, 0,
		    BJ_BUFLEN, 0,
		    SLJIT_IMM, k + width);
		if (status != SLJIT_SUCCESS)
			return status;

		/* ld_reg = buf + X; */
		ld_reg = BJ_TMP2REG;
		status = sljit_emit_op2(compiler,
		    SLJIT_ADD,
		    ld_reg, 0,
		    BJ_BUF, 0,
		    BJ_XREG, 0);
		if (status != SLJIT_SUCCESS)
			return status;

		/* if (tmp1 < X) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_LESS,
		    BJ_TMP1REG, 0,
		    BJ_XREG, 0);
		if (jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
		if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
			return SLJIT_ERR_ALLOC_FAILED;
	}

	/*
	 * Don't emit wrapped-around reads. They're dead code but
	 * dead code elimination logic isn't smart enough to figure
	 * it out.
	 */
	if (k <= UINT32_MAX - width + 1) {
		switch (width) {
		case 4:
			status = emit_read32(compiler, ld_reg, k);
			break;
		case 2:
			status = emit_read16(compiler, ld_reg, k);
			break;
		case 1:
			status = emit_read8(compiler, ld_reg, k);
			break;
		}

		if (status != SLJIT_SUCCESS)
			return status;
	}

#ifdef _KERNEL
	over_mchain_jump = sljit_emit_jump(compiler, SLJIT_JUMP);
	if (over_mchain_jump == NULL)
		return SLJIT_ERR_ALLOC_FAILED;

	/* entry point to mchain handler */
	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(to_mchain_jump, label);

	if (check_zero_buflen) {
		/* if (buflen != 0) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_NOT_EQUAL,
		    BJ_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
		if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
			return SLJIT_ERR_ALLOC_FAILED;
	}

	switch (width) {
	case 4:
		status = emit_xcall(compiler, hints, pc, BJ_AREG,
		    ret0, ret0_size, ret0_maxsize, &m_xword);
		break;
	case 2:
		status = emit_xcall(compiler, hints, pc, BJ_AREG,
		    ret0, ret0_size, ret0_maxsize, &m_xhalf);
		break;
	case 1:
		status = emit_xcall(compiler, hints, pc, BJ_AREG,
		    ret0, ret0_size, ret0_maxsize, &m_xbyte);
		break;
	}

	if (status != SLJIT_SUCCESS)
		return status;

	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(over_mchain_jump, label);
#endif

	return SLJIT_SUCCESS;
}

static int
emit_memload(struct sljit_compiler *compiler,
    sljit_si dst, uint32_t k, size_t extwords)
{
	int status;
	sljit_si src;
	sljit_sw srcw;

	srcw = k * sizeof(uint32_t);

	if (extwords == 0) {
		src = SLJIT_MEM1(SLJIT_LOCALS_REG);
		srcw += offsetof(struct bpfjit_stack, mem);
	} else {
		/* copy extmem pointer to the tmp1 register */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    BJ_TMP1REG, 0,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, extmem));
		if (status != SLJIT_SUCCESS)
			return status;
		src = SLJIT_MEM1(BJ_TMP1REG);
	}

	return sljit_emit_op1(compiler, SLJIT_MOV_UI, dst, 0, src, srcw);
}

static int
emit_memstore(struct sljit_compiler *compiler,
    sljit_si src, uint32_t k, size_t extwords)
{
	int status;
	sljit_si dst;
	sljit_sw dstw;

	dstw = k * sizeof(uint32_t);

	if (extwords == 0) {
		dst = SLJIT_MEM1(SLJIT_LOCALS_REG);
		dstw += offsetof(struct bpfjit_stack, mem);
	} else {
		/* copy extmem pointer to the tmp1 register */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    BJ_TMP1REG, 0,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, extmem));
		if (status != SLJIT_SUCCESS)
			return status;
		dst = SLJIT_MEM1(BJ_TMP1REG);
	}

	return sljit_emit_op1(compiler, SLJIT_MOV_UI, dst, dstw, src, 0);
}

/*
 * Emit code for BPF_LDX+BPF_B+BPF_MSH    X <- 4*(P[k:1]&0xf).
 */
static int
emit_msh(struct sljit_compiler *compiler, bpfjit_hint_t hints,
    const struct bpf_insn *pc, struct sljit_jump *to_mchain_jump,
    struct sljit_jump ***ret0, size_t *ret0_size, size_t *ret0_maxsize)
{
	int status;
#ifdef _KERNEL
	struct sljit_label *label;
	struct sljit_jump *jump, *over_mchain_jump;
	const bool check_zero_buflen = (to_mchain_jump != NULL);
#endif
	const uint32_t k = pc->k;

#ifdef _KERNEL
	if (to_mchain_jump == NULL) {
		to_mchain_jump = sljit_emit_cmp(compiler,
		    SLJIT_C_EQUAL,
		    BJ_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (to_mchain_jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
	}
#endif

	/* tmp1 = buf[k] */
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV_UB,
	    BJ_TMP1REG, 0,
	    SLJIT_MEM1(BJ_BUF), k);
	if (status != SLJIT_SUCCESS)
		return status;

#ifdef _KERNEL
	over_mchain_jump = sljit_emit_jump(compiler, SLJIT_JUMP);
	if (over_mchain_jump == NULL)
		return SLJIT_ERR_ALLOC_FAILED;

	/* entry point to mchain handler */
	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(to_mchain_jump, label);

	if (check_zero_buflen) {
		/* if (buflen != 0) return 0; */
		jump = sljit_emit_cmp(compiler,
		    SLJIT_C_NOT_EQUAL,
		    BJ_BUFLEN, 0,
		    SLJIT_IMM, 0);
		if (jump == NULL)
			return SLJIT_ERR_ALLOC_FAILED;
		if (!append_jump(jump, ret0, ret0_size, ret0_maxsize))
			return SLJIT_ERR_ALLOC_FAILED;
	}

	status = emit_xcall(compiler, hints, pc, BJ_TMP1REG,
	    ret0, ret0_size, ret0_maxsize, &m_xbyte);
	if (status != SLJIT_SUCCESS)
		return status;

	label = sljit_emit_label(compiler);
	if (label == NULL)
		return SLJIT_ERR_ALLOC_FAILED;
	sljit_set_label(over_mchain_jump, label);
#endif

	/* tmp1 &= 0xf */
	status = sljit_emit_op2(compiler,
	    SLJIT_AND,
	    BJ_TMP1REG, 0,
	    BJ_TMP1REG, 0,
	    SLJIT_IMM, 0xf);
	if (status != SLJIT_SUCCESS)
		return status;

	/* X = tmp1 << 2 */
	status = sljit_emit_op2(compiler,
	    SLJIT_SHL,
	    BJ_XREG, 0,
	    BJ_TMP1REG, 0,
	    SLJIT_IMM, 2);
	if (status != SLJIT_SUCCESS)
		return status;

	return SLJIT_SUCCESS;
}

/*
 * Emit code for A = A / k or A = A % k when k is a power of 2.
 * @pc BPF_DIV or BPF_MOD instruction.
 */
static int
emit_pow2_moddiv(struct sljit_compiler *compiler, const struct bpf_insn *pc)
{
	uint32_t k = pc->k;
	int status = SLJIT_SUCCESS;

	BJ_ASSERT(k != 0 && (k & (k - 1)) == 0);

	if (BPF_OP(pc->code) == BPF_MOD) {
		status = sljit_emit_op2(compiler,
		    SLJIT_AND,
		    BJ_AREG, 0,
		    BJ_AREG, 0,
		    SLJIT_IMM, k - 1);
	} else {
		int shift = 0;

		/*
		 * Do shift = __builtin_ctz(k).
		 * The loop is slower, but that's ok.
		 */
		while (k > 1) {
			k >>= 1;
			shift++;
		}

		if (shift != 0) {
			status = sljit_emit_op2(compiler,
			    SLJIT_LSHR|SLJIT_INT_OP,
			    BJ_AREG, 0,
			    BJ_AREG, 0,
			    SLJIT_IMM, shift);
		}
	}

	return status;
}

#if !defined(BPFJIT_USE_UDIV)
static sljit_uw
divide(sljit_uw x, sljit_uw y)
{

	return (uint32_t)x / (uint32_t)y;
}

static sljit_uw
modulus(sljit_uw x, sljit_uw y)
{

	return (uint32_t)x % (uint32_t)y;
}
#endif

/*
 * Emit code for A = A / div or A = A % div.
 * @pc BPF_DIV or BPF_MOD instruction.
 */
static int
emit_moddiv(struct sljit_compiler *compiler, const struct bpf_insn *pc)
{
	int status;
	const bool xdiv = BPF_OP(pc->code) == BPF_DIV;
	const bool xreg = BPF_SRC(pc->code) == BPF_X;

#if BJ_XREG == SLJIT_RETURN_REG   || \
    BJ_XREG == SLJIT_SCRATCH_REG1 || \
    BJ_XREG == SLJIT_SCRATCH_REG2 || \
    BJ_AREG == SLJIT_SCRATCH_REG2
#error "Not supported assignment of registers."
#endif

#if BJ_AREG != SLJIT_SCRATCH_REG1
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_SCRATCH_REG1, 0,
	    BJ_AREG, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif

	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    SLJIT_SCRATCH_REG2, 0,
	    xreg ? BJ_XREG : SLJIT_IMM,
	    xreg ? 0 : (uint32_t)pc->k);
	if (status != SLJIT_SUCCESS)
		return status;

#if defined(BPFJIT_USE_UDIV)
	status = sljit_emit_op0(compiler, SLJIT_UDIV|SLJIT_INT_OP);

	if (BPF_OP(pc->code) == BPF_DIV) {
#if BJ_AREG != SLJIT_SCRATCH_REG1
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BJ_AREG, 0,
		    SLJIT_SCRATCH_REG1, 0);
#endif
	} else {
#if BJ_AREG != SLJIT_SCRATCH_REG2
		/* Remainder is in SLJIT_SCRATCH_REG2. */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BJ_AREG, 0,
		    SLJIT_SCRATCH_REG2, 0);
#endif
	}

	if (status != SLJIT_SUCCESS)
		return status;
#else
	status = sljit_emit_ijump(compiler,
	    SLJIT_CALL2,
	    SLJIT_IMM, xdiv ? SLJIT_FUNC_OFFSET(divide) :
		SLJIT_FUNC_OFFSET(modulus));

#if BJ_AREG != SLJIT_RETURN_REG
	status = sljit_emit_op1(compiler,
	    SLJIT_MOV,
	    BJ_AREG, 0,
	    SLJIT_RETURN_REG, 0);
	if (status != SLJIT_SUCCESS)
		return status;
#endif
#endif

	return status;
}

/*
 * Return true if pc is a "read from packet" instruction.
 * If length is not NULL and return value is true, *length will
 * be set to a safe length required to read a packet.
 */
static bool
read_pkt_insn(const struct bpf_insn *pc, bpfjit_abc_length_t *length)
{
	bool rv;
	bpfjit_abc_length_t width = 0; /* XXXuninit */

	switch (BPF_CLASS(pc->code)) {
	default:
		rv = false;
		break;

	case BPF_LD:
		rv = BPF_MODE(pc->code) == BPF_ABS ||
		     BPF_MODE(pc->code) == BPF_IND;
		if (rv) {
			width = read_width(pc);
			rv = (width != 0);
		}
		break;

	case BPF_LDX:
		rv = BPF_MODE(pc->code) == BPF_MSH &&
		     BPF_SIZE(pc->code) == BPF_B;
		width = 1;
		break;
	}

	if (rv && length != NULL) {
		/*
		 * Values greater than UINT32_MAX will generate
		 * unconditional "return 0".
		 */
		*length = (uint32_t)pc->k + width;
	}

	return rv;
}

static void
optimize_init(struct bpfjit_insn_data *insn_dat, size_t insn_count)
{
	size_t i;

	for (i = 0; i < insn_count; i++) {
		SLIST_INIT(&insn_dat[i].bjumps);
		insn_dat[i].invalid = BJ_INIT_NOBITS;
	}
}

/*
 * The function divides instructions into blocks. Destination of a jump
 * instruction starts a new block. BPF_RET and BPF_JMP instructions
 * terminate a block. Blocks are linear, that is, there are no jumps out
 * from the middle of a block and there are no jumps in to the middle of
 * a block.
 *
 * The function also sets bits in *initmask for memwords that
 * need to be initialized to zero. Note that this set should be empty
 * for any valid kernel filter program.
 */
static bool
optimize_pass1(const bpf_ctx_t *bc, const struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count,
    bpf_memword_init_t *initmask, bpfjit_hint_t *hints)
{
	struct bpfjit_jump *jtf;
	size_t i;
	uint32_t jt, jf;
	bpfjit_abc_length_t length;
	bpf_memword_init_t invalid; /* borrowed from bpf_filter() */
	bool unreachable;

	const size_t memwords = GET_MEMWORDS(bc);

	*hints = 0;
	*initmask = BJ_INIT_NOBITS;

	unreachable = false;
	invalid = ~BJ_INIT_NOBITS;

	for (i = 0; i < insn_count; i++) {
		if (!SLIST_EMPTY(&insn_dat[i].bjumps))
			unreachable = false;
		insn_dat[i].unreachable = unreachable;

		if (unreachable)
			continue;

		invalid |= insn_dat[i].invalid;

		if (read_pkt_insn(&insns[i], &length) && length > UINT32_MAX)
			unreachable = true;

		switch (BPF_CLASS(insns[i].code)) {
		case BPF_RET:
			if (BPF_RVAL(insns[i].code) == BPF_A)
				*initmask |= invalid & BJ_INIT_ABIT;

			unreachable = true;
			continue;

		case BPF_LD:
			if (BPF_MODE(insns[i].code) == BPF_ABS)
				*hints |= BJ_HINT_ABS;

			if (BPF_MODE(insns[i].code) == BPF_IND) {
				*hints |= BJ_HINT_IND | BJ_HINT_XREG;
				*initmask |= invalid & BJ_INIT_XBIT;
			}

			if (BPF_MODE(insns[i].code) == BPF_MEM &&
			    (uint32_t)insns[i].k < memwords) {
				*initmask |= invalid & BJ_INIT_MBIT(insns[i].k);
			}

			invalid &= ~BJ_INIT_ABIT;
			continue;

		case BPF_LDX:
			*hints |= BJ_HINT_XREG | BJ_HINT_LDX;

			if (BPF_MODE(insns[i].code) == BPF_MEM &&
			    (uint32_t)insns[i].k < memwords) {
				*initmask |= invalid & BJ_INIT_MBIT(insns[i].k);
			}

			if (BPF_MODE(insns[i].code) == BPF_MSH &&
			    BPF_SIZE(insns[i].code) == BPF_B) {
				*hints |= BJ_HINT_MSH;
			}

			invalid &= ~BJ_INIT_XBIT;
			continue;

		case BPF_ST:
			*initmask |= invalid & BJ_INIT_ABIT;

			if ((uint32_t)insns[i].k < memwords)
				invalid &= ~BJ_INIT_MBIT(insns[i].k);

			continue;

		case BPF_STX:
			*hints |= BJ_HINT_XREG;
			*initmask |= invalid & BJ_INIT_XBIT;

			if ((uint32_t)insns[i].k < memwords)
				invalid &= ~BJ_INIT_MBIT(insns[i].k);

			continue;

		case BPF_ALU:
			*initmask |= invalid & BJ_INIT_ABIT;

			if (insns[i].code != (BPF_ALU|BPF_NEG) &&
			    BPF_SRC(insns[i].code) == BPF_X) {
				*hints |= BJ_HINT_XREG;
				*initmask |= invalid & BJ_INIT_XBIT;
			}

			invalid &= ~BJ_INIT_ABIT;
			continue;

		case BPF_MISC:
			switch (BPF_MISCOP(insns[i].code)) {
			case BPF_TAX: // X <- A
				*hints |= BJ_HINT_XREG;
				*initmask |= invalid & BJ_INIT_ABIT;
				invalid &= ~BJ_INIT_XBIT;
				continue;

			case BPF_TXA: // A <- X
				*hints |= BJ_HINT_XREG;
				*initmask |= invalid & BJ_INIT_XBIT;
				invalid &= ~BJ_INIT_ABIT;
				continue;

			case BPF_COPX:
				*hints |= BJ_HINT_XREG | BJ_HINT_COPX;
				/* FALLTHROUGH */

			case BPF_COP:
				*hints |= BJ_HINT_COP;
				*initmask |= invalid & BJ_INIT_ABIT;
				invalid &= ~BJ_INIT_ABIT;
				continue;
			}

			continue;

		case BPF_JMP:
			/* Initialize abc_length for ABC pass. */
			insn_dat[i].u.jdata.abc_length = MAX_ABC_LENGTH;

			*initmask |= invalid & BJ_INIT_ABIT;

			if (BPF_SRC(insns[i].code) == BPF_X) {
				*hints |= BJ_HINT_XREG;
				*initmask |= invalid & BJ_INIT_XBIT;
			}

			if (BPF_OP(insns[i].code) == BPF_JA) {
				jt = jf = insns[i].k;
			} else {
				jt = insns[i].jt;
				jf = insns[i].jf;
			}

			if (jt >= insn_count - (i + 1) ||
			    jf >= insn_count - (i + 1)) {
				return false;
			}

			if (jt > 0 && jf > 0)
				unreachable = true;

			jt += i + 1;
			jf += i + 1;

			jtf = insn_dat[i].u.jdata.jtf;

			jtf[0].jdata = &insn_dat[i].u.jdata;
			SLIST_INSERT_HEAD(&insn_dat[jt].bjumps,
			    &jtf[0], entries);

			if (jf != jt) {
				jtf[1].jdata = &insn_dat[i].u.jdata;
				SLIST_INSERT_HEAD(&insn_dat[jf].bjumps,
				    &jtf[1], entries);
			}

			insn_dat[jf].invalid |= invalid;
			insn_dat[jt].invalid |= invalid;
			invalid = 0;

			continue;
		}
	}

	return true;
}

/*
 * Array Bounds Check Elimination (ABC) pass.
 */
static void
optimize_pass2(const bpf_ctx_t *bc, const struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count)
{
	struct bpfjit_jump *jmp;
	const struct bpf_insn *pc;
	struct bpfjit_insn_data *pd;
	size_t i;
	bpfjit_abc_length_t length, abc_length = 0;

	const size_t extwords = GET_EXTWORDS(bc);

	for (i = insn_count; i != 0; i--) {
		pc = &insns[i-1];
		pd = &insn_dat[i-1];

		if (pd->unreachable)
			continue;

		switch (BPF_CLASS(pc->code)) {
		case BPF_RET:
			/*
			 * It's quite common for bpf programs to
			 * check packet bytes in increasing order
			 * and return zero if bytes don't match
			 * specified critetion. Such programs disable
			 * ABC optimization completely because for
			 * every jump there is a branch with no read
			 * instruction.
			 * With no side effects, BPF_STMT(BPF_RET+BPF_K, 0)
			 * is indistinguishable from out-of-bound load.
			 * Therefore, abc_length can be set to
			 * MAX_ABC_LENGTH and enable ABC for many
			 * bpf programs.
			 * If this optimization encounters any
			 * instruction with a side effect, it will
			 * reset abc_length.
			 */
			if (BPF_RVAL(pc->code) == BPF_K && pc->k == 0)
				abc_length = MAX_ABC_LENGTH;
			else
				abc_length = 0;
			break;

		case BPF_MISC:
			if (BPF_MISCOP(pc->code) == BPF_COP ||
			    BPF_MISCOP(pc->code) == BPF_COPX) {
				/* COP instructions can have side effects. */
				abc_length = 0;
			}
			break;

		case BPF_ST:
		case BPF_STX:
			if (extwords != 0) {
				/* Write to memory is visible after a call. */
				abc_length = 0;
			}
			break;

		case BPF_JMP:
			abc_length = pd->u.jdata.abc_length;
			break;

		default:
			if (read_pkt_insn(pc, &length)) {
				if (abc_length < length)
					abc_length = length;
				pd->u.rdata.abc_length = abc_length;
			}
			break;
		}

		SLIST_FOREACH(jmp, &pd->bjumps, entries) {
			if (jmp->jdata->abc_length > abc_length)
				jmp->jdata->abc_length = abc_length;
		}
	}
}

static void
optimize_pass3(const struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count)
{
	struct bpfjit_jump *jmp;
	size_t i;
	bpfjit_abc_length_t checked_length = 0;

	for (i = 0; i < insn_count; i++) {
		if (insn_dat[i].unreachable)
			continue;

		SLIST_FOREACH(jmp, &insn_dat[i].bjumps, entries) {
			if (jmp->jdata->checked_length < checked_length)
				checked_length = jmp->jdata->checked_length;
		}

		if (BPF_CLASS(insns[i].code) == BPF_JMP) {
			insn_dat[i].u.jdata.checked_length = checked_length;
		} else if (read_pkt_insn(&insns[i], NULL)) {
			struct bpfjit_read_pkt_data *rdata =
			    &insn_dat[i].u.rdata;
			rdata->check_length = 0;
			if (checked_length < rdata->abc_length) {
				checked_length = rdata->abc_length;
				rdata->check_length = checked_length;
			}
		}
	}
}

static bool
optimize(const bpf_ctx_t *bc, const struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count,
    bpf_memword_init_t *initmask, bpfjit_hint_t *hints)
{

	optimize_init(insn_dat, insn_count);

	if (!optimize_pass1(bc, insns, insn_dat, insn_count, initmask, hints))
		return false;

	optimize_pass2(bc, insns, insn_dat, insn_count);
	optimize_pass3(insns, insn_dat, insn_count);

	return true;
}

/*
 * Convert BPF_ALU operations except BPF_NEG and BPF_DIV to sljit operation.
 */
static int
bpf_alu_to_sljit_op(const struct bpf_insn *pc)
{
	const int bad = SLJIT_UNUSED;
	const uint32_t k = pc->k;

	/*
	 * Note: all supported 64bit arches have 32bit multiply
	 * instruction so SLJIT_INT_OP doesn't have any overhead.
	 */
	switch (BPF_OP(pc->code)) {
	case BPF_ADD: return SLJIT_ADD;
	case BPF_SUB: return SLJIT_SUB;
	case BPF_MUL: return SLJIT_MUL|SLJIT_INT_OP;
	case BPF_OR:  return SLJIT_OR;
	case BPF_XOR: return SLJIT_XOR;
	case BPF_AND: return SLJIT_AND;
	case BPF_LSH: return (k > 31) ? bad : SLJIT_SHL;
	case BPF_RSH: return (k > 31) ? bad : SLJIT_LSHR|SLJIT_INT_OP;
	default:
		return bad;
	}
}

/*
 * Convert BPF_JMP operations except BPF_JA to sljit condition.
 */
static int
bpf_jmp_to_sljit_cond(const struct bpf_insn *pc, bool negate)
{
	/*
	 * Note: all supported 64bit arches have 32bit comparison
	 * instructions so SLJIT_INT_OP doesn't have any overhead.
	 */
	int rv = SLJIT_INT_OP;

	switch (BPF_OP(pc->code)) {
	case BPF_JGT:
		rv |= negate ? SLJIT_C_LESS_EQUAL : SLJIT_C_GREATER;
		break;
	case BPF_JGE:
		rv |= negate ? SLJIT_C_LESS : SLJIT_C_GREATER_EQUAL;
		break;
	case BPF_JEQ:
		rv |= negate ? SLJIT_C_NOT_EQUAL : SLJIT_C_EQUAL;
		break;
	case BPF_JSET:
		rv |= negate ? SLJIT_C_EQUAL : SLJIT_C_NOT_EQUAL;
		break;
	default:
		BJ_ASSERT(false);
	}

	return rv;
}

/*
 * Convert BPF_K and BPF_X to sljit register.
 */
static int
kx_to_reg(const struct bpf_insn *pc)
{

	switch (BPF_SRC(pc->code)) {
	case BPF_K: return SLJIT_IMM;
	case BPF_X: return BJ_XREG;
	default:
		BJ_ASSERT(false);
		return 0;
	}
}

static sljit_sw
kx_to_reg_arg(const struct bpf_insn *pc)
{

	switch (BPF_SRC(pc->code)) {
	case BPF_K: return (uint32_t)pc->k; /* SLJIT_IMM, pc->k, */
	case BPF_X: return 0;               /* BJ_XREG, 0,      */
	default:
		BJ_ASSERT(false);
		return 0;
	}
}

static bool
generate_insn_code(struct sljit_compiler *compiler, bpfjit_hint_t hints,
    const bpf_ctx_t *bc, const struct bpf_insn *insns,
    struct bpfjit_insn_data *insn_dat, size_t insn_count)
{
	/* a list of jumps to out-of-bound return from a generated function */
	struct sljit_jump **ret0;
	size_t ret0_size, ret0_maxsize;

	struct sljit_jump *jump;
	struct sljit_label *label;
	const struct bpf_insn *pc;
	struct bpfjit_jump *bjump, *jtf;
	struct sljit_jump *to_mchain_jump;

	size_t i;
	int status;
	int branching, negate;
	unsigned int rval, mode, src, op;
	uint32_t jt, jf;

	bool unconditional_ret;
	bool rv;

	const size_t extwords = GET_EXTWORDS(bc);
	const size_t memwords = GET_MEMWORDS(bc);

	ret0 = NULL;
	rv = false;

	ret0_size = 0;
	ret0_maxsize = 64;
	ret0 = BJ_ALLOC(ret0_maxsize * sizeof(ret0[0]));
	if (ret0 == NULL)
		goto fail;

	/* reset sjump members of jdata */
	for (i = 0; i < insn_count; i++) {
		if (insn_dat[i].unreachable ||
		    BPF_CLASS(insns[i].code) != BPF_JMP) {
			continue;
		}

		jtf = insn_dat[i].u.jdata.jtf;
		jtf[0].sjump = jtf[1].sjump = NULL;
	}

	/* main loop */
	for (i = 0; i < insn_count; i++) {
		if (insn_dat[i].unreachable)
			continue;

		/*
		 * Resolve jumps to the current insn.
		 */
		label = NULL;
		SLIST_FOREACH(bjump, &insn_dat[i].bjumps, entries) {
			if (bjump->sjump != NULL) {
				if (label == NULL)
					label = sljit_emit_label(compiler);
				if (label == NULL)
					goto fail;
				sljit_set_label(bjump->sjump, label);
			}
		}

		to_mchain_jump = NULL;
		unconditional_ret = false;

		if (read_pkt_insn(&insns[i], NULL)) {
			if (insn_dat[i].u.rdata.check_length > UINT32_MAX) {
				/* Jump to "return 0" unconditionally. */
				unconditional_ret = true;
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;
				if (!append_jump(jump, &ret0,
				    &ret0_size, &ret0_maxsize))
					goto fail;
			} else if (insn_dat[i].u.rdata.check_length > 0) {
				/* if (buflen < check_length) return 0; */
				jump = sljit_emit_cmp(compiler,
				    SLJIT_C_LESS,
				    BJ_BUFLEN, 0,
				    SLJIT_IMM,
				    insn_dat[i].u.rdata.check_length);
				if (jump == NULL)
					goto fail;
#ifdef _KERNEL
				to_mchain_jump = jump;
#else
				if (!append_jump(jump, &ret0,
				    &ret0_size, &ret0_maxsize))
					goto fail;
#endif
			}
		}

		pc = &insns[i];
		switch (BPF_CLASS(pc->code)) {

		default:
			goto fail;

		case BPF_LD:
			/* BPF_LD+BPF_IMM          A <- k */
			if (pc->code == (BPF_LD|BPF_IMM)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BJ_AREG, 0,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LD+BPF_MEM          A <- M[k] */
			if (pc->code == (BPF_LD|BPF_MEM)) {
				if ((uint32_t)pc->k >= memwords)
					goto fail;
				status = emit_memload(compiler,
				    BJ_AREG, pc->k, extwords);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LD+BPF_W+BPF_LEN    A <- len */
			if (pc->code == (BPF_LD|BPF_W|BPF_LEN)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV, /* size_t source */
				    BJ_AREG, 0,
				    SLJIT_MEM1(BJ_ARGS),
				    offsetof(struct bpf_args, wirelen));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			mode = BPF_MODE(pc->code);
			if (mode != BPF_ABS && mode != BPF_IND)
				goto fail;

			if (unconditional_ret)
				continue;

			status = emit_pkt_read(compiler, hints, pc,
			    to_mchain_jump, &ret0, &ret0_size, &ret0_maxsize);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_LDX:
			mode = BPF_MODE(pc->code);

			/* BPF_LDX+BPF_W+BPF_IMM    X <- k */
			if (mode == BPF_IMM) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BJ_XREG, 0,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_W+BPF_LEN    X <- len */
			if (mode == BPF_LEN) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV, /* size_t source */
				    BJ_XREG, 0,
				    SLJIT_MEM1(BJ_ARGS),
				    offsetof(struct bpf_args, wirelen));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_W+BPF_MEM    X <- M[k] */
			if (mode == BPF_MEM) {
				if (BPF_SIZE(pc->code) != BPF_W)
					goto fail;
				if ((uint32_t)pc->k >= memwords)
					goto fail;
				status = emit_memload(compiler,
				    BJ_XREG, pc->k, extwords);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_LDX+BPF_B+BPF_MSH    X <- 4*(P[k:1]&0xf) */
			if (mode != BPF_MSH || BPF_SIZE(pc->code) != BPF_B)
				goto fail;

			if (unconditional_ret)
				continue;

			status = emit_msh(compiler, hints, pc,
			    to_mchain_jump, &ret0, &ret0_size, &ret0_maxsize);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_ST:
			if (pc->code != BPF_ST ||
			    (uint32_t)pc->k >= memwords) {
				goto fail;
			}

			status = emit_memstore(compiler,
			    BJ_AREG, pc->k, extwords);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_STX:
			if (pc->code != BPF_STX ||
			    (uint32_t)pc->k >= memwords) {
				goto fail;
			}

			status = emit_memstore(compiler,
			    BJ_XREG, pc->k, extwords);
			if (status != SLJIT_SUCCESS)
				goto fail;

			continue;

		case BPF_ALU:
			if (pc->code == (BPF_ALU|BPF_NEG)) {
				status = sljit_emit_op1(compiler,
				    SLJIT_NEG,
				    BJ_AREG, 0,
				    BJ_AREG, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			op = BPF_OP(pc->code);
			if (op != BPF_DIV && op != BPF_MOD) {
				const int op2 = bpf_alu_to_sljit_op(pc);

				if (op2 == SLJIT_UNUSED)
					goto fail;
				status = sljit_emit_op2(compiler,
				    op2, BJ_AREG, 0, BJ_AREG, 0,
				    kx_to_reg(pc), kx_to_reg_arg(pc));
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			/* BPF_DIV/BPF_MOD */

			src = BPF_SRC(pc->code);
			if (src != BPF_X && src != BPF_K)
				goto fail;

			/* division by zero? */
			if (src == BPF_X) {
				jump = sljit_emit_cmp(compiler,
				    SLJIT_C_EQUAL|SLJIT_INT_OP,
				    BJ_XREG, 0,
				    SLJIT_IMM, 0);
				if (jump == NULL)
					goto fail;
				if (!append_jump(jump, &ret0,
				    &ret0_size, &ret0_maxsize))
					goto fail;
			} else if (pc->k == 0) {
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;
				if (!append_jump(jump, &ret0,
				    &ret0_size, &ret0_maxsize))
					goto fail;
			}

			if (src == BPF_X) {
				status = emit_moddiv(compiler, pc);
				if (status != SLJIT_SUCCESS)
					goto fail;
			} else if (pc->k != 0) {
				if (pc->k & (pc->k - 1)) {
					status = emit_moddiv(compiler, pc);
				} else {
					status = emit_pow2_moddiv(compiler, pc);
				}
				if (status != SLJIT_SUCCESS)
					goto fail;
			}

			continue;

		case BPF_JMP:
			op = BPF_OP(pc->code);
			if (op == BPF_JA) {
				jt = jf = pc->k;
			} else {
				jt = pc->jt;
				jf = pc->jf;
			}

			negate = (jt == 0) ? 1 : 0;
			branching = (jt == jf) ? 0 : 1;
			jtf = insn_dat[i].u.jdata.jtf;

			if (branching) {
				if (op != BPF_JSET) {
					jump = sljit_emit_cmp(compiler,
					    bpf_jmp_to_sljit_cond(pc, negate),
					    BJ_AREG, 0,
					    kx_to_reg(pc), kx_to_reg_arg(pc));
				} else {
					status = sljit_emit_op2(compiler,
					    SLJIT_AND,
					    BJ_TMP1REG, 0,
					    BJ_AREG, 0,
					    kx_to_reg(pc), kx_to_reg_arg(pc));
					if (status != SLJIT_SUCCESS)
						goto fail;

					jump = sljit_emit_cmp(compiler,
					    bpf_jmp_to_sljit_cond(pc, negate),
					    BJ_TMP1REG, 0,
					    SLJIT_IMM, 0);
				}

				if (jump == NULL)
					goto fail;

				BJ_ASSERT(jtf[negate].sjump == NULL);
				jtf[negate].sjump = jump;
			}

			if (!branching || (jt != 0 && jf != 0)) {
				jump = sljit_emit_jump(compiler, SLJIT_JUMP);
				if (jump == NULL)
					goto fail;

				BJ_ASSERT(jtf[branching].sjump == NULL);
				jtf[branching].sjump = jump;
			}

			continue;

		case BPF_RET:
			rval = BPF_RVAL(pc->code);
			if (rval == BPF_X)
				goto fail;

			/* BPF_RET+BPF_K    accept k bytes */
			if (rval == BPF_K) {
				status = sljit_emit_return(compiler,
				    SLJIT_MOV_UI,
				    SLJIT_IMM, (uint32_t)pc->k);
				if (status != SLJIT_SUCCESS)
					goto fail;
			}

			/* BPF_RET+BPF_A    accept A bytes */
			if (rval == BPF_A) {
				status = sljit_emit_return(compiler,
				    SLJIT_MOV_UI,
				    BJ_AREG, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;
			}

			continue;

		case BPF_MISC:
			switch (BPF_MISCOP(pc->code)) {
			case BPF_TAX:
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV_UI,
				    BJ_XREG, 0,
				    BJ_AREG, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;

			case BPF_TXA:
				status = sljit_emit_op1(compiler,
				    SLJIT_MOV,
				    BJ_AREG, 0,
				    BJ_XREG, 0);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;

			case BPF_COP:
			case BPF_COPX:
				if (bc == NULL || bc->copfuncs == NULL)
					goto fail;
				if (BPF_MISCOP(pc->code) == BPF_COP &&
				    (uint32_t)pc->k >= bc->nfuncs) {
					goto fail;
				}

				status = emit_cop(compiler, hints, bc, pc,
				    &ret0, &ret0_size, &ret0_maxsize);
				if (status != SLJIT_SUCCESS)
					goto fail;

				continue;
			}

			goto fail;
		} /* switch */
	} /* main loop */

	BJ_ASSERT(ret0_size <= ret0_maxsize);

	if (ret0_size > 0) {
		label = sljit_emit_label(compiler);
		if (label == NULL)
			goto fail;
		for (i = 0; i < ret0_size; i++)
			sljit_set_label(ret0[i], label);
	}

	status = sljit_emit_return(compiler,
	    SLJIT_MOV_UI,
	    SLJIT_IMM, 0);
	if (status != SLJIT_SUCCESS)
		goto fail;

	rv = true;

fail:
	if (ret0 != NULL)
		BJ_FREE(ret0, ret0_maxsize * sizeof(ret0[0]));

	return rv;
}

bpfjit_func_t
bpfjit_generate_code(const bpf_ctx_t *bc,
    const struct bpf_insn *insns, size_t insn_count)
{
	void *rv;
	struct sljit_compiler *compiler;

	size_t i;
	int status;

	/* optimization related */
	bpf_memword_init_t initmask;
	bpfjit_hint_t hints;

	/* memory store location for initial zero initialization */
	sljit_si mem_reg;
	sljit_sw mem_off;

	struct bpfjit_insn_data *insn_dat;

	const size_t extwords = GET_EXTWORDS(bc);
	const size_t memwords = GET_MEMWORDS(bc);
	const bpf_memword_init_t preinited = extwords ? bc->preinited : 0;

	rv = NULL;
	compiler = NULL;
	insn_dat = NULL;

	if (memwords > MAX_MEMWORDS)
		goto fail;

	if (insn_count == 0 || insn_count > SIZE_MAX / sizeof(insn_dat[0]))
		goto fail;

	insn_dat = BJ_ALLOC(insn_count * sizeof(insn_dat[0]));
	if (insn_dat == NULL)
		goto fail;

	if (!optimize(bc, insns, insn_dat, insn_count, &initmask, &hints))
		goto fail;

	compiler = sljit_create_compiler();
	if (compiler == NULL)
		goto fail;

#if !defined(_KERNEL) && defined(SLJIT_VERBOSE) && SLJIT_VERBOSE
	sljit_compiler_verbose(compiler, stderr);
#endif

	status = sljit_emit_enter(compiler,
	    2, nscratches(hints), nsaveds(hints), sizeof(struct bpfjit_stack));
	if (status != SLJIT_SUCCESS)
		goto fail;

	if (hints & BJ_HINT_COP) {
		/* save ctx argument */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, ctx),
		    BJ_CTX_ARG, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	if (extwords == 0) {
		mem_reg = SLJIT_MEM1(SLJIT_LOCALS_REG);
		mem_off = offsetof(struct bpfjit_stack, mem);
	} else {
		/* copy "mem" argument from bpf_args to bpfjit_stack */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    BJ_TMP1REG, 0,
		    SLJIT_MEM1(BJ_ARGS), offsetof(struct bpf_args, mem));
		if (status != SLJIT_SUCCESS)
			goto fail;

		status = sljit_emit_op1(compiler,
		    SLJIT_MOV_P,
		    SLJIT_MEM1(SLJIT_LOCALS_REG),
		    offsetof(struct bpfjit_stack, extmem),
		    BJ_TMP1REG, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;

		mem_reg = SLJIT_MEM1(BJ_TMP1REG);
		mem_off = 0;
	}

	/*
	 * Exclude pre-initialised external memory words but keep
	 * initialization statuses of A and X registers in case
	 * bc->preinited wrongly sets those two bits.
	 */
	initmask &= ~preinited | BJ_INIT_ABIT | BJ_INIT_XBIT;

#if defined(_KERNEL)
	/* bpf_filter() checks initialization of memwords. */
	BJ_ASSERT((initmask & (BJ_INIT_MBIT(memwords) - 1)) == 0);
#endif
	for (i = 0; i < memwords; i++) {
		if (initmask & BJ_INIT_MBIT(i)) {
			/* M[i] = 0; */
			status = sljit_emit_op1(compiler,
			    SLJIT_MOV_UI,
			    mem_reg, mem_off + i * sizeof(uint32_t),
			    SLJIT_IMM, 0);
			if (status != SLJIT_SUCCESS)
				goto fail;
		}
	}

	if (initmask & BJ_INIT_ABIT) {
		/* A = 0; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BJ_AREG, 0,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	if (initmask & BJ_INIT_XBIT) {
		/* X = 0; */
		status = sljit_emit_op1(compiler,
		    SLJIT_MOV,
		    BJ_XREG, 0,
		    SLJIT_IMM, 0);
		if (status != SLJIT_SUCCESS)
			goto fail;
	}

	status = load_buf_buflen(compiler);
	if (status != SLJIT_SUCCESS)
		goto fail;

	if (!generate_insn_code(compiler, hints,
	    bc, insns, insn_dat, insn_count)) {
		goto fail;
	}

	rv = sljit_generate_code(compiler);

fail:
	if (compiler != NULL)
		sljit_free_compiler(compiler);

	if (insn_dat != NULL)
		BJ_FREE(insn_dat, insn_count * sizeof(insn_dat[0]));

	return (bpfjit_func_t)rv;
}

void
bpfjit_free_code(bpfjit_func_t code)
{

	sljit_free_code((void *)code);
}

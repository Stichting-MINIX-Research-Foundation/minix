/*	$NetBSD: bpf_filter.c,v 1.70 2015/02/11 12:53:15 alnsn Exp $	*/

/*-
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bpf_filter.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bpf_filter.c,v 1.70 2015/02/11 12:53:15 alnsn Exp $");

#if 0
#if !(defined(lint) || defined(KERNEL))
static const char rcsid[] =
    "@(#) Header: bpf_filter.c,v 1.33 97/04/26 13:37:18 leres Exp  (LBL)";
#endif
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <sys/endian.h>

#define	__BPF_PRIVATE
#include <net/bpf.h>

#ifdef _KERNEL

bpf_ctx_t *
bpf_create(void)
{
	return kmem_zalloc(sizeof(bpf_ctx_t), KM_SLEEP);
}

void
bpf_destroy(bpf_ctx_t *bc)
{
	kmem_free(bc, sizeof(bpf_ctx_t));
}

int
bpf_set_cop(bpf_ctx_t *bc, const bpf_copfunc_t *funcs, size_t n)
{
	bc->copfuncs = funcs;
	bc->nfuncs = n;
	return 0;
}

int
bpf_set_extmem(bpf_ctx_t *bc, size_t nwords, bpf_memword_init_t preinited)
{
	if (nwords > BPF_MAX_MEMWORDS || (preinited >> nwords) != 0) {
		return EINVAL;
	}
	bc->extwords = nwords;
	bc->preinited = preinited;
	return 0;
}

#endif

#define EXTRACT_SHORT(p)	be16dec(p)
#define EXTRACT_LONG(p)		be32dec(p)

#ifdef _KERNEL
#include <sys/mbuf.h>
#define MINDEX(len, m, k) 		\
{					\
	len = m->m_len; 		\
	while (k >= len) { 		\
		k -= len; 		\
		m = m->m_next; 		\
		if (m == 0) 		\
			return 0; 	\
		len = m->m_len; 	\
	}				\
}

uint32_t m_xword(const struct mbuf *, uint32_t, int *);
uint32_t m_xhalf(const struct mbuf *, uint32_t, int *);
uint32_t m_xbyte(const struct mbuf *, uint32_t, int *);

#define xword(p, k, err) m_xword((const struct mbuf *)(p), (k), (err))
#define xhalf(p, k, err) m_xhalf((const struct mbuf *)(p), (k), (err))
#define xbyte(p, k, err) m_xbyte((const struct mbuf *)(p), (k), (err))

uint32_t
m_xword(const struct mbuf *m, uint32_t k, int *err)
{
	int len;
	u_char *cp, *np;
	struct mbuf *m0;

	*err = 1;
	MINDEX(len, m, k);
	cp = mtod(m, u_char *) + k;
	if (len - k >= 4) {
		*err = 0;
		return EXTRACT_LONG(cp);
	}
	m0 = m->m_next;
	if (m0 == 0 || (len - k) + m0->m_len < 4)
		return 0;
	*err = 0;
	np = mtod(m0, u_char *);

	switch (len - k) {
	case 1:
		return (cp[0] << 24) | (np[0] << 16) | (np[1] << 8) | np[2];
	case 2:
		return (cp[0] << 24) | (cp[1] << 16) | (np[0] << 8) | np[1];
	default:
		return (cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | np[0];
	}
}

uint32_t
m_xhalf(const struct mbuf *m, uint32_t k, int *err)
{
	int len;
	u_char *cp;
	struct mbuf *m0;

	*err = 1;
	MINDEX(len, m, k);
	cp = mtod(m, u_char *) + k;
	if (len - k >= 2) {
		*err = 0;
		return EXTRACT_SHORT(cp);
	}
	m0 = m->m_next;
	if (m0 == 0)
		return 0;
	*err = 0;
	return (cp[0] << 8) | mtod(m0, u_char *)[0];
}

uint32_t
m_xbyte(const struct mbuf *m, uint32_t k, int *err)
{
	int len;

	*err = 1;
	MINDEX(len, m, k);
	*err = 0;
	return mtod(m, u_char *)[k];
}
#else /* _KERNEL */
#include <stdlib.h>
#endif /* !_KERNEL */

#include <net/bpf.h>

/*
 * Execute the filter program starting at pc on the packet p
 * wirelen is the length of the original packet
 * buflen is the amount of data present
 */
#ifdef _KERNEL

u_int
bpf_filter(const struct bpf_insn *pc, const u_char *p, u_int wirelen,
    u_int buflen)
{
	uint32_t mem[BPF_MEMWORDS];
	bpf_args_t args = {
		.pkt = p,
		.wirelen = wirelen,
		.buflen = buflen,
		.mem = mem,
		.arg = NULL
	};

	return bpf_filter_ext(NULL, pc, &args);
}

u_int
bpf_filter_ext(const bpf_ctx_t *bc, const struct bpf_insn *pc, bpf_args_t *args)
#else
u_int
bpf_filter(const struct bpf_insn *pc, const u_char *p, u_int wirelen,
    u_int buflen)
#endif
{
	uint32_t A, X, k;
#ifndef _KERNEL
	uint32_t mem[BPF_MEMWORDS];
	bpf_args_t args_store = {
		.pkt = p,
		.wirelen = wirelen,
		.buflen = buflen,
		.mem = mem,
		.arg = NULL
	};
	bpf_args_t * const args = &args_store;
#else
	const uint8_t * const p = args->pkt;
#endif
	if (pc == 0) {
		/*
		 * No filter means accept all.
		 */
		return (u_int)-1;
	}

	/*
	 * Note: safe to leave memwords uninitialised, as the validation
	 * step ensures that it will not be read, if it was not written.
	 */
	A = 0;
	X = 0;
	--pc;

	for (;;) {
		++pc;
		switch (pc->code) {

		default:
#ifdef _KERNEL
			return 0;
#else
			abort();
			/*NOTREACHED*/
#endif
		case BPF_RET|BPF_K:
			return (u_int)pc->k;

		case BPF_RET|BPF_A:
			return (u_int)A;

		case BPF_LD|BPF_W|BPF_ABS:
			k = pc->k;
			if (k > args->buflen ||
			    sizeof(int32_t) > args->buflen - k) {
#ifdef _KERNEL
				int merr;

				if (args->buflen != 0)
					return 0;
				A = xword(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = EXTRACT_LONG(&p[k]);
			continue;

		case BPF_LD|BPF_H|BPF_ABS:
			k = pc->k;
			if (k > args->buflen ||
			    sizeof(int16_t) > args->buflen - k) {
#ifdef _KERNEL
				int merr;

				if (args->buflen != 0)
					return 0;
				A = xhalf(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_ABS:
			k = pc->k;
			if (k >= args->buflen) {
#ifdef _KERNEL
				int merr;

				if (args->buflen != 0)
					return 0;
				A = xbyte(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = p[k];
			continue;

		case BPF_LD|BPF_W|BPF_LEN:
			A = args->wirelen;
			continue;

		case BPF_LDX|BPF_W|BPF_LEN:
			X = args->wirelen;
			continue;

		case BPF_LD|BPF_W|BPF_IND:
			k = X + pc->k;
			if (k < X || k >= args->buflen ||
			    sizeof(int32_t) > args->buflen - k) {
#ifdef _KERNEL
				int merr;

				if (k < X || args->buflen != 0)
					return 0;
				A = xword(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = EXTRACT_LONG(&p[k]);
			continue;

		case BPF_LD|BPF_H|BPF_IND:
			k = X + pc->k;
			if (k < X || k >= args->buflen ||
			    sizeof(int16_t) > args->buflen - k) {
#ifdef _KERNEL
				int merr;

				if (k < X || args->buflen != 0)
					return 0;
				A = xhalf(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_IND:
			k = X + pc->k;
			if (k < X || k >= args->buflen) {
#ifdef _KERNEL
				int merr;

				if (k < X || args->buflen != 0)
					return 0;
				A = xbyte(args->pkt, k, &merr);
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			A = p[k];
			continue;

		case BPF_LDX|BPF_MSH|BPF_B:
			k = pc->k;
			if (k >= args->buflen) {
#ifdef _KERNEL
				int merr;

				if (args->buflen != 0)
					return 0;
				X = (xbyte(args->pkt, k, &merr) & 0xf) << 2;
				if (merr != 0)
					return 0;
				continue;
#else
				return 0;
#endif
			}
			X = (p[pc->k] & 0xf) << 2;
			continue;

		case BPF_LD|BPF_IMM:
			A = pc->k;
			continue;

		case BPF_LDX|BPF_IMM:
			X = pc->k;
			continue;

		case BPF_LD|BPF_MEM:
			A = args->mem[pc->k];
			continue;

		case BPF_LDX|BPF_MEM:
			X = args->mem[pc->k];
			continue;

		case BPF_ST:
			args->mem[pc->k] = A;
			continue;

		case BPF_STX:
			args->mem[pc->k] = X;
			continue;

		case BPF_JMP|BPF_JA:
			pc += pc->k;
			continue;

		case BPF_JMP|BPF_JGT|BPF_K:
			pc += (A > pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_K:
			pc += (A >= pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_K:
			pc += (A == pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_K:
			pc += (A & pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGT|BPF_X:
			pc += (A > X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_X:
			pc += (A >= X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_X:
			pc += (A == X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_X:
			pc += (A & X) ? pc->jt : pc->jf;
			continue;

		case BPF_ALU|BPF_ADD|BPF_X:
			A += X;
			continue;

		case BPF_ALU|BPF_SUB|BPF_X:
			A -= X;
			continue;

		case BPF_ALU|BPF_MUL|BPF_X:
			A *= X;
			continue;

		case BPF_ALU|BPF_DIV|BPF_X:
			if (X == 0)
				return 0;
			A /= X;
			continue;

		case BPF_ALU|BPF_MOD|BPF_X:
			if (X == 0)
				return 0;
			A %= X;
			continue;

		case BPF_ALU|BPF_AND|BPF_X:
			A &= X;
			continue;

		case BPF_ALU|BPF_OR|BPF_X:
			A |= X;
			continue;

		case BPF_ALU|BPF_XOR|BPF_X:
			A ^= X;
			continue;

		case BPF_ALU|BPF_LSH|BPF_X:
			A <<= X;
			continue;

		case BPF_ALU|BPF_RSH|BPF_X:
			A >>= X;
			continue;

		case BPF_ALU|BPF_ADD|BPF_K:
			A += pc->k;
			continue;

		case BPF_ALU|BPF_SUB|BPF_K:
			A -= pc->k;
			continue;

		case BPF_ALU|BPF_MUL|BPF_K:
			A *= pc->k;
			continue;

		case BPF_ALU|BPF_DIV|BPF_K:
			A /= pc->k;
			continue;

		case BPF_ALU|BPF_MOD|BPF_K:
			A %= pc->k;
			continue;

		case BPF_ALU|BPF_AND|BPF_K:
			A &= pc->k;
			continue;

		case BPF_ALU|BPF_OR|BPF_K:
			A |= pc->k;
			continue;

		case BPF_ALU|BPF_XOR|BPF_K:
			A ^= pc->k;
			continue;

		case BPF_ALU|BPF_LSH|BPF_K:
			A <<= pc->k;
			continue;

		case BPF_ALU|BPF_RSH|BPF_K:
			A >>= pc->k;
			continue;

		case BPF_ALU|BPF_NEG:
			A = -A;
			continue;

		case BPF_MISC|BPF_TAX:
			X = A;
			continue;

		case BPF_MISC|BPF_TXA:
			A = X;
			continue;

		case BPF_MISC|BPF_COP:
#ifdef _KERNEL
			if (pc->k < bc->nfuncs) {
				const bpf_copfunc_t fn = bc->copfuncs[pc->k];
				A = fn(bc, args, A);
				continue;
			}
#endif
			return 0;

		case BPF_MISC|BPF_COPX:
#ifdef _KERNEL
			if (X < bc->nfuncs) {
				const bpf_copfunc_t fn = bc->copfuncs[X];
				A = fn(bc, args, A);
				continue;
			}
#endif
			return 0;
		}
	}
}

/*
 * Return true if the 'fcode' is a valid filter program.
 * The constraints are that each jump be forward and to a valid
 * code, that memory accesses are within valid ranges (to the
 * extent that this can be checked statically; loads of packet
 * data have to be, and are, also checked at run time), and that
 * the code terminates with either an accept or reject.
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.
 */

#if defined(KERNEL) || defined(_KERNEL)

int
bpf_validate(const struct bpf_insn *f, int signed_len)
{
	return bpf_validate_ext(NULL, f, signed_len);
}

int
bpf_validate_ext(const bpf_ctx_t *bc, const struct bpf_insn *f, int signed_len)
#else
int
bpf_validate(const struct bpf_insn *f, int signed_len)
#endif
{
	u_int i, from, len, ok = 0;
	const struct bpf_insn *p;
#if defined(KERNEL) || defined(_KERNEL)
	bpf_memword_init_t *mem, invalid;
	size_t size;
	const size_t extwords = bc ? bc->extwords : 0;
	const size_t memwords = extwords ? extwords : BPF_MEMWORDS;
	const bpf_memword_init_t preinited = extwords ? bc->preinited : 0;
#else
	const size_t memwords = BPF_MEMWORDS;
#endif

	len = (u_int)signed_len;
	if (len < 1)
		return 0;
#if defined(KERNEL) || defined(_KERNEL)
	if (len > BPF_MAXINSNS)
		return 0;
#endif
	if (f[len - 1].code != (BPF_RET|BPF_K) &&
	    f[len - 1].code != (BPF_RET|BPF_A)) {
		return 0;
	}

#if defined(KERNEL) || defined(_KERNEL)
	/* Note: only the pre-initialised is valid on startup */
	mem = kmem_zalloc(size = sizeof(*mem) * len, KM_SLEEP);
	invalid = ~preinited;
#endif

	for (i = 0; i < len; ++i) {
#if defined(KERNEL) || defined(_KERNEL)
		/* blend in any invalid bits for current pc */
		invalid |= mem[i];
#endif
		p = &f[i];
		switch (BPF_CLASS(p->code)) {
		/*
		 * Check that memory operations use valid addresses.
		 */
		case BPF_LD:
		case BPF_LDX:
			switch (BPF_MODE(p->code)) {
			case BPF_MEM:
				/*
				 * There's no maximum packet data size
				 * in userland.  The runtime packet length
				 * check suffices.
				 */
#if defined(KERNEL) || defined(_KERNEL)
				/*
				 * More strict check with actual packet length
				 * is done runtime.
				 */
				if (p->k >= memwords)
					goto out;
				/* check for current memory invalid */
				if (invalid & BPF_MEMWORD_INIT(p->k))
					goto out;
#endif
				break;
			case BPF_ABS:
			case BPF_IND:
			case BPF_MSH:
			case BPF_IMM:
			case BPF_LEN:
				break;
			default:
				goto out;
			}
			break;
		case BPF_ST:
		case BPF_STX:
			if (p->k >= memwords)
				goto out;
#if defined(KERNEL) || defined(_KERNEL)
			/* validate the memory word */
			invalid &= ~BPF_MEMWORD_INIT(p->k);
#endif
			break;
		case BPF_ALU:
			switch (BPF_OP(p->code)) {
			case BPF_ADD:
			case BPF_SUB:
			case BPF_MUL:
			case BPF_OR:
			case BPF_XOR:
			case BPF_AND:
			case BPF_LSH:
			case BPF_RSH:
			case BPF_NEG:
				break;
			case BPF_DIV:
			case BPF_MOD:
				/*
				 * Check for constant division by 0.
				 */
				if (BPF_SRC(p->code) == BPF_K && p->k == 0)
					goto out;
				break;
			default:
				goto out;
			}
			break;
		case BPF_JMP:
			/*
			 * Check that jumps are within the code block,
			 * and that unconditional branches don't go
			 * backwards as a result of an overflow.
			 * Unconditional branches have a 32-bit offset,
			 * so they could overflow; we check to make
			 * sure they don't.  Conditional branches have
			 * an 8-bit offset, and the from address is <=
			 * BPF_MAXINSNS, and we assume that BPF_MAXINSNS
			 * is sufficiently small that adding 255 to it
			 * won't overflow.
			 *
			 * We know that len is <= BPF_MAXINSNS, and we
			 * assume that BPF_MAXINSNS is < the maximum size
			 * of a u_int, so that i + 1 doesn't overflow.
			 *
			 * For userland, we don't know that the from
			 * or len are <= BPF_MAXINSNS, but we know that
			 * from <= len, and, except on a 64-bit system,
			 * it's unlikely that len, if it truly reflects
			 * the size of the program we've been handed,
			 * will be anywhere near the maximum size of
			 * a u_int.  We also don't check for backward
			 * branches, as we currently support them in
			 * userland for the protochain operation.
			 */
			from = i + 1;
			switch (BPF_OP(p->code)) {
			case BPF_JA:
				if (from + p->k >= len)
					goto out;
#if defined(KERNEL) || defined(_KERNEL)
				if (from + p->k < from)
					goto out;
				/*
				 * mark the currently invalid bits for the
				 * destination
				 */
				mem[from + p->k] |= invalid;
				invalid = 0;
#endif
				break;
			case BPF_JEQ:
			case BPF_JGT:
			case BPF_JGE:
			case BPF_JSET:
				if (from + p->jt >= len || from + p->jf >= len)
					goto out;
#if defined(KERNEL) || defined(_KERNEL)
				/*
				 * mark the currently invalid bits for both
				 * possible jump destinations
				 */
				mem[from + p->jt] |= invalid;
				mem[from + p->jf] |= invalid;
				invalid = 0;
#endif
				break;
			default:
				goto out;
			}
			break;
		case BPF_RET:
			break;
		case BPF_MISC:
			switch (BPF_MISCOP(p->code)) {
			case BPF_COP:
			case BPF_COPX:
				/* In-kernel COP use only. */
#if defined(KERNEL) || defined(_KERNEL)
				if (bc == NULL || bc->copfuncs == NULL)
					goto out;
				if (BPF_MISCOP(p->code) == BPF_COP &&
				    p->k >= bc->nfuncs) {
					goto out;
				}
				break;
#else
				goto out;
#endif
			default:
				break;
			}
			break;
		default:
			goto out;
		}
	}
	ok = 1;
out:
#if defined(KERNEL) || defined(_KERNEL)
	kmem_free(mem, size);
#endif
	return ok;
}

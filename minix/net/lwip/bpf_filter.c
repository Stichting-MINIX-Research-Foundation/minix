/* LWIP service - bpf_filter.c - Berkeley Packet Filter core implementation */
/*
 * This is basically a drop-in replacement of NetBSD's bpf_filter.c, which
 * itself can be compiled for either the NetBSD kernel or for userland.  On
 * MINIX 3, we would like to perform certain checks that NetBSD implements only
 * for its kernel (e.g., memory store access validation) while replacing the
 * NetBSD kernel specifics with our own (pbuf instead of mbuf, no BPF contexts
 * for now, etc.).  As a result, it is easier to reimplement the whole thing,
 * because there is not all that much to it.
 *
 * Support for the standard BSD API allows us to run standard tests against
 * this module from userland, where _MINIX_SYSTEM is not defined.  MINIX 3
 * specific extensions are enabled only if _MINIX_SYSTEM is defined.
 */
#include <string.h>
#include <limits.h>
#include <net/bpf.h>
#include <minix/bitmap.h>

#ifdef _MINIX_SYSTEM
#include "lwip.h"

/*
 * Obtain an unsigned 32-bit value in network byte order from the pbuf chain
 * 'pbuf' at offset 'k'.  The given offset is guaranteed to be within bounds.
 */
static uint32_t
bpf_get32_ext(const struct pbuf * pbuf, uint32_t k)
{
	uint32_t val;
	unsigned int i;

	/*
	 * Find the pbuf that contains the first byte.  We expect that most
	 * filters will operate only on the headers of packets, so that we
	 * mostly avoid going through this O(n) loop.  Since only the superuser
	 * can open BPF devices at all, we need not be worried about abuse in
	 * this regard.  However, it turns out that this loop is particularly
	 * CPU-intensive after all, we can probably improve it by caching the
	 * last visited pbuf, as read locality is likely high.
	 */
	while (k >= pbuf->len) {
		k -= pbuf->len;
		pbuf = pbuf->next;
		assert(pbuf != NULL);
	}

	/*
	 * We assume that every pbuf has some data, but we make no assumptions
	 * about any minimum amount of data per pbuf.  Therefore, we may have
	 * to take the bytes from anywhere between one and four pbufs.
	 * Hopefully the compiler will unroll this loop for us.
	 */
	val = (uint32_t)(((u_char *)pbuf->payload)[k]) << 24;

	for (i = 0; i < 3; i++) {
		if (k >= (uint32_t)pbuf->len - 1) {
			k = 0;
			pbuf = pbuf->next;
			assert(pbuf != NULL);
		} else
			k++;
		val = (val << 8) | (uint32_t)(((u_char *)pbuf->payload)[k]);
	}

	return val;
}

/*
 * Obtain an unsigned 16-bit value in network byte order from the pbuf chain
 * 'pbuf' at offset 'k'.  The given offset is guaranteed to be within bounds.
 */
static uint32_t
bpf_get16_ext(const struct pbuf * pbuf, uint32_t k)
{

	/* As above. */
	while (k >= pbuf->len) {
		k -= pbuf->len;
		pbuf = pbuf->next;
		assert(pbuf != NULL);
	}

	/*
	 * There are only two possible cases to cover here: either the two
	 * bytes are in the same pbuf, or they are in subsequent ones.
	 */
	if (k < (uint32_t)pbuf->len - 1) {
		return ((uint32_t)(((u_char *)pbuf->payload)[k]) << 8) |
		    (uint32_t)(((u_char *)pbuf->next->payload)[k + 1]);
	} else {
		assert(pbuf->next != NULL);
		return ((uint32_t)(((u_char *)pbuf->payload)[k]) << 8) |
		    (uint32_t)(((u_char *)pbuf->next->payload)[0]);
	}
}

/*
 * Obtain an unsigned 8-bit value from the pbuf chain 'pbuf' at offset 'k'.
 * The given offset is guaranteed to be within bounds.
 */
static uint32_t
bpf_get8_ext(const struct pbuf * pbuf, uint32_t k)
{

	/* As above. */
	while (k >= pbuf->len) {
		k -= pbuf->len;
		pbuf = pbuf->next;
		assert(pbuf != NULL);
	}

	return (uint32_t)(((u_char *)pbuf->payload)[k]);
}

#endif /* _MINIX_SYSTEM */

/*
 * Execute a BPF filter program on (the first part of) a packet, and return the
 * maximum size of the packet that should be delivered to the filter owner.
 *
 * The 'pc' parameter points to an array of BPF instructions that together form
 * the filter program to be executed.  If 'pc' is NULL, the packet is fully
 * accepted.  Otherwise, the given program MUST have passed a previous call to
 * bpf_validate().  Not doing so will allow for arbitrary memory access.
 *
 * The 'packet' array contains up to the whole packet.  The value of 'total'
 * denotes the total length of the packet; 'len' contains the size of the array
 * 'packet'.  Chunked storage of the packet is not supported at this time.
 *
 * If executing the program succeeds, the return value is the maximum number of
 * bytes from the packet to be delivered.  The return value may exceed the full
 * packet size.  If the number of bytes returned is zero, the packet is to be
 * ignored.  If the program fails to execute properly and return a value, a
 * value of zero is returned as well, thus also indicating that the packet
 * should be ignored.  This is intentional: it saves filter programs from
 * having to perform explicit checks on the packet they are filtering.
 */
u_int
bpf_filter(const struct bpf_insn * pc, const u_char * packet, u_int total,
	u_int len)
#ifdef _MINIX_SYSTEM
{

	return bpf_filter_ext(pc, NULL /*pbuf*/, packet, total, len);
}

u_int
bpf_filter_ext(const struct bpf_insn * pc, const struct pbuf * pbuf,
	const u_char * packet, u_int total, u_int len)
#endif /* _MINIX_SYSTEM */
{
	uint32_t k, a, x, mem[BPF_MEMWORDS];

	/* An empty program accepts all packets. */
	if (pc == NULL)
		return UINT_MAX;

	/*
	 * We need not clear 'mem': the checker guarantees that each memory
	 * store word is always written before it is read.
	 */
	a = 0;
	x = 0;

	/* Execute the program. */
	for (;; pc++) {
		k = pc->k;

		switch (pc->code) {
		case BPF_LD+BPF_W+BPF_IND:	/* A <- P[X+k:4] */
			if (k + x < k)
				return 0;
			k += x;
			/* FALLTHROUGH */
		case BPF_LD+BPF_W+BPF_ABS:	/* A <- P[k:4] */
			/*
			 * 'k' may have any value, so check bounds in such a
			 * way that 'k' cannot possibly overflow and wrap.
			 */
			if (len >= 3 && k < len - 3)
				a = ((uint32_t)packet[k] << 24) |
				    ((uint32_t)packet[k + 1] << 16) |
				    ((uint32_t)packet[k + 2] << 8) |
				    (uint32_t)packet[k + 3];
#ifdef _MINIX_SYSTEM
			else if (total >= 3 && k < total - 3)
				a = bpf_get32_ext(pbuf, k);
#endif /* _MINIX_SYSTEM */
			else
				return 0;
			break;
		case BPF_LD+BPF_H+BPF_IND:	/* A <- P[X+k:2] */
			if (k + x < k)
				return 0;
			k += x;
			/* FALLTHROUGH */
		case BPF_LD+BPF_H+BPF_ABS:	/* A <- P[k:2] */
			/* As above. */
			if (len >= 1 && k < len - 1)
				a = ((uint32_t)packet[k] << 8) |
				    (uint32_t)packet[k + 1];
#ifdef _MINIX_SYSTEM
			else if (total >= 1 && k < total - 1)
				a = bpf_get16_ext(pbuf, k);
#endif /* _MINIX_SYSTEM */
			else
				return 0;
			break;
		case BPF_LD+BPF_B+BPF_IND:	/* A <- P[X+k:1] */
			if (k + x < k)
				return 0;
			k += x;
			/* FALLTHROUGH */
		case BPF_LD+BPF_B+BPF_ABS:	/* A <- P[k:1] */
			if (k < len)
				a = (uint32_t)packet[k];
#ifdef _MINIX_SYSTEM
			else if (k < total)
				a = bpf_get8_ext(pbuf, k);
#endif /* _MINIX_SYSTEM */
			else
				return 0;
			break;
		case BPF_LD+BPF_W+BPF_LEN:	/* A <- len */
			a = total;
			break;
		case BPF_LD+BPF_IMM:		/* A <- k */
			a = k;
			break;
		case BPF_LD+BPF_MEM:		/* A <- M[k] */
			a = mem[k];
			break;

		case BPF_LDX+BPF_IMM:		/* X <- k */
			x = k;
			break;
		case BPF_LDX+BPF_MEM:		/* X <- M[k] */
			x = mem[k];
			break;
		case BPF_LDX+BPF_LEN:		/* X <- len */
			x = total;
			break;
		case BPF_LDX+BPF_B+BPF_MSH:	/* X <- 4*(P[k:1]&0xf) */
			if (k < len)
				x = ((uint32_t)packet[k] & 0xf) << 2;
#ifdef _MINIX_SYSTEM
			else if (k < total)
				x = (bpf_get8_ext(pbuf, k) & 0xf) << 2;
#endif /* _MINIX_SYSTEM */
			else
				return 0;
			break;

		case BPF_ST:			/* M[k] <- A */
			mem[k] = a;
			break;

		case BPF_STX:			/* M[k] <- X */
			mem[k] = x;
			break;

		case BPF_ALU+BPF_ADD+BPF_K:	/* A <- A + k */
			a += k;
			break;
		case BPF_ALU+BPF_SUB+BPF_K:	/* A <- A - k */
			a -= k;
			break;
		case BPF_ALU+BPF_MUL+BPF_K:	/* A <- A * k */
			a *= k;
			break;
		case BPF_ALU+BPF_DIV+BPF_K:	/* A <- A / k */
			a /= k;
			break;
		case BPF_ALU+BPF_MOD+BPF_K:	/* A <- A % k */
			a %= k;
			break;
		case BPF_ALU+BPF_AND+BPF_K:	/* A <- A & k */
			a &= k;
			break;
		case BPF_ALU+BPF_OR+BPF_K:	/* A <- A | k */
			a |= k;
			break;
		case BPF_ALU+BPF_XOR+BPF_K:	/* A <- A ^ k */
			a ^= k;
			break;
		case BPF_ALU+BPF_LSH+BPF_K:	/* A <- A << k */
			a <<= k;
			break;
		case BPF_ALU+BPF_RSH+BPF_K:	/* A <- A >> k */
			a >>= k;
			break;
		case BPF_ALU+BPF_ADD+BPF_X:	/* A <- A + X */
			a += x;
			break;
		case BPF_ALU+BPF_SUB+BPF_X:	/* A <- A - X */
			a -= x;
			break;
		case BPF_ALU+BPF_MUL+BPF_X:	/* A <- A * X */
			a *= x;
			break;
		case BPF_ALU+BPF_DIV+BPF_X:	/* A <- A / X */
			if (x == 0)
				return 0;
			a /= x;
			break;
		case BPF_ALU+BPF_MOD+BPF_X:	/* A <- A % X */
			if (x == 0)
				return 0;
			a %= x;
			break;
		case BPF_ALU+BPF_AND+BPF_X:	/* A <- A & X */
			a &= x;
			break;
		case BPF_ALU+BPF_OR+BPF_X:	/* A <- A | X */
			a |= x;
			break;
		case BPF_ALU+BPF_XOR+BPF_X:	/* A <- A ^ X */
			a ^= x;
			break;
		case BPF_ALU+BPF_LSH+BPF_X:	/* A <- A << X */
			if (x >= 32)
				return 0;
			a <<= x;
			break;
		case BPF_ALU+BPF_RSH+BPF_X:	/* A <- A >> X */
			if (x >= 32)
				return 0;
			a >>= x;
			break;
		case BPF_ALU+BPF_NEG:		/* A <- -A */
			a = -a;
			break;

		case BPF_JMP+BPF_JA:		/* pc += k */
			pc += k;
			break;
		case BPF_JMP+BPF_JGT+BPF_K:	/* pc += (A > k) ? jt : jf */
			pc += (a > k) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JGE+BPF_K:	/* pc += (A >= k) ? jt : jf */
			pc += (a >= k) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JEQ+BPF_K:	/* pc += (A == k) ? jt : jf */
			pc += (a == k) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JSET+BPF_K:	/* pc += (A & k) ? jt : jf */
			pc += (a & k) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JGT+BPF_X:	/* pc += (A > X) ? jt : jf */
			pc += (a > x) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JGE+BPF_X:	/* pc += (A >= X) ? jt : jf */
			pc += (a >= x) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JEQ+BPF_X:	/* pc += (A == X) ? jt : jf */
			pc += (a == x) ? pc->jt : pc->jf;
			break;
		case BPF_JMP+BPF_JSET+BPF_X:	/* pc += (A & X) ? jt : jf */
			pc += (a & x) ? pc->jt : pc->jf;
			break;

		case BPF_RET+BPF_A:		/* accept A bytes */
			return a;
		case BPF_RET+BPF_K:		/* accept K bytes */
			return k;

		case BPF_MISC+BPF_TAX:		/* X <- A */
			x = a;
			break;
		case BPF_MISC+BPF_TXA:		/* A <- X */
			a = x;
			break;

		default:			/* unknown instruction */
			return 0;
		}
	}

	/* NOTREACHED */
}

/*
 * In order to avoid having to perform explicit memory allocation, we store
 * some validation state on the stack, using data types that are as small as
 * possible for the current definitions.  The data types, and in fact the whole
 * assumption that we can store the state on the stack, may need to be revised
 * if certain constants are increased in the future.  As of writing, the
 * validation routine uses a little over 1KB of stack memory.
 */
#if BPF_MEMWORDS <= 16	/* value as of writing: 16 */
typedef uint16_t meminv_t;
#else
#error "increased BPF_MEMWORDS may require code revision"
#endif

#if BPF_MAXINSNS > 2048	/* value as of writing: 512 */
#error "increased BPF_MAXINSNS may require code revision"
#endif

/*
 * Verify that the given filter program is safe to execute, by performing as
 * many static validity checks as possible.  The program is given as 'insns',
 * which must be an array of 'ninsns' BPF instructions.  Unlike bpf_filter(),
 * this function does not accept empty filter programs.  The function returns 1
 * if the program was successfully validated, or 0 if the program should not be
 * accepted.
 */
int
bpf_validate(const struct bpf_insn * insns, int ninsns)
{
	bitchunk_t reachable[BITMAP_CHUNKS(BPF_MAXINSNS)];
	meminv_t invalid, meminv[BPF_MAXINSNS];
	const struct bpf_insn *insn;
	u_int pc, count, target;
	int advance;

	if (insns == NULL || ninsns <= 0 || ninsns > BPF_MAXINSNS)
		return 0;
	count = (u_int)ninsns;

	memset(reachable, 0, sizeof(reachable[0]) * BITMAP_CHUNKS(count));
	memset(meminv, 0, sizeof(meminv[0]) * count);

	SET_BIT(reachable, 0);
	meminv[0] = (meminv_t)~0;

	for (pc = 0; pc < count; pc++) {
		/* We completely ignore instructions that are not reachable. */
		if (!GET_BIT(reachable, pc))
			continue;

		invalid = meminv[pc];
		advance = 1;

		insn = &insns[pc];

		switch (insn->code) {
		case BPF_LD+BPF_W+BPF_ABS:
		case BPF_LD+BPF_H+BPF_ABS:
		case BPF_LD+BPF_B+BPF_ABS:
		case BPF_LD+BPF_W+BPF_IND:
		case BPF_LD+BPF_H+BPF_IND:
		case BPF_LD+BPF_B+BPF_IND:
		case BPF_LD+BPF_LEN:
		case BPF_LD+BPF_IMM:
		case BPF_LDX+BPF_IMM:
		case BPF_LDX+BPF_LEN:
		case BPF_LDX+BPF_B+BPF_MSH:
		case BPF_ALU+BPF_ADD+BPF_K:
		case BPF_ALU+BPF_SUB+BPF_K:
		case BPF_ALU+BPF_MUL+BPF_K:
		case BPF_ALU+BPF_AND+BPF_K:
		case BPF_ALU+BPF_OR+BPF_K:
		case BPF_ALU+BPF_XOR+BPF_K:
		case BPF_ALU+BPF_ADD+BPF_X:
		case BPF_ALU+BPF_SUB+BPF_X:
		case BPF_ALU+BPF_MUL+BPF_X:
		case BPF_ALU+BPF_DIV+BPF_X:
		case BPF_ALU+BPF_MOD+BPF_X:
		case BPF_ALU+BPF_AND+BPF_X:
		case BPF_ALU+BPF_OR+BPF_X:
		case BPF_ALU+BPF_XOR+BPF_X:
		case BPF_ALU+BPF_LSH+BPF_X:
		case BPF_ALU+BPF_RSH+BPF_X:
		case BPF_ALU+BPF_NEG:
		case BPF_MISC+BPF_TAX:
		case BPF_MISC+BPF_TXA:
			/* Nothing we can check for these. */
			break;
		case BPF_ALU+BPF_DIV+BPF_K:
		case BPF_ALU+BPF_MOD+BPF_K:
			/* No division by zero. */
			if (insn->k == 0)
				return 0;
			break;
		case BPF_ALU+BPF_LSH+BPF_K:
		case BPF_ALU+BPF_RSH+BPF_K:
			/* Do not invoke undefined behavior. */
			if (insn->k >= 32)
				return 0;
			break;
		case BPF_LD+BPF_MEM:
		case BPF_LDX+BPF_MEM:
			/*
			 * Only allow loading words that have been stored in
			 * all execution paths leading up to this instruction.
			 */
			if (insn->k >= BPF_MEMWORDS ||
			    (invalid & (1 << insn->k)))
				return 0;
			break;
		case BPF_ST:
		case BPF_STX:
			if (insn->k >= BPF_MEMWORDS)
				return 0;
			invalid &= ~(1 << insn->k);
			break;
		case BPF_JMP+BPF_JA:
			/*
			 * Make sure that the target instruction of the jump is
			 * still part of the program, and mark it as reachable.
			 */
			if (insn->k >= count - pc - 1)
				return 0;
			target = pc + insn->k + 1;
			SET_BIT(reachable, target);
			meminv[target] |= invalid;
			advance = 0;
			break;
		case BPF_JMP+BPF_JGT+BPF_K:
		case BPF_JMP+BPF_JGE+BPF_K:
		case BPF_JMP+BPF_JEQ+BPF_K:
		case BPF_JMP+BPF_JSET+BPF_K:
		case BPF_JMP+BPF_JGT+BPF_X:
		case BPF_JMP+BPF_JGE+BPF_X:
		case BPF_JMP+BPF_JEQ+BPF_X:
		case BPF_JMP+BPF_JSET+BPF_X:
			/*
			 * Make sure that both target instructions are still
			 * part of the program, and mark both as reachable.
			 * There is no chance that the additions will overflow.
			 */
			target = pc + insn->jt + 1;
			if (target >= count)
				return 0;
			SET_BIT(reachable, target);
			meminv[target] |= invalid;

			target = pc + insn->jf + 1;
			if (target >= count)
				return 0;
			SET_BIT(reachable, target);
			meminv[target] |= invalid;

			advance = 0;
			break;
		case BPF_RET+BPF_A:
		case BPF_RET+BPF_K:
			advance = 0;
			break;
		default:
			return 0;
		}

		/*
		 * After most instructions, we simply advance to the next.  For
		 * one thing, this means that there must be a next instruction
		 * at all.
		 */
		if (advance) {
			if (pc + 1 == count)
				return 0;
			SET_BIT(reachable, pc + 1);
			meminv[pc + 1] |= invalid;
		}
	}

	/* The program has passed all our basic tests. */
	return 1;
}

/*	$NetBSD: cpu_in_cksum.c,v 1.1 2008/01/25 21:12:14 joerg Exp $	*/
/*-
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>.
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
__KERNEL_RCSID(0, "$NetBSD: cpu_in_cksum.c,v 1.1 2008/01/25 21:12:14 joerg Exp $");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define KASSERT(x) assert(x)
#endif

#include <machine/limits.h>

#include <netinet/in.h>

#ifndef _KERNEL
int	cpu_in_cksum(struct mbuf*, int, int, uint32_t);
#endif

/*
 * Checksum routine for Internet Protocol family headers (Portable Version).
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 *
 * A discussion of different implementation techniques can be found in
 * RFC 1071.
 *
 * The default implementation for 32bit architectures is using
 * a 32bit accumulator and operating on 16bit operands.
 *
 * The default implementation for 64bit architectures is using
 * a 64bit accumulator and operating on 32bit operands.
 *
 * Both versions are unrolled to handle 32 Byte / 64 Byte fragments as core
 * of the inner loop. After each iteration of the inner loop, a partial
 * reduction is done to avoid carry in long packets.
 */

#if ULONG_MAX == 0xffffffffUL
/* 32bit version */
int
cpu_in_cksum(struct mbuf *m, int len, int off, uint32_t initial_sum)
{
	int mlen;
	uint32_t sum, partial;
	unsigned int final_acc;
	uint8_t *data;
	bool needs_swap, started_on_odd;

	KASSERT(len >= 0);
	KASSERT(off >= 0);

	needs_swap = false;
	started_on_odd = false;
	sum = (initial_sum >> 16) + (initial_sum & 0xffff);

	for (;;) {
		if (__predict_false(m == NULL)) {
			printf("in_cksum: out of data\n");
			return -1;
		}
		mlen = m->m_len;
		if (mlen > off) {
			mlen -= off;
			data = mtod(m, uint8_t *) + off;
			goto post_initial_offset;
		}
		off -= mlen;
		if (len == 0)
			break;
		m = m->m_next;
	}

	for (; len > 0; m = m->m_next) {
		if (__predict_false(m == NULL)) {
			printf("in_cksum: out of data\n");
			return -1;
		}
		mlen = m->m_len;
		data = mtod(m, uint8_t *);
 post_initial_offset:
		if (mlen == 0)
			continue;
		if (mlen > len)
			mlen = len;
		len -= mlen;

		partial = 0;
		if ((uintptr_t)data & 1) {
			/* Align on word boundary */
			started_on_odd = !started_on_odd;
#if _BYTE_ORDER == _LITTLE_ENDIAN
			partial = *data << 8;
#else
			partial = *data;
#endif
			++data;
			--mlen;
		}
		needs_swap = started_on_odd;
		while (mlen >= 32) {
			__builtin_prefetch(data + 32);
			partial += *(uint16_t *)data;
			partial += *(uint16_t *)(data + 2);
			partial += *(uint16_t *)(data + 4);
			partial += *(uint16_t *)(data + 6);
			partial += *(uint16_t *)(data + 8);
			partial += *(uint16_t *)(data + 10);
			partial += *(uint16_t *)(data + 12);
			partial += *(uint16_t *)(data + 14);
			partial += *(uint16_t *)(data + 16);
			partial += *(uint16_t *)(data + 18);
			partial += *(uint16_t *)(data + 20);
			partial += *(uint16_t *)(data + 22);
			partial += *(uint16_t *)(data + 24);
			partial += *(uint16_t *)(data + 26);
			partial += *(uint16_t *)(data + 28);
			partial += *(uint16_t *)(data + 30);
			data += 32;
			mlen -= 32;
			if (__predict_false(partial & 0xc0000000)) {
				if (needs_swap)
					partial = (partial << 8) + (partial >> 24);
				sum += (partial >> 16);
				sum += (partial & 0xffff);
				partial = 0;
			}
		}
		if (mlen & 16) {
			partial += *(uint16_t *)data;
			partial += *(uint16_t *)(data + 2);
			partial += *(uint16_t *)(data + 4);
			partial += *(uint16_t *)(data + 6);
			partial += *(uint16_t *)(data + 8);
			partial += *(uint16_t *)(data + 10);
			partial += *(uint16_t *)(data + 12);
			partial += *(uint16_t *)(data + 14);
			data += 16;
			mlen -= 16;
		}
		/*
		 * mlen is not updated below as the remaining tests
		 * are using bit masks, which are not affected.
		 */
		if (mlen & 8) {
			partial += *(uint16_t *)data;
			partial += *(uint16_t *)(data + 2);
			partial += *(uint16_t *)(data + 4);
			partial += *(uint16_t *)(data + 6);
			data += 8;
		}
		if (mlen & 4) {
			partial += *(uint16_t *)data;
			partial += *(uint16_t *)(data + 2);
			data += 4;
		}
		if (mlen & 2) {
			partial += *(uint16_t *)data;
			data += 2;
		}
		if (mlen & 1) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
			partial += *data;
#else
			partial += *data << 8;
#endif
			started_on_odd = !started_on_odd;
		}

		if (needs_swap)
			partial = (partial << 8) + (partial >> 24);
		sum += (partial >> 16) + (partial & 0xffff);
		/*
		 * Reduce sum to allow potential byte swap
		 * in the next iteration without carry.
		 */
		sum = (sum >> 16) + (sum & 0xffff);
	}
	final_acc = ((sum >> 16) & 0xffff) + (sum & 0xffff);
	final_acc = (final_acc >> 16) + (final_acc & 0xffff);
	return ~final_acc & 0xffff;
}

#else
/* 64bit version */
int
cpu_in_cksum(struct mbuf *m, int len, int off, uint32_t initial_sum)
{
	int mlen;
	uint64_t sum, partial;
	unsigned int final_acc;
	uint8_t *data;
	bool needs_swap, started_on_odd;

	KASSERT(len >= 0);
	KASSERT(off >= 0);

	needs_swap = false;
	started_on_odd = false;
	sum = initial_sum;

	for (;;) {
		if (__predict_false(m == NULL)) {
			printf("in_cksum: out of data\n");
			return -1;
		}
		mlen = m->m_len;
		if (mlen > off) {
			mlen -= off;
			data = mtod(m, uint8_t *) + off;
			goto post_initial_offset;
		}
		off -= mlen;
		if (len == 0)
			break;
		m = m->m_next;
	}

	for (; len > 0; m = m->m_next) {
		if (__predict_false(m == NULL)) {
			printf("in_cksum: out of data\n");
			return -1;
		}
		mlen = m->m_len;
		data = mtod(m, uint8_t *);
 post_initial_offset:
		if (mlen == 0)
			continue;
		if (mlen > len)
			mlen = len;
		len -= mlen;

		partial = 0;
		if ((uintptr_t)data & 1) {
			/* Align on word boundary */
			started_on_odd = !started_on_odd;
#if _BYTE_ORDER == _LITTLE_ENDIAN
			partial = *data << 8;
#else
			partial = *data;
#endif
			++data;
			--mlen;
		}
		needs_swap = started_on_odd;
		if ((uintptr_t)data & 2) {
			if (mlen < 2)
				goto trailing_bytes;
			partial += *(uint16_t *)data;
			data += 2;
			mlen -= 2;
		}
		while (mlen >= 64) {
			__builtin_prefetch(data + 32);
			__builtin_prefetch(data + 64);
			partial += *(uint32_t *)data;
			partial += *(uint32_t *)(data + 4);
			partial += *(uint32_t *)(data + 8);
			partial += *(uint32_t *)(data + 12);
			partial += *(uint32_t *)(data + 16);
			partial += *(uint32_t *)(data + 20);
			partial += *(uint32_t *)(data + 24);
			partial += *(uint32_t *)(data + 28);
			partial += *(uint32_t *)(data + 32);
			partial += *(uint32_t *)(data + 36);
			partial += *(uint32_t *)(data + 40);
			partial += *(uint32_t *)(data + 44);
			partial += *(uint32_t *)(data + 48);
			partial += *(uint32_t *)(data + 52);
			partial += *(uint32_t *)(data + 56);
			partial += *(uint32_t *)(data + 60);
			data += 64;
			mlen -= 64;
			if (__predict_false(partial & (3ULL << 62))) {
				if (needs_swap)
					partial = (partial << 8) + (partial >> 56);
				sum += (partial >> 32);
				sum += (partial & 0xffffffff);
				partial = 0;
			}
		}
		/*
		 * mlen is not updated below as the remaining tests
		 * are using bit masks, which are not affected.
		 */
		if (mlen & 32) {
			partial += *(uint32_t *)data;
			partial += *(uint32_t *)(data + 4);
			partial += *(uint32_t *)(data + 8);
			partial += *(uint32_t *)(data + 12);
			partial += *(uint32_t *)(data + 16);
			partial += *(uint32_t *)(data + 20);
			partial += *(uint32_t *)(data + 24);
			partial += *(uint32_t *)(data + 28);
			data += 32;
		}
		if (mlen & 16) {
			partial += *(uint32_t *)data;
			partial += *(uint32_t *)(data + 4);
			partial += *(uint32_t *)(data + 8);
			partial += *(uint32_t *)(data + 12);
			data += 16;
		}
		if (mlen & 8) {
			partial += *(uint32_t *)data;
			partial += *(uint32_t *)(data + 4);
			data += 8;
		}
		if (mlen & 4) {
			partial += *(uint32_t *)data;
			data += 4;
		}
		if (mlen & 2) {
			partial += *(uint16_t *)data;
			data += 2;
		}
 trailing_bytes:
		if (mlen & 1) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
			partial += *data;
#else
			partial += *data << 8;
#endif
			started_on_odd = !started_on_odd;
		}

		if (needs_swap)
			partial = (partial << 8) + (partial >> 56);
		sum += (partial >> 32) + (partial & 0xffffffff);
		/*
		 * Reduce sum to allow potential byte swap
		 * in the next iteration without carry.
		 */
		sum = (sum >> 32) + (sum & 0xffffffff);
	}
	final_acc = (sum >> 48) + ((sum >> 32) & 0xffff) +
	    ((sum >> 16) & 0xffff) + (sum & 0xffff);
	final_acc = (final_acc >> 16) + (final_acc & 0xffff);
	final_acc = (final_acc >> 16) + (final_acc & 0xffff);
	return ~final_acc & 0xffff;
}
#endif

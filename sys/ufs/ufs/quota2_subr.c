/* $NetBSD: quota2_subr.c,v 1.4 2011/06/07 14:56:13 bouyer Exp $ */
/*-
  * Copyright (c) 2010 Manuel Bouyer
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: quota2_subr.c,v 1.4 2011/06/07 14:56:13 bouyer Exp $");

#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ufs/quota2.h>

#ifndef _KERNEL
#include <string.h>
#endif

void
quota2_addfreeq2e(struct quota2_header *q2h, void *bp, uint64_t baseoff,
    uint64_t bsize, int ns)
{
	uint64_t blkoff = baseoff % bsize;
	int i, nq2e;
	struct quota2_entry *q2e;

	q2e = (void *)((char *)bp + blkoff);
	nq2e = (bsize - blkoff) / sizeof(*q2e);
	for (i = 0; i < nq2e; i++) {
		q2e[i].q2e_next = q2h->q2h_free;
		q2h->q2h_free = ufs_rw64(i * sizeof(*q2e) + baseoff, ns);
	}
}

void
quota2_create_blk0(uint64_t bsize, void *bp, int q2h_hash_shift, int type,
    int ns)
{
	struct quota2_header *q2h;
	const int quota2_hash_size = 1 << q2h_hash_shift;
	const int quota2_full_header_size = sizeof(struct quota2_header) +
	    sizeof(q2h->q2h_entries[0]) * quota2_hash_size;
	int i;

	memset(bp, 0, bsize);
	q2h = bp;
	q2h->q2h_magic_number = ufs_rw32(Q2_HEAD_MAGIC, ns);
	q2h->q2h_type = type;
	q2h->q2h_hash_shift = q2h_hash_shift;
	q2h->q2h_hash_size = ufs_rw16(quota2_hash_size, ns);
	/* setup defaut entry: unlimited, 7 days grace */
	for (i = 0; i < N_QL; i++) {
		q2h->q2h_defentry.q2e_val[i].q2v_hardlimit =
		    q2h->q2h_defentry.q2e_val[i].q2v_softlimit =
		    ufs_rw64(UQUAD_MAX, ns);
		q2h->q2h_defentry.q2e_val[i].q2v_grace =
		    ufs_rw64(7ULL * 24ULL * 3600ULL, ns);
	}

	/* first quota entry, after the hash table */
	quota2_addfreeq2e(q2h, bp, quota2_full_header_size, bsize, ns);
}

void
quota2_ufs_rwq2v(const struct quota2_val *s, struct quota2_val *d, int needswap)
{
	d->q2v_hardlimit = ufs_rw64(s->q2v_hardlimit, needswap);
	d->q2v_softlimit = ufs_rw64(s->q2v_softlimit, needswap);
	d->q2v_cur = ufs_rw64(s->q2v_cur, needswap);
	d->q2v_time = ufs_rw64(s->q2v_time, needswap);
	d->q2v_grace = ufs_rw64(s->q2v_grace, needswap);
}

void
quota2_ufs_rwq2e(const struct quota2_entry *s, struct quota2_entry *d,
int needswap)
{
	quota2_ufs_rwq2v(&s->q2e_val[QL_BLOCK], &d->q2e_val[QL_BLOCK],
	    needswap);
	quota2_ufs_rwq2v(&s->q2e_val[QL_FILE], &d->q2e_val[QL_FILE],
	    needswap);
	d->q2e_uid = ufs_rw32(s->q2e_uid, needswap);
}

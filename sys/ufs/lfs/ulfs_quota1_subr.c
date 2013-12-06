/*	$NetBSD: ulfs_quota1_subr.c,v 1.3 2013/06/06 00:49:28 dholland Exp $	*/
/*  from NetBSD: quota1_subr.c,v 1.7 2012/01/29 06:23:20 dholland Exp  */

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
__KERNEL_RCSID(0, "$NetBSD: ulfs_quota1_subr.c,v 1.3 2013/06/06 00:49:28 dholland Exp $");

#include <sys/types.h>
#include <machine/limits.h>

#include <sys/quota.h>
#include <ufs/lfs/ulfs_quota1.h>

static uint64_t
dqblk2q2e_limit(uint32_t lim)
{
	if (lim == 0)
		return UQUAD_MAX;
	else
		return (lim - 1);
}

static uint32_t
q2e2dqblk_limit(uint64_t lim)
{
	if (lim == UQUAD_MAX)
		return 0;
	else
		return (lim + 1);
}

void
lfs_dqblk_to_quotavals(const struct dqblk *dqblk,
		   struct quotaval *blocks, struct quotaval *files)
{
	/* XXX is qv_grace getting handled correctly? */

	blocks->qv_hardlimit  = dqblk2q2e_limit(dqblk->dqb_bhardlimit);
	blocks->qv_softlimit  = dqblk2q2e_limit(dqblk->dqb_bsoftlimit);
	blocks->qv_usage      = dqblk->dqb_curblocks;
	blocks->qv_expiretime = dqblk->dqb_btime;

	files->qv_hardlimit  = dqblk2q2e_limit(dqblk->dqb_ihardlimit);
	files->qv_softlimit  = dqblk2q2e_limit(dqblk->dqb_isoftlimit);
	files->qv_usage      = dqblk->dqb_curinodes;
	files->qv_expiretime = dqblk->dqb_itime;
}

void
lfs_quotavals_to_dqblk(const struct quotaval *blocks, const struct quotaval *files,
		   struct dqblk *dqblk)
{
	/* XXX is qv_grace getting handled correctly? */

	dqblk->dqb_bhardlimit = q2e2dqblk_limit(blocks->qv_hardlimit);
	dqblk->dqb_bsoftlimit = q2e2dqblk_limit(blocks->qv_softlimit);
	dqblk->dqb_curblocks  = blocks->qv_usage;
	dqblk->dqb_btime      = blocks->qv_expiretime;

	dqblk->dqb_ihardlimit = q2e2dqblk_limit(files->qv_hardlimit);
	dqblk->dqb_isoftlimit = q2e2dqblk_limit(files->qv_softlimit);
	dqblk->dqb_curinodes  = files->qv_usage;
	dqblk->dqb_itime      = files->qv_expiretime;
}


/* $NetBSD: quota1_subr.c,v 1.6 2011/11/25 16:55:05 dholland Exp $ */
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
__KERNEL_RCSID(0, "$NetBSD: quota1_subr.c,v 1.6 2011/11/25 16:55:05 dholland Exp $");

#include <sys/types.h>
#include <machine/limits.h>

#include <sys/quota.h>
#include <quota/quotaprop.h>
#include <ufs/ufs/quota1.h>

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
dqblk_to_quotaval(const struct dqblk *dqblk, struct quotaval *qv)
{
	/* XXX is qv_grace getting handled correctly? */

	qv[QUOTA_LIMIT_BLOCK].qv_hardlimit =
	    dqblk2q2e_limit(dqblk->dqb_bhardlimit);
	qv[QUOTA_LIMIT_BLOCK].qv_softlimit =
	    dqblk2q2e_limit(dqblk->dqb_bsoftlimit);
	qv[QUOTA_LIMIT_BLOCK].qv_usage       = dqblk->dqb_curblocks;
	qv[QUOTA_LIMIT_BLOCK].qv_expiretime      = dqblk->dqb_btime;

	qv[QUOTA_LIMIT_FILE].qv_hardlimit =
	    dqblk2q2e_limit(dqblk->dqb_ihardlimit);
	qv[QUOTA_LIMIT_FILE].qv_softlimit =
	    dqblk2q2e_limit(dqblk->dqb_isoftlimit);
	qv[QUOTA_LIMIT_FILE].qv_usage       = dqblk->dqb_curinodes;
	qv[QUOTA_LIMIT_FILE].qv_expiretime      = dqblk->dqb_itime;
}

void
quotaval_to_dqblk(const struct quotaval *qv, struct dqblk *dqblk)
{
	/* XXX is qv_grace getting handled correctly? */

	dqblk->dqb_bhardlimit =
	    q2e2dqblk_limit(qv[QUOTA_LIMIT_BLOCK].qv_hardlimit);
	dqblk->dqb_bsoftlimit =
	    q2e2dqblk_limit(qv[QUOTA_LIMIT_BLOCK].qv_softlimit);
	dqblk->dqb_curblocks  = qv[QUOTA_LIMIT_BLOCK].qv_usage;
	dqblk->dqb_btime      = qv[QUOTA_LIMIT_BLOCK].qv_expiretime;

	dqblk->dqb_ihardlimit =
	    q2e2dqblk_limit(qv[QUOTA_LIMIT_FILE].qv_hardlimit);
	dqblk->dqb_isoftlimit =
	    q2e2dqblk_limit(qv[QUOTA_LIMIT_FILE].qv_softlimit);
	dqblk->dqb_curinodes  = qv[QUOTA_LIMIT_FILE].qv_usage;
	dqblk->dqb_itime      = qv[QUOTA_LIMIT_FILE].qv_expiretime;
}


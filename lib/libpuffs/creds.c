/*	$NetBSD: creds.c,v 1.14.12.1 2009/11/28 16:01:03 bouyer Exp $	*/

/*
 * Copyright (c) 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the Ulla Tuominen Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: creds.c,v 1.14.12.1 2009/11/28 16:01:03 bouyer Exp $");
#endif /* !lint */

/*
 * Interface for dealing with credits.
 */

#include <sys/types.h>
#include <sys/param.h>

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "puffs.h"
#include "puffs_priv.h"

#define UUCCRED(a) (a->pkcr_type == PUFFCRED_TYPE_UUC)
#define INTCRED(a) (a->pkcr_type == PUFFCRED_TYPE_INTERNAL)

int
puffs_cred_getuid(const struct puffs_cred *pcr, uid_t *ruid)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	if (!UUCCRED(pkcr)) {
		errno = EOPNOTSUPP;
		return -1;
	}
	*ruid = pkcr->pkcr_uuc.cr_uid;

	return 0;
}

int
puffs_cred_getgid(const struct puffs_cred *pcr, gid_t *rgid)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	if (!UUCCRED(pkcr)) {
		errno = EOPNOTSUPP;
		return -1;
	}
	*rgid = pkcr->pkcr_uuc.cr_gid;

	return 0;
}

int
puffs_cred_getgroups(const struct puffs_cred *pcr, gid_t *rgids, short *ngids)
{
	PUFFS_MAKEKCRED(pkcr, pcr);
	size_t ncopy;

	if (!UUCCRED(pkcr)) {
		errno = EOPNOTSUPP;
		*ngids = 0;
		return -1;
	}

	ncopy = MIN(*ngids, pkcr->pkcr_uuc.cr_ngroups);
	(void)memcpy(rgids, pkcr->pkcr_uuc.cr_groups, sizeof(gid_t) * ncopy);
	*ngids = (short)ncopy;

	return 0;
}

bool
puffs_cred_isuid(const struct puffs_cred *pcr, uid_t uid)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	return UUCCRED(pkcr) && pkcr->pkcr_uuc.cr_uid == uid;
}

bool
puffs_cred_hasgroup(const struct puffs_cred *pcr, gid_t gid)
{
	PUFFS_MAKEKCRED(pkcr, pcr);
	short i;

	if (!UUCCRED(pkcr))
		return false;

	if (pkcr->pkcr_uuc.cr_gid == gid)
		return true;
	for (i = 0; i < pkcr->pkcr_uuc.cr_ngroups; i++)
		if (pkcr->pkcr_uuc.cr_groups[i] == gid)
			return true;

	return false;
}

bool
puffs_cred_isregular(const struct puffs_cred *pcr)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	return UUCCRED(pkcr);
}

bool
puffs_cred_iskernel(const struct puffs_cred *pcr)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	return INTCRED(pkcr) && pkcr->pkcr_internal == PUFFCRED_CRED_NOCRED;
}

bool
puffs_cred_isfs(const struct puffs_cred *pcr)
{
	PUFFS_MAKEKCRED(pkcr, pcr);

	return INTCRED(pkcr) && pkcr->pkcr_internal == PUFFCRED_CRED_FSCRED;
}

bool
puffs_cred_isjuggernaut(const struct puffs_cred *pcr)
{

	return puffs_cred_isuid(pcr, 0) || puffs_cred_iskernel(pcr)
	    || puffs_cred_isfs(pcr);
}

/*
 * Generic routine for checking file access rights.  Modeled after
 * vaccess() in the kernel.
 */
int
puffs_access(enum vtype type, mode_t file_mode, uid_t uid, gid_t gid,
	mode_t acc_mode, const struct puffs_cred *pcr)
{
	mode_t mask;

	/* megapower */
	if (puffs_cred_iskernel(pcr) || puffs_cred_isfs(pcr))
		return 0;

	/* superuser, allow all except exec if *ALL* exec bits are unset */
	if (puffs_cred_isuid(pcr, 0)) {
		if ((acc_mode & PUFFS_VEXEC) && type != VDIR &&
		    (file_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0)
			return EACCES;
		return 0;
	}

	mask = 0;
	/* owner */
	if (puffs_cred_isuid(pcr, uid)) {
		if (acc_mode & PUFFS_VEXEC)
			mask |= S_IXUSR;
		if (acc_mode & PUFFS_VREAD)
			mask |= S_IRUSR;
		if (acc_mode & PUFFS_VWRITE)
			mask |= S_IWUSR;
	/* group */
	} else if (puffs_cred_hasgroup(pcr, gid)) {
		if (acc_mode & PUFFS_VEXEC)
			mask |= S_IXGRP;
		if (acc_mode & PUFFS_VREAD)
			mask |= S_IRGRP;
		if (acc_mode & PUFFS_VWRITE)
			mask |= S_IWGRP;
	/* other */
	} else {
		if (acc_mode & PUFFS_VEXEC)
			mask |= S_IXOTH;
		if (acc_mode & PUFFS_VREAD)
			mask |= S_IROTH;
		if (acc_mode & PUFFS_VWRITE)
			mask |= S_IWOTH;
	}

	if ((file_mode & mask) == mask)
		return 0;
	else
		return EACCES;
}

int
puffs_access_chown(uid_t owner, gid_t group, uid_t newowner, gid_t newgroup,
	const struct puffs_cred *pcr)
{

	if (newowner == (uid_t)PUFFS_VNOVAL)
		newowner = owner;
	if (newgroup == (gid_t)PUFFS_VNOVAL)
		newgroup = group;

	if ((!puffs_cred_isuid(pcr, owner) || newowner != owner ||
	    ((newgroup != group && !puffs_cred_hasgroup(pcr, newgroup))))
	    && !puffs_cred_isjuggernaut(pcr))
		return EPERM;

	return 0;
}

int
puffs_access_chmod(uid_t owner, gid_t group, enum vtype type, mode_t mode,
	const struct puffs_cred *pcr)
{

	if (!puffs_cred_isuid(pcr, owner) && !puffs_cred_isjuggernaut(pcr))
		return EPERM;

	if (!puffs_cred_isjuggernaut(pcr)) {
		if (type != VDIR && (mode & S_ISTXT))
			return EFTYPE;
		if (!puffs_cred_hasgroup(pcr, group) && (mode & S_ISGID))
			return EPERM;
	}

	return 0;
}

int
puffs_access_times(uid_t uid, gid_t gid, mode_t mode, int va_utimes_null,
	const struct puffs_cred *pcr)
{

	if (!puffs_cred_isuid(pcr, uid) && !puffs_cred_isjuggernaut(pcr)
	    && (va_utimes_null == 0
	      || puffs_access(VNON, mode, uid, gid, PUFFS_VWRITE, pcr) != 0))
		return EPERM;

	return 0;
}

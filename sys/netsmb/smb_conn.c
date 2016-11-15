/*	$NetBSD: smb_conn.c,v 1.29 2012/04/29 20:27:31 dsl Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 2000-2001 Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_conn.c,v 1.3 2001/12/02 08:47:29 bp Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_conn.c,v 1.29 2012/04/29 20:27:31 dsl Exp $");

/*
 * Connection engine.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>		/* for M_SONAME */
#include <sys/kauth.h>

#include <netsmb/iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>

static struct smb_connobj smb_vclist;
static int smb_vcnext = 1;	/* next unique id for VC */
static kauth_listener_t smb_listener;

MALLOC_DEFINE(M_SMBCONN, "SMB conn", "SMB connection");
MALLOC_DECLARE(M_SMBCONN);

static void smb_co_init(struct smb_connobj *cp, int level, const char *objname);
static void smb_co_done(struct smb_connobj *cp);

static int  smb_vc_disconnect(struct smb_vc *vcp);
static void smb_vc_free(struct smb_connobj *cp);
static void smb_vc_gone(struct smb_connobj *cp, struct smb_cred *scred);
static smb_co_free_t smb_share_free;
static smb_co_gone_t smb_share_gone;

static int
smb_listener_cb(kauth_cred_t cred, kauth_action_t action, void *cookie,
    void *arg0, void *arg1, void *arg2, void *arg3)
{
	int result, ismember = 0;
	enum kauth_network_req req;

	if (action != KAUTH_NETWORK_SMB)
		return KAUTH_RESULT_DEFER;

	result = KAUTH_RESULT_DEFER;
	req = (enum kauth_network_req)arg0;

	switch (req) {
	case KAUTH_REQ_NETWORK_SMB_SHARE_ACCESS: {
		struct smb_share *ssp = arg1;
		mode_t mode = (mode_t)(uintptr_t)arg2;

		/* Owner can access. */
		if (kauth_cred_geteuid(cred) == ssp->ss_uid) {
			result = KAUTH_RESULT_ALLOW;
			break;
		}

		/* Try group permissions if member or other if not. */
		mode >>= 3;
		if (kauth_cred_ismember_gid(cred, ssp->ss_grp, &ismember) != 0 ||
		    !ismember)
			mode >>= 3;

		if ((ssp->ss_mode & mode) == mode)
			result = KAUTH_RESULT_ALLOW;

		break;
		}

	case KAUTH_REQ_NETWORK_SMB_SHARE_CREATE: {
		struct smb_sharespec *shspec = arg1;

		/*
		 * Only superuser can create shares with different uid and gid
		 */
		if (shspec->owner != SMBM_ANY_OWNER &&
		    shspec->owner != kauth_cred_geteuid(cred))
			break;
		if (shspec->group != SMBM_ANY_GROUP &&
		    (kauth_cred_ismember_gid(cred, shspec->group, &ismember) != 0 || !ismember))
			break;

		result = KAUTH_RESULT_ALLOW;

		break;
		}

	case KAUTH_REQ_NETWORK_SMB_VC_ACCESS: {
		struct smb_vc *vcp = arg1;
		mode_t mode = (mode_t)(uintptr_t)arg2;

		/* Owner can access. */
		if (kauth_cred_geteuid(cred) == vcp->vc_uid) {
			result = KAUTH_RESULT_ALLOW;
			break;
		}

		/* Try group permissions if member or other if not. */
		mode >>= 3;
		if (kauth_cred_ismember_gid(cred, vcp->vc_grp, &ismember) != 0 ||
		    !ismember)
			mode >>= 3;

		if ((vcp->vc_mode & mode) == mode)
			result = KAUTH_RESULT_ALLOW;

		break;
		}

	case KAUTH_REQ_NETWORK_SMB_VC_CREATE: {
		struct smb_vcspec *vcspec = arg1;

		/*
		 * Only superuser can create VCs with different uid and gid
		 */
		if (vcspec->owner != SMBM_ANY_OWNER &&
		    vcspec->owner != kauth_cred_geteuid(cred))
			break;
		if (vcspec->group != SMBM_ANY_GROUP &&
		    (kauth_cred_ismember_gid(cred, vcspec->group, &ismember) != 0 || !ismember))
			break;

		result = KAUTH_RESULT_ALLOW;

		break;
		}

	default:
		break;
	}

	return result;
}

int
smb_sm_init(void)
{

	smb_co_init(&smb_vclist, SMBL_SM, "smbsm");
	mutex_enter(&smb_vclist.co_interlock);
	smb_co_unlock(&smb_vclist);
	mutex_exit(&smb_vclist.co_interlock);
	smb_listener = kauth_listen_scope(KAUTH_SCOPE_NETWORK,
	    smb_listener_cb, NULL);
	return 0;
}

int
smb_sm_done(void)
{

	/* XXX: hold the mutex */
#ifdef DIAGNOSTIC
	if (smb_vclist.co_usecount > 1)
		panic("%d connections still active", smb_vclist.co_usecount - 1);
#endif
	smb_co_done(&smb_vclist);
	kauth_unlisten_scope(smb_listener);
	return 0;
}

static int
smb_sm_lockvclist(void)
{
	int error;

	mutex_enter(&smb_vclist.co_interlock);
	error = smb_co_lock(&smb_vclist);
	mutex_exit(&smb_vclist.co_interlock);

	return error;
}

static void
smb_sm_unlockvclist(void)
{

	mutex_enter(&smb_vclist.co_interlock);
	smb_co_unlock(&smb_vclist);
	mutex_exit(&smb_vclist.co_interlock);
}

static int
smb_sm_lookupint(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc **vcpp)
{
	struct smb_connobj *ocp;
	int exact = 1;
	int fail = 1;

	vcspec->shspec = shspec;
	SMBCO_FOREACH(ocp, &smb_vclist) {
		struct smb_vc *vcp = (struct smb_vc *)ocp;

		if (smb_vc_lock(vcp) != 0)
			continue;

		do {
			if ((ocp->co_flags & SMBV_PRIVATE) ||
			    !CONNADDREQ(vcp->vc_paddr, vcspec->sap) ||
			    strcmp(vcp->vc_username, vcspec->username) != 0)
				break;

			if (vcspec->owner != SMBM_ANY_OWNER) {
				if (vcp->vc_uid != vcspec->owner)
					break;
			} else
				exact = 0;
			if (vcspec->group != SMBM_ANY_GROUP) {
				if (vcp->vc_grp != vcspec->group)
					break;
			} else
				exact = 0;

			if (vcspec->mode & SMBM_EXACT) {
				if (!exact ||
				    (vcspec->mode & SMBM_MASK) != vcp->vc_mode)
					break;
			}
			if (smb_vc_access(vcp, scred, vcspec->mode) != 0)
				break;
			vcspec->ssp = NULL;
			if (shspec
			    &&smb_vc_lookupshare(vcp, shspec, scred, &vcspec->ssp) != 0)
				break;

			/* if we get here, all checks succeeded */
			smb_vc_ref(vcp);
			*vcpp = vcp;
			fail = 0;
			goto out;
		} while(0);

		smb_vc_unlock(vcp);
	}

    out:
	return fail;
}

int
smb_sm_lookup(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	int fail, error;

	*vcpp = vcp = NULL;

	error = smb_sm_lockvclist();
	if (error)
		return error;
	fail = smb_sm_lookupint(vcspec, shspec, scred, vcpp);
	if (!fail || (vcspec->flags & SMBV_CREATE) == 0) {
		smb_sm_unlockvclist();
		return 0;
	}
	fail = smb_sm_lookupint(vcspec, NULL, scred, &vcp);
	if (fail) {
		error = smb_vc_create(vcspec, scred, &vcp);
		if (error)
			goto out;
		error = smb_vc_connect(vcp, scred);
		if (error)
			goto out;
	}
	if (shspec == NULL)
		goto out;
	error = smb_share_create(vcp, shspec, scred, &ssp);
	if (error)
		goto out;
	error = smb_smb_treeconnect(ssp, scred);
	if (error == 0)
		vcspec->ssp = ssp;
	else
		smb_share_put(ssp, scred);
out:
	smb_sm_unlockvclist();
	if (error == 0)
		*vcpp = vcp;
	else if (vcp)
		smb_vc_put(vcp, scred);
	return error;
}

/*
 * Common code for connection object
 */
static void
smb_co_init(struct smb_connobj *cp, int level, const char *objname)
{
	SLIST_INIT(&cp->co_children);
	smb_sl_init(&cp->co_interlock, objname);
	cv_init(&cp->co_lock, "smblock");
	cp->co_lockcnt = 0;
	cp->co_locker = NULL;
	cp->co_level = level;
	cp->co_usecount = 1;
	mutex_enter(&cp->co_interlock);
	smb_co_lock(cp);
	mutex_exit(&cp->co_interlock);
}

static void
smb_co_done(struct smb_connobj *cp)
{
	smb_sl_destroy(&cp->co_interlock);
	cv_destroy(&cp->co_lock);
}

static void
smb_co_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_connobj *parent;

	if (cp->co_gone)
		cp->co_gone(cp, scred);
	parent = cp->co_parent;
	if (parent) {
		mutex_enter(&parent->co_interlock);
		smb_co_lock(parent);
		mutex_exit(&parent->co_interlock);
		SLIST_REMOVE(&parent->co_children, cp, smb_connobj, co_next);
		smb_co_put(parent, scred);
	}
	if (cp->co_free)
		cp->co_free(cp);
}

void
smb_co_ref(struct smb_connobj *cp)
{

	mutex_enter(&cp->co_interlock);
	cp->co_usecount++;
	mutex_exit(&cp->co_interlock);
}

void
smb_co_rele(struct smb_connobj *cp, struct smb_cred *scred)
{
	mutex_enter(&cp->co_interlock);
	smb_co_unlock(cp);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
		mutex_exit(&cp->co_interlock);
		return;
	}
#ifdef DIAGNOSTIC
	if (cp->co_usecount == 0)
		panic("negative use_count for object %d", cp->co_level);
#endif
	cp->co_usecount--;
	cp->co_flags |= SMBO_GONE;
	mutex_exit(&cp->co_interlock);

	smb_co_gone(cp, scred);
}

int
smb_co_get(struct smb_connobj *cp, struct smb_cred *scred)
{
	int error;

	KASSERT(mutex_owned(&cp->co_interlock));
	cp->co_usecount++;
	error = smb_co_lock(cp);
	if (error)
		cp->co_usecount--;
	return error;
}

void
smb_co_put(struct smb_connobj *cp, struct smb_cred *scred)
{

	mutex_enter(&cp->co_interlock);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
	} else if (cp->co_usecount == 1) {
		cp->co_usecount--;
		cp->co_flags |= SMBO_GONE;
	}
#ifdef DIAGNOSTIC
	else
		panic("smb_co_put: negative usecount");
#endif
	smb_co_unlock(cp);
	mutex_exit(&cp->co_interlock);
	if ((cp->co_flags & SMBO_GONE) == 0)
		return;
	smb_co_gone(cp, scred);
}

int
smb_co_lock(struct smb_connobj *cp)
{

	KASSERT(mutex_owned(&cp->co_interlock));

	for (;;) {
		if (cp->co_flags & SMBO_GONE)
			return EINVAL;
		if (cp->co_locker == NULL) {
			cp->co_locker = curlwp;
			return 0;
		}
		if (cp->co_locker == curlwp) {
			cp->co_lockcnt++;
			return 0;
		}
		cv_wait(&cp->co_lock, &cp->co_interlock);
	}
}

void
smb_co_unlock(struct smb_connobj *cp)
{

	KASSERT(mutex_owned(&cp->co_interlock));
	KASSERT(cp->co_locker == curlwp);

	if (cp->co_lockcnt != 0) {
		cp->co_lockcnt--;
		return;
	}
	cp->co_locker = NULL;
	cv_signal(&cp->co_lock);
}

static void
smb_co_addchild(struct smb_connobj *parent, struct smb_connobj *child)
{

	smb_co_ref(parent);
	SLIST_INSERT_HEAD(&parent->co_children, child, co_next);
	child->co_parent = parent;
}

/*
 * Session implementation
 */

int
smb_vc_create(struct smb_vcspec *vcspec,
	struct smb_cred *scred, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	kauth_cred_t cred = scred->scr_cred;
	uid_t uid = vcspec->owner;
	gid_t gid = vcspec->group;
	uid_t realuid;
	char *domain = vcspec->domain;
	int error;

	error = kauth_authorize_network(cred, KAUTH_NETWORK_SMB,
	    KAUTH_REQ_NETWORK_SMB_VC_CREATE, vcspec, NULL, NULL);
	if (error)
		return EPERM;

	realuid = kauth_cred_geteuid(cred);

	vcp = smb_zmalloc(sizeof(*vcp), M_SMBCONN, M_WAITOK);
	smb_co_init(VCTOCP(vcp), SMBL_VC, "smb_vc");
	vcp->obj.co_free = smb_vc_free;
	vcp->obj.co_gone = smb_vc_gone;
	vcp->vc_number = smb_vcnext++;
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	vcp->vc_mode = vcspec->rights & SMBM_MASK;
	vcp->obj.co_flags = vcspec->flags & (SMBV_PRIVATE | SMBV_SINGLESHARE);
	vcp->vc_tdesc = &smb_tran_nbtcp_desc;

	if (uid == SMBM_ANY_OWNER)
		uid = realuid;
	if (gid == SMBM_ANY_GROUP)
		gid = kauth_cred_group(cred, 0);
	vcp->vc_uid = uid;
	vcp->vc_grp = gid;

	smb_sl_init(&vcp->vc_stlock, "vcstlock");
	error = ENOMEM;
	if ((vcp->vc_paddr = dup_sockaddr(vcspec->sap, 1)) == NULL)
		goto fail;

	if ((vcp->vc_laddr = dup_sockaddr(vcspec->lap, 1)) == NULL)
		goto fail;

	if ((vcp->vc_pass = smb_strdup(vcspec->pass)) == NULL)
		goto fail;

	vcp->vc_domain = smb_strdup((domain && domain[0]) ? domain : "NODOMAIN");
	if (vcp->vc_domain == NULL)
		goto fail;

	if ((vcp->vc_srvname = smb_strdup(vcspec->srvname)) == NULL)
		goto fail;

	if ((vcp->vc_username = smb_strdup(vcspec->username)) == NULL)
		goto fail;

#define ithrow(cmd)				\
		if ((error = cmd))		\
			goto fail

	ithrow(iconv_open("tolower", vcspec->localcs, &vcp->vc_tolower));
	ithrow(iconv_open("toupper", vcspec->localcs, &vcp->vc_toupper));
	if (vcspec->servercs[0]) {
		ithrow(iconv_open(vcspec->servercs, vcspec->localcs,
		    &vcp->vc_toserver));
		ithrow(iconv_open(vcspec->localcs, vcspec->servercs,
		    &vcp->vc_tolocal));
	}

	ithrow(smb_iod_create(vcp));

#undef ithrow

	/* all is well, return success */
	*vcpp = vcp;
	smb_co_addchild(&smb_vclist, VCTOCP(vcp));

	return 0;

    fail:
	smb_vc_put(vcp, scred);
	return (error);

}

static void
smb_vc_free(struct smb_connobj *cp)
{
	struct smb_vc *vcp = CPTOVC(cp);

	if (vcp->vc_iod)
		smb_iod_destroy(vcp->vc_iod);
	SMB_STRFREE(vcp->vc_username);
	SMB_STRFREE(vcp->vc_srvname);
	SMB_STRFREE(vcp->vc_pass);
	SMB_STRFREE(vcp->vc_domain);
	if (vcp->vc_paddr)
		free(vcp->vc_paddr, M_SONAME);
	if (vcp->vc_laddr)
		free(vcp->vc_laddr, M_SONAME);
	if (vcp->vc_tolower)
		iconv_close(vcp->vc_tolower);
	if (vcp->vc_toupper)
		iconv_close(vcp->vc_toupper);
	if (vcp->vc_tolocal)
		iconv_close(vcp->vc_tolocal);
	if (vcp->vc_toserver)
		iconv_close(vcp->vc_toserver);
	smb_co_done(VCTOCP(vcp));
	smb_sl_destroy(&vcp->vc_stlock);
	free(vcp, M_SMBCONN);
}

/*
 * Called when use count of VC dropped to zero.
 * VC should be locked on enter with LK_DRAIN.
 */
static void
smb_vc_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_vc *vcp = CPTOVC(cp);

	smb_vc_disconnect(vcp);
}

void
smb_vc_ref(struct smb_vc *vcp)
{
	smb_co_ref(VCTOCP(vcp));
}

void
smb_vc_rele(struct smb_vc *vcp, struct smb_cred *scred)
{
	smb_co_rele(VCTOCP(vcp), scred);
}

int
smb_vc_get(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_connobj *cp = VCTOCP(vcp);
	int error;

	mutex_enter(&cp->co_interlock);
	error = smb_co_get(cp, scred);
	mutex_exit(&cp->co_interlock);

	return error;
}

void
smb_vc_put(struct smb_vc *vcp, struct smb_cred *scred)
{
	smb_co_put(VCTOCP(vcp), scred);
}

int
smb_vc_lock(struct smb_vc *vcp)
{
	struct smb_connobj *cp = VCTOCP(vcp);
	int error;

	mutex_enter(&cp->co_interlock);
	error = smb_co_lock(cp);
	mutex_exit(&cp->co_interlock);

	return error;
}

void
smb_vc_unlock(struct smb_vc *vcp)
{
	struct smb_connobj *cp = VCTOCP(vcp);

	mutex_enter(&cp->co_interlock);
	smb_co_unlock(cp);
	mutex_exit(&cp->co_interlock);
}


int
smb_vc_access(struct smb_vc *vcp, struct smb_cred *scred, mode_t mode)
{
	kauth_cred_t cred = scred->scr_cred;
	int error;

	error = kauth_authorize_network(cred, KAUTH_NETWORK_SMB,
	    KAUTH_REQ_NETWORK_SMB_VC_ACCESS, vcp, KAUTH_ARG(mode), NULL);
	if (error)
		return EACCES;

	return 0;
}

static int
smb_vc_cmpshare(struct smb_share *ssp, struct smb_sharespec *dp)
{
	int exact = 1;

	if (strcmp(ssp->ss_name, dp->name) != 0)
		return 1;
	if (dp->owner != SMBM_ANY_OWNER) {
		if (ssp->ss_uid != dp->owner)
			return 1;
	} else
		exact = 0;
	if (dp->group != SMBM_ANY_GROUP) {
		if (ssp->ss_grp != dp->group)
			return 1;
	} else
		exact = 0;

	if (dp->mode & SMBM_EXACT) {
		if (!exact)
			return 1;
		return (dp->mode & SMBM_MASK) == ssp->ss_mode ? 0 : 1;
	}
	if (smb_share_access(ssp, dp->scred, dp->mode) != 0)
		return 1;
	return 0;
}

/*
 * Lookup share in the given VC. Share referenced and locked on return.
 * VC expected to be locked on entry and will be left locked on exit.
 */
int
smb_vc_lookupshare(struct smb_vc *vcp, struct smb_sharespec *dp,
	struct smb_cred *scred,	struct smb_share **sspp)
{
	struct smb_connobj *osp;
	struct smb_share *ssp = NULL;
	int error;

	*sspp = NULL;
	dp->scred = scred;
	SMBCO_FOREACH(osp, VCTOCP(vcp)) {
		ssp = (struct smb_share *)osp;
		error = smb_share_lock(ssp);
		if (error)
			continue;
		if (smb_vc_cmpshare(ssp, dp) == 0)
			break;
		smb_share_unlock(ssp);
	}
	if (ssp) {
		smb_share_ref(ssp);
		*sspp = ssp;
		error = 0;
	} else
		error = ENOENT;
	return error;
}

int
smb_vc_connect(struct smb_vc *vcp, struct smb_cred *scred)
{

	return smb_iod_request(vcp->vc_iod, SMBIOD_EV_CONNECT | SMBIOD_EV_SYNC, NULL);
}

/*
 * Destroy VC to server, invalidate shares linked with it.
 * Transport should be locked on entry.
 */
int
smb_vc_disconnect(struct smb_vc *vcp)
{

	smb_iod_request(vcp->vc_iod, SMBIOD_EV_DISCONNECT | SMBIOD_EV_SYNC, NULL);
	return 0;
}

static const char * const smb_emptypass = "";

const char *
smb_vc_getpass(struct smb_vc *vcp)
{
	if (vcp->vc_pass)
		return vcp->vc_pass;
	return smb_emptypass;
}


u_short
smb_vc_nextmid(struct smb_vc *vcp)
{
	u_short r;

	mutex_enter(&vcp->obj.co_interlock);
	r = vcp->vc_mid++;
	mutex_exit(&vcp->obj.co_interlock);
	return r;
}

/*
 * Share implementation
 */
/*
 * Allocate share structure and attach it to the given VC
 * Connection expected to be locked on entry. Share will be returned
 * in locked state.
 */
int
smb_share_create(struct smb_vc *vcp, struct smb_sharespec *shspec,
	struct smb_cred *scred, struct smb_share **sspp)
{
	struct smb_share *ssp;
	kauth_cred_t cred = scred->scr_cred;
	uid_t realuid;
	uid_t uid = shspec->owner;
	gid_t gid = shspec->group;
	int error;

	error = kauth_authorize_network(cred, KAUTH_NETWORK_SMB,
	    KAUTH_REQ_NETWORK_SMB_SHARE_CREATE, shspec, NULL, NULL);
	if (error)
		return EPERM;

	realuid = kauth_cred_geteuid(cred);

	error = smb_vc_lookupshare(vcp, shspec, scred, &ssp);
	if (!error) {
		smb_share_put(ssp, scred);
		return EEXIST;
	}
	if (uid == SMBM_ANY_OWNER)
		uid = realuid;
	if (gid == SMBM_ANY_GROUP)
		gid = kauth_cred_group(cred, 0);
	ssp = smb_zmalloc(sizeof(*ssp), M_SMBCONN, M_WAITOK);
	smb_co_init(SSTOCP(ssp), SMBL_SHARE, "smbss");
	ssp->obj.co_free = smb_share_free;
	ssp->obj.co_gone = smb_share_gone;
	smb_sl_init(&ssp->ss_stlock, "ssstlock");
	ssp->ss_name = smb_strdup(shspec->name);
	if (shspec->pass && shspec->pass[0])
		ssp->ss_pass = smb_strdup(shspec->pass);
	ssp->ss_type = shspec->stype;
	ssp->ss_tid = SMB_TID_UNKNOWN;
	ssp->ss_uid = uid;
	ssp->ss_grp = gid;
	ssp->ss_mode = shspec->rights & SMBM_MASK;
	smb_co_addchild(VCTOCP(vcp), SSTOCP(ssp));
	*sspp = ssp;
	return 0;
}

static void
smb_share_free(struct smb_connobj *cp)
{
	struct smb_share *ssp = CPTOSS(cp);

	SMB_STRFREE(ssp->ss_name);
	SMB_STRFREE(ssp->ss_pass);
	smb_sl_destroy(&ssp->ss_stlock);
	smb_co_done(SSTOCP(ssp));
	free(ssp, M_SMBCONN);
}

static void
smb_share_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_share *ssp = CPTOSS(cp);

	smb_smb_treedisconnect(ssp, scred);
}

void
smb_share_ref(struct smb_share *ssp)
{
	smb_co_ref(SSTOCP(ssp));
}

void
smb_share_rele(struct smb_share *ssp, struct smb_cred *scred)
{
	smb_co_rele(SSTOCP(ssp), scred);
}

int
smb_share_get(struct smb_share *ssp, struct smb_cred *scred)
{
	struct smb_connobj *cp = SSTOCP(ssp);
	int error;

	mutex_enter(&cp->co_interlock);
	error = smb_co_get(cp, scred);
	mutex_exit(&cp->co_interlock);

	return error;
}

void
smb_share_put(struct smb_share *ssp, struct smb_cred *scred)
{
	smb_co_put(SSTOCP(ssp), scred);
}

int
smb_share_lock(struct smb_share *ssp)
{
	struct smb_connobj *cp = SSTOCP(ssp);
	int error;

	mutex_enter(&cp->co_interlock);
	error = smb_co_lock(cp);
	mutex_exit(&cp->co_interlock);

	return error;
}

void
smb_share_unlock(struct smb_share *ssp)
{
	struct smb_connobj *cp = SSTOCP(ssp);

	mutex_enter(&cp->co_interlock);
	smb_co_unlock(cp);
	mutex_exit(&cp->co_interlock);
}

int
smb_share_access(struct smb_share *ssp, struct smb_cred *scred, mode_t mode)
{
	kauth_cred_t cred = scred->scr_cred;
	int error;

	error = kauth_authorize_network(cred, KAUTH_NETWORK_SMB,
	    KAUTH_REQ_NETWORK_SMB_SHARE_ACCESS, ssp, KAUTH_ARG(mode), NULL);
	if (error)
		return EACCES;

	return 0;
}

int
smb_share_valid(struct smb_share *ssp)
{
	return ssp->ss_tid != SMB_TID_UNKNOWN &&
	    ssp->ss_vcgenid == SSTOVC(ssp)->vc_genid;
}

const char*
smb_share_getpass(struct smb_share *ssp)
{
	struct smb_vc *vcp;

	if (ssp->ss_pass)
		return ssp->ss_pass;
	vcp = SSTOVC(ssp);
	if (vcp->vc_pass)
		return vcp->vc_pass;
	return smb_emptypass;
}



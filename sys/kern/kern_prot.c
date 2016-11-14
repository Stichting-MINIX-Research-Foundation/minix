/*	$NetBSD: kern_prot.c,v 1.119 2015/08/24 22:50:32 pooka Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_prot.c	8.9 (Berkeley) 2/14/95
 */

/*
 * System calls related to processes and protection
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_prot.c,v 1.119 2015/08/24 22:50:32 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_compat_43.h"
#endif

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/pool.h>
#include <sys/prot.h>
#include <sys/syslog.h>
#include <sys/uidinfo.h>
#include <sys/kauth.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

int	sys_getpid(struct lwp *, const void *, register_t *);
int	sys_getpid_with_ppid(struct lwp *, const void *, register_t *);
int	sys_getuid(struct lwp *, const void *, register_t *);
int	sys_getuid_with_euid(struct lwp *, const void *, register_t *);
int	sys_getgid(struct lwp *, const void *, register_t *);
int	sys_getgid_with_egid(struct lwp *, const void *, register_t *);

/* ARGSUSED */
int
sys_getpid(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	*retval = p->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getpid_with_ppid(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	retval[0] = p->p_pid;
	retval[1] = p->p_ppid;
	return (0);
}

/* ARGSUSED */
int
sys_getppid(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	*retval = p->p_ppid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
int
sys_getpgrp(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	mutex_enter(proc_lock);
	*retval = p->p_pgrp->pg_id;
	mutex_exit(proc_lock);
	return (0);
}

/*
 * Return the process group ID of the session leader (session ID)
 * for the specified process.
 */
int
sys_getsid(struct lwp *l, const struct sys_getsid_args *uap, register_t *retval)
{
	/* {
		syscalldarg(pid_t) pid;
	} */
	pid_t pid = SCARG(uap, pid);
	struct proc *p;
	int error = 0;

	mutex_enter(proc_lock);
	if (pid == 0)
		*retval = l->l_proc->p_session->s_sid;
	else if ((p = proc_find(pid)) != NULL)
		*retval = p->p_session->s_sid;
	else
		error = ESRCH;
	mutex_exit(proc_lock);

	return error;
}

int
sys_getpgid(struct lwp *l, const struct sys_getpgid_args *uap, register_t *retval)
{
	/* {
		syscallarg(pid_t) pid;
	} */
	pid_t pid = SCARG(uap, pid);
	struct proc *p;
	int error = 0;

	mutex_enter(proc_lock);
	if (pid == 0)
		*retval = l->l_proc->p_pgid;
	else if ((p = proc_find(pid)) != NULL)
		*retval = p->p_pgid;
	else
		error = ESRCH;
	mutex_exit(proc_lock);

	return error;
}

/* ARGSUSED */
int
sys_getuid(struct lwp *l, const void *v, register_t *retval)
{

	*retval = kauth_cred_getuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getuid_with_euid(struct lwp *l, const void *v, register_t *retval)
{

	retval[0] = kauth_cred_getuid(l->l_cred);
	retval[1] = kauth_cred_geteuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_geteuid(struct lwp *l, const void *v, register_t *retval)
{

	*retval = kauth_cred_geteuid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getgid(struct lwp *l, const void *v, register_t *retval)
{

	*retval = kauth_cred_getgid(l->l_cred);
	return (0);
}

/* ARGSUSED */
int
sys_getgid_with_egid(struct lwp *l, const void *v, register_t *retval)
{

	retval[0] = kauth_cred_getgid(l->l_cred);
	retval[1] = kauth_cred_getegid(l->l_cred);
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
int
sys_getegid(struct lwp *l, const void *v, register_t *retval)
{

	*retval = kauth_cred_getegid(l->l_cred);
	return (0);
}

int
sys_getgroups(struct lwp *l, const struct sys_getgroups_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) gidsetsize;
		syscallarg(gid_t *) gidset;
	} */

	*retval = kauth_cred_ngroups(l->l_cred);
	if (SCARG(uap, gidsetsize) == 0)
		return 0;
	if (SCARG(uap, gidsetsize) < (int)*retval)
		return EINVAL;

	return kauth_cred_getgroups(l->l_cred, SCARG(uap, gidset), *retval,
	    UIO_USERSPACE);
}

int
sys_setsid(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;
	int error;

	error = proc_enterpgrp(p, p->p_pid, p->p_pid, true);
	*retval = p->p_pid;
	return (error);
}


/*
 * set process group (setpgid/old setpgrp)
 *
 * caller does setpgid(targpid, targpgid)
 *
 * pgid must be in valid range (EINVAL)
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 *
 * Permission checks now in proc_enterpgrp()
 */
int
sys_setpgid(struct lwp *l, const struct sys_setpgid_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int) pid;
		syscallarg(int) pgid;
	} */
	struct proc *p = l->l_proc;
	pid_t targp, pgid;

	if (SCARG(uap, pgid) < 0)
		return EINVAL;
	if ((targp = SCARG(uap, pid)) == 0)
		targp = p->p_pid;
	if ((pgid = SCARG(uap, pgid)) == 0)
		pgid = targp;

	return proc_enterpgrp(p, targp, pgid, false);
}

/*
 * Set real, effective and saved uids to the requested values.
 * non-root callers can only ever change uids to values that match
 * one of the processes current uid values.
 * This is further restricted by the flags argument.
 */

int
do_setresuid(struct lwp *l, uid_t r, uid_t e, uid_t sv, u_int flags)
{
	struct proc *p = l->l_proc;
	kauth_cred_t cred, ncred;

	ncred = kauth_cred_alloc();

	/* Get a write lock on the process credential. */
	proc_crmod_enter();
	cred = p->p_cred;

	/*
	 * Check that the new value is one of the allowed existing values,
	 * or that we have root privilege.
	 */
	if ((r != -1
	    && !((flags & ID_R_EQ_R) && r == kauth_cred_getuid(cred))
	    && !((flags & ID_R_EQ_E) && r == kauth_cred_geteuid(cred))
	    && !((flags & ID_R_EQ_S) && r == kauth_cred_getsvuid(cred))) ||
	    (e != -1
	    && !((flags & ID_E_EQ_R) && e == kauth_cred_getuid(cred))
	    && !((flags & ID_E_EQ_E) && e == kauth_cred_geteuid(cred))
	    && !((flags & ID_E_EQ_S) && e == kauth_cred_getsvuid(cred))) ||
	    (sv != -1
	    && !((flags & ID_S_EQ_R) && sv == kauth_cred_getuid(cred))
	    && !((flags & ID_S_EQ_E) && sv == kauth_cred_geteuid(cred))
	    && !((flags & ID_S_EQ_S) && sv == kauth_cred_getsvuid(cred)))) {
		int error;

		error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
		    p, NULL, NULL, NULL);
		if (error != 0) {
		 	proc_crmod_leave(cred, ncred, false);
			return error;
		}
	}

	/* If nothing has changed, short circuit the request */
	if ((r == -1 || r == kauth_cred_getuid(cred))
	    && (e == -1 || e == kauth_cred_geteuid(cred))
	    && (sv == -1 || sv == kauth_cred_getsvuid(cred))) {
		proc_crmod_leave(cred, ncred, false);
		return 0;
	}

	kauth_cred_clone(cred, ncred);

	if (r != -1 && r != kauth_cred_getuid(ncred)) {
		u_long nlwps;

		/* Update count of processes for this user. */
		(void)chgproccnt(kauth_cred_getuid(ncred), -1);
		(void)chgproccnt(r, 1);

		/* The first LWP of a process is excluded. */
		KASSERT(mutex_owned(p->p_lock));
		nlwps = p->p_nlwps - 1;
		(void)chglwpcnt(kauth_cred_getuid(ncred), -nlwps);
		(void)chglwpcnt(r, nlwps);

		kauth_cred_setuid(ncred, r);
	}
	if (sv != -1)
		kauth_cred_setsvuid(ncred, sv);
	if (e != -1)
		kauth_cred_seteuid(ncred, e);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);

	return 0;
}

/*
 * Set real, effective and saved gids to the requested values.
 * non-root callers can only ever change gids to values that match
 * one of the processes current gid values.
 * This is further restricted by the flags argument.
 */

int
do_setresgid(struct lwp *l, gid_t r, gid_t e, gid_t sv, u_int flags)
{
	struct proc *p = l->l_proc;
	kauth_cred_t cred, ncred;

	ncred = kauth_cred_alloc();

	/* Get a write lock on the process credential. */
	proc_crmod_enter();
	cred = p->p_cred;

	/*
	 * check new value is one of the allowed existing values.
	 * otherwise, check if we have root privilege.
	 */
	if ((r != -1
	    && !((flags & ID_R_EQ_R) && r == kauth_cred_getgid(cred))
	    && !((flags & ID_R_EQ_E) && r == kauth_cred_getegid(cred))
	    && !((flags & ID_R_EQ_S) && r == kauth_cred_getsvgid(cred))) ||
	    (e != -1
	    && !((flags & ID_E_EQ_R) && e == kauth_cred_getgid(cred))
	    && !((flags & ID_E_EQ_E) && e == kauth_cred_getegid(cred))
	    && !((flags & ID_E_EQ_S) && e == kauth_cred_getsvgid(cred))) ||
	    (sv != -1
	    && !((flags & ID_S_EQ_R) && sv == kauth_cred_getgid(cred))
	    && !((flags & ID_S_EQ_E) && sv == kauth_cred_getegid(cred))
	    && !((flags & ID_S_EQ_S) && sv == kauth_cred_getsvgid(cred)))) {
		int error;

		error = kauth_authorize_process(cred, KAUTH_PROCESS_SETID,
		    p, NULL, NULL, NULL);
		if (error != 0) {
		 	proc_crmod_leave(cred, ncred, false);
			return error;
		}
	}

	/* If nothing has changed, short circuit the request */
	if ((r == -1 || r == kauth_cred_getgid(cred))
	    && (e == -1 || e == kauth_cred_getegid(cred))
	    && (sv == -1 || sv == kauth_cred_getsvgid(cred))) {
	 	proc_crmod_leave(cred, ncred, false);
		return 0;
	}

	kauth_cred_clone(cred, ncred);

	if (r != -1)
		kauth_cred_setgid(ncred, r);
	if (sv != -1)
		kauth_cred_setsvgid(ncred, sv);
	if (e != -1)
		kauth_cred_setegid(ncred, e);

	/* Broadcast our credentials to the process and other LWPs. */
 	proc_crmod_leave(ncred, cred, true);

	return 0;
}

/* ARGSUSED */
int
sys_setuid(struct lwp *l, const struct sys_setuid_args *uap, register_t *retval)
{
	/* {
		syscallarg(uid_t) uid;
	} */
	uid_t uid = SCARG(uap, uid);

	return do_setresuid(l, uid, uid, uid,
			    ID_R_EQ_R | ID_E_EQ_R | ID_S_EQ_R);
}

/* ARGSUSED */
int
sys_seteuid(struct lwp *l, const struct sys_seteuid_args *uap, register_t *retval)
{
	/* {
		syscallarg(uid_t) euid;
	} */

	return do_setresuid(l, -1, SCARG(uap, euid), -1, ID_E_EQ_R | ID_E_EQ_S);
}

int
sys_setreuid(struct lwp *l, const struct sys_setreuid_args *uap, register_t *retval)
{
	/* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
	} */
	kauth_cred_t cred = l->l_cred;
	uid_t ruid, euid, svuid;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);

	if (ruid == -1)
		ruid = kauth_cred_getuid(cred);
	if (euid == -1)
		euid = kauth_cred_geteuid(cred);

	/* Saved uid is set to the new euid if the ruid changed */
	svuid = (ruid == kauth_cred_getuid(cred)) ? -1 : euid;

	return do_setresuid(l, ruid, euid, svuid,
			    ID_R_EQ_R | ID_R_EQ_E |
			    ID_E_EQ_R | ID_E_EQ_E | ID_E_EQ_S |
			    ID_S_EQ_R | ID_S_EQ_E | ID_S_EQ_S);
}

/* ARGSUSED */
int
sys_setgid(struct lwp *l, const struct sys_setgid_args *uap, register_t *retval)
{
	/* {
		syscallarg(gid_t) gid;
	} */
	gid_t gid = SCARG(uap, gid);

	return do_setresgid(l, gid, gid, gid,
			    ID_R_EQ_R | ID_E_EQ_R | ID_S_EQ_R);
}

/* ARGSUSED */
int
sys_setegid(struct lwp *l, const struct sys_setegid_args *uap, register_t *retval)
{
	/* {
		syscallarg(gid_t) egid;
	} */

	return do_setresgid(l, -1, SCARG(uap, egid), -1, ID_E_EQ_R | ID_E_EQ_S);
}

int
sys_setregid(struct lwp *l, const struct sys_setregid_args *uap, register_t *retval)
{
	/* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
	} */
	kauth_cred_t cred = l->l_cred;
	gid_t rgid, egid, svgid;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);

	if (rgid == -1)
		rgid = kauth_cred_getgid(cred);
	if (egid == -1)
		egid = kauth_cred_getegid(cred);

	/* Saved gid is set to the new egid if the rgid changed */
	svgid = rgid == kauth_cred_getgid(cred) ? -1 : egid;

	return do_setresgid(l, rgid, egid, svgid,
			ID_R_EQ_R | ID_R_EQ_E |
			ID_E_EQ_R | ID_E_EQ_E | ID_E_EQ_S |
			ID_S_EQ_R | ID_S_EQ_E | ID_S_EQ_S);
}

int
sys_issetugid(struct lwp *l, const void *v, register_t *retval)
{
	struct proc *p = l->l_proc;

	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use PK_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	*retval = (p->p_flag & PK_SUGID) != 0;
	return (0);
}

/* ARGSUSED */
int
sys_setgroups(struct lwp *l, const struct sys_setgroups_args *uap, register_t *retval)
{
	/* {
		syscallarg(int) gidsetsize;
		syscallarg(const gid_t *) gidset;
	} */
	kauth_cred_t ncred;
	int error;

	ncred = kauth_cred_alloc();
	error = kauth_cred_setgroups(ncred, SCARG(uap, gidset),
	    SCARG(uap, gidsetsize), -1, UIO_USERSPACE);
	if (error != 0) {
		kauth_cred_free(ncred);
		return error;
	}

	return kauth_proc_setgroups(l, ncred);
}

/*
 * Get login name, if available.
 */
/* ARGSUSED */
int
sys___getlogin(struct lwp *l, const struct sys___getlogin_args *uap, register_t *retval)
{
	/* {
		syscallarg(char *) namebuf;
		syscallarg(size_t) namelen;
	} */
	struct proc *p = l->l_proc;
	char login[sizeof(p->p_session->s_login)];
	size_t namelen = SCARG(uap, namelen);

	if (namelen > sizeof(login))
		namelen = sizeof(login);
	mutex_enter(proc_lock);
	memcpy(login, p->p_session->s_login, namelen);
	mutex_exit(proc_lock);
	return (copyout(login, (void *)SCARG(uap, namebuf), namelen));
}

/*
 * Set login name.
 */
/* ARGSUSED */
int
sys___setlogin(struct lwp *l, const struct sys___setlogin_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) namebuf;
	} */
	struct proc *p = l->l_proc;
	struct session *sp;
	char newname[sizeof sp->s_login + 1];
	int error;

	if ((error = kauth_authorize_process(l->l_cred, KAUTH_PROCESS_SETID,
	    p, NULL, NULL, NULL)) != 0)
		return (error);
	error = copyinstr(SCARG(uap, namebuf), newname, sizeof newname, NULL);
	if (error != 0)
		return (error == ENAMETOOLONG ? EINVAL : error);

	mutex_enter(proc_lock);
	sp = p->p_session;
	if (sp->s_flags & S_LOGIN_SET && p->p_pid != sp->s_sid &&
	    strncmp(newname, sp->s_login, sizeof sp->s_login) != 0)
		log(LOG_WARNING, "%s (pid %d) changing logname from "
		    "%.*s to %s\n", p->p_comm, p->p_pid,
		    (int)sizeof sp->s_login, sp->s_login, newname);
	sp->s_flags |= S_LOGIN_SET;
	strncpy(sp->s_login, newname, sizeof sp->s_login);
	mutex_exit(proc_lock);
	return (0);
}

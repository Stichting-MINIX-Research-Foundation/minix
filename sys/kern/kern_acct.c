/*	$NetBSD: kern_acct.c,v 1.94 2013/10/19 21:01:39 mrg Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_acct.c	8.8 (Berkeley) 5/14/95
 */

/*-
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)kern_acct.c	8.8 (Berkeley) 5/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_acct.c,v 1.94 2013/10/19 21:01:39 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/kmem.h>
#include <sys/namei.h>
#include <sys/errno.h>
#include <sys/acct.h>
#include <sys/resourcevar.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kauth.h>

#include <sys/syscallargs.h>

/*
 * The routines implemented in this file are described in:
 *      Leffler, et al.: The Design and Implementation of the 4.3BSD
 *	    UNIX Operating System (Addison Welley, 1989)
 * on pages 62-63.
 *
 * Arguably, to simplify accounting operations, this mechanism should
 * be replaced by one in which an accounting log file (similar to /dev/klog)
 * is read by a user process, etc.  However, that has its own problems.
 */

/*
 * Lock to serialize system calls and kernel threads.
 */
krwlock_t	acct_lock;

/*
 * The global accounting state and related data.  Gain the mutex before
 * accessing these variables.
 */
static enum {
	ACCT_STOP,
	ACCT_ACTIVE,
	ACCT_SUSPENDED
} acct_state;				/* The current accounting state. */
static struct vnode *acct_vp;		/* Accounting vnode pointer. */
static kauth_cred_t acct_cred;		/* Credential of accounting file
					   owner (i.e root).  Used when
 					   accounting file i/o.  */
static struct lwp *acct_dkwatcher;	/* Free disk space checker. */

/*
 * Values associated with enabling and disabling accounting
 */
int	acctsuspend = 2;	/* stop accounting when < 2% free space left */
int	acctresume = 4;		/* resume when free space risen to > 4% */
int	acctchkfreq = 15;	/* frequency (in seconds) to check space */

/*
 * Encode_comp_t converts from ticks in seconds and microseconds
 * to ticks in 1/AHZ seconds.  The encoding is described in
 * Leffler, et al., on page 63.
 */

#define	MANTSIZE	13			/* 13 bit mantissa. */
#define	EXPSIZE		3			/* Base 8 (3 bit) exponent. */
#define	MAXFRACT	((1 << MANTSIZE) - 1)	/* Maximum fractional value. */

static comp_t
encode_comp_t(u_long s, u_long us)
{
	int exp, rnd;

	exp = 0;
	rnd = 0;
	s *= AHZ;
	s += us / (1000000 / AHZ);	/* Maximize precision. */

	while (s > MAXFRACT) {
	rnd = s & (1 << (EXPSIZE - 1));	/* Round up? */
		s >>= EXPSIZE;		/* Base 8 exponent == 3 bit shift. */
		exp++;
	}

	/* If we need to round up, do it (and handle overflow correctly). */
	if (rnd && (++s > MAXFRACT)) {
		s >>= EXPSIZE;
		exp++;
	}

	/* Clean it up and polish it off. */
	exp <<= MANTSIZE;		/* Shift the exponent into place */
	exp += s;			/* and add on the mantissa. */
	return (exp);
}

static int
acct_chkfree(void)
{
	int error;
	struct statvfs *sb;
	fsblkcnt_t bavail;

	sb = kmem_alloc(sizeof(*sb), KM_SLEEP);
	if (sb == NULL)
		return (ENOMEM);
	error = VFS_STATVFS(acct_vp->v_mount, sb);
	if (error != 0) {
		kmem_free(sb, sizeof(*sb));
		return (error);
	}

	if (sb->f_bfree < sb->f_bresvd) {
		bavail = 0;
	} else {
		bavail = sb->f_bfree - sb->f_bresvd;
	}

	switch (acct_state) {
	case ACCT_SUSPENDED:
		if (bavail > acctresume * sb->f_blocks / 100) {
			acct_state = ACCT_ACTIVE;
			log(LOG_NOTICE, "Accounting resumed\n");
		}
		break;
	case ACCT_ACTIVE:
		if (bavail <= acctsuspend * sb->f_blocks / 100) {
			acct_state = ACCT_SUSPENDED;
			log(LOG_NOTICE, "Accounting suspended\n");
		}
		break;
	case ACCT_STOP:
		break;
	}
	kmem_free(sb, sizeof(*sb));
	return (0);
}

static void
acct_stop(void)
{
	int error;

	KASSERT(rw_write_held(&acct_lock));

	if (acct_vp != NULLVP && acct_vp->v_type != VBAD) {
		error = vn_close(acct_vp, FWRITE, acct_cred);
#ifdef DIAGNOSTIC
		if (error != 0)
			printf("acct_stop: failed to close, errno = %d\n",
			    error);
#else
		__USE(error);
#endif
		acct_vp = NULLVP;
	}
	if (acct_cred != NULL) {
		kauth_cred_free(acct_cred);
		acct_cred = NULL;
	}
	acct_state = ACCT_STOP;
}

/*
 * Periodically check the file system to see if accounting
 * should be turned on or off.  Beware the case where the vnode
 * has been vgone()'d out from underneath us, e.g. when the file
 * system containing the accounting file has been forcibly unmounted.
 */
static void
acctwatch(void *arg)
{
	int error;

	log(LOG_NOTICE, "Accounting started\n");
	rw_enter(&acct_lock, RW_WRITER);
	while (acct_state != ACCT_STOP) {
		if (acct_vp->v_type == VBAD) {
			log(LOG_NOTICE, "Accounting terminated\n");
			acct_stop();
			continue;
		}

		error = acct_chkfree();
#ifdef DIAGNOSTIC
		if (error != 0)
			printf("acctwatch: failed to statvfs, error = %d\n",
			    error);
#else
		__USE(error);
#endif
		rw_exit(&acct_lock);
		error = kpause("actwat", false, acctchkfreq * hz, NULL);
		rw_enter(&acct_lock, RW_WRITER);
#ifdef DIAGNOSTIC
		if (error != 0 && error != EWOULDBLOCK)
			printf("acctwatch: sleep error %d\n", error);
#endif
	}
	acct_dkwatcher = NULL;
	rw_exit(&acct_lock);

	kthread_exit(0);
}

void
acct_init(void)
{

	acct_state = ACCT_STOP;
	acct_vp = NULLVP;
	acct_cred = NULL;
	rw_init(&acct_lock);
}

/*
 * Accounting system call.  Written based on the specification and
 * previous implementation done by Mark Tinguely.
 */
int
sys_acct(struct lwp *l, const struct sys_acct_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *) path;
	} */
	struct pathbuf *pb;
	struct nameidata nd;
	int error;

	/* Make sure that the caller is root. */
	if ((error = kauth_authorize_system(l->l_cred, KAUTH_SYSTEM_ACCOUNTING,
	    0, NULL, NULL, NULL)))
		return (error);

	/*
	 * If accounting is to be started to a file, open that file for
	 * writing and make sure it's a 'normal'.
	 */
	if (SCARG(uap, path) != NULL) {
		struct vattr va;
		size_t pad;

		error = pathbuf_copyin(SCARG(uap, path), &pb);
		if (error) {
			return error;
		}
		NDINIT(&nd, LOOKUP, FOLLOW | TRYEMULROOT, pb);
		if ((error = vn_open(&nd, FWRITE|O_APPEND, 0)) != 0) {
			pathbuf_destroy(pb);
			return error;
		}
		if (nd.ni_vp->v_type != VREG) {
			VOP_UNLOCK(nd.ni_vp);
			error = EACCES;
			goto bad;
		}
		if ((error = VOP_GETATTR(nd.ni_vp, &va, l->l_cred)) != 0) {
			VOP_UNLOCK(nd.ni_vp);
			goto bad;
		}

		if ((pad = (va.va_size % sizeof(struct acct))) != 0) {
			u_quad_t size = va.va_size - pad;
#ifdef DIAGNOSTIC
			printf("Size of accounting file not a multiple of "
			    "%lu - incomplete record truncated\n",
			    (unsigned long)sizeof(struct acct));
#endif
			vattr_null(&va);
			va.va_size = size;
			error = VOP_SETATTR(nd.ni_vp, &va, l->l_cred);
			if (error != 0) {
				VOP_UNLOCK(nd.ni_vp);
				goto bad;
			}
		}
		VOP_UNLOCK(nd.ni_vp);
	}

	rw_enter(&acct_lock, RW_WRITER);

	/*
	 * If accounting was previously enabled, kill the old space-watcher,
	 * free credential for accounting file i/o,
	 * ... (and, if no new file was specified, leave).
	 */
	acct_stop();
	if (SCARG(uap, path) == NULL)
		goto out;

	/*
	 * Save the new accounting file vnode and credential,
	 * and schedule the new free space watcher.
	 */
	acct_state = ACCT_ACTIVE;
	acct_vp = nd.ni_vp;
	acct_cred = l->l_cred;
	kauth_cred_hold(acct_cred);

	pathbuf_destroy(pb);

	error = acct_chkfree();		/* Initial guess. */
	if (error != 0) {
		acct_stop();
		goto out;
	}

	if (acct_dkwatcher == NULL) {
		error = kthread_create(PRI_NONE, KTHREAD_MPSAFE, NULL,
		    acctwatch, NULL, &acct_dkwatcher, "acctwatch");
		if (error != 0)
			acct_stop();
	}

 out:
	rw_exit(&acct_lock);
	return (error);
 bad:
	vn_close(nd.ni_vp, FWRITE, l->l_cred);
	pathbuf_destroy(pb);
	return error;
}

/*
 * Write out process accounting information, on process exit.
 * Data to be written out is specified in Leffler, et al.
 * and are enumerated below.  (They're also noted in the system
 * "acct.h" header file.)
 */
int
acct_process(struct lwp *l)
{
	struct acct acct;
	struct timeval ut, st, tmp;
	struct rusage *r;
	int t, error = 0;
	struct rlimit orlim;
	struct proc *p = l->l_proc;

	if (acct_state != ACCT_ACTIVE)
		return 0;

	memset(&acct, 0, sizeof(acct));	/* to zerofill padded data */

	rw_enter(&acct_lock, RW_READER);

	/* If accounting isn't enabled, don't bother */
	if (acct_state != ACCT_ACTIVE)
		goto out;

	/*
	 * Temporarily raise the file limit so that accounting can't
	 * be stopped by the user.
	 *
	 * XXX We should think about the CPU limit, too.
	 */
	lim_privatise(p);
	orlim = p->p_rlimit[RLIMIT_FSIZE];
	/* Set current and max to avoid illegal values */
	p->p_rlimit[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	p->p_rlimit[RLIMIT_FSIZE].rlim_max = RLIM_INFINITY;

	/*
	 * Get process accounting information.
	 */

	/* (1) The name of the command that ran */
	strncpy(acct.ac_comm, p->p_comm, sizeof(acct.ac_comm));

	/* (2) The amount of user and system time that was used */
	mutex_enter(p->p_lock);
	calcru(p, &ut, &st, NULL, NULL);
	mutex_exit(p->p_lock);
	acct.ac_utime = encode_comp_t(ut.tv_sec, ut.tv_usec);
	acct.ac_stime = encode_comp_t(st.tv_sec, st.tv_usec);

	/* (3) The elapsed time the commmand ran (and its starting time) */
	acct.ac_btime = p->p_stats->p_start.tv_sec;
	getmicrotime(&tmp);
	timersub(&tmp, &p->p_stats->p_start, &tmp);
	acct.ac_etime = encode_comp_t(tmp.tv_sec, tmp.tv_usec);

	/* (4) The average amount of memory used */
	r = &p->p_stats->p_ru;
	timeradd(&ut, &st, &tmp);
	t = tmp.tv_sec * hz + tmp.tv_usec / tick;
	if (t)
		acct.ac_mem = (r->ru_ixrss + r->ru_idrss + r->ru_isrss) / t;
	else
		acct.ac_mem = 0;

	/* (5) The number of disk I/O operations done */
	acct.ac_io = encode_comp_t(r->ru_inblock + r->ru_oublock, 0);

	/* (6) The UID and GID of the process */
	acct.ac_uid = kauth_cred_getuid(l->l_cred);
	acct.ac_gid = kauth_cred_getgid(l->l_cred);

	/* (7) The terminal from which the process was started */
	mutex_enter(proc_lock);
	if ((p->p_lflag & PL_CONTROLT) && p->p_pgrp->pg_session->s_ttyp)
		acct.ac_tty = p->p_pgrp->pg_session->s_ttyp->t_dev;
	else
		acct.ac_tty = NODEV;
	mutex_exit(proc_lock);

	/* (8) The boolean flags that tell how the process terminated, etc. */
	acct.ac_flag = p->p_acflag;

	/*
	 * Now, just write the accounting information to the file.
	 */
	error = vn_rdwr(UIO_WRITE, acct_vp, (void *)&acct,
	    sizeof(acct), (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT,
	    acct_cred, NULL, NULL);
	if (error != 0)
		log(LOG_ERR, "Accounting: write failed %d\n", error);

	/* Restore limit - rather pointless since process is about to exit */
	p->p_rlimit[RLIMIT_FSIZE] = orlim;

 out:
	rw_exit(&acct_lock);
	return (error);
}

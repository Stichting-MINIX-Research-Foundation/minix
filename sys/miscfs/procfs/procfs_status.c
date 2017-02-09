/*	$NetBSD: procfs_status.c,v 1.36 2009/10/21 21:12:06 rmind Exp $	*/

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 */

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_status.c	8.4 (Berkeley) 6/15/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: procfs_status.c,v 1.36 2009/10/21 21:12:06 rmind Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>

#include <miscfs/procfs/procfs.h>

int
procfs_dostatus(
    struct lwp *curl,
    struct lwp *l,
    struct pfsnode *pfs,
    struct uio *uio
)
{
	struct session *sess;
	struct tty *tp;
	kauth_cred_t cr;
	struct proc *p = l->l_proc;
	char *ps;
	const char *sep;
	int pid, ppid, pgid, sid;
	u_int i;
	char psbuf[256+MAXHOSTNAMELEN];		/* XXX - conservative */
	uint16_t ngroups;

	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	mutex_enter(proc_lock);
	mutex_enter(p->p_lock);

	pid = p->p_pid;
	ppid = p->p_pptr ? p->p_pptr->p_pid : 0,
	pgid = p->p_pgrp->pg_id;
	sess = p->p_pgrp->pg_session;
	sid = sess->s_sid;

/* comm pid ppid pgid sid maj,min ctty,sldr start ut st wmsg uid gid groups ... */

	ps = psbuf;
	memcpy(ps, p->p_comm, MAXCOMLEN);
	ps[MAXCOMLEN] = '\0';
	ps += strlen(ps);
	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), " %d %d %d %d ",
	    pid, ppid, pgid, sid);

	if ((p->p_lflag & PL_CONTROLT) && (tp = sess->s_ttyp))
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "%llu,%llu ",
		    (unsigned long long)major(tp->t_dev),
		    (unsigned long long)minor(tp->t_dev));
	else
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "%d,%d ",
		    -1, -1);

	sep = "";
	if (sess->s_ttyvp) {
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "%sctty", sep);
		sep = ",";
	}
	if (SESS_LEADER(p)) {
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "%ssldr", sep);
		sep = ",";
	}
	if (*sep != ',')
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "noflags");

	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), " %lld,%ld",
	    (long long)p->p_stats->p_start.tv_sec,
	    (long)p->p_stats->p_start.tv_usec);

	{
		struct timeval ut, st;

		calcru(p, &ut, &st, (void *) 0, NULL);
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf),
		    " %lld,%ld %lld,%ld", (long long)ut.tv_sec,
		    (long)ut.tv_usec, (long long)st.tv_sec, (long)st.tv_usec);
	}

	lwp_lock(l);
	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), " %s",
	    (l->l_wchan && l->l_wmesg) ? l->l_wmesg : "nochan");
	lwp_unlock(l);

	cr = p->p_cred;

	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), " %d",
		       kauth_cred_geteuid(cr));
	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), " %d",
		       kauth_cred_getegid(cr));
	ngroups = kauth_cred_ngroups(cr);
	for (i = 0; i < ngroups; i++)
		ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), ",%d",
		    kauth_cred_group(cr, i));
	ps += snprintf(ps, sizeof(psbuf) - (ps - psbuf), "\n");

	mutex_exit(p->p_lock);
	mutex_exit(proc_lock);

	return (uiomove_frombuf(psbuf, ps - psbuf, uio));
}

/*	$NetBSD: uipc_accf.c,v 1.13 2014/02/25 18:30:11 pooka Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software developed for The NetBSD Foundation
 * by Andrew Doran.
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

/*-
 * Copyright (c) 2000 Paycounter, Inc.
 * Copyright (c) 2005 Robert N. M. Watson
 * Author: Alfred Perlstein <alfred@paycounter.com>, <alfred@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uipc_accf.c,v 1.13 2014/02/25 18:30:11 pooka Exp $");

#define ACCEPT_FILTER_MOD

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/rwlock.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/queue.h>
#include <sys/once.h>
#include <sys/atomic.h>
#include <sys/module.h>

static krwlock_t accept_filter_lock;

static LIST_HEAD(, accept_filter) accept_filtlsthd =
    LIST_HEAD_INITIALIZER(&accept_filtlsthd);

/*
 * Names of Accept filter sysctl objects
 */
static struct sysctllog *ctllog;
static void
sysctl_net_inet_accf_setup(void)
{

	sysctl_createv(&ctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "inet", NULL,
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, CTL_EOL);
	sysctl_createv(&ctllog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "accf",
		       SYSCTL_DESCR("Accept filters"),
		       NULL, 0, NULL, 0,
		       CTL_NET, PF_INET, SO_ACCEPTFILTER, CTL_EOL);
}

int
accept_filt_add(struct accept_filter *filt)
{
	struct accept_filter *p;

	accept_filter_init();

	rw_enter(&accept_filter_lock, RW_WRITER);
	LIST_FOREACH(p, &accept_filtlsthd, accf_next) {
		if (strcmp(p->accf_name, filt->accf_name) == 0)  {
			rw_exit(&accept_filter_lock);
			return EEXIST;
		}
	}				
	LIST_INSERT_HEAD(&accept_filtlsthd, filt, accf_next);
	rw_exit(&accept_filter_lock);

	return 0;
}

int
accept_filt_del(struct accept_filter *p)
{

	rw_enter(&accept_filter_lock, RW_WRITER);
	if (p->accf_refcnt != 0) {
		rw_exit(&accept_filter_lock);
		return EBUSY;
	}
	LIST_REMOVE(p, accf_next);
	rw_exit(&accept_filter_lock);

	return 0;
}

struct accept_filter *
accept_filt_get(char *name)
{
	struct accept_filter *p;
	char buf[32];
	u_int gen;

	do {
		rw_enter(&accept_filter_lock, RW_READER);
		LIST_FOREACH(p, &accept_filtlsthd, accf_next) {
			if (strcmp(p->accf_name, name) == 0) {
				atomic_inc_uint(&p->accf_refcnt);
				break;
			}
		}
		rw_exit(&accept_filter_lock);
		if (p != NULL) {
			break;
		}
		/* Try to autoload a module to satisfy the request. */
		strcpy(buf, "accf_");
		strlcat(buf, name, sizeof(buf));
		gen = module_gen;
		(void)module_autoload(buf, MODULE_CLASS_ANY);
	} while (gen != module_gen);

	return p;
}

/*
 * Accept filter initialization routine.
 * This should be called only once.
 */

static int
accept_filter_init0(void)
{

	rw_init(&accept_filter_lock);
	sysctl_net_inet_accf_setup();

	return 0;
}

/*
 * Initialization routine: This can also be replaced with 
 * accept_filt_generic_mod_event for attaching new accept filter.
 */

void
accept_filter_init(void)
{
	static ONCE_DECL(accept_filter_init_once);

	RUN_ONCE(&accept_filter_init_once, accept_filter_init0);
}

int
accept_filt_getopt(struct socket *so, struct sockopt *sopt)
{
	struct accept_filter_arg afa;
	int error;

	KASSERT(solocked(so));

	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto out;
	}
	if ((so->so_options & SO_ACCEPTFILTER) == 0) {
		error = EINVAL;
		goto out;
	}

	memset(&afa, 0, sizeof(afa));
	strcpy(afa.af_name, so->so_accf->so_accept_filter->accf_name);
	if (so->so_accf->so_accept_filter_str != NULL)
		strcpy(afa.af_arg, so->so_accf->so_accept_filter_str);
	error = sockopt_set(sopt, &afa, sizeof(afa));
out:
	return error;
}

/*
 * Simple delete case, with socket locked.
 */
int
accept_filt_clear(struct socket *so)
{
	struct accept_filter_arg afa;
	struct accept_filter *afp;
	struct socket *so2, *next;
	struct so_accf *af;

	KASSERT(solocked(so));

	if ((so->so_options & SO_ACCEPTCONN) == 0) {
		return EINVAL;
	}
	if (so->so_accf != NULL) {
		/* Break in-flight processing. */
		for (so2 = TAILQ_FIRST(&so->so_q0); so2 != NULL; so2 = next) {
			next = TAILQ_NEXT(so2, so_qe);
			if (so2->so_upcall == NULL) {
				continue;
			}
			so2->so_upcall = NULL;
			so2->so_upcallarg = NULL;
			so2->so_options &= ~SO_ACCEPTFILTER;
			so2->so_rcv.sb_flags &= ~SB_UPCALL;
			soisconnected(so2);
		}
		af = so->so_accf;
		afp = af->so_accept_filter;
		if (afp != NULL && afp->accf_destroy != NULL) {
			(*afp->accf_destroy)(so);
		}
		if (af->so_accept_filter_str != NULL) {
			kmem_free(af->so_accept_filter_str,
			    sizeof(afa.af_name));
		}
		kmem_free(af, sizeof(*af));
		so->so_accf = NULL;
		atomic_dec_uint(&afp->accf_refcnt);
	}
	so->so_options &= ~SO_ACCEPTFILTER;
	return 0;
}

/*
 * setsockopt() for accept filters.  Called with the socket unlocked,
 * will always return it locked.
 */
int
accept_filt_setopt(struct socket *so, const struct sockopt *sopt)
{
	struct accept_filter_arg afa;
	struct accept_filter *afp;
	struct so_accf *newaf;
	int error;

	accept_filter_init();

	if (sopt == NULL || sopt->sopt_size == 0) {
		solock(so);
		return accept_filt_clear(so);
	}

	/*
	 * Pre-allocate any memory we may need later to avoid blocking at
	 * untimely moments.  This does not optimize for invalid arguments.
	 */
	error = sockopt_get(sopt, &afa, sizeof(afa));
	if (error) {
		solock(so);
		return error;
	}
	afa.af_name[sizeof(afa.af_name)-1] = '\0';
	afa.af_arg[sizeof(afa.af_arg)-1] = '\0';
	afp = accept_filt_get(afa.af_name);
	if (afp == NULL) {
		solock(so);
		return ENOENT;
	}
	/*
	 * Allocate the new accept filter instance storage.  We may
	 * have to free it again later if we fail to attach it.  If
	 * attached properly, 'newaf' is NULLed to avoid a free()
	 * while in use.
	 */
	newaf = kmem_zalloc(sizeof(*newaf), KM_SLEEP);
	if (afp->accf_create != NULL && afa.af_name[0] != '\0') {
		/*
		 * FreeBSD did a variable-size allocation here
		 * with the actual string length from afa.af_name
		 * but it is so short, why bother tracking it?
		 * XXX as others have noted, this is an API mistake;
		 * XXX accept_filter_arg should have a mandatory namelen.
		 * XXX (but it's a bit too late to fix that now)
		 */
		newaf->so_accept_filter_str =
		    kmem_alloc(sizeof(afa.af_name), KM_SLEEP);
		strcpy(newaf->so_accept_filter_str, afa.af_name);
	}

	/*
	 * Require a listen socket; don't try to replace an existing filter
	 * without first removing it.
	 */
	solock(so);
	if ((so->so_options & SO_ACCEPTCONN) == 0 || so->so_accf != NULL) {
		error = EINVAL;
		goto out;
	}

	/*
	 * Invoke the accf_create() method of the filter if required.  The
	 * socket lock is held over this call, so create methods for filters
	 * shouldn't block.
	 */
	if (afp->accf_create != NULL) {
		newaf->so_accept_filter_arg =
		    (*afp->accf_create)(so, afa.af_arg);
		if (newaf->so_accept_filter_arg == NULL) {
			error = EINVAL;
			goto out;
		}
	}
	newaf->so_accept_filter = afp;
	so->so_accf = newaf;
	so->so_options |= SO_ACCEPTFILTER;
	newaf = NULL;
out:
	if (newaf != NULL) {
		if (newaf->so_accept_filter_str != NULL)
			kmem_free(newaf->so_accept_filter_str,
			    sizeof(afa.af_name));
		kmem_free(newaf, sizeof(*newaf));
		atomic_dec_uint(&afp->accf_refcnt);
	}
	return error;
}

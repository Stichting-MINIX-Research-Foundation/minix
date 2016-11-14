/* $NetBSD: subr_evcnt.c,v 1.12 2014/02/25 18:30:11 pooka Exp $ */

/*
 * Copyright (c) 1996, 2000 Christopher G. Demetriou
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * --(license Id: LICENSE.proto,v 1.1 2000/06/13 21:40:26 cgd Exp )--
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 * from: Header: subr_autoconf.c,v 1.12 93/02/01 19:31:48 torek Exp  (LBL)
 *
 *	@(#)subr_autoconf.c	8.3 (Berkeley) 5/17/94
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_evcnt.c,v 1.12 2014/02/25 18:30:11 pooka Exp $");

#include <sys/param.h>
#include <sys/evcnt.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

/* list of all events */
struct evcntlist allevents = TAILQ_HEAD_INITIALIZER(allevents);
static kmutex_t evcnt_lock __cacheline_aligned;
static bool init_done;
static uint32_t evcnt_generation;

/*
 * We need a dummy object to stuff into the evcnt link set to
 * ensure that there always is at least one object in the set.
 */
static struct evcnt dummy_static_evcnt;
__link_set_add_bss(evcnts, dummy_static_evcnt);

/*
 * Initialize event counters.  This does the attach procedure for
 * each of the static event counters in the "evcnts" link set.
 */
void
evcnt_init(void)
{
	__link_set_decl(evcnts, struct evcnt);
	struct evcnt * const *evp;

	KASSERT(!init_done);

	mutex_init(&evcnt_lock, MUTEX_DEFAULT, IPL_NONE);

	init_done = true;

	__link_set_foreach(evp, evcnts) {
		if (*evp == &dummy_static_evcnt)
			continue;
		evcnt_attach_static(*evp);
	}
}

/*
 * Attach a statically-initialized event.  The type and string pointers
 * are already set up.
 */
void
evcnt_attach_static(struct evcnt *ev)
{
	int len;

	KASSERTMSG(init_done,
	    "%s: evcnt non initialized: group=<%s> name=<%s>",
	    __func__, ev->ev_group, ev->ev_name);

	len = strlen(ev->ev_group);
#ifdef DIAGNOSTIC
	if (len == 0 || len >= EVCNT_STRING_MAX) /* ..._MAX includes NUL */
		panic("evcnt_attach_static: group length (%s)", ev->ev_group);
#endif
	ev->ev_grouplen = len;

	len = strlen(ev->ev_name);
#ifdef DIAGNOSTIC
	if (len == 0 || len >= EVCNT_STRING_MAX) /* ..._MAX includes NUL */
		panic("evcnt_attach_static: name length (%s)", ev->ev_name);
#endif
	ev->ev_namelen = len;

	mutex_enter(&evcnt_lock);
	TAILQ_INSERT_TAIL(&allevents, ev, ev_list);
	mutex_exit(&evcnt_lock);
}

/*
 * Attach a dynamically-initialized event.  Zero it, set up the type
 * and string pointers and then act like it was statically initialized.
 */
void
evcnt_attach_dynamic_nozero(struct evcnt *ev, int type,
    const struct evcnt *parent, const char *group, const char *name)
{

	ev->ev_type = type;
	ev->ev_parent = parent;
	ev->ev_group = group;
	ev->ev_name = name;
	evcnt_attach_static(ev);
}
/*
 * Attach a dynamically-initialized event.  Zero it, set up the type
 * and string pointers and then act like it was statically initialized.
 */
void
evcnt_attach_dynamic(struct evcnt *ev, int type, const struct evcnt *parent,
    const char *group, const char *name)
{

	memset(ev, 0, sizeof *ev);
	evcnt_attach_dynamic_nozero(ev, type, parent, group, name);
}

/*
 * Detach an event.
 */
void
evcnt_detach(struct evcnt *ev)
{

	mutex_enter(&evcnt_lock);
	TAILQ_REMOVE(&allevents, ev, ev_list);
	evcnt_generation++;
	mutex_exit(&evcnt_lock);
}

struct xevcnt_sysctl {
	struct evcnt_sysctl evs;
	char ev_strings[2*EVCNT_STRING_MAX];
};

static size_t
sysctl_fillevcnt(const struct evcnt *ev, struct xevcnt_sysctl *xevs,
	size_t *copylenp)
{
	const size_t copylen = offsetof(struct evcnt_sysctl, ev_strings)
	    + ev->ev_grouplen + 1 + ev->ev_namelen + 1;
	const size_t len = roundup2(copylen, sizeof(uint64_t));
	if (xevs != NULL) {
		xevs->evs.ev_count = ev->ev_count;
		xevs->evs.ev_addr = PTRTOUINT64(ev);
		xevs->evs.ev_parent = PTRTOUINT64(ev->ev_parent);
		xevs->evs.ev_type = ev->ev_type;
		xevs->evs.ev_grouplen = ev->ev_grouplen;
		xevs->evs.ev_namelen = ev->ev_namelen;
		xevs->evs.ev_len = len / sizeof(uint64_t);
		strcpy(xevs->evs.ev_strings, ev->ev_group);
		strcpy(xevs->evs.ev_strings + ev->ev_grouplen + 1, ev->ev_name);
	}

	*copylenp = copylen;
	return len;
}

static int
sysctl_doevcnt(SYSCTLFN_ARGS)
{       
	struct xevcnt_sysctl *xevs0 = NULL, *xevs;
	const struct evcnt *ev;
	int error;
	int retries;
	size_t needed, len;
	char *dp;
 
        if (namelen == 1 && name[0] == CTL_QUERY)
                return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (namelen != 2)
		return (EINVAL);

	/*
	 * We can filter on the type of evcnt.
	 */
	const int filter = name[0];
	if (filter != EVCNT_TYPE_ANY
	    && filter != EVCNT_TYPE_MISC
	    && filter != EVCNT_TYPE_INTR
	    && filter != EVCNT_TYPE_TRAP)
		return (EINVAL);

	const u_int count = name[1];
	if (count != KERN_EVCNT_COUNT_ANY
	    && count != KERN_EVCNT_COUNT_NONZERO)
		return (EINVAL);

	sysctl_unlock();

	if (oldp != NULL && xevs0 == NULL)
		xevs0 = kmem_alloc(sizeof(*xevs0), KM_SLEEP);

	retries = 100;
 retry:
	dp = oldp;
	len = (oldp != NULL) ? *oldlenp : 0;
	xevs = xevs0;
	error = 0;
	needed = 0;

	mutex_enter(&evcnt_lock);
	TAILQ_FOREACH(ev, &allevents, ev_list) {
		if (filter != EVCNT_TYPE_ANY && filter != ev->ev_type)
			continue;
		if (count == KERN_EVCNT_COUNT_NONZERO && ev->ev_count == 0)
			continue;

		/*
		 * Prepare to copy.  If xevs is NULL, fillevcnt will just
		 * how big the item is.
		 */
		size_t copylen;
		const size_t elem_size = sysctl_fillevcnt(ev, xevs, &copylen);
		needed += elem_size;

		if (len < elem_size) {
			xevs = NULL;
			continue;
		}

		KASSERT(xevs != NULL);
		KASSERT(xevs->evs.ev_grouplen != 0);
		KASSERT(xevs->evs.ev_namelen != 0);
		KASSERT(xevs->evs.ev_strings[0] != 0);

		const uint32_t last_generation = evcnt_generation;
		mutex_exit(&evcnt_lock);

		/*
		 * Only copy the actual number of bytes, not the rounded
		 * number.  If we did the latter we'd have to zero them
		 * first or we'd leak random kernel memory.
		 */
		error = copyout(xevs, dp, copylen);

		mutex_enter(&evcnt_lock);
		if (error)
			break;

		if (__predict_false(last_generation != evcnt_generation)) {
			/*
			 * This sysctl node is only for statistics.
			 * Retry; if the queue keeps changing, then
			 * bail out.
			 */
			if (--retries == 0) {
				error = EAGAIN;
				break;
			}
			mutex_exit(&evcnt_lock);
			goto retry;
		}

		/*
		 * Now we deal with the pointer/len since we aren't going to
		 * toss their values away.
		 */
		dp += elem_size;
		len -= elem_size;
	}
	mutex_exit(&evcnt_lock);

	if (xevs0 != NULL)
		kmem_free(xevs0, sizeof(*xevs0));

	sysctl_relock();

	*oldlenp = needed;
	if (oldp == NULL)
		*oldlenp += 1024;

	return (error);
}



SYSCTL_SETUP(sysctl_evcnt_setup, "sysctl kern.evcnt subtree setup")
{

	sysctl_createv(clog, 0, NULL, NULL,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_STRUCT, "evcnt",
		       SYSCTL_DESCR("Kernel evcnt information"),
		       sysctl_doevcnt, 0, NULL, 0,
		       CTL_KERN, KERN_EVCNT, CTL_EOL);
}

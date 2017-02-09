/*	$NetBSD: ath_netbsd.h,v 1.15 2013/01/27 12:48:56 jmcneill Exp $ */

/*-
 * Copyright (c) 2003, 2004 David Young
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
 */
#ifndef _ATH_NETBSD_H
#define _ATH_NETBSD_H

#include <sys/sysctl.h>

typedef struct ath_task {
	void	(*t_func)(void*, int);
	void	*t_context;
} ath_task_t;

#define ATH_CALLOUT_INIT(__ch, __mpsafe) callout_init((__ch), 0)

#define TASK_INIT(__task, __zero, __func, __context)	\
	do {						\
		(__task)->t_func = (__func);		\
		(__task)->t_context = (__context);	\
	} while (0)

#define TASK_RUN_OR_ENQUEUE(__task)	\
	((*(__task)->t_func)((__task)->t_context, 1))

typedef kmutex_t ath_txq_lock_t;
#define	ATH_TXQ_LOCK_INIT(_sc, _tq)	mutex_init(&(_tq)->axq_lock, MUTEX_DEFAULT, IPL_NET)
#define	ATH_TXQ_LOCK_DESTROY(_tq)	mutex_destroy(&(_tq)->axq_lock)
#define	ATH_TXQ_LOCK(_tq)		mutex_enter(&(_tq)->axq_lock)
#define	ATH_TXQ_UNLOCK(_tq)		mutex_exit(&(_tq)->axq_lock)
#define	ATH_TXQ_LOCK_ASSERT(_tq)	do { KASSERTMSG(mutex_owned(&(_tq)->axq_lock), "txq lock unheld"); } while (/*CONSTCOND*/true)

typedef kmutex_t ath_txbuf_lock_t;
#define	ATH_TXBUF_LOCK_INIT(_sc)	mutex_init(&(_sc)->sc_txbuflock, MUTEX_DEFAULT, IPL_NET)
#define	ATH_TXBUF_LOCK_DESTROY(_sc)	mutex_destroy(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_LOCK(_sc)		mutex_enter(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_UNLOCK(_sc)		mutex_exit(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_LOCK_ASSERT(_sc)	do { KASSERTMSG(mutex_owned(&(_sc)->sc_txbuflock), "txbuf lock unheld"); } while (/*CONSTCOND*/true)

#define	NET_LOCK_GIANT()
#define	NET_UNLOCK_GIANT()

#define	SYSCTL_INT_SUBR(__rw, __name, __descr)				     \
	sysctl_createv(log, 0, &rnode, &cnode, CTLFLAG_PERMANENT|(__rw),     \
	    CTLTYPE_INT, #__name, SYSCTL_DESCR(__descr), ath_sysctl_##__name,\
	    0, (void *)sc, 0, CTL_CREATE, CTL_EOL)

#define	__PFX(__pfx, __name)	__pfx##__name

#define	SYSCTL_PFX_INT(__pfx, __rw, __name, __descr)			\
	sysctl_createv(log, 0, &rnode, &cnode, CTLFLAG_PERMANENT|(__rw),\
	    CTLTYPE_INT, #__name, SYSCTL_DESCR(__descr), NULL, 0,	\
	    __PFX(&__pfx, __name), 0, CTL_CREATE, CTL_EOL)

#define	SYSCTL_INT(__rw, __name, __descr)				\
	SYSCTL_PFX_INT(sc->sc_, __rw, __name, __descr)

#define	SYSCTL_GLOBAL_INT(__rw, __name, __descr, __var)			\
	sysctl_createv(clog, 0, &rnode, &cnode,				\
	    CTLFLAG_PERMANENT|(__rw), CTLTYPE_INT, __name,		\
	    SYSCTL_DESCR(__descr), NULL, 0, &ath_##__var, 0, CTL_CREATE,\
	    CTL_EOL)

const struct sysctlnode *ath_sysctl_treetop(struct sysctllog **);
const struct sysctlnode *ath_sysctl_instance(const char *, struct sysctllog **);

#endif /* _ATH_NETBSD_H */

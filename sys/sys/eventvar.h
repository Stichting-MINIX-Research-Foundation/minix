/*	$NetBSD: eventvar.h,v 1.8 2008/03/21 21:53:35 ad Exp $	*/

/*-
 * Copyright (c) 1999,2000 Jonathan Lemon <jlemon@FreeBSD.org>
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
 *
 * FreeBSD: src/sys/sys/eventvar.h,v 1.4 2000/07/18 19:31:48 jlemon Exp
 */

/*
 * This header is provided for the kqueue implementation and kmem
 * grovellers, and is not expected to be used elsewhere.
 */

#ifndef _SYS_EVENTVAR_H_
#define	_SYS_EVENTVAR_H_

#include <sys/mutex.h>
#include <sys/selinfo.h>
#include <sys/filedesc.h>

#define	KQ_NEVENTS	8		/* minimize copy{in,out} calls */
#define	KQ_EXTENT	256		/* linear growth by this amount */
#define	KFILTER_MAXNAME	256		/* maximum size of a filter name */
#define	KFILTER_EXTENT	8		/* grow user_kfilters by this amt */

struct kqueue {
	TAILQ_HEAD(kqlist, knote) kq_head;	/* list of pending event */
	kmutex_t	kq_lock;		/* mutex for queue access */
	filedesc_t	*kq_fdp;
	struct selinfo	kq_sel;
	kcondvar_t	kq_cv;
	int		kq_count;		/* number of pending events */
};

#endif /* !_SYS_EVENTVAR_H_ */

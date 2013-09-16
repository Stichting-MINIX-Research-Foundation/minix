/*	$NetBSD: workqueue.h,v 1.9 2007/10/19 12:16:48 ad Exp $	*/

/*-
 * Copyright (c)2002, 2005 YAMAMOTO Takashi,
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

#ifndef _SYS_WORKQUEUE_H_
#define	_SYS_WORKQUEUE_H_

struct cpu_info;

/*
 * a simple "do it in thread context" framework.
 *
 * this framework is designed to make struct work small as
 * far as possible, so that it can be embedded into other structures
 * without too much cost.
 */

struct work {
	void *wk_dummy;
};

struct workqueue;

#define	WQ_MPSAFE	0x01
#define	WQ_PERCPU	0x02

int workqueue_create(struct workqueue **, const char *,
    void (*)(struct work *, void *), void *, pri_t, int, int);
void workqueue_destroy(struct workqueue *);

void workqueue_enqueue(struct workqueue *, struct work *, struct cpu_info *);

#endif /* _SYS_WORKQUEUE_H_ */

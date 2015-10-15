/*	$NetBSD: ipi.h,v 1.3 2015/01/18 23:16:35 rmind Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mindaugas Rasiukevicius.
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

#ifndef _SYS_IPI_H_
#define _SYS_IPI_H_

#if !defined(_KERNEL) && !defined(_KMEMUSER)
#error "not supposed to be exposed to userland"
#endif

typedef void (*ipi_func_t)(void *);

typedef struct {
	/* Public: function handler and an argument. */
	ipi_func_t	func;
	void *		arg;

	/* Private (internal) elements. */
	volatile u_int	_pending;
} ipi_msg_t;

/*
 * Internal constants and implementation hooks.
 *
 * IPI_MAXREG: the maximum number of asynchronous handlers which can
 * be registered on the system (normally, this should not be high).
 */
#define	IPI_MAXREG	32

#define	IPI_BITW_SHIFT	5
#define	IPI_BITW_MASK	(32 - 1)
#define	IPI_BITWORDS	(IPI_MAXREG >> IPI_BITW_SHIFT)

void	ipi_sysinit(void);
void	ipi_cpu_handler(void);
void	cpu_ipi(struct cpu_info *);

/* Public interface: asynchronous IPIs. */
u_int	ipi_register(ipi_func_t, void *);
void	ipi_unregister(u_int);
void	ipi_trigger(u_int, struct cpu_info *);
void	ipi_trigger_multi(u_int, const kcpuset_t *);

/* Public interface: synchronous IPIs. */
void	ipi_unicast(ipi_msg_t *, struct cpu_info *);
void	ipi_multicast(ipi_msg_t *, const kcpuset_t *);
void	ipi_broadcast(ipi_msg_t *);
void	ipi_wait(ipi_msg_t *);

#endif

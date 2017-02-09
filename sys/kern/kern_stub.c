/*	$NetBSD: kern_stub.c,v 1.42 2015/08/28 07:18:39 knakahara Exp $	*/

/*-
 * Copyright (c) 2007, 2008 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)subr_xxx.c	8.3 (Berkeley) 3/29/95
 */

/*
 * Stubs for system calls and facilities not included in the system.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_stub.c,v 1.42 2015/08/28 07:18:39 knakahara Exp $");

#ifdef _KERNEL_OPT
#include "opt_ptrace.h"
#include "opt_ktrace.h"
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fstypes.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <sys/intr.h>
#include <sys/cpu.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/userconf.h>

bool default_bus_space_is_equal(bus_space_tag_t, bus_space_tag_t);
bool default_bus_space_handle_is_equal(bus_space_tag_t, bus_space_handle_t,
    bus_space_handle_t);

/*
 * Nonexistent system call-- signal process (may want to handle it).  Flag
 * error in case process won't see signal immediately (blocked or ignored).
 */
#ifndef PTRACE
__strong_alias(sys_ptrace,sys_nosys);
#endif	/* PTRACE */

/*
 * ktrace stubs.  ktruser() goes to enosys as we want to fail the syscall,
 * but not kill the process: utrace() is a debugging feature.
 */
#ifndef KTRACE
__strong_alias(ktr_csw,nullop);		/* Probes */
__strong_alias(ktr_emul,nullop);
__strong_alias(ktr_geniov,nullop);
__strong_alias(ktr_genio,nullop);
__strong_alias(ktr_mibio,nullop);
__strong_alias(ktr_namei,nullop);
__strong_alias(ktr_namei2,nullop);
__strong_alias(ktr_psig,nullop);
__strong_alias(ktr_syscall,nullop);
__strong_alias(ktr_sysret,nullop);
__strong_alias(ktr_kuser,nullop);
__strong_alias(ktr_mib,nullop);
__strong_alias(ktr_execarg,nullop);
__strong_alias(ktr_execenv,nullop);
__strong_alias(ktr_execfd,nullop);

__strong_alias(sys_fktrace,sys_nosys);	/* Syscalls */
__strong_alias(sys_ktrace,sys_nosys);
__strong_alias(sys_utrace,sys_nosys);

int	ktrace_on;			/* Misc */
__strong_alias(ktruser,enosys);
__strong_alias(ktr_point,nullop);
#endif	/* KTRACE */

__weak_alias(device_register, voidop);
__weak_alias(device_register_post_config, voidop);
__weak_alias(spldebug_start, voidop);
__weak_alias(spldebug_stop, voidop);
__weak_alias(machdep_init,nullop);
__weak_alias(pci_chipset_tag_create, eopnotsupp);
__weak_alias(pci_chipset_tag_destroy, voidop);
__weak_alias(bus_space_reserve, eopnotsupp);
__weak_alias(bus_space_reserve_subregion, eopnotsupp);
__weak_alias(bus_space_release, voidop);
__weak_alias(bus_space_reservation_map, eopnotsupp);
__weak_alias(bus_space_reservation_unmap, voidop);
__weak_alias(bus_dma_tag_create, eopnotsupp);
__weak_alias(bus_dma_tag_destroy, voidop);
__weak_alias(bus_space_tag_create, eopnotsupp);
__weak_alias(bus_space_tag_destroy, voidop);
__strict_weak_alias(bus_space_is_equal, default_bus_space_is_equal);
__strict_weak_alias(bus_space_handle_is_equal,
    default_bus_space_handle_is_equal);
__weak_alias(userconf_bootinfo, voidop);
__weak_alias(userconf_init, voidop);
__weak_alias(userconf_prompt, voidop);

__weak_alias(kobj_renamespace, nullop);

__weak_alias(interrupt_get_count, nullop);
__weak_alias(interrupt_get_assigned, voidop);
__weak_alias(interrupt_get_available, voidop);
__weak_alias(interrupt_get_devname, voidop);
__weak_alias(interrupt_construct_intrids, nullret);
__weak_alias(interrupt_destruct_intrids, voidop);
__weak_alias(interrupt_distribute, eopnotsupp);
__weak_alias(interrupt_distribute_handler, eopnotsupp);

/*
 * Scheduler activations system calls.  These need to remain until libc's
 * major version is bumped.
 */
__strong_alias(sys_sa_register,sys_nosys);
__strong_alias(sys_sa_stacks,sys_nosys);
__strong_alias(sys_sa_enable,sys_nosys);
__strong_alias(sys_sa_setconcurrency,sys_nosys);
__strong_alias(sys_sa_yield,sys_nosys);
__strong_alias(sys_sa_preempt,sys_nosys);
__strong_alias(sys_sa_unblockyield,sys_nosys);

/*
 * Stubs for compat_netbsd32.
 */
__strong_alias(dosa_register,sys_nosys);
__strong_alias(sa_stacks1,sys_nosys);

/*
 * Stubs for architectures that do not support kernel preemption.
 */
#ifndef __HAVE_PREEMPTION
bool
cpu_kpreempt_enter(uintptr_t where, int s)
{

	return false;
}

void
cpu_kpreempt_exit(uintptr_t where)
{

}

bool
cpu_kpreempt_disabled(void)
{

	return true;
}
#else
# ifndef MULTIPROCESSOR
#   error __HAVE_PREEMPTION requires MULTIPROCESSOR
# endif
#endif	/* !__HAVE_PREEMPTION */

int
sys_nosys(struct lwp *l, const void *v, register_t *retval)
{

	mutex_enter(proc_lock);
	psignal(l->l_proc, SIGSYS);
	mutex_exit(proc_lock);
	return ENOSYS;
}

/*
 * Unsupported device function (e.g. writing to read-only device).
 */
int
enodev(void)
{

	return (ENODEV);
}

/*
 * Unconfigured device function; driver not configured.
 */
int
enxio(void)
{

	return (ENXIO);
}

/*
 * Unsupported ioctl function.
 */
int
enoioctl(void)
{

	return (ENOTTY);
}

/*
 * Unsupported system function.
 * This is used for an otherwise-reasonable operation
 * that is not supported by the current system binary.
 */
int
enosys(void)
{

	return (ENOSYS);
}

/*
 * Return error for operation not supported
 * on a specific object or file type.
 */
int
eopnotsupp(void)
{

	return (EOPNOTSUPP);
}

/*
 * Generic null operation, void return value.
 */
void
voidop(void)
{
}

/*
 * Generic null operation, always returns success.
 */
int
nullop(void *v)
{

	return (0);
}

/*
 * Generic null operation, always returns null.
 */
void *
nullret(void)
{

	return (NULL);
}

bool
default_bus_space_handle_is_equal(bus_space_tag_t t,
    bus_space_handle_t h1, bus_space_handle_t h2)
{

	return memcmp(&h1, &h2, sizeof(h1)) == 0;
}

bool
default_bus_space_is_equal(bus_space_tag_t t1, bus_space_tag_t t2)
{

	return memcmp(&t1, &t2, sizeof(t1)) == 0;
}

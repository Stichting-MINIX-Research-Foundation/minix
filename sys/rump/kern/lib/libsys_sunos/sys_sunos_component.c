/*	$NetBSD: sys_sunos_component.c,v 1.1 2014/03/13 02:04:14 pooka Exp $	*/

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include "rump_private.h"

#include "rump_sunos_syscall.h"

extern struct sysent rump_sunos_sysent[];

struct emul emul_rump_sys_sunos = {
	.e_name = "sunos-rump",
	.e_sysent = rump_sunos_sysent,
#ifndef __HAVE_MINIMAL_EMUL
	.e_nsysent = RUMP_SUNOS_SYS_NSYSENT,
#endif
	.e_vm_default_addr = uvm_default_mapaddr,
#ifdef __HAVE_SYSCALL_INTERN
	.e_syscall_intern = syscall_intern,
#endif
};

RUMP_COMPONENT(RUMP_COMPONENT_KERN)
{
	extern struct emul *emul_default;

	emul_default = &emul_rump_sys_sunos;
}

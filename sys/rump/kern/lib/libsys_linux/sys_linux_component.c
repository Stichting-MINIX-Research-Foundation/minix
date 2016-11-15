/*	$NetBSD: sys_linux_component.c,v 1.2 2014/04/04 18:24:12 njoly Exp $	*/

#include <sys/param.h>
#include <sys/proc.h>

#include <compat/linux/common/linux_errno.h>

#include <uvm/uvm_extern.h>

#include "rump_private.h"

#include "rump_linux_syscall.h"

extern struct sysent rump_linux_sysent[];

#ifdef __HAVE_SYSCALL_INTERN
static void
rumplinux_syscall_intern(struct proc *p)
{

	p->p_emuldata = __UNCONST(native_to_linux_errno);
}
#endif

struct emul emul_rump_sys_linux = {
	.e_name = "linux-rump",
	.e_sysent = rump_linux_sysent,
#ifndef __HAVE_MINIMAL_EMUL
	.e_nsysent = RUMP_LINUX_SYS_NSYSENT,
	.e_errno = native_to_linux_errno,
#endif
	.e_vm_default_addr = uvm_default_mapaddr,
#ifdef __HAVE_SYSCALL_INTERN
	.e_syscall_intern = rumplinux_syscall_intern,
#endif
};

RUMP_COMPONENT(RUMP_COMPONENT_KERN)
{
	extern struct emul *emul_default;

	emul_default = &emul_rump_sys_linux;
}

#include <compat/linux/common/linux_machdep.h>

dev_t
linux_fakedev(dev_t in, int raw)
{

	/* I don't really think it matters what we return here */
	return in;
}

/*
 * XXX: the linux emulation code is not split into factions
 */
void rumplinux__stub(void);
void rumplinux__stub(void) {panic("unavailable");}

/* vm-related */
__weak_alias(sys_mmap,rumplinux__stub);
__weak_alias(vm_map_unlock,rumplinux__stub);
__weak_alias(uvm_map_lookup_entry,rumplinux__stub);
__weak_alias(sys_obreak,rumplinux__stub);
__weak_alias(vm_map_lock,rumplinux__stub);
__weak_alias(uvm_mremap,rumplinux__stub);

/* signal.c */
__weak_alias(sigaction1,rumplinux__stub);
__weak_alias(kpsignal2,rumplinux__stub);
__weak_alias(sys_kill,rumplinux__stub);
__weak_alias(sigsuspend1,rumplinux__stub);
__weak_alias(sigtimedwait1,rumplinux__stub);
__weak_alias(lwp_find,rumplinux__stub);

/* misc */
__weak_alias(linux_machdepioctl,rumplinux__stub);
__weak_alias(linux_ioctl_sg,rumplinux__stub);
__weak_alias(oss_ioctl_mixer,rumplinux__stub);
__weak_alias(oss_ioctl_sequencer,rumplinux__stub);
__weak_alias(oss_ioctl_audio,rumplinux__stub);
__weak_alias(rusage_to_rusage50,rumplinux__stub);
__weak_alias(do_sys_wait,rumplinux__stub);

/* arch-specific */
__weak_alias(compat_offseterr,rumplinux__stub);
__weak_alias(linux_sys_ptrace_arch,rumplinux__stub);

#ifdef __i386__
const char *
linux_get_uname_arch(void)
{

	return MACHINE_ARCH;
}
#endif /* __i386__ */

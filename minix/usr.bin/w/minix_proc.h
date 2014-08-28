#ifndef _W_MINIX_PROC_H
#define _W_MINIX_PROC_H

/*
 * This header file is tailored very specifically to the needs of w(1), which
 * needs process information but uses a BSD infrastructure (namely, the Kernel
 * Data Access Library or, eh, KVM.. huh?) which we cannot implement in MINIX3
 * without making a very, very large number of changes all over the place.
 *
 * In order to allow w(1) to function on MINIX3, we override some of the KVM
 * functionality with MINIX3 implementations, by means of #defines. While that
 * is indeed butt ugly, this approach does have advantages: we are able to
 * implement the full expected functionality with minimal changes to w(1)
 * itself, and whenever w(1) uses a KVM function or a process information field
 * that this header does not supply, w(1) will break during compilation.
 */

struct minix_proc {
	pid_t p_pid;		/* process ID */
	pid_t p__pgid;		/* process group ID */
	pid_t p_tpgid;		/* tty process group ID (= p_pgid or 0) */
	dev_t p_tdev;		/* controlling terminal (= tty rdev or 0) */
	char p_minix_state;	/* minix specific: process kernel state */
	char p_minix_pstate;	/* minix specific: process PM state */
	char p_comm[256];	/* simple command line (= process name) */
};
#undef kinfo_proc2
#define kinfo_proc2 minix_proc

struct minix_proc *minix_getproc(void *dummy, int op, int arg, int elemsize,
	int *cnt);
#undef kvm_getproc2
#define kvm_getproc2 minix_getproc

#undef kvm_geterr
#define kvm_geterr(dummy) strerror(errno)

char **minix_getargv(void *dummy, const struct minix_proc *p, int nchr);
#undef kvm_getargv2
#define kvm_getargv2 minix_getargv

int minix_proc_compare(const struct minix_proc *p1,
	const struct minix_proc *p2);
#undef proc_compare_wrapper
#define proc_compare_wrapper minix_proc_compare

int minix_getuptime(time_t *timep);

#endif /* !_W_MINIX_PROC_H */

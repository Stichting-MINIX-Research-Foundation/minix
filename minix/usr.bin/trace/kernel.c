/*
 * This file, and only this file, should contain all the ugliness needed to
 * obtain values from the kernel.  It has to be recompiled every time the
 * layout of the kernel "struct proc" and/or "struct priv" structures changes.
 * In addition, this file contains the platform-dependent code related to
 * interpreting the registers exposed by the kernel.
 *
 * As a quick note, some functions return TRUE/FALSE, and some return 0/-1.
 * The former convention is used for functions that return a boolean value;
 * the latter is used for functions that set errno in all cases of failure,
 * and where the caller may conceivably use errno as a result.
 *
 * On a related note, relevant here and elsewhere: we define _MINIX_SYSTEM but
 * not _SYSTEM, which means that we should not get negative error numbers.
 */

#include "inc.h"

#include <machine/archtypes.h>
#include <minix/timers.h>
#include "kernel/proc.h"
#include "kernel/priv.h"
#if defined(__i386__)
#include "kernel/arch/i386/include/archconst.h" /* for the KTS_ constants */
#endif
#include <lib.h>

/*
 * Working area.  By obtaining values from the kernel into these local process
 * structures, and then returning them, we gain a little robustness against
 * changes in data types of the fields we need.
 */
static struct proc kernel_proc;
static struct priv kernel_priv;

/*
 * Check whether our notion of the kernel process structure layout matches that
 * of the kernel, by comparing magic values.  This can be done only once we
 * have attached to a process.  Return TRUE if everything seems alright; FALSE
 * otherwise.
 */
int
kernel_check(pid_t pid)
{

	if (mem_get_user(pid, offsetof(struct proc, p_magic),
	    &kernel_proc.p_magic, sizeof(kernel_proc.p_magic)) < 0)
		return FALSE;

	return (kernel_proc.p_magic == PMAGIC);
}

/*
 * Obtain the kernel name for the given (stopped) process.  Return 0 on
 * success, with the (possibly truncated) name stored in the 'name' buffer
 * which is of 'size' bytes; the name will be null-terminated.  Note that the
 * name may contain any suffixes as set by the kernel.  Return -1 on failure,
 * with errno set as appropriate.
 */
int
kernel_get_name(pid_t pid, char * name, size_t size)
{

	if (mem_get_user(pid, offsetof(struct proc, p_name),
	    kernel_proc.p_name, sizeof(kernel_proc.p_name)) < 0)
		return -1;

	strlcpy(name, kernel_proc.p_name, size);
	return 0;
}

/*
 * Check whether the given process, which we have just attached to, is a system
 * service.  PM does not prevent us from attaching to most system services,
 * even though this utility only supports tracing user programs.  Unlike a few
 * other routines in this file, this function can not use ProcFS to obtain its
 * result, because the given process may actually be VFS or ProcFS itself!
 * Return TRUE if the given process is a system service; FALSE if not.
 */
int
kernel_is_service(pid_t pid)
{
	size_t align, off;

	/*
	 * For T_GETUSER, the priv structure follows the proc structure, but
	 * possibly with padding in between so as to align the priv structure
	 * to long boundary.
	 */
	align = sizeof(long) - 1;
	off = (sizeof(struct proc) + align) & ~align;

	if (mem_get_user(pid, off + offsetof(struct priv, s_id),
	    &kernel_priv.s_id, sizeof(kernel_priv.s_id)) < 0)
		return FALSE; /* process may have disappeared, so no danger */

	return (kernel_priv.s_id != USER_PRIV_ID);
}

/*
 * For the given process, which must be stopped on entering a system call,
 * retrieve the three register values describing the system call.  Return 0 on
 * success, or -1 on failure with errno set as appropriate.
 */
int
kernel_get_syscall(pid_t pid, reg_t reg[3])
{

	assert(sizeof(kernel_proc.p_defer) == sizeof(reg_t) * 3);

	if (mem_get_user(pid, offsetof(struct proc, p_defer),
	    &kernel_proc.p_defer, sizeof(kernel_proc.p_defer)) < 0)
		return -1;

	reg[0] = kernel_proc.p_defer.r1;
	reg[1] = kernel_proc.p_defer.r2;
	reg[2] = kernel_proc.p_defer.r3;
	return 0;
}

/*
 * Retrieve the value of the primary return register for the given process,
 * which must be stopped on leaving a system call.  This register contains the
 * IPC-level result of the system call.  Return 0 on success, or -1 on failure
 * with errno set as appropriate.
 */
int
kernel_get_retreg(pid_t pid, reg_t * retreg)
{
	size_t off;

	/*
	 * Historically p_reg had to be the first field in the proc structure,
	 * but since this is no longer a hard requirement, getting its actual
	 * offset into the proc structure certainly doesn't hurt.
	 */
	off = offsetof(struct proc, p_reg);

	if (mem_get_user(pid, off + offsetof(struct stackframe_s, retreg),
	    &kernel_proc.p_reg.retreg, sizeof(kernel_proc.p_reg.retreg)) < 0)
		return -1;

	*retreg = kernel_proc.p_reg.retreg;
	return 0;
}

/*
 * Return the stack top for user processes.  This is needed for execve(), since
 * the supplied frame contains pointers prepared for the new location of the
 * frame, which is at the stack top of the process after the execve().
 */
vir_bytes
kernel_get_stacktop(void)
{

	return minix_get_user_sp();
}

/*
 * For the given stopped process, get its program counter (pc), stack pointer
 * (sp), and optionally its frame pointer (fp).  The given fp pointer may be
 * NULL, in which case the frame pointer is not obtained.  The given pc and sp
 * pointers must not be NULL, and this is intentional: obtaining fp may require
 * obtaining sp first.  Return 0 on success, or -1 on failure with errno set
 * as appropriate.  This functionality is not essential for tracing processes,
 * and may not be supported on all platforms, in part or full.  In particular,
 * on some platforms, a zero (= invalid) frame pointer may be returned on
 * success, indicating that obtaining frame pointers is not supported.
 */
int
kernel_get_context(pid_t pid, reg_t * pc, reg_t * sp, reg_t * fp)
{
	size_t off;

	off = offsetof(struct proc, p_reg); /* as above */

	if (mem_get_user(pid, off + offsetof(struct stackframe_s, pc),
	    &kernel_proc.p_reg.pc, sizeof(kernel_proc.p_reg.pc)) < 0)
		return -1;
	if (mem_get_user(pid, off + offsetof(struct stackframe_s, sp),
	    &kernel_proc.p_reg.sp, sizeof(kernel_proc.p_reg.sp)) < 0)
		return -1;

	*pc = kernel_proc.p_reg.pc;
	*sp = kernel_proc.p_reg.sp;

	if (fp == NULL)
		return 0;

#if defined(__i386__)
	if (mem_get_user(pid, offsetof(struct proc, p_seg) +
	    offsetof(struct segframe, p_kern_trap_style),
	    &kernel_proc.p_seg.p_kern_trap_style,
	    sizeof(kernel_proc.p_seg.p_kern_trap_style)) < 0)
		return -1;

	/* This is taken from the kernel i386 exception code. */
	switch (kernel_proc.p_seg.p_kern_trap_style) {
	case KTS_SYSENTER:
	case KTS_SYSCALL:
		if (mem_get_data(pid, *sp + 16, fp, sizeof(fp)) < 0)
			return -1;
		break;

	default:
		if (mem_get_user(pid, off + offsetof(struct stackframe_s, fp),
		    &kernel_proc.p_reg.fp, sizeof(kernel_proc.p_reg.fp)) < 0)
			return -1;

		*fp = kernel_proc.p_reg.fp;
	}
#else
	*fp = 0; /* not supported; this is not a failure (*pc is valid) */
#endif
	return 0;
}

/*
 * Given a frame pointer, obtain the next program counter and frame pointer.
 * Return 0 if successful, or -1 on failure with errno set appropriately.  The
 * functionality is not essential for tracing processes, and may not be
 * supported on all platforms.  Thus, on some platforms, this function may
 * always fail.
 */
static int
kernel_get_nextframe(pid_t pid, reg_t fp, reg_t * next_pc, reg_t * next_fp)
{
#if defined(__i386__)
	void *p[2];

	if (mem_get_data(pid, (vir_bytes)fp, &p, sizeof(p)) < 0)
		return -1;

	*next_pc = (reg_t)p[1];
	*next_fp = (reg_t)p[0];
	return 0;
#else
	/* Not supported (yet). */
	errno = ENOSYS;
	return -1;
#endif
}

/*
 * Print a stack trace for the given process, which is known to be stopped on
 * entering a system call.  This function does not really belong here, but
 * without a doubt it is going to have to be fully rewritten to support
 * anything other than i386.
 *
 * Getting symbol names is currently an absolute nightmare.  Not just because
 * of shared libraries, but also since ProcFS does not offer a /proc/NNN/exe,
 * so that we cannot reliably determine the binary being executed: not for
 * processes being attached to, and not for exec calls using a relative path.
 */
void
kernel_put_stacktrace(struct trace_proc * procp)
{
	unsigned int count, max;
	reg_t pc, sp, fp, low, high;

	if (kernel_get_context(procp->pid, &pc, &sp, &fp) < 0)
		return;

	/*
	 * A low default limit such as 6 looks much prettier, but is simply not
	 * useful enough for moderately-sized programs in practice.  Right now,
	 * 15 is about two lines on a 80-column terminal.
	 */
	if (verbose == 0) max = 15;
	else if (verbose == 1) max = 31;
	else max = UINT_MAX;

	/*
	 * We keep formatting to an absolute minimum, to facilitate passing
	 * the lines straight into tools such as addr2line.
	 */
	put_newline();
	put_fmt(procp, "  0x%x", pc);

	low = high = fp;

	for (count = 1; count < max && fp != 0; count++) {
		if (kernel_get_nextframe(procp->pid, fp, &pc, &fp) < 0)
			break;

		put_fmt(procp, " 0x%x", pc);

		/*
		 * Stop if we see a frame pointer that falls within the range
		 * of the frame pointers we have seen so far.  This also
		 * prevents getting stuck in a loop on the same frame pointer.
		 */
		if (fp >= low && fp <= high)
			break;
		if (low > fp)
			low = fp;
		if (high < fp)
			high = fp;
	}

	if (fp != 0)
		put_text(procp, " ..");
	put_newline();
}

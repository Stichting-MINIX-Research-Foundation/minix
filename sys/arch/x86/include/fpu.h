/*	$NetBSD: fpu.h,v 1.6 2014/02/25 22:16:52 dsl Exp $	*/

#ifndef	_X86_FPU_H_
#define	_X86_FPU_H_

#include <x86/cpu_extended_state.h>

#ifdef _KERNEL

struct cpu_info;
struct lwp;
struct trapframe;

void fpuinit(struct cpu_info *);
void fpusave_lwp(struct lwp *, bool);
void fpusave_cpu(bool);

void fpu_set_default_cw(struct lwp *, unsigned int);

void fputrap(struct trapframe *);
void fpudna(struct trapframe *);

void process_xmm_to_s87(const struct fxsave *, struct save87 *);
void process_s87_to_xmm(const struct save87 *, struct fxsave *);

/* Set all to defaults (eg during exec) */
void fpu_save_area_clear(struct lwp *, unsigned int);
/* Reset control words only - for signal handlers */
void fpu_save_area_reset(struct lwp *);

/* Copy data outside pcb during fork */
void fpu_save_area_fork(struct pcb *, const struct pcb *);

/* Load FP registers with user-supplied values */
void process_write_fpregs_xmm(struct lwp *lwp, const struct fxsave *fpregs);
void process_write_fpregs_s87(struct lwp *lwp, const struct save87 *fpregs);

/* Save FP registers for copy to userspace */
void process_read_fpregs_xmm(struct lwp *lwp, struct fxsave *fpregs);
void process_read_fpregs_s87(struct lwp *lwp, struct save87 *fpregs);

#endif

#endif /* _X86_FPU_H_ */

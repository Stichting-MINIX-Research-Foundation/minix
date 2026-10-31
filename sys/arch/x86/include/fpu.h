/*	$NetBSD: fpu.h,v 1.17 2019/06/26 12:30:13 mgorny Exp $	*/

#ifndef	_X86_FPU_H_
#define	_X86_FPU_H_

#include <x86/cpu_extended_state.h>

#ifdef _KERNEL

struct cpu_info;
struct lwp;
struct trapframe;

void fpuinit(struct cpu_info *);
void fpuinit_mxcsr_mask(void);

void fpu_area_save(void *, uint64_t);
void fpu_area_restore(void *, uint64_t);

void fpusave_lwp(struct lwp *, bool);
void fpusave_cpu(bool);

void fpu_eagerswitch(struct lwp *, struct lwp *);

void fpu_set_default_cw(struct lwp *, unsigned int);

void fputrap(struct trapframe *);
void fpudna(struct trapframe *);

void fpu_clear(struct lwp *, unsigned int);
void fpu_sigreset(struct lwp *);

void fpu_save_area_fork(struct pcb *, const struct pcb *);

void process_write_fpregs_xmm(struct lwp *, const struct fxsave *);
void process_write_fpregs_s87(struct lwp *, const struct save87 *);

void process_read_fpregs_xmm(struct lwp *, struct fxsave *);
void process_read_fpregs_s87(struct lwp *, struct save87 *);

int process_read_xstate(struct lwp *, struct xstate *);
int process_verify_xstate(const struct xstate *);
int process_write_xstate(struct lwp *, const struct xstate *);

#endif

#endif /* _X86_FPU_H_ */

#ifndef _AMD64_FPU_H_
#define _AMD64_FPU_H_

/*
 * This file is only present for backwards compatibility with
 * a few user programs, particularly firefox.
 */

#ifndef _KERNEL
#define fxsave64 fxsave
#include <x86/cpu_extended_state.h>
#endif

#if defined(__minix) && (defined(_LIBMINC) || !defined(_STANDALONE))
/*
 * MINIX FPU state types used in struct sigcontext.
 * Kept here so that <machine/signal.h> can include <machine/fpu.h>
 * and always find these definitions regardless of header install order.
 */
#ifndef FPU_H
#define FPU_H

#include <sys/types.h>

struct fpu_regs_s {
	u_int16_t fp_control;
	u_int16_t fp_unused_1;
	u_int16_t fp_status;
	u_int16_t fp_unused_2;
	u_int16_t fp_tag;
	u_int16_t fp_unused_3;
	u_int32_t fp_eip;
	u_int16_t fp_cs;
	u_int16_t fp_opcode;
	u_int32_t fp_dp;
	u_int16_t fp_ds;
	u_int16_t fp_unused_4;
	u_int16_t fp_st_regs[8][5];
};

struct xfp_save {
	u_int16_t fp_control;
	u_int16_t fp_status;
	u_int16_t fp_tag;
	u_int16_t fp_opcode;
	u_int32_t fp_eip;
	u_int16_t fp_cs;
	u_int16_t fp_unused_1;
	u_int32_t fp_dp;
	u_int16_t fp_ds;
	u_int16_t fp_unused_2;
	u_int32_t fp_mxcsr;
	u_int32_t fp_mxcsr_mask;
	u_int16_t fp_st_regs[8][8];
	u_int32_t fp_xreg_word[32];
	u_int32_t fp_padding[56];
};

#define FPU_XFP_SIZE	512

union fpu_state_u {
	struct fpu_regs_s fpu_regs;
	struct xfp_save   xfp_regs;
};

#endif /* FPU_H */
#endif /* __minix && !_STANDALONE */

#endif /* _AMD64_FPU_H_ */

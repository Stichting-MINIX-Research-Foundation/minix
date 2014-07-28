#ifndef FPU_H
#define FPU_H

/* x87 FPU state, MMX Technolodgy.
 * 108 bytes.*/
struct fpu_regs_s {
	u16_t fp_control;     /* control */
	u16_t fp_unused_1;
	u16_t fp_status;      /* status */
	u16_t fp_unused_2;
	u16_t fp_tag;         /* register tags */
	u16_t fp_unused_3;
	u32_t fp_eip;         /* eip at failed instruction */
	u16_t fp_cs;          /* cs at failed instruction */
	u16_t fp_opcode;      /* opcode of failed instruction */
	u32_t fp_dp;          /* data address */
	u16_t fp_ds;          /* data segment */
	u16_t fp_unused_4;
	u16_t fp_st_regs[8][5]; /* 8 80-bit FP registers */
};

/* x87 FPU, MMX Technolodgy and SSE state.
 * 512 bytes (if you need size use FPU_XFP_SIZE). */
struct xfp_save {
	u16_t fp_control;       /* control */
	u16_t fp_status;        /* status */
	u16_t fp_tag;           /* register tags */
	u16_t fp_opcode;        /* opcode of failed instruction */
	u32_t fp_eip;           /* eip at failed instruction */
	u16_t fp_cs;            /* cs at failed instruction */
	u16_t fp_unused_1;
	u32_t fp_dp;            /* data address */
	u16_t fp_ds;            /* data segment */
	u16_t fp_unused_2;
	u32_t fp_mxcsr;         /* MXCSR */
	u32_t fp_mxcsr_mask;    /* MXCSR_MASK */
	u16_t fp_st_regs[8][8];   /* 128 bytes for ST/MM regs */
	u32_t fp_xreg_word[32]; /* space for 8 128-bit XMM registers */
	u32_t fp_padding[56];
};

/* Size of xfp_save structure. */
#define FPU_XFP_SIZE		512

union fpu_state_u {
	struct fpu_regs_s fpu_regs;
	struct xfp_save xfp_regs;
};

#endif /* #ifndef FPU_H */

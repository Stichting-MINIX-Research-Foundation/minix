#ifndef _ARM_CPUFUNC_TIMER_H
#define _ARM_CPUFUNC_TIMER_H

/* Read CNTFRQ */
static inline u32_t read_cntfrq() {
	u32_t cntfrq;
	asm volatile("mrc p15, 0, %[cntfrq], c14, c0 , 0 @ READ CNTFRQ\n\t"
				: [cntfrq] "=r" (cntfrq));
	return cntfrq;
}

/* Read CNTV_TVAL, virtual timer value register */
static inline i32_t read_cntv_tval() {
	i32_t cntv_tval;
	asm volatile("mrc p15, 0, %[cntv_tval], c14, c3 , 0 @ READ CNTV_TVAL\n\t"
				: [cntv_tval] "=r" (cntv_tval));
	return cntv_tval;
}

/* write CNTV_TVAL, virtual timer control register */
static inline void write_cntv_tval(i32_t val) {
	asm volatile("mcr p15, 0, %[val], c14, c3 , 0 @ WRITE CNTV_CTL\n\t"
				: : [val] "r" (val));
}

/* Read CNTV_CTL, virtual timer control register */
static inline u32_t read_cntv_ctl() {
	u32_t cntv_ctl;
	asm volatile("mrc p15, 0, %[cntv_ctl], c14, c3 , 1 @ READ CNTV_CTL\n\t"
				: [cntv_ctl] "=r" (cntv_ctl));
	return cntv_ctl;
}

/* write CNTV_CTL, virtual timer control register */
static inline void write_cntv_ctl(u32_t val) {
	asm volatile("mcr p15, 0, %[val], c14, c3 , 1 @ WRITE CNTV_CTL\n\t"
				: : [val] "r" (val));
}

/* Read CNTV_CVAL, virtual timer compare value register */
static inline u64_t read_cntv_cval() {
	u32_t cntv_cval_lo, cntv_cval_hi;
	asm volatile("mrrc p15, 3, %[cntv_cval_lo], %[cntv_cval_hi], c14 @ READ CNTV_CVAL\n\t"
				: [cntv_cval_lo] "=r" (cntv_cval_lo), [cntv_cval_hi] "=r" (cntv_cval_hi));
	return ((u64_t)cntv_cval_hi) << 32 | cntv_cval_lo;
}

/* Write CNTV_CVAL, virtual timer compare value register */
static inline void write_cntv_cval(u64_t val) {
	u32_t cntv_cval_lo = val & 0xFFFFFFFF, cntv_cval_hi = val >> 32;
	asm volatile("mcrr p15, 3, %[cntv_cval_lo], %[cntv_cval_hi], c14 @ WRITE CNTV_CVAL\n\t"
				: : [cntv_cval_lo] "r" (cntv_cval_lo), [cntv_cval_hi] "r" (cntv_cval_hi));
}

/* Read CNTVCT, virtual count register */
static inline u64_t read_cntvct() {
	u32_t cntvct_lo, cntvct_hi;
	asm volatile("mrrc p15, 1, %[cntvct_lo], %[cntvct_hi], c14 @ READ CNTVCT\n\t"
				: [cntvct_lo] "=r" (cntvct_lo), [cntvct_hi] "=r" (cntvct_hi));
	return ((u64_t)cntvct_hi) << 32 | cntvct_lo;
}

#define ARMTIMER_ENABLE          0x1
#define ARMTIMER_IMASK           0x2
#define ARMTIMER_ISTATUS         0x4

#endif /* _ARM_CPUFUNC_TIMER_H */

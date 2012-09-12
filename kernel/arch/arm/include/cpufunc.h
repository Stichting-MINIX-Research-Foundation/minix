#ifndef _ARM_CPUFUNC_H
#define _ARM_CPUFUNC_H

/* Data memory barrier */
static inline void dmb(void)
{
	asm volatile("dmb" : : : "memory");
}

/* Data synchronization barrier */
static inline void dsb(void)
{
	asm volatile("dsb" : : : "memory");
}

/* Instruction synchronization barrier */
static inline void isb(void)
{
	asm volatile("isb" : : : "memory");
}

static inline void barrier(void)
{
	dsb();
	isb();
}

static inline void refresh_tlb(void)
{
	dsb();
	/* Invalidate entire unified TLB */
	asm volatile("mcr p15, 0, r0, c8, c7, 0 @ TLBIALL\n\t");
	dsb();
	isb();
}


/* Read System Control Register */
static inline u32_t read_sctlr()
{
    u32_t ctl;

    asm volatile("mrc p15, 0, %[ctl], c1, c0, 0 @ Read SCTLR\n\t"
		 : [ctl] "=r" (ctl));
    return ctl;
}

/* Write System Control Register */
static inline void write_sctlr(u32_t ctl)
{
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 0 @ Write SCTLR\n\t"
		 : : [ctl] "r" (ctl));
}

/* Read Translation Table Base Register 0 */
static inline u32_t read_ttbr0()
{
    u32_t bar;

    asm volatile("mrc p15, 0, %[bar], c2, c0, 0 @ Read TTBR0\n\t"
		 : [bar] "=r" (bar));
    return bar;
}

/* Write Translation Table Base Register 0 */
static inline void write_ttbr0(u32_t bar)
{
    barrier();
    asm volatile("mcr p15, 0, %[bar], c2, c0, 0 @ Write TTBR0\n\t"
		 : : [bar] "r" (bar));
    refresh_tlb();
}

/* Reload Translation Table Base Register 0 */
static inline void reload_ttbr0(void)
{
    reg_t ttbr = read_ttbr0();
    write_ttbr0(ttbr);
    refresh_tlb();
}

/* Read Translation Table Base Register 1 */
static inline u32_t read_ttbr1()
{
    u32_t bar;

    asm volatile("mrc p15, 0, %[bar], c2, c0, 1 @ Read TTBR1\n\t"
		 : [bar] "=r" (bar));
    return bar;
}

/* Write Translation Table Base Register 1 */
static inline void write_ttbr1(u32_t bar)
{
    barrier();
    asm volatile("mcr p15, 0, %[bar], c2, c0, 1 @ Write TTBR1\n\t"
		 : : [bar] "r" (bar));
    refresh_tlb();
}

/* Reload Translation Table Base Register 1 */
static inline void reload_ttbr1(void)
{
    reg_t ttbr = read_ttbr1();
    write_ttbr1(ttbr);
    refresh_tlb();
}

/* Read Translation Table Base Control Register */
static inline u32_t read_ttbcr()
{
    u32_t bcr;

    asm volatile("mrc p15, 0, %[bcr], c2, c0, 2 @ Read TTBCR\n\t"
		 : [bcr] "=r" (bcr));
    return bcr;
}

/* Write Translation Table Base Control Register */
static inline void write_ttbcr(u32_t bcr)
{
    asm volatile("mcr p15, 0, %[bcr], c2, c0, 2 @ Write TTBCR\n\t"
		 : : [bcr] "r" (bcr));
}

/* Read Domain Access Control Register */
static inline u32_t read_dacr()
{
    u32_t dacr;

    asm volatile("mrc p15, 0, %[dacr], c3, c0, 0 @ Read DACR\n\t"
		 : [dacr] "=r" (dacr));
    return dacr;
}

/* Write Domain Access Control Register */
static inline void write_dacr(u32_t dacr)
{
    asm volatile("mcr p15, 0, %[dacr], c3, c0, 0 @ Write DACR\n\t"
		 : : [dacr] "r" (dacr));
}

/* Read Data Fault Status Register */
static inline u32_t read_dfsr()
{
    u32_t fsr;

    asm volatile("mrc p15, 0, %[fsr], c5, c0, 0 @ Read DFSR\n\t"
		 : [fsr] "=r" (fsr));
    return fsr;
}

/* Write Data Fault Status Register */
static inline void write_dfsr(u32_t fsr)
{
    asm volatile("mcr p15, 0, %[fsr], c5, c0, 0 @ Write DFSR\n\t"
		 : : [fsr] "r" (fsr));
}

/* Read Instruction Fault Status Register */
static inline u32_t read_ifsr()
{
    u32_t fsr;

    asm volatile("mrc p15, 0, %[fsr], c5, c0, 1 @ Read IFSR\n\t"
		 : [fsr] "=r" (fsr));
    return fsr;
}

/* Write Instruction Fault Status Register */
static inline void write_ifsr(u32_t fsr)
{
    asm volatile("mcr p15, 0, %[fsr], c5, c0, 1 @ Write IFSR\n\t"
		 : : [fsr] "r" (fsr));
}

/* Read Data Fault Address Register */
static inline u32_t read_dfar()
{
    u32_t far;

    asm volatile("mrc p15, 0, %[far], c6, c0, 0 @ Read DFAR\n\t"
		 : [far] "=r" (far));
    return far;
}

/* Write Data Fault Address Register */
static inline void write_dfar(u32_t far)
{
    asm volatile("mcr p15, 0, %[far], c6, c0, 0 @ Write DFAR\n\t"
		 : : [far] "r" (far));
}

/* Read Instruction Fault Address Register */
static inline u32_t read_ifar()
{
    u32_t far;

    asm volatile("mrc p15, 0, %[far], c6, c0, 2 @ Read IFAR\n\t"
		 : [far] "=r" (far));
    return far;
}

/* Write Instruction Fault Address Register */
static inline void write_ifar(u32_t far)
{
    asm volatile("mcr p15, 0, %[far], c6, c0, 2 @ Write IFAR\n\t"
		 : : [far] "r" (far));
}

/* Read Vector Base Address Register */
static inline u32_t read_vbar()
{
    u32_t vbar;

    asm volatile("mrc p15, 0, %[vbar], c12, c0, 0 @ Read VBAR\n\t"
		 : [vbar] "=r" (vbar));
    return vbar;
}

/* Write Vector Base Address Register */
static inline void write_vbar(u32_t vbar)
{
    asm volatile("mcr p15, 0, %[vbar], c12, c0, 0 @ Write VBAR\n\t"
		 : : [vbar] "r" (vbar));
    asm volatile("dsb");
}

/* Read the Main ID Register  */
static inline u32_t read_midr()
{
    u32_t id;

    asm volatile("mrc p15, 0, %[id], c0, c0, 0 @ read MIDR\n\t"
		 : [id] "=r" (id));
    return id;
}

/* Read Auxiliary Control Register */
static inline u32_t read_actlr()
{
    u32_t ctl;

    asm volatile("mrc p15, 0, %[ctl], c1, c0, 1 @ Read ACTLR\n\t"
		 : [ctl] "=r" (ctl));
    return ctl;
}

/* Write Auxiliary Control Register */
static inline void write_actlr(u32_t ctl)
{
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 1 @ Write ACTLR\n\t"
		 : : [ctl] "r" (ctl));
}

/* Read Current Program Status Register */
static inline u32_t read_cpsr()
{
    u32_t status;

    asm volatile("mrs %[status], cpsr @ read CPSR"
		 : [status] "=r" (status));
    return status;
}

/* Write Current Program Status Register */
static inline void write_cpsr(u32_t status)
{
    asm volatile("msr cpsr_c, %[status] @ write CPSR"
		 : : [status] "r" (status));
}

#endif /* _ARM_CPUFUNC_H */

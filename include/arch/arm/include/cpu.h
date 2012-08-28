#ifndef _ARM_CPU_H_
#define _ARM_CPU_H_


/* xPSR - Program Status Registers */
#define PSR_T (1 << 5)  /* Thumb execution state bit */
#define PSR_F (1 << 6)  /* FIQ mask bit */
#define PSR_I (1 << 7)  /* IRQ mask bit */
#define PSR_A (1 << 8)  /* Asynchronous abort mask bit */
#define PSR_E (1 << 9)  /* Endianness execution state bit */
#define PSR_J (1 << 24) /* Jazelle bit */
#define PSR_Q (1 << 27) /* Cumulative saturation bit */
#define PSR_V (1 << 28) /* Overflow condition flag */
#define PSR_C (1 << 29) /* Carry condition flag */
#define PSR_Z (1 << 30) /* Zero condition flag */
#define PSR_N (1 << 31) /* Negative condition flag */

#define PSR_MODE_MASK 0x0000001F /* Mode field mask */

#define MODE_USR 0x10 /* User mode */
#define MODE_FIQ 0x11 /* FIQ mode */
#define MODE_IRQ 0x12 /* IRQ mode */
#define MODE_SVC 0x13 /* Supervisor mode */
#define MODE_MON 0x16 /* Monitor mode */
#define MODE_ABT 0x17 /* Abort mode */
#define MODE_HYP 0x1A /* Hyp mode */
#define MODE_UND 0x1B /* Undefined mode */
#define MODE_SYS 0x1F /* System mode */

/* SCTLR - System Control Register */
#define SCTLR_M       (1 << 0)  /* MMU enable */
#define SCTLR_A       (1 << 1)  /* Alignment check enable */
#define SCTLR_C       (1 << 2)  /* Data and Unified Cache enable */
#define SCTLR_CP15BEN (1 << 5)  /* CP15 barrier enable */
#define SCTLR_SW      (1 << 10) /* SWP and SWPB enable */
#define SCTLR_Z       (1 << 11) /* Branch prediction enable */
#define SCTLR_I       (1 << 12) /* Instruction cache enable */
#define SCTLR_V       (1 << 13) /* (High) Vectors bit */
#define SCTLR_RR      (1 << 14) /* Round Robin (cache) select */
#define SCTLR_HA      (1 << 17) /* Hardware Access flag enable */
#define SCTLR_FI      (1 << 21) /* Fast interrupts configuration enable */
#define SCTLR_VE      (1 << 24) /* Interrupt Vectors Enable */
#define SCTLR_EE      (1 << 25) /* Exception Endianness */
#define SCTLR_NMFI    (1 << 27) /* Non-maskable FIQ (NMFI) support */
#define SCTLR_TRE     (1 << 28) /* TEX remap enable */
#define SCTLR_AFE     (1 << 29) /* Access flag enable */
#define SCTLR_TE      (1 << 30) /* Thumb Exception enable */

/* ACTLR - Auxiliary Control Register */
#define A8_ACTLR_L1ALIAS    (1 << 0)  /* L1 Dcache hw alias check enable */
#define A8_ACTLR_L2EN       (1 << 1)  /* L2 cache enable */
#define A8_ACTLR_L1RSTDIS   (1 << 30) /* L1 hw reset disable */
#define A8_ACTLR_L2RSTDIS   (1 << 31) /* L2 hw reset disable */

#endif /* _ARM_CPU_H_ */

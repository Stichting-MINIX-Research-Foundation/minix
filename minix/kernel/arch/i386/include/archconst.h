
#ifndef _I386_ACONST_H
#define _I386_ACONST_H 1

#include <machine/interrupt.h>
#include <machine/memory.h>

/* Constants for protected mode. */

/* Table sizes. */
#define IDT_SIZE 256	/* the table is set to it's maximal size */

/* GDT layout (SYSENTER/SYSEXIT compliant) */
#define KERN_CS_INDEX        1
#define KERN_DS_INDEX        2
#define USER_CS_INDEX        3
#define USER_DS_INDEX        4
#define LDT_INDEX            5
#define TSS_INDEX_FIRST      6
#define TSS_INDEX(cpu)       (TSS_INDEX_FIRST + (cpu)) /* per cpu kernel tss */
#define GDT_SIZE             (TSS_INDEX(CONFIG_MAX_CPUS))	/* LDT descriptor */

#define SEG_SELECTOR(i)          ((i)*8)
#define KERN_CS_SELECTOR SEG_SELECTOR(KERN_CS_INDEX)
#define KERN_DS_SELECTOR SEG_SELECTOR(KERN_DS_INDEX)
#define USER_CS_SELECTOR (SEG_SELECTOR(USER_CS_INDEX) | USER_PRIVILEGE)
#define USER_DS_SELECTOR (SEG_SELECTOR(USER_DS_INDEX) | USER_PRIVILEGE)
#define LDT_SELECTOR SEG_SELECTOR(LDT_INDEX)
#define TSS_SELECTOR(cpu)	SEG_SELECTOR(TSS_INDEX(cpu))

#define DESC_SIZE	8

/* Privileges. */
#define INTR_PRIVILEGE       0	/* kernel and interrupt handlers */
#define USER_PRIVILEGE       3	/* servers and user processes */
#define RPL_MASK             0x03	/* bits in selector RPL */

/* 286 hardware constants. */

/* Exception vector numbers. */
#define BOUNDS_VECTOR        5	/* bounds check failed */
#define INVAL_OP_VECTOR      6	/* invalid opcode */
#define COPROC_NOT_VECTOR    7	/* coprocessor not available */
#define DOUBLE_FAULT_VECTOR  8
#define COPROC_SEG_VECTOR    9	/* coprocessor segment overrun */
#define INVAL_TSS_VECTOR    10	/* invalid TSS */
#define SEG_NOT_VECTOR      11	/* segment not present */
#define STACK_FAULT_VECTOR  12	/* stack exception */
#define PROTECTION_VECTOR   13	/* general protection */

/* Selector bits. */
#define TI                0x04	/* table indicator */
#define RPL               0x03	/* requester privilege level */

/* Base and limit sizes and shifts. */
#define BASE_MIDDLE_SHIFT   16	/* shift for base --> base_middle */

/* Access-byte and type-byte bits. */
#define PRESENT           0x80	/* set for descriptor present */
#define DPL               0x60	/* descriptor privilege level mask */
#define DPL_SHIFT            5
#define SEGMENT           0x10	/* set for segment-type descriptors */

/* Access-byte bits. */
#define EXECUTABLE        0x08	/* set for executable segment */
#define CONFORMING        0x04	/* set for conforming segment if executable */
#define EXPAND_DOWN       0x04	/* set for expand-down segment if !executable*/
#define READABLE          0x02	/* set for readable segment if executable */
#define WRITEABLE         0x02	/* set for writeable segment if !executable */
#define TSS_BUSY          0x02	/* set if TSS descriptor is busy */
#define ACCESSED          0x01	/* set if segment accessed */

/* Special descriptor types. */
#define AVL_286_TSS          1	/* available 286 TSS */
#define LDT                  2	/* local descriptor table */
#define BUSY_286_TSS         3	/* set transparently to the software */
#define CALL_286_GATE        4	/* not used */
#define TASK_GATE            5	/* only used by debugger */
#define INT_286_GATE         6	/* interrupt gate, used for all vectors */
#define TRAP_286_GATE        7	/* not used */

#define INT_GATE_TYPE	(INT_286_GATE | DESC_386_BIT)
#define TSS_TYPE	(AVL_286_TSS  | DESC_386_BIT)


/* Extra 386 hardware constants. */

/* Exception vector numbers. */
#define PAGE_FAULT_VECTOR   14
#define COPROC_ERR_VECTOR   16	/* coprocessor error */
#define ALIGNMENT_CHECK_VECTOR	17
#define MACHINE_CHECK_VECTOR	18
#define SIMD_EXCEPTION_VECTOR	19     /* SIMD Floating-Point Exception (#XM) */

/* Descriptor structure offsets. */
#define DESC_GRANULARITY     6	/* to granularity byte */
#define DESC_BASE_HIGH       7	/* to base_high */

/* Type-byte bits. */
#define DESC_386_BIT  0x08 /* 386 types are obtained by ORing with this */
				/* LDT's and TASK_GATE's don't need it */

/* Base and limit sizes and shifts. */ 
#define BASE_HIGH_SHIFT     24  /* shift for base --> base_high */
#define BYTE_GRAN_MAX   0xFFFFFL   /* maximum size for byte granular segment */
#define GRANULARITY_SHIFT   16  /* shift for limit --> granularity */
#define OFFSET_HIGH_SHIFT   16  /* shift for (gate) offset --> offset_high */
#define PAGE_GRAN_SHIFT     12  /* extra shift for page granular limits */

/* Granularity byte. */
#define GRANULAR  	  0x80	/* set for 4K granularilty */
#define DEFAULT   	  0x40	/* set for 32-bit defaults (executable seg) */
#define BIG       	  0x40	/* set for "BIG" (expand-down seg) */
#define AVL        	  0x10	/* 0 for available */
#define LIMIT_HIGH   	  0x0F	/* mask for high bits of limit */

/* Program stack words and masks. */
#define INIT_PSW      0x0200    /* initial psw */
#define INIT_TASK_PSW 0x1200    /* initial psw for tasks (with IOPL 1) */
#define TRACEBIT      0x0100    /* OR this with psw in proc[] for tracing */
#define SETPSW(rp, new)         /* permits only certain bits to be set */ \
        ((rp)->p_reg.psw = ((rp)->p_reg.psw & ~0xCD5) | ((new) & 0xCD5))
#define IF_MASK 0x00000200
#define IOPL_MASK 0x003000

#define INTEL_CPUID_GEN_EBX	0x756e6547 /* ASCII value of "Genu" */
#define INTEL_CPUID_GEN_EDX	0x49656e69 /* ASCII value of "ineI" */
#define INTEL_CPUID_GEN_ECX	0x6c65746e /* ASCII value of "ntel" */

#define AMD_CPUID_GEN_EBX	0x68747541 /* ASCII value of "Auth" */
#define AMD_CPUID_GEN_EDX	0x69746e65 /* ASCII value of "enti" */
#define AMD_CPUID_GEN_ECX	0x444d4163 /* ASCII value of "cAMD" */

#define CPU_VENDOR_INTEL	0
#define CPU_VENDOR_AMD		2
#define CPU_VENDOR_UNKNOWN	0xff

/* fpu context should be saved in 16-byte aligned memory */
#define FPUALIGN		16

/* Poweroff 16-bit code address */
#define BIOS_POWEROFF_ENTRY 0x1000


/* 
 * defines how many bytes are reserved at the top of the kernel stack for global
 * information like currently scheduled process or current cpu id
 */
#define X86_STACK_TOP_RESERVED	(2 * sizeof(reg_t))

#define PG_ALLOCATEME ((phys_bytes)-1)

/* MSRs */
#define INTEL_MSR_PERFMON_CRT0         0xc1
#define INTEL_MSR_SYSENTER_CS         0x174
#define INTEL_MSR_SYSENTER_ESP        0x175
#define INTEL_MSR_SYSENTER_EIP        0x176
#define INTEL_MSR_PERFMON_SEL0        0x186

#define INTEL_MSR_PERFMON_SEL0_ENABLE (1 << 22)

#define AMD_EFER_SCE		(1L << 0) /* SYSCALL/SYSRET enabled */
#define AMD_MSR_EFER		0xC0000080	/* extended features msr */
#define AMD_MSR_STAR		0xC0000081	/* SYSCALL params msr */

/* trap styles recorded on kernel entry and exit */
#define KTS_NONE	1 /* invalid */
#define KTS_INT_HARD	2 /* exception / hard interrupt */
#define KTS_INT_ORIG	3 /* soft interrupt from libc */
#define KTS_INT_UM	4 /* soft interrupt from usermapped code */
#define KTS_FULLCONTEXT	5 /* must restore full context */
#define KTS_SYSENTER	6 /* SYSENTER instruction (usermapped) */
#define KTS_SYSCALL	7 /* SYSCALL instruction (usermapped) */

#endif /* _I386_ACONST_H */

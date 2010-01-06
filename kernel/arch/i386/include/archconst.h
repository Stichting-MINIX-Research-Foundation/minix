
#ifndef _I386_ACONST_H
#define _I386_ACONST_H 1

#include <ibm/interrupt.h>
#include <ibm/memory.h>

#define NR_REMOTE_SEGS     3    /* # remote memory regions (variable) */

/* Constants for protected mode. */

/* Table sizes. */
#define GDT_SIZE (FIRST_LDT_INDEX + NR_TASKS + NR_PROCS) 
					/* spec. and LDT's */
#define IDT_SIZE 256	/* the table is set to it's maximal size */

/* Fixed global descriptors.  1 to 7 are prescribed by the BIOS. */
#define GDT_INDEX            1	/* GDT descriptor */
#define IDT_INDEX            2	/* IDT descriptor */
#define DS_INDEX             3	/* kernel DS */
#define ES_INDEX             4	/* kernel ES (386: flag 4 Gb at startup) */
#define SS_INDEX             5	/* kernel SS (386: monitor SS at startup) */
#define CS_INDEX             6	/* kernel CS */
#define MON_CS_INDEX         7	/* temp for BIOS (386: monitor CS at startup) */
#define DS_286_INDEX         8	/* scratch 16-bit source segment */
#define ES_286_INDEX         9	/* scratch 16-bit destination segment */
#define TSS_INDEX           10	/* kernel TSS */
#define FIRST_LDT_INDEX     11	/* rest of descriptors are LDT's */

/* Descriptor structure offsets. */
#define DESC_BASE            2	/* to base_low */
#define DESC_BASE_MIDDLE     4	/* to base_middle */
#define DESC_ACCESS          5	/* to access byte */
#define DESC_SIZE            8	/* sizeof (struct segdesc_s) */

/*
 * WARNING no () around the macros, be careful. This is because of ACK assembler
 * and will be fixed after switching to GAS
 */
#define GDT_SELECTOR		GDT_INDEX * DESC_SIZE
#define IDT_SELECTOR		IDT_INDEX * DESC_SIZE
#define DS_SELECTOR		DS_INDEX * DESC_SIZE
#define ES_SELECTOR		ES_INDEX * DESC_SIZE
/* flat DS is less privileged ES */
#define FLAT_DS_SELECTOR	ES_SELECTOR | TASK_PRIVILEGE
#define SS_SELECTOR		SS_INDEX * DESC_SIZE
#define CS_SELECTOR		CS_INDEX * DESC_SIZE
#define MON_CS_SELECTOR		MON_CS_INDEX * DESC_SIZE
#define TSS_SELECTOR		TSS_INDEX * DESC_SIZE
#define DS_286_SELECTOR		DS_286_INDEX*DESC_SIZE | TASK_PRIVILEGE
#define ES_286_SELECTOR		ES_286_INDEX*DESC_SIZE | TASK_PRIVILEGE

/* Privileges. */
#define INTR_PRIVILEGE       0	/* kernel and interrupt handlers */
#define TASK_PRIVILEGE       1	/* kernel tasks */
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
        ((rp)->p_reg.psw = (rp)->p_reg.psw & ~0xCD5 | (new) & 0xCD5)
#define IF_MASK 0x00000200
#define IOPL_MASK 0x003000

#define vir2phys(vir)   ((phys_bytes)((kinfo.data_base + (vir_bytes) (vir))))
#define phys2vir(ph)   ((vir_bytes)((vir_bytes) (ph) - kinfo.data_base))

#define INTEL_CPUID_GEN_EBX	0x756e6547 /* ASCII value of "Genu" */
#define INTEL_CPUID_GEN_EDX	0x49656e69 /* ASCII value of "ineI" */
#define INTEL_CPUID_GEN_ECX	0x6c65746e /* ASCII value of "ntel" */

#define AMD_CPUID_GEN_EBX	0x68747541 /* ASCII value of "Auth" */
#define AMD_CPUID_GEN_EDX	0x69746e65 /* ASCII value of "enti" */
#define AMD_CPUID_GEN_ECX	0x444d4163 /* ASCII value of "cAMD" */

/* fpu context should be saved in 16-byte aligned memory */
#define FPUALIGN		16

#endif /* _I386_ACONST_H */

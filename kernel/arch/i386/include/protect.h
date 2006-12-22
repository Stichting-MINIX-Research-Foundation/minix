/* Constants for protected mode. */

/* Table sizes. */
#define GDT_SIZE (FIRST_LDT_INDEX + NR_TASKS + NR_PROCS) 
					/* spec. and LDT's */
#define IDT_SIZE (IRQ8_VECTOR + 8)	/* only up to the highest vector */
#define LDT_SIZE (2 + NR_REMOTE_SEGS) 	/* CS, DS and remote segments */

/* Fixed global descriptors.  1 to 7 are prescribed by the BIOS. */
#define GDT_INDEX            1	/* GDT descriptor */
#define IDT_INDEX            2	/* IDT descriptor */
#define DS_INDEX             3	/* kernel DS */
#define ES_INDEX             4	/* kernel ES (386: flag 4 Gb at startup) */
#define SS_INDEX             5	/* kernel SS (386: monitor SS at startup) */
#define CS_INDEX             6	/* kernel CS */
#define MON_CS_INDEX         7	/* temp for BIOS (386: monitor CS at startup) */
#define TSS_INDEX            8	/* kernel TSS */
#define DS_286_INDEX         9	/* scratch 16-bit source segment */
#define ES_286_INDEX        10	/* scratch 16-bit destination segment */
#define A_INDEX             11	/* 64K memory segment at A0000 */
#define B_INDEX             12	/* 64K memory segment at B0000 */
#define C_INDEX             13	/* 64K memory segment at C0000 */
#define D_INDEX             14	/* 64K memory segment at D0000 */
#define FIRST_LDT_INDEX     15	/* rest of descriptors are LDT's */

#define GDT_SELECTOR      0x08	/* (GDT_INDEX * DESC_SIZE) bad for asld */
#define IDT_SELECTOR      0x10	/* (IDT_INDEX * DESC_SIZE) */
#define DS_SELECTOR       0x18	/* (DS_INDEX * DESC_SIZE) */
#define ES_SELECTOR       0x20	/* (ES_INDEX * DESC_SIZE) */
#define FLAT_DS_SELECTOR  0x21	/* less privileged ES */
#define SS_SELECTOR       0x28	/* (SS_INDEX * DESC_SIZE) */
#define CS_SELECTOR       0x30	/* (CS_INDEX * DESC_SIZE) */
#define MON_CS_SELECTOR   0x38	/* (MON_CS_INDEX * DESC_SIZE) */
#define TSS_SELECTOR      0x40	/* (TSS_INDEX * DESC_SIZE) */
#define DS_286_SELECTOR   0x49	/* (DS_286_INDEX*DESC_SIZE+TASK_PRIVILEGE) */
#define ES_286_SELECTOR   0x51	/* (ES_286_INDEX*DESC_SIZE+TASK_PRIVILEGE) */

/* Fixed local descriptors. */
#define CS_LDT_INDEX         0	/* process CS */
#define DS_LDT_INDEX         1	/* process DS=ES=FS=GS=SS */
#define EXTRA_LDT_INDEX      2	/* first of the extra LDT entries */

/* Privileges. */
#define INTR_PRIVILEGE       0	/* kernel and interrupt handlers */
#define TASK_PRIVILEGE       1	/* kernel tasks */
#define USER_PRIVILEGE       3	/* servers and user processes */

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

/* Descriptor structure offsets. */
#define DESC_BASE            2	/* to base_low */
#define DESC_BASE_MIDDLE     4	/* to base_middle */
#define DESC_ACCESS          5	/* to access byte */
#define DESC_SIZE            8	/* sizeof (struct segdesc_s) */

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

/* Extra 386 hardware constants. */

/* Exception vector numbers. */
#define PAGE_FAULT_VECTOR   14
#define COPROC_ERR_VECTOR   16	/* coprocessor error */

/* Descriptor structure offsets. */
#define DESC_GRANULARITY     6	/* to granularity byte */
#define DESC_BASE_HIGH       7	/* to base_high */

/* Base and limit sizes and shifts. */
#define BASE_HIGH_SHIFT     24	/* shift for base --> base_high */
#define BYTE_GRAN_MAX   0xFFFFFL   /* maximum size for byte granular segment */
#define GRANULARITY_SHIFT   16	/* shift for limit --> granularity */
#define OFFSET_HIGH_SHIFT   16	/* shift for (gate) offset --> offset_high */
#define PAGE_GRAN_SHIFT     12	/* extra shift for page granular limits */

/* Type-byte bits. */
#define DESC_386_BIT  0x08 /* 386 types are obtained by ORing with this */
				/* LDT's and TASK_GATE's don't need it */

/* Granularity byte. */
#define GRANULAR  	  0x80	/* set for 4K granularilty */
#define DEFAULT   	  0x40	/* set for 32-bit defaults (executable seg) */
#define BIG       	  0x40	/* set for "BIG" (expand-down seg) */
#define AVL        	  0x10	/* 0 for available */
#define LIMIT_HIGH   	  0x0F	/* mask for high bits of limit */

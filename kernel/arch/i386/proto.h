
#ifndef _I386_PROTO_H
#define _I386_PROTO_H

/* Hardware interrupt handlers. */
_PROTOTYPE( void hwint00, (void) );
_PROTOTYPE( void hwint01, (void) );
_PROTOTYPE( void hwint02, (void) );
_PROTOTYPE( void hwint03, (void) );
_PROTOTYPE( void hwint04, (void) );
_PROTOTYPE( void hwint05, (void) );
_PROTOTYPE( void hwint06, (void) );
_PROTOTYPE( void hwint07, (void) );
_PROTOTYPE( void hwint08, (void) );
_PROTOTYPE( void hwint09, (void) );
_PROTOTYPE( void hwint10, (void) );
_PROTOTYPE( void hwint11, (void) );
_PROTOTYPE( void hwint12, (void) );
_PROTOTYPE( void hwint13, (void) );
_PROTOTYPE( void hwint14, (void) );
_PROTOTYPE( void hwint15, (void) );

/* Exception handlers (real or protected mode), in numerical order. */
void _PROTOTYPE( int00, (void) ), _PROTOTYPE( divide_error, (void) );
void _PROTOTYPE( int01, (void) ), _PROTOTYPE( single_step_exception, (void) );
void _PROTOTYPE( int02, (void) ), _PROTOTYPE( nmi, (void) );
void _PROTOTYPE( int03, (void) ), _PROTOTYPE( breakpoint_exception, (void) );
void _PROTOTYPE( int04, (void) ), _PROTOTYPE( overflow, (void) );
void _PROTOTYPE( int05, (void) ), _PROTOTYPE( bounds_check, (void) );
void _PROTOTYPE( int06, (void) ), _PROTOTYPE( inval_opcode, (void) );
void _PROTOTYPE( int07, (void) ), _PROTOTYPE( copr_not_available, (void) );
void                              _PROTOTYPE( double_fault, (void) );
void                              _PROTOTYPE( copr_seg_overrun, (void) );
void                              _PROTOTYPE( inval_tss, (void) );
void                              _PROTOTYPE( segment_not_present, (void) );
void                              _PROTOTYPE( stack_exception, (void) );
void                              _PROTOTYPE( general_protection, (void) );
void                              _PROTOTYPE( page_fault, (void) );
void                              _PROTOTYPE( copr_error, (void) );
void                              _PROTOTYPE( alignment_check, (void) );
void                              _PROTOTYPE( machine_check, (void) );
void                              _PROTOTYPE( simd_exception, (void) );

/* Software interrupt handlers, in numerical order. */
_PROTOTYPE( void trp, (void) );
_PROTOTYPE( void ipc_entry, (void) );
_PROTOTYPE( void kernel_call_entry, (void) );
_PROTOTYPE( void level0_call, (void) );

/* memory.c */
_PROTOTYPE( void i386_freepde, (int pde));
_PROTOTYPE( void getcr3val, (void));


/* exception.c */
struct exception_frame {
	reg_t	vector;		/* which interrupt vector was triggered */
	reg_t	errcode;	/* zero if no exception does not push err code */
	reg_t	eip;
	reg_t	cs;
	reg_t	eflags;
	reg_t	esp;		/* undefined if trap is nested */
	reg_t	ss;		/* undefined if trap is nested */
};

_PROTOTYPE( void exception, (struct exception_frame * frame));

/* klib386.s */
_PROTOTYPE( __dead void monitor, (void)                                 );
_PROTOTYPE( __dead void reset, (void)                                   );
_PROTOTYPE( void int86, (void)                     			);
_PROTOTYPE( reg_t read_cr0, (void)					);
_PROTOTYPE( reg_t read_cr2, (void)					);
_PROTOTYPE( void write_cr0, (unsigned long value)                       );
_PROTOTYPE( unsigned long read_cr4, (void)                              );
_PROTOTYPE( void write_cr4, (unsigned long value)                       );
_PROTOTYPE( void write_cr3, (unsigned long value)                       );
_PROTOTYPE( unsigned long read_cpu_flags, (void)                        );
_PROTOTYPE( void phys_insb, (u16_t port, phys_bytes buf, size_t count)  );
_PROTOTYPE( void phys_insw, (u16_t port, phys_bytes buf, size_t count)  );
_PROTOTYPE( void phys_outsb, (u16_t port, phys_bytes buf, size_t count) );
_PROTOTYPE( void phys_outsw, (u16_t port, phys_bytes buf, size_t count) );
_PROTOTYPE( u32_t read_cr3, (void) );
_PROTOTYPE( void reload_cr3, (void) );
_PROTOTYPE( void phys_memset, (phys_bytes ph, u32_t c, phys_bytes bytes));
_PROTOTYPE( void reload_ds, (void)					);
_PROTOTYPE( void ia32_msr_read, (u32_t reg, u32_t * hi, u32_t * lo)	);
_PROTOTYPE( void ia32_msr_write, (u32_t reg, u32_t hi, u32_t lo)	);
_PROTOTYPE( void fninit, (void));
_PROTOTYPE( void clts, (void));
_PROTOTYPE( void fxsave, (void *));
_PROTOTYPE( void fnsave, (void *));
_PROTOTYPE( void fxrstor, (void *));
_PROTOTYPE( void frstor, (void *));
_PROTOTYPE( unsigned short fnstsw, (void));
_PROTOTYPE( void fnstcw, (unsigned short* cw));

/* protect.c */
struct tss_s {
  reg_t backlink;
  reg_t sp0;                    /* stack pointer to use during interrupt */
  reg_t ss0;                    /*   "   segment  "  "    "        "     */
  reg_t sp1;
  reg_t ss1;
  reg_t sp2;
  reg_t ss2;
  reg_t cr3;
  reg_t ip;
  reg_t flags;
  reg_t ax;
  reg_t cx;
  reg_t dx;
  reg_t bx;
  reg_t sp;
  reg_t bp;
  reg_t si;
  reg_t di;
  reg_t es;
  reg_t cs;
  reg_t ss;
  reg_t ds;
  reg_t fs;
  reg_t gs;
  reg_t ldt;
  u16_t trap;
  u16_t iobase;
/* u8_t iomap[0]; */
};

EXTERN struct tss_s tss;

_PROTOTYPE( void idt_init, (void)                     			);
_PROTOTYPE( void init_dataseg, (struct segdesc_s *segdp, phys_bytes base,
                vir_bytes size, int privilege)                          );
_PROTOTYPE( void enable_iop, (struct proc *pp)                          );
_PROTOTYPE( int prot_set_kern_seg_limit, (vir_bytes limit)             );
_PROTOTYPE( void printseg, (char *banner, int iscs, struct proc *pr, u32_t selector)             );
_PROTOTYPE( u32_t read_cs, (void));
_PROTOTYPE( u32_t read_ds, (void));
_PROTOTYPE( u32_t read_ss, (void));

/* prototype of an interrupt vector table entry */
struct gate_table_s {
	_PROTOTYPE( void (*gate), (void) );
	unsigned char vec_nr;
	unsigned char privilege;
};

EXTERN struct gate_table_s gate_table_pic[];

/* copies an array of vectors to the IDT. The last vector must be zero filled */
_PROTOTYPE(void idt_copy_vectors, (struct gate_table_s * first));
_PROTOTYPE(void idt_reload,(void));

EXTERN void * k_boot_stktop;

_PROTOTYPE( void int_gate, (unsigned vec_nr, vir_bytes offset,
		unsigned dpl_type) );

_PROTOTYPE(void __copy_msg_from_user_end, (void));
_PROTOTYPE(void __copy_msg_to_user_end, (void));
_PROTOTYPE(void __user_copy_msg_pointer_failure, (void));

_PROTOTYPE(int platform_tbl_checksum_ok, (void *ptr, unsigned int length));
_PROTOTYPE(int platform_tbl_ptr, (phys_bytes start,
					phys_bytes end,
					unsigned increment,
					void * buff,
					unsigned size,
					phys_bytes * phys_addr,
					int ((* cmp_f)(void *))));

/* breakpoints.c */
#define BREAKPOINT_COUNT		4
#define BREAKPOINT_FLAG_RW_MASK		(3 << 0)
#define BREAKPOINT_FLAG_RW_EXEC		(0 << 0)
#define BREAKPOINT_FLAG_RW_WRITE	(1 << 0)
#define BREAKPOINT_FLAG_RW_RW		(2 << 0)
#define BREAKPOINT_FLAG_LEN_MASK	(3 << 2)
#define BREAKPOINT_FLAG_LEN_1		(0 << 2)
#define BREAKPOINT_FLAG_LEN_2		(1 << 2)
#define BREAKPOINT_FLAG_LEN_4		(2 << 2)
#define BREAKPOINT_FLAG_MODE_MASK	(3 << 4)
#define BREAKPOINT_FLAG_MODE_OFF	(0 << 4)
#define BREAKPOINT_FLAG_MODE_LOCAL	(1 << 4)
#define BREAKPOINT_FLAG_MODE_GLOBAL	(2 << 4)

/* functions defined in architecture-independent kernel source. */
#include "kernel/proto.h"

#endif

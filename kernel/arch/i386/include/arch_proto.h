
#ifndef _I386_PROTO_H
#define _I386_PROTO_H

#include <machine/vm.h>

#define K_STACK_SIZE	I386_PAGE_SIZE

#ifndef __ASSEMBLY__

/* Hardware interrupt handlers. */
void hwint00(void);
void hwint01(void);
void hwint02(void);
void hwint03(void);
void hwint04(void);
void hwint05(void);
void hwint06(void);
void hwint07(void);
void hwint08(void);
void hwint09(void);
void hwint10(void);
void hwint11(void);
void hwint12(void);
void hwint13(void);
void hwint14(void);
void hwint15(void);

/* Exception handlers (real or protected mode), in numerical order. */
void int00(void), divide_error (void);
void int01(void), single_step_exception (void);
void int02(void), nmi (void);
void int03(void), breakpoint_exception (void);
void int04(void), overflow (void);
void int05(void), bounds_check (void);
void int06(void), inval_opcode (void);
void int07(void), copr_not_available (void);
void double_fault(void);
void copr_seg_overrun(void);
void inval_tss(void);
void segment_not_present(void);
void stack_exception(void);
void general_protection(void);
void page_fault(void);
void copr_error(void);
void alignment_check(void);
void machine_check(void);
void simd_exception(void);

/* Software interrupt handlers, in numerical order. */
void trp(void);
void ipc_entry(void);
void kernel_call_entry(void);
void level0_call(void);

/* memory.c */
void segmentation2paging(struct proc * current);
void i386_freepde(int pde);
void getcr3val(void);


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

void exception(struct exception_frame * frame);

/* klib386.s */
__dead void monitor(void);
__dead void reset(void);
__dead void x86_triplefault(void);
reg_t read_cr0(void);
reg_t read_cr2(void);
void write_cr0(unsigned long value);
unsigned long read_cr4(void);
void write_cr4(unsigned long value);
void write_cr3(unsigned long value);
unsigned long read_cpu_flags(void);
void phys_insb(u16_t port, phys_bytes buf, size_t count);
void phys_insw(u16_t port, phys_bytes buf, size_t count);
void phys_outsb(u16_t port, phys_bytes buf, size_t count);
void phys_outsw(u16_t port, phys_bytes buf, size_t count);
u32_t read_cr3(void);
void reload_cr3(void);
void i386_invlpg(phys_bytes linaddr);
vir_bytes phys_memset(phys_bytes ph, u32_t c, phys_bytes bytes);
void reload_ds(void);
void ia32_msr_read(u32_t reg, u32_t * hi, u32_t * lo);
void ia32_msr_write(u32_t reg, u32_t hi, u32_t lo);
void fninit(void);
void clts(void);
void fxsave(void *);
void fnsave(void *);
int fxrstor(void *);
int __fxrstor_end(void *);
int frstor(void *);
int __frstor_end(void *);
int __frstor_failure(void *);
unsigned short fnstsw(void);
void fnstcw(unsigned short* cw);

void switch_k_stack(void * esp, void (* continuation)(void));

void __switch_address_space(struct proc * p, struct proc ** __ptproc);
#define switch_address_space(proc)	\
	__switch_address_space(proc, get_cpulocal_var_ptr(ptproc))

void refresh_tlb(void);

/* multiboot.c */
void multiboot_init(void);

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

void prot_init(void);
void idt_init(void);
void init_dataseg(struct segdesc_s *segdp, phys_bytes base, vir_bytes
	size, int privilege);
void enable_iop(struct proc *pp);
int prot_set_kern_seg_limit(vir_bytes limit);
void printseg(char *banner, int iscs, struct proc *pr, u32_t selector);
u32_t read_cs(void);
u32_t read_ds(void);
u32_t read_ss(void);

/* prototype of an interrupt vector table entry */
struct gate_table_s {
 void(*gate) (void);
	unsigned char vec_nr;
	unsigned char privilege;
};

extern struct gate_table_s gate_table_pic[];

/* copies an array of vectors to the IDT. The last vector must be zero filled */
void idt_copy_vectors(struct gate_table_s * first);
void idt_reload(void);

EXTERN void * k_boot_stktop;
EXTERN void * k_stacks_start;
extern void * k_stacks;

#define get_k_stack_top(cpu)	((void *)(((char*)(k_stacks)) \
					+ 2 * ((cpu) + 1) * K_STACK_SIZE))

void mfence(void);
#define barrier()	do { mfence(); } while(0)


#ifndef __GNUC__
/* call a function to read the stack fram pointer (%ebp) */
reg_t read_ebp(void);
#define get_stack_frame(__X)	((reg_t)read_ebp())
#else
/* read %ebp directly */
#define get_stack_frame(__X)	((reg_t)__builtin_frame_address(0))
#endif

/*
 * sets up TSS for a cpu and assigns kernel stack and cpu id
 */
void tss_init(unsigned cpu, void * kernel_stack);

void int_gate(unsigned vec_nr, vir_bytes offset, unsigned dpl_type);

void __copy_msg_from_user_end(void);
void __copy_msg_to_user_end(void);
void __user_copy_msg_pointer_failure(void);

int platform_tbl_checksum_ok(void *ptr, unsigned int length);
int platform_tbl_ptr(phys_bytes start, phys_bytes end, unsigned
	increment, void * buff, unsigned size, phys_bytes * phys_addr, int ((*
	cmp_f)(void *)));

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

#endif /* __ASSEMBLY__ */

#endif

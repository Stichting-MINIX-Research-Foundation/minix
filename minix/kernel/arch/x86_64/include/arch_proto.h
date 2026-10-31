
#ifndef _I386_PROTO_H
#define _I386_PROTO_H

#include <machine/vm.h>

/*
 * Per-CPU kernel stack size.  This is also the granularity at which the SMP
 * `cpuid` macro (arch_smp.h) rounds %rbp to find the per-CPU id stored at the
 * top of the stack: it is only valid while the stack pointer stays within the
 * top K_STACK_SIZE of get_k_stack_top(cpu).  On x86_64 the kernel call frames
 * are ~2x i386's (8-byte regs, more spilling, the 40-byte CPU trap frame), so
 * a single 4 KB page is too small: a deep chain (e.g. RECEIVE(ANY) with async
 * delivery) grows past 4 KB, round_up(%rbp, K_STACK_SIZE) then lands on the
 * wrong boundary and `cpuid` reads garbage, corrupting per-CPU state and
 * triple-faulting.  Use 16 KB (matching NetBSD/amd64 UPAGES) so the cpuid-safe
 * window comfortably exceeds real kernel stack usage.  i386 keeps one page.
 */
#define K_STACK_SIZE	(4 * I386_PAGE_SIZE)

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

void restore_user_context_int(struct proc *);
void restore_user_context_sysenter(struct proc *);
void restore_user_context_syscall(struct proc *);

/* Software interrupt handlers, in numerical order. */
void trp(void);
void ipc_entry_softint_orig(void);
void ipc_entry_softint_um(void);
void ipc_entry_sysenter(void);
void ipc_entry_syscall_cpu0(void);
void ipc_entry_syscall_cpu1(void);
void ipc_entry_syscall_cpu2(void);
void ipc_entry_syscall_cpu3(void);
void ipc_entry_syscall_cpu4(void);
void ipc_entry_syscall_cpu5(void);
void ipc_entry_syscall_cpu6(void);
void ipc_entry_syscall_cpu7(void);
void kernel_call_entry_orig(void);
void kernel_call_entry_um(void);
void level0_call(void);

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

/* klib.S */
__dead void monitor(void);
__dead void reset(void);
__dead void poweroff_vmware_clihlt(void);
__dead void x86_triplefault(void);
reg_t read_cr0(void);
reg_t read_cr2(void);
void write_cr0(unsigned long value);
unsigned long read_cr4(void);
void write_cr4(unsigned long value);
void write_cr3(unsigned long value);
unsigned long read_cpu_flags(void);
phys_bytes vir2phys(void *);
void phys_insb(u16_t port, phys_bytes buf, size_t count);
void phys_insw(u16_t port, phys_bytes buf, size_t count);
void phys_outsb(u16_t port, phys_bytes buf, size_t count);
void phys_outsw(u16_t port, phys_bytes buf, size_t count);
u64_t read_cr3(void);
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
void x86_lgdt(void *);
void x86_lldt(u32_t);
void x86_ltr(u32_t);
void x86_lidt(void *);
void x86_load_kerncs(void);
void x86_load_ds(u32_t);
void x86_load_ss(u32_t);
void x86_load_es(u32_t);
void x86_load_fs(u32_t);
void x86_load_gs(u32_t);

/* ipc functions in usermapped_ipc.S */
int usermapped_send_softint(endpoint_t dest, message *m_ptr);
int usermapped_receive_softint(endpoint_t src, message *m_ptr, int *status_ptr);
int usermapped_sendrec_softint(endpoint_t src_dest, message *m_ptr);
int usermapped_sendnb_softint(endpoint_t dest, message *m_ptr);
int usermapped_notify_softint(endpoint_t dest);
int usermapped_do_kernel_call_softint(message *m_ptr);
int usermapped_senda_softint(asynmsg_t *table, size_t count);

int usermapped_send_syscall(endpoint_t dest, message *m_ptr);
int usermapped_receive_syscall(endpoint_t src, message *m_ptr, int *status_ptr);
int usermapped_sendrec_syscall(endpoint_t src_dest, message *m_ptr);
int usermapped_sendnb_syscall(endpoint_t dest, message *m_ptr);
int usermapped_notify_syscall(endpoint_t dest);
int usermapped_do_kernel_call_syscall(message *m_ptr);
int usermapped_senda_syscall(asynmsg_t *table, size_t count);

int usermapped_send_sysenter(endpoint_t dest, message *m_ptr);
int usermapped_receive_sysenter(endpoint_t src, message *m_ptr, int *status_ptr);
int usermapped_sendrec_sysenter(endpoint_t src_dest, message *m_ptr);
int usermapped_sendnb_sysenter(endpoint_t dest, message *m_ptr);
int usermapped_notify_sysenter(endpoint_t dest);
int usermapped_do_kernel_call_sysenter(message *m_ptr);
int usermapped_senda_sysenter(asynmsg_t *table, size_t count);

void switch_k_stack(void * esp, void (* continuation)(void));

void __switch_address_space(struct proc * p, struct proc ** __ptproc);
#define switch_address_space(proc)	\
	__switch_address_space(proc, get_cpulocal_var_ptr(ptproc))

void refresh_tlb(void);

/* multiboot.c */
void multiboot_init(void);

/* protect.c — 64-bit TSS (Intel SDM Vol.3 7.7) */
struct tss_s {
  u32_t reserved0;
  u64_t rsp0;      /* kernel stack pointer for ring 0 */
  u64_t rsp1;
  u64_t rsp2;
  u64_t reserved1;
  u64_t ist[7];    /* IST1–IST7 (interrupt stack table) */
  u64_t reserved2;
  u16_t reserved3;
  u16_t iobase;    /* I/O permission bitmap offset (sizeof(tss_s) = no bitmap) */
/* u8_t iomap[0]; */
} __attribute__((packed));

void enable_iop(struct proc *pp);
u32_t read_cs(void);
u32_t read_ds(void);
u32_t read_ss(void);

void add_memmap(kinfo_t *cbi, u64_t addr, u64_t len);
phys_bytes alloc_lowest(kinfo_t *cbi, phys_bytes len);
void vm_enable_paging(void);
void cut_memmap(kinfo_t *cbi, phys_bytes start, phys_bytes end);
phys_bytes pg_roundup(phys_bytes b);
void pg_info(reg_t *, u64_t **);
void pg_clear(void);
void pg_identity(kinfo_t *);
phys_bytes pg_load(void);
void pg_map(phys_bytes phys, vir_bytes vaddr, vir_bytes vaddr_end, kinfo_t *cbi);
int pg_mapkernel(void);
void pg_mapproc(struct proc *p, struct boot_image *ip, kinfo_t *cbi);

/* prototype of an interrupt vector table entry */
struct gate_table_s {
 void(*gate) (void);
	unsigned char vec_nr;
	unsigned char privilege;
};

/* copies an array of vectors to the IDT. The last vector must be zero filled */
void idt_copy_vectors(struct gate_table_s * first);
void idt_copy_vectors_pic(void);
void idt_reload(void);

EXTERN void * k_stacks_start;
extern void * k_stacks;
extern u64_t k_percpu_stacks[CONFIG_MAX_CPUS]; /* kernel stack top per CPU */

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
int tss_init(unsigned cpu, void * kernel_stack);

/*
 * Set up MSRs for sysenter or syscall for the current cpu.
 * Assumes that tss_init has been called before for that cpu ! */
void setup_sysenter_syscall(void);

void int_gate_idt(unsigned vec_nr, vir_bytes offset, unsigned dpl_type);

void __copy_msg_from_user_end(void);
void __copy_msg_to_user_end(void);
void __user_copy_msg_pointer_failure(void);

int platform_tbl_checksum_ok(void *ptr, unsigned int length);
int platform_tbl_ptr(phys_bytes start, phys_bytes end, unsigned
	increment, void * buff, unsigned size, phys_bytes * phys_addr, int ((*
	cmp_f)(void *)));

/* breakpoints.c */
int breakpoint_set(phys_bytes linaddr, int bp, const int flags);
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

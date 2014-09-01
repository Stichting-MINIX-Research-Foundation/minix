/* system dependent functions for use inside the whole kernel. */

#include "kernel/kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <machine/cmos.h>
#include <machine/bios.h>
#include <machine/cpu.h>
#include <minix/portio.h>
#include <minix/cpufeature.h>
#include <assert.h>
#include <signal.h>
#include <machine/vm.h>

#include <minix/u64.h>

#include "archconst.h"
#include "arch_proto.h"
#include "serial.h"
#include "oxpcie.h"
#include <machine/multiboot.h>

#include "glo.h"

#ifdef USE_APIC
#include "apic.h"
#endif

#ifdef USE_ACPI
#include "acpi.h"
#endif

static int osfxsr_feature; /* FXSAVE/FXRSTOR instructions support (SSEx) */

/* set MP and NE flags to handle FPU exceptions in native mode. */
#define CR0_MP_NE	0x0022
/* set CR4.OSFXSR[bit 9] if FXSR is supported. */
#define CR4_OSFXSR	(1L<<9)
/* set OSXMMEXCPT[bit 10] if we provide #XM handler. */
#define CR4_OSXMMEXCPT	(1L<<10)

void * k_stacks;

static void ser_debug(int c);
static void ser_dump_vfs(void);

#ifdef CONFIG_SMP
static void ser_dump_proc_cpu(void);
#endif
#if !CONFIG_OXPCIE
static void ser_init(void);
#endif

void fpu_init(void)
{
	unsigned short cw, sw;

	fninit();
	sw = fnstsw();
	fnstcw(&cw);

	if((sw & 0xff) == 0 &&
	   (cw & 0x103f) == 0x3f) {
		/* We have some sort of FPU, but don't check exact model.
		 * Set CR0_NE and CR0_MP to handle fpu exceptions
		 * in native mode. */
		write_cr0(read_cr0() | CR0_MP_NE);
		get_cpulocal_var(fpu_presence) = 1;
		if(_cpufeature(_CPUF_I386_FXSR)) {
			u32_t cr4 = read_cr4() | CR4_OSFXSR; /* Enable FXSR. */

			/* OSXMMEXCPT if supported
			 * FXSR feature can be available without SSE
			 */
			if(_cpufeature(_CPUF_I386_SSE))
				cr4 |= CR4_OSXMMEXCPT; 

			write_cr4(cr4);
			osfxsr_feature = 1;
		} else {
			osfxsr_feature = 0;
		}
	} else {
		/* No FPU presents. */
		get_cpulocal_var(fpu_presence) = 0;
                osfxsr_feature = 0;
                return;
        }
}

void save_local_fpu(struct proc *pr, int retain)
{
	char *state = pr->p_seg.fpu_state;

	/* Save process FPU context. If the 'retain' flag is set, keep the FPU
	 * state as is. If the flag is not set, the state is undefined upon
	 * return, and the caller is responsible for reloading a proper state.
	 */

	if(!is_fpu())
		return;

	assert(state);

	if(osfxsr_feature) {
		fxsave(state);
	} else {
		fnsave(state);
		if (retain)
			(void) frstor(state);
	}
}

void save_fpu(struct proc *pr)
{
#ifdef CONFIG_SMP
	if (cpuid != pr->p_cpu) {
		int stopped;

		/* remember if the process was already stopped */
		stopped = RTS_ISSET(pr, RTS_PROC_STOP);

		/* stop the remote process and force its context to be saved */
		smp_schedule_stop_proc_save_ctx(pr);

		/*
		 * If the process wasn't stopped let the process run again. The
		 * process is kept block by the fact that the kernel cannot run
		 * on its cpu
		 */
		if (!stopped)
			RTS_UNSET(pr, RTS_PROC_STOP);

		return;
	}
#endif

	if (get_cpulocal_var(fpu_owner) == pr) {
		disable_fpu_exception();
		save_local_fpu(pr, TRUE /*retain*/);
	}
}

/* reserve a chunk of memory for fpu state; every one has to
 * be FPUALIGN-aligned.
 */
static char fpu_state[NR_PROCS][FPU_XFP_SIZE] __aligned(FPUALIGN);

void arch_proc_reset(struct proc *pr)
{
	char *v = NULL;
	struct stackframe_s reg;

	assert(pr->p_nr < NR_PROCS);

	if(pr->p_nr >= 0) {
		v = fpu_state[pr->p_nr];
		/* verify alignment */
		assert(!((vir_bytes)v % FPUALIGN));
		/* initialize state */
		memset(v, 0, FPU_XFP_SIZE);
	}

	/* Clear process state. */
        memset(&reg, 0, sizeof(pr->p_reg));
        if(iskerneln(pr->p_nr))
        	reg.psw = INIT_TASK_PSW;
        else
        	reg.psw = INIT_PSW;

	pr->p_seg.fpu_state = v;

	/* Initialize the fundamentals that are (initially) the same for all
	 * processes - the segment selectors it gets to use.
	 */
	pr->p_reg.cs = USER_CS_SELECTOR;
	pr->p_reg.gs = 
	pr->p_reg.fs = 
	pr->p_reg.ss = 
	pr->p_reg.es = 
	pr->p_reg.ds = USER_DS_SELECTOR;

	/* set full context and make sure it gets restored */
	arch_proc_setcontext(pr, &reg, 0, KTS_FULLCONTEXT);
}

void arch_set_secondary_ipc_return(struct proc *p, u32_t val)
{
	p->p_reg.bx = val;
}

int restore_fpu(struct proc *pr)
{
	int failed;
	char *state = pr->p_seg.fpu_state;

	assert(state);

	if(!proc_used_fpu(pr)) {
		fninit();
		pr->p_misc_flags |= MF_FPU_INITIALIZED;
	} else {
		if(osfxsr_feature) {
			failed = fxrstor(state);
		} else {
			failed = frstor(state);
		}

		if (failed) return EINVAL;
	}

	return OK;
}

void cpu_identify(void)
{
	u32_t eax, ebx, ecx, edx;
	unsigned cpu = cpuid;
	
	eax = 0;
	_cpuid(&eax, &ebx, &ecx, &edx);

	if (ebx == INTEL_CPUID_GEN_EBX && ecx == INTEL_CPUID_GEN_ECX &&
			edx == INTEL_CPUID_GEN_EDX) {
		cpu_info[cpu].vendor = CPU_VENDOR_INTEL;
	} else if (ebx == AMD_CPUID_GEN_EBX && ecx == AMD_CPUID_GEN_ECX &&
			edx == AMD_CPUID_GEN_EDX) {
		cpu_info[cpu].vendor = CPU_VENDOR_AMD;
	} else
		cpu_info[cpu].vendor = CPU_VENDOR_UNKNOWN;

	if (eax == 0) 
		return;

	eax = 1;
	_cpuid(&eax, &ebx, &ecx, &edx);

	cpu_info[cpu].family = (eax >> 8) & 0xf;
	if (cpu_info[cpu].family == 0xf)
		cpu_info[cpu].family += (eax >> 20) & 0xff;
	cpu_info[cpu].model = (eax >> 4) & 0xf;
	if (cpu_info[cpu].model == 0xf || cpu_info[cpu].model == 0x6)
		cpu_info[cpu].model += ((eax >> 16) & 0xf) << 4 ;
	cpu_info[cpu].stepping = eax & 0xf;
	cpu_info[cpu].flags[0] = ecx;
	cpu_info[cpu].flags[1] = edx;
}

void arch_init(void)
{
	k_stacks = (void*) &k_stacks_start;
	assert(!((vir_bytes) k_stacks % K_STACK_SIZE));

#ifndef CONFIG_SMP
	/*
	 * use stack 0 and cpu id 0 on a single processor machine, SMP
	 * configuration does this in smp_init() for all cpus at once
	 */
	tss_init(0, get_k_stack_top(0));
#endif

#if !CONFIG_OXPCIE
	ser_init();
#endif

#ifdef USE_ACPI
	acpi_init();
#endif

#if defined(USE_APIC) && !defined(CONFIG_SMP)
	if (config_no_apic) {
		DEBUGBASIC(("APIC disabled, using legacy PIC\n"));
	}
	else if (!apic_single_cpu_init()) {
		DEBUGBASIC(("APIC not present, using legacy PIC\n"));
	}
#endif

	/* Reserve some BIOS ranges */
	cut_memmap(&kinfo, BIOS_MEM_BEGIN, BIOS_MEM_END);
	cut_memmap(&kinfo, BASE_MEM_TOP, UPPER_MEM_END);
}

/*===========================================================================*
 *				do_ser_debug				     * 
 *===========================================================================*/
void do_ser_debug()
{
	u8_t c, lsr;

#if CONFIG_OXPCIE
	{
		int oxin;
		if((oxin = oxpcie_in()) >= 0)
		ser_debug(oxin);
	}
#endif

	lsr= inb(COM1_LSR);
	if (!(lsr & LSR_DR))
		return;
	c = inb(COM1_RBR);
	ser_debug(c);
}

static void ser_dump_queue_cpu(unsigned cpu)
{
	int q;
	struct proc ** rdy_head;
	
	rdy_head = get_cpu_var(cpu, run_q_head);

	for(q = 0; q < NR_SCHED_QUEUES; q++) {
		struct proc *p;
		if(rdy_head[q])	 {
			printf("%2d: ", q);
			for(p = rdy_head[q]; p; p = p->p_nextready) {
				printf("%s / %d  ", p->p_name, p->p_endpoint);
			}
			printf("\n");
		}
	}
}

static void ser_dump_queues(void)
{
#ifdef CONFIG_SMP
	unsigned cpu;

	printf("--- run queues ---\n");
	for (cpu = 0; cpu < ncpus; cpu++) {
		printf("CPU %d :\n", cpu);
		ser_dump_queue_cpu(cpu);
	}
#else
	ser_dump_queue_cpu(0);
#endif
}

#ifdef CONFIG_SMP
static void dump_bkl_usage(void)
{
	unsigned cpu;

	printf("--- BKL usage ---\n");
	for (cpu = 0; cpu < ncpus; cpu++) {
		printf("cpu %3d kernel ticks 0x%x%08x bkl ticks 0x%x%08x succ %d tries %d\n", cpu,
				ex64hi(kernel_ticks[cpu]),
				ex64lo(kernel_ticks[cpu]),
				ex64hi(bkl_ticks[cpu]),
				ex64lo(bkl_ticks[cpu]),
				bkl_succ[cpu], bkl_tries[cpu]);
	}
}

static void reset_bkl_usage(void)
{
	memset(kernel_ticks, 0, sizeof(kernel_ticks));
	memset(bkl_ticks, 0, sizeof(bkl_ticks));
	memset(bkl_tries, 0, sizeof(bkl_tries));
	memset(bkl_succ, 0, sizeof(bkl_succ));
}
#endif

static void ser_debug(const int c)
{
	serial_debug_active = 1;

	switch(c)
	{
	case 'Q':
		minix_shutdown(NULL);
		NOT_REACHABLE;
#ifdef CONFIG_SMP
	case 'B':
		dump_bkl_usage();
		break;
	case 'b':
		reset_bkl_usage();
		break;
#endif
	case '1':
		ser_dump_proc();
		break;
	case '2':
		ser_dump_queues();
		break;
#ifdef CONFIG_SMP
	case '4':
		ser_dump_proc_cpu();
		break;
#endif
	case '5':
		ser_dump_vfs();
		break;
#if DEBUG_TRACE
#define TOGGLECASE(ch, flag)				\
	case ch: {					\
		if(verboseflags & flag)	{		\
			verboseflags &= ~flag;		\
			printf("%s disabled\n", #flag);	\
		} else {				\
			verboseflags |= flag;		\
			printf("%s enabled\n", #flag);	\
		}					\
		break;					\
		}
	TOGGLECASE('8', VF_SCHEDULING)
	TOGGLECASE('9', VF_PICKPROC)
#endif
#ifdef USE_APIC
	case 'I':
		dump_apic_irq_state();
		break;
#endif
	}
	serial_debug_active = 0;
}

#if DEBUG_SERIAL

static void ser_dump_vfs()
{
	/* Notify VFS it has to generate stack traces. Kernel can't do that as
	 * it's not aware of user space threads.
	 */
	mini_notify(proc_addr(KERNEL), VFS_PROC_NR);
}

#ifdef CONFIG_SMP
static void ser_dump_proc_cpu(void)
{
	struct proc *pp;
	unsigned cpu;

	for (cpu = 0; cpu < ncpus; cpu++) {
		printf("CPU %d processes : \n", cpu);
		for (pp= BEG_USER_ADDR; pp < END_PROC_ADDR; pp++) {
			if (isemptyp(pp) || pp->p_cpu != cpu)
				continue;
			print_proc(pp);
		}
	}
}
#endif

#endif /* DEBUG_SERIAL */

#if SPROFILE

int arch_init_profile_clock(const u32_t freq)
{
  int r;
  /* Set CMOS timer frequency. */
  outb(RTC_INDEX, RTC_REG_A);
  outb(RTC_IO, RTC_A_DV_OK | freq);
  /* Enable CMOS timer interrupts. */
  outb(RTC_INDEX, RTC_REG_B);
  r = inb(RTC_IO);
  outb(RTC_INDEX, RTC_REG_B); 
  outb(RTC_IO, r | RTC_B_PIE);
  /* Mandatory read of CMOS register to enable timer interrupts. */
  outb(RTC_INDEX, RTC_REG_C);
  inb(RTC_IO);

  return CMOS_CLOCK_IRQ;
}

void arch_stop_profile_clock(void)
{
  int r;
  /* Disable CMOS timer interrupts. */
  outb(RTC_INDEX, RTC_REG_B);
  r = inb(RTC_IO);
  outb(RTC_INDEX, RTC_REG_B);  
  outb(RTC_IO, r & ~RTC_B_PIE);
}

void arch_ack_profile_clock(void)
{
  /* Mandatory read of CMOS register to re-enable timer interrupts. */
  outb(RTC_INDEX, RTC_REG_C);
  inb(RTC_IO);
}

#endif

void arch_do_syscall(struct proc *proc)
{
  /* do_ipc assumes that it's running because of the current process */
  assert(proc == get_cpulocal_var(proc_ptr));
  /* Make the system call, for real this time. */
  assert(proc->p_misc_flags & MF_SC_DEFER);
  proc->p_reg.retreg =
	  do_ipc(proc->p_defer.r1, proc->p_defer.r2, proc->p_defer.r3);
}

struct proc * arch_finish_switch_to_user(void)
{
	char * stk;
	struct proc * p;

#ifdef CONFIG_SMP
	stk = (char *)tss[cpuid].sp0;
#else
	stk = (char *)tss[0].sp0;
#endif
	/* set pointer to the process to run on the stack */
	p = get_cpulocal_var(proc_ptr);
	*((reg_t *)stk) = (reg_t) p;

	/* make sure IF is on in FLAGS so that interrupts won't be disabled
	 * once p's context is restored.
	 */
        p->p_reg.psw |= IF_MASK;

	/* Set TRACEBIT state properly. */
	if(p->p_misc_flags & MF_STEP)
        	p->p_reg.psw |= TRACEBIT;
	else
        	p->p_reg.psw &= ~TRACEBIT;

	return p;
}

void arch_proc_setcontext(struct proc *p, struct stackframe_s *state,
	int isuser, int trap_style)
{
	if(isuser) {
		/* Restore user bits of psw from sc, maintain system bits
		 * from proc.
		 */
		state->psw  =  (state->psw & X86_FLAGS_USER) |
			(p->p_reg.psw & ~X86_FLAGS_USER);
	}

	/* someone wants to totally re-initialize process state */
	assert(sizeof(p->p_reg) == sizeof(*state));
	if(state != &p->p_reg) {
		memcpy(&p->p_reg, state, sizeof(*state));
	}

	/* further code is instructed to not touch the context
	 * any more
	 */
	p->p_misc_flags |= MF_CONTEXT_SET;

	/* on x86 this requires returning using iret (KTS_INT)
	 * so that the full context is restored instead of relying on
	 * the userspace doing it (as it would do on SYSEXIT).
	 * as ESP and EIP are also reset, userspace won't try to
	 * restore bogus context after returning.
	 *
	 * if the process is not blocked, or the kernel will ignore
	 * our trap style, we needn't panic but things will probably
	 * not go well for the process (restored context will be ignored)
	 * and the situation should be debugged.
	 */
	if(!(p->p_rts_flags)) {
		printf("WARNINIG: setting full context of runnable process\n");
		print_proc(p);
		util_stacktrace();
	}
	if(p->p_seg.p_kern_trap_style == KTS_NONE)
		printf("WARNINIG: setting full context of out-of-kernel process\n");
	p->p_seg.p_kern_trap_style = trap_style;
}

void restore_user_context(struct proc *p)
{
	int trap_style = p->p_seg.p_kern_trap_style;
#if 0
#define TYPES 10
	static int restores[TYPES], n = 0;

	if(trap_style >= 0 && trap_style < TYPES)
		restores[trap_style]++;

	if(!(n++ % 500000)) {
		int t;
		for(t = 0; t < TYPES; t++)
			if(restores[t])
				printf("%d: %d   ", t, restores[t]);
		printf("\n");
	}
#endif

	p->p_seg.p_kern_trap_style = KTS_NONE;

	if(trap_style == KTS_SYSENTER) {
		restore_user_context_sysenter(p);
		NOT_REACHABLE;
        }

	if(trap_style == KTS_SYSCALL) {
		restore_user_context_syscall(p);
		NOT_REACHABLE;
	}

        switch(trap_style) {
                case KTS_NONE:
                        panic("no entry trap style known");
                case KTS_INT_HARD:
                case KTS_INT_UM:
                case KTS_FULLCONTEXT:
                case KTS_INT_ORIG:
			restore_user_context_int(p);
			NOT_REACHABLE;
                default:
                        panic("unknown trap style recorded");
                        NOT_REACHABLE;
        }

        NOT_REACHABLE;
}

void fpu_sigcontext(struct proc *pr, struct sigframe_sigcontext *fr, struct sigcontext *sc)
{
	int fp_error;

	if (osfxsr_feature) {
		fp_error = sc->sc_fpu_state.xfp_regs.fp_status &
			~sc->sc_fpu_state.xfp_regs.fp_control;
	} else {
		fp_error = sc->sc_fpu_state.fpu_regs.fp_status &
			~sc->sc_fpu_state.fpu_regs.fp_control;
	}

	if (fp_error & 0x001) {      /* Invalid op */
		/*
		 * swd & 0x240 == 0x040: Stack Underflow
		 * swd & 0x240 == 0x240: Stack Overflow
		 * User must clear the SF bit (0x40) if set
		 */
		fr->sf_code = FPE_FLTINV;
	} else if (fp_error & 0x004) {
		fr->sf_code = FPE_FLTDIV; /* Divide by Zero */
	} else if (fp_error & 0x008) {
		fr->sf_code = FPE_FLTOVF; /* Overflow */
	} else if (fp_error & 0x012) {
		fr->sf_code = FPE_FLTUND; /* Denormal, Underflow */
	} else if (fp_error & 0x020) {
		fr->sf_code = FPE_FLTRES; /* Precision */
	} else {
		fr->sf_code = 0;  /* XXX - probably should be used for FPE_INTOVF or
				  * FPE_INTDIV */
	}
}

reg_t arch_get_sp(struct proc *p) { return p->p_reg.sp; }

#if !CONFIG_OXPCIE
static void ser_init(void)
{
	unsigned char lcr;
	unsigned divisor;

	/* keep BIOS settings if cttybaud is not set */
	if (kinfo.serial_debug_baud <= 0) return;

	/* set DLAB to make baud accessible */
	lcr = LCR_8BIT | LCR_1STOP | LCR_NPAR;
	outb(COM1_LCR, lcr | LCR_DLAB);

	/* set baud rate */
	divisor = UART_BASE_FREQ / kinfo.serial_debug_baud;
	if (divisor < 1) divisor = 1;
	if (divisor > 65535) divisor = 65535;
	
	outb(COM1_DLL, divisor & 0xff);
	outb(COM1_DLM, (divisor >> 8) & 0xff);

	/* clear DLAB */
	outb(COM1_LCR, lcr);
}
#endif

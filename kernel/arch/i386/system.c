/* system dependent functions for use inside the whole kernel. */

#include "../../kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <ibm/cmos.h>
#include <ibm/bios.h>
#include <minix/portio.h>
#include <minix/cpufeature.h>
#include <a.out.h>
#include <archconst.h>

#include "proto.h"
#include "../../proc.h"
#include "../../debug.h"

#ifdef CONFIG_APIC
#include "apic.h"
#endif

/* set MP and NE flags to handle FPU exceptions in native mode. */
#define CR0_MP_NE	0x0022
/* set CR4.OSFXSR[bit 9] if FXSR is supported. */
#define CR4_OSFXSR	(1L<<9)
/* set OSXMMEXCPT[bit 10] if we provide #XM handler. */
#define CR4_OSXMMEXCPT	(1L<<10)

FORWARD _PROTOTYPE( void ser_debug, (int c));

PUBLIC void arch_monitor(void)
{
	level0(monitor);
}

PUBLIC int cpu_has_tsc;

PUBLIC void arch_shutdown(int how)
{
	/* Mask all interrupts, including the clock. */
	outb( INT_CTLMASK, ~0);

	if(minix_panicing) {
		/* We're panicing? Then retrieve and decode currently
		 * loaded segment selectors.
		 */
		printseg("cs: ", 1, proc_ptr, read_cs());
		printseg("ds: ", 0, proc_ptr, read_ds());
		if(read_ds() != read_ss()) {
			printseg("ss: ", 0, NULL, read_ss());
		}
	}

	if(how != RBT_RESET) {
		/* return to boot monitor */

		outb( INT_CTLMASK, 0);            
		outb( INT2_CTLMASK, 0);
        
		/* Return to the boot monitor. Set
		 * the program if not already done.
		 */
		if (how != RBT_MONITOR)
			arch_set_params("", 1);
		if(minix_panicing) {
			int source, dest;
			static char mybuffer[sizeof(params_buffer)];
			char *lead = "echo \\n*** kernel messages:\\n";
			int leadlen = strlen(lead);
			strcpy(mybuffer, lead);

#define DECSOURCE source = (source - 1 + _KMESS_BUF_SIZE) % _KMESS_BUF_SIZE

			dest = sizeof(mybuffer)-1;
			mybuffer[dest--] = '\0';

			source = kmess.km_next;
			DECSOURCE; 

			while(dest >= leadlen) {
				char c = kmess.km_buf[source];
				if(c == '\n') {
					mybuffer[dest--] = 'n';
					mybuffer[dest] = '\\';
				} else if(isprint(c) &&
					c != '\'' && c != '"' &&
					c != '\\' && c != ';') {
					mybuffer[dest] = c;
				} else	mybuffer[dest] = ' ';

				DECSOURCE;
				dest--;
			}

			arch_set_params(mybuffer, strlen(mybuffer)+1);
		}
		arch_monitor();
	} else {
		/* Reset the system by forcing a processor shutdown. First stop
		 * the BIOS memory test by setting a soft reset flag.
		 */
		u16_t magic = STOP_MEM_CHECK;
		phys_copy(vir2phys(&magic), SOFT_RESET_FLAG_ADDR,
       	 	SOFT_RESET_FLAG_SIZE);
		level0(reset);
	}
}

/* address of a.out headers, set in mpx386.s */
phys_bytes aout;

PUBLIC void arch_get_aout_headers(int i, struct exec *h)
{
	/* The bootstrap loader created an array of the a.out headers at
	 * absolute address 'aout'. Get one element to h.
	 */
	phys_copy(aout + i * A_MINHDR, vir2phys(h), (phys_bytes) A_MINHDR);
}

PRIVATE void tss_init(struct tss_s * tss, void * kernel_stack, unsigned cpu)
{
	/*
	 * make space for process pointer and cpu id and point to the first
	 * usable word
	 */
	tss->sp0 = ((unsigned) kernel_stack) - 2 * sizeof(void *);
	tss->ss0 = DS_SELECTOR;

	/*
	 * set the cpu id at the top of the stack so we know on which cpu is
	 * this stak in use when we trap to kernel
	 */
	*((reg_t *)(tss->sp0 + 1 * sizeof(reg_t))) = cpu;
}

PUBLIC void arch_init(void)
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
		fpu_presence = 1;
		if(_cpufeature(_CPUF_I386_FXSR)) {
			register struct proc *rp;
			phys_bytes aligned_fp_area;

			/* Enable FXSR feature usage. */
			write_cr4(read_cr4() | CR4_OSFXSR | CR4_OSXMMEXCPT);
			osfxsr_feature = 1;

			for (rp = BEG_PROC_ADDR; rp < END_PROC_ADDR; ++rp) {
				/* FXSR requires 16-byte alignment of memory
				 * image, but unfortunately some old tools
				 * (probably linker) ignores ".balign 16"
				 * applied to our memory image.
				 * Thus we have to do manual alignment.
				 */
				aligned_fp_area =
					(phys_bytes) &rp->p_fpu_state.fpu_image;
				if(aligned_fp_area % FPUALIGN) {
				    aligned_fp_area += FPUALIGN -
						   (aligned_fp_area % FPUALIGN);
				}
				rp->p_fpu_state.fpu_save_area_p =
						    (void *) aligned_fp_area;
			}
		} else {
			osfxsr_feature = 0;
		}
	} else {
		/* No FPU presents. */
                fpu_presence = 0;
                osfxsr_feature = 0;
                return;
        }

#ifdef CONFIG_APIC
	/*
	 * this is setting kernel segments to cover most of the phys memory. The
	 * value is high enough to reach local APIC nad IOAPICs before paging is
	 * turned on.
	 */
	prot_set_kern_seg_limit(0xfff00000);
	reload_ds();
#endif

	idt_init();

	tss_init(&tss, &k_boot_stktop, 0);

#if defined(CONFIG_APIC) && !defined(CONFIG_SMP)
	if (config_no_apic) {
		BOOT_VERBOSE(kprintf("APIC disabled, using legacy PIC\n"));
	}
	else if (!apic_single_cpu_init()) {
		BOOT_VERBOSE(kprintf("APIC not present, using legacy PIC\n"));
	}
#endif

}

#define COM1_BASE       0x3F8
#define COM1_THR        (COM1_BASE + 0)
#define COM1_RBR (COM1_BASE + 0)
#define COM1_LSR        (COM1_BASE + 5)
#define		LSR_DR		0x01
#define		LSR_THRE	0x20

PUBLIC void ser_putc(char c)
{
        int i;
        int lsr, thr;

        lsr= COM1_LSR;
        thr= COM1_THR;
        for (i= 0; i<100000; i++)
        {
                if (inb( lsr) & LSR_THRE)
                        break;
        }
        outb( thr, c);
}

/*===========================================================================*
 *				do_ser_debug				     * 
 *===========================================================================*/
PUBLIC void do_ser_debug()
{
	u8_t c, lsr;

	lsr= inb(COM1_LSR);
	if (!(lsr & LSR_DR))
		return;
	c = inb(COM1_RBR);
	ser_debug(c);
}

PRIVATE void ser_dump_queues(void)
{
	int q;
	for(q = 0; q < NR_SCHED_QUEUES; q++) {
		struct proc *p;
		if(rdy_head[q])	
			printf("%2d: ", q);
		for(p = rdy_head[q]; p; p = p->p_nextready) {
			printf("%s / %d  ", p->p_name, p->p_endpoint);
		}
		printf("\n");
	}

}

PRIVATE void ser_dump_segs(void)
{
	struct proc *pp;
	for (pp= BEG_PROC_ADDR; pp < END_PROC_ADDR; pp++)
	{
		if (isemptyp(pp))
			continue;
		kprintf("%d: %s ep %d\n", proc_nr(pp), pp->p_name, pp->p_endpoint);
		printseg("cs: ", 1, pp, pp->p_reg.cs);
		printseg("ds: ", 0, pp, pp->p_reg.ds);
		if(pp->p_reg.ss != pp->p_reg.ds) {
			printseg("ss: ", 0, pp, pp->p_reg.ss);
		}
	}
}

PRIVATE void ser_debug(int c)
{
	int u = 0;

	serial_debug_active = 1;
	/* Disable interrupts so that we get a consistent state. */
	if(!intr_disabled()) { lock; u = 1; };

	switch(c)
	{
	case 'Q':
		minix_shutdown(NULL);
		NOT_REACHABLE;
	case '1':
		ser_dump_proc();
		break;
	case '2':
		ser_dump_queues();
		break;
	case '3':
		ser_dump_segs();
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
	}
	serial_debug_active = 0;
	if(u) { unlock; }
}

PRIVATE void printslot(struct proc *pp, int level)
{
	struct proc *depproc = NULL;
	int dep = NONE;
#define COL { int i; for(i = 0; i < level; i++) printf("> "); }

	if(level >= NR_PROCS) {
		kprintf("loop??\n");
		return;
	}

	COL

	kprintf("%d: %s %d prio %d/%d time %d/%d cr3 0x%lx rts %s misc %s",
		proc_nr(pp), pp->p_name, pp->p_endpoint, 
		pp->p_priority, pp->p_max_priority, pp->p_user_time,
		pp->p_sys_time, pp->p_seg.p_cr3,
		rtsflagstr(pp->p_rts_flags), miscflagstr(pp->p_misc_flags));

	if(pp->p_rts_flags & RTS_SENDING) {
		dep = pp->p_sendto_e;
		kprintf(" to: ");
	} else if(pp->p_rts_flags & RTS_RECEIVING) {
		dep = pp->p_getfrom_e;
		kprintf(" from: ");
	}

	if(dep != NONE) {
		if(dep == ANY) {
			kprintf(" ANY\n");
		} else {
			int procno;
			if(!isokendpt(dep, &procno)) {
				kprintf(" ??? %d\n", dep);
			} else {
				depproc = proc_addr(procno);
				if(isemptyp(depproc)) {
					kprintf(" empty slot %d???\n", procno);
					depproc = NULL;
				} else {
					kprintf(" %s\n", depproc->p_name);
				}
			}
		}
	} else {
		kprintf("\n");
	}

	COL
	proc_stacktrace(pp);


	if(depproc)
		printslot(depproc, level+1);
}


PUBLIC void ser_dump_proc()
{
	struct proc *pp;

	for (pp= BEG_PROC_ADDR; pp < END_PROC_ADDR; pp++)
	{
		if (isemptyp(pp))
			continue;
		printslot(pp, 0);
	}
}

#if SPROFILE

PUBLIC int arch_init_profile_clock(u32_t freq)
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

PUBLIC void arch_stop_profile_clock(void)
{
  int r;
  /* Disable CMOS timer interrupts. */
  outb(RTC_INDEX, RTC_REG_B);
  r = inb(RTC_IO);
  outb(RTC_INDEX, RTC_REG_B);  
  outb(RTC_IO, r & ~RTC_B_PIE);
}

PUBLIC void arch_ack_profile_clock(void)
{
  /* Mandatory read of CMOS register to re-enable timer interrupts. */
  outb(RTC_INDEX, RTC_REG_C);
  inb(RTC_IO);
}

#endif

#define COLOR_BASE	0xB8000L

PRIVATE void cons_setc(int pos, int c)
{
	char ch;

	ch= c;
	phys_copy(vir2phys((vir_bytes)&ch), COLOR_BASE+(20*80+pos)*2, 1);
}

PRIVATE void cons_seth(int pos, int n)
{
	n &= 0xf;
	if (n < 10)
		cons_setc(pos, '0'+n);
	else
		cons_setc(pos, 'A'+(n-10));
}

/* Saved by mpx386.s into these variables. */
u32_t params_size, params_offset, mon_ds;

PUBLIC int arch_get_params(char *params, int maxsize)
{
	phys_copy(seg2phys(mon_ds) + params_offset, vir2phys(params),
		MIN(maxsize, params_size));
	params[maxsize-1] = '\0';
	return OK;
}

PUBLIC int arch_set_params(char *params, int size)
{
	if(size > params_size)
		return E2BIG;
	phys_copy(vir2phys(params), seg2phys(mon_ds) + params_offset, size);
	return OK;
}

PUBLIC void arch_do_syscall(struct proc *proc)
{
/* Perform a previously postponed system call.
 */
  int call_nr, src_dst_e;
  message *m_ptr;
  long bit_map;

  /* Get the system call parameters from their respective registers. */
  call_nr = proc->p_reg.cx;
  src_dst_e = proc->p_reg.retreg;
  m_ptr = (message *) proc->p_reg.bx;
  bit_map = proc->p_reg.dx;

  /* sys_call() expects the given process's memory to be accessible. */
  vm_set_cr3(proc);

  /* Make the system call, for real this time. */
  proc->p_reg.retreg = sys_call(call_nr, src_dst_e, m_ptr, bit_map);
}

PUBLIC struct proc * arch_finish_schedcheck(void)
{
	char * stk;
	stk = (char *)tss.sp0;
	/* set pointer to the process to run on the stack */
	*((reg_t *)stk) = (reg_t) proc_ptr;
	return proc_ptr;
}

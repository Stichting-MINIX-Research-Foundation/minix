/* system dependent functions for use inside the whole kernel. */

#include "../../kernel.h"

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <ibm/cmos.h>
#include <ibm/bios.h>
#include <minix/portio.h>
#include <minix/u64.h>
#include <minix/sysutil.h>
#include <a.out.h>

#include "proto.h"
#include "../../proc.h"
#include "../../debug.h"

#define CR0_EM	0x0004		/* set to enable trap on any FP instruction */

FORWARD _PROTOTYPE( void ser_debug, (int c));

PUBLIC void arch_shutdown(int how)
{
	/* Mask all interrupts, including the clock. */
	outb( INT_CTLMASK, ~0);

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
		level0(monitor);
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

PUBLIC void system_init(void)
{
	prot_init();

#if 0
	/* Set CR0_EM until we get FP context switching */
	write_cr0(read_cr0() | CR0_EM);
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

PRIVATE void ser_debug(int c)
{
	int u = 0;

	do_serial_debug++;
	/* Disable interrupts so that we get a consistent state. */
	if(!intr_disabled()) { lock; u = 1; };

	switch(c)
	{
	case '1':
		ser_dump_proc();
		break;
	case '2':
		ser_dump_queues();
		break;
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
	}
	do_serial_debug--;
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

	if(pp->p_ready && pp->p_rts_flags) {
		printf("HUH? p_ready but rts flags!\n");
	}

	if(!pp->p_ready && !pp->p_rts_flags) {
		printf("HUH? not p_ready but no rts flags!\n");
	}

	COL

	kprintf("%d: %s %d prio %d/%d time %d/%d cr3 0x%lx rts %s misc %s ready %d",
		proc_nr(pp), pp->p_name, pp->p_endpoint, 
		pp->p_priority, pp->p_max_priority, pp->p_user_time,
		pp->p_sys_time, pp->p_seg.p_cr3,
		rtsflagstr(pp->p_rts_flags), miscflagstr(pp->p_misc_flags),
		pp->p_ready);

	if(pp->p_rts_flags & SENDING) {
		dep = pp->p_sendto_e;
		kprintf(" to: ");
	} else if(pp->p_rts_flags & RECEIVING) {
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
				if(depproc->p_rts_flags & SLOT_FREE) {
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

	if(pp->p_rts_flags & VMREQUEST) {
		COL
		printf("vmrequest set with: %s\n", pp->p_vmrequest.stacktrace);
	}

	if(depproc)
		printslot(depproc, level+1);
}

PUBLIC void ser_dump_queues()
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

PUBLIC void ser_dump_proc()
{
	struct proc *pp;

	CHECK_RUNQUEUES;

	for (pp= BEG_PROC_ADDR; pp < END_PROC_ADDR; pp++)
	{
		if (pp->p_rts_flags & SLOT_FREE)
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

PUBLIC void cons_setc(int pos, int c)
{
	char ch;

	ch= c;
	phys_copy(vir2phys((vir_bytes)&ch), COLOR_BASE+(20*80+pos)*2, 1);
}

PUBLIC void cons_seth(int pos, int n)
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


/* system dependent functions for use inside the whole kernel. */

#include "../../kernel.h"

#include <unistd.h>
#include <ibm/cmos.h>
#include <ibm/bios.h>
#include <minix/portio.h>

#include "proto.h"
#include "../../proc.h"

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
			phys_copy(vir2phys(""), kinfo.params_base, 1);
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

PUBLIC void system_init(void)
{
	prot_init();
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
	do_serial_debug++;
	kprintf("ser_debug: %d\n", c);
	switch(c)
	{
	case '1':
		ser_dump_proc();
		break;
	}
	do_serial_debug--;
}

PUBLIC void ser_dump_proc()
{
	struct proc *pp;

	for (pp= BEG_PROC_ADDR; pp < END_PROC_ADDR; pp++)
	{
		if (pp->p_rts_flags & SLOT_FREE)
			continue;
		kprintf(
	"%d: 0x%02x %s e %d src %d dst %d prio %d/%d time %d/%d EIP 0x%x\n",
			proc_nr(pp),
			pp->p_rts_flags, pp->p_name,
			pp->p_endpoint, pp->p_getfrom_e, pp->p_sendto_e,
			pp->p_priority, pp->p_max_priority,
			pp->p_user_time, pp->p_sys_time, 
			pp->p_reg.pc);
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

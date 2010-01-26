/* This file contains routines for initializing the 8259 interrupt controller:
 *	put_irq_handler: register an interrupt handler
 *	rm_irq_handler: deregister an interrupt handler
 *	intr_handle:	handle a hardware interrupt
 *	intr_init:	initialize the interrupt controller(s)
 */

#include "../../kernel.h"
#include "../../proc.h"
#include "proto.h"
#include <minix/portio.h>
#include <ibm/cpu.h>

#define ICW1_AT         0x11	/* edge triggered, cascade, need ICW4 */
#define ICW1_PC         0x13	/* edge triggered, no cascade, need ICW4 */
#define ICW1_PS         0x19	/* level triggered, cascade, need ICW4 */
#define ICW4_AT_SLAVE   0x01	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_AT_MASTER  0x05	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_PC_SLAVE   0x09	/* not SFNM, buffered, normal EOI, 8086 */
#define ICW4_PC_MASTER  0x0D	/* not SFNM, buffered, normal EOI, 8086 */
#define ICW4_AT_AEOI_SLAVE   0x03 /* not SFNM, not buffered, auto EOI, 8086 */
#define ICW4_AT_AEOI_MASTER  0x07 /* not SFNM, not buffered, auto EOI, 8086 */
#define ICW4_PC_AEOI_SLAVE   0x0B /* not SFNM, buffered, auto EOI, 8086 */
#define ICW4_PC_AEOI_MASTER  0x0F /* not SFNM, buffered, auto EOI, 8086 */

/*===========================================================================*
 *				intr_init				     *
 *===========================================================================*/
PUBLIC int intr_init(int mine, int auto_eoi)
{
/* Initialize the 8259s, finishing with all interrupts disabled.  This is
 * only done in protected mode, in real mode we don't touch the 8259s, but
 * use the BIOS locations instead.  The flag "mine" is set if the 8259s are
 * to be programmed for MINIX, or to be reset to what the BIOS expects.
 */
  if (!intr_disabled())
	  intr_disable();

      /* The AT and newer PS/2 have two interrupt controllers, one master,
       * one slaved at IRQ 2.  (We don't have to deal with the PC that
       * has just one controller, because it must run in real mode.)
       */
      outb( INT_CTL, machine.ps_mca ? ICW1_PS : ICW1_AT);
      outb( INT_CTLMASK, mine == INTS_MINIX ? IRQ0_VECTOR : BIOS_IRQ0_VEC);
					/* ICW2 for master */
      outb( INT_CTLMASK, (1 << CASCADE_IRQ));
					/* ICW3 tells slaves */
      if (auto_eoi)
          outb( INT_CTLMASK, ICW4_AT_AEOI_MASTER);
      else
          outb( INT_CTLMASK, ICW4_AT_MASTER);
      outb( INT_CTLMASK, ~(1 << CASCADE_IRQ)); /* IRQ 0-7 mask */
      outb( INT2_CTL, machine.ps_mca ? ICW1_PS : ICW1_AT);
      outb( INT2_CTLMASK, mine == INTS_MINIX ? IRQ8_VECTOR : BIOS_IRQ8_VEC);
						/* ICW2 for slave */
      outb( INT2_CTLMASK, CASCADE_IRQ);	/* ICW3 is slave nr */
      if (auto_eoi)
         outb( INT2_CTLMASK, ICW4_AT_AEOI_SLAVE);
      else
         outb( INT2_CTLMASK, ICW4_AT_SLAVE);
      outb( INT2_CTLMASK, ~0);		/* IRQ 8-15 mask */

      /* Copy the BIOS vectors from the BIOS to the Minix location, so we
       * can still make BIOS calls without reprogramming the i8259s.
       */
#if IRQ0_VECTOR != BIOS_IRQ0_VEC
      phys_copy(BIOS_VECTOR(0) * 4L, VECTOR(0) * 4L, 8 * 4L);
#endif
#if IRQ8_VECTOR != BIOS_IRQ8_VEC
      phys_copy(BIOS_VECTOR(8) * 4L, VECTOR(8) * 4L, 8 * 4L);
#endif

  return OK;
}

/*===========================================================================*
 *				intr_disabled			     	     *
 *===========================================================================*/
PUBLIC int intr_disabled(void)
{
	if(!(read_cpu_flags() & X86_FLAG_I))
		return 1;
	return 0;
}

PUBLIC void irq_8259_unmask(int irq)
{
	unsigned ctl_mask = irq < 8 ? INT_CTLMASK : INT2_CTLMASK;
	outb(ctl_mask, inb(ctl_mask) & ~(1 << (irq & 0x7)));
}

PUBLIC void irq_8259_mask(int irq)
{
	unsigned ctl_mask = irq < 8 ? INT_CTLMASK : INT2_CTLMASK;
	outb(ctl_mask, inb(ctl_mask) | (1 << (irq & 0x7)));
}

/* Disable 8259 - write 0xFF in OCW1 master and slave. */
PRIVATE void i8259_disable(void)
{
	outb(INT2_CTLMASK, 0xFF);
	outb(INT_CTLMASK, 0xFF);
	inb(INT_CTLMASK);
}


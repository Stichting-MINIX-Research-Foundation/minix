/* This file contains routines for initializing the 8259 interrupt controller:
 *	put_irq_handler: register an interrupt handler
 *	intr_handle:	handle a hardware interrupt
 *	intr_init:	initialize the interrupt controller(s)
 */

#include "kernel.h"

#define ICW1_AT         0x11	/* edge triggered, cascade, need ICW4 */
#define ICW1_PC         0x13	/* edge triggered, no cascade, need ICW4 */
#define ICW1_PS         0x19	/* level triggered, cascade, need ICW4 */
#define ICW4_AT         0x01	/* not SFNM, not buffered, normal EOI, 8086 */
#define ICW4_PC         0x09	/* not SFNM, buffered, normal EOI, 8086 */

#if _WORD_SIZE == 2
typedef _PROTOTYPE( void (*vecaddr_t), (void) );

FORWARD _PROTOTYPE( void set_vec, (int vec_nr, vecaddr_t addr) );

PRIVATE vecaddr_t int_vec[] = {
  int00, int01, int02, int03, int04, int05, int06, int07,
};

PRIVATE vecaddr_t irq_vec[] = {
  hwint00, hwint01, hwint02, hwint03, hwint04, hwint05, hwint06, hwint07,
  hwint08, hwint09, hwint10, hwint11, hwint12, hwint13, hwint14, hwint15,
};
#else
#define set_vec(nr, addr)	((void)0)
#endif


/*==========================================================================*
 *				intr_init				    *
 *==========================================================================*/
PUBLIC void intr_init(mine)
int mine;
{
/* Initialize the 8259s, finishing with all interrupts disabled.  This is
 * only done in protected mode, in real mode we don't touch the 8259s, but
 * use the BIOS locations instead.  The flag "mine" is set if the 8259s are
 * to be programmed for Minix, or to be reset to what the BIOS expects.
 */

  int i;

  lock();
  if (protected_mode) {
	/* The AT and newer PS/2 have two interrupt controllers, one master,
	 * one slaved at IRQ 2.  (We don't have to deal with the PC that
	 * has just one controller, because it must run in real mode.)
	 */
	outb(INT_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	outb(INT_CTLMASK, mine ? IRQ0_VECTOR : BIOS_IRQ0_VEC);
							/* ICW2 for master */
	outb(INT_CTLMASK, (1 << CASCADE_IRQ));		/* ICW3 tells slaves */
	outb(INT_CTLMASK, ICW4_AT);
	outb(INT_CTLMASK, ~(1 << CASCADE_IRQ));		/* IRQ 0-7 mask */
	outb(INT2_CTL, ps_mca ? ICW1_PS : ICW1_AT);
	outb(INT2_CTLMASK, mine ? IRQ8_VECTOR : BIOS_IRQ8_VEC);
							/* ICW2 for slave */
	outb(INT2_CTLMASK, CASCADE_IRQ);		/* ICW3 is slave nr */
	outb(INT2_CTLMASK, ICW4_AT);
	outb(INT2_CTLMASK, ~0);				/* IRQ 8-15 mask */

	/* Copy the BIOS vectors from the BIOS to the Minix location, so we
	 * can still make BIOS calls without reprogramming the i8259s.
	 */
#if IRQ0_VECTOR != BIOS_IRQ0_VEC
	phys_copy(BIOS_VECTOR(0) * 4L, VECTOR(0) * 4L, 8 * 4L);
#endif
#if IRQ8_VECTOR != BIOS_IRQ8_VEC
	phys_copy(BIOS_VECTOR(8) * 4L, VECTOR(8) * 4L, 8 * 4L);
#endif
  } else {
	/* Use the BIOS interrupt vectors in real mode.  We only reprogram the
	 * exceptions here, the interrupt vectors are reprogrammed on demand.
	 * SYS_VECTOR is the Minix system call for message passing.
	 */
	for (i = 0; i < 8; i++) set_vec(i, int_vec[i]);
	set_vec(SYS_VECTOR, s_call);
  }
}

/*=========================================================================*
 *				put_irq_handler				   *
 *=========================================================================*/
PUBLIC void put_irq_handler(hook, irq, handler)
irq_hook_t *hook;
int irq;
irq_handler_t handler;
{
/* Register an interrupt handler. */
  int id;
  irq_hook_t **line;

  if ((unsigned) irq >= NR_IRQ_VECTORS)
	panic("invalid call to put_irq_handler", irq);

  line = &irq_hooks[irq];
  id = 1;
  while (*line != NULL) {
	if (hook == *line) return;	/* extra initialization */
	line = &(*line)->next;
	id <<= 1;
  }
  if (id == 0) panic("Too many handlers for irq", irq);

  hook->next = NULL;
  hook->handler = handler;
  hook->irq = irq;
  hook->id = id;
  *line = hook;

  irq_use |= 1 << irq;
}

/*==========================================================================*
 *				intr_handle				    *
 *==========================================================================*/
PUBLIC void intr_handle(hook)
irq_hook_t *hook;
{
/* Call the interrupt handlers for an interrupt with the given hook list.
 * The assembly part of the handler has already masked the IRQ, reenabled the
 * controller(s) and enabled interrupts.
 */

  /* Call list of handlers for an IRQ. */
  while (hook != NULL) {
	/* For each handler in the list, mark it active by setting its ID bit,
	 * call the function, and unmark it if the function returns true.
	 */
	irq_actids[hook->irq] |= hook->id;
	if ((*hook->handler)(hook)) irq_actids[hook->irq] &= ~hook->id;
	hook = hook->next;
  }

  /* The assembly code will now disable interrupts, unmask the IRQ if and only
   * if all active ID bits are cleared, and restart a process.
   */
}

#if _WORD_SIZE == 2
/*===========================================================================*
 *                                   set_vec                                 *
 *===========================================================================*/
PRIVATE void set_vec(vec_nr, addr)
int vec_nr;			/* which vector */
vecaddr_t addr;			/* where to start */
{
/* Set up a real mode interrupt vector. */

  u16_t vec[2];

  /* Build the vector in the array 'vec'. */
  vec[0] = (u16_t) addr;
  vec[1] = (u16_t) physb_to_hclick(code_base);

  /* Copy the vector into place. */
  phys_copy(vir2phys(vec), vec_nr * 4L, 4L);
}
#endif /* _WORD_SIZE == 2 */

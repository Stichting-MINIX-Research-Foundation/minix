/* Interrupt numbers and hardware vectors. */

#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#if (CHIP == INTEL)

/* 8259A interrupt controller ports. */
#define INT_CTL         0x20	/* I/O port for interrupt controller */
#define INT_CTLMASK     0x21	/* setting bits in this port disables ints */
#define INT2_CTL        0xA0	/* I/O port for second interrupt controller */
#define INT2_CTLMASK    0xA1	/* setting bits in this port disables ints */

/* Magic numbers for interrupt controller. */
#define END_OF_INT      0x20	/* code used to re-enable after an interrupt */

/* Interrupt vectors defined/reserved by processor. */
#define DIVIDE_VECTOR      0	/* divide error */
#define DEBUG_VECTOR       1	/* single step (trace) */
#define NMI_VECTOR         2	/* non-maskable interrupt */
#define BREAKPOINT_VECTOR  3	/* software breakpoint */
#define OVERFLOW_VECTOR    4	/* from INTO */

/* Fixed system call vector. */
#define SYS_VECTOR        32	/* system calls are made with int SYSVEC */
#define SYS386_VECTOR     33	/* except 386 system calls use this */
#define LEVEL0_VECTOR     34	/* for execution of a function at level 0 */

/* Suitable irq bases for hardware interrupts.  Reprogram the 8259(s) from
 * the PC BIOS defaults since the BIOS doesn't respect all the processor's
 * reserved vectors (0 to 31).
 */
#define BIOS_IRQ0_VEC   0x08	/* base of IRQ0-7 vectors used by BIOS */
#define BIOS_IRQ8_VEC   0x70	/* base of IRQ8-15 vectors used by BIOS */
#define IRQ0_VECTOR     0x50	/* nice vectors to relocate IRQ0-7 to */
#define IRQ8_VECTOR     0x70	/* no need to move IRQ8-15 */

/* Hardware interrupt numbers. */
#define NR_IRQ_VECTORS    16
#define CLOCK_IRQ          0
#define KEYBOARD_IRQ       1
#define CASCADE_IRQ        2	/* cascade enable for 2nd AT controller */
#define ETHER_IRQ          3	/* default ethernet interrupt vector */
#define SECONDARY_IRQ      3	/* RS232 interrupt vector for port 2 */
#define RS232_IRQ          4	/* RS232 interrupt vector for port 1 */
#define XT_WINI_IRQ        5	/* xt winchester */
#define FLOPPY_IRQ         6	/* floppy disk */
#define PRINTER_IRQ        7
#define AT_WINI_0_IRQ     14	/* at winchester controller 0 */
#define AT_WINI_1_IRQ     15	/* at winchester controller 1 */

/* Interrupt number to hardware vector. */
#define BIOS_VECTOR(irq)	\
	(((irq) < 8 ? BIOS_IRQ0_VEC : BIOS_IRQ8_VEC) + ((irq) & 0x07))
#define VECTOR(irq)	\
	(((irq) < 8 ? IRQ0_VECTOR : IRQ8_VECTOR) + ((irq) & 0x07))

#endif /* (CHIP == INTEL) */

#endif /* _INTERRUPT_H */

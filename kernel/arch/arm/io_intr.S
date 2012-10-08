/*	intr_disable(), intr_enable - Disable/Enable hardware interrupts. */
/*	void intr_disable(void); */
/*	void intr_enable(void); */
#include <machine/asm.h>

ENTRY(intr_disable)
	dsb
	cpsid i
	bx lr

ENTRY(intr_enable)
	dsb
	cpsie i
	bx lr

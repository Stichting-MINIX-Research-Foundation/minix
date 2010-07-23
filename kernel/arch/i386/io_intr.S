/*	intr_disable(), intr_enable - Disable/Enable hardware interrupts. */
/*							Author: Kees J. Bot */
/*								18 Mar 1996 */
/*	void intr_disable(void); */
/*	void intr_enable(void); */

.text
.globl	_intr_disable
_intr_disable:
	cli
	ret

.globl	_intr_enable
_intr_enable:
	sti
	ret

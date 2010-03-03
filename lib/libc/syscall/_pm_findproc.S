.text
.extern	___pm_findproc
.globl	__pm_findproc
.balign	2

__pm_findproc:
	jmp	___pm_findproc

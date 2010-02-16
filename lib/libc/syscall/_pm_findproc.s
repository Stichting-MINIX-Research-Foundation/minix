.sect .text
.extern	___pm_findproc
.define	__pm_findproc
.align 2

__pm_findproc:
	jmp	___pm_findproc

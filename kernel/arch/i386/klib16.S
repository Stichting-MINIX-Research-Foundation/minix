/* sections */


#include <minix/config.h>
#include <minix/const.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "sconst.h"
#include "multiboot.h"

/*
 * This file contains a number of 16-bit assembly code utility routines needed by the
 * kernel.  They are:
 */

.globl	_poweroff16 /* enter real mode */
.globl	_poweroff16_end

.text

/*===========================================================================*/
/*				poweroff16					     */
/*===========================================================================*/
/* PUBLIC void poweroff16(); */
/* Power down system */
_poweroff16:
	/* Assume eax is already set to required value of cr0*/
.byte	0x0F,0x22,0xC0	/* mov %cr0,%eax */
	jmpf	$0,$(BIOS_POWEROFF_ENTRY + real_mode - _poweroff16)
real_mode:
	mov	$((BIOS_POWEROFF_ENTRY >> 4) + 0x200),%ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss
	mov	$0x1000, %sp

	xorb	%ah, %ah
/* Close gate A20 */
gate_A20:
	call	kb_wait
	movb	$0xD1,%al
	outb	0x64
	call	kb_wait
	movb	$0xDD,%al
	orb		%ah,%al
	outb	0x60
	call	kb_wait
	movb	$0xFF,%al
	outb	0x64
	call	kb_wait
	
	/* Connect to APM */
	mov	$0x5301,%ax
	mov	$0x0,%bx
	int	0x15
	
	/* Enable power management */
	mov	$0x5308,%ax
	mov	$0x1,%bx
	mov	$0x1,%cx
	int	0x15
	
	/* Set power state to off */
	mov	$0x5307,%ax
	mov	$0x01,%bx
	mov	$0x3,%cx
	int	0x15
0:	hlt
	jmp	0b
	
kb_wait:
	inb	0x64
	testb	$0x02,%al
	jnz	kb_wait
	ret
/*mark the end for copy*/
_poweroff16_end:

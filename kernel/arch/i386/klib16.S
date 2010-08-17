/* sections */


#include <minix/config.h>
#include <minix/const.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "sconst.h"
#include "multiboot.h"

/*
 * This file contains a number of 16-bit assembly code utility routines needed by the
 * kernel.
 */

.text
.code16

/*===========================================================================*/
/*				poweroff16					     */
/*===========================================================================*/
/* PUBLIC void poweroff16(); */
/* Power down system */
ENTRY(poweroff16)
	/* Assume eax is already set to required value of cr0*/
.byte	0x0F,0x22,0xC0	/* mov %cr0,%eax */
	ljmp	$0,$(BIOS_POWEROFF_ENTRY + real_mode - _C_LABEL(poweroff16))
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
	outb	$0x64
	call	kb_wait
	movb	$0xDD,%al
	orb		%ah,%al
	outb	$0x60
	call	kb_wait
	movb	$0xFF,%al
	outb	$0x64
	call	kb_wait
	
	/* Connect to APM */
	mov	$0x5301,%ax
	mov	$0x0,%bx
	int	$0x15
	jc	apm_err
	
	/* Enable power management */
	mov	$0x5308,%ax
	mov	$0x1,%bx
	mov	$0x1,%cx
	int	$0x15
	jc	apm_err
	
	/* Set power state to off */
	mov	$0x5307,%ax
	mov	$0x01,%bx
	mov	$0x3,%cx
	int	$0x15
	jc	apm_err
0:	hlt
	jmp	0b
	
poweroff_msg:
.ascii	"You can poweroff the machine safely now"
poweroff_msg_end:
#define POWEROFF_MSG_LEN	(poweroff_msg_end-poweroff_msg)
apm_err:
	/* If APM can't perform the shutdown, print something to inform */
	mov	$0x02, %ax	/* clear screen */
	int	$0x10
	
	mov	$(BIOS_POWEROFF_ENTRY >> 4), %ax
	mov	%ax, %es
	mov	$0x1301, %ax
	mov	$0x07, %bx
	mov	$POWEROFF_MSG_LEN, %cx
	mov	$0x0300, %dx
	mov	$(poweroff_msg - _C_LABEL(poweroff16)), %bp
	int	$0x10
0:	hlt
	jmp	0b
	
kb_wait:
	inb	$0x64
	testb	$0x02,%al
	jnz	kb_wait
	ret
/*mark the end for copy*/
LABEL(poweroff16_end)

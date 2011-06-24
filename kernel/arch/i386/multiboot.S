#include "kernel/kernel.h" /* configures the kernel */
#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "kernel/const.h"
#include "kernel/proc.h"
#include "sconst.h"
#include <machine/multiboot.h>

#define GDT_SET_ENTRY(selector, base, limit) \
	mov	%ebp, %edi; \
	add	$(_C_LABEL(gdt) + selector), %edi; \
	mov	base, %eax; \
	movw %ax, 2(%edi); \
	shr	$16, %eax; \
	movb	%al, 4(%edi); \
	and	$0xff00, %ax; \
	andw	$0xff, 6(%edi); \
	or	%ax, 6(%edi); \
	mov	limit, %eax; \
	movw	%ax, (%edi); \
	shr	$16, %eax; \
	and	$0xf, %ax; \
	andb	$0xf0, 6(%edi); \
	or	%ax, 6(%edi); \

IMPORT(pre_init)
.extern kernel_init

ENTRY(multiboot_init)
	mov	$(GDT_SIZE*DESC_SIZE), %eax
	mov	$(_C_LABEL(gdt) + GDT_SELECTOR), %edi
	mov	%ax, (%edi)
	mov	$_C_LABEL(gdt), %eax
	mov	%eax, 2(%edi)
	lgdt	(%edi)
	ljmp	$(CS_SELECTOR), $reload_cs

reload_cs:
	mov	$DS_SELECTOR, %eax
	mov	%eax, %ds
	mov	%eax, %ss
	mov	%eax, %es
	mov	%eax, %fs
	mov	%eax, %gs
	
	mov	$(multiboot_stack + MULTIBOOT_STACK_SIZE), %esp

	push	%ebx
	call	_C_LABEL(pre_init)
	
	add	$4, %esp

	/* return to old boot code of kernel */
	push	%eax
	push	$MULTIBOOT_PARAM_BUF_SIZE
	push	$_C_LABEL(multiboot_param_buf)
	push	$0

	mov	$ES_SELECTOR, %eax
	mov	%eax, %es

	jmp	kernel_init

.data
LABEL(multiboot_param_buf)
	.space	MULTIBOOT_PARAM_BUF_SIZE

multiboot_stack:
.space	MULTIBOOT_STACK_SIZE + 4

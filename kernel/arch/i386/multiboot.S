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
#include "multiboot.h"

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

.globl multiboot_init
multiboot_init:
	/* Get size of kernel text */
	mov	MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_KERNEL_a_text, %ecx
	
	/* Get size of kernel text and ceil to 0x1000, and it's the offset
	   of data seg */
	mov	%ecx, %eax
	dec	%eax
	and	$0xfffff000, %eax
	add	$0x1000, %eax

	/* Calculate and save kernel data base address */
	mov	$(MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_ENTRY_OFFSET), %ebp
	add	%eax, %ebp
	mov	%ebp, _C_LABEL(kernel_data_addr)(%ebp)
	
	/* Init text seg */
	GDT_SET_ENTRY(CS_SELECTOR, \
				$(MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_ENTRY_OFFSET), \
				%ecx)
	
	/* Init data seg */
	GDT_SET_ENTRY(DS_SELECTOR, \
				%ebp, \
				MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_KERNEL_a_total)
	
	/* Make up monitor data seg, the same value as DS, different entry */
	GDT_SET_ENTRY(SS_SELECTOR, \
				%ebp, \
				MULTIBOOT_LOAD_ADDRESS + MULTIBOOT_KERNEL_a_total)
	
	/* Make up monitor text seg, used to return to real mode when poweroff */
	GDT_SET_ENTRY(MON_CS_SELECTOR, \
				$BIOS_POWEROFF_ENTRY, \
				$0xffff)
	
	mov	$(GDT_SIZE*DESC_SIZE), %eax
	mov	%ebp, %edi
	add	$(_C_LABEL(gdt) + GDT_SELECTOR), %edi
	mov	%ax, (%edi)
	mov	%ebp, %eax
	add	$_C_LABEL(gdt), %eax
	mov	%eax, 2(%edi)
	lgdt	(%edi)
	
	ljmp	$(CS_SELECTOR), $reload_cs
	
reload_cs:
	mov	$DS_SELECTOR, %eax
	mov	%eax, %ds
	mov	%eax, %ss
	mov	%eax, %es
	
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
LABEL(kernel_data_addr)
	.long	0
LABEL(a_out_headers)
	.space	NR_BOOT_PROCS * 32 /* is A_MINHDR */

LABEL(multiboot_param_buf)
	.space	MULTIBOOT_PARAM_BUF_SIZE

multiboot_stack:
.space	MULTIBOOT_STACK_SIZE + 4

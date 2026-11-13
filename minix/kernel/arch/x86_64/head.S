#include "kernel/kernel.h" /* configures the kernel */

/* sections */

#include <machine/vm.h>
#include "kernel/kernel.h"
#include <minix/config.h>
#include <minix/const.h>
#include <minix/com.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include "archconst.h"
#include "sconst.h"
#define ASM_FILE
#include <machine/multiboot2.h>
#undef ASM_FILE

#include "arch_proto.h" /* K_STACK_SIZE */

#ifdef CONFIG_SMP
#include "kernel/smp.h"
#endif

IMPORT(pre_init)
IMPORT(kmain)
IMPORT(direct_com_print)

/*
 * Multiboot2 header — x86_64 UEFI kernel.
 *
 * The architecture field is MULTIBOOT2_ARCHITECTURE_I386 (0) per spec.
 * The EFI boot-services tag (type 7) is required to activate the EFI64
 * entry-address tag (type 9).  UEFI bootloaders then enter directly in
 * 64-bit long mode via multiboot_entry64_efi.
 */

.text
/*===========================================================================*/
/*				MINIX					     */
/*===========================================================================*/
.global MINIX
MINIX:
	/* Non-EFI entry: not supported; hang. */
.Lhang_early:
	hlt
	jmp	.Lhang_early

/*
 * MB2_HDR_LEN: total multiboot2 header size in bytes.
 * Fixed header (16) + info-request tag+pad (32) + EFI boot-services tag (8)
 * + EFI64 entry tag+pad (16) + end (8) = 80.
 * Must match the structure below exactly.
 */
#define MB2_HDR_LEN 80

/* Multiboot2 header — must appear within the first 32 KB of the image */
.balign 8
.globl Multiboot2_Header
_C_LABEL(Multiboot2_Header):
	.long	MULTIBOOT2_HEADER_MAGIC
	.long	MULTIBOOT_ARCHITECTURE_I386
	.long	MB2_HDR_LEN
	/* checksum: -(magic + arch + len) mod 2^32, for an 80-byte header */
	.long	0x17ADAEDA

	/* Information-request tag */
	.short	1		/* MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST */
	.short	0		/* required */
	.long	28		/* size: 8 hdr + 5 × 4 requests (no padding inside) */
	.long	1		/* MULTIBOOT_TAG_TYPE_CMDLINE */
	.long	3		/* MULTIBOOT_TAG_TYPE_MODULE */
	.long	6		/* MULTIBOOT_TAG_TYPE_MMAP */
	.long	8		/* MULTIBOOT_TAG_TYPE_FRAMEBUFFER */
	.long	17		/* MULTIBOOT_TAG_TYPE_EFI_MMAP */
	.long	0		/* inter-tag pad to 8-byte boundary */

	/* EFI boot services — required to make the EFI64 entry tag active */
	.short	7		/* MULTIBOOT_HEADER_TAG_EFI_BS */
	.short	0
	.long	8

	/* EFI-64 entry — UEFI bootloaders enter in 64-bit long mode */
	.short	9		/* MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS_EFI64 */
	.short	0
	.long	12
	.long	multiboot_entry64_efi
	.long	0		/* pad */

	/* UEFI does not support EGA console; no CONSOLE_FLAGS tag */

	/* End tag */
	.short	0		/* MULTIBOOT_HEADER_TAG_END */
	.short	0
	.long	8

/* Saved Multiboot2 arguments. */
.data
.balign 8
boot_saved_magic:
	.long	0
.balign 8
boot_saved_info:
	.quad	0
.Lpre_init_returned_msg:
	.asciz	"MINIX/amd64: pre_init returned\r\n"
.Lhigh_stack_ready_msg:
	.asciz	"MINIX/amd64: high kernel stack ready\r\n"

/*===========================================================================*/
/*  EFI-64 entry — bootloader has already set up 64-bit long mode           */
/*===========================================================================*/
/* Back to .text: the preceding .data block left us in the data section, so
 * without this the entry code would be emitted into .unpaged_data. */
.text
.code64
multiboot_entry64_efi:
	/*
	 * The Multiboot2 EFI amd64 handoff follows the UEFI x64 boot-services
	 * machine state.  In particular, the firmware GDT and identity-mapped
	 * page tables are still valid (the EFI_BS header tag told the loader
	 * not to call ExitBootServices).
	 *
	 * Do not replace the GDT or CR3 here.  pre_init() builds and installs
	 * MINIX's own 4-level page tables after it has consumed the boot info.
	 * Replacing either structure before that point needlessly discards
	 * firmware state and makes an early triple fault very difficult to
	 * diagnose.
	 */
	cli
	cld

	/* Save the Multiboot2 arguments before using the registers. */
	movl	%eax, boot_saved_magic(%rip)
	movq	%rbx, boot_saved_info(%rip)

	/* Use a private, page-aligned bootstrap stack in the kernel image. */
	leaq	boot_stack64_top(%rip), %rsp
	xorq	%rbp, %rbp

	/* SysV AMD64: pre_init(magic, 64-bit MBI address). */
	movl	boot_saved_magic(%rip), %edi
	movq	boot_saved_info(%rip), %rsi
	call	_C_LABEL(pre_init)

	/* Preserve the boot-info pointer while tracing the return path. */
	movq	%rax, %r12
	leaq	.Lpre_init_returned_msg(%rip), %rdi
	call	_C_LABEL(direct_com_print)
	movq	%r12, %rax

	/*
	 * pre_init() has installed the MINIX page tables.  The kernel image
	 * is mapped at its high virtual address now, so switch to the normal
	 * high kernel stack before entering kmain().  mpx.S aligns this stack
	 * to K_STACK_SIZE; enforce the SysV AMD64 16-byte call-site alignment
	 * here as well.
	 */
	movq	$k_initial_stktop, %rsp
	andq	$-16, %rsp
	xorq	%rbp, %rbp
	movq	%rax, %r12
	leaq	.Lhigh_stack_ready_msg(%rip), %rdi
	call	_C_LABEL(direct_com_print)
	movq	%r12, %rdi

	/* The physical-to-high-virtual displacement does not fit rel32. */
	movabsq	$_C_LABEL(kmain), %rax
	callq	*%rax

.Lhang:
	hlt
	jmp	.Lhang

/*===========================================================================*/
/*  Boot stack (placed in .data so it lands in the unpaged section)         */
/*===========================================================================*/
.data
/* The early C bootstrap needs the same 16 KiB stack budget as the kernel. */
.balign K_STACK_SIZE
boot_stack64:
	.space	K_STACK_SIZE
boot_stack64_top:


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

/*
 * Multiboot2 header — x86_64 UEFI kernel.
 *
 * The architecture field is MULTIBOOT2_ARCHITECTURE_I386 (0) per spec.
 * Only the EFI64 entry address tag (type 9) is provided; UEFI bootloaders
 * enter directly in 64-bit long mode via multiboot_entry64_efi.
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
 * Fixed header (16) + info-request tag+pad (32) + EFI64 tag+pad (16) + end (8) = 72.
 * Must match the structure below exactly.
 */
#define MB2_HDR_LEN 72

/* Multiboot2 header — must appear within the first 32 KB of the image */
.balign 8
.globl Multiboot2_Header
_C_LABEL(Multiboot2_Header):
	.long	MULTIBOOT2_HEADER_MAGIC
	.long	MULTIBOOT_ARCHITECTURE_I386
	.long	MB2_HDR_LEN
	/* checksum: -(magic + arch + len) mod 2^32 = 0x100000000 - 0xe85250d6 - 0 - 72 */
	.long	0x17ADAEE2

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

/*===========================================================================*/
/*  Minimal boot GDT  (used until prot_init() installs the real GDT)        */
/*  Placed in .data so it ends up in the unpaged section at a known phys     */
/*  address.                                                                  */
/*===========================================================================*/
.data
.balign 8
boot_gdt:
	/* 0: null descriptor */
	.quad	0
	/* 1 (KERN_CS_INDEX): 64-bit kernel code, DPL=0, L=1, D=0 */
	.long	0x0000FFFF		/* limit[15:0], base[15:0]=0 */
	.long	0x00AF9A00		/* G=1,L=1, access=0x9A, base=0 */
	/* 2 (KERN_DS_INDEX): kernel data, DPL=0 */
	.long	0x0000FFFF
	.long	0x00CF9200		/* G=1,D=1, access=0x92, base=0 */
boot_gdt_end:

/* 64-bit GDTR (10 bytes: u16 limit + u64 base) — used from EFI-64 entry */
.balign 4
boot_gdtptr64:
	.short	boot_gdt_end - boot_gdt - 1
	.quad	boot_gdt

/* Saved multiboot2 arguments — written early, before page-table loops */
boot_saved_magic:
	.long	0
boot_saved_info:
	.long	0

/*===========================================================================*/
/*  EFI-64 entry — bootloader has already set up 64-bit long mode           */
/*===========================================================================*/
/* Back to .text: the preceding .data block left us in the data section, so
 * without this the entry code would be emitted into .unpaged_data. */
.text
.code64
multiboot_entry64_efi:
	/*
	 * EAX = MULTIBOOT2_BOOTLOADER_MAGIC
	 * EBX = physical address of multiboot2 info structure
	 * CPU is in 64-bit mode with the bootloader's GDT and page tables.
	 */

	/* Save args to memory before we clobber anything (RIP-relative) */
	movl	%eax, boot_saved_magic(%rip)
	movl	%ebx, boot_saved_info(%rip)

	/* Install our boot GDT via RIP-relative address */
	leaq	boot_gdtptr64(%rip), %rax
	lgdtq	(%rax)

	/* Reload CS to our 64-bit code segment via far-return trick */
	leaq	.Lentry64_efi_cs_reloaded(%rip), %rax
	pushq	$KERN_CS_SELECTOR
	pushq	%rax
	lretq

.Lentry64_efi_cs_reloaded:
	/* Build boot page tables with 64-bit writes (RIP-relative addressing) */

	/* PML4[0] → pdpt_low */
	leaq	boot_pdpt_low(%rip), %rax
	orq	$3, %rax
	leaq	boot_pml4(%rip), %rcx
	movq	%rax, (%rcx)

	/* PML4[511] → pdpt_high */
	leaq	boot_pdpt_high(%rip), %rax
	orq	$3, %rax
	movq	%rax, 4088(%rcx)	/* 511 * 8 = 4088 */

	/* pdpt_low[0..3] → 4 consecutive PD pages (identity-map low 4 GB) */
	leaq	boot_pd_low(%rip), %rax
	orq	$3, %rax
	leaq	boot_pdpt_low(%rip), %rcx
	movl	$4, %edx
.Lfill_pdpt_low_efi:
	movq	%rax, (%rcx)
	addq	$8, %rcx
	addq	$0x1000, %rax		/* next PD page */
	decl	%edx
	jnz	.Lfill_pdpt_low_efi

	/* pdpt_high[510] → pd_kern */
	leaq	boot_pd_kern(%rip), %rax
	orq	$3, %rax
	leaq	boot_pdpt_high(%rip), %rcx
	movq	%rax, 4080(%rcx)	/* 510 * 8 = 4080 */

	/*
	 * Identity-map 0–4 GB: 2048 × 2 MB pages across the 4 PDs.
	 * The EFI loader allocates the multiboot2 info struct (and its own
	 * heap) with AllocateAnyPages, which routinely lands well above the
	 * first 16 MB.  pre_init() dereferences that pointer in
	 * get_parameters_mb2() *before* it can build the full identity map,
	 * so the boot map must already reach it.  4 GB covers RAM on the VMs
	 * we boot on.
	 */
	leaq	boot_pd_low(%rip), %rdi
	movq	$0x83, %rax
	movl	$2048, %ecx
.Lfill_pd_low_efi:
	movq	%rax, (%rdi)
	addq	$8, %rdi
	addq	$0x200000, %rax
	loop	.Lfill_pd_low_efi

	/* Map kernel at pd_kern[2] (virt 0xFFFFFFFF80400000) */
	leaq	boot_pd_kern(%rip), %rdi
	addq	$(2*8), %rdi
	movq	$0x00400083, %rax	/* phys 0x00400000, PS | RW | P */
	movl	$8, %ecx
.Lfill_pd_kern_efi:
	movq	%rax, (%rdi)
	addq	$8, %rdi
	addq	$0x200000, %rax
	loop	.Lfill_pd_kern_efi

	/* Switch to our page tables */
	leaq	boot_pml4(%rip), %rax
	movq	%rax, %cr3

	/* Fall through to common 64-bit entry */

/*===========================================================================*/
/*  Common 64-bit entry point                                                */
/*===========================================================================*/
.code64
.Lentry64_common:
	/* Null out data segment registers (valid in 64-bit mode) */
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	/* Switch to 64-bit boot stack */
	leaq	boot_stack64_top(%rip), %rsp
	xorq	%rbp, %rbp

	/*
	 * Load magic and info_ptr saved by multiboot_entry64_efi.
	 * AMD64 SysV ABI: args in rdi, rsi.
	 */
	movl	boot_saved_magic(%rip), %edi	/* arg1: magic */
	movl	boot_saved_info(%rip),  %esi	/* arg2: info_ptr */

	/*
	 * Call pre_init(u32_t magic, u32_t info_ptr).
	 */
	call	_C_LABEL(pre_init)

	/*
	 * pre_init() returns kinfo_t * in rax.
	 * Switch to the highly-mapped kernel stack and call kmain.
	 */
	movq	$k_initial_stktop, %rsp
	xorq	%rbp, %rbp
	movq	%rax, %rdi		/* kinfo_t * as first arg */

	/*
	 * Can't use a direct call: 32-bit PC-relative displacement from
	 * physical ~0x00400000 to virtual ~0xFFFFFFFF80400000 overflows.
	 */
	movabsq	$_C_LABEL(kmain), %rax
	callq	*%rax

.Lhang:
	hlt
	jmp	.Lhang

/*===========================================================================*/
/*  Boot stack (placed in .data so it lands in the unpaged section)         */
/*===========================================================================*/
.data
.balign 4096
boot_stack64:
	.space	4096
boot_stack64_top:

/*===========================================================================*/
/*  Boot page tables — .bss (zero-initialised, unpaged section)             */
/*===========================================================================*/
.bss
.balign 4096
boot_pml4:
	.space	4096
.balign 4096
boot_pdpt_low:
	.space	4096
.balign 4096
boot_pdpt_high:
	.space	4096
.balign 4096
boot_pd_low:
	.space	4096 * 4		/* 4 PDs: identity-map low 4 GB */
.balign 4096
boot_pd_kern:
	.space	4096

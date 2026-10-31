/*	$NetBSD: multiboot32.S,v 1.1.2.3 2019/09/27 09:20:16 martin Exp $ */

#include <machine/asm.h>
#include <machine/specialreg.h>

/*
 * multiboot32(entry 4(%esp), multiboot2_info 8(%esp), magic 12(%esp))
 *
 * IA32 UEFI firmware hands us control with PAE paging enabled (NX
 * active), but the multiboot2 machine state requires paging disabled.
 * A kernel that builds legacy 2-level page tables would have its page
 * directory read as a PAE PDPT the moment it loads %cr3 with CR4.PAE
 * still set, and triple-fault on the reserved bits.  Normalize to the
 * multiboot machine state (mirroring what GRUB provides on BIOS boot)
 * before jumping; this code runs identity-mapped, so paging can be
 * switched off in place.
 */
ENTRY(multiboot32_start)
start:
	cli
	movl	12(%esp),%eax
	movl	8(%esp),%ebx
	movl	4(%esp),%edx
	movl	%cr0,%ecx
	andl	$~(CR0_PG|CR0_WP),%ecx
	movl	%ecx,%cr0
	movl	%cr4,%ecx
	andl	$~(CR4_PAE|CR4_PGE|CR4_PSE),%ecx
	movl	%ecx,%cr4
	jmp	*%edx
multiboot32_end:

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
#include <machine/multiboot.h>

#include "arch_proto.h" /* K_STACK_SIZE */

#ifdef CONFIG_SMP
#include "kernel/smp.h"
#endif

/* Selected 386 tss offsets. */
#define TSS3_S_SP0	4

IMPORT(copr_not_available_handler)
IMPORT(params_size)
IMPORT(params_offset)
IMPORT(mon_ds)
IMPORT(switch_to_user)
IMPORT(multiboot_init)

.text
/*===========================================================================*/
/*				MINIX				     */
/*===========================================================================*/
.global MINIX
MINIX:
/* this is the entry point for the MINIX kernel */
	jmp multiboot_init

/* Multiboot header here*/

.balign 8

#define MULTIBOOT_FLAGS (MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN)

multiboot_magic:
	.long MULTIBOOT_HEADER_MAGIC
multiboot_flags:
	.long MULTIBOOT_FLAGS
multiboot_checksum:
	.long -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_FLAGS)
	.long 0
	.long 0
	.long 0
	.long 0
	.long 0
/* Video mode */
multiboot_mode_type:
	.long MULTIBOOT_VIDEO_MODE_EGA
multiboot_width:
	.long MULTIBOOT_CONSOLE_COLS
multiboot_height:
	.long MULTIBOOT_CONSOLE_LINES
multiboot_depth:
	.long 0

multiboot_init:
	mov	$load_stack_start, %esp	/* make usable stack */
	mov	$0, %ebp
	push	$0	/* set flags to known good state */
	popf	/* esp, clear nested task and int enable */
	push	$0

        push    %ebx	/* multiboot information struct */
	push	%eax	/* multiboot magic number */
        call    _C_LABEL(pre_init)

	/* Kernel is mapped high now and ready to go, with
	 * the boot info pointer returnd in %eax. Set the
	 * highly mapped stack, initialize it, push the boot
	 * info pointer and jump to the highly mapped kernel.
	 */
	mov	$k_initial_stktop, %esp
	push	$0	/* Terminate stack */
	push	%eax
        call    _C_LABEL(kmain)

	/* not reached */
hang:
	jmp hang

.data
load_stack:
	.space 4096
load_stack_start:

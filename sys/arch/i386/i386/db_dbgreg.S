/*	$NetBSD: db_dbgreg.S,v 1.7 2007/12/11 23:31:07 lukem Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1995 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* LINTSTUB: Empty */

#include <i386/include/asm.h>
__KERNEL_RCSID(0, "$NetBSD: db_dbgreg.S,v 1.7 2007/12/11 23:31:07 lukem Exp $");

#define S_ARG0	 4(%esp)
#define S_ARG1	 8(%esp)
#define S_ARG2	12(%esp)
#define S_ARG3	16(%esp)

#define B_ARG0	 8(%ebp)
#define B_ARG1	12(%ebp)
#define B_ARG2	16(%ebp)
#define B_ARG3	20(%ebp)

ENTRY(dr6)
	movl	%db6, %eax
	ret

/*	dr<i>(address, type, len, persistence)
 *	type:
 *	   00	execution (use len 00)
 *	   01	data write
 *	   11	data read/write
 *	len:
 *	   00	one byte
 *	   01	two bytes
 *	   11	four bytes
 */
ENTRY(dr0)
	movl	S_ARG0, %eax
	movl	%eax,_C_LABEL(dr_addr)
	movl	%eax, %db0
	movl	$0, %ecx
	jmp	0f
ENTRY(dr1)
	movl	S_ARG0, %eax
	movl	%eax,_C_LABEL(dr_addr)+1*4
	movl	%eax, %db1
	movl	$2, %ecx
	jmp	0f
ENTRY(dr2)
	movl	S_ARG0, %eax
	movl	%eax,_C_LABEL(dr_addr)+2*4
	movl	%eax, %db2
	movl	$4, %ecx
	jmp	0f

ENTRY(dr3)
	movl	S_ARG0, %eax
	movl	%eax,_C_LABEL(dr_addr)+3*4
	movl	%eax, %db3
	movl	$6, %ecx

0:
	pushl	%ebp
	movl	%esp, %ebp

	movl	%db7, %edx
	movl	%edx,_C_LABEL(dr_addr)+4*4
	andl	dr_msk(,%ecx,2),%edx	/* clear out new entry */
	movl	%edx,_C_LABEL(dr_addr)+5*4
	movzbl	B_ARG3, %eax
	andb	$3, %al
	shll	%cl, %eax
	orl	%eax, %edx

	shll	$1, %ecx

	movzbl	B_ARG1, %eax
	andb	$3, %al
	addb	$0x10, %cl
	shll	%cl, %eax
	orl	%eax, %edx

	movzbl	B_ARG2, %eax
	andb	$3, %al
	addb	$0x2, %cl
	shll	%cl, %eax
	orl	%eax, %edx

	movl	%edx, %db7
	movl	%edx,_C_LABEL(dr_addr)+7*4
	movl	%edx, %eax
	leave
	ret

	.data
dr_msk:
	.long	~0x000f0003
	.long	~0x00f0000c
	.long	~0x0f000030
	/* XXX
	 * The following should be ~0xf00000c0 but that confuses gas
	 * when cross-compiling i386 from amd64.
	 */
	.long	0x0fffff3f
_C_LABEL(dr_addr):
	.long	0,0,0,0
	.long	0,0,0,0
	.text

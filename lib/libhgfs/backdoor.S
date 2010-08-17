/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */
#include <machine/asm.h>

.text

	MAGIC = 0x564D5868
	BD_PORT = 0x5658
	IO_PORT = 0x5659

ENTRY(__libhgfs_backdoor)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	movl	4+16(%esp), %ebp
	movl	$MAGIC, %eax
	movl	4(%ebp), %ebx
	movl	8(%ebp), %ecx
	movl	12(%ebp), %edx
	movw	$BD_PORT, %dx
	movl	16(%ebp), %esi
	movl	20(%ebp), %edi
	inl	%dx
	movl	%eax, (%ebp)
	movl	%ebx, 4(%ebp)
	movl	%ecx, 8(%ebp)
	movl	%edx, 12(%ebp)
	movl	%esi, 16(%ebp)
	movl	%edi, 20(%ebp)
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

ENTRY(__libhgfs_backdoor_in)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	movl	4+16(%esp), %eax
	movl	4(%eax), %ebx
	movl	8(%eax), %ecx
	movl	12(%eax), %edx
	movw	$IO_PORT, %dx
	movl	16(%eax), %esi
	movl	20(%eax), %edi
	movl	24(%eax), %ebp
	movl	$MAGIC, %eax
	cld
	repe; insb
	pushl	%eax
	movl	4+20(%esp), %eax
	movl	%ebx, 4(%eax)
	movl	%ecx, 8(%eax)
	movl	%edx, 12(%eax)
	movl	%esi, 16(%eax)
	movl	%edi, 20(%eax)
	movl	%ebp, 24(%eax)
	popl	%ebx
	movl	%ebx, (%eax)
	movl	(%eax), %eax
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

ENTRY(__libhgfs_backdoor_out)
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	movl	4+16(%esp), %eax
	movl	4(%eax), %ebx
	movl	8(%eax), %ecx
	movl	12(%eax), %edx
	movw	$IO_PORT, %dx
	movl	16(%eax), %esi
	movl	20(%eax), %edi
	movl	24(%eax), %ebp
	movl	$MAGIC, %eax
	cld
	repe; outsb
	pushl	%eax
	movl	4+20(%esp), %eax
	movl	%ebx, 4(%eax)
	movl	%ecx, 8(%eax)
	movl	%edx, 12(%eax)
	movl	%esi, 16(%eax)
	movl	%edi, 20(%eax)
	movl	%ebp, 24(%eax)
	popl	%ebx
	movl	%ebx, (%eax)
	movl	(%eax), %eax
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ret

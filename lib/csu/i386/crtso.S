/* This is the C run-time start-off routine.  It's job is to take the */
/* arguments as put on the stack by EXEC, and to parse them and set them up the */
/* way _main expects them. */
/* It also initializes environ when this variable isn't defined by the */
/* programmer.  The detection of whether environ belong to us is rather */
/* simplistic.  We simply check for some magic value, but there is no other */
/* way. */

#include <machine/vm.h>


.globl	begtext, begdata, begbss
.text
begtext:
#ifdef __ACK__
.rom
#else
.data
#endif
begrom:
.data
begdata:
.bss
begbss:

#ifdef __ACK__
.globl __minix_datastart
#endif

.globl	crtso, __penviron, __penvp, ___prognamep, ___argc
.globl __minix_mainjump, __minix_unmapzero
.extern	_main, _exit

#if defined(__ELF__)
.section .init
#else
.text
#endif

#if defined(__ELF__)
.globl __start
__start:
#endif
crtso:
	xorl	%ebp, %ebp	/* clear for backtrace of core files */
	movl	(%esp), %eax	/* argc */
	leal	4(%esp), %edx	/* argv */
	leal	8(%esp,%eax,4), %ecx	/* envp */

/* Test if environ is in the initialized data area and is set to our */
/* magic number.  If so then it is not redefined by the user. */
	movl	$_environ, %ebx
	cmpl	$__edata, %ebx	/* within initialized data? */
	jae	0f
	testb	$3, %bl	/* aligned? */
	jne	0f
	cmpl	$0x53535353, (%ebx)	/* is it our environ? */
	jne	0f
	movl	%ebx, __penviron	/* _penviron = &environ; */
0:
	movl	__penviron, %ebx
	movl	%ecx, (%ebx)	/* *_penviron = envp; */

	/* Save argv[] and argc. This is so that we can return
	 * argv[0] in the future, without having to check whether
	 * argv can be dereferenced safely now.
	 */
	mov	%edx, (___prognamep)
	mov	%eax, (___argc)

	push	%ecx	/* push envp */
	push	%edx	/* push argv */
	push	%eax	/* push argc */

	jmp	__minix_mainjump

.balign I386_PAGE_SIZE
__minix_mainjump:
	/* unmap zero pages */
	call	__minix_unmapzero

	call	_main	/* main(argc, argv, envp) */

	push	%eax	/* push exit status */
	call	_exit

	hlt	/* force a trap if exit fails */

__minix_unmapzero:

	/* unmap 0-page code */
	push	$I386_PAGE_SIZE
	push	$crtso
	call	_munmap_text		/* unmap_text(crtso, I386_PAGE_SIZE) */
	add	$8, %esp

#ifdef __ACK__
	/*
	 * ack uses separate segments for text and data by default. We have a
	 * common segment when compiling using any other compiler
	 */

	/* unmap 0-page data */
	push	$I386_PAGE_SIZE
	push	$romstart
	call	_munmap			/* munmap(romstart, I386_PAGE_SIZE) */
	add	$8, %esp
#endif

	ret

#ifdef __ACK__
.rom
romstart:
.space	I386_PAGE_SIZE
__minix_datastart:
.space	4
#endif
.data
__penviron:
.long	__penvp	/* Pointer to environ, or hidden pointer */
.bss
___prognamep:
.space	4
___argc:
.space	4
.lcomm	__penvp, 4	/* Hidden environment vector */

.extern	endtext	/* Force loading of end labels. */

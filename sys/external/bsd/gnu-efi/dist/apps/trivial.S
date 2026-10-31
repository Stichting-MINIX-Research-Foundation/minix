/*	$NetBSD: trivial.S,v 1.1.1.1 2014/04/01 16:16:06 jakllsch Exp $	*/

	.text
	.align 4

	.globl _start
_start:
#if 0
        pushl %ebp
        movl %esp,%ebp
        pushl %ebx		# save ebx
        movl 12(%ebp),%eax	# eax <- systab
	movl 24(%eax),%ebx	# ebx <- systab->FirmwareVendor
	pushl %ebx
        movl 44(%eax),%ebx	# ebx <- systab->ConOut
        pushl %ebx
        movl 4(%ebx),%eax	# eax <- conout->OutputString
        call *%eax
        movl -4(%ebp),%ebx	# restore ebx
        leave
        ret

#else

        pushl %ebp
        movl %esp,%ebp
        pushl %ebx
	call 0f
0:	popl %eax
	addl $hello-0b,%eax
	pushl %eax
        movl 12(%ebp),%eax	# eax <- systab
        movl 44(%eax),%ebx	# ebx <- systab->ConOut
        pushl %ebx
        movl 4(%ebx),%eax	# eax <- conout->OutputString
        call *%eax
        movl -4(%ebp),%ebx
        leave
        ret

	.section .rodata
	.align 2
hello:	.byte 'h',0,'e',0,'l',0,'l',0,'o',0,'\n',0,'\r',0,0,0

#endif

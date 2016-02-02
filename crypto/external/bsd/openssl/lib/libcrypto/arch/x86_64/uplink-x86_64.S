#include <machine/asm.h>
.text	

.globl	OPENSSL_UplinkTable
.type	_lazy1,@function
.align	16
_lazy1:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$1,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*8(%rax)
_lazy1_end:
.size	_lazy1,.-_lazy1
.type	_lazy2,@function
.align	16
_lazy2:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$2,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*16(%rax)
_lazy2_end:
.size	_lazy2,.-_lazy2
.type	_lazy3,@function
.align	16
_lazy3:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$3,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*24(%rax)
_lazy3_end:
.size	_lazy3,.-_lazy3
.type	_lazy4,@function
.align	16
_lazy4:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$4,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*32(%rax)
_lazy4_end:
.size	_lazy4,.-_lazy4
.type	_lazy5,@function
.align	16
_lazy5:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$5,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*40(%rax)
_lazy5_end:
.size	_lazy5,.-_lazy5
.type	_lazy6,@function
.align	16
_lazy6:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$6,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*48(%rax)
_lazy6_end:
.size	_lazy6,.-_lazy6
.type	_lazy7,@function
.align	16
_lazy7:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$7,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*56(%rax)
_lazy7_end:
.size	_lazy7,.-_lazy7
.type	_lazy8,@function
.align	16
_lazy8:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$8,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*64(%rax)
_lazy8_end:
.size	_lazy8,.-_lazy8
.type	_lazy9,@function
.align	16
_lazy9:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$9,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*72(%rax)
_lazy9_end:
.size	_lazy9,.-_lazy9
.type	_lazy10,@function
.align	16
_lazy10:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$10,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*80(%rax)
_lazy10_end:
.size	_lazy10,.-_lazy10
.type	_lazy11,@function
.align	16
_lazy11:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$11,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*88(%rax)
_lazy11_end:
.size	_lazy11,.-_lazy11
.type	_lazy12,@function
.align	16
_lazy12:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$12,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*96(%rax)
_lazy12_end:
.size	_lazy12,.-_lazy12
.type	_lazy13,@function
.align	16
_lazy13:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$13,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*104(%rax)
_lazy13_end:
.size	_lazy13,.-_lazy13
.type	_lazy14,@function
.align	16
_lazy14:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$14,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*112(%rax)
_lazy14_end:
.size	_lazy14,.-_lazy14
.type	_lazy15,@function
.align	16
_lazy15:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$15,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*120(%rax)
_lazy15_end:
.size	_lazy15,.-_lazy15
.type	_lazy16,@function
.align	16
_lazy16:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$16,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*128(%rax)
_lazy16_end:
.size	_lazy16,.-_lazy16
.type	_lazy17,@function
.align	16
_lazy17:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$17,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*136(%rax)
_lazy17_end:
.size	_lazy17,.-_lazy17
.type	_lazy18,@function
.align	16
_lazy18:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$18,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*144(%rax)
_lazy18_end:
.size	_lazy18,.-_lazy18
.type	_lazy19,@function
.align	16
_lazy19:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$19,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*152(%rax)
_lazy19_end:
.size	_lazy19,.-_lazy19
.type	_lazy20,@function
.align	16
_lazy20:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$20,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*160(%rax)
_lazy20_end:
.size	_lazy20,.-_lazy20
.type	_lazy21,@function
.align	16
_lazy21:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$21,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*168(%rax)
_lazy21_end:
.size	_lazy21,.-_lazy21
.type	_lazy22,@function
.align	16
_lazy22:
.byte	0x48,0x83,0xEC,0x28	
	movq	%rcx,48(%rsp)
	movq	%rdx,56(%rsp)
	movq	%r8,64(%rsp)
	movq	%r9,72(%rsp)
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rcx
	movq	$22,%rdx
	call	OPENSSL_Uplink
	movq	48(%rsp),%rcx
	movq	56(%rsp),%rdx
	movq	64(%rsp),%r8
	movq	72(%rsp),%r9
	leaq	OPENSSL_UplinkTable@GOTPCREL(%rip),%rax
	addq	$40,%rsp
	jmp	*176(%rax)
_lazy22_end:
.size	_lazy22,.-_lazy22
.data	
OPENSSL_UplinkTable:
.quad	22
.quad	_lazy1
.quad	_lazy2
.quad	_lazy3
.quad	_lazy4
.quad	_lazy5
.quad	_lazy6
.quad	_lazy7
.quad	_lazy8
.quad	_lazy9
.quad	_lazy10
.quad	_lazy11
.quad	_lazy12
.quad	_lazy13
.quad	_lazy14
.quad	_lazy15
.quad	_lazy16
.quad	_lazy17
.quad	_lazy18
.quad	_lazy19
.quad	_lazy20
.quad	_lazy21
.quad	_lazy22
.section	.pdata,"r"
.align	4
.rva	_lazy1,_lazy1_end,_lazy_unwind_info
.rva	_lazy2,_lazy2_end,_lazy_unwind_info
.rva	_lazy3,_lazy3_end,_lazy_unwind_info
.rva	_lazy4,_lazy4_end,_lazy_unwind_info
.rva	_lazy5,_lazy5_end,_lazy_unwind_info
.rva	_lazy6,_lazy6_end,_lazy_unwind_info
.rva	_lazy7,_lazy7_end,_lazy_unwind_info
.rva	_lazy8,_lazy8_end,_lazy_unwind_info
.rva	_lazy9,_lazy9_end,_lazy_unwind_info
.rva	_lazy10,_lazy10_end,_lazy_unwind_info
.rva	_lazy11,_lazy11_end,_lazy_unwind_info
.rva	_lazy12,_lazy12_end,_lazy_unwind_info
.rva	_lazy13,_lazy13_end,_lazy_unwind_info
.rva	_lazy14,_lazy14_end,_lazy_unwind_info
.rva	_lazy15,_lazy15_end,_lazy_unwind_info
.rva	_lazy16,_lazy16_end,_lazy_unwind_info
.rva	_lazy17,_lazy17_end,_lazy_unwind_info
.rva	_lazy18,_lazy18_end,_lazy_unwind_info
.rva	_lazy19,_lazy19_end,_lazy_unwind_info
.rva	_lazy20,_lazy20_end,_lazy_unwind_info
.rva	_lazy21,_lazy21_end,_lazy_unwind_info
.rva	_lazy22,_lazy22_end,_lazy_unwind_info
.section	.xdata,"r"
.align	8
_lazy_unwind_info:
.byte	0x01,0x04,0x01,0x00
.byte	0x04,0x42,0x00,0x00

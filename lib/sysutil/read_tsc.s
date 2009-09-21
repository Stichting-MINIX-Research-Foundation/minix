# 
! sections

.sect .text; .sect .rom; .sect .data; .sect .bss

.define	_read_tsc	! read the cycle counter (Pentium and up)

.sect .text
!*===========================================================================*
! PUBLIC void read_tsc(unsigned long *high, unsigned long *low);
! Read the cycle counter of the CPU. Pentium and up. 
.align 16
_read_tsc:
	push edx
	push eax
.data1 0x0f		! this is the RDTSC instruction 
.data1 0x31		! it places the TSC in EDX:EAX
	push ebp
	mov ebp, 16(esp)
	mov (ebp), edx
	mov ebp, 20(esp)
	mov (ebp), eax
	pop ebp
	pop eax
	pop edx
	ret

!*===========================================================================*
! PUBLIC void read_host_time_ns(unsigned long *high, unsigned long *low);
! access real time in ns from host in vmware.
.align 16
_read_host_time_ns:
	push edx
	push eax
	push ecx
	mov ecx, 0x10001
.data1 0x0f		! this is the RDPMC instruction 
.data1 0x33		! it places the result in EDX:EAX
	push ebp
	mov ebp, 20(esp)
	mov (ebp), edx
	mov ebp, 24(esp)
	mov (ebp), eax
	pop ebp
	pop ecx
	pop eax
	pop edx
	ret


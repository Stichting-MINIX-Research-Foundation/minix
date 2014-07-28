
.global libvassert_process_backdoor 

libvassert_process_backdoor:
	push   %ebx
	push   %esi
	mov    0xc(%esp),%ecx
	mov    0x14(%esp),%edx
	mov    0x10(%esp),%ebx
	mov    $0x564d5868,%eax
	out    %eax,(%dx)
	mov    0x18(%esp),%esi
	mov    %eax,(%esi)
	mov    0x20(%esp),%eax
	mov    %ecx,(%eax)
	mov    0x24(%esp),%eax
	mov    %edx,(%eax)
	mov    0x1c(%esp),%eax
	mov    %ebx,(%eax)
	pop    %esi
	pop    %ebx
	ret

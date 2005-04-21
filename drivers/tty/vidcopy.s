# 
! This file contains two specialized assembly code routines to update the 
! video memory. The routines can copy from user to video memory, or from
! video to video memory.   

! sections

.sect .text; .sect .rom; .sect .data; .sect .bss

! exported functions

.define	_mem_vid_copy	! copy data to video ram
.define	_vid_vid_copy	! move data in video ram

! The routines only guarantee to preserve the registers the C compiler
! expects to be preserved (ebx, esi, edi, ebp, esp, segment registers, and
! direction bit in the flags).

.sect .text
!*===========================================================================*
!*				mem_vid_copy				     *
!*===========================================================================*
! PUBLIC void mem_vid_copy(u16 *src, unsigned dst, unsigned count);
!
! Copy count characters from kernel memory to video memory.  Src is an ordinary
! pointer to a word, but dst and count are character (word) based video offset
! and count.  If src is null then screen memory is blanked by filling it with
! blank_color.

_mem_vid_copy:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	push	es
	mov	esi, 8(ebp)		! source
	mov	edi, 12(ebp)		! destination
	mov	edx, 16(ebp)		! count
	mov	es, (_vid_seg)		! segment containing video memory
	cld				! make sure direction is up
mvc_loop:
	and	edi, (_vid_mask)	! wrap address
	mov	ecx, edx		! one chunk to copy
	mov	eax, (_vid_size)
	sub	eax, edi
	cmp	ecx, eax
	jbe	0f
	mov	ecx, eax		! ecx = min(ecx, vid_size - edi)
0:	sub	edx, ecx		! count -= ecx
	shl	edi, 1			! byte address
	add	edi, (_vid_off)		! in video memory
	test	esi, esi		! source == 0 means blank the screen
	jz	mvc_blank
mvc_copy:
	rep				! copy words to video memory
    o16	movs
	jmp	mvc_test
mvc_blank:
	mov	eax, (_blank_color)	! ax = blanking character
	rep
    o16	stos				! copy blanks to video memory
	!jmp	mvc_test
mvc_test:
	sub	edi, (_vid_off)
	shr	edi, 1			! back to a word address
	test	edx, edx
	jnz	mvc_loop
mvc_done:
	pop	es
	pop	edi
	pop	esi
	pop	ebp
	ret


!*===========================================================================*
!*				vid_vid_copy				     *
!*===========================================================================*
! PUBLIC void vid_vid_copy(unsigned src, unsigned dst, unsigned count);
!
! Copy count characters from video memory to video memory.  Handle overlap.
! Used for scrolling, line or character insertion and deletion.  Src, dst
! and count are character (word) based video offsets and count.

_vid_vid_copy:
	push	ebp
	mov	ebp, esp
	push	esi
	push	edi
	push	es
	mov	esi, 8(ebp)		! source
	mov	edi, 12(ebp)		! destination
	mov	edx, 16(ebp)		! count
	mov	es, (_vid_seg)		! segment containing video memory
	cmp	esi, edi		! copy up or down?
	jb	vvc_down
vvc_up:
	cld				! direction is up
vvc_uploop:
	and	esi, (_vid_mask)	! wrap addresses
	and	edi, (_vid_mask)
	mov	ecx, edx		! one chunk to copy
	mov	eax, (_vid_size)
	sub	eax, esi
	cmp	ecx, eax
	jbe	0f
	mov	ecx, eax		! ecx = min(ecx, vid_size - esi)
0:	mov	eax, (_vid_size)
	sub	eax, edi
	cmp	ecx, eax
	jbe	0f
	mov	ecx, eax		! ecx = min(ecx, vid_size - edi)
0:	sub	edx, ecx		! count -= ecx
	call	vvc_copy		! copy video words
	test	edx, edx
	jnz	vvc_uploop		! again?
	jmp	vvc_done
vvc_down:
	std				! direction is down
	lea	esi, -1(esi)(edx*1)	! start copying at the top
	lea	edi, -1(edi)(edx*1)
vvc_downloop:
	and	esi, (_vid_mask)	! wrap addresses
	and	edi, (_vid_mask)
	mov	ecx, edx		! one chunk to copy
	lea	eax, 1(esi)
	cmp	ecx, eax
	jbe	0f
	mov	ecx, eax		! ecx = min(ecx, esi + 1)
0:	lea	eax, 1(edi)
	cmp	ecx, eax
	jbe	0f
	mov	ecx, eax		! ecx = min(ecx, edi + 1)
0:	sub	edx, ecx		! count -= ecx
	call	vvc_copy
	test	edx, edx
	jnz	vvc_downloop		! again?
	cld				! C compiler expects up
	!jmp	vvc_done
vvc_done:
	pop	es
	pop	edi
	pop	esi
	pop	ebp
	ret

! Copy video words.  (Inner code of both the up and downcopying loop.)
vvc_copy:
	shl	esi, 1
	shl	edi, 1			! byte addresses
	add	esi, (_vid_off)
	add	edi, (_vid_off)		! in video memory
	rep
eseg o16 movs				! copy video words
	sub	esi, (_vid_off)
	sub	edi, (_vid_off)
	shr	esi, 1
	shr	edi, 1			! back to word addresses
	ret


#
;
; (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
; See the copyright notice in the ACK home directory, in the file "Copyright".
;

;
; Module:	coroutine primitives
; Author:	Kees Bot, Edwin Scheffer, Ceriel Jacobs
; Version:	$Header$
;

 mes 2,_EM_WSIZE,_EM_PSIZE

 ; topsize takes care of two things:
 ; - given a stack-break,
 ;   it computes the size of the chunk of memory needed to save the stack;
 ; - also, if this stack-break = 0, it creates one, assuming that caller is
 ;   the stack-break.
 ;
 ; This implementation assumes a continuous stack growing downwards

 exp $topsize
#ifdef __sparc
 inp $topsize2
 pro $topsize, 0
 mes 11
 zer _EM_PSIZE
 lal 0
 loi _EM_PSIZE
 cal $topsize2
 asp 2*_EM_PSIZE
 lfr _EM_WSIZE
 ret _EM_WSIZE
 end 0
 pro $topsize2, (3*_EM_WSIZE+3*_EM_PSIZE)
#else
 pro $topsize, (3*_EM_WSIZE+3*_EM_PSIZE)
#endif
 ; local space for line-number, ignoremask, filename, stack-break, size,
 ; and stack-pointer (see the topsave routine)
 mes 11
 lal 0
 loi _EM_PSIZE
 loi _EM_PSIZE		; stack-break or 0
 zer _EM_PSIZE
 cmp
 zne *1
 lxl 0
 dch			; local base of caller
#ifdef __sparc
 dch			; because of the extra layer
#endif
 lal 0
 loi _EM_PSIZE
 sti _EM_PSIZE
1
 lal 0
 loi _EM_PSIZE
 loi _EM_PSIZE
 lpb			; convert this local base to an argument base.
			; An implementation of a sort of "topsize" EM
			; instruction should take a local base, and save
			; the whole frame.

 lor 1			; stack-break  SP
 sbs _EM_WSIZE		; stack-break-SP
 ret _EM_WSIZE		; return size of block to be saved
 end 3*_EM_WSIZE+3*_EM_PSIZE

 exp $topsave
#ifdef __sparc
 inp $topsave2
 pro $topsave,0
 mes 11
 lal 0
 loi 2*_EM_PSIZE
 cal $topsave2
 asp 2*_EM_PSIZE
 lfr _EM_WSIZE
 ret _EM_WSIZE
 end 0
 pro $topsave2,0
#else
 pro $topsave, 0
#endif
 mes 11
 loe 0
 lae 4			; load line number and file name
 loi _EM_PSIZE
 lim			; ignore mask
 lor 0			; LB
 lal 0
 loi _EM_PSIZE		; stack-break
 lpb
 lor 1
 sbs _EM_WSIZE
 loc _EM_WSIZE
 adu _EM_WSIZE		; gives size
 dup _EM_WSIZE
 stl 0			; save size
 lor 1			; SP (the SP BEFORE pushing)
 lor 1			; SP (address of stack top to save)
 lal _EM_PSIZE		; area
 loi _EM_PSIZE
 lol 0			; size
 bls _EM_WSIZE		; move whole block
 asp 3*_EM_PSIZE+3*_EM_WSIZE	; remove the lot from the stack
 loc 1
 ret _EM_WSIZE			; return 1
 end 0

sv
 bss _EM_PSIZE,0,0

 exp $topload
#ifdef __sparc
 inp $topload1
 pro $topload,0
 lal 0
 loi _EM_PSIZE
 cal $topload1
 asp _EM_PSIZE
 lfr _EM_WSIZE
 ret _EM_WSIZE
 end 0
 pro $topload1, 0
#else
 pro $topload, 0
#endif
 mes 11

 lal 0
 loi _EM_PSIZE
 lae sv
 sti _EM_PSIZE		; saved parameter

 lxl 0
2
 dup _EM_PSIZE
 adp -3*_EM_PSIZE
 lal 0
 loi _EM_PSIZE		; compare target SP with current LB to see if we must
 loi _EM_PSIZE
 cmp			; find another LB first
 zgt *1
 dch			; just follow dynamic chain to make sure we find
			; a legal one
 bra *2
1
 str 0

 lae sv
 loi _EM_PSIZE
 loi _EM_PSIZE		; load indirect to
 str 1			; restore SP
 asp 0-_EM_PSIZE	; to stop int from complaining about non-existent memory
 lae sv
 loi _EM_PSIZE		; source address
 lor 1
 adp _EM_PSIZE		; destination address
 lae sv
 loi _EM_PSIZE
 adp _EM_PSIZE
 loi _EM_WSIZE		; size of block
 bls _EM_WSIZE
 asp _EM_PSIZE+_EM_WSIZE	; drop size + SP
 str 0			; restore local base
 sim			; ignore mask
 lae 4
 sti _EM_PSIZE
 ste 0			; line and file
 loc 0
 ret _EM_WSIZE
 end 0

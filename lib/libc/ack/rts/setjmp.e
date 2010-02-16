#
 mes 2,_EM_WSIZE,_EM_PSIZE
;
; layout of a setjmp buffer:
;
;  -----------------
; |      flag       |		(!0 when blocked signals saved (POSIX))
;  -----------------
; | signal mask/set |		(for Berkeley 4.[2-] / POSIX)
;  -----------------
; |                 |
; |  GTO descriptor |
; |   (SP, LB, PC)  |
; |                 |
;  -----------------
;
; setjmp saves the signalmask, PC, SP, and LB of caller, and creates a
; GTO descriptor from this.
; The big problem here is how to get the return address, i.e. the PC of
; the caller; This problem is solved by the front-end, which must pass
; it as an extra parameter to setjmp.

; a GTO descriptor must be in the global data area
gtobuf
 bss 3*_EM_PSIZE,0,0

 inp $fill_ret_area
 exp $__setjmp
 pro $__setjmp,0
#if	defined(_POSIX_SOURCE)
; save mask of currently blocked signals. 
; longjmp must restore this mask
 lol _EM_PSIZE			; the flag integer at offset _EM_PSIZE
 lal 0
 loi _EM_PSIZE
 stf 3*_EM_PSIZE+_EM_LSIZE
 lol _EM_PSIZE			; the flag integer at offset _EM_PSIZE
 zeq *1
 lal 0
 loi _EM_PSIZE
 adp 3*_EM_PSIZE
 cal $__newsigset
 asp _EM_PSIZE
1
#elif	defined(__BSD4_2)
 loc 0
 cal $sigblock
 asp _EM_WSIZE
 lfr _EM_WSIZE
 lal 0
 loi _EM_PSIZE
 stf 3*_EM_PSIZE
#endif
; create GTO descriptor for longjmp
 lxl 0
 dch		; Local Base of caller
 lxa 0		; Stackpointer of caller
 lal _EM_PSIZE+_EM_WSIZE
 loi _EM_PSIZE	; Return address of caller
 lal 0
 loi _EM_PSIZE	; address of jmpbuf
 sti 3*_EM_PSIZE	; LB, SP, and PC stored in jmpbuf
 loc 0
 ret _EM_WSIZE	; setjmp must return 0
 end 0

 pro $fill_ret_area,0
; put argument in function result area
 lol 0
 ret _EM_WSIZE
 end 0

 exp $longjmp
 pro $longjmp,?
#if	defined(_POSIX_SOURCE)
; restore blocked mask
 lal 0
 loi _EM_PSIZE
 lof 3*_EM_PSIZE+_EM_LSIZE
 zeq *2
 lal 0
 loi _EM_PSIZE
 adp 3*_EM_PSIZE
 cal $__oldsigset
 asp _EM_PSIZE
2
#elif	defined(__BSD4_2)
; restore signal mask
 lal 0
 loi _EM_PSIZE
 lof 3*_EM_PSIZE
 cal $_sigsetmask
 asp _EM_WSIZE
 lfr _EM_WSIZE
 asp _EM_WSIZE
#endif
 lal 0
 loi _EM_PSIZE	; address of jmpbuf
 lae gtobuf
 blm 3*_EM_PSIZE	; fill GTO descriptor from jmpbuf
 lol _EM_PSIZE	; second parameter of longjmp: the return value
 dup _EM_WSIZE
 zne *3
; of course, longjmp may not return 0!
 inc
3
; put return value in function result area
 cal $fill_ret_area
 asp _EM_WSIZE
 gto gtobuf	; there we go ...
; ASP and GTO do not damage function result area
 end 0

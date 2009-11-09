#
;
; (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
; See the copyright notice in the ACK home directory, in the file "Copyright".
;
;
; Module:	Modula-2 runtime startoff
; Author:	Ceriel J.H. Jacobs
; Version:	$Header$
;

 mes 2,_EM_WSIZE,_EM_PSIZE

 exa handler
 exa argv
 exa argc
 exa MainLB
 exa bkillbss
 exp $catch
 exp $init
 inp $trap_handler

bkillbss
 bss _EM_PSIZE,0,0

 exp $_m_a_i_n
 pro $_m_a_i_n, 0

 lor 0
 lae MainLB
 sti _EM_PSIZE

 lal _EM_WSIZE
 loi _EM_PSIZE
 lae argv		; save argument pointer
 sti _EM_PSIZE

 lol 0
 ste argc		; save argument count

 lpi $trap_handler
 sig
 asp _EM_PSIZE
 cal $init
 cal $__M2M_
 cal $halt
 loc 0			; should not get here
 ret _EM_WSIZE
 end

 pro $trap_handler,0
 lpi $trap_handler
 sig
 lol 0	; trap number
 lae handler
 loi _EM_PSIZE
 lpi $catch
 lae handler
 sti _EM_PSIZE
 cai
 asp _EM_PSIZE+_EM_WSIZE
 rtt
 end 0

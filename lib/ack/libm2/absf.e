#
;
; (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
; See the copyright notice in the ACK home directory, in the file "Copyright".
;
;
; Module:	REAL abs function
; Author:	Ceriel J.H. Jacobs
; Version:	$Header$
;
 mes 2,_EM_WSIZE,_EM_PSIZE
 exp $absf
 pro $absf,0
 mes 5
 mes 9,8
 lal 0
 loi _EM_FSIZE
 zrf _EM_FSIZE
 cmf _EM_FSIZE
 zlt *3
 lal 0
 loi _EM_FSIZE
 bra *4
3
 lal 0
 loi _EM_FSIZE
 ngf _EM_FSIZE
4
 ret _EM_FSIZE
 end 0

#
; $Header$
;
; (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
;
;          This product is part of the Amsterdam Compiler Kit.
;
; Permission to use, sell, duplicate or disclose this software must be
; obtained in writing. Requests for such permissions may be sent to
;
;      Dr. Andrew S. Tanenbaum
;      Wiskundig Seminarium
;      Vrije Universiteit
;      Postbox 7161
;      1007 MC Amsterdam
;      The Netherlands
;
;

 mes 2,_EM_WSIZE,_EM_PSIZE

#define FARG    0
#define ERES    _EM_DSIZE

; _fef is called with two parameters:
;       - address of exponent result (ERES)
;       - floating point number to be split (FARG)
; and returns an _EM_DSIZE-byte floating point number

 exp $_fef
 pro $_fef,0
 lal FARG
 loi _EM_DSIZE
 fef _EM_DSIZE
 lal ERES
 loi _EM_PSIZE
 sti _EM_WSIZE
 ret _EM_DSIZE
 end ?

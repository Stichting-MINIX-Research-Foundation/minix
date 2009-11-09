#define PROC    0

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

; _sig is called with one parameter:
;       - procedure instance identifier (PROC)
; and returns nothing.
; only the procedure identifier inside the PROC is used.

 exp $_sig
 pro $_sig,0
 lal PROC
 loi _EM_PSIZE
 sig
 asp _EM_PSIZE
 ret 0                  ; ignore the result of sig
 end ?

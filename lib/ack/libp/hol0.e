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

; _hol0 return the address of the ABS block (hol0)

 exp $_hol0
 pro $_hol0,0
 lae 0
 ret _EM_PSIZE
 end ?

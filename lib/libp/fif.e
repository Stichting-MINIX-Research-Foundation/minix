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

#define ARG1    0
#define ARG2    _EM_DSIZE
#define IRES    2*_EM_DSIZE

; _fif is called with three parameters:
;       - address of integer part result (IRES)
;       - float two (ARG2)
;       - float one (ARG1)
; and returns an _EM_DSIZE-byte floating point number

 exp $_fif
 pro $_fif,0
 lal 0
 loi 2*_EM_DSIZE
 fif _EM_DSIZE
 lal IRES
 loi _EM_PSIZE
 sti _EM_DSIZE
 ret _EM_DSIZE
 end ?

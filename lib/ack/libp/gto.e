#
; $Header$
;  (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
; 
;           This product is part of the Amsterdam Compiler Kit.
; 
;  Permission to use, sell, duplicate or disclose this software must be
;  obtained in writing. Requests for such permissions may be sent to
; 
;       Dr. Andrew S. Tanenbaum
;       Wiskundig Seminarium
;       Vrije Universiteit
;       Postbox 7161
;       1007 MC Amsterdam
;       The Netherlands
; 

/* Author: J.W. Stevenson */


 mes 2,_EM_WSIZE,_EM_PSIZE

#define TARLB   0
#define DESCR   _EM_PSIZE

#define NEWPC   0
#define SAVSP   _EM_PSIZE

#define D_PC    0
#define D_SP    _EM_PSIZE
#define D_LB    _EM_PSIZE+_EM_PSIZE

#define LOCLB   0-_EM_PSIZE

; _gto is called with two arguments:
;       - pointer to the label descriptor (DESCR)
;       - local base (LB) of target procedure (TARLB)
; the label descriptor contains two items:
;       - label address i.e. new PC (NEWPC)
;       - offset in target procedure frame (SAVSP)
; using this offset and the LB of the target procedure, the address of
; of local variable of the target procedure is constructed.
; the target procedure must have stored the correct target SP there.

descr
 bss 3*_EM_PSIZE,0,0

 exp $_gto
 pro $_gto,_EM_PSIZE
 lal DESCR
 loi _EM_PSIZE
 adp NEWPC
 loi _EM_PSIZE
 lae descr+D_PC
 sti _EM_PSIZE
 lal TARLB
 loi _EM_PSIZE
 zer _EM_PSIZE
 cmp
 zeq *1
 lal TARLB
 loi _EM_PSIZE
 bra *2
1
 lae _m_lb
 loi _EM_PSIZE
2
 lal LOCLB
 sti _EM_PSIZE
 lal LOCLB
 loi _EM_PSIZE
 lal DESCR
 loi _EM_PSIZE
 adp SAVSP
 loi _EM_WSIZE           ; or _EM_PSIZE ?
 ads _EM_WSIZE           ; or _EM_PSIZE ?
 loi _EM_PSIZE
 lae descr+D_SP
 sti _EM_PSIZE
 lal LOCLB
 loi _EM_PSIZE
 lae descr+D_LB
 sti _EM_PSIZE
 gto descr
 end ?

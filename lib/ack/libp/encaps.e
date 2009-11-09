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

 mes 2,_EM_WSIZE,_EM_PSIZE

; procedure encaps(procedure p; procedure(q(n:integer));
; {call q if a trap occurs during the execution of p}
; {if q returns, continue execution of p}


 inp $handler

#define PIISZ   2*_EM_PSIZE

#define PARG    0
#define QARG    PIISZ
#define E_ELB   0-_EM_PSIZE
#define E_EHA   -2*_EM_PSIZE

; encaps is called with two parameters:
;       - procedure instance identifier of q (QARG)
;       - procedure instance identifier of p (PARG)
; and two local variables:
;       - the lb of the previous encaps      (E_ELB)
;       - the procedure identifier of the previous handler (E_EHA)
;
; One static variable:
;       - the lb of the currently active encaps (enc_lb)

enc_lb
        bss _EM_PSIZE,0,0

 exp $encaps
 pro $encaps,PIISZ
 ; save lb of previous encaps
 lae enc_lb
 loi _EM_PSIZE
 lal E_ELB
 sti _EM_PSIZE
 ; set new lb
 lxl 0
 lae enc_lb
 sti _EM_PSIZE
 ; save old handler id while setting up the new handler
 lpi $handler
 sig
 lal E_EHA
 sti _EM_PSIZE
 ; handler is ready, p can be called
 ; p doesn't expect parameters except possibly the static link
 ; always passing the link won't hurt
 lal PARG
 loi PIISZ
 cai
 asp _EM_PSIZE
 ; reinstate old handler
 lal E_ELB
 loi _EM_PSIZE
 lae enc_lb
 sti _EM_PSIZE
 lal E_EHA
 loi _EM_PSIZE
 sig
 asp _EM_PSIZE
 ret 0
 end ?

#define TRAP    0
#define H_ELB   0-_EM_PSIZE

; handler is called with one parameter:
;       - trap number (TRAP)
; one local variable
;       - the current LB of the enclosing encaps (H_ELB)


 pro $handler,_EM_PSIZE
 ; save LB of nearest encaps
 lae enc_lb
 loi _EM_PSIZE
 lal H_ELB
 sti _EM_PSIZE
 ; fetch setting for previous encaps via LB of nearest
 lal H_ELB
 loi _EM_PSIZE
 adp E_ELB
 loi _EM_PSIZE   ; LB of previous encaps
 lae enc_lb
 sti _EM_PSIZE
 lal H_ELB
 loi _EM_PSIZE
 adp E_EHA
 loi _EM_PSIZE   ; previous handler
 sig
 asp _EM_PSIZE
 ; previous handler is re-instated, time to call Q
 lol TRAP       ; the one and only real parameter
 lal H_ELB
 loi _EM_PSIZE
 lpb            ; argument base of enclosing encaps
 adp QARG
 loi PIISZ
 exg _EM_PSIZE
 dup _EM_PSIZE   ; The static link is now on top
 zer _EM_PSIZE
 cmp
 zeq *1
 ; non-zero LB
 exg _EM_PSIZE
 cai
 asp _EM_WSIZE+_EM_PSIZE
 bra *2
1
 ; zero LB
 asp _EM_PSIZE
 cai
 asp _EM_WSIZE
2
 ; now reinstate handler for continued execution of p
 lal H_ELB
 loi _EM_PSIZE
 lae enc_lb
 sti _EM_PSIZE
 lpi $handler
 sig
 asp _EM_PSIZE
 rtt
 end ?

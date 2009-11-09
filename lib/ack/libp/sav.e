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

#define	PTRAD	0

#define	HP	2

; _sav called with one parameter:
;	- address of pointer variable (PTRAD)

 exp $_sav
 pro $_sav,0
 lor HP
 lal PTRAD
 loi _EM_PSIZE
 sti _EM_PSIZE
 ret 0
 end ?

; _rst is called with one parameter:
;	- address of pointer variable (PTRAD)

 exp $_rst
 pro $_rst,0
 lal PTRAD
 loi _EM_PSIZE
 loi _EM_PSIZE
 str HP
 ret 0
 end ?

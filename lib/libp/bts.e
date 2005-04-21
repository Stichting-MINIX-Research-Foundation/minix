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

; Author: J.W. Stevenson */

 mes 2,_EM_WSIZE,_EM_PSIZE

#define	SIZE	0
#define	HIGH	_EM_WSIZE
#define	LOWB	2*_EM_WSIZE
#define	BASE	3*_EM_WSIZE

; _bts is called with four parameters:
;	- the initial set (BASE)
;	- low bound of range of bits (LOWB)
;	- high bound of range of bits (HIGH)
;	- set size in bytes (SIZE)

 exp $_bts
 pro $_bts,0
 lal BASE	; address of initial set
 lol SIZE
 los _EM_WSIZE	; load initial set
1
 lol LOWB	; low bound
 lol HIGH	; high bound
 bgt *2		; while low <= high
 lol LOWB
 lol SIZE
 set ?		; create [low]
 lol SIZE
 ior ?		; merge with initial set
 inl LOWB	; increment low bound
 bra *1		; loop back
2
 lal BASE
 lol SIZE
 sts _EM_WSIZE	; store result over initial set
 ret 0
 end ?

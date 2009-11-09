#
;
; (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
; See the copyright notice in the ACK home directory, in the file "Copyright".
;
;
; Module:	Compute non-constant set displays
; Author:	Ceriel J.H. Jacobs
; Version:	$Header$
;
 mes 2,_EM_WSIZE,_EM_PSIZE

 ; LtoUset is called for set displays containing { expr1 .. expr2 }.
 ; It has six parameters, of which the caller must pop five:
 ; - The set in which bits must be set.
 ; - the lower bound of the set type.
 ; - The set size in bytes.
 ; - The upper bound of set elements, specified by the set-type.
 ; - "expr2", the upper bound
 ; - "expr1", the lower bound

#define SETBASE	5*_EM_WSIZE
#define SETLOW	4*_EM_WSIZE
#define SETSIZE 3*_EM_WSIZE
#define USETSIZ 2*_EM_WSIZE
#define LWB	_EM_WSIZE
#define UPB	0
 exp $LtoUset
 pro $LtoUset,0
 lal SETBASE	; address of initial set
 lol SETSIZE
 los _EM_WSIZE	; load initial set
 lol LWB	; low bound
 lol SETLOW
 sbu _EM_WSIZE
 stl LWB
 lol UPB	; high bound
 lol SETLOW
 sbu _EM_WSIZE
 stl UPB
1
 lol LWB
 lol UPB
 cmu _EM_WSIZE
 zgt *2		; while low <= high
 lol LWB
 lol SETSIZE
 set ?		; create [low]
 lol SETSIZE
 ior ?		; merge with initial set
 lol LWB
 loc 1
 adu _EM_WSIZE
 stl LWB
 bra *1		; loop back
2
 lal SETBASE
 lol SETSIZE
 sts _EM_WSIZE	; store result over initial set
 ret 0
 end 0

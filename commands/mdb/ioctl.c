/* 
 * ioctl.c for mdb -- decode an IOCTL system call
 */
#include "mdb.h"
#ifdef SYSCALLS_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <minix/type.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <sys/ioctl.h>
#include <sgtty.h>
#include "proto.h"

PRIVATE int get_request;

PRIVATE char *rnames[] = {
	"TIOCGETP",  
	"TIOCSETP",
	"TIOCGETC",
	"TIOCSETC"
};

#define GETPNAME	0
#define SETPNAME	1
#define GETCNAME	2
#define SETCNAME	3

/* 
 * decode ioctl call
 * send or receive = 'R' or 'S'
 */
void decode_ioctl(sr, m)
int sr;
message *m;
{
int rq;
int request, device;
int high;
long spek, flags;
int ispeed, ospeed, speed, erase, kill;
int t_intrc, t_quitc, t_startc, t_stopc, t_brk, t_eof;

   device  = m->m2_i1;
   request = m->m2_i3;
   spek    = m->m2_l1;
   flags   = m->m2_l2;

#ifdef DEBUG
   if( debug )
   Printf("%c device=%d request=%c|%d m2_l1=%lx m2_l2=%lx\n",
	sr,device,
	(request >> 8) & BYTE,
	request & BYTE,
	spek,flags);
#endif 

   if ( sr == 'R') request = get_request;

   switch(request) {
     case TIOCGETP:
     case TIOCSETP:
	rq = (request == TIOCGETP) ? GETPNAME : SETPNAME;
	if (sr == 'S') {
		get_request = request;
		Printf( "Sending %s ",rnames[rq] );
		if ( request == TIOCGETP ) break;
	}
		
	if ( (sr == 'R') && (request == TIOCSETP) ) break;

	erase = (spek >> 8) & BYTE;
	kill  = spek & BYTE;
  	flags = flags & 0xFFFF;
	speed = (spek >> 16) & 0xFFFFL;
	ispeed = speed & BYTE;
	ospeed = (speed >> 8) & BYTE;
	Printf("%s erase=%d kill=%d speed=%d/%d flags=%lx ",
		rnames[rq], erase, kill, ispeed, ospeed, flags);
	break;

     case TIOCGETC:
     case TIOCSETC:
	rq = (request == TIOCGETC) ? GETCNAME : SETCNAME;
	if (sr == 'S') {
		get_request = request;
		Printf( "Sending %s ",rnames[rq] );
		if ( request == TIOCGETC ) break;
	}

	if ( (sr == 'R') && (request == TIOCSETC) ) break;

  	t_intrc  = (spek >> 24) & BYTE;
  	t_quitc  = (spek >> 16) & BYTE;
  	t_startc = (spek >>  8) & BYTE;
  	t_stopc  = (spek >>  0) & BYTE;
	t_brk    = flags & BYTE;
	t_eof    = (flags >> 8 ) & BYTE;
	Printf("%s int %d quit %d start %d stop %d brk %d eof %d\n",
		rnames[rq],
		t_intrc, t_quitc, t_startc, t_stopc, t_brk, t_eof);
	break;

     default:

#ifdef	__i86	/* 16 bit */	
	if ( sr == 'S' ) {
		Printf("Sending ");
		get_request = request;
	}
	else
		Printf("Receiving ");
	
	Printf("Other IOCTL device=%d request=%c|%d\n",
		device, (request >> 8) & BYTE, request & BYTE );

	break;
#endif

#ifdef	__i386	      /* 32 bit encoding */
	if ( sr == 'S' ) {
		Printf("Sending (%lx) ",   request);
		get_request = request;
	}
	else
		Printf("Receiving (%lx) ", request);
	
	high = ( request & 0xFFFF0000 ) >> 16 ;
	request &= _IOCTYPE_MASK;

	Printf("Other IOCTL device=%d request=%c|%d flags=%x size =%d\n",
		device, (request >> 8) & BYTE, request & BYTE,
		(high  &  ~_IOCPARM_MASK ),
		(high  &   _IOCPARM_MASK )
		);
	break;
#endif
     }	
     Printf("\n");
}


#endif /* SYSCALLS_SUPPORT */

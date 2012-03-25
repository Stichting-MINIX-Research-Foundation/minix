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

static int get_request;

/* 
 * decode ioctl call
 * send or receive = 'R' or 'S'
 */
void decode_ioctl(sr, m)
int sr;
message *m;
{
int request, device;
int high;
long spek, flags;

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
   Printf("\n");
}


#endif /* SYSCALLS_SUPPORT */

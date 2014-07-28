/*
icmp.h

Copyright 1995 Philip Homburg
*/

#ifndef ICMP_H
#define ICMP_H

#define ICMP_MAX_DATAGRAM	8196
#define ICMP_DEF_TTL		96

/* Rate limit. The implementation is a bit sloppy and may send twice the
 * number of packets. 
 */
#define ICMP_MAX_RATE		100	/* This many per interval */
#define ICMP_RATE_INTERVAL	(1*HZ)	/* Interval in ticks */
#define ICMP_RATE_WARN		10	/* Report this many dropped packets */

/* Prototypes */

void icmp_prep ARGS(( void ));
void icmp_init ARGS(( void ));


#endif /* ICMP_H */

/*
 * $PchId: icmp.h,v 1.7 2001/04/19 19:06:18 philip Exp $
 */

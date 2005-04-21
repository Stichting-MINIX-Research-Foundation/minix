/*
icmp.h

Copyright 1995 Philip Homburg
*/

#ifndef ICMP_H
#define ICMP_H

#define ICMP_MAX_DATAGRAM	8196
#define ICMP_DEF_TTL		60

/* Prototypes */

void icmp_prep ARGS(( void ));
void icmp_init ARGS(( void ));


#endif /* ICMP_H */

/*
 * $PchId: icmp.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */

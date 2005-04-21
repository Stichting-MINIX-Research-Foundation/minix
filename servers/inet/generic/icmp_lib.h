/*
icmp_lib.h

Created Sept 30, 1991 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef ICMP_LIB_H
#define ICMP_LIB_H

/* Prototypes */

void icmp_snd_parmproblem ARGS(( acc_t *pack ));
void icmp_snd_time_exceeded ARGS(( int port_nr, acc_t *pack, int code ));
void icmp_snd_unreachable ARGS(( int port_nr, acc_t *pack, int code ));
void icmp_snd_redirect ARGS(( int port_nr, acc_t *pack, int code,
							ipaddr_t gw ));

#endif /* ICMP_LIB_H */

/*
 * $PchId: icmp_lib.h,v 1.5 1996/12/17 07:54:09 philip Exp $
 */

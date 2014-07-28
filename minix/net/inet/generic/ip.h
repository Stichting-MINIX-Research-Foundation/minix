/*
ip.h

Copyright 1995 Philip Homburg
*/

#ifndef INET_IP_H
#define INET_IP_H

/* Prototypes */

struct acc;

void ip_prep ARGS(( void ));
void ip_init ARGS(( void ));
int  ip_open ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t put_pkt, select_res_t select_res ));
int ip_ioctl ARGS(( int fd, ioreq_t req ));
int ip_read ARGS(( int fd, size_t count ));
int ip_write ARGS(( int fd, size_t count ));
int ip_send ARGS(( int fd, struct acc *data, size_t data_len ));

#endif /* INET_IP_H */

/*
 * $PchId: ip.h,v 1.8 2005/06/28 14:17:57 philip Exp $
 */

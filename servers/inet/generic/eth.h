/*
eth.h

Copyright 1995 Philip Homburg
*/

#ifndef ETH_H
#define ETH_H

#define NWEO_DEFAULT    (NWEO_EN_LOC | NWEO_DI_BROAD | NWEO_DI_MULTI | \
	NWEO_DI_PROMISC | NWEO_REMANY | NWEO_RWDATALL)

#define eth_addrcmp(a,b) (memcmp((_VOIDSTAR)&a, (_VOIDSTAR)&b, \
	sizeof(a)))

/* Forward declatations */

struct acc;

/* prototypes */

void eth_prep ARGS(( void ));
void eth_init ARGS(( void ));
int eth_open ARGS(( int port, int srfd,
	get_userdata_t get_userdata, put_userdata_t put_userdata,
	put_pkt_t put_pkt ));
int eth_ioctl ARGS(( int fd, ioreq_t req));
int eth_read ARGS(( int port, size_t count ));
int eth_write ARGS(( int port, size_t count ));
int eth_cancel ARGS(( int fd, int which_operation ));
void eth_close ARGS(( int fd ));
int eth_send ARGS(( int port, struct acc *data, size_t data_len ));

#endif /* ETH_H */

/*
 * $PchId: eth.h,v 1.6 1996/05/07 20:49:07 philip Exp $
 */

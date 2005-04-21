/*
inet/osdep_eth.h

Created:	Dec 30, 1991 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef INET__OSDEP_ETH_H
#define INET__OSDEP_ETH_H

#include "generic/event.h"

#define IOVEC_NR	16
#define RD_IOVEC	((ETH_MAX_PACK_SIZE + BUF_S -1)/BUF_S)

typedef struct osdep_eth_port
{
	int etp_task;
	int etp_port;
	int etp_recvconf;
	iovec_t etp_wr_iovec[IOVEC_NR];
	iovec_t etp_rd_iovec[RD_IOVEC];
	event_t etp_recvev;
	message etp_sendrepl;
	message etp_recvrepl;
} osdep_eth_port_t;

#endif /* INET__OSDEP_ETH_H */

/*
 * $PchId: osdep_eth.h,v 1.5 1995/11/21 06:41:28 philip Exp $
 */

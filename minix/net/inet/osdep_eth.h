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
	int etp_state;
	int etp_flags;
	endpoint_t etp_task;
	int etp_recvconf;
	iovec_s_t etp_wr_iovec[IOVEC_NR];
	cp_grant_id_t etp_wr_vec_grant;
	iovec_s_t etp_rd_iovec[RD_IOVEC];
	cp_grant_id_t etp_rd_vec_grant;
	event_t etp_recvev;
	cp_grant_id_t etp_stat_gid;
	eth_stat_t *etp_stat_buf;
} osdep_eth_port_t;

#define OEPS_INIT		0	/* Not initialized */
#define OEPS_CONF_SENT		1	/* Conf. request has been sent */
#define OEPS_IDLE		2	/* Device is ready to accept requests */
#define OEPS_RECV_SENT		3	/* Recv. request has been sent */
#define OEPS_SEND_SENT		4	/* Send request has been sent */
#define OEPS_GETSTAT_SENT	5	/* GETSTAT request has been sent */

#define OEPF_EMPTY	0
#define OEPF_NEED_RECV	1	/* Issue recv. request when the state becomes
				 * idle
				 */
#define OEPF_NEED_SEND	2	/* Issue send request when the state becomes
				 * idle
				 */
#define OEPF_NEED_CONF	4	/* Issue conf request when the state becomes
				 * idle
				 */
#define OEPF_NEED_STAT	8	/* Issue getstat request when the state becomes
				 * idle
				 */

#endif /* INET__OSDEP_ETH_H */

/*
 * $PchId: osdep_eth.h,v 1.6 2001/04/20 06:39:54 philip Exp $
 */

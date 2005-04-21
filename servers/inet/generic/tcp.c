/*
tcp.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "type.h"

#if !CRAMPED
#include "io.h"
#include "ip.h"
#endif
#include "sr.h"
#include "assert.h"
#include "tcp.h"
#include "tcp_int.h"

THIS_FILE

PUBLIC tcp_port_t *tcp_port_table;
PUBLIC tcp_fd_t tcp_fd_table[TCP_FD_NR];
PUBLIC tcp_conn_t tcp_conn_table[TCP_CONN_NR];

FORWARD void tcp_main ARGS(( tcp_port_t *port ));
FORWARD acc_t *tcp_get_data ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
FORWARD int tcp_put_data ARGS(( int fd, size_t offset,
	acc_t *data, int for_ioctl ));
FORWARD void tcp_put_pkt ARGS(( int fd, acc_t *data, size_t datalen ));
FORWARD void read_ip_packets ARGS(( tcp_port_t *port ));
FORWARD int tcp_setconf ARGS(( tcp_fd_t *tcp_fd ));
FORWARD int tcp_setopt ARGS(( tcp_fd_t *tcp_fd ));
FORWARD int tcp_connect ARGS(( tcp_fd_t *tcp_fd ));
FORWARD int tcp_listen ARGS(( tcp_fd_t *tcp_fd ));
FORWARD tcpport_t find_unused_port ARGS(( int fd ));
FORWARD int is_unused_port ARGS(( Tcpport_t port ));
FORWARD int reply_thr_put ARGS(( tcp_fd_t *tcp_fd, int reply,
	int for_ioctl ));
FORWARD void reply_thr_get ARGS(( tcp_fd_t *tcp_fd, int reply,
	int for_ioctl ));
FORWARD tcp_conn_t *find_conn_entry ARGS(( Tcpport_t locport,
	ipaddr_t locaddr, Tcpport_t remport, ipaddr_t readaddr ));
FORWARD tcp_conn_t *find_empty_conn ARGS(( void ));
FORWARD tcp_conn_t *find_best_conn ARGS(( ip_hdr_t *ip_hdr, 
	tcp_hdr_t *tcp_hdr ));
FORWARD int maybe_listen ARGS(( ipaddr_t locaddr, Tcpport_t locport,
				ipaddr_t remaddr, Tcpport_t remport ));
FORWARD int conn_right4fd ARGS(( tcp_conn_t *tcp_conn, tcp_fd_t *tcp_fd ));
FORWARD int tcp_su4connect ARGS(( tcp_fd_t *tcp_fd ));
FORWARD void tcp_buffree ARGS(( int priority ));
#ifdef BUF_CONSISTENCY_CHECK
FORWARD void tcp_bufcheck ARGS(( void ));
#endif
FORWARD void tcp_setup_conn ARGS(( tcp_conn_t *tcp_conn ));

PUBLIC void tcp_prep()
{
	tcp_port_table= alloc(ip_conf_nr * sizeof(tcp_port_table[0]));
}

PUBLIC void tcp_init()
{
	int i, j, k;
	tcp_fd_t *tcp_fd;
	tcp_port_t *tcp_port;
	tcp_conn_t *tcp_conn;

	assert (BUF_S >= sizeof(struct nwio_ipopt));
	assert (BUF_S >= sizeof(struct nwio_ipconf));
	assert (BUF_S >= sizeof(struct nwio_tcpconf));
	assert (BUF_S >= IP_MAX_HDR_SIZE + TCP_MAX_HDR_SIZE);

#if ZERO
	for (i=0, tcp_fd= tcp_fd_table; i<TCP_FD_NR; i++, tcp_fd++)
	{
		tcp_fd->tf_flags= TFF_EMPTY;
	}

	for (i=0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++,
		tcp_fd++)
	{
		tcp_conn->tc_flags= TCF_EMPTY;
		tcp_conn->tc_busy= 0;
	}
#endif

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(tcp_buffree);
#else
	bf_logon(tcp_buffree, tcp_bufcheck);
#endif

	for (i=0, tcp_port= tcp_port_table; i<ip_conf_nr; i++, tcp_port++)
	{
		tcp_port->tp_ipdev= i;

#if ZERO
		tcp_port->tp_flags= TPF_EMPTY;
		tcp_port->tp_state= TPS_EMPTY;
		tcp_port->tp_snd_head= NULL;
		tcp_port->tp_snd_tail= NULL;
		ev_init(&tcp_port->tp_snd_event);
#endif
		for (j= 0; j<TCP_CONN_HASH_NR; j++)
		{
			for (k= 0; k<4; k++)
			{
				tcp_port->tp_conn_hash[j][k]=
					&tcp_conn_table[0];
			}
		}

		sr_add_minor(if2minor(ip_conf[i].ic_ifno, TCP_DEV_OFF),
			i, tcp_open, tcp_close, tcp_read,
			tcp_write, tcp_ioctl, tcp_cancel);

		tcp_main(tcp_port);
	}
}

PRIVATE void tcp_main(tcp_port)
tcp_port_t *tcp_port;
{
	int result, i;
	tcp_conn_t *tcp_conn;
	tcp_fd_t *tcp_fd;

	switch (tcp_port->tp_state)
	{
	case TPS_EMPTY:
		tcp_port->tp_state= TPS_SETPROTO;
		tcp_port->tp_ipfd= ip_open(tcp_port->tp_ipdev,
			tcp_port->tp_ipdev, tcp_get_data,
			tcp_put_data, tcp_put_pkt);
		if (tcp_port->tp_ipfd < 0)
		{
			tcp_port->tp_state= TPS_ERROR;
			DBLOCK(1, printf("%s, %d: unable to open ip port\n",
				__FILE__, __LINE__));
			return;
		}

		result= ip_ioctl(tcp_port->tp_ipfd, NWIOSIPOPT);
		if (result == NW_SUSPEND)
			tcp_port->tp_flags |= TPF_SUSPEND;
		if (result < 0)
		{
			return;
		}
		if (tcp_port->tp_state != TPS_GETCONF)
			return;
		/* drops through */
	case TPS_GETCONF:
		tcp_port->tp_flags &= ~TPF_SUSPEND;

		result= ip_ioctl(tcp_port->tp_ipfd, NWIOGIPCONF);
		if (result == NW_SUSPEND)
			tcp_port->tp_flags |= TPF_SUSPEND;
		if (result < 0)
		{
			return;
		}
		if (tcp_port->tp_state != TPS_MAIN)
			return;
		/* drops through */
	case TPS_MAIN:
		tcp_port->tp_flags &= ~TPF_SUSPEND;
		tcp_port->tp_pack= 0;

		tcp_conn= &tcp_conn_table[tcp_port->tp_ipdev];
		tcp_conn->tc_flags= TCF_INUSE;
		assert(!tcp_conn->tc_busy);
		tcp_conn->tc_locport= 0;
		tcp_conn->tc_locaddr= tcp_port->tp_ipaddr;
		tcp_conn->tc_remport= 0;
		tcp_conn->tc_remaddr= 0;
		tcp_conn->tc_state= TCS_CLOSED;
		tcp_conn->tc_fd= 0;
		tcp_conn->tc_connInprogress= 0;
		tcp_conn->tc_orglisten= FALSE;
		tcp_conn->tc_senddis= 0;
		tcp_conn->tc_ISS= 0;
		tcp_conn->tc_SND_UNA= tcp_conn->tc_ISS;
		tcp_conn->tc_SND_TRM= tcp_conn->tc_ISS;
		tcp_conn->tc_SND_NXT= tcp_conn->tc_ISS;
		tcp_conn->tc_SND_UP= tcp_conn->tc_ISS;
		tcp_conn->tc_IRS= 0;
		tcp_conn->tc_RCV_LO= tcp_conn->tc_IRS;
		tcp_conn->tc_RCV_NXT= tcp_conn->tc_IRS;
		tcp_conn->tc_RCV_HI= tcp_conn->tc_IRS;
		tcp_conn->tc_RCV_UP= tcp_conn->tc_IRS;
		tcp_conn->tc_port= tcp_port;
		tcp_conn->tc_rcvd_data= NULL;
		tcp_conn->tc_adv_data= NULL;
		tcp_conn->tc_send_data= 0;
		tcp_conn->tc_remipopt= NULL;
		tcp_conn->tc_tcpopt= NULL;
		tcp_conn->tc_frag2send= 0;
		tcp_conn->tc_tos= TCP_DEF_TOS;
		tcp_conn->tc_ttl= IP_MAX_TTL;
		tcp_conn->tc_rcv_wnd= TCP_MAX_RCV_WND_SIZE;
		tcp_conn->tc_rt_dead= TCP_DEF_RT_DEAD;
		tcp_conn->tc_stt= 0;
		tcp_conn->tc_0wnd_to= 0;
		tcp_conn->tc_rtt= TCP_DEF_RTT;
		tcp_conn->tc_mss= TCP_DEF_MSS;
		tcp_conn->tc_error= NW_OK;
		tcp_conn->tc_snd_wnd= TCP_MAX_SND_WND_SIZE;
		tcp_conn->tc_snd_cinc=
			(long)TCP_DEF_MSS*TCP_DEF_MSS/TCP_MAX_SND_WND_SIZE+1;

		tcp_conn->tc_rt_time= 0;
		tcp_conn->tc_rt_seq= 0;
		tcp_conn->tc_rt_threshold= tcp_conn->tc_ISS;

		for (i=0, tcp_fd= tcp_fd_table; i<TCP_FD_NR; i++,
			tcp_fd++)
		{
			if (!(tcp_fd->tf_flags & TFF_INUSE))
				continue;
			if (tcp_fd->tf_port != tcp_port)
				continue;
			if (tcp_fd->tf_flags & TFF_IOC_INIT_SP)
			{
				tcp_fd->tf_flags &= ~TFF_IOC_INIT_SP;
				tcp_ioctl(i, tcp_fd->tf_ioreq);
			}
		}
		read_ip_packets(tcp_port);
		return;

#if !CRAMPED
	default:
		ip_panic(( "unknown state" ));
#endif
	}
}

PRIVATE acc_t *tcp_get_data (port, offset, count, for_ioctl)
int port;
size_t offset;
size_t count;
int for_ioctl;
{
	tcp_port_t *tcp_port;
	int result;

	tcp_port= &tcp_port_table[port];

	switch (tcp_port->tp_state)
	{
	case TPS_SETPROTO:
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				tcp_port->tp_state= TPS_ERROR;
				break;
			}
			tcp_port->tp_state= TPS_GETCONF;
			if (tcp_port->tp_flags & TPF_SUSPEND)
				tcp_main(tcp_port);
			return NW_OK;
		}
assert (!offset);
assert (count == sizeof(struct nwio_ipopt));
		{
			struct nwio_ipopt *ipopt;
			acc_t *acc;

			acc= bf_memreq(sizeof(*ipopt));
			ipopt= (struct nwio_ipopt *)ptr2acc_data(acc);
			ipopt->nwio_flags= NWIO_COPY |
				NWIO_EN_LOC | NWIO_DI_BROAD |
				NWIO_REMANY | NWIO_PROTOSPEC |
				NWIO_HDR_O_ANY | NWIO_RWDATALL;
			ipopt->nwio_proto= IPPROTO_TCP;
			return acc;
		}
	case TPS_MAIN:
		assert(tcp_port->tp_flags & TPF_WRITE_IP);
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				if (result == EDSTNOTRCH)
				{
					if (tcp_port->tp_snd_head)
					{
						tcp_notreach(tcp_port->
							tp_snd_head);
					}
				}
				else
				{
					ip_warning((
					"ip_write failed with error: %d\n", 
								result ));
				}
			}
			assert (tcp_port->tp_pack);
			bf_afree (tcp_port->tp_pack);
			tcp_port->tp_pack= 0;

			if (tcp_port->tp_flags & TPF_WRITE_SP)
			{
				tcp_port->tp_flags &= ~(TPF_WRITE_SP|
					TPF_WRITE_IP);
				if (tcp_port->tp_snd_head)
					tcp_port_write(tcp_port);
			}
			else
				tcp_port->tp_flags &= ~TPF_WRITE_IP;
		}
		else
		{
			return bf_cut (tcp_port->tp_pack, offset,
				count);
		}
		break;
	default:
#if !CRAMPED
		printf("tcp_get_data(%d, 0x%x, 0x%x) called but tp_state= 0x%x\n",
			port, offset, count, tcp_port->tp_state);
#endif
		break;
	}
	return NW_OK;
}

PRIVATE int tcp_put_data (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	tcp_port_t *tcp_port;
	int result;

	tcp_port= &tcp_port_table[fd];

	switch (tcp_port->tp_state)
	{
	case TPS_GETCONF:
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				tcp_port->tp_state= TPS_ERROR;
				return NW_OK;
			}
			tcp_port->tp_state= TPS_MAIN;
			if (tcp_port->tp_flags & TPF_SUSPEND)
				tcp_main(tcp_port);
		}
		else
		{
			struct nwio_ipconf *ipconf;

			data= bf_packIffLess(data, sizeof(*ipconf));
			ipconf= (struct nwio_ipconf *)ptr2acc_data(data);
assert (ipconf->nwic_flags & NWIC_IPADDR_SET);
			tcp_port->tp_ipaddr= ipconf->nwic_ipaddr;
			bf_afree(data);
		}
		break;
	case TPS_MAIN:
		assert(tcp_port->tp_flags & TPF_READ_IP);
		if (!data)
		{
			result= (int)offset;
			if (result<0)
				ip_panic(( "ip_read() failed" ));

			if (tcp_port->tp_flags & TPF_READ_SP)
			{
				tcp_port->tp_flags &= ~(TPF_READ_SP|
					TPF_READ_IP);
				read_ip_packets(tcp_port);
			}
			else
				tcp_port->tp_flags &= ~TPF_READ_IP;
		}
		else
		{
			assert(!offset);	
			/* this is an invalid assertion but ip sends
			  * only whole datagrams up */
			tcp_put_pkt(fd, data, bf_bufsize(data));
		}
		break;
	default:
#if !CRAMPED
		printf("tcp_put_data(%d, 0x%x, 0x%x) called but tp_state= 0x%x\n",
	fd, offset, data, tcp_port->tp_state);
#endif
		break;
	}
	return NW_OK;
}

/*
tcp_put_pkt
*/

PRIVATE void tcp_put_pkt(fd, data, datalen)
int fd;
acc_t *data;
size_t datalen;
{
	tcp_port_t *tcp_port;
	tcp_conn_t *tcp_conn, **conn_p;
	ip_hdr_t *ip_hdr;
	tcp_hdr_t *tcp_hdr;
	acc_t *ip_pack, *tcp_pack;
	size_t ip_datalen, tcp_datalen, ip_hdr_len, tcp_hdr_len;
	u16_t sum;
	u32_t bits;
	int hash;
	ipaddr_t srcaddr, dstaddr;
	tcpport_t srcport, dstport;

	tcp_port= &tcp_port_table[fd];

	/* Extract the IP header. */
	ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	ip_datalen= datalen - ip_hdr_len;
	if (ip_datalen == 0)
	{
		DBLOCK(1, printf("tcp_put_pkt: no TCP header\n"));
		bf_afree(data);
		return;
	}
	data->acc_linkC++;
	ip_pack= data;
	ip_pack= bf_align(ip_pack, ip_hdr_len, 4);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_pack);
	data= bf_delhead(data, ip_hdr_len);

	/* Compute the checksum */
	sum= tcp_pack_oneCsum(ip_hdr, data);

	/* Extract the TCP header */
	if (ip_datalen < TCP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("truncated TCP header\n"));
		bf_afree(ip_pack);
		bf_afree(data);
		return;
	}
	data= bf_packIffLess(data, TCP_MIN_HDR_SIZE);
	tcp_hdr= (tcp_hdr_t *)ptr2acc_data(data);
	tcp_hdr_len= (tcp_hdr->th_data_off & TH_DO_MASK) >> 2;
		/* actualy (>> 4) << 2 */
	if (ip_datalen < tcp_hdr_len || tcp_hdr_len < TCP_MIN_HDR_SIZE)
	{
		if (tcp_hdr_len < TCP_MIN_HDR_SIZE)
		{
			DBLOCK(1, printf("strange tcp header length %d\n",
				tcp_hdr_len));
		}
		else
		{
			DBLOCK(1, printf("truncated TCP header\n"));
		}
		bf_afree(ip_pack);
		bf_afree(data);
		return;
	}
	data->acc_linkC++;
	tcp_pack= data;
	tcp_pack= bf_align(tcp_pack, tcp_hdr_len, 4);
	tcp_hdr= (tcp_hdr_t *)ptr2acc_data(tcp_pack);
	if (ip_datalen == tcp_hdr_len)
	{
		bf_afree(data);
		data= NULL;
	}
	else
		data= bf_delhead(data, tcp_hdr_len);
	tcp_datalen= ip_datalen-tcp_hdr_len;

	if ((u16_t)~sum)
	{
		DBLOCK(1, printf("checksum error in tcp packet\n");
			printf("tcp_pack_oneCsum(...)= 0x%x length= %d\n", 
			(u16_t)~sum, tcp_datalen);
			printf("src ip_addr= "); writeIpAddr(ip_hdr->ih_src);
			printf("\n"));
		bf_afree(ip_pack);
		bf_afree(tcp_pack);
		bf_afree(data);
		return;
	}

	srcaddr= ip_hdr->ih_src;
	dstaddr= ip_hdr->ih_dst;
	srcport= tcp_hdr->th_srcport;
	dstport= tcp_hdr->th_dstport;
	bits= srcaddr ^ dstaddr ^ srcport ^ dstport;
	bits= (bits >> 16) ^ bits;
	bits= (bits >> 8) ^ bits;
	hash= ((bits >> TCP_CONN_HASH_SHIFT) ^ bits) & (TCP_CONN_HASH_NR-1);
	conn_p= tcp_port->tp_conn_hash[hash];
	if (conn_p[0]->tc_locport == dstport &&
		conn_p[0]->tc_remport == srcport &&
		conn_p[0]->tc_remaddr == srcaddr &&
		conn_p[0]->tc_locaddr == dstaddr)
	{
		tcp_conn= conn_p[0];
	}
	else if (conn_p[1]->tc_locport == dstport &&
		conn_p[1]->tc_remport == srcport &&
		conn_p[1]->tc_remaddr == srcaddr &&
		conn_p[1]->tc_locaddr == dstaddr)
	{
		tcp_conn= conn_p[1];
		conn_p[1]= conn_p[0];
		conn_p[0]= tcp_conn;
	}
	else if (conn_p[2]->tc_locport == dstport &&
		conn_p[2]->tc_remport == srcport &&
		conn_p[2]->tc_remaddr == srcaddr &&
		conn_p[2]->tc_locaddr == dstaddr)
	{
		tcp_conn= conn_p[2];
		conn_p[2]= conn_p[1];
		conn_p[1]= conn_p[0];
		conn_p[0]= tcp_conn;
	}
	else if (conn_p[3]->tc_locport == dstport &&
		conn_p[3]->tc_remport == srcport &&
		conn_p[3]->tc_remaddr == srcaddr &&
		conn_p[3]->tc_locaddr == dstaddr)
	{
		tcp_conn= conn_p[3];
		conn_p[3]= conn_p[2];
		conn_p[2]= conn_p[1];
		conn_p[1]= conn_p[0];
		conn_p[0]= tcp_conn;
	}
	else
		tcp_conn= NULL;
	if (tcp_conn != NULL && tcp_conn->tc_state == TCS_CLOSED ||
		(tcp_hdr->th_flags & THF_SYN))
	{
		tcp_conn= NULL;
	}

	if (tcp_conn == NULL)
	{
		tcp_conn= find_best_conn(ip_hdr, tcp_hdr);
		if (!tcp_conn)
		{
			/* listen backlog hack */
			bf_afree(ip_pack);
			bf_afree(tcp_pack);
			bf_afree(data);
			return;
		}
		if (tcp_conn->tc_state != TCS_CLOSED)
		{
			conn_p[3]= conn_p[2];
			conn_p[2]= conn_p[1];
			conn_p[1]= conn_p[0];
			conn_p[0]= tcp_conn;
		}
	}
	assert(tcp_conn->tc_busy == 0);
	tcp_conn->tc_busy++;
	tcp_frag2conn(tcp_conn, ip_hdr, tcp_hdr, data, tcp_datalen);
	tcp_conn->tc_busy--;
	bf_afree(ip_pack);
	bf_afree(tcp_pack);
}


PUBLIC int tcp_open (port, srfd, get_userdata, put_userdata, put_pkt)
int port;
int srfd;
get_userdata_t get_userdata;
put_userdata_t put_userdata;
put_pkt_t put_pkt;
{
	int i;
	tcp_fd_t *tcp_fd;

	for (i=0; i<TCP_FD_NR && (tcp_fd_table[i].tf_flags & TFF_INUSE);
		i++);
	if (i>=TCP_FD_NR)
	{
		return EAGAIN;
	}

	tcp_fd= &tcp_fd_table[i];

	tcp_fd->tf_flags= TFF_INUSE;
	tcp_fd->tf_flags |= TFF_PUSH_DATA;	/* XXX */

	tcp_fd->tf_port= &tcp_port_table[port];
	tcp_fd->tf_srfd= srfd;
	tcp_fd->tf_tcpconf.nwtc_flags= TCP_DEF_CONF;
	tcp_fd->tf_tcpconf.nwtc_remaddr= 0;
	tcp_fd->tf_tcpconf.nwtc_remport= 0;
	tcp_fd->tf_tcpopt.nwto_flags= TCP_DEF_OPT;
	tcp_fd->tf_get_userdata= get_userdata;
	tcp_fd->tf_put_userdata= put_userdata;
	tcp_fd->tf_conn= 0;
	return i;
}

/*
tcp_ioctl
*/
PUBLIC int tcp_ioctl (fd, req)
int fd;
ioreq_t req;
{
	tcp_fd_t *tcp_fd;
	tcp_port_t *tcp_port;
	tcp_conn_t *tcp_conn;
	nwio_tcpconf_t *tcp_conf;
	nwio_tcpopt_t *tcp_opt;
	acc_t *conf_acc, *opt_acc;
	int result;

	tcp_fd= &tcp_fd_table[fd];

assert (tcp_fd->tf_flags & TFF_INUSE);

	tcp_port= tcp_fd->tf_port;
	tcp_fd->tf_flags |= TFF_IOCTL_IP;
	tcp_fd->tf_ioreq= req;

	if (tcp_port->tp_state != TPS_MAIN)
	{
		tcp_fd->tf_flags |= TFF_IOC_INIT_SP;
		return NW_SUSPEND;
	}

	switch (req)
	{
	case NWIOSTCPCONF:
		if (tcp_fd->tf_flags & TFF_CONNECTED)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get (tcp_fd, EISCONN, TRUE);
			result= NW_OK;
			break;
		}
		result= tcp_setconf(tcp_fd);
		break;
	case NWIOGTCPCONF:
		conf_acc= bf_memreq(sizeof(*tcp_conf));
assert (conf_acc->acc_length == sizeof(*tcp_conf));
		tcp_conf= (nwio_tcpconf_t *)ptr2acc_data(conf_acc);

		*tcp_conf= tcp_fd->tf_tcpconf;
		if (tcp_fd->tf_flags & TFF_CONNECTED)
		{
			tcp_conn= tcp_fd->tf_conn;
			tcp_conf->nwtc_locport= tcp_conn->tc_locport;
			tcp_conf->nwtc_remaddr= tcp_conn->tc_remaddr;
			tcp_conf->nwtc_remport= tcp_conn->tc_remport;
		}
		tcp_conf->nwtc_locaddr= tcp_fd->tf_port->tp_ipaddr;
		result= (*tcp_fd->tf_put_userdata)(tcp_fd->tf_srfd,
			0, conf_acc, TRUE);
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_put(tcp_fd, result, TRUE);
		result= NW_OK;
		break;
	case NWIOSTCPOPT:
		result= tcp_setopt(tcp_fd);
		break;
	case NWIOGTCPOPT:
		opt_acc= bf_memreq(sizeof(*tcp_opt));
		assert (opt_acc->acc_length == sizeof(*tcp_opt));
		tcp_opt= (nwio_tcpopt_t *)ptr2acc_data(opt_acc);

		*tcp_opt= tcp_fd->tf_tcpopt;
		result= (*tcp_fd->tf_put_userdata)(tcp_fd->tf_srfd,
			0, opt_acc, TRUE);
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_put(tcp_fd, result, TRUE);
		result= NW_OK;
		break;
	case NWIOTCPCONN:
		if (tcp_fd->tf_flags & TFF_CONNECTED)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get (tcp_fd, EISCONN, TRUE);
			result= NW_OK;
			break;
		}
		result= tcp_connect(tcp_fd);
		break;
	case NWIOTCPLISTEN:
		if (tcp_fd->tf_flags & TFF_CONNECTED)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get (tcp_fd, EISCONN, TRUE);
			result= NW_OK;
			break;
		}
		result= tcp_listen(tcp_fd);
		break;
	case NWIOTCPSHUTDOWN:
		if (!(tcp_fd->tf_flags & TFF_CONNECTED))
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get (tcp_fd, ENOTCONN, TRUE);
			result= NW_OK;
			break;
		}
		tcp_fd->tf_flags |= TFF_IOCTL_IP;
		tcp_fd->tf_ioreq= req;
		tcp_conn= tcp_fd->tf_conn;

		tcp_conn->tc_busy++;
		tcp_fd_write(tcp_conn);
		tcp_conn->tc_busy--;
		tcp_conn_write(tcp_conn, 0);
		if (!(tcp_fd->tf_flags & TFF_IOCTL_IP))
			return NW_OK;
		else
			return NW_SUSPEND;
		break;
	default:
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADIOCTL, TRUE);
		result= NW_OK;
		break;
	}
	return result;
}


/*
tcp_setconf
*/

PRIVATE int tcp_setconf(tcp_fd)
tcp_fd_t *tcp_fd;
{
	nwio_tcpconf_t *tcpconf;
	nwio_tcpconf_t oldconf, newconf;
	acc_t *data;
	int result;
	tcpport_t port;
	tcp_fd_t *fd_ptr;
	unsigned int new_en_flags, new_di_flags,
		old_en_flags, old_di_flags, all_flags, flags;
	int i;

	data= (*tcp_fd->tf_get_userdata)
		(tcp_fd->tf_srfd, 0,
		sizeof(nwio_tcpconf_t), TRUE);

	if (!data)
		return EFAULT;

	data= bf_packIffLess(data, sizeof(nwio_tcpconf_t));
assert (data->acc_length == sizeof(nwio_tcpconf_t));

	tcpconf= (nwio_tcpconf_t *)ptr2acc_data(data);
	oldconf= tcp_fd->tf_tcpconf;
	newconf= *tcpconf;

	old_en_flags= oldconf.nwtc_flags & 0xffff;
	old_di_flags= (oldconf.nwtc_flags >> 16) &
		0xffff;
	new_en_flags= newconf.nwtc_flags & 0xffff;
	new_di_flags= (newconf.nwtc_flags >> 16) &
		0xffff;
	if (new_en_flags & new_di_flags)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADMODE, TRUE);
		bf_afree(data);
		return NW_OK;
	}

	/* NWTC_ACC_MASK */
	if (new_di_flags & NWTC_ACC_MASK)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADMODE, TRUE);
		bf_afree(data);
		return NW_OK;
		/* access modes can't be disabled */
	}

	if (!(new_en_flags & NWTC_ACC_MASK))
		new_en_flags |= (old_en_flags & NWTC_ACC_MASK);
	
	/* NWTC_LOCPORT_MASK */
	if (new_di_flags & NWTC_LOCPORT_MASK)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADMODE, TRUE);
		bf_afree(data);
		return NW_OK;
		/* the loc ports can't be disabled */
	}
	if (!(new_en_flags & NWTC_LOCPORT_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTC_LOCPORT_MASK);
		newconf.nwtc_locport= oldconf.nwtc_locport;
	}
	else if ((new_en_flags & NWTC_LOCPORT_MASK) == NWTC_LP_SEL)
	{
		newconf.nwtc_locport= find_unused_port(tcp_fd-
			tcp_fd_table);
	}
	else if ((new_en_flags & NWTC_LOCPORT_MASK) == NWTC_LP_SET)
	{
		if (!newconf.nwtc_locport)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get(tcp_fd, EBADMODE, TRUE);
			bf_afree(data);
			return NW_OK;
		}
	}
	
	/* NWTC_REMADDR_MASK */
	if (!((new_en_flags | new_di_flags) &
		NWTC_REMADDR_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTC_REMADDR_MASK);
		new_di_flags |= (old_di_flags &
			NWTC_REMADDR_MASK);
		newconf.nwtc_remaddr= oldconf.nwtc_remaddr;
	}
	else if (new_en_flags & NWTC_SET_RA)
	{
		if (!newconf.nwtc_remaddr)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get(tcp_fd, EBADMODE, TRUE);
			bf_afree(data);
			return NW_OK;
		}
	}
	else
	{
assert (new_di_flags & NWTC_REMADDR_MASK);
		newconf.nwtc_remaddr= 0;
	}

	/* NWTC_REMPORT_MASK */
	if (!((new_en_flags | new_di_flags) & NWTC_REMPORT_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTC_REMPORT_MASK);
		new_di_flags |= (old_di_flags &
			NWTC_REMPORT_MASK);
		newconf.nwtc_remport=
			oldconf.nwtc_remport;
	}
	else if (new_en_flags & NWTC_SET_RP)
	{
		if (!newconf.nwtc_remport)
		{
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get(tcp_fd, EBADMODE, TRUE);
			bf_afree(data);
			return NW_OK;
		}
	}
	else
	{
assert (new_di_flags & NWTC_REMPORT_MASK);
		newconf.nwtc_remport= 0;
	}

	newconf.nwtc_flags= ((unsigned long)new_di_flags
		<< 16) | new_en_flags;
	all_flags= new_en_flags | new_di_flags;

	/* check the access modes */
	if ((all_flags & NWTC_LOCPORT_MASK) != NWTC_LP_UNSET)
	{
		for (i=0, fd_ptr= tcp_fd_table; i<TCP_FD_NR; i++, fd_ptr++)
		{
			if (fd_ptr == tcp_fd)
				continue;
			if (!(fd_ptr->tf_flags & TFF_INUSE))
				continue;
			if (fd_ptr->tf_port != tcp_fd->tf_port)
				continue;
			flags= fd_ptr->tf_tcpconf.nwtc_flags;
			if ((flags & NWTC_LOCPORT_MASK) == NWTC_LP_UNSET)
				continue;
			if (fd_ptr->tf_tcpconf.nwtc_locport !=
				newconf.nwtc_locport)
				continue;
			if ((flags & NWTC_ACC_MASK) != (all_flags  &
				NWTC_ACC_MASK) ||
				(all_flags & NWTC_ACC_MASK) == NWTC_EXCL)
			{
				tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
				reply_thr_get(tcp_fd, EADDRINUSE, TRUE);
				bf_afree(data);
				return NW_OK;
			}
		}
	}
				
	tcp_fd->tf_tcpconf= newconf;

	if ((all_flags & NWTC_ACC_MASK) &&
		((all_flags & NWTC_LOCPORT_MASK) == NWTC_LP_SET ||
		(all_flags & NWTC_LOCPORT_MASK) == NWTC_LP_SEL) &&
		(all_flags & NWTC_REMADDR_MASK) &&
		(all_flags & NWTC_REMPORT_MASK))
		tcp_fd->tf_flags |= TFF_CONF_SET;
	else
	{
		tcp_fd->tf_flags &= ~TFF_CONF_SET;
	}
	bf_afree(data);
	tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
	reply_thr_get(tcp_fd, NW_OK, TRUE);
	return NW_OK;
}


/*
tcp_setopt
*/

PRIVATE int tcp_setopt(tcp_fd)
tcp_fd_t *tcp_fd;
{
	nwio_tcpopt_t *tcpopt;
	nwio_tcpopt_t oldopt, newopt;
	acc_t *data;
	int result;
	tcpport_t port;
	tcp_fd_t *fd_ptr;
	unsigned int new_en_flags, new_di_flags,
		old_en_flags, old_di_flags, all_flags, flags;
	int i;

	data= (*tcp_fd->tf_get_userdata) (tcp_fd->tf_srfd, 0,
		sizeof(nwio_tcpopt_t), TRUE);

	if (!data)
		return EFAULT;

	data= bf_packIffLess(data, sizeof(nwio_tcpopt_t));
assert (data->acc_length == sizeof(nwio_tcpopt_t));

	tcpopt= (nwio_tcpopt_t *)ptr2acc_data(data);
	oldopt= tcp_fd->tf_tcpopt;
	newopt= *tcpopt;

	old_en_flags= oldopt.nwto_flags & 0xffff;
	old_di_flags= (oldopt.nwto_flags >> 16) &
		0xffff;
	new_en_flags= newopt.nwto_flags & 0xffff;
	new_di_flags= (newopt.nwto_flags >> 16) &
		0xffff;
	if (new_en_flags & new_di_flags)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADMODE, TRUE);
		return NW_OK;
	}

	/* NWTO_SND_URG_MASK */
	if (!((new_en_flags | new_di_flags) &
		NWTO_SND_URG_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTO_SND_URG_MASK);
		new_di_flags |= (old_di_flags &
			NWTO_SND_URG_MASK);
	}

	/* NWTO_RCV_URG_MASK */
	if (!((new_en_flags | new_di_flags) &
		NWTO_RCV_URG_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTO_RCV_URG_MASK);
		new_di_flags |= (old_di_flags &
			NWTO_RCV_URG_MASK);
	}

	/* NWTO_BSD_URG_MASK */
	if (!((new_en_flags | new_di_flags) &
		NWTO_BSD_URG_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTO_BSD_URG_MASK);
		new_di_flags |= (old_di_flags &
			NWTO_BSD_URG_MASK);
	}
	else
	{
		if (tcp_fd->tf_conn == NULL)
		{
			bf_afree(data);
			tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
			reply_thr_get(tcp_fd, EINVAL, TRUE);
			return NW_OK;
		}
	}

	/* NWTO_DEL_RST_MASK */
	if (!((new_en_flags | new_di_flags) &
		NWTO_DEL_RST_MASK))
	{
		new_en_flags |= (old_en_flags &
			NWTO_DEL_RST_MASK);
		new_di_flags |= (old_di_flags &
			NWTO_DEL_RST_MASK);
	}

	newopt.nwto_flags= ((unsigned long)new_di_flags
		<< 16) | new_en_flags;
	tcp_fd->tf_tcpopt= newopt;
	if (newopt.nwto_flags & NWTO_SND_URG)
		tcp_fd->tf_flags |= TFF_WR_URG;
	else
		tcp_fd->tf_flags &= ~TFF_WR_URG;

	if (newopt.nwto_flags & NWTO_RCV_URG)
		tcp_fd->tf_flags |= TFF_RECV_URG;
	else
		tcp_fd->tf_flags &= ~TFF_RECV_URG;

	if (tcp_fd->tf_conn)
	{
		if (newopt.nwto_flags & NWTO_BSD_URG)
		{
			tcp_fd->tf_conn->tc_flags |= TCF_BSD_URG;
		}
		else
		{
			tcp_fd->tf_conn->tc_flags &= ~TCF_BSD_URG;
		}
	}

	if (newopt.nwto_flags & NWTO_DEL_RST)
		tcp_fd->tf_flags |= TFF_DEL_RST;
	else
		tcp_fd->tf_flags &= ~TFF_DEL_RST;

	bf_afree(data);
	tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
	reply_thr_get(tcp_fd, NW_OK, TRUE);
	return NW_OK;
}


PRIVATE tcpport_t find_unused_port(fd)
int fd;
{
	tcpport_t port, nw_port;

	nw_port= htons(0xC000+fd);
	if (is_unused_port(nw_port))
		return nw_port;

	for (port= 0xC000+TCP_FD_NR; port < 0xFFFF; port++)
	{
		nw_port= htons(port);
		if (is_unused_port(nw_port))
			return nw_port;
	}
#if !CRAMPED
	ip_panic(( "unable to find unused port (shouldn't occur)" ));
	return 0;
#endif
}

PRIVATE int is_unused_port(port)
tcpport_t port;
{
	int i;
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tcp_conn;

	for (i= 0, tcp_fd= tcp_fd_table; i<TCP_FD_NR; i++,
		tcp_fd++)
	{
		if (!(tcp_fd->tf_flags & TFF_CONF_SET))
			continue;
		if (tcp_fd->tf_tcpconf.nwtc_locport == port)
			return FALSE;
	}
	for (i= ip_conf_nr, tcp_conn= tcp_conn_table+i;
		i<TCP_CONN_NR; i++, tcp_conn++)
		/* the first ip_conf_nr ports are special */
	{
		if (!(tcp_conn->tc_flags & TCF_INUSE))
			continue;
		if (tcp_conn->tc_locport == port)
			return FALSE;
	}
	return TRUE;
}

PRIVATE int reply_thr_put(tcp_fd, reply, for_ioctl)
tcp_fd_t *tcp_fd;
int reply;
int for_ioctl;
{
	assert (tcp_fd);

	return (*tcp_fd->tf_put_userdata)(tcp_fd->tf_srfd, reply,
		(acc_t *)0, for_ioctl);
}

PRIVATE void reply_thr_get(tcp_fd, reply, for_ioctl)
tcp_fd_t *tcp_fd;
int reply;
int for_ioctl;
{
	acc_t *result;
	result= (*tcp_fd->tf_get_userdata)(tcp_fd->tf_srfd, reply,
		(size_t)0, for_ioctl);
	assert (!result);
}

PUBLIC int tcp_su4listen(tcp_fd)
tcp_fd_t *tcp_fd;
{
	tcp_conn_t *tcp_conn;
	acc_t *tmp_acc;

	tcp_conn= tcp_fd->tf_conn;

	tcp_conn->tc_locport= tcp_fd->tf_tcpconf.nwtc_locport;
	tcp_conn->tc_locaddr= tcp_fd->tf_port->tp_ipaddr;
	if (tcp_fd->tf_tcpconf.nwtc_flags & NWTC_SET_RP)
		tcp_conn->tc_remport= tcp_fd->tf_tcpconf.nwtc_remport;
	else
		tcp_conn->tc_remport= 0;
	if (tcp_fd->tf_tcpconf.nwtc_flags & NWTC_SET_RA)
		tcp_conn->tc_remaddr= tcp_fd->tf_tcpconf.nwtc_remaddr;
	else
		tcp_conn->tc_remaddr= 0;

	tcp_setup_conn(tcp_conn);
	tcp_conn->tc_port= tcp_fd->tf_port;
	tcp_conn->tc_fd= tcp_fd;
	tcp_conn->tc_connInprogress= 1;
	tcp_conn->tc_orglisten= TRUE;
	tcp_conn->tc_state= TCS_LISTEN;
	tcp_conn->tc_rt_dead= TCP_DEF_RT_MAX_LISTEN;
	return NW_SUSPEND;
}

/*
find_empty_conn

This function returns a connection that is not inuse.
This includes connections that are never used, and connections without a
user that are not used for a while.
*/

PRIVATE tcp_conn_t *find_empty_conn()
{
	int i;
	tcp_conn_t *tcp_conn;
	int state;

	for (i=ip_conf_nr, tcp_conn= tcp_conn_table+i;
		i<TCP_CONN_NR; i++, tcp_conn++)
		/* the first ip_conf_nr connections are reserved for
		 * RSTs
		 */
	{
		if (tcp_conn->tc_flags == TCF_EMPTY)
		{
			tcp_conn->tc_connInprogress= 0;
			tcp_conn->tc_fd= NULL;
			return tcp_conn;
		}
		if (tcp_conn->tc_fd)
			continue;
		if (tcp_conn->tc_senddis > get_time())
			continue;
		if (tcp_conn->tc_state != TCS_CLOSED)
		{
			 tcp_close_connection (tcp_conn, ENOCONN);
		}
		tcp_conn->tc_flags= 0;
		return tcp_conn;
	}
	return NULL;
}


/*
find_conn_entry

This function return a connection matching locport, locaddr, remport, remaddr.
If no such connection exists NULL is returned.
If a connection exists without mainuser it is closed.
*/

PRIVATE tcp_conn_t *find_conn_entry(locport, locaddr, remport, remaddr)
tcpport_t locport;
ipaddr_t locaddr;
tcpport_t remport;
ipaddr_t remaddr;
{
	tcp_conn_t *tcp_conn;
	int i, state;

	assert(remport);
	assert(remaddr);
	for (i=ip_conf_nr, tcp_conn= tcp_conn_table+i; i<TCP_CONN_NR;
		i++, tcp_conn++)
		/* the first ip_conf_nr connections are reserved for
			RSTs */
	{
		if (tcp_conn->tc_flags == TCF_EMPTY)
			continue;
		if (tcp_conn->tc_locport != locport ||
			tcp_conn->tc_locaddr != locaddr ||
			tcp_conn->tc_remport != remport ||
			tcp_conn->tc_remaddr != remaddr)
			continue;
		if (tcp_conn->tc_fd)
			return tcp_conn;
		state= tcp_conn->tc_state;
		if (state != TCS_CLOSED)
		{
			tcp_close_connection(tcp_conn, ENOCONN);
		}
		return tcp_conn;
	}
	return NULL;
}

PRIVATE void read_ip_packets(tcp_port)
tcp_port_t *tcp_port;
{
	int result;

	do
	{
		tcp_port->tp_flags |= TPF_READ_IP;
		result= ip_read(tcp_port->tp_ipfd, TCP_MAX_DATAGRAM);
		if (result == NW_SUSPEND)
		{
			tcp_port->tp_flags |= TPF_READ_SP;
			return;
		}
		assert(result == NW_OK);
		tcp_port->tp_flags &= ~TPF_READ_IP;
	} while(!(tcp_port->tp_flags & TPF_READ_IP));
}

/*
find_best_conn
*/

PRIVATE tcp_conn_t *find_best_conn(ip_hdr, tcp_hdr)
ip_hdr_t *ip_hdr;
tcp_hdr_t *tcp_hdr;
{
	
	int best_level, new_level;
	tcp_conn_t *best_conn, *listen_conn, *tcp_conn;
	tcp_fd_t *tcp_fd;
	int i;
	ipaddr_t locaddr;
	ipaddr_t remaddr;
	tcpport_t locport;
	tcpport_t remport;

	locaddr= ip_hdr->ih_dst;
	remaddr= ip_hdr->ih_src;
	locport= tcp_hdr->th_dstport;
	remport= tcp_hdr->th_srcport;
	if (!remport)	/* This can interfere with a listen, so we reject it
			 * by clearing the requested port 
			 */
		locport= 0;
		
	best_level= 0;
	best_conn= NULL;
	listen_conn= NULL;
	for (i= ip_conf_nr, tcp_conn= tcp_conn_table+i;
		i<TCP_CONN_NR; i++, tcp_conn++)
		/* the first ip_conf_nr connections are reserved for
			RSTs */
	{
		if (!(tcp_conn->tc_flags & TCF_INUSE))
			continue;
		/* First fast check for open connections. */
		if (tcp_conn->tc_locaddr == locaddr && 
			tcp_conn->tc_locport == locport &&
			tcp_conn->tc_remport == remport &&
			tcp_conn->tc_remaddr == remaddr &&
			tcp_conn->tc_fd)
		{
			return tcp_conn;
		}

		/* Now check for listens and abandoned connections. */
		if (tcp_conn->tc_locaddr != locaddr)
		{
			continue;
		}
		new_level= 0;
		if (tcp_conn->tc_locport)
		{
			if (tcp_conn->tc_locport != locport)
			{
				continue;
			}
			new_level += 4;
		}
		if (tcp_conn->tc_remport)
		{
			if (tcp_conn->tc_remport != remport)
			{
				continue;
			}
			new_level += 1;
		}
		if (tcp_conn->tc_remaddr)
		{
			if (tcp_conn->tc_remaddr != remaddr)
			{
				continue;
			}
			new_level += 2;
		}
		if (new_level<best_level)
			continue;
		if (new_level != 7 && tcp_conn->tc_state != TCS_LISTEN)
			continue;
		if (new_level == 7 && !tcp_conn->tc_fd)
			/* We found an abandoned connection */
		{
			if (best_conn && tcp_Lmod4G(tcp_conn->tc_ISS,
				best_conn->tc_ISS))
			{
				continue;
			}
			best_conn= tcp_conn;
			continue;
		}
		if (!(tcp_hdr->th_flags & THF_SYN))
			continue;
		best_level= new_level;
		listen_conn= tcp_conn;
	}
	if (!best_conn && !listen_conn)
	{
		if ((tcp_hdr->th_flags & THF_SYN) &&
			maybe_listen(locaddr, locport, remaddr, remport))
		{
			/* Quick hack to implement listen back logs:
			 * if a SYN arrives and there is no listen waiting
			 * for that packet, then no reply is sent.
			 */
			return NULL;
		}

		for (i=0, tcp_conn= tcp_conn_table; i<ip_conf_nr;
			i++, tcp_conn++)
		{
			/* find valid port to send RST */
			if ((tcp_conn->tc_flags & TCF_INUSE) &&
				tcp_conn->tc_locaddr==locaddr)
			{
				break;
			}
		}
		assert (tcp_conn);
		assert (tcp_conn->tc_state == TCS_CLOSED);

		tcp_conn->tc_locport= locport;
		tcp_conn->tc_locaddr= locaddr;
		tcp_conn->tc_remport= remport;
		tcp_conn->tc_remaddr= remaddr;
		assert (!tcp_conn->tc_fd);
		return tcp_conn;
	}

	if (best_conn)
	{
		assert(!best_conn->tc_fd);
		if (!listen_conn)
			return best_conn;

		tcp_fd= listen_conn->tc_fd;
		assert(tcp_fd && listen_conn->tc_connInprogress &&
			tcp_fd->tf_conn == listen_conn);

		if (best_conn->tc_state != TCS_CLOSED)
			tcp_close_connection(best_conn, ENOCONN);

		listen_conn->tc_ISS= best_conn->tc_ISS;
		if (best_conn->tc_senddis > listen_conn->tc_senddis)
			listen_conn->tc_senddis= best_conn->tc_senddis;
		return listen_conn;
	}
	assert (listen_conn);
	return listen_conn;
}

/*
maybe_listen
*/
PRIVATE int maybe_listen(locaddr, locport, remaddr, remport)
ipaddr_t locaddr;
tcpport_t locport;
ipaddr_t remaddr;
tcpport_t remport;
{
	int i;
	tcp_conn_t *tcp_conn;
	tcp_fd_t *fd;

	for (i= ip_conf_nr, tcp_conn= tcp_conn_table+i;
		i<TCP_CONN_NR; i++, tcp_conn++)
	{
		if (!(tcp_conn->tc_flags & TCF_INUSE))
			continue;

		if (tcp_conn->tc_locaddr != locaddr)
		{
			continue;
		}
		if (tcp_conn->tc_locport != locport )
		{
			continue;
		}
		if (!tcp_conn->tc_orglisten)
			continue;
		fd= tcp_conn->tc_fd;
		if (!fd)
			continue;
		if ((fd->tf_tcpconf.nwtc_flags & NWTC_SET_RP) &&
			tcp_conn->tc_remport != remport)
		{
			continue;
		}
		if ((fd->tf_tcpconf.nwtc_flags & NWTC_SET_RA) &&
			tcp_conn->tc_remaddr != remaddr)
		{
			continue;
		}
		if (!(fd->tf_flags & TFF_DEL_RST))
			continue;
		return 1;

	}
	return 0;
}


PUBLIC void tcp_reply_ioctl(tcp_fd, reply)
tcp_fd_t *tcp_fd;
int reply;
{
	assert (tcp_fd->tf_flags & TFF_IOCTL_IP);
	assert (tcp_fd->tf_ioreq == NWIOTCPSHUTDOWN ||
		tcp_fd->tf_ioreq == NWIOTCPLISTEN ||
		tcp_fd->tf_ioreq == NWIOTCPCONN);
	
	tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
	reply_thr_get (tcp_fd, reply, TRUE);
}

PUBLIC void tcp_reply_write(tcp_fd, reply)
tcp_fd_t *tcp_fd;
size_t reply;
{
	assert (tcp_fd->tf_flags & TFF_WRITE_IP);

	tcp_fd->tf_flags &= ~TFF_WRITE_IP;
	reply_thr_get (tcp_fd, reply, FALSE);
}

PUBLIC void tcp_reply_read(tcp_fd, reply)
tcp_fd_t *tcp_fd;
size_t reply;
{
	assert (tcp_fd->tf_flags & TFF_READ_IP);

	tcp_fd->tf_flags &= ~TFF_READ_IP;
	reply_thr_put (tcp_fd, reply, FALSE);
}

PUBLIC int tcp_write(fd, count)
int fd;
size_t count;
{
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tcp_conn;

	tcp_fd= &tcp_fd_table[fd];

	assert (tcp_fd->tf_flags & TFF_INUSE);

	if (!(tcp_fd->tf_flags & TFF_CONNECTED))
	{
		reply_thr_get (tcp_fd, ENOTCONN, FALSE);
		return NW_OK;
	}
	tcp_conn= tcp_fd->tf_conn;
	if (tcp_conn->tc_state == TCS_CLOSED)
	{
		reply_thr_get(tcp_fd, tcp_conn->tc_error, FALSE);
		return NW_OK;
	}
	if (tcp_conn->tc_flags & TCF_FIN_SENT)
	{
		reply_thr_get (tcp_fd, ESHUTDOWN, FALSE);
		return NW_OK;
	}

	tcp_fd->tf_flags |= TFF_WRITE_IP;
	tcp_fd->tf_write_offset= 0;
	tcp_fd->tf_write_count= count;

	/* Start the timer (if necessary) */
	if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT &&
		tcp_conn->tc_transmit_seq == tcp_conn->tc_SND_UNA)
	{
		tcp_set_send_timer(tcp_conn);
	}
	assert(tcp_conn->tc_transmit_timer.tim_active ||
		(tcp_print_conn(tcp_conn), printf("\n"), 0));

	assert(tcp_conn->tc_busy == 0);
	tcp_conn->tc_busy++;
	tcp_fd_write(tcp_conn);
	tcp_conn->tc_busy--;
	tcp_conn_write(tcp_conn, 0);

	if (!(tcp_fd->tf_flags & TFF_WRITE_IP))
		return NW_OK;
	else
		return NW_SUSPEND;
}

PUBLIC int
tcp_read(fd, count)
int fd;
size_t count;
{
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tcp_conn;

	tcp_fd= &tcp_fd_table[fd];

	assert (tcp_fd->tf_flags & TFF_INUSE);

	if (!(tcp_fd->tf_flags & TFF_CONNECTED))
	{
		reply_thr_put (tcp_fd, ENOTCONN, FALSE);
		return NW_OK;
	}
	tcp_conn= tcp_fd->tf_conn;

	tcp_fd->tf_flags |= TFF_READ_IP;
	tcp_fd->tf_read_offset= 0;
	tcp_fd->tf_read_count= count;

	assert(tcp_conn->tc_busy == 0);
	tcp_conn->tc_busy++;
	tcp_fd_read(tcp_conn, 0);
	tcp_conn->tc_busy--;
	if (!(tcp_fd->tf_flags & TFF_READ_IP))
		return NW_OK;
	else
		return NW_SUSPEND;
}

/*
tcp_restart_connect

reply the success or failure of a connect to the user.
*/


PUBLIC void tcp_restart_connect(tcp_fd)
tcp_fd_t *tcp_fd;
{
	tcp_conn_t *tcp_conn;
	int reply;

	assert(tcp_fd);
	assert(tcp_fd->tf_flags & TFF_IOCTL_IP);
	assert(tcp_fd->tf_ioreq == NWIOTCPLISTEN ||
		tcp_fd->tf_ioreq == NWIOTCPCONN);

	tcp_conn= tcp_fd->tf_conn;

	assert(tcp_conn);
	assert(tcp_conn->tc_connInprogress);

	if (tcp_conn->tc_state == TCS_CLOSED)
	{
		reply= tcp_conn->tc_error;
		assert(tcp_conn->tc_fd == tcp_fd);
		tcp_fd->tf_conn= NULL;
		tcp_conn->tc_fd= NULL;
	}
	else
	{
		tcp_fd->tf_flags |= TFF_CONNECTED;
		reply= NW_OK;
	}
	tcp_conn->tc_connInprogress= 0;
	tcp_reply_ioctl (tcp_fd, reply);
}

/*
tcp_close
*/

PUBLIC void tcp_close(fd)
int fd;
{
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tcp_conn;

	tcp_fd= &tcp_fd_table[fd];

	assert (tcp_fd->tf_flags & TFF_INUSE);
	assert (!(tcp_fd->tf_flags &
		(TFF_IOCTL_IP|TFF_READ_IP|TFF_WRITE_IP)));

	tcp_fd->tf_flags &= ~TFF_INUSE;
	if (!tcp_fd->tf_conn)
		return;

	tcp_conn= tcp_fd->tf_conn;
	assert(tcp_conn->tc_fd == tcp_fd);
	tcp_conn->tc_fd= 0;

	assert (!tcp_conn->tc_connInprogress);

	tcp_shutdown (tcp_conn);
	if (tcp_conn->tc_state == TCS_ESTABLISHED)
	{
		tcp_conn->tc_state= TCS_CLOSING;
	}

	/* Set the retransmission timeout a bit smaller. */
	tcp_conn->tc_rt_dead= TCP_DEF_RT_MAX_CLOSING;

	/* If all data has been acknowledged, close the connection. */
	if (tcp_conn->tc_SND_UNA == tcp_conn->tc_SND_NXT)
		tcp_close_connection(tcp_conn, ENOTCONN);
}

PUBLIC int tcp_cancel(fd, which_operation)
int fd;
int which_operation;
{
	tcp_fd_t *tcp_fd;
	tcp_conn_t *tcp_conn;
	int i;

	tcp_fd= &tcp_fd_table[fd];

	assert (tcp_fd->tf_flags & TFF_INUSE);

	tcp_conn= tcp_fd->tf_conn;

	switch (which_operation)
	{
	case SR_CANCEL_WRITE:
		assert (tcp_fd->tf_flags & TFF_WRITE_IP);
		tcp_fd->tf_flags &= ~TFF_WRITE_IP;

		if (tcp_fd->tf_write_offset)
			reply_thr_get (tcp_fd, tcp_fd->tf_write_offset, FALSE);
		else
			reply_thr_get (tcp_fd, EINTR, FALSE);
		break;
	case SR_CANCEL_READ:
		assert (tcp_fd->tf_flags & TFF_READ_IP);
		tcp_fd->tf_flags &= ~TFF_READ_IP;
		if (tcp_fd->tf_read_offset)
			reply_thr_put (tcp_fd, tcp_fd->tf_read_offset, FALSE);
		else
			reply_thr_put (tcp_fd, EINTR, FALSE);
		break;
	case SR_CANCEL_IOCTL:
assert (tcp_fd->tf_flags & TFF_IOCTL_IP);
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;

		switch (tcp_fd->tf_ioreq)
		{
		case NWIOGTCPCONF:
			reply_thr_put (tcp_fd, EINTR, TRUE);
			break;
		case NWIOSTCPCONF:
		case NWIOTCPSHUTDOWN:
			reply_thr_get (tcp_fd, EINTR, TRUE);
			break;
		case NWIOTCPCONN:
		case NWIOTCPLISTEN:
			assert (tcp_conn->tc_connInprogress);
			tcp_conn->tc_connInprogress= 0;
			tcp_conn->tc_fd= 0;
			tcp_fd->tf_conn= 0;
			tcp_close_connection(tcp_conn, ENOCONN);
			reply_thr_get (tcp_fd, EINTR, TRUE);
			break;
		default:
			ip_warning(( "unknown ioctl inprogress: 0x%x",
				tcp_fd->tf_ioreq ));
			reply_thr_get (tcp_fd, EINTR, TRUE);
			break;
		}
		break;
#if !CRAMPED
	default:
		ip_panic(( "unknown cancel request" ));
#endif
	}
	return NW_OK;
}

/*
tcp_connect
*/

PRIVATE int tcp_connect(tcp_fd)
tcp_fd_t *tcp_fd;
{
	tcp_conn_t *tcp_conn;
	int state;

	if (!(tcp_fd->tf_flags & TFF_CONF_SET))
	{
		tcp_reply_ioctl(tcp_fd, EBADMODE);
		return NW_OK;
	}
	if (tcp_fd->tf_flags & TFF_CONNECT)
	{
		tcp_reply_ioctl(tcp_fd, EISCONN);
		return NW_OK;
	}
	if ((tcp_fd->tf_tcpconf.nwtc_flags & (NWTC_SET_RA|NWTC_SET_RP))
		!= (NWTC_SET_RA|NWTC_SET_RP))
	{
		tcp_reply_ioctl(tcp_fd, EBADMODE);
		return NW_OK;
	}

	assert(!tcp_fd->tf_conn);
	tcp_conn= find_conn_entry(tcp_fd->tf_tcpconf.nwtc_locport,
		tcp_fd->tf_port->tp_ipaddr,
		tcp_fd->tf_tcpconf.nwtc_remport,
		tcp_fd->tf_tcpconf.nwtc_remaddr);
	if (tcp_conn)
	{
		if (tcp_conn->tc_fd)
		{
			tcp_reply_ioctl(tcp_fd, EADDRINUSE);
			return NW_OK;
		}
	}
	else
	{
		tcp_conn= find_empty_conn();
		if (!tcp_conn)
		{
			tcp_reply_ioctl(tcp_fd, EAGAIN);
			return NW_OK;
		}
	}
	tcp_fd->tf_conn= tcp_conn;

	return tcp_su4connect(tcp_fd);
}

/*
tcp_su4connect
*/

PRIVATE int tcp_su4connect(tcp_fd)
tcp_fd_t *tcp_fd;
{
	tcp_conn_t *tcp_conn;
	acc_t *tmp_acc;

	tcp_conn= tcp_fd->tf_conn;

	tcp_conn->tc_locport= tcp_fd->tf_tcpconf.nwtc_locport;
	tcp_conn->tc_locaddr= tcp_fd->tf_port->tp_ipaddr;

	assert (tcp_fd->tf_tcpconf.nwtc_flags & NWTC_SET_RP);
	assert (tcp_fd->tf_tcpconf.nwtc_flags & NWTC_SET_RA);
	tcp_conn->tc_remport= tcp_fd->tf_tcpconf.nwtc_remport;
	tcp_conn->tc_remaddr= tcp_fd->tf_tcpconf.nwtc_remaddr;

	tcp_setup_conn(tcp_conn);

	tcp_conn->tc_fd= tcp_fd;
	tcp_conn->tc_port= tcp_fd->tf_port;
	tcp_conn->tc_connInprogress= 1;
	tcp_conn->tc_orglisten= FALSE;
	tcp_conn->tc_state= TCS_SYN_SENT;
	tcp_conn->tc_rt_dead= TCP_DEF_RT_MAX_CONNECT;

	/* Start the timer (if necessary) */
	tcp_set_send_timer(tcp_conn);

	tcp_conn_write(tcp_conn, 0);

	if (tcp_conn->tc_connInprogress)
		return NW_SUSPEND;
	else
		return NW_OK;
}

PRIVATE int conn_right4fd(tcp_conn, tcp_fd)
tcp_fd_t *tcp_fd;
tcp_conn_t *tcp_conn;
{
	unsigned long flags;

	flags= tcp_fd->tf_tcpconf.nwtc_flags;

	if (tcp_fd->tf_tcpconf.nwtc_locport != tcp_conn->tc_locport)
		return FALSE;

	if ((flags & NWTC_SET_RA) && tcp_fd->tf_tcpconf.nwtc_remaddr !=
		tcp_conn->tc_remaddr)
		return FALSE;

	if ((flags & NWTC_SET_RP) && tcp_fd->tf_tcpconf.nwtc_remport !=
		tcp_conn->tc_remport)
		return FALSE;

	if (tcp_fd->tf_port != tcp_conn->tc_port)
		return FALSE;

	return TRUE;
}

/*
tcp_listen
*/

PRIVATE int tcp_listen(tcp_fd)
tcp_fd_t *tcp_fd;
{
	tcp_conn_t *tcp_conn;
	int state;

	if (!(tcp_fd->tf_flags & TFF_CONF_SET))
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EBADMODE, TRUE);
		return NW_OK;
	}
	if (tcp_fd->tf_flags & TFF_CONNECT)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get(tcp_fd, EISCONN, TRUE);
		return NW_OK;
	}
	tcp_conn= tcp_fd->tf_conn;
	assert(!tcp_conn);

	if ((tcp_fd->tf_tcpconf.nwtc_flags & (NWTC_SET_RA|NWTC_SET_RP))
		== (NWTC_SET_RA|NWTC_SET_RP))
	{
		tcp_conn= find_conn_entry(
			tcp_fd->tf_tcpconf.nwtc_locport,
			tcp_fd->tf_port->tp_ipaddr,
			tcp_fd->tf_tcpconf.nwtc_remport,
			tcp_fd->tf_tcpconf.nwtc_remaddr);
		if (tcp_conn)
		{
			if (tcp_conn->tc_fd)
			{
				tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
				reply_thr_get (tcp_fd, EADDRINUSE, TRUE);
				return NW_OK;
			}
			tcp_fd->tf_conn= tcp_conn;
			return tcp_su4listen(tcp_fd);
		}
	}
	tcp_conn= find_empty_conn();
	if (!tcp_conn)
	{
		tcp_fd->tf_flags &= ~TFF_IOCTL_IP;
		reply_thr_get (tcp_fd, EAGAIN, TRUE);
		return NW_OK;
	}
	tcp_fd->tf_conn= tcp_conn;
	return tcp_su4listen(tcp_fd);
}

PRIVATE void tcp_buffree (priority)
int priority;
{
	int i;
	tcp_conn_t *tcp_conn;

	if (priority == TCP_PRI_FRAG2SEND)
	{
		for (i=0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++,
			tcp_conn++)
		{
			if (!(tcp_conn->tc_flags & TCF_INUSE))
				continue;
			if (!tcp_conn->tc_frag2send)
				continue;
			if (tcp_conn->tc_busy)
				continue;
			bf_afree(tcp_conn->tc_frag2send);
			tcp_conn->tc_frag2send= 0;
		}
	}

	if (priority == TCP_PRI_CONN_EXTRA)
	{
		for (i=0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++,
			tcp_conn++)
		{
			if (!(tcp_conn->tc_flags & TCF_INUSE))
				continue;
			if (tcp_conn->tc_busy)
				continue;
			if (tcp_conn->tc_adv_data)
			{
				bf_afree(tcp_conn->tc_adv_data);
				tcp_conn->tc_adv_data= NULL;
			}
		}
	}

	if (priority == TCP_PRI_CONNwoUSER)
	{
		for (i=0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++,
			tcp_conn++)
		{
			if (!(tcp_conn->tc_flags & TCF_INUSE))
				continue;
			if (tcp_conn->tc_busy)
				continue;
			if (tcp_conn->tc_fd)
				continue;
			if (tcp_conn->tc_state == TCS_CLOSED)
				continue;
			if (tcp_conn->tc_rcvd_data == NULL &&
				tcp_conn->tc_send_data == NULL)
			{
				continue;
			}
			tcp_close_connection (tcp_conn, EOUTOFBUFS);
		}
	}

	if (priority == TCP_PRI_CONN_INUSE)
	{
		for (i=0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++,
			tcp_conn++)
		{
			if (!(tcp_conn->tc_flags & TCF_INUSE))
				continue;
			if (tcp_conn->tc_busy)
				continue;
			if (tcp_conn->tc_state == TCS_CLOSED)
				continue;
			if (tcp_conn->tc_rcvd_data == NULL &&
				tcp_conn->tc_send_data == NULL)
			{
				continue;
			}
			tcp_close_connection (tcp_conn, EOUTOFBUFS);
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
PRIVATE void tcp_bufcheck()
{
	int i;
	tcp_conn_t *tcp_conn;
	tcp_port_t *tcp_port;

	for (i= 0, tcp_port= tcp_port_table; i<ip_conf_nr; i++, tcp_port++)
	{
		if (tcp_port->tp_pack)
			bf_check_acc(tcp_port->tp_pack);
	}
	for (i= 0, tcp_conn= tcp_conn_table; i<TCP_CONN_NR; i++, tcp_conn++)
	{
		assert(!tcp_conn->tc_busy);
		if (tcp_conn->tc_rcvd_data)
			bf_check_acc(tcp_conn->tc_rcvd_data);
		if (tcp_conn->tc_adv_data)
			bf_check_acc(tcp_conn->tc_adv_data);
		if (tcp_conn->tc_send_data)
			bf_check_acc(tcp_conn->tc_send_data);
		if (tcp_conn->tc_remipopt)
			bf_check_acc(tcp_conn->tc_remipopt);
		if (tcp_conn->tc_tcpopt)
			bf_check_acc(tcp_conn->tc_tcpopt);
		if (tcp_conn->tc_frag2send)
			bf_check_acc(tcp_conn->tc_frag2send);
	}
}
#endif

PUBLIC void tcp_notreach(tcp_conn)
tcp_conn_t *tcp_conn;
{
	int new_ttl;

	new_ttl= tcp_conn->tc_ttl;
	if (new_ttl == IP_MAX_TTL)
	{
		if (tcp_conn->tc_state == TCS_SYN_SENT)
			tcp_close_connection(tcp_conn, EDSTNOTRCH);
		return;
	}
	else if (new_ttl == TCP_DEF_TTL)
		new_ttl= TCP_DEF_TTL_NEXT;
	else
	{
		new_ttl *= 2;
		if (new_ttl> IP_MAX_TTL)
			new_ttl= IP_MAX_TTL;
	}
	tcp_conn->tc_ttl= new_ttl;
	tcp_conn->tc_stt= 0;
	tcp_conn->tc_SND_TRM= tcp_conn->tc_SND_UNA;
	tcp_conn_write(tcp_conn, 1);
}

/*
tcp_setup_conn
*/

PRIVATE void tcp_setup_conn(tcp_conn)
tcp_conn_t *tcp_conn;
{
	assert(!tcp_conn->tc_connInprogress);
	if (tcp_conn->tc_flags & TCF_INUSE)
	{
		assert (tcp_conn->tc_state == TCS_CLOSED);
		assert (!tcp_conn->tc_send_data);
		if (tcp_conn->tc_senddis < get_time())
			tcp_conn->tc_ISS= 0;
	}
	else
	{
		assert(!tcp_conn->tc_busy);
		tcp_conn->tc_senddis= 0;
		tcp_conn->tc_ISS= 0;
		tcp_conn->tc_tos= TCP_DEF_TOS;
		tcp_conn->tc_ttl= TCP_DEF_TTL;
		tcp_conn->tc_rcv_wnd= TCP_MAX_RCV_WND_SIZE;
		tcp_conn->tc_fd= NULL;
	}
	if (!tcp_conn->tc_ISS)
	{
		tcp_conn->tc_ISS= (get_time()/HZ)*ISS_INC_FREQ;
	}
	tcp_conn->tc_SND_UNA= tcp_conn->tc_ISS;
	tcp_conn->tc_SND_TRM= tcp_conn->tc_ISS;
	tcp_conn->tc_SND_NXT= tcp_conn->tc_ISS+1;
	tcp_conn->tc_SND_UP= tcp_conn->tc_ISS;
	tcp_conn->tc_SND_PSH= tcp_conn->tc_ISS;
	tcp_conn->tc_IRS= 0;
	tcp_conn->tc_RCV_LO= tcp_conn->tc_IRS;
	tcp_conn->tc_RCV_NXT= tcp_conn->tc_IRS;
	tcp_conn->tc_RCV_HI= tcp_conn->tc_IRS;
	tcp_conn->tc_RCV_UP= tcp_conn->tc_IRS;

	assert(tcp_conn->tc_rcvd_data == NULL);
	assert(tcp_conn->tc_adv_data == NULL);
	assert(tcp_conn->tc_send_data == NULL);
	tcp_conn->tc_remipopt= NULL;
	tcp_conn->tc_tcpopt= NULL;

	assert(tcp_conn->tc_frag2send == NULL);

	tcp_conn->tc_stt= 0;
	tcp_conn->tc_rt_dead= TCP_DEF_RT_DEAD;
	tcp_conn->tc_0wnd_to= 0;
	tcp_conn->tc_rtt= TCP_DEF_RTT;
	tcp_conn->tc_mss= TCP_DEF_MSS;
	tcp_conn->tc_error= NW_OK;
	tcp_conn->tc_snd_cwnd= tcp_conn->tc_SND_UNA + 2*tcp_conn->tc_mss;
	tcp_conn->tc_snd_cthresh= TCP_MAX_SND_WND_SIZE;
	tcp_conn->tc_snd_cinc=
		(long)TCP_DEF_MSS*TCP_DEF_MSS/TCP_MAX_SND_WND_SIZE+1;
	tcp_conn->tc_snd_wnd= TCP_MAX_SND_WND_SIZE;
	tcp_conn->tc_rt_time= 0;
	tcp_conn->tc_rt_seq= 0;
	tcp_conn->tc_rt_threshold= tcp_conn->tc_ISS;
	tcp_conn->tc_flags= TCF_INUSE;

	clck_untimer(&tcp_conn->tc_transmit_timer);
	tcp_conn->tc_transmit_seq= 0;
}

/*
 * $PchId: tcp.c,v 1.14.2.2 1999/11/17 22:05:27 philip Exp $
 */

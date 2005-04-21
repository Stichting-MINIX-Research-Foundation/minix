/*
tcp_int.h

Copyright 1995 Philip Homburg
*/

#ifndef TCP_INT_H
#define TCP_INT_H

#define TCP_CONN_HASH_SHIFT	4
#define TCP_CONN_HASH_NR	(1 << TCP_CONN_HASH_SHIFT)

typedef struct tcp_port
{
	int tp_ipdev;
	int tp_flags;
	int tp_state;
	int tp_ipfd;
	acc_t *tp_pack;
	ipaddr_t tp_ipaddr;
	struct tcp_conn *tp_snd_head;
	struct tcp_conn *tp_snd_tail;
	event_t tp_snd_event;
	struct tcp_conn *tp_conn_hash[TCP_CONN_HASH_NR][4];
} tcp_port_t;

#define TPF_EMPTY	0x0
#define TPF_SUSPEND	0x1
#define TPF_READ_IP	0x2
#define TPF_READ_SP	0x4
#define TPF_WRITE_IP	0x8
#define TPF_WRITE_SP	0x10
#define TPF_DELAY_TCP	0x40

#define TPS_EMPTY	0
#define TPS_SETPROTO	1
#define TPS_GETCONF	2
#define TPS_MAIN	3
#define TPS_ERROR	4

typedef struct tcp_fd
{
	int tf_flags;
	tcp_port_t *tf_port;
	int tf_srfd;
	ioreq_t tf_ioreq;
	nwio_tcpconf_t tf_tcpconf;
	nwio_tcpopt_t tf_tcpopt;
	get_userdata_t tf_get_userdata;
	put_userdata_t tf_put_userdata;
	struct tcp_conn *tf_conn;
	size_t tf_write_offset;
	size_t tf_write_count;
	size_t tf_read_offset;
	size_t tf_read_count;
} tcp_fd_t;

#define TFF_EMPTY	   0x0
#define TFF_INUSE	   0x1
#define TFF_IOCTL_IP	   0x2
#define TFF_CONF_SET	   0x4
#define TFF_IOC_INIT_SP	   0x8
#define TFF_CONNECT	  0x20
#define TFF_WRITE_IP	  0x80
#define TFF_WR_URG	 0x100
#define TFF_PUSH_DATA	 0x200
#define TFF_READ_IP	 0x400
#define TFF_RECV_URG	 0x800
#define TFF_CONNECTED	0x1000
#define TFF_DEL_RST	0x2000

typedef struct tcp_conn
{
	int tc_flags;
	int tc_state;
	int tc_busy;		/* do not steal buffer when a counnection is 
				 * busy
				 */
	tcp_port_t *tc_port;
	tcp_fd_t *tc_fd;

	tcpport_t tc_locport;
	ipaddr_t tc_locaddr;
	tcpport_t tc_remport;
	ipaddr_t tc_remaddr;

#if 1
	int tc_connInprogress;
#endif
	int tc_orglisten;
	time_t tc_senddis;

	/* Sending side */
	u32_t tc_ISS;		/* initial sequence number */
	u32_t tc_SND_UNA;	/* least unacknowledged sequence number */
	u32_t tc_SND_TRM;	/* next sequence number to be transmitted */
	u32_t tc_SND_NXT;	/* next sequence number for new data */
	u32_t tc_SND_UP;	/* urgent pointer, first sequence number not 
				 * urgent */
	u32_t tc_SND_PSH;	/* push pointer, data should be pushed until
				 * the push pointer is reached */

	u32_t tc_snd_cwnd;	/* highest sequence number to be sent */
	u32_t tc_snd_cthresh;	/* threshold for send window */
	u32_t tc_snd_cinc;	/* increment for send window threshold */
	u16_t tc_snd_wnd;	/* max send queue size */

	/* round trip calculation. */
	time_t tc_rt_time;
	u32_t tc_rt_seq;
	u32_t tc_rt_threshold;
	time_t tc_rtt;

	acc_t *tc_send_data;
	acc_t *tc_frag2send;
	struct tcp_conn *tc_send_link;

	/* Receiving side */
	u32_t tc_IRS;
	u32_t tc_RCV_LO;
	u32_t tc_RCV_NXT;
	u32_t tc_RCV_HI;
	u32_t tc_RCV_UP;

	u16_t tc_rcv_wnd;
	acc_t *tc_rcvd_data;
	acc_t *tc_adv_data;
	u32_t tc_adv_seq;

	acc_t *tc_remipopt;
	acc_t *tc_tcpopt;
	u8_t tc_tos;
	u8_t tc_ttl;
	u16_t tc_mss;

	struct timer tc_transmit_timer;
	u32_t tc_transmit_seq;
	time_t tc_0wnd_to;
	time_t tc_stt;		/* time of first send after last ack */
	time_t tc_rt_dead;

	int tc_error;
	int tc_inconsistent; 
} tcp_conn_t;

#define TCF_EMPTY		0x0
#define TCF_INUSE		0x1
#define TCF_FIN_RECV		0x2
#define TCF_RCV_PUSH		0x4
#define TCF_MORE2WRITE		0x8
#define TCF_SEND_ACK		0x10
#define TCF_FIN_SENT		0x20
#define TCF_BSD_URG		0x40

#if DEBUG & 0x200
#define TCF_DEBUG		0x1000
#endif

#define TCS_CLOSED		0
#define TCS_LISTEN		1
#define TCS_SYN_RECEIVED	2
#define TCS_SYN_SENT		3
#define TCS_ESTABLISHED		4
#define TCS_CLOSING		5

/* tcp_recv.c */
void tcp_frag2conn ARGS(( tcp_conn_t *tcp_conn, ip_hdr_t *ip_hdr,
	tcp_hdr_t *tcp_hdr, acc_t *tcp_data, size_t data_len ));
void tcp_fd_read ARGS(( tcp_conn_t *tcp_conn, int enq ));

/* tcp_send.c */
void tcp_conn_write ARGS(( tcp_conn_t *tcp_conn, int enq ));
void tcp_release_retrans ARGS(( tcp_conn_t *tcp_conn, u32_t seg_ack,
	U16_t new_win ));
void tcp_set_send_timer ARGS(( tcp_conn_t *tcp_conn ));
void tcp_fd_write ARGS(( tcp_conn_t *tcp_conn ));
void tcp_close_connection ARGS(( tcp_conn_t *tcp_conn,
	int error ));
void tcp_port_write ARGS(( tcp_port_t *tcp_port ));
void tcp_shutdown ARGS(( tcp_conn_t *tcp_conn ));

/* tcp_lib.c */
void tcp_extract_ipopt ARGS(( tcp_conn_t *tcp_conn,
	ip_hdr_t *ip_hdr ));
void tcp_extract_tcpopt ARGS(( tcp_conn_t *tcp_conn,
	tcp_hdr_t *tcp_hdr ));
void tcp_get_ipopt ARGS(( tcp_conn_t *tcp_conn, ip_hdropt_t
	*ip_hdropt ));
void tcp_get_tcpopt ARGS(( tcp_conn_t *tcp_conn, tcp_hdropt_t
	*tcp_hdropt ));
acc_t *tcp_make_header ARGS(( tcp_conn_t *tcp_conn,
	ip_hdr_t **ref_ip_hdr, tcp_hdr_t **ref_tcp_hdr, acc_t *data ));
u16_t tcp_pack_oneCsum ARGS(( ip_hdr_t *ip_hdr, acc_t *tcp_pack ));
int tcp_check_conn ARGS(( tcp_conn_t *tcp_conn ));
void tcp_print_pack ARGS(( ip_hdr_t *ip_hdr, tcp_hdr_t *tcp_hdr ));
void tcp_print_state ARGS(( tcp_conn_t *tcp_conn ));
void tcp_print_conn ARGS(( tcp_conn_t *tcp_conn ));
int tcp_LEmod4G ARGS(( u32_t n1, u32_t n2 ));
int tcp_Lmod4G ARGS(( u32_t n1, u32_t n2 ));
int tcp_GEmod4G ARGS(( u32_t n1, u32_t n2 ));
int tcp_Gmod4G ARGS(( u32_t n1, u32_t n2 ));

/* tcp.c */
void tcp_restart_connect ARGS(( tcp_fd_t *tcp_fd ));
int tcp_su4listen ARGS(( tcp_fd_t *tcp_fd ));
void tcp_reply_ioctl ARGS(( tcp_fd_t *tcp_fd, int reply ));
void tcp_reply_write ARGS(( tcp_fd_t *tcp_fd, size_t reply ));
void tcp_reply_read ARGS(( tcp_fd_t *tcp_fd, size_t reply ));
void tcp_notreach ARGS(( tcp_conn_t *tcp_conn ));

#define TCP_FD_NR	(10*IP_PORT_MAX)
#define TCP_CONN_NR	(2*TCP_FD_NR)

EXTERN tcp_port_t *tcp_port_table;
EXTERN tcp_conn_t tcp_conn_table[TCP_CONN_NR];
EXTERN tcp_fd_t tcp_fd_table[TCP_FD_NR];

#define tcp_Lmod4G(n1,n2)	(!!(((n1)-(n2)) & 0x80000000L))
#define tcp_GEmod4G(n1,n2)	(!(((n1)-(n2)) & 0x80000000L))
#define tcp_Gmod4G(n1,n2)	(!!(((n2)-(n1)) & 0x80000000L))
#define tcp_LEmod4G(n1,n2)	(!(((n2)-(n1)) & 0x80000000L))

#endif /* TCP_INT_H */

/*
 * $PchId: tcp_int.h,v 1.10 1996/05/07 20:51:59 philip Exp $
 */

/*
ip_int.h

Copyright 1995 Philip Homburg
*/

#ifndef INET_IP_INT_H
#define INET_IP_INT_H

#define IP_FD_NR	(8*IP_PORT_MAX)
#define IP_ASS_NR	3

#define IP_42BSD_BCAST		1	/* hostnumber 0 is also network
					   broadcast */

#define IP_LT_NORMAL		0	/* Normal */
#define IP_LT_BROADCAST		1	/* Broadcast */
#define IP_LT_MULTICAST		2	/* Multicast */

struct ip_port;
struct ip_fd;
typedef void (*ip_dev_t) ARGS(( struct ip_port *ip_port ));
typedef int (*ip_dev_send_t) ARGS(( struct ip_port *ip_port, ipaddr_t dest, 
						acc_t *pack, int type ));

#define IP_PROTO_HASH_NR	32

typedef struct ip_port
{
	int ip_flags, ip_dl_type;
	int ip_port;
	union
	{
		struct
		{
			int de_state;
			int de_flags;
			int de_port;
			int de_fd;
			acc_t *de_frame;
			acc_t *de_q_head;
			acc_t *de_q_tail;
			acc_t *de_arp_head;
			acc_t *de_arp_tail;
		} dl_eth;
		struct
		{
			int ps_port;
			acc_t *ps_send_head;
			acc_t *ps_send_tail;
		} dl_ps;
	} ip_dl;
	ipaddr_t ip_ipaddr;
	ipaddr_t ip_subnetmask;
	ipaddr_t ip_classfulmask;
	u16_t ip_frame_id;
	u16_t ip_mtu;
	u16_t ip_mtu_max;		/* Max MTU for this kind of network */
	ip_dev_t ip_dev_main;
	ip_dev_t ip_dev_set_ipaddr;
	ip_dev_send_t ip_dev_send;
	acc_t *ip_loopb_head;
	acc_t *ip_loopb_tail;
	event_t ip_loopb_event;
	acc_t *ip_routeq_head;
	acc_t *ip_routeq_tail;
	event_t ip_routeq_event;
	struct ip_fd *ip_proto_any;
	struct ip_fd *ip_proto[IP_PROTO_HASH_NR];
} ip_port_t;

#define IES_EMPTY	0x0
#define	IES_SETPROTO	0x1
#define	IES_GETIPADDR	0x2
#define	IES_MAIN	0x3
#define	IES_ERROR	0x4

#define IEF_EMPTY	0x1
#define IEF_SUSPEND	0x8
#define IEF_READ_IP	0x10
#define IEF_READ_SP	0x20
#define IEF_WRITE_SP	0x80

#define IPF_EMPTY		0x0
#define IPF_CONFIGURED		0x1
#define IPF_IPADDRSET		0x2
#define IPF_NETMASKSET		0x4
#define IPF_SUBNET_BCAST	0x8	/* Subset support subnet broadcasts  */

#define IPDL_ETH	NETTYPE_ETH
#define IPDL_PSIP	NETTYPE_PSIP

typedef struct ip_ass
{
	acc_t *ia_frags;
	int ia_min_ttl;
	ip_port_t *ia_port;
	time_t ia_first_time;
	ipaddr_t ia_srcaddr, ia_dstaddr;
	int ia_proto, ia_id;
} ip_ass_t;

typedef struct ip_fd
{
	int if_flags;
	struct nwio_ipopt if_ipopt;
	ip_port_t *if_port;
	struct ip_fd *if_proto_next;
	int if_srfd;
	acc_t *if_rdbuf_head;
	acc_t *if_rdbuf_tail;
	get_userdata_t if_get_userdata;
	put_userdata_t if_put_userdata;
	put_pkt_t if_put_pkt;
	time_t if_exp_time;
	size_t if_rd_count;
	ioreq_t if_ioctl;
} ip_fd_t;

#define IFF_EMPTY	0x00
#define IFF_INUSE	0x01
#define IFF_OPTSET	0x02
#define IFF_BUSY	0x1C
#	define IFF_READ_IP	0x04
#	define IFF_IOCTL_IP	0x08

typedef enum nettype
{
	IPNT_ZERO,		/*   0.xx.xx.xx */
	IPNT_CLASS_A,		/*   1.xx.xx.xx .. 126.xx.xx.xx */
	IPNT_LOCAL,		/* 127.xx.xx.xx */
	IPNT_CLASS_B,		/* 128.xx.xx.xx .. 191.xx.xx.xx */
	IPNT_CLASS_C,		/* 192.xx.xx.xx .. 223.xx.xx.xx */
	IPNT_CLASS_D,		/* 224.xx.xx.xx .. 239.xx.xx.xx */
	IPNT_CLASS_E,		/* 240.xx.xx.xx .. 247.xx.xx.xx */
	IPNT_MARTIAN,		/* 248.xx.xx.xx .. 254.xx.xx.xx + others */
	IPNT_BROADCAST		/* 255.255.255.255 */
} nettype_t;

struct nwio_ipconf;

/* ip_eth.c */
int ipeth_init ARGS(( ip_port_t *ip_port ));

/* ip_ioctl.c */
void ip_hash_proto ARGS(( ip_fd_t *ip_fd ));
void ip_unhash_proto ARGS(( ip_fd_t *ip_fd ));
int ip_setconf ARGS(( int ip_port, struct nwio_ipconf *ipconfp ));

/* ip_lib.c */
ipaddr_t ip_get_netmask ARGS(( ipaddr_t hostaddr ));
ipaddr_t ip_get_ifaddr ARGS(( int ip_port_nr ));
int ip_chk_hdropt ARGS(( u8_t *opt, int optlen ));
void ip_print_frags ARGS(( acc_t *acc ));
nettype_t ip_nettype ARGS(( ipaddr_t ipaddr ));
ipaddr_t ip_netmask ARGS(( nettype_t nettype ));
char *ip_nettoa ARGS(( nettype_t nettype ));

/* ip_ps.c */
int ipps_init ARGS(( ip_port_t *ip_port ));
void ipps_get ARGS(( int ip_port_nr ));
void ipps_put ARGS(( int ip_port_nr, ipaddr_t nexthop, acc_t *pack ));

/* ip_read.c */
void ip_port_arrive ARGS(( ip_port_t *port, acc_t *pack, ip_hdr_t *ip_hdr ));
void ip_arrived ARGS(( ip_port_t *port, acc_t *pack ));
void ip_arrived_broadcast ARGS(( ip_port_t *port, acc_t *pack ));
void ip_process_loopb ARGS(( event_t *ev, ev_arg_t arg ));
void ip_packet2user ARGS(( ip_fd_t *ip_fd, acc_t *pack, time_t exp_time,
	size_t data_len ));

/* ip_write.c */
void dll_eth_write_frame ARGS(( ip_port_t *port ));
acc_t *ip_split_pack ARGS(( ip_port_t *ip_port, acc_t **ref_last, int mtu ));
void ip_hdr_chksum ARGS(( ip_hdr_t *ip_hdr, int ip_hdr_len ));


extern ip_fd_t ip_fd_table[IP_FD_NR];
extern ip_port_t *ip_port_table;
extern ip_ass_t ip_ass_table[IP_ASS_NR];

#define NWIO_DEFAULT    (NWIO_EN_LOC | NWIO_EN_BROAD | NWIO_REMANY | \
	NWIO_RWDATALL | NWIO_HDR_O_SPEC)

#endif /* INET_IP_INT_H */

/*
 * $PchId: ip_int.h,v 1.19 2004/08/03 16:24:23 philip Exp $
 */

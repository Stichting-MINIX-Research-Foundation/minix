/*
generic/udp_int.h

Created:	March 2001 by Philip Homburg <philip@f-mnx.phicoh.com>

Some internals of the UDP module
*/

#define UDP_FD_NR		(4*IP_PORT_MAX)
#define UDP_PORT_HASH_NR	16		/* Must be a power of 2 */

typedef struct udp_port
{
	int up_flags;
	int up_state;
	int up_ipfd;
	int up_ipdev;
	acc_t *up_wr_pack;
	ipaddr_t up_ipaddr;
	struct udp_fd *up_next_fd;
	struct udp_fd *up_write_fd;
	struct udp_fd *up_port_any;
	struct udp_fd *up_port_hash[UDP_PORT_HASH_NR];
} udp_port_t;

#define UPF_EMPTY	0x0
#define UPF_WRITE_IP	0x1
#define UPF_WRITE_SP	0x2
#define UPF_READ_IP	0x4
#define UPF_READ_SP	0x8
#define UPF_SUSPEND	0x10
#define UPF_MORE2WRITE	0x20

#define UPS_EMPTY	0
#define UPS_SETPROTO	1
#define UPS_GETCONF	2
#define UPS_MAIN	3
#define UPS_ERROR	4

typedef struct udp_fd
{
	int uf_flags;
	udp_port_t *uf_port;
	ioreq_t uf_ioreq;
	int uf_srfd;
	nwio_udpopt_t uf_udpopt;
	get_userdata_t uf_get_userdata;
	put_userdata_t uf_put_userdata;
	select_res_t uf_select_res;
	acc_t *uf_rdbuf_head;
	acc_t *uf_rdbuf_tail;
	size_t uf_rd_count;
	size_t uf_wr_count;
	clock_t uf_exp_tim;
	struct udp_fd *uf_port_next;
} udp_fd_t;

#define UFF_EMPTY	0x0
#define UFF_INUSE	0x1
#define UFF_IOCTL_IP	0x2
#define UFF_READ_IP	0x4
#define UFF_WRITE_IP	0x8
#define UFF_OPTSET	0x10
#define UFF_PEEK_IP	0x20
#define UFF_SEL_READ	0x40
#define UFF_SEL_WRITE	0x80

EXTERN udp_port_t *udp_port_table;
EXTERN udp_fd_t udp_fd_table[UDP_FD_NR];

/*
 * $PchId: udp_int.h,v 1.4 2004/08/03 11:12:01 philip Exp $
 */

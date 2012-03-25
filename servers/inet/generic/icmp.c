/*
icmp.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "event.h"
#include "type.h"

#include "assert.h"
#include "clock.h"
#include "icmp.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "ip_int.h"
#include "ipr.h"

THIS_FILE

typedef struct icmp_port
{
	int icp_flags;
	int icp_state;
	int icp_ipport;
	int icp_ipfd;
	unsigned icp_rate_count;
	unsigned icp_rate_report;
	time_t icp_rate_lasttime;
	acc_t *icp_head_queue;
	acc_t *icp_tail_queue;
	acc_t *icp_write_pack;
	event_t icp_event;
} icmp_port_t;

#define ICPF_EMPTY	0x0
#define ICPF_SUSPEND	0x1
#define ICPF_READ_IP	0x2
#define ICPF_READ_SP	0x4
#define ICPF_WRITE_IP	0x8
#define ICPF_WRITE_SP	0x10

#define ICPS_BEGIN	0
#define ICPS_IPOPT	1
#define ICPS_MAIN	2
#define ICPS_ERROR	3

static icmp_port_t *icmp_port_table;

static void icmp_main ARGS(( icmp_port_t *icmp_port ));
static acc_t *icmp_getdata ARGS(( int port, size_t offset,
	size_t count, int for_ioctl ));
static int icmp_putdata ARGS(( int port, size_t offset,
	acc_t *data, int for_ioctl ));
static void icmp_read ARGS(( icmp_port_t *icmp_port ));
static void process_data ARGS(( icmp_port_t *icmp_port,
	acc_t *data ));
static u16_t icmp_pack_oneCsum ARGS(( acc_t *ip_pack ));
static void icmp_echo_request ARGS(( icmp_port_t *icmp_port,
	acc_t *ip_pack, int ip_hdr_len, ip_hdr_t *ip_hdr,
	acc_t *icmp_pack, int icmp_len, icmp_hdr_t *icmp_hdr ));
static void icmp_dst_unreach ARGS(( icmp_port_t *icmp_port,
	acc_t *ip_pack, int ip_hdr_len, ip_hdr_t *ip_hdr,
	acc_t *icmp_pack, int icmp_len, icmp_hdr_t *icmp_hdr ));
static void icmp_time_exceeded ARGS(( icmp_port_t *icmp_port,
	acc_t *ip_pack, int ip_hdr_len, ip_hdr_t *ip_hdr,
	acc_t *icmp_pack, int icmp_len, icmp_hdr_t *icmp_hdr ));
static void icmp_router_advertisement ARGS(( icmp_port_t *icmp_port,
	acc_t *icmp_pack, int icmp_len, icmp_hdr_t *icmp_hdr ));
static void icmp_redirect ARGS(( icmp_port_t *icmp_port,
	ip_hdr_t *ip_hdr, acc_t *icmp_pack, int icmp_len,
	icmp_hdr_t *icmp_hdr ));
static acc_t *make_repl_ip ARGS(( ip_hdr_t *ip_hdr,
	int ip_len ));
static void enqueue_pack ARGS(( icmp_port_t *icmp_port,
	acc_t *reply_ip_hdr ));
static int icmp_rate_limit ARGS(( icmp_port_t *icmp_port,
	acc_t *reply_ip_hdr ));
static void icmp_write ARGS(( event_t *ev, ev_arg_t ev_arg ));
static void icmp_buffree ARGS(( int priority ));
static acc_t *icmp_err_pack ARGS(( acc_t *pack, icmp_hdr_t **icmp_hdr_pp ));
#ifdef BUF_CONSISTENCY_CHECK
static void icmp_bufcheck ARGS(( void ));
#endif

void icmp_prep()
{
	icmp_port_table= alloc(ip_conf_nr * sizeof(icmp_port_table[0]));
}

void icmp_init()
{
	int i;
	icmp_port_t *icmp_port;

	assert (BUF_S >= sizeof (nwio_ipopt_t));

	for (i= 0, icmp_port= icmp_port_table; i<ip_conf_nr; i++, icmp_port++)
	{
		icmp_port->icp_flags= ICPF_EMPTY;
		icmp_port->icp_state= ICPS_BEGIN;
		icmp_port->icp_ipport= i;
		icmp_port->icp_rate_count= 0;
		icmp_port->icp_rate_report= ICMP_MAX_RATE;
		icmp_port->icp_rate_lasttime= 0;
		ev_init(&icmp_port->icp_event);
	}

#ifndef BUF_CONSISTENCY_CHECK
	bf_logon(icmp_buffree);
#else
	bf_logon(icmp_buffree, icmp_bufcheck);
#endif

	for (i= 0, icmp_port= icmp_port_table; i<ip_conf_nr; i++, icmp_port++)
	{
		icmp_main (icmp_port);
	}
}

static void icmp_main(icmp_port)
icmp_port_t *icmp_port;
{
	int result;
	switch (icmp_port->icp_state)
	{
	case ICPS_BEGIN:
		icmp_port->icp_head_queue= 0;
		icmp_port->icp_ipfd= ip_open(icmp_port->icp_ipport,
			icmp_port->icp_ipport, icmp_getdata, icmp_putdata,
			0 /* no put_pkt */, 0 /* no select_res */);
		if (icmp_port->icp_ipfd<0)
		{
			DBLOCK(1, printf("unable to open ip_port %d\n",
				icmp_port->icp_ipport));
			break;
		}
		icmp_port->icp_state= ICPS_IPOPT;
		icmp_port->icp_flags &= ~ICPF_SUSPEND;
		result= ip_ioctl (icmp_port->icp_ipfd, NWIOSIPOPT);
		if (result == NW_SUSPEND)
		{
			icmp_port->icp_flags |= ICPF_SUSPEND;
			break;
		}
		assert(result == NW_OK);

		/* falls through */
	case ICPS_IPOPT:
		icmp_port->icp_state= ICPS_MAIN;
		icmp_port->icp_flags &= ~ICPF_SUSPEND;
		icmp_read(icmp_port);
		break;
	default:
		DBLOCK(1, printf("unknown state %d\n",
			icmp_port->icp_state));
		break;
	}
}

static acc_t *icmp_getdata(port, offset, count, for_ioctl)
int port;
size_t offset, count;
int for_ioctl;
{
	icmp_port_t *icmp_port;
	nwio_ipopt_t *ipopt;
	acc_t *data;
	int result;
	ev_arg_t ev_arg;

	icmp_port= &icmp_port_table[port];

	if (icmp_port->icp_flags & ICPF_WRITE_IP)
	{
		if (!count)
		{
			bf_afree(icmp_port->icp_write_pack);
			icmp_port->icp_write_pack= 0;

			result= (int)offset;
			if (result<0)
			{
				DBLOCK(1, printf("got write error %d\n",
					result));
			}
			if (icmp_port->icp_flags & ICPF_WRITE_SP)
			{
				icmp_port->icp_flags &= ~ICPF_WRITE_SP;
				ev_arg.ev_ptr= icmp_port;
				ev_enqueue(&icmp_port->icp_event, icmp_write,
					ev_arg);
			}
			return NW_OK;
		}
		return bf_cut(icmp_port->icp_write_pack, offset, count);
	}
	switch (icmp_port->icp_state)
	{
	case ICPS_IPOPT:
		if (!count)
		{
			result= (int)offset;
			assert(result == NW_OK);
			if (result < 0)
			{
				icmp_port->icp_state= ICPS_ERROR;
				break;
			}
			if (icmp_port->icp_flags & ICPF_SUSPEND)
				icmp_main(icmp_port);
			return NW_OK;
		}

		data= bf_memreq (sizeof (*ipopt));
		ipopt= (nwio_ipopt_t *)ptr2acc_data(data);
		ipopt->nwio_flags= NWIO_COPY | NWIO_EN_LOC |
			NWIO_EN_BROAD |
			NWIO_REMANY | NWIO_PROTOSPEC |
			NWIO_HDR_O_ANY | NWIO_RWDATALL;
		ipopt->nwio_proto= IPPROTO_ICMP;
		return data;
	default:
		break;
	}
	DBLOCK(1, printf("unknown state %d\n", icmp_port->icp_state));
	return NULL;
}

static int icmp_putdata(port, offset, data, for_ioctl)
int port;
size_t offset;
acc_t *data;
int for_ioctl;
{
	icmp_port_t *icmp_port;
	int result;

	icmp_port= &icmp_port_table[port];

	if (icmp_port->icp_flags & ICPF_READ_IP)
	{
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				DBLOCK(1, printf("got read error %d\n",
					result));
			}
			if (icmp_port->icp_flags & ICPF_READ_SP)
			{
				icmp_port->icp_flags &=
					~(ICPF_READ_IP|ICPF_READ_SP);
				icmp_read (icmp_port);
			}
			return NW_OK;
		}
		process_data(icmp_port, data);
		return NW_OK;
	}
	switch (icmp_port->icp_state)
	{
	default:
		DBLOCK(1, printf("unknown state %d\n",
			icmp_port->icp_state));
		return 0;
	}
}

static void icmp_read(icmp_port)
icmp_port_t *icmp_port;
{
	int result;

	for (;;)
	{
		icmp_port->icp_flags |= ICPF_READ_IP;
		icmp_port->icp_flags &= ~ICPF_READ_SP;

		result= ip_read(icmp_port->icp_ipfd, ICMP_MAX_DATAGRAM);
		if (result == NW_SUSPEND)
		{
			icmp_port->icp_flags |= ICPF_READ_SP;
			return;
		}
	}
}

void icmp_snd_time_exceeded(port_nr, pack, code)
int port_nr;
acc_t *pack;
int code;
{
	icmp_hdr_t *icmp_hdr;
	icmp_port_t *icmp_port;

	if (port_nr >= 0 && port_nr < ip_conf_nr)
		icmp_port= &icmp_port_table[port_nr];
	else
	{
		printf("icmp_snd_time_exceeded: strange port %d\n", port_nr);
		bf_afree(pack);
		return;
	}
	pack= icmp_err_pack(pack, &icmp_hdr);
	if (pack == NULL)
		return;
	icmp_hdr->ih_type= ICMP_TYPE_TIME_EXCEEDED;
	icmp_hdr->ih_code= code;
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_type, 2);
	enqueue_pack(icmp_port, pack);
}

void icmp_snd_redirect(port_nr, pack, code, gw)
int port_nr;
acc_t *pack;
int code;
ipaddr_t gw;
{
	icmp_hdr_t *icmp_hdr;
	icmp_port_t *icmp_port;

	if (port_nr >= 0 && port_nr < ip_conf_nr)
		icmp_port= &icmp_port_table[port_nr];
	else
	{
		printf("icmp_snd_redirect: strange port %d\n", port_nr);
		bf_afree(pack);
		return;
	}
	pack= icmp_err_pack(pack, &icmp_hdr);
	if (pack == NULL)
		return;
	icmp_hdr->ih_type= ICMP_TYPE_REDIRECT;
	icmp_hdr->ih_code= code;
	icmp_hdr->ih_hun.ihh_gateway= gw;
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_type, 2);
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_hun.ihh_gateway, 4);
	enqueue_pack(icmp_port, pack);
}

void icmp_snd_unreachable(port_nr, pack, code)
int port_nr;
acc_t *pack;
int code;
{
	icmp_hdr_t *icmp_hdr;
	icmp_port_t *icmp_port;

	if (port_nr >= 0 && port_nr < ip_conf_nr)
		icmp_port= &icmp_port_table[port_nr];
	else
	{
		printf("icmp_snd_unreachable: strange port %d\n", port_nr);
		bf_afree(pack);
		return;
	}
	pack= icmp_err_pack(pack, &icmp_hdr);
	if (pack == NULL)
		return;
	icmp_hdr->ih_type= ICMP_TYPE_DST_UNRCH;
	icmp_hdr->ih_code= code;
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_type, 2);
	enqueue_pack(icmp_port, pack);
}

void icmp_snd_mtu(
  int port_nr,
  acc_t *pack,
  u16_t mtu
)
{
	icmp_hdr_t *icmp_hdr;
	icmp_port_t *icmp_port;

	if (port_nr >= 0 && port_nr < ip_conf_nr)
		icmp_port= &icmp_port_table[port_nr];
	else
	{
		printf("icmp_snd_mtu: strange port %d\n", port_nr);
		bf_afree(pack);
		return;
	}

	pack= icmp_err_pack(pack, &icmp_hdr);
	if (pack == NULL)
		return;
	icmp_hdr->ih_type= ICMP_TYPE_DST_UNRCH;
	icmp_hdr->ih_code= ICMP_FRAGM_AND_DF;
	icmp_hdr->ih_hun.ihh_mtu.im_mtu= htons(mtu);
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_type, 2);
	icmp_hdr->ih_chksum= ~oneC_sum(~icmp_hdr->ih_chksum,
		(u16_t *)&icmp_hdr->ih_hun.ihh_mtu.im_mtu, 2);
	enqueue_pack(icmp_port, pack);
}

static void process_data(icmp_port, data)
icmp_port_t *icmp_port;
acc_t *data;
{
	ip_hdr_t *ip_hdr;
	icmp_hdr_t *icmp_hdr;
	acc_t *icmp_data;
	int ip_hdr_len;
	size_t pack_len;

	/* Align entire packet */
	data= bf_align(data, BUF_S, 4);

	data= bf_packIffLess(data, IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
	DIFBLOCK(0x10, (ip_hdr->ih_dst & HTONL(0xf0000000)) == HTONL(0xe0000000),
		printf("got multicast packet\n"));
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;

	if (ip_hdr_len>IP_MIN_HDR_SIZE)
	{
		data= bf_packIffLess(data, ip_hdr_len);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
	}

	pack_len= bf_bufsize(data);
	pack_len -= ip_hdr_len;
	if (pack_len < ICMP_MIN_HDR_SIZE)
	{
		if (pack_len == 0 && ip_hdr->ih_proto == 0)
		{
			/* IP layer reports new ip address, which can be
			 * ignored.
			 */
		}
		else
			DBLOCK(1, printf("got an incomplete icmp packet\n"));
		bf_afree(data);
		return;
	}

	icmp_data= bf_cut(data, ip_hdr_len, pack_len);

	icmp_data= bf_packIffLess (icmp_data, ICMP_MIN_HDR_SIZE);
	icmp_hdr= (icmp_hdr_t *)ptr2acc_data(icmp_data);

	if ((u16_t)~icmp_pack_oneCsum(icmp_data))
	{
		DBLOCK(1, printf(
			"got packet with bad checksum (= 0x%x, 0x%x)\n",
			icmp_hdr->ih_chksum,
			(u16_t)~icmp_pack_oneCsum(icmp_data)));
		bf_afree(data);
		bf_afree(icmp_data);
		return;
	}

	switch (icmp_hdr->ih_type)
	{
	case ICMP_TYPE_ECHO_REPL:
		break;
	case ICMP_TYPE_DST_UNRCH:
		icmp_dst_unreach (icmp_port, data, ip_hdr_len, ip_hdr,
			icmp_data, pack_len, icmp_hdr);
		break;
	case ICMP_TYPE_SRC_QUENCH:
		/* Ignore src quench ICMPs */
		DBLOCK(2, printf("ignoring SRC QUENCH ICMP.\n"));
		break;
	case ICMP_TYPE_REDIRECT:
		icmp_redirect (icmp_port, ip_hdr, icmp_data, pack_len,
			icmp_hdr);
		break;
	case ICMP_TYPE_ECHO_REQ:
		icmp_echo_request(icmp_port, data, ip_hdr_len, ip_hdr,
			icmp_data, pack_len, icmp_hdr);
		return;
	case ICMP_TYPE_ROUTER_ADVER:
		icmp_router_advertisement(icmp_port, icmp_data, pack_len, 
			icmp_hdr);
		break;
	case ICMP_TYPE_ROUTE_SOL:
		break;	/* Should be handled by a routing deamon. */
	case ICMP_TYPE_TIME_EXCEEDED:
		icmp_time_exceeded (icmp_port, data, ip_hdr_len, ip_hdr,
			icmp_data, pack_len, icmp_hdr);
		break;
	default:
		DBLOCK(1, printf("got an unknown icmp (%d) from ",
			icmp_hdr->ih_type); 
			writeIpAddr(ip_hdr->ih_src); printf("\n"));
		break;
	}
	bf_afree(data);
	bf_afree(icmp_data);
}

static void icmp_echo_request(icmp_port, ip_data, ip_len, ip_hdr,
	icmp_data, icmp_len, icmp_hdr)
icmp_port_t *icmp_port;
acc_t *ip_data, *icmp_data;
int ip_len, icmp_len;
ip_hdr_t *ip_hdr;
icmp_hdr_t *icmp_hdr;
{
	acc_t *repl_ip_hdr, *repl_icmp;
	ipaddr_t tmpaddr, locaddr, netmask;
	icmp_hdr_t *repl_icmp_hdr;
	i32_t tmp_chksum;
	ip_port_t *ip_port;

	if (icmp_hdr->ih_code != 0)
	{
		DBLOCK(1,
		printf("got an icmp echo request with unknown code (%d)\n",
			icmp_hdr->ih_code));
		bf_afree(ip_data);
		bf_afree(icmp_data);
		return;
	}
	if (icmp_len < ICMP_MIN_HDR_SIZE + sizeof(icmp_id_seq_t))
	{
		DBLOCK(1, printf("got an incomplete icmp echo request\n"));
		bf_afree(ip_data);
		bf_afree(icmp_data);
		return;
	}
	tmpaddr= ntohl(ip_hdr->ih_dst);
	if ((tmpaddr & 0xe0000000) == 0xe0000000 &&
		tmpaddr != 0xffffffff)
	{
		/* Respond only to the all hosts multicast address until
		 * a decent listening service has been implemented
		 */
		if (tmpaddr != 0xe0000001)
		{
			bf_afree(ip_data);
			bf_afree(icmp_data);
			return;
		}
	}

	/* Limit subnet broadcasts to the local net */
	ip_port= &ip_port_table[icmp_port->icp_ipport];
	locaddr= ip_port->ip_ipaddr;
	netmask= ip_port->ip_subnetmask;
	if (ip_hdr->ih_dst == (locaddr | ~netmask) &&
		(ip_port->ip_flags & IPF_SUBNET_BCAST) &&
		((ip_hdr->ih_src ^ locaddr) & netmask) != 0)
	{
		/* Directed broadcast */
		bf_afree(ip_data);
		bf_afree(icmp_data);
		return;
	}

	repl_ip_hdr= make_repl_ip(ip_hdr, ip_len);
	repl_icmp= bf_memreq (ICMP_MIN_HDR_SIZE);
	repl_icmp_hdr= (icmp_hdr_t *)ptr2acc_data(repl_icmp);
	repl_icmp_hdr->ih_type= ICMP_TYPE_ECHO_REPL;
	repl_icmp_hdr->ih_code= 0;

	DBLOCK(2,
	printf("ih_chksum= 0x%x, ih_type= 0x%x, repl->ih_type= 0x%x\n",
		icmp_hdr->ih_chksum, *(u16_t *)&icmp_hdr->ih_type, 
		*(u16_t *)&repl_icmp_hdr->ih_type));
	tmp_chksum= (~icmp_hdr->ih_chksum & 0xffff) - 
		(i32_t)*(u16_t *)&icmp_hdr->ih_type+
		*(u16_t *)&repl_icmp_hdr->ih_type;
	tmp_chksum= (tmp_chksum >> 16) + (tmp_chksum & 0xffff);
	tmp_chksum= (tmp_chksum >> 16) + (tmp_chksum & 0xffff);
	repl_icmp_hdr->ih_chksum= ~tmp_chksum;
	DBLOCK(2, printf("sending chksum 0x%x\n", repl_icmp_hdr->ih_chksum));

	repl_ip_hdr->acc_next= repl_icmp;
	repl_icmp->acc_next= bf_cut (icmp_data, ICMP_MIN_HDR_SIZE,
		icmp_len - ICMP_MIN_HDR_SIZE);

	bf_afree(ip_data);
	bf_afree(icmp_data);

	enqueue_pack(icmp_port, repl_ip_hdr);
}

static u16_t icmp_pack_oneCsum(icmp_pack)
acc_t *icmp_pack;
{
	u16_t prev;
	int odd_byte;
	char *data_ptr;
	int length;
	char byte_buf[2];

	prev= 0;

	odd_byte= FALSE;
	for (; icmp_pack; icmp_pack= icmp_pack->acc_next)
	{
		data_ptr= ptr2acc_data(icmp_pack);
		length= icmp_pack->acc_length;

		if (!length)
			continue;
		if (odd_byte)
		{
			byte_buf[1]= *data_ptr;
			prev= oneC_sum(prev, (u16_t *)byte_buf, 2);
			data_ptr++;
			length--;
			odd_byte= FALSE;
		}
		if (length & 1)
		{
			odd_byte= TRUE;
			length--;
			byte_buf[0]= data_ptr[length];
		}
		if (!length)
			continue;
		prev= oneC_sum (prev, (u16_t *)data_ptr, length);
	}
	if (odd_byte)
		prev= oneC_sum (prev, (u16_t *)byte_buf, 1);
	return prev;
}

static acc_t *make_repl_ip(ip_hdr, ip_len)
ip_hdr_t *ip_hdr;
int ip_len;
{
	ip_hdr_t *repl_ip_hdr;
	acc_t *repl;
	int repl_hdr_len;

	if (ip_len>IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("ip_hdr options NOT supported (yet?)\n"));
		ip_len= IP_MIN_HDR_SIZE;
	}

	repl_hdr_len= IP_MIN_HDR_SIZE;

	repl= bf_memreq(repl_hdr_len);

	repl_ip_hdr= (ip_hdr_t *)ptr2acc_data(repl);

	repl_ip_hdr->ih_vers_ihl= repl_hdr_len >> 2;
	repl_ip_hdr->ih_tos= ip_hdr->ih_tos;
	repl_ip_hdr->ih_ttl= ICMP_DEF_TTL;
	repl_ip_hdr->ih_proto= IPPROTO_ICMP;
	repl_ip_hdr->ih_dst= ip_hdr->ih_src;
	repl_ip_hdr->ih_flags_fragoff= 0;

	return repl;
}

static void enqueue_pack(icmp_port, reply_ip_hdr)
icmp_port_t *icmp_port;
acc_t *reply_ip_hdr;
{
	int r;
	ev_arg_t ev_arg;

	/* Check rate */
	if (icmp_port->icp_rate_count >= ICMP_MAX_RATE)
	{
		/* Something is going wrong; check policy */
		r= icmp_rate_limit(icmp_port, reply_ip_hdr);
		if (r == -1)
		{
			bf_afree(reply_ip_hdr);
			return;
		}

		/* OK, continue */
	}
	icmp_port->icp_rate_count++;

	reply_ip_hdr->acc_ext_link= 0;

	if (icmp_port->icp_head_queue)
	{
		icmp_port->icp_tail_queue->acc_ext_link=
			reply_ip_hdr;
	}
	else
	{
		icmp_port->icp_head_queue= reply_ip_hdr;
	}
	reply_ip_hdr->acc_ext_link= NULL;
	icmp_port->icp_tail_queue= reply_ip_hdr;

	if (!(icmp_port->icp_flags & ICPF_WRITE_IP))
	{
		icmp_port->icp_flags |= ICPF_WRITE_IP;
		ev_arg.ev_ptr= icmp_port;
		ev_enqueue(&icmp_port->icp_event, icmp_write, ev_arg);
	}
}

static int icmp_rate_limit(icmp_port, reply_ip_hdr)
icmp_port_t *icmp_port;
acc_t *reply_ip_hdr;
{
	time_t t;
	acc_t *pack;
	ip_hdr_t *ip_hdr;
	icmp_hdr_t *icmp_hdr;
	int hdrlen, icmp_hdr_len, type;

	/* Check the time first */
	t= get_time();
	if (t >= icmp_port->icp_rate_lasttime + ICMP_RATE_INTERVAL)
	{
		icmp_port->icp_rate_lasttime= t;
		icmp_port->icp_rate_count= 0;
		return 0;
	}

	icmp_port->icp_rate_count++;

	/* Adjust report limit if necessary */
	if (icmp_port->icp_rate_count >
		icmp_port->icp_rate_report+ICMP_RATE_WARN)
	{
		icmp_port->icp_rate_report *= 2;
		return -1;
	}

	/* Do we need to report */
	if (icmp_port->icp_rate_count < icmp_port->icp_rate_report)
		return -1;

	pack= bf_dupacc(reply_ip_hdr);
	pack= bf_packIffLess(pack, IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	printf("icmp[%d]: dropping ICMP packet #%d to ",
		icmp_port->icp_ipport, icmp_port->icp_rate_count);
	writeIpAddr(ip_hdr->ih_dst);
	hdrlen= (ip_hdr->ih_vers_ihl & IH_IHL_MASK)*4;
	pack= bf_packIffLess(pack, hdrlen+ICMP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	icmp_hdr= (icmp_hdr_t *)(ptr2acc_data(pack)+hdrlen);
	type= icmp_hdr->ih_type;
	printf(" type %d, code %d\n", type, icmp_hdr->ih_code);
	switch(type)
	{
	case ICMP_TYPE_DST_UNRCH:
	case ICMP_TYPE_SRC_QUENCH:
	case ICMP_TYPE_REDIRECT:
	case ICMP_TYPE_TIME_EXCEEDED:
	case ICMP_TYPE_PARAM_PROBLEM:
		icmp_hdr_len= offsetof(struct icmp_hdr, ih_dun);
		pack= bf_packIffLess(pack,
			hdrlen+icmp_hdr_len+IP_MIN_HDR_SIZE);
		ip_hdr= (ip_hdr_t *)(ptr2acc_data(pack)+hdrlen+icmp_hdr_len);
		icmp_hdr= (icmp_hdr_t *)(ptr2acc_data(pack)+hdrlen);
		printf("\tinfo %08x, original dst ",
			ntohs(icmp_hdr->ih_hun.ihh_unused));
		writeIpAddr(ip_hdr->ih_dst);
		printf(", proto %d, length %u\n",
			ip_hdr->ih_proto, ntohs(ip_hdr->ih_length));
		break;
	default:
		break;
	}
	bf_afree(pack); pack= NULL;

	return -1;
}

static void icmp_write(ev, ev_arg)
event_t *ev;
ev_arg_t ev_arg;
{
	int result;
	icmp_port_t *icmp_port;
	acc_t *data;

	icmp_port= ev_arg.ev_ptr;
	assert(ev == &icmp_port->icp_event);

	assert (icmp_port->icp_flags & ICPF_WRITE_IP);
	assert (!(icmp_port->icp_flags & ICPF_WRITE_SP));

	while (icmp_port->icp_head_queue != NULL)
	{
		data= icmp_port->icp_head_queue;
		icmp_port->icp_head_queue= data->acc_ext_link;

		result= ip_send(icmp_port->icp_ipfd, data,
			bf_bufsize(data));
		if (result != NW_WOULDBLOCK)
		{
			if (result == NW_OK)
				continue;
			DBLOCK(1, printf("icmp_write: error %d\n", result););
			continue;
		}

		assert(icmp_port->icp_write_pack == NULL);
		icmp_port->icp_write_pack= data;
			
		result= ip_write(icmp_port->icp_ipfd,
			bf_bufsize(icmp_port->icp_write_pack));
		if (result == NW_SUSPEND)
		{
			icmp_port->icp_flags |= ICPF_WRITE_SP;
			return;
		}
	}
	icmp_port->icp_flags &= ~ICPF_WRITE_IP;
}

static void icmp_buffree(priority)
int priority;
{
	acc_t *tmp_acc;
	int i;
	icmp_port_t *icmp_port;

	if (priority == ICMP_PRI_QUEUE)
	{
		for (i=0, icmp_port= icmp_port_table; i<ip_conf_nr;
			i++, icmp_port++)
		{
			while(icmp_port->icp_head_queue)
			{
				tmp_acc= icmp_port->icp_head_queue;
				icmp_port->icp_head_queue=
					tmp_acc->acc_ext_link;
				bf_afree(tmp_acc);
			}
		}
	}
}

#ifdef BUF_CONSISTENCY_CHECK
static void icmp_bufcheck()
{
	int i;
	icmp_port_t *icmp_port;
	acc_t *pack;

	for (i= 0, icmp_port= icmp_port_table; i<ip_conf_nr; i++, icmp_port++)
	{
		for (pack= icmp_port->icp_head_queue; pack; 
			pack= pack->acc_ext_link)
		{
			bf_check_acc(pack);
		}
		bf_check_acc(icmp_port->icp_write_pack);
	}
}
#endif

static void icmp_dst_unreach(icmp_port, ip_pack, ip_hdr_len, ip_hdr, icmp_pack,
	icmp_len, icmp_hdr)
icmp_port_t *icmp_port;
acc_t *ip_pack;
int ip_hdr_len;
ip_hdr_t *ip_hdr;
acc_t *icmp_pack;
int icmp_len;
icmp_hdr_t *icmp_hdr;
{
	acc_t *old_ip_pack;
	ip_hdr_t *old_ip_hdr;
	int ip_port_nr;
	ipaddr_t dst, mask;
	size_t old_pack_size;
	u16_t new_mtu;

	if (icmp_len < 8 + IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("dest unrch with wrong size\n"));
		return;
	}
	old_ip_pack= bf_cut (icmp_pack, 8, icmp_len-8);
	old_ip_pack= bf_packIffLess(old_ip_pack, IP_MIN_HDR_SIZE);
	old_ip_hdr= (ip_hdr_t *)ptr2acc_data(old_ip_pack);

	if (old_ip_hdr->ih_src != ip_hdr->ih_dst)
	{
		DBLOCK(1, printf("dest unrch based on wrong packet\n"));
		bf_afree(old_ip_pack);
		return;
	}

	ip_port_nr= icmp_port->icp_ipport;

	switch(icmp_hdr->ih_code)
	{
	case ICMP_NET_UNRCH:
		dst= old_ip_hdr->ih_dst;
		mask= ip_get_netmask(dst);
		ipr_destunrch (ip_port_nr, dst & mask, mask,
			IPR_UNRCH_TIMEOUT);
		break;
	case ICMP_HOST_UNRCH:
		ipr_destunrch (ip_port_nr, old_ip_hdr->ih_dst, (ipaddr_t)-1,
			IPR_UNRCH_TIMEOUT);
		break;
	case ICMP_PORT_UNRCH:
		/* At the moment we don't do anything with this information.
		 * It should be handed to the appropriate transport layer.
		 */
		break;
	case ICMP_FRAGM_AND_DF:

		DBLOCK(1, printf("icmp_dst_unreach: got mtu icmp from ");
			writeIpAddr(ip_hdr->ih_src);
			printf("; original destination: ");
			writeIpAddr(old_ip_hdr->ih_dst);
			printf("; protocol: %d\n",
			old_ip_hdr->ih_proto));
		old_pack_size= ntohs(old_ip_hdr->ih_length);
		if (!old_pack_size)
			break;
		new_mtu= ntohs(icmp_hdr->ih_hun.ihh_mtu.im_mtu);
		if (!new_mtu || new_mtu > old_pack_size)
			new_mtu= old_pack_size-1;
		ipr_mtu(ip_port_nr, old_ip_hdr->ih_dst, new_mtu,
			IPR_MTU_TIMEOUT);
		break;

	default:
		DBLOCK(1, printf("icmp_dst_unreach: got strange code %d from ",
			icmp_hdr->ih_code);
			writeIpAddr(ip_hdr->ih_src);
			printf("; original destination: ");
			writeIpAddr(old_ip_hdr->ih_dst);
			printf("; protocol: %d\n",
			old_ip_hdr->ih_proto));
		break;
	}
	bf_afree(old_ip_pack);
}

static void icmp_time_exceeded(icmp_port, ip_pack, ip_hdr_len, ip_hdr,
	icmp_pack, icmp_len, icmp_hdr)
icmp_port_t *icmp_port;
acc_t *ip_pack;
int ip_hdr_len;
ip_hdr_t *ip_hdr;
acc_t *icmp_pack;
int icmp_len;
icmp_hdr_t *icmp_hdr;
{
	acc_t *old_ip_pack;
	ip_hdr_t *old_ip_hdr;
	int ip_port_nr;

	if (icmp_len < 8 + IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("time exceeded with wrong size\n"));
		return;
	}
	old_ip_pack= bf_cut (icmp_pack, 8, icmp_len-8);
	old_ip_pack= bf_packIffLess(old_ip_pack, IP_MIN_HDR_SIZE);
	old_ip_hdr= (ip_hdr_t *)ptr2acc_data(old_ip_pack);

	if (old_ip_hdr->ih_src != ip_hdr->ih_dst)
	{
		DBLOCK(1, printf("time exceeded based on wrong packet\n"));
		bf_afree(old_ip_pack);
		return;
	}

	ip_port_nr= icmp_port->icp_ipport;

	switch(icmp_hdr->ih_code)
	{
	case ICMP_TTL_EXC:
		ipr_ttl_exc (ip_port_nr, old_ip_hdr->ih_dst, (ipaddr_t)-1,
			IPR_TTL_TIMEOUT);
		break;
	case ICMP_FRAG_REASSEM:
		/* Ignore reassembly time-outs. */
		break;
	default:
		DBLOCK(1, printf("got strange code: %d\n",
			icmp_hdr->ih_code));
		break;
	}
	bf_afree(old_ip_pack);
}

static void icmp_router_advertisement(icmp_port, icmp_pack, icmp_len, icmp_hdr)
icmp_port_t *icmp_port;
acc_t *icmp_pack;
int icmp_len;
icmp_hdr_t *icmp_hdr;
{
	int entries;
	int entry_size;
	u16_t lifetime;
	int i;
	char *bufp;

	if (icmp_len < 8)
	{
		DBLOCK(1,
		printf("router advertisement with wrong size (%d)\n",
			icmp_len));
		return;
	}
	if (icmp_hdr->ih_code != 0)
	{
		DBLOCK(1,
		printf("router advertisement with wrong code (%d)\n", 
			icmp_hdr->ih_code));
		return;
	}
	entries= icmp_hdr->ih_hun.ihh_ram.iram_na;
	entry_size= icmp_hdr->ih_hun.ihh_ram.iram_aes * 4;
	if (entries < 1)
	{
		DBLOCK(1, printf(
		"router advertisement with wrong number of entries (%d)\n", 
			entries));
		return;
	}
	if (entry_size < 8)
	{
		DBLOCK(1, printf(
		"router advertisement with wrong entry size (%d)\n", 
			entry_size));
		return;
	}
	if (icmp_len < 8 + entries * entry_size)
	{
		DBLOCK(1,
			printf("router advertisement with wrong size\n"); 
			printf(
			"\t(entries= %d, entry_size= %d, icmp_len= %d)\n",
			entries, entry_size, icmp_len));
		return;
	}
	lifetime= ntohs(icmp_hdr->ih_hun.ihh_ram.iram_lt);
	if (lifetime > 9000)
	{
		DBLOCK(1, printf(
			"router advertisement with wrong lifetime (%d)\n",
			lifetime));
		return;
	}
	for (i= 0, bufp= (char *)&icmp_hdr->ih_dun.uhd_data[0]; i< entries; i++,
		bufp += entry_size)
	{
		u32_t addr;
		i32_t pref;

		addr= *(ipaddr_t *)bufp;
		pref= ntohl(*(u32_t *)(bufp+4));
		ipr_add_oroute(icmp_port->icp_ipport, HTONL(0L), HTONL(0L), 
			addr, lifetime ? lifetime * HZ : 1,
			1, 0, 0, pref, NULL);
	}
}
		
static void icmp_redirect(icmp_port, ip_hdr, icmp_pack, icmp_len, icmp_hdr)
icmp_port_t *icmp_port;
ip_hdr_t *ip_hdr;
acc_t *icmp_pack;
int icmp_len;
icmp_hdr_t *icmp_hdr;
{
	acc_t *old_ip_pack;
	ip_hdr_t *old_ip_hdr;
	int ip_port_nr;
	ipaddr_t dst, mask;

	if (icmp_len < 8 + IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("redirect with wrong size\n"));
		return;
	}
	old_ip_pack= bf_cut (icmp_pack, 8, icmp_len-8);
	old_ip_pack= bf_packIffLess(old_ip_pack, IP_MIN_HDR_SIZE);
	old_ip_hdr= (ip_hdr_t *)ptr2acc_data(old_ip_pack);

	ip_port_nr= icmp_port->icp_ipport;

	switch(icmp_hdr->ih_code)
	{
	case ICMP_REDIRECT_NET:
		dst= old_ip_hdr->ih_dst;
		mask= ip_get_netmask(dst);
		ipr_redirect (ip_port_nr, dst & mask, mask,
			ip_hdr->ih_src, icmp_hdr->ih_hun.ihh_gateway, 
			IPR_REDIRECT_TIMEOUT);
		break;
	case ICMP_REDIRECT_HOST:
		ipr_redirect (ip_port_nr, old_ip_hdr->ih_dst, (ipaddr_t)-1,
			ip_hdr->ih_src, icmp_hdr->ih_hun.ihh_gateway, 
			IPR_REDIRECT_TIMEOUT);
		break;
	default:
		DBLOCK(1, printf("got strange code: %d\n",
			icmp_hdr->ih_code));
		break;
	}
	bf_afree(old_ip_pack);
}

static acc_t *icmp_err_pack(pack, icmp_hdr_pp)
acc_t *pack;
icmp_hdr_t **icmp_hdr_pp;
{
	ip_hdr_t *ip_hdr;
	icmp_hdr_t *icmp_hdr_p;
	acc_t *ip_pack, *icmp_pack, *tmp_pack;
	int ip_hdr_len, icmp_hdr_len, ih_type;
	size_t size, pack_len;
	ipaddr_t dest;
	nettype_t nettype;

	pack= bf_packIffLess(pack, IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	pack_len= bf_bufsize(pack);

	/* If the IP protocol is ICMP (except echo request/reply) or the
	 * fragment offset is non-zero,
	 * drop the packet. Also check if the source address is valid.
	 */
	if ((ntohs(ip_hdr->ih_flags_fragoff) & IH_FRAGOFF_MASK) != 0)
	{
		bf_afree(pack);
		return NULL;
	}
	if (ip_hdr->ih_proto == IPPROTO_ICMP)
	{
		if (ip_hdr_len>IP_MIN_HDR_SIZE)
		{
			pack= bf_packIffLess(pack, ip_hdr_len);
			ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
		}

		if (pack_len < ip_hdr_len+ICMP_MIN_HDR_SIZE)
		{
			bf_afree(pack);
			return NULL;
		}
		icmp_pack= bf_cut(pack, ip_hdr_len, ICMP_MIN_HDR_SIZE);
		icmp_pack= bf_packIffLess (icmp_pack, ICMP_MIN_HDR_SIZE);
		icmp_hdr_p= (icmp_hdr_t *)ptr2acc_data(icmp_pack);
		ih_type= icmp_hdr_p->ih_type;
		bf_afree(icmp_pack); icmp_pack= NULL;

		if (ih_type != ICMP_TYPE_ECHO_REQ &&
			ih_type != ICMP_TYPE_ECHO_REPL)
		{
			bf_afree(pack);
			return NULL;
		}
	}
	dest= ip_hdr->ih_src;
	nettype= ip_nettype(dest);
	if (nettype != IPNT_CLASS_A && nettype != IPNT_LOCAL &&
		nettype != IPNT_CLASS_B && nettype != IPNT_CLASS_C)
	{
		printf("icmp_err_pack: invalid source address: ");
		writeIpAddr(dest);
		printf("\n");
		bf_afree(pack);
		return NULL;
	}

	/* Take the IP header and the first 64 bits of user data. */
	size= ntohs(ip_hdr->ih_length);
	if (size < ip_hdr_len || pack_len < size)
	{
		printf("icmp_err_pack: wrong packet size:\n");
		printf("\thdrlen= %d, ih_length= %d, bufsize= %d\n",
			ip_hdr_len, size, pack_len);
		bf_afree(pack);
		return NULL;
	}
	if (ip_hdr_len + 8 < size)
		size= ip_hdr_len+8;
	tmp_pack= bf_cut(pack, 0, size);
	bf_afree(pack);
	pack= tmp_pack;
	tmp_pack= NULL;

	/* Create a minimal size ICMP hdr. */
	icmp_hdr_len= offsetof(icmp_hdr_t, ih_dun);
	icmp_pack= bf_memreq(icmp_hdr_len);
	pack= bf_append(icmp_pack, pack);
	size += icmp_hdr_len;
	pack= bf_packIffLess(pack, icmp_hdr_len);
	icmp_hdr_p= (icmp_hdr_t *)ptr2acc_data(pack);
	icmp_hdr_p->ih_type= 0;
	icmp_hdr_p->ih_code= 0;
	icmp_hdr_p->ih_chksum= 0;
	icmp_hdr_p->ih_hun.ihh_unused= 0;
	icmp_hdr_p->ih_chksum= ~icmp_pack_oneCsum(pack);
	*icmp_hdr_pp= icmp_hdr_p;

	/* Create an IP header */
	ip_hdr_len= IP_MIN_HDR_SIZE;

	ip_pack= bf_memreq(ip_hdr_len);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(ip_pack);

	ip_hdr->ih_vers_ihl= ip_hdr_len >> 2;
	ip_hdr->ih_tos= 0;
	ip_hdr->ih_length= htons(ip_hdr_len + size);
	ip_hdr->ih_flags_fragoff= 0;
	ip_hdr->ih_ttl= ICMP_DEF_TTL;
	ip_hdr->ih_proto= IPPROTO_ICMP;
	ip_hdr->ih_dst= dest;

	assert(ip_pack->acc_next == NULL);
	ip_pack->acc_next= pack;
	return ip_pack;
}

/*
 * $PchId: icmp.c,v 1.23 2005/06/28 14:16:56 philip Exp $
 */

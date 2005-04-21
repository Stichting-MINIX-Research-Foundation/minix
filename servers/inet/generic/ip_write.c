/*
ip_write.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "event.h"
#include "type.h"

#include "arp.h"
#include "assert.h"
#include "clock.h"
#include "eth.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "ip_int.h"
#include "ipr.h"

THIS_FILE

FORWARD void error_reply ARGS(( ip_fd_t *fd, int error ));

PUBLIC int ip_write (fd, count)
int fd;
size_t count;
{
	ip_fd_t *ip_fd;
	acc_t *pack;
	int r;

	ip_fd= &ip_fd_table[fd];
	if (count > IP_MAX_PACKSIZE)
	{
		error_reply (ip_fd, EPACKSIZE);
		return NW_OK;
	}
	pack= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, (size_t)0,
		count, FALSE);
	if (!pack)
		return NW_OK;
	r= ip_send(fd, pack, count);
	assert(r != NW_WOULDBLOCK);

	if (r == NW_OK)
		error_reply (ip_fd, count);
	else
		error_reply (ip_fd, r);
	return NW_OK;
}

PUBLIC int ip_send(fd, data, data_len)
int fd;
acc_t *data;
size_t data_len;
{
	ip_port_t *ip_port;
	ip_fd_t *ip_fd;
	ip_hdr_t *ip_hdr, *tmp_hdr;
	ipaddr_t dstaddr, netmask, nexthop, hostrep_dst;
	u8_t *addrInBytes;
	acc_t *tmp_pack, *tmp_pack1;
	int hdr_len, hdr_opt_len, r;
	int broadcast, ttl;
	ev_arg_t arg;

	ip_fd= &ip_fd_table[fd];
	ip_port= ip_fd->if_port;

	if (!(ip_fd->if_flags & IFF_OPTSET))
	{
		bf_afree(data);
		return EBADMODE;
	}

	data_len= bf_bufsize(data);

	assert(ip_fd->if_port->ip_flags & IPF_IPADDRSET);

	if (ip_fd->if_ipopt.nwio_flags & NWIO_RWDATONLY)
	{
		tmp_pack= bf_memreq(IP_MIN_HDR_SIZE);
		tmp_pack->acc_next= data;
		data= tmp_pack;
		data_len += IP_MIN_HDR_SIZE;
	}
	if (data_len<IP_MIN_HDR_SIZE)
	{
		bf_afree(data);
		return EPACKSIZE;
	}

	data= bf_packIffLess(data, IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
	if (data->acc_linkC != 1 || data->acc_buffer->buf_linkC != 1)
	{
		tmp_pack= bf_memreq(IP_MIN_HDR_SIZE);
		tmp_hdr= (ip_hdr_t *)ptr2acc_data(tmp_pack);
		*tmp_hdr= *ip_hdr;
		tmp_pack->acc_next= bf_cut(data, IP_MIN_HDR_SIZE,
			data_len-IP_MIN_HDR_SIZE);
		bf_afree(data);
		ip_hdr= tmp_hdr;
		data= tmp_pack;
		assert (data->acc_length >= IP_MIN_HDR_SIZE);
	}

	if (ip_fd->if_ipopt.nwio_flags & NWIO_HDR_O_SPEC)
	{
		hdr_opt_len= ip_fd->if_ipopt.nwio_hdropt.iho_opt_siz;
		if (hdr_opt_len)
		{
			tmp_pack= bf_cut(data, 0, IP_MIN_HDR_SIZE);
			tmp_pack1= bf_cut (data, IP_MIN_HDR_SIZE,
				data_len-IP_MIN_HDR_SIZE);
			bf_afree(data);
			data= bf_packIffLess(tmp_pack, IP_MIN_HDR_SIZE);
			ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
			tmp_pack= bf_memreq (hdr_opt_len);
			memcpy (ptr2acc_data(tmp_pack), ip_fd->if_ipopt.
				nwio_hdropt.iho_data, hdr_opt_len);
			data->acc_next= tmp_pack;
			tmp_pack->acc_next= tmp_pack1;
			hdr_len= IP_MIN_HDR_SIZE+hdr_opt_len;
		}
		else
			hdr_len= IP_MIN_HDR_SIZE;
		ip_hdr->ih_vers_ihl= hdr_len/4;
		ip_hdr->ih_tos= ip_fd->if_ipopt.nwio_tos;
		ip_hdr->ih_flags_fragoff= 0;
		if (ip_fd->if_ipopt.nwio_df)
			ip_hdr->ih_flags_fragoff |= HTONS(IH_DONT_FRAG);
		ip_hdr->ih_ttl= ip_fd->if_ipopt.nwio_ttl;
		ttl= ORTD_UNREACHABLE+1;		/* Don't check TTL */
	}
	else
	{
		hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK)*4;
		r= NW_OK;
		if (hdr_len<IP_MIN_HDR_SIZE)
			r= EINVAL;
		else if (hdr_len>data_len)
			r= EPACKSIZE;
		else if (!ip_hdr->ih_ttl)
			r= EINVAL;
		if (r != NW_OK)
		{
			bf_afree(data);
			return r;
		}

		data= bf_packIffLess(data, hdr_len);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(data);
		if (hdr_len != IP_MIN_HDR_SIZE)
		{
			r= ip_chk_hdropt((u8_t *)(ptr2acc_data(data) +
				IP_MIN_HDR_SIZE),
				hdr_len-IP_MIN_HDR_SIZE);
			if (r != NW_OK)
			{
				bf_afree(data);
				return r;
			}
		}
		ttl= ip_hdr->ih_ttl;
	}
	
	ip_hdr->ih_vers_ihl= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) |
		(IP_VERSION << 4);
	ip_hdr->ih_length= htons(data_len);
	ip_hdr->ih_flags_fragoff &= ~HTONS(IH_FRAGOFF_MASK |
		IH_FLAGS_UNUSED | IH_MORE_FRAGS);
	if (ip_fd->if_ipopt.nwio_flags & NWIO_PROTOSPEC)
		ip_hdr->ih_proto= ip_fd->if_ipopt.nwio_proto;
	ip_hdr->ih_id= htons(ip_port->ip_frame_id++);
	ip_hdr->ih_src= ip_fd->if_port->ip_ipaddr;
	if (ip_fd->if_ipopt.nwio_flags & NWIO_REMSPEC)
		ip_hdr->ih_dst= ip_fd->if_ipopt.nwio_rem;

	dstaddr= ip_hdr->ih_dst;
	hostrep_dst= ntohl(dstaddr);
	r= 0;
	if (hostrep_dst == (ipaddr_t)-1)
		;	/* OK, local broadcast */
	else if ((hostrep_dst & 0xe0000000l) == 0xe0000000l)
		;	/* OK, Multicast */
	else if ((hostrep_dst & 0xf0000000l) == 0xf0000000l)
		r= EBADDEST;	/* Bad class */
	else if ((dstaddr ^ ip_port->ip_ipaddr) & ip_port->ip_subnetmask)
		;	/* OK, remote destination */
	else if (!(dstaddr & ~ip_port->ip_subnetmask))
		r= EBADDEST;	/* Zero host part */
	if (r<0)
	{
		DIFBLOCK(1, r == EBADDEST,
			printf("bad destination: ");
			writeIpAddr(ip_hdr->ih_dst);
			printf("\n"));
		bf_afree(data);
		return r;
	}
	ip_hdr_chksum(ip_hdr, hdr_len);

	data= bf_packIffLess(data, IP_MIN_HDR_SIZE);
	assert (data->acc_length >= IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(data);

	addrInBytes= (u8_t *)&dstaddr;

	if ((addrInBytes[0] & 0xff) == 0x7f)	/* local loopback */
	{
		assert (data->acc_linkC == 1);
		dstaddr= ip_hdr->ih_dst;	/* swap src and dst 
						 * addresses */
		ip_hdr->ih_dst= ip_hdr->ih_src;
		ip_hdr->ih_src= dstaddr;
		data->acc_ext_link= NULL;
		if (ip_port->ip_loopb_head == NULL)
		{
			ip_port->ip_loopb_head= data;
			arg.ev_ptr= ip_port;
			ev_enqueue(&ip_port->ip_loopb_event,
				ip_process_loopb, arg);
		}
		else
			ip_port->ip_loopb_tail->acc_ext_link= data;
		ip_port->ip_loopb_tail= data;

		return NW_OK;
	}

	if (dstaddr == (ipaddr_t)-1)
	{
		r= (*ip_port->ip_dev_send)(ip_port, dstaddr, data,
			/* broadcast */ 1);
		return r;
	}
	netmask= ip_get_netmask(dstaddr);

	if (dstaddr == ip_port->ip_ipaddr)
	{
		assert (data->acc_linkC == 1);

		data->acc_ext_link= NULL;
		if (ip_port->ip_loopb_head == NULL)
		{
			ip_port->ip_loopb_head= data;
			arg.ev_ptr= ip_port;
			ev_enqueue(&ip_port->ip_loopb_event,
				ip_process_loopb, arg);
		}
		else
			ip_port->ip_loopb_tail->acc_ext_link= data;
		ip_port->ip_loopb_tail= data;

		return NW_OK;
	}

	if (((dstaddr ^ ip_port->ip_ipaddr) & ip_port->ip_subnetmask) == 0)
	{
		broadcast= (dstaddr == (ip_port->ip_ipaddr |
			~ip_port->ip_subnetmask));

		r= (*ip_port->ip_dev_send)(ip_port, dstaddr, data,
								broadcast);
		return r;
	}

	r= oroute_frag (ip_port - ip_port_table, dstaddr, ttl, &nexthop);

	if (r == NW_OK)
	{
		if (nexthop == ip_port->ip_ipaddr)
		{
			data->acc_ext_link= NULL;
			if (ip_port->ip_loopb_head == NULL)
			{
				ip_port->ip_loopb_head= data;
				arg.ev_ptr= ip_port;
				ev_enqueue(&ip_port->ip_loopb_event,
					ip_process_loopb, arg);
			}
			else
				ip_port->ip_loopb_tail->acc_ext_link= data;
			ip_port->ip_loopb_tail= data;
		}
		else
		{
			r= (*ip_port->ip_dev_send)(ip_port,
				nexthop, data, /* no broadcast */ 0);
		}
	}
	else
	{
		DBLOCK(0x10, printf("got error %d\n", r));
		bf_afree(data);
	}
	return r;
}

PUBLIC void ip_hdr_chksum(ip_hdr, ip_hdr_len)
ip_hdr_t *ip_hdr;
int ip_hdr_len;
{
	ip_hdr->ih_hdr_chk= 0;
	ip_hdr->ih_hdr_chk= ~oneC_sum (0, (u16_t *)ip_hdr, ip_hdr_len);
}

PUBLIC acc_t *ip_split_pack (ip_port, ref_last, first_size)
ip_port_t *ip_port;
acc_t **ref_last;
int first_size;
{
	int pack_siz;
	ip_hdr_t *first_hdr, *second_hdr;
	int first_hdr_len, second_hdr_len;
	int first_data_len, second_data_len;
	int new_first_data_len;
	int first_opt_size, second_opt_size;
	acc_t *first_pack, *second_pack, *tmp_pack, *tmp_pack1;
	u8_t *first_optptr, *second_optptr;
	int i, optlen;

	first_pack= *ref_last;
	*ref_last= 0;
	second_pack= 0;

	first_pack= bf_packIffLess(first_pack, IP_MIN_HDR_SIZE);
	assert (first_pack->acc_length >= IP_MIN_HDR_SIZE);

	first_hdr= (ip_hdr_t *)ptr2acc_data(first_pack);
	first_hdr_len= (first_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;

	pack_siz= bf_bufsize(first_pack);
	assert(pack_siz > first_size);

	if (first_hdr->ih_flags_fragoff & HTONS(IH_DONT_FRAG))
	{
		icmp_snd_unreachable(ip_port->ip_port, first_pack,
			ICMP_FRAGM_AND_DF);
		return NULL;
	}

	first_data_len= ntohs(first_hdr->ih_length) - first_hdr_len;
	new_first_data_len= (first_size- first_hdr_len) & ~7;
		/* data goes in 8 byte chuncks */
	second_data_len= first_data_len-new_first_data_len;
	second_pack= bf_cut(first_pack, first_hdr_len+
		new_first_data_len, second_data_len);
	tmp_pack= first_pack;
	first_data_len= new_first_data_len;
	first_pack= bf_cut (tmp_pack, 0, first_hdr_len+first_data_len);
	bf_afree(tmp_pack);
	tmp_pack= bf_memreq(first_hdr_len);
	tmp_pack->acc_next= second_pack;
	second_pack= tmp_pack;
	second_hdr= (ip_hdr_t *)ptr2acc_data(second_pack);
	*second_hdr= *first_hdr;
	second_hdr->ih_flags_fragoff= htons(
		ntohs(first_hdr->ih_flags_fragoff)+(first_data_len/8));

	first_opt_size= first_hdr_len-IP_MIN_HDR_SIZE;
	second_opt_size= 0;
	if (first_opt_size)
	{
		first_pack= bf_packIffLess (first_pack,
			first_hdr_len);
		first_hdr= (ip_hdr_t *)ptr2acc_data(first_pack);
		assert (first_pack->acc_length>=first_hdr_len);
		first_optptr= (u8_t *)ptr2acc_data(first_pack)+
			IP_MIN_HDR_SIZE;
		second_optptr= (u8_t *)ptr2acc_data(
			second_pack)+IP_MIN_HDR_SIZE;
		i= 0;
		while (i<first_opt_size)
		{
			switch (*first_optptr & IP_OPT_NUMBER)
			{
			case 0:
			case 1:
				optlen= 1;
				break;
			default:
				optlen= first_optptr[1];
				break;
			}
			assert (i + optlen <= first_opt_size);
			i += optlen;
			if (*first_optptr & IP_OPT_COPIED)
			{
				second_opt_size += optlen;
				while (optlen--)
					*second_optptr++=
						*first_optptr++;
			}
			else
				first_optptr += optlen;
		}
		while (second_opt_size & 3)
		{
			*second_optptr++= 0;
			second_opt_size++;
		}
	}
	second_hdr_len= IP_MIN_HDR_SIZE + second_opt_size;

	second_hdr->ih_vers_ihl= second_hdr->ih_vers_ihl & 0xf0
		+ (second_hdr_len/4);
	second_hdr->ih_length= htons(second_data_len+
		second_hdr_len);
	second_pack->acc_length= second_hdr_len;

	assert(first_pack->acc_linkC == 1);
	assert(first_pack->acc_buffer->buf_linkC == 1);

	first_hdr->ih_flags_fragoff |= HTONS(IH_MORE_FRAGS);
	first_hdr->ih_length= htons(first_data_len+
		first_hdr_len);
	assert (!(second_hdr->ih_flags_fragoff & HTONS(IH_DONT_FRAG)));

	ip_hdr_chksum(first_hdr, first_hdr_len);
	if (second_data_len+second_hdr_len <= first_size)
	{
		/* second_pack will not be split any further, so we have to
		 * calculate the header checksum.
		 */
		ip_hdr_chksum(second_hdr, second_hdr_len);
	}

	*ref_last= second_pack;
	return first_pack;
}

PRIVATE void error_reply (ip_fd, error)
ip_fd_t *ip_fd;
int error;
{
	if ((*ip_fd->if_get_userdata)(ip_fd->if_srfd, (size_t)error,
		(size_t)0, FALSE))
	{
#if !CRAMPED
		ip_panic(( "can't error_reply" ));
#endif
	}
}

/*
 * $PchId: ip_write.c,v 1.7.1.1.1.1 2001/01/22 19:59:07 philip Exp $
 */

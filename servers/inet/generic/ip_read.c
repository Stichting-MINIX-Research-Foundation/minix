/*
ip_read.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "clock.h"
#include "event.h"
#include "type.h"

#include "assert.h"
#include "icmp_lib.h"
#include "io.h"
#include "ip.h"
#include "ip_int.h"
#include "ipr.h"

THIS_FILE

FORWARD ip_ass_t *find_ass_ent ARGS(( ip_port_t *ip_port, U16_t id,
	int proto, ipaddr_t src, ipaddr_t dst ));
FORWARD acc_t *merge_frags ARGS(( acc_t *first, acc_t *second ));
FORWARD int ip_frag_chk ARGS(( acc_t *pack ));
FORWARD acc_t *reassemble ARGS(( ip_port_t *ip_port, acc_t *pack, 
	ip_hdr_t *ip_hdr ));
FORWARD int broadcast_dst ARGS(( ip_port_t *ip_port, ipaddr_t dest ));
FORWARD void packet2user ARGS(( ip_fd_t *ip_fd, acc_t *pack,
	time_t exp_time ));

PUBLIC int ip_read (fd, count)
int fd;
size_t count;
{
	ip_fd_t *ip_fd;
	acc_t *pack;

	ip_fd= &ip_fd_table[fd];
	if (!(ip_fd->if_flags & IFF_OPTSET))
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, EBADMODE,
			(acc_t *)0, FALSE);

	ip_fd->if_rd_count= count;

	ip_fd->if_flags |= IFF_READ_IP;
	if (ip_fd->if_rdbuf_head)
	{
		if (get_time() <= ip_fd->if_exp_time)
		{
			pack= ip_fd->if_rdbuf_head;
			ip_fd->if_rdbuf_head= pack->acc_ext_link;
			packet2user (ip_fd, pack, ip_fd->if_exp_time);
			assert(!(ip_fd->if_flags & IFF_READ_IP));
			return NW_OK;
		}
		while (ip_fd->if_rdbuf_head)
		{
			pack= ip_fd->if_rdbuf_head;
			ip_fd->if_rdbuf_head= pack->acc_ext_link;
			bf_afree(pack);
		}
	}
	return NW_SUSPEND;
}

PRIVATE acc_t *reassemble (ip_port, pack, pack_hdr)
ip_port_t *ip_port;
acc_t *pack;
ip_hdr_t *pack_hdr;
{
	ip_ass_t *ass_ent;
	size_t pack_hdr_len, pack_data_len, pack_offset, tmp_offset;
	u16_t pack_flags_fragoff;
	acc_t *prev_acc, *curr_acc, *next_acc, *head_acc, *tmp_acc;
	ip_hdr_t *tmp_hdr;
	time_t first_time;

	ass_ent= find_ass_ent (ip_port, pack_hdr->ih_id,
		pack_hdr->ih_proto, pack_hdr->ih_src, pack_hdr->ih_dst);

	pack_flags_fragoff= ntohs(pack_hdr->ih_flags_fragoff);
	pack_hdr_len= (pack_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;
	pack_data_len= ntohs(pack_hdr->ih_length)-pack_hdr_len;
	pack_offset= (pack_flags_fragoff & IH_FRAGOFF_MASK)*8;
	pack->acc_ext_link= NULL;

	head_acc= ass_ent->ia_frags;
	ass_ent->ia_frags= NULL;
	if (head_acc == NULL)
	{
		ass_ent->ia_frags= pack;
		return NULL;
	}

	prev_acc= NULL;
	curr_acc= NULL;
	next_acc= head_acc;

	while(next_acc)
	{
		tmp_hdr= (ip_hdr_t *)ptr2acc_data(next_acc);
		tmp_offset= (ntohs(tmp_hdr->ih_flags_fragoff) &
			IH_FRAGOFF_MASK)*8;

		if (pack_offset < tmp_offset)
			break;

		prev_acc= curr_acc;
		curr_acc= next_acc;
		next_acc= next_acc->acc_ext_link;
	}
	if (curr_acc == NULL)
	{
		assert(prev_acc == NULL);
		assert(next_acc != NULL);

		curr_acc= merge_frags(pack, next_acc);
		head_acc= curr_acc;
	}
	else
	{
		curr_acc= merge_frags(curr_acc, pack);
		if (next_acc != NULL)
			curr_acc= merge_frags(curr_acc, next_acc);
		if (prev_acc != NULL)
			prev_acc->acc_ext_link= curr_acc;
		else
			head_acc= curr_acc;
	}
	ass_ent->ia_frags= head_acc;

	pack= ass_ent->ia_frags;
	pack_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	pack_flags_fragoff= ntohs(pack_hdr->ih_flags_fragoff);

	if (!(pack_flags_fragoff & (IH_FRAGOFF_MASK|IH_MORE_FRAGS)))
		/* it's now a complete packet */
	{
		first_time= ass_ent->ia_first_time;

		ass_ent->ia_frags= 0;
		ass_ent->ia_first_time= 0;

		while (pack->acc_ext_link)
		{
			tmp_acc= pack->acc_ext_link;
			pack->acc_ext_link= tmp_acc->acc_ext_link;
			bf_afree(tmp_acc);
		}
		if ((ass_ent->ia_min_ttl) * HZ + first_time <
			get_time())
			icmp_snd_time_exceeded(ip_port->ip_port, pack,
				ICMP_FRAG_REASSEM);
		else
			return pack;
	}
	return NULL;
}

PRIVATE acc_t *merge_frags (first, second)
acc_t *first, *second;
{
	ip_hdr_t *first_hdr, *second_hdr;
	size_t first_hdr_size, second_hdr_size, first_datasize, second_datasize,
		first_offset, second_offset;
	acc_t *cut_second, *tmp_acc;

	if (!second)
	{
		first->acc_ext_link= NULL;
		return first;
	}

assert (first->acc_length >= IP_MIN_HDR_SIZE);
assert (second->acc_length >= IP_MIN_HDR_SIZE);

	first_hdr= (ip_hdr_t *)ptr2acc_data(first);
	first_offset= (ntohs(first_hdr->ih_flags_fragoff) &
		IH_FRAGOFF_MASK) * 8;
	first_hdr_size= (first_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;
	first_datasize= ntohs(first_hdr->ih_length) - first_hdr_size;

	second_hdr= (ip_hdr_t *)ptr2acc_data(second);
	second_offset= (ntohs(second_hdr->ih_flags_fragoff) &
		IH_FRAGOFF_MASK) * 8;
	second_hdr_size= (second_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;
	second_datasize= ntohs(second_hdr->ih_length) - second_hdr_size;

	assert (first_hdr_size + first_datasize == bf_bufsize(first));
	assert (second_hdr_size + second_datasize == bf_bufsize(second));
	assert (second_offset >= first_offset);

	if (second_offset > first_offset+first_datasize)
	{
		DBLOCK(1, printf("ip fragments out of order\n"));
		first->acc_ext_link= second;
		return first;
	}

	if (second_offset + second_datasize <= first_offset +
		first_datasize)
	{
		/* May cause problems if we try to merge. */
		bf_afree(first);
		return second;
	}

	if (!(second_hdr->ih_flags_fragoff & HTONS(IH_MORE_FRAGS)))
		first_hdr->ih_flags_fragoff &= ~HTONS(IH_MORE_FRAGS);

	second_datasize= second_offset+second_datasize-(first_offset+
		first_datasize);
	cut_second= bf_cut(second, second_hdr_size + first_offset+
		first_datasize-second_offset, second_datasize);
	tmp_acc= second->acc_ext_link;
	bf_afree(second);
	second= tmp_acc;

	first_datasize += second_datasize;
	first_hdr->ih_length= htons(first_hdr_size + first_datasize);

	first= bf_append (first, cut_second);
	first->acc_ext_link= second;

assert (first_hdr_size + first_datasize == bf_bufsize(first));

	return first;
}

PRIVATE ip_ass_t *find_ass_ent (ip_port, id, proto, src, dst)
ip_port_t *ip_port;
u16_t id;
ipproto_t proto;
ipaddr_t src;
ipaddr_t dst;
{
	ip_ass_t *new_ass_ent, *tmp_ass_ent;
	int i;
	acc_t *tmp_acc, *curr_acc;

	new_ass_ent= 0;

	for (i=0, tmp_ass_ent= ip_ass_table; i<IP_ASS_NR; i++,
		tmp_ass_ent++)
	{
		if (!tmp_ass_ent->ia_frags && tmp_ass_ent->ia_first_time)
		{
			DBLOCK(1,
		printf("strange ip_ass entry (can be a race condition)\n"));
			continue;
		}

		if ((tmp_ass_ent->ia_srcaddr == src) &&
			(tmp_ass_ent->ia_dstaddr == dst) &&
			(tmp_ass_ent->ia_proto == proto) &&
			(tmp_ass_ent->ia_id == id) &&
			(tmp_ass_ent->ia_port == ip_port))
		{
			return tmp_ass_ent;
		}
		if (!new_ass_ent || tmp_ass_ent->ia_first_time <
			new_ass_ent->ia_first_time)
		{
			new_ass_ent= tmp_ass_ent;
		}
	}

	if (new_ass_ent->ia_frags)
	{
		DBLOCK(1, printf("old frags id= %u, proto= %u, src= ",
			ntohs(new_ass_ent->ia_id),
			ntohs(new_ass_ent->ia_proto));
			writeIpAddr(new_ass_ent->ia_srcaddr); printf(" dst= ");
			writeIpAddr(new_ass_ent->ia_dstaddr); printf(": ");
			ip_print_frags(new_ass_ent->ia_frags); printf("\n"));
		curr_acc= new_ass_ent->ia_frags->acc_ext_link;
		while (curr_acc)
		{
			tmp_acc= curr_acc->acc_ext_link;
			bf_afree(curr_acc);
			curr_acc= tmp_acc;
		}
		curr_acc= new_ass_ent->ia_frags;
		new_ass_ent->ia_frags= 0;
		icmp_snd_time_exceeded(ip_port->ip_port, curr_acc,
			ICMP_FRAG_REASSEM);
	}
	new_ass_ent->ia_min_ttl= IP_MAX_TTL;
	new_ass_ent->ia_port= ip_port;
	new_ass_ent->ia_first_time= get_time();
	new_ass_ent->ia_srcaddr= src;
	new_ass_ent->ia_dstaddr= dst;
	new_ass_ent->ia_proto= proto;
	new_ass_ent->ia_id= id;

	return new_ass_ent;
}

PRIVATE int ip_frag_chk(pack)
acc_t *pack;
{
	ip_hdr_t *ip_hdr;
	int hdr_len;

	if (pack->acc_length < sizeof(ip_hdr_t))
	{
		DBLOCK(1, printf("wrong length\n"));
		return FALSE;
	}

	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);

	hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;
	if (pack->acc_length < hdr_len)
	{
		DBLOCK(1, printf("wrong length\n"));

		return FALSE;
	}

	if (((ip_hdr->ih_vers_ihl >> 4) & IH_VERSION_MASK) !=
		IP_VERSION)
	{
		DBLOCK(1, printf("wrong version (ih_vers_ihl=0x%x)\n",
			ip_hdr->ih_vers_ihl));
		return FALSE;
	}
	if (ntohs(ip_hdr->ih_length) != bf_bufsize(pack))
	{
		DBLOCK(1, printf("wrong size\n"));

		return FALSE;
	}
	if ((u16_t)~oneC_sum(0, (u16_t *)ip_hdr, hdr_len))
	{
		DBLOCK(1, printf("packet with wrong checksum (= %x)\n", 
			(u16_t)~oneC_sum(0, (u16_t *)ip_hdr, hdr_len)));
		return FALSE;
	}
	if (hdr_len>IP_MIN_HDR_SIZE && ip_chk_hdropt((u8_t *)
		(ptr2acc_data(pack) + IP_MIN_HDR_SIZE),
		hdr_len-IP_MIN_HDR_SIZE))
	{
		DBLOCK(1, printf("packet with wrong options\n"));
		return FALSE;
	}
	return TRUE;
}

PRIVATE void packet2user (ip_fd, pack, exp_time)
ip_fd_t *ip_fd;
acc_t *pack;
time_t exp_time;
{
	acc_t *tmp_pack;
	ip_hdr_t *ip_hdr;
	int result, ip_hdr_len;
	size_t size, transf_size;

	assert (ip_fd->if_flags & IFF_INUSE);
	if (!(ip_fd->if_flags & IFF_READ_IP))
	{
		if (pack->acc_linkC != 1)
		{
			tmp_pack= bf_dupacc(pack);
			bf_afree(pack);
			pack= tmp_pack;
			tmp_pack= NULL;
		}
		pack->acc_ext_link= NULL;
		if (ip_fd->if_rdbuf_head == NULL)
		{
			ip_fd->if_rdbuf_head= pack;
			ip_fd->if_exp_time= exp_time;
		}
		else
			ip_fd->if_rdbuf_tail->acc_ext_link= pack;
		ip_fd->if_rdbuf_tail= pack;
		return;
	}

	size= bf_bufsize (pack);
	if (ip_fd->if_ipopt.nwio_flags & NWIO_RWDATONLY)
	{

		pack= bf_packIffLess (pack, IP_MIN_HDR_SIZE);
		assert (pack->acc_length >= IP_MIN_HDR_SIZE);

		ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
		ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) * 4;

		assert (size >= ip_hdr_len);
		size -= ip_hdr_len;
		pack= bf_delhead(pack, ip_hdr_len);
	}

	if (size>ip_fd->if_rd_count)
	{
		tmp_pack= bf_cut (pack, 0, ip_fd->if_rd_count);
		bf_afree(pack);
		pack= tmp_pack;
		transf_size= ip_fd->if_rd_count;
	}
	else
		transf_size= size;

	if (ip_fd->if_put_pkt)
	{
		(*ip_fd->if_put_pkt)(ip_fd->if_srfd, pack, transf_size);
		return;
	}

	result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
		(size_t)0, pack, FALSE);
	if (result >= 0)
		if (size > transf_size)
			result= EPACKSIZE;
		else
			result= transf_size;

	ip_fd->if_flags &= ~IFF_READ_IP;
	result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result,
			(acc_t *)0, FALSE);
	assert (result >= 0);
	return;
}

PUBLIC void ip_port_arrive (ip_port, pack, ip_hdr)
ip_port_t *ip_port;
acc_t *pack;
ip_hdr_t *ip_hdr;
{
	ip_fd_t *ip_fd, *first_fd, *share_fd;
	ip_hdr_t *hdr;
	int port_nr;
	unsigned long ip_pack_stat;
	int i;
	int hash, proto;
	time_t exp_time;

	assert (pack->acc_linkC>0);
	assert (pack->acc_length >= IP_MIN_HDR_SIZE);

	if (ntohs(ip_hdr->ih_flags_fragoff) & (IH_FRAGOFF_MASK|IH_MORE_FRAGS))
	{
		pack= reassemble (ip_port, pack, ip_hdr);
		if (!pack)
			return;
		assert (pack->acc_length >= IP_MIN_HDR_SIZE);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
		assert (!(ntohs(ip_hdr->ih_flags_fragoff) &
			(IH_FRAGOFF_MASK|IH_MORE_FRAGS)));
	}

	exp_time= get_time() + (ip_hdr->ih_ttl+1) * HZ;

	if (ip_hdr->ih_dst == ip_port->ip_ipaddr)
		ip_pack_stat= NWIO_EN_LOC;
	else
		ip_pack_stat= NWIO_EN_BROAD;

	proto= ip_hdr->ih_proto;
	hash= proto & (IP_PROTO_HASH_NR-1);

	first_fd= NULL;
	for (i= 0; i<2; i++)
	{
		share_fd= NULL;

		ip_fd= (i == 0) ? ip_port->ip_proto_any :
			ip_port->ip_proto[hash];
		for (; ip_fd; ip_fd= ip_fd->if_proto_next)
		{
			if (i && ip_fd->if_ipopt.nwio_proto != proto)
				continue;
			if (!(ip_fd->if_ipopt.nwio_flags & ip_pack_stat))
				continue;
			if ((ip_fd->if_ipopt.nwio_flags & NWIO_REMSPEC) &&
				ip_hdr->ih_src != ip_fd->if_ipopt.nwio_rem)
			{
				continue;
			}
			if ((ip_fd->if_ipopt.nwio_flags & NWIO_ACC_MASK) ==
				NWIO_SHARED)
			{
				if (!share_fd)
				{
					share_fd= ip_fd;
					continue;
				}
				if (!ip_fd->if_rdbuf_head)
					share_fd= ip_fd;
				continue;
			}
			if (!first_fd)
			{
				first_fd= ip_fd;
				continue;
			}
			pack->acc_linkC++;
			packet2user(ip_fd, pack, exp_time);

		}
		if (share_fd)
		{
			pack->acc_linkC++;
			packet2user(share_fd, pack, exp_time);
		}
	}
	if (first_fd)
	{
		if (first_fd->if_put_pkt &&
			(first_fd->if_flags & IFF_READ_IP) &&
			!(first_fd->if_ipopt.nwio_flags & NWIO_RWDATONLY))
		{
			(*first_fd->if_put_pkt)(first_fd->if_srfd, pack,
				ntohs(ip_hdr->ih_length));
		}
		else
			packet2user(first_fd, pack, exp_time);
	}
	else
	{
		if (ip_pack_stat == NWIO_EN_LOC)
		{
			DBLOCK(0x01,
			printf("ip_port_arrive: dropping packet for proto %d\n",
				proto));
		}
		else
		{
			DBLOCK(0x20, printf("dropping packet for proto %d\n",
				proto));
		}
		bf_afree(pack);
	}
}

PUBLIC void ip_arrived(ip_port, pack)
ip_port_t *ip_port;
acc_t *pack;
{
	ip_port_t *next_port;
	ip_hdr_t *ip_hdr;
	iroute_t *iroute;
	ipaddr_t dest;
	nettype_t nettype;
	int ip_frag_len, ip_hdr_len;
	size_t pack_size;
	acc_t *tmp_pack;
	int broadcast;

	pack_size= bf_bufsize(pack);

	if (pack_size < IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("wrong acc_length\n"));
		bf_afree(pack);
		return;
	}
	pack= bf_align(pack, IP_MIN_HDR_SIZE, 4);
	pack= bf_packIffLess(pack, IP_MIN_HDR_SIZE);
assert (pack->acc_length >= IP_MIN_HDR_SIZE);

	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	if (ip_hdr_len>IP_MIN_HDR_SIZE)
	{
		pack= bf_align(pack, IP_MIN_HDR_SIZE, 4);
		pack= bf_packIffLess(pack, ip_hdr_len);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	}
	ip_frag_len= ntohs(ip_hdr->ih_length);
	if (ip_frag_len<pack_size)
	{
		tmp_pack= pack;
		pack= bf_cut(tmp_pack, 0, ip_frag_len);
		bf_afree(tmp_pack);
	}

	if (!ip_frag_chk(pack))
	{
		DBLOCK(1, printf("fragment not allright\n"));
		bf_afree(pack);
		return;
	}

	/* Decide about local delivery or routing. Local delivery can happen
	 * when the destination is the local ip address, or one of the 
	 * broadcast addresses and the packet happens to be delivered 
	 * point-to-point.
	 */

	dest= ip_hdr->ih_dst;

	if (dest == ip_port->ip_ipaddr)
	{
		ip_port_arrive (ip_port, pack, ip_hdr);
		return;
	}
	if (broadcast_dst(ip_port, dest))
	{
		ip_port_arrive (ip_port, pack, ip_hdr);
		return;
	}

	/* Try to decrement the ttl field with one. */
	if (ip_hdr->ih_ttl < 2)
	{
		icmp_snd_time_exceeded(ip_port->ip_port, pack, ICMP_TTL_EXC);
		return;
	}
	ip_hdr->ih_ttl--;
	ip_hdr_chksum(ip_hdr, ip_hdr_len);

	/* Avoid routing to bad destinations. */
	nettype= ip_nettype(dest);
	if (nettype != IPNT_CLASS_A && nettype != IPNT_CLASS_B && nettype !=
		IPNT_CLASS_C)
	{
		/* Bogus destination address */
		if (nettype == IPNT_CLASS_D || nettype == IPNT_CLASS_E)
			bf_afree(pack);
		else
		{
			icmp_snd_unreachable(ip_port->ip_port, pack,
				ICMP_HOST_UNRCH);
		}
		return;
	}
	iroute= iroute_frag(ip_port->ip_port, dest);
	if (iroute == NULL || iroute->irt_dist == IRTD_UNREACHABLE)
	{
		/* Also unreachable */
		/* Finding out if we send a network unreachable is too much
		 * trouble.
		 */
		icmp_snd_unreachable(ip_port->ip_port, pack,
			ICMP_HOST_UNRCH);
		return;
	}
	next_port= &ip_port_table[iroute->irt_port];
	if (next_port != ip_port)
	{
		if (iroute->irt_gateway != 0)
		{
			/* Just send the packet to the next gateway */
			next_port->ip_dev_send(next_port, iroute->irt_gateway,
				pack, /* no bradcast */ 0);
			return;
		}
		/* The packet is for the attached network. Special addresses
		 * are the ip address of the interface and net.0 if
		 * no IP_42BSD_BCAST.
		 */
		if (dest == next_port->ip_ipaddr)
		{
			ip_port_arrive (next_port, pack, ip_hdr);
			return;
		}
		if (dest == iroute->irt_dest)
		{
#if IP_42BSD_BCAST
			broadcast= 1;
#else
			/* Bogus destination address */
			icmp_snd_dstunrch(pack);
			return;
#endif
		}
		else if (dest == (iroute->irt_dest | ~iroute->irt_subnetmask))
			broadcast= 1;
		else
			broadcast= 0;

		/* Just send the packet to it's destination */
		next_port->ip_dev_send(next_port, dest, pack, broadcast);
		return;
	}

	/* Now we know that the packet should be route over the same network
	 * as it came from. If there is a next hop gateway, we can send
	 * the packet to that gateway and send a redirect ICMP to the sender
	 * if the sender is on the attached network. If there is no gateway
	 * complain.
	 */
	if (iroute->irt_gateway == 0)
	{
#if !CRAMPED
		printf("packet should not be here, src=");
		writeIpAddr(ip_hdr->ih_src);
		printf(" dst=");
		writeIpAddr(ip_hdr->ih_dst);
		printf("\n");
#endif
		bf_afree(pack);
		return;
	}
	if (((ip_hdr->ih_src ^ ip_port->ip_ipaddr) &
		ip_port->ip_subnetmask) == 0)
	{
		/* Finding out if we can send a network redirect instead of
		 * a host redirect is too much trouble.
		 */
		pack->acc_linkC++;
		icmp_snd_redirect(ip_port->ip_port, pack,
			ICMP_REDIRECT_HOST, iroute->irt_gateway);
	}
	else
	{
#if !CRAMPED
		printf("packet is wrongly routed, src=");
		writeIpAddr(ip_hdr->ih_src);
		printf(" dst=");
		writeIpAddr(ip_hdr->ih_dst);
		printf("\n");
#endif
		bf_afree(pack);
		return;
	}
	ip_port->ip_dev_send(ip_port, iroute->irt_gateway, pack,
		/* no broadcast */ 0);
}

PUBLIC void ip_arrived_broadcast(ip_port, pack)
ip_port_t *ip_port;
acc_t *pack;
{
	ip_hdr_t *ip_hdr;
	int ip_frag_len, ip_hdr_len;
	size_t pack_size;
	acc_t *tmp_pack;

	pack_size= bf_bufsize(pack);

	if (pack_size < IP_MIN_HDR_SIZE)
	{
		DBLOCK(1, printf("wrong acc_length\n"));
		bf_afree(pack);
		return;
	}
	pack= bf_align(pack, IP_MIN_HDR_SIZE, 4);
	pack= bf_packIffLess(pack, IP_MIN_HDR_SIZE);
assert (pack->acc_length >= IP_MIN_HDR_SIZE);

	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);

	DIFBLOCK(0x20, (ip_hdr->ih_dst & HTONL(0xf0000000)) == HTONL(0xe0000000),
		printf("got multicast packet\n"));

	ip_hdr_len= (ip_hdr->ih_vers_ihl & IH_IHL_MASK) << 2;
	if (ip_hdr_len>IP_MIN_HDR_SIZE)
	{
		pack= bf_align(pack, IP_MIN_HDR_SIZE, 4);
		pack= bf_packIffLess(pack, ip_hdr_len);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);
	}
	ip_frag_len= ntohs(ip_hdr->ih_length);
	if (ip_frag_len<pack_size)
	{
		tmp_pack= pack;
		pack= bf_cut(tmp_pack, 0, ip_frag_len);
		bf_afree(tmp_pack);
	}

	if (!ip_frag_chk(pack))
	{
		DBLOCK(1, printf("fragment not allright\n"));
		bf_afree(pack);
		return;
	}

	if (!broadcast_dst(ip_port, ip_hdr->ih_dst))
	{
#if !CRAMPED
		printf(
		"ip[%d]: broadcast packet for ip-nonbroadcast addr, src=",
			ip_port->ip_port);
		writeIpAddr(ip_hdr->ih_src);
		printf(" dst=");
		writeIpAddr(ip_hdr->ih_dst);
		printf("\n");
#endif
		bf_afree(pack);
		return;
	}

	ip_port_arrive (ip_port, pack, ip_hdr);
}

PRIVATE int broadcast_dst(ip_port, dest)
ip_port_t *ip_port;
ipaddr_t dest;
{
	/* Treat class D (multicast) address as broadcasts. */
	if ((dest & HTONL(0xF0000000)) == HTONL(0xE0000000))
	{
		return 1;
	}

	/* Accept without complaint if netmask not yet configured. */
	if (!(ip_port->ip_flags & IPF_NETMASKSET))
	{
		return 1;
	}

	if (((ip_port->ip_ipaddr ^ dest) & ip_port->ip_netmask) != 0)
	{
		/* Two possibilities, 0 (iff IP_42BSD_BCAST) and -1 */
		if (dest == HTONL((ipaddr_t)-1))
			return 1;
#if IP_42BSD_BCAST
		if (dest == HTONL((ipaddr_t)0))
			return 1;
#endif
		return 0;
	}
	if (((ip_port->ip_ipaddr ^ dest) & ip_port->ip_subnetmask) != 0)
	{
		/* Two possibilities, netwerk.0 (iff IP_42BSD_BCAST) and
		 * netwerk.-1
		 */
		if ((dest & ~ip_port->ip_netmask) == ~ip_port->ip_netmask)
			return 1;
#if IP_42BSD_BCAST
		if ((dest & ~ip_port->ip_netmask) == 0)
			return 1;
#endif
		return 0;
	}

	/* Two possibilities, netwerk.subnet.0 (iff IP_42BSD_BCAST) and
	 * netwerk.subnet.-1
	 */
	if ((dest & ~ip_port->ip_subnetmask) == ~ip_port->ip_subnetmask)
		return 1;
#if IP_42BSD_BCAST
	if ((dest & ~ip_port->ip_subnetmask) == 0)
		return 1;
#endif
	return 0;
}

void ip_process_loopb(ev, arg)
event_t *ev;
ev_arg_t arg;
{
	ip_port_t *ip_port;
	acc_t *pack;

	ip_port= arg.ev_ptr;
	assert(ev == &ip_port->ip_loopb_event);

	while(pack= ip_port->ip_loopb_head)
	{
		ip_port->ip_loopb_head= pack->acc_ext_link;
		ip_arrived(ip_port, pack);
	}
}

/*
 * $PchId: ip_read.c,v 1.9 1997/01/31 08:51:39 philip Exp $
 */

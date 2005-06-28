/*
ip_ioctl.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "event.h"
#include "type.h"

#include "arp.h"
#include "assert.h"
#include "clock.h"
#include "icmp_lib.h"
#include "ip.h"
#include "ip_int.h"
#include "ipr.h"

THIS_FILE

FORWARD int ip_checkopt ARGS(( ip_fd_t *ip_fd ));
FORWARD void reply_thr_get ARGS(( ip_fd_t *ip_fd, size_t
	reply, int for_ioctl ));
FORWARD void report_addr ARGS(( ip_port_t *ip_port ));

PUBLIC int ip_ioctl (fd, req)
int fd;
ioreq_t req;
{
	ip_fd_t *ip_fd;
	ip_port_t *ip_port;
	nwio_ipopt_t *ipopt;
	nwio_ipopt_t oldopt, newopt;
	nwio_ipconf2_t *ipconf2;
	nwio_ipconf_t *ipconf;
	nwio_route_t *route_ent;
	acc_t *data;
	int result;
	unsigned int new_en_flags, new_di_flags,
		old_en_flags, old_di_flags;
	unsigned long new_flags;
	int ent_no, r;
	nwio_ipconf_t ipconf_var;

	assert (fd>=0 && fd<=IP_FD_NR);
	ip_fd= &ip_fd_table[fd];

	assert (ip_fd->if_flags & IFF_INUSE);

	switch (req)
	{
	case NWIOSIPOPT:
		ip_port= ip_fd->if_port;

		if (!(ip_port->ip_flags & IPF_IPADDRSET))
		{
			ip_fd->if_ioctl= NWIOSIPOPT;
			ip_fd->if_flags |= IFF_IOCTL_IP;
			return NW_SUSPEND;
		}
		ip_fd->if_flags &= ~IFF_IOCTL_IP;

		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 0,
			sizeof(nwio_ipopt_t), TRUE);

		data= bf_packIffLess (data, sizeof(nwio_ipopt_t));
		assert (data->acc_length == sizeof(nwio_ipopt_t));

		ipopt= (nwio_ipopt_t *)ptr2acc_data(data);
		oldopt= ip_fd->if_ipopt;
		newopt= *ipopt;

		old_en_flags= oldopt.nwio_flags & 0xffff;
		old_di_flags= (oldopt.nwio_flags >> 16) & 0xffff;
		new_en_flags= newopt.nwio_flags & 0xffff;
		new_di_flags= (newopt.nwio_flags >> 16) & 0xffff;
		if (new_en_flags & new_di_flags)
		{
			bf_afree(data);
			reply_thr_get (ip_fd, EBADMODE, TRUE);
			return NW_OK;
		}

		/* NWIO_ACC_MASK */
		if (new_di_flags & NWIO_ACC_MASK)
		{
			bf_afree(data);
			reply_thr_get (ip_fd, EBADMODE, TRUE);
			return NW_OK;
			/* access modes can't be disable */
		}

		if (!(new_en_flags & NWIO_ACC_MASK))
			new_en_flags |= (old_en_flags & NWIO_ACC_MASK);

		/* NWIO_LOC_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_LOC_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_LOC_MASK);
			new_di_flags |= (old_di_flags & NWIO_LOC_MASK);
		}

		/* NWIO_BROAD_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_BROAD_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_BROAD_MASK);
			new_di_flags |= (old_di_flags & NWIO_BROAD_MASK);
		}

		/* NWIO_REM_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_REM_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_REM_MASK);
			new_di_flags |= (old_di_flags & NWIO_REM_MASK);
			newopt.nwio_rem= oldopt.nwio_rem;
		}

		/* NWIO_PROTO_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_PROTO_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_PROTO_MASK);
			new_di_flags |= (old_di_flags & NWIO_PROTO_MASK);
			newopt.nwio_proto= oldopt.nwio_proto;
		}

		/* NWIO_HDR_O_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_HDR_O_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_HDR_O_MASK);
			new_di_flags |= (old_di_flags & NWIO_HDR_O_MASK);
			newopt.nwio_tos= oldopt.nwio_tos;
			newopt.nwio_ttl= oldopt.nwio_ttl;
			newopt.nwio_df= oldopt.nwio_df;
			newopt.nwio_hdropt= oldopt.nwio_hdropt;
		}

		/* NWIO_RW_MASK */
		if (!((new_en_flags|new_di_flags) & NWIO_RW_MASK))
		{
			new_en_flags |= (old_en_flags & NWIO_RW_MASK);
			new_di_flags |= (old_di_flags & NWIO_RW_MASK);
		}

		new_flags= ((unsigned long)new_di_flags << 16) | new_en_flags;

		if ((new_flags & NWIO_RWDATONLY) && (new_flags &
			(NWIO_REMANY|NWIO_PROTOANY|NWIO_HDR_O_ANY)))
		{
			bf_afree(data);
			reply_thr_get(ip_fd, EBADMODE, TRUE);
			return NW_OK;
		}

		if (ip_fd->if_flags & IFF_OPTSET)
			ip_unhash_proto(ip_fd);

		newopt.nwio_flags= new_flags;
		ip_fd->if_ipopt= newopt;

		result= ip_checkopt(ip_fd);

		if (result<0)
			ip_fd->if_ipopt= oldopt;

		bf_afree(data);
		reply_thr_get (ip_fd, result, TRUE);
		return NW_OK;

	case NWIOGIPOPT:
		data= bf_memreq(sizeof(nwio_ipopt_t));

		ipopt= (nwio_ipopt_t *)ptr2acc_data(data);

		*ipopt= ip_fd->if_ipopt;

		result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0, data, 
									TRUE);
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result, 
							(acc_t *)0, TRUE);

	case NWIOSIPCONF2:
	case NWIOSIPCONF:
		ip_port= ip_fd->if_port;

		if (req == NWIOSIPCONF2)
		{
			data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 0, 
				sizeof(*ipconf2), TRUE);
			data= bf_packIffLess (data, sizeof(*ipconf2));
			assert (data->acc_length == sizeof(*ipconf2));

			ipconf2= (nwio_ipconf2_t *)ptr2acc_data(data);

			ipconf= &ipconf_var;
			ipconf->nwic_flags= ipconf2->nwic_flags;
			ipconf->nwic_ipaddr= ipconf2->nwic_ipaddr;
			ipconf->nwic_netmask= ipconf2->nwic_netmask;
			ipconf->nwic_flags &= ~NWIC_MTU_SET;
		}
		else
		{
			data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 0, 
				sizeof(*ipconf), TRUE);
			data= bf_packIffLess (data, sizeof(*ipconf));
			assert (data->acc_length == sizeof(*ipconf));

			ipconf= (nwio_ipconf_t *)ptr2acc_data(data);
		}
		r= ip_setconf(ip_port-ip_port_table, ipconf);
		bf_afree(data);
		return (*ip_fd->if_put_userdata)(ip_fd-> if_srfd, r, 
							(acc_t *)0, TRUE);

	case NWIOGIPCONF2:
		ip_port= ip_fd->if_port;

		if (!(ip_port->ip_flags & IPF_IPADDRSET))
		{
			ip_fd->if_ioctl= NWIOGIPCONF2;
			ip_fd->if_flags |= IFF_IOCTL_IP;
			return NW_SUSPEND;
		}
		ip_fd->if_flags &= ~IFF_IOCTL_IP;
		data= bf_memreq(sizeof(nwio_ipconf_t));
		ipconf2= (nwio_ipconf2_t *)ptr2acc_data(data);
		ipconf2->nwic_flags= NWIC_IPADDR_SET;
		ipconf2->nwic_ipaddr= ip_port->ip_ipaddr;
		ipconf2->nwic_netmask= ip_port->ip_subnetmask;
		if (ip_port->ip_flags & IPF_NETMASKSET)
			ipconf2->nwic_flags |= NWIC_NETMASK_SET;

		result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0, data, 
									TRUE);
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result, 
							(acc_t *)0, TRUE);
	
	case NWIOGIPCONF:
		ip_port= ip_fd->if_port;

		if (!(ip_port->ip_flags & IPF_IPADDRSET))
		{
			ip_fd->if_ioctl= NWIOGIPCONF;
			ip_fd->if_flags |= IFF_IOCTL_IP;
			return NW_SUSPEND;
		}
		ip_fd->if_flags &= ~IFF_IOCTL_IP;
		data= bf_memreq(sizeof(*ipconf));
		ipconf= (nwio_ipconf_t *)ptr2acc_data(data);
		ipconf->nwic_flags= NWIC_IPADDR_SET;
		ipconf->nwic_ipaddr= ip_port->ip_ipaddr;
		ipconf->nwic_netmask= ip_port->ip_subnetmask;
		if (ip_port->ip_flags & IPF_NETMASKSET)
			ipconf->nwic_flags |= NWIC_NETMASK_SET;
		ipconf->nwic_mtu= ip_port->ip_mtu;

		result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0, data, 
									TRUE);
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result, 
							(acc_t *)0, TRUE);

	case NWIOGIPOROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		ent_no= route_ent->nwr_ent_no;
		bf_afree(data);

		data= bf_memreq(sizeof(nwio_route_t));
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_get_oroute(ent_no, route_ent);
		if (result < 0)
			bf_afree(data);
		else
		{
			assert(result == NW_OK);
			result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0,
				data, TRUE);
		}
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	case NWIOSIPOROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}
		if (!(ip_fd->if_port->ip_flags & IPF_IPADDRSET))
		{
			/* Interface is down, no changes allowed */
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EINVAL, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_add_oroute(ip_fd->if_port-ip_port_table, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway, (time_t)0, 
			route_ent->nwr_dist, route_ent->nwr_mtu,
			!!(route_ent->nwr_flags & NWRF_STATIC), 
			route_ent->nwr_pref, NULL);
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	case NWIODIPOROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_del_oroute(ip_fd->if_port-ip_port_table, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway,
			!!(route_ent->nwr_flags & NWRF_STATIC));
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	case NWIOGIPIROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		ent_no= route_ent->nwr_ent_no;
		bf_afree(data);

		data= bf_memreq(sizeof(nwio_route_t));
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_get_iroute(ent_no, route_ent);
		if (result < 0)
			bf_afree(data);
		else
		{
			assert(result == NW_OK);
			result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0,
				data, TRUE);
		}
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	case NWIOSIPIROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}
		if (!(ip_fd->if_port->ip_flags & IPF_IPADDRSET))
		{
			/* Interface is down, no changes allowed */
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EINVAL, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_add_iroute(ip_fd->if_port-ip_port_table, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway,
			(route_ent->nwr_flags & NWRF_UNREACHABLE) ? 
				IRTD_UNREACHABLE : route_ent->nwr_dist,
			route_ent->nwr_mtu,
			!!(route_ent->nwr_flags & NWRF_STATIC), NULL);
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	case NWIODIPIROUTE:
		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd,
			0, sizeof(nwio_route_t), TRUE);
		if (data == NULL)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
				EFAULT, NULL, TRUE);
		}

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_del_iroute(ip_fd->if_port-ip_port_table, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway,
			!!(route_ent->nwr_flags & NWRF_STATIC));
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

		/* The following ARP ioctls are only valid if the
		 * underlying device is an ethernet.
		 */
	case NWIOARPGIP:
	case NWIOARPGNEXT:
	case NWIOARPSIP:
	case NWIOARPDIP:
		ip_port= ip_fd->if_port;

		if (ip_port->ip_dl_type != IPDL_ETH)
		{
			return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 
				EBADIOCTL, (acc_t *)0, TRUE);
		}
		result= arp_ioctl(ip_port->ip_dl.dl_eth.de_port,
			ip_fd->if_srfd, req, ip_fd->if_get_userdata,
			ip_fd->if_put_userdata);
		assert (result != SUSPEND);
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result,
			(acc_t *)0, TRUE);

	default:
		break;
	}
	DBLOCK(1, printf("replying EBADIOCTL: 0x%x\n", req));
	return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, EBADIOCTL,
		(acc_t *)0, TRUE);
}

PUBLIC void ip_hash_proto(ip_fd)
ip_fd_t *ip_fd;
{
	ip_port_t *ip_port;
	int hash;

	ip_port= ip_fd->if_port;
	if (ip_fd->if_ipopt.nwio_flags & NWIO_PROTOANY)
	{
		ip_fd->if_proto_next= ip_port->ip_proto_any;
		ip_port->ip_proto_any= ip_fd;
	}
	else
	{
		hash= ip_fd->if_ipopt.nwio_proto & (IP_PROTO_HASH_NR-1);
		ip_fd->if_proto_next= ip_port->ip_proto[hash];
		ip_port->ip_proto[hash]= ip_fd;
	}
}

PUBLIC void ip_unhash_proto(ip_fd)
ip_fd_t *ip_fd;
{
	ip_port_t *ip_port;
	ip_fd_t *prev, *curr, **ip_fd_p;
	int hash;

	ip_port= ip_fd->if_port;
	if (ip_fd->if_ipopt.nwio_flags & NWIO_PROTOANY)
	{
		ip_fd_p= &ip_port->ip_proto_any;
	}
	else
	{
		hash= ip_fd->if_ipopt.nwio_proto & (IP_PROTO_HASH_NR-1);
		ip_fd_p= &ip_port->ip_proto[hash];
	}
	for (prev= NULL, curr= *ip_fd_p; curr;
		prev= curr, curr= curr->if_proto_next)
	{
		if (curr == ip_fd)
			break;
	}
	assert(curr);
	if (prev)
		prev->if_proto_next= curr->if_proto_next;
	else
		*ip_fd_p= curr->if_proto_next;
}

PUBLIC int ip_setconf(ip_port_nr, ipconf)
int ip_port_nr;
nwio_ipconf_t *ipconf;
{
	int i, old_ip_flags, do_report;
	ip_port_t *ip_port;
	ip_fd_t *ip_fd;
	ipaddr_t ipaddr;
	u32_t mtu;

	ip_port= &ip_port_table[ip_port_nr];

	old_ip_flags= ip_port->ip_flags;

	if (ipconf->nwic_flags & ~NWIC_FLAGS)
		return EBADMODE;

	do_report= 0;
	if (ipconf->nwic_flags & NWIC_MTU_SET)
	{
		mtu= ipconf->nwic_mtu;
		if (mtu < IP_MIN_MTU || mtu > ip_port->ip_mtu_max)
			return EINVAL;
		ip_port->ip_mtu= mtu;
		do_report= 1;
	}

	if (ipconf->nwic_flags & NWIC_NETMASK_SET)
	{
		ip_port->ip_subnetmask= ipconf->nwic_netmask;
		ip_port->ip_flags |= IPF_NETMASKSET|IPF_SUBNET_BCAST;
		if (ntohl(ip_port->ip_subnetmask) >= 0xfffffffe)
			ip_port->ip_flags &= ~IPF_SUBNET_BCAST;
		do_report= 1;
	}
	if (ipconf->nwic_flags & NWIC_IPADDR_SET)
	{
		ipaddr= ipconf->nwic_ipaddr;
		ip_port->ip_ipaddr= ipaddr;
		ip_port->ip_flags |= IPF_IPADDRSET;
		ip_port->ip_classfulmask=
			ip_netmask(ip_nettype(ipaddr));
		if (!(ip_port->ip_flags & IPF_NETMASKSET))
		{
		    ip_port->ip_subnetmask= ip_port->ip_classfulmask;
		}
		if (ipaddr == HTONL(0x00000000))
		{
			/* Special case. Use 0.0.0.0 to shutdown interface. */
			ip_port->ip_flags &= ~(IPF_IPADDRSET|IPF_NETMASKSET);
			ip_port->ip_subnetmask= HTONL(0x00000000);
		}
		(*ip_port->ip_dev_set_ipaddr)(ip_port);

		/* revive calls waiting for an ip addresses */
		for (i=0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
		{
			if (!(ip_fd->if_flags & IFF_INUSE))
				continue;
			if (ip_fd->if_port != ip_port)
				continue;
			if (ip_fd->if_flags & IFF_IOCTL_IP)
				ip_ioctl (i, ip_fd->if_ioctl);
		}
		
		do_report= 1;
	}

	ipr_chk_itab(ip_port-ip_port_table, ip_port->ip_ipaddr,
		ip_port->ip_subnetmask);
	ipr_chk_otab(ip_port-ip_port_table, ip_port->ip_ipaddr,
		ip_port->ip_subnetmask);
	if (do_report)
		report_addr(ip_port);

	return 0;
}

PRIVATE int ip_checkopt (ip_fd)
ip_fd_t *ip_fd;
{
/* bug: we don't check access modes yet */

	unsigned long flags;
	unsigned int en_di_flags;
	acc_t *pack;
	int result;

	flags= ip_fd->if_ipopt.nwio_flags;
	en_di_flags= (flags >>16) | (flags & 0xffff);

	if (flags & NWIO_HDR_O_SPEC)
	{
		result= ip_chk_hdropt (ip_fd->if_ipopt.nwio_hdropt.iho_data,
			ip_fd->if_ipopt.nwio_hdropt.iho_opt_siz);
		if (result<0)
			return result;
	}
	if ((en_di_flags & NWIO_ACC_MASK) &&
		(en_di_flags & NWIO_LOC_MASK) &&
		(en_di_flags & NWIO_BROAD_MASK) &&
		(en_di_flags & NWIO_REM_MASK) &&
		(en_di_flags & NWIO_PROTO_MASK) &&
		(en_di_flags & NWIO_HDR_O_MASK) &&
		(en_di_flags & NWIO_RW_MASK))
	{
		ip_fd->if_flags |= IFF_OPTSET;

		ip_hash_proto(ip_fd);
	}

	else
		ip_fd->if_flags &= ~IFF_OPTSET;

	while (ip_fd->if_rdbuf_head)
	{
		pack= ip_fd->if_rdbuf_head;
		ip_fd->if_rdbuf_head= pack->acc_ext_link;
		bf_afree(pack);
	}
	return NW_OK;
}

PRIVATE void reply_thr_get(ip_fd, reply, for_ioctl)
ip_fd_t *ip_fd;
size_t reply;
int for_ioctl;
{
	acc_t *result;
	result= (ip_fd->if_get_userdata)(ip_fd->if_srfd, reply,
		(size_t)0, for_ioctl);
	assert (!result);
}

PRIVATE void report_addr(ip_port)
ip_port_t *ip_port;
{
	int i, hdr_len;
	ip_fd_t *ip_fd;
	acc_t *pack;
	ip_hdr_t *ip_hdr;

	pack= bf_memreq(IP_MIN_HDR_SIZE);
	ip_hdr= (ip_hdr_t *)ptr2acc_data(pack);

	hdr_len= IP_MIN_HDR_SIZE;
	ip_hdr->ih_vers_ihl= (IP_VERSION << 4) | (hdr_len/4);
	ip_hdr->ih_tos= 0;
	ip_hdr->ih_length= htons(ip_port->ip_mtu);
	ip_hdr->ih_id= 0;
	ip_hdr->ih_flags_fragoff= 0;
	ip_hdr->ih_ttl= 0;
	ip_hdr->ih_proto= 0;
	ip_hdr->ih_src= ip_port->ip_ipaddr;
	ip_hdr->ih_dst= ip_port->ip_subnetmask;
	ip_hdr_chksum(ip_hdr, hdr_len);

	for (i=0, ip_fd= ip_fd_table; i<IP_FD_NR; i++, ip_fd++)
	{
		if (!(ip_fd->if_flags & IFF_INUSE))
		{
			continue;
		}
		if (ip_fd->if_port != ip_port)
		{
			continue;
		}

		/* Deliver packet to user */
		pack->acc_linkC++;
		ip_packet2user(ip_fd, pack, 255, IP_MIN_HDR_SIZE);
	}
	bf_afree(pack); pack= NULL;
}

/*
 * $PchId: ip_ioctl.c,v 1.22 2004/08/03 11:10:08 philip Exp $
 */

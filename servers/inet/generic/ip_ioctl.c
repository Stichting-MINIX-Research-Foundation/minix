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

PUBLIC int ip_ioctl (fd, req)
int fd;
ioreq_t req;
{
	ip_fd_t *ip_fd;
	ip_port_t *ip_port;
	nwio_ipopt_t *ipopt;
	nwio_ipopt_t oldopt, newopt;
	nwio_ipconf_t *ipconf;
	nwio_route_t *route_ent;
	acc_t *data;
	int result;
	unsigned int new_en_flags, new_di_flags,
		old_en_flags, old_di_flags;
	unsigned long new_flags;
	int old_ip_flags;
	int ent_no;

	assert (fd>=0 && fd<=IP_FD_NR);
	ip_fd= &ip_fd_table[fd];

	assert (ip_fd->if_flags & IFF_INUSE);

	switch (req)
	{
	case NWIOSIPOPT:
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

	case NWIOSIPCONF:
		ip_port= ip_fd->if_port;

		data= (*ip_fd->if_get_userdata)(ip_fd->if_srfd, 0, 
						sizeof(nwio_ipconf_t), TRUE);

		data= bf_packIffLess (data, sizeof(nwio_ipconf_t));
		assert (data->acc_length == sizeof(nwio_ipconf_t));

		old_ip_flags= ip_port->ip_flags;

		ipconf= (nwio_ipconf_t *)ptr2acc_data(data);

		if (ipconf->nwic_flags & ~NWIC_FLAGS)
		{
			bf_afree(data);
			return (*ip_fd->if_put_userdata)(ip_fd-> if_srfd, 
						EBADMODE, (acc_t *)0, TRUE);
		}

		if (ipconf->nwic_flags & NWIC_IPADDR_SET)
		{
			ip_port->ip_ipaddr= ipconf->nwic_ipaddr;
			ip_port->ip_flags |= IPF_IPADDRSET;
			ip_port->ip_netmask=
				ip_netmask(ip_nettype(ipconf->nwic_ipaddr));
			if (!(ip_port->ip_flags & IPF_NETMASKSET)) {
				ip_port->ip_subnetmask= ip_port->ip_netmask;
			}
			(*ip_port->ip_dev_set_ipaddr)(ip_port);
		}
		if (ipconf->nwic_flags & NWIC_NETMASK_SET)
		{
			ip_port->ip_subnetmask= ipconf->nwic_netmask;
			ip_port->ip_flags |= IPF_NETMASKSET;
		}

		bf_afree(data);
		return (*ip_fd->if_put_userdata)(ip_fd-> if_srfd, NW_OK, 
							(acc_t *)0, TRUE);

	case NWIOGIPCONF:
		ip_port= ip_fd->if_port;

		if (!(ip_port->ip_flags & IPF_IPADDRSET))
		{
			ip_fd->if_flags |= IFF_GIPCONF_IP;
			return NW_SUSPEND;
		}
		ip_fd->if_flags &= ~IFF_GIPCONF_IP;
		data= bf_memreq(sizeof(nwio_ipconf_t));
		ipconf= (nwio_ipconf_t *)ptr2acc_data(data);
		ipconf->nwic_flags= NWIC_IPADDR_SET;
		ipconf->nwic_ipaddr= ip_port->ip_ipaddr;
		ipconf->nwic_netmask= ip_port->ip_subnetmask;
		if (ip_port->ip_flags & IPF_NETMASKSET)
			ipconf->nwic_flags |= NWIC_NETMASK_SET;

		result= (*ip_fd->if_put_userdata)(ip_fd->if_srfd, 0, data, 
									TRUE);
		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd, result, 
							(acc_t *)0, TRUE);
	
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

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_add_iroute(ip_fd->if_port->ip_port, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway,
			(route_ent->nwr_flags & NWRF_UNREACHABLE) ? 
				IRTD_UNREACHABLE : route_ent->nwr_dist,
			!!(route_ent->nwr_flags & NWRF_STATIC), NULL);
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

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
		result= ipr_del_iroute(ip_fd->if_port->ip_port, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway,
			(route_ent->nwr_flags & NWRF_UNREACHABLE) ? 
				IRTD_UNREACHABLE : route_ent->nwr_dist,
			!!(route_ent->nwr_flags & NWRF_STATIC));
		bf_afree(data);

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

		data= bf_packIffLess (data, sizeof(nwio_route_t) );
		route_ent= (nwio_route_t *)ptr2acc_data(data);
		result= ipr_add_oroute(ip_fd->if_port->ip_port, 
			route_ent->nwr_dest, route_ent->nwr_netmask, 
			route_ent->nwr_gateway, (time_t)0, 
			route_ent->nwr_dist,
			!!(route_ent->nwr_flags & NWRF_STATIC), 
			route_ent->nwr_pref, NULL);
		bf_afree(data);

		return (*ip_fd->if_put_userdata)(ip_fd->if_srfd,
			result, (acc_t *)0, TRUE);

	default:
		break;
	}
	DBLOCK(1, printf("replying EBADIOCTL\n"));
	return (*ip_fd->if_put_userdata)(ip_fd-> if_srfd, EBADIOCTL,
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

PRIVATE int ip_checkopt (ip_fd)
ip_fd_t *ip_fd;
{
/* bug: we don't check access modes yet */

	unsigned long flags;
	unsigned int en_di_flags;
	ip_port_t *port;
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

/*
 * $PchId: ip_ioctl.c,v 1.8 1996/12/17 07:56:18 philip Exp $
 */

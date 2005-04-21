/*
generic/ip_ps.c

pseudo IP specific part of the IP implementation

Created:	Apr 23, 1993 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "assert.h"
#include "type.h"
#include "buf.h"
#include "event.h"
#include "ip.h"
#include "ip_int.h"
#include "psip.h"

THIS_FILE

FORWARD void ipps_main ARGS(( ip_port_t *ip_port ));
FORWARD void ipps_set_ipaddr ARGS(( ip_port_t *ip_port ));
FORWARD int ipps_send ARGS(( struct ip_port *ip_port, ipaddr_t dest, 
						acc_t *pack, int broadcast ));

PUBLIC int ipps_init(ip_port)
ip_port_t *ip_port;
{
	int result;

	result= psip_enable(ip_port->ip_dl.dl_ps.ps_port, ip_port->ip_port);
	if (result == -1)
		return -1;
#if ZERO
	ip_port->ip_dl.dl_ps.ps_send_head= NULL;
	ip_port->ip_dl.dl_ps.ps_send_tail= NULL;
#endif
	ip_port->ip_dev_main= ipps_main;
	ip_port->ip_dev_set_ipaddr= ipps_set_ipaddr;
	ip_port->ip_dev_send= ipps_send;
	return result;
}

PUBLIC void ipps_get(ip_port_nr)
int ip_port_nr;
{
	int result;
	acc_t *pack;
	ip_port_t *ip_port;

	assert(ip_port_nr >= 0 && ip_port_nr < ip_conf_nr);
	ip_port= &ip_port_table[ip_port_nr];
	assert(ip_port->ip_dl_type == IPDL_PSIP);

	while (ip_port->ip_dl.dl_ps.ps_send_head != NULL)
	{
		pack= ip_port->ip_dl.dl_ps.ps_send_head;
		ip_port->ip_dl.dl_ps.ps_send_head= pack->acc_ext_link;
		result= psip_send(ip_port->ip_dl.dl_ps.ps_port, pack);
		if (result != NW_SUSPEND)
		{
			assert(result == NW_OK);
			continue;
		}
		pack->acc_ext_link= ip_port->ip_dl.dl_ps.ps_send_head;
		ip_port->ip_dl.dl_ps.ps_send_head= pack;
		if (pack->acc_ext_link == NULL)
			ip_port->ip_dl.dl_ps.ps_send_tail= pack;
		break;
	}
}

PUBLIC void ipps_put(ip_port_nr, pack)
int ip_port_nr;
acc_t *pack;
{
	ip_port_t *ip_port;

	assert(ip_port_nr >= 0 && ip_port_nr < ip_conf_nr);
	ip_port= &ip_port_table[ip_port_nr];
	assert(ip_port->ip_dl_type == IPDL_PSIP);
	ip_arrived(ip_port, pack);
}

PRIVATE void ipps_main(ip_port)
ip_port_t *ip_port;
{
	/* nothing to do */
}

PRIVATE void ipps_set_ipaddr(ip_port)
ip_port_t *ip_port;
{
	int i;
	ip_fd_t *ip_fd;

	/* revive calls waiting for an ip addresses */
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
		if (ip_fd->if_flags & IFF_GIPCONF_IP)
		{
			ip_ioctl (i, NWIOGIPCONF);
		}
	}
}

PRIVATE int ipps_send(ip_port, dest, pack, broadcast)
struct ip_port *ip_port;
ipaddr_t dest;
acc_t *pack;
int broadcast;
{
	int result;

	if (broadcast)
		ip_arrived_broadcast(ip_port, bf_dupacc(pack));

	if (ip_port->ip_dl.dl_ps.ps_send_head == NULL)
	{
		result= psip_send(ip_port->ip_dl.dl_ps.ps_port, pack);
		if (result != NW_SUSPEND)
		{
			assert(result == NW_OK);
			return result;
		}
		assert (ip_port->ip_dl.dl_ps.ps_send_head == NULL);
		ip_port->ip_dl.dl_ps.ps_send_head= pack;
	}
	else
		ip_port->ip_dl.dl_ps.ps_send_tail->acc_ext_link= pack;
	ip_port->ip_dl.dl_ps.ps_send_tail= pack;
	pack->acc_ext_link= NULL;

	return NW_OK;
}

/*
 * $PchId: ip_ps.c,v 1.5 1995/11/21 06:45:27 philip Exp $
 */

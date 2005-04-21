/*
ip_lib.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "buf.h"
#include "event.h"
#include "type.h"

#include "assert.h"
#include "io.h"
#include "ip_int.h"

THIS_FILE

PUBLIC ipaddr_t ip_get_netmask (hostaddr)
ipaddr_t hostaddr;
{
	return ip_netmask(ip_nettype(hostaddr));
}

PUBLIC int ip_chk_hdropt (opt, optlen)
u8_t *opt;
int optlen;
{
	int i, security_present= FALSE, lose_source_present= FALSE,
		strict_source_present= FALSE, record_route_present= FALSE,
		timestamp_present= FALSE;

assert (!(optlen & 3));
	i= 0;
	while (i<optlen)
	{
		DBLOCK(1, printf("*opt= %d\n", *opt));

		switch (*opt)
		{
		case 0x0:		/* End of Option list */
			return NW_OK;
		case 0x1:		/* No Operation */
			i++;
			opt++;
			break;
		case 0x82:		/* Security */
			if (security_present)
				return EINVAL;
			security_present= TRUE;
			if (opt[1] != 11)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case 0x83:		/* Lose Source and Record Route */
			if (lose_source_present)
			{
				DBLOCK(1, printf("snd lose soruce route\n"));
				return EINVAL;
			}
			lose_source_present= TRUE;
			if (opt[1]<3)
			{
				DBLOCK(1,
				printf("wrong length in source route\n"));
				return EINVAL;
			}
			i += opt[1];
			opt += opt[1];
			break;
		case 0x89:		/* Strict Source and Record Route */
			if (strict_source_present)
				return EINVAL;
			strict_source_present= TRUE;
			if (opt[1]<3)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case 0x7:		/* Record Route */
			if (record_route_present)
				return EINVAL;
			record_route_present= TRUE;
			if (opt[1]<3)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case 0x88:
			if (timestamp_present)
				return EINVAL;
			timestamp_present= TRUE;
			if (opt[1] != 4)
				return EINVAL;
			switch (opt[3] & 0xff)
			{
			case 0:
			case 1:
			case 3:
				break;
			default:
				return EINVAL;
			}
			i += opt[1];
			opt += opt[1];
			break;
		default:
			return EINVAL;
		}
	}
	if (i > optlen)
	{
		DBLOCK(1, printf("option of wrong length\n"));
		return EINVAL;
	}
	return NW_OK;
}

PUBLIC void ip_print_frags(acc)
acc_t *acc;
{
#if DEBUG
	ip_hdr_t *ip_hdr;
	int first;

	if (!acc)
		printf("(null)");

	for (first= 1; acc; acc= acc->acc_ext_link, first= 0)
	{
assert (acc->acc_length >= IP_MIN_HDR_SIZE);
		ip_hdr= (ip_hdr_t *)ptr2acc_data(acc);
		if (first)
		{
			writeIpAddr(ip_hdr->ih_src);
			printf(" > ");
			writeIpAddr(ip_hdr->ih_dst);
		}
		printf(" {%x:%d@%d%c}", ntohs(ip_hdr->ih_id),
			ntohs(ip_hdr->ih_length), 
			(ntohs(ip_hdr->ih_flags_fragoff) & IH_FRAGOFF_MASK)*8,
			(ntohs(ip_hdr->ih_flags_fragoff) & IH_MORE_FRAGS) ?
			'+' : '\0');
	}
#endif
}

PUBLIC ipaddr_t ip_get_ifaddr(port_nr)
int port_nr;
{
	assert(port_nr >= 0 && port_nr < ip_conf_nr);

	return ip_port_table[port_nr].ip_ipaddr;
}

PUBLIC nettype_t ip_nettype(ipaddr)
ipaddr_t ipaddr;
{
	u8_t highbyte;
	nettype_t nettype;

	ipaddr= ntohl(ipaddr);
	highbyte= (ipaddr >> 24) & 0xff;
	if (highbyte == 0)
	{
		if (ipaddr == 0)
			nettype= IPNT_ZERO;
		else
			nettype= IPNT_MARTIAN;
	}
	else if (highbyte < 127)
		nettype= IPNT_CLASS_A;
	else if (highbyte == 127)
		nettype= IPNT_LOCAL;
	else if (highbyte < 192)
		nettype= IPNT_CLASS_B;
	else if (highbyte < 224)
		nettype= IPNT_CLASS_C;
	else if (highbyte < 240)
		nettype= IPNT_CLASS_D;
	else if (highbyte < 248)
		nettype= IPNT_CLASS_E;
	else if (highbyte < 255)
		nettype= IPNT_MARTIAN;
	else
	{
		if (ipaddr == (ipaddr_t)-1)
			nettype= IPNT_BROADCAST;
		else
			nettype= IPNT_MARTIAN;
	}
	return nettype;
}

PUBLIC ipaddr_t ip_netmask(nettype)
nettype_t nettype;
{
	switch(nettype)
	{
	case IPNT_ZERO:		return HTONL(0x00000000);
	case IPNT_CLASS_A:
	case IPNT_LOCAL:	return HTONL(0xff000000);
	case IPNT_CLASS_B:	return HTONL(0xffff0000);
	case IPNT_CLASS_C:	return HTONL(0xffffff00);
	default:		return HTONL(0xffffffff);
	}
}

#if 0
PUBLIC char *ip_nettoa(nettype)
nettype_t nettype;
{
	switch(nettype)
	{
	case IPNT_ZERO:		return "zero";
	case IPNT_CLASS_A:	return "class A";
	case IPNT_LOCAL:	return "local";
	case IPNT_CLASS_B:	return "class B";
	case IPNT_CLASS_C:	return "class C";
	case IPNT_CLASS_D:	return "class D";
	case IPNT_CLASS_E:	return "class E";
	case IPNT_MARTIAN:	return "martian";
	case IPNT_BROADCAST:	return "broadcast";
	default:		return "<unknown>";
	}
}
#endif

/*
 * $PchId: ip_lib.c,v 1.6 1996/12/17 07:59:36 philip Exp $
 */

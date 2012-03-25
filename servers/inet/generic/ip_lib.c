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

ipaddr_t ip_get_netmask (ipaddr_t hostaddr)
{
	return ip_netmask(ip_nettype(hostaddr));
}

int ip_chk_hdropt (u8_t *opt, int optlen)
{
	int i, security_present= FALSE, lose_source_present= FALSE,
		strict_source_present= FALSE, record_route_present= FALSE,
		timestamp_present= FALSE;

	assert (!(optlen & 3));
	i= 0;
	while (i<optlen)
	{
		DBLOCK(2, printf("*opt= %d\n", *opt));

		switch (*opt)
		{
		case IP_OPT_EOL:	/* End of Option list */
			return NW_OK;
		case IP_OPT_NOP:	/* No Operation */
			i++;
			opt++;
			break;
		case IP_OPT_SEC:	/* Security */
			if (security_present)
				return EINVAL;
			security_present= TRUE;
			if (opt[1] != 11)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case IP_OPT_LSRR:	/* Lose Source and Record Route */
			if (lose_source_present)
			{
				DBLOCK(1, printf("2nd lose soruce route\n"));
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
		case IP_OPT_SSRR:	/* Strict Source and Record Route */
			if (strict_source_present)
				return EINVAL;
			strict_source_present= TRUE;
			if (opt[1]<3)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case IP_OPT_RR:		/* Record Route */
			if (record_route_present)
				return EINVAL;
			record_route_present= TRUE;
			if (opt[1]<3)
				return EINVAL;
			i += opt[1];
			opt += opt[1];
			break;
		case IP_OPT_TS:		/* Timestamp */
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
		case IP_OPT_RTRALT:
			if (opt[1] != 4)
				return EINVAL;
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

void ip_print_frags(acc_t *acc)
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

ipaddr_t ip_get_ifaddr(int port_nr)
{
	assert(port_nr >= 0 && port_nr < ip_conf_nr);

	return ip_port_table[port_nr].ip_ipaddr;
}

nettype_t ip_nettype(ipaddr_t ipaddr)
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

ipaddr_t ip_netmask(nettype_t nettype)
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
char *ip_nettoa(nettype_t nettype)
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
 * $PchId: ip_lib.c,v 1.10 2002/06/08 21:35:52 philip Exp $
 */

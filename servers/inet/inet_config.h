/*
inet/inet_config.h

Created:	Nov 11, 1992 by Philip Homburg

Defines values for configurable parameters. The structure definitions for
configuration information are also here.

Copyright 1995 Philip Homburg
*/

#ifndef INET__INET_CONFIG_H
#define INET__INET_CONFIG_H

/* Inet configuration file. */
#define PATH_INET_CONF	(char *) "/etc/inet.conf"

#define IP_PORT_MAX	32	/* Up to this many network devices */
extern int eth_conf_nr;		/* Number of ethernets */
extern int psip_conf_nr;	/* Number of Pseudo IP networks */
extern int ip_conf_nr;		/* Number of configured IP layers */
extern int tcp_conf_nr;		/* Number of configured TCP layers */
extern int udp_conf_nr;		/* Number of configured UDP layers */

extern dev_t ip_dev;		/* Device number of /dev/ip */

struct eth_conf
{
	char *ec_label;		/* Process label name if nonnull */
	u8_t ec_port;		/* Ethernet port for VLAN if label == NULL */
	u8_t ec_ifno;		/* Interface number of /dev/eth* */
	u16_t ec_vlan;		/* VLAN number of this net if label == NULL */
};
#define eth_is_vlan(ecp)	((ecp)->ec_label == NULL)

struct psip_conf
{
	u8_t pc_ifno;		/* Interface number of /dev/psip* */
};

struct ip_conf
{
	u8_t ic_devtype;	/* Underlying device type: Ethernet / PSIP */
	u8_t ic_port;		/* Port of underlying device */
	u8_t ic_ifno;		/* Interface number of /dev/ip*, tcp*, udp* */
};

struct tcp_conf
{
	u8_t tc_port;		/* IP port number */
};

struct udp_conf
{
	u8_t uc_port;		/* IP port number */
};

/* Types of networks. */
#define NETTYPE_ETH	1
#define NETTYPE_PSIP	2

/* To compute the minor device number for a device on an interface. */
#define if2minor(ifno, dev)	(1 + (ifno) * 8 + (dev))

#define IPSTAT_DEV	(char *) "/dev/ipstat"
#define IPSTAT_MODE	0666	/* Is this right? What about just setuid apps */
#define IPSTAT_MINOR	0	/* Minor number of /dev/ipstat */

/* Offsets of the minor device numbers within a group per interface. */
#define ETH_DEV_OFF	0
#define PSIP_DEV_OFF	0
#define IP_DEV_OFF	1
#define TCP_DEV_OFF	2
#define UDP_DEV_OFF	3

extern struct eth_conf eth_conf[IP_PORT_MAX];
extern struct psip_conf psip_conf[IP_PORT_MAX];
extern struct ip_conf ip_conf[IP_PORT_MAX];
extern struct tcp_conf tcp_conf[IP_PORT_MAX];
extern struct udp_conf udp_conf[IP_PORT_MAX];
void read_conf(void);
extern void *sbrk(int);
void *alloc(size_t size);

/* Options */
extern int ip_forward_directed_bcast;

#undef HTONL
#undef HTONS
#define HTONL htonl
#define HTONS htons

#endif /* INET__INET_CONFIG_H */

/*
 * $PchId: inet_config.h,v 1.10 2003/08/21 09:24:33 philip Exp $
 */

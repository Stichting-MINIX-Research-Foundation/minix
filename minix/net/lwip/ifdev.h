#ifndef MINIX_NET_LWIP_IFDEV_H
#define MINIX_NET_LWIP_IFDEV_H

#include <net/if.h>
#include <net/if_types.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

/*
 * NetBSD makes setting a hardware address through ifconfig(8) a whole lot
 * harder than it needs to be, namely by keeping a list of possible hardware
 * addresses and marking one of them as active.  For us, that level of extra
 * flexibility is completely useless.  In order to shield individual interface
 * modules from having to deal with the rather extended interface for the list
 * management, we maintain the list in ifdev and simply use a iop_set_hwaddr()
 * call to the modules when the active address changes.  This setting is the
 * maximum number of hardware addresses in the list maintained by ifdev.  It
 * should be at least 2, or changing hardware addresses will not be possible.
 */
#define IFDEV_NUM_HWADDRS	3

struct ifdev;
struct bpfdev_link;
struct sockaddr_dlx;

/* Interface operations table. */
struct ifdev_ops {
	err_t (* iop_init)(struct ifdev * ifdev, struct netif * netif);
	err_t (* iop_input)(struct pbuf * pbuf, struct netif * netif);
	err_t (* iop_output)(struct ifdev * ifdev, struct pbuf * pbuf,
	    struct netif * netif);
	err_t (* iop_output_v4)(struct netif * netif, struct pbuf * pbuf,
	    const ip4_addr_t * ipaddr);
	err_t (* iop_output_v6)(struct netif * netif, struct pbuf * pbuf,
	    const ip6_addr_t * ipaddr);
	void (* iop_hdrcmplt)(struct ifdev * ifdev, struct pbuf * pbuf);
	void (* iop_poll)(struct ifdev * ifdev);
	int (* iop_set_ifflags)(struct ifdev * ifdev, unsigned int ifflags);
	void (* iop_get_ifcap)(struct ifdev * ifdev, uint64_t * ifcap,
	    uint64_t * ifena);
	int (* iop_set_ifcap)(struct ifdev * ifdev, uint64_t ifcap);
	void (* iop_get_ifmedia)(struct ifdev * ifdev, int * ifcurrent,
	    int * ifactive);
	int (* iop_set_ifmedia)(struct ifdev * ifdev, int ifmedia);
	void (* iop_set_promisc)(struct ifdev * ifdev, int promisc);
	int (* iop_set_hwaddr)(struct ifdev * ifdev, const uint8_t * hwaddr);
	int (* iop_set_mtu)(struct ifdev * ifdev, unsigned int mtu);
	int (* iop_destroy)(struct ifdev * ifdev);
};

/* Hardware address list entry.  The first entry, if any, is the active one. */
struct ifdev_hwaddr {
	uint8_t ifhwa_addr[NETIF_MAX_HWADDR_LEN];
	uint8_t ifhwa_flags;
};
#define IFHWAF_VALID		0x01	/* entry contains an address */
#define IFHWAF_FACTORY		0x02	/* factory (device-given) address */

/* Interface structure. */
struct ifdev {
	TAILQ_ENTRY(ifdev) ifdev_next;	/* list of active interfaces */
	char ifdev_name[IFNAMSIZ];	/* interface name, null terminated */
	unsigned int ifdev_ifflags;	/* NetBSD-style interface flags */
	unsigned int ifdev_dlt;		/* data link type (DLT_) */
	unsigned int ifdev_promisc;	/* number of promiscuity requestors */
	struct netif ifdev_netif;	/* lwIP interface structure */
	struct if_data ifdev_data;	/* NetBSD-style interface data */
	char ifdev_v4set;		/* interface has an IPv4 address? */
	uint8_t ifdev_v6prefix[LWIP_IPV6_NUM_ADDRESSES]; /* IPv6 prefixes */
	uint8_t ifdev_v6flags[LWIP_IPV6_NUM_ADDRESSES]; /* v6 address flags */
	uint8_t ifdev_v6state[LWIP_IPV6_NUM_ADDRESSES]; /* v6 shadow states */
	uint8_t ifdev_v6scope[LWIP_IPV6_NUM_ADDRESSES]; /* cached v6 scopes */
	struct ifdev_hwaddr ifdev_hwlist[IFDEV_NUM_HWADDRS];	/* HW addr's */
	uint32_t ifdev_nd6flags;	/* ND6-related flags (ND6_IFF_) */
	const struct ifdev_ops *ifdev_ops; /* interface operations table */
	TAILQ_HEAD(, bpfdev_link) ifdev_bpf; /* list of attached BPF devices */
};

#define ifdev_get_name(ifdev)	((ifdev)->ifdev_name)
#define ifdev_get_ifflags(ifdev) ((ifdev)->ifdev_ifflags)
#define ifdev_get_dlt(ifdev)	((ifdev)->ifdev_dlt)
#define ifdev_is_promisc(ifdev)	((ifdev)->ifdev_promisc != 0)
#define ifdev_get_netif(ifdev)	(&(ifdev)->ifdev_netif)
#define ifdev_get_nd6flags(ifdev) ((ifdev)->ifdev_nd6flags)
#define ifdev_get_iftype(ifdev)	((ifdev)->ifdev_data.ifi_type)
#define ifdev_get_hwlen(ifdev)	((ifdev)->ifdev_data.ifi_addrlen)
#define ifdev_get_hdrlen(ifdev)	((ifdev)->ifdev_data.ifi_hdrlen)
#define ifdev_get_link(ifdev)	((ifdev)->ifdev_data.ifi_link_state)
#define ifdev_get_mtu(ifdev)	((ifdev)->ifdev_data.ifi_mtu)
#define ifdev_get_metric(ifdev)	((ifdev)->ifdev_data.ifi_metric)
#define ifdev_get_ifdata(ifdev)	(&(ifdev)->ifdev_data)
#define ifdev_is_loopback(ifdev) ((ifdev)->ifdev_ifflags & IFF_LOOPBACK)
#define ifdev_is_up(ifdev)	((ifdev)->ifdev_ifflags & IFF_UP)
#define ifdev_is_link_up(ifdev)	(netif_is_link_up(&(ifdev)->ifdev_netif))
#define ifdev_set_metric(ifdev, metric)	\
	((void)((ifdev)->ifdev_data.ifi_metric = (metric)))
#define ifdev_get_index(ifdev)  \
	((uint32_t)(netif_get_index(ifdev_get_netif(ifdev))))

#define ifdev_output_drop(ifdev) ((ifdev)->ifdev_data.ifi_oerrors++)

#define netif_get_ifdev(netif)	((struct ifdev *)(netif)->state)

void ifdev_init(void);
void ifdev_poll(void);

void ifdev_register(const char * name, int (* create)(const char *));

void ifdev_input(struct ifdev * ifdev, struct pbuf * pbuf,
	struct netif * netif, int to_bpf);
err_t ifdev_output(struct ifdev * ifdev, struct pbuf * pbuf,
	struct netif * netif, int to_bpf, int hdrcmplt);

void ifdev_attach_bpf(struct ifdev * ifdev, struct bpfdev_link * bpfl);
void ifdev_detach_bpf(struct ifdev * ifdev, struct bpfdev_link * bpfl);

struct ifdev *ifdev_get_by_index(uint32_t ifindex);
struct ifdev *ifdev_find_by_name(const char * name);
struct ifdev *ifdev_enum(struct ifdev * last);

int ifdev_check_name(const char * name, unsigned int * vtype_slot);

int ifdev_set_promisc(struct ifdev * ifdev);
void ifdev_clear_promisc(struct ifdev * ifdev);

int ifdev_set_ifflags(struct ifdev * ifdev, unsigned int ifflags);
void ifdev_update_ifflags(struct ifdev * ifdev, unsigned int ifflags);

void ifdev_get_ifcap(struct ifdev * ifdev, uint64_t * ifcap,
	uint64_t * ifena);
int ifdev_set_ifcap(struct ifdev * ifdev, uint64_t ifena);

int ifdev_get_ifmedia(struct ifdev * ifdev, int * ifcurrent, int * ifactive);
int ifdev_set_ifmedia(struct ifdev * ifdev, int ifmedia);

int ifdev_set_mtu(struct ifdev * ifdev, unsigned int mtu);

int ifdev_set_nd6flags(struct ifdev * ifdev, uint32_t nd6flags);

void ifdev_add(struct ifdev * ifdev, const char * name, unsigned int ifflags,
	unsigned int iftype, size_t hdrlen, size_t addrlen, unsigned int dlt,
	unsigned int mtu, uint32_t nd6flags, const struct ifdev_ops * iop);
int ifdev_remove(struct ifdev * ifdev);

struct ifdev *ifdev_get_loopback(void);

void ifdev_update_link(struct ifdev * ifdev, int link);
void ifdev_update_hwaddr(struct ifdev * ifdev, const uint8_t * hwaddr,
	int is_factory);

int ifdev_create(const char * name);
int ifdev_destroy(struct ifdev * ifdev);
const char *ifdev_enum_vtypes(unsigned int num);

#endif /* !MINIX_NET_LWIP_IFDEV_H */

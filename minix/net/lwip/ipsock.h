#ifndef MINIX_NET_LWIP_IPSOCK_H
#define MINIX_NET_LWIP_IPSOCK_H

/* IP-level socket, shared by TCP, UDP, and RAW. */
struct ipsock {
	struct sock ip_sock;		/* socket object, MUST be first */
	unsigned int ip_flags;		/* all socket flags */
	size_t ip_sndbuf;		/* send buffer size */
	size_t ip_rcvbuf;		/* receive buffer size */
};

/*
 * Socket flags.  In order to reduce memory consumption, all these flags are
 * stored in the same field (ipsock.ip_flags) and thus must not overlap between
 * the same users of the field, and that is why they are all here.  For
 * example, UDPF/PKTF/IPF should all be unique, and TCPF/IPF should be unique,
 * but UDPF/PKTF may overlap with TCPF and UDPF may overlap with RAWF.  In
 * practice, we have no UDPF or RAWF flags and plenty of space to make all
 * flags unique anyway.
 */
#define IPF_IPV6		0x0000001	/* socket is IPv6 */
#define IPF_V6ONLY		0x0000002	/* socket is IPv6 only */

#define PKTF_RECVINFO		0x0000010	/* receive ancillary PKTINFO */
#define PKTF_RECVTTL		0x0000020	/* receive ancillary TTL */
#define PKTF_RECVTOS		0x0000040	/* receive ancillary TOS */
#define PKTF_MCAWARE		0x0000080	/* owner is multicast aware */

#define TCPF_CONNECTING		0x0001000	/* attempting to connect */
#define TCPF_SENT_FIN		0x0002000	/* send FIN when possible */
#define TCPF_RCVD_FIN		0x0004000	/* received FIN from peer */
#define TCPF_FULL		0x0008000	/* PCB send buffer is full */
#define TCPF_OOM		0x0010000	/* memory allocation failed */

#define ipsock_get_sock(ip)		(&(ip)->ip_sock)
#define ipsock_is_ipv6(ip)		((ip)->ip_flags & IPF_IPV6)
#define ipsock_is_v6only(ip)		((ip)->ip_flags & IPF_V6ONLY)
#define ipsock_get_flags(ip)		((ip)->ip_flags)
#define ipsock_get_flag(ip,fl)		((ip)->ip_flags & (fl))
#define ipsock_set_flag(ip,fl)		((ip)->ip_flags |= (fl))
#define ipsock_clear_flag(ip,fl)	((ip)->ip_flags &= ~(fl))
#define ipsock_get_sndbuf(ip)		((ip)->ip_sndbuf)
#define ipsock_get_rcvbuf(ip)		((ip)->ip_rcvbuf)

/*
 * IP-level option pointers.  This is necessary because even though lwIP's
 * TCP, UDP, and RAW PCBs share the same initial fields, the C standard does
 * not permit generic access to such initial fields (due to both possible
 * padding differences and strict-aliasing rules).  The fields in this
 * structure are therefore pointers to the initial fields of each of the PCB
 * structures.  If lwIP ever groups its IP PCB fields into a single structure
 * and uses that structure as first field of each of the other PCBs, then we
 * will be able to replace this structure with a pointer to the IP PCB instead.
 * For convenience we also carry the send and receive buffer limits here.
 */
struct ipopts {
	ip_addr_t *local_ip;
	ip_addr_t *remote_ip;
	uint8_t *tos;
	uint8_t *ttl;
	size_t sndmin;
	size_t sndmax;
	size_t rcvmin;
	size_t rcvmax;
};

struct ifdev;

void ipsock_init(void);
int ipsock_socket(struct ipsock * ip, int domain, size_t sndbuf, size_t rcvbuf,
	struct sock ** sockp);
void ipsock_clone(struct ipsock * ip, struct ipsock * newip, sockid_t newid);
void ipsock_get_any_addr(struct ipsock * ip, ip_addr_t * ipaddr);
int ipsock_check_src_addr(struct ipsock * ip, ip_addr_t * ipaddr,
	int allow_mcast, struct ifdev ** ifdevp);
int ipsock_get_src_addr(struct ipsock * ip, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, ip_addr_t * local_ip,
	uint16_t local_port, int allow_mcast, ip_addr_t * ipaddr,
	uint16_t * portp);
int ipsock_get_dst_addr(struct ipsock * ip, const struct sockaddr * addr,
	socklen_t addr_len, const ip_addr_t * local_addr, ip_addr_t * dst_addr,
	uint16_t * dst_port);
void ipsock_put_addr(struct ipsock * ip, struct sockaddr * addr,
	socklen_t * addr_len, ip_addr_t * ipaddr, uint16_t port);
int ipsock_setsockopt(struct ipsock * ip, int level, int name,
	const struct sockdriver_data * data, socklen_t len,
	struct ipopts * ipopts);
int ipsock_getsockopt(struct ipsock * ip, int level, int name,
	const struct sockdriver_data * data, socklen_t * len,
	struct ipopts * ipopts);
void ipsock_get_info(struct kinfo_pcb * ki, const ip_addr_t * local_ip,
	uint16_t local_port, const ip_addr_t * remote_ip,
	uint16_t remote_port);

#endif /* !MINIX_NET_LWIP_IPSOCK_H */

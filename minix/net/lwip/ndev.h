#ifndef MINIX_NET_LWIP_NDEV_H
#define MINIX_NET_LWIP_NDEV_H

/* The maximum supported number of network device drivers. */
#define NR_NDEV		8

typedef uint32_t ndev_id_t;

struct ndev_hwaddr {
	uint8_t nhwa_addr[NDEV_HWADDR_MAX];
};

struct ndev_conf {
	uint32_t nconf_set;			/* fields to set (NDEV_SET_) */
	uint32_t nconf_mode;			/* desired mode (NDEV_MODE_) */
	struct ndev_hwaddr *nconf_mclist;	/* multicast list pointer */
	size_t nconf_mccount;			/* multicast list count */
	uint32_t nconf_caps;			/* capabilities (NDEV_CAP_) */
	uint32_t nconf_flags;			/* flags to set (NDEV_FLAG_) */
	uint32_t nconf_media;			/* media selection (IFM_) */
	struct ndev_hwaddr nconf_hwaddr;	/* desired hardware address */
};

void ndev_init(void);
void ndev_check(void);
void ndev_process(const message * m_ptr, int ipc_status);

int ndev_conf(ndev_id_t id, const struct ndev_conf * nconf);
int ndev_send(ndev_id_t id, const struct pbuf * pbuf);
int ndev_can_recv(ndev_id_t id);
int ndev_recv(ndev_id_t id, struct pbuf * pbuf);

#endif /* !MINIX_NET_LWIP_NDEV_H */

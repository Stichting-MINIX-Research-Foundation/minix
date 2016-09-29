#ifndef MINIX_NET_LWIP_ETHIF_H
#define MINIX_NET_LWIP_ETHIF_H

#include "ndev.h"

struct ethif;

void ethif_init(void);

struct ethif *ethif_add(ndev_id_t id, const char * name, uint32_t caps);
int ethif_enable(struct ethif * ethif, const char * name,
	const struct ndev_hwaddr * hwaddr, uint8_t hwaddr_len, uint32_t caps,
	uint32_t link, uint32_t media);
void ethif_disable(struct ethif * ethif);
void ethif_remove(struct ethif * ethif);

void ethif_configured(struct ethif * ethif, int32_t result);
void ethif_sent(struct ethif * ethif, int32_t result);
void ethif_received(struct ethif * ethif, int32_t result);

void ethif_status(struct ethif * ethif, uint32_t link, uint32_t media,
	uint32_t oerror, uint32_t coll, uint32_t ierror, uint32_t iqdrop);

#endif /* !MINIX_NET_LWIP_ETHIF_H */

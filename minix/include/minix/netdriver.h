#ifndef _MINIX_NETDRIVER_H
#define _MINIX_NETDRIVER_H

/*
 * Prototypes and definitions for network drivers.
 */
#include <minix/config.h>
#include <minix/endpoint.h>
#include <minix/ipc.h>
#include <minix/com.h>

#include <net/if_media.h>

/* Opaque data structure for copying in and out actual packet data. */
struct netdriver_data;

/* Network (ethernet) address structure. */
typedef struct {
	uint8_t na_addr[NDEV_HWADDR_MAX];
} netdriver_addr_t;

/* Information and function call table for network drivers. */
struct netdriver {
	const char *ndr_name;
	int (* ndr_init)(unsigned int instance, netdriver_addr_t * hwaddr,
	    uint32_t * caps, unsigned int * ticks);
	void (* ndr_stop)(void);
	void (* ndr_set_mode)(unsigned int mode,
	    const netdriver_addr_t * mcast_list, unsigned int mcast_count);
	void (* ndr_set_caps)(uint32_t caps);
	void (* ndr_set_flags)(uint32_t flags);
	void (* ndr_set_media)(uint32_t media);
	void (* ndr_set_hwaddr)(const netdriver_addr_t * hwaddr);
	ssize_t (* ndr_recv)(struct netdriver_data * data, size_t max);
	int (* ndr_send)(struct netdriver_data * data, size_t size);
	unsigned int (* ndr_get_link)(uint32_t * media);
	void (* ndr_intr)(unsigned int mask);
	void (* ndr_tick)(void);
	void (* ndr_other)(const message * m_ptr, int ipc_status);
};

/* Functions defined by libnetdriver. */
void netdriver_task(const struct netdriver * ndp);

int netdriver_init(const struct netdriver * ndp);
void netdriver_process(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr, int ipc_status);
void netdriver_terminate(void);

const char *netdriver_name(void);

void netdriver_recv(void);
void netdriver_send(void);
void netdriver_link(void);

void netdriver_stat_oerror(uint32_t count);
void netdriver_stat_coll(uint32_t count);
void netdriver_stat_ierror(uint32_t count);
void netdriver_stat_iqdrop(uint32_t count);

void netdriver_copyin(struct netdriver_data * __restrict data, size_t off,
	void * __restrict ptr, size_t size);
void netdriver_copyout(struct netdriver_data * __restrict data, size_t off,
	const void * __restrict ptr, size_t size);

void netdriver_portinb(struct netdriver_data * data, size_t off, long port,
	size_t size);
void netdriver_portoutb(struct netdriver_data * data, size_t off, long port,
	size_t size);
void netdriver_portinw(struct netdriver_data * data, size_t off, long port,
	size_t size);
void netdriver_portoutw(struct netdriver_data * data, size_t off, long port,
	size_t size);

#endif /* _MINIX_NETDRIVER_H */

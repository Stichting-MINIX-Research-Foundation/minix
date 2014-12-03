/* Prototypes and definitions for network drivers. */

#ifndef _MINIX_NETDRIVER_H
#define _MINIX_NETDRIVER_H

#include <minix/endpoint.h>
#include <minix/ipc.h>
#include <minix/com.h>

/* The flags that make up the requested receive mode. */
#define NDEV_NOMODE	DL_NOMODE		/* targeted packets only */
#define NDEV_PROMISC	DL_PROMISC_REQ		/* promiscuous mode */
#define NDEV_MULTI	DL_MULTI_REQ		/* receive multicast packets */
#define NDEV_BROAD	DL_BROAD_REQ		/* receive broadcast packets */

/*
 * For now, only ethernet-type network drivers are supported, and thus, we use
 * some ethernet-specific data structures.
 */
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

/* Opaque data structure for copying in and out actual packet data. */
struct netdriver_data;

/* Function call table for network drivers. */
struct netdriver {
	int (*ndr_init)(unsigned int instance, ether_addr_t *addr);
	void (*ndr_stop)(void);
	void (*ndr_mode)(unsigned int mode);
	ssize_t (*ndr_recv)(struct netdriver_data *data, size_t max);
	int (*ndr_send)(struct netdriver_data *data, size_t size);
	void (*ndr_stat)(eth_stat_t *stat);
	void (*ndr_intr)(unsigned int mask);
	void (*ndr_alarm)(clock_t stamp);
	void (*ndr_other)(const message *m_ptr, int ipc_status);
};

/* Functions defined by libnetdriver. */
void netdriver_task(const struct netdriver *ndp);

void netdriver_announce(void);			/* legacy; deprecated */
int netdriver_init(const struct netdriver *ndp);
void netdriver_process(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr, int ipc_status);
void netdriver_terminate(void);

void netdriver_recv(void);
void netdriver_send(void);

void netdriver_copyin(struct netdriver_data * __restrict data, size_t off,
	void * __restrict ptr, size_t size);
void netdriver_copyout(struct netdriver_data * __restrict data, size_t off,
	const void * __restrict ptr, size_t size);

void netdriver_portinb(struct netdriver_data *data, size_t off, long port,
	size_t size);
void netdriver_portoutb(struct netdriver_data *data, size_t off, long port,
	size_t size);
void netdriver_portinw(struct netdriver_data *data, size_t off, long port,
	size_t size);
void netdriver_portoutw(struct netdriver_data *data, size_t off, long port,
	size_t size);

#define netdriver_receive sef_receive_status	/* legacy; deprecated */

#endif /* _MINIX_NETDRIVER_H */

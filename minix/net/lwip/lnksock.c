/* LWIP service - lnksock.c - link sockets */
/*
 * This module contains absolutely minimal support for AF_LINK type sockets,
 * because for now we need them only to support a specific set of IOCTLs, as
 * required by for example ifconfig(8).
 */

#include "lwip.h"

/* The number of link sockets. */
#define NR_LNKSOCK	4

static struct lnksock {
	struct sock lnk_sock;		/* socket object, MUST be first */
	SIMPLEQ_ENTRY(lnksock) lnk_next; /* next in free list */
} lnk_array[NR_LNKSOCK];

static SIMPLEQ_HEAD(, lnksock) lnk_freelist;	/* list of free link sockets */

static const struct sockevent_ops lnksock_ops;

/*
 * Initialize the link sockets module.
 */
void
lnksock_init(void)
{
	unsigned int slot;

	/* Initialize the list of free link sockets. */
	SIMPLEQ_INIT(&lnk_freelist);

	for (slot = 0; slot < __arraycount(lnk_array); slot++)
		SIMPLEQ_INSERT_TAIL(&lnk_freelist, &lnk_array[slot], lnk_next);
}

/*
 * Create a link socket.
 */
sockid_t
lnksock_socket(int type, int protocol, struct sock ** sockp,
	const struct sockevent_ops ** ops)
{
	struct lnksock *lnk;

	if (type != SOCK_DGRAM)
		return EPROTOTYPE;

	if (protocol != 0)
		return EPROTONOSUPPORT;

	if (SIMPLEQ_EMPTY(&lnk_freelist))
		return ENOBUFS;

	lnk = SIMPLEQ_FIRST(&lnk_freelist);
	SIMPLEQ_REMOVE_HEAD(&lnk_freelist, lnk_next);

	*sockp = &lnk->lnk_sock;
	*ops = &lnksock_ops;
	return SOCKID_LNK | (sockid_t)(lnk - lnk_array);
}

/*
 * Free up a closed link socket.
 */
static void
lnksock_free(struct sock * sock)
{
	struct lnksock *lnk = (struct lnksock *)sock;

	SIMPLEQ_INSERT_HEAD(&lnk_freelist, lnk, lnk_next);
}

static const struct sockevent_ops lnksock_ops = {
	.sop_ioctl		= ifconf_ioctl,
	.sop_free		= lnksock_free
};

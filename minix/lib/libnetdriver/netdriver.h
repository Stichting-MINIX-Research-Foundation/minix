#ifndef _MINIX_LIB_NETDRIVER_NETDRIVER_H
#define _MINIX_LIB_NETDRIVER_NETDRIVER_H

/* Data (I/O) structure. */
struct netdriver_data {
	endpoint_t endpt;
	uint32_t id;
	size_t size;
	unsigned int count;
	iovec_s_t iovec[NDEV_IOV_MAX];
};

size_t netdriver_prepare_copy(struct netdriver_data *data, size_t offp,
	size_t size, unsigned int * indexp);

#endif /* !_MINIX_LIB_NETDRIVER_NETDRIVER_H */

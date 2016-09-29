#ifndef MINIX_NET_LWIP_UTIL_H
#define MINIX_NET_LWIP_UTIL_H

/* util.c */
int util_timeval_to_ticks(const struct timeval * tv, clock_t * ticksp);
void util_ticks_to_timeval(clock_t ticks, struct timeval * tv);
int util_copy_data(const struct sockdriver_data * data, size_t len, size_t off,
	const struct pbuf * pbuf, size_t skip, int copy_in);
ssize_t util_coalesce(char * buf, size_t max, const iovec_t * iov,
	unsigned int iovcnt);
int util_convert_err(err_t err);
int util_is_root(endpoint_t user_endpt);
ssize_t util_pcblist(struct rmib_call * call, struct rmib_oldp * oldp,
	const void *(*enum_proc)(const void *),
	void (*get_info_proc)(struct kinfo_pcb *, const void *));

/*
 * In our code, pbuf header adjustments should never fail.  This wrapper checks
 * that the pbuf_header() call succeeds, and panics otherwise.
 */
#define util_pbuf_header(pbuf,incr)					    \
	do {								    \
		if (pbuf_header((pbuf), (incr)))			    \
			panic("unexpected pbuf header adjustment failure"); \
	} while (0)

#endif /* !MINIX_NET_LWIP_UTIL_H */

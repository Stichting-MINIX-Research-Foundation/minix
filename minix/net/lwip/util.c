/* LWIP service - util.c - shared utility functions */

#include "lwip.h"

#define US	1000000			/* number of microseconds per second */

/*
 * Convert the given timeval structure to a number of clock ticks, checking
 * whether the given structure is valid and whether the resulting number of
 * ticks can be expressed as a (relative) clock ticks value.  Upon success,
 * return OK, with the number of clock ticks stored in 'ticksp'.  Upon failure,
 * return a negative error code that may be returned to userland directly.  In
 * that case, the contents of 'ticksp' are left unchanged.
 *
 * TODO: move this function into libsys and remove other redundant copies.
 */
int
util_timeval_to_ticks(const struct timeval * tv, clock_t * ticksp)
{
	clock_t ticks;

	if (tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= US)
		return EINVAL;

	if (tv->tv_sec >= TMRDIFF_MAX / sys_hz())
		return EDOM;

	ticks = tv->tv_sec * sys_hz() + (tv->tv_usec * sys_hz() + US - 1) / US;
	assert(ticks <= TMRDIFF_MAX);

	*ticksp = ticks;
	return OK;
}

/*
 * Convert the given number of clock ticks to a timeval structure.  This
 * function never fails.
 */
void
util_ticks_to_timeval(clock_t ticks, struct timeval * tv)
{

	memset(tv, 0, sizeof(*tv));
	tv->tv_sec = ticks / sys_hz();
	tv->tv_usec = (ticks % sys_hz()) * US / sys_hz();
}

/*
 * Copy data between a user process and a chain of buffers.  If the 'copy_in'
 * flag is set, the data will be copied in from the user process to the given
 * chain of buffers; otherwise, the data will be copied out from the given
 * buffer chain to the user process.  The 'data' parameter is a sockdriver-
 * supplied structure identifying the remote source or destination of the data.
 * The 'len' parameter contains the number of bytes to copy, and 'off' contains
 * the offset into the remote source or destination.  'pbuf' is a pointer to
 * the buffer chain, and 'skip' is the number of bytes to skip in the first
 * buffer on the chain.  Return OK on success, or a negative error code if the
 * copy operation failed.  This function is packet queue friendly.
 */
int
util_copy_data(const struct sockdriver_data * data, size_t len, size_t off,
	const struct pbuf * pbuf, size_t skip, int copy_in)
{
	iovec_t iov[SOCKDRIVER_IOV_MAX];
	unsigned int i;
	size_t sub, chunk;
	int r;

	while (len > 0) {
		sub = 0;

		for (i = 0; len > 0 && i < __arraycount(iov); i++) {
			assert(pbuf != NULL);

			chunk = (size_t)pbuf->len - skip;
			if (chunk > len)
				chunk = len;

			iov[i].iov_addr = (vir_bytes)pbuf->payload + skip;
			iov[i].iov_size = chunk;

			sub += chunk;
			len -= chunk;

			pbuf = pbuf->next;
			skip = 0;
		}

		if (copy_in)
			r = sockdriver_vcopyin(data, off, iov, i);
		else
			r = sockdriver_vcopyout(data, off, iov, i);
		if (r != OK)
			return r;

		off += sub;
	}

	return OK;
}

/*
 * Copy from a vector of (local) buffers to a single (local) buffer.  Return
 * the total number of copied bytes on success, or E2BIG if not all of the
 * results could be stored in the given bfufer.
 */
ssize_t
util_coalesce(char * ptr, size_t max, const iovec_t * iov, unsigned int iovcnt)
{
	size_t off, size;

	for (off = 0; iovcnt > 0; iov++, iovcnt--) {
		if ((size = iov->iov_size) > max)
			return E2BIG;

		memcpy(&ptr[off], (void *)iov->iov_addr, size);

		off += size;
		max -= size;
	}

	return off;
}

/*
 * Return TRUE if the given endpoint has superuser privileges, FALSE otherwise.
 */
int
util_is_root(endpoint_t endpt)
{

	return (getnuid(endpt) == ROOT_EUID);
}

/*
 * Convert a lwIP-provided error code (of type err_t) to a negative MINIX 3
 * error code.
 */
int
util_convert_err(err_t err)
{

	switch (err) {
	case ERR_OK:		return OK;
	case ERR_MEM:		return ENOMEM;
	case ERR_BUF:		return ENOBUFS;
	case ERR_TIMEOUT:	return ETIMEDOUT;
	case ERR_RTE:		return EHOSTUNREACH;
	case ERR_VAL:		return EINVAL;
	case ERR_USE:		return EADDRINUSE;
	case ERR_ALREADY:	return EALREADY;
	case ERR_ISCONN:	return EISCONN;
	case ERR_CONN:		return ENOTCONN;
	case ERR_IF:		return ENETDOWN;
	case ERR_ABRT:		return ECONNABORTED;
	case ERR_RST:		return ECONNRESET;
	case ERR_INPROGRESS:	return EINPROGRESS; /* should not be thrown */
	case ERR_WOULDBLOCK:	return EWOULDBLOCK; /* should not be thrown */
	case ERR_ARG:		return EINVAL;
	case ERR_CLSD:		/* should be caught as separate case */
	default:		/* should have a case here */
		printf("LWIP: unexpected error from lwIP: %d", err);
		return EGENERIC;
	}
}

/*
 * Obtain the list of protocol control blocks for a particular domain and
 * protocol.  The call may be used for requesting either IPv4 or IPv6 PCBs,
 * based on the path used to get here.  It is used for TCP, UDP, and RAW PCBs.
 */
ssize_t
util_pcblist(struct rmib_call * call, struct rmib_oldp * oldp,
	const void *(*enum_proc)(const void *),
	void (*get_info_proc)(struct kinfo_pcb *, const void *))
{
	const void *pcb;
	ip_addr_t local_ip;
	struct kinfo_pcb ki;
	ssize_t off;
	int r, size, max, domain, protocol;

	if (call->call_namelen != 4)
		return EINVAL;

	/* The first two added name fields are not used. */

	size = call->call_name[2];
	if (size < 0 || (size_t)size > sizeof(ki))
		return EINVAL;
	if (size == 0)
		size = sizeof(ki);
	max = call->call_name[3];

	domain = call->call_oname[1];
	protocol = call->call_oname[2];

	off = 0;

	for (pcb = enum_proc(NULL); pcb != NULL; pcb = enum_proc(pcb)) {
		/* Filter on IPv4/IPv6. */
		memcpy(&local_ip, &((const struct ip_pcb *)pcb)->local_ip,
		    sizeof(local_ip));

		/*
		 * lwIP does not support IPv6 sockets with IPv4-mapped IPv6
		 * addresses, and requires that those be represented as IPv4
		 * sockets instead.  We perform the appropriate conversions to
		 * make that work in general, but here we only have the lwIP
		 * PCB to go on, and that PCB may not even have an associated
		 * sock data structure.  As a result, we have to report IPv6
		 * sockets with IPv4-mapped IPv6 addresses as IPv4 sockets
		 * here.  There is little room for improvement until lwIP
		 * allows us to store a "this is really an IPv6 socket" flag in
		 * its PCBs.  As documented in the ipsock module, a partial
		 * solution would for example cause TCP sockets to "jump" from
		 * the IPv6 listing to the IPv4 listing when entering TIME_WAIT
		 * state.  The jumping already occurs now for sockets that are
		 * getting bound, but that is not as problematic.
		 */
		if ((domain == AF_INET) != IP_IS_V4(&local_ip))
			continue;

		if (rmib_inrange(oldp, off)) {
			memset(&ki, 0, sizeof(ki));

			ki.ki_pcbaddr = (uint64_t)(uintptr_t)pcb;
			ki.ki_ppcbaddr = (uint64_t)(uintptr_t)pcb;
			ki.ki_family = domain;
			ki.ki_protocol = protocol;

			get_info_proc(&ki, pcb);

			if ((r = rmib_copyout(oldp, off, &ki, size)) < OK)
				return r;
		}

		off += size;
		if (max > 0 && --max == 0)
			break;
	}

	/*
	 * Margin to limit the possible effects of the inherent race condition
	 * between receiving just the data size and receiving the actual data.
	 */
	if (oldp == NULL)
		off += PCB_SLOP * size;

	return off;
}

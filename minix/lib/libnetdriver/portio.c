/*
 * Port-based I/O routines.  These are in a separate module because most
 * drivers will not use them, and system services are statically linked.
 */
#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <assert.h>

#include "netdriver.h"

/*
 * Port-based I/O byte sequence copy routine.
 */
static void
netdriver_portb(struct netdriver_data * data, size_t off, long port,
	size_t size, int portin)
{
	size_t chunk;
	unsigned int i;
	int r, req;

	off = netdriver_prepare_copy(data, off, size, &i);

	req = portin ? DIO_SAFE_INPUT_BYTE : DIO_SAFE_OUTPUT_BYTE;

	while (size > 0) {
		chunk = data->iovec[i].iov_size - off;
		if (chunk > size)
			chunk = size;
		assert(chunk > 0);

		if ((r = sys_sdevio(req, port, data->endpt,
		    (void *)data->iovec[i].iov_grant, chunk, off)) != OK)
			panic("netdriver: port I/O failed: %d", r);

		i++;
		off = 0;
		size -= chunk;
	}
}

/*
 * Transfer bytes from hardware to a destination buffer using port-based I/O.
 */
void
netdriver_portinb(struct netdriver_data * data, size_t off, long port,
	size_t size)
{

	return netdriver_portb(data, off, port, size, TRUE /*portin*/);
}

/*
 * Transfer bytes from a source buffer to hardware using port-based I/O.
 */
void
netdriver_portoutb(struct netdriver_data * data, size_t off, long port,
	size_t size)
{

	return netdriver_portb(data, off, port, size, FALSE /*portin*/);
}

/*
 * Transfer words from hardware to a destination buffer using port-based I/O.
 */
void
netdriver_portinw(struct netdriver_data * data, size_t off, long port,
	size_t size)
{
	uint16_t buf;
	uint32_t value;
	size_t chunk;
	unsigned int i;
	int r, odd_byte;

	off = netdriver_prepare_copy(data, off, size, &i);

	odd_byte = 0;
	while (size > 0) {
		chunk = data->iovec[i].iov_size - off;
		if (chunk > size)
			chunk = size;
		assert(chunk > 0);

		if (odd_byte) {
			if ((r = sys_safecopyto(data->endpt,
			    data->iovec[i].iov_grant, off,
			    (vir_bytes)&((char *)&buf)[1], 1)) != OK)
				panic("netdriver: unable to copy data: %d", r);

			off++;
			size--;
			chunk--;
		}

		odd_byte = chunk & 1;
		chunk -= odd_byte;

		if (chunk > 0) {
			if ((r = sys_safe_insw(port, data->endpt,
			    data->iovec[i].iov_grant, off, chunk)) != OK)
				panic("netdriver: port input failed: %d", r);

			off += chunk;
			size -= chunk;
		}

		if (odd_byte) {
			if ((r = sys_inw(port, &value)) != OK)
				panic("netdriver: port input failed: %d", r);
			buf = (uint16_t)value;

			if ((r = sys_safecopyto(data->endpt,
			    data->iovec[i].iov_grant, off,
			    (vir_bytes)&((char *)&buf)[0], 1)) != OK)
				panic("netdriver: unable to copy data: %d", r);

			size--;
		}

		i++;
		off = 0;
	}
}

/*
 * Transfer words from a source buffer to hardware using port-based I/O.
 */
void
netdriver_portoutw(struct netdriver_data * data, size_t off, long port,
	size_t size)
{
	uint16_t buf;
	size_t chunk;
	unsigned int i;
	int r, odd_byte;

	off = netdriver_prepare_copy(data, off, size, &i);

	odd_byte = 0;
	while (size > 0) {
		chunk = data->iovec[i].iov_size - off;
		if (chunk > size)
			chunk = size;
		assert(chunk > 0);

		if (odd_byte) {
			if ((r = sys_safecopyfrom(data->endpt,
			    data->iovec[i].iov_grant, off,
			    (vir_bytes)&((char *)&buf)[1], 1)) != OK)
				panic("netdriver: unable to copy data: %d", r);

			if ((r = sys_outw(port, buf)) != OK)
				panic("netdriver: port output failed: %d", r);

			off++;
			size--;
			chunk--;
		}

		odd_byte = chunk & 1;
		chunk -= odd_byte;

		if (chunk > 0) {
			if ((r = sys_safe_outsw(port, data->endpt,
			    data->iovec[i].iov_grant, off, chunk)) != OK)
				panic("netdriver: port output failed: %d", r);

			off += chunk;
			size -= chunk;
		}

		if (odd_byte) {
			if ((r = sys_safecopyfrom(data->endpt,
			    data->iovec[i].iov_grant, off,
			    (vir_bytes)&((char *)&buf)[0], 1)) != OK)
				panic("netdriver: unable to copy data: %d", r);

			size--;
		}

		i++;
		off = 0;
	}

	if (odd_byte) {
		((char *)&buf)[1] = 0;

		if ((r = sys_outw(port, buf)) != OK)
			panic("netdriver: port output failed: %d", r);
	}
}

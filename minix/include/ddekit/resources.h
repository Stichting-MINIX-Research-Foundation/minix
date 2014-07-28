#ifndef _DDEKIT_RESOURCES_H
#define _DDEKIT_RESOURCES_H

#include <ddekit/ddekit.h>
#include <ddekit/types.h>


/*
 * request/release an isa DMA-channel
 */

int ddekit_request_dma(int nr);
int ddekit_release_dma(int nr);

/*
 * request/release an io-port range starting at addr start, lenght of count
 */
int ddekit_request_io(ddekit_addr_t start, ddekit_addr_t count);
int ddekit_release_io(ddekit_addr_t start, ddekit_addr_t count);

/*
 * request an IO-Memory mapping from address start to vaddr of the length count
 */
int ddekit_request_mem(ddekit_addr_t start, ddekit_addr_t count,
	ddekit_addr_t *vaddr);
int ddekit_release_mem(ddekit_addr_t start, ddekit_addr_t count);

/**
 * Read I/O port (byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned char ddekit_inb(ddekit_addr_t port);

/**
 * Read I/O port (2-byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned short ddekit_inw(ddekit_addr_t port);

/**
 * Read I/O port (4-byte)
 *
 * \param port  port to read
 *
 * \return value read from port
 */
unsigned long ddekit_inl(ddekit_addr_t port);

/**
 * Write I/O port (byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void ddekit_outb(ddekit_addr_t port, unsigned char val);

/**
 * Write I/O port (2-byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void ddekit_outw(ddekit_addr_t port, unsigned short val);

/**
 * Write I/O port (4-byte)
 *
 * \param port  port to write
 * \param val   value to write
 */
void ddekit_outl(ddekit_addr_t port, unsigned long val);


#endif

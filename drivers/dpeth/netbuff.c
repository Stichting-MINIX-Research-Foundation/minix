/*
**  File:	netbuff.c	Jun. 10, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains specific implementation of buffering
**  for network packets.
*/

#include <minix/drivers.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include "dp.h"

#if (HAVE_BUFFERS == 1)

static m_hdr_t *allocptr = NULL;
static char tx_rx_buff[8192];

/*
**  Name:	void *alloc_buff(dpeth_t *dep, int size)
**  Function:	Allocates a buffer from the common pool.
*/
void *alloc_buff(dpeth_t *dep, int size)
{
  m_hdr_t *ptr, *wrk = allocptr;
  int units = ((size + sizeof(m_hdr_t) - 1) / sizeof(m_hdr_t)) + 1;

  lock();
  for (ptr = wrk->next;; wrk = ptr, ptr = ptr->next) {
	if (ptr->size >= units) {
		/* Memory is available, carve requested size from pool */
		if (ptr->size == units) {
			wrk->next = ptr->next;
		} else {
			/* Get memory from top address */
			ptr->size -= units;
			ptr += ptr->size;
			ptr->size = units;
		}
		allocptr = wrk;
		unlock();
		return ptr + 1;
	}
	if (ptr == allocptr) break;
  }
  unlock();
  return NULL;			/* No memory available */
}

/*
**  Name:	void free_buff(dpeth_t *dep, void *blk)
**  Function:	Returns a buffer to the common pool.
*/
void free_buff(dpeth_t *dep, void *blk)
{
  m_hdr_t *wrk, *ptr = (m_hdr_t *) blk - 1;

  lock();			/* Scan linked list for the correct place */
  for (wrk = allocptr; !(ptr > wrk && ptr < wrk->next); wrk = wrk->next)
	if (wrk >= wrk->next && (ptr > wrk || ptr < wrk->next)) break;
  
  /* Check if adjacent block is free and join blocks */
  if (ptr + ptr->size == wrk->next) {
	ptr->size += wrk->next->size;
	ptr->next = wrk->next->next;
  } else
	ptr->next = wrk->next;
  if (wrk + wrk->size == ptr) {
	wrk->size += ptr->size;
	wrk->next = ptr->next;
  } else
	wrk->next = ptr;
  allocptr = wrk;		/* Point allocptr to block just released */
  unlock();
  return;
}

/*
**  Name:	void init_buff(dpeth_t *dep, buff_t **tx_buff)
**  Function:	Initalizes driver data structures.
*/
void init_buff(dpeth_t *dep, buff_t **tx_buff)
{

  /* Initializes buffer pool */
  if (allocptr == NULL) {
	m_hdr_t *rx = (m_hdr_t *) tx_rx_buff;
	rx->next = allocptr = rx;
	rx->size = 0;
	rx += 1;
	rx->next = NULL;
	rx->size = (sizeof(tx_rx_buff) / sizeof(m_hdr_t)) - 1;
	free_buff(dep, rx + 1);
	dep->de_recvq_tail = dep->de_recvq_head = NULL;
	if (tx_buff != NULL) {
		*tx_buff = alloc_buff(dep, ETH_MAX_PACK_SIZE + sizeof(buff_t));
		(*tx_buff)->size = 0;
	}
  }
  return;			/* Done */
}

/*
**  Name:	void mem2user(dpeth_t *dep, buff_t *rxbuff);
**  Function:	Copies a packet from local buffer to user area.
*/
void mem2user(dpeth_t *dep, buff_t *rxbuff)
{
  int bytes, ix = 0;
  iovec_dat_s_t *iovp = &dep->de_read_iovec;
  int r, pktsize = rxbuff->size;
  char *buffer = rxbuff->buffer;

  do {				/* Reads chuncks of packet into user buffers */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of buffer */
	if (bytes > pktsize) bytes = pktsize;

	/* Reads from Rx buffer to user area */
	r= sys_safecopyto(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_grant, 0,
		(vir_bytes)buffer, bytes);
	if (r != OK)
		panic("mem2user: sys_safecopyto failed: %d", r);
	buffer += bytes;

	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  return;
}

/*
**  Name:	void user2mem(dpeth_t *dep, buff_t *txbuff)
**  Function:	Copies a packet from user area to local buffer.
*/
void user2mem(dpeth_t *dep, buff_t *txbuff)
{
  int bytes, ix = 0;
  iovec_dat_s_t *iovp = &dep->de_write_iovec;
  int r, pktsize = txbuff->size;
  char *buffer = txbuff->buffer;

  do {				/* Reads chuncks of packet from user buffers */

	bytes = iovp->iod_iovec[ix].iov_size;	/* Size of buffer */
	if (bytes > pktsize) bytes = pktsize;
	r= sys_safecopyfrom(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_grant,
		0, (vir_bytes)buffer, bytes);
	if (r != OK)
		panic("user2mem: sys_safecopyfrom failed: %d", r);
	buffer += bytes;

	if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
		dp_next_iovec(iovp);
		ix = 0;
	}
	/* Till packet done */
  } while ((pktsize -= bytes) > 0);
  return;
}

#endif				/* HAVE_BUFFERS */

/** netbuff.c **/

/*
**  File:	netbuff.c	Jun. 10, 2000
**
**  Author:	Giovanni Falzoni <gfalzoni@inwind.it>
**
**  This file contains specific implementation of buffering
**  for network packets.
*/

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include "dp.h"

#if (HAVE_BUFFERS == 1)

static m_hdr_t *allocptr = NULL;
static char tx_rx_buff[8192];

/*
**  Name:	alloc_buff
**  Function:	Allocates a buffer from the common pool.
*/
void *alloc_buff(dpeth_t *dep, int size)
{
  m_hdr_t *ptr, *wrk = allocptr;
  int units = ((size + sizeof(m_hdr_t) - 1) / sizeof(m_hdr_t)) + 1;

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
		return ptr + 1;
	}
	if (ptr == allocptr) break;
  }
  return NULL;			/* No memory available */
}

/*
**  Name:	free_buff
**  Function:	Returns a buffer to the common pool.
*/
void free_buff(dpeth_t *dep, void *blk)
{
  m_hdr_t *wrk, *ptr = (m_hdr_t *) blk - 1;

  /* Scan linked list for the correct place */
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
}

/*
**  Name:	init_buff
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
		*tx_buff = alloc_buff(dep,
		    NDEV_ETH_PACKET_MAX + sizeof(buff_t));
		(*tx_buff)->size = 0;
	}
  }
}

#endif				/* HAVE_BUFFERS */

/** netbuff.c **/

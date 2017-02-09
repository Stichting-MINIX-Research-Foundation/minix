/* $NetBSD: spivar.h,v 1.6 2014/07/13 17:12:23 dholland Exp $ */

/*-
 * Copyright (c) 2006 Urbana-Champaign Independent Media Center.
 * Copyright (c) 2006 Garrett D'Amore.
 * All rights reserved.
 *
 * Portions of this code were written by Garrett D'Amore for the
 * Champaign-Urbana Community Wireless Network Project.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgements:
 *      This product includes software developed by the Urbana-Champaign
 *      Independent Media Center.
 *	This product includes software developed by Garrett D'Amore.
 * 4. Urbana-Champaign Independent Media Center's name and Garrett
 *    D'Amore's name may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER AND GARRETT D'AMORE ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE URBANA-CHAMPAIGN INDEPENDENT
 * MEDIA CENTER OR GARRETT D'AMORE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_SPI_SPIVAR_H_
#define	_DEV_SPI_SPIVAR_H_

#include <sys/queue.h>

/*
 * Serial Peripheral Interface bus.  This is a 4-wire bus common for
 * connecting flash, clocks, sensors, and various other low-speed
 * peripherals.
 */

struct spi_handle;
struct spi_transfer;

/*
 * De facto standard latching modes.
 */
#define	SPI_MODE_0	0	/* CPOL = 0, CPHA = 0 */
#define	SPI_MODE_1	1	/* CPOL = 0, CPHA = 1 */
#define	SPI_MODE_2	2	/* CPOL = 1, CPHA = 0 */
#define	SPI_MODE_3	3	/* CPOL = 1, CPHA = 1 */
/* Philips' Microwire is just Mode 0 */
#define	SPI_MODE_MICROWIRE	SPI_MODE_0

struct spi_controller {
	void	*sct_cookie;	/* controller private data */
	int	sct_nslaves;
	int	(*sct_configure)(void *, int, int, int);
	int	(*sct_transfer)(void *, struct spi_transfer *);
};

int spibus_print(void *, const char *);

/* one per chip select */
struct spibus_attach_args {
	struct spi_controller	*sba_controller;
};

struct spi_attach_args {
	struct spi_handle	*sa_handle;
};

/*
 * This is similar in some respects to struct buf, but we cannot use
 * that structure because it was not designed to support full-duplex
 * IO.
 */
struct spi_chunk {
	struct spi_chunk *chunk_next;
	int		chunk_count;
	uint8_t		*chunk_read;
	const uint8_t	*chunk_write;
	/* for private use by framework and bus driver */
	uint8_t		*chunk_rptr;
	const uint8_t	*chunk_wptr;
	int		chunk_rresid;
	int		chunk_wresid;
};

struct spi_transfer {
	struct spi_chunk *st_chunks;		/* chained bufs */
	SIMPLEQ_ENTRY(spi_transfer) st_chain;	/* chain of submitted jobs */
	int		st_flags;
	int		st_errno;
	int		st_slave;
	void		*st_private;
	void		(*st_done)(struct spi_transfer *);
	kmutex_t	st_lock;
	kcondvar_t	st_cv;
	void		*st_busprivate;
};

/* declare a list of transfers */
SIMPLEQ_HEAD(spi_transq, spi_transfer);

#define	spi_transq_init(q)	\
	SIMPLEQ_INIT(q)

#define	spi_transq_enqueue(q, trans)	\
	SIMPLEQ_INSERT_TAIL(q, trans, st_chain)

#define	spi_transq_dequeue(q)		\
	SIMPLEQ_REMOVE_HEAD(q, st_chain)

#define	spi_transq_first(q)		\
	SIMPLEQ_FIRST(q)

#define	SPI_F_DONE		0x0001
#define	SPI_F_ERROR		0x0002

int spi_configure(struct spi_handle *, int mode, int speed);
int spi_transfer(struct spi_handle *, struct spi_transfer *);
void spi_transfer_init(struct spi_transfer *);
void spi_chunk_init(struct spi_chunk *, int, const uint8_t *, uint8_t *);
void spi_transfer_add(struct spi_transfer *, struct spi_chunk *);
void spi_wait(struct spi_transfer *);
void spi_done(struct spi_transfer *, int);

/* convenience wrappers */
int spi_send(struct spi_handle *, int, const uint8_t *);
int spi_recv(struct spi_handle *, int, uint8_t *);
int spi_send_recv(struct spi_handle *, int, const uint8_t *, int, uint8_t *);

#endif	/* _DEV_SPI_SPIVAR_H_ */

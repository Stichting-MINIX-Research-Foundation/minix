/* $NetBSD: spiflash.c,v 1.18 2015/07/22 10:07:59 ryo Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: spiflash.c,v 1.18 2015/07/22 10:07:59 ryo Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/uio.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <dev/spi/spivar.h>
#include <dev/spi/spiflash.h>

/*
 * This is an MI block driver for SPI flash devices.  It could probably be
 * converted to some more generic framework, if someone wanted to create one
 * for NOR flashes.  Note that some flashes have the ability to handle
 * interrupts.
 */

struct spiflash_softc {
	struct disk		sc_dk;

	struct spiflash_hw_if	sc_hw;
	void			*sc_cookie;

	const char		*sc_name;
	struct spi_handle	*sc_handle;
	int			sc_device_size;
	int			sc_write_size;
	int			sc_erase_size;
	int			sc_read_size;
	int			sc_device_blks;

	struct bufq_state	*sc_waitq;
	struct bufq_state	*sc_workq;
	struct bufq_state	*sc_doneq;
	lwp_t			*sc_thread;
};

#define	sc_getname	sc_hw.sf_getname
#define	sc_gethandle	sc_hw.sf_gethandle
#define	sc_getsize	sc_hw.sf_getsize
#define	sc_getflags	sc_hw.sf_getflags
#define	sc_erase	sc_hw.sf_erase
#define	sc_write	sc_hw.sf_write
#define	sc_read		sc_hw.sf_read
#define	sc_getstatus	sc_hw.sf_getstatus
#define	sc_setstatus	sc_hw.sf_setstatus

struct spiflash_attach_args {
	const struct spiflash_hw_if	*hw;
	void				*cookie;
};

#define	STATIC
STATIC int spiflash_match(device_t , cfdata_t , void *);
STATIC void spiflash_attach(device_t , device_t , void *);
STATIC int spiflash_print(void *, const char *);
STATIC int spiflash_common_erase(spiflash_handle_t, size_t, size_t);
STATIC int spiflash_common_write(spiflash_handle_t, size_t, size_t,
    const uint8_t *);
STATIC int spiflash_common_read(spiflash_handle_t, size_t, size_t, uint8_t *);
STATIC void spiflash_process_done(spiflash_handle_t, int);
STATIC void spiflash_process_read(spiflash_handle_t);
STATIC void spiflash_process_write(spiflash_handle_t);
STATIC void spiflash_thread(void *);
STATIC int spiflash_nsectors(spiflash_handle_t, struct buf *);
STATIC int spiflash_nsectors(spiflash_handle_t, struct buf *);
STATIC int spiflash_sector(spiflash_handle_t, struct buf *);

CFATTACH_DECL_NEW(spiflash, sizeof(struct spiflash_softc),
	      spiflash_match, spiflash_attach, NULL, NULL);

#ifdef	SPIFLASH_DEBUG
#define	DPRINTF(x)	do { printf x; } while (0/*CONSTCOND*/)
#else
#define	DPRINTF(x)	do {  } while (0/*CONSTCOND*/)
#endif

extern struct cfdriver spiflash_cd;

dev_type_open(spiflash_open);
dev_type_close(spiflash_close);
dev_type_read(spiflash_read);
dev_type_write(spiflash_write);
dev_type_ioctl(spiflash_ioctl);
dev_type_strategy(spiflash_strategy);

const struct bdevsw spiflash_bdevsw = {
	.d_open = spiflash_open,
	.d_close = spiflash_close,
	.d_strategy = spiflash_strategy,
	.d_ioctl = spiflash_ioctl,
	.d_dump = nodump,
	.d_psize = nosize,
	.d_discard = nodiscard,
	.d_flag = D_DISK,
};

const struct cdevsw spiflash_cdevsw = {
	.d_open = spiflash_open,
	.d_close = spiflash_close,
	.d_read = spiflash_read,
	.d_write = spiflash_write,
	.d_ioctl = spiflash_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK,
};

static struct dkdriver spiflash_dkdriver = {
	.d_strategy = spiflash_strategy
};

spiflash_handle_t
spiflash_attach_mi(const struct spiflash_hw_if *hw, void *cookie,
    device_t dev)
{
	struct spiflash_attach_args sfa;
	sfa.hw = hw;
	sfa.cookie = cookie;

	return (spiflash_handle_t)config_found(dev, &sfa, spiflash_print);
}

int
spiflash_print(void *aux, const char *pnp)
{
	if (pnp != NULL)
		printf("spiflash at %s\n", pnp);

	return UNCONF;
}

int
spiflash_match(device_t parent, cfdata_t cf, void *aux)
{

	return 1;
}

void
spiflash_attach(device_t parent, device_t self, void *aux)
{
	struct spiflash_softc *sc = device_private(self);
	struct spiflash_attach_args *sfa = aux;
	void *cookie = sfa->cookie;

	sc->sc_hw = *sfa->hw;
	sc->sc_cookie = cookie;
	sc->sc_name = sc->sc_getname(cookie);
	sc->sc_handle = sc->sc_gethandle(cookie);
	sc->sc_device_size = sc->sc_getsize(cookie, SPIFLASH_SIZE_DEVICE);
	sc->sc_erase_size = sc->sc_getsize(cookie, SPIFLASH_SIZE_ERASE);
	sc->sc_write_size = sc->sc_getsize(cookie, SPIFLASH_SIZE_WRITE);
	sc->sc_read_size = sc->sc_getsize(cookie, SPIFLASH_SIZE_READ);
	sc->sc_device_blks = sc->sc_device_size / DEV_BSIZE;

	if (sc->sc_read == NULL)
		sc->sc_read = spiflash_common_read;
	if (sc->sc_write == NULL)
		sc->sc_write = spiflash_common_write;
	if (sc->sc_erase == NULL)
		sc->sc_erase = spiflash_common_erase;

	aprint_naive(": SPI flash\n");
	aprint_normal(": %s SPI flash\n", sc->sc_name);
	/* XXX: note that this has to change for boot-sectored flash */
	aprint_normal_dev(self, "%d KB, %d sectors of %d KB each\n",
	    sc->sc_device_size / 1024,
	    sc->sc_device_size / sc->sc_erase_size,
	    sc->sc_erase_size / 1024);

	/* first-come first-served strategy works best for us */
	bufq_alloc(&sc->sc_waitq, "fcfs", BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&sc->sc_workq, "fcfs", BUFQ_SORT_RAWBLOCK);
	bufq_alloc(&sc->sc_doneq, "fcfs", BUFQ_SORT_RAWBLOCK);

	disk_init(&sc->sc_dk, device_xname(self), &spiflash_dkdriver);
	disk_attach(&sc->sc_dk);

	/* arrange to allocate the kthread */
	kthread_create(PRI_NONE, 0, NULL, spiflash_thread, sc,
	    &sc->sc_thread, "spiflash");
}

int
spiflash_open(dev_t dev, int flags, int mode, struct lwp *l)
{
	spiflash_handle_t sc;

	sc = device_lookup_private(&spiflash_cd, DISKUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	/*
	 * XXX: We need to handle partitions here.  The problem is
	 * that it isn't entirely clear to me how to deal with this.
	 * There are devices that could be used "in the raw" with a
	 * NetBSD label, but then you get into devices that have other
	 * kinds of data on them -- some have VxWorks data, some have
	 * RedBoot data, and some have other contraints -- for example
	 * some devices might have a portion that is read-only,
	 * whereas others might have a portion that is read-write.
	 *
	 * For now we just permit access to the entire device.
	 */
	return 0;
}

int
spiflash_close(dev_t dev, int flags, int mode, struct lwp *l)
{
	spiflash_handle_t sc;

	sc = device_lookup_private(&spiflash_cd, DISKUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	return 0;
}

int
spiflash_read(dev_t dev, struct uio *uio, int ioflag)
{

	return physio(spiflash_strategy, NULL, dev, B_READ, minphys, uio);
}

int
spiflash_write(dev_t dev, struct uio *uio, int ioflag)
{

	return physio(spiflash_strategy, NULL, dev, B_WRITE, minphys, uio);
}

int
spiflash_ioctl(dev_t dev, u_long cmd, void *data, int flags, struct lwp *l)
{
	spiflash_handle_t sc;

	sc = device_lookup_private(&spiflash_cd, DISKUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	return EINVAL;
}

void
spiflash_strategy(struct buf *bp)
{
	spiflash_handle_t sc;
	int	s;

	sc = device_lookup_private(&spiflash_cd, DISKUNIT(bp->b_dev));
	if (sc == NULL) {
		bp->b_error = ENXIO;
		biodone(bp);
		return;
	}

	if (((bp->b_bcount % sc->sc_write_size) != 0) ||
	    (bp->b_blkno < 0)) {
		bp->b_error = EINVAL;
		biodone(bp);
		return;
	}

	/* no work? */
	if (bp->b_bcount == 0) {
		biodone(bp);
		return;
	}

	if (bounds_check_with_mediasize(bp, DEV_BSIZE,
		sc->sc_device_blks) <= 0) {
		biodone(bp);
		return;
	}

	bp->b_resid = bp->b_bcount;

	/* all ready, hand off to thread for async processing */
	s = splbio();
	bufq_put(sc->sc_waitq, bp);
	wakeup(&sc->sc_thread);
	splx(s);
}

void
spiflash_process_done(spiflash_handle_t sc, int err)
{
	struct buf	*bp;
	int		cnt = 0;
	int		flag = 0;

	while ((bp = bufq_get(sc->sc_doneq)) != NULL) {
		flag = bp->b_flags & B_READ;
		if ((bp->b_error = err) == 0)
			bp->b_resid = 0;
		cnt += bp->b_bcount - bp->b_resid;
		biodone(bp);
	}
	disk_unbusy(&sc->sc_dk, cnt, flag);
}

void
spiflash_process_read(spiflash_handle_t sc)
{
	struct buf	*bp;
	int		err = 0;

	disk_busy(&sc->sc_dk);
	while ((bp = bufq_get(sc->sc_workq)) != NULL) {
		size_t addr = bp->b_blkno * DEV_BSIZE;
		uint8_t *data = bp->b_data;
		int cnt = bp->b_resid;

		bufq_put(sc->sc_doneq, bp);

		DPRINTF(("read from addr %x, cnt %d\n", (unsigned)addr, cnt));

		if ((err = sc->sc_read(sc, addr, cnt, data)) != 0) {
			/* error occurred, fail all pending workq bufs */
			bufq_move(sc->sc_doneq, sc->sc_workq);
			break;
		}
		
		bp->b_resid -= cnt;
		data += cnt;
		addr += cnt;
	}
	spiflash_process_done(sc, err);
}

void
spiflash_process_write(spiflash_handle_t sc)
{
	int	len;
	size_t	base;
	daddr_t	blkno;
	uint8_t	*save;
	int	err = 0, neederase = 0;
	struct buf *bp;

	/*
	 * due to other considerations, we are guaranteed that
	 * we will only have multiple buffers if they are all in
	 * the same erase sector.  Therefore we never need to look
	 * beyond the first block to determine how much data we need
	 * to save.
	 */

	bp = bufq_peek(sc->sc_workq);
	len = spiflash_nsectors(sc, bp)  * sc->sc_erase_size;
	blkno = bp->b_blkno;
	base = (blkno * DEV_BSIZE) & ~ (sc->sc_erase_size - 1);

	/* get ourself a scratch buffer */
	save = malloc(len, M_DEVBUF, M_WAITOK);

	disk_busy(&sc->sc_dk);
	/* read in as much of the data as we need */
	DPRINTF(("reading in %d bytes\n", len));
	if ((err = sc->sc_read(sc, base, len, save)) != 0) {
		bufq_move(sc->sc_doneq, sc->sc_workq);	
		spiflash_process_done(sc, err);
		return;
	}

	/*
	 * now coalesce the writes into the save area, but also
	 * check to see if we need to do an erase
	 */
	while ((bp = bufq_get(sc->sc_workq)) != NULL) {
		uint8_t	*data, *dst;
		int resid = bp->b_resid;

		DPRINTF(("coalesce write, blkno %x, count %d, resid %d\n",
			    (unsigned)bp->b_blkno, bp->b_bcount, resid));

		data = bp->b_data;
		dst = save + (bp->b_blkno * DEV_BSIZE) - base;

		/*
		 * NOR flash bits.  We can clear a bit, but we cannot
		 * set a bit, without erasing.  This should help reduce
		 * unnecessary erases.
		 */
		while (resid) {
			if ((*data) & ~(*dst))
				neederase = 1;
			*dst++ = *data++;
			resid--;
		}

		bufq_put(sc->sc_doneq, bp);
	}
	
	/*
	 * do the erase, if we need to.
	 */
	if (neederase) {
		DPRINTF(("erasing from %zx - %zx\n", base, base + len));
		if ((err = sc->sc_erase(sc, base, len)) != 0) {
			spiflash_process_done(sc, err);
			return;
		}
	}

	/*
	 * now write our save area, and finish up.
	 */
	DPRINTF(("flashing %d bytes to %zx from %p\n", len, base, save));
	err = sc->sc_write(sc, base, len, save);
	spiflash_process_done(sc, err);
}


int
spiflash_nsectors(spiflash_handle_t sc, struct buf *bp)
{
	unsigned	addr, sector;

	addr = bp->b_blkno * DEV_BSIZE;
	sector = addr / sc->sc_erase_size;

	addr += bp->b_bcount;
	addr--;
	return (((addr / sc->sc_erase_size)  - sector) + 1);
}

int
spiflash_sector(spiflash_handle_t sc, struct buf *bp)
{
	unsigned	addr, sector;

	addr = bp->b_blkno * DEV_BSIZE;
	sector = addr / sc->sc_erase_size;

	/* if it spans multiple blocks, error it */
	addr += bp->b_bcount;
	addr--;
	if (sector != (addr / sc->sc_erase_size))
		return -1;

	return sector;
}

void
spiflash_thread(void *arg)
{
	spiflash_handle_t sc = arg;
	struct buf	*bp;
	int		sector;

	(void)splbio();
	for (;;) {
		if ((bp = bufq_get(sc->sc_waitq)) == NULL) {
			tsleep(&sc->sc_thread, PRIBIO, "spiflash_thread", 0);
			continue;
		}

		bufq_put(sc->sc_workq, bp);

		if (bp->b_flags & B_READ) {
			/* just do the read */
			spiflash_process_read(sc);
			continue;
		}

		/*
		 * Because writing a flash filesystem is particularly
		 * painful, involving erase, modify, write, we prefer
		 * to coalesce writes to the same sector together.
		 */

		sector = spiflash_sector(sc, bp);

		/*
		 * if the write spans multiple sectors, skip
		 * coalescing.  (It would be nice if we could break
		 * these up.  minphys is honored for read/write, but
		 * not necessarily for bread.)
		 */
		if (sector < 0)
			goto dowrite;

		while ((bp = bufq_peek(sc->sc_waitq)) != NULL) {
			/* can't deal with read requests! */
			if (bp->b_flags & B_READ)
				break;

			/* is it for the same sector? */
			if (spiflash_sector(sc, bp) != sector)
				break;

			bp = bufq_get(sc->sc_waitq);
			bufq_put(sc->sc_workq, bp);
		}

	dowrite:
		spiflash_process_write(sc);
	}
}
/*
 * SPI flash common implementation.
 */

/*
 * Most devices take on the order of 1 second for each block that they
 * delete.
 */
int
spiflash_common_erase(spiflash_handle_t sc, size_t start, size_t size)
{
	int		rv;

	if ((start % sc->sc_erase_size) || (size % sc->sc_erase_size))
		return EINVAL;

	/* the second test is to test against wrap */ 
	if ((start > sc->sc_device_size) ||
	    ((start + size) > sc->sc_device_size))
		return EINVAL;

	/*
	 * XXX: check protection status?  Requires master table mapping
	 * sectors to status bits, and so forth.
	 */

	while (size) {
		if ((rv = spiflash_write_enable(sc)) != 0) {
			spiflash_write_disable(sc);
			return rv;
		}
		if ((rv = spiflash_cmd(sc, SPIFLASH_CMD_ERASE, 3, start, 0,
			 NULL, NULL)) != 0) {
			spiflash_write_disable(sc);
			return rv;
		}

		/*
		 * The devices I have all say typical for sector erase
		 * is ~1sec.  We check ten times that often.  (There
		 * is no way to interrupt on this.)
		 */
		if ((rv = spiflash_wait(sc, hz / 10)) != 0)
			return rv;

		start += sc->sc_erase_size;
		size -= sc->sc_erase_size;

		/* NB: according to the docs I have, the write enable
		 * is automatically cleared upon completion of an erase
		 * command, so there is no need to explicitly disable it.
		 */
	}

	return 0;
}

int
spiflash_common_write(spiflash_handle_t sc, size_t start, size_t size,
    const uint8_t *data)
{
	int		rv;

	if ((start % sc->sc_write_size) || (size % sc->sc_write_size))
		return EINVAL;

	while (size) {
		int cnt;

		if ((rv = spiflash_write_enable(sc)) != 0) {
			spiflash_write_disable(sc);
			return rv;
		}

		cnt = min(size, sc->sc_write_size);
		if ((rv = spiflash_cmd(sc, SPIFLASH_CMD_PROGRAM, 3, start,
			 cnt, data, NULL)) != 0) {
			spiflash_write_disable(sc);
			return rv;
		}

		/*
		 * It seems that most devices can write bits fairly
		 * quickly.  For example, one part I have access to
		 * takes ~5msec to process the entire 256 byte page.
		 * Probably this should be modified to cope with
		 * device-specific timing, and maybe also take into
		 * account systems with higher values of HZ (which
		 * could benefit from sleeping.)
		 */
		if ((rv = spiflash_wait(sc, 0)) != 0)
			return rv;

		data += cnt;
		start += cnt;
		size -= cnt;
	}

	return 0;
}

int
spiflash_common_read(spiflash_handle_t sc, size_t start, size_t size,
    uint8_t *data)
{
	int		rv;

	while (size) {
		int cnt;

		if (sc->sc_read_size > 0)
			cnt = min(size, sc->sc_read_size);
		else 
			cnt = size;

		if ((rv = spiflash_cmd(sc, SPIFLASH_CMD_READ, 3, start,
			 cnt, NULL, data)) != 0) {
			return rv;
		}

		data += cnt;
		start += cnt;
		size -= cnt;
	}

	return 0;
}

/* read status register */
int
spiflash_read_status(spiflash_handle_t sc, uint8_t *sr)
{

	return spiflash_cmd(sc, SPIFLASH_CMD_RDSR, 0, 0, 1, NULL, sr);
}

int
spiflash_write_enable(spiflash_handle_t sc)
{

	return spiflash_cmd(sc, SPIFLASH_CMD_WREN, 0, 0, 0, NULL, NULL);
}

int
spiflash_write_disable(spiflash_handle_t sc)
{

	return spiflash_cmd(sc, SPIFLASH_CMD_WRDI, 0, 0, 0, NULL, NULL);
}

int
spiflash_cmd(spiflash_handle_t sc, uint8_t cmd,
    size_t addrlen, uint32_t addr,
    size_t cnt, const uint8_t *wdata, uint8_t *rdata)
{
	struct spi_transfer	trans;
	struct spi_chunk	chunk1, chunk2;
	char buf[4];
	int i;

	buf[0] = cmd;

	if (addrlen > 3)
		return EINVAL;

	for (i = addrlen; i > 0; i--) {
		buf[i] = addr & 0xff;
		addr >>= 8;
	}
	spi_transfer_init(&trans);
	spi_chunk_init(&chunk1, addrlen + 1, buf, NULL);
	spi_transfer_add(&trans, &chunk1);
	if (cnt) {
		spi_chunk_init(&chunk2, cnt, wdata, rdata);
		spi_transfer_add(&trans, &chunk2);
	}

	spi_transfer(sc->sc_handle, &trans);
	spi_wait(&trans);

	if (trans.st_flags & SPI_F_ERROR)
		return trans.st_errno;
	return 0;
}

int
spiflash_wait(spiflash_handle_t sc, int tmo)
{
	int	rv;
	uint8_t	sr;

	for (;;) {
		if ((rv = spiflash_read_status(sc, &sr)) != 0)
			return rv;

		if ((sr & SPIFLASH_SR_BUSY) == 0)
			break;
		/*
		 * The devices I have all say typical for sector
		 * erase is ~1sec.  We check time times that often.
		 * (There is no way to interrupt on this.)
		 */
		if (tmo)
			tsleep(&sr, PWAIT, "spiflash_wait", tmo);
	}
	return 0;
}

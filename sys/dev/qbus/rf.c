/*	$NetBSD: rf.c,v 1.32 2015/04/26 15:15:20 mlelstv Exp $	*/
/*
 * Copyright (c) 2002 Jochen Kunz.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Jochen Kunz may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOCHEN KUNZ
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JOCHEN KUNZ
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
TODO:
- Better LBN bound checking, block padding for SD disks.
- Formatting / "Set Density"
- Better error handling / detailed error reason reporting.
*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rf.c,v 1.32 2015/04/26 15:15:20 mlelstv Exp $");

/* autoconfig stuff */
#include <sys/param.h>
#include <sys/device.h>
#include <sys/conf.h>
#include "locators.h"
#include "ioconf.h"

/* bus_space / bus_dma */
#include <sys/bus.h>

/* UniBus / QBus specific stuff */
#include <dev/qbus/ubavar.h>

/* disk interface */
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

/* general system data and functions */
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>

/* physio / buffer handling */
#include <sys/buf.h>
#include <sys/bufq.h>

/* tsleep / sleep / wakeup */
#include <sys/proc.h>
/* hz for above */
#include <sys/kernel.h>

/* bitdefinitions for RX211 */
#include <dev/qbus/rfreg.h>


#define	RFS_DENS	0x0001		/* single or double density */
#define	RFS_AD		0x0002		/* density auto detect */
#define	RFS_NOTINIT	0x0000		/* not initialized */
#define	RFS_PROBING	0x0010		/* density detect / verify started */
#define	RFS_FBUF	0x0020		/* Fill Buffer */
#define	RFS_EBUF	0x0030		/* Empty Buffer */
#define	RFS_WSEC	0x0040		/* Write Sector */
#define	RFS_RSEC	0x0050		/* Read Sector */
#define	RFS_SMD		0x0060		/* Set Media Density */
#define	RFS_RSTAT	0x0070		/* Read Status */
#define	RFS_WDDS	0x0080		/* Write Deleted Data Sector */
#define	RFS_REC		0x0090		/* Read Error Code */
#define	RFS_IDLE	0x00a0		/* controller is idle */
#define	RFS_CMDS	0x00f0		/* command mask */
#define	RFS_OPEN_A	0x0100		/* partition a open */
#define	RFS_OPEN_B	0x0200		/* partition b open */
#define	RFS_OPEN_C	0x0400		/* partition c open */
#define	RFS_OPEN_MASK	0x0f00		/* mask for open partitions */
#define RFS_OPEN_SHIFT	8		/* to shift 1 to get RFS_OPEN_A */
#define	RFS_SETCMD(rf, state)	((rf) = ((rf) & ~RFS_CMDS) | (state))



/* autoconfig stuff */
static int rfc_match(device_t, cfdata_t, void *);
static void rfc_attach(device_t, device_t, void *);
static int rf_match(device_t, cfdata_t, void *);
static void rf_attach(device_t, device_t, void *);
static int rf_print(void *, const char *);

/* device interface functions / interface to disk(9) */
dev_type_open(rfopen);
dev_type_close(rfclose);
dev_type_read(rfread);
dev_type_write(rfwrite);
dev_type_ioctl(rfioctl);
dev_type_strategy(rfstrategy);
dev_type_dump(rfdump);
dev_type_size(rfsize);


/* Entries in block and character major device number switch table. */
const struct bdevsw rf_bdevsw = {
	.d_open = rfopen,
	.d_close = rfclose,
	.d_strategy = rfstrategy,
	.d_ioctl = rfioctl,
	.d_dump = rfdump,
	.d_psize = rfsize,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};

const struct cdevsw rf_cdevsw = {
	.d_open = rfopen,
	.d_close = rfclose,
	.d_read = rfread,
	.d_write = rfwrite,
	.d_ioctl = rfioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_DISK
};



struct rfc_softc {
	device_t sc_dev;		/* common device data */
	device_t sc_childs[2];		/* child devices */
	struct evcnt sc_intr_count;	/* Interrupt counter for statistics */
	struct buf *sc_curbuf;		/* buf that is currently in work */
	bus_space_tag_t sc_iot;		/* bus_space I/O tag */
	bus_space_handle_t sc_ioh;	/* bus_space I/O handle */
	bus_dma_tag_t sc_dmat;		/* bus_dma DMA tag */
	bus_dmamap_t sc_dmam;		/* bus_dma DMA map */
	void *sc_bufidx;		/* current position in buffer data */
	int sc_curchild;		/* child whos bufq is in work */
	int sc_bytesleft;		/* bytes left to transfer */
	u_int8_t type;			/* controller type, 1 or 2 */
};



CFATTACH_DECL_NEW(
	rfc,
	sizeof(struct rfc_softc),
	rfc_match,
	rfc_attach,
	NULL,
	NULL
);



struct rf_softc {
	device_t sc_dev;		/* common device data */
	struct disk sc_disk;		/* common disk device data */
	struct rfc_softc *sc_rfc;	/* our parent */
	struct bufq_state *sc_bufq;	/* queue of pending transfers */
	int sc_state;			/* state of drive */
	u_int8_t sc_dnum;		/* drive number, 0 or 1 */
};



CFATTACH_DECL_NEW(
	rf,
	sizeof(struct rf_softc),
	rf_match,
	rf_attach,
	NULL,
	NULL
);



struct rfc_attach_args {
	u_int8_t type;		/* controller type, 1 or 2 */
	u_int8_t dnum;		/* drive number, 0 or 1 */
};



const struct dkdriver rfdkdriver = {
	.d_strategy = rfstrategy
};



/* helper functions */
int rfc_sendcmd(struct rfc_softc *, int, int, int);
struct rf_softc* get_new_buf( struct rfc_softc *);
static void rfc_intr(void *);



/*
 * Issue a reset command to the controller and look for the bits in
 * RX2CS and RX2ES.
 * RX2CS_RX02 and / or RX2CS_DD can be set,
 * RX2ES has to be set, all other bits must be 0
 */
int
rfc_match(device_t parent, cfdata_t match, void *aux)
{
	struct uba_attach_args *ua = aux;
	int i;

	/* Issue reset command. */
	bus_space_write_2(ua->ua_iot, ua->ua_ioh, RX2CS, RX2CS_INIT);
	/* Wait for the controller to become ready, that is when
	 * RX2CS_DONE, RX2ES_RDY and RX2ES_ID are set. */
	for (i = 0 ; i < 20 ; i++) {
		if ((bus_space_read_2(ua->ua_iot, ua->ua_ioh, RX2CS)
		    & RX2CS_DONE) != 0
		    && (bus_space_read_2(ua->ua_iot, ua->ua_ioh, RX2ES)
		    & (RX2ES_RDY | RX2ES_ID)) != 0)
			break;
		DELAY(100000);	/* wait 100ms */
	}
	/*
	 * Give up if the timeout has elapsed
	 * and the controller is not ready.
	 */
	if (i >= 20)
		return(0);
	/*
	 * Issue a Read Status command with interrupt enabled.
	 * The uba(4) driver wants to catch the interrupt to get the
	 * interrupt vector and level of the device
	 */
	bus_space_write_2(ua->ua_iot, ua->ua_ioh, RX2CS,
	    RX2CS_RSTAT | RX2CS_IE);
	/*
	 * Wait for command to finish, ignore errors and
	 * abort if the controller does not respond within the timeout
	 */
	for (i = 0 ; i < 20 ; i++) {
		if ((bus_space_read_2(ua->ua_iot, ua->ua_ioh, RX2CS)
		    & (RX2CS_DONE | RX2CS_IE)) != 0
		    && (bus_space_read_2(ua->ua_iot, ua->ua_ioh, RX2ES)
		    & RX2ES_RDY) != 0 )
			return(1);
		DELAY(100000);	/* wait 100ms */
	}
	return(0);
}



/* #define RX02_PROBE 1 */
#ifdef RX02_PROBE
/*
 * Probe the density of an inserted floppy disk.
 * This is done by reading a sector from disk.
 * Return -1 on error, 0 on SD and 1 on DD.
 */
int rfcprobedens(struct rfc_softc *, int);
int
rfcprobedens(struct rfc_softc *rfc_sc, int dnum)
{
	int dens_flag;
	int i;

	dens_flag = 0;
	do {
		bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS,
		    RX2CS_RSEC | (dens_flag == 0 ? 0 : RX2CS_DD)
		    | (dnum == 0 ? 0 : RX2CS_US));
		/*
		 * Transfer request set?
		 * Wait 50us, the controller needs this time to setle
		 */
		DELAY(50);
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_TR) == 0) {
			printf("%s: did not respond to Read Sector CMD(1)\n",
			    device_xname(rfc_sc->sc_dev));
			return(-1);
		}
		bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2SA, 1);
		/* Wait 50us, the controller needs this time to setle */
		DELAY(50);
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_TR) == 0) {
			printf("%s: did not respond to Read Sector CMD(2)\n",
			    device_xname(rfc_sc->sc_dev));
			return(-1);
		}
		bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2TA, 1);
		/* Wait for the command to finish */
		for (i = 0 ; i < 200 ; i++) {
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_DONE) != 0)
				break;
			DELAY(10000);	/* wait 10ms */
		}
		if (i >= 200) {
			printf("%s: did not respond to Read Sector CMD(3)\n",
			    device_xname(rfc_sc->sc_dev));
			return(-1);
		}
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_ERR) == 0)
			return(dens_flag);
	} while (rfc_sc->type == 2 && dens_flag++ == 0);
	return(-1);
}
#endif /* RX02_PROBE */



void
rfc_attach(device_t parent, device_t self, void *aux)
{
	struct rfc_softc *rfc_sc = device_private(self);
	struct uba_attach_args *ua = aux;
	struct rfc_attach_args rfc_aa;
	int i;

	rfc_sc->sc_dev = self;
	rfc_sc->sc_iot = ua->ua_iot;
	rfc_sc->sc_ioh = ua->ua_ioh;
	rfc_sc->sc_dmat = ua->ua_dmat;
	rfc_sc->sc_curbuf = NULL;
	/* Tell the QBus busdriver about our interrupt handler. */
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec, rfc_intr, rfc_sc,
	    &rfc_sc->sc_intr_count);
	/* Attach to the interrupt counter, see evcnt(9) */
	evcnt_attach_dynamic(&rfc_sc->sc_intr_count, EVCNT_TYPE_INTR,
	    ua->ua_evcnt, device_xname(rfc_sc->sc_dev), "intr");
	/* get a bus_dma(9) handle */
	i = bus_dmamap_create(rfc_sc->sc_dmat, RX2_BYTE_DD, 1, RX2_BYTE_DD, 0,
	    BUS_DMA_ALLOCNOW, &rfc_sc->sc_dmam);
	if (i != 0) {
		printf("rfc_attach: Error creating bus dma map: %d\n", i);
		return;
	}

	/* Issue reset command. */
	bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS, RX2CS_INIT);
	/*
	 * Wait for the controller to become ready, that is when
	 * RX2CS_DONE, RX2ES_RDY and RX2ES_ID are set.
	 */
	for (i = 0 ; i < 20 ; i++) {
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_DONE) != 0
		    && (bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2ES)
		    & (RX2ES_RDY | RX2ES_ID)) != 0)
			break;
		DELAY(100000);	/* wait 100ms */
	}
	/*
	 * Give up if the timeout has elapsed
	 * and the controller is not ready.
	 */
	if (i >= 20) {
		printf(": did not respond to INIT CMD\n");
		return;
	}
	/* Is ths a RX01 or a RX02? */
	if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
	    & RX2CS_RX02) != 0) {
		rfc_sc->type = 2;
		rfc_aa.type = 2;
	} else {
		rfc_sc->type = 1;
		rfc_aa.type = 1;
	}
	printf(": RX0%d\n", rfc_sc->type);

#ifndef RX02_PROBE
	/*
	 * Bouth disk drievs and the controller are one physical unit.
	 * If we found the controller, there will be bouth disk drievs.
	 * So attach them.
	 */
	rfc_aa.dnum = 0;
	rfc_sc->sc_childs[0] = config_found(rfc_sc->sc_dev, &rfc_aa, rf_print);
	rfc_aa.dnum = 1;
	rfc_sc->sc_childs[1] = config_found(rfc_sc->sc_dev, &rfc_aa, rf_print);
#else /* RX02_PROBE */
	/*
	 * There are clones of the DEC RX system with standard shugart
	 * interface. In this case we can not be sure that there are
	 * bouth disk drievs. So we want to do a detection of attached
	 * drives. This is done by reading a sector from disk. This means
	 * that there must be a formatted disk in the drive at boot time.
	 * This is bad, but I did not find another way to detect the
	 * (non)existence of a floppy drive.
	 */
	if (rfcprobedens(rfc_sc, 0) >= 0) {
		rfc_aa.dnum = 0;
		rfc_sc->sc_childs[0] = config_found(rfc_sc->sc_dev, &rfc_aa,
		    rf_print);
	} else
		rfc_sc->sc_childs[0] = NULL;
	if (rfcprobedens(rfc_sc, 1) >= 0) {
		rfc_aa.dnum = 1;
		rfc_sc->sc_childs[1] = config_found(rfc_sc->sc_dev, &rfc_aa,
		    rf_print);
	} else
		rfc_sc->sc_childs[1] = NULL;
#endif /* RX02_PROBE */
	return;
}



int
rf_match(device_t parent, cfdata_t match, void *aux)
{
	struct rfc_attach_args *rfc_aa = aux;

	/*
	 * Only attach if the locator is wildcarded or
	 * if the specified locator addresses the current device.
	 */
	if (match->cf_loc[RFCCF_DRIVE] == RFCCF_DRIVE_DEFAULT ||
	    match->cf_loc[RFCCF_DRIVE] == rfc_aa->dnum)
		return(1);
	return(0);
}



void
rf_attach(device_t parent, device_t self, void *aux)
{
	struct rf_softc *rf_sc = device_private(self);
	struct rfc_softc *rfc_sc = device_private(parent);
	struct rfc_attach_args *rfc_aa = (struct rfc_attach_args *)aux;
	struct disklabel *dl;

	rf_sc->sc_dev = self;
	rf_sc->sc_rfc = rfc_sc;
	rf_sc->sc_dnum = rfc_aa->dnum;
	rf_sc->sc_state = 0;
	disk_init(&rf_sc->sc_disk, device_xname(rf_sc->sc_dev), &rfdkdriver);
	disk_attach(&rf_sc->sc_disk);
	dl = rf_sc->sc_disk.dk_label;
	dl->d_type = DKTYPE_FLOPPY;		/* drive type */
	dl->d_magic = DISKMAGIC;		/* the magic number */
	dl->d_magic2 = DISKMAGIC;
	dl->d_typename[0] = 'R';
	dl->d_typename[1] = 'X';
	dl->d_typename[2] = '0';
	dl->d_typename[3] = rfc_sc->type == 1 ? '1' : '2';	/* type name */
	dl->d_typename[4] = '\0';
	dl->d_secsize = DEV_BSIZE;		/* bytes per sector */
	/*
	 * Fill in some values to have a initialized data structure. Some
	 * values will be reset by rfopen() depending on the actual density.
	 */
	dl->d_nsectors = RX2_SECTORS;		/* sectors per track */
	dl->d_ntracks = 1;								/* tracks per cylinder */
	dl->d_ncylinders = RX2_TRACKS;		/* cylinders per unit */
	dl->d_secpercyl = RX2_SECTORS;		/* sectors per cylinder */
	dl->d_secperunit = RX2_SECTORS * RX2_TRACKS;	/* sectors per unit */
	dl->d_rpm = 360;			/* rotational speed */
	dl->d_interleave = 1;			/* hardware sector interleave */
	/* number of partitions in following */
	dl->d_npartitions = MAXPARTITIONS;
	dl->d_bbsize = 0;		/* size of boot area at sn0, bytes */
	dl->d_sbsize = 0;		/* max size of fs superblock, bytes */
	/* number of sectors in partition */
	dl->d_partitions[0].p_size = 501;
	dl->d_partitions[0].p_offset = 0;	/* starting sector */
	dl->d_partitions[0].p_fsize = 0;	/* fs basic fragment size */
	dl->d_partitions[0].p_fstype = 0;	/* fs type */
	dl->d_partitions[0].p_frag = 0;		/* fs fragments per block */
	dl->d_partitions[1].p_size = RX2_SECTORS * RX2_TRACKS / 2;
	dl->d_partitions[1].p_offset = 0;	/* starting sector */
	dl->d_partitions[1].p_fsize = 0;	/* fs basic fragment size */
	dl->d_partitions[1].p_fstype = 0;	/* fs type */
	dl->d_partitions[1].p_frag = 0;		/* fs fragments per block */
	dl->d_partitions[2].p_size = RX2_SECTORS * RX2_TRACKS;
	dl->d_partitions[2].p_offset = 0;	/* starting sector */
	dl->d_partitions[2].p_fsize = 0;	/* fs basic fragment size */
	dl->d_partitions[2].p_fstype = 0;	/* fs type */
	dl->d_partitions[2].p_frag = 0;		/* fs fragments per block */
	bufq_alloc(&rf_sc->sc_bufq, "disksort", BUFQ_SORT_CYLINDER);
	printf("\n");
	return;
}



int
rf_print(void *aux, const char *name)
{
	struct rfc_attach_args *rfc_aa = aux;

	if (name != NULL)
		aprint_normal("RX0%d at %s", rfc_aa->type, name);
	aprint_normal(" drive %d", rfc_aa->dnum);
	return(UNCONF);
}



/* Send a command to the controller */
int
rfc_sendcmd(struct rfc_softc *rfc_sc, int cmd, int data1, int data2)
{

	/* Write command to CSR. */
	bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS, cmd);
	/* Wait 50us, the controller needs this time to setle. */
	DELAY(50);
	/* Write parameter 1 to DBR */
	if ((cmd & RX2CS_FC) != RX2CS_RSTAT) {
		/* Transfer request set? */
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_TR) == 0) {
			printf("%s: did not respond to CMD %x (1)\n",
			    device_xname(rfc_sc->sc_dev), cmd);
			return(-1);
		}
		bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2DB,
		    data1);
	}
	/* Write parameter 2 to DBR */
	if ((cmd & RX2CS_FC) <= RX2CS_RSEC || (cmd & RX2CS_FC) == RX2CS_WDDS) {
		/* Wait 50us, the controller needs this time to setle. */
		DELAY(50);
		/* Transfer request set? */
		if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2CS)
		    & RX2CS_TR) == 0) {
			printf("%s: did not respond to CMD %x (2)\n",
			    device_xname(rfc_sc->sc_dev), cmd);
			return(-1);
		}
		bus_space_write_2(rfc_sc->sc_iot, rfc_sc->sc_ioh, RX2DB,
		    data2);
	}
	return(1);
}



void
rfstrategy(struct buf *buf)
{
	struct rf_softc *rf_sc;
	struct rfc_softc *rfc_sc;
	int s;

	if ((rf_sc = device_lookup_private(&rf_cd, DISKUNIT(buf->b_dev))) == NULL) {
		buf->b_error = ENXIO;
		biodone(buf);
		return;
	}
	rfc_sc = rf_sc->sc_rfc;
	/* We are going to operate on a non-open dev? PANIC! */
	if ((rf_sc->sc_state & (1 << (DISKPART(buf->b_dev) + RFS_OPEN_SHIFT)))
	    == 0)
		panic("rfstrategy: can not operate on non-open drive %s "
		    "partition %"PRIu32, device_xname(rf_sc->sc_dev),
		    DISKPART(buf->b_dev));
	if (buf->b_bcount == 0) {
		biodone(buf);
		return;
	}
	/*
	 * bufq_put() operates on b_rawblkno. rfstrategy() gets
	 * only b_blkno that is partition relative. As a floppy does not
	 * have partitions b_rawblkno == b_blkno.
	 */
	buf->b_rawblkno = buf->b_blkno;
	/*
	 * from sys/kern/subr_disk.c:
	 * Seek sort for disks.  We depend on the driver which calls us using
	 * b_resid as the current cylinder number.
	 */
	s = splbio();
	if (rfc_sc->sc_curbuf == NULL) {
		rfc_sc->sc_curchild = rf_sc->sc_dnum;
		rfc_sc->sc_curbuf = buf;
		rfc_sc->sc_bufidx = buf->b_data;
		rfc_sc->sc_bytesleft = buf->b_bcount;
		rfc_intr(rfc_sc);
	} else {
		buf->b_resid = buf->b_blkno / RX2_SECTORS;
		bufq_put(rf_sc->sc_bufq, buf);
		buf->b_resid = 0;
	}
	splx(s);
}

/*
 * Look if there is another buffer in the bufferqueue of this drive
 * and start to process it if there is one.
 * If the bufferqueue is empty, look at the bufferqueue of the other drive
 * that is attached to this controller.
 * Start procesing the bufferqueue of the other drive if it isn't empty.
 * Return a pointer to the softc structure of the drive that is now
 * ready to process a buffer or NULL if there is no buffer in either queues.
 */
struct rf_softc*
get_new_buf( struct rfc_softc *rfc_sc)
{
	struct rf_softc *rf_sc;
	struct rf_softc *other_drive;

	rf_sc = device_private(rfc_sc->sc_childs[rfc_sc->sc_curchild]);
	rfc_sc->sc_curbuf = bufq_get(rf_sc->sc_bufq);
	if (rfc_sc->sc_curbuf != NULL) {
		rfc_sc->sc_bufidx = rfc_sc->sc_curbuf->b_data;
		rfc_sc->sc_bytesleft = rfc_sc->sc_curbuf->b_bcount;
	} else {
		RFS_SETCMD(rf_sc->sc_state, RFS_IDLE);
		other_drive = device_private(
		    rfc_sc->sc_childs[ rfc_sc->sc_curchild == 0 ? 1 : 0]);
		if (other_drive != NULL
		    && bufq_peek(other_drive->sc_bufq) != NULL) {
			rfc_sc->sc_curchild = rfc_sc->sc_curchild == 0 ? 1 : 0;
			rf_sc = other_drive;
			rfc_sc->sc_curbuf = bufq_get(rf_sc->sc_bufq);
			rfc_sc->sc_bufidx = rfc_sc->sc_curbuf->b_data;
			rfc_sc->sc_bytesleft = rfc_sc->sc_curbuf->b_bcount;
		} else
			return(NULL);
	}
	return(rf_sc);
}



void
rfc_intr(void *intarg)
{
	struct rfc_softc *rfc_sc = intarg;
	struct rf_softc *rf_sc;
	int i;

	rf_sc = device_private(rfc_sc->sc_childs[rfc_sc->sc_curchild]);
	for (;;) {
		/*
		 * First clean up from previous command...
		 */
		switch (rf_sc->sc_state & RFS_CMDS) {
		case RFS_PROBING:	/* density detect / verify started */
			disk_unbusy(&rf_sc->sc_disk, 0, 1);
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_ERR) == 0) {
				RFS_SETCMD(rf_sc->sc_state, RFS_IDLE);
				wakeup(rf_sc);
			} else {
				if (rfc_sc->type == 2
				    && (rf_sc->sc_state & RFS_DENS) == 0
				    && (rf_sc->sc_state & RFS_AD) != 0) {
					/* retry at DD */
					rf_sc->sc_state |= RFS_DENS;
					disk_busy(&rf_sc->sc_disk);
					if (rfc_sendcmd(rfc_sc, RX2CS_RSEC
					    | RX2CS_IE | RX2CS_DD |
					    (rf_sc->sc_dnum == 0 ? 0 :
					    RX2CS_US), 1, 1) < 0) {
						disk_unbusy(&rf_sc->sc_disk,
						    0, 1);
						RFS_SETCMD(rf_sc->sc_state,
						    RFS_NOTINIT);
						wakeup(rf_sc);
					}
				} else {
					printf("%s: density error.\n",
					    device_xname(rf_sc->sc_dev));
					RFS_SETCMD(rf_sc->sc_state,RFS_NOTINIT);
					wakeup(rf_sc);
				}
			}
			return;
		case RFS_IDLE:	/* controller is idle */
			if (rfc_sc->sc_curbuf->b_bcount
			    % ((rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD) != 0) {
				/*
				 * can only handle blocks that are a multiple
				 * of the physical block size
				 */
				rfc_sc->sc_curbuf->b_error = EIO;
			}
			RFS_SETCMD(rf_sc->sc_state, (rfc_sc->sc_curbuf->b_flags
			    & B_READ) != 0 ? RFS_RSEC : RFS_FBUF);
			break;
		case RFS_RSEC:	/* Read Sector */
			disk_unbusy(&rf_sc->sc_disk, 0, 1);
			/* check for errors */
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_ERR) != 0) {
				/* should do more verbose error reporting */
				printf("rfc_intr: Error reading secotr: %x\n",
				    bus_space_read_2(rfc_sc->sc_iot,
				    rfc_sc->sc_ioh, RX2ES) );
				rfc_sc->sc_curbuf->b_error = EIO;
			}
			RFS_SETCMD(rf_sc->sc_state, RFS_EBUF);
			break;
		case RFS_WSEC:	/* Write Sector */
			i = (rf_sc->sc_state & RFS_DENS) == 0
				? RX2_BYTE_SD : RX2_BYTE_DD;
			disk_unbusy(&rf_sc->sc_disk, i, 0);
			/* check for errors */
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_ERR) != 0) {
				/* should do more verbose error reporting */
				printf("rfc_intr: Error writing secotr: %x\n",
				    bus_space_read_2(rfc_sc->sc_iot,
				    rfc_sc->sc_ioh, RX2ES) );
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			if (rfc_sc->sc_bytesleft > i) {
				rfc_sc->sc_bytesleft -= i;
				rfc_sc->sc_bufidx =
				    (char *)rfc_sc->sc_bufidx + i;
			} else {
				biodone(rfc_sc->sc_curbuf);
				rf_sc = get_new_buf( rfc_sc);
				if (rf_sc == NULL)
					return;
			}
			RFS_SETCMD(rf_sc->sc_state,
			    (rfc_sc->sc_curbuf->b_flags & B_READ) != 0
			    ? RFS_RSEC : RFS_FBUF);
			break;
		case RFS_FBUF:	/* Fill Buffer */
			disk_unbusy(&rf_sc->sc_disk, 0, 0);
			bus_dmamap_unload(rfc_sc->sc_dmat, rfc_sc->sc_dmam);
			/* check for errors */
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_ERR) != 0) {
				/* should do more verbose error reporting */
				printf("rfc_intr: Error while DMA: %x\n",
				    bus_space_read_2(rfc_sc->sc_iot,
				    rfc_sc->sc_ioh, RX2ES));
				rfc_sc->sc_curbuf->b_error = EIO;
			}
			RFS_SETCMD(rf_sc->sc_state, RFS_WSEC);
			break;
		case RFS_EBUF:	/* Empty Buffer */
			i = (rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD;
			disk_unbusy(&rf_sc->sc_disk, i, 1);
			bus_dmamap_unload(rfc_sc->sc_dmat, rfc_sc->sc_dmam);
			/* check for errors */
			if ((bus_space_read_2(rfc_sc->sc_iot, rfc_sc->sc_ioh,
			    RX2CS) & RX2CS_ERR) != 0) {
				/* should do more verbose error reporting */
				printf("rfc_intr: Error while DMA: %x\n",
				    bus_space_read_2(rfc_sc->sc_iot,
				    rfc_sc->sc_ioh, RX2ES));
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			if (rfc_sc->sc_bytesleft > i) {
				rfc_sc->sc_bytesleft -= i;
				rfc_sc->sc_bufidx =
				    (char *)rfc_sc->sc_bufidx + i;
			} else {
				biodone(rfc_sc->sc_curbuf);
				rf_sc = get_new_buf( rfc_sc);
				if (rf_sc == NULL)
					return;
			}
			RFS_SETCMD(rf_sc->sc_state,
			    (rfc_sc->sc_curbuf->b_flags & B_READ) != 0
			    ? RFS_RSEC : RFS_FBUF);
			break;
		case RFS_NOTINIT: /* Device is not open */
		case RFS_SMD:	/* Set Media Density */
		case RFS_RSTAT:	/* Read Status */
		case RFS_WDDS:	/* Write Deleted Data Sector */
		case RFS_REC:	/* Read Error Code */
		default:
			panic("Impossible state in rfc_intr(1): 0x%x\n",
			    rf_sc->sc_state & RFS_CMDS);
		}

		if (rfc_sc->sc_curbuf->b_error != 0) {
			/*
			 * An error occurred while processing this buffer.
			 * Finish it and try to get a new buffer to process.
			 * Return if there are no buffers in the queues.
			 * This loops until the queues are empty or a new
			 * action was successfully scheduled.
			 */
			rfc_sc->sc_curbuf->b_resid = rfc_sc->sc_bytesleft;
			rfc_sc->sc_curbuf->b_error = EIO;
			biodone(rfc_sc->sc_curbuf);
			rf_sc = get_new_buf( rfc_sc);
			if (rf_sc == NULL)
				return;
			continue;
		}

		/*
		 * ... then initiate next command.
		 */
		switch (rf_sc->sc_state & RFS_CMDS) {
		case RFS_EBUF:	/* Empty Buffer */
			i = bus_dmamap_load(rfc_sc->sc_dmat, rfc_sc->sc_dmam,
			    rfc_sc->sc_bufidx, (rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD,
			    rfc_sc->sc_curbuf->b_proc, BUS_DMA_NOWAIT);
			if (i != 0) {
				printf("rfc_intr: Error loading dmamap: %d\n",
				i);
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			disk_busy(&rf_sc->sc_disk);
			if (rfc_sendcmd(rfc_sc, RX2CS_EBUF | RX2CS_IE
			    | ((rf_sc->sc_state & RFS_DENS) == 0 ? 0 : RX2CS_DD)
			    | (rf_sc->sc_dnum == 0 ? 0 : RX2CS_US)
			    | ((rfc_sc->sc_dmam->dm_segs[0].ds_addr
			    & 0x30000) >>4), ((rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD) / 2,
			    rfc_sc->sc_dmam->dm_segs[0].ds_addr & 0xffff) < 0) {
				disk_unbusy(&rf_sc->sc_disk, 0, 1);
				rfc_sc->sc_curbuf->b_error = EIO;
				bus_dmamap_unload(rfc_sc->sc_dmat,
				rfc_sc->sc_dmam);
			}
			break;
		case RFS_FBUF:	/* Fill Buffer */
			i = bus_dmamap_load(rfc_sc->sc_dmat, rfc_sc->sc_dmam,
			    rfc_sc->sc_bufidx, (rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD,
			    rfc_sc->sc_curbuf->b_proc, BUS_DMA_NOWAIT);
			if (i != 0) {
				printf("rfc_intr: Error loading dmamap: %d\n",
				    i);
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			disk_busy(&rf_sc->sc_disk);
			if (rfc_sendcmd(rfc_sc, RX2CS_FBUF | RX2CS_IE
			    | ((rf_sc->sc_state & RFS_DENS) == 0 ? 0 : RX2CS_DD)
			    | (rf_sc->sc_dnum == 0 ? 0 : RX2CS_US)
			    | ((rfc_sc->sc_dmam->dm_segs[0].ds_addr
			    & 0x30000)>>4), ((rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD) / 2,
			    rfc_sc->sc_dmam->dm_segs[0].ds_addr & 0xffff) < 0) {
				disk_unbusy(&rf_sc->sc_disk, 0, 0);
				rfc_sc->sc_curbuf->b_error = EIO;
				bus_dmamap_unload(rfc_sc->sc_dmat,
				    rfc_sc->sc_dmam);
			}
			break;
		case RFS_WSEC:	/* Write Sector */
			i = (rfc_sc->sc_curbuf->b_bcount - rfc_sc->sc_bytesleft
			    + rfc_sc->sc_curbuf->b_blkno * DEV_BSIZE) /
			    ((rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD);
			if (i > RX2_TRACKS * RX2_SECTORS) {
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			disk_busy(&rf_sc->sc_disk);
			if (rfc_sendcmd(rfc_sc, RX2CS_WSEC | RX2CS_IE
			    | (rf_sc->sc_dnum == 0 ? 0 : RX2CS_US)
			    | ((rf_sc->sc_state& RFS_DENS) == 0 ? 0 : RX2CS_DD),
			    i % RX2_SECTORS + 1, i / RX2_SECTORS) < 0) {
				disk_unbusy(&rf_sc->sc_disk, 0, 0);
				rfc_sc->sc_curbuf->b_error = EIO;
			}
			break;
		case RFS_RSEC:	/* Read Sector */
			i = (rfc_sc->sc_curbuf->b_bcount - rfc_sc->sc_bytesleft
			    + rfc_sc->sc_curbuf->b_blkno * DEV_BSIZE) /
			    ((rf_sc->sc_state & RFS_DENS) == 0
			    ? RX2_BYTE_SD : RX2_BYTE_DD);
			if (i > RX2_TRACKS * RX2_SECTORS) {
				rfc_sc->sc_curbuf->b_error = EIO;
				break;
			}
			disk_busy(&rf_sc->sc_disk);
			if (rfc_sendcmd(rfc_sc, RX2CS_RSEC | RX2CS_IE
			    | (rf_sc->sc_dnum == 0 ? 0 : RX2CS_US)
			    | ((rf_sc->sc_state& RFS_DENS) == 0 ? 0 : RX2CS_DD),
			    i % RX2_SECTORS + 1, i / RX2_SECTORS) < 0) {
				disk_unbusy(&rf_sc->sc_disk, 0, 1);
				rfc_sc->sc_curbuf->b_error = EIO;
			}
			break;
		case RFS_NOTINIT: /* Device is not open */
		case RFS_PROBING: /* density detect / verify started */
		case RFS_IDLE:	/* controller is idle */
		case RFS_SMD:	/* Set Media Density */
		case RFS_RSTAT:	/* Read Status */
		case RFS_WDDS:	/* Write Deleted Data Sector */
		case RFS_REC:	/* Read Error Code */
		default:
			panic("Impossible state in rfc_intr(2): 0x%x\n",
			    rf_sc->sc_state & RFS_CMDS);
		}

		if (rfc_sc->sc_curbuf->b_error != 0) {
			/*
			 * An error occurred while processing this buffer.
			 * Finish it and try to get a new buffer to process.
			 * Return if there are no buffers in the queues.
			 * This loops until the queues are empty or a new
			 * action was successfully scheduled.
			 */
			rfc_sc->sc_curbuf->b_resid = rfc_sc->sc_bytesleft;
			rfc_sc->sc_curbuf->b_error = EIO;
			biodone(rfc_sc->sc_curbuf);
			rf_sc = get_new_buf( rfc_sc);
			if (rf_sc == NULL)
				return;
			continue;
		}
		break;
	}
	return;
}



int
rfdump(dev_t dev, daddr_t blkno, void *va, size_t size)
{

	/* A 0.5MB floppy is much to small to take a system dump... */
	return(ENXIO);
}



int
rfsize(dev_t dev)
{

	return(-1);
}



int
rfopen(dev_t dev, int oflags, int devtype, struct lwp *l)
{
	struct rf_softc *rf_sc;
	struct rfc_softc *rfc_sc;
	struct disklabel *dl;

	if ((rf_sc = device_lookup_private(&rf_cd, DISKUNIT(dev))) == NULL)
		return ENXIO;

	rfc_sc = rf_sc->sc_rfc;
	dl = rf_sc->sc_disk.dk_label;
	switch (DISKPART(dev)) {
		case 0:			/* Part. a is single density. */
			/* opening in single and double density is senseless */
			if ((rf_sc->sc_state & RFS_OPEN_B) != 0 )
				return(ENXIO);
			rf_sc->sc_state &= ~RFS_DENS;
			rf_sc->sc_state &= ~RFS_AD;
			rf_sc->sc_state |= RFS_OPEN_A;
		break;
		case 1:			/* Part. b is double density. */
			/*
			 * Opening a single density only drive in double
			 * density or simultaneous opening in single and
			 * double density is senseless.
			 */
			if (rfc_sc->type == 1
			    || (rf_sc->sc_state & RFS_OPEN_A) != 0 )
				return(ENXIO);
			rf_sc->sc_state |= RFS_DENS;
			rf_sc->sc_state &= ~RFS_AD;
			rf_sc->sc_state |= RFS_OPEN_B;
		break;
		case 2:			/* Part. c is auto density. */
			rf_sc->sc_state |= RFS_AD;
			rf_sc->sc_state |= RFS_OPEN_C;
		break;
		default:
			return(ENXIO);
		break;
	}
	if ((rf_sc->sc_state & RFS_CMDS) == RFS_NOTINIT) {
		rfc_sc->sc_curchild = rf_sc->sc_dnum;
		/*
		 * Controller is idle and density is not detected.
		 * Start a density probe by issuing a read sector command
		 * and sleep until the density probe finished.
		 * Due to this it is imposible to open unformatted media.
		 * As the RX02/02 is not able to format its own media,
		 * media must be purchased preformatted. fsck DEC makreting!
		 */
		RFS_SETCMD(rf_sc->sc_state, RFS_PROBING);
		disk_busy(&rf_sc->sc_disk);
		if (rfc_sendcmd(rfc_sc, RX2CS_RSEC | RX2CS_IE
		    | (rf_sc->sc_dnum == 0 ? 0 : RX2CS_US)
		    | ((rf_sc->sc_state & RFS_DENS) == 0 ? 0 : RX2CS_DD),
		    1, 1) < 0) {
			rf_sc->sc_state = 0;
			return(ENXIO);
		}
		/* wait max. 2 sec for density probe to finish */
		if (tsleep(rf_sc, PRIBIO | PCATCH, "density probe", 2 * hz)
		    != 0 || (rf_sc->sc_state & RFS_CMDS) == RFS_NOTINIT) {
			/* timeout elapsed and / or something went wrong */
			rf_sc->sc_state = 0;
			return(ENXIO);
		}
	}
	/* disklabel. We use different fake geometries for SD and DD. */
	if ((rf_sc->sc_state & RFS_DENS) == 0) {
		dl->d_nsectors = 10;		/* sectors per track */
		dl->d_secpercyl = 10;		/* sectors per cylinder */
		dl->d_ncylinders = 50;		/* cylinders per unit */
		dl->d_secperunit = 501; /* sectors per unit */
		/* number of sectors in partition */
		dl->d_partitions[2].p_size = 500;
	} else {
		dl->d_nsectors = RX2_SECTORS / 2;  /* sectors per track */
		dl->d_secpercyl = RX2_SECTORS / 2; /* sectors per cylinder */
		dl->d_ncylinders = RX2_TRACKS;	   /* cylinders per unit */
		/* sectors per unit */
		dl->d_secperunit = RX2_SECTORS * RX2_TRACKS / 2;
		/* number of sectors in partition */
		dl->d_partitions[2].p_size = RX2_SECTORS * RX2_TRACKS / 2;
	}
	return(0);
}



int
rfclose(dev_t dev, int fflag, int devtype, struct lwp *l)
{
	struct rf_softc *rf_sc = device_lookup_private(&rf_cd, DISKUNIT(dev));

	if ((rf_sc->sc_state & 1 << (DISKPART(dev) + RFS_OPEN_SHIFT)) == 0)
		panic("rfclose: can not close non-open drive %s "
		    "partition %"PRIu32, device_xname(rf_sc->sc_dev), DISKPART(dev));
	else
		rf_sc->sc_state &= ~(1 << (DISKPART(dev) + RFS_OPEN_SHIFT));
	if ((rf_sc->sc_state & RFS_OPEN_MASK) == 0)
		rf_sc->sc_state = 0;
	return(0);
}



int
rfread(dev_t dev, struct uio *uio, int ioflag)
{

	return(physio(rfstrategy, NULL, dev, B_READ, minphys, uio));
}



int
rfwrite(dev_t dev, struct uio *uio, int ioflag)
{

	return(physio(rfstrategy, NULL, dev, B_WRITE, minphys, uio));
}



int
rfioctl(dev_t dev, u_long cmd, void *data, int fflag, struct lwp *l)
{
	struct rf_softc *rf_sc = device_lookup_private(&rf_cd, DISKUNIT(dev));
	int error;

	/* We are going to operate on a non-open dev? PANIC! */
	if ((rf_sc->sc_state & 1 << (DISKPART(dev) + RFS_OPEN_SHIFT)) == 0)
		panic("rfioctl: can not operate on non-open drive %s "
		    "partition %"PRIu32, device_xname(rf_sc->sc_dev), DISKPART(dev));
	error = disk_ioctl(&rf_sc->sc_disk, dev, cmd, data, fflag, l);
	if (error != EPASSTHROUGH)
		return error;

	switch (cmd) {
	/* get and set disklabel; DIOCGPART used internally */
	case DIOCSDINFO: /* set */
		return(0);
	case DIOCWDINFO: /* set, update disk */
		return(0);
	/* do format operation, read or write */
	case DIOCRFORMAT:
	break;
	case DIOCWFORMAT:
	break;

	case DIOCSSTEP: /* set step rate */
	break;
	case DIOCSRETRIES: /* set # of retries */
	break;
	case DIOCKLABEL: /* keep/drop label on close? */
	break;
	case DIOCWLABEL: /* write en/disable label */
	break;

/*	case DIOCSBAD: / * set kernel dkbad */
	break; /* */
	case DIOCEJECT: /* eject removable disk */
	break;
	case ODIOCEJECT: /* eject removable disk */
	break;
	case DIOCLOCK: /* lock/unlock pack */
	break;

	/* get default label, clear label */
	case DIOCGDEFLABEL:
	break;
	case DIOCCLRLABEL:
	break;
	default:
		return(ENOTTY);
	}

	return(ENOTTY);
}

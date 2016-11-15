/* $NetBSD: spiflash.h,v 1.4 2009/05/12 14:45:08 cegger Exp $ */

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

#ifndef	_DEV_SPI_SPIFLASH_H_
#define	_DEV_SPI_SPIFLASH_H_

#define	SPIFLASH_CMD_RDSR		0x05	/* read status register */
#define	SPIFLASH_CMD_WRSR		0x01	/* write status register */
#define	SPIFLASH_CMD_WREN		0x06	/* enable WRSR */
#define	SPIFLASH_CMD_WRDI		0x04	/* disable WRSR */

/*
 * Different chips offer different ways to read a device ID, although
 * newer parts should all offer the standard JEDEC variant.
 * Additionally, many parts have a faster read, though how to make use
 * of this sometimes requires special hacks.  E.g. some parts use an
 * extra data pin, and some crank the clock rate up.
 */
#define	SPIFLASH_CMD_READ		0x03	/* read data (normal) */
#define	SPIFLASH_CMD_RDID		0xab	/* read id */
#define	SPIFLASH_CMD_RDID2		0x90	/* read id (alternate) */
#define	SPIFLASH_CMD_RDJI		0x9f	/* read JEDEC id */
#define	SPIFLASH_CMD_READFAST		0x0b	/* fast read */

/*
 * Different chips offer different variations on the sector erase.
 * E.g. SST parts offer 4k, 32k, and 64k erase sizes on the same part,
 * with just different cmds.  However, at least SST, AMD, and Winbond
 * all offer at least the main (0xd8) variant.
 */
#define	SPIFLASH_CMD_ERASE		0xd8	/* sector erase */
#define	SPIFLASH_CMD_ERASE2		0x52	/* sector erase (alternate) */
#define	SPIFLASH_CMD_ERASE3		0x20	/* sector erase (alternate) */
#define	SPIFLASH_CMD_ERASE4		0x81	/* page erase */
#define	SPIFLASH_CMD_CHIPERASE		0xc7	/* chip erase */

/*
 * Some parts can stream bytes with the program command, whereas others require
 * a separate command sequence for each byte.
 */
#define	SPIFLASH_CMD_PROGRAM		0x02	/* page or byte program */
#define	SPIFLASH_CMD_PROGRAM_AA		0xad	/* program (autoincrement) */

/*
 * Some additional commands.  Again, mostly device specific.
 */
#define	SPIFLASH_CMD_EBSY		0x70	/* output busy signal (SST) */
#define	SPIFLASH_CMD_DBSY		0x80	/* disable busy signal (SST) */

/*
 * Status register bits.  Not all devices implement all bits.  In
 * addition, the meanings of the BP bits seem to vary from device to
 * device.
 */
#define	SPIFLASH_SR_BUSY		0x01	/* program in progress */
#define	SPIFLASH_SR_WEL			0x02	/* write enable latch */
#define	SPIFLASH_SR_BP0			0x04	/* block protect bits */
#define	SPIFLASH_SR_BP1			0x08
#define	SPIFLASH_SR_BP2			0x10
#define	SPIFLASH_SR_BP3			0x20
#define	SPIFLASH_SR_AAI			0x40	/* auto-increment mode */
#define	SPIFLASH_SR_SRP			0x80	/* SR write protected */

/*
 * This needs to change to accommodate boot-sectored devices.
 */

typedef struct spiflash_softc *spiflash_handle_t;

struct spiflash_hw_if {
	/*
	 * Driver MUST provide these.
	 */
	const char *(*sf_getname)(void *);
	struct spi_handle *(*sf_gethandle)(void *);
	int	(*sf_getflags)(void *);
	int	(*sf_getsize)(void *, int);

	/*
	 * SPI framework will provide these if the driver does not.
	 */
	int	(*sf_erase)(spiflash_handle_t, size_t, size_t);
	int	(*sf_write)(spiflash_handle_t, size_t, size_t,
	    const uint8_t *);
	int	(*sf_read)(spiflash_handle_t, size_t, size_t, uint8_t *);
	/*
	 * Not implemented yet.
	 */
	int	(*sf_getstatus)(spiflash_handle_t, int, int);
	int	(*sf_setstatus)(spiflash_handle_t, int, int, int);
};

#define	SPIFLASH_SIZE_DEVICE		0
#define	SPIFLASH_SIZE_ERASE		1
#define	SPIFLASH_SIZE_WRITE		2	/* return -1 for unlimited */
#define	SPIFLASH_SIZE_READ		3	/* return -1 for unlimited */
#define	SPIFLASH_SIZE_COUNT		4

#define	SPIFLASH_FLAG_FAST_READ		0x0004	/* use fast read sequence */

spiflash_handle_t spiflash_attach_mi(const struct spiflash_hw_if *, void *,
    device_t);
void spiflash_set_private(spiflash_handle_t, void *);
void *spiflash_get_private(spiflash_handle_t);
int spiflash_read_status(spiflash_handle_t, uint8_t *);
int spiflash_write_disable(spiflash_handle_t);
int spiflash_write_enable(spiflash_handle_t);
int spiflash_cmd(spiflash_handle_t, uint8_t, size_t, uint32_t, size_t,
    const uint8_t *, uint8_t *);
int spiflash_wait(spiflash_handle_t, int);


#endif	/* _DEV_SPI_SPIFLASH_H_ */

/*	$NetBSD: sdhcvar.h,v 1.24 2015/08/09 13:24:39 mlelstv Exp $	*/
/*	$OpenBSD: sdhcvar.h,v 1.3 2007/09/06 08:01:01 jsg Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SDHCVAR_H_
#define _SDHCVAR_H_

#include <sys/bus.h>
#include <sys/device.h>
#include <sys/pmf.h>

struct sdhc_host;
struct sdmmc_command;

struct sdhc_softc {
	device_t		sc_dev;

	struct sdhc_host	**sc_host;
	int			sc_nhosts;

	bus_dma_tag_t		sc_dmat;

	uint32_t		sc_flags;
#define	SDHC_FLAG_USE_DMA	0x00000001
#define	SDHC_FLAG_FORCE_DMA	0x00000002
#define	SDHC_FLAG_NO_PWR0	0x00000004 /* Freescale ESDHC */
#define	SDHC_FLAG_HAVE_DVS	0x00000008 /* Freescale ESDHC */
#define	SDHC_FLAG_32BIT_ACCESS	0x00000010 /* Freescale ESDHC */
#define	SDHC_FLAG_ENHANCED	0x00000020 /* Freescale ESDHC */
#define	SDHC_FLAG_8BIT_MODE	0x00000040 /* MMC 8bit mode is supported */
#define	SDHC_FLAG_HAVE_CGM	0x00000080 /* Netlogic XLP */
#define	SDHC_FLAG_NO_LED_ON	0x00000100 /* LED_ON unsupported in HOST_CTL */
#define	SDHC_FLAG_HOSTCAPS	0x00000200 /* No device provided capabilities */
#define	SDHC_FLAG_RSP136_CRC	0x00000400 /* Resp 136 with CRC and end-bit */
#define	SDHC_FLAG_SINGLE_ONLY	0x00000800 /* Single transfer only */
#define	SDHC_FLAG_WAIT_RESET	0x00001000 /* Wait for soft resets to start */
#define	SDHC_FLAG_NO_HS_BIT	0x00002000 /* Don't set SDHC_HIGH_SPEED bit */
#define	SDHC_FLAG_EXTERNAL_DMA	0x00004000
#define	SDHC_FLAG_EXTDMA_DMAEN	0x00008000 /* ext. dma need SDHC_DMA_ENABLE */
#define	SDHC_FLAG_NO_CLKBASE	0x00020000 /* ignore clkbase register */
#define	SDHC_FLAG_SINGLE_POWER_WRITE 0x00040000
#define	SDHC_FLAG_NO_TIMEOUT	0x00080000 /* ignore timeout interrupts */
#define	SDHC_FLAG_USE_ADMA2	0x00100000
#define	SDHC_FLAG_POLL_CARD_DET	0x00200000 /* polling card detect */
#define	SDHC_FLAG_SLOW_SDR50  	0x00400000 /* reduce SDR50 speed */

	uint32_t		sc_clkbase;
	int			sc_clkmsk;	/* Mask for SDCLK */
	uint32_t		sc_caps;/* attachment provided capabilities */
	uint32_t		sc_caps2;

	int (*sc_vendor_rod)(struct sdhc_softc *, int);
	int (*sc_vendor_write_protect)(struct sdhc_softc *);
	int (*sc_vendor_card_detect)(struct sdhc_softc *);
	int (*sc_vendor_bus_clock)(struct sdhc_softc *, int);
	int (*sc_vendor_transfer_data_dma)(struct sdhc_softc *, struct sdmmc_command *);
};

/* Host controller functions called by the attachment driver. */
int	sdhc_host_found(struct sdhc_softc *, bus_space_tag_t,
	    bus_space_handle_t, bus_size_t);
int	sdhc_intr(void *);
int	sdhc_detach(struct sdhc_softc *, int);
bool	sdhc_suspend(device_t, const pmf_qual_t *);
bool	sdhc_resume(device_t, const pmf_qual_t *);
bool	sdhc_shutdown(device_t, int);
kmutex_t *sdhc_host_lock(struct sdhc_host *);

#endif	/* _SDHCVAR_H_ */

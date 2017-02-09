/*	$NetBSD: sdmmcchip.h,v 1.7 2015/08/05 10:29:37 jmcneill Exp $	*/
/*	$OpenBSD: sdmmcchip.h,v 1.3 2007/05/31 10:09:01 uwe Exp $	*/

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

#ifndef	_SDMMC_CHIP_H_
#define	_SDMMC_CHIP_H_

#include <sys/device.h>

#include <sys/bus.h>

struct sdmmc_command;

typedef struct sdmmc_chip_functions *sdmmc_chipset_tag_t;
typedef struct sdmmc_spi_chip_functions *sdmmc_spi_chipset_tag_t;
typedef void *sdmmc_chipset_handle_t;

struct sdmmc_chip_functions {
	/* host controller reset */
	int		(*host_reset)(sdmmc_chipset_handle_t);

	/* host capabilities */
	uint32_t	(*host_ocr)(sdmmc_chipset_handle_t);
	int		(*host_maxblklen)(sdmmc_chipset_handle_t);

	/* card detection */
	int		(*card_detect)(sdmmc_chipset_handle_t);

	/* write protect */
	int		(*write_protect)(sdmmc_chipset_handle_t);

	/* bus power, clock frequency, width and ROD(OpenDrain/PushPull) */
	int		(*bus_power)(sdmmc_chipset_handle_t, uint32_t);
	int		(*bus_clock)(sdmmc_chipset_handle_t, int);
	int		(*bus_width)(sdmmc_chipset_handle_t, int);
	int		(*bus_rod)(sdmmc_chipset_handle_t, int);

	/* command execution */
	void		(*exec_command)(sdmmc_chipset_handle_t,
			    struct sdmmc_command *);

	/* card interrupt */
	void		(*card_enable_intr)(sdmmc_chipset_handle_t, int);
	void		(*card_intr_ack)(sdmmc_chipset_handle_t);

	/* UHS functions */
	int		(*signal_voltage)(sdmmc_chipset_handle_t, int);
	int		(*bus_clock_ddr)(sdmmc_chipset_handle_t, int, bool);
	int		(*execute_tuning)(sdmmc_chipset_handle_t, int);
};

/* host controller reset */
#define sdmmc_chip_host_reset(tag, handle)				\
	((tag)->host_reset((handle)))
/* host capabilities */
#define sdmmc_chip_host_ocr(tag, handle)				\
	((tag)->host_ocr((handle)))
#define sdmmc_chip_host_maxblklen(tag, handle)				\
	((tag)->host_maxblklen((handle)))
/* card detection */
#define sdmmc_chip_card_detect(tag, handle)				\
	((tag)->card_detect((handle)))
/* write protect */
#define sdmmc_chip_write_protect(tag, handle)				\
	((tag)->write_protect((handle)))
/* bus power, clock frequency, width and rod */
#define sdmmc_chip_bus_power(tag, handle, ocr)				\
	((tag)->bus_power((handle), (ocr)))
#define sdmmc_chip_bus_clock(tag, handle, freq, ddr)			\
	((tag)->bus_clock_ddr ? (tag)->bus_clock_ddr((handle), (freq), (ddr)) : ((ddr) ? EINVAL : ((tag)->bus_clock((handle), (freq)))))
#define sdmmc_chip_bus_width(tag, handle, width)			\
	((tag)->bus_width((handle), (width)))
#define sdmmc_chip_bus_rod(tag, handle, width)				\
	((tag)->bus_rod((handle), (width)))
/* command execution */
#define sdmmc_chip_exec_command(tag, handle, cmdp)			\
	((tag)->exec_command((handle), (cmdp)))
/* card interrupt */
#define sdmmc_chip_card_enable_intr(tag, handle, enable)		\
	((tag)->card_enable_intr((handle), (enable)))
#define sdmmc_chip_card_intr_ack(tag, handle)				\
	((tag)->card_intr_ack((handle)))
/* UHS functions */
#define sdmmc_chip_signal_voltage(tag, handle, voltage)			\
	((tag)->signal_voltage((handle), (voltage)))
#define sdmmc_chip_execute_tuning(tag, handle, timing)			\
	((tag)->execute_tuning ? (tag)->execute_tuning((handle), (timing)) : EINVAL)

/* clock frequencies for sdmmc_chip_bus_clock() */
#define SDMMC_SDCLK_OFF		0
#define SDMMC_SDCLK_400K	400

/* voltage levels for sdmmc_chip_signal_voltage() */
#define SDMMC_SIGNAL_VOLTAGE_330	0
#define SDMMC_SIGNAL_VOLTAGE_180	1

/* timings for sdmmc_chip_execute_tuning() */
#define SDMMC_TIMING_UHS_SDR50		0
#define SDMMC_TIMING_UHS_SDR104		1
#define SDMMC_TIMING_MMC_HS200		2

/* SPI mode */
struct sdmmc_spi_chip_functions {
	/* card initialize */
	void		(*initialize)(sdmmc_chipset_handle_t);
};
#define sdmmc_spi_chip_initialize(tag, handle)				\
	((tag)->initialize((handle)))

struct sdmmcbus_attach_args {
	const char		*saa_busname;
	sdmmc_chipset_tag_t	saa_sct;
	sdmmc_spi_chipset_tag_t	saa_spi_sct;
	sdmmc_chipset_handle_t	saa_sch;
	bus_dma_tag_t		saa_dmat;
	u_int			saa_clkmin;
	u_int			saa_clkmax;
	uint32_t		saa_caps;	/* see sdmmc_softc.sc_caps */
};

void	sdmmc_needs_discover(device_t);
void	sdmmc_card_intr(device_t);
void	sdmmc_delay(u_int);

#endif	/* _SDMMC_CHIP_H_ */

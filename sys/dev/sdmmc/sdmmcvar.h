/*	$NetBSD: sdmmcvar.h,v 1.20 2015/10/06 14:32:51 mlelstv Exp $	*/
/*	$OpenBSD: sdmmcvar.h,v 1.13 2009/01/09 10:55:22 jsg Exp $	*/

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

#ifndef	_SDMMCVAR_H_
#define	_SDMMCVAR_H_

#ifdef _KERNEL_OPT
#include "opt_sdmmc.h"
#endif

#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/callout.h>

#include <sys/bus.h>

#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>

#define	SDMMC_SECTOR_SIZE_SB	9
#define	SDMMC_SECTOR_SIZE	(1 << SDMMC_SECTOR_SIZE_SB)	/* =512 */

struct sdmmc_csd {
	int	csdver;		/* CSD structure format */
	u_int	mmcver;		/* MMC version (for CID format) */
	int	capacity;	/* total number of sectors */
	int	read_bl_len;	/* block length for reads */
	int	write_bl_len;	/* block length for writes */
	int	r2w_factor;
	int	tran_speed;	/* transfer speed (kbit/s) */
	int	ccc;		/* Card Command Class for SD */
	/* ... */
};

struct sdmmc_cid {
	int	mid;		/* manufacturer identification number */
	int	oid;		/* OEM/product identification number */
	char	pnm[8];		/* product name (MMC v1 has the longest) */
	int	rev;		/* product revision */
	int	psn;		/* product serial number */
	int	mdt;		/* manufacturing date */
};

struct sdmmc_scr {
	int	sd_spec;
	int	bus_width;
};

typedef uint32_t sdmmc_response[4];

struct sdmmc_softc;

struct sdmmc_task {
	void (*func)(void *arg);
	void *arg;
	int onqueue;
	struct sdmmc_softc *sc;
	TAILQ_ENTRY(sdmmc_task) next;
};

#define	sdmmc_init_task(xtask, xfunc, xarg)				\
do {									\
	(xtask)->func = (xfunc);					\
	(xtask)->arg = (xarg);						\
	(xtask)->onqueue = 0;						\
	(xtask)->sc = NULL;						\
} while (/*CONSTCOND*/0)

#define sdmmc_task_pending(xtask) ((xtask)->onqueue)

struct sdmmc_command {
	struct sdmmc_task c_task;	/* task queue entry */
	uint16_t	 c_opcode;	/* SD or MMC command index */
	uint32_t	 c_arg;		/* SD/MMC command argument */
	sdmmc_response	 c_resp;	/* response buffer */
	bus_dmamap_t	 c_dmamap;
	int		 c_dmaseg;	/* DMA segment number */
	int		 c_dmaoff;	/* offset in DMA segment */
	void		*c_data;	/* buffer to send or read into */
	int		 c_datalen;	/* length of data buffer */
	int		 c_blklen;	/* block length */
	int		 c_flags;	/* see below */
#define SCF_ITSDONE	(1U << 0)		/* command is complete */
#define SCF_RSP_PRESENT	(1U << 1)
#define SCF_RSP_BSY	(1U << 2)
#define SCF_RSP_136	(1U << 3)
#define SCF_RSP_CRC	(1U << 4)
#define SCF_RSP_IDX	(1U << 5)
#define SCF_CMD_READ	(1U << 6)	/* read command (data expected) */
/* non SPI */
#define SCF_CMD_AC	(0U << 8)
#define SCF_CMD_ADTC	(1U << 8)
#define SCF_CMD_BC	(2U << 8)
#define SCF_CMD_BCR	(3U << 8)
#define SCF_CMD_MASK	(3U << 8)
/* SPI */
#define SCF_RSP_SPI_S1	(1U << 10)
#define SCF_RSP_SPI_S2	(1U << 11)
#define SCF_RSP_SPI_B4	(1U << 12)
#define SCF_RSP_SPI_BSY	(1U << 13)
/* Probing */
#define SCF_TOUT_OK	(1U << 14)	/* command timeout expected */
/* response types */
#define SCF_RSP_R0	0	/* none */
#define SCF_RSP_R1	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R1B	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX|SCF_RSP_BSY)
#define SCF_RSP_R2	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_136)
#define SCF_RSP_R3	(SCF_RSP_PRESENT)
#define SCF_RSP_R4	(SCF_RSP_PRESENT)
#define SCF_RSP_R5	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R5B	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX|SCF_RSP_BSY)
#define SCF_RSP_R6	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R7	(SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_MASK	(0x1f << 1)
/* SPI */
#define SCF_RSP_SPI_R1	(SCF_RSP_SPI_S1)
#define SCF_RSP_SPI_R1B	(SCF_RSP_SPI_S1|SCF_RSP_SPI_BSY)
#define SCF_RSP_SPI_R2	(SCF_RSP_SPI_S1|SCF_RSP_SPI_S2)
#define SCF_RSP_SPI_R3	(SCF_RSP_SPI_S1|SCF_RSP_SPI_B4)
#define SCF_RSP_SPI_R4	(SCF_RSP_SPI_S1|SCF_RSP_SPI_B4)
#define SCF_RSP_SPI_R5	(SCF_RSP_SPI_S1|SCF_RSP_SPI_S2)
#define SCF_RSP_SPI_R7	(SCF_RSP_SPI_S1|SCF_RSP_SPI_B4)
#define SCF_RSP_SPI_MASK (0xf << 10)
	int		 c_error;	/* errno value on completion */

	/* Host controller owned fields for data xfer in progress */
	int c_resid;			/* remaining I/O */
	u_char *c_buf;			/* remaining data */
};

/*
 * Decoded PC Card 16 based Card Information Structure (CIS),
 * per card (function 0) and per function (1 and greater).
 */
struct sdmmc_cis {
	uint16_t	 manufacturer;
#define SDMMC_VENDOR_INVALID	0xffff
	uint16_t	 product;
#define SDMMC_PRODUCT_INVALID	0xffff
	uint8_t		 function;
#define SDMMC_FUNCTION_INVALID	0xff
	u_char		 cis1_major;
	u_char		 cis1_minor;
	char		 cis1_info_buf[256];
	char		*cis1_info[4];
};

/*
 * Structure describing either an SD card I/O function or a SD/MMC
 * memory card from a "stack of cards" that responded to CMD2.  For a
 * combo card with one I/O function and one memory card, there will be
 * two of these structures allocated.  Each card slot has such a list
 * of sdmmc_function structures.
 */
struct sdmmc_function {
	/* common members */
	struct sdmmc_softc *sc;		/* card slot softc */
	uint16_t rca;			/* relative card address */
	int interface;			/* SD/MMC:0, SDIO:standard interface */
	int width;			/* bus width */
	int flags;
#define SFF_ERROR		0x0001	/* function is poo; ignore it */
#define SFF_SDHC		0x0002	/* SD High Capacity card */
	SIMPLEQ_ENTRY(sdmmc_function) sf_list;
	/* SD card I/O function members */
	int number;			/* I/O function number or -1 */
	device_t child;			/* function driver */
	struct sdmmc_cis cis;		/* decoded CIS */
	/* SD/MMC memory card members */
	struct sdmmc_csd csd;		/* decoded CSD value */
	struct sdmmc_cid cid;		/* decoded CID value */
	sdmmc_response raw_cid;		/* temp. storage for decoding */
	uint32_t raw_scr[2];
	struct sdmmc_scr scr;		/* decoded SCR value */

	void *bbuf;			/* bounce buffer */
	bus_dmamap_t bbuf_dmap;		/* DMA map for bounce buffer */
	bus_dmamap_t sseg_dmap;		/* DMA map for single segment */
};

/*
 * Structure describing a single SD/MMC/SDIO card slot.
 */
struct sdmmc_softc {
	device_t sc_dev;		/* base device */
#define SDMMCDEVNAME(sc)	(device_xname(sc->sc_dev))

	sdmmc_chipset_tag_t sc_sct;	/* host controller chipset tag */
	sdmmc_spi_chipset_tag_t sc_spi_sct;
	sdmmc_chipset_handle_t sc_sch;	/* host controller chipset handle */
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_dmap;
#define	SDMMC_MAXNSEGS		((MAXPHYS / PAGE_SIZE) + 1)

	struct kmutex sc_mtx;		/* lock around host controller */
	int sc_dying;			/* bus driver is shutting down */

	uint32_t sc_flags;
#define SMF_INITED		0x0001
#define SMF_SD_MODE		0x0002	/* host in SD mode (MMC otherwise) */
#define SMF_IO_MODE		0x0004	/* host in I/O mode (SD mode only) */
#define SMF_MEM_MODE		0x0008	/* host in memory mode (SD or MMC) */
#define SMF_CARD_PRESENT	0x4000	/* card presence noticed */
#define SMF_CARD_ATTACHED	0x8000	/* card driver(s) attached */
#define SMF_UHS_MODE		0x10000	/* host in UHS mode */

	uint32_t sc_caps;		/* host capability */
#define SMC_CAPS_AUTO_STOP	0x0001	/* send CMD12 automagically by host */
#define SMC_CAPS_4BIT_MODE	0x0002	/* 4-bits data bus width */
#define SMC_CAPS_DMA		0x0004	/* DMA transfer */
#define SMC_CAPS_SPI_MODE	0x0008	/* SPI mode */
#define SMC_CAPS_POLL_CARD_DET	0x0010	/* Polling card detect */
#define SMC_CAPS_SINGLE_ONLY	0x0020	/* only single read/write */
#define SMC_CAPS_8BIT_MODE	0x0040	/* 8-bits data bus width */
#define SMC_CAPS_MULTI_SEG_DMA	0x0080	/* multiple segment DMA transfer */
#define SMC_CAPS_SD_HIGHSPEED	0x0100	/* SD high-speed timing */
#define SMC_CAPS_MMC_HIGHSPEED	0x0200	/* MMC high-speed timing */
#define SMC_CAPS_UHS_SDR50	0x1000	/* UHS SDR50 timing */
#define SMC_CAPS_UHS_SDR104	0x2000	/* UHS SDR104 timing */
#define SMC_CAPS_UHS_DDR50	0x4000	/* UHS DDR50 timing */
#define SMC_CAPS_UHS_MASK	0x7000
#define SMC_CAPS_MMC_HS200	0x8000	/* eMMC HS200 timing */

	/* function */
	int sc_function_count;		/* number of I/O functions (SDIO) */
	struct sdmmc_function *sc_card;	/* selected card */
	struct sdmmc_function *sc_fn0;	/* function 0, the card itself */
	SIMPLEQ_HEAD(, sdmmc_function) sf_head; /* list of card functions */

	/* task queue */
	struct lwp *sc_tskq_lwp;	/* asynchronous tasks */
	TAILQ_HEAD(, sdmmc_task) sc_tskq;   /* task thread work queue */
	struct kmutex sc_tskq_mtx;
	struct kcondvar sc_tskq_cv;

	/* discover task */
	struct sdmmc_task sc_discover_task; /* card attach/detach task */
	struct kmutex sc_discover_task_mtx;

	/* interrupt task */
	struct sdmmc_task sc_intr_task;	/* card interrupt task */
	struct kmutex sc_intr_task_mtx;
	TAILQ_HEAD(, sdmmc_intr_handler) sc_intrq; /* interrupt handlers */

	u_int sc_clkmin;		/* host min bus clock */
	u_int sc_clkmax;		/* host max bus clock */
	u_int sc_busclk;		/* host bus clock */
	bool sc_busddr;			/* host bus clock is in DDR mode */
	int sc_buswidth;		/* host bus width */
	const char *sc_transfer_mode;	/* current transfer mode */

	callout_t sc_card_detect_ch;	/* polling card insert/remove */
};

/*
 * Attach devices at the sdmmc bus.
 */
struct sdmmc_attach_args {
	uint16_t manufacturer;
	uint16_t product;
	int interface;
	struct sdmmc_function *sf;
};

struct sdmmc_product {
	uint16_t	pp_vendor;
	uint16_t	pp_product;
	const char	*pp_cisinfo[4];
};

#ifndef	IPL_SDMMC
#define IPL_SDMMC	IPL_BIO
#endif

#ifndef	splsdmmc
#define splsdmmc()	splbio()
#endif

#define	SDMMC_LOCK(sc)
#define	SDMMC_UNLOCK(sc)

#ifdef SDMMC_DEBUG
extern int sdmmcdebug;
#endif

void	sdmmc_add_task(struct sdmmc_softc *, struct sdmmc_task *);
void	sdmmc_del_task(struct sdmmc_task *);

struct	sdmmc_function *sdmmc_function_alloc(struct sdmmc_softc *);
void	sdmmc_function_free(struct sdmmc_function *);
int	sdmmc_set_bus_power(struct sdmmc_softc *, uint32_t, uint32_t);
int	sdmmc_mmc_command(struct sdmmc_softc *, struct sdmmc_command *);
int	sdmmc_app_command(struct sdmmc_softc *, struct sdmmc_function *,
	    struct sdmmc_command *);
void	sdmmc_stop_transmission(struct sdmmc_softc *);
void	sdmmc_go_idle_state(struct sdmmc_softc *);
int	sdmmc_select_card(struct sdmmc_softc *, struct sdmmc_function *);
int	sdmmc_set_relative_addr(struct sdmmc_softc *, struct sdmmc_function *);

void	sdmmc_intr_enable(struct sdmmc_function *);
void	sdmmc_intr_disable(struct sdmmc_function *);
void	*sdmmc_intr_establish(device_t, int (*)(void *), void *, const char *);
void	sdmmc_intr_disestablish(void *);
void	sdmmc_intr_task(void *);

int	sdmmc_decode_csd(struct sdmmc_softc *, sdmmc_response,
	    struct sdmmc_function *);
int	sdmmc_decode_cid(struct sdmmc_softc *, sdmmc_response,
	    struct sdmmc_function *);
void	sdmmc_print_cid(struct sdmmc_cid *);
#ifdef SDMMC_DUMP_CSD
void	sdmmc_print_csd(sdmmc_response, struct sdmmc_csd *);
#endif
void	sdmmc_dump_data(const char *, void *, size_t);

int	sdmmc_io_enable(struct sdmmc_softc *);
void	sdmmc_io_scan(struct sdmmc_softc *);
int	sdmmc_io_init(struct sdmmc_softc *, struct sdmmc_function *);
uint8_t sdmmc_io_read_1(struct sdmmc_function *, int);
uint16_t sdmmc_io_read_2(struct sdmmc_function *, int);
uint32_t sdmmc_io_read_4(struct sdmmc_function *, int);
int	sdmmc_io_read_multi_1(struct sdmmc_function *, int, u_char *, int);
void	sdmmc_io_write_1(struct sdmmc_function *, int, uint8_t);
void	sdmmc_io_write_2(struct sdmmc_function *, int, uint16_t);
void	sdmmc_io_write_4(struct sdmmc_function *, int, uint32_t);
int	sdmmc_io_write_multi_1(struct sdmmc_function *, int, u_char *, int);
int	sdmmc_io_function_enable(struct sdmmc_function *);
void	sdmmc_io_function_disable(struct sdmmc_function *);

int	sdmmc_read_cis(struct sdmmc_function *, struct sdmmc_cis *);
void	sdmmc_print_cis(struct sdmmc_function *);
void	sdmmc_check_cis_quirks(struct sdmmc_function *);

int	sdmmc_mem_enable(struct sdmmc_softc *);
void	sdmmc_mem_scan(struct sdmmc_softc *);
int	sdmmc_mem_init(struct sdmmc_softc *, struct sdmmc_function *);
int	sdmmc_mem_send_op_cond(struct sdmmc_softc *, uint32_t, uint32_t *);
int	sdmmc_mem_send_if_cond(struct sdmmc_softc *, uint32_t, uint32_t *);
int	sdmmc_mem_set_blocklen(struct sdmmc_softc *, struct sdmmc_function *,
	    int);
int	sdmmc_mem_read_block(struct sdmmc_function *, uint32_t, u_char *,
	    size_t);
int	sdmmc_mem_write_block(struct sdmmc_function *, uint32_t, u_char *,
	    size_t);

#endif	/* _SDMMCVAR_H_ */

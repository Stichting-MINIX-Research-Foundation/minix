/*	$NetBSD: rtsx.c,v 1.2 2014/10/29 14:24:09 nonaka Exp $	*/
/*	$OpenBSD: rtsx.c,v 1.10 2014/08/19 17:55:03 phessler Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2012 Stefan Sperling <stsp@openbsd.org>
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

/*
 * Realtek RTS5209/RTS5227/RTS5229/RTL8402/RTL8411/RTL8411B Card Reader driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rtsx.c,v 1.2 2014/10/29 14:24:09 nonaka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <dev/ic/rtsxreg.h>
#include <dev/ic/rtsxvar.h>

#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

/* 
 * We use two DMA buffers, a command buffer and a data buffer.
 *
 * The command buffer contains a command queue for the host controller,
 * which describes SD/MMC commands to run, and other parameters. The chip
 * runs the command queue when a special bit in the RTSX_HCBAR register is set
 * and signals completion with the TRANS_OK interrupt.
 * Each command is encoded as a 4 byte sequence containing command number
 * (read, write, or check a host controller register), a register address,
 * and a data bit-mask and value.
 *
 * The data buffer is used to transfer data sectors to or from the SD card.
 * Data transfer is controlled via the RTSX_HDBAR register. Completion is
 * also signalled by the TRANS_OK interrupt.
 *
 * The chip is unable to perform DMA above 4GB.
 *
 * SD/MMC commands which do not transfer any data from/to the card only use
 * the command buffer.
 */

#define RTSX_DMA_MAX_SEGSIZE	0x80000
#define RTSX_HOSTCMD_MAX	256
#define RTSX_HOSTCMD_BUFSIZE	(sizeof(uint32_t) * RTSX_HOSTCMD_MAX)
#define RTSX_DMA_DATA_BUFSIZE	MAXPHYS

#define READ4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define WRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

#define RTSX_READ(sc, reg, val) 				\
	do {							\
		int err = rtsx_read((sc), (reg), (val)); 	\
		if (err) 					\
			return err;				\
	} while (/*CONSTCOND*/0)

#define RTSX_WRITE(sc, reg, val) 				\
	do {							\
		int err = rtsx_write((sc), (reg), 0xff, (val));	\
		if (err) 					\
			return err;				\
	} while (/*CONSTCOND*/0)

#define RTSX_CLR(sc, reg, bits)					\
	do {							\
		int err = rtsx_write((sc), (reg), (bits), 0); 	\
		if (err) 					\
			return err;				\
	} while (/*CONSTCOND*/0)

#define RTSX_SET(sc, reg, bits)					\
	do {							\
		int err = rtsx_write((sc), (reg), (bits), 0xff);\
		if (err) 					\
			return err;				\
	} while (/*CONSTCOND*/0)

#define RTSX_BITOP(sc, reg, mask, bits)				\
	do {							\
		int err = rtsx_write((sc), (reg), (mask), (bits));\
		if (err) 					\
			return err;				\
	} while (/*CONSTCOND*/0)

static int	rtsx_host_reset(sdmmc_chipset_handle_t);
static uint32_t	rtsx_host_ocr(sdmmc_chipset_handle_t);
static int	rtsx_host_maxblklen(sdmmc_chipset_handle_t);
static int	rtsx_card_detect(sdmmc_chipset_handle_t);
static int	rtsx_write_protect(sdmmc_chipset_handle_t);
static int	rtsx_bus_power(sdmmc_chipset_handle_t, uint32_t);
static int	rtsx_bus_clock(sdmmc_chipset_handle_t, int);
static int	rtsx_bus_width(sdmmc_chipset_handle_t, int);
static int	rtsx_bus_rod(sdmmc_chipset_handle_t, int);
static void	rtsx_exec_command(sdmmc_chipset_handle_t,
		    struct sdmmc_command *);
static int	rtsx_init(struct rtsx_softc *, int);
static void	rtsx_soft_reset(struct rtsx_softc *);
static int	rtsx_bus_power_off(struct rtsx_softc *);
static int	rtsx_bus_power_on(struct rtsx_softc *);
static int	rtsx_set_bus_width(struct rtsx_softc *, int);
static int	rtsx_stop_sd_clock(struct rtsx_softc *);
static int	rtsx_switch_sd_clock(struct rtsx_softc *, uint8_t, int, int);
static int	rtsx_wait_intr(struct rtsx_softc *, int, int);
static int	rtsx_read(struct rtsx_softc *, uint16_t, uint8_t *);
static int	rtsx_write(struct rtsx_softc *, uint16_t, uint8_t, uint8_t);
#ifdef notyet
static int	rtsx_read_phy(struct rtsx_softc *, uint8_t, uint16_t *);
#endif
static int	rtsx_write_phy(struct rtsx_softc *, uint8_t, uint16_t);
static int	rtsx_read_cfg(struct rtsx_softc *, uint8_t, uint16_t,
		    uint32_t *);
#ifdef notyet
static int	rtsx_write_cfg(struct rtsx_softc *, uint8_t, uint16_t, uint32_t,
		    uint32_t);
#endif
static void	rtsx_hostcmd(uint32_t *, int *, uint8_t, uint16_t, uint8_t,
		    uint8_t);
static int	rtsx_hostcmd_send(struct rtsx_softc *, int);
static uint8_t	rtsx_response_type(uint16_t);
static int	rtsx_read_ppbuf(struct rtsx_softc *, struct sdmmc_command *,
		    uint32_t *);
static int	rtsx_write_ppbuf(struct rtsx_softc *, struct sdmmc_command *,
		    uint32_t *);
static int	rtsx_exec_short_xfer(struct rtsx_softc *,
		    struct sdmmc_command *, uint32_t *, uint8_t);
static int	rtsx_xfer(struct rtsx_softc *, struct sdmmc_command *,
		    uint32_t *);
static void	rtsx_card_insert(struct rtsx_softc *);
static void	rtsx_card_eject(struct rtsx_softc *);
static int	rtsx_led_enable(struct rtsx_softc *);
static int	rtsx_led_disable(struct rtsx_softc *);
static void	rtsx_save_regs(struct rtsx_softc *);
static void	rtsx_restore_regs(struct rtsx_softc *);

#ifdef RTSX_DEBUG
int rtsxdebug = 0;
#define DPRINTF(n,s)	do { if ((n) <= rtsxdebug) printf s; } while (0)
#else
#define DPRINTF(n,s)	/**/
#endif

#define	DEVNAME(sc)	SDMMCDEVNAME(sc)

static struct sdmmc_chip_functions rtsx_chip_functions = {
	/* host controller reset */
	.host_reset = rtsx_host_reset,

	/* host controller capabilities */
	.host_ocr = rtsx_host_ocr,
	.host_maxblklen = rtsx_host_maxblklen,

	/* card detection */
	.card_detect = rtsx_card_detect,

	/* write protect */
	.write_protect = rtsx_write_protect,

	/* bus power, clock frequency, width and ROD(OpenDrain/PushPull) */
	.bus_power = rtsx_bus_power,
	.bus_clock = rtsx_bus_clock,
	.bus_width = rtsx_bus_width,
	.bus_rod = rtsx_bus_rod,

	/* command execution */
	.exec_command = rtsx_exec_command,

	/* card interrupt */
	.card_enable_intr = NULL,
	.card_intr_ack = NULL,
};

/*
 * Called by attachment driver.
 */
int
rtsx_attach(struct rtsx_softc *sc, bus_space_tag_t iot,
    bus_space_handle_t ioh, bus_size_t iosize, bus_dma_tag_t dmat, int flags)
{
	struct sdmmcbus_attach_args saa;
	uint32_t sdio_cfg;

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_iosize = iosize;
	sc->sc_dmat = dmat;
	sc->sc_flags = flags;

	mutex_init(&sc->sc_host_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	mutex_init(&sc->sc_intr_mtx, MUTEX_DEFAULT, IPL_SDMMC);
	cv_init(&sc->sc_intr_cv, "rtsxintr");

	if (rtsx_init(sc, 1))
		goto error;

	if (rtsx_read_cfg(sc, 0, RTSX_SDIOCFG_REG, &sdio_cfg) == 0) {
		if (sdio_cfg & (RTSX_SDIOCFG_SDIO_ONLY|RTSX_SDIOCFG_HAVE_SDIO)){
			sc->sc_flags |= RTSX_F_SDIO_SUPPORT;
		}
	}

	if (bus_dmamap_create(sc->sc_dmat, RTSX_HOSTCMD_BUFSIZE, 1,
	    RTSX_DMA_MAX_SEGSIZE, 0, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW,
	    &sc->sc_dmap_cmd) != 0)
		goto error;

	/*
	 * Attach the generic SD/MMC bus driver.  (The bus driver must
	 * not invoke any chipset functions before it is attached.)
	 */
	memset(&saa, 0, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.saa_sct = &rtsx_chip_functions;
	saa.saa_spi_sct = NULL;
	saa.saa_sch = sc;
	saa.saa_dmat = sc->sc_dmat;
	saa.saa_clkmin = SDMMC_SDCLK_400K;
	saa.saa_clkmax = 25000;
	saa.saa_caps = SMC_CAPS_DMA|SMC_CAPS_4BIT_MODE;

	sc->sc_sdmmc = config_found(sc->sc_dev, &saa, NULL);
	if (sc->sc_sdmmc == NULL)
		goto destroy_dmamap_cmd;

	/* Now handle cards discovered during attachment. */
	if (ISSET(sc->sc_flags, RTSX_F_CARD_PRESENT))
		rtsx_card_insert(sc);

	return 0;

destroy_dmamap_cmd:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap_cmd);
error:
	cv_destroy(&sc->sc_intr_cv);
	mutex_destroy(&sc->sc_intr_mtx);
	mutex_destroy(&sc->sc_host_mtx);
	return 1;
}

int
rtsx_detach(struct rtsx_softc *sc, int flags)
{
	int rv;

	if (sc->sc_sdmmc != NULL) {
		rv = config_detach(sc->sc_sdmmc, flags);
		if (rv != 0)
			return rv;
		sc->sc_sdmmc = NULL;
	}

	/* disable interrupts */
	if ((flags & DETACH_FORCE) == 0) {
		WRITE4(sc, RTSX_BIER, 0);
		rtsx_soft_reset(sc);
	}

	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmap_cmd);
	cv_destroy(&sc->sc_intr_cv);
	mutex_destroy(&sc->sc_intr_mtx);
	mutex_destroy(&sc->sc_host_mtx);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);

	return 0;
}

bool
rtsx_suspend(device_t dev, const pmf_qual_t *qual)
{
	struct rtsx_softc *sc = device_private(dev);

	/* Save the host controller state. */
	rtsx_save_regs(sc);

	return true;
}

bool
rtsx_resume(device_t dev, const pmf_qual_t *qual)
{
	struct rtsx_softc *sc = device_private(dev);

	/* Restore the host controller state. */
	rtsx_restore_regs(sc);

	if (READ4(sc, RTSX_BIPR) & RTSX_SD_EXIST)
		rtsx_card_insert(sc);
	else
		rtsx_card_eject(sc);

	return true;
}

bool
rtsx_shutdown(device_t dev, int flags)
{
	struct rtsx_softc *sc = device_private(dev);

	/* XXX chip locks up if we don't disable it before reboot. */
	(void)rtsx_host_reset(sc);

	return true;
}

static int
rtsx_init(struct rtsx_softc *sc, int attaching)
{
	uint32_t status;
	uint8_t reg;
	int error;

	if (attaching) {
		if (RTSX_IS_RTS5229(sc)) {
			/* Read IC version from dummy register. */
			RTSX_READ(sc, RTSX_DUMMY_REG, &reg);
			switch (reg & 0x0f) {
			case RTSX_IC_VERSION_A:
			case RTSX_IC_VERSION_B:
			case RTSX_IC_VERSION_D:
				break;
			case RTSX_IC_VERSION_C:
				sc->sc_flags |= RTSX_F_5229_TYPE_C;
				break;
			default:
				aprint_error_dev(sc->sc_dev,
				    "unknown RTS5229 version 0x%02x\n", reg);
				return 1;
			}
		} else if (RTSX_IS_RTL8411B(sc)) {
			RTSX_READ(sc, RTSX_RTL8411B_PACKAGE, &reg);
			if (reg & RTSX_RTL8411B_QFN48)
				sc->sc_flags |= RTSX_F_8411B_QFN48;
		}
	}

	/* Enable interrupt write-clear (default is read-clear). */
	RTSX_CLR(sc, RTSX_NFTS_TX_CTRL, RTSX_INT_READ_CLR);

	/* Clear any pending interrupts. */
	status = READ4(sc, RTSX_BIPR);
	WRITE4(sc, RTSX_BIPR, status);

	/* Check for cards already inserted at attach time. */
	if (attaching && (status & RTSX_SD_EXIST))
		sc->sc_flags |= RTSX_F_CARD_PRESENT;

	/* Enable interrupts. */
	WRITE4(sc, RTSX_BIER,
	    RTSX_TRANS_OK_INT_EN | RTSX_TRANS_FAIL_INT_EN | RTSX_SD_INT_EN);

	/* Power on SSC clock. */
	RTSX_CLR(sc, RTSX_FPDCTL, RTSX_SSC_POWER_DOWN);
	delay(200);

	/* XXX magic numbers from linux driver */
	if (RTSX_IS_RTS5209(sc))
		error = rtsx_write_phy(sc, 0x00, 0xB966);
	else if (RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc))
		error = rtsx_write_phy(sc, 0x00, 0xBA42);
	else
		error = 0;
	if (error) {
		aprint_error_dev(sc->sc_dev, "couldn't write phy register\n");
		return 1;
	}

	RTSX_SET(sc, RTSX_CLK_DIV, 0x07);

	/* Disable sleep mode. */
	RTSX_CLR(sc, RTSX_HOST_SLEEP_STATE,
	    RTSX_HOST_ENTER_S1 | RTSX_HOST_ENTER_S3);

	/* Disable card clock. */
	RTSX_CLR(sc, RTSX_CARD_CLK_EN, RTSX_CARD_CLK_EN_ALL);

	RTSX_CLR(sc, RTSX_CHANGE_LINK_STATE,
	    RTSX_FORCE_RST_CORE_EN | RTSX_NON_STICKY_RST_N_DBG | 0x04);
	RTSX_WRITE(sc, RTSX_SD30_DRIVE_SEL, RTSX_SD30_DRIVE_SEL_3V3);

	/* Enable SSC clock. */
	RTSX_WRITE(sc, RTSX_SSC_CTL1, RTSX_SSC_8X_EN | RTSX_SSC_SEL_4M);
	RTSX_WRITE(sc, RTSX_SSC_CTL2, 0x12);

	RTSX_SET(sc, RTSX_CHANGE_LINK_STATE, RTSX_MAC_PHY_RST_N_DBG);
	RTSX_SET(sc, RTSX_IRQSTAT0, RTSX_LINK_READY_INT);

	RTSX_WRITE(sc, RTSX_PERST_GLITCH_WIDTH, 0x80);

	/* Set RC oscillator to 400K. */
	RTSX_CLR(sc, RTSX_RCCTL, RTSX_RCCTL_F_2M);

	/* Request clock by driving CLKREQ pin to zero. */
	RTSX_SET(sc, RTSX_PETXCFG, RTSX_PETXCFG_CLKREQ_PIN);

	/* Set up LED GPIO. */
	if (RTSX_IS_RTS5209(sc)) {
		RTSX_WRITE(sc, RTSX_CARD_GPIO, 0x03);
		RTSX_WRITE(sc, RTSX_CARD_GPIO_DIR, 0x03);
	} else if (RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_SET(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON);
		/* Switch LDO3318 source from DV33 to 3V3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_SET(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_SET(sc, RTSX_OLT_LED_CTL, RTSX_OLT_LED_PERIOD);
	} else if (RTSX_IS_RTL8402(sc)
	           || RTSX_IS_RTL8411(sc)
	           || RTSX_IS_RTL8411B(sc)) {
		if (RTSX_IS_RTL8411B_QFN48(sc))
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf5);
		/* Enable SD interrupt */
		RTSX_WRITE(sc, RTSX_CARD_PAD_CTL, 0x05);
		RTSX_BITOP(sc, RTSX_EFUSE_CONTENT, 0xe0, 0x80);
		if (RTSX_IS_RTL8411B(sc))
			RTSX_WRITE(sc, RTSX_FUNC_FORCE_CTL, 0x00);
	}

	return 0;
}

int
rtsx_led_enable(struct rtsx_softc *sc)
{

	if (RTSX_IS_RTS5209(sc)) {
		RTSX_CLR(sc, RTSX_CARD_GPIO, RTSX_CARD_GPIO_LED_OFF);
		RTSX_WRITE(sc, RTSX_CARD_AUTO_BLINK,
		    RTSX_LED_BLINK_EN | RTSX_LED_BLINK_SPEED);
	} else if (RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_SET(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON);
		RTSX_SET(sc, RTSX_OLT_LED_CTL, RTSX_OLT_LED_AUTOBLINK);
	} else if (RTSX_IS_RTL8402(sc)
	           || RTSX_IS_RTL8411(sc)
	           || RTSX_IS_RTL8411B(sc)) {
		RTSX_CLR(sc, RTSX_GPIO_CTL, 0x01);
		RTSX_WRITE(sc, RTSX_CARD_AUTO_BLINK,
		    RTSX_LED_BLINK_EN | RTSX_LED_BLINK_SPEED);
	}

	return 0;
}

int
rtsx_led_disable(struct rtsx_softc *sc)
{

	if (RTSX_IS_RTS5209(sc)) {
		RTSX_CLR(sc, RTSX_CARD_AUTO_BLINK, RTSX_LED_BLINK_EN);
		RTSX_WRITE(sc, RTSX_CARD_GPIO, RTSX_CARD_GPIO_LED_OFF);
	} else if (RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_CLR(sc, RTSX_OLT_LED_CTL, RTSX_OLT_LED_AUTOBLINK);
		RTSX_CLR(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON);
	} else if (RTSX_IS_RTL8402(sc)
	           || RTSX_IS_RTL8411(sc)
	           || RTSX_IS_RTL8411B(sc)) {
		RTSX_CLR(sc, RTSX_CARD_AUTO_BLINK, RTSX_LED_BLINK_EN);
		RTSX_SET(sc, RTSX_GPIO_CTL, 0x01);
	}

	return 0;
}

/*
 * Reset the host controller.  Called during initialization, when
 * cards are removed, upon resume, and during error recovery.
 */
int
rtsx_host_reset(sdmmc_chipset_handle_t sch)
{
	struct rtsx_softc *sc = sch;
	int error;

	DPRINTF(1,("%s: host reset\n", DEVNAME(sc)));

	mutex_enter(&sc->sc_host_mtx);

	if (ISSET(sc->sc_flags, RTSX_F_CARD_PRESENT))
		rtsx_soft_reset(sc);

	error = rtsx_init(sc, 0);

	mutex_exit(&sc->sc_host_mtx);

	return error;
}

static uint32_t
rtsx_host_ocr(sdmmc_chipset_handle_t sch)
{

	return RTSX_SUPPORT_VOLTAGE;
}

static int
rtsx_host_maxblklen(sdmmc_chipset_handle_t sch)
{

	return 512;
}

/*
 * Return non-zero if the card is currently inserted.
 */
static int
rtsx_card_detect(sdmmc_chipset_handle_t sch)
{
	struct rtsx_softc *sc = sch;

	return ISSET(sc->sc_flags, RTSX_F_CARD_PRESENT);
}

static int
rtsx_write_protect(sdmmc_chipset_handle_t sch)
{

	return 0; /* XXX */
}

/*
 * Notice that the meaning of RTSX_PWR_GATE_CTRL changes between RTS5209 and
 * RTS5229. In RTS5209 it is a mask of disabled power gates, while in RTS5229
 * it is a mask of *enabled* gates.
 */

static int
rtsx_bus_power_off(struct rtsx_softc *sc)
{
	int error;
	uint8_t disable3;

	error = rtsx_stop_sd_clock(sc);
	if (error)
		return error;

	/* Disable SD output. */
	RTSX_CLR(sc, RTSX_CARD_OE, RTSX_CARD_OUTPUT_EN);

	/* Turn off power. */
	disable3 = RTSX_PULL_CTL_DISABLE3;
	if (RTSX_IS_RTS5209(sc))
		RTSX_SET(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_OFF);
	else if (RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_CLR(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_VCC1 |
		    RTSX_LDO3318_VCC2);
		if (RTSX_IS_RTS5229_TYPE_C(sc))
			disable3 = RTSX_PULL_CTL_DISABLE3_TYPE_C;
	} else if (RTSX_IS_RTL8402(sc)
	           || RTSX_IS_RTL8411(sc)
	           || RTSX_IS_RTL8411B(sc)) {
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
		    RTSX_BPP_POWER_OFF);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
		    RTSX_BPP_LDO_SUSPEND);
	}

	RTSX_SET(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_OFF);
	RTSX_CLR(sc, RTSX_CARD_PWR_CTL, RTSX_PMOS_STRG_800mA);

	/* Disable pull control. */
	if (RTSX_IS_RTS5209(sc) || RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, disable3);
	} else if (RTSX_IS_RTL8402(sc) || RTSX_IS_RTL8411(sc)) {
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x65);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x65);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0x95);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x05);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x04);
	} else if (RTSX_IS_RTL8411B(sc)) {
		if (RTSX_IS_RTL8411B_QFN48(sc)) {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf5);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		} else {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x65);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xd9);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x59);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		}
	}

	return 0;
}

static int
rtsx_bus_power_on(struct rtsx_softc *sc)
{
	uint8_t enable3;

	/* Select SD card. */
	RTSX_WRITE(sc, RTSX_CARD_SELECT, RTSX_SD_MOD_SEL);
	RTSX_WRITE(sc, RTSX_CARD_SHARE_MODE, RTSX_CARD_SHARE_48_SD);
	RTSX_SET(sc, RTSX_CARD_CLK_EN, RTSX_SD_CLK_EN);

	/* Enable pull control. */
	if (RTSX_IS_RTS5209(sc) || RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, RTSX_PULL_CTL_ENABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_ENABLE12);
		if (RTSX_IS_RTS5229_TYPE_C(sc))
			enable3 = RTSX_PULL_CTL_ENABLE3_TYPE_C;
		else
			enable3 = RTSX_PULL_CTL_ENABLE3;
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, enable3);
	} else if (RTSX_IS_RTL8402(sc) || RTSX_IS_RTL8411(sc)) {
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0xaa);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xa9);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x04);
	} else if (RTSX_IS_RTL8411B(sc)) {
		if (RTSX_IS_RTL8411B_QFN48(sc)) {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf9);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x19);
		} else {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xd9);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x59);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x59);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		}
	}

	/*
	 * To avoid a current peak, enable card power in two phases with a
	 * delay in between.
	 */

	if (RTSX_IS_RTS5209(sc) || RTSX_IS_RTS5227(sc) || RTSX_IS_RTS5229(sc)) {
		/* Partial power. */
		RTSX_SET(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PARTIAL_PWR_ON);
		if (RTSX_IS_RTS5209(sc))
			RTSX_SET(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_SUSPEND);
		else
			RTSX_SET(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_VCC1);

		delay(200);

		/* Full power. */
		RTSX_CLR(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_OFF);
		if (RTSX_IS_RTS5209(sc))
			RTSX_CLR(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_OFF);
		else
			RTSX_SET(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_VCC2);
	} else if (RTSX_IS_RTL8402(sc)
	           || RTSX_IS_RTL8411(sc)
	           || RTSX_IS_RTL8411B(sc)) {
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
		    RTSX_BPP_POWER_5_PERCENT_ON);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
		    RTSX_BPP_LDO_SUSPEND);
		delay(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
		    RTSX_BPP_POWER_10_PERCENT_ON);
		delay(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
		    RTSX_BPP_POWER_15_PERCENT_ON);
		delay(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
		    RTSX_BPP_POWER_ON);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
		    RTSX_BPP_LDO_ON);
	}

	/* Enable SD card output. */
	RTSX_WRITE(sc, RTSX_CARD_OE, RTSX_SD_OUTPUT_EN);

	return 0;
}

static int
rtsx_set_bus_width(struct rtsx_softc *sc, int width)
{
	uint32_t bus_width;

	DPRINTF(1,("%s: bus width=%d\n", DEVNAME(sc), width));

	switch (width) {
	case 8:
		bus_width = RTSX_BUS_WIDTH_8;
		break;
	case 4:
		bus_width = RTSX_BUS_WIDTH_4;
		break;
	case 1:
		bus_width = RTSX_BUS_WIDTH_1;
		break;
	default:
		return EINVAL;
	}

	if (bus_width == RTSX_BUS_WIDTH_1)
		RTSX_CLR(sc, RTSX_SD_CFG1, RTSX_BUS_WIDTH_MASK);
	else
		RTSX_SET(sc, RTSX_SD_CFG1, bus_width);

	return 0;
}

static int
rtsx_stop_sd_clock(struct rtsx_softc *sc)
{

	RTSX_CLR(sc, RTSX_CARD_CLK_EN, RTSX_CARD_CLK_EN_ALL);
	RTSX_SET(sc, RTSX_SD_BUS_STAT, RTSX_SD_CLK_FORCE_STOP);

	return 0;
}

static int
rtsx_switch_sd_clock(struct rtsx_softc *sc, uint8_t n, int div, int mcu)
{

	/* Enable SD 2.0 mode. */
	RTSX_CLR(sc, RTSX_SD_CFG1, RTSX_SD_MODE_MASK);

	RTSX_SET(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

	RTSX_WRITE(sc, RTSX_CARD_CLK_SOURCE,
	    RTSX_CRC_FIX_CLK | RTSX_SD30_VAR_CLK0 | RTSX_SAMPLE_VAR_CLK1);
	RTSX_CLR(sc, RTSX_SD_SAMPLE_POINT_CTL, RTSX_SD20_RX_SEL_MASK);
	RTSX_WRITE(sc, RTSX_SD_PUSH_POINT_CTL, RTSX_SD20_TX_NEG_EDGE);
	RTSX_WRITE(sc, RTSX_CLK_DIV, (div << 4) | mcu);
	RTSX_CLR(sc, RTSX_SSC_CTL1, RTSX_RSTB);
	RTSX_CLR(sc, RTSX_SSC_CTL2, RTSX_SSC_DEPTH_MASK);
	RTSX_WRITE(sc, RTSX_SSC_DIV_N_0, n);
	RTSX_SET(sc, RTSX_SSC_CTL1, RTSX_RSTB);
	delay(100);

	RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

	return 0;
}

/*
 * Set or change SD bus voltage and enable or disable SD bus power.
 * Return zero on success.
 */
static int
rtsx_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	struct rtsx_softc *sc = sch;
	int error = 0;

	DPRINTF(1,("%s: voltage change ocr=0x%x\n", DEVNAME(sc), ocr));

	mutex_enter(&sc->sc_host_mtx);

	/*
	 * Disable bus power before voltage change.
	 */
	error = rtsx_bus_power_off(sc);
	if (error)
		goto ret;

	delay(200);

	/* If power is disabled, reset the host and return now. */
	if (ocr == 0) {
		mutex_exit(&sc->sc_host_mtx);
		(void)rtsx_host_reset(sc);
		return 0;
	}

	if (!ISSET(ocr, RTSX_SUPPORT_VOLTAGE)) {
		/* Unsupported voltage level requested. */
		DPRINTF(1,("%s: unsupported voltage ocr=0x%x\n",
		    DEVNAME(sc), ocr));
		error = EINVAL;
		goto ret;
	}

	error = rtsx_set_bus_width(sc, 1);
	if (error)
		goto ret;

	error = rtsx_bus_power_on(sc);
ret:
	mutex_exit(&sc->sc_host_mtx);

	return error;
}

/*
 * Set or change SDCLK frequency or disable the SD clock.
 * Return zero on success.
 */
static int
rtsx_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct rtsx_softc *sc = sch;
	uint8_t n;
	int div;
	int mcu;
	int error = 0;

	DPRINTF(1,("%s: bus clock change freq=%d\n", DEVNAME(sc), freq));

	mutex_enter(&sc->sc_host_mtx);

	if (freq == SDMMC_SDCLK_OFF) {
		error = rtsx_stop_sd_clock(sc);
		goto ret;
	}

	/*
	 * Configure the clock frequency.
	 */
	switch (freq) {
	case SDMMC_SDCLK_400K:
		n = 80; /* minimum */
		div = RTSX_CLK_DIV_8;
		mcu = 7;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_128, 0xff);
		if (error)
			goto ret;
		break;
	case 20000:
		n = 80;
		div = RTSX_CLK_DIV_4;
		mcu = 7;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, 0);
		if (error)
			goto ret;
		break;
	case 25000:
		n = 100;
		div = RTSX_CLK_DIV_4;
		mcu = 7;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, 0);
		if (error)
			goto ret;
		break;
	case 30000:
		n = 120;
		div = RTSX_CLK_DIV_4;
		mcu = 7;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, 0);
		if (error)
			goto ret;
		break;
	case 40000:
		n = 80;
		div = RTSX_CLK_DIV_2;
		mcu = 7;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, 0);
		if (error)
			goto ret;
		break;
	case 50000:
		n = 100;
		div = RTSX_CLK_DIV_2;
		mcu = 6;
		error = rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, 0);
		if (error)
			goto ret;
		break;
	default:
		error = EINVAL;
		goto ret;
	}

	/*
	 * Enable SD clock.
	 */
	error = rtsx_switch_sd_clock(sc, n, div, mcu);
ret:
	mutex_exit(&sc->sc_host_mtx);

	return error;
}

static int
rtsx_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct rtsx_softc *sc = sch;

	return rtsx_set_bus_width(sc, width);
}

static int
rtsx_bus_rod(sdmmc_chipset_handle_t sch, int on)
{

	/* Not support */
	return -1;
}

static int
rtsx_read(struct rtsx_softc *sc, uint16_t addr, uint8_t *val)
{
	int tries = 1024;
	uint32_t reg;

	WRITE4(sc, RTSX_HAIMR, RTSX_HAIMR_BUSY |
	    (uint32_t)((addr & 0x3FFF) << 16));

	while (tries--) {
		reg = READ4(sc, RTSX_HAIMR);
		if (!(reg & RTSX_HAIMR_BUSY))
			break;
	}

	*val = (reg & 0xff);
	return (tries == 0) ? ETIMEDOUT : 0;
}

static int
rtsx_write(struct rtsx_softc *sc, uint16_t addr, uint8_t mask, uint8_t val)
{
	int tries = 1024;
	uint32_t reg;

	WRITE4(sc, RTSX_HAIMR,
	    RTSX_HAIMR_BUSY | RTSX_HAIMR_WRITE |
	    (uint32_t)(((addr & 0x3FFF) << 16) |
	    (mask << 8) | val));

	while (tries--) {
		reg = READ4(sc, RTSX_HAIMR);
		if (!(reg & RTSX_HAIMR_BUSY)) {
			if (val != (reg & 0xff))
				return EIO;
			return 0;
		}
	}
	return ETIMEDOUT;
}

#ifdef notyet
static int
rtsx_read_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t *val)
{
	int timeout = 100000;
	uint8_t data0;
	uint8_t data1;
	uint8_t rwctl;

	RTSX_WRITE(sc, RTSX_PHY_ADDR, addr);
	RTSX_WRITE(sc, RTSX_PHY_RWCTL, RTSX_PHY_BUSY|RTSX_PHY_READ);

	while (timeout--) {
		RTSX_READ(sc, RTSX_PHY_RWCTL, &rwctl);
		if (!(rwctl & RTSX_PHY_BUSY))
			break;
	}
	if (timeout == 0)
		return ETIMEDOUT;

	RTSX_READ(sc, RTSX_PHY_DATA0, &data0);
	RTSX_READ(sc, RTSX_PHY_DATA1, &data1);
	*val = data0 | (data1 << 8);

	return 0;
}
#endif

static int
rtsx_write_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t val)
{
	int timeout = 100000;
	uint8_t rwctl;

	RTSX_WRITE(sc, RTSX_PHY_DATA0, val);
	RTSX_WRITE(sc, RTSX_PHY_DATA1, val >> 8);
	RTSX_WRITE(sc, RTSX_PHY_ADDR, addr);
	RTSX_WRITE(sc, RTSX_PHY_RWCTL, RTSX_PHY_BUSY|RTSX_PHY_WRITE);

	while (timeout--) {
		RTSX_READ(sc, RTSX_PHY_RWCTL, &rwctl);
		if (!(rwctl & RTSX_PHY_BUSY))
			break;
	}
	if (timeout == 0)
		return ETIMEDOUT;

	return 0;
}

static int
rtsx_read_cfg(struct rtsx_softc *sc, uint8_t func, uint16_t addr, uint32_t *val)
{
	int tries = 1024;
	uint8_t data0, data1, data2, data3, rwctl;

	RTSX_WRITE(sc, RTSX_CFGADDR0, addr);
	RTSX_WRITE(sc, RTSX_CFGADDR1, addr >> 8);
	RTSX_WRITE(sc, RTSX_CFGRWCTL, RTSX_CFG_BUSY | (func & 0x03 << 4));

	while (tries--) {
		RTSX_READ(sc, RTSX_CFGRWCTL, &rwctl);
		if (!(rwctl & RTSX_CFG_BUSY))
			break;
	}
	if (tries == 0)
		return EIO;

	RTSX_READ(sc, RTSX_CFGDATA0, &data0);
	RTSX_READ(sc, RTSX_CFGDATA1, &data1);
	RTSX_READ(sc, RTSX_CFGDATA2, &data2);
	RTSX_READ(sc, RTSX_CFGDATA3, &data3);
	*val = (data3 << 24) | (data2 << 16) | (data1 << 8) | data0;

	return 0;
}

#ifdef notyet
static int
rtsx_write_cfg(struct rtsx_softc *sc, uint8_t func, uint16_t addr,
    uint32_t mask, uint32_t val)
{
	uint32_t writemask = 0;
	int i, tries = 1024;
	uint8_t rwctl;

	for (i = 0; i < 4; i++) {
		if (mask & 0xff) {
			RTSX_WRITE(sc, RTSX_CFGDATA0 + i, val & mask & 0xff);
			writemask |= (1 << i);
		}
		mask >>= 8;
		val >>= 8;
	}

	if (writemask) {
		RTSX_WRITE(sc, RTSX_CFGADDR0, addr);
		RTSX_WRITE(sc, RTSX_CFGADDR1, addr >> 8);
		RTSX_WRITE(sc, RTSX_CFGRWCTL,
		    RTSX_CFG_BUSY | writemask | (func & 0x03 << 4));
	}

	while (tries--) {
		RTSX_READ(sc, RTSX_CFGRWCTL, &rwctl);
		if (!(rwctl & RTSX_CFG_BUSY))
			break;
	}
	if (tries == 0)
		return EIO;

	return 0;
}
#endif

/* Append a properly encoded host command to the host command buffer. */
static void
rtsx_hostcmd(uint32_t *cmdbuf, int *n, uint8_t cmd, uint16_t reg,
    uint8_t mask, uint8_t data)
{

	KASSERT(*n < RTSX_HOSTCMD_MAX);

	cmdbuf[(*n)++] = htole32((uint32_t)(cmd & 0x3) << 30) |
	    ((uint32_t)(reg & 0x3fff) << 16) |
	    ((uint32_t)(mask) << 8) |
	    ((uint32_t)data);
}

static void
rtsx_save_regs(struct rtsx_softc *sc)
{
	int i;
	uint16_t reg;

	mutex_enter(&sc->sc_host_mtx);

	i = 0;
	for (reg = 0xFDA0; reg < 0xFDAE; reg++)
		(void)rtsx_read(sc, reg, &sc->sc_regs[i++]);
	for (reg = 0xFD52; reg < 0xFD69; reg++)
		(void)rtsx_read(sc, reg, &sc->sc_regs[i++]);
	for (reg = 0xFE20; reg < 0xFE34; reg++)
		(void)rtsx_read(sc, reg, &sc->sc_regs[i++]);

	sc->sc_regs4[0] = READ4(sc, RTSX_HCBAR);
	sc->sc_regs4[1] = READ4(sc, RTSX_HCBCTLR);
	sc->sc_regs4[2] = READ4(sc, RTSX_HDBAR);
	sc->sc_regs4[3] = READ4(sc, RTSX_HDBCTLR);
	sc->sc_regs4[4] = READ4(sc, RTSX_HAIMR);
	sc->sc_regs4[5] = READ4(sc, RTSX_BIER);
	/* Not saving RTSX_BIPR. */

	mutex_exit(&sc->sc_host_mtx);
}

static void
rtsx_restore_regs(struct rtsx_softc *sc)
{
	int i;
	uint16_t reg;

	mutex_enter(&sc->sc_host_mtx);

	WRITE4(sc, RTSX_HCBAR, sc->sc_regs4[0]);
	WRITE4(sc, RTSX_HCBCTLR, sc->sc_regs4[1]);
	WRITE4(sc, RTSX_HDBAR, sc->sc_regs4[2]);
	WRITE4(sc, RTSX_HDBCTLR, sc->sc_regs4[3]);
	WRITE4(sc, RTSX_HAIMR, sc->sc_regs4[4]);
	WRITE4(sc, RTSX_BIER, sc->sc_regs4[5]);
	/* Not writing RTSX_BIPR since doing so would clear it. */

	i = 0;
	for (reg = 0xFDA0; reg < 0xFDAE; reg++)
		(void)rtsx_write(sc, reg, 0xff, sc->sc_regs[i++]);
	for (reg = 0xFD52; reg < 0xFD69; reg++)
		(void)rtsx_write(sc, reg, 0xff, sc->sc_regs[i++]);
	for (reg = 0xFE20; reg < 0xFE34; reg++)
		(void)rtsx_write(sc, reg, 0xff, sc->sc_regs[i++]);

	mutex_exit(&sc->sc_host_mtx);
}

static uint8_t
rtsx_response_type(uint16_t sdmmc_rsp)
{
	static const struct rsp_type {
		uint16_t	sdmmc_rsp;
		uint8_t		rtsx_rsp;
	} rsp_types[] = {
		{ SCF_RSP_R0,	RTSX_SD_RSP_TYPE_R0 },
		{ SCF_RSP_R1,	RTSX_SD_RSP_TYPE_R1 },
		{ SCF_RSP_R1B,	RTSX_SD_RSP_TYPE_R1B },
		{ SCF_RSP_R2,	RTSX_SD_RSP_TYPE_R2 },
		{ SCF_RSP_R3,	RTSX_SD_RSP_TYPE_R3 },
		{ SCF_RSP_R4,	RTSX_SD_RSP_TYPE_R4 },
		{ SCF_RSP_R5,	RTSX_SD_RSP_TYPE_R5 },
		{ SCF_RSP_R6,	RTSX_SD_RSP_TYPE_R6 },
		{ SCF_RSP_R7,	RTSX_SD_RSP_TYPE_R7 }
	};
	size_t i;

	for (i = 0; i < __arraycount(rsp_types); i++) {
		if (sdmmc_rsp == rsp_types[i].sdmmc_rsp)
			return rsp_types[i].rtsx_rsp;
	}
	return 0;
}

static int
rtsx_hostcmd_send(struct rtsx_softc *sc, int ncmd)
{

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_cmd, 0, RTSX_HOSTCMD_BUFSIZE,
	    BUS_DMASYNC_PREWRITE);

	mutex_enter(&sc->sc_host_mtx);

	/* Tell the chip where the command buffer is and run the commands. */
	WRITE4(sc, RTSX_HCBAR, sc->sc_dmap_cmd->dm_segs[0].ds_addr);
	WRITE4(sc, RTSX_HCBCTLR,
	    ((ncmd * 4) & 0x00ffffff) | RTSX_START_CMD | RTSX_HW_AUTO_RSP);

	mutex_exit(&sc->sc_host_mtx);

	return 0;
}

static int
rtsx_read_ppbuf(struct rtsx_softc *sc, struct sdmmc_command *cmd,
    uint32_t *cmdbuf)
{
	uint8_t *ptr;
	int ncmd, remain;
	uint16_t reg;
	int error;
	int i, j;

	DPRINTF(3,("%s: read %d bytes from ppbuf2\n", DEVNAME(sc),
	    cmd->c_datalen));

	reg = RTSX_PPBUF_BASE2;
	ptr = cmd->c_data;
	remain = cmd->c_datalen;
	for (j = 0; j < cmd->c_datalen / RTSX_HOSTCMD_MAX; j++) {
		ncmd = 0;
		for (i = 0; i < RTSX_HOSTCMD_MAX; i++) {
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_READ_REG_CMD, reg++,
			    0, 0);
		}
		error = rtsx_hostcmd_send(sc, ncmd);
		if (error == 0)
			error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, hz / 4);
		if (error)
			goto ret;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_cmd, 0,
		    RTSX_HOSTCMD_BUFSIZE, BUS_DMASYNC_POSTREAD);
		memcpy(ptr, cmdbuf, RTSX_HOSTCMD_MAX);
		ptr += RTSX_HOSTCMD_MAX;
		remain -= RTSX_HOSTCMD_MAX;
	}
	if (remain > 0) {
		ncmd = 0;
		for (i = 0; i < remain; i++) {
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_READ_REG_CMD, reg++,
			    0, 0);
		}
		error = rtsx_hostcmd_send(sc, ncmd);
		if (error == 0)
			error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, hz / 4);
		if (error)
			goto ret;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_cmd, 0,
		    RTSX_HOSTCMD_BUFSIZE, BUS_DMASYNC_POSTREAD);
		memcpy(ptr, cmdbuf, remain);
	}
ret:
	return error;
}

static int
rtsx_write_ppbuf(struct rtsx_softc *sc, struct sdmmc_command *cmd,
    uint32_t *cmdbuf)
{
	const uint8_t *ptr;
	int ncmd, remain;
	uint16_t reg;
	int error;
	int i, j;

	DPRINTF(3,("%s: write %d bytes to ppbuf2\n", DEVNAME(sc),
	    cmd->c_datalen));

	reg = RTSX_PPBUF_BASE2;
	ptr = cmd->c_data;
	remain = cmd->c_datalen;
	for (j = 0; j < cmd->c_datalen / RTSX_HOSTCMD_MAX; j++) {
		ncmd = 0;
		for (i = 0; i < RTSX_HOSTCMD_MAX; i++) {
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, reg++,
			    0xff, *ptr++);
		}
		error = rtsx_hostcmd_send(sc, ncmd);
		if (error == 0)
			error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, hz / 4);
		if (error)
			goto ret;
		remain -= RTSX_HOSTCMD_MAX;
	}
	if (remain > 0) {
		ncmd = 0;
		for (i = 0; i < remain; i++) {
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, reg++,
			    0xff, *ptr++);
		}
		error = rtsx_hostcmd_send(sc, ncmd);
		if (error == 0)
			error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, hz / 4);
		if (error)
			goto ret;
	}
ret:
	return error;
}

static int
rtsx_exec_short_xfer(struct rtsx_softc *sc, struct sdmmc_command *cmd,
    uint32_t *cmdbuf, uint8_t rsp_type)
{
	int read = ISSET(cmd->c_flags, SCF_CMD_READ);
	int ncmd;
	uint8_t tmode = read ? RTSX_TM_NORMAL_READ : RTSX_TM_AUTO_WRITE2;
	int error;

	DPRINTF(3,("%s: %s short xfer: %d bytes with block size %d\n",
	    DEVNAME(sc), read ? "read" : "write", cmd->c_datalen,
	    cmd->c_blklen));

	if (cmd->c_datalen > 512) {
		DPRINTF(3, ("%s: cmd->c_datalen too large: %d > %d\n",
		    DEVNAME(sc), cmd->c_datalen, 512));
		return ENOMEM;
	}

	if (!read && cmd->c_data != NULL && cmd->c_datalen > 0) {
		error = rtsx_write_ppbuf(sc, cmd, cmdbuf);
		if (error)
			goto ret;
	}

	/* The command buffer queues commands the host controller will
	 * run asynchronously. */
	ncmd = 0;

	/* Queue commands to set SD command index and argument. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD0,
	    0xff, 0x40 | cmd->c_opcode); 
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD1,
	    0xff, cmd->c_arg >> 24);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD2,
	    0xff, cmd->c_arg >> 16);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD3,
	    0xff, cmd->c_arg >> 8);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD4,
	    0xff, cmd->c_arg);

	/* Queue commands to configure data transfer size. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_L,
	    0xff, cmd->c_datalen);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_H,
	    0xff, cmd->c_datalen >> 8);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_L,
	    0xff, 0x01);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_H,
	    0xff, 0x00);

	/* Queue command to set response type. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2,
	    0xff, rsp_type);

	if (tmode == RTSX_TM_NORMAL_READ) {
		rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD,
		    RTSX_CARD_DATA_SOURCE, 0x01, RTSX_PINGPONG_BUFFER);
	}

	/* Queue commands to perform SD transfer. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
	    0xff, tmode | RTSX_SD_TRANSFER_START);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
	    RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

	/* Run the command queue and wait for completion. */
	error = rtsx_hostcmd_send(sc, ncmd);
	if (error == 0)
		error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, 2 * hz);
	if (error)
		goto ret;

	if (read && cmd->c_data != NULL && cmd->c_datalen > 0)
		error = rtsx_read_ppbuf(sc, cmd, cmdbuf);
ret:
	DPRINTF(3,("%s: short xfer done, error=%d\n", DEVNAME(sc), error));
	return error;
}

static int
rtsx_xfer(struct rtsx_softc *sc, struct sdmmc_command *cmd, uint32_t *cmdbuf)
{
	int ncmd, dma_dir, error, tmode;
	int read = ISSET(cmd->c_flags, SCF_CMD_READ);
	uint8_t cfg2;

	DPRINTF(3,("%s: %s xfer: %d bytes with block size %d\n", DEVNAME(sc),
	    read ? "read" : "write", cmd->c_datalen, cmd->c_blklen));

	if (cmd->c_datalen > RTSX_DMA_DATA_BUFSIZE) {
		DPRINTF(3, ("%s: cmd->c_datalen too large: %d > %d\n",
		    DEVNAME(sc), cmd->c_datalen, RTSX_DMA_DATA_BUFSIZE));
		return ENOMEM;
	}

	/* Configure DMA transfer mode parameters. */
	cfg2 = RTSX_SD_NO_CHECK_WAIT_CRC_TO | RTSX_SD_CHECK_CRC16 |
	    RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_RSP_LEN_0;
	if (read) {
		dma_dir = RTSX_DMA_DIR_FROM_CARD;
		/* Use transfer mode AUTO_READ3, which assumes we've already
		 * sent the read command and gotten the response, and will
		 * send CMD 12 manually after reading multiple blocks. */
		tmode = RTSX_TM_AUTO_READ3;
		cfg2 |= RTSX_SD_CALCULATE_CRC7 | RTSX_SD_CHECK_CRC7;
	} else {
		dma_dir = RTSX_DMA_DIR_TO_CARD;
		/* Use transfer mode AUTO_WRITE3, which assumes we've already
		 * sent the write command and gotten the response, and will
		 * send CMD 12 manually after writing multiple blocks. */
		tmode = RTSX_TM_AUTO_WRITE3;
		cfg2 |= RTSX_SD_NO_CALCULATE_CRC7 | RTSX_SD_NO_CHECK_CRC7;
	}

	/* The command buffer queues commands the host controller will
	 * run asynchronously. */
	ncmd = 0;

	/* Queue command to set response type. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2,
	    0xff, cfg2); 

	/* Queue commands to configure data transfer size. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_L,
	    0xff, 0x00);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_H,
	    0xff, 0x02);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_L,
	    0xff, cmd->c_datalen / cmd->c_blklen);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_H,
	    0xff, (cmd->c_datalen / cmd->c_blklen) >> 8);

	/* Use the DMA ring buffer for commands which transfer data. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_CARD_DATA_SOURCE,
	    0x01, RTSX_RING_BUFFER);

	/* Configure DMA controller. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_IRQSTAT0,
	    RTSX_DMA_DONE_INT, RTSX_DMA_DONE_INT);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_DMATC3,
	    0xff, cmd->c_datalen >> 24);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_DMATC2,
	    0xff, cmd->c_datalen >> 16);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_DMATC1,
	    0xff, cmd->c_datalen >> 8);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_DMATC0,
	    0xff, cmd->c_datalen);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_DMACTL,
	    RTSX_DMA_EN | RTSX_DMA_DIR | RTSX_DMA_PACK_SIZE_MASK,
	    RTSX_DMA_EN | dma_dir | RTSX_DMA_512);

	/* Queue commands to perform SD transfer. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
	    0xff, tmode | RTSX_SD_TRANSFER_START);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
	    RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

	error = rtsx_hostcmd_send(sc, ncmd);
	if (error)
		goto ret;

	mutex_enter(&sc->sc_host_mtx);

	/* Tell the chip where the data buffer is and run the transfer. */
	WRITE4(sc, RTSX_HDBAR, cmd->c_dmamap->dm_segs[0].ds_addr);
	WRITE4(sc, RTSX_HDBCTLR, RTSX_TRIG_DMA | (read ? RTSX_DMA_READ : 0) |
	    (cmd->c_dmamap->dm_segs[0].ds_len & 0x00ffffff));

	mutex_exit(&sc->sc_host_mtx);

	/* Wait for completion. */
	error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, 10*hz);
ret:
	DPRINTF(3,("%s: xfer done, error=%d\n", DEVNAME(sc), error));
	return error;
}

static void
rtsx_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct rtsx_softc *sc = sch;
	bus_dma_segment_t segs[1];
	int rsegs;
	void *cmdkvap;
	uint32_t *cmdbuf;
	uint8_t rsp_type;
	uint16_t r;
	int ncmd;
	int error = 0;

	DPRINTF(3,("%s: executing cmd %hu\n", DEVNAME(sc), cmd->c_opcode));

	/* Refuse SDIO probe if the chip doesn't support SDIO. */
	if (cmd->c_opcode == SD_IO_SEND_OP_COND &&
	    !ISSET(sc->sc_flags, RTSX_F_SDIO_SUPPORT)) {
		error = ENOTSUP;
		goto ret;
	}

	rsp_type = rtsx_response_type(cmd->c_flags & SCF_RSP_MASK);
	if (rsp_type == 0) {
		aprint_error_dev(sc->sc_dev, "unknown response type 0x%x\n",
		    cmd->c_flags & SCF_RSP_MASK);
		error = EINVAL;
		goto ret;
	}

	/* Allocate and map the host command buffer. */
	error = bus_dmamem_alloc(sc->sc_dmat, RTSX_HOSTCMD_BUFSIZE, 0, 0,
	    segs, 1, &rsegs, BUS_DMA_WAITOK);
	if (error)
		goto ret;
	error = bus_dmamem_map(sc->sc_dmat, segs, rsegs, RTSX_HOSTCMD_BUFSIZE,
	    &cmdkvap, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	if (error)
		goto free_cmdbuf;

	/* Load command DMA buffer. */
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmap_cmd, cmdkvap,
	    RTSX_HOSTCMD_BUFSIZE, NULL, BUS_DMA_WAITOK);
	if (error)
		goto unmap_cmdbuf;

	/* Use another transfer method when data size < 512. */
	if (cmd->c_data != NULL && cmd->c_datalen < 512) {
		error = rtsx_exec_short_xfer(sch, cmd, cmdkvap, rsp_type);
		goto unload_cmdbuf;
	}

	/* The command buffer queues commands the host controller will
	 * run asynchronously. */
	cmdbuf = cmdkvap;
	ncmd = 0;

	/* Queue commands to set SD command index and argument. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD0,
	    0xff, 0x40 | cmd->c_opcode); 
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD1,
	    0xff, cmd->c_arg >> 24);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD2,
	    0xff, cmd->c_arg >> 16);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD3,
	    0xff, cmd->c_arg >> 8);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CMD4,
	    0xff, cmd->c_arg);

	/* Queue command to set response type. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2,
	    0xff, rsp_type);

	/* Use the ping-pong buffer for commands which do not transfer data. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_CARD_DATA_SOURCE,
	    0x01, RTSX_PINGPONG_BUFFER);

	/* Queue commands to perform SD transfer. */
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
	    0xff, RTSX_TM_CMD_RSP | RTSX_SD_TRANSFER_START);
	rtsx_hostcmd(cmdbuf, &ncmd, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
	    RTSX_SD_TRANSFER_END | RTSX_SD_STAT_IDLE,
	    RTSX_SD_TRANSFER_END | RTSX_SD_STAT_IDLE);

	/* Queue commands to read back card status response.*/
	if (rsp_type == RTSX_SD_RSP_TYPE_R2) {
		for (r = RTSX_PPBUF_BASE2 + 15; r > RTSX_PPBUF_BASE2; r--)
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_READ_REG_CMD, r, 0, 0);
		rtsx_hostcmd(cmdbuf, &ncmd, RTSX_READ_REG_CMD, RTSX_SD_CMD5,
		    0, 0);
	} else if (rsp_type != RTSX_SD_RSP_TYPE_R0) {
		for (r = RTSX_SD_CMD0; r <= RTSX_SD_CMD4; r++)
			rtsx_hostcmd(cmdbuf, &ncmd, RTSX_READ_REG_CMD, r, 0, 0);
	}

	/* Run the command queue and wait for completion. */
	error = rtsx_hostcmd_send(sc, ncmd);
	if (error == 0)
		error = rtsx_wait_intr(sc, RTSX_TRANS_OK_INT, hz);
	if (error)
		goto unload_cmdbuf;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmap_cmd, 0, RTSX_HOSTCMD_BUFSIZE,
	    BUS_DMASYNC_POSTREAD);

	/* Copy card response into sdmmc response buffer. */
	if (ISSET(cmd->c_flags, SCF_RSP_PRESENT)) {
		/* Copy bytes like sdhc(4), which on little-endian uses
		 * different byte order for short and long responses... */
		if (ISSET(cmd->c_flags, SCF_RSP_136)) {
			uint8_t *resp = cmdkvap;
			memcpy(cmd->c_resp, resp + 1, sizeof(cmd->c_resp));
		} else {
			/* First byte is CHECK_REG_CMD return value, second
			 * one is the command op code -- we skip those. */
			cmd->c_resp[0] =
			    ((be32toh(cmdbuf[0]) & 0x0000ffff) << 16) |
			    ((be32toh(cmdbuf[1]) & 0xffff0000) >> 16);
		}
	}

	if (cmd->c_data) {
		error = rtsx_xfer(sc, cmd, cmdbuf);
		if (error) {
			uint8_t stat1;
			if (rtsx_read(sc, RTSX_SD_STAT1, &stat1) == 0 &&
			    (stat1 & RTSX_SD_CRC_ERR)) {
				aprint_error_dev(sc->sc_dev,
				    "CRC error (stat=0x%x)\n", stat1);
			}
		}
	}

unload_cmdbuf:
	bus_dmamap_unload(sc->sc_dmat, sc->sc_dmap_cmd);
unmap_cmdbuf:
	bus_dmamem_unmap(sc->sc_dmat, cmdkvap, RTSX_HOSTCMD_BUFSIZE);
free_cmdbuf:
	bus_dmamem_free(sc->sc_dmat, segs, rsegs);
ret:
	SET(cmd->c_flags, SCF_ITSDONE);
	cmd->c_error = error;
}

/* Prepare for another command. */
static void
rtsx_soft_reset(struct rtsx_softc *sc)
{

	DPRINTF(1,("%s: soft reset\n", DEVNAME(sc)));

	/* Stop command transfer. */
	WRITE4(sc, RTSX_HCBCTLR, RTSX_STOP_CMD);

	(void)rtsx_write(sc, RTSX_CARD_STOP, RTSX_SD_STOP|RTSX_SD_CLR_ERR,
		    RTSX_SD_STOP|RTSX_SD_CLR_ERR);

	/* Stop DMA transfer. */
	WRITE4(sc, RTSX_HDBCTLR, RTSX_STOP_DMA);
	(void)rtsx_write(sc, RTSX_DMACTL, RTSX_DMA_RST, RTSX_DMA_RST);

	(void)rtsx_write(sc, RTSX_RBCTL, RTSX_RB_FLUSH, RTSX_RB_FLUSH);
}

static int
rtsx_wait_intr(struct rtsx_softc *sc, int mask, int timo)
{
	int status;
	int error = 0;

	mask |= RTSX_TRANS_FAIL_INT;

	mutex_enter(&sc->sc_intr_mtx);

	status = sc->sc_intr_status & mask;
	while (status == 0) {
		if (cv_timedwait(&sc->sc_intr_cv, &sc->sc_intr_mtx, timo)
		    == EWOULDBLOCK) {
			rtsx_soft_reset(sc);
			error = ETIMEDOUT;
			break;
		}
		status = sc->sc_intr_status & mask;
	}
	sc->sc_intr_status &= ~status;

	/* Has the card disappeared? */
	if (!ISSET(sc->sc_flags, RTSX_F_CARD_PRESENT))
		error = ENODEV;

	mutex_exit(&sc->sc_intr_mtx);

	if (error == 0 && (status & RTSX_TRANS_FAIL_INT))
		error = EIO;
	return error;
}

static void
rtsx_card_insert(struct rtsx_softc *sc)
{

	DPRINTF(1, ("%s: card inserted\n", DEVNAME(sc)));

	sc->sc_flags |= RTSX_F_CARD_PRESENT;
	(void)rtsx_led_enable(sc);

	/* Schedule card discovery task. */
	sdmmc_needs_discover(sc->sc_sdmmc);
}

static void
rtsx_card_eject(struct rtsx_softc *sc)
{

	DPRINTF(1, ("%s: card ejected\n", DEVNAME(sc)));

	sc->sc_flags &= ~RTSX_F_CARD_PRESENT;
	(void)rtsx_led_disable(sc);

	/* Schedule card discovery task. */
	sdmmc_needs_discover(sc->sc_sdmmc);
}

/*
 * Established by attachment driver at interrupt priority IPL_SDMMC.
 */
int
rtsx_intr(void *arg)
{
	struct rtsx_softc *sc = arg;
	uint32_t enabled, status;

	enabled = READ4(sc, RTSX_BIER);
	status = READ4(sc, RTSX_BIPR);

	/* Ack interrupts. */
	WRITE4(sc, RTSX_BIPR, status);

	if (((enabled & status) == 0) || status == 0xffffffff)
		return 0;

	mutex_enter(&sc->sc_intr_mtx);

	if (status & RTSX_SD_INT) {
		if (status & RTSX_SD_EXIST) {
			if (!ISSET(sc->sc_flags, RTSX_F_CARD_PRESENT))
				rtsx_card_insert(sc);
		} else {
			rtsx_card_eject(sc);
		}
	}

	if (status & (RTSX_TRANS_OK_INT | RTSX_TRANS_FAIL_INT)) {
		sc->sc_intr_status |= status;
		cv_broadcast(&sc->sc_intr_cv);
	}

	mutex_exit(&sc->sc_intr_mtx);

	return 1;
}

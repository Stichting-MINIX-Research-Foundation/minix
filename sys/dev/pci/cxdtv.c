/* $NetBSD: cxdtv.c,v 1.14 2014/03/29 19:28:24 christos Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cxdtv.c,v 1.14 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>

#include <dev/i2c/tvpllvar.h>
#include <dev/i2c/tvpll_tuners.h>

#include <dev/i2c/nxt2kvar.h>
#include <dev/i2c/lg3303var.h>

#include <dev/dtv/dtvif.h>

#include <dev/pci/cxdtvreg.h>
#include <dev/pci/cxdtvvar.h>
#include <dev/pci/cxdtv_boards.h>

#define CXDTV_MMBASE		0x10

#define CXDTV_SRAM_CH_MPEG	0
#define CXDTV_TS_PKTSIZE	(188 * 8)

static int cxdtv_match(device_t, cfdata_t, void *);
static void cxdtv_attach(device_t, device_t, void *);
static int cxdtv_detach(device_t, int);
static int cxdtv_rescan(device_t, const char *, const int *);
static void cxdtv_childdet(device_t, device_t);
static int cxdtv_intr(void *);

static bool cxdtv_resume(device_t, const pmf_qual_t *);

static int	cxdtv_iic_acquire_bus(void *, int);
static void	cxdtv_iic_release_bus(void *, int);
static int	cxdtv_iic_send_start(void *, int);
static int	cxdtv_iic_send_stop(void *, int);
static int	cxdtv_iic_initiate_xfer(void *, i2c_addr_t, int);
static int	cxdtv_iic_read_byte(void *, uint8_t *, int);
static int	cxdtv_iic_write_byte(void *, uint8_t, int);

static void	cxdtv_i2cbb_set_bits(void *, uint32_t);
static void	cxdtv_i2cbb_set_dir(void *, uint32_t);
static uint32_t	cxdtv_i2cbb_read_bits(void *);

static int	cxdtv_sram_ch_setup(struct cxdtv_softc *,
				    struct cxdtv_sram_ch *, uint32_t);
static int	cxdtv_allocmem(struct cxdtv_softc *, size_t, size_t,
    struct cxdtv_dma *);
static int	cxdtv_freemem(struct cxdtv_softc *, struct cxdtv_dma *);
static int	cxdtv_risc_buffer(struct cxdtv_softc *, uint32_t, uint32_t);
static int	cxdtv_risc_field(struct cxdtv_softc *, uint32_t *, uint32_t);

static int     cxdtv_mpeg_attach(struct cxdtv_softc *);
static int     cxdtv_mpeg_detach(struct cxdtv_softc *, int flags);
static int     cxdtv_mpeg_intr(struct cxdtv_softc *);
static int     cxdtv_mpeg_reset(struct cxdtv_softc *);

static int     cxdtv_mpeg_trigger(struct cxdtv_softc *, void *);
static int     cxdtv_mpeg_halt(struct cxdtv_softc *);
static void *  cxdtv_mpeg_malloc(struct cxdtv_softc *, size_t);
static void    cxdtv_mpeg_free(struct cxdtv_softc *, void *);

static void cxdtv_card_init_hd5500(struct cxdtv_softc *);
static void cxdtv_card_init_hdtvwonder(struct cxdtv_softc *);

/* MPEG TS Port */
static void cxdtv_dtv_get_devinfo(void *, struct dvb_frontend_info *);
static int cxdtv_dtv_open(void *, int);
static void cxdtv_dtv_close(void *);
static int cxdtv_dtv_set_tuner(void *, const struct dvb_frontend_parameters *);
static fe_status_t cxdtv_dtv_get_status(void *);
static uint16_t cxdtv_dtv_get_signal_strength(void *);
static uint16_t cxdtv_dtv_get_snr(void *);
static int cxdtv_dtv_start_transfer(void *,
    void (*)(void *, const struct dtv_payload *), void *);
static int cxdtv_dtv_stop_transfer(void *);

static const struct dtv_hw_if cxdtv_dtv_if = {
	.get_devinfo = cxdtv_dtv_get_devinfo,
	.open = cxdtv_dtv_open,
	.close = cxdtv_dtv_close,
	.set_tuner = cxdtv_dtv_set_tuner,
	.get_status = cxdtv_dtv_get_status,
	.get_signal_strength = cxdtv_dtv_get_signal_strength,
	.get_snr = cxdtv_dtv_get_snr,
	.start_transfer = cxdtv_dtv_start_transfer,
	.stop_transfer = cxdtv_dtv_stop_transfer,
};

const struct i2c_bitbang_ops cxdtv_i2cbb_ops = {
	cxdtv_i2cbb_set_bits,
	cxdtv_i2cbb_set_dir,
	cxdtv_i2cbb_read_bits,
	{ CXDTV_I2C_C_DATACONTROL_SDA, CXDTV_I2C_C_DATACONTROL_SCL, 0, 0 }
};

/* Maybe make this dynamically allocated. */
static struct cxdtv_sram_ch cxdtv_sram_chs[] = {
	[CXDTV_SRAM_CH_MPEG] = {
		.csc_cmds = 0x180200, /* CMDS for ch. 28 */
		.csc_iq = 0x180340, /* after last CMDS */
		.csc_iqsz = 0x40, /* 16 dwords */
		.csc_cdt = 0x180380, /* after iq */
		.csc_cdtsz = 0x40, /* cluster discriptor space */
		.csc_fifo = 0x180400, /* after cdt */
		.csc_fifosz = 0x001C00, /* let's just align this up */
		.csc_risc = 0x182000, /* after fifo */
		.csc_riscsz = 0x6000, /* room for dma programs */
		.csc_ptr1 = CXDTV_DMA28_PTR1,
		.csc_ptr2 = CXDTV_DMA28_PTR2,
		.csc_cnt1 = CXDTV_DMA28_CNT1,
		.csc_cnt2 = CXDTV_DMA28_CNT2,
	},
};

CFATTACH_DECL2_NEW(cxdtv, sizeof(struct cxdtv_softc),
    cxdtv_match, cxdtv_attach, cxdtv_detach, NULL,
    cxdtv_rescan, cxdtv_childdet);

static int
cxdtv_match(device_t parent, cfdata_t match, void *aux)
{
	const struct pci_attach_args *pa;
	pcireg_t reg;

	pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CONEXANT)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_CONEXANT_CX2388XMPEG)
		return 0;

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	if (cxdtv_board_lookup(PCI_VENDOR(reg), PCI_PRODUCT(reg)) == NULL)
		return 0;

	return 1;
}

static void
cxdtv_attach(device_t parent, device_t self, void *aux)
{
	struct cxdtv_softc *sc;
	const struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t reg;
	const char *intrstr;
	struct i2cbus_attach_args iba;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);

	sc->sc_dev = self;
	sc->sc_pc = pa->pa_pc;

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	sc->sc_vendor = PCI_VENDOR(reg);
	sc->sc_product = PCI_PRODUCT(reg);

	sc->sc_board = cxdtv_board_lookup(sc->sc_vendor, sc->sc_product);
	KASSERT(sc->sc_board != NULL);

	pci_aprint_devinfo(pa, NULL);

	if (pci_mapreg_map(pa, CXDTV_MMBASE, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems)) {
		aprint_error_dev(self, "couldn't map memory space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_VM, cxdtv_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	/* set master */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	mutex_init(&sc->sc_i2c_buslock, MUTEX_DRIVER, IPL_NONE);
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_exec = NULL;
	sc->sc_i2c.ic_acquire_bus = cxdtv_iic_acquire_bus;
	sc->sc_i2c.ic_release_bus = cxdtv_iic_release_bus;
	sc->sc_i2c.ic_send_start = cxdtv_iic_send_start;
	sc->sc_i2c.ic_send_stop = cxdtv_iic_send_stop;
	sc->sc_i2c.ic_initiate_xfer = cxdtv_iic_initiate_xfer;
	sc->sc_i2c.ic_read_byte = cxdtv_iic_read_byte;
	sc->sc_i2c.ic_write_byte = cxdtv_iic_write_byte;

#if notyet
	/* enable i2c compatible software mode */
	val = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_I2C_C_DATACONTROL);
	val = CXDTV_I2C_C_DATACONTROL_SCL | CXDTV_I2C_C_DATACONTROL_SDA;
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_I2C_C_DATACONTROL, val);
#endif

	cxdtv_mpeg_attach(sc);

	/* attach other devices to iic(4) */
	memset(&iba, 0, sizeof(iba));
	iba.iba_tag = &sc->sc_i2c;
	config_found_ia(self, "i2cbus", &iba, iicbus_print);

	if (!pmf_device_register(self, NULL, cxdtv_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

static int
cxdtv_detach(device_t self, int flags)
{
	struct cxdtv_softc *sc = device_private(self);
	int error;

	error = cxdtv_mpeg_detach(sc, flags);
	if (error)
		return error;

	if (sc->sc_ih)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);

	if (sc->sc_mems)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);

	mutex_destroy(&sc->sc_i2c_buslock);

	return 0;
}

static int
cxdtv_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct cxdtv_softc *sc = device_private(self);
	struct dtv_attach_args daa;

	daa.hw = &cxdtv_dtv_if;
	daa.priv = sc;

	if (ifattr_match(ifattr, "dtvbus") && sc->sc_dtvdev == NULL)
		sc->sc_dtvdev = config_found_ia(sc->sc_dev, "dtvbus",
		    &daa, dtv_print);

	return 0;
}

static void
cxdtv_childdet(device_t self, device_t child)
{
	struct cxdtv_softc *sc = device_private(self);

	if (child == sc->sc_dtvdev)
		sc->sc_dtvdev = NULL;
}

static bool
cxdtv_resume(device_t dv, const pmf_qual_t *qual)
{
	/* XXX revisit */

	aprint_debug_dev(dv, "%s\n", __func__);

	return true;
}

static int
cxdtv_intr(void *intarg)
{
	struct cxdtv_softc *sc = intarg;
	uint32_t val;

	val = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MSTAT);
	if (val == 0) {
		return 0; /* not ours */
	}

	if (val & CXT_PI_TS_INT) {
		cxdtv_mpeg_intr(sc);
	}

	if (val & ~CXT_PI_TS_INT) {
		device_printf(sc->sc_dev, "%s, %08x\n", __func__, val);
	}

	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_STAT, val);

	return 1;
}

/* I2C interface */

static void
cxdtv_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct cxdtv_softc *sc = cookie;

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_I2C_C_DATACONTROL, bits);
	(void)bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_I2C_C_DATACONTROL);

	return;
}

static void
cxdtv_i2cbb_set_dir(void *cookie, uint32_t bits)
{
	return;
}

static uint32_t
cxdtv_i2cbb_read_bits(void *cookie)
{
	struct cxdtv_softc *sc = cookie;
	uint32_t value;

	value = bus_space_read_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_I2C_C_DATACONTROL);

	return value;
}

static int
cxdtv_iic_acquire_bus(void *cookie, int flags)
{
	struct cxdtv_softc *sc = cookie;

	mutex_enter(&sc->sc_i2c_buslock);

	return 0;
}

static void
cxdtv_iic_release_bus(void *cookie, int flags)
{
	struct cxdtv_softc *sc = cookie;

	mutex_exit(&sc->sc_i2c_buslock);

	return;
}

static int
cxdtv_iic_send_start(void *cookie, int flags)
{
	return i2c_bitbang_send_start(cookie, flags, &cxdtv_i2cbb_ops);
}

static int
cxdtv_iic_send_stop(void *cookie, int flags)
{
	return i2c_bitbang_send_stop(cookie, flags, &cxdtv_i2cbb_ops);
}

static int
cxdtv_iic_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{
	return i2c_bitbang_initiate_xfer(cookie, addr, flags, &cxdtv_i2cbb_ops);
}

static int
cxdtv_iic_read_byte(void *cookie, uint8_t *data, int flags)
{
	return i2c_bitbang_read_byte(cookie, data, flags, &cxdtv_i2cbb_ops);
}

static int
cxdtv_iic_write_byte(void *cookie, uint8_t data, int flags)
{
	return i2c_bitbang_write_byte(cookie, data, flags, &cxdtv_i2cbb_ops);
}

int
cxdtv_mpeg_attach(struct cxdtv_softc *sc)
{
	struct cxdtv_sram_ch *ch;

	CX_DPRINTF(("cxdtv_mpeg_attach\n"));

	ch = &cxdtv_sram_chs[CXDTV_SRAM_CH_MPEG];
	
	sc->sc_riscbufsz = ch->csc_riscsz;
	sc->sc_riscbuf = kmem_alloc(ch->csc_riscsz, KM_SLEEP);

	if ( sc->sc_riscbuf == NULL )
		panic("riscbuf null");

	aprint_debug_dev(sc->sc_dev, "attaching frontend...\n");

	switch(sc->sc_vendor) {
	case PCI_VENDOR_ATI:
		cxdtv_card_init_hdtvwonder(sc);
		break;
	case PCI_VENDOR_PCHDTV:
		if (sc->sc_product == PCI_PRODUCT_PCHDTV_HD5500) {
			cxdtv_card_init_hd5500(sc);
		}
		break;
	}

	KASSERT(sc->sc_tuner == NULL);
	KASSERT(sc->sc_demod == NULL);

	switch(sc->sc_board->cb_demod) {
	case CXDTV_DEMOD_NXT2004:
		sc->sc_demod = nxt2k_open(sc->sc_dev, &sc->sc_i2c, 0x0a, 0);
		break;
	case CXDTV_DEMOD_LG3303:
		sc->sc_demod = lg3303_open(sc->sc_dev, &sc->sc_i2c, 0x59,
		    LG3303_CFG_SERIAL_INPUT);
		break;
	default:
		break;
	}

	switch(sc->sc_board->cb_tuner) {
	case CXDTV_TUNER_PLL:
		if (sc->sc_vendor == PCI_VENDOR_ATI)
			sc->sc_tuner = tvpll_open(sc->sc_dev, &sc->sc_i2c, 0x61, &tvpll_tuv1236d_pll);
		if (sc->sc_vendor == PCI_VENDOR_PCHDTV)
			sc->sc_tuner = tvpll_open(sc->sc_dev, &sc->sc_i2c, 0x61, &tvpll_tdvs_h06xf_pll);
		break;
	default:
		break;
	}

	KASSERT(sc->sc_tuner != NULL);
	KASSERT(sc->sc_demod != NULL);

	cxdtv_rescan(sc->sc_dev, NULL, NULL);

	return (sc->sc_dtvdev != NULL);
}

int
cxdtv_mpeg_detach(struct cxdtv_softc *sc, int flags)
{
	int error = 0;

	if (sc->sc_dtvdev) {
		error = config_detach(sc->sc_dtvdev, flags);
		if (error)
			return error;
	}

	if (sc->sc_demod) {
		switch (sc->sc_board->cb_demod) {
		case CXDTV_DEMOD_NXT2004:
			nxt2k_close(sc->sc_demod);
			break;
		case CXDTV_DEMOD_LG3303:
			lg3303_close(sc->sc_demod);
			break;
		default:
			break;
		}
		sc->sc_demod = NULL;
	}
	if (sc->sc_tuner) {
		switch (sc->sc_board->cb_tuner) {
		case CXDTV_TUNER_PLL:
			tvpll_close(sc->sc_tuner);
			break;
		default:
			break;
		}
		sc->sc_tuner = NULL;
	}

	if (sc->sc_riscbuf) {
		kmem_free(sc->sc_riscbuf, sc->sc_riscbufsz);
		sc->sc_riscbuf = NULL;
		sc->sc_riscbufsz = 0;
	}

	return error;
}

static void
cxdtv_dtv_get_devinfo(void *priv, struct dvb_frontend_info *info)
{
	memset(info, 0, sizeof(*info));
	strlcpy(info->name, "CX23880", sizeof(info->name));
	info->type = FE_ATSC;
	info->frequency_min = 54000000;
	info->frequency_max = 858000000;
	info->frequency_stepsize = 62500;
	info->caps = FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_8VSB;
}

static int
cxdtv_dtv_open(void *priv, int flags)
{
	struct cxdtv_softc *sc = priv;

	KASSERT(sc->sc_tsbuf == NULL);

	cxdtv_mpeg_reset(sc);

	/* allocate two alternating DMA areas for MPEG TS packets */
	sc->sc_tsbuf = cxdtv_mpeg_malloc(sc, CXDTV_TS_PKTSIZE * 2);

	if (sc->sc_tsbuf == NULL)
		return ENOMEM;

	return 0;
}

static void
cxdtv_dtv_close(void *priv)
{
	struct cxdtv_softc *sc = priv;

	cxdtv_dtv_stop_transfer(sc);

	if (sc->sc_tsbuf != NULL) {
		cxdtv_mpeg_free(sc, sc->sc_tsbuf);
		sc->sc_tsbuf = NULL;
	}
}

static int
cxdtv_dtv_set_tuner(void *priv, const struct dvb_frontend_parameters *params)
{
	struct cxdtv_softc *sc = priv;
	int error = -1;

	switch(sc->sc_board->cb_tuner) {
	case CXDTV_TUNER_PLL:
		error = tvpll_tune_dtv(sc->sc_tuner, params);
	}
	if (error)
		goto bad;

	switch(sc->sc_board->cb_demod) {
	case CXDTV_DEMOD_NXT2004:
		error = nxt2k_set_modulation(sc->sc_demod, params->u.vsb.modulation);
		break;
	case CXDTV_DEMOD_LG3303:
		error = lg3303_set_modulation(sc->sc_demod, params->u.vsb.modulation);
		break;
	default:
		break;
	}

bad:
	return error;
}

static fe_status_t
cxdtv_dtv_get_status(void *priv)
{
	struct cxdtv_softc *sc = priv;

	switch(sc->sc_board->cb_demod) {
	case CXDTV_DEMOD_NXT2004:
		return nxt2k_get_dtv_status(sc->sc_demod);
	case CXDTV_DEMOD_LG3303:
		return lg3303_get_dtv_status(sc->sc_demod);
	default:
		return 0;
	}
}

static uint16_t
cxdtv_dtv_get_signal_strength(void *priv)
{
	struct cxdtv_softc *sc = priv;

	switch(sc->sc_board->cb_demod) {
	case CXDTV_DEMOD_NXT2004:
		return 0;	/* TODO */
	case CXDTV_DEMOD_LG3303:
		return lg3303_get_signal_strength(sc->sc_demod);
	}

	return 0;
}

static uint16_t
cxdtv_dtv_get_snr(void *priv)
{
	struct cxdtv_softc *sc = priv;

	switch(sc->sc_board->cb_demod) {
	case CXDTV_DEMOD_NXT2004:
		return 0;	/* TODO */
	case CXDTV_DEMOD_LG3303:
		return lg3303_get_snr(sc->sc_demod);
	}

	return 0;
}

static int
cxdtv_dtv_start_transfer(void *priv,
    void (*cb)(void *, const struct dtv_payload *), void *arg)
{
	struct cxdtv_softc *sc = priv;
	
	sc->sc_dtvsubmitcb = cb;
	sc->sc_dtvsubmitarg = arg;

	/* allocate two alternating DMA areas for MPEG TS packets */
	sc->sc_tsbuf = cxdtv_mpeg_malloc(sc, CXDTV_TS_PKTSIZE * 2);

	cxdtv_mpeg_trigger(sc, sc->sc_tsbuf);

	return 0;
}

static int
cxdtv_dtv_stop_transfer(void *priv)
{
	struct cxdtv_softc *sc = priv;

	cxdtv_mpeg_halt(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK, 0);

	sc->sc_dtvsubmitcb = NULL;
	sc->sc_dtvsubmitarg = NULL;

	return 0;
}

int
cxdtv_mpeg_reset(struct cxdtv_softc *sc)
{
	uint32_t v;

	CX_DPRINTF(("cxdtv_mpeg_reset\n"));

	v = (uint32_t)-1; 

	/* shutdown */
	/* hold RISC in reset */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_DEV_CNTRL2, 0);
	/* disable FIFO and RISC */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_DMA_CNTRL, 0);
	/* mask off all interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK, 0);

	/* clear interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_STAT, v);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_STAT, v);

	memset(sc->sc_riscbuf, 0, sc->sc_riscbufsz);

	/* XXX magic */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PDMA_STHRSH, 0x0707);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PDMA_DTHRSH, 0x0707);

	/* reset external components*/
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_SRST_IO, 0);
	kpause("cxdtvrst", false, MAX(1, mstohz(1)), NULL);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_SRST_IO, 1);

	/* let error interrupts happen */
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK,
	    v | 0x00fc00); /* XXX magic */

	return 0;
}

static int
cxdtv_risc_buffer(struct cxdtv_softc *sc, uint32_t bpl, uint32_t lines)
{
	uint32_t *rm;
	uint32_t size;

	CX_DPRINTF(("cxdtv_risc_buffer: bpl=0x%x\n", bpl));

	size = 1 + (bpl * lines) / PAGE_SIZE + lines;
	size += 2;

	device_printf(sc->sc_dev, "%s: est. inst. %d\n", __func__, size);

	size *= 8;
	device_printf(sc->sc_dev, "%s: est. qword %d\n", __func__, size);

	if (sc->sc_riscbuf == NULL) {
		device_printf(sc->sc_dev, "not enough memory for RISC\n");
		return ENOMEM;
	}

	rm = (uint32_t *)sc->sc_riscbuf;
	cxdtv_risc_field(sc, rm, bpl);

	return 0;
}

static int
cxdtv_risc_field(struct cxdtv_softc *sc, uint32_t *rm, uint32_t bpl)
{
	struct cxdtv_dma *p;

	CX_DPRINTF(("cxdtv_risc_field: bpl=0x%x\n", bpl));

	for (p = sc->sc_dma; p && KERNADDR(p) != sc->sc_tsbuf; p = p->next)
		continue;
	if (p == NULL) {
		device_printf(sc->sc_dev, "cxdtv_risc_field: bad addr %p\n",
		    sc->sc_tsbuf);
		return ENOENT;
	}

	memset(sc->sc_riscbuf, 0, sc->sc_riscbufsz);

	rm = sc->sc_riscbuf;

	/* htole32 will be done when program is copied to chip SRAM */

	/* XXX */
	*(rm++) = (CX_RISC_SYNC|0);

	*(rm++) = (CX_RISC_WRITE|CX_RISC_SOL|CX_RISC_EOL|CX_RISC_IRQ1|bpl);
	*(rm++) = (DMAADDR(p) + 0 * bpl);

	*(rm++) = (CX_RISC_WRITE|CX_RISC_SOL|CX_RISC_EOL|CX_RISC_IRQ2|bpl);
	*(rm++) = (DMAADDR(p) + 1 * bpl);

	*(rm++) = (CX_RISC_JUMP|1);
	*(rm++) = (cxdtv_sram_chs[CXDTV_SRAM_CH_MPEG].csc_risc + 4);

	return 0;
}

static int
cxdtv_sram_ch_setup(struct cxdtv_softc *sc, struct cxdtv_sram_ch *csc,
    uint32_t bpl)
{
	unsigned int i, lines;
	uint32_t cdt;

	CX_DPRINTF(("cxdtv_sram_ch_setup: bpl=0x%x\n", bpl));

	/* XXX why round? */
	bpl = (bpl + 7) & ~7;
	CX_DPRINTF(("cxdtv_sram_ch_setup: bpl=0x%x\n", bpl));
	cdt = csc->csc_cdt;
	lines = csc->csc_fifosz / bpl;
	device_printf(sc->sc_dev, "%s %d lines\n", __func__, lines);

	/* fill in CDT */
	for (i = 0; i < lines; i++) {
		CX_DPRINTF(("CDT ent %08x, %08x\n", cdt + (16 * i),
		    csc->csc_fifo + (bpl * i)));
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    cdt + (16 * i),
		    csc->csc_fifo + (bpl * i));
	}

	/* copy DMA program */

	/* converts program to little endian as it goes into SRAM */
	bus_space_write_region_4(sc->sc_memt, sc->sc_memh,
	     csc->csc_risc, (void *)sc->sc_riscbuf, sc->sc_riscbufsz >> 2);

	/* fill in CMDS */
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CX_CMDS_O_IRPC, csc->csc_risc);

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CX_CMDS_O_CDTB, csc->csc_cdt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CX_CMDS_O_CDTS, (lines * 16) >> 3); /* XXX magic */

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CX_CMDS_O_IQB, csc->csc_iq);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CX_CMDS_O_IQS,
	    CX_CMDS_IQS_ISRP | (csc->csc_iqsz >> 2) );

	/* zero rest of CMDS */
	bus_space_set_region_4(sc->sc_memt, sc->sc_memh, 0x14, 0, 0x2c/4);

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cnt1, (bpl >> 3) - 1);

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_ptr2, cdt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cnt2, (lines * 16) >> 3);

	return 0;
}

int
cxdtv_mpeg_trigger(struct cxdtv_softc *sc, void *buf)
{
	struct cxdtv_dma *p;
	struct cxdtv_sram_ch *ch;
	uint32_t v;

	ch = &cxdtv_sram_chs[CXDTV_SRAM_CH_MPEG];

	for (p = sc->sc_dma; p && KERNADDR(p) != buf; p = p->next)
		continue;
	if (p == NULL) {
		device_printf(sc->sc_dev, "cxdtv_mpeg_trigger: bad addr %p\n",
		    buf);
		return ENOENT;
	}

	CX_DPRINTF(("cxdtv_mpeg_trigger: buf=%p\n", buf));

	cxdtv_risc_buffer(sc, CXDTV_TS_PKTSIZE, 1);
	cxdtv_sram_ch_setup(sc, ch, CXDTV_TS_PKTSIZE);

	/* software reset */

	switch(sc->sc_vendor) {
	case PCI_VENDOR_ATI:
		/* both ATI boards with DTV are the same */
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    CXDTV_TS_GEN_CONTROL, IPB_SW_RST);
		delay(100);
		/* parallel MPEG port */
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    CXDTV_PINMUX_IO, MPEG_PAR_EN);
		break;
	case PCI_VENDOR_PCHDTV:
		if (sc->sc_product == PCI_PRODUCT_PCHDTV_HD5500) {
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    CXDTV_TS_GEN_CONTROL, IPB_SW_RST|IPB_SMODE);
			delay(100);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    CXDTV_PINMUX_IO, 0x00); /* serial MPEG port */
			/* byte-width start-of-packet */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    CXDTV_HW_SOP_CONTROL,
			    0x47 << 16 | 188 << 4 | 1);
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    CXDTV_TS_SOP_STATUS, IPB_SOP_BYTEWIDE);
			/* serial MPEG port on HD5500 */
			bus_space_write_4(sc->sc_memt, sc->sc_memh,
			    CXDTV_TS_GEN_CONTROL, IPB_SMODE);
		}
		break;
	default:
		break;
	}

	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_LNGTH,
	    CXDTV_TS_PKTSIZE);

	/* Configure for standard MPEG TS, 1 good packet to sync  */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_HW_SOP_CONTROL,
	    0x47 << 16 | 188 << 4 | 1);

	/* zero counter */
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    CXDTV_TS_GP_CNT_CNTRL, 0x03);

	/* enable bad packet interrupt */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_BD_PKT_STATUS,
	0x1000);

	/* enable overflow counter */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_FIFO_OVFL_STAT,
	0x1000);

	/* unmask TS interrupt */
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK,
	    v | CXT_PI_TS_INT);

	/* unmask all TS interrupts */
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK,
	    v | 0x1f1011);

	/* enable RISC DMA engine */
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_DEV_CNTRL2);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_DEV_CNTRL2,
	    v | CXDTV_DEV_CNTRL2_RUN_RISC);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_DMA_CNTRL);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_DMA_CNTRL,
	    v | CXDTV_TS_RISC_EN | CXDTV_TS_FIFO_EN);

	return 0;
}

int
cxdtv_mpeg_halt(struct cxdtv_softc *sc)
{
	uint32_t v;

	CX_DPRINTF(("cxdtv_mpeg_halt\n"));

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_DMA_CNTRL);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_DMA_CNTRL,
	    v & ~(CXDTV_TS_RISC_EN|CXDTV_TS_FIFO_EN));

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_PCI_INT_MASK,
	    v & ~CXT_PI_TS_INT);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK,
	    v & ~0x1f1011);

	return 0;
}

int
cxdtv_mpeg_intr(struct cxdtv_softc *sc)
{
	struct dtv_payload payload;
	uint32_t s, m;

	s = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_STAT);
	m = bus_space_read_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_MASK);
	if ((s & m) == 0)
		return 0;

	if ( (s & ~CXDTV_TS_RISCI) != 0 )
		device_printf(sc->sc_dev, "unexpected TS IS %08x\n", s);

	if (sc->sc_dtvsubmitcb == NULL)
		goto done;

	if ((s & CXDTV_TS_RISCI1) == CXDTV_TS_RISCI1) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma->map,
			0, CXDTV_TS_PKTSIZE,
			BUS_DMASYNC_POSTREAD);
		payload.data = KERNADDR(sc->sc_dma);
		payload.size = CXDTV_TS_PKTSIZE;
		sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);
	}

	if ((s & CXDTV_TS_RISCI2) == CXDTV_TS_RISCI2) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma->map,
			CXDTV_TS_PKTSIZE, CXDTV_TS_PKTSIZE,
			BUS_DMASYNC_POSTREAD);
		payload.data = (char *)(KERNADDR(sc->sc_dma)) + (uintptr_t)CXDTV_TS_PKTSIZE;
		payload.size = CXDTV_TS_PKTSIZE;
		sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);
	}

done:
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_TS_INT_STAT, s);

	return 1;
}

static int
cxdtv_allocmem(struct cxdtv_softc *sc, size_t size, size_t align,
    struct cxdtv_dma *p)
{
	int err;

	p->size = size;
	err = bus_dmamem_alloc(sc->sc_dmat, p->size, align, 0,
	    p->segs, __arraycount(p->segs),
	    &p->nsegs, BUS_DMA_NOWAIT);
	if (err)
		return err;
	err = bus_dmamem_map(sc->sc_dmat, p->segs, p->nsegs, p->size,
	    &p->addr, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (err)
		goto free;
	err = bus_dmamap_create(sc->sc_dmat, p->size, 1, p->size, 0,
	    BUS_DMA_NOWAIT, &p->map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, p->size, NULL,
	    BUS_DMA_NOWAIT);
	if (err)
		goto destroy;

	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return err;
}

static int
cxdtv_freemem(struct cxdtv_softc *sc, struct cxdtv_dma *p)
{

	bus_dmamap_unload(sc->sc_dmat, p->map);
	bus_dmamap_destroy(sc->sc_dmat, p->map);
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return 0;
}

void *
cxdtv_mpeg_malloc(struct cxdtv_softc *sc, size_t size)
{
	struct cxdtv_dma *p;
	int err;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL) {
		return NULL;
	}

	err = cxdtv_allocmem(sc, size, 16, p);
	if (err) {
		kmem_free(p, sizeof(*p));
		device_printf(sc->sc_dev, "not enough memory\n");
		return NULL;
	}

	p->next = sc->sc_dma;
	sc->sc_dma = p;

	return KERNADDR(p);
}

static void
cxdtv_mpeg_free(struct cxdtv_softc *sc, void *addr)
{
	struct cxdtv_dma *p;
	struct cxdtv_dma **pp;

	for (pp = &sc->sc_dma; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == addr) {
			cxdtv_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}

	device_printf(sc->sc_dev, "%p is already free\n", addr);

	return;
}


/* ATI HDTV Wonder */
static void
cxdtv_card_init_hdtvwonder(struct cxdtv_softc *sc)
{
	int i, x;
	i2c_addr_t na;
	uint8_t nb[5][2] = {
	    {0x10, 0x12}, {0x13, 0x04}, {0x16, 0x00},
	    {0x14, 0x04}, {0x17, 0x00}
	};

	/* prepare TUV1236D/TU1236F NIM */

	na = 0x0a; /* Nxt2004 address */
 	x = 0;

	iic_acquire_bus(&sc->sc_i2c, I2C_F_POLL);

	for(i = 0; i < 5; i++)
		x |= iic_exec(&sc->sc_i2c, I2C_OP_WRITE_WITH_STOP, na,
		    nb[i], 2, NULL, 0, I2C_F_POLL);

	iic_release_bus(&sc->sc_i2c, I2C_F_POLL);

	if (x)
		aprint_error_dev(sc->sc_dev, "HDTV Wonder tuner init failed");
}

/* pcHDTV HD5500 */
#define	cxdtv_write_field(_mask, _shift, _value)	\
	(((_value) & (_mask)) << (_shift))

static void
cxdtv_write_gpio(struct cxdtv_softc *sc, uint32_t mask, uint32_t value)
{
	uint32_t v = 0;
	v |= cxdtv_write_field(0xff, 16, mask);
	v |= cxdtv_write_field(0xff, 8, mask);
	v |= cxdtv_write_field(0xff, 0, (mask & value));
	bus_space_write_4(sc->sc_memt, sc->sc_memh, CXDTV_GP0_IO, v);
}

static void
cxdtv_card_init_hd5500(struct cxdtv_softc *sc)
{
	/* hardware (demod) reset */
	cxdtv_write_gpio(sc, 1, 0);
	delay(100000);
	cxdtv_write_gpio(sc, 1, 1);
	delay(200000);
}

MODULE(MODULE_CLASS_DRIVER, cxdtv, "tvpll,nxt2k,lg3303,pci");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
cxdtv_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		return config_init_component(cfdriver_ioconf_cxdtv,
		    cfattach_ioconf_cxdtv, cfdata_ioconf_cxdtv);
#else
		return 0;
#endif
	case MODULE_CMD_FINI:
#ifdef _MODULE
		return config_fini_component(cfdriver_ioconf_cxdtv,
		    cfattach_ioconf_cxdtv, cfdata_ioconf_cxdtv);
#else
		return 0;
#endif
	default:
		return ENOTTY;
	}
}

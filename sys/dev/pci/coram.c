/* $NetBSD: coram.c,v 1.13 2014/03/29 19:28:24 christos Exp $ */

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
__KERNEL_RCSID(0, "$NetBSD: coram.c,v 1.13 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <dev/dtv/dtvif.h>

#include <dev/pci/cx23885reg.h>
#include <dev/pci/coramvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/i2c/i2cvar.h>
#include <dev/i2c/at24cxxvar.h>

#include <dev/i2c/cx24227var.h>
#include <dev/i2c/mt2131var.h>

/* #define CORAM_DEBUG */
/* #define CORAM_ATTACH_I2C */

static const struct coram_board coram_boards[] = {
	{ PCI_VENDOR_HAUPPAUGE, 0x7911, "Hauppauge HVR-1250" },
};

static int coram_match(device_t, cfdata_t, void *);
static void coram_attach(device_t, device_t, void *);
static int coram_detach(device_t, int);
static int coram_rescan(device_t, const char *, const int *);
static void coram_childdet(device_t, device_t);
static bool coram_resume(device_t, const pmf_qual_t *);
static int coram_intr(void *);
static const struct coram_board * coram_board_lookup(uint16_t, uint16_t);

static int coram_iic_exec(void *, i2c_op_t, i2c_addr_t,
    const void *, size_t, void *, size_t, int);
static int coram_iic_acquire_bus(void *, int);
static void coram_iic_release_bus(void *, int);
static int coram_iic_read(struct coram_iic_softc *, i2c_op_t, i2c_addr_t,
    const void *, size_t, void *, size_t, int);
static int coram_iic_write(struct coram_iic_softc *, i2c_op_t, i2c_addr_t,
    const void *, size_t, void *, size_t, int);

static void coram_dtv_get_devinfo(void *, struct dvb_frontend_info *);
static int coram_dtv_open(void *, int);
static void coram_dtv_close(void *);
static int coram_dtv_set_tuner(void *, const struct dvb_frontend_parameters *);
static fe_status_t coram_dtv_get_status(void *);
static uint16_t coram_dtv_get_signal_strength(void *);
static uint16_t coram_dtv_get_snr(void *);
static int coram_dtv_start_transfer(void *, void (*)(void *, const struct dtv_payload *), void *);
static int coram_dtv_stop_transfer(void *);

static int coram_mpeg_attach(struct coram_softc *);
static int coram_mpeg_detach(struct coram_softc *, int);
static int coram_mpeg_reset(struct coram_softc *);
static void * coram_mpeg_malloc(struct coram_softc *, size_t);
static int coram_allocmem(struct coram_softc *, size_t, size_t, struct coram_dma *);
static void coram_mpeg_free(struct coram_softc *, void *);
static int coram_mpeg_halt(struct coram_softc *);
static int coram_freemem(struct coram_softc *, struct coram_dma *);
static int coram_mpeg_trigger(struct coram_softc *, void *);
static int coram_risc_buffer(struct coram_softc *, uint32_t, uint32_t);
static int coram_risc_field(struct coram_softc *, uint32_t *, uint32_t);
static int coram_sram_ch_setup(struct coram_softc *, struct coram_sram_ch *, uint32_t);
static int coram_mpeg_intr(struct coram_softc *);

CFATTACH_DECL2_NEW(coram, sizeof(struct coram_softc),
    coram_match, coram_attach, coram_detach, NULL,
    coram_rescan, coram_childdet);

#define CORAM_SRAM_CH6 0

#define CORAM_TS_PKTSIZE        (188 * 8)

static struct coram_sram_ch coram_sram_chs[] = {
	[CORAM_SRAM_CH6] = {
		.csc_cmds= 0x10140,
		.csc_iq	= 0x10500,
		.csc_iqsz = 0x40,
		.csc_cdt = 0x10600,
		.csc_cdtsz = 0x10,
		.csc_fifo = 0x6000,
		.csc_fifosz = 0x1000,
		.csc_risc = 0x10800,
		.csc_riscsz = 0x800,
		.csc_ptr1 = DMA5_PTR1,
		.csc_ptr2 = DMA5_PTR2,
		.csc_cnt1 = DMA5_CNT1,
		.csc_cnt2 = DMA5_CNT2,
	},
};

static const struct dtv_hw_if coram_dtv_if = {
	.get_devinfo = coram_dtv_get_devinfo,
	.open = coram_dtv_open,
	.close = coram_dtv_close,
	.set_tuner = coram_dtv_set_tuner,
	.get_status = coram_dtv_get_status,
	.get_signal_strength = coram_dtv_get_signal_strength,
	.get_snr = coram_dtv_get_snr,
	.start_transfer = coram_dtv_start_transfer,
	.stop_transfer = coram_dtv_stop_transfer,
};

static int
coram_match(device_t parent, cfdata_t match, void *v)
{
	const struct pci_attach_args *pa = v;
	pcireg_t subid;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CONEXANT)
		return 0;
	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_CONEXANT_CX23885)
		return 0;

	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	if (coram_board_lookup(PCI_VENDOR(subid), PCI_PRODUCT(subid)) == NULL)
		return 0;

	return 1;
}

static void
coram_attach(device_t parent, device_t self, void *aux)
{
	struct coram_softc *sc = device_private(self);
	const struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t reg;
	const char *intrstr;
	struct coram_iic_softc *cic;
	uint32_t value;
	int i;
#ifdef CORAM_ATTACH_I2C
	struct i2cbus_attach_args iba;
#endif
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;

	pci_aprint_devinfo(pa, NULL);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	sc->sc_board = coram_board_lookup(PCI_VENDOR(reg), PCI_PRODUCT(reg));
	KASSERT(sc->sc_board != NULL);

	if (pci_mapreg_map(pa, CX23885_MMBASE, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems)) {
		aprint_error_dev(self, "couldn't map memory space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pa->pa_pc;

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_VM, coram_intr, self);
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

	/* I2C */
	for(i = 0; i < I2C_NUM; i++) {
		cic = &sc->sc_iic[i];

		cic->cic_sc = sc;
		if (bus_space_subregion(sc->sc_memt, sc->sc_memh,
		    I2C_BASE + (I2C_SIZE * i), I2C_SIZE, &cic->cic_regh))
			panic("failed to subregion i2c");

		mutex_init(&cic->cic_busmutex, MUTEX_DRIVER, IPL_NONE);
		cic->cic_i2c.ic_cookie = cic;
		cic->cic_i2c.ic_acquire_bus = coram_iic_acquire_bus;
		cic->cic_i2c.ic_release_bus = coram_iic_release_bus;
		cic->cic_i2c.ic_exec = coram_iic_exec;

#ifdef CORAM_ATTACH_I2C
		/* attach iic(4) */
		memset(&iba, 0, sizeof(iba));
		iba.iba_tag = &cic->cic_i2c;
		iba.iba_type = I2C_TYPE_SMBUS;
		cic->cic_i2cdev = config_found_ia(self, "i2cbus", &iba,
		    iicbus_print);
#endif
	}

	/* HVR1250 GPIO */
	value = bus_space_read_4(sc->sc_memt, sc->sc_memh, 0x110010);
#if 1
	value &= ~0x00010001;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, 0x110010, value);
	delay(5000);
#endif
	value |= 0x00010001;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, 0x110010, value);

#if 0
	int i;
	uint8_t foo[256];
	uint8_t bar;
	bar = 0;
//	seeprom_bootstrap_read(&sc->sc_i2c, 0x50, 0, 256, foo, 256);

	iic_acquire_bus(&sc->sc_i2c, I2C_F_POLL);
	iic_exec(&sc->sc_i2c, I2C_OP_READ_WITH_STOP, 0x50, &bar, 1, foo, 256,
	    I2C_F_POLL);
	iic_release_bus(&sc->sc_i2c, I2C_F_POLL);

	printf("\n");
	for ( i = 0; i < 256; i++) {
		if ( (i % 8) == 0 )
			printf("%02x: ", i);

		printf("%02x", foo[i]);

		if ( (i % 8) == 7 )
			printf("\n");
		else
			printf(" ");
	}
	printf("\n");
#endif

	sc->sc_demod = cx24227_open(sc->sc_dev, &sc->sc_iic[0].cic_i2c, 0x19);
	if (sc->sc_demod == NULL)
		aprint_error_dev(self, "couldn't open cx24227\n");
	sc->sc_tuner = mt2131_open(sc->sc_dev, &sc->sc_iic[0].cic_i2c, 0x61);
	if (sc->sc_tuner == NULL)
		aprint_error_dev(self, "couldn't open mt2131\n");

	coram_mpeg_attach(sc);

	if (!pmf_device_register(self, NULL, coram_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

static int
coram_detach(device_t self, int flags)
{
	struct coram_softc *sc = device_private(self);
	struct coram_iic_softc *cic;
	unsigned int i;
	int error;

	error = coram_mpeg_detach(sc, flags);
	if (error)
		return error;

	if (sc->sc_tuner)
		mt2131_close(sc->sc_tuner);
	if (sc->sc_demod)
		cx24227_close(sc->sc_demod);
	for (i = 0; i < I2C_NUM; i++) {
		cic = &sc->sc_iic[i];
		if (cic->cic_i2cdev)
			config_detach(cic->cic_i2cdev, flags);
		mutex_destroy(&cic->cic_busmutex);
	}
	pmf_device_deregister(self);

	if (sc->sc_mems)
		bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	if (sc->sc_ih)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);

	return 0;
}

static int
coram_rescan(device_t self, const char *ifattr, const int *locs)
{
	struct coram_softc *sc = device_private(self);
	struct dtv_attach_args daa;

	daa.hw = &coram_dtv_if;
	daa.priv = sc;

	if (ifattr_match(ifattr, "dtvbus") && sc->sc_dtvdev == NULL)
		sc->sc_dtvdev = config_found_ia(sc->sc_dev, "dtvbus",
		    &daa, dtv_print);

	return 0;
}
 
static void
coram_childdet(device_t self, device_t child)
{
	struct coram_softc *sc = device_private(self);
	struct coram_iic_softc *cic;
	unsigned int i;

	if (sc->sc_dtvdev == child)
		sc->sc_dtvdev = NULL;

	for (i = 0; i < I2C_NUM; i++) {
		cic = &sc->sc_iic[i];
		if (cic->cic_i2cdev == child)
			cic->cic_i2cdev = NULL;
	}
}

static int
coram_intr(void *v)
{
	device_t self = v;
	struct coram_softc *sc;
	uint32_t val;

	sc = device_private(self);

	val = bus_space_read_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSTAT );
	if (val == 0)
		return 0; /* not ours */

	/* vid c */
	if (val & __BIT(2))
		coram_mpeg_intr(sc);

	if (val & ~__BIT(2))
		printf("%s %08x\n", __func__, val);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_STAT, val);

	return 1;
}

static const struct coram_board *
coram_board_lookup(uint16_t vendor, uint16_t product)
{
	unsigned int i;

	for (i = 0; i < __arraycount(coram_boards); i++) {
		if (coram_boards[i].vendor == vendor &&
		    coram_boards[i].product == product) {
			return &coram_boards[i];
		}
	}

	return NULL;
}

#define CXDTV_TS_RISCI2  (1 << 4)
#define CXDTV_TS_RISCI1  (1 << 0)

#define CXDTV_TS_RISCI (CXDTV_TS_RISCI1|CXDTV_TS_RISCI2)

static int
coram_mpeg_intr(struct coram_softc *sc)
{
	struct dtv_payload payload;
	uint32_t s, m, v;
	int i;

	s = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_INT_STAT);
	m = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK);

	if ((s & m) == 0)
		return 0;

	if ( (s & ~CXDTV_TS_RISCI) != 0 ) {
		printf("%s: unexpected TS IS %08x\n",
		    device_xname(sc->sc_dev), s);

		printf("cmds:\n");
		for(i = 0; i < 20; i++)
		{
			v = bus_space_read_4(sc->sc_memt, sc->sc_memh, 0x10140 +(i*4));
			printf("%06x %08x\n", 0x10140+(i*4), v);
		}
	}

	if (sc->sc_dtvsubmitcb == NULL)
		goto done;

	if ((s & CXDTV_TS_RISCI1) == CXDTV_TS_RISCI1) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma->map,
		    0, CORAM_TS_PKTSIZE,
		    BUS_DMASYNC_POSTREAD);
		payload.data = KERNADDR(sc->sc_dma);
		payload.size = CORAM_TS_PKTSIZE;
		sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);
	}

	if ((s & CXDTV_TS_RISCI2) == CXDTV_TS_RISCI2) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma->map,
		    CORAM_TS_PKTSIZE, CORAM_TS_PKTSIZE,
		    BUS_DMASYNC_POSTREAD);
		payload.data = (char *)(KERNADDR(sc->sc_dma)) + (uintptr_t)CORAM_TS_PKTSIZE;
		payload.size = CORAM_TS_PKTSIZE;
		sc->sc_dtvsubmitcb(sc->sc_dtvsubmitarg, &payload);
	}

done:
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_INT_STAT, s);

	return 1;
}

static bool
coram_resume(device_t dv, const pmf_qual_t *qual)
{
	return true;
}

static int
coram_iic_acquire_bus(void *cookie, int flags)
{
	struct coram_iic_softc *cic;

	cic = cookie;

	if (flags & I2C_F_POLL) {
		while (mutex_tryenter(&cic->cic_busmutex) == 0)
			delay(50);
		return 0;
	}

	mutex_enter(&cic->cic_busmutex);

	return 0;
}

static void
coram_iic_release_bus(void *cookie, int flags)
{
	struct coram_iic_softc *cic;

	cic = cookie;

	mutex_exit(&cic->cic_busmutex);

	return;
}

/* I2C Bus */

#define I2C_ADDR  0x0000
#define I2C_WDATA 0x0004
#define I2C_CTRL  0x0008
#define I2C_RDATA 0x000c
#define I2C_STAT  0x0010

#define I2C_EXTEND  (1 << 3)
#define I2C_NOSTOP  (1 << 4)

static int
coram_iic_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct coram_iic_softc *cic;
	int ret;

	cic = cookie;

	if(cmdlen) {
		ret = coram_iic_write(cic, op, addr, cmdbuf, cmdlen, buf, len, flags);
	if(ret)
		return ret;
	}

	if(len) {
		ret = coram_iic_read(cic, op, addr, cmdbuf, cmdlen, buf, len, flags);
	if(ret)
		return ret;
	}


	return 0;

}

static int
coram_iic_read(struct coram_iic_softc *cic, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	uint8_t *rb;
	uint32_t ctrl;
	int bn;

	rb = buf;

	for ( bn = 0; bn < len; bn++) {
		ctrl = (0x9d << 24) | (1 << 12) | (1 << 2) | 1;
		if ( bn < len - 1 )
			ctrl |= I2C_NOSTOP | I2C_EXTEND;

		bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_ADDR, addr<<25);
	        bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_CTRL, ctrl);

		while((bus_space_read_4(cic->cic_sc->sc_memt, cic->cic_regh,
		    I2C_STAT) & 0x02)) {
			delay(25);
		}
		if((bus_space_read_4(cic->cic_sc->sc_memt, cic->cic_regh,
		    I2C_STAT) & 0x01) == 0x00) {
//			printf("%s %d no ack\n", __func__, bn);
			return EIO;
		}

		rb[bn] = bus_space_read_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_RDATA);

	}

	return 0;
}

static int
coram_iic_write(struct coram_iic_softc *cic, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	const uint8_t *wb;
	uint32_t wdata, addrreg, ctrl;
	int bn;

	wb = cmdbuf;

	addrreg = (addr << 25) | wb[0];
	wdata = wb[0];
	ctrl = (0x9d << 24) | (1 << 12) | (1 << 2);

	if ( cmdlen > 1 )
		ctrl |= I2C_NOSTOP | I2C_EXTEND;
	else if (len)
		ctrl |= I2C_NOSTOP;

	bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_ADDR, addrreg);
	bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_WDATA, wdata);
	bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_CTRL, ctrl);

	while((bus_space_read_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_STAT) & 0x02)) {
		delay(25); }

	for ( bn = 1; bn < cmdlen; bn++) {
		ctrl = (0x9d << 24) | (1 << 12) | (1 << 2);
		wdata = wb[bn];

		if ( bn < cmdlen - 1 )
			ctrl |= I2C_NOSTOP | I2C_EXTEND;
		else if (len)
			ctrl |= I2C_NOSTOP;

		bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_ADDR, addrreg);
		bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_WDATA, wdata);
		bus_space_write_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_CTRL, ctrl);

		while((bus_space_read_4(cic->cic_sc->sc_memt, cic->cic_regh, I2C_STAT) & 0x02)) {
			delay(25); }
	}

	return 0;
}

static int
coram_mpeg_attach(struct coram_softc *sc)
{
	struct coram_sram_ch *ch;

	ch = &coram_sram_chs[CORAM_SRAM_CH6];

	sc->sc_riscbufsz = ch->csc_riscsz;
	sc->sc_riscbuf = kmem_alloc(ch->csc_riscsz, KM_SLEEP);

	if ( sc->sc_riscbuf == NULL )
		panic("riscbuf null");

	coram_mpeg_reset(sc);

	sc->sc_tsbuf = NULL;

	coram_rescan(sc->sc_dev, NULL, NULL);

	return (sc->sc_dtvdev != NULL);
}

static int
coram_mpeg_detach(struct coram_softc *sc, int flags)
{
	struct coram_sram_ch *ch = &coram_sram_chs[CORAM_SRAM_CH6];
	int error;

	if (sc->sc_dtvdev) {
		error = config_detach(sc->sc_dtvdev, flags);
		if (error)
			return error;
	}
	if (sc->sc_riscbuf) {
		kmem_free(sc->sc_riscbuf, ch->csc_riscsz);
	}

	return 0;
}

static void
coram_dtv_get_devinfo(void *cookie, struct dvb_frontend_info *info)
{
	struct coram_softc *sc = cookie;

	memset(info, 0, sizeof(*info));
	strlcpy(info->name, sc->sc_board->name, sizeof(info->name));
	info->type = FE_ATSC;
	info->frequency_min = 54000000;
	info->frequency_max = 858000000;
	info->frequency_stepsize = 62500;
	info->caps = FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_8VSB;
}

static int
coram_dtv_open(void *cookie, int flags)
{
	struct coram_softc *sc = cookie;

#ifdef CORAM_DEBUG
	device_printf(sc->sc_dev, "%s\n", __func__);
#endif

	//KASSERT(sc->sc_tsbuf == NULL);

	if (sc->sc_tuner == NULL || sc->sc_demod == NULL)
		return ENXIO;

	coram_mpeg_reset(sc);

	/* allocate two alternating DMA areas for MPEG TS packets */
	sc->sc_tsbuf = coram_mpeg_malloc(sc, CORAM_TS_PKTSIZE * 2);

	if (sc->sc_tsbuf == NULL)
		return ENOMEM;

	return 0;
}

static void
coram_dtv_close(void *cookie)
{
	struct coram_softc *sc = cookie;

#ifdef CORAM_DEBUG
	device_printf(sc->sc_dev, "%s\n", __func__);
#endif

	coram_mpeg_halt(sc);

	if (sc->sc_tsbuf != NULL) {
		coram_mpeg_free(sc, sc->sc_tsbuf);
		sc->sc_tsbuf = NULL;
	}
}

static int
coram_dtv_set_tuner(void *cookie, const struct dvb_frontend_parameters *params)
{
	struct coram_softc *sc = cookie;

	KASSERT(sc->sc_tuner != NULL);
	mt2131_tune_dtv(sc->sc_tuner, params);
	KASSERT(sc->sc_demod != NULL);
	return cx24227_set_modulation(sc->sc_demod, params->u.vsb.modulation);
}

static fe_status_t
coram_dtv_get_status(void *cookie)
{
	struct coram_softc *sc = cookie;

	if (sc->sc_demod == NULL)
		return ENXIO;

	return cx24227_get_dtv_status(sc->sc_demod);;
}

static uint16_t
coram_dtv_get_signal_strength(void *cookie)
{
	return 0;
}

static uint16_t
coram_dtv_get_snr(void *cookie)
{
	return 0;
}

static int
coram_dtv_start_transfer(void *cookie,
    void (*cb)(void *, const struct dtv_payload *), void *arg)
{
	struct coram_softc *sc = cookie;

#ifdef CORAM_DEBUG
	device_printf(sc->sc_dev, "%s\n", __func__);
#endif

	sc->sc_dtvsubmitcb = cb;
	sc->sc_dtvsubmitarg = arg;

	coram_mpeg_trigger(sc, sc->sc_tsbuf);

	return 0;
}

static int
coram_dtv_stop_transfer(void *cookie)
{
	struct coram_softc *sc = cookie;

#ifdef CORAM_DEBUG
	device_printf(sc->sc_dev, "%s\n", __func__);
#endif

	coram_mpeg_halt(sc);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK, 0);

	sc->sc_dtvsubmitcb = NULL;
	sc->sc_dtvsubmitarg = NULL;

	return 0;
}


static int
coram_mpeg_reset(struct coram_softc *sc)
{
	/* hold RISC in reset */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, DEV_CNTRL2, 0);

	/* disable fifo + risc */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_DMA_CTL, 0);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK, 0);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_STAT, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_INT_STAT, 0);

	memset(sc->sc_riscbuf, 0, sc->sc_riscbufsz);

	return 0;
}

static void *
coram_mpeg_malloc(struct coram_softc *sc, size_t size)
{
	struct coram_dma *p;
	int err;

	p = kmem_alloc(sizeof(struct coram_dma), KM_SLEEP);
	if ( p == NULL )
		return NULL;
	err = coram_allocmem(sc, size, 16, p);
	if (err) {
		kmem_free(p, sizeof(struct coram_dma));
		return NULL;
	}

	p->next = sc->sc_dma;
	sc->sc_dma = p;

	return KERNADDR(p);
}

static int
coram_allocmem(struct coram_softc *sc, size_t size, size_t align,
    struct coram_dma *p)
{
	int err;

	p->size = size;
	err = bus_dmamem_alloc(sc->sc_dmat, p->size, align, 0,
	    p->segs, sizeof(p->segs) / sizeof(p->segs[0]),
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
coram_mpeg_halt(struct coram_softc *sc)
{
	uint32_t v;

#ifdef CORAM_DEBUG
	device_printf(sc->sc_dev, "%s\n", __func__);
#endif

	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_DMA_CTL, 0);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK,
	    v & __BIT(2));

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK,
	    v & 0);

	return 0;
}

static void
coram_mpeg_free(struct coram_softc *sc, void *addr)
{
	struct coram_dma *p;
	struct coram_dma **pp;

	for (pp = &sc->sc_dma; (p = *pp) != NULL; pp = &p->next)
		if (KERNADDR(p) == addr) {
			coram_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(struct coram_dma));
			return;
		}

	printf("%s: %p is already free\n", device_xname(sc->sc_dev), addr);
	return;
}

static int
coram_freemem(struct coram_softc *sc, struct coram_dma *p)
{
	bus_dmamap_unload(sc->sc_dmat, p->map);
	bus_dmamap_destroy(sc->sc_dmat, p->map);
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return 0;
}

static int
coram_mpeg_trigger(struct coram_softc *sc, void *buf)
{
	struct coram_dma *p;
	struct coram_sram_ch *ch;
	uint32_t v;

	ch = &coram_sram_chs[CORAM_SRAM_CH6];

	for (p = sc->sc_dma; p && KERNADDR(p) != buf; p = p->next)
		continue;
	if (p == NULL) {
		printf("%s: coram_mpeg_trigger: bad addr %p\n",
		    device_xname(sc->sc_dev), buf);
		return ENOENT;
	}

	/* disable fifo + risc */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_DMA_CTL, 0);

	coram_risc_buffer(sc, CORAM_TS_PKTSIZE, 1);
	coram_sram_ch_setup(sc, ch, CORAM_TS_PKTSIZE);

	/* let me hope this bit is the same as on the 2388[0-3] */
	/* software reset */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_GEN_CTL, 0x0040);
	delay (100*1000);

	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_LNGTH, CORAM_TS_PKTSIZE);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_HW_SOP_CTL, 0x47 << 16 | 188 << 4);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_TS_CLK_EN, 1);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_VLD_MISC, 0);
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_GEN_CTL, 12);
	delay (100*1000);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, PAD_CTRL);
	v &= ~0x4; /* Clear TS2_SOP_OE */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, PAD_CTRL, v);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK);
	v |= 0x111111;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_INT_MSK, v);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_DMA_CTL);
	v |= 0x11; /* Enable RISC controller and FIFO */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, VID_C_DMA_CTL, v);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, DEV_CNTRL2);
	v |= __BIT(5); /* Enable RISC controller */
	bus_space_write_4(sc->sc_memt, sc->sc_memh, DEV_CNTRL2, v);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK);
	v |= 0x001f00;
	v |= 0x04;
	bus_space_write_4(sc->sc_memt, sc->sc_memh, PCI_INT_MSK, v);

	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_GEN_CTL);
#ifdef CORAM_DEBUG
	printf("%s, %06x %08x\n", __func__, VID_C_GEN_CTL, v);
#endif
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_SOP_STATUS);
#ifdef CORAM_DEBUG
	printf("%s, %06x %08x\n", __func__, VID_C_SOP_STATUS, v);
#endif
	delay(100*1000);
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_GEN_CTL);
#ifdef CORAM_DEBUG
	printf("%s, %06x %08x\n", __func__, VID_C_GEN_CTL, v);
#endif
	v = bus_space_read_4(sc->sc_memt, sc->sc_memh, VID_C_SOP_STATUS);
#ifdef CORAM_DEBUG
	printf("%s, %06x %08x\n", __func__, VID_C_SOP_STATUS, v);
#endif

	return 0;
}

static int
coram_risc_buffer(struct coram_softc *sc, uint32_t bpl, uint32_t lines)
{
	uint32_t *rm;
	uint32_t size;

	size = 1 + (bpl * lines) / PAGE_SIZE + lines;
	size += 2;

	if (sc->sc_riscbuf == NULL) {
		return ENOMEM;
	}

	rm = (uint32_t *)sc->sc_riscbuf;
	coram_risc_field(sc, rm, bpl);

	return 0;
}

static int
coram_risc_field(struct coram_softc *sc, uint32_t *rm, uint32_t bpl)
{
	struct coram_dma *p;

	for (p = sc->sc_dma; p && KERNADDR(p) != sc->sc_tsbuf; p = p->next)
		continue;
	if (p == NULL) {
		printf("%s: coram_risc_field: bad addr %p\n",
		    device_xname(sc->sc_dev), sc->sc_tsbuf);
		return ENOENT;
	}

	memset(sc->sc_riscbuf, 0, sc->sc_riscbufsz);

	rm = sc->sc_riscbuf;

	/* htole32 will be done when program is copied to chip sram */

	/* XXX */
	*(rm++) = (CX_RISC_SYNC|0);

	*(rm++) = (CX_RISC_WRITE|CX_RISC_SOL|CX_RISC_EOL|CX_RISC_IRQ1|bpl);
	*(rm++) = (DMAADDR(p) + 0 * bpl);
	*(rm++) = 0; /* high dword */

	*(rm++) = (CX_RISC_WRITE|CX_RISC_SOL|CX_RISC_EOL|CX_RISC_IRQ2|bpl);
	*(rm++) = (DMAADDR(p) + 1 * bpl);
	*(rm++) = 0;

	*(rm++) = (CX_RISC_JUMP|1);
	*(rm++) = (coram_sram_chs[CORAM_SRAM_CH6].csc_risc + 4);
	*(rm++) = 0;

	return 0;
}

static int
coram_sram_ch_setup(struct coram_softc *sc, struct coram_sram_ch *csc,
    uint32_t bpl)
{
	unsigned int i, lines;
	uint32_t cdt;

	/* XXX why round? */
	bpl = (bpl + 7) & ~7;
	cdt = csc->csc_cdt;
	lines = csc->csc_fifosz / bpl;
#ifdef CORAM_DEBUG
	printf("%s %d lines\n", __func__, lines);
#endif

	/* fill in CDT */
	for (i = 0; i < lines; i++) {
#ifdef CORAM_DEBUG
		printf("CDT ent %08x, %08x\n", cdt + (16 * i),
		    csc->csc_fifo + (bpl * i));
#endif
		bus_space_write_4(sc->sc_memt, sc->sc_memh,
		    cdt + (16 * i), csc->csc_fifo + (bpl * i));
	}

	/* copy program */
	/* converts program to little endian as it goes into sram */
	bus_space_write_region_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_risc, (void *)sc->sc_riscbuf, sc->sc_riscbufsz >> 2);

	/* fill in CMDS */
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_IRPC, csc->csc_risc);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_IRPC + 4, 0);

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_CDTB, csc->csc_cdt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_CDTS, (lines * 16) >> 3); /* XXX magic */

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_IQB, csc->csc_iq);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cmds + CMDS_O_IQS,
	    CMDS_IQS_ISRP | (csc->csc_iqsz >> 2) );

	/* zero rest of CMDS */
	bus_space_set_region_4(sc->sc_memt, sc->sc_memh, 0x18, 0, 20);

	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_ptr1, csc->csc_fifo);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_ptr2, cdt);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cnt2, (lines * 16) >> 3);
	bus_space_write_4(sc->sc_memt, sc->sc_memh,
	    csc->csc_cnt1, (bpl >> 3) - 1);

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, coram, "cx24227,mt2131,pci");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
coram_modcmd(modcmd_t cmd, void *v)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_coram,
		    cfattach_ioconf_coram, cfdata_ioconf_coram);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_coram,
		    cfattach_ioconf_coram, cfdata_ioconf_coram);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

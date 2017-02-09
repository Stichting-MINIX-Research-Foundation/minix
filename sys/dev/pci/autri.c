/*	$NetBSD: autri.c,v 1.52 2014/08/01 16:41:58 joerg Exp $	*/

/*
 * Copyright (c) 2001 SOMEYA Yoshihiko and KUROSAWA Takahiro.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Trident 4DWAVE-DX/NX, SiS 7018, ALi M5451 Sound Driver
 *
 * The register information is taken from the ALSA driver.
 *
 * Documentation links:
 * - ftp://ftp.alsa-project.org/pub/manuals/trident/
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: autri.c,v 1.52 2014/08/01 16:41:58 joerg Exp $");

#include "midi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/audioio.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>
#include <dev/ic/mpuvar.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/autrireg.h>
#include <dev/pci/autrivar.h>

#ifdef AUDIO_DEBUG
# define DPRINTF(x)	if (autridebug) printf x
# define DPRINTFN(n,x)	if (autridebug > (n)) printf x
int autridebug = 0;
#else
# define DPRINTF(x)
# define DPRINTFN(n,x)
#endif

static int	autri_intr(void *);

#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

static int     autri_allocmem(struct autri_softc *, size_t,
			      size_t, struct autri_dma *);
static int     autri_freemem(struct autri_softc *, struct autri_dma *);

#define TWRITE1(sc, r, x) bus_space_write_1((sc)->memt, (sc)->memh, (r), (x))
#define TWRITE2(sc, r, x) bus_space_write_2((sc)->memt, (sc)->memh, (r), (x))
#define TWRITE4(sc, r, x) bus_space_write_4((sc)->memt, (sc)->memh, (r), (x))
#define TREAD1(sc, r) bus_space_read_1((sc)->memt, (sc)->memh, (r))
#define TREAD2(sc, r) bus_space_read_2((sc)->memt, (sc)->memh, (r))
#define TREAD4(sc, r) bus_space_read_4((sc)->memt, (sc)->memh, (r))

static int	autri_attach_codec(void *, struct ac97_codec_if *);
static int	autri_read_codec(void *, uint8_t, uint16_t *);
static int	autri_write_codec(void *, uint8_t, uint16_t);
static int	autri_reset_codec(void *);
static enum ac97_host_flags	autri_flags_codec(void *);

static bool autri_resume(device_t, const pmf_qual_t *);
static int  autri_init(void *);
static struct autri_dma *autri_find_dma(struct autri_softc *, void *);
static void autri_setup_channel(struct autri_softc *, int,
				const audio_params_t *param);
static void autri_enable_interrupt(struct autri_softc *, int);
static void autri_disable_interrupt(struct autri_softc *, int);
static void autri_startch(struct autri_softc *, int, int);
static void autri_stopch(struct autri_softc *, int, int);
static void autri_enable_loop_interrupt(void *);
#if 0
static void autri_disable_loop_interrupt(void *);
#endif

static int	autri_open(void *, int);
static int	autri_query_encoding(void *, struct audio_encoding *);
static int	autri_set_params(void *, int, int, audio_params_t *,
				 audio_params_t *, stream_filter_list_t *,
				 stream_filter_list_t *);
static int	autri_round_blocksize(void *, int, int, const audio_params_t *);
static int	autri_trigger_output(void *, void *, void *, int,
				     void (*)(void *), void *,
				     const audio_params_t *);
static int	autri_trigger_input(void *, void *, void *, int,
				    void (*)(void *), void *,
				    const audio_params_t *);
static int	autri_halt_output(void *);
static int	autri_halt_input(void *);
static int	autri_getdev(void *, struct audio_device *);
static int	autri_mixer_set_port(void *, mixer_ctrl_t *);
static int	autri_mixer_get_port(void *, mixer_ctrl_t *);
static void*	autri_malloc(void *, int, size_t);
static void	autri_free(void *, void *, size_t);
static size_t	autri_round_buffersize(void *, int, size_t);
static paddr_t autri_mappage(void *, void *, off_t, int);
static int	autri_get_props(void *);
static int	autri_query_devinfo(void *, mixer_devinfo_t *);
static void	autri_get_locks(void *, kmutex_t **, kmutex_t **);

static const struct audio_hw_if autri_hw_if = {
	autri_open,
	NULL,			/* close */
	NULL,			/* drain */
	autri_query_encoding,
	autri_set_params,
	autri_round_blocksize,
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	autri_halt_output,
	autri_halt_input,
	NULL,			/* speaker_ctl */
	autri_getdev,
	NULL,			/* setfd */
	autri_mixer_set_port,
	autri_mixer_get_port,
	autri_query_devinfo,
	autri_malloc,
	autri_free,
	autri_round_buffersize,
	autri_mappage,
	autri_get_props,
	autri_trigger_output,
	autri_trigger_input,
	NULL,			/* dev_ioctl */
	autri_get_locks,
};

#if NMIDI > 0
static void	autri_midi_close(void *);
static void	autri_midi_getinfo(void *, struct midi_info *);
static int	autri_midi_open(void *, int, void (*)(void *, int),
			   void (*)(void *), void *);
static int	autri_midi_output(void *, int);

static const struct midi_hw_if autri_midi_hw_if = {
	autri_midi_open,
	autri_midi_close,
	autri_midi_output,
	autri_midi_getinfo,
	NULL,			/* ioctl */
	autri_get_locks,
};
#endif

#define AUTRI_NFORMATS	8
static const struct audio_format autri_formats[AUTRI_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
};

/*
 * register set/clear bit
 */
#if NMIDI > 0
static inline void
autri_reg_set_1(struct autri_softc *sc, int no, uint8_t mask)
{
	bus_space_write_1(sc->memt, sc->memh, no,
	    (bus_space_read_1(sc->memt, sc->memh, no) | mask));
}

static inline void
autri_reg_clear_1(struct autri_softc *sc, int no, uint8_t mask)
{
	bus_space_write_1(sc->memt, sc->memh, no,
	    (bus_space_read_1(sc->memt, sc->memh, no) & ~mask));
}
#endif

static inline void
autri_reg_set_4(struct autri_softc *sc, int no, uint32_t mask)
{
	bus_space_write_4(sc->memt, sc->memh, no,
	    (bus_space_read_4(sc->memt, sc->memh, no) | mask));
}

static inline void
autri_reg_clear_4(struct autri_softc *sc, int no, uint32_t mask)
{
	bus_space_write_4(sc->memt, sc->memh, no,
	    (bus_space_read_4(sc->memt, sc->memh, no) & ~mask));
}

/*
 * AC'97 codec
 */
static int
autri_attach_codec(void *sc_, struct ac97_codec_if *codec_if)
{
	struct autri_codec_softc *sc;

	DPRINTF(("autri_attach_codec()\n"));
	sc = sc_;
	sc->codec_if = codec_if;
	return 0;
}

static int
autri_read_codec(void *sc_, uint8_t index, uint16_t *data)
{
	struct autri_codec_softc *codec;
	struct autri_softc *sc;
	uint32_t status, addr, cmd, busy;
	uint16_t count;

	codec = sc_;
	sc = codec->sc;
	/*DPRINTF(("sc->sc->type : 0x%X",sc->sc->type));*/

	switch (sc->sc_devid) {
	case AUTRI_DEVICE_ID_4DWAVE_DX:
		addr = AUTRI_DX_ACR1;
		cmd  = AUTRI_DX_ACR1_CMD_READ;
		busy = AUTRI_DX_ACR1_BUSY_READ;
		break;
	case AUTRI_DEVICE_ID_4DWAVE_NX:
		addr = AUTRI_NX_ACR2;
		cmd  = AUTRI_NX_ACR2_CMD_READ;
		busy = AUTRI_NX_ACR2_BUSY_READ | AUTRI_NX_ACR2_RECV_WAIT;
		break;
	case AUTRI_DEVICE_ID_SIS_7018:
		addr = AUTRI_SIS_ACRD;
		cmd  = AUTRI_SIS_ACRD_CMD_READ;
		busy = AUTRI_SIS_ACRD_BUSY_READ | AUTRI_SIS_ACRD_AUDIO_BUSY;
		break;
	case AUTRI_DEVICE_ID_ALI_M5451:
		if (sc->sc_revision > 0x01)
			addr = AUTRI_ALI_ACWR;
		else
			addr = AUTRI_ALI_ACRD;
		cmd  = AUTRI_ALI_ACRD_CMD_READ;
		busy = AUTRI_ALI_ACRD_BUSY_READ;
		break;
	default:
		printf("%s: autri_read_codec : unknown device\n",
		       device_xname(sc->sc_dev));
		return -1;
	}

	/* wait for 'Ready to Read' */
	for (count=0; count<0xffff; count++) {
		if ((TREAD4(sc, addr) & busy) == 0)
			break;
	}

	if (count == 0xffff) {
		printf("%s: Codec timeout. Busy reading AC'97 codec.\n",
		       device_xname(sc->sc_dev));
		return -1;
	}

	/* send Read Command to AC'97 */
	TWRITE4(sc, addr, (index & 0x7f) | cmd);

	/* wait for 'Returned data is avalable' */
	for (count=0; count<0xffff; count++) {
		status = TREAD4(sc, addr);
		if ((status & busy) == 0)
			break;
	}

	if (count == 0xffff) {
		printf("%s: Codec timeout. Busy reading AC'97 codec.\n",
		       device_xname(sc->sc_dev));
		return -1;
	}

	*data =  (status >> 16) & 0x0000ffff;
	/*DPRINTF(("autri_read_codec(0x%X) return 0x%X\n",reg,*data));*/
	return 0;
}

static int
autri_write_codec(void *sc_, uint8_t index, uint16_t data)
{
	struct autri_codec_softc *codec;
	struct autri_softc *sc;
	uint32_t addr, cmd, busy;
	uint16_t count;

	codec = sc_;
	sc = codec->sc;
	/*DPRINTF(("autri_write_codec(0x%X,0x%X)\n",index,data));*/

	switch (sc->sc_devid) {
	case AUTRI_DEVICE_ID_4DWAVE_DX:
		addr = AUTRI_DX_ACR0;
		cmd  = AUTRI_DX_ACR0_CMD_WRITE;
		busy = AUTRI_DX_ACR0_BUSY_WRITE;
		break;
	case AUTRI_DEVICE_ID_4DWAVE_NX:
		addr = AUTRI_NX_ACR1;
		cmd  = AUTRI_NX_ACR1_CMD_WRITE;
		busy = AUTRI_NX_ACR1_BUSY_WRITE;
		break;
	case AUTRI_DEVICE_ID_SIS_7018:
		addr = AUTRI_SIS_ACWR;
		cmd  = AUTRI_SIS_ACWR_CMD_WRITE;
		busy = AUTRI_SIS_ACWR_BUSY_WRITE | AUTRI_SIS_ACWR_AUDIO_BUSY;
		break;
	case AUTRI_DEVICE_ID_ALI_M5451:
		addr = AUTRI_ALI_ACWR;
		cmd  = AUTRI_ALI_ACWR_CMD_WRITE;
		if (sc->sc_revision > 0x01)
			cmd  |= 0x0100;
		busy = AUTRI_ALI_ACWR_BUSY_WRITE;
		break;
	default:
		printf("%s: autri_write_codec : unknown device.\n",
		       device_xname(sc->sc_dev));
		return -1;
	}

	/* wait for 'Ready to Write' */
	for (count=0; count<0xffff; count++) {
		if ((TREAD4(sc, addr) & busy) == 0)
			break;
	}

	if (count == 0xffff) {
		printf("%s: Codec timeout. Busy writing AC'97 codec\n",
		       device_xname(sc->sc_dev));
		return -1;
	}

	/* send Write Command to AC'97 */
	TWRITE4(sc, addr, (data << 16) | (index & 0x7f) | cmd);

	return 0;
}

static int
autri_reset_codec(void *sc_)
{
	struct autri_codec_softc *codec;
	struct autri_softc *sc;
	uint32_t reg, ready;
	int addr, count;

	codec = sc_;
	sc = codec->sc;
	count = 200;
	DPRINTF(("autri_reset_codec(codec=%p,sc=%p)\n", codec, sc));
	DPRINTF(("sc->sc_devid=%X\n", sc->sc_devid));

	switch (sc->sc_devid) {
	case AUTRI_DEVICE_ID_4DWAVE_DX:
		/* warm reset AC'97 codec */
		autri_reg_set_4(sc, AUTRI_DX_ACR2, 1);
		delay(100);
		/* release reset */
		autri_reg_clear_4(sc, AUTRI_DX_ACR2, 1);
		delay(100);

		addr = AUTRI_DX_ACR2;
		ready = AUTRI_DX_ACR2_CODEC_READY;
		break;
	case AUTRI_DEVICE_ID_4DWAVE_NX:
		/* warm reset AC'97 codec */
		autri_reg_set_4(sc, AUTRI_NX_ACR0, 1);
		delay(100);
		/* release reset */
		autri_reg_clear_4(sc, AUTRI_NX_ACR0, 1);
		delay(100);

		addr = AUTRI_NX_ACR0;
		ready = AUTRI_NX_ACR0_CODEC_READY;
		break;
	case AUTRI_DEVICE_ID_SIS_7018:
		/* cold reset AC'97 codec */
		autri_reg_set_4(sc, AUTRI_SIS_SCTRL, 2);
		delay(1000);
		/* release reset (warm & cold) */
		autri_reg_clear_4(sc, AUTRI_SIS_SCTRL, 3);
		delay(2000);

		addr = AUTRI_SIS_SCTRL;
		ready = AUTRI_SIS_SCTRL_CODEC_READY;
		break;
	case AUTRI_DEVICE_ID_ALI_M5451:
		/* warm reset AC'97 codec */
		autri_reg_set_4(sc, AUTRI_ALI_SCTRL, 1);
		delay(100);
		/* release reset (warm & cold) */
		autri_reg_clear_4(sc, AUTRI_ALI_SCTRL, 3);
		delay(100);

		addr = AUTRI_ALI_SCTRL;
		ready = AUTRI_ALI_SCTRL_CODEC_READY;
		break;
	default:
		printf("%s: autri_reset_codec : unknown device\n",
		       device_xname(sc->sc_dev));
		return EOPNOTSUPP;
	}

	/* wait for 'Codec Ready' */
	while (count--) {
		reg = TREAD4(sc, addr);
		if (reg & ready)
			break;
		delay(1000);
	}

	if (count == 0) {
		printf("%s: Codec timeout. AC'97 is not ready for operation.\n",
		       device_xname(sc->sc_dev));
		return ETIMEDOUT;
	}
	return 0;
}

static enum ac97_host_flags
autri_flags_codec(void *sc)
{
	return AC97_HOST_DONT_READ;
}

/*
 *
 */

static int
autri_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_TRIDENT:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_TRIDENT_4DWAVE_DX:
			/*
			 * IBM makes a pcn network card and improperly
			 * sets the vendor and product ID's.  Avoid matching.
			 */
			if (PCI_CLASS(pa->pa_class) == PCI_CLASS_NETWORK)
				return 0;
		/* FALLTHROUGH */
		case PCI_PRODUCT_TRIDENT_4DWAVE_NX:
			return 1;
		}
		break;
	case PCI_VENDOR_SIS:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_SIS_7018:
			return 1;
		}
		break;
	case PCI_VENDOR_ALI:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ALI_M5451:
			return 1;
		}
		break;
	}

	return 0;
}

static void
autri_attach(device_t parent, device_t self, void *aux)
{
	struct autri_softc *sc;
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	struct autri_codec_softc *codec;
	pci_intr_handle_t ih;
	char const *intrstr;
	int r;
	uint32_t reg;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;

	sc->sc_devid = pa->pa_id;
	sc->sc_class = pa->pa_class;

	pci_aprint_devinfo(pa, "Audio controller");
	sc->sc_revision = PCI_REVISION(pa->pa_class);

	/* map register to memory */
	if (pci_mapreg_map(pa, AUTRI_PCI_MEMORY_BASE,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->memt, &sc->memh, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "can't map memory space\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* map and establish the interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, autri_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	sc->sc_dmatag = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pt = pa->pa_tag;

	/* enable the device */
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= (PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);

	/* initialize the device */
	autri_init(sc);

	/* attach AC'97 codec */
	codec = &sc->sc_codec;
	codec->sc = sc;

	codec->host_if.arg = codec;
	codec->host_if.attach = autri_attach_codec;
	codec->host_if.reset = autri_reset_codec;
	codec->host_if.read = autri_read_codec;
	codec->host_if.write = autri_write_codec;
	codec->host_if.flags = autri_flags_codec;

	r = ac97_attach(&codec->host_if, self, &sc->sc_lock);
	if (r != 0) {
		aprint_error_dev(sc->sc_dev, "can't attach codec (error 0x%X)\n", r);
		return;
	}

	if (!pmf_device_register(self, NULL, autri_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	audio_attach_mi(&autri_hw_if, sc, sc->sc_dev);

#if NMIDI > 0
	midi_attach_mi(&autri_midi_hw_if, sc, sc->sc_dev);
#endif
}

CFATTACH_DECL_NEW(autri, sizeof(struct autri_softc),
    autri_match, autri_attach, NULL, NULL);

static bool
autri_resume(device_t dv, const pmf_qual_t *qual)
{
	struct autri_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	autri_init(sc);
	mutex_spin_exit(&sc->sc_intr_lock);
	(sc->sc_codec.codec_if->vtbl->restore_ports)(sc->sc_codec.codec_if);
	mutex_exit(&sc->sc_lock);

	return true;
}

static int
autri_init(void *sc_)
{
	struct autri_softc *sc;
	uint32_t reg;
	pci_chipset_tag_t pc;
	pcitag_t pt;

	sc = sc_;
	pc = sc->sc_pc;
	pt = sc->sc_pt;
	DPRINTF(("in autri_init()\n"));
	DPRINTFN(5,("pci_conf_read(0x40) : 0x%X\n",pci_conf_read(pc,pt,0x40)));
	DPRINTFN(5,("pci_conf_read(0x44) : 0x%X\n",pci_conf_read(pc,pt,0x44)));

	switch (sc->sc_devid) {
	case AUTRI_DEVICE_ID_4DWAVE_DX:
		/* disable Legacy Control */
		pci_conf_write(pc, pt, AUTRI_PCI_DDMA_CFG,0);
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & 0xffff0000);
		delay(100);
		/* audio engine reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg | 0x00040000);
		delay(100);
		/* release reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & ~0x00040000);
		delay(100);
		/* DAC on */
		autri_reg_set_4(sc,AUTRI_DX_ACR2,0x02);
		break;
	case AUTRI_DEVICE_ID_4DWAVE_NX:
		/* disable Legacy Control */
		pci_conf_write(pc, pt, AUTRI_PCI_DDMA_CFG,0);
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & 0xffff0000);
		delay(100);
		/* audio engine reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg | 0x00010000);
		delay(100);
		/* release reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & ~0x00010000);
		delay(100);
		/* DAC on */
		autri_reg_set_4(sc,AUTRI_NX_ACR0,0x02);
		break;
	case AUTRI_DEVICE_ID_SIS_7018:
		/* disable Legacy Control */
		pci_conf_write(pc, pt, AUTRI_PCI_DDMA_CFG,0);
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & 0xffff0000);
		delay(100);
		/* reset Digital Controller */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg | 0x000c0000);
		delay(100);
		/* release reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & ~0x00040000);
		delay(100);
		/* disable AC97 GPIO interrupt */
		TWRITE1(sc, AUTRI_SIS_ACGPIO, 0);
		/* enable 64 channel mode */
		autri_reg_set_4(sc, AUTRI_LFO_GC_CIR, BANK_B_EN);
		break;
	case AUTRI_DEVICE_ID_ALI_M5451:
		/* disable Legacy Control */
		pci_conf_write(pc, pt, AUTRI_PCI_DDMA_CFG,0);
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & 0xffff0000);
		delay(100);
		/* reset Digital Controller */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg | 0x000c0000);
		delay(100);
		/* release reset */
		reg = pci_conf_read(pc, pt, AUTRI_PCI_LEGACY_IOBASE);
		pci_conf_write(pc, pt, AUTRI_PCI_LEGACY_IOBASE, reg & ~0x00040000);
		delay(100);
		/* enable PCM input */
		autri_reg_set_4(sc, AUTRI_ALI_GCONTROL, AUTRI_ALI_GCONTROL_PCM_IN);
		break;
	}

	if (sc->sc_devid == AUTRI_DEVICE_ID_ALI_M5451) {
		sc->sc_play.ch      = 0;
		sc->sc_play.ch_intr = 1;
		sc->sc_rec.ch       = 31;
		sc->sc_rec.ch_intr  = 2;
	} else {
		sc->sc_play.ch      = 0x20;
		sc->sc_play.ch_intr = 0x21;
		sc->sc_rec.ch       = 0x22;
		sc->sc_rec.ch_intr  = 0x23;
	}

	/* clear channel status */
	TWRITE4(sc, AUTRI_STOP_A, 0xffffffff);
	TWRITE4(sc, AUTRI_STOP_B, 0xffffffff);

	/* disable channel interrupt */
	TWRITE4(sc, AUTRI_AINTEN_A, 0);
	TWRITE4(sc, AUTRI_AINTEN_B, 0);

#if 0
	/* TLB */
	if (sc->sc_devid == AUTRI_DEVICE_ID_4DWAVE_NX) {
		TWRITE4(sc,AUTRI_NX_TLBC,0);
	}
#endif

	autri_enable_loop_interrupt(sc);

	DPRINTF(("out autri_init()\n"));
	return 0;
}

static void
autri_enable_loop_interrupt(void *sc_)
{
	struct autri_softc *sc;
	uint32_t reg;

	/*reg = (ENDLP_IE | MIDLP_IE);*/
	reg = ENDLP_IE;
	sc = sc_;
	if (sc->sc_devid == AUTRI_DEVICE_ID_SIS_7018)
		reg |= BANK_B_EN;

	autri_reg_set_4(sc, AUTRI_LFO_GC_CIR, reg);
}

#if 0
static void
autri_disable_loop_interrupt(void *sc_)
{
	struct autri_softc *sc;
	uint32_t reg;

	reg = (ENDLP_IE | MIDLP_IE);
	sc = sc_;
	autri_reg_clear_4(sc, AUTRI_LFO_GC_CIR, reg);
}
#endif

static int
autri_intr(void *p)
{
	struct autri_softc *sc;
	uint32_t intsrc;
	uint32_t mask, active[2];
	int ch, endch;
/*
	u_int32_t reg;
	u_int32_t cso,eso;
*/
	sc = p;
	mutex_spin_enter(&sc->sc_intr_lock);

	intsrc = TREAD4(sc, AUTRI_MISCINT);
	if ((intsrc & (ADDRESS_IRQ | MPU401_IRQ)) == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	if (intsrc & ADDRESS_IRQ) {

		active[0] = TREAD4(sc,AUTRI_AIN_A);
		active[1] = TREAD4(sc,AUTRI_AIN_B);

		if (sc->sc_devid == AUTRI_DEVICE_ID_ALI_M5451) {
			endch = 32;
		} else {
			endch = 64;
		}

		for (ch = 0; ch < endch; ch++) {
			mask = 1 << (ch & 0x1f);
			if (active[(ch & 0x20) ? 1 : 0] & mask) {

				/* clear interrupt */
				TWRITE4(sc, (ch & 0x20) ? AUTRI_AIN_B : AUTRI_AIN_A, mask);
				/* disable interrupt */
				autri_reg_clear_4(sc,(ch & 0x20) ? AUTRI_AINTEN_B : AUTRI_AINTEN_A, mask);
#if 0
				reg = TREAD4(sc,AUTRI_LFO_GC_CIR) & ~0x0000003f;
				TWRITE4(sc,AUTRI_LFO_GC_CIR, reg | ch);

				if (sc->sc_devid == AUTRI_DEVICE_ID_4DWAVE_NX) {
					cso = TREAD4(sc, 0xe0) & 0x00ffffff;
					eso = TREAD4(sc, 0xe8) & 0x00ffffff;
				} else {
					cso = (TREAD4(sc, 0xe0) >> 16) & 0x0000ffff;
					eso = (TREAD4(sc, 0xe8) >> 16) & 0x0000ffff;
				}
				/*printf("cso=%d, eso=%d\n",cso,eso);*/
#endif
				if (ch == sc->sc_play.ch_intr) {
					if (sc->sc_play.intr)
						sc->sc_play.intr(sc->sc_play.intr_arg);
				}

				if (ch == sc->sc_rec.ch_intr) {
					if (sc->sc_rec.intr)
						sc->sc_rec.intr(sc->sc_rec.intr_arg);
				}

				/* enable interrupt */
				autri_reg_set_4(sc, (ch & 0x20) ? AUTRI_AINTEN_B : AUTRI_AINTEN_A, mask);
			}
		}
	}

	if (intsrc & MPU401_IRQ) {
		/* XXX */
	}

	autri_reg_set_4(sc,AUTRI_MISCINT,
		ST_TARGET_REACHED | MIXER_OVERFLOW | MIXER_UNDERFLOW);

	mutex_spin_exit(&sc->sc_intr_lock);
	return 1;
}

/*
 *
 */

static int
autri_allocmem(struct autri_softc *sc, size_t size, size_t align,
	       struct autri_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
	    p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
	    &p->nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size,
	    &p->addr, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
	    0, BUS_DMA_WAITOK, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL,
	    BUS_DMA_WAITOK);
	if (error)
		goto destroy;
	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return error;
}

static int
autri_freemem(struct autri_softc *sc, struct autri_dma *p)
{

	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return 0;
}

static int
autri_open(void *addr, int flags)
{
	DPRINTF(("autri_open()\n"));
	DPRINTFN(5,("MISCINT    : 0x%08X\n",
		    TREAD4((struct autri_softc *)addr, AUTRI_MISCINT)));
	DPRINTFN(5,("LFO_GC_CIR : 0x%08X\n",
		    TREAD4((struct autri_softc *)addr, AUTRI_LFO_GC_CIR)));
	return 0;
}

static int
autri_query_encoding(void *addr, struct audio_encoding *fp)
{

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return EINVAL;
	}

	return 0;
}

static int
autri_set_params(void *addr, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	if (setmode & AUMODE_RECORD) {
		if (auconv_set_converter(autri_formats, AUTRI_NFORMATS,
					 AUMODE_RECORD, rec, FALSE, rfil) < 0)
			return EINVAL;
	}
	if (setmode & AUMODE_PLAY) {
		if (auconv_set_converter(autri_formats, AUTRI_NFORMATS,
					 AUMODE_PLAY, play, FALSE, pfil) < 0)
			return EINVAL;
	}
	return 0;
}

static int
autri_round_blocksize(void *addr, int block,
    int mode, const audio_params_t *param)
{
	return block & -4;
}

static int
autri_halt_output(void *addr)
{
	struct autri_softc *sc;

	DPRINTF(("autri_halt_output()\n"));
	sc = addr;
	sc->sc_play.intr = NULL;
	autri_stopch(sc, sc->sc_play.ch, sc->sc_play.ch_intr);
	autri_disable_interrupt(sc, sc->sc_play.ch_intr);

	return 0;
}

static int
autri_halt_input(void *addr)
{
	struct autri_softc *sc;

	DPRINTF(("autri_halt_input()\n"));
	sc = addr;
	sc->sc_rec.intr = NULL;
	autri_stopch(sc, sc->sc_rec.ch, sc->sc_rec.ch_intr);
	autri_disable_interrupt(sc, sc->sc_rec.ch_intr);

	return 0;
}

static int
autri_getdev(void *addr, struct audio_device *retp)
{
	struct autri_softc *sc;

	DPRINTF(("autri_getdev().\n"));
	sc = addr;
	strncpy(retp->name, "Trident 4DWAVE", sizeof(retp->name));
	snprintf(retp->version, sizeof(retp->version), "0x%02x",
	     PCI_REVISION(sc->sc_class));

	switch (sc->sc_devid) {
	case AUTRI_DEVICE_ID_4DWAVE_DX:
		strncpy(retp->config, "4DWAVE-DX", sizeof(retp->config));
		break;
	case AUTRI_DEVICE_ID_4DWAVE_NX:
		strncpy(retp->config, "4DWAVE-NX", sizeof(retp->config));
		break;
	case AUTRI_DEVICE_ID_SIS_7018:
		strncpy(retp->config, "SiS 7018", sizeof(retp->config));
		break;
	case AUTRI_DEVICE_ID_ALI_M5451:
		strncpy(retp->config, "ALi M5451", sizeof(retp->config));
		break;
	default:
		strncpy(retp->config, "unknown", sizeof(retp->config));
	}

	return 0;
}

static int
autri_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct autri_softc *sc;

	sc = addr;
	return sc->sc_codec.codec_if->vtbl->mixer_set_port(
	    sc->sc_codec.codec_if, cp);
}

static int
autri_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct autri_softc *sc;

	sc = addr;
	return sc->sc_codec.codec_if->vtbl->mixer_get_port(
	    sc->sc_codec.codec_if, cp);
}

static int
autri_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct autri_softc *sc;

	sc = addr;
	return sc->sc_codec.codec_if->vtbl->query_devinfo(
	    sc->sc_codec.codec_if, dip);
}

static void *
autri_malloc(void *addr, int direction, size_t size)
{
	struct autri_softc *sc;
	struct autri_dma *p;
	int error;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (!p)
		return NULL;
	sc = addr;
#if 0
	error = autri_allocmem(sc, size, 16, p);
#endif
	error = autri_allocmem(sc, size, 0x10000, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		return NULL;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return KERNADDR(p);
}

static void
autri_free(void *addr, void *ptr, size_t size)
{
	struct autri_softc *sc;
	struct autri_dma **pp, *p;

	sc = addr;
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			autri_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

static struct autri_dma *
autri_find_dma(struct autri_softc *sc, void *addr)
{
	struct autri_dma *p;

	for (p = sc->sc_dmas; p && KERNADDR(p) != addr; p = p->next)
		continue;

	return p;
}

static size_t
autri_round_buffersize(void *addr, int direction, size_t size)
{

	return size;
}

static paddr_t
autri_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct autri_softc *sc;
	struct autri_dma *p;

	if (off < 0)
		return -1;
	sc = addr;
	p = autri_find_dma(sc, mem);
	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

static int
autri_get_props(void *addr)
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
	    AUDIO_PROP_FULLDUPLEX;
}

static void
autri_setup_channel(struct autri_softc *sc, int mode,
		    const audio_params_t *param)
{
	int i, ch, channel;
	uint32_t reg, cr[5];
	uint32_t cso, eso;
	uint32_t delta, dch[2], ctrl;
	uint32_t alpha_fms, fm_vol, attribute;

	uint32_t dmaaddr, dmalen;
	int factor, rvol, cvol;
	struct autri_chstatus *chst;

	ctrl = AUTRI_CTRL_LOOPMODE;
	switch (param->encoding) {
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_SLINEAR_LE:
		ctrl |= AUTRI_CTRL_SIGNED;
		break;
	}

	factor = 0;
	if (param->precision == 16) {
		ctrl |= AUTRI_CTRL_16BIT;
		factor++;
	}

	if (param->channels == 2) {
		ctrl |= AUTRI_CTRL_STEREO;
		factor++;
	}

	delta = param->sample_rate;
	if (delta < 4000)
		delta = 4000;
	if (delta > 48000)
		delta = 48000;

	attribute = 0;

	dch[1] = ((delta << 12) / 48000) & 0x0000ffff;
	if (mode == AUMODE_PLAY) {
		chst = &sc->sc_play;
		dch[0] = ((delta << 12) / 48000) & 0x0000ffff;
		ctrl |= AUTRI_CTRL_WAVEVOL;
	} else {
		chst = &sc->sc_rec;
		dch[0] = ((48000 << 12) / delta) & 0x0000ffff;
		if (sc->sc_devid == AUTRI_DEVICE_ID_SIS_7018) {
			ctrl |= AUTRI_CTRL_MUTEVOL_SIS;
			attribute = AUTRI_ATTR_PCMREC_SIS;
			if (delta != 48000)
				attribute |= AUTRI_ATTR_ENASRC_SIS;
		} else
			ctrl |= AUTRI_CTRL_MUTEVOL;
	}

	dmaaddr = DMAADDR(chst->dma);
	cso = alpha_fms = 0;
	rvol = cvol = 0x7f;
	fm_vol = 0x0 | ((rvol & 0x7f) << 7) | (cvol & 0x7f);

	for (ch = 0; ch < 2; ch++) {

		if (ch == 0)
			dmalen = (chst->length >> factor);
		else {
			/* channel for interrupt */
			dmalen = (chst->blksize >> factor);
			if (sc->sc_devid == AUTRI_DEVICE_ID_SIS_7018)
				ctrl |= AUTRI_CTRL_MUTEVOL_SIS;
			else
				ctrl |= AUTRI_CTRL_MUTEVOL;
			attribute = 0;
			cso = dmalen - 1;
		}

		eso = dmalen - 1;

		switch (sc->sc_devid) {
		case AUTRI_DEVICE_ID_4DWAVE_DX:
			cr[0] = (cso << 16) | (alpha_fms & 0x0000ffff);
			cr[1] = dmaaddr;
			cr[2] = (eso << 16) | (dch[ch] & 0x0000ffff);
			cr[3] = fm_vol;
			cr[4] = ctrl;
			break;
		case AUTRI_DEVICE_ID_4DWAVE_NX:
			cr[0] = (dch[ch] << 24) | (cso & 0x00ffffff);
			cr[1] = dmaaddr;
			cr[2] = ((dch[ch] << 16) & 0xff000000) | (eso & 0x00ffffff);
			cr[3] = (alpha_fms << 16) | (fm_vol & 0x0000ffff);
			cr[4] = ctrl;
			break;
		case AUTRI_DEVICE_ID_SIS_7018:
			cr[0] = (cso << 16) | (alpha_fms & 0x0000ffff);
			cr[1] = dmaaddr;
			cr[2] = (eso << 16) | (dch[ch] & 0x0000ffff);
			cr[3] = attribute;
			cr[4] = ctrl;
			break;
		case AUTRI_DEVICE_ID_ALI_M5451:
			cr[0] = (cso << 16) | (alpha_fms & 0x0000ffff);
			cr[1] = dmaaddr;
			cr[2] = (eso << 16) | (dch[ch] & 0x0000ffff);
			cr[3] = 0;
			cr[4] = ctrl;
			break;
		}

		/* write channel data */
		channel = (ch == 0) ? chst->ch : chst->ch_intr;

		reg = TREAD4(sc,AUTRI_LFO_GC_CIR) & ~0x0000003f;
		TWRITE4(sc,AUTRI_LFO_GC_CIR, reg | channel);

		for (i = 0; i < 5; i++) {
			TWRITE4(sc, AUTRI_ARAM_CR + i*sizeof(cr[0]), cr[i]);
			DPRINTFN(5,("cr[%d] : 0x%08X\n", i, cr[i]));
		}

		/* Bank A only */
		if (channel < 0x20) {
			TWRITE4(sc, AUTRI_EBUF1, AUTRI_EMOD_STILL);
			TWRITE4(sc, AUTRI_EBUF2, AUTRI_EMOD_STILL);
		}
	}

}

static int
autri_trigger_output(void *addr, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     const audio_params_t *param)
{
	struct autri_softc *sc;
	struct autri_dma *p;

	DPRINTFN(5,("autri_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc = addr;
	sc->sc_play.intr = intr;
	sc->sc_play.intr_arg = arg;
	sc->sc_play.offset = 0;
	sc->sc_play.blksize = blksize;
	sc->sc_play.length = (char *)end - (char *)start;

	p = autri_find_dma(sc, start);
	if (!p) {
		printf("autri_trigger_output: bad addr %p\n", start);
		return (EINVAL);
	}

	sc->sc_play.dma = p;

	/* */
	autri_setup_channel(sc, AUMODE_PLAY, param);

	/* volume set to no attenuation */
	TWRITE4(sc, AUTRI_MUSICVOL_WAVEVOL, 0);

	/* enable interrupt */
	autri_enable_interrupt(sc, sc->sc_play.ch_intr);

	/* start channel */
	autri_startch(sc, sc->sc_play.ch, sc->sc_play.ch_intr);

	return 0;
}

static int
autri_trigger_input(void *addr, void *start, void *end, int blksize,
		    void (*intr)(void *), void *arg,
		    const audio_params_t *param)
{
	struct autri_softc *sc;
	struct autri_dma *p;

	DPRINTFN(5,("autri_trigger_input: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc = addr;
	sc->sc_rec.intr = intr;
	sc->sc_rec.intr_arg = arg;
	sc->sc_rec.offset = 0;
	sc->sc_rec.blksize = blksize;
	sc->sc_rec.length = (char *)end - (char *)start;

	/* */
	p = autri_find_dma(sc, start);
	if (!p) {
		printf("autri_trigger_input: bad addr %p\n", start);
		return (EINVAL);
	}

	sc->sc_rec.dma = p;

	/* */
	if (sc->sc_devid == AUTRI_DEVICE_ID_4DWAVE_NX) {
		autri_reg_set_4(sc, AUTRI_NX_ACR0, AUTRI_NX_ACR0_PSB_CAPTURE);
		TWRITE1(sc, AUTRI_NX_RCI3, AUTRI_NX_RCI3_ENABLE | sc->sc_rec.ch);
	}

#if 0
	/* 4DWAVE only allows capturing at a 48 kHz rate */
	if (sc->sc_devid == AUTRI_DEVICE_ID_4DWAVE_DX ||
	    sc->sc_devid == AUTRI_DEVICE_ID_4DWAVE_NX)
		param->sample_rate = 48000;
#endif

	autri_setup_channel(sc, AUMODE_RECORD, param);

	/* enable interrupt */
	autri_enable_interrupt(sc, sc->sc_rec.ch_intr);

	/* start channel */
	autri_startch(sc, sc->sc_rec.ch, sc->sc_rec.ch_intr);

	return 0;
}


static void
autri_get_locks(void *addr, kmutex_t **intr, kmutex_t **proc)
{
	struct autri_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*proc = &sc->sc_lock;
}

#if 0
static int
autri_halt(struct autri_softc *sc)
{

	DPRINTF(("autri_halt().\n"));
	/*autri_stopch(sc);*/
	autri_disable_interrupt(sc, sc->sc_play.channel);
	autri_disable_interrupt(sc, sc->sc_rec.channel);
	return 0;
}
#endif

static void
autri_enable_interrupt(struct autri_softc *sc, int ch)
{
	int reg;

	reg = (ch & 0x20) ? AUTRI_AINTEN_B : AUTRI_AINTEN_A;
	ch &= 0x1f;

	autri_reg_set_4(sc, reg, 1 << ch);
}

static void
autri_disable_interrupt(struct autri_softc *sc, int ch)
{
	int reg;

	reg = (ch & 0x20) ? AUTRI_AINTEN_B : AUTRI_AINTEN_A;
	ch &= 0x1f;

	autri_reg_clear_4(sc, reg, 1 << ch);
}

static void
autri_startch(struct autri_softc *sc, int ch, int ch_intr)
{
	int reg;
	uint32_t chmask;

	reg = (ch & 0x20) ? AUTRI_START_B : AUTRI_START_A;
	ch &= 0x1f;
	ch_intr &= 0x1f;
	chmask = (1 << ch) | (1 << ch_intr);

	autri_reg_set_4(sc, reg, chmask);
}

static void
autri_stopch(struct autri_softc *sc, int ch, int ch_intr)
{
	int reg;
	uint32_t chmask;

	reg = (ch & 0x20) ? AUTRI_STOP_B : AUTRI_STOP_A;
	ch &= 0x1f;
	ch_intr &= 0x1f;
	chmask = (1 << ch) | (1 << ch_intr);

	autri_reg_set_4(sc, reg, chmask);
}

#if NMIDI > 0
static int
autri_midi_open(void *addr, int flags, void (*iintr)(void *, int),
		void (*ointr)(void *), void *arg)
{
	struct autri_softc *sc;

	DPRINTF(("autri_midi_open()\n"));
	sc = addr;
	DPRINTFN(5,("MPUR1 : 0x%02X\n", TREAD1(sc, AUTRI_MPUR1)));
	DPRINTFN(5,("MPUR2 : 0x%02X\n", TREAD1(sc, AUTRI_MPUR2)));

	sc->sc_iintr = iintr;
	sc->sc_ointr = ointr;
	sc->sc_arg = arg;

	if (flags & FREAD)
		autri_reg_clear_1(sc, AUTRI_MPUR2, AUTRI_MIDIIN_ENABLE_INTR);

	if (flags & FWRITE)
		autri_reg_set_1(sc, AUTRI_MPUR2, AUTRI_MIDIOUT_CONNECT);

	return 0;
}

static void
autri_midi_close(void *addr)
{
	struct autri_softc *sc;

	DPRINTF(("autri_midi_close()\n"));
	sc = addr;
	kpause("autri", FALSE, hz/10, &sc->sc_lock); /* give uart a chance to drain */

	sc->sc_iintr = NULL;
	sc->sc_ointr = NULL;
}

static int
autri_midi_output(void *addr, int d)
{
	struct autri_softc *sc;
	int x;

	sc = addr;
	for (x = 0; x != MIDI_BUSY_WAIT; x++) {
		if ((TREAD1(sc, AUTRI_MPUR1) & AUTRI_MIDIOUT_READY) == 0) {
			TWRITE1(sc, AUTRI_MPUR0, d);
			return 0;
		}
		delay(MIDI_BUSY_DELAY);
	}
	return EIO;
}

static void
autri_midi_getinfo(void *addr, struct midi_info *mi)
{

	mi->name = "4DWAVE MIDI UART";
	mi->props = MIDI_PROP_CAN_INPUT;
}

#endif

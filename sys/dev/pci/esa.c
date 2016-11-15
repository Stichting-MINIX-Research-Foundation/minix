/* $NetBSD: esa.c,v 1.60 2014/03/29 19:28:24 christos Exp $ */

/*
 * Copyright (c) 2001-2008 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ESS Allegro-1 / Maestro3 Audio Driver
 *
 * Based on the FreeBSD maestro3 driver and the NetBSD eap driver.
 * Original driver by Don Kim.
 *
 * The list management code could possibly be written better, but what
 * we have right now does the job nicely. Thanks to Zach Brown <zab@zabbo.net>
 * and Andrew MacDonald <amac@epsilon.yi.org> for helping me debug the
 * problems with the original list management code present in the Linux
 * driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esa.c,v 1.60 2014/03/29 19:28:24 christos Exp $");

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/null.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/select.h>
#include <sys/audioio.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97var.h>
#include <dev/ic/ac97reg.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/esareg.h>
#include <dev/pci/esadsp.h>
#include <dev/pci/esavar.h>

#define PCI_CBIO	0x10

#define ESA_DAC_DATA	0x1100

enum {
	ESS_ALLEGRO1,
	ESS_MAESTRO3
};

static const struct esa_card_type {
	uint16_t pci_vendor_id;
	uint16_t pci_product_id;
	int type;
	int delay1, delay2;
} esa_card_types[] = {
	{ PCI_VENDOR_ESSTECH, PCI_PRODUCT_ESSTECH_ALLEGRO1,
	  ESS_ALLEGRO1, 50, 800 },
	{ PCI_VENDOR_ESSTECH, PCI_PRODUCT_ESSTECH_MAESTRO3,
	  ESS_MAESTRO3, 20, 500 },
	{ PCI_VENDOR_ESSTECH, PCI_PRODUCT_ESSTECH_MAESTRO3_2,
	  ESS_MAESTRO3, 20, 500 },
	{ 0, 0, 0, 0, 0 }
};

static struct audio_device esa_device = {
	"ESS Allegro",
	"",
	"esa"
};

static int		esa_match(device_t, cfdata_t, void *);
static void		esa_attach(device_t, device_t, void *);
static int		esa_detach(device_t, int);
static void		esa_childdet(device_t, device_t);

/* audio(9) functions */
static int		esa_query_encoding(void *, struct audio_encoding *);
static int		esa_set_params(void *, int, int, audio_params_t *,
				       audio_params_t *, stream_filter_list_t *,
				       stream_filter_list_t *);
static int		esa_round_blocksize(void *, int, int,
					    const audio_params_t *);
static int		esa_commit_settings(void *);
static int		esa_halt_output(void *);
static int		esa_halt_input(void *);
static int		esa_set_port(void *, mixer_ctrl_t *);
static int		esa_get_port(void *, mixer_ctrl_t *);
static int		esa_query_devinfo(void *, mixer_devinfo_t *);
static void *		esa_malloc(void *, int, size_t);
static void		esa_free(void *, void *, size_t);
static int		esa_getdev(void *, struct audio_device *);
static size_t		esa_round_buffersize(void *, int, size_t);
static int		esa_get_props(void *);
static int		esa_trigger_output(void *, void *, void *, int,
					   void (*)(void *), void *,
					   const audio_params_t *);
static int		esa_trigger_input(void *, void *, void *, int,
					  void (*)(void *), void *,
					  const audio_params_t *);
static void		esa_get_locks(void *, kmutex_t **, kmutex_t **);

static int		esa_intr(void *);
static int		esa_allocmem(struct esa_softc *, size_t, size_t,
				     struct esa_dma *);
static int		esa_freemem(struct esa_softc *, struct esa_dma *);
static paddr_t		esa_mappage(void *, void *, off_t, int);

/* Supporting subroutines */
static uint16_t		esa_read_assp(struct esa_softc *, uint16_t, uint16_t);
static void		esa_write_assp(struct esa_softc *, uint16_t, uint16_t,
				       uint16_t);
static int		esa_init_codec(struct esa_softc *);
static int		esa_attach_codec(void *, struct ac97_codec_if *);
static int		esa_read_codec(void *, uint8_t, uint16_t *);
static int		esa_write_codec(void *, uint8_t, uint16_t);
static int		esa_reset_codec(void *);
static enum ac97_host_flags	esa_flags_codec(void *);
static int		esa_wait(struct esa_softc *);
static int		esa_init(struct esa_softc *);
static void		esa_config(struct esa_softc *);
static uint8_t		esa_assp_halt(struct esa_softc *);
static void		esa_codec_reset(struct esa_softc *);
static int		esa_amp_enable(struct esa_softc *);
static void		esa_enable_interrupts(struct esa_softc *);
static uint32_t		esa_get_pointer(struct esa_softc *,
					struct esa_channel *);

/* list management */
static int		esa_add_list(struct esa_voice *, struct esa_list *,
				     uint16_t, int);
static void		esa_remove_list(struct esa_voice *, struct esa_list *,
					int);

/* power management */
static bool		esa_suspend(device_t, const pmf_qual_t *);
static bool		esa_resume(device_t, const pmf_qual_t *);


#define ESA_NENCODINGS 8
static audio_encoding_t esa_encoding[ESA_NENCODINGS] = {
	{ 0, AudioEulinear, AUDIO_ENCODING_ULINEAR, 8, 0 },
	{ 1, AudioEmulaw, AUDIO_ENCODING_ULAW, 8,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 2, AudioEalaw, AUDIO_ENCODING_ALAW, 8, AUDIO_ENCODINGFLAG_EMULATED },
	{ 3, AudioEslinear, AUDIO_ENCODING_SLINEAR, 8,
		AUDIO_ENCODINGFLAG_EMULATED }, /* XXX: Are you sure? */
	{ 4, AudioEslinear_le, AUDIO_ENCODING_SLINEAR_LE, 16, 0 },
	{ 5, AudioEulinear_le, AUDIO_ENCODING_ULINEAR_LE, 16,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 6, AudioEslinear_be, AUDIO_ENCODING_SLINEAR_BE, 16,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 7, AudioEulinear_be, AUDIO_ENCODING_ULINEAR_BE, 16,
		AUDIO_ENCODINGFLAG_EMULATED }
};

#define ESA_NFORMATS	4
static const struct audio_format esa_formats[ESA_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {ESA_MINRATE, ESA_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {ESA_MINRATE, ESA_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {ESA_MINRATE, ESA_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {ESA_MINRATE, ESA_MAXRATE}},
};

static const struct audio_hw_if esa_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* drain */
	esa_query_encoding,
	esa_set_params,
	esa_round_blocksize,
	esa_commit_settings,
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	esa_halt_output,
	esa_halt_input,
	NULL,			/* speaker_ctl */
	esa_getdev,
	NULL,			/* getfd */
	esa_set_port,
	esa_get_port,
	esa_query_devinfo,
	esa_malloc,
	esa_free,
	esa_round_buffersize,
	esa_mappage,
	esa_get_props,
	esa_trigger_output,
	esa_trigger_input,
	NULL,	/* dev_ioctl */
	esa_get_locks,
};

CFATTACH_DECL2_NEW(esa, sizeof(struct esa_softc), esa_match, esa_attach,
    esa_detach, NULL, NULL, esa_childdet);

/*
 * audio(9) functions
 */

static int
esa_query_encoding(void *hdl, struct audio_encoding *ae)
{

	if (ae->index < 0 || ae->index >= ESA_NENCODINGS)
		return EINVAL;
	*ae = esa_encoding[ae->index];

	return 0;
}

static int
esa_set_params(void *hdl, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	struct esa_voice *vc;
	struct esa_channel *ch;
	struct audio_params *p;
	stream_filter_list_t *fil;
	int mode, i;

	vc = hdl;
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = (mode == AUMODE_RECORD) ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		switch (mode) {
		case AUMODE_PLAY:
			p = play;
			ch = &vc->play;
			fil = pfil;
			break;
		case AUMODE_RECORD:
			p = rec;
			ch = &vc->rec;
			fil = rfil;
			break;
		default:
			return EINVAL;
		}

		if (p->sample_rate < ESA_MINRATE ||
		    p->sample_rate > ESA_MAXRATE ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels < 1 || p->channels > 2))
			return EINVAL;

		i = auconv_set_converter(esa_formats, ESA_NFORMATS,
					 mode, p, FALSE, fil);
		if (i < 0)
			return EINVAL;
		if (fil->req_size > 0)
			p = &fil->filters[0].param;
		ch->mode = *p;
	}

	return 0;
}

static int
esa_commit_settings(void *hdl)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	const audio_params_t *p;
	const audio_params_t *r;
	uint32_t data;
	uint32_t freq;
	int data_bytes;

	vc = hdl;
	sc = device_private(vc->parent);
	p = &vc->play.mode;
	r = &vc->rec.mode;
	data_bytes = (((ESA_MINISRC_TMP_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_IN_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255)
	    &~ 255;
	/* playback */
	vc->play.data_offset = ESA_DAC_DATA + (data_bytes * vc->index);
	if (p->channels == 1)
		data = 1;
	else
		data = 0;
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->play.data_offset + ESA_SRC3_MODE_OFFSET,
		       data);
	if (p->precision  == 8)
		data = 1;
	else
		data = 0;
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->play.data_offset + ESA_SRC3_WORD_LENGTH_OFFSET,
		       data);
	if ((freq = ((p->sample_rate << 15) + 24000) / 48000) != 0) {
		freq--;
	}
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->play.data_offset + ESA_CDATA_FREQUENCY, freq);

	/* recording */
	vc->rec.data_offset = ESA_DAC_DATA + (data_bytes * vc->index) +
			      (data_bytes / 2);
	if (r->channels == 1)
		data = 1;
	else
		data = 0;
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->rec.data_offset + ESA_SRC3_MODE_OFFSET,
		       data);
	if (r->precision == 8)
		data = 1;
	else
		data = 0;
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->rec.data_offset + ESA_SRC3_WORD_LENGTH_OFFSET,
		       data);
	if ((freq = ((r->sample_rate << 15) + 24000) / 48000) != 0) {
		freq--;
	}
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       vc->rec.data_offset + ESA_CDATA_FREQUENCY, freq);

	return 0;
};

static int
esa_round_blocksize(void *hdl, int bs, int mode,
    const audio_params_t *param)
{

	return bs & ~0x20;	/* Be conservative; align to 32 bytes */
}

static int
esa_halt_output(void *hdl)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint16_t data;

	vc = hdl;
	sc = device_private(vc->parent);
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (vc->play.active == 0)
		return 0;

	vc->play.active = 0;

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       ESA_CDATA_INSTANCE_READY + vc->play.data_offset, 0);

	sc->sc_ntimers--;
	if (sc->sc_ntimers == 0) {
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
			       ESA_KDATA_TIMER_COUNT_RELOAD, 0);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
			       ESA_KDATA_TIMER_COUNT_CURRENT, 0);
		data = bus_space_read_2(iot, ioh, ESA_HOST_INT_CTRL);
		bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL,
		    data & ~ESA_CLKRUN_GEN_ENABLE);
	}

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       ESA_KDATA_MIXER_TASK_NUMBER,
		       sc->mixer_list.indexmap[vc->index]);
	/* remove ourselves from the packed lists */
	esa_remove_list(vc, &sc->mixer_list, vc->index);
	esa_remove_list(vc, &sc->dma_list, vc->index);
	esa_remove_list(vc, &sc->msrc_list, vc->index);

	return 0;
}

static int
esa_halt_input(void *hdl)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t data;

	vc = hdl;
	sc = device_private(vc->parent);
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (vc->rec.active == 0)
		return 0;

	vc->rec.active = 0;

	sc->sc_ntimers--;
	if (sc->sc_ntimers == 0) {
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
			       ESA_KDATA_TIMER_COUNT_RELOAD, 0);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
			       ESA_KDATA_TIMER_COUNT_CURRENT, 0);
		data = bus_space_read_2(iot, ioh, ESA_HOST_INT_CTRL);
		bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL,
				  data & ~ESA_CLKRUN_GEN_ENABLE);
	}

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, vc->rec.data_offset +
		       ESA_CDATA_INSTANCE_READY, 0);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, ESA_KDATA_ADC1_REQUEST,
		       0);

	/* remove ourselves from the packed lists */
	esa_remove_list(vc, &sc->adc1_list, vc->index + ESA_NUM_VOICES);
	esa_remove_list(vc, &sc->dma_list, vc->index + ESA_NUM_VOICES);
	esa_remove_list(vc, &sc->msrc_list, vc->index + ESA_NUM_VOICES);

	return 0;
}

static void *
esa_malloc(void *hdl, int direction, size_t size)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	struct esa_dma *p;
	int error;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	vc = hdl;
	sc = device_private(vc->parent);
	error = esa_allocmem(sc, size, 16, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		aprint_error_dev(sc->sc_dev,
		    "%s: not enough memory\n", __func__);
		return 0;
	}
	p->next = vc->dma;
	vc->dma = p;

	return KERNADDR(p);
}

static void
esa_free(void *hdl, void *addr, size_t size)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	struct esa_dma *p;
	struct esa_dma **pp;

	vc = hdl;
	sc = device_private(vc->parent);
	for (pp = &vc->dma; (p = *pp) != NULL; pp = &p->next)
		if (KERNADDR(p) == addr) {
			esa_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
}

static int
esa_getdev(void *hdl, struct audio_device *ret)
{

	*ret = esa_device;
	return 0;
}

static int
esa_set_port(void *hdl, mixer_ctrl_t *mc)
{
	struct esa_voice *vc;
	struct esa_softc *sc;

	vc = hdl;
	sc = device_private(vc->parent);
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, mc);
}

static int
esa_get_port(void *hdl, mixer_ctrl_t *mc)
{
	struct esa_voice *vc;
	struct esa_softc *sc;

	vc = hdl;
	sc = device_private(vc->parent);
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, mc);
}

static int
esa_query_devinfo(void *hdl, mixer_devinfo_t *di)
{
	struct esa_voice *vc;
	struct esa_softc *sc;

	vc = hdl;
	sc = device_private(vc->parent);
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, di);
}

static size_t
esa_round_buffersize(void *hdl, int direction, size_t bufsize)
{

	return bufsize;
}

static int
esa_get_props(void *hdl)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

static int
esa_trigger_output(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	struct esa_dma *p;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	size_t size;
	uint32_t data, bufaddr, i;
	int data_bytes, dac_data, dsp_in_size;
	int dsp_out_size, dsp_in_buf, dsp_out_buf;

	vc = hdl;
	sc = device_private(vc->parent);
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	data_bytes = (((ESA_MINISRC_TMP_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_IN_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255) & ~255;
	dac_data = ESA_DAC_DATA + (data_bytes * vc->index);
	dsp_in_size = ESA_MINISRC_IN_BUFFER_SIZE - (0x20 * 2);
	dsp_out_size = ESA_MINISRC_OUT_BUFFER_SIZE - (0x20 * 2);
	dsp_in_buf = dac_data + (ESA_MINISRC_TMP_BUFFER_SIZE / 2);
	dsp_out_buf = dsp_in_buf + (dsp_in_size / 2) + 1;

	if (vc->play.active)
		return EINVAL;

	for (p = vc->dma; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (p == NULL) {
		aprint_error_dev(sc->sc_dev, "%s: bad addr %p\n", __func__,
		    start);
		return EINVAL;
	}

	vc->play.active = 1;
	vc->play.intr = intr;
	vc->play.arg = intrarg;
	vc->play.pos = 0;
	vc->play.count = 0;
	vc->play.buf = start;
	vc->play.bufsize = size = (size_t)(((char *)end - (char *)start));
	vc->play.blksize = blksize;
	bufaddr = DMAADDR(p);
	vc->play.start = bufaddr;

#define LO(x) ((x) & 0x0000ffff)
#define HI(x) ((x) >> 16)

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_ADDRL, LO(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_ADDRH, HI(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_END_PLUS_1L, LO(bufaddr + size));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_END_PLUS_1H, HI(bufaddr + size));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_CURRENTL, LO(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_HOST_SRC_CURRENTH, HI(bufaddr));

	/* DSP buffers */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_IN_BUF_BEGIN, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_IN_BUF_END_PLUS_1, dsp_in_buf + (dsp_in_size / 2));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_IN_BUF_HEAD, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_IN_BUF_TAIL, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_OUT_BUF_BEGIN, dsp_out_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_OUT_BUF_END_PLUS_1, dsp_out_buf + (dsp_out_size / 2));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_OUT_BUF_HEAD, dsp_out_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_OUT_BUF_TAIL, dsp_out_buf);

	/* Some per-client initializers */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_SRC3_DIRECTION_OFFSET + 12, dac_data + 40 + 8);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_SRC3_DIRECTION_OFFSET + 19, 0x400 + ESA_MINISRC_COEF_LOC);
	/* Enable or disable low-pass filter? (0xff if rate > 45000) */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_SRC3_DIRECTION_OFFSET + 22,
	    vc->play.mode.sample_rate > 45000 ? 0xff : 0);
	/* Tell it which way DMA is going */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_DMA_CONTROL,
	    ESA_DMACONTROL_AUTOREPEAT + ESA_DMAC_PAGE3_SELECTOR +
	    ESA_DMAC_BLOCKF_SELECTOR);

	/* Set an armload of static initializers */
	for (i = 0; i < __arraycount(esa_playvals); i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
		    esa_playvals[i].addr, esa_playvals[i].val);

	/* Put us in the packed task lists */
	esa_add_list(vc, &sc->msrc_list, dac_data >> ESA_DP_SHIFT_COUNT,
		     vc->index);
	esa_add_list(vc, &sc->dma_list, dac_data >> ESA_DP_SHIFT_COUNT,
		     vc->index);
	esa_add_list(vc, &sc->mixer_list, dac_data >> ESA_DP_SHIFT_COUNT,
		     vc->index);
#undef LO
#undef HI

	sc->sc_ntimers++;

	if (sc->sc_ntimers == 1) {
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_TIMER_COUNT_RELOAD, 240);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_TIMER_COUNT_CURRENT, 240);
		data = bus_space_read_2(iot, ioh, ESA_HOST_INT_CTRL);
		bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL,
		    data | ESA_CLKRUN_GEN_ENABLE);
	}

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, dac_data +
	    ESA_CDATA_INSTANCE_READY, 1);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
	    ESA_KDATA_MIXER_TASK_NUMBER,
	    sc->mixer_list.indexmap[vc->index]);

	return 0;
}

static int
esa_trigger_input(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	struct esa_dma *p;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t data, bufaddr, i;
	size_t size;
	int data_bytes, adc_data, dsp_in_size;
	int dsp_out_size, dsp_in_buf, dsp_out_buf;

	vc = hdl;
	sc = device_private(vc->parent);
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	data_bytes = (((ESA_MINISRC_TMP_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_IN_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255) & ~255;
	adc_data = ESA_DAC_DATA + (data_bytes * vc->index) + (data_bytes / 2);
	dsp_in_size = ESA_MINISRC_IN_BUFFER_SIZE - (0x10 * 2);
	dsp_out_size = ESA_MINISRC_OUT_BUFFER_SIZE - (0x10 * 2);
	dsp_in_buf = adc_data + (ESA_MINISRC_TMP_BUFFER_SIZE / 2);
	dsp_out_buf = dsp_in_buf + (dsp_in_size / 2) + 1;

	vc->rec.data_offset = adc_data;

	/* We only support 1 recording channel */
	if (vc->index > 0)
		return ENODEV;

	if (vc->rec.active)
		return EINVAL;

	for (p = vc->dma; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (p == NULL) {
		aprint_error_dev(sc->sc_dev, "%s: bad addr %p\n",
		    __func__, start);
		return EINVAL;
	}

	vc->rec.active = 1;
	vc->rec.intr = intr;
	vc->rec.arg = intrarg;
	vc->rec.pos = 0;
	vc->rec.count = 0;
	vc->rec.buf = start;
	vc->rec.bufsize = size = (size_t)(((char *)end - (char *)start));
	vc->rec.blksize = blksize;
	bufaddr = DMAADDR(p);
	vc->rec.start = bufaddr;

#define LO(x) ((x) & 0x0000ffff)
#define HI(x) ((x) >> 16)

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_ADDRL, LO(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_ADDRH, HI(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_END_PLUS_1L, LO(bufaddr + size));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_END_PLUS_1H, HI(bufaddr + size));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_CURRENTL, LO(bufaddr));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_HOST_SRC_CURRENTH, HI(bufaddr));

	/* DSP buffers */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_IN_BUF_BEGIN, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_IN_BUF_END_PLUS_1, dsp_in_buf + (dsp_in_size / 2));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_IN_BUF_HEAD, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_IN_BUF_TAIL, dsp_in_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_OUT_BUF_BEGIN, dsp_out_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_OUT_BUF_END_PLUS_1, dsp_out_buf + (dsp_out_size / 2));
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_OUT_BUF_HEAD, dsp_out_buf);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_OUT_BUF_TAIL, dsp_out_buf);

	/* Some per-client initializers */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_SRC3_DIRECTION_OFFSET + 12, adc_data + 40 + 8);
	/* Tell it which way DMA is going */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_DMA_CONTROL,
	    ESA_DMACONTROL_DIRECTION + ESA_DMACONTROL_AUTOREPEAT +
	    ESA_DMAC_PAGE3_SELECTOR + ESA_DMAC_BLOCKF_SELECTOR);

	/* Set an armload of static initializers */
	for (i = 0; i < __arraycount(esa_recvals); i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
		    esa_recvals[i].addr, esa_recvals[i].val);

	/* Put us in the packed task lists */
	esa_add_list(vc, &sc->adc1_list, adc_data >> ESA_DP_SHIFT_COUNT,
		     vc->index + ESA_NUM_VOICES);
	esa_add_list(vc, &sc->msrc_list, adc_data >> ESA_DP_SHIFT_COUNT,
		     vc->index + ESA_NUM_VOICES);
	esa_add_list(vc, &sc->dma_list, adc_data >> ESA_DP_SHIFT_COUNT,
		     vc->index + ESA_NUM_VOICES);
#undef LO
#undef HI

	sc->sc_ntimers++;
	if (sc->sc_ntimers == 1) {
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_TIMER_COUNT_RELOAD, 240);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_TIMER_COUNT_CURRENT, 240);
		data = bus_space_read_2(iot, ioh, ESA_HOST_INT_CTRL);
		bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL,
		    data | ESA_CLKRUN_GEN_ENABLE);
	}

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, adc_data +
	    ESA_CDATA_INSTANCE_READY, 1);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, ESA_KDATA_ADC1_REQUEST,
	    1);

	return 0;
}

/* Interrupt handler */
static int
esa_intr(void *hdl)
{
	struct esa_softc *sc;
	struct esa_voice *vc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint8_t status;
	uint32_t pos;
	uint32_t diff;
	uint32_t blksize;
	int i;

	sc = hdl;
	mutex_spin_enter(&sc->sc_intr_lock);

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	status = bus_space_read_1(iot, ioh, ESA_HOST_INT_STATUS);
	if (status == 0xff) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	/* ack the interrupt */
	bus_space_write_1(iot, ioh, ESA_HOST_INT_STATUS, status);

	if (status & ESA_HV_INT_PENDING) {
		uint8_t event;

		aprint_normal_dev(sc->sc_dev, "hardware volume interrupt\n");
		event = bus_space_read_1(iot, ioh, ESA_HW_VOL_COUNTER_MASTER);
		switch(event) {
		case 0xaa:	/* volume up */
			pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_UP);
			break;
		case 0x66:	/* volume down */
			pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_DOWN);
			break;
		case 0x88:	/* mute */
			pmf_event_inject(NULL, PMFE_AUDIO_VOLUME_TOGGLE);
			break;
		default:
			aprint_normal_dev(sc->sc_dev,
			    "unknown hwvol event 0x%02x\n", event);
			break;
		}
		bus_space_write_1(iot, ioh, ESA_HW_VOL_COUNTER_MASTER, 0x88);
	}

	if ((status & ESA_ASSP_INT_PENDING) == 0 ||
	    (bus_space_read_1(iot, ioh,
	     ESA_ASSP_CONTROL_B) & ESA_STOP_ASSP_CLOCK) != 0 ||
	    (bus_space_read_1(iot, ioh,
	     ESA_ASSP_HOST_INT_STATUS) & ESA_DSP2HOST_REQ_TIMER) == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 1;
	}

	bus_space_write_1(iot, ioh, ESA_ASSP_HOST_INT_STATUS,
	    ESA_DSP2HOST_REQ_TIMER);

	for (i = 0; i < ESA_NUM_VOICES; i++) {
		vc = &sc->voice[i];

		if (vc->play.active) {
			pos = esa_get_pointer(sc, &vc->play) % vc->play.bufsize;
			diff = (vc->play.bufsize + pos - vc->play.pos) %
			    vc->play.bufsize;

			vc->play.pos = pos;
			vc->play.count += diff;
			blksize = vc->play.blksize;

			while (vc->play.count >= blksize) {
				vc->play.count -= blksize;
				(*vc->play.intr)(vc->play.arg);
			}
		}

		if (vc->rec.active) {
			pos = esa_get_pointer(sc, &vc->rec) % vc->rec.bufsize;
			diff = (vc->rec.bufsize + pos - vc->rec.pos) %
			    vc->rec.bufsize;

			vc->rec.pos = pos;
			vc->rec.count += diff;
			blksize = vc->rec.blksize;

			while (vc->rec.count >= blksize) {
				vc->rec.count -= blksize;
				(*vc->rec.intr)(vc->rec.arg);
			}
		}
	}

	mutex_spin_exit(&sc->sc_intr_lock);
	return 1;
}

static int
esa_allocmem(struct esa_softc *sc, size_t size, size_t align,
	     struct esa_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmat, p->size, align, 0,
				 p->segs, __arraycount(p->segs),
				 &p->nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = bus_dmamem_map(sc->sc_dmat, p->segs, p->nsegs, p->size,
				&p->addr, BUS_DMA_WAITOK | BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmat, p->size, 1, p->size, 0,
				  BUS_DMA_WAITOK, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, p->size, NULL,
				BUS_DMA_WAITOK);
	if (error)
		goto destroy;

	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return error;
}

static int
esa_freemem(struct esa_softc *sc, struct esa_dma *p)
{

	bus_dmamap_unload(sc->sc_dmat, p->map);
	bus_dmamap_destroy(sc->sc_dmat, p->map);
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return 0;
}

/*
 * Supporting Subroutines
 */

static int
esa_match(device_t dev, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ESSTECH:
		switch(PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ESSTECH_ALLEGRO1:
		case PCI_PRODUCT_ESSTECH_MAESTRO3:
		case PCI_PRODUCT_ESSTECH_MAESTRO3_2:
			return 1;
		}
	}

	return 0;
}

static void
esa_attach(device_t parent, device_t self, void *aux)
{
	struct esa_softc *sc;
	struct pci_attach_args *pa;
	pcitag_t tag;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const struct esa_card_type *card;
	const char *intrstr;
	uint32_t data;
	int revision, i, error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	pa = (struct pci_attach_args *)aux;
	tag = pa->pa_tag;
	pc = pa->pa_pc;

	pci_aprint_devinfo(pa, "Audio controller");

	revision = PCI_REVISION(pa->pa_class);

	for (card = esa_card_types; card->pci_vendor_id; card++)
		if (PCI_VENDOR(pa->pa_id) == card->pci_vendor_id &&
		    PCI_PRODUCT(pa->pa_id) == card->pci_product_id) {
			sc->type = card->type;
			sc->delay1 = card->delay1;
			sc->delay2 = card->delay2;
			break;
		}

	data = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	data |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE
	    | PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, data);

	/* Map I/O register */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, &sc->sc_iob, &sc->sc_ios)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	/* Initialize softc */
	sc->sc_dev = self;
	sc->sc_tag = tag;
	sc->sc_pct = pc;
	sc->sc_dmat = pa->pa_dmat;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* Map and establish an interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "can't map interrupt\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, esa_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    pci_activate_null)) && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n", error);
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* Init chip */
	if (esa_init(sc) == -1) {
		aprint_error_dev(sc->sc_dev,
		    "esa_attach: unable to initialize the card\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* create suspend save area */
	sc->savememsz = sizeof(uint16_t) * (ESA_REV_B_CODE_MEMORY_LENGTH
	    + ESA_REV_B_DATA_MEMORY_LENGTH + 1);
	sc->savemem = kmem_zalloc(sc->savememsz, KM_SLEEP);
	if (sc->savemem == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate suspend buffer\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/*
	 * Every card I've seen has had their channels swapped with respect
	 * to the mixer. Ie:
	 *  $ mixerctl -w outputs.master=0,191
	 * Would result in the _right_ speaker being turned off.
	 *
	 * So, we will swap the left and right mixer channels to compensate
	 * for this.
	 *
	 * XXX PR# 23620: The Dell C810 channels are not swapped. Match
	 *     on revision ID for now; this is probably wrong.
	 */
	if (revision == 0x10 && sc->type == ESS_MAESTRO3)
		sc->codec_flags = 0;
	else
		sc->codec_flags = AC97_HOST_SWAPPED_CHANNELS;

	/* initialize list management structures */
	sc->mixer_list.mem_addr = ESA_KDATA_MIXER_XFER0;
	sc->mixer_list.max = ESA_MAX_VIRTUAL_MIXER_CHANNELS;
	sc->adc1_list.mem_addr = ESA_KDATA_ADC1_XFER0;
	sc->adc1_list.max = ESA_MAX_VIRTUAL_ADC1_CHANNELS;
	sc->dma_list.mem_addr = ESA_KDATA_DMA_XFER0;
	sc->dma_list.max = ESA_MAX_VIRTUAL_DMA_CHANNELS;
	sc->msrc_list.mem_addr = ESA_KDATA_INSTANCE0_MINISRC;
	sc->msrc_list.max = ESA_MAX_INSTANCE_MINISRC;

	/* initialize index maps */
	for (i = 0; i < ESA_NUM_VOICES * 2; i++) {
		sc->mixer_list.indexmap[i] = -1;
		sc->msrc_list.indexmap[i] = -1;
		sc->dma_list.indexmap[i] = -1;
		sc->adc1_list.indexmap[i] = -1;
	}

	/* Attach AC97 host interface */
	sc->host_if.arg = sc;
	sc->host_if.attach = esa_attach_codec;
	sc->host_if.read = esa_read_codec;
	sc->host_if.write = esa_write_codec;
	sc->host_if.reset = esa_reset_codec;
	sc->host_if.flags = esa_flags_codec;

	if (ac97_attach(&sc->host_if, self, &sc->sc_lock) != 0) {
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* Attach audio interface. */
	for (i = 0; i < ESA_NUM_VOICES; i++) {
		sc->voice[i].parent = sc->sc_dev;
		sc->voice[i].index = i;
		sc->sc_audiodev[i] =
		    audio_attach_mi(&esa_hw_if, &sc->voice[i], sc->sc_dev);
	}

	if (!pmf_device_register(self, esa_suspend, esa_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

void
esa_childdet(device_t self, device_t child)
{
	struct esa_softc *sc = device_private(self);
	int i;

	for (i = 0; i < ESA_NUM_VOICES; i++) {
		if (sc->sc_audiodev[i] == child) {
			sc->sc_audiodev[i] = NULL;
			break;
		}
	}
	KASSERT(i < ESA_NUM_VOICES);
}

static int
esa_detach(device_t self, int flags)
{
	struct esa_softc *sc;
	int i;

	sc = device_private(self);
	for (i = 0; i < ESA_NUM_VOICES; i++) {
		if (sc->sc_audiodev[i] != NULL)
			config_detach(sc->sc_audiodev[i], flags);
	}

	if (sc->sc_ih != NULL)
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
	if (sc->sc_ios)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);

	kmem_free(sc->savemem, sc->savememsz);
	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	return 0;
}

static uint16_t
esa_read_assp(struct esa_softc *sc, uint16_t region, uint16_t index)
{
	uint16_t data;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	bus_space_write_2(iot, ioh, ESA_DSP_PORT_MEMORY_TYPE,
	    region & ESA_MEMTYPE_MASK);
	bus_space_write_2(iot, ioh, ESA_DSP_PORT_MEMORY_INDEX, index);
	data = bus_space_read_2(iot, ioh, ESA_DSP_PORT_MEMORY_DATA);

	return data;
}

static void
esa_write_assp(struct esa_softc *sc, uint16_t region, uint16_t index,
	       uint16_t data)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	bus_space_write_2(iot, ioh, ESA_DSP_PORT_MEMORY_TYPE,
	    region & ESA_MEMTYPE_MASK);
	bus_space_write_2(iot, ioh, ESA_DSP_PORT_MEMORY_INDEX, index);
	bus_space_write_2(iot, ioh, ESA_DSP_PORT_MEMORY_DATA, data);

	return;
}

static int
esa_init_codec(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t data;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	data = bus_space_read_1(iot, ioh, ESA_CODEC_COMMAND);

	return (data & 0x1) ? 0 : 1;
}

static int
esa_attach_codec(void *aux, struct ac97_codec_if *codec_if)
{
	struct esa_softc *sc;

	sc = aux;
	sc->codec_if = codec_if;

	return 0;
}

static int
esa_read_codec(void *aux, uint8_t reg, uint16_t *result)
{
	struct esa_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	sc = aux;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (esa_wait(sc))
		aprint_error_dev(sc->sc_dev, "esa_read_codec: timed out\n");
	bus_space_write_1(iot, ioh, ESA_CODEC_COMMAND, (reg & 0x7f) | 0x80);
	delay(50);
	if (esa_wait(sc))
		aprint_error_dev(sc->sc_dev, "esa_read_codec: timed out\n");
	*result = bus_space_read_2(iot, ioh, ESA_CODEC_DATA);

	return 0;
}

static int
esa_write_codec(void *aux, uint8_t reg, uint16_t data)
{
	struct esa_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	sc = aux;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (esa_wait(sc)) {
		aprint_error_dev(sc->sc_dev, "esa_write_codec: timed out\n");
		return -1;
	}
	bus_space_write_2(iot, ioh, ESA_CODEC_DATA, data);
	bus_space_write_1(iot, ioh, ESA_CODEC_COMMAND, reg & 0x7f);
	delay(50);

	return 0;
}

static int
esa_reset_codec(void *aux)
{

	return 0;
}

static enum ac97_host_flags
esa_flags_codec(void *aux)
{
	struct esa_softc *sc;

	sc = aux;
	return sc->codec_flags;
}

static int
esa_wait(struct esa_softc *sc)
{
	int i, val;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	for (i = 0; i < 20; i++) {
		val = bus_space_read_1(iot, ioh, ESA_CODEC_STATUS);
		if ((val & 1) == 0)
			return 0;
		delay(2);
	}

	return -1;
}

static int
esa_init(struct esa_softc *sc)
{
	struct esa_voice *vc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pcitag_t tag;
	pci_chipset_tag_t pc;
	uint32_t data, i, size;
	uint8_t reset_state;
	int data_bytes;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	tag = sc->sc_tag;
	pc = sc->sc_pct;
	data_bytes = (((ESA_MINISRC_TMP_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_IN_BUFFER_SIZE & ~1) +
	    (ESA_MINISRC_OUT_BUFFER_SIZE & ~1) + 4) + 255) & ~255;

	mutex_spin_enter(&sc->sc_intr_lock);

	/* Disable legacy emulation */
	data = pci_conf_read(pc, tag, PCI_LEGACY_AUDIO_CTRL);
	data |= DISABLE_LEGACY;
	pci_conf_write(pc, tag, PCI_LEGACY_AUDIO_CTRL, data);

	esa_config(sc);

	reset_state = esa_assp_halt(sc);

	esa_init_codec(sc);
	esa_codec_reset(sc);

	/* Zero kernel and mixer data */
	size = ESA_REV_B_DATA_MEMORY_UNIT_LENGTH * ESA_NUM_UNITS_KERNEL_DATA;
	for (i = 0; i < size / 2; i++) {
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_BASE_ADDR + i, 0);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		    ESA_KDATA_BASE_ADDR2 + i, 0);
	}

	/* Init DMA pointer */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, ESA_KDATA_CURRENT_DMA,
	    ESA_KDATA_DMA_XFER0);

	/* Write kernel code into memory */
	for (i = 0; i < __arraycount(esa_assp_kernel_image); i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_CODE,
		    ESA_REV_B_CODE_MEMORY_BEGIN + i, esa_assp_kernel_image[i]);

	for (i = 0; i < __arraycount(esa_assp_minisrc_image); i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_CODE, 0x400 + i,
		    esa_assp_minisrc_image[i]);

	/* Write the coefficients for the low pass filter */
	for (i = 0; i < __arraycount(esa_minisrc_lpf_image); i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_CODE,
		    0x400 + ESA_MINISRC_COEF_LOC + i,
		    esa_minisrc_lpf_image[i]);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_CODE,
	    0x400 + ESA_MINISRC_COEF_LOC + size, 0x8000);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, ESA_KDATA_TASK0, 0x400);
	/* Init the mixer number */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
	     ESA_KDATA_MIXER_TASK_NUMBER, 0);
	/* Extreme kernel master volume */
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
	    ESA_KDATA_DAC_LEFT_VOLUME, ESA_ARB_VOLUME);
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
	    ESA_KDATA_DAC_RIGHT_VOLUME, ESA_ARB_VOLUME);

	if (esa_amp_enable(sc))
		return -1;

	/* Zero entire DAC/ADC area */
	for (i = 0x1100; i < 0x1c00; i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, i, 0);

	/* set some sane defaults */
	for (i = 0; i < ESA_NUM_VOICES; i++) {
		vc = &sc->voice[i];
		vc->play.data_offset = ESA_DAC_DATA + (data_bytes * i);
		vc->rec.data_offset = ESA_DAC_DATA + (data_bytes * i * 2);
	}

	esa_enable_interrupts(sc);

	bus_space_write_1(iot, ioh, ESA_DSP_PORT_CONTROL_REG_B,
	    reset_state | ESA_REGB_ENABLE_RESET);

	mutex_spin_exit(&sc->sc_intr_lock);

	return 0;
}

static void
esa_config(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pcitag_t tag;
	pci_chipset_tag_t pc;
	uint32_t data;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	tag = sc->sc_tag;
	pc = sc->sc_pct;

	data = pci_conf_read(pc, tag, ESA_PCI_ALLEGRO_CONFIG);
	data &= ESA_REDUCED_DEBOUNCE;
	data |= ESA_PM_CTRL_ENABLE | ESA_CLK_DIV_BY_49 | ESA_USE_PCI_TIMING;
	pci_conf_write(pc, tag, ESA_PCI_ALLEGRO_CONFIG, data);

	bus_space_write_1(iot, ioh, ESA_ASSP_CONTROL_B, ESA_RESET_ASSP);
	data = pci_conf_read(pc, tag, ESA_PCI_ALLEGRO_CONFIG);
	data &= ~ESA_INT_CLK_SELECT;
	if (sc->type == ESS_MAESTRO3) {
		data &= ~ESA_INT_CLK_MULT_ENABLE;
		data |= ESA_INT_CLK_SRC_NOT_PCI;
	}
	data &= ~(ESA_CLK_MULT_MODE_SELECT | ESA_CLK_MULT_MODE_SELECT_2);
	pci_conf_write(pc, tag, ESA_PCI_ALLEGRO_CONFIG, data);

	if (sc->type == ESS_ALLEGRO1) {
		data = pci_conf_read(pc, tag, ESA_PCI_USER_CONFIG);
		data |= ESA_IN_CLK_12MHZ_SELECT;
		pci_conf_write(pc, tag, ESA_PCI_USER_CONFIG, data);
	}

	data = bus_space_read_1(iot, ioh, ESA_ASSP_CONTROL_A);
	data &= ~(ESA_DSP_CLK_36MHZ_SELECT | ESA_ASSP_CLK_49MHZ_SELECT);
	data |= ESA_ASSP_CLK_49MHZ_SELECT;	/* XXX: Assumes 49MHz DSP */
	data |= ESA_ASSP_0_WS_ENABLE;
	bus_space_write_1(iot, ioh, ESA_ASSP_CONTROL_A, data);

	bus_space_write_1(iot, ioh, ESA_ASSP_CONTROL_B, ESA_RUN_ASSP);

	return;
}

static uint8_t
esa_assp_halt(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint8_t data, reset_state;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	data = bus_space_read_1(iot, ioh, ESA_DSP_PORT_CONTROL_REG_B);
	reset_state = data & ~ESA_REGB_STOP_CLOCK;
	delay(10000);
	bus_space_write_1(iot, ioh, ESA_DSP_PORT_CONTROL_REG_B,
			reset_state & ~ESA_REGB_ENABLE_RESET);
	delay(10000);

	return reset_state;
}

static void
esa_codec_reset(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint16_t data, dir;
	int retry;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	retry = 0;
	do {
		data = bus_space_read_2(iot, ioh, ESA_GPIO_DIRECTION);
		dir = data | 0x10; /* assuming pci bus master? */

		/* remote codec config */
		data = bus_space_read_2(iot, ioh, ESA_RING_BUS_CTRL_B);
		bus_space_write_2(iot, ioh, ESA_RING_BUS_CTRL_B,
		    data & ~ESA_SECOND_CODEC_ID_MASK);
		data = bus_space_read_2(iot, ioh, ESA_SDO_OUT_DEST_CTRL);
		bus_space_write_2(iot, ioh, ESA_SDO_OUT_DEST_CTRL,
		    data & ~ESA_COMMAND_ADDR_OUT);
		data = bus_space_read_2(iot, ioh, ESA_SDO_IN_DEST_CTRL);
		bus_space_write_2(iot, ioh, ESA_SDO_IN_DEST_CTRL,
		    data & ~ESA_STATUS_ADDR_IN);

		bus_space_write_2(iot, ioh, ESA_RING_BUS_CTRL_A,
				  ESA_IO_SRAM_ENABLE);
		delay(20);

		bus_space_write_2(iot, ioh, ESA_GPIO_DIRECTION,
		    dir & ~ESA_GPO_PRIMARY_AC97);
		bus_space_write_2(iot, ioh, ESA_GPIO_MASK,
				  ~ESA_GPO_PRIMARY_AC97);
		bus_space_write_2(iot, ioh, ESA_GPIO_DATA, 0);
		bus_space_write_2(iot, ioh, ESA_GPIO_DIRECTION,
		    dir | ESA_GPO_PRIMARY_AC97);
		delay(sc->delay1 * 1000);
		bus_space_write_2(iot, ioh, ESA_GPIO_DATA,
				  ESA_GPO_PRIMARY_AC97);
		delay(5);
		bus_space_write_2(iot, ioh, ESA_RING_BUS_CTRL_A,
		    ESA_IO_SRAM_ENABLE | ESA_SERIAL_AC_LINK_ENABLE);
		bus_space_write_2(iot, ioh, ESA_GPIO_MASK, ~0);
		delay(sc->delay2 * 1000);

		esa_read_codec(sc, 0x7c, &data);
		if ((data == 0) || (data == 0xffff)) {
			retry++;
			if (retry > 3) {
				aprint_error_dev(sc->sc_dev,
				    "esa_codec_reset: failed\n");
				break;
			}
			aprint_normal_dev(sc->sc_dev,
			    "esa_codec_reset: retrying\n");
		} else
			retry = 0;
	} while (retry);

	return;
}

static int
esa_amp_enable(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t gpo, polarity_port, polarity;
	uint16_t data;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	switch (sc->type) {
	case ESS_ALLEGRO1:
		polarity_port = 0x1800;
		break;
	case ESS_MAESTRO3:
		polarity_port = 0x1100;
		break;
	default:
		aprint_error_dev(sc->sc_dev,
		    "esa_amp_enable: Unknown chip type!!!\n");
		return 1;
	}

	gpo = (polarity_port >> 8) & 0x0f;
	polarity = polarity_port >> 12;
	polarity = !polarity;	/* Enable */
	polarity = polarity << gpo;
	gpo = 1 << gpo;
	bus_space_write_2(iot, ioh, ESA_GPIO_MASK, ~gpo);
	data = bus_space_read_2(iot, ioh, ESA_GPIO_DIRECTION);
	bus_space_write_2(iot, ioh, ESA_GPIO_DIRECTION, data | gpo);
	data = ESA_GPO_SECONDARY_AC97 | ESA_GPO_PRIMARY_AC97 | polarity;
	bus_space_write_2(iot, ioh, ESA_GPIO_DATA, data);
	bus_space_write_2(iot, ioh, ESA_GPIO_MASK, ~0);

	return 0;
}

static void
esa_enable_interrupts(struct esa_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint8_t data;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL,
	    ESA_ASSP_INT_ENABLE | ESA_HV_INT_ENABLE);
	data = bus_space_read_1(iot, ioh, ESA_ASSP_CONTROL_C);
	bus_space_write_1(iot, ioh, ESA_ASSP_CONTROL_C,
	    data | ESA_ASSP_HOST_INT_ENABLE);
}

/*
 * List management
 */
static int
esa_add_list(struct esa_voice *vc, struct esa_list *el,
	     uint16_t val, int index)
{
	struct esa_softc *sc;

	sc = device_private(vc->parent);
	el->indexmap[index] = el->currlen;
	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       el->mem_addr + el->currlen,
		       val);

	return el->currlen++;
}

static void
esa_remove_list(struct esa_voice *vc, struct esa_list *el, int index)
{
	struct esa_softc *sc;
	uint16_t val;
	int lastindex;
	int vindex;
	int i;

	sc = device_private(vc->parent);
	lastindex = el->currlen - 1;
	vindex = el->indexmap[index];

	/* reset our virtual index */
	el->indexmap[index] = -1;

	if (vindex != lastindex) {
		val = esa_read_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
				    el->mem_addr + lastindex);
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
			       el->mem_addr + vindex,
			       val);
		for (i = 0; i < ESA_NUM_VOICES * 2; i++)
			if (el->indexmap[i] == lastindex)
				break;
		if (i >= ESA_NUM_VOICES * 2)
			aprint_error_dev(sc->sc_dev,
			    "esa_remove_list: invalid task index\n");
		else
			el->indexmap[i] = vindex;
	}

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA,
		       el->mem_addr + lastindex, 0);
	el->currlen--;

	return;
}

static bool
esa_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct esa_softc *sc = device_private(dv);
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, index;

	index = 0;

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);

	bus_space_write_2(iot, ioh, ESA_HOST_INT_CTRL, 0);
	bus_space_write_1(iot, ioh, ESA_ASSP_CONTROL_C, 0);

	esa_assp_halt(sc);

	/* Save ASSP state */
	for (i = ESA_REV_B_CODE_MEMORY_BEGIN; i <= ESA_REV_B_CODE_MEMORY_END;
	    i++)
		sc->savemem[index++] = esa_read_assp(sc,
		    ESA_MEMTYPE_INTERNAL_CODE, i);
	for (i = ESA_REV_B_DATA_MEMORY_BEGIN; i <= ESA_REV_B_DATA_MEMORY_END;
	    i++)
		sc->savemem[index++] = esa_read_assp(sc,
		    ESA_MEMTYPE_INTERNAL_DATA, i);

	mutex_spin_exit(&sc->sc_intr_lock);
	mutex_exit(&sc->sc_lock);

	return true;
}

static bool
esa_resume(device_t dv, const pmf_qual_t *qual)
{
	struct esa_softc *sc = device_private(dv);
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, index;
	uint8_t reset_state;
	pcireg_t data;

	index = 0;

	delay(10000);

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);

	data = pci_conf_read(sc->sc_pct, sc->sc_tag, PCI_LEGACY_AUDIO_CTRL);
	pci_conf_write(sc->sc_pct, sc->sc_tag, PCI_LEGACY_AUDIO_CTRL,
	    data | DISABLE_LEGACY);

	bus_space_write_4(iot, ioh, ESA_PCI_ACPI_CONTROL, ESA_PCI_ACPI_D0);

	esa_config(sc);

	reset_state = esa_assp_halt(sc);

	esa_init_codec(sc);
	esa_codec_reset(sc);

	/* restore ASSP */
	for (i = ESA_REV_B_CODE_MEMORY_BEGIN; i <= ESA_REV_B_CODE_MEMORY_END;
	    i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_CODE, i,
		    sc->savemem[index++]);
	for (i = ESA_REV_B_DATA_MEMORY_BEGIN; i <= ESA_REV_B_DATA_MEMORY_END;
	    i++)
		esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, i,
		    sc->savemem[index++]);

	esa_write_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, ESA_KDATA_DMA_ACTIVE, 0);
	bus_space_write_1(iot, ioh, ESA_DSP_PORT_CONTROL_REG_B,
	    reset_state | ESA_REGB_ENABLE_RESET);

	esa_enable_interrupts(sc);
	esa_amp_enable(sc);

	mutex_spin_exit(&sc->sc_intr_lock);

	/* Finally, power up AC97 codec */
	delay(1000);

	sc->codec_if->vtbl->restore_ports(sc->codec_if);

	mutex_exit(&sc->sc_lock);

	return true;
}

static uint32_t
esa_get_pointer(struct esa_softc *sc, struct esa_channel *ch)
{
	uint16_t hi, lo;
	uint32_t addr;
	int data_offset;

	data_offset = ch->data_offset;
	hi = esa_read_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, data_offset +
	    ESA_CDATA_HOST_SRC_CURRENTH);
	lo = esa_read_assp(sc, ESA_MEMTYPE_INTERNAL_DATA, data_offset +
	    ESA_CDATA_HOST_SRC_CURRENTL);

	addr = lo | ((uint32_t)hi << 16);
	return (addr - ch->start);
}

static paddr_t
esa_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct esa_voice *vc;
	struct esa_softc *sc;
	struct esa_dma *p;

	vc = addr;
	sc = device_private(vc->parent);
	if (off < 0)
		return -1;
	for (p = vc->dma; p && KERNADDR(p) != mem; p = p->next)
		continue;
	if (p == NULL)
		return -1;
	return bus_dmamem_mmap(sc->sc_dmat, p->segs, p->nsegs,
			       off, prot, BUS_DMA_WAITOK);
}

static void
esa_get_locks(void *addr, kmutex_t **intr, kmutex_t **proc)
{
	struct esa_voice *vc;
	struct esa_softc *sc;

	vc = addr;
	sc = device_private(vc->parent);

	*intr = &sc->sc_intr_lock;
	*proc = &sc->sc_lock;
}

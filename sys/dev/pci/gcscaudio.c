/*	$NetBSD: gcscaudio.c,v 1.14 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2008 SHIMIZU Ryo <ryo@nerv.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: gcscaudio.c,v 1.14 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>
#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/gcscaudioreg.h>


#define	GCSCAUDIO_NPRDTABLE	256	/* including a JMP-PRD for loop */
#define	GCSCAUDIO_PRD_SIZE_MAX	65532	/* limited by CS5536 Controller */
#define	GCSCAUDIO_BUFSIZE_MAX	(GCSCAUDIO_PRD_SIZE_MAX * (GCSCAUDIO_NPRDTABLE - 1))

struct gcscaudio_prd {
	/* PRD table for play/rec */
	struct gcscaudio_prdtables {
#define	PRD_TABLE_FRONT		0
#define	PRD_TABLE_SURR		1
#define	PRD_TABLE_CENTER	2
#define	PRD_TABLE_LFE		3
#define	PRD_TABLE_REC		4
#define	PRD_TABLE_MAX		5
		struct acc_prd prdtbl[PRD_TABLE_MAX][GCSCAUDIO_NPRDTABLE];
	} *p_prdtables;
	bus_dmamap_t p_prdmap;
	bus_dma_segment_t p_prdsegs[1];
	int p_prdnseg;
};

struct gcscaudio_dma {
	LIST_ENTRY(gcscaudio_dma) list;
	bus_dmamap_t map;
	void *addr;
	size_t size;
	bus_dma_segment_t segs[1];
	int nseg;
};

struct gcscaudio_softc_ch {
	void (*ch_intr)(void *);
	void *ch_intr_arg;
	struct audio_params ch_params;
};

struct gcscaudio_softc {
	device_t sc_dev;
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pt;
	void *sc_ih;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_size_t sc_ios;
	bus_dma_tag_t sc_dmat;

	/* allocated DMA buffer list */
	LIST_HEAD(, gcscaudio_dma) sc_dmalist;

#define GCSCAUDIO_MAXFORMATS	4
	struct audio_format sc_formats[GCSCAUDIO_MAXFORMATS];
	int sc_nformats;
	struct audio_encoding_set *sc_encodings;

	/* AC97 codec */
	struct ac97_host_if host_if;
	struct ac97_codec_if *codec_if;

	/* input, output channels */
	struct gcscaudio_softc_ch sc_play;
	struct gcscaudio_softc_ch sc_rec;
	struct gcscaudio_prd sc_prd;

	/* multi channel splitter work; {4,6}ch stream to {2,4} DMA buffers */
	void *sc_mch_split_buf;
	void *sc_mch_split_start;
	int sc_mch_split_off;
	int sc_mch_split_size;
	int sc_mch_split_blksize;
	void (*sc_mch_splitter)(void *, void *, int, int);
	bool sc_spdif;
};

/* for cfattach */
static int gcscaudio_match(device_t, cfdata_t, void *);
static void gcscaudio_attach(device_t, device_t, void *);

/* for audio_hw_if */
static int gcscaudio_open(void *, int);
static void gcscaudio_close(void *);
static int gcscaudio_query_encoding(void *, struct audio_encoding *);
static int gcscaudio_set_params(void *, int, int, audio_params_t *,
                                audio_params_t *, stream_filter_list_t *,
                                stream_filter_list_t *);
static int gcscaudio_round_blocksize(void *, int, int, const audio_params_t *);
static int gcscaudio_halt_output(void *);
static int gcscaudio_halt_input(void *);
static int gcscaudio_getdev(void *, struct audio_device *);
static int gcscaudio_set_port(void *, mixer_ctrl_t *);
static int gcscaudio_get_port(void *, mixer_ctrl_t *);
static int gcscaudio_query_devinfo(void *, mixer_devinfo_t *);
static void *gcscaudio_malloc(void *, int, size_t);
static void gcscaudio_free(void *, void *, size_t);
static size_t gcscaudio_round_buffersize(void *, int, size_t);
static paddr_t gcscaudio_mappage(void *, void *, off_t, int);
static int gcscaudio_get_props(void *);
static int gcscaudio_trigger_output(void *, void *, void *, int,
                                    void (*)(void *), void *,
                                    const audio_params_t *);
static int gcscaudio_trigger_input(void *, void *, void *, int,
                                   void (*)(void *), void *,
                                   const audio_params_t *);
static void gcscaudio_get_locks(void *, kmutex_t **, kmutex_t **);
static bool gcscaudio_resume(device_t, const pmf_qual_t *);
static int gcscaudio_intr(void *);

/* for codec_if */
static int gcscaudio_attach_codec(void *, struct ac97_codec_if *);
static int gcscaudio_write_codec(void *, uint8_t, uint16_t);
static int gcscaudio_read_codec(void *, uint8_t, uint16_t *);
static int gcscaudio_reset_codec(void *);
static void gcscaudio_spdif_event_codec(void *, bool);

/* misc */
static int gcscaudio_append_formats(struct gcscaudio_softc *,
                                    const struct audio_format *);
static int gcscaudio_wait_ready_codec(struct gcscaudio_softc *sc, const char *);
static int gcscaudio_set_params_ch(struct gcscaudio_softc *,
                                   struct gcscaudio_softc_ch *, int,
                                   audio_params_t *, stream_filter_list_t *);
static int gcscaudio_allocate_dma(struct gcscaudio_softc *, size_t, void **,
                                  bus_dma_segment_t *, int, int *,
                                  bus_dmamap_t *);


CFATTACH_DECL_NEW(gcscaudio, sizeof (struct gcscaudio_softc),
    gcscaudio_match, gcscaudio_attach, NULL, NULL);


static struct audio_device gcscaudio_device = {
	"AMD Geode CS5536",
	"",
	"gcscaudio"
};

static const struct audio_hw_if gcscaudio_hw_if = {
	.open			= gcscaudio_open,
	.close			= gcscaudio_close,
	.drain			= NULL,
	.query_encoding		= gcscaudio_query_encoding,
	.set_params		= gcscaudio_set_params,
	.round_blocksize	= gcscaudio_round_blocksize,
	.commit_settings	= NULL,
	.init_output		= NULL,
	.init_input		= NULL,
	.start_output		= NULL,
	.start_input		= NULL,
	.halt_output		= gcscaudio_halt_output,
	.halt_input		= gcscaudio_halt_input,
	.speaker_ctl		= NULL,
	.getdev			= gcscaudio_getdev,
	.setfd			= NULL,
	.set_port		= gcscaudio_set_port,
	.get_port		= gcscaudio_get_port,
	.query_devinfo		= gcscaudio_query_devinfo,
	.allocm			= gcscaudio_malloc,
	.freem			= gcscaudio_free,
	.round_buffersize	= gcscaudio_round_buffersize,
	.mappage		= gcscaudio_mappage,
	.get_props		= gcscaudio_get_props,
	.trigger_output		= gcscaudio_trigger_output,
	.trigger_input		= gcscaudio_trigger_input,
	.dev_ioctl		= NULL,
	.get_locks		= gcscaudio_get_locks,
};

static const struct audio_format gcscaudio_formats_2ch = {
	NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	2, AUFMT_STEREO, 0, {8000, 48000}
};

static const struct audio_format gcscaudio_formats_4ch = {
	NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	4, AUFMT_SURROUND4, 0, {8000, 48000}
};

static const struct audio_format gcscaudio_formats_6ch = {
	NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	6, AUFMT_DOLBY_5_1, 0, {8000, 48000}
};

static int
gcscaudio_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD) &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_CS5536_AUDIO))
		return 1;

	return 0;
}

static int
gcscaudio_append_formats(struct gcscaudio_softc *sc,
                         const struct audio_format *format)
{
	if (sc->sc_nformats >= GCSCAUDIO_MAXFORMATS) {
		aprint_error_dev(sc->sc_dev, "too many formats\n");
		return EINVAL;
	}
	sc->sc_formats[sc->sc_nformats++] = *format;
	return 0;
}

static void
gcscaudio_attach(device_t parent, device_t self, void *aux)
{
	struct gcscaudio_softc *sc;
	struct pci_attach_args *pa;
	const char *intrstr;
	pci_intr_handle_t ih;
	int rc, i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);

	sc->sc_dev = self;

	aprint_naive(": Audio controller\n");

	pa = aux;
	sc->sc_pc = pa->pa_pc;
	sc->sc_pt = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	LIST_INIT(&sc->sc_dmalist);
	sc->sc_mch_split_buf = NULL;
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	aprint_normal(": AMD Geode CS5536 Audio\n");

	if (pci_mapreg_map(pa, PCI_MAPREG_START, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_ios)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		goto attach_failure_unmap;
	}
	intrstr = pci_intr_string(sc->sc_pc, ih, intrbuf, sizeof(intrbuf));

	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_AUDIO,
	    gcscaudio_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto attach_failure_unmap;
	}

	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);


	if (gcscaudio_allocate_dma(sc, sizeof(*sc->sc_prd.p_prdtables),
	    (void **)&(sc->sc_prd.p_prdtables), sc->sc_prd.p_prdsegs, 1,
	    &(sc->sc_prd.p_prdnseg), &(sc->sc_prd.p_prdmap)) != 0)
		goto attach_failure_intr;

	sc->host_if.arg = sc;
	sc->host_if.attach = gcscaudio_attach_codec;
	sc->host_if.read = gcscaudio_read_codec;
	sc->host_if.write = gcscaudio_write_codec;
	sc->host_if.reset = gcscaudio_reset_codec;
	sc->host_if.spdif_event = gcscaudio_spdif_event_codec;

	if ((rc = ac97_attach(&sc->host_if, self, &sc->sc_lock)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "can't attach codec (error=%d)\n", rc);
		goto attach_failure_intr;
	}

	if (!pmf_device_register(self, NULL, gcscaudio_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");


	sc->sc_nformats = 0;
	gcscaudio_append_formats(sc, &gcscaudio_formats_2ch);

	mutex_enter(&sc->sc_lock);
	if (AC97_IS_4CH(sc->codec_if))
		gcscaudio_append_formats(sc, &gcscaudio_formats_4ch);
	if (AC97_IS_6CH(sc->codec_if))
		gcscaudio_append_formats(sc, &gcscaudio_formats_6ch);
	if (AC97_IS_FIXED_RATE(sc->codec_if)) {
		for (i = 0; i < sc->sc_nformats; i++) {
			sc->sc_formats[i].frequency_type = 1;
			sc->sc_formats[i].frequency[0] = 48000;
		}
	}
	mutex_exit(&sc->sc_lock);

	if ((rc = auconv_create_encodings(sc->sc_formats, sc->sc_nformats,
	    &sc->sc_encodings)) != 0) {
		aprint_error_dev(self,
		    "auconv_create_encoding: error=%d\n", rc);
		goto attach_failure_codec;
	}

	audio_attach_mi(&gcscaudio_hw_if, sc, sc->sc_dev);
	return;

attach_failure_codec:
	sc->codec_if->vtbl->detach(sc->codec_if);
attach_failure_intr:
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
attach_failure_unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	return;
}

static int
gcscaudio_attach_codec(void *arg, struct ac97_codec_if *codec_if)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	sc->codec_if = codec_if;
	return 0;
}

static int
gcscaudio_reset_codec(void *arg)
{
	struct gcscaudio_softc *sc;
	sc = (struct gcscaudio_softc *)arg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_CODEC_CNTL,
	    ACC_CODEC_CNTL_LNK_WRM_RST |
	    ACC_CODEC_CNTL_CMD_NEW);

	if (gcscaudio_wait_ready_codec(sc, "reset timeout\n"))
		return 1;

	return 0;
}

static void
gcscaudio_spdif_event_codec(void *arg, bool flag)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	sc->sc_spdif = flag;
}

static int
gcscaudio_wait_ready_codec(struct gcscaudio_softc *sc, const char *timeout_msg)
{
	int i;

#define GCSCAUDIO_WAIT_READY_CODEC_TIMEOUT	500
	for (i = GCSCAUDIO_WAIT_READY_CODEC_TIMEOUT; (i >= 0) &&
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, ACC_CODEC_CNTL) &
	    ACC_CODEC_CNTL_CMD_NEW); i--)
		delay(1);

	if (i < 0) {
		aprint_error_dev(sc->sc_dev, "%s", timeout_msg);
		return 1;
	}

	return 0;
}

static int
gcscaudio_write_codec(void *arg, uint8_t reg, uint16_t val)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_CODEC_CNTL,
	    ACC_CODEC_CNTL_WRITE_CMD |
	    ACC_CODEC_CNTL_CMD_NEW |
	    ACC_CODEC_REG2ADDR(reg) |
	    (val & ACC_CODEC_CNTL_CMD_DATA_MASK));

	if (gcscaudio_wait_ready_codec(sc, "codec write timeout\n"))
		return 1;

#ifdef GCSCAUDIO_CODEC_DEBUG
	aprint_error_dev(sc->sc_dev, "codec write: reg=0x%02x, val=0x%04x\n",
	    reg, val);
#endif

	return 0;
}

static int
gcscaudio_read_codec(void *arg, uint8_t reg, uint16_t *val)
{
	struct gcscaudio_softc *sc;
	uint32_t v;
	int i;

	sc = (struct gcscaudio_softc *)arg;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_CODEC_CNTL,
	    ACC_CODEC_CNTL_READ_CMD | ACC_CODEC_CNTL_CMD_NEW |
	    ACC_CODEC_REG2ADDR(reg));

	if (gcscaudio_wait_ready_codec(sc, "codec write timeout for reading"))
		return 1;

#define GCSCAUDIO_READ_CODEC_TIMEOUT	50
	for (i = GCSCAUDIO_READ_CODEC_TIMEOUT; i >= 0; i--) {
		v = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ACC_CODEC_STATUS);
		if ((v & ACC_CODEC_STATUS_STS_NEW) &&
		    (ACC_CODEC_ADDR2REG(v) == reg))
			break;

		delay(10);
	}

	if (i < 0) {
		aprint_error_dev(sc->sc_dev, "codec read timeout\n");
		return 1;
	}

#ifdef GCSCAUDIO_CODEC_DEBUG
	aprint_error_dev(sc->sc_dev, "codec read: reg=0x%02x, val=0x%04x\n",
	    reg, v & ACC_CODEC_STATUS_STS_DATA_MASK);
#endif

	*val = v;
	return 0;
}

static int
gcscaudio_open(void *arg, int flags)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	sc->codec_if->vtbl->lock(sc->codec_if);
	return 0;
}

static void
gcscaudio_close(void *arg)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	sc->codec_if->vtbl->unlock(sc->codec_if);
}

static int
gcscaudio_query_encoding(void *arg, struct audio_encoding *fp)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	return auconv_query_encoding(sc->sc_encodings, fp);
}

static int
gcscaudio_set_params_ch(struct gcscaudio_softc *sc,
                        struct gcscaudio_softc_ch *ch, int mode,
                        audio_params_t *p, stream_filter_list_t *fil)
{
	int error, idx;

	if ((p->sample_rate < 8000) || (p->sample_rate > 48000))
		return EINVAL;

	if (p->precision != 8 && p->precision != 16)
		return EINVAL;

	if ((idx = auconv_set_converter(sc->sc_formats, sc->sc_nformats,
	    mode, p, TRUE, fil)) < 0)
		return EINVAL;

	if (fil->req_size > 0)
		p = &fil->filters[0].param;

	if (mode == AUMODE_PLAY) {
		if (!AC97_IS_FIXED_RATE(sc->codec_if)) {
			/* setup rate of DAC */
			if ((error = sc->codec_if->vtbl->set_rate(sc->codec_if,
			    AC97_REG_PCM_FRONT_DAC_RATE, &p->sample_rate)) != 0)
				return error;

			/* additional rate of DAC for Surround */
			if ((p->channels >= 4) &&
			    (error = sc->codec_if->vtbl->set_rate(sc->codec_if,
			    AC97_REG_PCM_SURR_DAC_RATE, &p->sample_rate)) != 0)
				return error;

			/* additional rate of DAC for LowFrequencyEffect */
			if ((p->channels == 6) &&
			    (error = sc->codec_if->vtbl->set_rate(sc->codec_if,
			    AC97_REG_PCM_LFE_DAC_RATE, &p->sample_rate)) != 0)
				return error;
		}
	}

	if (mode == AUMODE_RECORD) {
		if (!AC97_IS_FIXED_RATE(sc->codec_if)) {
			/* setup rate of ADC */
			if ((error = sc->codec_if->vtbl->set_rate(sc->codec_if,
			    AC97_REG_PCM_LR_ADC_RATE, &p->sample_rate)) != 0)
				return error;
		}
	}

	ch->ch_params = *p;
	return 0;
}

static int
gcscaudio_set_params(void *arg, int setmode, int usemode,
                     audio_params_t *play, audio_params_t *rec,
                     stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct gcscaudio_softc *sc;
	int error;

	sc = (struct gcscaudio_softc *)arg;

	if (setmode & AUMODE_PLAY) {
		if ((error = gcscaudio_set_params_ch(sc, &sc->sc_play,
		    AUMODE_PLAY, play, pfil)) != 0)
			return error;
	}
	if (setmode & AUMODE_RECORD) {
		if ((error = gcscaudio_set_params_ch(sc, &sc->sc_rec,
		    AUMODE_RECORD, rec, rfil)) != 0)
			return error;
	}

	return 0;
}

static int
gcscaudio_round_blocksize(void *arg, int blk, int mode,
                          const audio_params_t *param)
{
	blk &= -4;
	if (blk > GCSCAUDIO_PRD_SIZE_MAX)
		blk = GCSCAUDIO_PRD_SIZE_MAX;

	return blk;
}

static int
gcscaudio_halt_output(void *arg)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM0_CMD,
	    ACC_BMx_CMD_BM_CTL_DISABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM4_CMD,
	    ACC_BMx_CMD_BM_CTL_DISABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM6_CMD,
	    ACC_BMx_CMD_BM_CTL_DISABLE);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM7_CMD,
	    ACC_BMx_CMD_BM_CTL_DISABLE);
	sc->sc_play.ch_intr = NULL;

	/* channel splitter */
	sc->sc_mch_splitter = NULL;
	if (sc->sc_mch_split_buf)
		gcscaudio_free(sc, sc->sc_mch_split_buf, sc->sc_mch_split_size);
	sc->sc_mch_split_buf = NULL;

	return 0;
}

static int
gcscaudio_halt_input(void *arg)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM1_CMD,
	    ACC_BMx_CMD_BM_CTL_DISABLE);
	sc->sc_rec.ch_intr = NULL;
	return 0;
}

static int
gcscaudio_getdev(void *addr, struct audio_device *retp)
{
	*retp = gcscaudio_device;
	return 0;
}

static int
gcscaudio_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct gcscaudio_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

static int
gcscaudio_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct gcscaudio_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

static int
gcscaudio_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct gcscaudio_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip);
}

static void *
gcscaudio_malloc(void *arg, int direction, size_t size)
{
	struct gcscaudio_softc *sc;
	struct gcscaudio_dma *p;
	int error;

	sc = (struct gcscaudio_softc *)arg;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	p->size = size;

	error = gcscaudio_allocate_dma(sc, size, &p->addr,
	    p->segs, sizeof(p->segs)/sizeof(p->segs[0]), &p->nseg, &p->map);
	if (error) {
		kmem_free(p, sizeof(*p));
		return NULL;
	}

	LIST_INSERT_HEAD(&sc->sc_dmalist, p, list);
	return p->addr;
}

static void
gcscaudio_free(void *arg, void *ptr, size_t size)
{
	struct gcscaudio_softc *sc;
	struct gcscaudio_dma *p;

	sc = (struct gcscaudio_softc *)arg;

	LIST_FOREACH(p, &sc->sc_dmalist, list) {
		if (p->addr == ptr) {
			bus_dmamap_unload(sc->sc_dmat, p->map);
			bus_dmamap_destroy(sc->sc_dmat, p->map);
			bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmat, p->segs, p->nseg);

			LIST_REMOVE(p, list);
			kmem_free(p, sizeof(*p));
			break;
		}
	}
}

static paddr_t
gcscaudio_mappage(void *arg, void *mem, off_t off, int prot)
{
	struct gcscaudio_softc *sc;
	struct gcscaudio_dma *p;

	if (off < 0)
		return -1;

	sc = (struct gcscaudio_softc *)arg;
	LIST_FOREACH(p, &sc->sc_dmalist, list) {
		if (p->addr == mem) {
			return bus_dmamem_mmap(sc->sc_dmat, p->segs, p->nseg,
			    off, prot, BUS_DMA_WAITOK);
		}
	}

	return -1;
}

static size_t
gcscaudio_round_buffersize(void *addr, int direction, size_t size)
{
	if (size > GCSCAUDIO_BUFSIZE_MAX)
		size = GCSCAUDIO_BUFSIZE_MAX;

	return size;
}

static int
gcscaudio_get_props(void *addr)
{
	struct gcscaudio_softc *sc;
	int props;

	sc = (struct gcscaudio_softc *)addr;
	props = AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
	/*
	 * Even if the codec is fixed-rate, set_param() succeeds for any sample
	 * rate because of aurateconv.  Applications can't know what rate the
	 * device can process in the case of mmap().
	 */
	if (!AC97_IS_FIXED_RATE(sc->codec_if))
		props |= AUDIO_PROP_MMAP;
	return props;
}

static int
build_prdtables(struct gcscaudio_softc *sc, int prdidx,
                void *addr, size_t size, int blksize, int blklen, int blkoff)
{
	struct gcscaudio_dma *p;
	struct acc_prd *prdp;
	bus_addr_t paddr;
	int i;

	/* get physical address of start */
	paddr = (bus_addr_t)0;
	LIST_FOREACH(p, &sc->sc_dmalist, list) {
		if (p->addr == addr) {
			paddr = p->map->dm_segs[0].ds_addr;
			break;
		}
	}
	if (!paddr) {
		aprint_error_dev(sc->sc_dev, "bad addr %p\n", addr);
		return EINVAL;
	}

#define PRDADDR(prdidx,idx) \
	(sc->sc_prd.p_prdmap->dm_segs[0].ds_addr) + sizeof(struct acc_prd) * \
	(((prdidx) * GCSCAUDIO_NPRDTABLE) + (idx))

	/*
	 * build PRD table
	 *   prdtbl[] = <PRD0>, <PRD1>, <PRD2>, ..., <PRDn>, <jmp to PRD0>
	 */
	prdp = sc->sc_prd.p_prdtables->prdtbl[prdidx];
	for (i = 0; size > 0; size -= blksize, i++) {
		prdp[i].address = paddr + blksize * i + blkoff;
		prdp[i].ctrlsize =
		    (size < blklen ? size : blklen) | ACC_BMx_PRD_CTRL_EOP;
	}
	prdp[i].address = PRDADDR(prdidx, 0);
	prdp[i].ctrlsize = ACC_BMx_PRD_CTRL_JMP;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_prd.p_prdmap, 0,
	    sizeof(struct acc_prd) * i, BUS_DMASYNC_PREWRITE);

	return 0;
}

static void
split_buffer_4ch(void *dst, void *src, int size, int blksize)
{
	int left, i;
	uint16_t *s, *d;

	/*
	 * src[blk0]: L,R,SL,SR,L,R,SL,SR,L,R,SL,SR,....
	 * src[blk1]: L,R,SL,SR,L,R,SL,SR,L,R,SL,SR,....
	 * src[blk2]: L,R,SL,SR,L,R,SL,SR,L,R,SL,SR,....
	 *     :
	 *
	 *   rearrange to
	 *
	 * src[blk0]: L,R,L,R,L,R,L,R,..
	 * src[blk1]: L,R,L,R,L,R,L,R,..
	 * src[blk2]: L,R,L,R,L,R,L,R,..
	 *     :
	 * dst[blk0]: SL,SR,SL,SR,SL,SR,SL,SR,..
	 * dst[blk1]: SL,SR,SL,SR,SL,SR,SL,SR,..
	 * dst[blk2]: SL,SR,SL,SR,SL,SR,SL,SR,..
	 *     :
	 */
	for (left = size; left > 0; left -= blksize) {
		s = (uint16_t *)src;
		d = (uint16_t *)dst;
		for (i = 0; i < blksize / sizeof(uint16_t) / 4; i++) {
			/* L,R,SL,SR -> SL,SR */
			s++;
			s++;
			*d++ = *s++;
			*d++ = *s++;
		}

		s = (uint16_t *)src;
		d = (uint16_t *)src;
		for (i = 0; i < blksize / sizeof(uint16_t) / 2 / 2; i++) {
			/* L,R,SL,SR -> L,R */
			*d++ = *s++;
			*d++ = *s++;
			s++;
			s++;
		}

		src = (char *)src + blksize;
		dst = (char *)dst + blksize;
	}
}

static void
split_buffer_6ch(void *dst, void *src, int size, int blksize)
{
	int left, i;
	uint16_t *s, *d, *dc, *dl;

	/*
	 * by default, treat as WAV style 5.1ch order
	 *   5.1ch(WAV): L R C LFE SL SR
	 *   5.1ch(AAC): C L R SL SR LFE
	 *        :
	 */

	/*
	 * src[blk0]: L,R,C,LFE,SL,SR,L,R,C,LFE,SL,SR,...
	 * src[blk1]: L,R,C,LFE,SL,SR,L,R,C,LFE,SL,SR,...
	 * src[blk2]: L,R,C,LFE,SL,SR,L,R,C,LFE,SL,SR,...
	 *     :
	 * src[N-1] : L,R,C,LFE,SL,SR,L,R,C,LFE,SL,SR,...
	 *
	 *   rearrange to
	 *
	 * src[blk0]: L,R,L,R,..
	 * src[blk1]: L,R,L,R,..
	 * src[blk2]: L,R,L,R,..
	 *     :
	 *
	 * dst[blk0]: SL,SR,SL,SR,..
	 * dst[blk1]: SL,SR,SL,SR,..
	 * dst[blk2]: SL,SR,SL,SR,..
	 *     :
	 *
	 * dst[N/2+0]: C,C,C,..
	 * dst[N/2+1]: C,C,C,..
	 *     :
	 *
	 * dst[N/2+N/4+0]: LFE,LFE,LFE,..
	 * dst[N/2+N/4+1]: LFE,LFE,LFE,..
	 *     :
	 */

	for (left = size; left > 0; left -= blksize) {
		s = (uint16_t *)src;
		d = (uint16_t *)dst;
		dc = (uint16_t *)((char *)dst + blksize / 2);
		dl = (uint16_t *)((char *)dst + blksize / 2 + blksize / 4);
		for (i = 0; i < blksize / sizeof(uint16_t) / 6; i++) {
#ifdef GCSCAUDIO_5_1CH_AAC_ORDER
			/*
			 * AAC: [C,L,R,SL,SR,LFE]
			 *  => [SL,SR]
			 *  => [C]
			 *  => [LFE]
			 */
			*dc++ = s[0];	/* C */
			*dl++ = s[5];	/* LFE */
			*d++ = s[3];	/* SL */
			*d++ = s[4];	/* SR */
#else
			/*
			 * WAV: [L,R,C,LFE,SL,SR]
			 *  => [SL,SR]
			 *  => [C]
			 *  => [LFE]
			 */
			*dc++ = s[2];	/* C */
			*dl++ = s[3];	/* LFE */
			*d++ = s[4];	/* SL */
			*d++ = s[5];	/* SR */
#endif
			s += 6;
		}

		s = (uint16_t *)src;
		d = (uint16_t *)src;
		for (i = 0; i < blksize / sizeof(uint16_t) / 2 / 2; i++) {
#ifdef GCSCAUDIO_5_1CH_AAC_ORDER
			/* AAC: [C,L,R,SL,SR,LFE] => [L,R] */
			*d++ = s[1];
			*d++ = s[2];
#else
			/* WAV: [L,R,C,LFE,SL,SR] => [L,R] */
			*d++ = s[0];
			*d++ = s[1];
#endif
			s += 6;
		}

		src = (char *)src + blksize;
		dst = (char *)dst + blksize;
	}
}

static void
channel_splitter(struct gcscaudio_softc *sc)
{
	int splitsize, left;
	void *src, *dst;

	if (sc->sc_mch_splitter == NULL)
		return;

	left = sc->sc_mch_split_size - sc->sc_mch_split_off;
	splitsize = sc->sc_mch_split_blksize;
	if (left < splitsize)
		splitsize = left;

	src = (char *)sc->sc_mch_split_start + sc->sc_mch_split_off;
	dst = (char *)sc->sc_mch_split_buf + sc->sc_mch_split_off;

	sc->sc_mch_splitter(dst, src, splitsize, sc->sc_mch_split_blksize);

	sc->sc_mch_split_off += sc->sc_mch_split_blksize;
	if (sc->sc_mch_split_off >= sc->sc_mch_split_size)
		sc->sc_mch_split_off = 0;
}

static int
gcscaudio_trigger_output(void *addr, void *start, void *end, int blksize,
                         void (*intr)(void *), void *arg,
                         const audio_params_t *param)
{
	struct gcscaudio_softc *sc;
	size_t size;

	sc = (struct gcscaudio_softc *)addr;
	sc->sc_play.ch_intr = intr;
	sc->sc_play.ch_intr_arg = arg;
	size = (char *)end - (char *)start;

	switch (sc->sc_play.ch_params.channels) {
	case 2:
		if (build_prdtables(sc, PRD_TABLE_FRONT, start, size, blksize,
		    blksize, 0))
			return EINVAL;

		if (!AC97_IS_4CH(sc->codec_if)) {
			/*
			 * output 2ch PCM to FRONT.LR(BM0)
			 *
			 * 2ch: L,R,L,R,L,R,L,R,... => BM0: L,R,L,R,L,R,L,R,...
			 *
			 */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM0_PRD,
			    PRDADDR(PRD_TABLE_FRONT, 0));

			/* start DMA transfer */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM0_CMD,
			    ACC_BMx_CMD_WRITE |
			    ACC_BMx_CMD_BYTE_ORD_EL |
			    ACC_BMx_CMD_BM_CTL_ENABLE);
		} else {
			/*
			 * output same PCM to FRONT.LR(BM0) and SURROUND.LR(BM6).
			 * CENTER(BM4) and LFE(BM7) doesn't sound.
			 *
			 * 2ch: L,R,L,R,L,R,L,R,... => BM0: L,R,L,R,L,R,L,R,...
			 *                             BM6: (same of BM0)
			 *                             BM4: none
			 *                             BM7: none
			 */
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM0_PRD,
			    PRDADDR(PRD_TABLE_FRONT, 0));
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM6_PRD,
			    PRDADDR(PRD_TABLE_FRONT, 0));

			/* start DMA transfer */
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM0_CMD,
			    ACC_BMx_CMD_WRITE |
			    ACC_BMx_CMD_BYTE_ORD_EL |
			    ACC_BMx_CMD_BM_CTL_ENABLE);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM6_CMD,
			    ACC_BMx_CMD_WRITE |
			    ACC_BMx_CMD_BYTE_ORD_EL |
			    ACC_BMx_CMD_BM_CTL_ENABLE);
		}
		break;
	case 4:
		/*
		 * output 4ch PCM split to FRONT.LR(BM0) and SURROUND.LR(BM6).
		 * CENTER(BM4) and LFE(BM7) doesn't sound.
		 *
		 * rearrange ordered channel to continuous per channel
		 *
		 *   4ch: L,R,SL,SR,L,R,SL,SR,... => BM0: L,R,L,R,...
		 *                                   BM6: SL,SR,SL,SR,...
		 *                                   BM4: none
		 *                                   BM7: none
		 */
		if (sc->sc_mch_split_buf)
			gcscaudio_free(sc, sc->sc_mch_split_buf,
			    sc->sc_mch_split_size);

		if ((sc->sc_mch_split_buf = gcscaudio_malloc(sc, AUMODE_PLAY,
		    size)) == NULL)
			return ENOMEM;

		/*
		 * 1st and 2nd blocks are split immediately.
		 * Other blocks will be split synchronous with intr.
		 */
		split_buffer_4ch(sc->sc_mch_split_buf, start, blksize * 2,
		    blksize);

		sc->sc_mch_split_start = start;
		sc->sc_mch_split_size = size;
		sc->sc_mch_split_blksize = blksize;
		sc->sc_mch_split_off = (blksize * 2) % size;
		sc->sc_mch_splitter = split_buffer_4ch;	/* split function */

		if (build_prdtables(sc, PRD_TABLE_FRONT, start, size, blksize,
		    blksize / 2, 0))
			return EINVAL;
		if (build_prdtables(sc, PRD_TABLE_SURR, sc->sc_mch_split_buf,
		    size, blksize, blksize / 2, 0))
			return EINVAL;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM0_PRD,
		    PRDADDR(PRD_TABLE_FRONT, 0));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM6_PRD,
		    PRDADDR(PRD_TABLE_SURR, 0));

		/* start DMA transfer */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM0_CMD,
		    ACC_BMx_CMD_WRITE |
		    ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM6_CMD,
		    ACC_BMx_CMD_WRITE |
		    ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		break;
	case 6:
		/*
		 * output 6ch PCM split to
		 * FRONT.LR(BM0), SURROUND.LR(BM6), CENTER(BM4) and LFE(BM7)
		 *
		 * rearrange ordered channel to continuous per channel
		 *
		 *   5.1ch: L,R,C,LFE,SL,SR,... => BM0: L,R,...
		 *                                 BM4: C,...
		 *                                 BM6: SL,SR,...
		 *                                 BM7: LFE,...
		 *
		 */
		if (sc->sc_mch_split_buf)
			gcscaudio_free(sc, sc->sc_mch_split_buf,
			    sc->sc_mch_split_size);

		if ((sc->sc_mch_split_buf = gcscaudio_malloc(sc, AUMODE_PLAY,
		    size)) == NULL)
			return ENOMEM;

		/*
		 * 1st and 2nd blocks are split immediately.
		 * Other block will be split synchronous with intr.
		 */
		split_buffer_6ch(sc->sc_mch_split_buf, start, blksize * 2,
		    blksize);

		sc->sc_mch_split_start = start;
		sc->sc_mch_split_size = size;
		sc->sc_mch_split_blksize = blksize;
		sc->sc_mch_split_off = (blksize * 2) % size;
		sc->sc_mch_splitter = split_buffer_6ch;	/* split function */

		if (build_prdtables(sc, PRD_TABLE_FRONT, start, size, blksize,
		    blksize / 3, 0))
			return EINVAL;
		if (build_prdtables(sc, PRD_TABLE_CENTER, sc->sc_mch_split_buf,
		    size, blksize, blksize / 3, blksize / 2))
			return EINVAL;
		if (build_prdtables(sc, PRD_TABLE_SURR, sc->sc_mch_split_buf,
		    size, blksize, blksize / 3, 0))
			return EINVAL;
		if (build_prdtables(sc, PRD_TABLE_LFE, sc->sc_mch_split_buf,
		    size, blksize, blksize / 3, blksize / 2 + blksize / 4))
			return EINVAL;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM0_PRD,
		    PRDADDR(PRD_TABLE_FRONT, 0));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM4_PRD,
		    PRDADDR(PRD_TABLE_CENTER, 0));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM6_PRD,
		    PRDADDR(PRD_TABLE_SURR, 0));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM7_PRD,
		    PRDADDR(PRD_TABLE_LFE, 0));

		/* start DMA transfer */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM0_CMD,
		    ACC_BMx_CMD_WRITE | ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM4_CMD,
		    ACC_BMx_CMD_WRITE | ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM6_CMD,
		    ACC_BMx_CMD_WRITE | ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM7_CMD,
		    ACC_BMx_CMD_WRITE | ACC_BMx_CMD_BYTE_ORD_EL |
		    ACC_BMx_CMD_BM_CTL_ENABLE);
		break;
	}

	return 0;
}

static int
gcscaudio_trigger_input(void *addr, void *start, void *end, int blksize,
                        void (*intr)(void *), void *arg,
                        const audio_params_t *param)
{
	struct gcscaudio_softc *sc;
	size_t size;

	sc = (struct gcscaudio_softc *)addr;
	sc->sc_rec.ch_intr = intr;
	sc->sc_rec.ch_intr_arg = arg;
	size = (char *)end - (char *)start;

	if (build_prdtables(sc, PRD_TABLE_REC, start, size, blksize, blksize, 0))
		return EINVAL;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ACC_BM1_PRD,
	    PRDADDR(PRD_TABLE_REC, 0));

	/* start transfer */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ACC_BM1_CMD,
	    ACC_BMx_CMD_READ |
	    ACC_BMx_CMD_BYTE_ORD_EL |
	    ACC_BMx_CMD_BM_CTL_ENABLE);

	return 0;
}

static void
gcscaudio_get_locks(void *arg, kmutex_t **intr, kmutex_t **thread)
{
	struct gcscaudio_softc *sc;

	sc = (struct gcscaudio_softc *)arg;

	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

static int
gcscaudio_intr(void *arg)
{
	struct gcscaudio_softc *sc;
	uint16_t intr;
	uint8_t bmstat;
	int nintr;

	nintr = 0;
	sc = (struct gcscaudio_softc *)arg;

	mutex_spin_enter(&sc->sc_intr_lock);

	intr = bus_space_read_2(sc->sc_iot, sc->sc_ioh, ACC_IRQ_STATUS);
	if (intr == 0)
		goto done;

	/* Front output */
	if (intr & ACC_IRQ_STATUS_BM0_IRQ_STS) {
		bmstat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ACC_BM0_STATUS);
		if (bmstat & ACC_BMx_STATUS_BM_EOP_ERR)
			aprint_normal_dev(sc->sc_dev, "BM0: Bus Master Error\n");
		if (!(bmstat & ACC_BMx_STATUS_EOP))
			aprint_normal_dev(sc->sc_dev, "BM0: NO End of Page?\n");

		if (sc->sc_play.ch_intr) {
			sc->sc_play.ch_intr(sc->sc_play.ch_intr_arg);
			channel_splitter(sc);
		}
		nintr++;
	}

	/* Center output */
	if (intr & ACC_IRQ_STATUS_BM4_IRQ_STS) {
		bmstat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ACC_BM4_STATUS);
		if (bmstat & ACC_BMx_STATUS_BM_EOP_ERR)
			aprint_normal_dev(sc->sc_dev, "BM4: Bus Master Error\n");
		if (!(bmstat & ACC_BMx_STATUS_EOP))
			aprint_normal_dev(sc->sc_dev, "BM4: NO End of Page?\n");

		nintr++;
	}

	/* Surround output */
	if (intr & ACC_IRQ_STATUS_BM6_IRQ_STS) {
		bmstat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ACC_BM6_STATUS);
		if (bmstat & ACC_BMx_STATUS_BM_EOP_ERR)
			aprint_normal_dev(sc->sc_dev, "BM6: Bus Master Error\n");
		if (!(bmstat & ACC_BMx_STATUS_EOP))
			aprint_normal_dev(sc->sc_dev, "BM6: NO End of Page?\n");

		nintr++;
	}

	/* LowFrequencyEffect output */
	if (intr & ACC_IRQ_STATUS_BM7_IRQ_STS) {
		bmstat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ACC_BM7_STATUS);
		if (bmstat & ACC_BMx_STATUS_BM_EOP_ERR)
			aprint_normal_dev(sc->sc_dev, "BM7: Bus Master Error\n");
		if (!(bmstat & ACC_BMx_STATUS_EOP))
			aprint_normal_dev(sc->sc_dev, "BM7: NO End of Page?\n");

		nintr++;
	}

	/* record */
	if (intr & ACC_IRQ_STATUS_BM1_IRQ_STS) {
		bmstat = bus_space_read_1(sc->sc_iot, sc->sc_ioh, ACC_BM1_STATUS);
		if (bmstat & ACC_BMx_STATUS_BM_EOP_ERR)
			aprint_normal_dev(sc->sc_dev, "BM1: Bus Master Error\n");
		if (!(bmstat & ACC_BMx_STATUS_EOP))
			aprint_normal_dev(sc->sc_dev, "BM1: NO End of Page?\n");

		if (sc->sc_rec.ch_intr) {
			sc->sc_rec.ch_intr(sc->sc_rec.ch_intr_arg);
		}
		nintr++;
	}

#ifdef GCSCAUDIO_DEBUG
	if (intr & ACC_IRQ_STATUS_IRQ_STS)
		aprint_normal_dev(sc->sc_dev, "Codec GPIO IRQ Status\n");
	if (intr & ACC_IRQ_STATUS_WU_IRQ_STS)
		aprint_normal_dev(sc->sc_dev, "Codec GPIO Wakeup IRQ Status\n");
	if (intr & ACC_IRQ_STATUS_BM2_IRQ_STS)
		aprint_normal_dev(sc->sc_dev, "Audio Bus Master 2 IRQ Status\n");
	if (intr & ACC_IRQ_STATUS_BM3_IRQ_STS)
		aprint_normal_dev(sc->sc_dev, "Audio Bus Master 3 IRQ Status\n");
	if (intr & ACC_IRQ_STATUS_BM5_IRQ_STS)
		aprint_normal_dev(sc->sc_dev, "Audio Bus Master 5 IRQ Status\n");
#endif

done:
	mutex_spin_exit(&sc->sc_intr_lock);

	return nintr ? 1 : 0;
}

static bool
gcscaudio_resume(device_t dv, const pmf_qual_t *qual)
{
	struct gcscaudio_softc *sc = device_private(dv);

	gcscaudio_reset_codec(sc);
	DELAY(1000);
	(sc->codec_if->vtbl->restore_ports)(sc->codec_if);

	return true;
}

static int
gcscaudio_allocate_dma(struct gcscaudio_softc *sc, size_t size, void **addrp,
                       bus_dma_segment_t *seglist, int nseg, int *rsegp,
                       bus_dmamap_t *mapp)
{
	int error;

	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, seglist,
	    nseg, rsegp, BUS_DMA_WAITOK)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to allocate DMA buffer, error=%d\n", error);
		goto fail_alloc;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, seglist, nseg, size, addrp,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to map DMA buffer, error=%d\n",
		    error);
		goto fail_map;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, nseg, size, 0,
	    BUS_DMA_WAITOK, mapp)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to create DMA map, error=%d\n", error);
		goto fail_create;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, *mapp, *addrp, size, NULL,
	    BUS_DMA_WAITOK)) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "unable to load DMA map, error=%d\n", error);
		goto fail_load;
	}

	return 0;

fail_load:
	bus_dmamap_destroy(sc->sc_dmat, *mapp);
fail_create:
	bus_dmamem_unmap(sc->sc_dmat, *addrp, size);
fail_map:
	bus_dmamem_free(sc->sc_dmat, seglist, nseg);
fail_alloc:
	return error;
}

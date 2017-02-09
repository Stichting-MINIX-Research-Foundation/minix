/* $NetBSD: auixp.c,v 1.41 2014/10/18 08:33:28 snj Exp $ */

/*
 * Copyright (c) 2004, 2005 Reinoud Zandijk <reinoud@netbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * NetBSD audio driver for ATI IXP-{150,200,...} audio driver hardware.
 *
 * Recording and playback has been tested OK on various sample rates and
 * encodings.
 *
 * Known problems and issues :
 * - SPDIF is untested and needs some work still (LED stays off)
 * - 32 bit audio playback failed last time i tried but that might an AC'97
 *   codec support problem.
 * - 32 bit recording works but can't try out playing: see above.
 * - no suspend/resume support yet.
 * - multiple codecs are `supported' but not tested; the implemetation needs
 *   some cleaning up.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auixp.c,v 1.41 2014/10/18 08:33:28 snj Exp $");

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
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97var.h>
#include <dev/ic/ac97reg.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/auixpreg.h>
#include <dev/pci/auixpvar.h>


/* #define DEBUG_AUIXP */


/* why isn't this base address register not in the headerfile? */
#define PCI_CBIO 0x10


/* macro's used */
#define KERNADDR(p)	((void *)((p)->addr))
#define	DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)


/* the differences might be irrelevant */
enum {
	IXP_200,
	IXP_300,
	IXP_400
};


/* our `cards' */
static const struct auixp_card_type {
	uint16_t pci_vendor_id;
	uint16_t pci_product_id;
	int type;
} auixp_card_types[] = {
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_IXP_AUDIO_200, IXP_200 },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_IXP_AUDIO_300, IXP_300 },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_IXP_AUDIO_400, IXP_400 },
	{ 0, 0, 0 }
};


struct audio_device auixp_device = {
	"ATI IXP audio",
	"",
	"auixp"
};


/* codec detection constant indicating the interrupt flags */
#define ALL_CODECS_NOT_READY \
	    (ATI_REG_ISR_CODEC0_NOT_READY |\
	     ATI_REG_ISR_CODEC1_NOT_READY |\
	     ATI_REG_ISR_CODEC2_NOT_READY)
#define CODEC_CHECK_BITS (ALL_CODECS_NOT_READY|ATI_REG_ISR_NEW_FRAME)


/* autoconfig */
static int	auixp_match(device_t, cfdata_t, void *);
static void	auixp_attach(device_t, device_t, void *);
static int	auixp_detach(device_t, int);


/* audio(9) function prototypes */
static int	auixp_query_encoding(void *, struct audio_encoding *);
static int	auixp_set_params(void *, int, int, audio_params_t *,
				 audio_params_t *,
		stream_filter_list_t *, stream_filter_list_t *);
static int	auixp_commit_settings(void *);
static int	auixp_round_blocksize(void *, int, int, const audio_params_t *);
static int	auixp_trigger_output(void *, void *, void *, int,
				     void (*)(void *),
		void *, const audio_params_t *);
static int	auixp_trigger_input(void *, void *, void *, int,
				    void (*)(void *),
		void *, const audio_params_t *);
static int	auixp_halt_output(void *);
static int	auixp_halt_input(void *);
static int	auixp_set_port(void *, mixer_ctrl_t *);
static int	auixp_get_port(void *, mixer_ctrl_t *);
static int	auixp_query_devinfo(void *, mixer_devinfo_t *);
static void *	auixp_malloc(void *, int, size_t);
static void	auixp_free(void *, void *, size_t);
static int	auixp_getdev(void *, struct audio_device *);
static size_t	auixp_round_buffersize(void *, int, size_t);
static int	auixp_get_props(void *);
static int	auixp_intr(void *);
static int	auixp_allocmem(struct auixp_softc *, size_t, size_t,
		struct auixp_dma *);
static int	auixp_freemem(struct auixp_softc *, struct auixp_dma *);
static paddr_t	auixp_mappage(void *, void *, off_t, int);

/* Supporting subroutines */
static int	auixp_init(struct auixp_softc *);
static void	auixp_autodetect_codecs(struct auixp_softc *);
static void	auixp_post_config(device_t);

static void	auixp_reset_aclink(struct auixp_softc *);
static int	auixp_attach_codec(void *, struct ac97_codec_if *);
static int	auixp_read_codec(void *, uint8_t, uint16_t *);
static int	auixp_write_codec(void *, uint8_t, uint16_t);
static int	auixp_wait_for_codecs(struct auixp_softc *, const char *);
static int	auixp_reset_codec(void *);
static enum ac97_host_flags	auixp_flags_codec(void *);

static void	auixp_enable_dma(struct auixp_softc *, struct auixp_dma *);
static void	auixp_disable_dma(struct auixp_softc *, struct auixp_dma *);
static void	auixp_enable_interrupts(struct auixp_softc *);
static void	auixp_disable_interrupts(struct auixp_softc *);


/* statics */
static void	auixp_link_daisychain(struct auixp_softc *,
				      struct auixp_dma *, struct auixp_dma *,
				      int, int);
static int	auixp_allocate_dma_chain(struct auixp_softc *,
					 struct auixp_dma **);
static void	auixp_program_dma_chain(struct auixp_softc *,
					struct auixp_dma *);
static void	auixp_dma_update(struct auixp_softc *, struct auixp_dma *);
static void	auixp_update_busbusy(struct auixp_softc *);
static void	auixp_get_locks(void *, kmutex_t **, kmutex_t **);

static bool	auixp_resume(device_t, const pmf_qual_t *);


#ifdef DEBUG_AUIXP
static struct auixp_softc *static_sc;
static void auixp_dumpreg(void);
#	define DPRINTF(x) printf x;
#else
#	define DPRINTF(x)
#endif


static const struct audio_hw_if auixp_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* drain */
	auixp_query_encoding,
	auixp_set_params,
	auixp_round_blocksize,
	auixp_commit_settings,
	NULL,			/* init_output  */
	NULL,			/* init_input   */
	NULL,			/* start_output */
	NULL,			/* start_input  */
	auixp_halt_output,
	auixp_halt_input,
	NULL,			/* speaker_ctl */
	auixp_getdev,
	NULL,			/* getfd */
	auixp_set_port,
	auixp_get_port,
	auixp_query_devinfo,
	auixp_malloc,
	auixp_free,
	auixp_round_buffersize,
	auixp_mappage,
	auixp_get_props,
	auixp_trigger_output,
	auixp_trigger_input,
	NULL,			/* dev_ioctl */
	auixp_get_locks,
};


CFATTACH_DECL_NEW(auixp, sizeof(struct auixp_softc), auixp_match, auixp_attach,
    auixp_detach, NULL);


/*
 * audio(9) functions
 */

static int
auixp_query_encoding(void *hdl, struct audio_encoding *ae)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	return auconv_query_encoding(sc->sc_encodings, ae);
}


static int
auixp_set_rate(struct auixp_codec *co, int mode, u_int srate)
{
	int ret;
	u_int ratetmp;

	ratetmp = srate;
	if (mode == AUMODE_RECORD) {
		ret = co->codec_if->vtbl->set_rate(co->codec_if,
			AC97_REG_PCM_LR_ADC_RATE, &ratetmp);
		return ret;
	}

	/* play mode */
	ret = co->codec_if->vtbl->set_rate(co->codec_if,
		AC97_REG_PCM_FRONT_DAC_RATE, &ratetmp);
	if (ret)
		return ret;

	ratetmp = srate;
	ret = co->codec_if->vtbl->set_rate(co->codec_if,
		AC97_REG_PCM_SURR_DAC_RATE, &ratetmp);
	if (ret)
		return ret;

	ratetmp = srate;
	ret = co->codec_if->vtbl->set_rate(co->codec_if,
		AC97_REG_PCM_LFE_DAC_RATE, &ratetmp);
	return ret;
}


/* commit setting and program ATI IXP chip */
static int
auixp_commit_settings(void *hdl)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	bus_space_tag_t    iot;
	bus_space_handle_t ioh;
	struct audio_params *params;
	uint32_t value;

	/* XXX would it be better to stop interrupts first? XXX */
	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	/* process input settings */
	params = &sc->sc_play_params;

	/* set input interleaving (precision) */
	value  =  bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value &= ~ATI_REG_CMD_INTERLEAVE_IN;
	if (params->precision <= 16)
		value |= ATI_REG_CMD_INTERLEAVE_IN;
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

	/* process output settings */
	params = &sc->sc_play_params;

	value  =  bus_space_read_4(iot, ioh, ATI_REG_OUT_DMA_SLOT);
	value &= ~ATI_REG_OUT_DMA_SLOT_MASK;

	/* TODO SPDIF case for 8 channels */
	switch (params->channels) {
	case 6:
		value |= ATI_REG_OUT_DMA_SLOT_BIT(7) |
			 ATI_REG_OUT_DMA_SLOT_BIT(8);
		/* fallthru */
	case 4:
		value |= ATI_REG_OUT_DMA_SLOT_BIT(6) |
			 ATI_REG_OUT_DMA_SLOT_BIT(9);
		/* fallthru */
	default:
		value |= ATI_REG_OUT_DMA_SLOT_BIT(3) |
			 ATI_REG_OUT_DMA_SLOT_BIT(4);
		break;
	}
	/* set output threshold */
	value |= 0x04 << ATI_REG_OUT_DMA_THRESHOLD_SHIFT;
	bus_space_write_4(iot, ioh, ATI_REG_OUT_DMA_SLOT, value);

	/* set output interleaving (precision) */
	value  =  bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value &= ~ATI_REG_CMD_INTERLEAVE_OUT;
	if (params->precision <= 16)
		value |= ATI_REG_CMD_INTERLEAVE_OUT;
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

	/* enable 6 channel reordering */
	value  =  bus_space_read_4(iot, ioh, ATI_REG_6CH_REORDER);
	value &= ~ATI_REG_6CH_REORDER_EN;
	if (params->channels == 6)
		value |= ATI_REG_6CH_REORDER_EN;
	bus_space_write_4(iot, ioh, ATI_REG_6CH_REORDER, value);

	if (sc->has_spdif) {
		/* set SPDIF (if present) */
		value  =  bus_space_read_4(iot, ioh, ATI_REG_CMD);
		value &= ~ATI_REG_CMD_SPDF_CONFIG_MASK;
		value |=  ATI_REG_CMD_SPDF_CONFIG_34; /* NetBSD AC'97 default */

		/* XXX this prolly is not nessisary unless splitted XXX */
		value &= ~ATI_REG_CMD_INTERLEAVE_SPDF;
		if (params->precision <= 16)
			value |= ATI_REG_CMD_INTERLEAVE_SPDF;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);
	}

	return 0;
}


/* set audio properties in desired setting */
static int
auixp_set_params(void *hdl, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	audio_params_t *params;
	stream_filter_list_t *fil;
	int mode, index;

	/*
	 * In current NetBSD AC'97 implementation, SPDF is linked to channel 3
	 * and 4 i.e. stereo output.
	 */

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = (mode == AUMODE_RECORD) ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		params = (mode == AUMODE_PLAY) ? play :  rec;
		fil    = (mode == AUMODE_PLAY) ? pfil : rfil;
		if (params == NULL)
			continue;

		/* AD1888 settings ... don't know the IXP limits */
		if (params->sample_rate < AUIXP_MINRATE)
			return EINVAL;
		if (params->sample_rate > AUIXP_MAXRATE)
			return EINVAL;

		index = auconv_set_converter(sc->sc_formats, AUIXP_NFORMATS,
					     mode, params, TRUE, fil);

		/* nothing found? */
		if (index < 0)
			return EINVAL;

		/* not sure yet as to why i have to change params here */
		if (fil->req_size > 0)
			params = &fil->filters[0].param;

		/* if variable speed and we can't set the desired rate, fail */
		if ((sc->sc_formats[index].frequency_type != 1) &&
		    auixp_set_rate(co, mode, params->sample_rate))
			return EINVAL;

		/* preserve the settings */
		if (mode == AUMODE_PLAY)
			sc->sc_play_params = *params;
		if (mode == AUMODE_RECORD)
			sc->sc_rec_params  = *params;
	}

	return 0;
}


/* called to translate a requested blocksize to a hw-possible one */
static int
auixp_round_blocksize(void *hdl, int bs, int mode,
    const audio_params_t *param)
{
	uint32_t new_bs;

	new_bs = bs;
	/* Be conservative; align to 32 bytes and maximise it to 64 kb */
	/* 256 kb possible */
	if (new_bs > 0x10000)
		bs = 0x10000;			/* 64 kb max */
	new_bs = (bs & ~0x20);			/* 32 bytes align */

	return new_bs;
}


/*
 * allocate dma capable memory and record its information for later retrieval
 * when we program the dma chain itself. The trigger routines passes on the
 * kernel virtual address we return here as a reference to the mapping.
 */
static void *
auixp_malloc(void *hdl, int direction, size_t size)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma *dma;
	int error;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	/* get us a auixp_dma structure */
	dma = kmem_alloc(sizeof(*dma), KM_SLEEP);
	if (!dma)
		return NULL;

	/* get us a dma buffer itself */
	error = auixp_allocmem(sc, size, 16, dma);
	if (error) {
		kmem_free(dma, sizeof(*dma));
		aprint_error_dev(sc->sc_dev, "auixp_malloc: not enough memory\n");

		return NULL;
	}
	SLIST_INSERT_HEAD(&sc->sc_dma_list, dma, dma_chain);

	DPRINTF(("auixp_malloc: returning kern %p,   hw 0x%08x for %d bytes "
	    "in %d segs\n", KERNADDR(dma), (uint32_t) DMAADDR(dma), dma->size,
	    dma->nsegs)
	);

	return KERNADDR(dma);
}


/*
 * free and release dma capable memory we allocated before and remove its
 * recording
 */
static void
auixp_free(void *hdl, void *addr, size_t size)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma *dma;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	SLIST_FOREACH(dma, &sc->sc_dma_list, dma_chain) {
		if (KERNADDR(dma) == addr) {
			SLIST_REMOVE(&sc->sc_dma_list, dma, auixp_dma,
			    dma_chain);
			auixp_freemem(sc, dma);
			kmem_free(dma, sizeof(*dma));
			return;
		}
	}
}


static int
auixp_getdev(void *hdl, struct audio_device *ret)
{

	*ret = auixp_device;
	return 0;
}


/* pass request to AC'97 codec code */
static int
auixp_set_port(void *hdl, mixer_ctrl_t *mc)
{
	struct auixp_codec *co;

	co = (struct auixp_codec *) hdl;
	return co->codec_if->vtbl->mixer_set_port(co->codec_if, mc);
}


/* pass request to AC'97 codec code */
static int
auixp_get_port(void *hdl, mixer_ctrl_t *mc)
{
	struct auixp_codec *co;

	co = (struct auixp_codec *) hdl;
	return co->codec_if->vtbl->mixer_get_port(co->codec_if, mc);
}

/* pass request to AC'97 codec code */
static int
auixp_query_devinfo(void *hdl, mixer_devinfo_t *di)
{
	struct auixp_codec *co;

	co = (struct auixp_codec *) hdl;
	return co->codec_if->vtbl->query_devinfo(co->codec_if, di);
}


static size_t
auixp_round_buffersize(void *hdl, int direction,
    size_t bufsize)
{

	/* XXX force maximum? i.e. 256 kb? */
	return bufsize;
}


static int
auixp_get_props(void *hdl)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}


/*
 * A dma descriptor has dma->nsegs segments defined in dma->segs set up when
 * we claimed the memory.
 *
 * Due to our demand for one contiguous DMA area, we only have one segment. A
 * c_dma structure is about 3 kb for the 256 entries we maximally program
 * -arbitrary limit AFAIK- so all is most likely to be in one segment/page
 * anyway.
 *
 * XXX ought to implement fragmented dma area XXX
 *
 * Note that _v variables depict kernel virtual addresses, _p variables depict
 * physical addresses.
 */
static void
auixp_link_daisychain(struct auixp_softc *sc,
		struct auixp_dma *c_dma, struct auixp_dma *s_dma,
		int blksize, int blocks)
{
	atiixp_dma_desc_t *caddr_v, *next_caddr_v;
	uint32_t caddr_p, next_caddr_p, saddr_p;
	int i;

	/* just make sure we are not changing when its running */
	auixp_disable_dma(sc, c_dma);

	/* setup dma chain start addresses */
	caddr_v = KERNADDR(c_dma);
	caddr_p = DMAADDR(c_dma);
	saddr_p = DMAADDR(s_dma);

	/* program the requested number of blocks */
	for (i = 0; i < blocks; i++) {
		/* clear the block just in case */
		memset(caddr_v, 0, sizeof(atiixp_dma_desc_t));

		/* round robin the chain dma addresses for its successor */
		next_caddr_v = caddr_v + 1;
		next_caddr_p = caddr_p + sizeof(atiixp_dma_desc_t);

		if (i == blocks-1) {
			next_caddr_v = KERNADDR(c_dma);
			next_caddr_p = DMAADDR(c_dma);
		}

		/* fill in the hardware dma chain descriptor in little-endian */
		caddr_v->addr   = htole32(saddr_p);
		caddr_v->status = htole16(0);
		caddr_v->size   = htole16((blksize >> 2)); /* in dwords (!!!) */
		caddr_v->next   = htole32(next_caddr_p);

		/* advance slot */
		saddr_p += blksize;	/* XXX assuming contiguous XXX */
		caddr_v  = next_caddr_v;
		caddr_p  = next_caddr_p;
	}
}


static int
auixp_allocate_dma_chain(struct auixp_softc *sc, struct auixp_dma **dmap)
{
	struct auixp_dma *dma;
	int error;

	/* allocate keeper of dma area */
	*dmap = NULL;
	dma = kmem_zalloc(sizeof(struct auixp_dma), KM_SLEEP);
	if (!dma)
		return ENOMEM;

	/* allocate for daisychain of IXP hardware-dma descriptors */
	error = auixp_allocmem(sc, DMA_DESC_CHAIN * sizeof(atiixp_dma_desc_t),
	    16, dma);
	if (error) {
		aprint_error_dev(sc->sc_dev, "can't malloc dma descriptor chain\n");
		kmem_free(dma, sizeof(*dma));
		return ENOMEM;
	}

	/* return info and initialise structure */
	dma->intr    = NULL;
	dma->intrarg = NULL;

	*dmap = dma;
	return 0;
}


/* program dma chain in its link address descriptor */
static void
auixp_program_dma_chain(struct auixp_softc *sc, struct auixp_dma *dma)
{
	bus_space_tag_t    iot;
	bus_space_handle_t ioh;
	uint32_t value;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* get hardware start address of DMA chain and set valid-flag in it */
	/* XXX always at start? XXX */
	value = DMAADDR(dma);
	value = value | ATI_REG_LINKPTR_EN;

	/* reset linkpointer */
	bus_space_write_4(iot, ioh, dma->linkptr, 0);

	/* reset this DMA engine */
	auixp_disable_dma(sc, dma);
	auixp_enable_dma(sc, dma);

	/* program new DMA linkpointer */
	bus_space_write_4(iot, ioh, dma->linkptr, value);
}


/* called from interrupt code to signal end of one dma-slot */
static void
auixp_dma_update(struct auixp_softc *sc, struct auixp_dma *dma)
{

	/* be very paranoid */
	if (!dma)
		panic("%s: update: dma = NULL", device_xname(sc->sc_dev));
	if (!dma->intr)
		panic("%s: update: dma->intr = NULL", device_xname(sc->sc_dev));

	/* request more input from upper layer */
	(*dma->intr)(dma->intrarg);
}


/*
 * The magic `busbusy' bit that needs to be set when dma is active; allowing
 * busmastering?
 */
static void
auixp_update_busbusy(struct auixp_softc *sc)
{
	bus_space_tag_t    iot;
	bus_space_handle_t ioh;
	uint32_t value;
	int running;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* set bus-busy flag when either recording or playing is performed */
	value  = bus_space_read_4(iot, ioh, ATI_REG_IER);
	value &= ~ATI_REG_IER_SET_BUS_BUSY;

	running = ((sc->sc_output_dma->running) || (sc->sc_input_dma->running));
	if (running)
		value |= ATI_REG_IER_SET_BUS_BUSY;

	bus_space_write_4(iot, ioh, ATI_REG_IER, value);

}


/*
 * Called from upper audio layer to request playing audio, only called once;
 * audio is refilled by calling the intr() function when space is available
 * again.
 */
/* XXX allmost literaly a copy of trigger-input; could be factorised XXX */
static int
auixp_trigger_output(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma   *chain_dma;
	struct auixp_dma   *sound_dma;
	uint32_t blocks;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	chain_dma = sc->sc_output_dma;
	/* add functions to call back */
	chain_dma->intr    = intr;
	chain_dma->intrarg = intrarg;

	/*
	 * Program output DMA chain with blocks from [start...end] with
	 * blksize fragments.
	 *
	 * NOTE, we can assume its in one block since we asked for it to be in
	 * one contiguous blob; XXX change this? XXX
	 */
	blocks = (size_t) (((char *) end) - ((char *) start)) / blksize;

	/* lookup `start' address in our list of DMA area's */
	SLIST_FOREACH(sound_dma, &sc->sc_dma_list, dma_chain) {
		if (KERNADDR(sound_dma) == start)
			break;
	}

	/* not ours ? then bail out */
	if (!sound_dma) {
		printf("%s: auixp_trigger_output: bad sound addr %p\n",
		    device_xname(sc->sc_dev), start);
		return EINVAL;
	}

	/* link round-robin daisychain and program hardware */
	auixp_link_daisychain(sc, chain_dma, sound_dma, blksize, blocks);
	auixp_program_dma_chain(sc, chain_dma);

	/* mark we are now able to run now */
	chain_dma->running = 1;

	/* update bus-flags; XXX programs more flags XXX */
	auixp_update_busbusy(sc);

	/* callbacks happen in interrupt routine */
	return 0;
}


/* halt output of audio, just disable its dma and update bus state */
static int
auixp_halt_output(void *hdl)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma   *dma;

	co  = (struct auixp_codec *) hdl;
	sc  = co->sc;
	dma = sc->sc_output_dma;
	auixp_disable_dma(sc, dma);

	dma->running = 0;
	auixp_update_busbusy(sc);

	return 0;
}


/* XXX allmost literaly a copy of trigger-output; could be factorised XXX */
static int
auixp_trigger_input(void *hdl, void *start, void *end, int blksize,
    void (*intr)(void *), void *intrarg, const audio_params_t *param)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma   *chain_dma;
	struct auixp_dma   *sound_dma;
	uint32_t blocks;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	chain_dma = sc->sc_input_dma;
	/* add functions to call back */
	chain_dma->intr    = intr;
	chain_dma->intrarg = intrarg;

	/*
	 * Program output DMA chain with blocks from [start...end] with
	 * blksize fragments.
	 *
	 * NOTE, we can assume its in one block since we asked for it to be in
	 * one contiguous blob; XXX change this? XXX
	 */
	blocks = (size_t) (((char *) end) - ((char *) start)) / blksize;

	/* lookup `start' address in our list of DMA area's */
	SLIST_FOREACH(sound_dma, &sc->sc_dma_list, dma_chain) {
		if (KERNADDR(sound_dma) == start)
			break;
	}

	/* not ours ? then bail out */
	if (!sound_dma) {
		printf("%s: auixp_trigger_input: bad sound addr %p\n",
		    device_xname(sc->sc_dev), start);
		return EINVAL;
	}

	/* link round-robin daisychain and program hardware */
	auixp_link_daisychain(sc, chain_dma, sound_dma, blksize, blocks);
	auixp_program_dma_chain(sc, chain_dma);

	/* mark we are now able to run now */
	chain_dma->running = 1;

	/* update bus-flags; XXX programs more flags XXX */
	auixp_update_busbusy(sc);

	/* callbacks happen in interrupt routine */
	return 0;
}


/* halt sampling audio, just disable its dma and update bus state */
static int
auixp_halt_input(void *hdl)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma   *dma;

	co = (struct auixp_codec *) hdl;
	sc = co->sc;
	dma = sc->sc_input_dma;
	auixp_disable_dma(sc, dma);

	dma->running = 0;
	auixp_update_busbusy(sc);

	return 0;
}


/*
 * IXP audio interrupt handler
 *
 * note that we return the number of bits handled; the return value is not
 * documentated but i saw it implemented in other drivers. Prolly returning a
 * value > 0 means "i've dealt with it"
 *
 */
static int
auixp_intr(void *softc)
{
	struct auixp_softc *sc;
	bus_space_tag_t    iot;
	bus_space_handle_t ioh;
	uint32_t status, enable, detected_codecs;
	int ret;

	sc = softc;
	mutex_spin_enter(&sc->sc_intr_lock);

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	ret = 0;
	/* get status from the interrupt status register */
	status = bus_space_read_4(iot, ioh, ATI_REG_ISR);

	if (status == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	DPRINTF(("%s: (status = %x)\n", device_xname(sc->sc_dev), status));

	/* check DMA UPDATE flags for input & output */
	if (status & ATI_REG_ISR_IN_STATUS) {
		ret++; DPRINTF(("IN_STATUS\n"));
		auixp_dma_update(sc, sc->sc_input_dma);
	}
	if (status & ATI_REG_ISR_OUT_STATUS) {
		ret++; DPRINTF(("OUT_STATUS\n"));
		auixp_dma_update(sc, sc->sc_output_dma);
	}

	/* XXX XRUN flags not used/needed yet; should i implement it? XXX */
	/* acknowledge the interrupts nevertheless */
	if (status & ATI_REG_ISR_IN_XRUN) {
		ret++; DPRINTF(("IN_XRUN\n"));
		/* auixp_dma_xrun(sc, sc->sc_input_dma);  */
	}
	if (status & ATI_REG_ISR_OUT_XRUN) {
		ret++; DPRINTF(("OUT_XRUN\n"));
		/* auixp_dma_xrun(sc, sc->sc_output_dma); */
	}

	/* check if we are looking for codec detection */
	if (status & CODEC_CHECK_BITS) {
		ret++;
		/* mark missing codecs as not ready */
		detected_codecs = status & CODEC_CHECK_BITS;
		sc->sc_codec_not_ready_bits |= detected_codecs;

		/* disable detected interrupt sources */
		enable  = bus_space_read_4(iot, ioh, ATI_REG_IER);
		enable &= ~detected_codecs;
		bus_space_write_4(iot, ioh, ATI_REG_IER, enable);
	}

	/* acknowledge interrupt sources */
	bus_space_write_4(iot, ioh, ATI_REG_ISR, status);

	mutex_spin_exit(&sc->sc_intr_lock);
	return ret;
}


/* allocate memory for dma purposes; on failure of any of the steps, roll back */
static int
auixp_allocmem(struct auixp_softc *sc, size_t size,
	       size_t align, struct auixp_dma *dma)
{
	int error;

	/* remember size */
	dma->size = size;

	/* allocate DMA safe memory but in just one segment for now :( */
	error = bus_dmamem_alloc(sc->sc_dmat, dma->size, align, 0,
	    dma->segs, sizeof(dma->segs) / sizeof(dma->segs[0]), &dma->nsegs,
	    BUS_DMA_WAITOK);
	if (error)
		return error;

	/*
	 * map allocated memory into kernel virtual address space and keep it
	 * coherent with the CPU.
	 */
	error = bus_dmamem_map(sc->sc_dmat, dma->segs, dma->nsegs, dma->size,
				&dma->addr, BUS_DMA_WAITOK | BUS_DMA_COHERENT);
	if (error)
		goto free;

	/* allocate associated dma handle and initialize it. */
	error = bus_dmamap_create(sc->sc_dmat, dma->size, 1, dma->size, 0,
				  BUS_DMA_WAITOK, &dma->map);
	if (error)
		goto unmap;

	/*
	 * load the dma handle with mappings for a dma transfer; all pages
	 * need to be wired.
	 */
	error = bus_dmamap_load(sc->sc_dmat, dma->map, dma->addr, dma->size, NULL,
				BUS_DMA_WAITOK);
	if (error)
		goto destroy;

	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmat, dma->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, dma->addr, dma->size);
free:
	bus_dmamem_free(sc->sc_dmat, dma->segs, dma->nsegs);

	return error;
}


/* undo dma mapping and release memory allocated */
static int
auixp_freemem(struct auixp_softc *sc, struct auixp_dma *p)
{

	bus_dmamap_unload(sc->sc_dmat, p->map);
	bus_dmamap_destroy(sc->sc_dmat, p->map);
	bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmat, p->segs, p->nsegs);

	return 0;
}


/* memory map dma memory */
static paddr_t
auixp_mappage(void *hdl, void *mem, off_t off, int prot)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	struct auixp_dma *p;

	co = (struct auixp_codec *) hdl;
	sc  = co->sc;
	/* for sanity */
	if (off < 0)
		return -1;

	/* look up allocated DMA area */
	SLIST_FOREACH(p, &sc->sc_dma_list, dma_chain) {
		if (KERNADDR(p) == mem)
			break;
	}

	/* have we found it ? */
	if (!p)
		return -1;

	/* return mmap'd region */
	return bus_dmamem_mmap(sc->sc_dmat, p->segs, p->nsegs,
			       off, prot, BUS_DMA_WAITOK);
}


/*
 * Attachment section
 */

/* Is it my hardware? */
static int
auixp_match(device_t dev, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch(PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ATI:
		switch(PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ATI_IXP_AUDIO_200:
		case PCI_PRODUCT_ATI_IXP_AUDIO_300:
		case PCI_PRODUCT_ATI_IXP_AUDIO_400:
			return 1;
		}
	}

	return 0;
}


/* it is... now hook up and set up the resources we need */
static void
auixp_attach(device_t parent, device_t self, void *aux)
{
	struct auixp_softc *sc;
	struct pci_attach_args *pa;
	pcitag_t tag;
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	const struct auixp_card_type *card;
	const char *intrstr;
	uint32_t data;
	int error;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	tag = pa->pa_tag;
	pc = pa->pa_pc;
#ifdef DEBUG_AUIXP
	static_sc = sc;
#endif

	/* print information confirming attachment */
	pci_aprint_devinfo(pa, "Audio controller");

	/* set up details from our set of known `cards'/chips */
	for (card = auixp_card_types; card->pci_vendor_id; card++)
		if (PCI_VENDOR(pa->pa_id) == card->pci_vendor_id &&
		    PCI_PRODUCT(pa->pa_id) == card->pci_product_id) {
			sc->type = card->type;
			break;
		}

	/* device only has 32 bit non prefetchable memory		*/
	/* set MEM space access and enable the card's busmastering	*/
	data = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	data |= (PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, data);

	/* map memory; its not sized -> what is the size? max PCI slot size? */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_iot, &sc->sc_ioh, &sc->sc_iob, &sc->sc_ios)) {
		aprint_error_dev(sc->sc_dev, "can't map memory space\n");
		return;
	}

	/* Initialize softc */
	sc->sc_tag = tag;
	sc->sc_pct = pc;
	sc->sc_dmat = pa->pa_dmat;
	SLIST_INIT(&sc->sc_dma_list);

	/* get us the auixp_dma structures */
	auixp_allocate_dma_chain(sc, &sc->sc_output_dma);
	auixp_allocate_dma_chain(sc, &sc->sc_input_dma);

	/* when that fails we are dead in the water */
	if (!sc->sc_output_dma || !sc->sc_input_dma)
		return;

#if 0
	/* could preliminary program DMA chain */
	auixp_program_dma_chain(sc, sc->sc_output_dma);
	auixp_program_dma_chain(sc, sc->sc_input_dma);
#endif

	/* map interrupt on the pci bus */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "can't map interrupt\n");
		return;
	}

	/* where are we connected at ? */
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* establish interrupt routine hookup at IPL_AUDIO level */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, auixp_intr, self);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    pci_activate_null)) && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n",
		    error);
		return;
	}

	/* init chip */
	if (auixp_init(sc) == -1) {
		aprint_error_dev(sc->sc_dev, "auixp_attach: unable to initialize the card\n");
		return;
	}

	if (!pmf_device_register(self, NULL, auixp_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * delay further configuration of codecs and audio after interrupts
	 * are enabled.
	 */
	config_interrupts(self, auixp_post_config);
}


/* called from autoconfigure system when interrupts are enabled */
static void
auixp_post_config(device_t self)
{
	struct auixp_softc *sc;
	struct auixp_codec *codec;
	int codec_nr;
	int res, i;

	sc = device_private(self);
	/* detect the AC97 codecs */
	auixp_autodetect_codecs(sc);

	/* setup audio translation formats : following codec0 (!) */
	codec = &sc->sc_codec[0];
	if (!codec->present) {
		/* nothing??? then invalidate all formats */
		for (i = 0; i < AUIXP_NFORMATS; i++) {
			AUFMT_INVALIDATE(&sc->sc_formats[i]);
		}
		return;
	}

	/* copy formats and invalidate entries not suitable for codec0 */
	memcpy(sc->sc_formats, auixp_formats, sizeof(auixp_formats));
	mutex_enter(&sc->sc_lock);
	sc->has_4ch   = AC97_IS_4CH(codec->codec_if);
	sc->has_6ch   = AC97_IS_6CH(codec->codec_if);
	sc->is_fixed  = AC97_IS_FIXED_RATE(codec->codec_if);
	sc->has_spdif = AC97_HAS_SPDIF(codec->codec_if);
	mutex_exit(&sc->sc_lock);

	for (i = 0; i < AUIXP_NFORMATS; i++) {
		if (sc->is_fixed) {
			sc->sc_formats[i].frequency_type = 1;
			sc->sc_formats[i].frequency[0]   = 48000;
		}
		switch (sc->sc_formats[i].channels) {
		case 4 :
			if (sc->has_4ch)
				break;
			AUFMT_INVALIDATE(&sc->sc_formats[i]);
			break;
		case 6 :
			if (sc->has_6ch)
				break;
			AUFMT_INVALIDATE(&sc->sc_formats[i]);
			break;
		default :
			break;
		}
	}

	/*
	 * Create all encodings (and/or -translations) based on the formats
	 * supported. */
	res = auconv_create_encodings(sc->sc_formats, AUIXP_NFORMATS,
	    &sc->sc_encodings);
	if (res) {
		printf("%s: auconv_create_encodings failed; "
		    "no attachments\n", device_xname(sc->sc_dev));
		return;
	}

	if (sc->has_spdif) {
		aprint_normal_dev(sc->sc_dev, "codec spdif support detected but disabled "
		    "for now\n");
		sc->has_spdif = 0;
	}

	/* fill in the missing details about the dma channels. */
	/* for output */
	sc->sc_output_dma->linkptr        = ATI_REG_OUT_DMA_LINKPTR;
	sc->sc_output_dma->dma_enable_bit = ATI_REG_CMD_OUT_DMA_EN |
					    ATI_REG_CMD_SEND_EN;
	/* have spdif? then this too! XXX not seeing LED yet! XXX */
	if (sc->has_spdif)
		sc->sc_output_dma->dma_enable_bit |= ATI_REG_CMD_SPDF_OUT_EN;

	/* and for input */
	sc->sc_input_dma->linkptr         = ATI_REG_IN_DMA_LINKPTR;
	sc->sc_input_dma->dma_enable_bit  = ATI_REG_CMD_IN_DMA_EN  |
					    ATI_REG_CMD_RECEIVE_EN;

	/* attach audio devices for all detected codecs */
	/* XXX wise? look at other multiple-codec able chipsets XXX */
	for (codec_nr = 0; codec_nr < ATI_IXP_CODECS; codec_nr++) {
		codec = &sc->sc_codec[codec_nr];
		if (codec->present)
			audio_attach_mi(&auixp_hw_if, codec, sc->sc_dev);
	}

	/* done! now enable all interrupts we can service */
	auixp_enable_interrupts(sc);
}

static void
auixp_enable_interrupts(struct auixp_softc *sc)
{
	bus_space_tag_t     iot;
	bus_space_handle_t  ioh;
	uint32_t value;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	mutex_spin_enter(&sc->sc_intr_lock);

	/* clear all pending */
	bus_space_write_4(iot, ioh, ATI_REG_ISR, 0xffffffff);

	/* enable all relevant interrupt sources we can handle */
	value = bus_space_read_4(iot, ioh, ATI_REG_IER);

	value |= ATI_REG_IER_IO_STATUS_EN;
#ifdef notyet
	value |= ATI_REG_IER_IN_XRUN_EN;
	value |= ATI_REG_IER_OUT_XRUN_EN;

	value |= ATI_REG_IER_SPDIF_XRUN_EN;
	value |= ATI_REG_IER_SPDF_STATUS_EN;
#endif

	bus_space_write_4(iot, ioh, ATI_REG_IER, value);

	mutex_spin_exit(&sc->sc_intr_lock);
}


static void
auixp_disable_interrupts(struct auixp_softc *sc)
{
	bus_space_tag_t     iot;
	bus_space_handle_t  ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	mutex_spin_enter(&sc->sc_intr_lock);

	/* disable all interrupt sources */
	bus_space_write_4(iot, ioh, ATI_REG_IER, 0);

	/* clear all pending */
	bus_space_write_4(iot, ioh, ATI_REG_ISR, 0xffffffff);

	mutex_spin_exit(&sc->sc_intr_lock);
}


/* dismantle what we've set up by undoing setup */
static int
auixp_detach(device_t self, int flags)
{
	struct auixp_softc *sc;

	sc = device_private(self);
	/* XXX shouldn't we just reset the chip? XXX */
	/*
	 * should we explicitly disable interrupt generation and acknowledge
	 * what's left on? better be safe than sorry.
	 */
	auixp_disable_interrupts(sc);

	/* tear down .... */
	config_detach(sc->sc_dev, flags);	/* XXX OK? XXX */
	pmf_device_deregister(self);

	if (sc->sc_ih != NULL)
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
	if (sc->sc_ios)
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	return 0;
}


/*
 * codec handling
 *
 * IXP audio support can have upto 3 codecs! are they chained ? or
 * alternative outlets with the same audio feed i.e. with different mixer
 * settings? XXX does NetBSD support more than one audio codec? XXX
 */


static int
auixp_attach_codec(void *aux, struct ac97_codec_if *codec_if)
{
	struct auixp_codec *ixp_codec;

	ixp_codec = aux;
	ixp_codec->codec_if = codec_if;
	ixp_codec->present  = 1;

	return 0;
}


static int
auixp_read_codec(void *aux, uint8_t reg, uint16_t *result)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	bus_space_tag_t     iot;
	bus_space_handle_t  ioh;
	uint32_t data;
	int timeout;

	co  = aux;
	sc  = co->sc;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (auixp_wait_for_codecs(sc, "read_codec"))
		return 0xffff;

	/* build up command for reading codec register */
	data = (reg << ATI_REG_PHYS_OUT_ADDR_SHIFT) |
		ATI_REG_PHYS_OUT_ADDR_EN |
		ATI_REG_PHYS_OUT_RW |
		co->codec_nr;

	bus_space_write_4(iot, ioh, ATI_REG_PHYS_OUT_ADDR, data);

	if (auixp_wait_for_codecs(sc, "read_codec"))
		return 0xffff;

	/* wait until codec info is clocked in */
	timeout = 500;		/* 500*2 usec -> 0.001 sec */
	do {
		data = bus_space_read_4(iot, ioh, ATI_REG_PHYS_IN_ADDR);
		if (data & ATI_REG_PHYS_IN_READ_FLAG) {
			DPRINTF(("read ac'97 codec reg 0x%x = 0x%08x\n",
				reg, data >> ATI_REG_PHYS_IN_DATA_SHIFT)
			);
			*result = data >> ATI_REG_PHYS_IN_DATA_SHIFT;
			return 0;
		}
		DELAY(2);
		timeout--;
	} while (timeout > 0);

	if (reg < 0x7c)
		printf("%s: codec read timeout! (reg %x)\n",
		    device_xname(sc->sc_dev), reg);

	return 0xffff;
}


static int
auixp_write_codec(void *aux, uint8_t reg, uint16_t data)
{
	struct auixp_codec *co;
	struct auixp_softc *sc;
	bus_space_tag_t     iot;
	bus_space_handle_t  ioh;
	uint32_t value;

	DPRINTF(("write ac'97 codec reg 0x%x = 0x%08x\n", reg, data));
	co  = aux;
	sc  = co->sc;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	if (auixp_wait_for_codecs(sc, "write_codec"))
		return -1;

	/* build up command for writing codec register */
	value = (((uint32_t) data) << ATI_REG_PHYS_OUT_DATA_SHIFT) |
		(((uint32_t)  reg) << ATI_REG_PHYS_OUT_ADDR_SHIFT) |
		ATI_REG_PHYS_OUT_ADDR_EN |
		co->codec_nr;

	bus_space_write_4(iot, ioh, ATI_REG_PHYS_OUT_ADDR, value);

	return 0;
}


static int
auixp_reset_codec(void *aux)
{

	/* nothing to be done? */
	return 0;
}


static enum ac97_host_flags
auixp_flags_codec(void *aux)
{
	struct auixp_codec *ixp_codec;

	ixp_codec = aux;
	return ixp_codec->codec_flags;
}


static int
auixp_wait_for_codecs(struct auixp_softc *sc, const char *func)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	uint32_t value;
	int timeout;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* wait until all codec transfers are done */
	timeout = 500;		/* 500*2 usec -> 0.001 sec */
	do {
		value = bus_space_read_4(iot, ioh, ATI_REG_PHYS_OUT_ADDR);
		if ((value & ATI_REG_PHYS_OUT_ADDR_EN) == 0)
			return 0;

		DELAY(2);
		timeout--;
	} while (timeout > 0);

	printf("%s: %s: timed out\n", func, device_xname(sc->sc_dev));
	return -1;
}



static void
auixp_autodetect_codecs(struct auixp_softc *sc)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	struct auixp_codec  *codec;
	int timeout, codec_nr;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* ATI IXP can have upto 3 codecs; mark all codecs as not existing */
	sc->sc_codec_not_ready_bits = 0;
	sc->sc_num_codecs = 0;

	/* enable all codecs to interrupt as well as the new frame interrupt */
	bus_space_write_4(iot, ioh, ATI_REG_IER, CODEC_CHECK_BITS);

	/* wait for the interrupts to happen */
	timeout = 100;		/* 100.000 usec -> 0.1 sec */

	while (timeout > 0) {
		DELAY(1000);
		if (sc->sc_codec_not_ready_bits)
			break;
		timeout--;
	}

	if (timeout == 0)
		printf("%s: WARNING: timeout during codec detection; "
			"codecs might be present but haven't interrupted\n",
			device_xname(sc->sc_dev));

	/* disable all interrupts for now */
	auixp_disable_interrupts(sc);

	/* Attach AC97 host interfaces */
	for (codec_nr = 0; codec_nr < ATI_IXP_CODECS; codec_nr++) {
		codec = &sc->sc_codec[codec_nr];
		memset(codec, 0, sizeof(struct auixp_codec));

		codec->sc       = sc;
		codec->codec_nr = codec_nr;
		codec->present  = 0;

		codec->host_if.arg    = codec;
		codec->host_if.attach = auixp_attach_codec;
		codec->host_if.read   = auixp_read_codec;
		codec->host_if.write  = auixp_write_codec;
		codec->host_if.reset  = auixp_reset_codec;
		codec->host_if.flags  = auixp_flags_codec;
	}

	if (!(sc->sc_codec_not_ready_bits & ATI_REG_ISR_CODEC0_NOT_READY)) {
		/* codec 0 present */
		DPRINTF(("auixp : YAY! codec 0 present!\n"));
		if (ac97_attach(&sc->sc_codec[0].host_if, sc->sc_dev,
		    &sc->sc_lock) == 0)
			sc->sc_num_codecs++;
	}

	if (!(sc->sc_codec_not_ready_bits & ATI_REG_ISR_CODEC1_NOT_READY)) {
		/* codec 1 present */
		DPRINTF(("auixp : YAY! codec 1 present!\n"));
		if (ac97_attach(&sc->sc_codec[1].host_if, sc->sc_dev,
		    &sc->sc_lock) == 0)
			sc->sc_num_codecs++;
	}

	if (!(sc->sc_codec_not_ready_bits & ATI_REG_ISR_CODEC2_NOT_READY)) {
		/* codec 2 present */
		DPRINTF(("auixp : YAY! codec 2 present!\n"));
		if (ac97_attach(&sc->sc_codec[2].host_if, sc->sc_dev,
		    &sc->sc_lock) == 0)
			sc->sc_num_codecs++;
	}

	if (sc->sc_num_codecs == 0) {
		printf("%s: no codecs detected or "
				"no codecs managed to initialise\n",
				device_xname(sc->sc_dev));
		return;
	}

}



/* initialisation routines */

static void
auixp_disable_dma(struct auixp_softc *sc, struct auixp_dma *dma)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	uint32_t value;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* lets not stress the DMA engine more than nessisary */
	value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	if (value & dma->dma_enable_bit) {
		value &= ~dma->dma_enable_bit;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);
	}
}


static void
auixp_enable_dma(struct auixp_softc *sc, struct auixp_dma *dma)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	uint32_t value;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* lets not stress the DMA engine more than nessisary */
	value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	if (!(value & dma->dma_enable_bit)) {
		value |= dma->dma_enable_bit;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);
	}
}


static void
auixp_reset_aclink(struct auixp_softc *sc)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	uint32_t value, timeout;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	/* if power is down, power it up */
	value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	if (value & ATI_REG_CMD_POWERDOWN) {
		printf("%s: powering up\n", device_xname(sc->sc_dev));

		/* explicitly enable power */
		value &= ~ATI_REG_CMD_POWERDOWN;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

		/* have to wait at least 10 usec for it to initialise */
		DELAY(20);
	};

	printf("%s: soft resetting aclink\n", device_xname(sc->sc_dev));

	/* perform a soft reset */
	value  = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value |= ATI_REG_CMD_AC_SOFT_RESET;
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

	/* need to read the CMD reg and wait aprox. 10 usec to init */
	value  = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	DELAY(20);

	/* clear soft reset flag again */
	value  = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value &= ~ATI_REG_CMD_AC_SOFT_RESET;
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

	/* check if the ac-link is working; reset device otherwise */
	timeout = 10;
	value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	while (!(value & ATI_REG_CMD_ACLINK_ACTIVE)) {
		printf("%s: not up; resetting aclink hardware\n",
			device_xname(sc->sc_dev));

		/* dip aclink reset but keep the acsync */
		value &= ~ATI_REG_CMD_AC_RESET;
		value |=  ATI_REG_CMD_AC_SYNC;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

		/* need to read CMD again and wait again (clocking in issue?) */
		value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
		DELAY(20);

		/* assert aclink reset again */
		value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
		value |=  ATI_REG_CMD_AC_RESET;
		bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

		/* check if its active now */
		value = bus_space_read_4(iot, ioh, ATI_REG_CMD);

		timeout--;
		if (timeout == 0) break;
	};

	if (timeout == 0) {
		printf("%s: giving up aclink reset\n", device_xname(sc->sc_dev));
	};
	if (timeout != 10) {
		printf("%s: aclink hardware reset successful\n",
			device_xname(sc->sc_dev));
	};

	/* assert reset and sync for safety */
	value  = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value |= ATI_REG_CMD_AC_SYNC | ATI_REG_CMD_AC_RESET;
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);
}


/* chip hard init */
static int
auixp_init(struct auixp_softc *sc)
{
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	uint32_t value;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	/* disable all interrupts and clear all sources */
	auixp_disable_interrupts(sc);

	/* clear all DMA enables (preserving rest of settings) */
	value = bus_space_read_4(iot, ioh, ATI_REG_CMD);
	value &= ~( ATI_REG_CMD_IN_DMA_EN  |
		    ATI_REG_CMD_OUT_DMA_EN |
		    ATI_REG_CMD_SPDF_OUT_EN );
	bus_space_write_4(iot, ioh, ATI_REG_CMD, value);

	/* Reset AC-link */
	auixp_reset_aclink(sc);

	/*
	 * codecs get auto-detected later
	 *
	 * note: we are NOT enabling interrupts yet, no codecs have been
	 * detected yet nor is anything else set up
	 */

	return 0;
}

static bool
auixp_resume(device_t dv, const pmf_qual_t *qual)
{
	struct auixp_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_lock);
	auixp_reset_codec(sc);
	delay(1000);
	(sc->sc_codec[0].codec_if->vtbl->restore_ports)(sc->sc_codec[0].codec_if);
	mutex_exit(&sc->sc_lock);

	return true;
}

#ifdef DEBUG_AUIXP

static void
auixp_dumpreg(void)
{
	struct auixp_softc  *sc;
	bus_space_tag_t      iot;
	bus_space_handle_t   ioh;
	int i;

	sc  = static_sc;
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	printf("%s register dump:\n", device_xname(sc->sc_dev));
	for (i = 0; i < 256; i+=4) {
		printf("\t0x%02x: 0x%08x\n", i, bus_space_read_4(iot, ioh, i));
	}
	printf("\n");
}
#endif

static void
auixp_get_locks(void *addr, kmutex_t **intr, kmutex_t **proc)
{
	struct auixp_codec *co = addr;
	struct auixp_softc *sc = co->sc;

	*intr = &sc->sc_intr_lock;
	*proc = &sc->sc_lock;
}

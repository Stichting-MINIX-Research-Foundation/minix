/*      $NetBSD: sv.c,v 1.50 2014/03/29 19:28:25 christos Exp $ */
/*      $OpenBSD: sv.c,v 1.2 1998/07/13 01:50:15 csapuntz Exp $ */

/*
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*
 * Copyright (c) 1998 Constantine Paul Sapuntzakis
 * All rights reserved
 *
 * Author: Constantine Paul Sapuntzakis (csapuntz@cvs.openbsd.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The author's name or those of the contributors may be used to
 *    endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS
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

/*
 * S3 SonicVibes driver
 *   Heavily based on the eap driver by Lennart Augustsson
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sv.c,v 1.50 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/i8237reg.h>
#include <dev/pci/svreg.h>
#include <dev/pci/svvar.h>

#include <sys/bus.h>

/* XXX
 * The SonicVibes DMA is broken and only works on 24-bit addresses.
 * As long as bus_dmamem_alloc_range() is missing we use the ISA
 * DMA tag on i386.
 */
#if defined(amd64) || defined(i386)
#include <dev/isa/isavar.h>
#endif

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (svdebug) printf x
#define DPRINTFN(n,x)	if (svdebug>(n)) printf x
int	svdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static int	sv_match(device_t, cfdata_t, void *);
static void	sv_attach(device_t, device_t, void *);
static int	sv_intr(void *);

struct sv_dma {
	bus_dmamap_t map;
	void *addr;
	bus_dma_segment_t segs[1];
	int nsegs;
	size_t size;
	struct sv_dma *next;
};
#define DMAADDR(p) ((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p) ((void *)((p)->addr))

CFATTACH_DECL_NEW(sv, sizeof(struct sv_softc),
    sv_match, sv_attach, NULL, NULL);

static struct audio_device sv_device = {
	"S3 SonicVibes",
	"",
	"sv"
};

#define ARRAY_SIZE(foo)  ((sizeof(foo)) / sizeof(foo[0]))

static int	sv_allocmem(struct sv_softc *, size_t, size_t, int,
			    struct sv_dma *);
static int	sv_freemem(struct sv_softc *, struct sv_dma *);

static void	sv_init_mixer(struct sv_softc *);

static int	sv_open(void *, int);
static int	sv_query_encoding(void *, struct audio_encoding *);
static int	sv_set_params(void *, int, int, audio_params_t *,
			      audio_params_t *, stream_filter_list_t *,
			      stream_filter_list_t *);
static int	sv_round_blocksize(void *, int, int, const audio_params_t *);
static int	sv_trigger_output(void *, void *, void *, int, void (*)(void *),
				  void *, const audio_params_t *);
static int	sv_trigger_input(void *, void *, void *, int, void (*)(void *),
				 void *, const audio_params_t *);
static int	sv_halt_output(void *);
static int	sv_halt_input(void *);
static int	sv_getdev(void *, struct audio_device *);
static int	sv_mixer_set_port(void *, mixer_ctrl_t *);
static int	sv_mixer_get_port(void *, mixer_ctrl_t *);
static int	sv_query_devinfo(void *, mixer_devinfo_t *);
static void *	sv_malloc(void *, int, size_t);
static void	sv_free(void *, void *, size_t);
static size_t	sv_round_buffersize(void *, int, size_t);
static paddr_t	sv_mappage(void *, void *, off_t, int);
static int	sv_get_props(void *);
static void	sv_get_locks(void *, kmutex_t **, kmutex_t **);

#ifdef AUDIO_DEBUG
void    sv_dumpregs(struct sv_softc *sc);
#endif

static const struct audio_hw_if sv_hw_if = {
	sv_open,
	NULL,			/* close */
	NULL,
	sv_query_encoding,
	sv_set_params,
	sv_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	sv_halt_output,
	sv_halt_input,
	NULL,
	sv_getdev,
	NULL,
	sv_mixer_set_port,
	sv_mixer_get_port,
	sv_query_devinfo,
	sv_malloc,
	sv_free,
	sv_round_buffersize,
	sv_mappage,
	sv_get_props,
	sv_trigger_output,
	sv_trigger_input,
	NULL,
	sv_get_locks,
};

#define SV_NFORMATS	4
static const struct audio_format sv_formats[SV_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {2000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {2000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {2000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {2000, 48000}},
};


static void
sv_write(struct sv_softc *sc, uint8_t reg, uint8_t val)
{

	DPRINTFN(8,("sv_write(0x%x, 0x%x)\n", reg, val));
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val);
}

static uint8_t
sv_read(struct sv_softc *sc, uint8_t reg)
{
	uint8_t val;

	val = bus_space_read_1(sc->sc_iot, sc->sc_ioh, reg);
	DPRINTFN(8,("sv_read(0x%x) = 0x%x\n", reg, val));
	return val;
}

static uint8_t
sv_read_indirect(struct sv_softc *sc, uint8_t reg)
{
	uint8_t val;

	sv_write(sc, SV_CODEC_IADDR, reg & SV_IADDR_MASK);
	val = sv_read(sc, SV_CODEC_IDATA);
	return val;
}

static void
sv_write_indirect(struct sv_softc *sc, uint8_t reg, uint8_t val)
{
	uint8_t iaddr;

	iaddr = reg & SV_IADDR_MASK;
	if (reg == SV_DMA_DATA_FORMAT)
		iaddr |= SV_IADDR_MCE;

	sv_write(sc, SV_CODEC_IADDR, iaddr);
	sv_write(sc, SV_CODEC_IDATA, val);
}

static int
sv_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_S3 &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_S3_SONICVIBES)
		return 1;

	return 0;
}

static pcireg_t pci_io_alloc_low, pci_io_alloc_high;

static int
pci_alloc_io(pci_chipset_tag_t pc, pcitag_t pt, int pcioffs,
    bus_space_tag_t iot, bus_size_t size, bus_size_t align,
    bus_size_t bound, int flags, bus_space_handle_t *ioh)
{
	bus_addr_t addr;
	int error;

	error = bus_space_alloc(iot, pci_io_alloc_low, pci_io_alloc_high,
				size, align, bound, flags, &addr, ioh);
	if (error)
		return error;

	pci_conf_write(pc, pt, pcioffs, addr);
	return 0;
}

/*
 * Allocate IO addresses when all other configuration is done.
 */
static void
sv_defer(device_t self)
{
	struct sv_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t pt;
	pcireg_t dmaio;

	sc = device_private(self);
	pc = sc->sc_pa.pa_pc;
	pt = sc->sc_pa.pa_tag;
	DPRINTF(("sv_defer: %p\n", sc));

	/* XXX
	 * Get a reasonable default for the I/O range.
	 * Assume the range around SB_PORTBASE is valid on this PCI bus.
	 */
	pci_io_alloc_low = pci_conf_read(pc, pt, SV_SB_PORTBASE_SLOT);
	pci_io_alloc_high = pci_io_alloc_low + 0x1000;

	if (pci_alloc_io(pc, pt, SV_DMAA_CONFIG_OFF,
			  sc->sc_iot, SV_DMAA_SIZE, SV_DMAA_ALIGN, 0,
			  0, &sc->sc_dmaa_ioh)) {
		printf("sv_attach: cannot allocate DMA A range\n");
		return;
	}
	dmaio = pci_conf_read(pc, pt, SV_DMAA_CONFIG_OFF);
	DPRINTF(("sv_attach: addr a dmaio=0x%lx\n", (u_long)dmaio));
	pci_conf_write(pc, pt, SV_DMAA_CONFIG_OFF,
		       dmaio | SV_DMA_CHANNEL_ENABLE | SV_DMAA_EXTENDED_ADDR);

	if (pci_alloc_io(pc, pt, SV_DMAC_CONFIG_OFF,
			  sc->sc_iot, SV_DMAC_SIZE, SV_DMAC_ALIGN, 0,
			  0, &sc->sc_dmac_ioh)) {
		printf("sv_attach: cannot allocate DMA C range\n");
		return;
	}
	dmaio = pci_conf_read(pc, pt, SV_DMAC_CONFIG_OFF);
	DPRINTF(("sv_attach: addr c dmaio=0x%lx\n", (u_long)dmaio));
	pci_conf_write(pc, pt, SV_DMAC_CONFIG_OFF,
		       dmaio | SV_DMA_CHANNEL_ENABLE);

	sc->sc_dmaset = 1;
}

static void
sv_attach(device_t parent, device_t self, void *aux)
{
	struct sv_softc *sc;
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	pcitag_t pt;
	pci_intr_handle_t ih;
	pcireg_t csr;
	char const *intrstr;
	uint8_t reg;
	struct audio_attach_args arg;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	pa = aux;
	pc = pa->pa_pc;
	pt = pa->pa_tag;
	printf ("\n");

	/* Map I/O registers */
	if (pci_mapreg_map(pa, SV_ENHANCED_PORTBASE_SLOT,
			   PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map enhanced i/o space\n");
		return;
	}
	if (pci_mapreg_map(pa, SV_FM_PORTBASE_SLOT,
			   PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc_opliot, &sc->sc_oplioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map FM i/o space\n");
		return;
	}
	if (pci_mapreg_map(pa, SV_MIDI_PORTBASE_SLOT,
			   PCI_MAPREG_TYPE_IO, 0,
			   &sc->sc_midiiot, &sc->sc_midiioh, NULL, NULL)) {
		aprint_error_dev(self, "can't map MIDI i/o space\n");
		return;
	}
	DPRINTF(("sv: IO ports: enhanced=0x%x, OPL=0x%x, MIDI=0x%x\n",
		 (int)sc->sc_ioh, (int)sc->sc_oplioh, (int)sc->sc_midiioh));

#if defined(alpha)
	/* XXX Force allocation through the SGMAP. */
	sc->sc_dmatag = alphabus_dma_get_tag(pa->pa_dmat, ALPHA_BUS_ISA);
#elif defined(amd64) || defined(i386)
/* XXX
 * The SonicVibes DMA is broken and only works on 24-bit addresses.
 * As long as bus_dmamem_alloc_range() is missing we use the ISA
 * DMA tag on i386.
 */
	sc->sc_dmatag = &isa_bus_dma_tag;
#else
	sc->sc_dmatag = pa->pa_dmat;
#endif

	pci_conf_write(pc, pt, SV_DMAA_CONFIG_OFF, SV_DMAA_EXTENDED_ADDR);
	pci_conf_write(pc, pt, SV_DMAC_CONFIG_OFF, 0);

	/* Enable the device. */
	csr = pci_conf_read(pc, pt, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pt, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	sv_write_indirect(sc, SV_ANALOG_POWER_DOWN_CONTROL, 0);
	sv_write_indirect(sc, SV_DIGITAL_POWER_DOWN_CONTROL, 0);

	/* initialize codec registers */
	reg = sv_read(sc, SV_CODEC_CONTROL);
	reg |= SV_CTL_RESET;
	sv_write(sc, SV_CODEC_CONTROL, reg);
	delay(50);

	reg = sv_read(sc, SV_CODEC_CONTROL);
	reg &= ~SV_CTL_RESET;
	reg |= SV_CTL_INTA | SV_CTL_ENHANCED;

	/* This write clears the reset */
	sv_write(sc, SV_CODEC_CONTROL, reg);
	delay(50);

	/* This write actually shoves the new values in */
	sv_write(sc, SV_CODEC_CONTROL, reg);

	DPRINTF(("sv_attach: control=0x%x\n", sv_read(sc, SV_CODEC_CONTROL)));

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, sv_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	printf("%s: interrupting at %s\n", device_xname(self), intrstr);
	printf("%s: rev %d", device_xname(self),
	       sv_read_indirect(sc, SV_REVISION_LEVEL));
	if (sv_read(sc, SV_CODEC_CONTROL) & SV_CTL_MD1)
		printf(", reverb SRAM present");
	if (!(sv_read_indirect(sc, SV_WAVETABLE_SOURCE_SELECT) & SV_WSS_WT0))
		printf(", wavetable ROM present");
	printf("\n");

	/* Enable DMA interrupts */
	reg = sv_read(sc, SV_CODEC_INTMASK);
	reg &= ~(SV_INTMASK_DMAA | SV_INTMASK_DMAC);
	reg |= SV_INTMASK_UD | SV_INTMASK_SINT | SV_INTMASK_MIDI;
	sv_write(sc, SV_CODEC_INTMASK, reg);
	sv_read(sc, SV_CODEC_STATUS);

	sv_init_mixer(sc);

	audio_attach_mi(&sv_hw_if, sc, self);

	arg.type = AUDIODEV_TYPE_OPL;
	arg.hwif = 0;
	arg.hdl = 0;
	(void)config_found(self, &arg, audioprint);

	sc->sc_pa = *pa;	/* for deferred setup */
	config_defer(self, sv_defer);
}

#ifdef AUDIO_DEBUG
void
sv_dumpregs(struct sv_softc *sc)
{
	int idx;

#if 0
	for (idx = 0; idx < 0x50; idx += 4)
		printf ("%02x = %x\n", idx,
			pci_conf_read(pa->pa_pc, pa->pa_tag, idx));
#endif

	for (idx = 0; idx < 6; idx++)
		printf ("REG %02x = %02x\n", idx, sv_read(sc, idx));

	for (idx = 0; idx < 0x32; idx++)
		printf ("IREG %02x = %02x\n", idx, sv_read_indirect(sc, idx));

	for (idx = 0; idx < 0x10; idx++)
		printf ("DMA %02x = %02x\n", idx,
			bus_space_read_1(sc->sc_iot, sc->sc_dmaa_ioh, idx));
}
#endif

static int
sv_intr(void *p)
{
	struct sv_softc *sc;
	uint8_t intr;

	sc = p;

	mutex_spin_enter(&sc->sc_intr_lock);

	intr = sv_read(sc, SV_CODEC_STATUS);
	DPRINTFN(5,("sv_intr: intr=0x%x\n", intr));

	if (intr & SV_INTSTATUS_DMAA) {
		if (sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);
	}

	if (intr & SV_INTSTATUS_DMAC) {
		if (sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);
	}

	mutex_spin_exit(&sc->sc_intr_lock);

	return (intr & (SV_INTSTATUS_DMAA | SV_INTSTATUS_DMAC)) != 0;
}

static int
sv_allocmem(struct sv_softc *sc, size_t size, size_t align,
    int direction, struct sv_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
	    p->segs, ARRAY_SIZE(p->segs), &p->nsegs, BUS_DMA_WAITOK);
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
	    BUS_DMA_WAITOK | (direction == AUMODE_RECORD) ? BUS_DMA_READ : BUS_DMA_WRITE);
	if (error)
		goto destroy;
	DPRINTF(("sv_allocmem: pa=%lx va=%lx pba=%lx\n",
	    (long)p->segs[0].ds_addr, (long)KERNADDR(p), (long)DMAADDR(p)));
	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return error;
}

static int
sv_freemem(struct sv_softc *sc, struct sv_dma *p)
{

	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return 0;
}

static int
sv_open(void *addr, int flags)
{
	struct sv_softc *sc;

	sc = addr;
	DPRINTF(("sv_open\n"));
	if (!sc->sc_dmaset)
		return ENXIO;

	return 0;
}

static int
sv_query_encoding(void *addr, struct audio_encoding *fp)
{

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return 0;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return 0;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	default:
		return EINVAL;
	}
}

static int
sv_set_params(void *addr, int setmode, int usemode, audio_params_t *play,
    audio_params_t *rec, stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct sv_softc *sc;
	audio_params_t *p;
	uint32_t val;

	sc = addr;
	p = NULL;
	/*
	 * This device only has one clock, so make the sample rates match.
	 */
	if (play->sample_rate != rec->sample_rate &&
	    usemode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (setmode == AUMODE_PLAY) {
			rec->sample_rate = play->sample_rate;
			setmode |= AUMODE_RECORD;
		} else if (setmode == AUMODE_RECORD) {
			play->sample_rate = rec->sample_rate;
			setmode |= AUMODE_PLAY;
		} else
			return EINVAL;
	}

	if (setmode & AUMODE_RECORD) {
		p = rec;
		if (auconv_set_converter(sv_formats, SV_NFORMATS,
					 AUMODE_RECORD, rec, FALSE, rfil) < 0)
			return EINVAL;
	}
	if (setmode & AUMODE_PLAY) {
		p = play;
		if (auconv_set_converter(sv_formats, SV_NFORMATS,
					 AUMODE_PLAY, play, FALSE, pfil) < 0)
			return EINVAL;
	}

	if (p == NULL)
		return 0;

	val = p->sample_rate * 65536 / 48000;
	/*
	 * If the sample rate is exactly 48 kHz, the fraction would overflow the
	 * register, so we have to bias it.  This causes a little clock drift.
	 * The drift is below normal crystal tolerance (.0001%), so although
	 * this seems a little silly, we can pretty much ignore it.
	 * (I tested the output speed with values of 1-20, just to be sure this
	 * register isn't *supposed* to have a bias.  It isn't.)
	 * - mycroft
	 */
	if (val > 65535)
		val = 65535;

	mutex_spin_enter(&sc->sc_intr_lock);
	sv_write_indirect(sc, SV_PCM_SAMPLE_RATE_0, val & 0xff);
	sv_write_indirect(sc, SV_PCM_SAMPLE_RATE_1, val >> 8);
	mutex_spin_exit(&sc->sc_intr_lock);

#define F_REF 24576000

#define ABS(x) (((x) < 0) ? (-x) : (x))

	if (setmode & AUMODE_RECORD) {
		/* The ADC reference frequency (f_out) is 512 * sample rate */

		/* f_out is dervied from the 24.576MHz crystal by three values:
		   M & N & R. The equation is as follows:

		   f_out = (m + 2) * f_ref / ((n + 2) * (2 ^ a))

		   with the constraint that:

		   80 MHz < (m + 2) / (n + 2) * f_ref <= 150MHz
		   and n, m >= 1
		*/

		int  goal_f_out;
		int  a, n, m, best_n, best_m, best_error;
		int  pll_sample;
		int  error;

		goal_f_out = 512 * rec->sample_rate;
		best_n = 0;
		best_m = 0;
		best_error = 10000000;
		for (a = 0; a < 8; a++) {
			if ((goal_f_out * (1 << a)) >= 80000000)
				break;
		}

		/* a != 8 because sample_rate >= 2000 */

		for (n = 33; n > 2; n--) {
			m = (goal_f_out * n * (1 << a)) / F_REF;
			if ((m > 257) || (m < 3))
				continue;

			pll_sample = (m * F_REF) / (n * (1 << a));
			pll_sample /= 512;

			/* Threshold might be good here */
			error = pll_sample - rec->sample_rate;
			error = ABS(error);

			if (error < best_error) {
				best_error = error;
				best_n = n;
				best_m = m;
				if (error == 0) break;
			}
		}

		best_n -= 2;
		best_m -= 2;

		mutex_spin_enter(&sc->sc_intr_lock);
		sv_write_indirect(sc, SV_ADC_PLL_M, best_m);
		sv_write_indirect(sc, SV_ADC_PLL_N,
				  best_n | (a << SV_PLL_R_SHIFT));
		mutex_spin_exit(&sc->sc_intr_lock);
	}

	return 0;
}

static int
sv_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	return blk & -32;	/* keep good alignment */
}

static int
sv_trigger_output(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct sv_softc *sc;
	struct sv_dma *p;
	uint8_t mode;
	int dma_count;

	DPRINTFN(1, ("sv_trigger_output: sc=%p start=%p end=%p blksize=%d "
	    "intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc = addr;
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	mode = sv_read_indirect(sc, SV_DMA_DATA_FORMAT);
	mode &= ~(SV_DMAA_FORMAT16 | SV_DMAA_STEREO);
	if (param->precision == 16)
		mode |= SV_DMAA_FORMAT16;
	if (param->channels == 2)
		mode |= SV_DMAA_STEREO;
	sv_write_indirect(sc, SV_DMA_DATA_FORMAT, mode);

	for (p = sc->sc_dmas; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (p == NULL) {
		printf("sv_trigger_output: bad addr %p\n", start);
		return EINVAL;
	}

	dma_count = ((char *)end - (char *)start) - 1;
	DPRINTF(("sv_trigger_output: DMA start loop input addr=%x cc=%d\n",
	    (int)DMAADDR(p), dma_count));

	bus_space_write_4(sc->sc_iot, sc->sc_dmaa_ioh, SV_DMA_ADDR0,
			  DMAADDR(p));
	bus_space_write_4(sc->sc_iot, sc->sc_dmaa_ioh, SV_DMA_COUNT0,
			  dma_count);
	bus_space_write_1(sc->sc_iot, sc->sc_dmaa_ioh, SV_DMA_MODE,
			  DMA37MD_READ | DMA37MD_LOOP);

	DPRINTF(("sv_trigger_output: current addr=%x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_dmaa_ioh, SV_DMA_ADDR0)));

	dma_count = blksize - 1;

	sv_write_indirect(sc, SV_DMAA_COUNT1, dma_count >> 8);
	sv_write_indirect(sc, SV_DMAA_COUNT0, dma_count & 0xFF);

	mode = sv_read_indirect(sc, SV_PLAY_RECORD_ENABLE);
	sv_write_indirect(sc, SV_PLAY_RECORD_ENABLE, mode | SV_PLAY_ENABLE);

	return 0;
}

static int
sv_trigger_input(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct sv_softc *sc;
	struct sv_dma *p;
	uint8_t mode;
	int dma_count;

	DPRINTFN(1, ("sv_trigger_input: sc=%p start=%p end=%p blksize=%d "
	    "intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc = addr;
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	mode = sv_read_indirect(sc, SV_DMA_DATA_FORMAT);
	mode &= ~(SV_DMAC_FORMAT16 | SV_DMAC_STEREO);
	if (param->precision == 16)
		mode |= SV_DMAC_FORMAT16;
	if (param->channels == 2)
		mode |= SV_DMAC_STEREO;
	sv_write_indirect(sc, SV_DMA_DATA_FORMAT, mode);

	for (p = sc->sc_dmas; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (!p) {
		printf("sv_trigger_input: bad addr %p\n", start);
		return EINVAL;
	}

	dma_count = (((char *)end - (char *)start) >> 1) - 1;
	DPRINTF(("sv_trigger_input: DMA start loop input addr=%x cc=%d\n",
	    (int)DMAADDR(p), dma_count));

	bus_space_write_4(sc->sc_iot, sc->sc_dmac_ioh, SV_DMA_ADDR0,
			  DMAADDR(p));
	bus_space_write_4(sc->sc_iot, sc->sc_dmac_ioh, SV_DMA_COUNT0,
			  dma_count);
	bus_space_write_1(sc->sc_iot, sc->sc_dmac_ioh, SV_DMA_MODE,
			  DMA37MD_WRITE | DMA37MD_LOOP);

	DPRINTF(("sv_trigger_input: current addr=%x\n",
	    bus_space_read_4(sc->sc_iot, sc->sc_dmac_ioh, SV_DMA_ADDR0)));

	dma_count = (blksize >> 1) - 1;

	sv_write_indirect(sc, SV_DMAC_COUNT1, dma_count >> 8);
	sv_write_indirect(sc, SV_DMAC_COUNT0, dma_count & 0xFF);

	mode = sv_read_indirect(sc, SV_PLAY_RECORD_ENABLE);
	sv_write_indirect(sc, SV_PLAY_RECORD_ENABLE, mode | SV_RECORD_ENABLE);

	return 0;
}

static int
sv_halt_output(void *addr)
{
	struct sv_softc *sc;
	uint8_t mode;

	DPRINTF(("sv: sv_halt_output\n"));
	sc = addr;
	mode = sv_read_indirect(sc, SV_PLAY_RECORD_ENABLE);
	sv_write_indirect(sc, SV_PLAY_RECORD_ENABLE, mode & ~SV_PLAY_ENABLE);
	sc->sc_pintr = 0;

	return 0;
}

static int
sv_halt_input(void *addr)
{
	struct sv_softc *sc;
	uint8_t mode;

	DPRINTF(("sv: sv_halt_input\n"));
	sc = addr;
	mode = sv_read_indirect(sc, SV_PLAY_RECORD_ENABLE);
	sv_write_indirect(sc, SV_PLAY_RECORD_ENABLE, mode & ~SV_RECORD_ENABLE);
	sc->sc_rintr = 0;

	return 0;
}

static int
sv_getdev(void *addr, struct audio_device *retp)
{

	*retp = sv_device;
	return 0;
}


/*
 * Mixer related code is here
 *
 */

#define SV_INPUT_CLASS 0
#define SV_OUTPUT_CLASS 1
#define SV_RECORD_CLASS 2

#define SV_LAST_CLASS 2

static const char *mixer_classes[] =
	{ AudioCinputs, AudioCoutputs, AudioCrecord };

static const struct {
	uint8_t   l_port;
	uint8_t   r_port;
	uint8_t   mask;
	uint8_t   class;
	const char *audio;
} ports[] = {
  { SV_LEFT_AUX1_INPUT_CONTROL, SV_RIGHT_AUX1_INPUT_CONTROL, SV_AUX1_MASK,
    SV_INPUT_CLASS, "aux1" },
  { SV_LEFT_CD_INPUT_CONTROL, SV_RIGHT_CD_INPUT_CONTROL, SV_CD_MASK,
    SV_INPUT_CLASS, AudioNcd },
  { SV_LEFT_LINE_IN_INPUT_CONTROL, SV_RIGHT_LINE_IN_INPUT_CONTROL, SV_LINE_IN_MASK,
    SV_INPUT_CLASS, AudioNline },
  { SV_MIC_INPUT_CONTROL, 0, SV_MIC_MASK, SV_INPUT_CLASS, AudioNmicrophone },
  { SV_LEFT_SYNTH_INPUT_CONTROL, SV_RIGHT_SYNTH_INPUT_CONTROL,
    SV_SYNTH_MASK, SV_INPUT_CLASS, AudioNfmsynth },
  { SV_LEFT_AUX2_INPUT_CONTROL, SV_RIGHT_AUX2_INPUT_CONTROL, SV_AUX2_MASK,
    SV_INPUT_CLASS, "aux2" },
  { SV_LEFT_PCM_INPUT_CONTROL, SV_RIGHT_PCM_INPUT_CONTROL, SV_PCM_MASK,
    SV_INPUT_CLASS, AudioNdac },
  { SV_LEFT_MIXER_OUTPUT_CONTROL, SV_RIGHT_MIXER_OUTPUT_CONTROL,
    SV_MIXER_OUT_MASK, SV_OUTPUT_CLASS, AudioNmaster }
};


static const struct {
	int idx;
	const char *name;
} record_sources[] = {
	{ SV_REC_CD, AudioNcd },
	{ SV_REC_DAC, AudioNdac },
	{ SV_REC_AUX2, "aux2" },
	{ SV_REC_LINE, AudioNline },
	{ SV_REC_AUX1, "aux1" },
	{ SV_REC_MIC, AudioNmicrophone },
	{ SV_REC_MIXER, AudioNmixerout }
};


#define SV_DEVICES_PER_PORT 2
#define SV_FIRST_MIXER (SV_LAST_CLASS + 1)
#define SV_LAST_MIXER (SV_DEVICES_PER_PORT * (ARRAY_SIZE(ports)) + SV_LAST_CLASS)
#define SV_RECORD_SOURCE (SV_LAST_MIXER + 1)
#define SV_MIC_BOOST (SV_LAST_MIXER + 2)
#define SV_RECORD_GAIN (SV_LAST_MIXER + 3)
#define SV_SRS_MODE (SV_LAST_MIXER + 4)

static int
sv_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	int i;

	/* It's a class */
	if (dip->index <= SV_LAST_CLASS) {
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = dip->index;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, mixer_classes[dip->index]);
		return 0;
	}

	if (dip->index >= SV_FIRST_MIXER &&
	    dip->index <= SV_LAST_MIXER) {
		int off, mute ,idx;

		off = dip->index - SV_FIRST_MIXER;
		mute = (off % SV_DEVICES_PER_PORT);
		idx = off / SV_DEVICES_PER_PORT;
		dip->mixer_class = ports[idx].class;
		strcpy(dip->label.name, ports[idx].audio);

		if (!mute) {
			dip->type = AUDIO_MIXER_VALUE;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = dip->index + 1;

			if (ports[idx].r_port != 0)
				dip->un.v.num_channels = 2;
			else
				dip->un.v.num_channels = 1;

			strcpy(dip->un.v.units.name, AudioNvolume);
		} else {
			dip->type = AUDIO_MIXER_ENUM;
			dip->prev = dip->index - 1;
			dip->next = AUDIO_MIXER_LAST;

			strcpy(dip->label.name, AudioNmute);
			dip->un.e.num_mem = 2;
			strcpy(dip->un.e.member[0].label.name, AudioNoff);
			dip->un.e.member[0].ord = 0;
			strcpy(dip->un.e.member[1].label.name, AudioNon);
			dip->un.e.member[1].ord = 1;
		}

		return 0;
	}

	switch (dip->index) {
	case SV_RECORD_SOURCE:
		dip->mixer_class = SV_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = SV_RECORD_GAIN;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_ENUM;

		dip->un.e.num_mem = ARRAY_SIZE(record_sources);
		for (i = 0; i < ARRAY_SIZE(record_sources); i++) {
			strcpy(dip->un.e.member[i].label.name,
			       record_sources[i].name);
			dip->un.e.member[i].ord = record_sources[i].idx;
		}
		return 0;

	case SV_RECORD_GAIN:
		dip->mixer_class = SV_RECORD_CLASS;
		dip->prev = SV_RECORD_SOURCE;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "gain");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case SV_MIC_BOOST:
		dip->mixer_class = SV_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "micboost");
		goto on_off;

	case SV_SRS_MODE:
		dip->mixer_class = SV_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNspatial);

	on_off:
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;
	}

	return ENXIO;
}

static int
sv_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct sv_softc *sc;
	uint8_t reg;
	int idx;

	sc = addr;
	if (cp->dev >= SV_FIRST_MIXER &&
	    cp->dev <= SV_LAST_MIXER) {
		int off, mute;

		off = cp->dev - SV_FIRST_MIXER;
		mute = (off % SV_DEVICES_PER_PORT);
		idx = off / SV_DEVICES_PER_PORT;

		if (mute) {
			if (cp->type != AUDIO_MIXER_ENUM)
				return EINVAL;

			mutex_spin_enter(&sc->sc_intr_lock);
			reg = sv_read_indirect(sc, ports[idx].l_port);
			if (cp->un.ord)
				reg |= SV_MUTE_BIT;
			else
				reg &= ~SV_MUTE_BIT;
			sv_write_indirect(sc, ports[idx].l_port, reg);

			if (ports[idx].r_port) {
				reg = sv_read_indirect(sc, ports[idx].r_port);
				if (cp->un.ord)
					reg |= SV_MUTE_BIT;
				else
					reg &= ~SV_MUTE_BIT;
				sv_write_indirect(sc, ports[idx].r_port, reg);
			}
			mutex_spin_exit(&sc->sc_intr_lock);
		} else {
			int  lval, rval;

			if (cp->type != AUDIO_MIXER_VALUE)
				return EINVAL;

			if (cp->un.value.num_channels != 1 &&
			    cp->un.value.num_channels != 2)
				return (EINVAL);

			if (ports[idx].r_port == 0) {
				if (cp->un.value.num_channels != 1)
					return (EINVAL);
				lval = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
				rval = 0; /* shut up GCC */
			} else {
				if (cp->un.value.num_channels != 2)
					return (EINVAL);

				lval = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
				rval = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			}

			mutex_spin_enter(&sc->sc_intr_lock);
			reg = sv_read_indirect(sc, ports[idx].l_port);
			reg &= ~(ports[idx].mask);
			lval = (AUDIO_MAX_GAIN - lval) * ports[idx].mask /
				AUDIO_MAX_GAIN;
			reg |= lval;
			sv_write_indirect(sc, ports[idx].l_port, reg);

			if (ports[idx].r_port != 0) {
				reg = sv_read_indirect(sc, ports[idx].r_port);
				reg &= ~(ports[idx].mask);

				rval = (AUDIO_MAX_GAIN - rval) * ports[idx].mask /
					AUDIO_MAX_GAIN;
				reg |= rval;

				sv_write_indirect(sc, ports[idx].r_port, reg);
			}

			sv_read_indirect(sc, ports[idx].l_port);
			mutex_spin_exit(&sc->sc_intr_lock);
		}

		return 0;
	}


	switch (cp->dev) {
	case SV_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		for (idx = 0; idx < ARRAY_SIZE(record_sources); idx++) {
			if (record_sources[idx].idx == cp->un.ord)
				goto found;
		}

		return EINVAL;

	found:
		mutex_spin_enter(&sc->sc_intr_lock);
		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL);
		reg &= ~SV_REC_SOURCE_MASK;
		reg |= (((cp->un.ord) << SV_REC_SOURCE_SHIFT) & SV_REC_SOURCE_MASK);
		sv_write_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL, reg);

		reg = sv_read_indirect(sc, SV_RIGHT_ADC_INPUT_CONTROL);
		reg &= ~SV_REC_SOURCE_MASK;
		reg |= (((cp->un.ord) << SV_REC_SOURCE_SHIFT) & SV_REC_SOURCE_MASK);
		sv_write_indirect(sc, SV_RIGHT_ADC_INPUT_CONTROL, reg);
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;

	case SV_RECORD_GAIN:
	{
		int val;

		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		if (cp->un.value.num_channels != 1)
			return EINVAL;

		val = (cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]
		    * SV_REC_GAIN_MASK) / AUDIO_MAX_GAIN;

		mutex_spin_enter(&sc->sc_intr_lock);
		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL);
		reg &= ~SV_REC_GAIN_MASK;
		reg |= val;
		sv_write_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL, reg);

		reg = sv_read_indirect(sc, SV_RIGHT_ADC_INPUT_CONTROL);
		reg &= ~SV_REC_GAIN_MASK;
		reg |= val;
		sv_write_indirect(sc, SV_RIGHT_ADC_INPUT_CONTROL, reg);
		mutex_spin_exit(&sc->sc_intr_lock);
	}
	return (0);

	case SV_MIC_BOOST:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mutex_spin_enter(&sc->sc_intr_lock);
		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL);
		if (cp->un.ord) {
			reg |= SV_MIC_BOOST_BIT;
		} else {
			reg &= ~SV_MIC_BOOST_BIT;
		}

		sv_write_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL, reg);
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;

	case SV_SRS_MODE:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		mutex_spin_enter(&sc->sc_intr_lock);
		reg = sv_read_indirect(sc, SV_SRS_SPACE_CONTROL);
		if (cp->un.ord) {
			reg &= ~SV_SRS_SPACE_ONOFF;
		} else {
			reg |= SV_SRS_SPACE_ONOFF;
		}

		sv_write_indirect(sc, SV_SRS_SPACE_CONTROL, reg);
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	return EINVAL;
}

static int
sv_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct sv_softc *sc;
	int val, error;
	uint8_t reg;

	sc = addr;
	error = 0;

	mutex_spin_enter(&sc->sc_intr_lock);

	if (cp->dev >= SV_FIRST_MIXER &&
	    cp->dev <= SV_LAST_MIXER) {
		int off = cp->dev - SV_FIRST_MIXER;
		int mute = (off % 2);
		int idx = off / 2;

		off = cp->dev - SV_FIRST_MIXER;
		mute = (off % 2);
		idx = off / 2;
		if (mute) {
			if (cp->type != AUDIO_MIXER_ENUM)
				error = EINVAL;
			else {
				reg = sv_read_indirect(sc, ports[idx].l_port);
				cp->un.ord = ((reg & SV_MUTE_BIT) ? 1 : 0);
			}
		} else {
			if (cp->type != AUDIO_MIXER_VALUE ||
			    (cp->un.value.num_channels != 1 &&
			    cp->un.value.num_channels != 2) ||
			   ((ports[idx].r_port == 0 &&
			     cp->un.value.num_channels != 1) ||
			    (ports[idx].r_port != 0 &&
			     cp->un.value.num_channels != 2)))
				error = EINVAL;
			else {
				reg = sv_read_indirect(sc, ports[idx].l_port);
				reg &= ports[idx].mask;

				val = AUDIO_MAX_GAIN -
				    ((reg * AUDIO_MAX_GAIN) / ports[idx].mask);

				if (ports[idx].r_port != 0) {
					cp->un.value.level
					    [AUDIO_MIXER_LEVEL_LEFT] = val;

					reg = sv_read_indirect(sc,
					    ports[idx].r_port);
					reg &= ports[idx].mask;

					val = AUDIO_MAX_GAIN -
					    ((reg * AUDIO_MAX_GAIN)
					    / ports[idx].mask);
					cp->un.value.level
					    [AUDIO_MIXER_LEVEL_RIGHT] = val;
				} else
					cp->un.value.level
					    [AUDIO_MIXER_LEVEL_MONO] = val;
			}
		}

		return error;
	}

	switch (cp->dev) {
	case SV_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_ENUM) {
			error = EINVAL;
			break;
		}

		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL);
		cp->un.ord = ((reg & SV_REC_SOURCE_MASK) >> SV_REC_SOURCE_SHIFT);

		break;

	case SV_RECORD_GAIN:
		if (cp->type != AUDIO_MIXER_VALUE) {
			error = EINVAL;
			break;
		}
		if (cp->un.value.num_channels != 1) {
			error = EINVAL;
			break;
		}

		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL) & SV_REC_GAIN_MASK;
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			(((unsigned int)reg) * AUDIO_MAX_GAIN) / SV_REC_GAIN_MASK;

		break;

	case SV_MIC_BOOST:
		if (cp->type != AUDIO_MIXER_ENUM) {
			error = EINVAL;
			break;
		}
		reg = sv_read_indirect(sc, SV_LEFT_ADC_INPUT_CONTROL);
		cp->un.ord = ((reg & SV_MIC_BOOST_BIT) ? 1 : 0);
		break;

	case SV_SRS_MODE:
		if (cp->type != AUDIO_MIXER_ENUM) {
			error = EINVAL;
			break;
		}
		reg = sv_read_indirect(sc, SV_SRS_SPACE_CONTROL);
		cp->un.ord = ((reg & SV_SRS_SPACE_ONOFF) ? 0 : 1);
		break;
	default:
		error = EINVAL;
		break;
	}

	mutex_spin_exit(&sc->sc_intr_lock);
	return error;
}

static void
sv_init_mixer(struct sv_softc *sc)
{
	mixer_ctrl_t cp;
	int i;

	cp.type = AUDIO_MIXER_ENUM;
	cp.dev = SV_SRS_MODE;
	cp.un.ord = 0;

	sv_mixer_set_port(sc, &cp);

	for (i = 0; i < ARRAY_SIZE(ports); i++) {
		if (!strcmp(ports[i].audio, AudioNdac)) {
			cp.type = AUDIO_MIXER_ENUM;
			cp.dev = SV_FIRST_MIXER + i * SV_DEVICES_PER_PORT + 1;
			cp.un.ord = 0;
			sv_mixer_set_port(sc, &cp);
			break;
		}
	}
}

static void *
sv_malloc(void *addr, int direction, size_t size)
{
	struct sv_softc *sc;
	struct sv_dma *p;
	int error;

	sc = addr;
	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	error = sv_allocmem(sc, size, 16, direction, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		return 0;
	}
	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return KERNADDR(p);
}

static void
sv_free(void *addr, void *ptr, size_t size)
{
	struct sv_softc *sc;
	struct sv_dma **pp, *p;

	sc = addr;
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			sv_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

static size_t
sv_round_buffersize(void *addr, int direction, size_t size)
{

	return size;
}

static paddr_t
sv_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct sv_softc *sc;
	struct sv_dma *p;

	sc = addr;
	if (off < 0)
		return -1;
	for (p = sc->sc_dmas; p && KERNADDR(p) != mem; p = p->next)
		continue;
	if (p == NULL)
		return -1;
	return bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs,
			       off, prot, BUS_DMA_WAITOK);
}

static int
sv_get_props(void *addr)
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

static void
sv_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct sv_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

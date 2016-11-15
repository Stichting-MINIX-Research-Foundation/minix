/*	$NetBSD: cs428x.c,v 1.17 2012/10/27 17:18:31 chs Exp $	*/

/*
 * Copyright (c) 2000 Tatoku Ogaito.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Tatoku Ogaito
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/* Common functions for CS4280 and CS4281 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs428x.c,v 1.17 2012/10/27 17:18:31 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/audioio.h>
#include <sys/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/cs428xreg.h>
#include <dev/pci/cs428x.h>

#if defined(CS4280_DEBUG) || defined(CS4281_DEBUG)
int cs428x_debug = 0;
#endif

int
cs428x_round_blocksize(void *addr, int blk,
    int mode, const audio_params_t *param)
{
	struct cs428x_softc *sc;
	int retval;

	DPRINTFN(5,("cs428x_round_blocksize blk=%d -> ", blk));

	sc = addr;
	if (blk < sc->hw_blocksize)
		retval = sc->hw_blocksize;
	else
		retval = blk & -(sc->hw_blocksize);

	DPRINTFN(5,("%d\n", retval));

	return retval;
}

int
cs428x_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct cs428x_softc *sc;
	int val;

	sc = addr;
	val = sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
	DPRINTFN(3,("mixer_set_port: val=%d\n", val));
	return (val);
}

int
cs428x_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct cs428x_softc *sc;

	sc = addr;
	return (sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp));
}

int
cs428x_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct cs428x_softc *sc;

	sc = addr;
	return (sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip));
}

void *
cs428x_malloc(void *addr, int direction, size_t size)
{
	struct cs428x_softc *sc;
	struct cs428x_dma   *p;
	int error;

	sc = addr;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return 0;

	error = cs428x_allocmem(sc, size, p);

	if (error) {
		kmem_free(p, sizeof(*p));
		return 0;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return BUFADDR(p);
}

void
cs428x_free(void *addr, void *ptr, size_t size)
{
	struct cs428x_softc *sc;
	struct cs428x_dma **pp, *p;

	sc = addr;
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (BUFADDR(p) == ptr) {
			bus_dmamap_unload(sc->sc_dmatag, p->map);
			bus_dmamap_destroy(sc->sc_dmatag, p->map);
			bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
			kmem_free(p->dum, p->size);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

size_t
cs428x_round_buffersize(void *addr, int direction,
    size_t size)
{
	/* The real DMA buffersize are 4KB for CS4280
	 * and 64kB/MAX_CHANNELS for CS4281.
	 * But they are too small for high quality audio,
	 * let the upper layer(audio) use a larger buffer.
	 * (originally suggested by Lennart Augustsson.)
	 */
	return size;
}

paddr_t
cs428x_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct cs428x_softc *sc;
	struct cs428x_dma *p;

	sc = addr;

	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && BUFADDR(p) != mem; p = p->next)
		continue;

	if (p == NULL) {
		DPRINTF(("cs428x_mappage: bad buffer address\n"));
		return -1;
	}

	return (bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK));
}

int
cs428x_get_props(void *addr)
{
	int retval;

	retval = AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
#ifdef MMAP_READY
	/* How can I mmap ? */
	retval |= AUDIO_PROP_MMAP;
#endif
	return retval;
}

/* AC97 */
int
cs428x_attach_codec(void *addr, struct ac97_codec_if *codec_if)
{
	struct cs428x_softc *sc;

	DPRINTF(("cs428x_attach_codec:\n"));
	sc = addr;
	sc->codec_if = codec_if;
	return 0;
}

int
cs428x_read_codec(void *addr, uint8_t ac97_addr, uint16_t *ac97_data)
{
	struct cs428x_softc *sc;
	uint32_t acctl;
	int n;

	sc = addr;

	DPRINTFN(5,("read_codec: add=0x%02x ", ac97_addr));
	/*
	 * Make sure that there is not data sitting around from a previous
	 * uncompleted access.
	 */
	BA0READ4(sc, CS428X_ACSDA);

	/* Set up AC97 control registers. */
	BA0WRITE4(sc, CS428X_ACCAD, ac97_addr);
	BA0WRITE4(sc, CS428X_ACCDA, 0);

	acctl = ACCTL_ESYN | ACCTL_VFRM | ACCTL_CRW  | ACCTL_DCV;
	if (sc->type == TYPE_CS4280)
		acctl |= ACCTL_RSTN;
	BA0WRITE4(sc, CS428X_ACCTL, acctl);

	if (cs428x_src_wait(sc) < 0) {
		printf("%s: AC97 read prob. (DCV!=0) for add=0x%0x\n",
		       device_xname(sc->sc_dev), ac97_addr);
		return 1;
	}

	/* wait for valid status bit is active */
	n = 0;
	while ((BA0READ4(sc, CS428X_ACSTS) & ACSTS_VSTS) == 0) {
		delay(1);
		while (++n > 1000) {
			printf("%s: AC97 read fail (VSTS==0) for add=0x%0x\n",
			       device_xname(sc->sc_dev), ac97_addr);
			return 1;
		}
	}
	*ac97_data = BA0READ4(sc, CS428X_ACSDA);
	DPRINTFN(5,("data=0x%04x\n", *ac97_data));
	return 0;
}

int
cs428x_write_codec(void *addr, uint8_t ac97_addr, uint16_t ac97_data)
{
	struct cs428x_softc *sc;
	uint32_t acctl;

	sc = addr;

	DPRINTFN(5,("write_codec: add=0x%02x  data=0x%04x\n", ac97_addr, ac97_data));
	BA0WRITE4(sc, CS428X_ACCAD, ac97_addr);
	BA0WRITE4(sc, CS428X_ACCDA, ac97_data);

	acctl = ACCTL_ESYN | ACCTL_VFRM | ACCTL_DCV;
	if (sc->type == TYPE_CS4280)
		acctl |= ACCTL_RSTN;
	BA0WRITE4(sc, CS428X_ACCTL, acctl);

	if (cs428x_src_wait(sc) < 0) {
		printf("%s: AC97 write fail (DCV!=0) for add=0x%02x data="
		       "0x%04x\n", device_xname(sc->sc_dev), ac97_addr, ac97_data);
		return 1;
	}
	return 0;
}

/* Internal functions */
int
cs428x_allocmem(struct cs428x_softc *sc, size_t size, struct cs428x_dma *p)
{
	int error;
	size_t align;

	align   = sc->dma_align;
	p->size = sc->dma_size;
	/* allocate memory for upper audio driver */
	p->dum  = kmem_alloc(size, KM_SLEEP);
	if (p->dum == NULL)
		return 1;

	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_WAITOK);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to allocate DMA. error=%d\n",
		       error);
		goto allfree;
	}

	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size,
			       &p->addr, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to map DMA, error=%d\n",
		       error);
		goto free;
	}

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
				  0, BUS_DMA_WAITOK, &p->map);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to create DMA map, error=%d\n",
		       error);
		goto unmap;
	}

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL,
				BUS_DMA_WAITOK);
	if (error) {
		aprint_error_dev(sc->sc_dev, "unable to load DMA map, error=%d\n",
		       error);
		goto destroy;
	}
	return 0;

 destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
 unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
 free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
 allfree:
	kmem_free(p->dum, size);

	return error;
}

int
cs428x_src_wait(struct cs428x_softc *sc)
{
	int n;

	n = 0;
	while ((BA0READ4(sc, CS428X_ACCTL) & ACCTL_DCV)) {
		delay(1000);
		while (++n > 1000) {
			printf("cs428x_src_wait: 0x%08x\n",
			    BA0READ4(sc, CS428X_ACCTL));
			return -1;
		}
	}
	return 0;
}

void
cs428x_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct cs428x_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

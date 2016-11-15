/*	$NetBSD: mvmebus.c,v 1.19 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 2000, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
__KERNEL_RCSID(0, "$NetBSD: mvmebus.c,v 1.19 2012/10/27 17:18:27 chs Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kcore.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/mvme/mvmebus.h>

#ifdef DIAGNOSTIC
int	mvmebus_dummy_dmamap_create(bus_dma_tag_t, bus_size_t, int, bus_size_t,
	    bus_size_t, int, bus_dmamap_t *);
void	mvmebus_dummy_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
int	mvmebus_dummy_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t,
	    bus_size_t, bus_dma_segment_t *, int, int *, int);
void	mvmebus_dummy_dmamem_free(bus_dma_tag_t, bus_dma_segment_t *, int);
#endif

#ifdef DEBUG
static const char *mvmebus_mod_string(vme_addr_t, vme_size_t,
	    vme_am_t, vme_datasize_t);
#endif

static void mvmebus_offboard_ram(struct mvmebus_softc *);
static int mvmebus_dmamap_load_common(struct mvmebus_softc *, bus_dmamap_t);

vme_am_t	_mvmebus_am_cap[] = {
	MVMEBUS_AM_CAP_BLKD64 | MVMEBUS_AM_CAP_USER,
	MVMEBUS_AM_CAP_DATA   | MVMEBUS_AM_CAP_USER,
	MVMEBUS_AM_CAP_PROG   | MVMEBUS_AM_CAP_USER,
	MVMEBUS_AM_CAP_BLK    | MVMEBUS_AM_CAP_USER,
	MVMEBUS_AM_CAP_BLKD64 | MVMEBUS_AM_CAP_SUPER,
	MVMEBUS_AM_CAP_DATA   | MVMEBUS_AM_CAP_SUPER,
	MVMEBUS_AM_CAP_PROG   | MVMEBUS_AM_CAP_SUPER,
	MVMEBUS_AM_CAP_BLK    | MVMEBUS_AM_CAP_SUPER
};

const char *mvmebus_irq_name[] = {
	"vmeirq0", "vmeirq1", "vmeirq2", "vmeirq3",
	"vmeirq4", "vmeirq5", "vmeirq6", "vmeirq7"
};

extern phys_ram_seg_t mem_clusters[0];
extern int mem_cluster_cnt;


static void
mvmebus_offboard_ram(struct mvmebus_softc *sc)
{
	struct mvmebus_range *svr, *mvr;
	vme_addr_t start, end, size;
	int i;

	/*
	 * If we have any offboard RAM (i.e. a VMEbus RAM board) then
	 * we need to record its details since it's effectively another
	 * VMEbus slave image as far as we're concerned.
	 * The chip-specific backend will have reserved sc->sc_slaves[0]
	 * for exactly this purpose.
	 */
	svr = sc->sc_slaves;
	if (mem_cluster_cnt < 2) {
		svr->vr_am = MVMEBUS_AM_DISABLED;
		return;
	}

	start = mem_clusters[1].start;
	size = mem_clusters[1].size - 1;
	end = start + size;

	/*
	 * Figure out which VMEbus master image the RAM is
	 * visible through. This will tell us the address
	 * modifier and datasizes it uses, as well as allowing
	 * us to calculate its `real' VMEbus address.
	 *
	 * XXX FIXME: This is broken if the RAM is mapped through
	 * a translated address space. For example, on mvme167 it's
	 * perfectly legal to set up the following A32 mapping:
	 *
	 *  vr_locaddr  == 0x80000000
	 *  vr_vmestart == 0x10000000
	 *  vr_vmeend   == 0x10ffffff
	 *
	 * In this case, RAM at VMEbus address 0x10800000 will appear at local
	 * address 0x80800000, but we need to set the slave vr_vmestart to
	 * 0x10800000.
	 */
	for (i = 0, mvr = sc->sc_masters; i < sc->sc_nmasters; i++, mvr++) {
		vme_addr_t vstart = mvr->vr_locstart + mvr->vr_vmestart;

		if (start >= vstart &&
		    end <= vstart + (mvr->vr_vmeend - mvr->vr_vmestart))
			break;
	}
	if (i == sc->sc_nmasters) {
		svr->vr_am = MVMEBUS_AM_DISABLED;
#ifdef DEBUG
		printf("%s: No VMEbus master mapping for offboard RAM!\n",
		    device_xname(sc->sc_dev));
#endif
		return;
	}

	svr->vr_locstart = start;
	svr->vr_vmestart = start & mvr->vr_mask;
	svr->vr_vmeend = svr->vr_vmestart + size;
	svr->vr_datasize = mvr->vr_datasize;
	svr->vr_mask = mvr->vr_mask;
	svr->vr_am = mvr->vr_am & VME_AM_ADRSIZEMASK;
	svr->vr_am |= MVMEBUS_AM_CAP_DATA  | MVMEBUS_AM_CAP_PROG |
		      MVMEBUS_AM_CAP_SUPER | MVMEBUS_AM_CAP_USER;
}

void
mvmebus_attach(struct mvmebus_softc *sc)
{
	struct vmebus_attach_args vaa;
	int i;

	/* Zap the IRQ reference counts */
	for (i = 0; i < 8; i++)
		sc->sc_irqref[i] = 0;

	/* If there's offboard RAM, get its VMEbus slave attributes */
	mvmebus_offboard_ram(sc);

#ifdef DEBUG
	for (i = 0; i < sc->sc_nmasters; i++) {
		struct mvmebus_range *vr = &sc->sc_masters[i];
		if (vr->vr_am == MVMEBUS_AM_DISABLED) {
			printf("%s: Master#%d: disabled\n",
			    device_xname(sc->sc_dev), i);
			continue;
		}
		printf("%s: Master#%d: 0x%08lx -> %s\n",
		    device_xname(sc->sc_dev), i,
		    vr->vr_locstart + (vr->vr_vmestart & vr->vr_mask),
		    mvmebus_mod_string(vr->vr_vmestart,
			(vr->vr_vmeend - vr->vr_vmestart) + 1,
			vr->vr_am, vr->vr_datasize));
	}

	for (i = 0; i < sc->sc_nslaves; i++) {
		struct mvmebus_range *vr = &sc->sc_slaves[i];
		if (vr->vr_am == MVMEBUS_AM_DISABLED) {
			printf("%s:  Slave#%d: disabled\n",
			    device_xname(sc->sc_dev), i);
			continue;
		}
		printf("%s:  Slave#%d: 0x%08lx -> %s\n",
		    device_xname(sc->sc_dev), i, vr->vr_locstart,
		    mvmebus_mod_string(vr->vr_vmestart,
			(vr->vr_vmeend - vr->vr_vmestart) + 1,
			vr->vr_am, vr->vr_datasize));
	}
#endif

	sc->sc_vct.cookie = sc;
	sc->sc_vct.vct_probe = mvmebus_probe;
	sc->sc_vct.vct_map = mvmebus_map;
	sc->sc_vct.vct_unmap = mvmebus_unmap;
	sc->sc_vct.vct_int_map = mvmebus_intmap;
	sc->sc_vct.vct_int_evcnt = mvmebus_intr_evcnt;
	sc->sc_vct.vct_int_establish = mvmebus_intr_establish;
	sc->sc_vct.vct_int_disestablish = mvmebus_intr_disestablish;
	sc->sc_vct.vct_dmamap_create = mvmebus_dmamap_create;
	sc->sc_vct.vct_dmamap_destroy = mvmebus_dmamap_destroy;
	sc->sc_vct.vct_dmamem_alloc = mvmebus_dmamem_alloc;
	sc->sc_vct.vct_dmamem_free = mvmebus_dmamem_free;

	sc->sc_mvmedmat._cookie = sc;
	sc->sc_mvmedmat._dmamap_load = mvmebus_dmamap_load;
	sc->sc_mvmedmat._dmamap_load_mbuf = mvmebus_dmamap_load_mbuf;
	sc->sc_mvmedmat._dmamap_load_uio = mvmebus_dmamap_load_uio;
	sc->sc_mvmedmat._dmamap_load_raw = mvmebus_dmamap_load_raw;
	sc->sc_mvmedmat._dmamap_unload = mvmebus_dmamap_unload;
	sc->sc_mvmedmat._dmamap_sync = mvmebus_dmamap_sync;
	sc->sc_mvmedmat._dmamem_map = mvmebus_dmamem_map;
	sc->sc_mvmedmat._dmamem_unmap = mvmebus_dmamem_unmap;
	sc->sc_mvmedmat._dmamem_mmap = mvmebus_dmamem_mmap;

#ifdef DIAGNOSTIC
	sc->sc_mvmedmat._dmamap_create = mvmebus_dummy_dmamap_create;
	sc->sc_mvmedmat._dmamap_destroy = mvmebus_dummy_dmamap_destroy;
	sc->sc_mvmedmat._dmamem_alloc = mvmebus_dummy_dmamem_alloc;
	sc->sc_mvmedmat._dmamem_free = mvmebus_dummy_dmamem_free;
#else
	sc->sc_mvmedmat._dmamap_create = NULL;
	sc->sc_mvmedmat._dmamap_destroy = NULL;
	sc->sc_mvmedmat._dmamem_alloc = NULL;
	sc->sc_mvmedmat._dmamem_free = NULL;
#endif

	vaa.va_vct = &sc->sc_vct;
	vaa.va_bdt = &sc->sc_mvmedmat;
	vaa.va_slaveconfig = NULL;

	config_found(sc->sc_dev, &vaa, 0);
}

int
mvmebus_map(void *vsc, vme_addr_t vmeaddr, vme_size_t len, vme_am_t am, vme_datasize_t datasize, vme_swap_t swap, bus_space_tag_t *tag, bus_space_handle_t *handle, vme_mapresc_t *resc)
{
	struct mvmebus_softc *sc;
	struct mvmebus_mapresc *mr;
	struct mvmebus_range *vr;
	vme_addr_t end;
	vme_am_t cap, as;
	paddr_t paddr;
	int rv, i;

	sc = vsc;
	end = (vmeaddr + len) - 1;
	paddr = 0;
	vr = sc->sc_masters;
	cap = MVMEBUS_AM2CAP(am);
	as = am & VME_AM_ADRSIZEMASK;

	for (i = 0; i < sc->sc_nmasters && paddr == 0; i++, vr++) {
		if (vr->vr_am == MVMEBUS_AM_DISABLED)
			continue;

		if (cap == (vr->vr_am & cap) &&
		    as == (vr->vr_am & VME_AM_ADRSIZEMASK) &&
		    datasize <= vr->vr_datasize &&
		    vmeaddr >= vr->vr_vmestart && end < vr->vr_vmeend)
			paddr = vr->vr_locstart + (vmeaddr & vr->vr_mask);
	}
	if (paddr == 0)
		return (ENOMEM);

	rv = bus_space_map(sc->sc_bust, paddr, len, 0, handle);
	if (rv != 0)
		return (rv);

	/* Allocate space for the resource tag */
	if ((mr = malloc(sizeof(*mr), M_DEVBUF, M_NOWAIT)) == NULL) {
		bus_space_unmap(sc->sc_bust, *handle, len);
		return (ENOMEM);
	}

	/* Record the range's details */
	mr->mr_am = am;
	mr->mr_datasize = datasize;
	mr->mr_addr = vmeaddr;
	mr->mr_size = len;
	mr->mr_handle = *handle;
	mr->mr_range = i;

	*tag = sc->sc_bust;
	*resc = (vme_mapresc_t *) mr;

	return (0);
}

/* ARGSUSED */
void
mvmebus_unmap(void *vsc, vme_mapresc_t resc)
{
	struct mvmebus_softc *sc = vsc;
	struct mvmebus_mapresc *mr = (struct mvmebus_mapresc *) resc;

	bus_space_unmap(sc->sc_bust, mr->mr_handle, mr->mr_size);

	free(mr, M_DEVBUF);
}

int
mvmebus_probe(void *vsc, vme_addr_t vmeaddr, vme_size_t len, vme_am_t am, vme_datasize_t datasize, int (*callback)(void *, bus_space_tag_t, bus_space_handle_t), void *arg)
{
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	vme_mapresc_t resc;
	vme_size_t offs;
	int rv;

	/* Get a temporary mapping to the VMEbus range */
	rv = mvmebus_map(vsc, vmeaddr, len, am, datasize, 0,
	    &tag, &handle, &resc);
	if (rv)
		return (rv);

	if (callback)
		rv = (*callback) (arg, tag, handle);
	else
		for (offs = 0; offs < len && rv == 0;) {
			switch (datasize) {
			case VME_D8:
				rv = bus_space_peek_1(tag, handle, offs, NULL);
				offs += 1;
				break;

			case VME_D16:
				rv = bus_space_peek_2(tag, handle, offs, NULL);
				offs += 2;
				break;

			case VME_D32:
				rv = bus_space_peek_4(tag, handle, offs, NULL);
				offs += 4;
				break;
			}
		}

	mvmebus_unmap(vsc, resc);

	return (rv);
}

/* ARGSUSED */
int
mvmebus_intmap(void *vsc, int level, int vector, vme_intr_handle_t *handlep)
{

	if (level < 1 || level > 7 || vector < 0x80 || vector > 0xff)
		return (EINVAL);

	/* This is rather gross */
	*handlep = (void *) (int) ((level << 8) | vector);
	return (0);
}

/* ARGSUSED */
const struct evcnt *
mvmebus_intr_evcnt(void *vsc, vme_intr_handle_t handle)
{
	struct mvmebus_softc *sc = vsc;

	return (&sc->sc_evcnt[(((int) handle) >> 8) - 1]);
}

void *
mvmebus_intr_establish(void *vsc, vme_intr_handle_t handle, int prior, int (*func)(void *), void *arg)
{
	struct mvmebus_softc *sc;
	int level, vector, first;

	sc = vsc;

	/* Extract the interrupt's level and vector */
	level = ((int) handle) >> 8;
	vector = ((int) handle) & 0xff;

#ifdef DIAGNOSTIC
	if (vector < 0 || vector > 0xff) {
		printf("%s: Illegal vector offset: 0x%x\n",
		    device_xname(sc->sc_dev), vector);
		panic("mvmebus_intr_establish");
	}
	if (level < 1 || level > 7) {
		printf("%s: Illegal interrupt level: %d\n",
		    device_xname(sc->sc_dev), level);
		panic("mvmebus_intr_establish");
	}
#endif

	first = (sc->sc_irqref[level]++ == 0);

	(*sc->sc_intr_establish)(sc->sc_chip, prior, level, vector, first,
	    func, arg, &sc->sc_evcnt[level - 1]);

	return ((void *) handle);
}

void
mvmebus_intr_disestablish(void *vsc, vme_intr_handle_t handle)
{
	struct mvmebus_softc *sc;
	int level, vector, last;

	sc = vsc;

	/* Extract the interrupt's level and vector */
	level = ((int) handle) >> 8;
	vector = ((int) handle) & 0xff;

#ifdef DIAGNOSTIC
	if (vector < 0 || vector > 0xff) {
		printf("%s: Illegal vector offset: 0x%x\n",
		    device_xname(sc->sc_dev), vector);
		panic("mvmebus_intr_disestablish");
	}
	if (level < 1 || level > 7) {
		printf("%s: Illegal interrupt level: %d\n",
		    device_xname(sc->sc_dev), level);
		panic("mvmebus_intr_disestablish");
	}
	if (sc->sc_irqref[level] == 0) {
		printf("%s: VMEirq#%d: Reference count already zero!\n",
		    device_xname(sc->sc_dev), level);
		panic("mvmebus_intr_disestablish");
	}
#endif

	last = (--(sc->sc_irqref[level]) == 0);

	(*sc->sc_intr_disestablish)(sc->sc_chip, level, vector, last,
	    &sc->sc_evcnt[level - 1]);
}

#ifdef DIAGNOSTIC
/* ARGSUSED */
int
mvmebus_dummy_dmamap_create(bus_dma_tag_t t, bus_size_t size, int nsegs, bus_size_t maxsegsz, bus_size_t boundary, int flags, bus_dmamap_t *dmamp)
{

	panic("Must use vme_dmamap_create() in place of bus_dmamap_create()");
	return (0);	/* Shutup the compiler */
}

/* ARGSUSED */
void
mvmebus_dummy_dmamap_destroy(bus_dma_tag_t t, bus_dmamap_t map)
{

	panic("Must use vme_dmamap_destroy() in place of bus_dmamap_destroy()");
}
#endif

/* ARGSUSED */
int
mvmebus_dmamap_create(
	void *vsc,
	vme_size_t len,
	vme_am_t am,
	vme_datasize_t datasize,
	vme_swap_t swap,
	int nsegs,
	vme_size_t segsz,
	vme_addr_t bound,
	int flags,
	bus_dmamap_t *mapp)
{
	struct mvmebus_softc *sc = vsc;
	struct mvmebus_dmamap *vmap;
	struct mvmebus_range *vr;
	vme_am_t cap, as;
	int i, rv;

	cap = MVMEBUS_AM2CAP(am);
	as = am & VME_AM_ADRSIZEMASK;

	/*
	 * Verify that we even stand a chance of satisfying
	 * the VMEbus address space and datasize requested.
	 */
	for (i = 0, vr = sc->sc_slaves; i < sc->sc_nslaves; i++, vr++) {
		if (vr->vr_am == MVMEBUS_AM_DISABLED)
			continue;

		if (as == (vr->vr_am & VME_AM_ADRSIZEMASK) &&
		    cap == (vr->vr_am & cap) && datasize <= vr->vr_datasize &&
		    len <= (vr->vr_vmeend - vr->vr_vmestart))
			break;
	}

	if (i == sc->sc_nslaves)
		return (EINVAL);

	if ((vmap = malloc(sizeof(*vmap), M_DMAMAP,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);


	rv = bus_dmamap_create(sc->sc_dmat, len, nsegs, segsz,
	    bound, flags, mapp);
	if (rv != 0) {
		free(vmap, M_DMAMAP);
		return (rv);
	}

	vmap->vm_am = am;
	vmap->vm_datasize = datasize;
	vmap->vm_swap = swap;
	vmap->vm_slave = vr;

	(*mapp)->_dm_cookie = vmap;

	return (0);
}

void
mvmebus_dmamap_destroy(void *vsc, bus_dmamap_t map)
{
	struct mvmebus_softc *sc = vsc;

	free(map->_dm_cookie, M_DMAMAP);
	bus_dmamap_destroy(sc->sc_dmat, map);
}

static int
mvmebus_dmamap_load_common(struct mvmebus_softc *sc, bus_dmamap_t map)
{
	struct mvmebus_dmamap *vmap = map->_dm_cookie;
	struct mvmebus_range *vr = vmap->vm_slave;
	bus_dma_segment_t *ds;
	vme_am_t cap, am;
	int i;

	cap = MVMEBUS_AM2CAP(vmap->vm_am);
	am = vmap->vm_am & VME_AM_ADRSIZEMASK;

	/*
	 * Traverse the list of segments which make up this map, and
	 * convert the CPU-relative addresses therein to VMEbus addresses.
	 */
	for (ds = &map->dm_segs[0]; ds < &map->dm_segs[map->dm_nsegs]; ds++) {
		/*
		 * First, see if this map's slave image can access the
		 * segment, otherwise we have to waste time scanning all
		 * the slave images.
		 */
		vr = vmap->vm_slave;
		if (am == (vr->vr_am & VME_AM_ADRSIZEMASK) &&
		    cap == (vr->vr_am & cap) &&
		    vmap->vm_datasize <= vr->vr_datasize &&
		    ds->_ds_cpuaddr >= vr->vr_locstart &&
		    ds->ds_len <= (vr->vr_vmeend - vr->vr_vmestart))
			goto found;

		for (i = 0, vr = sc->sc_slaves; i < sc->sc_nslaves; i++, vr++) {
			if (vr->vr_am == MVMEBUS_AM_DISABLED)
				continue;

			/*
			 * Filter out any slave images which don't have the
			 * same VMEbus address modifier and datasize as
			 * this DMA map, and those which don't cover the
			 * physical address region containing the segment.
			 */
			if (vr != vmap->vm_slave &&
			    am == (vr->vr_am & VME_AM_ADRSIZEMASK) &&
			    cap == (vr->vr_am & cap) &&
			    vmap->vm_datasize <= vr->vr_datasize &&
			    ds->_ds_cpuaddr >= vr->vr_locstart &&
			    ds->ds_len <= (vr->vr_vmeend - vr->vr_vmestart))
				break;
		}

		/*
		 * Did we find an applicable slave image which covers this
		 * segment?
		 */
		if (i == sc->sc_nslaves) {
			/*
			 * XXX TODO:
			 *
			 * Bounce this segment via a bounce buffer allocated
			 * from this DMA map.
			 */
			printf("mvmebus_dmamap_load_common: bounce needed!\n");
			return (EINVAL);
		}

found:
		/*
		 * Generate the VMEbus address of this segment
		 */
		ds->ds_addr = (ds->_ds_cpuaddr - vr->vr_locstart) +
		    vr->vr_vmestart;
	}

	return (0);
}

int
mvmebus_dmamap_load(bus_dma_tag_t t, bus_dmamap_t map, void *buf, bus_size_t buflen, struct proc *p, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;
	int rv;

	rv = bus_dmamap_load(sc->sc_dmat, map, buf, buflen, p, flags);
	if (rv != 0)
		return rv;

	return mvmebus_dmamap_load_common(sc, map);
}

int
mvmebus_dmamap_load_mbuf(bus_dma_tag_t t, bus_dmamap_t map, struct mbuf *chain, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;
	int rv;

	rv = bus_dmamap_load_mbuf(sc->sc_dmat, map, chain, flags);
	if (rv != 0)
		return rv;

	return mvmebus_dmamap_load_common(sc, map);
}

int
mvmebus_dmamap_load_uio(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;
	int rv;

	rv = bus_dmamap_load_uio(sc->sc_dmat, map, uio, flags);
	if (rv != 0)
		return rv;

	return mvmebus_dmamap_load_common(sc, map);
}

int
mvmebus_dmamap_load_raw(bus_dma_tag_t t, bus_dmamap_t map, bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;
	int rv;

	/*
	 * mvmebus_dmamem_alloc() will ensure that the physical memory
	 * backing these segments is 100% accessible in at least one
	 * of the board's VMEbus slave images.
	 */
	rv = bus_dmamap_load_raw(sc->sc_dmat, map, segs, nsegs, size, flags);
	if (rv != 0)
		return rv;

	return mvmebus_dmamap_load_common(sc, map);
}

void
mvmebus_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct mvmebus_softc *sc = t->_cookie;

	/* XXX Deal with bounce buffers */

	bus_dmamap_unload(sc->sc_dmat, map);
}

void
mvmebus_dmamap_sync(bus_dma_tag_t t, bus_dmamap_t map, bus_addr_t offset, bus_size_t len, int ops)
{
	struct mvmebus_softc *sc = t->_cookie;

	/* XXX Bounce buffers */

	bus_dmamap_sync(sc->sc_dmat, map, offset, len, ops);
}

#ifdef DIAGNOSTIC
/* ARGSUSED */
int
mvmebus_dummy_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t align, bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{

	panic("Must use vme_dmamem_alloc() in place of bus_dmamem_alloc()");
}

/* ARGSUSED */
void
mvmebus_dummy_dmamem_free(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs)
{

	panic("Must use vme_dmamem_free() in place of bus_dmamem_free()");
}
#endif

/* ARGSUSED */
int
mvmebus_dmamem_alloc(void *vsc, vme_size_t len, vme_am_t am, vme_datasize_t datasize, vme_swap_t swap, bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags)
{
	extern paddr_t avail_start;
	struct mvmebus_softc *sc = vsc;
	struct mvmebus_range *vr;
	bus_addr_t low, high;
	bus_size_t bound;
	vme_am_t cap;
	int i;

	cap = MVMEBUS_AM2CAP(am);
	am &= VME_AM_ADRSIZEMASK;

	/*
	 * Find a slave mapping in the requested VMEbus address space.
	 */
	for (i = 0, vr = sc->sc_slaves; i < sc->sc_nslaves; i++, vr++) {
		if (vr->vr_am == MVMEBUS_AM_DISABLED)
			continue;

		if (i == 0 && (flags & BUS_DMA_ONBOARD_RAM) != 0)
			continue;

		if (am == (vr->vr_am & VME_AM_ADRSIZEMASK) &&
		    cap == (vr->vr_am & cap) && datasize <= vr->vr_datasize &&
		    len <= (vr->vr_vmeend - vr->vr_vmestart))
			break;
	}
	if (i == sc->sc_nslaves)
		return (EINVAL);

	/*
	 * Set up the constraints so we can allocate physical memory which
	 * is visible in the requested address space
	 */
	low = max(vr->vr_locstart, avail_start);
	high = vr->vr_locstart + (vr->vr_vmeend - vr->vr_vmestart) + 1;
	bound = (bus_size_t) vr->vr_mask + 1;

	/*
	 * Allocate physical memory.
	 *
	 * Note: This fills in the segments with CPU-relative physical
	 * addresses. A further call to bus_dmamap_load_raw() (with a
	 * DMA map which specifies the same VMEbus address space and
	 * constraints as the call to here) must be made. The segments
	 * of the DMA map will then contain VMEbus-relative physical
	 * addresses of the memory allocated here.
	 */
	return _bus_dmamem_alloc_common(sc->sc_dmat, low, high,
	    len, 0, bound, segs, nsegs, rsegs, flags);
}

void
mvmebus_dmamem_free(void *vsc, bus_dma_segment_t *segs, int nsegs)
{
	struct mvmebus_softc *sc = vsc;

	bus_dmamem_free(sc->sc_dmat, segs, nsegs);
}

int
mvmebus_dmamem_map(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, size_t size, void **kvap, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;

	return bus_dmamem_map(sc->sc_dmat, segs, nsegs, size, kvap, flags);
}

void
mvmebus_dmamem_unmap(bus_dma_tag_t t, void *kva, size_t size)
{
	struct mvmebus_softc *sc = t->_cookie;

	bus_dmamem_unmap(sc->sc_dmat, kva, size);
}

paddr_t
mvmebus_dmamem_mmap(bus_dma_tag_t t, bus_dma_segment_t *segs, int nsegs, off_t offset, int prot, int flags)
{
	struct mvmebus_softc *sc = t->_cookie;

	return bus_dmamem_mmap(sc->sc_dmat, segs, nsegs, offset, prot, flags);
}

#ifdef DEBUG
static const char *
mvmebus_mod_string(vme_addr_t addr, vme_size_t len, vme_am_t am, vme_datasize_t ds)
{
	static const char *mode[] = {"BLT64)", "DATA)", "PROG)", "BLT32)"};
	static const char *dsiz[] = {"(", "(D8,", "(D16,", "(D16-D8,",
	"(D32,", "(D32,D8,", "(D32-D16,", "(D32-D8,"};
	static const char *adrfmt[] = { "A32:%08x-%08x ", "USR:%08x-%08x ",
	    "A16:%04x-%04x ", "A24:%06x-%06x " };
	static char mstring[40];

	snprintf(mstring, sizeof(mstring),
	    adrfmt[(am & VME_AM_ADRSIZEMASK) >> VME_AM_ADRSIZESHIFT],
	    addr, addr + len - 1);
	strlcat(mstring, dsiz[ds & 0x7], sizeof(mstring));

	if (MVMEBUS_AM_HAS_CAP(am)) {
		if (am & MVMEBUS_AM_CAP_DATA)
			strlcat(mstring, "D", sizeof(mstring));
		if (am & MVMEBUS_AM_CAP_PROG)
			strlcat(mstring, "P", sizeof(mstring));
		if (am & MVMEBUS_AM_CAP_USER)
			strlcat(mstring, "U", sizeof(mstring));
		if (am & MVMEBUS_AM_CAP_SUPER)
			strlcat(mstring, "S", sizeof(mstring));
		if (am & MVMEBUS_AM_CAP_BLK)
			strlcat(mstring, "B", sizeof(mstring));
		if (am & MVMEBUS_AM_CAP_BLKD64)
			strlcat(mstring, "6", sizeof(mstring));
		strlcat(mstring, ")", sizeof(mstring));
	} else {
		strlcat(mstring, ((am & VME_AM_PRIVMASK) == VME_AM_USER) ?
		    "USER," : "SUPER,", sizeof(mstring));
		strlcat(mstring, mode[am & VME_AM_MODEMASK], sizeof(mstring));
	}

	return (mstring);
}
#endif

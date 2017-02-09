/*	$NetBSD: kdb.c,v 1.47 2010/01/16 18:43:34 dyoung Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed at Ludd, University of
 *	Lule}, Sweden and its contributors.
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

/*
 * KDB50 disk device driver
 */
/*
 * TODO
 *   Implement node reset routine.
 *   Nices hardware error handling.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kdb.c,v 1.47 2010/01/16 18:43:34 dyoung Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/sched.h>

#include <uvm/uvm_extern.h>

#ifdef __vax__
#include <machine/pte.h>
#include <machine/pcb.h>
#endif
#include <sys/bus.h>

#include <dev/bi/bireg.h>
#include <dev/bi/bivar.h>
#include <dev/bi/kdbreg.h>

#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>

#include "locators.h"

#define KDB_WL(adr, val) bus_space_write_4(sc->sc_iot, sc->sc_ioh, adr, val)
#define KDB_RL(adr) bus_space_read_4(sc->sc_iot, sc->sc_ioh, adr)
#define KDB_RS(adr) bus_space_read_2(sc->sc_iot, sc->sc_ioh, adr)

#define	    b_forw  b_hash.le_next
/*
 * Software status, per controller.
 */
struct	kdb_softc {
	device_t sc_dev;		/* Autoconfig info */
	struct evcnt sc_intrcnt;	/* Interrupt counting */
	void *sc_kdb;			/* Struct for kdb communication */
	struct mscp_softc *sc_softc;	/* MSCP info (per mscpvar.h) */
	bus_dma_tag_t sc_dmat;
	bus_dmamap_t sc_cmap;		/* Control structures */
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

int	kdbmatch(device_t, cfdata_t, void *);
void	kdbattach(device_t, device_t, void *);
void	kdbreset(int);
void	kdbintr(void *);
void	kdbctlrdone(device_t);
int	kdbprint(void *, const char *);
void	kdbsaerror(device_t, int);
void	kdbgo(device_t, struct mscp_xi *);

CFATTACH_DECL_NEW(kdb, sizeof(struct kdb_softc),
    kdbmatch, kdbattach, NULL, NULL);

/*
 * More driver definitions, for generic MSCP code.
 */
struct	mscp_ctlr kdb_mscp_ctlr = {
	kdbctlrdone,
	kdbgo,
	kdbsaerror,
};

int
kdbprint(void *aux, const char *name)
{
	if (name)
		aprint_normal("%s: mscpbus", name);
	return UNCONF;
}

/*
 * Poke at a supposed KDB to see if it is there.
 */
int
kdbmatch(device_t parent, cfdata_t cf, void *aux)
{
	struct bi_attach_args *ba = aux;

	if (bus_space_read_2(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE) != BIDT_KDB50)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

void
kdbattach(device_t parent, device_t self, void *aux)
{
	struct kdb_softc *sc = device_private(self);
	struct bi_attach_args *ba = aux;
	struct mscp_attach_args ma;
	volatile int i = 10000;
	int error, rseg;
	bus_dma_segment_t seg;

	sc->sc_dev = self;

	printf("\n");
	bi_intr_establish(ba->ba_icookie, ba->ba_ivec,
		kdbintr, sc, &sc->sc_intrcnt);
	evcnt_attach_dynamic(&sc->sc_intrcnt, EVCNT_TYPE_INTR, NULL,
		device_xname(sc->sc_dev), "intr");

	sc->sc_iot = ba->ba_iot;
	sc->sc_ioh = ba->ba_ioh;
	sc->sc_dmat = ba->ba_dmat;

	/*
	 * Map the communication area and command and
	 * response packets into Unibus space.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct mscp_pack),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("Alloc ctrl area %d\n", error);
		return;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    sizeof(struct mscp_pack), &sc->sc_kdb,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("Map ctrl area %d\n", error);
err:		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	if ((error = bus_dmamap_create(sc->sc_dmat, sizeof(struct mscp_pack),
	    1, sizeof(struct mscp_pack), 0, BUS_DMA_NOWAIT, &sc->sc_cmap))) {
		printf("Create DMA map %d\n", error);
err2:		bus_dmamem_unmap(sc->sc_dmat, sc->sc_kdb,
		    sizeof(struct mscp_pack));
		goto err;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_cmap,
	    sc->sc_kdb, sizeof(struct mscp_pack), 0, BUS_DMA_NOWAIT))) {
		printf("Load ctrl map %d\n", error);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_cmap);
		goto err2;
	}
	memset(sc->sc_kdb, 0, sizeof(struct mscp_pack));

	ma.ma_mc = &kdb_mscp_ctlr;
	ma.ma_type = MSCPBUS_DISK|MSCPBUS_KDB;
	ma.ma_uda = (struct mscp_pack *)sc->sc_kdb;
	ma.ma_softc = &sc->sc_softc;
	ma.ma_iot = sc->sc_iot;
	ma.ma_iph = sc->sc_ioh + KDB_IP;
	ma.ma_sah = sc->sc_ioh + KDB_SA;
	ma.ma_swh = sc->sc_ioh + KDB_SW;
	ma.ma_dmat = sc->sc_dmat;
	ma.ma_dmam = sc->sc_cmap;
	ma.ma_ivec = ba->ba_ivec;
	ma.ma_ctlrnr = ba->ba_nodenr;
	ma.ma_adapnr = ba->ba_busnr;

	KDB_WL(BIREG_VAXBICSR, KDB_RL(BIREG_VAXBICSR) | BICSR_NRST);
	while (i--) /* Need delay??? */
		;
	KDB_WL(BIREG_INTRDES, ba->ba_intcpu); /* Interrupt on CPU # */
	KDB_WL(BIREG_BCICSR, KDB_RL(BIREG_BCICSR) |
	    BCI_STOPEN | BCI_IDENTEN | BCI_UINTEN | BCI_INTEN);
	KDB_WL(BIREG_UINTRCSR, ba->ba_ivec);
	config_found(sc->sc_dev, &ma, kdbprint);
}

void
kdbgo(device_t usc, struct mscp_xi *mxi)
{
	struct kdb_softc *sc = device_private(usc);
	struct buf *bp = mxi->mxi_bp;
	struct mscp *mp = mxi->mxi_mp;
	u_int32_t addr = (u_int32_t)bp->b_data;
	u_int32_t mapaddr;
	int err;

	/*
	 * The KDB50 wants to read VAX Page tables directly, therefore
	 * the result from bus_dmamap_load() is uninteresting. (But it
	 * should never fail!).
	 *
	 * On VAX, point to the corresponding page tables. (user/sys)
	 * On other systems, do something else...
	 */
	err = bus_dmamap_load(sc->sc_dmat, mxi->mxi_dmam, bp->b_data,
	    bp->b_bcount, (bp->b_flags & B_PHYS ? bp->b_proc : 0),
	    BUS_DMA_NOWAIT);

	if (err) /* Shouldn't happen */
		panic("kdbgo: bus_dmamap_load: error %d", err);

#ifdef __vax__
	/*
	 * Get a pointer to the pte pointing out the first virtual address.
	 * Use different ways in kernel and user space.
	 */
	if ((bp->b_flags & B_PHYS) == 0) {
		mapaddr = ((u_int32_t)kvtopte(addr)) & ~KERNBASE;
	} else {

/* XXX: This code does not belong here! */
#define	UVTOPTE(addr, pmap) (((addr) < 0x40000000) ? \
    &(*pmap)->pm_p0br[PG_PFNUM(addr)] : &(*pmap)->pm_p1br[PG_PFNUM(addr)])

		pmap_t *pmap = &bp->b_proc->p_vmspace->vm_map.pmap;
		u_int32_t eaddr = addr + (bp->b_bcount - 1);
		u_int32_t emapaddr = (u_int32_t)UVTOPTE(eaddr, pmap);

		mapaddr = (u_int32_t)UVTOPTE(addr, pmap);
		if (trunc_page(mapaddr) != trunc_page(emapaddr)) {
			mp->mscp_seq.seq_bytecount =
			    (((round_page(mapaddr) - mapaddr)/4) * 512);
		}
		mapaddr = kvtophys(mapaddr);
	}
#else
#error Must write code to handle KDB50 on non-vax.
#endif

	mp->mscp_seq.seq_mapbase = mapaddr;
	mxi->mxi_dmam->dm_segs[0].ds_addr = (addr & 511) | KDB_MAP;
	mscp_dgo(sc->sc_softc, mxi);
}

void
kdbsaerror(device_t usc, int doreset)
{
	struct kdb_softc *sc = device_private(usc);

	if ((KDB_RS(KDB_SA) & MP_ERR) == 0)
		return;
	printf("%s: controller error, sa=0x%x\n", device_xname(sc->sc_dev),
	    KDB_RS(KDB_SA));
	/* What to do now??? */
}

/*
 * Interrupt routine.  Depending on the state of the controller,
 * continue initialisation, or acknowledge command and response
 * interrupts, and process responses.
 */
void
kdbintr(void *arg)
{
	struct kdb_softc *sc = arg;

	if (KDB_RS(KDB_SA) & MP_ERR) {	/* ctlr fatal error */
		kdbsaerror(sc->sc_dev, 1);
		return;
	}
	KERNEL_LOCK(1, NULL);
	mscp_intr(sc->sc_softc);
	KERNEL_UNLOCK_ONE(NULL);
}

void
kdbctlrdone(device_t usc)
{
}

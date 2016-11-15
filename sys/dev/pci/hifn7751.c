/*	$NetBSD: hifn7751.c,v 1.61 2015/05/14 07:27:14 maxv Exp $	*/
/*	$FreeBSD: hifn7751.c,v 1.5.2.7 2003/10/08 23:52:00 sam Exp $ */
/*	$OpenBSD: hifn7751.c,v 1.140 2003/08/01 17:55:54 deraadt Exp $	*/

/*
 * Invertex AEON / Hifn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 * Copyright (c) 1999 Theo de Raadt
 * Copyright (c) 2000-2001 Network Security Technologies, Inc.
 *			http://www.netsec.net
 * Copyright (c) 2003 Hifn Inc.
 *
 * This driver is based on a previous driver by Invertex, for which they
 * requested:  Please send any comments, feedback, bug-fixes, or feature
 * requests to software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

/*
 * Driver for various  Hifn pre-HIPP encryption processors.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hifn7751.c,v 1.61 2015/05/14 07:27:14 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/module.h>

#ifdef __OpenBSD__
#include <crypto/crypto.h>
#include <dev/rndvar.h>
#else
#include <opencrypto/cryptodev.h>
#include <sys/cprng.h>
#include <sys/rndpool.h>
#include <sys/rndsource.h>
#include <sys/sha1.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/hifn7751reg.h>
#include <dev/pci/hifn7751var.h>

#undef HIFN_DEBUG

#ifdef __NetBSD__
#define M_DUP_PKTHDR M_COPY_PKTHDR	/* XXX */
#endif

#ifdef HIFN_DEBUG
extern int hifn_debug;		/* patchable */
int hifn_debug = 1;
#endif

#ifdef __OpenBSD__
#define HAVE_CRYPTO_LZS		/* OpenBSD OCF supports CRYPTO_COMP_LZS */
#endif

/*
 * Prototypes and count for the pci_device structure
 */
#ifdef __OpenBSD__
static int hifn_probe((struct device *, void *, void *);
#else
static int hifn_probe(device_t, cfdata_t, void *);
#endif
static void hifn_attach(device_t, device_t, void *);
#ifdef __NetBSD__
static int hifn_detach(device_t, int);

CFATTACH_DECL_NEW(hifn, sizeof(struct hifn_softc),
    hifn_probe, hifn_attach, hifn_detach, NULL);
#else
CFATTACH_DECL_NEW(hifn, sizeof(struct hifn_softc),
    hifn_probe, hifn_attach, NULL, NULL);
#endif

#ifdef __OpenBSD__
struct cfdriver hifn_cd = {
	0, "hifn", DV_DULL
};
#endif

static void	hifn_reset_board(struct hifn_softc *, int);
static void	hifn_reset_puc(struct hifn_softc *);
static void	hifn_puc_wait(struct hifn_softc *);
static const char *hifn_enable_crypto(struct hifn_softc *, pcireg_t);
static void	hifn_set_retry(struct hifn_softc *);
static void	hifn_init_dma(struct hifn_softc *);
static void	hifn_init_pci_registers(struct hifn_softc *);
static int	hifn_sramsize(struct hifn_softc *);
static int	hifn_dramsize(struct hifn_softc *);
static int	hifn_ramtype(struct hifn_softc *);
static void	hifn_sessions(struct hifn_softc *);
static int	hifn_intr(void *);
static u_int	hifn_write_command(struct hifn_command *, u_int8_t *);
static u_int32_t hifn_next_signature(u_int32_t a, u_int cnt);
static int	hifn_newsession(void*, u_int32_t *, struct cryptoini *);
static int	hifn_freesession(void*, u_int64_t);
static int	hifn_process(void*, struct cryptop *, int);
static void	hifn_callback(struct hifn_softc *, struct hifn_command *,
			      u_int8_t *);
static int	hifn_crypto(struct hifn_softc *, struct hifn_command *,
			    struct cryptop*, int);
static int	hifn_readramaddr(struct hifn_softc *, int, u_int8_t *);
static int	hifn_writeramaddr(struct hifn_softc *, int, u_int8_t *);
static int	hifn_dmamap_aligned(bus_dmamap_t);
static int	hifn_dmamap_load_src(struct hifn_softc *,
				     struct hifn_command *);
static int	hifn_dmamap_load_dst(struct hifn_softc *,
				     struct hifn_command *);
static int	hifn_init_pubrng(struct hifn_softc *);
static void	hifn_rng(void *);
static void	hifn_rng_locked(void *);
static void	hifn_tick(void *);
static void	hifn_abort(struct hifn_softc *);
static void	hifn_alloc_slot(struct hifn_softc *, int *, int *, int *,
				int *);
static void	hifn_write_4(struct hifn_softc *, int, bus_size_t, u_int32_t);
static u_int32_t hifn_read_4(struct hifn_softc *, int, bus_size_t);
#ifdef	HAVE_CRYPTO_LZS
static int	hifn_compression(struct hifn_softc *, struct cryptop *,
				 struct hifn_command *);
static struct mbuf *hifn_mkmbuf_chain(int, struct mbuf *);
static int	hifn_compress_enter(struct hifn_softc *, struct hifn_command *);
static void	hifn_callback_comp(struct hifn_softc *, struct hifn_command *,
				   u_int8_t *);
#endif	/* HAVE_CRYPTO_LZS */

struct hifn_stats hifnstats;

static const struct hifn_product {
	pci_vendor_id_t		hifn_vendor;
	pci_product_id_t	hifn_product;
	int			hifn_flags;
	const char		*hifn_name;
} hifn_products[] = {
	{ PCI_VENDOR_INVERTEX,	PCI_PRODUCT_INVERTEX_AEON,
	  0,
	  "Invertex AEON",
	},

	{ PCI_VENDOR_HIFN,	PCI_PRODUCT_HIFN_7751,
	  0,
	  "Hifn 7751",
	},
	{ PCI_VENDOR_NETSEC,	PCI_PRODUCT_NETSEC_7751,
	  0,
	  "Hifn 7751 (NetSec)"
	},

	{ PCI_VENDOR_HIFN,	PCI_PRODUCT_HIFN_7811,
	  HIFN_IS_7811 | HIFN_HAS_RNG | HIFN_HAS_LEDS | HIFN_NO_BURSTWRITE,
	  "Hifn 7811",
	},

	{ PCI_VENDOR_HIFN,	PCI_PRODUCT_HIFN_7951,
	  HIFN_HAS_RNG | HIFN_HAS_PUBLIC,
	  "Hifn 7951",
	},

	{ PCI_VENDOR_HIFN,	PCI_PRODUCT_HIFN_7955,
	  HIFN_HAS_RNG | HIFN_HAS_PUBLIC | HIFN_IS_7956 | HIFN_HAS_AES,
	  "Hifn 7955",
	},

	{ PCI_VENDOR_HIFN,	PCI_PRODUCT_HIFN_7956,
	  HIFN_HAS_RNG | HIFN_HAS_PUBLIC | HIFN_IS_7956 | HIFN_HAS_AES,
	  "Hifn 7956",
	},


	{ 0,			0,
	  0,
	  NULL
	}
};

static const struct hifn_product *
hifn_lookup(const struct pci_attach_args *pa)
{
	const struct hifn_product *hp;

	for (hp = hifn_products; hp->hifn_name != NULL; hp++) {
		if (PCI_VENDOR(pa->pa_id) == hp->hifn_vendor &&
		    PCI_PRODUCT(pa->pa_id) == hp->hifn_product)
			return (hp);
	}
	return (NULL);
}

static int
hifn_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (hifn_lookup(pa) != NULL)
		return 1;

	return 0;
}

static void
hifn_attach(device_t parent, device_t self, void *aux)
{
	struct hifn_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	const struct hifn_product *hp;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const char *hifncap;
	char rbase;
#ifdef __NetBSD__
#define iosize0 sc->sc_iosz0
#define iosize1 sc->sc_iosz1
#else
	bus_size_t iosize0, iosize1;
#endif
	u_int32_t cmd;
	u_int16_t ena;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	int rseg;
	void *kva;
	char intrbuf[PCI_INTRSTR_LEN];

	hp = hifn_lookup(pa);
	if (hp == NULL) {
		printf("\n");
		panic("hifn_attach: impossible");
	}

	pci_aprint_devinfo_fancy(pa, "Crypto processor", hp->hifn_name, 1);

	sc->sc_dv = self;
	sc->sc_pci_pc = pa->pa_pc;
	sc->sc_pci_tag = pa->pa_tag;

	sc->sc_flags = hp->hifn_flags;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);

	if (pci_mapreg_map(pa, HIFN_BAR0, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st0, &sc->sc_sh0, NULL, &iosize0)) {
		aprint_error_dev(sc->sc_dv, "can't map mem space %d\n", 0);
		return;
	}

	if (pci_mapreg_map(pa, HIFN_BAR1, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st1, &sc->sc_sh1, NULL, &iosize1)) {
		aprint_error_dev(sc->sc_dv, "can't find mem space %d\n", 1);
		goto fail_io0;
	}

	hifn_set_retry(sc);

	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		sc->sc_waw_lastgroup = -1;
		sc->sc_waw_lastreg = 1;
	}

	sc->sc_dmat = pa->pa_dmat;
	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(*sc->sc_dma), PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dv, "can't alloc DMA buffer\n");
		goto fail_io1;
        }
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, sizeof(*sc->sc_dma), &kva,
	    BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dv, "can't map DMA buffers (%lu bytes)\n",
		    (u_long)sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		goto fail_io1;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(*sc->sc_dma), 1,
	    sizeof(*sc->sc_dma), 0, BUS_DMA_NOWAIT, &dmamap)) {
		aprint_error_dev(sc->sc_dv, "can't create DMA map\n");
		bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		goto fail_io1;
	}
	if (bus_dmamap_load(sc->sc_dmat, dmamap, kva, sizeof(*sc->sc_dma),
	    NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sc_dv, "can't load DMA map\n");
		bus_dmamap_destroy(sc->sc_dmat, dmamap);
		bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		goto fail_io1;
	}
	sc->sc_dmamap = dmamap;
	sc->sc_dma = (struct hifn_dma *)kva;
	memset(sc->sc_dma, 0, sizeof(*sc->sc_dma));

	hifn_reset_board(sc, 0);

	if ((hifncap = hifn_enable_crypto(sc, pa->pa_id)) == NULL) {
		aprint_error_dev(sc->sc_dv, "crypto enabling failed\n");
		goto fail_mem;
	}
	hifn_reset_puc(sc);

	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);

	/* XXX can't dynamically determine ram type for 795x; force dram */
	if (sc->sc_flags & HIFN_IS_7956)
		sc->sc_drammodel = 1;
	else if (hifn_ramtype(sc))
		goto fail_mem;

	if (sc->sc_drammodel == 0)
		hifn_sramsize(sc);
	else
		hifn_dramsize(sc);

	/*
	 * Workaround for NetSec 7751 rev A: half ram size because two
	 * of the address lines were left floating
	 */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NETSEC &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NETSEC_7751 &&
	    PCI_REVISION(pa->pa_class) == 0x61)
		sc->sc_ramsize >>= 1;

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dv, "couldn't map interrupt\n");
		goto fail_mem;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
#ifdef	__OpenBSD__
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, hifn_intr, sc,
	    device_xname(self));
#else
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, hifn_intr, sc);
#endif
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dv, "couldn't establish interrupt\n");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail_mem;
	}

	hifn_sessions(sc);

	rseg = sc->sc_ramsize / 1024;
	rbase = 'K';
	if (sc->sc_ramsize >= (1024 * 1024)) {
		rbase = 'M';
		rseg /= 1024;
	}
	aprint_normal_dev(sc->sc_dv, "%s, %d%cB %cRAM, interrupting at %s\n",
	    hifncap, rseg, rbase,
	    sc->sc_drammodel ? 'D' : 'S', intrstr);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		aprint_error_dev(sc->sc_dv, "couldn't get crypto driver id\n");
		goto fail_intr;
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG,
	    READ_REG_0(sc, HIFN_0_PUCNFG) | HIFN_PUCNFG_CHIPID);
	ena = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	switch (ena) {
	case HIFN_PUSTAT_ENA_2:
		crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_ARC4, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		if (sc->sc_flags & HIFN_HAS_AES)
			crypto_register(sc->sc_cid, CRYPTO_AES_CBC,  0, 0,
				hifn_newsession, hifn_freesession,
				hifn_process, sc);
		/*FALLTHROUGH*/
	case HIFN_PUSTAT_ENA_1:
		crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC_96, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC_96, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0,
		    hifn_newsession, hifn_freesession, hifn_process, sc);
		break;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap, 0,
	    sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);

	if (sc->sc_flags & (HIFN_HAS_PUBLIC | HIFN_HAS_RNG)) {
		hifn_init_pubrng(sc);
		sc->sc_rng_need = RND_POOLBITS / NBBY;
	}

#ifdef	__OpenBSD__
	timeout_set(&sc->sc_tickto, hifn_tick, sc);
	timeout_add(&sc->sc_tickto, hz);
#else
	callout_init(&sc->sc_tickto, CALLOUT_MPSAFE);
	callout_reset(&sc->sc_tickto, hz, hifn_tick, sc);
#endif
	return;

fail_intr:
	pci_intr_disestablish(pc, sc->sc_ih);
fail_mem:
	bus_dmamap_unload(sc->sc_dmat, dmamap);
	bus_dmamap_destroy(sc->sc_dmat, dmamap);
	bus_dmamem_unmap(sc->sc_dmat, kva, sizeof(*sc->sc_dma));
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);

	/* Turn off DMA polling */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

fail_io1:
	bus_space_unmap(sc->sc_st1, sc->sc_sh1, iosize1);
fail_io0:
	bus_space_unmap(sc->sc_st0, sc->sc_sh0, iosize0);
}

#ifdef __NetBSD__
static int
hifn_detach(device_t self, int flags)
{
	struct hifn_softc *sc = device_private(self);

	hifn_abort(sc);

	hifn_reset_board(sc, 1);

	pci_intr_disestablish(sc->sc_pci_pc, sc->sc_ih);

	crypto_unregister_all(sc->sc_cid);

	rnd_detach_source(&sc->sc_rnd_source);

	mutex_enter(&sc->sc_mtx);
	callout_halt(&sc->sc_tickto, NULL);
	if (sc->sc_flags & (HIFN_HAS_PUBLIC | HIFN_HAS_RNG))
		callout_halt(&sc->sc_rngto, NULL);
	mutex_exit(&sc->sc_mtx);

	bus_space_unmap(sc->sc_st1, sc->sc_sh1, sc->sc_iosz1);
	bus_space_unmap(sc->sc_st0, sc->sc_sh0, sc->sc_iosz0);

	/*
	 * XXX It's not clear if any additional buffers have been
	 * XXX allocated and require free()ing
	 */

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, hifn, "pci,opencrypto");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
hifn_modcmd(modcmd_t cmd, void *data)
{
	int error = 0;

	switch(cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_hifn,
		    cfattach_ioconf_hifn, cfdata_ioconf_hifn);
#endif
		return error;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_hifn,
		    cfattach_ioconf_hifn, cfdata_ioconf_hifn);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

#endif /* ifdef __NetBSD__ */

static void
hifn_rng_get(size_t bytes, void *priv)
{
	struct hifn_softc *sc = priv;

	mutex_enter(&sc->sc_mtx);
	sc->sc_rng_need = bytes;
	callout_reset(&sc->sc_rngto, 0, hifn_rng, sc);
	mutex_exit(&sc->sc_mtx);
}

static int
hifn_init_pubrng(struct hifn_softc *sc)
{
	u_int32_t r;
	int i;

	if ((sc->sc_flags & HIFN_IS_7811) == 0) {
		/* Reset 7951 public key/rng engine */
		WRITE_REG_1(sc, HIFN_1_PUB_RESET,
		    READ_REG_1(sc, HIFN_1_PUB_RESET) | HIFN_PUBRST_RESET);

		for (i = 0; i < 100; i++) {
			DELAY(1000);
			if ((READ_REG_1(sc, HIFN_1_PUB_RESET) &
			    HIFN_PUBRST_RESET) == 0)
				break;
		}

		if (i == 100) {
			printf("%s: public key init failed\n",
			    device_xname(sc->sc_dv));
			return (1);
		}
	}

	/* Enable the rng, if available */
	if (sc->sc_flags & HIFN_HAS_RNG) {
		if (sc->sc_flags & HIFN_IS_7811) {
			r = READ_REG_1(sc, HIFN_1_7811_RNGENA);
			if (r & HIFN_7811_RNGENA_ENA) {
				r &= ~HIFN_7811_RNGENA_ENA;
				WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
			}
			WRITE_REG_1(sc, HIFN_1_7811_RNGCFG,
			    HIFN_7811_RNGCFG_DEFL);
			r |= HIFN_7811_RNGENA_ENA;
			WRITE_REG_1(sc, HIFN_1_7811_RNGENA, r);
		} else
			WRITE_REG_1(sc, HIFN_1_RNG_CONFIG,
			    READ_REG_1(sc, HIFN_1_RNG_CONFIG) |
			    HIFN_RNGCFG_ENA);

		/*
		 * The Hifn RNG documentation states that at their
		 * recommended "conservative" RNG config values,
		 * the RNG must warm up for 0.4s before providing
		 * data that meet their worst-case estimate of 0.06
		 * bits of random data per output register bit.
		 */
		DELAY(4000);

#ifdef __NetBSD__
		rndsource_setcb(&sc->sc_rnd_source, hifn_rng_get, sc);
		rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dv),
				  RND_TYPE_RNG,
				  RND_FLAG_COLLECT_VALUE|RND_FLAG_HASCB);
#endif

		if (hz >= 100)
			sc->sc_rnghz = hz / 100;
		else
			sc->sc_rnghz = 1;
#ifdef	__OpenBSD__
		timeout_set(&sc->sc_rngto, hifn_rng, sc);
#else	/* !__OpenBSD__ */
		callout_init(&sc->sc_rngto, CALLOUT_MPSAFE);
#endif	/* !__OpenBSD__ */
	}

	/* Enable public key engine, if available */
	if (sc->sc_flags & HIFN_HAS_PUBLIC) {
		WRITE_REG_1(sc, HIFN_1_PUB_IEN, HIFN_PUBIEN_DONE);
		sc->sc_dmaier |= HIFN_DMAIER_PUBDONE;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	/* Call directly into the RNG once to prime the pool. */
	hifn_rng(sc);   /* Sets callout/timeout at end */

	return (0);
}

static void
hifn_rng_locked(void *vsc)
{
	struct hifn_softc *sc = vsc;
#ifdef __NetBSD__
	uint32_t num[64];
#else
	uint32_t num[2];
#endif
	uint32_t sts;
	int i;
	size_t got, gotent;

	if (sc->sc_rng_need < 1) {
		callout_stop(&sc->sc_rngto);
		return;
	}

	if (sc->sc_flags & HIFN_IS_7811) {
		for (i = 0; i < 5; i++) {	/* XXX why 5? */
			sts = READ_REG_1(sc, HIFN_1_7811_RNGSTS);
			if (sts & HIFN_7811_RNGSTS_UFL) {
				printf("%s: RNG underflow: disabling\n",
				    device_xname(sc->sc_dv));
				return;
			}
			if ((sts & HIFN_7811_RNGSTS_RDY) == 0)
				break;

			/*
			 * There are at least two words in the RNG FIFO
			 * at this point.
			 */
			num[0] = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			num[1] = READ_REG_1(sc, HIFN_1_7811_RNGDAT);
			got = 2 * sizeof(num[0]);
			gotent = (got * NBBY) / HIFN_RNG_BITSPER;

#ifdef __NetBSD__
			rnd_add_data(&sc->sc_rnd_source, num, got, gotent);
			sc->sc_rng_need -= gotent;
#else
			/*
			 * XXX This is a really bad idea.
			 * XXX Hifn estimate as little as 0.06
			 * XXX actual bits of entropy per output
			 * XXX register bit.  How can we tell the
			 * XXX kernel RNG subsystem we're handing
			 * XXX it 64 "true" random bits, for any
			 * XXX sane value of "true"?
			 * XXX
			 * XXX The right thing to do here, if we
			 * XXX cannot supply an estimate ourselves,
			 * XXX would be to hash the bits locally.
			 */
			add_true_randomness(num[0]);
			add_true_randomness(num[1]);
#endif
				
		}
	} else {
		int nwords = 0;

		if (sc->sc_rng_need) {
			nwords = (sc->sc_rng_need * NBBY) / HIFN_RNG_BITSPER;
			nwords = MIN(__arraycount(num), nwords);
		}

		if (nwords < 2) {
			nwords = 2;
		}

		/*
		 * We must be *extremely* careful here.  The Hifn
		 * 795x differ from the published 6500 RNG design
		 * in more ways than the obvious lack of the output
		 * FIFO and LFSR control registers.  In fact, there
		 * is only one LFSR, instead of the 6500's two, and
		 * it's 32 bits, not 31.
		 *
		 * Further, a block diagram obtained from Hifn shows
		 * a very curious latching of this register: the LFSR
		 * rotates at a frequency of RNG_Clk / 8, but the
		 * RNG_Data register is latched at a frequency of
		 * RNG_Clk, which means that it is possible for
		 * consecutive reads of the RNG_Data register to read
		 * identical state from the LFSR.  The simplest
		 * workaround seems to be to read eight samples from
		 * the register for each one that we use.  Since each
		 * read must require at least one PCI cycle, and
		 * RNG_Clk is at least PCI_Clk, this is safe.
		 */
		for(i = 0 ; i < nwords * 8; i++)
		{
			volatile u_int32_t regtmp;
			regtmp = READ_REG_1(sc, HIFN_1_RNG_DATA);
			num[i / 8] = regtmp;
		}

		got = nwords * sizeof(num[0]);
		gotent = (got * NBBY) / HIFN_RNG_BITSPER;
#ifdef __NetBSD__
		rnd_add_data(&sc->sc_rnd_source, num, got, gotent);
		sc->sc_rng_need -= gotent;
#else
		/* XXX a bad idea; see 7811 block above */
		add_true_randomness(num[0]);
#endif
	}

#ifdef	__OpenBSD__
	timeout_add(&sc->sc_rngto, sc->sc_rnghz);
#else
	if (sc->sc_rng_need > 0) {
		callout_reset(&sc->sc_rngto, sc->sc_rnghz, hifn_rng, sc);
	}
#endif
}

static void
hifn_rng(void *vsc)
{
	struct hifn_softc *sc = vsc;

	mutex_spin_enter(&sc->sc_mtx);
	hifn_rng_locked(vsc);
	mutex_spin_exit(&sc->sc_mtx);
}

static void
hifn_puc_wait(struct hifn_softc *sc)
{
	int i;

	for (i = 5000; i > 0; i--) {
		DELAY(1);
		if (!(READ_REG_0(sc, HIFN_0_PUCTRL) & HIFN_PUCTRL_RESET))
			break;
	}
	if (!i)
		printf("%s: proc unit did not reset\n", device_xname(sc->sc_dv));
}

/*
 * Reset the processing unit.
 */
static void
hifn_reset_puc(struct hifn_softc *sc)
{
	/* Reset processing unit */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	hifn_puc_wait(sc);
}

static void
hifn_set_retry(struct hifn_softc *sc)
{
	u_int32_t r;

	r = pci_conf_read(sc->sc_pci_pc, sc->sc_pci_tag, HIFN_TRDY_TIMEOUT);
	r &= 0xffff0000;
	pci_conf_write(sc->sc_pci_pc, sc->sc_pci_tag, HIFN_TRDY_TIMEOUT, r);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
static void
hifn_reset_board(struct hifn_softc *sc, int full)
{
	u_int32_t reg;

	/*
	 * Set polling in the DMA configuration register to zero.  0x7 avoids
	 * resetting the board and zeros out the other fields.
	 */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	/*
	 * Now that polling has been disabled, we have to wait 1 ms
	 * before resetting the board.
	 */
	DELAY(1000);

	/* Reset the DMA unit */
	if (full) {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MODE);
		DELAY(1000);
	} else {
		WRITE_REG_1(sc, HIFN_1_DMA_CNFG,
		    HIFN_DMACNFG_MODE | HIFN_DMACNFG_MSTRESET);
		hifn_reset_puc(sc);
	}

	memset(sc->sc_dma, 0, sizeof(*sc->sc_dma));

	/* Bring dma unit out of reset */
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);

	hifn_puc_wait(sc);

	hifn_set_retry(sc);

	if (sc->sc_flags & HIFN_IS_7811) {
		for (reg = 0; reg < 1000; reg++) {
			if (READ_REG_1(sc, HIFN_1_7811_MIPSRST) &
			    HIFN_MIPSRST_CRAMINIT)
				break;
			DELAY(1000);
		}
		if (reg == 1000)
			printf(": cram init timeout\n");
	}
}

static u_int32_t
hifn_next_signature(u_int32_t a, u_int cnt)
{
	int i;
	u_int32_t v;

	for (i = 0; i < cnt; i++) {

		/* get the parity */
		v = a & 0x80080125;
		v ^= v >> 16;
		v ^= v >> 8;
		v ^= v >> 4;
		v ^= v >> 2;
		v ^= v >> 1;

		a = (v & 1) ^ (a << 1);
	}

	return a;
}

static struct pci2id {
	u_short		pci_vendor;
	u_short		pci_prod;
	char		card_id[13];
} const pci2id[] = {
	{
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7951,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7955,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7956,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_NETSEC,
		PCI_PRODUCT_NETSEC_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_INVERTEX,
		PCI_PRODUCT_INVERTEX_AEON,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7811,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	}, {
		/*
		 * Other vendors share this PCI ID as well, such as
		 * http://www.powercrypt.com, and obviously they also
		 * use the same key.
		 */
		PCI_VENDOR_HIFN,
		PCI_PRODUCT_HIFN_7751,
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00 }
	},
};

/*
 * Checks to see if crypto is already enabled.  If crypto isn't enable,
 * "hifn_enable_crypto" is called to enable it.  The check is important,
 * as enabling crypto twice will lock the board.
 */
static const char *
hifn_enable_crypto(struct hifn_softc *sc, pcireg_t pciid)
{
	u_int32_t dmacfg, ramcfg, encl, addr, i;
	const char *offtbl = NULL;

	for (i = 0; i < sizeof(pci2id)/sizeof(pci2id[0]); i++) {
		if (pci2id[i].pci_vendor == PCI_VENDOR(pciid) &&
		    pci2id[i].pci_prod == PCI_PRODUCT(pciid)) {
			offtbl = pci2id[i].card_id;
			break;
		}
	}

	if (offtbl == NULL) {
#ifdef HIFN_DEBUG
		aprint_debug_dev(sc->sc_dv, "Unknown card!\n");
#endif
		return (NULL);
	}

	ramcfg = READ_REG_0(sc, HIFN_0_PUCNFG);
	dmacfg = READ_REG_1(sc, HIFN_1_DMA_CNFG);

	/*
	 * The RAM config register's encrypt level bit needs to be set before
	 * every read performed on the encryption level register.
	 */
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);

	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

	/*
	 * Make sure we don't re-unlock.  Two unlocks kills chip until the
	 * next reboot.
	 */
	if (encl == HIFN_PUSTAT_ENA_1 || encl == HIFN_PUSTAT_ENA_2) {
#ifdef HIFN_DEBUG
		aprint_debug_dev(sc->sc_dv, "Strong Crypto already enabled!\n");
#endif
		goto report;
	}

	if (encl != 0 && encl != HIFN_PUSTAT_ENA_0) {
#ifdef HIFN_DEBUG
		aprint_debug_dev(sc->sc_dv, "Unknown encryption level\n");
#endif
		return (NULL);
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_UNLOCK |
	    HIFN_DMACNFG_MSTRESET | HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE);
	DELAY(1000);
	addr = READ_REG_1(sc, HIFN_1_UNLOCK_SECRET1);
	DELAY(1000);
	WRITE_REG_1(sc, HIFN_1_UNLOCK_SECRET2, 0);
	DELAY(1000);

	for (i = 0; i <= 12; i++) {
		addr = hifn_next_signature(addr, offtbl[i] + 0x101);
		WRITE_REG_1(sc, HIFN_1_UNLOCK_SECRET2, addr);

		DELAY(1000);
	}

	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg | HIFN_PUCNFG_CHIPID);
	encl = READ_REG_0(sc, HIFN_0_PUSTAT) & HIFN_PUSTAT_CHIPENA;

#ifdef HIFN_DEBUG
	if (encl != HIFN_PUSTAT_ENA_1 && encl != HIFN_PUSTAT_ENA_2)
		aprint_debug("Encryption engine is permanently locked until next system reset.");
	else
		aprint_debug("Encryption engine enabled successfully!");
#endif

report:
	WRITE_REG_0(sc, HIFN_0_PUCNFG, ramcfg);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, dmacfg);

	switch (encl) {
	case HIFN_PUSTAT_ENA_0:
		return ("LZS-only (no encr/auth)");

	case HIFN_PUSTAT_ENA_1:
		return ("DES");

	case HIFN_PUSTAT_ENA_2:
		if (sc->sc_flags & HIFN_HAS_AES)
		    return ("3DES/AES");
		else
		    return ("3DES");

	default:
		return ("disabled");
	}
	/* NOTREACHED */
}

/*
 * Give initial values to the registers listed in the "Register Space"
 * section of the HIFN Software Development reference manual.
 */
static void
hifn_init_pci_registers(struct hifn_softc *sc)
{
	/* write fixed values needed by the Initialization registers */
	WRITE_REG_0(sc, HIFN_0_PUCTRL, HIFN_PUCTRL_DMAENA);
	WRITE_REG_0(sc, HIFN_0_FIFOCNFG, HIFN_FIFOCNFG_THRESHOLD);
	WRITE_REG_0(sc, HIFN_0_PUIER, HIFN_PUIER_DSTOVER);

	/* write all 4 ring address registers */
	WRITE_REG_1(sc, HIFN_1_DMA_CRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, cmdr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_SRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, srcr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_DRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, dstr[0]));
	WRITE_REG_1(sc, HIFN_1_DMA_RRAR, sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, resr[0]));

	DELAY(2000);

	/* write status register */
	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS |
	    HIFN_DMACSR_S_CTRL_DIS | HIFN_DMACSR_C_CTRL_DIS |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_D_DONE | HIFN_DMACSR_D_LAST |
	    HIFN_DMACSR_D_WAIT | HIFN_DMACSR_D_OVER |
	    HIFN_DMACSR_R_ABORT | HIFN_DMACSR_R_DONE | HIFN_DMACSR_R_LAST |
	    HIFN_DMACSR_R_WAIT | HIFN_DMACSR_R_OVER |
	    HIFN_DMACSR_S_ABORT | HIFN_DMACSR_S_DONE | HIFN_DMACSR_S_LAST |
	    HIFN_DMACSR_S_WAIT |
	    HIFN_DMACSR_C_ABORT | HIFN_DMACSR_C_DONE | HIFN_DMACSR_C_LAST |
	    HIFN_DMACSR_C_WAIT |
	    HIFN_DMACSR_ENGINE |
	    ((sc->sc_flags & HIFN_HAS_PUBLIC) ?
		HIFN_DMACSR_PUBDONE : 0) |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMACSR_ILLW | HIFN_DMACSR_ILLR : 0));

	sc->sc_d_busy = sc->sc_r_busy = sc->sc_s_busy = sc->sc_c_busy = 0;
	sc->sc_dmaier |= HIFN_DMAIER_R_DONE | HIFN_DMAIER_C_ABORT |
	    HIFN_DMAIER_D_OVER | HIFN_DMAIER_R_OVER |
	    HIFN_DMAIER_S_ABORT | HIFN_DMAIER_D_ABORT | HIFN_DMAIER_R_ABORT |
	    HIFN_DMAIER_ENGINE |
	    ((sc->sc_flags & HIFN_IS_7811) ?
		HIFN_DMAIER_ILLW | HIFN_DMAIER_ILLR : 0);
	sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
	WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	CLR_LED(sc, HIFN_MIPSRST_LED0 | HIFN_MIPSRST_LED1 | HIFN_MIPSRST_LED2);

	if (sc->sc_flags & HIFN_IS_7956) {
		WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
		    HIFN_PUCNFG_TCALLPHASES |
		    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32);
		WRITE_REG_1(sc, HIFN_1_PLL, HIFN_PLL_7956);
	} else {
		WRITE_REG_0(sc, HIFN_0_PUCNFG, HIFN_PUCNFG_COMPSING |
		    HIFN_PUCNFG_DRFR_128 | HIFN_PUCNFG_TCALLPHASES |
		    HIFN_PUCNFG_TCDRVTOTEM | HIFN_PUCNFG_BUS32 |
		    (sc->sc_drammodel ? HIFN_PUCNFG_DRAM : HIFN_PUCNFG_SRAM));
	}

	WRITE_REG_0(sc, HIFN_0_PUISR, HIFN_PUISR_DSTOVER);
	WRITE_REG_1(sc, HIFN_1_DMA_CNFG, HIFN_DMACNFG_MSTRESET |
	    HIFN_DMACNFG_DMARESET | HIFN_DMACNFG_MODE | HIFN_DMACNFG_LAST |
	    ((HIFN_POLL_FREQUENCY << 16 ) & HIFN_DMACNFG_POLLFREQ) |
	    ((HIFN_POLL_SCALAR << 8) & HIFN_DMACNFG_POLLINVAL));
}

/*
 * The maximum number of sessions supported by the card
 * is dependent on the amount of context ram, which
 * encryption algorithms are enabled, and how compression
 * is configured.  This should be configured before this
 * routine is called.
 */
static void
hifn_sessions(struct hifn_softc *sc)
{
	u_int32_t pucnfg;
	int ctxsize;

	pucnfg = READ_REG_0(sc, HIFN_0_PUCNFG);

	if (pucnfg & HIFN_PUCNFG_COMPSING) {
		if (pucnfg & HIFN_PUCNFG_ENCCNFG)
			ctxsize = 128;
		else
			ctxsize = 512;
		/*
		 * 7955/7956 has internal context memory of 32K
		 */
		if (sc->sc_flags & HIFN_IS_7956)
			sc->sc_maxses = 32768 / ctxsize;
		else
			sc->sc_maxses = 1 +
			    ((sc->sc_ramsize - 32768) / ctxsize);
	}
	else
		sc->sc_maxses = sc->sc_ramsize / 16384;

	if (sc->sc_maxses > 2048)
		sc->sc_maxses = 2048;
}

/*
 * Determine ram type (sram or dram).  Board should be just out of a reset
 * state when this is called.
 */
static int
hifn_ramtype(struct hifn_softc *sc)
{
	u_int8_t data[8], dataexpect[8];
	int i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0x55;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (memcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = 0xaa;
	if (hifn_writeramaddr(sc, 0, data))
		return (-1);
	if (hifn_readramaddr(sc, 0, data))
		return (-1);
	if (memcmp(data, dataexpect, sizeof(data)) != 0) {
		sc->sc_drammodel = 1;
		return (0);
	}

	return (0);
}

#define	HIFN_SRAM_MAX		(32 << 20)
#define	HIFN_SRAM_STEP_SIZE	16384
#define	HIFN_SRAM_GRANULARITY	(HIFN_SRAM_MAX / HIFN_SRAM_STEP_SIZE)

static int
hifn_sramsize(struct hifn_softc *sc)
{
	u_int32_t a;
	u_int8_t data[8];
	u_int8_t dataexpect[sizeof(data)];
	int32_t i;

	for (i = 0; i < sizeof(data); i++)
		data[i] = dataexpect[i] = i ^ 0x5a;

	for (i = HIFN_SRAM_GRANULARITY - 1; i >= 0; i--) {
		a = i * HIFN_SRAM_STEP_SIZE;
		memcpy(data, &i, sizeof(i));
		hifn_writeramaddr(sc, a, data);
	}

	for (i = 0; i < HIFN_SRAM_GRANULARITY; i++) {
		a = i * HIFN_SRAM_STEP_SIZE;
		memcpy(dataexpect, &i, sizeof(i));
		if (hifn_readramaddr(sc, a, data) < 0)
			return (0);
		if (memcmp(data, dataexpect, sizeof(data)) != 0)
			return (0);
		sc->sc_ramsize = a + HIFN_SRAM_STEP_SIZE;
	}

	return (0);
}

/*
 * XXX For dram boards, one should really try all of the
 * HIFN_PUCNFG_DSZ_*'s.  This just assumes that PUCNFG
 * is already set up correctly.
 */
static int
hifn_dramsize(struct hifn_softc *sc)
{
	u_int32_t cnfg;

	if (sc->sc_flags & HIFN_IS_7956) {
		/*
		 * 7955/7956 have a fixed internal ram of only 32K.
		 */
		sc->sc_ramsize = 32768;
	} else {
		cnfg = READ_REG_0(sc, HIFN_0_PUCNFG) &
		    HIFN_PUCNFG_DRAMMASK;
		sc->sc_ramsize = 1 << ((cnfg >> 13) + 18);
	}
	return (0);
}

static void
hifn_alloc_slot(struct hifn_softc *sc, int *cmdp, int *srcp, int *dstp,
    int *resp)
{
	struct hifn_dma *dma = sc->sc_dma;

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*cmdp = dma->cmdi++;
	dma->cmdk = dma->cmdi;

	if (dma->srci == HIFN_D_SRC_RSIZE) {
		dma->srci = 0;
		dma->srcr[HIFN_D_SRC_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*srcp = dma->srci++;
	dma->srck = dma->srci;

	if (dma->dsti == HIFN_D_DST_RSIZE) {
		dma->dsti = 0;
		dma->dstr[HIFN_D_DST_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, HIFN_D_DST_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*dstp = dma->dsti++;
	dma->dstk = dma->dsti;

	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	*resp = dma->resi++;
	dma->resk = dma->resi;
}

static int
hifn_writeramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_base_command wc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, resi, srci, dsti;

	wc.masks = htole16(3 << 13);
	wc.session_num = htole16(addr >> 14);
	wc.total_source_count = htole16(8);
	wc.total_dest_count = htole16(addr & 0x3fff);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	/* build write command */
	memset(dma->command_bufs[cmdi], 0, HIFN_MAX_COMMAND);
	*(struct hifn_base_command *)dma->command_bufs[cmdi] = wc;
	memcpy(&dma->test_src, data, sizeof(dma->test_src));

	dma->srcr[srci].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr
	    + offsetof(struct hifn_dma, test_src));
	dma->dstr[dsti].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr
	    + offsetof(struct hifn_dma, test_dst));

	dma->cmdr[cmdi].l = htole32(16 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(4 | masks);
	dma->resr[resi].l = htole32(4 | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    0, sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		printf("%s: writeramaddr -- "
		    "result[%d](addr %d) still valid\n",
		    device_xname(sc->sc_dv), resi, addr);
		r = -1;
		return (-1);
	} else
		r = 0;

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

static int
hifn_readramaddr(struct hifn_softc *sc, int addr, u_int8_t *data)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_base_command rc;
	const u_int32_t masks = HIFN_D_VALID | HIFN_D_LAST | HIFN_D_MASKDONEIRQ;
	int r, cmdi, srci, dsti, resi;

	rc.masks = htole16(2 << 13);
	rc.session_num = htole16(addr >> 14);
	rc.total_source_count = htole16(addr & 0x3fff);
	rc.total_dest_count = htole16(8);

	hifn_alloc_slot(sc, &cmdi, &srci, &dsti, &resi);

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_ENA | HIFN_DMACSR_S_CTRL_ENA |
	    HIFN_DMACSR_D_CTRL_ENA | HIFN_DMACSR_R_CTRL_ENA);

	memset(dma->command_bufs[cmdi], 0, HIFN_MAX_COMMAND);
	*(struct hifn_base_command *)dma->command_bufs[cmdi] = rc;

	dma->srcr[srci].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, test_src));
	dma->test_src = 0;
	dma->dstr[dsti].p =  htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
	    offsetof(struct hifn_dma, test_dst));
	dma->test_dst = 0;
	dma->cmdr[cmdi].l = htole32(8 | masks);
	dma->srcr[srci].l = htole32(8 | masks);
	dma->dstr[dsti].l = htole32(8 | masks);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT | masks);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
	    0, sc->sc_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (r = 10000; r >= 0; r--) {
		DELAY(10);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if ((dma->resr[resi].l & htole32(HIFN_D_VALID)) == 0)
			break;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    0, sc->sc_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	if (r == 0) {
		printf("%s: readramaddr -- "
		    "result[%d](addr %d) still valid\n",
		    device_xname(sc->sc_dv), resi, addr);
		r = -1;
	} else {
		r = 0;
		memcpy(data, &dma->test_dst, sizeof(dma->test_dst));
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CSR,
	    HIFN_DMACSR_C_CTRL_DIS | HIFN_DMACSR_S_CTRL_DIS |
	    HIFN_DMACSR_D_CTRL_DIS | HIFN_DMACSR_R_CTRL_DIS);

	return (r);
}

/*
 * Initialize the descriptor rings.
 */
static void
hifn_init_dma(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	int i;

	hifn_set_retry(sc);

	/* initialize static pointer values */
	for (i = 0; i < HIFN_D_CMD_RSIZE; i++)
		dma->cmdr[i].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, command_bufs[i][0]));
	for (i = 0; i < HIFN_D_RES_RSIZE; i++)
		dma->resr[i].p = htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, result_bufs[i][0]));

	dma->cmdr[HIFN_D_CMD_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, cmdr[0]));
	dma->srcr[HIFN_D_SRC_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, srcr[0]));
	dma->dstr[HIFN_D_DST_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, dstr[0]));
	dma->resr[HIFN_D_RES_RSIZE].p =
	    htole32(sc->sc_dmamap->dm_segs[0].ds_addr +
		offsetof(struct hifn_dma, resr[0]));

	dma->cmdu = dma->srcu = dma->dstu = dma->resu = 0;
	dma->cmdi = dma->srci = dma->dsti = dma->resi = 0;
	dma->cmdk = dma->srck = dma->dstk = dma->resk = 0;
}

/*
 * Writes out the raw command buffer space.  Returns the
 * command buffer size.
 */
static u_int
hifn_write_command(struct hifn_command *cmd, u_int8_t *buf)
{
	u_int8_t *buf_pos;
	struct hifn_base_command *base_cmd;
	struct hifn_mac_command *mac_cmd;
	struct hifn_crypt_command *cry_cmd;
	struct hifn_comp_command *comp_cmd;
	int using_mac, using_crypt, using_comp, len, ivlen;
	u_int32_t dlen, slen;

	buf_pos = buf;
	using_mac = cmd->base_masks & HIFN_BASE_CMD_MAC;
	using_crypt = cmd->base_masks & HIFN_BASE_CMD_CRYPT;
	using_comp = cmd->base_masks & HIFN_BASE_CMD_COMP;

	base_cmd = (struct hifn_base_command *)buf_pos;
	base_cmd->masks = htole16(cmd->base_masks);
	slen = cmd->src_map->dm_mapsize;
	if (cmd->sloplen)
		dlen = cmd->dst_map->dm_mapsize - cmd->sloplen +
		    sizeof(u_int32_t);
	else
		dlen = cmd->dst_map->dm_mapsize;
	base_cmd->total_source_count = htole16(slen & HIFN_BASE_CMD_LENMASK_LO);
	base_cmd->total_dest_count = htole16(dlen & HIFN_BASE_CMD_LENMASK_LO);
	dlen >>= 16;
	slen >>= 16;
	base_cmd->session_num = htole16(cmd->session_num |
	    ((slen << HIFN_BASE_CMD_SRCLEN_S) & HIFN_BASE_CMD_SRCLEN_M) |
	    ((dlen << HIFN_BASE_CMD_DSTLEN_S) & HIFN_BASE_CMD_DSTLEN_M));
	buf_pos += sizeof(struct hifn_base_command);

	if (using_comp) {
		comp_cmd = (struct hifn_comp_command *)buf_pos;
		dlen = cmd->compcrd->crd_len;
		comp_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		comp_cmd->masks = htole16(cmd->comp_masks |
		    ((dlen << HIFN_COMP_CMD_SRCLEN_S) & HIFN_COMP_CMD_SRCLEN_M));
		comp_cmd->header_skip = htole16(cmd->compcrd->crd_skip);
		comp_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_comp_command);
	}

	if (using_mac) {
		mac_cmd = (struct hifn_mac_command *)buf_pos;
		dlen = cmd->maccrd->crd_len;
		mac_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		mac_cmd->masks = htole16(cmd->mac_masks |
		    ((dlen << HIFN_MAC_CMD_SRCLEN_S) & HIFN_MAC_CMD_SRCLEN_M));
		mac_cmd->header_skip = htole16(cmd->maccrd->crd_skip);
		mac_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_mac_command);
	}

	if (using_crypt) {
		cry_cmd = (struct hifn_crypt_command *)buf_pos;
		dlen = cmd->enccrd->crd_len;
		cry_cmd->source_count = htole16(dlen & 0xffff);
		dlen >>= 16;
		cry_cmd->masks = htole16(cmd->cry_masks |
		    ((dlen << HIFN_CRYPT_CMD_SRCLEN_S) & HIFN_CRYPT_CMD_SRCLEN_M));
		cry_cmd->header_skip = htole16(cmd->enccrd->crd_skip);
		cry_cmd->reserved = 0;
		buf_pos += sizeof(struct hifn_crypt_command);
	}

	if (using_mac && cmd->mac_masks & HIFN_MAC_CMD_NEW_KEY) {
		memcpy(buf_pos, cmd->mac, HIFN_MAC_KEY_LENGTH);
		buf_pos += HIFN_MAC_KEY_LENGTH;
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_KEY) {
		switch (cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) {
		case HIFN_CRYPT_CMD_ALG_3DES:
			memcpy(buf_pos, cmd->ck, HIFN_3DES_KEY_LENGTH);
			buf_pos += HIFN_3DES_KEY_LENGTH;
			break;
		case HIFN_CRYPT_CMD_ALG_DES:
			memcpy(buf_pos, cmd->ck, HIFN_DES_KEY_LENGTH);
			buf_pos += HIFN_DES_KEY_LENGTH;
			break;
		case HIFN_CRYPT_CMD_ALG_RC4:
			len = 256;
			do {
				int clen;

				clen = MIN(cmd->cklen, len);
				memcpy(buf_pos, cmd->ck, clen);
				len -= clen;
				buf_pos += clen;
			} while (len > 0);
			memset(buf_pos, 0, 4);
			buf_pos += 4;
			break;
		case HIFN_CRYPT_CMD_ALG_AES:
			/*
			 * AES keys are variable 128, 192 and
			 * 256 bits (16, 24 and 32 bytes).
			 */
			memcpy(buf_pos, cmd->ck, cmd->cklen);
			buf_pos += cmd->cklen;
			break;
		}
	}

	if (using_crypt && cmd->cry_masks & HIFN_CRYPT_CMD_NEW_IV) {
		switch (cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) {
		case HIFN_CRYPT_CMD_ALG_AES:
			ivlen = HIFN_AES_IV_LENGTH;
			break;
		default:
			ivlen = HIFN_IV_LENGTH;
			break;
		}
		memcpy(buf_pos, cmd->iv, ivlen);
		buf_pos += ivlen;
	}

	if ((cmd->base_masks & (HIFN_BASE_CMD_MAC | HIFN_BASE_CMD_CRYPT |
	    HIFN_BASE_CMD_COMP)) == 0) {
		memset(buf_pos, 0, 8);
		buf_pos += 8;
	}

	return (buf_pos - buf);
}

static int
hifn_dmamap_aligned(bus_dmamap_t map)
{
	int i;

	for (i = 0; i < map->dm_nsegs; i++) {
		if (map->dm_segs[i].ds_addr & 3)
			return (0);
		if ((i != (map->dm_nsegs - 1)) &&
		    (map->dm_segs[i].ds_len & 3))
			return (0);
	}
	return (1);
}

static int
hifn_dmamap_load_dst(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	bus_dmamap_t map = cmd->dst_map;
	u_int32_t p, l;
	int idx, used = 0, i;

	idx = dma->dsti;
	for (i = 0; i < map->dm_nsegs - 1; i++) {
		dma->dstr[idx].p = htole32(map->dm_segs[i].ds_addr);
		dma->dstr[idx].l = htole32(HIFN_D_VALID |
		    HIFN_D_MASKDONEIRQ | map->dm_segs[i].ds_len);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		used++;

		if (++idx == HIFN_D_DST_RSIZE) {
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			idx = 0;
		}
	}

	if (cmd->sloplen == 0) {
		p = map->dm_segs[i].ds_addr;
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    map->dm_segs[i].ds_len;
	} else {
		p = sc->sc_dmamap->dm_segs[0].ds_addr +
		    offsetof(struct hifn_dma, slop[cmd->slopidx]);
		l = HIFN_D_VALID | HIFN_D_MASKDONEIRQ | HIFN_D_LAST |
		    sizeof(u_int32_t);

		if ((map->dm_segs[i].ds_len - cmd->sloplen) != 0) {
			dma->dstr[idx].p = htole32(map->dm_segs[i].ds_addr);
			dma->dstr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_MASKDONEIRQ |
			    (map->dm_segs[i].ds_len - cmd->sloplen));
			HIFN_DSTR_SYNC(sc, idx,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			used++;

			if (++idx == HIFN_D_DST_RSIZE) {
				dma->dstr[idx].l = htole32(HIFN_D_VALID |
				    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
				HIFN_DSTR_SYNC(sc, idx,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
				idx = 0;
			}
		}
	}
	dma->dstr[idx].p = htole32(p);
	dma->dstr[idx].l = htole32(l);
	HIFN_DSTR_SYNC(sc, idx, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	used++;

	if (++idx == HIFN_D_DST_RSIZE) {
		dma->dstr[idx].l = htole32(HIFN_D_VALID | HIFN_D_JUMP |
		    HIFN_D_MASKDONEIRQ);
		HIFN_DSTR_SYNC(sc, idx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		idx = 0;
	}

	dma->dsti = idx;
	dma->dstu += used;
	return (idx);
}

static int
hifn_dmamap_load_src(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	bus_dmamap_t map = cmd->src_map;
	int idx, i;
	u_int32_t last = 0;

	idx = dma->srci;
	for (i = 0; i < map->dm_nsegs; i++) {
		if (i == map->dm_nsegs - 1)
			last = HIFN_D_LAST;

		dma->srcr[idx].p = htole32(map->dm_segs[i].ds_addr);
		dma->srcr[idx].l = htole32(map->dm_segs[i].ds_len |
		    HIFN_D_VALID | HIFN_D_MASKDONEIRQ | last);
		HIFN_SRCR_SYNC(sc, idx,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

		if (++idx == HIFN_D_SRC_RSIZE) {
			dma->srcr[idx].l = htole32(HIFN_D_VALID |
			    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
			HIFN_SRCR_SYNC(sc, HIFN_D_SRC_RSIZE,
			    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
			idx = 0;
		}
	}
	dma->srci = idx;
	dma->srcu += map->dm_nsegs;
	return (idx);
}

static int
hifn_crypto(struct hifn_softc *sc, struct hifn_command *cmd,
    struct cryptop *crp, int hint)
{
	struct	hifn_dma *dma = sc->sc_dma;
	u_int32_t cmdlen;
	int	cmdi, resi, err = 0;

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->src_map))
		return (ENOMEM);

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto err_srcmap1;
		}
	} else {
		err = EINVAL;
		goto err_srcmap1;
	}

	if (hifn_dmamap_aligned(cmd->src_map)) {
		cmd->sloplen = cmd->src_map->dm_mapsize & 3;
		if (crp->crp_flags & CRYPTO_F_IOV)
			cmd->dstu.dst_io = cmd->srcu.src_io;
		else if (crp->crp_flags & CRYPTO_F_IMBUF)
			cmd->dstu.dst_m = cmd->srcu.src_m;
		cmd->dst_map = cmd->src_map;
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			err = EINVAL;
			goto err_srcmap;
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			int totlen, len;
			struct mbuf *m, *m0, *mlast;

			totlen = cmd->src_map->dm_mapsize;
			if (cmd->srcu.src_m->m_flags & M_PKTHDR) {
				len = MHLEN;
				MGETHDR(m0, M_DONTWAIT, MT_DATA);
			} else {
				len = MLEN;
				MGET(m0, M_DONTWAIT, MT_DATA);
			}
			if (m0 == NULL) {
				err = ENOMEM;
				goto err_srcmap;
			}
			if (len == MHLEN)
				M_DUP_PKTHDR(m0, cmd->srcu.src_m);
			if (totlen >= MINCLSIZE) {
				MCLGET(m0, M_DONTWAIT);
				if (m0->m_flags & M_EXT)
					len = MCLBYTES;
			}
			totlen -= len;
			m0->m_pkthdr.len = m0->m_len = len;
			mlast = m0;

			while (totlen > 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					err = ENOMEM;
					m_freem(m0);
					goto err_srcmap;
				}
				len = MLEN;
				if (totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if (m->m_flags & M_EXT)
						len = MCLBYTES;
				}

				m->m_len = len;
				if (m0->m_flags & M_PKTHDR)
					m0->m_pkthdr.len += len;
				totlen -= len;

				mlast->m_next = m;
				mlast = m;
			}
			cmd->dstu.dst_m = m0;
		}
	}

	if (cmd->dst_map == NULL) {
		if (bus_dmamap_create(sc->sc_dmat,
		    HIFN_MAX_SEGLEN * MAX_SCATTER, MAX_SCATTER,
		    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->dst_map)) {
			err = ENOMEM;
			goto err_srcmap;
		}
		if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
			    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
				err = ENOMEM;
				goto err_dstmap1;
			}
		} else if (crp->crp_flags & CRYPTO_F_IOV) {
			if (bus_dmamap_load_uio(sc->sc_dmat, cmd->dst_map,
			    cmd->dstu.dst_io, BUS_DMA_NOWAIT)) {
				err = ENOMEM;
				goto err_dstmap1;
			}
		}
	}

#ifdef HIFN_DEBUG
	if (hifn_debug)
		printf("%s: Entering cmd: stat %8x ien %8x u %d/%d/%d/%d n %d/%d\n",
		    device_xname(sc->sc_dv),
		    READ_REG_1(sc, HIFN_1_DMA_CSR),
		    READ_REG_1(sc, HIFN_1_DMA_IER),
		    dma->cmdu, dma->srcu, dma->dstu, dma->resu,
		    cmd->src_map->dm_nsegs, cmd->dst_map->dm_nsegs);
#endif

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	}

	/*
	 * need 1 cmd, and 1 res
	 * need N src, and N dst
	 */
	if ((dma->cmdu + 1) > HIFN_D_CMD_RSIZE ||
	    (dma->resu + 1) > HIFN_D_RES_RSIZE) {
		err = ENOMEM;
		goto err_dstmap;
	}
	if ((dma->srcu + cmd->src_map->dm_nsegs) > HIFN_D_SRC_RSIZE ||
	    (dma->dstu + cmd->dst_map->dm_nsegs + 1) > HIFN_D_DST_RSIZE) {
		err = ENOMEM;
		goto err_dstmap;
	}

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	cmdi = dma->cmdi++;
	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
	HIFN_CMD_SYNC(sc, cmdi, BUS_DMASYNC_PREWRITE);

	/* .p for command/result already set */
	dma->cmdr[cmdi].l = htole32(cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ);
	HIFN_CMDR_SYNC(sc, cmdi,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	dma->cmdu++;
	if (sc->sc_c_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_C_CTRL_ENA);
		sc->sc_c_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED0);
	}

	/*
	 * We don't worry about missing an interrupt (which a "command wait"
	 * interrupt salvages us from), unless there is more than one command
	 * in the queue.
	 *
	 * XXX We do seem to miss some interrupts.  So we always enable
	 * XXX command wait.  From OpenBSD revision 1.149.
	 *
	 */
#if 0
	if (dma->cmdu > 1) {
#endif
		sc->sc_dmaier |= HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
#if 0
	}
#endif

	hifnstats.hst_ipackets++;
	hifnstats.hst_ibytes += cmd->src_map->dm_mapsize;

	hifn_dmamap_load_src(sc, cmd);
	if (sc->sc_s_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_S_CTRL_ENA);
		sc->sc_s_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED1);
	}

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
#ifdef HIFN_DEBUG
	if (hifn_debug)
		printf("load res\n");
#endif
	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	resi = dma->resi++;
	dma->hifn_commands[resi] = cmd;
	HIFN_RES_SYNC(sc, resi, BUS_DMASYNC_PREREAD);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
	    HIFN_D_VALID | HIFN_D_LAST);
	HIFN_RESR_SYNC(sc, resi,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dma->resu++;
	if (sc->sc_r_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_CTRL_ENA);
		sc->sc_r_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED2);
	}

	if (cmd->sloplen)
		cmd->slopidx = resi;

	hifn_dmamap_load_dst(sc, cmd);

	if (sc->sc_d_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA);
		sc->sc_d_busy = 1;
	}

#ifdef HIFN_DEBUG
	if (hifn_debug)
		printf("%s: command: stat %8x ier %8x\n",
		    device_xname(sc->sc_dv),
		    READ_REG_1(sc, HIFN_1_DMA_CSR), READ_REG_1(sc, HIFN_1_DMA_IER));
#endif

	sc->sc_active = 5;
	return (err);		/* success */

err_dstmap:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
err_dstmap1:
	if (cmd->src_map != cmd->dst_map)
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
err_srcmap:
	if (crp->crp_flags & CRYPTO_F_IMBUF &&
	    cmd->srcu.src_m != cmd->dstu.dst_m)
		m_freem(cmd->dstu.dst_m);
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
err_srcmap1:
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	return (err);
}

static void
hifn_tick(void *vsc)
{
	struct hifn_softc *sc = vsc;

	mutex_spin_enter(&sc->sc_mtx);
	if (sc->sc_active == 0) {
		struct hifn_dma *dma = sc->sc_dma;
		u_int32_t r = 0;

		if (dma->cmdu == 0 && sc->sc_c_busy) {
			sc->sc_c_busy = 0;
			r |= HIFN_DMACSR_C_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED0);
		}
		if (dma->srcu == 0 && sc->sc_s_busy) {
			sc->sc_s_busy = 0;
			r |= HIFN_DMACSR_S_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED1);
		}
		if (dma->dstu == 0 && sc->sc_d_busy) {
			sc->sc_d_busy = 0;
			r |= HIFN_DMACSR_D_CTRL_DIS;
		}
		if (dma->resu == 0 && sc->sc_r_busy) {
			sc->sc_r_busy = 0;
			r |= HIFN_DMACSR_R_CTRL_DIS;
			CLR_LED(sc, HIFN_MIPSRST_LED2);
		}
		if (r)
			WRITE_REG_1(sc, HIFN_1_DMA_CSR, r);
	}
	else
		sc->sc_active--;
#ifdef	__OpenBSD__
	timeout_add(&sc->sc_tickto, hz);
#else
	callout_reset(&sc->sc_tickto, hz, hifn_tick, sc);
#endif
	mutex_spin_exit(&sc->sc_mtx);
}

static int
hifn_intr(void *arg)
{
	struct hifn_softc *sc = arg;
	struct hifn_dma *dma = sc->sc_dma;
	u_int32_t dmacsr, restart;
	int i, u;

	dmacsr = READ_REG_1(sc, HIFN_1_DMA_CSR);

#ifdef HIFN_DEBUG
	if (hifn_debug)
		printf("%s: irq: stat %08x ien %08x u %d/%d/%d/%d\n",
		       device_xname(sc->sc_dv),
		       dmacsr, READ_REG_1(sc, HIFN_1_DMA_IER),
		       dma->cmdu, dma->srcu, dma->dstu, dma->resu);
#endif

	mutex_spin_enter(&sc->sc_mtx);

	/* Nothing in the DMA unit interrupted */
	if ((dmacsr & sc->sc_dmaier) == 0) {
		mutex_spin_exit(&sc->sc_mtx);
		return (0);
	}

	WRITE_REG_1(sc, HIFN_1_DMA_CSR, dmacsr & sc->sc_dmaier);

	if (dmacsr & HIFN_DMACSR_ENGINE)
		WRITE_REG_0(sc, HIFN_0_PUISR, READ_REG_0(sc, HIFN_0_PUISR));

	if ((sc->sc_flags & HIFN_HAS_PUBLIC) &&
	    (dmacsr & HIFN_DMACSR_PUBDONE))
		WRITE_REG_1(sc, HIFN_1_PUB_STATUS,
		    READ_REG_1(sc, HIFN_1_PUB_STATUS) | HIFN_PUBSTS_DONE);

	restart = dmacsr & (HIFN_DMACSR_R_OVER | HIFN_DMACSR_D_OVER);
	if (restart)
		printf("%s: overrun %x\n", device_xname(sc->sc_dv), dmacsr);

	if (sc->sc_flags & HIFN_IS_7811) {
		if (dmacsr & HIFN_DMACSR_ILLR)
			printf("%s: illegal read\n", device_xname(sc->sc_dv));
		if (dmacsr & HIFN_DMACSR_ILLW)
			printf("%s: illegal write\n", device_xname(sc->sc_dv));
	}

	restart = dmacsr & (HIFN_DMACSR_C_ABORT | HIFN_DMACSR_S_ABORT |
	    HIFN_DMACSR_D_ABORT | HIFN_DMACSR_R_ABORT);
	if (restart) {
		printf("%s: abort, resetting.\n", device_xname(sc->sc_dv));
		hifnstats.hst_abort++;
		hifn_abort(sc);
		goto out;
	}

	if ((dmacsr & HIFN_DMACSR_C_WAIT) && (dma->resu == 0)) {
		/*
		 * If no slots to process and we receive a "waiting on
		 * command" interrupt, we disable the "waiting on command"
		 * (by clearing it).
		 */
		sc->sc_dmaier &= ~HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	/* clear the rings */
	i = dma->resk;
	while (dma->resu != 0) {
		HIFN_RESR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->resr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_RESR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}

		if (i != HIFN_D_RES_RSIZE) {
			struct hifn_command *cmd;

			HIFN_RES_SYNC(sc, i, BUS_DMASYNC_POSTREAD);
			cmd = dma->hifn_commands[i];
			KASSERT(cmd != NULL
				/*("hifn_intr: null command slot %u", i)*/);
			dma->hifn_commands[i] = NULL;

			hifn_callback(sc, cmd, dma->result_bufs[i]);
			hifnstats.hst_opackets++;
		}

		if (++i == (HIFN_D_RES_RSIZE + 1))
			i = 0;
		else
			dma->resu--;
	}
	dma->resk = i;

	i = dma->srck; u = dma->srcu;
	while (u != 0) {
		HIFN_SRCR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->srcr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_SRCR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_SRC_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->srck = i; dma->srcu = u;

	i = dma->cmdk; u = dma->cmdu;
	while (u != 0) {
		HIFN_CMDR_SYNC(sc, i,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->cmdr[i].l & htole32(HIFN_D_VALID)) {
			HIFN_CMDR_SYNC(sc, i,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (i != HIFN_D_CMD_RSIZE) {
			u--;
			HIFN_CMD_SYNC(sc, i, BUS_DMASYNC_POSTWRITE);
		}
		if (++i == (HIFN_D_CMD_RSIZE + 1))
			i = 0;
	}
	dma->cmdk = i; dma->cmdu = u;

out:
	mutex_spin_exit(&sc->sc_mtx);
	return (1);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int
hifn_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c;
	struct hifn_softc *sc = arg;
	int i, mac = 0, cry = 0, comp = 0, retval = EINVAL;

	KASSERT(sc != NULL /*, ("hifn_newsession: null softc")*/);
	if (sidp == NULL || cri == NULL || sc == NULL)
		return retval;

	mutex_spin_enter(&sc->sc_mtx);

	for (i = 0; i < sc->sc_maxses; i++)
		if (sc->sc_sessions[i].hs_state == HS_STATE_FREE)
			break;
	if (i == sc->sc_maxses) {
		retval = ENOMEM;
		goto out;
	}

	for (c = cri; c != NULL; c = c->cri_next) {
		switch (c->cri_alg) {
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_MD5_HMAC_96:
		case CRYPTO_SHA1_HMAC_96:
			if (mac) {
				goto out;
			}
			mac = 1;
			break;
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_AES_CBC:
			/* Note that this is an initialization
			   vector, not a cipher key; any function
			   giving sufficient Hamming distance
			   between outputs is fine.  Use of RC4
			   to generate IVs has been FIPS140-2
			   certified by several labs. */
#ifdef __NetBSD__
			cprng_fast(sc->sc_sessions[i].hs_iv,
			    c->cri_alg == CRYPTO_AES_CBC ?
				HIFN_AES_IV_LENGTH : HIFN_IV_LENGTH);
#else	/* FreeBSD and OpenBSD have get_random_bytes */
			/* XXX this may read fewer, does it matter? */
 			get_random_bytes(sc->sc_sessions[i].hs_iv,
				c->cri_alg == CRYPTO_AES_CBC ?
					HIFN_AES_IV_LENGTH : HIFN_IV_LENGTH);
#endif
			/*FALLTHROUGH*/
		case CRYPTO_ARC4:
			if (cry) {
				goto out;
			}
			cry = 1;
			break;
#ifdef HAVE_CRYPTO_LZS
		case CRYPTO_LZS_COMP:
			if (comp) {
				goto out;
			}
			comp = 1;
			break;
#endif
		default:
			goto out;
		}
	}
	if (mac == 0 && cry == 0 && comp == 0) {
		goto out;
	}

	/*
	 * XXX only want to support compression without chaining to
	 * MAC/crypt engine right now
	 */
	if ((comp && mac) || (comp && cry)) {
		goto out;
	}

	*sidp = HIFN_SID(device_unit(sc->sc_dv), i);
	sc->sc_sessions[i].hs_state = HS_STATE_USED;

	retval = 0;
out:
	mutex_spin_exit(&sc->sc_mtx);
	return retval;
}

/*
 * Deallocate a session.
 * XXX this routine should run a zero'd mac/encrypt key into context ram.
 * XXX to blow away any keys already stored there.
 */
static int
hifn_freesession(void *arg, u_int64_t tid)
{
	struct hifn_softc *sc = arg;
	int session;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	KASSERT(sc != NULL /*, ("hifn_freesession: null softc")*/);
	if (sc == NULL)
		return (EINVAL);

	mutex_spin_enter(&sc->sc_mtx);
	session = HIFN_SESSION(sid);
	if (session >= sc->sc_maxses) {
		mutex_spin_exit(&sc->sc_mtx);
		return (EINVAL);
	}

	memset(&sc->sc_sessions[session], 0, sizeof(sc->sc_sessions[session]));
	mutex_spin_exit(&sc->sc_mtx);
	return (0);
}

static int
hifn_process(void *arg, struct cryptop *crp, int hint)
{
	struct hifn_softc *sc = arg;
	struct hifn_command *cmd = NULL;
	int session, err, ivlen;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;

	if (crp == NULL || crp->crp_callback == NULL) {
		hifnstats.hst_invalid++;
		return (EINVAL);
	}

	mutex_spin_enter(&sc->sc_mtx);
	session = HIFN_SESSION(crp->crp_sid);

	if (sc == NULL || session >= sc->sc_maxses) {
		err = EINVAL;
		goto errout;
	}

	cmd = (struct hifn_command *)malloc(sizeof(struct hifn_command),
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (cmd == NULL) {
		hifnstats.hst_nomem++;
		err = ENOMEM;
		goto errout;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		cmd->srcu.src_m = (struct mbuf *)crp->crp_buf;
		cmd->dstu.dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		cmd->srcu.src_io = (struct uio *)crp->crp_buf;
		cmd->dstu.dst_io = (struct uio *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous buffers! */
	}

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC_96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC_96 ||
		    crd1->crd_alg == CRYPTO_SHA1 ||
		    crd1->crd_alg == CRYPTO_MD5) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
			   crd1->crd_alg == CRYPTO_3DES_CBC ||
			   crd1->crd_alg == CRYPTO_AES_CBC ||
			   crd1->crd_alg == CRYPTO_ARC4) {
			if ((crd1->crd_flags & CRD_F_ENCRYPT) == 0)
				cmd->base_masks |= HIFN_BASE_CMD_DECODE;
			maccrd = NULL;
			enccrd = crd1;
#ifdef	HAVE_CRYPTO_LZS
		} else if (crd1->crd_alg == CRYPTO_LZS_COMP) {
		  return (hifn_compression(sc, crp, cmd));
#endif
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC_96 ||
		     crd1->crd_alg == CRYPTO_SHA1_HMAC_96 ||
		     crd1->crd_alg == CRYPTO_MD5 ||
		     crd1->crd_alg == CRYPTO_SHA1) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
		     crd2->crd_alg == CRYPTO_3DES_CBC ||
		     crd2->crd_alg == CRYPTO_AES_CBC ||
		     crd2->crd_alg == CRYPTO_ARC4) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			cmd->base_masks = HIFN_BASE_CMD_DECODE;
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
			    crd1->crd_alg == CRYPTO_ARC4 ||
			    crd1->crd_alg == CRYPTO_3DES_CBC ||
			    crd1->crd_alg == CRYPTO_AES_CBC) &&
			   (crd2->crd_alg == CRYPTO_MD5_HMAC_96 ||
			    crd2->crd_alg == CRYPTO_SHA1_HMAC_96 ||
			    crd2->crd_alg == CRYPTO_MD5 ||
			    crd2->crd_alg == CRYPTO_SHA1) &&
			   (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the 7751 as requested
			 */
			err = EINVAL;
			goto errout;
		}
	}

	if (enccrd) {
		cmd->enccrd = enccrd;
		cmd->base_masks |= HIFN_BASE_CMD_CRYPT;
		switch (enccrd->crd_alg) {
		case CRYPTO_ARC4:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_RC4;
			if ((enccrd->crd_flags & CRD_F_ENCRYPT)
			    != sc->sc_sessions[session].hs_prev_op)
				sc->sc_sessions[session].hs_state =
				    HS_STATE_USED;
			break;
		case CRYPTO_DES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_DES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		case CRYPTO_3DES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_3DES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		case CRYPTO_AES_CBC:
			cmd->cry_masks |= HIFN_CRYPT_CMD_ALG_AES |
			    HIFN_CRYPT_CMD_MODE_CBC |
			    HIFN_CRYPT_CMD_NEW_IV;
			break;
		default:
			err = EINVAL;
			goto errout;
		}
		if (enccrd->crd_alg != CRYPTO_ARC4) {
			ivlen = ((enccrd->crd_alg == CRYPTO_AES_CBC) ?
				HIFN_AES_IV_LENGTH : HIFN_IV_LENGTH);
			if (enccrd->crd_flags & CRD_F_ENCRYPT) {
				if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
					memcpy(cmd->iv, enccrd->crd_iv, ivlen);
				else
					bcopy(sc->sc_sessions[session].hs_iv,
					    cmd->iv, ivlen);

				if ((enccrd->crd_flags & CRD_F_IV_PRESENT)
				    == 0) {
					if (crp->crp_flags & CRYPTO_F_IMBUF)
						m_copyback(cmd->srcu.src_m,
						    enccrd->crd_inject,
						    ivlen, cmd->iv);
					else if (crp->crp_flags & CRYPTO_F_IOV)
						cuio_copyback(cmd->srcu.src_io,
						    enccrd->crd_inject,
						    ivlen, cmd->iv);
				}
			} else {
				if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
					memcpy(cmd->iv, enccrd->crd_iv, ivlen);
				else if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copydata(cmd->srcu.src_m,
					    enccrd->crd_inject, ivlen, cmd->iv);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copydata(cmd->srcu.src_io,
					    enccrd->crd_inject, ivlen, cmd->iv);
			}
		}

		cmd->ck = enccrd->crd_key;
		cmd->cklen = enccrd->crd_klen >> 3;

		/*
		 * Need to specify the size for the AES key in the masks.
		 */
		if ((cmd->cry_masks & HIFN_CRYPT_CMD_ALG_MASK) ==
		    HIFN_CRYPT_CMD_ALG_AES) {
			switch (cmd->cklen) {
			case 16:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_128;
				break;
			case 24:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_192;
				break;
			case 32:
				cmd->cry_masks |= HIFN_CRYPT_CMD_KSZ_256;
				break;
			default:
				err = EINVAL;
				goto errout;
			}
		}

		if (sc->sc_sessions[session].hs_state == HS_STATE_USED)
			cmd->cry_masks |= HIFN_CRYPT_CMD_NEW_KEY;
	}

	if (maccrd) {
		cmd->maccrd = maccrd;
		cmd->base_masks |= HIFN_BASE_CMD_MAC;

		switch (maccrd->crd_alg) {
		case CRYPTO_MD5:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HASH |
			    HIFN_MAC_CMD_POS_IPSEC;
			break;
		case CRYPTO_MD5_HMAC_96:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_MD5 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		case CRYPTO_SHA1:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HASH |
			    HIFN_MAC_CMD_POS_IPSEC;
			break;
		case CRYPTO_SHA1_HMAC_96:
			cmd->mac_masks |= HIFN_MAC_CMD_ALG_SHA1 |
			    HIFN_MAC_CMD_RESULT | HIFN_MAC_CMD_MODE_HMAC |
			    HIFN_MAC_CMD_POS_IPSEC | HIFN_MAC_CMD_TRUNC;
			break;
		}

		if ((maccrd->crd_alg == CRYPTO_SHA1_HMAC_96 ||
		     maccrd->crd_alg == CRYPTO_MD5_HMAC_96) &&
		    sc->sc_sessions[session].hs_state == HS_STATE_USED) {
			cmd->mac_masks |= HIFN_MAC_CMD_NEW_KEY;
			memcpy(cmd->mac, maccrd->crd_key, maccrd->crd_klen >> 3);
			memset(cmd->mac + (maccrd->crd_klen >> 3), 0,
			    HIFN_MAC_KEY_LENGTH - (maccrd->crd_klen >> 3));
		}
	}

	cmd->crp = crp;
	cmd->session_num = session;
	cmd->softc = sc;

	err = hifn_crypto(sc, cmd, crp, hint);
	if (err == 0) {
		if (enccrd)
			sc->sc_sessions[session].hs_prev_op =
				enccrd->crd_flags & CRD_F_ENCRYPT;
		if (sc->sc_sessions[session].hs_state == HS_STATE_USED)
			sc->sc_sessions[session].hs_state = HS_STATE_KEY;
		mutex_spin_exit(&sc->sc_mtx);
		return 0;
	} else if (err == ERESTART) {
		/*
		 * There weren't enough resources to dispatch the request
		 * to the part.  Notify the caller so they'll requeue this
		 * request and resubmit it again soon.
		 */
#ifdef HIFN_DEBUG
		if (hifn_debug)
			printf("%s: requeue request\n", device_xname(sc->sc_dv));
#endif
		free(cmd, M_DEVBUF);
		sc->sc_needwakeup |= CRYPTO_SYMQ;
		mutex_spin_exit(&sc->sc_mtx);
		return (err);
	}

errout:
	if (cmd != NULL)
		free(cmd, M_DEVBUF);
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	mutex_spin_exit(&sc->sc_mtx);
	crypto_done(crp);
	return (0);
}

static void
hifn_abort(struct hifn_softc *sc)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct hifn_command *cmd;
	struct cryptop *crp;
	int i, u;

	i = dma->resk; u = dma->resu;
	while (u != 0) {
		cmd = dma->hifn_commands[i];
		KASSERT(cmd != NULL /*, ("hifn_abort: null cmd slot %u", i)*/);
		dma->hifn_commands[i] = NULL;
		crp = cmd->crp;

		if ((dma->resr[i].l & htole32(HIFN_D_VALID)) == 0) {
			/* Salvage what we can. */
			hifnstats.hst_opackets++;
			hifn_callback(sc, cmd, dma->result_bufs[i]);
		} else {
			if (cmd->src_map == cmd->dst_map) {
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
						0, cmd->src_map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			} else {
				bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
				    0, cmd->src_map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
				    0, cmd->dst_map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
			}

			if (cmd->srcu.src_m != cmd->dstu.dst_m) {
				m_freem(cmd->srcu.src_m);
				crp->crp_buf = (void *)cmd->dstu.dst_m;
			}

			/* non-shared buffers cannot be restarted */
			if (cmd->src_map != cmd->dst_map) {
				/*
				 * XXX should be EAGAIN, delayed until
				 * after the reset.
				 */
				crp->crp_etype = ENOMEM;
				bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
				bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
			} else
				crp->crp_etype = ENOMEM;

			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
			bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);

			free(cmd, M_DEVBUF);
			if (crp->crp_etype != EAGAIN)
				crypto_done(crp);
		}

		if (++i == HIFN_D_RES_RSIZE)
			i = 0;
		u--;
	}
	dma->resk = i; dma->resu = u;

	/* Force upload of key next time */
	for (i = 0; i < sc->sc_maxses; i++)
		if (sc->sc_sessions[i].hs_state == HS_STATE_KEY)
			sc->sc_sessions[i].hs_state = HS_STATE_USED;

	hifn_reset_board(sc, 1);
	hifn_init_dma(sc);
	hifn_init_pci_registers(sc);
}

static void
hifn_callback(struct hifn_softc *sc, struct hifn_command *cmd, u_int8_t *resbuf)
{
	struct hifn_dma *dma = sc->sc_dma;
	struct cryptop *crp = cmd->crp;
	struct cryptodesc *crd;
	struct mbuf *m;
	int totlen, i, u, ivlen;

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (cmd->srcu.src_m != cmd->dstu.dst_m) {
			crp->crp_buf = (void *)cmd->dstu.dst_m;
			totlen = cmd->src_map->dm_mapsize;
			for (m = cmd->dstu.dst_m; m != NULL; m = m->m_next) {
				if (totlen < m->m_len) {
					m->m_len = totlen;
					totlen = 0;
				} else
					totlen -= m->m_len;
			}
			cmd->dstu.dst_m->m_pkthdr.len =
			    cmd->srcu.src_m->m_pkthdr.len;
			m_freem(cmd->srcu.src_m);
		}
	}

	if (cmd->sloplen != 0) {
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copyback((struct mbuf *)crp->crp_buf,
			    cmd->src_map->dm_mapsize - cmd->sloplen,
			    cmd->sloplen, (void *)&dma->slop[cmd->slopidx]);
		else if (crp->crp_flags & CRYPTO_F_IOV)
			cuio_copyback((struct uio *)crp->crp_buf,
			    cmd->src_map->dm_mapsize - cmd->sloplen,
			    cmd->sloplen, (void *)&dma->slop[cmd->slopidx]);
	}

	i = dma->dstk; u = dma->dstu;
	while (u != 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    offsetof(struct hifn_dma, dstr[i]), sizeof(struct hifn_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->dstr[i].l & htole32(HIFN_D_VALID)) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    offsetof(struct hifn_dma, dstr[i]),
			    sizeof(struct hifn_desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_DST_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->dstk = i; dma->dstu = u;

	hifnstats.hst_obytes += cmd->dst_map->dm_mapsize;

	if ((cmd->base_masks & (HIFN_BASE_CMD_CRYPT | HIFN_BASE_CMD_DECODE)) ==
	    HIFN_BASE_CMD_CRYPT) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC &&
			    crd->crd_alg != CRYPTO_AES_CBC)
				continue;
			ivlen = ((crd->crd_alg == CRYPTO_AES_CBC) ?
				HIFN_AES_IV_LENGTH : HIFN_IV_LENGTH);
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - ivlen,
				    ivlen,
				    cmd->softc->sc_sessions[cmd->session_num].hs_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV) {
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - ivlen,
				    ivlen,
				    cmd->softc->sc_sessions[cmd->session_num].hs_iv);
			}
			/* XXX We do not handle contig data */
			break;
		}
	}

	if (cmd->base_masks & HIFN_BASE_CMD_MAC) {
		u_int8_t *macbuf;

		macbuf = resbuf + sizeof(struct hifn_base_result);
		if (cmd->base_masks & HIFN_BASE_CMD_COMP)
			macbuf += sizeof(struct hifn_comp_result);
		macbuf += sizeof(struct hifn_mac_result);

		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			int len;

			if (crd->crd_alg == CRYPTO_MD5)
				len = 16;
			else if (crd->crd_alg == CRYPTO_SHA1)
				len = 20;
			else if (crd->crd_alg == CRYPTO_MD5_HMAC_96 ||
			    crd->crd_alg == CRYPTO_SHA1_HMAC_96)
				len = 12;
			else
				continue;

			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copyback((struct mbuf *)crp->crp_buf,
				    crd->crd_inject, len, macbuf);
			else if ((crp->crp_flags & CRYPTO_F_IOV) && crp->crp_mac)
				memcpy(crp->crp_mac, (void *)macbuf, len);
			break;
		}
	}

	if (cmd->src_map != cmd->dst_map) {
		bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	free(cmd, M_DEVBUF);
	crypto_done(crp);
}

#ifdef HAVE_CRYPTO_LZS

static int
hifn_compression(struct hifn_softc *sc, struct cryptop *crp,
    struct hifn_command *cmd)
{
	struct cryptodesc *crd = crp->crp_desc;
	int s, err = 0;

	cmd->compcrd = crd;
	cmd->base_masks |= HIFN_BASE_CMD_COMP;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) == 0) {
		/*
		 * XXX can only handle mbufs right now since we can
		 * XXX dynamically resize them.
		 */
		err = EINVAL;
		return (ENOMEM);
	}

	if ((crd->crd_flags & CRD_F_COMP) == 0)
		cmd->base_masks |= HIFN_BASE_CMD_DECODE;
	if (crd->crd_alg == CRYPTO_LZS_COMP)
		cmd->comp_masks |= HIFN_COMP_CMD_ALG_LZS |
		    HIFN_COMP_CMD_CLEARHIST;

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->src_map)) {
		err = ENOMEM;
		goto fail;
	}

	if (bus_dmamap_create(sc->sc_dmat, HIFN_MAX_DMALEN, MAX_SCATTER,
	    HIFN_MAX_SEGLEN, 0, BUS_DMA_NOWAIT, &cmd->dst_map)) {
		err = ENOMEM;
		goto fail;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		int len;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}

		len = cmd->src_map->dm_mapsize / MCLBYTES;
		if ((cmd->src_map->dm_mapsize % MCLBYTES) != 0)
			len++;
		len *= MCLBYTES;

		if ((crd->crd_flags & CRD_F_COMP) == 0)
			len *= 4;

		if (len > HIFN_MAX_DMALEN)
			len = HIFN_MAX_DMALEN;

		cmd->dstu.dst_m = hifn_mkmbuf_chain(len, cmd->srcu.src_m);
		if (cmd->dstu.dst_m == NULL) {
			err = ENOMEM;
			goto fail;
		}

		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->src_map,
		    cmd->srcu.src_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
		if (bus_dmamap_load_uio(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_io, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto fail;
		}
	}

	if (cmd->src_map == cmd->dst_map)
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	else {
		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	}

	cmd->crp = crp;
	/*
	 * Always use session 0.  The modes of compression we use are
	 * stateless and there is always at least one compression
	 * context, zero.
	 */
	cmd->session_num = 0;
	cmd->softc = sc;

	err = hifn_compress_enter(sc, cmd);

	if (err != 0)
		goto fail;
	return (0);

fail:
	if (cmd->dst_map != NULL) {
		if (cmd->dst_map->dm_nsegs > 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	if (cmd->src_map != NULL) {
		if (cmd->src_map->dm_nsegs > 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	}
	free(cmd, M_DEVBUF);
	if (err == EINVAL)
		hifnstats.hst_invalid++;
	else
		hifnstats.hst_nomem++;
	crp->crp_etype = err;
	crypto_done(crp);
	return (0);
}

static int
hifn_compress_enter(struct hifn_softc *sc, struct hifn_command *cmd)
{
	struct hifn_dma *dma = sc->sc_dma;
	int cmdi, resi;
	u_int32_t cmdlen;

	if ((dma->cmdu + 1) > HIFN_D_CMD_RSIZE ||
	    (dma->resu + 1) > HIFN_D_CMD_RSIZE)
		return (ENOMEM);

	if ((dma->srcu + cmd->src_map->dm_nsegs) > HIFN_D_SRC_RSIZE ||
	    (dma->dstu + cmd->dst_map->dm_nsegs) > HIFN_D_DST_RSIZE)
		return (ENOMEM);

	if (dma->cmdi == HIFN_D_CMD_RSIZE) {
		dma->cmdi = 0;
		dma->cmdr[HIFN_D_CMD_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_CMDR_SYNC(sc, HIFN_D_CMD_RSIZE,
		    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	}
	cmdi = dma->cmdi++;
	cmdlen = hifn_write_command(cmd, dma->command_bufs[cmdi]);
	HIFN_CMD_SYNC(sc, cmdi, BUS_DMASYNC_PREWRITE);

	/* .p for command/result already set */
	dma->cmdr[cmdi].l = htole32(cmdlen | HIFN_D_VALID | HIFN_D_LAST |
	    HIFN_D_MASKDONEIRQ);
	HIFN_CMDR_SYNC(sc, cmdi,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);
	dma->cmdu++;
	if (sc->sc_c_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_C_CTRL_ENA);
		sc->sc_c_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED0);
	}

	/*
	 * We don't worry about missing an interrupt (which a "command wait"
	 * interrupt salvages us from), unless there is more than one command
	 * in the queue.
	 */
	if (dma->cmdu > 1) {
		sc->sc_dmaier |= HIFN_DMAIER_C_WAIT;
		WRITE_REG_1(sc, HIFN_1_DMA_IER, sc->sc_dmaier);
	}

	hifnstats.hst_ipackets++;
	hifnstats.hst_ibytes += cmd->src_map->dm_mapsize;

	hifn_dmamap_load_src(sc, cmd);
	if (sc->sc_s_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_S_CTRL_ENA);
		sc->sc_s_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED1);
	}

	/*
	 * Unlike other descriptors, we don't mask done interrupt from
	 * result descriptor.
	 */
	if (dma->resi == HIFN_D_RES_RSIZE) {
		dma->resi = 0;
		dma->resr[HIFN_D_RES_RSIZE].l = htole32(HIFN_D_VALID |
		    HIFN_D_JUMP | HIFN_D_MASKDONEIRQ);
		HIFN_RESR_SYNC(sc, HIFN_D_RES_RSIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	resi = dma->resi++;
	dma->hifn_commands[resi] = cmd;
	HIFN_RES_SYNC(sc, resi, BUS_DMASYNC_PREREAD);
	dma->resr[resi].l = htole32(HIFN_MAX_RESULT |
	    HIFN_D_VALID | HIFN_D_LAST);
	HIFN_RESR_SYNC(sc, resi,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	dma->resu++;
	if (sc->sc_r_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_R_CTRL_ENA);
		sc->sc_r_busy = 1;
		SET_LED(sc, HIFN_MIPSRST_LED2);
	}

	if (cmd->sloplen)
		cmd->slopidx = resi;

	hifn_dmamap_load_dst(sc, cmd);

	if (sc->sc_d_busy == 0) {
		WRITE_REG_1(sc, HIFN_1_DMA_CSR, HIFN_DMACSR_D_CTRL_ENA);
		sc->sc_d_busy = 1;
	}
	sc->sc_active = 5;
	cmd->cmd_callback = hifn_callback_comp;
	return (0);
}

static void
hifn_callback_comp(struct hifn_softc *sc, struct hifn_command *cmd,
    u_int8_t *resbuf)
{
	struct hifn_base_result baseres;
	struct cryptop *crp = cmd->crp;
	struct hifn_dma *dma = sc->sc_dma;
	struct mbuf *m;
	int err = 0, i, u;
	u_int32_t olen;
	bus_size_t dstsize;

	bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
	    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
	    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);

	dstsize = cmd->dst_map->dm_mapsize;
	bus_dmamap_unload(sc->sc_dmat, cmd->dst_map);

	memcpy(&baseres, resbuf, sizeof(struct hifn_base_result));

	i = dma->dstk; u = dma->dstu;
	while (u != 0) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
		    offsetof(struct hifn_dma, dstr[i]), sizeof(struct hifn_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		if (dma->dstr[i].l & htole32(HIFN_D_VALID)) {
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap,
			    offsetof(struct hifn_dma, dstr[i]),
			    sizeof(struct hifn_desc),
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			break;
		}
		if (++i == (HIFN_D_DST_RSIZE + 1))
			i = 0;
		else
			u--;
	}
	dma->dstk = i; dma->dstu = u;

	if (baseres.flags & htole16(HIFN_BASE_RES_DSTOVERRUN)) {
		bus_size_t xlen;

		xlen = dstsize;

		m_freem(cmd->dstu.dst_m);

		if (xlen == HIFN_MAX_DMALEN) {
			/* We've done all we can. */
			err = E2BIG;
			goto out;
		}

		xlen += MCLBYTES;

		if (xlen > HIFN_MAX_DMALEN)
			xlen = HIFN_MAX_DMALEN;

		cmd->dstu.dst_m = hifn_mkmbuf_chain(xlen,
		    cmd->srcu.src_m);
		if (cmd->dstu.dst_m == NULL) {
			err = ENOMEM;
			goto out;
		}
		if (bus_dmamap_load_mbuf(sc->sc_dmat, cmd->dst_map,
		    cmd->dstu.dst_m, BUS_DMA_NOWAIT)) {
			err = ENOMEM;
			goto out;
		}

		bus_dmamap_sync(sc->sc_dmat, cmd->src_map,
		    0, cmd->src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, cmd->dst_map,
		    0, cmd->dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);

		err = hifn_compress_enter(sc, cmd);
		if (err != 0)
			goto out;
		return;
	}

	olen = dstsize - (letoh16(baseres.dst_cnt) |
	    (((letoh16(baseres.session) & HIFN_BASE_RES_DSTLEN_M) >>
	    HIFN_BASE_RES_DSTLEN_S) << 16));

	crp->crp_olen = olen - cmd->compcrd->crd_skip;

	bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);

	m = cmd->dstu.dst_m;
	if (m->m_flags & M_PKTHDR)
		m->m_pkthdr.len = olen;
	crp->crp_buf = (void *)m;
	for (; m != NULL; m = m->m_next) {
		if (olen >= m->m_len)
			olen -= m->m_len;
		else {
			m->m_len = olen;
			olen = 0;
		}
	}

	m_freem(cmd->srcu.src_m);
	free(cmd, M_DEVBUF);
	crp->crp_etype = 0;
	crypto_done(crp);
	return;

out:
	if (cmd->dst_map != NULL) {
		if (cmd->src_map->dm_nsegs != 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->dst_map);
	}
	if (cmd->src_map != NULL) {
		if (cmd->src_map->dm_nsegs != 0)
			bus_dmamap_unload(sc->sc_dmat, cmd->src_map);
		bus_dmamap_destroy(sc->sc_dmat, cmd->src_map);
	}
	if (cmd->dstu.dst_m != NULL)
		m_freem(cmd->dstu.dst_m);
	free(cmd, M_DEVBUF);
	crp->crp_etype = err;
	crypto_done(crp);
}

static struct mbuf *
hifn_mkmbuf_chain(int totlen, struct mbuf *mtemplate)
{
	int len;
	struct mbuf *m, *m0, *mlast;

	if (mtemplate->m_flags & M_PKTHDR) {
		len = MHLEN;
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
	} else {
		len = MLEN;
		MGET(m0, M_DONTWAIT, MT_DATA);
	}
	if (m0 == NULL)
		return (NULL);
	if (len == MHLEN)
		M_DUP_PKTHDR(m0, mtemplate);
	MCLGET(m0, M_DONTWAIT);
	if (!(m0->m_flags & M_EXT)) {
 		m_freem(m0);
		return (NULL);
	}
	len = MCLBYTES;

	totlen -= len;
	m0->m_pkthdr.len = m0->m_len = len;
	mlast = m0;

	while (totlen > 0) {
		MGET(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			m_freem(m0);
			return (NULL);
		}
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_freem(m);
			m_freem(m0);
			return (NULL);
		}
		len = MCLBYTES;
		m->m_len = len;
		if (m0->m_flags & M_PKTHDR)
			m0->m_pkthdr.len += len;
		totlen -= len;

		mlast->m_next = m;
		mlast = m;
	}

	return (m0);
}
#endif	/* HAVE_CRYPTO_LZS */

static void
hifn_write_4(struct hifn_softc *sc, int reggrp, bus_size_t reg, u_int32_t val)
{
	/*
	 * 7811 PB3 rev/2 parts lock-up on burst writes to Group 0
	 * and Group 1 registers; avoid conditions that could create
	 * burst writes by doing a read in between the writes.
	 */
	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		if (sc->sc_waw_lastgroup == reggrp &&
		    sc->sc_waw_lastreg == reg - 4) {
			bus_space_read_4(sc->sc_st1, sc->sc_sh1, HIFN_1_REVID);
		}
		sc->sc_waw_lastgroup = reggrp;
		sc->sc_waw_lastreg = reg;
	}
	if (reggrp == 0)
		bus_space_write_4(sc->sc_st0, sc->sc_sh0, reg, val);
	else
		bus_space_write_4(sc->sc_st1, sc->sc_sh1, reg, val);

}

static u_int32_t
hifn_read_4(struct hifn_softc *sc, int reggrp, bus_size_t reg)
{
	if (sc->sc_flags & HIFN_NO_BURSTWRITE) {
		sc->sc_waw_lastgroup = -1;
		sc->sc_waw_lastreg = 1;
	}
	if (reggrp == 0)
		return (bus_space_read_4(sc->sc_st0, sc->sc_sh0, reg));
	return (bus_space_read_4(sc->sc_st1, sc->sc_sh1, reg));
}

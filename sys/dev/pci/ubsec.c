/*	$NetBSD: ubsec.c,v 1.42 2015/04/13 15:43:43 riastradh Exp $	*/
/* $FreeBSD: src/sys/dev/ubsec/ubsec.c,v 1.6.2.6 2003/01/23 21:06:43 sam Exp $ */
/*	$OpenBSD: ubsec.c,v 1.143 2009/03/27 13:31:30 reyk Exp$	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
 * Copyright (c) 2000 Theo de Raadt (deraadt@openbsd.org)
 * Copyright (c) 2001 Patrik Lindergren (patrik@ipunplugged.com)
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ubsec.c,v 1.42 2015/04/13 15:43:43 riastradh Exp $");

#undef UBSEC_DEBUG

/*
 * uBsec 5[56]01, 58xx hardware crypto accelerator
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/endian.h>
#ifdef __NetBSD__
  #define UBSEC_NO_RNG	/* hangs on attach */
  #define letoh16 htole16
  #define letoh32 htole32
#endif
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>
#ifdef __OpenBSD__
 #include <dev/rndvar.h>
 #include <sys/md5k.h>
#else
 #include <sys/cprng.h>
 #include <sys/md5.h>
 #include <sys/rndpool.h>
 #include <sys/rndsource.h>
#endif
#include <sys/sha1.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/ubsecreg.h>
#include <dev/pci/ubsecvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
static	int ubsec_probe(device_t, cfdata_t, void *);
static	void ubsec_attach(device_t, device_t, void *);
static	int  ubsec_detach(device_t, int);
static	int  ubsec_sysctl_init(void);
static	void ubsec_reset_board(struct ubsec_softc *);
static	void ubsec_init_board(struct ubsec_softc *);
static	void ubsec_init_pciregs(struct pci_attach_args *pa);
static	void ubsec_cleanchip(struct ubsec_softc *);
static	void ubsec_totalreset(struct ubsec_softc *);
static	int  ubsec_free_q(struct ubsec_softc*, struct ubsec_q *);

#ifdef __OpenBSD__
struct cfattach ubsec_ca = {
	sizeof(struct ubsec_softc), ubsec_probe, ubsec_attach,
};

struct cfdriver ubsec_cd = {
	0, "ubsec", DV_DULL
};
#else
CFATTACH_DECL_NEW(ubsec, sizeof(struct ubsec_softc), ubsec_probe, ubsec_attach,
	      ubsec_detach, NULL);
extern struct cfdriver ubsec_cd;
#endif

/* patchable */
#ifdef	UBSEC_DEBUG
extern int ubsec_debug;
int ubsec_debug=1;
#endif

static	int	ubsec_intr(void *);
static	int	ubsec_newsession(void*, u_int32_t *, struct cryptoini *);
static	int	ubsec_freesession(void*, u_int64_t);
static	int	ubsec_process(void*, struct cryptop *, int hint);
static	void	ubsec_callback(struct ubsec_softc *, struct ubsec_q *);
static	void	ubsec_feed(struct ubsec_softc *);
static	void	ubsec_mcopy(struct mbuf *, struct mbuf *, int, int);
static	void	ubsec_callback2(struct ubsec_softc *, struct ubsec_q2 *);
static	void	ubsec_feed2(struct ubsec_softc *);
static	void	ubsec_feed4(struct ubsec_softc *);
#ifndef UBSEC_NO_RNG
static  void	ubsec_rng(void *);
static  void	ubsec_rng_locked(void *);
static  void	ubsec_rng_get(size_t, void *);
#endif /* UBSEC_NO_RNG */
static	int 	ubsec_dma_malloc(struct ubsec_softc *, bus_size_t,
				 struct ubsec_dma_alloc *, int);
static	void	ubsec_dma_free(struct ubsec_softc *, struct ubsec_dma_alloc *);
static	int	ubsec_dmamap_aligned(bus_dmamap_t);

static	int	ubsec_kprocess(void*, struct cryptkop *, int);
static	int	ubsec_kprocess_modexp_sw(struct ubsec_softc *,
					 struct cryptkop *, int);
static	int	ubsec_kprocess_modexp_hw(struct ubsec_softc *,
					 struct cryptkop *, int);
static	int	ubsec_kprocess_rsapriv(struct ubsec_softc *,
				       struct cryptkop *, int);
static	void	ubsec_kfree(struct ubsec_softc *, struct ubsec_q2 *);
static	int	ubsec_ksigbits(struct crparam *);
static	void	ubsec_kshift_r(u_int, u_int8_t *, u_int, u_int8_t *, u_int);
static	void	ubsec_kshift_l(u_int, u_int8_t *, u_int, u_int8_t *, u_int);

#ifdef UBSEC_DEBUG
static void	ubsec_dump_pb(volatile struct ubsec_pktbuf *);
static void	ubsec_dump_mcr(struct ubsec_mcr *);
static	void	ubsec_dump_ctx2(volatile struct ubsec_ctx_keyop *);
#endif

#define	READ_REG(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define WRITE_REG(sc,reg,val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, reg, val)

#define	SWAP32(x) (x) = htole32(ntohl((x)))
#ifndef HTOLE32
 #define	HTOLE32(x) (x) = htole32(x)
#endif

struct ubsec_stats ubsecstats;

static struct sysctllog *ubsec_sysctllog;

/*
 * ubsec_maxbatch controls the number of crypto ops to voluntarily
 * collect into one submission to the hardware.  This batching happens
 * when ops are dispatched from the crypto subsystem with a hint that
 * more are to follow immediately.  These ops must also not be marked
 * with a ``no delay'' flag.
 */
static	int ubsec_maxbatch = 1;

/*
 * ubsec_maxaggr controls the number of crypto ops to submit to the
 * hardware as a unit.  This aggregation reduces the number of interrupts
 * to the host at the expense of increased latency (for all but the last
 * operation).  For network traffic setting this to one yields the highest
 * performance but at the expense of more interrupt processing.
 */
static	int ubsec_maxaggr = 1;

static const struct ubsec_product {
	pci_vendor_id_t		ubsec_vendor;
	pci_product_id_t	ubsec_product;
	int			ubsec_flags;
	int			ubsec_statmask;
	int			ubsec_maxaggr;
	const char		*ubsec_name;
} ubsec_products[] = {
	{ PCI_VENDOR_BLUESTEEL,	PCI_PRODUCT_BLUESTEEL_5501,
	  0,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Bluesteel 5501"
	},
	{ PCI_VENDOR_BLUESTEEL,	PCI_PRODUCT_BLUESTEEL_5601,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Bluesteel 5601"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5801,
	  0,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5801"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5802,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5802"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5805,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5805"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5820,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5820"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5821,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5821"
	},
	{ PCI_VENDOR_SUN,	PCI_PRODUCT_SUN_SCA1K,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Sun Crypto Accelerator 1000"
	},
	{ PCI_VENDOR_SUN,	PCI_PRODUCT_SUN_5821,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5821 (Sun)"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5822,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5822"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5823,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY | UBS_FLAGS_AES,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5823"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5825,
	  UBS_FLAGS_KEY | UBS_FLAGS_RNG | UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_HWNORM | UBS_FLAGS_BIGKEY | UBS_FLAGS_AES,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY,
	  UBS_MIN_AGGR,
	  "Broadcom BCM5825"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5860,
	  UBS_FLAGS_MULTIMCR | UBS_FLAGS_HWNORM |
	      UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_RNG | UBS_FLAGS_RNG4 |
	      UBS_FLAGS_KEY | UBS_FLAGS_BIGKEY | UBS_FLAGS_AES,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY |
	      BS_STAT_MCR3_ALLEMPTY | BS_STAT_MCR4_ALLEMPTY,
	  UBS_MAX_AGGR,
	  "Broadcom BCM5860"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5861,
	  UBS_FLAGS_MULTIMCR | UBS_FLAGS_HWNORM |
	      UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_RNG | UBS_FLAGS_RNG4 |
	      UBS_FLAGS_KEY | UBS_FLAGS_BIGKEY | UBS_FLAGS_AES,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY |
	      BS_STAT_MCR3_ALLEMPTY | BS_STAT_MCR4_ALLEMPTY,
	  UBS_MAX_AGGR,
	  "Broadcom BCM5861"
	},

	{ PCI_VENDOR_BROADCOM,	PCI_PRODUCT_BROADCOM_5862,
	  UBS_FLAGS_MULTIMCR | UBS_FLAGS_HWNORM |
	      UBS_FLAGS_LONGCTX |
	      UBS_FLAGS_RNG | UBS_FLAGS_RNG4 |
	      UBS_FLAGS_KEY | UBS_FLAGS_BIGKEY | UBS_FLAGS_AES,
	  BS_STAT_MCR1_DONE | BS_STAT_DMAERR |
	      BS_STAT_MCR1_ALLEMPTY | BS_STAT_MCR2_ALLEMPTY |
	      BS_STAT_MCR3_ALLEMPTY | BS_STAT_MCR4_ALLEMPTY,
	  UBS_MAX_AGGR,
	  "Broadcom BCM5862"
	},

	{ 0,			0,
	  0,
	  0,
	  0,
	  NULL
	}
};

static const struct ubsec_product *
ubsec_lookup(const struct pci_attach_args *pa)
{
	const struct ubsec_product *up;

	for (up = ubsec_products; up->ubsec_name != NULL; up++) {
		if (PCI_VENDOR(pa->pa_id) == up->ubsec_vendor &&
		    PCI_PRODUCT(pa->pa_id) == up->ubsec_product)
			return (up);
	}
	return (NULL);
}

static int
ubsec_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (ubsec_lookup(pa) != NULL)
		return (1);

	return (0);
}

static void
ubsec_attach(device_t parent, device_t self, void *aux)
{
	struct ubsec_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	const struct ubsec_product *up;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	pcireg_t memtype;
	struct ubsec_dma *dmap;
	u_int32_t cmd, i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	sc->sc_pct = pc;

	up = ubsec_lookup(pa);
	if (up == NULL) {
		printf("\n");
		panic("ubsec_attach: impossible");
	}

	pci_aprint_devinfo_fancy(pa, "Crypto processor", up->ubsec_name, 1);

	SIMPLEQ_INIT(&sc->sc_queue);
	SIMPLEQ_INIT(&sc->sc_qchip);
	SIMPLEQ_INIT(&sc->sc_queue2);
	SIMPLEQ_INIT(&sc->sc_qchip2);
	SIMPLEQ_INIT(&sc->sc_queue4);
	SIMPLEQ_INIT(&sc->sc_qchip4);
	SIMPLEQ_INIT(&sc->sc_q2free);

	sc->sc_flags = up->ubsec_flags;
	sc->sc_statmask = up->ubsec_statmask;
	sc->sc_maxaggr = up->ubsec_maxaggr;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BS_BAR);
	if (pci_mapreg_map(pa, BS_BAR, memtype, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &sc->sc_memsize)) {
		aprint_error_dev(self, "can't find mem space");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ubsec_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		aprint_error_dev(self, "couldn't get crypto driver id\n");
		pci_intr_disestablish(pc, sc->sc_ih);
		return;
	}

	sc->sc_rng_need = RND_POOLBITS / NBBY;
	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);

	SIMPLEQ_INIT(&sc->sc_freequeue);
	dmap = sc->sc_dmaa;
	for (i = 0; i < UBS_MAX_NQUEUE; i++, dmap++) {
		struct ubsec_q *q;

		q = (struct ubsec_q *)malloc(sizeof(struct ubsec_q),
		    M_DEVBUF, M_ZERO|M_NOWAIT);
		if (q == NULL) {
			aprint_error_dev(self, "can't allocate queue buffers\n");
			break;
		}

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_dmachunk),
		    &dmap->d_alloc, 0)) {
			aprint_error_dev(self, "can't allocate dma buffers\n");
			free(q, M_DEVBUF);
			break;
		}
		dmap->d_dma = (struct ubsec_dmachunk *)dmap->d_alloc.dma_vaddr;

		q->q_dma = dmap;
		sc->sc_queuea[i] = q;

		SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
	}

	crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
	    ubsec_newsession, ubsec_freesession, ubsec_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0,
	    ubsec_newsession, ubsec_freesession, ubsec_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC_96, 0, 0,
	    ubsec_newsession, ubsec_freesession, ubsec_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC_96, 0, 0,
	    ubsec_newsession, ubsec_freesession, ubsec_process, sc);
	if (sc->sc_flags & UBS_FLAGS_AES) {
		crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0,
		    ubsec_newsession, ubsec_freesession, ubsec_process, sc);
	}

	/*
	 * Reset Broadcom chip
	 */
	ubsec_reset_board(sc);

	/*
	 * Init Broadcom specific PCI settings
	 */
	ubsec_init_pciregs(pa);

	/*
	 * Init Broadcom chip
	 */
	ubsec_init_board(sc);

#ifndef UBSEC_NO_RNG
	if (sc->sc_flags & UBS_FLAGS_RNG) {
		if (sc->sc_flags & UBS_FLAGS_RNG4)
			sc->sc_statmask |= BS_STAT_MCR4_DONE;
		else
			sc->sc_statmask |= BS_STAT_MCR2_DONE;

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_mcr),
		    &sc->sc_rng.rng_q.q_mcr, 0))
			goto skip_rng;

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_ctx_rngbypass),
		    &sc->sc_rng.rng_q.q_ctx, 0)) {
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_mcr);
			goto skip_rng;
		}

		if (ubsec_dma_malloc(sc, sizeof(u_int32_t) *
		    UBSEC_RNG_BUFSIZ, &sc->sc_rng.rng_buf, 0)) {
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_ctx);
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_mcr);
			goto skip_rng;
		}

		rndsource_setcb(&sc->sc_rnd_source, ubsec_rng_get, sc);
		rnd_attach_source(&sc->sc_rnd_source, device_xname(sc->sc_dev),
				  RND_TYPE_RNG,
				  RND_FLAG_COLLECT_VALUE|RND_FLAG_HASCB);
		if (hz >= 100)
			sc->sc_rnghz = hz / 100;
		else
			sc->sc_rnghz = 1;
#ifdef __OpenBSD__
		timeout_set(&sc->sc_rngto, ubsec_rng, sc);
		timeout_add(&sc->sc_rngto, sc->sc_rnghz);
#else
		callout_init(&sc->sc_rngto, 0);
		callout_setfunc(&sc->sc_rngto, ubsec_rng, sc);
		callout_schedule(&sc->sc_rngto, sc->sc_rnghz);
#endif
 skip_rng:
		if (sc->sc_rnghz)
			aprint_normal_dev(self, "random number generator enabled\n");
		else
			aprint_error_dev(self, "WARNING: random number generator "
			    "disabled\n");
	}
#endif /* UBSEC_NO_RNG */

	if (sc->sc_flags & UBS_FLAGS_KEY) {
		sc->sc_statmask |= BS_STAT_MCR2_DONE;

		crypto_kregister(sc->sc_cid, CRK_MOD_EXP, 0,
				 ubsec_kprocess, sc);
#if 0
		crypto_kregister(sc->sc_cid, CRK_MOD_EXP_CRT, 0,
				 ubsec_kprocess, sc);
#endif
	}
}

static int
ubsec_detach(device_t self, int flags)
{
	struct ubsec_softc *sc = device_private(self);
	struct ubsec_q *q, *qtmp;
	volatile u_int32_t ctrl;

	/* disable interrupts */
	/* XXX wait/abort current ops? where is DMAERR enabled? */
	ctrl = READ_REG(sc, BS_CTRL);

	ctrl &= ~(BS_CTRL_MCR2INT | BS_CTRL_MCR1INT | BS_CTRL_DMAERR);
	if (sc->sc_flags & UBS_FLAGS_MULTIMCR)
		ctrl &= ~BS_CTRL_MCR4INT;

	WRITE_REG(sc, BS_CTRL, ctrl);

#ifndef UBSEC_NO_RNG
	if (sc->sc_flags & UBS_FLAGS_RNG) {
		callout_halt(&sc->sc_rngto, NULL);
		ubsec_dma_free(sc, &sc->sc_rng.rng_buf);
		ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_ctx);
		ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_mcr);
		rnd_detach_source(&sc->sc_rnd_source);
	}
#endif /* UBSEC_NO_RNG */

	crypto_unregister_all(sc->sc_cid);

	mutex_spin_enter(&sc->sc_mtx);

	ubsec_totalreset(sc);  /* XXX leaves the chip running */

	SIMPLEQ_FOREACH_SAFE(q, &sc->sc_freequeue, q_next, qtmp) {
		ubsec_dma_free(sc, &q->q_dma->d_alloc);
		if (q->q_src_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, q->q_src_map);
		if (q->q_cached_dst_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, q->q_cached_dst_map);
		free(q, M_DEVBUF);
	}

	mutex_spin_exit(&sc->sc_mtx);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc_memsize != 0) {
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_memsize);
		sc->sc_memsize = 0;
	}

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, ubsec, "pci,opencrypto");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
ubsec_modcmd(modcmd_t cmd, void *data)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_ubsec,
		    cfattach_ioconf_ubsec, cfdata_ioconf_ubsec);
#endif
		if (error == 0)
			error = ubsec_sysctl_init();
		return error;
	case MODULE_CMD_FINI:
		if (ubsec_sysctllog != NULL)
			sysctl_teardown(&ubsec_sysctllog);
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_ubsec,
		    cfattach_ioconf_ubsec, cfdata_ioconf_ubsec);
#endif
		return error;
	default:
		return ENOTTY;
	}
}

static int
ubsec_sysctl_init(void)
{
	const struct sysctlnode *node = NULL;

	ubsec_sysctllog = NULL;

	sysctl_createv(&ubsec_sysctllog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "ubsec", 
		SYSCTL_DESCR("ubsec opetions"),
		NULL, 0, NULL, 0,
		CTL_HW, CTL_CREATE, CTL_EOL);
	sysctl_createv(&ubsec_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "maxbatch",
		SYSCTL_DESCR("max ops to batch w/o interrupt"),
		NULL, 0, &ubsec_maxbatch, 0,
		CTL_CREATE, CTL_EOL);
	sysctl_createv(&ubsec_sysctllog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "maxaggr",
		SYSCTL_DESCR("max ops to aggregate under one interrupt"),
		NULL, 0, &ubsec_maxaggr, 0,
		CTL_CREATE, CTL_EOL);

	return 0;
}

/*
 * UBSEC Interrupt routine
 */
static int
ubsec_intr(void *arg)
{
	struct ubsec_softc *sc = arg;
	volatile u_int32_t stat;
	struct ubsec_q *q;
	struct ubsec_dma *dmap;
	int flags;
	int npkts = 0, i;

	mutex_spin_enter(&sc->sc_mtx);
	stat = READ_REG(sc, BS_STAT);
	stat &= sc->sc_statmask;
	if (stat == 0) {
		mutex_spin_exit(&sc->sc_mtx);
		return (0);
	}

	WRITE_REG(sc, BS_STAT, stat);		/* IACK */

	/*
	 * Check to see if we have any packets waiting for us
	 */
	if ((stat & BS_STAT_MCR1_DONE)) {
		while (!SIMPLEQ_EMPTY(&sc->sc_qchip)) {
			q = SIMPLEQ_FIRST(&sc->sc_qchip);
			dmap = q->q_dma;

			if ((dmap->d_dma->d_mcr.mcr_flags & htole16(UBS_MCR_DONE)) == 0)
				break;

			q = SIMPLEQ_FIRST(&sc->sc_qchip);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, /*q,*/ q_next);

			npkts = q->q_nstacked_mcrs;
			sc->sc_nqchip -= 1+npkts;
			/*
			 * search for further sc_qchip ubsec_q's that share
			 * the same MCR, and complete them too, they must be
			 * at the top.
			 */
			for (i = 0; i < npkts; i++) {
				if(q->q_stacked_mcr[i])
					ubsec_callback(sc, q->q_stacked_mcr[i]);
				else
					break;
			}
			ubsec_callback(sc, q);
		}

		/*
		 * Don't send any more packet to chip if there has been
		 * a DMAERR.
		 */
		if (!(stat & BS_STAT_DMAERR))
			ubsec_feed(sc);
	}

	/*
	 * Check to see if we have any key setups/rng's waiting for us
	 */
	if ((sc->sc_flags & (UBS_FLAGS_KEY|UBS_FLAGS_RNG)) &&
	    (stat & BS_STAT_MCR2_DONE)) {
		struct ubsec_q2 *q2;
		struct ubsec_mcr *mcr;

		while (!SIMPLEQ_EMPTY(&sc->sc_qchip2)) {
			q2 = SIMPLEQ_FIRST(&sc->sc_qchip2);

			bus_dmamap_sync(sc->sc_dmat, q2->q_mcr.dma_map,
			    0, q2->q_mcr.dma_map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			mcr = (struct ubsec_mcr *)q2->q_mcr.dma_vaddr;

			/* A bug in new devices requires to swap this field */
			if (sc->sc_flags & UBS_FLAGS_MULTIMCR)
				flags = htole16(mcr->mcr_flags);
			else
				flags = mcr->mcr_flags;
			if ((flags & htole16(UBS_MCR_DONE)) == 0) {
				bus_dmamap_sync(sc->sc_dmat,
				    q2->q_mcr.dma_map, 0,
				    q2->q_mcr.dma_map->dm_mapsize,
				    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
				break;
			}
			q2 = SIMPLEQ_FIRST(&sc->sc_qchip2);
			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip2, /*q2,*/ q_next);
			ubsec_callback2(sc, q2);
			/*
			 * Don't send any more packet to chip if there has been
			 * a DMAERR.
			 */
			if (!(stat & BS_STAT_DMAERR))
				ubsec_feed2(sc);
		}
	}
	if ((sc->sc_flags & UBS_FLAGS_RNG4) && (stat & BS_STAT_MCR4_DONE)) {
		struct ubsec_q2 *q2;
		struct ubsec_mcr *mcr;

		while (!SIMPLEQ_EMPTY(&sc->sc_qchip4)) {
			q2 = SIMPLEQ_FIRST(&sc->sc_qchip4);

			bus_dmamap_sync(sc->sc_dmat, q2->q_mcr.dma_map,
			    0, q2->q_mcr.dma_map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			mcr = (struct ubsec_mcr *)q2->q_mcr.dma_vaddr;

			/* A bug in new devices requires to swap this field */
			flags = htole16(mcr->mcr_flags);

			if ((flags & htole16(UBS_MCR_DONE)) == 0) {
				bus_dmamap_sync(sc->sc_dmat,
				    q2->q_mcr.dma_map, 0,
				    q2->q_mcr.dma_map->dm_mapsize,
				    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
				break;
			}
			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip4, q_next);
			ubsec_callback2(sc, q2);
			/*
			 * Don't send any more packet to chip if there has been
			 * a DMAERR.
			 */
			if (!(stat & BS_STAT_DMAERR))
				ubsec_feed4(sc);
		}
	}

	/*
	 * Check to see if we got any DMA Error
	 */
	if (stat & BS_STAT_DMAERR) {
#ifdef UBSEC_DEBUG
		if (ubsec_debug) {
			volatile u_int32_t a = READ_REG(sc, BS_ERR);

			printf("%s: dmaerr %s@%08x\n", device_xname(sc->sc_dev),
			    (a & BS_ERR_READ) ? "read" : "write",
			       a & BS_ERR_ADDR);
		}
#endif /* UBSEC_DEBUG */
		ubsecstats.hst_dmaerr++;
		ubsec_totalreset(sc);
		ubsec_feed(sc);
	}

	if (sc->sc_needwakeup) {		/* XXX check high watermark */
		int wkeup = sc->sc_needwakeup & (CRYPTO_SYMQ|CRYPTO_ASYMQ);
#ifdef UBSEC_DEBUG
		if (ubsec_debug)
			printf("%s: wakeup crypto (%x)\n", device_xname(sc->sc_dev),
				sc->sc_needwakeup);
#endif /* UBSEC_DEBUG */
		sc->sc_needwakeup &= ~wkeup;
		crypto_unblock(sc->sc_cid, wkeup);
	}
	mutex_spin_exit(&sc->sc_mtx);
	return (1);
}

/*
 * ubsec_feed() - aggregate and post requests to chip
 * OpenBSD comments:
 *		  It is assumed that the caller set splnet()
 */
static void
ubsec_feed(struct ubsec_softc *sc)
{
	struct ubsec_q *q, *q2;
	int npkts, i;
	void *v;
	u_int32_t stat;
#ifdef UBSEC_DEBUG
	static int max;
#endif /* UBSEC_DEBUG */

	npkts = sc->sc_nqueue;
	if (npkts > ubsecstats.hst_maxqueue)
		ubsecstats.hst_maxqueue = npkts;
	if (npkts < 2)
		goto feed1;

	/*
	 * Decide how many ops to combine in a single MCR.  We cannot
	 * aggregate more than UBS_MAX_AGGR because this is the number
	 * of slots defined in the data structure.  Otherwise we clamp
	 * based on the tunable parameter ubsec_maxaggr.  Note that
	 * aggregation can happen in two ways: either by batching ops
	 * from above or because the h/w backs up and throttles us.
	 * Aggregating ops reduces the number of interrupts to the host
	 * but also (potentially) increases the latency for processing
	 * completed ops as we only get an interrupt when all aggregated
	 * ops have completed.
	 */
	if (npkts > sc->sc_maxaggr)
		npkts = sc->sc_maxaggr;
	if (npkts > ubsec_maxaggr)
		npkts = ubsec_maxaggr;
	if (npkts > ubsecstats.hst_maxbatch)
		ubsecstats.hst_maxbatch = npkts;
	if (npkts < 2)
		goto feed1;
	ubsecstats.hst_totbatch += npkts-1;

	if ((stat = READ_REG(sc, BS_STAT)) & (BS_STAT_MCR1_FULL | BS_STAT_DMAERR)) {
		if (stat & BS_STAT_DMAERR) {
			ubsec_totalreset(sc);
			ubsecstats.hst_dmaerr++;
		} else {
			ubsecstats.hst_mcr1full++;
		}
		return;
	}

#ifdef UBSEC_DEBUG
	if (ubsec_debug)
	    printf("merging %d records\n", npkts);
	/* XXX temporary aggregation statistics reporting code */
	if (max < npkts) {
		max = npkts;
		printf("%s: new max aggregate %d\n", device_xname(sc->sc_dev), max);
	}
#endif /* UBSEC_DEBUG */

	q = SIMPLEQ_FIRST(&sc->sc_queue);
	SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, /*q,*/ q_next);
	--sc->sc_nqueue;

	bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
	    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	if (q->q_dst_map != NULL)
		bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
		    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	q->q_nstacked_mcrs = npkts - 1;		/* Number of packets stacked */

	for (i = 0; i < q->q_nstacked_mcrs; i++) {
		q2 = SIMPLEQ_FIRST(&sc->sc_queue);
		bus_dmamap_sync(sc->sc_dmat, q2->q_src_map,
		    0, q2->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		if (q2->q_dst_map != NULL)
			bus_dmamap_sync(sc->sc_dmat, q2->q_dst_map,
			    0, q2->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		q2= SIMPLEQ_FIRST(&sc->sc_queue);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, /*q2,*/ q_next);
		--sc->sc_nqueue;

		v = ((void *)&q2->q_dma->d_dma->d_mcr);
		v = (char*)v + (sizeof(struct ubsec_mcr) -
				 sizeof(struct ubsec_mcr_add));
		memcpy(&q->q_dma->d_dma->d_mcradd[i], v, sizeof(struct ubsec_mcr_add));
		q->q_stacked_mcr[i] = q2;
	}
	q->q_dma->d_dma->d_mcr.mcr_pkts = htole16(npkts);
	SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	sc->sc_nqchip += npkts;
	if (sc->sc_nqchip > ubsecstats.hst_maxqchip)
		ubsecstats.hst_maxqchip = sc->sc_nqchip;
	bus_dmamap_sync(sc->sc_dmat, q->q_dma->d_alloc.dma_map,
	    0, q->q_dma->d_alloc.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	WRITE_REG(sc, BS_MCR1, q->q_dma->d_alloc.dma_paddr +
	    offsetof(struct ubsec_dmachunk, d_mcr));
	return;

feed1:
	while (!SIMPLEQ_EMPTY(&sc->sc_queue)) {
		if ((stat = READ_REG(sc, BS_STAT)) & (BS_STAT_MCR1_FULL | BS_STAT_DMAERR)) {
			if (stat & BS_STAT_DMAERR) {
				ubsec_totalreset(sc);
				ubsecstats.hst_dmaerr++;
			} else {
				ubsecstats.hst_mcr1full++;
			}
			break;
		}

		q = SIMPLEQ_FIRST(&sc->sc_queue);

		bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
		    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		if (q->q_dst_map != NULL)
			bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
			    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		bus_dmamap_sync(sc->sc_dmat, q->q_dma->d_alloc.dma_map,
		    0, q->q_dma->d_alloc.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, BS_MCR1, q->q_dma->d_alloc.dma_paddr +
		    offsetof(struct ubsec_dmachunk, d_mcr));
#ifdef UBSEC_DEBUG
		if (ubsec_debug)
			printf("feed: q->chip %p %08x stat %08x\n",
 		    	       q, (u_int32_t)q->q_dma->d_alloc.dma_paddr,
			       stat);
#endif /* UBSEC_DEBUG */
		q = SIMPLEQ_FIRST(&sc->sc_queue);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, /*q,*/ q_next);
		--sc->sc_nqueue;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
		sc->sc_nqchip++;
	}
	if (sc->sc_nqchip > ubsecstats.hst_maxqchip)
		ubsecstats.hst_maxqchip = sc->sc_nqchip;
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int
ubsec_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c, *encini = NULL, *macini = NULL;
	struct ubsec_softc *sc;
	struct ubsec_session *ses = NULL;
	MD5_CTX md5ctx;
	SHA1_CTX sha1ctx;
	int i, sesn;

	sc = arg;
	KASSERT(sc != NULL /*, ("ubsec_newsession: null softc")*/);

	if (sidp == NULL || cri == NULL || sc == NULL)
		return (EINVAL);

	for (c = cri; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5_HMAC_96 ||
		    c->cri_alg == CRYPTO_SHA1_HMAC_96) {
			if (macini)
				return (EINVAL);
			macini = c;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
		    c->cri_alg == CRYPTO_3DES_CBC ||
		    c->cri_alg == CRYPTO_AES_CBC) {
			if (encini)
				return (EINVAL);
			encini = c;
		} else
			return (EINVAL);
	}
	if (encini == NULL && macini == NULL)
		return (EINVAL);

	if (encini && encini->cri_alg == CRYPTO_AES_CBC) {
		switch (encini->cri_klen) {
		case 128:
		case 192:
		case 256:
			break;
		default:
			return (EINVAL);
		}
	}

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct ubsec_session *)malloc(
		    sizeof(struct ubsec_session), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (sc->sc_sessions[sesn].ses_used == 0) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			ses = (struct ubsec_session *)malloc((sesn + 1) *
			    sizeof(struct ubsec_session), M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			memcpy(ses, sc->sc_sessions, sesn *
			    sizeof(struct ubsec_session));
			memset(sc->sc_sessions, 0, sesn *
			    sizeof(struct ubsec_session));
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}

	memset(ses, 0, sizeof(struct ubsec_session));
	ses->ses_used = 1;
	if (encini) {
		/* get an IV, network byte order */
#ifdef __NetBSD__
		cprng_fast(ses->ses_iv, sizeof(ses->ses_iv));
#else
		get_random_bytes(ses->ses_iv, sizeof(ses->ses_iv));
#endif

		/* Go ahead and compute key in ubsec's byte order */
		if (encini->cri_alg == CRYPTO_AES_CBC) {
			memcpy(ses->ses_key, encini->cri_key,
			    encini->cri_klen / 8);
		}
		if (encini->cri_alg == CRYPTO_DES_CBC) {
			memcpy(&ses->ses_key[0], encini->cri_key, 8);
			memcpy(&ses->ses_key[2], encini->cri_key, 8);
			memcpy(&ses->ses_key[4], encini->cri_key, 8);
		} else
			memcpy(ses->ses_key, encini->cri_key, 24);

		SWAP32(ses->ses_key[0]);
		SWAP32(ses->ses_key[1]);
		SWAP32(ses->ses_key[2]);
		SWAP32(ses->ses_key[3]);
		SWAP32(ses->ses_key[4]);
		SWAP32(ses->ses_key[5]);
	}

	if (macini) {
		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_IPAD_VAL;

		if (macini->cri_alg == CRYPTO_MD5_HMAC_96) {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, macini->cri_key,
			    macini->cri_klen / 8);
			MD5Update(&md5ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			memcpy(ses->ses_hminner, md5ctx.state,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			memcpy(ses->ses_hminner, sha1ctx.state,
			    sizeof(sha1ctx.state));
		}

		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

		if (macini->cri_alg == CRYPTO_MD5_HMAC_96) {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, macini->cri_key,
			    macini->cri_klen / 8);
			MD5Update(&md5ctx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			memcpy(ses->ses_hmouter, md5ctx.state,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			memcpy(ses->ses_hmouter, sha1ctx.state,
			    sizeof(sha1ctx.state));
		}

		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_OPAD_VAL;
	}

	*sidp = UBSEC_SID(device_unit(sc->sc_dev), sesn);
	return (0);
}

/*
 * Deallocate a session.
 */
static int
ubsec_freesession(void *arg, u_int64_t tid)
{
	struct ubsec_softc *sc;
	int session;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	sc = arg;
	KASSERT(sc != NULL /*, ("ubsec_freesession: null softc")*/);

	session = UBSEC_SESSION(sid);
	if (session >= sc->sc_nsessions)
		return (EINVAL);

	memset(&sc->sc_sessions[session], 0, sizeof(sc->sc_sessions[session]));
	return (0);
}

#ifdef __FreeBSD__ /* Ugly gratuitous changes to bus_dma */
static void
ubsec_op_cb(void *arg, bus_dma_segment_t *seg, int nsegs, bus_size_t mapsize, int error)
{
	struct ubsec_operand *op = arg;

	KASSERT(nsegs <= UBS_MAX_SCATTER
		/*, ("Too many DMA segments returned when mapping operand")*/);
#ifdef UBSEC_DEBUG
	if (ubsec_debug)
		printf("ubsec_op_cb: mapsize %u nsegs %d\n",
			(u_int) mapsize, nsegs);
#endif
	op->mapsize = mapsize;
	op->nsegs = nsegs;
	memcpy(op->segs, seg, nsegs * sizeof (seg[0]));
}
#endif

static int
ubsec_process(void *arg, struct cryptop *crp, int hint)
{
	struct ubsec_q *q = NULL;
#ifdef	__OpenBSD__
	int card;
#endif
	int err = 0, i, j, nicealign;
	struct ubsec_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;
	int encoffset = 0, macoffset = 0, cpskip, cpoffset;
	int sskip, dskip, stheend, dtheend;
	int16_t coffset;
	struct ubsec_session *ses, key;
	struct ubsec_dma *dmap = NULL;
	u_int16_t flags = 0;
	int ivlen = 0, keylen = 0;

	sc = arg;
	KASSERT(sc != NULL /*, ("ubsec_process: null softc")*/);

	if (crp == NULL || crp->crp_callback == NULL || sc == NULL) {
		ubsecstats.hst_invalid++;
		return (EINVAL);
	}
	if (UBSEC_SESSION(crp->crp_sid) >= sc->sc_nsessions) {
		ubsecstats.hst_badsession++;
		return (EINVAL);
	}

	mutex_spin_enter(&sc->sc_mtx);

	if (SIMPLEQ_EMPTY(&sc->sc_freequeue)) {
		ubsecstats.hst_queuefull++;
		sc->sc_needwakeup |= CRYPTO_SYMQ;
		mutex_spin_exit(&sc->sc_mtx);
		return(ERESTART);
	}

	q = SIMPLEQ_FIRST(&sc->sc_freequeue);
	SIMPLEQ_REMOVE_HEAD(&sc->sc_freequeue, /*q,*/ q_next);
	mutex_spin_exit(&sc->sc_mtx);

	dmap = q->q_dma; /* Save dma pointer */
	/* don't lose the cached dmamaps q_src_map and q_cached_dst_map */
	memset(q, 0, offsetof(struct ubsec_q, q_src_map));
	memset(&key, 0, sizeof(key));

	q->q_sesn = UBSEC_SESSION(crp->crp_sid);
	q->q_dma = dmap;
	ses = &sc->sc_sessions[q->q_sesn];

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		q->q_src_m = (struct mbuf *)crp->crp_buf;
		q->q_dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		q->q_src_io = (struct uio *)crp->crp_buf;
		q->q_dst_io = (struct uio *)crp->crp_buf;
	} else {
		ubsecstats.hst_badflags++;
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous blocks! */
	}

	memset(&dmap->d_dma->d_mcr, 0, sizeof(struct ubsec_mcr));

	dmap->d_dma->d_mcr.mcr_pkts = htole16(1);
	dmap->d_dma->d_mcr.mcr_flags = 0;
	q->q_crp = crp;

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		ubsecstats.hst_nodesc++;
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC_96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC_96) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_AES_CBC) {
			maccrd = NULL;
			enccrd = crd1;
		} else {
			ubsecstats.hst_badalg++;
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC_96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC_96) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
		    crd2->crd_alg == CRYPTO_3DES_CBC ||
		    crd2->crd_alg == CRYPTO_AES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_AES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC_96 ||
		    crd2->crd_alg == CRYPTO_SHA1_HMAC_96) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the ubsec as requested
			 */
			ubsecstats.hst_badalg++;
			err = EINVAL;
			goto errout;
		}
	}

	if (enccrd) {
		if (enccrd->crd_alg == CRYPTO_AES_CBC) {
			if ((sc->sc_flags & UBS_FLAGS_AES) == 0) {
				/*
				 * We cannot order the ubsec as requested
				 */
				ubsecstats.hst_badalg++;
				err = EINVAL;
				goto errout;
			}
			flags |= htole16(UBS_PKTCTX_ENC_AES);
			switch (enccrd->crd_klen) {
			case 128:
			case 192:
			case 256:
				keylen = enccrd->crd_klen / 8;
				break;
			default:
				err = EINVAL;
				goto errout;
			}
			ivlen = 16;
		} else {
			flags |= htole16(UBS_PKTCTX_ENC_3DES);
			ivlen = 8;
			keylen = 24;
		}

		encoffset = enccrd->crd_skip;

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			q->q_flags |= UBSEC_QFLAGS_COPYOUTIV;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				memcpy(key.ses_iv, enccrd->crd_iv, ivlen);
			else {
				for (i = 0; i < (ivlen / 4); i++)
					key.ses_iv[i] = ses->ses_iv[i];
			}

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copyback(q->q_src_m,
					    enccrd->crd_inject,
					    ivlen, (void *)key.ses_iv);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copyback(q->q_src_io,
					    enccrd->crd_inject,
					    ivlen, (void *)key.ses_iv);
			}
		} else {
			flags |= htole16(UBS_PKTCTX_INBOUND);

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				memcpy(key.ses_iv, enccrd->crd_iv, ivlen);
			else if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata(q->q_src_m, enccrd->crd_inject,
				    ivlen, (void *)key.ses_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata(q->q_src_io,
				    enccrd->crd_inject, 8,
				    (void *)key.ses_iv);
		}

		for (i = 0; i < (keylen / 4); i++)
			key.ses_key[i] = ses->ses_key[i];
		for (i = 0; i < (ivlen / 4); i++)
			SWAP32(key.ses_iv[i]);
	}

	if (maccrd) {
		macoffset = maccrd->crd_skip;

		if (maccrd->crd_alg == CRYPTO_MD5_HMAC_96)
			flags |= htole16(UBS_PKTCTX_AUTH_MD5);
		else
			flags |= htole16(UBS_PKTCTX_AUTH_SHA1);

		for (i = 0; i < 5; i++) {
			key.ses_hminner[i] = ses->ses_hminner[i];
			key.ses_hmouter[i] = ses->ses_hmouter[i];

			HTOLE32(key.ses_hminner[i]);
			HTOLE32(key.ses_hmouter[i]);
		}
	}

	if (enccrd && maccrd) {
		/*
		 * ubsec cannot handle packets where the end of encryption
		 * and authentication are not the same, or where the
		 * encrypted part begins before the authenticated part.
		 */
		if ((encoffset + enccrd->crd_len) !=
		    (macoffset + maccrd->crd_len)) {
			ubsecstats.hst_lenmismatch++;
			err = EINVAL;
			goto errout;
		}
		if (enccrd->crd_skip < maccrd->crd_skip) {
			ubsecstats.hst_skipmismatch++;
			err = EINVAL;
			goto errout;
		}
		sskip = maccrd->crd_skip;
		cpskip = dskip = enccrd->crd_skip;
		stheend = maccrd->crd_len;
		dtheend = enccrd->crd_len;
		coffset = enccrd->crd_skip - maccrd->crd_skip;
		cpoffset = cpskip + dtheend;
#ifdef UBSEC_DEBUG
		if (ubsec_debug) {
			printf("mac: skip %d, len %d, inject %d\n",
			       maccrd->crd_skip, maccrd->crd_len, maccrd->crd_inject);
			printf("enc: skip %d, len %d, inject %d\n",
			       enccrd->crd_skip, enccrd->crd_len, enccrd->crd_inject);
			printf("src: skip %d, len %d\n", sskip, stheend);
			printf("dst: skip %d, len %d\n", dskip, dtheend);
			printf("ubs: coffset %d, pktlen %d, cpskip %d, cpoffset %d\n",
			       coffset, stheend, cpskip, cpoffset);
		}
#endif
	} else {
		cpskip = dskip = sskip = macoffset + encoffset;
		dtheend = stheend = (enccrd)?enccrd->crd_len:maccrd->crd_len;
		cpoffset = cpskip + dtheend;
		coffset = 0;
	}

	if (q->q_src_map == NULL) {
		/* XXX FIXME: jonathan asks, what the heck's that 0xfff0?  */
		if (bus_dmamap_create(sc->sc_dmat, 0xfff0, UBS_MAX_SCATTER,
			0xfff0, 0, BUS_DMA_NOWAIT, &q->q_src_map) != 0) {
			err = ENOMEM;
			goto errout;
		}
	}
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, q->q_src_map,
		    q->q_src_m, BUS_DMA_NOWAIT) != 0) {
			ubsecstats.hst_noload++;
			err = ENOMEM;
			goto errout;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, q->q_src_map,
		    q->q_src_io, BUS_DMA_NOWAIT) != 0) {
			ubsecstats.hst_noload++;
			err = ENOMEM;
			goto errout;
		}
	}
	nicealign = ubsec_dmamap_aligned(q->q_src_map);

	dmap->d_dma->d_mcr.mcr_pktlen = htole16(stheend);

#ifdef UBSEC_DEBUG
	if (ubsec_debug)
		printf("src skip: %d nicealign: %u\n", sskip, nicealign);
#endif
	for (i = j = 0; i < q->q_src_map->dm_nsegs; i++) {
		struct ubsec_pktbuf *pb;
		bus_size_t packl = q->q_src_map->dm_segs[i].ds_len;
		bus_addr_t packp = q->q_src_map->dm_segs[i].ds_addr;

		if (sskip >= packl) {
			sskip -= packl;
			continue;
		}

		packl -= sskip;
		packp += sskip;
		sskip = 0;

		if (packl > 0xfffc) {
			err = EIO;
			goto errout;
		}

		if (j == 0)
			pb = &dmap->d_dma->d_mcr.mcr_ipktbuf;
		else
			pb = &dmap->d_dma->d_sbuf[j - 1];

		pb->pb_addr = htole32(packp);

		if (stheend) {
			if (packl > stheend) {
				pb->pb_len = htole32(stheend);
				stheend = 0;
			} else {
				pb->pb_len = htole32(packl);
				stheend -= packl;
			}
		} else
			pb->pb_len = htole32(packl);

		if ((i + 1) == q->q_src_map->dm_nsegs)
			pb->pb_next = 0;
		else
			pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
			    offsetof(struct ubsec_dmachunk, d_sbuf[j]));
		j++;
	}

	if (enccrd == NULL && maccrd != NULL) {
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_addr = 0;
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_len = 0;
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_next = htole32(dmap->d_alloc.dma_paddr +
		    offsetof(struct ubsec_dmachunk, d_macbuf[0]));
#ifdef UBSEC_DEBUG
		if (ubsec_debug)
			printf("opkt: %x %x %x\n",
	 		    dmap->d_dma->d_mcr.mcr_opktbuf.pb_addr,
	 		    dmap->d_dma->d_mcr.mcr_opktbuf.pb_len,
	 		    dmap->d_dma->d_mcr.mcr_opktbuf.pb_next);

#endif
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			if (!nicealign) {
				ubsecstats.hst_iovmisaligned++;
				err = EINVAL;
				goto errout;
			}
			if (q->q_dst_map == NULL) {
				if (q->q_cached_dst_map == NULL) {
					/* XXX: ``what the heck's that'' 0xfff0? */
					if (bus_dmamap_create(sc->sc_dmat, 0xfff0,
					    UBS_MAX_SCATTER, 0xfff0, 0, BUS_DMA_NOWAIT,
					    &q->q_cached_dst_map) != 0) {
						ubsecstats.hst_nomap++;
						err = ENOMEM;
						goto errout;
					}
				}
				q->q_dst_map = q->q_cached_dst_map;
			}
			if (bus_dmamap_load_uio(sc->sc_dmat, q->q_dst_map,
			    q->q_dst_io, BUS_DMA_NOWAIT) != 0) {
				ubsecstats.hst_noload++;
				err = ENOMEM;
				goto errout;
			}
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (nicealign) {
				q->q_dst_m = q->q_src_m;
				q->q_dst_map = q->q_src_map;
			} else {
				int totlen, len;
				struct mbuf *m, *top, **mp;

				ubsecstats.hst_unaligned++;
				totlen = q->q_src_map->dm_mapsize;
				if (q->q_src_m->m_flags & M_PKTHDR) {
					len = MHLEN;
					MGETHDR(m, M_DONTWAIT, MT_DATA);
					/*XXX FIXME: m_dup_pkthdr */
					if (m && 1 /*!m_dup_pkthdr(m, q->q_src_m, M_DONTWAIT)*/) {
						m_free(m);
						m = NULL;
					}
				} else {
					len = MLEN;
					MGET(m, M_DONTWAIT, MT_DATA);
				}
				if (m == NULL) {
					ubsecstats.hst_nombuf++;
					err = sc->sc_nqueue ? ERESTART : ENOMEM;
					goto errout;
				}
				if (len == MHLEN)
				  /*XXX was M_DUP_PKTHDR*/
				  M_COPY_PKTHDR(m, q->q_src_m);
				if (totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if ((m->m_flags & M_EXT) == 0) {
						m_free(m);
						ubsecstats.hst_nomcl++;
						err = sc->sc_nqueue ? ERESTART : ENOMEM;
						goto errout;
					}
					len = MCLBYTES;
				}
				m->m_len = len;
				top = NULL;
				mp = &top;

				while (totlen > 0) {
					if (top) {
						MGET(m, M_DONTWAIT, MT_DATA);
						if (m == NULL) {
							m_freem(top);
							ubsecstats.hst_nombuf++;
							err = sc->sc_nqueue ? ERESTART : ENOMEM;
							goto errout;
						}
						len = MLEN;
					}
					if (top && totlen >= MINCLSIZE) {
						MCLGET(m, M_DONTWAIT);
						if ((m->m_flags & M_EXT) == 0) {
							*mp = m;
							m_freem(top);
							ubsecstats.hst_nomcl++;
							err = sc->sc_nqueue ? ERESTART : ENOMEM;
							goto errout;
						}
						len = MCLBYTES;
					}
					m->m_len = len = min(totlen, len);
					totlen -= len;
					*mp = m;
					mp = &m->m_next;
				}
				q->q_dst_m = top;
				ubsec_mcopy(q->q_src_m, q->q_dst_m,
				    cpskip, cpoffset);
				if (q->q_dst_map == NULL) {
					if (q->q_cached_dst_map == NULL) {
						/* XXX again, what the heck is that 0xfff0? */
						if (bus_dmamap_create(sc->sc_dmat, 0xfff0,
						    UBS_MAX_SCATTER, 0xfff0, 0, BUS_DMA_NOWAIT,
						    &q->q_cached_dst_map) != 0) {
							ubsecstats.hst_nomap++;
							err = ENOMEM;
							goto errout;
						}
					}
					q->q_dst_map = q->q_cached_dst_map;
				}
				if (bus_dmamap_load_mbuf(sc->sc_dmat,
				    q->q_dst_map, q->q_dst_m,
				    BUS_DMA_NOWAIT) != 0) {
					ubsecstats.hst_noload++;
					err = ENOMEM;
					goto errout;
				}
			}
		} else {
			ubsecstats.hst_badflags++;
			err = EINVAL;
			goto errout;
		}

#ifdef UBSEC_DEBUG
		if (ubsec_debug)
			printf("dst skip: %d\n", dskip);
#endif
		for (i = j = 0; i < q->q_dst_map->dm_nsegs; i++) {
			struct ubsec_pktbuf *pb;
			bus_size_t packl = q->q_dst_map->dm_segs[i].ds_len;
			bus_addr_t packp = q->q_dst_map->dm_segs[i].ds_addr;

			if (dskip >= packl) {
				dskip -= packl;
				continue;
			}

			packl -= dskip;
			packp += dskip;
			dskip = 0;

			if (packl > 0xfffc) {
				err = EIO;
				goto errout;
			}

			if (j == 0)
				pb = &dmap->d_dma->d_mcr.mcr_opktbuf;
			else
				pb = &dmap->d_dma->d_dbuf[j - 1];

			pb->pb_addr = htole32(packp);

			if (dtheend) {
				if (packl > dtheend) {
					pb->pb_len = htole32(dtheend);
					dtheend = 0;
				} else {
					pb->pb_len = htole32(packl);
					dtheend -= packl;
				}
			} else
				pb->pb_len = htole32(packl);

			if ((i + 1) == q->q_dst_map->dm_nsegs) {
				if (maccrd)
					pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
					    offsetof(struct ubsec_dmachunk, d_macbuf[0]));
				else
					pb->pb_next = 0;
			} else
				pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
				    offsetof(struct ubsec_dmachunk, d_dbuf[j]));
			j++;
		}
	}

	dmap->d_dma->d_mcr.mcr_cmdctxp = htole32(dmap->d_alloc.dma_paddr +
	    offsetof(struct ubsec_dmachunk, d_ctx));

	if (enccrd && enccrd->crd_alg == CRYPTO_AES_CBC) {
		struct ubsec_pktctx_aes128	*aes128;
		struct ubsec_pktctx_aes192	*aes192;
		struct ubsec_pktctx_aes256	*aes256;
		struct ubsec_pktctx_hdr		*ph;
		u_int8_t			*ctx;

		ctx = (u_int8_t *)(dmap->d_alloc.dma_vaddr) +
		    offsetof(struct ubsec_dmachunk, d_ctx);

		ph = (struct ubsec_pktctx_hdr *)ctx;
		ph->ph_type = htole16(UBS_PKTCTX_TYPE_IPSEC_AES);
		ph->ph_flags = flags;
		ph->ph_offset = htole16(coffset >> 2);

		switch (enccrd->crd_klen) {
		case 128:
			aes128 = (struct ubsec_pktctx_aes128 *)ctx;
 			ph->ph_len = htole16(sizeof(*aes128));
			ph->ph_flags |= htole16(UBS_PKTCTX_KEYSIZE_128);
			for (i = 0; i < 4; i++)
				aes128->pc_aeskey[i] = key.ses_key[i];
			for (i = 0; i < 5; i++)
				aes128->pc_hminner[i] = key.ses_hminner[i];
			for (i = 0; i < 5; i++)
				aes128->pc_hmouter[i] = key.ses_hmouter[i];   
			for (i = 0; i < 4; i++)
				aes128->pc_iv[i] = key.ses_iv[i];
			break;
		case 192:
			aes192 = (struct ubsec_pktctx_aes192 *)ctx;
			ph->ph_len = htole16(sizeof(*aes192));
			ph->ph_flags |= htole16(UBS_PKTCTX_KEYSIZE_192);
			for (i = 0; i < 6; i++)
				aes192->pc_aeskey[i] = key.ses_key[i];
			for (i = 0; i < 5; i++)
				aes192->pc_hminner[i] = key.ses_hminner[i];
			for (i = 0; i < 5; i++)
				aes192->pc_hmouter[i] = key.ses_hmouter[i];   
			for (i = 0; i < 4; i++)
				aes192->pc_iv[i] = key.ses_iv[i];
			break;
		case 256:
			aes256 = (struct ubsec_pktctx_aes256 *)ctx;
			ph->ph_len = htole16(sizeof(*aes256));
			ph->ph_flags |= htole16(UBS_PKTCTX_KEYSIZE_256);
			for (i = 0; i < 8; i++)
				aes256->pc_aeskey[i] = key.ses_key[i];
			for (i = 0; i < 5; i++)
				aes256->pc_hminner[i] = key.ses_hminner[i];
			for (i = 0; i < 5; i++)
				aes256->pc_hmouter[i] = key.ses_hmouter[i];   
			for (i = 0; i < 4; i++)
				aes256->pc_iv[i] = key.ses_iv[i];
			break;
		}
	} else if (sc->sc_flags & UBS_FLAGS_LONGCTX) {
		struct ubsec_pktctx_3des	*ctx;
		struct ubsec_pktctx_hdr		*ph;

		ctx = (struct ubsec_pktctx_3des *)
		    ((u_int8_t *)(dmap->d_alloc.dma_vaddr) +
		    offsetof(struct ubsec_dmachunk, d_ctx));

		ph = (struct ubsec_pktctx_hdr *)ctx;
		ph->ph_len = htole16(sizeof(*ctx));
		ph->ph_type = htole16(UBS_PKTCTX_TYPE_IPSEC_3DES);
		ph->ph_flags = flags;
		ph->ph_offset = htole16(coffset >> 2);

		for (i = 0; i < 6; i++)
			ctx->pc_deskey[i] = key.ses_key[i];
		for (i = 0; i < 5; i++)
			ctx->pc_hminner[i] = key.ses_hminner[i];
		for (i = 0; i < 5; i++)
			ctx->pc_hmouter[i] = key.ses_hmouter[i];
		for (i = 0; i < 2; i++)
			ctx->pc_iv[i] = key.ses_iv[i];
	} else {
		struct ubsec_pktctx *ctx = (struct ubsec_pktctx *)
		    ((u_int8_t *)dmap->d_alloc.dma_vaddr +
		    offsetof(struct ubsec_dmachunk, d_ctx));

		ctx->pc_flags = flags;
		ctx->pc_offset = htole16(coffset >> 2);
		for (i = 0; i < 6; i++)
			ctx->pc_deskey[i] = key.ses_key[i];
		for (i = 0; i < 5; i++)
			ctx->pc_hminner[i] = key.ses_hminner[i];
		for (i = 0; i < 5; i++)
			ctx->pc_hmouter[i] = key.ses_hmouter[i];   
		for (i = 0; i < 2; i++)
			ctx->pc_iv[i] = key.ses_iv[i];
	}

	mutex_spin_enter(&sc->sc_mtx);
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
	sc->sc_nqueue++;
	ubsecstats.hst_ipackets++;
	ubsecstats.hst_ibytes += dmap->d_alloc.dma_map->dm_mapsize;
	if ((hint & CRYPTO_HINT_MORE) == 0 || sc->sc_nqueue >= ubsec_maxbatch)
		ubsec_feed(sc);
	mutex_spin_exit(&sc->sc_mtx);
	return (0);

errout:
	if (q != NULL) {
		if ((q->q_dst_m != NULL) && (q->q_src_m != q->q_dst_m))
			m_freem(q->q_dst_m);

		if (q->q_dst_map != NULL && q->q_dst_map != q->q_src_map) {
			bus_dmamap_unload(sc->sc_dmat, q->q_dst_map);
		}
		if (q->q_src_map != NULL) {
			bus_dmamap_unload(sc->sc_dmat, q->q_src_map);
		}

		mutex_spin_enter(&sc->sc_mtx);
		SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
		mutex_spin_exit(&sc->sc_mtx);
	}
#if 0 /* jonathan says: this openbsd code seems to be subsumed elsewhere */
	if (err == EINVAL)
		ubsecstats.hst_invalid++;
	else
		ubsecstats.hst_nomem++;
#endif
	if (err != ERESTART) {
		crp->crp_etype = err;
		crypto_done(crp);
	} else {
		sc->sc_needwakeup |= CRYPTO_SYMQ;
	}
	return (err);
}

static void
ubsec_callback(struct ubsec_softc *sc, struct ubsec_q *q)
{
	struct cryptop *crp = (struct cryptop *)q->q_crp;
	struct cryptodesc *crd;
	struct ubsec_dma *dmap = q->q_dma;

	ubsecstats.hst_opackets++;
	ubsecstats.hst_obytes += dmap->d_alloc.dma_size;

	bus_dmamap_sync(sc->sc_dmat, dmap->d_alloc.dma_map, 0,
	    dmap->d_alloc.dma_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	if (q->q_dst_map != NULL && q->q_dst_map != q->q_src_map) {
		bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
		    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, q->q_dst_map);
	}
	bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
	    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, q->q_src_map);

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (q->q_src_m != q->q_dst_m)) {
		m_freem(q->q_src_m);
		crp->crp_buf = (void *)q->q_dst_m;
	}

	/* copy out IV for future use */
	if (q->q_flags & UBSEC_QFLAGS_COPYOUTIV) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC &&
			    crd->crd_alg != CRYPTO_AES_CBC)
				continue;
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8,
				    (void *)sc->sc_sessions[q->q_sesn].ses_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV) {
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8,
				    (void *)sc->sc_sessions[q->q_sesn].ses_iv);
			}
			break;
		}
	}

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		if (crd->crd_alg != CRYPTO_MD5_HMAC_96 &&
		    crd->crd_alg != CRYPTO_SHA1_HMAC_96)
			continue;
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copyback((struct mbuf *)crp->crp_buf,
			    crd->crd_inject, 12,
			    (void *)dmap->d_dma->d_macbuf);
		else if (crp->crp_flags & CRYPTO_F_IOV && crp->crp_mac)
			bcopy((void *)dmap->d_dma->d_macbuf,
			    crp->crp_mac, 12);
		break;
	}
	SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
	crypto_done(crp);
}

static void
ubsec_mcopy(struct mbuf *srcm, struct mbuf *dstm, int hoffset, int toffset)
{
	int i, j, dlen, slen;
	char *dptr, *sptr;

	j = 0;
	sptr = srcm->m_data;
	slen = srcm->m_len;
	dptr = dstm->m_data;
	dlen = dstm->m_len;

	while (1) {
		for (i = 0; i < min(slen, dlen); i++) {
			if (j < hoffset || j >= toffset)
				*dptr++ = *sptr++;
			slen--;
			dlen--;
			j++;
		}
		if (slen == 0) {
			srcm = srcm->m_next;
			if (srcm == NULL)
				return;
			sptr = srcm->m_data;
			slen = srcm->m_len;
		}
		if (dlen == 0) {
			dstm = dstm->m_next;
			if (dstm == NULL)
				return;
			dptr = dstm->m_data;
			dlen = dstm->m_len;
		}
	}
}

/*
 * feed the key generator, must be called at splnet() or higher.
 */
static void
ubsec_feed2(struct ubsec_softc *sc)
{
	struct ubsec_q2 *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue2)) {
		if (READ_REG(sc, BS_STAT) & BS_STAT_MCR2_FULL)
			break;
		q = SIMPLEQ_FIRST(&sc->sc_queue2);

		bus_dmamap_sync(sc->sc_dmat, q->q_mcr.dma_map, 0,
		    q->q_mcr.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, q->q_ctx.dma_map, 0,
		    q->q_ctx.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, BS_MCR2, q->q_mcr.dma_paddr);
		q = SIMPLEQ_FIRST(&sc->sc_queue2);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue2, /*q,*/ q_next);
		--sc->sc_nqueue2;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip2, q, q_next);
	}
}

/*
 * feed the RNG (used instead of ubsec_feed2() on 5827+ devices)
 */
void
ubsec_feed4(struct ubsec_softc *sc)
{
	struct ubsec_q2 *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue4)) {
		if (READ_REG(sc, BS_STAT) & BS_STAT_MCR4_FULL)
			break;
		q = SIMPLEQ_FIRST(&sc->sc_queue4);

		bus_dmamap_sync(sc->sc_dmat, q->q_mcr.dma_map, 0,
		    q->q_mcr.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, q->q_ctx.dma_map, 0,
		    q->q_ctx.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, BS_MCR4, q->q_mcr.dma_paddr);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue4, q_next);
		--sc->sc_nqueue4;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip4, q, q_next);
	}
}

/*
 * Callback for handling random numbers
 */
static void
ubsec_callback2(struct ubsec_softc *sc, struct ubsec_q2 *q)
{
	struct cryptkop *krp;
	struct ubsec_ctx_keyop *ctx;

	ctx = (struct ubsec_ctx_keyop *)q->q_ctx.dma_vaddr;
	bus_dmamap_sync(sc->sc_dmat, q->q_ctx.dma_map, 0,
	    q->q_ctx.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

	switch (q->q_type) {
#ifndef UBSEC_NO_RNG
	case UBS_CTXOP_RNGSHA1:
	case UBS_CTXOP_RNGBYPASS: {
		struct ubsec_q2_rng *rng = (struct ubsec_q2_rng *)q;
		u_int32_t *p;
		int i;

		bus_dmamap_sync(sc->sc_dmat, rng->rng_buf.dma_map, 0,
		    rng->rng_buf.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		p = (u_int32_t *)rng->rng_buf.dma_vaddr;
#ifndef __NetBSD__
		for (i = 0; i < UBSEC_RNG_BUFSIZ; p++, i++)
			add_true_randomness(letoh32(*p));
#else
		i = UBSEC_RNG_BUFSIZ * sizeof(u_int32_t);
		rnd_add_data(&sc->sc_rnd_source, (char *)p, i, i * NBBY);
		sc->sc_rng_need -= i;
#endif
		rng->rng_used = 0;
#ifdef __OpenBSD__
		timeout_add(&sc->sc_rngto, sc->sc_rnghz);
#else
		if (sc->sc_rng_need > 0) {
			callout_schedule(&sc->sc_rngto, sc->sc_rnghz);
		}
#endif
		break;
	}
#endif
	case UBS_CTXOP_MODEXP: {
		struct ubsec_q2_modexp *me = (struct ubsec_q2_modexp *)q;
		u_int rlen, clen;

		krp = me->me_krp;
		rlen = (me->me_modbits + 7) / 8;
		clen = (krp->krp_param[krp->krp_iparams].crp_nbits + 7) / 8;

		bus_dmamap_sync(sc->sc_dmat, me->me_M.dma_map,
		    0, me->me_M.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, me->me_E.dma_map,
		    0, me->me_E.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, me->me_C.dma_map,
		    0, me->me_C.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_sync(sc->sc_dmat, me->me_epb.dma_map,
		    0, me->me_epb.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

		if (clen < rlen)
			krp->krp_status = E2BIG;
		else {
			if (sc->sc_flags & UBS_FLAGS_HWNORM) {
				memset(krp->krp_param[krp->krp_iparams].crp_p, 0,
				    (krp->krp_param[krp->krp_iparams].crp_nbits
					+ 7) / 8);
				bcopy(me->me_C.dma_vaddr,
				    krp->krp_param[krp->krp_iparams].crp_p,
				    (me->me_modbits + 7) / 8);
			} else
				ubsec_kshift_l(me->me_shiftbits,
				    me->me_C.dma_vaddr, me->me_normbits,
				    krp->krp_param[krp->krp_iparams].crp_p,
				    krp->krp_param[krp->krp_iparams].crp_nbits);
		}

		crypto_kdone(krp);

		/* bzero all potentially sensitive data */
		memset(me->me_E.dma_vaddr, 0, me->me_E.dma_size);
		memset(me->me_M.dma_vaddr, 0, me->me_M.dma_size);
		memset(me->me_C.dma_vaddr, 0, me->me_C.dma_size);
		memset(me->me_q.q_ctx.dma_vaddr, 0, me->me_q.q_ctx.dma_size);

		/* Can't free here, so put us on the free list. */
		SIMPLEQ_INSERT_TAIL(&sc->sc_q2free, &me->me_q, q_next);
		break;
	}
	case UBS_CTXOP_RSAPRIV: {
		struct ubsec_q2_rsapriv *rp = (struct ubsec_q2_rsapriv *)q;
		u_int len;

		krp = rp->rpr_krp;
		bus_dmamap_sync(sc->sc_dmat, rp->rpr_msgin.dma_map, 0,
		    rp->rpr_msgin.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_sync(sc->sc_dmat, rp->rpr_msgout.dma_map, 0,
		    rp->rpr_msgout.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);

		len = (krp->krp_param[UBS_RSAPRIV_PAR_MSGOUT].crp_nbits + 7) / 8;
		bcopy(rp->rpr_msgout.dma_vaddr,
		    krp->krp_param[UBS_RSAPRIV_PAR_MSGOUT].crp_p, len);

		crypto_kdone(krp);

		memset(rp->rpr_msgin.dma_vaddr, 0, rp->rpr_msgin.dma_size);
		memset(rp->rpr_msgout.dma_vaddr, 0, rp->rpr_msgout.dma_size);
		memset(rp->rpr_q.q_ctx.dma_vaddr, 0, rp->rpr_q.q_ctx.dma_size);

		/* Can't free here, so put us on the free list. */
		SIMPLEQ_INSERT_TAIL(&sc->sc_q2free, &rp->rpr_q, q_next);
		break;
	}
	default:
		printf("%s: unknown ctx op: %x\n", device_xname(sc->sc_dev),
		    letoh16(ctx->ctx_op));
		break;
	}
}

#ifndef UBSEC_NO_RNG

static void
ubsec_rng_get(size_t bytes, void *vsc)
{
	struct ubsec_softc *sc = vsc;

	mutex_spin_enter(&sc->sc_mtx);
	sc->sc_rng_need = bytes;
	ubsec_rng_locked(sc);
	mutex_spin_exit(&sc->sc_mtx);

}

static void
ubsec_rng(void *vsc)
{
	struct ubsec_softc *sc = vsc;
	mutex_spin_enter(&sc->sc_mtx);
	ubsec_rng_locked(sc);
	mutex_spin_exit(&sc->sc_mtx);
}

static void
ubsec_rng_locked(void *vsc)
{
	struct ubsec_softc *sc = vsc;
	struct ubsec_q2_rng *rng = &sc->sc_rng;
	struct ubsec_mcr *mcr;
	struct ubsec_ctx_rngbypass *ctx;
	int *nqueue;

	/* Caller is responsible to lock and release sc_mtx. */
	KASSERT(mutex_owned(&sc->sc_mtx));

	if (rng->rng_used) {
		return;
	}

	if (sc->sc_rng_need < 1) {
		callout_stop(&sc->sc_rngto);
		return;
	}

	if (sc->sc_flags & UBS_FLAGS_RNG4)
		nqueue = &sc->sc_nqueue4;
	else
		nqueue = &sc->sc_nqueue2;

	(*nqueue)++;
	if (*nqueue >= UBS_MAX_NQUEUE)
 		goto out;

	mcr = (struct ubsec_mcr *)rng->rng_q.q_mcr.dma_vaddr;
	ctx = (struct ubsec_ctx_rngbypass *)rng->rng_q.q_ctx.dma_vaddr;

	mcr->mcr_pkts = htole16(1);
	mcr->mcr_flags = 0;
	mcr->mcr_cmdctxp = htole32(rng->rng_q.q_ctx.dma_paddr);
	mcr->mcr_ipktbuf.pb_addr = mcr->mcr_ipktbuf.pb_next = 0;
	mcr->mcr_ipktbuf.pb_len = 0;
	mcr->mcr_reserved = mcr->mcr_pktlen = 0;
	mcr->mcr_opktbuf.pb_addr = htole32(rng->rng_buf.dma_paddr);
	mcr->mcr_opktbuf.pb_len = htole32(((sizeof(u_int32_t) * UBSEC_RNG_BUFSIZ)) &
	    UBS_PKTBUF_LEN);
	mcr->mcr_opktbuf.pb_next = 0;

	ctx->rbp_len = htole16(sizeof(struct ubsec_ctx_rngbypass));
	ctx->rbp_op = htole16(UBS_CTXOP_RNGSHA1);
	rng->rng_q.q_type = UBS_CTXOP_RNGSHA1;

	bus_dmamap_sync(sc->sc_dmat, rng->rng_buf.dma_map, 0,
	    rng->rng_buf.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	if (sc->sc_flags & UBS_FLAGS_RNG4) {
		SIMPLEQ_INSERT_TAIL(&sc->sc_queue4, &rng->rng_q, q_next);
		ubsec_feed4(sc);
	} else {
		SIMPLEQ_INSERT_TAIL(&sc->sc_queue2, &rng->rng_q, q_next);
		ubsec_feed2(sc);
	}
	rng->rng_used = 1;
	ubsecstats.hst_rng++;

	return;

out:
	/*
	 * Something weird happened, generate our own call back.
	 */
	(*nqueue)--;
#ifdef __OpenBSD__
	timeout_add(&sc->sc_rngto, sc->sc_rnghz);
#else
	callout_schedule(&sc->sc_rngto, sc->sc_rnghz);
#endif
}
#endif /* UBSEC_NO_RNG */

static int
ubsec_dma_malloc(struct ubsec_softc *sc, bus_size_t size,
		 struct ubsec_dma_alloc *dma,int mapflags)
{
	int r;

	if ((r = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &dma->dma_seg, 1, &dma->dma_nseg, BUS_DMA_NOWAIT)) != 0)
		goto fail_0;

	if ((r = bus_dmamem_map(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg,
	    size, &dma->dma_vaddr, mapflags | BUS_DMA_NOWAIT)) != 0)
		goto fail_1;

	if ((r = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &dma->dma_map)) != 0)
		goto fail_2;

	if ((r = bus_dmamap_load(sc->sc_dmat, dma->dma_map, dma->dma_vaddr,
	    size, NULL, BUS_DMA_NOWAIT)) != 0)
		goto fail_3;

	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	dma->dma_size = size;
	return (0);

fail_3:
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, size);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
fail_0:
	dma->dma_map = NULL;
	return (r);
}

static void
ubsec_dma_free(struct ubsec_softc *sc, struct ubsec_dma_alloc *dma)
{
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, dma->dma_size);
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
static void
ubsec_reset_board(struct ubsec_softc *sc)
{
	volatile u_int32_t ctrl;

	ctrl = READ_REG(sc, BS_CTRL);
	ctrl |= BS_CTRL_RESET;
	WRITE_REG(sc, BS_CTRL, ctrl);

	/*
	* Wait aprox. 30 PCI clocks = 900 ns = 0.9 us
	*/
	DELAY(10);

	/* Enable RNG and interrupts on newer devices */
	if (sc->sc_flags & UBS_FLAGS_MULTIMCR) {
#ifndef UBSEC_NO_RNG
		WRITE_REG(sc, BS_CFG, BS_CFG_RNG);
#endif
		WRITE_REG(sc, BS_INT, BS_INT_DMAINT);
	}
}

/*
 * Init Broadcom registers
 */
static void
ubsec_init_board(struct ubsec_softc *sc)
{
	u_int32_t ctrl;

	ctrl = READ_REG(sc, BS_CTRL);
	ctrl &= ~(BS_CTRL_BE32 | BS_CTRL_BE64);
	ctrl |= BS_CTRL_LITTLE_ENDIAN | BS_CTRL_MCR1INT;

	/*
	 * XXX: Sam Leffler's code has (UBS_FLAGS_KEY|UBS_FLAGS_RNG)).
	 * anyone got hw docs?
	 */
	if (sc->sc_flags & UBS_FLAGS_KEY)
		ctrl |= BS_CTRL_MCR2INT;
	else
		ctrl &= ~BS_CTRL_MCR2INT;

	if (sc->sc_flags & UBS_FLAGS_HWNORM)
		ctrl &= ~BS_CTRL_SWNORM;

	if (sc->sc_flags & UBS_FLAGS_MULTIMCR) {
		ctrl |= BS_CTRL_BSIZE240;
		ctrl &= ~BS_CTRL_MCR3INT; /* MCR3 is reserved for SSL */

		if (sc->sc_flags & UBS_FLAGS_RNG4)
			ctrl |= BS_CTRL_MCR4INT;
		else
			ctrl &= ~BS_CTRL_MCR4INT;
	}

	WRITE_REG(sc, BS_CTRL, ctrl);
}

/*
 * Init Broadcom PCI registers
 */
static void
ubsec_init_pciregs(struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t misc;

	/*
	 * This will set the cache line size to 1, this will
	 * force the BCM58xx chip just to do burst read/writes.
	 * Cache line read/writes are to slow
	 */
	misc = pci_conf_read(pc, pa->pa_tag, PCI_BHLC_REG);
	misc = (misc & ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT))
	    | ((UBS_DEF_CACHELINE & 0xff) << PCI_CACHELINE_SHIFT);
	pci_conf_write(pc, pa->pa_tag, PCI_BHLC_REG, misc);
}

/*
 * Clean up after a chip crash.
 * It is assumed that the caller in splnet()
 */
static void
ubsec_cleanchip(struct ubsec_softc *sc)
{
	struct ubsec_q *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_qchip)) {
		q = SIMPLEQ_FIRST(&sc->sc_qchip);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, /*q,*/ q_next);
		ubsec_free_q(sc, q);
	}
	sc->sc_nqchip = 0;
}

/*
 * free a ubsec_q
 * It is assumed that the caller is within splnet()
 */
static int
ubsec_free_q(struct ubsec_softc *sc, struct ubsec_q *q)
{
	struct ubsec_q *q2;
	struct cryptop *crp;
	int npkts;
	int i;

	npkts = q->q_nstacked_mcrs;

	for (i = 0; i < npkts; i++) {
		if(q->q_stacked_mcr[i]) {
			q2 = q->q_stacked_mcr[i];

			if ((q2->q_dst_m != NULL) && (q2->q_src_m != q2->q_dst_m))
				m_freem(q2->q_dst_m);

			crp = (struct cryptop *)q2->q_crp;

			SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q2, q_next);

			crp->crp_etype = EFAULT;
			crypto_done(crp);
		} else {
			break;
		}
	}

	/*
	 * Free header MCR
	 */
	if ((q->q_dst_m != NULL) && (q->q_src_m != q->q_dst_m))
		m_freem(q->q_dst_m);

	crp = (struct cryptop *)q->q_crp;

	SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);

	crp->crp_etype = EFAULT;
	crypto_done(crp);
	return(0);
}

/*
 * Routine to reset the chip and clean up.
 * It is assumed that the caller is in splnet()
 */
static void
ubsec_totalreset(struct ubsec_softc *sc)
{
	ubsec_reset_board(sc);
	ubsec_init_board(sc);
	ubsec_cleanchip(sc);
}

static int
ubsec_dmamap_aligned(bus_dmamap_t map)
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

#ifdef __OpenBSD__
struct ubsec_softc *
ubsec_kfind(struct cryptkop *krp)
{
	struct ubsec_softc *sc;
	int i;

	for (i = 0; i < ubsec_cd.cd_ndevs; i++) {
		sc = ubsec_cd.cd_devs[i];
		if (sc == NULL)
			continue;
		if (sc->sc_cid == krp->krp_hid)
			return (sc);
	}
	return (NULL);
}
#endif

static void
ubsec_kfree(struct ubsec_softc *sc, struct ubsec_q2 *q)
{
	switch (q->q_type) {
	case UBS_CTXOP_MODEXP: {
		struct ubsec_q2_modexp *me = (struct ubsec_q2_modexp *)q;

		ubsec_dma_free(sc, &me->me_q.q_mcr);
		ubsec_dma_free(sc, &me->me_q.q_ctx);
		ubsec_dma_free(sc, &me->me_M);
		ubsec_dma_free(sc, &me->me_E);
		ubsec_dma_free(sc, &me->me_C);
		ubsec_dma_free(sc, &me->me_epb);
		free(me, M_DEVBUF);
		break;
	}
	case UBS_CTXOP_RSAPRIV: {
		struct ubsec_q2_rsapriv *rp = (struct ubsec_q2_rsapriv *)q;

		ubsec_dma_free(sc, &rp->rpr_q.q_mcr);
		ubsec_dma_free(sc, &rp->rpr_q.q_ctx);
		ubsec_dma_free(sc, &rp->rpr_msgin);
		ubsec_dma_free(sc, &rp->rpr_msgout);
		free(rp, M_DEVBUF);
		break;
	}
	default:
		printf("%s: invalid kfree 0x%x\n", device_xname(sc->sc_dev),
		    q->q_type);
		break;
	}
}

static int
ubsec_kprocess(void *arg, struct cryptkop *krp, int hint)
{
	struct ubsec_softc *sc;
	int r;

	if (krp == NULL || krp->krp_callback == NULL)
		return (EINVAL);
#ifdef __OpenBSD__
	if ((sc = ubsec_kfind(krp)) == NULL)
		return (EINVAL);
#else
	sc = arg;
	KASSERT(sc != NULL /*, ("ubsec_kprocess: null softc")*/);
#endif

	while (!SIMPLEQ_EMPTY(&sc->sc_q2free)) {
		struct ubsec_q2 *q;

		q = SIMPLEQ_FIRST(&sc->sc_q2free);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_q2free, /*q,*/ q_next);
		ubsec_kfree(sc, q);
	}

	switch (krp->krp_op) {
	case CRK_MOD_EXP:
		if (sc->sc_flags & UBS_FLAGS_HWNORM)
			r = ubsec_kprocess_modexp_hw(sc, krp, hint);
		else
			r = ubsec_kprocess_modexp_sw(sc, krp, hint);
		break;
	case CRK_MOD_EXP_CRT:
		r = ubsec_kprocess_rsapriv(sc, krp, hint);
		break;
	default:
		printf("%s: kprocess: invalid op 0x%x\n",
		    device_xname(sc->sc_dev), krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		r = 0;
	}
	return (r);
}

/*
 * Start computation of cr[C] = (cr[M] ^ cr[E]) mod cr[N] (sw normalization)
 */
static int
ubsec_kprocess_modexp_sw(struct ubsec_softc *sc, struct cryptkop *krp,
			 int hint)
{
	struct ubsec_q2_modexp *me;
	struct ubsec_mcr *mcr;
	struct ubsec_ctx_modexp *ctx;
	struct ubsec_pktbuf *epb;
	int err = 0;
	u_int nbits, normbits, mbits, shiftbits, ebits;

	me = (struct ubsec_q2_modexp *)malloc(sizeof *me, M_DEVBUF, M_NOWAIT);
	if (me == NULL) {
		err = ENOMEM;
		goto errout;
	}
	memset(me, 0, sizeof *me);
	me->me_krp = krp;
	me->me_q.q_type = UBS_CTXOP_MODEXP;

	nbits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_N]);
	if (nbits <= 512)
		normbits = 512;
	else if (nbits <= 768)
		normbits = 768;
	else if (nbits <= 1024)
		normbits = 1024;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && nbits <= 1536)
		normbits = 1536;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && nbits <= 2048)
		normbits = 2048;
	else {
		err = E2BIG;
		goto errout;
	}

	shiftbits = normbits - nbits;

	me->me_modbits = nbits;
	me->me_shiftbits = shiftbits;
	me->me_normbits = normbits;

	/* Sanity check: result bits must be >= true modulus bits. */
	if (krp->krp_param[krp->krp_iparams].crp_nbits < nbits) {
		err = ERANGE;
		goto errout;
	}

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_mcr),
	    &me->me_q.q_mcr, 0)) {
		err = ENOMEM;
		goto errout;
	}
	mcr = (struct ubsec_mcr *)me->me_q.q_mcr.dma_vaddr;

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_ctx_modexp),
	    &me->me_q.q_ctx, 0)) {
		err = ENOMEM;
		goto errout;
	}

	mbits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_M]);
	if (mbits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_M, 0)) {
		err = ENOMEM;
		goto errout;
	}
	ubsec_kshift_r(shiftbits,
	    krp->krp_param[UBS_MODEXP_PAR_M].crp_p, mbits,
	    me->me_M.dma_vaddr, normbits);

	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_C, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(me->me_C.dma_vaddr, 0, me->me_C.dma_size);

	ebits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_E]);
	if (ebits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_E, 0)) {
		err = ENOMEM;
		goto errout;
	}
	ubsec_kshift_r(shiftbits,
	    krp->krp_param[UBS_MODEXP_PAR_E].crp_p, ebits,
	    me->me_E.dma_vaddr, normbits);

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_pktbuf),
	    &me->me_epb, 0)) {
		err = ENOMEM;
		goto errout;
	}
	epb = (struct ubsec_pktbuf *)me->me_epb.dma_vaddr;
	epb->pb_addr = htole32(me->me_E.dma_paddr);
	epb->pb_next = 0;
	epb->pb_len = htole32(normbits / 8);

#ifdef UBSEC_DEBUG
	if (ubsec_debug) {
		printf("Epb ");
		ubsec_dump_pb(epb);
	}
#endif

	mcr->mcr_pkts = htole16(1);
	mcr->mcr_flags = 0;
	mcr->mcr_cmdctxp = htole32(me->me_q.q_ctx.dma_paddr);
	mcr->mcr_reserved = 0;
	mcr->mcr_pktlen = 0;

	mcr->mcr_ipktbuf.pb_addr = htole32(me->me_M.dma_paddr);
	mcr->mcr_ipktbuf.pb_len = htole32(normbits / 8);
	mcr->mcr_ipktbuf.pb_next = htole32(me->me_epb.dma_paddr);

	mcr->mcr_opktbuf.pb_addr = htole32(me->me_C.dma_paddr);
	mcr->mcr_opktbuf.pb_next = 0;
	mcr->mcr_opktbuf.pb_len = htole32(normbits / 8);

#ifdef DIAGNOSTIC
	/* Misaligned output buffer will hang the chip. */
	if ((letoh32(mcr->mcr_opktbuf.pb_addr) & 3) != 0)
		panic("%s: modexp invalid addr 0x%x",
		    device_xname(sc->sc_dev), letoh32(mcr->mcr_opktbuf.pb_addr));
	if ((letoh32(mcr->mcr_opktbuf.pb_len) & 3) != 0)
		panic("%s: modexp invalid len 0x%x",
		    device_xname(sc->sc_dev), letoh32(mcr->mcr_opktbuf.pb_len));
#endif

	ctx = (struct ubsec_ctx_modexp *)me->me_q.q_ctx.dma_vaddr;
	memset(ctx, 0, sizeof(*ctx));
	ubsec_kshift_r(shiftbits,
	    krp->krp_param[UBS_MODEXP_PAR_N].crp_p, nbits,
	    ctx->me_N, normbits);
	ctx->me_len = htole16((normbits / 8) + (4 * sizeof(u_int16_t)));
	ctx->me_op = htole16(UBS_CTXOP_MODEXP);
	ctx->me_E_len = htole16(nbits);
	ctx->me_N_len = htole16(nbits);

#ifdef UBSEC_DEBUG
	if (ubsec_debug) {
		ubsec_dump_mcr(mcr);
		ubsec_dump_ctx2((struct ubsec_ctx_keyop *)ctx);
	}
#endif

	/*
	 * ubsec_feed2 will sync mcr and ctx, we just need to sync
	 * everything else.
	 */
	bus_dmamap_sync(sc->sc_dmat, me->me_M.dma_map,
	    0, me->me_M.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, me->me_E.dma_map,
	    0, me->me_E.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, me->me_C.dma_map,
	    0, me->me_C.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->sc_dmat, me->me_epb.dma_map,
	    0, me->me_epb.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	/* Enqueue and we're done... */
	mutex_spin_enter(&sc->sc_mtx);
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue2, &me->me_q, q_next);
	ubsec_feed2(sc);
	ubsecstats.hst_modexp++;
	mutex_spin_exit(&sc->sc_mtx);

	return (0);

errout:
	if (me != NULL) {
		if (me->me_q.q_mcr.dma_map != NULL)
			ubsec_dma_free(sc, &me->me_q.q_mcr);
		if (me->me_q.q_ctx.dma_map != NULL) {
			memset(me->me_q.q_ctx.dma_vaddr, 0, me->me_q.q_ctx.dma_size);
			ubsec_dma_free(sc, &me->me_q.q_ctx);
		}
		if (me->me_M.dma_map != NULL) {
			memset(me->me_M.dma_vaddr, 0, me->me_M.dma_size);
			ubsec_dma_free(sc, &me->me_M);
		}
		if (me->me_E.dma_map != NULL) {
			memset(me->me_E.dma_vaddr, 0, me->me_E.dma_size);
			ubsec_dma_free(sc, &me->me_E);
		}
		if (me->me_C.dma_map != NULL) {
			memset(me->me_C.dma_vaddr, 0, me->me_C.dma_size);
			ubsec_dma_free(sc, &me->me_C);
		}
		if (me->me_epb.dma_map != NULL)
			ubsec_dma_free(sc, &me->me_epb);
		free(me, M_DEVBUF);
	}
	krp->krp_status = err;
	crypto_kdone(krp);
	return (0);
}

/*
 * Start computation of cr[C] = (cr[M] ^ cr[E]) mod cr[N] (hw normalization)
 */
static int
ubsec_kprocess_modexp_hw(struct ubsec_softc *sc, struct cryptkop *krp,
			 int hint)
{
	struct ubsec_q2_modexp *me;
	struct ubsec_mcr *mcr;
	struct ubsec_ctx_modexp *ctx;
	struct ubsec_pktbuf *epb;
	int err = 0;
	u_int nbits, normbits, mbits, shiftbits, ebits;

	me = (struct ubsec_q2_modexp *)malloc(sizeof *me, M_DEVBUF, M_NOWAIT);
	if (me == NULL) {
		err = ENOMEM;
		goto errout;
	}
	memset(me, 0, sizeof *me);
	me->me_krp = krp;
	me->me_q.q_type = UBS_CTXOP_MODEXP;

	nbits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_N]);
	if (nbits <= 512)
		normbits = 512;
	else if (nbits <= 768)
		normbits = 768;
	else if (nbits <= 1024)
		normbits = 1024;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && nbits <= 1536)
		normbits = 1536;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && nbits <= 2048)
		normbits = 2048;
	else {
		err = E2BIG;
		goto errout;
	}

	shiftbits = normbits - nbits;

	/* XXX ??? */
	me->me_modbits = nbits;
	me->me_shiftbits = shiftbits;
	me->me_normbits = normbits;

	/* Sanity check: result bits must be >= true modulus bits. */
	if (krp->krp_param[krp->krp_iparams].crp_nbits < nbits) {
		err = ERANGE;
		goto errout;
	}

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_mcr),
	    &me->me_q.q_mcr, 0)) {
		err = ENOMEM;
		goto errout;
	}
	mcr = (struct ubsec_mcr *)me->me_q.q_mcr.dma_vaddr;

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_ctx_modexp),
	    &me->me_q.q_ctx, 0)) {
		err = ENOMEM;
		goto errout;
	}

	mbits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_M]);
	if (mbits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_M, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(me->me_M.dma_vaddr, 0, normbits / 8);
	bcopy(krp->krp_param[UBS_MODEXP_PAR_M].crp_p,
	    me->me_M.dma_vaddr, (mbits + 7) / 8);

	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_C, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(me->me_C.dma_vaddr, 0, me->me_C.dma_size);

	ebits = ubsec_ksigbits(&krp->krp_param[UBS_MODEXP_PAR_E]);
	if (ebits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, normbits / 8, &me->me_E, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(me->me_E.dma_vaddr, 0, normbits / 8);
	bcopy(krp->krp_param[UBS_MODEXP_PAR_E].crp_p,
	    me->me_E.dma_vaddr, (ebits + 7) / 8);

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_pktbuf),
	    &me->me_epb, 0)) {
		err = ENOMEM;
		goto errout;
	}
	epb = (struct ubsec_pktbuf *)me->me_epb.dma_vaddr;
	epb->pb_addr = htole32(me->me_E.dma_paddr);
	epb->pb_next = 0;
	epb->pb_len = htole32((ebits + 7) / 8);

#ifdef UBSEC_DEBUG
	if (ubsec_debug) {
		printf("Epb ");
		ubsec_dump_pb(epb);
	}
#endif

	mcr->mcr_pkts = htole16(1);
	mcr->mcr_flags = 0;
	mcr->mcr_cmdctxp = htole32(me->me_q.q_ctx.dma_paddr);
	mcr->mcr_reserved = 0;
	mcr->mcr_pktlen = 0;

	mcr->mcr_ipktbuf.pb_addr = htole32(me->me_M.dma_paddr);
	mcr->mcr_ipktbuf.pb_len = htole32(normbits / 8);
	mcr->mcr_ipktbuf.pb_next = htole32(me->me_epb.dma_paddr);

	mcr->mcr_opktbuf.pb_addr = htole32(me->me_C.dma_paddr);
	mcr->mcr_opktbuf.pb_next = 0;
	mcr->mcr_opktbuf.pb_len = htole32(normbits / 8);

#ifdef DIAGNOSTIC
	/* Misaligned output buffer will hang the chip. */
	if ((letoh32(mcr->mcr_opktbuf.pb_addr) & 3) != 0)
		panic("%s: modexp invalid addr 0x%x",
		    device_xname(sc->sc_dev), letoh32(mcr->mcr_opktbuf.pb_addr));
	if ((letoh32(mcr->mcr_opktbuf.pb_len) & 3) != 0)
		panic("%s: modexp invalid len 0x%x",
		    device_xname(sc->sc_dev), letoh32(mcr->mcr_opktbuf.pb_len));
#endif

	ctx = (struct ubsec_ctx_modexp *)me->me_q.q_ctx.dma_vaddr;
	memset(ctx, 0, sizeof(*ctx));
	memcpy(ctx->me_N, krp->krp_param[UBS_MODEXP_PAR_N].crp_p,
	    (nbits + 7) / 8);
	ctx->me_len = htole16((normbits / 8) + (4 * sizeof(u_int16_t)));
	ctx->me_op = htole16(UBS_CTXOP_MODEXP);
	ctx->me_E_len = htole16(ebits);
	ctx->me_N_len = htole16(nbits);

#ifdef UBSEC_DEBUG
	if (ubsec_debug) {
		ubsec_dump_mcr(mcr);
		ubsec_dump_ctx2((struct ubsec_ctx_keyop *)ctx);
	}
#endif

	/*
	 * ubsec_feed2 will sync mcr and ctx, we just need to sync
	 * everything else.
	 */
	bus_dmamap_sync(sc->sc_dmat, me->me_M.dma_map,
	    0, me->me_M.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, me->me_E.dma_map,
	    0, me->me_E.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, me->me_C.dma_map,
	    0, me->me_C.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->sc_dmat, me->me_epb.dma_map,
	    0, me->me_epb.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

	/* Enqueue and we're done... */
	mutex_spin_enter(&sc->sc_mtx);
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue2, &me->me_q, q_next);
	ubsec_feed2(sc);
	mutex_spin_exit(&sc->sc_mtx);

	return (0);

errout:
	if (me != NULL) {
		if (me->me_q.q_mcr.dma_map != NULL)
			ubsec_dma_free(sc, &me->me_q.q_mcr);
		if (me->me_q.q_ctx.dma_map != NULL) {
			memset(me->me_q.q_ctx.dma_vaddr, 0, me->me_q.q_ctx.dma_size);
			ubsec_dma_free(sc, &me->me_q.q_ctx);
		}
		if (me->me_M.dma_map != NULL) {
			memset(me->me_M.dma_vaddr, 0, me->me_M.dma_size);
			ubsec_dma_free(sc, &me->me_M);
		}
		if (me->me_E.dma_map != NULL) {
			memset(me->me_E.dma_vaddr, 0, me->me_E.dma_size);
			ubsec_dma_free(sc, &me->me_E);
		}
		if (me->me_C.dma_map != NULL) {
			memset(me->me_C.dma_vaddr, 0, me->me_C.dma_size);
			ubsec_dma_free(sc, &me->me_C);
		}
		if (me->me_epb.dma_map != NULL)
			ubsec_dma_free(sc, &me->me_epb);
		free(me, M_DEVBUF);
	}
	krp->krp_status = err;
	crypto_kdone(krp);
	return (0);
}

static int
ubsec_kprocess_rsapriv(struct ubsec_softc *sc, struct cryptkop *krp,
		       int hint)
{
	struct ubsec_q2_rsapriv *rp = NULL;
	struct ubsec_mcr *mcr;
	struct ubsec_ctx_rsapriv *ctx;
	int err = 0;
	u_int padlen, msglen;

	msglen = ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_P]);
	padlen = ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_Q]);
	if (msglen > padlen)
		padlen = msglen;

	if (padlen <= 256)
		padlen = 256;
	else if (padlen <= 384)
		padlen = 384;
	else if (padlen <= 512)
		padlen = 512;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && padlen <= 768)
		padlen = 768;
	else if (sc->sc_flags & UBS_FLAGS_BIGKEY && padlen <= 1024)
		padlen = 1024;
	else {
		err = E2BIG;
		goto errout;
	}

	if (ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_DP]) > padlen) {
		err = E2BIG;
		goto errout;
	}

	if (ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_DQ]) > padlen) {
		err = E2BIG;
		goto errout;
	}

	if (ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_PINV]) > padlen) {
		err = E2BIG;
		goto errout;
	}

	rp = malloc(sizeof *rp, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (rp == NULL)
		return (ENOMEM);
	rp->rpr_krp = krp;
	rp->rpr_q.q_type = UBS_CTXOP_RSAPRIV;

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_mcr),
	    &rp->rpr_q.q_mcr, 0)) {
		err = ENOMEM;
		goto errout;
	}
	mcr = (struct ubsec_mcr *)rp->rpr_q.q_mcr.dma_vaddr;

	if (ubsec_dma_malloc(sc, sizeof(struct ubsec_ctx_rsapriv),
	    &rp->rpr_q.q_ctx, 0)) {
		err = ENOMEM;
		goto errout;
	}
	ctx = (struct ubsec_ctx_rsapriv *)rp->rpr_q.q_ctx.dma_vaddr;
	memset(ctx, 0, sizeof *ctx);

	/* Copy in p */
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_P].crp_p,
	    &ctx->rpr_buf[0 * (padlen / 8)],
	    (krp->krp_param[UBS_RSAPRIV_PAR_P].crp_nbits + 7) / 8);

	/* Copy in q */
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_Q].crp_p,
	    &ctx->rpr_buf[1 * (padlen / 8)],
	    (krp->krp_param[UBS_RSAPRIV_PAR_Q].crp_nbits + 7) / 8);

	/* Copy in dp */
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_DP].crp_p,
	    &ctx->rpr_buf[2 * (padlen / 8)],
	    (krp->krp_param[UBS_RSAPRIV_PAR_DP].crp_nbits + 7) / 8);

	/* Copy in dq */
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_DQ].crp_p,
	    &ctx->rpr_buf[3 * (padlen / 8)],
	    (krp->krp_param[UBS_RSAPRIV_PAR_DQ].crp_nbits + 7) / 8);

	/* Copy in pinv */
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_PINV].crp_p,
	    &ctx->rpr_buf[4 * (padlen / 8)],
	    (krp->krp_param[UBS_RSAPRIV_PAR_PINV].crp_nbits + 7) / 8);

	msglen = padlen * 2;

	/* Copy in input message (aligned buffer/length). */
	if (ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_MSGIN]) > msglen) {
		/* Is this likely? */
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, (msglen + 7) / 8, &rp->rpr_msgin, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(rp->rpr_msgin.dma_vaddr, 0, (msglen + 7) / 8);
	bcopy(krp->krp_param[UBS_RSAPRIV_PAR_MSGIN].crp_p,
	    rp->rpr_msgin.dma_vaddr,
	    (krp->krp_param[UBS_RSAPRIV_PAR_MSGIN].crp_nbits + 7) / 8);

	/* Prepare space for output message (aligned buffer/length). */
	if (ubsec_ksigbits(&krp->krp_param[UBS_RSAPRIV_PAR_MSGOUT]) < msglen) {
		/* Is this likely? */
		err = E2BIG;
		goto errout;
	}
	if (ubsec_dma_malloc(sc, (msglen + 7) / 8, &rp->rpr_msgout, 0)) {
		err = ENOMEM;
		goto errout;
	}
	memset(rp->rpr_msgout.dma_vaddr, 0, (msglen + 7) / 8);

	mcr->mcr_pkts = htole16(1);
	mcr->mcr_flags = 0;
	mcr->mcr_cmdctxp = htole32(rp->rpr_q.q_ctx.dma_paddr);
	mcr->mcr_ipktbuf.pb_addr = htole32(rp->rpr_msgin.dma_paddr);
	mcr->mcr_ipktbuf.pb_next = 0;
	mcr->mcr_ipktbuf.pb_len = htole32(rp->rpr_msgin.dma_size);
	mcr->mcr_reserved = 0;
	mcr->mcr_pktlen = htole16(msglen);
	mcr->mcr_opktbuf.pb_addr = htole32(rp->rpr_msgout.dma_paddr);
	mcr->mcr_opktbuf.pb_next = 0;
	mcr->mcr_opktbuf.pb_len = htole32(rp->rpr_msgout.dma_size);

#ifdef DIAGNOSTIC
	if (rp->rpr_msgin.dma_paddr & 3 || rp->rpr_msgin.dma_size & 3) {
		panic("%s: rsapriv: invalid msgin 0x%lx(0x%lx)",
		    device_xname(sc->sc_dev), (u_long) rp->rpr_msgin.dma_paddr,
		    (u_long) rp->rpr_msgin.dma_size);
	}
	if (rp->rpr_msgout.dma_paddr & 3 || rp->rpr_msgout.dma_size & 3) {
		panic("%s: rsapriv: invalid msgout 0x%lx(0x%lx)",
		    device_xname(sc->sc_dev), (u_long) rp->rpr_msgout.dma_paddr,
		    (u_long) rp->rpr_msgout.dma_size);
	}
#endif

	ctx->rpr_len = (sizeof(u_int16_t) * 4) + (5 * (padlen / 8));
	ctx->rpr_op = htole16(UBS_CTXOP_RSAPRIV);
	ctx->rpr_q_len = htole16(padlen);
	ctx->rpr_p_len = htole16(padlen);

	/*
	 * ubsec_feed2 will sync mcr and ctx, we just need to sync
	 * everything else.
	 */
	bus_dmamap_sync(sc->sc_dmat, rp->rpr_msgin.dma_map,
	    0, rp->rpr_msgin.dma_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, rp->rpr_msgout.dma_map,
	    0, rp->rpr_msgout.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	/* Enqueue and we're done... */
	mutex_spin_enter(&sc->sc_mtx);
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue2, &rp->rpr_q, q_next);
	ubsec_feed2(sc);
	ubsecstats.hst_modexpcrt++;
	mutex_spin_exit(&sc->sc_mtx);
	return (0);

errout:
	if (rp != NULL) {
		if (rp->rpr_q.q_mcr.dma_map != NULL)
			ubsec_dma_free(sc, &rp->rpr_q.q_mcr);
		if (rp->rpr_msgin.dma_map != NULL) {
			memset(rp->rpr_msgin.dma_vaddr, 0, rp->rpr_msgin.dma_size);
			ubsec_dma_free(sc, &rp->rpr_msgin);
		}
		if (rp->rpr_msgout.dma_map != NULL) {
			memset(rp->rpr_msgout.dma_vaddr, 0, rp->rpr_msgout.dma_size);
			ubsec_dma_free(sc, &rp->rpr_msgout);
		}
		free(rp, M_DEVBUF);
	}
	krp->krp_status = err;
	crypto_kdone(krp);
	return (0);
}

#ifdef UBSEC_DEBUG
static void
ubsec_dump_pb(volatile struct ubsec_pktbuf *pb)
{
	printf("addr 0x%x (0x%x) next 0x%x\n",
	    pb->pb_addr, pb->pb_len, pb->pb_next);
}

static void
ubsec_dump_ctx2(volatile struct ubsec_ctx_keyop *c)
{
	printf("CTX (0x%x):\n", c->ctx_len);
	switch (letoh16(c->ctx_op)) {
	case UBS_CTXOP_RNGBYPASS:
	case UBS_CTXOP_RNGSHA1:
		break;
	case UBS_CTXOP_MODEXP:
	{
		struct ubsec_ctx_modexp *cx = (void *)c;
		int i, len;

		printf(" Elen %u, Nlen %u\n",
		    letoh16(cx->me_E_len), letoh16(cx->me_N_len));
		len = (cx->me_N_len + 7)/8;
		for (i = 0; i < len; i++)
			printf("%s%02x", (i == 0) ? " N: " : ":", cx->me_N[i]);
		printf("\n");
		break;
	}
	default:
		printf("unknown context: %x\n", c->ctx_op);
	}
	printf("END CTX\n");
}

static void
ubsec_dump_mcr(struct ubsec_mcr *mcr)
{
	volatile struct ubsec_mcr_add *ma;
	int i;

	printf("MCR:\n");
	printf(" pkts: %u, flags 0x%x\n",
	    letoh16(mcr->mcr_pkts), letoh16(mcr->mcr_flags));
	ma = (volatile struct ubsec_mcr_add *)&mcr->mcr_cmdctxp;
	for (i = 0; i < letoh16(mcr->mcr_pkts); i++) {
		printf(" %d: ctx 0x%x len 0x%x rsvd 0x%x\n", i,
		    letoh32(ma->mcr_cmdctxp), letoh16(ma->mcr_pktlen),
		    letoh16(ma->mcr_reserved));
		printf(" %d: ipkt ", i);
		ubsec_dump_pb(&ma->mcr_ipktbuf);
		printf(" %d: opkt ", i);
		ubsec_dump_pb(&ma->mcr_opktbuf);
		ma++;
	}
	printf("END MCR\n");
}
#endif /* UBSEC_DEBUG */

/*
 * Return the number of significant bits of a big number.
 */
static int
ubsec_ksigbits(struct crparam *cr)
{
	u_int plen = (cr->crp_nbits + 7) / 8;
	int i, sig = plen * 8;
	u_int8_t c, *p = cr->crp_p;

	for (i = plen - 1; i >= 0; i--) {
		c = p[i];
		if (c != 0) {
			while ((c & 0x80) == 0) {
				sig--;
				c <<= 1;
			}
			break;
		}
		sig -= 8;
	}
	return (sig);
}

static void
ubsec_kshift_r(u_int shiftbits, u_int8_t *src, u_int srcbits,
    u_int8_t *dst, u_int dstbits)
{
	u_int slen, dlen;
	int i, si, di, n;

	slen = (srcbits + 7) / 8;
	dlen = (dstbits + 7) / 8;

	for (i = 0; i < slen; i++)
		dst[i] = src[i];
	for (i = 0; i < dlen - slen; i++)
		dst[slen + i] = 0;

	n = shiftbits / 8;
	if (n != 0) {
		si = dlen - n - 1;
		di = dlen - 1;
		while (si >= 0)
			dst[di--] = dst[si--];
		while (di >= 0)
			dst[di--] = 0;
	}

	n = shiftbits % 8;
	if (n != 0) {
		for (i = dlen - 1; i > 0; i--)
			dst[i] = (dst[i] << n) |
			    (dst[i - 1] >> (8 - n));
		dst[0] = dst[0] << n;
	}
}

static void
ubsec_kshift_l(u_int shiftbits, u_int8_t *src, u_int srcbits,
    u_int8_t *dst, u_int dstbits)
{
	int slen, dlen, i, n;

	slen = (srcbits + 7) / 8;
	dlen = (dstbits + 7) / 8;

	n = shiftbits / 8;
	for (i = 0; i < slen; i++)
		dst[i] = src[i + n];
	for (i = 0; i < dlen - slen; i++)
		dst[slen + i] = 0;

	n = shiftbits % 8;
	if (n != 0) {
		for (i = 0; i < (dlen - 1); i++)
			dst[i] = (dst[i] >> n) | (dst[i + 1] << (8 - n));
		dst[dlen - 1] = dst[dlen - 1] >> n;
	}
}

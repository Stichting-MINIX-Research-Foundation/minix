/*	$NetBSD: mvxpsec.c,v 1.1 2015/06/03 04:20:02 hsuenaga Exp $	*/
/*
 * Copyright (c) 2015 Internet Initiative Japan Inc.
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
 */
/*
 * Cryptographic Engine and Security Accelerator(MVXPSEC)
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/evcnt.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/callout.h>
#include <sys/pool.h>
#include <sys/cprng.h>
#include <sys/syslog.h>
#include <sys/mutex.h>
#include <sys/kthread.h>
#include <sys/atomic.h>
#include <sys/sha1.h>
#include <sys/md5.h>

#include <uvm/uvm_extern.h>

#include <crypto/rijndael/rijndael.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include <net/net_stats.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <netipsec/esp_var.h>

#include <arm/cpufunc.h>
#include <arm/marvell/mvsocvar.h>
#include <arm/marvell/armadaxpreg.h>
#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>
#include <dev/marvell/mvxpsecreg.h>
#include <dev/marvell/mvxpsecvar.h>

#ifdef DEBUG
#define STATIC __attribute__ ((noinline)) extern
#define _STATIC __attribute__ ((noinline)) extern
#define INLINE __attribute__ ((noinline)) extern
#define _INLINE __attribute__ ((noinline)) extern
#else
#define STATIC static
#define _STATIC __attribute__ ((unused)) static
#define INLINE static inline
#define _INLINE __attribute__ ((unused)) static inline
#endif

/*
 * IRQ and SRAM spaces for each of unit
 * XXX: move to attach_args
 */
struct {
	int		err_int;
} mvxpsec_config[] = {
	{ .err_int = ARMADAXP_IRQ_CESA0_ERR, }, /* unit 0 */
	{ .err_int = ARMADAXP_IRQ_CESA1_ERR, }, /* unit 1 */
};
#define MVXPSEC_ERR_INT(sc) \
    mvxpsec_config[device_unit((sc)->sc_dev)].err_int

/*
 * AES
 */
#define MAXBC				(128/32)
#define MAXKC				(256/32)
#define MAXROUNDS			14
STATIC int mv_aes_ksched(uint8_t[4][MAXKC], int,
    uint8_t[MAXROUNDS+1][4][MAXBC]);
STATIC int mv_aes_deckey(uint8_t *, uint8_t *, int);

/*
 * device driver autoconf interface
 */
STATIC int mvxpsec_match(device_t, cfdata_t, void *);
STATIC void mvxpsec_attach(device_t, device_t, void *);
STATIC void mvxpsec_evcnt_attach(struct mvxpsec_softc *);

/*
 * register setup
 */
STATIC int mvxpsec_wininit(struct mvxpsec_softc *, enum marvell_tags *);

/*
 * timer(callout) interface
 *
 * XXX: callout is not MP safe...
 */
STATIC void mvxpsec_timer(void *);

/*
 * interrupt interface
 */
STATIC int mvxpsec_intr(void *);
INLINE void mvxpsec_intr_cleanup(struct mvxpsec_softc *);
STATIC int mvxpsec_eintr(void *);
STATIC uint32_t mvxpsec_intr_ack(struct mvxpsec_softc *);
STATIC uint32_t mvxpsec_eintr_ack(struct mvxpsec_softc *);
INLINE void mvxpsec_intr_cnt(struct mvxpsec_softc *, int);

/*
 * memory allocators and VM management
 */
STATIC struct mvxpsec_devmem *mvxpsec_alloc_devmem(struct mvxpsec_softc *,
    paddr_t, int);
STATIC int mvxpsec_init_sram(struct mvxpsec_softc *);

/*
 * Low-level DMA interface
 */
STATIC int mvxpsec_init_dma(struct mvxpsec_softc *,
    struct marvell_attach_args *);
INLINE int mvxpsec_dma_wait(struct mvxpsec_softc *);
INLINE int mvxpsec_acc_wait(struct mvxpsec_softc *);
INLINE struct mvxpsec_descriptor_handle *mvxpsec_dma_getdesc(struct mvxpsec_softc *);
_INLINE void mvxpsec_dma_putdesc(struct mvxpsec_softc *, struct mvxpsec_descriptor_handle *);
INLINE void mvxpsec_dma_setup(struct mvxpsec_descriptor_handle *,
    uint32_t, uint32_t, uint32_t);
INLINE void mvxpsec_dma_cat(struct mvxpsec_softc *,
    struct mvxpsec_descriptor_handle *, struct mvxpsec_descriptor_handle *);

/*
 * High-level DMA interface
 */
INLINE int mvxpsec_dma_copy0(struct mvxpsec_softc *,
    mvxpsec_dma_ring *, uint32_t, uint32_t, uint32_t);
INLINE int mvxpsec_dma_copy(struct mvxpsec_softc *,
    mvxpsec_dma_ring *, uint32_t, uint32_t, uint32_t);
INLINE int mvxpsec_dma_acc_activate(struct mvxpsec_softc *,
    mvxpsec_dma_ring *);
INLINE void mvxpsec_dma_finalize(struct mvxpsec_softc *,
    mvxpsec_dma_ring *);
INLINE void mvxpsec_dma_free(struct mvxpsec_softc *,
    mvxpsec_dma_ring *);
INLINE int mvxpsec_dma_copy_packet(struct mvxpsec_softc *, struct mvxpsec_packet *);
INLINE int mvxpsec_dma_sync_packet(struct mvxpsec_softc *, struct mvxpsec_packet *);

/*
 * Session management interface (OpenCrypto)
 */
#define MVXPSEC_SESSION(sid)	((sid) & 0x0fffffff)
#define MVXPSEC_SID(crd, sesn)	(((crd) << 28) | ((sesn) & 0x0fffffff))
/* pool management */
STATIC int mvxpsec_session_ctor(void *, void *, int);
STATIC void mvxpsec_session_dtor(void *, void *);
STATIC int mvxpsec_packet_ctor(void *, void *, int);
STATIC void mvxpsec_packet_dtor(void *, void *);

/* session management */
STATIC struct mvxpsec_session *mvxpsec_session_alloc(struct mvxpsec_softc *);
STATIC void mvxpsec_session_dealloc(struct mvxpsec_session *);
INLINE struct mvxpsec_session *mvxpsec_session_lookup(struct mvxpsec_softc *, int);
INLINE int mvxpsec_session_ref(struct mvxpsec_session *);
INLINE void mvxpsec_session_unref(struct mvxpsec_session *);

/* packet management */
STATIC struct mvxpsec_packet *mvxpsec_packet_alloc(struct mvxpsec_session *);
INLINE void mvxpsec_packet_enqueue(struct mvxpsec_packet *);
STATIC void mvxpsec_packet_dealloc(struct mvxpsec_packet *);
STATIC int mvxpsec_done_packet(struct mvxpsec_packet *);

/* session header manegement */
STATIC int mvxpsec_header_finalize(struct mvxpsec_packet *);

/* packet queue management */
INLINE void mvxpsec_drop(struct mvxpsec_softc *, struct cryptop *, struct mvxpsec_packet *, int);
STATIC int mvxpsec_dispatch_queue(struct mvxpsec_softc *);

/* opencrypto opration */
INLINE int mvxpsec_parse_crd(struct mvxpsec_packet *, struct cryptodesc *);
INLINE int mvxpsec_parse_crp(struct mvxpsec_packet *);

/* payload data management */
INLINE int mvxpsec_packet_setcrp(struct mvxpsec_packet *, struct cryptop *);
STATIC int mvxpsec_packet_setdata(struct mvxpsec_packet *, void *, uint32_t);
STATIC int mvxpsec_packet_setmbuf(struct mvxpsec_packet *, struct mbuf *);
STATIC int mvxpsec_packet_setuio(struct mvxpsec_packet *, struct uio *);
STATIC int mvxpsec_packet_rdata(struct mvxpsec_packet *, int, int, void *);
_STATIC int mvxpsec_packet_wdata(struct mvxpsec_packet *, int, int, void *);
STATIC int mvxpsec_packet_write_iv(struct mvxpsec_packet *, void *, int);
STATIC int mvxpsec_packet_copy_iv(struct mvxpsec_packet *, int, int);

/* key pre-computation */
STATIC int mvxpsec_key_precomp(int, void *, int, void *, void *);
STATIC int mvxpsec_hmac_precomp(int, void *, int, void *, void *);

/* crypto operation management */
INLINE void mvxpsec_packet_reset_op(struct mvxpsec_packet *);
INLINE void mvxpsec_packet_update_op_order(struct mvxpsec_packet *, int);

/*
 * parameter converters
 */
INLINE uint32_t mvxpsec_alg2acc(uint32_t alg);
INLINE uint32_t mvxpsec_aesklen(int klen);

/*
 * string formatters
 */
_STATIC const char *s_ctrlreg(uint32_t);
_STATIC const char *s_winreg(uint32_t);
_STATIC const char *s_errreg(uint32_t);
_STATIC const char *s_xpsecintr(uint32_t);
_STATIC const char *s_ctlalg(uint32_t);
_STATIC const char *s_xpsec_op(uint32_t);
_STATIC const char *s_xpsec_enc(uint32_t);
_STATIC const char *s_xpsec_mac(uint32_t);
_STATIC const char *s_xpsec_frag(uint32_t);

/*
 * debugging supports
 */
#ifdef MVXPSEC_DEBUG
_STATIC void mvxpsec_dump_dmaq(struct mvxpsec_descriptor_handle *);
_STATIC void mvxpsec_dump_reg(struct mvxpsec_softc *);
_STATIC void mvxpsec_dump_sram(const char *, struct mvxpsec_softc *, size_t);
_STATIC void mvxpsec_dump_data(const char *, void *, size_t);

_STATIC void mvxpsec_dump_packet(const char *, struct mvxpsec_packet *);
_STATIC void mvxpsec_dump_packet_data(const char *, struct mvxpsec_packet *);
_STATIC void mvxpsec_dump_packet_desc(const char *, struct mvxpsec_packet *);

_STATIC void mvxpsec_dump_acc_config(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_encdata(const char *, uint32_t, uint32_t);
_STATIC void mvxpsec_dump_acc_enclen(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_enckey(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_enciv(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_macsrc(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_macdst(const char *, uint32_t);
_STATIC void mvxpsec_dump_acc_maciv(const char *, uint32_t);
#endif

/*
 * global configurations, params, work spaces, ...
 *
 * XXX: use sysctl for global configurations
 */
/* waiting for device */
static int mvxpsec_wait_interval = 10;		/* usec */
static int mvxpsec_wait_retry = 100;		/* times = wait for 1 [msec] */
#ifdef MVXPSEC_DEBUG
static uint32_t mvxpsec_debug = MVXPSEC_DEBUG;	/* debug level */
#endif

/*
 * Register accessors
 */
#define MVXPSEC_WRITE(sc, off, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (off), (val))
#define MVXPSEC_READ(sc, off) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (off))

/*
 * device driver autoconf interface
 */
CFATTACH_DECL2_NEW(mvxpsec_mbus, sizeof(struct mvxpsec_softc),
    mvxpsec_match, mvxpsec_attach, NULL, NULL, NULL, NULL);

STATIC int
mvxpsec_match(device_t dev, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;
	uint32_t tag;
	int window;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT)
		return 0;

	switch (mva->mva_unit) {
	case 0:
		tag = ARMADAXP_TAG_CRYPT0;
		break;
	case 1:
		tag = ARMADAXP_TAG_CRYPT1;
		break;
	default:
		aprint_error_dev(dev,
		    "unit %d is not supported\n", mva->mva_unit);
		return 0;
	}

	window = mvsoc_target(tag, NULL, NULL, NULL, NULL);
	if (window >= nwindow) {
		aprint_error_dev(dev,
		    "Security Accelerator SRAM is not configured.\n");
		return 0;
	}

	return 1;
}

STATIC void
mvxpsec_attach(device_t parent, device_t self, void *aux)
{
	struct marvell_attach_args *mva = aux;
	struct mvxpsec_softc *sc = device_private(self);
	int v;
	int i;

	sc->sc_dev = self;

	aprint_normal(": Marvell Crypto Engines and Security Accelerator\n");
	aprint_naive("\n");
#ifdef MVXPSEC_MULTI_PACKET
	aprint_normal_dev(sc->sc_dev, "multi-packet chained mode enabled.\n");
#else
	aprint_normal_dev(sc->sc_dev, "multi-packet chained mode disabled.\n");
#endif
	aprint_normal_dev(sc->sc_dev,
	    "Max %d sessions.\n", MVXPSEC_MAX_SESSIONS);

	/* mutex */
	mutex_init(&sc->sc_session_mtx, MUTEX_DEFAULT, IPL_NET);
	mutex_init(&sc->sc_dma_mtx, MUTEX_DEFAULT, IPL_NET);
	mutex_init(&sc->sc_queue_mtx, MUTEX_DEFAULT, IPL_NET);

	/* Packet queue */
	SIMPLEQ_INIT(&sc->sc_wait_queue);
	SIMPLEQ_INIT(&sc->sc_run_queue);
	SLIST_INIT(&sc->sc_free_list);
	sc->sc_wait_qlen = 0;
#ifdef MVXPSEC_MULTI_PACKET
	sc->sc_wait_qlimit = 16;
#else
	sc->sc_wait_qlimit = 0;
#endif
	sc->sc_free_qlen = 0;

	/* Timer */
	callout_init(&sc->sc_timeout, 0); /* XXX: use CALLOUT_MPSAFE */
	callout_setfunc(&sc->sc_timeout, mvxpsec_timer, sc);

	/* I/O */
	sc->sc_iot = mva->mva_iot;
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh,
	    mva->mva_offset, mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "Cannot map registers\n");
		return;
	}

	/* DMA */
	sc->sc_dmat = mva->mva_dmat;
	if (mvxpsec_init_dma(sc, mva) < 0)
		return;

	/* SRAM */
	if (mvxpsec_init_sram(sc) < 0)
		return;

	/* Registers */
	mvxpsec_wininit(sc, mva->mva_tags);

	/* INTR */
	MVXPSEC_WRITE(sc, MVXPSEC_INT_MASK, MVXPSEC_DEFAULT_INT);
	MVXPSEC_WRITE(sc, MV_TDMA_ERR_MASK, MVXPSEC_DEFAULT_ERR);
	sc->sc_done_ih = 
	    marvell_intr_establish(mva->mva_irq, IPL_NET, mvxpsec_intr, sc);
	/* XXX: sould pass error IRQ using mva */
	sc->sc_error_ih = marvell_intr_establish(MVXPSEC_ERR_INT(sc),
	    IPL_NET, mvxpsec_eintr, sc);
	aprint_normal_dev(self,
	    "Error Reporting IRQ %d\n", MVXPSEC_ERR_INT(sc));

	/* Initialize TDMA (It's enabled here, but waiting for SA) */
	if (mvxpsec_dma_wait(sc) < 0)
		panic("%s: DMA DEVICE not responding\n", __func__);
	MVXPSEC_WRITE(sc, MV_TDMA_CNT, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_SRC, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_DST, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_NXT, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_CUR, 0);
	v  = MVXPSEC_READ(sc, MV_TDMA_CONTROL);
	v |= MV_TDMA_CONTROL_ENABLE;
	MVXPSEC_WRITE(sc, MV_TDMA_CONTROL, v);

	/* Initialize SA */
	if (mvxpsec_acc_wait(sc) < 0)
		panic("%s: MVXPSEC not responding\n", __func__);
	v  = MVXPSEC_READ(sc, MV_ACC_CONFIG);
	v &= ~MV_ACC_CONFIG_STOP_ON_ERR;
	v |= MV_ACC_CONFIG_MULT_PKT;
	v |= MV_ACC_CONFIG_WAIT_TDMA;
	v |= MV_ACC_CONFIG_ACT_TDMA;
	MVXPSEC_WRITE(sc, MV_ACC_CONFIG, v);
	MVXPSEC_WRITE(sc, MV_ACC_DESC, 0);
	MVXPSEC_WRITE(sc, MV_ACC_COMMAND, MV_ACC_COMMAND_STOP);

	/* Session */
	sc->sc_session_pool = 
	    pool_cache_init(sizeof(struct mvxpsec_session), 0, 0, 0,
	    "mvxpsecpl", NULL, IPL_NET,
	    mvxpsec_session_ctor, mvxpsec_session_dtor, sc);
	pool_cache_sethiwat(sc->sc_session_pool, MVXPSEC_MAX_SESSIONS);
	pool_cache_setlowat(sc->sc_session_pool, MVXPSEC_MAX_SESSIONS / 2);
	sc->sc_last_session = NULL;

	/* Pakcet */
	sc->sc_packet_pool =
	    pool_cache_init(sizeof(struct mvxpsec_session), 0, 0, 0,
	    "mvxpsec_pktpl", NULL, IPL_NET,
	    mvxpsec_packet_ctor, mvxpsec_packet_dtor, sc);
	pool_cache_sethiwat(sc->sc_packet_pool, MVXPSEC_MAX_SESSIONS);
	pool_cache_setlowat(sc->sc_packet_pool, MVXPSEC_MAX_SESSIONS / 2);

	/* Register to EVCNT framework */
	mvxpsec_evcnt_attach(sc);

	/* Register to Opencrypto */
	for (i = 0; i < MVXPSEC_MAX_SESSIONS; i++) {
		sc->sc_sessions[i] = NULL;
	}
	if (mvxpsec_register(sc))
		panic("cannot initialize OpenCrypto module.\n");

	return;
}

STATIC void
mvxpsec_evcnt_attach(struct mvxpsec_softc *sc)
{
	struct mvxpsec_evcnt *sc_ev = &sc->sc_ev;

	evcnt_attach_dynamic(&sc_ev->intr_all, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "Main Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_auth, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "Auth Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_des, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "DES Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_aes_enc, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "AES-Encrypt Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_aes_dec, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "AES-Decrypt Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_enc, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "Crypto Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_sa, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "SA Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_acctdma, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "AccTDMA Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_comp, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "TDMA-Complete Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_own, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "TDMA-Ownership Intr.");
	evcnt_attach_dynamic(&sc_ev->intr_acctdma_cont, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->sc_dev), "AccTDMA-Continue Intr.");

	evcnt_attach_dynamic(&sc_ev->session_new, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "New-Session");
	evcnt_attach_dynamic(&sc_ev->session_free, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Free-Session");

	evcnt_attach_dynamic(&sc_ev->packet_ok, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Packet-OK");
	evcnt_attach_dynamic(&sc_ev->packet_err, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Packet-ERR");

	evcnt_attach_dynamic(&sc_ev->dispatch_packets, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Packet-Dispatch");
	evcnt_attach_dynamic(&sc_ev->dispatch_queue, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Queue-Dispatch");
	evcnt_attach_dynamic(&sc_ev->queue_full, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Queue-Full");
	evcnt_attach_dynamic(&sc_ev->max_dispatch, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Max-Dispatch");
	evcnt_attach_dynamic(&sc_ev->max_done, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->sc_dev), "Max-Done");
}

/*
 * Register setup
 */
STATIC int mvxpsec_wininit(struct mvxpsec_softc *sc, enum marvell_tags *tags)
{
	device_t pdev = device_parent(sc->sc_dev);
	uint64_t base;
	uint32_t size, reg;
	int window, target, attr, rv, i;

	/* disable all window */
	for (window = 0; window < MV_TDMA_NWINDOW; window++)
	{
		MVXPSEC_WRITE(sc, MV_TDMA_BAR(window), 0);
		MVXPSEC_WRITE(sc, MV_TDMA_ATTR(window), 0);
	}

	for (window = 0, i = 0;
	    tags[i] != MARVELL_TAG_UNDEFINED && window < MV_TDMA_NWINDOW; i++) {
		rv = marvell_winparams_by_tag(pdev, tags[i],
		    &target, &attr, &base, &size);
		if (rv != 0 || size == 0)
			continue;

		if (base > 0xffffffffULL) {
			aprint_error_dev(sc->sc_dev,
			    "can't remap window %d\n", window);
			continue;
		}

		reg  = MV_TDMA_BAR_BASE(base);
		MVXPSEC_WRITE(sc, MV_TDMA_BAR(window), reg);

		reg  = MV_TDMA_ATTR_TARGET(target);
		reg |= MV_TDMA_ATTR_ATTR(attr);
		reg |= MV_TDMA_ATTR_SIZE(size);
		reg |= MV_TDMA_ATTR_ENABLE;
		MVXPSEC_WRITE(sc, MV_TDMA_ATTR(window), reg);

		window++;
	}

	return 0;
}

/*
 * Timer handling
 */
STATIC void
mvxpsec_timer(void *aux)
{
	struct mvxpsec_softc *sc = aux;
	struct mvxpsec_packet *mv_p;
	uint32_t reg;
	int ndone;
	int refill;
	int s;

	/* IPL_SOFTCLOCK */

	log(LOG_ERR, "%s: device timeout.\n", __func__);
#ifdef MVXPSEC_DEBUG
	mvxpsec_dump_reg(sc);
#endif
	
	s = splnet();
	/* stop security accelerator */
	MVXPSEC_WRITE(sc, MV_ACC_COMMAND, MV_ACC_COMMAND_STOP);

	/* stop TDMA */
	MVXPSEC_WRITE(sc, MV_TDMA_CONTROL, 0);

	/* cleanup packet queue */
	mutex_enter(&sc->sc_queue_mtx);
	ndone = 0;
	while ( (mv_p = SIMPLEQ_FIRST(&sc->sc_run_queue)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_run_queue, queue);

		mv_p->crp->crp_etype = EINVAL;
		mvxpsec_done_packet(mv_p);
		ndone++;
	}
	MVXPSEC_EVCNT_MAX(sc, max_done, ndone);
	sc->sc_flags &= ~HW_RUNNING;
	refill = (sc->sc_wait_qlen > 0) ? 1 : 0;
	mutex_exit(&sc->sc_queue_mtx);

	/* reenable TDMA */
	if (mvxpsec_dma_wait(sc) < 0)
		panic("%s: failed to reset DMA DEVICE. give up.", __func__);
	MVXPSEC_WRITE(sc, MV_TDMA_CNT, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_SRC, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_DST, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_CUR, 0);
	MVXPSEC_WRITE(sc, MV_TDMA_NXT, 0);
	reg  = MV_TDMA_DEFAULT_CONTROL;
	reg |= MV_TDMA_CONTROL_ENABLE;
	MVXPSEC_WRITE(sc, MV_TDMA_CONTROL, reg);

	if (mvxpsec_acc_wait(sc) < 0)
		panic("%s: failed to reset MVXPSEC. give up.", __func__);
	reg  = MV_ACC_CONFIG_MULT_PKT;
	reg |= MV_ACC_CONFIG_WAIT_TDMA;
	reg |= MV_ACC_CONFIG_ACT_TDMA;
	MVXPSEC_WRITE(sc, MV_ACC_CONFIG, reg);
	MVXPSEC_WRITE(sc, MV_ACC_DESC, 0);

	if (refill) {
		mutex_enter(&sc->sc_queue_mtx);
		mvxpsec_dispatch_queue(sc);
		mutex_exit(&sc->sc_queue_mtx);
	}

	crypto_unblock(sc->sc_cid, CRYPTO_SYMQ|CRYPTO_ASYMQ);
	splx(s);
}

/*
 * DMA handling
 */

/*
 * Allocate kernel devmem and DMA safe memory with bus_dma API
 * used for DMA descriptors.
 *
 * if phys != 0, assume phys is a DMA safe memory and bypass
 * allocator.
 */
STATIC struct mvxpsec_devmem *
mvxpsec_alloc_devmem(struct mvxpsec_softc *sc, paddr_t phys, int size)
{
	struct mvxpsec_devmem *devmem;
	bus_dma_segment_t seg;
	int rseg;
	int err;

	if (sc == NULL)
		return NULL;

	devmem = kmem_alloc(sizeof(*devmem), KM_NOSLEEP);
	if (devmem == NULL) {
		aprint_error_dev(sc->sc_dev, "can't alloc kmem\n");
		return NULL;
	}

	devmem->size = size;

	if (phys) {
		seg.ds_addr = phys;
		seg.ds_len = devmem->size;
		rseg = 1;
		err = 0;
	}
	else {
		err = bus_dmamem_alloc(sc->sc_dmat,
		    devmem->size, PAGE_SIZE, 0,
		    &seg, MVXPSEC_DMA_MAX_SEGS, &rseg, BUS_DMA_NOWAIT);
	}
	if (err) {
		aprint_error_dev(sc->sc_dev, "can't alloc DMA buffer\n");
		goto fail_kmem_free;
	}

	err = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	     devmem->size, &devmem->kva, BUS_DMA_NOWAIT);
	if (err) {
		aprint_error_dev(sc->sc_dev, "can't map DMA buffer\n");
		goto fail_dmamem_free;
	}

	err = bus_dmamap_create(sc->sc_dmat,
	    size, 1, size, 0, BUS_DMA_NOWAIT, &devmem->map);
	if (err) {
		aprint_error_dev(sc->sc_dev, "can't create DMA map\n");
		goto fail_unmap;
	}

	err = bus_dmamap_load(sc->sc_dmat,
	    devmem->map, devmem->kva, devmem->size, NULL,
	    BUS_DMA_NOWAIT);
	if (err) {
		aprint_error_dev(sc->sc_dev,
		   "can't load DMA buffer VA:%p PA:0x%08x\n",
		    devmem->kva, (int)seg.ds_addr);
		goto fail_destroy;
	}

	return devmem;

fail_destroy:
	bus_dmamap_destroy(sc->sc_dmat, devmem->map);
fail_unmap:
	bus_dmamem_unmap(sc->sc_dmat, devmem->kva, devmem->size);
fail_dmamem_free:
	bus_dmamem_free(sc->sc_dmat, &seg, rseg);
fail_kmem_free:
	kmem_free(devmem, sizeof(*devmem));

	return NULL;
}

/*
 * Get DMA Descriptor from (DMA safe) descriptor pool.
 */
INLINE struct mvxpsec_descriptor_handle *
mvxpsec_dma_getdesc(struct mvxpsec_softc *sc)
{
	struct mvxpsec_descriptor_handle *entry;

	/* must called with sc->sc_dma_mtx held */
	KASSERT(mutex_owned(&sc->sc_dma_mtx));

	if (sc->sc_desc_ring_prod == sc->sc_desc_ring_cons)
		return NULL;

	entry = &sc->sc_desc_ring[sc->sc_desc_ring_prod];
	sc->sc_desc_ring_prod++;
	if (sc->sc_desc_ring_prod >= sc->sc_desc_ring_size)
		sc->sc_desc_ring_prod -= sc->sc_desc_ring_size;

	return entry;
}

/*
 * Put DMA Descriptor to descriptor pool.
 */
_INLINE void
mvxpsec_dma_putdesc(struct mvxpsec_softc *sc,
    struct mvxpsec_descriptor_handle *dh)
{
	/* must called with sc->sc_dma_mtx held */
	KASSERT(mutex_owned(&sc->sc_dma_mtx));

	sc->sc_desc_ring_cons++;
	if (sc->sc_desc_ring_cons >= sc->sc_desc_ring_size)
		sc->sc_desc_ring_cons -= sc->sc_desc_ring_size;

	return;
}

/*
 * Setup DMA Descriptor
 * copy from 'src' to 'dst' by 'size' bytes.
 * 'src' or 'dst' must be SRAM address.
 */
INLINE void
mvxpsec_dma_setup(struct mvxpsec_descriptor_handle *dh,
    uint32_t dst, uint32_t src, uint32_t size)
{
	struct mvxpsec_descriptor *desc;

	desc = (struct mvxpsec_descriptor *)dh->_desc;

	desc->tdma_dst = dst;
	desc->tdma_src = src;
	desc->tdma_word0 = size;
	if (size != 0)
		desc->tdma_word0 |= MV_TDMA_CNT_OWN;
	/* size == 0 is owned by ACC, not TDMA */

#ifdef MVXPSEC_DEBUG
	mvxpsec_dump_dmaq(dh);
#endif

}

/*
 * Concat 2 DMA
 */
INLINE void
mvxpsec_dma_cat(struct mvxpsec_softc *sc,
    struct mvxpsec_descriptor_handle *dh1,
    struct mvxpsec_descriptor_handle *dh2)
{
	((struct mvxpsec_descriptor*)dh1->_desc)->tdma_nxt = dh2->phys_addr;
	MVXPSEC_SYNC_DESC(sc, dh1, BUS_DMASYNC_PREWRITE);
}

/*
 * Schedule DMA Copy
 */
INLINE int
mvxpsec_dma_copy0(struct mvxpsec_softc *sc, mvxpsec_dma_ring *r,
    uint32_t dst, uint32_t src, uint32_t size)
{
	struct mvxpsec_descriptor_handle *dh;

	dh = mvxpsec_dma_getdesc(sc);
	if (dh == NULL) {
		log(LOG_ERR, "%s: descriptor full\n", __func__);
		return -1;
	}

	mvxpsec_dma_setup(dh, dst, src, size);
	if (r->dma_head == NULL) {
		r->dma_head = dh;
		r->dma_last = dh;
		r->dma_size = 1;
	}
	else {
		mvxpsec_dma_cat(sc, r->dma_last, dh);
		r->dma_last = dh;
		r->dma_size++;
	}

	return 0;
}

INLINE int
mvxpsec_dma_copy(struct mvxpsec_softc *sc, mvxpsec_dma_ring *r,
    uint32_t dst, uint32_t src, uint32_t size)
{
	if (size == 0) /* 0 is very special descriptor */
		return 0;

	return mvxpsec_dma_copy0(sc, r, dst, src, size);
}

/*
 * Schedule ACC Activate
 */
INLINE int
mvxpsec_dma_acc_activate(struct mvxpsec_softc *sc, mvxpsec_dma_ring *r)
{
	return mvxpsec_dma_copy0(sc, r, 0, 0, 0);
}

/*
 * Finalize DMA setup
 */
INLINE void
mvxpsec_dma_finalize(struct mvxpsec_softc *sc, mvxpsec_dma_ring *r)
{
	struct mvxpsec_descriptor_handle *dh;

	dh = r->dma_last;
	((struct mvxpsec_descriptor*)dh->_desc)->tdma_nxt = 0;
	MVXPSEC_SYNC_DESC(sc, dh, BUS_DMASYNC_PREWRITE);
}

/*
 * Free entire DMA ring
 */
INLINE void
mvxpsec_dma_free(struct mvxpsec_softc *sc, mvxpsec_dma_ring *r)
{
	sc->sc_desc_ring_cons += r->dma_size;
	if (sc->sc_desc_ring_cons >= sc->sc_desc_ring_size)
		sc->sc_desc_ring_cons -= sc->sc_desc_ring_size;
	r->dma_head = NULL;
	r->dma_last = NULL;
	r->dma_size = 0;
}

/*
 * create DMA descriptor chain for the packet
 */
INLINE int
mvxpsec_dma_copy_packet(struct mvxpsec_softc *sc, struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	uint32_t src, dst, len;
	uint32_t pkt_off, pkt_off_r;
	int err;
	int i;

	/* must called with sc->sc_dma_mtx held */
	KASSERT(mutex_owned(&sc->sc_dma_mtx));

	/*
	 * set offset for mem->device copy
	 *
	 * typical packet image:
	 *
	 *   enc_ivoff
	 *   mac_off
	 *   |
	 *   |    enc_off
	 *   |    |
	 *   v    v
	 *   +----+--------...
	 *   |IV  |DATA    
	 *   +----+--------...
	 */
	pkt_off = 0;
	if (mv_p->mac_off > 0)
		pkt_off = mv_p->mac_off;
	if ((mv_p->flags & CRP_EXT_IV) == 0 && pkt_off > mv_p->enc_ivoff)
		pkt_off = mv_p->enc_ivoff;
	if (mv_p->enc_off > 0 && pkt_off > mv_p->enc_off)
		pkt_off = mv_p->enc_off;
	pkt_off_r = pkt_off;

	/* make DMA descriptors to copy packet header: DRAM -> SRAM */
	dst = (uint32_t)MVXPSEC_SRAM_PKT_HDR_PA(sc);
	src = (uint32_t)mv_p->pkt_header_map->dm_segs[0].ds_addr;
	len = sizeof(mv_p->pkt_header);
	err = mvxpsec_dma_copy(sc, &mv_p->dma_ring, dst, src, len);
	if (__predict_false(err))
		return err;

	/* 
	 * make DMA descriptors to copy session header: DRAM -> SRAM
	 * we can reuse session header on SRAM if session is not changed.
	 */
	if (sc->sc_last_session != mv_s) {
		dst = (uint32_t)MVXPSEC_SRAM_SESS_HDR_PA(sc);
		src = (uint32_t)mv_s->session_header_map->dm_segs[0].ds_addr;
		len = sizeof(mv_s->session_header);
		err = mvxpsec_dma_copy(sc, &mv_p->dma_ring, dst, src, len);
		if (__predict_false(err))
			return err;
		sc->sc_last_session = mv_s;
	}

	/* make DMA descriptor to copy payload data: DRAM -> SRAM */
	dst = MVXPSEC_SRAM_PAYLOAD_PA(sc, 0);
	for (i = 0; i < mv_p->data_map->dm_nsegs; i++) {
		src = mv_p->data_map->dm_segs[i].ds_addr;
		len = mv_p->data_map->dm_segs[i].ds_len;
		if (pkt_off) {
			if (len <= pkt_off) {
				/* ignore the segment */
				dst += len;
				pkt_off -= len;
				continue;
			}
			/* copy from the middle of the segment */
			dst += pkt_off;
			src += pkt_off;
			len -= pkt_off;
			pkt_off = 0;
		}
		err = mvxpsec_dma_copy(sc, &mv_p->dma_ring, dst, src, len);
		if (__predict_false(err))
			return err;
		dst += len;
	}

	/* make special descriptor to activate security accelerator */
	err = mvxpsec_dma_acc_activate(sc, &mv_p->dma_ring);
	if (__predict_false(err))
		return err;

	/* make DMA descriptors to copy payload: SRAM -> DRAM */
	src = (uint32_t)MVXPSEC_SRAM_PAYLOAD_PA(sc, 0);
	for (i = 0; i < mv_p->data_map->dm_nsegs; i++) {
		dst = (uint32_t)mv_p->data_map->dm_segs[i].ds_addr;
		len = (uint32_t)mv_p->data_map->dm_segs[i].ds_len;
		if (pkt_off_r) {
			if (len <= pkt_off_r) {
				/* ignore the segment */
				src += len;
				pkt_off_r -= len;
				continue;
			}
			/* copy from the middle of the segment */
			src += pkt_off_r;
			dst += pkt_off_r;
			len -= pkt_off_r;
			pkt_off_r = 0;
		}
		err = mvxpsec_dma_copy(sc, &mv_p->dma_ring, dst, src, len);
		if (__predict_false(err))
			return err;
		src += len;
	}
	KASSERT(pkt_off == 0);
	KASSERT(pkt_off_r == 0);

	/*
	 * make DMA descriptors to copy packet header: SRAM->DRAM
	 * if IV is present in the payload, no need to copy.
	 */
	if (mv_p->flags & CRP_EXT_IV) {
		dst = (uint32_t)mv_p->pkt_header_map->dm_segs[0].ds_addr;
		src = (uint32_t)MVXPSEC_SRAM_PKT_HDR_PA(sc);
		len = sizeof(mv_p->pkt_header);
		err = mvxpsec_dma_copy(sc, &mv_p->dma_ring, dst, src, len);
		if (__predict_false(err))
			return err;
	}

	return 0;
}

INLINE int
mvxpsec_dma_sync_packet(struct mvxpsec_softc *sc, struct mvxpsec_packet *mv_p)
{
	/* sync packet header */
	bus_dmamap_sync(sc->sc_dmat,
	    mv_p->pkt_header_map, 0, sizeof(mv_p->pkt_header),
	    BUS_DMASYNC_PREWRITE);

#ifdef MVXPSEC_DEBUG
	/* sync session header */
	if (mvxpsec_debug != 0) {
		struct mvxpsec_session *mv_s = mv_p->mv_s;

		/* only debug code touch the session header after newsession */
		bus_dmamap_sync(sc->sc_dmat,
		    mv_s->session_header_map,
		    0, sizeof(mv_s->session_header),
		    BUS_DMASYNC_PREWRITE);
	}
#endif

	/* sync packet buffer */
	bus_dmamap_sync(sc->sc_dmat,
	    mv_p->data_map, 0, mv_p->data_len,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	return 0;
}

/*
 * Initialize MVXPSEC Internal SRAM
 *
 * - must be called after DMA initizlization.
 * - make VM mapping for SRAM area on MBus.
 */
STATIC int
mvxpsec_init_sram(struct mvxpsec_softc *sc)
{
	uint32_t tag, target, attr, base, size;
	vaddr_t va;
	int window;

	switch (sc->sc_dev->dv_unit) {
	case 0:
		tag = ARMADAXP_TAG_CRYPT0;
		break;
	case 1:
		tag = ARMADAXP_TAG_CRYPT1;
		break;
	default:
		aprint_error_dev(sc->sc_dev, "no internal SRAM mapping\n");
		return -1;
	}

	window = mvsoc_target(tag, &target, &attr, &base, &size);
	if (window >= nwindow) {
		aprint_error_dev(sc->sc_dev, "no internal SRAM mapping\n");
		return -1;
	}

	if (sizeof(struct mvxpsec_crypt_sram) > size) {
		aprint_error_dev(sc->sc_dev,
		    "SRAM Data Structure Excceeds SRAM window size.\n");
		return -1;
	}

	aprint_normal_dev(sc->sc_dev,
	    "internal SRAM window at 0x%08x-0x%08x",
	    base, base + size - 1);
	sc->sc_sram_pa = base;

	/* get vmspace to read/write device internal SRAM */
	va = uvm_km_alloc(kernel_map, PAGE_SIZE, PAGE_SIZE,
			UVM_KMF_VAONLY | UVM_KMF_NOWAIT);
	if (va == 0) {
		aprint_error_dev(sc->sc_dev, "cannot map SRAM window\n");
		sc->sc_sram_va = NULL;
		aprint_normal("\n");
		return 0;
	}
	/* XXX: not working. PMAP_NOCACHE is not affected? */
	pmap_kenter_pa(va, base, VM_PROT_READ|VM_PROT_WRITE, PMAP_NOCACHE);
	pmap_update(pmap_kernel());
	sc->sc_sram_va = (void *)va;
	aprint_normal(" va %p\n", sc->sc_sram_va);
	memset(sc->sc_sram_va, 0xff, MV_ACC_SRAM_SIZE);

	return 0;
}

/*
 * Initialize TDMA engine.
 */
STATIC int
mvxpsec_init_dma(struct mvxpsec_softc *sc, struct marvell_attach_args *mva)
{
	struct mvxpsec_descriptor_handle *dh;
	uint8_t *va;
	paddr_t pa;
	off_t va_off, pa_off;
	int i, n, seg, ndh;

	/* Init Deviced's control parameters (disabled yet) */
	MVXPSEC_WRITE(sc, MV_TDMA_CONTROL, MV_TDMA_DEFAULT_CONTROL);

	/* Init Software DMA Handlers */
	sc->sc_devmem_desc =
	    mvxpsec_alloc_devmem(sc, 0, PAGE_SIZE * MVXPSEC_DMA_DESC_PAGES);
	if (sc->sc_devmem_desc == NULL)
		panic("Cannot allocate memory\n");
	ndh = (PAGE_SIZE / sizeof(struct mvxpsec_descriptor))
	    * MVXPSEC_DMA_DESC_PAGES;
	sc->sc_desc_ring =
	    kmem_alloc(sizeof(struct mvxpsec_descriptor_handle) * ndh,
	        KM_NOSLEEP);
	if (sc->sc_desc_ring == NULL)
		panic("Cannot allocate memory\n");
	aprint_normal_dev(sc->sc_dev, "%d DMA handles in %zu bytes array\n",
	    ndh, sizeof(struct mvxpsec_descriptor_handle) * ndh);

	ndh = 0;
	for (seg = 0; seg < devmem_nseg(sc->sc_devmem_desc); seg++) {
		va = devmem_va(sc->sc_devmem_desc);
		pa = devmem_pa(sc->sc_devmem_desc, seg);
		n = devmem_palen(sc->sc_devmem_desc, seg) /
		       	sizeof(struct mvxpsec_descriptor);
		va_off = (PAGE_SIZE * seg);
		pa_off = 0;
		for (i = 0; i < n; i++) {
			dh = &sc->sc_desc_ring[ndh];
			dh->map = devmem_map(sc->sc_devmem_desc);
			dh->off = va_off + pa_off;
			dh->_desc = (void *)(va + va_off + pa_off);
			dh->phys_addr = pa + pa_off;
			pa_off += sizeof(struct mvxpsec_descriptor);
			ndh++;
		}
	}
	sc->sc_desc_ring_size = ndh;
	sc->sc_desc_ring_prod = 0;
	sc->sc_desc_ring_cons = sc->sc_desc_ring_size - 1;

	return 0;
}

/*
 * Wait for TDMA controller become idle
 */
INLINE int
mvxpsec_dma_wait(struct mvxpsec_softc *sc)
{
	int retry = 0;

	while (MVXPSEC_READ(sc, MV_TDMA_CONTROL) & MV_TDMA_CONTROL_ACT) {
		delay(mvxpsec_wait_interval);
		if (retry++ >= mvxpsec_wait_retry)
			return -1;
	}
	return 0;
}

/*
 * Wait for Security Accelerator become idle
 */
INLINE int
mvxpsec_acc_wait(struct mvxpsec_softc *sc)
{
	int retry = 0;

	while (MVXPSEC_READ(sc, MV_ACC_COMMAND) & MV_ACC_COMMAND_ACT) {
		delay(mvxpsec_wait_interval);
		if (++retry >= mvxpsec_wait_retry)
			return -1;
	}
	return 0;
}

/*
 * Entry of interrupt handler
 *
 * register this to kernel via marvell_intr_establish()
 */
int
mvxpsec_intr(void *arg)
{
	struct mvxpsec_softc *sc = arg;
	uint32_t v;

	/* IPL_NET */
	while ((v = mvxpsec_intr_ack(sc)) != 0) {
		mvxpsec_intr_cnt(sc, v);
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_INTR, "MVXPSEC Intr 0x%08x\n", v);
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_INTR, "%s\n", s_xpsecintr(v));
#ifdef MVXPSEC_DEBUG
		mvxpsec_dump_reg(sc);
#endif

		/* call high-level handlers */
		if (v & MVXPSEC_INT_ACCTDMA)
			mvxpsec_done(sc);
	}

	return 0;
}

INLINE void
mvxpsec_intr_cleanup(struct mvxpsec_softc *sc)
{
	struct mvxpsec_packet *mv_p;

	/* must called with sc->sc_dma_mtx held */
	KASSERT(mutex_owned(&sc->sc_dma_mtx));

	/*
	 * there is only one intr for run_queue.
	 * no one touch sc_run_queue.
	 */
	SIMPLEQ_FOREACH(mv_p, &sc->sc_run_queue, queue)
		mvxpsec_dma_free(sc, &mv_p->dma_ring);
}

/*
 * Acknowledge to interrupt
 *
 * read cause bits, clear it, and return it.
 * NOTE: multiple cause bits may be returned at once.
 */
STATIC uint32_t
mvxpsec_intr_ack(struct mvxpsec_softc *sc)
{
	uint32_t reg;

	reg  = MVXPSEC_READ(sc, MVXPSEC_INT_CAUSE);
	reg &= MVXPSEC_DEFAULT_INT;
	MVXPSEC_WRITE(sc, MVXPSEC_INT_CAUSE, ~reg);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_INTR, "Int: %s\n", s_xpsecintr(reg));

	return reg;
}

/*
 * Entry of TDMA error interrupt handler
 *
 * register this to kernel via marvell_intr_establish()
 */
int
mvxpsec_eintr(void *arg)
{
	struct mvxpsec_softc *sc = arg;
	uint32_t err;

	/* IPL_NET */
again:
	err = mvxpsec_eintr_ack(sc);
	if (err == 0)
		goto done;

	log(LOG_ERR, "%s: DMA Error Interrupt: %s\n", __func__,
	    s_errreg(err));
#ifdef MVXPSEC_DEBUG
	mvxpsec_dump_reg(sc);
#endif

	goto again;
done:
	return 0;
}

/*
 * Acknowledge to TDMA error interrupt
 *
 * read cause bits, clear it, and return it.
 * NOTE: multiple cause bits may be returned at once.
 */
STATIC uint32_t
mvxpsec_eintr_ack(struct mvxpsec_softc *sc)
{
	uint32_t reg;
 
	reg  = MVXPSEC_READ(sc, MV_TDMA_ERR_CAUSE);
	reg &= MVXPSEC_DEFAULT_ERR;
	MVXPSEC_WRITE(sc, MV_TDMA_ERR_CAUSE, ~reg);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_INTR, "Int: %s\n", s_xpsecintr(reg));

	return reg;
}

/*
 * Interrupt statistics
 *
 * this is NOT a statistics of how may times the events 'occured'.
 * this ONLY means how many times the events 'handled'.
 */
INLINE void
mvxpsec_intr_cnt(struct mvxpsec_softc *sc, int cause)
{
	MVXPSEC_EVCNT_INCR(sc, intr_all);
	if (cause & MVXPSEC_INT_AUTH)
		MVXPSEC_EVCNT_INCR(sc, intr_auth);
	if (cause & MVXPSEC_INT_DES)
		MVXPSEC_EVCNT_INCR(sc, intr_des);
	if (cause & MVXPSEC_INT_AES_ENC)
		MVXPSEC_EVCNT_INCR(sc, intr_aes_enc);
	if (cause & MVXPSEC_INT_AES_DEC)
		MVXPSEC_EVCNT_INCR(sc, intr_aes_dec);
	if (cause & MVXPSEC_INT_ENC)
		MVXPSEC_EVCNT_INCR(sc, intr_enc);
	if (cause & MVXPSEC_INT_SA)
		MVXPSEC_EVCNT_INCR(sc, intr_sa);
	if (cause & MVXPSEC_INT_ACCTDMA)
		MVXPSEC_EVCNT_INCR(sc, intr_acctdma);
	if (cause & MVXPSEC_INT_TDMA_COMP)
		MVXPSEC_EVCNT_INCR(sc, intr_comp);
	if (cause & MVXPSEC_INT_TDMA_OWN)
		MVXPSEC_EVCNT_INCR(sc, intr_own);
	if (cause & MVXPSEC_INT_ACCTDMA_CONT)
		MVXPSEC_EVCNT_INCR(sc, intr_acctdma_cont);
}

/*
 * Setup MVXPSEC header structure.
 *
 * the header contains descriptor of security accelerator,
 * key material of chiphers, iv of ciphers and macs, ...
 *
 * the header is transfered to MVXPSEC Internal SRAM by TDMA,
 * and parsed by MVXPSEC H/W.
 */
STATIC int
mvxpsec_header_finalize(struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_acc_descriptor *desc = &mv_p->pkt_header.desc;
	int enc_start, enc_len, iv_offset;
	int mac_start, mac_len, mac_offset;

	/* offset -> device address */
	enc_start = MVXPSEC_SRAM_PAYLOAD_DA(mv_p->enc_off);
	enc_len = mv_p->enc_len;
	if (mv_p->flags & CRP_EXT_IV)
		iv_offset = mv_p->enc_ivoff;
	else
		iv_offset = MVXPSEC_SRAM_PAYLOAD_DA(mv_p->enc_ivoff);
	mac_start = MVXPSEC_SRAM_PAYLOAD_DA(mv_p->mac_off);
	mac_len = mv_p->mac_len;
	mac_offset = MVXPSEC_SRAM_PAYLOAD_DA(mv_p->mac_dst);

	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "PAYLOAD at 0x%08x\n", (int)MVXPSEC_SRAM_PAYLOAD_OFF);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "ENC from 0x%08x\n", enc_start);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "MAC from 0x%08x\n", mac_start);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "MAC to 0x%08x\n", mac_offset);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "ENC IV at 0x%08x\n", iv_offset);

	/* setup device addresses in Security Accelerator Descriptors */
	desc->acc_encdata = MV_ACC_DESC_ENC_DATA(enc_start, enc_start);
	desc->acc_enclen = MV_ACC_DESC_ENC_LEN(enc_len);
	if (desc->acc_config & MV_ACC_CRYPTO_DECRYPT)
		desc->acc_enckey =
		    MV_ACC_DESC_ENC_KEY(MVXPSEC_SRAM_KEY_D_DA);
	else
		desc->acc_enckey =
		    MV_ACC_DESC_ENC_KEY(MVXPSEC_SRAM_KEY_DA);
	desc->acc_enciv =
	    MV_ACC_DESC_ENC_IV(MVXPSEC_SRAM_IV_WORK_DA, iv_offset);

	desc->acc_macsrc = MV_ACC_DESC_MAC_SRC(mac_start, mac_len);
	desc->acc_macdst = MV_ACC_DESC_MAC_DST(mac_offset, mac_len);
	desc->acc_maciv =
	    MV_ACC_DESC_MAC_IV(MVXPSEC_SRAM_MIV_IN_DA,
	        MVXPSEC_SRAM_MIV_OUT_DA);

	return 0;
}

/*
 * constractor of session structure.
 *
 * this constrator will be called by pool_cache framework.
 */
STATIC int
mvxpsec_session_ctor(void *arg, void *obj, int flags)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_session *mv_s = obj;

	/* pool is owned by softc */
	mv_s->sc = sc;

	/* Create and load DMA map for session header */
	mv_s->session_header_map = 0;
	if (bus_dmamap_create(sc->sc_dmat,
	    sizeof(mv_s->session_header), 1,
	    sizeof(mv_s->session_header), 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &mv_s->session_header_map)) {
		log(LOG_ERR, "%s: cannot create DMA map\n", __func__);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmat, mv_s->session_header_map,
	    &mv_s->session_header, sizeof(mv_s->session_header),
	    NULL, BUS_DMA_NOWAIT)) {
		log(LOG_ERR, "%s: cannot load header\n", __func__);
		goto fail;
	}

	return 0;
fail:
	if (mv_s->session_header_map)
		bus_dmamap_destroy(sc->sc_dmat, mv_s->session_header_map);
	return ENOMEM;
}

/*
 * destractor of session structure.
 *
 * this destrator will be called by pool_cache framework.
 */
STATIC void
mvxpsec_session_dtor(void *arg, void *obj)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_session *mv_s = obj;

	if (mv_s->sc != sc)
		panic("inconsitent context\n");

	bus_dmamap_destroy(sc->sc_dmat, mv_s->session_header_map);
}

/*
 * constructor of packet structure.
 */
STATIC int
mvxpsec_packet_ctor(void *arg, void *obj, int flags)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_packet *mv_p = obj;

	mv_p->dma_ring.dma_head = NULL;
	mv_p->dma_ring.dma_last = NULL;
	mv_p->dma_ring.dma_size = 0;

	/* Create and load DMA map for packet header */
	mv_p->pkt_header_map = 0;
	if (bus_dmamap_create(sc->sc_dmat,
	    sizeof(mv_p->pkt_header), 1, sizeof(mv_p->pkt_header), 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &mv_p->pkt_header_map)) {
		log(LOG_ERR, "%s: cannot create DMA map\n", __func__);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmat, mv_p->pkt_header_map, 
	    &mv_p->pkt_header, sizeof(mv_p->pkt_header),
	    NULL, BUS_DMA_NOWAIT)) {
		log(LOG_ERR, "%s: cannot load header\n", __func__);
		goto fail;
	}

	/* Create DMA map for session data. */
	mv_p->data_map = 0;
	if (bus_dmamap_create(sc->sc_dmat,
	    MVXPSEC_DMA_MAX_SIZE, MVXPSEC_DMA_MAX_SEGS, MVXPSEC_DMA_MAX_SIZE,
	    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mv_p->data_map)) {
		log(LOG_ERR, "%s: cannot create DMA map\n", __func__);
		goto fail;
	}

	return 0;
fail:
	if (mv_p->pkt_header_map)
		bus_dmamap_destroy(sc->sc_dmat, mv_p->pkt_header_map);
	if (mv_p->data_map)
		bus_dmamap_destroy(sc->sc_dmat, mv_p->data_map);
	return ENOMEM;
}

/*
 * destractor of packet structure.
 */
STATIC void
mvxpsec_packet_dtor(void *arg, void *obj)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_packet *mv_p = obj;

	mutex_enter(&sc->sc_dma_mtx);
	mvxpsec_dma_free(sc, &mv_p->dma_ring);
	mutex_exit(&sc->sc_dma_mtx);
	bus_dmamap_destroy(sc->sc_dmat, mv_p->pkt_header_map);
	bus_dmamap_destroy(sc->sc_dmat, mv_p->data_map);
}

/*
 * allocate new session struture.
 */
STATIC struct mvxpsec_session *
mvxpsec_session_alloc(struct mvxpsec_softc *sc)
{
	struct mvxpsec_session *mv_s;

	mv_s = pool_cache_get(sc->sc_session_pool, 0);
	if (mv_s == NULL) {
		log(LOG_ERR, "%s: cannot allocate memory\n", __func__);
		return NULL;
	}
	mv_s->refs = 1; /* 0 means session is alredy invalid */
	mv_s->sflags = 0;

	return mv_s;
}

/*
 * deallocate session structure.
 */
STATIC void
mvxpsec_session_dealloc(struct mvxpsec_session *mv_s)
{
	struct mvxpsec_softc *sc = mv_s->sc;

	mv_s->sflags |= DELETED;
	mvxpsec_session_unref(mv_s);
	crypto_unblock(sc->sc_cid, CRYPTO_SYMQ|CRYPTO_ASYMQ);

	return;
}

STATIC int
mvxpsec_session_ref(struct mvxpsec_session *mv_s)
{
	uint32_t refs;

	if (mv_s->sflags & DELETED) {
		log(LOG_ERR,
		    "%s: session is already deleted.\n", __func__);
		return -1;
	}

	refs = atomic_inc_32_nv(&mv_s->refs);
	if (refs == 1) {
		/* 
		 * a session with refs == 0 is
		 * already invalidated. revert it.
		 * XXX: use CAS ?
		 */
		atomic_dec_32(&mv_s->refs);
		log(LOG_ERR,
		    "%s: session is already invalidated.\n", __func__);
		return -1;
	}
	
	return 0;
}

STATIC void
mvxpsec_session_unref(struct mvxpsec_session *mv_s)
{
	uint32_t refs;

	refs = atomic_dec_32_nv(&mv_s->refs);
	if (refs == 0)
		pool_cache_put(mv_s->sc->sc_session_pool, mv_s);
}

/*
 * look for session is exist or not
 */
INLINE struct mvxpsec_session *
mvxpsec_session_lookup(struct mvxpsec_softc *sc, int sid)
{
	struct mvxpsec_session *mv_s;
	int session;

	/* must called sc->sc_session_mtx held */
	KASSERT(mutex_owned(&sc->sc_session_mtx));

	session = MVXPSEC_SESSION(sid);
	if (__predict_false(session > MVXPSEC_MAX_SESSIONS)) {
		log(LOG_ERR, "%s: session number too large %d\n",
		    __func__, session);
		return NULL;
	}
	if (__predict_false( (mv_s = sc->sc_sessions[session]) == NULL)) {
		log(LOG_ERR, "%s: invalid session %d\n",
		    __func__, session);
		return NULL;
	}

	KASSERT(mv_s->sid == session);

	return mv_s;
}

/*
 * allocation new packet structure.
 */
STATIC struct mvxpsec_packet *
mvxpsec_packet_alloc(struct mvxpsec_session *mv_s)
{
	struct mvxpsec_softc *sc = mv_s->sc;
	struct mvxpsec_packet *mv_p;

	/* must be called mv_queue_mtx held. */
	KASSERT(mutex_owned(&sc->sc_queue_mtx));
	/* must be called mv_session_mtx held. */
	KASSERT(mutex_owned(&sc->sc_session_mtx));

	if (mvxpsec_session_ref(mv_s) < 0) {
		log(LOG_ERR, "%s: invalid session.\n", __func__);
		return NULL;
	}

	if ( (mv_p = SLIST_FIRST(&sc->sc_free_list)) != NULL) {
		SLIST_REMOVE_HEAD(&sc->sc_free_list, free_list);
		sc->sc_free_qlen--;
	}
	else {
		mv_p = pool_cache_get(sc->sc_packet_pool, 0);
		if (mv_p == NULL) {
			log(LOG_ERR, "%s: cannot allocate memory\n",
			    __func__);
			mvxpsec_session_unref(mv_s);
			return NULL;
		}
	}
	mv_p->mv_s = mv_s;
	mv_p->flags = 0;
	mv_p->data_ptr = NULL;

	return mv_p;
}

/*
 * free packet structure.
 */
STATIC void
mvxpsec_packet_dealloc(struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	struct mvxpsec_softc *sc = mv_s->sc;

	/* must called with sc->sc_queue_mtx held */
	KASSERT(mutex_owned(&sc->sc_queue_mtx));

	if (mv_p->dma_ring.dma_size != 0) {
		sc->sc_desc_ring_cons += mv_p->dma_ring.dma_size;
	}
	mv_p->dma_ring.dma_head = NULL;
	mv_p->dma_ring.dma_last = NULL;
	mv_p->dma_ring.dma_size = 0;

	if (mv_p->data_map) {
		if (mv_p->flags & RDY_DATA) {
			bus_dmamap_unload(sc->sc_dmat, mv_p->data_map);
			mv_p->flags &= ~RDY_DATA;
		}
	}

	if (sc->sc_free_qlen > sc->sc_wait_qlimit)
		pool_cache_put(sc->sc_packet_pool, mv_p);
	else {
		SLIST_INSERT_HEAD(&sc->sc_free_list, mv_p, free_list);
		sc->sc_free_qlen++;
	}
	mvxpsec_session_unref(mv_s);
}

INLINE void
mvxpsec_packet_enqueue(struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_softc *sc = mv_p->mv_s->sc;
	struct mvxpsec_packet *last_packet;
	struct mvxpsec_descriptor_handle *cur_dma, *prev_dma;

	/* must called with sc->sc_queue_mtx held */
	KASSERT(mutex_owned(&sc->sc_queue_mtx));

	if (sc->sc_wait_qlen == 0) {
		SIMPLEQ_INSERT_TAIL(&sc->sc_wait_queue, mv_p, queue);
		sc->sc_wait_qlen++;
		mv_p->flags |= SETUP_DONE;
		return;
	}

	last_packet = SIMPLEQ_LAST(&sc->sc_wait_queue, mvxpsec_packet, queue);
	SIMPLEQ_INSERT_TAIL(&sc->sc_wait_queue, mv_p, queue);
	sc->sc_wait_qlen++;

	/* chain the DMA */
	cur_dma = mv_p->dma_ring.dma_head;
	prev_dma = last_packet->dma_ring.dma_last;
	mvxpsec_dma_cat(sc, prev_dma, cur_dma);
	mv_p->flags |= SETUP_DONE;
}

/*
 * called by interrupt handler
 */
STATIC int
mvxpsec_done_packet(struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	struct mvxpsec_softc *sc = mv_s->sc;

	KASSERT((mv_p->flags & RDY_DATA));
	KASSERT((mv_p->flags & SETUP_DONE));

	/* unload data */
	bus_dmamap_sync(sc->sc_dmat, mv_p->data_map,
	    0, mv_p->data_len,
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, mv_p->data_map);
	mv_p->flags &= ~RDY_DATA;

#ifdef MVXPSEC_DEBUG
	if (mvxpsec_debug != 0) {
		int s;

		bus_dmamap_sync(sc->sc_dmat, mv_p->pkt_header_map,
		    0, sizeof(mv_p->pkt_header),
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
		bus_dmamap_sync(sc->sc_dmat, mv_s->session_header_map,
		    0, sizeof(mv_s->session_header),
		    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

		if (mvxpsec_debug & MVXPSEC_DEBUG_OPENCRYPTO) {
			char buf[1500];
			struct mbuf *m;
			struct uio *uio;
			size_t len;

			switch (mv_p->data_type) {
			case MVXPSEC_DATA_MBUF:
				m = mv_p->data_mbuf;
				len = m->m_pkthdr.len;
				if (len > sizeof(buf))
					len = sizeof(buf);
				m_copydata(m, 0, len, buf);
				break;
			case MVXPSEC_DATA_UIO:
				uio = mv_p->data_uio;
				len = uio->uio_resid;
				if (len > sizeof(buf))
					len = sizeof(buf);
				cuio_copydata(uio, 0, len, buf);
				break;
			default:
				len = 0;
			}
			if (len > 0)
				mvxpsec_dump_data(__func__, buf, len);
		}

		if (mvxpsec_debug & MVXPSEC_DEBUG_PAYLOAD) {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_PAYLOAD,
			    "%s: session_descriptor:\n", __func__);
			mvxpsec_dump_packet_desc(__func__, mv_p);
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_PAYLOAD,
			    "%s: session_data:\n", __func__);
			mvxpsec_dump_packet_data(__func__, mv_p);
		}

		if (mvxpsec_debug & MVXPSEC_DEBUG_SRAM) {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_SRAM,
			    "%s: SRAM\n", __func__);
			mvxpsec_dump_sram(__func__, sc, 2000);
		}

		s = MVXPSEC_READ(sc, MV_ACC_STATUS);
		if (s & MV_ACC_STATUS_MAC_ERR) {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_INTR,
			    "%s: Message Authentication Failed.\n", __func__);
		}
	}
#endif

	/* copy back IV */
	if (mv_p->flags & CRP_EXT_IV) {
		memcpy(mv_p->ext_iv,
		    &mv_p->pkt_header.crp_iv_ext, mv_p->ext_ivlen);
		mv_p->ext_iv = NULL;
		mv_p->ext_ivlen = 0;
	}

	/* notify opencrypto */
	mv_p->crp->crp_etype = 0;
	crypto_done(mv_p->crp);
	mv_p->crp = NULL;

	/* unblock driver */
	mvxpsec_packet_dealloc(mv_p);
	crypto_unblock(sc->sc_cid, CRYPTO_SYMQ|CRYPTO_ASYMQ);

	MVXPSEC_EVCNT_INCR(sc, packet_ok);

	return 0;
}


/*
 * Opencrypto API registration
 */
int
mvxpsec_register(struct mvxpsec_softc *sc)
{
	int oplen = SRAM_PAYLOAD_SIZE;
	int flags = 0;
	int err;

	sc->sc_nsessions = 0;
	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		log(LOG_ERR,
		    "%s: crypto_get_driverid() failed.\n", __func__);
		err = EINVAL;
		goto done;
	}

	/* Ciphers */
	err = crypto_register(sc->sc_cid, CRYPTO_DES_CBC, oplen, flags,
	    mvxpsec_newsession, mvxpsec_freesession, mvxpsec_dispatch, sc);
	if (err)
		goto done;

	err = crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, oplen, flags,
	    mvxpsec_newsession, mvxpsec_freesession, mvxpsec_dispatch, sc);
	if (err)
		goto done;

	err = crypto_register(sc->sc_cid, CRYPTO_AES_CBC, oplen, flags,
	    mvxpsec_newsession, mvxpsec_freesession, mvxpsec_dispatch, sc);
	if (err)
		goto done;

	/* MACs */
	err = crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC_96,
	    oplen, flags,
	    mvxpsec_newsession, mvxpsec_freesession, mvxpsec_dispatch, sc);
	if (err)
		goto done;

	err = crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC_96,
	    oplen, flags,
	    mvxpsec_newsession, mvxpsec_freesession, mvxpsec_dispatch, sc);
	if (err)
		goto done;

#ifdef DEBUG
	log(LOG_DEBUG,
	    "%s: registered to opencrypto(max data = %d bytes)\n",
	    device_xname(sc->sc_dev), oplen);
#endif

	err = 0;
done:
	return err;
}

/*
 * Create new opencrypto session
 *
 *   - register cipher key, mac key.
 *   - initialize mac internal state.
 */
int
mvxpsec_newsession(void *arg, uint32_t *sidp, struct cryptoini *cri)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_session *mv_s = NULL;
	struct cryptoini *c;
	static int hint = 0;
	int session = -1;
	int sid;
	int err;
	int i;

	/* allocate driver session context */
	mv_s = mvxpsec_session_alloc(sc);
	if (mv_s == NULL)
		return ENOMEM;

	/*
	 * lookup opencrypto session table
	 *
	 * we have sc_session_mtx after here.
	 */
	mutex_enter(&sc->sc_session_mtx);
	if (sc->sc_nsessions >= MVXPSEC_MAX_SESSIONS) {
		mutex_exit(&sc->sc_session_mtx);
		log(LOG_ERR, "%s: too many IPsec SA(max %d)\n",
				__func__, MVXPSEC_MAX_SESSIONS);
		mvxpsec_session_dealloc(mv_s);
		return ENOMEM;
	}
	for (i = hint; i < MVXPSEC_MAX_SESSIONS; i++) {
		if (sc->sc_sessions[i])
			continue;
		session = i;
		hint = session + 1;
	       	break;
	}
	if (session < 0) {
		for (i = 0; i < hint; i++) {
			if (sc->sc_sessions[i])
				continue;
			session = i;
			hint = session + 1;
			break;
		}
		if (session < 0) {
			mutex_exit(&sc->sc_session_mtx);
			/* session full */
			log(LOG_ERR, "%s: too many IPsec SA(max %d)\n",
				__func__, MVXPSEC_MAX_SESSIONS);
			mvxpsec_session_dealloc(mv_s);
			hint = 0;
			return ENOMEM;
		}
	}
	if (hint >= MVXPSEC_MAX_SESSIONS)
		hint = 0;
	sc->sc_nsessions++;
	sc->sc_sessions[session] = mv_s;
#ifdef DEBUG
	log(LOG_DEBUG, "%s: new session %d allocated\n", __func__, session);
#endif

	sid = MVXPSEC_SID(device_unit(sc->sc_dev), session);
	mv_s->sid = sid;

	/* setup the session key ... */
	for (c = cri; c; c = c->cri_next) {
		switch (c->cri_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_AES_CBC:
			/* key */
			if (mvxpsec_key_precomp(c->cri_alg,
			    c->cri_key, c->cri_klen,
			    &mv_s->session_header.crp_key,
			    &mv_s->session_header.crp_key_d)) {
				log(LOG_ERR,
				    "%s: Invalid HMAC key for %s.\n",
				    __func__, s_ctlalg(c->cri_alg));
				err = EINVAL;
				goto fail;
			}
			if (mv_s->sflags & RDY_CRP_KEY) {
				log(LOG_WARNING,
				    "%s: overwrite cipher: %s->%s.\n",
				    __func__,
				    s_ctlalg(mv_s->cipher_alg),
				    s_ctlalg(c->cri_alg));
			}
			mv_s->sflags |= RDY_CRP_KEY;
			mv_s->enc_klen = c->cri_klen;
			mv_s->cipher_alg = c->cri_alg;
			/* create per session IV (compatible with KAME IPsec) */
			cprng_fast(&mv_s->session_iv, sizeof(mv_s->session_iv));
			mv_s->sflags |= RDY_CRP_IV;
			break;
		case CRYPTO_SHA1_HMAC_96:
		case CRYPTO_MD5_HMAC_96:
			/* key */
			if (mvxpsec_hmac_precomp(c->cri_alg,
			    c->cri_key, c->cri_klen,
			    (uint32_t *)&mv_s->session_header.miv_in,
			    (uint32_t *)&mv_s->session_header.miv_out)) {
				log(LOG_ERR,
				    "%s: Invalid MAC key\n", __func__);
				err = EINVAL;
				goto fail;
			}
			if (mv_s->sflags & RDY_MAC_KEY ||
			    mv_s->sflags & RDY_MAC_IV) {
				log(LOG_ERR,
				    "%s: overwrite HMAC: %s->%s.\n",
				    __func__, s_ctlalg(mv_s->hmac_alg),
				    s_ctlalg(c->cri_alg));
			}
			mv_s->sflags |= RDY_MAC_KEY;
			mv_s->sflags |= RDY_MAC_IV;

			mv_s->mac_klen = c->cri_klen;
			mv_s->hmac_alg = c->cri_alg;
			break;
		default:
			log(LOG_ERR, "%s: Unknown algorithm %d\n",
			    __func__, c->cri_alg);
			err = EINVAL;
			goto fail;
		}
	}
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "H/W Crypto session (id:%u) added.\n", session);

	*sidp = sid;
	MVXPSEC_EVCNT_INCR(sc, session_new);
	mutex_exit(&sc->sc_session_mtx);

	/* sync session header(it's never touched after here) */
	bus_dmamap_sync(sc->sc_dmat,
	    mv_s->session_header_map,
	    0, sizeof(mv_s->session_header),
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:
	sc->sc_nsessions--;
	sc->sc_sessions[session] = NULL;
	hint = session;
	if (mv_s)
		mvxpsec_session_dealloc(mv_s);
	log(LOG_WARNING,
	    "%s: Failed to add H/W crypto sessoin (id:%u): err=%d\n",
	   __func__, session, err);

	mutex_exit(&sc->sc_session_mtx);
	return err;
}

/*
 * remove opencrypto session
 */
int
mvxpsec_freesession(void *arg, uint64_t tid)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_session *mv_s;
	int session;
	uint32_t sid = ((uint32_t)tid) & 0xffffffff;

	session = MVXPSEC_SESSION(sid);
	if (session < 0 || session >= MVXPSEC_MAX_SESSIONS) {
		log(LOG_ERR, "%s: invalid session (id:%u)\n",
		    __func__, session);
		return EINVAL;
	}

	mutex_enter(&sc->sc_session_mtx);
	if ( (mv_s = sc->sc_sessions[session]) == NULL) {
		mutex_exit(&sc->sc_session_mtx);
#ifdef DEBUG
		log(LOG_DEBUG, "%s: session %d already inactivated\n",
		    __func__, session);
#endif
		return ENOENT;
	}
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "%s: inactivate session %d\n", __func__, session);

	/* inactivate mvxpsec session */
	sc->sc_sessions[session] = NULL;
	sc->sc_nsessions--;
	sc->sc_last_session = NULL;
	mutex_exit(&sc->sc_session_mtx);

	KASSERT(sc->sc_nsessions >= 0);
	KASSERT(mv_s->sid == sid);

	mvxpsec_session_dealloc(mv_s);
	MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
	    "H/W Crypto session (id: %d) deleted.\n", session);

	/* force unblock opencrypto */
	crypto_unblock(sc->sc_cid, CRYPTO_SYMQ|CRYPTO_ASYMQ);

	MVXPSEC_EVCNT_INCR(sc, session_free);

	return 0;
}

/*
 * process data with existing session
 */
int
mvxpsec_dispatch(void *arg, struct cryptop *crp, int hint)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_session *mv_s;
	struct mvxpsec_packet *mv_p;
	int q_full;
	int running;
	int err;

	mutex_enter(&sc->sc_queue_mtx);

	/*
	 * lookup session
	 */
	mutex_enter(&sc->sc_session_mtx);
	mv_s = mvxpsec_session_lookup(sc, crp->crp_sid);
	if (__predict_false(mv_s == NULL)) {
		err = EINVAL;
		mv_p = NULL;
		mutex_exit(&sc->sc_session_mtx);
		goto fail;
	}
	mv_p = mvxpsec_packet_alloc(mv_s);
	if (__predict_false(mv_p == NULL)) {
		mutex_exit(&sc->sc_session_mtx);
		mutex_exit(&sc->sc_queue_mtx);
		return ERESTART; /* => queued in opencrypto layer */
	}
	mutex_exit(&sc->sc_session_mtx);

	/*
	 * check queue status
	 */
#ifdef MVXPSEC_MULTI_PACKET
	q_full = (sc->sc_wait_qlen >= sc->sc_wait_qlimit) ? 1 : 0;
#else
	q_full = (sc->sc_wait_qlen != 0) ? 1 : 0;
#endif
	running = (sc->sc_flags & HW_RUNNING) ?  1: 0;
	if (q_full) {
		/* input queue is full. */
		if (!running && sc->sc_wait_qlen > 0)
			mvxpsec_dispatch_queue(sc);
		MVXPSEC_EVCNT_INCR(sc, queue_full);
		mvxpsec_packet_dealloc(mv_p);
		mutex_exit(&sc->sc_queue_mtx);
		return ERESTART; /* => queued in opencrypto layer */
	}

	/*
	 * Load and setup packet data
	 */
	err = mvxpsec_packet_setcrp(mv_p, crp);
	if (__predict_false(err))
		goto fail;
	
	/*
	 * Setup DMA descriptor chains
	 */
	mutex_enter(&sc->sc_dma_mtx);
	err = mvxpsec_dma_copy_packet(sc, mv_p);
	mutex_exit(&sc->sc_dma_mtx);
	if (__predict_false(err))
		goto fail;

#ifdef MVXPSEC_DEBUG
	mvxpsec_dump_packet(__func__, mv_p);
#endif

	/*
	 * Sync/inval the data cache
	 */
	err = mvxpsec_dma_sync_packet(sc, mv_p);
	if (__predict_false(err))
		goto fail;

	/*
	 * Enqueue the packet
	 */
	MVXPSEC_EVCNT_INCR(sc, dispatch_packets);
#ifdef MVXPSEC_MULTI_PACKET
	mvxpsec_packet_enqueue(mv_p);
	if (!running)
		mvxpsec_dispatch_queue(sc);
#else
	SIMPLEQ_INSERT_TAIL(&sc->sc_wait_queue, mv_p, queue);
	sc->sc_wait_qlen++;
	mv_p->flags |= SETUP_DONE;
	if (!running)
		mvxpsec_dispatch_queue(sc);
#endif
	mutex_exit(&sc->sc_queue_mtx);
	return 0;

fail:
	/* Drop the incoming packet */
	mvxpsec_drop(sc, crp, mv_p, err);
	mutex_exit(&sc->sc_queue_mtx);
	return 0;
}

/*
 * back the packet to the IP stack
 */
void
mvxpsec_done(void *arg)
{
	struct mvxpsec_softc *sc = arg;
	struct mvxpsec_packet *mv_p;
	mvxpsec_queue_t ret_queue;
	int ndone;

	mutex_enter(&sc->sc_queue_mtx);

	/* stop wdog timer */
	callout_stop(&sc->sc_timeout);

	/* refill MVXPSEC */
	ret_queue = sc->sc_run_queue;
	SIMPLEQ_INIT(&sc->sc_run_queue);
	sc->sc_flags &= ~HW_RUNNING;
	if (sc->sc_wait_qlen > 0)
		mvxpsec_dispatch_queue(sc);

	ndone = 0;
	while ( (mv_p = SIMPLEQ_FIRST(&ret_queue)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&ret_queue, queue);
		mvxpsec_dma_free(sc, &mv_p->dma_ring);
		mvxpsec_done_packet(mv_p);
		ndone++;
	}
	MVXPSEC_EVCNT_MAX(sc, max_done, ndone);

	mutex_exit(&sc->sc_queue_mtx);
}

/*
 * drop the packet
 */
INLINE void
mvxpsec_drop(struct mvxpsec_softc *sc, struct cryptop *crp,
    struct mvxpsec_packet *mv_p, int err)
{
	/* must called with sc->sc_queue_mtx held */
	KASSERT(mutex_owned(&sc->sc_queue_mtx));

	if (mv_p)
		mvxpsec_packet_dealloc(mv_p);
	if (err < 0)
		err = EINVAL;
	crp->crp_etype = err;
	crypto_done(crp);
	MVXPSEC_EVCNT_INCR(sc, packet_err);

	/* dispatch other packets in queue */
	if (sc->sc_wait_qlen > 0 &&
	    !(sc->sc_flags & HW_RUNNING))
		mvxpsec_dispatch_queue(sc);

	/* unblock driver for dropped packet */
	crypto_unblock(sc->sc_cid, CRYPTO_SYMQ|CRYPTO_ASYMQ);
}

/* move wait queue entry to run queue */
STATIC int
mvxpsec_dispatch_queue(struct mvxpsec_softc *sc)
{
	struct mvxpsec_packet *mv_p;
	paddr_t head;
	int ndispatch = 0;

	/* must called with sc->sc_queue_mtx held */
	KASSERT(mutex_owned(&sc->sc_queue_mtx));

	/* check there is any task */
	if (__predict_false(sc->sc_flags & HW_RUNNING)) {
		log(LOG_WARNING,
		    "%s: another packet already exist.\n", __func__);
		return 0;
	}
	if (__predict_false(SIMPLEQ_EMPTY(&sc->sc_wait_queue))) {
		log(LOG_WARNING,
		    "%s: no waiting packet yet(qlen=%d).\n",
		    __func__, sc->sc_wait_qlen);
		return 0;
	}

	/* move queue */
	sc->sc_run_queue = sc->sc_wait_queue;
	sc->sc_flags |= HW_RUNNING; /* dropped by intr or timeout */
	SIMPLEQ_INIT(&sc->sc_wait_queue);
	ndispatch = sc->sc_wait_qlen;
	sc->sc_wait_qlen = 0;

	/* get 1st DMA descriptor */
	mv_p = SIMPLEQ_FIRST(&sc->sc_run_queue);
	head = mv_p->dma_ring.dma_head->phys_addr;

	/* terminate last DMA descriptor */
	mv_p = SIMPLEQ_LAST(&sc->sc_run_queue, mvxpsec_packet, queue);
	mvxpsec_dma_finalize(sc, &mv_p->dma_ring);

	/* configure TDMA */
	if (mvxpsec_dma_wait(sc) < 0) {
		log(LOG_ERR, "%s: DMA DEVICE not responding", __func__);
		callout_schedule(&sc->sc_timeout, hz);
		return 0;
	}
	MVXPSEC_WRITE(sc, MV_TDMA_NXT, head);

	/* trigger ACC */
	if (mvxpsec_acc_wait(sc) < 0) {
		log(LOG_ERR, "%s: MVXPSEC not responding", __func__);
		callout_schedule(&sc->sc_timeout, hz);
		return 0;
	}
	MVXPSEC_WRITE(sc, MV_ACC_COMMAND, MV_ACC_COMMAND_ACT);

	MVXPSEC_EVCNT_MAX(sc, max_dispatch, ndispatch);
	MVXPSEC_EVCNT_INCR(sc, dispatch_queue);
	callout_schedule(&sc->sc_timeout, hz);
	return 0;
}

/*
 * process opencrypto operations(cryptop) for packets.
 */
INLINE int
mvxpsec_parse_crd(struct mvxpsec_packet *mv_p, struct cryptodesc *crd)
{
	int ivlen;

	KASSERT(mv_p->flags & RDY_DATA);

	/* MAC & Ciphers: set data location and operation */
	switch (crd->crd_alg) {
	case CRYPTO_SHA1_HMAC_96:
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_MAC_96;
		/* fall through */
	case CRYPTO_SHA1_HMAC:
		mv_p->mac_dst = crd->crd_inject;
		mv_p->mac_off = crd->crd_skip;
		mv_p->mac_len = crd->crd_len;
		MV_ACC_CRYPTO_MAC_SET(mv_p->pkt_header.desc.acc_config,
		    MV_ACC_CRYPTO_MAC_HMAC_SHA1);
		mvxpsec_packet_update_op_order(mv_p, MV_ACC_CRYPTO_OP_MAC);
		/* No more setup for MAC */
		return 0;
	case CRYPTO_MD5_HMAC_96:
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_MAC_96;
		/* fall through */
	case CRYPTO_MD5_HMAC:
		mv_p->mac_dst = crd->crd_inject;
		mv_p->mac_off = crd->crd_skip;
		mv_p->mac_len = crd->crd_len;
		MV_ACC_CRYPTO_MAC_SET(mv_p->pkt_header.desc.acc_config,
		    MV_ACC_CRYPTO_MAC_HMAC_MD5);
		mvxpsec_packet_update_op_order(mv_p, MV_ACC_CRYPTO_OP_MAC);
		/* No more setup for MAC */
		return 0;
	case CRYPTO_DES_CBC:
		mv_p->enc_ivoff = crd->crd_inject;
		mv_p->enc_off = crd->crd_skip;
		mv_p->enc_len = crd->crd_len;
		ivlen = 8;
		MV_ACC_CRYPTO_ENC_SET(mv_p->pkt_header.desc.acc_config,
		    MV_ACC_CRYPTO_ENC_DES);
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_CBC;
		mvxpsec_packet_update_op_order(mv_p, MV_ACC_CRYPTO_OP_ENC);
		break;
	case CRYPTO_3DES_CBC:
		mv_p->enc_ivoff = crd->crd_inject;
		mv_p->enc_off = crd->crd_skip;
		mv_p->enc_len = crd->crd_len;
		ivlen = 8;
		MV_ACC_CRYPTO_ENC_SET(mv_p->pkt_header.desc.acc_config,
		    MV_ACC_CRYPTO_ENC_3DES);
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_CBC;
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_3DES_EDE;
		mvxpsec_packet_update_op_order(mv_p, MV_ACC_CRYPTO_OP_ENC);
		break;
	case CRYPTO_AES_CBC:
		mv_p->enc_ivoff = crd->crd_inject;
		mv_p->enc_off = crd->crd_skip;
		mv_p->enc_len = crd->crd_len;
		ivlen = 16;
		MV_ACC_CRYPTO_ENC_SET(mv_p->pkt_header.desc.acc_config,
		    MV_ACC_CRYPTO_ENC_AES);
		MV_ACC_CRYPTO_AES_KLEN_SET(
		    mv_p->pkt_header.desc.acc_config,
		   mvxpsec_aesklen(mv_p->mv_s->enc_klen));
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_CBC;
		mvxpsec_packet_update_op_order(mv_p, MV_ACC_CRYPTO_OP_ENC);
		break;
	default:
		log(LOG_ERR, "%s: Unknown algorithm %d\n",
		    __func__, crd->crd_alg);
		return EINVAL;
	}

	/* Operations only for Cipher, not MAC */
	if (crd->crd_flags & CRD_F_ENCRYPT) {
		/* Ciphers: Originate IV for Encryption.*/
		mv_p->pkt_header.desc.acc_config &= ~MV_ACC_CRYPTO_DECRYPT;
		mv_p->flags |= DIR_ENCRYPT;

		if (crd->crd_flags & CRD_F_IV_EXPLICIT) {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_ENC_IV, "EXPLICIT IV\n");
			mv_p->flags |= CRP_EXT_IV;
			mvxpsec_packet_write_iv(mv_p, crd->crd_iv, ivlen);
			mv_p->enc_ivoff = MVXPSEC_SRAM_IV_EXT_OFF;
		}
		else if (crd->crd_flags & CRD_F_IV_PRESENT) {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_ENC_IV, "IV is present\n");
			mvxpsec_packet_copy_iv(mv_p, crd->crd_inject, ivlen);
		}
		else {
			MVXPSEC_PRINTF(MVXPSEC_DEBUG_ENC_IV, "Create New IV\n");
			mvxpsec_packet_write_iv(mv_p, NULL, ivlen);
		}
	}
	else {
		/* Ciphers: IV is loadded from crd_inject when it's present */
		mv_p->pkt_header.desc.acc_config |= MV_ACC_CRYPTO_DECRYPT;
		mv_p->flags |= DIR_DECRYPT;

		if (crd->crd_flags & CRD_F_IV_EXPLICIT) {
#ifdef MVXPSEC_DEBUG
			if (mvxpsec_debug & MVXPSEC_DEBUG_ENC_IV) {
				MVXPSEC_PRINTF(MVXPSEC_DEBUG_ENC_IV,
				    "EXPLICIT IV(Decrypt)\n");
				mvxpsec_dump_data(__func__, crd->crd_iv, ivlen);
			}
#endif
			mv_p->flags |= CRP_EXT_IV;
			mvxpsec_packet_write_iv(mv_p, crd->crd_iv, ivlen);
			mv_p->enc_ivoff = MVXPSEC_SRAM_IV_EXT_OFF;
		}
	}

	KASSERT(!((mv_p->flags & DIR_ENCRYPT) && (mv_p->flags & DIR_DECRYPT)));

	return 0;
}

INLINE int
mvxpsec_parse_crp(struct mvxpsec_packet *mv_p)
{
	struct cryptop *crp = mv_p->crp;
	struct cryptodesc *crd;
	int err;

	KASSERT(crp);

	mvxpsec_packet_reset_op(mv_p);

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		err = mvxpsec_parse_crd(mv_p, crd);
		if (err)
			return err;
	}

	return 0;
}

INLINE int
mvxpsec_packet_setcrp(struct mvxpsec_packet *mv_p, struct cryptop *crp)
{
	int err = EINVAL;

	/* regiseter crp to the MVXPSEC packet */
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		err = mvxpsec_packet_setmbuf(mv_p,
		    (struct mbuf *)crp->crp_buf);
		mv_p->crp = crp;
	}
	else if (crp->crp_flags & CRYPTO_F_IOV) {
		err = mvxpsec_packet_setuio(mv_p,
		    (struct uio *)crp->crp_buf);
		mv_p->crp = crp;
	}
	else {
		err = mvxpsec_packet_setdata(mv_p,
		    (struct mbuf *)crp->crp_buf, crp->crp_ilen);
		mv_p->crp = crp;
	}
	if (__predict_false(err))
		return err;

	/* parse crp and setup MVXPSEC registers/descriptors */
	err = mvxpsec_parse_crp(mv_p);
	if (__predict_false(err))
		return err;

	/* fixup data offset to fit MVXPSEC internal SRAM */
	err = mvxpsec_header_finalize(mv_p);
	if (__predict_false(err))
		return err;

	return 0;
}

/*
 * load data for encrypt/decrypt/authentication
 *
 * data is raw kernel memory area.
 */
STATIC int
mvxpsec_packet_setdata(struct mvxpsec_packet *mv_p,
    void *data, uint32_t data_len)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	struct mvxpsec_softc *sc = mv_s->sc;

	if (bus_dmamap_load(sc->sc_dmat, mv_p->data_map, data, data_len,
	    NULL, BUS_DMA_NOWAIT)) {
		log(LOG_ERR, "%s: cannot load data\n", __func__);
		return -1;
	}
	mv_p->data_type = MVXPSEC_DATA_RAW;
	mv_p->data_raw = data;
	mv_p->data_len = data_len;
	mv_p->flags |= RDY_DATA;

	return 0;
}

/*
 * load data for encrypt/decrypt/authentication
 *
 * data is mbuf based network data.
 */
STATIC int
mvxpsec_packet_setmbuf(struct mvxpsec_packet *mv_p, struct mbuf *m)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	struct mvxpsec_softc *sc = mv_s->sc;
	size_t pktlen = 0;

	if (__predict_true(m->m_flags & M_PKTHDR))
		pktlen = m->m_pkthdr.len;
	else {
		struct mbuf *mp = m;

		while (mp != NULL) {
			pktlen += m->m_len;
			mp = mp->m_next;
		}
	}
	if (pktlen > SRAM_PAYLOAD_SIZE) {
		extern   percpu_t *espstat_percpu;
	       	/* XXX:
		 * layer violation. opencrypto knows our max packet size
		 * from crypto_register(9) API.
		 */

		_NET_STATINC(espstat_percpu, ESP_STAT_TOOBIG);
		log(LOG_ERR,
		    "%s: ESP Packet too large: %zu [oct.] > %zu [oct.]\n",
		    device_xname(sc->sc_dev),
		    (size_t)pktlen, SRAM_PAYLOAD_SIZE);
		mv_p->data_type = MVXPSEC_DATA_NONE;
		mv_p->data_mbuf = NULL;
		return -1;
	}

	if (bus_dmamap_load_mbuf(sc->sc_dmat, mv_p->data_map, m,
	    BUS_DMA_NOWAIT)) {
		mv_p->data_type = MVXPSEC_DATA_NONE;
		mv_p->data_mbuf = NULL;
		log(LOG_ERR, "%s: cannot load mbuf\n", __func__);
		return -1;
	}

	/* set payload buffer */
	mv_p->data_type = MVXPSEC_DATA_MBUF;
	mv_p->data_mbuf = m;
	if (m->m_flags & M_PKTHDR) {
		mv_p->data_len = m->m_pkthdr.len;
	}
	else {
		mv_p->data_len = 0;
		while (m) {
			mv_p->data_len += m->m_len;
			m = m->m_next;
		}
	}
	mv_p->flags |= RDY_DATA;

	return 0;
}

STATIC int
mvxpsec_packet_setuio(struct mvxpsec_packet *mv_p, struct uio *uio)
{
	struct mvxpsec_session *mv_s = mv_p->mv_s;
	struct mvxpsec_softc *sc = mv_s->sc;

	if (uio->uio_resid > SRAM_PAYLOAD_SIZE) {
		extern   percpu_t *espstat_percpu;
	       	/* XXX:
		 * layer violation. opencrypto knows our max packet size
		 * from crypto_register(9) API.
		 */

		_NET_STATINC(espstat_percpu, ESP_STAT_TOOBIG);
		log(LOG_ERR,
		    "%s: uio request too large: %zu [oct.] > %zu [oct.]\n",
		    device_xname(sc->sc_dev),
		    uio->uio_resid, SRAM_PAYLOAD_SIZE);
		mv_p->data_type = MVXPSEC_DATA_NONE;
		mv_p->data_mbuf = NULL;
		return -1;
	}

	if (bus_dmamap_load_uio(sc->sc_dmat, mv_p->data_map, uio,
	    BUS_DMA_NOWAIT)) {
		mv_p->data_type = MVXPSEC_DATA_NONE;
		mv_p->data_mbuf = NULL;
		log(LOG_ERR, "%s: cannot load uio buf\n", __func__);
		return -1;
	}

	/* set payload buffer */
	mv_p->data_type = MVXPSEC_DATA_UIO;
	mv_p->data_uio = uio;
	mv_p->data_len = uio->uio_resid;
	mv_p->flags |= RDY_DATA;

	return 0;
}

STATIC int
mvxpsec_packet_rdata(struct mvxpsec_packet *mv_p,
    int off, int len, void *cp)
{
	uint8_t *p;

	if (mv_p->data_type == MVXPSEC_DATA_RAW) {
		p = (uint8_t *)mv_p->data_raw + off;
		memcpy(cp, p, len);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_MBUF) {
		m_copydata(mv_p->data_mbuf, off, len, cp);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_UIO) {
		cuio_copydata(mv_p->data_uio, off, len, cp);
	}
	else
		return -1;

	return 0;
}

STATIC int
mvxpsec_packet_wdata(struct mvxpsec_packet *mv_p,
    int off, int len, void *cp)
{
	uint8_t *p;

	if (mv_p->data_type == MVXPSEC_DATA_RAW) {
		p = (uint8_t *)mv_p->data_raw + off;
		memcpy(p, cp, len);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_MBUF) {
		m_copyback(mv_p->data_mbuf, off, len, cp);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_UIO) {
		cuio_copyback(mv_p->data_uio, off, len, cp);
	}
	else
		return -1;

	return 0;
}

/*
 * Set initial vector of cipher to the session.
 */
STATIC int
mvxpsec_packet_write_iv(struct mvxpsec_packet *mv_p, void *iv, int ivlen)
{
	uint8_t ivbuf[16];
	
	KASSERT(ivlen == 8 || ivlen == 16);

	if (iv == NULL) {
	       	if (mv_p->mv_s->sflags & RDY_CRP_IV) {
			/* use per session IV (compatible with KAME IPsec) */
			mv_p->pkt_header.crp_iv_work = mv_p->mv_s->session_iv;
			mv_p->flags |= RDY_CRP_IV;
			return 0;
		}
		cprng_fast(ivbuf, ivlen);
		iv = ivbuf;
	}
	memcpy(&mv_p->pkt_header.crp_iv_work, iv, ivlen);
	if (mv_p->flags & CRP_EXT_IV) {
		memcpy(&mv_p->pkt_header.crp_iv_ext, iv, ivlen);
		mv_p->ext_iv = iv;
		mv_p->ext_ivlen = ivlen;
	}
	mv_p->flags |= RDY_CRP_IV;

	return 0;
}

STATIC int
mvxpsec_packet_copy_iv(struct mvxpsec_packet *mv_p, int off, int ivlen)
{
	mvxpsec_packet_rdata(mv_p, off, ivlen,
	    &mv_p->pkt_header.crp_iv_work);
	mv_p->flags |= RDY_CRP_IV;

	return 0;
}

/*
 * set a encryption or decryption key to the session
 *
 * Input key material is big endian.
 */
STATIC int
mvxpsec_key_precomp(int alg, void *keymat, int kbitlen,
    void *key_encrypt, void *key_decrypt)
{
	uint32_t *kp = keymat;
	uint32_t *ekp = key_encrypt;
	uint32_t *dkp = key_decrypt;
	int i;

	switch (alg) {
	case CRYPTO_DES_CBC:
		if (kbitlen < 64 || (kbitlen % 8) != 0) {
			log(LOG_WARNING,
			    "mvxpsec: invalid DES keylen %d\n", kbitlen);
			return EINVAL;
		}
		for (i = 0; i < 2; i++)
			dkp[i] = ekp[i] = kp[i];
		for (; i < 8; i++)
			dkp[i] = ekp[i] = 0;
		break;
	case CRYPTO_3DES_CBC:
		if (kbitlen < 192 || (kbitlen % 8) != 0) {
			log(LOG_WARNING,
			    "mvxpsec: invalid 3DES keylen %d\n", kbitlen);
			return EINVAL;
		}
		for (i = 0; i < 8; i++)
			dkp[i] = ekp[i] = kp[i];
		break;
	case CRYPTO_AES_CBC:
		if (kbitlen < 128) {
			log(LOG_WARNING,
			    "mvxpsec: invalid AES keylen %d\n", kbitlen);
			return EINVAL;
		}
		else if (kbitlen < 192) {
			/* AES-128 */
			for (i = 0; i < 4; i++)
				ekp[i] = kp[i];
			for (; i < 8; i++)
				ekp[i] = 0;
		}
	       	else if (kbitlen < 256) {
			/* AES-192 */
			for (i = 0; i < 6; i++)
				ekp[i] = kp[i];
			for (; i < 8; i++)
				ekp[i] = 0;
		}
		else  {
			/* AES-256 */
			for (i = 0; i < 8; i++)
				ekp[i] = kp[i];
		}
		/* make decryption key */
		mv_aes_deckey((uint8_t *)dkp, (uint8_t *)ekp, kbitlen);
		break;
	default:
		for (i = 0; i < 8; i++)
			ekp[0] = dkp[0] = 0;
		break;
	}

#ifdef MVXPSEC_DEBUG
	if (mvxpsec_debug & MVXPSEC_DEBUG_OPENCRYPTO) {
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: keyregistered\n", __func__);
		mvxpsec_dump_data(__func__, ekp, 32);
	}
#endif

	return 0;
}

/*
 * set MAC key to the session
 *
 * MAC engine has no register for key itself, but the engine has
 * inner and outer IV register. software must compute IV before
 * enable the engine.
 *
 * IV is a hash of ipad/opad. these are defined by FIPS-198a
 * standard.
 */
STATIC int
mvxpsec_hmac_precomp(int alg, void *key, int kbitlen,
    void *iv_inner, void *iv_outer)
{
	SHA1_CTX sha1;
	MD5_CTX md5;
	uint8_t *key8 = key;
	uint8_t kbuf[64];
	uint8_t ipad[64];
	uint8_t opad[64];
	uint32_t *iv_in = iv_inner;
	uint32_t *iv_out = iv_outer;
	int kbytelen;
	int i;
#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5c

	kbytelen = kbitlen / 8;
	KASSERT(kbitlen == kbytelen * 8);
	if (kbytelen > 64) {
		SHA1Init(&sha1);
		SHA1Update(&sha1, key, kbytelen);
		SHA1Final(kbuf, &sha1);
		key8 = kbuf;
		kbytelen = 64;
	}

	/* make initial 64 oct. string */
	switch (alg) {
	case CRYPTO_SHA1_HMAC_96:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_MD5_HMAC_96:
	case CRYPTO_MD5_HMAC:
		for (i = 0; i < kbytelen; i++) {
			ipad[i] = (key8[i] ^ HMAC_IPAD);
			opad[i] = (key8[i] ^ HMAC_OPAD);
		}
		for (; i < 64; i++) {
			ipad[i] = HMAC_IPAD;
			opad[i] = HMAC_OPAD;
		}
		break;
	default:
		break;
	}
#ifdef MVXPSEC_DEBUG
	if (mvxpsec_debug & MVXPSEC_DEBUG_OPENCRYPTO) {
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: HMAC-KEY Pre-comp:\n", __func__);
		mvxpsec_dump_data(__func__, key, 64);
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: ipad:\n", __func__);
		mvxpsec_dump_data(__func__, ipad, sizeof(ipad));
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: opad:\n", __func__);
		mvxpsec_dump_data(__func__, opad, sizeof(opad));
	}
#endif

	/* make iv from string */
	switch (alg) {
	case CRYPTO_SHA1_HMAC_96:
	case CRYPTO_SHA1_HMAC:
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: Generate iv_in(SHA1)\n", __func__);
		SHA1Init(&sha1);
		SHA1Update(&sha1, ipad, 64);
		/* XXX: private state... (LE) */
		iv_in[0] = htobe32(sha1.state[0]);
		iv_in[1] = htobe32(sha1.state[1]);
		iv_in[2] = htobe32(sha1.state[2]);
		iv_in[3] = htobe32(sha1.state[3]);
		iv_in[4] = htobe32(sha1.state[4]);

		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: Generate iv_out(SHA1)\n", __func__);
		SHA1Init(&sha1);
		SHA1Update(&sha1, opad, 64);
		/* XXX: private state... (LE) */
		iv_out[0] = htobe32(sha1.state[0]);
		iv_out[1] = htobe32(sha1.state[1]);
		iv_out[2] = htobe32(sha1.state[2]);
		iv_out[3] = htobe32(sha1.state[3]);
		iv_out[4] = htobe32(sha1.state[4]);
		break;
	case CRYPTO_MD5_HMAC_96:
	case CRYPTO_MD5_HMAC:
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: Generate iv_in(MD5)\n", __func__);
		MD5Init(&md5);
		MD5Update(&md5, ipad, sizeof(ipad));
		/* XXX: private state... (LE) */
		iv_in[0] = htobe32(md5.state[0]);
		iv_in[1] = htobe32(md5.state[1]);
		iv_in[2] = htobe32(md5.state[2]);
		iv_in[3] = htobe32(md5.state[3]);
		iv_in[4] = 0;

		MVXPSEC_PRINTF(MVXPSEC_DEBUG_OPENCRYPTO,
		    "%s: Generate iv_out(MD5)\n", __func__);
		MD5Init(&md5);
		MD5Update(&md5, opad, sizeof(opad));
		/* XXX: private state... (LE) */
		iv_out[0] = htobe32(md5.state[0]);
		iv_out[1] = htobe32(md5.state[1]);
		iv_out[2] = htobe32(md5.state[2]);
		iv_out[3] = htobe32(md5.state[3]);
		iv_out[4] = 0;
		break;
	default:
		break;
	}

#ifdef MVXPSEC_DEBUG
	if (mvxpsec_debug & MVXPSEC_DEBUG_HASH_IV) {
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_HASH_IV,
		    "%s: HMAC IV-IN\n", __func__);
		mvxpsec_dump_data(__func__, (uint8_t *)iv_in, 20);
		MVXPSEC_PRINTF(MVXPSEC_DEBUG_HASH_IV,
		    "%s: HMAC IV-OUT\n", __func__);
		mvxpsec_dump_data(__func__, (uint8_t *)iv_out, 20);
	}
#endif

	return 0;
#undef HMAC_IPAD
#undef HMAC_OPAD
}

/*
 * AES Support routine
 */
static uint8_t AES_SBOX[256] = {
	 99, 124, 119, 123, 242, 107, 111, 197,  48,   1, 103,  43, 254, 215,
       	171, 118, 202, 130, 201, 125, 250,  89,  71, 240, 173, 212, 162, 175,
       	156, 164, 114, 192, 183, 253, 147,  38,  54,  63, 247, 204,  52, 165,
       	229, 241, 113, 216,  49,  21,   4, 199,  35, 195,  24, 150,   5, 154,
       	  7,  18, 128, 226, 235,  39, 178, 117,   9, 131,  44,  26,  27, 110,
	 90, 160,  82,  59, 214, 179,  41, 227,  47, 132,  83, 209,   0, 237,
       	 32, 252, 177,  91, 106, 203, 190,  57,  74,  76,  88, 207, 208, 239,
	170, 251,  67,  77,  51, 133,  69, 249,   2, 127,  80,  60, 159, 168, 
	 81, 163,  64, 143, 146, 157,  56, 245, 188, 182, 218,  33,  16, 255,
	243, 210, 205,  12,  19, 236,  95, 151,  68,  23, 196, 167, 126,  61,
       	100,  93,  25, 115,  96, 129,  79, 220,  34,  42, 144, 136,  70, 238,
       	184,  20, 222,  94,  11, 219, 224,  50,  58,  10,  73,   6,  36,  92,
       	194, 211, 172,  98, 145, 149, 228, 121, 231, 200,  55, 109, 141, 213,
      	 78, 169, 108,  86, 244, 234, 101, 122, 174,   8, 186, 120,  37,  46,
       	 28, 166, 180, 198, 232, 221, 116,  31,  75, 189, 139, 138, 112,  62,
	181, 102,  72,   3, 246,  14,  97,  53,  87, 185, 134, 193,  29, 158,
       	225, 248, 152,  17, 105, 217, 142, 148, 155,  30, 135, 233, 206,  85,
      	 40, 223, 140, 161, 137,  13, 191, 230,  66, 104,  65, 153,  45,  15,
	176,  84, 187,  22
};

static uint32_t AES_RCON[30] = { 
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8,
       	0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4,
       	0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91
};

STATIC int
mv_aes_ksched(uint8_t k[4][MAXKC], int keyBits,
    uint8_t W[MAXROUNDS+1][4][MAXBC]) 
{
	int KC, BC, ROUNDS;
	int i, j, t, rconpointer = 0;
	uint8_t tk[4][MAXKC];   

	switch (keyBits) {
	case 128:
		ROUNDS = 10;
		KC = 4;
		break;
	case 192:
		ROUNDS = 12;
		KC = 6;
	       	break;
	case 256:
		ROUNDS = 14;
	       	KC = 8;
	       	break;
	default:
	       	return (-1);
	}
	BC = 4; /* 128 bits */

	for(j = 0; j < KC; j++)
		for(i = 0; i < 4; i++)
			tk[i][j] = k[i][j];
	t = 0;

	/* copy values into round key array */
	for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
		for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk[i][j];
		
	while (t < (ROUNDS+1)*BC) { /* while not enough round key material calculated */
		/* calculate new values */
		for(i = 0; i < 4; i++)
			tk[i][0] ^= AES_SBOX[tk[(i+1)%4][KC-1]];
		tk[0][0] ^= AES_RCON[rconpointer++];

		if (KC != 8)
			for(j = 1; j < KC; j++)
				for(i = 0; i < 4; i++)
				       	tk[i][j] ^= tk[i][j-1];
		else {
			for(j = 1; j < KC/2; j++)
				for(i = 0; i < 4; i++)
				       	tk[i][j] ^= tk[i][j-1];
			for(i = 0; i < 4; i++)
			       	tk[i][KC/2] ^= AES_SBOX[tk[i][KC/2 - 1]];
			for(j = KC/2 + 1; j < KC; j++)
				for(i = 0; i < 4; i++)
				       	tk[i][j] ^= tk[i][j-1];
	}
	/* copy values into round key array */
	for(j = 0; (j < KC) && (t < (ROUNDS+1)*BC); j++, t++)
		for(i = 0; i < 4; i++) W[t / BC][i][t % BC] = tk[i][j];
	}		

	return 0;
}
      
STATIC int
mv_aes_deckey(uint8_t *expandedKey, uint8_t *keyMaterial, int keyLen)
{
	uint8_t   W[MAXROUNDS+1][4][MAXBC];
	uint8_t   k[4][MAXKC];
	uint8_t   j;
	int     i, rounds, KC;

	if (expandedKey == NULL)
		return -1;

	if (!((keyLen == 128) || (keyLen == 192) || (keyLen == 256))) 
		return -1;

	if (keyMaterial == NULL) 
		return -1;

	/* initialize key schedule: */ 
	for (i=0; i<keyLen/8; i++) {
		j = keyMaterial[i];
		k[i % 4][i / 4] = j; 
	}

	mv_aes_ksched(k, keyLen, W);
	switch (keyLen) {
	case 128: 
		rounds = 10;
		KC = 4; 
		break;
	case 192: 
		rounds = 12;
		KC = 6; 
		break;
	case 256: 
		rounds = 14;
		KC = 8; 
		break;
	default:
		return -1;
	}

	for(i=0; i<MAXBC; i++)
		for(j=0; j<4; j++)
			expandedKey[i*4+j] = W[rounds][j][i];
	for(; i<KC; i++)
		for(j=0; j<4; j++)
			expandedKey[i*4+j] = W[rounds-1][j][i+MAXBC-KC];

	return 0;
}

/*
 * Clear cipher/mac operation state
 */
INLINE void
mvxpsec_packet_reset_op(struct mvxpsec_packet *mv_p)
{
	mv_p->pkt_header.desc.acc_config = 0;
	mv_p->enc_off = mv_p->enc_ivoff = mv_p->enc_len = 0;
	mv_p->mac_off = mv_p->mac_dst = mv_p->mac_len = 0;
}

/*
 * update MVXPSEC operation order
 */
INLINE void
mvxpsec_packet_update_op_order(struct mvxpsec_packet *mv_p, int op)
{
	struct mvxpsec_acc_descriptor *acc_desc = &mv_p->pkt_header.desc;
	uint32_t cur_op = acc_desc->acc_config & MV_ACC_CRYPTO_OP_MASK;

	KASSERT(op == MV_ACC_CRYPTO_OP_MAC || op == MV_ACC_CRYPTO_OP_ENC);
	KASSERT((op & MV_ACC_CRYPTO_OP_MASK) == op);

	if (cur_op == 0)
		acc_desc->acc_config |= op;
	else if (cur_op == MV_ACC_CRYPTO_OP_MAC && op == MV_ACC_CRYPTO_OP_ENC) {
		acc_desc->acc_config &= ~MV_ACC_CRYPTO_OP_MASK;
		acc_desc->acc_config |= MV_ACC_CRYPTO_OP_MACENC;
		/* MAC then ENC (= decryption) */
	}
	else if (cur_op == MV_ACC_CRYPTO_OP_ENC && op == MV_ACC_CRYPTO_OP_MAC) {
		acc_desc->acc_config &= ~MV_ACC_CRYPTO_OP_MASK;
		acc_desc->acc_config |= MV_ACC_CRYPTO_OP_ENCMAC;
		/* ENC then MAC (= encryption) */
	}
	else {
		log(LOG_ERR, "%s: multiple %s algorithm is not supported.\n",
		    __func__,
		    (op == MV_ACC_CRYPTO_OP_ENC) ?  "encryption" : "authentication");
	}
}

/*
 * Parameter Conversions
 */
INLINE uint32_t
mvxpsec_alg2acc(uint32_t alg)
{
	uint32_t reg;

	switch (alg) {
	case CRYPTO_DES_CBC:
		reg = MV_ACC_CRYPTO_ENC_DES;
		reg |= MV_ACC_CRYPTO_CBC;
		break;
	case CRYPTO_3DES_CBC:
		reg = MV_ACC_CRYPTO_ENC_3DES;
		reg |= MV_ACC_CRYPTO_3DES_EDE;
		reg |= MV_ACC_CRYPTO_CBC;
		break;
	case CRYPTO_AES_CBC:
		reg = MV_ACC_CRYPTO_ENC_AES;
		reg |= MV_ACC_CRYPTO_CBC;
		break;
	case CRYPTO_SHA1_HMAC_96:
		reg = MV_ACC_CRYPTO_MAC_HMAC_SHA1;
		reg |= MV_ACC_CRYPTO_MAC_96;
		break;
	case CRYPTO_MD5_HMAC_96:
		reg = MV_ACC_CRYPTO_MAC_HMAC_MD5;
		reg |= MV_ACC_CRYPTO_MAC_96;
		break;
	default:
		reg = 0;
		break;
	}

	return reg;
}

INLINE uint32_t
mvxpsec_aesklen(int klen)
{
	if (klen < 128)
		return 0;
	else if (klen < 192)
		return MV_ACC_CRYPTO_AES_KLEN_128;
	else if (klen < 256)
		return MV_ACC_CRYPTO_AES_KLEN_192;
	else
		return MV_ACC_CRYPTO_AES_KLEN_256;

	return 0;
}

/*
 * String Conversions
 */
STATIC const char *
s_errreg(uint32_t v)
{
	static char buf[80];

	snprintf(buf, sizeof(buf),
	    "%sMiss %sDoubleHit %sBothHit %sDataError",
	    (v & MV_TDMA_ERRC_MISS) ? "+" : "-",
	    (v & MV_TDMA_ERRC_DHIT) ? "+" : "-",
	    (v & MV_TDMA_ERRC_BHIT) ? "+" : "-",
	    (v & MV_TDMA_ERRC_DERR) ? "+" : "-");

	return (const char *)buf;
}

STATIC const char *
s_winreg(uint32_t v)
{
	static char buf[80];
	
	snprintf(buf, sizeof(buf),
	    "%s TGT 0x%x ATTR 0x%02x size %u(0x%04x)[64KB]",
	    (v & MV_TDMA_ATTR_ENABLE) ? "EN" : "DIS",
	    MV_TDMA_ATTR_GET_TARGET(v), MV_TDMA_ATTR_GET_ATTR(v),
	    MV_TDMA_ATTR_GET_SIZE(v), MV_TDMA_ATTR_GET_SIZE(v));

	return (const char *)buf;
}

STATIC const char *
s_ctrlreg(uint32_t reg)
{
	static char buf[80];
	
	snprintf(buf, sizeof(buf),
	    "%s: %sFETCH DBURST-%u SBURST-%u %sOUTS %sCHAIN %sBSWAP %sACT",
	    (reg & MV_TDMA_CONTROL_ENABLE) ? "ENABLE" : "DISABLE",
	    (reg & MV_TDMA_CONTROL_FETCH) ? "+" : "-",
	    MV_TDMA_CONTROL_GET_DST_BURST(reg),
	    MV_TDMA_CONTROL_GET_SRC_BURST(reg),
	    (reg & MV_TDMA_CONTROL_OUTS_EN) ? "+" : "-",
	    (reg & MV_TDMA_CONTROL_CHAIN_DIS) ? "-" : "+",
	    (reg & MV_TDMA_CONTROL_BSWAP_DIS) ? "-" : "+",
	    (reg & MV_TDMA_CONTROL_ACT) ? "+" : "-");

	return (const char *)buf;
}

_STATIC const char *
s_xpsecintr(uint32_t v)
{
	static char buf[160];

	snprintf(buf, sizeof(buf),
	    "%sAuth %sDES %sAES-ENC %sAES-DEC %sENC %sSA %sAccAndTDMA "
	    "%sTDMAComp %sTDMAOwn %sAccAndTDMA_Cont",
	    (v & MVXPSEC_INT_AUTH) ? "+" : "-",
	    (v & MVXPSEC_INT_DES) ? "+" : "-",
	    (v & MVXPSEC_INT_AES_ENC) ? "+" : "-",
	    (v & MVXPSEC_INT_AES_DEC) ? "+" : "-",
	    (v & MVXPSEC_INT_ENC) ? "+" : "-",
	    (v & MVXPSEC_INT_SA) ? "+" : "-",
	    (v & MVXPSEC_INT_ACCTDMA) ? "+" : "-",
	    (v & MVXPSEC_INT_TDMA_COMP) ? "+" : "-",
	    (v & MVXPSEC_INT_TDMA_OWN) ? "+" : "-",
	    (v & MVXPSEC_INT_ACCTDMA_CONT) ? "+" : "-");

	return (const char *)buf;
}

STATIC const char *
s_ctlalg(uint32_t alg)
{
	switch (alg) {
	case CRYPTO_SHA1_HMAC_96:
		return "HMAC-SHA1-96";
	case CRYPTO_SHA1_HMAC:
		return "HMAC-SHA1";
	case CRYPTO_SHA1:
		return "SHA1";
	case CRYPTO_MD5_HMAC_96:
		return "HMAC-MD5-96";
	case CRYPTO_MD5_HMAC:
		return "HMAC-MD5";
	case CRYPTO_MD5:
		return "MD5";
	case CRYPTO_DES_CBC:
		return "DES-CBC";
	case CRYPTO_3DES_CBC:
		return "3DES-CBC";
	case CRYPTO_AES_CBC:
		return "AES-CBC";
	default:
		break;
	}

	return "Unknown";
}

STATIC const char *
s_xpsec_op(uint32_t reg)
{
	reg &= MV_ACC_CRYPTO_OP_MASK;
	switch (reg) {
	case MV_ACC_CRYPTO_OP_ENC:
		return "ENC";
	case MV_ACC_CRYPTO_OP_MAC:
		return "MAC";
	case MV_ACC_CRYPTO_OP_ENCMAC:
		return "ENC-MAC";
	case MV_ACC_CRYPTO_OP_MACENC:
		return "MAC-ENC";
	default:
		break;
	}
	
	return "Unknown";

}

STATIC const char *
s_xpsec_enc(uint32_t alg)
{
	alg <<= MV_ACC_CRYPTO_ENC_SHIFT;
	switch (alg) {
	case MV_ACC_CRYPTO_ENC_DES:
		return "DES";
	case MV_ACC_CRYPTO_ENC_3DES:
		return "3DES";
	case MV_ACC_CRYPTO_ENC_AES:
		return "AES";
	default:
		break;
	}

	return "Unknown";
}

STATIC const char *
s_xpsec_mac(uint32_t alg)
{
	alg <<= MV_ACC_CRYPTO_MAC_SHIFT;
	switch (alg) {
	case MV_ACC_CRYPTO_MAC_NONE:
		return "Disabled";
	case MV_ACC_CRYPTO_MAC_MD5:
		return "MD5";
	case MV_ACC_CRYPTO_MAC_SHA1:
		return "SHA1";
	case MV_ACC_CRYPTO_MAC_HMAC_MD5:
		return "HMAC-MD5";
	case MV_ACC_CRYPTO_MAC_HMAC_SHA1:
		return "HMAC-SHA1";
	default:
		break;
	}

	return "Unknown";
}

STATIC const char *
s_xpsec_frag(uint32_t frag)
{
	frag <<= MV_ACC_CRYPTO_FRAG_SHIFT;
	switch (frag) {
	case MV_ACC_CRYPTO_NOFRAG:
		return "NoFragment";
	case MV_ACC_CRYPTO_FRAG_FIRST:
		return "FirstFragment";
	case MV_ACC_CRYPTO_FRAG_MID:
		return "MiddleFragment";
	case MV_ACC_CRYPTO_FRAG_LAST:
		return "LastFragment";
	default:
		break;
	}

	return "Unknown";
}

#ifdef MVXPSEC_DEBUG
void
mvxpsec_dump_reg(struct mvxpsec_softc *sc)
{
	uint32_t reg;
	int i;

	if ((mvxpsec_debug & MVXPSEC_DEBUG_DESC) == 0)
		return;

	printf("--- Interrupt Registers ---\n");
	reg = MVXPSEC_READ(sc, MVXPSEC_INT_CAUSE);
	printf("MVXPSEC INT CAUSE: 0x%08x\n", reg);
	printf("MVXPSEC INT CAUSE: %s\n", s_xpsecintr(reg));
	reg = MVXPSEC_READ(sc, MVXPSEC_INT_MASK);
	printf("MVXPSEC INT MASK: 0x%08x\n", reg);
	printf("MVXPSEC INT MASKE: %s\n", s_xpsecintr(reg));

	printf("--- DMA Configuration Registers ---\n");
	for (i = 0; i < MV_TDMA_NWINDOW; i++) {
		reg = MVXPSEC_READ(sc, MV_TDMA_BAR(i));
		printf("TDMA BAR%d: 0x%08x\n", i, reg);
		reg = MVXPSEC_READ(sc, MV_TDMA_ATTR(i));
		printf("TDMA ATTR%d: 0x%08x\n", i, reg);
		printf("  -> %s\n", s_winreg(reg));
	}

	printf("--- DMA Control Registers ---\n");

	reg = MVXPSEC_READ(sc, MV_TDMA_CONTROL);
	printf("TDMA CONTROL: 0x%08x\n", reg);
	printf("  -> %s\n", s_ctrlreg(reg));

	printf("--- DMA Current Command Descriptors ---\n");

	reg = MVXPSEC_READ(sc, MV_TDMA_ERR_CAUSE);
	printf("TDMA ERR CAUSE: 0x%08x\n", reg);

	reg = MVXPSEC_READ(sc, MV_TDMA_ERR_MASK);
	printf("TDMA ERR MASK: 0x%08x\n", reg);

	reg = MVXPSEC_READ(sc, MV_TDMA_CNT);
	printf("TDMA DATA OWNER: %s\n",
	    (reg & MV_TDMA_CNT_OWN) ? "DMAC" : "CPU");
	printf("TDMA DATA COUNT: %d(0x%x)\n",
	    (reg & ~MV_TDMA_CNT_OWN), (reg & ~MV_TDMA_CNT_OWN));

	reg = MVXPSEC_READ(sc, MV_TDMA_SRC);
	printf("TDMA DATA SRC: 0x%08x\n", reg);

	reg = MVXPSEC_READ(sc, MV_TDMA_DST);
	printf("TDMA DATA DST: 0x%08x\n", reg);

	reg = MVXPSEC_READ(sc, MV_TDMA_NXT);
	printf("TDMA DATA NXT: 0x%08x\n", reg);

	reg = MVXPSEC_READ(sc, MV_TDMA_CUR);
	printf("TDMA DATA CUR: 0x%08x\n", reg);

	printf("--- ACC Command Register ---\n");
	reg = MVXPSEC_READ(sc, MV_ACC_COMMAND);
	printf("ACC COMMAND: 0x%08x\n", reg);
	printf("ACC: %sACT %sSTOP\n",
	    (reg & MV_ACC_COMMAND_ACT) ? "+" : "-",
	    (reg & MV_ACC_COMMAND_STOP) ? "+" : "-");

	reg = MVXPSEC_READ(sc, MV_ACC_CONFIG);
	printf("ACC CONFIG: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_ACC_DESC);
	printf("ACC DESC: 0x%08x\n", reg);

	printf("--- DES Key Register ---\n");
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY0L);
	printf("DES KEY0  Low: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY0H);
	printf("DES KEY0 High: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY1L);
	printf("DES KEY1  Low: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY1H);
	printf("DES KEY1 High: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY2L);
	printf("DES KEY2  Low: 0x%08x\n", reg);
	reg = MVXPSEC_READ(sc, MV_CE_DES_KEY2H);
	printf("DES KEY2 High: 0x%08x\n", reg);

	printf("--- AES Key Register ---\n");
	for (i = 0; i < 8; i++) {
		reg = MVXPSEC_READ(sc, MV_CE_AES_EKEY(i));
		printf("AES ENC KEY COL%d: %08x\n", i, reg);
	}
	for (i = 0; i < 8; i++) {
		reg = MVXPSEC_READ(sc, MV_CE_AES_DKEY(i));
		printf("AES DEC KEY COL%d: %08x\n", i, reg);
	}

	return;
}

STATIC void
mvxpsec_dump_sram(const char *name, struct mvxpsec_softc *sc, size_t len)
{
	uint32_t reg;

	if (sc->sc_sram_va == NULL)
		return;

	if (len == 0) {
		printf("\n%s NO DATA(len=0)\n", name);
		return;
	}
	else if (len > MV_ACC_SRAM_SIZE)
		len = MV_ACC_SRAM_SIZE;

	mutex_enter(&sc->sc_dma_mtx);
	reg = MVXPSEC_READ(sc, MV_TDMA_CONTROL);
	if (reg & MV_TDMA_CONTROL_ACT) {
		printf("TDMA is active, cannot access SRAM\n");
		mutex_exit(&sc->sc_dma_mtx);
		return;
	}
	reg = MVXPSEC_READ(sc, MV_ACC_COMMAND);
	if (reg & MV_ACC_COMMAND_ACT) {
		printf("SA is active, cannot access SRAM\n");
		mutex_exit(&sc->sc_dma_mtx);
		return;
	}

	printf("%s: dump SRAM, %zu bytes\n", name, len);
	mvxpsec_dump_data(name, sc->sc_sram_va, len);
	mutex_exit(&sc->sc_dma_mtx);
	return;
}


_STATIC void
mvxpsec_dump_dmaq(struct mvxpsec_descriptor_handle *dh)
{
	struct mvxpsec_descriptor *d =
           (struct mvxpsec_descriptor *)dh->_desc;

	printf("--- DMA Command Descriptor ---\n");
	printf("DESC: VA=%p PA=0x%08x\n",
	    d, (uint32_t)dh->phys_addr);
	printf("DESC: WORD0 = 0x%08x\n", d->tdma_word0);
	printf("DESC: SRC = 0x%08x\n", d->tdma_src);
	printf("DESC: DST = 0x%08x\n", d->tdma_dst);
	printf("DESC: NXT = 0x%08x\n", d->tdma_nxt);

	return;
}

STATIC void
mvxpsec_dump_data(const char *name, void *p, size_t len)
{
	uint8_t *data = p;
	off_t off;

	printf("%s: dump %p, %zu bytes", name, p, len);
	if (p == NULL || len == 0) {
		printf("\n%s: NO DATA\n", name);
		return;
	}
	for (off = 0; off < len; off++) {
		if ((off % 16) == 0) {
			printf("\n%s: 0x%08x:", name, (uint32_t)off);
		}
		if ((off % 4) == 0) {
			printf(" ");
		}
		printf("%02x", data[off]);
	}
	printf("\n");

	return;
}

_STATIC void
mvxpsec_dump_packet(const char *name, struct mvxpsec_packet *mv_p)
{
	struct mvxpsec_softc *sc = mv_p->mv_s->sc;

	printf("%s: packet_data:\n", name);
	mvxpsec_dump_packet_data(name, mv_p);

	printf("%s: SRAM:\n", name);
	mvxpsec_dump_sram(name, sc, 2000);

	printf("%s: packet_descriptor:\n", name);
	mvxpsec_dump_packet_desc(name, mv_p);
}

_STATIC void
mvxpsec_dump_packet_data(const char *name, struct mvxpsec_packet *mv_p)
{
	static char buf[1500];
	int len;

	if (mv_p->data_type == MVXPSEC_DATA_MBUF) {
		struct mbuf *m;

		m = mv_p->data.mbuf;
		len = m->m_pkthdr.len;
		if (len > sizeof(buf))
			len = sizeof(buf);
		m_copydata(m, 0, len, buf);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_UIO) {
		struct uio *uio;

		uio = mv_p->data.uio;
		len = uio->uio_resid;
		if (len > sizeof(buf))
			len = sizeof(buf);
		cuio_copydata(uio, 0, len, buf);
	}
	else if (mv_p->data_type == MVXPSEC_DATA_RAW) {
		len = mv_p->data_len;
		if (len > sizeof(buf))
			len = sizeof(buf);
		memcpy(buf, mv_p->data.raw, len);
	}
	else
		return;
	mvxpsec_dump_data(name, buf, len);

	return;
}

_STATIC void
mvxpsec_dump_packet_desc(const char *name, struct mvxpsec_packet *mv_p)
{
	uint32_t *words;

	if (mv_p == NULL)
		return;

	words = &mv_p->pkt_header.desc.acc_desc_dword0;
	mvxpsec_dump_acc_config(name, words[0]);
	mvxpsec_dump_acc_encdata(name, words[1], words[2]);
	mvxpsec_dump_acc_enclen(name, words[2]);
	mvxpsec_dump_acc_enckey(name, words[3]);
	mvxpsec_dump_acc_enciv(name, words[4]);
	mvxpsec_dump_acc_macsrc(name, words[5]);
	mvxpsec_dump_acc_macdst(name, words[6]);
	mvxpsec_dump_acc_maciv(name, words[7]);

	return;
}

_STATIC void
mvxpsec_dump_acc_config(const char *name, uint32_t w)
{
	/* SA: Dword 0 */
	printf("%s: Dword0=0x%08x\n", name, w);
	printf("%s:   OP = %s\n", name,
	    s_xpsec_op(MV_ACC_CRYPTO_OP(w)));
	printf("%s:   MAC = %s\n", name,
	    s_xpsec_mac(MV_ACC_CRYPTO_MAC(w)));
	printf("%s:   MAC_LEN = %s\n", name,
	    w & MV_ACC_CRYPTO_MAC_96 ? "96-bit" : "full-bit");
	printf("%s:   ENC = %s\n", name,
	    s_xpsec_enc(MV_ACC_CRYPTO_ENC(w)));
	printf("%s:   DIR = %s\n", name,
	    w & MV_ACC_CRYPTO_DECRYPT ? "decryption" : "encryption");
	printf("%s:   CHAIN = %s\n", name,
	    w & MV_ACC_CRYPTO_CBC ? "CBC" : "ECB");
	printf("%s:   3DES = %s\n", name,
	    w & MV_ACC_CRYPTO_3DES_EDE ? "EDE" : "EEE");
	printf("%s:   FRAGMENT = %s\n", name,
	    s_xpsec_frag(MV_ACC_CRYPTO_FRAG(w)));
	return;
}

STATIC void
mvxpsec_dump_acc_encdata(const char *name, uint32_t w, uint32_t w2)
{
	/* SA: Dword 1 */
	printf("%s: Dword1=0x%08x\n", name, w);
	printf("%s:   ENC SRC = 0x%x\n", name, MV_ACC_DESC_GET_VAL_1(w));
	printf("%s:   ENC DST = 0x%x\n", name, MV_ACC_DESC_GET_VAL_2(w));
	printf("%s:   ENC RANGE = 0x%x - 0x%x\n", name,
	    MV_ACC_DESC_GET_VAL_1(w),
	    MV_ACC_DESC_GET_VAL_1(w) + MV_ACC_DESC_GET_VAL_1(w2) - 1);
	return;
}

STATIC void
mvxpsec_dump_acc_enclen(const char *name, uint32_t w)
{
	/* SA: Dword 2 */
	printf("%s: Dword2=0x%08x\n", name, w);
	printf("%s:   ENC LEN = %d\n", name,
	    MV_ACC_DESC_GET_VAL_1(w));
	return;
}

STATIC void
mvxpsec_dump_acc_enckey(const char *name, uint32_t w)
{
	/* SA: Dword 3 */
	printf("%s: Dword3=0x%08x\n", name, w);
	printf("%s:   EKEY = 0x%x\n", name,
	    MV_ACC_DESC_GET_VAL_1(w));
	return;
}

STATIC void
mvxpsec_dump_acc_enciv(const char *name, uint32_t w)
{
	/* SA: Dword 4 */
	printf("%s: Dword4=0x%08x\n", name, w);
	printf("%s:   EIV = 0x%x\n", name, MV_ACC_DESC_GET_VAL_1(w));
	printf("%s:   EIV_BUF = 0x%x\n", name, MV_ACC_DESC_GET_VAL_2(w));
	return;
}

STATIC void
mvxpsec_dump_acc_macsrc(const char *name, uint32_t w)
{
	/* SA: Dword 5 */
	printf("%s: Dword5=0x%08x\n", name, w);
	printf("%s:   MAC_SRC = 0x%x\n", name,
	    MV_ACC_DESC_GET_VAL_1(w));
	printf("%s:   MAC_TOTAL_LEN = %d\n", name,
	    MV_ACC_DESC_GET_VAL_3(w));
	printf("%s:   MAC_RANGE = 0x%0x - 0x%0x\n", name,
	    MV_ACC_DESC_GET_VAL_1(w),
	    MV_ACC_DESC_GET_VAL_1(w) + MV_ACC_DESC_GET_VAL_3(w) - 1);
	return;
}

STATIC void
mvxpsec_dump_acc_macdst(const char *name, uint32_t w)
{
	/* SA: Dword 6 */
	printf("%s: Dword6=0x%08x\n", name, w);
	printf("%s:   MAC_DST = 0x%x\n", name, MV_ACC_DESC_GET_VAL_1(w));
	printf("%s:   MAC_BLOCK_LEN = %d\n", name,
	    MV_ACC_DESC_GET_VAL_2(w));
	return;
}

STATIC void
mvxpsec_dump_acc_maciv(const char *name, uint32_t w)
{
	/* SA: Dword 7 */
	printf("%s: Dword7=0x%08x\n", name, w);
	printf("%s:   MAC_INNER_IV = 0x%x\n", name,
	    MV_ACC_DESC_GET_VAL_1(w));
	printf("%s:   MAC_OUTER_IV = 0x%x\n", name,
	    MV_ACC_DESC_GET_VAL_2(w));
	return;
}
#endif


/*	$NetBSD: if_ray.c,v 1.81 2012/12/07 05:45:17 msaitoh Exp $	*/

/*
 * Copyright (c) 2000 Christian E. Hopps
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver for the Raylink (Raytheon) / WebGear IEEE 802.11 (FH) WLANs
 *
 *	2-way communication with the card is through command structures
 *	stored in shared ram.  To communicate with the card a free
 *	command structure is filled in and then the card is interrupted.
 *	The card does the same with a different set of command structures.
 *	Only one command can be processed at a time.  This is indicated
 *	by the interrupt having not been cleared since it was last set.
 *	The bit is cleared when the command has been processed (although
 *	it may not yet be complete).
 *
 *	This driver was only tested with the Aviator 2.4 wireless
 *	The author didn't have the pro version or raylink to test
 *	with.
 *
 *	N.B. Its unclear yet whether the Aviator 2.4 cards interoperate
 *	with other 802.11 FH 2Mbps cards, since this was also untested.
 *	Given the nature of the buggy build 4 firmware there may be problems.
 *
 *	Authentication added by Steve Weiss <srw@alum.mit.edu> based on
 *	advice from Corey Thomas (author of the Linux RayLink driver).
 *	Authentication is currently limited to adhoc networks, and was
 *	added to support a requirement of the newest Windows drivers for
 *	the RayLink.  Tested with Aviator Pro (firmware 5.63) on Win98.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ray.c,v 1.81 2012/12/07 05:45:17 msaitoh Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_llc.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/pcmcia/if_rayreg.h>

#define	RAY_DEBUG

#ifndef	RAY_PID_COUNTRY_CODE_DEFAULT
#define	RAY_PID_COUNTRY_CODE_DEFAULT	RAY_PID_COUNTRY_CODE_USA
#endif

/* amount of time to poll for non-return of certain command status */
#ifndef	RAY_CHECK_CCS_TIMEOUT
#define	RAY_CHECK_CCS_TIMEOUT	(hz / 2)
#endif

/* ammount of time to consider start/join failed */
#ifndef	RAY_START_TIMEOUT
#define	RAY_START_TIMEOUT	(30 * hz)
#endif

/*
 * if a command cannot execute because device is busy try later
 * this is also done after interrupts and other command timeouts
 * so we can use a large value safely.
 */
#ifndef	RAY_CHECK_SCHED_TIMEOUT
#define	RAY_CHECK_SCHED_TIMEOUT	(hz)	/* XXX 5 */
#endif

#ifndef	RAY_MODE_DEFAULT
#define	RAY_MODE_DEFAULT	SC_MODE_ADHOC
#endif

#ifndef	RAY_DEF_NWID
#define	RAY_DEF_NWID	"NETWORK_NAME"
#endif

/*
 * The number of times the HW is reset in 30s before disabling.
 * This is needed because resets take ~2s and currently pcmcia
 * spins for the reset.
 */
#ifndef	RAY_MAX_RESETS
#define	RAY_MAX_RESETS	3
#endif

/*
 * Types
 */

struct ray_softc {
	device_t	sc_dev;
	struct ethercom	sc_ec;
	struct ifmedia	sc_media;

	struct pcmcia_function		*sc_pf;
	void				*sc_ih;
	int				sc_attached;

	int				sc_resetloop;

	struct callout			sc_check_ccs_ch;
	struct callout			sc_check_scheduled_ch;
	struct callout			sc_reset_resetloop_ch;
	struct callout			sc_disable_ch;
	struct callout			sc_start_join_timo_ch;

	struct ray_ecf_startup		sc_ecf_startup;
	struct ray_startup_params_head	sc_startup;
	union {
		struct ray_startup_params_tail_5	u_params_5;
		struct ray_startup_params_tail_4	u_params_4;
	} sc_u;

	u_int8_t	sc_ccsinuse[64];	/* ccs in use -- not for tx */
	u_int		sc_txfree;	/* a free count for efficiency */

	u_int8_t	sc_bssid[ETHER_ADDR_LEN];	/* current net values */
	u_int8_t	sc_authid[ETHER_ADDR_LEN];	/* ID of authenticating
							   station */
	struct ieee80211_nwid	sc_cnwid;	/* last nwid */
	struct ieee80211_nwid	sc_dnwid;	/* desired nwid */
	u_int8_t	sc_omode;	/* old operating mode SC_MODE_xx */
	u_int8_t	sc_mode;	/* current operating mode SC_MODE_xx */
	u_int8_t	sc_countrycode;	/* current country code */
	u_int8_t	sc_dcountrycode; /* desired country code */
	int		sc_havenet;	/* true if we have acquired a network */
	bus_size_t	sc_txpad;	/* tib size plus "phy" size */
	u_int8_t	sc_deftxrate;	/* default transfer rate */
	u_int8_t	sc_encrypt;
	u_int8_t	sc_authstate;	/* authentication state */

	int		sc_promisc;	/* current set value */
	int		sc_running;	/* things we are doing */
	int		sc_scheduled;	/* things we need to do */
	int		sc_timoneed;	/* set if timeout is sched */
	int		sc_timocheck;	/* set if timeout is sched */
	bus_size_t	sc_startccs;	/* ccs of start/join */
	u_int		sc_startcmd;	/* cmd (start | join) */

	int		sc_checkcounters;
	u_int64_t	sc_rxoverflow;
	u_int64_t	sc_rxcksum;
	u_int64_t	sc_rxhcksum;
	u_int8_t	sc_rxnoise;

	/* use to return values to the user */
	struct ray_param_req	*sc_repreq;
	struct ray_param_req	*sc_updreq;

	bus_space_tag_t	sc_memt;
	bus_space_handle_t sc_memh;

#ifdef RAY_DO_SIGLEV
	struct ray_siglev	sc_siglevs[RAY_NSIGLEVRECS];
#endif
};
#define	sc_ccrt	sc_pf->pf_ccrt
#define	sc_ccrh	sc_pf->pf_ccrh
#define	sc_ccroff sc_pf->pf_ccr_offset
#define	sc_startup_4	sc_u.u_params_4
#define	sc_startup_5	sc_u.u_params_5
#define	sc_version	sc_ecf_startup.e_fw_build_string
#define	sc_tibsize	sc_ecf_startup.e_tib_size
#define	sc_if		sc_ec.ec_if

/* modes of operation */
#define	SC_MODE_ADHOC	0	/* ad-hoc mode */
#define	SC_MODE_INFRA	1	/* infrastructure mode */

/* commands -- priority given to LSB */
#define	SCP_FIRST		0x0001
#define	SCP_UPDATESUBCMD	0x0001
#define	SCP_STARTASSOC		0x0002
#define	SCP_REPORTPARAMS	0x0004
#define	SCP_IFSTART		0x0008

/* update sub commands -- issues are serialized priority to LSB */
#define	SCP_UPD_FIRST		0x0100
#define	SCP_UPD_STARTUP		0x0100
#define	SCP_UPD_STARTJOIN	0x0200
#define	SCP_UPD_PROMISC		0x0400
#define	SCP_UPD_MCAST		0x0800
#define	SCP_UPD_UPDATEPARAMS	0x1000
#define	SCP_UPD_SHIFT		8
#define	SCP_UPD_MASK		0xff00

/* these command (a subset of the update set) require timeout checking */
#define	SCP_TIMOCHECK_CMD_MASK	\
	(SCP_UPD_UPDATEPARAMS | SCP_UPD_STARTUP | SCP_UPD_MCAST | \
	SCP_UPD_PROMISC)


#define	IFM_ADHOC	\
	IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_FH2, IFM_IEEE80211_ADHOC, 0)
#define	IFM_INFRA	\
	IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_FH2, 0, 0)

typedef	void (*ray_cmd_func_t)(struct ray_softc *);

#define	SC_BUILD_5	0x5
#define	SC_BUILD_4	0x55

/* sc_authstate */
#define	RAY_AUTH_UNAUTH		0
#define	RAY_AUTH_WAITING	1
#define	RAY_AUTH_AUTH		2
#define	RAY_AUTH_NEEDED		3

#define	OPEN_AUTH_REQUEST	1
#define	OPEN_AUTH_RESPONSE	2
#define	BROADCAST_DEAUTH	0xc0

static int ray_alloc_ccs(struct ray_softc *, bus_size_t *, u_int, u_int);
static bus_size_t ray_fill_in_tx_ccs(struct ray_softc *, size_t,
    u_int, u_int);
static int ray_validate_config(struct pcmcia_config_entry *);
static void ray_attach(device_t, device_t, void *);
static ray_cmd_func_t ray_ccs_done(struct ray_softc *, bus_size_t);
static void ray_check_ccs(void *);
static void ray_check_scheduled(void *);
static void ray_cmd_cancel(struct ray_softc *, int);
static void ray_cmd_schedule(struct ray_softc *, int);
static void ray_cmd_ran(struct ray_softc *, int);
static int ray_cmd_is_running(struct ray_softc *, int);
static int ray_cmd_is_scheduled(struct ray_softc *, int);
static void ray_cmd_done(struct ray_softc *, int);
static int ray_detach(device_t, int);
static int ray_activate(device_t, enum devact);
static void ray_disable(struct ray_softc *);
static void ray_download_params(struct ray_softc *);
static int ray_enable(struct ray_softc *);
static u_int ray_find_free_tx_ccs(struct ray_softc *, u_int);
static u_int8_t ray_free_ccs(struct ray_softc *, bus_size_t);
static void ray_free_ccs_chain(struct ray_softc *, u_int);
static void ray_if_start(struct ifnet *);
static void ray_if_stop(struct ifnet *, int);
static int ray_init(struct ray_softc *);
static int ray_intr(void *);
static void ray_intr_start(struct ray_softc *);
static int ray_ioctl(struct ifnet *, u_long, void *);
static int ray_issue_cmd(struct ray_softc *, bus_size_t, u_int);
static int ray_match(device_t, cfdata_t, void *);
static int ray_media_change(struct ifnet *);
static void ray_media_status(struct ifnet *, struct ifmediareq *);
static ray_cmd_func_t ray_rccs_intr(struct ray_softc *, bus_size_t);
static void ray_read_region(struct ray_softc *, bus_size_t,void *,size_t);
static void ray_recv(struct ray_softc *, bus_size_t);
static void ray_recv_auth(struct ray_softc *, struct ieee80211_frame *);
static void ray_report_params(struct ray_softc *);
static void ray_reset(struct ray_softc *);
static void ray_reset_resetloop(void *);
static int ray_send_auth(struct ray_softc *, u_int8_t *, u_int8_t);
static void ray_set_pending(struct ray_softc *, u_int);
static int ray_simple_cmd(struct ray_softc *, u_int, u_int);
static void ray_start_assoc(struct ray_softc *);
static void ray_start_join_net(struct ray_softc *);
static ray_cmd_func_t ray_start_join_net_done(struct ray_softc *,
    u_int, bus_size_t, u_int);
static void ray_start_join_timo(void *);
static void ray_stop(struct ray_softc *);
static void ray_update_error_counters(struct ray_softc *);
static void ray_update_mcast(struct ray_softc *);
static ray_cmd_func_t ray_update_params_done(struct ray_softc *,
    bus_size_t, u_int);
static void ray_update_params(struct ray_softc *);
static void ray_update_promisc(struct ray_softc *);
static void ray_update_subcmd(struct ray_softc *);
static int ray_user_report_params(struct ray_softc *,
    struct ray_param_req *);
static int ray_user_update_params(struct ray_softc *,
    struct ray_param_req *);
static void ray_write_region(struct ray_softc *,bus_size_t,void *,size_t);

#ifdef RAY_DO_SIGLEV
static void ray_update_siglev(struct ray_softc *, u_int8_t *, u_int8_t);
#endif

#ifdef RAY_DEBUG
static int ray_debug = 0;
static int ray_debug_xmit_sum = 0;
static int ray_debug_dump_desc = 0;
static int ray_debug_dump_rx = 0;
static int ray_debug_dump_tx = 0;
static struct timeval rtv, tv1, tv2, *ttp, *ltp;
#define	RAY_DPRINTF(x)	do { if (ray_debug) {	\
	struct timeval *ttmp;			\
	microtime(ttp);				\
	timersub(ttp, ltp, &rtv);		\
	ttmp = ttp; ttp = ltp; ltp = ttmp;	\
	printf("%ld:%ld %ld:%06ld: ",		\
	    (long int)ttp->tv_sec,		\
	    (long int)ttp->tv_usec,		\
	    (long int)rtv.tv_sec,		\
	    (long int)rtv.tv_usec);		\
	printf x ;				\
	} } while (0)
#define	RAY_DPRINTF_XMIT(x)	do { if (ray_debug_xmit_sum) {	\
	struct timeval *ttmp;			\
	microtime(ttp);				\
	timersub(ttp, ltp, &rtv);		\
	ttmp = ttp; ttp = ltp; ltp = ttmp;	\
	printf("%ld:%ld %ld:%06ld: ",		\
	    (long int)ttp->tv_sec,		\
	    (long int)ttp->tv_usec,		\
	    (long int)rtv.tv_sec,		\
	    (long int)rtv.tv_usec);		\
	printf x ;				\
	} } while (0)

#define	HEXDF_NOCOMPRESS	0x1
#define	HEXDF_NOOFFSET		0x2
#define HEXDF_NOASCII		0x4
void hexdump(const u_int8_t *, int, int, int, int);
static void ray_dump_mbuf(struct ray_softc *, struct mbuf *);

#else	/* !RAY_DEBUG */

#define	RAY_DPRINTF(x)
#define	RAY_DPRINTF_XMIT(x)

#endif	/* !RAY_DEBUG */

/*
 * macros for writing to various regions in the mapped memory space
 */

	/* use already mapped ccrt */
#define	REG_WRITE(sc, off, val) \
	bus_space_write_1((sc)->sc_ccrt, (sc)->sc_ccrh, ((sc)->sc_ccroff + (off)), (val))

#define	REG_READ(sc, off) \
	bus_space_read_1((sc)->sc_ccrt, (sc)->sc_ccrh, ((sc)->sc_ccroff + (off)))

#define	SRAM_READ_1(sc, off) \
	((u_int8_t)bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (off)))

#define	SRAM_READ_FIELD_1(sc, off, s, f) \
	SRAM_READ_1(sc, (off) + offsetof(struct s, f))

#define	SRAM_READ_FIELD_2(sc, off, s, f)			\
	((((u_int16_t)SRAM_READ_1(sc, (off) + offsetof(struct s, f)) << 8) \
	|(SRAM_READ_1(sc, (off) + 1 + offsetof(struct s, f)))))

#define	SRAM_READ_FIELD_N(sc, off, s, f, p, n)	\
	ray_read_region(sc, (off) + offsetof(struct s, f), (p), (n))

#define	SRAM_WRITE_1(sc, off, val)	\
	bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (off), (val))

#define	SRAM_WRITE_FIELD_1(sc, off, s, f, v) 	\
	SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (v))

#define	SRAM_WRITE_FIELD_2(sc, off, s, f, v) do {	\
	SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (((v) >> 8 ) & 0xff)); \
	SRAM_WRITE_1(sc, (off) + 1 + offsetof(struct s, f), ((v) & 0xff)); \
    } while (0)

#define	SRAM_WRITE_FIELD_N(sc, off, s, f, p, n)	\
	ray_write_region(sc, (off) + offsetof(struct s, f), (p), (n))

/*
 * Macros of general usefulness
 */

#define	M_PULLUP(m, s) do {	\
	if ((m)->m_len < (s))	\
		(m) = m_pullup((m), (s)); \
    } while (0)

#define	RAY_ECF_READY(sc)	(!(REG_READ(sc, RAY_ECFIR) & RAY_ECSIR_IRQ))
#define	RAY_ECF_START_CMD(sc)	REG_WRITE(sc, RAY_ECFIR, RAY_ECSIR_IRQ)
#define	RAY_GET_INDEX(ccs)	(((ccs) - RAY_CCS_BASE) / RAY_CCS_SIZE)
#define	RAY_GET_CCS(i)		(RAY_CCS_BASE + (i) * RAY_CCS_SIZE)

/*
 * Globals
 */

static u_int8_t llc_snapid[6] = { LLC_SNAP_LSAP, LLC_SNAP_LSAP, LLC_UI, };

/* based on bit index in SCP_xx */
static ray_cmd_func_t ray_cmdtab[] = {
	ray_update_subcmd,	/* SCP_UPDATESUBCMD */
	ray_start_assoc,	/* SCP_STARTASSOC */
	ray_report_params,	/* SCP_REPORTPARAMS */
	ray_intr_start		/* SCP_IFSTART */
};
static int ray_ncmdtab = sizeof(ray_cmdtab) / sizeof(*ray_cmdtab);

static ray_cmd_func_t ray_subcmdtab[] = {
	ray_download_params,	/* SCP_UPD_STARTUP */
	ray_start_join_net,	/* SCP_UPD_STARTJOIN */
	ray_update_promisc,	/* SCP_UPD_PROMISC */
	ray_update_mcast,	/* SCP_UPD_MCAST */
	ray_update_params	/* SCP_UPD_UPDATEPARAMS */
};
static int ray_nsubcmdtab = sizeof(ray_subcmdtab) / sizeof(*ray_subcmdtab);

/* autoconf information */
CFATTACH_DECL_NEW(ray, sizeof(struct ray_softc),
    ray_match, ray_attach, ray_detach, ray_activate);

/*
 * Config Routines
 */

static int
ray_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct pcmcia_attach_args *pa = aux;

#ifdef RAY_DEBUG
	if (!ltp) {
		/* initialize timestamp XXX */
		ttp = &tv1;
		ltp = &tv2;
		microtime(ltp);
	}
#endif
	return (pa->manufacturer == PCMCIA_VENDOR_RAYTHEON
	    && pa->product == PCMCIA_PRODUCT_RAYTHEON_WLAN);
}

static int
ray_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 1 ||
	    cfe->num_iospace != 0 ||
	    cfe->memspace[0].length != RAY_SRAM_MEM_SIZE)
		return (EINVAL);
	return (0);
}

static void
ray_attach(device_t parent, device_t self, void *aux)
{
	struct ray_softc *sc = device_private(self);
	struct pcmcia_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_if;
	struct pcmcia_config_entry *cfe;
	struct ray_ecf_startup *ep;
	int error;

	sc->sc_dev = self;
	sc->sc_pf = pa->pf;

	/*XXXmem8|common*/
	error = pcmcia_function_configure(pa->pf, ray_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_memt = cfe->memspace[0].handle.memt;
	sc->sc_memh = cfe->memspace[0].handle.memh;

	callout_init(&sc->sc_reset_resetloop_ch, 0);
	callout_init(&sc->sc_disable_ch, 0);
	callout_init(&sc->sc_start_join_timo_ch, 0);

	error = ray_enable(sc);
	if (error)
		goto fail;

	/* get startup results */
	ep = &sc->sc_ecf_startup;
	ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep,
	    sizeof(sc->sc_ecf_startup));

	/* check to see that card initialized properly */
	if (ep->e_status != RAY_ECFS_CARD_OK) {
		aprint_error_dev(self, "card failed self test: status %d\n",
		    sc->sc_ecf_startup.e_status);
		goto fail2;
	}

	/* check firmware version */
	if (sc->sc_version != SC_BUILD_4 && sc->sc_version != SC_BUILD_5) {
		aprint_error_dev(self, "unsupported firmware version %d\n",
		    ep->e_fw_build_string);
		goto fail2;
	}

	/* clear any interrupt if present */
	REG_WRITE(sc, RAY_HCSIR, 0);

	/*
	 * set the parameters that will survive stop/init
	 */
	memset(&sc->sc_dnwid, 0, sizeof(sc->sc_dnwid));
	sc->sc_dnwid.i_len = strlen(RAY_DEF_NWID);
	if (sc->sc_dnwid.i_len > IEEE80211_NWID_LEN)
		sc->sc_dnwid.i_len = IEEE80211_NWID_LEN;
	if (sc->sc_dnwid.i_len > 0)
		memcpy(sc->sc_dnwid.i_nwid, RAY_DEF_NWID, sc->sc_dnwid.i_len);
	memcpy(&sc->sc_cnwid, &sc->sc_dnwid, sizeof(sc->sc_cnwid));
	sc->sc_omode = sc->sc_mode = RAY_MODE_DEFAULT;
	sc->sc_countrycode = sc->sc_dcountrycode =
	    RAY_PID_COUNTRY_CODE_DEFAULT;

	/*
	 * attach the interface
	 */
	/* The version isn't the most accurate way, but it's easy. */
	aprint_normal_dev(self, "firmware version %d\n",
	    sc->sc_version);
	if (sc->sc_version != SC_BUILD_4)
		aprint_normal_dev(self, "supported rates %0x:%0x:%0x:%0x:%0x:%0x:%0x:%0x\n",
		    ep->e_rates[0], ep->e_rates[1],
		    ep->e_rates[2], ep->e_rates[3], ep->e_rates[4],
		    ep->e_rates[5], ep->e_rates[6], ep->e_rates[7]);
	aprint_normal_dev(self, "802.11 address %s\n",
	    ether_sprintf(ep->e_station_addr));

	memcpy(ifp->if_xname, device_xname(self), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = ray_if_start;
	ifp->if_stop = ray_if_stop;
	ifp->if_ioctl = ray_ioctl;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	ether_ifattach(ifp, ep->e_station_addr);
	/* need enough space for ieee80211_header + (snap or e2) */
	ifp->if_hdrlen =
	    sizeof(struct ieee80211_frame) + sizeof(struct ether_header);

	ifmedia_init(&sc->sc_media, 0, ray_media_change, ray_media_status);
	ifmedia_add(&sc->sc_media, IFM_ADHOC, 0, 0);
	ifmedia_add(&sc->sc_media, IFM_INFRA, 0, 0);
	if (sc->sc_mode == SC_MODE_ADHOC)
		ifmedia_set(&sc->sc_media, IFM_ADHOC);
	else
		ifmedia_set(&sc->sc_media, IFM_INFRA);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* The attach is successful. */
	sc->sc_attached = 1;
	ray_disable(sc);
	return;

fail2:
	ray_disable(sc);
fail:
	pcmcia_function_unconfigure(pa->pf);
}

static int
ray_activate(device_t self, enum devact act)
{
	struct ray_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->sc_if;

	RAY_DPRINTF(("%s: activate\n", device_xname(self)));

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(ifp);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static int
ray_detach(device_t self, int flags)
{
	struct ray_softc *sc;
	struct ifnet *ifp;

	sc = device_private(self);
	ifp = &sc->sc_if;
	RAY_DPRINTF(("%s: detach\n", device_xname(sc->sc_dev)));

	if (!sc->sc_attached)
                return (0);

	pmf_device_deregister(self);

	if (sc->sc_if.if_flags & IFF_UP)
		ray_disable(sc);

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

/*
 * start the card running
 */
static int
ray_enable(struct ray_softc *sc)
{
	int error;

	RAY_DPRINTF(("%s: enable\n", device_xname(sc->sc_dev)));

	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET,
	    ray_intr, sc);
	if (!sc->sc_ih)
		return (EIO);

	error = ray_init(sc);
	if (error) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (error);
}

/*
 * stop the card running
 */
static void
ray_disable(struct ray_softc *sc)
{
	RAY_DPRINTF(("%s: disable\n", device_xname(sc->sc_dev)));

	ray_stop(sc);

	sc->sc_resetloop = 0;
	sc->sc_rxoverflow = 0;
	sc->sc_rxcksum = 0;
	sc->sc_rxhcksum = 0;
	sc->sc_rxnoise = 0;

	if (sc->sc_ih) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}
}

/*
 * start the card running
 */
static int
ray_init(struct ray_softc *sc)
{
	struct ray_ecf_startup *ep;
	bus_size_t ccs;
	int i;

	RAY_DPRINTF(("%s: init\n", device_xname(sc->sc_dev)));

	if ((sc->sc_if.if_flags & IFF_RUNNING))
		ray_stop(sc);

	if (pcmcia_function_enable(sc->sc_pf))
		return (EIO);

	RAY_DPRINTF(("%s: init post-enable\n", device_xname(sc->sc_dev)));

	/* reset some values */
	memset(sc->sc_ccsinuse, 0, sizeof(sc->sc_ccsinuse));
	sc->sc_havenet = 0;
	memset(sc->sc_bssid, 0, sizeof(sc->sc_bssid));
	sc->sc_deftxrate = 0;
	sc->sc_encrypt = 0;
	sc->sc_txpad = 0;
	sc->sc_promisc = 0;
	sc->sc_scheduled = 0;
	sc->sc_running = 0;
	sc->sc_txfree = RAY_CCS_NTX;
	sc->sc_checkcounters = 0;
	sc->sc_authstate = RAY_AUTH_UNAUTH;

	/* get startup results */
	ep = &sc->sc_ecf_startup;
	ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep,
	    sizeof(sc->sc_ecf_startup));

	/* check to see that card initialized properly */
	if (ep->e_status != RAY_ECFS_CARD_OK) {
		pcmcia_function_disable(sc->sc_pf);
		printf("%s: card failed self test: status %d\n",
		    device_xname(sc->sc_dev), sc->sc_ecf_startup.e_status);
		return (EIO);
	}

	/* fixup tib size to be correct */
	if (sc->sc_version == SC_BUILD_4 && sc->sc_tibsize == 0x55)
		sc->sc_tibsize = 32;
	sc->sc_txpad = sc->sc_tibsize;

	/* set all ccs to be free */
	ccs = RAY_GET_CCS(0);
	for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status,
		    RAY_CCS_STATUS_FREE);

	/* clear the interrupt if present */
	REG_WRITE(sc, RAY_HCSIR, 0);

	callout_init(&sc->sc_check_ccs_ch, 0);
	callout_init(&sc->sc_check_scheduled_ch, 0);

	/* we are now up and running -- and are busy until download is cplt */
	sc->sc_if.if_flags |= IFF_RUNNING | IFF_OACTIVE;

	/* set this now so it gets set in the download */
	if (sc->sc_if.if_flags & IFF_ALLMULTI)
		sc->sc_if.if_flags |= IFF_PROMISC;
	else if (sc->sc_if.if_pcount == 0)
		sc->sc_if.if_flags &= ~IFF_PROMISC;
	sc->sc_promisc = !!(sc->sc_if.if_flags & IFF_PROMISC);

	/* call after we mark ourselves running */
	ray_download_params(sc);

	return (0);
}

/*
 * stop the card running
 */
static void
ray_stop(struct ray_softc *sc)
{
	RAY_DPRINTF(("%s: stop\n", device_xname(sc->sc_dev)));

	callout_stop(&sc->sc_check_ccs_ch);
	sc->sc_timocheck = 0;

	callout_stop(&sc->sc_check_scheduled_ch);
	sc->sc_timoneed = 0;

	if (sc->sc_repreq) {
		sc->sc_repreq->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		wakeup(ray_report_params);
	}
	if (sc->sc_updreq) {
		sc->sc_updreq->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		wakeup(ray_update_params);
	}

	sc->sc_if.if_flags &= ~IFF_RUNNING;
	pcmcia_function_disable(sc->sc_pf);
}

/*
 * reset the card
 */
static void
ray_reset(struct ray_softc *sc)
{
	if (++sc->sc_resetloop >= RAY_MAX_RESETS) {
		if (sc->sc_resetloop == RAY_MAX_RESETS) {
			aprint_error_dev(sc->sc_dev, "unable to correct, disabling\n");
			callout_stop(&sc->sc_reset_resetloop_ch);
			callout_reset(&sc->sc_disable_ch, 1,
			    (void (*)(void *))ray_disable, sc);
		}
	} else {
		aprint_error_dev(sc->sc_dev, "unexpected failure resetting hw [%d more]\n",
		    RAY_MAX_RESETS - sc->sc_resetloop);
		callout_stop(&sc->sc_reset_resetloop_ch);
		ray_init(sc);
		callout_reset(&sc->sc_reset_resetloop_ch, 30 * hz,
		    ray_reset_resetloop, sc);
	}
}

/*
 * return resetloop to zero (enough time has expired to allow user to
 * disable a whacked interface)  the main reason for all this nonesense
 * is that resets take ~2 seconds and currently the pcmcia code spins
 * on these resets
 */
static void
ray_reset_resetloop(void *arg)
{
	struct ray_softc *sc;

	sc = arg;
	sc->sc_resetloop = 0;
}

static int
ray_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ieee80211_nwid nwid;
	struct ray_param_req pr;
	struct ray_softc *sc;
	struct ifreq *ifr;
	struct ifaddr *ifa;
	int error, error2, s, i;

	sc = ifp->if_softc;
	error = 0;

	ifr = (struct ifreq *)data;

	s = splnet();

	RAY_DPRINTF(("%s: ioctl: cmd 0x%lx data 0x%lx\n", ifp->if_xname,
	    cmd, (long)data));
	switch (cmd) {
	case SIOCINITIFADDR:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCINITIFADDR\n", ifp->if_xname));
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			if ((error = ray_enable(sc)))
				break;
		ifp->if_flags |= IFF_UP;
		ifa = (struct ifaddr *)data;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_if, ifa);
			break;
#endif
		default:
			break;
		}
		break;
	case SIOCSIFFLAGS:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCSIFFLAGS\n", ifp->if_xname));
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				if ((error = ray_enable(sc)))
					break;
			} else
				ray_update_promisc(sc);
		} else if (ifp->if_flags & IFF_RUNNING)
			ray_disable(sc);
		break;
	case SIOCADDMULTI:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCADDMULTI\n", ifp->if_xname));
	case SIOCDELMULTI:
		if (cmd == SIOCDELMULTI)
			RAY_DPRINTF(("%s: ioctl: cmd SIOCDELMULTI\n",
			    ifp->if_xname));
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				ray_update_mcast(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCSIFMEDIA\n", ifp->if_xname));
	case SIOCGIFMEDIA:
		if (cmd == SIOCGIFMEDIA)
			RAY_DPRINTF(("%s: ioctl: cmd SIOCGIFMEDIA\n",
			    ifp->if_xname));
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;
	case SIOCSRAYPARAM:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCSRAYPARAM\n", ifp->if_xname));
		if ((error = copyin(ifr->ifr_data, &pr, sizeof(pr))))
			break;
		/* disallow certain command that have another interface */
		switch (pr.r_paramid) {
		case RAY_PID_NET_TYPE:	/* through media opt */
		case RAY_PID_AP_STATUS:	/* unsupported */
		case RAY_PID_SSID:	/* use SIOC80211[GS]NWID */
		case RAY_PID_MAC_ADDR:	/* XXX need interface? */
		case RAY_PID_PROMISC:	/* bpf */
			error = EINVAL;
			break;
		}
		error = ray_user_update_params(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;
	case SIOCGRAYPARAM:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCGRAYPARAM\n", ifp->if_xname));
		if ((error = copyin(ifr->ifr_data, &pr, sizeof(pr))))
			break;
		error = ray_user_report_params(sc, &pr);
		error2 = copyout(&pr, ifr->ifr_data, sizeof(pr));
		error = error2 ? error2 : error;
		break;
	case SIOCS80211NWID:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCS80211NWID\n", ifp->if_xname));
		/*
		 * if later people overwrite thats ok -- the latest version
		 * will always get start/joined even if it was set by
		 * a previous command
		 */
		if ((error = copyin(ifr->ifr_data, &nwid, sizeof(nwid))))
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		/* clear trailing garbages */
		for (i = nwid.i_len; i < IEEE80211_NWID_LEN; i++)
			nwid.i_nwid[i] = 0;
		if (!memcmp(&sc->sc_dnwid, &nwid, sizeof(nwid)))
			break;
		memcpy(&sc->sc_dnwid, &nwid, sizeof(nwid));
		if (ifp->if_flags & IFF_RUNNING)
			ray_start_join_net(sc);
		break;
	case SIOCG80211NWID:
		RAY_DPRINTF(("%s: ioctl: cmd SIOCG80211NWID\n", ifp->if_xname));
		error = copyout(&sc->sc_cnwid, ifr->ifr_data,
		    sizeof(sc->sc_cnwid));
		break;
#ifdef RAY_DO_SIGLEV
	case SIOCGRAYSIGLEV:
		error = copyout(sc->sc_siglevs, ifr->ifr_data,
			    sizeof sc->sc_siglevs);
		break;
#endif
	default:
		RAY_DPRINTF(("%s: ioctl: unknown\n", ifp->if_xname));
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	RAY_DPRINTF(("%s: ioctl: returns %d\n", ifp->if_xname, error));

	splx(s);

	return (error);
}

/*
 * ifnet interface to start transmission on the interface
 */
static void
ray_if_start(struct ifnet *ifp)
{
	struct ray_softc *sc;

	sc = ifp->if_softc;
	ray_intr_start(sc);
}

static void
ray_if_stop(struct ifnet *ifp, int disable)
{
	struct ray_softc *sc = ifp->if_softc;

	ray_stop(sc);
}

static int
ray_media_change(struct ifnet *ifp)
{
	struct ray_softc *sc;

	sc = ifp->if_softc;
	RAY_DPRINTF(("%s: media change cur %d\n", ifp->if_xname,
	    sc->sc_media.ifm_cur->ifm_media));
	if (sc->sc_media.ifm_cur->ifm_media & IFM_IEEE80211_ADHOC)
		sc->sc_mode = SC_MODE_ADHOC;
	else
		sc->sc_mode = SC_MODE_INFRA;
	if (sc->sc_mode != sc->sc_omode)
		ray_start_join_net(sc);
	return (0);
}

static void
ray_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ray_softc *sc;

	sc = ifp->if_softc;

	RAY_DPRINTF(("%s: media status\n", ifp->if_xname));

	imr->ifm_status = IFM_AVALID;
	if (sc->sc_havenet)
		imr->ifm_status |= IFM_ACTIVE;

	if (sc->sc_mode == SC_MODE_ADHOC)
		imr->ifm_active = IFM_ADHOC;
	else
		imr->ifm_active = IFM_INFRA;
}

/*
 * called to start from ray_intr.  We don't check for pending
 * interrupt as a result
 */
static void
ray_intr_start(struct ray_softc *sc)
{
	struct ieee80211_frame *iframe;
	struct ether_header *eh;
	size_t len, pktlen, tmplen;
	bus_size_t bufp, ebufp;
	struct mbuf *m0, *m;
	struct ifnet *ifp;
	u_int firsti, hinti, previ, i, pcount;
	u_int16_t et;
	u_int8_t *d;

	ifp = &sc->sc_if;

	RAY_DPRINTF(("%s: start free %d\n",
	    ifp->if_xname, sc->sc_txfree));

	ray_cmd_cancel(sc, SCP_IFSTART);

	if ((ifp->if_flags & IFF_RUNNING) == 0 || !sc->sc_havenet)
		return;

	if (IFQ_IS_EMPTY(&ifp->if_snd))
		return;

	firsti = i = previ = RAY_CCS_LINK_NULL;
	hinti = RAY_CCS_TX_FIRST;

	if (!RAY_ECF_READY(sc)) {
		ray_cmd_schedule(sc, SCP_IFSTART);
		return;
	}

	/* Check to see if we need to authenticate before sending packets. */
	if (sc->sc_authstate == RAY_AUTH_NEEDED) {
		RAY_DPRINTF(("%s: Sending auth request.\n", ifp->if_xname));
		sc->sc_authstate = RAY_AUTH_WAITING;
		ray_send_auth(sc, sc->sc_authid, OPEN_AUTH_REQUEST);
		return;
	}

	pcount = 0;
	for (;;) {
		/* if we have no descriptors be done */
		if (i == RAY_CCS_LINK_NULL) {
			i = ray_find_free_tx_ccs(sc, hinti);
			if (i == RAY_CCS_LINK_NULL) {
				RAY_DPRINTF(("%s: no descriptors.\n",
				    ifp->if_xname));
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (!m0) {
			RAY_DPRINTF(("%s: dry queue.\n", ifp->if_xname));
			break;
		}
		RAY_DPRINTF(("%s: gotmbuf 0x%lx\n", ifp->if_xname, (long)m0));
		pktlen = m0->m_pkthdr.len;
		if (pktlen > ETHER_MAX_LEN - ETHER_CRC_LEN) {
			RAY_DPRINTF((
			    "%s: mbuf too long %ld\n", ifp->if_xname,
			    (u_long)pktlen));
			ifp->if_oerrors++;
			m_freem(m0);
			continue;
		}
		RAY_DPRINTF(("%s: mbuf.m_pkthdr.len %d\n", ifp->if_xname,
		    (int)pktlen));

		/* we need the ether_header now for pktlen adjustments */
		M_PULLUP(m0, sizeof(struct ether_header));
		if (!m0) {
			RAY_DPRINTF(( "%s: couldn\'t pullup ether header\n",
			    ifp->if_xname));
			ifp->if_oerrors++;
			continue;
		}
		RAY_DPRINTF(("%s: got pulled up mbuf 0x%lx\n", ifp->if_xname,
		    (long)m0));

		/* first peek at the type of packet and figure out what to do */
		eh = mtod(m0, struct ether_header *);
		et = ntohs(eh->ether_type);
		if (ifp->if_flags & IFF_LINK0) {
			/* don't support llc for windows compat operation */
			if (et <= ETHERMTU) {
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}
			tmplen = sizeof(struct ieee80211_frame);
		} else if (et > ETHERMTU) {
			/* adjust for LLC/SNAP header */
			tmplen = sizeof(struct ieee80211_frame) - ETHER_ADDR_LEN;
		} else {
			tmplen = 0;
		}
		/* now get our space for the 802.11 frame */
		M_PREPEND(m0, tmplen, M_DONTWAIT);
		if (m0)
			M_PULLUP(m0, sizeof(struct ether_header) + tmplen);
		if (!m0) {
			RAY_DPRINTF(("%s: couldn\'t prepend header\n",
			    ifp->if_xname));
			ifp->if_oerrors++;
			continue;
		}
		/* copy the frame into the mbuf for tapping */
		iframe = mtod(m0, struct ieee80211_frame *);
		eh = (struct ether_header *)((u_int8_t *)iframe + tmplen);
		iframe->i_fc[0] =
		    (IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA);
		if (sc->sc_mode == SC_MODE_ADHOC) {
			iframe->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			memcpy(iframe->i_addr1, eh->ether_dhost,ETHER_ADDR_LEN);
			memcpy(iframe->i_addr2, eh->ether_shost,ETHER_ADDR_LEN);
			memcpy(iframe->i_addr3, sc->sc_bssid, ETHER_ADDR_LEN);
		} else {
			iframe->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			memcpy(iframe->i_addr1, sc->sc_bssid,ETHER_ADDR_LEN);
			memcpy(iframe->i_addr2, eh->ether_shost,ETHER_ADDR_LEN);
			memmove(iframe->i_addr3,eh->ether_dhost,ETHER_ADDR_LEN);
		}
		iframe->i_dur[0] = iframe->i_dur[1] = 0;
		iframe->i_seq[0] = iframe->i_seq[1] = 0;

		/* if not using crummy E2 in 802.11 make it LLC/SNAP */
		if ((ifp->if_flags & IFF_LINK0) == 0 && et > ETHERMTU)
			memcpy(iframe + 1, llc_snapid, sizeof(llc_snapid));

		RAY_DPRINTF(("%s: i %d previ %d\n", ifp->if_xname, i, previ));

		if (firsti == RAY_CCS_LINK_NULL)
			firsti = i;

		pktlen = m0->m_pkthdr.len;
		bufp = ray_fill_in_tx_ccs(sc, pktlen, i, previ);
		previ = hinti = i;
		i = RAY_CCS_LINK_NULL;

		RAY_DPRINTF(("%s: bufp 0x%lx new pktlen %d\n",
		    ifp->if_xname, (long)bufp, (int)pktlen));

		/* copy out mbuf */
		for (m = m0; m; m = m->m_next) {
			if ((len = m->m_len) == 0)
				continue;
			RAY_DPRINTF((
			    "%s: copying mbuf 0x%lx bufp 0x%lx len %d\n",
			    ifp->if_xname, (long)m, (long)bufp, (int)len));
			d = mtod(m, u_int8_t *);
			ebufp = bufp + len;
			if (ebufp <= RAY_TX_END)
				ray_write_region(sc, bufp, d, len);
			else {
				panic("ray_intr_start");	/* XXX */
				/* wrapping */
				tmplen = ebufp - bufp;
				len -= tmplen;
				ray_write_region(sc, bufp, d, tmplen);
				d += tmplen;
				bufp = RAY_TX_BASE;
				ray_write_region(sc, bufp, d, len);
			}
			bufp += len;
		}
		if (ifp->if_bpf) {
			if (ifp->if_flags & IFF_LINK0) {
				m0->m_data += sizeof(struct ieee80211_frame);
				m0->m_len -=  sizeof(struct ieee80211_frame);
				m0->m_pkthdr.len -=  sizeof(struct ieee80211_frame);
			}
			bpf_mtap(ifp, m0);
			if (ifp->if_flags & IFF_LINK0) {
				m0->m_data -= sizeof(struct ieee80211_frame);
				m0->m_len +=  sizeof(struct ieee80211_frame);
				m0->m_pkthdr.len +=  sizeof(struct ieee80211_frame);
			}
		}

#ifdef RAY_DEBUG
		if (ray_debug && ray_debug_dump_tx)
			ray_dump_mbuf(sc, m0);
#endif
		pcount++;
		m_freem(m0);

		RAY_DPRINTF_XMIT(("%s: sent packet: len %ld\n", device_xname(sc->sc_dev),
		    (u_long)pktlen));
	}

	if (firsti == RAY_CCS_LINK_NULL)
		return;
	i = 0;
	if (!RAY_ECF_READY(sc)) {
		/*
		 * if this can really happen perhaps we need to save
		 * the chain and use it later.  I think this might
		 * be a confused state though because we check above
		 * and don't issue any commands between.
		 */
		printf("%s: dropping tx packets device busy\n", device_xname(sc->sc_dev));
		ray_free_ccs_chain(sc, firsti);
		ifp->if_oerrors += pcount;
		return;
	}

	/* send it off */
	RAY_DPRINTF(("%s: ray_start issuing %d \n", device_xname(sc->sc_dev), firsti));
	SRAM_WRITE_1(sc, RAY_SCB_CCSI, firsti);
	RAY_ECF_START_CMD(sc);

	ifp->if_opackets += pcount;
}

/*
 * recevice a packet from the card
 */
static void
ray_recv(struct ray_softc *sc, bus_size_t ccs)
{
	struct ieee80211_frame *frame;
	struct ether_header *eh;
	struct mbuf *m;
	size_t pktlen, fudge, len, lenread = 0;
	bus_size_t bufp, ebufp, tmp;
	struct ifnet *ifp;
	u_int8_t *src, *d;
	u_int frag = 0, ni, i, issnap, first;
	u_int8_t fc0;
#ifdef RAY_DO_SIGLEV
	u_int8_t siglev;
#endif

#ifdef RAY_DEBUG
	/* have a look if you want to see how the card rx works :) */
	if (ray_debug && ray_debug_dump_desc)
		hexdump((char *)sc->sc_memh + RAY_RCS_BASE, 0x400,
		    16, 4, 0);
#endif

	m = 0;
	ifp = &sc->sc_if;

	/*
	 * If we're expecting the E2-in-802.11 encapsulation that the
	 * WebGear Windows driver produces, fudge the packet forward
	 * in the mbuf by 2 bytes so that the payload after the
	 * Ethernet header will be aligned.  If we end up getting a
	 * packet that's not of this type, we'll just drop it anyway.
	 */
	if (ifp->if_flags & IFF_LINK0)
		fudge = 2;
	else
		fudge = 0;

	/* it looks like at least with build 4 there is no CRC in length */
	first = RAY_GET_INDEX(ccs);
	pktlen = SRAM_READ_FIELD_2(sc, ccs, ray_cmd_rx, c_pktlen);
#ifdef RAY_DO_SIGLEV
	siglev = SRAM_READ_FIELD_1(sc, ccs, ray_cmd_rx, c_siglev);
#endif

	RAY_DPRINTF(("%s: recv pktlen %ld frag %d\n", device_xname(sc->sc_dev),
	    (u_long)pktlen, frag));
	RAY_DPRINTF_XMIT(("%s: received packet: len %ld\n", device_xname(sc->sc_dev),
	    (u_long)pktlen));
	if (pktlen > MCLBYTES || pktlen < sizeof(*frame)) {
		RAY_DPRINTF(("%s: PKTLEN TOO BIG OR TOO SMALL\n",
		    device_xname(sc->sc_dev)));
		ifp->if_ierrors++;
		goto done;
	}
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (!m) {
		RAY_DPRINTF(("%s: MGETHDR FAILED\n", device_xname(sc->sc_dev)));
		ifp->if_ierrors++;
		goto done;
	}
	if ((pktlen + fudge) > MHLEN) {
		/* XXX should allow chaining? */
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			RAY_DPRINTF(("%s: MCLGET FAILED\n", device_xname(sc->sc_dev)));
			ifp->if_ierrors++;
			m_freem(m);
			m = 0;
			goto done;
		}
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = pktlen;
	m->m_len = pktlen;
	m->m_data += fudge;
	d = mtod(m, u_int8_t *);

	RAY_DPRINTF(("%s: recv ccs index %d\n", device_xname(sc->sc_dev), first));
	i = ni = first;
	while ((i = ni) && i != RAY_CCS_LINK_NULL) {
		ccs = RAY_GET_CCS(i);
		bufp = SRAM_READ_FIELD_2(sc, ccs, ray_cmd_rx, c_bufp);
		len = SRAM_READ_FIELD_2(sc, ccs, ray_cmd_rx, c_len);
		/* remove the CRC */
#if 0
		/* at least with build 4 no crc seems to be here */
		if (frag++ == 0)
			len -= 4;
#endif
		ni = SRAM_READ_FIELD_1(sc, ccs, ray_cmd_rx, c_nextfrag);
		RAY_DPRINTF((
		    "%s: recv frag index %d len %ld bufp 0x%llx ni %d\n",
		    device_xname(sc->sc_dev), i, (u_long)len, (unsigned long long)bufp,
		    ni));
		if (len + lenread > pktlen) {
			RAY_DPRINTF(("%s: BAD LEN current 0x%lx pktlen 0x%lx\n",
			    device_xname(sc->sc_dev), (u_long)(len + lenread),
			    (u_long)pktlen));
			ifp->if_ierrors++;
			m_freem(m);
			m = 0;
			goto done;
		}
		if (i < RAY_RCCS_FIRST) {
			printf("ray_recv: bad ccs index 0x%x\n", i);
			m_freem(m);
			m = 0;
			goto done;
		}

		ebufp = bufp + len;
		if (ebufp <= RAY_RX_END)
			ray_read_region(sc, bufp, d, len);
		else {
			/* wrapping */
			ray_read_region(sc, bufp, d, (tmp = RAY_RX_END - bufp));
			ray_read_region(sc, RAY_RX_BASE, d + tmp, ebufp - RAY_RX_END);
		}
		d += len;
		lenread += len;
	}
done:

	RAY_DPRINTF(("%s: recv frag count %d\n", device_xname(sc->sc_dev), frag));

	/* free the rcss */
	ni = first;
	while ((i = ni) && (i != RAY_CCS_LINK_NULL)) {
		ccs = RAY_GET_CCS(i);
		ni = SRAM_READ_FIELD_1(sc, ccs, ray_cmd_rx, c_nextfrag);
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status,
		    RAY_CCS_STATUS_FREE);
	}

	if (!m)
		return;

	RAY_DPRINTF(("%s: recv got packet pktlen %ld actual %ld\n",
	    device_xname(sc->sc_dev), (u_long)pktlen, (u_long)lenread));
#ifdef RAY_DEBUG
	if (ray_debug && ray_debug_dump_rx)
		ray_dump_mbuf(sc, m);
#endif
	/* receivce the packet */
	frame = mtod(m, struct ieee80211_frame *);
	fc0 = frame->i_fc[0]
	   & (IEEE80211_FC0_VERSION_MASK|IEEE80211_FC0_TYPE_MASK);
	if ((fc0 & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
		RAY_DPRINTF(("%s: pkt not version 0 fc 0x%x\n",
		    device_xname(sc->sc_dev), fc0));
		m_freem(m);
		return;
	}
	if ((fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) {
		switch (frame->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
		case IEEE80211_FC0_SUBTYPE_BEACON:
			/* Ignore beacon silently. */
			break;
		case IEEE80211_FC0_SUBTYPE_AUTH:
			ray_recv_auth(sc, frame);
			break;
		case IEEE80211_FC0_SUBTYPE_DEAUTH:
			sc->sc_authstate = RAY_AUTH_UNAUTH;
			break;
		default:
			RAY_DPRINTF(("%s: mgt packet not supported\n",
			    device_xname(sc->sc_dev)));
#ifdef RAY_DEBUG
			hexdump((const u_int8_t*)frame, pktlen, 16, 4, 0);
#endif
			RAY_DPRINTF(("\n"));
			break;
		}
		m_freem(m);
		return;
	} else if ((fc0 & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_DATA) {
		RAY_DPRINTF(("%s: pkt not type data fc0 0x%x\n",
		    device_xname(sc->sc_dev), fc0));
		m_freem(m);
		return;
	}

	if (pktlen < sizeof(*frame) + sizeof(struct llc)) {
		RAY_DPRINTF(("%s: pkt too small for llc (%ld)\n",
		    device_xname(sc->sc_dev), (u_long)pktlen));
		m_freem(m);
		return;
	}

	if (!memcmp(frame + 1, llc_snapid, sizeof(llc_snapid)))
		issnap = 1;
	else {
		/*
		 * if user has link0 flag set we allow the weird
		 * Ethernet2 in 802.11 encapsulation produced by
		 * the windows driver for the WebGear card
		 */
		RAY_DPRINTF(("%s: pkt not snap 0\n", device_xname(sc->sc_dev)));
		if ((ifp->if_flags & IFF_LINK0) == 0) {
			m_freem(m);
			return;
		}
		issnap = 0;
	}
	switch (frame->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		src = frame->i_addr2;
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		src = frame->i_addr3;
		break;
	case IEEE80211_FC1_DIR_TODS:
		RAY_DPRINTF(("%s: pkt ap2ap\n", device_xname(sc->sc_dev)));
		m_freem(m);
		return;
	default:
		RAY_DPRINTF(("%s: pkt type unknown\n", device_xname(sc->sc_dev)));
		m_freem(m);
		return;
	}

#ifdef RAY_DO_SIGLEV
	ray_update_siglev(sc, src, siglev);
#endif

	/*
	 * This is a mess.. we should support other LLC frame types
	 */
	if (issnap) {
		/* create an ether_header over top of the 802.11+SNAP header */
		eh = (struct ether_header *)((char *)(frame + 1) - 6);
		memcpy(eh->ether_shost, src, ETHER_ADDR_LEN);
		memcpy(eh->ether_dhost, frame->i_addr1, ETHER_ADDR_LEN);
	} else {
		/* this is the weird e2 in 802.11 encapsulation */
		eh = (struct ether_header *)(frame + 1);
	}
	m_adj(m, (char *)eh - (char *)frame);
	bpf_mtap(ifp, m);
	/* XXX doesn't appear to be included m->m_flags |= M_HASFCS; */
	ifp->if_ipackets++;
	(*ifp->if_input)(ifp, m);
}

/*
 * receive an auth packet
 */
static void
ray_recv_auth(struct ray_softc *sc, struct ieee80211_frame *frame)
{
	u_int8_t *var = (u_int8_t *)(frame + 1);

	if (sc->sc_mode == SC_MODE_ADHOC) {
		RAY_DPRINTF(("%s: recv auth packet:\n", device_xname(sc->sc_dev)));
#ifdef RAY_DEBUG
		hexdump((const u_int8_t *)frame, sizeof(*frame) + 6, 16, 4, 0);
#endif
		RAY_DPRINTF(("\n"));

		if (var[2] == OPEN_AUTH_REQUEST) {
			RAY_DPRINTF(("%s: Sending authentication response.\n",
			    device_xname(sc->sc_dev)));
			if (ray_send_auth(sc, frame->i_addr2,
			    OPEN_AUTH_RESPONSE) == 0) {
				sc->sc_authstate = RAY_AUTH_NEEDED;
				memcpy(sc->sc_authid, frame->i_addr2,
				    ETHER_ADDR_LEN);
			}
		} else if (var[2] == OPEN_AUTH_RESPONSE) {
			RAY_DPRINTF(("%s: Authenticated!\n",
			    device_xname(sc->sc_dev)));
			sc->sc_authstate = RAY_AUTH_AUTH;
		}
	}
}

/*
 * send an auth packet
 */
static int
ray_send_auth(struct ray_softc *sc, u_int8_t *dest, u_int8_t auth_type)
{
	u_int8_t packet[sizeof(struct ieee80211_frame) + ETHER_ADDR_LEN], *var;
	struct ieee80211_frame *frame;
	bus_size_t bufp;
	int ccsindex;

	ccsindex = ray_find_free_tx_ccs(sc, RAY_CCS_TX_FIRST);
	if (ccsindex == RAY_CCS_LINK_NULL) {
		RAY_DPRINTF(("%s: send auth failed -- no free tx slots\n",
		    device_xname(sc->sc_dev)));
		return (ENOMEM);
	}

	bufp = ray_fill_in_tx_ccs(sc, sizeof(packet), ccsindex,
	    RAY_CCS_LINK_NULL);
	frame = (struct ieee80211_frame *) packet;
	frame->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_SUBTYPE_AUTH;
	frame->i_fc[1] = 0;
	memcpy(frame->i_addr1, dest, ETHER_ADDR_LEN);
	memcpy(frame->i_addr2, sc->sc_ecf_startup.e_station_addr,
	    ETHER_ADDR_LEN);
	memcpy(frame->i_addr3, sc->sc_bssid, ETHER_ADDR_LEN);

	var = (u_int8_t *)(frame + 1);
	memset(var, 0, ETHER_ADDR_LEN);
	var[2] = auth_type;

	ray_write_region(sc, bufp, packet, sizeof(packet));

	SRAM_WRITE_1(sc, RAY_SCB_CCSI, ccsindex);
	RAY_ECF_START_CMD(sc);

	RAY_DPRINTF_XMIT(("%s: sent auth packet: len %lu\n",
	    device_xname(sc->sc_dev), (u_long) sizeof(packet)));

	return (0);
}

/*
 * scan for free buffers
 *
 * Note: do _not_ try to optimize this away, there is some kind of
 * horrible interaction with receiving tx interrupts and they
 * have to be done as fast as possible, which means zero processing.
 * this took ~ever to figure out, don't make someone do it again!
 */
static u_int
ray_find_free_tx_ccs(struct ray_softc *sc, u_int hint)
{
	u_int i, stat;

	for (i = hint; i <= RAY_CCS_TX_LAST; i++) {
		stat = SRAM_READ_FIELD_1(sc, RAY_GET_CCS(i), ray_cmd, c_status);
		if (stat == RAY_CCS_STATUS_FREE)
			return (i);
	}

	if (hint == RAY_CCS_TX_FIRST)
		return (RAY_CCS_LINK_NULL);

	for (i = RAY_CCS_TX_FIRST; i < hint; i++) {
		stat = SRAM_READ_FIELD_1(sc, RAY_GET_CCS(i), ray_cmd, c_status);
		if (stat == RAY_CCS_STATUS_FREE)
			return (i);
	}
	return (RAY_CCS_LINK_NULL);
}

/*
 * allocate, initialize and link in a tx ccs for the given
 * page and the current chain values
 */
static bus_size_t
ray_fill_in_tx_ccs(struct ray_softc *sc, size_t pktlen, u_int i, u_int pi)
{
	bus_size_t ccs, bufp;

	/* pktlen += RAY_TX_PHY_SIZE; */
	bufp = RAY_TX_BASE + i * RAY_TX_BUF_SIZE;
	bufp += sc->sc_txpad;
	ccs = RAY_GET_CCS(i);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_status, RAY_CCS_STATUS_BUSY);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_cmd, RAY_CMD_TX_REQ);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_link, RAY_CCS_LINK_NULL);
	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_bufp, bufp);
	SRAM_WRITE_FIELD_2(sc, ccs, ray_cmd_tx, c_len, pktlen);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_tx_rate, sc->sc_deftxrate);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_apm_mode, 0);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_tx, c_antenna, 0);

	/* link us in */
	if (pi != RAY_CCS_LINK_NULL)
		SRAM_WRITE_FIELD_1(sc, RAY_GET_CCS(pi), ray_cmd_tx, c_link, i);

	RAY_DPRINTF(("%s: ray_alloc_tx_ccs bufp 0x%llx idx %u pidx %u\n",
	    device_xname(sc->sc_dev), (unsigned long long)bufp, i, pi));

	return (bufp + RAY_TX_PHY_SIZE);
}

/*
 * an update params command has completed lookup which command and
 * the status
 */
static ray_cmd_func_t
ray_update_params_done(struct ray_softc *sc, bus_size_t ccs, u_int stat)
{
	ray_cmd_func_t rcmd;

	rcmd = 0;

	RAY_DPRINTF(("%s: ray_update_params_done stat %d\n",
	   device_xname(sc->sc_dev), stat));

	/* this will get more complex as we add commands */
	if (stat == RAY_CCS_STATUS_FAIL) {
		printf("%s: failed to update a promisc\n", device_xname(sc->sc_dev));
		/* XXX should probably reset */
		/* rcmd = ray_reset; */
	}

	if (sc->sc_running & SCP_UPD_PROMISC) {
		ray_cmd_done(sc, SCP_UPD_PROMISC);
		sc->sc_promisc = SRAM_READ_1(sc, RAY_HOST_TO_ECF_BASE);
		RAY_DPRINTF(("%s: new promisc value %d\n", device_xname(sc->sc_dev),
		    sc->sc_promisc));
	} else if (sc->sc_updreq) {
		ray_cmd_done(sc, SCP_UPD_UPDATEPARAMS);
		/* get the update parameter */
		sc->sc_updreq->r_failcause =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_update, c_failcause);
		sc->sc_updreq = 0;
		wakeup(ray_update_params);

		rcmd = ray_start_join_net;
	}
	return (rcmd);
}

/*
 *  check too see if we have any pending commands.
 */
static void
ray_check_scheduled(void *arg)
{
	struct ray_softc *sc;
	int s, i, mask;

	s = splnet();

	sc = arg;
	RAY_DPRINTF((
	    "%s: ray_check_scheduled enter schd 0x%x running 0x%x ready %d\n",
	    device_xname(sc->sc_dev), sc->sc_scheduled, sc->sc_running, RAY_ECF_READY(sc)));

	if (sc->sc_timoneed) {
		callout_stop(&sc->sc_check_scheduled_ch);
		sc->sc_timoneed = 0;
	}

	/* if update subcmd is running -- clear it in scheduled */
	if (sc->sc_running & SCP_UPDATESUBCMD)
		sc->sc_scheduled &= ~SCP_UPDATESUBCMD;

	mask = SCP_FIRST;
	for (i = 0; i < ray_ncmdtab; mask <<= 1, i++) {
		if ((sc->sc_scheduled & ~SCP_UPD_MASK) == 0)
			break;
		if (!RAY_ECF_READY(sc))
			break;
		if (sc->sc_scheduled & mask)
			(*ray_cmdtab[i])(sc);
	}

	RAY_DPRINTF((
	    "%s: ray_check_scheduled exit sched 0x%x running 0x%x ready %d\n",
	    device_xname(sc->sc_dev), sc->sc_scheduled, sc->sc_running, RAY_ECF_READY(sc)));

	if (sc->sc_scheduled & ~SCP_UPD_MASK)
		ray_set_pending(sc, sc->sc_scheduled);

	splx(s);
}

/*
 * check for unreported returns
 *
 * this routine is coded to only expect one outstanding request for the
 * timed out requests at a time, but thats all that can be outstanding
 * per hardware limitations
 */
static void
ray_check_ccs(void *arg)
{
	ray_cmd_func_t fp;
	struct ray_softc *sc;
	u_int i, cmd, stat = 0;
	bus_size_t ccs = 0;
	int s;

	s = splnet();
	sc = arg;

	RAY_DPRINTF(("%s: ray_check_ccs\n", device_xname(sc->sc_dev)));

	sc->sc_timocheck = 0;
	for (i = RAY_CCS_CMD_FIRST; i <= RAY_CCS_CMD_LAST; i++) {
		if (!sc->sc_ccsinuse[i])
			continue;
		ccs = RAY_GET_CCS(i);
		cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
		switch (cmd) {
		case RAY_CMD_START_PARAMS:
		case RAY_CMD_UPDATE_MCAST:
		case RAY_CMD_UPDATE_PARAMS:
			stat = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
			RAY_DPRINTF(("%s: check ccs idx %u ccs 0x%llx "
			    "cmd 0x%x stat %u\n", device_xname(sc->sc_dev), i,
			    (unsigned long long)ccs, cmd, stat));
			goto breakout;
		}
	}
breakout:
	/* see if we got one of the commands we are looking for */
	if (i > RAY_CCS_CMD_LAST)
		; /* nothing */
	else if (stat == RAY_CCS_STATUS_FREE) {
		stat = RAY_CCS_STATUS_COMPLETE;
		if ((fp = ray_ccs_done(sc, ccs)))
			(*fp)(sc);
	} else if (stat != RAY_CCS_STATUS_BUSY) {
		if (sc->sc_ccsinuse[i] == 1) {
			/* give a chance for the interrupt to occur */
			sc->sc_ccsinuse[i] = 2;
			if (!sc->sc_timocheck) {
				callout_reset(&sc->sc_check_ccs_ch, 1,
				    ray_check_ccs, sc);
				sc->sc_timocheck = 1;
			}
		} else if ((fp = ray_ccs_done(sc, ccs)))
			(*fp)(sc);
	} else {
		callout_reset(&sc->sc_check_ccs_ch, RAY_CHECK_CCS_TIMEOUT,
		    ray_check_ccs, sc);
		sc->sc_timocheck = 1;
	}
	splx(s);
}

/*
 * read the counters, the card implements the following protocol
 * to keep the values from being changed while read:  It checks
 * the `own' bit and if zero writes the current internal counter
 * value, it then sets the `own' bit to 1.  If the `own' bit was 1 it
 * increments its internal counter.  The user thus reads the counter
 * if the `own' bit is one and then sets the own bit to 0.
 */
static void
ray_update_error_counters(struct ray_softc *sc)
{
	bus_size_t csc;

	/* try and update the error counters */
	csc = RAY_STATUS_BASE;
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_mrxo_own)) {
		sc->sc_rxoverflow +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_mrx_overflow);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_mrxo_own, 0);
	}
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_mrxc_own)) {
		sc->sc_rxcksum +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_mrx_overflow);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_mrxc_own, 0);
	}
	if (SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_rxhc_own)) {
		sc->sc_rxhcksum +=
		    SRAM_READ_FIELD_2(sc, csc, ray_csc, csc_rx_hcksum);
		SRAM_WRITE_FIELD_1(sc, csc, ray_csc, csc_rxhc_own, 0);
	}
	sc->sc_rxnoise = SRAM_READ_FIELD_1(sc, csc, ray_csc, csc_rx_noise);
}

/*
 * one of the commands we issued has completed, process.
 */
static ray_cmd_func_t
ray_ccs_done(struct ray_softc *sc, bus_size_t ccs)
{
	ray_cmd_func_t rcmd;
	u_int cmd, stat;

	cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
	stat = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);

	RAY_DPRINTF(("%s: ray_ccs_done idx %llu cmd 0x%x stat %u\n",
	    device_xname(sc->sc_dev), (unsigned long long)RAY_GET_INDEX(ccs), cmd, stat));

	rcmd = 0;
	switch (cmd) {
	/*
	 * solicited commands
	 */
	case RAY_CMD_START_PARAMS:
		/* start network */
		ray_cmd_done(sc, SCP_UPD_STARTUP);

		/* ok to start queueing packets */
		sc->sc_if.if_flags &= ~IFF_OACTIVE;

		sc->sc_omode = sc->sc_mode;
		memcpy(&sc->sc_cnwid, &sc->sc_dnwid, sizeof(sc->sc_cnwid));

		rcmd = ray_start_join_net;
		break;
	case RAY_CMD_UPDATE_PARAMS:
		rcmd = ray_update_params_done(sc, ccs, stat);
		break;
	case RAY_CMD_REPORT_PARAMS:
		/* get the reported parameters */
		ray_cmd_done(sc, SCP_REPORTPARAMS);
		if (!sc->sc_repreq)
			break;
		sc->sc_repreq->r_failcause =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_failcause);
		sc->sc_repreq->r_len =
		    SRAM_READ_FIELD_1(sc, ccs, ray_cmd_report, c_len);
		ray_read_region(sc, RAY_ECF_TO_HOST_BASE, sc->sc_repreq->r_data,
		    sc->sc_repreq->r_len);
		sc->sc_repreq = 0;
		wakeup(ray_report_params);
		break;
	case RAY_CMD_UPDATE_MCAST:
		ray_cmd_done(sc, SCP_UPD_MCAST);
		if (stat == RAY_CCS_STATUS_FAIL)
			rcmd = ray_reset;
		break;
	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
		rcmd = ray_start_join_net_done(sc, cmd, ccs, stat);
		break;
	case RAY_CMD_TX_REQ:
		if (sc->sc_if.if_flags & IFF_OACTIVE) {
			sc->sc_if.if_flags &= ~IFF_OACTIVE;
			/* this may also be a problem */
			rcmd = ray_intr_start;
		}
		/* free it -- no tracking */
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status,
		    RAY_CCS_STATUS_FREE);
		goto done;
	case RAY_CMD_START_ASSOC:
		ray_cmd_done(sc, SCP_STARTASSOC);
		if (stat == RAY_CCS_STATUS_FAIL)
			rcmd = ray_start_join_net;	/* XXX check */
		else {
			sc->sc_havenet = 1;
			rcmd = ray_intr_start;
		}
		break;
	case RAY_CMD_UPDATE_APM:
	case RAY_CMD_TEST_MEM:
	case RAY_CMD_SHUTDOWN:
	case RAY_CMD_DUMP_MEM:
	case RAY_CMD_START_TIMER:
		break;
	default:
		printf("%s: intr: unknown command 0x%x\n",
		    sc->sc_if.if_xname, cmd);
		break;
	}
	ray_free_ccs(sc, ccs);
done:
	/*
	 * see if needed things can be done now that a command
	 * has completed
	 */
	ray_check_scheduled(sc);

	return (rcmd);
}

/*
 * an unsolicited interrupt, i.e., the ECF is sending us a command
 */
static ray_cmd_func_t
ray_rccs_intr(struct ray_softc *sc, bus_size_t ccs)
{
	ray_cmd_func_t rcmd;
	u_int cmd, stat;

	cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
	stat = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);

	RAY_DPRINTF(("%s: ray_rccs_intr idx %llu cmd 0x%x stat %u\n",
	    device_xname(sc->sc_dev), (unsigned long long)RAY_GET_INDEX(ccs), cmd, stat));

	rcmd = 0;
	switch (cmd) {
	/*
	 * unsolicited commands
	 */
	case RAY_ECMD_RX_DONE:
		ray_recv(sc, ccs);
		goto done;
	case RAY_ECMD_REJOIN_DONE:
		if (sc->sc_mode == SC_MODE_ADHOC)
			break;
		/* get the current ssid */
		SRAM_READ_FIELD_N(sc, ccs, ray_cmd_net, c_bss_id,
		    sc->sc_bssid, sizeof(sc->sc_bssid));
		rcmd = ray_start_assoc;
		break;
	case RAY_ECMD_ROAM_START:
		/* no longer have network */
		sc->sc_havenet = 0;
		break;
	case RAY_ECMD_JAPAN_CALL_SIGNAL:
		break;
	default:
		ray_update_error_counters(sc);

		/* this is a bogus return from build 4 don't free 0x55 */
		if (sc->sc_version == SC_BUILD_4 && cmd == 0x55
		    && RAY_GET_INDEX(ccs) == 0x55) {
			goto done;
		}
		printf("%s: intr: unknown command 0x%x\n",
		    sc->sc_if.if_xname, cmd);
		break;
	}
	/* free the ccs */
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);
done:
	return (rcmd);
}

/*
 * process an interrupt
 */
static int
ray_intr(void *arg)
{
	struct ray_softc *sc;
	ray_cmd_func_t rcmd;
	u_int i, count;

	sc = arg;

	RAY_DPRINTF(("%s: ray_intr\n", device_xname(sc->sc_dev)));

	if ((++sc->sc_checkcounters % 32) == 0)
		ray_update_error_counters(sc);

	count = 0;
	rcmd = 0;
	if (!REG_READ(sc, RAY_HCSIR))
		count = 0;
	else {
		count = 1;
		i = SRAM_READ_1(sc, RAY_SCB_RCCSI);
		if (i <= RAY_CCS_LAST)
			rcmd = ray_ccs_done(sc, RAY_GET_CCS(i));
		else if (i <= RAY_RCCS_LAST)
			rcmd = ray_rccs_intr(sc, RAY_GET_CCS(i));
		else
			printf("%s: intr: bad cmd index %d\n", device_xname(sc->sc_dev), i);
	}

	if (rcmd)
		(*rcmd)(sc);

	if (count)
		REG_WRITE(sc, RAY_HCSIR, 0);

	RAY_DPRINTF(("%s: interrupt handled %d\n", device_xname(sc->sc_dev), count));

	return (count ? 1 : 0);
}


/*
 * Generic CCS handling
 */

/*
 * free the chain of descriptors -- used for freeing allocated tx chains
 */
static void
ray_free_ccs_chain(struct ray_softc *sc, u_int ni)
{
	u_int i;

	while ((i = ni) != RAY_CCS_LINK_NULL) {
		ni = SRAM_READ_FIELD_1(sc, RAY_GET_CCS(i), ray_cmd, c_link);
		SRAM_WRITE_FIELD_1(sc, RAY_GET_CCS(i), ray_cmd, c_status,
		    RAY_CCS_STATUS_FREE);
	}
}

/*
 * free up a cmd and return the old status
 * this routine is only used for commands
 */
static u_int8_t
ray_free_ccs(struct ray_softc *sc, bus_size_t ccs)
{
	u_int8_t stat;

	RAY_DPRINTF(("%s: free_ccs idx %llu\n", device_xname(sc->sc_dev),
	    (unsigned long long)RAY_GET_INDEX(ccs)));

	stat = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);
	if (ccs <= RAY_GET_CCS(RAY_CCS_LAST))
		sc->sc_ccsinuse[RAY_GET_INDEX(ccs)] = 0;

	return (stat);
}

/*
 * returns 1 and in `ccb' the bus offset of the free ccb
 * or 0 if none are free
 *
 * If `track' is not zero, handles tracking this command
 * possibly indicating a callback is needed and setting a timeout
 * also if ECF isn't ready we terminate earlier to avoid overhead.
 *
 * this routine is only used for commands
 */
static int
ray_alloc_ccs(struct ray_softc *sc, bus_size_t *ccsp, u_int cmd, u_int track)
{
	bus_size_t ccs;
	u_int i;

	RAY_DPRINTF(("%s: alloc_ccs cmd %d\n", device_xname(sc->sc_dev), cmd));

	/* for tracked commands, if not ready just set pending */
	if (track && !RAY_ECF_READY(sc)) {
		ray_cmd_schedule(sc, track);
		return (0);
	}

	/* first scan our inuse array */
	for (i = RAY_CCS_CMD_FIRST; i <= RAY_CCS_CMD_LAST; i++) {
		/* XXX wonder if we have to probe here to make the card go */
		(void)SRAM_READ_FIELD_1(sc, RAY_GET_CCS(i), ray_cmd, c_status);
		if (!sc->sc_ccsinuse[i])
			break;
	}
	if (i > RAY_CCS_CMD_LAST) {
		if (track)
			ray_cmd_schedule(sc, track);
		return (0);
	}
	sc->sc_ccsinuse[i] = 1;
	ccs = RAY_GET_CCS(i);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_BUSY);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_cmd, cmd);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_link, RAY_CCS_LINK_NULL);

	*ccsp = ccs;
	return (1);
}


/*
 * this function sets the pending bit for the command given in 'need'
 * and schedules a timeout if none is scheduled already.  Any command
 * that uses the `host to ecf' region must be serialized.
 */
static void
ray_set_pending(struct ray_softc *sc, u_int cmdf)
{
	RAY_DPRINTF(("%s: ray_set_pending 0x%x\n", device_xname(sc->sc_dev), cmdf));

	sc->sc_scheduled |= cmdf;
	if (!sc->sc_timoneed) {
		RAY_DPRINTF(("%s: ray_set_pending new timo\n", device_xname(sc->sc_dev)));
		callout_reset(&sc->sc_check_scheduled_ch,
		    RAY_CHECK_SCHED_TIMEOUT, ray_check_scheduled, sc);
		sc->sc_timoneed = 1;
	}
}

/*
 * schedule the `cmdf' for completion later
 */
static void
ray_cmd_schedule(struct ray_softc *sc, int cmdf)
{
	int track;

	RAY_DPRINTF(("%s: ray_cmd_schedule 0x%x\n", device_xname(sc->sc_dev), cmdf));

	track = cmdf;
	if ((cmdf & SCP_UPD_MASK) == 0)
		ray_set_pending(sc, track);
	else if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		/* don't do timeout mechanism if subcmd already going */
		sc->sc_scheduled |= cmdf;
	} else
		ray_set_pending(sc, cmdf | SCP_UPDATESUBCMD);
}

/*
 * check to see if `cmdf' has been scheduled
 */
static int
ray_cmd_is_scheduled(struct ray_softc *sc, int cmdf)
{
	RAY_DPRINTF(("%s: ray_cmd_is_scheduled 0x%x\n", device_xname(sc->sc_dev), cmdf));

	return ((sc->sc_scheduled & cmdf) ? 1 : 0);
}

/*
 * cancel a scheduled command (not a running one though!)
 */
static void
ray_cmd_cancel(struct ray_softc *sc, int cmdf)
{
	RAY_DPRINTF(("%s: ray_cmd_cancel 0x%x\n", device_xname(sc->sc_dev), cmdf));

	sc->sc_scheduled &= ~cmdf;
	if ((cmdf & SCP_UPD_MASK) && (sc->sc_scheduled & SCP_UPD_MASK) == 0)
		sc->sc_scheduled &= ~SCP_UPDATESUBCMD;

	/* if nothing else needed cancel the timer */
	if (sc->sc_scheduled == 0 && sc->sc_timoneed) {
		callout_stop(&sc->sc_check_scheduled_ch);
		sc->sc_timoneed = 0;
	}
}

/*
 * called to indicate the 'cmdf' has been issued
 */
static void
ray_cmd_ran(struct ray_softc *sc, int cmdf)
{
	RAY_DPRINTF(("%s: ray_cmd_ran 0x%x\n", device_xname(sc->sc_dev), cmdf));

	if (cmdf & SCP_UPD_MASK)
		sc->sc_running |= cmdf | SCP_UPDATESUBCMD;
	else
		sc->sc_running |= cmdf;

	if ((cmdf & SCP_TIMOCHECK_CMD_MASK) && !sc->sc_timocheck) {
		callout_reset(&sc->sc_check_ccs_ch, RAY_CHECK_CCS_TIMEOUT,
		    ray_check_ccs, sc);
		sc->sc_timocheck = 1;
	}
}

/*
 * check to see if `cmdf' has been issued
 */
static int
ray_cmd_is_running(struct ray_softc *sc, int cmdf)
{
	RAY_DPRINTF(("%s: ray_cmd_is_running 0x%x\n", device_xname(sc->sc_dev), cmdf));

	return ((sc->sc_running & cmdf) ? 1 : 0);
}

/*
 * the given `cmdf' that was issued has completed
 */
static void
ray_cmd_done(struct ray_softc *sc, int cmdf)
{
	RAY_DPRINTF(("%s: ray_cmd_done 0x%x\n", device_xname(sc->sc_dev), cmdf));

	sc->sc_running &= ~cmdf;
	if (cmdf & SCP_UPD_MASK) {
		sc->sc_running &= ~SCP_UPDATESUBCMD;
		if (sc->sc_scheduled & SCP_UPD_MASK)
			ray_cmd_schedule(sc, sc->sc_scheduled & SCP_UPD_MASK);
	}
	if ((sc->sc_running & SCP_TIMOCHECK_CMD_MASK) == 0 && sc->sc_timocheck){
		callout_stop(&sc->sc_check_ccs_ch);
		sc->sc_timocheck = 0;
	}
}

/*
 * issue the command
 * only used for commands not tx
 */
static int
ray_issue_cmd(struct ray_softc *sc, bus_size_t ccs, u_int track)
{
	u_int i;

	RAY_DPRINTF(("%s: ray_cmd_issue 0x%x\n", device_xname(sc->sc_dev), track));

	/*
	 * XXX other drivers did this, but I think
	 * what we really want to do is just make sure we don't
	 * get here or that spinning is ok
	 */
	i = 0;
	while (!RAY_ECF_READY(sc))
		if (++i > 50) {
			ray_free_ccs(sc, ccs);
			if (track)
				ray_cmd_schedule(sc, track);
			return (0);
		}

	SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_GET_INDEX(ccs));
	RAY_ECF_START_CMD(sc);
	ray_cmd_ran(sc, track);

	return (1);
}

/*
 * send a simple command if we can
 */
static int
ray_simple_cmd(struct ray_softc *sc, u_int cmd, u_int track)
{
	bus_size_t ccs;

	return (ray_alloc_ccs(sc, &ccs, cmd, track) &&
	    ray_issue_cmd(sc, ccs, track));
}

/*
 * Functions based on CCS commands
 */

/*
 * run a update subcommand
 */
static void
ray_update_subcmd(struct ray_softc *sc)
{
	int submask, i;

	RAY_DPRINTF(("%s: ray_update_subcmd\n", device_xname(sc->sc_dev)));

	ray_cmd_cancel(sc, SCP_UPDATESUBCMD);
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	submask = SCP_UPD_FIRST;
	for (i = 0; i < ray_nsubcmdtab; submask <<= 1, i++) {
		if ((sc->sc_scheduled & SCP_UPD_MASK) == 0)
			break;
		/* when done the next command will be scheduled */
		if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD))
			break;
		if (!RAY_ECF_READY(sc))
			break;
		/*
		 * give priority to LSB -- e.g., if previous loop rescheduled
		 * doing this command after calling the function won't catch
		 * if a later command sets an earlier bit
		 */
		if (sc->sc_scheduled & ((submask - 1) & SCP_UPD_MASK))
			break;
		if (sc->sc_scheduled & submask)
			(*ray_subcmdtab[i])(sc);
	}
}

/*
 * report a parameter
 */
static void
ray_report_params(struct ray_softc *sc)
{
	bus_size_t ccs;

	ray_cmd_cancel(sc, SCP_REPORTPARAMS);

	if (!sc->sc_repreq)
		return;

	/* do the issue check before equality check */
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_REPORTPARAMS)) {
		ray_cmd_schedule(sc, SCP_REPORTPARAMS);
		return;
	} else if (!ray_alloc_ccs(sc, &ccs, RAY_CMD_REPORT_PARAMS,
	    SCP_REPORTPARAMS))
		return;

	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_report, c_paramid,
	    sc->sc_repreq->r_paramid);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_report, c_nparam, 1);
	(void)ray_issue_cmd(sc, ccs, SCP_REPORTPARAMS);
}

/*
 * start an association
 */
static void
ray_start_assoc(struct ray_softc *sc)
{
	ray_cmd_cancel(sc, SCP_STARTASSOC);
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_STARTASSOC))
		return;
	(void)ray_simple_cmd(sc, RAY_CMD_START_ASSOC, SCP_STARTASSOC);
}

/*
 * Subcommand functions that use the SCP_UPDATESUBCMD command
 * (and are serialized with respect to other update sub commands
 */

/*
 * download the startup parameters to the card
 *	-- no outstanding commands expected
 */
static void
ray_download_params(struct ray_softc *sc)
{
	struct ray_startup_params_head *sp;
	struct ray_startup_params_tail_5 *sp5;
	struct ray_startup_params_tail_4 *sp4;
	bus_size_t off;

	RAY_DPRINTF(("%s: init_startup_params\n", device_xname(sc->sc_dev)));

	ray_cmd_cancel(sc, SCP_UPD_STARTUP);

#define	PUT2(p, v) 	\
	do { (p)[0] = ((v >> 8) & 0xff); (p)[1] = (v & 0xff); } while(0)

	sp = &sc->sc_startup;
	sp4 = &sc->sc_startup_4;
	sp5 = &sc->sc_startup_5;
	memset(sp, 0, sizeof(*sp));
	if (sc->sc_version == SC_BUILD_4)
		memset(sp4, 0, sizeof(*sp4));
	else
		memset(sp5, 0, sizeof(*sp5));
	/* XXX: Raylink firmware doesn't have length field for ssid */
	memcpy(sp->sp_ssid, sc->sc_dnwid.i_nwid, sizeof(sp->sp_ssid));
	sp->sp_scan_mode = 0x1;
	memcpy(sp->sp_mac_addr, sc->sc_ecf_startup.e_station_addr,
	    ETHER_ADDR_LEN);
	PUT2(sp->sp_frag_thresh, 0x7fff);	/* disabled */
	if (sc->sc_version == SC_BUILD_4) {
#if 1
		/* linux/fbsd */
		PUT2(sp->sp_dwell_time, 0x200);
		PUT2(sp->sp_beacon_period, 1);
#else
		/* divined */
		PUT2(sp->sp_dwell_time, 0x400);
		PUT2(sp->sp_beacon_period, 0);
#endif
	} else {
		PUT2(sp->sp_dwell_time, 128);
		PUT2(sp->sp_beacon_period, 256);
	}
	sp->sp_dtim_interval = 1;
#if 0
	/* these are the documented defaults for build 5/6 */
	sp->sp_max_retry = 0x1f;
	sp->sp_ack_timo = 0x86;
	sp->sp_sifs = 0x1c;
#elif 1
	/* these were scrounged from the linux driver */
	sp->sp_max_retry = 0x07;

	sp->sp_ack_timo = 0xa3;
	sp->sp_sifs = 0x1d;
#else
	/* these were divined */
	sp->sp_max_retry = 0x03;

	sp->sp_ack_timo = 0xa3;
	sp->sp_sifs = 0x1d;
#endif
#if 0
	/* these are the documented defaults for build 5/6 */
	sp->sp_difs = 0x82;
	sp->sp_pifs = 0;
#else
	/* linux/fbsd */
	sp->sp_difs = 0x82;

	if (sc->sc_version == SC_BUILD_4)
		sp->sp_pifs = 0xce;
	else
		sp->sp_pifs = 0x4e;
#endif

	PUT2(sp->sp_rts_thresh, 0x7fff);	/* disabled */
	if (sc->sc_version == SC_BUILD_4) {
		PUT2(sp->sp_scan_dwell, 0xfb1e);
		PUT2(sp->sp_scan_max_dwell, 0xc75c);
	} else {
		PUT2(sp->sp_scan_dwell, 0x4e2);
		PUT2(sp->sp_scan_max_dwell, 0x38a4);
	}
	sp->sp_assoc_timo = 0x5;
	if (sc->sc_version == SC_BUILD_4) {
#if 0
		/* linux/fbsd */
		sp->sp_adhoc_scan_cycle = 0x4;
		sp->sp_infra_scan_cycle = 0x2;
		sp->sp_infra_super_scan_cycle = 0x4;
#else
		/* divined */
		sp->sp_adhoc_scan_cycle = 0x8;
		sp->sp_infra_scan_cycle = 0x1;
		sp->sp_infra_super_scan_cycle = 0x18;
#endif
	} else {
		sp->sp_adhoc_scan_cycle = 0x8;
		sp->sp_infra_scan_cycle = 0x2;
		sp->sp_infra_super_scan_cycle = 0x8;
	}
	sp->sp_promisc = sc->sc_promisc;
	PUT2(sp->sp_uniq_word, 0x0cbd);
	if (sc->sc_version == SC_BUILD_4) {
	/* XXX what is this value anyway..? the std says 50us */
		/* XXX sp->sp_slot_time = 0x4e; */
		sp->sp_slot_time = 0x4e;
#if 1
		/*linux/fbsd*/
		sp->sp_roam_low_snr_thresh = 0xff;
#else
		/*divined*/
		sp->sp_roam_low_snr_thresh = 0x30;
#endif
	} else {
		sp->sp_slot_time = 0x32;
		sp->sp_roam_low_snr_thresh = 0xff;	/* disabled */
	}
#if 1
	sp->sp_low_snr_count = 0xff;		/* disabled */
#else
	/* divined -- check */
	sp->sp_low_snr_count = 0x07;		/* disabled */
#endif
#if 0
	sp->sp_infra_missed_beacon_count = 0x2;
#elif 1
	/* linux/fbsd */
	sp->sp_infra_missed_beacon_count = 0x5;
#else
	/* divined -- check, looks fishy */
	sp->sp_infra_missed_beacon_count = 0x7;
#endif
	sp->sp_adhoc_missed_beacon_count = 0xff;
	sp->sp_country_code = sc->sc_dcountrycode;
	sp->sp_hop_seq = 0x0b;
	if (sc->sc_version == SC_BUILD_4) {
		sp->sp_hop_seq_len = 0x4e;
		sp4->sp_cw_max = 0x3f;	/* single byte on build 4 */
		sp4->sp_cw_min = 0x0f;	/* single byte on build 4 */
		sp4->sp_noise_filter_gain = 0x4;
		sp4->sp_noise_limit_offset = 0x8;
		sp4->sp_rssi_thresh_offset = 0x28;
		sp4->sp_busy_thresh_offset = 0x28;
		sp4->sp_sync_thresh = 0x07;
		sp4->sp_test_mode = 0x0;
		sp4->sp_test_min_chan = 0x2;
		sp4->sp_test_max_chan = 0x2;
	} else {
		sp->sp_hop_seq_len = 0x4f;
		PUT2(sp5->sp_cw_max, 0x3f);
		PUT2(sp5->sp_cw_min, 0x0f);
		sp5->sp_noise_filter_gain = 0x4;
		sp5->sp_noise_limit_offset = 0x8;
		sp5->sp_rssi_thresh_offset = 0x28;
		sp5->sp_busy_thresh_offset = 0x28;
		sp5->sp_sync_thresh = 0x07;
		sp5->sp_test_mode = 0x0;
		sp5->sp_test_min_chan = 0x2;
		sp5->sp_test_max_chan = 0x2;
#if 0
		sp5->sp_allow_probe_resp = 0x1;
#else
		sp5->sp_allow_probe_resp = 0x0;
#endif
		sp5->sp_privacy_must_start = 0x0;
		sp5->sp_privacy_can_join = 0x0;
		sp5->sp_basic_rate_set[0] = 0x2;
		    /* 2 = 1Mbps, 3 = old 2Mbps 4 = 2Mbps */
	}

	/* we shouldn't be called with some command pending */
	if (!RAY_ECF_READY(sc))
		panic("ray_download_params busy");

	/* write the compatible part */
	off = RAY_HOST_TO_ECF_BASE;
	ray_write_region(sc, off, sp, sizeof(sc->sc_startup));
	off += sizeof(sc->sc_startup);
	if (sc->sc_version == SC_BUILD_4)
		ray_write_region(sc, off, sp4, sizeof(*sp4));
	else
		ray_write_region(sc, off, sp5, sizeof(*sp5));
	if (!ray_simple_cmd(sc, RAY_CMD_START_PARAMS, SCP_UPD_STARTUP))
		panic("ray_download_params issue");
}

/*
 * start or join a network
 */
static void
ray_start_join_net(struct ray_softc *sc)
{
	struct ray_net_params np;
	bus_size_t ccs;
	int cmd;

	ray_cmd_cancel(sc, SCP_UPD_STARTJOIN);
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;

	/* XXX check we may not want to re-issue */
	if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		ray_cmd_schedule(sc, SCP_UPD_STARTJOIN);
		return;
	}

	if (sc->sc_mode == SC_MODE_ADHOC)
		cmd = RAY_CMD_START_NET;
	else
		cmd = RAY_CMD_JOIN_NET;

	if (!ray_alloc_ccs(sc, &ccs, cmd, SCP_UPD_STARTJOIN))
		return;
	sc->sc_startccs = ccs;
	sc->sc_startcmd = cmd;
	if (!memcmp(&sc->sc_cnwid, &sc->sc_dnwid, sizeof(sc->sc_cnwid))
	    && sc->sc_omode == sc->sc_mode)
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_net, c_upd_param, 0);
	else {
		sc->sc_havenet = 0;
		memset(&np, 0, sizeof(np));
		np.p_net_type = sc->sc_mode;
		memcpy(np.p_ssid, sc->sc_dnwid.i_nwid, sizeof(np.p_ssid));
		ray_write_region(sc, RAY_HOST_TO_ECF_BASE, &np, sizeof(np));
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_net, c_upd_param, 1);
	}
	if (ray_issue_cmd(sc, ccs, SCP_UPD_STARTJOIN))
		callout_reset(&sc->sc_start_join_timo_ch, RAY_START_TIMEOUT,
		    ray_start_join_timo, sc);
}

static void
ray_start_join_timo(void *arg)
{
	struct ray_softc *sc;
	u_int stat;

	sc = arg;
	stat = SRAM_READ_FIELD_1(sc, sc->sc_startccs, ray_cmd, c_status);
	ray_start_join_net_done(sc, sc->sc_startcmd, sc->sc_startccs, stat);
}

/*
 * The start/join has completed.  Note: we timeout the start
 * command because it seems to fail to work at least on the
 * build 4 firmware without reporting an error.  This actually
 * may be a result of not putting the correct params in the
 * initial download.  If this is a timeout `stat' will be
 * marked busy.
 */
static ray_cmd_func_t
ray_start_join_net_done(struct ray_softc *sc, u_int cmd, bus_size_t ccs, u_int stat)
{
	int i;
	struct ray_net_params np;

	callout_stop(&sc->sc_start_join_timo_ch);
	ray_cmd_done(sc, SCP_UPD_STARTJOIN);

	if (stat == RAY_CCS_STATUS_FAIL) {
		/* XXX poke ifmedia when it supports this */
		sc->sc_havenet = 0;
		return (ray_start_join_net);
	}
	if (stat == RAY_CCS_STATUS_BUSY || stat == RAY_CCS_STATUS_FREE) {
		/* handle the timeout condition */
		callout_reset(&sc->sc_start_join_timo_ch, RAY_START_TIMEOUT,
		    ray_start_join_timo, sc);

		/* be safe -- not a lot occurs with no net though */
		if (!RAY_ECF_READY(sc))
			return (0);

		/* see if our nwid is up to date */
		if (!memcmp(&sc->sc_cnwid, &sc->sc_dnwid, sizeof(sc->sc_cnwid))
		    && sc->sc_omode == sc->sc_mode)
			SRAM_WRITE_FIELD_1(sc,ccs, ray_cmd_net, c_upd_param, 0);
		else {
			memset(&np, 0, sizeof(np));
			np.p_net_type = sc->sc_mode;
			memcpy(np.p_ssid, sc->sc_dnwid.i_nwid,
			    sizeof(np.p_ssid));
			ray_write_region(sc, RAY_HOST_TO_ECF_BASE, &np,
			    sizeof(np));
			SRAM_WRITE_FIELD_1(sc,ccs, ray_cmd_net, c_upd_param, 1);
		}

		if (sc->sc_mode == SC_MODE_ADHOC)
			cmd = RAY_CMD_START_NET;
		else
			cmd = RAY_CMD_JOIN_NET;
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_net, c_cmd,
		    RAY_CCS_STATUS_BUSY);
		SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_net, c_status,
		    RAY_CCS_STATUS_BUSY);

		/* we simply poke the card again issuing the same ccs */
		SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_GET_INDEX(ccs));
		RAY_ECF_START_CMD(sc);
		ray_cmd_ran(sc, SCP_UPD_STARTJOIN);
		return (0);
	}
	/* get the current ssid */
	SRAM_READ_FIELD_N(sc, ccs, ray_cmd_net, c_bss_id, sc->sc_bssid,
	    sizeof(sc->sc_bssid));

	sc->sc_deftxrate = SRAM_READ_FIELD_1(sc, ccs, ray_cmd_net,c_def_txrate);
	sc->sc_encrypt = SRAM_READ_FIELD_1(sc, ccs, ray_cmd_net, c_encrypt);

	/* adjust values for buggy build 4 */
	if (sc->sc_deftxrate == 0x55)
		sc->sc_deftxrate = RAY_PID_BASIC_RATE_1500K;
	if (sc->sc_encrypt == 0x55)
		sc->sc_encrypt = 0;

	if (SRAM_READ_FIELD_1(sc, ccs, ray_cmd_net, c_upd_param)) {
		ray_read_region(sc, RAY_HOST_TO_ECF_BASE, &np, sizeof(np));
		/* XXX: Raylink firmware doesn't have length field for ssid */
		for (i = 0; i < sizeof(np.p_ssid); i++) {
			if (np.p_ssid[i] == '\0')
				break;
		}
		sc->sc_cnwid.i_len = i;
		memcpy(sc->sc_cnwid.i_nwid, np.p_ssid, i);
		sc->sc_omode = sc->sc_mode;
		if (np.p_net_type != sc->sc_mode)
			return (ray_start_join_net);
	}
	RAY_DPRINTF(("%s: net start/join nwid %.32s bssid %s inited %d\n",
	    device_xname(sc->sc_dev), sc->sc_cnwid.i_nwid, ether_sprintf(sc->sc_bssid),
		SRAM_READ_FIELD_1(sc, ccs, ray_cmd_net, c_inited)));

	/* network is now active */
	ray_cmd_schedule(sc, SCP_UPD_MCAST|SCP_UPD_PROMISC);
	if (cmd == RAY_CMD_JOIN_NET)
		return (ray_start_assoc);
	else {
		sc->sc_havenet = 1;
		return (ray_intr_start);
	}
}

/*
 * set the card in/out of promiscuous mode
 */
static void
ray_update_promisc(struct ray_softc *sc)
{
	bus_size_t ccs;
	int promisc;

	ray_cmd_cancel(sc, SCP_UPD_PROMISC);

	/* do the issue check before equality check */
	if (sc->sc_if.if_flags & IFF_ALLMULTI)
		sc->sc_if.if_flags |= IFF_PROMISC;
	else if (sc->sc_if.if_pcount == 0)
		sc->sc_if.if_flags &= ~IFF_PROMISC;
	promisc = !!(sc->sc_if.if_flags & IFF_PROMISC);
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		ray_cmd_schedule(sc, SCP_UPD_PROMISC);
		return;
	} else if (promisc == sc->sc_promisc)
		return;
	else if (!ray_alloc_ccs(sc,&ccs,RAY_CMD_UPDATE_PARAMS, SCP_UPD_PROMISC))
		return;
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_paramid, RAY_PID_PROMISC);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_nparam, 1);
	SRAM_WRITE_1(sc, RAY_HOST_TO_ECF_BASE, promisc);
	(void)ray_issue_cmd(sc, ccs, SCP_UPD_PROMISC);
}

/*
 * update the parameter based on what the user passed in
 */
static void
ray_update_params(struct ray_softc *sc)
{
	bus_size_t ccs;

	ray_cmd_cancel(sc, SCP_UPD_UPDATEPARAMS);
	if (!sc->sc_updreq) {
		/* XXX do we need to wakeup here? */
		return;
	}

	/* do the issue check before equality check */
	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		ray_cmd_schedule(sc, SCP_UPD_UPDATEPARAMS);
		return;
	} else if (!ray_alloc_ccs(sc, &ccs, RAY_CMD_UPDATE_PARAMS,
	    SCP_UPD_UPDATEPARAMS))
		return;

	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_paramid,
	    sc->sc_updreq->r_paramid);
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update, c_nparam, 1);
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE, sc->sc_updreq->r_data,
	    sc->sc_updreq->r_len);

	(void)ray_issue_cmd(sc, ccs, SCP_UPD_UPDATEPARAMS);
}

/*
 * set the multicast filter list
 */
static void
ray_update_mcast(struct ray_softc *sc)
{
	bus_size_t ccs;
	struct ether_multistep step;
	struct ether_multi *enm;
	struct ethercom *ec;
	bus_size_t bufp;
	int count;

	ec = &sc->sc_ec;
	ray_cmd_cancel(sc, SCP_UPD_MCAST);

	/* see if we have any ranges */
	if ((count = sc->sc_ec.ec_multicnt) < 17) {
		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm) {
			/* see if this is a range */
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
				ETHER_ADDR_LEN)) {
				count = 17;
				break;
			}
			ETHER_NEXT_MULTI(step, enm);
		}
	}

	/* track this stuff even when not running */
	if (count > 16) {
		sc->sc_if.if_flags |= IFF_ALLMULTI;
		ray_update_promisc(sc);
		return;
	} else if (sc->sc_if.if_flags & IFF_ALLMULTI) {
		sc->sc_if.if_flags &= ~IFF_ALLMULTI;
		ray_update_promisc(sc);
	}

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	else if (ray_cmd_is_running(sc, SCP_UPDATESUBCMD)) {
		ray_cmd_schedule(sc, SCP_UPD_MCAST);
		return;
	} else if (!ray_alloc_ccs(sc,&ccs, RAY_CMD_UPDATE_MCAST, SCP_UPD_MCAST))
		return;
	SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd_update_mcast, c_nmcast, count);
	bufp = RAY_HOST_TO_ECF_BASE;
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm) {
		ray_write_region(sc, bufp, enm->enm_addrlo, ETHER_ADDR_LEN);
		bufp += ETHER_ADDR_LEN;
		ETHER_NEXT_MULTI(step, enm);
	}
	(void)ray_issue_cmd(sc, ccs, SCP_UPD_MCAST);
}

/*
 * User issued commands
 */

/*
 * issue an "update params"
 *
 * expected to be called in sleepable context -- intended for user stuff
 */
static int
ray_user_update_params(struct ray_softc *sc, struct ray_param_req *pr)
{
	int rv;

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO);
	}

	/* wait to be able to issue the command */
	rv = 0;
	while (ray_cmd_is_running(sc, SCP_UPD_UPDATEPARAMS) ||
	    ray_cmd_is_scheduled(sc, SCP_UPD_UPDATEPARAMS)) {
		rv = tsleep(ray_update_params, 0|PCATCH, "cmd in use", 0);
		if (rv)
			return (rv);
		if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
			pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
			return (EIO);
		}
	}

	pr->r_failcause = RAY_FAILCAUSE_WAITING;
	sc->sc_updreq = pr;
	ray_cmd_schedule(sc, SCP_UPD_UPDATEPARAMS);
	ray_check_scheduled(sc);

	while (pr->r_failcause == RAY_FAILCAUSE_WAITING)
		(void)tsleep(ray_update_params, 0, "waiting cmd", 0);
	wakeup(ray_update_params);

	return (0);
}

/*
 * issue a report params
 *
 * expected to be called in sleepable context -- intended for user stuff
 */
static int
ray_user_report_params(struct ray_softc *sc, struct ray_param_req *pr)
{
	int rv;

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
		pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
		return (EIO);
	}

	/* wait to be able to issue the command */
	rv = 0;
	while (ray_cmd_is_running(sc, SCP_REPORTPARAMS)
	    || ray_cmd_is_scheduled(sc, SCP_REPORTPARAMS)) {
		rv = tsleep(ray_report_params, 0|PCATCH, "cmd in use", 0);
		if (rv)
			return (rv);
		if ((sc->sc_if.if_flags & IFF_RUNNING) == 0) {
			pr->r_failcause = RAY_FAILCAUSE_EDEVSTOP;
			return (EIO);
		}
	}

	pr->r_failcause = RAY_FAILCAUSE_WAITING;
	sc->sc_repreq = pr;
	ray_cmd_schedule(sc, SCP_REPORTPARAMS);
	ray_check_scheduled(sc);

	while (pr->r_failcause == RAY_FAILCAUSE_WAITING)
		(void)tsleep(ray_report_params, 0, "waiting cmd", 0);
	wakeup(ray_report_params);

	return (0);
}


/*
 * this is a temporary wrapper around bus_space_read_region_1
 * as it seems to mess with gcc.  the line numbers get offset
 * presumably this is related to the inline asm on i386.
 */

static void
ray_read_region(struct ray_softc *sc, bus_size_t off, void *vp, size_t c)
{
#ifdef RAY_USE_OPTIMIZED_COPY
	u_int n2, n4, tmp;
	u_int8_t *p;

	p = vp;

	/* XXX we may be making poor assumptions here but lets hope */
	switch ((off|(bus_addr_t)p) & 0x03) {
	case 0:
		if ((n4 = c / 4)) {
			bus_space_read_region_4(sc->sc_memt, sc->sc_memh, off,
			    p, n4);
			tmp = c & ~0x3;
			c &= 0x3;
			p += tmp;
			off += tmp;
		}
		switch (c) {
		case 3:
			*p = bus_space_read_1(sc->sc_memt,sc->sc_memh, off);
			p++, off++;
		case 2:
			*p = bus_space_read_1(sc->sc_memt,sc->sc_memh, off);
			p++, off++;
		case 1:
			*p = bus_space_read_1(sc->sc_memt,sc->sc_memh, off);
		}
		break;
	case 2:
		if ((n2 = (c >> 1)))
			bus_space_read_region_2(sc->sc_memt, sc->sc_memh, off,
			    p, n2);
		if (c & 1) {
			c &= ~0x1;
			*(p + c) = bus_space_read_1(sc->sc_memt, sc->sc_memh,
			    off + c);
		}
		break;
	case 1:
	case 3:
		bus_space_read_region_1(sc->sc_memt, sc->sc_memh, off, p, c);
		break;
	}
#else
	bus_space_read_region_1(sc->sc_memt, sc->sc_memh, off, vp, c);
#endif
}

/*
 * this is a temporary wrapper around bus_space_write_region_1
 * as it seems to mess with gcc.  the line numbers get offset
 * presumably this is related to the inline asm on i386.
 */
static void
ray_write_region(struct ray_softc *sc, bus_size_t off, void *vp, size_t c)
{
#ifdef RAY_USE_OPTIMIZED_COPY
	size_t n2, n4, tmp;
	u_int8_t *p;

	p = vp;
	/* XXX we may be making poor assumptions here but lets hope */
	switch ((off|(bus_addr_t)p) & 0x03) {
	case 0:
		if ((n4 = (c >> 2))) {
			bus_space_write_region_4(sc->sc_memt, sc->sc_memh, off,
			    p, n4);
			tmp = c & ~0x3;
			c &= 0x3;
			p += tmp;
			off += tmp;
		}
		switch (c) {
		case 3:
			bus_space_write_1(sc->sc_memt,sc->sc_memh, off, *p);
			p++, off++;
		case 2:
			bus_space_write_1(sc->sc_memt,sc->sc_memh, off, *p);
			p++, off++;
		case 1:
			bus_space_write_1(sc->sc_memt,sc->sc_memh, off, *p);
		}
		break;
	case 2:
		if ((n2 = (c >> 1)))
			bus_space_write_region_2(sc->sc_memt, sc->sc_memh, off,
			    p, n2);
		if (c & 0x1) {
			c &= ~0x1;
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    off + c, *(p + c));
		}
		break;
	case 1:
	case 3:
		bus_space_write_region_1(sc->sc_memt, sc->sc_memh, off, p, c);
		break;
	}
#else
	bus_space_write_region_1(sc->sc_memt, sc->sc_memh, off, vp, c);
#endif
}

#ifdef RAY_DEBUG

#define PRINTABLE(c) ((c) >= 0x20 && (c) <= 0x7f)

void
hexdump(const u_int8_t *d, int len, int br, int div, int fl)
{
	int i, j, offw, first, tlen, ni, nj, sp;

	sp = br / div;
	offw = 0;
	if (len && (fl & HEXDF_NOOFFSET) == 0) {
		tlen = len;
		do {
			offw++;
		} while (tlen /= br);
	}
	if (offw)
		printf("%0*x: ", offw, 0);
	for (i = 0; i < len; i++, d++) {
		if (i && (i % br) == 0) {
			if ((fl & HEXDF_NOASCII) == 0) {
				printf("   ");
				d -= br;
				for (j = 0; j < br; d++, j++) {
					if (j && (j % sp) == 0)
						printf(" ");
					if (PRINTABLE(*d))
						printf("%c", (int)*d);
					else
						printf(".");
				}
			}
			if (offw)
				printf("\n%0*x: ", offw, i);
			else
				printf("\n");
			if ((fl & HEXDF_NOCOMPRESS) == 0) {
				first = 1;
				while (len - i >= br) {
					if (memcmp(d, d - br, br))
						break;
					d += br;
					i += br;
					if (first) {
						printf("*");
						first = 0;
					}
				}
				if (len == i) {
					printf("\n%0*x", offw, i);
					return;
				}
			}
		} else if (i && (i % sp) == 0)
			printf(" ");
		printf("%02x ", *d);
	}
	if (len && (((i - 1) % br) || i == 1)) {
		if ((fl & HEXDF_NOASCII) == 0) {
			i = i % br ? i % br : br;
			ni = (br - i) % br;
			j = (i - 1) / sp;
			nj = (div - j - 1) % div;
			j = 3 * ni + nj + 3;
			printf("%*s", j, "");
			d -= i;
			for (j = 0; j < i; d++, j++) {
				if (j && (j % sp) == 0)
					printf(" ");
				if (PRINTABLE(*d))
					printf("%c", (int)*d);
				else
					printf(".");
			}
		}
		printf("\n");
	}
}



static void
ray_dump_mbuf(struct ray_softc *sc, struct mbuf *m)
{
	u_int8_t *d, *ed;
	u_int i;

	printf("%s: pkt dump:", device_xname(sc->sc_dev));
	i = 0;
	for (; m; m = m->m_next) {
		d = mtod(m, u_int8_t *);
		ed = d + m->m_len;

		for (; d < ed; i++, d++) {
			if ((i % 16) == 0)
				printf("\n\t");
			else if ((i % 8) == 0)
				printf("  ");
			printf(" %02x", *d);
		}
	}
	if ((i - 1) % 16)
		printf("\n");
}
#endif	/* RAY_DEBUG */

#ifdef RAY_DO_SIGLEV
static void
ray_update_siglev(struct ray_softc *sc, u_int8_t *src, u_int8_t siglev)
{
	int i, mini;
	struct timeval mint;
	struct ray_siglev *sl;

	/* try to find host */
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (memcmp(sl->rsl_host, src, ETHER_ADDR_LEN) == 0)
			goto found;
	}
	/* not found, find oldest slot */
	mini = 0;
	mint.tv_sec = LONG_MAX;
	mint.tv_usec = 0;
	for (i = 0; i < RAY_NSIGLEVRECS; i++) {
		sl = &sc->sc_siglevs[i];
		if (timercmp(&sl->rsl_time, &mint, <)) {
			mini = i;
			mint = sl->rsl_time;
		}
	}
	sl = &sc->sc_siglevs[mini];
	memset(sl->rsl_siglevs, 0, RAY_NSIGLEV);
	memcpy(sl->rsl_host, src, ETHER_ADDR_LEN);

 found:
	microtime(&sl->rsl_time);
	memmove(&sl->rsl_siglevs[1], sl->rsl_siglevs, RAY_NSIGLEV-1);
	sl->rsl_siglevs[0] = siglev;
}
#endif

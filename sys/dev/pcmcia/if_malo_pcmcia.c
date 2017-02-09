/*	$NetBSD: if_malo_pcmcia.c,v 1.7 2014/05/12 02:26:19 christos Exp $	*/
/*      $OpenBSD: if_malo.c,v 1.65 2009/03/29 21:53:53 sthen Exp $ */

/*
 * Copyright (c) 2007 Marcus Glocker <mglocker@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_malo_pcmcia.c,v 1.7 2014/05/12 02:26:19 christos Exp $");

#ifdef _MODULE
#include <sys/module.h>
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/pmf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/pcmcia/if_malo_pcmciavar.h>
#include <dev/pcmcia/if_malo_pcmciareg.h>

/*
 * Driver for the Marvell 88W8385 chip (Compact Flash).
 */

#ifdef CMALO_DEBUG
int cmalo_d = 1;
#define DPRINTF(l, x...)	do { if ((l) <= cmalo_d) printf(x); } while (0)
#else
#define DPRINTF(l, x...)	do {} while (0)
#endif

static int	malo_pcmcia_match(device_t, cfdata_t, void *);
static void	malo_pcmcia_attach(device_t, device_t, void *);
static int	malo_pcmcia_detach(device_t, int);
static int	malo_pcmcia_activate(device_t, devact_t);

static int	malo_pcmcia_validate_config(struct pcmcia_config_entry *);

static int	malo_pcmcia_enable(struct malo_softc *);
static void	malo_pcmcia_disable(struct malo_softc *);

static void	cmalo_attach(void *);
static void	cmalo_detach(void *);
static int	cmalo_intr(void *);

static void	cmalo_start(struct ifnet *);
static int	cmalo_ioctl(struct ifnet *, u_long, void *);
static int	cmalo_init(struct ifnet *);
static void	cmalo_watchdog(struct ifnet *);
static int	cmalo_media_change(struct ifnet *);
static int	cmalo_newstate(struct ieee80211com *, enum ieee80211_state,
			       int);

static int	firmware_load(const char *, const char *, uint8_t **, size_t *);
static int	cmalo_fw_alloc(struct malo_softc *);
static void	cmalo_fw_free(struct malo_softc *);
static int	cmalo_fw_load_helper(struct malo_softc *);
static int	cmalo_fw_load_main(struct malo_softc *);

static void	cmalo_stop(struct malo_softc *);
static void	cmalo_intr_mask(struct malo_softc *, int);
static void	cmalo_rx(struct malo_softc *);
static int	cmalo_tx(struct malo_softc *, struct mbuf *);
static void	cmalo_tx_done(struct malo_softc *);
static void	cmalo_event(struct malo_softc *);
static void	cmalo_select_network(struct malo_softc *);
static void	cmalo_reflect_network(struct malo_softc *);
static int	cmalo_wep(struct malo_softc *);
static int	cmalo_rate2bitmap(int);

static void	cmalo_hexdump(void *, int);
static int	cmalo_cmd_get_hwspec(struct malo_softc *);
static int	cmalo_cmd_rsp_hwspec(struct malo_softc *);
static int	cmalo_cmd_set_reset(struct malo_softc *);
static int	cmalo_cmd_set_scan(struct malo_softc *);
static int	cmalo_cmd_rsp_scan(struct malo_softc *);
static int	cmalo_parse_elements(struct malo_softc *, uint8_t *, int, int);
static int	cmalo_cmd_set_auth(struct malo_softc *);
static int	cmalo_cmd_set_wep(struct malo_softc *, uint16_t,
		    struct ieee80211_key *);
static int	cmalo_cmd_set_snmp(struct malo_softc *, uint16_t);
static int	cmalo_cmd_set_radio(struct malo_softc *, uint16_t);
static int	cmalo_cmd_set_channel(struct malo_softc *, uint16_t);
static int	cmalo_cmd_set_txpower(struct malo_softc *, int16_t);
static int	cmalo_cmd_set_antenna(struct malo_softc *, uint16_t);
static int	cmalo_cmd_set_macctrl(struct malo_softc *);
static int	cmalo_cmd_set_macaddr(struct malo_softc *, uint8_t *);
static int	cmalo_cmd_set_assoc(struct malo_softc *);
static int	cmalo_cmd_rsp_assoc(struct malo_softc *);
static int	cmalo_cmd_set_rate(struct malo_softc *, int);
static int	cmalo_cmd_request(struct malo_softc *, uint16_t, int);
static int	cmalo_cmd_response(struct malo_softc *);

/*
 * PCMCIA bus.
 */
struct malo_pcmcia_softc {
	struct malo_softc	 sc_malo;

	struct pcmcia_function	*sc_pf;
	struct pcmcia_io_handle	 sc_pcioh;
	int			 sc_io_window;
	void			*sc_ih;
};

CFATTACH_DECL_NEW(malo_pcmcia, sizeof(struct malo_pcmcia_softc),
	malo_pcmcia_match, malo_pcmcia_attach, malo_pcmcia_detach,
	malo_pcmcia_activate);


static int
malo_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_AMBICOM &&
	    pa->product == PCMCIA_PRODUCT_AMBICOM_WL54CF)
		return 1;

	return 0;
}

static void
malo_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct malo_pcmcia_softc *psc = device_private(self);
	struct malo_softc *sc = &psc->sc_malo;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int error;

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

        error = pcmcia_function_configure(pa->pf, malo_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	malo_pcmcia_enable(sc);

	cfe = pa->pf->cfe;
	sc->sc_iot = cfe->iospace[0].handle.iot;
	sc->sc_ioh = cfe->iospace[0].handle.ioh;

	cmalo_attach(sc);
	if (!(sc->sc_flags & MALO_DEVICE_ATTACHED))
		goto fail;

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, &sc->sc_if);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

fail:
	malo_pcmcia_disable(sc);

	if (sc->sc_flags & MALO_DEVICE_ATTACHED)
		return;

	pcmcia_function_unconfigure(pa->pf);
	return;
}

static int
malo_pcmcia_detach(device_t dev, int flags)
{
	struct malo_pcmcia_softc *psc = device_private(dev);
	struct malo_softc *sc = &psc->sc_malo;

	cmalo_detach(sc);
	malo_pcmcia_disable(sc);
	pcmcia_function_unconfigure(psc->sc_pf);

	return 0;
}

static int
malo_pcmcia_activate(device_t dev, devact_t act)
{
	struct malo_pcmcia_softc *psc = device_private(dev);
	struct malo_softc *sc = &psc->sc_malo;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	s = splnet();
	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(ifp);
		break;
	default:
		return EOPNOTSUPP;
	}
	splx(s);

	return 0;
}


int
malo_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{

	if (cfe->iftype != PCMCIA_IFTYPE_IO || cfe->num_iospace != 1)
		return EINVAL;
	/* Some cards have a memory space, but we don't use it. */
	cfe->num_memspace = 0;
	return 0;
}


static int
malo_pcmcia_enable(struct malo_softc *sc)
{
	struct malo_pcmcia_softc *psc = (struct malo_pcmcia_softc *)sc;

	/* establish interrupt */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, cmalo_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error(": can't establish interrupt\n");
		return -1;
	}

	if (pcmcia_function_enable(psc->sc_pf)) {
		aprint_error(": can't enable function\n");
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		return -1;
	}
	sc->sc_flags |= MALO_DEVICE_ENABLED;

	return 0;
}

static void
malo_pcmcia_disable(struct malo_softc *sc)
{
	struct malo_pcmcia_softc *psc = (struct malo_pcmcia_softc *)sc;

	pcmcia_function_disable(psc->sc_pf);
	if (psc->sc_ih)
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = NULL;
	sc->sc_flags &= ~MALO_DEVICE_ENABLED;
}


/*
 * Driver.
 */
static void
cmalo_attach(void *arg)
{
	struct malo_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int i;

	/* disable interrupts */
	cmalo_intr_mask(sc, 0);

	/* load firmware */
	if (cmalo_fw_alloc(sc) != 0 ||
	    cmalo_fw_load_helper(sc) != 0 ||
	    cmalo_fw_load_main(sc) != 0) {
		/* free firmware */
		cmalo_fw_free(sc);
		return;
	}
	sc->sc_flags |= MALO_FW_LOADED;

	/* allocate command buffer */
	sc->sc_cmd = malloc(MALO_CMD_BUFFER_SIZE, M_DEVBUF, M_NOWAIT);

	/* allocate data buffer */
	sc->sc_data = malloc(MALO_DATA_BUFFER_SIZE, M_DEVBUF, M_NOWAIT);

	/* enable interrupts */
	cmalo_intr_mask(sc, 1);

	/* we are context save here for FW commands */
	sc->sc_cmd_ctxsave = 1;

	mutex_init(&sc->sc_mtx, MUTEX_DEFAULT, IPL_VM);
	cv_init(&sc->sc_cv, "malo");

	/* get hardware specs */
	cmalo_cmd_get_hwspec(sc);

	/* setup interface */
	ifp->if_softc = sc;
	ifp->if_start = cmalo_start;
	ifp->if_ioctl = cmalo_ioctl;
	ifp->if_init = cmalo_init;
	ifp->if_watchdog = cmalo_watchdog;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST;
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;
	ic->ic_caps = IEEE80211_C_MONITOR | IEEE80211_C_WEP;

	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	for (i = 0; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	/* attach interface */
	if_attach(ifp);
	ieee80211_ifattach(ic);

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = cmalo_newstate;
	ieee80211_media_init(ic, cmalo_media_change, ieee80211_media_status);

	/* second attach line */
	aprint_normal_dev(sc->sc_dev, "address %s\n",
	    ether_sprintf(ic->ic_myaddr));

	ieee80211_announce(ic);

	/* device attached */
	sc->sc_flags |= MALO_DEVICE_ATTACHED;
}

static void
cmalo_detach(void *arg)
{
	struct malo_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;

	if (!(sc->sc_flags & MALO_DEVICE_ATTACHED)) {
		/* free firmware */
		cmalo_fw_free(sc);

		/* device was not properly attached */
		return;
	}

	if (ifp->if_flags & IFF_RUNNING)
		cmalo_stop(sc);

	/* free command buffer */
	if (sc->sc_cmd != NULL)
		free(sc->sc_cmd, M_DEVBUF);

	/* free data buffer */
	if (sc->sc_data != NULL)
		free(sc->sc_data, M_DEVBUF);

	/* free firmware */
	cmalo_fw_free(sc);

	/* detach inferface */
	ieee80211_ifdetach(ic);
	if_detach(ifp);

	mutex_destroy(&sc->sc_mtx);
	cv_destroy(&sc->sc_cv);
}

static int
cmalo_intr(void *arg)
{
	struct malo_softc *sc = arg;
	uint16_t intr = 0;

	/* read interrupt reason */
	intr = MALO_READ_2(sc, MALO_REG_HOST_INTR_CAUSE);
	if (intr == 0)
		/* interrupt not for us */
		return 0;
	if (intr == 0xffff)
		/* card has been detached */
		return 0;

	/* disable interrupts */
	cmalo_intr_mask(sc, 0);

	/* acknowledge interrupt */
	MALO_WRITE_2(sc, MALO_REG_HOST_INTR_CAUSE,
	    intr & MALO_VAL_HOST_INTR_MASK_ON);

	/* enable interrupts */
	cmalo_intr_mask(sc, 1);

	DPRINTF(2, "%s: interrupt handler called (intr = 0x%04x)\n",
	    device_xname(sc->sc_dev), intr);

	if (intr & MALO_VAL_HOST_INTR_TX)
		/* TX frame sent */
		cmalo_tx_done(sc);
	if (intr & MALO_VAL_HOST_INTR_RX)
		/* RX frame received */
		cmalo_rx(sc);
	if (intr & MALO_VAL_HOST_INTR_CMD) {
		/* command response */
		mutex_enter(&sc->sc_mtx);
		cv_signal(&sc->sc_cv);
		mutex_exit(&sc->sc_mtx);
		if (!sc->sc_cmd_ctxsave)
			cmalo_cmd_response(sc);
	}
	if (intr & MALO_VAL_HOST_INTR_EVENT)
		/* event */
		cmalo_event(sc);

	return 1;
}


/*
 * Network functions
 */
static void
cmalo_start(struct ifnet *ifp)
{
	struct malo_softc *sc = ifp->if_softc;
	struct mbuf *m;

	/* don't transmit packets if interface is busy or down */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	IFQ_POLL(&ifp->if_snd, m);
	if (m == NULL)
		return;

	IFQ_DEQUEUE(&ifp->if_snd, m);

	if (ifp->if_bpf)
		bpf_ops->bpf_mtap(ifp->if_bpf, m);

	if (cmalo_tx(sc, m) != 0)
		ifp->if_oerrors++;
}

static int
cmalo_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct malo_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_RUNNING:
			cmalo_stop(sc);
			break;

		case IFF_UP:
			cmalo_init(ifp);
			break;

		default:
			break;
		}
		error = 0;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET)
			/* setup multicast filter, etc */
			error = 0;
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			cmalo_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

static int
cmalo_init(struct ifnet *ifp)
{
	struct malo_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (!(sc->sc_flags & MALO_DEVICE_ENABLED))
		malo_pcmcia_enable(sc);

	/* reload the firmware if necessary */
	if (!(sc->sc_flags & MALO_FW_LOADED)) {
		/* disable interrupts */
		cmalo_intr_mask(sc, 0);

		/* load firmware */
		if (cmalo_fw_load_helper(sc) != 0)
			return EIO;
		if (cmalo_fw_load_main(sc) != 0)
			return EIO;
		sc->sc_flags |= MALO_FW_LOADED;

		/* enable interrupts */
		cmalo_intr_mask(sc, 1);
	}

	if (ifp->if_flags & IFF_RUNNING)
		cmalo_stop(sc);

	/* reset association state flag */
	sc->sc_flags &= ~MALO_ASSOC_FAILED;

	/* get current channel */
	ic->ic_curchan = ic->ic_ibss_chan;
	sc->sc_curchan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	DPRINTF(1, "%s: current channel is %d\n",
	    device_xname(sc->sc_dev), sc->sc_curchan);

	/* setup device */
	if (cmalo_cmd_set_macctrl(sc) != 0)
		return EIO;
	if (cmalo_cmd_set_txpower(sc, 15) != 0)
		return EIO;
	if (cmalo_cmd_set_antenna(sc, 1) != 0)
		return EIO;
	if (cmalo_cmd_set_antenna(sc, 2) != 0)
		return EIO;
	if (cmalo_cmd_set_radio(sc, 1) != 0)
		return EIO;
	if (cmalo_cmd_set_channel(sc, sc->sc_curchan) != 0)
		return EIO;
	if (cmalo_cmd_set_rate(sc, ic->ic_fixed_rate) != 0)
		return EIO;
	if (cmalo_cmd_set_snmp(sc, MALO_OID_RTSTRESH) != 0)
		return EIO;
	if (cmalo_cmd_set_snmp(sc, MALO_OID_SHORTRETRY) != 0)
		return EIO;
	if (cmalo_cmd_set_snmp(sc, MALO_OID_FRAGTRESH) != 0)
		return EIO;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	if (cmalo_cmd_set_macaddr(sc, ic->ic_myaddr) != 0)
		return EIO;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		if (cmalo_wep(sc) != 0)
			return EIO;

	/* device up */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* start network */
	if (ic->ic_opmode != IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	if (sc->sc_flags & MALO_ASSOC_FAILED)
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	/* we are not context save anymore for FW commands */
	sc->sc_cmd_ctxsave = 0;

	return 0;
}

static void
cmalo_watchdog(struct ifnet *ifp)
{
	DPRINTF(2, "watchdog timeout\n");

	/* accept TX packets again */
	ifp->if_flags &= ~IFF_OACTIVE;
}

static int
cmalo_media_change(struct ifnet *ifp)
{
	int error;

	if ((error = ieee80211_media_change(ifp) != ENETRESET))
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		cmalo_init(ifp);

	return 0;
}

static int
cmalo_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct malo_softc *sc = ic->ic_ifp->if_softc;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;

	if (ostate == nstate)
		goto out;

	switch (nstate) {
		case IEEE80211_S_INIT:
			DPRINTF(1, "%s: newstate is IEEE80211_S_INIT\n",
			    device_xname(sc->sc_dev));
			break;
		case IEEE80211_S_SCAN:
			DPRINTF(1, "%s: newstate is IEEE80211_S_SCAN\n",
			    device_xname(sc->sc_dev));
			cmalo_cmd_set_scan(sc);
			if (!sc->sc_net_num) {
				/* no networks found */
				DPRINTF(1, "%s: no networks found\n",
				    device_xname(sc->sc_dev));
				break;
			}
			cmalo_select_network(sc);
			cmalo_cmd_set_auth(sc);
			cmalo_cmd_set_assoc(sc);
			break;
		case IEEE80211_S_AUTH:
			DPRINTF(1, "%s: newstate is IEEE80211_S_AUTH\n",
			    device_xname(sc->sc_dev));
			break;
		case IEEE80211_S_ASSOC:
			DPRINTF(1, "%s: newstate is IEEE80211_S_ASSOC\n",
			    device_xname(sc->sc_dev));
			break;
		case IEEE80211_S_RUN:
			DPRINTF(1, "%s: newstate is IEEE80211_S_RUN\n",
			    device_xname(sc->sc_dev));
			cmalo_reflect_network(sc);
			break;
		default:
			break;
	}

out:
	return sc->sc_newstate(ic, nstate, arg);
}


static int
firmware_load(const char *dname, const char *iname, uint8_t **ucodep,
	      size_t *sizep)
{
	firmware_handle_t fh;
	int error;

	if ((error = firmware_open(dname, iname, &fh)) != 0)
		return error;
	*sizep = firmware_get_size(fh);
	if ((*ucodep = firmware_malloc(*sizep)) == NULL) {
		firmware_close(fh);
		return ENOMEM;
	}
	if ((error = firmware_read(fh, 0, *ucodep, *sizep)) != 0)
		firmware_free(*ucodep, *sizep);
	firmware_close(fh);

	return error;
}

static int
cmalo_fw_alloc(struct malo_softc *sc)
{
	const char *name_h = "malo8385-h";
	const char *name_m = "malo8385-m";
	int error;

	if (sc->sc_fw_h == NULL) {
		/* read helper firmware image */
		error = firmware_load("malo", name_h, &sc->sc_fw_h,
		    &sc->sc_fw_h_size);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "error %d, could not read firmware %s\n",
			    error, name_h);
			return EIO;
		}
	}

	if (sc->sc_fw_m == NULL) {
		/* read main firmware image */
		error = firmware_load("malo", name_m, &sc->sc_fw_m,
		    &sc->sc_fw_m_size);
		if (error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "error %d, could not read firmware %s\n",
			    error, name_m);
			return EIO;
		}
	}

	return 0;
}

static void
cmalo_fw_free(struct malo_softc *sc)
{

	if (sc->sc_fw_h != NULL) {
		firmware_free(sc->sc_fw_h, sc->sc_fw_h_size);
		sc->sc_fw_h = NULL;
	}

	if (sc->sc_fw_m != NULL) {
		firmware_free(sc->sc_fw_m, sc->sc_fw_m_size);
		sc->sc_fw_m = NULL;
	}
}

static int
cmalo_fw_load_helper(struct malo_softc *sc)
{
	uint8_t val8;
	uint16_t bsize, *uc;
	int offset, i;

	/* verify if the card is ready for firmware download */
	val8 = MALO_READ_1(sc, MALO_REG_SCRATCH);
	if (val8 == MALO_VAL_SCRATCH_FW_LOADED)
		/* firmware already loaded */
		return 0;
	if (val8 != MALO_VAL_SCRATCH_READY) {
		/* bad register value */
		aprint_error_dev(sc->sc_dev,
		    "device not ready for FW download\n");
		return EIO;
	}

	/* download the helper firmware */
	for (offset = 0; offset < sc->sc_fw_h_size; offset += bsize) {
		if (sc->sc_fw_h_size - offset >= MALO_FW_HELPER_BSIZE)
			bsize = MALO_FW_HELPER_BSIZE;
		else
			bsize = sc->sc_fw_h_size - offset;

		/* send a block in words and confirm it */
		DPRINTF(3, "%s: download helper FW block (%d bytes, %d off)\n",
		    device_xname(sc->sc_dev), bsize, offset);
		MALO_WRITE_2(sc, MALO_REG_CMD_WRITE_LEN, bsize);
		uc = (uint16_t *)(sc->sc_fw_h + offset);
		for (i = 0; i < bsize / 2; i++)
			MALO_WRITE_2(sc, MALO_REG_CMD_WRITE, htole16(uc[i]));
		MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_CMD_DL_OVER);
		MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE,
		    MALO_VAL_CMD_DL_OVER);

		/* poll for an acknowledgement */
		for (i = 0; i < 50; i++) {
			if (MALO_READ_1(sc, MALO_REG_CARD_STATUS) ==
			    MALO_VAL_CMD_DL_OVER)
				break;
			delay(1000);
		}
		if (i == 50) {
			aprint_error_dev(sc->sc_dev,
			    "timeout while helper FW block download\n");
			return EIO;
		}
	}

	/* helper firmware download done */
	MALO_WRITE_2(sc, MALO_REG_CMD_WRITE_LEN, 0);
	MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_CMD_DL_OVER);
	MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE, MALO_VAL_CMD_DL_OVER);
	DPRINTF(1, "%s: helper FW downloaded\n", device_xname(sc->sc_dev));

	return 0;
}

static int
cmalo_fw_load_main(struct malo_softc *sc)
{
	uint16_t val16, bsize = 0, *uc;
	int offset, i, retry = 0;

	/* verify if the helper firmware has been loaded correctly */
	for (i = 0; i < 10; i++) {
		if (MALO_READ_1(sc, MALO_REG_RBAL) == MALO_FW_HELPER_LOADED)
			break;
		delay(1000);
	}
	if (i == 10) {
		aprint_error_dev(sc->sc_dev, "helper FW not loaded\n");
		return EIO;
	}
	DPRINTF(1, "%s: helper FW loaded successfully\n",
	    device_xname(sc->sc_dev));

	/* download the main firmware */
	for (offset = 0; offset < sc->sc_fw_m_size; offset += bsize) {
		val16 = MALO_READ_2(sc, MALO_REG_RBAL);
		/*
		 * If the helper firmware serves us an odd integer then
		 * something went wrong and we retry to download the last
		 * block until we receive a good integer again, or give up.
		 */
		if (val16 & 0x0001) {
			if (retry > MALO_FW_MAIN_MAXRETRY) {
				aprint_error_dev(sc->sc_dev,
				    "main FW download failed\n");
				return EIO;
			}
			retry++;
			offset -= bsize;
		} else {
			retry = 0;
			bsize = val16;
		}

		/* send a block in words and confirm it */
		DPRINTF(3, "%s: download main FW block (%d bytes, %d off)\n",
		    device_xname(sc->sc_dev), bsize, offset);
		MALO_WRITE_2(sc, MALO_REG_CMD_WRITE_LEN, bsize);
		uc = (uint16_t *)(sc->sc_fw_m + offset);
		for (i = 0; i < bsize / 2; i++)
			MALO_WRITE_2(sc, MALO_REG_CMD_WRITE, htole16(uc[i]));
		MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_CMD_DL_OVER);
                MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE,
		    MALO_VAL_CMD_DL_OVER);

		/* poll for an acknowledgement */
		for (i = 0; i < 5000; i++) {
			if (MALO_READ_1(sc, MALO_REG_CARD_STATUS) ==
			    MALO_VAL_CMD_DL_OVER)
				break;
		}
		if (i == 5000) {
			aprint_error_dev(sc->sc_dev,
			    "timeout while main FW block download\n");
			return EIO;
		}
	}

	DPRINTF(1, "%s: main FW downloaded\n", device_xname(sc->sc_dev));

	/* verify if the main firmware has been loaded correctly */
	for (i = 0; i < 500; i++) {
		if (MALO_READ_1(sc, MALO_REG_SCRATCH) ==
		    MALO_VAL_SCRATCH_FW_LOADED)
			break;
		delay(1000);
	}
	if (i == 500) {
		aprint_error_dev(sc->sc_dev, "main FW not loaded\n");
		return EIO;
	}

	DPRINTF(1, "%s: main FW loaded successfully\n",
	    device_xname(sc->sc_dev));

	return 0;
}

static void
cmalo_stop(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
        struct ifnet *ifp = &sc->sc_if;

	/* device down */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* change device back to initial state */
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	/* reset device */
	cmalo_cmd_set_reset(sc);
	sc->sc_flags &= ~MALO_FW_LOADED;

	if (sc->sc_flags & MALO_DEVICE_ENABLED)
		malo_pcmcia_disable(sc);

	DPRINTF(1, "%s: device down\n", device_xname(sc->sc_dev));
}

static void
cmalo_intr_mask(struct malo_softc *sc, int enable)
{
	uint16_t val16;

	val16 = MALO_READ_2(sc, MALO_REG_HOST_INTR_MASK);

	DPRINTF(3, "%s: intr mask changed from 0x%04x ",
	    device_xname(sc->sc_dev), val16);

	if (enable)
		MALO_WRITE_2(sc, MALO_REG_HOST_INTR_MASK,
		    val16 & ~MALO_VAL_HOST_INTR_MASK_ON);
	else
		MALO_WRITE_2(sc, MALO_REG_HOST_INTR_MASK,
		    val16 | MALO_VAL_HOST_INTR_MASK_ON);

	val16 = MALO_READ_2(sc, MALO_REG_HOST_INTR_MASK);

	DPRINTF(3, "to 0x%04x\n", val16);
}

static void
cmalo_rx(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct malo_rx_desc *rxdesc;
	struct mbuf *m;
	uint8_t *data;
	uint16_t psize;
	int i;

	/* read the whole RX packet which is always 802.3 */
	psize = MALO_READ_2(sc, MALO_REG_DATA_READ_LEN);
	if (psize > MALO_DATA_BUFFER_SIZE) {
		aprint_error_dev(sc->sc_dev,
		    "received data too large: %dbyte\n", psize);
		return;
	}

	MALO_READ_MULTI_2(sc, MALO_REG_DATA_READ,
	    (uint16_t *)sc->sc_data, psize / sizeof(uint16_t));
	if (psize & 0x0001)
		sc->sc_data[psize - 1] = MALO_READ_1(sc, MALO_REG_DATA_READ);
	MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_RX_DL_OVER);
	MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE, MALO_VAL_RX_DL_OVER);

	/* access RX packet descriptor */
	rxdesc = (struct malo_rx_desc *)sc->sc_data;
	rxdesc->status = le16toh(rxdesc->status);
	rxdesc->pkglen = le16toh(rxdesc->pkglen);
	rxdesc->pkgoffset = le32toh(rxdesc->pkgoffset);

	DPRINTF(2, "RX status=%d, pkglen=%d, pkgoffset=%d\n",
	    rxdesc->status, rxdesc->pkglen, rxdesc->pkgoffset);

	if (rxdesc->status != MALO_RX_STATUS_OK)
		/* RX packet is not OK */
		return;

	/* remove the LLC / SNAP header */
	data = sc->sc_data + rxdesc->pkgoffset;
	i = (ETHER_ADDR_LEN * 2) + sizeof(struct llc);
	memcpy(data + (ETHER_ADDR_LEN * 2), data + i, rxdesc->pkglen - i);
	rxdesc->pkglen -= sizeof(struct llc);

#define ETHER_ALIGN	2 /* XXX */
	/* prepare mbuf */
	m = m_devget(sc->sc_data + rxdesc->pkgoffset,
	    rxdesc->pkglen, ETHER_ALIGN, ifp, NULL);
	if (m == NULL) {
		DPRINTF(1, "RX m_devget failed\n");
		ifp->if_ierrors++;
		return;
	}

	if (ifp->if_bpf)
		bpf_ops->bpf_mtap(ifp->if_bpf, m);

	/* push the frame up to the network stack if not in monitor mode */
	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		(*ifp->if_input)(ifp, m);
		ifp->if_ipackets++;
	}
}

static int
cmalo_tx(struct malo_softc *sc, struct mbuf *m)
{
	struct ifnet *ifp = &sc->sc_if;
	struct malo_tx_desc *txdesc = (struct malo_tx_desc *)sc->sc_data;
	uint8_t *data;
	uint16_t psize;

	memset(sc->sc_data, 0, sizeof(*txdesc));
	psize = sizeof(*txdesc) + m->m_pkthdr.len;
	data = mtod(m, uint8_t *);

	/* prepare TX descriptor */
	txdesc->pkgoffset = htole32(sizeof(*txdesc));
	txdesc->pkglen = htole16(m->m_pkthdr.len);
	memcpy(txdesc->dstaddr, data, ETHER_ADDR_LEN);

	/* copy mbuf data to the buffer */
	m_copydata(m, 0, m->m_pkthdr.len, sc->sc_data + sizeof(*txdesc));
	m_freem(m);

	/* send TX packet to the device */
	MALO_WRITE_2(sc, MALO_REG_DATA_WRITE_LEN, psize);
	MALO_WRITE_MULTI_2(sc, MALO_REG_DATA_WRITE,
	    (uint16_t *)sc->sc_data, psize / sizeof(uint16_t));
	if (psize & 0x0001) {
		data = sc->sc_data;
		MALO_WRITE_1(sc, MALO_REG_DATA_WRITE, data[psize - 1]);
	}
	MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_TX_DL_OVER);
	MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE, MALO_VAL_TX_DL_OVER);

	ifp->if_flags |= IFF_OACTIVE;
	ifp->if_timer = 5;

	DPRINTF(2, "%s: TX status=%d, pkglen=%d, pkgoffset=%zd\n",
	    device_xname(sc->sc_dev), txdesc->status, le16toh(txdesc->pkglen),
	    sizeof(*txdesc));

	return 0;
}

static void
cmalo_tx_done(struct malo_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;

	DPRINTF(2, "%s: TX done\n", device_xname(sc->sc_dev));

	ifp->if_opackets++;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	cmalo_start(ifp);
}

static void
cmalo_event(struct malo_softc *sc)
{
	uint16_t event;

	/* read event reason */
	event = MALO_READ_2(sc, MALO_REG_CARD_STATUS);
	event &= MALO_VAL_CARD_STATUS_MASK;
	event = event >> 8;

	switch (event) {
	case MALO_EVENT_DEAUTH:
		DPRINTF(1, "%s: got deauthentication event (0x%04x)\n",
		    device_xname(sc->sc_dev), event);
		/* try to associate again */
		cmalo_cmd_set_assoc(sc);
		break;
	case MALO_EVENT_DISASSOC:
		DPRINTF(1, "%s: got disassociation event (0x%04x)\n",
		    device_xname(sc->sc_dev), event);
		/* try to associate again */
		cmalo_cmd_set_assoc(sc);
		break;
	default:
		DPRINTF(1, "%s: got unknown event (0x%04x)\n",
		    device_xname(sc->sc_dev), event);
		break;
	}

	/* acknowledge event */
	MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE, MALO_VAL_HOST_INTR_EVENT);
}

static void
cmalo_select_network(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int i, best_rssi;

	/* reset last selected network */
	sc->sc_net_cur = 0;

	/* get desired network */
	if (ic->ic_des_esslen) {
		for (i = 0; i < sc->sc_net_num; i++) {
			if (!strcmp(ic->ic_des_essid, sc->sc_net[i].ssid)) {
				sc->sc_net_cur = i;
				DPRINTF(1, "%s: desired network found (%s)\n",
				    device_xname(sc->sc_dev),
				    ic->ic_des_essid);
				return;
			}
		}
		DPRINTF(1, "%s: desired network not found in scan results "
		    "(%s)\n",
		    device_xname(sc->sc_dev), ic->ic_des_essid);
	}

	/* get network with best signal strength */
	best_rssi = sc->sc_net[0].rssi;
	for (i = 0; i < sc->sc_net_num; i++) {
		if (best_rssi < sc->sc_net[i].rssi) {
			best_rssi = sc->sc_net[i].rssi;
			sc->sc_net_cur = i;
		}
	}
	DPRINTF(1, "%s: best network found (%s)\n",
	    device_xname(sc->sc_dev), sc->sc_net[sc->sc_net_cur].ssid);
}

static void
cmalo_reflect_network(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint8_t chan;

	/* reflect active network to our 80211 stack */

	/* BSSID */
	IEEE80211_ADDR_COPY(ic->ic_bss->ni_bssid,
	    sc->sc_net[sc->sc_net_cur].bssid);

	/* SSID */
	ic->ic_bss->ni_esslen = strlen(sc->sc_net[sc->sc_net_cur].ssid);
	memcpy(ic->ic_bss->ni_essid, sc->sc_net[sc->sc_net_cur].ssid,
	    ic->ic_bss->ni_esslen);

	/* channel */
	chan = sc->sc_net[sc->sc_net_cur].channel;
	ic->ic_curchan = &ic->ic_channels[chan];
}

static int
cmalo_wep(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		struct ieee80211_key *key = &ic->ic_crypto.cs_nw_keys[i];

		if (!key->wk_keylen)
			continue;

		DPRINTF(1, "%s: setting wep key for index %d\n",
		    device_xname(sc->sc_dev), i);

		cmalo_cmd_set_wep(sc, i, key);
	}

	return 0;
}

static int
cmalo_rate2bitmap(int rate)
{
	switch (rate) {
	/* CCK rates */
	case  0:	return MALO_RATE_BITMAP_DS1;
	case  1:	return MALO_RATE_BITMAP_DS2;
	case  2:	return MALO_RATE_BITMAP_DS5;
	case  3:	return MALO_RATE_BITMAP_DS11;

	/* OFDM rates */
	case  4:	return MALO_RATE_BITMAP_OFDM6;
	case  5:	return MALO_RATE_BITMAP_OFDM9;
	case  6:	return MALO_RATE_BITMAP_OFDM12;
	case  7:	return MALO_RATE_BITMAP_OFDM18;
	case  8:	return MALO_RATE_BITMAP_OFDM24;
	case  9:	return MALO_RATE_BITMAP_OFDM36;
	case 10:	return MALO_RATE_BITMAP_OFDM48;
	case 11:	return MALO_RATE_BITMAP_OFDM54;

	/* unknown rate: should not happen */
	default:	return 0;
	}
}

static void
cmalo_hexdump(void *buf, int len)
{
#ifdef CMALO_DEBUG
	int i;

	if (cmalo_d >= 2) {
		for (i = 0; i < len; i++) {
			if (i % 16 == 0)
				printf("%s%5i:", i ? "\n" : "", i);
			if (i % 4 == 0)
				printf(" ");
			printf("%02x", (int)*((u_char *)buf + i));
		}
		printf("\n");
	}
#endif
}

static int
cmalo_cmd_get_hwspec(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_spec *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_HWSPEC);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_spec *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	/* set all bits for MAC address, otherwise we won't get one back */
	memset(body->macaddr, 0xff, ETHER_ADDR_LEN);

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_rsp_hwspec(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct malo_cmd_header *hdr = (struct malo_cmd_header *)sc->sc_cmd;
	struct malo_cmd_body_spec *body;
	int i;

	body = (struct malo_cmd_body_spec *)(hdr + 1);

	/* get our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		ic->ic_myaddr[i] = body->macaddr[i];

	return 0;
}

static int
cmalo_cmd_set_reset(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr = (struct malo_cmd_header *)sc->sc_cmd;
	const uint16_t psize = sizeof(*hdr);

	hdr->cmd = htole16(MALO_CMD_RESET);
	hdr->size = 0;
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 1) != 0)
		return EIO;

	/* give the device some time to finish the reset */
	delay(100);

	return 0;
}

static int
cmalo_cmd_set_scan(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_scan *body;
	struct malo_cmd_tlv_ssid *body_ssid;
	struct malo_cmd_tlv_chanlist *body_chanlist;
	struct malo_cmd_tlv_rates *body_rates;
	uint16_t psize;
	int i;

	psize = sizeof(*hdr) + sizeof(*body) +
	    sizeof(*body_ssid) + sizeof(*body_chanlist) + sizeof(*body_rates);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_SCAN);
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_scan *)(hdr + 1);
	body->bsstype = 0x03; /* any BSS */
	memset(body->bssid, 0xff, ETHER_ADDR_LEN);

	body_ssid = (struct malo_cmd_tlv_ssid *)(body + 1);
	body_ssid->type = htole16(MALO_TLV_TYPE_SSID);
	body_ssid->size = htole16(0);

	body_chanlist = (struct malo_cmd_tlv_chanlist *)(body_ssid + 1);
	body_chanlist->type = htole16(MALO_TLV_TYPE_CHANLIST);
	body_chanlist->size = htole16(sizeof(body_chanlist->data));
	for (i = 0; i < CHANNELS; i++) {
		body_chanlist->data[i].radiotype = 0x00;
		body_chanlist->data[i].channumber = (i + 1);
		body_chanlist->data[i].scantype = 0x00; /* active */
		body_chanlist->data[i].minscantime = htole16(0);
		body_chanlist->data[i].maxscantime = htole16(100);
	}

	body_rates = (struct malo_cmd_tlv_rates *)(body_chanlist + 1);
	body_rates->type = htole16(MALO_TLV_TYPE_RATES);
	body_rates->size =
	    htole16(ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates);
	memcpy(body_rates->data, ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates,
	    ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates);
	psize += le16toh(body_rates->size);

	memset((char *)(body_rates + 1) + le16toh(body_rates->size), 0,
	    sizeof(struct malo_cmd_tlv_numprobes));

	hdr->size = htole16(psize - sizeof(*hdr));

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_rsp_scan(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr = (struct malo_cmd_header *)sc->sc_cmd;
	struct malo_cmd_body_rsp_scan *body;
	struct malo_cmd_body_rsp_scan_set *set;
	int i;

	memset(sc->sc_net, 0, sizeof(sc->sc_net));

	body = (struct malo_cmd_body_rsp_scan *)(hdr + 1);
	body->bufsize = le16toh(body->bufsize);

	DPRINTF(1, "bufsize=%d, APs=%d\n", body->bufsize, body->numofset);
	sc->sc_net_num = body->numofset;

	set = (struct malo_cmd_body_rsp_scan_set *)(body + 1);

	/* cycle through found networks */
	for (i = 0; i < body->numofset; i++) {
		set->size = le16toh(set->size);
		set->beaconintvl = le16toh(set->beaconintvl);
		set->capinfo = le16toh(set->capinfo);

		DPRINTF(1, "size=%d, bssid=%s, rssi=%d, beaconintvl=%d, "
		    "capinfo=0x%04x\n",
		    set->size, ether_sprintf(set->bssid), set->rssi,
		    set->beaconintvl, set->capinfo);

		/* save scan results */
		memcpy(sc->sc_net[i].bssid, set->bssid, sizeof(set->bssid));
		sc->sc_net[i].rssi = set->rssi;
		memcpy(sc->sc_net[i].timestamp, set->timestamp,
		    sizeof(set->timestamp));
		sc->sc_net[i].beaconintvl = set->beaconintvl;
		sc->sc_net[i].capinfo = set->capinfo;

		cmalo_parse_elements(sc, set->data,
		    set->size - (sizeof(*set) - sizeof(set->size)), i);

		set = (struct malo_cmd_body_rsp_scan_set *)
				((char *)set + sizeof(set->size) + set->size);
	}

	return 0;
}

static int
cmalo_parse_elements(struct malo_softc *sc, uint8_t *buf, int size, int pos)
{
	uint8_t eid, len;
	int i;

	DPRINTF(2, "element_size=%d, element_pos=%d\n", size, pos);

	for (i = 0; i < size; ) {
		eid = *(uint8_t *)(buf + i);
		i++;
		len = *(uint8_t *)(buf + i);
		i++;
		DPRINTF(2, "eid=%d, len=%d, ", eid, len);

		switch (eid) {
		case IEEE80211_ELEMID_SSID:
			memcpy(sc->sc_net[pos].ssid, buf + i, len);
			DPRINTF(2, "ssid=%s\n", sc->sc_net[pos].ssid);
			break;
		case IEEE80211_ELEMID_RATES:
			memcpy(sc->sc_net[pos].rates, buf + i, len);
			DPRINTF(2, "rates\n");
			break;
		case IEEE80211_ELEMID_DSPARMS:
			sc->sc_net[pos].channel = *(uint8_t *)(buf + i);
			DPRINTF(2, "chnl=%d\n", sc->sc_net[pos].channel);
			break;
		default:
			DPRINTF(2, "unknown\n");
			break;
		}

		i += len;
	}

	return 0;
}

static int
cmalo_cmd_set_auth(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_auth *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_AUTH);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_auth *)(hdr + 1);
	memcpy(body->peermac, sc->sc_net[sc->sc_net_cur].bssid, ETHER_ADDR_LEN);
	body->authtype = 0;

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_wep(struct malo_softc *sc, uint16_t index,
		  struct ieee80211_key *key)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_wep *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_WEP);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_wep *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	body->action = htole16(MALO_WEP_ACTION_TYPE_ADD);
	body->key_index = htole16(index);

	if (body->key_index == 0) {
		if (key->wk_keylen > 5)
			body->key_type_1 = MALO_WEP_KEY_TYPE_104BIT;
		else
			body->key_type_1 = MALO_WEP_KEY_TYPE_40BIT;
		memcpy(body->key_value_1, key->wk_key, key->wk_keylen);
	}
	if (body->key_index == 1) {
		if (key->wk_keylen > 5)
			body->key_type_2 = MALO_WEP_KEY_TYPE_104BIT;
		else
			body->key_type_2 = MALO_WEP_KEY_TYPE_40BIT;
		memcpy(body->key_value_2, key->wk_key, key->wk_keylen);
	}
	if (body->key_index == 2) {
		if (key->wk_keylen > 5)
			body->key_type_3 = MALO_WEP_KEY_TYPE_104BIT;
		else
			body->key_type_3 = MALO_WEP_KEY_TYPE_40BIT;
		memcpy(body->key_value_3, key->wk_key, key->wk_keylen);
	}
	if (body->key_index == 3) {
		if (key->wk_keylen > 5)
			body->key_type_4 = MALO_WEP_KEY_TYPE_104BIT;
		else
			body->key_type_4 = MALO_WEP_KEY_TYPE_40BIT;
		memcpy(body->key_value_4, key->wk_key, key->wk_keylen);
	}

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_snmp(struct malo_softc *sc, uint16_t oid)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_snmp *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_SNMP);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_snmp *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	body->action = htole16(1);

	switch (oid) {
	case MALO_OID_RTSTRESH:
		body->oid = htole16(MALO_OID_RTSTRESH);
		body->size = htole16(2);
		*(uint16_t *)body->data = htole16(2347);
		break;
	case MALO_OID_SHORTRETRY:
		body->oid = htole16(MALO_OID_SHORTRETRY);
		body->size = htole16(2);
		*(uint16_t *)body->data = htole16(4);
		break;
	case MALO_OID_FRAGTRESH:
		body->oid = htole16(MALO_OID_FRAGTRESH);
		body->size = htole16(2);
		*(uint16_t *)body->data = htole16(2346);
		break;
	case MALO_OID_80211D:
		body->oid = htole16(MALO_OID_80211D);
		body->size = htole16(2);
		*(uint16_t *)body->data = htole16(1);
		break;
	default:
		break;
	}

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_radio(struct malo_softc *sc, uint16_t control)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_radio *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_RADIO);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_radio *)(hdr + 1);
	body->action = htole16(1);
	if (control)
		body->control =
		    htole16(MALO_CMD_RADIO_ON | MALO_CMD_RADIO_AUTO_P);
	else
		body->control = 0;

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_channel(struct malo_softc *sc, uint16_t channel)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_channel *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_CHANNEL);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_channel *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	body->action = htole16(1);
	body->channel = htole16(channel);

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}


static int
cmalo_cmd_set_txpower(struct malo_softc *sc, int16_t txpower)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_txpower *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_TXPOWER);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_txpower *)(hdr + 1);
	body->action = htole16(1);
	body->txpower = htole16(txpower);

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_antenna(struct malo_softc *sc, uint16_t action)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_antenna *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_ANTENNA);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_antenna *)(hdr + 1);
	/* 1 = set RX, 2 = set TX */
	body->action = htole16(action);

	switch (action) {
	case 1:
		/* set RX antenna */
		body->antenna_mode = htole16(0xffff);
		break;

	case 2:
		/* set TX antenna */
		body->antenna_mode = htole16(2);
		break;

	default:
		body->antenna_mode = 0;
		break;
	}

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_macctrl(struct malo_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_macctrl *body;
	uint16_t psize;

	psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_MACCTRL);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_macctrl *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	body->action = htole16(MALO_CMD_MACCTRL_RX_ON | MALO_CMD_MACCTRL_TX_ON);
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		body->action |= htole16(MALO_CMD_MACCTRL_PROMISC_ON);

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_macaddr(struct malo_softc *sc, uint8_t *macaddr)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_macaddr *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_MACADDR);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_macaddr *)(hdr + 1);
	body->action = htole16(1);
	memcpy(body->macaddr, macaddr, ETHER_ADDR_LEN);

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_set_assoc(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_assoc *body;
	struct malo_cmd_tlv_ssid *body_ssid;
	struct malo_cmd_tlv_phy *body_phy;
	struct malo_cmd_tlv_cf *body_cf;
	struct malo_cmd_tlv_rates *body_rates;
	struct malo_cmd_tlv_passeid *body_passeid;
	uint16_t psize;

	psize = sizeof(*hdr) + sizeof(*body) + sizeof(*body_ssid) +
	    sizeof(body_phy) + sizeof(*body_cf) + sizeof(*body_rates);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_ASSOC);
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_assoc *)(hdr + 1);
	memset(body, 0, sizeof(*body));
	memcpy(body->peermac, sc->sc_net[sc->sc_net_cur].bssid, ETHER_ADDR_LEN);
	body->capinfo = htole16(sc->sc_net[sc->sc_net_cur].capinfo);
	body->listenintrv = htole16(10);

	body_ssid = (struct malo_cmd_tlv_ssid *)(body + 1);
	body_ssid->type = htole16(MALO_TLV_TYPE_SSID);
	body_ssid->size = htole16(strlen(sc->sc_net[sc->sc_net_cur].ssid));
	memcpy(body_ssid->data, sc->sc_net[sc->sc_net_cur].ssid,
	    le16toh(body_ssid->size));
	psize += le16toh(body_ssid->size);

	body_phy = (struct malo_cmd_tlv_phy *)
			((char *)(body_ssid + 1) + le16toh(body_ssid->size));
	body_phy->type = htole16(MALO_TLV_TYPE_PHY);
	body_phy->size = htole16(1);
	body_phy->data[0] = sc->sc_net[sc->sc_net_cur].channel;
	psize += le16toh(body_phy->size);

	body_cf = (struct malo_cmd_tlv_cf *)
			((char *)(body_phy + 1) + le16toh(body_phy->size));
	body_cf->type = htole16(MALO_TLV_TYPE_CF);
	body_cf->size = htole16(0);

	body_rates = (struct malo_cmd_tlv_rates *)(body_cf + 1);
	body_rates->type = htole16(MALO_TLV_TYPE_RATES);
	body_rates->size = htole16(strlen(sc->sc_net[sc->sc_net_cur].rates));
	memcpy(body_rates->data, sc->sc_net[sc->sc_net_cur].rates,
	    le16toh(body_rates->size));
	psize += le16toh(body_rates->size);

	/* hack to correct FW's wrong generated rates-element-id */
	body_passeid = (struct malo_cmd_tlv_passeid *)
			((char *)(body_rates + 1) + le16toh(body_rates->size));
	body_passeid->type = htole16(MALO_TLV_TYPE_PASSEID);
	body_passeid->size = body_rates->size;
	memcpy(body_passeid->data, body_rates->data, le16toh(body_rates->size));
	psize += le16toh(body_passeid->size);

	hdr->size = htole16(psize - sizeof(*hdr));

	/* process command request */
	if (!sc->sc_cmd_ctxsave) {
		if (cmalo_cmd_request(sc, psize, 1) != 0)
			return EIO;
		return 0;
	}
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_rsp_assoc(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr = (struct malo_cmd_header *)sc->sc_cmd;
	struct malo_cmd_body_rsp_assoc *body;

	body = (struct malo_cmd_body_rsp_assoc *)(hdr + 1);

	if (body->status) {
		DPRINTF(1, "%s: association failed (status %d)\n",
		    device_xname(sc->sc_dev), body->status);
		sc->sc_flags |= MALO_ASSOC_FAILED;
	} else
		DPRINTF(1, "%s: association successful\n",
		    device_xname(sc->sc_dev));

	return 0;
}

static int
cmalo_cmd_set_rate(struct malo_softc *sc, int rate)
{
	struct malo_cmd_header *hdr;
	struct malo_cmd_body_rate *body;
	const uint16_t psize = sizeof(*hdr) + sizeof(*body);

	hdr = (struct malo_cmd_header *)sc->sc_cmd;
	hdr->cmd = htole16(MALO_CMD_RATE);
	hdr->size = htole16(sizeof(*body));
	hdr->seqnum = htole16(1);
	hdr->result = 0;

	body = (struct malo_cmd_body_rate *)(hdr + 1);
	body->action = htole16(1);
	if (rate == IEEE80211_FIXED_RATE_NONE) {
 		body->hwauto = htole16(1);
		body->ratebitmap = htole16(MALO_RATE_BITMAP_AUTO);
	} else {
 		body->hwauto = 0;
		body->ratebitmap = htole16(cmalo_rate2bitmap(rate));
	}

	/* process command request */
	if (cmalo_cmd_request(sc, psize, 0) != 0)
		return EIO;

	/* process command repsonse */
	cmalo_cmd_response(sc);

	return 0;
}

static int
cmalo_cmd_request(struct malo_softc *sc, uint16_t psize, int no_response)
{
	uint8_t *cmd;

	mutex_enter(&sc->sc_mtx);

	cmalo_hexdump(sc->sc_cmd, psize);

	/* send command request */
	MALO_WRITE_2(sc, MALO_REG_CMD_WRITE_LEN, psize);
	MALO_WRITE_MULTI_2(sc, MALO_REG_CMD_WRITE,
	    (uint16_t *)sc->sc_cmd, psize / sizeof(uint16_t));
	if (psize & 0x0001) {
		cmd = sc->sc_cmd;
		MALO_WRITE_1(sc, MALO_REG_CMD_WRITE, cmd[psize - 1]);
	}
	MALO_WRITE_1(sc, MALO_REG_HOST_STATUS, MALO_VAL_CMD_DL_OVER);
	MALO_WRITE_2(sc, MALO_REG_CARD_INTR_CAUSE, MALO_VAL_CMD_DL_OVER);

	if (no_response) {
		mutex_exit(&sc->sc_mtx);

		/* we don't expect a response */
		return 0;
	}

	/* wait for the command response */
	if (cv_timedwait_sig(&sc->sc_cv, &sc->sc_mtx, 500) == EWOULDBLOCK) {
		mutex_exit(&sc->sc_mtx);
		aprint_error_dev(sc->sc_dev,
		    "timeout while waiting for cmd response\n");
		return EIO;
	}
	mutex_exit(&sc->sc_mtx);

	return 0;
}

static int
cmalo_cmd_response(struct malo_softc *sc)
{
	struct malo_cmd_header *hdr = (struct malo_cmd_header *)sc->sc_cmd;
	uint16_t psize;
	int s;

	s = splnet();

#ifdef CMALO_DEBUG
	memset(sc->sc_cmd, 0, MALO_CMD_BUFFER_SIZE);
#endif

	/* read the whole command response */
	psize = MALO_READ_2(sc, MALO_REG_CMD_READ_LEN);
	if (psize > MALO_CMD_BUFFER_SIZE) {
		aprint_error_dev(sc->sc_dev,
		    "command response too large: %dbyte\n", psize);
		return EIO;
	}

	MALO_READ_MULTI_2(sc, MALO_REG_CMD_READ,
	    (uint16_t *)sc->sc_cmd, psize / sizeof(uint16_t));
	if (psize & 0x0001)
		sc->sc_cmd[psize - 1] = MALO_READ_1(sc, MALO_REG_CMD_READ);

	cmalo_hexdump(sc->sc_cmd, psize);

	/*
	 * We convert the header values into the machines correct endianess,
	 * so we don't have to le16toh() all over the code.  The body is
	 * kept in the cards order, little endian.  We need to take care
	 * about the body endianess in the corresponding response routines.
	 */
	hdr->cmd = le16toh(hdr->cmd);
	hdr->size = le16toh(hdr->size);
	hdr->seqnum = le16toh(hdr->seqnum);
	hdr->result = le16toh(hdr->result);

	/* check for a valid command response */
	if (!(hdr->cmd & MALO_CMD_RESP)) {
		aprint_error_dev(sc->sc_dev,
		    "got invalid command response (0x%04x)\n", hdr->cmd);
		splx(s);
		return EIO;
	}
	hdr->cmd &= ~MALO_CMD_RESP;

	/* association cmd response is special */
	if (hdr->cmd == 0x0012)
		hdr->cmd = MALO_CMD_ASSOC;

	/* to which command does the response belong */
	switch (hdr->cmd) {
	case MALO_CMD_HWSPEC:
		DPRINTF(1, "%s: got hwspec cmd response\n",
		    device_xname(sc->sc_dev));
		cmalo_cmd_rsp_hwspec(sc);
		break;
	case MALO_CMD_RESET:
		/* reset will not send back a response */
		break;
	case MALO_CMD_SCAN:
		DPRINTF(1, "%s: got scan cmd response\n",
		    device_xname(sc->sc_dev));
		cmalo_cmd_rsp_scan(sc);
		break;
	case MALO_CMD_AUTH:
		/* do nothing */
		DPRINTF(1, "%s: got auth cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_WEP:
		/* do nothing */
		DPRINTF(1, "%s: got wep cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_SNMP:
		/* do nothing */
		DPRINTF(1, "%s: got snmp cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_RADIO:
		/* do nothing */
		DPRINTF(1, "%s: got radio cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_CHANNEL:
		/* do nothing */
		DPRINTF(1, "%s: got channel cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_TXPOWER:
		/* do nothing */
		DPRINTF(1, "%s: got txpower cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_ANTENNA:
		/* do nothing */
		DPRINTF(1, "%s: got antenna cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_MACCTRL:
		/* do nothing */
		DPRINTF(1, "%s: got macctrl cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_MACADDR:
		/* do nothing */
		DPRINTF(1, "%s: got macaddr cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_ASSOC:
		/* do nothing */
		DPRINTF(1, "%s: got assoc cmd response\n",
		    device_xname(sc->sc_dev));
		cmalo_cmd_rsp_assoc(sc);
		break;
	case MALO_CMD_80211D:
		/* do nothing */
		DPRINTF(1, "%s: got 80211d cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_BGSCAN_CONFIG:
		/* do nothing */
		DPRINTF(1, "%s: got bgscan config cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_BGSCAN_QUERY:
		/* do nothing */
		DPRINTF(1, "%s: got bgscan query cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	case MALO_CMD_RATE:
		/* do nothing */
		DPRINTF(1, "%s: got rate cmd response\n",
		    device_xname(sc->sc_dev));
		break;
	default:
		aprint_error_dev(sc->sc_dev,
		    "got unknown cmd response (0x%04x)\n", hdr->cmd);
		break;
	}

	splx(s);

	return 0;
}

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, malo_pcmcia, NULL);

CFDRIVER_DECL(malo_pcmcia, DV_IFNET, NULL);
extern struct cfattach malo_pcmcia_ca;
static int malo_pcmcialoc[] = { -1 };
static struct cfparent pcmciaparent = {
	"pcmcia", NULL, DVUNIT_ANY
};
static struct cfdata malo_pcmcia_cfdata[] = {
	{
		.cf_name = "malo_pcmcia",
		.cf_atname = "malo",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = malo_pcmcialoc,
		.cf_flags = 0,
		.cf_pspec = &pcmciaparent,
	},
	{ NULL }
};

static int
malo_pcmcia_modcmd(modcmd_t cmd, void *arg)
{
	int err;

	switch (cmd) {
	case MODULE_CMD_INIT:
		err = config_cfdriver_attach(&malo_pcmcia_cd);
		if (err)
			return err;
		err = config_cfattach_attach("malo_pcmcia", &malo_pcmcia_ca);
		if (err) {
			config_cfdriver_detach(&malo_pcmcia_cd);
			return err;
		}
		err = config_cfdata_attach(malo_pcmcia_cfdata, 1);
		if (err) {
			config_cfattach_detach("malo_pcmcia", &malo_pcmcia_ca);
			config_cfdriver_detach(&malo_pcmcia_cd);
			return err;
		}
		return 0;
	case MODULE_CMD_FINI:
		err = config_cfdata_detach(malo_pcmcia_cfdata);
		if (err)
			return err;
		config_cfattach_detach("malo_pcmcia", &malo_pcmcia_ca);
		config_cfdriver_detach(&malo_pcmcia_cd);
		return 0;
	default:
		return ENOTTY;
	}
}
#endif

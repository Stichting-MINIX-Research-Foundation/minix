/*	$NetBSD: an.c,v 1.62 2015/02/07 04:11:06 christos Exp $	*/
/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/an/if_an.c,v 1.12 2000/11/13 23:04:12 wpaul Exp $
 */
/*
 * Copyright (c) 2004, 2005 David Young.  All rights reserved.
 * Copyright (c) 2004, 2005 OJC Technologies.  All rights reserved.
 * Copyright (c) 2004, 2005 Dayton Data Center Services, LLC.  All
 *     rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY David Young AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL David Young AND CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Aironet 4500/4800 802.11 PCMCIA/ISA/PCI driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * Ported to NetBSD from FreeBSD by Atsushi Onoe at the San Diego
 * IETF meeting.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: an.c,v 1.62 2015/02/07 04:11:06 christos Exp $");


#include <sys/param.h>
#include <sys/callout.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/md4.h>
#include <sys/endian.h>
#include <sys/kauth.h>

#include <sys/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/ic/anreg.h>
#include <dev/ic/anvar.h>

static int	an_reset(struct an_softc *);
static void	an_wait(struct an_softc *);
static int	an_init(struct ifnet *);
static void	an_stop(struct ifnet *, int);
static void	an_start(struct ifnet *);
static void	an_watchdog(struct ifnet *);
static int	an_ioctl(struct ifnet *, u_long, void *);
static int	an_media_change(struct ifnet *);
static void	an_media_status(struct ifnet *, struct ifmediareq *);

static int	an_set_nwkey(struct an_softc *, struct ieee80211_nwkey *);
static int	an_set_nwkey_wep(struct an_softc *, struct ieee80211_nwkey *);
static int	an_set_nwkey_eap(struct an_softc *, struct ieee80211_nwkey *);
static int	an_get_nwkey(struct an_softc *, struct ieee80211_nwkey *);
static int	an_write_wepkey(struct an_softc *, int, struct an_wepkey *,
				int);

static void	an_rx_intr(struct an_softc *);
static void	an_tx_intr(struct an_softc *, int);
static void	an_linkstat_intr(struct an_softc *);

static int	an_cmd(struct an_softc *, int, int);
static int	an_seek_bap(struct an_softc *, int, int);
static int	an_read_bap(struct an_softc *, int, int, void *, int);
static int	an_write_bap(struct an_softc *, int, int, void *, int);
static int	an_mwrite_bap(struct an_softc *, int, int, struct mbuf *, int);
static int	an_read_rid(struct an_softc *, int, void *, int *);
static int	an_write_rid(struct an_softc *, int, void *, int);

static int	an_alloc_fid(struct an_softc *, int, int *);

static int	an_newstate(struct ieee80211com *, enum ieee80211_state, int);

#ifdef AN_DEBUG
int an_debug = 0;

#define	DPRINTF(X)	if (an_debug) printf X
#define	DPRINTF2(X)	if (an_debug > 1) printf X
static int an_sysctl_verify(SYSCTLFN_PROTO, int lower, int upper);
static int an_sysctl_verify_debug(SYSCTLFN_PROTO);
#else
#define	DPRINTF(X)
#define	DPRINTF2(X)
#endif

int
an_attach(struct an_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int i, s;
	struct an_rid_wepkey *akey;
	int buflen, kid, rid;
	int chan, chan_min, chan_max;

	s = splnet();

	an_wait(sc);
	if (an_reset(sc) != 0) {
		config_deactivate(sc->sc_dev);
		splx(s);
		return 1;
	}

	/* Load factory config */
	if (an_cmd(sc, AN_CMD_READCFG, 0) != 0) {
		splx(s);
		aprint_error_dev(sc->sc_dev, "failed to load config data\n");
		return 1;
	}

	/* Read the current configuration */
	buflen = sizeof(sc->sc_config);
	if (an_read_rid(sc, AN_RID_GENCONFIG, &sc->sc_config, &buflen) != 0) {
		splx(s);
		aprint_error_dev(sc->sc_dev, "read config failed\n");
		return 1;
	}

	/* Read the card capabilities */
	buflen = sizeof(sc->sc_caps);
	if (an_read_rid(sc, AN_RID_CAPABILITIES, &sc->sc_caps, &buflen) != 0) {
		splx(s);
		aprint_error_dev(sc->sc_dev, "read caps failed\n");
		return 1;
	}

#ifdef AN_DEBUG
	if (an_debug) {
		static const int dumprid[] = {
		    AN_RID_GENCONFIG, AN_RID_CAPABILITIES, AN_RID_SSIDLIST,
		    AN_RID_APLIST, AN_RID_STATUS, AN_RID_ENCAP
		};

		for (rid = 0; rid < sizeof(dumprid)/sizeof(dumprid[0]); rid++) {
			buflen = sizeof(sc->sc_buf);
			if (an_read_rid(sc, dumprid[rid], &sc->sc_buf, &buflen)
			    != 0)
				continue;
			printf("%04x (%d):\n", dumprid[rid], buflen);
			for (i = 0; i < (buflen + 1) / 2; i++)
				printf(" %04x", sc->sc_buf.sc_val[i]);
			printf("\n");
		}
	}
#endif

	/* Read WEP settings from persistent memory */
	akey = &sc->sc_buf.sc_wepkey;
	buflen = sizeof(struct an_rid_wepkey);
	rid = AN_RID_WEP_VOLATILE;	/* first persistent key */
	while (an_read_rid(sc, rid, akey, &buflen) == 0) {
		kid = le16toh(akey->an_key_index);
		DPRINTF(("an_attach: wep rid=0x%x len=%d(%zu) index=0x%04x "
		    "mac[0]=%02x keylen=%d\n",
		    rid, buflen, sizeof(*akey), kid,
		    akey->an_mac_addr[0], le16toh(akey->an_key_len)));
		if (kid == 0xffff) {
			sc->sc_tx_perskey = akey->an_mac_addr[0];
			sc->sc_tx_key = -1;
			break;
		}
		if (kid >= IEEE80211_WEP_NKID)
			break;
		sc->sc_perskeylen[kid] = le16toh(akey->an_key_len);
		sc->sc_wepkeys[kid].an_wep_keylen = -1;
		rid = AN_RID_WEP_PERSISTENT;	/* for next key */
		buflen = sizeof(struct an_rid_wepkey);
	}

	aprint_normal_dev(sc->sc_dev, "%s %s (firmware %s)\n",
	    sc->sc_caps.an_manufname, sc->sc_caps.an_prodname,
	    sc->sc_caps.an_prodvers);

	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_NOTRAILERS | IFF_SIMPLEX |
	    IFF_MULTICAST | IFF_ALLMULTI;
	ifp->if_ioctl = an_ioctl;
	ifp->if_start = an_start;
	ifp->if_init = an_init;
	ifp->if_stop = an_stop;
	ifp->if_watchdog = an_watchdog;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_WEP | IEEE80211_C_PMGT | IEEE80211_C_IBSS |
	    IEEE80211_C_MONITOR;
	ic->ic_state = IEEE80211_S_INIT;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, sc->sc_caps.an_oemaddr);

	switch (le16toh(sc->sc_caps.an_regdomain)) {
	default:
	case AN_REGDOMAIN_USA:
	case AN_REGDOMAIN_CANADA:
		chan_min = 1; chan_max = 11; break;
	case AN_REGDOMAIN_EUROPE:
	case AN_REGDOMAIN_AUSTRALIA:
		chan_min = 1; chan_max = 13; break;
	case AN_REGDOMAIN_JAPAN:
		chan_min = 14; chan_max = 14; break;
	case AN_REGDOMAIN_SPAIN:
		chan_min = 10; chan_max = 11; break;
	case AN_REGDOMAIN_FRANCE:
		chan_min = 10; chan_max = 13; break;
	case AN_REGDOMAIN_JAPANWIDE:
		chan_min = 1; chan_max = 14; break;
	}

	for (chan = chan_min; chan <= chan_max; chan++) {
		ic->ic_channels[chan].ic_freq =
		    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_B;
	}
	ic->ic_ibss_chan = &ic->ic_channels[chan_min];

	aprint_normal("%s: 802.11 address: %s, channel: %d-%d\n",
	    ifp->if_xname, ether_sprintf(ic->ic_myaddr), chan_min, chan_max);

	/* Find supported rate */
	for (i = 0; i < sizeof(sc->sc_caps.an_rates); i++) {
		if (sc->sc_caps.an_rates[i] == 0)
			continue;
		ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[
		    ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates++] =
		    sc->sc_caps.an_rates[i];
	}

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ieee80211_ifattach(ic);

	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = an_newstate;

	ieee80211_media_init(ic, an_media_change, an_media_status);

	/*
	 * radiotap BPF device
	 */
	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64, &sc->sc_drvbpf);

	memset(&sc->sc_rxtapu, 0, sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.ar_ihdr.it_len = htole16(sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.ar_ihdr.it_present = htole32(AN_RX_RADIOTAP_PRESENT);

	memset(&sc->sc_txtapu, 0, sizeof(sc->sc_txtapu));
	sc->sc_txtap.at_ihdr.it_len = htole16(sizeof(sc->sc_txtapu));
	sc->sc_txtap.at_ihdr.it_present = htole32(AN_TX_RADIOTAP_PRESENT);

	sc->sc_attached = 1;
	splx(s);

	ieee80211_announce(ic);
	return 0;
}

#ifdef AN_DEBUG
/*
 * Setup sysctl(3) MIB, hw.an.*
 *
 * TBD condition CTLFLAG_PERMANENT on being a module or not
 */
SYSCTL_SETUP(sysctl_an, "sysctl an(4) subtree setup")
{
	int rc;
	const struct sysctlnode *cnode, *rnode;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "an",
	    "Cisco/Aironet 802.11 controls",
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	/* control debugging printfs */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Enable Cisco/Aironet debugging output"),
	    an_sysctl_verify_debug, 0, &an_debug, 0,
	    CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return;
err:
	printf("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

static int
an_sysctl_verify(SYSCTLFN_ARGS, int lower, int upper)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (t < lower || t > upper)
		return (EINVAL);

	*(int*)rnode->sysctl_data = t;

	return (0);
}

static int
an_sysctl_verify_debug(SYSCTLFN_ARGS)
{
	return an_sysctl_verify(SYSCTLFN_CALL(rnode), 0, 2);
}
#endif /* AN_DEBUG */

int
an_detach(struct an_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	int s;

	if (!sc->sc_attached)
		return 0;

	s = splnet();
	an_stop(ifp, 1);
	ieee80211_ifdetach(ic);
	if_detach(ifp);
	splx(s);
	return 0;
}

int
an_activate(device_t self, enum devact act)
{
	struct an_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_if);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
an_intr(void *arg)
{
	struct an_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	int i;
	u_int16_t status;

	if (!sc->sc_enabled || !device_is_active(sc->sc_dev) ||
	    (ifp->if_flags & IFF_RUNNING) == 0)
		return 0;

	if ((ifp->if_flags & IFF_UP) == 0) {
		CSR_WRITE_2(sc, AN_INT_EN, 0);
		CSR_WRITE_2(sc, AN_EVENT_ACK, ~0);
		return 1;
	}

	/* maximum 10 loops per interrupt */
	for (i = 0; i < 10; i++) {
		if (!sc->sc_enabled || !device_is_active(sc->sc_dev))
			return 1;
		if (CSR_READ_2(sc, AN_SW0) != AN_MAGIC) {
			DPRINTF(("an_intr: magic number changed: %x\n",
			    CSR_READ_2(sc, AN_SW0)));
			config_deactivate(sc->sc_dev);
			return 1;
		}
		status = CSR_READ_2(sc, AN_EVENT_STAT);
		CSR_WRITE_2(sc, AN_EVENT_ACK, status & ~(AN_INTRS));
		if ((status & AN_INTRS) == 0)
			break;

		if (status & AN_EV_RX)
			an_rx_intr(sc);

		if (status & (AN_EV_TX | AN_EV_TX_EXC))
			an_tx_intr(sc, status);

		if (status & AN_EV_LINKSTAT)
			an_linkstat_intr(sc);

		if ((ifp->if_flags & IFF_OACTIVE) == 0 &&
		    sc->sc_ic.ic_state == IEEE80211_S_RUN &&
		    !IFQ_IS_EMPTY(&ifp->if_snd))
			an_start(ifp);
	}

	return 1;
}

static int
an_init(struct ifnet *ifp)
{
	struct an_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i, error, fid;

	DPRINTF(("an_init: enabled %d\n", sc->sc_enabled));
	if (!sc->sc_enabled) {
		if (sc->sc_enable)
			(*sc->sc_enable)(sc);
		an_wait(sc);
		sc->sc_enabled = 1;
	} else {
		an_stop(ifp, 0);
		if ((error = an_reset(sc)) != 0) {
			printf("%s: failed to reset\n", ifp->if_xname);
			an_stop(ifp, 1);
			return error;
		}
	}
	CSR_WRITE_2(sc, AN_SW0, AN_MAGIC);

	/* Allocate the TX buffers */
	for (i = 0; i < AN_TX_RING_CNT; i++) {
		if ((error = an_alloc_fid(sc, AN_TX_MAX_LEN, &fid)) != 0) {
			printf("%s: failed to allocate nic memory\n",
			    ifp->if_xname);
			an_stop(ifp, 1);
			return error;
		}
		DPRINTF2(("an_init: txbuf %d allocated %x\n", i, fid));
		sc->sc_txd[i].d_fid = fid;
		sc->sc_txd[i].d_inuse = 0;
	}
	sc->sc_txcur = sc->sc_txnext = 0;

	IEEE80211_ADDR_COPY(sc->sc_config.an_macaddr, ic->ic_myaddr);
	sc->sc_config.an_scanmode = htole16(AN_SCANMODE_ACTIVE);
	sc->sc_config.an_authtype = htole16(AN_AUTHTYPE_OPEN);	/*XXX*/
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		sc->sc_config.an_authtype |=
		    htole16(AN_AUTHTYPE_PRIVACY_IN_USE);
		if (sc->sc_use_leap)
			sc->sc_config.an_authtype |=
			    htole16(AN_AUTHTYPE_LEAP);
	}
	sc->sc_config.an_listen_interval = htole16(ic->ic_lintval);
	sc->sc_config.an_beacon_period = htole16(ic->ic_lintval);
	if (ic->ic_flags & IEEE80211_F_PMGTON)
		sc->sc_config.an_psave_mode = htole16(AN_PSAVE_PSP);
	else
		sc->sc_config.an_psave_mode = htole16(AN_PSAVE_CAM);
	sc->sc_config.an_ds_channel =
	    htole16(ieee80211_chan2ieee(ic, ic->ic_ibss_chan));

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->sc_config.an_opmode =
		    htole16(AN_OPMODE_INFRASTRUCTURE_STATION);
		sc->sc_config.an_rxmode = htole16(AN_RXMODE_BC_MC_ADDR);
		break;
	case IEEE80211_M_IBSS:
		sc->sc_config.an_opmode = htole16(AN_OPMODE_IBSS_ADHOC);
		sc->sc_config.an_rxmode = htole16(AN_RXMODE_BC_MC_ADDR);
		break;
	case IEEE80211_M_MONITOR:
		sc->sc_config.an_opmode =
		    htole16(AN_OPMODE_INFRASTRUCTURE_STATION);
		sc->sc_config.an_rxmode =
		    htole16(AN_RXMODE_80211_MONITOR_ANYBSS);
		sc->sc_config.an_authtype = htole16(AN_AUTHTYPE_NONE);
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			sc->sc_config.an_authtype |=
			    htole16(AN_AUTHTYPE_PRIVACY_IN_USE |
		            AN_AUTHTYPE_ALLOW_UNENCRYPTED);
		break;
	default:
		printf("%s: bad opmode %d\n", ifp->if_xname, ic->ic_opmode);
		an_stop(ifp, 1);
		return EIO;
	}
	sc->sc_config.an_rxmode |= htole16(AN_RXMODE_NO_8023_HEADER);

	/* Set the ssid list */
	memset(&sc->sc_buf, 0, sizeof(sc->sc_buf.sc_ssidlist));
	sc->sc_buf.sc_ssidlist.an_entry[0].an_ssid_len =
	    htole16(ic->ic_des_esslen);
	if (ic->ic_des_esslen)
		memcpy(sc->sc_buf.sc_ssidlist.an_entry[0].an_ssid,
		    ic->ic_des_essid, ic->ic_des_esslen);
	if ((error = an_write_rid(sc, AN_RID_SSIDLIST, &sc->sc_buf,
	    sizeof(sc->sc_buf.sc_ssidlist))) != 0) {
		printf("%s: failed to write ssid list\n", ifp->if_xname);
		an_stop(ifp, 1);
		return error;
	}

	/* Set the AP list */
	memset(&sc->sc_buf, 0, sizeof(sc->sc_buf.sc_aplist));
	(void)an_write_rid(sc, AN_RID_APLIST, &sc->sc_buf,
	    sizeof(sc->sc_buf.sc_aplist));

	/* Set the encapsulation */
	for (i = 0; i < AN_ENCAP_NENTS; i++) {
		sc->sc_buf.sc_encap.an_entry[i].an_ethertype = htole16(0);
		sc->sc_buf.sc_encap.an_entry[i].an_action =
		    htole16(AN_RXENCAP_RFC1024 | AN_TXENCAP_RFC1024);
	}
	(void)an_write_rid(sc, AN_RID_ENCAP, &sc->sc_buf,
	    sizeof(sc->sc_buf.sc_encap));

	/* Set the WEP Keys */
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		an_write_wepkey(sc, AN_RID_WEP_VOLATILE, sc->sc_wepkeys,
		    sc->sc_tx_key);

	/* Set the configuration */
#ifdef AN_DEBUG
	if (an_debug) {
		printf("write config:\n");
		for (i = 0; i < sizeof(sc->sc_config) / 2; i++)
			printf(" %04x", ((u_int16_t *)&sc->sc_config)[i]);
		printf("\n");
	}
#endif
	if ((error = an_write_rid(sc, AN_RID_GENCONFIG, &sc->sc_config,
	    sizeof(sc->sc_config))) != 0) {
		printf("%s: failed to write config\n", ifp->if_xname);
		an_stop(ifp, 1);
		return error;
	}

	/* Enable the MAC */
	if (an_cmd(sc, AN_CMD_ENABLE, 0)) {
		aprint_error_dev(sc->sc_dev, "failed to enable MAC\n");
		an_stop(ifp, 1);
		return ENXIO;
	}
	if (ifp->if_flags & IFF_PROMISC)
		an_cmd(sc, AN_CMD_SET_MODE, 0xffff);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ic->ic_state = IEEE80211_S_INIT;
	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	/* enable interrupts */
	CSR_WRITE_2(sc, AN_INT_EN, AN_INTRS);
	return 0;
}

static void
an_stop(struct ifnet *ifp, int disable)
{
	struct an_softc *sc = ifp->if_softc;
	int i, s;

	if (!sc->sc_enabled)
		return;

	DPRINTF(("an_stop: disable %d\n", disable));

	s = splnet();
	ieee80211_new_state(&sc->sc_ic, IEEE80211_S_INIT, -1);
	if (device_is_active(sc->sc_dev)) {
		an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0);
		CSR_WRITE_2(sc, AN_INT_EN, 0);
		an_cmd(sc, AN_CMD_DISABLE, 0);

		for (i = 0; i < AN_TX_RING_CNT; i++)
			an_cmd(sc, AN_CMD_DEALLOC_MEM, sc->sc_txd[i].d_fid);
	}

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	if (disable) {
		if (sc->sc_disable)
			(*sc->sc_disable)(sc);
		sc->sc_enabled = 0;
	}
	splx(s);
}

static void
an_start(struct ifnet *ifp)
{
	struct an_softc *sc = (struct an_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct an_txframe frmhdr;
	struct ether_header *eh;
	struct mbuf *m;
	u_int16_t len;
	int cur, fid;

	if (!sc->sc_enabled || !device_is_active(sc->sc_dev)) {
		DPRINTF(("an_start: noop: enabled %d invalid %d\n",
		    sc->sc_enabled, !device_is_active(sc->sc_dev)));
		return;
	}

	memset(&frmhdr, 0, sizeof(frmhdr));
	cur = sc->sc_txnext;
	for (;;) {
		if (ic->ic_state != IEEE80211_S_RUN) {
			DPRINTF(("an_start: not running %d\n", ic->ic_state));
			break;
		}
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL) {
			DPRINTF2(("an_start: no pending mbuf\n"));
			break;
		}
		if (sc->sc_txd[cur].d_inuse) {
			DPRINTF2(("an_start: %x/%d busy\n",
			    sc->sc_txd[cur].d_fid, cur));
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		IFQ_DEQUEUE(&ifp->if_snd, m);
		ifp->if_opackets++;
		bpf_mtap(ifp, m);
		eh = mtod(m, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			/* NB: ieee80211_find_txnode does stat+msg */
			goto bad;
		}
		if ((m = ieee80211_encap(ic, m, ni)) == NULL)
			goto bad;
		ieee80211_free_node(ni);
		bpf_mtap3(ic->ic_rawbpf, m);

		wh = mtod(m, struct ieee80211_frame *);
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			wh->i_fc[1] |= IEEE80211_FC1_WEP;
		m_copydata(m, 0, sizeof(struct ieee80211_frame),
		    (void *)&frmhdr.an_whdr);

		/* insert payload length in front of llc/snap */
		len = htons(m->m_pkthdr.len - sizeof(struct ieee80211_frame));
		m_adj(m, sizeof(struct ieee80211_frame) - sizeof(len));
		if (mtod(m, u_long) & 0x01)
			memcpy(mtod(m, void *), &len, sizeof(len));
		else
			*mtod(m, u_int16_t *) = len;

		/*
		 * XXX Aironet firmware apparently convert the packet
		 * with longer than 1500 bytes in length into LLC/SNAP.
		 * If we have 1500 bytes in ethernet payload, it is
		 * 1508 bytes including LLC/SNAP and will be inserted
		 * additional LLC/SNAP header with 1501-1508 in its
		 * ethertype !!
		 * So we skip LLC/SNAP header and force firmware to
		 * convert it to LLC/SNAP again.
		 */
		m_adj(m, sizeof(struct llc));

		frmhdr.an_tx_ctl = htole16(AN_TXCTL_80211);
		frmhdr.an_tx_payload_len = htole16(m->m_pkthdr.len);
		frmhdr.an_gaplen = htole16(AN_TXGAP_802_11);

		if (ic->ic_fixed_rate != -1)
			frmhdr.an_tx_rate =
			    ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[
			    ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		else
			frmhdr.an_tx_rate = 0;

		/* XXX radiotap for tx must be completed */
		if (sc->sc_drvbpf) {
			struct an_tx_radiotap_header *tap = &sc->sc_txtap;
			tap->at_rate = ic->ic_bss->ni_rates.rs_rates[ic->ic_bss->ni_txrate];
			tap->at_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
			tap->at_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
			/* TBD tap->wt_flags */
			bpf_mtap2(sc->sc_drvbpf, tap, tap->at_ihdr.it_len, m);
		}

#ifdef AN_DEBUG
		if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			ieee80211_dump_pkt((u_int8_t *)&frmhdr.an_whdr,
			    sizeof(struct ieee80211_frame), -1, 0);
			printf(" txctl 0x%x plen %u\n",
			    le16toh(frmhdr.an_tx_ctl),
			    le16toh(frmhdr.an_tx_payload_len));
		}
#endif
		if (sizeof(frmhdr) + AN_TXGAP_802_11 + sizeof(len) +
		    m->m_pkthdr.len > AN_TX_MAX_LEN)
			goto bad;

		fid = sc->sc_txd[cur].d_fid;
		if (an_write_bap(sc, fid, 0, &frmhdr, sizeof(frmhdr)) != 0)
			goto bad;
		/* dummy write to avoid seek. */
		an_write_bap(sc, fid, -1, &frmhdr, AN_TXGAP_802_11);
		an_mwrite_bap(sc, fid, -1, m, m->m_pkthdr.len);
		m_freem(m);

		DPRINTF2(("an_start: send %zu byte via %x/%d\n",
		    ntohs(len) + sizeof(struct ieee80211_frame),
		    fid, cur));
		sc->sc_txd[cur].d_inuse = 1;
		if (an_cmd(sc, AN_CMD_TX, fid)) {
			printf("%s: xmit failed\n", ifp->if_xname);
			sc->sc_txd[cur].d_inuse = 0;
			continue;
		}
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
		AN_INC(cur, AN_TX_RING_CNT);
		sc->sc_txnext = cur;
		continue;
bad:
		ifp->if_oerrors++;
		m_freem(m);
	}
}

static int
an_reset(struct an_softc *sc)
{

	DPRINTF(("an_reset\n"));

	if (!sc->sc_enabled)
		return ENXIO;

	an_cmd(sc, AN_CMD_ENABLE, 0);
	an_cmd(sc, AN_CMD_FW_RESTART, 0);
	an_cmd(sc, AN_CMD_NOOP2, 0);

	if (an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0) == ETIMEDOUT) {
		aprint_error_dev(sc->sc_dev, "reset failed\n");
		return ETIMEDOUT;
	}

	an_cmd(sc, AN_CMD_DISABLE, 0);
	return 0;
}

static void
an_watchdog(struct ifnet *ifp)
{
	struct an_softc *sc = ifp->if_softc;

	if (!sc->sc_enabled)
		return;

	if (sc->sc_tx_timer) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", ifp->if_xname);
			ifp->if_oerrors++;
			an_init(ifp);
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(&sc->sc_ic);
}

static int
an_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct an_softc *sc = ifp->if_softc;
	int s, error = 0;

	if (!device_is_active(sc->sc_dev))
		return ENXIO;

	s = splnet();

	switch (command) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (sc->sc_enabled) {
				/*
				 * To avoid rescanning another access point,
				 * do not call an_init() here.  Instead, only
				 * reflect promisc mode settings.
				 */
				error = an_cmd(sc, AN_CMD_SET_MODE,
				    (ifp->if_flags & IFF_PROMISC) ? 0xffff : 0);
			} else
				error = an_init(ifp);
		} else if (sc->sc_enabled)
			an_stop(ifp, 1);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = ether_ioctl(ifp, command, data);
		if (error == ENETRESET) {
			/* we don't have multicast filter. */
			error = 0;
		}
		break;
	case SIOCS80211NWKEY:
		error = an_set_nwkey(sc, (struct ieee80211_nwkey *)data);
			break;
	case SIOCG80211NWKEY:
		error = an_get_nwkey(sc, (struct ieee80211_nwkey *)data);
		break;
	default:
		error = ieee80211_ioctl(&sc->sc_ic, command, data);
		break;
	}
	if (error == ENETRESET) {
		if (sc->sc_enabled)
			error = an_init(ifp);
		else
			error = 0;
	}
	splx(s);
	return error;
}

/* TBD factor with ieee80211_media_change */
static int
an_media_change(struct ifnet *ifp)
{
	struct an_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifmedia_entry *ime;
	enum ieee80211_opmode newmode;
	int i, rate, error = 0;

	ime = ic->ic_media.ifm_cur;
	if (IFM_SUBTYPE(ime->ifm_media) == IFM_AUTO) {
		i = -1;
	} else {
		struct ieee80211_rateset *rs =
		    &ic->ic_sup_rates[IEEE80211_MODE_11B];
		rate = ieee80211_media2rate(ime->ifm_media);
		if (rate == 0)
			return EINVAL;
		for (i = 0; i < rs->rs_nrates; i++) {
			if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == rate)
				break;
		}
		if (i == rs->rs_nrates)
			return EINVAL;
	}
	if (ic->ic_fixed_rate != i) {
		ic->ic_fixed_rate = i;
		error = ENETRESET;
	}

	if (ime->ifm_media & IFM_IEEE80211_ADHOC)
		newmode = IEEE80211_M_IBSS;
	else if (ime->ifm_media & IFM_IEEE80211_HOSTAP)
		newmode = IEEE80211_M_HOSTAP;
	else if (ime->ifm_media & IFM_IEEE80211_MONITOR)
		newmode = IEEE80211_M_MONITOR;
	else
		newmode = IEEE80211_M_STA;
	if (ic->ic_opmode != newmode) {
		ic->ic_opmode = newmode;
		error = ENETRESET;
	}
	if (error == ENETRESET) {
		if (sc->sc_enabled)
			error = an_init(ifp);
		else
			error = 0;
	}
	ifp->if_baudrate = ifmedia_baudrate(ic->ic_media.ifm_cur->ifm_media);

	return error;
}

static void
an_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct an_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int rate, buflen;

	if (sc->sc_enabled == 0) {
		imr->ifm_active = IFM_IEEE80211 | IFM_NONE;
		imr->ifm_status = 0;
		return;
	}

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;
	buflen = sizeof(sc->sc_buf);
	if (ic->ic_fixed_rate != -1)
		rate = ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[
		    ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
	else if (an_read_rid(sc, AN_RID_STATUS, &sc->sc_buf, &buflen) != 0)
		rate = 0;
	else
		rate = le16toh(sc->sc_buf.sc_status.an_current_tx_rate);
	imr->ifm_active |= ieee80211_rate2media(ic, rate, IEEE80211_MODE_11B);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_HOSTAP:
		imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;
	default:
		break;
	}
}

static int
an_set_nwkey(struct an_softc *sc, struct ieee80211_nwkey *nwkey)
{
	int error;
	struct ieee80211com *ic = &sc->sc_ic;
	u_int16_t prevauth;

	error = 0;
	prevauth = sc->sc_config.an_authtype;

	switch (nwkey->i_wepon) {
	case IEEE80211_NWKEY_OPEN:
		sc->sc_config.an_authtype = AN_AUTHTYPE_OPEN;
		ic->ic_flags &= ~IEEE80211_F_PRIVACY;
		break;

	case IEEE80211_NWKEY_WEP:
	case IEEE80211_NWKEY_WEP | IEEE80211_NWKEY_PERSIST:
		error = an_set_nwkey_wep(sc, nwkey);
		if (error == 0 || error == ENETRESET) {
			sc->sc_config.an_authtype =
			    AN_AUTHTYPE_OPEN | AN_AUTHTYPE_PRIVACY_IN_USE;
			ic->ic_flags |= IEEE80211_F_PRIVACY;
		}
		break;

	case IEEE80211_NWKEY_EAP:
		error = an_set_nwkey_eap(sc, nwkey);
		if (error == 0 || error == ENETRESET) {
			sc->sc_config.an_authtype = AN_AUTHTYPE_OPEN |
			    AN_AUTHTYPE_PRIVACY_IN_USE | AN_AUTHTYPE_LEAP;
			ic->ic_flags |= IEEE80211_F_PRIVACY;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0 && prevauth != sc->sc_config.an_authtype)
		error = ENETRESET;
	return error;
}

static int
an_set_nwkey_wep(struct an_softc *sc, struct ieee80211_nwkey *nwkey)
{
	int i, txkey, anysetkey, needreset, error;
	struct an_wepkey keys[IEEE80211_WEP_NKID];

	error = 0;
	memset(keys, 0, sizeof(keys));
	anysetkey = needreset = 0;

	/* load argument and sanity check */
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		keys[i].an_wep_keylen = nwkey->i_key[i].i_keylen;
		if (keys[i].an_wep_keylen < 0)
			continue;
		if (keys[i].an_wep_keylen != 0 &&
		    keys[i].an_wep_keylen < IEEE80211_WEP_KEYLEN)
			return EINVAL;
		if (keys[i].an_wep_keylen > sizeof(keys[i].an_wep_key))
			return EINVAL;
		if ((error = copyin(nwkey->i_key[i].i_keydat,
		    keys[i].an_wep_key, keys[i].an_wep_keylen)) != 0)
			return error;
		anysetkey++;
	}
	txkey = nwkey->i_defkid - 1;
	if (txkey >= 0) {
		if (txkey >= IEEE80211_WEP_NKID)
			return EINVAL;
		/* default key must have a valid value */
		if (keys[txkey].an_wep_keylen == 0 ||
		    (keys[txkey].an_wep_keylen < 0 &&
		    sc->sc_perskeylen[txkey] == 0))
			return EINVAL;
		anysetkey++;
	}
	DPRINTF(("an_set_nwkey_wep: %s: %sold(%d:%d,%d,%d,%d) "
	    "pers(%d:%d,%d,%d,%d) new(%d:%d,%d,%d,%d)\n",
	    device_xname(sc->sc_dev),
	    ((nwkey->i_wepon & IEEE80211_NWKEY_PERSIST) ? "persist: " : ""),
	    sc->sc_tx_key,
	    sc->sc_wepkeys[0].an_wep_keylen, sc->sc_wepkeys[1].an_wep_keylen,
	    sc->sc_wepkeys[2].an_wep_keylen, sc->sc_wepkeys[3].an_wep_keylen,
	    sc->sc_tx_perskey,
	    sc->sc_perskeylen[0], sc->sc_perskeylen[1],
	    sc->sc_perskeylen[2], sc->sc_perskeylen[3],
	    txkey,
	    keys[0].an_wep_keylen, keys[1].an_wep_keylen,
	    keys[2].an_wep_keylen, keys[3].an_wep_keylen));
	if (!(nwkey->i_wepon & IEEE80211_NWKEY_PERSIST)) {
		/* set temporary keys */
		sc->sc_tx_key = txkey;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (keys[i].an_wep_keylen < 0)
				continue;
			memcpy(&sc->sc_wepkeys[i], &keys[i], sizeof(keys[i]));
		}
	} else {
		/* set persist keys */
		if (anysetkey) {
			/* prepare to write nvram */
			if (!sc->sc_enabled) {
				if (sc->sc_enable)
					(*sc->sc_enable)(sc);
				an_wait(sc);
				sc->sc_enabled = 1;
				error = an_write_wepkey(sc,
				    AN_RID_WEP_PERSISTENT, keys, txkey);
				if (sc->sc_disable)
					(*sc->sc_disable)(sc);
				sc->sc_enabled = 0;
			} else {
				an_cmd(sc, AN_CMD_DISABLE, 0);
				error = an_write_wepkey(sc,
				    AN_RID_WEP_PERSISTENT, keys, txkey);
				an_cmd(sc, AN_CMD_ENABLE, 0);
			}
			if (error)
				return error;
		}
		if (txkey >= 0)
			sc->sc_tx_perskey = txkey;
		if (sc->sc_tx_key >= 0) {
			sc->sc_tx_key = -1;
			needreset++;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (sc->sc_wepkeys[i].an_wep_keylen >= 0) {
				memset(&sc->sc_wepkeys[i].an_wep_key, 0,
				    sizeof(sc->sc_wepkeys[i].an_wep_key));
				sc->sc_wepkeys[i].an_wep_keylen = -1;
				needreset++;
			}
			if (keys[i].an_wep_keylen >= 0)
				sc->sc_perskeylen[i] = keys[i].an_wep_keylen;
		}
	}
	if (needreset) {
		/* firmware restart to reload persistent key */
		an_reset(sc);
	}
	if (anysetkey || needreset)
		error = ENETRESET;
	return error;
}

static int
an_set_nwkey_eap(struct an_softc *sc, struct ieee80211_nwkey *nwkey)
{
	int i, error, len;
	struct ifnet *ifp = &sc->sc_if;
	struct an_rid_leapkey *key;
	u_int16_t unibuf[sizeof(key->an_key)];
	static const int leap_rid[] = { AN_RID_LEAP_PASS, AN_RID_LEAP_USER };
	MD4_CTX ctx;

	error = 0;

	if (nwkey->i_key[0].i_keydat == NULL &&
	    nwkey->i_key[1].i_keydat == NULL)
		return 0;
	if (!sc->sc_enabled)
		return ENXIO;
	an_cmd(sc, AN_CMD_DISABLE, 0);
	key = &sc->sc_buf.sc_leapkey;
	for (i = 0; i < 2; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		len = nwkey->i_key[i].i_keylen;
		if (len > sizeof(key->an_key))
			return EINVAL;
		memset(key, 0, sizeof(*key));
		key->an_key_len = htole16(len);
		if ((error = copyin(nwkey->i_key[i].i_keydat, key->an_key,
		    len)) != 0)
			return error;
		if (i == 1) {
			/*
			 * Cisco seems to use PasswordHash and PasswordHashHash
			 * in RFC-2759 (MS-CHAP-V2).
			 */
			memset(unibuf, 0, sizeof(unibuf));
			/* XXX: convert password to unicode */
			for (i = 0; i < len; i++)
				unibuf[i] = key->an_key[i];
			/* set PasswordHash */
			MD4Init(&ctx);
			MD4Update(&ctx, (u_int8_t *)unibuf, len * 2);
			MD4Final(key->an_key, &ctx);
			/* set PasswordHashHash */
			MD4Init(&ctx);
			MD4Update(&ctx, key->an_key, 16);
			MD4Final(key->an_key + 16, &ctx);
			key->an_key_len = htole16(32);
		}
		if ((error = an_write_rid(sc, leap_rid[i], key,
		    sizeof(*key))) != 0) {
			printf("%s: LEAP set failed\n", ifp->if_xname);
			return error;
		}
	}
	error = an_cmd(sc, AN_CMD_ENABLE, 0);
	if (error)
		printf("%s: an_set_nwkey: failed to enable MAC\n",
		    ifp->if_xname);
	else
		error = ENETRESET;
	return error;
}

static int
an_get_nwkey(struct an_softc *sc, struct ieee80211_nwkey *nwkey)
{
	int i, error;

	error = 0;
	if (sc->sc_config.an_authtype & AN_AUTHTYPE_LEAP)
		nwkey->i_wepon = IEEE80211_NWKEY_EAP;
	else if (sc->sc_config.an_authtype & AN_AUTHTYPE_PRIVACY_IN_USE)
		nwkey->i_wepon = IEEE80211_NWKEY_WEP;
	else
		nwkey->i_wepon = IEEE80211_NWKEY_OPEN;
	if (sc->sc_tx_key == -1)
		nwkey->i_defkid = sc->sc_tx_perskey + 1;
	else
		nwkey->i_defkid = sc->sc_tx_key + 1;
	if (nwkey->i_key[0].i_keydat == NULL)
		return 0;
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		/* do not show any keys to non-root user */
		/* XXX-elad: why is this inside a loop? */
		if ((error = kauth_authorize_network(curlwp->l_cred,
		    KAUTH_NETWORK_INTERFACE,
		    KAUTH_REQ_NETWORK_INTERFACE_GETPRIV, sc->sc_ic.ic_ifp,
		    KAUTH_ARG(SIOCG80211NWKEY), NULL)) != 0)
			break;
		nwkey->i_key[i].i_keylen = sc->sc_wepkeys[i].an_wep_keylen;
		if (nwkey->i_key[i].i_keylen < 0) {
			if (sc->sc_perskeylen[i] == 0)
				nwkey->i_key[i].i_keylen = 0;
			continue;
		}
		if ((error = copyout(sc->sc_wepkeys[i].an_wep_key,
		    nwkey->i_key[i].i_keydat,
		    sc->sc_wepkeys[i].an_wep_keylen)) != 0)
			break;
	}
	return error;
}

static int
an_write_wepkey(struct an_softc *sc, int type, struct an_wepkey *keys, int kid)
{
	int i, error;
	struct an_rid_wepkey *akey;

	error = 0;
	akey = &sc->sc_buf.sc_wepkey;
	memset(akey, 0, sizeof(struct an_rid_wepkey));
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (keys[i].an_wep_keylen < 0 ||
		    keys[i].an_wep_keylen > sizeof(akey->an_key))
			continue;
		akey->an_key_len = htole16(keys[i].an_wep_keylen);
		akey->an_key_index = htole16(i);
		akey->an_mac_addr[0] = 1;	/* default mac */
		memcpy(akey->an_key, keys[i].an_wep_key, keys[i].an_wep_keylen);
		if ((error = an_write_rid(sc, type, akey, sizeof(*akey))) != 0)
			return error;
	}
	if (kid >= 0) {
		akey->an_key_index = htole16(0xffff);
		akey->an_mac_addr[0] = kid;
		akey->an_key_len = htole16(0);
		memset(akey->an_key, 0, sizeof(akey->an_key));
		error = an_write_rid(sc, type, akey, sizeof(*akey));
	}
	return error;
}

#ifdef AN_DEBUG
static void
an_dump_pkt(const char *devname, struct mbuf *m)
{
	int col, col0, i;
	uint8_t *pkt = mtod(m, uint8_t *);
	const char *delim = "";
	int delimw = 0;

	printf("%s: pkt ", devname);
	col = col0 = strlen(devname) + strlen(": pkt ");
	for (i = 0; i < m->m_len; i++) {
		printf("%s%02x", delim, pkt[i]);
		delim = ":";
		delimw = 1;
		col += delimw + 2;
		if (col >= 72) {
			printf("\n%*s", col0, "");
			col = col0;
			delim = "";
			delimw = 0;
		}
	}
	if (col != 0)
		printf("\n");
}
#endif /* AN_DEBUG */

/*
 * Low level functions
 */

static void
an_rx_intr(struct an_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &sc->sc_if;
	struct ieee80211_frame_min *wh;
	struct ieee80211_node *ni;
	struct an_rxframe frmhdr;
	struct mbuf *m;
	u_int16_t status;
	int fid, gaplen, len, off;
	uint8_t *gap;

	fid = CSR_READ_2(sc, AN_RX_FID);

	/* First read in the frame header */
	if (an_read_bap(sc, fid, 0, &frmhdr, sizeof(frmhdr)) != 0) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("an_rx_intr: read fid %x failed\n", fid));
		return;
	}

#ifdef AN_DEBUG
	if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2)) {
		ieee80211_dump_pkt((u_int8_t *)&frmhdr.an_whdr,
		    sizeof(struct ieee80211_frame), frmhdr.an_rx_rate,
		    frmhdr.an_rx_signal_strength);
		printf(" time 0x%x status 0x%x plen %u chan %u"
		    " plcp %02x %02x %02x %02x gap %u\n",
		    le32toh(frmhdr.an_rx_time), le16toh(frmhdr.an_rx_status),
		    le16toh(frmhdr.an_rx_payload_len), frmhdr.an_rx_chan,
		    frmhdr.an_plcp_hdr[0], frmhdr.an_plcp_hdr[1],
		    frmhdr.an_plcp_hdr[2], frmhdr.an_plcp_hdr[3],
		    le16toh(frmhdr.an_gaplen));
	}
#endif

	status = le16toh(frmhdr.an_rx_status);
	if ((status & AN_STAT_ERRSTAT) != 0 &&
	    ic->ic_opmode != IEEE80211_M_MONITOR) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("an_rx_intr: fid %x status %x\n", fid, status));
		return;
	}

	/* the payload length field includes a 16-bit "mystery field" */
	len = le16toh(frmhdr.an_rx_payload_len) - sizeof(uint16_t);
	off = ALIGN(sizeof(struct ieee80211_frame));

	if (off + len > MCLBYTES) {
		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
			ifp->if_ierrors++;
			DPRINTF(("an_rx_intr: oversized packet %d\n", len));
			return;
		}
		len = 0;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
		ifp->if_ierrors++;
		DPRINTF(("an_rx_intr: MGET failed\n"));
		return;
	}
	if (off + len + AN_GAPLEN_MAX > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
			m_freem(m);
			ifp->if_ierrors++;
			DPRINTF(("an_rx_intr: MCLGET failed\n"));
			return;
		}
	}
	m->m_data += off - sizeof(struct ieee80211_frame);

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		gaplen = le16toh(frmhdr.an_gaplen);
		if (gaplen > AN_GAPLEN_MAX) {
			CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
			m_freem(m);
			ifp->if_ierrors++;
			DPRINTF(("%s: gap too long\n", __func__));
			return;
		}
		/*
		 * We don't need the 16-bit mystery field (payload length?),
		 * so read it into the region reserved for the 802.11 header.
		 *
		 * When Cisco Aironet 350 cards w/ firmware version 5 or
		 * greater operate with certain Cisco 350 APs,
		 * the "gap" is filled with the SNAP header.  Read
		 * it in after the 802.11 header.
		 */
		gap = m->m_data + sizeof(struct ieee80211_frame) -
		    sizeof(uint16_t);
		an_read_bap(sc, fid, -1, gap, gaplen + sizeof(u_int16_t));
#ifdef AN_DEBUG
		if ((ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			int i;
			printf(" gap&len");
			for (i = 0; i < gaplen + sizeof(u_int16_t); i++)
				printf(" %02x", gap[i]);
			printf("\n");
		}
#endif
	} else
		gaplen = 0;

	an_read_bap(sc, fid, -1,
	    m->m_data + sizeof(struct ieee80211_frame) + gaplen, len);
	m->m_pkthdr.len = m->m_len = sizeof(struct ieee80211_frame) + gaplen +
	    len;

	memcpy(m->m_data, &frmhdr.an_whdr, sizeof(struct ieee80211_frame));
	m->m_pkthdr.rcvif = ifp;
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);

	if (sc->sc_drvbpf) {
		struct an_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->ar_rate = frmhdr.an_rx_rate;
		tap->ar_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
		tap->ar_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->ar_antsignal = frmhdr.an_rx_signal_strength;
		if ((le16toh(frmhdr.an_rx_status) & AN_STAT_BADCRC) ||
		    (le16toh(frmhdr.an_rx_status) & AN_STAT_ERRSTAT) ||
		    (le16toh(frmhdr.an_rx_status) & AN_STAT_UNDECRYPTABLE))
		    tap->ar_flags |= IEEE80211_RADIOTAP_F_BADFCS;

		bpf_mtap2(sc->sc_drvbpf, tap, tap->ar_ihdr.it_len, m);
	}
	wh = mtod(m, struct ieee80211_frame_min *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		/*
		 * WEP is decrypted by hardware. Clear WEP bit
		 * header for ieee80211_input().
		 */
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
	}

#ifdef AN_DEBUG
	if (an_debug > 1)
		an_dump_pkt(device_xname(sc->sc_dev), m);
#endif /* AN_DEBUG */

	ni = ieee80211_find_rxnode(ic, wh);
	ieee80211_input(ic, m, ni, frmhdr.an_rx_signal_strength,
	    le32toh(frmhdr.an_rx_time));
	ieee80211_free_node(ni);
}

static void
an_tx_intr(struct an_softc *sc, int status)
{
	struct ifnet *ifp = &sc->sc_if;
	int cur, fid;

	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	fid = CSR_READ_2(sc, AN_TX_CMP_FID);
	CSR_WRITE_2(sc, AN_EVENT_ACK, status & (AN_EV_TX | AN_EV_TX_EXC));

	if (status & AN_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	cur = sc->sc_txcur;
	if (sc->sc_txd[cur].d_fid == fid) {
		sc->sc_txd[cur].d_inuse = 0;
		DPRINTF2(("an_tx_intr: sent %x/%d\n", fid, cur));
		AN_INC(cur, AN_TX_RING_CNT);
		sc->sc_txcur = cur;
	} else {
		for (cur = 0; cur < AN_TX_RING_CNT; cur++) {
			if (fid == sc->sc_txd[cur].d_fid) {
				sc->sc_txd[cur].d_inuse = 0;
				break;
			}
		}
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: tx mismatch: "
			    "expected %x(%d), actual %x(%d)\n",
			    device_xname(sc->sc_dev),
			    sc->sc_txd[sc->sc_txcur].d_fid, sc->sc_txcur,
			    fid, cur);
	}

	return;
}

static void
an_linkstat_intr(struct an_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int16_t status;

	status = CSR_READ_2(sc, AN_LINKSTAT);
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_LINKSTAT);
	DPRINTF(("an_linkstat_intr: status 0x%x\n", status));

	if (status == AN_LINKSTAT_ASSOCIATED) {
		if (ic->ic_state != IEEE80211_S_RUN ||
		    ic->ic_opmode == IEEE80211_M_IBSS)
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	} else {
		if (ic->ic_opmode == IEEE80211_M_STA)
			ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	}
}

/* Must be called at proper protection level! */
static int
an_cmd(struct an_softc *sc, int cmd, int val)
{
	int i, status;

	/* make sure that previous command completed */
	if (CSR_READ_2(sc, AN_COMMAND) & AN_CMD_BUSY) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
			printf("%s: command 0x%x busy\n", device_xname(sc->sc_dev),
			    CSR_READ_2(sc, AN_COMMAND));
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CLR_STUCK_BUSY);
	}

	CSR_WRITE_2(sc, AN_PARAM0, val);
	CSR_WRITE_2(sc, AN_PARAM1, 0);
	CSR_WRITE_2(sc, AN_PARAM2, 0);
	CSR_WRITE_2(sc, AN_COMMAND, cmd);

	if (cmd == AN_CMD_FW_RESTART) {
		/* XXX: should sleep here */
		DELAY(100*1000);
	}

	for (i = 0; i < AN_TIMEOUT; i++) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_CMD)
			break;
		DELAY(10);
	}

	status = CSR_READ_2(sc, AN_STATUS);

	/* clear stuck command busy if necessary */
	if (CSR_READ_2(sc, AN_COMMAND) & AN_CMD_BUSY)
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CLR_STUCK_BUSY);

	/* Ack the command */
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CMD);

	if (i == AN_TIMEOUT) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
			printf("%s: command 0x%x param 0x%x timeout\n",
			    device_xname(sc->sc_dev), cmd, val);
		return ETIMEDOUT;
	}
	if (status & AN_STAT_CMD_RESULT) {
		if (sc->sc_if.if_flags & IFF_DEBUG)
			printf("%s: command 0x%x param 0x%x status 0x%x "
			    "resp 0x%x 0x%x 0x%x\n",
			    device_xname(sc->sc_dev), cmd, val, status,
			    CSR_READ_2(sc, AN_RESP0), CSR_READ_2(sc, AN_RESP1),
			    CSR_READ_2(sc, AN_RESP2));
		return EIO;
	}

	return 0;
}


/*
 * Wait for firmware come up after power enabled.
 */
static void
an_wait(struct an_softc *sc)
{
	int i;

	CSR_WRITE_2(sc, AN_COMMAND, AN_CMD_NOOP2);
	for (i = 0; i < 3*hz; i++) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_CMD)
			break;
		(void)tsleep(sc, PWAIT, "anatch", 1);
	}
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CMD);
}

static int
an_seek_bap(struct an_softc *sc, int id, int off)
{
	int i, status;

	CSR_WRITE_2(sc, AN_SEL0, id);
	CSR_WRITE_2(sc, AN_OFF0, off);

	for (i = 0; ; i++) {
		status = CSR_READ_2(sc, AN_OFF0);
		if ((status & AN_OFF_BUSY) == 0)
			break;
		if (i == AN_TIMEOUT) {
			printf("%s: timeout in an_seek_bap to 0x%x/0x%x\n",
			    device_xname(sc->sc_dev), id, off);
			sc->sc_bap_off = AN_OFF_ERR;	/* invalidate */
			return ETIMEDOUT;
		}
		DELAY(10);
	}
	if (status & AN_OFF_ERR) {
		aprint_error_dev(sc->sc_dev, "failed in an_seek_bap to 0x%x/0x%x\n",
		    id, off);
		sc->sc_bap_off = AN_OFF_ERR;	/* invalidate */
		return EIO;
	}
	sc->sc_bap_id = id;
	sc->sc_bap_off = off;
	return 0;
}

static int
an_read_bap(struct an_softc *sc, int id, int off, void *buf, int buflen)
{
	int error, cnt;

	if (buflen == 0)
		return 0;
	if (off == -1)
		off = sc->sc_bap_off;
	if (id != sc->sc_bap_id || off != sc->sc_bap_off) {
		if ((error = an_seek_bap(sc, id, off)) != 0)
			return EIO;
	}

	cnt = (buflen + 1) / 2;
	CSR_READ_MULTI_STREAM_2(sc, AN_DATA0, (u_int16_t *)buf, cnt);
	sc->sc_bap_off += cnt * 2;
	return 0;
}

static int
an_write_bap(struct an_softc *sc, int id, int off, void *buf, int buflen)
{
	int error, cnt;

	if (buflen == 0)
		return 0;
	if (off == -1)
		off = sc->sc_bap_off;
	if (id != sc->sc_bap_id || off != sc->sc_bap_off) {
		if ((error = an_seek_bap(sc, id, off)) != 0)
			return EIO;
	}

	cnt = (buflen + 1) / 2;
	CSR_WRITE_MULTI_STREAM_2(sc, AN_DATA0, (u_int16_t *)buf, cnt);
	sc->sc_bap_off += cnt * 2;
	return 0;
}

static int
an_mwrite_bap(struct an_softc *sc, int id, int off, struct mbuf *m, int totlen)
{
	int error, len, cnt;

	if (off == -1)
		off = sc->sc_bap_off;
	if (id != sc->sc_bap_id || off != sc->sc_bap_off) {
		if ((error = an_seek_bap(sc, id, off)) != 0)
			return EIO;
	}

	for (len = 0; m != NULL; m = m->m_next) {
		if (m->m_len == 0)
			continue;
		len = min(m->m_len, totlen);

		if ((mtod(m, u_long) & 0x1) || (len & 0x1)) {
			m_copydata(m, 0, totlen, (void *)&sc->sc_buf.sc_txbuf);
			cnt = (totlen + 1) / 2;
			CSR_WRITE_MULTI_STREAM_2(sc, AN_DATA0,
			    sc->sc_buf.sc_val, cnt);
			off += cnt * 2;
			break;
		}
		cnt = len / 2;
		CSR_WRITE_MULTI_STREAM_2(sc, AN_DATA0, mtod(m, u_int16_t *),
		    cnt);
		off += len;
		totlen -= len;
	}
	sc->sc_bap_off = off;
	return 0;
}

static int
an_alloc_fid(struct an_softc *sc, int len, int *idp)
{
	int i;

	if (an_cmd(sc, AN_CMD_ALLOC_MEM, len)) {
		aprint_error_dev(sc->sc_dev, "failed to allocate %d bytes on NIC\n",
		    len);
		return ENOMEM;
	}

	for (i = 0; i < AN_TIMEOUT; i++) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_ALLOC)
			break;
		if (i == AN_TIMEOUT) {
			printf("%s: timeout in alloc\n", device_xname(sc->sc_dev));
			return ETIMEDOUT;
		}
		DELAY(10);
	}

	*idp = CSR_READ_2(sc, AN_ALLOC_FID);
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_ALLOC);
	return 0;
}

static int
an_read_rid(struct an_softc *sc, int rid, void *buf, int *buflenp)
{
	int error;
	u_int16_t len;

	/* Tell the NIC to enter record read mode. */
	error = an_cmd(sc, AN_CMD_ACCESS | AN_ACCESS_READ, rid);
	if (error)
		return error;

	/* length in byte, including length itself */
	error = an_read_bap(sc, rid, 0, &len, sizeof(len));
	if (error)
		return error;

	len = le16toh(len) - 2;
	if (*buflenp < len) {
		aprint_error_dev(sc->sc_dev, "record buffer is too small, "
		    "rid=%x, size=%d, len=%d\n",
		    rid, *buflenp, len);
		return ENOSPC;
	}
	*buflenp = len;
	return an_read_bap(sc, rid, sizeof(len), buf, len);
}

static int
an_write_rid(struct an_softc *sc, int rid, void *buf, int buflen)
{
	int error;
	u_int16_t len;

	/* length in byte, including length itself */
	len = htole16(buflen + 2);

	error = an_write_bap(sc, rid, 0, &len, sizeof(len));
	if (error)
		return error;
	error = an_write_bap(sc, rid, sizeof(len), buf, buflen);
	if (error)
		return error;

	return an_cmd(sc, AN_CMD_ACCESS | AN_ACCESS_WRITE, rid);
}

static int
an_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct an_softc *sc = (struct an_softc *)ic->ic_ifp->if_softc;
	struct ieee80211_node *ni = ic->ic_bss;
	int buflen;

	DPRINTF(("an_newstate: %s -> %s\n", ieee80211_state_name[ic->ic_state],
	    ieee80211_state_name[nstate]));

	switch (nstate) {
	case IEEE80211_S_INIT:
		ic->ic_flags &= ~IEEE80211_F_IBSSON;
		return (*sc->sc_newstate)(ic, nstate, arg);

	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		ic->ic_state = nstate; /* NB: skip normal ieee80211 handling */
		return 0;

	case IEEE80211_S_RUN:
		buflen = sizeof(sc->sc_buf);
		an_read_rid(sc, AN_RID_STATUS, &sc->sc_buf, &buflen);
		IEEE80211_ADDR_COPY(ni->ni_bssid,
		    sc->sc_buf.sc_status.an_cur_bssid);
		IEEE80211_ADDR_COPY(ni->ni_macaddr, ni->ni_bssid);
		ni->ni_chan = &ic->ic_channels[
		    le16toh(sc->sc_buf.sc_status.an_cur_channel)];
		ni->ni_esslen = le16toh(sc->sc_buf.sc_status.an_ssidlen);
		if (ni->ni_esslen > IEEE80211_NWID_LEN)
			ni->ni_esslen = IEEE80211_NWID_LEN;	/*XXX*/
		memcpy(ni->ni_essid, sc->sc_buf.sc_status.an_ssid,
		    ni->ni_esslen);
		ni->ni_rates = ic->ic_sup_rates[IEEE80211_MODE_11B];	/*XXX*/
		if (ic->ic_ifp->if_flags & IFF_DEBUG) {
			printf("%s: ", device_xname(sc->sc_dev));
			if (ic->ic_opmode == IEEE80211_M_STA)
				printf("associated ");
			else
				printf("synchronized ");
			printf("with %s ssid ", ether_sprintf(ni->ni_bssid));
			ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
			printf(" channel %u start %uMb\n",
			    le16toh(sc->sc_buf.sc_status.an_cur_channel),
			    le16toh(sc->sc_buf.sc_status.an_current_tx_rate)/2);
		}
		break;

	default:
		break;
	}
	return (*sc->sc_newstate)(ic, nstate, arg);
}

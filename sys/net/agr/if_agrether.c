/*	$NetBSD: if_agrether.c,v 1.9 2011/10/19 01:49:50 dyoung Exp $	*/

/*-
 * Copyright (c)2005 YAMAMOTO Takashi,
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_agrether.c,v 1.9 2011/10/19 01:49:50 dyoung Exp $");

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <net/agr/if_agrvar_impl.h>
#include <net/agr/if_agrethervar.h>
#include <net/agr/if_agrsubr.h>

#include <net/agr/ieee8023_slowprotocols.h>
#include <net/agr/ieee8023_tlv.h>
#include <net/agr/ieee8023ad.h>
#include <net/agr/ieee8023ad_lacp.h>
#include <net/agr/ieee8023ad_lacp_impl.h>
#include <net/agr/ieee8023ad_impl.h>

static int agrether_ctor(struct agr_softc *, struct ifnet *);
static void agrether_dtor(struct agr_softc *);
static int agrether_portinit(struct agr_softc *, struct agr_port *);
static int agrether_portfini(struct agr_softc *, struct agr_port *);
static struct agr_port *agrether_select_tx_port(struct agr_softc *,
    struct mbuf *);
static int agrether_configmulti_port(struct agr_softc *, struct agr_port *,
    bool);
static int agrether_configmulti_ifreq(struct agr_softc *, struct ifreq *,
    bool);

const struct agr_iftype_ops agrether_ops = {
	.iftop_tick = NULL,
	.iftop_porttick = ieee8023ad_lacp_porttick,
	.iftop_portstate = ieee8023ad_lacp_portstate,
	.iftop_ctor = agrether_ctor,
	.iftop_dtor = agrether_dtor,
	.iftop_portinit = agrether_portinit,
	.iftop_portfini = agrether_portfini,
	.iftop_hashmbuf = agrether_hashmbuf,
	.iftop_select_tx_port = agrether_select_tx_port,
	.iftop_configmulti_port = agrether_configmulti_port,
	.iftop_configmulti_ifreq = agrether_configmulti_ifreq
};

struct agrether_private {
	struct ieee8023ad_softc aep_ieee8023ad_softc;
	struct agr_multiaddrs aep_multiaddrs;
};

struct agrether_port_private {
	struct ieee8023ad_port aepp_ieee8023ad_port;
};

static int
agrether_ctor(struct agr_softc *sc, struct ifnet *ifp_port)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ethercom *ec = (void *)ifp;
	struct agrether_private *priv;

	priv = malloc(sizeof(*priv), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!priv)
		return ENOMEM;

	agr_mc_init(sc, &priv->aep_multiaddrs);

	sc->sc_iftprivate = priv;
	/*
	 * inherit ports capabilities
	 * XXX this really needs to be the intersection of all
	 * ports capabilities, not just the latest port.
	 * Okay if ports are the same.
	 */
	ifp->if_capabilities = ifp_port->if_capabilities &
			(IFCAP_TSOv4 | IFCAP_TSOv6 |
			IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx |
			IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
			IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx |
			IFCAP_CSUM_TCPv6_Tx | IFCAP_CSUM_TCPv6_Rx |
			IFCAP_CSUM_UDPv6_Tx | IFCAP_CSUM_UDPv6_Rx);

	ether_ifattach(ifp, CLLADDR(ifp_port->if_sadl));
	ec->ec_capabilities =
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING | ETHERCAP_JUMBO_MTU;

	ieee8023ad_ctor(sc);

	return 0;
}

static void
agrether_dtor(struct agr_softc *sc)
{
	struct agrether_private *priv = sc->sc_iftprivate;

	if (priv == NULL) {
		return;
	}

	ieee8023ad_dtor(sc);
	agr_mc_purgeall(sc, &priv->aep_multiaddrs);
	free(priv, M_DEVBUF);
	sc->sc_iftprivate = NULL;
}

static int
agrether_portinit(struct agr_softc *sc, struct agr_port *port)
{
	struct ifreq ifr;
	struct agrether_port_private *priv;
	int error;
	struct ethercom *ec = (void *)&sc->sc_if;
	struct ethercom *ec_port = (void *)port->port_ifp;

	port->port_media = IFM_ETHER | IFM_NONE;

	/*
	 * XXX it's better to always advertise ETHERCAP_VLAN_HWTAGGING
	 * and do tag insertion by ourselves if necessary,
	 * so that we can mix devices with different capabilities.
	 * ditto about if_capabilities.
	 */

	if (ec->ec_capabilities &
	    ~ec_port->ec_capabilities &
	    (ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING)) {
		if (ec->ec_nvlans > 0) {
			return EINVAL;
		}
		ec->ec_capabilities &=
		    ec_port->ec_capabilities |
		    ~(ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING);
	}

	/* Enable vlan support */
	if ((ec->ec_nvlans > 0) && 
	     ec_port->ec_nvlans++ == 0 &&
	    (ec_port->ec_capabilities & ETHERCAP_VLAN_MTU) != 0) {
		struct ifnet *p = port->port_ifp;
		/*
		 * Enable Tx/Rx of VLAN-sized frames.
		 */
		ec_port->ec_capenable |= ETHERCAP_VLAN_MTU;
		if (p->if_flags & IFF_UP) {
			error = if_flags_set(p, p->if_flags);
			if (error) {
				if (ec_port->ec_nvlans-- == 1)
					ec_port->ec_capenable &=
					    ~ETHERCAP_VLAN_MTU;
				return (error);
			}
		}
	}
	/* XXX ETHERCAP_JUMBO_MTU */

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK | M_ZERO);
	if (priv == NULL) {
		return ENOMEM;
	}

	port->port_iftprivate = priv;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_addr.sa_len = sizeof(ifr.ifr_addr);
	ifr.ifr_addr.sa_family = AF_UNSPEC;
	KASSERT(sizeof(ifr.ifr_addr) >=
	    sizeof(ethermulticastaddr_slowprotocols));
	memcpy(&ifr.ifr_addr.sa_data,
	    &ethermulticastaddr_slowprotocols, 
	    sizeof(ethermulticastaddr_slowprotocols));
	error = agrport_ioctl(port, SIOCADDMULTI, (void *)&ifr);
	if (error) {
		free(port->port_iftprivate, M_DEVBUF);
		port->port_iftprivate = NULL;
		return error;
	}

	ieee8023ad_portinit(port);

	return error;
}

static int
agrether_portfini(struct agr_softc *sc, struct agr_port *port)
{
	struct ifreq ifr;
	struct ethercom *ec_port = (void *)port->port_ifp;
	int error;

	if (port->port_iftprivate == NULL) {
		return 0;
	}

	if (ec_port->ec_nvlans > 0) {
		/* Disable vlan support */
		ec_port->ec_nvlans = 0;
		/*
		 * Disable Tx/Rx of VLAN-sized frames.
		 */
		ec_port->ec_capenable &= ~ETHERCAP_VLAN_MTU;
		if (port->port_ifp->if_flags & IFF_UP) {
			(void)if_flags_set(port->port_ifp,
			    port->port_ifp->if_flags);
		}
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_addr.sa_len = sizeof(ifr.ifr_addr);
	ifr.ifr_addr.sa_family = AF_UNSPEC;
	KASSERT(sizeof(ifr.ifr_addr) >=
	    sizeof(ethermulticastaddr_slowprotocols));
	memcpy(&ifr.ifr_addr.sa_data,
	    &ethermulticastaddr_slowprotocols, 
	    sizeof(ethermulticastaddr_slowprotocols));
	error = agrport_ioctl(port, SIOCDELMULTI, (void *)&ifr);
	if (error) {
		return error;
	}

	ieee8023ad_portfini(port);

	free(port->port_iftprivate, M_DEVBUF);
	port->port_iftprivate = NULL;

	return error;
}

static int
agrether_configmulti_port(struct agr_softc *sc, struct agr_port *port,
    bool add)
{
	struct agrether_private *priv = sc->sc_iftprivate;

	return agr_configmulti_port(&priv->aep_multiaddrs, port, add);
}

static int
agrether_configmulti_ifreq(struct agr_softc *sc, struct ifreq *ifr,
    bool add)
{
	struct agrether_private *priv = sc->sc_iftprivate;

	return agr_configmulti_ifreq(sc, &priv->aep_multiaddrs, ifr, add);
}

/* -------------------- */

static struct agr_port *
agrether_select_tx_port(struct agr_softc *sc, struct mbuf *m)
{

	return ieee8023ad_select_tx_port(sc, m);
}

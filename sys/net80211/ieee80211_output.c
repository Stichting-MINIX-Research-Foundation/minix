/*	$NetBSD: ieee80211_output.c,v 1.53 2015/08/24 22:21:26 pooka Exp $	*/
/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_output.c,v 1.34 2005/08/10 16:22:29 sam Exp $");
#endif
#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: ieee80211_output.c,v 1.53 2015/08/24 22:21:26 pooka Exp $");
#endif

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#ifdef __NetBSD__
#endif /* __NetBSD__ */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_ether.h>
#include <net/if_llc.h>
#include <net/if_vlanvar.h>

#include <net80211/ieee80211_netbsd.h>
#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <net/if_ether.h>
#endif

static int ieee80211_fragment(struct ieee80211com *, struct mbuf *,
	u_int hdrsize, u_int ciphdrsize, u_int mtu);

#ifdef IEEE80211_DEBUG
/*
 * Decide if an outbound management frame should be
 * printed when debugging is enabled.  This filters some
 * of the less interesting frames that come frequently
 * (e.g. beacons).
 */
static __inline int
doprint(struct ieee80211com *ic, int subtype)
{
	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		return (ic->ic_opmode == IEEE80211_M_IBSS);
	}
	return 1;
}
#endif

/*
 * Set the direction field and address fields of an outgoing
 * non-QoS frame.  Note this should be called early on in
 * constructing a frame as it sets i_fc[1]; other bits can
 * then be or'd in.
 */
static void
ieee80211_send_setup(struct ieee80211com *ic,
	struct ieee80211_node *ni,
	struct ieee80211_frame *wh,
	int type,
	const u_int8_t sa[IEEE80211_ADDR_LEN],
	const u_int8_t da[IEEE80211_ADDR_LEN],
	const u_int8_t bssid[IEEE80211_ADDR_LEN])
{
#define	WH4(wh)	((struct ieee80211_frame_addr4 *)wh)

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | type;
	if ((type & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, da);
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, sa);
			IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
			break;
		case IEEE80211_M_HOSTAP:
			wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
			IEEE80211_ADDR_COPY(wh->i_addr1, da);
			IEEE80211_ADDR_COPY(wh->i_addr2, bssid);
			IEEE80211_ADDR_COPY(wh->i_addr3, sa);
			break;
		case IEEE80211_M_MONITOR:	/* NB: to quiet compiler */
			break;
		}
	} else {
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, da);
		IEEE80211_ADDR_COPY(wh->i_addr2, sa);
		IEEE80211_ADDR_COPY(wh->i_addr3, bssid);
	}
	*(u_int16_t *)&wh->i_dur[0] = 0;
	/* NB: use non-QoS tid */
	*(u_int16_t *)&wh->i_seq[0] =
	    htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseqs[0]++;
#undef WH4
}

/*
 * Send a management frame to the specified node.  The node pointer
 * must have a reference as the pointer will be passed to the driver
 * and potentially held for a long time.  If the frame is successfully
 * dispatched to the driver, then it is responsible for freeing the
 * reference (and potentially free'ing up any associated storage).
 */
static int
ieee80211_mgmt_output(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct mbuf *m, int type, int timer)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;

	IASSERT(ni != NULL, ("null node"));

	/*
	 * Yech, hack alert!  We want to pass the node down to the
	 * driver's start routine.  If we don't do so then the start
	 * routine must immediately look it up again and that can
	 * cause a lock order reversal if, for example, this frame
	 * is being sent because the station is being timedout and
	 * the frame being sent is a DEAUTH message.  We could stick
	 * this in an m_tag and tack that on to the mbuf.  However
	 * that's rather expensive to do for every frame so instead
	 * we stuff it in the rcvif field since outbound frames do
	 * not (presently) use this.
	 */
	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
#ifdef __FreeBSD__
	KASSERT(m->m_pkthdr.rcvif == NULL, ("rcvif not null"));
#endif
	m->m_pkthdr.rcvif = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh, 
		IEEE80211_FC0_TYPE_MGT | type,
		ic->ic_myaddr, ni->ni_macaddr, ni->ni_bssid);
	if ((m->m_flags & M_LINK0) != 0 && ni->ni_challenge != NULL) {
		m->m_flags &= ~M_LINK0;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			"[%s] encrypting frame (%s)\n",
			ether_sprintf(wh->i_addr1), __func__);
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
	}
#ifdef IEEE80211_DEBUG
	/* avoid printing too many frames */
	if ((ieee80211_msg_debug(ic) && doprint(ic, type)) ||
	    ieee80211_msg_dumppkts(ic)) {
		printf("[%s] send %s on channel %u\n",
		    ether_sprintf(wh->i_addr1),
		    ieee80211_mgt_subtype_name[
			(type & IEEE80211_FC0_SUBTYPE_MASK) >>
				IEEE80211_FC0_SUBTYPE_SHIFT],
		    ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
#endif
	IEEE80211_NODE_STAT(ni, tx_mgmt);
	IF_ENQUEUE(&ic->ic_mgtq, m);
	if (timer) {
		/*
		 * Set the mgt frame timeout.
		 */
		ic->ic_mgt_timer = timer;
		ifp->if_timer = 1;
	}
	(*ifp->if_start)(ifp);
	return 0;
}

/*
 * Send a null data frame to the specified node.
 *
 * NB: the caller is assumed to have setup a node reference
 *     for use; this is necessary to deal with a race condition
 *     when probing for inactive stations.
 */
int
ieee80211_send_nulldata(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *m;
	struct ieee80211_frame *wh;

	MGETHDR(m, M_NOWAIT, MT_HEADER);
	if (m == NULL) {
		/* XXX debug msg */
		ic->ic_stats.is_tx_nobuf++;
		ieee80211_unref_node(&ni);
		return ENOMEM;
	}
	m->m_pkthdr.rcvif = (void *) ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh,
		IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_NODATA,
		ic->ic_myaddr, ni->ni_macaddr, ni->ni_bssid);
	/* NB: power management bit is never sent by an AP */
	if ((ni->ni_flags & IEEE80211_NODE_PWR_MGT) &&
	    ic->ic_opmode != IEEE80211_M_HOSTAP)
		wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
	m->m_len = m->m_pkthdr.len = sizeof(struct ieee80211_frame);

	IEEE80211_NODE_STAT(ni, tx_data);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
	    "[%s] send null data frame on channel %u, pwr mgt %s\n",
	    ether_sprintf(ni->ni_macaddr),
	    ieee80211_chan2ieee(ic, ic->ic_curchan),
	    wh->i_fc[1] & IEEE80211_FC1_PWR_MGT ? "ena" : "dis");

	IF_ENQUEUE(&ic->ic_mgtq, m);		/* cheat */
	(*ifp->if_start)(ifp);

	return 0;
}

/* 
 * Assign priority to a frame based on any vlan tag assigned
 * to the station and/or any Diffserv setting in an IP header.
 * Finally, if an ACM policy is setup (in station mode) it's
 * applied.
 */
int
ieee80211_classify(struct ieee80211com *ic, struct mbuf *m, struct ieee80211_node *ni)
{
	int v_wme_ac, d_wme_ac, ac;
#ifdef INET
	struct ether_header *eh;
#endif

	if ((ni->ni_flags & IEEE80211_NODE_QOS) == 0) {
		ac = WME_AC_BE;
		goto done;
	}

	/* 
	 * If node has a vlan tag then all traffic
	 * to it must have a matching tag.
	 */
	v_wme_ac = 0;
	if (ni->ni_vlan != 0) {
		/* XXX used to check ec_nvlans. */
		struct m_tag *mtag = m_tag_find(m, PACKET_TAG_VLAN, NULL);
		if (mtag == NULL) {
			IEEE80211_NODE_STAT(ni, tx_novlantag);
			return 1;
		}
		if (EVL_VLANOFTAG(VLAN_TAG_VALUE(mtag)) !=
		    EVL_VLANOFTAG(ni->ni_vlan)) {
			IEEE80211_NODE_STAT(ni, tx_vlanmismatch);
			return 1;
		}
		/* map vlan priority to AC */
		switch (EVL_PRIOFTAG(ni->ni_vlan)) {
		case 1:
		case 2:
			v_wme_ac = WME_AC_BK;
			break;
		case 0:
		case 3:
			v_wme_ac = WME_AC_BE;
			break;
		case 4:
		case 5:
			v_wme_ac = WME_AC_VI;
			break;
		case 6:
		case 7:
			v_wme_ac = WME_AC_VO;
			break;
		}
	}

#ifdef INET
	eh = mtod(m, struct ether_header *);
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		const struct ip *ip = (struct ip *)
			(mtod(m, u_int8_t *) + sizeof (*eh));
		/*
		 * IP frame, map the TOS field.
		 */
		switch (ip->ip_tos) {
		case 0x08:
		case 0x20:
			d_wme_ac = WME_AC_BK;	/* background */
			break;
		case 0x28:
		case 0xa0:
			d_wme_ac = WME_AC_VI;	/* video */
			break;
		case 0x30:			/* voice */
		case 0xe0:
		case 0x88:			/* XXX UPSD */
		case 0xb8:
			d_wme_ac = WME_AC_VO;
			break;
		default:
			d_wme_ac = WME_AC_BE;
			break;
		}
	} else {
#endif /* INET */
		d_wme_ac = WME_AC_BE;
#ifdef INET
	}
#endif
	/*
	 * Use highest priority AC.
	 */
	if (v_wme_ac > d_wme_ac)
		ac = v_wme_ac;
	else
		ac = d_wme_ac;

	/*
	 * Apply ACM policy.
	 */
	if (ic->ic_opmode == IEEE80211_M_STA) {
		static const int acmap[4] = {
			WME_AC_BK,	/* WME_AC_BE */
			WME_AC_BK,	/* WME_AC_BK */
			WME_AC_BE,	/* WME_AC_VI */
			WME_AC_VI,	/* WME_AC_VO */
		};
		while (ac != WME_AC_BK &&
		    ic->ic_wme.wme_wmeBssChanParams.cap_wmeParams[ac].wmep_acm)
			ac = acmap[ac];
	}
done:
	M_WME_SETAC(m, ac);
	return 0;
}

/*
 * Insure there is sufficient contiguous space to encapsulate the
 * 802.11 data frame.  If room isn't already there, arrange for it.
 * Drivers and cipher modules assume we have done the necessary work
 * and fail rudely if they don't find the space they need.
 */
static struct mbuf *
ieee80211_mbuf_adjust(struct ieee80211com *ic, int hdrsize,
	struct ieee80211_key *key, struct mbuf *m)
{
#define	TO_BE_RECLAIMED	(sizeof(struct ether_header) - sizeof(struct llc))
	int needed_space = hdrsize;
	int wlen = 0;

	if (key != NULL) {
		/* XXX belongs in crypto code? */
		needed_space += key->wk_cipher->ic_header;
		/* XXX frags */
	}
	/*
	 * We know we are called just before stripping an Ethernet
	 * header and prepending an LLC header.  This means we know
	 * there will be
	 *	sizeof(struct ether_header) - sizeof(struct llc)
	 * bytes recovered to which we need additional space for the
	 * 802.11 header and any crypto header.
	 */
	/* XXX check trailing space and copy instead? */
	if (M_LEADINGSPACE(m) < needed_space - TO_BE_RECLAIMED) {
		struct mbuf *n = m_gethdr(M_NOWAIT, m->m_type);
		if (n == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
			    "%s: cannot expand storage\n", __func__);
			ic->ic_stats.is_tx_nobuf++;
			m_freem(m);
			return NULL;
		}
		IASSERT(needed_space <= MHLEN,
		    ("not enough room, need %u got %zu\n", needed_space, MHLEN));
		/*
		 * Setup new mbuf to have leading space to prepend the
		 * 802.11 header and any crypto header bits that are
		 * required (the latter are added when the driver calls
		 * back to ieee80211_crypto_encap to do crypto encapsulation).
		 */
		/* NB: must be first 'cuz it clobbers m_data */
		M_MOVE_PKTHDR(n, m);
		n->m_len = 0;			/* NB: m_gethdr does not set */
		n->m_data += needed_space;
		/*
		 * Pull up Ethernet header to create the expected layout.
		 * We could use m_pullup but that's overkill (i.e. we don't
		 * need the actual data) and it cannot fail so do it inline
		 * for speed.
		 */
		/* NB: struct ether_header is known to be contiguous */
		n->m_len += sizeof(struct ether_header);
		m->m_len -= sizeof(struct ether_header);
		m->m_data += sizeof(struct ether_header);
		/*
		 * Replace the head of the chain.
		 */
		n->m_next = m;
		m = n;
	} else {
                /* We will overwrite the ethernet header in the
                 * 802.11 encapsulation stage.  Make sure that it
                 * is writable.
		 */
		wlen = sizeof(struct ether_header);
	}

	/*
	 * If we're going to s/w encrypt the mbuf chain make sure it is
	 * writable.
	 */
	if (key != NULL && (key->wk_flags & IEEE80211_KEY_SWCRYPT) != 0)
		wlen = M_COPYALL;

	if (wlen != 0 && m_makewritable(&m, 0, wlen, M_DONTWAIT) != 0) {
		m_freem(m);
		return NULL;
	}
	return m;
#undef TO_BE_RECLAIMED
}

/*
 * Return the transmit key to use in sending a unicast frame.
 * If a unicast key is set we use that.  When no unicast key is set
 * we fall back to the default transmit key.
 */ 
static __inline struct ieee80211_key *
ieee80211_crypto_getucastkey(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	if (IEEE80211_KEY_UNDEFINED(ni->ni_ucastkey)) {
		if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE ||
		    IEEE80211_KEY_UNDEFINED(ic->ic_nw_keys[ic->ic_def_txkey]))
			return NULL;
		return &ic->ic_nw_keys[ic->ic_def_txkey];
	} else {
		return &ni->ni_ucastkey;
	}
}

/*
 * Return the transmit key to use in sending a multicast frame.
 * Multicast traffic always uses the group key which is installed as
 * the default tx key.
 */ 
static __inline struct ieee80211_key *
ieee80211_crypto_getmcastkey(struct ieee80211com *ic,
    struct ieee80211_node *ni)
{
	if (ic->ic_def_txkey == IEEE80211_KEYIX_NONE ||
	    IEEE80211_KEY_UNDEFINED(ic->ic_nw_keys[ic->ic_def_txkey]))
		return NULL;
	return &ic->ic_nw_keys[ic->ic_def_txkey];
}

/*
 * Encapsulate an outbound data frame.  The mbuf chain is updated.
 * If an error is encountered NULL is returned.  The caller is required
 * to provide a node reference and pullup the ethernet header in the
 * first mbuf.
 */
struct mbuf *
ieee80211_encap(struct ieee80211com *ic, struct mbuf *m,
	struct ieee80211_node *ni)
{
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct ieee80211_key *key;
	struct llc *llc;
	int hdrsize, datalen, addqos, txfrag;

	IASSERT(m->m_len >= sizeof(eh), ("no ethernet header!"));
	memcpy(&eh, mtod(m, void *), sizeof(struct ether_header));

	/*
	 * Insure space for additional headers.  First identify
	 * transmit key to use in calculating any buffer adjustments
	 * required.  This is also used below to do privacy
	 * encapsulation work.  Then calculate the 802.11 header
	 * size and any padding required by the driver.
	 *
	 * Note key may be NULL if we fall back to the default
	 * transmit key and that is not set.  In that case the
	 * buffer may not be expanded as needed by the cipher
	 * routines, but they will/should discard it.
	 */
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		if (ic->ic_opmode == IEEE80211_M_STA ||
		    !IEEE80211_IS_MULTICAST(eh.ether_dhost))
			key = ieee80211_crypto_getucastkey(ic, ni);
		else
			key = ieee80211_crypto_getmcastkey(ic, ni);
		if (key == NULL && eh.ether_type != htons(ETHERTYPE_PAE)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_CRYPTO,
			    "[%s] no default transmit key (%s) deftxkey %u\n",
			    ether_sprintf(eh.ether_dhost), __func__,
			    ic->ic_def_txkey);
			ic->ic_stats.is_tx_nodefkey++;
		}
	} else
		key = NULL;
	/* XXX 4-address format */
	/*
	 * XXX Some ap's don't handle QoS-encapsulated EAPOL
	 * frames so suppress use.  This may be an issue if other
	 * ap's require all data frames to be QoS-encapsulated
	 * once negotiated in which case we'll need to make this
	 * configurable.
	 */
	addqos = (ni->ni_flags & IEEE80211_NODE_QOS) &&
		 eh.ether_type != htons(ETHERTYPE_PAE);
	if (addqos)
		hdrsize = sizeof(struct ieee80211_qosframe);
	else
		hdrsize = sizeof(struct ieee80211_frame);
	if (ic->ic_flags & IEEE80211_F_DATAPAD)
		hdrsize = roundup(hdrsize, sizeof(u_int32_t));
	m = ieee80211_mbuf_adjust(ic, hdrsize, key, m);
	if (m == NULL) {
		/* NB: ieee80211_mbuf_adjust handles msgs+statistics */
		goto bad;
	}

	/* NB: this could be optimized because of ieee80211_mbuf_adjust */
	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	datalen = m->m_pkthdr.len;		/* NB: w/o 802.11 header */

	M_PREPEND(m, hdrsize, M_DONTWAIT);
	if (m == NULL) {
		ic->ic_stats.is_tx_nobuf++;
		goto bad;
	}
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)wh->i_dur = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		/*
		 * NB: always use the bssid from ic_bss as the
		 *     neighbor's may be stale after an ibss merge
		 */
		IEEE80211_ADDR_COPY(wh->i_addr3, ic->ic_bss->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
#ifndef IEEE80211_NO_HOSTAP
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
#endif /* !IEEE80211_NO_HOSTAP */
		break;
	case IEEE80211_M_MONITOR:
		goto bad;
	}
	if (m->m_flags & M_MORE_DATA)
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	if (addqos) {
		struct ieee80211_qosframe *qwh =
			(struct ieee80211_qosframe *) wh;
		int ac, tid;

		ac = M_WME_GETAC(m);
		/* map from access class/queue to 11e header priorty value */
		tid = WME_AC_TO_TID(ac);
		qwh->i_qos[0] = tid & IEEE80211_QOS_TID;
		if (ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy)
			qwh->i_qos[0] |= 1 << IEEE80211_QOS_ACKPOLICY_S;
		qwh->i_qos[1] = 0;
		qwh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;

		*(u_int16_t *)wh->i_seq =
		    htole16(ni->ni_txseqs[tid] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[tid]++;
	} else {
		*(u_int16_t *)wh->i_seq =
		    htole16(ni->ni_txseqs[0] << IEEE80211_SEQ_SEQ_SHIFT);
		ni->ni_txseqs[0]++;
	}
	/* check if xmit fragmentation is required */
	txfrag = (m->m_pkthdr.len > ic->ic_fragthreshold &&
	    !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (m->m_flags & M_FF) == 0);          /* NB: don't fragment ff's */
	if (key != NULL) {
		/*
		 * IEEE 802.1X: send EAPOL frames always in the clear.
		 * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
		 */
		if (eh.ether_type != htons(ETHERTYPE_PAE) ||
		    ((ic->ic_flags & IEEE80211_F_WPA) &&
		     (ic->ic_opmode == IEEE80211_M_STA ?
		      !IEEE80211_KEY_UNDEFINED(*key) :
		      !IEEE80211_KEY_UNDEFINED(ni->ni_ucastkey)))) {
			wh->i_fc[1] |= IEEE80211_FC1_WEP;
			if (!ieee80211_crypto_enmic(ic, key, m, txfrag)) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
				    "[%s] enmic failed, discard frame\n",
				    ether_sprintf(eh.ether_dhost));
				ic->ic_stats.is_crypto_enmicfail++;
				goto bad;
			}
		}
	}
	if (txfrag && !ieee80211_fragment(ic, m, hdrsize,
	    key != NULL ? key->wk_cipher->ic_header : 0, ic->ic_fragthreshold))
		goto bad;

	IEEE80211_NODE_STAT(ni, tx_data);
	IEEE80211_NODE_STAT_ADD(ni, tx_bytes, datalen);

	return m;
bad:
	if (m != NULL)
		m_freem(m);
	return NULL;
}

/*
 * Arguments in:
 *
 * paylen:  payload length (no FCS, no WEP header)
 *
 * hdrlen:  header length
 *
 * rate:    MSDU speed, units 500kb/s
 *
 * flags:   IEEE80211_F_SHPREAMBLE (use short preamble),
 *          IEEE80211_F_SHSLOT (use short slot length)
 *
 * Arguments out:
 *
 * d:       802.11 Duration field for RTS,
 *          802.11 Duration field for data frame,
 *          PLCP Length for data frame,
 *          residual octets at end of data slot
 */
static int
ieee80211_compute_duration1(int len, int use_ack, uint32_t icflags, int rate,
    struct ieee80211_duration *d)
{
	int pre, ctsrate;
	int ack, bitlen, data_dur, remainder;

	/* RTS reserves medium for SIFS | CTS | SIFS | (DATA) | SIFS | ACK
	 * DATA reserves medium for SIFS | ACK,
	 *
	 * (XXX or SIFS | ACK | SIFS | DATA | SIFS | ACK, if more fragments)
	 *
	 * XXXMYC: no ACK on multicast/broadcast or control packets
	 */

	bitlen = len * 8;

	pre = IEEE80211_DUR_DS_SIFS;
	if ((icflags & IEEE80211_F_SHPREAMBLE) != 0)
		pre += IEEE80211_DUR_DS_SHORT_PREAMBLE + IEEE80211_DUR_DS_FAST_PLCPHDR;
	else
		pre += IEEE80211_DUR_DS_LONG_PREAMBLE + IEEE80211_DUR_DS_SLOW_PLCPHDR;

	d->d_residue = 0;
	data_dur = (bitlen * 2) / rate;
	remainder = (bitlen * 2) % rate;
	if (remainder != 0) {
		d->d_residue = (rate - remainder) / 16;
		data_dur++;
	}

	switch (rate) {
	case 2:		/* 1 Mb/s */
	case 4:		/* 2 Mb/s */
		/* 1 - 2 Mb/s WLAN: send ACK/CTS at 1 Mb/s */
		ctsrate = 2;
		break;
	case 11:	/* 5.5 Mb/s */
	case 22:	/* 11  Mb/s */
	case 44:	/* 22  Mb/s */
		/* 5.5 - 11 Mb/s WLAN: send ACK/CTS at 2 Mb/s */
		ctsrate = 4;
		break;
	default:
		/* TBD */
		return -1;
	}

	d->d_plcp_len = data_dur;

	ack = (use_ack) ? pre + (IEEE80211_DUR_DS_SLOW_ACK * 2) / ctsrate : 0;

	d->d_rts_dur =
	    pre + (IEEE80211_DUR_DS_SLOW_CTS * 2) / ctsrate +
	    pre + data_dur +
	    ack;

	d->d_data_dur = ack;

	return 0;
}

/*
 * Arguments in:
 *
 * wh:      802.11 header
 *
 * paylen:  payload length (no FCS, no WEP header)
 *
 * rate:    MSDU speed, units 500kb/s
 *
 * fraglen: fragment length, set to maximum (or higher) for no
 *          fragmentation
 *
 * flags:   IEEE80211_F_PRIVACY (hardware adds WEP),
 *          IEEE80211_F_SHPREAMBLE (use short preamble),
 *          IEEE80211_F_SHSLOT (use short slot length)
 *
 * Arguments out:
 *
 * d0: 802.11 Duration fields (RTS/Data), PLCP Length, Service fields
 *     of first/only fragment
 *
 * dn: 802.11 Duration fields (RTS/Data), PLCP Length, Service fields
 *     of last fragment
 *
 * ieee80211_compute_duration assumes crypto-encapsulation, if any,
 * has already taken place.
 */
int
ieee80211_compute_duration(const struct ieee80211_frame_min *wh,
    const struct ieee80211_key *wk, int len,
    uint32_t icflags, int fraglen, int rate, struct ieee80211_duration *d0,
    struct ieee80211_duration *dn, int *npktp, int debug)
{
	int ack, rc;
	int cryptolen,	/* crypto overhead: header+trailer */
	    firstlen,	/* first fragment's payload + overhead length */
	    hdrlen,	/* header length w/o driver padding */
	    lastlen,	/* last fragment's payload length w/ overhead */
	    lastlen0,	/* last fragment's payload length w/o overhead */
	    npkt,	/* number of fragments */
	    overlen,	/* non-802.11 header overhead per fragment */
	    paylen;	/* payload length w/o overhead */

	hdrlen = ieee80211_anyhdrsize((const void *)wh);

        /* Account for padding required by the driver. */
	if (icflags & IEEE80211_F_DATAPAD)
		paylen = len - roundup(hdrlen, sizeof(u_int32_t));
	else
		paylen = len - hdrlen;

	overlen = IEEE80211_CRC_LEN;

	if (wk != NULL) {
		cryptolen = wk->wk_cipher->ic_header +
		            wk->wk_cipher->ic_trailer;
		paylen -= cryptolen;
		overlen += cryptolen;
	}

	npkt = paylen / fraglen;
	lastlen0 = paylen % fraglen;

	if (npkt == 0)			/* no fragments */
		lastlen = paylen + overlen;
	else if (lastlen0 != 0) {	/* a short "tail" fragment */
		lastlen = lastlen0 + overlen;
		npkt++;
	} else				/* full-length "tail" fragment */
		lastlen = fraglen + overlen;

	if (npktp != NULL)
		*npktp = npkt;

	if (npkt > 1)
		firstlen = fraglen + overlen;
	else
		firstlen = paylen + overlen;

	if (debug) {
		printf("%s: npkt %d firstlen %d lastlen0 %d lastlen %d "
		    "fraglen %d overlen %d len %d rate %d icflags %08x\n",
		    __func__, npkt, firstlen, lastlen0, lastlen, fraglen,
		    overlen, len, rate, icflags);
	}

	ack = !IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    (wh->i_fc[1] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_CTL;

	rc = ieee80211_compute_duration1(firstlen + hdrlen,
	    ack, icflags, rate, d0);
	if (rc == -1)
		return rc;

	if (npkt <= 1) {
		*dn = *d0;
		return 0;
	}
	return ieee80211_compute_duration1(lastlen + hdrlen, ack, icflags, rate,
	    dn);
}

/*
 * Fragment the frame according to the specified mtu.
 * The size of the 802.11 header (w/o padding) is provided
 * so we don't need to recalculate it.  We create a new
 * mbuf for each fragment and chain it through m_nextpkt;
 * we might be able to optimize this by reusing the original
 * packet's mbufs but that is significantly more complicated.
 */
static int
ieee80211_fragment(struct ieee80211com *ic, struct mbuf *m0,
	u_int hdrsize, u_int ciphdrsize, u_int mtu)
{
	struct ieee80211_frame *wh, *whf;
	struct mbuf *m, *prev, *next;
	u_int totalhdrsize, fragno, fragsize, off, remainder, payload;

	IASSERT(m0->m_nextpkt == NULL, ("mbuf already chained?"));
	IASSERT(m0->m_pkthdr.len > mtu,
		("pktlen %u mtu %u", m0->m_pkthdr.len, mtu));

	wh = mtod(m0, struct ieee80211_frame *);
	/* NB: mark the first frag; it will be propagated below */
	wh->i_fc[1] |= IEEE80211_FC1_MORE_FRAG;
	totalhdrsize = hdrsize + ciphdrsize;
	fragno = 1;
	off = mtu - ciphdrsize;
	remainder = m0->m_pkthdr.len - off;
	prev = m0;
	do {
		fragsize = totalhdrsize + remainder;
		if (fragsize > mtu)
			fragsize = mtu;
		IASSERT(fragsize < MCLBYTES,
			("fragment size %u too big!", fragsize));
		if (fragsize > MHLEN)
			m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		else
			m = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m == NULL)
			goto bad;
		/* leave room to prepend any cipher header */
		m_align(m, fragsize - ciphdrsize);

		/*
		 * Form the header in the fragment.  Note that since
		 * we mark the first fragment with the MORE_FRAG bit
		 * it automatically is propagated to each fragment; we
		 * need only clear it on the last fragment (done below).
		 */
		whf = mtod(m, struct ieee80211_frame *);
		memcpy(whf, wh, hdrsize);
		*(u_int16_t *)&whf->i_seq[0] |= htole16(
			(fragno & IEEE80211_SEQ_FRAG_MASK) <<
				IEEE80211_SEQ_FRAG_SHIFT);
		fragno++;

		payload = fragsize - totalhdrsize;
		/* NB: destination is known to be contiguous */
		m_copydata(m0, off, payload, mtod(m, u_int8_t *) + hdrsize);
		m->m_len = hdrsize + payload;
		m->m_pkthdr.len = hdrsize + payload;
		m->m_flags |= M_FRAG;

		/* chain up the fragment */
		prev->m_nextpkt = m;
		prev = m;

		/* deduct fragment just formed */
		remainder -= payload;
		off += payload;
	} while (remainder != 0);
	whf->i_fc[1] &= ~IEEE80211_FC1_MORE_FRAG;

	/* strip first mbuf now that everything has been copied */
	m_adj(m0, -(m0->m_pkthdr.len - (mtu - ciphdrsize)));
	m0->m_flags |= M_FIRSTFRAG | M_FRAG;

	ic->ic_stats.is_tx_fragframes++;
	ic->ic_stats.is_tx_frags += fragno-1;

	return 1;
bad:
	/* reclaim fragments but leave original frame for caller to free */
	for (m = m0->m_nextpkt; m != NULL; m = next) {
		next = m->m_nextpkt;
		m->m_nextpkt = NULL;            /* XXX paranoid */
		m_freem(m);
	}
	m0->m_nextpkt = NULL;
	return 0;
}

/*
 * Add a supported rates element id to a frame.
 */
static u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	int nrates;

	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
static u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
	/*
	 * Add an extended supported rates element if operating in 11g mode.
	 */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}
	return frm;
}

/* 
 * Add an ssid elemet to a frame.
 */
static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return frm + len;
}

/*
 * Add an erp element to a frame.
 */
static u_int8_t *
ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic)
{
	u_int8_t erp;

	*frm++ = IEEE80211_ELEMID_ERP;
	*frm++ = 1;
	erp = 0;
	if (ic->ic_nonerpsta != 0)
		erp |= IEEE80211_ERP_NON_ERP_PRESENT;
	if (ic->ic_flags & IEEE80211_F_USEPROT)
		erp |= IEEE80211_ERP_USE_PROTECTION;
	if (ic->ic_flags & IEEE80211_F_USEBARKER)
		erp |= IEEE80211_ERP_LONG_PREAMBLE;
	*frm++ = erp;
	return frm;
}

static u_int8_t *
ieee80211_setup_wpa_ie(struct ieee80211com *ic, u_int8_t *ie)
{
#define	WPA_OUI_BYTES		0x00, 0x50, 0xf2
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const u_int8_t oui[4] = { WPA_OUI_BYTES, WPA_OUI_TYPE };
	static const u_int8_t cipher_suite[][4] = {
		{ WPA_OUI_BYTES, WPA_CSE_WEP40 },	/* NB: 40-bit */
		{ WPA_OUI_BYTES, WPA_CSE_TKIP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX WRAP */
		{ WPA_OUI_BYTES, WPA_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ WPA_OUI_BYTES, WPA_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ WPA_OUI_BYTES, WPA_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ WPA_OUI_BYTES, WPA_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_VENDOR;
	*frm++ = 0;				/* length filled in below */
	memcpy(frm, oui, sizeof(oui));		/* WPA OUI */
	frm += sizeof(oui);
	ADDSHORT(frm, WPA_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	if (rsn->rsn_caps != 0 && rsn->rsn_caps != RSN_CAP_PREAUTH)
		ADDSHORT(frm, rsn->rsn_caps);

	/* calculate element length */
	ie[1] = frm - ie - 2;
	IASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
		("WPA IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSHORT
#undef ADDSELECTOR
#undef WPA_OUI_BYTES
}

static u_int8_t *
ieee80211_setup_rsn_ie(struct ieee80211com *ic, u_int8_t *ie)
{
#define	RSN_OUI_BYTES		0x00, 0x0f, 0xac
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDSELECTOR(frm, sel) do {		\
	memcpy(frm, sel, 4);			\
	frm += 4;				\
} while (0)
	static const u_int8_t cipher_suite[][4] = {
		{ RSN_OUI_BYTES, RSN_CSE_WEP40 },	/* NB: 40-bit */
		{ RSN_OUI_BYTES, RSN_CSE_TKIP },
		{ RSN_OUI_BYTES, RSN_CSE_WRAP },
		{ RSN_OUI_BYTES, RSN_CSE_CCMP },
		{ 0x00, 0x00, 0x00, 0x00 },		/* XXX CKIP */
		{ RSN_OUI_BYTES, RSN_CSE_NULL },
	};
	static const u_int8_t wep104_suite[4] =
		{ RSN_OUI_BYTES, RSN_CSE_WEP104 };
	static const u_int8_t key_mgt_unspec[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ RSN_OUI_BYTES, RSN_ASE_8021X_PSK };
	const struct ieee80211_rsnparms *rsn = &ic->ic_bss->ni_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;

	*frm++ = IEEE80211_ELEMID_RSN;
	*frm++ = 0;				/* length filled in below */
	ADDSHORT(frm, RSN_VERSION);

	/* XXX filter out CKIP */

	/* multicast cipher */
	if (rsn->rsn_mcastcipher == IEEE80211_CIPHER_WEP &&
	    rsn->rsn_mcastkeylen >= 13)
		ADDSELECTOR(frm, wep104_suite);
	else
		ADDSELECTOR(frm, cipher_suite[rsn->rsn_mcastcipher]);

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_AES_CCM)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
	}
	if (rsn->rsn_ucastcipherset & (1<<IEEE80211_CIPHER_TKIP)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
	}

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* optional capabilities */
	ADDSHORT(frm, rsn->rsn_caps);
	/* XXX PMKID */

	/* calculate element length */
	ie[1] = frm - ie - 2;
	IASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
		("RSN IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSELECTOR
#undef ADDSHORT
#undef RSN_OUI_BYTES
}

/*
 * Add a WPA/RSN element to a frame.
 */
static u_int8_t *
ieee80211_add_wpa(u_int8_t *frm, struct ieee80211com *ic)
{

	IASSERT(ic->ic_flags & IEEE80211_F_WPA, ("no WPA/RSN!"));
	if (ic->ic_flags & IEEE80211_F_WPA2)
		frm = ieee80211_setup_rsn_ie(ic, frm);
	if (ic->ic_flags & IEEE80211_F_WPA1)
		frm = ieee80211_setup_wpa_ie(ic, frm);
	return frm;
}

#define	WME_OUI_BYTES		0x00, 0x50, 0xf2
/*
 * Add a WME information element to a frame.
 */
static u_int8_t *
ieee80211_add_wme_info(u_int8_t *frm, struct ieee80211_wme_state *wme)
{
	static const struct ieee80211_wme_info info = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_info) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_INFO_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
		.wme_info	= 0,
	};
	memcpy(frm, &info, sizeof(info));
	return frm + sizeof(info); 
}

/*
 * Add a WME parameters element to a frame.
 */
static u_int8_t *
ieee80211_add_wme_param(u_int8_t *frm, struct ieee80211_wme_state *wme)
{
#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
	/* NB: this works 'cuz a param has an info at the front */
	static const struct ieee80211_wme_info param = {
		.wme_id		= IEEE80211_ELEMID_VENDOR,
		.wme_len	= sizeof(struct ieee80211_wme_param) - 2,
		.wme_oui	= { WME_OUI_BYTES },
		.wme_type	= WME_OUI_TYPE,
		.wme_subtype	= WME_PARAM_OUI_SUBTYPE,
		.wme_version	= WME_VERSION,
	};
	int i;

	memcpy(frm, &param, sizeof(param));
	frm += offsetof(struct ieee80211_wme_info, wme_info);
	*frm++ = wme->wme_bssChanParams.cap_info;	/* AC info */
	*frm++ = 0;					/* reserved field */
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct wmeParams *ac =
		       &wme->wme_bssChanParams.cap_wmeParams[i];
		*frm++ = SM(i, WME_PARAM_ACI)
		       | SM(ac->wmep_acm, WME_PARAM_ACM)
		       | SM(ac->wmep_aifsn, WME_PARAM_AIFSN)
		       ;
		*frm++ = SM(ac->wmep_logcwmax, WME_PARAM_LOGCWMAX)
		       | SM(ac->wmep_logcwmin, WME_PARAM_LOGCWMIN)
		       ;
		ADDSHORT(frm, ac->wmep_txopLimit);
	}
	return frm;
#undef SM
#undef ADDSHORT
}
#undef WME_OUI_BYTES

/*
 * Send a probe request frame with the specified ssid
 * and any optional information element data.
 */
int
ieee80211_send_probereq(struct ieee80211_node *ni,
	const u_int8_t sa[IEEE80211_ADDR_LEN],
	const u_int8_t da[IEEE80211_ADDR_LEN],
	const u_int8_t bssid[IEEE80211_ADDR_LEN],
	const u_int8_t *ssid, size_t ssidlen,
	const void *optie, size_t optielen)
{
	struct ieee80211com *ic = ni->ni_ic;
	enum ieee80211_phymode mode;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	u_int8_t *frm;

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n",
		__func__, __LINE__,
		ni, ether_sprintf(ni->ni_macaddr),
		ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] user-specified ie's
	 */
	m = ieee80211_getmgtframe(&frm,
		 2 + IEEE80211_NWID_LEN
	       + 2 + IEEE80211_RATE_SIZE
	       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
	       + (optie != NULL ? optielen : 0)
	);
	if (m == NULL) {
		ic->ic_stats.is_tx_nobuf++;
		ieee80211_free_node(ni);
		return ENOMEM;
	}

	frm = ieee80211_add_ssid(frm, ssid, ssidlen);
	mode = ieee80211_chan2mode(ic, ic->ic_curchan);
	frm = ieee80211_add_rates(frm, &ic->ic_sup_rates[mode]);
	frm = ieee80211_add_xrates(frm, &ic->ic_sup_rates[mode]);

	if (optie != NULL) {
		memcpy(frm, optie, optielen);
		frm += optielen;
	}
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	if (m == NULL)
		return ENOMEM;
	IASSERT(m->m_pkthdr.rcvif == NULL, ("rcvif not null"));
	m->m_pkthdr.rcvif = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	ieee80211_send_setup(ic, ni, wh,
		IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_REQ,
		sa, da, bssid);
	/* XXX power management? */

	IEEE80211_NODE_STAT(ni, tx_probereq);
	IEEE80211_NODE_STAT(ni, tx_mgmt);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_DEBUG | IEEE80211_MSG_DUMPPKTS,
	    "[%s] send probe req on channel %u\n",
	    ether_sprintf(wh->i_addr1),
	    ieee80211_chan2ieee(ic, ic->ic_curchan));

	IF_ENQUEUE(&ic->ic_mgtq, m);
	(*ic->ic_ifp->if_start)(ic->ic_ifp);
	return 0;
}

/*
 * Send a management frame.  The node is for the destination (or ic_bss
 * when in station mode).  Nodes other than ic_bss have their reference
 * count bumped to reflect our use for an indeterminant time.
 */
int
ieee80211_send_mgmt(struct ieee80211com *ic, struct ieee80211_node *ni,
	int type, int arg)
{
#define	senderr(_x, _v)	do { ic->ic_stats._v++; ret = _x; goto bad; } while (0)
	struct mbuf *m;
	u_int8_t *frm;
	u_int16_t capinfo;
	int has_challenge, is_shared_key, ret, timer, status;

	IASSERT(ni != NULL, ("null node"));

	/*
	 * Hold a reference on the node so it doesn't go away until after
	 * the xmit is complete all the way in the driver.  On error we
	 * will remove our reference.
	 */
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"ieee80211_ref_node (%s:%u) %p<%s> refcnt %d\n",
		__func__, __LINE__,
		ni, ether_sprintf(ni->ni_macaddr),
		ieee80211_node_refcnt(ni)+1);
	ieee80211_ref_node(ni);

	timer = 0;
	switch (type) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		/*
		 * probe response frame format
		 *	[8] time stamp
		 *	[2] beacon interval
		 *	[2] cabability information
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] parameter set (FH/DS)
		 *	[tlv] parameter set (IBSS)
		 *	[tlv] extended rate phy (ERP)
		 *	[tlv] extended supported rates
		 *	[tlv] WPA
		 *	[tlv] WME (optional)
		 */
		m = ieee80211_getmgtframe(&frm,
			 8
		       + sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + 2 + IEEE80211_NWID_LEN
		       + 2 + IEEE80211_RATE_SIZE
		       + 7	/* max(7,3) */
		       + 6
		       + 3
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       /* XXX !WPA1+WPA2 fits w/o a cluster */
		       + (ic->ic_flags & IEEE80211_F_WPA ?
				2*sizeof(struct ieee80211_ie_wpa) : 0)
		       + sizeof(struct ieee80211_wme_param)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		memset(frm, 0, 8);	/* timestamp should be filled later */
		frm += 8;
		*(u_int16_t *)frm = htole16(ic->ic_bss->ni_intval);
		frm += 2;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			capinfo = IEEE80211_CAPINFO_IBSS;
		else
			capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		frm = ieee80211_add_ssid(frm, ic->ic_bss->ni_essid,
				ic->ic_bss->ni_esslen);
		frm = ieee80211_add_rates(frm, &ni->ni_rates);

		if (ic->ic_phytype == IEEE80211_T_FH) {
                        *frm++ = IEEE80211_ELEMID_FHPARMS;
                        *frm++ = 5;
                        *frm++ = ni->ni_fhdwell & 0x00ff;
                        *frm++ = (ni->ni_fhdwell >> 8) & 0x00ff;
                        *frm++ = IEEE80211_FH_CHANSET(
			    ieee80211_chan2ieee(ic, ic->ic_curchan));
                        *frm++ = IEEE80211_FH_CHANPAT(
			    ieee80211_chan2ieee(ic, ic->ic_curchan));
                        *frm++ = ni->ni_fhindex;
		} else {
			*frm++ = IEEE80211_ELEMID_DSPARMS;
			*frm++ = 1;
			*frm++ = ieee80211_chan2ieee(ic, ic->ic_curchan);
		}

		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			*frm++ = IEEE80211_ELEMID_IBSSPARMS;
			*frm++ = 2;
			*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		}
		if (ic->ic_flags & IEEE80211_F_WPA)
			frm = ieee80211_add_wpa(frm, ic);
		if (ic->ic_curmode == IEEE80211_MODE_11G)
			frm = ieee80211_add_erp(frm, ic);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		if (ic->ic_flags & IEEE80211_F_WME)
			frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_AUTH:
		status = arg >> 16;
		arg &= 0xffff;
		has_challenge = ((arg == IEEE80211_AUTH_SHARED_CHALLENGE ||
		    arg == IEEE80211_AUTH_SHARED_RESPONSE) &&
		    ni->ni_challenge != NULL);

		/*
		 * Deduce whether we're doing open authentication or
		 * shared key authentication.  We do the latter if
		 * we're in the middle of a shared key authentication
		 * handshake or if we're initiating an authentication
		 * request and configured to use shared key.
		 */
		is_shared_key = has_challenge ||
		     arg >= IEEE80211_AUTH_SHARED_RESPONSE ||
		     (arg == IEEE80211_AUTH_SHARED_REQUEST &&
		      ic->ic_bss->ni_authmode == IEEE80211_AUTH_SHARED);

		m = ieee80211_getmgtframe(&frm,
			  3 * sizeof(u_int16_t)
			+ (has_challenge && status == IEEE80211_STATUS_SUCCESS ?
				sizeof(u_int16_t)+IEEE80211_CHALLENGE_LEN : 0)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		((u_int16_t *)frm)[0] =
		    (is_shared_key) ? htole16(IEEE80211_AUTH_ALG_SHARED)
		                    : htole16(IEEE80211_AUTH_ALG_OPEN);
		((u_int16_t *)frm)[1] = htole16(arg);	/* sequence number */
		((u_int16_t *)frm)[2] = htole16(status);/* status */

		if (has_challenge && status == IEEE80211_STATUS_SUCCESS) {
			((u_int16_t *)frm)[3] =
			    htole16((IEEE80211_CHALLENGE_LEN << 8) |
			    IEEE80211_ELEMID_CHALLENGE);
			memcpy(&((u_int16_t *)frm)[4], ni->ni_challenge,
			    IEEE80211_CHALLENGE_LEN);
			m->m_pkthdr.len = m->m_len =
				4 * sizeof(u_int16_t) + IEEE80211_CHALLENGE_LEN;
			if (arg == IEEE80211_AUTH_SHARED_RESPONSE) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
				    "[%s] request encrypt frame (%s)\n",
				    ether_sprintf(ni->ni_macaddr), __func__);
				m->m_flags |= M_LINK0; /* WEP-encrypt, please */
			}
		} else
			m->m_pkthdr.len = m->m_len = 3 * sizeof(u_int16_t);

		/* XXX not right for shared key */
		if (status == IEEE80211_STATUS_SUCCESS)
			IEEE80211_NODE_STAT(ni, tx_auth);
		else
			IEEE80211_NODE_STAT(ni, tx_auth_fail);

		if (ic->ic_opmode == IEEE80211_M_STA)
			timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_DEAUTH:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_AUTH,
			"[%s] send station deauthenticate (reason %d)\n",
			ether_sprintf(ni->ni_macaddr), arg);
		m = ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(u_int16_t *)frm = htole16(arg);	/* reason */
		m->m_pkthdr.len = m->m_len = sizeof(u_int16_t);

		IEEE80211_NODE_STAT(ni, tx_deauth);
		IEEE80211_NODE_STAT_SET(ni, tx_deauth_code, arg);

		ieee80211_node_unauthorize(ni);		/* port closed */
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
	case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] listen interval
		 *	[6*] current AP address (reassoc only)
		 *	[tlv] ssid
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] WME
		 *	[tlv] user-specified ie's
		 */
		m = ieee80211_getmgtframe(&frm,
			 sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + IEEE80211_ADDR_LEN
		       + 2 + IEEE80211_NWID_LEN
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       + sizeof(struct ieee80211_wme_info)
		       + (ic->ic_opt_ie != NULL ? ic->ic_opt_ie_len : 0)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		capinfo = 0;
		if (ic->ic_opmode == IEEE80211_M_IBSS)
			capinfo |= IEEE80211_CAPINFO_IBSS;
		else		/* IEEE80211_M_STA */
			capinfo |= IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		/*
		 * NB: Some 11a AP's reject the request when
		 *     short premable is set.
		 */
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) &&
		    (ic->ic_caps & IEEE80211_C_SHSLOT))
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(ic->ic_lintval);
		frm += 2;

		if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
			IEEE80211_ADDR_COPY(frm, ic->ic_bss->ni_bssid);
			frm += IEEE80211_ADDR_LEN;
		}

		frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
			frm = ieee80211_add_wme_info(frm, &ic->ic_wme);
		if (ic->ic_opt_ie != NULL) {
			memcpy(frm, ic->ic_opt_ie, ic->ic_opt_ie_len);
			frm += ic->ic_opt_ie_len;
		}
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

		timer = IEEE80211_TRANS_WAIT;
		break;

	case IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
	case IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
		/*
		 * asreq frame format
		 *	[2] capability information
		 *	[2] status
		 *	[2] association ID
		 *	[tlv] supported rates
		 *	[tlv] extended supported rates
		 *	[tlv] WME (if enabled and STA enabled)
		 */
		m = ieee80211_getmgtframe(&frm,
			 sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + sizeof(u_int16_t)
		       + 2 + IEEE80211_RATE_SIZE
		       + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		       + sizeof(struct ieee80211_wme_param)
		);
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);

		capinfo = IEEE80211_CAPINFO_ESS;
		if (ic->ic_flags & IEEE80211_F_PRIVACY)
			capinfo |= IEEE80211_CAPINFO_PRIVACY;
		if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
		    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
		if (ic->ic_flags & IEEE80211_F_SHSLOT)
			capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
		*(u_int16_t *)frm = htole16(capinfo);
		frm += 2;

		*(u_int16_t *)frm = htole16(arg);	/* status */
		frm += 2;

		if (arg == IEEE80211_STATUS_SUCCESS) {
			*(u_int16_t *)frm = htole16(ni->ni_associd);
			IEEE80211_NODE_STAT(ni, tx_assoc);
		} else
			IEEE80211_NODE_STAT(ni, tx_assoc_fail);
		frm += 2;

		frm = ieee80211_add_rates(frm, &ni->ni_rates);
		frm = ieee80211_add_xrates(frm, &ni->ni_rates);
		if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_wme_ie != NULL)
			frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
		m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);
		break;

	case IEEE80211_FC0_SUBTYPE_DISASSOC:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			"[%s] send station disassociate (reason %d)\n",
			ether_sprintf(ni->ni_macaddr), arg);
		m = ieee80211_getmgtframe(&frm, sizeof(u_int16_t));
		if (m == NULL)
			senderr(ENOMEM, is_tx_nobuf);
		*(u_int16_t *)frm = htole16(arg);	/* reason */
		m->m_pkthdr.len = m->m_len = sizeof(u_int16_t);

		IEEE80211_NODE_STAT(ni, tx_disassoc);
		IEEE80211_NODE_STAT_SET(ni, tx_disassoc_code, arg);
		break;

	default:
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"[%s] invalid mgmt frame type %u\n",
			ether_sprintf(ni->ni_macaddr), type);
		senderr(EINVAL, is_tx_unknownmgt);
		/* NOTREACHED */
	}
	ret = ieee80211_mgmt_output(ic, ni, m, type, timer);
	if (ret != 0) {
bad:
		ieee80211_free_node(ni);
	}
	return ret;
#undef senderr
}

/*
 * Build a RTS (Request To Send) control frame.
 */
struct mbuf *
ieee80211_get_rts(struct ieee80211com *ic, const struct ieee80211_frame *wh,
    uint16_t dur)
{
	struct ieee80211_frame_rts *rts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	m->m_pkthdr.len = m->m_len = sizeof(struct ieee80211_frame_rts);

	rts = mtod(m, struct ieee80211_frame_rts *);
	rts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_RTS;
	rts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)rts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(rts->i_ra, wh->i_addr1);
	IEEE80211_ADDR_COPY(rts->i_ta, wh->i_addr2);

	return m;
}

/*
 * Build a CTS-to-self (Clear To Send) control frame.
 */
struct mbuf *
ieee80211_get_cts_to_self(struct ieee80211com *ic, uint16_t dur)
{
	struct ieee80211_frame_cts *cts;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;

	m->m_pkthdr.len = m->m_len = sizeof(struct ieee80211_frame_cts);

	cts = mtod(m, struct ieee80211_frame_cts *);
	cts->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_CTL |
	    IEEE80211_FC0_SUBTYPE_CTS;
	cts->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(uint16_t *)cts->i_dur = htole16(dur);
	IEEE80211_ADDR_COPY(cts->i_ra, ic->ic_myaddr);

	return m;
}

/*
 * Allocate a beacon frame and fillin the appropriate bits.
 */
struct mbuf *
ieee80211_beacon_alloc(struct ieee80211com *ic, struct ieee80211_node *ni,
	struct ieee80211_beacon_offsets *bo)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	int pktlen;
	u_int8_t *frm, *efrm;
	u_int16_t capinfo;
	struct ieee80211_rateset *rs;

	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[3] parameter set (DS)
	 *	[tlv] parameter set (IBSS/TIM)
	 *	[tlv] extended rate phy (ERP)
	 *	[tlv] extended supported rates
	 *	[tlv] WME parameters
	 *	[tlv] WPA/RSN parameters
	 * XXX Vendor-specific OIDs (e.g. Atheros)
	 * NB: we allocate the max space required for the TIM bitmap.
	 */
	rs = &ni->ni_rates;
	pktlen =   8					/* time stamp */
		 + sizeof(u_int16_t)			/* beacon interval */
		 + sizeof(u_int16_t)			/* capabilities */
		 + 2 + ni->ni_esslen			/* ssid */
	         + 2 + IEEE80211_RATE_SIZE		/* supported rates */
	         + 2 + 1				/* DS parameters */
		 + 2 + 4 + ic->ic_tim_len		/* DTIM/IBSSPARMS */
		 + 2 + 1				/* ERP */
	         + 2 + (IEEE80211_RATE_MAXSIZE - IEEE80211_RATE_SIZE)
		 + (ic->ic_caps & IEEE80211_C_WME ?	/* WME */
			sizeof(struct ieee80211_wme_param) : 0)
		 + (ic->ic_caps & IEEE80211_C_WPA ?	/* WPA 1+2 */
			2*sizeof(struct ieee80211_ie_wpa) : 0)
		 ;
	m = ieee80211_getmgtframe(&frm, pktlen);
	if (m == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"%s: cannot get buf; size %u\n", __func__, pktlen);
		ic->ic_stats.is_tx_nobuf++;
		return NULL;
	}

	memset(frm, 0, 8);	/* XXX timestamp is set by hardware/driver */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	bo->bo_caps = (u_int16_t *)frm;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;
	*frm++ = IEEE80211_ELEMID_SSID;
	if ((ic->ic_flags & IEEE80211_F_HIDESSID) == 0) {
		*frm++ = ni->ni_esslen;
		memcpy(frm, ni->ni_essid, ni->ni_esslen);
		frm += ni->ni_esslen;
	} else
		*frm++ = 0;
	frm = ieee80211_add_rates(frm, rs);
	if (ic->ic_curmode != IEEE80211_MODE_FH) {
		*frm++ = IEEE80211_ELEMID_DSPARMS;
		*frm++ = 1;
		*frm++ = ieee80211_chan2ieee(ic, ni->ni_chan);
	}
	bo->bo_tim = frm;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
		bo->bo_tim_len = 0;
	} else {
		struct ieee80211_tim_ie *tie = (struct ieee80211_tim_ie *) frm;

		tie->tim_ie = IEEE80211_ELEMID_TIM;
		tie->tim_len = 4;	/* length */
		tie->tim_count = 0;	/* DTIM count */ 
		tie->tim_period = ic->ic_dtim_period;	/* DTIM period */
		tie->tim_bitctl = 0;	/* bitmap control */
		tie->tim_bitmap[0] = 0;	/* Partial Virtual Bitmap */
		frm += sizeof(struct ieee80211_tim_ie);
		bo->bo_tim_len = 1;
	}
	bo->bo_trailer = frm;
	if (ic->ic_flags & IEEE80211_F_WME) {
		bo->bo_wme = frm;
		frm = ieee80211_add_wme_param(frm, &ic->ic_wme);
		ic->ic_flags &= ~IEEE80211_F_WMEUPDATE;
	}
	if (ic->ic_flags & IEEE80211_F_WPA)
		frm = ieee80211_add_wpa(frm, ic);
	if (ic->ic_curmode == IEEE80211_MODE_11G)
		frm = ieee80211_add_erp(frm, ic);
	efrm = ieee80211_add_xrates(frm, rs);
	bo->bo_trailer_len = efrm - bo->bo_trailer;
	m->m_pkthdr.len = m->m_len = efrm - mtod(m, u_int8_t *);

	M_PREPEND(m, sizeof(struct ieee80211_frame), M_DONTWAIT);
	IASSERT(m != NULL, ("no space for 802.11 header?"));
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_BEACON;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	IEEE80211_ADDR_COPY(wh->i_addr1, ifp->if_broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
	*(u_int16_t *)wh->i_seq = 0;

	return m;
}

/*
 * Update the dynamic parts of a beacon frame based on the current state.
 */
int
ieee80211_beacon_update(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_beacon_offsets *bo, struct mbuf *m, int mcast)
{
	int len_changed = 0;
	u_int16_t capinfo;

	IEEE80211_BEACON_LOCK(ic);
	/* XXX faster to recalculate entirely or just changes? */
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	*bo->bo_caps = htole16(capinfo);

	if (ic->ic_flags & IEEE80211_F_WME) {
		struct ieee80211_wme_state *wme = &ic->ic_wme;

		/*
		 * Check for agressive mode change.  When there is
		 * significant high priority traffic in the BSS
		 * throttle back BE traffic by using conservative
		 * parameters.  Otherwise BE uses agressive params
		 * to optimize performance of legacy/non-QoS traffic.
		 */
		if (wme->wme_flags & WME_F_AGGRMODE) {
			if (wme->wme_hipri_traffic >
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, disable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags &= ~WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
			} else
				wme->wme_hipri_traffic = 0;
		} else {
			if (wme->wme_hipri_traffic <=
			    wme->wme_hipri_switch_thresh) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
				    "%s: traffic %u, enable aggressive mode\n",
				    __func__, wme->wme_hipri_traffic);
				wme->wme_flags |= WME_F_AGGRMODE;
				ieee80211_wme_updateparams_locked(ic);
				wme->wme_hipri_traffic = 0;
			} else
				wme->wme_hipri_traffic =
					wme->wme_hipri_switch_hysteresis;
		}
		if (ic->ic_flags & IEEE80211_F_WMEUPDATE) {
			(void) ieee80211_add_wme_param(bo->bo_wme, wme);
			ic->ic_flags &= ~IEEE80211_F_WMEUPDATE;
		}
	}

#ifndef IEEE80211_NO_HOSTAP
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {	/* NB: no IBSS support*/
		struct ieee80211_tim_ie *tie =
			(struct ieee80211_tim_ie *) bo->bo_tim;
		if (ic->ic_flags & IEEE80211_F_TIMUPDATE) {
			u_int timlen, timoff, i;
			/* 
			 * ATIM/DTIM needs updating.  If it fits in the
			 * current space allocated then just copy in the
			 * new bits.  Otherwise we need to move any trailing
			 * data to make room.  Note that we know there is
			 * contiguous space because ieee80211_beacon_allocate
			 * insures there is space in the mbuf to write a
			 * maximal-size virtual bitmap (based on ic_max_aid).
			 */
			/*
			 * Calculate the bitmap size and offset, copy any
			 * trailer out of the way, and then copy in the
			 * new bitmap and update the information element.
			 * Note that the tim bitmap must contain at least
			 * one byte and any offset must be even.
			 */
			if (ic->ic_ps_pending != 0) {
				timoff = 128;		/* impossibly large */
				for (i = 0; i < ic->ic_tim_len; i++)
					if (ic->ic_tim_bitmap[i]) {
						timoff = i &~ 1;
						break;
					}
				IASSERT(timoff != 128, ("tim bitmap empty!"));
				for (i = ic->ic_tim_len-1; i >= timoff; i--)
					if (ic->ic_tim_bitmap[i])
						break;
				timlen = 1 + (i - timoff);
			} else {
				timoff = 0;
				timlen = 1;
			}
			if (timlen != bo->bo_tim_len) {
				/* copy up/down trailer */
				ovbcopy(bo->bo_trailer, tie->tim_bitmap+timlen,
					bo->bo_trailer_len);
				bo->bo_trailer = tie->tim_bitmap+timlen;
				bo->bo_wme = bo->bo_trailer;
				bo->bo_tim_len = timlen;

				/* update information element */
				tie->tim_len = 3 + timlen;
				tie->tim_bitctl = timoff;
				len_changed = 1;
			}
			memcpy(tie->tim_bitmap, ic->ic_tim_bitmap + timoff,
				bo->bo_tim_len);

			ic->ic_flags &= ~IEEE80211_F_TIMUPDATE;

			IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
				"%s: TIM updated, pending %u, off %u, len %u\n",
				__func__, ic->ic_ps_pending, timoff, timlen);
		}
		/* count down DTIM period */
		if (tie->tim_count == 0)
			tie->tim_count = tie->tim_period - 1;
		else
			tie->tim_count--;
		/* update state for buffered multicast frames on DTIM */
		if (mcast && (tie->tim_count == 1 || tie->tim_period == 1))
			tie->tim_bitctl |= 1;
		else
			tie->tim_bitctl &= ~1;
	}
#endif /* !IEEE80211_NO_HOSTAP */
	IEEE80211_BEACON_UNLOCK(ic);

	return len_changed;
}

/*
 * Save an outbound packet for a node in power-save sleep state.
 * The new packet is placed on the node's saved queue, and the TIM
 * is changed, if necessary.
 */
void
ieee80211_pwrsave(struct ieee80211com *ic, struct ieee80211_node *ni, 
		  struct mbuf *m)
{
	int qlen, age;

	IEEE80211_NODE_SAVEQ_LOCK(ni);
	if (IF_QFULL(&ni->ni_savedq)) {
		IF_DROP(&ni->ni_savedq);
		IEEE80211_NODE_SAVEQ_UNLOCK(ni);
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"[%s] pwr save q overflow, drops %d (size %d)\n",
			ether_sprintf(ni->ni_macaddr), 
			ni->ni_savedq.ifq_drops, IEEE80211_PS_MAX_QUEUE);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_dumppkts(ic))
			ieee80211_dump_pkt(mtod(m, void *), m->m_len, -1, -1);
#endif
		m_freem(m);
		return;
	}
	/*
	 * Tag the frame with its expiry time and insert
	 * it in the queue.  The aging interval is 4 times
	 * the listen interval specified by the station. 
	 * Frames that sit around too long are reclaimed
	 * using this information.
	 */
	/* XXX handle overflow? */
	age = ((ni->ni_intval * ic->ic_bintval) << 2) / 1024; /* TU -> secs */
	_IEEE80211_NODE_SAVEQ_ENQUEUE(ni, m, qlen, age);
	IEEE80211_NODE_SAVEQ_UNLOCK(ni);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		"[%s] save frame with age %d, %u now queued\n",
		ether_sprintf(ni->ni_macaddr), age, qlen);

	if (qlen == 1)
		ic->ic_set_tim(ni, 1);
}

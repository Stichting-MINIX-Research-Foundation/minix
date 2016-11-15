/*	$NetBSD: in_selsrc.c,v 1.16 2015/09/21 13:32:26 skrll Exp $	*/

/*-
 * Copyright (c) 2005 David Young.  All rights reserved.
 *
 * This code was written by David Young.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in_selsrc.c,v 1.16 2015/09/21 13:32:26 skrll Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_inet_conf.h"
#endif

#include <lib/libkern/libkern.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syslog.h>

#include <net/if.h>

#include <net/if_ether.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_ifattach.h>
#include <netinet/in_pcb.h>
#include <netinet/if_inarp.h>
#include <netinet/ip_mroute.h>
#include <netinet/igmp_var.h>
#include <netinet/in_selsrc.h>

#ifdef INET
struct score_src_name {
	const char		*sn_name;
	const in_score_src_t	sn_score_src;
};

static const struct sysctlnode *in_domifattach_sysctl(struct in_ifsysctl *);
static int in_preference(const struct in_addr *, int, int,
    const struct in_addr *);
static int in_index(const struct in_addr *, int, int, const struct in_addr *);
static int in_matchlen(const struct in_addr *, int, int,
    const struct in_addr *);
static int in_match_category(const struct in_addr *, int, int,
    const struct in_addr *);
static size_t in_get_selectsrc(const struct in_ifselsrc *, char *,
    const size_t);
static int in_set_selectsrc(struct in_ifselsrc *, char *buf);
static int in_sysctl_selectsrc(SYSCTLFN_PROTO);
static in_score_src_t name_to_score_src(const char *);
static const char *score_src_to_name(const in_score_src_t);
static void in_score(const in_score_src_t *, int *, int *,
    const struct in_addr *, int, int, const struct in_addr *);

static const struct score_src_name score_src_names[] = {
	  {"same-category", in_match_category}
	, {"common-prefix-len", in_matchlen}
	, {"index", in_index}
	, {"preference", in_preference}
	, {NULL, NULL}
};

static const struct in_ifselsrc initial_iss = { 0, {NULL} };

static struct in_ifselsrc default_iss = { 0, {in_index} };

#ifdef GETIFA_DEBUG
int in_selsrc_debug = 0;
#endif /* GETIFA_DEBUG */

SYSCTL_SETUP(sysctl_selectsrc_setup, "sysctl selectsrc subtree setup")
{
	int rc;
	const struct sysctlnode *rnode, *cnode;

	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "inet",
	    NULL, NULL, 0, NULL, 0, CTL_NET, PF_INET, CTL_EOL)) != 0) {
		printf("%s: could not create net.inet, rc = %d\n", __func__,
		    rc);
		return;
	}
	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "ip",
	    NULL, NULL, 0, NULL, 0,
	    CTL_NET, PF_INET, IPPROTO_IP, CTL_EOL)) != 0) {
		printf("%s: could not create net.inet.ip, rc = %d\n", __func__,
		    rc);
		return;
	}
	if ((rc = sysctl_createv(clog, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "selectsrc",
	    NULL, NULL, 0, NULL, 0,
	    CTL_NET, PF_INET, IPPROTO_IP, CTL_CREATE, CTL_EOL)) != 0) {
		printf("%s: could not create net.inet.ip.selectsrc, "
		       "rc = %d\n", __func__, rc);
		return;
	}
#ifdef GETIFA_DEBUG
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_PERMANENT|CTLFLAG_READWRITE, CTLTYPE_INT, "debug",
	    SYSCTL_DESCR("enable source-selection debug messages"),
	    NULL, 0, &in_selsrc_debug, 0, CTL_CREATE, CTL_EOL)) != 0) {
		printf("%s: could not create net.inet.ip.selectsrc.debug, "
		       "rc = %d\n", __func__, rc);
		return;
	}
#endif /* GETIFA_DEBUG */
	if ((rc = sysctl_createv(clog, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_STRUCT, "default",
	    SYSCTL_DESCR("default source selection policy"),
	    in_sysctl_selectsrc, 0, &default_iss, IN_SELECTSRC_LEN,
	    CTL_CREATE, CTL_EOL)) != 0) {
		printf(
		    "%s: could not create net.inet.ip.selectsrc.default (%d)\n",
		    __func__, rc);
		return;
	}
}

/*
 * Score by address preference: prefer addresses with higher preference
 * number.  Preference numbers are assigned with ioctl SIOCSIFADDRPREF.
 */
static int
in_preference(const struct in_addr *src, int preference,
    int idx, const struct in_addr *dst)
{
	return preference;
}

/*
 * Score by address "index": prefer addresses nearer the head of
 * the ifaddr list.
 */
static int
in_index(const struct in_addr *src, int preference, int idx,
    const struct in_addr *dst)
{
	return -idx;
}

/*
 * Length of longest common prefix of src and dst.
 *
 * (Derived from in6_matchlen.)
 */
static int
in_matchlen(const struct in_addr *src, int preference,
    int idx, const struct in_addr *dst)
{
	int match = 0;
	const uint8_t *s = (const uint8_t *)src, *d = (const uint8_t *)dst;
	const uint8_t *lim = s + 4;
	uint_fast8_t r = 0;

	while (s < lim && (r = (*d++ ^ *s++)) == 0)
		match += 8;

	if (s == lim)
		return match;

	while ((r & 0x80) == 0) {
		match++;
		r <<= 1;
	}
	return match;
}

static enum in_category
in_categorize(const struct in_addr *s)
{
	if (IN_ANY_LOCAL(s->s_addr))
		return IN_CATEGORY_LINKLOCAL;
	else if (IN_PRIVATE(s->s_addr))
		return IN_CATEGORY_PRIVATE;
	else
		return IN_CATEGORY_OTHER;
}

static int
in_match_category(const struct in_addr *src, int preference,
    int idx, const struct in_addr *dst)
{
	enum in_category dst_c = in_categorize(dst),
	                 src_c = in_categorize(src);
#ifdef GETIFA_DEBUG
	if (in_selsrc_debug) {
		printf("%s: dst %#08" PRIx32 " categ %d, src %#08" PRIx32
		    " categ %d\n", __func__, ntohl(dst->s_addr), dst_c,
		    ntohl(src->s_addr), src_c);
	}
#endif /* GETIFA_DEBUG */

	if (dst_c == src_c)
		return 2;
	else if (dst_c == IN_CATEGORY_LINKLOCAL && src_c == IN_CATEGORY_PRIVATE)
		return 1;
	else if (dst_c == IN_CATEGORY_PRIVATE && src_c == IN_CATEGORY_LINKLOCAL)
		return 1;
	else if (dst_c == IN_CATEGORY_OTHER && src_c == IN_CATEGORY_PRIVATE)
		return 1;
	else
		return 0;
}

static void
in_score(const in_score_src_t *score_src, int *score, int *scorelenp,
    const struct in_addr *src, int preference, int idx,
    const struct in_addr *dst)
{
	int i;

	for (i = 0; i < IN_SCORE_SRC_MAX && score_src[i] != NULL; i++)
		score[i] = (*score_src[i])(src, preference, idx, dst);
	if (scorelenp != NULL)
		*scorelenp = i;
}

static int
in_score_cmp(int *score1, int *score2, int scorelen)
{
	int i;

	for (i = 0; i < scorelen; i++) {
		if (score1[i] == score2[i])
			continue;
		return score1[i] - score2[i];
	}
	return 0;
}

#ifdef GETIFA_DEBUG
static void
in_score_println(int *score, int scorelen)
{
	int i;
	const char *delim = "[";

	for (i = 0; i < scorelen; i++) {
		printf("%s%d", delim, score[i]);
		delim = ", ";
	}
	printf("]\n");
}
#endif /* GETIFA_DEBUG */

/* Scan the interface addresses on the interface ifa->ifa_ifp for
 * the source address that best matches the destination, dst0,
 * according to the source address-selection policy for this
 * interface.  If there is no better match than `ifa', return `ifa'.
 * Otherwise, return the best address.
 *
 * Note that in_getifa is called after the kernel has decided which
 * output interface to use (ifa->ifa_ifp), and in_getifa will not
 * scan an address belonging to any other interface.
 */
struct ifaddr *
in_getifa(struct ifaddr *ifa, const struct sockaddr *dst0)
{
	const in_score_src_t *score_src;
	int idx, scorelen;
	const struct sockaddr_in *dst, *src;
	struct ifaddr *alt_ifa, *best_ifa;
	struct ifnet *ifp;
	struct in_ifsysctl *isc;
	struct in_ifselsrc *iss;
	int best_score[IN_SCORE_SRC_MAX], score[IN_SCORE_SRC_MAX];
	struct in_ifaddr *ia;

	if (ifa->ifa_addr->sa_family != AF_INET ||
	    dst0 == NULL || dst0->sa_family != AF_INET) {	/* Possible. */
		ifa->ifa_seqno = NULL;
		return ifa;
	}

	ifp = ifa->ifa_ifp;
	KASSERT(ifp->if_afdata[AF_INET] != NULL);
	isc = ((struct in_ifinfo *)(ifp)->if_afdata[AF_INET])->ii_selsrc;
	if (isc != NULL && isc->isc_selsrc != NULL &&
	    isc->isc_selsrc->iss_score_src[0] != NULL)
		iss = isc->isc_selsrc;
	else
		iss = &default_iss;
	score_src = &iss->iss_score_src[0];

	dst = (const struct sockaddr_in *)dst0;

	best_ifa = ifa;

	/* Find out the index of this ifaddr. */
	idx = 0;
	IFADDR_FOREACH(alt_ifa, ifa->ifa_ifp) {
		if (alt_ifa == best_ifa)
			break;
		idx++;
	}
	in_score(score_src, best_score, &scorelen, &IA_SIN(best_ifa)->sin_addr,
	    best_ifa->ifa_preference, idx, &dst->sin_addr);

#ifdef GETIFA_DEBUG
	if (in_selsrc_debug) {
		printf("%s: enter dst %#" PRIx32 " src %#" PRIx32 " score ",
		    __func__, ntohl(dst->sin_addr.s_addr),
		    ntohl(satosin(best_ifa->ifa_addr)->sin_addr.s_addr));
		in_score_println(best_score, scorelen);
	}
#endif /* GETIFA_DEBUG */

	idx = -1;
	IFADDR_FOREACH(alt_ifa, ifa->ifa_ifp) {
		++idx;
		src = IA_SIN(alt_ifa);

		if (alt_ifa == ifa || src->sin_family != AF_INET)
			continue;
		ia = (struct in_ifaddr *)alt_ifa;
		if (ia->ia4_flags & IN_IFF_NOTREADY)
			continue;

		in_score(score_src, score, NULL, &src->sin_addr,
		         alt_ifa->ifa_preference, idx, &dst->sin_addr);

#ifdef GETIFA_DEBUG
		if (in_selsrc_debug) {
			printf("%s: src %#" PRIx32 " score ", __func__,
			    ntohl(src->sin_addr.s_addr));
			in_score_println(score, scorelen);
		}
#endif /* GETIFA_DEBUG */

		if (in_score_cmp(score, best_score, scorelen) > 0) {
			(void)memcpy(best_score, score, sizeof(best_score));
			best_ifa = alt_ifa;
		}
	}

	ia = (struct in_ifaddr *)best_ifa;
	if (ia->ia4_flags & IN_IFF_NOTREADY)
		return NULL;

#ifdef GETIFA_DEBUG
	if (in_selsrc_debug) {
		printf("%s: choose src %#" PRIx32 " score ", __func__,
		    ntohl(IA_SIN(best_ifa)->sin_addr.s_addr));
		in_score_println(best_score, scorelen);
	}
#endif /* GETIFA_DEBUG */

	best_ifa->ifa_seqno = &iss->iss_seqno;
	return best_ifa;
}

static in_score_src_t
name_to_score_src(const char *name)
{
	int i;

	for (i = 0; score_src_names[i].sn_name != NULL; i++) {
		if (strcmp(score_src_names[i].sn_name, name) == 0)
			return score_src_names[i].sn_score_src;
	}
	return NULL;
}

static const char *
score_src_to_name(const in_score_src_t score_src)
{
	int i;
	for (i = 0; score_src_names[i].sn_name != NULL; i++) {
		if (score_src == score_src_names[i].sn_score_src)
			return score_src_names[i].sn_name;
	}
	return "<unknown>";
}

static size_t
in_get_selectsrc(const struct in_ifselsrc *iss, char *buf0,
    const size_t buflen0)
{
	int i, rc;
	char *buf = buf0;
	const char *delim;
	size_t buflen = buflen0;

	KASSERT(buflen >= 1);

	for (delim = "", i = 0;
	     i < IN_SCORE_SRC_MAX && iss->iss_score_src[i] != NULL;
	     delim = ",", i++) {
		rc = snprintf(buf, buflen, "%s%s",
		    delim, score_src_to_name(iss->iss_score_src[i]));
		if (rc == -1)
			return buflen0 - buflen;
		if (rc >= buflen)
			return buflen0 + rc - buflen;
		buf += rc;
		buflen -= rc;
	}
	if (buf == buf0)
		*buf++ = '\0';
	return buf - buf0;
}

static int
in_set_selectsrc(struct in_ifselsrc *iss, char *buf)
{
	int i, s;
	char *next = buf;
	const char *name;
	in_score_src_t score_src;
	in_score_src_t scorers[IN_SCORE_SRC_MAX];

	memset(&scorers, 0, sizeof(scorers));
	for (i = 0;
	     (name = strsep(&next, ",")) != NULL && i < IN_SCORE_SRC_MAX;
	     i++) {
		if (strcmp(name, "") == 0)
			break;
		if ((score_src = name_to_score_src(name)) == NULL)
			return EINVAL;
		scorers[i] = score_src;
	}
	if (i == IN_SCORE_SRC_MAX && name != NULL)
		return EFBIG;
	s = splnet();
	(void)memcpy(iss->iss_score_src, scorers, sizeof(iss->iss_score_src));
        /* If iss affects a specific interface that used to use
         * the default policy, increase the sequence number on the
         * default policy, forcing routes that cache a source
         * (rt_ifa) found by the default policy to refresh their
         * cache.
	 */
	if (iss != &default_iss && iss->iss_score_src[0] == NULL &&
	    scorers[0] != NULL)
		default_iss.iss_seqno++;
	iss->iss_seqno++;
	splx(s);
	return 0;
}

/*
 * sysctl helper routine for net.inet.ip.interfaces.<interface>.selectsrc.
 * Pulls the old value out as a human-readable string, interprets
 * and records the new value.
 */
static int
in_sysctl_selectsrc(SYSCTLFN_ARGS)
{
	char policy[IN_SELECTSRC_LEN];
	int error;
	struct sysctlnode node;
	struct in_ifselsrc *iss;

	node = *rnode;
	iss = (struct in_ifselsrc *)node.sysctl_data;
	if (oldp != NULL &&
	    (error = in_get_selectsrc(iss, policy, sizeof(policy))) >= sizeof(policy))
		return error;
	node.sysctl_data = &policy[0];
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	return in_set_selectsrc(iss, policy);
}

static const struct sysctlnode *
in_domifattach_sysctl(struct in_ifsysctl *isc)
{
	int rc;
	const struct sysctlnode *rnode;

	if ((rc = sysctl_createv(&isc->isc_log, 0, NULL, &rnode,
	                         CTLFLAG_READONLY, CTLTYPE_NODE,
				 "interfaces", NULL,
				 NULL, 0, NULL, 0,
				 CTL_NET, PF_INET, IPPROTO_IP, CTL_CREATE,
				 CTL_EOL)) != 0) {
		printf("%s: could not create net.inet.ip.interfaces, rc = %d\n",
		    __func__, rc);
		return NULL;
	}
	if ((rc = sysctl_createv(&isc->isc_log, 0, &rnode, &rnode,
	                         CTLFLAG_READONLY, CTLTYPE_NODE,
				 isc->isc_ifp->if_xname,
				 SYSCTL_DESCR("interface ip options"),
				 NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL)) != 0) {
		printf("%s: could not create net.inet.ip.interfaces.%s, "
		       "rc = %d\n", __func__, isc->isc_ifp->if_xname, rc);
		goto err;
	}
	if ((rc = sysctl_createv(&isc->isc_log, 0, &rnode, &rnode,
	                         CTLFLAG_READWRITE, CTLTYPE_STRING,
				 "selectsrc",
				 SYSCTL_DESCR("source selection policy"),
				 in_sysctl_selectsrc, 0,
				 (void *)isc->isc_selsrc, IN_SELECTSRC_LEN,
				 CTL_CREATE, CTL_EOL)) != 0) {
		printf(
		    "%s: could not create net.inet.ip.%s.selectsrc, rc = %d\n",
		    __func__, isc->isc_ifp->if_xname, rc);
		goto err;
	}
	return rnode;
err:
	sysctl_teardown(&isc->isc_log);
	return NULL;
}

void *
in_selsrc_domifattach(struct ifnet *ifp)
{
	struct in_ifsysctl *isc;
	struct in_ifselsrc *iss;

	isc = (struct in_ifsysctl *)malloc(sizeof(*isc), M_IFADDR,
	    M_WAITOK | M_ZERO);

	iss = (struct in_ifselsrc *)malloc(sizeof(*iss), M_IFADDR,
	    M_WAITOK | M_ZERO);

	memcpy(&iss->iss_score_src[0], &initial_iss.iss_score_src[0],
	    MIN(sizeof(iss->iss_score_src), sizeof(initial_iss.iss_score_src)));

	isc->isc_ifp = ifp;
	isc->isc_selsrc = iss;

	if (in_domifattach_sysctl(isc) == NULL)
		goto err;

	return isc;
err:
	free(iss, M_IFADDR);
	free(isc, M_IFADDR);
	return NULL;
}

void
in_selsrc_domifdetach(struct ifnet *ifp, void *aux)
{
	struct in_ifsysctl *isc;
	struct in_ifselsrc *iss;

	if (aux == NULL)
		return;
	isc = (struct in_ifsysctl *)aux;
	iss = isc->isc_selsrc;
	sysctl_teardown(&isc->isc_log);
	free(isc, M_IFADDR);
	free(iss, M_IFADDR);
}
#endif /* INET */

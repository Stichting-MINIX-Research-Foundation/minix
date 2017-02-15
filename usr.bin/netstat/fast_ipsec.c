/*	$NetBSD: fast_ipsec.c,v 1.20 2013/04/15 21:20:39 christos Exp $ */
/* 	$FreeBSD: src/tools/tools/crypto/ipsecstats.c,v 1.1.4.1 2003/06/03 00:13:13 sam Exp $ */

/*-
 * Copyright (c) 2003, 2004 Jonathan Stone
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 *
 * $FreeBSD: src/tools/tools/crypto/ipsecstats.c,v 1.1.4.1 2003/06/03 00:13:13 sam Exp $
 */

#include <sys/cdefs.h>
#ifndef lint
#ifdef __NetBSD__
__RCSID("$NetBSD: fast_ipsec.c,v 1.20 2013/04/15 21:20:39 christos Exp $");
#endif
#endif /* not lint*/

/* Kernel headers required, but not included, by netstat.h */
#include <sys/types.h>
#include <sys/socket.h>

/* Kernel headers for sysctl(3). */
#include <sys/param.h>
#include <sys/sysctl.h>

/* Kernel headers for FAST_IPSEC statistics */
#include <net/pfkeyv2.h>
#include <netipsec/esp_var.h>
#include <netipsec/ah_var.h>
#include <netipsec/ipip_var.h>
#include <netipsec/ipcomp_var.h>
#include <netipsec/ipsec_var.h>

#include <machine/int_fmtio.h>

#include <kvm.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "netstat.h"

/*
 * Table-driven mapping from SADB algorithm codes to string names.
 */
struct alg {
	int		a;
	const char	*name;
};
static const struct alg aalgs[] = {
	{ SADB_AALG_NONE,	"none", },
	{ SADB_AALG_MD5HMAC,	"hmac-md5", },
	{ SADB_AALG_SHA1HMAC,	"hmac-sha1", },
	{ SADB_X_AALG_MD5,	"md5", },
	{ SADB_X_AALG_SHA,	"sha", },
	{ SADB_X_AALG_NULL,	"null", },
	{ SADB_X_AALG_SHA2_256,	"hmac-sha2-256", },
	{ SADB_X_AALG_SHA2_384,	"hmac-sha2-384", },
	{ SADB_X_AALG_SHA2_512,	"hmac-sha2-512", },
	{ SADB_X_AALG_AES_XCBC_MAC, "aes-xcbc-mac", },
	{ SADB_X_AALG_AES128GMAC, "aes-128-gmac", },
	{ SADB_X_AALG_AES192GMAC, "aes-192-gmac", },
	{ SADB_X_AALG_AES256GMAC, "aes-256-gmac", },
};
static const struct alg espalgs[] = {
	{ SADB_EALG_NONE,	"none", },
	{ SADB_EALG_DESCBC,	"des-cbc", },
	{ SADB_EALG_3DESCBC,	"3des-cbc", },
	{ SADB_EALG_NULL,	"null", },
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
	{ SADB_X_EALG_RIJNDAELCBC, "aes-cbc", },
	{ SADB_X_EALG_CAMELLIACBC, "camellia-cbc", },
	{ SADB_X_EALG_AESCTR,	"aes-ctr", },
	{ SADB_X_EALG_AESGCM16,	"aes-gcm-16", },
	{ SADB_X_EALG_AESGMAC, "aes-gmac", },
};
static const struct alg ipcompalgs[] = {
	{ SADB_X_CALG_NONE,	"none", },
	{ SADB_X_CALG_OUI,	"oui", },
	{ SADB_X_CALG_DEFLATE,	"deflate", },
	{ SADB_X_CALG_LZS,	"lzs", },
};
#define	N(a)	(sizeof(a)/sizeof(a[0]))

static const char*
algname(int a, const struct alg algs[], int nalgs)
{
	static char buf[80];
	int i;

	for (i = 0; i < nalgs; i++)
		if (algs[i].a == a)
			return algs[i].name;
	snprintf(buf, sizeof(buf), "alg#%u", a);
	return buf;
}

/*
 * Print the fast_ipsec statistics.
 * Since NetBSD's netstat(1) seems not to find us for "netstat -s", 
 * but does(?) find KAME, be prepared to be called explicitly from
 * netstat's main program for "netstat -s"; but silently do nothing
 * if that happens when we are running on KAME IPsec.
 */
void
fast_ipsec_stats(u_long off, const char *name)
{
	uint64_t ipsecstats[IPSEC_NSTATS];
	uint64_t ahstats[AH_NSTATS];
	uint64_t espstats[ESP_NSTATS];
	uint64_t ipcs[IPCOMP_NSTATS];
	uint64_t ipips[IPIP_NSTATS];
	int status;
	size_t slen;
	int i;

	if (! use_sysctl) {
		warnx("IPsec stats not available via KVM.");
		return;
	}

	memset(ipsecstats, 0, sizeof(ipsecstats));
	memset(ahstats, 0, sizeof(ahstats));
	memset(espstats, 0, sizeof(espstats));
	memset(ipcs, 0, sizeof(ipcs));
	memset(ipips, 0, sizeof(ipips));

	slen = sizeof(ipsecstats);
	status = sysctlbyname("net.inet.ipsec.ipsecstats", ipsecstats, &slen,
			      NULL, 0);
	if (status < 0) {
		if (errno == ENOENT)
			return;
		if (errno != ENOMEM)
			err(1, "net.inet.ipsec.ipsecstats");
	}

	slen = sizeof (ahstats);
	status = sysctlbyname("net.inet.ah.ah_stats", ahstats, &slen, NULL, 0);
	if (status < 0 && errno != ENOMEM)
		err(1, "net.inet.ah.ah_stats");

	slen = sizeof (espstats);
	status = sysctlbyname("net.inet.esp.esp_stats", espstats, &slen, NULL, 0);
	if (status < 0 && errno != ENOMEM)
		err(1, "net.inet.esp.esp_stats");

	slen = sizeof(ipcs);
	status = sysctlbyname("net.inet.ipcomp.ipcomp_stats", ipcs, &slen, NULL, 0);
	if (status < 0 && errno != ENOMEM)
		err(1, "net.inet.ipcomp.ipcomp_stats");

	slen = sizeof(ipips);
	status = sysctlbyname("net.inet.ipip.ipip_stats", ipips, &slen, NULL, 0);
	if (status < 0 && errno != ENOMEM)
		err(1, "net.inet.ipip.ipip_stats");

	printf("(Fast) IPsec:\n");

#define	STAT(x,fmt)	if ((x) || sflag <= 1) printf("\t%"PRIu64" " fmt "\n", x)
	if (ipsecstats[IPSEC_STAT_IN_POLVIO]+ipsecstats[IPSEC_STAT_OUT_POLVIO])
		printf("\t%"PRIu64" policy violations: %"PRIu64" input %"PRIu64" output\n",
		        ipsecstats[IPSEC_STAT_IN_POLVIO] + ipsecstats[IPSEC_STAT_OUT_POLVIO],
			ipsecstats[IPSEC_STAT_IN_POLVIO], ipsecstats[IPSEC_STAT_OUT_POLVIO]);
	STAT(ipsecstats[IPSEC_STAT_OUT_NOSA], "no SA found (output)");
	STAT(ipsecstats[IPSEC_STAT_OUT_NOMEM], "no memory available (output)");
	STAT(ipsecstats[IPSEC_STAT_OUT_NOROUTE], "no route available (output)");
	STAT(ipsecstats[IPSEC_STAT_OUT_INVAL], "generic errors (output)");
	STAT(ipsecstats[IPSEC_STAT_OUT_BUNDLESA], "bundled SA processed (output)");
	STAT(ipsecstats[IPSEC_STAT_SPDCACHELOOKUP], "SPD cache lookups");
	STAT(ipsecstats[IPSEC_STAT_SPDCACHEMISS], "SPD cache misses");
#undef STAT
	printf("\n");
	
	printf("IPsec ah:\n");
#define	AHSTAT(x,fmt)	if ((x) || sflag <= 1) printf("\t%"PRIu64" ah " fmt "\n", x)
	AHSTAT(ahstats[AH_STAT_INPUT],   "input packets processed");
	AHSTAT(ahstats[AH_STAT_OUTPUT],  "output packets processed");
	AHSTAT(ahstats[AH_STAT_HDROPS],  "headers too short");
	AHSTAT(ahstats[AH_STAT_NOPF],    "headers for unsupported address family");
	AHSTAT(ahstats[AH_STAT_NOTDB],   "packets with no SA");
	AHSTAT(ahstats[AH_STAT_BADKCR], "packets dropped by crypto returning NULL mbuf");
	AHSTAT(ahstats[AH_STAT_BADAUTH], "packets with bad authentication");
	AHSTAT(ahstats[AH_STAT_NOXFORM], "packets with no xform");
	AHSTAT(ahstats[AH_STAT_QFULL], "packets dropped due to queue full");
	AHSTAT(ahstats[AH_STAT_WRAP],  "packets dropped for replay counter wrap");
	AHSTAT(ahstats[AH_STAT_REPLAY],  "packets dropped for possible replay");
	AHSTAT(ahstats[AH_STAT_BADAUTHL],"packets dropped for bad authenticator length");
	AHSTAT(ahstats[AH_STAT_INVALID], "packets with an invalid SA");
	AHSTAT(ahstats[AH_STAT_TOOBIG],  "packets too big");
	AHSTAT(ahstats[AH_STAT_PDROPS],  "packets blocked due to policy");
	AHSTAT(ahstats[AH_STAT_CRYPTO],  "failed crypto requests");
	AHSTAT(ahstats[AH_STAT_TUNNEL],  "tunnel sanity check failures");

	printf("\tah histogram:\n");
	for (i = 0; i < AH_ALG_MAX; i++)
		if (ahstats[AH_STAT_HIST + i])
			printf("\t\tah packets with %s: %"PRIu64"\n"
				, algname(i, aalgs, N(aalgs))
				, ahstats[AH_STAT_HIST + i]
			);
	AHSTAT(ahstats[AH_STAT_IBYTES], "bytes received");
	AHSTAT(ahstats[AH_STAT_OBYTES], "bytes transmitted");
#undef AHSTAT
	printf("\n");

	printf("IPsec esp:\n");
#define	ESPSTAT(x,fmt) if ((x) || sflag <= 1) printf("\t%"PRIu64" esp " fmt "\n", x)
	ESPSTAT(espstats[ESP_STAT_INPUT],"input packets processed");
	ESPSTAT(espstats[ESP_STAT_OUTPUT],"output packets processed");
	ESPSTAT(espstats[ESP_STAT_HDROPS],"headers too short");
	ESPSTAT(espstats[ESP_STAT_NOPF], "headers for unsupported address family");
	ESPSTAT(espstats[ESP_STAT_NOTDB],"packets with no SA");
	ESPSTAT(espstats[ESP_STAT_BADKCR],"packets dropped by crypto returning NULL mbuf");
	ESPSTAT(espstats[ESP_STAT_QFULL],"packets dropped due to queue full");
	ESPSTAT(espstats[ESP_STAT_NOXFORM],"packets with no xform");
	ESPSTAT(espstats[ESP_STAT_BADILEN],"packets with bad ilen");
	ESPSTAT(espstats[ESP_STAT_BADENC],"packets with bad encryption");
	ESPSTAT(espstats[ESP_STAT_BADAUTH],"packets with bad authentication");
	ESPSTAT(espstats[ESP_STAT_WRAP], "packets dropped for replay counter wrap");
	ESPSTAT(espstats[ESP_STAT_REPLAY],"packets dropped for possible replay");
	ESPSTAT(espstats[ESP_STAT_INVALID],"packets with an invalid SA");
	ESPSTAT(espstats[ESP_STAT_TOOBIG],"packets too big");
	ESPSTAT(espstats[ESP_STAT_PDROPS],"packets blocked due to policy");
	ESPSTAT(espstats[ESP_STAT_CRYPTO],"failed crypto requests");
	ESPSTAT(espstats[ESP_STAT_TUNNEL],"tunnel sanity check failures");
	printf("\tesp histogram:\n");
	for (i = 0; i < ESP_ALG_MAX; i++)
		if (espstats[ESP_STAT_HIST + i])
			printf("\t\tesp packets with %s: %"PRIu64"\n"
				, algname(i, espalgs, N(espalgs))
				, espstats[ESP_STAT_HIST + i]
			);
	ESPSTAT(espstats[ESP_STAT_IBYTES], "bytes received");
	ESPSTAT(espstats[ESP_STAT_OBYTES], "bytes transmitted");
#undef ESPSTAT
	printf("IPsec ipip:\n");

#define	IPIPSTAT(x,fmt) \
	if ((x) || sflag <= 1) printf("\t%"PRIu64" ipip " fmt "\n", x)
	IPIPSTAT(ipips[IPIP_STAT_IPACKETS],"total input packets");
	IPIPSTAT(ipips[IPIP_STAT_OPACKETS],"total output packets");
	IPIPSTAT(ipips[IPIP_STAT_HDROPS],"packets too short for header length");
	IPIPSTAT(ipips[IPIP_STAT_QFULL],"packets dropped due to queue full");
	IPIPSTAT(ipips[IPIP_STAT_PDROPS],"packets blocked due to policy");
	IPIPSTAT(ipips[IPIP_STAT_SPOOF],"IP spoofing attempts");
	IPIPSTAT(ipips[IPIP_STAT_FAMILY],"protocol family mismatched");
	IPIPSTAT(ipips[IPIP_STAT_UNSPEC],"missing tunnel-endpoint address");
	IPIPSTAT(ipips[IPIP_STAT_IBYTES],"input bytes received");
	IPIPSTAT(ipips[IPIP_STAT_OBYTES],"output bytes processed");
#undef IPIPSTAT

	printf("IPsec ipcomp:\n");
#define	IPCOMP(x,fmt) \
	if ((x) || sflag <= 1) printf("\t%"PRIu64" ipcomp " fmt "\n", x)

	IPCOMP(ipcs[IPCOMP_STAT_HDROPS],"packets too short for header length");
	IPCOMP(ipcs[IPCOMP_STAT_NOPF],	"protocol family not supported");
	IPCOMP(ipcs[IPCOMP_STAT_NOTDB],	"packets with no SA");
	IPCOMP(ipcs[IPCOMP_STAT_BADKCR],"packets dropped by crypto returning NULL mbuf");
	IPCOMP(ipcs[IPCOMP_STAT_QFULL],	"queue full");
        IPCOMP(ipcs[IPCOMP_STAT_NOXFORM],"no support for transform");
	IPCOMP(ipcs[IPCOMP_STAT_WRAP],  "packets dropped for replay counter wrap");
	IPCOMP(ipcs[IPCOMP_STAT_INPUT],	"input IPcomp packets");
	IPCOMP(ipcs[IPCOMP_STAT_OUTPUT],"output IPcomp packets");
	IPCOMP(ipcs[IPCOMP_STAT_INVALID],"packets with an invalid SA");
	IPCOMP(ipcs[IPCOMP_STAT_TOOBIG],"packets decompressed as too big");
	IPCOMP(ipcs[IPCOMP_STAT_MINLEN], "packets too short to be compressed");
	IPCOMP(ipcs[IPCOMP_STAT_USELESS],"packet for which compression was useless");
	IPCOMP(ipcs[IPCOMP_STAT_PDROPS],"packets blocked due to policy");
	IPCOMP(ipcs[IPCOMP_STAT_CRYPTO],"failed crypto requests");

	printf("\tIPcomp histogram:\n");
	for (i = 0; i < IPCOMP_ALG_MAX; i++)
		if (ipcs[IPCOMP_STAT_HIST + i])
			printf("\t\tIPcomp packets with %s: %"PRIu64"\n"
				, algname(i, ipcompalgs, N(ipcompalgs))
				, ipcs[IPCOMP_STAT_HIST + i]
			);
	IPCOMP(ipcs[IPCOMP_STAT_IBYTES],"input bytes");
	IPCOMP(ipcs[IPCOMP_STAT_OBYTES],"output bytes");
#undef IPCOMP
}

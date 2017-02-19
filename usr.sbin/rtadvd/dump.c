/*	$NetBSD: dump.c,v 1.12 2015/06/05 14:09:20 roy Exp $	*/
/*	$KAME: dump.c,v 1.34 2004/06/14 05:35:59 itojun Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>

/* XXX: the following two are non-standard include files */
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "rtadvd.h"
#include "timer.h"
#include "if.h"
#include "dump.h"

static FILE *fp;

static char *ether_str(struct sockaddr_dl *);
static void if_dump(void);

static const char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};

static char *
ether_str(struct sockaddr_dl *sdl)
{
	static char hbuf[NI_MAXHOST];

	if (sdl->sdl_alen) {
		if (getnameinfo((struct sockaddr *)sdl, sdl->sdl_len,
		    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			snprintf(hbuf, sizeof(hbuf), "<invalid>");
	} else
		snprintf(hbuf, sizeof(hbuf), "NONE");

	return(hbuf);
}

static void
if_dump(void)
{
	struct rainfo *rai;
	struct prefix *pfx;
	struct rtinfo *rti;
	struct rdnss *rdns;
	struct rdnss_addr *rdnsa;
	struct dnssl *dnsl;
	struct dnssl_domain *dnsd;
	char *p, len;
	char prefixbuf[INET6_ADDRSTRLEN];
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now); /* XXX: unused in most cases */
	TAILQ_FOREACH(rai, &ralist, next) {
		fprintf(fp, "%s:\n", rai->ifname);

		fprintf(fp, "  Status: %s\n",
			(rai->ifflags & IFF_UP) ? "UP" : "DOWN");

		/* control information */
		if (rai->lastsent.tv_sec) {
			/* note that ctime() appends CR by itself */
			fprintf(fp, "  Last RA sent: %s",
				ctime((time_t *)&rai->lastsent.tv_sec));
		}
		if (rai->timer) {
			fprintf(fp, "  Next RA will be sent: %s",
				ctime((time_t *)&rai->timer->tm.tv_sec));
		}
		else
			fprintf(fp, "  RA timer is stopped");
		fprintf(fp, "  waits: %d, initcount: %d\n",
			rai->waiting, rai->initcounter);

		/* statistics */
		fprintf(fp, "  statistics: RA(out/in/inconsistent): "
		    "%llu/%llu/%llu, ",
		    (unsigned long long)rai->raoutput,
		    (unsigned long long)rai->rainput,
		    (unsigned long long)rai->rainconsistent);
		fprintf(fp, "RS(input): %llu\n",
		    (unsigned long long)rai->rsinput);

		/* interface information */
		if (rai->advlinkopt)
			fprintf(fp, "  Link-layer address: %s\n",
			    ether_str(rai->sdl));
		fprintf(fp, "  MTU: %d\n", rai->phymtu);

		/* Router configuration variables */
		fprintf(fp, "  DefaultLifetime: %d, MaxAdvInterval: %d, "
		    "MinAdvInterval: %d\n", rai->lifetime, rai->maxinterval,
		    rai->mininterval);
		fprintf(fp, "  Flags: %s%s%s, ",
		    rai->managedflg ? "M" : "", rai->otherflg ? "O" : "",
		    "");
		fprintf(fp, "Preference: %s, ",
			rtpref_str[(rai->rtpref >> 3) & 0xff]);
		fprintf(fp, "MTU: %d\n", rai->linkmtu);
		fprintf(fp, "  ReachableTime: %d, RetransTimer: %d, "
			"CurHopLimit: %d\n", rai->reachabletime,
			rai->retranstimer, rai->hoplimit);
		if (rai->clockskew)
			fprintf(fp, "  Clock skew: %dsec\n",
			    rai->clockskew);
		TAILQ_FOREACH(pfx, &rai->prefix, next) {
			if (pfx == TAILQ_FIRST(&rai->prefix))
				fprintf(fp, "  Prefixes:\n");
			fprintf(fp, "    %s/%d(",
			    inet_ntop(AF_INET6, &pfx->prefix, prefixbuf,
			    sizeof(prefixbuf)), pfx->prefixlen);
			switch (pfx->origin) {
			case PREFIX_FROM_KERNEL:
				fprintf(fp, "KERNEL, ");
				break;
			case PREFIX_FROM_CONFIG:
				fprintf(fp, "CONFIG, ");
				break;
			case PREFIX_FROM_DYNAMIC:
				fprintf(fp, "DYNAMIC, ");
				break;
			}
			if (pfx->validlifetime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "vltime: infinity");
			else
				fprintf(fp, "vltime: %ld",
					(long)pfx->validlifetime);
			if (pfx->vltimeexpire != 0)
				fprintf(fp, "(decr,expire %lld), ", (long long)
					(pfx->vltimeexpire > now.tv_sec ?
					pfx->vltimeexpire - now.tv_sec : 0));
			else
				fprintf(fp, ", ");
			if (pfx->preflifetime ==  ND6_INFINITE_LIFETIME)
				fprintf(fp, "pltime: infinity");
			else
				fprintf(fp, "pltime: %ld",
					(long)pfx->preflifetime);
			if (pfx->pltimeexpire != 0)
				fprintf(fp, "(decr,expire %lld), ", (long long)
					(pfx->pltimeexpire > now.tv_sec ?
					pfx->pltimeexpire - now.tv_sec : 0));
			else
				fprintf(fp, ", ");
			fprintf(fp, "flags: %s%s%s",
				pfx->onlinkflg ? "L" : "",
				pfx->autoconfflg ? "A" : "",
				"");
			if (pfx->timer) {
				struct timespec *rest;

				rest = rtadvd_timer_rest(pfx->timer);
				if (rest) { /* XXX: what if not? */
					fprintf(fp, ", expire in: %ld",
					    (long)rest->tv_sec);
				}
			}
			fprintf(fp, ")\n");
		}

		TAILQ_FOREACH(rti, &rai->route, next) {
			if (rti == TAILQ_FIRST(&rai->route))
				fprintf(fp, "  Route Information:\n");
			fprintf(fp, "    %s/%d (",
				inet_ntop(AF_INET6, &rti->prefix,
					  prefixbuf, sizeof(prefixbuf)),
				rti->prefixlen);
			fprintf(fp, "preference: %s, ",
				rtpref_str[0xff & (rti->rtpref >> 3)]);
			if (rti->ltime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "lifetime: infinity");
			else
				fprintf(fp, "lifetime: %ld", (long)rti->ltime);
			fprintf(fp, ")\n");
		}

		TAILQ_FOREACH(rdns, &rai->rdnss, next) {
			fprintf(fp, "  Recursive DNS Servers:\n");
			if (rdns->lifetime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "    lifetime: infinity\n");
			else
				fprintf(fp, "    lifetime: %ld\n",
				    (long)rdns->lifetime);
			TAILQ_FOREACH(rdnsa, &rdns->list, next)
				fprintf(fp, "    %s\n",
				    inet_ntop(AF_INET6, &rdnsa->addr,
				    prefixbuf, sizeof(prefixbuf)));
		}

		TAILQ_FOREACH(dnsl, &rai->dnssl, next) {
			fprintf(fp, "  DNS Search List:\n");
			if (dnsl->lifetime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "    lifetime: infinity\n");
			else
				fprintf(fp, "    lifetime: %ld\n",
				    (long)dnsl->lifetime);
			TAILQ_FOREACH(dnsd, &dnsl->list, next) {
				fprintf(fp, "    ");
				for (p = dnsd->domain, len = *p++;
				    len != 0;
				    len = *p++)
				{
					if (p != dnsd->domain)
					    fputc('.', fp);
					while(len-- != 0)	
					    fputc(*p++, fp);
				}
				fputc('\n', fp);
			}
		}
	}
}

void
rtadvd_dump_file(const char *dumpfile)
{
	syslog(LOG_DEBUG, "<%s> dump current status to %s", __func__,
	    dumpfile);

	if ((fp = fopen(dumpfile, "w")) == NULL) {
		syslog(LOG_WARNING, "<%s> open a dump file(%s): %m",
		       __func__, dumpfile);
		return;
	}

	if_dump();

	fclose(fp);
}

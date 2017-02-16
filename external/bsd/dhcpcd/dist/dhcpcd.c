#include <sys/cdefs.h>
 __RCSID("$NetBSD: dhcpcd.c,v 1.28 2015/09/04 12:25:01 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

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

const char dhcpcd_copyright[] = "Copyright (c) 2006-2015 Roy Marples";

#define _WITH_DPRINTF /* Stop FreeBSD bitching */

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "config.h"
#include "arp.h"
#include "common.h"
#include "control.h"
#include "dev.h"
#include "dhcpcd.h"
#include "dhcp6.h"
#include "duid.h"
#include "eloop.h"
#include "if.h"
#include "if-options.h"
#include "ipv4.h"
#include "ipv4ll.h"
#include "ipv6.h"
#include "ipv6nd.h"
#include "script.h"

#ifdef USE_SIGNALS
const int dhcpcd_signals[] = {
	SIGTERM,
	SIGINT,
	SIGALRM,
	SIGHUP,
	SIGUSR1,
	SIGUSR2,
	SIGPIPE
};
const size_t dhcpcd_signals_len = __arraycount(dhcpcd_signals);
#endif

#if defined(USE_SIGNALS) || !defined(THERE_IS_NO_FORK)
static pid_t
read_pid(const char *pidfile)
{
	FILE *fp;
	pid_t pid;

	if ((fp = fopen(pidfile, "r")) == NULL) {
		errno = ENOENT;
		return 0;
	}
	if (fscanf(fp, "%d", &pid) != 1)
		pid = 0;
	fclose(fp);
	return pid;
}

static int
write_pid(int fd, pid_t pid)
{

	if (ftruncate(fd, (off_t)0) == -1)
		return -1;
	lseek(fd, (off_t)0, SEEK_SET);
	return dprintf(fd, "%d\n", (int)pid);
}
#endif

static void
usage(void)
{

printf("usage: "PACKAGE"\t[-46ABbDdEGgHJKkLnpqTVw]\n"
	"\t\t[-C, --nohook hook] [-c, --script script]\n"
	"\t\t[-e, --env value] [-F, --fqdn FQDN] [-f, --config file]\n"
	"\t\t[-h, --hostname hostname] [-I, --clientid clientid]\n"
	"\t\t[-i, --vendorclassid vendorclassid] [-l, --leasetime seconds]\n"
	"\t\t[-m, --metric metric] [-O, --nooption option]\n"
	"\t\t[-o, --option option] [-Q, --require option]\n"
	"\t\t[-r, --request address] [-S, --static value]\n"
	"\t\t[-s, --inform address[/cidr]] [-t, --timeout seconds]\n"
	"\t\t[-u, --userclass class] [-v, --vendor code, value]\n"
	"\t\t[-W, --whitelist address[/cidr]] [-y, --reboot seconds]\n"
	"\t\t[-X, --blacklist address[/cidr]] [-Z, --denyinterfaces pattern]\n"
	"\t\t[-z, --allowinterfaces pattern] [interface] [...]\n"
	"       "PACKAGE"\t-k, --release [interface]\n"
	"       "PACKAGE"\t-U, --dumplease interface\n"
	"       "PACKAGE"\t--version\n"
	"       "PACKAGE"\t-x, --exit [interface]\n");
}

static void
free_globals(struct dhcpcd_ctx *ctx)
{
	struct dhcp_opt *opt;

	if (ctx->ifac) {
		for (; ctx->ifac > 0; ctx->ifac--)
			free(ctx->ifav[ctx->ifac - 1]);
		free(ctx->ifav);
		ctx->ifav = NULL;
	}
	if (ctx->ifdc) {
		for (; ctx->ifdc > 0; ctx->ifdc--)
			free(ctx->ifdv[ctx->ifdc - 1]);
		free(ctx->ifdv);
		ctx->ifdv = NULL;
	}
	if (ctx->ifcc) {
		for (; ctx->ifcc > 0; ctx->ifcc--)
			free(ctx->ifcv[ctx->ifcc - 1]);
		free(ctx->ifcv);
		ctx->ifcv = NULL;
	}

#ifdef INET
	if (ctx->dhcp_opts) {
		for (opt = ctx->dhcp_opts;
		    ctx->dhcp_opts_len > 0;
		    opt++, ctx->dhcp_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->dhcp_opts);
		ctx->dhcp_opts = NULL;
	}
#endif
#ifdef INET6
	if (ctx->nd_opts) {
		for (opt = ctx->nd_opts;
		    ctx->nd_opts_len > 0;
		    opt++, ctx->nd_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->nd_opts);
		ctx->nd_opts = NULL;
	}
	if (ctx->dhcp6_opts) {
		for (opt = ctx->dhcp6_opts;
		    ctx->dhcp6_opts_len > 0;
		    opt++, ctx->dhcp6_opts_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->dhcp6_opts);
		ctx->dhcp6_opts = NULL;
	}
#endif
	if (ctx->vivso) {
		for (opt = ctx->vivso;
		    ctx->vivso_len > 0;
		    opt++, ctx->vivso_len--)
			free_dhcp_opt_embenc(opt);
		free(ctx->vivso);
		ctx->vivso = NULL;
	}
}

static void
handle_exit_timeout(void *arg)
{
	struct dhcpcd_ctx *ctx;

	ctx = arg;
	logger(ctx, LOG_ERR, "timed out");
	if (!(ctx->options & DHCPCD_MASTER)) {
		eloop_exit(ctx->eloop, EXIT_FAILURE);
		return;
	}
	ctx->options |= DHCPCD_NOWAITIP;
	dhcpcd_daemonise(ctx);
}

static const char *
dhcpcd_af(int af)
{

	switch (af) {
	case AF_UNSPEC:
		return "IP";
	case AF_INET:
		return "IPv4";
	case AF_INET6:
		return "IPv6";
	default:
		return NULL;
	}
}

int
dhcpcd_ifafwaiting(const struct interface *ifp)
{
	unsigned long long opts;

	opts = ifp->options->options;
	if (opts & DHCPCD_WAITIP4 && !ipv4_hasaddr(ifp))
		return AF_INET;
	if (opts & DHCPCD_WAITIP6 && !ipv6_hasaddr(ifp))
		return AF_INET6;
	if (opts & DHCPCD_WAITIP &&
	    !(opts & (DHCPCD_WAITIP4 | DHCPCD_WAITIP6)) &&
	    !ipv4_hasaddr(ifp) && !ipv6_hasaddr(ifp))
		return AF_UNSPEC;
	return AF_MAX;
}

int
dhcpcd_afwaiting(const struct dhcpcd_ctx *ctx)
{
	unsigned long long opts;
	const struct interface *ifp;
	int af;

	if (!(ctx->options & DHCPCD_WAITOPTS))
		return AF_MAX;

	opts = ctx->options;
	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if (opts & (DHCPCD_WAITIP | DHCPCD_WAITIP4) &&
		    ipv4_hasaddr(ifp))
			opts &= ~(DHCPCD_WAITIP | DHCPCD_WAITIP4);
		if (opts & (DHCPCD_WAITIP | DHCPCD_WAITIP6) &&
		    ipv6_hasaddr(ifp))
			opts &= ~(DHCPCD_WAITIP | DHCPCD_WAITIP6);
		if (!(opts & DHCPCD_WAITOPTS))
			break;
	}
	if (opts & DHCPCD_WAITIP)
		af = AF_UNSPEC;
	else if (opts & DHCPCD_WAITIP4)
		af = AF_INET;
	else if (opts & DHCPCD_WAITIP6)
		af = AF_INET6;
	else
		return AF_MAX;
	return af;
}

static int
dhcpcd_ipwaited(struct dhcpcd_ctx *ctx)
{
	struct interface *ifp;
	int af;

	TAILQ_FOREACH(ifp, ctx->ifaces, next) {
		if ((af = dhcpcd_ifafwaiting(ifp)) != AF_MAX) {
			logger(ctx, LOG_DEBUG,
			    "%s: waiting for an %s address",
			    ifp->name, dhcpcd_af(af));
			return 0;
		}
	}

	if ((af = dhcpcd_afwaiting(ctx)) != AF_MAX) {
		logger(ctx, LOG_DEBUG,
		    "waiting for an %s address",
		    dhcpcd_af(af));
		return 0;
	}

	return 1;
}

/* Returns the pid of the child, otherwise 0. */
pid_t
dhcpcd_daemonise(struct dhcpcd_ctx *ctx)
{
#ifdef THERE_IS_NO_FORK
	eloop_timeout_delete(ctx->eloop, handle_exit_timeout, ctx);
	errno = ENOSYS;
	return 0;
#else
	pid_t pid;
	char buf = '\0';
	int sidpipe[2], fd;

	if (ctx->options & DHCPCD_DAEMONISE &&
	    !(ctx->options & (DHCPCD_DAEMONISED | DHCPCD_NOWAITIP)))
	{
		if (!dhcpcd_ipwaited(ctx))
			return 0;
	}

	eloop_timeout_delete(ctx->eloop, handle_exit_timeout, ctx);
	if (ctx->options & DHCPCD_DAEMONISED ||
	    !(ctx->options & DHCPCD_DAEMONISE))
		return 0;
	/* Setup a signal pipe so parent knows when to exit. */
	if (pipe(sidpipe) == -1) {
		logger(ctx, LOG_ERR, "pipe: %m");
		return 0;
	}
	logger(ctx, LOG_DEBUG, "forking to background");
	switch (pid = fork()) {
	case -1:
		logger(ctx, LOG_ERR, "fork: %m");
		return 0;
	case 0:
		setsid();
		/* Some polling methods don't survive after forking,
		 * so ensure we can requeue all our events. */
		if (eloop_requeue(ctx->eloop) == -1) {
			logger(ctx, LOG_ERR, "eloop_requeue: %m");
			eloop_exit(ctx->eloop, EXIT_FAILURE);
		}
		/* Notify parent it's safe to exit as we've detached. */
		close(sidpipe[0]);
		if (write(sidpipe[1], &buf, 1) == -1)
			logger(ctx, LOG_ERR, "failed to notify parent: %m");
		close(sidpipe[1]);
		if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			close(fd);
		}
		break;
	default:
		/* Wait for child to detach */
		close(sidpipe[1]);
		if (read(sidpipe[0], &buf, 1) == -1)
			logger(ctx, LOG_ERR, "failed to read child: %m");
		close(sidpipe[0]);
		break;
	}
	/* Done with the fd now */
	if (pid != 0) {
		logger(ctx, LOG_INFO, "forked to background, child pid %d", pid);
		write_pid(ctx->pid_fd, pid);
		close(ctx->pid_fd);
		ctx->pid_fd = -1;
		ctx->options |= DHCPCD_FORKED;
		eloop_exit(ctx->eloop, EXIT_SUCCESS);
		return pid;
	}
	ctx->options |= DHCPCD_DAEMONISED;
	return pid;
#endif
}

static void
dhcpcd_drop(struct interface *ifp, int stop)
{

	dhcp6_drop(ifp, stop ? NULL : "EXPIRE6");
	ipv6nd_drop(ifp);
	ipv6_drop(ifp);
	ipv4ll_drop(ifp);
	dhcp_drop(ifp, stop ? "STOP" : "EXPIRE");
	arp_close(ifp);
}

static void
stop_interface(struct interface *ifp)
{
	struct dhcpcd_ctx *ctx;

	ctx = ifp->ctx;
	logger(ctx, LOG_INFO, "%s: removing interface", ifp->name);
	ifp->options->options |= DHCPCD_STOPPING;

	dhcpcd_drop(ifp, 1);
	if (ifp->options->options & DHCPCD_DEPARTED)
		script_runreason(ifp, "DEPARTED");
	else
		script_runreason(ifp, "STOPPED");

	/* Delete all timeouts for the interfaces */
	eloop_q_timeout_delete(ctx->eloop, 0, NULL, ifp);

	/* Remove the interface from our list */
	TAILQ_REMOVE(ifp->ctx->ifaces, ifp, next);
	if_free(ifp);

	if (!(ctx->options & (DHCPCD_MASTER | DHCPCD_TEST)))
		eloop_exit(ctx->eloop, EXIT_FAILURE);
}

static void
configure_interface1(struct interface *ifp)
{
	struct if_options *ifo = ifp->options;
	int ra_global, ra_iface;
#ifdef INET6
	size_t i;
#endif

	/* Do any platform specific configuration */
	if_conf(ifp);

	/* If we want to release a lease, we can't really persist the
	 * address either. */
	if (ifo->options & DHCPCD_RELEASE)
		ifo->options &= ~DHCPCD_PERSISTENT;

	if (ifp->flags & IFF_POINTOPOINT && !(ifo->options & DHCPCD_INFORM))
		ifo->options |= DHCPCD_STATIC;
	if (ifp->flags & IFF_NOARP ||
	    ifo->options & (DHCPCD_INFORM | DHCPCD_STATIC))
		ifo->options &= ~DHCPCD_IPV4LL;
	if (ifp->flags & (IFF_POINTOPOINT | IFF_LOOPBACK) ||
	    !(ifp->flags & IFF_MULTICAST))
		ifo->options &= ~DHCPCD_IPV6RS;

	if (ifo->metric != -1)
		ifp->metric = (unsigned int)ifo->metric;

	if (!(ifo->options & DHCPCD_IPV4))
		ifo->options &= ~(DHCPCD_DHCP | DHCPCD_IPV4LL | DHCPCD_WAITIP4);

	if (!(ifo->options & DHCPCD_IPV6))
		ifo->options &=
		    ~(DHCPCD_IPV6RS | DHCPCD_DHCP6 | DHCPCD_WAITIP6);

	if (ifo->options & DHCPCD_SLAACPRIVATE &&
	    !(ifp->ctx->options & (DHCPCD_DUMPLEASE | DHCPCD_TEST)))
		ifo->options |= DHCPCD_IPV6RA_OWN;

	/* We want to disable kernel interface RA as early as possible. */
	if (ifo->options & DHCPCD_IPV6RS &&
	    !(ifp->ctx->options & DHCPCD_DUMPLEASE))
	{
		/* If not doing any DHCP, disable the RDNSS requirement. */
		if (!(ifo->options & (DHCPCD_DHCP | DHCPCD_DHCP6)))
			ifo->options &= ~DHCPCD_IPV6RA_REQRDNSS;
		ra_global = if_checkipv6(ifp->ctx, NULL,
		    ifp->ctx->options & DHCPCD_IPV6RA_OWN ? 1 : 0);
		ra_iface = if_checkipv6(ifp->ctx, ifp,
		    ifp->options->options & DHCPCD_IPV6RA_OWN ? 1 : 0);
		if (ra_global == -1 || ra_iface == -1)
			ifo->options &= ~DHCPCD_IPV6RS;
		else if (ra_iface == 0 &&
		    !(ifp->ctx->options & DHCPCD_TEST))
			ifo->options |= DHCPCD_IPV6RA_OWN;
	}

	/* If we haven't specified a ClientID and our hardware address
	 * length is greater than DHCP_CHADDR_LEN then we enforce a ClientID
	 * of the hardware address family and the hardware address.
	 * If there is no hardware address and no ClientID set,
	 * force a DUID based ClientID. */
	if (ifp->hwlen > DHCP_CHADDR_LEN)
		ifo->options |= DHCPCD_CLIENTID;
	else if (ifp->hwlen == 0 && !(ifo->options & DHCPCD_CLIENTID))
		ifo->options |= DHCPCD_CLIENTID | DHCPCD_DUID;

	/* Firewire and InfiniBand interfaces require ClientID and
	 * the broadcast option being set. */
	switch (ifp->family) {
	case ARPHRD_IEEE1394:	/* FALLTHROUGH */
	case ARPHRD_INFINIBAND:
		ifo->options |= DHCPCD_CLIENTID | DHCPCD_BROADCAST;
		break;
	}

	if (!(ifo->options & DHCPCD_IAID)) {
		/*
		 * An IAID is for identifying a unqiue interface within
		 * the client. It is 4 bytes long. Working out a default
		 * value is problematic.
		 *
		 * Interface name and number are not stable
		 * between different OS's. Some OS's also cannot make
		 * up their mind what the interface should be called
		 * (yes, udev, I'm looking at you).
		 * Also, the name could be longer than 4 bytes.
		 * Also, with pluggable interfaces the name and index
		 * could easily get swapped per actual interface.
		 *
		 * The MAC address is 6 bytes long, the final 3
		 * being unique to the manufacturer and the initial 3
		 * being unique to the organisation which makes it.
		 * We could use the last 4 bytes of the MAC address
		 * as the IAID as it's the most stable part given the
		 * above, but equally it's not guaranteed to be
		 * unique.
		 *
		 * Given the above, and our need to reliably work
		 * between reboots without persitent storage,
		 * generating the IAID from the MAC address is the only
		 * logical default.
		 *
		 * dhclient uses the last 4 bytes of the MAC address.
		 * dibbler uses an increamenting counter.
		 * wide-dhcpv6 uses 0 or a configured value.
		 * odhcp6c uses 1.
		 * Windows 7 uses the first 3 bytes of the MAC address
		 * and an unknown byte.
		 * dhcpcd-6.1.0 and earlier used the interface name,
		 * falling back to interface index if name > 4.
		 */
		if (ifp->hwlen >= sizeof(ifo->iaid))
			memcpy(ifo->iaid,
			    ifp->hwaddr + ifp->hwlen - sizeof(ifo->iaid),
			    sizeof(ifo->iaid));
		else {
			uint32_t len;

			len = (uint32_t)strlen(ifp->name);
			if (len <= sizeof(ifo->iaid)) {
				memcpy(ifo->iaid, ifp->name, len);
				if (len < sizeof(ifo->iaid))
					memset(ifo->iaid + len, 0,
					    sizeof(ifo->iaid) - len);
			} else {
				/* IAID is the same size as a uint32_t */
				len = htonl(ifp->index);
				memcpy(ifo->iaid, &len, sizeof(len));
			}
		}
		ifo->options |= DHCPCD_IAID;
	}

#ifdef INET6
	if (ifo->ia_len == 0 && ifo->options & DHCPCD_IPV6 &&
	    ifp->name[0] != '\0')
	{
		ifo->ia = malloc(sizeof(*ifo->ia));
		if (ifo->ia == NULL)
			logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
		else {
			ifo->ia_len = 1;
			ifo->ia->ia_type = D6_OPTION_IA_NA;
			memcpy(ifo->ia->iaid, ifo->iaid, sizeof(ifo->iaid));
			memset(&ifo->ia->addr, 0, sizeof(ifo->ia->addr));
			ifo->ia->sla = NULL;
			ifo->ia->sla_len = 0;
		}
	} else {
		for (i = 0; i < ifo->ia_len; i++) {
			if (!ifo->ia[i].iaid_set) {
				memcpy(&ifo->ia[i].iaid, ifo->iaid,
				    sizeof(ifo->ia[i].iaid));
				ifo->ia[i].iaid_set = 1;
			}
		}
	}
#endif
}

int
dhcpcd_selectprofile(struct interface *ifp, const char *profile)
{
	struct if_options *ifo;
	char pssid[PROFILE_LEN];

	if (ifp->ssid_len) {
		ssize_t r;

		r = print_string(pssid, sizeof(pssid), ESCSTRING,
		    ifp->ssid, ifp->ssid_len);
		if (r == -1) {
			logger(ifp->ctx, LOG_ERR,
			    "%s: %s: %m", ifp->name, __func__);
			pssid[0] = '\0';
		}
	} else
		pssid[0] = '\0';
	ifo = read_config(ifp->ctx, ifp->name, pssid, profile);
	if (ifo == NULL) {
		logger(ifp->ctx, LOG_DEBUG, "%s: no profile %s",
		    ifp->name, profile);
		return -1;
	}
	if (profile != NULL) {
		strlcpy(ifp->profile, profile, sizeof(ifp->profile));
		logger(ifp->ctx, LOG_INFO, "%s: selected profile %s",
		    ifp->name, profile);
	} else
		*ifp->profile = '\0';

	free_options(ifp->options);
	ifp->options = ifo;
	if (profile)
		configure_interface1(ifp);
	return 1;
}

static void
configure_interface(struct interface *ifp, int argc, char **argv,
    unsigned long long options)
{
	time_t old;

	old = ifp->options ? ifp->options->mtime : 0;
	dhcpcd_selectprofile(ifp, NULL);
	add_options(ifp->ctx, ifp->name, ifp->options, argc, argv);
	ifp->options->options |= options;
	configure_interface1(ifp);

	/* If the mtime has changed drop any old lease */
	if (ifp->options && old != 0 && ifp->options->mtime != old) {
		logger(ifp->ctx, LOG_WARNING,
		    "%s: confile file changed, expiring leases", ifp->name);
		dhcpcd_drop(ifp, 0);
	}
}

static void
dhcpcd_pollup(void *arg)
{
	struct interface *ifp = arg;
	int carrier;

	carrier = if_carrier(ifp); /* will set ifp->flags */
	if (carrier == LINK_UP && !(ifp->flags & IFF_UP)) {
		struct timespec tv;

		tv.tv_sec = 0;
		tv.tv_nsec = IF_POLL_UP * NSEC_PER_MSEC;
		eloop_timeout_add_tv(ifp->ctx->eloop, &tv, dhcpcd_pollup, ifp);
		return;
	}

	dhcpcd_handlecarrier(ifp->ctx, carrier, ifp->flags, ifp->name);
}

void
dhcpcd_handlecarrier(struct dhcpcd_ctx *ctx, int carrier, unsigned int flags,
    const char *ifname)
{
	struct interface *ifp;

	ifp = if_find(ctx->ifaces, ifname);
	if (ifp == NULL || !(ifp->options->options & DHCPCD_LINK))
		return;

	switch(carrier) {
	case LINK_UNKNOWN:
		carrier = if_carrier(ifp); /* will set ifp->flags */
		break;
	case LINK_UP:
		/* we have a carrier! Still need to check for IFF_UP */
		if (flags & IFF_UP)
			ifp->flags = flags;
		else {
			/* So we need to poll for IFF_UP as there is no
			 * kernel notification when it's set. */
			dhcpcd_pollup(ifp);
			return;
		}
		break;
	default:
		ifp->flags = flags;
	}

	/* If we here, we don't need to poll for IFF_UP any longer
	 * if generated by a kernel event. */
	eloop_timeout_delete(ifp->ctx->eloop, dhcpcd_pollup, ifp);

	if (carrier == LINK_UNKNOWN) {
		if (errno != ENOTTY) /* For example a PPP link on BSD */
			logger(ctx, LOG_ERR, "%s: carrier_status: %m", ifname);
	} else if (carrier == LINK_DOWN || (ifp->flags & IFF_UP) == 0) {
		if (ifp->carrier != LINK_DOWN) {
			if (ifp->carrier == LINK_UP)
				logger(ctx, LOG_INFO, "%s: carrier lost",
				    ifp->name);
			ifp->carrier = LINK_DOWN;
			script_runreason(ifp, "NOCARRIER");
#ifdef NOCARRIER_PRESERVE_IP
			arp_close(ifp);
			dhcp_abort(ifp);
			if_sortinterfaces(ctx);
			ipv4_preferanother(ifp);
			ipv6nd_expire(ifp, 0);
#else
			dhcpcd_drop(ifp, 0);
#endif
		}
	} else if (carrier == LINK_UP && ifp->flags & IFF_UP) {
		if (ifp->carrier != LINK_UP) {
			logger(ctx, LOG_INFO, "%s: carrier acquired",
			    ifp->name);
			ifp->carrier = LINK_UP;
#if !defined(__linux__) && !defined(__NetBSD__)
			/* BSD does not emit RTM_NEWADDR or RTM_CHGADDR when the
			 * hardware address changes so we have to go
			 * through the disovery process to work it out. */
			dhcpcd_handleinterface(ctx, 0, ifp->name);
#endif
			if (ifp->wireless) {
				uint8_t ossid[IF_SSIDSIZE];
#ifdef NOCARRIER_PRESERVE_IP
				size_t olen;

				olen = ifp->ssid_len;
#endif
				memcpy(ossid, ifp->ssid, ifp->ssid_len);
				if_getssid(ifp);
#ifdef NOCARRIER_PRESERVE_IP
				/* If we changed SSID network, drop leases */
				if (ifp->ssid_len != olen ||
				    memcmp(ifp->ssid, ossid, ifp->ssid_len))
					dhcpcd_drop(ifp, 0);
#endif
			}
			dhcpcd_initstate(ifp, 0);
			script_runreason(ifp, "CARRIER");
#ifdef NOCARRIER_PRESERVE_IP
			/* Set any IPv6 Routers we remembered to expire
			 * faster than they would normally as we
			 * maybe on a new network. */
			ipv6nd_expire(ifp, RTR_CARRIER_EXPIRE);
#endif
			/* RFC4941 Section 3.5 */
			if (ifp->options->options & DHCPCD_IPV6RA_OWN)
				ipv6_gentempifid(ifp);
			dhcpcd_startinterface(ifp);
		}
	}
}

static void
warn_iaid_conflict(struct interface *ifp, uint8_t *iaid)
{
	struct interface *ifn;
	size_t i;

	TAILQ_FOREACH(ifn, ifp->ctx->ifaces, next) {
		if (ifn == ifp)
			continue;
		if (memcmp(ifn->options->iaid, iaid,
		    sizeof(ifn->options->iaid)) == 0)
			break;
		for (i = 0; i < ifn->options->ia_len; i++) {
			if (memcmp(&ifn->options->ia[i].iaid, iaid,
			    sizeof(ifn->options->ia[i].iaid)) == 0)
				break;
		}
	}

	/* This is only a problem if the interfaces are on the same network. */
	if (ifn)
		logger(ifp->ctx, LOG_ERR,
		    "%s: IAID conflicts with one assigned to %s",
		    ifp->name, ifn->name);
}

static void
pre_start(struct interface *ifp)
{

	/* Add our link-local address before upping the interface
	 * so our RFC7217 address beats the hwaddr based one.
	 * This is also a safety check incase it was ripped out
	 * from under us. */
	if (ifp->options->options & DHCPCD_IPV6 && ipv6_start(ifp) == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: ipv6_start: %m", ifp->name);
		ifp->options->options &= ~DHCPCD_IPV6;
	}
}

void
dhcpcd_startinterface(void *arg)
{
	struct interface *ifp = arg;
	struct if_options *ifo = ifp->options;
	size_t i;
	char buf[DUID_LEN * 3];
	int carrier;
	struct timespec tv;

	if (ifo->options & DHCPCD_LINK) {
		switch (ifp->carrier) {
		case LINK_UP:
			break;
		case LINK_DOWN:
			logger(ifp->ctx, LOG_INFO, "%s: waiting for carrier",
			    ifp->name);
			return;
		case LINK_UNKNOWN:
			/* No media state available.
			 * Loop until both IFF_UP and IFF_RUNNING are set */
			if ((carrier = if_carrier(ifp)) == LINK_UNKNOWN) {
				tv.tv_sec = 0;
				tv.tv_nsec = IF_POLL_UP * NSEC_PER_MSEC;
				eloop_timeout_add_tv(ifp->ctx->eloop,
				    &tv, dhcpcd_startinterface, ifp);
			} else
				dhcpcd_handlecarrier(ifp->ctx, carrier,
				    ifp->flags, ifp->name);
			return;
		}
	}

	if (ifo->options & (DHCPCD_DUID | DHCPCD_IPV6)) {
		/* Report client DUID */
		if (ifp->ctx->duid == NULL) {
			if (duid_init(ifp) == 0)
				return;
			logger(ifp->ctx, LOG_INFO, "DUID %s",
			    hwaddr_ntoa(ifp->ctx->duid,
			    ifp->ctx->duid_len,
			    buf, sizeof(buf)));
		}
	}

	if (ifo->options & (DHCPCD_DUID | DHCPCD_IPV6)) {
		/* Report IAIDs */
		logger(ifp->ctx, LOG_INFO, "%s: IAID %s", ifp->name,
		    hwaddr_ntoa(ifo->iaid, sizeof(ifo->iaid),
		    buf, sizeof(buf)));
		warn_iaid_conflict(ifp, ifo->iaid);
		for (i = 0; i < ifo->ia_len; i++) {
			if (memcmp(ifo->iaid, ifo->ia[i].iaid,
			    sizeof(ifo->iaid)))
			{
				logger(ifp->ctx, LOG_INFO, "%s: IAID %s",
				    ifp->name, hwaddr_ntoa(ifo->ia[i].iaid,
				    sizeof(ifo->ia[i].iaid),
				    buf, sizeof(buf)));
				warn_iaid_conflict(ifp, ifo->ia[i].iaid);
			}
		}
	}

	if (ifo->options & DHCPCD_IPV6) {
		if (ifo->options & DHCPCD_IPV6RS &&
		    !(ifo->options & DHCPCD_INFORM))
			ipv6nd_startrs(ifp);

		if (ifo->options & DHCPCD_DHCP6)
			dhcp6_find_delegates(ifp);

		if (!(ifo->options & DHCPCD_IPV6RS) ||
		    ifo->options & DHCPCD_IA_FORCED)
		{
			ssize_t nolease;

			if (ifo->options & DHCPCD_IA_FORCED)
				nolease = dhcp6_start(ifp, DH6S_INIT);
			else {
				nolease = 0;
				/* Enabling the below doesn't really make
				 * sense as there is currently no standard
				 * to push routes via DHCPv6.
				 * (There is an expired working draft,
				 * maybe abandoned?)
				 * You can also get it to work by forcing
				 * an IA as shown above. */
#if 0
				/* With no RS or delegates we might
				 * as well try and solicit a DHCPv6 address */
				if (nolease == 0)
					nolease = dhcp6_start(ifp, DH6S_INIT);
#endif
			}
			if (nolease == -1)
			        logger(ifp->ctx, LOG_ERR,
				    "%s: dhcp6_start: %m", ifp->name);
		}
	}

	if (ifo->options & DHCPCD_IPV4) {
		/* Ensure we have an IPv4 state before starting DHCP */
		if (ipv4_getstate(ifp) != NULL)
			dhcp_start(ifp);
	}
}

static void
dhcpcd_prestartinterface(void *arg)
{
	struct interface *ifp = arg;

	pre_start(ifp);
	if ((!(ifp->ctx->options & DHCPCD_MASTER) ||
	    ifp->options->options & DHCPCD_IF_UP) &&
	    if_up(ifp) == -1)
		logger(ifp->ctx, LOG_ERR, "%s: if_up: %m", ifp->name);

	if (ifp->options->options & DHCPCD_LINK &&
	    ifp->carrier == LINK_UNKNOWN)
	{
		int carrier;

		if ((carrier = if_carrier(ifp)) != LINK_UNKNOWN) {
			dhcpcd_handlecarrier(ifp->ctx, carrier,
			    ifp->flags, ifp->name);
			return;
		}
		logger(ifp->ctx, LOG_INFO,
		    "%s: unknown carrier, waiting for interface flags",
		    ifp->name);
	}

	dhcpcd_startinterface(ifp);
}

static void
handle_link(void *arg)
{
	struct dhcpcd_ctx *ctx;

	ctx = arg;
	if (if_managelink(ctx) == -1) {
		logger(ctx, LOG_ERR, "if_managelink: %m");
		eloop_event_delete(ctx->eloop, ctx->link_fd);
		close(ctx->link_fd);
		ctx->link_fd = -1;
	}
}

static void
dhcpcd_initstate1(struct interface *ifp, int argc, char **argv,
    unsigned long long options)
{
	struct if_options *ifo;

	configure_interface(ifp, argc, argv, options);
	ifo = ifp->options;

	if (ifo->options & DHCPCD_IPV4 && ipv4_init(ifp->ctx) == -1) {
		logger(ifp->ctx, LOG_ERR, "ipv4_init: %m");
		ifo->options &= ~DHCPCD_IPV4;
	}
	if (ifo->options & DHCPCD_IPV6 && ipv6_init(ifp->ctx) == NULL) {
		logger(ifp->ctx, LOG_ERR, "ipv6_init: %m");
		ifo->options &= ~DHCPCD_IPV6RS;
	}

	/* Add our link-local address before upping the interface
	 * so our RFC7217 address beats the hwaddr based one.
	 * This needs to happen before PREINIT incase a hook script
	 * inadvertently ups the interface. */
	if (ifo->options & DHCPCD_IPV6 && ipv6_start(ifp) == -1) {
		logger(ifp->ctx, LOG_ERR, "%s: ipv6_start: %m", ifp->name);
		ifo->options &= ~DHCPCD_IPV6;
	}
}

void
dhcpcd_initstate(struct interface *ifp, unsigned long long options)
{

	dhcpcd_initstate1(ifp, ifp->ctx->argc, ifp->ctx->argv, options);
}

static void
run_preinit(struct interface *ifp)
{

	pre_start(ifp);
	if (ifp->ctx->options & DHCPCD_TEST)
		return;

	script_runreason(ifp, "PREINIT");

	if (ifp->options->options & DHCPCD_LINK && ifp->carrier != LINK_UNKNOWN)
		script_runreason(ifp,
		    ifp->carrier == LINK_UP ? "CARRIER" : "NOCARRIER");
}

int
dhcpcd_handleinterface(void *arg, int action, const char *ifname)
{
	struct dhcpcd_ctx *ctx;
	struct if_head *ifs;
	struct interface *ifp, *iff, *ifn;
	const char * const argv[] = { ifname };
	int i;

	ctx = arg;
	if (action == -1) {
		ifp = if_find(ctx->ifaces, ifname);
		if (ifp == NULL) {
			errno = ESRCH;
			return -1;
		}
		logger(ctx, LOG_DEBUG, "%s: interface departed", ifp->name);
		ifp->options->options |= DHCPCD_DEPARTED;
		stop_interface(ifp);
		return 0;
	}

	/* If running off an interface list, check it's in it. */
	if (ctx->ifc && action != 2) {
		for (i = 0; i < ctx->ifc; i++)
			if (strcmp(ctx->ifv[i], ifname) == 0)
				break;
		if (i >= ctx->ifc)
			return 0;
	}

	i = -1;
	ifs = if_discover(ctx, -1, UNCONST(argv));
	if (ifs == NULL) {
		logger(ctx, LOG_ERR, "%s: if_discover: %m", __func__);
		return -1;
	}
	TAILQ_FOREACH_SAFE(ifp, ifs, next, ifn) {
		if (strcmp(ifp->name, ifname) != 0)
			continue;
		i = 0;
		/* Check if we already have the interface */
		iff = if_find(ctx->ifaces, ifp->name);
		if (iff) {
			logger(ctx, LOG_DEBUG, "%s: interface updated", iff->name);
			/* The flags and hwaddr could have changed */
			iff->flags = ifp->flags;
			iff->hwlen = ifp->hwlen;
			if (ifp->hwlen != 0)
				memcpy(iff->hwaddr, ifp->hwaddr, iff->hwlen);
		} else {
			logger(ctx, LOG_DEBUG, "%s: interface added", ifp->name);
			TAILQ_REMOVE(ifs, ifp, next);
			TAILQ_INSERT_TAIL(ctx->ifaces, ifp, next);
			dhcpcd_initstate(ifp, 0);
			run_preinit(ifp);
			iff = ifp;
		}
		if (action > 0)
			dhcpcd_prestartinterface(iff);
	}

	/* Free our discovered list */
	while ((ifp = TAILQ_FIRST(ifs))) {
		TAILQ_REMOVE(ifs, ifp, next);
		if_free(ifp);
	}
	free(ifs);

	if (i == -1)
		errno = ENOENT;
	return i;
}

void
dhcpcd_handlehwaddr(struct dhcpcd_ctx *ctx, const char *ifname,
    const uint8_t *hwaddr, uint8_t hwlen)
{
	struct interface *ifp;
	char buf[sizeof(ifp->hwaddr) * 3];

	ifp = if_find(ctx->ifaces, ifname);
	if (ifp == NULL)
		return;

	if (hwlen > sizeof(ifp->hwaddr)) {
		errno = ENOBUFS;
		logger(ctx, LOG_ERR, "%s: %s: %m", ifp->name, __func__);
		return;
	}

	if (ifp->hwlen == hwlen && memcmp(ifp->hwaddr, hwaddr, hwlen) == 0)
		return;

	logger(ctx, LOG_INFO, "%s: new hardware address: %s", ifp->name,
	    hwaddr_ntoa(hwaddr, hwlen, buf, sizeof(buf)));
	ifp->hwlen = hwlen;
	memcpy(ifp->hwaddr, hwaddr, hwlen);
}

static void
if_reboot(struct interface *ifp, int argc, char **argv)
{
	unsigned long long oldopts;

	oldopts = ifp->options->options;
	script_runreason(ifp, "RECONFIGURE");
	dhcpcd_initstate1(ifp, argc, argv, 0);
	dhcp_reboot_newopts(ifp, oldopts);
	dhcp6_reboot(ifp);
	dhcpcd_prestartinterface(ifp);
}

static void
reload_config(struct dhcpcd_ctx *ctx)
{
	struct if_options *ifo;

	free_globals(ctx);
	ifo = read_config(ctx, NULL, NULL, NULL);
	add_options(ctx, NULL, ifo, ctx->argc, ctx->argv);
	/* We need to preserve these two options. */
	if (ctx->options & DHCPCD_MASTER)
		ifo->options |= DHCPCD_MASTER;
	if (ctx->options & DHCPCD_DAEMONISED)
		ifo->options |= DHCPCD_DAEMONISED;
	ctx->options = ifo->options;
	free_options(ifo);
}

static void
reconf_reboot(struct dhcpcd_ctx *ctx, int action, int argc, char **argv, int oi)
{
	struct if_head *ifs;
	struct interface *ifn, *ifp;

	ifs = if_discover(ctx, argc - oi, argv + oi);
	if (ifs == NULL) {
		logger(ctx, LOG_ERR, "%s: if_discover: %m", __func__);
		return;
	}

	while ((ifp = TAILQ_FIRST(ifs))) {
		TAILQ_REMOVE(ifs, ifp, next);
		ifn = if_find(ctx->ifaces, ifp->name);
		if (ifn) {
			if (action)
				if_reboot(ifn, argc, argv);
			else
				ipv4_applyaddr(ifn);
			if_free(ifp);
		} else {
			TAILQ_INSERT_TAIL(ctx->ifaces, ifp, next);
			dhcpcd_initstate1(ifp, argc, argv, 0);
			run_preinit(ifp);
			dhcpcd_prestartinterface(ifp);
		}
	}
	free(ifs);
}

static void
stop_all_interfaces(struct dhcpcd_ctx *ctx, int do_release)
{
	struct interface *ifp;

	/* Drop the last interface first */
	while ((ifp = TAILQ_LAST(ctx->ifaces, if_head)) != NULL) {
		if (do_release) {
			ifp->options->options |= DHCPCD_RELEASE;
			ifp->options->options &= ~DHCPCD_PERSISTENT;
		}
		ifp->options->options |= DHCPCD_EXITING;
		stop_interface(ifp);
	}
}

#ifdef USE_SIGNALS
#define sigmsg "received %s, %s"
static void
signal_cb(int sig, void *arg)
{
	struct dhcpcd_ctx *ctx = arg;
	struct interface *ifp;
	int do_release, exit_code;

	do_release = 0;
	exit_code = EXIT_FAILURE;
	switch (sig) {
	case SIGINT:
		logger(ctx, LOG_INFO, sigmsg, "SIGINT", "stopping");
		break;
	case SIGTERM:
		logger(ctx, LOG_INFO, sigmsg, "SIGTERM", "stopping");
		exit_code = EXIT_SUCCESS;
		break;
	case SIGALRM:
		logger(ctx, LOG_INFO, sigmsg, "SIGALRM", "releasing");
		do_release = 1;
		exit_code = EXIT_SUCCESS;
		break;
	case SIGHUP:
		logger(ctx, LOG_INFO, sigmsg, "SIGHUP", "rebinding");
		reload_config(ctx);
		/* Preserve any options passed on the commandline
		 * when we were started. */
		reconf_reboot(ctx, 1, ctx->argc, ctx->argv,
		    ctx->argc - ctx->ifc);
		return;
	case SIGUSR1:
		logger(ctx, LOG_INFO, sigmsg, "SIGUSR1", "reconfiguring");
		TAILQ_FOREACH(ifp, ctx->ifaces, next) {
			ipv4_applyaddr(ifp);
		}
		return;
	case SIGUSR2:
		logger_close(ctx);
		logger_open(ctx);
		logger(ctx, LOG_INFO, sigmsg, "SIGUSR2", "reopened logfile");
		return;
	case SIGPIPE:
		logger(ctx, LOG_WARNING, "received SIGPIPE");
		return;
	default:
		logger(ctx, LOG_ERR,
		    "received signal %d, "
		    "but don't know what to do with it",
		    sig);
		return;
	}

	if (!(ctx->options & DHCPCD_TEST))
		stop_all_interfaces(ctx, do_release);
	eloop_exit(ctx->eloop, exit_code);
}
#endif

static void
dhcpcd_getinterfaces(void *arg)
{
	struct fd_list *fd = arg;
	struct interface *ifp;
	size_t len;

	len = 0;
	TAILQ_FOREACH(ifp, fd->ctx->ifaces, next) {
		len++;
		if (D_STATE_RUNNING(ifp))
			len++;
		if (IPV4LL_STATE_RUNNING(ifp))
			len++;
		if (RS_STATE_RUNNING(ifp))
			len++;
		if (D6_STATE_RUNNING(ifp))
			len++;
	}
	if (write(fd->fd, &len, sizeof(len)) != sizeof(len))
		return;
	eloop_event_remove_writecb(fd->ctx->eloop, fd->fd);
	TAILQ_FOREACH(ifp, fd->ctx->ifaces, next) {
		if (send_interface(fd, ifp) == -1)
			logger(ifp->ctx, LOG_ERR,
			    "send_interface %d: %m", fd->fd);
	}
}

int
dhcpcd_handleargs(struct dhcpcd_ctx *ctx, struct fd_list *fd,
    int argc, char **argv)
{
	struct interface *ifp;
	int do_exit = 0, do_release = 0, do_reboot = 0;
	int opt, oi = 0;
	size_t len, l;
	char *tmp, *p;

	/* Special commands for our control socket
	 * as the other end should be blocking until it gets the
	 * expected reply we should be safely able just to change the
	 * write callback on the fd */
	if (strcmp(*argv, "--version") == 0) {
		return control_queue(fd, UNCONST(VERSION),
		    strlen(VERSION) + 1, 0);
	} else if (strcmp(*argv, "--getconfigfile") == 0) {
		return control_queue(fd, UNCONST(fd->ctx->cffile),
		    strlen(fd->ctx->cffile) + 1, 0);
	} else if (strcmp(*argv, "--getinterfaces") == 0) {
		eloop_event_add(fd->ctx->eloop, fd->fd, NULL, NULL,
		    dhcpcd_getinterfaces, fd);
		return 0;
	} else if (strcmp(*argv, "--listen") == 0) {
		fd->flags |= FD_LISTEN;
		return 0;
	}

	/* Only priviledged users can control dhcpcd via the socket. */
	if (fd->flags & FD_UNPRIV) {
		errno = EPERM;
		return -1;
	}

	/* Log the command */
	len = 1;
	for (opt = 0; opt < argc; opt++)
		len += strlen(argv[opt]) + 1;
	tmp = malloc(len);
	if (tmp == NULL)
		return -1;
	p = tmp;
	for (opt = 0; opt < argc; opt++) {
		l = strlen(argv[opt]);
		strlcpy(p, argv[opt], len);
		len -= l + 1;
		p += l;
		*p++ = ' ';
	}
	*--p = '\0';
	logger(ctx, LOG_INFO, "control command: %s", tmp);
	free(tmp);

	optind = 0;
	while ((opt = getopt_long(argc, argv, IF_OPTS, cf_options, &oi)) != -1)
	{
		switch (opt) {
		case 'g':
			/* Assumed if below not set */
			break;
		case 'k':
			do_release = 1;
			break;
		case 'n':
			do_reboot = 1;
			break;
		case 'x':
			do_exit = 1;
			break;
		}
	}

	if (do_release || do_exit) {
		if (optind == argc) {
			stop_all_interfaces(ctx, do_release);
			eloop_exit(ctx->eloop, EXIT_SUCCESS);
			return 0;
		}
		for (oi = optind; oi < argc; oi++) {
			if ((ifp = if_find(ctx->ifaces, argv[oi])) == NULL)
				continue;
			if (do_release) {
				ifp->options->options |= DHCPCD_RELEASE;
				ifp->options->options &= ~DHCPCD_PERSISTENT;
			}
			ifp->options->options |= DHCPCD_EXITING;
			stop_interface(ifp);
		}
		return 0;
	}

	reload_config(ctx);
	/* XXX: Respect initial commandline options? */
	reconf_reboot(ctx, do_reboot, argc, argv, optind - 1);
	return 0;
}

int
main(int argc, char **argv)
{
	struct dhcpcd_ctx ctx;
	struct if_options *ifo;
	struct interface *ifp;
	uint16_t family = 0;
	int opt, oi = 0, i;
	time_t t;
	ssize_t len;
#if defined(USE_SIGNALS) || !defined(THERE_IS_NO_FORK)
	pid_t pid;
#endif
#ifdef USE_SIGNALS
	int sig = 0;
	const char *siga = NULL;
#endif

	/* Test for --help and --version */
	if (argc > 1) {
		if (strcmp(argv[1], "--help") == 0) {
			usage();
			return EXIT_SUCCESS;
		} else if (strcmp(argv[1], "--version") == 0) {
			printf(""PACKAGE" "VERSION"\n%s\n", dhcpcd_copyright);
			return EXIT_SUCCESS;
		}
	}

	memset(&ctx, 0, sizeof(ctx));
	closefrom(3);

	ctx.log_fd = -1;
	logger_open(&ctx);
	logger_mask(&ctx, LOG_UPTO(LOG_INFO));

	ifo = NULL;
	ctx.cffile = CONFIG;
	ctx.pid_fd = ctx.control_fd = ctx.control_unpriv_fd = ctx.link_fd = -1;
	ctx.pf_inet_fd = -1;
#if defined(INET6) && defined(BSD)
	ctx.pf_inet6_fd = -1;
#endif
#ifdef IFLR_ACTIVE
	ctx.pf_link_fd = -1;
#endif

	TAILQ_INIT(&ctx.control_fds);
#ifdef PLUGIN_DEV
	ctx.dev_fd = -1;
#endif
#ifdef INET
	ctx.udp_fd = -1;
#endif
	i = 0;
	while ((opt = getopt_long(argc, argv, IF_OPTS, cf_options, &oi)) != -1)
	{
		switch (opt) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'f':
			ctx.cffile = optarg;
			break;
#ifdef USE_SIGNALS
		case 'g':
			sig = SIGUSR1;
			siga = "USR1";
			break;
		case 'j':
			ctx.logfile = strdup(optarg);
			logger_close(&ctx);
			logger_open(&ctx);
			break;
		case 'k':
			sig = SIGALRM;
			siga = "ARLM";
			break;
		case 'n':
			sig = SIGHUP;
			siga = "HUP";
			break;
		case 'x':
			sig = SIGTERM;
			siga = "TERM";;
			break;
#endif
		case 'T':
			i = 1;
			break;
		case 'U':
			i = 3;
			break;
		case 'V':
			i = 2;
			break;
		case '?':
			usage();
			goto exit_failure;
		}
	}

	ctx.argv = argv;
	ctx.argc = argc;
	ctx.ifc = argc - optind;
	ctx.ifv = argv + optind;

	ifo = read_config(&ctx, NULL, NULL, NULL);
	if (ifo == NULL)
		goto exit_failure;
	opt = add_options(&ctx, NULL, ifo, argc, argv);
	if (opt != 1) {
		if (opt == 0)
			usage();
		goto exit_failure;
	}
	if (i == 2) {
		printf("Interface options:\n");
		if (optind == argc - 1) {
			free_options(ifo);
			ifo = read_config(&ctx, argv[optind], NULL, NULL);
			if (ifo == NULL)
				goto exit_failure;
			add_options(&ctx, NULL, ifo, argc, argv);
		}
		if_printoptions();
#ifdef INET
		if (family == 0 || family == AF_INET) {
			printf("\nDHCPv4 options:\n");
			dhcp_printoptions(&ctx,
			    ifo->dhcp_override, ifo->dhcp_override_len);
		}
#endif
#ifdef INET6
		if (family == 0 || family == AF_INET6) {
			printf("\nND options:\n");
			ipv6nd_printoptions(&ctx,
			    ifo->nd_override, ifo->nd_override_len);
			printf("\nDHCPv6 options:\n");
			dhcp6_printoptions(&ctx,
			    ifo->dhcp6_override, ifo->dhcp6_override_len);
		}
#endif
		goto exit_success;
	}
	ctx.options = ifo->options;
	if (i != 0) {
		if (i == 1)
			ctx.options |= DHCPCD_TEST;
		else
			ctx.options |= DHCPCD_DUMPLEASE;
		ctx.options |= DHCPCD_PERSISTENT;
		ctx.options &= ~DHCPCD_DAEMONISE;
	}

#ifdef THERE_IS_NO_FORK
	ctx.options &= ~DHCPCD_DAEMONISE;
#endif

	if (ctx.options & DHCPCD_DEBUG)
		logger_mask(&ctx, LOG_UPTO(LOG_DEBUG));
	if (ctx.options & DHCPCD_QUIET) {
		i = open(_PATH_DEVNULL, O_RDWR);
		if (i == -1)
			logger(&ctx, LOG_ERR, "%s: open: %m", __func__);
		else {
			dup2(i, STDERR_FILENO);
			close(i);
		}
	}

	if (!(ctx.options & (DHCPCD_TEST | DHCPCD_DUMPLEASE))) {
		/* If we have any other args, we should run as a single dhcpcd
		 *  instance for that interface. */
		if (optind == argc - 1 && !(ctx.options & DHCPCD_MASTER)) {
			const char *per;

			if (strlen(argv[optind]) > IF_NAMESIZE) {
				logger(&ctx, LOG_ERR,
				    "%s: interface name too long",
				    argv[optind]);
				goto exit_failure;
			}
			/* Allow a dhcpcd interface per address family */
			switch(family) {
			case AF_INET:
				per = "-4";
				break;
			case AF_INET6:
				per = "-6";
				break;
			default:
				per = "";
			}
			snprintf(ctx.pidfile, sizeof(ctx.pidfile),
			    PIDFILE, "-", argv[optind], per);
		} else {
			snprintf(ctx.pidfile, sizeof(ctx.pidfile),
			    PIDFILE, "", "", "");
			ctx.options |= DHCPCD_MASTER;
		}
	}

	if (chdir("/") == -1)
		logger(&ctx, LOG_ERR, "chdir `/': %m");

	/* Freeing allocated addresses from dumping leases can trigger
	 * eloop removals as well, so init here. */
	if ((ctx.eloop = eloop_new()) == NULL) {
		logger(&ctx, LOG_ERR, "%s: eloop_init: %m", __func__);
		goto exit_failure;
	}

	if (ctx.options & DHCPCD_DUMPLEASE) {
		if (optind != argc - 1) {
			logger(&ctx, LOG_ERR,
			    "dumplease requires an interface");
			goto exit_failure;
		}
		i = 0;
		/* We need to try and find the interface so we can
		 * load the hardware address to compare automated IAID */
		ctx.ifaces = if_discover(&ctx, 1, argv + optind);
		if (ctx.ifaces == NULL) {
			logger(&ctx, LOG_ERR, "if_discover: %m");
			goto exit_failure;
		}
		ifp = TAILQ_FIRST(ctx.ifaces);
		if (ifp == NULL) {
			ifp = calloc(1, sizeof(*ifp));
			if (ifp == NULL) {
				logger(&ctx, LOG_ERR, "%s: %m", __func__);
				goto exit_failure;
			}
			strlcpy(ctx.pidfile, argv[optind], sizeof(ctx.pidfile));
			ifp->ctx = &ctx;
			TAILQ_INSERT_HEAD(ctx.ifaces, ifp, next);
			if (family == 0) {
				if (ctx.pidfile[strlen(ctx.pidfile) - 1] == '6')
					family = AF_INET6;
				else
					family = AF_INET;
			}
		}
		configure_interface(ifp, ctx.argc, ctx.argv, 0);
		if (family == 0 || family == AF_INET) {
			if (dhcp_dump(ifp) == -1)
				i = 1;
		}
		if (family == 0 || family == AF_INET6) {
			if (dhcp6_dump(ifp) == -1)
				i = 1;
		}
		if (i == -1)
			goto exit_failure;
		goto exit_success;
	}

#ifdef USE_SIGNALS
	if (!(ctx.options & DHCPCD_TEST) &&
	    (sig == 0 || ctx.ifc != 0))
	{
#endif
		if (ctx.options & DHCPCD_MASTER)
			i = -1;
		else
			i = control_open(&ctx, argv[optind]);
		if (i == -1)
			i = control_open(&ctx, NULL);
		if (i != -1) {
			logger(&ctx, LOG_INFO,
			    "sending commands to master dhcpcd process");
			len = control_send(&ctx, argc, argv);
			control_close(&ctx);
			if (len > 0) {
				logger(&ctx, LOG_DEBUG, "send OK");
				goto exit_success;
			} else {
				logger(&ctx, LOG_ERR,
				    "failed to send commands");
				goto exit_failure;
			}
		} else {
			if (errno != ENOENT)
				logger(&ctx, LOG_ERR, "control_open: %m");
		}
#ifdef USE_SIGNALS
	}
#endif

#ifdef USE_SIGNALS
	if (sig != 0) {
		pid = read_pid(ctx.pidfile);
		if (pid != 0)
			logger(&ctx, LOG_INFO, "sending signal %s to pid %d",
			    siga, pid);
		if (pid == 0 || kill(pid, sig) != 0) {
			if (sig != SIGHUP && errno != EPERM)
				logger(&ctx, LOG_ERR, ""PACKAGE" not running");
			if (pid != 0 && errno != ESRCH) {
				logger(&ctx, LOG_ERR, "kill: %m");
				goto exit_failure;
			}
			unlink(ctx.pidfile);
			if (sig != SIGHUP)
				goto exit_failure;
		} else {
			struct timespec ts;

			if (sig == SIGHUP || sig == SIGUSR1)
				goto exit_success;
			/* Spin until it exits */
			logger(&ctx, LOG_INFO,
			    "waiting for pid %d to exit", pid);
			ts.tv_sec = 0;
			ts.tv_nsec = 100000000; /* 10th of a second */
			for(i = 0; i < 100; i++) {
				nanosleep(&ts, NULL);
				if (read_pid(ctx.pidfile) == 0)
					goto exit_success;
			}
			logger(&ctx, LOG_ERR, "pid %d failed to exit", pid);
			goto exit_failure;
		}
	}

	if (!(ctx.options & DHCPCD_TEST)) {
		if ((pid = read_pid(ctx.pidfile)) > 0 &&
		    kill(pid, 0) == 0)
		{
			logger(&ctx, LOG_ERR, ""PACKAGE
			    " already running on pid %d (%s)",
			    pid, ctx.pidfile);
			goto exit_failure;
		}

		/* Ensure we have the needed directories */
		if (mkdir(RUNDIR, 0755) == -1 && errno != EEXIST)
			logger(&ctx, LOG_ERR, "mkdir `%s': %m", RUNDIR);
		if (mkdir(DBDIR, 0755) == -1 && errno != EEXIST)
			logger(&ctx, LOG_ERR, "mkdir `%s': %m", DBDIR);

		opt = O_WRONLY | O_CREAT | O_NONBLOCK;
#ifdef O_CLOEXEC
		opt |= O_CLOEXEC;
#endif
		ctx.pid_fd = open(ctx.pidfile, opt, 0664);
		if (ctx.pid_fd == -1)
			logger(&ctx, LOG_ERR, "open `%s': %m", ctx.pidfile);
		else {
#ifdef LOCK_EX
			/* Lock the file so that only one instance of dhcpcd
			 * runs on an interface */
			if (flock(ctx.pid_fd, LOCK_EX | LOCK_NB) == -1) {
				logger(&ctx, LOG_ERR, "flock `%s': %m", ctx.pidfile);
				close(ctx.pid_fd);
				ctx.pid_fd = -1;
				goto exit_failure;
			}
#endif
#ifndef O_CLOEXEC
			if (fcntl(ctx.pid_fd, F_GETFD, &opt) == -1 ||
			    fcntl(ctx.pid_fd, F_SETFD, opt | FD_CLOEXEC) == -1)
			{
				logger(&ctx, LOG_ERR, "fcntl: %m");
				close(ctx.pid_fd);
				ctx.pid_fd = -1;
				goto exit_failure;
			}
#endif
			write_pid(ctx.pid_fd, getpid());
		}
	}

	if (ctx.options & DHCPCD_MASTER) {
		if (control_start(&ctx, NULL) == -1)
			logger(&ctx, LOG_ERR, "control_start: %m");
	}
#else
	if (control_start(&ctx,
	    ctx.options & DHCPCD_MASTER ? NULL : argv[optind]) == -1)
	{
		logger(&ctx, LOG_ERR, "control_start: %m");
		goto exit_failure;
	}
#endif

	logger(&ctx, LOG_DEBUG, PACKAGE "-" VERSION " starting");
	ctx.options |= DHCPCD_STARTED;
#ifdef USE_SIGNALS
	if (eloop_signal_set_cb(ctx.eloop,
	    dhcpcd_signals, dhcpcd_signals_len,
	    signal_cb, &ctx) == -1)
	{
		logger(&ctx, LOG_ERR, "eloop_signal_mask: %m");
		goto exit_failure;
	}
	/* Save signal mask, block and redirect signals to our handler */
	if (eloop_signal_mask(ctx.eloop, &ctx.sigset) == -1) {
		logger(&ctx, LOG_ERR, "eloop_signal_mask: %m");
		goto exit_failure;
	}
#endif

	/* When running dhcpcd against a single interface, we need to retain
	 * the old behaviour of waiting for an IP address */
	if (ctx.ifc == 1 && !(ctx.options & DHCPCD_BACKGROUND))
		ctx.options |= DHCPCD_WAITIP;

	/* Open our persistent sockets. */
	if (if_opensockets(&ctx) == -1) {
		logger(&ctx, LOG_ERR, "if_opensockets: %m");
		goto exit_failure;
	}
	eloop_event_add(ctx.eloop, ctx.link_fd, handle_link, &ctx, NULL, NULL);

	/* Start any dev listening plugin which may want to
	 * change the interface name provided by the kernel */
	if ((ctx.options & (DHCPCD_MASTER | DHCPCD_DEV)) ==
	    (DHCPCD_MASTER | DHCPCD_DEV))
		dev_start(&ctx);

	ctx.ifaces = if_discover(&ctx, ctx.ifc, ctx.ifv);
	if (ctx.ifaces == NULL) {
		logger(&ctx, LOG_ERR, "if_discover: %m");
		goto exit_failure;
	}
	for (i = 0; i < ctx.ifc; i++) {
		if (if_find(ctx.ifaces, ctx.ifv[i]) == NULL)
			logger(&ctx, LOG_ERR,
			    "%s: interface not found or invalid",
			    ctx.ifv[i]);
	}
	if (TAILQ_FIRST(ctx.ifaces) == NULL) {
		if (ctx.ifc == 0)
			logger(&ctx, LOG_ERR, "no valid interfaces found");
		else
			goto exit_failure;
		if (!(ctx.options & DHCPCD_LINK)) {
			logger(&ctx, LOG_ERR,
			    "aborting as link detection is disabled");
			goto exit_failure;
		}
	}

	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		dhcpcd_initstate1(ifp, argc, argv, 0);
	}

	if (ctx.options & DHCPCD_BACKGROUND && dhcpcd_daemonise(&ctx))
		goto exit_success;

	opt = 0;
	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		run_preinit(ifp);
		if (ifp->carrier != LINK_DOWN)
			opt = 1;
	}

	if (!(ctx.options & DHCPCD_BACKGROUND)) {
		if (ctx.options & DHCPCD_MASTER)
			t = ifo->timeout;
		else if ((ifp = TAILQ_FIRST(ctx.ifaces)))
			t = ifp->options->timeout;
		else
			t = 0;
		if (opt == 0 &&
		    ctx.options & DHCPCD_LINK &&
		    !(ctx.options & DHCPCD_WAITIP))
		{
			logger(&ctx, LOG_WARNING,
			    "no interfaces have a carrier");
			if (dhcpcd_daemonise(&ctx))
				goto exit_success;
		} else if (t > 0 &&
		    /* Test mode removes the daemonise bit, so check for both */
		    ctx.options & (DHCPCD_DAEMONISE | DHCPCD_TEST))
		{
			eloop_timeout_add_sec(ctx.eloop, t,
			    handle_exit_timeout, &ctx);
		}
	}
	free_options(ifo);
	ifo = NULL;

	if_sortinterfaces(&ctx);
	TAILQ_FOREACH(ifp, ctx.ifaces, next) {
		eloop_timeout_add_sec(ctx.eloop, 0,
		    dhcpcd_prestartinterface, ifp);
	}

	i = eloop_start(ctx.eloop, &ctx.sigset);
	if (i < 0) {
		syslog(LOG_ERR, "eloop_start: %m");
		goto exit_failure;
	}
	goto exit1;

exit_success:
	i = EXIT_SUCCESS;
	goto exit1;

exit_failure:
	i = EXIT_FAILURE;

exit1:
	/* Free memory and close fd's */
	if (ctx.ifaces) {
		while ((ifp = TAILQ_FIRST(ctx.ifaces))) {
			TAILQ_REMOVE(ctx.ifaces, ifp, next);
			if_free(ifp);
		}
		free(ctx.ifaces);
	}
	free(ctx.duid);
	if (ctx.link_fd != -1) {
		eloop_event_delete(ctx.eloop, ctx.link_fd);
		close(ctx.link_fd);
	}
	if (ctx.pf_inet_fd != -1)
		close(ctx.pf_inet_fd);
#if defined(INET6) && defined(BSD)
	if (ctx.pf_inet6_fd != -1)
		close(ctx.pf_inet6_fd);
#endif
#ifdef IFLR_ACTIVE
	if (ctx.pf_link_fd != -1)
		close(ctx.pf_link_fd);
#endif


	free_options(ifo);
	free_globals(&ctx);
	ipv4_ctxfree(&ctx);
	ipv6_ctxfree(&ctx);
	dev_stop(&ctx);
	if (control_stop(&ctx) == -1)
		logger(&ctx, LOG_ERR, "control_stop: %m:");
	if (ctx.pid_fd != -1) {
		close(ctx.pid_fd);
		unlink(ctx.pidfile);
	}
	eloop_free(ctx.eloop);

	if (ctx.options & DHCPCD_STARTED && !(ctx.options & DHCPCD_FORKED))
		logger(&ctx, LOG_INFO, PACKAGE " exited");
	logger_close(&ctx);
	free(ctx.logfile);
	return i;
}

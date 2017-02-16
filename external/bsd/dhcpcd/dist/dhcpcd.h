/* $NetBSD: dhcpcd.h,v 1.13 2015/08/21 10:39:00 roy Exp $ */

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

#ifndef DHCPCD_H
#define DHCPCD_H

#include <sys/socket.h>
#include <net/if.h>

#include "config.h"
#ifdef HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#include "defs.h"
#include "control.h"
#include "if-options.h"

#define HWADDR_LEN	20
#define IF_SSIDSIZE	33
#define PROFILE_LEN	64
#define SECRET_LEN	64

#define LINK_UP		1
#define LINK_UNKNOWN	0
#define LINK_DOWN	-1

#define IF_DATA_IPV4	0
#define IF_DATA_ARP	1
#define IF_DATA_IPV4LL	2
#define IF_DATA_DHCP	3
#define IF_DATA_IPV6	4
#define IF_DATA_IPV6ND	5
#define IF_DATA_DHCP6	6
#define IF_DATA_MAX	7

/* If the interface does not support carrier status (ie PPP),
 * dhcpcd can poll it for the relevant flags periodically */
#define IF_POLL_UP	100	/* milliseconds */

#ifdef __QNX__
/* QNX carries defines for, but does not actually support PF_LINK */
#undef IFLR_ACTIVE
#endif

struct interface {
	struct dhcpcd_ctx *ctx;
	TAILQ_ENTRY(interface) next;
	char name[IF_NAMESIZE];
#ifdef __linux__
	char alias[IF_NAMESIZE];
#endif
	unsigned int index;
	unsigned int flags;
	sa_family_t family;
	unsigned char hwaddr[HWADDR_LEN];
	uint8_t hwlen;
	unsigned int metric;
	int carrier;
	int wireless;
	uint8_t ssid[IF_SSIDSIZE];
	unsigned int ssid_len;

	char profile[PROFILE_LEN];
	struct if_options *options;
	void *if_data[IF_DATA_MAX];
};
TAILQ_HEAD(if_head, interface);

struct dhcpcd_ctx {
	int pid_fd;
	char pidfile[sizeof(PIDFILE) + IF_NAMESIZE + 1];
	const char *cffile;
	unsigned long long options;
	char *logfile;
	int log_fd;
	int argc;
	char **argv;
	int ifac;	/* allowed interfaces */
	char **ifav;	/* allowed interfaces */
	int ifdc;	/* denied interfaces */
	char **ifdv;	/* denied interfaces */
	int ifc;	/* listed interfaces */
	char **ifv;	/* listed interfaces */
	int ifcc;	/* configured interfaces */
	char **ifcv;	/* configured interfaces */
	unsigned char *duid;
	size_t duid_len;
	struct if_head *ifaces;

	int pf_inet_fd;
#if defined(INET6) && defined(BSD)
	int pf_inet6_fd;
#endif
#ifdef IFLR_ACTIVE
	int pf_link_fd;
#endif
	int link_fd;

#ifdef USE_SIGNALS
	sigset_t sigset;
#endif
	struct eloop *eloop;

	int control_fd;
	int control_unpriv_fd;
	struct fd_list_head control_fds;
	char control_sock[sizeof(CONTROLSOCKET) + IF_NAMESIZE];
	gid_t control_group;

	/* DHCP Enterprise options, RFC3925 */
	struct dhcp_opt *vivso;
	size_t vivso_len;

	char *randomstate; /* original state */

#ifdef INET
	struct dhcp_opt *dhcp_opts;
	size_t dhcp_opts_len;
	struct rt_head *ipv4_routes;
	struct rt_head *ipv4_kroutes;

	int udp_fd;
	uint8_t *packet;

	/* Our aggregate option buffer.
	 * We ONLY use this when options are split, which for most purposes is
	 * practically never. See RFC3396 for details. */
	uint8_t *opt_buffer;
#endif
#ifdef INET6
	unsigned char secret[SECRET_LEN];
	size_t secret_len;

	struct dhcp_opt *nd_opts;
	size_t nd_opts_len;
	struct dhcp_opt *dhcp6_opts;
	size_t dhcp6_opts_len;
	struct ipv6_ctx *ipv6;
#ifndef __linux__
	int ra_global;
#endif
#endif /* INET6 */

#ifdef PLUGIN_DEV
	char *dev_load;
	int dev_fd;
	struct dev *dev;
	void *dev_handle;
#endif
};

#ifdef USE_SIGNALS
extern const int dhcpcd_signals[];
extern const size_t dhcpcd_signals_len;
#endif

int dhcpcd_ifafwaiting(const struct interface *);
int dhcpcd_afwaiting(const struct dhcpcd_ctx *);
pid_t dhcpcd_daemonise(struct dhcpcd_ctx *);

int dhcpcd_handleargs(struct dhcpcd_ctx *, struct fd_list *, int, char **);
void dhcpcd_handlecarrier(struct dhcpcd_ctx *, int, unsigned int, const char *);
int dhcpcd_handleinterface(void *, int, const char *);
void dhcpcd_handlehwaddr(struct dhcpcd_ctx *, const char *,
    const unsigned char *, uint8_t);
void dhcpcd_dropinterface(struct interface *, const char *);
int dhcpcd_selectprofile(struct interface *, const char *);

void dhcpcd_startinterface(void *);
void dhcpcd_initstate(struct interface *, unsigned long long);

#endif

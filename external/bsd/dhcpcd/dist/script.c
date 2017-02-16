#include <sys/cdefs.h>
 __RCSID("$NetBSD: script.c,v 1.23 2015/09/04 12:25:01 roy Exp $");

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

#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
/* We can't include spawn.h here because it may not exist.
 * config.h will pull it in, or our compat one. */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "if.h"
#include "if-options.h"
#include "ipv4ll.h"
#include "ipv6nd.h"
#include "script.h"

#ifdef HAVE_SPAWN_H
#include <spawn.h>
#else
#include "compat/posix_spawn.h"
#endif

/* Allow the OS to define another script env var name */
#ifndef RC_SVCNAME
#define RC_SVCNAME "RC_SVCNAME"
#endif

#define DEFAULT_PATH	"PATH=/usr/bin:/usr/sbin:/bin:/sbin"

static const char * const if_params[] = {
	"interface",
	"reason",
	"pid",
	"ifcarrier",
	"ifmetric",
	"ifwireless",
	"ifflags",
	"ssid",
	"profile",
	"interface_order",
	NULL
};

void
if_printoptions(void)
{
	const char * const *p;

	for (p = if_params; *p; p++)
		printf(" -  %s\n", *p);
}

static int
exec_script(const struct dhcpcd_ctx *ctx, char *const *argv, char *const *env)
{
	pid_t pid;
	posix_spawnattr_t attr;
	int r;
#ifdef USE_SIGNALS
	size_t i;
	short flags;
	sigset_t defsigs;
#else
	UNUSED(ctx);
#endif

	/* posix_spawn is a safe way of executing another image
	 * and changing signals back to how they should be. */
	if (posix_spawnattr_init(&attr) == -1)
		return -1;
#ifdef USE_SIGNALS
	flags = POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
	posix_spawnattr_setflags(&attr, flags);
	sigemptyset(&defsigs);
	for (i = 0; i < dhcpcd_signals_len; i++)
		sigaddset(&defsigs, dhcpcd_signals[i]);
	posix_spawnattr_setsigdefault(&attr, &defsigs);
	posix_spawnattr_setsigmask(&attr, &ctx->sigset);
#endif
	errno = 0;
	r = posix_spawn(&pid, argv[0], NULL, &attr, argv, env);
	if (r) {
		errno = r;
		return -1;
	}
	return pid;
}

#ifdef INET
static char *
make_var(struct dhcpcd_ctx *ctx, const char *prefix, const char *var)
{
	size_t len;
	char *v;

	len = strlen(prefix) + strlen(var) + 2;
	if ((v = malloc(len)) == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return NULL;
	}
	snprintf(v, len, "%s_%s", prefix, var);
	return v;
}


static int
append_config(struct dhcpcd_ctx *ctx, char ***env, size_t *len,
    const char *prefix, const char *const *config)
{
	size_t i, j, e1;
	char **ne, *eq, **nep, *p;
	int ret;

	if (config == NULL)
		return 0;

	ne = *env;
	ret = 0;
	for (i = 0; config[i] != NULL; i++) {
		eq = strchr(config[i], '=');
		e1 = (size_t)(eq - config[i] + 1);
		for (j = 0; j < *len; j++) {
			if (strncmp(ne[j], prefix, strlen(prefix)) == 0 &&
			    ne[j][strlen(prefix)] == '_' &&
			    strncmp(ne[j] + strlen(prefix) + 1,
			    config[i], e1) == 0)
			{
				p = make_var(ctx, prefix, config[i]);
				if (p == NULL) {
					ret = -1;
					break;
				}
				free(ne[j]);
				ne[j] = p;
				break;
			}
		}
		if (j == *len) {
			j++;
			p = make_var(ctx, prefix, config[i]);
			if (p == NULL) {
				ret = -1;
				break;
			}
			nep = realloc(ne, sizeof(char *) * (j + 1));
			if (nep == NULL) {
				logger(ctx, LOG_ERR, "%s: %m", __func__);
				free(p);
				ret = -1;
				break;
			}
			ne = nep;
			ne[j - 1] = p;
			*len = j;
		}
	}
	*env = ne;
	return ret;
}
#endif

static ssize_t
arraytostr(const char *const *argv, char **s)
{
	const char *const *ap;
	char *p;
	size_t len, l;

	if (*argv == NULL)
		return 0;
	len = 0;
	ap = argv;
	while (*ap)
		len += strlen(*ap++) + 1;
	*s = p = malloc(len);
	if (p == NULL)
		return -1;
	ap = argv;
	while (*ap) {
		l = strlen(*ap) + 1;
		memcpy(p, *ap, l);
		p += l;
		ap++;
	}
	return (ssize_t)len;
}

static ssize_t
make_env(const struct interface *ifp, const char *reason, char ***argv)
{
	char **env, **nenv, *p;
	size_t e, elen, l;
#if defined(INET) || defined(INET6)
	ssize_t n;
#endif
	const struct if_options *ifo = ifp->options;
	const struct interface *ifp2;
	int af;
#ifdef INET
	int dhcp, ipv4ll;
	const struct dhcp_state *state;
	const struct ipv4ll_state *istate;
#endif
#ifdef INET6
	const struct dhcp6_state *d6_state;
	int dhcp6, ra;
#endif

#ifdef INET
	dhcp = ipv4ll = 0;
	state = D_STATE(ifp);
	istate = IPV4LL_CSTATE(ifp);
#endif
#ifdef INET6
	dhcp6 = ra = 0;
	d6_state = D6_CSTATE(ifp);
#endif
	if (strcmp(reason, "TEST") == 0) {
		if (1 == 2) {}
#ifdef INET6
		else if (d6_state && d6_state->new)
			dhcp6 = 1;
		else if (ipv6nd_hasra(ifp))
			ra = 1;
#endif
#ifdef INET
		else if (state->added)
			dhcp = 1;
		else
			ipv4ll = 1;
#endif
	}
#ifdef INET6
	else if (reason[strlen(reason) - 1] == '6')
		dhcp6 = 1;
	else if (strcmp(reason, "ROUTERADVERT") == 0)
		ra = 1;
#endif
	else if (strcmp(reason, "PREINIT") == 0 ||
	    strcmp(reason, "CARRIER") == 0 ||
	    strcmp(reason, "NOCARRIER") == 0 ||
	    strcmp(reason, "UNKNOWN") == 0 ||
	    strcmp(reason, "DEPARTED") == 0 ||
	    strcmp(reason, "STOPPED") == 0)
	{
		/* This space left intentionally blank */
	}
#ifdef INET
	else if (strcmp(reason, "IPV4LL") == 0)
		ipv4ll = 1;
	else
		dhcp = 1;
#endif

	/* When dumping the lease, we only want to report interface and
	   reason - the other interface variables are meaningless */
	if (ifp->ctx->options & DHCPCD_DUMPLEASE)
		elen = 2;
	else
		elen = 11;

#define EMALLOC(i, l) if ((env[(i)] = malloc((l))) == NULL) goto eexit;
	/* Make our env + space for profile, wireless and debug */
	env = calloc(1, sizeof(char *) * (elen + 4 + 1));
	if (env == NULL)
		goto eexit;
	e = strlen("interface") + strlen(ifp->name) + 2;
	EMALLOC(0, e);
	snprintf(env[0], e, "interface=%s", ifp->name);
	e = strlen("reason") + strlen(reason) + 2;
	EMALLOC(1, e);
	snprintf(env[1], e, "reason=%s", reason);
	if (ifp->ctx->options & DHCPCD_DUMPLEASE)
		goto dumplease;
	e = 20;
	EMALLOC(2, e);
	snprintf(env[2], e, "pid=%d", getpid());
	EMALLOC(3, e);
	snprintf(env[3], e, "ifcarrier=%s",
	    ifp->carrier == LINK_UNKNOWN ? "unknown" :
	    ifp->carrier == LINK_UP ? "up" : "down");
	EMALLOC(4, e);
	snprintf(env[4], e, "ifmetric=%d", ifp->metric);
	EMALLOC(5, e);
	snprintf(env[5], e, "ifwireless=%d", ifp->wireless);
	EMALLOC(6, e);
	snprintf(env[6], e, "ifflags=%u", ifp->flags);
	EMALLOC(7, e);
	snprintf(env[7], e, "ifmtu=%d", if_getmtu(ifp));
	l = e = strlen("interface_order=");
	TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
		e += strlen(ifp2->name) + 1;
	}
	EMALLOC(8, e);
	p = env[8];
	strlcpy(p, "interface_order=", e);
	e -= l;
	p += l;
	TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
		l = strlcpy(p, ifp2->name, e);
		p += l;
		e -= l;
		*p++ = ' ';
		e--;
	}
	*--p = '\0';
	if (strcmp(reason, "STOPPED") == 0) {
		env[9] = strdup("if_up=false");
		if (ifo->options & DHCPCD_RELEASE)
			env[10] = strdup("if_down=true");
		else
			env[10] = strdup("if_down=false");
	} else if (strcmp(reason, "TEST") == 0 ||
	    strcmp(reason, "PREINIT") == 0 ||
	    strcmp(reason, "CARRIER") == 0 ||
	    strcmp(reason, "UNKNOWN") == 0)
	{
		env[9] = strdup("if_up=false");
		env[10] = strdup("if_down=false");
	} else if (1 == 2 /* appease ifdefs */
#ifdef INET
	    || (dhcp && state && state->new)
	    || (ipv4ll && IPV4LL_STATE_RUNNING(ifp))
#endif
#ifdef INET6
	    || (dhcp6 && d6_state && d6_state->new)
	    || (ra && ipv6nd_hasra(ifp))
#endif
	    )
	{
		env[9] = strdup("if_up=true");
		env[10] = strdup("if_down=false");
	} else {
		env[9] = strdup("if_up=false");
		env[10] = strdup("if_down=true");
	}
	if (env[9] == NULL || env[10] == NULL)
		goto eexit;
	if ((af = dhcpcd_ifafwaiting(ifp)) != AF_MAX) {
		e = 20;
		EMALLOC(elen, e);
		snprintf(env[elen++], e, "if_afwaiting=%d", af);
	}
	if ((af = dhcpcd_afwaiting(ifp->ctx)) != AF_MAX) {
		TAILQ_FOREACH(ifp2, ifp->ctx->ifaces, next) {
			if ((af = dhcpcd_ifafwaiting(ifp2)) != AF_MAX)
				break;
		}
	}
	if (af != AF_MAX) {
		e = 20;
		EMALLOC(elen, e);
		snprintf(env[elen++], e, "af_waiting=%d", af);
	}
	if (ifo->options & DHCPCD_DEBUG) {
		e = strlen("syslog_debug=true") + 1;
		EMALLOC(elen, e);
		snprintf(env[elen++], e, "syslog_debug=true");
	}
	if (*ifp->profile) {
		e = strlen("profile=") + strlen(ifp->profile) + 1;
		EMALLOC(elen, e);
		snprintf(env[elen++], e, "profile=%s", ifp->profile);
	}
	if (ifp->wireless) {
		static const char *pfx = "ifssid=";
		size_t pfx_len;
		ssize_t psl;

		pfx_len = strlen(pfx);
		psl = print_string(NULL, 0, ESCSTRING,
		    (const uint8_t *)ifp->ssid, ifp->ssid_len);
		if (psl != -1) {
			EMALLOC(elen, pfx_len + (size_t)psl + 1);
			memcpy(env[elen], pfx, pfx_len);
			print_string(env[elen] + pfx_len, (size_t)psl + 1,
			    ESCSTRING,
			    (const uint8_t *)ifp->ssid, ifp->ssid_len);
			elen++;
		}
	}
#ifdef INET
	if (dhcp && state && state->old) {
		n = dhcp_env(NULL, NULL, state->old, ifp);
		if (n == -1)
			goto eexit;
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			n = dhcp_env(env + elen, "old", state->old, ifp);
			if (n == -1)
				goto eexit;
			elen += (size_t)n;
		}
		if (append_config(ifp->ctx, &env, &elen, "old",
		    (const char *const *)ifo->config) == -1)
			goto eexit;
	}
#endif
#ifdef INET6
	if (dhcp6 && d6_state && d6_state->old) {
		n = dhcp6_env(NULL, NULL, ifp,
		    d6_state->old, d6_state->old_len);
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			n = dhcp6_env(env + elen, "old", ifp,
			    d6_state->old, d6_state->old_len);
			if (n == -1)
				goto eexit;
			elen += (size_t)n;
		}
	}
#endif

dumplease:
#ifdef INET
	if (ipv4ll) {
		n = ipv4ll_env(NULL, NULL, ifp);
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			if ((n = ipv4ll_env(env + elen,
			    istate->down ? "old" : "new", ifp)) == -1)
				goto eexit;
			elen += (size_t)n;
		}
	}
	if (dhcp && state && state->new) {
		n = dhcp_env(NULL, NULL, state->new, ifp);
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			n = dhcp_env(env + elen, "new",
			    state->new, ifp);
			if (n == -1)
				goto eexit;
			elen += (size_t)n;
		}
		if (append_config(ifp->ctx, &env, &elen, "new",
		    (const char *const *)ifo->config) == -1)
			goto eexit;
	}
#endif
#ifdef INET6
	if (dhcp6 && D6_STATE_RUNNING(ifp)) {
		n = dhcp6_env(NULL, NULL, ifp,
		    d6_state->new, d6_state->new_len);
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			n = dhcp6_env(env + elen, "new", ifp,
			    d6_state->new, d6_state->new_len);
			if (n == -1)
				goto eexit;
			elen += (size_t)n;
		}
	}
	if (ra) {
		n = ipv6nd_env(NULL, NULL, ifp);
		if (n > 0) {
			nenv = realloc(env, sizeof(char *) *
			    (elen + (size_t)n + 1));
			if (nenv == NULL)
				goto eexit;
			env = nenv;
			n = ipv6nd_env(env + elen, NULL, ifp);
			if (n == -1)
				goto eexit;
			elen += (size_t)n;
		}
	}
#endif

	/* Add our base environment */
	if (ifo->environ) {
		e = 0;
		while (ifo->environ[e++])
			;
		nenv = realloc(env, sizeof(char *) * (elen + e + 1));
		if (nenv == NULL)
			goto eexit;
		env = nenv;
		e = 0;
		while (ifo->environ[e]) {
			env[elen + e] = strdup(ifo->environ[e]);
			if (env[elen + e] == NULL)
				goto eexit;
			e++;
		}
		elen += e;
	}
	env[elen] = NULL;

	*argv = env;
	return (ssize_t)elen;

eexit:
	logger(ifp->ctx, LOG_ERR, "%s: %m", __func__);
	if (env) {
		nenv = env;
		while (*nenv)
			free(*nenv++);
		free(env);
	}
	return -1;
}

static int
send_interface1(struct fd_list *fd, const struct interface *iface,
    const char *reason)
{
	char **env, **ep, *s;
	size_t elen;
	int retval;

	if (make_env(iface, reason, &env) == -1)
		return -1;
	s = NULL;
	elen = (size_t)arraytostr((const char *const *)env, &s);
	if ((ssize_t)elen == -1) {
		free(s);
		return -1;
	}
	retval = control_queue(fd, s, elen, 1);
	ep = env;
	while (*ep)
		free(*ep++);
	free(env);
	return retval;
}

int
send_interface(struct fd_list *fd, const struct interface *ifp)
{
	const char *reason;
	int retval = 0;
#ifdef INET
	const struct dhcp_state *d;
#endif
#ifdef INET6
	const struct dhcp6_state *d6;
#endif

	switch (ifp->carrier) {
	case LINK_UP:
		reason = "CARRIER";
		break;
	case LINK_DOWN:
		reason = "NOCARRIER";
		break;
	default:
		reason = "UNKNOWN";
		break;
	}
	if (send_interface1(fd, ifp, reason) == -1)
		retval = -1;
#ifdef INET
	if (D_STATE_RUNNING(ifp)) {
		d = D_CSTATE(ifp);
		if (send_interface1(fd, ifp, d->reason) == -1)
			retval = -1;
	}
	if (IPV4LL_STATE_RUNNING(ifp)) {
		if (send_interface1(fd, ifp, "IPV4LL") == -1)
			retval = -1;
	}
#endif

#ifdef INET6
	if (RS_STATE_RUNNING(ifp)) {
		if (send_interface1(fd, ifp, "ROUTERADVERT") == -1)
			retval = -1;
	}
	if (D6_STATE_RUNNING(ifp)) {
		d6 = D6_CSTATE(ifp);
		if (send_interface1(fd, ifp, d6->reason) == -1)
			retval = -1;
	}
#endif

	return retval;
}

int
script_runreason(const struct interface *ifp, const char *reason)
{
	char *argv[2];
	char **env = NULL, **ep;
	char *svcname, *path, *bigenv;
	size_t e, elen = 0;
	pid_t pid;
	int status = 0;
	struct fd_list *fd;

	if (ifp->options->script &&
	    (ifp->options->script[0] == '\0' ||
	    strcmp(ifp->options->script, "/dev/null") == 0) &&
	    TAILQ_FIRST(&ifp->ctx->control_fds) == NULL)
		return 0;

	/* Make our env */
	elen = (size_t)make_env(ifp, reason, &env);
	if (elen == (size_t)-1) {
		logger(ifp->ctx, LOG_ERR, "%s: make_env: %m", ifp->name);
		return -1;
	}

	if (ifp->options->script &&
	    (ifp->options->script[0] == '\0' ||
	    strcmp(ifp->options->script, "/dev/null") == 0))
	    	goto send_listeners;

	argv[0] = ifp->options->script ? ifp->options->script : UNCONST(SCRIPT);
	argv[1] = NULL;
	logger(ifp->ctx, LOG_DEBUG, "%s: executing `%s' %s",
	    ifp->name, argv[0], reason);

	/* Resize for PATH and RC_SVCNAME */
	svcname = getenv(RC_SVCNAME);
	ep = realloc(env, sizeof(char *) * (elen + 2 + (svcname ? 1 : 0)));
	if (ep == NULL) {
		elen = 0;
		goto out;
	}
	env = ep;
	/* Add path to it */
	path = getenv("PATH");
	if (path) {
		e = strlen("PATH") + strlen(path) + 2;
		env[elen] = malloc(e);
		if (env[elen] == NULL) {
			elen = 0;
			goto out;
		}
		snprintf(env[elen], e, "PATH=%s", path);
	} else {
		env[elen] = strdup(DEFAULT_PATH);
		if (env[elen] == NULL) {
			elen = 0;
			goto out;
		}
	}
	if (svcname) {
		e = strlen(RC_SVCNAME) + strlen(svcname) + 2;
		env[++elen] = malloc(e);
		if (env[elen] == NULL) {
			elen = 0;
			goto out;
		}
		snprintf(env[elen], e, "%s=%s", RC_SVCNAME, svcname);
	}
	env[++elen] = NULL;

	pid = exec_script(ifp->ctx, argv, env);
	if (pid == -1)
		logger(ifp->ctx, LOG_ERR, "%s: %s: %m", __func__, argv[0]);
	else if (pid != 0) {
		/* Wait for the script to finish */
		while (waitpid(pid, &status, 0) == -1) {
			if (errno != EINTR) {
				logger(ifp->ctx, LOG_ERR, "waitpid: %m");
				status = 0;
				break;
			}
		}
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status))
				logger(ifp->ctx, LOG_ERR,
				    "%s: %s: WEXITSTATUS %d",
				    __func__, argv[0], WEXITSTATUS(status));
		} else if (WIFSIGNALED(status))
			logger(ifp->ctx, LOG_ERR, "%s: %s: %s",
			    __func__, argv[0], strsignal(WTERMSIG(status)));
	}

send_listeners:
	/* Send to our listeners */
	bigenv = NULL;
	status = 0;
	TAILQ_FOREACH(fd, &ifp->ctx->control_fds, next) {
		if (!(fd->flags & FD_LISTEN))
			continue;
		if (bigenv == NULL) {
			elen = (size_t)arraytostr((const char *const *)env,
			    &bigenv);
			if ((ssize_t)elen == -1) {
				logger(ifp->ctx, LOG_ERR, "%s: arraytostr: %m",
				    ifp->name);
				    break;
			}
		}
		if (control_queue(fd, bigenv, elen, 1) == -1)
			logger(ifp->ctx, LOG_ERR,
			    "%s: control_queue: %m", __func__);
		else
			status = 1;
	}
	if (!status)
		free(bigenv);

out:
	/* Cleanup */
	ep = env;
	while (*ep)
		free(*ep++);
	free(env);
	if (elen == 0)
		return -1;
	return WEXITSTATUS(status);
}

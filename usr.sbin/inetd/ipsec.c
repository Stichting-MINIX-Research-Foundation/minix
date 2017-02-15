/*	$NetBSD: ipsec.c,v 1.4 2012/01/04 16:09:43 drochner Exp $	*/

/*
 * Copyright (C) 1999 WIDE Project.
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifdef IPSEC
#include <netipsec/ipsec.h>
#ifndef IPSEC_POLICY_IPSEC	/* no ipsec support on old ipsec */
#undef IPSEC
#endif
#endif

#include "ipsec.h"

#ifdef IPSEC
int
ipsecsetup(int af, int fd, const char *policy)
{
	char *p0, *p;
	int error;

	if (!policy || policy == '\0')
		p0 = p = strdup("in entrust; out entrust");
	else
		p0 = p = strdup(policy);

	error = 0;
	for (;;) {
		p = strtok(p, ";");
		if (p == NULL)
			break;
		while (*p && isspace((unsigned char)*p))
			p++;
		if (!*p) {
			p = NULL;
			continue;
		}
		error = ipsecsetup0(af, fd, p, 1);
		if (error < 0)
			break;
		p = NULL;
	}

	free(p0);
	return error;
}

int
ipsecsetup_test(const char *policy)
{
	char *p0, *p;
	char *buf;
	int error;

	if (!policy)
		return -1;
	p0 = p = strdup(policy);
	if (p == NULL)
		return -1;

	error = 0;
	for (;;) {
		p = strtok(p, ";");
		if (p == NULL)
			break;
		while (*p && isspace((unsigned char)*p))
			p++;
		if (!*p) {
			p = NULL;
			continue;
		}
		buf = ipsec_set_policy(p, (int)strlen(p));
		if (buf == NULL) {
			error = -1;
			break;
		}
		free(buf);
		p = NULL;
	}

	free(p0);
	return error;
}

int
ipsecsetup0(int af, int fd, const char *policy, int commit)
{
	int level;
	int opt;
	char *buf;
	int error;

	switch (af) {
	case AF_INET:
		level = IPPROTO_IP;
		opt = IP_IPSEC_POLICY;
		break;
#ifdef INET6
	case AF_INET6:
		level = IPPROTO_IPV6;
		opt = IPV6_IPSEC_POLICY;
		break;
#endif
	default:
		return -1;
	}

	buf = ipsec_set_policy(policy, (int)strlen(policy));
	if (buf != NULL) {
		error = 0;
		if (commit && setsockopt(fd, level, opt,
		    buf, (socklen_t)ipsec_get_policylen(buf)) < 0) {
			error = -1;
		}
		free(buf);
	} else
		error = -1;
	return error;
}
#endif

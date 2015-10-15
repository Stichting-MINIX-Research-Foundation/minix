/*	$NetBSD: getifaddrs_w32.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

/***********************************************************************
 * Copyright (c) 2009, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

#include<config.h>

#include <krb5/roken.h>

#include <ifaddrs.h>

#ifndef _WIN32
#error This is a Windows specific implementation.
#endif

static struct sockaddr *
dupaddr(const sockaddr_gen * src)
{
    sockaddr_gen * d = malloc(sizeof(*d));

    if (d) {
	memcpy(d, src, sizeof(*d));
    }

    return (struct sockaddr *) d;
}

int ROKEN_LIB_FUNCTION
rk_getifaddrs(struct ifaddrs **ifpp)
{
    SOCKET s = INVALID_SOCKET;
    size_t il_len = 8192;
    int ret = -1;
    INTERFACE_INFO *il = NULL;

    *ifpp = NULL;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET)
	return -1;

    for (;;) {
	DWORD cbret = 0;

	il = malloc(il_len);
	if (!il)
	    break;

	ZeroMemory(il, il_len);

	if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0,
		     (LPVOID) il, (DWORD) il_len, &cbret,
		     NULL, NULL) == 0) {
	    il_len = cbret;
	    break;
	}

	free (il);
	il = NULL;

	if (WSAGetLastError() == WSAEFAULT && cbret > il_len) {
	    il_len = cbret;
	} else {
	    break;
	}
    }

    if (!il)
	goto _exit;

    /* il is an array of INTERFACE_INFO structures.  il_len has the
       actual size of the buffer.  The number of elements is
       il_len/sizeof(INTERFACE_INFO) */

    {
	size_t n = il_len / sizeof(INTERFACE_INFO);
	size_t i;

	for (i = 0; i < n; i++ ) {
	    struct ifaddrs *ifp;

	    ifp = malloc(sizeof(*ifp));
	    if (ifp == NULL)
		break;

	    ZeroMemory(ifp, sizeof(*ifp));

	    ifp->ifa_next = NULL;
	    ifp->ifa_name = NULL;
	    ifp->ifa_flags = il[i].iiFlags;
	    ifp->ifa_addr = dupaddr(&il[i].iiAddress);
	    ifp->ifa_netmask = dupaddr(&il[i].iiNetmask);
	    ifp->ifa_broadaddr = dupaddr(&il[i].iiBroadcastAddress);
	    ifp->ifa_data = NULL;

	    *ifpp = ifp;
	    ifpp = &ifp->ifa_next;
	}

	if (i == n)
	    ret = 0;
    }

 _exit:

    if (s != INVALID_SOCKET)
	closesocket(s);

    if (il)
	free (il);

    return ret;
}

void ROKEN_LIB_FUNCTION
rk_freeifaddrs(struct ifaddrs *ifp)
{
    struct ifaddrs *p, *q;

    for(p = ifp; p; ) {
	if (p->ifa_name)
	    free(p->ifa_name);
	if(p->ifa_addr)
	    free(p->ifa_addr);
	if(p->ifa_dstaddr)
	    free(p->ifa_dstaddr);
	if(p->ifa_netmask)
	    free(p->ifa_netmask);
	if(p->ifa_data)
	    free(p->ifa_data);
	q = p;
	p = p->ifa_next;
	free(q);
    }
}

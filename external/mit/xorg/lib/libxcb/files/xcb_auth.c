/* Copyright (C) 2001-2004 Bart Massey and Jamey Sharp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

/* Authorization systems for the X protocol. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <X11/Xauth.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __INTERIX
/* _don't_ ask. interix has INADDR_LOOPBACK in here. */
#include <rpc/types.h>
#endif

#ifdef _WIN32
#ifdef HASXDMAUTH
/* We must include the wrapped windows.h before any system header which includes
   it unwrapped, to avoid conflicts with types defined in X headers */
#include <X11/Xwindows.h>
#endif
#include "xcb_windefs.h"
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#endif /* _WIN32 */

#include "xcb.h"
#include "xcbint.h"

#ifdef HASXDMAUTH
#include <X11/Xdmcp.h>
#endif

enum auth_protos {
#ifdef HASXDMAUTH
    AUTH_XA1,
#endif
    AUTH_MC1,
    N_AUTH_PROTOS
};

#define AUTH_PROTO_XDM_AUTHORIZATION "XDM-AUTHORIZATION-1"
#define AUTH_PROTO_MIT_MAGIC_COOKIE "MIT-MAGIC-COOKIE-1"

static char *authnames[N_AUTH_PROTOS] = {
#ifdef HASXDMAUTH
    AUTH_PROTO_XDM_AUTHORIZATION,
#endif
    AUTH_PROTO_MIT_MAGIC_COOKIE,
};

static int authnameslen[N_AUTH_PROTOS] = {
#ifdef HASXDMAUTH
    sizeof(AUTH_PROTO_XDM_AUTHORIZATION) - 1,
#endif
    sizeof(AUTH_PROTO_MIT_MAGIC_COOKIE) - 1,
};

static size_t memdup(char **dst, void *src, size_t len)
{
    if(len)
	*dst = malloc(len);
    else
	*dst = 0;
    if(!*dst)
	return 0;
    memcpy(*dst, src, len);
    return len;
}

static int authname_match(enum auth_protos kind, char *name, size_t namelen)
{
    if(authnameslen[kind] != namelen)
	return 0;
    if(memcmp(authnames[kind], name, namelen))
	return 0;
    return 1;
}

#define SIN6_ADDR(s) (&((struct sockaddr_in6 *)s)->sin6_addr)

static Xauth *get_authptr(struct sockaddr *sockname, int display)
{
    char *addr = 0;
    int addrlen = 0;
    unsigned short family;
    char hostnamebuf[256];   /* big enough for max hostname */
    char dispbuf[40];   /* big enough to hold more than 2^64 base 10 */
    int dispbuflen;

    family = FamilyLocal; /* 256 */
    switch(sockname->sa_family)
    {
#ifdef AF_INET6
    case AF_INET6:
        addr = (char *) SIN6_ADDR(sockname);
        addrlen = sizeof(*SIN6_ADDR(sockname));
        if(!IN6_IS_ADDR_V4MAPPED(SIN6_ADDR(sockname)))
        {
            if(!IN6_IS_ADDR_LOOPBACK(SIN6_ADDR(sockname)))
                family = XCB_FAMILY_INTERNET_6;
            break;
        }
        addr += 12;
        /* if v4-mapped, fall through. */
#endif
    case AF_INET:
        if(!addr)
            addr = (char *) &((struct sockaddr_in *)sockname)->sin_addr;
        addrlen = sizeof(((struct sockaddr_in *)sockname)->sin_addr);
        if(*(in_addr_t *) addr != htonl(INADDR_LOOPBACK))
            family = XCB_FAMILY_INTERNET;
        break;
    case AF_UNIX:
        break;
    default:
        return 0;   /* cannot authenticate this family */
    }

    dispbuflen = snprintf(dispbuf, sizeof(dispbuf), "%d", display);
    if(dispbuflen < 0)
        return 0;
    /* snprintf may have truncate our text */
    dispbuflen = MIN(dispbuflen, sizeof(dispbuf) - 1);

    if (family == FamilyLocal) {
        if (gethostname(hostnamebuf, sizeof(hostnamebuf)) == -1)
            return 0;   /* do not know own hostname */
        addr = hostnamebuf;
        addrlen = strlen(addr);
    }

    return XauGetBestAuthByAddr (family,
                                 (unsigned short) addrlen, addr,
                                 (unsigned short) dispbuflen, dispbuf,
                                 N_AUTH_PROTOS, authnames, authnameslen);
}

#ifdef HASXDMAUTH
static int next_nonce(void)
{
    static int nonce = 0;
    static pthread_mutex_t nonce_mutex = PTHREAD_MUTEX_INITIALIZER;
    int ret;
    pthread_mutex_lock(&nonce_mutex);
    ret = nonce++;
    pthread_mutex_unlock(&nonce_mutex);
    return ret;
}

static void do_append(char *buf, int *idxp, void *val, size_t valsize) {
    memcpy(buf + *idxp, val, valsize);
    *idxp += valsize;
}
#endif
     
static int compute_auth(xcb_auth_info_t *info, Xauth *authptr, struct sockaddr *sockname)
{
    if (authname_match(AUTH_MC1, authptr->name, authptr->name_length)) {
        info->datalen = memdup(&info->data, authptr->data, authptr->data_length);
        if(!info->datalen)
            return 0;
        return 1;
    }
#ifdef HASXDMAUTH
#define APPEND(buf,idx,val) do_append((buf),&(idx),&(val),sizeof(val))
    if (authname_match(AUTH_XA1, authptr->name, authptr->name_length)) {
	int j;

	info->data = malloc(192 / 8);
	if(!info->data)
	    return 0;

	for (j = 0; j < 8; j++)
	    info->data[j] = authptr->data[j];
	switch(sockname->sa_family) {
        case AF_INET:
            /*block*/ {
	    struct sockaddr_in *si = (struct sockaddr_in *) sockname;
	    APPEND(info->data, j, si->sin_addr.s_addr);
	    APPEND(info->data, j, si->sin_port);
	}
	break;
#ifdef AF_INET6
        case AF_INET6:
            /*block*/ {
            struct sockaddr_in6 *si6 = (struct sockaddr_in6 *) sockname;
            if(IN6_IS_ADDR_V4MAPPED(SIN6_ADDR(sockname)))
            {
                do_append(info->data, &j, &si6->sin6_addr.s6_addr[12], 4);
                APPEND(info->data, j, si6->sin6_port);
            }
            else
            {
                /* XDM-AUTHORIZATION-1 does not handle IPv6 correctly.  Do the
                   same thing Xlib does: use all zeroes for the 4-byte address
                   and 2-byte port number. */
                uint32_t fakeaddr = 0;
                uint16_t fakeport = 0;
                APPEND(info->data, j, fakeaddr);
                APPEND(info->data, j, fakeport);
            }
        }
        break;
#endif
        case AF_UNIX:
            /*block*/ {
	    uint32_t fakeaddr = htonl(0xffffffff - next_nonce());
	    uint16_t fakeport = htons(getpid());
	    APPEND(info->data, j, fakeaddr);
	    APPEND(info->data, j, fakeport);
	}
	break;
        default:
            free(info->data);
            return 0;   /* do not know how to build this */
	}
	{
	    uint32_t now = htonl(time(0));
	    APPEND(info->data, j, now);
	}
	assert(j <= 192 / 8);
	while (j < 192 / 8)
	    info->data[j++] = 0;
	info->datalen = j;
	XdmcpWrap ((unsigned char *) info->data, (unsigned char *) authptr->data + 8, (unsigned char *) info->data, info->datalen);
	return 1;
    }
#undef APPEND
#endif

    return 0;   /* Unknown authorization type */
}

/* `sockaddr_un.sun_path' typical size usually ranges between 92 and 108 */
#define INITIAL_SOCKNAME_SLACK 108

/* Return a dynamically allocated socket address structure according
   to the value returned by either getpeername() or getsockname()
   (according to POSIX, applications should not assume a particular
   length for `sockaddr_un.sun_path') */
static struct sockaddr *get_peer_sock_name(int (*socket_func)(int,
							      struct sockaddr *,
							      socklen_t *),
					   int fd)
{
    socklen_t socknamelen = sizeof(struct sockaddr) + INITIAL_SOCKNAME_SLACK;
    socklen_t actual_socknamelen = socknamelen;
    struct sockaddr *sockname = malloc(socknamelen);

    if (sockname == NULL)
        return NULL;

    /* Both getpeername() and getsockname() truncates sockname if
       there is not enough space and set the required length in
       actual_socknamelen */
    if (socket_func(fd, sockname, &actual_socknamelen) == -1)
        goto sock_or_realloc_error;

    if (actual_socknamelen > socknamelen)
    {
        struct sockaddr *new_sockname = NULL;
        socknamelen = actual_socknamelen;

        if ((new_sockname = realloc(sockname, actual_socknamelen)) == NULL)
            goto sock_or_realloc_error;

        sockname = new_sockname;

        if (socket_func(fd, sockname, &actual_socknamelen) == -1 ||
            actual_socknamelen > socknamelen)
            goto sock_or_realloc_error;
    }

    return sockname;

 sock_or_realloc_error:
    free(sockname);
    return NULL;
}

int _xcb_get_auth_info(int fd, xcb_auth_info_t *info, int display)
{
    /* code adapted from Xlib/ConnDis.c, xtrans/Xtranssocket.c,
       xtrans/Xtransutils.c */
    struct sockaddr *sockname = NULL;
    int gotsockname = 0;
    Xauth *authptr = 0;
    int ret = 1;

    /* Some systems like hpux or Hurd do not expose peer names
     * for UNIX Domain Sockets, but this is irrelevant,
     * since compute_auth() ignores the peer name in this
     * case anyway.*/
    if ((sockname = get_peer_sock_name(getpeername, fd)) == NULL)
    {
        if ((sockname = get_peer_sock_name(getsockname, fd)) == NULL)
            return 0;   /* can only authenticate sockets */
        if (sockname->sa_family != AF_UNIX)
        {
            free(sockname);
            return 0;   /* except for AF_UNIX, sockets should have peernames */
        }
        gotsockname = 1;
    }

    authptr = get_authptr(sockname, display);
    if (authptr == 0)
    {
        free(sockname);
        return 0;   /* cannot find good auth data */
    }

    info->namelen = memdup(&info->name, authptr->name, authptr->name_length);
    if (!info->namelen)
        goto no_auth;   /* out of memory */

    if (!gotsockname)
    {
        free(sockname);

        if ((sockname = get_peer_sock_name(getsockname, fd)) == NULL)
        {
            free(info->name);
            goto no_auth;   /* can only authenticate sockets */
        }
    }

    ret = compute_auth(info, authptr, sockname);
    if(!ret)
    {
        free(info->name);
        goto no_auth;   /* cannot build auth record */
    }

    free(sockname);
    sockname = NULL;

    XauDisposeAuth(authptr);
    return ret;

 no_auth:
    free(sockname);

    info->name = 0;
    info->namelen = 0;
    XauDisposeAuth(authptr);
    return 0;
}

#ifndef lint
static char *rcsid = "$Id: stub.c,v 1.1.1.1 2003-06-04 00:27:13 marka Exp $";
#endif

/*
 * Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#include <idn/logmacro.h>
#include <idn/debug.h>

#include "stub.h"

#ifndef RTLD_NEXT
typedef struct {
	const char *name;
	void *handle;
} shared_obj_t;

static shared_obj_t shobj[] = {
#ifdef SOPATH_LIBC
	{ SOPATH_LIBC },
#endif
#ifdef SOPATH_LIBNSL
	{ SOPATH_LIBNSL },
#endif
	{ NULL },
};
#endif

static void	*shared_obj_findsym(void *handle, const char *name);
static void	*shared_obj_findsymx(void *handle, const char *name);
static void	*get_func_addr(const char *name);

#ifndef RTLD_NEXT
static void *
shared_obj_open(const char *path) {
#ifdef HAVE_DLOPEN
	return (dlopen(path, RTLD_LAZY));
#endif
	FATAL(("stub: no way to load shared object file\n"));
	return (NULL);
}
#endif

static void *
shared_obj_findsym(void *handle, const char *name) {
	char namebuf[100];
	void *addr;
	static int need_leading_underscore = -1;

	/* Prepend underscore. */
	namebuf[0] = '_';
	(void)strcpy(namebuf + 1, name);
	name = namebuf;

	if (need_leading_underscore < 0) {
		/* First try without one. */
		if ((addr = shared_obj_findsymx(handle, name + 1)) != NULL) {
			need_leading_underscore = 0;
			return (addr);
		}
		/* Then try with one. */
		if ((addr = shared_obj_findsymx(handle, name)) != NULL) {
			need_leading_underscore = 1;
			return (addr);
		}
	} else if (need_leading_underscore) {
		return (shared_obj_findsymx(handle, name));
	} else {
		return (shared_obj_findsymx(handle, name + 1));
	}
	return (NULL);
}
		
static void *
shared_obj_findsymx(void *handle, const char *name) {
#ifdef HAVE_DLSYM
	return (dlsym(handle, name));
#endif
	/* logging */
	FATAL(("stub: no way to get symbol address\n"));
	return (NULL);
}

static void *
get_func_addr(const char *name) {
#ifdef RTLD_NEXT
	void *addr = shared_obj_findsym(RTLD_NEXT, name);

	if (addr != NULL) {
		TRACE(("stub: %s found in the subsequent objects\n", name));
		return (addr);
	}
#else
	int i;

	for (i = 0; shobj[i].name != NULL; i++) {
		if (shobj[i].handle == NULL) {
			TRACE(("stub: loading %s\n", shobj[i].name));
			shobj[i].handle = shared_obj_open(shobj[i].name);
		}
		if (shobj[i].handle != NULL) {
			void *addr = shared_obj_findsym(shobj[i].handle, name);
			if (addr != NULL) {
				TRACE(("stub: %s found in %s\n",
				       name, shobj[i].name));
				return (addr);
			}
		}
	}
#endif
	TRACE(("stub: %s not found\n", name));
	return (NULL);
}

#ifdef HAVE_GETHOSTBYNAME
struct hostent *
idn_stub_gethostbyname(const char *name) {
	static struct hostent *(*fp)(const char *name);

	if (fp == NULL)
		fp = get_func_addr("gethostbyname");
	if (fp != NULL)
		return ((*fp)(name));
	return (NULL);
}
#endif

#ifdef HAVE_GETHOSTBYNAME2
struct hostent *
idn_stub_gethostbyname2(const char *name, int af) {
	static struct hostent *(*fp)(const char *name, int af);

	if (fp == NULL)
		fp = get_func_addr("gethostbyname2");
	if (fp != NULL)
		return ((*fp)(name, af));
	return (NULL);
}
#endif

#ifdef HAVE_GETHOSTBYADDR
struct hostent *
idn_stub_gethostbyaddr(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type) {
	static struct hostent *(*fp)(GHBA_ADDR_T name,
				     GHBA_ADDRLEN_T len, int type);

	if (fp == NULL)
		fp = get_func_addr("gethostbyaddr");
	if (fp != NULL)
		return ((*fp)(addr, len, type));
	return (NULL);
}
#endif

#ifdef GETHOST_R_GLIBC_FLAVOR

#ifdef HAVE_GETHOSTBYNAME_R
int
idn_stub_gethostbyname_r(const char *name, struct hostent *result,
			 char *buffer, size_t buflen,
			 struct hostent **rp, int *errp)
{
	static int (*fp)(const char *name, struct hostent *result,
			 char *buffer, size_t buflen,
			 struct hostent **rp, int *errp);

	if (fp == NULL)
		fp = get_func_addr("gethostbyname_r");
	if (fp != NULL)
		return ((*fp)(name, result, buffer, buflen, rp, errp));
	return (ENOENT);	/* ??? */
}
#endif

#ifdef HAVE_GETHOSTBYNAME2_R
int
idn_stub_gethostbyname2_r(const char *name, int af, struct hostent *result,
			  char *buffer, size_t buflen,
			  struct hostent **rp, int *errp)
{
	static int (*fp)(const char *name, int af, struct hostent *result,
			 char *buffer, size_t buflen,
			 struct hostent **rp, int *errp);

	if (fp == NULL)
		fp = get_func_addr("gethostbyname2_r");
	if (fp != NULL)
		return ((*fp)(name, af, result, buffer, buflen, rp, errp));
	return (ENOENT);	/* ??? */
}
#endif

#ifdef HAVE_GETHOSTBYADDR_R
int
idn_stub_gethostbyaddr_r(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type,
			 struct hostent *result, char *buffer,
			 size_t buflen, struct hostent **rp, int *errp)
{
	static int (*fp)(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type,
			 struct hostent *result, char *buffer,
			 size_t buflen, struct hostent **rp, int *errp);

	if (fp == NULL)
		fp = get_func_addr("gethostbyaddr_r");
	if (fp != NULL)
		return ((*fp)(addr, len, type, result,
			      buffer, buflen, rp, errp));
	return (ENOENT);	/* ??? */
}
#endif

#else /* GETHOST_R_GLIBC_FLAVOR */

#ifdef HAVE_GETHOSTBYNAME_R
struct hostent *
idn_stub_gethostbyname_r(const char *name, struct hostent *result,
			 char *buffer, int buflen, int *errp)
{
	static struct hostent *(*fp)(const char *name, struct hostent *result,
				     char *buffer, int buflen, int *errp);

	if (fp == NULL)
		fp = get_func_addr("gethostbyname_r");
	if (fp != NULL)
		return ((*fp)(name, result, buffer, buflen, errp));
	return (NULL);
}
#endif

#ifdef HAVE_GETHOSTBYADDR_R
struct hostent *
idn_stub_gethostbyaddr_r(GHBA_ADDR_T addr, int len, int type,
			 struct hostent *result, char *buffer,
			 int buflen, int *errp)
{
	static struct hostent *(*fp)(GHBA_ADDR_T addr, int len, int type,
				     struct hostent *result, char *buffer,
				     int buflen, int *errp);

	if (fp == NULL)
		fp = get_func_addr("gethostbyaddr_r");
	if (fp != NULL)
		return ((*fp)(addr, len, type, result, buffer, buflen, errp));
	return (NULL);
}
#endif

#endif /* GETHOST_R_GLIBC_FLAVOR */

#ifdef HAVE_GETIPNODEBYNAME
struct hostent *
idn_stub_getipnodebyname(const char *name, int af, int flags, int *errp) {
	static struct hostent *(*fp)(const char *name, int af, int flags,
				     int *errp);

	if (fp == NULL)
		fp = get_func_addr("getipnodebyname");
	if (fp != NULL)
		return ((*fp)(name, af, flags, errp));
	return (NULL);
}
#endif

#ifdef HAVE_GETIPNODEBYADDR
struct hostent *
idn_stub_getipnodebyaddr(const void *src, size_t len, int af, int *errp) {
	static struct hostent *(*fp)(const void *src, size_t len, int af,
				     int *errp);

	if (fp == NULL)
		fp = get_func_addr("getipnodebyaddr");
	if (fp != NULL)
		return ((*fp)(src, len, af, errp));
	return (NULL);
}
#endif

#ifdef HAVE_FREEHOSTENT
void
idn_stub_freehostent(struct hostent *hp) {
	static void (*fp)(struct hostent *hp);

	if (fp == NULL)
		fp = get_func_addr("freehostent");
	if (fp != NULL)
		(*fp)(hp);
}
#endif

#ifdef HAVE_GETADDRINFO
int
idn_stub_getaddrinfo(const char *nodename, const char *servname,
		     const struct addrinfo *hints, struct addrinfo **res)
{
	static int (*fp)(const char *nodename, const char *servname,
			 const struct addrinfo *hints, struct addrinfo **res);

	if (fp == NULL)
		fp = get_func_addr("getaddrinfo");
	if (fp != NULL)
		return ((*fp)(nodename, servname, hints, res));
	return (EAI_FAIL);
}
#endif

#ifdef HAVE_FREEADDRINFO
void
idn_stub_freeaddrinfo(struct addrinfo *aip) {
	static void (*fp)(struct addrinfo *aip);

	if (fp == NULL)
		fp = get_func_addr("freeaddrinfo");
	if (fp != NULL)
		(*fp)(aip);
}
#endif

#ifdef HAVE_GETNAMEINFO
int
idn_stub_getnameinfo(const struct sockaddr *sa, GNI_SALEN_T salen,
		     char *host, GNI_HOSTLEN_T hostlen,
		     char *serv, GNI_SERVLEN_T servlen, GNI_FLAGS_T flags) {
	static int (*fp)(const struct sockaddr *sa, GNI_SALEN_T salen,
			 char *host, GNI_HOSTLEN_T hostlen,
			 char *serv, GNI_SERVLEN_T servlen,
			 GNI_FLAGS_T flags);

	if (fp == NULL)
		fp = get_func_addr("getnameinfo");
	if (fp != NULL)
		return ((*fp)(sa, salen, host, hostlen, serv, servlen, flags));
	return (EAI_FAIL);
}
#endif

#ifndef lint
static char *rcsid = "$Id: resolver.c,v 1.1.1.1 2003-06-04 00:27:12 marka Exp $";
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

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include <idn/api.h>
#include <idn/log.h>
#include <idn/logmacro.h>
#include <idn/debug.h>

#ifdef FOR_RUNIDN
/*
 * This file is specially compiled for runidn.
 * runidn replaces existing resolver functions dynamically with ones
 * with IDN processing (encoding conversion and normalization).
 * So entry names must be same as the system's one.
 */
#include "stub.h"

#define ENTRY(name) name
#define REAL(name) idn_stub_ ## name
#else
/*
 * For normal use.  All the entry names are prefixed with "idn_resolver_".
 * <idn/resolver.h> has bunch of #defines to substitute the standard
 * name resolver functions with ones provided here.
 */
#include "resolver.h"
#undef  gethostbyname
#undef  gethostbyname2
#undef  gethostbyaddr
#undef  gethostbyname_r
#undef  gethostbyname2_r
#undef  gethostbyaddr_r
#undef  getipnodebyname
#undef  getipnodebyaddr
#undef  getaddrinfo
#undef  getnameinfo

#define ENTRY(name) idn_resolver_ ## name
#define REAL(name) name
#endif

#define IDN_NAME_SIZE		512

#define IDN_HOSTBUF_SIZE	2048
typedef union {
	char *dummy_for_alignment;
	char data[IDN_HOSTBUF_SIZE];
} hostbuf_t;

typedef struct obj_lock {
	void *key;
	struct obj_lock *next;
} obj_lock_t;

#define OBJLOCKHASH_SIZE	127
static obj_lock_t *obj_lock_hash[OBJLOCKHASH_SIZE];

/*
 * This variable is to prevent IDN processing occuring more than once for
 * a single name resolution.  This will happen if some resolver function
 * is implemented using another function (e.g. gethostbyname() implemented
 * using gethostbyname2()).
 * No, using the static variable is not a correct thing to do for a multi-
 * threading environment, but I don't think of a better solution..
 */
static int idn_isprocessing = 0;

static int		obj_hash(void *key);
static int		obj_islocked(void *key);
static void		obj_lock(void *key);
static void		obj_unlock(void *key);
static struct hostent	*copy_decode_hostent_static(struct hostent *hp,
						    struct hostent *newhp,
						    char *buf, size_t buflen,
						    int *errp);
static char		*decode_name_dynamic(const char *name);
static struct hostent	*copy_decode_hostent_dynamic(struct hostent *hp,
						     int *errp);
static void		free_copied_hostent(struct hostent *hp);
#ifdef HAVE_GETADDRINFO
static struct addrinfo	*copy_decode_addrinfo_dynamic(struct addrinfo *aip);
#endif
#ifdef HAVE_FREEADDRINFO
static void		free_copied_addrinfo(struct addrinfo *aip);
#endif

/*
 * Object locking facility.
 */

static int
obj_hash(void *key) {
	/*
	 * Hash function for obj_*.
	 * 'key' is supposed to be an address.
	 */
	unsigned long v = (unsigned long)key;

	return ((v >> 3) % OBJLOCKHASH_SIZE);
}

static int
obj_islocked(void *key)
{
	/*
	 * Check if the object specified by 'key' is locked.
	 * Return 1 if so, 0 otherwise.
	 */
	int h = obj_hash(key);
	obj_lock_t *olp = obj_lock_hash[h];

	while (olp != NULL) {
		if (olp->key == key)
			return (1);
		olp = olp->next;
	}
	return (0);
}

static void
obj_lock(void *key)
{
	/*
	 * Lock an object specified by 'key'.
	 */
	int h = obj_hash(key);
	obj_lock_t *olp;

	olp = malloc(sizeof(obj_lock_t));
	if (olp != NULL) {
		olp->key = key;
		olp->next = obj_lock_hash[h];
		obj_lock_hash[h] = olp;
	}
}

static void
obj_unlock(void *key)
{
	/*
	 * Unlock an object specified by 'key'.
	 */
	int h = obj_hash(key);
	obj_lock_t *olp, *olp0;

	olp = obj_lock_hash[h];
	olp0 = NULL;
	while (olp != NULL) {
		if (olp->key == key) {
			if (olp0 == NULL)
				obj_lock_hash[h] = olp->next;
			else
				olp0->next = olp->next;
			free(olp);
			return;
		}
		olp0 = olp;
		olp = olp->next;
	}
}

static struct hostent *
copy_decode_hostent_static(struct hostent *hp, struct hostent *newhp,
			   char *buf, size_t buflen, int *errp)
{
	/*
	 * Copy "struct hostent" data referenced by 'hp' to 'newhp'.
	 * It's a deep-copy, meaning all the data referenced by 'hp' are
	 * also copied.  They are copied into 'buf', whose length is 'buflen'.
	 * The domain names ('hp->h_name' and 'hp->h_aliases') are
	 * decoded from ACE to the local encoding before they are copied.
	 * If 'buf' is too small to hold all the data, NULL will be
	 * returned and '*errp' is set to NO_RECOVERY.
	 */
	int naliases = 0;
	int naddrs = 0;

	if (hp == NULL)
		return (NULL);

	*newhp = *hp;

	if (hp->h_aliases != NULL) {
		/*
		 * Allocate aliase table in 'buf'.
		 */
		size_t sz;

		while (hp->h_aliases[naliases] != NULL)
			naliases++;

		newhp->h_aliases = (char **)buf;
		sz = sizeof(char *) * (naliases + 1);

		if (buflen < sz)
			goto overflow;

		buf += sz;
		buflen -= sz;
	}

	if (hp->h_addr_list != NULL) {
		/*
		 * Allocate address table in 'buf'.
		 */
		size_t sz;
		int i;

		while (hp->h_addr_list[naddrs] != NULL)
			naddrs++;

		newhp->h_addr_list = (char **)buf;
		sz = sizeof(char *) * (naddrs + 1);

		if (buflen < sz)
			goto overflow;

		buf += sz;
		buflen -= sz;

		/*
		 * Copy the addresses.
		 */
		sz = hp->h_length * naddrs;
		if (buflen < sz)
			goto overflow;

		for (i = 0; i < naddrs; i++) {
			newhp->h_addr_list[i] = buf;
			memcpy(buf, hp->h_addr_list[i], hp->h_length);
			buf += hp->h_length;
		}
		newhp->h_addr_list[naddrs] = NULL;

		buf += sz;
		buflen -= sz;
	}

	if (hp->h_name != NULL) {
		/*
		 * Decode the name in h_name.
		 */
		idn_result_t r;
		size_t slen;

		idn_enable(1);
		idn_nameinit(1);
		r = idn_decodename(IDN_DECODE_APP, hp->h_name,
				   buf, buflen);
		switch (r) {
		case idn_success:
			newhp->h_name = buf;
			break;
		default:
			/* Copy hp->h_name verbatim. */
			if (strlen(hp->h_name) + 1 <= buflen) {
				newhp->h_name = buf;
				strcpy(buf, hp->h_name);
				break;
			}
			/* falllthrough */
		case idn_buffer_overflow:
			goto overflow;
		}

		slen = strlen(buf) + 1;
		buf += slen;
		buflen -= slen;
	}

	if (hp->h_aliases != NULL) {
		/*
		 * Decode the names in h_aliases.
		 */
		char **aliases = hp->h_aliases;
		char **newaliases = newhp->h_aliases;
		int i;

		for (i = 0; i < naliases; i++) {
			idn_result_t r;
			size_t slen;

			idn_enable(1);
			idn_nameinit(1);
			r = idn_decodename(IDN_DECODE_APP, aliases[i],
					   buf, buflen);

			switch (r) {
			case idn_success:
				newaliases[i] = buf;
				break;
			default:
				/* Copy hp->h_name verbatim. */
				if (strlen(aliases[i]) + 1 <= buflen) {
					newaliases[i] = buf;
					strcpy(buf, aliases[i]);
					break;
				}
				/* falllthrough */
			case idn_buffer_overflow:
				goto overflow;
			}

			slen = strlen(buf) + 1;
			buf += slen;
			buflen -= slen;
		}
		newaliases[naliases] = NULL;
	}

	return (newhp);

 overflow:
	*errp = NO_RECOVERY;
	return (NULL);
}

static char *
decode_name_dynamic(const char *name) {
	idn_result_t r;
	char buf[IDN_NAME_SIZE];
	char *s;

	idn_enable(1);
	idn_nameinit(1);
	r = idn_decodename(IDN_DECODE_APP, name, buf, sizeof(buf));
	if (r == idn_success) {
		name = buf;
	}
	s = malloc(strlen(name) + 1);
	if (s == NULL)
		return (NULL);
	else
		return (strcpy(s, name));
}
		
static struct hostent *
copy_decode_hostent_dynamic(struct hostent *hp, int *errp) {
	/*
	 * Make a deep-copy of the data referenced by 'hp', and return
	 * a pointer to the copied data.
	 * All the data are dynamically allocated using malloc().
	 * The domain names ('hp->h_name' and 'hp->h_aliases') are
	 * decoded from ACE to the local encoding before they are copied.
	 * If malloc() fails, NULL will be returned and '*errp' is set to
	 * NO_RECOVERY.
	 */
	struct hostent *newhp;
	char **pp;
	size_t alloc_size;
	int naliases = 0;
	int naddrs = 0;
	int i;

	if (hp == NULL)
		return (NULL);

	if (hp->h_aliases != NULL) {
		while (hp->h_aliases[naliases] != NULL)
			naliases++;
	}

	if (hp->h_addr_list != NULL) {
		while (hp->h_addr_list[naddrs] != NULL)
			naddrs++;
	}

	alloc_size = sizeof(struct hostent) +
		sizeof(char *) * (naliases + 1) +
		sizeof(char *) * (naddrs + 1) +
		hp->h_length * naddrs;

	if ((newhp = malloc(alloc_size)) == NULL) {
		return (hp);
	}

	memset(newhp, 0, alloc_size);

	pp = (char **)(newhp + 1);

	if (hp->h_name != NULL) {
		newhp->h_name = decode_name_dynamic(hp->h_name);
		if (newhp->h_name == NULL)
			goto alloc_fail;
	}

	newhp->h_addrtype = hp->h_addrtype;
	newhp->h_length = hp->h_length;

	if (hp->h_aliases != NULL) {
		newhp->h_aliases = pp;
		for (i = 0; i < naliases; i++) {
			newhp->h_aliases[i] =
				decode_name_dynamic(hp->h_aliases[i]);
			if (newhp->h_aliases[i] == NULL)
				goto alloc_fail;
		}
		newhp->h_aliases[naliases] = NULL;
		pp += naliases + 1;
	}

	if (hp->h_addr_list != NULL) {
		char *p;

		newhp->h_addr_list = pp;
		pp += naddrs + 1;
		p = (char *)pp;

		for (i = 0; i < naddrs; i++) {
			newhp->h_addr_list[i] = p;
			memcpy(p, hp->h_addr_list[i], hp->h_length);
			p += hp->h_length;
		}
		newhp->h_addr_list[naddrs] = NULL;
	}

	return (newhp);

 alloc_fail:
	free_copied_hostent(hp);
	*errp = NO_RECOVERY;
	return (NULL);
}

static void
free_copied_hostent(struct hostent *hp) {
	/*
	 * Free all the memory allocated by copy_decode_hostent_dynamic().
	 */
	if (hp->h_name != NULL)
		free(hp->h_name);
	if (hp->h_aliases != NULL) {
		char **pp = hp->h_aliases;
		while (*pp != NULL)
			free(*pp++);
	}
	free(hp);
}

#ifdef HAVE_GETNAMEINFO
static struct addrinfo *
copy_decode_addrinfo_dynamic(struct addrinfo *aip) {
	struct addrinfo *newaip;

	if (aip == NULL)
		return (NULL);

	newaip = malloc(sizeof(struct addrinfo) + aip->ai_addrlen);
	if (newaip == NULL)
		return (NULL);

	*newaip = *aip;
	newaip->ai_addr = (struct sockaddr *)(newaip + 1);
	memcpy(newaip->ai_addr, aip->ai_addr, aip->ai_addrlen);

	if (newaip->ai_canonname != NULL)
		newaip->ai_canonname = decode_name_dynamic(aip->ai_canonname);

	newaip->ai_next = copy_decode_addrinfo_dynamic(aip->ai_next);
	return (newaip);
}
#endif

#ifdef HAVE_FREEADDRINFO
static void
free_copied_addrinfo(struct addrinfo *aip) {
	while (aip != NULL) {
		struct addrinfo *next = aip->ai_next;

		if (aip->ai_canonname != NULL)
			free(aip->ai_canonname);
		free(aip);
		aip = next;
	}
}
#endif

#ifdef HAVE_GETHOSTBYNAME
struct hostent *
ENTRY(gethostbyname)(const char *name) {
	static hostbuf_t buf;
	static struct hostent he;
	idn_result_t r;
	struct hostent *hp;

	if (idn_isprocessing)
		return (REAL(gethostbyname)(name));

	TRACE(("gethostbyname(name=%s)\n", idn__debug_xstring(name, 60)));

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, buf.data, sizeof(buf));
	if (r == idn_success)
		name = buf.data;

	hp = copy_decode_hostent_static(REAL(gethostbyname)(name),
					&he, buf.data, sizeof(buf),
					&h_errno);
	idn_isprocessing = 0;
	return (hp);
}
#endif

#ifdef HAVE_GETHOSTBYNAME2
struct hostent *
ENTRY(gethostbyname2)(const char *name, int af) {
	static hostbuf_t buf;
	static struct hostent he;
	idn_result_t r;
	struct hostent *hp;

	if (idn_isprocessing)
		return (REAL(gethostbyname2)(name, af));

	TRACE(("gethostbyname2(name=%s)\n", idn__debug_xstring(name, 60), af));

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, buf.data, sizeof(buf));
	if (r == idn_success)
		name = buf.data;

	hp = copy_decode_hostent_static(REAL(gethostbyname2)(name, af),
					&he, buf.data, sizeof(buf),
					&h_errno);
	idn_isprocessing = 0;
	return (hp);
}
#endif

#ifdef HAVE_GETHOSTBYADDR
struct hostent *
ENTRY(gethostbyaddr)(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type) {
	static hostbuf_t buf;
	static struct hostent he;
	struct hostent *hp;

	if (idn_isprocessing)
		return (REAL(gethostbyaddr)(addr, len, type));

	TRACE(("gethostbyaddr()\n"));

	idn_isprocessing = 1;
	hp = copy_decode_hostent_static(REAL(gethostbyaddr)(addr, len, type),
					&he, buf.data, sizeof(buf),
					&h_errno);
	idn_isprocessing = 0;
	return (hp);
}
#endif

#ifdef GETHOST_R_GLIBC_FLAVOR

#ifdef HAVE_GETHOSTBYNAME_R
int
ENTRY(gethostbyname_r)(const char *name, struct hostent *result,
		       char *buffer, size_t buflen,
		       struct hostent **rp, int *errp)
{
	char namebuf[IDN_NAME_SIZE];
	char *data;
	size_t datalen;
	idn_result_t r;
	struct hostent he;
	hostbuf_t buf;
	int n;

	if (idn_isprocessing)
		return (REAL(gethostbyname_r)(name, result, buffer,
					      buflen, rp, errp));

	TRACE(("gethostbyname_r(name=%s,buflen=%d)\n",
	       idn__debug_xstring(name, 60), buflen));

	if (buflen <= sizeof(buf)) {
		data = buf.data;
		datalen = sizeof(buf);
	} else {
		data = malloc(buflen);
		datalen = buflen;
		if (data == NULL) {
			*errp = NO_RECOVERY;
			return (ENOMEM);
		}
	}

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, namebuf, sizeof(namebuf));
	if (r == idn_success)
		name = namebuf;

	*errp = 0;
	n = REAL(gethostbyname_r)(name, &he, data, datalen, rp, errp);

	if (n == 0 && *rp != NULL)
		*rp = copy_decode_hostent_static(*rp, result, buffer, buflen,
						 errp);
	idn_isprocessing = 0;

	if (data != buf.data)
		free(data);

	if (*errp != 0)
		n = EINVAL;	/* XXX */

	return (n);
}
#endif

#ifdef HAVE_GETHOSTBYNAME2_R
int
ENTRY(gethostbyname2_r)(const char *name, int af, struct hostent *result,
			char *buffer, size_t buflen,
			struct hostent **rp, int *errp)
{
	char namebuf[IDN_NAME_SIZE];
	char *data;
	size_t datalen;
	idn_result_t r;
	struct hostent he;
	hostbuf_t buf;
	int n;

	if (idn_isprocessing)
		return (REAL(gethostbyname2_r)(name, af, result, buffer,
					       buflen, rp, errp));

	TRACE(("gethostbyname2_r(name=%s,buflen=%d)\n",
	       idn__debug_xstring(name, 60), buflen));

	if (buflen <= sizeof(buf)) {
		data = buf.data;
		datalen = sizeof(buf);
	} else {
		data = malloc(buflen);
		datalen = buflen;
		if (data == NULL) {
			*errp = NO_RECOVERY;
			return (ENOMEM);
		}
	}

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, namebuf, sizeof(namebuf));
	if (r == idn_success)
		name = namebuf;

	n = REAL(gethostbyname2_r)(name, af, &he, data, datalen, rp, errp);

	if (n == 0 && *rp != NULL)
		*rp = copy_decode_hostent_static(*rp, result, buffer, buflen,
						 errp);
	idn_isprocessing = 0;

	if (data != buf.data)
		free(data);

	if (*errp != 0)
		n = EINVAL;	/* XXX */

	return (n);
}
#endif

#ifdef HAVE_GETHOSTBYADDR_R
int
ENTRY(gethostbyaddr_r)(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type,
		       struct hostent *result,
		       char *buffer, size_t buflen,
		       struct hostent **rp, int *errp)
{
	char *data;
	size_t datalen;
	struct hostent he;
	hostbuf_t buf;
	int n;

	if (idn_isprocessing) {
		return (REAL(gethostbyaddr_r)(addr, len, type, result,
					      buffer, buflen, rp, errp));
	}

	TRACE(("gethostbyaddr_r(buflen=%d)\n", buflen));

	if (buflen <= sizeof(buf)) {
		data = buf.data;
		datalen = sizeof(buf);
	} else {
		data = malloc(buflen);
		datalen = buflen;
		if (data == NULL) {
			*errp = NO_RECOVERY;
			return (ENOMEM);
		}
	}

	idn_isprocessing = 1;
	n = REAL(gethostbyaddr_r)(addr, len, type, &he,
				   data, datalen, rp, errp);

	if (n == 0 && *rp != NULL)
		*rp = copy_decode_hostent_static(*rp, result, buffer, buflen,
						 errp);
	idn_isprocessing = 0;

	if (data != buf.data)
		free(data);

	if (*errp != 0)
		n = EINVAL;	/* XXX */

	return (0);
}
#endif

#else /* GETHOST_R_GLIBC_FLAVOR */

#ifdef HAVE_GETHOSTBYNAME_R
struct hostent *
ENTRY(gethostbyname_r)(const char *name, struct hostent *result,
		       char *buffer, int buflen, int *errp)
{
	char namebuf[IDN_NAME_SIZE];
	char *data;
	size_t datalen;
	idn_result_t r;
	struct hostent *hp, he;
	hostbuf_t buf;

	if (idn_isprocessing)
		return (REAL(gethostbyname_r)(name, result, buffer,
					      buflen, errp));

	TRACE(("gethostbyname_r(name=%s,buflen=%d)\n",
	       idn__debug_xstring(name, 60), buflen));

	if (buflen <= sizeof(buf)) {
		data = buf.data;
		datalen = sizeof(buf);
	} else {
		data = malloc(buflen);
		datalen = buflen;
		if (data == NULL) {
			*errp = NO_RECOVERY;
			return (NULL);
		}
	}

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, namebuf, sizeof(namebuf));
	if (r == idn_success)
		name = namebuf;

	hp = REAL(gethostbyname_r)(name, &he, data, datalen, errp);

	if (hp != NULL)
		hp = copy_decode_hostent_static(hp, result, buffer, buflen,
						errp);
	idn_isprocessing = 0;

	if (data != buf.data)
		free(data);

	return (hp);
}
#endif

#ifdef HAVE_GETHOSTBYADDR_R
struct hostent *
ENTRY(gethostbyaddr_r)(GHBA_ADDR_T addr, GHBA_ADDRLEN_T len, int type,
		       struct hostent *result,
		       char *buffer, int buflen, int *errp)
{
	char *data;
	size_t datalen;
	struct hostent *hp, he;
	hostbuf_t buf;

	if (idn_isprocessing) {
		return (REAL(gethostbyaddr_r)(addr, len, type, result,
					      buffer, buflen, errp));
	}

	TRACE(("gethostbyaddr_r(buflen=%d)\n", buflen));

	if (buflen <= sizeof(buf)) {
		data = buf.data;
		datalen = sizeof(buf);
	} else {
		data = malloc(buflen);
		datalen = buflen;
		if (data == NULL) {
			*errp = NO_RECOVERY;
			return (NULL);
		}
	}

	idn_isprocessing = 1;
	hp = REAL(gethostbyaddr_r)(addr, len, type, &he, data, datalen, errp);

	if (hp != NULL)
		hp = copy_decode_hostent_static(hp, result, buffer, buflen,
						errp);
	idn_isprocessing = 0;

	if (data != buf.data)
		free(data);

	return (hp);
}
#endif

#endif /* GETHOST_R_GLIBC_FLAVOR */

#ifdef HAVE_GETIPNODEBYNAME
struct hostent *
ENTRY(getipnodebyname)(const char *name, int af, int flags, int *errp) {
	char namebuf[IDN_NAME_SIZE];
	idn_result_t r;
	struct hostent *hp;

	if (idn_isprocessing)
		return (REAL(getipnodebyname)(name, af, flags, errp));

	TRACE(("getipnodebyname(name=%s)\n", idn__debug_xstring(name, 60), af));

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, name, namebuf, sizeof(namebuf));
	if (r == idn_success)
		name = namebuf;

	hp = REAL(getipnodebyname)(name, af, flags, errp);
	if (hp != NULL) {
		struct hostent *newhp = copy_decode_hostent_dynamic(hp, errp);
		if (newhp != hp) {
			REAL(freehostent)(hp);
			obj_lock(newhp);
			hp = newhp;
		}
	}
	idn_isprocessing = 0;
	return (hp);
}
#endif

#ifdef HAVE_GETIPNODEBYADDR
struct hostent *
ENTRY(getipnodebyaddr)(const void *src, size_t len, int af, int *errp) {
	struct hostent *hp;

	if (idn_isprocessing)
		return (REAL(getipnodebyaddr)(src, len, af, errp));

	TRACE(("getipnodebyaddr()\n"));

	idn_isprocessing = 1;
	hp = REAL(getipnodebyaddr)(src, len, af, errp);
	if (hp != NULL) {
		struct hostent *newhp = copy_decode_hostent_dynamic(hp, errp);
		if (newhp != hp) {
			REAL(freehostent)(hp);
			obj_lock(newhp);
			hp = newhp;
		}
	}
	idn_isprocessing = 0;
	return (hp);
}
#endif

#ifdef HAVE_FREEHOSTENT
void
ENTRY(freehostent)(struct hostent *hp) {
	TRACE(("freehostent(hp=%p)\n", (void *)hp));

	if (obj_islocked(hp)) {
		/*
		 * We allocated the data.
		 */
		obj_unlock(hp);
		free_copied_hostent(hp);
	} else {
		/*
		 * It was allocated the original getipnodeby*().
		 */
		REAL(freehostent)(hp);
	}
}
#endif

#ifdef HAVE_GETADDRINFO
int
ENTRY(getaddrinfo)(const char *nodename, const char *servname,
		   const struct addrinfo *hints, struct addrinfo **res)
{
	char namebuf[IDN_NAME_SIZE];
	idn_result_t r;
	struct addrinfo *aip;
	int err;

	if (nodename == NULL || idn_isprocessing)
		return (REAL(getaddrinfo)(nodename, servname, hints, res));

	TRACE(("getaddrinfo(nodename=%s)\n", idn__debug_xstring(nodename, 60)));

	idn_isprocessing = 1;
	idn_enable(1);
	idn_nameinit(1);
	r = idn_encodename(IDN_ENCODE_APP, nodename,
			   namebuf, sizeof(namebuf));
	if (r == idn_success)
		nodename = namebuf;

	err = REAL(getaddrinfo)(nodename, servname, hints, &aip);
	if (err == 0 && aip != NULL) {
		*res = copy_decode_addrinfo_dynamic(aip);
		if (*res == NULL)
			err = EAI_FAIL;
		else 
			obj_lock(*res);
		if (aip != NULL)
			REAL(freeaddrinfo)(aip);
	}
	idn_isprocessing = 0;
	return (err);
}
#endif

#ifdef HAVE_FREEADDRINFO
void
ENTRY(freeaddrinfo)(struct addrinfo *aip) {
	TRACE(("freeaddrinfo(aip=%p)\n", (void *)aip));

	if (obj_islocked(aip)) {
		/*
		 * We allocated the data.
		 */
		obj_unlock(aip);
		free_copied_addrinfo(aip);
	} else {
		/*
		 * It was allocated the original getaddrinfo().
		 */
		REAL(freeaddrinfo)(aip);
	}
}
#endif

#ifdef HAVE_GETNAMEINFO
int
ENTRY(getnameinfo)(const struct sockaddr *sa, GNI_SALEN_T salen,
		   char *host, GNI_HOSTLEN_T hostlen, char *serv,
		   GNI_SERVLEN_T servlen, GNI_FLAGS_T flags)
{
	char name[IDN_NAME_SIZE];
	size_t namelen = sizeof(name);
	int code;
	idn_result_t r;

	if (host == NULL || hostlen == 0 || idn_isprocessing) {
		return (REAL(getnameinfo)(sa, salen, host, hostlen,
					  serv, servlen, flags));
	}

	TRACE(("getnameinfo(hostlen=%u)\n", hostlen));

	idn_isprocessing = 1;
	code = REAL(getnameinfo)(sa, salen, name, namelen,
				 serv, servlen, flags);
	if (code == 0 && name[0] != '\0') {
		idn_enable(1);
		idn_nameinit(1);
		r = idn_decodename(IDN_DECODE_APP, name, host, hostlen);
		switch (r) {
		case idn_success:
			code = 0;
			break;
		case idn_buffer_overflow:
		case idn_nomemory:
			code = EAI_MEMORY;
			break;
		default:
			code = EAI_FAIL;
			break;
		}
	}
	idn_isprocessing = 0;
	return (code);
}
#endif

/*
 * dllfunc.c - wrapper functions
 */

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
 * All rights reserved.
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

#include <windows.h>
#include <svcguid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include "dlldef.h"

#ifndef EAI_MEMORY
#define EAI_MEMORY	WSA_NOT_ENOUGH_MEMORY
#endif
#ifndef EAI_FAIL
#define EAI_FAIL	WSANO_RECOVERY
#endif

static GUID guid_habn = SVCID_INET_HOSTADDRBYNAME;
static GUID guid_habis = SVCID_INET_HOSTADDRBYINETSTRING;

#define SVCID_IS_HABN(p) (memcmp(p, &guid_habn, sizeof(GUID)) == 0)
#define SVCID_IS_HABIS(p) (memcmp(p, &guid_habis, sizeof(GUID)) == 0)

/*
 * Rename addrinfo to my_addrinfo for avoiding possible name conflict.
 */
struct my_addrinfo {
	int     ai_flags;
	int     ai_family;
	int     ai_socktype;
	int     ai_protocol;
	size_t  ai_addrlen;
	char   *ai_canonname;
	struct sockaddr  *ai_addr;
	struct my_addrinfo  *ai_next;
};

typedef struct obj_lock {
	void *key;
	struct obj_lock *next;
} obj_lock_t;

#define OBJLOCKHASH_SIZE	127
static obj_lock_t *obj_lock_hash[OBJLOCKHASH_SIZE];

static int	obj_hash(void *key);
static int	obj_islocked(void *key);
static void	obj_lock(void *key);
static void	obj_unlock(void *key);
static char	*decode_name_dynamic(const char *name, idn_resconf_t idnctx);
static struct my_addrinfo
		*copy_decode_addrinfo_dynamic(struct my_addrinfo *aip,
					      idn_resconf_t idnctx);
static void	free_copied_addrinfo(struct my_addrinfo *aip);

WRAPPER_EXPORT int WSAAPI
gethostname(char FAR * name, int namelen) {
	int ret;
    
	TRACE("ENTER gethostname\n");
	ret = _org_gethostname(name, namelen);
	TRACE("LEAVE gethostname %d <%-.100s>\n", ret, name);

	return (ret);
}

WRAPPER_EXPORT struct hostent FAR * WSAAPI
gethostbyname(const char FAR * name) {
	struct hostent FAR *ret;
	char    nbuff[256];
	char    hbuff[256];
	BOOL    stat;
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER gethostbyname <%-.100s>\n",
	      (name != NULL ? name : "NULL"));
    
	encodeCtx = idnGetContext();

	if (encodeCtx == NULL || name == NULL) {
		ret = _org_gethostbyname(name);
	} else {
		stat = idnConvReq(encodeCtx, name, nbuff, sizeof(nbuff));
		if (stat == FALSE) {
			TRACE("idnConvReq failed\n");
			ret = NULL;
		} else {
			TRACE("Converted Name <%s>\n",
			      dumpName(nbuff, hbuff, sizeof(hbuff)));
			ret = _org_gethostbyname(nbuff);
		}
	}

	if (ret != NULL && encodeCtx != NULL) {
		TRACE("Resulting Name <%s>\n",
		      dumpName(ret->h_name, hbuff, sizeof(hbuff)));
		stat = idnConvRsp(encodeCtx, ret->h_name,
				  nbuff, sizeof(nbuff));
		if (stat == FALSE) {
			TRACE("Decoding failed - return the name verbatim\n");
		} else {
			TRACE("Converted Back <%s>\n",
			      dumpName(nbuff, hbuff, sizeof(hbuff)));
			strcpy(ret->h_name, nbuff);
		}
	}

	if (ret == NULL) {
		TRACE("LEAVE gethostbyname NULL\n");
	} else {
		TRACE("LEAVE gethostbyname <%s>\n",
		      dumpHost(ret, hbuff, sizeof(hbuff)));
	}
	return (ret);
}

WRAPPER_EXPORT struct hostent FAR * WSAAPI
gethostbyaddr(const char FAR * addr, int len, int type) {
	struct hostent FAR *ret;
	char    nbuff[256];
	char    abuff[256];
	char    hbuff[256];
	BOOL    stat;
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER gethostbyaddr <%s>\n",
	      dumpAddr(addr, len, abuff, sizeof(abuff)));

	encodeCtx = idnGetContext();

	ret = _org_gethostbyaddr(addr, len, type);

	if (ret != NULL && encodeCtx != NULL) {
		TRACE("Resulting Name <%s>\n",
		      dumpName(ret->h_name, hbuff, sizeof(hbuff)));
		stat = idnConvRsp(encodeCtx, ret->h_name,
				  nbuff, sizeof(nbuff));
		if (stat == FALSE) {
			TRACE("Decoding failed - return the name verbatim\n");
		} else {
			TRACE("Converted Back <%s>\n",
			      dumpName(nbuff, hbuff, sizeof(hbuff)));
			strcpy(ret->h_name, nbuff);
		}
	}
    
	if (ret == NULL) {
		TRACE("LEAVE gethostbyaddr NULL\n");
	} else {
		TRACE("LEAVE gethostbyaddr <%s>\n",
		      dumpHost(ret, hbuff, sizeof(hbuff)));
	}    
	return (ret);
}

WRAPPER_EXPORT HANDLE WSAAPI
WSAAsyncGetHostByName(HWND hWnd, u_int wMsg, 
		      const char FAR * name, char FAR * buf, int buflen)
{
	HANDLE  ret;
	char    nbuff[256];
	char    hbuff[256];
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER WSAAsyncGetHostByName <%-.100s>\n", name);

	encodeCtx = idnGetContext();

	if (encodeCtx == NULL || name == NULL) {
		ret = _org_WSAAsyncGetHostByName(hWnd, wMsg,
						 name, buf, buflen);
	} else {
		idnHook(hWnd, wMsg, buf, encodeCtx);
		idnConvReq(encodeCtx, name, nbuff, sizeof(nbuff));
		TRACE("Converted Name <%s>\n",
		      dumpName(nbuff, hbuff, sizeof(hbuff)));
		ret = _org_WSAAsyncGetHostByName(hWnd, wMsg, nbuff,
						 buf, buflen);
	}

	TRACE("LEAVE WSAAsyncGetHostByName HANDLE %08x\n", ret);
    
	return (ret);
}

WRAPPER_EXPORT HANDLE WSAAPI
WSAAsyncGetHostByAddr(HWND hWnd, u_int wMsg, const char FAR * addr,
		      int len, int type, char FAR * buf, int buflen)
{
	HANDLE  ret;
	char    abuff[256];
	idn_resconf_t	encodeCtx;
    
	encodeCtx = idnGetContext();

	if (encodeCtx != NULL) {
		idnHook(hWnd, wMsg, buf, encodeCtx);
	}
    
	TRACE("ENTER WSAAsyncGetHostByAddr <%s>\n",
	      dumpAddr(addr, len, abuff, sizeof(abuff)));
	ret = _org_WSAAsyncGetHostByAddr(hWnd, wMsg, addr, len, type,
					 buf, buflen);
	TRACE("LEAVE WSAAsyncGetHostByAddr HANDLE %08x\n", ret);

	return (ret);
}

WRAPPER_EXPORT INT WSAAPI
WSALookupServiceBeginA(LPWSAQUERYSETA lpqsRestrictions, 
		       DWORD dwControlFlags, LPHANDLE lphLookup)
{
	INT     ret;
	char    nbuff[256];
	char    hbuff[256];
	LPSTR   name = lpqsRestrictions->lpszServiceInstanceName;
	LPGUID  class = lpqsRestrictions->lpServiceClassId;
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER WSALookupServiceBeginA <%-.100s>\n",
	      name == NULL ? "<NULL>" : name);

	encodeCtx = idnGetContext();

	if (name != NULL && encodeCtx != NULL && SVCID_IS_HABN(class) == 0) {
		idnConvReq(encodeCtx, name, nbuff, sizeof(nbuff));
		TRACE("Converted Name <%s>\n",
		      dumpName(nbuff, hbuff, sizeof(hbuff)));
		/* strcpy(lpqsRestrictions->lpszQueryString, nbuff); */
		lpqsRestrictions->lpszServiceInstanceName = nbuff;
	}
	ret = _org_WSALookupServiceBeginA(lpqsRestrictions,
					  dwControlFlags, lphLookup);
	TRACE("LEAVE WSALookupServiceBeginA %d\n", ret);

	return (ret);
}

WRAPPER_EXPORT INT WSAAPI
WSALookupServiceNextA(HANDLE hLookup, DWORD dwControlFlags, 
		      LPDWORD lpdwBufferLength, LPWSAQUERYSETA lpqsResults)
{
	INT     ret;
	char    nbuff[256];
	char    hbuff[256];
	LPGUID  class;
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER WSALookupServiceNextA\n");

	encodeCtx = idnGetContext();

	ret = _org_WSALookupServiceNextA(hLookup, dwControlFlags,
					 lpdwBufferLength, lpqsResults);
	class = lpqsResults->lpServiceClassId;

	if (ret == 0 &&
	    encodeCtx != NULL &&
	    (dwControlFlags & LUP_RETURN_NAME) &&
	    (SVCID_IS_HABN(class) || SVCID_IS_HABIS(class))) {
		TRACE("Resulting Name <%s>\n",
		      dumpName(lpqsResults->lpszServiceInstanceName,
			       hbuff, sizeof(hbuff)));
		if (idnConvRsp(encodeCtx, 
			       lpqsResults->lpszServiceInstanceName,
			       nbuff, sizeof(nbuff)) == FALSE) {
			TRACE("Decoding failed - return the name verbatim\n");
		} else {
			TRACE("Converted Back <%s>\n",
			      dumpName(nbuff, hbuff, sizeof(hbuff)));
			strcpy(lpqsResults->lpszServiceInstanceName, nbuff);
		}
	}
	TRACE("LEAVE WSALookupServiceNextA %d <%s>\n", ret, nbuff);

	return (ret);
}         

WRAPPER_EXPORT INT WSAAPI
WSALookupServiceBeginW(LPWSAQUERYSETW lpqsRestrictions,
		       DWORD dwControlFlags, LPHANDLE lphLookup)
{
	INT     ret;
    
	TRACE("ENTER WSALookupServiceBeginW\n");
	ret = _org_WSALookupServiceBeginW(lpqsRestrictions,
					  dwControlFlags,lphLookup);
	TRACE("LEAVE WSALookupServiceBeginW %d\n", ret);

	return (ret);
}

WRAPPER_EXPORT INT WSAAPI
WSALookupServiceNextW(HANDLE hLookup, DWORD dwControlFlags,
		      LPDWORD lpdwBufferLength, LPWSAQUERYSETW lpqsResults)
{
	INT     ret;
    
	TRACE("ENTER WSALookupServiceNextW\n");
	ret = _org_WSALookupServiceNextW(hLookup, dwControlFlags,
					 lpdwBufferLength, lpqsResults);
	TRACE("LEAVE WSALookupServiceNextW %d\n", ret);

	return (ret);
}         

WRAPPER_EXPORT INT WSAAPI
WSALookupServiceEnd(HANDLE  hLookup) {
	INT     ret;
    
	TRACE("ENTER WSALookupServiceEnd\n");
	ret = _org_WSALookupServiceEnd(hLookup);
	TRACE("LEAVE WSALookupServiceEnd %d\n", ret);

	return (ret);
}

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

static char *
decode_name_dynamic(const char *name, idn_resconf_t idnctx) {
	BOOL stat;
	char buf[256], tmp[256];
	char *s;

	if (idnConvRsp(idnctx, name, buf, sizeof(buf)) == TRUE) {
		TRACE("Converted Back <%s>\n",
		      dumpName(buf, tmp, sizeof(tmp)));
		name = buf;
	} else {
		TRACE("Decoding failed - return the name verbatim\n");
	}
	s = malloc(strlen(name) + 1);
	if (s == NULL)
		return (NULL);
	else
		return (strcpy(s, name));
}
		
static struct my_addrinfo *
copy_decode_addrinfo_dynamic(struct my_addrinfo *aip, idn_resconf_t idnctx)
{
	struct my_addrinfo *newaip;

	if (aip == NULL)
		return (NULL);

	newaip = malloc(sizeof(struct my_addrinfo) + aip->ai_addrlen);
	if (newaip == NULL)
		return (NULL);

	*newaip = *aip;
	newaip->ai_addr = (struct sockaddr *)(newaip + 1);
	memcpy(newaip->ai_addr, aip->ai_addr, aip->ai_addrlen);

	if (newaip->ai_canonname != NULL)
		newaip->ai_canonname = decode_name_dynamic(aip->ai_canonname,
							   idnctx);

	newaip->ai_next = copy_decode_addrinfo_dynamic(aip->ai_next, idnctx);
	return (newaip);
}

static void
free_copied_addrinfo(struct my_addrinfo *aip) {
	while (aip != NULL) {
		struct my_addrinfo *next = aip->ai_next;

		if (aip->ai_canonname != NULL)
			free(aip->ai_canonname);
		free(aip);
		aip = next;
	}
}

WRAPPER_EXPORT int WSAAPI
getaddrinfo(const char *nodename, const char *servname,
	    const struct my_addrinfo *hints, struct my_addrinfo **res)
{
	char namebuf[256];
	BOOL stat;
	struct my_addrinfo *aip;
	int err;
	idn_resconf_t	encodeCtx;

	TRACE("ENTER getaddrinfo <%-.100s>\n", nodename ? nodename : "NULL");

	encodeCtx = idnGetContext();

	if (nodename == NULL || encodeCtx == NULL) {
		TRACE("conversion unnecessary\n");
		err = _org_getaddrinfo(nodename, servname, hints, res);
	} else {
		stat = idnConvReq(encodeCtx, nodename,
				  namebuf, sizeof(namebuf));
		if (stat == TRUE) {
			nodename = namebuf;
			TRACE("Converted Name <%-.100s>\n", namebuf);
		}

		err = _org_getaddrinfo(nodename, servname, hints, &aip);
		if (err == 0 && aip != NULL) {
			*res = copy_decode_addrinfo_dynamic(aip, encodeCtx);
			if (*res == NULL)
				err = EAI_FAIL;
			else 
				obj_lock(*res);
			if (aip != NULL)
				_org_freeaddrinfo(aip);
		}
	}

	TRACE("LEAVE getaddrinfo %d\n", err);
	return (err);
}

WRAPPER_EXPORT void WSAAPI
freeaddrinfo(struct my_addrinfo *aip) {
	TRACE("ENTER freeaddrinfo aip=%p\n", (void *)aip);

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
		TRACE("Not allocated by the wrapper\n");
		_org_freeaddrinfo(aip);
	}
	TRACE("LEAVE freeaddrinfo\n");
}

WRAPPER_EXPORT int WSAAPI
getnameinfo(const struct sockaddr *sa, DWORD salen,
	    char *host, DWORD hostlen, char *serv,
	    DWORD servlen, int flags)
{
	char name[256];
	size_t namelen = sizeof(name);
	int code;
	BOOL stat;
	idn_resconf_t	encodeCtx;

	TRACE("ENTER getnameinfo\n");

	encodeCtx = idnGetContext();

	if (host == NULL || hostlen == 0 || encodeCtx == NULL) {
		TRACE("conversion unnecessary\n");
		code = _org_getnameinfo(sa, salen, host, hostlen,
					serv, servlen, flags);
	} else {
		code = _org_getnameinfo(sa, salen, name, namelen,
					serv, servlen, flags);
		if (code == 0 && name[0] != '\0') {
			stat = idnConvRsp(encodeCtx, name, host, hostlen);
			if (stat == FALSE) {
				TRACE("Decoding failed - return the name verbatim\n");
				if (strlen(name) >= hostlen) {
					code = EAI_FAIL;
				} else {
					strcpy(host, name);
				}
			} else {
				TRACE("Converted Back <%s>\n",
				      dumpName(host, name, sizeof(name)));
			}
		}
	}

	TRACE("LEAVE getnameinfo %d\n", code);
	return (code);
}

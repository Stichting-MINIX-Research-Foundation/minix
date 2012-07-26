/*
 * dllfunc.c - wrapper functions
 */

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include "dlldef.h"

WRAPPER_EXPORT int PASCAL FAR
gethostname(char FAR * name, int namelen) {
	int ret;
    
	TRACE("ENTER gethostname\n");
	ret = _org_gethostname(name, namelen);
	TRACE("LEAVE gethostname %d <%-.100s>\n", ret, name);

	return (ret);
}

WRAPPER_EXPORT struct hostent FAR * PASCAL FAR
gethostbyname(const char FAR * name) {
	struct hostent FAR *ret;
	char    nbuff[256];
	char    hbuff[256];
	BOOL    stat;
	idn_resconf_t	encodeCtx;
    
	TRACE("ENTER gethostbyname <%-.100s>\n",
	      (name != NULL ? name : "NULL"));

	encodeCtx = idnGetContext();

	if (encodeCtx == NULL) {
		TRACE("gethostbyname: not encode here\n");
		ret = _org_gethostbyname(name);
	} else if (name == NULL) {
		TRACE("gethostbyname: name is NULL\n");
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
		stat = idnConvRsp(encodeCtx, ret->h_name, nbuff,
				  sizeof(nbuff));
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

WRAPPER_EXPORT struct hostent FAR * PASCAL FAR
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
		TRACE("LEAVE gethostbyaddr NULL\n") ;
	} else {
		TRACE("LEAVE gethostbyaddr <%s>\n",
		      dumpHost(ret, hbuff, sizeof(hbuff)));
	}    
	return (ret);
}

WRAPPER_EXPORT HANDLE PASCAL FAR
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
		ret = _org_WSAAsyncGetHostByName(hWnd, wMsg, name,
						 buf, buflen);
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

WRAPPER_EXPORT HANDLE PASCAL FAR
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



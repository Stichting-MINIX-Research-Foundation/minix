/*
 * dlldef.h
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

#ifndef _DLLDEF_H
#define _DLLDEF_H

#include "../common/wrapcommon.h"

/*
 * Execution Tracing
 */
 
extern  int     procPid;

#ifdef  DEBUG
#define TRACE   idnPrintf
#define FATAL   idnPrintf
#else
#define TRACE
#define FATAL   idnPrintf
#endif

/*
 * entry points to wrap
 */

extern  int WSAAPI
_org_gethostname(char FAR * name, int namelen);

extern  struct hostent FAR * WSAAPI
_org_gethostbyaddr(const char FAR * addr, int len, int type);

extern  struct hostent FAR * WSAAPI
_org_gethostbyname(const char FAR * name);

extern  HANDLE WSAAPI
_org_WSAAsyncGetHostByName(HWND hWnd, u_int wMsg, const char FAR * name,
			   char FAR * buf,int buflen);

extern  HANDLE WSAAPI
_org_WSAAsyncGetHostByAddr(HWND hWnd, u_int wMsg, const char FAR * addr,
			   int len, int type, char FAR * buf, int buflen);

extern  INT WSAAPI
_org_WSALookupServiceBeginA(LPWSAQUERYSETA lpqsRestrictions,
			    DWORD          dwControlFlags,
			    LPHANDLE       lphLookup);

extern  INT WSAAPI
_org_WSALookupServiceBeginW(LPWSAQUERYSETW lpqsRestrictions,
			    DWORD          dwControlFlags,
			    LPHANDLE       lphLookup);

extern  INT WSAAPI
_org_WSALookupServiceNextA(HANDLE           hLookup,
			   DWORD            dwControlFlags,
			   LPDWORD          lpdwBufferLength,
			   LPWSAQUERYSETA   lpqsResults);

extern  INT WSAAPI
_org_WSALookupServiceNextW(HANDLE           hLookup,
			   DWORD            dwControlFlags,
			   LPDWORD          lpdwBufferLength,
			   LPWSAQUERYSETW   lpqsResults);

extern  INT WSAAPI
_org_WSALookupServiceEnd(HANDLE  hLookup);

extern int WSAAPI
_org_getaddrinfo(const char *nodename, const char *servname,
		 LPVOID hints, LPVOID res);

extern void WSAAPI
_org_freeaddrinfo(LPVOID aip);

extern int WSAAPI
_org_getnameinfo(LPVOID sa, DWORD salen, char* host, DWORD hostlen,
		 char* serv, DWORD servlen, int flags);

#endif  /* _DLLDEF_H */

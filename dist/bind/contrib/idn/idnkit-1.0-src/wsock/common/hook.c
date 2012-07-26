/*
 * hook.c - Hooking Asynchronous Completion
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrapcommon.h"

/*
 * Hook Managements
 */

static  HHOOK   hookHandle = NULL ;

typedef struct _HOOK    *HOOKPTR;

typedef struct _HOOK {
	HOOKPTR     prev;
	HOOKPTR     next;
	idn_resconf_t ctx;
	HWND        hWnd;
	u_int       wMsg;
	char FAR    *pBuf;
} HOOKREC;

static  HOOKREC hookList = { 0 } ;

static void
hookListInit(void) {
	if (hookList.prev == NULL || hookList.next == NULL) {
		hookList.prev = &hookList;
		hookList.next = &hookList;
	}
}

static HOOKPTR
hookListSearch(HWND hWnd, u_int wMsg) {
	HOOKPTR hp;
    
	for (hp = hookList.next ; hp != &hookList ; hp = hp->next) {
		if (hp->hWnd == hWnd && hp->wMsg == wMsg) {
			return (hp);
		}
	}
	return (NULL);
}

static BOOL
hookListAppend(HWND hWnd, u_int wMsg, char FAR *buf, idn_resconf_t ctx) {
	HOOKPTR hp, prev, next;
    
	if ((hp = (HOOKPTR)malloc(sizeof(HOOKREC))) == NULL) {
		idnPrintf("cannot create hook record\n");
		return (FALSE);
	}
	memset(hp, 0, sizeof(*hp));
    
	hp->ctx = ctx;
	hp->hWnd = hWnd;
	hp->wMsg = wMsg;
	hp->pBuf = buf;
    
	prev = hookList.prev;
	next = prev->next;
	prev->next = hp;
	next->prev = hp;
	hp->next = next;
	hp->prev = prev;    

	return (TRUE);
}

static void
hookListDelete(HOOKPTR hp)
{
	HOOKPTR prev, next;
    
	prev = hp->prev;
	next = hp->next;
	prev->next = next;
	next->prev = prev;
    
	free(hp);
}

static void
hookListDone(void)
{
	HOOKPTR hp;
    
	while ((hp = hookList.next) != &hookList) {
		hookListDelete(hp);
	}
}

/*
 * idnHookInit - initialize Hook Management
 */
void
idnHookInit(void) {
	hookListInit();
}

/*
 * idnHookDone - finalize Hook Management
 */
void
idnHookDone(void) {
	if (hookHandle != NULL) {
		UnhookWindowsHookEx(hookHandle);
		hookHandle = NULL;
	}
	hookListDone();
}

/*
 * hookProc - hookprocedure, used as WH_GETMESSAGE hook
 */
LRESULT CALLBACK
hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
	MSG             *pMsg;
	HOOKPTR         pHook;
	struct  hostent *pHost;
	char            nbuff[256];
	char            hbuff[256];
    
	if (nCode < 0) {
		return (CallNextHookEx(hookHandle, nCode, wParam, lParam));
	} else if (nCode != HC_ACTION) {
		return (0);
	}
	if ((pMsg = (MSG *)lParam) == NULL) {
		return (0);
	}
	if ((pHook = hookListSearch(pMsg->hwnd, pMsg->message)) == NULL) {
		return (0);
	}
    
	/*
	 * Convert the Host Name
	 */
	pHost = (struct hostent *)pHook->pBuf;
	idnPrintf("AsyncComplete Resulting <%s>\n",
		  dumpName(pHost->h_name, hbuff, sizeof(hbuff)));
	if (idnConvRsp(pHook->ctx, pHost->h_name,
		       nbuff, sizeof(nbuff)) == TRUE) {
		idnPrintf("AsyncComplete Converted <%s>\n",
			  dumpName(nbuff, hbuff, sizeof(hbuff)));
		strcpy(pHost->h_name, nbuff);
	}

	/*
	 * Delete target
	 */
	hookListDelete(pHook);

	return (0);
}

/*
 * idnHook - hook async. completion message
 */
BOOL
idnHook(HWND hWnd, u_int wMsg, char FAR *buf, idn_resconf_t ctx)
{
	if (hookHandle == NULL) {
		hookHandle = SetWindowsHookEx(WH_GETMESSAGE, hookProc,
					      NULL, GetCurrentThreadId());
	}
	if (hookHandle == NULL) {
		idnPrintf("idnHook: cannot set hook\n");
		return (FALSE);
	}
	if (hookListAppend(hWnd, wMsg, buf, ctx) != TRUE) {
		return (FALSE);
	}
	return (TRUE);
}

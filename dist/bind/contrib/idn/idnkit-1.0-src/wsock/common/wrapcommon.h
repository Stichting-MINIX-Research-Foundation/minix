/*
 * wrapcommon.h
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

#ifndef _WRAPCOMMON_H
#define _WRAPCOMMON_H

#include <idn/res.h>
#include <idn/log.h>
#include <idn/version.h>

#define WRAPPER_EXPORT	extern __declspec(dllexport)

extern void idnPrintf(char *fmt, ...);
extern void idnLogPrintf(int level, char *fmt, ...);
extern void idnLogInit(const char *title);
extern void idnLogReset(void);
extern void idnLogFinish(void);

extern char *dumpAddr(const char FAR *addr, int len, char *buff, size_t size);
extern char *dumpHost(const struct hostent FAR *hp, char *buff, size_t size);
extern char *dumpName(const char *name, char *buff, size_t size);

extern int idnEncodeWhere(void);

#define IDN_ENCODE_ALWAYS   0
#define IDN_ENCODE_CHECK    1
#define IDN_ENCODE_ONLY11   2
#define IDN_ENCODE_ONLY20   3

extern BOOL idnGetPrgEncoding(char *enc, size_t len);
extern BOOL idnGetLogFile(char *file, size_t len);

extern int  idnGetLogLevel(void);  /* 0 : fatal        */
                                    /* 1 : error        */
				    /* 2 : warning      */
				    /* 3 : info         */
				    /* 4 : trace        */
				    /* 5 : dump         */
extern int  idnGetInstallDir(char *dir, size_t len);

extern idn_resconf_t idnConvInit(void);
extern void idnConvDone(idn_resconf_t ctx);

extern BOOL idnWinsockVersion(const char *version);
extern HINSTANCE idnWinsockHandle(void);
extern idn_resconf_t idnGetContext(void);

/*
 * Converting Request/Response
 */

extern BOOL idnConvReq(idn_resconf_t ctx, const char FAR *from,
		       char FAR *to, size_t tolen);
extern BOOL idnConvRsp(idn_resconf_t ctx, const char FAR *from,
		       char FAR *to, size_t tolen);

/*
 * Hook for Asynchronouse Query
 */

extern void idnHookInit(void);
extern void idnHookDone(void);
extern BOOL idnHook(HWND hWnd, u_int wMsg, char FAR *buf, idn_resconf_t ctx);

#endif  /* _WRAPCOMMON_H */

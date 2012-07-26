#ifndef lint
static char *rcsid = "$Id: localencoding.c,v 1.1.1.1 2003-06-04 00:25:53 marka Exp $";
#endif

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

#include <config.h>

#ifdef WIN32
#include <windows.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include <idn/logmacro.h>
#include <idn/localencoding.h>
#include <idn/debug.h>

#ifdef ENABLE_MDNKIT_COMPAT
#include <mdn/localencoding.h>
#endif

const char *
idn_localencoding_name(void) {
	char *name;

	TRACE(("idn_localencoding_name()\n"));

	if ((name = getenv(IDN_LOCALCS_ENV)) != NULL) {
		TRACE(("local encoding=\"%-.30s\"\n",
		      name == NULL ? "<null>" : name));
		return (name);
	}
#ifdef ENABLE_MDNKIT_COMPAT
	if ((name = getenv(MDN_LOCALCS_ENV)) != NULL) {
		TRACE(("local encoding=\"%-.30s\"\n",
		      name == NULL ? "<null>" : name));
		return (name);
	}
#endif

#ifdef WIN32
	{
		static char cp_str[40];	/* enough */
		(void)sprintf(cp_str, "CP%u", GetACP());
		TRACE(("local encoding(codepage)=\"%-.30s\"\n", cp_str));
		return (cp_str);
	}
#else /* WIN32 */
#ifdef HAVE_LIBCHARSET
	name = locale_charset();
	TRACE(("local encoding=\"%-.30s\"\n",
	       name == NULL ? "<null>" : name));
	return (name);
#endif

#if defined(HAVE_NL_LANGINFO) && defined(CODESET)
	if ((name = nl_langinfo(CODESET)) != NULL) {
		TRACE(("local encoding=\"%-.30s\"\n",
		       name == NULL ? "<null>" : name));
		return (name);
	}
#endif
	(void)(
#ifdef HAVE_SETLOCALE
		(name = setlocale(LC_CTYPE, NULL)) ||
#endif
		(name = getenv("LC_ALL")) ||
		(name = getenv("LC_CTYPE")) ||
		(name = getenv("LANG")));
	TRACE(("local encoding=\"%-.30s\"\n", name == NULL ? "<null>" : name));
	return (name);
#endif /* WIN32 */
}

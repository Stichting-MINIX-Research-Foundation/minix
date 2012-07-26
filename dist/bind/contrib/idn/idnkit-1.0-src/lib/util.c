#ifndef lint
static char *rcsid = "$Id: util.c,v 1.1.1.1 2003-06-04 00:26:45 marka Exp $";
#endif

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

#include <config.h>

#ifdef WIN32
#include <windows.h>
#undef ERROR
#endif

#include <stddef.h>

#include <idn/assert.h>
#include <idn/result.h>
#include <idn/logmacro.h>
#include <idn/util.h>
#include <idn/ucs4.h>

#ifdef WIN32
#define IDNKEY_IDNKIT		"Software\\JPNIC\\IDN"
#endif

/*
 * ASCII ctype macros.
 * Note that these macros evaluate the argument multiple times.  Be careful.
 */
#define ASCII_ISDIGIT(c) \
	('0' <= (c) && (c) <= '9')
#define ASCII_ISUPPER(c) \
	('A' <= (c) && (c) <= 'Z')
#define ASCII_ISLOWER(c) \
	('a' <= (c) && (c) <= 'z')
#define ASCII_ISALPHA(c) \
	(ASCII_ISUPPER(c) || ASCII_ISLOWER(c))
#define ASCII_ISALNUM(c) \
	(ASCII_ISDIGIT(c) || ASCII_ISUPPER(c) || ASCII_ISLOWER(c))

#define ASCII_TOUPPER(c) \
	(('a' <= (c) && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))
#define ASCII_TOLOWER(c) \
	(('A' <= (c) && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))

int
idn__util_asciihaveaceprefix(const char *str, const char *prefix) {
	assert(str != NULL && prefix != NULL);

	while (*prefix != '\0') {
		if (ASCII_TOLOWER(*str) != ASCII_TOLOWER(*prefix))
			return 0;
		str++;
		prefix++;
	}

	return (1);
}

int
idn__util_ucs4haveaceprefix(const unsigned long *str, const char *prefix) {
	assert(str != NULL && prefix != NULL);

	while (*prefix != '\0') {
		if (ASCII_TOLOWER(*str) != ASCII_TOLOWER(*prefix))
			return 0;
		str++;
		prefix++;
	}

	return (1);
}

int
idn__util_ucs4isasciirange(const unsigned long *str) {
	while (*str != '\0') {
		if (*str > 0x7f)
			return (0);
		str++;
	}

	return (1);
}

#ifdef WIN32
int
idn__util_getregistrystring(idn__util_hkey_t topkey, const char *name,
			    char *str, size_t length)
{
	HKEY top;
	LONG stat;
	HKEY hk;
	DWORD len, type;

	assert((topkey == idn__util_hkey_currentuser ||
		topkey == idn__util_hkey_localmachine) &&
	       name != NULL && str != NULL);

	if (topkey == idn__util_hkey_currentuser) {
		top= HKEY_CURRENT_USER;
	} else {	/* idn__util_hkey_localmachine */
		top = HKEY_LOCAL_MACHINE;
	}

	stat = RegOpenKeyEx(top, IDNKEY_IDNKIT, 0, KEY_READ, &hk);
	if (stat != ERROR_SUCCESS) {
		return (0);
	}

	len = (DWORD)length;
	stat = RegQueryValueEx(hk, (LPCTSTR)name, NULL,
			       &type, (LPBYTE)str, &len);
	RegCloseKey(hk);

	if (stat != ERROR_SUCCESS || type != REG_SZ) {
		return (0);
	}

	return (1);
}
#endif /* WIN32 */

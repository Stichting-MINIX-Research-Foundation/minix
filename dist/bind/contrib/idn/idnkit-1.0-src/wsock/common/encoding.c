/*
 * encoding.c - get DNS/Local encodings
 *
 *      Software\JPNIC\IDN\Where
 *                        \LogFile
 *			  \LogLevel
 *			  \InstallDir   
 *                        \PerProg\<name>\Where
 *                        \PerProg\<name>\Encoding
 */

/*
 * Copyright (c) 2000,2001,2002 Japan Network Information Center.
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
#include <ctype.h>

#include "wrapcommon.h"

#define IDN_GLOBAL	1
#define IDN_PERPROG	2
#define IDN_CURUSER	4

/*
 * Registry of Encodings
 */

#define	IDNKEY_WRAPPER	"Software\\JPNIC\\IDN"
#define	IDNKEY_PERPROG	"Software\\JPNIC\\IDN\\PerProg"
#define	IDNVAL_WHERE	"Where"
#define	IDNVAL_ENCODE	"Encoding"
#define	IDNVAL_LOGLVL	"LogLevel"
#define	IDNVAL_LOGFILE	"LogFile"
#define IDNVAL_INSDIR	"InstallDir"

static int	GetRegistry(HKEY top, const char *key, const char *name,
			    DWORD type, void *param, DWORD length);
static char	*GetPerProgKey(char *buf, size_t len);
static int	GetFromRegistry(const char *name, int where, DWORD type,
				void *param, DWORD length);
static int	GetIntFromRegistry(const char *name, int defvalue, int where);
static BOOL	GetStringFromRegistry(const char *name, char *result,
				      size_t length, int where);

static int
GetRegistry(HKEY top, const char *key, const char *name, DWORD type,
	    void *param, DWORD length)
{
	LONG stat;
	HKEY hk;
	DWORD realtype;

	stat = RegOpenKeyEx(top, key, 0, KEY_READ, &hk);
	if (stat != ERROR_SUCCESS) {
		return 0;
	}

	stat = RegQueryValueEx(hk, (LPCTSTR)name, NULL,
			       &realtype, (LPBYTE)param, &length);

	RegCloseKey(hk);

	if (stat != ERROR_SUCCESS || realtype != type)
		return 0;

	return 1;
}

static char *
GetPerProgKey(char *buf, size_t len)
{
	char exename[256];
	char prgname[256];
	char *p, *last;

	GetModuleFileName(NULL, exename, 256);
     
	for (p = exename, last = NULL; *p != '\0'; p++) {
		if (*p == '/' || *p == '\\') {
			last = p;
		}
	}
	strcpy(prgname, (last == NULL) ? exename : (last + 1));
	if ((p = strrchr(prgname, '.')) != NULL) {
		*p = '\0';
	}

	if (strlen(IDNKEY_PERPROG) + 1 + strlen(prgname) >= len) {
		return (NULL);
	}
	sprintf(buf, "%s\\%s", IDNKEY_PERPROG, prgname);
	return buf;
}

static int
GetFromRegistry(const char *name, int where, DWORD type,
		void *param, DWORD length)
{
	if (where & IDN_PERPROG) {
		/*
		 * First, try program specific setting.
		 */
		char keyname[256];

		/*
		 * Try HKEY_CURRENT_USER and HKEY_LOCAL_MACHINE.
		 */
		if (GetPerProgKey(keyname, sizeof(keyname)) != NULL) {
			if (((where & IDN_CURUSER) &&
			     GetRegistry(HKEY_CURRENT_USER, keyname, name,
					 type, param, length)) ||
			    GetRegistry(HKEY_LOCAL_MACHINE, keyname, name,
					type, param, length)) {
				return (1);
			}
		}
	}

	if (where & IDN_GLOBAL) {
		/*
		 * Try global setting.
		 */
		if (((where & IDN_CURUSER) &&
		     GetRegistry(HKEY_CURRENT_USER, IDNKEY_WRAPPER, name,
				 type, param, length)) ||
		    GetRegistry(HKEY_LOCAL_MACHINE, IDNKEY_WRAPPER, name,
				type, param, length)) {
			return (1);
		}
	}

	/*
	 * Not found.
	 */
	return (0);
}

static int
GetIntFromRegistry(const char *name, int defvalue, int where)
{
    DWORD param;

    if (GetFromRegistry(name, where, REG_DWORD, &param, sizeof(param))) {
	    return ((int)param);
    }
    return (defvalue);
}

static BOOL
GetStringFromRegistry(const char *name, char *result, size_t length, int where)
{
    if (GetFromRegistry(name, where, REG_SZ, result, (DWORD)length)) {
	    return (TRUE);
    }
    return (FALSE);
}

/*
 * idnEncodeWhere - which module should convert domain name
 */
int
idnEncodeWhere(void)
{
	int v = GetIntFromRegistry(IDNVAL_WHERE, IDN_ENCODE_ALWAYS,
				   IDN_GLOBAL|IDN_PERPROG|IDN_CURUSER);

	idnLogPrintf(idn_log_level_trace, "idnEncodeWhere: %d\n", v);
	return (v);
}

/*
 * idnGetLogFile - refer to log file
 */
BOOL
idnGetLogFile(char *file, size_t len)
{
	BOOL v = GetStringFromRegistry(IDNVAL_LOGFILE, file, len,
				       IDN_GLOBAL|IDN_CURUSER);

	idnLogPrintf(idn_log_level_trace, "idnGetLogFile: %-.100s\n",
		     (v == TRUE) ? file : "<none>");
	return (v);
}

/*
 * idnGetPrgEncoding - refer to Program's Local Encoding
 *
 *      use program name as registry key
 */
BOOL
idnGetPrgEncoding(char *enc, size_t len)
{
	if (GetStringFromRegistry(IDNVAL_ENCODE, enc, len,
				  IDN_PERPROG|IDN_CURUSER) != TRUE ||
	    enc[0] == '\0') {
		sprintf(enc, "CP%d", GetACP());
	}
	idnLogPrintf(idn_log_level_trace,
		     "idnGetPrgEncoding: %-.30s\n", enc);
	return (TRUE);
}

/*
 * idnGetLogLevel
 */
int
idnGetLogLevel(void)
{
	int v  = GetIntFromRegistry(IDNVAL_LOGLVL, 0,
				    IDN_GLOBAL|IDN_CURUSER);

	idnLogPrintf(idn_log_level_trace, "idnGetLogLevel: %d\n", v);
	return (v);
}

/*
 * idnGetInstallDir - get idn wrapper install directory
 */
BOOL
idnGetInstallDir(char *dir, size_t len)
{
	/* No need to look at HKEY_CURRENT_USER */
	BOOL v = GetStringFromRegistry(IDNVAL_INSDIR, dir, len, IDN_GLOBAL);

	idnLogPrintf(idn_log_level_trace, "idnGetInstallDir: %-.100s\n",
		     (v == TRUE) ? dir : "<none>");
	return (v);
}

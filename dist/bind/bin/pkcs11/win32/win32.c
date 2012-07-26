/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: win32.c,v 1.5 2009-10-26 23:47:35 tbox Exp $ */

/* $Id */

/*! \file */

/* missing code for WIN32 */

#include <windows.h>
#include <string.h>

#define HAVE_GETPASSPHRASE

char *
getpassphrase(const char *prompt)
{
	static char buf[128];
	HANDLE h;
	DWORD cc, mode;
	int cnt;

	h = GetStdHandle(STD_INPUT_HANDLE);
	fputs(prompt, stderr);
	fflush(stderr);
	fflush(stdout);
	FlushConsoleInputBuffer(h);
	GetConsoleMode(h, &mode);
	SetConsoleMode(h, ENABLE_PROCESSED_INPUT);

	for (cnt = 0; cnt < sizeof(buf) - 1; cnt++)
	{
		ReadFile(h, buf + cnt, 1, &cc, NULL);
		if (buf[cnt] == '\r')
			break;
		fputc('*', stdout);
		fflush(stderr);
		fflush(stdout);
	}

	SetConsoleMode(h, mode);
	buf[cnt] = '\0';
	fputs("\n", stderr);
	return buf;
}

/* From ISC isc_commandline_parse() */

int optind = 1;		/* index into parent argv vector */
int optopt;		/* character checked for validity */
char *optarg;		/* argument associated with option */
static char endopt = '\0';

#define	BADOPT	(int)'?'
#define	BADARG	(int)':'
#define	ENDOPT	&endopt

int
getopt(int nargc, char * const nargv[], const char *ostr)
{
	static char *place = ENDOPT;		/* option letter processing */
	char *option;				/* option letter list index */

	if (*place == '\0') {			/* update scanning pointer */
		place = nargv[optind];
		if (optind >= nargc || *place++ != '-') {
			/* index out of range or points to non-option */
			place = ENDOPT;
			return (-1);
		}
		optopt = *place++;
		if (optopt == '-' && *place == '\0') {
			/* "--" signals end of options */
			++optind;
			place = ENDOPT;
			return (-1);
		}
	} else
		optopt = *place++;

	/* See if option letter is one the caller wanted... */
	if (optopt == ':' || (option = strchr(ostr, optopt)) == NULL) {
		if (*place == '\0')
			++optind;
		return (BADOPT);
	}

	if (*++option != ':') {
		/* option doesn't take an argument */
		optarg = NULL;
		if (*place == '\0')
			++optind;
	} else {
		/* option needs an argument */
		if (*place != '\0')
			/* -D1 style */
			optarg = place;
		else if (nargc > ++optind)
			/* -D 1 style */
			optarg = nargv[optind];
		else {
			/* needed but absent */
			place = ENDOPT;
			if (*ostr == ':')
				return (BADARG);
			return (BADOPT);
		}
		place = ENDOPT;
		++optind;
	}
	return (optopt);
}

/* load PKCS11 DLL */

#ifndef PK11_LIB_LOCATION
#error "PK11_LIB_LOCATION is not defined"
#endif

const char *pk11_libname = PK11_LIB_LOCATION ".dll";

HINSTANCE hPK11 = NULL;

#define C_Initialize isc_C_Initialize

CK_RV
C_Initialize(CK_VOID_PTR pReserved)
{
	CK_C_Initialize sym;

	if (pk11_libname == NULL)
		return 0xfe;
	/* Visual Studio convertion issue... */
	if (*pk11_libname == ' ')
		pk11_libname++;

	hPK11 = LoadLibraryA(pk11_libname);

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Initialize)GetProcAddress(hPK11, "C_Initialize");
	if (sym == NULL)
		return 0xff;
	return (*sym)(pReserved);
}

#define C_Finalize isc_C_Finalize

CK_RV
C_Finalize(CK_VOID_PTR pReserved)
{
	CK_C_Finalize sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Finalize)GetProcAddress(hPK11, "C_Finalize");
	if (sym == NULL)
		return 0xff;
	return (*sym)(pReserved);
}

#define C_OpenSession isc_C_OpenSession

CK_RV
C_OpenSession(CK_SLOT_ID slotID,
	      CK_FLAGS flags,
	      CK_VOID_PTR pApplication,
	      CK_RV  (*Notify) (CK_SESSION_HANDLE hSession,
				CK_NOTIFICATION event,
				CK_VOID_PTR pApplication),
	      CK_SESSION_HANDLE_PTR phSession)
{
	CK_C_OpenSession sym;

	if (hPK11 == NULL)
		hPK11 = LoadLibraryA(pk11_libname);
	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_OpenSession)GetProcAddress(hPK11, "C_OpenSession");
	if (sym == NULL)
		return 0xff;
	return (*sym)(slotID, flags, pApplication, Notify, phSession);
}

#define C_CloseSession isc_C_CloseSession

CK_RV
C_CloseSession(CK_SESSION_HANDLE hSession)
{
	CK_C_CloseSession sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_CloseSession)GetProcAddress(hPK11, "C_CloseSession");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession);
}

#define C_Login isc_C_Login

CK_RV
C_Login(CK_SESSION_HANDLE hSession,
	CK_USER_TYPE userType,
	CK_CHAR_PTR pPin,
	CK_ULONG usPinLen)
{
	CK_C_Login sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Login)GetProcAddress(hPK11, "C_Login");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, userType, pPin, usPinLen);
}

#define C_CreateObject isc_C_CreateObject

CK_RV
C_CreateObject(CK_SESSION_HANDLE hSession,
	       CK_ATTRIBUTE_PTR pTemplate,
	       CK_ULONG usCount,
	       CK_OBJECT_HANDLE_PTR phObject)
{
	CK_C_CreateObject sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_CreateObject)GetProcAddress(hPK11, "C_CreateObject");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, pTemplate, usCount, phObject);
}

#define C_DestroyObject isc_C_DestroyObject

CK_RV
C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
	CK_C_DestroyObject sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_DestroyObject)GetProcAddress(hPK11, "C_DestroyObject");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject);
}

#define C_GetAttributeValue isc_C_GetAttributeValue

CK_RV
C_GetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	CK_C_GetAttributeValue sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_GetAttributeValue)GetProcAddress(hPK11,
						     "C_GetAttributeValue");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

#define C_SetAttributeValue isc_C_SetAttributeValue

CK_RV
C_SetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	CK_C_SetAttributeValue sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_SetAttributeValue)GetProcAddress(hPK11,
						     "C_SetAttributeValue");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

#define C_FindObjectsInit isc_C_FindObjectsInit

CK_RV
C_FindObjectsInit(CK_SESSION_HANDLE hSession,
		  CK_ATTRIBUTE_PTR pTemplate,
		  CK_ULONG usCount)
{
	CK_C_FindObjectsInit sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjectsInit)GetProcAddress(hPK11,
						   "C_FindObjectsInit");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, pTemplate, usCount);
}

#define C_FindObjects isc_C_FindObjects

CK_RV
C_FindObjects(CK_SESSION_HANDLE hSession,
	      CK_OBJECT_HANDLE_PTR phObject,
	      CK_ULONG usMaxObjectCount,
	      CK_ULONG_PTR pusObjectCount)
{
	CK_C_FindObjects sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjects)GetProcAddress(hPK11, "C_FindObjects");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, phObject, usMaxObjectCount, pusObjectCount);
}

#define C_FindObjectsFinal isc_C_FindObjectsFinal

CK_RV
C_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
	CK_C_FindObjectsFinal sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjectsFinal)GetProcAddress(hPK11,
						    "C_FindObjectsFinal");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession);
}

#define C_GenerateKeyPair isc_C_GenerateKeyPair

CK_RV
C_GenerateKeyPair(CK_SESSION_HANDLE hSession,
		  CK_MECHANISM_PTR pMechanism,
		  CK_ATTRIBUTE_PTR pPublicKeyTemplate,
		  CK_ULONG usPublicKeyAttributeCount,
		  CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
		  CK_ULONG usPrivateKeyAttributeCount,
		  CK_OBJECT_HANDLE_PTR phPrivateKey,
		  CK_OBJECT_HANDLE_PTR phPublicKey)
{
	CK_C_GenerateKeyPair sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_GenerateKeyPair)GetProcAddress(hPK11,
						   "C_GenerateKeyPair");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession,
		      pMechanism,
		      pPublicKeyTemplate,
		      usPublicKeyAttributeCount,
		      pPrivateKeyTemplate,
		      usPrivateKeyAttributeCount,
		      phPrivateKey,
		      phPublicKey);
}

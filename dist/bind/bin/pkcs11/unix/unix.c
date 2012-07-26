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

/* $Id: unix.c,v 1.4 2009-10-26 23:47:35 tbox Exp $ */

/* $Id */

/*! \file */

/* dynamic loader (ifndef FORCE_STATIC_PROVIDER) */

#include <dlfcn.h>

/* load PKCS11 dynamic object */

#ifndef PK11_LIB_LOCATION
#error "PK11_LIB_LOCATION is not set"
#endif

const char *pk11_libname = PK11_LIB_LOCATION;

void *hPK11 = NULL;

#define C_Initialize isc_C_Initialize

CK_RV
C_Initialize(CK_VOID_PTR pReserved);

CK_RV
C_Initialize(CK_VOID_PTR pReserved)
{
	CK_C_Initialize sym;

	hPK11 = dlopen(pk11_libname, RTLD_NOW);

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Initialize)dlsym(hPK11, "C_Initialize");
	if (sym == NULL)
		return 0xff;
	return (*sym)(pReserved);
}

#define C_Finalize isc_C_Finalize

CK_RV
C_Finalize(CK_VOID_PTR pReserved);

CK_RV
C_Finalize(CK_VOID_PTR pReserved)
{
	CK_C_Finalize sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Finalize)dlsym(hPK11, "C_Finalize");
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
	      CK_SESSION_HANDLE_PTR phSession);

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
		hPK11 = dlopen(pk11_libname, RTLD_NOW);
	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_OpenSession)dlsym(hPK11, "C_OpenSession");
	if (sym == NULL)
		return 0xff;
	return (*sym)(slotID, flags, pApplication, Notify, phSession);
}

#define C_CloseSession isc_C_CloseSession

CK_RV
C_CloseSession(CK_SESSION_HANDLE hSession);

CK_RV
C_CloseSession(CK_SESSION_HANDLE hSession)
{
	CK_C_CloseSession sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_CloseSession)dlsym(hPK11, "C_CloseSession");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession);
}

#define C_Login isc_C_Login

CK_RV
C_Login(CK_SESSION_HANDLE hSession,
	CK_USER_TYPE userType,
	CK_CHAR_PTR pPin,
	CK_ULONG usPinLen);

CK_RV
C_Login(CK_SESSION_HANDLE hSession,
	CK_USER_TYPE userType,
	CK_CHAR_PTR pPin,
	CK_ULONG usPinLen)
{
	CK_C_Login sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_Login)dlsym(hPK11, "C_Login");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, userType, pPin, usPinLen);
}

#define C_CreateObject isc_C_CreateObject

CK_RV
C_CreateObject(CK_SESSION_HANDLE hSession,
	       CK_ATTRIBUTE_PTR pTemplate,
	       CK_ULONG usCount,
	       CK_OBJECT_HANDLE_PTR phObject);

CK_RV
C_CreateObject(CK_SESSION_HANDLE hSession,
	       CK_ATTRIBUTE_PTR pTemplate,
	       CK_ULONG usCount,
	       CK_OBJECT_HANDLE_PTR phObject)
{
	CK_C_CreateObject sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_CreateObject)dlsym(hPK11, "C_CreateObject");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, pTemplate, usCount, phObject);
}

#define C_DestroyObject isc_C_DestroyObject

CK_RV
C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject);

CK_RV
C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
	CK_C_DestroyObject sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_DestroyObject)dlsym(hPK11, "C_DestroyObject");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject);
}

#define C_GetAttributeValue isc_C_GetAttributeValue

CK_RV
C_GetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount);

CK_RV
C_GetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	CK_C_GetAttributeValue sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_GetAttributeValue)dlsym(hPK11, "C_GetAttributeValue");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

#define C_SetAttributeValue isc_C_SetAttributeValue

CK_RV
C_SetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount);

CK_RV
C_SetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	CK_C_SetAttributeValue sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_SetAttributeValue)dlsym(hPK11, "C_SetAttributeValue");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

#define C_FindObjectsInit isc_C_FindObjectsInit

CK_RV
C_FindObjectsInit(CK_SESSION_HANDLE hSession,
		  CK_ATTRIBUTE_PTR pTemplate,
		  CK_ULONG usCount);

CK_RV
C_FindObjectsInit(CK_SESSION_HANDLE hSession,
		  CK_ATTRIBUTE_PTR pTemplate,
		  CK_ULONG usCount)
{
	CK_C_FindObjectsInit sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjectsInit)dlsym(hPK11, "C_FindObjectsInit");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, pTemplate, usCount);
}

#define C_FindObjects isc_C_FindObjects

CK_RV
C_FindObjects(CK_SESSION_HANDLE hSession,
	      CK_OBJECT_HANDLE_PTR phObject,
	      CK_ULONG usMaxObjectCount,
	      CK_ULONG_PTR pusObjectCount);

CK_RV
C_FindObjects(CK_SESSION_HANDLE hSession,
	      CK_OBJECT_HANDLE_PTR phObject,
	      CK_ULONG usMaxObjectCount,
	      CK_ULONG_PTR pusObjectCount)
{
	CK_C_FindObjects sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjects)dlsym(hPK11, "C_FindObjects");
	if (sym == NULL)
		return 0xff;
	return (*sym)(hSession, phObject, usMaxObjectCount, pusObjectCount);
}

#define C_FindObjectsFinal isc_C_FindObjectsFinal

CK_RV
C_FindObjectsFinal(CK_SESSION_HANDLE hSession);

CK_RV
C_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
	CK_C_FindObjectsFinal sym;

	if (hPK11 == NULL)
		return 0xfe;
	sym = (CK_C_FindObjectsFinal)dlsym(hPK11, "C_FindObjectsFinal");
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
		  CK_OBJECT_HANDLE_PTR phPublicKey);

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
	sym = (CK_C_GenerateKeyPair)dlsym(hPK11, "C_GenerateKeyPair");
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

/*	$NetBSD: pk11_api.c,v 1.1.1.4 2014/12/10 03:34:45 christos Exp $	*/

/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

/* Id */

/*! \file */

/* missing code for WIN32 */

#include <config.h>

#include <string.h>
#include <windows.h>

#include <isc/log.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/stdio.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <pk11/pk11.h>
#include <pk11/internal.h>

#define HAVE_GETPASSPHRASE

char *
getpassphrase(const char *prompt) {
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
	return (buf);
}

/* load PKCS11 DLL */

static HINSTANCE hPK11 = NULL;

CK_RV
pkcs_C_Initialize(CK_VOID_PTR pReserved) {
	CK_C_Initialize sym;
	const char *lib_name = pk11_get_lib_name();

	if (hPK11 != NULL)
		return (CKR_LIBRARY_ALREADY_INITIALIZED);

	if (lib_name == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	/* Visual Studio convertion issue... */
	if (*lib_name == ' ')
		lib_name++;

	hPK11 = LoadLibraryA(lib_name);

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	sym = (CK_C_Initialize)GetProcAddress(hPK11, "C_Initialize");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(pReserved);
}

CK_RV
pkcs_C_Finalize(CK_VOID_PTR pReserved) {
	CK_C_Finalize sym;
	CK_RV rv;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	sym = (CK_C_Finalize)GetProcAddress(hPK11, "C_Finalize");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	rv = (*sym)(pReserved);
	if ((rv == CKR_OK) && (FreeLibrary(hPK11) == 0))
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	hPK11 = NULL;
	return (rv);
}

CK_RV
pkcs_C_GetSlotList(CK_BBOOL tokenPresent,
	      CK_SLOT_ID_PTR pSlotList,
	      CK_ULONG_PTR pulCount)
{
	static CK_C_GetSlotList sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GetSlotList)GetProcAddress(hPK11, "C_GetSlotList");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(tokenPresent, pSlotList, pulCount);
}

CK_RV
pkcs_C_GetTokenInfo(CK_SLOT_ID slotID,
	       CK_TOKEN_INFO_PTR pInfo)
{
	static CK_C_GetTokenInfo sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GetTokenInfo)GetProcAddress(hPK11,
							"C_GetTokenInfo");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, pInfo);
}

CK_RV
pkcs_C_GetMechanismInfo(CK_SLOT_ID slotID,
		   CK_MECHANISM_TYPE type,
		   CK_MECHANISM_INFO_PTR pInfo)
{
	static CK_C_GetMechanismInfo sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GetMechanismInfo)GetProcAddress(hPK11,
							"C_GetMechanismInfo");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, type, pInfo);
}

CK_RV
pkcs_C_OpenSession(CK_SLOT_ID slotID,
	      CK_FLAGS flags,
	      CK_VOID_PTR pApplication,
	      CK_RV  (*Notify) (CK_SESSION_HANDLE hSession,
				CK_NOTIFICATION event,
				CK_VOID_PTR pApplication),
	      CK_SESSION_HANDLE_PTR phSession)
{
	static CK_C_OpenSession sym = NULL;

	if (hPK11 == NULL)
		hPK11 = LoadLibraryA(pk11_get_lib_name());
	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_OpenSession)GetProcAddress(hPK11, "C_OpenSession");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, flags, pApplication, Notify, phSession);
}

CK_RV
pkcs_C_CloseSession(CK_SESSION_HANDLE hSession) {
	static CK_C_CloseSession sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_CloseSession)GetProcAddress(hPK11,
							"C_CloseSession");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_Login(CK_SESSION_HANDLE hSession,
	CK_USER_TYPE userType,
	CK_CHAR_PTR pPin,
	CK_ULONG usPinLen)
{
	static CK_C_Login sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_Login)GetProcAddress(hPK11, "C_Login");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, userType, pPin, usPinLen);
}

CK_RV
pkcs_C_Logout(CK_SESSION_HANDLE hSession) {
	static CK_C_Logout sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_Logout)GetProcAddress(hPK11, "C_Logout");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_CreateObject(CK_SESSION_HANDLE hSession,
	       CK_ATTRIBUTE_PTR pTemplate,
	       CK_ULONG usCount,
	       CK_OBJECT_HANDLE_PTR phObject)
{
	static CK_C_CreateObject sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_CreateObject)GetProcAddress(hPK11,
							"C_CreateObject");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pTemplate, usCount, phObject);
}

CK_RV
pkcs_C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject) {
	static CK_C_DestroyObject sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_DestroyObject)GetProcAddress(hPK11,
							 "C_DestroyObject");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject);
}

CK_RV
pkcs_C_GetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	static CK_C_GetAttributeValue sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GetAttributeValue)GetProcAddress(hPK11,
							"C_GetAttributeValue");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

CK_RV
pkcs_C_SetAttributeValue(CK_SESSION_HANDLE hSession,
		    CK_OBJECT_HANDLE hObject,
		    CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount)
{
	static CK_C_SetAttributeValue sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_SetAttributeValue)GetProcAddress(hPK11,
							"C_SetAttributeValue");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

CK_RV
pkcs_C_FindObjectsInit(CK_SESSION_HANDLE hSession,
		  CK_ATTRIBUTE_PTR pTemplate,
		  CK_ULONG usCount)
{
	static CK_C_FindObjectsInit sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_FindObjectsInit)GetProcAddress(hPK11,
							"C_FindObjectsInit");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pTemplate, usCount);
}

CK_RV
pkcs_C_FindObjects(CK_SESSION_HANDLE hSession,
	      CK_OBJECT_HANDLE_PTR phObject,
	      CK_ULONG usMaxObjectCount,
	      CK_ULONG_PTR pusObjectCount)
{
	static CK_C_FindObjects sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_FindObjects)GetProcAddress(hPK11, "C_FindObjects");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, phObject, usMaxObjectCount, pusObjectCount);
}

CK_RV
pkcs_C_FindObjectsFinal(CK_SESSION_HANDLE hSession) {
	static CK_C_FindObjectsFinal sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_FindObjectsFinal)GetProcAddress(hPK11,
							"C_FindObjectsFinal");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_EncryptInit(CK_SESSION_HANDLE hSession,
		   CK_MECHANISM_PTR pMechanism,
		   CK_OBJECT_HANDLE hKey)
{
	static CK_C_EncryptInit sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_EncryptInit)GetProcAddress(hPK11, "C_EncryptInit");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Encrypt(CK_SESSION_HANDLE hSession,
	       CK_BYTE_PTR pData,
	       CK_ULONG ulDataLen,
	       CK_BYTE_PTR pEncryptedData,
	       CK_ULONG_PTR pulEncryptedDataLen)
{
	static CK_C_Encrypt sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_Encrypt)GetProcAddress(hPK11, "C_Encrypt");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen,
		      pEncryptedData, pulEncryptedDataLen);
}

CK_RV
pkcs_C_DigestInit(CK_SESSION_HANDLE hSession,
	     CK_MECHANISM_PTR pMechanism)
{
	static CK_C_DigestInit sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_DigestInit)GetProcAddress(hPK11, "C_DigestInit");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism);
}

CK_RV
pkcs_C_DigestUpdate(CK_SESSION_HANDLE hSession,
	       CK_BYTE_PTR pPart,
	       CK_ULONG ulPartLen)
{
	static CK_C_DigestUpdate sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_DigestUpdate)GetProcAddress(hPK11,
							"C_DigestUpdate");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_DigestFinal(CK_SESSION_HANDLE hSession,
	      CK_BYTE_PTR pDigest,
	      CK_ULONG_PTR pulDigestLen)
{
	static CK_C_DigestFinal sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_DigestFinal)GetProcAddress(hPK11, "C_DigestFinal");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pDigest, pulDigestLen);
}

CK_RV
pkcs_C_SignInit(CK_SESSION_HANDLE hSession,
	   CK_MECHANISM_PTR pMechanism,
	   CK_OBJECT_HANDLE hKey)
{
	static CK_C_SignInit sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_SignInit)GetProcAddress(hPK11, "C_SignInit");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Sign(CK_SESSION_HANDLE hSession,
       CK_BYTE_PTR pData,
       CK_ULONG ulDataLen,
       CK_BYTE_PTR pSignature,
       CK_ULONG_PTR pulSignatureLen)
{
	static CK_C_Sign sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_Sign)GetProcAddress(hPK11, "C_Sign");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen, pSignature, pulSignatureLen);
}

CK_RV
pkcs_C_SignUpdate(CK_SESSION_HANDLE hSession,
	     CK_BYTE_PTR pPart,
	     CK_ULONG ulPartLen)
{
	static CK_C_SignUpdate sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_SignUpdate)GetProcAddress(hPK11, "C_SignUpdate");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_SignFinal(CK_SESSION_HANDLE hSession,
	    CK_BYTE_PTR pSignature,
	    CK_ULONG_PTR pulSignatureLen)
{
	static CK_C_SignFinal sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_SignFinal)GetProcAddress(hPK11, "C_SignFinal");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSignature, pulSignatureLen);
}

CK_RV
pkcs_C_VerifyInit(CK_SESSION_HANDLE hSession,
	     CK_MECHANISM_PTR pMechanism,
	     CK_OBJECT_HANDLE hKey)
{
	static CK_C_VerifyInit sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_VerifyInit)GetProcAddress(hPK11, "C_VerifyInit");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Verify(CK_SESSION_HANDLE hSession,
	 CK_BYTE_PTR pData,
	 CK_ULONG ulDataLen,
	 CK_BYTE_PTR pSignature,
	 CK_ULONG ulSignatureLen)
{
	static CK_C_Verify sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_Verify)GetProcAddress(hPK11, "C_Verify");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen, pSignature, ulSignatureLen);
}

CK_RV
pkcs_C_VerifyUpdate(CK_SESSION_HANDLE hSession,
	       CK_BYTE_PTR pPart,
	       CK_ULONG ulPartLen)
{
	static CK_C_VerifyUpdate sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_VerifyUpdate)GetProcAddress(hPK11,
							"C_VerifyUpdate");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_VerifyFinal(CK_SESSION_HANDLE hSession,
	      CK_BYTE_PTR pSignature,
	      CK_ULONG ulSignatureLen)
{
	static CK_C_VerifyFinal sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_VerifyFinal)GetProcAddress(hPK11, "C_VerifyFinal");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSignature, ulSignatureLen);
}

CK_RV
pkcs_C_GenerateKey(CK_SESSION_HANDLE hSession,
	      CK_MECHANISM_PTR pMechanism,
	      CK_ATTRIBUTE_PTR pTemplate,
	      CK_ULONG ulCount,
	      CK_OBJECT_HANDLE_PTR phKey)
{
	static CK_C_GenerateKey sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GenerateKey)GetProcAddress(hPK11, "C_GenerateKey");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, pTemplate, ulCount, phKey);
}

CK_RV
pkcs_C_GenerateKeyPair(CK_SESSION_HANDLE hSession,
		  CK_MECHANISM_PTR pMechanism,
		  CK_ATTRIBUTE_PTR pPublicKeyTemplate,
		  CK_ULONG usPublicKeyAttributeCount,
		  CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
		  CK_ULONG usPrivateKeyAttributeCount,
		  CK_OBJECT_HANDLE_PTR phPrivateKey,
		  CK_OBJECT_HANDLE_PTR phPublicKey)
{
	static CK_C_GenerateKeyPair sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GenerateKeyPair)GetProcAddress(hPK11,
							"C_GenerateKeyPair");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession,
		      pMechanism,
		      pPublicKeyTemplate,
		      usPublicKeyAttributeCount,
		      pPrivateKeyTemplate,
		      usPrivateKeyAttributeCount,
		      phPrivateKey,
		      phPublicKey);
}

CK_RV
pkcs_C_DeriveKey(CK_SESSION_HANDLE hSession,
	    CK_MECHANISM_PTR pMechanism,
	    CK_OBJECT_HANDLE hBaseKey,
	    CK_ATTRIBUTE_PTR pTemplate,
	    CK_ULONG ulAttributeCount,
	    CK_OBJECT_HANDLE_PTR phKey)
{
	static CK_C_DeriveKey sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_DeriveKey)GetProcAddress(hPK11, "C_DeriveKey");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession,
		      pMechanism,
		      hBaseKey,
		      pTemplate,
		      ulAttributeCount,
		      phKey);
}

CK_RV
pkcs_C_SeedRandom(CK_SESSION_HANDLE hSession,
	     CK_BYTE_PTR pSeed,
	     CK_ULONG ulSeedLen)
{
	static CK_C_SeedRandom sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_SeedRandom)GetProcAddress(hPK11, "C_SeedRandom");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSeed, ulSeedLen);
}

CK_RV
pkcs_C_GenerateRandom(CK_SESSION_HANDLE hSession,
		 CK_BYTE_PTR RandomData,
		 CK_ULONG ulRandomLen)
{
	static CK_C_GenerateRandom sym = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if (sym == NULL)
		sym = (CK_C_GenerateRandom)GetProcAddress(hPK11,
							  "C_GenerateRandom");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, RandomData, ulRandomLen);
}

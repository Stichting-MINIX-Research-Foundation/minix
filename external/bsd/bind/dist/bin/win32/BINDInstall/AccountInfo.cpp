/*
 * Portions Copyright (C) 2004, 2007, 2009, 2013  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001, 2002  Internet Software Consortium.
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

/* Id: AccountInfo.cpp,v 1.10 2009/09/29 23:48:04 tbox Exp  */

/* Compiled with UNICODE */

#include "stdafx.h"

#include <windows.h>
#include <lm.h>
#include <ntsecapi.h>

#include <isc/ntgroups.h>
#include <isc/result.h>
#include "AccountInfo.h"

#define MAX_NAME_LENGTH 256

NTSTATUS
OpenPolicy(
    LPWSTR ServerName,		/* machine to open policy on (Unicode) */
    DWORD DesiredAccess,	/* desired access to policy */
    PLSA_HANDLE PolicyHandle	/* resultant policy handle */
    );

BOOL
GetAccountSid(
    LPTSTR SystemName,		/* where to lookup account */
    LPTSTR AccountName,		/* account of interest */
    PSID *Sid			/* resultant buffer containing SID */
    );

NTSTATUS
SetPrivilegeOnAccount(
    LSA_HANDLE PolicyHandle,	/* open policy handle */
    PSID AccountSid,		/* SID to grant privilege to */
    LPWSTR PrivilegeName,	/* privilege to grant (Unicode) */
    BOOL bEnable		/* enable or disable */
    );

NTSTATUS
GetPrivilegesOnAccount(
    LSA_HANDLE PolicyHandle,	/* open policy handle */
    PSID AccountSid,		/* SID to grant privilege to */
    wchar_t **PrivList,		/* Ptr to List of Privileges found */
    unsigned int *PrivCount	/* total number of Privileges in list */
    );

NTSTATUS
AddPrivilegeToAcccount(
    LPTSTR AccountName,		/* Name of the account */
    LPWSTR PrivilegeName	/* Privilege to Add */
    );

void
InitLsaString(
    PLSA_UNICODE_STRING LsaString,	/* destination */
    LPWSTR String			/* source (Unicode) */
    );

void
DisplayNtStatus(
    LPSTR szAPI,		/* pointer to function name (ANSI) */
    NTSTATUS Status		/* NTSTATUS error value */
    );

void
DisplayWinError(
    LPSTR szAPI,		/* pointer to function name (ANSI) */
    DWORD WinError		/* DWORD WinError */
    );

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)
#endif

/*
 * Note that this code only retrieves the list of privileges of the
 * requested account or group. However, all accounts belong to the
 * Everyone group even though that group is not returned by the
 * calls to get the groups to which that account belongs.
 * The Everyone group has two privileges associated with it:
 * SeChangeNotifyPrivilege and SeNetworkLogonRight
 * It is not advisable to disable or remove these privileges
 * from the group nor can the account be removed from the Everyone
 * group
 * The None group has no privileges associated with it and is the group
 * to which an account belongs if it is associated with no group.
 */

int
GetAccountPrivileges(char *name, wchar_t **PrivList, unsigned int *PrivCount,
		     char **Accounts, unsigned int *totalAccounts,
		     int maxAccounts)
{
	LSA_HANDLE PolicyHandle;
	TCHAR AccountName[256];		/* static account name buffer */
	PSID pSid;
	unsigned int i;
	NTSTATUS Status;
	isc_result_t istatus;
	int iRetVal = RTN_ERROR;	/* assume error from main */

	/*
	 * Open the policy on the target machine.
	 */
	if ((Status = OpenPolicy(NULL,
				 POLICY_LOOKUP_NAMES,
				 &PolicyHandle)) != STATUS_SUCCESS)
		return (RTN_ERROR);

	/*
	 * Let's see if the account exists. Return if not
	 */
	wsprintf(AccountName, TEXT("%hS"), name);
	if (!GetAccountSid(NULL, AccountName, &pSid))
		return (RTN_NOACCOUNT);
	/*
	 * Find out what groups the account belongs to
	 */
	istatus = isc_ntsecurity_getaccountgroups(name, Accounts, maxAccounts,
						  totalAccounts);
	if (istatus == ISC_R_NOMEMORY)
		return (RTN_NOMEMORY);
	else if (istatus != ISC_R_SUCCESS)
		return (RTN_ERROR);

	Accounts[*totalAccounts] = name; /* Add the account to the list */
	(*totalAccounts)++;

	/*
	 * Loop through each Account to get the list of privileges
	 */
	for (i = 0; i < *totalAccounts; i++) {
		wsprintf(AccountName, TEXT("%hS"), Accounts[i]);
		 /* Obtain the SID of the user/group. */
		if (!GetAccountSid(NULL, AccountName, &pSid))
			continue;	/* Try the next one */
		/* Get the Privileges allocated to this SID */
		if ((Status = GetPrivilegesOnAccount(PolicyHandle, pSid,
			PrivList, PrivCount)) == STATUS_SUCCESS)
		{
			iRetVal=RTN_OK;
			if (pSid != NULL)
				HeapFree(GetProcessHeap(), 0, pSid);
		} else {
			if (pSid != NULL)
				HeapFree(GetProcessHeap(), 0, pSid);
			continue;	/* Try the next one */
		}
	}
	/*
	 * Close the policy handle.
	 */
	LsaClose(PolicyHandle);

	(*totalAccounts)--;	/* Correct for the number of groups */
	return iRetVal;
}

BOOL
CreateServiceAccount(char *name, char *password) {
	NTSTATUS retstat;
	USER_INFO_1 ui;
	DWORD dwLevel = 1;
	DWORD dwError = 0;
	NET_API_STATUS nStatus;

	size_t namelen = strlen(name);
	size_t passwdlen = strlen(password);
	wchar_t AccountName[MAX_NAME_LENGTH];
	wchar_t AccountPassword[MAX_NAME_LENGTH];

	mbstowcs(AccountName, name, namelen + 1);
	mbstowcs(AccountPassword, password, passwdlen + 1);

	/*
	 * Set up the USER_INFO_1 structure.
	 * USER_PRIV_USER: name is required here when creating an account
	 * rather than an administrator or a guest.
	 */

	ui.usri1_name = (LPWSTR) &AccountName;
	ui.usri1_password = (LPWSTR) &AccountPassword;
	ui.usri1_priv = USER_PRIV_USER;
	ui.usri1_home_dir = NULL;
	ui.usri1_comment = L"ISC BIND Service Account";
	ui.usri1_flags = UF_PASSWD_CANT_CHANGE | UF_DONT_EXPIRE_PASSWD |
			 UF_SCRIPT;
	ui.usri1_script_path = NULL;
	/*
	 * Call the NetUserAdd function, specifying level 1.
	 */
	nStatus = NetUserAdd(NULL, dwLevel, (LPBYTE)&ui, &dwError);

	if (nStatus != NERR_Success)
		return (FALSE);

	retstat = AddPrivilegeToAcccount(name, SE_SERVICE_LOGON_PRIV);
	return (TRUE);
}

NTSTATUS
AddPrivilegeToAcccount(LPTSTR name, LPWSTR PrivilegeName) {
	LSA_HANDLE PolicyHandle;
	TCHAR AccountName[256];		/* static account name buffer */
	PSID pSid;
	NTSTATUS Status;
	unsigned long err;

	/*
	 * Open the policy on the target machine.
	 */
	if ((Status = OpenPolicy(NULL, POLICY_ALL_ACCESS, &PolicyHandle))
		!= STATUS_SUCCESS)
		return (RTN_ERROR);

	/*
	 * Let's see if the account exists. Return if not
	 */
	wsprintf(AccountName, TEXT("%hS"), name);
	if (!GetAccountSid(NULL, AccountName, &pSid))
		return (RTN_NOACCOUNT);

	err = LsaNtStatusToWinError(SetPrivilegeOnAccount(PolicyHandle,
		pSid, PrivilegeName, TRUE));

	LsaClose(PolicyHandle);
	if (err == ERROR_SUCCESS)
		return (RTN_OK);
	else
		return (err);
}

void
InitLsaString(PLSA_UNICODE_STRING LsaString, LPWSTR String){
	size_t StringLength;

	if (String == NULL) {
		LsaString->Buffer = NULL;
		LsaString->Length = 0;
		LsaString->MaximumLength = 0;
		return;
	}

	StringLength = wcslen(String);
	LsaString->Buffer = String;
	LsaString->Length = (USHORT) StringLength * sizeof(WCHAR);
	LsaString->MaximumLength = (USHORT)(StringLength+1) * sizeof(WCHAR);
}

NTSTATUS
OpenPolicy(LPWSTR ServerName, DWORD DesiredAccess, PLSA_HANDLE PolicyHandle){
	LSA_OBJECT_ATTRIBUTES ObjectAttributes;
	LSA_UNICODE_STRING ServerString;
	PLSA_UNICODE_STRING Server = NULL;

	/*
	 * Always initialize the object attributes to all zeroes.
	 */
	ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));

	if (ServerName != NULL) {
		/*
		 * Make a LSA_UNICODE_STRING out of the LPWSTR passed in
		 */
		InitLsaString(&ServerString, ServerName);
		Server = &ServerString;
	}

	/*
	 * Attempt to open the policy.
	 */
	return (LsaOpenPolicy(Server, &ObjectAttributes, DesiredAccess,
		PolicyHandle));
}

BOOL
GetAccountSid(LPTSTR SystemName, LPTSTR AccountName, PSID *Sid) {
	LPTSTR ReferencedDomain = NULL;
	DWORD cbSid = 128;    /* initial allocation attempt */
	DWORD cbReferencedDomain = 16; /* initial allocation size */
	SID_NAME_USE peUse;
	BOOL bSuccess = FALSE; /* assume this function will fail */

	__try {
		/*
		 * initial memory allocations
		 */
		if ((*Sid = HeapAlloc(GetProcessHeap(), 0, cbSid)) == NULL)
			__leave;

		if ((ReferencedDomain = (LPTSTR) HeapAlloc(GetProcessHeap(), 0,
				       cbReferencedDomain)) == NULL) __leave;

		/*
		 * Obtain the SID of the specified account on the specified system.
		 */
		while (!LookupAccountName(SystemName, AccountName, *Sid, &cbSid,
					  ReferencedDomain, &cbReferencedDomain,
					  &peUse))
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
				/* reallocate memory */
				if ((*Sid = HeapReAlloc(GetProcessHeap(), 0,
					*Sid, cbSid)) == NULL) __leave;

				if ((ReferencedDomain= (LPTSTR) HeapReAlloc(
					GetProcessHeap(), 0, ReferencedDomain,
					cbReferencedDomain)) == NULL)
				__leave;
			}
			else
				__leave;
		}
		bSuccess = TRUE;
	} /* finally */
	__finally {

		/* Cleanup and indicate failure, if appropriate. */

		HeapFree(GetProcessHeap(), 0, ReferencedDomain);

		if (!bSuccess) {
			if (*Sid != NULL) {
				HeapFree(GetProcessHeap(), 0, *Sid);
				*Sid = NULL;
			}
		}

	}

	return (bSuccess);
}

NTSTATUS
SetPrivilegeOnAccount(LSA_HANDLE PolicyHandle, PSID AccountSid,
		      LPWSTR PrivilegeName, BOOL bEnable)
{
	LSA_UNICODE_STRING PrivilegeString;

	/* Create a LSA_UNICODE_STRING for the privilege name. */
	InitLsaString(&PrivilegeString, PrivilegeName);

	/* grant or revoke the privilege, accordingly */
	if (bEnable)
		return (LsaAddAccountRights(PolicyHandle, AccountSid,
			&PrivilegeString, 1));
	else
		return (LsaRemoveAccountRights(PolicyHandle, AccountSid,
			FALSE, &PrivilegeString, 1));
}

NTSTATUS
GetPrivilegesOnAccount(LSA_HANDLE PolicyHandle, PSID AccountSid,
		       wchar_t **PrivList, unsigned int *PrivCount)
{
	NTSTATUS Status;
	LSA_UNICODE_STRING *UserRights;
	ULONG CountOfRights;
	unsigned int retlen = 0;
	DWORD i, j;
	int found;

	Status = LsaEnumerateAccountRights(PolicyHandle, AccountSid,
		&UserRights, &CountOfRights);
	/* Only continue if there is something */
	if (UserRights == NULL || Status != STATUS_SUCCESS)
		return (Status);

	for (i = 0; i < CountOfRights; i++) {
		found = -1;
		retlen = UserRights[i].Length/sizeof(wchar_t);
		for (j = 0; j < *PrivCount; j++) {
			found = wcsncmp(PrivList[j], UserRights[i].Buffer,
					retlen);
			if (found == 0)
				break;
		}
		if (found != 0) {
			PrivList[*PrivCount] =
			    (wchar_t *)malloc(UserRights[i].MaximumLength);
			if (PrivList[*PrivCount] == NULL)
				return (RTN_NOMEMORY);

			wcsncpy(PrivList[*PrivCount], UserRights[i].Buffer,
				retlen);
			PrivList[*PrivCount][retlen] = L'\0';
			(*PrivCount)++;
		}

	}

	return (Status);
}

void
DisplayNtStatus(LPSTR szAPI, NTSTATUS Status) {
	/* Convert the NTSTATUS to Winerror. Then call DisplayWinError(). */
	DisplayWinError(szAPI, LsaNtStatusToWinError(Status));
}

void
DisplayWinError(LPSTR szAPI, DWORD WinError) {
	LPSTR MessageBuffer;
	DWORD dwBufferLength;

	if (dwBufferLength=FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WinError, GetUserDefaultLangID(),
		(LPSTR) &MessageBuffer, 0, NULL)){
		DWORD dwBytesWritten; /* unused */

		/* Output message string on stderr. */
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), MessageBuffer,
			  dwBufferLength, &dwBytesWritten, NULL);

		/* Free the buffer allocated by the system. */
		LocalFree(MessageBuffer);
	}
}

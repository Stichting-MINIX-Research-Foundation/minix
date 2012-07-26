/*
 * Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001  Internet Software Consortium.
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

/* $Id: AccountInfo.h,v 1.6 2007-06-19 23:47:07 tbox Exp $ */


#define RTN_OK		0
#define RTN_NOACCOUNT	1
#define RTN_NOMEMORY	2
#define RTN_ERROR	10

#define SE_SERVICE_LOGON_PRIV	L"SeServiceLogonRight"

/*
 * This routine retrieves the list of all Privileges associated with
 * a given account as well as the groups to which it beongs
 */
int
GetAccountPrivileges(
	char *name,			/* Name of Account */
	wchar_t **PrivList,		/* List of Privileges returned */
	unsigned int *PrivCount,	/* Count of Privileges returned */
	char **Groups,		/* List of Groups to which account belongs */
	unsigned int *totalGroups,	/* Count of Groups returned */
	int maxGroups		/* Maximum number of Groups to return */
	);

/*
 * This routine creates an account with the given name which has just
 * the logon service privilege and no membership of any groups,
 * i.e. it's part of the None group.
 */
BOOL
CreateServiceAccount(char *name, char *password);

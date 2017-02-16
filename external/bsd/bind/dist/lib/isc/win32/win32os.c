/*	$NetBSD: win32os.c,v 1.6 2015/07/08 17:29:00 christos Exp $	*/

/*
 * Copyright (C) 2004, 2007, 2013-2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2002  Internet Software Consortium.
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

#include <windows.h>

#ifndef TESTVERSION
#include <isc/win32os.h>
#else
#include <stdio.h>
#endif

int
isc_win32os_versioncheck(unsigned int major, unsigned int minor,
			 unsigned int spmajor, unsigned int spminor)
{
	OSVERSIONINFOEX osVer;
	DWORD typeMask;
	ULONGLONG conditionMask;

	memset(&osVer, 0, sizeof(OSVERSIONINFOEX));
	osVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	typeMask = 0;
	conditionMask = 0;

	/* Optimistic: likely greater */
	osVer.dwMajorVersion = major;
	typeMask |= VER_MAJORVERSION;
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_MAJORVERSION,
					    VER_GREATER);
	osVer.dwMinorVersion = minor;
	typeMask |= VER_MINORVERSION;
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_MINORVERSION,
					    VER_GREATER);
	osVer.wServicePackMajor = spmajor;
	typeMask |= VER_SERVICEPACKMAJOR;
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_SERVICEPACKMAJOR,
					    VER_GREATER);
	osVer.wServicePackMinor = spminor;
	typeMask |= VER_SERVICEPACKMINOR;
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_SERVICEPACKMINOR,
					    VER_GREATER);
	if (VerifyVersionInfo(&osVer, typeMask, conditionMask))
		return (1);

	/* Failed: retry with equal */
	conditionMask = 0;
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_MAJORVERSION,
					    VER_EQUAL);
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_MINORVERSION,
					    VER_EQUAL);
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_SERVICEPACKMAJOR,
					    VER_EQUAL);
	conditionMask = VerSetConditionMask(conditionMask,
					    VER_SERVICEPACKMINOR,
					    VER_EQUAL);
	if (VerifyVersionInfo(&osVer, typeMask, conditionMask))
		return (0);
	else
		return (-1);
}

#ifdef TESTVERSION
int
main(int argc, char **argv) {
	unsigned int major = 0;
	unsigned int minor = 0;
	unsigned int spmajor = 0;
	unsigned int spminor = 0;
	int ret;

	if (argc > 1) {
		--argc;
		++argv;
		major = (unsigned int) atoi(argv[0]);
	}
	if (argc > 1) {
		--argc;
		++argv;
		minor = (unsigned int) atoi(argv[0]);
	}
	if (argc > 1) {
		--argc;
		++argv;
		spmajor = (unsigned int) atoi(argv[0]);
	}
	if (argc > 1) {
		--argc;
		++argv;
		spminor = (unsigned int) atoi(argv[0]);
	}

	ret = isc_win32os_versioncheck(major, minor, spmajor, spminor);

	printf("%s major %u minor %u SP major %u SP minor %u\n",
	       ret > 0 ? "greater" : (ret == 0 ? "equal" : "less"),
	       major, minor, spmajor, spminor);
	return (ret);
}
#endif

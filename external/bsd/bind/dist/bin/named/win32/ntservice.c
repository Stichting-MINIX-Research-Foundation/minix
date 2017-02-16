/*	$NetBSD: ntservice.c,v 1.6 2014/12/10 04:37:52 christos Exp $	*/

/*
 * Copyright (C) 2004, 2006, 2007, 2009, 2011, 2013, 2014  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* Id: ntservice.c,v 1.16 2011/01/13 08:50:29 tbox Exp  */

#include <config.h>
#include <stdio.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/log.h>

#include <named/globals.h>
#include <named/ntservice.h>
#include <named/main.h>
#include <named/server.h>

/* Handle to SCM for updating service status */
static SERVICE_STATUS_HANDLE hServiceStatus = 0;
static BOOL foreground = FALSE;
static char ConsoleTitle[128];

/*
 * Forward declarations
 */
void ServiceControl(DWORD dwCtrlCode);
int bindmain(int, char *[]); /* From main.c */

/*
 * Initialize the Service by registering it.
 */
void
ntservice_init(void) {
	if (!foreground) {
		/* Register handler with the SCM */
		hServiceStatus = RegisterServiceCtrlHandler(BIND_SERVICE_NAME,
					(LPHANDLER_FUNCTION)ServiceControl);
		if (!hServiceStatus) {
			ns_main_earlyfatal(
				"could not register service control handler");
			UpdateSCM(SERVICE_STOPPED);
			exit(1);
		}
		UpdateSCM(SERVICE_RUNNING);
	} else {
		strcpy(ConsoleTitle, "BIND Version ");
		strcat(ConsoleTitle, VERSION);
		SetConsoleTitle(ConsoleTitle);
	}
}

void
ntservice_shutdown(void) {
	UpdateSCM(SERVICE_STOPPED);
}
/*
 * Routine to check if this is a service or a foreground program
 */
BOOL
ntservice_isservice(void) {
	return(!foreground);
}
/*
 * ServiceControl(): Handles requests from the SCM and passes them on
 * to named.
 */
void
ServiceControl(DWORD dwCtrlCode) {
	/* Handle the requested control code */
	switch(dwCtrlCode) {
	case SERVICE_CONTROL_INTERROGATE:
		UpdateSCM(0);
		break;

	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		ns_server_flushonshutdown(ns_g_server, ISC_TRUE);
		isc_app_shutdown();
		UpdateSCM(SERVICE_STOPPED);
		break;
	default:
		break;
	}
}

/*
 * Tell the Service Control Manager the state of the service.
 */
void UpdateSCM(DWORD state) {
	SERVICE_STATUS ss;
	static DWORD dwState = SERVICE_STOPPED;

	if (hServiceStatus) {
		if (state)
			dwState = state;

		memset(&ss, 0, sizeof(SERVICE_STATUS));
		ss.dwServiceType |= SERVICE_WIN32_OWN_PROCESS;
		ss.dwCurrentState = dwState;
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP |
					SERVICE_ACCEPT_SHUTDOWN;
		ss.dwCheckPoint = 0;
		ss.dwServiceSpecificExitCode = 0;
		ss.dwWin32ExitCode = NO_ERROR;
		ss.dwWaitHint = dwState == SERVICE_STOP_PENDING ? 10000 : 1000;

		if (!SetServiceStatus(hServiceStatus, &ss)) {
			ss.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(hServiceStatus, &ss);
		}
	}
}

/* unhook main */

#undef main

/*
 * This is the entry point for the executable
 * We can now call bindmain() explicitly or via StartServiceCtrlDispatcher()
 * as we need to.
 */
int main(int argc, char *argv[])
{
	int rc, ch;

	/* Command line users should put -f in the options. */
	isc_commandline_errprint = ISC_FALSE;
	while ((ch = isc_commandline_parse(argc, argv,
					   "46c:C:d:D:E:fFgi:lm:n:N:p:P:"
					   "sS:t:T:U:u:vVx:")) != -1) {
		switch (ch) {
		case 'f':
		case 'g':
		case 'v':
		case 'V':
			foreground = TRUE;
			break;
		default:
			break;
		}
	}
	isc_commandline_reset = ISC_TRUE;

	if (foreground) {
		/* run in console window */
		exit(bindmain(argc, argv));
	} else {
		/* Start up as service */
		char *SERVICE_NAME = BIND_SERVICE_NAME;

		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{ TEXT(SERVICE_NAME),
			  (LPSERVICE_MAIN_FUNCTION)bindmain },
			{ NULL, NULL }
		};

		rc = StartServiceCtrlDispatcher(dispatchTable);
		if (!rc) {
			fprintf(stderr,
				"Use -f to run from the command line.\n");
			/* will be 1063 when launched as a console app */
			exit(GetLastError());
		}
	}
	exit(0);
}

/*
 * Copyright (C) 2004, 2005, 2007-2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: os.c,v 1.37 2009-08-05 18:43:37 each Exp $ */

#include <config.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include <isc/print.h>
#include <isc/result.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/ntpaths.h>
#include <isc/util.h>
#include <isc/win32os.h>

#include <named/main.h>
#include <named/log.h>
#include <named/os.h>
#include <named/globals.h>
#include <named/ntservice.h>


static char *pidfile = NULL;
static int devnullfd = -1;

static BOOL Initialized = FALSE;

static char *version_error =
	"named requires Windows 2000 Service Pack 2 or later to run correctly";

void
ns_paths_init() {
	if (!Initialized)
		isc_ntpaths_init();

	ns_g_conffile = isc_ntpaths_get(NAMED_CONF_PATH);
	lwresd_g_conffile = isc_ntpaths_get(LWRES_CONF_PATH);
	lwresd_g_resolvconffile = isc_ntpaths_get(RESOLV_CONF_PATH);
	ns_g_conffile = isc_ntpaths_get(NAMED_CONF_PATH);
	ns_g_defaultpidfile = isc_ntpaths_get(NAMED_PID_PATH);
	lwresd_g_defaultpidfile = isc_ntpaths_get(LWRESD_PID_PATH);
	ns_g_keyfile = isc_ntpaths_get(RNDC_KEY_PATH);
	ns_g_defaultsessionkeyfile = isc_ntpaths_get(SESSION_KEY_PATH);

	Initialized = TRUE;
}

/*
 * Due to Knowledge base article Q263823 we need to make sure that
 * Windows 2000 systems have Service Pack 2 or later installed and
 * warn when it isn't.
 */
static void
version_check(const char *progname) {

	if(isc_win32os_majorversion() < 5)
		return;	/* No problem with Version 4.0 */
	if(isc_win32os_versioncheck(5, 0, 2, 0) < 0)
		if (ntservice_isservice())
			NTReportError(progname, version_error);
		else
			fprintf(stderr, "%s\n", version_error);
}

static void
setup_syslog(const char *progname) {
	int options;

	options = LOG_PID;
#ifdef LOG_NDELAY
	options |= LOG_NDELAY;
#endif

	openlog(progname, options, LOG_DAEMON);
}

void
ns_os_init(const char *progname) {
	ns_paths_init();
	setup_syslog(progname);
	/*
	 * XXXMPA. We may need to split ntservice_init() in two and
	 * just mark as running in ns_os_started().  If we do that
	 * this is where the first part of ntservice_init() should be
	 * called from.
	 *
	 * XXX970 Remove comment if no problems by 9.7.0.
	 *
	 * ntservice_init();
	 */
	version_check(progname);
}

void
ns_os_daemonize(void) {
	/*
	 * Try to set stdin, stdout, and stderr to /dev/null, but press
	 * on even if it fails.
	 */
	if (devnullfd != -1) {
		if (devnullfd != _fileno(stdin)) {
			close(_fileno(stdin));
			(void)_dup2(devnullfd, _fileno(stdin));
		}
		if (devnullfd != _fileno(stdout)) {
			close(_fileno(stdout));
			(void)_dup2(devnullfd, _fileno(stdout));
		}
		if (devnullfd != _fileno(stderr)) {
			close(_fileno(stderr));
			(void)_dup2(devnullfd, _fileno(stderr));
		}
	}
}

void
ns_os_opendevnull(void) {
	devnullfd = open("NUL", O_RDWR, 0);
}

void
ns_os_closedevnull(void) {
	if (devnullfd != _fileno(stdin) &&
	    devnullfd != _fileno(stdout) &&
	    devnullfd != _fileno(stderr)) {
		close(devnullfd);
		devnullfd = -1;
	}
}

void
ns_os_chroot(const char *root) {
	if (root != NULL)
		ns_main_earlyfatal("chroot(): isn't supported by Win32 API");
}

void
ns_os_inituserinfo(const char *username) {
}

void
ns_os_changeuser(void) {
}

void
ns_os_adjustnofile(void) {
}

void
ns_os_minprivs(void) {
}

static int
safe_open(const char *filename, int mode, isc_boolean_t append) {
	int fd;
	struct stat sb;

	if (stat(filename, &sb) == -1) {
		if (errno != ENOENT)
			return (-1);
	} else if ((sb.st_mode & S_IFREG) == 0)
		return (-1);

	if (append)
		fd = open(filename, O_WRONLY|O_CREAT|O_APPEND, mode);
	else {
		(void)unlink(filename);
		fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, mode);
	}
	return (fd);
}

static void
cleanup_pidfile(void) {
	if (pidfile != NULL) {
		(void)unlink(pidfile);
		free(pidfile);
	}
	pidfile = NULL;
}

FILE *
ns_os_openfile(const char *filename, int mode, isc_boolean_t switch_user) {
	char strbuf[ISC_STRERRORSIZE];
	FILE *fp;
	int fd;

	UNUSED(switch_user);
	fd = safe_open(filename, mode, ISC_FALSE);
	if (fd < 0) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlywarning("could not open file '%s': %s",
				     filename, strbuf);
	}

	fp = fdopen(fd, "w");
	if (fp == NULL) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		ns_main_earlywarning("could not fdopen() file '%s': %s",
				     filename, strbuf);
		close(fd);
	}

	return (fp);
}

void
ns_os_writepidfile(const char *filename, isc_boolean_t first_time) {
	FILE *lockfile;
	pid_t pid;
	char strbuf[ISC_STRERRORSIZE];
	void (*report)(const char *, ...);

	/*
	 * The caller must ensure any required synchronization.
	 */

	report = first_time ? ns_main_earlyfatal : ns_main_earlywarning;

	cleanup_pidfile();

	if (filename == NULL)
		return;

	pidfile = strdup(filename);
	if (pidfile == NULL) {
		isc__strerror(errno, strbuf, sizeof(strbuf));
		(*report)("couldn't strdup() '%s': %s", filename, strbuf);
		return;
	}

	lockfile = ns_os_openfile(filename, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH,
				  ISC_FALSE);
	if (lockfile == NULL) {
		free(pidfile);
		pidfile = NULL;
		return;
	}

	pid = getpid();

	if (fprintf(lockfile, "%ld\n", (long)pid) < 0) {
		(*report)("fprintf() to pid file '%s' failed", filename);
		(void)fclose(lockfile);
		cleanup_pidfile();
		return;
	}
	if (fflush(lockfile) == EOF) {
		(*report)("fflush() to pid file '%s' failed", filename);
		(void)fclose(lockfile);
		cleanup_pidfile();
		return;
	}
	(void)fclose(lockfile);
}

void
ns_os_shutdown(void) {
	closelog();
	cleanup_pidfile();
	ntservice_shutdown();	/* This MUST be the last thing done */
}

isc_result_t
ns_os_gethostname(char *buf, size_t len) {
	int n;

	n = gethostname(buf, len);
	return ((n == 0) ? ISC_R_SUCCESS : ISC_R_FAILURE);
}

void
ns_os_shutdownmsg(char *command, isc_buffer_t *text) {
	UNUSED(command);
	UNUSED(text);
}

void
ns_os_tzset(void) {
#ifdef HAVE_TZSET
	tzset();
#endif
}

void
ns_os_started(void) {
	ntservice_init();
}

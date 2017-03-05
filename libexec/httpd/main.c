/*	$NetBSD: main.c,v 1.8 2014/07/16 07:41:43 mrg Exp $	*/

/*	$eterna: main.c,v 1.6 2011/11/18 09:21:15 mrg Exp $	*/
/* from: eterna: bozohttpd.c,v 1.159 2009/05/23 02:14:30 mrg Exp 	*/

/*
 * Copyright (c) 1997-2014 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer and
 *    dedication in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* this program is dedicated to the Great God of Processed Cheese */

/*
 * main.c:  C front end to bozohttpd
 */

#include <sys/types.h>
#include <sys/param.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "bozohttpd.h"

/* variables and functions */
#ifndef LOG_FTP
#define LOG_FTP LOG_DAEMON
#endif

/* print a usage message, and then exit */
BOZO_DEAD static void
usage(bozohttpd_t *httpd, char *progname)
{
	bozo_warn(httpd, "usage: %s [options] slashdir [virtualhostname]",
			progname);
	bozo_warn(httpd, "options:");
#ifndef NO_DEBUG
	bozo_warn(httpd, "   -d\t\t\tenable debug support");
#endif
	bozo_warn(httpd, "   -s\t\t\talways log to stderr");
#ifndef NO_USER_SUPPORT
	bozo_warn(httpd, "   -u\t\t\tenable ~user/public_html support");
	bozo_warn(httpd, "   -p dir\t\tchange `public_html' directory name]");
#endif
#ifndef NO_DYNAMIC_CONTENT
	bozo_warn(httpd, "   -M arg t c c11\tadd this mime extenstion");
#endif
#ifndef NO_CGIBIN_SUPPORT
#ifndef NO_DYNAMIC_CONTENT
	bozo_warn(httpd, "   -C arg prog\t\tadd this CGI handler");
#endif
	bozo_warn(httpd,
		"   -c cgibin\t\tenable cgi-bin support in this directory");
#endif
#ifndef NO_LUA_SUPPORT
	bozo_warn(httpd, "   -L arg script\tadd this Lua script");
#endif
	bozo_warn(httpd, "   -I port\t\tbind or use on this port");
#ifndef NO_DAEMON_MODE
	bozo_warn(httpd, "   -b\t\t\tbackground and go into daemon mode");
	bozo_warn(httpd, "   -f\t\t\tkeep daemon mode in the foreground");
	bozo_warn(httpd,
		"   -i address\t\tbind on this address (daemon mode only)");
	bozo_warn(httpd, "   -P pidfile\t\tpath to the pid file to create");
#endif
	bozo_warn(httpd, "   -S version\t\tset server version string");
	bozo_warn(httpd, "   -t dir\t\tchroot to `dir'");
	bozo_warn(httpd, "   -U username\t\tchange user to `user'");
	bozo_warn(httpd,
		"   -e\t\t\tdon't clean the environment (-t and -U only)");
	bozo_warn(httpd,
		"   -v virtualroot\tenable virtual host support "
		"in this directory");
	bozo_warn(httpd,
		"   -r\t\t\tmake sure sub-pages come from "
		"this host via referrer");
#ifndef NO_DIRINDEX_SUPPORT
	bozo_warn(httpd,
		"   -X\t\t\tenable automatic directory index support");
	bozo_warn(httpd,
		"   -H\t\t\thide files starting with a period (.)"
		" in index mode");
#endif
	bozo_warn(httpd,
		"   -x index\t\tchange default `index.html' file name");
#ifndef NO_SSL_SUPPORT
	bozo_warn(httpd,
		"   -Z cert privkey\tspecify path to server certificate"
			" and private key file\n"
		"\t\t\tin pem format and enable bozohttpd in SSL mode");
#endif /* NO_SSL_SUPPORT */
	bozo_err(httpd, 1, "%s failed to start", progname);
}

int
main(int argc, char **argv)
{
	bozo_httpreq_t	*request;
	bozohttpd_t	 httpd;
	bozoprefs_t	 prefs;
	char		*progname;
	int		 c;

	(void) memset(&httpd, 0x0, sizeof(httpd));
	(void) memset(&prefs, 0x0, sizeof(prefs));

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		progname++;

	openlog(progname, LOG_PID|LOG_NDELAY, LOG_FTP);

	bozo_set_defaults(&httpd, &prefs);

	while ((c = getopt(argc, argv,
	    "C:HI:L:M:P:S:U:VXZ:bc:defhi:np:rst:uv:x:z:")) != -1) {
		switch(c) {

		case 'L':
#ifdef NO_LUA_SUPPORT
			bozo_err(&httpd, 1,
				"Lua support is not enabled");
			/* NOTREACHED */
#else
			/* make sure there's two argument */
			if (argc - optind < 1)
				usage(&httpd, progname);
			bozo_add_lua_map(&httpd, optarg, argv[optind]);
			optind++;
			break;
#endif /* NO_LUA_SUPPORT */
		case 'M':
#ifdef NO_DYNAMIC_CONTENT
			bozo_err(&httpd, 1,
				"dynamic mime content support is not enabled");
			/* NOTREACHED */
#else
			/* make sure there's four arguments */
			if (argc - optind < 3)
				usage(&httpd, progname);
			bozo_add_content_map_mime(&httpd, optarg, argv[optind],
			    argv[optind+1], argv[optind+2]);
			optind += 3;
			break;
#endif /* NO_DYNAMIC_CONTENT */

		case 'n':
			bozo_set_pref(&prefs, "numeric", "true");
			break;

		case 'r':
			bozo_set_pref(&prefs, "trusted referal", "true");
			break;

		case 's':
			bozo_set_pref(&prefs, "log to stderr", "true");
			break;

		case 'S':
			bozo_set_pref(&prefs, "server software", optarg);
			break;
		case 'Z':
#ifdef NO_SSL_SUPPORT
			bozo_err(&httpd, 1, "ssl support is not enabled");
			/* NOT REACHED */
#else
			/* make sure there's two arguments */
			if (argc - optind < 1)
				usage(&httpd, progname);
			bozo_ssl_set_opts(&httpd, optarg, argv[optind++]);
			break;
#endif /* NO_SSL_SUPPORT */
		case 'U':
			bozo_set_pref(&prefs, "username", optarg);
			break;

		case 'V':
			bozo_set_pref(&prefs, "unknown slash", "true");
			break;

		case 'v':
			bozo_set_pref(&prefs, "virtual base", optarg);
			break;

		case 'x':
			bozo_set_pref(&prefs, "index.html", optarg);
			break;

		case 'I':
			bozo_set_pref(&prefs, "port number", optarg);
			break;

#ifdef NO_DAEMON_MODE
		case 'b':
		case 'e':
		case 'f':
		case 'i':
		case 'P':
			bozo_err(&httpd, 1, "Daemon mode is not enabled");
			/* NOTREACHED */
#else
		case 'b':
			/*
			 * test suite support - undocumented
			 * background == 2 (aka, -b -b) means to
			 * only process 1 per kid
			 */
			if (bozo_get_pref(&prefs, "background") == NULL) {
				bozo_set_pref(&prefs, "background", "1");
			} else {
				bozo_set_pref(&prefs, "background", "2");
			}
			break;

		case 'e':
			bozo_set_pref(&prefs, "dirty environment", "true");
			break;

		case 'f':
			bozo_set_pref(&prefs, "foreground", "true");
			break;

		case 'i':
			bozo_set_pref(&prefs, "bind address", optarg);
			break;

		case 'P':
			bozo_set_pref(&prefs, "pid file", optarg);
			break;
#endif /* NO_DAEMON_MODE */

#ifdef NO_CGIBIN_SUPPORT
		case 'c':
		case 'C':
			bozo_err(&httpd, 1, "CGI is not enabled");
			/* NOTREACHED */
#else
		case 'c':
			bozo_cgi_setbin(&httpd, optarg);
			break;

		case 'C':
#  ifdef NO_DYNAMIC_CONTENT
			bozo_err(&httpd, 1,
				"dynamic CGI handler support is not enabled");
			/* NOTREACHED */
#  else
			/* make sure there's two arguments */
			if (argc - optind < 1)
				usage(&httpd, progname);
			bozo_add_content_map_cgi(&httpd, optarg,
					argv[optind++]);
			break;
#  endif /* NO_DYNAMIC_CONTENT */
#endif /* NO_CGIBIN_SUPPORT */

		case 'd':
			httpd.debug++;
#ifdef NO_DEBUG
			if (httpd.debug == 1)
				bozo_warn(&httpd, "Debugging is not enabled");
#endif /* NO_DEBUG */
			break;

		case 't':
			bozo_set_pref(&prefs, "chroot dir", optarg);
			break;

#ifdef NO_USER_SUPPORT
		case 'p':
		case 'u':
			bozo_err(&httpd, 1, "User support is not enabled");
			/* NOTREACHED */
#else
		case 'p':
			bozo_set_pref(&prefs, "public_html", optarg);
			break;

		case 'u':
			bozo_set_pref(&prefs, "enable users", "true");
			break;
#endif /* NO_USER_SUPPORT */

#ifdef NO_DIRINDEX_SUPPORT
		case 'H':
		case 'X':
			bozo_err(&httpd, 1,
				"directory indexing is not enabled");
			/* NOTREACHED */
#else
		case 'H':
			bozo_set_pref(&prefs, "hide dots", "true");
			break;

		case 'X':
			bozo_set_pref(&prefs, "directory indexing", "true");
			break;

#endif /* NO_DIRINDEX_SUPPORT */

		default:
			usage(&httpd, progname);
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0 || argc > 2) {
		usage(&httpd, progname);
	}

	/* virtual host, and root of tree to serve */
	bozo_setup(&httpd, &prefs, argv[1], argv[0]);

	/*
	 * read and process the HTTP request.
	 */
	do {
		if ((request = bozo_read_request(&httpd)) != NULL) {
			bozo_process_request(request);
			bozo_clean_request(request);
		}
	} while (httpd.background);

	return (0);
}

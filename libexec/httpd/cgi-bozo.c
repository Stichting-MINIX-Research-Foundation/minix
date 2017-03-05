/*	$NetBSD: cgi-bozo.c,v 1.27 2015/05/02 11:35:48 mrg Exp $	*/

/*	$eterna: cgi-bozo.c,v 1.40 2011/11/18 09:21:15 mrg Exp $	*/

/*
 * Copyright (c) 1997-2015 Matthew R. Green
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

/* this code implements CGI/1.2 for bozohttpd */

#ifndef NO_CGIBIN_SUPPORT

#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <netinet/in.h>

#include "bozohttpd.h"

#define CGIBIN_PREFIX		"cgi-bin/"
#define CGIBIN_PREFIX_LEN	(sizeof(CGIBIN_PREFIX)-1)

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

/*
 * given the file name, return a CGI interpreter
 */
static const char *
content_cgihandler(bozohttpd_t *httpd, bozo_httpreq_t *request,
		const char *file)
{
	bozo_content_map_t	*map;

	USE_ARG(request);
	debug((httpd, DEBUG_FAT, "content_cgihandler: trying file %s", file));
	map = bozo_match_content_map(httpd, file, 0);
	if (map)
		return map->cgihandler;
	return NULL;
}

static int
parse_header(bozohttpd_t *httpd, const char *str, ssize_t len, char **hdr_str,
		char **hdr_val)
{
	char	*name, *value;

	/* if the string passed is zero-length bail out */
	if (*str == '\0')
		return -1;

	value = bozostrdup(httpd, str);

	/* locate the ':' separator in the header/value */
	name = bozostrnsep(&value, ":", &len);

	if (NULL == name || -1 == len) {
		free(name);
		return -1;
	}

	/* skip leading space/tab */
	while (*value == ' ' || *value == '\t')
		len--, value++;

	*hdr_str = name;
	*hdr_val = value;

	return 0;
} 

/*
 * handle parsing a CGI header output, transposing a Status: header
 * into the HTTP reply (ie, instead of "200 OK").
 */
static void
finish_cgi_output(bozohttpd_t *httpd, bozo_httpreq_t *request, int in, int nph)
{
	char	buf[BOZO_WRSZ];
	char	*str;
	ssize_t	len;
	ssize_t rbytes;
	SIMPLEQ_HEAD(, bozoheaders)	headers;
	bozoheaders_t *hdr, *nhdr;
	int	write_header, nheaders = 0;

	/* much of this code is like bozo_read_request()'s header loop. */
	SIMPLEQ_INIT(&headers);
	write_header = nph == 0;
	/* was read(2) here - XXX - agc */
	while (nph == 0 &&
		(str = bozodgetln(httpd, in, &len, bozo_read)) != NULL) {
		char	*hdr_name, *hdr_value;

		if (parse_header(httpd, str, len, &hdr_name, &hdr_value))
			break;

		/*
		 * The CGI 1.{1,2} spec both say that if the cgi program
		 * returns a `Status:' header field then the server MUST
		 * return it in the response.  If the cgi program does
		 * not return any `Status:' header then the server should
		 * respond with 200 OK.
		 * XXX The CGI 1.1 and 1.2 specification differ slightly on
		 * this in that v1.2 says that the script MUST NOT return a
		 * `Status:' header if it is returning a `Location:' header.
		 * For compatibility we are going with the CGI 1.1 behavior.
		 */
		if (strcasecmp(hdr_name, "status") == 0) {
			debug((httpd, DEBUG_OBESE,
				"bozo_process_cgi:  writing HTTP header "
				"from status %s ..", hdr_value));
			bozo_printf(httpd, "%s %s\r\n", request->hr_proto,
					hdr_value);
			bozo_flush(httpd, stdout);
			write_header = 0;
			free(hdr_name);
			break;
		}

		hdr = bozomalloc(httpd, sizeof *hdr);
		hdr->h_header = hdr_name;
		hdr->h_value = hdr_value;
		SIMPLEQ_INSERT_TAIL(&headers, hdr, h_next);
		nheaders++;
	}

	if (write_header) {
		debug((httpd, DEBUG_OBESE,
			"bozo_process_cgi:  writing HTTP header .."));
		bozo_printf(httpd,
			"%s 200 OK\r\n", request->hr_proto);
		bozo_flush(httpd, stdout);
	}

	if (nheaders) {
		debug((httpd, DEBUG_OBESE,
			"bozo_process_cgi:  writing delayed HTTP headers .."));
		SIMPLEQ_FOREACH_SAFE(hdr, &headers, h_next, nhdr) {
			bozo_printf(httpd, "%s: %s\r\n", hdr->h_header,
					hdr->h_value);
			free(hdr->h_header);
			free(hdr);
		}
		bozo_printf(httpd, "\r\n");
		bozo_flush(httpd, stdout);
	}

	/* XXX we should have some goo that times us out
	 */
	while ((rbytes = read(in, buf, sizeof buf)) > 0) {
		ssize_t wbytes;
		char *bp = buf;

		while (rbytes) {
			wbytes = bozo_write(httpd, STDOUT_FILENO, buf,
						(size_t)rbytes);
			if (wbytes > 0) {
				rbytes -= wbytes;
				bp += wbytes;
			} else
				bozo_err(httpd, 1,
					"cgi output write failed: %s",
					strerror(errno));
		}		
	}
}

static void
append_index_html(bozohttpd_t *httpd, char **url)
{
	*url = bozorealloc(httpd, *url,
			strlen(*url) + strlen(httpd->index_html) + 1);
	strcat(*url, httpd->index_html);
	debug((httpd, DEBUG_NORMAL,
		"append_index_html: url adjusted to `%s'", *url));
}

void
bozo_cgi_setbin(bozohttpd_t *httpd, const char *path)
{
	httpd->cgibin = strdup(path);
	debug((httpd, DEBUG_OBESE, "cgibin (cgi-bin directory) is %s",
		httpd->cgibin));
}

/* help build up the environ pointer */
void
bozo_setenv(bozohttpd_t *httpd, const char *env, const char *val,
		char **envp)
{
	char *s1 = bozomalloc(httpd, strlen(env) + strlen(val) + 2);

	strcpy(s1, env);
	strcat(s1, "=");
	strcat(s1, val);
	debug((httpd, DEBUG_OBESE, "bozo_setenv: %s", s1));
	*envp = s1;
}

/*
 * Checks if the request has asked for a cgi-bin.  Should only be called if
 * cgibin is set.  If it starts CGIBIN_PREFIX or has a ncontent handler,
 * process the cgi, otherwise just return.  Returns 0 if it did not handle
 * the request.
 */
int
bozo_process_cgi(bozo_httpreq_t *request)
{
	bozohttpd_t *httpd = request->hr_httpd;
	char	buf[BOZO_WRSZ];
	char	date[40];
	bozoheaders_t *headp;
	const char *type, *clen, *info, *cgihandler;
	char	*query, *s, *t, *path, *env, *file, *url;
	char	command[MAXPATHLEN];
	char	**envp, **curenvp, *argv[4];
	char	*uri;
	size_t	len;
	ssize_t rbytes;
	pid_t	pid;
	int	envpsize, ix, nph;
	int	sv[2];

	if (!httpd->cgibin && !httpd->process_cgi)
		return 0;

	if (request->hr_oldfile && strcmp(request->hr_oldfile, "/") != 0)
		uri = request->hr_oldfile;
	else
		uri = request->hr_file;

	if (uri[0] == '/')
		file = bozostrdup(httpd, uri);
	else
		asprintf(&file, "/%s", uri);
	if (file == NULL)
		return 0;

	if (request->hr_query && strlen(request->hr_query))
		query = bozostrdup(httpd, request->hr_query);
	else
		query = NULL;

	asprintf(&url, "%s%s%s", file, query ? "?" : "", query ? query : "");
	if (url == NULL)
		goto out;
	debug((httpd, DEBUG_NORMAL, "bozo_process_cgi: url `%s'", url));

	path = NULL;
	envp = NULL;
	cgihandler = NULL;
	info = NULL;

	len = strlen(url);

	if (bozo_auth_check(request, url + 1))
		goto out;

	if (!httpd->cgibin ||
	    strncmp(url + 1, CGIBIN_PREFIX, CGIBIN_PREFIX_LEN) != 0) {
		cgihandler = content_cgihandler(httpd, request, file + 1);
		if (cgihandler == NULL) {
			debug((httpd, DEBUG_FAT,
				"bozo_process_cgi: no handler, returning"));
			goto out;
		}
		if (len == 0 || file[len - 1] == '/')
			append_index_html(httpd, &file);
		debug((httpd, DEBUG_NORMAL, "bozo_process_cgi: cgihandler `%s'",
		    cgihandler));
	} else if (len - 1 == CGIBIN_PREFIX_LEN)	/* url is "/cgi-bin/" */
		append_index_html(httpd, &file);

	ix = 0;
	if (cgihandler) {
		snprintf(command, sizeof(command), "%s", file + 1);
		path = bozostrdup(httpd, cgihandler);
		argv[ix++] = path;
			/* argv[] = [ path, command, query, NULL ] */
	} else {
		snprintf(command, sizeof(command), "%s",
		    file + CGIBIN_PREFIX_LEN + 1);
		if ((s = strchr(command, '/')) != NULL) {
			info = bozostrdup(httpd, s);
			*s = '\0';
		}
		path = bozomalloc(httpd,
				strlen(httpd->cgibin) + 1 + strlen(command) + 1);
		strcpy(path, httpd->cgibin);
		strcat(path, "/");
		strcat(path, command);
			/* argv[] = [ command, query, NULL ] */
	}
	argv[ix++] = command;
	argv[ix++] = query;
	argv[ix++] = NULL;

	nph = strncmp(command, "nph-", 4) == 0;

	type = request->hr_content_type;
	clen = request->hr_content_length;

	envpsize = 13 + request->hr_nheaders + 
	    (info && *info ? 1 : 0) +
	    (query && *query ? 1 : 0) +
	    (type && *type ? 1 : 0) +
	    (clen && *clen ? 1 : 0) +
	    (request->hr_remotehost && *request->hr_remotehost ? 1 : 0) +
	    (request->hr_remoteaddr && *request->hr_remoteaddr ? 1 : 0) +
	    bozo_auth_cgi_count(request) +
	    (request->hr_serverport && *request->hr_serverport ? 1 : 0);

	debug((httpd, DEBUG_FAT,
		"bozo_process_cgi: path `%s', cmd `%s', info `%s', "
		"query `%s', nph `%d', envpsize `%d'",
		path, command, strornull(info),
		strornull(query), nph, envpsize));

	envp = bozomalloc(httpd, sizeof(*envp) * envpsize);
	for (ix = 0; ix < envpsize; ix++)
		envp[ix] = NULL;
	curenvp = envp;

	SIMPLEQ_FOREACH(headp, &request->hr_headers, h_next) {
		const char *s2;
		env = bozomalloc(httpd, 6 + strlen(headp->h_header) + 1 +
		    strlen(headp->h_value));

		t = env;
		strcpy(t, "HTTP_");
		t += strlen(t);
		for (s2 = headp->h_header; *s2; t++, s2++)
			if (islower((u_int)*s2))
				*t = toupper((u_int)*s2);
			else if (*s2 == '-')
				*t = '_';
			else
				*t = *s2;
		*t = '\0';
		debug((httpd, DEBUG_OBESE, "setting header %s as %s = %s",
		    headp->h_header, env, headp->h_value));
		bozo_setenv(httpd, env, headp->h_value, curenvp++);
		free(env);
	}

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH "/usr/bin:/bin"
#endif

	bozo_setenv(httpd, "PATH", _PATH_DEFPATH, curenvp++);
	bozo_setenv(httpd, "IFS", " \t\n", curenvp++);
	bozo_setenv(httpd, "SERVER_NAME", BOZOHOST(httpd,request), curenvp++);
	bozo_setenv(httpd, "GATEWAY_INTERFACE", "CGI/1.1", curenvp++);
	bozo_setenv(httpd, "SERVER_PROTOCOL", request->hr_proto, curenvp++);
	bozo_setenv(httpd, "REQUEST_METHOD", request->hr_methodstr, curenvp++);
	bozo_setenv(httpd, "SCRIPT_NAME", file, curenvp++);
	bozo_setenv(httpd, "SCRIPT_FILENAME", file + 1, curenvp++);
	bozo_setenv(httpd, "SERVER_SOFTWARE", httpd->server_software,
			curenvp++);
	bozo_setenv(httpd, "REQUEST_URI", uri, curenvp++);
	bozo_setenv(httpd, "DATE_GMT", bozo_http_date(date, sizeof(date)),
			curenvp++);
	if (query && *query)
		bozo_setenv(httpd, "QUERY_STRING", query, curenvp++);
	if (info && *info)
		bozo_setenv(httpd, "PATH_INFO", info, curenvp++);
	if (type && *type)
		bozo_setenv(httpd, "CONTENT_TYPE", type, curenvp++);
	if (clen && *clen)
		bozo_setenv(httpd, "CONTENT_LENGTH", clen, curenvp++);
	if (request->hr_serverport && *request->hr_serverport)
		bozo_setenv(httpd, "SERVER_PORT", request->hr_serverport,
				curenvp++);
	if (request->hr_remotehost && *request->hr_remotehost)
		bozo_setenv(httpd, "REMOTE_HOST", request->hr_remotehost,
				curenvp++);
	if (request->hr_remoteaddr && *request->hr_remoteaddr)
		bozo_setenv(httpd, "REMOTE_ADDR", request->hr_remoteaddr,
				curenvp++);
	/*
	 * XXX Apache does this when invoking content handlers, and PHP
	 * XXX 5.3 requires it as a "security" measure.
	 */
	if (cgihandler)
		bozo_setenv(httpd, "REDIRECT_STATUS", "200", curenvp++);
	bozo_auth_cgi_setenv(request, &curenvp);

	free(file);
	free(url);

	debug((httpd, DEBUG_FAT, "bozo_process_cgi: going exec %s, %s %s %s",
	    path, argv[0], strornull(argv[1]), strornull(argv[2])));

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sv) == -1)
		bozo_err(httpd, 1, "child socketpair failed: %s",
				strerror(errno));

	/*
	 * We create 2 procs: one to become the CGI, one read from
	 * the CGI and output to the network, and this parent will
	 * continue reading from the network and writing to the
	 * CGI procsss.
	 */
	switch (fork()) {
	case -1: /* eep, failure */
		bozo_err(httpd, 1, "child fork failed: %s", strerror(errno));
		/*NOTREACHED*/
	case 0:
		close(sv[0]);
		dup2(sv[1], STDIN_FILENO);
		dup2(sv[1], STDOUT_FILENO);
		close(2);
		close(sv[1]);
		closelog();
		bozo_daemon_closefds(httpd);

		if (-1 == execve(path, argv, envp))
			bozo_err(httpd, 1, "child exec failed: %s: %s",
			      path, strerror(errno));
		/* NOT REACHED */
		bozo_err(httpd, 1, "child execve returned?!");
	}

	close(sv[1]);

	/* parent: read from stdin (bozo_read()) write to sv[0] */
	/* child: read from sv[0] (bozo_write()) write to stdout */
	pid = fork();
	if (pid == -1)
		bozo_err(httpd, 1, "io child fork failed: %s", strerror(errno));
	else if (pid == 0) {
		/* child reader/writer */
		close(STDIN_FILENO);
		finish_cgi_output(httpd, request, sv[0], nph);
		/* if we're done output, our parent is useless... */
		kill(getppid(), SIGKILL);
		debug((httpd, DEBUG_FAT, "done processing cgi output"));
		_exit(0);
	}
	close(STDOUT_FILENO);

	/* XXX we should have some goo that times us out
	 */
	while ((rbytes = bozo_read(httpd, STDIN_FILENO, buf, sizeof buf)) > 0) {
		ssize_t wbytes;
		char *bp = buf;

		while (rbytes) {
			wbytes = write(sv[0], buf, (size_t)rbytes);
			if (wbytes > 0) {
				rbytes -= wbytes;
				bp += wbytes;
			} else
				bozo_err(httpd, 1, "write failed: %s",
					strerror(errno));
		}		
	}
	debug((httpd, DEBUG_FAT, "done processing cgi input"));
	exit(0);

 out:
	free(query);
	free(file);
	free(url);
	return 0;
}

#ifndef NO_DYNAMIC_CONTENT
/* cgi maps are simple ".postfix /path/to/prog" */
void
bozo_add_content_map_cgi(bozohttpd_t *httpd, const char *arg, const char *cgihandler)
{
	bozo_content_map_t *map;

	debug((httpd, DEBUG_NORMAL, "bozo_add_content_map_cgi: name %s cgi %s",
		arg, cgihandler));

	httpd->process_cgi = 1;

	map = bozo_get_content_map(httpd, arg);
	map->name = arg;
	map->type = map->encoding = map->encoding11 = NULL;
	map->cgihandler = cgihandler;
}
#endif /* NO_DYNAMIC_CONTENT */

#endif /* NO_CGIBIN_SUPPORT */

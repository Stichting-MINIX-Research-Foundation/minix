/*	$NetBSD: tilde-luzah-bozo.c,v 1.11 2015/07/16 12:19:23 shm Exp $	*/

/*	$eterna: tilde-luzah-bozo.c,v 1.16 2011/11/18 09:21:15 mrg Exp $	*/

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

/* this code implements ~user support for bozohttpd */

#ifndef NO_USER_SUPPORT

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bozohttpd.h"

/*
 * bozo_user_transform does this:
 *	- chdir's /~user/public_html
 *	- returns the rest of the file, index.html appended if required
 *	- returned malloced file to serve in request->hr_file,
 *        ala transform_request().
 *
 * transform_request() is supposed to check that we have user support
 * enabled.
 */
int
bozo_user_transform(bozo_httpreq_t *request, int *isindex)
{
	bozohttpd_t *httpd = request->hr_httpd;
	char	c, *s, *file = NULL, *user;
	struct	passwd *pw;

	*isindex = 0;

	/* find username */
	user = strchr(request->hr_file + 2, '~');

	/* this shouldn't happen, but "better paranoid than sorry" */
	assert(user != NULL);
	
	user++;

	if ((s = strchr(user, '/')) != NULL) {
		*s++ = '\0';
		c = s[strlen(s)-1];
		*isindex = (c == '/' || c == '\0');
	}

	debug((httpd, DEBUG_OBESE, "looking for user %s",
		user));
	pw = getpwnam(user);
	/* fix this up immediately */
	if (s)
		s[-1] = '/';
	if (pw == NULL) {
		(void)bozo_http_error(httpd, 404, request, "no such user");
		return 0;
	}

	debug((httpd, DEBUG_OBESE, "user %s dir %s/%s uid %d gid %d",
	      pw->pw_name, pw->pw_dir, httpd->public_html,
	      pw->pw_uid, pw->pw_gid));

	if (chdir(pw->pw_dir) < 0) {
		bozo_warn(httpd, "chdir1 error: %s: %s", pw->pw_dir,
			strerror(errno));
		(void)bozo_http_error(httpd, 404, request,
			"can't chdir to homedir");
		return 0;
	}
	if (chdir(httpd->public_html) < 0) {
		bozo_warn(httpd, "chdir2 error: %s: %s", httpd->public_html,
			strerror(errno));
		(void)bozo_http_error(httpd, 404, request,
			"can't chdir to public_html");
		return 0;
	}
	if (s == NULL || *s == '\0') {
		file = bozostrdup(httpd, httpd->index_html);
	} else {
		file = bozomalloc(httpd, strlen(s) +
		    (*isindex ? strlen(httpd->index_html) + 1 : 1));
		strcpy(file, s);
		if (*isindex)
			strcat(file, httpd->index_html);
	}

	/* see transform_request() */
	if (*file == '/' || strcmp(file, "..") == 0 ||
	    strstr(file, "/..") || strstr(file, "../")) {
		(void)bozo_http_error(httpd, 403, request, "illegal request");
		free(file);
		return 0;
	}

	if (bozo_auth_check(request, file)) {
		free(file);
		return 0;
	}

	free(request->hr_file);
	request->hr_file = file;

	debug((httpd, DEBUG_FAT, "transform_user returning %s under %s", file,
	    pw->pw_dir));
	return 1;
}
#endif /* NO_USER_SUPPORT */

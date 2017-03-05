/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@netbsd.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <bozohttpd.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define LUA_LIB
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifndef __UNCONST
#define __UNCONST(a) ((void *)(unsigned long)(const void *)(a))
#endif /* !__UNCONST */

int luaopen_bozohttpd(lua_State *);

#if 0
typedef struct strarg_t {
	const char	*s;	/* string */
	const int	 n;	/* corresponding int value */
} strarg_t;

/* map a string onto an int */
static int
findtype(strarg_t *strs, const char *s)
{
	strarg_t	*sp;

	for (sp = strs ; sp->s && strcasecmp(sp->s, s) != 0 ; sp++) {
	}
	return sp->n;
}
#endif

/* init() */
static int
l_new(lua_State *L)
{
	bozohttpd_t	*httpd;

	httpd = lua_newuserdata(L, sizeof(*httpd));
	(void) memset(httpd, 0x0, sizeof(*httpd));
	return 1;
}

/* initialise(httpd) */
static int
l_init_httpd(lua_State *L)
{
	bozohttpd_t	*httpd;

	httpd = lua_touserdata(L, 1);
	lua_pushnumber(L, bozo_init_httpd(httpd));
	return 1;
}

/* initialise(prefs) */
static int
l_init_prefs(lua_State *L)
{
	bozoprefs_t	*prefs;

	prefs = lua_newuserdata(L, sizeof(*prefs));
	(void) memset(prefs, 0x0, sizeof(*prefs));
	(void) bozo_init_prefs(prefs);
	return 1;
}

/* bozo_set_pref(prefs, name, value) */
static int
l_bozo_set_pref(lua_State *L)
{
	bozoprefs_t	*prefs;
	const char	*name;
	const char	*value;

	prefs = lua_touserdata(L, 1);
	name = luaL_checkstring(L, 2);
	value = luaL_checkstring(L, 3);
	lua_pushnumber(L, bozo_set_pref(prefs, name, value));
	return 1;
}

/* bozo_get_pref(prefs, name) */
static int
l_bozo_get_pref(lua_State *L)
{
	bozoprefs_t	*prefs;
	const char	*name;

	prefs = lua_touserdata(L, 1);
	name = luaL_checkstring(L, 2);
	lua_pushstring(L, bozo_get_pref(prefs, name));
	return 1;
}

/* bozo_setup(httpd, prefs, host, root) */
static int
l_bozo_setup(lua_State *L)
{
	bozohttpd_t	*httpd;
	bozoprefs_t	*prefs;
	const char	*vhost;
	const char	*root;

	httpd = lua_touserdata(L, 1);
	prefs = lua_touserdata(L, 2);
	vhost = luaL_checkstring(L, 3);
	if (vhost && *vhost == 0x0) {
		vhost = NULL;
	}
	root = luaL_checkstring(L, 4);
	lua_pushnumber(L, bozo_setup(httpd, prefs, vhost, root));
	return 1;
}

/* bozo_read_request(httpd) */
static int
l_bozo_read_request(lua_State *L)
{
	bozo_httpreq_t	*req;
	bozohttpd_t	*httpd;

	httpd = lua_touserdata(L, 1);
	req = bozo_read_request(httpd);
	lua_pushlightuserdata(L, req);
	return 1;
}

/* bozo_process_request(httpd, req) */
static int
l_bozo_process_request(lua_State *L)
{
	bozo_httpreq_t	*req;
	bozohttpd_t	*httpd;

	httpd = lua_touserdata(L, 1);
	req = lua_touserdata(L, 2);
	bozo_process_request(httpd, req);
	lua_pushnumber(L, 1);
	return 1;
}

/* bozo_clean_request(req) */
static int
l_bozo_clean_request(lua_State *L)
{
	bozo_httpreq_t	*req;

	req = lua_touserdata(L, 1);
	bozo_clean_request(req);
	lua_pushnumber(L, 1);
	return 1;
}

/* dynamic_mime(httpd, one, two, three, four) */
static int
l_bozo_dynamic_mime(lua_State *L)
{
	bozohttpd_t	*httpd;
	const char	*s[4];

	httpd = lua_touserdata(L, 1);
	s[0] = luaL_checkstring(L, 2);
	s[1] = luaL_checkstring(L, 3);
	s[2] = luaL_checkstring(L, 4);
	s[3] = luaL_checkstring(L, 5);
	bozo_add_content_map_mime(httpd, s[0], s[1], s[2], s[3]);
	lua_pushnumber(L, 1);
	return 1;
}

/* ssl_set_opts(httpd, one, two) */
static int
l_bozo_ssl_set_opts(lua_State *L)
{
	bozohttpd_t	*httpd;
	const char	*s[2];

	httpd = lua_touserdata(L, 1);
	s[0] = luaL_checkstring(L, 2);
	s[1] = luaL_checkstring(L, 3);
	bozo_ssl_set_opts(httpd, s[0], s[1]);
	lua_pushnumber(L, 1);
	return 1;
}

/* cgi_setbin(httpd, bin) */
static int
l_bozo_cgi_setbin(lua_State *L)
{
	bozohttpd_t	*httpd;
	const char	*bin;

	httpd = lua_touserdata(L, 1);
	bin = luaL_checkstring(L, 2);
	bozo_cgi_setbin(httpd, bin);
	lua_pushnumber(L, 1);
	return 1;
}

/* cgi_map(httpd, 1, 2) */
static int
l_bozo_cgi_map(lua_State *L)
{
	bozohttpd_t	*httpd;
	const char	*s[2];

	httpd = lua_touserdata(L, 1);
	s[0] = luaL_checkstring(L, 2);
	s[1] = luaL_checkstring(L, 3);
	bozo_add_content_map_cgi(httpd, s[0], s[1]);
	lua_pushnumber(L, 1);
	return 1;
}

const struct luaL_reg libluabozohttpd[] = {
	{ "new",		l_new },
	{ "init_httpd",		l_init_httpd },
	{ "init_prefs",		l_init_prefs },

	{ "set_pref",		l_bozo_set_pref },
	{ "get_pref",		l_bozo_get_pref },
	{ "setup",		l_bozo_setup },
	{ "dynamic_mime",	l_bozo_dynamic_mime },
	{ "ssl_set_opts",	l_bozo_ssl_set_opts },
	{ "cgi_setbin",		l_bozo_cgi_setbin },
	{ "cgi_map",		l_bozo_cgi_map },

	{ "read_request",	l_bozo_read_request },
	{ "process_request",	l_bozo_process_request },
	{ "clean_request",	l_bozo_clean_request },

	{ NULL,			NULL }
};

int 
luaopen_bozohttpd(lua_State *L)
{
	luaL_openlib(L, "bozohttpd", libluabozohttpd, 0);
	return 1;
}

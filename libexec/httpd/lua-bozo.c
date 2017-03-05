/*	$NetBSD: lua-bozo.c,v 1.12 2015/07/04 22:39:23 christos Exp $	*/

/*
 * Copyright (c) 2013 Marc Balmer <marc@msys.ch>
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

/* this code implements dynamic content generation using Lua for bozohttpd */

#ifndef NO_LUA_SUPPORT

#include <sys/param.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bozohttpd.h"

/* Lua binding for bozohttp */

#if LUA_VERSION_NUM < 502
#define LUA_HTTPDLIBNAME "httpd"
#endif

#define FORM	"application/x-www-form-urlencoded"

static int
lua_flush(lua_State *L)
{
	bozohttpd_t *httpd;

	lua_pushstring(L, "bozohttpd");
	lua_gettable(L, LUA_REGISTRYINDEX);
	httpd = lua_touserdata(L, -1);
	lua_pop(L, 1);

	bozo_flush(httpd, stdout);
	return 0;
}

static int
lua_print(lua_State *L)
{
	bozohttpd_t *httpd;

	lua_pushstring(L, "bozohttpd");
	lua_gettable(L, LUA_REGISTRYINDEX);
	httpd = lua_touserdata(L, -1);
	lua_pop(L, 1);

	bozo_printf(httpd, "%s\r\n", lua_tostring(L, -1));
	return 0;
}

static int
lua_read(lua_State *L)
{
	bozohttpd_t *httpd;
	int n, len;
	char *data;

	lua_pushstring(L, "bozohttpd");
	lua_gettable(L, LUA_REGISTRYINDEX);
	httpd = lua_touserdata(L, -1);
	lua_pop(L, 1);

	len = luaL_checkinteger(L, -1);
	data = bozomalloc(httpd, len + 1);
	n = bozo_read(httpd, STDIN_FILENO, data, len);
	if (n >= 0) {
		data[n] = '\0';
		lua_pushstring(L, data);
	} else
		lua_pushnil(L);
	free(data);
	return 1;
}

static int
lua_register_handler(lua_State *L)
{
	lua_state_map_t *map;
	lua_handler_t *handler;
	bozohttpd_t *httpd;

	lua_pushstring(L, "lua_state_map");
	lua_gettable(L, LUA_REGISTRYINDEX);
	map = lua_touserdata(L, -1);
	lua_pushstring(L, "bozohttpd");
	lua_gettable(L, LUA_REGISTRYINDEX);
	httpd = lua_touserdata(L, -1);
	lua_pop(L, 2);

	luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	handler = bozomalloc(httpd, sizeof(lua_handler_t));

	handler->name = bozostrdup(httpd, lua_tostring(L, 1));
	handler->ref = luaL_ref(L, LUA_REGISTRYINDEX);
	SIMPLEQ_INSERT_TAIL(&map->handlers, handler, h_next);
	httpd->process_lua = 1;
	return 0;
}

static int
lua_write(lua_State *L)
{
	bozohttpd_t *httpd;
	const char *data;

	lua_pushstring(L, "bozohttpd");
	lua_gettable(L, LUA_REGISTRYINDEX);
	httpd = lua_touserdata(L, -1);
	lua_pop(L, 1);

	data = luaL_checkstring(L, -1);
	lua_pushinteger(L, bozo_write(httpd, STDIN_FILENO, data, strlen(data)));
	return 1;
}

static int
luaopen_httpd(lua_State *L)
{
	struct luaL_Reg functions[] = {
		{ "flush",		lua_flush },
		{ "print",		lua_print },
		{ "read",		lua_read },
		{ "register_handler",	lua_register_handler },
		{ "write",		lua_write },
		{ NULL, NULL }
	};
#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, functions);
#else
	luaL_register(L, LUA_HTTPDLIBNAME, functions);
#endif
	lua_pushstring(L, "httpd 1.0.0");
	lua_setfield(L, -2, "_VERSION");
	return 1;
}

#if LUA_VERSION_NUM < 502
static void
lua_openlib(lua_State *L, const char *name, lua_CFunction fn)
{
	lua_pushcfunction(L, fn);
	lua_pushstring(L, name);
	lua_call(L, 1, 0);
}
#endif

/* bozohttpd integration */
void
bozo_add_lua_map(bozohttpd_t *httpd, const char *prefix, const char *script)
{
	lua_state_map_t *map;

	map = bozomalloc(httpd, sizeof(lua_state_map_t));
	map->prefix = bozostrdup(httpd, prefix);
	if (*script == '/')
		map->script = bozostrdup(httpd, script);
	else {
		char cwd[MAXPATHLEN], *path;

		getcwd(cwd, sizeof(cwd) - 1);
		asprintf(&path, "%s/%s", cwd, script);
		map->script = path;
	}
	map->L = luaL_newstate();
	if (map->L == NULL)
		bozo_err(httpd, 1, "can't create Lua state");
	SIMPLEQ_INIT(&map->handlers);

#if LUA_VERSION_NUM >= 502
	luaL_openlibs(map->L);
	lua_getglobal(map->L, "package");
	lua_getfield(map->L, -1, "preload");
	lua_pushcfunction(map->L, luaopen_httpd);
	lua_setfield(map->L, -2, "httpd");
	lua_pop(map->L, 2);
#else
	lua_openlib(map->L, "", luaopen_base);
	lua_openlib(map->L, LUA_LOADLIBNAME, luaopen_package);
	lua_openlib(map->L, LUA_TABLIBNAME, luaopen_table);
	lua_openlib(map->L, LUA_STRLIBNAME, luaopen_string);
	lua_openlib(map->L, LUA_MATHLIBNAME, luaopen_math);
	lua_openlib(map->L, LUA_OSLIBNAME, luaopen_os);
	lua_openlib(map->L, LUA_IOLIBNAME, luaopen_io);
	lua_openlib(map->L, LUA_HTTPDLIBNAME, luaopen_httpd);
#endif
	lua_pushstring(map->L, "lua_state_map");
	lua_pushlightuserdata(map->L, map);
	lua_settable(map->L, LUA_REGISTRYINDEX);

	lua_pushstring(map->L, "bozohttpd");
	lua_pushlightuserdata(map->L, httpd);
	lua_settable(map->L, LUA_REGISTRYINDEX);

	if (luaL_loadfile(map->L, script))
		bozo_err(httpd, 1, "failed to load script %s: %s", script,
		    lua_tostring(map->L, -1));
	if (lua_pcall(map->L, 0, 0, 0))
		bozo_err(httpd, 1, "failed to execute script %s: %s", script,
		    lua_tostring(map->L, -1));
	SIMPLEQ_INSERT_TAIL(&httpd->lua_states, map, s_next);
}

static void
lua_env(lua_State *L, const char *name, const char *value)
{
	lua_pushstring(L, value);
	lua_setfield(L, -2, name);
}

/* decode query string */
static void
lua_url_decode(lua_State *L, char *s)
{
	char *v, *p, *val, *q;
	char buf[3];
	int c;

	v = strchr(s, '=');
	if (v == NULL)
		return;
	*v++ = '\0';
	val = malloc(strlen(v) + 1);
	if (val == NULL)
		return;

	for (p = v, q = val; *p; p++) {
		switch (*p) {
		case '%':
			if (*(p + 1) == '\0' || *(p + 2) == '\0') {
				free(val);
				return;
			}
			buf[0] = *++p;
			buf[1] = *++p;
			buf[2] = '\0';
			sscanf(buf, "%2x", &c);
			*q++ = (char)c;
			break;
		case '+':
			*q++ = ' ';
			break;
		default:
			*q++ = *p;
		}
	}
	*q = '\0';
	lua_pushstring(L, val);
	lua_setfield(L, -2, s);
	free(val);
}

static void
lua_decode_query(lua_State *L, char *query)
{
	char *s;

	s = strtok(query, "&");
	while (s) {
		lua_url_decode(L, s);
		s = strtok(NULL, "&");
	}
}

int
bozo_process_lua(bozo_httpreq_t *request)
{
	bozohttpd_t *httpd = request->hr_httpd;
	lua_state_map_t *map;
	lua_handler_t *hndlr;
	int n, ret, length;
	char date[40];
	bozoheaders_t *headp;
	char *s, *query, *uri, *file, *command, *info, *content;
	const char *type, *clen;
	char *prefix, *handler, *p;
	int rv = 0;

	if (!httpd->process_lua)
		return 0;

	info = NULL;
	query = NULL;
	prefix = NULL;
	uri = request->hr_oldfile ? request->hr_oldfile : request->hr_file;

	if (*uri == '/') {
		file = bozostrdup(httpd, uri);
		if (file == NULL)
			goto out;
		prefix = bozostrdup(httpd, &uri[1]);
	} else {
		if (asprintf(&file, "/%s", uri) < 0)
			goto out;
		prefix = bozostrdup(httpd, uri);
	}
	if (prefix == NULL)
		goto out;

	if (request->hr_query && request->hr_query[0])
		query = bozostrdup(httpd, request->hr_query);

	p = strchr(prefix, '/');
	if (p == NULL)
		goto out;
	*p++ = '\0';
	handler = p;
	if (!*handler)
		goto out;
	p = strchr(handler, '/');
	if (p != NULL)
		*p++ = '\0';

	command = file + 1;
	if ((s = strchr(command, '/')) != NULL) {
		info = bozostrdup(httpd, s);
		*s = '\0';
	}

	type = request->hr_content_type;
	clen = request->hr_content_length;

	SIMPLEQ_FOREACH(map, &httpd->lua_states, s_next) {
		if (strcmp(map->prefix, prefix))
			continue;

		SIMPLEQ_FOREACH(hndlr, &map->handlers, h_next) {
			if (strcmp(hndlr->name, handler))
				continue;

			lua_rawgeti(map->L, LUA_REGISTRYINDEX, hndlr->ref);

			/* Create the "environment" */
			lua_newtable(map->L);
			lua_env(map->L, "SERVER_NAME",
			    BOZOHOST(httpd, request));
			lua_env(map->L, "GATEWAY_INTERFACE", "Luigi/1.0");
			lua_env(map->L, "SERVER_PROTOCOL", request->hr_proto);
			lua_env(map->L, "REQUEST_METHOD",
			    request->hr_methodstr);
			lua_env(map->L, "SCRIPT_PREFIX", map->prefix);
			lua_env(map->L, "SCRIPT_NAME", file);
			lua_env(map->L, "HANDLER_NAME", hndlr->name);
			lua_env(map->L, "SCRIPT_FILENAME", map->script);
			lua_env(map->L, "SERVER_SOFTWARE",
			    httpd->server_software);
			lua_env(map->L, "REQUEST_URI", uri);
			lua_env(map->L, "DATE_GMT",
			    bozo_http_date(date, sizeof(date)));
			if (query && *query)
				lua_env(map->L, "QUERY_STRING", query);
			if (info && *info)
				lua_env(map->L, "PATH_INFO", info);
			if (type && *type)
				lua_env(map->L, "CONTENT_TYPE", type);
			if (clen && *clen)
				lua_env(map->L, "CONTENT_LENGTH", clen);
			if (request->hr_serverport && *request->hr_serverport)
				lua_env(map->L, "SERVER_PORT",
				    request->hr_serverport);
			if (request->hr_remotehost && *request->hr_remotehost)
				lua_env(map->L, "REMOTE_HOST",
				    request->hr_remotehost);
			if (request->hr_remoteaddr && *request->hr_remoteaddr)
				lua_env(map->L, "REMOTE_ADDR",
				    request->hr_remoteaddr);

			/* Pass the headers in a separate table */
			lua_newtable(map->L);
			SIMPLEQ_FOREACH(headp, &request->hr_headers, h_next)
				lua_env(map->L, headp->h_header,
				    headp->h_value);

			/* Pass the query variables */
			if ((query && *query) ||
			    (type && *type && !strcmp(type, FORM))) {
				lua_newtable(map->L);
				if (query && *query)
					lua_decode_query(map->L, query);
				if (type && *type && !strcmp(type, FORM)) {
					if (clen && *clen && atol(clen) > 0) {
						length = atol(clen);
						content = bozomalloc(httpd,
						    length + 1);
						n = bozo_read(httpd,
						    STDIN_FILENO, content,
						    length);
						if (n >= 0) {
							content[n] = '\0';
							lua_decode_query(map->L,
							    content);
						} else {
							lua_pop(map->L, 1);
							lua_pushnil(map->L);
						}
						free(content);
					}
				}
			} else
				lua_pushnil(map->L);

			ret = lua_pcall(map->L, 3, 0, 0);
			if (ret)
				printf("<br>Lua error: %s\n",
				    lua_tostring(map->L, -1));
			bozo_flush(httpd, stdout);
			rv = 1;
			goto out;
		}
	}
out:
	free(prefix);
	free(uri);
	free(info);
	free(query);
	free(file);
	return rv;
}

#endif /* NO_LUA_SUPPORT */

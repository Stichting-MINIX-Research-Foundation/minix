/*	$NetBSD: syslog.c,v 1.1 2013/11/12 14:32:03 mbalmer Exp $ */

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
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Lua binding for syslog */

#include <sys/types.h>

#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

int luaopen_syslog(lua_State *);

static int
syslog_openlog(lua_State *L)
{
	const char *ident;
	int option;
	int facility;

	ident = luaL_checkstring(L, 1);
	option = luaL_checkinteger(L, 2);
	facility = luaL_checkinteger(L, 3);
	openlog(ident, option, facility);
	return 0;
}

static int
syslog_syslog(lua_State *L)
{
	syslog(luaL_checkint(L, 1), "%s", luaL_checkstring(L, 2));
	return 0;
}

static int
syslog_closelog(lua_State *L)
{
	closelog();
	return 0;
}

static int
syslog_setlogmask(lua_State *L)
{
	lua_pushinteger(L, setlogmask(luaL_checkint(L, 1)));
	return 1;
}

static void
syslog_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2013 by "
	    "Marc Balmer <marc@msys.ch>");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "syslog binding for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "syslog 1.0.0");
	lua_settable(L, -3);
}

struct constant {
	const char *name;
	int value;
};

#define CONSTANT(NAME)		{ #NAME, NAME }

static struct constant syslog_constant[] = {
	/* syslog options */
	CONSTANT(LOG_CONS),
	CONSTANT(LOG_NDELAY),
	CONSTANT(LOG_NOWAIT),
	CONSTANT(LOG_ODELAY),
	CONSTANT(LOG_PERROR),
	CONSTANT(LOG_PID),

	/* syslog facilities */
	CONSTANT(LOG_AUTH),
	CONSTANT(LOG_AUTHPRIV),
	CONSTANT(LOG_CRON),
	CONSTANT(LOG_DAEMON),
	CONSTANT(LOG_FTP),
	CONSTANT(LOG_KERN),
	CONSTANT(LOG_LOCAL0),
	CONSTANT(LOG_LOCAL1),
	CONSTANT(LOG_LOCAL2),
	CONSTANT(LOG_LOCAL3),
	CONSTANT(LOG_LOCAL4),
	CONSTANT(LOG_LOCAL5),
	CONSTANT(LOG_LOCAL6),
	CONSTANT(LOG_LOCAL7),
	CONSTANT(LOG_LPR),
	CONSTANT(LOG_MAIL),
	CONSTANT(LOG_NEWS),
	CONSTANT(LOG_SYSLOG),
	CONSTANT(LOG_USER),
	CONSTANT(LOG_UUCP),

	/* syslog levels */
	CONSTANT(LOG_EMERG),
	CONSTANT(LOG_ALERT),
	CONSTANT(LOG_CRIT),
	CONSTANT(LOG_ERR),
	CONSTANT(LOG_WARNING),
	CONSTANT(LOG_NOTICE),
	CONSTANT(LOG_INFO),
	CONSTANT(LOG_DEBUG),

	{ NULL, 0 }
};

int
luaopen_syslog(lua_State *L)
{
	int n;
	struct luaL_Reg luasyslog[] = {
		{ "openlog",	syslog_openlog },
		{ "syslog",	syslog_syslog },
		{ "closelog",	syslog_closelog },
		{ "setlogmask",	syslog_setlogmask },
		{ NULL, NULL }
	};

#if LUA_VERSION_NUM >= 502
	luaL_newlib(L, luasyslog);
#else
	luaL_register(L, "syslog", luasyslog);
#endif
	syslog_set_info(L);
	for (n = 0; syslog_constant[n].name != NULL; n++) {
		lua_pushinteger(L, syslog_constant[n].value);
		lua_setfield(L, -2, syslog_constant[n].name);
	};
	return 1;
}

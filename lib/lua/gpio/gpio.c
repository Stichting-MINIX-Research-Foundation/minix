/*	$NetBSD: gpio.c,v 1.8 2013/10/26 09:18:00 mbalmer Exp $ */

/*
 * Copyright (c) 2011 Marc Balmer <marc@msys.ch>
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

/* GPIO interface for Lua */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/gpio.h>
#include <sys/ioctl.h>

#define GPIO_METATABLE "GPIO object methods"

static __printflike(2, 3) void
gpio_error(lua_State *L, const char *fmt, ...)
{
	va_list ap;
	int len;
	char *msg;

	va_start(ap, fmt);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (len != -1) {
		lua_pushstring(L, msg);
		free(msg);
	} else
		lua_pushstring(L, "vasprintf failed");
	lua_error(L);
}

static int
gpio_open(lua_State *L)
{
	int *fd;

	fd = lua_newuserdata(L, sizeof(int));
	*fd = open(luaL_checkstring(L, -2), O_RDWR);
	if (*fd == -1) {
		gpio_error(L, "%s", strerror(errno));
		/* NOTREACHED */
		return 0;
	}
	luaL_getmetatable(L, GPIO_METATABLE);
	lua_setmetatable(L, -2);
	return 1;
}

static int
gpio_close(lua_State *L)
{
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	if (*fd != -1) {
		close(*fd);
		*fd = -1;
	}
	return 0;
}

static int
gpio_info(lua_State *L)
{
	struct gpio_info info;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	if (ioctl(*fd, GPIOINFO, &info) == -1)
		gpio_error(L, "GPIOINFO");
	lua_pushinteger(L, info.gpio_npins);
	return 1;
}

static void
gpio_get_pin(lua_State *L, int n, struct gpio_req *req)
{
	switch (lua_type(L, n)) {
	case LUA_TNUMBER:
		req->gp_pin = (int)lua_tointeger(L, n);	/* not 1 based! */
		break;
	case LUA_TSTRING:
		strlcpy(req->gp_name, lua_tostring(L, n), sizeof(req->gp_name));
		break;
	default:
		luaL_argerror(L, n, "expected string or integer");
		/* NOTREACHED */
	}
}

static int
gpio_set(lua_State *L)
{
	struct gpio_set set;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	memset(&set, 0, sizeof(set));
	gpio_get_pin(L, 2, (void *)&set);
	set.gp_flags = (int)luaL_checkinteger(L, 3);
	if (ioctl(*fd, GPIOSET, &set) == -1)
		gpio_error(L, "GPIOSET");
	return 0;
}

static int
gpio_unset(lua_State *L)
{
	struct gpio_set set;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	memset(&set, 0, sizeof(set));
	gpio_get_pin(L, 2, (void *)&set);
	if (ioctl(*fd, GPIOUNSET, &set) == -1)
		gpio_error(L, "GPIOUNSET");
	return 0;
}

static int
gpio_read(lua_State *L)
{
	struct gpio_req req;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	memset(&req, 0, sizeof(req));
	gpio_get_pin(L, 2, &req);
	if (ioctl(*fd, GPIOREAD, &req) == -1)
		gpio_error(L, "GPIOREAD");
	lua_pushinteger(L, req.gp_value);
	return 1;
}


static int
gpio_write(lua_State *L)
{
	struct gpio_req req;
	int *fd, val;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	val = (int)luaL_checkinteger(L, 3);
	if (val != GPIO_PIN_HIGH && val != GPIO_PIN_LOW)
		gpio_error(L, "%d: invalid value", val);
	memset(&req, 0, sizeof(req));
	gpio_get_pin(L, 2, &req);
	req.gp_value = val;
	if (ioctl(*fd, GPIOWRITE, &req) == -1)
		gpio_error(L, "GPIOWRITE");
	lua_pushinteger(L, req.gp_value);
	return 1;
}

static int
gpio_toggle(lua_State *L)
{
	struct gpio_req req;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	memset(&req, 0, sizeof(req));
	gpio_get_pin(L, 2, &req);
	if (ioctl(*fd, GPIOTOGGLE, &req) == -1)
		gpio_error(L, "GPIOTOGGLE");
	lua_pushinteger(L, req.gp_value);
	return 1;
}

static int
gpio_attach(lua_State *L)
{
	struct gpio_attach attach;
	int *fd;

	fd = luaL_checkudata(L, 1, GPIO_METATABLE);
	memset(&attach, 0, sizeof(attach));
	strlcpy(attach.ga_dvname, luaL_checkstring(L, 2),
	    sizeof(attach.ga_dvname));
	attach.ga_offset = (int)luaL_checkinteger(L, 3);
	attach.ga_mask = (int)luaL_checkinteger(L, 4);
	if (lua_gettop(L) > 4)
		attach.ga_flags = (int)luaL_checkinteger(L, 5);
	else
		attach.ga_flags = 0;

	if (ioctl(*fd, GPIOATTACH, &attach) == -1)
		gpio_error(L, "GPIOATTACH");
	return 0;
}

struct constant {
	const char *name;
	int value;
};

static const struct constant gpio_constant[] = {
	/* GPIO pin states */
	{ "PIN_LOW",		GPIO_PIN_LOW },
	{ "PIN_HIGH",		GPIO_PIN_HIGH },

	/* GPIO pin configuration flags */
	{ "PIN_INPUT",		GPIO_PIN_INPUT },
	{ "PIN_OUTPUT",		GPIO_PIN_OUTPUT },
	{ "PIN_INOUT",		GPIO_PIN_INOUT },
	{ "PIN_OPENDRAIN",	GPIO_PIN_OPENDRAIN },
	{ "PIN_PUSHPULL",	GPIO_PIN_PUSHPULL },
	{ "PIN_TRISTATE",	GPIO_PIN_TRISTATE },
	{ "PIN_PULLUP",		GPIO_PIN_PULLUP },
	{ "PIN_PULLDOWN",	GPIO_PIN_PULLDOWN },
	{ "PIN_INVIN",		GPIO_PIN_INVIN },
	{ "PIN_INVOUT",		GPIO_PIN_INVOUT },
	{ "PIN_USER",		GPIO_PIN_USER },
	{ "PIN_PULSATE",	GPIO_PIN_PULSATE },
	{ "PIN_SET",		GPIO_PIN_SET },
	{ NULL,			0 }
};

static void
gpio_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2011, 2013 Marc Balmer "
	    "<marc@msys.ch>");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "GPIO interface for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "gpio 1.0.2");
	lua_settable(L, -3);
}

int luaopen_gpio(lua_State*);

int
luaopen_gpio(lua_State* L)
{
	static const struct luaL_Reg methods[] = {
		{ "open",	gpio_open },
		{ NULL,		NULL }
	};
	static const struct luaL_Reg gpio_methods[] = {
		{ "info",	gpio_info },
		{ "close",	gpio_close },
		{ "set",	gpio_set },
		{ "unset",	gpio_unset },
		{ "read",	gpio_read },
		{ "write",	gpio_write },
		{ "toggle",	gpio_toggle },
		{ "attach",	gpio_attach },
		{ NULL,		NULL }
	};
	int n;

	luaL_register(L, "gpio", methods);
	luaL_register(L, NULL, gpio_methods);
	gpio_set_info(L);

	/* The gpio metatable */
	if (luaL_newmetatable(L, GPIO_METATABLE)) {
		luaL_register(L, NULL, gpio_methods);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, gpio_close);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	for (n = 0; gpio_constant[n].name != NULL; n++) {
		lua_pushinteger(L, gpio_constant[n].value);
		lua_setfield(L, -2, gpio_constant[n].name);
	};
	return 1;
}

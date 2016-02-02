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

#include <inttypes.h>
#include <netpgp.h>
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

#define DEFAULT_HASH_ALG        "SHA256"

int luaopen_netpgp(lua_State *);

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

/* set the home directory value to "home/subdir" */
static int
set_homedir(netpgp_t *netpgp, char *home, const char *subdir, const int quiet)
{
	struct stat	st;
	char		d[MAXPATHLEN];

	if (home == NULL) {
		if (!quiet) {
			(void) fprintf(stderr, "NULL HOME directory\n");
		}
		return 0;
	}
	(void) snprintf(d, sizeof(d), "%s%s", home, (subdir) ? subdir : "");
	if (stat(d, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			netpgp_setvar(netpgp, "homedir", d);
			return 1;
		}
		(void) fprintf(stderr, "netpgp: homedir \"%s\" is not a dir\n",
					d);
		return 0;
	}
	if (!quiet) {
		(void) fprintf(stderr,
			"netpgp: warning homedir \"%s\" not found\n", d);
	}
	return 1;
}


/* init() */
static int
l_new(lua_State *L)
{
	netpgp_t	*netpgp;

	netpgp = lua_newuserdata(L, sizeof(*netpgp));
	(void) memset(netpgp, 0x0, sizeof(*netpgp));
	set_homedir(netpgp, getenv("HOME"), "/.gnupg", 1);
	netpgp_setvar(netpgp, "hash", DEFAULT_HASH_ALG);
	return 1;
}

/* initialise(netpgp) */
static int
l_init(lua_State *L)
{
	netpgp_t	*netpgp;

	netpgp = lua_touserdata(L, 1);
	lua_pushnumber(L, netpgp_init(netpgp));
	return 1;
}

/* homedir(netpgp, homedir) */
static int
l_homedir(lua_State *L)
{
	const char	*home;
	netpgp_t	*netpgp;

	netpgp = lua_touserdata(L, 1);
	home = luaL_checkstring(L, 2);
	lua_pushnumber(L, set_homedir(netpgp, __UNCONST(home), NULL, 0));
	return 1;
}

static strarg_t	armourtypes[] = {
	{	"armoured",	1	},
	{	"armored",	1	},
	{	"armour",	1	},
	{	"armor",	1	},
	{	NULL,		0	}
};

/* encrypt_file(netpgp, f, output, armour) */
static int
l_encrypt_file(lua_State *L)
{
	const char	*output;
	const char	*f;
	netpgp_t	*netpgp;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	netpgp_setvar(netpgp, "need userid", "1");
	f = luaL_checkstring(L, 2);
	output = luaL_checkstring(L, 3);
	if (*output == 0x0) {
		output = NULL;
	}
	armour = findtype(armourtypes, luaL_checkstring(L, 4));
	ret = netpgp_encrypt_file(netpgp, netpgp_getvar(netpgp, "userid"),
				f, __UNCONST("a.gpg"), armour);
	lua_pushnumber(L, ret);
	return 1;
}

/* decrypt_file(netpgp, f, output, armour) */
static int
l_decrypt_file(lua_State *L)
{
	const char	*output;
	const char	*f;
	netpgp_t	*netpgp;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	f = luaL_checkstring(L, 2);
	output = luaL_checkstring(L, 3);
	if (*output == 0x0) {
		output = NULL;
	}
	armour = findtype(armourtypes, luaL_checkstring(L, 4));
	ret = netpgp_decrypt_file(netpgp, f, __UNCONST(output), armour);
	lua_pushnumber(L, ret);
	return 1;
}

static strarg_t	detachtypes[] = {
	{	"detached",	1	},
	{	"separate",	1	},
	{	"detach",	1	},
	{	NULL,		0	}
};

/* sign_file(netpgp, f, output, armour, detached) */
static int
l_sign_file(lua_State *L)
{
	const char	*output;
	const char	*f;
	const int	 binary = 0;
	netpgp_t	*netpgp;
	int		 detached;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	netpgp_setvar(netpgp, "need userid", "1");
	f = luaL_checkstring(L, 2);
	output = luaL_checkstring(L, 3);
	if (*output == 0x0) {
		output = NULL;
	}
	armour = findtype(armourtypes, luaL_checkstring(L, 4));
	detached = findtype(detachtypes, luaL_checkstring(L, 5));
	ret = netpgp_sign_file(netpgp, netpgp_getvar(netpgp, "userid"),
				f, __UNCONST(output), armour, binary,
				detached);
	lua_pushnumber(L, ret);
	return 1;
}

/* clearsign_file(netpgp, f, output, armour, detached) */
static int
l_clearsign_file(lua_State *L)
{
	const char	*output;
	const char	*f;
	const int	 cleartext = 1;
	netpgp_t	*netpgp;
	int		 detached;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	netpgp_setvar(netpgp, "need userid", "1");
	f = luaL_checkstring(L, 2);
	output = luaL_checkstring(L, 3);
	armour = findtype(armourtypes, luaL_checkstring(L, 4));
	detached = findtype(detachtypes, luaL_checkstring(L, 5));
	ret = netpgp_sign_file(netpgp, netpgp_getvar(netpgp, "userid"),
				f, __UNCONST(output), armour, cleartext,
				detached);
	lua_pushnumber(L, ret);
	return 1;
}

/* verify_file(netpgp, f, armour) */
static int
l_verify_file(lua_State *L)
{
	const char	*f;
	netpgp_t	*netpgp;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	f = luaL_checkstring(L, 2);
	armour = findtype(armourtypes, luaL_checkstring(L, 3));
	ret = netpgp_verify_file(netpgp, f, NULL, armour);
	lua_pushnumber(L, ret);
	return 1;
}

/* verify_cat_file(netpgp, f, output, armour) */
static int
l_verify_cat_file(lua_State *L)
{
	const char	*output;
	const char	*f;
	netpgp_t	*netpgp;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	f = luaL_checkstring(L, 2);
	output = luaL_checkstring(L, 3);
	armour = findtype(armourtypes, luaL_checkstring(L, 4));
	ret = netpgp_verify_file(netpgp, f, output, armour);
	lua_pushnumber(L, ret);
	return 1;
}

/* list_packets(netpgp, f, armour) */
static int
l_list_packets(lua_State *L)
{
	const char	*f;
	netpgp_t	*netpgp;
	int		 armour;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	f = luaL_checkstring(L, 2);
	armour = findtype(armourtypes, luaL_checkstring(L, 3));
	ret = netpgp_list_packets(netpgp, __UNCONST(f), armour, NULL);
	lua_pushnumber(L, ret);
	return 1;
}

/* setvar(netpgp, name, value) */
static int
l_setvar(lua_State *L)
{
	const char	*name;
	const char	*value;
	netpgp_t	*netpgp;
	int		 ret;

	netpgp = lua_touserdata(L, 1);
	name = luaL_checkstring(L, 2);
	value = luaL_checkstring(L, 3);
	ret = netpgp_setvar(netpgp, name, value);
	lua_pushnumber(L, ret);
	return 1;
}

/* getvar(netpgp, name, value) */
static int
l_getvar(lua_State *L)
{
	const char	*name;
	const char	*ret;
	netpgp_t	*netpgp;

	netpgp = lua_touserdata(L, 1);
	name = luaL_checkstring(L, 2);
	ret = netpgp_getvar(netpgp, name);
	lua_pushstring(L, ret);
	return 1;
}

const struct luaL_reg libluanetpgp[] = {
	{ "new",		l_new },
	{ "init",		l_init },

	{ "encrypt_file",	l_encrypt_file },
	{ "decrypt_file",	l_decrypt_file },
	{ "sign_file",		l_sign_file },
	{ "clearsign_file",	l_clearsign_file },
	{ "verify_file",	l_verify_file },
	{ "verify_cat_file",	l_verify_cat_file },

	{ "list_packets",	l_list_packets },

	{ "getvar",		l_getvar },
	{ "setvar",		l_setvar },

	{ "homedir",		l_homedir },

	{ NULL,			NULL }
};

int 
luaopen_netpgp(lua_State *L)
{
	luaL_openlib(L, "netpgp", libluanetpgp, 0);
	return 1;
}

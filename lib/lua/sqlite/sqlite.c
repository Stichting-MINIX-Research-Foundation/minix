/*	$NetBSD: sqlite.c,v 1.6 2013/10/27 12:38:08 mbalmer Exp $ */

/*
 * Copyright (c) 2011, 2013 Marc Balmer <marc@msys.ch>
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

/* SQLite interface for Lua */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sqlite3.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define SQLITE_DB_METATABLE "SQLite database connection methods"
#define SQLITE_STMT_METATABLE "SQLite statement methods"

int luaopen_sqlite(lua_State*);

static __printflike(2, 3) void
sqlite_error(lua_State *L, const char *fmt, ...)
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
sqlite_initialize(lua_State *L)
{
	lua_pushinteger(L, sqlite3_initialize());
	return 1;
}

static int
sqlite_shutdown(lua_State *L)
{
	lua_pushinteger(L, sqlite3_shutdown());
	return 1;
}

static int
sqlite_open(lua_State *L)
{
	sqlite3 **db;

	db = lua_newuserdata(L, sizeof(sqlite3 *));
	luaL_getmetatable(L, SQLITE_DB_METATABLE);
	lua_setmetatable(L, -2);

	if (lua_gettop(L) > 2)
		lua_pushinteger(L, sqlite3_open_v2(luaL_checkstring(L, -3), db,
		    (int)luaL_checkinteger(L, -2), NULL));
	else
		lua_pushinteger(L, sqlite3_open(luaL_checkstring(L, -2), db));
	return 2;

}

static int
sqlite_libversion(lua_State *L)
{
	lua_pushstring(L, sqlite3_libversion());
	return 1;
}

static int
sqlite_libversion_number(lua_State *L)
{
	lua_pushinteger(L, sqlite3_libversion_number());
	return 1;
}

static int
sqlite_sourceid(lua_State *L)
{
	lua_pushstring(L, sqlite3_sourceid());
	return 1;
}

static int
db_close(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushinteger(L, sqlite3_close(*db));
	return 1;

}

static int
db_prepare(lua_State *L)
{
	sqlite3 **db;
	sqlite3_stmt **stmt;
	const char *sql;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	stmt = lua_newuserdata(L, sizeof(sqlite3_stmt *));
	sql = luaL_checkstring(L, 2);
	lua_pushinteger(L, sqlite3_prepare_v2(*db, sql,
	    (int)strlen(sql) + 1, stmt, NULL));
	luaL_getmetatable(L, SQLITE_STMT_METATABLE);
	lua_setmetatable(L, -3);
	return 2;

}

static int
db_exec(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushinteger(L, sqlite3_exec(*db, lua_tostring(L, 2), NULL,
	    NULL, NULL));
	return 1;
}

static int
db_errcode(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushinteger(L, sqlite3_errcode(*db));
	return 1;
}

static int
db_errmsg(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushstring(L, sqlite3_errmsg(*db));
	return 1;
}

static int
db_get_autocommit(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushboolean(L, sqlite3_get_autocommit(*db));
	return 1;
}

static int
db_changes(lua_State *L)
{
	sqlite3 **db;

	db = luaL_checkudata(L, 1, SQLITE_DB_METATABLE);
	lua_pushinteger(L, sqlite3_changes(*db));
	return 1;
}

static int
stmt_bind(lua_State *L)
{
	sqlite3_stmt **stmt;
	int pidx;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	pidx = (int)luaL_checkinteger(L, 2);

	switch (lua_type(L, 3)) {
	case LUA_TNUMBER:
		lua_pushinteger(L, sqlite3_bind_double(*stmt, pidx,
		    lua_tonumber(L, 3)));
		break;
	case LUA_TSTRING:
		lua_pushinteger(L, sqlite3_bind_text(*stmt, pidx,
		    lua_tostring(L, 3), -1, SQLITE_TRANSIENT));
		break;
	case LUA_TNIL:
		lua_pushinteger(L, sqlite3_bind_null(*stmt, pidx));
		break;
	default:
		sqlite_error(L, "unsupported data type %s",
		    luaL_typename(L, 3));
	}
	return 1;
}

static int
stmt_bind_parameter_count(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	lua_pushinteger(L, sqlite3_bind_parameter_count(*stmt));
	return 1;
}

static int
stmt_bind_parameter_index(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	lua_pushinteger(L, sqlite3_bind_parameter_index(*stmt,
	    lua_tostring(L, 2)));
	return 1;
}

static int
stmt_bind_parameter_name(lua_State *L)
{
	sqlite3_stmt **stmt;
	int pidx;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	pidx = (int)luaL_checkinteger(L, 2);
	lua_pushstring(L, sqlite3_bind_parameter_name(*stmt, pidx));
	return 1;
}

static int
stmt_step(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	lua_pushinteger(L, sqlite3_step(*stmt));
	return 1;
}

static int
stmt_column_name(lua_State *L)
{
	sqlite3_stmt **stmt;
	int cidx;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	cidx = (int)luaL_checkinteger(L, 2) - 1;

	lua_pushstring(L, sqlite3_column_name(*stmt, cidx));
	return 1;
}

static int
stmt_column_count(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	lua_pushinteger(L, sqlite3_column_count(*stmt));
	return 1;
}

static int
stmt_column(lua_State *L)
{
	sqlite3_stmt **stmt;
	int cidx;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	cidx = (int)luaL_checkinteger(L, 2) - 1;

	switch (sqlite3_column_type(*stmt, cidx)) {
	case SQLITE_INTEGER:
		lua_pushinteger(L, sqlite3_column_int(*stmt, cidx));
		break;
	case SQLITE_FLOAT:
		lua_pushnumber(L, sqlite3_column_double(*stmt, cidx));
		break;
	case SQLITE_TEXT:
		lua_pushstring(L, (const char *)sqlite3_column_text(*stmt,
		    cidx));
		break;
	case SQLITE_BLOB:
	case SQLITE_NULL:
		lua_pushnil(L);
		break;
	}
	return 1;
}

static int
stmt_reset(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	sqlite3_reset(*stmt);
	return 0;
}

static int
stmt_clear_bindings(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	sqlite3_clear_bindings(*stmt);
	return 0;
}

static int
stmt_finalize(lua_State *L)
{
	sqlite3_stmt **stmt;

	stmt = luaL_checkudata(L, 1, SQLITE_STMT_METATABLE);
	sqlite3_finalize(*stmt);
	return 0;
}

struct constant {
	const char *name;
	int value;
};

static const struct constant sqlite_constant[] = {
	/* SQLite return codes */
	{ "OK",			SQLITE_OK },
	{ "ERROR",		SQLITE_ERROR },
	{ "INTERNAL",		SQLITE_INTERNAL },
	{ "PERM",		SQLITE_PERM },
	{ "ABORT",		SQLITE_ABORT },
	{ "BUSY",		SQLITE_BUSY },
	{ "LOCKED",		SQLITE_LOCKED },
	{ "NOMEM",		SQLITE_NOMEM },
	{ "READONLY",		SQLITE_READONLY },
	{ "INTERRUPT",		SQLITE_INTERRUPT },
	{ "IOERR",		SQLITE_IOERR },
	{ "CORRUPT",		SQLITE_CORRUPT },
	{ "NOTFOUND",		SQLITE_NOTFOUND },
	{ "FULL",		SQLITE_FULL },
	{ "CANTOPEN",		SQLITE_CANTOPEN },
	{ "PROTOCOL",		SQLITE_PROTOCOL },
	{ "EMPTY",		SQLITE_EMPTY },
	{ "SCHEMA",		SQLITE_SCHEMA },
	{ "TOOBIG",		SQLITE_TOOBIG },
	{ "CONSTRAINT",		SQLITE_CONSTRAINT },
	{ "MISMATCH",		SQLITE_MISMATCH },
	{ "MISUSE",		SQLITE_MISUSE },
	{ "NOLFS",		SQLITE_NOLFS },
	{ "AUTH",		SQLITE_AUTH },
	{ "FORMAT",		SQLITE_FORMAT },
	{ "RANGE",		SQLITE_RANGE },
	{ "NOTADB",		SQLITE_NOTADB },
	{ "ROW",		SQLITE_ROW },
	{ "DONE",		SQLITE_DONE },

	/* File modes */
	{ "OPEN_READONLY",	SQLITE_OPEN_READONLY },
	{ "OPEN_READWRITE",	SQLITE_OPEN_READWRITE },
	{ "OPEN_CREATE",	SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE },

	{ NULL,			0 }
};

static void
gpio_set_info(lua_State *L)
{
	lua_pushliteral(L, "_COPYRIGHT");
	lua_pushliteral(L, "Copyright (C) 2011, 2012, 2013 by "
	    "Marc Balmer <marc@msys.ch>");
	lua_settable(L, -3);
	lua_pushliteral(L, "_DESCRIPTION");
	lua_pushliteral(L, "SQLite interface for Lua");
	lua_settable(L, -3);
	lua_pushliteral(L, "_VERSION");
	lua_pushliteral(L, "sqlite 1.0.3");
	lua_settable(L, -3);
}

int
luaopen_sqlite(lua_State* L)
{
	static const struct luaL_Reg sqlite_methods[] = {
		{ "initialize",			sqlite_initialize },
		{ "shutdown",			sqlite_shutdown },
		{ "open",			sqlite_open },
		{ "libversion",			sqlite_libversion },
		{ "libversion_number",		sqlite_libversion_number },
		{ "sourceid",			sqlite_sourceid },
		{ NULL,				NULL }
	};
	static const struct luaL_Reg db_methods[] = {
		{ "close",			db_close },
		{ "prepare",			db_prepare },
		{ "exec",			db_exec },
		{ "errcode",			db_errcode },
		{ "errmsg",			db_errmsg },
		{ "get_autocommit",		db_get_autocommit },
		{ "changes",			db_changes },
		{ NULL,				NULL }
	};
	static const struct luaL_Reg stmt_methods[] = {
		{ "bind",			stmt_bind },
		{ "bind_parameter_count",	stmt_bind_parameter_count },
		{ "bind_parameter_index",	stmt_bind_parameter_index },
		{ "bind_parameter_name",	stmt_bind_parameter_name },
		{ "step",			stmt_step },
		{ "column",			stmt_column },
		{ "reset",			stmt_reset },
		{ "clear_bindings",		stmt_clear_bindings },
		{ "finalize",			stmt_finalize },
		{ "column_name",		stmt_column_name },
		{ "column_count",		stmt_column_count },
		{ NULL,		NULL }
	};
	int n;

	sqlite3_initialize();

	luaL_register(L, "sqlite", sqlite_methods);
	luaL_register(L, NULL, db_methods);
	luaL_register(L, NULL, stmt_methods);
	gpio_set_info(L);

	/* The database connection metatable */
	if (luaL_newmetatable(L, SQLITE_DB_METATABLE)) {
		luaL_register(L, NULL, db_methods);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, db_close);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	/* The statement metatable */
	if (luaL_newmetatable(L, SQLITE_STMT_METATABLE)) {
		luaL_register(L, NULL, stmt_methods);

		lua_pushliteral(L, "__gc");
		lua_pushcfunction(L, stmt_finalize);
		lua_settable(L, -3);

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, -2);
		lua_settable(L, -3);

		lua_pushliteral(L, "__metatable");
		lua_pushliteral(L, "must not access this metatable");
		lua_settable(L, -3);
	}
	lua_pop(L, 1);

	for (n = 0; sqlite_constant[n].name != NULL; n++) {
		lua_pushinteger(L, sqlite_constant[n].value);
		lua_setfield(L, -2, sqlite_constant[n].name);
	};
	return 1;
}

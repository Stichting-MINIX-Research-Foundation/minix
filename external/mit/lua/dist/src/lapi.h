/*	$NetBSD: lapi.h,v 1.1.1.2 2012/03/15 00:08:13 alnsn Exp $	*/

/*
** $Id: lapi.h,v 1.1.1.2 2012/03/15 00:08:13 alnsn Exp $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


LUAI_FUNC void luaA_pushobject (lua_State *L, const TValue *o);

#endif

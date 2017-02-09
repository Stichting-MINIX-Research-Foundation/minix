/* $NetBSD: sysmon_envsys_util.c,v 1.5 2007/11/16 08:00:16 xtraeme Exp $ */

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sysmon_envsys_util.c,v 1.5 2007/11/16 08:00:16 xtraeme Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/sysmon/sysmon_envsysvar.h>
#include <prop/proplib.h>

/*
 * Functions to create objects in a dictionary if they do not exist, or
 * for updating its value if it doesn't match with the value in dictionary.
 */

int
sme_sensor_upbool(prop_dictionary_t dict, const char *key, bool val)
{
	prop_object_t obj;

	KASSERT(dict != NULL);

	obj = prop_dictionary_get(dict, key);
	if (obj) {
		if (prop_bool_true(obj) != val) {
			if (!prop_dictionary_set_bool(dict, key, val)) {
				DPRINTF(("%s: (up) set_bool %s:%d\n",
				    __func__, key, val));
				return EINVAL;
			}
		}
	} else {
		if (!prop_dictionary_set_bool(dict, key, val)) {
			DPRINTF(("%s: (set) set_bool %s:%d\n",
			    __func__, key, val));
			return EINVAL;
		}
	}

	return 0;
}

int
sme_sensor_upint32(prop_dictionary_t dict, const char *key, int32_t val)
{
	prop_object_t obj;

	KASSERT(dict != NULL);

	obj = prop_dictionary_get(dict, key);
	if (obj) {
		if (!prop_number_equals_integer(obj, val)) {
			if (!prop_dictionary_set_int32(dict, key, val)) {
				DPRINTF(("%s: (up) set_int32 %s:%d\n",
				    __func__, key, val));
				return EINVAL;
			}
		}
	} else {
		if (!prop_dictionary_set_int32(dict, key, val)) {
			DPRINTF(("%s: (set) set_int32 %s:%d\n",
			    __func__, key, val));
			return EINVAL;
		}
	}

	return 0;
}

int
sme_sensor_upuint32(prop_dictionary_t dict, const char *key, uint32_t val)
{
	prop_object_t obj;

	KASSERT(dict != NULL);

	obj = prop_dictionary_get(dict, key);
	if (obj) {
		if (!prop_number_equals_unsigned_integer(obj, val)) {
			if (!prop_dictionary_set_uint32(dict, key, val)) {
				DPRINTF(("%s: (up) set_uint32 %s:%d\n",
				    __func__, key, val));
				return EINVAL;
			}
		}
	} else {
		if (!prop_dictionary_set_uint32(dict, key, val)) {
			DPRINTF(("%s: (set) set_uint32 %s:%d\n",
			    __func__, key, val));
			return EINVAL;
		}
	}

	return 0;
}

int
sme_sensor_upstring(prop_dictionary_t dict, const char *key, const char *str)
{
	prop_object_t obj;

	KASSERT(dict != NULL);

	obj = prop_dictionary_get(dict, key);
	if (obj == NULL) {
		if (!prop_dictionary_set_cstring(dict, key, str)) {
			DPRINTF(("%s: (up) set_cstring %s:%s\n",
			    __func__, key, str));
			return EINVAL;
		}
	} else {
		if (!prop_string_equals_cstring(obj, str)) {
			if (!prop_dictionary_set_cstring(dict, key, str)) {
				DPRINTF(("%s: (set) set_cstring %s:%s\n",
				    __func__, key, str));
				return EINVAL;
			}
		}
	}

	return 0;
}

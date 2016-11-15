/* $NetBSD: acpi_debug.c,v 1.5 2014/02/25 18:30:09 pooka Exp $ */

/*-
 * Copyright (c) 2010 Jukka Ruohonen <jruohonen@iki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_debug.c,v 1.5 2014/02/25 18:30:09 pooka Exp $");

#include <sys/param.h>
#include <sys/sysctl.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <prop/proplib.h>

#ifdef ACPI_DEBUG

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME            ("acpi_debug")

#define ACPI_DEBUG_MAX  64
#define ACPI_DEBUG_NONE  0

#define ACPI_DEBUG_ADD(d, x)						      \
	do {								      \
		(void)prop_dictionary_set_uint32(d, #x, x);		      \
									      \
	} while (/* CONSTCOND */ 0)


static prop_dictionary_t acpi_debug_layer_d;
static prop_dictionary_t acpi_debug_level_d;
static char              acpi_debug_layer_s[ACPI_DEBUG_MAX];
static char              acpi_debug_level_s[ACPI_DEBUG_MAX];

static int               acpi_debug_create(void);
static const char       *acpi_debug_getkey(prop_dictionary_t, uint32_t);
static int               acpi_debug_sysctl_layer(SYSCTLFN_PROTO);
static int               acpi_debug_sysctl_level(SYSCTLFN_PROTO);

void
acpi_debug_init(void)
{
	const struct sysctlnode *rnode;
	const char *layer, *level;
	int rv;

	KASSERT(acpi_debug_layer_d == NULL);
	KASSERT(acpi_debug_level_d == NULL);

	rv = acpi_debug_create();

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(NULL, 0, NULL, &rnode,
	    CTLFLAG_PERMANENT, CTLTYPE_NODE, "acpi",
	    NULL, NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(NULL, 0, &rnode, &rnode,
	    0, CTLTYPE_NODE, "debug",
	    SYSCTL_DESCR("ACPI debug subtree"),
	    NULL, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(NULL, 0, &rnode, NULL,
	    CTLFLAG_READWRITE, CTLTYPE_STRING, "layer",
	    SYSCTL_DESCR("ACPI debug layer"),
	    acpi_debug_sysctl_layer, 0, acpi_debug_layer_s, ACPI_DEBUG_MAX,
	    CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(NULL, 0, &rnode, NULL,
	    CTLFLAG_READWRITE, CTLTYPE_STRING, "level",
	    SYSCTL_DESCR("ACPI debug level"),
	    acpi_debug_sysctl_level, 0, acpi_debug_level_s, ACPI_DEBUG_MAX,
	    CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	rv = sysctl_createv(NULL, 0, &rnode, NULL,
	    CTLFLAG_READWRITE, CTLTYPE_BOOL, "object",
	    SYSCTL_DESCR("ACPI debug object"),
	    NULL, 0, &AcpiGbl_EnableAmlDebugObject, 0,
	    CTL_CREATE, CTL_EOL);

	if (rv != 0)
		goto fail;

	layer = acpi_debug_getkey(acpi_debug_layer_d, AcpiDbgLayer);
	level = acpi_debug_getkey(acpi_debug_level_d, AcpiDbgLevel);

	(void)memcpy(acpi_debug_layer_s, layer, ACPI_DEBUG_MAX);
	(void)memcpy(acpi_debug_level_s, level, ACPI_DEBUG_MAX);

	return;

fail:
	aprint_error("acpi0: failed to initialize ACPI debug\n");
}

static int
acpi_debug_create(void)
{

	acpi_debug_layer_d = prop_dictionary_create();
	acpi_debug_level_d = prop_dictionary_create();

	KASSERT(acpi_debug_layer_d != NULL);
	KASSERT(acpi_debug_level_d != NULL);

	/*
	 * General components.
	 */
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_UTILITIES);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_HARDWARE);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_EVENTS);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_TABLES);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_NAMESPACE);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_PARSER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_DISPATCHER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_EXECUTER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_RESOURCES);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_CA_DEBUGGER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_OS_SERVICES);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_CA_DISASSEMBLER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_COMPILER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_TOOLS);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_EXAMPLE);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_DRIVER);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_ALL_COMPONENTS);

	/*
	 * NetBSD specific components.
	 */
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_BUS_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_ACAD_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_BAT_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_BUTTON_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_EC_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_LID_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_RESOURCE_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_TZ_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_DISPLAY_COMPONENT);
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_ALL_DRIVERS);

	/*
	 * Debug levels.
	 */
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_INIT);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_DEBUG_OBJECT);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_INFO);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_ALL_EXCEPTIONS);

	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_INIT_NAMES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_PARSE);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_LOAD);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_DISPATCH);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_EXEC);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_NAMES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_OPREGION);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_BFIELD);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_TABLES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VALUES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_OBJECTS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_RESOURCES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_USER_REQUESTS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_PACKAGE);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VERBOSITY1);

	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_ALLOCATIONS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_FUNCTIONS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_OPTIMIZATIONS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VERBOSITY2);

	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_MUTEX);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_THREADS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_IO);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_INTERRUPTS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VERBOSITY3);

	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_AML_DISASSEMBLE);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VERBOSE_INFO);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_FULL_TABLES);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_EVENTS);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_LV_VERBOSE);

	/*
	 * The default debug level.
	 */
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_DEBUG_DEFAULT);

	/*
	 * A custom ACPI_DEBUG_NONE disables debugging.
	 */
	ACPI_DEBUG_ADD(acpi_debug_layer_d, ACPI_DEBUG_NONE);
	ACPI_DEBUG_ADD(acpi_debug_level_d, ACPI_DEBUG_NONE);

	prop_dictionary_make_immutable(acpi_debug_layer_d);
	prop_dictionary_make_immutable(acpi_debug_level_d);

	return 0;
}

static const char *
acpi_debug_getkey(prop_dictionary_t dict, uint32_t arg)
{
	prop_object_iterator_t i;
	prop_object_t obj, val;
	const char *key;
	uint32_t num;

	i = prop_dictionary_iterator(dict);

	while ((obj = prop_object_iterator_next(i)) != NULL) {

		key = prop_dictionary_keysym_cstring_nocopy(obj);
		val = prop_dictionary_get(dict, key);
		num = prop_number_unsigned_integer_value(val);

		if (arg == num)
			return key;
	}

	return "UNKNOWN";
}

static int
acpi_debug_sysctl_layer(SYSCTLFN_ARGS)
{
	char buf[ACPI_DEBUG_MAX];
	struct sysctlnode node;
	prop_object_t obj;
	int error;

	node = *rnode;
	node.sysctl_data = buf;

	(void)memcpy(node.sysctl_data, rnode->sysctl_data, ACPI_DEBUG_MAX);

	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	obj = prop_dictionary_get(acpi_debug_layer_d, node.sysctl_data);

	if (obj == NULL)
		return EINVAL;

	AcpiDbgLayer = prop_number_unsigned_integer_value(obj);

	(void)memcpy(rnode->sysctl_data, node.sysctl_data, ACPI_DEBUG_MAX);

	return 0;
}

static int
acpi_debug_sysctl_level(SYSCTLFN_ARGS)
{
	char buf[ACPI_DEBUG_MAX];
	struct sysctlnode node;
	prop_object_t obj;
	int error;

	node = *rnode;
	node.sysctl_data = buf;

	(void)memcpy(node.sysctl_data, rnode->sysctl_data, ACPI_DEBUG_MAX);

	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	obj = prop_dictionary_get(acpi_debug_level_d, node.sysctl_data);

	if (obj == NULL)
		return EINVAL;

	AcpiDbgLevel = prop_number_unsigned_integer_value(obj);

	(void)memcpy(rnode->sysctl_data, node.sysctl_data, ACPI_DEBUG_MAX);

	return 0;
}

#endif	/* ACPI_DEBUG */

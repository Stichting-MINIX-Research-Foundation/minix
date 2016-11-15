/* $NetBSD: keylock.c,v 1.3 2014/02/25 18:30:09 pooka Exp $ */

/*
 * Copyright (c) 2009 Marc Balmer <marc@msys.ch>
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

#include "opt_secmodel_keylock.h"

/* Support for multi-position electro-mechanical keylocks */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <dev/keylock.h>

#ifdef secmodel_keylock
#include <sys/kauth.h>
#include <secmodel/keylock/keylock.h>
#endif

static int (*keylock_pos_cb)(void *) = NULL;
static void *keylock_pos_cb_arg = NULL;
static int keylock_npos = 0;
static int keylock_order = 0;

int keylock_pos_sysctl(SYSCTLFN_PROTO);
int keylock_state_sysctl(SYSCTLFN_PROTO);
int keylock_order_sysctl(SYSCTLFN_PROTO);

SYSCTL_SETUP(sysctl_keylock_setup, "sysctl keylock setup")
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(clog, 0, NULL, &node,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "keylock",
	    SYSCTL_DESCR("Keylock state"),
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

	sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY,
	    CTLTYPE_INT, "pos",
	    SYSCTL_DESCR("Current keylock position"),
	    keylock_pos_sysctl, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY,
	    CTLTYPE_INT, "npos",
	    SYSCTL_DESCR("Number of keylock positions"),
	    NULL, 0, &keylock_npos, 0,
	    CTL_CREATE, CTL_EOL);
	sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READONLY,
	    CTLTYPE_INT, "state",
	    SYSCTL_DESCR("Keylock state"),
	    keylock_state_sysctl, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);
       sysctl_createv(clog, 0, &node, NULL,
	    CTLFLAG_PERMANENT | CTLFLAG_READWRITE, 
	    CTLTYPE_INT, "order", 
	    SYSCTL_DESCR("Keylock closedness order"),
	    keylock_order_sysctl, 0, NULL, 0,
	    CTL_CREATE, CTL_EOL);
}

int
keylock_register(void *cb_arg, int npos, int (*cb)(void *))
{
	if (keylock_pos_cb != NULL)
		return -1;

	keylock_pos_cb = cb;
	keylock_pos_cb_arg = cb_arg;
	keylock_npos = npos;
#ifdef secmodel_keylock
	secmodel_keylock_start();
#endif
	return 0;
}

void
keylock_unregister(void *cb_arg, int (*cb)(void *))
{
	if (keylock_pos_cb != cb || keylock_pos_cb_arg != cb_arg)
		return;

#ifdef secmodel_keylock
	secmodel_keylock_stop();
#endif
	keylock_pos_cb = NULL;
	keylock_pos_cb_arg = NULL;
	keylock_npos = 0;
}

int
keylock_position(void)
{
	if (keylock_pos_cb == NULL)
		return 0;

	return (*keylock_pos_cb)(keylock_pos_cb_arg);
}

int
keylock_num_positions(void)
{
	return keylock_npos;
}

int
keylock_state(void)
{
        int pos;

        if (keylock_npos == 0)
                return KEYLOCK_ABSENT;

        pos = keylock_position();
        if (pos == 0)
                return KEYLOCK_TAMPER;

        /*
	 * XXX How should the intermediate positions be handled?
	 * At the moment only the ultimate positions are properly handled,
	 * we need to think about what we do with the intermediate positions.
	 * For now we return KEYLOCK_SEMIOPEN for them.
	 */
        if (pos == 1)
                return keylock_order == 0 ? KEYLOCK_CLOSE : KEYLOCK_OPEN;
        else if (pos == keylock_npos)
                return keylock_order == 0 ? KEYLOCK_OPEN : KEYLOCK_CLOSE;
        return KEYLOCK_SEMIOPEN;
}

int
keylock_pos_sysctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int val;

	node = *rnode;
	node.sysctl_data = &val;

	val = keylock_position();
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

int
keylock_state_sysctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int val;

	node = *rnode;
	node.sysctl_data = &val;

	val = keylock_state();
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

int
keylock_order_sysctl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int val, error;

	node = *rnode;
	node.sysctl_data = &val;

	val = keylock_order;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;
	if (keylock_state() != KEYLOCK_OPEN)
		return -1;

	keylock_order = val;
	return 0;
}


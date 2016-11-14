/*	$NetBSD: rumpnet_if_wrappers.c,v 1.5 2013/07/03 19:22:21 pooka Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpnet.ifspec,v 1.3 2013/07/03 19:21:11 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.6 2013/02/14 10:54:54 pooka Exp 
 */

#include <sys/cdefs.h>
#include <sys/systm.h>

#include <rump/rump.h>
#include <rump/rumpnet_if_pub.h>

#include "rump_private.h"
#include "rumpnet_if_priv.h"

void __dead rump_net_unavailable(void);
void __dead
rump_net_unavailable(void)
{

	panic("net interface unavailable");
}

int
rump_pub_shmif_create(const char *arg1, int *arg2)
{
	int rv;

	rump_schedule();
	rv = rump_shmif_create(arg1, arg2);
	rump_unschedule();

	return rv;
}
__weak_alias(rump_shmif_create,rump_net_unavailable);

/*	$NetBSD: rumpkern_if_wrappers.c,v 1.16 2014/04/25 17:50:28 pooka Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpkern.ifspec,v 1.13 2014/04/25 13:10:42 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.8 2014/04/25 17:50:01 pooka Exp 
 */

#include <sys/cdefs.h>
#include <sys/systm.h>

#include <rump/rump.h>
#include <rump/rumpkern_if_pub.h>

#include "rump_private.h"
#include "rumpkern_if_priv.h"

void __dead rump_kern_unavailable(void);
void __dead
rump_kern_unavailable(void)
{

	panic("kern interface unavailable");
}

int
rump_pub_module_init(const struct modinfo * const *arg1, size_t arg2)
{
	int rv;

	rump_schedule();
	rv = rump_module_init(arg1, arg2);
	rump_unschedule();

	return rv;
}

int
rump_pub_module_fini(const struct modinfo *arg1)
{
	int rv;

	rump_schedule();
	rv = rump_module_fini(arg1);
	rump_unschedule();

	return rv;
}

int
rump_pub_kernelfsym_load(void *arg1, uint64_t arg2, char *arg3, uint64_t arg4)
{
	int rv;

	rump_schedule();
	rv = rump_kernelfsym_load(arg1, arg2, arg3, arg4);
	rump_unschedule();

	return rv;
}

struct uio *
rump_pub_uio_setup(void *arg1, size_t arg2, off_t arg3, enum rump_uiorw arg4)
{
	struct uio * rv;

	rump_schedule();
	rv = rump_uio_setup(arg1, arg2, arg3, arg4);
	rump_unschedule();

	return rv;
}

size_t
rump_pub_uio_getresid(struct uio *arg1)
{
	size_t rv;

	rump_schedule();
	rv = rump_uio_getresid(arg1);
	rump_unschedule();

	return rv;
}

off_t
rump_pub_uio_getoff(struct uio *arg1)
{
	off_t rv;

	rump_schedule();
	rv = rump_uio_getoff(arg1);
	rump_unschedule();

	return rv;
}

size_t
rump_pub_uio_free(struct uio *arg1)
{
	size_t rv;

	rump_schedule();
	rv = rump_uio_free(arg1);
	rump_unschedule();

	return rv;
}

struct kauth_cred*
rump_pub_cred_create(uid_t arg1, gid_t arg2, size_t arg3, gid_t *arg4)
{
	struct kauth_cred* rv;

	rump_schedule();
	rv = rump_cred_create(arg1, arg2, arg3, arg4);
	rump_unschedule();

	return rv;
}

void
rump_pub_cred_put(struct kauth_cred *arg1)
{

	rump_schedule();
	rump_cred_put(arg1);
	rump_unschedule();
}

int
rump_pub_lwproc_rfork(int arg1)
{
	int rv;

	rump_schedule();
	rv = rump_lwproc_rfork(arg1);
	rump_unschedule();

	return rv;
}

int
rump_pub_lwproc_newlwp(pid_t arg1)
{
	int rv;

	rump_schedule();
	rv = rump_lwproc_newlwp(arg1);
	rump_unschedule();

	return rv;
}

void
rump_pub_lwproc_switch(struct lwp *arg1)
{

	rump_schedule();
	rump_lwproc_switch(arg1);
	rump_unschedule();
}

void
rump_pub_lwproc_releaselwp(void)
{

	rump_schedule();
	rump_lwproc_releaselwp();
	rump_unschedule();
}

struct lwp *
rump_pub_lwproc_curlwp(void)
{
	struct lwp * rv;

	rump_schedule();
	rv = rump_lwproc_curlwp();
	rump_unschedule();

	return rv;
}

void
rump_pub_lwproc_sysent_usenative(void)
{

	rump_schedule();
	rump_lwproc_sysent_usenative();
	rump_unschedule();
}

void
rump_pub_allbetsareoff_setid(pid_t arg1, int arg2)
{

	rump_schedule();
	rump_allbetsareoff_setid(arg1, arg2);
	rump_unschedule();
}

int
rump_pub_etfs_register(const char *arg1, const char *arg2, enum rump_etfs_type arg3)
{
	int rv;

	rump_schedule();
	rv = rump_etfs_register(arg1, arg2, arg3);
	rump_unschedule();

	return rv;
}

int
rump_pub_etfs_register_withsize(const char *arg1, const char *arg2, enum rump_etfs_type arg3, uint64_t arg4, uint64_t arg5)
{
	int rv;

	rump_schedule();
	rv = rump_etfs_register_withsize(arg1, arg2, arg3, arg4, arg5);
	rump_unschedule();

	return rv;
}

int
rump_pub_etfs_remove(const char *arg1)
{
	int rv;

	rump_schedule();
	rv = rump_etfs_remove(arg1);
	rump_unschedule();

	return rv;
}

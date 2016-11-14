/*	$NetBSD: rumpvfs_if_wrappers.c,v 1.13 2015/04/23 10:51:20 pooka Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpvfs.ifspec,v 1.10 2015/04/23 10:50:29 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.9 2015/04/23 10:50:00 pooka Exp 
 */

#include <sys/cdefs.h>
#include <sys/systm.h>

#include <rump/rump.h>
#include <rump/rumpvfs_if_pub.h>

#include "rump_private.h"
#include "rumpvfs_if_priv.h"

void __dead rump_vfs_unavailable(void);
void __dead
rump_vfs_unavailable(void)
{

	panic("vfs interface unavailable");
}

void
rump_pub_getvninfo(struct vnode *arg1, enum rump_vtype *arg2, off_t *arg3, dev_t *arg4)
{

	rump_schedule();
	rump_getvninfo(arg1, arg2, arg3, arg4);
	rump_unschedule();
}

struct vfsops *
rump_pub_vfslist_iterate(struct vfsops *arg1)
{
	struct vfsops * rv;

	rump_schedule();
	rv = rump_vfslist_iterate(arg1);
	rump_unschedule();

	return rv;
}

struct vfsops *
rump_pub_vfs_getopsbyname(const char *arg1)
{
	struct vfsops * rv;

	rump_schedule();
	rv = rump_vfs_getopsbyname(arg1);
	rump_unschedule();

	return rv;
}

struct vattr *
rump_pub_vattr_init(void)
{
	struct vattr * rv;

	rump_schedule();
	rv = rump_vattr_init();
	rump_unschedule();

	return rv;
}

void
rump_pub_vattr_settype(struct vattr *arg1, enum rump_vtype arg2)
{

	rump_schedule();
	rump_vattr_settype(arg1, arg2);
	rump_unschedule();
}

void
rump_pub_vattr_setmode(struct vattr *arg1, mode_t arg2)
{

	rump_schedule();
	rump_vattr_setmode(arg1, arg2);
	rump_unschedule();
}

void
rump_pub_vattr_setrdev(struct vattr *arg1, dev_t arg2)
{

	rump_schedule();
	rump_vattr_setrdev(arg1, arg2);
	rump_unschedule();
}

void
rump_pub_vattr_free(struct vattr *arg1)
{

	rump_schedule();
	rump_vattr_free(arg1);
	rump_unschedule();
}

void
rump_pub_vp_incref(struct vnode *arg1)
{

	rump_schedule();
	rump_vp_incref(arg1);
	rump_unschedule();
}

int
rump_pub_vp_getref(struct vnode *arg1)
{
	int rv;

	rump_schedule();
	rv = rump_vp_getref(arg1);
	rump_unschedule();

	return rv;
}

void
rump_pub_vp_rele(struct vnode *arg1)
{

	rump_schedule();
	rump_vp_rele(arg1);
	rump_unschedule();
}

void
rump_pub_vp_interlock(struct vnode *arg1)
{

	rump_schedule();
	rump_vp_interlock(arg1);
	rump_unschedule();
}

void
rump_pub_freecn(struct componentname *arg1, int arg2)
{

	rump_schedule();
	rump_freecn(arg1, arg2);
	rump_unschedule();
}

int
rump_pub_namei(uint32_t arg1, uint32_t arg2, const char *arg3, struct vnode **arg4, struct vnode **arg5, struct componentname **arg6)
{
	int rv;

	rump_schedule();
	rv = rump_namei(arg1, arg2, arg3, arg4, arg5, arg6);
	rump_unschedule();

	return rv;
}

struct componentname *
rump_pub_makecn(u_long arg1, u_long arg2, const char *arg3, size_t arg4, struct kauth_cred *arg5, struct lwp *arg6)
{
	struct componentname * rv;

	rump_schedule();
	rv = rump_makecn(arg1, arg2, arg3, arg4, arg5, arg6);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_unmount(struct mount *arg1, int arg2)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_unmount(arg1, arg2);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_root(struct mount *arg1, struct vnode **arg2, int arg3)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_root(arg1, arg2, arg3);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_statvfs(struct mount *arg1, struct statvfs *arg2)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_statvfs(arg1, arg2);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_sync(struct mount *arg1, int arg2, struct kauth_cred *arg3)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_sync(arg1, arg2, arg3);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_fhtovp(struct mount *arg1, struct fid *arg2, struct vnode **arg3)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_fhtovp(arg1, arg2, arg3);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_vptofh(struct vnode *arg1, struct fid *arg2, size_t *arg3)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_vptofh(arg1, arg2, arg3);
	rump_unschedule();

	return rv;
}

int
rump_pub_vfs_extattrctl(struct mount *arg1, int arg2, struct vnode *arg3, int arg4, const char *arg5)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_extattrctl(arg1, arg2, arg3, arg4, arg5);
	rump_unschedule();

	return rv;
}

void
rump_pub_vfs_syncwait(struct mount *arg1)
{

	rump_schedule();
	rump_vfs_syncwait(arg1);
	rump_unschedule();
}

int
rump_pub_vfs_getmp(const char *arg1, struct mount **arg2)
{
	int rv;

	rump_schedule();
	rv = rump_vfs_getmp(arg1, arg2);
	rump_unschedule();

	return rv;
}

void
rump_pub_vfs_mount_print(const char *arg1, int arg2)
{

	rump_schedule();
	rump_vfs_mount_print(arg1, arg2);
	rump_unschedule();
}

int
rump_pub_syspuffs_glueinit(int arg1, int *arg2)
{
	int rv;

	rump_schedule();
	rv = rump_syspuffs_glueinit(arg1, arg2);
	rump_unschedule();

	return rv;
}
__weak_alias(rump_syspuffs_glueinit,rump_vfs_unavailable);

#ifdef COMPAT_50
void
rump_pub_vattr50_to_vattr(const struct vattr *arg1, struct vattr *arg2)
{

	rump_schedule();
	rump_vattr50_to_vattr(arg1, arg2);
	rump_unschedule();
}
#else
__strong_alias(rump_pub_vattr50_to_vattr,rump_vfs_unavailable);
#endif /* COMPAT_50 */

#ifdef COMPAT_50
void
rump_pub_vattr_to_vattr50(const struct vattr *arg1, struct vattr *arg2)
{

	rump_schedule();
	rump_vattr_to_vattr50(arg1, arg2);
	rump_unschedule();
}
#else
__strong_alias(rump_pub_vattr_to_vattr50,rump_vfs_unavailable);
#endif /* COMPAT_50 */

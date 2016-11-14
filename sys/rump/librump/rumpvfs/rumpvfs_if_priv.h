/*	$NetBSD: rumpvfs_if_priv.h,v 1.13 2015/04/23 10:51:20 pooka Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpvfs.ifspec,v 1.10 2015/04/23 10:50:29 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.9 2015/04/23 10:50:00 pooka Exp 
 */

#ifndef _RUMP_PRIF_VFS_H_
#define _RUMP_PRIF_VFS_H_

void rump_getvninfo(struct vnode *, enum rump_vtype *, off_t *, dev_t *);
typedef void (*rump_getvninfo_fn)(struct vnode *, enum rump_vtype *, off_t *, dev_t *);
struct vfsops * rump_vfslist_iterate(struct vfsops *);
typedef struct vfsops * (*rump_vfslist_iterate_fn)(struct vfsops *);
struct vfsops * rump_vfs_getopsbyname(const char *);
typedef struct vfsops * (*rump_vfs_getopsbyname_fn)(const char *);
struct vattr * rump_vattr_init(void);
typedef struct vattr * (*rump_vattr_init_fn)(void);
void rump_vattr_settype(struct vattr *, enum rump_vtype);
typedef void (*rump_vattr_settype_fn)(struct vattr *, enum rump_vtype);
void rump_vattr_setmode(struct vattr *, mode_t);
typedef void (*rump_vattr_setmode_fn)(struct vattr *, mode_t);
void rump_vattr_setrdev(struct vattr *, dev_t);
typedef void (*rump_vattr_setrdev_fn)(struct vattr *, dev_t);
void rump_vattr_free(struct vattr *);
typedef void (*rump_vattr_free_fn)(struct vattr *);
void rump_vp_incref(struct vnode *);
typedef void (*rump_vp_incref_fn)(struct vnode *);
int rump_vp_getref(struct vnode *);
typedef int (*rump_vp_getref_fn)(struct vnode *);
void rump_vp_rele(struct vnode *);
typedef void (*rump_vp_rele_fn)(struct vnode *);
void rump_vp_interlock(struct vnode *);
typedef void (*rump_vp_interlock_fn)(struct vnode *);
void rump_freecn(struct componentname *, int);
typedef void (*rump_freecn_fn)(struct componentname *, int);
int rump_namei(uint32_t, uint32_t, const char *, struct vnode **, struct vnode **, struct componentname **);
typedef int (*rump_namei_fn)(uint32_t, uint32_t, const char *, struct vnode **, struct vnode **, struct componentname **);
struct componentname * rump_makecn(u_long, u_long, const char *, size_t, struct kauth_cred *, struct lwp *);
typedef struct componentname * (*rump_makecn_fn)(u_long, u_long, const char *, size_t, struct kauth_cred *, struct lwp *);
int rump_vfs_unmount(struct mount *, int);
typedef int (*rump_vfs_unmount_fn)(struct mount *, int);
int rump_vfs_root(struct mount *, struct vnode **, int);
typedef int (*rump_vfs_root_fn)(struct mount *, struct vnode **, int);
int rump_vfs_statvfs(struct mount *, struct statvfs *);
typedef int (*rump_vfs_statvfs_fn)(struct mount *, struct statvfs *);
int rump_vfs_sync(struct mount *, int, struct kauth_cred *);
typedef int (*rump_vfs_sync_fn)(struct mount *, int, struct kauth_cred *);
int rump_vfs_fhtovp(struct mount *, struct fid *, struct vnode **);
typedef int (*rump_vfs_fhtovp_fn)(struct mount *, struct fid *, struct vnode **);
int rump_vfs_vptofh(struct vnode *, struct fid *, size_t *);
typedef int (*rump_vfs_vptofh_fn)(struct vnode *, struct fid *, size_t *);
int rump_vfs_extattrctl(struct mount *, int, struct vnode *, int, const char *);
typedef int (*rump_vfs_extattrctl_fn)(struct mount *, int, struct vnode *, int, const char *);
void rump_vfs_syncwait(struct mount *);
typedef void (*rump_vfs_syncwait_fn)(struct mount *);
int rump_vfs_getmp(const char *, struct mount **);
typedef int (*rump_vfs_getmp_fn)(const char *, struct mount **);
void rump_vfs_mount_print(const char *, int);
typedef void (*rump_vfs_mount_print_fn)(const char *, int);
int rump_syspuffs_glueinit(int, int *);
typedef int (*rump_syspuffs_glueinit_fn)(int, int *);
void rump_vattr50_to_vattr(const struct vattr *, struct vattr *);
typedef void (*rump_vattr50_to_vattr_fn)(const struct vattr *, struct vattr *);
void rump_vattr_to_vattr50(const struct vattr *, struct vattr *);
typedef void (*rump_vattr_to_vattr50_fn)(const struct vattr *, struct vattr *);

#endif /* _RUMP_PRIF_VFS_H_ */

/*	$NetBSD: rumpkern_if_priv.h,v 1.16 2014/04/25 17:50:28 pooka Exp $	*/

/*
 * Automatically generated.  DO NOT EDIT.
 * from: NetBSD: rumpkern.ifspec,v 1.13 2014/04/25 13:10:42 pooka Exp 
 * by:   NetBSD: makerumpif.sh,v 1.8 2014/04/25 17:50:01 pooka Exp 
 */

#ifndef _RUMP_PRIF_KERN_H_
#define _RUMP_PRIF_KERN_H_

int rump_module_init(const struct modinfo * const *, size_t);
typedef int (*rump_module_init_fn)(const struct modinfo * const *, size_t);
int rump_module_fini(const struct modinfo *);
typedef int (*rump_module_fini_fn)(const struct modinfo *);
int rump_kernelfsym_load(void *, uint64_t, char *, uint64_t);
typedef int (*rump_kernelfsym_load_fn)(void *, uint64_t, char *, uint64_t);
struct uio * rump_uio_setup(void *, size_t, off_t, enum rump_uiorw);
typedef struct uio * (*rump_uio_setup_fn)(void *, size_t, off_t, enum rump_uiorw);
size_t rump_uio_getresid(struct uio *);
typedef size_t (*rump_uio_getresid_fn)(struct uio *);
off_t rump_uio_getoff(struct uio *);
typedef off_t (*rump_uio_getoff_fn)(struct uio *);
size_t rump_uio_free(struct uio *);
typedef size_t (*rump_uio_free_fn)(struct uio *);
struct kauth_cred* rump_cred_create(uid_t, gid_t, size_t, gid_t *);
typedef struct kauth_cred* (*rump_cred_create_fn)(uid_t, gid_t, size_t, gid_t *);
void rump_cred_put(struct kauth_cred *);
typedef void (*rump_cred_put_fn)(struct kauth_cred *);
int rump_lwproc_rfork(int);
typedef int (*rump_lwproc_rfork_fn)(int);
int rump_lwproc_newlwp(pid_t);
typedef int (*rump_lwproc_newlwp_fn)(pid_t);
void rump_lwproc_switch(struct lwp *);
typedef void (*rump_lwproc_switch_fn)(struct lwp *);
void rump_lwproc_releaselwp(void);
typedef void (*rump_lwproc_releaselwp_fn)(void);
struct lwp * rump_lwproc_curlwp(void);
typedef struct lwp * (*rump_lwproc_curlwp_fn)(void);
void rump_lwproc_sysent_usenative(void);
typedef void (*rump_lwproc_sysent_usenative_fn)(void);
void rump_allbetsareoff_setid(pid_t, int);
typedef void (*rump_allbetsareoff_setid_fn)(pid_t, int);
int rump_etfs_register(const char *, const char *, enum rump_etfs_type);
typedef int (*rump_etfs_register_fn)(const char *, const char *, enum rump_etfs_type);
int rump_etfs_register_withsize(const char *, const char *, enum rump_etfs_type, uint64_t, uint64_t);
typedef int (*rump_etfs_register_withsize_fn)(const char *, const char *, enum rump_etfs_type, uint64_t, uint64_t);
int rump_etfs_remove(const char *);
typedef int (*rump_etfs_remove_fn)(const char *);

#endif /* _RUMP_PRIF_KERN_H_ */

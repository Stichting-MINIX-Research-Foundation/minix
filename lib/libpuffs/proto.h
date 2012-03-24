#ifndef PUFFS_PROTO_H
#define PUFFS_PROTO_H

struct puffs_usermount;
struct puffs_node;

/* Function prototypes. */

int fs_new_driver(void);

/* inode.c */
int fs_putnode(void);
void release_node(struct puffs_usermount *pu, struct puffs_node *pn );

/* device.c */
int dev_open(endpoint_t driver_e, dev_t dev, endpoint_t proc_e, int
	flags);
void dev_close(endpoint_t driver_e, dev_t dev);

/* link.c */
int fs_ftrunc(void);
int fs_link(void);
int fs_rdlink(void);
int fs_rename(void);
int fs_unlink(void);

/* misc.c */
int fs_flush(void);
int fs_sync(void);

/* mount.c */
int fs_mountpoint(void);
int fs_readsuper(void);
int fs_unmount(void);

/* open.c */
int fs_create(void);
int fs_inhibread(void);
int fs_mkdir(void);
int fs_mknod(void);
int fs_slink(void);

/* path.c */
int fs_lookup(void);
struct puffs_node *advance(struct puffs_node *dirp, char string[NAME_MAX
	+ 1], int chk_perm);

/* protect.c */
int fs_chmod(void);
int fs_chown(void);
int fs_getdents(void);
int forbidden(struct puffs_node *rip, mode_t access_desired);

/* read.c */
int fs_breadwrite(void);
int fs_readwrite(void);

/* stadir.c */
int fs_fstatfs(void);
int fs_stat(void);
int fs_statvfs(void);

/* time.c */
int fs_utime(void);

/* utility.c */
int no_sys(void);
void mfs_nul_f(const char *file, int line, char *str, unsigned int len,
	unsigned int maxlen);
time_t clock_time(void);
int update_times(struct puffs_node *pn, int fl, time_t t);
void lpuffs_debug(const char *format, ...);

#endif /* PUFFS_PROTO_H */

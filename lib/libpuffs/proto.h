#ifndef PUFFS_PROTO_H
#define PUFFS_PROTO_H

struct puffs_usermount;
struct puffs_node;

/* Function prototypes. */

_PROTOTYPE( int fs_new_driver, (void)					);

/* inode.c */
_PROTOTYPE( int fs_putnode, (void)					);
_PROTOTYPE( void release_node, (struct puffs_usermount *pu,
	                        struct puffs_node *pn )			);

/* device.c */
_PROTOTYPE( int dev_open, (endpoint_t driver_e, dev_t dev,
			endpoint_t proc_e, int flags)			);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, dev_t dev)		);

/* link.c */
_PROTOTYPE( int fs_ftrunc, (void)					);
_PROTOTYPE( int fs_link, (void)						);
_PROTOTYPE( int fs_rdlink, (void)					);
_PROTOTYPE( int fs_rename, (void)					);
_PROTOTYPE( int fs_unlink, (void)					);

/* misc.c */
_PROTOTYPE( int fs_flush, (void)					);
_PROTOTYPE( int fs_sync, (void)						);

/* mount.c */
_PROTOTYPE( int fs_mountpoint, (void)					);
_PROTOTYPE( int fs_readsuper, (void)                                    );
_PROTOTYPE( int fs_unmount, (void)					);

/* open.c */
_PROTOTYPE( int fs_create, (void)					);
_PROTOTYPE( int fs_inhibread, (void)					);
_PROTOTYPE( int fs_mkdir, (void)					);
_PROTOTYPE( int fs_mknod, (void)					);
_PROTOTYPE( int fs_slink, (void)					);

/* path.c */
_PROTOTYPE( int fs_lookup, (void)					);
_PROTOTYPE( struct puffs_node *advance, (struct puffs_node *dirp,
                                char string[NAME_MAX + 1], int chk_perm));

/* protect.c */
_PROTOTYPE( int fs_chmod, (void)					);
_PROTOTYPE( int fs_chown, (void)					);
_PROTOTYPE( int fs_getdents, (void)					);
_PROTOTYPE( int forbidden, (struct puffs_node *rip,
			mode_t access_desired)				);

/* read.c */
_PROTOTYPE( int fs_breadwrite, (void)					);
_PROTOTYPE( int fs_readwrite, (void)					);

/* stadir.c */
_PROTOTYPE( int fs_fstatfs, (void)					);
_PROTOTYPE( int fs_stat, (void)						);
_PROTOTYPE( int fs_statvfs, (void)					);

/* time.c */
_PROTOTYPE( int fs_utime, (void)					);

/* utility.c */
_PROTOTYPE( int no_sys, (void)                                          );
_PROTOTYPE( void mfs_nul_f, (const char *file, int line, char *str,
                             unsigned int len, unsigned int maxlen)     );
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( int update_times, (struct puffs_node *pn, int fl, time_t t) );
_PROTOTYPE( void lpuffs_debug, (const char *format, ...)			);

#endif /* PUFFS_PROTO_H */

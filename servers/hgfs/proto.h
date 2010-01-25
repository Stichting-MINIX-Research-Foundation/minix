
/* dentry.c */
_PROTOTYPE( void init_dentry, (void)					);
_PROTOTYPE( struct inode *lookup_dentry, (struct inode *parent,
						char *name)		);
_PROTOTYPE( void add_dentry, (struct inode *parent, char *name,
						struct inode *ino)	);
_PROTOTYPE( void del_dentry, (struct inode *ino)			);

/* handle.c */
_PROTOTYPE( int get_handle, (struct inode *ino)				);
_PROTOTYPE( void put_handle, (struct inode *ino)			);

/* inode.c */
_PROTOTYPE( struct inode *init_inode, (void)				);
_PROTOTYPE( struct inode *find_inode, (ino_t ino_nr)			);
_PROTOTYPE( void get_inode, (struct inode *ino)				);
_PROTOTYPE( void put_inode, (struct inode *ino)				);
_PROTOTYPE( void link_inode, (struct inode *parent, struct inode *ino)	);
_PROTOTYPE( void unlink_inode, (struct inode *ino)			);
_PROTOTYPE( struct inode *get_free_inode, (void)			);
_PROTOTYPE( int have_free_inode, (void)					);
_PROTOTYPE( int have_used_inode, (void)					);
_PROTOTYPE( int do_putnode, (void)					);

/* link.c */
_PROTOTYPE( int do_create, (void)					);
_PROTOTYPE( int do_mkdir, (void)					);
_PROTOTYPE( int do_unlink, (void)					);
_PROTOTYPE( int do_rmdir, (void)					);
_PROTOTYPE( int do_rename, (void)					);

/* lookup.c */
_PROTOTYPE( int do_lookup, (void)					);

/* main.c */
_PROTOTYPE( int main, (int argc, char *argv[])				);

/* misc.c */
_PROTOTYPE( int do_fstatfs, (void)					);

/* mount.c */
_PROTOTYPE( int do_readsuper, (void)					);
_PROTOTYPE( int do_unmount, (void)					);

/* name.c */
_PROTOTYPE( void normalize_name, (char dst[NAME_MAX+1], char *src)	);
_PROTOTYPE( int compare_name, (char *name1, char *name2)		);

/* path.c */
_PROTOTYPE( int make_path, (char path[PATH_MAX], struct inode *ino)	);
_PROTOTYPE( int push_path, (char path[PATH_MAX], char *name)		);
_PROTOTYPE( void pop_path, (char path[PATH_MAX])			);

/* read.c */
_PROTOTYPE( int do_read, (void)						);
_PROTOTYPE( int do_getdents, (void)					);

/* stat.c */
_PROTOTYPE( mode_t get_mode, (struct inode *ino, int mode)		);
_PROTOTYPE( int do_stat, (void)						);
_PROTOTYPE( int do_chmod, (void)					);
_PROTOTYPE( int do_utime, (void)					);

/* util.c */
_PROTOTYPE( int get_name, (cp_grant_id_t grant, size_t len,
						char name[NAME_MAX+1])	);
_PROTOTYPE( int do_noop, (void)						);
_PROTOTYPE( int no_sys, (void)						);

/* verify.c */
_PROTOTYPE( int verify_path, (char *path, struct inode *ino,
			struct hgfs_attr *attr, int *stale)		);
_PROTOTYPE( int verify_inode, (struct inode *ino, char path[PATH_MAX],
			struct hgfs_attr *attr)				);
_PROTOTYPE( int verify_dentry, (struct inode *parent,
			char name[NAME_MAX+1], char path[PATH_MAX],
			struct inode **res_ino)				);

/* write.c */
_PROTOTYPE( int do_write, (void)					);
_PROTOTYPE( int do_ftrunc, (void)					);

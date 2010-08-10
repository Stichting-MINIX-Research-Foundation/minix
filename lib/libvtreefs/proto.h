#ifndef _VTREEFS_PROTO_H
#define _VTREEFS_PROTO_H

/* inode.c */
_PROTOTYPE( void init_inodes, (unsigned int inodes,
	struct inode_stat *stat, index_t nr_indexed_entries)		);
_PROTOTYPE( void cleanup_inodes, (void)					);
_PROTOTYPE( struct inode *find_inode, (ino_t num)			);
_PROTOTYPE( struct inode *get_inode, (ino_t num)			);
_PROTOTYPE( void put_inode, (struct inode *node)			);
_PROTOTYPE( void ref_inode, (struct inode *node)			);
_PROTOTYPE( int get_inode_number, (struct inode *node)			);
_PROTOTYPE( int is_inode_deleted, (struct inode *node)			);
_PROTOTYPE( int fs_putnode, (void)	 				);

/* link.c */
_PROTOTYPE( int fs_rdlink, (void)					);

/* mount.c */
_PROTOTYPE( int fs_readsuper, (void)					);
_PROTOTYPE( int fs_unmount, (void)					);

/* path.c */
_PROTOTYPE( int fs_lookup, (void)					);

/* read.c */
_PROTOTYPE( int fs_read, (void)						);
_PROTOTYPE( int fs_getdents, (void)					);

/* sdbm.c */
_PROTOTYPE( long sdbm_hash, (char *str, int len)			);

/* stadir.c */
_PROTOTYPE( int fs_stat, (void)						);
_PROTOTYPE( int fs_fstatfs, (void)					);
_PROTOTYPE( int fs_statvfs, (void)					);

/* utility.c */
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( int do_noop, (void)						);

#endif /* _VTREEFS_PROTO_H */

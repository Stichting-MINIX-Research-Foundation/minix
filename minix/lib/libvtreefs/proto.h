#ifndef _VTREEFS_PROTO_H
#define _VTREEFS_PROTO_H

/* inode.c */
void init_inodes(unsigned int inodes, struct inode_stat *stat, index_t
	nr_indexed_entries);
void cleanup_inodes(void);
struct inode *find_inode(ino_t num);
struct inode *get_inode(ino_t num);
void put_inode(struct inode *node);
void ref_inode(struct inode *node);
int get_inode_number(struct inode *node);
int is_inode_deleted(struct inode *node);
int fs_putnode(void);

/* link.c */
int fs_rdlink(void);

/* mount.c */
int fs_readsuper(void);
int fs_unmount(void);

/* path.c */
int fs_lookup(void);

/* read.c */
int fs_read(void);
int fs_getdents(void);

/* sdbm.c */
long sdbm_hash(char *str, int len);

/* stadir.c */
int fs_stat(void);
int fs_statvfs(void);

/* utility.c */
int no_sys(void);
int do_noop(void);

#endif /* _VTREEFS_PROTO_H */

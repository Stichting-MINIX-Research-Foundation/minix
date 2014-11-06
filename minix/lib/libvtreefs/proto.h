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
int get_inode_number(const struct inode *node);
int is_inode_deleted(const struct inode *node);
int fs_putnode(ino_t ino_nr, unsigned int count);

/* link.c */
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes);

/* mount.c */
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags);
void fs_unmount(void);

/* main.c */
void fs_other(const message *m_ptr, int ipc_status);

/* path.c */
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);

/* read.c */
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos);

/* sdbm.c */
long sdbm_hash(const char *str, int len);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *buf);
int fs_statvfs(struct statvfs *buf);

#endif /* _VTREEFS_PROTO_H */

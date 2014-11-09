#ifndef _VTREEFS_PROTO_H
#define _VTREEFS_PROTO_H

/* extra.c */
int init_extra(unsigned int inodes, size_t inode_extra);
void clear_inode_extra(struct inode *node);

/* file.c */
int init_buf(size_t size);
void cleanup_buf(void);
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_write(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
int fs_trunc(ino_t ino_nr, off_t start_pos, off_t end_pos);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos);

/* inode.c */
int init_inodes(unsigned int inodes, struct inode_stat *stat,
	index_t nr_indexed_entries);
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
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes);
int fs_mknod(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	dev_t rdev);
int fs_unlink(ino_t dir_nr, char *name, int call);

/* mount.c */
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags);
void fs_unmount(void);

/* main.c */
void fs_other(const message *m_ptr, int ipc_status);

/* path.c */
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);

/* sdbm.c */
long sdbm_hash(const char *str, int len);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *buf);
int fs_chmod(ino_t ino_nr, mode_t *mode);
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode);
int fs_statvfs(struct statvfs *buf);

#endif /* _VTREEFS_PROTO_H */

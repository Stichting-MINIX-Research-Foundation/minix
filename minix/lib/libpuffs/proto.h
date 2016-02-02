#ifndef PUFFS_PROTO_H
#define PUFFS_PROTO_H

struct puffs_usermount;
struct puffs_node;

/* Function prototypes. */

/* inode.c */
int fs_putnode(ino_t ino_nr, unsigned int count);
void release_node(struct puffs_usermount *pu, struct puffs_node *pn );

/* link.c */
int fs_trunc(ino_t ino_nr, off_t start, off_t end);
int fs_link(ino_t dir_nr, char *name, ino_t ino_nr);
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes);
int fs_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name);
int fs_unlink(ino_t dir_nr, char *name, int call);

/* misc.c */
void fs_sync(void);

/* mount.c */
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags);
void fs_unmount(void);
int fs_mountpt(ino_t ino_nr);

/* open.c */
int fs_create(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	struct fsdriver_node *node);
int fs_mkdir(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid);
int fs_mknod(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	dev_t dev);
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes);

/* path.c */
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);
struct puffs_node *advance(struct puffs_node *dirp, char string[NAME_MAX + 1]);

/* protect.c */
int fs_chmod(ino_t ino_nr, mode_t *mode);
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode);

/* read.c */
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_write(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos);

/* stadir.c */
int fs_stat(ino_t ino, struct stat *statbuf);
int fs_statvfs(struct statvfs *st);

/* time.c */
int fs_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime);

/* utility.c */
void *find_inode_cb(struct puffs_usermount *pu, struct puffs_node *pn,
	void *arg);
int update_timens(struct puffs_node *pn, int fl, struct timespec *);

#endif /* PUFFS_PROTO_H */

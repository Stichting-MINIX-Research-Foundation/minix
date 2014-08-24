#ifndef _SFFS_PROTO_H
#define _SFFS_PROTO_H

/* dentry.c */
void init_dentry(void);
struct inode *lookup_dentry(struct inode *parent, char *name);
void add_dentry(struct inode *parent, char *name, struct inode *ino);
void del_dentry(struct inode *ino);

/* handle.c */
int get_handle(struct inode *ino);
void put_handle(struct inode *ino);

/* inode.c */
struct inode *init_inode(void);
struct inode *find_inode(ino_t ino_nr);
void get_inode(struct inode *ino);
void put_inode(struct inode *ino);
void link_inode(struct inode *parent, struct inode *ino);
void unlink_inode(struct inode *ino);
struct inode *get_free_inode(void);
int have_free_inode(void);
int have_used_inode(void);
int do_putnode(ino_t ino_nr, unsigned int count);

/* link.c */
int do_create(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid,
	struct fsdriver_node *node);
int do_mkdir(ino_t dir_nr, char *name, mode_t mode, uid_t uid, gid_t gid);
int do_unlink(ino_t dir_nr, char *name, int call);
int do_rmdir(ino_t dir_nr, char *name, int call);
int do_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name);

/* lookup.c */
int do_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);

/* main.c */
int main(int argc, char *argv[]);

/* misc.c */
int do_statvfs(struct statvfs *statvfs);

/* mount.c */
int do_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags);
void do_unmount(void);

/* name.c */
void normalize_name(char dst[NAME_MAX+1], char *src);
int compare_name(char *name1, char *name2);

/* path.c */
int make_path(char path[PATH_MAX], struct inode *ino);
int push_path(char path[PATH_MAX], char *name);
void pop_path(char path[PATH_MAX]);

/* read.c */
ssize_t do_read(ino_t ino_nr, struct fsdriver_data *data, size_t count,
	off_t pos, int call);
ssize_t do_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos);

/* stat.c */
mode_t get_mode(struct inode *ino, int mode);
int do_stat(ino_t ino_nr, struct stat *stat);
int do_chmod(ino_t ino_nr, mode_t *mode);
int do_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime);

/* verify.c */
int verify_path(char *path, struct inode *ino, struct sffs_attr *attr,
	int *stale);
int verify_inode(struct inode *ino, char path[PATH_MAX],
	struct sffs_attr *attr);
int verify_dentry(struct inode *parent, char name[NAME_MAX+1],
	char path[PATH_MAX], struct inode **res_ino);

/* write.c */
ssize_t do_write(ino_t ino_nr, struct fsdriver_data *data, size_t count,
	off_t pos, int call);
int do_trunc(ino_t ino_nr, off_t start, off_t end);

#endif /* _SFFS_PROTO_H */

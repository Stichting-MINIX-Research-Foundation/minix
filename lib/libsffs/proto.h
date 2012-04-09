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
int do_putnode(void);

/* link.c */
int do_create(void);
int do_mkdir(void);
int do_unlink(void);
int do_rmdir(void);
int do_rename(void);

/* lookup.c */
int do_lookup(void);

/* main.c */
int main(int argc, char *argv[]);

/* misc.c */
int do_fstatfs(void);
int do_statvfs(void);

/* mount.c */
int do_readsuper(void);
int do_unmount(void);

/* name.c */
void normalize_name(char dst[NAME_MAX+1], char *src);
int compare_name(char *name1, char *name2);

/* path.c */
int make_path(char path[PATH_MAX], struct inode *ino);
int push_path(char path[PATH_MAX], char *name);
void pop_path(char path[PATH_MAX]);

/* read.c */
int do_read(void);
int do_getdents(void);

/* stat.c */
mode_t get_mode(struct inode *ino, int mode);
int do_stat(void);
int do_chmod(void);
int do_utime(void);

/* util.c */
int get_name(cp_grant_id_t grant, size_t len, char name[NAME_MAX+1]);
int do_noop(void);
int no_sys(void);

/* verify.c */
int verify_path(char *path, struct inode *ino, struct sffs_attr *attr,
	int *stale);
int verify_inode(struct inode *ino, char path[PATH_MAX],
	struct sffs_attr *attr);
int verify_dentry(struct inode *parent, char name[NAME_MAX+1],
	char path[PATH_MAX], struct inode **res_ino);

/* write.c */
int do_write(void);
int do_ftrunc(void);

#endif /* _SFFS_PROTO_H */

#ifndef FFS_PROTO_H
#define FFS_PROTO_H

#define put_block(n) lmfs_put_block(n)

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct inode;
struct super_block;
struct cg;
struct timespec;

/* balloc.c */
struct cg *ffs_read_cg(u_int cg);
void ffs_write_cg(u_int cg, struct cg *cgp);
daddr_t ffs_alloc(struct inode *rip, daddr_t lbn, daddr_t bpref, int size);
daddr_t ffs_realloccg(struct inode *rip, daddr_t lbn, daddr_t bprev,
	daddr_t bpref, int osize, int nsize);
void ffs_blkfree(struct inode *rip, daddr_t bno, long size);
daddr_t ffs_blkpref(struct inode *rip, daddr_t lbn, int indx, int64_t *bap);

/* ialloc.c */
struct inode *alloc_inode(struct inode *parent, mode_t bits, uid_t uid,
	gid_t gid);
void free_inode(struct inode *rip);

/* inode.c */
void init_inode_cache(void);
struct inode *get_inode(dev_t dev, ino_t numb);
struct inode *find_inode(dev_t dev, ino_t numb);
void put_inode(struct inode *rip);
void dup_inode(struct inode *ip);
void update_times(struct inode *rip);
int inline_symlink(const struct inode *rip);
void rw_inode(struct inode *rip, int rw_flag);
int fs_putnode(ino_t ino_nr, unsigned int count);
void fs_seek(ino_t ino_nr);

/* link.c */
int fs_link(ino_t dir_nr, char *name, ino_t ino_nr);
int fs_unlink(ino_t dir_nr, char *name, int call);
int fs_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name);
int fs_trunc(ino_t ino_nr, off_t start, off_t end);
int truncate_inode(struct inode *rip, off_t len);

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
	dev_t rdev);
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes);

/* path.c */
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);
struct inode *advance(struct inode *dirp, const char *string);
int search_dir(struct inode *ldir_ptr, const char *string, ino_t *numb,
	int flag, mode_t mode);

/* protect.c */
int fs_chmod(ino_t ino_nr, mode_t *mode);
int fs_chown(ino_t ino_nr, uid_t uid, gid_t gid, mode_t *mode);

/* read.c */
ssize_t fs_readwrite(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp);
block64_t read_map(struct inode *rip, off_t pos, int opportunistic);
struct buf *get_block_map(struct inode *rip, uint64_t position);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *statbuf);
int fs_statvfs(struct statvfs *st);
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes);

/* super.c */
unsigned int get_block_size(dev_t dev);
struct super_block *get_super(dev_t dev);
int read_super(struct super_block *sp);
void write_super(struct super_block *sp);
void free_super(struct super_block *sp);

/* time.c */
int fs_utime(ino_t ino_nr, struct timespec *atime, struct timespec *mtime);

/* utility.c */
struct buf *get_block(dev_t dev, block64_t block, int how);

/* write.c */
block64_t ffs_balloc(struct inode *rip, off_t off, int size);
struct buf *new_block(struct inode *rip, off_t position);
void zero_block(struct buf *bp);

#endif /* FFS_PROTO_H */

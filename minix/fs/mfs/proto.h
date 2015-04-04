#ifndef __MFS_PROTO_H__
#define __MFS_PROTO_H__

/* Some shortcuts to functions in -lminixfs */
#define put_block(b) lmfs_put_block(b)

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;
struct super_block;

/* cache.c */
zone_t alloc_zone(dev_t dev, zone_t z);
void free_zone(dev_t dev, zone_t numb);
struct buf *get_block(dev_t dev, block_t block, int how);

/* inode.c */
struct inode *alloc_inode(dev_t dev, mode_t bits, uid_t uid, gid_t gid);
void dup_inode(struct inode *ip);
struct inode *find_inode(dev_t dev, ino_t numb);
int fs_putnode(ino_t ino_nr, unsigned int count);
void init_inode_cache(void);
struct inode *get_inode(dev_t dev, ino_t numb);
void put_inode(struct inode *rip);
void update_times(struct inode *rip);
void rw_inode(struct inode *rip, int rw_flag);

/* link.c */
int fs_trunc(ino_t ino_nr, off_t start, off_t end);
int fs_link(ino_t dir_nr, char *name, ino_t ino_nr);
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes);
int fs_rename(ino_t old_dir_nr, char *old_name, ino_t new_dir_nr,
	char *new_name);
int fs_unlink(ino_t dir_nr, char *name, int call);
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
	dev_t dev);
int fs_slink(ino_t dir_nr, char *name, uid_t uid, gid_t gid,
	struct fsdriver_data *data, size_t bytes);
void fs_seek(ino_t ino_nr);

/* path.c */
int fs_lookup(ino_t dir_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);
struct inode *advance(struct inode *dirp, const char *string);
int search_dir(struct inode *ldir_ptr, const char *string, ino_t *numb,
	int flag);

/* protect.c */
int fs_chmod(ino_t ino, mode_t *mode);
int fs_chown(ino_t ino, uid_t uid, gid_t gid, mode_t *mode);

/* read.c */
ssize_t fs_readwrite(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
block_t read_map(struct inode *rip, off_t pos, int opportunistic);
struct buf *get_block_map(register struct inode *rip, u64_t position);
zone_t rd_indir(struct buf *bp, int index);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *posp);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *statbuf);
int fs_statvfs(struct statvfs *st);

/* super.c */
bit_t alloc_bit(struct super_block *sp, int map, bit_t origin);
void free_bit(struct super_block *sp, int map, bit_t bit_returned);
unsigned int get_block_size(dev_t dev);
int read_super(struct super_block *sp);
int write_super(struct super_block *sp);

/* stats.c */
bit_t count_free_bits(struct super_block *sp, int map);

/* time.c */
int fs_utime(ino_t ino_t, struct timespec *atime, struct timespec *mtime);

/* utility.c */
unsigned conv2(int norm, int w);
long conv4(int norm, long x);

/* write.c */
void clear_zone(struct inode *rip, off_t pos, int flag);
struct buf *new_block(struct inode *rip, off_t position);
void zero_block(struct buf *bp);
int write_map(struct inode *, off_t, zone_t, int);

#endif


#ifndef __MFS_PROTO_H__
#define __MFS_PROTO_H__

/* Some shortcuts to functions in -lminixfs */
#define get_block(d, b, t) lmfs_get_block(d, b, t)
#define put_block(b, t) lmfs_put_block(b, t)

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;
struct super_block;


/* cache.c */
zone_t alloc_zone(dev_t dev, zone_t z);
void free_zone(dev_t dev, zone_t numb);

/* inode.c */
struct inode *alloc_inode(dev_t dev, mode_t bits);
void dup_inode(struct inode *ip);
struct inode *find_inode(dev_t dev, ino_t numb);
int fs_putnode(void);
void init_inode_cache(void);
struct inode *get_inode(dev_t dev, ino_t numb);
void put_inode(struct inode *rip);
void update_times(struct inode *rip);
void rw_inode(struct inode *rip, int rw_flag);

/* link.c */
int fs_ftrunc(void);
int fs_link(void);
int fs_rdlink(void);
int fs_rename(void);
int fs_unlink(void);
int truncate_inode(struct inode *rip, off_t len);

/* misc.c */
int fs_flush(void);
int fs_sync(void);
int fs_new_driver(void);
int fs_bpeek(void);

/* mount.c */
int fs_mountpoint(void);
int fs_readsuper(void);
int fs_unmount(void);

/* open.c */
int fs_create(void);
int fs_inhibread(void);
int fs_mkdir(void);
int fs_mknod(void);
int fs_slink(void);

/* path.c */
int fs_lookup(void);
struct inode *advance(struct inode *dirp, char string[MFS_NAME_MAX], int
	chk_perm);
int search_dir(struct inode *ldir_ptr, char string [MFS_NAME_MAX], ino_t
	*numb, int flag, int check_permissions);


/* protect.c */
int fs_chmod(void);
int fs_chown(void);
int fs_getdents(void);
int forbidden(struct inode *rip, mode_t access_desired);
int read_only(struct inode *ip);

/* read.c */
int fs_breadwrite(void);
int fs_readwrite(void);
void read_ahead(void);
block_t read_map(struct inode *rip, off_t pos, int opportunistic);
struct buf *get_block_map(register struct inode *rip, u64_t position);
zone_t rd_indir(struct buf *bp, int index);

/* stadir.c */
int fs_stat(void);
int fs_statvfs(void);

/* super.c */
bit_t alloc_bit(struct super_block *sp, int map, bit_t origin);
void free_bit(struct super_block *sp, int map, bit_t bit_returned);
unsigned int get_block_size(dev_t dev);
struct super_block *get_super(dev_t dev);
int read_super(struct super_block *sp);
int write_super(struct super_block *sp);
u32_t get_used_blocks(struct super_block *sp);

/* stats.c */
bit_t count_free_bits(struct super_block *sp, int map);

/* time.c */
int fs_utime(void);

/* utility.c */
time_t clock_time(void);
unsigned conv2(int norm, int w);
long conv4(int norm, long x);
void mfs_nul_f(char *file, int line, char *str, unsigned int len,
	unsigned int maxlen);
int min(unsigned int l, unsigned int r);
int no_sys(void);
void sanitycheck(char *file, int line);
#define SANITYCHECK sanitycheck(__FILE__, __LINE__)

/* write.c */
void clear_zone(struct inode *rip, off_t pos, int flag);
struct buf *new_block(struct inode *rip, off_t position);
void zero_block(struct buf *bp);
int write_map(struct inode *, off_t, zone_t, int);

#endif


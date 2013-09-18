#ifndef EXT2_PROTO_H
#define EXT2_PROTO_H

#define get_block(d, n, t) lmfs_get_block(d, n, t)
#define put_block(n, t) lmfs_put_block(n, t)

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;
struct inode;
struct super_block;


/* balloc.c */
void discard_preallocated_blocks(struct inode *rip);
block_t alloc_block(struct inode *rip, block_t goal);
void free_block(struct super_block *sp, bit_t bit);

/* ialloc.c */
struct inode *alloc_inode(struct inode *parent, mode_t bits);
void free_inode(struct inode *rip);

/* inode.c */
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
struct inode *advance(struct inode *dirp, char string[NAME_MAX + 1], int
	chk_perm);
int search_dir(struct inode *ldir_ptr, const char string [NAME_MAX + 1], ino_t
	*numb, int flag, int check_permissions, int ftype);

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
block_t rd_indir(struct buf *bp, int index);
block_t read_map(struct inode *rip, off_t pos, int opportunistic);
struct buf *get_block_map(register struct inode *rip, u64_t position);

/* stadir.c */
int fs_stat(void);
int fs_statvfs(void);

/* super.c */
unsigned int get_block_size(dev_t dev);
struct super_block *get_super(dev_t dev);
int read_super(struct super_block *sp);
void write_super(struct super_block *sp);
struct group_desc* get_group_desc(unsigned int bnum);

/* time.c */
int fs_utime(void);

/* utility.c */
time_t clock_time(void);
unsigned conv2(int norm, int w);
long conv4(int norm, long x);
void mfs_nul_f(const char *file, int line, const char *str, unsigned int len,
	unsigned int maxlen);
int min(unsigned int l, unsigned int r);
int no_sys(void);
void sanitycheck(char *file, int line);
#define SANITYCHECK sanitycheck(__FILE__, __LINE__)
int ansi_strcmp(register const char* ansi_s, register const char *s2,
	register size_t ansi_s_length);
bit_t setbit(bitchunk_t *bitmap, bit_t max_bits, unsigned int word);
bit_t setbyte(bitchunk_t *bitmap, bit_t max_bits);
int unsetbit(bitchunk_t *bitmap, bit_t bit);

/* write.c */
struct buf *new_block(struct inode *rip, off_t position);
void zero_block(struct buf *bp);
int write_map(struct inode *, off_t, block_t, int);

#endif /* EXT2_PROTO_H */

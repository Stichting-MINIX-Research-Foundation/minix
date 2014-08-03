/* Function prototypes for iso9660 file system. */

struct dir_record;
struct dir_extent;
struct rrii_dir_record;
struct iso9660_dir_record;
struct iso9660_vol_pri_desc;
struct inode;

#include <minix/libminixfs.h>

#define get_block(n) lmfs_get_block(fs_dev, n, NORMAL)
#define put_block(n) lmfs_put_block(n, FULL_DATA_BLOCK)

/* main.c */
int main(int argc, char *argv[]);
void reply(int who, message *m_out);

/* inode.c */
int fs_putnode(void);

struct inode* alloc_inode();
struct inode* find_inode(ino_t i);
void put_inode(struct inode *i);
void dup_inode(struct inode *i_node);
struct inode* get_inode(ino_t i);

int read_inode(struct inode *i_node, struct dir_extent *extent, size_t offset,
	size_t *new_offset);
void read_inode_iso9660(struct inode *i, const struct iso9660_dir_record *dir_rec);
void read_inode_extents(struct inode *i, const struct iso9660_dir_record *dir_rec,
	struct dir_extent *extent, size_t *offset);
void read_inode_susp(struct inode *i, const struct iso9660_dir_record *dir_rec,
	struct buf *bp, size_t offset);

int check_dir_record(const struct iso9660_dir_record *d, size_t offset);

/* link.c */
int fs_rdlink(void);

/* misc.c */
int fs_sync(void);
int fs_new_driver(void);

/* mount.c */
int fs_readsuper(void);
int fs_mountpoint(void);
int fs_unmount(void);

/* path.c */
int fs_lookup(void);
int advance(struct inode *dirp, char string[NAME_MAX], struct
	inode **resp);
int search_dir(struct inode *ldir_ptr, char string [NAME_MAX], ino_t *numb);

/* read.c */
int fs_read(void);
int fs_bread(void);
int fs_getdents(void);
int read_chunk(struct inode *rip, u64_t position, unsigned off, int
	chunk, unsigned left, cp_grant_id_t gid, unsigned buf_off, int
	block_size, int *completed, int rw);


/* stadir.c */
int fs_stat(void);
int fs_statvfs(void);

/* super.c */
int release_vol_pri_desc(struct iso9660_vol_pri_desc *v_pri);
int create_vol_pri_desc(struct iso9660_vol_pri_desc *v_pri, char *buf,
	size_t address);
int read_vds(struct iso9660_vol_pri_desc *v_pri, dev_t dev);

/* susp.c */
int parse_susp(struct rrii_dir_record *dir, char *buffer);
void parse_susp_buffer(struct rrii_dir_record *dir, char *buffer, u32_t size);

/* susp_rock_ridge.c */
void parse_susp_rock_ridge_sl(struct rrii_dir_record *dir, char *buffer, int length);
int parse_susp_rock_ridge(struct rrii_dir_record *dir, char *buffer);

/* utility.c */
struct dir_extent* alloc_extent();
void free_extent(struct dir_extent *extent);
struct buf* read_extent_block(struct dir_extent *e, size_t block);
size_t get_extent_absolute_block_id(struct dir_extent *e, size_t block);

time_t date7_to_time_t(const u8_t *date);
int do_noop(void);
int no_sys(void);


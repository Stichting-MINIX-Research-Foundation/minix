/* Function prototypes for iso9660 file system. */

struct dir_record;
struct dir_extent;
struct rrii_dir_record;
struct iso9660_dir_record;
struct iso9660_vol_pri_desc;
struct inode;

/* inode.c */
int fs_putnode(ino_t ino_nr, unsigned int count);

struct inode* alloc_inode(void);
struct inode* find_inode(ino_t i);
void put_inode(struct inode *i);
void dup_inode(struct inode *i_node);
struct inode* get_inode(ino_t i);

int read_inode(struct inode *i_node, struct dir_extent *extent, size_t offset,
	size_t *new_offset);
void read_inode_iso9660(struct inode *i,
	const struct iso9660_dir_record *dir_rec);
void read_inode_extents(struct inode *i,
	const struct iso9660_dir_record *dir_rec, struct dir_extent *extent,
	size_t *offset);
void read_inode_susp(struct inode *i, const struct iso9660_dir_record *dir_rec,
	struct buf *bp, size_t offset);

int check_dir_record(const struct iso9660_dir_record *d, size_t offset);

int check_inodes(void);

/* link.c */
ssize_t fs_rdlink(ino_t ino_nr, struct fsdriver_data *data, size_t bytes);

/* mount.c */
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *root_node,
	unsigned int *res_flags);
int fs_mountpt(ino_t ino_nr);
void fs_unmount(void);

/* path.c */
int fs_lookup(ino_t ino_nr, char *name, struct fsdriver_node *node,
	int *is_mountpt);

/* read.c */
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *statbuf);
int fs_statvfs(struct statvfs *st);

/* super.c */
int release_vol_pri_desc(struct iso9660_vol_pri_desc *v_pri);
int read_vds(struct iso9660_vol_pri_desc *v_pri, dev_t dev);

/* susp.c */
int parse_susp(struct rrii_dir_record *dir, char *buffer);
void parse_susp_buffer(struct rrii_dir_record *dir, char *buffer, u32_t size);

/* susp_rock_ridge.c */
void parse_susp_rock_ridge_sl(struct rrii_dir_record *dir, char *buffer,
	int length);
int parse_susp_rock_ridge(struct rrii_dir_record *dir, char *buffer);

/* utility.c */
struct dir_extent* alloc_extent(void);
void free_extent(struct dir_extent *extent);
struct buf* read_extent_block(struct dir_extent *e, size_t block);
size_t get_extent_absolute_block_id(struct dir_extent *e, size_t block);

time_t date7_to_time_t(const u8_t *date);

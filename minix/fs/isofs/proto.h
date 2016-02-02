/* Function prototypes for iso9660 file system. */

struct dir_record;
struct dir_extent;
struct rrii_dir_record;
struct iso9660_dir_record;
struct iso9660_vol_pri_desc;
struct inode;
struct inode_dir_entry;

/* inode.c */
int fs_putnode(ino_t ino_nr, unsigned int count);

struct inode* get_inode(ino_t ino_nr);
struct inode* open_inode(ino_t ino_nr);
void put_inode(struct inode *i);
void dup_inode(struct inode *i_node);

int read_directory(struct inode *dir);

int read_inode(struct inode_dir_entry *dir_entry, struct dir_extent *extent,
    size_t *offset);

struct inode* inode_cache_get(ino_t ino_nr);
void inode_cache_add(ino_t ino_nr, struct inode *i_node);

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
int release_vol_pri_desc(struct iso9660_vol_pri_desc *vol_pri);
int read_vds(struct iso9660_vol_pri_desc *v_pri, dev_t dev);

/* susp.c */
int parse_susp(struct rrii_dir_record *dir, char *buffer);
void parse_susp_buffer(struct rrii_dir_record *dir, char *buffer, u32_t size);

/* susp_rock_ridge.c */
void parse_susp_rock_ridge_sl(struct rrii_dir_record *dir, char *buffer,
	int length);
int parse_susp_rock_ridge(struct rrii_dir_record *dir, char *buffer);

/* utility.c */
void free_inode_dir_entry(struct inode_dir_entry *e);
void free_extent(struct dir_extent *extent);
struct buf* read_extent_block(struct dir_extent *e, size_t block);
size_t get_extent_absolute_block_id(struct dir_extent *e, size_t block);

time_t date7_to_time_t(const u8_t *date);
void* alloc_mem(size_t s);
/* Function prototypes for iso9660 file system. */

struct dir_record;
struct ext_attr_rec;
struct iso9660_vd_pri;

int fs_getnode(void);
int fs_putnode(void);
int fs_new_driver(void);
int fs_sync(void);
int lookup(void);
int fs_access(void);
int fs_getdents(void);
int fs_getdents_o(void);

/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void reply, (int who, message *m_out)			);

/* device.c */
_PROTOTYPE( int block_dev_io, (int op, Dev_t dev, int proc, void *buf,
			       u64_t pos, int bytes, int flags));
_PROTOTYPE( int dev_open, (endpoint_t driver_e, Dev_t dev, int proc,
			   int flags));
_PROTOTYPE( void dev_close, (endpoint_t driver_e, Dev_t dev));

/* super.c */
_PROTOTYPE(int release_v_pri,(struct iso9660_vd_pri *v_pri));
_PROTOTYPE(int read_vds,(struct iso9660_vd_pri *v_pri, Dev_t dev));
_PROTOTYPE(int create_v_pri,(struct iso9660_vd_pri *v_pri, char *buffer,unsigned long address));

/* inode.c */
_PROTOTYPE(int release_dir_record,(struct dir_record *dir));
_PROTOTYPE(struct dir_record *get_free_dir_record,(void));
_PROTOTYPE(struct dir_record *get_dir_record,(ino_t id_dir));
_PROTOTYPE(struct ext_attr_rec *get_free_ext_attr,(void));
_PROTOTYPE(int create_ext_attr,(struct ext_attr_rec *ext,
				char *buffer));
_PROTOTYPE(int create_dir_record,(struct dir_record *dir, char *buffer,
				  u32_t address));
_PROTOTYPE(struct dir_record *load_dir_record_from_disk,(u32_t address));

/* path.c */
int fs_lookup_s(void);
_PROTOTYPE(struct dir_record *advance,(struct dir_record **dirp,
				       char string[NAME_MAX]));
_PROTOTYPE( int search_dir, (struct dir_record *ldir_ptr,
			     char string [NAME_MAX], ino_t *numb));
_PROTOTYPE( struct dir_record *parse_path, (char *path,
					    char string[NAME_MAX], 
                                            int action));

/* read.c */
int fs_read_s(void);
int fs_read(void);
int fs_bread(void);
int fs_bread_s(void);
_PROTOTYPE(int read_chunk,(struct dir_record *dir, u64_t position,
			   unsigned off, int chunk, char *buff, int seg, 
			   int usr, int block_size, int *completed));

/* utility.c */
_PROTOTYPE(int no_sys, (void));
_PROTOTYPE(void panic, (char *who, char *mess, int num));

/* cache.c */
_PROTOTYPE(struct buf *get_block,(block_t block));
_PROTOTYPE(void put_block,(struct buf *bp));

/* ids.c */
/* _PROTOTYPE(void hash_init, (void)); */
/* _PROTOTYPE(int assign_id_to_dir_record, (struct dir_record *dir)); */
/* _PROTOTYPE(struct dir_record *get_dir_record_by_id,(int id)); */

/* mount.c */
int fs_readsuper(void);
int fs_readsuper_s(void);
int fs_mountpoint_s(void);
int fs_unmount(void);

/* stadir.c */
int fs_stat(void);
int fs_fstatfs(void);

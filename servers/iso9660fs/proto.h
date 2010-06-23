/* Function prototypes for iso9660 file system. */

struct dir_record;
struct ext_attr_rec;
struct iso9660_vd_pri;


/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void reply, (int who, message *m_out)			);

/* cache.c */
_PROTOTYPE(struct buf *get_block,(block_t block));
_PROTOTYPE(void put_block,(struct buf *bp));

/* device.c */
_PROTOTYPE( int block_dev_io, (int op, dev_t dev, int proc, void *buf,
			       u64_t pos, int bytes, int flags)		);
_PROTOTYPE( int dev_open, (endpoint_t driver_e, dev_t dev, int proc,
			   int flags)					);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, dev_t dev)		);
_PROTOTYPE( int fs_new_driver, (void)					);

/* inode.c */
_PROTOTYPE( int create_dir_record,(struct dir_record *dir, char *buffer,
				   u32_t address)			);
_PROTOTYPE( int create_ext_attr,(struct ext_attr_rec *ext, char *buffer));
_PROTOTYPE( int fs_getnode, (void)					);
_PROTOTYPE( int fs_putnode, (void)					);
_PROTOTYPE( struct dir_record *get_dir_record, (ino_t id_dir)		);
_PROTOTYPE( struct dir_record *get_free_dir_record, (void)		);
_PROTOTYPE( struct ext_attr_rec *get_free_ext_attr, (void)		);
_PROTOTYPE( struct dir_record *load_dir_record_from_disk,(u32_t address));
_PROTOTYPE( int release_dir_record, (struct dir_record *dir)		);

/* misc.c */
_PROTOTYPE( int fs_sync, (void)						);

/* mount.c */
_PROTOTYPE( int fs_readsuper, (void)					);
_PROTOTYPE( int fs_mountpoint, (void)					);
_PROTOTYPE( int fs_unmount, (void)					);

/* path.c */
_PROTOTYPE( int fs_lookup, (void)					);
_PROTOTYPE( int advance, (struct dir_record *dirp, char string[NAME_MAX],
			  struct dir_record **resp)			);
_PROTOTYPE( int search_dir, (struct dir_record *ldir_ptr,
			     char string [NAME_MAX], ino_t *numb)	);

/* protect.c */
_PROTOTYPE( int fs_access, (void)					);

/* read.c */
_PROTOTYPE( int fs_read, (void)						);
_PROTOTYPE( int fs_bread, (void)					);
_PROTOTYPE( int fs_getdents, (void)					);
_PROTOTYPE( int read_chunk, (struct dir_record *rip, u64_t position,
			     unsigned off, int chunk, unsigned left,
			     cp_grant_id_t gid, unsigned buf_off,
			     int block_size, int *completed)		);

/* stadir.c */
_PROTOTYPE( int fs_stat, (void)						);
_PROTOTYPE( int fs_fstatfs, (void)					);
_PROTOTYPE( int fs_statvfs, (void)					);

/* super.c */
_PROTOTYPE(int release_v_pri, (struct iso9660_vd_pri *v_pri)		);
_PROTOTYPE(int read_vds, (struct iso9660_vd_pri *v_pri, dev_t dev)	);
_PROTOTYPE(int create_v_pri, (struct iso9660_vd_pri *v_pri, char *buffer,
			      unsigned long address)			);

/* utility.c */
_PROTOTYPE(int no_sys, (void));


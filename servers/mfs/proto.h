/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;
struct super_block;


/* cache.c */
_PROTOTYPE( zone_t alloc_zone, (Dev_t dev, zone_t z)			);
_PROTOTYPE( void buf_pool, (void)					);
_PROTOTYPE( void flushall, (Dev_t dev)					);
_PROTOTYPE( void free_zone, (Dev_t dev, zone_t numb)			);
_PROTOTYPE( struct buf *get_block, (Dev_t dev, block_t block,int only_search));
_PROTOTYPE( void invalidate, (Dev_t device)				);
_PROTOTYPE( void put_block, (struct buf *bp, int block_type)		);
_PROTOTYPE( void set_blocksize, (int blocksize)				);
_PROTOTYPE( void rw_scattered, (Dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);

/* device.c */
_PROTOTYPE( int block_dev_io, (int op, Dev_t dev, int proc, void *buf,
			u64_t pos, int bytes, int flags)		);
_PROTOTYPE( int dev_open, (endpoint_t driver_e, Dev_t dev, int proc,
							int flags)	);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, Dev_t dev)		);
_PROTOTYPE( int fs_clone_opcl, (void)					);
_PROTOTYPE( int fs_new_driver, (void)					);

/* inode.c */
_PROTOTYPE( struct inode *alloc_inode, (dev_t dev, mode_t bits)		);
_PROTOTYPE( void dup_inode, (struct inode *ip)				);
_PROTOTYPE( struct inode *find_inode, (Dev_t dev, int numb)		);
_PROTOTYPE( void free_inode, (Dev_t dev, Ino_t numb)			);
_PROTOTYPE( int fs_getnode, (void)					);
_PROTOTYPE( int fs_putnode, (void)					);
_PROTOTYPE( void init_inode_cache, (void)				);
_PROTOTYPE( struct inode *get_inode, (Dev_t dev, int numb)		);
_PROTOTYPE( void put_inode, (struct inode *rip)				);
_PROTOTYPE( void update_times, (struct inode *rip)			);
_PROTOTYPE( void rw_inode, (struct inode *rip, int rw_flag)		);
_PROTOTYPE( void wipe_inode, (struct inode *rip)			);

/* link.c */
_PROTOTYPE( int freesp_inode, (struct inode *rip, off_t st, off_t end)	);
_PROTOTYPE( int fs_ftrunc, (void)					);
_PROTOTYPE( int fs_link, (void)						);
_PROTOTYPE( int fs_rdlink, (void)					);
_PROTOTYPE( int fs_rename, (void)					);
_PROTOTYPE( int fs_unlink, (void)					);
_PROTOTYPE( int truncate_inode, (struct inode *rip, off_t len)		);

/* main.c */
_PROTOTYPE( void reply, (int who, message *m_out)			);

/* misc.c */
_PROTOTYPE( int fs_flush, (void)					);
_PROTOTYPE( int fs_sync, (void)						);

/* mount.c */
_PROTOTYPE( int fs_mountpoint, (void)					);
_PROTOTYPE( int fs_readsuper, (void)                                    );
_PROTOTYPE( int fs_unmount, (void)					);

/* open.c */
_PROTOTYPE( int fs_create, (void)					);
_PROTOTYPE( int fs_inhibread, (void)					);
_PROTOTYPE( int fs_mkdir, (void)					);
_PROTOTYPE( int fs_mknod, (void)					);
_PROTOTYPE( int fs_newnode, (void)					);
_PROTOTYPE( int fs_slink, (void)					);

/* path.c */
_PROTOTYPE( int fs_lookup, (void)					);
_PROTOTYPE( struct inode *advance, (struct inode *dirp,
				char string[NAME_MAX], int chk_perm)	);
_PROTOTYPE( int search_dir, (struct inode *ldir_ptr, 
			char string [NAME_MAX], ino_t *numb, int flag,
			     int check_permissions)			);	


/* protect.c */
_PROTOTYPE( int fs_chmod, (void)					);
_PROTOTYPE( int fs_chown, (void)					);
_PROTOTYPE( int fs_getdents, (void)					);
_PROTOTYPE( int forbidden, (struct inode *rip, mode_t access_desired)	);
_PROTOTYPE( int read_only, (struct inode *ip)				);

/* read.c */
_PROTOTYPE( int fs_breadwrite, (void)					);
_PROTOTYPE( int fs_readwrite, (void)					);
_PROTOTYPE( struct buf *rahead, (struct inode *rip, block_t baseblock,
			u64_t position, unsigned bytes_ahead)		);
_PROTOTYPE( void read_ahead, (void)					);
_PROTOTYPE( block_t read_map, (struct inode *rip, off_t pos)		);
_PROTOTYPE( int read_write, (int rw_flag)				);
_PROTOTYPE( zone_t rd_indir, (struct buf *bp, int index)		);

/* stadir.c */
_PROTOTYPE( int fs_fstatfs, (void)					);
_PROTOTYPE( int fs_stat, (void)						);

/* super.c */
_PROTOTYPE( bit_t alloc_bit, (struct super_block *sp, int map, bit_t origin));
_PROTOTYPE( void free_bit, (struct super_block *sp, int map,
						bit_t bit_returned)	);
_PROTOTYPE( int get_block_size, (dev_t dev)				);
_PROTOTYPE( struct super_block *get_super, (Dev_t dev)			);
_PROTOTYPE( int mounted, (struct inode *rip)				);
_PROTOTYPE( int read_super, (struct super_block *sp)			);

/* time.c */
_PROTOTYPE( int fs_utime, (void)					);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( unsigned conv2, (int norm, int w)				);
_PROTOTYPE( long conv4, (int norm, long x)				);
_PROTOTYPE( int fetch_name, (char *path, int len, int flag)		);
_PROTOTYPE( void mfs_nul_f, (char *file, int line, char *str, int len, 
				int maxlen)				);
_PROTOTYPE( int mfs_min_f, (char *file, int line, int len1, int len2)	);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( int isokendpt_f, (char *f, int l, int e, int *p, int ft));
_PROTOTYPE( void sanitycheck, (char *file, int line)			);
#define SANITYCHECK sanitycheck(__FILE__, __LINE__)

/* write.c */
_PROTOTYPE( void clear_zone, (struct inode *rip, off_t pos, int flag)	);
_PROTOTYPE( struct buf *new_block, (struct inode *rip, off_t position)	);
_PROTOTYPE( void zero_block, (struct buf *bp)				);
_PROTOTYPE( int write_map, (struct inode *, off_t, zone_t, int)		);


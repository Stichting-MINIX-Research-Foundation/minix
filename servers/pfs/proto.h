/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;

/* buffer.c */
_PROTOTYPE( struct buf *get_block, (Dev_t dev, ino_t inum)		);
_PROTOTYPE( struct buf *new_block, (Dev_t dev, ino_t inum)			);
_PROTOTYPE( void put_block, (Dev_t dev, ino_t inum)				);

/* cache.c */
_PROTOTYPE( void buf_pool, (void)					);

/* inode.c */
_PROTOTYPE( struct inode *alloc_inode, (dev_t dev, mode_t mode)		);
_PROTOTYPE( void dup_inode, (struct inode *ip)				);
_PROTOTYPE( struct inode *find_inode, (int numb)			);
_PROTOTYPE( void free_inode, (struct inode *rip)			);
_PROTOTYPE( int fs_putnode, (void)					);
_PROTOTYPE( void init_inode_cache, (void)				);
_PROTOTYPE( struct inode *get_inode, (Dev_t dev, int numb)		);
_PROTOTYPE( void put_inode, (struct inode *rip)				);
_PROTOTYPE( void update_times, (struct inode *rip)			);
_PROTOTYPE( void wipe_inode, (struct inode *rip)			);

/* link.c */
_PROTOTYPE( int fs_ftrunc, (void)					);
_PROTOTYPE( int truncate_inode, (struct inode *rip, off_t newsize)				);


/* main.c */
_PROTOTYPE( void reply, (int who, message *m_out)			);

/* misc.c */
_PROTOTYPE( int fs_sync, (void)						);

/* open.c */
_PROTOTYPE( int fs_newnode, (void)					);

/* read.c */
_PROTOTYPE( int fs_readwrite, (void)					);
_PROTOTYPE( block_t read_map, (struct inode *rip, off_t pos)		);
_PROTOTYPE( int read_write, (int rw_flag)				);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( int no_sys, (void)						);

/* stadir.c */
_PROTOTYPE( int fs_stat, (void)						);

/* super.c */
_PROTOTYPE( bit_t alloc_bit, (void)					);
_PROTOTYPE( void free_bit, (bit_t bit_returned)				);

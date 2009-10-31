/* Function prototypes. */

#include "timers.h"

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct filp;		
struct inode;
struct super_block;


int fs_putnode(void);
int fs_getnode(void);
int fs_pipe(void);
int fs_clone_opcl(void);
int fs_new_driver(void);
int fs_ftrunc(void);
int fs_chown(void);
int fs_chmod(void);
int fs_inhibread(void);
int fs_stat(void);
int fs_utime(void);
int fs_fstatfs(void);

int fs_unmount(void);
int fs_trunc(void);
int fs_sync(void);
int fs_stime(void);

int fs_getdents_o(void);
int fs_getdents(void);
int fs_flush(void);

void init_inode_cache(void);

/* cache.c */
_PROTOTYPE( zone_t alloc_zone, (Dev_t dev, zone_t z)			);
_PROTOTYPE( void flushall, (Dev_t dev)					);
_PROTOTYPE( void free_zone, (Dev_t dev, zone_t numb)			);
_PROTOTYPE( struct buf *get_block, (Dev_t dev, block_t block,int only_search));
_PROTOTYPE( void invalidate, (Dev_t device)				);
_PROTOTYPE( void put_block, (struct buf *bp, int block_type)		);
_PROTOTYPE( void set_blocksize, (int blocksize)				);
_PROTOTYPE( void rw_scattered, (Dev_t dev,
			struct buf **bufq, int bufqsize, int rw_flag)	);
_PROTOTYPE( void buf_pool, (void)				);

/* device.c */
_PROTOTYPE( int block_dev_io, (int op, Dev_t dev, int proc, void *buf,
			u64_t pos, int bytes, int flags)		);
_PROTOTYPE( int dev_open, (endpoint_t driver_e, Dev_t dev, int proc,
							int flags)	);
_PROTOTYPE( void dev_close, (endpoint_t driver_e, Dev_t dev)		);


/* inode.c */
_PROTOTYPE( struct inode *find_inode, (Dev_t dev, int numb)		);

_PROTOTYPE( struct inode *alloc_inode, (dev_t dev, mode_t bits)		);
_PROTOTYPE( void dup_inode, (struct inode *ip)				);
_PROTOTYPE( void free_inode, (Dev_t dev, Ino_t numb)			);
_PROTOTYPE( struct inode *get_inode, (Dev_t dev, int numb)		);
_PROTOTYPE( void put_inode, (struct inode *rip)				);
_PROTOTYPE( void update_times, (struct inode *rip)			);
_PROTOTYPE( void rw_inode, (struct inode *rip, int rw_flag)		);
_PROTOTYPE( void wipe_inode, (struct inode *rip)			);

/* link.c */
int fs_link_o(void);
int fs_link_s(void);
int fs_rdlink_o(void);
int fs_rdlink_so(void);
int fs_rdlink_s(void);
int fs_rename_o(void);
int fs_rename_s(void);
int fs_unlink_o(void);
int fs_unlink_s(void);
_PROTOTYPE( int truncate_inode, (struct inode *rip, off_t len)		);
_PROTOTYPE( int freesp_inode, (struct inode *rip, off_t st, off_t end)	);

/* main.c */
_PROTOTYPE( void reply, (int who, message *m_out)			);

/* mount.c */
int fs_mountpoint_o(void);
int fs_mountpoint_s(void);
int fs_readsuper_o(void);
int fs_readsuper_s(void);
_PROTOTYPE( int unmount, (Dev_t dev)					);

/* open.c */
int fs_create_o(void);
int fs_create_s(void);
int fs_mkdir_o(void);
int fs_mkdir_s(void);
int fs_mknod_o(void);
int fs_mknod_s(void);
int fs_slink_o(void);
int fs_slink_s(void);
int fs_newnode(void);

/* path.c */
int lookup_o(void);
int fs_lookup_s(void);
_PROTOTYPE( struct inode *advance_o,(struct inode **dirp,
						char string[NAME_MAX])	);
_PROTOTYPE( struct inode *advance_nocheck,(struct inode **dirp,
						char string[NAME_MAX])	);
_PROTOTYPE( int search_dir, (struct inode *ldir_ptr,
			char string [NAME_MAX], ino_t *numb, int flag)	);
_PROTOTYPE( int search_dir_nocheck, (struct inode *ldir_ptr,
			char string [NAME_MAX], ino_t *numb, int flag)	);
_PROTOTYPE( struct inode *eat_path_o, (char *path)			);
_PROTOTYPE( struct inode *last_dir_o, (char *path, char string [NAME_MAX]));
_PROTOTYPE( struct inode *parse_path_o, (char *path,
				char string[NAME_MAX], int action)	);


/* protect.c */
int fs_access_o(void);
_PROTOTYPE( int forbidden, (struct inode *rip, mode_t access_desired)	);
_PROTOTYPE( int read_only, (struct inode *ip)				);

/* read.c */
int fs_breadwrite_o(void);
int fs_breadwrite_s(void);
int fs_readwrite_o(void);
int fs_readwrite_s(void);
_PROTOTYPE( struct buf *rahead, (struct inode *rip, block_t baseblock,
			u64_t position, unsigned bytes_ahead)		);
_PROTOTYPE( void read_ahead, (void)					);
_PROTOTYPE( block_t read_map, (struct inode *rip, off_t pos)		);
_PROTOTYPE( int read_write, (int rw_flag)				);
_PROTOTYPE( zone_t rd_indir, (struct buf *bp, int index)		);

/* super.c */
_PROTOTYPE( bit_t alloc_bit, (struct super_block *sp, int map, bit_t origin));
_PROTOTYPE( void free_bit, (struct super_block *sp, int map,
						bit_t bit_returned)	);
_PROTOTYPE( struct super_block *get_super, (Dev_t dev)			);
_PROTOTYPE( int mounted, (struct inode *rip)				);
_PROTOTYPE( int read_super, (struct super_block *sp)			);
_PROTOTYPE( int get_block_size, (dev_t dev)				);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( unsigned conv2, (int norm, int w)				);
_PROTOTYPE( long conv4, (int norm, long x)				);
_PROTOTYPE( int no_sys, (void)						);
_PROTOTYPE( void mfs_nul_f, (char *file, int line, char *str, int len, int maxlen));
_PROTOTYPE( int mfs_min_f, (char *file, int line, int len1, int len2)	);
_PROTOTYPE( void sanitycheck, (char *file, int line)	);

#define SANITYCHECK sanitycheck(__FILE__, __LINE__)

/* write.c */
_PROTOTYPE( void clear_zone, (struct inode *rip, off_t pos, int flag)	);
_PROTOTYPE( int do_write, (void)					);
_PROTOTYPE( struct buf *new_block, (struct inode *rip, off_t position)	);
_PROTOTYPE( void zero_block, (struct buf *bp)				);
_PROTOTYPE( int write_map, (struct inode *, off_t, zone_t, int)		);


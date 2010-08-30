#ifndef __PFS_PROTO_H__
#define __PFS_PROTO_H__

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct inode;
struct sockaddr_un;
struct ancillary;

/* buffer.c */
_PROTOTYPE( struct buf *get_block, (dev_t dev, ino_t inum)		);
_PROTOTYPE( void put_block, (dev_t dev, ino_t inum)			);

/* cache.c */
_PROTOTYPE( void buf_pool, (void)					);

/* inode.c */
_PROTOTYPE( struct inode *alloc_inode, (dev_t dev, mode_t mode)		);
_PROTOTYPE( void dup_inode, (struct inode *ip)				);
_PROTOTYPE( struct inode *find_inode, (ino_t numb)			);
_PROTOTYPE( void free_inode, (struct inode *rip)			);
_PROTOTYPE( int fs_putnode, (message *fs_m_in, message *fs_m_out)	);
_PROTOTYPE( void init_inode_cache, (void)				);
_PROTOTYPE( struct inode *get_inode, (dev_t dev, ino_t numb)		);
_PROTOTYPE( void put_inode, (struct inode *rip)				);
_PROTOTYPE( void update_times, (struct inode *rip)			);
_PROTOTYPE( void wipe_inode, (struct inode *rip)			);

/* link.c */
_PROTOTYPE( int fs_ftrunc, (message *fs_m_in, message *fs_m_out)	);
_PROTOTYPE( int truncate_inode, (struct inode *rip, off_t newsize)	);


/* main.c */
_PROTOTYPE( void reply, (endpoint_t who, message *m_out)		);

/* misc.c */
_PROTOTYPE( int fs_sync, (message *fs_m_in, message *fs_m_out)		);

/* open.c */
_PROTOTYPE( int fs_newnode, (message *fs_m_in, message *fs_m_out)	);

/* read.c */
_PROTOTYPE( int fs_readwrite, (message *fs_m_in, message *fs_m_out)	);

/* utility.c */
_PROTOTYPE( time_t clock_time, (void)					);
_PROTOTYPE( int no_sys, (message *pfs_m_in, message *pfs_m_out)		);

/* stadir.c */
_PROTOTYPE( int fs_stat, (message *fs_m_in, message *fs_m_out)		);

/* super.c */
_PROTOTYPE( bit_t alloc_bit, (void)					);
_PROTOTYPE( void free_bit, (bit_t bit_returned)				);

/* dev_uds.c */
_PROTOTYPE( int uds_open, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_close, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_read, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_write, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_ioctl, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_select, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_status, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int uds_cancel, (message *dev_m_in, message *dev_m_out)	);

/* uds.c */
_PROTOTYPE( void uds_init, (void)					);
_PROTOTYPE( int do_accept, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_connect, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_listen, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_socket, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_bind, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getsockname, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getpeername, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_shutdown, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_socketpair, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getsockopt_sotype,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getsockopt_peercred,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getsockopt_sndbuf,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_setsockopt_sndbuf,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_getsockopt_rcvbuf,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_setsockopt_rcvbuf,
				(message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_sendto, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_recvfrom, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_sendmsg, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int do_recvmsg, (message *dev_m_in, message *dev_m_out)	);
_PROTOTYPE( int perform_connection,
				(message *dev_m_in, message *dev_m_out, 
				struct sockaddr_un *addr, int minorx, 
				int minory)				);
_PROTOTYPE( int clear_fds, (int minor, struct ancillary *data)		);
#endif

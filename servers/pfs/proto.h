#ifndef __PFS_PROTO_H__
#define __PFS_PROTO_H__

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct inode;
struct sockaddr_un;
struct ancillary;

/* buffer.c */
struct buf *get_block(dev_t dev, ino_t inum);
void put_block(dev_t dev, ino_t inum);

/* cache.c */
void buf_pool(void);

/* inode.c */
struct inode *alloc_inode(dev_t dev, mode_t mode);
void dup_inode(struct inode *ip);
struct inode *find_inode(ino_t numb);
void free_inode(struct inode *rip);
int fs_putnode(message *fs_m_in, message *fs_m_out);
void init_inode_cache(void);
struct inode *get_inode(dev_t dev, ino_t numb);
void put_inode(struct inode *rip);
void update_times(struct inode *rip);
void wipe_inode(struct inode *rip);

/* link.c */
int fs_ftrunc(message *fs_m_in, message *fs_m_out);
int truncate_inode(struct inode *rip, off_t newsize);


/* main.c */
void reply(endpoint_t who, message *m_out);

/* misc.c */
int fs_sync(message *fs_m_in, message *fs_m_out);
int fs_chmod(message *fs_m_in, message *fs_m_out);

/* mount.c */
int fs_unmount(message *fs_m_in, message *fs_m_out);

/* open.c */
int fs_newnode(message *fs_m_in, message *fs_m_out);

/* read.c */
int fs_readwrite(message *fs_m_in, message *fs_m_out);

/* utility.c */
time_t clock_time(void);
int no_sys(message *pfs_m_in, message *pfs_m_out);

/* stadir.c */
int fs_stat(message *fs_m_in, message *fs_m_out);

/* super.c */
bit_t alloc_bit(void);
void free_bit(bit_t bit_returned);

/* dev_uds.c */
int uds_open(message *dev_m_in, message *dev_m_out);
int uds_close(message *dev_m_in, message *dev_m_out);
int uds_read(message *dev_m_in, message *dev_m_out);
int uds_write(message *dev_m_in, message *dev_m_out);
int uds_ioctl(message *dev_m_in, message *dev_m_out);
int uds_select(message *dev_m_in, message *dev_m_out);
int uds_unsuspend(endpoint_t m_source, int minor);
int uds_cancel(message *dev_m_in, message *dev_m_out);

/* uds.c */
void uds_init(void);
int do_accept(message *dev_m_in, message *dev_m_out);
int do_connect(message *dev_m_in, message *dev_m_out);
int do_listen(message *dev_m_in, message *dev_m_out);
int do_socket(message *dev_m_in, message *dev_m_out);
int do_bind(message *dev_m_in, message *dev_m_out);
int do_getsockname(message *dev_m_in, message *dev_m_out);
int do_getpeername(message *dev_m_in, message *dev_m_out);
int do_shutdown(message *dev_m_in, message *dev_m_out);
int do_socketpair(message *dev_m_in, message *dev_m_out);
int do_getsockopt_sotype(message *dev_m_in, message *dev_m_out);
int do_getsockopt_peercred(message *dev_m_in, message *dev_m_out);
int do_getsockopt_sndbuf(message *dev_m_in, message *dev_m_out);
int do_setsockopt_sndbuf(message *dev_m_in, message *dev_m_out);
int do_getsockopt_rcvbuf(message *dev_m_in, message *dev_m_out);
int do_setsockopt_rcvbuf(message *dev_m_in, message *dev_m_out);
int do_sendto(message *dev_m_in, message *dev_m_out);
int do_recvfrom(message *dev_m_in, message *dev_m_out);
int do_sendmsg(message *dev_m_in, message *dev_m_out);
int do_recvmsg(message *dev_m_in, message *dev_m_out);
int perform_connection(message *dev_m_in, message *dev_m_out, struct
	sockaddr_un *addr, int minorx, int minory);
int clear_fds(int minor, struct ancillary *data);
#endif

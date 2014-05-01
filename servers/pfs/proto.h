#ifndef __PFS_PROTO_H__
#define __PFS_PROTO_H__

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct inode;

/* buffer.c */
struct buf *get_block(dev_t dev, ino_t inum);
void put_block(dev_t dev, ino_t inum);

/* cache.c */
void buf_pool(void);

/* inode.c */
struct inode *alloc_inode(dev_t dev, mode_t mode, uid_t uid, gid_t gid);
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

#endif

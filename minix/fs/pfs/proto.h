#ifndef __PFS_PROTO_H__
#define __PFS_PROTO_H__

/* Function prototypes. */

/* Structs used in prototypes must be declared as such first. */
struct buf;
struct inode;

/* buffer.c */
struct buf *get_block(dev_t dev, ino_t inum);
void put_block(dev_t dev, ino_t inum);
void buf_pool(void);

/* inode.c */
struct inode *alloc_inode(dev_t dev, mode_t mode, uid_t uid, gid_t gid);
void dup_inode(struct inode *ip);
struct inode *find_inode(ino_t numb);
void free_inode(struct inode *rip);
int fs_putnode(ino_t ino_nr, unsigned int count);
void init_inode_cache(void);
struct inode *get_inode(dev_t dev, ino_t numb);
void put_inode(struct inode *rip);
void update_times(struct inode *rip);
void wipe_inode(struct inode *rip);

/* link.c */
int fs_trunc(ino_t ino_nr, off_t start, off_t end);
int truncate_inode(struct inode *rip, off_t newsize);

/* misc.c */
int fs_chmod(ino_t ino_nr, mode_t *mode);

/* mount.c */
int fs_mount(dev_t dev, unsigned int flags, struct fsdriver_node *node,
	unsigned int *res_flags);
void fs_unmount(void);

/* open.c */
int fs_newnode(mode_t mode, uid_t uid, gid_t gid, dev_t dev,
	struct fsdriver_node *node);

/* read.c */
ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);
ssize_t fs_write(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int call);

/* stadir.c */
int fs_stat(ino_t ino_nr, struct stat *statbuf);

/* super.c */
bit_t alloc_bit(void);
void free_bit(bit_t bit_returned);

#endif

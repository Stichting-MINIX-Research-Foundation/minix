#ifndef _MKFS_MFS_TYPE_H__
#define _MKFS_MFS_TYPE_H__

/* Declaration of the V1 inode as it is on the disk (not in core). */
struct inode {	/* V1 disk inode */
  uint16_t i_mode;		/* file type, protection, etc. */
  uint16_t i_uid;		/* user id of the file's owner. */
  uint32_t i_size;		/* current file size in bytes */
  uint32_t i_mtime;		/* when was file data last changed */
  uint8_t i_gid;		/* group number */
  uint8_t i_nlinks;		/* how many links to this file. */
  uint16_t i_zone[NR_TZONES];	/* zone nums for direct, ind, and dbl ind */
};

/* Note: in V1 there was only one kind of timestamp kept in inodes! */
#define	MFS_INODE_ONLY_MTIME

#endif

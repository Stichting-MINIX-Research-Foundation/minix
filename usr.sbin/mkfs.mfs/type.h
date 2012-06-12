#ifndef __MFS_TYPE_H__
#define __MFS_TYPE_H__

/* Declaration of the V1 inode as it is on the disk (not in core). */
typedef struct {		/* V1.x disk inode */
  uint16_t d1_mode;		/* file type, protection, etc. */
  int16_t d1_uid;		/* user id of the file's owner */
  int32_t d1_size;		/* current file size in bytes */
  int32_t d1_mtime;		/* when was file data last changed */
  uint8_t d1_gid;		/* group number */
  uint8_t d1_nlinks;		/* how many links to this file */
  uint16_t d1_zone[V1_NR_TZONES];/* block nums for direct, ind, and dbl ind */
} d1_inode;

/* Declaration of the V2 inode as it is on the disk (not in core). */
typedef struct {		/* V2.x disk inode */
  uint16_t d2_mode;		/* file type, protection, etc. */
  uint16_t d2_nlinks;		/* how many links to this file. HACK! */
  int16_t d2_uid;		/* user id of the file's owner. */
  uint16_t d2_gid;		/* group number HACK! */
  int32_t d2_size;		/* current file size in bytes */
  int32_t d2_atime;		/* when was file data last accessed */
  int32_t d2_mtime;		/* when was file data last changed */
  int32_t d2_ctime;		/* when was inode data last changed */
  uint32_t d2_zone[V2_NR_TZONES];/* block nums for direct, ind, and dbl ind */
} d2_inode;

struct buf {
  /* Data portion of the buffer. */
  union fsdata_u *bp;

  /* Header portion of the buffer. */
  struct buf *b_next;		/* used to link all free bufs in a chain */
  struct buf *b_prev;		/* used to link all free bufs the other way */
  struct buf *b_hash;		/* used to link bufs on hash chains */
  uint32_t b_blocknr;		/* block number of its (minor) device */
  dev_t b_dev;			/* major | minor device where block resides */
  char b_dirt;			/* BP_CLEAN or BP_DIRTY */
  char b_count;			/* number of users of this buffer */
  unsigned int b_bytes;		/* Number of bytes allocated in bp */
};

#endif


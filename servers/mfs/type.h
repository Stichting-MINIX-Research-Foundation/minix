/* Declaration of the V1 inode as it is on the disk (not in core). */
typedef struct {		/* V1.x disk inode */
  mode_t d1_mode;		/* file type, protection, etc. */
  uid_t d1_uid;			/* user id of the file's owner */
  off_t d1_size;		/* current file size in bytes */
  time_t d1_mtime;		/* when was file data last changed */
  u8_t d1_gid;			/* group number */
  u8_t d1_nlinks;		/* how many links to this file */
  u16_t d1_zone[V1_NR_TZONES];	/* block nums for direct, ind, and dbl ind */
} d1_inode;

/* Declaration of the V2 inode as it is on the disk (not in core). */
typedef struct {		/* V2.x disk inode */
  mode_t d2_mode;		/* file type, protection, etc. */
  u16_t d2_nlinks;		/* how many links to this file. HACK! */
  uid_t d2_uid;			/* user id of the file's owner. */
  u16_t d2_gid;			/* group number HACK! */
  off_t d2_size;		/* current file size in bytes */
  time_t d2_atime;		/* when was file data last accessed */
  time_t d2_mtime;		/* when was file data last changed */
  time_t d2_ctime;		/* when was inode data last changed */
  zone_t d2_zone[V2_NR_TZONES];	/* block nums for direct, ind, and dbl ind */
} d2_inode;

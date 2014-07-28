/* Inode table.  This table holds inodes that are currently in use.  In some
 * cases they have been opened by an open() or creat() system call, in other
 * cases the file system itself needs the inode for one reason or another,
 * such as to search a directory for a path name.
 * The first part of the struct holds fields that are present on the
 * disk; the second part holds fields not present on the disk.
 * The disk inode part is also declared in "type.h" as 'd_inode'
 *
 */

#ifndef EXT2_INODE_H
#define EXT2_INODE_H

#include <sys/queue.h>

/* Disk part of inode structure was taken from
 * linux/include/linux/ext2_fs.h.
 */
EXTERN struct inode {
    u16_t  i_mode;         /* File mode */
    u16_t  i_uid;          /* Low 16 bits of Owner Uid */
    u32_t  i_size;         /* Size in bytes */
    u32_t  i_atime;        /* Access time */
    u32_t  i_ctime;        /* Creation time */
    u32_t  i_mtime;        /* Modification time */
    u32_t  i_dtime;        /* Deletion Time */
    u16_t  i_gid;          /* Low 16 bits of Group Id */
    u16_t  i_links_count;  /* Links count */
    u32_t  i_blocks;       /* 512-byte blocks count */
    u32_t  i_flags;        /* File flags */
    union {
        struct {
                u32_t  l_i_reserved1;
        } linux1;
        struct {
                u32_t  h_i_translator;
        } hurd1;
        struct {
                u32_t  m_i_reserved1;
        } masix1;
    } osd1;                         /* OS dependent 1 */
    u32_t  i_block[EXT2_N_BLOCKS];  /* Pointers to blocks */
    u32_t  i_generation;            /* File version (for NFS) */
    u32_t  i_file_acl;              /* File ACL */
    u32_t  i_dir_acl;               /* Directory ACL */
    u32_t  i_faddr;                 /* Fragment address */
    union {
        struct {
            u8_t    l_i_frag;       /* Fragment number */
            u8_t    l_i_fsize;      /* Fragment size */
            u16_t   i_pad1;
            u16_t  l_i_uid_high;   /* these 2 fields    */
            u16_t  l_i_gid_high;   /* were reserved2[0] */
            u32_t   l_i_reserved2;
        } linux2;
        struct {
            u8_t    h_i_frag;       /* Fragment number */
            u8_t    h_i_fsize;      /* Fragment size */
            u16_t  h_i_mode_high;
            u16_t  h_i_uid_high;
            u16_t  h_i_gid_high;
            u32_t  h_i_author;
        } hurd2;
        struct {
            u8_t    m_i_frag;       /* Fragment number */
            u8_t    m_i_fsize;      /* Fragment size */
            u16_t   m_pad1;
            u32_t   m_i_reserved2[2];
        } masix2;
    } osd2;                         /* OS dependent 2 */

    /* The following items are not present on the disk. */
    dev_t i_dev;                /* which device is the inode on */
    ino_t i_num;               /* inode number on its (minor) device */
    int i_count;                /* # times inode used; 0 means slot is free */
    struct super_block *i_sp;   /* pointer to super block for inode's device */
    char i_dirt;                /* CLEAN or DIRTY */
    block_t i_bsearch;          /* where to start search for new blocks,
                                 * also this is last allocated block.
				 */
    off_t i_last_pos_bl_alloc;  /* last write position for which we allocated
                                 * a new block (should be block i_bsearch).
				 * used to check for sequential operation.
				 */
    off_t i_last_dpos;          /* where to start dentry search */
    int i_last_dentry_size;	/* size of last found dentry */

    char i_mountpoint;          /* true if mounted on */

    char i_seek;                /* set on LSEEK, cleared on READ/WRITE */
    char i_update;              /* the ATIME, CTIME, and MTIME bits are here */

    block_t i_prealloc_blocks[EXT2_PREALLOC_BLOCKS];	/* preallocated blocks */
    int i_prealloc_count;	/* number of preallocated blocks */
    int i_prealloc_index;	/* index into i_prealloc_blocks */
    int i_preallocation;	/* use preallocation for this inode, normally
				 * it's reset only when non-sequential write
				 * happens.
				 */

    LIST_ENTRY(inode) i_hash;     /* hash list */
    TAILQ_ENTRY(inode) i_unused;  /* free and unused list */

} inode[NR_INODES];


/* list of unused/free inodes */
EXTERN TAILQ_HEAD(unused_inodes_t, inode)  unused_inodes;

/* inode hashtable */
EXTERN LIST_HEAD(inodelist, inode)         hash_inodes[INODE_HASH_SIZE];

EXTERN unsigned int inode_cache_hit;
EXTERN unsigned int inode_cache_miss;

/* Field values.  Note that CLEAN and DIRTY are defined in "const.h" */
#define NO_SEEK            0    /* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK              1    /* i_seek = ISEEK if last op was SEEK */

#endif /* EXT2_INODE_H */

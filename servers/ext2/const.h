#ifndef EXT2_CONST_H
#define EXT2_CONST_H

/* Tables sizes */

#define NR_INODES        512    /* # slots in "in core" inode table;
				 * should be more or less the same as
				 * NR_VNODES in vfs
				 */
#define GETDENTS_BUFSIZ  257

#define INODE_HASH_LOG2   7     /* 2 based logarithm of the inode hash size */
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)


/* The type of sizeof may be (unsigned) long.  Use the following macro for
 * taking the sizes of small objects so that there are no surprises like
 * (small) long constants being passed to routines expecting an int.
 */
#define usizeof(t) ((unsigned) sizeof(t))

#define SUPER_MAGIC   0xEF53  /* magic number contained in super-block */

#define EXT2_NAME_MAX	255

/* Miscellaneous constants */
#define SU_UID          ((uid_t) 0)     /* super_user's uid_t */
#define NORMAL          0               /* forces get_block to do disk read */
#define NO_READ         1       /* prevents get_block from doing disk read */
#define PREFETCH        2       /* tells get_block not to read or mark dev */

#define NO_BIT   ((bit_t) 0)    /* returned by alloc_bit() to signal failure */

#define LOOK_UP            0 /* tells search_dir to lookup string */
#define ENTER              1 /* tells search_dir to make dir entry */
#define DELETE             2 /* tells search_dir to delete entry */
#define IS_EMPTY           3 /* tells search_dir to ret. OK or ENOTEMPTY */

/* write_map() args */
#define WMAP_FREE           (1 << 0)

#define IGN_PERM            0
#define CHK_PERM            1

#define IN_CLEAN              0    /* inode disk and memory copies identical */
#define IN_DIRTY              1    /* inode disk and memory copies differ */
#define ATIME            002    /* set if atime field needs updating */
#define CTIME            004    /* set if ctime field needs updating */
#define MTIME            010    /* set if mtime field needs updating */

#define BYTE_SWAP          0    /* tells conv2/conv4 to swap bytes */

#define END_OF_FILE   (-104)    /* eof detected */

#define SUPER_BLOCK_BYTES       (1024)         /* bytes offset */

#define ROOT_INODE      ((ino_t) 2)   /* inode number for root directory */
#define BOOT_BLOCK      ((block_t) 0) /* block number of boot block */
#define START_BLOCK     ((block_t) 2) /* first block of FS (not counting SB) */
#define BLOCK_ADDRESS_BYTES	4     /* bytes per address */

#define SUPER_SIZE      usizeof (struct super_block) /* sb size in RAM */
#define SUPER_SIZE_D    (1024)  /* max size of superblock stored on disk */

/* Directories related macroses */

#define DIR_ENTRY_ALIGN         4

/* ino + rec_len + name_len + file_type, doesn't include name and padding */
#define MIN_DIR_ENTRY_SIZE	8

#define DIR_ENTRY_CONTENTS_SIZE(d) (MIN_DIR_ENTRY_SIZE + (d)->d_name_len)

/* size with padding */
#define DIR_ENTRY_ACTUAL_SIZE(d) (DIR_ENTRY_CONTENTS_SIZE(d) + \
        ((DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) == 0 ? 0 : \
			DIR_ENTRY_ALIGN - (DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) ))

/* How many bytes can be taken from the end of dentry */
#define DIR_ENTRY_SHRINK(d)    (conv2(le_CPU, (d)->d_rec_len) \
					- DIR_ENTRY_ACTUAL_SIZE(d))

/* Dentry can have padding, which can be used to enlarge namelen */
#define DIR_ENTRY_MAX_NAME_LEN(d)	(conv2(le_CPU, (d)->d_rec_len) \
						- MIN_DIR_ENTRY_SIZE)

/* Constants relative to the data blocks */
/* When change EXT2_NDIR_BLOCKS, modify ext2_max_size()!!!*/
#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK         (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS           (EXT2_TIND_BLOCK + 1)

#define FS_BITMAP_CHUNKS(b) ((b)/usizeof (bitchunk_t))/* # map chunks/blk   */
#define FS_BITCHUNK_BITS        (usizeof(bitchunk_t) * CHAR_BIT)
#define FS_BITS_PER_BLOCK(b)    (FS_BITMAP_CHUNKS(b) * FS_BITCHUNK_BITS)

/* Inodes */

/* Next 4 following macroses were taken from linux' ext2_fs.h */
#define EXT2_GOOD_OLD_INODE_SIZE	128
#define EXT2_GOOD_OLD_FIRST_INO		11

#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				EXT2_GOOD_OLD_INODE_SIZE : \
							(s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level == EXT2_GOOD_OLD_REV) ? \
				EXT2_GOOD_OLD_FIRST_INO : \
							(s)->s_first_ino)

/* Maximum size of a fast symlink including trailing '\0' */
#define MAX_FAST_SYMLINK_LENGTH \
	( sizeof(((d_inode *)0)->i_block[0]) * EXT2_N_BLOCKS )

#define NUL(str,l,m) mfs_nul_f(__FILE__,__LINE__,(str), (l), (m))

/* FS states */
#define EXT2_VALID_FS                   0x0001  /* Cleanly unmounted */
#define EXT2_ERROR_FS                   0x0002  /* Errors detected */

#define EXT2_GOOD_OLD_REV       0       /* The good old (original) format */
#define EXT2_DYNAMIC_REV        1       /* V2 format w/ dynamic inode sizes */

/* ext2 features, names shorted (cut EXT2_ prefix) */
#define COMPAT_DIR_PREALLOC        0x0001
#define COMPAT_IMAGIC_INODES       0x0002
#define COMPAT_HAS_JOURNAL         0x0004
#define COMPAT_EXT_ATTR            0x0008
#define COMPAT_RESIZE_INO          0x0010
#define COMPAT_DIR_INDEX           0x0020
#define COMPAT_ANY                 0xffffffff

#define RO_COMPAT_SPARSE_SUPER     0x0001
#define RO_COMPAT_LARGE_FILE       0x0002
#define RO_COMPAT_BTREE_DIR        0x0004
#define RO_COMPAT_ANY              0xffffffff

#define INCOMPAT_COMPRESSION       0x0001
#define INCOMPAT_FILETYPE          0x0002
#define INCOMPAT_RECOVER           0x0004
#define INCOMPAT_JOURNAL_DEV       0x0008
#define INCOMPAT_META_BG           0x0010
#define INCOMPAT_ANY               0xffffffff

/* What do we support? */
#define SUPPORTED_INCOMPAT_FEATURES	(INCOMPAT_FILETYPE)
#define SUPPORTED_RO_COMPAT_FEATURES	(RO_COMPAT_SPARSE_SUPER | \
					 RO_COMPAT_LARGE_FILE)

/* Ext2 directory file types. Only the low 3 bits are used.
 * The other bits are reserved for now.
 */
#define EXT2_FT_UNKNOWN         0
#define EXT2_FT_REG_FILE        1
#define EXT2_FT_DIR             2
#define EXT2_FT_CHRDEV          3
#define EXT2_FT_BLKDEV          4
#define EXT2_FT_FIFO            5
#define EXT2_FT_SOCK            6
#define EXT2_FT_SYMLINK         7

#define EXT2_FT_MAX             8

#define HAS_COMPAT_FEATURE(sp, mask)                        \
        ( (sp)->s_feature_compat & (mask) )
#define HAS_RO_COMPAT_FEATURE(sp, mask)                     \
	( (sp)->s_feature_ro_compat & (mask) )
#define HAS_INCOMPAT_FEATURE(sp, mask)                      \
	( (sp)->s_feature_incompat & (mask) )


/* hash-indexed directory */
#define EXT2_INDEX_FL			0x00001000
/* Top of directory hierarchies*/
#define EXT2_TOPDIR_FL                  0x00020000

#define EXT2_PREALLOC_BLOCKS		8


#endif /* EXT2_CONST_H */

#ifndef _MKFS_MFS_SUPER_H__
#define _MKFS_MFS_SUPER_H__

/* Super block table.  The entry holds information about the sizes of the bit
 * maps and inodes.  The s_ninodes field gives the number of inodes available
 * for files and directories, including the root directory.  Inode 0 is 
 * on the disk, but not used.  Thus s_ninodes = 4 means that 5 bits will be
 * used in the bit map, bit 0, which is always 1 and not used, and bits 1-4
 * for files and directories.  The disk layout is:
 *
 *    Item        # blocks
 *    boot block      1
 *    super block     1    (offset 1kB)
 *    inode map     s_imap_blocks
 *    zone map      s_zmap_blocks
 *    inodes        (s_ninodes + 'inodes per block' - 1)/'inodes per block'
 *    unused        whatever is needed to fill out the current zone
 *    data zones    (s_zones - s_firstdatazone) << s_log_zone_size
 */

struct super_block {
  uint32_t s_ninodes;		/* # usable inodes on the minor device */
  uint16_t  s_nzones;		/* total device size, including bit maps etc */
  int16_t s_imap_blocks;	/* # of blocks used by inode bit map */
  int16_t s_zmap_blocks;	/* # of blocks used by zone bit map */
  uint16_t s_firstdatazone_old;	/* number of first data zone (small) */
  uint16_t s_log_zone_size;	/* log2 of blocks/zone */
  uint16_t s_flags;		/* FS state flags */
  int32_t s_max_size;		/* maximum file size on this device */
  uint32_t s_zones;		/* number of zones (replaces s_nzones in V2) */
  int16_t s_magic;		/* magic number to recognize super-blocks */

  /* The following items are valid on disk only for V3 and above */

  int16_t s_pad2;		/* try to avoid compiler-dependent padding */
  /* The block size in bytes. Minimum MIN_BLOCK SIZE. SECTOR_SIZE multiple.*/
  uint16_t s_block_size;	/* block size in bytes. */
  int8_t s_disk_version;	/* filesystem format sub-version */

  /* The following items are only used when the super_block is in memory.
   * If this ever changes, i.e. more fields after s_disk_version has to go to
   * disk, update LAST_ONDISK_FIELD in servers/mfs/super.c as that controls
   * which part of the struct is copied to and from disk.
   */
/* XXX padding inserted here... */

  unsigned s_inodes_per_block;	/* precalculated from magic number */
  uint32_t s_firstdatazone;	/* number of first data zone (big) */
} superblock;

/* s_flags contents; undefined flags are guaranteed to be zero on disk
 * (not counting future versions of mfs setting them!)
 */
#define MFSFLAG_CLEAN	(1L << 0) /* 0: dirty; 1: FS was unmounted cleanly */

/* Future compatability (or at least, graceful failure):
 * if any of these bits are on, and the MFS or fsck
 * implementation doesn't understand them, do not mount/fsck
 * the FS.
 */
#define MFSFLAG_MANDATORY_MASK 0xff00

/* The block size should be registered on disk, even
 * multiple. If V1 or V2 filesystem, this should be
 * initialised to STATIC_BLOCK_SIZE.
 */
#define	MFS_SUPER_BLOCK_SIZE	s_block_size

/* To keep the super block on disk clean, the MFS server only read/write up to
 * and including this field:
 */
#define LAST_ONDISK_FIELD	s_disk_version

#endif

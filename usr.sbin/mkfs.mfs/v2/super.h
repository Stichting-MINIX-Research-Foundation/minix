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
 *    super block     1
 *    inode map     s_imap_blocks
 *    zone map      s_zmap_blocks
 *    inodes        (s_ninodes + 'inodes per block' - 1)/'inodes per block'
 *    unused        whatever is needed to fill out the current zone
 *    data zones    (s_zones - s_firstdatazone) << s_log_zone_size
 */

struct super_block {
  uint16_t s_ninodes;		/* # usable inodes on the minor device */
  uint16_t  s_nzones;		/* total device size, including bit maps etc */
  int16_t s_imap_blocks;	/* # of blocks used by inode bit map */
  int16_t s_zmap_blocks;	/* # of blocks used by zone bit map */
  uint16_t s_firstdatazone;	/* number of first data zone (small) */
  int16_t s_log_zone_size;	/* log2 of blocks/zone */
  uint32_t s_max_size;		/* maximum file size on this device */
  int16_t s_magic;		/* magic number to recognize super-blocks */
  int16_t s_pad;		/* try to avoid compiler-dependent padding */
  uint32_t s_zones;		/* number of zones (replaces s_nzones in V2) */
} superblock;

#define s_firstdatazone_old	s_firstdatazone

#undef MFSFLAG_CLEAN
#undef MFSFLAG_MANDATORY_MASK

#endif

/* Super block table.  The root file system and every mounted file system
 * has an entry here.  The entry holds information about the sizes of the bit
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
 *
 * A super_block slot is free if s_dev == NO_DEV. 
 */

EXTERN struct super_block {
  ino_t s_ninodes;		/* # usable inodes on the minor device */
  zone1_t  s_nzones;		/* total device size, including bit maps etc */
  short s_imap_blocks;		/* # of blocks used by inode bit map */
  short s_zmap_blocks;		/* # of blocks used by zone bit map */
  zone1_t s_firstdatazone;	/* number of first data zone */
  short s_log_zone_size;	/* log2 of blocks/zone */
  short s_pad;			/* try to avoid compiler-dependent padding */
  off_t s_max_size;		/* maximum file size on this device */
  zone_t s_zones;		/* number of zones (replaces s_nzones in V2) */
  short s_magic;		/* magic number to recognize super-blocks */

  /* The following items are valid on disk only for V3 and above */

  /* The block size in bytes. Minimum MIN_BLOCK SIZE. SECTOR_SIZE
   * multiple. If V1 or V2 filesystem, this should be
   * initialised to STATIC_BLOCK_SIZE. Maximum MAX_BLOCK_SIZE.
   */
  short s_pad2;			/* try to avoid compiler-dependent padding */
  unsigned short s_block_size;	/* block size in bytes. */
  char s_disk_version;		/* filesystem format sub-version */

  /* The following items are only used when the super_block is in memory. */
  struct inode *s_isup;		/* inode for root dir of mounted file sys */
  struct inode *s_imount;	/* inode mounted on */
  unsigned s_inodes_per_block;	/* precalculated from magic number */
  dev_t s_dev;			/* whose super block is this? */
  int s_rd_only;		/* set to 1 iff file sys mounted read only */
  int s_native;			/* set to 1 iff not byte swapped file system */
  int s_version;		/* file system version, zero means bad magic */
  int s_ndzones;		/* # direct zones in an inode */
  int s_nindirs;		/* # indirect zones per indirect block */
  bit_t s_isearch;		/* inodes below this bit number are in use */
  bit_t s_zsearch;		/* all zones below this bit number are in use*/
} super_block[NR_SUPERS];

#define NIL_SUPER (struct super_block *) 0
#define IMAP		0	/* operating on the inode bit map */
#define ZMAP		1	/* operating on the zone bit map */

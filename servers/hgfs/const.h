
/* Number of inodes. */
/* The following number must not exceed 16. The i_num field is only a short. */
#define NUM_INODE_BITS	8

/* Number of entries in the name hashtable. */
#define NUM_HASH_SLOTS	1023

/* Arbitrary block size constant returned by fstatfs. du(1) uses this.
 * Also used by getdents. This is not the actual HGFS data transfer unit size.
 */
#define BLOCK_SIZE	4096

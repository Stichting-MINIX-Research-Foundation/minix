#ifndef _SFFS_CONST_H
#define _SFFS_CONST_H

/* Number of inodes. */
/* The following number must not exceed 16. The i_num field is only a short. */
#define NUM_INODE_BITS	8

/* Number of entries in the name hashtable. */
#define NUM_HASH_SLOTS	1023

/* Arbitrary block size constant returned by statvfs. Also used by getdents.
 * This is not the underlying data transfer unit size.
 */
#define BLOCK_SIZE	4096

#endif /* _SFFS_CONST_H */

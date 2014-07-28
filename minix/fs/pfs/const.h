#ifndef __PFS_CONST_H__
#define __PFS_CONST_H__

#define PFS_NR_INODES        512 	/* # slots in "in core" inode table */

#define INODE_HASH_LOG2   7     /* 2 based logarithm of the inode hash size */
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)

#define NO_BIT   ((bit_t) 0)	/* returned by alloc_bit() to signal failure */

#define ATIME            002	/* set if atime field needs updating */
#define CTIME            004	/* set if ctime field needs updating */
#define MTIME            010	/* set if mtime field needs updating */

#define FS_BITMAP_CHUNKS(b) ((b)/sizeof (bitchunk_t))/* # map chunks/blk   */
#define FS_BITCHUNK_BITS		(sizeof(bitchunk_t) * CHAR_BIT)
#define FS_BITS_PER_BLOCK(b)	(FS_BITMAP_CHUNKS(b) * FS_BITCHUNK_BITS)

#define FS_CALL_VEC_SIZE 31

#endif

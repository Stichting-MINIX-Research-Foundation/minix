#ifndef __PFS_CONST_H__
#define __PFS_CONST_H__

#define NR_INODES        256 	/* # slots in "in core" inode table */

/* Size of descriptor table for unix domain sockets. This should be
 * equal to the maximum number of minor devices (currently 256).
 */
#define NR_FDS           256

#define INODE_HASH_LOG2   7     /* 2 based logarithm of the inode hash size */
#define INODE_HASH_SIZE   ((unsigned long)1<<INODE_HASH_LOG2)
#define INODE_HASH_MASK   (((unsigned long)1<<INODE_HASH_LOG2)-1)


/* The type of sizeof may be (unsigned) long.  Use the following macro for
 * taking the sizes of small objects so that there are no surprises like
 * (small) long constants being passed to routines expecting an int.
 */
#define usizeof(t) ((unsigned) sizeof(t))

/* Miscellaneous constants */
#define INVAL_UID ((uid_t) -1)	/* Invalid user ID */
#define INVAL_GID ((gid_t) -1)	/* Invalid group ID */
#define NORMAL	           0	/* forces get_block to do disk read */
#define NO_READ            1	/* prevents get_block from doing disk read */
#define PREFETCH           2	/* tells get_block not to read or mark dev */

#define NO_BIT   ((bit_t) 0)	/* returned by alloc_bit() to signal failure */

#define ATIME            002	/* set if atime field needs updating */
#define CTIME            004	/* set if ctime field needs updating */
#define MTIME            010	/* set if mtime field needs updating */

#define FS_BITMAP_CHUNKS(b) ((b)/usizeof (bitchunk_t))/* # map chunks/blk   */
#define FS_BITCHUNK_BITS		(usizeof(bitchunk_t) * CHAR_BIT)
#define FS_BITS_PER_BLOCK(b)	(FS_BITMAP_CHUNKS(b) * FS_BITCHUNK_BITS)

#define FS_CALL_VEC_SIZE 31
#define DEV_CALL_VEC_SIZE 25

#endif

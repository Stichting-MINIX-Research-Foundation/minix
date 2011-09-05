/* The <sys/types.h> header contains important data type definitions.
 * It is considered good programming practice to use these definitions,
 * instead of the underlying base type.  By convention, all type names end
 * with _t.
 */

#ifndef _TYPES_H
#define _TYPES_H

#ifndef _MINIX_ANSI_H
#include <minix/ansi.h>
#endif

typedef unsigned char   u8_t;	   /* 8 bit type */
typedef unsigned short u16_t;	   /* 16 bit type */
typedef signed char     i8_t;      /* 8 bit signed type */
typedef short          i16_t;      /* 16 bit signed type */

#if __SIZEOF_LONG__ > 4
/* compiling with gcc on some (e.g. x86-64) platforms */
typedef unsigned int  u32_t;	   /* 32 bit type */
typedef int           i32_t;      /* 32 bit signed type */
#else
/* default for ACK or gcc on 32 bit platforms */
typedef unsigned long  u32_t;	   /* 32 bit type */
typedef long           i32_t;      /* 32 bit signed type */
#endif

#if !defined(__LONG_LONG_SUPPORTED)
typedef struct {
	u32_t lo;
	u32_t hi;
} u64_t;
#else
#if __SIZEOF_LONG__ > 4
typedef unsigned long u64_t;
#else
typedef unsigned long long u64_t;
#endif
#endif

/* some Minix specific types that do not conflict with posix */
typedef u32_t zone_t;	   /* zone number */
typedef u32_t block_t;	   /* block number */
typedef u32_t bit_t;	   /* bit number in a bit map */
typedef u16_t zone1_t;	   /* zone number for V1 file systems */
typedef u32_t bitchunk_t; /* collection of bits in a bitmap */

/* ANSI C makes writing down the promotion of unsigned types very messy.  When
 * sizeof(short) == sizeof(int), there is no promotion, so the type stays
 * unsigned.  When the compiler is not ANSI, there is usually no loss of
 * unsignedness, and there are usually no prototypes so the promoted type
 * doesn't matter.  The use of types like Ino_t is an attempt to use ints
 * (which are not promoted) while providing information to the reader.
 */

typedef unsigned long  Ino_t;

#if defined(_MINIX) || defined(__minix)

/* The type size_t holds all results of the sizeof operator.  At first glance,
 * it seems obvious that it should be an unsigned int, but this is not always
 * the case. For example, MINIX-ST (68000) has 32-bit pointers and 16-bit
 * integers. When one asks for the size of a 70K struct or array, the result
 * requires 17 bits to express, so size_t must be a long type.  The type
 * ssize_t is the signed version of size_t.
 */
#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef int ssize_t;
#endif

#ifndef _TIME_T
#define _TIME_T
typedef long time_t;		   /* time in sec since 1 Jan 1970 0000 GMT */
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef long clock_t;		   /* unit for system accounting */
#endif

#ifndef _SIGSET_T
#define _SIGSET_T
typedef unsigned long sigset_t;
#endif

#ifndef _KEY_T
#define _KEY_T
typedef long key_t;
#endif

/* Open Group Base Specifications Issue 6 (not complete) */
typedef long useconds_t;	/* Time in microseconds */

typedef u32_t          dev_t;	   /* holds (major|minor) device pair */
typedef u32_t          big_dev_t;

/* Types used in disk, inode, etc. data structures.
 * Some u64_t should be i64_t, but anyway with old libc we use .lo only.
 */
typedef u32_t          gid_t;	   /* group id */
typedef u32_t          big_gid_t;  /* group id */
typedef unsigned long  ino_t; 	   /* i-node number (V3 filesystem) */
typedef u64_t          big_ino_t;  /* i-node number (V3 filesystem) */
typedef unsigned short mode_t;	   /* file type and permissions bits */
typedef u32_t          big_mode_t; /* file type and permissions bits */
typedef short          nlink_t;	   /* number of links to a file */
typedef u32_t          big_nlink_t;/* number of links to a file */
typedef long	       off_t;	   /* offset within a file */
typedef u64_t          big_off_t;  /* offset within a file */
typedef int            pid_t;	   /* process id (must be signed) */
typedef u32_t          uid_t;	   /* user id */
typedef u32_t          big_uid_t;  /* user id */
typedef unsigned long  fsblkcnt_t; /* File system block count */
typedef unsigned long  fsfilcnt_t; /* File system file count */
typedef u32_t          blkcnt_t;   /* File system block count */
typedef unsigned long  blksize_t;  /* File system block size */

/* Signal handler type, e.g. SIG_IGN */
typedef void _PROTOTYPE( (*sighandler_t), (int) );
typedef sighandler_t sig_t;

/* Compatibility with other systems */
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef char		*caddr_t;

/* Devices. */
#define MAJOR              8    /* major device = (dev>>MAJOR) & 0377 */
#define MINOR              0    /* minor device = (dev>>MINOR) & 0377 */

#ifndef minor
#define minor(dev)      (((dev) >> MINOR) & 0xff)
#endif

#ifndef major
#define major(dev)      (((dev) >> MAJOR) & 0xff)
#endif

#ifndef makedev
#define makedev(major, minor)   \
                        ((dev_t) (((major) << MAJOR) | ((minor) << MINOR)))
#endif

#endif /* _MINIX || __minix */

#if defined(_MINIX)
typedef unsigned int    uint;           /* Sys V compatibility */
#endif

#endif /* _TYPES_H */

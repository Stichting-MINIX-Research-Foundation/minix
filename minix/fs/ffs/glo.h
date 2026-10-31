/* EXTERN should be extern except for the table file. */

#ifndef FFS_GLO_H
#define FFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

EXTERN dev_t fs_dev;		/* the device that is handled by this FS proc */

EXTERN struct opt opt;		/* global options */

/* This server is native-little-endian and UFS2-only; le_CPU is always TRUE
 * and is kept only so shared idioms read clearly. */
EXTERN int le_CPU;

extern struct fsdriver ffs_table;

#endif /* FFS_GLO_H */

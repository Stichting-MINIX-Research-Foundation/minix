/* EXTERN should be extern except for the table file */

#ifndef EXT2_GLO_H
#define EXT2_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;        /* temporary storage for error number */

EXTERN int cch[NR_INODES];

EXTERN dev_t fs_dev;              /* The device that is handled by this FS proc
                                   */

/* Little hack for syncing group descriptors. */
EXTERN int group_descriptors_dirty;

EXTERN struct opt opt;		/* global options */

/* On ext2 metadata is stored in little endian format, so we shoud take
 * care about byte swapping, when have BE CPU. */
EXTERN int le_CPU;	/* little/big endian, if TRUE do not swap bytes */

extern struct fsdriver ext2_table;

#endif /* EXT2_GLO_H */

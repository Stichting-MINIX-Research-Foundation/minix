#ifndef __MFS_GLO_H__
#define __MFS_GLO_H__

/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

/* The following variables are used for returning results to the caller. */
EXTERN int err_code;		/* temporary storage for error number */

EXTERN int cch[NR_INODES];

EXTERN dev_t fs_dev;    	/* The device that is handled by this FS proc.
				 */

EXTERN zone_t used_zones;

extern struct fsdriver mfs_table;

#endif

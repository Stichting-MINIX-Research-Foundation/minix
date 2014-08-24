/* EXTERN should be extern except for the table file */
#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN dev_t fs_dev;            /* the device that is handled by this FS proc */

EXTERN struct opt opt;          /* global mount options */

extern struct fsdriver isofs_table;	/* function call table */

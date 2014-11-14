#ifndef _SFFS_GLO_H
#define _SFFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN char *sffs_name;				/* file server name */
EXTERN const struct sffs_table *sffs_table;	/* call table */
EXTERN struct sffs_params *sffs_params;		/* parameters */

EXTERN int read_only;				/* mounted read-only? */

extern struct fsdriver sffs_dtable;		/* driver table */

#endif /* _SFFS_GLO_H */

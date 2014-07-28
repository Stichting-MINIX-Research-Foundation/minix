#ifndef _SFFS_GLO_H
#define _SFFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN char *sffs_name;				/* file server name */
EXTERN const struct sffs_table *sffs_table;	/* call table */
EXTERN struct sffs_params *sffs_params;		/* parameters */

EXTERN message m_in;				/* request message */
EXTERN message m_out;				/* reply message */
EXTERN struct state state;			/* global state */

extern int(*call_vec[]) (void);

#endif /* _SFFS_GLO_H */

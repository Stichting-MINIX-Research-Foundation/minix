#ifndef _VTREEFS_GLO_H
#define _VTREEFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN struct fs_hooks *vtreefs_hooks;

EXTERN dev_t fs_dev;

extern struct fsdriver vtreefs_table;

#endif /* _VTREEFS_GLO_H */

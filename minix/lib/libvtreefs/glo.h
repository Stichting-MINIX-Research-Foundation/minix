#ifndef _VTREEFS_GLO_H
#define _VTREEFS_GLO_H

#ifdef _TABLE
#undef EXTERN
#define EXTERN
#endif

EXTERN struct fs_hooks *vtreefs_hooks;

EXTERN message fs_m_in;
EXTERN message fs_m_out;

EXTERN dev_t fs_dev;

EXTERN int fs_mounted;

extern int(*fs_call_vec[]) (void);

#endif /* _VTREEFS_GLO_H */

/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#ifndef _MINIX_HGFS_H
#define _MINIX_HGFS_H

#include <minix/sffs.h>

int hgfs_init(const struct sffs_table **tablep);
void hgfs_cleanup(void);

#endif /* _MINIX_HGFS_H */

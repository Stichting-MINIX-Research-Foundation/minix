/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#ifndef _MINIX_VBOXFS_H
#define _MINIX_VBOXFS_H

#include <minix/sffs.h>

int vboxfs_init(char *share, const struct sffs_table **tablep,
	int *case_insens, int *read_only);
void vboxfs_cleanup(void);

#endif /* _MINIX_VBOXFS_H */

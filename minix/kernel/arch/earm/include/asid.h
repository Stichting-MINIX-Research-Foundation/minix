#ifndef __MINIX_ASID_H
#define __MINIX_ASID_H

#include "kernel/proc.h"

void asid_init(void);
void asid_alloc(struct proc *p);
void asid_free(struct proc *p);

#endif /* __MINIX_ASID_H */

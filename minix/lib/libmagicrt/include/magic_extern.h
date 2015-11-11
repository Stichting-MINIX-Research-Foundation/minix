#ifndef _MAGIC_EXTERN_H
#define _MAGIC_EXTERN_H

/*
 * TODO: libsys/sef_llvm.c should include this file, all weak external
 * function declarations used in that file should be here, and it should
 * probably be moved into include/minix/.
 */

#include <stdlib.h>
#include <minix/sef.h>

void magic_data_init(void);
void _magic_ds_st_init(void);

void *magic_nested_mmap(void *start, size_t length, int prot, int flags,
	int fd, off_t offset);
int magic_nested_munmap(void *start, size_t length);

int _magic_state_transfer(sef_init_info_t *info);
void _magic_dump_eval_bool(char *expr);
void *_magic_real_alloc_contig(size_t len, int flags, uint32_t *phys);
int _magic_real_free_contig(void *addr, size_t len);
int _magic_real_brk(char *newbrk);
void* _magic_real_mmap(void *buf, size_t len, int prot, int flags, int fd,
	off_t offset);
int _magic_real_munmap(void *addr, size_t length);

#endif /* !_MAGIC_EXTERN_H */

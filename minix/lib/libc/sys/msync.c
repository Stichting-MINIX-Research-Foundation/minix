#include <sys/mman.h>

/* MINIX: mmap writes are not cached, msync is a no-op */
int __msync13(void *addr, size_t len, int flags) {
	return 0;
}


#include <stdio.h>
#include <minix/ipc.h>

struct minix_kerninfo *_minix_kerninfo = NULL;

void    __minix_init(void) __attribute__((__constructor__, __used__));

void __minix_init(void)
{
	if((_minix_kernel_info_struct(&_minix_kerninfo)) != 0
	  || _minix_kerninfo->kerninfo_magic != KERNINFO_MAGIC) {
		_minix_kerninfo = NULL;
	}
}


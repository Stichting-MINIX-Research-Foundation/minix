
#include <stdio.h>
#include <minix/ipc.h>

/* Minix kernel info, IPC functions pointers */
struct minix_kerninfo *_minix_kerninfo = NULL;

void    __minix_init(void) __attribute__((__constructor__, __used__));

struct minix_ipcvecs _minix_ipcvecs = {
	.sendrec	= _sendrec_orig,
	.send		= _send_orig,
	.notify		= _notify_orig,
	.senda		= _senda_orig,
	.sendnb		= _sendnb_orig,
	.receive	= _receive_orig,
	.do_kernel_call	= _do_kernel_call_orig,
};

void __minix_init(void)
{
	if((_minix_kernel_info_struct(&_minix_kerninfo)) != 0
	  || _minix_kerninfo->kerninfo_magic != KERNINFO_MAGIC) {
		_minix_kerninfo = NULL;
         } else if((_minix_kerninfo->ki_flags & MINIX_KIF_IPCVECS) &&
         	_minix_kerninfo->minix_ipcvecs) {
		_minix_ipcvecs = *_minix_kerninfo->minix_ipcvecs;
         }
}



#include <stdio.h>
#include <minix/ipc.h>

/* Minix kernel info, IPC functions pointers */
struct minix_kerninfo *_minix_kerninfo = NULL;

void    __minix_init(void) __attribute__((__constructor__, __used__));

struct minix_ipcvecs _minix_ipcvecs = {
	.sendrec	= _ipc_sendrec_intr,
	.send		= _ipc_send_intr,
	.notify		= _ipc_notify_intr,
	.senda		= _ipc_senda_intr,
	.sendnb		= _ipc_sendnb_intr,
	.receive	= _ipc_receive_intr,
	.do_kernel_call	= _do_kernel_call_intr,
};

void __minix_init(void)
{
	if((ipc_minix_kerninfo(&_minix_kerninfo) != 0) ||
		(_minix_kerninfo->kerninfo_magic != KERNINFO_MAGIC))
	{
		_minix_kerninfo = NULL;
	}
	else if((_minix_kerninfo->ki_flags & MINIX_KIF_IPCVECS) &&
		(_minix_kerninfo->minix_ipcvecs != NULL))
	{
		_minix_ipcvecs = *_minix_kerninfo->minix_ipcvecs;
	}
}

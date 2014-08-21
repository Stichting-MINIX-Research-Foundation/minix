#include "kernel/kernel.h"
#include "arch_proto.h"

struct minix_ipcvecs minix_ipcvecs_softint __section(".usermapped") = {
	.send		= usermapped_send_softint,
	.receive	= usermapped_receive_softint,
	.sendrec	= usermapped_sendrec_softint,
	.sendnb		= usermapped_sendnb_softint,
	.notify		= usermapped_notify_softint,
	.do_kernel_call	= usermapped_do_kernel_call_softint,
	.senda		= usermapped_senda_softint
};

struct minix_ipcvecs minix_ipcvecs_sysenter __section(".usermapped") = {
	.send		= usermapped_send_sysenter,
	.receive	= usermapped_receive_sysenter,
	.sendrec	= usermapped_sendrec_sysenter,
	.sendnb		= usermapped_sendnb_sysenter,
	.notify		= usermapped_notify_sysenter,
	.do_kernel_call = usermapped_do_kernel_call_sysenter,
	.senda		= usermapped_senda_sysenter
};

struct minix_ipcvecs minix_ipcvecs_syscall __section(".usermapped") = {
	.send		= usermapped_send_syscall,
	.receive	= usermapped_receive_syscall,
	.sendrec	= usermapped_sendrec_syscall,
	.sendnb		= usermapped_sendnb_syscall,
	.notify		= usermapped_notify_syscall,
	.do_kernel_call	= usermapped_do_kernel_call_syscall,
	.senda		= usermapped_senda_syscall
};


#include "kernel.h"
#include "arch_proto.h"

struct minix_ipcvecs minix_ipcvecs_softint = {
	.send_ptr = usermapped_send_softint,
	.receive_ptr = usermapped_receive_softint,
	.sendrec_ptr = usermapped_sendrec_softint,
	.sendnb_ptr = usermapped_sendnb_softint,
	.notify_ptr = usermapped_notify_softint,
	.do_kernel_call_ptr = usermapped_do_kernel_call_softint,
	.senda_ptr = usermapped_senda_softint
};

struct minix_ipcvecs minix_ipcvecs_sysenter = {
	.send_ptr = usermapped_send_sysenter,
	.receive_ptr = usermapped_receive_sysenter,
	.sendrec_ptr = usermapped_sendrec_sysenter,
	.sendnb_ptr = usermapped_sendnb_sysenter,
	.notify_ptr = usermapped_notify_sysenter,
	.do_kernel_call_ptr = usermapped_do_kernel_call_sysenter,
	.senda_ptr = usermapped_senda_sysenter
};

struct minix_ipcvecs minix_ipcvecs_syscall = {
	.send_ptr = usermapped_send_syscall,
	.receive_ptr = usermapped_receive_syscall,
	.sendrec_ptr = usermapped_sendrec_syscall,
	.sendnb_ptr = usermapped_sendnb_syscall,
	.notify_ptr = usermapped_notify_syscall,
	.do_kernel_call_ptr = usermapped_do_kernel_call_syscall,
	.senda_ptr = usermapped_senda_syscall
};


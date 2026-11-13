#ifndef _I386_IPCCONST_H_
#define _I386_IPCCONST_H_

/* IPC message payload size (bytes after m_source and m_type). */
#define _MSG_PAYLOAD_SIZE	56

#define KERVEC_INTR 32     /* syscall trap to kernel */
#define IPCVEC_INTR 33     /* ipc trap to kernel  */

#define KERVEC_UM 34     /* syscall trap to kernel, user-mapped code */
#define IPCVEC_UM 35     /* ipc trap to kernel, user-mapped code  */

#define IPC_STATUS_REG		bx

#endif  /* _I386_IPCCONST_H_ */

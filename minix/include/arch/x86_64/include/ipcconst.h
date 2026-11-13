#ifndef _X86_64_IPCCONST_H_
#define _X86_64_IPCCONST_H_

#include <machine/interrupt.h>

/*
 * IPC message payload size: 88 bytes (4 m_source + 4 m_type + 88 payload = 96
 * total), sized to hold 11 x 8-byte pointers/longs for the AMD64 ABI.
 */
#define _MSG_PAYLOAD_SIZE	88

/*
 * Individual mess_* struct size assertions are still suppressed on x86_64:
 * the existing structs were designed for i386 (56-byte payload) and most do
 * not yet have amd64-specific padding/sizing.  The top-level message union
 * size IS enforced via the _ASSERT_message typedef in ipc.h.
 */
#define _ASSERT_MSG_SIZE(msg_type)	/* not enforced per-struct on x86_64 */

/* Keep the software-call vectors in sync with machine/interrupt.h. */
#define KERVEC_INTR KERN_CALL_VECTOR_ORIG
#define IPCVEC_INTR IPC_VECTOR_ORIG

#define KERVEC_UM KERN_CALL_VECTOR_UM
#define IPCVEC_UM IPC_VECTOR_UM

#define IPC_STATUS_REG		bx

#endif  /* _X86_64_IPCCONST_H_ */

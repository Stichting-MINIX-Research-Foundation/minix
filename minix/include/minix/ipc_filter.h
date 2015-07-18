/* IPC filter definitions. */

#ifndef _MINIX_IPC_FILTER_H
#define _MINIX_IPC_FILTER_H

#include <minix/com.h>
#include <minix/config.h>

/* Special message sources, allowed in IPC filters only. */
#define ANY_USR		_ENDPOINT(1, _ENDPOINT_P(ANY))
#define ANY_SYS		_ENDPOINT(2, _ENDPOINT_P(ANY))
#define ANY_TSK		_ENDPOINT(3, _ENDPOINT_P(ANY))

/* IPC filter constants. */
#define IPCF_MAX_ELEMENTS       (NR_SYS_PROCS * 2)

/* IPC filter flags. */
#define IPCF_MATCH_M_SOURCE    0x1
#define IPCF_MATCH_M_TYPE      0x2
#define IPCF_EL_BLACKLIST      0x4
#define IPCF_EL_WHITELIST      0x8

struct ipc_filter_el_s {
    int flags;
    endpoint_t m_source;
    int m_type;
};
typedef struct ipc_filter_el_s ipc_filter_el_t;

#endif /* _MINIX_IPC_FILTER_H */

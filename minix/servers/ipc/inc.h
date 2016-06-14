#define _SYSTEM		1	/* get OK and negative error codes */

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/sysutil.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <minix/rmib.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <machine/vmparam.h>

#include <lib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

/*
 * On NetBSD, these macros are only defined when _KERNEL is set.  However,
 * since ipcs(1) uses IXSEQ_TO_IPCID, NetBSD cannot change these macros without
 * breaking the userland API.  Thus, having a copy of them here is not risky.
 */
#define IPCID_TO_IX(id)		((id) & 0xffff)
#define IPCID_TO_SEQ(id)	(((id) >> 16) & 0xffff)

/* main.c */
void update_sem_sub(int);

/* shm.c */
int do_shmget(message *);
int do_shmat(message *);
int do_shmdt(message *);
int do_shmctl(message *);
int get_shm_mib_info(struct rmib_oldp *);
int is_shm_nil(void);
void update_refcount_and_destroy(void);

/* sem.c */
int do_semget(message *);
int do_semctl(message *);
int do_semop(message *);
int get_sem_mib_info(struct rmib_oldp *);
int is_sem_nil(void);
void sem_process_event(endpoint_t, int);

/* utility.c */
int check_perm(struct ipc_perm *, endpoint_t, int);
void prepare_mib_perm(struct ipc_perm_sysctl *, const struct ipc_perm *);

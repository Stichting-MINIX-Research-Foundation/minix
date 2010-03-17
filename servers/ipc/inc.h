#define _POSIX_SOURCE      1	/* tell headers to include POSIX stuff */
#define _MINIX             1	/* tell headers to include MINIX stuff */
#define _SYSTEM            1    /* get OK and negative error codes */

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/sysutil.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <machine/vm.h>
#include <sys/vm.h>

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

_PROTOTYPE( int do_shmget, (message *)                                   );
_PROTOTYPE( int do_shmat, (message *)                                    );
_PROTOTYPE( int do_shmdt, (message *)                                    );
_PROTOTYPE( int do_shmctl, (message *)                                   );
_PROTOTYPE( int check_perm, (struct ipc_perm *, endpoint_t, int)         );
_PROTOTYPE( void update_refcount_and_destroy, (void)                     );
_PROTOTYPE( int do_semget, (message *)                                   );
_PROTOTYPE( int do_semctl, (message *)                                   );
_PROTOTYPE( int do_semop, (message *)                                    );
_PROTOTYPE( int is_sem_nil, (void)                                       );
_PROTOTYPE( int is_shm_nil, (void)                                       );
_PROTOTYPE( void sem_process_vm_notify, (void)                           );

EXTERN int identifier;
EXTERN endpoint_t who_e;
EXTERN int call_type;
EXTERN endpoint_t SELF_E;


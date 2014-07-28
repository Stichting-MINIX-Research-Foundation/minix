#ifndef __LWIP_SYS_ARCH_H__
#define __LWIP_SYS_ARCH_H__

#include <sys/cdefs.h>
#include <minix/sysutil.h>

#define NOT_IMPLEMENTED panic("liblwip : %s NOT_IMPLEMENTED", __func__)

typedef int sys_sem_t;

static inline err_t sys_sem_new(__unused sys_sem_t *sem, __unused u8_t count)
{
	return ERR_OK;
}

static inline void sys_sem_signal(__unused sys_sem_t *sem)
{
}

static inline u32_t sys_arch_sem_wait(__unused sys_sem_t *sem, __unused u32_t timeout)
{
	return 0;
}

static inline void sys_sem_free(__unused sys_sem_t *sem)
{
	NOT_IMPLEMENTED;
}

static inline int sys_sem_valid(__unused sys_sem_t *sem)
{
	NOT_IMPLEMENTED;
}

static inline void sys_sem_set_invalid(__unused sys_sem_t *sem)
{
	NOT_IMPLEMENTED;
}

#define LWIP_COMPAT_MUTEX 1

typedef int sys_mbox_t;

static inline err_t sys_mbox_new(__unused sys_mbox_t *mbox, __unused int size)
{
	NOT_IMPLEMENTED;
}

static inline void sys_mbox_post(__unused sys_mbox_t *mbox, __unused void *msg)
{
	NOT_IMPLEMENTED;
}

static inline err_t sys_mbox_trypost(__unused sys_mbox_t *mbox, __unused void *msg)
{
	NOT_IMPLEMENTED;
}

static inline u32_t sys_arch_mbox_fetch(__unused sys_mbox_t *mbox,
					__unused void **msg,
					__unused u32_t timeout)
{
	NOT_IMPLEMENTED;
}

static inline u32_t sys_arch_mbox_tryfetch(__unused sys_mbox_t *mbox, __unused void **msg)
{
	NOT_IMPLEMENTED;
}

static inline void sys_mbox_free(__unused sys_mbox_t *mbox)
{
	NOT_IMPLEMENTED;
}

static inline int sys_mbox_valid(__unused sys_mbox_t *mbox)
{
	NOT_IMPLEMENTED;
}

static inline void sys_mbox_set_invalid(__unused sys_mbox_t *mbox)
{
	NOT_IMPLEMENTED;
}

typedef int sys_thread_t;

#endif /* __LWIP_SYS_ARCH_H__ */

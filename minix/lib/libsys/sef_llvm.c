#include "syslib.h"
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <minix/sysutil.h>

/* Stack refs definitions. */
extern char **environ;
extern char **env_argv;
extern int env_argc;

#define sef_llvm_stack_refs_save_one(P, T, R) { *((T*)P) = R; P += sizeof(T); }
#define sef_llvm_stack_refs_restore_one(P, T, R) { R = *((T*)P); P += sizeof(T); }

/*===========================================================================*
 *      	              sef_llvm_magic_enabled                         *
 *===========================================================================*/
int sef_llvm_magic_enabled(void)
{
    extern void __attribute__((weak)) magic_init(void);
    if (!magic_init)
        return 0;
    return 1;
}

/*===========================================================================*
 *      	                sef_llvm_real_brk                            *
 *===========================================================================*/
int sef_llvm_real_brk(char *newbrk)
{
    extern int __attribute__((weak)) _magic_real_brk(char*);
    if (!_magic_real_brk)
        return brk(newbrk);
    return _magic_real_brk(newbrk);
}

/*===========================================================================*
 *      	              sef_llvm_state_cleanup                         *
 *===========================================================================*/
int sef_llvm_state_cleanup(void)
{
    return OK;
}

/*===========================================================================*
 *      	                sef_llvm_dump_eval                           *
 *===========================================================================*/
void sef_llvm_dump_eval(char *expr)
{
    extern void __attribute__((weak)) _magic_dump_eval_bool(char*);
    if (!_magic_dump_eval_bool)
        return;
    return _magic_dump_eval_bool(expr);
}

/*===========================================================================*
 *      	               sef_llvm_eval_bool                            *
 *===========================================================================*/
int sef_llvm_eval_bool(char *expr, char *result)
{
    extern int __attribute__((weak)) magic_eval_bool(char*, char*);
    if (!magic_eval_bool)
        return 0;
    return magic_eval_bool(expr, result);
}

/*===========================================================================*
 *      	            sef_llvm_state_table_addr                        *
 *===========================================================================*/
void *sef_llvm_state_table_addr(void)
{
    extern void* __attribute__((weak)) _magic_vars_addr(void);
    if (!_magic_vars_addr)
        return NULL;
    return _magic_vars_addr();
}

/*===========================================================================*
 *      	            sef_llvm_state_table_size                        *
 *===========================================================================*/
size_t sef_llvm_state_table_size(void)
{
    extern size_t __attribute__((weak)) _magic_vars_size(void);
    if (!_magic_vars_size)
        return 0;
    return _magic_vars_size();
}

/*===========================================================================*
 *      	            sef_llvm_stack_refs_save                         *
 *===========================================================================*/
void sef_llvm_stack_refs_save(char *stack_buff)
{
    extern void __attribute__((weak)) st_stack_refs_save_restore(char*, int);
    char *p = stack_buff;

    sef_llvm_stack_refs_save_one(p, char**, environ);
    sef_llvm_stack_refs_save_one(p, char**, env_argv);
    sef_llvm_stack_refs_save_one(p, int, env_argc);

    if (st_stack_refs_save_restore)
        st_stack_refs_save_restore(p, 1);
}

/*===========================================================================*
 *      	           sef_llvm_stack_refs_restore                       *
 *===========================================================================*/
void sef_llvm_stack_refs_restore(char *stack_buff)
{
    extern void __attribute__((weak)) st_stack_refs_save_restore(char*, int);
    char *p = stack_buff;

    sef_llvm_stack_refs_restore_one(p, char**, environ);
    sef_llvm_stack_refs_restore_one(p, char**, env_argv);
    sef_llvm_stack_refs_restore_one(p, int, env_argc);

    if (st_stack_refs_save_restore)
        st_stack_refs_save_restore(p, 0);
}

/*===========================================================================*
 *      	            sef_llvm_state_transfer                          *
 *===========================================================================*/
int sef_llvm_state_transfer(sef_init_info_t *info)
{
    extern int __attribute__((weak)) _magic_state_transfer(sef_init_info_t *info);
    if (!_magic_state_transfer)
        return ENOSYS;
    return _magic_state_transfer(info);
}

/*===========================================================================*
 *      	        sef_llvm_add_special_mem_region                      *
 *===========================================================================*/
int sef_llvm_add_special_mem_region(void *addr, size_t len, const char* name)
{
    extern int __attribute__((weak)) st_add_special_mmapped_region(void *addr,
        size_t len, const char* name);
    if (!st_add_special_mmapped_region)
        return 0;
    return st_add_special_mmapped_region(addr, len, name);
}

/*===========================================================================*
 *      	    sef_llvm_del_special_mem_region_by_addr                  *
 *===========================================================================*/
int sef_llvm_del_special_mem_region_by_addr(void *addr)
{
    extern int __attribute__((weak)) st_del_special_mmapped_region_by_addr(
        void *addr);
    if (!st_del_special_mmapped_region_by_addr)
        return 0;
    return st_del_special_mmapped_region_by_addr(addr);
}

/*===========================================================================*
 *				sef_llvm_ds_st_init			     *
 *===========================================================================*/
void sef_llvm_ds_st_init(void)
{
    extern void __attribute__((weak)) _magic_ds_st_init(void);
    if (!_magic_ds_st_init)
        return;
    _magic_ds_st_init();
}

/*===========================================================================*
 *				sef_llvm_ac_mmap			     *
 *===========================================================================*/
void* sef_llvm_ac_mmap(void *buf, size_t len, int prot, int flags, int fd,
	off_t offset)
{
    int r;
    extern void* __attribute__((weak))
       _magic_real_mmap(void*, size_t, int, int, int, off_t);
    if (!_magic_real_mmap)
        return mmap(buf, len, prot, flags, fd, offset);

    /* Avoid regular dsentries for non-relocatable regions (e.g., DMA buffers).
     */
    buf = _magic_real_mmap(buf, len, prot, flags, fd, offset);
    if(buf == MAP_FAILED)
        return buf;
    r = sef_llvm_add_special_mem_region(buf, len, NULL);
    if(r < 0)
        printf("sef_llvm_add_special_mem_region failed: %d\n", r);
    return buf;
}

/*===========================================================================*
 *				sef_llvm_ac_munmap			     *
 *===========================================================================*/
int sef_llvm_ac_munmap(void *buf, size_t len)
{
    int r;
    extern int __attribute__((weak)) _magic_real_munmap(void*, size_t);
    if (!_magic_real_munmap)
        return munmap(buf, len);

    if ((r = _magic_real_munmap(buf, len)) != 0)
        return r;
    if ((r = sef_llvm_del_special_mem_region_by_addr(buf)) < 0)
        printf("sef_llvm_del_special_mem_region_by_addr failed: %d\n", r);
    return 0;
}

/*===========================================================================*
 *      	             sef_llvm_ltckpt_enabled                         *
 *===========================================================================*/
int sef_llvm_ltckpt_enabled(void)
{
    extern int __attribute__((weak)) ltckpt_mechanism_enabled(void);
    if (!sef_llvm_get_ltckpt_offset() || !ltckpt_mechanism_enabled())
        return 0;
    return 1;
}

/*===========================================================================*
 *      	            sef_llvm_ltckpt_get_offset                       *
 *===========================================================================*/
int sef_llvm_get_ltckpt_offset(void)
{
    extern int __attribute__((weak)) ltckpt_get_offset(void);
    if (!ltckpt_get_offset)
        return 0;
    return ltckpt_get_offset();
}

/*===========================================================================*
 *      	             sef_llvm_ltckpt_restart                         *
 *===========================================================================*/
int sef_llvm_ltckpt_restart(int type, sef_init_info_t *info)
{
    extern int __attribute__((weak)) ltckpt_restart(void *);

    if(!sef_llvm_ltckpt_enabled())
        return sef_cb_init_identity_state_transfer(type, info);

    assert(ltckpt_restart);
    return ltckpt_restart(info);
}

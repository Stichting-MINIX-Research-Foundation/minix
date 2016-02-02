#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>

#include <magic.h>
#include <magic_mem.h>
#include <st/state_transfer.h>
#include <st/special.h>

static const char* _magic_generic_debug_header(void)
{
    return "[DEBUG]";
}

/*===========================================================================*
 *      	                _magic_dump_eval_bool                           *
 *===========================================================================*/
void _magic_dump_eval_bool(char *expr)
{
    extern char *sef_lu_state_eval;
    char result;
    int print_style;
    (void)(result);
    print_style = magic_eval_get_print_style();
    magic_eval_set_print_style(MAGIC_EVAL_PRINT_STYLE_ALL);
    magic_eval_bool(sef_lu_state_eval, &result);
    magic_eval_set_print_style(print_style);
}

/*===========================================================================*
 *                          _magic_real_alloc_contig                         *
 *===========================================================================*/
void *_magic_real_alloc_contig(size_t len, int flags, uint32_t *phys)
{
    return magic_real_mmap(NULL, len, PROT_READ|PROT_WRITE,
        MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
}

/*===========================================================================*
 *                          _magic_real_free_contig                          *
 *===========================================================================*/
int _magic_real_free_contig(void *addr, size_t len)
{
    return munmap(addr, len);
}

/*===========================================================================*
 *      	                _magic_real_brk                            *
 *===========================================================================*/
int _magic_real_brk(char *newbrk)
{
    return magic_real_brk(newbrk);
}

/*===========================================================================*
 *      	                _magic_real_mmap                            *
 *===========================================================================*/
void* _magic_real_mmap(void *buf, size_t len, int prot, int flags, int fd, off_t offset)
{
    return magic_real_mmap(buf, len, prot, flags, fd, offset);
}

/*===========================================================================*
 *                              _magic_real_munmap                           *
 *===========================================================================*/
int _magic_real_munmap(void *addr, size_t length)
{
    return magic_real_munmap(addr, length);
}

/*===========================================================================*
 *      	            _magic_state_transfer                          *
 *===========================================================================*/
int _magic_state_transfer(sef_init_info_t *info)
{
    st_init_info_t st_info;
    /* Convert SEF flags into ST flags. */
    st_info.flags = 0;
    if (info->flags & SEF_LU_ASR)
        st_info.flags |= ST_LU_ASR;
    if (info->flags & SEF_LU_NOMMAP)
        st_info.flags |= ST_LU_NOMMAP;
    st_info.init_buff_start = info->init_buff_start;
    st_info.init_buff_cleanup_start = info->init_buff_cleanup_start;
    st_info.init_buff_len = info->init_buff_len;
    /* Transmit sef_init_info opaquely to the state transfer framework. */
    st_info.info_opaque = (void *) (info);
    /* Add the OS callbacks. */
    st_info.st_cbs_os.panic = &(panic);                                              /* panic() callback. */
    st_info.st_cbs_os.old_state_table_lookup = &(sef_old_state_table_lookup_opaque); /* old_state_table_lookup() callback. */
    st_info.st_cbs_os.copy_state_region = &(sef_copy_state_region_opaque);           /* copy_state_region() callback. */
    st_info.st_cbs_os.alloc_contig = &(_magic_real_alloc_contig);                    /* alloc_contig() callback. */
    st_info.st_cbs_os.free_contig = &(_magic_real_free_contig);                      /* free_contig() callback. */
    st_info.st_cbs_os.debug_header = &(_magic_generic_debug_header);                 /* debug_header() callback. */
    return st_state_transfer(&st_info);
}



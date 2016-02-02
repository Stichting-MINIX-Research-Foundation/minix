#include <magic.h>
#include <magic_mem.h>
#include <magic_asr.h>
#include <magic_eval.h>
#include <magic_analysis.h>
#include <magic_splay_tree.h>
#include <stdarg.h>
#if MAGIC_MEM_USAGE_OUTPUT_CTL
#include <common/util/time.h>
#endif

/* Workaround for extern-only structs. */
#ifndef __MINIX
#include <stdio.h>
EXTERN FILE *stdout;
PUBLIC FILE **UNUSED(_____magic_instr_FILE_unused) = &stdout;

#include <ncurses.h>
PUBLIC WINDOW *UNUSED(_____magic_instr_WINDOW_unused);
#endif

#include <netinet/in.h>
PUBLIC struct in6_addr *UNUSED(_____magic_instr_in6_addr_unused);

/* Magic printf. */
MAGIC_VAR printf_ptr_t _magic_printf = MAGIC_PRINTF_DEFAULT;

/* Magic lock primitives. */
PUBLIC magic_lock_t magic_dsentry_lock = NULL;
PUBLIC magic_unlock_t magic_dsentry_unlock = NULL;
PUBLIC void *magic_dsentry_lock_args = NULL;
PUBLIC void *magic_dsentry_unlock_args = NULL;

PUBLIC magic_lock_t magic_dfunction_lock = NULL;
PUBLIC magic_unlock_t magic_dfunction_unlock = NULL;
PUBLIC void *magic_dfunction_lock_args = NULL;
PUBLIC void *magic_dfunction_unlock_args = NULL;

PUBLIC magic_lock_t magic_dsodesc_lock = NULL;
PUBLIC magic_unlock_t magic_dsodesc_unlock = NULL;
PUBLIC void *magic_dsodesc_lock_args = NULL;
PUBLIC void *magic_dsodesc_unlock_args = NULL;

PUBLIC magic_lock_t magic_mpdesc_lock = NULL;
PUBLIC magic_unlock_t magic_mpdesc_unlock = NULL;
PUBLIC void *magic_mpdesc_lock_args = NULL;
PUBLIC void *magic_mpdesc_unlock_args = NULL;

/* Magic vars references. */
MAGIC_VAR struct _magic_vars_t _magic_vars_buff = {

    /* Address Space Randomization (ASRPass) */
    0, /* asr_seed */
    0, /* asr_heap_map_do_permutate */
    0, /* asr_heap_max_offset */
    0, /* asr_heap_max_padding */
    0, /* asr_map_max_offset_pages */
    0, /* asr_map_max_padding_pages */

    /* Runtime flags. */
    0,    /* no_mem_inst */

    /* Magic type array. */
    NULL, /* types */
    0,    /* types_num */
    0,    /* types_next_id */

    /* Magic function array. */
    NULL, /* functions */
    0,    /* functions_num */
    0,    /* functions_next_id */

    /* Magic state entry array. */
    NULL, /* sentries */
    0,    /* sentries_num */
    0,    /* sentries_str_num */
    0,    /* sentries_next_id */

    /* Magic dynamic state index array. */
    NULL, /* dsindexes */
    0,    /* dsindexes_num */

    /* Magic dynamic state entry list. */
    NULL, /* first_dsentry */
    0,    /* num_dead_sentries */
    0,    /* size_dead_dsentries */

    /* Magic memory pool dynamic state entry list. */
    NULL, /* first_mempool_dsentry */

    /* Magic dynamic function list. */
    NULL, /* first_dfunction */
    NULL, /* last_dfunction */
    0,    /* dfunctions_num */

    /* Magic SO library descriptor list. */
    NULL, /* first_sodesc */
    NULL, /* last_sodesc */
    0,    /* sodescs_num */

    /* Magic DSO library descriptor list. */
    NULL, /* first_dsodesc */
    NULL, /* last_dsodesc */
    0,    /* dsodescs_num */

    /* Magic stack-related variables. */
    NULL, /* first_stack_dsentry */
    NULL, /* last_stack_dsentry */

    /* Magic memory ranges */

    { (void*) ULONG_MAX, (void*) 0 }, /* *null_range[2] */
    {0,0}, /* *data_range[2] */
    {0,0}, /* *heap_range[2] */
    {0,0}, /* *map_range[2] */
    {0,0}, /* *shm_range[2] */
    {0,0}, /* *stack_range[2] */
    {0,0}, /* *text_range[2] */

    {0,0}, /* *sentry_range[2] */
    {0,0}, /* *function_range[2] */
    {0,0}, /* *dfunction_range[2] */

    NULL, /* *heap_start */
    NULL, /* *heap_end */
    1,     /* update_dsentry_ranges */
    1,     /* update_dfunction_ranges */

#ifdef __MINIX
    { { NULL, 0 } }, /* unmap_mem */
#endif

    NULL,  /* sentry_rl_buff */
    0,     /* sentry_rl_buff_offset */
    0,     /* sentry_rl_buff_size */
    NULL,  /* sentry_rl_index */

    NULL,  /* sentry_hash_buff */
    0,     /* sentry_hash_buff_offset */
    0,     /* sentry_hash_buff_size */
    NULL,  /* sentry_hash_head */

    NULL,  /* function_hash_buff */
    0,     /* function_hash_buff_offset */
    0,     /* function_hash_buff_size */
    NULL,  /* function_hash_head */

    0      /* fake_malloc */
};

PUBLIC struct _magic_vars_t *_magic_vars = &_magic_vars_buff;

/* Magic void ptr and array (force at the least 1 void* and 1 void array in the list of globals). */
PUBLIC void* MAGIC_VOID_PTR = NULL;
PUBLIC char MAGIC_VOID_ARRAY[1];

/* Magic special types. */
MAGIC_VAR struct _magic_type *MAGIC_VOID_PTR_TYPE = NULL;
MAGIC_VAR struct _magic_type *MAGIC_VOID_PTR_INT_CAST_TYPE = NULL;
MAGIC_VAR struct _magic_type *MAGIC_VOID_ARRAY_TYPE = NULL;
MAGIC_VAR struct _magic_type *MAGIC_PTRINT_TYPE = NULL;
MAGIC_VAR struct _magic_type *MAGIC_PTRINT_ARRAY_TYPE = NULL;

/* Magic annotations. */
MAGIC_VAR VOLATILE int MAGIC_CALL_ANNOTATION_VAR;

/* Magic status variables. */
PUBLIC int magic_init_done = 0;
PUBLIC int magic_libcommon_active = 0;
PUBLIC int magic_lookup_nested_dsentries = 1;
PUBLIC int magic_allow_dead_dsentries = MAGIC_ALLOW_DEAD_DSENTRIES_DEFAULT;
PUBLIC int magic_ignore_dead_dsentries = 0;
PUBLIC int magic_mmap_dsentry_header_prot = PROT_READ | PROT_WRITE;
MAGIC_VAR int _magic_enabled = 0;
MAGIC_VAR int _magic_checkpoint_enabled = 0;
MAGIC_VAR int _magic_lazy_checkpoint_enabled = 0;

/* Magic out-of-band dsentries. */
PUBLIC struct _magic_obdsentry _magic_obdsentries[MAGIC_MAX_OBDSENTRIES];

/* Pool management data. */
PUBLIC struct _magic_mpdesc _magic_mpdescs[MAGIC_MAX_MEMPOOLS];

/* Magic page size. */
PUBLIC unsigned long magic_sys_pagesize = 0;

/* Private variables. */
PUBLIC int magic_type_str_print_style = MAGIC_TYPE_STR_PRINT_STYLE_DEFAULT;
PRIVATE THREAD_LOCAL const struct _magic_type* magic_nested_types[MAGIC_MAX_RECURSIVE_TYPES] = {0};
PRIVATE THREAD_LOCAL const struct _magic_type* magic_nested_types2[MAGIC_MAX_RECURSIVE_TYPES] = {0};
PRIVATE THREAD_LOCAL unsigned magic_level = 0;
PRIVATE THREAD_LOCAL unsigned magic_counter;
PRIVATE THREAD_LOCAL struct _magic_dsentry magic_dsentry_buff;
PRIVATE THREAD_LOCAL struct _magic_dfunction magic_dfunction_buff;

/* Magic default stubs. */
PUBLIC struct _magic_type magic_default_type = {
    0, "", NULL, 0, "", 0, 0, NULL, NULL, NULL, NULL, NULL, MAGIC_TYPE_OPAQUE, 0, 0, NULL
};

PUBLIC struct _magic_dsentry magic_default_dsentry = {
    MAGIC_DSENTRY_MNUM, /* magic_number */
    "", /* parent_name */
    { 0 }, /* name_ext_buff */
    { 0, "", NULL, MAGIC_STATE_DYNAMIC, NULL, NULL }, /* sentry */
    { 0, "", NULL, 0, "", 0, 0, NULL, NULL, NULL, NULL, NULL, MAGIC_TYPE_ARRAY, MAGIC_TYPE_IS_ROOT|MAGIC_TYPE_DYNAMIC, 0, NULL }, /* type */
    { NULL }, /* type_array */
#if MAGIC_DSENTRY_ALLOW_PREV
    NULL, /* prev */
#endif
    NULL, /* next */
    NULL, /* next_mpool */
    NULL, /* next_mblock */
#ifndef __MINIX
    NULL, /* next_sobject */
    NULL, /* sobject_base_addr */
#endif
    NULL, /* ext */
    MAGIC_DSENTRY_MSTATE_ALIVE, /* magic_state */
    { { 0, 0 } }, /* alloc_flags */
    0 /* site_id */
};

PUBLIC struct _magic_dfunction magic_default_dfunction = {
    MAGIC_DFUNCTION_MNUM,
    "",
    { 0, "", NULL, MAGIC_STATE_DYNAMIC|MAGIC_STATE_TEXT|MAGIC_STATE_CONSTANT, NULL },
    NULL,
    NULL
};

PUBLIC struct _magic_type magic_default_ret_addr_type = {
    0, "", NULL, 0, "", sizeof(void*), 1, NULL, NULL, NULL, NULL, NULL, MAGIC_TYPE_POINTER, MAGIC_TYPE_IS_ROOT|MAGIC_TYPE_DYNAMIC|MAGIC_TYPE_INT_CAST|MAGIC_TYPE_STRICT_VALUE_SET, 0, NULL
};

/* Magic code reentrant flag. */
PRIVATE int magic_reentrant = 1;

/*===========================================================================*
 *      	            _magic_vars_addr                        *
 *===========================================================================*/
void *_magic_vars_addr()
{
    return _magic_vars;
}

/*===========================================================================*
 *      	            _magic_vars_size                        *
 *===========================================================================*/
size_t _magic_vars_size()
{
    return sizeof(struct _magic_vars_t);
}

/*===========================================================================*
 *                          magic_null_printf                                *
 *===========================================================================*/
PUBLIC int magic_null_printf(const char *format, ...)
{
  return 0;
}

#ifndef __MINIX
/*===========================================================================*
 *                           magic_err_printf                                *
 *===========================================================================*/
PUBLIC int magic_err_printf(const char *format, ...)
{
  va_list va;
  int ret;
  va_start(va, format);
  ret = vfprintf(stderr, format, va);
  va_end(va);

  return ret;
}
#endif

/*===========================================================================*
 *                           magic_set_printf                                *
 *===========================================================================*/
PUBLIC void magic_set_printf(printf_ptr_t func_ptr)
{
   assert(func_ptr);
   _magic_printf = func_ptr;
}

/*===========================================================================*
 *                           magic_get_printf                                *
 *===========================================================================*/
PUBLIC printf_ptr_t magic_get_printf(void)
{
   return _magic_printf;
}

/*===========================================================================*
 *                        magic_reentrant_enable                             *
 *===========================================================================*/
PUBLIC void magic_reentrant_enable(void)
{
    magic_reentrant = 1;
}
/*===========================================================================*
 *                        magic_reentrant_disable                            *
 *===========================================================================*/
PUBLIC void magic_reentrant_disable(void)
{
    magic_reentrant = 0;
}

/*===========================================================================*
 *                          magic_assert_failed                              *
 *===========================================================================*/
PUBLIC void __dead magic_assert_failed(const char *assertion, const char *file,
    const char *function, const int line)
{
    _magic_printf("Assertion '%s' failed in file %s, function %s(), line %d, pid %d\n",
        assertion, file, function, line, getpid());
    abort();
}

/*===========================================================================*
 *                         magic_get_sys_pagesize                            *
 *===========================================================================*/
PUBLIC unsigned long magic_get_sys_pagesize()
{
    if(!magic_sys_pagesize) {
        magic_sys_pagesize = SYS_PAGESIZE;
    }
    return magic_sys_pagesize;
}

/*===========================================================================*
 *                   magic_dsentry_set_lock_primitives                       *
 *===========================================================================*/
PUBLIC void magic_dsentry_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args)
{
    assert(lock && unlock);
    magic_dsentry_lock = lock;
    magic_dsentry_unlock = unlock;
    magic_dsentry_lock_args = lock_args;
    magic_dsentry_unlock_args = unlock_args;
}

/*===========================================================================*
 *                   magic_dfunction_set_lock_primitives                     *
 *===========================================================================*/
PUBLIC void magic_dfunction_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args)
{
    assert(lock && unlock);
    magic_dfunction_lock = lock;
    magic_dfunction_unlock = unlock;
    magic_dfunction_lock_args = lock_args;
    magic_dfunction_unlock_args = unlock_args;
}

/*===========================================================================*
 *                    magic_dsodesc_set_lock_primitives                      *
 *===========================================================================*/
PUBLIC void magic_dsodesc_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args)
{
    assert(lock && unlock);
    magic_dsodesc_lock = lock;
    magic_dsodesc_unlock = unlock;
    magic_dsodesc_lock_args = lock_args;
    magic_dsodesc_unlock_args = unlock_args;
}

/*===========================================================================*
 *                 magic_mpdesc_set_lock_primitives                          *
 *===========================================================================*/
PUBLIC void magic_mpdesc_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args)
{
    assert(lock && unlock);
    magic_mpdesc_lock = lock;
    magic_mpdesc_unlock = unlock;
    magic_mpdesc_lock_args = lock_args;
    magic_mpdesc_unlock_args = unlock_args;
}

/*===========================================================================*
 *                            magic_types_init                               *
 *===========================================================================*/
PRIVATE void magic_types_init()
{
    static struct _magic_type _magic_void_ptr_type_buff;
    static struct _magic_type _magic_void_array_type_buff;
    static struct _magic_type *_magic_void_array_type_contained_types[1];
    static struct _magic_type _magic_ptrint_type_buff;
    static const char* _magic_ptrint_type_name = "ptrint";
    static char _magic_ptrint_type_str_buff[8];
    static struct _magic_type _magic_ptrint_array_type_buff;
    static struct _magic_type *_magic_ptrint_array_type_contained_types[1];

    assert(MAGIC_VOID_PTR_TYPE);
    assert(MAGIC_VOID_PTR_TYPE->size == sizeof(void*));
    assert(MAGIC_VOID_TYPE->size == sizeof(char));

    MAGIC_VOID_PTR_INT_CAST_TYPE = &_magic_void_ptr_type_buff;
    *MAGIC_VOID_PTR_INT_CAST_TYPE = *MAGIC_VOID_PTR_TYPE;
    MAGIC_VOID_PTR_INT_CAST_TYPE->flags |= MAGIC_TYPE_INT_CAST;

    MAGIC_VOID_ARRAY_TYPE = &_magic_void_array_type_buff;
    *MAGIC_VOID_ARRAY_TYPE = magic_default_type;
    MAGIC_TYPE_ARRAY_CREATE_FROM_N(MAGIC_VOID_ARRAY_TYPE, MAGIC_VOID_TYPE,
        _magic_void_array_type_contained_types, 1);

    MAGIC_PTRINT_TYPE = &_magic_ptrint_type_buff;
    *MAGIC_PTRINT_TYPE = magic_default_type;
    MAGIC_TYPE_INT_CREATE(MAGIC_PTRINT_TYPE, MAGIC_VOID_PTR_TYPE->size,
        _magic_ptrint_type_name, _magic_ptrint_type_str_buff);

    MAGIC_PTRINT_ARRAY_TYPE = &_magic_ptrint_array_type_buff;
    *MAGIC_PTRINT_ARRAY_TYPE = magic_default_type;
    MAGIC_TYPE_ARRAY_CREATE_FROM_N(MAGIC_PTRINT_ARRAY_TYPE, MAGIC_PTRINT_TYPE,
        _magic_ptrint_array_type_contained_types, 1);
}

/*===========================================================================*
 *                           magic_data_init                                 *
 *===========================================================================*/
MAGIC_FUNC void magic_data_init(void)
{
    MAGIC_FUNC_BODY();
}

/*===========================================================================*
 *                              magic_init                                   *
 *===========================================================================*/
PUBLIC void magic_init(void)
{
  unsigned i;

  if(magic_init_done || !_magic_enabled) {
     return;
  }

  /* Initialize magic data structures first. */
  magic_data_init();

  /* Initialize asr support. */
  magic_asr_init();

  /* Initialize eval support. */
  magic_eval_init();

  /* Initialize magic obdsentries. */
  memset(_magic_obdsentries, 0, MAGIC_MAX_OBDSENTRIES * sizeof(struct _magic_obdsentry));

  /* Initialize memory pool descriptors. */
  for (i = 0; i < MAGIC_MAX_MEMPOOLS; i++) {
      snprintf(_magic_mpdescs[i].name, sizeof(_magic_mpdescs[i].name), "%s%d%s", MAGIC_MEMPOOL_NAME_PREFIX, i, MAGIC_ALLOC_NAME_SUFFIX);
  }

  /* Initialize special types. */
  magic_types_init();

  /* Initialize magic ranges. */
  magic_ranges_init();

  /* Perform stack-related initialization. */
  magic_stack_init();

#if MAGIC_MEM_USAGE_OUTPUT_CTL
   /* Initialize CPU frequency - used for timestamp logging. */
   magic_cycles_per_ns = util_time_get_cycles_per_ns(1);
#endif

  /* Checks. */
  assert(magic_check_sentries() && "Bad sentries!");
  assert(magic_check_dsentries_safe() && "Bad dsentries!");

  /* All done. */
  magic_init_done = 1;
}

/*===========================================================================*
 *                          magic_do_check_dfunction                         *
 *===========================================================================*/
PRIVATE INLINE int magic_do_check_dfunction(struct _magic_dfunction *dfunction, int flags)
{
    struct _magic_function *function;
    int is_mnum_ok, is_flags_ok, is_prev_ok, is_next_ok;
    assert(dfunction && "NULL dfunction found!");
    function = MAGIC_DFUNCTION_TO_FUNCTION(dfunction);
    assert(function && "NULL function found!");
    is_mnum_ok = MAGIC_DFUNCTION_MNUM_OK(dfunction);
    if(!is_mnum_ok) {
        return FALSE;
    }
    is_flags_ok = ((function->flags & flags) == flags) && (function->flags & MAGIC_STATE_DYNAMIC) && (MAGIC_STATE_REGION(function) & MAGIC_STATE_TEXT);
    is_prev_ok = (dfunction->prev ? dfunction->prev->next && dfunction->prev->next == dfunction : dfunction == _magic_first_dfunction);
    is_next_ok = (dfunction->next ? dfunction->next->prev && dfunction->next->prev == dfunction : dfunction == _magic_last_dfunction);
    if(!is_flags_ok || !is_prev_ok || !is_next_ok) {
        _magic_printf("magic_do_check_dfunction: bad dfunction, checks: %d %d %d\n", is_flags_ok, is_prev_ok, is_next_ok);
        MAGIC_DFUNCTION_PRINT(dfunction, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        return FALSE;
    }
    return TRUE;
}

/*===========================================================================*
 *                            magic_check_dfunction                          *
 *===========================================================================*/
PUBLIC int magic_check_dfunction(struct _magic_dfunction *dfunction, int flags)
{
    int check;
    check = magic_do_check_dfunction(dfunction, flags);
    if(!check) {
        return FALSE;
    }

#if MAGIC_CHECK_LEVEL == 2
    check = magic_check_dfunctions();
    if(!check) {
        _magic_printf("magic_check_dfunction: bad other dfunction\n");
        return FALSE;
    }
#endif

    return TRUE;
}

/*===========================================================================*
 *                          magic_check_dfunctions                           *
 *===========================================================================*/
PUBLIC int magic_check_dfunctions()
{
    int magic_dfunctions_found = 0;
    struct _magic_dfunction* dfunction = NULL;
    struct _magic_function* function = NULL;
    int ret, check = TRUE;

    MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
        ret = magic_do_check_dfunction(dfunction, 0);
        if(ret == FALSE) {
            check = FALSE;
        }
        magic_dfunctions_found++;
    );
    if(magic_dfunctions_found != _magic_dfunctions_num) {
        _magic_printf("magic_check_dfunctions: magic_dfunctions_found=%d != _magic_dfunctions_num%d\n", magic_dfunctions_found, _magic_dfunctions_num);
        check = FALSE;
    }
    if(dfunction != _magic_last_dfunction) {
        _magic_printf("magic_check_dfunctions: dfunction=0x%08x != _magic_last_dfunction=0x%08x\n", dfunction, _magic_last_dfunction);
        check = FALSE;
    }
    return check;
}

/*===========================================================================*
 *                       magic_check_dfunctions_safe                         *
 *===========================================================================*/
PUBLIC int magic_check_dfunctions_safe()
{
    int ret;
    MAGIC_DFUNCTION_LOCK();
    ret = magic_check_dfunctions();
    MAGIC_DFUNCTION_UNLOCK();
    return ret;
}

/*===========================================================================*
 *                         magic_print_dfunction                             *
 *===========================================================================*/
PUBLIC void magic_print_dfunction(struct _magic_dfunction *dfunction)
{
    MAGIC_DFUNCTION_PRINT(dfunction, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                         magic_print_dfunctions                            *
 *===========================================================================*/
PUBLIC void magic_print_dfunctions()
{
    int magic_dfunctions_found = 0;
    struct _magic_dfunction* dfunction;
    struct _magic_function* function;

    _magic_printf("magic_print_dfunctions: Printing %d functions\n", _magic_dfunctions_num);
    MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
        MAGIC_DFUNCTION_PRINT(dfunction, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        magic_dfunctions_found++;
    );
    if(magic_dfunctions_found != _magic_dfunctions_num) {
        _magic_printf("magic_print_dfunctions: magic_dfunctions_found=%d != _magic_dfunctions_num%d\n", magic_dfunctions_found, _magic_dfunctions_num);
    }
}

/*===========================================================================*
 *                      magic_print_dfunctions_safe                          *
 *===========================================================================*/
PUBLIC void magic_print_dfunctions_safe()
{
    MAGIC_DFUNCTION_LOCK();
    magic_print_dfunctions();
    MAGIC_DFUNCTION_UNLOCK();
}

/*===========================================================================*
 *                             magic_copy_dfunction                          *
 *===========================================================================*/
PUBLIC void magic_copy_dfunction(struct _magic_dfunction *dfunction,
    struct _magic_dfunction *dst_dfunction)
{
    /* Raw copy. */
    memcpy(dst_dfunction, dfunction, sizeof(struct _magic_dfunction));
}

/*===========================================================================*
 *                          magic_print_dsindex                              *
 *===========================================================================*/
PUBLIC void magic_print_dsindex(struct _magic_dsindex *dsindex)
{
    MAGIC_DSINDEX_PRINT(dsindex, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                         magic_print_dsindexes                             *
 *===========================================================================*/
PUBLIC void magic_print_dsindexes()
{
    int i;
    struct _magic_dsindex* dsindex;

    _magic_printf("magic_print_dsindexes: Printing %d indexes\n", _magic_dsindexes_num);
    for(i=0;i<_magic_dsindexes_num;i++) {
        dsindex = &_magic_dsindexes[i];
        MAGIC_DSINDEX_PRINT(dsindex, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }
}

/*===========================================================================*
 *                           magic_do_check_dsentry                          *
 *===========================================================================*/
PRIVATE INLINE int magic_do_check_dsentry(struct _magic_dsentry *dsentry, int flags)
{
    struct _magic_sentry *sentry;
    int is_mnum_ok, is_mstate_ok, is_flags_ok;
    assert(dsentry && "NULL dsentry found!");
    sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
    assert(sentry && "NULL sentry found!");
    is_mnum_ok = MAGIC_DSENTRY_MNUM_OK(dsentry);
    if(!is_mnum_ok) {
        _magic_printf("magic_do_check_dsentry: bad ~mnum %08x\n", ~(dsentry->magic_number));
        return FALSE;
    }
    is_mstate_ok = MAGIC_DSENTRY_MSTATE_OK(dsentry);
    is_flags_ok = ((sentry->flags & flags) == flags) && (sentry->flags & MAGIC_STATE_DYNAMIC) && MAGIC_STATE_REGION(sentry);
    if(!is_mstate_ok || !is_flags_ok) {
        _magic_printf("magic_do_check_dsentry: bad dsentry, checks: %d %d\n", is_mstate_ok, is_flags_ok);
        MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        return FALSE;
    }
    return TRUE;
}

/*===========================================================================*
 *                             magic_check_dsentry                           *
 *===========================================================================*/
PUBLIC int magic_check_dsentry(struct _magic_dsentry *dsentry, int flags)
{
#if MAGIC_CHECK_LEVEL >= 1
    int check;
    check = magic_do_check_dsentry(dsentry, flags);
    if(!check) {
        return FALSE;
    }
#endif

#if MAGIC_CHECK_LEVEL == 2
    check = magic_check_dsentries();
    if(!check) {
        _magic_printf("magic_check_dsentry: bad other dsentry\n");
        return FALSE;
    }
#endif

    return TRUE;
}

/*===========================================================================*
 *                          magic_check_dsentries                            *
 *===========================================================================*/
PUBLIC int magic_check_dsentries()
{
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;
    int ret, check = TRUE;

    MAGIC_DSENTRY_NESTED_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        ret = magic_do_check_dsentry(dsentry, 0);
        if(ret == FALSE) {
            check = FALSE;
        }
    );
    return check;
}

/*===========================================================================*
 *                       magic_check_dsentries_safe                          *
 *===========================================================================*/
PUBLIC int magic_check_dsentries_safe()
{
    int ret;
    MAGIC_DSENTRY_LOCK();
    ret = magic_check_dsentries();
    MAGIC_DSENTRY_UNLOCK();
    return ret;
}

/*===========================================================================*
 *                          magic_print_dsentry                              *
 *===========================================================================*/
PUBLIC void magic_print_dsentry(struct _magic_dsentry *dsentry)
{
    MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                         magic_print_dsentries                             *
 *===========================================================================*/
PUBLIC void magic_print_dsentries()
{
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;
    int magic_dsentries_num = 0;

    _magic_printf("magic_print_dsentries: Printing entries\n");
    MAGIC_DSENTRY_NESTED_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        magic_dsentries_num++;
    );
    _magic_printf("magic_print_dsentries: %d entries found\n", magic_dsentries_num);
}

/*===========================================================================*
 *                       magic_print_dsentries_safe                          *
 *===========================================================================*/
PUBLIC void magic_print_dsentries_safe()
{
    MAGIC_DSENTRY_LOCK();
    magic_print_dsentries();
    MAGIC_DSENTRY_UNLOCK();
}

/*===========================================================================*
 *                             magic_copy_dsentry                            *
 *===========================================================================*/
PUBLIC void magic_copy_dsentry(struct _magic_dsentry *dsentry,
    struct _magic_dsentry *dst_dsentry)
{
    struct _magic_sentry *sentry;

    /* Raw copy. */
    memcpy(dst_dsentry, dsentry, sizeof(struct _magic_dsentry));

    /* Adjust pointers. */
    sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
    if(sentry->type == &dsentry->type) {
        MAGIC_DSENTRY_TO_SENTRY(dst_dsentry)->type = &dst_dsentry->type;
        if(sentry->type->contained_types == dsentry->type_array) {
            MAGIC_DSENTRY_TO_SENTRY(dst_dsentry)->type->contained_types = dst_dsentry->type_array;
        }
    }
}

/*===========================================================================*
 *                         magic_print_sodesc                                *
 *===========================================================================*/
PUBLIC void magic_print_sodesc(struct _magic_sodesc *sodesc)
{
    MAGIC_SODESC_PRINT(sodesc);
}

/*===========================================================================*
 *                         magic_print_sodescs                               *
 *===========================================================================*/
PUBLIC void magic_print_sodescs()
{
    int magic_sodescs_found = 0;
    struct _magic_sodesc* sodesc;

    _magic_printf("magic_print_sodescs: Printing %d sodescs\n", _magic_sodescs_num);
    MAGIC_SODESC_ITER(_magic_first_sodesc, sodesc,
        MAGIC_SODESC_PRINT(sodesc);
        _magic_printf("\n");
        magic_sodescs_found++;
    );
    if(magic_sodescs_found != _magic_sodescs_num) {
        _magic_printf("magic_print_sodescs: magic_sodescs_found=%d != _magic_sodescs_num%d\n", magic_sodescs_found, _magic_sodescs_num);
    }
}

/*===========================================================================*
 *                         magic_print_dsodesc                               *
 *===========================================================================*/
PUBLIC void magic_print_dsodesc(struct _magic_dsodesc *dsodesc)
{
    MAGIC_DSODESC_PRINT(dsodesc);
}

/*===========================================================================*
 *                         magic_print_dsodescs                              *
 *===========================================================================*/
PUBLIC void magic_print_dsodescs()
{
    int magic_dsodescs_found = 0;
    struct _magic_dsodesc* dsodesc;

    _magic_printf("magic_print_dsodescs: Printing %d dsodescs\n", _magic_dsodescs_num);
    MAGIC_DSODESC_ITER(_magic_first_dsodesc, dsodesc,
        MAGIC_DSODESC_PRINT(dsodesc);
        _magic_printf("\n");
        magic_dsodescs_found++;
    );
    if(magic_dsodescs_found != _magic_dsodescs_num) {
        _magic_printf("magic_print_dsodescs: magic_dsodescs_found=%d != _magic_dsodescs_num%d\n", magic_dsodescs_found, _magic_dsodescs_num);
    }
}

/*===========================================================================*
 *                       magic_print_dsodescs_safe                           *
 *===========================================================================*/
PUBLIC void magic_print_dsodescs_safe()
{
    MAGIC_DSODESC_LOCK();
    magic_print_dsodescs();
    MAGIC_DSODESC_UNLOCK();
}

/*===========================================================================*
 *                           magic_print_sections                            *
 *===========================================================================*/
PUBLIC void magic_print_sections(void)
{
    _magic_printf("magic_print_sections: data=[0x%08x;0x%08x], ro=[0x%08x;0x%08x], text=[0x%08x;0x%08x], st_data=[0x%08x;0x%08x], st_ro=[0x%08x;0x%08x], st_text=[0x%08x;0x%08x]",
            (unsigned long) MAGIC_DATA_SECTION_START, (unsigned long) MAGIC_DATA_SECTION_END,
            (unsigned long) MAGIC_DATA_RO_SECTION_START, (unsigned long) MAGIC_DATA_RO_SECTION_END,
            (unsigned long) MAGIC_TEXT_SECTION_START, (unsigned long) MAGIC_TEXT_SECTION_END,
            (unsigned long) MAGIC_ST_DATA_SECTION_START, (unsigned long) MAGIC_ST_DATA_SECTION_END,
            (unsigned long) MAGIC_ST_DATA_RO_SECTION_START, (unsigned long) MAGIC_ST_DATA_RO_SECTION_END,
            (unsigned long) MAGIC_ST_TEXT_SECTION_START, (unsigned long) MAGIC_ST_TEXT_SECTION_END);
}

/*===========================================================================*
 *                   magic_mempool_sentry_lookup_by_addr                     *
 *===========================================================================*/
PUBLIC struct _magic_sentry* magic_mempool_sentry_lookup_by_range(void *addr, struct _magic_dsentry *dsentry_buff)
{
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;
    struct _magic_sentry* entry = NULL;
    void *start_address, *end_address;

    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_MEMPOOL_ALIVE_ITER(_magic_first_mempool_dsentry, prev_dsentry, dsentry, sentry,
        start_address = sentry->address;
        end_address = (void*) (((char*)sentry->address) + sentry->type->size - 1);
        if(MAGIC_ADDR_IS_WITHIN(addr, start_address, end_address)) {
            if(dsentry_buff) {
                magic_copy_dsentry(dsentry, dsentry_buff);
                entry = MAGIC_DSENTRY_TO_SENTRY(dsentry_buff);
            }
            else {
                entry = sentry;
            }
            break;
        }
    );
    MAGIC_DSENTRY_UNLOCK();

    return entry;
}

/*===========================================================================*
 *                   magic_dsindex_lookup_by_name                            *
 *===========================================================================*/
PUBLIC struct _magic_dsindex*
magic_dsindex_lookup_by_name(const char *parent_name, const char *name)
{
    int i;
    struct _magic_dsindex* index = NULL;
    assert(parent_name && name);

    /* Scan all the indexes and return the one matching the provided names. */
    for(i=0;i<_magic_dsindexes_num;i++) {
        if(!strcmp(_magic_dsindexes[i].parent_name, parent_name)
            && !strcmp(_magic_dsindexes[i].name, name)) {
            index = &_magic_dsindexes[i];
            break;
        }
    }

    return index;
}

/*===========================================================================*
 *                          magic_dsentry_prev_lookup                        *
 *===========================================================================*/
PUBLIC struct _magic_dsentry* magic_dsentry_prev_lookup(struct _magic_dsentry* dsentry)
{
    struct _magic_dsentry *prev_dsentry, *it_dsentry;
    struct _magic_sentry *sentry;
    int found = 0;

#if MAGIC_DSENTRY_ALLOW_PREV
    return dsentry->prev;
#else
    MAGIC_DSENTRY_ITER(_magic_first_dsentry, prev_dsentry, it_dsentry, sentry,
        if(dsentry == it_dsentry) {
            found = 1;
            break;
        }
    );
    if(!found) {
        return (struct _magic_dsentry*) MAGIC_ENOPTR;
    }
    return prev_dsentry;
#endif
}

/*===========================================================================*
 *                      magic_mempool_dsentry_prev_lookup                    *
 *===========================================================================*/
PUBLIC struct _magic_dsentry* magic_mempool_dsentry_prev_lookup(struct _magic_dsentry* dsentry)
{
    struct _magic_dsentry *prev_dsentry, *it_dsentry;
    struct _magic_sentry *sentry;
    int found = 0;

    MAGIC_DSENTRY_MEMPOOL_ITER(_magic_first_mempool_dsentry, prev_dsentry, it_dsentry, sentry,
        if(dsentry == it_dsentry) {
            found = 1;
            break;
        }
    );
    if(!found) {
        return (struct _magic_dsentry*) MAGIC_ENOPTR;
    }
    return prev_dsentry;
}

/*===========================================================================*
 *                         magic_function_lookup_by_id                       *
 *===========================================================================*/
PUBLIC struct _magic_function* magic_function_lookup_by_id(_magic_id_t id,
    struct _magic_dfunction *dfunction_buff)
{
    struct _magic_function* entry = NULL;
    struct _magic_function* function;
    struct _magic_dfunction* dfunction;

    if(id <= 0) {
        return NULL;
    }

    /* O(1) ID lookup for functions. */
#if MAGIC_LOOKUP_FUNCTION
    if((int)id <= _magic_functions_num) {
        return &_magic_functions[id - 1];
    }
#endif

    /* O(N) ID lookup for dfunctions. */
#if MAGIC_LOOKUP_DFUNCTION
    MAGIC_DFUNCTION_LOCK();
    MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
        if(function->id == id) {
            if(dfunction_buff) {
                magic_copy_dfunction(dfunction, dfunction_buff);
                entry = MAGIC_DFUNCTION_TO_FUNCTION(dfunction_buff);
            }
            else {
                entry = function;
            }
            break;
        }
    );
    MAGIC_DFUNCTION_UNLOCK();
#endif

    return entry;
}

/*===========================================================================*
 *                        magic_function_lookup_by_addr                      *
 *===========================================================================*/
PUBLIC struct _magic_function* magic_function_lookup_by_addr(void *addr,
    struct _magic_dfunction *dfunction_buff)
{
    int i;
    struct _magic_function *entry = NULL;
    struct _magic_function *function;
    struct _magic_dfunction *dfunction;

#if MAGIC_LOOKUP_FUNCTION_ALLOW_ADDR_HASH
    if (magic_function_hash_head) {
        return magic_function_lookup_by_addr_hash(addr, dfunction_buff);
    }
#endif

    /* Scan all the entries and return the one matching the provided address. */
#if MAGIC_LOOKUP_FUNCTION
    if (MAGIC_ADDR_IS_IN_RANGE(addr, magic_function_range)) {
        for (i = 0 ; i < _magic_functions_num ; i++) {
            if (_magic_functions[i].address == addr) {
                entry = &_magic_functions[i];
                break;
            }
        }
    }
#endif

#if MAGIC_LOOKUP_DFUNCTION
    MAGIC_DFUNCTION_LOCK();
    if(!MAGIC_ADDR_LOOKUP_USE_DFUNCTION_RANGES || magic_range_is_dfunction(addr)) {
        MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
            if(function->address == addr) {
                if(dfunction_buff) {
                    magic_copy_dfunction(dfunction, dfunction_buff);
                    entry = MAGIC_DFUNCTION_TO_FUNCTION(dfunction_buff);
                }
                else {
                    entry = function;
                }
                break;
            }
        );
    }
    MAGIC_DFUNCTION_UNLOCK();
#endif

    return entry;
}

/*===========================================================================*
 *                       magic_function_lookup_by_name                       *
 *===========================================================================*/
PUBLIC struct _magic_function*
magic_function_lookup_by_name(const char *parent_name, const char *name)
{
    int i;
    struct _magic_function* entry = NULL;

    /* Scan all the entries and return the one matching the provided name(s). */
#if MAGIC_LOOKUP_FUNCTION
    for (i = 0 ; i < _magic_functions_num ; i++) {
        if (!strcmp(_magic_functions[i].name, name)) {
            if (!parent_name || !strcmp(MAGIC_FUNCTION_PARENT(&_magic_functions[i]), parent_name)) {
                entry = &_magic_functions[i];
                break;
            }
        }
    }
#endif

    return entry;
}

/*===========================================================================*
 *                         magic_function_hash_insert                        *
 *===========================================================================*/
PRIVATE void magic_function_hash_insert(struct _magic_function_hash **head,
    struct _magic_function_hash *elem)
{
    if (head != NULL) {
        struct _magic_function_hash *tmp;
        HASH_FIND_PTR(*head, &elem->key, tmp);
        /* Skip inserting this function if an identical one already exists. */
        if (tmp)
            return;
    }
/*
 * **** START UTHASH SPECIFIC DEFINITIONS ****
 */
#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(size)             magic_function_hash_alloc(size)
#define uthash_free(addr, size)         magic_function_hash_dealloc(addr, size)
/*
 * Since we have a limited buffer, we need to stop bucket expansion when
 * reaching a certain limit.
 */
#undef uthash_expand_fyi
#define uthash_expand_fyi(tbl)                                                 \
    do {                                                                       \
        if (tbl->num_buckets == MAGIC_FUNCTION_ADDR_EST_MAX_BUCKETS) {         \
            _magic_printf("Warning! Function address hash maximum bucket "     \
                "number reached! Consider increasing "                         \
                "MAGIC_FUNCTION_ADDR_EST_MAX_BUCKETS, unless you are "         \
                "comfortable with the current performance.\n");                \
            tbl->noexpand = 1;                                                 \
        }                                                                      \
    } while(0);
/*
 * **** FINISH UTHASH SPECIFIC DEFINITIONS ****
 */
    HASH_ADD_PTR(*head, key, elem);
/*
 * **** START UTHASH DEFINITION REMOVAL ****
 */
#undef uthash_malloc
#undef uthash_free
#undef uthash_expand_fyi
/*
 * **** FINISH UTHASH DEFINITION REMOVAL ****
 */
}

/*===========================================================================*
 *                         magic_function_hash_build                         *
 *===========================================================================*/
PUBLIC void magic_function_hash_build(void *buff, size_t buff_size)
{
    /*
     * XXX:
     * Warning: this implementation is thread unsafe and also makes
     * magic_function_lookup_by_addr thread unsafe!
     */
    int i;
    struct _magic_dfunction *dfunction;
    struct _magic_function *function;
    struct _magic_function_hash *function_hash, *head;

    assert(buff && buff_size > 0);
    magic_function_hash_buff = buff;
    magic_function_hash_buff_offset = 0;
    magic_function_hash_buff_size = buff_size;

    head = NULL;

    /* Add all the functions to the hash. */
#if MAGIC_LOOKUP_FUNCTION
    for(i = 0 ; i < _magic_functions_num ; i++) {
        function_hash = (struct _magic_function_hash *)
            magic_function_hash_alloc(sizeof(struct _magic_function_hash));
        function = &_magic_functions[i];
        MAGIC_FUNCTION_TO_HASH_EL(function, function_hash);
        magic_function_hash_insert(&head, function_hash);
    }
#endif

    /* Add all the dfunctions to the hash. */
#if MAGIC_LOOKUP_DFUNCTION
    MAGIC_DFUNCTION_LOCK();
    MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
        function_hash = (struct _magic_function_hash *)
            magic_function_hash_alloc(sizeof(struct _magic_function_hash));
        MAGIC_DFUNCTION_TO_HASH_EL(dfunction, function, function_hash);
        magic_function_hash_insert(&head, function_hash);
    );
    MAGIC_DFUNCTION_UNLOCK();
#endif
    magic_function_hash_head = (void *)head;
    assert(magic_function_hash_head);
}

/*===========================================================================*
 *                        magic_function_hash_destroy                        *
 *===========================================================================*/
PUBLIC void magic_function_hash_destroy(void)
{
    magic_function_hash_buff = NULL;
    magic_function_hash_buff_offset = 0;
    magic_function_hash_buff_size = 0;
    magic_function_hash_head = NULL;
}

/*===========================================================================*
 *                    magic_function_hash_estimate_buff_size                 *
 *===========================================================================*/
PUBLIC size_t magic_function_hash_estimate_buff_size(int functions_num)
{
    if (functions_num == 0) {
        functions_num = _magic_dfunctions_num;
        functions_num += _magic_functions_num;
    }

    return (functions_num * sizeof(struct _magic_function_hash)) +
        MAGIC_FUNCTION_ADDR_HASH_OVERHEAD;
}

/*===========================================================================*
 *                           magic_function_hash_alloc                       *
 *===========================================================================*/
PUBLIC void* magic_function_hash_alloc(size_t size)
{
    void *addr;

    assert(magic_function_hash_buff);
    assert(magic_function_hash_buff_offset + size <=
        magic_function_hash_buff_size);

    addr = (char*) magic_function_hash_buff + magic_function_hash_buff_offset;
    magic_function_hash_buff_offset += size;

    return addr;
}

/*===========================================================================*
 *                          magic_function_hash_dealloc                      *
 *===========================================================================*/
PUBLIC void magic_function_hash_dealloc(UNUSED(void *object), UNUSED(size_t sz))
{
    return;
}

/*===========================================================================*
 *                      magic_function_lookup_by_addr_hash                   *
 *===========================================================================*/
PUBLIC struct _magic_function *magic_function_lookup_by_addr_hash(
    void *addr, struct _magic_dfunction *dfunction_buff)
{
    /*
     * Warning: this implementation is thread unsafe!
     */
    struct _magic_function_hash *res, *head;
    head = (struct _magic_function_hash *) magic_function_hash_head;

    HASH_FIND_PTR(head, &addr, res);
    if (res == NULL)
        return NULL;

    if (MAGIC_STATE_FLAG(res->function, MAGIC_STATE_DYNAMIC) &&
        dfunction_buff != NULL) {
        magic_copy_dfunction(MAGIC_DFUNCTION_FROM_FUNCTION(res->function),
            dfunction_buff);
    }

    return res->function;
}

/*===========================================================================*
 *                         magic_type_lookup_by_name                         *
 *===========================================================================*/
PUBLIC struct _magic_type* magic_type_lookup_by_name(const char *name)
{
    int i;
    unsigned int j;
    struct _magic_type* entry = NULL;

    /* Scan all the entries and return the one matching the provided name. */
#if MAGIC_LOOKUP_TYPE
    for (i = 0 ; i < _magic_types_num ; i++) {
        if (!strcmp(_magic_types[i].name, name)) {
            entry = &_magic_types[i];
            break;
        }
        if (_magic_types[i].names) {
            for (j = 0 ; j < _magic_types[i].num_names ; j++) {
                if (!strcmp(_magic_types[i].names[j], name)) {
                    entry = &_magic_types[i];
                    break;
                }
            }
            if (entry) {
                break;
            }
        }
    }
#endif

    return entry;
}

/*===========================================================================*
 *                      magic_dsodesc_lookup_by_handle                       *
 *===========================================================================*/
PUBLIC struct _magic_dsodesc* magic_dsodesc_lookup_by_handle(void *handle)
{
    struct _magic_dsodesc* desc = NULL;
    struct _magic_dsodesc* dsodesc;

    /*
     * Scan all the descriptors and return the one matching the provided handle.
     * Note that there is no locking here. The caller has to explicitely call
     * MAGIC_DSODESC_LOCK/UNLOCK before/after invoking this function.
     */
    MAGIC_DSODESC_ITER(_magic_first_dsodesc, dsodesc,
        if(dsodesc->handle == handle) {
            desc = dsodesc;
            break;
        }
    );

    return desc;
}

/*===========================================================================*
 *                         magic_print_function                              *
 *===========================================================================*/
PUBLIC void magic_print_function(struct _magic_function *function)
{
    MAGIC_FUNCTION_PRINT(function, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                         magic_print_functions                             *
 *===========================================================================*/
PUBLIC void magic_print_functions()
{
    int i;
    struct _magic_function* function;

    _magic_printf("magic_print_functions: Printing %d entries\n", _magic_functions_num);
    for(i=0;i<_magic_functions_num;i++) {
        function = &_magic_functions[i];
        MAGIC_FUNCTION_PRINT(function, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }
}

/*===========================================================================*
 *                           magic_print_type                                *
 *===========================================================================*/
PUBLIC void magic_print_type(const struct _magic_type* type)
{
    MAGIC_TYPE_PRINT(type, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                           magic_print_types                               *
 *===========================================================================*/
PUBLIC void magic_print_types()
{
    int i;
    struct _magic_type* type;

    _magic_printf("magic_print_types: Printing %d types\n", _magic_types_num);
    for(i=0;i<_magic_types_num;i++) {
        type = &_magic_types[i];
        MAGIC_TYPE_PRINT(type, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }
}

/*===========================================================================*
 *                      magic_type_str_set_print_style                       *
 *===========================================================================*/
PUBLIC void magic_type_str_set_print_style(const int style)
{
    magic_type_str_print_style = style;
}

/*===========================================================================*
 *                      magic_type_str_get_print_style                       *
 *===========================================================================*/
PUBLIC int magic_type_str_get_print_style()
{
    return magic_type_str_print_style;
}

/*===========================================================================*
 *                       magic_type_get_nesting_level                        *
 *===========================================================================*/
PRIVATE INLINE int magic_type_get_nesting_level(const struct _magic_type* type,
    int level)
{
    int i;
    int nesting_level = -1;

    for(i=0;i<level;i++) {
        if(magic_nested_types[i] == type) {
            nesting_level = i;
            break;
        }
    }

    return nesting_level;
}

/*===========================================================================*
 *                            magic_type_str_print                           *
 *===========================================================================*/
PUBLIC void magic_type_str_print(const struct _magic_type* type)
{
    int num_contained_types;
    int is_empty_str = !type->type_str || !strcmp(type->type_str, "");
    int type_has_name = type->name && strcmp(type->name, "");
    int print_multi_names = (magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_MULTI_NAMES) && MAGIC_TYPE_HAS_MULTI_NAMES(type);
    int print_ptr_name = (magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_MULTI_NAMES) && !MAGIC_TYPE_HAS_MULTI_NAMES(type) && type_has_name;
    assert(magic_level < MAGIC_MAX_RECURSIVE_TYPES);

    if(magic_level == 0) {
        magic_counter = 0;
    }
    else if(magic_counter >= MAGIC_TYPE_STR_PRINT_MAX) {
        _magic_printf("%%");
        return;
    }
    else if(magic_level >= MAGIC_TYPE_STR_PRINT_MAX_LEVEL) {
        _magic_printf("%%");
        return;
    }

    if(MAGIC_TYPE_STR_PRINT_DEBUG) {
        _magic_printf("Entering level %d...\n", magic_level);
    }

    if(type->type_id == MAGIC_TYPE_OPAQUE) {
        _magic_printf("opaque");
        magic_counter += strlen("opaque");
        return;
    }

    num_contained_types = MAGIC_TYPE_NUM_CONTAINED_TYPES(type);
    if(num_contained_types == 0) {
        assert(!is_empty_str);
        if((magic_type_str_print_style & (MAGIC_TYPE_STR_PRINT_LLVM_TYPES|MAGIC_TYPE_STR_PRINT_SOURCE_TYPES)) == (MAGIC_TYPE_STR_PRINT_LLVM_TYPES|MAGIC_TYPE_STR_PRINT_SOURCE_TYPES)) {
            if(print_multi_names) {
                _magic_printf("%s/", type->type_str);
                magic_type_names_print(type);
                magic_counter += strlen(type->type_str)+1+strlen(type->name)*type->num_names;
            }
            else {
                _magic_printf("%s/%s", type->type_str, type->name);
                magic_counter += strlen(type->type_str)+1+strlen(type->name);
            }
        }
        else if(magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_LLVM_TYPES) {
            _magic_printf(type->type_str);
            magic_counter += strlen(type->type_str);
        }
        else if(magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_SOURCE_TYPES) {
            if(print_multi_names) {
                magic_type_names_print(type);
                magic_counter += strlen(type->name)*type->num_names;
            }
            else {
                _magic_printf(type->name);
                magic_counter += strlen(type->name);
            }
        }
        return;
    }

    if(type->type_id == MAGIC_TYPE_POINTER) {
        int nesting_level = magic_type_get_nesting_level(type, magic_level);
        if(nesting_level >= 0) {
            _magic_printf("\\%d", magic_level - nesting_level);
            magic_counter += 2;
            return;
        }
    }

    magic_nested_types[magic_level] = type;
    magic_level++;
    if(type->type_id == MAGIC_TYPE_POINTER) {
        magic_type_str_print(type->contained_types[0]);
        _magic_printf("*");
        magic_counter += 1;
        if(print_multi_names) {
            _magic_printf("|");
            magic_type_names_print(type);
            magic_counter += 1+strlen(type->name)*type->num_names;
        }
        else if(print_ptr_name) {
            _magic_printf("|");
            _magic_printf("%s", type->name);
            magic_counter += 1+strlen(type->name);
        }
    }
    else if(type->type_id == MAGIC_TYPE_ARRAY || type->type_id == MAGIC_TYPE_VECTOR) {
        int num_elements = type->num_child_types;
        char start_sep = type->type_id == MAGIC_TYPE_ARRAY ? '[' : '<';
        char end_sep = type->type_id == MAGIC_TYPE_ARRAY ? ']' : '>';
        _magic_printf("%c", start_sep);
        magic_counter += 1;
        if(MAGIC_TYPE_FLAG(type, MAGIC_TYPE_VARSIZE)) {
            _magic_printf(" (V) ");
            magic_counter += 5;
        }
        if(num_elements) {
            _magic_printf("%d x ", num_elements);
            magic_counter += 5;
        }
        magic_type_str_print(type->contained_types[0]);
        _magic_printf("%c", end_sep);
        magic_counter += 1;
    }
    else if(type->type_id == MAGIC_TYPE_STRUCT || type->type_id == MAGIC_TYPE_UNION) {
        int i;
        int skip_struct = type->type_id == MAGIC_TYPE_STRUCT && (magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_SKIP_STRUCTS);
        int skip_union = type->type_id == MAGIC_TYPE_UNION && (magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_SKIP_UNIONS);
        _magic_printf("{ ");
        magic_counter += 2;
        if(type->type_id == MAGIC_TYPE_UNION) {
            _magic_printf("(U) ");
            magic_counter += 4;
        }
        if(print_multi_names) {
            _magic_printf("$");
            magic_type_names_print(type);
            _magic_printf(" ");
            magic_counter += 2 + strlen(type->name)*type->num_names;
        }
        else {
            _magic_printf("$%s ", strcmp(type->name, "") ? type->name : "ANONYMOUS");
            magic_counter += 2 + strlen(strcmp(type->name, "") ? type->name : "ANONYMOUS");
        }
        assert(type->member_names);
        if(!skip_struct && !skip_union) {
            for(i=0;i<num_contained_types;i++) {
                if(i > 0) {
                    _magic_printf(", ");
                    magic_counter += 2;
                }
                if((magic_type_str_print_style & MAGIC_TYPE_STR_PRINT_MEMBER_NAMES)
                    && (!MAGIC_TYPE_STR_PRINT_MAX || magic_counter < MAGIC_TYPE_STR_PRINT_MAX)) {
                    assert(type->member_names[i] && strcmp(type->member_names[i], "") && "Invalid member name!");
                    _magic_printf("%s ", type->member_names[i]);
                    magic_counter += strlen(type->member_names[i])+1;
                }
                magic_type_str_print(type->contained_types[i]);
            }
        }
        _magic_printf(" }");
        magic_counter += 2;
   }
   else if(type->type_id == MAGIC_TYPE_FUNCTION) {
       int i;
       assert(num_contained_types > 0);
       magic_type_str_print(type->contained_types[0]);
       num_contained_types--;
       _magic_printf(" (");
       magic_counter += 2;
       for(i=0;i<num_contained_types;i++) {
           if(i > 0) {
               _magic_printf(", ");
               magic_counter += 2;
           }
           magic_type_str_print(type->contained_types[i+1]);
       }
       _magic_printf(")");
       magic_counter += 1;
   }
   else {
       _magic_printf("???[id=%d,child_types=%d,size=%zu]", type->type_id, type->num_child_types, type->size);
       magic_counter += 30;
   }
   magic_level--;
   if(MAGIC_TYPE_STR_PRINT_DEBUG) {
       _magic_printf("Exiting level %d...\n", magic_level);
   }
}

/*===========================================================================*
 *                         magic_type_values_print                           *
 *===========================================================================*/
PUBLIC void magic_type_values_print(const struct _magic_type* type)
{
    int i=0;

    if(!MAGIC_TYPE_HAS_VALUE_SET(type)) {
        return;
    }
    while(MAGIC_TYPE_HAS_VALUE(type, i)) {
        int value = MAGIC_TYPE_VALUE(type, i);
        _magic_printf("%s%d", (i==0 ? "" : ", "), value);
        i++;
    }
}

/*===========================================================================*
 *                         magic_type_names_print                            *
 *===========================================================================*/
PUBLIC void magic_type_names_print(const struct _magic_type* type)
{
    unsigned int i;

    for(i=0;i<type->num_names;i++) {
        _magic_printf("%s%s", (i==0 ? "" : "|"), type->names[i]);
    }
}

/*===========================================================================*
 *                       magic_type_comp_types_print                         *
 *===========================================================================*/
PUBLIC void magic_type_comp_types_print(const struct _magic_type* type,
    int flags)
{
    int num;
    int i=0;
    const struct _magic_type* comp_type;

    if(!MAGIC_TYPE_HAS_COMP_TYPES(type)) {
        return;
    }
    MAGIC_TYPE_NUM_COMP_TYPES(type, &num);
    _magic_printf("#%d", num);
    if(flags & MAGIC_SKIP_COMP_TYPES) {
        return;
    }
    flags &= ~MAGIC_EXPAND_TYPE_STR;

    MAGIC_TYPE_COMP_ITER(type, comp_type,
        _magic_printf("%s%d=", (i==0 ? ": " : ", "), i+1);
        MAGIC_TYPE_PRINT(comp_type, flags|MAGIC_SKIP_COMP_TYPES);
        i++;
    );
}

/*===========================================================================*
 *                     magic_type_str_print_from_target                      *
 *===========================================================================*/
PUBLIC int magic_type_str_print_from_target(void* target)
{
    int printed_types=0;
    int ret;
    ret = magic_type_target_walk(target, NULL, NULL,
        magic_type_str_print_cb, &printed_types);
    if(ret < 0) {
        return ret;
    }
    if(printed_types == 0) {
        _magic_printf("BAD OFFSET");
    }
    return printed_types;
}

/*===========================================================================*
 *                            magic_type_equals                              *
 *===========================================================================*/
PUBLIC int magic_type_equals(const struct _magic_type* type, const struct _magic_type* other_type)
{
    assert(magic_level < MAGIC_MAX_RECURSIVE_TYPES);

    if(type == other_type) {
        return TRUE;
    }
    if(type->type_id != other_type->type_id) {
        return FALSE;
    }
    if((type->flags & MAGIC_TYPE_EXTERNAL) || (other_type->flags & MAGIC_TYPE_EXTERNAL)) {
        int i, nesting_level;
        if(type->num_child_types == other_type->num_child_types) {
            int num_contained_types = MAGIC_TYPE_NUM_CONTAINED_TYPES(type);
            if(num_contained_types == 0) {
                return !strcmp(type->type_str, other_type->type_str);
            }
            nesting_level = magic_type_get_nesting_level(type, magic_level);
            if(nesting_level >= 0) {
                return (other_type == magic_nested_types2[nesting_level]);
            }
            magic_nested_types[magic_level] = type;
            magic_nested_types2[magic_level] = other_type;
            magic_level++;
            for(i=0;i<num_contained_types;i++) {
                if(magic_type_equals(type->contained_types[i], other_type->contained_types[i]) == FALSE) {
                    magic_level--;
                    return FALSE;
                }
            }
            magic_level--;
            return TRUE;
        }
    }
    return FALSE;
}

/*===========================================================================*
 *                           magic_type_compatible                           *
 *===========================================================================*/
PUBLIC int magic_type_compatible(const struct _magic_type* type, const struct _magic_type* other_type, int flags)
{
    int i, nesting_level, num_contained_types;
    assert(magic_level < MAGIC_MAX_RECURSIVE_TYPES);

    if(type == other_type) {
        return TRUE;
    }

    if(type->type_id != other_type->type_id) {
        return FALSE;
    }

    if(type->num_child_types != other_type->num_child_types) {
        return FALSE;
    }

    if(flags & MAGIC_TYPE_COMPARE_FLAGS) {
        if((type->flags & (~MAGIC_TYPE_IS_ROOT)) != (other_type->flags & (~MAGIC_TYPE_IS_ROOT))){
            return FALSE;
        }
    }

    if(flags & MAGIC_TYPE_COMPARE_VALUE_SET) {
        if(MAGIC_TYPE_HAS_VALUE_SET(type) != MAGIC_TYPE_HAS_VALUE_SET(other_type)){
            return FALSE;
        }
        if(MAGIC_TYPE_HAS_VALUE_SET(type)){
            i=0;
            while(MAGIC_TYPE_HAS_VALUE(type, i) && MAGIC_TYPE_HAS_VALUE(other_type, i)) {
                if(MAGIC_TYPE_VALUE(type, i) != MAGIC_TYPE_VALUE(other_type, i)){
                    /* a value is different */
                    return FALSE;
                }
                i++;
            }
            if(MAGIC_TYPE_HAS_VALUE(type, i) || MAGIC_TYPE_HAS_VALUE(other_type, i)) {
                return FALSE;
            }
        }
    }

    if(flags & MAGIC_TYPE_COMPARE_NAME) {
        if(strcmp(type->name, other_type->name)){
            return FALSE;
        }
    }

    if(flags & MAGIC_TYPE_COMPARE_NAMES) {
        if(type->num_names != other_type->num_names) {
            return FALSE;
        }
        if(type->num_names > 1) {
            for(i=0; (unsigned int)i<type->num_names; i++){
                if(strcmp(type->names[i], other_type->names[i])) {
                    return FALSE;
                }
            }
        }
    }

    num_contained_types = MAGIC_TYPE_NUM_CONTAINED_TYPES(type);
    if(num_contained_types == 0) {
        return type->size == other_type->size && !strcmp(type->type_str, other_type->type_str);
    }

    if(type->type_id == MAGIC_TYPE_STRUCT) {
        if(flags & MAGIC_TYPE_COMPARE_MEMBER_NAMES) {
            for(i=0; (unsigned int)i<type->num_child_types; i++){
                if(strcmp(type->member_names[i], other_type->member_names[i])) {
                    return FALSE;
                }
            }
        }
        if(flags & MAGIC_TYPE_COMPARE_MEMBER_OFFSETS) {
            for(i=0; (unsigned int)i<type->num_child_types; i++){
                if(type->member_offsets[i] != other_type->member_offsets[i]) {
                    return FALSE;
                }
            }
        }
    }

    nesting_level = magic_type_get_nesting_level(type, magic_level);
    if(nesting_level >= 0) {
        return (other_type == magic_nested_types2[nesting_level]);
    }
    magic_nested_types[magic_level] = type;
    magic_nested_types2[magic_level] = other_type;
    magic_level++;
    for(i=0;i<num_contained_types;i++) {
        if(!magic_type_compatible(type->contained_types[i], other_type->contained_types[i], flags)) {
            magic_level--;
            return FALSE;
        }
    }
    magic_level--;
    return TRUE;
}

/*===========================================================================*
 *                        magic_type_comp_compatible                         *
 *===========================================================================*/
PUBLIC int magic_type_comp_compatible(const struct _magic_type* type, const struct _magic_type* other_type)
{
    const struct _magic_type *comp_type;

    MAGIC_TYPE_COMP_ITER(type, comp_type,
        if(magic_type_compatible(comp_type, other_type, 0)) {
             return TRUE;
        }
    );

    return FALSE;
}

/*===========================================================================*
 *                          magic_type_ptr_is_text                           *
 *===========================================================================*/
PUBLIC int magic_type_ptr_is_text(const struct _magic_type* ptr_type)
{
    const struct _magic_type *comp_type;

    assert(ptr_type->type_id == MAGIC_TYPE_POINTER);
    if(ptr_type->contained_types[0]->type_id == MAGIC_TYPE_FUNCTION
        || MAGIC_TYPE_IS_VOID(ptr_type->contained_types[0])) {
        return TRUE;
    }

    MAGIC_TYPE_COMP_ITER(ptr_type, comp_type,
        if(comp_type->type_id == MAGIC_TYPE_FUNCTION
            || MAGIC_TYPE_IS_VOID(comp_type)) {
            return TRUE;
        }
    );

    return FALSE;
}

/*===========================================================================*
 *                          magic_type_ptr_is_data                           *
 *===========================================================================*/
PUBLIC int magic_type_ptr_is_data(const struct _magic_type* ptr_type)
{
    const struct _magic_type *comp_type;

    assert(ptr_type->type_id == MAGIC_TYPE_POINTER);
    if(ptr_type->contained_types[0]->type_id != MAGIC_TYPE_FUNCTION) {
        return TRUE;
    }

    MAGIC_TYPE_COMP_ITER(ptr_type, comp_type,
        if(comp_type->type_id != MAGIC_TYPE_FUNCTION) {
            return TRUE;
        }
    );

    return FALSE;
}

/*===========================================================================*
 *                    magic_type_alloc_needs_varsized_array                  *
 *===========================================================================*/
PUBLIC int magic_type_alloc_needs_varsized_array(const struct _magic_type* type,
    size_t alloc_size, int *num_elements)
{
    /* See if this type needs a var-sized array for the given allocation size */
    const struct _magic_type *array_type, *array_el_type;
    size_t array_offset, array_size;
    if(!MAGIC_TYPE_FLAG(type, MAGIC_TYPE_VARSIZE)) {
        return FALSE;
    }
    assert(type->type_id == MAGIC_TYPE_STRUCT);

    if(alloc_size <= type->size || type->num_child_types == 0) {
        return FALSE;
    }
    array_type = type->contained_types[type->num_child_types-1];
    if(array_type->type_id != MAGIC_TYPE_ARRAY) {
        return FALSE;
    }
    array_el_type = array_type->contained_types[0];
    array_offset = type->member_offsets[type->num_child_types-1]+array_type->num_child_types*array_el_type->size;
    array_size = alloc_size - array_offset;
    if(array_size == 0 || array_size % array_el_type->size != 0) {
        return FALSE;
    }
    if(num_elements) {
        *num_elements = 1+array_size/array_el_type->size;
    }

    return TRUE;
}

/*===========================================================================*
 *                     magic_type_alloc_get_varsized_array_size              *
 *===========================================================================*/
PUBLIC size_t magic_type_alloc_get_varsized_array_size(const struct _magic_type* type,
    int num_elements)
{
    /* Get the allocation size from the number of elements of the varsized array. */
    const struct _magic_type *array_type, *array_el_type;
    size_t array_offset;
    if(!MAGIC_TYPE_FLAG(type, MAGIC_TYPE_VARSIZE)) {
        return 0;
    }
    assert(type->type_id == MAGIC_TYPE_STRUCT);

    if(num_elements == 1) {
        return type->size;
    }
    array_type = type->contained_types[type->num_child_types-1];
    if(array_type->type_id != MAGIC_TYPE_ARRAY) {
        return 0;
    }
    array_el_type = array_type->contained_types[0];
    array_offset = type->member_offsets[type->num_child_types-1]+array_type->num_child_types*array_el_type->size;
    return array_offset + array_el_type->size*(num_elements-1);
}

/*===========================================================================*
 *                      magic_type_parse_varsized_array                      *
 *===========================================================================*/
PUBLIC void magic_type_parse_varsized_array(const struct _magic_type *type,
    const struct _magic_type **sub_struct_type, const struct _magic_type **sub_array_type,
    size_t *sub_array_offset, size_t *sub_array_size)
{
    /* Parse a var-sized array containing a variable-sized struct. */
    const struct _magic_type *_sub_struct_type, *_sub_array_type, *_sub_array_el_type;
    size_t _sub_array_offset, _sub_array_size;

    assert(type->type_id == MAGIC_TYPE_ARRAY && MAGIC_TYPE_FLAG(type, MAGIC_TYPE_DYNAMIC));
    _sub_struct_type = type->contained_types[0];
    assert(magic_type_alloc_needs_varsized_array(_sub_struct_type, type->size, NULL));

    _sub_array_type = _sub_struct_type->contained_types[_sub_struct_type->num_child_types-1];
    _sub_array_el_type = _sub_array_type->contained_types[0];
    _sub_array_offset = _sub_struct_type->member_offsets[_sub_struct_type->num_child_types-1]+_sub_array_type->num_child_types*_sub_array_el_type->size;
    _sub_array_size = type->size - _sub_array_offset;

    if(sub_struct_type) *sub_struct_type = _sub_struct_type;
    if(sub_array_type) *sub_array_type = _sub_array_type;
    if(sub_array_offset) *sub_array_offset = _sub_array_offset;
    if(sub_array_size) *sub_array_size = _sub_array_size;
}

/*===========================================================================*
 *                          magic_type_walk_flags                            *
 *===========================================================================*/
PUBLIC int magic_type_walk_flags(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num,
    const struct _magic_type* type, unsigned offset,
    const unsigned min_offset, const unsigned max_offset,
    const magic_type_walk_cb_t cb, void* cb_args, int flags) {
    static THREAD_LOCAL int magic_depth = 0;
    int ret, status, action;
    ret = MAGIC_TYPE_WALK_CONTINUE;

    if(offset >= min_offset && offset <= max_offset) {
         ret = cb(parent_type, parent_offset, child_num, type, offset, magic_depth, cb_args);
    }
    else if(offset > max_offset) {
        ret = MAGIC_TYPE_WALK_STOP;
    }
    else if(offset+type->size <= min_offset) {
        ret = MAGIC_TYPE_WALK_SKIP_PATH;
    }

    /* The status code returned to the caller is propagated directly from the
     * callback only in case of ret<0 and ret == MAGIC_TYPE_WALK_STOP.
     * In all the other cases, we return 0 the caller.
     */
    status = ret < 0 ? ret : 0;
    action = ret < 0 ? MAGIC_TYPE_WALK_STOP : ret;
    switch(action) {
        case MAGIC_TYPE_WALK_STOP:
            status = status ? status : MAGIC_TYPE_WALK_STOP;
        break;
        case MAGIC_TYPE_WALK_SKIP_PATH:
            status = 0;
        break;
        case MAGIC_TYPE_WALK_CONTINUE:
            if(!MAGIC_TYPE_IS_WALKABLE(type)) {
                status = 0;
            }
            else {
                int i, num_child_types, start;
                num_child_types = type->num_child_types;
                start = 0;
                if(type->type_id == MAGIC_TYPE_ARRAY || type->type_id == MAGIC_TYPE_VECTOR) {
                    if(!MAGIC_TYPE_FLAG(type, MAGIC_TYPE_VARSIZE) && offset < min_offset) {
                        /* Skip irrelevant array iterations. */
                        start = (min_offset-offset)/(type->contained_types[0]->size);
                    }
                }
                for(i=start;i<num_child_types;i++) {
                    const struct _magic_type *child_type;
                    unsigned child_offset;
                    magic_type_walk_step(type, i, &child_type, &child_offset, flags);
                    magic_depth++;
                    status = magic_type_walk_flags(type, offset, i, child_type, offset+child_offset, min_offset, max_offset, cb, cb_args, flags);
                    magic_depth--;
                    if(status < 0 || status == MAGIC_TYPE_WALK_STOP) {
                        break;
                    }
                }
            }
        break;
        default:
            _magic_printf("magic_type_walk: unrecognized callback return code: %d, stopping type walk...\n", action);
            status = MAGIC_TYPE_WALK_STOP;
        break;
    }
    return status;
}

/*===========================================================================*
 *                          magic_type_target_walk                           *
 *===========================================================================*/
PUBLIC int magic_type_target_walk(void *target,
    struct _magic_dsentry **trg_dsentry, struct _magic_dfunction **trg_dfunction,
    const magic_type_walk_cb_t cb, void *cb_args)
{
    int ret;
    struct _magic_sentry *sentry = NULL;
    struct _magic_function *function = NULL;
    sentry = magic_sentry_lookup_by_range(target, magic_reentrant ? &magic_dsentry_buff : NULL);
    if (sentry == NULL) {
        function = magic_function_lookup_by_addr(target, magic_reentrant ? &magic_dfunction_buff : NULL);
        if (function == NULL) {
            /* No known entry found. */
            return MAGIC_ENOENT;
        }
        if (MAGIC_STATE_FLAG(function, MAGIC_STATE_ADDR_NOT_TAKEN)) {
            /* A function has been found, but it was not supposed to be a target. */
            return MAGIC_EBADENT;
        }
    }
    else if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_ADDR_NOT_TAKEN)) {
        /* An entry has been found, but it was not supposed to be a target. */
        return MAGIC_EBADENT;
    }
    assert(sentry || function);
    if (magic_reentrant) {
        if (sentry) {
            if (trg_dsentry) {
                if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_DYNAMIC)) {
                    magic_copy_dsentry(MAGIC_DSENTRY_FROM_SENTRY(sentry), *trg_dsentry);
                }
                else {
                    memcpy(MAGIC_DSENTRY_TO_SENTRY(*trg_dsentry), sentry, sizeof(struct _magic_sentry));
                }
            }
            if (trg_dfunction) {
                *trg_dfunction = NULL;
            }
        }
        else {
            if (trg_dfunction) {
                if (MAGIC_STATE_FLAG(function, MAGIC_STATE_DYNAMIC)) {
                    magic_copy_dfunction(MAGIC_DFUNCTION_FROM_FUNCTION(function), *trg_dfunction);
                }
                else {
                    memcpy(MAGIC_DFUNCTION_TO_FUNCTION(*trg_dfunction), function, sizeof(struct _magic_function));
                }
            }
            if (trg_dsentry) {
                *trg_dsentry = NULL;
            }
        }
    } else {
        /*
         * Just return the pointer to the target object.
         * NB!: Because the target objects can be static (i.e. sentries,
         * functions), the user MUST first check the flag
         * of the returned target element to see if it is a sentry
         * or function. Otherwise, he might end up accessing invalid
         * memory.
         */
        if (sentry) {
            if (trg_dsentry) {
                *trg_dsentry = MAGIC_DSENTRY_FROM_SENTRY(sentry);
            }
            if (trg_dfunction) {
                *trg_dfunction = NULL;
            }
        }
        else {
            if (trg_dfunction) {
                *trg_dfunction = MAGIC_DFUNCTION_FROM_FUNCTION(function);
            }
            if (trg_dsentry) {
                *trg_dsentry = NULL;
            }
        }
    }

    if (sentry) {
        ret = magic_type_walk_root_at_offset(sentry->type, (char *) target - (char *) sentry->address, cb, cb_args);
    } else {
        ret = magic_type_walk_root_at_offset(function->type, (char*) target - (char*) function->address, cb, cb_args);
    }

    return ret;
}

/*===========================================================================*
 *                      magic_type_walk_as_void_array                        *
 *===========================================================================*/
PUBLIC int magic_type_walk_as_void_array(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num, const struct _magic_type* type,
    unsigned offset, const unsigned min_offset, const unsigned max_offset,
    const magic_type_walk_cb_t cb, void* cb_args)
{
    struct _magic_type void_array_type;
    MAGIC_TYPE_VOID_ARRAY_GET_FROM_SIZE(&void_array_type, type->size);
    return magic_type_walk(parent_type, parent_offset, child_num, &void_array_type,
        offset, min_offset, max_offset, cb, cb_args);
}

/*===========================================================================*
 *                     magic_type_walk_as_ptrint_array                       *
 *===========================================================================*/
PUBLIC int magic_type_walk_as_ptrint_array(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num, const struct _magic_type* type, void* offset_addr,
    unsigned offset, const unsigned min_offset, const unsigned max_offset,
    const magic_type_walk_cb_t cb, void* cb_args)
{
    struct _magic_type ptrint_array_type;
    unsigned type_size = type->size;
    unsigned addr_diff = ((unsigned)offset_addr) % sizeof(void*);
    if(addr_diff > 0) {
        unsigned addr_off_by = sizeof(void*) - addr_diff;
        if(type_size <= addr_off_by) {
            return MAGIC_EBADWALK;
        }
        type_size -= addr_off_by;
        offset_addr = (void*)((unsigned)offset_addr + addr_off_by);
        offset += addr_off_by;
    }
    addr_diff = (((unsigned)offset_addr)+type_size) % sizeof(void*);
    if(addr_diff > 0) {
        unsigned addr_off_by = addr_diff;
        if(type_size <= addr_off_by) {
            return MAGIC_EBADWALK;
        }
        type_size -= addr_off_by;
    }
    MAGIC_TYPE_PTRINT_ARRAY_GET_FROM_SIZE(&ptrint_array_type, type_size);
    return magic_type_walk(parent_type, parent_offset, child_num, &ptrint_array_type,
        offset, min_offset, max_offset, cb, cb_args);
}

/*===========================================================================*
 *                         magic_type_str_print_cb                           *
 *===========================================================================*/
PUBLIC int magic_type_str_print_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    int *printed_types = (int*) cb_args;
    if(printed_types) (*printed_types)++;
    magic_type_str_print(type);
    _magic_printf("; ");
    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                            magic_type_count_cb                            *
 *===========================================================================*/
PUBLIC int magic_type_count_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    int *type_counter = (int*) cb_args;
    if(type_counter) (*type_counter)++;
    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                         magic_type_child_offset_cb                        *
 *===========================================================================*/
PUBLIC int magic_type_child_offset_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    void **args_array = (void**) cb_args;
    int *my_child_num = (int*) args_array[0];
    unsigned *child_offset = (unsigned*) args_array[1];

    if(!parent_type) {
        return MAGIC_TYPE_WALK_CONTINUE;
    }
    if(child_num == *my_child_num) {
        *child_offset = offset;
        return MAGIC_TYPE_WALK_STOP;
    }
    return MAGIC_TYPE_WALK_SKIP_PATH;
}

/*===========================================================================*
 *                            magic_type_walk_step                           *
 *===========================================================================*/
PUBLIC void magic_type_walk_step(const struct _magic_type *type,
    int child_num, const struct _magic_type **child_type, unsigned *child_offset,
    int walk_flags)
{
    int type_id;
    struct _magic_type type_buff;
    if(type->type_id == MAGIC_TYPE_UNION && (walk_flags & MAGIC_TYPE_WALK_UNIONS_AS_VOID)) {
        MAGIC_TYPE_VOID_ARRAY_GET_FROM_SIZE(&type_buff, type->size);
        type = &type_buff;
    }
    type_id = type->type_id;
    if(type_id == MAGIC_TYPE_STRUCT || type_id == MAGIC_TYPE_UNION) {
        *child_type = type->contained_types[child_num];
        *child_offset = type->member_offsets[child_num];
    }
    else {
        assert(type_id == MAGIC_TYPE_ARRAY || type_id == MAGIC_TYPE_VECTOR);
        if(MAGIC_TYPE_FLAG(type, MAGIC_TYPE_VARSIZE) && child_num > 0) {
            const struct _magic_type *sub_array_type, *sub_array_el_type;
            size_t sub_array_offset;
            magic_type_parse_varsized_array(type, NULL, &sub_array_type, &sub_array_offset, NULL);
            sub_array_el_type = sub_array_type->contained_types[0];
            *child_type = sub_array_el_type;
            *child_offset = sub_array_offset + (child_num-1)*sub_array_el_type->size;
        }
        else {
            *child_type = type->contained_types[0];
            *child_offset = child_num*((*child_type)->size);
        }
    }
}

/*===========================================================================*
 *                            magic_type_get_size                            *
 *===========================================================================*/
PUBLIC size_t magic_type_get_size(struct _magic_type *type, int flags)
{
    size_t size;
    int i, num_contained_types;

    size = sizeof(type->size) +
           sizeof(type->num_child_types) + sizeof(type->contained_types) +
           sizeof(type->member_offsets) + sizeof(type->type_id) + sizeof(type->flags);
    num_contained_types = MAGIC_TYPE_NUM_CONTAINED_TYPES(type);

    if(num_contained_types > 0) {
        size += sizeof(*(type->contained_types))*num_contained_types;
    }
    if(type->type_id == MAGIC_TYPE_STRUCT) {
        size += sizeof(*(type->member_offsets))*num_contained_types;
        if(flags & MAGIC_SIZE_MEMBER_NAMES) {
            size += sizeof(*(type->member_names))*num_contained_types;
            for(i=0;i<num_contained_types;i++) {
                size += strlen(type->member_names[i])+1;
            }
        }
    }

    if(flags & MAGIC_SIZE_VALUE_SET) {
        if(MAGIC_TYPE_HAS_VALUE_SET(type)) {
            int num;
            MAGIC_TYPE_NUM_VALUES(type, &num);
            size += sizeof(int)+(num+1);
        }
    }
    if(flags & MAGIC_SIZE_TYPE_NAMES) {
        size += sizeof(type->num_names) + sizeof(type->names) + sizeof(*(type->names))*(type->num_names);
        for(i=0;(unsigned int)i<type->num_names;i++) {
            size += strlen(type->names[i])+1;
        }
    }
    if(flags & MAGIC_SIZE_COMP_TYPES) {
        if(MAGIC_TYPE_HAS_COMP_TYPES(type)) {
            int num;
            MAGIC_TYPE_NUM_COMP_TYPES(type, &num);
            size += sizeof(*(type->compatible_types))*num;
        }
    }

    return size;
}

/*===========================================================================*
 *                            magic_types_get_size                           *
 *===========================================================================*/
PUBLIC size_t magic_types_get_size(int flags)
{
    size_t size;
    int i;

    size = 0;
    for(i=0;i<_magic_types_num;i++) {
        size += magic_type_get_size(&_magic_types[i], flags);
    }

    return size;
}

/*===========================================================================*
 *                          magic_function_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_function_get_size(struct _magic_function *function, int flags)
{
    size_t size;

    size = sizeof(function->type) + sizeof(function->flags) + sizeof(function->address);

    if(flags & MAGIC_SIZE_NAMES) {
        size += sizeof(function->name) + strlen(function->name)+1;
    }

    return size;
}

/*===========================================================================*
 *                          magic_functions_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_functions_get_size(int flags)
{
    size_t size;
    int i;

    size = 0;
    for(i=0;i<_magic_functions_num;i++) {
        size += magic_function_get_size(&_magic_functions[i], flags);
    }

    return size;
}

/*===========================================================================*
 *                         magic_dfunctions_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_dfunctions_get_size(int flags)
{
    size_t size;
    struct _magic_dfunction* dfunction;
    struct _magic_function* function;

    size = 0;
    MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
        size += magic_function_get_size(function, flags);
    );

    return size;
}

/*===========================================================================*
 *                           magic_sentry_get_size                           *
 *===========================================================================*/
PUBLIC size_t magic_sentry_get_size(struct _magic_sentry *sentry, int flags)
{
    size_t size;

    size = sizeof(sentry->type) + sizeof(sentry->flags);

    if(MAGIC_SENTRY_IS_DSENTRY(sentry)) {
        struct _magic_dsentry *dsentry = MAGIC_DSENTRY_FROM_SENTRY(sentry);
        if(flags & MAGIC_SIZE_DSENTRY_NAMES) {
            size += sizeof(sentry->name) + strlen(sentry->name)+1;
            if(dsentry->parent_name) {
                size += sizeof(dsentry->parent_name) + strlen(dsentry->parent_name)+1;
            }
        }
        if(sentry->type == &dsentry->type) {
            size += sizeof(dsentry->type.num_child_types);
        }
        size += sizeof(dsentry->next);
    }
    else {
        size += sizeof(sentry->address);
        if(flags & MAGIC_SIZE_NAMES) {
            size += sizeof(sentry->name) + strlen(sentry->name)+1;
        }
    }

    return size;
}

/*===========================================================================*
 *                           magic_sentries_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_sentries_get_size(int flags)
{
    size_t size;
    int i;

    size = 0;
    for(i=0;i<_magic_sentries_num;i++) {
        size += magic_sentry_get_size(&_magic_sentries[i], flags);
    }

    return size;
}

/*===========================================================================*
 *                          magic_dsentries_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_dsentries_get_size(int flags)
{
    size_t size;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;

    size = 0;
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        if(!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND)) {
            size += magic_sentry_get_size(sentry, flags);
        }
    );

    return size;
}

/*===========================================================================*
 *                           magic_dsindex_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_dsindex_get_size(struct _magic_dsindex *dsindex, int flags)
{
    size_t size;

    size = sizeof(dsindex->type) + sizeof(dsindex->flags);

    if(flags & MAGIC_SIZE_DSINDEX_NAMES) {
        size += sizeof(dsindex->parent_name) + strlen(dsindex->parent_name)+1;
        size += sizeof(dsindex->name) + strlen(dsindex->name)+1;
    }

    return size;
}

/*===========================================================================*
 *                          magic_dsindexes_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_dsindexes_get_size(int flags)
{
    size_t size;
    int i;

    size = 0;
    for(i=0;i<_magic_dsindexes_num;i++) {
        size += magic_dsindex_get_size(&_magic_dsindexes[i], flags);
    }

    return size;
}

/*===========================================================================*
 *                           magic_sodesc_get_size                           *
 *===========================================================================*/
PUBLIC size_t magic_sodesc_get_size(struct _magic_sodesc *sodesc, int flags)
{
    return sizeof(struct _magic_sodesc);
}

/*===========================================================================*
 *                           magic_sodescs_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_sodescs_get_size(int flags)
{
    size_t size;
    struct _magic_sodesc* sodesc;

    size = 0;
    MAGIC_SODESC_ITER(_magic_first_sodesc, sodesc,
        size += magic_sodesc_get_size(sodesc, flags);
    );

    return size;
}

/*===========================================================================*
 *                           magic_dsodesc_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_dsodesc_get_size(struct _magic_dsodesc *dsodesc, int flags)
{
    return sizeof(struct _magic_dsodesc);
}

/*===========================================================================*
 *                          magic_dsodescs_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_dsodescs_get_size(int flags)
{
    size_t size;
    struct _magic_dsodesc* dsodesc;

    size = 0;
    MAGIC_DSODESC_ITER(_magic_first_dsodesc, dsodesc,
        size += magic_dsodesc_get_size(dsodesc, flags);
    );

    return size;
}

/*===========================================================================*
 *                          magic_metadata_get_size                          *
 *===========================================================================*/
PUBLIC size_t magic_metadata_get_size(int flags)
{
    size_t size = 0;

    size += magic_types_get_size(flags);
    size += magic_functions_get_size(flags);
    size += magic_dfunctions_get_size(flags);
    size += magic_sentries_get_size(flags);
    size += magic_dsentries_get_size(flags);
    size += magic_dsindexes_get_size(flags);
    size += magic_dsodescs_get_size(flags);

    return size;
}

/*===========================================================================*
 *                       magic_sentries_data_get_size                        *
 *===========================================================================*/
PUBLIC size_t magic_sentries_data_get_size(int flags)
{
    size_t size;
    int i;

    size = 0;
    for(i=0;i<_magic_sentries_num;i++) {
        size += _magic_sentries[i].type->size;
    }

    return size;
}

/*===========================================================================*
 *                       magic_dsentries_data_get_size                       *
 *===========================================================================*/
PUBLIC size_t magic_dsentries_data_get_size(int flags)
{
    size_t size;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;

    size = 0;
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        if(!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND)) {
            size += sentry->type->size;
            if(MAGIC_STATE_FLAG(sentry, MAGIC_STATE_HEAP)) {
                /* Assume a couple of words for malloc header. */
                size += 2*sizeof(void*);
            }
        }
    );

    return size;
}

/*===========================================================================*
 *                         magic_other_data_get_size                         *
 *===========================================================================*/
PUBLIC size_t magic_other_data_get_size(int flags)
{
    size_t size = 0;

    MAGIC_DSENTRY_LOCK();
    magic_range_is_stack(NULL);
    MAGIC_DSENTRY_UNLOCK();
    size += MAGIC_RANGE_SIZE(magic_stack_range);
    size += MAGIC_RANGE_SIZE(magic_text_range);

    return size;
}

/*===========================================================================*
 *                            magic_data_get_size                            *
 *===========================================================================*/
PUBLIC size_t magic_data_get_size(int flags)
{
    size_t size = 0;

    size += magic_sentries_data_get_size(flags);
    size += magic_dsentries_data_get_size(flags);
    size += magic_other_data_get_size(flags);

    return size;
}

/*===========================================================================*
 *                           magic_print_size_stats                          *
 *===========================================================================*/
PUBLIC void magic_print_size_stats(int flags)
{
    size_t sentries_data_size, sentries_metadata_size;
    size_t dsentries_data_size, dsentries_metadata_size;
    size_t data_size, metadata_size;
    int dsentries_num;
    sentries_data_size = magic_sentries_data_get_size(flags);
    sentries_metadata_size = magic_sentries_get_size(flags);
    dsentries_data_size = magic_dsentries_data_get_size(flags);
    dsentries_metadata_size = magic_dsentries_get_size(flags);
    data_size = magic_data_get_size(flags);
    metadata_size = magic_metadata_get_size(flags);
    MAGIC_DSENTRY_NUM(_magic_first_dsentry, &dsentries_num);
    _magic_printf("--------------------------------------------------------\n");
    _magic_printf("magic_print_size_stats: Printing size stats:\n");
    _magic_printf("    - sentries:  # %6d, data %8d, metadata %8d, total %8d, ratio %.3f\n", _magic_sentries_num, sentries_data_size, sentries_metadata_size, sentries_data_size+sentries_metadata_size, ((double)sentries_metadata_size)/sentries_data_size);
    _magic_printf("    - dsentries: # %6d, data %8d, metadata %8d, total %8d, ratio %.3f\n", dsentries_num, dsentries_data_size, dsentries_metadata_size, dsentries_data_size+dsentries_metadata_size, ((double)dsentries_metadata_size)/dsentries_data_size);
    _magic_printf("    - other:     # %6d, data %8d\n", 2, magic_other_data_get_size(flags));
    _magic_printf("    - state all: # %6d, data %8d, metadata %8d, total %8d, ratio %.3f\n", _magic_sentries_num+dsentries_num, sentries_data_size+dsentries_data_size, metadata_size, sentries_data_size+dsentries_data_size+metadata_size, ((double)metadata_size)/(sentries_data_size+dsentries_data_size));
    _magic_printf("    - all:       # %6d, data %8d, metadata %8d, total %8d, ratio %.3f\n", _magic_sentries_num+dsentries_num+2, data_size, metadata_size, data_size+metadata_size, ((double)metadata_size)/data_size);
    _magic_printf("--------------------------------------------------------\n");
    _magic_printf("magic_print_size_stats: Printing metadata size breakdown:\n");
    _magic_printf("    - types:     # %6d, metadata %8d\n", _magic_types_num, magic_types_get_size(flags));
    _magic_printf("    - functions: # %6d, metadata %8d\n", _magic_functions_num, magic_functions_get_size(flags));
    _magic_printf("    - dfunctions # %6d, metadata %8d\n", 0, magic_dfunctions_get_size(flags));
    _magic_printf("    - sentries:  # %6d, metadata %8d\n", _magic_sentries_num, sentries_metadata_size);
    _magic_printf("    - dsentries: # %6d, metadata %8d\n", dsentries_num, dsentries_metadata_size);
    _magic_printf("    - dsindexes: # %6d, metadata %8d\n", _magic_dsindexes_num, magic_dsindexes_get_size(flags));
    _magic_printf("    - dsodescs:  # %6d, metadata %8d\n", 0, magic_dsodescs_get_size(flags));
    _magic_printf("--------------------------------------------------------\n");
}


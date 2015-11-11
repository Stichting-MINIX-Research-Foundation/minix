#define MAGIC_RANGE_DEBUG                   MAGIC_DEBUG_SET(0)

#include <magic_range.h>

/*===========================================================================*
 *                           magic_range_is_shlib                            *
 *===========================================================================*/
PRIVATE int magic_range_is_shlib(void *addr, void **container)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSODESC lock.
     */
    int ret = 0;
    struct _magic_sodesc *sodesc;
    struct _magic_dsodesc *dsodesc;

    /* First iterate through the SO descriptors. */
    MAGIC_SODESC_ITER(_magic_first_sodesc, sodesc,
        /* First check the text range. */
        MAGIC_RANGE_DEBUG_ADDR(addr, sodesc->lib.text_range);
        if (MAGIC_ADDR_IS_IN_RANGE(addr, sodesc->lib.text_range)) {
            ret |= MAGIC_STATE_TEXT;
            goto found_so;
        }
        /* Next check the data range. */
        MAGIC_RANGE_DEBUG_ADDR(addr, sodesc->lib.data_range);
        if (MAGIC_ADDR_IS_IN_RANGE(addr, sodesc->lib.data_range)) {
            ret |= MAGIC_STATE_DATA;
            goto found_so;
        }
    );

    /* Next iterate through the DSO descriptors. */
    MAGIC_SODESC_ITER(_magic_first_dsodesc, dsodesc,
        /* First check the text range. */
        MAGIC_RANGE_DEBUG_ADDR(addr, dsodesc->lib.text_range);
        if (MAGIC_ADDR_IS_IN_RANGE(addr, dsodesc->lib.text_range)) {
            ret |= MAGIC_STATE_TEXT;
            goto found_dso;
        }
        /* Next check the data range. */
        MAGIC_RANGE_DEBUG_ADDR(addr, dsodesc->lib.data_range);
        if (MAGIC_ADDR_IS_IN_RANGE(addr, dsodesc->lib.data_range)) {
            ret |= MAGIC_STATE_DATA;
            goto found_dso;
        }
    );


out:
    return ret;
found_so:
    ret |= MAGIC_STATE_LIB | MAGIC_STATE_LIB_SO;
    if (container != NULL)
        *container = (void *)(sodesc);
    goto out;
found_dso:
    ret |= MAGIC_STATE_LIB | MAGIC_STATE_LIB_DSO;
    if (container != NULL) {
        *container = (void *)(dsodesc);
    }
    goto out;

}

/*===========================================================================*
 *                          magic_range_is_data                              *
 *===========================================================================*/
PRIVATE INLINE int magic_range_is_data(void *addr)
{
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_data_range);
    return MAGIC_ADDR_IS_IN_RANGE(addr, magic_data_range) ? MAGIC_STATE_DATA : 0;
}

/*===========================================================================*
 *                          magic_range_is_text                              *
 *===========================================================================*/
PRIVATE INLINE int magic_range_is_text(void *addr)
{
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_text_range);
    return MAGIC_ADDR_IS_IN_RANGE(addr, magic_text_range) ? MAGIC_STATE_TEXT : 0;
}

/*===========================================================================*
 *                          magic_range_is_heap                              *
 *===========================================================================*/
PRIVATE INLINE int magic_range_is_heap(void *addr)
{
    void* heap_range[2];
    MAGIC_RANGE_SET_MIN(heap_range, magic_heap_start);
    MAGIC_RANGE_SET_MAX(heap_range, (char *)magic_heap_end + MAGIC_HEAP_GAP);
    MAGIC_RANGE_DEBUG_ADDR(addr, heap_range);
    return MAGIC_ADDR_IS_IN_RANGE(addr, heap_range) ? MAGIC_STATE_HEAP : 0;
}

/*===========================================================================*
 *                          magic_range_is_stack                             *
 *===========================================================================*/
PUBLIC int magic_range_is_stack(void *addr)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSENTRY lock.
     */
    struct _magic_sentry *sentry;
    int ret;
    void *stack_range[2];

    MAGIC_RANGE_INIT(stack_range);
    assert(_magic_first_stack_dsentry && _magic_last_stack_dsentry);
    sentry = MAGIC_DSENTRY_TO_SENTRY(_magic_first_stack_dsentry);
    MAGIC_RANGE_SET_MIN(stack_range,
        (char *) MAGIC_DSENTRY_TO_SENTRY(_magic_last_stack_dsentry)->address -
        MAGIC_STACK_GAP);
    MAGIC_RANGE_SET_MAX(stack_range,
        ((char *) sentry->address) + sentry->type->size - 1);
#if MAGIC_RANGE_ROUND_STACK
    MAGIC_RANGE_PAGE_ROUND(stack_range);
#endif
    MAGIC_RANGE_DEBUG_ADDR(addr, stack_range);
    ret = MAGIC_ADDR_IS_IN_RANGE(addr, stack_range) ? MAGIC_STATE_STACK : 0;

    return ret;
}

/*===========================================================================*
 *                         magic_range_is_dsentry                            *
 *===========================================================================*/
PUBLIC int magic_range_is_dsentry(void *addr)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSENTRY lock.
     */
    int ret = 0;
    void *start_address, *end_address;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    int region;

    if(magic_update_dsentry_ranges) {
        MAGIC_RANGE_INIT(magic_heap_range);
        MAGIC_RANGE_INIT(magic_map_range);
        MAGIC_RANGE_INIT(magic_shm_range);
        MAGIC_RANGE_INIT(magic_stack_range);
        MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
            start_address = sentry->address;
            end_address = (void *) (((char *)sentry->address) +
                sentry->type->size - 1);
            region = MAGIC_STATE_REGION(sentry);
            if(region & MAGIC_STATE_HEAP) {
                MAGIC_RANGE_UPDATE(magic_heap_range, start_address, end_address);
            }
            else if(region & MAGIC_STATE_MAP) {
                MAGIC_RANGE_UPDATE(magic_map_range, start_address, end_address);
            }
            else if(region & MAGIC_STATE_SHM) {
                MAGIC_RANGE_UPDATE(magic_shm_range, start_address, end_address);
            }
            else if(region & MAGIC_STATE_STACK) {
                MAGIC_RANGE_UPDATE(magic_stack_range, start_address, end_address);
            }
        );
        magic_update_dsentry_ranges = 0;
    }
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_heap_range);
    if(MAGIC_ADDR_IS_IN_RANGE(addr, magic_heap_range)) {
        ret |= MAGIC_STATE_HEAP;
    }
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_map_range);
    if(MAGIC_ADDR_IS_IN_RANGE(addr, magic_map_range)) {
        ret |= MAGIC_STATE_MAP;
    }
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_shm_range);
    if(MAGIC_ADDR_IS_IN_RANGE(addr, magic_shm_range)) {
        ret |= MAGIC_STATE_SHM;
    }
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_stack_range);
    if(MAGIC_ADDR_IS_IN_RANGE(addr, magic_stack_range)) {
        ret |= MAGIC_STATE_STACK;
    }

    return ret;
}

/*===========================================================================*
 *                        magic_range_is_dfunction                           *
 *===========================================================================*/
PUBLIC int magic_range_is_dfunction(void *addr)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DFUNCTION lock.
     */
    int ret = 0;
    void *start_address;
    struct _magic_dfunction* dfunction;
    struct _magic_function* function;
    int region;

    if(magic_update_dfunction_ranges) {
        MAGIC_RANGE_INIT(magic_dfunction_range);
        MAGIC_DFUNCTION_FUNC_ITER(_magic_first_dfunction, dfunction, function,
            start_address = function->address;
            region = MAGIC_STATE_REGION(function);
            assert(region & MAGIC_STATE_TEXT);
            MAGIC_RANGE_UPDATE(magic_dfunction_range, start_address, start_address);
        );
        magic_update_dfunction_ranges = 0;
    }
    MAGIC_RANGE_DEBUG_ADDR(addr, magic_dfunction_range);
    if(MAGIC_ADDR_IS_IN_RANGE(addr, magic_dfunction_range)) {
        ret |= MAGIC_STATE_TEXT;
    }

    return ret;
}

/*===========================================================================*
 *                           magic_ranges_init                               *
 *===========================================================================*/
PUBLIC void magic_ranges_init(void)
{
    int i,j;
    void *start_address, *end_address;
    const char* linker_vars[] = { MAGIC_LINKER_VAR_NAMES };

    /* Init sentry and data range. */
    MAGIC_RANGE_INIT(magic_data_range);
    MAGIC_RANGE_INIT(magic_sentry_range);
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        start_address = _magic_sentries[i].address;
        end_address = (void *) (((char *)_magic_sentries[i].address)+_magic_sentries[i].type->size-1);
        MAGIC_RANGE_UPDATE(magic_sentry_range, start_address, end_address);
        j = 0;
        while (linker_vars[j] && strcmp(linker_vars[j], _magic_sentries[i].name)) j++;
        if (linker_vars[j] || MAGIC_STATE_FLAG(&_magic_sentries[i], MAGIC_STATE_THREAD_LOCAL)
            || MAGIC_STATE_FLAG(&_magic_sentries[i], MAGIC_STATE_EXTERNAL)) {
            continue;
        }
        MAGIC_RANGE_UPDATE(magic_data_range, start_address, end_address);
    }
#if MAGIC_RANGE_ROUND_DATA
    MAGIC_RANGE_PAGE_ROUND(magic_data_range);
#endif

    /* Init function range. */
    MAGIC_RANGE_INIT(magic_function_range);
    for (i = 0 ; i < _magic_functions_num ; i++) {
        start_address = _magic_functions[i].address;
        MAGIC_RANGE_UPDATE(magic_function_range, start_address, start_address);
    }

    /* Init text range. */
#ifdef __MINIX
    MAGIC_RANGE_SET(magic_text_range, MAGIC_TEXT_START,
        MAGIC_TEXT_END ? MAGIC_TEXT_END : ((char *)magic_function_range[1]));
#else
    MAGIC_RANGE_SET(magic_text_range, MAGIC_TEXT_START,
        MAGIC_TEXT_END ? MAGIC_TEXT_END : ((char *)magic_data_range[0] - 1));
#endif
#if MAGIC_RANGE_ROUND_TEXT
    MAGIC_RANGE_PAGE_ROUND(magic_text_range);
#endif

    /* Init heap start. */
    magic_heap_start = MAGIC_HEAP_START ? MAGIC_HEAP_START : ((char *)magic_data_range[1] + 1);
    magic_heap_end = ((char *)sbrk(0)) - 1;

    /* Defaults for other ranges. */
    MAGIC_RANGE_INIT(magic_heap_range);
    MAGIC_RANGE_INIT(magic_map_range);
    MAGIC_RANGE_INIT(magic_shm_range);
    MAGIC_RANGE_INIT(magic_stack_range);
    MAGIC_RANGE_INIT(magic_dfunction_range);
}

/*===========================================================================*
 *                        magic_range_lookup_by_addr                         *
 *===========================================================================*/
PUBLIC int magic_range_lookup_by_addr(void *addr, void **container)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSENTRY, DFUNCTION and DSODESC locks.
     */
    int ret;
    /* Constant ranges first. */
    if (magic_range_is_data(addr)) {
        return MAGIC_STATE_DATA;
    }
    if (magic_range_is_text(addr)) {
        return MAGIC_STATE_TEXT;
    }

    /* Non-dsentry ranges next. */
    if (magic_range_is_heap(addr)) {
        return MAGIC_STATE_HEAP;
    }
    if (magic_range_is_stack(addr)) {
        return MAGIC_STATE_STACK;
    }

    /* Shared library ranges. */
#if 0
    /* XXX: This kind of range isn't very accurate. */
    if (magic_range_is_dfunction(addr)) {
        return MAGIC_STATE_LIB | MAGIC_STATE_TEXT;
    }
#endif

    if ((ret = magic_range_is_shlib(addr, container))) {
        return ret;
    }

    /* Dsentry ranges last. */
    return magic_range_is_dsentry(addr);
}



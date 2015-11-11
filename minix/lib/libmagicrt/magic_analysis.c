
#include <magic_analysis.h>

PUBLIC char magic_ne_str[] =  "NULL_TYPE";
PUBLIC char magic_enf_str[] = "UNKNOWN_TYPE";
PUBLIC char magic_bo_str[] =  "BAD_TYPE";
PUBLIC char magic_be_str[] =  "BAD_ENTRY_TYPE";
PUBLIC char magic_bv_str[] =  "BAD_VALUE_TYPE";
PUBLIC char magic_vf_str[] =  "VALUE_FOUND_TYPE";
PUBLIC const struct _magic_type magic_NULL_ENTRY_TYPE =      MAGIC_TYPE_SPECIAL_INIT(magic_ne_str);
PUBLIC const struct _magic_type magic_ENTRY_NOT_FOUND_TYPE = MAGIC_TYPE_SPECIAL_INIT(magic_enf_str);
PUBLIC const struct _magic_type magic_BAD_OFFSET_TYPE =      MAGIC_TYPE_SPECIAL_INIT(magic_bo_str);
PUBLIC const struct _magic_type magic_BAD_ENTRY_TYPE =       MAGIC_TYPE_SPECIAL_INIT(magic_be_str);
PUBLIC const struct _magic_type magic_BAD_VALUE_TYPE =       MAGIC_TYPE_SPECIAL_INIT(magic_bv_str);
PUBLIC const struct _magic_type magic_VALUE_FOUND =          MAGIC_TYPE_SPECIAL_INIT(magic_vf_str);

PRIVATE magic_cb_sentries_analyze_pre_t magic_sentries_analyze_pre_cb = NULL;

/*===========================================================================*
 *                      magic_setcb_sentries_analyze_pre                     *
 *===========================================================================*/
PUBLIC void magic_setcb_sentries_analyze_pre(magic_cb_sentries_analyze_pre_t cb)
{
    magic_sentries_analyze_pre_cb = cb;
}

/*===========================================================================*
 *                        magic_sentry_print_ptr_types                       *
 *===========================================================================*/
PUBLIC int magic_sentry_print_ptr_types(struct _magic_sentry* entry)
{
    int ret, ptrs_found = 0;
    void* args_array[2];
    args_array[0] = entry;
    args_array[1] = &ptrs_found;
    ret = magic_type_walk_root_all(entry->type, magic_type_examine_ptr_cb, args_array);
    assert(ret >= 0);
    return ptrs_found;
}

/*===========================================================================*
 *                         magic_sentry_extract_ptrs                         *
 *===========================================================================*/
PUBLIC int magic_sentry_extract_ptrs(struct _magic_sentry* entry, void ****ptr_map, const struct _magic_type ***ptr_type_map, int *ptr_num)
{
    int from_wrapper = MAGIC_MEM_WRAPPER_IS_ACTIVE();
    int ret = magic_type_count_ptrs(entry->type, ptr_num);
    assert(ret == 0);
    if(*ptr_num == 0) {
        *ptr_map = NULL;
        *ptr_type_map = NULL;
    }
    else {
        void* args_array[4];
        int ptr_num2 = 0;
        if(!from_wrapper) {
            MAGIC_MEM_WRAPPER_BEGIN();
        }
        *ptr_map = (void ***) malloc((*ptr_num)*sizeof(void **));
        *ptr_type_map = (const struct _magic_type **) malloc((*ptr_num)*sizeof(const struct _magic_type *));
        if(!from_wrapper) {
            MAGIC_MEM_WRAPPER_END();
        }
        args_array[0] = entry;
        args_array[1] = *ptr_map;
        args_array[2] = *ptr_type_map;
        args_array[3] = &ptr_num2;
        ret = magic_type_walk_root_all(entry->type, magic_type_extract_ptr_cb, args_array);
        assert(ret >= 0);
        assert(*ptr_num == ptr_num2);
    }
    return 0;
}

/*===========================================================================*
 *                           magic_sentry_analyze                            *
 *===========================================================================*/
PUBLIC int magic_sentry_analyze(struct _magic_sentry* sentry, int flags,
    const magic_sentry_analyze_cb_t cb, void* cb_args,
    _magic_sel_stats_t *sentry_stats)
{
    int ret;
    int selement_num = 0, sel_analyzed_num = 0;
    void* args_array[7];
    args_array[0] = (void*) &flags;
    args_array[1] = (void*) __UNCONST(&cb);
    args_array[2] = (void*) cb_args;
    args_array[3] = (void*) sentry;
    args_array[4] = (void*) sentry_stats;
    args_array[5] = (void*) &selement_num;
    args_array[6] = (void*) &sel_analyzed_num;
    ret = magic_type_walk_root_all(sentry->type, magic_type_analyzer_cb,
        args_array);
    if(ret < 0) {
        return ret;
    }

    return flags;
}

/*===========================================================================*
 *                          magic_sentries_analyze                           *
 *===========================================================================*/
PUBLIC int magic_sentries_analyze(int flags, const magic_sentry_analyze_cb_t cb,
    void *cb_args, _magic_sel_stats_t *sentries_stats)
{
    int i, ret;
    struct _magic_sentry *sentry;

    if (flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)
        magic_reentrant_disable();

    /* See if any pre-analyze callback has been registered. */
    if (magic_sentries_analyze_pre_cb) {
        ret = magic_sentries_analyze_pre_cb();
        if (ret < 0) {
            return ret;
        }
    }

    /* Analyze all the sentries. */
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        sentry = &_magic_sentries[i];
        ret = magic_sentry_analyze(sentry, flags, cb, cb_args, sentries_stats);
        if (ret < 0) {
            return ret;
        }
        else {
            flags |= ret;
        }
    }

    /* Analyze all the dsentries if asked to. */
    if (flags & MAGIC_SEL_ANALYZE_DYNAMIC) {
        return magic_dsentries_analyze(flags, cb, cb_args, sentries_stats);
    }

    if (flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)
        magic_reentrant_enable();

    return flags;
}

/*===========================================================================*
 *                       magic_sentry_print_selements                        *
 *===========================================================================*/
PUBLIC int magic_sentry_print_selements(struct _magic_sentry* sentry)
{
    int flags = (MAGIC_SEL_ANALYZE_POINTERS|MAGIC_SEL_ANALYZE_NONPOINTERS|MAGIC_SEL_ANALYZE_DATA|MAGIC_SEL_ANALYZE_INVARIANTS|MAGIC_SEL_ANALYZE_VIOLATIONS);
    return magic_sentry_analyze(sentry, flags, magic_sentry_print_el_cb, NULL, NULL);
}

/*===========================================================================*
 *                     magic_sentry_print_ptr_selements                      *
 *===========================================================================*/
PUBLIC int magic_sentry_print_ptr_selements(struct _magic_sentry* sentry,
    int skip_null_ptrs, int max_target_recusions)
{
    int flags = (MAGIC_SEL_ANALYZE_POINTERS|MAGIC_SEL_ANALYZE_DATA|MAGIC_SEL_ANALYZE_INVARIANTS|MAGIC_SEL_ANALYZE_VIOLATIONS);
    void* args_array[2];
    args_array[0] = &skip_null_ptrs;
    args_array[1] = &max_target_recusions;
    return magic_sentry_analyze(sentry, flags, magic_sentry_print_ptr_el_cb, args_array, NULL);
}

/*===========================================================================*
 *                         magic_dsentries_analyze                           *
 *===========================================================================*/
PUBLIC int magic_dsentries_analyze(int flags, const magic_sentry_analyze_cb_t cb,
    void *cb_args, _magic_sel_stats_t *dsentries_stats)
{
    int ret = 0;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;

    /* If dead dsentries are enabled, garbage collect them to ensure consistency. */
    if (magic_allow_dead_dsentries)
        magic_free_dead_dsentries();

    /* Analyze all the dsentries. */
    /*
     * We need to hold the DSENTRY, DFUNCTION and DSODESC locks for the
     * magic_range_lookup_by_addr() function.
     */
    MAGIC_MULTIPLE_LOCK(1, 1, 1, 0);
    MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry,
        sentry,

        /*
         * Check if we should analyze out-of-band dsentries.
         */
        if (!(flags & MAGIC_SEL_ANALYZE_OUT_OF_BAND) &&
            MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND)) {
            continue;
        }

        /*
         * Check if we should analyze shlib state dsentries.
         */
        if (!(flags & MAGIC_SEL_ANALYZE_LIB_SRC) &&
            MAGIC_STATE_FLAG(sentry, MAGIC_STATE_LIB)) {
            continue;
        }

        ret = magic_sentry_analyze(sentry, flags, cb, cb_args, dsentries_stats);
        if (ret < 0) {
            break;
        }
        else {
            flags |= ret;
        }
    );
    MAGIC_MULTIPLE_UNLOCK(1, 1, 1, 0);

    return ret < 0 ? ret : flags;
}

/*===========================================================================*
 *                         magic_sentry_print_el_cb                          *
 *===========================================================================*/
PUBLIC int magic_sentry_print_el_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    if(sel_analyzed->num == 1) {
        MAGIC_SENTRY_PRINT(selement->sentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }

    MAGIC_SELEMENT_PRINT(selement, MAGIC_EXPAND_TYPE_STR);
    _magic_printf("\n");
    MAGIC_SEL_ANALYZED_PRINT(sel_analyzed, MAGIC_EXPAND_TYPE_STR);
    _magic_printf("\n");
    MAGIC_SEL_STATS_PRINT(sel_stats);
    _magic_printf("\n\n");

    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

/*===========================================================================*
 *                      magic_sentry_print_ptr_el_cb                         *
 *===========================================================================*/
PUBLIC int magic_sentry_print_ptr_el_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    int r;
    void** args_array = (void**) cb_args;
    int skip_null_ptrs = args_array ? *((int*)args_array[0]) : 0;
    int max_target_recursions = args_array ? *((int*)args_array[1]) : 0;

    if(sel_analyzed->type_id != MAGIC_TYPE_POINTER) {
        return MAGIC_SENTRY_ANALYZE_CONTINUE;
    }
    if(skip_null_ptrs && sel_analyzed->u.ptr.value == 0) {
        return MAGIC_SENTRY_ANALYZE_CONTINUE;
    }
    magic_sentry_print_el_cb(selement, sel_analyzed, sel_stats, cb_args);
    if(max_target_recursions>0 && !(sel_analyzed->flags & MAGIC_SEL_FOUND_VIOLATIONS)) {
        struct _magic_sentry *sentry = &sel_analyzed->u.ptr.trg.dsentry.sentry;
        if(MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(sel_analyzed) && MAGIC_SENTRY_ID(sentry)!=MAGIC_SENTRY_ID(selement->sentry)) {
            r = magic_sentry_print_ptr_selements(sentry, skip_null_ptrs,
                max_target_recursions-1);
            if(r < 0) {
                _magic_printf("magic_sentry_print_ptr_el_cb: recursive step reported error %d\n", r);
                return r;
            }
        }
    }

    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

/*===========================================================================*
 *                   magic_sentry_print_el_with_trg_reg_cb                   *
 *===========================================================================*/
PUBLIC int magic_sentry_print_el_with_trg_reg_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    if(MAGIC_SEL_ANALYZED_TRG_FLAGS(sel_analyzed)) {
        return magic_sentry_print_el_cb(selement, sel_analyzed, sel_stats,
            cb_args);
    }

    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

/*===========================================================================*
 *                     magic_sentry_print_el_with_trg_cb                     *
 *===========================================================================*/
PUBLIC int magic_sentry_print_el_with_trg_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    if(MAGIC_SEL_ANALYZED_TRG_FLAGS(sel_analyzed)
        && MAGIC_SELEMENT_HAS_TRG(selement)) {
        return magic_sentry_print_el_cb(selement, sel_analyzed, sel_stats,
            cb_args);
    }

    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

/*===========================================================================*
 *                          magic_type_count_ptrs                            *
 *===========================================================================*/
PUBLIC int magic_type_count_ptrs(const struct _magic_type* type, int* ptr_num)
{
    int ret;
    void* args_array[4] = { NULL, NULL, NULL };
    args_array[3] = ptr_num;
    *ptr_num = 0;
    ret = magic_type_walk_root_all(type, magic_type_extract_ptr_cb, args_array);
    assert(ret >= 0);
    return 0;
}

/*===========================================================================*
 *                         magic_type_examine_ptr_cb                         *
 *===========================================================================*/
PUBLIC int magic_type_examine_ptr_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    int ret;
    void** args_array = (void**) cb_args;
    if(type->type_id == MAGIC_TYPE_POINTER) {
        const struct _magic_sentry *root_entry = (const struct _magic_sentry *) args_array[0];
        int *ptrs_found = (int*) args_array[1];
        char* root_address = root_entry->address;
        void* ptr_address = root_address ? root_address+offset : NULL;
        void* target_address = ptr_address ? *((void**)ptr_address) : NULL;
        (*ptrs_found)++;
        _magic_printf("Pointer found for root entry (name=%s, address=0x%08x) at offset %d, static target type is: ", root_entry->name, (unsigned) root_address, offset);
        magic_type_str_print(type->contained_types[0]);
        _magic_printf(" - dynamic target types are: ");
        if(!target_address) {
            _magic_printf("NULL");
        }
        else {
            ret = magic_type_str_print_from_target(target_address);
            if(ret < 0) {
                _magic_printf("ENTRY NOT FOUND");
            }
        }
        _magic_printf("\n");
    }
    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                         magic_type_extract_ptr_cb                         *
 *===========================================================================*/
PUBLIC int magic_type_extract_ptr_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    void** args_array = (void**) cb_args;
    static void* null_ptr=NULL;
    if(type->type_id == MAGIC_TYPE_POINTER) {
        const struct _magic_sentry *root_entry = (const struct _magic_sentry *) args_array[0];
        void ***ptr_map = (void ***) args_array[1];
        const struct _magic_type **ptr_type_map = (const struct _magic_type **) args_array[2];
        int *ptr_num = (int*) args_array[3];
        char* root_ptr;
        void** ptr_ptr;
        assert(ptr_num);
        if(root_entry && ptr_map && ptr_type_map) {
            root_ptr = root_entry->address;
            ptr_ptr= root_ptr ? (void**)(root_ptr+offset) : &null_ptr;
            ptr_map[*ptr_num] = ptr_ptr;
            ptr_type_map[*ptr_num] = type;
        }
        (*ptr_num)++;
    }
    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                           magic_type_analyzer_cb                          *
 *===========================================================================*/
PUBLIC int magic_type_analyzer_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args)
{
    int ret;
    void **args_array = (void **) cb_args;
    int *flags = (int *)args_array[0];
    magic_sentry_analyze_cb_t sentry_analyze_cb =
        *((magic_sentry_analyze_cb_t *) args_array[1]);
    void *sentry_analyze_cb_args = (void *) args_array[2];
    struct _magic_sentry* sentry = (struct _magic_sentry *) args_array[3];
    _magic_sel_stats_t *sentry_stats = (_magic_sel_stats_t *) args_array[4];
    int *selement_num = (int *) args_array[5];
    int *sel_analyzed_num = (int *) args_array[6];
    static int likely_pointer_orig_type_id;
    static int likely_pointer_orig_contained_type_id;
    _magic_selement_t selement;
    _magic_sel_analyzed_t sel_analyzed;
    _magic_sel_stats_t sel_stats;

    if (type->type_id == MAGIC_TYPE_UNION &&
        ((*flags) & MAGIC_SEL_SKIP_UNIONS)) {
        /* Skip unions when requested. */
        return MAGIC_TYPE_WALK_SKIP_PATH;
    }

    if ((type->type_id == MAGIC_TYPE_INTEGER ||
            type->type_id == MAGIC_TYPE_ENUM) &&
            ((*flags) & MAGIC_SEL_SKIP_INTEGERS)) {
        /* Skip integers when requested. */
        return MAGIC_TYPE_WALK_SKIP_PATH;
    }

    if (((*flags) & MAGIC_SEL_ANALYZE_LIKELYPOINTERS) &&
        (MAGIC_TYPE_IS_RAW_ARRAY(type) ||
        (MAGIC_TYPE_IS_INT_ARRAY(type) &&
            type->contained_types[0]->size != sizeof(void *) &&
            type->size >= sizeof(void *)) ||
        (type->type_id == MAGIC_TYPE_INTEGER && type->size > sizeof(void *)))) {
        /* This can be either UNION, INTEGER or ARRAY (of VOID or INTEGER). */
        likely_pointer_orig_type_id = type->type_id;
        if (type->type_id == MAGIC_TYPE_ARRAY)
            likely_pointer_orig_contained_type_id =
                type->contained_types[0]->type_id;
        /* Try to find likely pointers in raw arrays. */
        ret = magic_type_walk_as_ptrint_array(parent_type, parent_offset,
            child_num, type, (char *)sentry->address + offset, offset,
            0, ULONG_MAX, magic_type_analyzer_cb, cb_args);
        likely_pointer_orig_type_id = likely_pointer_orig_contained_type_id = 0;
        if (ret != MAGIC_EBADWALK) {
            return ret == 0 ? MAGIC_TYPE_WALK_SKIP_PATH : ret;
        }
    }

    selement.sentry = sentry;
    selement.parent_type = parent_type;
    selement.parent_address = (char *)sentry->address + parent_offset;
    selement.child_num = child_num;
    selement.type = type;
    selement.address = (char *)sentry->address + offset;
    selement.depth = depth;
    selement.num = ++(*selement_num);
    selement.cb_args = cb_args;

    ret = magic_selement_analyze(&selement, *flags, &sel_analyzed, &sel_stats);
    if (ret &&
        (((ret & MAGIC_SEL_FOUND_DATA) &&
          ((*flags) & MAGIC_SEL_ANALYZE_DATA)) ||
         ((ret & MAGIC_SEL_FOUND_INVARIANTS) &&
          ((*flags) & MAGIC_SEL_ANALYZE_INVARIANTS)) ||
         ((ret & MAGIC_SEL_FOUND_VIOLATIONS) &&
          ((*flags) & MAGIC_SEL_ANALYZE_VIOLATIONS)) ||
         ((ret & MAGIC_SEL_FOUND_WALKABLE) &&
          ((*flags) & MAGIC_SEL_ANALYZE_WALKABLE))
        )) {
        *flags |= ret;
        sel_analyzed.num = ++(*sel_analyzed_num);
        if (likely_pointer_orig_type_id) {
            sel_analyzed.type_id = likely_pointer_orig_type_id;
            sel_analyzed.contained_type_id =
                likely_pointer_orig_contained_type_id;
        }
        ret = sentry_analyze_cb(&selement, &sel_analyzed, &sel_stats,
            sentry_analyze_cb_args);
        if (sel_analyzed.flags & MAGIC_SEL_FOUND_INVARIANTS) {
            _magic_sel_stats_t* sel_stats_ptr = &sel_stats;
            if (sentry_stats) {
                MAGIC_SEL_STATS_INCR(sentry_stats, sel_stats_ptr);
            }
        }
        if (ret != MAGIC_SENTRY_ANALYZE_CONTINUE) {
            switch (ret) {
                case MAGIC_SENTRY_ANALYZE_SKIP_PATH:
                    ret = MAGIC_TYPE_WALK_SKIP_PATH;
                break;
                case MAGIC_SENTRY_ANALYZE_STOP:
                    ret = MAGIC_TYPE_WALK_STOP;
                break;
                default:
                    assert(ret < 0 && "Invalid error code!");
                break;
            }
            return ret;
        }
    }

    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                          magic_selement_analyze                           *
 *===========================================================================*/
PUBLIC int magic_selement_analyze(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    const struct _magic_type *type = selement->type;
    short is_ptr_el = type->type_id == MAGIC_TYPE_POINTER;
    short is_nonptr_el = type->num_child_types == 0 || (type->type_id == MAGIC_TYPE_INTEGER && type->num_child_types > 0);
    short analyze_ptr_el, analyze_nonptr_el;

    if (!is_ptr_el && !is_nonptr_el) {
        if (MAGIC_TYPE_IS_WALKABLE(type)) {
            sel_analyzed->type_id = MAGIC_TYPE_OPAQUE;
            return MAGIC_SEL_FOUND_WALKABLE;
        }
        /* Not an element to analyze. */
        return 0;
    }
    assert(is_ptr_el ^ is_nonptr_el);

    analyze_ptr_el = is_ptr_el && (flags & MAGIC_SEL_ANALYZE_POINTERS);
    analyze_nonptr_el = 0;
    if (is_nonptr_el && ((flags & MAGIC_SEL_ANALYZE_DATA) || MAGIC_TYPE_HAS_VALUE_SET(type))) {
        if (flags & MAGIC_SEL_ANALYZE_NONPOINTERS) {
            analyze_nonptr_el = 1;
        }
        else if (flags & MAGIC_SEL_ANALYZE_LIKELYPOINTERS) {
            short is_intvalue_el = type->type_id == MAGIC_TYPE_ENUM
                || type->type_id == MAGIC_TYPE_INTEGER;
            if (is_intvalue_el && type->size == sizeof(void *)) {
                long value = magic_selement_to_int(selement);
                analyze_nonptr_el = MAGIC_INT_IS_LIKELY_PTR(value);
            }
        }
    }

    if (analyze_nonptr_el && (flags & MAGIC_SEL_ANALYZE_NONPTRS_AS_PTRS) &&
        type->size == sizeof(void *)) {
        struct _magic_type tmp_type;
        int ret;
        tmp_type = *(selement->type);
        tmp_type.type_id = MAGIC_TYPE_POINTER;
        selement->type = &tmp_type;

        /* Analyze non-pointer element as a pointer. */
        ret = magic_selement_analyze_ptr(selement, flags, sel_analyzed, sel_stats);

        selement->type = type;
        /* Keep original type in sel_analyzed. */
        sel_analyzed->type_id = type->type_id;

        return ret;
    }

    assert(!analyze_ptr_el || !analyze_nonptr_el);

    if (analyze_ptr_el) {
        /* Analyze pointer element. */
        return magic_selement_analyze_ptr(selement, flags, sel_analyzed,
            sel_stats);
    }
    if (analyze_nonptr_el) {
        /* Analyze nonpointer element. */
        return magic_selement_analyze_nonptr(selement, flags, sel_analyzed,
            sel_stats);
    }

    /* Nothing to analyze. */
    return 0;
}

/*===========================================================================*
 *                   magic_selement_analyze_ptr_value_invs                   *
 *===========================================================================*/
PUBLIC int magic_selement_analyze_ptr_value_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    const struct _magic_type* ptr_type = selement->type;
    int i, ret = 0;
    const struct _magic_type *first_trg_type;
    void* value = sel_analyzed->u.ptr.value;

    first_trg_type = MAGIC_SEL_ANALYZED_PTR_FIRST_TRG_TYPE(sel_analyzed);
    assert(sel_analyzed->u.ptr.num_trg_types > 0 && first_trg_type);
    if(first_trg_type == MAGIC_TYPE_NULL_ENTRY) {
        ret |= MAGIC_SEL_FOUND_INVARIANTS;
        sel_stats->null_type_found++;
    }
    else if(MAGIC_TYPE_FLAG(ptr_type, MAGIC_TYPE_INT_CAST)) {
        if(MAGIC_TYPE_HAS_VALUE_SET(ptr_type)) {
            i=0;
            while(MAGIC_TYPE_HAS_VALUE(ptr_type, i)) {
                int trg_value = MAGIC_TYPE_VALUE(ptr_type, i);
                if(trg_value == (int) value) {
                    ret |= MAGIC_SEL_FOUND_INVARIANTS;
                    MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_VALUE_FOUND);
                    sel_stats->value_found++;
                    break;
                }
                i++;
            }
            if(!(ret & MAGIC_SEL_FOUND_INVARIANTS) && MAGIC_TYPE_FLAG(ptr_type, MAGIC_TYPE_STRICT_VALUE_SET)) {
                ret |= MAGIC_SEL_FOUND_INVARIANTS;
                MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_BAD_VALUE);
                sel_stats->badvalue_found++;
            }
        }
        else if(MAGIC_PTR_IS_LIKELY_INT(value)) {
            ret |= MAGIC_SEL_FOUND_INVARIANTS;
            MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_VALUE_FOUND);
            sel_stats->value_found++;
        }
    }

    return ret;
}

/*===========================================================================*
 *                    magic_selement_analyze_ptr_trg_invs                    *
 *===========================================================================*/
PUBLIC int magic_selement_analyze_ptr_trg_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    int ret = 0;
    const struct _magic_type *first_trg_type;

    first_trg_type = MAGIC_SEL_ANALYZED_PTR_FIRST_TRG_TYPE(sel_analyzed);
    assert(sel_analyzed->u.ptr.num_trg_types > 0 && first_trg_type);
    sel_stats->trg_flags |= sel_analyzed->u.ptr.trg_flags;
    if(first_trg_type == MAGIC_TYPE_ENTRY_NOT_FOUND) {
        ret |= MAGIC_SEL_FOUND_INVARIANTS;
        sel_stats->unknown_found++;
    }
    else if(first_trg_type == MAGIC_TYPE_BAD_ENTRY) {
        ret |= MAGIC_SEL_FOUND_INVARIANTS;
        sel_stats->badentry_found++;
    }
    else if(first_trg_type == MAGIC_TYPE_BAD_OFFSET) {
        ret |= MAGIC_SEL_FOUND_INVARIANTS;
        sel_stats->badoffset_found++;
    }

    return ret;
}

/*===========================================================================*
 *                      magic_selement_analyze_ptr_target                    *
 *===========================================================================*/
PUBLIC _magic_trg_stats_t magic_selement_analyze_ptr_target(const struct _magic_type *ptr_type,
    const struct _magic_type *trg_type, int trg_flags)
{
    const struct _magic_type* type = ptr_type->contained_types[0];

    /* Analyze void target types first. */
    if(MAGIC_TYPE_IS_VOID(trg_type)) {
        int ptr_can_point_to_text = magic_type_ptr_is_text(ptr_type);
        int ptr_can_point_to_data = magic_type_ptr_is_data(ptr_type);
        int is_trg_data = (MAGIC_STATE_FLAGS_REGION(trg_flags) & ~MAGIC_STATE_TEXT) != 0;
        int is_trg_text = trg_flags & MAGIC_STATE_TEXT;
        assert(ptr_can_point_to_text || ptr_can_point_to_data);
        assert(is_trg_text || is_trg_data);
        if((!ptr_can_point_to_text && is_trg_text)
            || (!ptr_can_point_to_data && is_trg_data)) {
            return _badentry_found;
        }
        else {
            return _void_type_found;
        }
    }

    /* Analyze void types next. */
    if(MAGIC_TYPE_IS_VOID(type)) {
        /* Pretend the pointer has been found, void* can point to any valid target. */
        return _ptr_type_found;
    }

    /* See if the target type is compatible with the static type. */
    if(magic_type_compatible(trg_type, type, 0)) {
        return _ptr_type_found;
    }

    /* See if the target type is compatible with some static cast type. */
    if(MAGIC_TYPE_HAS_COMP_TYPES(ptr_type) && magic_type_comp_compatible(ptr_type, trg_type)) {
        return _comp_trg_types_found;
    }

    /* No chance. The pointer is pointing to some other invalid type. */
    return _other_types_found;
}

/*===========================================================================*
 *                    magic_selement_analyze_ptr_type_invs                   *
 *===========================================================================*/
PUBLIC int magic_selement_analyze_ptr_type_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    const struct _magic_type* ptr_type = selement->type;
    unsigned int i;
    int ret = 0;
    int trg_flags;
    const struct _magic_type *trg_type;
    _magic_trg_stats_t trg_stats;

    assert(sel_analyzed->u.ptr.num_trg_types > 0);
    if(MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(sel_analyzed)) {
        /* No invariants if we only have a special target type. */
        return ret;
    }
    trg_flags= sel_analyzed->u.ptr.trg_flags;
    sel_stats->trg_flags |= trg_flags;
    ret |= MAGIC_SEL_FOUND_INVARIANTS;

    /* Analyze targets. */
    sel_analyzed->u.ptr.first_legal_trg_type = -1;
    sel_analyzed->u.ptr.num_legal_trg_types = 0;
    for(i=0;i<sel_analyzed->u.ptr.num_trg_types;i++) {
        trg_type = sel_analyzed->u.ptr.trg_selements[i].type;
        trg_stats = magic_selement_analyze_ptr_target(ptr_type, trg_type, trg_flags);
        sel_analyzed->u.ptr.trg_stats[i] = trg_stats;
        if(!MAGIC_SEL_ANALYZED_TRG_STATS_HAS_VIOLATIONS(trg_stats)) {
            sel_analyzed->u.ptr.num_legal_trg_types++;
            if(sel_analyzed->u.ptr.first_legal_trg_type == -1) {
                sel_analyzed->u.ptr.first_legal_trg_type = i;
            }
        }
    }

    /* Set global stats. */
    for(i=0;i<sel_analyzed->u.ptr.num_trg_types;i++) {
        trg_stats = sel_analyzed->u.ptr.trg_stats[i];
        if(trg_stats == _badentry_found) {
            sel_stats->badentry_found++;
            return ret;
        }
        else if(trg_stats == _void_type_found) {
            sel_stats->void_type_found++;
            return ret;
        }
    }
    for(i=0;i<sel_analyzed->u.ptr.num_trg_types;i++) {
        trg_stats = sel_analyzed->u.ptr.trg_stats[i];
        if(trg_stats == _ptr_type_found) {
            sel_stats->ptr_type_found++;
            return ret;
        }
    }
    for(i=0;i<sel_analyzed->u.ptr.num_trg_types;i++) {
        trg_stats = sel_analyzed->u.ptr.trg_stats[i];
        if(trg_stats == _comp_trg_types_found) {
            sel_stats->comp_trg_types_found++;
            return ret;
        }
    }
    sel_stats->other_types_found++;
    return ret;
}

/*===========================================================================*
 *                          magic_selement_recurse_ptr                       *
 *===========================================================================*/
PUBLIC int magic_selement_recurse_ptr(_magic_selement_t *selement,
    _magic_selement_t *new_selement, int max_steps)
{
    _magic_sel_stats_t sel_stats;
    _magic_sel_analyzed_t sel_analyzed;
    int steps = 0;

    if(selement->type->type_id != MAGIC_TYPE_POINTER) {
        return MAGIC_EINVAL;
    }

    *new_selement = *selement;
    while(1) {
        magic_selement_analyze_ptr(new_selement, MAGIC_SEL_ANALYZE_ALL,
            &sel_analyzed, &sel_stats);
        if(MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(&sel_analyzed)) {
            return steps;
        }
        *new_selement = sel_analyzed.u.ptr.trg_selements[0];
        steps++;
        if(new_selement->type->type_id != MAGIC_TYPE_POINTER || (max_steps > 0 && steps >= max_steps)) {
            break;
        }
    }

    return steps;
}

/*===========================================================================*
 *                         magic_sentry_analyze_ptr_trg_cb                   *
 *===========================================================================*/
PRIVATE int magic_sentry_analyze_ptr_trg_cb(const struct _magic_type *trg_parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type *trg_type, const unsigned offset, int depth, void *cb_args)
{
    void **args_array = (void **) cb_args;
    _magic_sel_analyzed_t *sel_analyzed = (_magic_sel_analyzed_t *) args_array[3];
    _magic_selement_t *sel;
    char *trg_address;
    int analysis_flags = (*(int *)(args_array[4]));

    if (trg_type->type_id == MAGIC_TYPE_ARRAY) {
        /* Skip arrays. */
        return MAGIC_TYPE_WALK_CONTINUE;
    }

    if (!sel_analyzed->u.ptr.trg_flags) {
        /* Set trg flags and offset only the first time. */
        struct _magic_dsentry **trg_dsentry = (struct _magic_dsentry **) args_array[0];
        struct _magic_dfunction **trg_dfunction = (struct _magic_dfunction **) args_array[1];
        int flags;
        if (*trg_dsentry) {
            assert(!(*trg_dfunction));
            flags = MAGIC_DSENTRY_TO_SENTRY(*trg_dsentry)->flags;
            if (flags & MAGIC_STATE_DYNAMIC) {
                assert(!(flags & MAGIC_STATE_DATA) || (flags & MAGIC_STATE_LIB));
                assert(MAGIC_STATE_REGION(MAGIC_DSENTRY_TO_SENTRY(*trg_dsentry)));
            }
            else {
                assert((flags & MAGIC_STATE_DATA) && !(flags & MAGIC_STATE_LIB));
            }
        }
        else {
            assert(*trg_dfunction);
            flags = MAGIC_DFUNCTION_TO_FUNCTION(*trg_dfunction)->flags;
            assert(flags & MAGIC_STATE_TEXT);
        }
        sel_analyzed->u.ptr.trg_flags = flags;
        if (analysis_flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)
            sel_analyzed->u.ptr.trg_flags |= MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS;
        sel_analyzed->u.ptr.trg_offset = offset;
    }

    /* Add target types. */
    trg_address = MAGIC_SEL_ANALYZED_PTR_TRG_ADDRESS(sel_analyzed);
    assert(trg_address);
    sel = &sel_analyzed->u.ptr.trg_selements[sel_analyzed->u.ptr.num_trg_types];
    if (!(analysis_flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)) {
        sel->sentry = MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(sel_analyzed) ? MAGIC_DSENTRY_TO_SENTRY(&sel_analyzed->u.ptr.trg.dsentry) : NULL;
    } else {
        sel->sentry = MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(sel_analyzed) ? MAGIC_DSENTRY_TO_SENTRY(sel_analyzed->u.ptr.trg_p.dsentry) : NULL;
    }
    sel->parent_type = trg_parent_type;
    sel->parent_address = trg_address + parent_offset;
    sel->child_num = child_num;
    sel->type = trg_type;
    sel->address = trg_address + offset;
    sel_analyzed->u.ptr.num_trg_types++;

    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                        magic_selement_analyze_ptr                         *
 *===========================================================================*/
PUBLIC int magic_selement_analyze_ptr(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    const struct _magic_type *ptr_type = selement->type;
    short is_ptr_el = ptr_type->type_id == MAGIC_TYPE_POINTER;
    int r, ret = 0;

    sel_analyzed->type_id = 0;
    if (!is_ptr_el) {
        /* Nothing to do. */
        return 0;
    }
    memset(&sel_analyzed->u.ptr, 0, sizeof(sel_analyzed->u.ptr));
    memset(sel_stats, 0, sizeof(_magic_sel_stats_t));

    if (flags & (MAGIC_SEL_ANALYZE_DATA | MAGIC_SEL_ANALYZE_INVARIANTS | MAGIC_SEL_ANALYZE_VIOLATIONS)) {
        /* Analyze data first. */
        void *value = magic_selement_to_ptr(selement);
        sel_analyzed->type_id = MAGIC_TYPE_POINTER;
        sel_analyzed->u.ptr.value = value;
        ret |= MAGIC_SEL_FOUND_DATA;
        if (value == NULL) {
            /* Null pointer. */
            MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_NULL_ENTRY);
        }
        else {
            /* Check target. */
            struct _magic_dsentry *trg_dsentry_ptr;
            struct _magic_dfunction *trg_dfunction_ptr;
            void *args_array[5];
            if (!(flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)) {
                trg_dsentry_ptr = &sel_analyzed->u.ptr.trg.dsentry;
                trg_dfunction_ptr = &sel_analyzed->u.ptr.trg.dfunction;
                args_array[0] = &trg_dsentry_ptr;
                args_array[1] = &trg_dfunction_ptr;
            } else {
                args_array[0] = &sel_analyzed->u.ptr.trg_p.dsentry;
                args_array[1] = &sel_analyzed->u.ptr.trg_p.dfunction;
            }
            args_array[2] = selement;
            args_array[3] = sel_analyzed;
            args_array[4] = &flags;
            r = magic_type_target_walk(value, args_array[0], args_array[1],
                magic_sentry_analyze_ptr_trg_cb, args_array);
            if (r == MAGIC_ENOENT) {
                MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_ENTRY_NOT_FOUND);
                sel_analyzed->u.ptr.trg_flags = magic_range_lookup_by_addr(value, NULL);
            }
            else if (r == MAGIC_EBADENT) {
                MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_BAD_ENTRY);
                sel_analyzed->u.ptr.trg_flags = magic_range_lookup_by_addr(value, NULL);
            }
            else if (sel_analyzed->u.ptr.num_trg_types == 0) {
                MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(sel_analyzed, MAGIC_TYPE_BAD_OFFSET);
                sel_analyzed->u.ptr.trg_flags = magic_range_lookup_by_addr(value, NULL);
            }
        }

        if (flags & (MAGIC_SEL_ANALYZE_INVARIANTS | MAGIC_SEL_ANALYZE_VIOLATIONS)) {
            /* Check value-based invariants. */
            ret |= magic_selement_analyze_ptr_value_invs(selement,
                sel_analyzed, sel_stats);

            /* Check target-based invariants. */
            if (!(ret & MAGIC_SEL_FOUND_INVARIANTS)) {
                ret |= magic_selement_analyze_ptr_trg_invs(selement,
                    sel_analyzed, sel_stats);
            }

            /* Check type-based invariants. */
            if (!(ret & MAGIC_SEL_FOUND_INVARIANTS)) {
                ret |= magic_selement_analyze_ptr_type_invs(selement,
                    sel_analyzed, sel_stats);
            }

            assert(ret & MAGIC_SEL_FOUND_INVARIANTS);
            sel_stats->ptr_found++;
            if (MAGIC_SEL_STATS_HAS_VIOLATIONS(sel_stats)) {
                ret |= MAGIC_SEL_FOUND_VIOLATIONS;
            }
        }
    }

    sel_analyzed->flags = ret;
    return ret;
}

/*===========================================================================*
 *                  magic_selement_analyze_nonptr_value_invs                 *
 *===========================================================================*/
PRIVATE int magic_selement_analyze_nonptr_value_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    const struct _magic_type* type = selement->type;
    int i, ret = 0;
    int value = sel_analyzed->u.nonptr.value;

    if(MAGIC_STATE_FLAG(selement->sentry, MAGIC_STATE_EXTERNAL))
        return 0;

    if(MAGIC_TYPE_HAS_VALUE_SET(type)) {
        i=0;
        ret |= MAGIC_SEL_FOUND_INVARIANTS;
        while(MAGIC_TYPE_HAS_VALUE(type, i)) {
            int trg_value = MAGIC_TYPE_VALUE(type, i);
            if(trg_value == value) {
                sel_stats->value_found++;
                break;
            }
            i++;
        }
        if(!MAGIC_TYPE_HAS_VALUE(type, i)) {
            sel_stats->badvalue_found++;
        }
    }

    return ret;
}

/*===========================================================================*
 *                       magic_selement_analyze_nonptr                       *
 *===========================================================================*/
PUBLIC int magic_selement_analyze_nonptr(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    int ret = 0;

    sel_analyzed->type_id = 0;
    memset(&sel_analyzed->u.nonptr, 0, sizeof(sel_analyzed->u.nonptr));
    memset(sel_stats, 0, sizeof(_magic_sel_stats_t));

    if(flags & (MAGIC_SEL_ANALYZE_DATA|MAGIC_SEL_ANALYZE_INVARIANTS|MAGIC_SEL_ANALYZE_VIOLATIONS)) {
        /* Analyze data first. */
        switch(selement->type->type_id) {
            case MAGIC_TYPE_VOID:
                sel_analyzed->type_id = MAGIC_TYPE_VOID;
                sel_analyzed->u.nonptr.value = (long) *((char*)selement->address);
                ret |= MAGIC_SEL_FOUND_DATA;
            break;

            case MAGIC_TYPE_FLOAT:
                sel_analyzed->type_id = MAGIC_TYPE_FLOAT;
                sel_analyzed->u.nonptr.value = (long) magic_selement_to_float(selement);
                ret |= MAGIC_SEL_FOUND_DATA;
            break;

            case MAGIC_TYPE_INTEGER:
            case MAGIC_TYPE_ENUM:
                sel_analyzed->type_id = selement->type->type_id;
                sel_analyzed->u.nonptr.value = magic_selement_to_int(selement);
                ret |= MAGIC_SEL_FOUND_DATA;
                if((flags & MAGIC_SEL_ANALYZE_LIKELYPOINTERS) && selement->type->size == sizeof(void*)) {
                    sel_analyzed->u.nonptr.trg_flags = magic_range_lookup_by_addr((void*) sel_analyzed->u.nonptr.value, NULL);
                }
                if(flags & (MAGIC_SEL_ANALYZE_INVARIANTS|MAGIC_SEL_ANALYZE_VIOLATIONS)) {
                    /* Check value-based invariants. */
                    ret |= magic_selement_analyze_nonptr_value_invs(selement,
                        sel_analyzed, sel_stats);

                    if(ret & MAGIC_SEL_FOUND_INVARIANTS) {
                        sel_stats->nonptr_found++;
                        if(MAGIC_SEL_STATS_HAS_VIOLATIONS(sel_stats)) {
                            ret |= MAGIC_SEL_FOUND_VIOLATIONS;
                        }
                    }
                    else {
                        sel_stats->nonptr_unconstrained_found++;
                    }
                }
            break;
        }
    }

    sel_analyzed->flags = ret;
    return ret;
}

/*===========================================================================*
 *                 magic_sel_analyzed_trg_selements_print                    *
 *===========================================================================*/
PUBLIC void magic_sel_analyzed_trg_selements_print(_magic_sel_analyzed_t *sel_analyzed,
    int flags)
{
    int num;
    int i=0;
    const _magic_selement_t* trg_selement;
    _magic_trg_stats_t trg_stats;

    num = sel_analyzed->u.ptr.num_trg_types;
    if(num == 0) {
        return;
    }
    _magic_printf("#%d|%d", num, sel_analyzed->u.ptr.num_legal_trg_types);

    for(;i<num;i++) {
        trg_selement = &sel_analyzed->u.ptr.trg_selements[i];
        trg_stats = sel_analyzed->u.ptr.trg_stats[i];
        _magic_printf("%s%d|%c=", (i==0 ? ": " : ", "), i+1, MAGIC_SEL_ANALYZED_TRG_STATS_C(trg_stats));
        MAGIC_SELEMENT_PRINT(trg_selement, flags|MAGIC_SKIP_COMP_TYPES);
    }
}

/*===========================================================================*
 *                        magic_selement_type_cast                           *
 *===========================================================================*/
PUBLIC _magic_selement_t* magic_selement_type_cast(
    _magic_selement_t *selement, int flags, const struct _magic_type* type,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats)
{
    _magic_sel_stats_t my_sel_stats;

    selement->type = type;
    if(sel_analyzed) {
        magic_selement_analyze(selement, flags,
            sel_analyzed, sel_stats ? sel_stats : &my_sel_stats);
    }
    return selement;
}


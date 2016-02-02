#include <magic_sentry.h>
#include <magic_splay_tree.h>

/*===========================================================================*
 *                         magic_sentry_get_off_by_n                         *
 *===========================================================================*/
PUBLIC long magic_sentry_get_off_by_n(struct _magic_sentry *sentry,
    void *addr, int flags)
{
    char *el_addr = (char*) addr, *first_el_addr, *last_el_addr;
    size_t el_size;
    unsigned long n, diff;
    long long_n;

    if (sentry->type->type_id != MAGIC_TYPE_ARRAY) {
        return LONG_MAX;
    }
    el_size = sentry->type->contained_types[0]->size;
    first_el_addr = (char*) sentry->address;
    last_el_addr = (first_el_addr+sentry->type->size-el_size);
    if (el_addr >= last_el_addr) {
        diff = (unsigned long) (el_addr - last_el_addr);
    }
    else if (el_addr <= first_el_addr) {
        diff = (unsigned long) (first_el_addr - el_addr);
    }
    else {
        return LONG_MAX;
    }
    if (diff % el_size != 0) {
        return LONG_MAX;
    }
    n = diff / el_size;
    if (n >= LONG_MAX) {
        return LONG_MAX;
    }
    long_n = (el_addr >= last_el_addr ? (long) n : -((long)n));
    if ((long_n < 0 && !(flags & MAGIC_SENTRY_OFF_BY_N_NEGATIVE))
        || (long_n > 0 && !(flags & MAGIC_SENTRY_OFF_BY_N_POSITIVE))
        || (long_n == 0 && !(flags & MAGIC_SENTRY_OFF_BY_N_ZERO))) {
        return LONG_MAX;
    }
    return long_n;
}

/*===========================================================================*
 *                           magic_do_check_sentry                           *
 *===========================================================================*/
PRIVATE INLINE int magic_do_check_sentry(struct _magic_sentry *sentry)
{
    int is_size_ok;
    assert(sentry && "NULL sentry found!");
    is_size_ok = sentry->type->size > 0;
    if (!is_size_ok) {
        _magic_printf("magic_do_check_sentry: bad sentry, checks: %d\n", is_size_ok);
        MAGIC_SENTRY_PRINT(sentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        return FALSE;
    }
    return TRUE;
}

/*===========================================================================*
 *                             magic_check_sentry                            *
 *===========================================================================*/
PUBLIC int magic_check_sentry(struct _magic_sentry *sentry)
{
    int check;
    check = magic_do_check_sentry(sentry);
    if (!check) {
        return FALSE;
    }

#if MAGIC_CHECK_LEVEL == 2
    check = magic_check_sentries();
    if (!check) {
        _magic_printf("magic_check_sentry: bad other sentry\n");
        return FALSE;
    }
#endif

    return TRUE;
}

/*===========================================================================*
 *                          magic_check_sentries                             *
 *===========================================================================*/
PUBLIC int magic_check_sentries()
{
    int i, ret, check = TRUE;

    for (i = 0 ; i < _magic_sentries_num ; i++) {
        ret = magic_do_check_sentry(&_magic_sentries[i]);
        if (ret == FALSE) {
            check = FALSE;
        }
    }

    return check;
}

/*===========================================================================*
 *                         magic_sentry_lookup_by_id                         *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_id(_magic_id_t id,
    struct _magic_dsentry *dsentry_buff)
{
    struct _magic_sentry *entry = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;

    if (id <= 0) {
        return NULL;
    }

    /* O(1) ID lookup for sentries. */
#if MAGIC_LOOKUP_SENTRY
    if ((int)id <= _magic_sentries_num) {
        return &_magic_sentries[id - 1];
    }
#endif

    /* O(N) ID lookup for dsentries. */
#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_NESTED_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        if(sentry->id == id) {
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
#endif

    return entry;
}

/*===========================================================================*
 *                        magic_sentry_lookup_by_addr                        *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_addr(void *addr,
    struct _magic_dsentry *dsentry_buff)
{
    int i;
    struct _magic_sentry *entry = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
    if (magic_sentry_rl_index) {
        sentry = magic_sentry_lookup_by_range_index(addr, dsentry_buff);
        if (sentry && sentry->address == addr) {
            return sentry;
        } else {
            return NULL;
        }
    }
#endif

    /* Scan all the entries and return the one matching the provided address. */
#if MAGIC_LOOKUP_SENTRY
    if (MAGIC_ADDR_IS_IN_RANGE(addr, magic_sentry_range)) {
        for (i = 0 ; i < _magic_sentries_num ; i++) {
            if (_magic_sentries[i].address == addr) {
                entry = &_magic_sentries[i];
                break;
            }
        }
        if (entry) {
            return entry;
        }
    }
#endif

#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    if (!MAGIC_ADDR_LOOKUP_USE_DSENTRY_RANGES || magic_range_is_dsentry(addr)) {
        MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
            if (sentry->address == addr) {
                if (dsentry_buff) {
                    magic_copy_dsentry(dsentry, dsentry_buff);
                    entry = MAGIC_DSENTRY_TO_SENTRY(dsentry_buff);
                }
                else {
                    entry = sentry;
                }
                break;
            }
        );
    }
    MAGIC_DSENTRY_UNLOCK();
#endif

    return entry;
}

/*===========================================================================*
 *                       magic_sentry_lookup_by_name                         *
 *===========================================================================*/
PUBLIC struct _magic_sentry *
    magic_sentry_lookup_by_name(const char *parent_name, const char *name,
    _magic_id_t site_id, struct _magic_dsentry *dsentry_buff)
{
    int i;
    struct _magic_sentry *entry = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;

#if MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH
    if (magic_sentry_hash_head) {
        return magic_sentry_lookup_by_name_hash(parent_name, name,
            site_id, dsentry_buff);
    }
#endif

    /* Scan all the entries and return the one matching the provided name. */
#if MAGIC_LOOKUP_SENTRY
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        if (!strcmp(_magic_sentries[i].name, name)) {
            if (!parent_name ||
                    !strcmp(MAGIC_SENTRY_PARENT(&_magic_sentries[i]),
                    parent_name)) {
                if (MAGIC_SENTRY_SITE_ID(&_magic_sentries[i]) == site_id) {
                    entry = &_magic_sentries[i];
                    break;
                }
            }
        }
    }
    if (entry) {
        return entry;
    }
#endif

#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry,
        dsentry, sentry,
        if (!strcmp(sentry->name, name)) {
            if (!parent_name ||
                    !strcmp(MAGIC_SENTRY_PARENT(sentry), parent_name)) {
                if (site_id == MAGIC_DSENTRY_SITE_ID_NULL ||
                        dsentry->site_id == site_id) {
                    if (dsentry_buff) {
                        magic_copy_dsentry(dsentry, dsentry_buff);
                        entry = MAGIC_DSENTRY_TO_SENTRY(dsentry_buff);
                    }
                    else {
                        entry = sentry;
                    }
                    break;
                }
            }
        }
    );
    MAGIC_DSENTRY_UNLOCK();
#endif

    return entry;
}

/*===========================================================================*
 *                        magic_sentry_lookup_by_range                       *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_range(void *addr,
    struct _magic_dsentry *dsentry_buff)
{
    int i;
    struct _magic_sentry *entry = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    void *start_address, *end_address;

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
    if (magic_sentry_rl_index) {
        return magic_sentry_lookup_by_range_index(addr, dsentry_buff);
    }
#endif

    /* Scan all the entries and return the one with a valid range. */
#if MAGIC_LOOKUP_SENTRY
    if (MAGIC_ADDR_IS_IN_RANGE(addr, magic_sentry_range)) {
        for (i = 0 ; i < _magic_sentries_num ; i++) {
            start_address = _magic_sentries[i].address;
            end_address = (void *) (((char *)_magic_sentries[i].address) +
                _magic_sentries[i].type->size - 1);
            if (MAGIC_ADDR_IS_WITHIN(addr, start_address, end_address)) {
                entry = &_magic_sentries[i];
                break;
            }
        }
        if (entry) {
            return entry;
        }
    }
#endif

#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    if (!MAGIC_ADDR_LOOKUP_USE_DSENTRY_RANGES || magic_range_is_dsentry(addr)) {
        MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
            start_address = sentry->address;
            end_address = (void *) (((char *)sentry->address) +
                sentry->type->size - 1);
            if (MAGIC_ADDR_IS_WITHIN(addr, start_address, end_address)) {
                if (dsentry_buff) {
                    magic_copy_dsentry(dsentry, dsentry_buff);
                    entry = MAGIC_DSENTRY_TO_SENTRY(dsentry_buff);
                }
                else {
                    entry = sentry;
                }
                break;
            }
        );
    }
    MAGIC_DSENTRY_UNLOCK();
#endif

    return entry;
}

/*===========================================================================*
 *                    magic_sentry_lookup_by_min_off_by_n                    *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_min_off_by_n(void *addr,
    int flags, long *min_n_ptr, struct _magic_dsentry *dsentry_buff)
{
    int i;
    struct _magic_sentry *entry = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    long n, abs_n, min_n, min_abs_n = LONG_MAX;
    int has_multiple_min_entries = FALSE;

    /* Scan all the entries and return the one with the minimum off-by-n. */
#if MAGIC_LOOKUP_SENTRY
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        n = magic_sentry_get_off_by_n(&_magic_sentries[i], addr, flags);
        abs_n = MAGIC_ABS(n);
        if (n == LONG_MAX || abs_n > min_abs_n) {
            continue;
        }
        if (abs_n == min_abs_n) {
            has_multiple_min_entries = TRUE;
        }
        else {
            min_abs_n = abs_n;
            min_n = n;
            has_multiple_min_entries = FALSE;
            entry = &_magic_sentries[i];
        }
    }
#endif

#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        n = magic_sentry_get_off_by_n(sentry, addr, flags);
        abs_n = MAGIC_ABS(n);
        if (n == LONG_MAX || abs_n > min_abs_n) {
            continue;
        }
        if (abs_n == min_abs_n) {
            has_multiple_min_entries = TRUE;
        }
        else {
            min_abs_n = abs_n;
            min_n = n;
            has_multiple_min_entries = FALSE;
            if (dsentry_buff) {
                magic_copy_dsentry(dsentry, dsentry_buff);
                entry = MAGIC_DSENTRY_TO_SENTRY(dsentry_buff);
            }
            else {
                entry = sentry;
            }
        }
    );
    MAGIC_DSENTRY_UNLOCK();
#endif

    if (has_multiple_min_entries && (flags & MAGIC_SENTRY_OFF_BY_N_NO_DUPLICATES)) {
        entry = NULL;
    }
    if (entry && min_n_ptr) {
        *min_n_ptr = min_n;
    }
    return entry;
}

/*===========================================================================*
 *                       magic_sentry_lookup_by_string                       *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_string(const char *string)
{
    int i;
    struct _magic_sentry *entry = NULL;

    /* Scan all the string entries and return the matching one. */
#if MAGIC_LOOKUP_SENTRY
    for(i = 0 ; i < _magic_sentries_num ; i++) {
        if (MAGIC_STATE_FLAG(&_magic_sentries[i], MAGIC_STATE_STRING)
            && !strcmp((char *)_magic_sentries[i].address, string)) {
            entry = &_magic_sentries[i];
            break;
        }
    }
#endif

    return entry;
}

/*===========================================================================*
 *                          magic_print_sentry                               *
 *===========================================================================*/
PUBLIC void magic_print_sentry(struct _magic_sentry *sentry)
{
    MAGIC_SENTRY_PRINT(sentry, MAGIC_EXPAND_TYPE_STR);
}

/*===========================================================================*
 *                      magic_print_sentry_abs_name                          *
 *===========================================================================*/
PUBLIC void magic_print_sentry_abs_name(struct _magic_sentry *sentry)
{
    if (!(sentry->flags & MAGIC_STATE_DYNAMIC)) {
        _magic_printf(sentry->name);
    }
    else {
        struct _magic_dsentry *dsentry = MAGIC_DSENTRY_FROM_SENTRY(sentry);
        assert(dsentry->parent_name && strcmp(dsentry->parent_name, ""));
        assert(sentry->name);
        assert(strcmp(sentry->name, ""));
        _magic_printf("%lu%s%s%s%s%s" MAGIC_ID_FORMAT, (unsigned long)MAGIC_SENTRY_ID(sentry),
            MAGIC_DSENTRY_ABS_NAME_SEP, dsentry->parent_name,
            MAGIC_DSENTRY_ABS_NAME_SEP, sentry->name,
            MAGIC_DSENTRY_ABS_NAME_SEP, dsentry->site_id);
    }
}

/*===========================================================================*
 *                         magic_print_sentries                              *
 *===========================================================================*/
PUBLIC void magic_print_sentries()
{
    int i;
    struct _magic_sentry* sentry;

    _magic_printf("magic_print_sentries: Printing %d entries\n", _magic_sentries_num);
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        sentry = &_magic_sentries[i];
        MAGIC_SENTRY_PRINT(sentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }
}

/*===========================================================================*
 *                       magic_print_nonstr_sentries                         *
 *===========================================================================*/
PUBLIC void magic_print_nonstr_sentries()
{
    int i;
    struct _magic_sentry *sentry;

    _magic_printf("magic_print_nonstr_sentries: Printing %d/%d non-string entries\n",
        _magic_sentries_num - _magic_sentries_str_num, _magic_sentries_num);
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        sentry = &_magic_sentries[i];
        if (MAGIC_SENTRY_IS_STRING(sentry)) {
            continue;
        }
        MAGIC_SENTRY_PRINT(sentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
    }
}

/*===========================================================================*
 *                         magic_print_str_sentries                          *
 *===========================================================================*/
PUBLIC void magic_print_str_sentries()
{
    int i;
    struct _magic_sentry *sentry;

    _magic_printf("magic_print_str_sentries: Printing %d/%d string entries\n",
        _magic_sentries_str_num, _magic_sentries_num);
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        sentry = &_magic_sentries[i];
        if (!MAGIC_SENTRY_IS_STRING(sentry)) {
            continue;
        }
        MAGIC_SENTRY_PRINT(sentry, MAGIC_EXPAND_TYPE_STR);
        _magic_printf(", string=\"%s\"\n", (char*)sentry->address);
    }
}

/*===========================================================================*
 *                           magic_sentry_rl_alloc                           *
 *===========================================================================*/
PRIVATE void *magic_sentry_rl_alloc(int size, UNUSED(void *data))
{
    void *addr;

    assert(magic_sentry_rl_buff);
    assert(magic_sentry_rl_buff_offset + size <= magic_sentry_rl_buff_size);

    addr = (char*) magic_sentry_rl_buff + magic_sentry_rl_buff_offset;
    magic_sentry_rl_buff_offset += size;

    return addr;
}

/*===========================================================================*
 *                          magic_sentry_rl_dealloc                          *
 *===========================================================================*/
PRIVATE void magic_sentry_rl_dealloc(UNUSED(void *object), UNUSED(void *data))
{
    return;
}

/*===========================================================================*
 *                         magic_sentry_rl_build_index                       *
 *===========================================================================*/
PUBLIC void magic_sentry_rl_build_index(void *buff, size_t buff_size)
{
/*
 * Warning: this implementation is thread unsafe and also makes
 *              magic_sentry_lookup_by_range thread unsafe!
 */
    int i;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    void *start_address;
    splay_tree index;

    assert(buff && buff_size > 0);
    magic_sentry_rl_buff = buff;
    magic_sentry_rl_buff_offset = 0;
    magic_sentry_rl_buff_size = buff_size;
    index = splay_tree_new_with_allocator(
        splay_tree_compare_pointers,
        NULL, NULL,
        magic_sentry_rl_alloc, magic_sentry_rl_dealloc,
        NULL);
    magic_sentry_rl_index = index;
    assert(magic_sentry_rl_index);

    /* Add all the sentries to the index. */
#if MAGIC_LOOKUP_SENTRY
    for (i = 0 ; i < _magic_sentries_num ; i++) {
        start_address = _magic_sentries[i].address;
        sentry = &_magic_sentries[i];
        magic_sentry_rl_insert(start_address, sentry);
    }
#endif

    /* Add all the dsentries to the index. */
#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        start_address = sentry->address;
        magic_sentry_rl_insert(start_address, sentry);
    );
    MAGIC_DSENTRY_UNLOCK();
#endif
}

/*===========================================================================*
 *                        magic_sentry_rl_destroy_index                      *
 *===========================================================================*/
PUBLIC void magic_sentry_rl_destroy_index(void)
{
    magic_sentry_rl_buff = NULL;
    magic_sentry_rl_buff_offset = 0;
    magic_sentry_rl_buff_size = 0;
    magic_sentry_rl_index = NULL;
}

/*===========================================================================*
 *                    magic_sentry_rl_estimate_index_buff_size               *
 *===========================================================================*/
PUBLIC size_t magic_sentry_rl_estimate_index_buff_size(int sentries_num)
{
    if (sentries_num == 0) {
        MAGIC_DSENTRY_ALIVE_BLOCK_NUM(_magic_first_dsentry, &sentries_num);
        sentries_num += _magic_sentries_num;
    }

    return (sentries_num * sizeof(struct splay_tree_node_s)) +
        (sizeof(struct splay_tree_s) * 2);
}

/*===========================================================================*
 *                       magic_sentry_rl_count_index_cb                      *
 *===========================================================================*/
PRIVATE int magic_sentry_rl_count_index_cb(splay_tree_node node, void *data)
{
    size_t *count = (size_t *) data;

    (*count)++;
    return 0;
}

/*===========================================================================*
 *                       magic_sentry_rl_print_index_cb                      *
 *===========================================================================*/
PRIVATE int magic_sentry_rl_print_index_cb(splay_tree_node node, void *data)
{
    _magic_printf("NODE<key, value>: <%08x, %08x>\n", (unsigned int) node->key,
        (unsigned int) node->value);
    return 0;
}

/*===========================================================================*
 *                         magic_sentry_rl_print_index                       *
 *===========================================================================*/
PUBLIC void magic_sentry_rl_print_index(void)
{
    size_t num_nodes = 0;
    assert(magic_sentry_rl_index);

    splay_tree_foreach((splay_tree) magic_sentry_rl_index,
        magic_sentry_rl_count_index_cb, &num_nodes);
    _magic_printf("magic_sentry_rl_print_index: Found %d nodes:\n", num_nodes);
    splay_tree_foreach((splay_tree) magic_sentry_rl_index,
        magic_sentry_rl_print_index_cb, NULL);
}

/*===========================================================================*
 *                          magic_sentry_rl_lookup                           *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_rl_lookup(void *start_addr)
{
    splay_tree_node node;
    struct _magic_sentry *sentry = NULL;

    node = splay_tree_lookup((splay_tree) magic_sentry_rl_index,
        (splay_tree_key) start_addr);
    if (node) {
        sentry = (struct _magic_sentry*) node->value;
    }

    return sentry;
}

/*===========================================================================*
 *                           magic_sentry_rl_insert                          *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_rl_insert(void *start_addr,
    struct _magic_sentry *sentry)
{
    if (!splay_tree_lookup((splay_tree) magic_sentry_rl_index,
        (splay_tree_key) start_addr)) {
        splay_tree_insert((splay_tree) magic_sentry_rl_index,
            (splay_tree_key) start_addr,
            (splay_tree_value) sentry);
    }
    else {
        sentry = NULL;
    }

    return sentry;
}

/*===========================================================================*
 *                        magic_sentry_rl_pred_lookup                        *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_rl_pred_lookup(void *addr)
{
    splay_tree_node node;
    struct _magic_sentry *sentry = NULL;

    node = splay_tree_predecessor((splay_tree) magic_sentry_rl_index,
        (splay_tree_key) addr);
    if (node) {
        sentry = (struct _magic_sentry*) node->value;
    }

    return sentry;
}

/*===========================================================================*
 *                      magic_sentry_lookup_by_range_index                   *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_range_index(
    void *addr, struct _magic_dsentry *dsentry_buff)
{
    /*
     * Warning: this implementation is thread unsafe!
     */
    void *start_address, *end_address;
    struct _magic_sentry *sentry =
        magic_sentry_rl_pred_lookup((char *)addr + 1);

    if (sentry) {
        start_address = sentry->address;
        end_address = (void *) (((char *)start_address) +
            sentry->type->size - 1);
        if (!MAGIC_ADDR_IS_WITHIN(addr, start_address, end_address)) {
            sentry = NULL;
        } else {
            if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_DYNAMIC) &&
                dsentry_buff != NULL) {
                magic_copy_dsentry(MAGIC_DSENTRY_FROM_SENTRY(sentry),
                    dsentry_buff);
            }
        }
    }

    return sentry;
}

/*===========================================================================*
 *                         magic_sentry_hash_insert                          *
 *===========================================================================*/
PRIVATE void magic_sentry_hash_insert(struct _magic_sentry_hash **head,
    struct _magic_sentry_hash *elem)
{
    if (head != NULL) {
        struct _magic_sentry_hash *tmp;
        HASH_FIND_STR(*head, elem->key, tmp);
        if (tmp) {
            LL_APPEND(tmp->sentry_list, elem->sentry_list);
            return;
        }
    }
/*
 * **** START UTHASH SPECIFIC DEFINITIONS ****
 */
#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(size)             magic_sentry_hash_alloc(size)
#define uthash_free(addr, size)         magic_sentry_hash_dealloc(addr, size)
/*
 * Since we have a limited buffer, we need to stop bucket expansion when
 * reaching a certain limit.
 */
#undef uthash_expand_fyi
#define uthash_expand_fyi(tbl)                                                 \
    do {                                                                       \
        if (tbl->num_buckets == MAGIC_SENTRY_NAME_EST_MAX_BUCKETS) {           \
            _magic_printf("Warning! Sentry name hash maximum bucket number "   \
                "reached! Consider increasing "                                \
                "MAGIC_SENTRY_NAME_EST_MAX_BUCKETS, unless you are comfortable"\
                " with the current performance.\n");                           \
            tbl->noexpand = 1;                                                 \
        }                                                                      \
    } while(0);
/*
 * **** FINISH UTHASH SPECIFIC DEFINITIONS ****
 */
    HASH_ADD_STR(*head, key, elem);
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
 *                         magic_sentry_hash_build                           *
 *===========================================================================*/
PUBLIC void magic_sentry_hash_build(void *buff, size_t buff_size)
{
    /*
     * XXX:
     * Warning: this implementation is thread unsafe and also makes
     * magic_sentry_lookup_by_name thread unsafe!
     */
    int i;
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    struct _magic_sentry_hash *sentry_hash, *head;
    struct _magic_sentry_list *sentry_list;

    assert(buff && buff_size > 0);
    magic_sentry_hash_buff = buff;
    magic_sentry_hash_buff_offset = 0;
    magic_sentry_hash_buff_size = buff_size;

    head = NULL;

    /* Add all the sentries to the hash. */
#if MAGIC_LOOKUP_SENTRY
    for(i = 0 ; i < _magic_sentries_num ; i++) {
        sentry_hash = (struct _magic_sentry_hash *)
            magic_sentry_hash_alloc(sizeof(struct _magic_sentry_hash));
        sentry_list = (struct _magic_sentry_list *)
            magic_sentry_hash_alloc(sizeof(struct _magic_sentry_list));
        sentry = &_magic_sentries[i];
        MAGIC_SENTRY_TO_HASH_EL(sentry, sentry_hash, sentry_list);
        magic_sentry_hash_insert(&head, sentry_hash);
    }
#endif

    /* Add all the dsentries to the hash. */
#if MAGIC_LOOKUP_DSENTRY
    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        sentry_hash = (struct _magic_sentry_hash *)
            magic_sentry_hash_alloc(sizeof(struct _magic_sentry_hash));
        sentry_list = (struct _magic_sentry_list *)
            magic_sentry_hash_alloc(sizeof(struct _magic_sentry_list));
        MAGIC_DSENTRY_TO_HASH_EL(dsentry, sentry, sentry_hash, sentry_list);
        magic_sentry_hash_insert(&head, sentry_hash);
    );
    MAGIC_DSENTRY_UNLOCK();
#endif
    magic_sentry_hash_head = (void *)head;
    assert(magic_sentry_hash_head || (!_magic_sentries_num && _magic_first_dsentry == NULL));
}

/*===========================================================================*
 *                        magic_sentry_hash_destroy                          *
 *===========================================================================*/
PUBLIC void magic_sentry_hash_destroy(void)
{
    magic_sentry_hash_buff = NULL;
    magic_sentry_hash_buff_offset = 0;
    magic_sentry_hash_buff_size = 0;
    magic_sentry_hash_head = NULL;
}

/*===========================================================================*
 *                    magic_sentry_hash_estimate_buff_size                   *
 *===========================================================================*/
PUBLIC size_t magic_sentry_hash_estimate_buff_size(int sentries_num)
{
    if (sentries_num == 0) {
        MAGIC_DSENTRY_ALIVE_NUM(_magic_first_dsentry, &sentries_num);
        sentries_num += _magic_sentries_num;
    }

    return (sentries_num * (sizeof(struct _magic_sentry_hash) +
        sizeof(struct _magic_sentry_list))) + MAGIC_SENTRY_NAME_HASH_OVERHEAD;
}

/*===========================================================================*
 *                           magic_sentry_hash_alloc                         *
 *===========================================================================*/
PUBLIC void *magic_sentry_hash_alloc(size_t size)
{
    void *addr;

    assert(magic_sentry_hash_buff);
    assert(magic_sentry_hash_buff_offset + size <= magic_sentry_hash_buff_size);

    addr = (char *) magic_sentry_hash_buff + magic_sentry_hash_buff_offset;
    magic_sentry_hash_buff_offset += size;

    return addr;
}

/*===========================================================================*
 *                          magic_sentry_hash_dealloc                        *
 *===========================================================================*/
PUBLIC void magic_sentry_hash_dealloc(UNUSED(void *object), UNUSED(size_t sz))
{
    return;
}

/*===========================================================================*
 *                      magic_sentry_lookup_by_name_hash                     *
 *===========================================================================*/
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_name_hash(
    const char *parent_name, const char *name, _magic_id_t site_id,
    struct _magic_dsentry *dsentry_buff)
{
    /*
     * Warning: this implementation is thread unsafe!
     */
    char key[MAGIC_SENTRY_NAME_MAX_KEY_LEN];
    struct _magic_sentry_hash *res, *head;
    key[0] = 0;
    snprintf(key, sizeof(key), "%s%s%s%s" MAGIC_ID_FORMAT, parent_name,
        MAGIC_DSENTRY_ABS_NAME_SEP, name, MAGIC_DSENTRY_ABS_NAME_SEP, site_id);
    head = (struct _magic_sentry_hash *) magic_sentry_hash_head;

    HASH_FIND_STR(head, key, res);
    if (res == NULL)
        return NULL;

    return res->sentry_list->sentry;
}

/*===========================================================================*
 *                    magic_sentry_list_lookup_by_name_hash                  *
 *===========================================================================*/
PUBLIC struct _magic_sentry_list *magic_sentry_list_lookup_by_name_hash(
    const char *parent_name, const char *name, _magic_id_t site_id,
    struct _magic_dsentry *dsentry_buff)
{
    /*
     * Warning: this implementation is thread unsafe!
     */
    char key[MAGIC_SENTRY_NAME_MAX_KEY_LEN];
    struct _magic_sentry_hash *res, *head;
    key[0] = 0;
    snprintf(key, sizeof(key), "%s%s%s%s" MAGIC_ID_FORMAT, parent_name,
        MAGIC_DSENTRY_ABS_NAME_SEP, name, MAGIC_DSENTRY_ABS_NAME_SEP, site_id);
    head = (struct _magic_sentry_hash *) magic_sentry_hash_head;

    HASH_FIND_STR(head, key, res);
    if (res == NULL)
        return NULL;

    return res->sentry_list;
}


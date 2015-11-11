#include <magic_selement.h>

/*===========================================================================*
 *                      magic_selement_lookup_by_name                        *
 *===========================================================================*/
PUBLIC int magic_selement_lookup_by_name(char *name,
    _magic_selement_t *selement, struct _magic_dsentry *dsentry_buff)
{
    char token_buff[MAGIC_MAX_NAME_LEN * 2 + 2];
    char *token_start = name;
    int last_num_seps = 0;
    char *s;
    if (!name || *name == '\0' || *name == MAGIC_SELEMENT_SEP[0]) {
        return MAGIC_ENOENT;
    }
    for (s = name; *s; s++) {
        if (*(s + 1) == '\0' || *(s + 1) == MAGIC_SELEMENT_SEP[0]) {
            size_t len = s - token_start + 1;
            if (len >= MAGIC_MAX_NAME_LEN * 2 + 1) {
                return MAGIC_ENOMEM;
            }
            strncpy(token_buff, token_start, len);
            token_buff[len] = '\0';
            if (token_start == name) {
                struct _magic_sentry *sentry;
                const char *sentry_parent_name = "", *sentry_name = NULL;
                char *delim = NULL;
                _magic_id_t dsentry_site_id = MAGIC_DSENTRY_SITE_ID_NULL;
                if (!(delim = strchr(token_buff, MAGIC_DSENTRY_ABS_NAME_SEP[0]))) {
                    /* Regular sentry, no parent name or site_id. */
                    sentry_name = token_buff;
                } else {
                    /*
                     * Dsentry. Will contain: sentry_id<DELIM>parent_name<DELIM>name<DELIM>site_id.
                     */
                    *delim = '\0';
                    /* Skip sentry_id */
                    sentry_parent_name = delim + 1;
                    delim = strchr(delim + 1, MAGIC_DSENTRY_ABS_NAME_SEP[0]);
                    assert(!delim && "No dsentry name found in selement name!");
                    *delim = '\0';
                    sentry_name = delim + 1;
                    delim = strchr(delim + 1, MAGIC_DSENTRY_ABS_NAME_SEP[0]);
                    assert(!delim && "No dsentry site id found in selement name!");
                    *delim = '\0';
                    dsentry_site_id = strtoul((const char*)delim+1, NULL, 10);
                }

                sentry = magic_sentry_lookup_by_name(sentry_parent_name, sentry_name,
                    dsentry_site_id, dsentry_buff);
                if (!sentry) {
                    return MAGIC_ENOENT;
                }
                magic_selement_from_sentry(sentry, selement);
            }
            else {
                _magic_selement_t child_selement;
                if (!magic_selement_from_relative_name(selement, &child_selement, token_buff)) {
                    return MAGIC_ENOENT;
                }
                *selement = child_selement;
            }
            s++;
            last_num_seps = 0;
            while (*s == MAGIC_SELEMENT_SEP[0]) {
                s++;
                last_num_seps++;
            }
            token_start = s;
            s--;
        }
    }
    if (last_num_seps > 0 && selement->type->type_id == MAGIC_TYPE_POINTER) {
        int steps = magic_selement_recurse_ptr(selement, selement, last_num_seps);
        if(steps != last_num_seps) {
            return MAGIC_EINVAL;
        }
    }

    return 0;
}

/*===========================================================================*
 *                       magic_selement_name_print_cb                        *
 *===========================================================================*/
PUBLIC int magic_selement_name_print_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    _magic_selement_t *selement = (_magic_selement_t*) cb_args;
    struct _magic_sentry* sentry = selement->sentry;
    void *address = (char*)sentry->address + offset;
    void *range[2];
    MAGIC_RANGE_SET_MIN(range, address);
    MAGIC_RANGE_SET_MAX(range, (char*) address + type->size-1);
    if(!MAGIC_ADDR_IS_IN_RANGE(selement->address, range)) {
        return MAGIC_TYPE_WALK_SKIP_PATH;
    }
    if(address == sentry->address && type == sentry->type) {
        magic_print_sentry_abs_name(sentry);
    }
    else {
        short is_parent_array = parent_type->type_id == MAGIC_TYPE_ARRAY || parent_type->type_id == MAGIC_TYPE_VECTOR;
        if(is_parent_array) {
            _magic_printf("%s%d", MAGIC_SELEMENT_SEP, child_num);
        }
        else {
            _magic_printf("%s%s", MAGIC_SELEMENT_SEP, parent_type->member_names[child_num]);
        }
    }
    if(type->num_child_types == 0
        || (address == selement->address && type == selement->type)) {
        return MAGIC_TYPE_WALK_STOP;
    }
    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                       magic_selement_name_get_cb                          *
 *===========================================================================*/
PUBLIC int magic_selement_name_get_cb(const struct _magic_type *parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type *type,
    const unsigned offset, int depth, void *args_array)
{
    void **cb_args = (void **) args_array;

    _magic_selement_t *selement = (_magic_selement_t*) cb_args[0];
    char **buf = (char **) cb_args + 1;
    int count, *buf_size = (int *) cb_args + 2;

    short is_array = type->type_id == MAGIC_TYPE_ARRAY || type->type_id == MAGIC_TYPE_VECTOR;

    struct _magic_sentry *sentry = selement->sentry;
    void *address = (char *)sentry->address + offset;
    void *range[2];
    MAGIC_RANGE_SET_MIN(range, address);
    MAGIC_RANGE_SET_MAX(range, (char *) address + type->size - 1);
    if (!MAGIC_ADDR_IS_IN_RANGE(selement->address, range)) {
        return MAGIC_TYPE_WALK_SKIP_PATH;
    }

    if (address == sentry->address && type == sentry->type) {
        if (!(sentry->flags & MAGIC_STATE_DYNAMIC)) {
            count = snprintf(*buf, *buf_size, "%s", sentry->name);
            if (count >= *buf_size) return MAGIC_TYPE_WALK_STOP;    /* Buffer too small. */
            *buf += count;
            *buf_size -= count;
        } else {
            struct _magic_dsentry *dsentry = MAGIC_DSENTRY_FROM_SENTRY(sentry);
            assert(dsentry->parent_name && strcmp(dsentry->parent_name, ""));
            assert(sentry->name && strcmp(sentry->name, ""));
            count = snprintf(*buf, *buf_size, "%lu%s%s%s%s%s" MAGIC_ID_FORMAT,
                (unsigned long)MAGIC_SENTRY_ID(sentry), MAGIC_DSENTRY_ABS_NAME_SEP,
                dsentry->parent_name, MAGIC_DSENTRY_ABS_NAME_SEP, sentry->name,
                MAGIC_DSENTRY_ABS_NAME_SEP, dsentry->site_id);
            if (count >= *buf_size) return MAGIC_TYPE_WALK_STOP;    /* Buffer too small. */
            *buf += count;
            *buf_size -= count;
        }
    } else {
        short is_parent_array = parent_type->type_id == MAGIC_TYPE_ARRAY ||
            parent_type->type_id == MAGIC_TYPE_VECTOR;
        if (is_parent_array) {
            count = snprintf(*buf, *buf_size, "%s%d",
                MAGIC_SELEMENT_SEP, child_num);
            if (count >= *buf_size) return MAGIC_TYPE_WALK_STOP;    /* Buffer too small. */
            *buf += count;
            *buf_size -= count;
        } else {
            count = snprintf(*buf, *buf_size, "%s%s",
                MAGIC_SELEMENT_SEP, parent_type->member_names[child_num]);
            if (count >= *buf_size) return MAGIC_TYPE_WALK_STOP;    /* Buffer too small. */
            *buf += count;
            *buf_size -= count;
        }
    }

    if (type->num_child_types == 0
    || (address == selement->address && type == selement->type)
    || (is_array && address == selement->address && type == selement->parent_type)) {
        return MAGIC_TYPE_WALK_STOP;
    }

    return MAGIC_TYPE_WALK_CONTINUE;
}

/*===========================================================================*
 *                        magic_selement_print_value                         *
 *===========================================================================*/
PUBLIC void magic_selement_print_value(const _magic_selement_t *selement)
{
    int type_id = selement->type->type_id;
    unsigned size = selement->type->size;
    double dvalue;
    void *pvalue;
    unsigned long uvalue;
    long ivalue;
    char vvalue;
    switch(type_id) {
        case MAGIC_TYPE_FLOAT:
            dvalue = magic_selement_to_float(selement);
            _magic_printf("float(%d):%g", size, dvalue);
            _magic_printf("/%d", (long) dvalue);
        break;

        case MAGIC_TYPE_POINTER:
            pvalue = magic_selement_to_ptr(selement);
            _magic_printf("ptr:%08x", pvalue);
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(MAGIC_TYPE_FLAG(selement->type, MAGIC_TYPE_UNSIGNED)) {
                uvalue = magic_selement_to_unsigned(selement);
                _magic_printf("unsigned %s(%d):%u", type_id == MAGIC_TYPE_INTEGER ? "int" : "enum", size, uvalue);
            }
            else {
                ivalue = magic_selement_to_int(selement);
                _magic_printf("%s(%d):%d", type_id == MAGIC_TYPE_INTEGER ? "int" : "enum", size, ivalue);
            }
        break;

        case MAGIC_TYPE_VOID:
            vvalue = *((char*) selement->address);
            _magic_printf("void(%d):%d", size, vvalue);
        break;

        default:
            _magic_printf("???");
        break;
    }
}

/*===========================================================================*
 *                         magic_selement_to_unsigned                        *
 *===========================================================================*/
PUBLIC unsigned long magic_selement_to_unsigned(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    unsigned long value = 0;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_INTEGER
        || type->type_id == MAGIC_TYPE_ENUM);

    if(address == NULL)
	return 0;

    if(size == sizeof(unsigned char)) {
        value = (unsigned long) *((unsigned char*) address);
    }
    else if(size == sizeof(unsigned short)) {
        value = (unsigned long) *((unsigned short*) address);
    }
#ifdef MAGIC_LONG_LONG_SUPPORTED
    else if(size == sizeof(unsigned long long)) {
        value = (unsigned long) *((unsigned long long*) address);
    }
#endif
    else {
        assert(size == sizeof(unsigned long));
        value = *((unsigned long*) address);
    }

    return value;
}

/*===========================================================================*
 *                           magic_selement_to_int                           *
 *===========================================================================*/
PUBLIC long magic_selement_to_int(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    long value = 0;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_INTEGER
        || type->type_id == MAGIC_TYPE_ENUM);

    if(address == NULL)
	return 0;

    if(size == sizeof(char)) {
        value = (long) *((char*) address);
    }
    else if(size == sizeof(short)) {
        value = (long) *((short*) address);
    }
#ifdef MAGIC_LONG_LONG_SUPPORTED
    else if(size == sizeof(long long)) {
        value = (long) *((long long*) address);
    }
#endif
    else {
        assert(size == sizeof(long));
        value = *((long*) address);
    }

    return value;
}

#ifdef MAGIC_LONG_LONG_SUPPORTED
/*===========================================================================*
 *                            magic_selement_to_llu                          *
 *===========================================================================*/
PUBLIC unsigned long long magic_selement_to_llu(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    unsigned long long value;
    unsigned size = type->size;

    if(address == NULL)
	return 0;

    if (size == sizeof(unsigned long long))
        value = *((unsigned long long*) address);
    else
        value = (unsigned long long) magic_selement_to_unsigned(selement);
    return value;
}

/*===========================================================================*
 *                            magic_selement_to_ll                           *
 *===========================================================================*/
PUBLIC long long magic_selement_to_ll(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    long long value;
    unsigned size = type->size;

    if(address == NULL)
	return 0;

    if (size == sizeof(long long))
        value = *((long long*) address);
    else
        value = (long long) magic_selement_to_int(selement);
    return value;
}
#endif

/*===========================================================================*
 *                          magic_selement_to_float                          *
 *===========================================================================*/
PUBLIC double magic_selement_to_float(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    double value = 0.0;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_FLOAT);

    if(address == NULL)
	return 0;

    if(size == sizeof(float)) {
        value = (double) *((float*) address);
    }
#ifdef MAGIC_LONG_DOUBLE_SUPPORTED
    else if(size == sizeof(long double)) {
        value = (double) *((long double*) address);
    }
#endif
    else {
        assert(size == sizeof(double));
        value = *((double*) address);
    }

    return value;
}

/*===========================================================================*
 *                           magic_selement_to_ptr                           *
 *===========================================================================*/
PUBLIC void* magic_selement_to_ptr(const _magic_selement_t *selement)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    void* value = NULL;
    assert(type->type_id == MAGIC_TYPE_POINTER);

    if (!address)
        return NULL;
    value = *((void**) address);
    return value;
}

/*===========================================================================*
 *                         magic_selement_from_unsigned                      *
 *===========================================================================*/
PUBLIC void magic_selement_from_unsigned(const _magic_selement_t *selement, unsigned long value)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_INTEGER
        || type->type_id == MAGIC_TYPE_ENUM);

    /* Prevent a store to NULL. */
    if(address == NULL)
	return;

    if(size == sizeof(unsigned char)) {
        *((unsigned char*) address) = (unsigned char) value;
    }
    else if(size == sizeof(unsigned short)) {
        *((unsigned short*) address) = (unsigned short) value;
    }
#ifdef MAGIC_LONG_LONG_SUPPORTED
    else if(size == sizeof(unsigned long long)) {
        *((unsigned long long*) address) = (unsigned long long) value;
    }
#endif
    else {
        assert(size == sizeof(unsigned long));
        *((unsigned long*) address) = (unsigned long) value;
    }
}

/*===========================================================================*
 *                           magic_selement_from_int                         *
 *===========================================================================*/
PUBLIC void magic_selement_from_int(const _magic_selement_t *selement, long value)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_INTEGER
        || type->type_id == MAGIC_TYPE_ENUM);

    /* Prevent a store to NULL. */
    if(address == NULL)
	return;

    if(size == sizeof(char)) {
        *((char*) address) = (char) value;
    }
    else if(size == sizeof(short)) {
        *((short*) address) = (short) value;
    }
#ifdef MAGIC_LONG_LONG_SUPPORTED
    else if(size == sizeof(long long)) {
        *((long long*) address) = (long long) value;
    }
#endif
    else {
        assert(size == sizeof(long));
        *((long*) address) = (long) value;
    }
}

/*===========================================================================*
 *                          magic_selement_from_float                        *
 *===========================================================================*/
PUBLIC void magic_selement_from_float(const _magic_selement_t *selement, double value)
{
    void *address = selement->address;
    const struct _magic_type* type = selement->type;
    unsigned size = type->size;
    assert(size > 0);
    assert(type->type_id == MAGIC_TYPE_FLOAT);

    /* Prevent a store to NULL. */
    if(address == NULL)
	return;

    if(size == sizeof(float)) {
        *((float*) address) = (float) value;
    }
#ifdef MAGIC_LONG_DOUBLE_SUPPORTED
    else if(size == sizeof(long double)) {
        *((long double*) address) = (long double) value;
    }
#endif
    else {
        assert(size == sizeof(double));
        *((double*) address) = (double) value;
    }
}

/*===========================================================================*
 *                      magic_selement_ptr_value_cast                        *
 *===========================================================================*/
PUBLIC int magic_selement_ptr_value_cast(const _magic_selement_t *src_selement, const _magic_selement_t *dst_selement, void* value_buffer)
{
    int src_type_id = src_selement->type->type_id;
    int dst_type_id = dst_selement->type->type_id;
    unsigned src_size = src_selement->type->size;
    unsigned dst_size = dst_selement->type->size;
    void* src_value;
    int r = 0;
    assert(dst_size > 0);

    if(dst_type_id != MAGIC_TYPE_POINTER) {
        return EINVAL;
    }
    assert(dst_size == sizeof(void*));
    if(src_size != sizeof(void*)) {
        return EINVAL;
    }
    switch(src_type_id) {
        case MAGIC_TYPE_POINTER:
            return 0;
        break;

        case MAGIC_TYPE_INTEGER:
            if(MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED)) {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_unsigned(src_selement), unsigned long, src_value, void*, &r, 0);
                assert(r == 0);
            }
            else {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_int(src_selement), long, src_value, void*, &r, 0);
                assert(r == 0);
            }
        break;

        default:
            return EINVAL;
        break;
    }

    MAGIC_CHECKED_VALUE_DST_CAST(src_value, void*, value_buffer, void*, &r);
    assert(r == 0);

    return dst_size;
}

/*===========================================================================*
 *                    magic_selement_unsigned_value_cast                     *
 *===========================================================================*/
PUBLIC int magic_selement_unsigned_value_cast(const _magic_selement_t *src_selement, const _magic_selement_t *dst_selement, void* value_buffer)
{
    int src_type_id = src_selement->type->type_id;
    int dst_type_id = dst_selement->type->type_id;
    int r = 0;
    unsigned src_size = src_selement->type->size;
    unsigned dst_size = dst_selement->type->size;
    unsigned long src_value;
    assert(dst_size > 0);

    if(dst_type_id != MAGIC_TYPE_INTEGER && dst_type_id != MAGIC_TYPE_ENUM) {
        return EINVAL;
    }
    switch(src_type_id) {
        case MAGIC_TYPE_FLOAT:
            MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_float(src_selement), double, src_value, unsigned long, &r, 1);
        break;

        case MAGIC_TYPE_POINTER:
            if(dst_size != sizeof(void*)) {
                return EINVAL;
            }
            MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_ptr(src_selement), void*, src_value, unsigned long, &r, 0);
            assert(r == 0);
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(src_size == dst_size && MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED) == MAGIC_TYPE_FLAG(dst_selement->type, MAGIC_TYPE_UNSIGNED)) {
                return 0;
            }
            if(MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED)) {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_unsigned(src_selement), unsigned long, src_value, unsigned long, &r, 0);
                assert(r == 0);
            }
            else {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_int(src_selement), long, src_value, unsigned long, &r, 1);
            }
        break;

        default:
            return EINVAL;
        break;
    }

    switch(dst_size) {
        case sizeof(unsigned char):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, unsigned long, value_buffer, unsigned char, &r);
        break;

        case sizeof(unsigned short):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, unsigned long, value_buffer, unsigned short, &r);
        break;

        case sizeof(unsigned int):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, unsigned long, value_buffer, unsigned int, &r);
        break;

#ifdef MAGIC_LONG_LONG_SUPPORTED
        case sizeof(unsigned long long):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, unsigned long, value_buffer, unsigned long long, &r);
        break;
#endif

        default:
            return EINVAL;
        break;
    }

    if(r == 0) {
        r = dst_size;
    }

    return r;
}

/*===========================================================================*
 *                      magic_selement_int_value_cast                        *
 *===========================================================================*/
PUBLIC int magic_selement_int_value_cast(const _magic_selement_t *src_selement, const _magic_selement_t *dst_selement, void* value_buffer)
{
    int src_type_id = src_selement->type->type_id;
    int dst_type_id = dst_selement->type->type_id;
    int r = 0;
    unsigned src_size = src_selement->type->size;
    unsigned dst_size = dst_selement->type->size;
    long src_value;
    assert(dst_size > 0);

    if(dst_type_id != MAGIC_TYPE_INTEGER && dst_type_id != MAGIC_TYPE_ENUM) {
        return EINVAL;
    }

    switch(src_type_id) {
        case MAGIC_TYPE_FLOAT:
            MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_float(src_selement), double, src_value, long, &r, 1);
        break;

        case MAGIC_TYPE_POINTER:
            if(dst_size != sizeof(void*)) {
                return EINVAL;
            }
            MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_ptr(src_selement), void*, src_value, long, &r, 0);
            assert(r == 0);
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(src_size == dst_size && MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED) == MAGIC_TYPE_FLAG(dst_selement->type, MAGIC_TYPE_UNSIGNED)) {
                return 0;
            }
            if(MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED)) {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_unsigned(src_selement), unsigned long, src_value, long, &r, 1);
            }
            else {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_int(src_selement), long, src_value, long, &r, 0);
                assert(r == 0);
            }
        break;

        default:
            return EINVAL;
        break;
    }

    switch(dst_size) {
        case sizeof(char):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, long, value_buffer, char, &r);
        break;

        case sizeof(short):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, long, value_buffer, short, &r);
        break;

        case sizeof(int):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, long, value_buffer, int, &r);
        break;

#ifdef MAGIC_LONG_LONG_SUPPORTED
        case sizeof(long long):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, long, value_buffer, long long, &r);
        break;
#endif

        default:
            return EINVAL;
        break;
    }

    if(r == 0) {
        r = dst_size;
    }

    return r;
}

/*===========================================================================*
 *                      magic_selement_float_value_cast                      *
 *===========================================================================*/
PUBLIC int magic_selement_float_value_cast(const _magic_selement_t *src_selement, const _magic_selement_t *dst_selement, void* value_buffer)
{
    int src_type_id = src_selement->type->type_id;
    int dst_type_id = dst_selement->type->type_id;
    int r = 0;
    unsigned src_size = src_selement->type->size;
    unsigned dst_size = dst_selement->type->size;
    double src_value;
    assert(dst_size > 0);

    if(dst_type_id != MAGIC_TYPE_FLOAT) {
        return EINVAL;
    }
    switch(src_type_id) {
        case MAGIC_TYPE_FLOAT:
            if(src_size == dst_size) {
                return 0;
            }
            MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_float(src_selement), double, src_value, double, &r, 0);
            assert(r == 0);
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED)) {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_unsigned(src_selement), unsigned long, src_value, double, &r, 1);
            }
            else {
                MAGIC_CHECKED_VALUE_SRC_CAST(magic_selement_to_int(src_selement), long, src_value, double, &r, 1);
            }
        break;

        default:
            return EINVAL;
        break;
    }


    switch(dst_size) {
        case sizeof(float):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, double, value_buffer, float, &r);
        break;

        case sizeof(double):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, double, value_buffer, double, &r);
        break;

#ifdef MAGIC_LONG_DOUBLE_SUPPORTED
        case sizeof(long double):
            MAGIC_CHECKED_VALUE_DST_CAST(src_value, double, value_buffer, long double, &r);
        break;
#endif

        default:
            return EINVAL;
        break;
    }

    if(r == 0) {
        r = dst_size;
    }

    return r;
}

/*===========================================================================*
 *                         magic_selement_value_cast                         *
 *===========================================================================*/
PUBLIC int magic_selement_value_cast(const _magic_selement_t *src_selement, const _magic_selement_t *dst_selement, void* value_buffer)
{
    int r, src_type_id, dst_type_id;
    size_t src_size, dst_size;
    src_type_id = src_selement->type->type_id;
    dst_type_id = dst_selement->type->type_id;
    src_size = src_selement->type->size;
    dst_size = dst_selement->type->size;
    if(src_type_id == dst_type_id && src_size == dst_size && MAGIC_TYPE_FLAG(src_selement->type, MAGIC_TYPE_UNSIGNED) == MAGIC_TYPE_FLAG(dst_selement->type, MAGIC_TYPE_UNSIGNED)) {
        return 0;
    }

    /* No size change allowed in opaque value casts. */
    if(src_type_id == MAGIC_TYPE_OPAQUE || dst_type_id == MAGIC_TYPE_OPAQUE) {
        return src_size == dst_size ? 0 : EINVAL;
    }

    /* No size change allowed in void value casts. */
    if(src_type_id == MAGIC_TYPE_VOID || dst_type_id == MAGIC_TYPE_VOID) {
        return src_size == dst_size ? 0 : EINVAL;
    }

    switch(dst_type_id) {
        case MAGIC_TYPE_POINTER:
            /* Cast to pointer values. */
            r = magic_selement_ptr_value_cast(src_selement, dst_selement, value_buffer);
        break;

        case MAGIC_TYPE_FLOAT:
            /* Cast to float values. */
            r = magic_selement_float_value_cast(src_selement, dst_selement, value_buffer);
        break;

        case MAGIC_TYPE_INTEGER:
        case MAGIC_TYPE_ENUM:
            if(MAGIC_TYPE_FLAG(dst_selement->type, MAGIC_TYPE_UNSIGNED)) {
                /* Cast to unsigned values. */
                r = magic_selement_unsigned_value_cast(src_selement, dst_selement, value_buffer);
            }
            else {
                /* Cast to integer values. */
                r = magic_selement_int_value_cast(src_selement, dst_selement, value_buffer);
            }
        break;

        default:
            r = EINVAL;
        break;
    }
    return r;
}

/*===========================================================================*
 *                         magic_selement_get_parent                         *
 *===========================================================================*/
PUBLIC _magic_selement_t* magic_selement_get_parent(
    const _magic_selement_t *selement, _magic_selement_t *parent_selement)
{
    if(!selement->parent_type) {
        return NULL;
    }

    parent_selement->sentry = selement->sentry;
    parent_selement->parent_type = NULL;
    parent_selement->child_num = 0;
    parent_selement->type = selement->parent_type;
    parent_selement->address = selement->parent_address;
    parent_selement->num = 0;
    assert(parent_selement->address >= parent_selement->sentry->address);

    return parent_selement;
}

/*===========================================================================*
 *                    magic_selement_fill_from_parent_info                   *
 *===========================================================================*/
PUBLIC void magic_selement_fill_from_parent_info(_magic_selement_t *selement,
    int walk_flags)
{
    unsigned offset;
    magic_type_walk_step(selement->parent_type,
        selement->child_num, &selement->type, &offset, walk_flags);
    selement->address = (char*) selement->parent_address + offset;
}

/*===========================================================================*
 *                         magic_selement_from_sentry                        *
 *===========================================================================*/
PUBLIC _magic_selement_t* magic_selement_from_sentry(struct _magic_sentry *sentry,
    _magic_selement_t *selement)
{
    selement->sentry = sentry;
    selement->parent_type = NULL;
    selement->child_num = 0;
    selement->type = sentry->type;
    selement->address = sentry->address;
    selement->num = 1;

    return selement;
}

/*===========================================================================*
 *                      magic_selement_from_relative_name                    *
 *===========================================================================*/
PUBLIC _magic_selement_t* magic_selement_from_relative_name(
    _magic_selement_t *parent_selement, _magic_selement_t *selement, char* name)
{
    _magic_selement_t new_parent_selement;
    const struct _magic_type* parent_type = parent_selement->type;
    int parent_type_id = parent_type->type_id;
    int walk_flags = 0;
    int i, child_num = -1;
    char *end;

    if(!name || *name == '\0') {
        return NULL;
    }

    if(parent_type_id == MAGIC_TYPE_UNION && (*name >= '0' && *name <= '9')) {
        parent_type_id = MAGIC_TYPE_ARRAY;
        walk_flags = MAGIC_TYPE_WALK_UNIONS_AS_VOID;
    }

    switch(parent_type_id) {
        case MAGIC_TYPE_ARRAY:
        case MAGIC_TYPE_VECTOR:
            child_num = (int) strtol(name, &end, 10);
            if(end == name || *end != '\0' || errno == ERANGE) {
                return NULL;
            }
        break;

        case MAGIC_TYPE_STRUCT:
        case MAGIC_TYPE_UNION:
            for(i=0; (unsigned int)i<parent_type->num_child_types;i++) {
                if(!strcmp(parent_type->member_names[i], name)) {
                    child_num = i;
                    break;
                }
            }
            if((unsigned int)i == parent_type->num_child_types) {
                return NULL;
            }
        break;

        case MAGIC_TYPE_POINTER:
            i = magic_selement_recurse_ptr(parent_selement, selement, MAGIC_SELEMENT_MAX_PTR_RECURSIONS);
            if(i <= 0 || i >= MAGIC_SELEMENT_MAX_PTR_RECURSIONS) {
                return NULL;
            }
            new_parent_selement = *selement;
            return magic_selement_from_relative_name(&new_parent_selement, selement, name);
        break;

        default:
            return NULL;
        break;
    }

    if(child_num != -1) {
        selement->sentry = parent_selement->sentry;
        selement->parent_type = parent_type;
        selement->parent_address = parent_selement->address;
        selement->child_num = child_num;
        selement->num = parent_selement->num+1;
        magic_selement_fill_from_parent_info(selement, walk_flags);
    }

    return selement;
}


/*	$NetBSD: array.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "baselocl.h"

/*
 *
 */

struct heim_array_data {
    size_t len;
    heim_object_t *val;
    size_t allocated_len;
    heim_object_t *allocated;
};

static void
array_dealloc(heim_object_t ptr)
{
    heim_array_t array = ptr;
    size_t n;
    for (n = 0; n < array->len; n++)
	heim_release(array->val[n]);
    free(array->allocated);
}

struct heim_type_data array_object = {
    HEIM_TID_ARRAY,
    "dict-object",
    NULL,
    array_dealloc,
    NULL,
    NULL,
    NULL,
    NULL
};

/**
 * Allocate an array
 *
 * @return A new allocated array, free with heim_release()
 */

heim_array_t
heim_array_create(void)
{
    heim_array_t array;

    array = _heim_alloc_object(&array_object, sizeof(*array));
    if (array == NULL)
	return NULL;

    array->allocated = NULL;
    array->allocated_len = 0;
    array->val = NULL;
    array->len = 0;

    return array;
}

/**
 * Get type id of an dict
 *
 * @return the type id
 */

heim_tid_t
heim_array_get_type_id(void)
{
    return HEIM_TID_ARRAY;
}

/**
 * Append object to array
 *
 * @param array array to add too
 * @param object the object to add
 *
 * @return zero if added, errno otherwise
 */

int
heim_array_append_value(heim_array_t array, heim_object_t object)
{
    heim_object_t *ptr;
    size_t leading = array->val - array->allocated; /* unused leading slots */
    size_t trailing = array->allocated_len - array->len - leading;
    size_t new_len;

    if (trailing > 0) {
	/* We have pre-allocated space; use it */
	array->val[array->len++] = heim_retain(object);
	return 0;
    }

    if (leading > (array->len + 1)) {
	/*
	 * We must have appending to, and deleting at index 0 from this
	 * array a lot; don't want to grow forever!
	 */
	(void) memmove(&array->allocated[0], &array->val[0],
		       array->len * sizeof(array->val[0]));
	array->val = array->allocated;

	/* We have pre-allocated space; use it */
	array->val[array->len++] = heim_retain(object);
	return 0;
    }

    /* Pre-allocate extra .5 times number of used slots */
    new_len = leading + array->len + 1 + (array->len >> 1);
    ptr = realloc(array->allocated, new_len * sizeof(array->val[0]));
    if (ptr == NULL)
	return ENOMEM;
    array->allocated = ptr;
    array->allocated_len = new_len;
    array->val = &ptr[leading];
    array->val[array->len++] = heim_retain(object);

    return 0;
}

/*
 * Internal function to insert at index 0, taking care to optimize the
 * case where we're always inserting at index 0, particularly the case
 * where we insert at index 0 and delete from the right end.
 */
static int
heim_array_prepend_value(heim_array_t array, heim_object_t object)
{
    heim_object_t *ptr;
    size_t leading = array->val - array->allocated; /* unused leading slots */
    size_t trailing = array->allocated_len - array->len - leading;
    size_t new_len;

    if (leading > 0) {
	/* We have pre-allocated space; use it */
	array->val--;
	array->val[0] = heim_retain(object);
	array->len++;
	return 0;
    }
    if (trailing > (array->len + 1)) {
	/*
	 * We must have prepending to, and deleting at index
	 * array->len - 1 from this array a lot; don't want to grow
	 * forever!
	 */
	(void) memmove(&array->allocated[array->len], &array->val[0],
		       array->len * sizeof(array->val[0]));
	array->val = &array->allocated[array->len];

	/* We have pre-allocated space; use it */
	array->val--;
	array->val[0] = heim_retain(object);
	array->len++;
	return 0;
    }
    /* Pre-allocate extra .5 times number of used slots */
    new_len = array->len + 1 + trailing + (array->len >> 1);
    ptr = realloc(array->allocated, new_len * sizeof(array->val[0]));
    if (ptr == NULL)
	return ENOMEM;
    (void) memmove(&ptr[1], &ptr[0], array->len * sizeof (array->val[0]));
    array->allocated = ptr;
    array->allocated_len = new_len;
    array->val = &ptr[0];
    array->val[0] = heim_retain(object);
    array->len++;

    return 0;
}

/**
 * Insert an object at a given index in an array
 *
 * @param array array to add too
 * @param idx index where to add element (-1 == append, -2 next to last, ...)
 * @param object the object to add
 *
 * @return zero if added, errno otherwise
 */

int
heim_array_insert_value(heim_array_t array, size_t idx, heim_object_t object)
{
    int ret;

    if (idx == 0)
	return heim_array_prepend_value(array, object);
    else if (idx > array->len)
	heim_abort("index too large");

    /*
     * We cheat: append this element then rotate elements around so we
     * have this new element at the desired location, unless we're truly
     * appending the new element.  This means reusing array growth in
     * heim_array_append_value() instead of duplicating that here.
     */
    ret = heim_array_append_value(array, object);
    if (ret != 0 || idx == (array->len - 1))
	return ret;
    /*
     * Shift to the right by one all the elements after idx, then set
     * [idx] to the new object.
     */
    (void) memmove(&array->val[idx + 1], &array->val[idx],
	           (array->len - idx - 1) * sizeof(array->val[0]));
    array->val[idx] = heim_retain(object);

    return 0;
}

/**
 * Iterate over all objects in array
 *
 * @param array array to iterate over
 * @param ctx context passed to fn
 * @param fn function to call on each object
 */

void
heim_array_iterate_f(heim_array_t array, void *ctx, heim_array_iterator_f_t fn)
{
    size_t n;
    int stop = 0;
    for (n = 0; n < array->len; n++) {
	fn(array->val[n], ctx, &stop);
	if (stop)
	    return;
    }
}

#ifdef __BLOCKS__
/**
 * Iterate over all objects in array
 *
 * @param array array to iterate over
 * @param fn block to call on each object
 */

void
heim_array_iterate(heim_array_t array, void (^fn)(heim_object_t, int *))
{
    size_t n;
    int stop = 0;
    for (n = 0; n < array->len; n++) {
	fn(array->val[n], &stop);
	if (stop)
	    return;
    }
}
#endif

/**
 * Iterate over all objects in array, backwards
 *
 * @param array array to iterate over
 * @param ctx context passed to fn
 * @param fn function to call on each object
 */

void
heim_array_iterate_reverse_f(heim_array_t array, void *ctx, heim_array_iterator_f_t fn)
{
    size_t n;
    int stop = 0;

    for (n = array->len; n > 0; n--) {
	fn(array->val[n - 1], ctx, &stop);
	if (stop)
	    return;
    }
}

#ifdef __BLOCKS__
/**
 * Iterate over all objects in array, backwards
 *
 * @param array array to iterate over
 * @param fn block to call on each object
 */

void
heim_array_iterate_reverse(heim_array_t array, void (^fn)(heim_object_t, int *))
{
    size_t n;
    int stop = 0;
    for (n = array->len; n > 0; n--) {
	fn(array->val[n - 1], &stop);
	if (stop)
	    return;
    }
}
#endif

/**
 * Get length of array
 *
 * @param array array to get length of
 *
 * @return length of array
 */

size_t
heim_array_get_length(heim_array_t array)
{
    return array->len;
}

/**
 * Get value of element at array index
 *
 * @param array array copy object from
 * @param idx index of object, 0 based, must be smaller then
 *        heim_array_get_length()
 *
 * @return a not-retained copy of the object
 */

heim_object_t
heim_array_get_value(heim_array_t array, size_t idx)
{
    if (idx >= array->len)
	heim_abort("index too large");
    return array->val[idx];
}

/**
 * Get value of element at array index
 *
 * @param array array copy object from
 * @param idx index of object, 0 based, must be smaller then
 *        heim_array_get_length()
 *
 * @return a retained copy of the object
 */

heim_object_t
heim_array_copy_value(heim_array_t array, size_t idx)
{
    if (idx >= array->len)
	heim_abort("index too large");
    return heim_retain(array->val[idx]);
}

/**
 * Set value at array index
 *
 * @param array array copy object from
 * @param idx index of object, 0 based, must be smaller then
 *        heim_array_get_length()
 * @param value value to set 
 *
 */

void
heim_array_set_value(heim_array_t array, size_t idx, heim_object_t value)
{
    if (idx >= array->len)
	heim_abort("index too large");
    heim_release(array->val[idx]);
    array->val[idx] = heim_retain(value);
}

/**
 * Delete value at idx
 *
 * @param array the array to modify
 * @param idx the key to delete
 */

void
heim_array_delete_value(heim_array_t array, size_t idx)
{
    heim_object_t obj;
    if (idx >= array->len)
	heim_abort("index too large");
    obj = array->val[idx];

    array->len--;

    /*
     * Deleting the first or last elements is cheap, as we leave
     * allocated space for opportunistic reuse later; no realloc(), no
     * memmove().  All others require a memmove().
     *
     * If we ever need to optimize deletion of non-last/ non-first
     * element we can use a tagged object type to signify "deleted
     * value" so we can leave holes in the array, avoid memmove()s on
     * delete, and opportunistically re-use those holes on insert.
     */
    if (idx == 0)
	array->val++;
    else if (idx < array->len)
	(void) memmove(&array->val[idx], &array->val[idx + 1],
		       (array->len - idx) * sizeof(array->val[0]));

    heim_release(obj);
}

/**
 * Filter out entres of array when function return true
 *
 * @param array the array to modify
 * @param fn filter function
 */

void
heim_array_filter_f(heim_array_t array, void *ctx, heim_array_filter_f_t fn)
{
    size_t n = 0;

    while (n < array->len) {
	if (fn(array->val[n], ctx)) {
	    heim_array_delete_value(array, n);
	} else {
	    n++;
	}
    }
}

#ifdef __BLOCKS__

/**
 * Filter out entres of array when block return true
 *
 * @param array the array to modify
 * @param block filter block
 */

void
heim_array_filter(heim_array_t array, int (^block)(heim_object_t))
{
    size_t n = 0;

    while (n < array->len) {
	if (block(array->val[n])) {
	    heim_array_delete_value(array, n);
	} else {
	    n++;
	}
    }
}

#endif /* __BLOCKS__ */

/*	$NetBSD: test_base.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2010-2016 Kungliga Tekniska Högskolan
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

/*
 * This is a test of libheimbase functionality.  If you make any changes
 * to libheimbase or to this test you should run it under valgrind with
 * the following options:
 *
 *  -v --track-fds=yes --num-callers=30 --leak-check=full
 *  
 * and make sure that there are no leaks that don't have
 * __heim_string_constant() or heim_db_register() in their stack trace.
 */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <sys/file.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include "baselocl.h"

static void
memory_free(heim_object_t obj)
{
}

static int
test_memory(void)
{
    void *ptr;

    ptr = heim_alloc(10, "memory", memory_free);

    heim_retain(ptr);
    heim_release(ptr);

    heim_retain(ptr);
    heim_release(ptr);

    heim_release(ptr);

    ptr = heim_alloc(10, "memory", NULL);
    heim_release(ptr);

    return 0;
}

static int
test_mutex(void)
{
    HEIMDAL_MUTEX m = HEIMDAL_MUTEX_INITIALIZER;

    HEIMDAL_MUTEX_lock(&m);
    HEIMDAL_MUTEX_unlock(&m);
    HEIMDAL_MUTEX_destroy(&m);

    HEIMDAL_MUTEX_init(&m);
    HEIMDAL_MUTEX_lock(&m);
    HEIMDAL_MUTEX_unlock(&m);
    HEIMDAL_MUTEX_destroy(&m);

    return 0;
}

static int
test_rwlock(void)
{
    HEIMDAL_RWLOCK l = HEIMDAL_RWLOCK_INITIALIZER;

    HEIMDAL_RWLOCK_rdlock(&l);
    HEIMDAL_RWLOCK_unlock(&l);
    HEIMDAL_RWLOCK_wrlock(&l);
    HEIMDAL_RWLOCK_unlock(&l);
    if (HEIMDAL_RWLOCK_trywrlock(&l) != 0)
	err(1, "HEIMDAL_RWLOCK_trywrlock() failed with lock not held");
    HEIMDAL_RWLOCK_unlock(&l);
    if (HEIMDAL_RWLOCK_tryrdlock(&l))
	err(1, "HEIMDAL_RWLOCK_tryrdlock() failed with lock not held");
    HEIMDAL_RWLOCK_unlock(&l);
    HEIMDAL_RWLOCK_destroy(&l);

    HEIMDAL_RWLOCK_init(&l);
    HEIMDAL_RWLOCK_rdlock(&l);
    HEIMDAL_RWLOCK_unlock(&l);
    HEIMDAL_RWLOCK_wrlock(&l);
    HEIMDAL_RWLOCK_unlock(&l);
    if (HEIMDAL_RWLOCK_trywrlock(&l))
	err(1, "HEIMDAL_RWLOCK_trywrlock() failed with lock not held");
    HEIMDAL_RWLOCK_unlock(&l);
    if (HEIMDAL_RWLOCK_tryrdlock(&l))
	err(1, "HEIMDAL_RWLOCK_tryrdlock() failed with lock not held");
    HEIMDAL_RWLOCK_unlock(&l);
    HEIMDAL_RWLOCK_destroy(&l);

    return 0;
}

static int
test_dict(void)
{
    heim_dict_t dict;
    heim_number_t a1 = heim_number_create(1);
    heim_string_t a2 = heim_string_create("hejsan");
    heim_number_t a3 = heim_number_create(3);
    heim_string_t a4 = heim_string_create("foosan");

    dict = heim_dict_create(10);

    heim_dict_set_value(dict, a1, a2);
    heim_dict_set_value(dict, a3, a4);

    heim_dict_delete_key(dict, a3);
    heim_dict_delete_key(dict, a1);

    heim_release(a1);
    heim_release(a2);
    heim_release(a3);
    heim_release(a4);

    heim_release(dict);

    return 0;
}

static int
test_auto_release(void)
{
    heim_auto_release_t ar1, ar2;
    heim_number_t n1;
    heim_string_t s1;

    ar1 = heim_auto_release_create();

    s1 = heim_string_create("hejsan");
    heim_auto_release(s1);

    n1 = heim_number_create(1);
    heim_auto_release(n1);

    ar2 = heim_auto_release_create();

    n1 = heim_number_create(1);
    heim_auto_release(n1);

    heim_release(ar2);
    heim_release(ar1);

    return 0;
}

static int
test_string(void)
{
    heim_string_t s1, s2;
    const char *string = "hejsan";

    s1 = heim_string_create(string);
    s2 = heim_string_create(string);

    if (heim_cmp(s1, s2) != 0) {
	printf("the same string is not the same\n");
	exit(1);
    }

    heim_release(s1);
    heim_release(s2);

    return 0;
}

static int
test_error(void)
{
    heim_error_t e;
    heim_string_t s;

    e = heim_error_create(10, "foo: %s", "bar");
    heim_assert(heim_error_get_code(e) == 10, "error_code != 10");

    s = heim_error_copy_string(e);
    heim_assert(strcmp(heim_string_get_utf8(s), "foo: bar") == 0, "msg wrong");

    heim_release(s);
    heim_release(e);

    return 0;
}

static int
test_json(void)
{
    static char *j[] = {
	"{ \"k1\" : \"s1\", \"k2\" : \"s2\" }",
	"{ \"k1\" : [\"s1\", \"s2\", \"s3\"], \"k2\" : \"s3\" }",
	"{ \"k1\" : {\"k2\":\"s1\",\"k3\":\"s2\",\"k4\":\"s3\"}, \"k5\" : \"s4\" }",
	"[ \"v1\", \"v2\", [\"v3\",\"v4\",[\"v 5\",\" v 7 \"]], -123456789, "
	    "null, true, false, 123456789, \"\"]",
	" -1"
    };
    char *s;
    size_t i, k;
    heim_object_t o, o2;
    heim_string_t k1 = heim_string_create("k1");

    o = heim_json_create("\"string\"", 10, 0, NULL);
    heim_assert(o != NULL, "string");
    heim_assert(heim_get_tid(o) == heim_string_get_type_id(), "string-tid");
    heim_assert(strcmp("string", heim_string_get_utf8(o)) == 0, "wrong string");
    heim_release(o);

    o = heim_json_create(" \"foo\\\"bar\" ]", 10, 0, NULL);
    heim_assert(o != NULL, "string");
    heim_assert(heim_get_tid(o) == heim_string_get_type_id(), "string-tid");
    heim_assert(strcmp("foo\"bar", heim_string_get_utf8(o)) == 0, "wrong string");
    heim_release(o);

    o = heim_json_create(" { \"key\" : \"value\" }", 10, 0, NULL);
    heim_assert(o != NULL, "dict");
    heim_assert(heim_get_tid(o) == heim_dict_get_type_id(), "dict-tid");
    heim_release(o);

    o = heim_json_create("{ { \"k1\" : \"s1\", \"k2\" : \"s2\" } : \"s3\", "
			 "{ \"k3\" : \"s4\" } : -1 }", 10, 0, NULL);
    heim_assert(o != NULL, "dict");
    heim_assert(heim_get_tid(o) == heim_dict_get_type_id(), "dict-tid");
    heim_release(o);

    o = heim_json_create("{ { \"k1\" : \"s1\", \"k2\" : \"s2\" } : \"s3\", "
			 "{ \"k3\" : \"s4\" } : -1 }", 10,
			 HEIM_JSON_F_STRICT_DICT, NULL);
    heim_assert(o == NULL, "dict");

    o = heim_json_create(" { \"k1\" : \"s1\", \"k2\" : \"s2\" }", 10, 0, NULL);
    heim_assert(o != NULL, "dict");
    heim_assert(heim_get_tid(o) == heim_dict_get_type_id(), "dict-tid");
    o2 = heim_dict_copy_value(o, k1);
    heim_assert(heim_get_tid(o2) == heim_string_get_type_id(), "string-tid");
    heim_release(o2);
    heim_release(o);

    o = heim_json_create(" { \"k1\" : { \"k2\" : \"s2\" } }", 10, 0, NULL);
    heim_assert(o != NULL, "dict");
    heim_assert(heim_get_tid(o) == heim_dict_get_type_id(), "dict-tid");
    o2 = heim_dict_copy_value(o, k1);
    heim_assert(heim_get_tid(o2) == heim_dict_get_type_id(), "dict-tid");
    heim_release(o2);
    heim_release(o);

    o = heim_json_create("{ \"k1\" : 1 }", 10, 0, NULL);
    heim_assert(o != NULL, "array");
    heim_assert(heim_get_tid(o) == heim_dict_get_type_id(), "dict-tid");
    o2 = heim_dict_copy_value(o, k1);
    heim_assert(heim_get_tid(o2) == heim_number_get_type_id(), "number-tid");
    heim_release(o2);
    heim_release(o);

    o = heim_json_create("-10", 10, 0, NULL);
    heim_assert(o != NULL, "number");
    heim_assert(heim_get_tid(o) == heim_number_get_type_id(), "number-tid");
    heim_release(o);

    o = heim_json_create("99", 10, 0, NULL);
    heim_assert(o != NULL, "number");
    heim_assert(heim_get_tid(o) == heim_number_get_type_id(), "number-tid");
    heim_release(o);

    o = heim_json_create(" [ 1 ]", 10, 0, NULL);
    heim_assert(o != NULL, "array");
    heim_assert(heim_get_tid(o) == heim_array_get_type_id(), "array-tid");
    heim_release(o);

    o = heim_json_create(" [ -1 ]", 10, 0, NULL);
    heim_assert(o != NULL, "array");
    heim_assert(heim_get_tid(o) == heim_array_get_type_id(), "array-tid");
    heim_release(o);

    for (i = 0; i < (sizeof (j) / sizeof (j[0])); i++) {
	o = heim_json_create(j[i], 10, 0, NULL);
	if (o == NULL) {
	    fprintf(stderr, "Failed to parse this JSON: %s\n", j[i]);
	    return 1;
	}
	heim_release(o);
	/* Simple fuzz test */
	for (k = strlen(j[i]) - 1; k > 0; k--) {
	    o = heim_json_create_with_bytes(j[i], k, 10, 0, NULL);
	    if (o != NULL) {
		fprintf(stderr, "Invalid JSON parsed: %.*s\n", (int)k, j[i]);
		return EINVAL;
	    }
	}
	/* Again, but this time make it so valgrind can find invalid accesses */
	for (k = strlen(j[i]) - 1; k > 0; k--) {
	    s = strndup(j[i], k);
	    if (s == NULL)
		return ENOMEM;
	    o = heim_json_create(s, 10, 0, NULL);
	    free(s);
	    if (o != NULL) {
		fprintf(stderr, "Invalid JSON parsed: %s\n", j[i]);
		return EINVAL;
	    }
	}
	/* Again, but with no NUL termination */
	for (k = strlen(j[i]) - 1; k > 0; k--) {
	    s = malloc(k);
	    if (s == NULL)
		return ENOMEM;
	    memcpy(s, j[i], k);
	    o = heim_json_create_with_bytes(s, k, 10, 0, NULL);
	    free(s);
	    if (o != NULL) {
		fprintf(stderr, "Invalid JSON parsed: %s\n", j[i]);
		return EINVAL;
	    }
	}
    }

    heim_release(k1);

    return 0;
}

static int
test_path(void)
{
    heim_dict_t dict = heim_dict_create(11);
    heim_string_t p1 = heim_string_create("abc");
    heim_string_t p2a = heim_string_create("def");
    heim_string_t p2b = heim_string_create("DEF");
    heim_number_t p3 = heim_number_create(0);
    heim_string_t p4a = heim_string_create("ghi");
    heim_string_t p4b = heim_string_create("GHI");
    heim_array_t a = heim_array_create();
    heim_number_t l1 = heim_number_create(42);
    heim_number_t l2 = heim_number_create(813);
    heim_number_t l3 = heim_number_create(1234);
    heim_string_t k1 = heim_string_create("k1");
    heim_string_t k2 = heim_string_create("k2");
    heim_string_t k3 = heim_string_create("k3");
    heim_string_t k2_1 = heim_string_create("k2-1");
    heim_string_t k2_2 = heim_string_create("k2-2");
    heim_string_t k2_3 = heim_string_create("k2-3");
    heim_string_t k2_4 = heim_string_create("k2-4");
    heim_string_t k2_5 = heim_string_create("k2-5");
    heim_string_t k2_5_1 = heim_string_create("k2-5-1");
    heim_object_t o;
    heim_object_t neg_num;
    int ret;

    if (!dict || !p1 || !p2a || !p2b || !p4a || !p4b)
	return ENOMEM;

    ret = heim_path_create(dict, 11, a, NULL, p1, p2a, NULL);
    heim_release(a);
    if (ret)
	return ret;
    ret = heim_path_create(dict, 11, l3, NULL, p1, p2b, NULL);
    if (ret)
	return ret;
    o = heim_path_get(dict, NULL, p1, p2b, NULL);
    if (o != l3)
	return 1;
    ret = heim_path_create(dict, 11, NULL, NULL, p1, p2a, p3, NULL);
    if (ret)
	return ret;
    ret = heim_path_create(dict, 11, l1, NULL, p1, p2a, p3, p4a, NULL);
    if (ret)
	return ret;
    ret = heim_path_create(dict, 11, l2, NULL, p1, p2a, p3, p4b, NULL);
    if (ret)
	return ret;

    o = heim_path_get(dict, NULL, p1, p2a, p3, p4a, NULL);
    if (o != l1)
	return 1;
    o = heim_path_get(dict, NULL, p1, p2a, p3, p4b, NULL);
    if (o != l2)
	return 1;

    heim_release(dict);

    /* Test that JSON parsing works right by using heim_path_get() */
    dict = heim_json_create("{\"k1\":1,"
			    "\"k2\":{\"k2-1\":21,"
				    "\"k2-2\":null,"
				    "\"k2-3\":true,"
				    "\"k2-4\":false,"
				    "\"k2-5\":[1,2,3,{\"k2-5-1\":-1},-2]},"
			    "\"k3\":[true,false,0,42]}", 10, 0, NULL);
    heim_assert(dict != NULL, "dict");
    o = heim_path_get(dict, NULL, k1, NULL);
    if (heim_cmp(o, heim_number_create(1))) return 1;
    o = heim_path_get(dict, NULL, k2, NULL);
    if (heim_get_tid(o) != heim_dict_get_type_id()) return 1;
    o = heim_path_get(dict, NULL, k2, k2_1, NULL);
    if (heim_cmp(o, heim_number_create(21))) return 1;
    o = heim_path_get(dict, NULL, k2, k2_2, NULL);
    if (heim_cmp(o, heim_null_create())) return 1;
    o = heim_path_get(dict, NULL, k2, k2_3, NULL);
    if (heim_cmp(o, heim_bool_create(1))) return 1;
    o = heim_path_get(dict, NULL, k2, k2_4, NULL);
    if (heim_cmp(o, heim_bool_create(0))) return 1;
    o = heim_path_get(dict, NULL, k2, k2_5, NULL);
    if (heim_get_tid(o) != heim_array_get_type_id()) return 1;
    o = heim_path_get(dict, NULL, k2, k2_5, heim_number_create(0), NULL);
    if (heim_cmp(o, heim_number_create(1))) return 1;
    o = heim_path_get(dict, NULL, k2, k2_5, heim_number_create(1), NULL);
    if (heim_cmp(o, heim_number_create(2))) return 1;
    o = heim_path_get(dict, NULL, k2, k2_5, heim_number_create(3), k2_5_1, NULL);
    if (heim_cmp(o, neg_num = heim_number_create(-1))) return 1;
    heim_release(neg_num);
    o = heim_path_get(dict, NULL, k2, k2_5, heim_number_create(4), NULL);
    if (heim_cmp(o, neg_num = heim_number_create(-2))) return 1;
    heim_release(neg_num);
    o = heim_path_get(dict, NULL, k3, heim_number_create(3), NULL);
    if (heim_cmp(o, heim_number_create(42))) return 1;

    heim_release(dict);
    heim_release(p1);
    heim_release(p2a);
    heim_release(p2b);
    heim_release(p4a);
    heim_release(p4b);
    heim_release(k1);
    heim_release(k2);
    heim_release(k3);
    heim_release(k2_1);
    heim_release(k2_2);
    heim_release(k2_3);
    heim_release(k2_4);
    heim_release(k2_5);
    heim_release(k2_5_1);

    return 0;
}

typedef struct dict_db {
    heim_dict_t dict;
    int locked;
} *dict_db_t;

static int
dict_db_open(void *plug, const char *dbtype, const char *dbname,
	     heim_dict_t options, void **db, heim_error_t *error)
{
    dict_db_t dictdb;
    heim_dict_t contents = NULL;

    if (error)
	*error = NULL;
    if (dbtype && *dbtype && strcmp(dbtype, "dictdb"))
	return EINVAL;
    if (dbname && *dbname && strcmp(dbname, "MEMORY") != 0)
	return EINVAL;
    dictdb = heim_alloc(sizeof (*dictdb), "dict_db", NULL);
    if (dictdb == NULL)
	return ENOMEM;

    if (contents != NULL)
	dictdb->dict = contents;
    else {
	dictdb->dict = heim_dict_create(29);
	if (dictdb->dict == NULL) {
	    heim_release(dictdb);
	    return ENOMEM;
	}
    }

    *db = dictdb;
    return 0;
}

static int
dict_db_close(void *db, heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;
    heim_release(dictdb->dict);
    heim_release(dictdb);
    return 0;
}

static int
dict_db_lock(void *db, int read_only, heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;
    if (dictdb->locked)
	return EWOULDBLOCK;
    dictdb->locked = 1;
    return 0;
}

static int
dict_db_unlock(void *db, heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;
    dictdb->locked = 0;
    return 0;
}

static heim_data_t
dict_db_copy_value(void *db, heim_string_t table, heim_data_t key,
		  heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;

    return heim_retain(heim_path_get(dictdb->dict, error, table, key, NULL));
}

static int
dict_db_set_value(void *db, heim_string_t table,
		  heim_data_t key, heim_data_t value, heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    return heim_path_create(dictdb->dict, 29, value, error, table, key, NULL);
}

static int
dict_db_del_key(void *db, heim_string_t table, heim_data_t key,
		heim_error_t *error)
{
    dict_db_t dictdb = db;

    if (error)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    heim_path_delete(dictdb->dict, error, table, key, NULL);
    return 0;
}

struct dict_db_iter_ctx {
    heim_db_iterator_f_t        iter_f;
    void                        *iter_ctx;
};

static void dict_db_iter_f(heim_object_t key, heim_object_t value, void *arg)
{
    struct dict_db_iter_ctx *ctx = arg;

    ctx->iter_f((heim_object_t)key, (heim_object_t)value, ctx->iter_ctx);
}

static void
dict_db_iter(void *db, heim_string_t table, void *iter_data,
	     heim_db_iterator_f_t iter_f, heim_error_t *error)
{
    dict_db_t dictdb = db;
    struct dict_db_iter_ctx ctx;
    heim_dict_t table_dict;

    if (error)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    table_dict = heim_dict_copy_value(dictdb->dict, table);
    if (table_dict == NULL)
	return;

    ctx.iter_ctx = iter_data;
    ctx.iter_f = iter_f;

    heim_dict_iterate_f(table_dict, &ctx, dict_db_iter_f);
    heim_release(table_dict);
}

static void
test_db_iter(heim_data_t k, heim_data_t v, void *arg)
{
    int *ret = arg;
    const void *kptr, *vptr;
    size_t klen, vlen;

    heim_assert(heim_get_tid(k) == heim_data_get_type_id(), "...");

    kptr = heim_data_get_ptr(k);
    klen = heim_data_get_length(k);
    vptr = heim_data_get_ptr(v);
    vlen = heim_data_get_length(v);

    if (klen == strlen("msg") && !strncmp(kptr, "msg", strlen("msg")) &&
	vlen == strlen("abc") && !strncmp(vptr, "abc", strlen("abc")))
	*ret &= ~(1);
    else if (klen == strlen("msg2") &&
	!strncmp(kptr, "msg2", strlen("msg2")) &&
	vlen == strlen("FooBar") && !strncmp(vptr, "FooBar", strlen("FooBar")))
	*ret &= ~(2);
    else
	*ret |= 4;
}

static struct heim_db_type dbt = {
    1, dict_db_open, NULL, dict_db_close,
    dict_db_lock, dict_db_unlock, NULL, NULL, NULL, NULL,
    dict_db_copy_value, dict_db_set_value,
    dict_db_del_key, dict_db_iter
};

static int
test_db(const char *dbtype, const char *dbname)
{
    heim_data_t k1, k2, v, v1, v2, v3;
    heim_db_t db;
    int ret;

    if (dbtype == NULL) {
	ret = heim_db_register("dictdb", NULL, &dbt);
	heim_assert(!ret, "...");
	db = heim_db_create("dictdb", "foo", NULL, NULL);
	heim_assert(!db, "...");
	db = heim_db_create("foobar", "MEMORY", NULL, NULL);
	heim_assert(!db, "...");
	db = heim_db_create("dictdb", "MEMORY", NULL, NULL);
	heim_assert(db, "...");
    } else {
	heim_dict_t options;

	options = heim_dict_create(11);
	if (options == NULL) return ENOMEM;
	if (heim_dict_set_value(options, HSTR("journal-filename"),
				HSTR("json-journal")))
	    return ENOMEM;
	if (heim_dict_set_value(options, HSTR("create"), heim_null_create()))
	    return ENOMEM;
	if (heim_dict_set_value(options, HSTR("truncate"), heim_null_create()))
	    return ENOMEM;
	db = heim_db_create(dbtype, dbname, options, NULL);
	heim_assert(db, "...");
	heim_release(options);
    }

    k1 = heim_data_create("msg", strlen("msg"));
    k2 = heim_data_create("msg2", strlen("msg2"));
    v1 = heim_data_create("Hello world!", strlen("Hello world!"));
    v2 = heim_data_create("FooBar", strlen("FooBar"));
    v3 = heim_data_create("abc", strlen("abc"));

    ret = heim_db_set_value(db, NULL, k1, v1, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v1), "...");
    heim_release(v);

    ret = heim_db_set_value(db, NULL, k2, v2, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k2, NULL);
    heim_assert(v && !heim_cmp(v, v2), "...");
    heim_release(v);

    ret = heim_db_set_value(db, NULL, k1, v3, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v3), "...");
    heim_release(v);

    ret = 3;
    heim_db_iterate_f(db, NULL, &ret, test_db_iter, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_begin(db, 0, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_commit(db, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_begin(db, 0, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_rollback(db, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_begin(db, 0, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_set_value(db, NULL, k1, v1, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v1), "...");
    heim_release(v);

    ret = heim_db_rollback(db, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v3), "...");
    heim_release(v);

    ret = heim_db_begin(db, 0, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_set_value(db, NULL, k1, v1, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v1), "...");
    heim_release(v);

    ret = heim_db_commit(db, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v1), "...");
    heim_release(v);

    ret = heim_db_begin(db, 0, NULL);
    heim_assert(!ret, "...");

    ret = heim_db_delete_key(db, NULL, k1, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v == NULL, "...");
    heim_release(v);

    ret = heim_db_rollback(db, NULL);
    heim_assert(!ret, "...");

    v = heim_db_copy_value(db, NULL, k1, NULL);
    heim_assert(v && !heim_cmp(v, v1), "...");
    heim_release(v);

    if (dbtype != NULL) {
	heim_data_t k3 = heim_data_create("value-is-a-dict", strlen("value-is-a-dict"));
	heim_dict_t vdict = heim_dict_create(11);
	heim_db_t db2;

	heim_assert(k3 && vdict, "...");
	ret = heim_dict_set_value(vdict, HSTR("vdict-k1"), heim_number_create(11));
	heim_assert(!ret, "...");
	ret = heim_dict_set_value(vdict, HSTR("vdict-k2"), heim_null_create());
	heim_assert(!ret, "...");
	ret = heim_dict_set_value(vdict, HSTR("vdict-k3"), HSTR("a value"));
	heim_assert(!ret, "...");
	ret = heim_db_set_value(db, NULL, k3, (heim_data_t)vdict, NULL);
	heim_assert(!ret, "...");

	heim_release(vdict);

	db2 = heim_db_create(dbtype, dbname, NULL, NULL);
	heim_assert(db2, "...");

	vdict = (heim_dict_t)heim_db_copy_value(db2, NULL, k3, NULL);
	heim_release(db2);
	heim_release(k3);
	heim_assert(vdict, "...");
	heim_assert(heim_get_tid(vdict) == heim_dict_get_type_id(), "...");

	v = heim_dict_copy_value(vdict, HSTR("vdict-k1"));
	heim_assert(v && !heim_cmp(v, heim_number_create(11)), "...");
	heim_release(v);

	v = heim_dict_copy_value(vdict, HSTR("vdict-k2"));
	heim_assert(v && !heim_cmp(v, heim_null_create()), "...");
	heim_release(v);

	v = heim_dict_copy_value(vdict, HSTR("vdict-k3"));
	heim_assert(v && !heim_cmp(v, HSTR("a value")), "...");
	heim_release(v);

	heim_release(vdict);
    }

    heim_release(db);
    heim_release(k1);
    heim_release(k2);
    heim_release(v1);
    heim_release(v2);
    heim_release(v3);

    return 0;
}

struct test_array_iter_ctx {
    char buf[256];
};

static void test_array_iter(heim_object_t elt, void *arg, int *stop)
{
    struct test_array_iter_ctx *iter_ctx = arg;

    strcat(iter_ctx->buf, heim_string_get_utf8((heim_string_t)elt));
}

static int
test_array()
{
    struct test_array_iter_ctx iter_ctx;
    heim_string_t s1 = heim_string_create("abc");
    heim_string_t s2 = heim_string_create("def");
    heim_string_t s3 = heim_string_create("ghi");
    heim_string_t s4 = heim_string_create("jkl");
    heim_string_t s5 = heim_string_create("mno");
    heim_string_t s6 = heim_string_create("pqr");
    heim_array_t a = heim_array_create();

    if (!s1 || !s2 || !s3 || !s4 || !s5 || !s6 || !a)
	return ENOMEM;

    heim_array_append_value(a, s4);
    heim_array_append_value(a, s5);
    heim_array_insert_value(a, 0, s3);
    heim_array_insert_value(a, 0, s2);
    heim_array_append_value(a, s6);
    heim_array_insert_value(a, 0, s1);

    iter_ctx.buf[0] = '\0';
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "abcdefghijklmnopqr") != 0)
	return 1;

    iter_ctx.buf[0] = '\0';
    heim_array_delete_value(a, 2);
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "abcdefjklmnopqr") != 0)
	return 1;

    iter_ctx.buf[0] = '\0';
    heim_array_delete_value(a, 2);
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "abcdefmnopqr") != 0)
	return 1;

    iter_ctx.buf[0] = '\0';
    heim_array_delete_value(a, 0);
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "defmnopqr") != 0)
	return 1;

    iter_ctx.buf[0] = '\0';
    heim_array_delete_value(a, 2);
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "defmno") != 0)
	return 1;

    heim_array_insert_value(a, 0, s1);
    iter_ctx.buf[0] = '\0';
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "abcdefmno") != 0)
	return 1;

    heim_array_insert_value(a, 0, s2);
    iter_ctx.buf[0] = '\0';
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "defabcdefmno") != 0)
	return 1;

    heim_array_append_value(a, s3);
    iter_ctx.buf[0] = '\0';
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "defabcdefmnoghi") != 0)
	return 1;

    heim_array_append_value(a, s6);
    iter_ctx.buf[0] = '\0';
    heim_array_iterate_f(a, &iter_ctx, test_array_iter);
    if (strcmp(iter_ctx.buf, "defabcdefmnoghipqr") != 0)
	return 1;

    heim_release(s1);
    heim_release(s2);
    heim_release(s3);
    heim_release(s4);
    heim_release(s5);
    heim_release(s6);
    heim_release(a);

    return 0;
}

int
main(int argc, char **argv)
{
    int res = 0;

    res |= test_memory();
    res |= test_mutex();
    res |= test_rwlock();
    res |= test_dict();
    res |= test_auto_release();
    res |= test_string();
    res |= test_error();
    res |= test_json();
    res |= test_path();
    res |= test_db(NULL, NULL);
    res |= test_db("json", argc > 1 ? argv[1] : "test_db.json");
    res |= test_array();

    return res ? 1 : 0;
}

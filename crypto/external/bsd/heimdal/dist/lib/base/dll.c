/*	$NetBSD: dll.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/***********************************************************************
 * Copyright (c) 2016 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

/*
 * This is an implementation of thread-specific storage with
 * destructors.  WIN32 doesn't quite have this.  Instead it has
 * DllMain(), an entry point in every DLL that gets called to notify the
 * DLL of thread/process "attach"/"detach" events.
 *
 * We use __thread (or __declspec(thread)) for the thread-local itself
 * and DllMain() DLL_THREAD_DETACH events to drive destruction of
 * thread-local values.
 *
 * When building in maintainer mode on non-Windows pthread systems this
 * uses a single pthread key instead to implement multiple keys.  This
 * keeps the code from rotting when modified by non-Windows developers.
 */

#include "baselocl.h"

#ifdef WIN32
#include <windows.h>
#endif

#ifdef HEIM_WIN32_TLS
#include <assert.h>
#include <err.h>
#include <heim_threads.h>

#ifndef WIN32
#include <pthread.h>
#endif

/* Logical array of keys that grows lock-lessly */
typedef struct tls_keys tls_keys;
struct tls_keys {
    void (**keys_dtors)(void *);    /* array of destructors         */
    size_t keys_start_idx;          /* index of first destructor    */
    size_t keys_num;
    tls_keys *keys_next;
};

/*
 * Well, not quite locklessly.  We need synchronization primitives to do
 * this locklessly.  An atomic CAS will do.
 */
static HEIMDAL_MUTEX tls_key_defs_lock = HEIMDAL_MUTEX_INITIALIZER;
static tls_keys *tls_key_defs;

/* Logical array of values (per-thread; no locking needed here) */
struct tls_values {
    void **values; /* realloc()ed */
    size_t values_num;
};

static HEIMDAL_THREAD_LOCAL struct tls_values values;

#define DEAD_KEY ((void *)8)

void
heim_w32_service_thread_detach(void *unused)
{
    tls_keys *key_defs;
    void (*dtor)(void*);
    size_t i;

    HEIMDAL_MUTEX_lock(&tls_key_defs_lock);
    key_defs = tls_key_defs;
    HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);

    if (key_defs == NULL)
        return;

    for (i = 0; i < values.values_num; i++) {
        assert(i >= key_defs->keys_start_idx);
        if (i >= key_defs->keys_start_idx + key_defs->keys_num) {
            HEIMDAL_MUTEX_lock(&tls_key_defs_lock);
            key_defs = key_defs->keys_next;
            HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);

            assert(key_defs != NULL);
            assert(i >= key_defs->keys_start_idx);
            assert(i < key_defs->keys_start_idx + key_defs->keys_num);
        }
        dtor = key_defs->keys_dtors[i - key_defs->keys_start_idx];
        if (values.values[i] != NULL && dtor != NULL && dtor != DEAD_KEY)
            dtor(values.values[i]);
        values.values[i] = NULL;
    }
}

#if !defined(WIN32)
static pthread_key_t pt_key;
pthread_once_t pt_once = PTHREAD_ONCE_INIT;

static void
atexit_del_tls_for_thread(void)
{
    heim_w32_service_thread_detach(NULL);
}

static void
create_pt_key(void)
{
    int ret;

    /* The main thread may not execute TLS destructors */
    atexit(atexit_del_tls_for_thread);
    ret = pthread_key_create(&pt_key, heim_w32_service_thread_detach);
    if (ret != 0)
        err(1, "pthread_key_create() failed");
}

#endif

int
heim_w32_key_create(HEIM_PRIV_thread_key *key, void (*dtor)(void *))
{
    tls_keys *key_defs, *new_key_defs;
    size_t i, k;
    int ret = ENOMEM;

#if !defined(WIN32)
    (void) pthread_once(&pt_once, create_pt_key);
    (void) pthread_setspecific(pt_key, DEAD_KEY);
#endif

    HEIMDAL_MUTEX_lock(&tls_key_defs_lock);
    if (tls_key_defs == NULL) {
        /* First key */
        new_key_defs = calloc(1, sizeof(*new_key_defs));
        if (new_key_defs == NULL) {
            HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
            return ENOMEM;
        }
        new_key_defs->keys_num = 8;
        new_key_defs->keys_dtors = calloc(new_key_defs->keys_num,
                                          sizeof(*new_key_defs->keys_dtors));
        if (new_key_defs->keys_dtors == NULL) {
            HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
            free(new_key_defs);
            return ENOMEM;
        }
        tls_key_defs = new_key_defs;
        new_key_defs->keys_dtors[0] = dtor;
        for (i = 1; i < new_key_defs->keys_num; i++)
            new_key_defs->keys_dtors[i] = NULL;
        HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
        return 0;
    }

    for (key_defs = tls_key_defs;
         key_defs != NULL;
         key_defs = key_defs->keys_next) {
        k = key_defs->keys_start_idx;
        for (i = 0; i < key_defs->keys_num; i++, k++) {
            if (key_defs->keys_dtors[i] == NULL) {
                /* Found free slot; use it */
                key_defs->keys_dtors[i] = dtor;
                *key = k;
                HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
                return 0;
            }
        }
        if (key_defs->keys_next != NULL)
            continue;

        /* Grow the registration array */
        /* XXX DRY */
        new_key_defs = calloc(1, sizeof(*new_key_defs));
        if (new_key_defs == NULL)
            break;

        new_key_defs->keys_dtors =
            calloc(key_defs->keys_num + key_defs->keys_num / 2,
                   sizeof(*new_key_defs->keys_dtors));
        if (new_key_defs->keys_dtors == NULL) {
            free(new_key_defs);
            break;
        }
        new_key_defs->keys_start_idx = key_defs->keys_start_idx +
            key_defs->keys_num;
        new_key_defs->keys_num = key_defs->keys_num + key_defs->keys_num / 2;
        new_key_defs->keys_dtors[i] = dtor;
        for (i = 1; i < new_key_defs->keys_num; i++)
            new_key_defs->keys_dtors[i] = NULL;
        key_defs->keys_next = new_key_defs;
        ret = 0;
        break;
    }
    HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
    return ret;
}

static void
key_lookup(HEIM_PRIV_thread_key key, tls_keys **kd,
           size_t *dtor_idx, void (**dtor)(void *))
{
    tls_keys *key_defs;

    if (kd != NULL)
        *kd = NULL;
    if (dtor_idx != NULL)
        *dtor_idx = 0;
    if (dtor != NULL)
        *dtor = NULL;

    HEIMDAL_MUTEX_lock(&tls_key_defs_lock);
    key_defs = tls_key_defs;
    HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);

    while (key_defs != NULL) {
        if (key >= key_defs->keys_start_idx &&
            key < key_defs->keys_start_idx + key_defs->keys_num) {
            if (kd != NULL)
                *kd = key_defs;
            if (dtor_idx != NULL)
                *dtor_idx = key - key_defs->keys_start_idx;
            if (dtor != NULL)
                *dtor = key_defs->keys_dtors[key - key_defs->keys_start_idx];
            return;
        }

        HEIMDAL_MUTEX_lock(&tls_key_defs_lock);
        key_defs = key_defs->keys_next;
        HEIMDAL_MUTEX_unlock(&tls_key_defs_lock);
        assert(key_defs != NULL);
        assert(key >= key_defs->keys_start_idx);
    }
}

int
heim_w32_delete_key(HEIM_PRIV_thread_key key)
{
    tls_keys *key_defs;
    size_t dtor_idx;

    key_lookup(key, &key_defs, &dtor_idx, NULL);
    if (key_defs == NULL)
        return EINVAL;
    key_defs->keys_dtors[dtor_idx] = DEAD_KEY;
    return 0;
}

int
heim_w32_setspecific(HEIM_PRIV_thread_key key, void *value)
{
    void **new_values;
    size_t new_num;
    void (*dtor)(void *);
    size_t i;

#if !defined(WIN32)
    (void) pthread_setspecific(pt_key, DEAD_KEY);
#endif

    key_lookup(key, NULL, NULL, &dtor);
    if (dtor == NULL)
        return EINVAL;

    if (key >= values.values_num) {
        if (values.values_num == 0) {
            values.values = NULL;
            new_num = 8;
        } else {
            new_num = (values.values_num + values.values_num / 2);
        }
        new_values = realloc(values.values, sizeof(void *) * new_num);
        if (new_values == NULL)
            return ENOMEM;
        for (i = values.values_num; i < new_num; i++)
            new_values[i] = NULL;
        values.values = new_values;
        values.values_num = new_num;
    }

    assert(key < values.values_num);

    if (values.values[key] != NULL && dtor != NULL && dtor != DEAD_KEY)
        dtor(values.values[key]);

    values.values[key] = value;
    return 0;
}

void *
heim_w32_getspecific(HEIM_PRIV_thread_key key)
{
    if (key >= values.values_num)
        return NULL;
    return values.values[key];
}

#else
static char dummy;
#endif /* HEIM_WIN32_TLS */

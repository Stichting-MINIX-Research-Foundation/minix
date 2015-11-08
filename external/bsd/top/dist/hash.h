/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* hash.m4h */

/* Interface definition for hash.c */

/* The file hash.h is generated from hash.m4h via the preprocessor M4 */

#ifndef _HASH_H
#define _HASH_H

#include <sys/types.h>

typedef struct pidthr_t {
    pid_t k_pid;
    id_t k_thr;
} pidthr_t;

typedef struct llistitem {
    void *datum;
    struct llistitem *next;
} llistitem;

typedef struct llist {
    llistitem *head;
    unsigned int count;
} llist;

typedef struct bucket {
    llist list;
} bucket_t;

typedef struct hash_table {
    int num_buckets;
    bucket_t *buckets;
} hash_table;

typedef struct hash_pos {
    int num_buckets;
    int curr;
    bucket_t *hash_bucket;
    llistitem *ll_item;
    llistitem *ll_last;
} hash_pos;

hash_table *hash_create(int num);
void hash_sizeinfo(unsigned int *sizes, int max, hash_table *ht);




typedef struct hash_item_uint {
    unsigned int key;
    void *value;
} hash_item_uint;

void *hash_add_uint(hash_table *ht, unsigned int key, void *value);
void *hash_replace_uint(hash_table *ht, unsigned int key, void *value);
void *hash_lookup_uint(hash_table *ht, unsigned int key);
void *hash_remove_uint(hash_table *ht, unsigned int key);
hash_item_uint *hash_first_uint(hash_table *ht, hash_pos *pos);
hash_item_uint *hash_next_uint(hash_pos *pos);
void *hash_remove_pos_uint(hash_pos *pos);


typedef struct hash_item_pid {
    pid_t key;
    void *value;
} hash_item_pid;

void *hash_add_pid(hash_table *ht, pid_t key, void *value);
void *hash_replace_pid(hash_table *ht, pid_t key, void *value);
void *hash_lookup_pid(hash_table *ht, pid_t key);
void *hash_remove_pid(hash_table *ht, pid_t key);
hash_item_pid *hash_first_pid(hash_table *ht, hash_pos *pos);
hash_item_pid *hash_next_pid(hash_pos *pos);
void *hash_remove_pos_pid(hash_pos *pos);


typedef struct hash_item_string {
    char * key;
    void *value;
} hash_item_string;

void *hash_add_string(hash_table *ht, char * key, void *value);
void *hash_replace_string(hash_table *ht, char * key, void *value);
void *hash_lookup_string(hash_table *ht, char * key);
void *hash_remove_string(hash_table *ht, char * key);
hash_item_string *hash_first_string(hash_table *ht, hash_pos *pos);
hash_item_string *hash_next_string(hash_pos *pos);
void *hash_remove_pos_string(hash_pos *pos);


typedef struct hash_item_pidthr {
    pidthr_t key;
    void *value;
} hash_item_pidthr;

void *hash_add_pidthr(hash_table *ht, pidthr_t key, void *value);
void *hash_replace_pidthr(hash_table *ht, pidthr_t key, void *value);
void *hash_lookup_pidthr(hash_table *ht, pidthr_t key);
void *hash_remove_pidthr(hash_table *ht, pidthr_t key);
hash_item_pidthr *hash_first_pidthr(hash_table *ht, hash_pos *pos);
hash_item_pidthr *hash_next_pidthr(hash_pos *pos);
void *hash_remove_pos_pidthr(hash_pos *pos);

#if HAVE_LWPID_T

typedef struct hash_item_lwpid {
    lwpid_t key;
    void *value;
} hash_item_lwpid;

void *hash_add_lwpid(hash_table *ht, lwpid_t key, void *value);
void *hash_replace_lwpid(hash_table *ht, lwpid_t key, void *value);
void *hash_lookup_lwpid(hash_table *ht, lwpid_t key);
void *hash_remove_lwpid(hash_table *ht, lwpid_t key);
hash_item_lwpid *hash_first_lwpid(hash_table *ht, hash_pos *pos);
hash_item_lwpid *hash_next_lwpid(hash_pos *pos);
void *hash_remove_pos_lwpid(hash_pos *pos);

#endif


#endif

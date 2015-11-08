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

/* hash.m4c */

/* The file hash.c is generated from hash.m4c via the preprocessor M4 */

/*
 * Hash table functions:  init, add, lookup, first, next, sizeinfo.
 * This is a conventional "bucket hash".  The key is hashed in to a number
 * less than or equal to the number of buckets and the result is used
 * to index in to the array of buckets.  Each bucket is a linked list
 * that contains all the key/value pairs which hashed to that index.
 */

#include "os.h"

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "hash.h"

static int
next_prime(int x)

{
    double i, j;
    int f;

    i = x;
    while (i++)
    {
	f=1;
	for (j=2; j<i; j++)
	{
	    if ((i/j)==floor(i/j))
	    {
		f=0;
		break;
	    }
	}
	if (f)
	{
	    return (int)i;
	}
    }
    return 1;
}

/* string hashes */

static int
string_hash(hash_table *ht, char *key)

{
    unsigned long s = 0;
    unsigned char ch;
    int shifting = 24;

    while ((ch = (unsigned char)*key++) != '\0')
    {
	if (shifting == 0)
	{
	    s = s + ch;
	    shifting = 24;
	}
	else
	{
	    s = s + (ch << shifting);
	    shifting -= 8;
	}
    }

    return (s % ht->num_buckets);
}

static void ll_init(llist *q)

{
    q->head = NULL;
    q->count = 0;
}

static llistitem *ll_newitem(int size)

{
    llistitem *qi;

    qi = emalloc(sizeof(llistitem) + size);
    qi->datum = ((char *)qi + sizeof(llistitem));
    return qi;
}

static void ll_freeitem(llistitem *li)

{
    free(li);
}

static void ll_add(llist *q, llistitem *new)

{
    new->next = q->head;
    q->head = new;
    q->count++;
}

static void ll_extract(llist *q, llistitem *qi, llistitem *last)

{
    if (last == NULL)
    {
	q->head = qi->next;
    }
    else
    {
	last->next = qi->next;
    }
    qi->next = NULL;
    q->count--;
}

#define LL_FIRST(q) ((q)->head)
#define LL_NEXT(q, qi)  ((qi) != NULL ? (qi)->next : NULL)
#define LL_ISEMPTY(ll)  ((ll)->count == 0)

#ifdef notdef
static llistitem *
ll_first(llist *q)

{
    return q->head;
}

static llistitem *
ll_next(llist *q, llistitem *qi)

{
    return (qi != NULL ? qi->next : NULL);
}

static int
ll_isempty(llist *ll)

{
    return (ll->count == 0);
}
#endif

/*
 * hash_table *hash_create(int num)
 *
 * Creates a hash table structure with at least "num" buckets.
 */

hash_table *
hash_create(int num)

{
    hash_table *result;
    bucket_t *b;
    int bytes;
    int i;

    /* create the resultant structure */
    result = emalloc(sizeof(hash_table));

    /* adjust bucket count to be prime */
    num = next_prime(num);

    /* create the buckets */
    bytes = sizeof(bucket_t) * num;
    result->buckets = b = emalloc(bytes);
    result->num_buckets = num;

    /* create each bucket as a linked list */
    i = num;
    while (--i >= 0)
    {
	ll_init(&(b->list));
	b++;
    }

    return result;
}

/*
 * unsigned int hash_count(hash_table *ht)
 *
 * Return total number of elements contained in hash table.
 */

#ifdef notdef
static unsigned int
hash_count(hash_table *ht)

{
    register int i = 0;
    register int cnt = 0;
    register bucket_t *bucket;

    bucket = ht->buckets;
    while (i++ < ht->num_buckets)
    {
	cnt += bucket->list.count;
	bucket++;
    }

    return cnt;
}
#endif

/*
 * void hash_sizeinfo(unsigned int *sizes, int max, hash_table *ht)
 *
 * Fill in "sizes" with information about bucket lengths in hash
 * table "max".
 */

void 
hash_sizeinfo(unsigned int *sizes, int max, hash_table *ht)

{
    register int i;
    register int idx;
    register bucket_t *bucket;

    memzero(sizes, max * sizeof(unsigned int));

    bucket = ht->buckets;
    i = 0;
    while (i++ < ht->num_buckets)
    {
	idx = bucket->list.count;
	sizes[idx >= max ? max-1 : idx]++;
	bucket++;
    }
}







/*
 * void hash_add_uint(hash_table *ht, unsigned int key, void *value)
 *
 * Add an element to table "ht".  The element is described by
 * "key" and "value".  Return NULL on success.  If the key
 * already exists in the table, then no action is taken and
 * the data for the existing element is returned.
 * Key type is unsigned int
 */

void *
hash_add_uint(hash_table *ht, unsigned int key, void *value)

{
    bucket_t *bucket;
    hash_item_uint *hi;
    hash_item_uint *h;
    llist *ll;
    llistitem *li;
    llistitem *newli;
    unsigned int k1;

    /* allocate the space we will need */
    newli = ll_newitem(sizeof(hash_item_uint));
    hi = (hash_item_uint *)newli->datum;

    /* fill in the values */
    hi->key = key;
    hi->value = value;

    /* hash to the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list to make sure we do not have a duplicate */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	h = (hash_item_uint *)li->datum;
	k1 = h->key;
	if (key == k1)
	{
	    /* found one */
	    break;
	}
	li = LL_NEXT(ll, li);
    }
    li = NULL;

    /* is there already one there? */
    if (li == NULL)
    {
	/* add the unique element to the buckets list */
	ll_add(&(bucket->list), newli);
	return NULL;
    }
    else
    {
	/* free the stuff we just allocated */
	ll_freeitem(newli);
	return ((hash_item_uint *)(li->datum))->value;
    }
}

/*
 * void *hash_replace_uint(hash_table *ht, unsigned int key, void *value)
 *
 * Replace an existing value in the hash table with a new one and
 * return the old value.  If the key does not already exist in
 * the hash table, add a new element and return NULL.
 * Key type is unsigned int
 */

void *
hash_replace_uint(hash_table *ht, unsigned int key, void *value)

{
    bucket_t *bucket;
    hash_item_uint *hi;
    llist *ll;
    llistitem *li;
    void *result = NULL;
    unsigned int k1;

    /* find the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list until we find the existing item */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	hi = (hash_item_uint *)li->datum;
	k1 = hi->key;
	if (key == k1)
	{
	    /* found it: now replace the value with the new one */
	    result = hi->value;
	    hi->value = value;
	    break;
	}
	li = LL_NEXT(ll, li);
    }

    /* if the element wasnt found, add it */
    if (result == NULL)
    {
	li = ll_newitem(sizeof(hash_item_uint));
	hi = (hash_item_uint *)li->datum;
	hi->key = key;
	hi->value = value;
	ll_add(&(bucket->list), li);
    }

    /* return the old value (so it can be freed) */
    return result;
}

/*
 * void *hash_lookup_uint(hash_table *ht, unsigned int key)
 *
 * Look up "key" in "ht" and return the associated value.  If "key"
 * is not found, return NULL.  Key type is unsigned int
 */

void *
hash_lookup_uint(hash_table *ht, unsigned int key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    hash_item_uint *h;
    void *result;
    unsigned int k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	while (li != NULL)
	{
	    h = (hash_item_uint *)li->datum;
	    k1 = h->key;
	    if (key == k1)
	    {
		result = h->value;
		break;
	    }
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * void *hash_remove_uint(hash_table *ht, unsigned int key)
 *
 * Remove the element associated with "key" from the hash table
 * "ht".  Return the value or NULL if the key was not found.
 */

void *
hash_remove_uint(hash_table *ht, unsigned int key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    llistitem *lilast;
    hash_item_uint *hi;
    void *result;
    unsigned int k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	lilast = NULL;
	while (li != NULL)
	{
	    hi = (hash_item_uint *)li->datum;
	    k1 = hi->key;
	    if (key == k1)
	    {
		ll_extract(ll, li, lilast);
		result = hi->value;
		key = hi->key;
		;
		ll_freeitem(li);
		break;
	    }
	    lilast = li;
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * hash_item_uint *hash_first_uint(hash_table *ht, hash_pos *pos)
 *
 * First function to call when iterating through all items in the hash
 * table.  Returns the first item in "ht" and initializes "*pos" to track
 * the current position.
 */

hash_item_uint *
hash_first_uint(hash_table *ht, hash_pos *pos)

{
    /* initialize pos for first item in first bucket */
    pos->num_buckets = ht->num_buckets;
    pos->hash_bucket = ht->buckets;
    pos->curr = 0;
    pos->ll_last = NULL;

    /* find the first non-empty bucket */
    while(pos->hash_bucket->list.count == 0)
    {
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    return NULL;
	}
    }

    /* set and return the first item */
    pos->ll_item = LL_FIRST(&(pos->hash_bucket->list));
    return (hash_item_uint *)pos->ll_item->datum;
}


/*
 * hash_item_uint *hash_next_uint(hash_pos *pos)
 *
 * Return the next item in the hash table, using "pos" as a description
 * of the present position in the hash table.  "pos" also identifies the
 * specific hash table.  Return NULL if there are no more elements.
 */

hash_item_uint *
hash_next_uint(hash_pos *pos)

{
    llistitem *li;

    /* move item to last and check for NULL */
    if ((pos->ll_last = pos->ll_item) == NULL)
    {
	/* are we really at the end of the hash table? */
	if (pos->curr >= pos->num_buckets)
	{
	    /* yes: return NULL */
	    return NULL;
	}
	/* no: regrab first item in current bucket list (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }
    else
    {
	/* get the next item in the llist */
	li = LL_NEXT(&(pos->hash_bucket->list), pos->ll_item);
    }

    /* if its NULL we have to find another bucket */
    while (li == NULL)
    {
	/* locate another bucket */
	pos->ll_last = NULL;

	/* move to the next one */
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    /* at the end of the hash table */
	    pos->ll_item = NULL;
	    return NULL;
	}

	/* get the first element (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }

    /* li is the next element to dish out */
    pos->ll_item = li;
    return (hash_item_uint *)li->datum;
}

/*
 * void *hash_remove_pos_uint(hash_pos *pos)
 *
 * Remove the hash table entry pointed to by position marker "pos".
 * The data from the entry is returned upon success, otherwise NULL.
 */


void *
hash_remove_pos_uint(hash_pos *pos)

{
    llistitem *li;
    void *ans;
    hash_item_uint *hi;

    /* sanity checks */
    if (pos == NULL || pos->ll_last == pos->ll_item)
    {
	return NULL;
    }

    /* at this point pos contains the item to remove (ll_item)
       and its predecesor (ll_last) */
    /* extract the item from the llist */
    li = pos->ll_item;
    ll_extract(&(pos->hash_bucket->list), li, pos->ll_last);

    /* retain the data */
    hi = (hash_item_uint *)li->datum;
    ans = hi->value;

    ll_freeitem(li);

    /* back up to previous item */
    /* its okay for ll_item to be null: hash_next will detect it */
    pos->ll_item = pos->ll_last;

    return ans;
}



/*
 * void hash_add_pid(hash_table *ht, pid_t key, void *value)
 *
 * Add an element to table "ht".  The element is described by
 * "key" and "value".  Return NULL on success.  If the key
 * already exists in the table, then no action is taken and
 * the data for the existing element is returned.
 * Key type is pid_t
 */

void *
hash_add_pid(hash_table *ht, pid_t key, void *value)

{
    bucket_t *bucket;
    hash_item_pid *hi;
    hash_item_pid *h;
    llist *ll;
    llistitem *li;
    llistitem *newli;
    pid_t k1;

    /* allocate the space we will need */
    newli = ll_newitem(sizeof(hash_item_pid));
    hi = (hash_item_pid *)newli->datum;

    /* fill in the values */
    hi->key = key;
    hi->value = value;

    /* hash to the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list to make sure we do not have a duplicate */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	h = (hash_item_pid *)li->datum;
	k1 = h->key;
	if (key == k1)
	{
	    /* found one */
	    break;
	}
	li = LL_NEXT(ll, li);
    }
    li = NULL;

    /* is there already one there? */
    if (li == NULL)
    {
	/* add the unique element to the buckets list */
	ll_add(&(bucket->list), newli);
	return NULL;
    }
    else
    {
	/* free the stuff we just allocated */
	ll_freeitem(newli);
	return ((hash_item_pid *)(li->datum))->value;
    }
}

/*
 * void *hash_replace_pid(hash_table *ht, pid_t key, void *value)
 *
 * Replace an existing value in the hash table with a new one and
 * return the old value.  If the key does not already exist in
 * the hash table, add a new element and return NULL.
 * Key type is pid_t
 */

void *
hash_replace_pid(hash_table *ht, pid_t key, void *value)

{
    bucket_t *bucket;
    hash_item_pid *hi;
    llist *ll;
    llistitem *li;
    void *result = NULL;
    pid_t k1;

    /* find the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list until we find the existing item */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	hi = (hash_item_pid *)li->datum;
	k1 = hi->key;
	if (key == k1)
	{
	    /* found it: now replace the value with the new one */
	    result = hi->value;
	    hi->value = value;
	    break;
	}
	li = LL_NEXT(ll, li);
    }

    /* if the element wasnt found, add it */
    if (result == NULL)
    {
	li = ll_newitem(sizeof(hash_item_pid));
	hi = (hash_item_pid *)li->datum;
	hi->key = key;
	hi->value = value;
	ll_add(&(bucket->list), li);
    }

    /* return the old value (so it can be freed) */
    return result;
}

/*
 * void *hash_lookup_pid(hash_table *ht, pid_t key)
 *
 * Look up "key" in "ht" and return the associated value.  If "key"
 * is not found, return NULL.  Key type is pid_t
 */

void *
hash_lookup_pid(hash_table *ht, pid_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    hash_item_pid *h;
    void *result;
    pid_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	while (li != NULL)
	{
	    h = (hash_item_pid *)li->datum;
	    k1 = h->key;
	    if (key == k1)
	    {
		result = h->value;
		break;
	    }
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * void *hash_remove_pid(hash_table *ht, pid_t key)
 *
 * Remove the element associated with "key" from the hash table
 * "ht".  Return the value or NULL if the key was not found.
 */

void *
hash_remove_pid(hash_table *ht, pid_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    llistitem *lilast;
    hash_item_pid *hi;
    void *result;
    pid_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	lilast = NULL;
	while (li != NULL)
	{
	    hi = (hash_item_pid *)li->datum;
	    k1 = hi->key;
	    if (key == k1)
	    {
		ll_extract(ll, li, lilast);
		result = hi->value;
		key = hi->key;
		;
		ll_freeitem(li);
		break;
	    }
	    lilast = li;
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * hash_item_pid *hash_first_pid(hash_table *ht, hash_pos *pos)
 *
 * First function to call when iterating through all items in the hash
 * table.  Returns the first item in "ht" and initializes "*pos" to track
 * the current position.
 */

hash_item_pid *
hash_first_pid(hash_table *ht, hash_pos *pos)

{
    /* initialize pos for first item in first bucket */
    pos->num_buckets = ht->num_buckets;
    pos->hash_bucket = ht->buckets;
    pos->curr = 0;
    pos->ll_last = NULL;

    /* find the first non-empty bucket */
    while(pos->hash_bucket->list.count == 0)
    {
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    return NULL;
	}
    }

    /* set and return the first item */
    pos->ll_item = LL_FIRST(&(pos->hash_bucket->list));
    return (hash_item_pid *)pos->ll_item->datum;
}


/*
 * hash_item_pid *hash_next_pid(hash_pos *pos)
 *
 * Return the next item in the hash table, using "pos" as a description
 * of the present position in the hash table.  "pos" also identifies the
 * specific hash table.  Return NULL if there are no more elements.
 */

hash_item_pid *
hash_next_pid(hash_pos *pos)

{
    llistitem *li;

    /* move item to last and check for NULL */
    if ((pos->ll_last = pos->ll_item) == NULL)
    {
	/* are we really at the end of the hash table? */
	if (pos->curr >= pos->num_buckets)
	{
	    /* yes: return NULL */
	    return NULL;
	}
	/* no: regrab first item in current bucket list (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }
    else
    {
	/* get the next item in the llist */
	li = LL_NEXT(&(pos->hash_bucket->list), pos->ll_item);
    }

    /* if its NULL we have to find another bucket */
    while (li == NULL)
    {
	/* locate another bucket */
	pos->ll_last = NULL;

	/* move to the next one */
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    /* at the end of the hash table */
	    pos->ll_item = NULL;
	    return NULL;
	}

	/* get the first element (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }

    /* li is the next element to dish out */
    pos->ll_item = li;
    return (hash_item_pid *)li->datum;
}

/*
 * void *hash_remove_pos_pid(hash_pos *pos)
 *
 * Remove the hash table entry pointed to by position marker "pos".
 * The data from the entry is returned upon success, otherwise NULL.
 */


void *
hash_remove_pos_pid(hash_pos *pos)

{
    llistitem *li;
    void *ans;
    hash_item_pid *hi;

    /* sanity checks */
    if (pos == NULL || pos->ll_last == pos->ll_item)
    {
	return NULL;
    }

    /* at this point pos contains the item to remove (ll_item)
       and its predecesor (ll_last) */
    /* extract the item from the llist */
    li = pos->ll_item;
    ll_extract(&(pos->hash_bucket->list), li, pos->ll_last);

    /* retain the data */
    hi = (hash_item_pid *)li->datum;
    ans = hi->value;

    /* free up the space */
    ll_freeitem(li);

    /* back up to previous item */
    /* its okay for ll_item to be null: hash_next will detect it */
    pos->ll_item = pos->ll_last;

    return ans;
}



/*
 * void hash_add_string(hash_table *ht, char * key, void *value)
 *
 * Add an element to table "ht".  The element is described by
 * "key" and "value".  Return NULL on success.  If the key
 * already exists in the table, then no action is taken and
 * the data for the existing element is returned.
 * Key type is char *
 */

void *
hash_add_string(hash_table *ht, char * key, void *value)

{
    bucket_t *bucket;
    hash_item_string *hi;
    hash_item_string *h;
    llist *ll;
    llistitem *li;
    llistitem *newli;
    char * k1;

    /* allocate the space we will need */
    newli = ll_newitem(sizeof(hash_item_string));
    hi = (hash_item_string *)newli->datum;

    /* fill in the values */
    hi->key = estrdup(key);
    hi->value = value;

    /* hash to the bucket */
    bucket = &(ht->buckets[string_hash(ht, key)]);

    /* walk the list to make sure we do not have a duplicate */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	h = (hash_item_string *)li->datum;
	k1 = h->key;
	if (strcmp(key, k1) == 0)
	{
	    /* found one */
	    break;
	}
	li = LL_NEXT(ll, li);
    }
    li = NULL;

    /* is there already one there? */
    if (li == NULL)
    {
	/* add the unique element to the buckets list */
	ll_add(&(bucket->list), newli);
	return NULL;
    }
    else
    {
	/* free the stuff we just allocated */
	ll_freeitem(newli);
	return ((hash_item_string *)(li->datum))->value;
    }
}

/*
 * void *hash_replace_string(hash_table *ht, char * key, void *value)
 *
 * Replace an existing value in the hash table with a new one and
 * return the old value.  If the key does not already exist in
 * the hash table, add a new element and return NULL.
 * Key type is char *
 */

void *
hash_replace_string(hash_table *ht, char * key, void *value)

{
    bucket_t *bucket;
    hash_item_string *hi;
    llist *ll;
    llistitem *li;
    void *result = NULL;
    char * k1;

    /* find the bucket */
    bucket = &(ht->buckets[string_hash(ht, key)]);

    /* walk the list until we find the existing item */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	hi = (hash_item_string *)li->datum;
	k1 = hi->key;
	if (strcmp(key, k1) == 0)
	{
	    /* found it: now replace the value with the new one */
	    result = hi->value;
	    hi->value = value;
	    break;
	}
	li = LL_NEXT(ll, li);
    }

    /* if the element wasnt found, add it */
    if (result == NULL)
    {
	li = ll_newitem(sizeof(hash_item_string));
	hi = (hash_item_string *)li->datum;
	hi->key = estrdup(key);
	hi->value = value;
	ll_add(&(bucket->list), li);
    }

    /* return the old value (so it can be freed) */
    return result;
}

/*
 * void *hash_lookup_string(hash_table *ht, char * key)
 *
 * Look up "key" in "ht" and return the associated value.  If "key"
 * is not found, return NULL.  Key type is char *
 */

void *
hash_lookup_string(hash_table *ht, char * key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    hash_item_string *h;
    void *result;
    char * k1;

    result = NULL;
    if ((bucket = &(ht->buckets[string_hash(ht, key)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	while (li != NULL)
	{
	    h = (hash_item_string *)li->datum;
	    k1 = h->key;
	    if (strcmp(key, k1) == 0)
	    {
		result = h->value;
		break;
	    }
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * void *hash_remove_string(hash_table *ht, char * key)
 *
 * Remove the element associated with "key" from the hash table
 * "ht".  Return the value or NULL if the key was not found.
 */

void *
hash_remove_string(hash_table *ht, char * key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    llistitem *lilast;
    hash_item_string *hi;
    void *result;
    char * k1;

    result = NULL;
    if ((bucket = &(ht->buckets[string_hash(ht, key)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	lilast = NULL;
	while (li != NULL)
	{
	    hi = (hash_item_string *)li->datum;
	    k1 = hi->key;
	    if (strcmp(key, k1) == 0)
	    {
		ll_extract(ll, li, lilast);
		result = hi->value;
		key = hi->key;
		free(key);
		ll_freeitem(li);
		break;
	    }
	    lilast = li;
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * hash_item_string *hash_first_string(hash_table *ht, hash_pos *pos)
 *
 * First function to call when iterating through all items in the hash
 * table.  Returns the first item in "ht" and initializes "*pos" to track
 * the current position.
 */

hash_item_string *
hash_first_string(hash_table *ht, hash_pos *pos)

{
    /* initialize pos for first item in first bucket */
    pos->num_buckets = ht->num_buckets;
    pos->hash_bucket = ht->buckets;
    pos->curr = 0;
    pos->ll_last = NULL;

    /* find the first non-empty bucket */
    while(pos->hash_bucket->list.count == 0)
    {
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    return NULL;
	}
    }

    /* set and return the first item */
    pos->ll_item = LL_FIRST(&(pos->hash_bucket->list));
    return (hash_item_string *)pos->ll_item->datum;
}


/*
 * hash_item_string *hash_next_string(hash_pos *pos)
 *
 * Return the next item in the hash table, using "pos" as a description
 * of the present position in the hash table.  "pos" also identifies the
 * specific hash table.  Return NULL if there are no more elements.
 */

hash_item_string *
hash_next_string(hash_pos *pos)

{
    llistitem *li;

    /* move item to last and check for NULL */
    if ((pos->ll_last = pos->ll_item) == NULL)
    {
	/* are we really at the end of the hash table? */
	if (pos->curr >= pos->num_buckets)
	{
	    /* yes: return NULL */
	    return NULL;
	}
	/* no: regrab first item in current bucket list (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }
    else
    {
	/* get the next item in the llist */
	li = LL_NEXT(&(pos->hash_bucket->list), pos->ll_item);
    }

    /* if its NULL we have to find another bucket */
    while (li == NULL)
    {
	/* locate another bucket */
	pos->ll_last = NULL;

	/* move to the next one */
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    /* at the end of the hash table */
	    pos->ll_item = NULL;
	    return NULL;
	}

	/* get the first element (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }

    /* li is the next element to dish out */
    pos->ll_item = li;
    return (hash_item_string *)li->datum;
}

/*
 * void *hash_remove_pos_string(hash_pos *pos)
 *
 * Remove the hash table entry pointed to by position marker "pos".
 * The data from the entry is returned upon success, otherwise NULL.
 */


void *
hash_remove_pos_string(hash_pos *pos)

{
    llistitem *li;
    void *ans;
    hash_item_string *hi;
    char * key;

    /* sanity checks */
    if (pos == NULL || pos->ll_last == pos->ll_item)
    {
	return NULL;
    }

    /* at this point pos contains the item to remove (ll_item)
       and its predecesor (ll_last) */
    /* extract the item from the llist */
    li = pos->ll_item;
    ll_extract(&(pos->hash_bucket->list), li, pos->ll_last);

    /* retain the data */
    hi = (hash_item_string *)li->datum;
    ans = hi->value;

    /* free up the space */
    key = hi->key;
    free(key);
    ll_freeitem(li);

    /* back up to previous item */
    /* its okay for ll_item to be null: hash_next will detect it */
    pos->ll_item = pos->ll_last;

    return ans;
}



/*
 * void hash_add_pidthr(hash_table *ht, pidthr_t key, void *value)
 *
 * Add an element to table "ht".  The element is described by
 * "key" and "value".  Return NULL on success.  If the key
 * already exists in the table, then no action is taken and
 * the data for the existing element is returned.
 * Key type is pidthr_t
 */

void *
hash_add_pidthr(hash_table *ht, pidthr_t key, void *value)

{
    bucket_t *bucket;
    hash_item_pidthr *hi;
    hash_item_pidthr *h;
    llist *ll;
    llistitem *li;
    llistitem *newli;
    pidthr_t k1;

    /* allocate the space we will need */
    newli = ll_newitem(sizeof(hash_item_pidthr));
    hi = (hash_item_pidthr *)newli->datum;

    /* fill in the values */
    hi->key = key;
    hi->value = value;

    /* hash to the bucket */
    bucket = &(ht->buckets[((key.k_thr * 10000 + key.k_pid) % ht->num_buckets)]);

    /* walk the list to make sure we do not have a duplicate */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	h = (hash_item_pidthr *)li->datum;
	k1 = h->key;
	if ((key.k_pid == k1.k_pid && key.k_thr == k1.k_thr))
	{
	    /* found one */
	    break;
	}
	li = LL_NEXT(ll, li);
    }
    li = NULL;

    /* is there already one there? */
    if (li == NULL)
    {
	/* add the unique element to the buckets list */
	ll_add(&(bucket->list), newli);
	return NULL;
    }
    else
    {
	/* free the stuff we just allocated */
	ll_freeitem(newli);
	return ((hash_item_pidthr *)(li->datum))->value;
    }
}

/*
 * void *hash_replace_pidthr(hash_table *ht, pidthr_t key, void *value)
 *
 * Replace an existing value in the hash table with a new one and
 * return the old value.  If the key does not already exist in
 * the hash table, add a new element and return NULL.
 * Key type is pidthr_t
 */

void *
hash_replace_pidthr(hash_table *ht, pidthr_t key, void *value)

{
    bucket_t *bucket;
    hash_item_pidthr *hi;
    llist *ll;
    llistitem *li;
    void *result = NULL;
    pidthr_t k1;

    /* find the bucket */
    bucket = &(ht->buckets[((key.k_thr * 10000 + key.k_pid) % ht->num_buckets)]);

    /* walk the list until we find the existing item */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	hi = (hash_item_pidthr *)li->datum;
	k1 = hi->key;
	if ((key.k_pid == k1.k_pid && key.k_thr == k1.k_thr))
	{
	    /* found it: now replace the value with the new one */
	    result = hi->value;
	    hi->value = value;
	    break;
	}
	li = LL_NEXT(ll, li);
    }

    /* if the element wasnt found, add it */
    if (result == NULL)
    {
	li = ll_newitem(sizeof(hash_item_pidthr));
	hi = (hash_item_pidthr *)li->datum;
	hi->key = key;
	hi->value = value;
	ll_add(&(bucket->list), li);
    }

    /* return the old value (so it can be freed) */
    return result;
}

/*
 * void *hash_lookup_pidthr(hash_table *ht, pidthr_t key)
 *
 * Look up "key" in "ht" and return the associated value.  If "key"
 * is not found, return NULL.  Key type is pidthr_t
 */

void *
hash_lookup_pidthr(hash_table *ht, pidthr_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    hash_item_pidthr *h;
    void *result;
    pidthr_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[((key.k_thr * 10000 + key.k_pid) % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	while (li != NULL)
	{
	    h = (hash_item_pidthr *)li->datum;
	    k1 = h->key;
	    if ((key.k_pid == k1.k_pid && key.k_thr == k1.k_thr))
	    {
		result = h->value;
		break;
	    }
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * void *hash_remove_pidthr(hash_table *ht, pidthr_t key)
 *
 * Remove the element associated with "key" from the hash table
 * "ht".  Return the value or NULL if the key was not found.
 */

void *
hash_remove_pidthr(hash_table *ht, pidthr_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    llistitem *lilast;
    hash_item_pidthr *hi;
    void *result;
    pidthr_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[((key.k_thr * 10000 + key.k_pid) % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	lilast = NULL;
	while (li != NULL)
	{
	    hi = (hash_item_pidthr *)li->datum;
	    k1 = hi->key;
	    if ((key.k_pid == k1.k_pid && key.k_thr == k1.k_thr))
	    {
		ll_extract(ll, li, lilast);
		result = hi->value;
		key = hi->key;
		;
		ll_freeitem(li);
		break;
	    }
	    lilast = li;
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * hash_item_pidthr *hash_first_pidthr(hash_table *ht, hash_pos *pos)
 *
 * First function to call when iterating through all items in the hash
 * table.  Returns the first item in "ht" and initializes "*pos" to track
 * the current position.
 */

hash_item_pidthr *
hash_first_pidthr(hash_table *ht, hash_pos *pos)

{
    /* initialize pos for first item in first bucket */
    pos->num_buckets = ht->num_buckets;
    pos->hash_bucket = ht->buckets;
    pos->curr = 0;
    pos->ll_last = NULL;

    /* find the first non-empty bucket */
    while(pos->hash_bucket->list.count == 0)
    {
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    return NULL;
	}
    }

    /* set and return the first item */
    pos->ll_item = LL_FIRST(&(pos->hash_bucket->list));
    return (hash_item_pidthr *)pos->ll_item->datum;
}


/*
 * hash_item_pidthr *hash_next_pidthr(hash_pos *pos)
 *
 * Return the next item in the hash table, using "pos" as a description
 * of the present position in the hash table.  "pos" also identifies the
 * specific hash table.  Return NULL if there are no more elements.
 */

hash_item_pidthr *
hash_next_pidthr(hash_pos *pos)

{
    llistitem *li;

    /* move item to last and check for NULL */
    if ((pos->ll_last = pos->ll_item) == NULL)
    {
	/* are we really at the end of the hash table? */
	if (pos->curr >= pos->num_buckets)
	{
	    /* yes: return NULL */
	    return NULL;
	}
	/* no: regrab first item in current bucket list (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }
    else
    {
	/* get the next item in the llist */
	li = LL_NEXT(&(pos->hash_bucket->list), pos->ll_item);
    }

    /* if its NULL we have to find another bucket */
    while (li == NULL)
    {
	/* locate another bucket */
	pos->ll_last = NULL;

	/* move to the next one */
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    /* at the end of the hash table */
	    pos->ll_item = NULL;
	    return NULL;
	}

	/* get the first element (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }

    /* li is the next element to dish out */
    pos->ll_item = li;
    return (hash_item_pidthr *)li->datum;
}

/*
 * void *hash_remove_pos_pidthr(hash_pos *pos)
 *
 * Remove the hash table entry pointed to by position marker "pos".
 * The data from the entry is returned upon success, otherwise NULL.
 */


void *
hash_remove_pos_pidthr(hash_pos *pos)

{
    llistitem *li;
    void *ans;
    hash_item_pidthr *hi;

    /* sanity checks */
    if (pos == NULL || pos->ll_last == pos->ll_item)
    {
	return NULL;
    }

    /* at this point pos contains the item to remove (ll_item)
       and its predecesor (ll_last) */
    /* extract the item from the llist */
    li = pos->ll_item;
    ll_extract(&(pos->hash_bucket->list), li, pos->ll_last);

    /* retain the data */
    hi = (hash_item_pidthr *)li->datum;
    ans = hi->value;

    /* free up the space */
    ll_freeitem(li);

    /* back up to previous item */
    /* its okay for ll_item to be null: hash_next will detect it */
    pos->ll_item = pos->ll_last;

    return ans;
}

#if HAVE_LWPID_T


/*
 * void hash_add_lwpid(hash_table *ht, lwpid_t key, void *value)
 *
 * Add an element to table "ht".  The element is described by
 * "key" and "value".  Return NULL on success.  If the key
 * already exists in the table, then no action is taken and
 * the data for the existing element is returned.
 * Key type is lwpid_t
 */

void *
hash_add_lwpid(hash_table *ht, lwpid_t key, void *value)

{
    bucket_t *bucket;
    hash_item_lwpid *hi;
    hash_item_lwpid *h;
    llist *ll;
    llistitem *li;
    llistitem *newli;
    lwpid_t k1;

    /* allocate the space we will need */
    newli = ll_newitem(sizeof(hash_item_lwpid));
    hi = (hash_item_lwpid *)newli->datum;

    /* fill in the values */
    hi->key = key;
    hi->value = value;

    /* hash to the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list to make sure we do not have a duplicate */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	h = (hash_item_lwpid *)li->datum;
	k1 = h->key;
	if (key == k1)
	{
	    /* found one */
	    break;
	}
	li = LL_NEXT(ll, li);
    }
    li = NULL;

    /* is there already one there? */
    if (li == NULL)
    {
	/* add the unique element to the buckets list */
	ll_add(&(bucket->list), newli);
	return NULL;
    }
    else
    {
	/* free the stuff we just allocated */
	ll_freeitem(newli);
	return ((hash_item_lwpid *)(li->datum))->value;
    }
}

/*
 * void *hash_replace_lwpid(hash_table *ht, lwpid_t key, void *value)
 *
 * Replace an existing value in the hash table with a new one and
 * return the old value.  If the key does not already exist in
 * the hash table, add a new element and return NULL.
 * Key type is lwpid_t
 */

void *
hash_replace_lwpid(hash_table *ht, lwpid_t key, void *value)

{
    bucket_t *bucket;
    hash_item_lwpid *hi;
    llist *ll;
    llistitem *li;
    void *result = NULL;
    lwpid_t k1;

    /* find the bucket */
    bucket = &(ht->buckets[(key % ht->num_buckets)]);

    /* walk the list until we find the existing item */
    ll = &(bucket->list);
    li = LL_FIRST(ll);
    while (li != NULL)
    {
	hi = (hash_item_lwpid *)li->datum;
	k1 = hi->key;
	if (key == k1)
	{
	    /* found it: now replace the value with the new one */
	    result = hi->value;
	    hi->value = value;
	    break;
	}
	li = LL_NEXT(ll, li);
    }

    /* if the element wasnt found, add it */
    if (result == NULL)
    {
	li = ll_newitem(sizeof(hash_item_lwpid));
	hi = (hash_item_lwpid *)li->datum;
	hi->key = key;
	hi->value = value;
	ll_add(&(bucket->list), li);
    }

    /* return the old value (so it can be freed) */
    return result;
}

/*
 * void *hash_lookup_lwpid(hash_table *ht, lwpid_t key)
 *
 * Look up "key" in "ht" and return the associated value.  If "key"
 * is not found, return NULL.  Key type is lwpid_t
 */

void *
hash_lookup_lwpid(hash_table *ht, lwpid_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    hash_item_lwpid *h;
    void *result;
    lwpid_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	while (li != NULL)
	{
	    h = (hash_item_lwpid *)li->datum;
	    k1 = h->key;
	    if (key == k1)
	    {
		result = h->value;
		break;
	    }
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * void *hash_remove_lwpid(hash_table *ht, lwpid_t key)
 *
 * Remove the element associated with "key" from the hash table
 * "ht".  Return the value or NULL if the key was not found.
 */

void *
hash_remove_lwpid(hash_table *ht, lwpid_t key)

{
    bucket_t *bucket;
    llist *ll;
    llistitem *li;
    llistitem *lilast;
    hash_item_lwpid *hi;
    void *result;
    lwpid_t k1;

    result = NULL;
    if ((bucket = &(ht->buckets[(key % ht->num_buckets)])) != NULL)
    {
	ll = &(bucket->list);
	li = LL_FIRST(ll);
	lilast = NULL;
	while (li != NULL)
	{
	    hi = (hash_item_lwpid *)li->datum;
	    k1 = hi->key;
	    if (key == k1)
	    {
		ll_extract(ll, li, lilast);
		result = hi->value;
		key = hi->key;
		;
		ll_freeitem(li);
		break;
	    }
	    lilast = li;
	    li = LL_NEXT(ll, li);
	}
    }
    return result;
}

/*
 * hash_item_lwpid *hash_first_lwpid(hash_table *ht, hash_pos *pos)
 *
 * First function to call when iterating through all items in the hash
 * table.  Returns the first item in "ht" and initializes "*pos" to track
 * the current position.
 */

hash_item_lwpid *
hash_first_lwpid(hash_table *ht, hash_pos *pos)

{
    /* initialize pos for first item in first bucket */
    pos->num_buckets = ht->num_buckets;
    pos->hash_bucket = ht->buckets;
    pos->curr = 0;
    pos->ll_last = NULL;

    /* find the first non-empty bucket */
    while(pos->hash_bucket->list.count == 0)
    {
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    return NULL;
	}
    }

    /* set and return the first item */
    pos->ll_item = LL_FIRST(&(pos->hash_bucket->list));
    return (hash_item_lwpid *)pos->ll_item->datum;
}


/*
 * hash_item_lwpid *hash_next_lwpid(hash_pos *pos)
 *
 * Return the next item in the hash table, using "pos" as a description
 * of the present position in the hash table.  "pos" also identifies the
 * specific hash table.  Return NULL if there are no more elements.
 */

hash_item_lwpid *
hash_next_lwpid(hash_pos *pos)

{
    llistitem *li;

    /* move item to last and check for NULL */
    if ((pos->ll_last = pos->ll_item) == NULL)
    {
	/* are we really at the end of the hash table? */
	if (pos->curr >= pos->num_buckets)
	{
	    /* yes: return NULL */
	    return NULL;
	}
	/* no: regrab first item in current bucket list (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }
    else
    {
	/* get the next item in the llist */
	li = LL_NEXT(&(pos->hash_bucket->list), pos->ll_item);
    }

    /* if its NULL we have to find another bucket */
    while (li == NULL)
    {
	/* locate another bucket */
	pos->ll_last = NULL;

	/* move to the next one */
	pos->hash_bucket++;
	if (++pos->curr >= pos->num_buckets)
	{
	    /* at the end of the hash table */
	    pos->ll_item = NULL;
	    return NULL;
	}

	/* get the first element (might be NULL) */
	li = LL_FIRST(&(pos->hash_bucket->list));
    }

    /* li is the next element to dish out */
    pos->ll_item = li;
    return (hash_item_lwpid *)li->datum;
}

/*
 * void *hash_remove_pos_lwpid(hash_pos *pos)
 *
 * Remove the hash table entry pointed to by position marker "pos".
 * The data from the entry is returned upon success, otherwise NULL.
 */


void *
hash_remove_pos_lwpid(hash_pos *pos)

{
    llistitem *li;
    void *ans;
    hash_item_lwpid *hi;

    /* sanity checks */
    if (pos == NULL || pos->ll_last == pos->ll_item)
    {
	return NULL;
    }

    /* at this point pos contains the item to remove (ll_item)
       and its predecesor (ll_last) */
    /* extract the item from the llist */
    li = pos->ll_item;
    ll_extract(&(pos->hash_bucket->list), li, pos->ll_last);

    /* retain the data */
    hi = (hash_item_lwpid *)li->datum;
    ans = hi->value;

    /* free up the space */
    ll_freeitem(li);

    /* back up to previous item */
    /* its okay for ll_item to be null: hash_next will detect it */
    pos->ll_item = pos->ll_last;

    return ans;
}

#endif

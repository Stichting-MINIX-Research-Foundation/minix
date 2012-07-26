#ifndef lint
static char *rcsid = "$Id: strhash.c,v 1.1.1.1 2003-06-04 00:26:13 marka Exp $";
#endif

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/result.h>
#include <idn/strhash.h>

/*
 * Initially, the number of hash buckets is INITIAL_HASH_SIZE.
 * As the more elements are put in the hash, the number of elements
 * per bucket will exceed THRESHOLD eventually.  When it happens,
 * the number of buckets will be multiplied by FACTOR.
 */
#define INITIAL_HASH_SIZE	67
#define FACTOR			7
#define THRESHOLD		5

#define HASH_MULT		31

typedef struct strhash_entry {
	struct strhash_entry *next;
	unsigned long hash_value;
	char *key;
	void *value;
} strhash_entry_t;

struct idn__strhash {
	int nbins;
	int nelements;
	strhash_entry_t **bins;
};

static unsigned long	hash_value(const char *key);
static strhash_entry_t	*find_entry(strhash_entry_t *entry, const char *key,
				    unsigned long hash);
static strhash_entry_t	*new_entry(const char *key, void *value);
static idn_result_t	expand_bins(idn__strhash_t hash, int new_size);

idn_result_t
idn__strhash_create(idn__strhash_t *hashp) {
	idn__strhash_t hash;
	idn_result_t r;

	TRACE(("idn__strhash_create()\n"));

	assert(hashp != NULL);

	*hashp = NULL;

	if ((hash = malloc(sizeof(struct idn__strhash))) == NULL) {
		WARNING(("idn__strhash_create: malloc failed (hash)\n"));
		return (idn_nomemory);
	}
	hash->nbins = 0;
	hash->nelements = 0;
	hash->bins = NULL;
	if ((r = expand_bins(hash, INITIAL_HASH_SIZE)) != idn_success) {
		WARNING(("idn__strhash_create: malloc failed (bins)\n"));
		free(hash);
		return (r);
	}

	*hashp = hash;

	return (idn_success);
}

void
idn__strhash_destroy(idn__strhash_t hash, idn__strhash_freeproc_t proc) {
	int i;

	assert(hash != NULL && hash->bins != NULL);

	for (i = 0; i < hash->nbins; i++) {
		strhash_entry_t *bin = hash->bins[i];
		strhash_entry_t *next;

		while (bin != NULL) {
			next = bin->next;
			if (proc != NULL)
				(*proc)(bin->value);
			free(bin);
			bin = next;
		}
	}
	free(hash->bins);
	free(hash);
}

idn_result_t
idn__strhash_put(idn__strhash_t hash, const char *key, void *value) {
	unsigned long h, h_index;
	strhash_entry_t *entry;

	assert(hash != NULL && key != NULL);

	h = hash_value(key);
	h_index = h % hash->nbins;

	if ((entry = find_entry(hash->bins[h_index], key, h)) != NULL) {
		/* Entry exists.  Replace the value. */
		entry->value = value;
	} else {
		/* Create new entry. */
		if ((entry = new_entry(key, value)) == NULL) {
			return (idn_nomemory);
		}
		/* Insert it to the list. */
		entry->next = hash->bins[h_index];
		hash->bins[h_index] = entry;
		hash->nelements++;

		if (hash->nelements > hash->nbins * THRESHOLD) {
			idn_result_t r;
			r = expand_bins(hash, hash->nbins * FACTOR);
			if (r != idn_success) {
				TRACE(("idn__strhash_put: hash table "
					"expansion failed\n"));
			}
		}
	}

	return (idn_success);
}

idn_result_t
idn__strhash_get(idn__strhash_t hash, const char *key, void **valuep) {
	unsigned long h;
	strhash_entry_t *entry;

	assert(hash != NULL && key != NULL && valuep != NULL);

	h = hash_value(key);
	entry = find_entry(hash->bins[h % hash->nbins], key, h);
	if (entry == NULL)
		return (idn_noentry);

	*valuep = entry->value;
	return (idn_success);
}

int
idn__strhash_exists(idn__strhash_t hash, const char *key) {
	unsigned long h;

	assert(hash != NULL && key != NULL);

	h = hash_value(key);
	return (find_entry(hash->bins[h % hash->nbins], key, h) != NULL);
}

static unsigned long
hash_value(const char *key) {
	unsigned long h = 0;
	unsigned char *p = (unsigned char *)key;
	int c;

	while ((c = *p++) != '\0') {
		h = h * HASH_MULT + c;
	}
	return (h);
}

static strhash_entry_t *
find_entry(strhash_entry_t *entry, const char *key, unsigned long hash) {
	assert(key != NULL);

	while (entry != NULL) {
		if (entry->hash_value == hash && strcmp(key, entry->key) == 0)
			return (entry);
		entry = entry->next;
	}
	return (NULL);
}

static strhash_entry_t *
new_entry(const char *key, void *value) {
	strhash_entry_t *entry;
	int len;

	assert(key != NULL);

	len = strlen(key) + 1;
	if ((entry = malloc(sizeof(strhash_entry_t) + len)) == NULL) {
		return (NULL);
	}
	entry->next = NULL;
	entry->hash_value = hash_value(key);
	entry->key = (char *)(entry + 1);
	(void)strcpy(entry->key, key);
	entry->value = value;

	return (entry);
}

static idn_result_t
expand_bins(idn__strhash_t hash, int new_size) {
	strhash_entry_t **old_bins, **new_bins;
	int old_size;
	int old_index, new_index;

	new_bins = malloc(sizeof(strhash_entry_t *) * new_size);
	if (new_bins == NULL)
		return (idn_nomemory);

	memset(new_bins, 0, sizeof(strhash_entry_t *) * new_size);

	old_bins = hash->bins;
	old_size = hash->nbins;
	for (old_index = 0; old_index < old_size; old_index++) {
		strhash_entry_t *entries = old_bins[old_index];

		while (entries != NULL) {
			strhash_entry_t *e = entries;

			/* Remove the top element from the linked list. */
			entries = entries->next;

			/* ..and move to the new hash. */
			new_index = e->hash_value % new_size;
			e->next = new_bins[new_index];
			new_bins[new_index] = e;
		}
	}

	hash->nbins = new_size;
	hash->bins = new_bins;

	if (old_bins != NULL)
		free(old_bins);

	return (idn_success);
}

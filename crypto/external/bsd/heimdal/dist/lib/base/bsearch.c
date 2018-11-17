/*	$NetBSD: bsearch.c,v 1.2 2017/01/28 21:31:45 christos Exp $	*/

/*
 * Copyright (c) 2011, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
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
 */

#include "baselocl.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_IO_H
#include <io.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>
#include <assert.h>

/*
 * This file contains functions for binary searching flat text in memory
 * and in text files where each line is a [variable length] record.
 * Each record has a key and an optional value separated from the key by
 * unquoted whitespace.  Whitespace in the key, and leading whitespace
 * for the value, can be quoted with backslashes (but CR and LF must be
 * quoted in such a way that they don't appear in the quoted result).
 *
 * Binary searching a tree are normally a dead simple algorithm.  It
 * turns out that binary searching flat text with *variable* length
 * records is... tricky.  There's no indexes to record beginning bytes,
 * thus any index selected during the search is likely to fall in the
 * middle of a record.  When deciding to search a left sub-tree one
 * might fail to find the last record in that sub-tree on account of the
 * right boundary falling in the middle of it -- the chosen solution to
 * this makes left sub-tree searches slightly less efficient than right
 * sub-tree searches.
 *
 * If binary searching flat text in memory is tricky, using block-wise
 * I/O instead is trickier!  But it's necessary in order to support
 * large files (which we either can't or wouldn't want to read or map
 * into memory).  Each block we read has to be large enough that the
 * largest record can fit in it.  And each block might start and/or end
 * in the middle of a record.  Here it is the right sub-tree searches
 * that are less efficient than left sub-tree searches.
 *
 * bsearch_common() contains the common text block binary search code.
 *
 * _bsearch_text() is the interface for searching in-core text.
 * _bsearch_file() is the interface for block-wise searching files.
 */

struct bsearch_file_handle {
    int fd;          /* file descriptor */
    char *cache;     /* cache bytes */
    char *page;      /* one double-size page worth of bytes */
    size_t file_sz;  /* file size */
    size_t cache_sz; /* cache size */
    size_t page_sz;  /* page size */
};

/* Find a new-line */
static const char *
find_line(const char *buf, size_t i, size_t right)
{
    if (i == 0)
	return &buf[i];
    for (; i < right; i++) {
	if (buf[i] == '\n') {
	    if ((i + 1) < right)
		return &buf[i + 1];
	    return NULL;
	}
    }
    return NULL;
}

/*
 * Common routine for binary searching text in core.
 *
 * Perform a binary search of a char array containing a block from a
 * text file where each line is a record (LF and CRLF supported).  Each
 * record consists of a key followed by an optional value separated from
 * the key by whitespace.  Whitespace can be quoted with backslashes.
 * It's the caller's responsibility to encode/decode keys/values if
 * quoting is desired; newlines should be encoded such that a newline
 * does not appear in the result.
 *
 * All output arguments are optional.
 *
 * Returns 0 if key is found, -1 if not found, or an error code such as
 * ENOMEM in case of error.
 *
 * Inputs:
 *
 * @buf          String to search
 * @sz           Size of string to search
 * @key          Key string to search for
 * @buf_is_start True if the buffer starts with a record, false if it
 *               starts in the middle of a record or if the caller
 *               doesn't know.
 *
 * Outputs:
 *
 * @value        Location to store a copy of the value (caller must free)
 * @location     Record location if found else the location where the
 *               record should be inserted (index into @buf)
 * @cmp	         Set to less than or greater than 0 to indicate that a
 *               key not found would have fit in an earlier or later
 *               part of a file.  Callers should use this to decide
 *               whether to read a block to the left or to the right and
 *               search that.
 * @loops        Location to store a count of bisections required for
 *               search (useful for confirming logarithmic performance)
 */
static int
bsearch_common(const char *buf, size_t sz, const char *key,
	       int buf_is_start, char **value, size_t *location,
	       int *cmp, size_t *loops)
{
    const char *linep;
    size_t key_start, key_len; /* key string in buf */
    size_t val_start, val_len; /* value string in buf */
    int key_cmp = -1;
    size_t k;
    size_t l;    /* left side of buffer for binary search */
    size_t r;    /* right side of buffer for binary search */
    size_t rmax; /* right side of buffer for binary search */
    size_t i;    /* index into buffer, typically in the middle of l and r */
    size_t loop_count = 0;
    int ret = -1;

    if (value)
	*value = NULL;
    if (cmp)
	*cmp = 0;
    if (loops)
	*loops = 0;

    /* Binary search; file should be sorted */
    for (l = 0, r = rmax = sz, i = sz >> 1; i >= l && i < rmax; loop_count++) {
	heim_assert(i < sz, "invalid aname2lname db index");

	/* buf[i] is likely in the middle of a line; find the next line */
	linep = find_line(buf, i, rmax);
	k = linep ? linep - buf : i;
	if (linep == NULL || k >= rmax) {
	    /*
	     * No new line found to the right; search to the left then
	     * but don't change rmax (this isn't optimal, but it's
	     * simple).
	     */
	    if (i == l)
		break;
	    r = i;
	    i = l + ((r - l) >> 1);
	    continue;
	}
	i = k;
	heim_assert(i >= l && i < rmax, "invalid aname2lname db index");

	/* Got a line; check it */

	/* Search for and split on unquoted whitespace */
	val_start = 0;
	for (key_start = i, key_len = 0, val_len = 0, k = i; k < rmax; k++) {
	    if (buf[k] == '\\') {
		k++;
		continue;
	    }
	    if (buf[k] == '\r' || buf[k] == '\n') {
		/* We now know where the key ends, and there's no value */
		key_len = k - i;
		break;
	    }
	    if (!isspace((unsigned char)buf[k]))
		continue;

	    while (k < rmax && isspace((unsigned char)buf[k])) {
		key_len = k - i;
		k++;
	    }
	    if (k < rmax)
		val_start = k;
	    /* Find end of value */
	    for (; k < rmax && buf[k] != '\0'; k++) {
		if (buf[k] == '\r' || buf[k] == '\n') {
		    val_len = k - val_start;
		    break;
		}
	    }
	    break;
	}

	/*
	 * The following logic is for dealing with partial buffers,
	 * which we use for block-wise binary searches of large files
	 */
	if (key_start == 0 && !buf_is_start) {
	    /*
	     * We're at the beginning of a block that might have started
	     * in the middle of a record whose "key" might well compare
	     * as greater than the key we're looking for, so we don't
	     * bother comparing -- we know key_cmp must be -1 here.
	     */
	    key_cmp = -1;
	    break;
	}
	if ((val_len && buf[val_start + val_len] != '\n') ||
	    (!val_len && buf[key_start + key_len] != '\n')) {
	    /*
	     * We're at the end of a block that ends in the middle of a
	     * record whose "key" might well compare as less than the
	     * key we're looking for, so we don't bother comparing -- we
	     * know key_cmp must be >= 0 but we can't tell.  Our caller
	     * will end up reading a double-size block to handle this.
	     */
	    key_cmp = 1;
	    break;
	}

	key_cmp = strncmp(key, &buf[key_start], key_len);
	if (key_cmp == 0 && strlen(key) != key_len)
	    key_cmp = 1;
	if (key_cmp < 0) {
	    /* search left */
	    r = rmax = (linep - buf);
	    i = l + ((r - l) >> 1);
	    if (location)
		*location = key_start;
	} else if (key_cmp > 0) {
	    /* search right */
	    if (l == i)
		break; /* not found */
	    l = i;
	    i = l + ((r - l) >> 1);
	    if (location)
		*location = val_start + val_len;
	} else {
	    /* match! */
	    if (location)
		*location = key_start;
	    ret = 0;
	    if (val_len && value) {
		/* Avoid strndup() so we don't need libroken here yet */
		*value = malloc(val_len + 1);
		if (!*value)
		    ret = errno;
		(void) memcpy(*value, &buf[val_start], val_len);
		(*value)[val_len] = '\0';
	    }
	    break;
	}
    }

    if (cmp)
	*cmp = key_cmp;
    if (loops)
	*loops = loop_count;

    return ret;
}

/*
 * Binary search a char array containing sorted text records separated
 * by new-lines (or CRLF).  Each record consists of a key and an
 * optional value following the key, separated from the key by unquoted
 * whitespace.
 *
 * All output arguments are optional.
 *
 * Returns 0 if key is found, -1 if not found, or an error code such as
 * ENOMEM in case of error.
 *
 * Inputs:
 *
 * @buf      Char array pointer
 * @buf_sz   Size of buf
 * @key      Key to search for
 *
 * Outputs:
 *
 * @value    Location where to put the value, if any (caller must free)
 * @location Record location if found else the location where the record
 *           should be inserted (index into @buf)
 * @loops    Location where to put a number of loops (or comparisons)
 *           needed for the search (useful for benchmarking)
 */
int
_bsearch_text(const char *buf, size_t buf_sz, const char *key,
	       char **value, size_t *location, size_t *loops)
{
    return bsearch_common(buf, buf_sz, key, 1, value, location, NULL, loops);
}

#define MAX_BLOCK_SIZE (1024 * 1024)
#define DEFAULT_MAX_FILE_SIZE (1024 * 1024)
/*
 * Open a file for binary searching.  The file will be read in entirely
 * if it is smaller than @max_sz, else a cache of @max_sz bytes will be
 * allocated.
 *
 * Returns 0 on success, else an error number or -1 if the file is empty.
 *
 * Inputs:
 *
 * @fname   Name of file to open
 * @max_sz  Maximum size of cache to allocate, in bytes (if zero, default)
 * @page_sz Page size (must be a power of two, larger than 256, smaller
 *          than 1MB; if zero use default)
 * 
 * Outputs:
 *
 * @bfh     Handle for use with _bsearch_file() and _bsearch_file_close()
 * @reads   Number of reads performed
 */
int
_bsearch_file_open(const char *fname, size_t max_sz, size_t page_sz,
		    bsearch_file_handle *bfh, size_t *reads)
{
    bsearch_file_handle new_bfh = NULL;
    struct stat st;
    size_t i;
    int fd;
    int ret;

    *bfh = NULL;

    if (reads)
	*reads = 0;

    fd = open(fname, O_RDONLY);
    if (fd == -1)
	return errno;

    if (fstat(fd, &st) == -1) {
	ret = errno;
	goto err;
    }

    if (st.st_size == 0) {
	ret = -1; /* no data -> no binary search */
	goto err;
    }

    /* Validate / default arguments */
    if (max_sz == 0)
	max_sz = DEFAULT_MAX_FILE_SIZE;
    for (i = page_sz; i; i >>= 1) {
	/* Make sure page_sz is a power of two */
	if ((i % 2) && (i >> 1)) {
	    page_sz = 0;
	    break;
	}
    }
    if (page_sz == 0)
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
	page_sz = st.st_blksize;
#else
	page_sz = 4096;
#endif
    for (i = page_sz; i; i >>= 1) {
	/* Make sure page_sz is a power of two */
	if ((i % 2) && (i >> 1)) {
	    /* Can't happen! Filesystems always use powers of two! */
	    page_sz = 4096;
	    break;
	}
    }
    if (page_sz > MAX_BLOCK_SIZE)
	page_sz = MAX_BLOCK_SIZE;

    new_bfh = calloc(1, sizeof (*new_bfh));
    if (new_bfh == NULL) {
	ret = ENOMEM;
	goto err;
    }

    new_bfh->fd = fd;
    new_bfh->page_sz = page_sz;
    new_bfh->file_sz = st.st_size;

    if (max_sz >= st.st_size) {
	/* Whole-file method */
	new_bfh->cache = malloc(st.st_size + 1);
	if (new_bfh->cache) {
	    new_bfh->cache[st.st_size] = '\0';
	    new_bfh->cache_sz = st.st_size;
	    ret = read(fd, new_bfh->cache, st.st_size);
	    if (ret < 0) {
		ret = errno;
		goto err;
	    }
	    if (ret != st.st_size) {
		ret = EIO; /* XXX ??? */
		goto err;
	    }
	    if (reads)
		*reads = 1;
	    (void) close(fd);
	    new_bfh->fd = -1;
	    *bfh = new_bfh;
	    return 0;
	}
    }

    /* Block-size method, or above malloc() failed */
    new_bfh->page = malloc(new_bfh->page_sz << 1);
    if (new_bfh->page == NULL) {
	/* Can't even allocate a single double-size page! */
	ret = ENOMEM;
	goto err;
    }

    new_bfh->cache_sz = max_sz < st.st_size ? max_sz : st.st_size;
    new_bfh->cache = malloc(new_bfh->cache_sz);
    *bfh = new_bfh;

    /*
     * malloc() may have failed because we were asking for a lot of
     * memory, but we may still be able to operate without a cache,
     * so let's not fail.
     */
    if (new_bfh->cache == NULL) {
	new_bfh->cache_sz = 0;
	return 0;
    }

    /* Initialize cache */
    for (i = 0; i < new_bfh->cache_sz; i += new_bfh->page_sz)
	new_bfh->cache[i] = '\0';
    return 0;

err:
    (void) close(fd);
    if (new_bfh) {
	free(new_bfh->page);
	free(new_bfh->cache);
	free(new_bfh);
    }
    return ret;
}

/*
 * Indicate whether the given binary search file handle will be searched
 * with block-wise method.
 */
void
_bsearch_file_info(bsearch_file_handle bfh,
		    size_t *page_sz, size_t *max_sz, int *blockwise)
{
    if (page_sz)
	*page_sz = bfh->page_sz;
    if (max_sz)
	*max_sz = bfh->cache_sz;
    if (blockwise)
	*blockwise = (bfh->file_sz != bfh->cache_sz);
}

/*
 * Close the given binary file search handle.
 *
 * Inputs:
 *
 * @bfh Pointer to variable containing handle to close.
 */
void
_bsearch_file_close(bsearch_file_handle *bfh)
{
    if (!*bfh)
	return;
    if ((*bfh)->fd >= 0)
	(void) close((*bfh)->fd);
    if ((*bfh)->page)
	free((*bfh)->page);
    if ((*bfh)->cache)
	free((*bfh)->cache);
    free(*bfh);
    *bfh = NULL;
}

/*
 * Private function to get a page from a cache.  The cache is a char
 * array of 2^n - 1 double-size page worth of bytes, where n is the
 * number of tree levels that the cache stores.  The cache can be
 * smaller than n implies.
 *
 * The page may or may not be valid.  If the first byte of it is NUL
 * then it's not valid, else it is.
 *
 * Returns 1 if page is in cache and valid, 0 if the cache is too small
 * or the page is invalid.  The page address is output in @buf if the
 * cache is large enough to contain it regardless of whether the page is
 * valid.
 *
 * Inputs:
 *
 * @bfh      Binary search file handle
 * @level    Level in the tree that we want a page for
 * @page_idx Page number in the given level (0..2^level - 1)
 *
 * Outputs:
 *
 * @buf      Set to address of page if the cache is large enough
 */
static int
get_page_from_cache(bsearch_file_handle bfh, size_t level, size_t page_idx,
		    char **buf)
{
    size_t idx = 0;
    size_t page_sz;

    page_sz = bfh->page_sz << 1; /* we use double-size pages in the cache */

    *buf = NULL;

    /*
     * Compute index into cache.  The cache is basically an array of
     * double-size pages.  The first (zeroth) double-size page in the
     * cache will be the middle page of the file -- the root of the
     * tree.  The next two double-size pages will be the left and right
     * pages of the second level in the tree.  The next four double-size
     * pages will be the four pages at the next level.  And so on for as
     * many pages as fit in the cache.
     *
     * The page index is the number of the page at the given level.  We
     * then compute (2^level - 1 + page index) * 2page size, check that
     * we have that in the cache, check that the page has been read (it
     * doesn't start with NUL).
     */
    if (level)
	idx = (1 << level) - 1 + page_idx;
    if (((idx + 1) * page_sz * 2) > bfh->cache_sz)
	return 0;

    *buf = &bfh->cache[idx * page_sz * 2];
    if (bfh->cache[idx * page_sz * 2] == '\0')
	return 0; /* cache[idx] == NUL -> page not loaded in cache */
    return 1;
}

/*
 * Private function to read a page of @page_sz from @fd at offset @off
 * into @buf, outputing the number of bytes read, which will be the same
 * as @page_sz unless the page being read is the last page, in which
 * case the number of remaining bytes in the file will be output.
 *
 * Returns 0 on success or an errno value otherwise (EIO if reads are
 * short).
 *
 * Inputs:
 *
 * @bfh        Binary search file handle
 * @level      Level in the binary search tree that we're at
 * @page_idx   Page "index" at the @level of the tree that we want
 * @page       Actual page number that we want
 * want_double Whether we need a page or double page read
 *
 * Outputs:
 *
 * @buf        Page read or cached
 * @bytes      Bytes read (may be less than page or double page size in
 *             the case of the last page, of course)
 */
static int
read_page(bsearch_file_handle bfh, size_t level, size_t page_idx, size_t page,
	  int want_double, const char **buf, size_t *bytes)
{
    int ret;
    off_t off;
    size_t expected;
    size_t wanted;
    char *page_buf;

    /* Figure out where we're reading and how much */
    off = page * bfh->page_sz;
    if (off < 0)
	return EOVERFLOW;

    wanted = bfh->page_sz << want_double;
    expected = ((bfh->file_sz - off) > wanted) ? wanted : bfh->file_sz - off;

    if (get_page_from_cache(bfh, level, page_idx, &page_buf)) {
	*buf = page_buf;
	*bytes = expected;
	return 0; /* found in cache */
    }


    *bytes = 0;
    *buf = NULL;

    /* OK, we have to read a page or double-size page */

    if (page_buf)
	want_double = 1; /* we'll be caching; we cache double-size pages */
    else
	page_buf = bfh->page; /* we won't cache this page */

    wanted = bfh->page_sz << want_double;
    expected = ((bfh->file_sz - off) > wanted) ? wanted : bfh->file_sz - off;

#ifdef HAVE_PREAD
    ret = pread(bfh->fd, page_buf, expected, off);
#else
    if (lseek(bfh->fd, off, SEEK_SET) == (off_t)-1)
	return errno;
    ret = read(bfh->fd, page_buf, expected);
#endif
    if (ret < 0)
	return errno;
    
    if (ret != expected)
	return EIO; /* XXX ??? */

    *buf = page_buf;
    *bytes = expected;
    return 0;
}

/*
 * Perform a binary search of a file where each line is a record (LF and
 * CRLF supported).  Each record consists of a key followed by an
 * optional value separated from the key by whitespace.  Whitespace can
 * be quoted with backslashes.  It's the caller's responsibility to
 * encode/decode keys/values if quoting is desired; newlines should be
 * encoded such that a newline does not appear in the result.
 *
 * The search is done with block-wise I/O (i.e., the whole file is not
 * read into memory).
 *
 * All output arguments are optional.
 *
 * Returns 0 if key is found, -1 if not found, or an error code such as
 * ENOMEM in case of error.
 *
 * NOTE: We could improve this by not freeing the buffer, instead
 *       requiring that the caller provide it.  Further, we could cache
 *       the top N levels of [double-size] pages (2^N - 1 pages), which
 *       should speed up most searches by reducing the number of reads
 *       by N.
 *
 * Inputs:
 *
 * @fd           File descriptor (file to search)
 * @page_sz      Page size (if zero then the file's st_blksize will be used)
 * @key          Key string to search for
 *
 * Outputs:
 *
 * @value        Location to store a copy of the value (caller must free)
 * @location     Record location if found else the location where the
 *               record should be inserted (index into @buf)
 * @loops        Location to store a count of bisections required for
 *               search (useful for confirming logarithmic performance)
 * @reads        Location to store a count of pages read during search
 *               (useful for confirming logarithmic performance)
 */
int
_bsearch_file(bsearch_file_handle bfh, const char *key,
	       char **value, size_t *location, size_t *loops, size_t *reads)
{
    int ret;
    const char *buf;
    size_t buf_sz;
    size_t page, l, r;
    size_t my_reads = 0;
    size_t my_loops_total = 0;
    size_t my_loops;
    size_t level;        /* level in the tree */
    size_t page_idx = 0; /* page number in the tree level */
    size_t buf_location;
    int cmp;
    int buf_ends_in_eol = 0;
    int buf_is_start = 0;

    if (reads)
	*reads = 0;

    /* If whole file is in memory then search that and we're done */
    if (bfh->file_sz == bfh->cache_sz)
	return _bsearch_text(bfh->cache, bfh->cache_sz, key, value, location, loops);

    /* Else block-wise binary search */

    if (value)
	*value = NULL;
    if (loops)
	*loops = 0;

    l = 0;
    r = (bfh->file_sz / bfh->page_sz) + 1;
    for (level = 0, page = r >> 1; page >= l && page < r ; level++) {
	ret = read_page(bfh, level, page_idx, page, 0, &buf, &buf_sz);
	if (ret != 0)
	    return ret;
	my_reads++;
	if (buf[buf_sz - 1] == '\r' || buf[buf_sz - 1] == '\n')
	    buf_ends_in_eol = 1;
	else
	    buf_ends_in_eol = 0;

	buf_is_start = page == 0 ? 1 : 0;
	ret = bsearch_common(buf, (size_t)buf_sz, key, buf_is_start,
			     value, &buf_location, &cmp, &my_loops);
	if (ret > 0)
	    return ret;
	/* Found or no we update stats */
	my_loops_total += my_loops;
	if (loops)
	    *loops = my_loops_total;
	if (reads)
	    *reads = my_reads;
	if (location)
	    *location = page * bfh->page_sz + buf_location;
	if (ret == 0)
	    return 0; /* found! */
	/* Not found */
	if (cmp < 0) {
	    /* Search left */
	    page_idx <<= 1;
	    r = page;
	    page = l + ((r - l) >> 1);
	    continue;
	} else {
	    /*
	     * Search right, but first search the current and next
	     * blocks in case that the record we're looking for either
	     * straddles the boundary between this and the next record,
	     * or in case the record starts exactly at the next page.
	     */
	    heim_assert(cmp > 0, "cmp > 0");

	    if (!buf_ends_in_eol || page == l || page == (r - 1)) {
		ret = read_page(bfh, level, page_idx, page, 1, &buf, &buf_sz);
		if (ret != 0)
		    return ret;
		my_reads++;

		buf_is_start = page == l ? 1 : 0;

		ret = bsearch_common(buf, (size_t)buf_sz, key, buf_is_start,
				     value, &buf_location, &cmp, &my_loops);
		if (ret > 0)
		    return ret;
		my_loops_total += my_loops;
		if (loops)
		    *loops = my_loops_total;
		if (reads)
		    *reads = my_reads;
		if (location)
		    *location = page * bfh->page_sz + buf_location;
		if (ret == 0)
		    return 0;
	    }

	    /* Oh well, search right */
	    if (l == page && r == (l + 1))
		break;
	    page_idx = (page_idx << 1) + 1;
	    l = page;
	    page = l + ((r - l) >> 1);
	    continue;
	}
    }
    return -1;
}


static int
stdb_open(void *plug, const char *dbtype, const char *dbname,
	     heim_dict_t options, void **db, heim_error_t *error)
{
    bsearch_file_handle bfh;
    char *p;
    int ret;

    if (error)
	*error = NULL;
    if (dbname == NULL || *dbname == '\0') {
	if (error)
	    *error = heim_error_create(EINVAL,
				       N_("DB name required for sorted-text DB "
					  "plugin", ""));
	return EINVAL;
    }
    p = strrchr(dbname, '.');
    if (p == NULL || strcmp(p, ".txt") != 0) {
	if (error)
	    *error = heim_error_create(ENOTSUP,
				       N_("Text file (name ending in .txt) "
				       "required for sorted-text DB plugin",
				       ""));
	return ENOTSUP;
    }

    ret = _bsearch_file_open(dbname, 0, 0, &bfh, NULL);
    if (ret)
	return ret;

    *db = bfh;
    return 0;
}

static int
stdb_close(void *db, heim_error_t *error)
{
    bsearch_file_handle bfh = db;

    if (error)
	*error = NULL;
    _bsearch_file_close(&bfh);
    return 0;
}

static heim_data_t
stdb_copy_value(void *db, heim_string_t table, heim_data_t key,
	       heim_error_t *error)
{
    bsearch_file_handle bfh = db;
    const char *k;
    char *v;
    heim_data_t value;
    int ret;

    if (error)
	*error = NULL;

    if (table == NULL)
	table = HSTR("");

    if (table != HSTR(""))
	return NULL;

    if (heim_get_tid(key) == HEIM_TID_STRING)
	k = heim_string_get_utf8((heim_string_t)key);
    else
	k = (const char *)heim_data_get_ptr(key);
    ret = _bsearch_file(bfh, k, &v, NULL, NULL, NULL);
    if (ret != 0) {
	if (ret > 0 && error)
	    *error = heim_error_create(ret, "%s", strerror(ret));
	return NULL;
    }
    value = heim_data_create(v, strlen(v));
    free(v);
    /* XXX Handle ENOMEM */
    return value;
}

struct heim_db_type heim_sorted_text_file_dbtype = {
    1, stdb_open, NULL, stdb_close, NULL, NULL, NULL, NULL, NULL, NULL,
    stdb_copy_value, NULL, NULL, NULL
};

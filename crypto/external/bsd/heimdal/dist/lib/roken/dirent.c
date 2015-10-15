/*	$NetBSD: dirent.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

/***********************************************************************
 * Copyright (c) 2009, Secure Endpoints Inc.
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
 **********************************************************************/

#include<config.h>

#include <stdlib.h>
#include <io.h>
#include <string.h>
#include <errno.h>
#include "dirent.h"

#ifndef _WIN32
#error Only implemented for Win32
#endif

struct _dirent_dirinfo {
    int             magic;
    long            n_entries;
    long            nc_entries;
    long            cursor;
    struct dirent **entries;
};
#define DIRINFO_MAGIC 0xf8c0639d
#define IS_DP(p) ((p) && ((DIR *)(p))->magic == DIRINFO_MAGIC)

#define INITIAL_ENTRIES 16

/**
 * Create a filespec for use with _findfirst() using a path spec
 *
 * If the last component of the path spec contains wildcards, we let
 * it be.  If the last component doesn't end with a slash, we add one.
 */
static const char *
filespec_from_dir_path(const char * path, char * buffer, size_t cch_buffer)
{
    char *comp, *t;
    size_t pos;
    int found_sep = 0;

    if (strcpy_s(buffer, cch_buffer, path) != 0)
        return NULL;

    comp = strrchr(buffer, '\\');
    if (comp == NULL)
        comp = buffer;
    else
        found_sep = 1;

    t = strrchr(comp, '/');
    if (t != NULL) {
        comp = t;
        found_sep = 1;
    }

    if (found_sep)
        comp++;

    pos = strcspn(comp, "*?");
    if (comp[pos] != '\0')
        return buffer;

    /* We don't append a slash if pos == 0 because that changes the
     * meaning:
     *
     * "*.*" is all files in the current directory.
     * "\*.*" is all files in the root directory of the current drive.
     */
    if (pos > 0 && comp[pos - 1] != '\\' &&
        comp[pos - 1] != '/') {
        strcat_s(comp, cch_buffer - (comp - buffer), "\\");
    }

    strcat_s(comp, cch_buffer - (comp - buffer), "*.*");

    return buffer;
}

ROKEN_LIB_FUNCTION DIR * ROKEN_LIB_CALL
opendir(const char * path)
{
    DIR *              dp;
    struct _finddata_t fd;
    intptr_t           fd_handle;
    const char         *filespec;
    char               path_buffer[1024];

    memset(&fd, 0, sizeof(fd));

    filespec = filespec_from_dir_path(path, path_buffer, sizeof(path_buffer)/sizeof(char));
    if (filespec == NULL)
        return NULL;

    fd_handle = _findfirst(filespec, &fd);

    if (fd_handle == -1)
        return NULL;

    dp = malloc(sizeof(*dp));
    if (dp == NULL)
        goto done;

    memset(dp, 0, sizeof(*dp));
    dp->magic      = DIRINFO_MAGIC;
    dp->cursor     = 0;
    dp->n_entries  = 0;
    dp->nc_entries = INITIAL_ENTRIES;
    dp->entries    = calloc(dp->nc_entries, sizeof(dp->entries[0]));

    if (dp->entries == NULL) {
        closedir(dp);
        dp = NULL;
        goto done;
    }

    do {
        size_t len = strlen(fd.name);
        struct dirent * e;

        if (dp->n_entries == dp->nc_entries) {
	    struct dirent ** ne;

            dp->nc_entries *= 2;
            ne = realloc(dp->entries, sizeof(dp->entries[0]) * dp->nc_entries);

            if (ne == NULL) {
                closedir(dp);
                dp = NULL;
                goto done;
            }

	    dp->entries = ne;
        }

        e = malloc(sizeof(*e) + len * sizeof(char));
        if (e == NULL) {
            closedir(dp);
            dp = NULL;
            goto done;
        }

        e->d_ino = 0;           /* no inodes :( */
        strcpy_s(e->d_name, len + 1, fd.name);

        dp->entries[dp->n_entries++] = e;

    } while (_findnext(fd_handle, &fd) == 0);

 done:
    if (fd_handle != -1)
        _findclose(fd_handle);

    return dp;
}

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
closedir(DIR * dp)
{
    if (!IS_DP(dp))
        return EINVAL;

    if (dp->entries) {
        long i;

        for (i=0; i < dp->n_entries; i++) {
            free(dp->entries[i]);
        }

        free(dp->entries);
    }

    free(dp);

    return 0;
}

ROKEN_LIB_FUNCTION struct dirent * ROKEN_LIB_CALL
readdir(DIR * dp)
{
    if (!IS_DP(dp) ||
        dp->cursor < 0 ||
        dp->cursor >= dp->n_entries)

        return NULL;

    return dp->entries[dp->cursor++];
}

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
rewinddir(DIR * dp)
{
    if (IS_DP(dp))
        dp->cursor = 0;
}

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
seekdir(DIR * dp, long offset)
{
    if (IS_DP(dp) && offset >= 0 && offset < dp->n_entries)
        dp->cursor = offset;
}

ROKEN_LIB_FUNCTION long ROKEN_LIB_CALL
telldir(DIR * dp)
{
    return dp->cursor;
}

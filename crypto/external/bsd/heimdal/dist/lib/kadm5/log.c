/*	$NetBSD: log.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
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

#include "kadm5_locl.h"
#include "heim_threads.h"

__RCSID("$NetBSD: log.c,v 1.2 2017/01/28 21:31:49 christos Exp $");

/*
 * A log consists of a sequence of records of this form:
 *
 * version number		4 bytes -\
 * time in seconds		4 bytes   +> preamble --+> header
 * operation (enum kadm_ops)	4 bytes -/             /
 * n, length of payload		4 bytes --------------+
 *      PAYLOAD DATA...		n bytes
 * n, length of payload		4 bytes ----------------+> trailer
 * version number		4 bytes ->postamble ---/
 *
 * I.e., records have a header and a trailer so that knowing the offset
 * of an record's start or end one can traverse the log forwards and
 * backwards.
 *
 * The log always starts with a nop record (uber record) that contains the
 * offset (8 bytes) of the first unconfirmed record (typically EOF), and the
 * version number and timestamp of the preceding last confirmed record:
 *
 * offset of next new record    8 bytes
 * last record time             4 bytes
 * last record version number   4 bytes
 *
 * When an iprop slave receives a complete database, it saves that version as
 * the last confirmed version, without writing any other records to the log.  We
 * use that version as the basis for further updates.
 *
 * kadm5 write operations are done in this order:
 *
 *  - replay unconfirmed log records
 *  - write (append) and fsync() the log record for the kadm5 update
 *  - update the HDB (which includes fsync() or moral equivalent)
 *  - update the log uber record to mark the log record written as
 *    confirmed (not fsync()ed)
 *
 * This makes it possible and safe to seek to the logical end of the log
 * (that is, the end of the last confirmed record) without traversing
 * the whole log forward from offset zero.  Unconfirmed records (which
 * -currently- should never be more than one) can then be found (and
 * rolled forward) by traversing forward from the logical end of the
 * log.  The trailers make it possible to traverse the log backwards
 * from the logical end.
 *
 * This also makes the log + the HDB a two-phase commit with
 * roll-forward system.
 *
 * HDB entry exists and HDB entry does not exist errors occurring during
 * replay of unconfirmed records are ignored.  This is because the
 * corresponding HDB update might have completed.  But also because a
 * change to add aliases to a principal can fail because we don't check
 * for alias conflicts before going ahead with the write operation.
 *
 * Non-sensical and incomplete log records found during roll-forward are
 * truncated.  A log record is non-sensical if its header and trailer
 * don't match.
 *
 * Recovery (by rolling forward) occurs at the next read or write by a
 * kadm5 API reader (e.g., kadmin), but not by an hdb API reader (e.g.,
 * the KDC).  This means that, e.g., a principal rename could fail in
 * between the store and the delete, and recovery might not take place
 * until the next write operation.
 *
 * The log record payload format for create is:
 *
 * DER-encoded HDB_entry        n bytes
 *
 * The log record payload format for update is:
 *
 * mask                         4 bytes
 * DER-encoded HDB_entry        n-4 bytes
 *
 * The log record payload format for delete is:
 *
 * krb5_store_principal         n bytes
 *
 * The log record payload format for rename is:
 *
 * krb5_store_principal         m bytes (old principal name)
 * DER-encoded HDB_entry        n-m bytes (new record)
 *
 * The log record payload format for nop varies:
 *
 *  - The zeroth record in new logs is a nop with a 16 byte payload:
 *
 *    offset of end of last confirmed record        8 bytes
 *    timestamp of last confirmed record            4 bytes
 *    version number of last confirmed record       4 bytes
 *
 *  - New non-zeroth nop records:
 *
 *    nop type                                      4 bytes
 *
 *  - Old nop records:
 *
 *    version number                                4 bytes
 *    timestamp                                     4 bytes
 *
 * Upon initialization, the log's uber record will have version 1, and
 * will be followed by a nop record with version 2.  The version numbers
 * of additional records will be monotonically increasing.
 *
 * Truncation (kadm5_log_truncate()) takes some N > 0 records from the
 * tail of the log and writes them to the beginning of the log after an
 * uber record whose version will then be one less than the first of
 * those records.
 *
 * On masters the log should never have more than one unconfirmed
 * record, but slaves append all of a master's "diffs" and then call
 * kadm5_log_recover() to recover.
 */

/*
 * HDB and log lock order on the master:
 *
 * 1) open and lock the HDB
 * 2) open and lock the log
 * 3) do something
 * 4) unlock and close the log
 * 5) repeat (2)..(4) if desired
 * 6) unlock and close the HDB
 *
 * The kadmin -l lock command can be used to hold the HDB open and
 * locked for multiple operations.
 *
 * HDB and log lock order on the slave:
 *
 * 1) open and lock the log
 * 2) open and lock the HDB
 * 3) replay entries
 * 4) unlock and close the HDB
 * 5) repeat (2)..(4) until signaled
 * 6) unlock and close the HDB
 *
 * The slave doesn't want to allow other local writers, after all, thus
 * the order is reversed.  This means that using "kadmin -l" on a slave
 * will deadlock with ipropd-slave -- don't do that.
 */

#define LOG_HEADER_SZ   ((off_t)(sizeof(uint32_t) * 4))
#define LOG_TRAILER_SZ  ((off_t)(sizeof(uint32_t) * 2))
#define LOG_WRAPPER_SZ  ((off_t)(LOG_HEADER_SZ + LOG_TRAILER_SZ))
#define LOG_UBER_LEN    ((off_t)(sizeof(uint64_t) + sizeof(uint32_t) * 2))
#define LOG_UBER_SZ     ((off_t)(LOG_WRAPPER_SZ + LOG_UBER_LEN))

#define LOG_NOPEEK 0
#define LOG_DOPEEK 1

/*
 * Read the header of the record starting at the current offset into sp.
 *
 * Preserves sp's offset on success if `peek', else skips the header.
 *
 * Preserves sp's offset on failure where possible.
 */
static kadm5_ret_t
get_header(krb5_storage *sp, int peek, uint32_t *verp, uint32_t *tstampp,
           enum kadm_ops *opp, uint32_t *lenp)
{
    krb5_error_code ret;
    uint32_t tstamp, op, len;
    off_t off, new_off;

    if (tstampp == NULL)
        tstampp = &tstamp;
    if (lenp == NULL)
        lenp = &len;

    *verp = 0;
    *tstampp = 0;
    if (opp != NULL)
        *opp = kadm_nop;
    *lenp = 0;

    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off < 0)
        return errno;
    ret = krb5_ret_uint32(sp, verp);
    if (ret == HEIM_ERR_EOF) {
        (void) krb5_storage_seek(sp, off, SEEK_SET);
        return HEIM_ERR_EOF;
    }
    if (ret)
        goto log_corrupt;
    ret = krb5_ret_uint32(sp, tstampp);
    if (ret)
        goto log_corrupt;

    /* Note: sizeof(*opp) might not == sizeof(op) */
    ret = krb5_ret_uint32(sp, &op);
    if (ret)
        goto log_corrupt;
    if (opp != NULL)
        *opp = op;

    ret = krb5_ret_uint32(sp, lenp);
    if (ret)
        goto log_corrupt;

    /* Restore offset if requested */
    if (peek == LOG_DOPEEK) {
        new_off = krb5_storage_seek(sp, off, SEEK_SET);
        if (new_off == -1)
            return errno;
        if (new_off != off)
            return EIO;
    }

    return 0;

log_corrupt:
    (void) krb5_storage_seek(sp, off, SEEK_SET);
    return KADM5_LOG_CORRUPT;
}

/*
 * Seek to the start of the preceding record's header and returns its
 * offset.  If sp is at offset zero this sets *verp = 0 and returns 0.
 *
 * Does not verify the header of the previous entry.
 *
 * On error returns -1, setting errno (possibly to a kadm5_ret_t or
 * krb5_error_code value) and preserves sp's offset where possible.
 */
static off_t
seek_prev(krb5_storage *sp, uint32_t *verp, uint32_t *lenp)
{
    krb5_error_code ret;
    uint32_t len, ver;
    off_t off_len;
    off_t off, new_off;

    if (lenp == NULL)
        lenp = &len;
    if (verp == NULL)
        verp = &ver;

    *verp = 0;
    *lenp = 0;

    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off < 0)
        return off;
    if (off == 0)
        return 0;

    /* Check that `off' allows for the record's header and trailer */
    if (off < LOG_WRAPPER_SZ)
        goto log_corrupt;

    /* Get the previous entry's length and version from its trailer */
    new_off = krb5_storage_seek(sp, -8, SEEK_CUR);
    if (new_off == -1)
        return -1;
    if (new_off != off - 8) {
        errno = EIO;
        return -1;
    }
    ret = krb5_ret_uint32(sp, lenp);
    if (ret)
        goto log_corrupt;

    /* Check for overflow/sign extension */
    off_len = (off_t)*lenp;
    if (off_len < 0 || *lenp != (uint32_t)off_len)
        goto log_corrupt;

    ret = krb5_ret_uint32(sp, verp);
    if (ret)
        goto log_corrupt;

    /* Check that `off' allows for the record */
    if (off < LOG_WRAPPER_SZ + off_len)
        goto log_corrupt;

    /* Seek backwards to the entry's start */
    new_off = krb5_storage_seek(sp, -(LOG_WRAPPER_SZ + off_len), SEEK_CUR);
    if (new_off == -1)
        return -1;
    if (new_off != off - (LOG_WRAPPER_SZ + off_len)) {
        errno = EIO;
        return -1;
    }
    return new_off;

log_corrupt:
    (void) krb5_storage_seek(sp, off, SEEK_SET);
    errno = KADM5_LOG_CORRUPT;
    return -1;
}

/*
 * Seek to the start of the next entry's header.
 *
 * On error returns -1 and preserves sp's offset.
 */
static off_t
seek_next(krb5_storage *sp)
{
    krb5_error_code ret;
    uint32_t ver, ver2, len, len2;
    enum kadm_ops op;
    uint32_t tstamp;
    off_t off, off_len, new_off;

    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off < 0)
        return off;

    errno = get_header(sp, LOG_NOPEEK, &ver, &tstamp, &op, &len);
    if (errno)
        return -1;

    /* Check for overflow */
    off_len = len;
    if (off_len < 0)
        goto log_corrupt;

    new_off = krb5_storage_seek(sp, off_len, SEEK_CUR);
    if (new_off == -1) {
        (void) krb5_storage_seek(sp, off, SEEK_SET);
        return -1;
    }
    if (new_off != off + LOG_HEADER_SZ + off_len)
        goto log_corrupt;
    ret = krb5_ret_uint32(sp, &len2);
    if (ret || len2 != len)
        goto log_corrupt;
    ret = krb5_ret_uint32(sp, &ver2);
    if (ret || ver2 != ver)
        goto log_corrupt;
    new_off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (new_off == -1) {
        (void) krb5_storage_seek(sp, off, SEEK_SET);
        return -1;
    }
    if (new_off != off + off_len + LOG_WRAPPER_SZ)
        goto log_corrupt;

    return off + off_len + LOG_WRAPPER_SZ;

log_corrupt:
    (void) krb5_storage_seek(sp, off, SEEK_SET);
    errno = KADM5_LOG_CORRUPT;
    return -1;
}

/*
 * Get the version of the entry ending at the current offset into sp.
 * If it is the uber record, return its nominal version instead.
 *
 * Returns HEIM_ERR_EOF if sp is at offset zero.
 *
 * Preserves sp's offset.
 */
static kadm5_ret_t
get_version_prev(krb5_storage *sp, uint32_t *verp, uint32_t *tstampp)
{
    krb5_error_code ret;
    uint32_t ver, ver2, len, len2;
    off_t off, prev_off, new_off;

    *verp = 0;
    if (tstampp != NULL)
        *tstampp = 0;

    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off < 0)
        return errno;
    if (off == 0)
        return HEIM_ERR_EOF;

    /* Read the trailer and seek back */
    prev_off = seek_prev(sp, &ver, &len);
    if (prev_off == -1)
        return errno;

    /* Uber record? Return nominal version. */
    if (prev_off == 0 && len == LOG_UBER_LEN && ver == 0) {
        /* Skip 8 byte offset and 4 byte time */
        if (krb5_storage_seek(sp, LOG_HEADER_SZ + 12, SEEK_SET)
            != LOG_HEADER_SZ + 12)
            return errno;
        ret = krb5_ret_uint32(sp, verp);
        if (krb5_storage_seek(sp, 0, SEEK_SET) != 0)
            return errno;
        if (ret != 0)
            return ret;
    } else {
        *verp = ver;
    }

    /* Verify that the trailer matches header */
    ret = get_header(sp, LOG_NOPEEK, &ver2, tstampp, NULL, &len2);
    if (ret || ver != ver2 || len != len2)
        goto log_corrupt;

    /* Preserve offset */
    new_off = krb5_storage_seek(sp, off, SEEK_SET);
    if (new_off == -1)
        return errno;
    if (new_off != off) {
        errno = EIO;
        return errno;
    }
    return 0;

log_corrupt:
    (void) krb5_storage_seek(sp, off, SEEK_SET);
    return KADM5_LOG_CORRUPT;
}

static size_t
get_max_log_size(krb5_context context)
{
    off_t n;

    /* Use database-label-specific lookup?  No, ETOOHARD. */
    /* Default to 50MB max log size */
    n = krb5_config_get_int_default(context, NULL, 52428800,
                                    "kdc",
                                    "log-max-size",
                                    NULL);
    if (n >= 4 * (LOG_UBER_LEN + LOG_WRAPPER_SZ) && n == (size_t)n)
        return (size_t)n;
    return 0;
}

static kadm5_ret_t truncate_if_needed(kadm5_server_context *);
static krb5_storage *log_goto_first(kadm5_server_context *, int);

/*
 * Get the version and timestamp metadata of either the first, or last
 * confirmed entry in the log.
 *
 * If `which' is LOG_VERSION_UBER, then this gets the version number of the uber
 * uber record which must be 0, or else we need to upgrade the log.
 *
 * If `which' is LOG_VERSION_FIRST, then this gets the metadata for the
 * logically first entry past the uberblock, or returns HEIM_EOF if
 * only the uber record is present.
 *
 * If `which' is LOG_VERSION_LAST, then this gets metadata for the last
 * confirmed entry's version and timestamp. If only the uber record is present,
 * then the version will be its "nominal" version, which may differ from its
 * actual version (0).
 *
 * The `fd''s offset will be set to the start of the header of the entry
 * identified by `which'.
 */
kadm5_ret_t
kadm5_log_get_version_fd(kadm5_server_context *server_context, int fd,
                         int which, uint32_t *ver, uint32_t *tstamp)
{
    kadm5_ret_t ret = 0;
    krb5_storage *sp;
    enum kadm_ops op = kadm_get;
    uint32_t len = 0;
    uint32_t tmp;

    if (fd == -1)
        return 0; /* /dev/null */

    if (tstamp == NULL)
        tstamp = &tmp;

    *ver = 0;
    *tstamp = 0;

    switch (which) {
    case LOG_VERSION_LAST:
        sp = kadm5_log_goto_end(server_context, fd);
        if (sp == NULL)
            return errno;
        ret = get_version_prev(sp, ver, tstamp);
        krb5_storage_free(sp);
        break;
    case LOG_VERSION_FIRST:
        sp = log_goto_first(server_context, fd);
        if (sp == NULL)
            return errno;
        ret = get_header(sp, LOG_DOPEEK, ver, tstamp, NULL, NULL);
        krb5_storage_free(sp);
        break;
    case LOG_VERSION_UBER:
        sp = krb5_storage_from_fd(server_context->log_context.log_fd);
        if (sp == NULL)
            return errno;
        if (krb5_storage_seek(sp, 0, SEEK_SET) == 0)
            ret = get_header(sp, LOG_DOPEEK, ver, tstamp, &op, &len);
        else
            ret = errno;
        if (ret == 0 && (op != kadm_nop || len != LOG_UBER_LEN || *ver != 0))
            ret = KADM5_LOG_NEEDS_UPGRADE;
        krb5_storage_free(sp);
        break;
    default:
        return ENOTSUP;
    }

    return ret;
}

/* Get the version of the last confirmed entry in the log */
kadm5_ret_t
kadm5_log_get_version(kadm5_server_context *server_context, uint32_t *ver)
{
    return kadm5_log_get_version_fd(server_context,
                                    server_context->log_context.log_fd,
                                    LOG_VERSION_LAST, ver, NULL);
}

/* Sets the version in the context, but NOT in the log */
kadm5_ret_t
kadm5_log_set_version(kadm5_server_context *context, uint32_t vno)
{
    kadm5_log_context *log_context = &context->log_context;

    log_context->version = vno;
    return 0;
}

/*
 * Open the log and setup server_context->log_context
 */
static kadm5_ret_t
log_open(kadm5_server_context *server_context, int lock_mode)
{
    int fd = -1;
    int lock_it = 0;
    int lock_nb = 0;
    int oflags = O_RDWR;
    kadm5_ret_t ret;
    kadm5_log_context *log_context = &server_context->log_context;

    if (lock_mode & LOCK_NB) {
        lock_mode &= ~LOCK_NB;
        lock_nb = LOCK_NB;
    }

    if (lock_mode == log_context->lock_mode && log_context->log_fd != -1)
        return 0;

    if (strcmp(log_context->log_file, "/dev/null") == 0) {
        /* log_context->log_fd should be -1 here */
        return 0;
    }

    if (log_context->log_fd != -1) {
        /* Lock or change lock */
        fd = log_context->log_fd;
        if (lseek(fd, 0, SEEK_SET) == -1)
            return errno;
        lock_it = (lock_mode != log_context->lock_mode);
    } else {
        /* Open and lock */
        if (lock_mode != LOCK_UN)
            oflags |= O_CREAT;
        fd = open(log_context->log_file, oflags, 0600);
        if (fd < 0) {
            ret = errno;
            krb5_set_error_message(server_context->context, ret,
                                   "log_open: open %s", log_context->log_file);
            return ret;
        }
        lock_it = (lock_mode != LOCK_UN);
    }
    if (lock_it && flock(fd, lock_mode | lock_nb) < 0) {
	ret = errno;
	krb5_set_error_message(server_context->context, ret,
                               "log_open: flock %s", log_context->log_file);
        if (fd != log_context->log_fd)
            (void) close(fd);
	return ret;
    }

    log_context->log_fd = fd;
    log_context->lock_mode = lock_mode;
    log_context->read_only = (lock_mode != LOCK_EX);

    return 0;
}

/*
 * Open the log and setup server_context->log_context
 */
static kadm5_ret_t
log_init(kadm5_server_context *server_context, int lock_mode)
{
    int fd;
    struct stat st;
    uint32_t vno;
    size_t maxbytes = get_max_log_size(server_context->context);
    kadm5_ret_t ret;
    kadm5_log_context *log_context = &server_context->log_context;

    if (strcmp(log_context->log_file, "/dev/null") == 0) {
        /* log_context->log_fd should be -1 here */
        return 0;
    }

    ret = log_open(server_context, lock_mode);
    if (ret)
        return ret;

    fd = log_context->log_fd;
    if (!log_context->read_only) {
        if (fstat(fd, &st) == -1)
            ret = errno;
        if (ret == 0 && st.st_size == 0) {
            /* Write first entry */
            log_context->version = 0;
            ret = kadm5_log_nop(server_context, kadm_nop_plain);
            if (ret == 0)
                return 0; /* no need to truncate_if_needed(): it's not */
        }
        if (ret == 0) {
            ret = kadm5_log_get_version_fd(server_context, fd,
                                           LOG_VERSION_UBER, &vno, NULL);

            /* Upgrade the log if it was an old-style log */
            if (ret == KADM5_LOG_NEEDS_UPGRADE)
                ret = kadm5_log_truncate(server_context, 0, maxbytes / 4);
        }
        if (ret == 0)
            ret = kadm5_log_recover(server_context, kadm_recover_replay);
    }

    if (ret == 0) {
        ret = kadm5_log_get_version_fd(server_context, fd, LOG_VERSION_LAST,
                                       &log_context->version, NULL);
        if (ret == HEIM_ERR_EOF)
            ret = 0;
    }

    if (ret == 0)
        ret = truncate_if_needed(server_context);

    if (ret != 0)
        (void) kadm5_log_end(server_context);
    return ret;
}

/* Open the log with an exclusive lock */
kadm5_ret_t
kadm5_log_init(kadm5_server_context *server_context)
{
    return log_init(server_context, LOCK_EX);
}

/* Open the log with an exclusive non-blocking lock */
kadm5_ret_t
kadm5_log_init_nb(kadm5_server_context *server_context)
{
    return log_init(server_context, LOCK_EX | LOCK_NB);
}

/* Open the log with no locks */
kadm5_ret_t
kadm5_log_init_nolock(kadm5_server_context *server_context)
{
    return log_init(server_context, LOCK_UN);
}

/* Open the log with a shared lock */
kadm5_ret_t
kadm5_log_init_sharedlock(kadm5_server_context *server_context, int lock_flags)
{
    return log_init(server_context, LOCK_SH | lock_flags);
}

/*
 * Reinitialize the log and open it
 */
kadm5_ret_t
kadm5_log_reinit(kadm5_server_context *server_context, uint32_t vno)
{
    int ret;
    kadm5_log_context *log_context = &server_context->log_context;

    ret = log_open(server_context, LOCK_EX);
    if (ret)
	return ret;
    if (log_context->log_fd != -1) {
        if (ftruncate(log_context->log_fd, 0) < 0) {
            ret = errno;
            return ret;
        }
        if (lseek(log_context->log_fd, 0, SEEK_SET) < 0) {
            ret = errno;
            return ret;
        }
    }

    /* Write uber entry and truncation nop with version `vno` */
    log_context->version = vno;
    return kadm5_log_nop(server_context, kadm_nop_plain);
}

/* Close the server_context->log_context. */
kadm5_ret_t
kadm5_log_end(kadm5_server_context *server_context)
{
    kadm5_log_context *log_context = &server_context->log_context;
    kadm5_ret_t ret = 0;
    int fd = log_context->log_fd;

    if (fd != -1) {
        if (log_context->lock_mode != LOCK_UN) {
            if (flock(fd, LOCK_UN) == -1 && errno == EBADF)
                ret = errno;
        }
        if (ret != EBADF && close(fd) == -1)
            ret = errno;
    }
    log_context->log_fd = -1;
    log_context->lock_mode = LOCK_UN;
    return ret;
}

/*
 * Write the version, timestamp, and op for a new entry.
 *
 * Note that the sp should be a krb5_storage_emem(), not a file.
 *
 * On success the sp's offset will be where the length of the payload
 * should be written.
 */
static kadm5_ret_t
kadm5_log_preamble(kadm5_server_context *context,
		   krb5_storage *sp,
		   enum kadm_ops op,
		   uint32_t vno)
{
    kadm5_log_context *log_context = &context->log_context;
    time_t now = time(NULL);
    kadm5_ret_t ret;

    ret = krb5_store_uint32(sp, vno);
    if (ret)
        return ret;
    ret = krb5_store_uint32(sp, now);
    if (ret)
        return ret;
    log_context->last_time = now;

    if (op < kadm_first || op > kadm_last)
        return ERANGE;
    return krb5_store_uint32(sp, op);
}

/* Writes the version part of the trailer */
static kadm5_ret_t
kadm5_log_postamble(kadm5_log_context *context,
		    krb5_storage *sp,
		    uint32_t vno)
{
    return krb5_store_uint32(sp, vno);
}

/*
 * Signal the ipropd-master about changes to the log.
 */
/*
 * XXX Get rid of the ifdef by having a sockaddr in log_context in both
 * cases.
 *
 * XXX Better yet, just connect to the master's socket that slaves
 * connect to, and then disconnect.  The master should then check the
 * log on every connection accepted.  Then we wouldn't need IPC to
 * signal the master.
 */
void
kadm5_log_signal_master(kadm5_server_context *context)
{
    kadm5_log_context *log_context = &context->log_context;
#ifndef NO_UNIX_SOCKETS
    sendto(log_context->socket_fd,
	   (void *)&log_context->version,
	   sizeof(log_context->version),
	   0,
	   (struct sockaddr *)&log_context->socket_name,
	   sizeof(log_context->socket_name));
#else
    sendto(log_context->socket_fd,
	   (void *)&log_context->version,
	   sizeof(log_context->version),
	   0,
	   log_context->socket_info->ai_addr,
	   log_context->socket_info->ai_addrlen);
#endif
}

/*
 * Write sp's contents (which must be a fully formed record, complete
 * with header, payload, and trailer) to the log and fsync the log.
 *
 * Does not free sp.
 */

static kadm5_ret_t
kadm5_log_flush(kadm5_server_context *context, krb5_storage *sp)
{
    kadm5_log_context *log_context = &context->log_context;
    kadm5_ret_t ret;
    krb5_data data;
    size_t len;
    krb5_ssize_t bytes;
    uint32_t new_ver, prev_ver;
    off_t off, end;

    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return 0;

    if (log_context->read_only)
        return EROFS;

    if (krb5_storage_seek(sp, 0, SEEK_SET) == -1)
        return errno;

    ret = get_header(sp, LOG_DOPEEK, &new_ver, NULL, NULL, NULL);
    if (ret)
        return ret;

    ret = krb5_storage_to_data(sp, &data);
    if (ret)
        return ret;

    /* Abandon the emem storage reference */
    sp = krb5_storage_from_fd(log_context->log_fd);
    if (sp == NULL) {
        krb5_data_free(&data);
        return ENOMEM;
    }

    /* Check that we are at the end of the log and fail if not */
    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off == -1) {
        krb5_data_free(&data);
        krb5_storage_free(sp);
        return errno;
    }
    end = krb5_storage_seek(sp, 0, SEEK_END);
    if (end == -1) {
        krb5_data_free(&data);
        krb5_storage_free(sp);
        return errno;
    }
    if (end != off) {
        krb5_data_free(&data);
        krb5_storage_free(sp);
        return KADM5_LOG_CORRUPT;
    }

    /* Enforce monotonically incremented versioning of records */
    if (seek_prev(sp, &prev_ver, NULL) == -1 ||
        krb5_storage_seek(sp, end, SEEK_SET) == -1) {
        ret = errno;
        krb5_data_free(&data);
        krb5_storage_free(sp);
        return ret;
    }

    if (prev_ver != 0 && prev_ver != log_context->version)
        return EINVAL; /* Internal error, really; just a consistency check */

    if (prev_ver != 0 && new_ver != prev_ver + 1) {
        krb5_warnx(context->context, "refusing to write a log record "
                   "with non-monotonic version (new: %u, old: %u)",
                   new_ver, prev_ver);
        return KADM5_LOG_CORRUPT;
    }

    len = data.length;
    bytes = krb5_storage_write(sp, data.data, len);
    krb5_data_free(&data);
    if (bytes < 0) {
        krb5_storage_free(sp);
	return errno;
    }
    if (bytes != (krb5_ssize_t)len) {
        krb5_storage_free(sp);
        return EIO;
    }

    ret = krb5_storage_fsync(sp);
    krb5_storage_free(sp);
    if (ret)
        return ret;

    /* Retain the nominal database version when flushing the uber record */
    if (new_ver != 0)
        log_context->version = new_ver;
    return 0;
}

/*
 * Add a `create' operation to the log and perform the create against the HDB.
 */
kadm5_ret_t
kadm5_log_create(kadm5_server_context *context, hdb_entry *entry)
{
    krb5_storage *sp;
    kadm5_ret_t ret;
    krb5_data value;
    hdb_entry_ex ent;
    kadm5_log_context *log_context = &context->log_context;

    memset(&ent, 0, sizeof(ent));
    ent.ctx = 0;
    ent.free_entry = 0;
    ent.entry = *entry;

    /*
     * If we're not logging then we can't recover-to-perform, so just
     * perform.
     */
    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return context->db->hdb_store(context->context, context->db, 0, &ent);

    /*
     * Test for any conflicting entries before writing the log.  If we commit
     * to the log we'll end-up rolling forward on recovery, but that would be
     * wrong if the initial create is rejected.
     */
    ret = context->db->hdb_store(context->context, context->db,
                                 HDB_F_PRECHECK, &ent);
    if (ret == 0)
        ret = hdb_entry2value(context->context, entry, &value);
    if (ret)
        return ret;
    sp = krb5_storage_emem();
    if (sp == NULL)
        ret = ENOMEM;
    if (ret == 0)
        ret = kadm5_log_preamble(context, sp, kadm_create,
                                 log_context->version + 1);
    if (ret == 0)
        ret = krb5_store_uint32(sp, value.length);
    if (ret == 0) {
        if (krb5_storage_write(sp, value.data, value.length) !=
            (krb5_ssize_t)value.length)
            ret = errno;
    }
    if (ret == 0)
        ret = krb5_store_uint32(sp, value.length);
    if (ret == 0)
        ret = kadm5_log_postamble(log_context, sp,
                                  log_context->version + 1);
    if (ret == 0)
        ret = kadm5_log_flush(context, sp);
    krb5_storage_free(sp);
    krb5_data_free(&value);
    if (ret == 0)
        ret = kadm5_log_recover(context, kadm_recover_commit);
    return ret;
}

/*
 * Read the data of a create log record from `sp' and change the
 * database.
 */
static kadm5_ret_t
kadm5_log_replay_create(kadm5_server_context *context,
		        uint32_t ver,
		        uint32_t len,
		        krb5_storage *sp)
{
    krb5_error_code ret;
    krb5_data data;
    hdb_entry_ex ent;

    memset(&ent, 0, sizeof(ent));

    ret = krb5_data_alloc(&data, len);
    if (ret) {
	krb5_set_error_message(context->context, ret, "out of memory");
	return ret;
    }
    krb5_storage_read(sp, data.data, len);
    ret = hdb_value2entry(context->context, &data, &ent.entry);
    krb5_data_free(&data);
    if (ret) {
	krb5_set_error_message(context->context, ret,
			       "Unmarshaling hdb entry in log failed, "
                               "version: %ld", (long)ver);
	return ret;
    }
    ret = context->db->hdb_store(context->context, context->db, 0, &ent);
    hdb_free_entry(context->context, &ent);
    return ret;
}

/*
 * Add a `delete' operation to the log.
 */
kadm5_ret_t
kadm5_log_delete(kadm5_server_context *context,
		 krb5_principal princ)
{
    kadm5_ret_t ret;
    kadm5_log_context *log_context = &context->log_context;
    krb5_storage *sp;
    uint32_t len = 0;   /* So dumb compilers don't warn */
    off_t end_off = 0;  /* Ditto; this allows de-indentation by two levels */
    off_t off;

    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return context->db->hdb_remove(context->context, context->db, 0,
                                       princ);
    ret = context->db->hdb_remove(context->context, context->db,
                                  HDB_F_PRECHECK, princ);
    if (ret)
        return ret;
    sp = krb5_storage_emem();
    if (sp == NULL)
        ret = ENOMEM;
    if (ret == 0)
        ret = kadm5_log_preamble(context, sp, kadm_delete,
                                 log_context->version + 1);
    if (ret) {
        krb5_storage_free(sp);
        return ret;
    }

    /*
     * Write a 0 length which we overwrite once we know the length of
     * the principal name payload.
     */
    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off == -1)
        ret = errno;
    if (ret == 0)
        ret = krb5_store_uint32(sp, 0);
    if (ret == 0)
        ret = krb5_store_principal(sp, princ);
    if (ret == 0) {
        end_off = krb5_storage_seek(sp, 0, SEEK_CUR);
        if (end_off == -1)
            ret = errno;
        else if (end_off < off)
            ret = KADM5_LOG_CORRUPT;
    }
    if (ret == 0) {
        /* We wrote sizeof(uint32_t) + payload length bytes */
        len = (uint32_t)(end_off - off);
        if (end_off - off != len || len < sizeof(len))
            ret = KADM5_LOG_CORRUPT;
        else
            len -= sizeof(len);
    }
    if (ret == 0 && krb5_storage_seek(sp, off, SEEK_SET) == -1)
        ret = errno;
    if (ret == 0)
        ret = krb5_store_uint32(sp, len);
    if (ret == 0 && krb5_storage_seek(sp, end_off, SEEK_SET) == -1)
        ret = errno;
    if (ret == 0)
        ret = krb5_store_uint32(sp, len);
    if (ret == 0)
        ret = kadm5_log_postamble(log_context, sp,
                                  log_context->version + 1);
    if (ret == 0)
        ret = kadm5_log_flush(context, sp);
    if (ret == 0)
        ret = kadm5_log_recover(context, kadm_recover_commit);
    krb5_storage_free(sp);
    return ret;
}

/*
 * Read a `delete' log operation from `sp' and apply it.
 */
static kadm5_ret_t
kadm5_log_replay_delete(kadm5_server_context *context,
		        uint32_t ver, uint32_t len, krb5_storage *sp)
{
    krb5_error_code ret;
    krb5_principal principal;

    ret = krb5_ret_principal(sp, &principal);
    if (ret) {
	krb5_set_error_message(context->context,  ret, "Failed to read deleted "
			       "principal from log version: %ld",  (long)ver);
	return ret;
    }

    ret = context->db->hdb_remove(context->context, context->db, 0, principal);
    krb5_free_principal(context->context, principal);
    return ret;
}

static kadm5_ret_t kadm5_log_replay_rename(kadm5_server_context *,
                                           uint32_t, uint32_t,
                                           krb5_storage *);

/*
 * Add a `rename' operation to the log.
 */
kadm5_ret_t
kadm5_log_rename(kadm5_server_context *context,
		 krb5_principal source,
		 hdb_entry *entry)
{
    krb5_storage *sp;
    kadm5_ret_t ret;
    uint32_t len = 0;   /* So dumb compilers don't warn */
    off_t end_off = 0;  /* Ditto; this allows de-indentation by two levels */
    off_t off;
    krb5_data value;
    hdb_entry_ex ent;
    kadm5_log_context *log_context = &context->log_context;

    memset(&ent, 0, sizeof(ent));
    ent.ctx = 0;
    ent.free_entry = 0;
    ent.entry = *entry;

    if (strcmp(log_context->log_file, "/dev/null") == 0) {
        ret = context->db->hdb_store(context->context, context->db, 0, &ent);
        if (ret == 0)
            return context->db->hdb_remove(context->context, context->db, 0,
                                           source);
        return ret;
    }

    /*
     * Pre-check that the transaction will succeed.
     *
     * Note that rename doesn't work to swap a principal's canonical
     * name with one of its aliases.  To make that work would require
     * adding an hdb_rename() method for renaming principals (there's an
     * hdb_rename() method already, but for renaming the HDB), which is
     * ETOOMUCHWORK for the time being.
     */
    ret = context->db->hdb_store(context->context, context->db,
                                 HDB_F_PRECHECK, &ent);
    if (ret == 0)
        ret = context->db->hdb_remove(context->context, context->db,
                                       HDB_F_PRECHECK, source);
    if (ret)
        return ret;

    sp = krb5_storage_emem();
    krb5_data_zero(&value);
    if (sp == NULL)
	ret = ENOMEM;
    if (ret == 0)
        ret = kadm5_log_preamble(context, sp, kadm_rename,
                                 log_context->version + 1);
    if (ret == 0)
        ret = hdb_entry2value(context->context, entry, &value);
    if (ret) {
        krb5_data_free(&value);
        krb5_storage_free(sp);
        return ret;
    }

    /*
     * Write a zero length which we'll overwrite once we know the length of the
     * payload.
     */
    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (off == -1)
        ret = errno;
    if (ret == 0)
        ret = krb5_store_uint32(sp, 0);
    if (ret == 0)
        ret = krb5_store_principal(sp, source);
    if (ret == 0) {
        errno = 0;
        if (krb5_storage_write(sp, value.data, value.length) !=
            (krb5_ssize_t)value.length)
            ret = errno ? errno : EIO;
    }
    if (ret == 0) {
        end_off = krb5_storage_seek(sp, 0, SEEK_CUR);
        if (end_off == -1)
            ret = errno;
        else if (end_off < off)
            ret = KADM5_LOG_CORRUPT;
    }
    if (ret == 0) {
        /* We wrote sizeof(uint32_t) + payload length bytes */
        len = (uint32_t)(end_off - off);
        if (end_off - off != len || len < sizeof(len))
            ret = KADM5_LOG_CORRUPT;
        else
            len -= sizeof(len);
        if (ret == 0 && krb5_storage_seek(sp, off, SEEK_SET) == -1)
            ret = errno;
        if (ret == 0)
            ret = krb5_store_uint32(sp, len);
        if (ret == 0 && krb5_storage_seek(sp, end_off, SEEK_SET) == -1)
            ret = errno;
        if (ret == 0)
            ret = krb5_store_uint32(sp, len);
        if (ret == 0)
            ret = kadm5_log_postamble(log_context, sp,
                                      log_context->version + 1);
        if (ret == 0)
            ret = kadm5_log_flush(context, sp);
        if (ret == 0)
            ret = kadm5_log_recover(context, kadm_recover_commit);
    }
    krb5_data_free(&value);
    krb5_storage_free(sp);
    return ret;
}

/*
 * Read a `rename' log operation from `sp' and apply it.
 */

static kadm5_ret_t
kadm5_log_replay_rename(kadm5_server_context *context,
		        uint32_t ver,
		        uint32_t len,
		        krb5_storage *sp)
{
    krb5_error_code ret;
    krb5_principal source;
    hdb_entry_ex target_ent;
    krb5_data value;
    off_t off;
    size_t princ_len, data_len;

    memset(&target_ent, 0, sizeof(target_ent));

    off = krb5_storage_seek(sp, 0, SEEK_CUR);
    ret = krb5_ret_principal(sp, &source);
    if (ret) {
	krb5_set_error_message(context->context, ret, "Failed to read renamed "
			       "principal in log, version: %ld", (long)ver);
	return ret;
    }
    princ_len = krb5_storage_seek(sp, 0, SEEK_CUR) - off;
    data_len = len - princ_len;
    ret = krb5_data_alloc(&value, data_len);
    if (ret) {
	krb5_free_principal (context->context, source);
	return ret;
    }
    krb5_storage_read(sp, value.data, data_len);
    ret = hdb_value2entry(context->context, &value, &target_ent.entry);
    krb5_data_free(&value);
    if (ret) {
	krb5_free_principal(context->context, source);
	return ret;
    }
    ret = context->db->hdb_store(context->context, context->db,
				 0, &target_ent);
    hdb_free_entry(context->context, &target_ent);
    if (ret) {
	krb5_free_principal(context->context, source);
	return ret;
    }
    ret = context->db->hdb_remove(context->context, context->db, 0, source);
    krb5_free_principal(context->context, source);

    return ret;
}

/*
 * Add a `modify' operation to the log.
 */
kadm5_ret_t
kadm5_log_modify(kadm5_server_context *context,
		 hdb_entry *entry,
		 uint32_t mask)
{
    krb5_storage *sp;
    kadm5_ret_t ret;
    krb5_data value;
    uint32_t len;
    hdb_entry_ex ent;
    kadm5_log_context *log_context = &context->log_context;

    memset(&ent, 0, sizeof(ent));
    ent.ctx = 0;
    ent.free_entry = 0;
    ent.entry = *entry;

    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return context->db->hdb_store(context->context, context->db,
                                      HDB_F_REPLACE, &ent);

    ret = context->db->hdb_store(context->context, context->db,
                                 HDB_F_PRECHECK | HDB_F_REPLACE, &ent);
    if (ret)
        return ret;

    sp = krb5_storage_emem();
    krb5_data_zero(&value);
    if (sp == NULL)
        ret = ENOMEM;
    if (ret == 0)
        ret = hdb_entry2value(context->context, entry, &value);
    if (ret) {
        krb5_data_free(&value);
        krb5_storage_free(sp);
	return ret;
    }

    len = value.length + sizeof(len);
    if (value.length > len || len > INT32_MAX)
        ret = E2BIG;
    if (ret == 0)
        ret = kadm5_log_preamble(context, sp, kadm_modify,
                                 log_context->version + 1);
    if (ret == 0)
        ret = krb5_store_uint32(sp, len);
    if (ret == 0)
        ret = krb5_store_uint32(sp, mask);
    if (ret == 0) {
        if (krb5_storage_write(sp, value.data, value.length) !=
            (krb5_ssize_t)value.length)
            ret = errno;
    }
    if (ret == 0)
        ret = krb5_store_uint32(sp, len);
    if (ret == 0)
        ret = kadm5_log_postamble(log_context, sp,
                                  log_context->version + 1);
    if (ret == 0)
        ret = kadm5_log_flush(context, sp);
    if (ret == 0)
        ret = kadm5_log_recover(context, kadm_recover_commit);
    krb5_data_free(&value);
    krb5_storage_free(sp);
    return ret;
}

/*
 * Read a `modify' log operation from `sp' and apply it.
 */
static kadm5_ret_t
kadm5_log_replay_modify(kadm5_server_context *context,
		        uint32_t ver,
		        uint32_t len,
		        krb5_storage *sp)
{
    krb5_error_code ret;
    uint32_t mask;
    krb5_data value;
    hdb_entry_ex ent, log_ent;

    memset(&log_ent, 0, sizeof(log_ent));

    ret = krb5_ret_uint32(sp, &mask);
    if (ret)
        return ret;
    len -= 4;
    ret = krb5_data_alloc (&value, len);
    if (ret) {
	krb5_set_error_message(context->context, ret, "out of memory");
	return ret;
    }
    errno = 0;
    if (krb5_storage_read (sp, value.data, len) != (krb5_ssize_t)len) {
        ret = errno ? errno : EIO;
        return ret;
    }
    ret = hdb_value2entry (context->context, &value, &log_ent.entry);
    krb5_data_free(&value);
    if (ret)
	return ret;

    memset(&ent, 0, sizeof(ent));
    ret = context->db->hdb_fetch_kvno(context->context, context->db,
				      log_ent.entry.principal,
				      HDB_F_DECRYPT|HDB_F_ALL_KVNOS|
				      HDB_F_GET_ANY|HDB_F_ADMIN_DATA, 0, &ent);
    if (ret)
	goto out;
    if (mask & KADM5_PRINC_EXPIRE_TIME) {
	if (log_ent.entry.valid_end == NULL) {
	    ent.entry.valid_end = NULL;
	} else {
	    if (ent.entry.valid_end == NULL) {
		ent.entry.valid_end = malloc(sizeof(*ent.entry.valid_end));
		if (ent.entry.valid_end == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_message(context->context, ret, "out of memory");
		    goto out;
		}
	    }
	    *ent.entry.valid_end = *log_ent.entry.valid_end;
	}
    }
    if (mask & KADM5_PW_EXPIRATION) {
	if (log_ent.entry.pw_end == NULL) {
	    ent.entry.pw_end = NULL;
	} else {
	    if (ent.entry.pw_end == NULL) {
		ent.entry.pw_end = malloc(sizeof(*ent.entry.pw_end));
		if (ent.entry.pw_end == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_message(context->context, ret, "out of memory");
		    goto out;
		}
	    }
	    *ent.entry.pw_end = *log_ent.entry.pw_end;
	}
    }
    if (mask & KADM5_LAST_PWD_CHANGE) {
        krb5_warnx (context->context,
                    "Unimplemented mask KADM5_LAST_PWD_CHANGE");
    }
    if (mask & KADM5_ATTRIBUTES) {
	ent.entry.flags = log_ent.entry.flags;
    }
    if (mask & KADM5_MAX_LIFE) {
	if (log_ent.entry.max_life == NULL) {
	    ent.entry.max_life = NULL;
	} else {
	    if (ent.entry.max_life == NULL) {
		ent.entry.max_life = malloc (sizeof(*ent.entry.max_life));
		if (ent.entry.max_life == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_message(context->context, ret, "out of memory");
		    goto out;
		}
	    }
	    *ent.entry.max_life = *log_ent.entry.max_life;
	}
    }
    if ((mask & KADM5_MOD_TIME) && (mask & KADM5_MOD_NAME)) {
	if (ent.entry.modified_by == NULL) {
	    ent.entry.modified_by = malloc(sizeof(*ent.entry.modified_by));
	    if (ent.entry.modified_by == NULL) {
		ret = ENOMEM;
		krb5_set_error_message(context->context, ret, "out of memory");
		goto out;
	    }
	} else
	    free_Event(ent.entry.modified_by);
	ret = copy_Event(log_ent.entry.modified_by, ent.entry.modified_by);
	if (ret) {
	    krb5_set_error_message(context->context, ret, "out of memory");
	    goto out;
	}
    }
    if (mask & KADM5_KVNO) {
	ent.entry.kvno = log_ent.entry.kvno;
    }
    if (mask & KADM5_MKVNO) {
        krb5_warnx(context->context, "Unimplemented mask KADM5_KVNO");
    }
    if (mask & KADM5_AUX_ATTRIBUTES) {
        krb5_warnx(context->context,
                   "Unimplemented mask KADM5_AUX_ATTRIBUTES");
    }
    if (mask & KADM5_POLICY_CLR) {
        krb5_warnx(context->context, "Unimplemented mask KADM5_POLICY_CLR");
    }
    if (mask & KADM5_MAX_RLIFE) {
	if (log_ent.entry.max_renew == NULL) {
	    ent.entry.max_renew = NULL;
	} else {
	    if (ent.entry.max_renew == NULL) {
		ent.entry.max_renew = malloc (sizeof(*ent.entry.max_renew));
		if (ent.entry.max_renew == NULL) {
		    ret = ENOMEM;
		    krb5_set_error_message(context->context, ret, "out of memory");
		    goto out;
		}
	    }
	    *ent.entry.max_renew = *log_ent.entry.max_renew;
	}
    }
    if (mask & KADM5_LAST_SUCCESS) {
        krb5_warnx(context->context, "Unimplemented mask KADM5_LAST_SUCCESS");
    }
    if (mask & KADM5_LAST_FAILED) {
        krb5_warnx(context->context, "Unimplemented mask KADM5_LAST_FAILED");
    }
    if (mask & KADM5_FAIL_AUTH_COUNT) {
        krb5_warnx(context->context,
                   "Unimplemented mask KADM5_FAIL_AUTH_COUNT");
    }
    if (mask & KADM5_KEY_DATA) {
	size_t num;
	size_t i;

	/*
	 * We don't need to do anything about key history here because
	 * the log entry contains a complete entry, including hdb
	 * extensions.  We do need to make sure that KADM5_TL_DATA is in
	 * the mask though, since that's what it takes to update the
	 * extensions (see below).
	 */
	mask |= KADM5_TL_DATA;

	for (i = 0; i < ent.entry.keys.len; ++i)
	    free_Key(&ent.entry.keys.val[i]);
	free (ent.entry.keys.val);

	num = log_ent.entry.keys.len;

	ent.entry.keys.len = num;
	ent.entry.keys.val = malloc(len * sizeof(*ent.entry.keys.val));
	if (ent.entry.keys.val == NULL) {
	    krb5_set_error_message(context->context, ENOMEM, "out of memory");
            ret = ENOMEM;
	    goto out;
	}
	for (i = 0; i < ent.entry.keys.len; ++i) {
	    ret = copy_Key(&log_ent.entry.keys.val[i],
			   &ent.entry.keys.val[i]);
	    if (ret) {
		krb5_set_error_message(context->context, ret, "out of memory");
		goto out;
	    }
	}
    }
    if ((mask & KADM5_TL_DATA) && log_ent.entry.extensions) {
	HDB_extensions *es = ent.entry.extensions;

	ent.entry.extensions = calloc(1, sizeof(*ent.entry.extensions));
	if (ent.entry.extensions == NULL)
	    goto out;

	ret = copy_HDB_extensions(log_ent.entry.extensions,
				  ent.entry.extensions);
	if (ret) {
	    krb5_set_error_message(context->context, ret, "out of memory");
	    free(ent.entry.extensions);
	    ent.entry.extensions = es;
	    goto out;
	}
	if (es) {
	    free_HDB_extensions(es);
	    free(es);
	}
    }
    ret = context->db->hdb_store(context->context, context->db,
				 HDB_F_REPLACE, &ent);
 out:
    hdb_free_entry(context->context, &ent);
    hdb_free_entry(context->context, &log_ent);
    return ret;
}

/*
 * Update the first entry (which should be a `nop'), the "uber-entry".
 */
static kadm5_ret_t
log_update_uber(kadm5_server_context *context, off_t off)
{
    kadm5_log_context *log_context = &context->log_context;
    kadm5_ret_t ret = 0;
    krb5_storage *sp, *mem_sp;
    krb5_data data;
    uint32_t op, len;
    ssize_t bytes;

    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return 0;

    if (log_context->read_only)
        return EROFS;

    krb5_data_zero(&data);

    mem_sp = krb5_storage_emem();
    if (mem_sp == NULL)
        return ENOMEM;

    sp = krb5_storage_from_fd(log_context->log_fd);
    if (sp == NULL) {
        krb5_storage_free(mem_sp);
        return ENOMEM;
    }

    /* Skip first entry's version and timestamp */
    if (krb5_storage_seek(sp, 8, SEEK_SET) == -1) {
        ret = errno;
        goto out;
    }

    /* If the first entry is not a nop, there's nothing we can do here */
    ret = krb5_ret_uint32(sp, &op);
    if (ret || op != kadm_nop)
        goto out;

    /* If the first entry is not a 16-byte nop, ditto */
    ret = krb5_ret_uint32(sp, &len);
    if (ret || len != LOG_UBER_LEN)
        goto out;

    /*
     * Try to make the writes here as close to atomic as possible: a
     * single write() call.
     */
    ret = krb5_store_uint64(mem_sp, off);
    if (ret)
        goto out;
    ret = krb5_store_uint32(mem_sp, log_context->last_time);
    if (ret)
        goto out;
    ret = krb5_store_uint32(mem_sp, log_context->version);
    if (ret)
        goto out;

    krb5_storage_to_data(mem_sp, &data);
    bytes = krb5_storage_write(sp, data.data, data.length);
    if (bytes < 0)
        ret = errno;
    else if (bytes != data.length)
        ret = EIO;

    /*
     * We don't fsync() this write because we can recover if the write
     * doesn't complete, though for now we don't have code for properly
     * dealing with the offset not getting written completely.
     *
     * We should probably have two copies of the offset so we can use
     * one copy to verify the other, and when they don't match we could
     * traverse the whole log forwards, replaying just the last entry.
     */

out:
    if (ret == 0)
        kadm5_log_signal_master(context);
    krb5_data_free(&data);
    krb5_storage_free(sp);
    krb5_storage_free(mem_sp);
    if (lseek(log_context->log_fd, off, SEEK_SET) == -1)
        ret = ret ? ret : errno;

    return ret;
}

/*
 * Add a `nop' operation to the log. Does not close the log.
 */
kadm5_ret_t
kadm5_log_nop(kadm5_server_context *context, enum kadm_nop_type nop_type)
{
    krb5_storage *sp;
    kadm5_ret_t ret;
    kadm5_log_context *log_context = &context->log_context;
    off_t off;
    uint32_t vno = log_context->version;

    if (strcmp(log_context->log_file, "/dev/null") == 0)
        return 0;

    off = lseek(log_context->log_fd, 0, SEEK_CUR);
    if (off == -1)
        return errno;

    sp = krb5_storage_emem();
    ret = kadm5_log_preamble(context, sp, kadm_nop, off == 0 ? 0 : vno + 1);
    if (ret)
        goto out;

    if (off == 0) {
        /*
         * First entry (uber-entry) gets room for offset of next new
         * entry and time and version of last entry.
         */
        ret = krb5_store_uint32(sp, LOG_UBER_LEN);
        /* These get overwritten with the same values below */
        if (ret == 0)
            ret = krb5_store_uint64(sp, LOG_UBER_SZ);
        if (ret == 0)
            ret = krb5_store_uint32(sp, log_context->last_time);
        if (ret == 0)
            ret = krb5_store_uint32(sp, vno);
        if (ret == 0)
            ret = krb5_store_uint32(sp, LOG_UBER_LEN);
    } else if (nop_type == kadm_nop_plain) {
        ret = krb5_store_uint32(sp, 0);
        if (ret == 0)
            ret = krb5_store_uint32(sp, 0);
    } else {
        ret = krb5_store_uint32(sp, sizeof(uint32_t));
        if (ret == 0)
            ret = krb5_store_uint32(sp, nop_type);
        if (ret == 0)
            ret = krb5_store_uint32(sp, sizeof(uint32_t));
    }

    if (ret == 0)
        ret = kadm5_log_postamble(log_context, sp, off == 0 ? 0 : vno + 1);
    if (ret == 0)
        ret = kadm5_log_flush(context, sp);

    if (ret == 0 && off == 0 && nop_type != kadm_nop_plain)
        ret = kadm5_log_nop(context, nop_type);

    if (ret == 0 && off != 0)
        ret = kadm5_log_recover(context, kadm_recover_commit);

out:
    krb5_storage_free(sp);
    return ret;
}

/*
 * Read a `nop' log operation from `sp' and "apply" it (there's nothing
 * to do).
 *
 * FIXME Actually, if the nop payload is 4 bytes and contains an enum
 * kadm_nop_type value of kadm_nop_trunc then we should truncate the
 * log, and if it contains a kadm_nop_close then we should rename a new
 * log into place.  However, this is not implemented yet.
 */
static kadm5_ret_t
kadm5_log_replay_nop(kadm5_server_context *context,
		     uint32_t ver,
		     uint32_t len,
		     krb5_storage *sp)
{
    return 0;
}

struct replay_cb_data {
    size_t count;
    uint32_t ver;
    enum kadm_recover_mode mode;
};


/*
 * Recover or perform the initial commit of an unconfirmed log entry
 */
static kadm5_ret_t
recover_replay(kadm5_server_context *context,
               uint32_t ver, time_t timestamp, enum kadm_ops op,
               uint32_t len, krb5_storage *sp, void *ctx)
{
    struct replay_cb_data *data = ctx;
    kadm5_ret_t ret;
    off_t off;

    /* On initial commit there must be just one pending unconfirmed entry */
    if (data->count > 0 && data->mode == kadm_recover_commit)
        return KADM5_LOG_CORRUPT;

    /* We're at the start of the payload; compute end of entry offset */
    off = krb5_storage_seek(sp, 0, SEEK_CUR) + len + LOG_TRAILER_SZ;

    /* We cannot perform log recovery on LDAP and such backends */
    if (data->mode == kadm_recover_replay &&
        (context->db->hdb_capability_flags & HDB_CAP_F_SHARED_DIRECTORY))
        ret = 0;
    else
        ret = kadm5_log_replay(context, op, ver, len, sp);
    switch (ret) {
    case HDB_ERR_NOENTRY:
    case HDB_ERR_EXISTS:
        if (data->mode != kadm_recover_replay)
            return ret;
    case 0:
        break;
    case KADM5_LOG_CORRUPT:
        return -1;
    default:
        krb5_warn(context->context, ret, "unexpected error while replaying");
        return -1;
    }
    data->count++;
    data->ver = ver;

    /*
     * With replay we may be making multiple HDB changes.  We must sync the
     * confirmation of each one before moving on to the next.  Otherwise, we
     * might attempt to replay multiple already applied updates, and this may
     * introduce unintended intermediate states or fail to yield the same final
     * result.
     */
    kadm5_log_set_version(context, ver);
    ret = log_update_uber(context, off);
    if (ret == 0 && data->mode != kadm_recover_commit)
        ret = krb5_storage_fsync(sp);
    return ret;
}


kadm5_ret_t
kadm5_log_recover(kadm5_server_context *context, enum kadm_recover_mode mode)
{
    kadm5_ret_t ret;
    krb5_storage *sp;
    struct replay_cb_data replay_data;

    replay_data.count = 0;
    replay_data.ver = 0;
    replay_data.mode = mode;

    sp = kadm5_log_goto_end(context, context->log_context.log_fd);
    if (sp == NULL)
        return errno ? errno : EIO;

    ret = kadm5_log_foreach(context, kadm_forward | kadm_unconfirmed,
                            NULL, recover_replay, &replay_data);
    if (ret == 0 && mode == kadm_recover_commit && replay_data.count != 1)
        ret = KADM5_LOG_CORRUPT;
    krb5_storage_free(sp);
    return ret;
}

/*
 * Call `func' for each log record in the log in `context'.
 *
 * `func' is optional.
 *
 * If `func' returns -1 then log traversal terminates and this returns 0.
 * Otherwise `func''s return is returned if there are no other errors.
 */
kadm5_ret_t
kadm5_log_foreach(kadm5_server_context *context,
                  enum kadm_iter_opts iter_opts,
                  off_t *off_lastp,
		  kadm5_ret_t (*func)(kadm5_server_context *server_context,
                                      uint32_t ver, time_t timestamp,
                                      enum kadm_ops op, uint32_t len,
                                      krb5_storage *sp, void *ctx),
		  void *ctx)
{
    kadm5_ret_t ret = 0;
    int fd = context->log_context.log_fd;
    krb5_storage *sp;
    off_t off_last;
    off_t this_entry = 0;
    off_t log_end = 0;

    if (strcmp(context->log_context.log_file, "/dev/null") == 0)
        return 0;

    if (off_lastp == NULL)
        off_lastp = &off_last;
    *off_lastp = -1;

    if (((iter_opts & kadm_forward) && (iter_opts & kadm_backward)) ||
        (!(iter_opts & kadm_confirmed) && !(iter_opts & kadm_unconfirmed)))
        return EINVAL;

    if ((iter_opts & kadm_forward) && (iter_opts & kadm_confirmed) &&
        (iter_opts & kadm_unconfirmed)) {
        /*
         * We want to traverse all log entries, confirmed or not, from
         * the start, then there's no need to kadm5_log_goto_end()
         * -- no reason to try to find the end.
         */
        sp = krb5_storage_from_fd(fd);
        if (sp == NULL)
            return errno;

        log_end = krb5_storage_seek(sp, 0, SEEK_END);
        if (log_end == -1 ||
            krb5_storage_seek(sp, 0, SEEK_SET) == -1) {
            ret = errno;
            krb5_storage_free(sp);
            return ret;
        }
    } else {
        /* Get the end of the log based on the uber entry */
        sp = kadm5_log_goto_end(context, fd);
        if (sp == NULL)
            return errno;
        log_end = krb5_storage_seek(sp, 0, SEEK_CUR);
    }

    *off_lastp = log_end;

    if ((iter_opts & kadm_forward) && (iter_opts & kadm_confirmed)) {
        /* Start at the beginning */
        if (krb5_storage_seek(sp, 0, SEEK_SET) == -1) {
            ret = errno;
            krb5_storage_free(sp);
            return ret;
        }
    } else if ((iter_opts & kadm_backward) && (iter_opts & kadm_unconfirmed)) {
        /*
         * We're at the confirmed end but need to be at the unconfirmed
         * end.  Skip forward to the real end, re-entering to do it.
         */
        ret = kadm5_log_foreach(context, kadm_forward | kadm_unconfirmed,
                                &log_end, NULL, NULL);
        if (ret)
            return ret;
        if (krb5_storage_seek(sp, log_end, SEEK_SET) == -1) {
            ret = errno;
            krb5_storage_free(sp);
            return ret;
        }
    }

    for (;;) {
	uint32_t ver, ver2, len, len2;
	uint32_t tstamp;
        time_t timestamp;
        enum kadm_ops op;

        if ((iter_opts & kadm_backward)) {
            off_t o;

            o = krb5_storage_seek(sp, 0, SEEK_CUR);
            if (o == 0 ||
                ((iter_opts & kadm_unconfirmed) && o <= *off_lastp))
                break;
            ret = kadm5_log_previous(context->context, sp, &ver,
                                     &timestamp, &op, &len);
            if (ret)
                break;

            /* Offset is now at payload of current entry */

            o = krb5_storage_seek(sp, 0, SEEK_CUR);
            if (o == -1) {
                ret = errno;
                break;
            }
            this_entry = o - LOG_HEADER_SZ;
            if (this_entry < 0) {
                ret = KADM5_LOG_CORRUPT;
                break;
            }
        } else {
            /* Offset is now at start of current entry, read header */
            this_entry = krb5_storage_seek(sp, 0, SEEK_CUR);
            if (!(iter_opts & kadm_unconfirmed) && this_entry == log_end)
                break;
            ret = get_header(sp, LOG_NOPEEK, &ver, &tstamp, &op, &len);
            if (ret == HEIM_ERR_EOF) {
                ret = 0;
                break;
            }
            timestamp = tstamp;
            if (ret)
                break;
            /* Offset is now at payload of current entry */
        }

        /* Validate trailer before calling the callback */
        if (krb5_storage_seek(sp, len, SEEK_CUR) == -1) {
            ret = errno;
            break;
        }

	ret = krb5_ret_uint32(sp, &len2);
        if (ret)
            break;
	ret = krb5_ret_uint32(sp, &ver2);
        if (ret)
            break;
	if (len != len2 || ver != ver2) {
            ret = KADM5_LOG_CORRUPT;
	    break;
        }

        /* Rewind to start of payload and call callback if we have one */
        if (krb5_storage_seek(sp, this_entry + LOG_HEADER_SZ,
                              SEEK_SET) == -1) {
            ret = errno;
            break;
        }

        if (func != NULL) {
            ret = (*func)(context, ver, timestamp, op, len, sp, ctx);
            if (ret) {
                /* Callback signals desire to stop by returning -1 */
                if (ret == -1)
                    ret = 0;
                break;
            }
        }
        if ((iter_opts & kadm_forward)) {
            off_t o;

            o = krb5_storage_seek(sp, this_entry+LOG_WRAPPER_SZ+len, SEEK_SET);
            if (o == -1) {
                ret = errno;
                break;
            }
            if (o > log_end)
                *off_lastp = o;
        } else if ((iter_opts & kadm_backward)) {
            /*
             * Rewind to the start of this entry so kadm5_log_previous()
             * can find the previous one.
             */
            if (krb5_storage_seek(sp, this_entry, SEEK_SET) == -1) {
                ret = errno;
                break;
            }
        }
    }
    if ((ret == HEIM_ERR_EOF || ret == KADM5_LOG_CORRUPT) &&
        (iter_opts & kadm_forward) &&
        context->log_context.lock_mode == LOCK_EX) {
        /*
         * Truncate partially written last log entry so we can write
         * again.
         */
        ret = krb5_storage_truncate(sp, this_entry);
        if (ret == 0 &&
            krb5_storage_seek(sp, this_entry, SEEK_SET) == -1)
            ret = errno;
        krb5_warnx(context->context, "Truncating log at partial or "
                   "corrupt %s entry",
                   this_entry > log_end ? "unconfirmed" : "confirmed");
    }
    krb5_storage_free(sp);
    return ret;
}

/*
 * Go to the second record, which, if we have an uber record, will be
 * the first record.
 */
static krb5_storage *
log_goto_first(kadm5_server_context *server_context, int fd)
{
    krb5_storage *sp;
    enum kadm_ops op;
    uint32_t ver, len;
    kadm5_ret_t ret;

    if (fd == -1) {
        errno = EINVAL;
        return NULL;
    }

    sp = krb5_storage_from_fd(fd);
    if (sp == NULL)
        return NULL;

    if (krb5_storage_seek(sp, 0, SEEK_SET) == -1)
        return NULL;

    ret = get_header(sp, LOG_DOPEEK, &ver, NULL, &op, &len);
    if (ret) {
        krb5_storage_free(sp);
        errno = ret;
        return NULL;
    }
    if (op == kadm_nop && len == LOG_UBER_LEN && seek_next(sp) == -1) {
        krb5_storage_free(sp);
        return NULL;
    }
    return sp;
}

/*
 * Go to end of log.
 *
 * XXX This really needs to return a kadm5_ret_t and either output a
 * krb5_storage * via an argument, or take one as input.
 */

krb5_storage *
kadm5_log_goto_end(kadm5_server_context *server_context, int fd)
{
    krb5_error_code ret = 0;
    krb5_storage *sp;
    enum kadm_ops op;
    uint32_t ver, len;
    uint32_t tstamp;
    uint64_t off;

    if (fd == -1) {
        errno = EINVAL;
        return NULL;
    }

    sp = krb5_storage_from_fd(fd);
    if (sp == NULL)
        return NULL;

    if (krb5_storage_seek(sp, 0, SEEK_SET) == -1) {
        ret = errno;
        goto fail;
    }
    ret = get_header(sp, LOG_NOPEEK, &ver, &tstamp, &op, &len);
    if (ret == HEIM_ERR_EOF) {
        (void) krb5_storage_seek(sp, 0, SEEK_SET);
        return sp;
    }
    if (ret == KADM5_LOG_CORRUPT)
        goto truncate;
    if (ret)
        goto fail;

    if (op == kadm_nop && len == LOG_UBER_LEN) {
        /* New style log */
        ret = krb5_ret_uint64(sp, &off);
        if (ret)
            goto truncate;

        if (krb5_storage_seek(sp, off, SEEK_SET) == -1)
            goto fail;

        if (off >= LOG_UBER_SZ) {
            ret = get_version_prev(sp, &ver, NULL);
            if (ret == 0)
                return sp;
        }
        /* Invalid offset in uber entry */
        goto truncate;
    }

    /* Old log with no uber entry */
    if (krb5_storage_seek(sp, 0, SEEK_END) == -1) {
        static int warned = 0;
        if (!warned) {
            warned = 1;
            krb5_warnx(server_context->context,
                       "Old log found; truncate it to upgrade");
        }
    }
    ret = get_version_prev(sp, &ver, NULL);
    if (ret)
        goto truncate;
    return sp;

truncate:
    /* If we can, truncate */
    if (server_context->log_context.lock_mode == LOCK_EX) {
        ret = kadm5_log_reinit(server_context, 0);
        if (ret == 0) {
            krb5_warn(server_context->context, ret,
                      "Invalid log; truncating to recover");
            if (krb5_storage_seek(sp, 0, SEEK_END) == -1)
                return NULL;
            return sp;
        }
    }
    krb5_warn(server_context->context, ret,
              "Invalid log; truncate to recover");

fail:
    errno = ret;
    krb5_storage_free(sp);
    return NULL;
}

/*
 * Return previous log entry.
 *
 * The pointer in `sp' is assumed to be at the top of the entry after
 * previous entry (e.g., at EOF).  On success, the `sp' pointer is set to
 * data portion of previous entry.  In case of error, it's not changed
 * at all.
 */
kadm5_ret_t
kadm5_log_previous(krb5_context context,
		   krb5_storage *sp,
		   uint32_t *verp,
		   time_t *tstampp,
		   enum kadm_ops *opp,
		   uint32_t *lenp)
{
    krb5_error_code ret;
    off_t oldoff;
    uint32_t ver2, len2;
    uint32_t tstamp;

    oldoff = krb5_storage_seek(sp, 0, SEEK_CUR);
    if (oldoff == -1)
        goto log_corrupt;

    /* This reads the physical version of the uber record */
    if (seek_prev(sp, verp, lenp) == -1)
        goto log_corrupt;

    ret = get_header(sp, LOG_NOPEEK, &ver2, &tstamp, opp, &len2);
    if (ret) {
        (void) krb5_storage_seek(sp, oldoff, SEEK_SET);
        return ret;
    }
    if (tstampp)
        *tstampp = tstamp;
    if (ver2 != *verp || len2 != *lenp)
        goto log_corrupt;

    return 0;

log_corrupt:
    (void) krb5_storage_seek(sp, oldoff, SEEK_SET);
    return KADM5_LOG_CORRUPT;
}

/*
 * Replay a record from the log
 */

kadm5_ret_t
kadm5_log_replay(kadm5_server_context *context,
		 enum kadm_ops op,
		 uint32_t ver,
		 uint32_t len,
		 krb5_storage *sp)
{
    switch (op) {
    case kadm_create :
	return kadm5_log_replay_create(context, ver, len, sp);
    case kadm_delete :
	return kadm5_log_replay_delete(context, ver, len, sp);
    case kadm_rename :
	return kadm5_log_replay_rename(context, ver, len, sp);
    case kadm_modify :
	return kadm5_log_replay_modify(context, ver, len, sp);
    case kadm_nop :
	return kadm5_log_replay_nop(context, ver, len, sp);
    default :
        /*
         * FIXME This default arm makes it difficult to add new kadm_ops
         *       values.
         */
	krb5_set_error_message(context->context, KADM5_FAILURE,
			       "Unsupported replay op %d", (int)op);
        (void) krb5_storage_seek(sp, len, SEEK_CUR);
	return KADM5_FAILURE;
    }
}

struct load_entries_data {
    krb5_data *entries;
    unsigned char *p;
    uint32_t first;
    uint32_t last;
    size_t bytes;
    size_t nentries;
    size_t maxbytes;
    size_t maxentries;
};


/*
 * Prepend one entry with header and trailer to the entry buffer, stopping when
 * we've reached either of the byte or entry-count limits (if non-zero).
 *
 * This is a two-pass algorithm:
 *
 * In the first pass, when entries->entries == NULL,  we compute the space
 * required, and count the entries that fit up from zero.
 *
 * In the second pass we fill the buffer, and count the entries back down to
 * zero.  The space used must be an exact fit, and the number of entries must
 * reach zero at that point or an error is returned.
 *
 * The caller MUST check that entries->nentries == 0 at the end of the second
 * pass.
 */
static kadm5_ret_t
load_entries_cb(kadm5_server_context *server_context,
            uint32_t ver,
            time_t timestamp,
            enum kadm_ops op,
            uint32_t len,
            krb5_storage *sp,
            void *ctx)
{
    struct load_entries_data *entries = ctx;
    kadm5_ret_t ret;
    ssize_t bytes;
    size_t entry_len = len + LOG_WRAPPER_SZ;
    unsigned char *base;

    if (entries->entries == NULL) {
        size_t total = entries->bytes + entry_len;

        /*
         * First run: find the size of krb5_data buffer needed.
         *
         * If the log was huge we'd have to perhaps open a temp file for this.
         * For now KISS.
         */
        if ((op == kadm_nop && entry_len == LOG_UBER_SZ) ||
            entry_len < len /*overflow?*/ ||
            (entries->maxbytes > 0 && total > entries->maxbytes) ||
            total < entries->bytes /*overflow?*/ ||
            (entries->maxentries > 0 && entries->nentries == entries->maxentries))
            return -1; /* stop iteration */
        entries->bytes = total;
        entries->first = ver;
        if (entries->nentries++ == 0)
            entries->last = ver;
        return 0;
    }

    /* Second run: load the data into memory */
    base = (unsigned char *)entries->entries->data;
    if (entries->p - base < entry_len && entries->p != base) {
        /*
         * This can't happen normally: we stop the log record iteration
         * above before we get here.  This could happen if someone wrote
         * garbage to the log while we were traversing it.  We return an
         * error instead of asserting.
         */
        return KADM5_LOG_CORRUPT;
    }

    /*
     * sp here is a krb5_storage_from_fd() of the log file, and the
     * offset pointer points at the current log record payload.
     *
     * Seek back to the start of the record poayload so we can read the
     * whole record.
     */
    if (krb5_storage_seek(sp, -LOG_HEADER_SZ, SEEK_CUR) == -1)
        return errno;

    /*
     * We read the header, payload, and trailer into the buffer we have, that
     * many bytes before the previous record we read.
     */
    errno = 0;
    bytes = krb5_storage_read(sp, entries->p - entry_len, entry_len);
    ret = errno;
    if (bytes < 0 || bytes != entry_len)
        return ret ? ret : EIO;

    entries->first = ver;
    --entries->nentries;
    entries->p -= entry_len;
    return (entries->p == base) ? -1 : 0;
}


/*
 * Serialize a tail fragment of the log as a krb5_data, this is constrained to
 * at most `maxbytes' bytes and to at most `maxentries' entries if not zero.
 */
static kadm5_ret_t
load_entries(kadm5_server_context *context, krb5_data *p,
             size_t maxentries, size_t maxbytes,
             uint32_t *first, uint32_t *last)
{
    struct load_entries_data entries;
    kadm5_ret_t ret;
    unsigned char *base;

    krb5_data_zero(p);

    *first = 0;

    memset(&entries, 0, sizeof(entries));
    entries.entries = NULL;
    entries.p = NULL;
    entries.maxentries = maxentries;
    entries.maxbytes = maxbytes;

    /* Figure out how many bytes it will take */
    ret = kadm5_log_foreach(context, kadm_backward | kadm_confirmed,
                            NULL, load_entries_cb, &entries);
    if (ret)
        return ret;

    /*
     * If no entries fit our limits, we do not truncate, instead the caller can
     * call kadm5_log_reinit() if desired.
     */
    if (entries.bytes == 0)
        return 0;

    ret = krb5_data_alloc(p, entries.bytes);
    if (ret)
        return ret;

    *first = entries.first;
    *last = entries.last;
    entries.entries = p;
    base = (unsigned char *)entries.entries->data;
    entries.p = base + entries.bytes;

    ret = kadm5_log_foreach(context, kadm_backward | kadm_confirmed,
                            NULL, load_entries_cb, &entries);
    if (ret == 0 &&
        (entries.nentries || entries.p != base || entries.first != *first))
            ret = KADM5_LOG_CORRUPT;
    if (ret)
        krb5_data_free(p);
    return ret;
}

/*
 * Truncate the log, retaining at most `keep' entries and at most `maxbytes'.
 * If `maxbytes' is zero, keep at most the default log size limit.
 */
kadm5_ret_t
kadm5_log_truncate(kadm5_server_context *context, size_t keep, size_t maxbytes)
{
    kadm5_ret_t ret;
    uint32_t first, last, last_tstamp;
    time_t now = time(NULL);
    krb5_data entries;
    krb5_storage *sp;
    ssize_t bytes;
    uint64_t sz;
    off_t off;

    if (maxbytes == 0)
        maxbytes = get_max_log_size(context->context);

    if (strcmp(context->log_context.log_file, "/dev/null") == 0)
        return 0;

    if (context->log_context.read_only)
        return EROFS;

    /* Get the desired records. */
    krb5_data_zero(&entries);
    ret = load_entries(context, &entries, keep, maxbytes, &first, &last);
    if (ret)
        return ret;

    if (first == 0) {
        /*
         * No records found/fit within resource limits.  The caller should call
         * kadm5_log_reinit(context) to truly truncate and reset the log to
         * version 0, else call again with better limits.
         */
        krb5_data_free(&entries);
        return EINVAL;
    }

    /* Check that entries.length won't overflow off_t */
    sz = LOG_UBER_SZ + entries.length;
    off = (off_t)sz;
    if (off < 0 || off != sz || sz < entries.length) {
        krb5_data_free(&entries);
        return EOVERFLOW; /* caller should ask for fewer entries */
    }

    /* Truncate to zero size and seek to zero offset */
    if (ftruncate(context->log_context.log_fd, 0) < 0 ||
        lseek(context->log_context.log_fd, 0, SEEK_SET) < 0) {
        krb5_data_free(&entries);
        return errno;
    }

    /*
     * Write the uber record and then the records loaded.  Confirm the entries
     * after writing them.
     *
     * If we crash then the log may not have all the entries we want, and
     * replaying only some of the entries will leave us in a bad state.
     * Additionally, we don't have mathematical proof that replaying the last
     * N>1 entries is always idempotent.  And though we believe we can make
     * such replays idempotent, they would still leave the HDB with
     * intermediate states that would not have occurred on the master.
     *
     * By initially setting the offset in the uber record to 0, the log will be
     * seen as invalid should we crash here, thus the only
     * harm will be that we'll reinitialize the log and force full props.
     *
     * We can't use the normal kadm5_log_*() machinery for this because
     * we must set specific version numbers and timestamps.  To keep
     * things simple we don't try to do a single atomic write here as we
     * do in kadm5_log_flush().
     *
     * We really do want to keep the new first entry's version and
     * timestamp so we don't trip up iprop.
     *
     * Keep this in sync with kadm5_log_nop().
     */
    sp = krb5_storage_from_fd(context->log_context.log_fd);
    if (sp == NULL) {
        ret = errno;
        krb5_warn(context->context, ret, "Unable to keep entries");
        krb5_data_free(&entries);
        return errno;
    }
    ret = krb5_store_uint32(sp, 0);
    if (ret == 0)
        ret = krb5_store_uint32(sp, now);
    if (ret == 0)
        ret = krb5_store_uint32(sp, kadm_nop);      /* end of preamble */
    if (ret == 0)
        ret = krb5_store_uint32(sp, LOG_UBER_LEN);  /* end of header */
    if (ret == 0)
        ret = krb5_store_uint64(sp, LOG_UBER_SZ);
    if (ret == 0)
        ret = krb5_store_uint32(sp, now);
    if (ret == 0)
        ret = krb5_store_uint32(sp, last);
    if (ret == 0)
        ret = krb5_store_uint32(sp, LOG_UBER_LEN);
    if (ret == 0)
        ret = krb5_store_uint32(sp, 0);             /* end of trailer */
    if (ret == 0) {
        bytes = krb5_storage_write(sp, entries.data, entries.length);
        if (bytes == -1)
            ret = errno;
    }
    if (ret == 0)
        ret = krb5_storage_fsync(sp);
    /* Confirm all the records now */
    if (ret == 0) {
        if (krb5_storage_seek(sp, LOG_HEADER_SZ, SEEK_SET) == -1)
            ret = errno;
    }
    if (ret == 0)
        ret = krb5_store_uint64(sp, off);
    krb5_data_free(&entries);
    krb5_storage_free(sp);

    if (ret) {
        krb5_warn(context->context, ret, "Unable to keep entries");
        (void) ftruncate(context->log_context.log_fd, LOG_UBER_SZ);
        (void) lseek(context->log_context.log_fd, 0, SEEK_SET);
        return ret;
    }

    /* Done.  Now rebuild the log_context state. */
    (void) lseek(context->log_context.log_fd, off, SEEK_SET);
    sp = kadm5_log_goto_end(context, context->log_context.log_fd);
    if (sp == NULL)
        return ENOMEM;
    ret = get_version_prev(sp, &context->log_context.version, &last_tstamp);
    context->log_context.last_time = last_tstamp;
    krb5_storage_free(sp);
    return ret;
}

/*
 * "Truncate" the log if not read only and over the desired maximum size.  We
 * attempt to retain 1/4 of the existing storage.
 *
 * Called after successful log recovery, so at this point we must have no
 * unconfirmed entries in the log.
 */
static kadm5_ret_t
truncate_if_needed(kadm5_server_context *context)
{
    kadm5_ret_t ret = 0;
    kadm5_log_context *log_context = &context->log_context;
    size_t maxbytes;
    struct stat st;

    if (log_context->log_fd == -1 || log_context->read_only)
        return 0;
    if (strcmp(context->log_context.log_file, "/dev/null") == 0)
        return 0;

    maxbytes = get_max_log_size(context->context);
    if (maxbytes <= 0)
        return 0;

    if (fstat(log_context->log_fd, &st) == -1)
        return errno;
    if (st.st_size == (size_t)st.st_size && (size_t)st.st_size <= maxbytes)
        return 0;

    /* Shrink the log by a factor of 4 */
    ret = kadm5_log_truncate(context, 0, maxbytes/4);
    return ret == EINVAL ? 0 : ret;
}

#ifndef NO_UNIX_SOCKETS

static char *default_signal = NULL;
static HEIMDAL_MUTEX signal_mutex = HEIMDAL_MUTEX_INITIALIZER;

const char *
kadm5_log_signal_socket(krb5_context context)
{
    int ret = 0;

    HEIMDAL_MUTEX_lock(&signal_mutex);
    if (!default_signal)
	ret = asprintf(&default_signal, "%s/signal", hdb_db_dir(context));
    if (ret == -1)
	default_signal = NULL;
    HEIMDAL_MUTEX_unlock(&signal_mutex);

    return krb5_config_get_string_default(context,
					  NULL,
					  default_signal,
					  "kdc",
					  "signal_socket",
					  NULL);
}

#else  /* NO_UNIX_SOCKETS */

#define SIGNAL_SOCKET_HOST "127.0.0.1"
#define SIGNAL_SOCKET_PORT "12701"

kadm5_ret_t
kadm5_log_signal_socket_info(krb5_context context,
			     int server_end,
			     struct addrinfo **ret_addrs)
{
    struct addrinfo hints;
    struct addrinfo *addrs = NULL;
    kadm5_ret_t ret = KADM5_FAILURE;
    int wsret;

    memset(&hints, 0, sizeof(hints));

    hints.ai_flags = AI_NUMERICHOST;
    if (server_end)
	hints.ai_flags |= AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    wsret = getaddrinfo(SIGNAL_SOCKET_HOST,
			SIGNAL_SOCKET_PORT,
			&hints, &addrs);

    if (wsret != 0) {
	krb5_set_error_message(context, KADM5_FAILURE,
			       "%s", gai_strerror(wsret));
	goto done;
    }

    if (addrs == NULL) {
	krb5_set_error_message(context, KADM5_FAILURE,
			       "getaddrinfo() failed to return address list");
	goto done;
    }

    *ret_addrs = addrs;
    addrs = NULL;
    ret = 0;

 done:
    if (addrs)
	freeaddrinfo(addrs);
    return ret;
}

#endif

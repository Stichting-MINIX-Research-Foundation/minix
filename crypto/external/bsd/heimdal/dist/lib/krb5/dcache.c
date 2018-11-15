/*	$NetBSD: dcache.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"

typedef struct krb5_dcache{
    krb5_ccache fcache;
    char *dir;
    char *name;
} krb5_dcache;

#define DCACHE(X) ((krb5_dcache*)(X)->data.data)
#define D2FCACHE(X) ((X)->fcache)

static krb5_error_code KRB5_CALLCONV dcc_close(krb5_context, krb5_ccache);
static krb5_error_code KRB5_CALLCONV dcc_get_default_name(krb5_context, char **);


static char *
primary_create(krb5_dcache *dc)
{
    char *primary = NULL;

    asprintf(&primary, "%s/primary", dc->dir);
    if (primary == NULL)
	return NULL;

    return primary;
}

static int
is_filename_cacheish(const char *name)
{
    return strncmp(name, "tkt", 3) == 0;
	
}

static krb5_error_code
set_default_cache(krb5_context context, krb5_dcache *dc, const char *residual)
{
    char *path = NULL, *primary = NULL;
    krb5_error_code ret;
    struct iovec iov[2];
    size_t len;
    int fd = -1;

    if (!is_filename_cacheish(residual)) {
	krb5_set_error_message(context, KRB5_CC_FORMAT,
			       "name %s is not a cache (doesn't start with tkt)", residual);
	return KRB5_CC_FORMAT;
    }

    asprintf(&path, "%s/primary-XXXXXX", dc->dir);
    if (path == NULL)
	return krb5_enomem(context);

    fd = mkstemp(path);
    if (fd < 0) {
	ret = errno;
	goto out;
    }
    rk_cloexec(fd);
#ifndef _WIN32
    if (fchmod(fd, S_IRUSR | S_IWUSR) < 0) {
	ret = errno;
	goto out;
    }
#endif
    len = strlen(residual);

    iov[0].iov_base = rk_UNCONST(residual);
    iov[0].iov_len = len;
    iov[1].iov_base = "\n";
    iov[1].iov_len = 1;

    if (writev(fd, iov, sizeof(iov)/sizeof(iov[0])) != len + 1) {
	ret = errno;
	goto out;
    }
    
    primary = primary_create(dc);
    if (primary == NULL) {
	ret = krb5_enomem(context);
	goto out;
    }

    if (rename(path, primary) < 0) {
	ret = errno;
	goto out;
    }

    close(fd);
    fd = -1;

    ret = 0;
 out:
    if (fd >= 0) {
	(void)unlink(path);
	close(fd);
    }
    if (path)
	free(path);
    if (primary)
	free(primary);

    return ret;
}

static krb5_error_code
get_default_cache(krb5_context context, krb5_dcache *dc, char **residual)
{
    krb5_error_code ret;
    char buf[MAXPATHLEN];
    char *primary;
    FILE *f;

    *residual = NULL;
    primary = primary_create(dc);
    if (primary == NULL)
	return krb5_enomem(context);

    f = fopen(primary, "r");
    if (f == NULL) {
	if (errno == ENOENT) {
	    free(primary);
	    *residual = strdup("tkt");
	    if (*residual == NULL)
		return krb5_enomem(context);
	    return 0;
	}
	ret = errno;
	krb5_set_error_message(context, ret, "failed to open %s", primary);
	free(primary);
	return ret;
    }

    if (fgets(buf, sizeof(buf), f) == NULL) {
	ret = ferror(f);
	fclose(f);
	krb5_set_error_message(context, ret, "read file %s", primary);
	free(primary);
	return ret;
    }
    fclose(f);
	
    buf[strcspn(buf, "\r\n")] = '\0';

    if (!is_filename_cacheish(buf)) {
	krb5_set_error_message(context, KRB5_CC_FORMAT,
			       "name in %s is not a cache (doesn't start with tkt)", primary);
	free(primary);
        return KRB5_CC_FORMAT;
    }

    free(primary);

    *residual = strdup(buf);
    if (*residual == NULL)
	return krb5_enomem(context);

    return 0;
}



static const char* KRB5_CALLCONV
dcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    krb5_dcache *dc = DCACHE(id);
    return dc->name;
}


static krb5_error_code
verify_directory(krb5_context context, const char *path)
{
    struct stat sb;

    if (stat(path, &sb) != 0) {
	if (errno == ENOENT) {
	    /* XXX should use mkdirx_np()  */
	    if (rk_mkdir(path, S_IRWXU) == 0)
		return 0;

	    krb5_set_error_message(context, ENOENT,
				   N_("DIR directory %s doesn't exists", ""), path);
	    return ENOENT;
	} else {
	    int ret = errno;
	    krb5_set_error_message(context, ret,
				   N_("DIR directory %s is bad: %s", ""), path, strerror(ret));
	    return errno;
	}
    }
    if (!S_ISDIR(sb.st_mode)) {
	krb5_set_error_message(context, KRB5_CC_BADNAME, 
			       N_("DIR directory %s is not a directory", ""), path);
	return KRB5_CC_BADNAME;
    }

    return 0;
}

static void
dcc_release(krb5_context context, krb5_dcache *dc)
{
    if (dc->fcache)
	krb5_cc_close(context, dc->fcache);
    if (dc->dir)
	free(dc->dir);
    if (dc->name)
	free(dc->name);
    memset(dc, 0, sizeof(*dc));
    free(dc);
}

static krb5_error_code KRB5_CALLCONV
dcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    char *filename = NULL;
    krb5_error_code ret;
    krb5_dcache *dc;
    const char *p;

    p = res;
    do {
	p = strstr(p, "..");
	if (p && (p == res || ISPATHSEP(p[-1])) && (ISPATHSEP(p[2]) || p[2] == '\0')) {
	    krb5_set_error_message(context, KRB5_CC_FORMAT,
				   N_("Path contains a .. component", ""));
	    return KRB5_CC_FORMAT;
	}
	if (p)
	    p += 3;
    } while (p);

    dc = calloc(1, sizeof(*dc));
    if (dc == NULL) {
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    
    /* check for explicit component */
    if (res[0] == ':') {
	char *q;

	dc->dir = strdup(&res[1]);
#ifdef _WIN32
	q = strrchr(dc->dir, '\\');
	if (q == NULL)
#endif
	q = strrchr(dc->dir, '/');
	if (q) {
	    *q++ = '\0';
	} else {
	    krb5_set_error_message(context, KRB5_CC_FORMAT, N_("Cache not an absolute path: %s", ""), dc->dir);
	    dcc_release(context, dc);
	    return KRB5_CC_FORMAT;
	}

	if (!is_filename_cacheish(q)) {
	    krb5_set_error_message(context, KRB5_CC_FORMAT,
				   N_("Name %s is not a cache (doesn't start with tkt)", ""), q);
	    dcc_release(context, dc);
	    return KRB5_CC_FORMAT;
	}
	
	ret = verify_directory(context, dc->dir);
	if (ret) {
	    dcc_release(context, dc);
	    return ret;
	}

	dc->name = strdup(res);
	if (dc->name == NULL) {
	    dcc_release(context, dc);
	    return krb5_enomem(context);
	}

    } else {
	char *residual;
	size_t len;

	dc->dir = strdup(res);
	if (dc->dir == NULL) {
	    dcc_release(context, dc);
	    return krb5_enomem(context);
	}

	len = strlen(dc->dir);

	if (ISPATHSEP(dc->dir[len - 1]))
	    dc->dir[len - 1] = '\0';

	ret = verify_directory(context, dc->dir);
	if (ret) {
	    dcc_release(context, dc);
	    return ret;
	}

	ret = get_default_cache(context, dc, &residual);
	if (ret) {
	    dcc_release(context, dc);
	    return ret;
	}
	asprintf(&dc->name, ":%s/%s", dc->dir, residual);
	free(residual);
	if (dc->name == NULL) {
	    dcc_release(context, dc);
	    return krb5_enomem(context);
	}
    }

    asprintf(&filename, "FILE%s", dc->name);
    if (filename == NULL) {
	dcc_release(context, dc);
	return krb5_enomem(context);
    }

    ret = krb5_cc_resolve(context, filename, &dc->fcache);
    free(filename);
    if (ret) {
	dcc_release(context, dc);
	return ret;
    }


    (*id)->data.data = dc;
    (*id)->data.length = sizeof(*dc);
    return 0;
}

static char *
copy_default_dcc_cache(krb5_context context)
{
    const char *defname;
    krb5_error_code ret;
    char *name = NULL;
    size_t len;

    len = strlen(krb5_dcc_ops.prefix);

    defname = krb5_cc_default_name(context);
    if (defname == NULL ||
	strncmp(defname, krb5_dcc_ops.prefix, len) != 0 ||
	defname[len] != ':')
    {
	ret = dcc_get_default_name(context, &name);
	if (ret)
	    return NULL;

	return name;
    } else {
	return strdup(&defname[len + 1]);
    }
}


static krb5_error_code KRB5_CALLCONV
dcc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_error_code ret;
    char *name = NULL;
    krb5_dcache *dc;
    int fd;
    size_t len;

    name = copy_default_dcc_cache(context);
    if (name == NULL) {
	krb5_set_error_message(context, KRB5_CC_FORMAT,
			       N_("Can't generate DIR caches unless its the default type", ""));
	return KRB5_CC_FORMAT;
    }

    len = strlen(krb5_dcc_ops.prefix);
    if (strncmp(name, krb5_dcc_ops.prefix, len) == 0 && name[len] == ':')
	++len;
    else
	len = 0;

    ret = dcc_resolve(context, id, name + len);
    free(name);
    name = NULL;
    if (ret)
	return ret;

    dc = DCACHE((*id));

    asprintf(&name, ":%s/tktXXXXXX", dc->dir);
    if (name == NULL) {
	dcc_close(context, *id);
	return krb5_enomem(context);
    }

    fd = mkstemp(&name[1]);
    if (fd < 0) {
	dcc_close(context, *id);
	return krb5_enomem(context);
    }
    close(fd);

    free(dc->name);
    dc->name = name;

    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_initialize(context, D2FCACHE(dc), primary_principal);
}

static krb5_error_code KRB5_CALLCONV
dcc_close(krb5_context context,
	  krb5_ccache id)
{
    dcc_release(context, DCACHE(id));
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_dcache *dc = DCACHE(id);
    krb5_ccache fcache = D2FCACHE(dc);
    dc->fcache = NULL;
    return krb5_cc_destroy(context, fcache);
}

static krb5_error_code KRB5_CALLCONV
dcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_store_cred(context, D2FCACHE(dc), creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_get_principal(context, D2FCACHE(dc), principal);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_start_seq_get(context, D2FCACHE(dc), cursor);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_next_cred(context, D2FCACHE(dc), cursor, creds);
}

static krb5_error_code KRB5_CALLCONV
dcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_end_seq_get(context, D2FCACHE(dc), cursor);
}

static krb5_error_code KRB5_CALLCONV
dcc_remove_cred(krb5_context context,
		 krb5_ccache id,
		 krb5_flags which,
		 krb5_creds *cred)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_remove_cred(context, D2FCACHE(dc), which, cred);
}

static krb5_error_code KRB5_CALLCONV
dcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_set_flags(context, D2FCACHE(dc), flags);
}

static int KRB5_CALLCONV
dcc_get_version(krb5_context context,
		krb5_ccache id)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_get_version(context, D2FCACHE(dc));
}

struct dcache_iter {
    int first;
    krb5_dcache *dc;
};

static krb5_error_code KRB5_CALLCONV
dcc_get_cache_first(krb5_context context, krb5_cc_cursor *cursor)
{
    struct dcache_iter *iter;
    krb5_error_code ret;
    char *name;

    *cursor = NULL;
    iter = calloc(1, sizeof(*iter));
    if (iter == NULL)
	return krb5_enomem(context);
    iter->first = 1;

    name = copy_default_dcc_cache(context);
    if (name == NULL) {
        free(iter);
	krb5_set_error_message(context, KRB5_CC_FORMAT,
			       N_("Can't generate DIR caches unless its the default type", ""));
	return KRB5_CC_FORMAT;
    }

    ret = dcc_resolve(context, NULL, name);
    free(name);
    if (ret) {
        free(iter);
        return ret;
    }

    /* XXX We need to opendir() here */

    *cursor = iter;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_get_cache_next(krb5_context context, krb5_cc_cursor cursor, krb5_ccache *id)
{
    struct dcache_iter *iter = cursor;

    if (iter == NULL)
        return krb5_einval(context, 2);

    if (!iter->first) {
	krb5_clear_error_message(context);
	return KRB5_CC_END;
    }

    /* XXX We need to readdir() here */
    iter->first = 0;

    return KRB5_CC_END;
}

static krb5_error_code KRB5_CALLCONV
dcc_end_cache_get(krb5_context context, krb5_cc_cursor cursor)
{
    struct dcache_iter *iter = cursor;

    if (iter == NULL)
        return krb5_einval(context, 2);

    /* XXX We need to closedir() here */
    if (iter->dc)
	dcc_release(context, iter->dc);
    free(iter);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
dcc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_dcache *dcfrom = DCACHE(from);
    krb5_dcache *dcto = DCACHE(to);
    return krb5_cc_move(context, D2FCACHE(dcfrom), D2FCACHE(dcto));
}

static krb5_error_code KRB5_CALLCONV
dcc_get_default_name(krb5_context context, char **str)
{
    return _krb5_expand_default_cc_name(context,
					KRB5_DEFAULT_CCNAME_DIR,
					str);
}

static krb5_error_code KRB5_CALLCONV
dcc_set_default(krb5_context context, krb5_ccache id)
{
    krb5_dcache *dc = DCACHE(id);
    const char *name;

    name = krb5_cc_get_name(context, D2FCACHE(dc));
    if (name == NULL)
	return ENOENT;

    return set_default_cache(context, dc, name);
}

static krb5_error_code KRB5_CALLCONV
dcc_lastchange(krb5_context context, krb5_ccache id, krb5_timestamp *mtime)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_last_change_time(context, D2FCACHE(dc), mtime);
}

static krb5_error_code KRB5_CALLCONV
dcc_set_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat kdc_offset)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_set_kdc_offset(context, D2FCACHE(dc), kdc_offset);
}

static krb5_error_code KRB5_CALLCONV
dcc_get_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat *kdc_offset)
{
    krb5_dcache *dc = DCACHE(id);
    return krb5_cc_get_kdc_offset(context, D2FCACHE(dc), kdc_offset);
}


/**
 * Variable containing the DIR based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

KRB5_LIB_VARIABLE const krb5_cc_ops krb5_dcc_ops = {
    KRB5_CC_OPS_VERSION,
    "DIR",
    dcc_get_name,
    dcc_resolve,
    dcc_gen_new,
    dcc_initialize,
    dcc_destroy,
    dcc_close,
    dcc_store_cred,
    NULL, /* dcc_retrieve */
    dcc_get_principal,
    dcc_get_first,
    dcc_get_next,
    dcc_end_get,
    dcc_remove_cred,
    dcc_set_flags,
    dcc_get_version,
    dcc_get_cache_first,
    dcc_get_cache_next,
    dcc_end_cache_get,
    dcc_move,
    dcc_get_default_name,
    dcc_set_default,
    dcc_lastchange,
    dcc_set_kdc_offset,
    dcc_get_kdc_offset
};

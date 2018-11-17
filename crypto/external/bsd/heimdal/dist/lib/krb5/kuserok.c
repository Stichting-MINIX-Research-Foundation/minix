/*	$NetBSD: kuserok.c,v 1.2.4.1 2017/09/11 04:58:44 snj Exp $	*/

/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include "kuserok_plugin.h"
#include <dirent.h>

#ifndef SYSTEM_K5LOGIN_DIR
/*
 * System k5login location.  File namess in this directory are expected
 * to be usernames and to contain a list of principals allowed to login
 * as the user named the same as the file.
 */
#define SYSTEM_K5LOGIN_DIR SYSCONFDIR "/k5login.d"
#endif

/* Plugin framework bits */

struct plctx {
    const char           *rule;
    const char           *k5login_dir;
    const char           *luser;
    krb5_const_principal principal;
    unsigned int         flags;
    krb5_boolean         result;
};

static krb5_error_code KRB5_LIB_CALL
plcallback(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_kuserok_ftable *locate = plug;
    struct plctx *plctx = userctx;

    return locate->kuserok(plugctx, context, plctx->rule, plctx->flags,
			   plctx->k5login_dir, plctx->luser, plctx->principal,
			   &plctx->result);
}

static krb5_error_code plugin_reg_ret;
static krb5plugin_kuserok_ftable kuserok_simple_plug;
static krb5plugin_kuserok_ftable kuserok_sys_k5login_plug;
static krb5plugin_kuserok_ftable kuserok_user_k5login_plug;
static krb5plugin_kuserok_ftable kuserok_deny_plug;

static void
reg_def_plugins_once(void *ctx)
{
    krb5_error_code ret;
    krb5_context context = ctx;

    plugin_reg_ret = krb5_plugin_register(context, PLUGIN_TYPE_DATA,
					  KRB5_PLUGIN_KUSEROK,
					  &kuserok_simple_plug);
    ret = krb5_plugin_register(context, PLUGIN_TYPE_DATA,
                               KRB5_PLUGIN_KUSEROK, &kuserok_sys_k5login_plug);
    if (!plugin_reg_ret)
	plugin_reg_ret = ret;
    ret = krb5_plugin_register(context, PLUGIN_TYPE_DATA,
                               KRB5_PLUGIN_KUSEROK, &kuserok_user_k5login_plug);
    if (!plugin_reg_ret)
	plugin_reg_ret = ret;
    ret = krb5_plugin_register(context, PLUGIN_TYPE_DATA,
                               KRB5_PLUGIN_KUSEROK, &kuserok_deny_plug);
    if (!plugin_reg_ret)
	plugin_reg_ret = ret;
}

/**
 * This function is designed to be portable for Win32 and POSIX.  The
 * design does lead to multiple getpwnam_r() calls, but this is probably
 * not a big deal.
 *
 * Inputs:
 *
 * @param context            A krb5_context
 * @param filename	     Name of item to introspection
 * @param is_system_location TRUE if the dir/file are system locations or
 *                     	     FALSE if they are user home directory locations
 * @param dir                Directory (optional)
 * @param dirlstat           A pointer to struct stat for the directory (optional)
 * @param file               File (optional)
 * @param owner              Name of user that is expected to own the file
 */

static krb5_error_code
check_owner_dir(krb5_context context,
		const char *filename,
		krb5_boolean is_system_location,
		DIR *dir,
		struct stat *dirlstat,
		const char *owner)
{
#ifdef _WIN32
    /*
     * XXX Implement this!
     *
     * The thing to do is to call _get_osfhandle() on fileno(file) and
     * dirfd(dir) to get HANDLEs to the same, then call
     * GetSecurityInfo() on those HANDLEs to get the security descriptor
     * (SD), then check the owner and DACL.  Checking the DACL sounds
     * like a lot of work (what, derive a mode from the ACL the way
     * NFSv4 servers do?).  Checking the owner means doing an LSARPC
     * lookup at least (to get the user's SID). 
     */
    if (is_system_location || owner == NULL)
	return 0;
    krb5_set_error_message(context, EACCES,
			   "User k5login files not supported on Windows");
    return EACCES;
#else
    struct passwd pw, *pwd = NULL;
    char pwbuf[2048];
    struct stat st;

    heim_assert(owner != NULL, "no directory owner ?");

    if (rk_getpwnam_r(owner, &pw, pwbuf, sizeof(pwbuf), &pwd) != 0) {
	krb5_set_error_message(context, errno,
			       "User unknown %s (getpwnam_r())", owner);
	return EACCES;
    }
    if (pwd == NULL) {
	krb5_set_error_message(context, EACCES, "no user %s", owner);
	return EACCES;
    }

    if (fstat(dirfd(dir), &st) == -1) {
	krb5_set_error_message(context, EACCES,
			       "fstat(%s) of k5login.d failed",
			       filename);
	return EACCES;
    }
    if (!S_ISDIR(st.st_mode)) {
	krb5_set_error_message(context, ENOTDIR, "%s not a directory",
			       filename);
	return ENOTDIR;
    }
    if (st.st_dev != dirlstat->st_dev || st.st_ino != dirlstat->st_ino) {
	krb5_set_error_message(context, EACCES,
			       "%s was renamed during kuserok "
			       "operation", filename);
	return EACCES;
    }
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
	krb5_set_error_message(context, EACCES,
			       "%s has world and/or group write "
			       "permissions", filename);
	return EACCES;
    }
    if (pwd->pw_uid != st.st_uid && st.st_uid != 0) {
	krb5_set_error_message(context, EACCES,
			       "%s not owned by the user (%s) or root",
			       filename, owner);
	return EACCES;
    }
    
    return 0;
#endif
}

static krb5_error_code
check_owner_file(krb5_context context,
		 const char *filename,
		 FILE *file, const char *owner)
{
#ifdef _WIN32
    /*
     * XXX Implement this!
     *
     * The thing to do is to call _get_osfhandle() on fileno(file) and
     * dirfd(dir) to get HANDLEs to the same, then call
     * GetSecurityInfo() on those HANDLEs to get the security descriptor
     * (SD), then check the owner and DACL.  Checking the DACL sounds
     * like a lot of work (what, derive a mode from the ACL the way
     * NFSv4 servers do?).  Checking the owner means doing an LSARPC
     * lookup at least (to get the user's SID). 
     */
    if (owner == NULL)
	return 0;

    krb5_set_error_message(context, EACCES,
			   "User k5login files not supported on Windows");
    return EACCES;
#else
    struct passwd pw, *pwd = NULL;
    char pwbuf[2048];
    struct stat st;

    if (owner == NULL)
	return 0;

    if (rk_getpwnam_r(owner, &pw, pwbuf, sizeof(pwbuf), &pwd) != 0) {
	krb5_set_error_message(context, errno,
			       "User unknown %s (getpwnam_r())", owner);
	return EACCES;
    }
    if (pwd == NULL) {
	krb5_set_error_message(context, EACCES, "no user %s", owner);
	return EACCES;
    }

    if (fstat(fileno(file), &st) == -1) {
	krb5_set_error_message(context, EACCES, "fstat(%s) of k5login failed",
			       filename);
	return EACCES;
    }
    if (S_ISDIR(st.st_mode)) {
	krb5_set_error_message(context, EISDIR, "k5login: %s is a directory",
			       filename);
	return EISDIR;
    }
    if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
	krb5_set_error_message(context, EISDIR,
			       "k5login %s has world and/or group write "
			       "permissions", filename);
	return EACCES;
    }
    if (pwd->pw_uid != st.st_uid && st.st_uid != 0) {
	krb5_set_error_message(context, EACCES,
			       "k5login %s not owned by the user or root",
			       filename);
	return EACCES;
    }

    return 0;
#endif
}


/* see if principal is mentioned in the filename access file, return
   TRUE (in result) if so, FALSE otherwise */

static krb5_error_code
check_one_file(krb5_context context,
	       const char *filename,
	       const char *owner,
	       krb5_boolean is_system_location,
	       krb5_const_principal principal,
	       krb5_boolean *result)
{
    FILE *f;
    char buf[BUFSIZ];
    krb5_error_code ret;

    *result = FALSE;

    f = fopen(filename, "r");
    if (f == NULL)
	return errno;
    rk_cloexec_file(f);

    ret = check_owner_file(context, filename, f, owner);
    if (ret)
	goto out;

    while (fgets(buf, sizeof(buf), f) != NULL) {
	krb5_principal tmp;
	char *newline = buf + strcspn(buf, "\n");

	if (*newline != '\n') {
	    int c;
	    c = fgetc(f);
	    if (c != EOF) {
		while (c != EOF && c != '\n')
		    c = fgetc(f);
		/* line was too long, so ignore it */
		continue;
	    }
	}
	*newline = '\0';
	ret = krb5_parse_name(context, buf, &tmp);
	if (ret)
	    continue;
	*result = krb5_principal_compare(context, principal, tmp);
	krb5_free_principal(context, tmp);
	if (*result) {
	    fclose (f);
	    return 0;
	}
    }

out:
    fclose(f);
    return 0;
}

static krb5_error_code
check_directory(krb5_context context,
		const char *dirname,
		const char *owner,
	        krb5_boolean is_system_location,
		krb5_const_principal principal,
		krb5_boolean *result)
{
    DIR *d;
    struct dirent *dent;
    char filename[MAXPATHLEN];
    size_t len;
    krb5_error_code ret = 0;
    struct stat st;

    *result = FALSE;

    if (lstat(dirname, &st) < 0)
	return errno;

    if (!S_ISDIR(st.st_mode)) {
	krb5_set_error_message(context, ENOTDIR, "k5login.d not a directory");
	return ENOTDIR;
    }

    if ((d = opendir(dirname)) == NULL) {
	krb5_set_error_message(context, ENOTDIR, "Could not open k5login.d");
	return errno;
    }

    ret = check_owner_dir(context, dirname, is_system_location, d, &st, owner);
    if (ret)
	goto out;

    while ((dent = readdir(d)) != NULL) {
	/*
	 * XXX: Should we also skip files whose names start with "."?
	 * Vim ".filename.swp" files are also good candidates to skip.
	 * Once we ignore "#*" and "*~", it is not clear what other
	 * heuristics to apply.
	 */
	if (strcmp(dent->d_name, ".") == 0 ||
	   strcmp(dent->d_name, "..") == 0 ||
	   dent->d_name[0] == '#' ||			  /* emacs autosave */
	   dent->d_name[strlen(dent->d_name) - 1] == '~') /* emacs backup */
	    continue;
	len = snprintf(filename, sizeof(filename), "%s/%s", dirname, dent->d_name);
	/* Skip too-long filenames that got truncated by snprintf() */
	if (len < sizeof(filename)) {
	    ret = check_one_file(context, filename, owner, is_system_location,
				 principal, result);
	    if (ret == 0 && *result == TRUE)
		break;
	}
	ret = 0; /* don't propagate errors upstream */
    }

out:
    closedir(d);
    return ret;
}

static krb5_error_code
check_an2ln(krb5_context context,
	    krb5_const_principal principal,
	    const char *luser,
	    krb5_boolean *result)
{
    krb5_error_code ret;
    char *lname;

#if 0
    /* XXX Should we make this an option? */
    /* multi-component principals can never match */
    if (krb5_principal_get_comp_string(context, principal, 1) != NULL) {
	*result =  FALSE;
	return 0;
    }
#endif

    lname = malloc(strlen(luser) + 1);
    if (lname == NULL)
	return krb5_enomem(context);
    ret = krb5_aname_to_localname(context, principal, strlen(luser)+1, lname);
    if (ret)
	goto out;
    if (strcmp(lname, luser) == 0)
	*result = TRUE;
    else
	*result = FALSE;

out:
    free(lname);
    return 0;

}

/**
 * This function takes the name of a local user and checks if
 * principal is allowed to log in as that user.
 *
 * The user may have a ~/.k5login file listing principals that are
 * allowed to login as that user. If that file does not exist, all
 * principals with a only one component that is identical to the
 * username, and a realm considered local, are allowed access.
 *
 * The .k5login file must contain one principal per line, be owned by
 * user and not be writable by group or other (but must be readable by
 * anyone).
 *
 * Note that if the file exists, no implicit access rights are given
 * to user@@LOCALREALM.
 *
 * Optionally, a set of files may be put in ~/.k5login.d (a
 * directory), in which case they will all be checked in the same
 * manner as .k5login.  The files may be called anything, but files
 * starting with a hash (#) , or ending with a tilde (~) are
 * ignored. Subdirectories are not traversed. Note that this directory
 * may not be checked by other Kerberos implementations.
 *
 * If no configuration file exists, match user against local domains,
 * ie luser@@LOCAL-REALMS-IN-CONFIGURATION-FILES.
 *
 * @param context Kerberos 5 context.
 * @param principal principal to check if allowed to login
 * @param luser local user id
 *
 * @return returns TRUE if access should be granted, FALSE otherwise.
 *
 * @ingroup krb5_support
 */

KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
krb5_kuserok(krb5_context context,
	     krb5_principal principal,
	     const char *luser)
{
    return _krb5_kuserok(context, principal, luser, TRUE);
}


KRB5_LIB_FUNCTION krb5_boolean KRB5_LIB_CALL
_krb5_kuserok(krb5_context context,
	      krb5_principal principal,
	      const char *luser,
	      krb5_boolean an2ln_ok)
{
    static heim_base_once_t reg_def_plugins = HEIM_BASE_ONCE_INIT;
    krb5_error_code ret;
    struct plctx ctx;
    char **rules;

    /*
     * XXX we should have a struct with a krb5_context field and a
     * krb5_error_code fied and pass the address of that as the ctx
     * argument of heim_base_once_f().  For now we use a static to
     * communicate failures.  Actually, we ignore failures anyways,
     * since we can't return them.
     */
    heim_base_once_f(&reg_def_plugins, context, reg_def_plugins_once);

    ctx.flags = 0;
    ctx.luser = luser;
    ctx.principal = principal;
    ctx.result = FALSE;

    ctx.k5login_dir = krb5_config_get_string(context, NULL, "libdefaults",
					     "k5login_directory", NULL);

    if (an2ln_ok)
	ctx.flags |= KUSEROK_ANAME_TO_LNAME_OK;

    if (krb5_config_get_bool_default(context, NULL, FALSE, "libdefaults",
				     "k5login_authoritative", NULL))
	ctx.flags |= KUSEROK_K5LOGIN_IS_AUTHORITATIVE;

    if ((ctx.flags & KUSEROK_K5LOGIN_IS_AUTHORITATIVE) && plugin_reg_ret)
	return plugin_reg_ret; /* fail safe */

    rules = krb5_config_get_strings(context, NULL, "libdefaults",
				    "kuserok", NULL);
    if (rules == NULL) {
	/* Default: check ~/.k5login */
	ctx.rule = "USER-K5LOGIN";

	ret = plcallback(context, &kuserok_user_k5login_plug, NULL, &ctx);
	if (ret == 0)
	    goto out;

	ctx.rule = "SIMPLE";
	ret = plcallback(context, &kuserok_simple_plug, NULL, &ctx);
	if (ret == 0)
	    goto out;

	ctx.result = FALSE;
    } else {
	size_t n;

	for (n = 0; rules[n]; n++) {
	    ctx.rule = rules[n];

	    ret = _krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_KUSEROK,
				     KRB5_PLUGIN_KUSEROK_VERSION_0, 0,
				     &ctx, plcallback);
	    if (ret != KRB5_PLUGIN_NO_HANDLE) 
		goto out;
	}
    }

out:
    krb5_config_free_strings(rules);

    return ctx.result;
}

/*
 * Simple kuserok: check that the lname for the aname matches luser.
 */

static krb5_error_code KRB5_LIB_CALL
kuserok_simple_plug_f(void *plug_ctx, krb5_context context, const char *rule,
		      unsigned int flags, const char *k5login_dir,
		      const char *luser, krb5_const_principal principal,
		      krb5_boolean *result)
{
    krb5_error_code ret;

    if (strcmp(rule, "SIMPLE") != 0 || (flags & KUSEROK_ANAME_TO_LNAME_OK) == 0)
	return KRB5_PLUGIN_NO_HANDLE;

    ret = check_an2ln(context, principal, luser, result);
    if (ret == 0 && *result == FALSE)
	return KRB5_PLUGIN_NO_HANDLE;

    return 0;
}

/*
 * Check k5login files in a system location, rather than in home
 * directories.
 */

static krb5_error_code KRB5_LIB_CALL
kuserok_sys_k5login_plug_f(void *plug_ctx, krb5_context context,
			   const char *rule, unsigned int flags,
			   const char *k5login_dir, const char *luser,
			   krb5_const_principal principal, krb5_boolean *result)
{
    char filename[MAXPATHLEN];
    size_t len;
    const char *profile_dir = NULL;
    krb5_error_code ret;

    *result = FALSE;

    if (strcmp(rule, "SYSTEM-K5LOGIN") != 0 &&
	strncmp(rule, "SYSTEM-K5LOGIN:", strlen("SYSTEM-K5LOGIN:")) != 0)
	return KRB5_PLUGIN_NO_HANDLE;

    profile_dir = strchr(rule, ':');
    if (profile_dir == NULL)
	profile_dir = k5login_dir ? k5login_dir : SYSTEM_K5LOGIN_DIR;
    else
	profile_dir++;

    len = snprintf(filename, sizeof(filename), "%s/%s", profile_dir, luser);
    if (len < sizeof(filename)) {
	ret = check_one_file(context, filename, NULL, TRUE, principal, result);

	if (ret == 0 &&
	    ((flags & KUSEROK_K5LOGIN_IS_AUTHORITATIVE) || *result == TRUE))
	    return 0;
    }

    *result = FALSE;
    return KRB5_PLUGIN_NO_HANDLE;
}

/*
 * Check ~luser/.k5login and/or ~/luser/.k5login.d
 */

static krb5_error_code KRB5_LIB_CALL
kuserok_user_k5login_plug_f(void *plug_ctx, krb5_context context,
			    const char *rule, unsigned int flags,
			    const char *k5login_dir, const char *luser,
			    krb5_const_principal principal,
			    krb5_boolean *result)
{
#ifdef _WIN32
    return KRB5_PLUGIN_NO_HANDLE;
#else
    char *path;
    char *path_exp;
    const char *profile_dir = NULL;
    krb5_error_code ret;
    krb5_boolean found_file = FALSE;
    struct passwd pw, *pwd = NULL;
    char pwbuf[2048];

    if (strcmp(rule, "USER-K5LOGIN") != 0)
	return KRB5_PLUGIN_NO_HANDLE;

    profile_dir = k5login_dir;
    if (profile_dir == NULL) {
	/* Don't deadlock with gssd or anything of the sort */
	if (!_krb5_homedir_access(context))
	    return KRB5_PLUGIN_NO_HANDLE;

	if (rk_getpwnam_r(luser, &pw, pwbuf, sizeof(pwbuf), &pwd) != 0) {
	    krb5_set_error_message(context, errno, "User unknown (getpwnam_r())");
	    return KRB5_PLUGIN_NO_HANDLE;
	}
	if (pwd == NULL) {
	    krb5_set_error_message(context, errno, "User unknown (getpwnam())");
	    return KRB5_PLUGIN_NO_HANDLE;
	}
	profile_dir = pwd->pw_dir;
    }

#define KLOGIN "/.k5login"

    if (asprintf(&path, "%s/.k5login.d", profile_dir) == -1)
	return krb5_enomem(context);

    ret = _krb5_expand_path_tokensv(context, path, 1, &path_exp,
				    "luser", luser, NULL);
    free(path);
    if (ret)
	return ret;
    path = path_exp;

    /* check user's ~/.k5login */
    path[strlen(path) - strlen(".d")] = '\0';
    ret = check_one_file(context, path, luser, FALSE, principal, result);

    /*
     * A match in ~/.k5login is sufficient.  A non-match, falls through to the
     * .k5login.d code below.
     */
    if (ret == 0 && *result == TRUE) {
	free(path);
	return 0;
    }
    if (ret != ENOENT)
	found_file = TRUE;

    /*
     * A match in ~/.k5login.d/somefile is sufficient.  A non-match, falls
     * through to the code below that handles negative results.
     *
     * XXX: put back the .d; clever|hackish? you decide
     */
    path[strlen(path)] = '.';
    ret = check_directory(context, path, luser, FALSE, principal, result);
    free(path);
    if (ret == 0 && *result == TRUE)
	return 0;
    if (ret != ENOENT && ret != ENOTDIR)
	found_file = TRUE;

    /*
     * When either ~/.k5login or ~/.k5login.d/ exists, but neither matches
     * and we're authoritative, we're done.  Otherwise, give other plugins
     * a chance.
     */
    *result = FALSE;
    if (found_file && (flags & KUSEROK_K5LOGIN_IS_AUTHORITATIVE))
	return 0;
    return KRB5_PLUGIN_NO_HANDLE;
#endif
}

static krb5_error_code KRB5_LIB_CALL
kuserok_deny_plug_f(void *plug_ctx, krb5_context context, const char *rule,
		    unsigned int flags, const char *k5login_dir,
		    const char *luser, krb5_const_principal principal,
		    krb5_boolean *result)
{
    if (strcmp(rule, "DENY") != 0)
	return KRB5_PLUGIN_NO_HANDLE;

    *result = FALSE;
    return 0;
}

static krb5_error_code KRB5_LIB_CALL
kuser_ok_null_plugin_init(krb5_context context, void **ctx)
{
    *ctx = NULL;
    return 0;
}

static void KRB5_LIB_CALL
kuser_ok_null_plugin_fini(void *ctx)
{
    return;
}

static krb5plugin_kuserok_ftable kuserok_simple_plug = {
    KRB5_PLUGIN_KUSEROK_VERSION_0,
    kuser_ok_null_plugin_init,
    kuser_ok_null_plugin_fini,
    kuserok_simple_plug_f,
};

static krb5plugin_kuserok_ftable kuserok_sys_k5login_plug = {
    KRB5_PLUGIN_KUSEROK_VERSION_0,
    kuser_ok_null_plugin_init,
    kuser_ok_null_plugin_fini,
    kuserok_sys_k5login_plug_f,
};

static krb5plugin_kuserok_ftable kuserok_user_k5login_plug = {
    KRB5_PLUGIN_KUSEROK_VERSION_0,
    kuser_ok_null_plugin_init,
    kuser_ok_null_plugin_fini,
    kuserok_user_k5login_plug_f,
};

static krb5plugin_kuserok_ftable kuserok_deny_plug = {
    KRB5_PLUGIN_KUSEROK_VERSION_0,
    kuser_ok_null_plugin_init,
    kuser_ok_null_plugin_fini,
    kuserok_deny_plug_f,
};


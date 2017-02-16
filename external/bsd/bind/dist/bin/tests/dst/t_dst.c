/*	$NetBSD: t_dst.c,v 1.10 2014/12/10 04:37:53 christos Exp $	*/

/*
 * Copyright (C) 2004, 2005, 2007-2009, 2011-2014  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* Id: t_dst.c,v 1.60 2011/03/17 23:47:29 tbox Exp  */

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>		/* XXX */
#else
#include <direct.h>
#endif

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/mem.h>
#include <isc/region.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/name.h>

#include <dst/dst.h>
#include <dst/result.h>

#include <tests/t_api.h>

#ifndef PATH_MAX
#define PATH_MAX	256
#endif

/*
 * Adapted from the original dst_test.c program.
 */

static void
cleandir(char *path) {
	isc_dir_t	dir;
	char		fullname[PATH_MAX + 1];
	size_t		l;
	isc_result_t	ret;

	isc_dir_init(&dir);
	ret = isc_dir_open(&dir, path);
	if (ret != ISC_R_SUCCESS) {
		t_info("isc_dir_open(%s) failed %s\n",
		       path, isc_result_totext(ret));
		return;
	}

	while (isc_dir_read(&dir) == ISC_R_SUCCESS) {
		if (!strcmp(dir.entry.name, "."))
			continue;
		if (!strcmp(dir.entry.name, ".."))
			continue;
		(void)strlcpy(fullname, path, sizeof(fullname));
		(void)strlcat(fullname, "/", sizeof(fullname));
		l = strlcat(fullname, dir.entry.name, sizeof(fullname));
		if (l < sizeof(fullname)) {
			if (remove(fullname))
				t_info("remove(%s) failed %d\n", fullname,
				       errno);
		} else
		       t_info("unable to remove '%s/%s': path too long\n",
			      path, dir.entry.name);

	}
	isc_dir_close(&dir);
	if (rmdir(path))
		t_info("rmdir(%s) failed %d\n", path, errno);

	return;
}

static void
use(dst_key_t *key, isc_mem_t *mctx, isc_result_t exp_result, int *nfails) {

	isc_result_t ret;
	const char *data = "This is some data";
	unsigned char sig[512];
	isc_buffer_t databuf, sigbuf;
	isc_region_t datareg, sigreg;
	dst_context_t *ctx = NULL;

	isc_buffer_init(&sigbuf, sig, sizeof(sig));
	isc_buffer_constinit(&databuf, data, strlen(data));
	isc_buffer_add(&databuf, strlen(data));
	isc_buffer_usedregion(&databuf, &datareg);

	ret = dst_context_create3(key, mctx,
				  DNS_LOGCATEGORY_GENERAL, ISC_TRUE, &ctx);
	if (ret != exp_result) {
		t_info("dst_context_create(%d) returned (%s) expected (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret),
		       dst_result_totext(exp_result));
		++*nfails;
		return;
	}
	if (exp_result != ISC_R_SUCCESS)
		return;
	ret = dst_context_adddata(ctx, &datareg);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_context_adddata(%d) returned (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret));
		++*nfails;
		dst_context_destroy(&ctx);
		return;
	}
	ret = dst_context_sign(ctx, &sigbuf);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_context_sign(%d) returned (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret));
		++*nfails;
		dst_context_destroy(&ctx);
		return;
	}
	dst_context_destroy(&ctx);

	isc_buffer_remainingregion(&sigbuf, &sigreg);
	ret = dst_context_create3(key, mctx,
				  DNS_LOGCATEGORY_GENERAL, ISC_FALSE, &ctx);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_context_create(%d) returned (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret));
		++*nfails;
		return;
	}
	ret = dst_context_adddata(ctx, &datareg);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_context_adddata(%d) returned (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret));
		++*nfails;
		dst_context_destroy(&ctx);
		return;
	}
	ret = dst_context_verify(ctx, &sigreg);
	if (ret != exp_result) {
		t_info("dst_context_verify(%d) returned (%s) expected (%s)\n",
		       dst_key_alg(key), dst_result_totext(ret),
		       dst_result_totext(exp_result));
		++*nfails;
		dst_context_destroy(&ctx);
		return;
	}
	dst_context_destroy(&ctx);
}

static void
dh(dns_name_t *name1, int id1, dns_name_t *name2, int id2, isc_mem_t *mctx,
   isc_result_t exp_result, int *nfails, int *nprobs)
{
	dst_key_t	*key1 = NULL, *key2 = NULL;
	isc_result_t	ret;
	char		current[PATH_MAX + 1];
	char		tmp[PATH_MAX + 1];
	char		*p;
	int		alg = DST_ALG_DH;
	int		type = DST_TYPE_PUBLIC|DST_TYPE_PRIVATE|DST_TYPE_KEY;
	unsigned char	array1[1024], array2[1024];
	isc_buffer_t	b1, b2;
	isc_region_t	r1, r2;

	UNUSED(exp_result);

	p = getcwd(current, PATH_MAX);;
	if (p == NULL) {
		t_info("getcwd failed %d\n", errno);
		++*nprobs;
		goto cleanup;
	}

	ret = dst_key_fromfile(name1, id1, alg, type, current, mctx, &key1);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_key_fromfile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	ret = dst_key_fromfile(name2, id2, alg, type, current, mctx, &key2);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_key_fromfile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

#ifndef WIN32
	ret = isc_file_mktemplate("/tmp/", tmp, sizeof(tmp));
#else
	ret = isc_file_mktemplate(getenv("TEMP"), tmp, sizeof(tmp));
#endif
	if (ret != ISC_R_SUCCESS) {
		t_info("isc_file_mktemplate failed %s\n",
		       isc_result_totext(ret));
		++*nprobs;
		goto cleanup;
	}

	ret = isc_dir_createunique(tmp);
	if (ret != ISC_R_SUCCESS) {
		t_info("isc_dir_createunique failed %s\n",
		       isc_result_totext(ret));
		++*nprobs;
		goto cleanup;
	}

	ret = dst_key_tofile(key1, type, tmp);
	if (ret != 0) {
		t_info("dst_key_tofile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	ret = dst_key_tofile(key2, type, tmp);
	if (ret != 0) {
		t_info("dst_key_tofile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	cleandir(tmp);

	isc_buffer_init(&b1, array1, sizeof(array1));
	ret = dst_key_computesecret(key1, key2, &b1);
	if (ret != 0) {
		t_info("dst_computesecret() returned: %s\n",
		       dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	isc_buffer_init(&b2, array2, sizeof(array2));
	ret = dst_key_computesecret(key2, key1, &b2);
	if (ret != 0) {
		t_info("dst_computesecret() returned: %s\n",
		       dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	isc_buffer_usedregion(&b1, &r1);
	isc_buffer_usedregion(&b2, &r2);
	if (r1.length != r2.length || memcmp(r1.base, r2.base, r1.length) != 0)
	{
		t_info("computed secrets don't match\n");
		++*nfails;
		goto cleanup;
	}

 cleanup:
	if (key1 != NULL)
		dst_key_free(&key1);
	if (key2 != NULL)
		dst_key_free(&key2);
}

static void
io(dns_name_t *name, isc_uint16_t id, isc_uint16_t alg, int type,
   isc_mem_t *mctx, isc_result_t exp_result, int *nfails, int *nprobs)
{
	dst_key_t	*key = NULL;
	isc_result_t	ret;
	char		current[PATH_MAX + 1];
	char		tmp[PATH_MAX + 1];
	char		*p;

	p = getcwd(current, PATH_MAX);;
	if (p == NULL) {
		t_info("getcwd failed %d\n", errno);
		++*nprobs;
		goto failure;
	}

	ret = dst_key_fromfile(name, id, alg, type, current, mctx, &key);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_key_fromfile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto failure;
	}

	if (dst_key_id(key) != id) {
		t_info("key ID incorrect\n");
		++*nfails;
		goto failure;
	}

	if (dst_key_alg(key) != alg) {
		t_info("key algorithm incorrect\n");
		++*nfails;
		goto failure;
	}

	if (dst_key_getttl(key) != 0) {
		t_info("initial key TTL incorrect\n");
		++*nfails;
		goto failure;
	}

#ifndef WIN32
	ret = isc_file_mktemplate("/tmp/", tmp, sizeof(tmp));
#else
	ret = isc_file_mktemplate(getenv("TEMP"), tmp, sizeof(tmp));
#endif
	if (ret != ISC_R_SUCCESS) {
		t_info("isc_file_mktemplate failed %s\n",
		       isc_result_totext(ret));
		++*nprobs;
		goto failure;
	}

	ret = isc_dir_createunique(tmp);
	if (ret != ISC_R_SUCCESS) {
		t_info("mkdir failed %d\n", errno);
		++*nprobs;
		goto failure;
	}

	ret = dst_key_tofile(key, type, tmp);
	if (ret != 0) {
		t_info("dst_key_tofile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto failure;
	}

	if (dst_key_alg(key) != DST_ALG_DH)
		use(key, mctx, exp_result, nfails);

	/*
	 * Skip the rest of this test if we weren't expecting
	 * the read to be successful.
	 */
	if (exp_result != ISC_R_SUCCESS)
		goto cleanup;

	dst_key_setttl(key, 3600);
	ret = dst_key_tofile(key, type, tmp);
	if (ret != 0) {
		t_info("dst_key_tofile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto failure;
	}

	/* Reread key to confirm TTL was changed */
	dst_key_free(&key);
	ret = dst_key_fromfile(name, id, alg, type, tmp, mctx, &key);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_key_fromfile(%d) returned: %s\n",
		       alg, dst_result_totext(ret));
		++*nfails;
		goto failure;
	}

	if (dst_key_getttl(key) != 3600) {
		t_info("modified key TTL incorrect\n");
		++*nfails;
		goto failure;
	}

 cleanup:
	cleandir(tmp);

 failure:
	dst_key_free(&key);
}

static void
generate(int alg, isc_mem_t *mctx, int size, int *nfails) {
	isc_result_t ret;
	dst_key_t *key = NULL;

	ret = dst_key_generate(dns_rootname, alg, size, 0, 0, 0,
			       dns_rdataclass_in, mctx, &key);
	if (ret != ISC_R_SUCCESS) {
		t_info("dst_key_generate(%d) returned: %s\n", alg,
		       dst_result_totext(ret));
		++*nfails;
		goto cleanup;
	}

	if (alg != DST_ALG_DH)
		use(key, mctx, ISC_R_SUCCESS, nfails);
 cleanup:
	if (key != NULL)
		dst_key_free(&key);
}

#define	DBUFSIZ	25

static const char *a1 =
		"the dst module provides the capability to "
		"generate, store and retrieve public and private keys, "
		"sign and verify data using the RSA, DSA and MD5 algorithms, "
		"and compute Diffie-Hellman shared secrets.";
static void
t1(void) {
	isc_mem_t	*mctx;
	isc_entropy_t	*ectx;
	int		nfails;
	int		nprobs;
	int		result;
	isc_result_t	isc_result;
	dns_fixedname_t	fname;
	dns_name_t	*name;
	isc_buffer_t	b;

	t_assert("dst", 1, T_REQUIRED, "%s", a1);

	nfails = 0;
	nprobs = 0;
	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}
	ectx = NULL;
	isc_result = isc_entropy_create(mctx, &ectx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_entropy_create failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}
	isc_result = isc_entropy_createfilesource(ectx, "randomfile");
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_entropy_create failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}
	isc_result = dst_lib_init(mctx, ectx, ISC_ENTROPY_BLOCKING);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_lib_init failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}

	if (!dst_algorithm_supported(DST_ALG_RSAMD5)) {
		dst_lib_destroy();
		t_info("library built without crypto support\n");
		t_result(T_SKIPPED);
		return;
	}

	t_info("testing use of stored keys [1]\n");

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_constinit(&b, "test.", 5);
	isc_buffer_add(&b, 5);
	isc_result = dns_name_fromtext(name, &b, NULL, 0, NULL);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}
	io(name, 23616, DST_ALG_DSA, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
			mctx, ISC_R_SUCCESS, &nfails, &nprobs);
	t_info("testing use of stored keys [2]\n");
	io(name, 54622, DST_ALG_RSAMD5, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
			mctx, ISC_R_SUCCESS, &nfails, &nprobs);

	t_info("testing use of stored keys [3]\n");
	io(name, 49667, DST_ALG_DSA, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
			mctx, DST_R_NULLKEY, &nfails, &nprobs);
	t_info("testing use of stored keys [4]\n");
	io(name, 2, DST_ALG_RSAMD5, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
			mctx, DST_R_NULLKEY, &nfails, &nprobs);

	isc_buffer_constinit(&b, "dh.", 3);
	isc_buffer_add(&b, 3);
	isc_result = dns_name_fromtext(name, &b, NULL, 0, NULL);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
		       isc_result_totext(isc_result));
		t_result(T_UNRESOLVED);
		return;
	}

	dh(name, 18602, name, 48957, mctx, ISC_R_SUCCESS, &nfails, &nprobs);

	t_info("testing use of generated keys\n");
	generate(DST_ALG_RSAMD5, mctx, 512, &nfails);
	generate(DST_ALG_DSA, mctx, 512, &nfails);
	generate(DST_ALG_DH, mctx, 512, &nfails);
	/*
	 * This one uses a constant.
	 */
	generate(DST_ALG_DH, mctx, 768, &nfails);
	generate(DST_ALG_HMACMD5, mctx, 512, &nfails);

	dst_lib_destroy();

	isc_entropy_detach(&ectx);

	isc_mem_destroy(&mctx);

	result = T_UNRESOLVED;
	if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;
	else if (nfails)
		result = T_FAIL;
	t_result(result);

}

#define	T_SIGMAX	512

#undef	NEWSIG	/* Define NEWSIG to generate the original signature file. */

#ifdef	NEWSIG

/*
 * Write a sig in buf to file at path.
 */
static int
sig_tofile(char *path, isc_buffer_t *buf) {
	int		rval;
	int		fd;
	int		len;
	int		nprobs;
	int		cnt;
	unsigned char	c;
	unsigned char	val;

	cnt = 0;
	nprobs = 0;
	len = buf->used - buf->current;

	t_info("buf: current %d used %d len %d\n",
	       buf->current, buf->used, len);

	fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, S_IRWXU|S_IRWXO|S_IRWXG);
	if (fd < 0) {
		t_info("open %s failed %d\n", path, errno);
		return(1);
	}

	while (len) {
		c = (unsigned char) isc_buffer_getuint8(buf);
		val = ((c >> 4 ) & 0x0f);
		if ((0 <= val) && (val <= 9))
			val = '0' + val;
		else
			val = 'A' + val - 10;
		rval = write(fd, &val, 1);
		if (rval != 1) {
			++nprobs;
			t_info("write failed %d %d\n", rval, errno);
			break;
		}
		val = (c & 0x0f);
		if ((0 <= val) && (val <= 9))
			val = '0' + val;
		else
			val = 'A' + val - 10;
		rval = write(fd, &val, 1);
		if (rval != 1) {
			++nprobs;
			t_info("write failed %d %d\n", rval, errno);
			break;
		}
		--len;
		++cnt;
		if ((cnt % 16) == 0) {
			val = '\n';
			rval = write(fd, &val, 1);
			if (rval != 1) {
				++nprobs;
				t_info("write failed %d %d\n", rval, errno);
				break;
			}
		}
	}
	val = '\n';
	rval = write(fd, &val, 1);
	if (rval != 1) {
		++nprobs;
		t_info("write failed %d %d\n", rval, errno);
	}
	(void) close(fd);
	return(nprobs);
}

#endif	/* NEWSIG */

/*
 * Read sig in file at path to buf.
 */
static int
sig_fromfile(char *path, isc_buffer_t *iscbuf) {
	size_t		rval;
	size_t		len;
	FILE		*fp;
	unsigned char	val;
	char		*p;
	char		*buf;
	isc_result_t	isc_result;
	off_t		size;

	isc_result = isc_stdio_open(path, "rb", &fp);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("open failed, result: %s\n",
		       isc_result_totext(isc_result));
		return(1);
	}

	isc_result = isc_file_getsizefd(fileno(fp), &size);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("stat %s failed, result: %s\n",
		       path, isc_result_totext(isc_result));
		isc_stdio_close(fp);
		return(1);
	}

	buf = (char *) malloc((size + 1) * sizeof(char));
	if (buf == NULL) {
		t_info("malloc failed, errno == %d\n", errno);
		isc_stdio_close(fp);
		return(1);
	}

	len = (size_t)size;
	p = buf;
	while (len != 0U) {
		isc_result = isc_stdio_read(p, 1, len, fp, &rval);
		if (isc_result == ISC_R_SUCCESS) {
			len -= rval;
			p += rval;
		} else {
			t_info("read failed %d, result: %s\n",
			       (int)rval, isc_result_totext(isc_result));
			(void) free(buf);
			(void) isc_stdio_close(fp);
			return(1);
		}
	}
	isc_stdio_close(fp);

	p = buf;
	len = size;
	while (len > 0U) {
		if ((*p == '\r') || (*p == '\n')) {
			++p;
			--len;
			continue;
		} else if (len < 2U)
		       goto err;
		if (('0' <= *p) && (*p <= '9'))
			val = *p - '0';
		else if (('A' <= *p) && (*p <= 'F'))
			val = *p - 'A' + 10;
		else
			goto err;
		++p;
		val <<= 4;
		--len;
		if (('0' <= *p) && (*p <= '9'))
			val |= (*p - '0');
		else if (('A' <= *p) && (*p <= 'F'))
			val |= (*p - 'A' + 10);
		else
			goto err;
		++p;
		--len;
		isc_buffer_putuint8(iscbuf, val);
	}
	(void) free(buf);
	return(0);

 err:
	(void) free(buf);
	return (1);
}

static void
t2_sigchk(char *datapath, char *sigpath, char *keyname,
		int id, int alg, int type,
		isc_mem_t *mctx, char *expected_result,
		int *nfails, int *nprobs)
{
	size_t		rval;
	size_t		len;
	FILE		*fp;
	int		exp_res;
	dst_key_t	*key = NULL;
	unsigned char	sig[T_SIGMAX];
	unsigned char	*p;
	unsigned char	*data;
	off_t		size;
	isc_result_t	isc_result;
	isc_buffer_t	databuf;
	isc_buffer_t	sigbuf;
	isc_region_t	datareg;
	isc_region_t	sigreg;
	dns_fixedname_t	fname;
	dns_name_t	*name;
	isc_buffer_t	b;
	dst_context_t	*ctx = NULL;

	/*
	 * Read data from file in a form usable by dst_verify.
	 */
	isc_result = isc_stdio_open(datapath, "rb", &fp);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("t2_sigchk: open failed %s\n",
		       isc_result_totext(isc_result));
		++*nprobs;
		return;
	}

	isc_result = isc_file_getsizefd(fileno(fp), &size);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("t2_sigchk: stat (%s) failed %s\n",
		       datapath, isc_result_totext(isc_result));
		++*nprobs;
		isc_stdio_close(fp);
		return;
	}

	data = (unsigned char *) malloc(size * sizeof(unsigned char));
	if (data == NULL) {
		t_info("t2_sigchk: malloc failed %d\n", errno);
		++*nprobs;
		isc_stdio_close(fp);
		return;
	}

	p = data;
	len = (size_t)size;
	do {
		isc_result = isc_stdio_read(p, 1, len, fp, &rval);
		if (isc_result == ISC_R_SUCCESS) {
			len -= rval;
			p += rval;
		}
	} while (len);
	(void) isc_stdio_close(fp);

	/*
	 * Read key from file in a form usable by dst_verify.
	 */
	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_constinit(&b, keyname, strlen(keyname));
	isc_buffer_add(&b, strlen(keyname));
	isc_result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dns_name_fromtext failed %s\n",
			isc_result_totext(isc_result));
		(void) free(data);
		++*nprobs;
		return;
	}
	isc_result = dst_key_fromfile(name, id, alg, type, NULL, mctx, &key);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_key_fromfile failed %s\n",
			isc_result_totext(isc_result));
		(void) free(data);
		++*nprobs;
		return;
	}

	isc_buffer_init(&databuf, data, (unsigned int)size);
	isc_buffer_add(&databuf, (unsigned int)size);
	isc_buffer_usedregion(&databuf, &datareg);

#ifdef	NEWSIG

	/*
	 * If we're generating a signature for the first time,
	 * sign the data and save the signature to a file
	 */

	memset(sig, 0, sizeof(sig));
	isc_buffer_init(&sigbuf, sig, sizeof(sig));

	isc_result = dst_context_create3(key, mctx,
					 DNS_LOGCATEGORY_GENERAL,
					 ISC_TRUE, &ctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_context_create(%d) failed %s\n",
		       dst_result_totext(isc_result));
		(void) free(data);
		dst_key_free(&key);
		++*nprobs;
		return;
	}
	isc_result = dst_context_adddata(ctx, &datareg);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_context_adddata(%d) failed %s\n",
		       dst_result_totext(isc_result));
		(void) free(data);
		dst_key_free(&key);
		dst_context_destroy(&ctx);
		++*nprobs;
		return;
	}
	isc_result = dst_context_sign(ctx, &sigbuf);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_sign(%d) failed %s\n",
		       dst_result_totext(isc_result));
		(void) free(data);
		dst_key_free(&key);
		dst_context_destroy(&ctx);
		++*nprobs;
		return;
	}
	dst_context_destroy(&ctx);

	rval = sig_tofile(sigpath, &sigbuf);
	if (rval != 0) {
		t_info("sig_tofile failed\n");
		++*nprobs;
		(void) free(data);
		dst_key_free(&key);
		return;
	}

#endif	/* NEWSIG */

	memset(sig, 0, sizeof(sig));
	isc_buffer_init(&sigbuf, sig, sizeof(sig));

	/*
	 * Read precomputed signature from file in a form usable by dst_verify.
	 */
	rval = sig_fromfile(sigpath, &sigbuf);
	if (rval != 0U) {
		t_info("sig_fromfile failed\n");
		(void) free(data);
		dst_key_free(&key);
		++*nprobs;
		return;
	}

	/*
	 * Verify that the key signed the data.
	 */
	isc_buffer_remainingregion(&sigbuf, &sigreg);

	exp_res = 0;
	if (strstr(expected_result, "!"))
		exp_res = 1;

	isc_result = dst_context_create3(key, mctx,
					 DNS_LOGCATEGORY_GENERAL,
					 ISC_FALSE, &ctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_context_create returned %s\n",
			isc_result_totext(isc_result));
		(void) free(data);
		dst_key_free(&key);
		++*nfails;
		return;
	}
	isc_result = dst_context_adddata(ctx, &datareg);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_context_adddata returned %s\n",
			isc_result_totext(isc_result));
		(void) free(data);
		dst_context_destroy(&ctx);
		dst_key_free(&key);
		++*nfails;
		return;
	}
	isc_result = dst_context_verify(ctx, &sigreg);
	if (	((exp_res == 0) && (isc_result != ISC_R_SUCCESS))	||
		((exp_res != 0) && (isc_result == ISC_R_SUCCESS)))	{

		t_info("dst_context_verify returned %s, expected %s\n",
			isc_result_totext(isc_result),
			expected_result);
		++*nfails;
	}

	(void) free(data);
	dst_context_destroy(&ctx);
	dst_key_free(&key);
	return;
}

/*
 * The astute observer will note that t1() signs then verifies data
 * during the test but that t2() verifies data that has been
 * signed at some earlier time, possibly with an entire different
 * version or implementation of the DSA and RSA algorithms
 */
static const char *a2 =
		"the dst module provides the capability to "
		"verify data signed with the RSA and DSA algorithms";

/*
 * av ==  datafile, sigpath, keyname, keyid, alg, exp_result.
 */
static int
t2_vfy(char **av) {
	char		*datapath;
	char		*sigpath;
	char		*keyname;
	char		*key;
	int		keyid;
	char		*alg;
	int		algid;
	char		*exp_result;
	int		nfails;
	int		nprobs;
	isc_mem_t	*mctx;
	isc_entropy_t	*ectx;
	isc_result_t	isc_result;
	int		result;

	datapath	= *av++;
	sigpath		= *av++;
	keyname		= *av++;
	key		= *av++;
	keyid		= atoi(key);
	alg		= *av++;
	exp_result	= *av++;
	nfails		= 0;
	nprobs		= 0;

	if (! strcasecmp(alg, "DST_ALG_DSA"))
		algid = DST_ALG_DSA;
	else if (! strcasecmp(alg, "DST_ALG_RSAMD5"))
		algid = DST_ALG_RSAMD5;
	else {
		t_info("Unknown algorithm %s\n", alg);
		return(T_UNRESOLVED);
	}

	mctx = NULL;
	isc_result = isc_mem_create(0, 0, &mctx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_mem_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}
	ectx = NULL;
	isc_result = isc_entropy_create(mctx, &ectx);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_entropy_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}
	isc_result = isc_entropy_createfilesource(ectx, "randomfile");
	if (isc_result != ISC_R_SUCCESS) {
		t_info("isc_entropy_create failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}
	isc_result = dst_lib_init(mctx, ectx, ISC_ENTROPY_BLOCKING);
	if (isc_result != ISC_R_SUCCESS) {
		t_info("dst_lib_init failed %s\n",
		       isc_result_totext(isc_result));
		return(T_UNRESOLVED);
	}

	if (!dst_algorithm_supported(DST_ALG_RSAMD5)) {
		dst_lib_destroy();
		t_info("library built without crypto support\n");
		return (T_SKIPPED);
	}

	t_info("testing %s, %s, %s, %s, %s, %s\n",
			datapath, sigpath, keyname, key, alg, exp_result);
	t2_sigchk(datapath, sigpath, keyname, keyid,
			algid, DST_TYPE_PRIVATE|DST_TYPE_PUBLIC,
			mctx, exp_result,
			&nfails, &nprobs);

	dst_lib_destroy();

	isc_entropy_detach(&ectx);

	isc_mem_destroy(&mctx);

	result = T_UNRESOLVED;
	if (nfails)
		result = T_FAIL;
	else if ((nfails == 0) && (nprobs == 0))
		result = T_PASS;

	return(result);
}

static void
t2(void) {
	int	result;
	t_assert("dst", 2, T_REQUIRED, "%s", a2);
	result = t_eval("dst_2_data", t2_vfy, 6);
	t_result(result);
}

testspec_t	T_testlist[] = {
	{	(PFV) t1,	"basic dst module verification"	},
	{	(PFV) t2,	"signature ineffability"	},
	{	(PFV) 0,	NULL				}
};

#ifdef WIN32
int
main(int argc, char **argv) {
	t_settests(T_testlist);
	return (t_main(argc, argv));
}
#endif

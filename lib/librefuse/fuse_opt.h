/*	$NetBSD: fuse_opt.h,v 1.5 2009/04/19 22:25:29 christos Exp $	*/

/*
 * Copyright (c) 2007 Alistair Crooks.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FUSE_OPT_H_
#define _FUSE_OPT_H_

#ifdef __cplusplus
extern "C" {
#endif  

enum {
	FUSE_OPT_KEY_OPT = -1,
	FUSE_OPT_KEY_NONOPT = -2,
	FUSE_OPT_KEY_KEEP = -3,
	FUSE_OPT_KEY_DISCARD = -4
};

struct fuse_opt {
	const char	*templ;
	int32_t		offset;
	int32_t		value;
};

#define FUSE_OPT_KEY(templ, key) { templ, -1U, key }
#define FUSE_OPT_END { .templ = NULL }

typedef int (*fuse_opt_proc_t)(void *, const char *, int, struct fuse_args *);


int fuse_opt_add_arg(struct fuse_args *, const char *);
struct fuse_args *fuse_opt_deep_copy_args(int, char **);
void fuse_opt_free_args(struct fuse_args *);
int fuse_opt_insert_arg(struct fuse_args *, int, const char *);
int fuse_opt_add_opt(char **, const char *);
int fuse_opt_parse(struct fuse_args *, void *,
		   const struct fuse_opt *, fuse_opt_proc_t);
int fuse_opt_match(const struct fuse_opt *, const char *);

#ifdef __cplusplus
}
#endif 

#endif /* _FUSE_OPT_H_ */

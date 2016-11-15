/*	$NetBSD: firmload.c,v 1.21 2015/01/04 19:25:32 pooka Exp $	*/

/*-
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: firmload.c,v 1.21 2015/01/04 19:25:32 pooka Exp $");

/*
 * The firmload API provides an interface for device drivers to access
 * firmware images that must be loaded onto their devices.
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kmem.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/lwp.h>

#include <dev/firmload.h>

struct firmware_handle {
	struct vnode	*fh_vp;
	off_t		 fh_size;
};

static firmware_handle_t
firmware_handle_alloc(void)
{

	return (kmem_alloc(sizeof(struct firmware_handle), KM_SLEEP));
}

static void
firmware_handle_free(firmware_handle_t fh)
{

	kmem_free(fh, sizeof(*fh));
}

#if !defined(FIRMWARE_PATHS)
#define	FIRMWARE_PATHS		\
	"/libdata/firmware:/usr/libdata/firmware:/usr/pkg/libdata/firmware:/usr/pkg/libdata"
#endif

static char firmware_paths[PATH_MAX+1] = FIRMWARE_PATHS;

static int
sysctl_hw_firmware_path(SYSCTLFN_ARGS)
{
	int error, i;
	char newpath[PATH_MAX+1];
	struct sysctlnode node;
	char expected_char;

	node = *rnode;
	node.sysctl_data = &newpath[0];
	memcpy(node.sysctl_data, rnode->sysctl_data, PATH_MAX+1);
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);
	
	/*
	 * Make sure that all of the paths in the new path list are
	 * absolute.
	 *
	 * When sysctl_lookup() deals with a string, it's guaranteed
	 * to come back nul-terminated.
	 */
	expected_char = '/';
	for (i = 0; i < PATH_MAX+1; i++) {
		if (expected_char != 0 && newpath[i] != expected_char)
		    	return (EINVAL);
		if (newpath[i] == '\0')
			break;
		else if (newpath[i] == ':')
			expected_char = '/';
		else
			expected_char = 0;
	}

	memcpy(rnode->sysctl_data, node.sysctl_data, PATH_MAX+1);

	return (0);
}

SYSCTL_SETUP(sysctl_hw_firmware_setup, "sysctl hw.firmware subtree setup")
{
	const struct sysctlnode *firmware_node;
	
	if (sysctl_createv(clog, 0, NULL, &firmware_node,
	    CTLFLAG_PERMANENT,
	    CTLTYPE_NODE, "firmware", NULL,
	    NULL, 0, NULL, 0,
	    CTL_HW, CTL_CREATE, CTL_EOL) != 0)
	    	return;

	sysctl_createv(clog, 0, NULL, NULL,
	    CTLFLAG_READWRITE,
	    CTLTYPE_STRING, "path",
	    SYSCTL_DESCR("Device firmware loading path list"),
	    sysctl_hw_firmware_path, 0, firmware_paths, PATH_MAX+1,
	    CTL_HW, firmware_node->sysctl_num, CTL_CREATE, CTL_EOL);
}

static char *
firmware_path_next(const char *drvname, const char *imgname, char *pnbuf,
    char **prefixp)
{
	char *prefix = *prefixp;
	size_t maxprefix, i;

	if (prefix == NULL		/* terminated early */
	    || *prefix == '\0'		/* no more left */
	    || *prefix != '/') {	/* not absolute */
		*prefixp = NULL;
	    	return (NULL);
	}

	/*
	 * Compute the max path prefix based on the length of the provided
	 * names.
	 */
	maxprefix = MAXPATHLEN -
		(1 /* / */
		 + strlen(drvname)
		 + 1 /* / */
		 + strlen(imgname)
		 + 1 /* terminating NUL */);

	/* Check for underflow (size_t is unsigned). */
	if (maxprefix > MAXPATHLEN) {
		*prefixp = NULL;
		return (NULL);
	}

	for (i = 0; i < maxprefix; i++) {
		if (*prefix == ':' || *prefix == '\0')
			break;
		pnbuf[i] = *prefix++;
	}

	if (*prefix != ':' && *prefix != '\0') {
		/* Path prefix was too long. */
		*prefixp = NULL;
		return (NULL);
	}

	if (*prefix != '\0')
		prefix++;
	*prefixp = prefix;

	KASSERT(MAXPATHLEN >= i);
	snprintf(pnbuf + i, MAXPATHLEN - i, "/%s/%s", drvname, imgname);

	return (pnbuf);
}

static char *
firmware_path_first(const char *drvname, const char *imgname, char *pnbuf,
    char **prefixp)
{

	*prefixp = firmware_paths;
	return (firmware_path_next(drvname, imgname, pnbuf, prefixp));
}

/*
 * firmware_open:
 *
 *	Open a firmware image and return its handle.
 */
int
firmware_open(const char *drvname, const char *imgname, firmware_handle_t *fhp)
{
	struct pathbuf *pb;
	struct nameidata nd;
	struct vattr va;
	char *pnbuf, *path, *prefix;
	firmware_handle_t fh;
	struct vnode *vp;
	int error;
	extern struct cwdinfo cwdi0;

	if (drvname == NULL || imgname == NULL)
		return (EINVAL);

	if (cwdi0.cwdi_cdir == NULL) {
		printf("firmware_open(%s/%s) called too early.\n",
			drvname, imgname);
		return ENOENT;
	}

	pnbuf = PNBUF_GET();
	KASSERT(pnbuf != NULL);

	fh = firmware_handle_alloc();
	KASSERT(fh != NULL);

	error = 0;
	for (path = firmware_path_first(drvname, imgname, pnbuf, &prefix);
	     path != NULL;
	     path = firmware_path_next(drvname, imgname, pnbuf, &prefix)) {
		pb = pathbuf_create(path);
		if (pb == NULL) {
			error = ENOMEM;
			break;
		}
		NDINIT(&nd, LOOKUP, FOLLOW | NOCHROOT, pb);
		error = vn_open(&nd, FREAD, 0);
		pathbuf_destroy(pb);
		if (error == ENOENT) {
			continue;
		}
		break;
	}

	PNBUF_PUT(pnbuf);
	if (error) {
		firmware_handle_free(fh);
		return (error);
	}

	vp = nd.ni_vp;

	error = VOP_GETATTR(vp, &va, kauth_cred_get());
	if (error) {
		VOP_UNLOCK(vp);
		(void)vn_close(vp, FREAD, kauth_cred_get());
		firmware_handle_free(fh);
		return (error);
	}

	if (va.va_type != VREG) {
		VOP_UNLOCK(vp);
		(void)vn_close(vp, FREAD, kauth_cred_get());
		firmware_handle_free(fh);
		return (EINVAL);
	}

	/* XXX Mark as busy text file. */

	fh->fh_vp = vp;
	fh->fh_size = va.va_size;

	VOP_UNLOCK(vp);

	*fhp = fh;
	return (0);
}

/*
 * firmware_close:
 *
 *	Close a firmware image.
 */
int
firmware_close(firmware_handle_t fh)
{
	int error;

	error = vn_close(fh->fh_vp, FREAD, kauth_cred_get());
	firmware_handle_free(fh);
	return (error);
}

/*
 * firmware_get_size:
 *
 *	Return the total size of a firmware image.
 */
off_t
firmware_get_size(firmware_handle_t fh)
{

	return (fh->fh_size);
}

/*
 * firmware_read:
 *
 *	Read data from a firmware image at the specified offset into
 *	the provided buffer.
 */
int
firmware_read(firmware_handle_t fh, off_t offset, void *buf, size_t len)
{

	return (vn_rdwr(UIO_READ, fh->fh_vp, buf, len, offset,
			UIO_SYSSPACE, 0, kauth_cred_get(), NULL, curlwp));
}

/*
 * firmware_malloc:
 *
 *	Allocate a firmware buffer of the specified size.
 *
 *	NOTE: This routine may block.
 */
void *
firmware_malloc(size_t size)
{

	return (kmem_alloc(size, KM_SLEEP));
}

/*
 * firmware_free:
 *
 *	Free a previously allocated firmware buffer.
 */
/*ARGSUSED*/
void
firmware_free(void *v, size_t size)
{

	kmem_free(v, size);
}

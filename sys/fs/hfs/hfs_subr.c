/*	$NetBSD: hfs_subr.c,v 1.19 2015/06/21 13:43:58 maxv Exp $	*/

/*-
 * Copyright (c) 2005, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Yevgeny Binder and Dieter Baron.
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
__KERNEL_RCSID(0, "$NetBSD: hfs_subr.c,v 1.19 2015/06/21 13:43:58 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/mount.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/buf.h>

#include <fs/hfs/hfs.h>

#include <miscfs/specfs/specdev.h>

/*
 * Initialize the vnode associated with a new hfsnode.
 */
void
hfs_vinit(struct mount *mp, int (**specops)(void *), int (**fifoops)(void *),
	   struct vnode **vpp)
{
	struct hfsnode	*hp;
	struct vnode	*vp;

	vp = *vpp;
	hp = VTOH(vp);

	vp->v_type = hfs_catalog_keyed_record_vtype(
		(hfs_catalog_keyed_record_t *)&hp->h_rec);

	switch(vp->v_type) {
		case VCHR:
		case VBLK:
			vp->v_op = specops;
			spec_node_init(vp,
			    HFS_CONVERT_RDEV(hp->h_rec.file.bsd.special.raw_device));
			break;
		case VFIFO:
			vp->v_op = fifoops;
			break;

		case VNON:
		case VBAD:
		case VSOCK:
		case VDIR:
		case VREG:
		case VLNK:
			break;
	}

	if (hp->h_rec.u.cnid == HFS_CNID_ROOT_FOLDER)
		vp->v_vflag |= VV_ROOT;

	*vpp = vp;
}

/*
 * Callbacks for libhfs
 */

void
hfs_libcb_error(
	const char* format,
	const char* file,
	int line,
	va_list args)
{
#ifdef HFS_DEBUG
	if (file != NULL)
		printf("%s:%i: ", file, line);
	else
		printf("hfs: ");
#else
	printf("hfs: ");
#endif

	/* XXX Should we really display this if debugging is off? */
	vprintf(format, args);
	printf("\n");
}

/* XXX change malloc/realloc/free to use pools */

void*
hfs_libcb_malloc(size_t size, hfs_callback_args* cbargs)
{
	return malloc(size, /*M_HFSMNT*/ M_TEMP, M_WAITOK);
}

void*
hfs_libcb_realloc(void* ptr, size_t size, hfs_callback_args* cbargs)
{
	return realloc(ptr, size, /*M_HFSMNT*/ M_TEMP, M_WAITOK);
}

void
hfs_libcb_free(void* ptr, hfs_callback_args* cbargs)
{
	free(ptr, /*M_HFSMNT*/ M_TEMP);
}

/*
 * hfs_libcb_opendev()
 *
 * hfslib uses this callback to open a volume's device node by name. However,
 * by the time this is called here, the device node has already been opened by
 * VFS. So we are passed the vnode to this volume's block device and use that
 * instead of the device's name.
 */
int
hfs_libcb_opendev(
	hfs_volume* vol,
	const char* devname,
	hfs_callback_args* cbargs)
{
	hfs_libcb_data* cbdata = NULL;
	hfs_libcb_argsopen* args;
	int result, mode;
	uint64_t psize;
	unsigned secsize;

	result = 0;
	args = (hfs_libcb_argsopen*)(cbargs->openvol);

	if (vol == NULL || devname == NULL) {
		result = EINVAL;
		goto error;
	}

	cbdata = malloc(sizeof(hfs_libcb_data), M_HFSMNT, M_WAITOK);
	if (cbdata == NULL) {
		result = ENOMEM;
		goto error;
	}
	vol->cbdata = cbdata;

	cbdata->devvp = NULL;

	/* Open the device node. */
	mode = vol->readonly ? FREAD : FREAD|FWRITE;
	vn_lock(args->devvp, LK_EXCLUSIVE | LK_RETRY);
	result = VOP_OPEN(args->devvp, mode, FSCRED);
	VOP_UNLOCK(args->devvp);
	if (result != 0)
		goto error;

	/* Flush out any old buffers remaining from a previous use. */
	vn_lock(args->devvp, LK_EXCLUSIVE | LK_RETRY);
	result = vinvalbuf(args->devvp, V_SAVE, args->cred, args->l, 0, 0);
	VOP_UNLOCK(args->devvp);
	if (result != 0) {
		VOP_CLOSE(args->devvp, mode, FSCRED);
		goto error;
	}

	cbdata->devvp = args->devvp;

	/* Determine the device's block size. Default to DEV_BSIZE if unavailable.*/
	if (getdisksize(args->devvp, &psize, &secsize) != 0)
		cbdata->devblksz = DEV_BSIZE;
	else
		cbdata->devblksz = secsize;

	return 0;

error:
	if (cbdata != NULL) {
		if (cbdata->devvp != NULL) {
			vn_lock(cbdata->devvp, LK_EXCLUSIVE | LK_RETRY);
			(void)VOP_CLOSE(cbdata->devvp, vol->readonly ? FREAD :
				FREAD | FWRITE, NOCRED);
			VOP_UNLOCK(cbdata->devvp);
		}
		free(cbdata, M_HFSMNT);
		vol->cbdata = NULL;
	}

	return result;
}

void
hfs_libcb_closedev(hfs_volume* in_vol, hfs_callback_args* cbargs)
{
	struct vnode *devvp;

	if (in_vol == NULL)
		return;

	if (in_vol->cbdata != NULL) {
		devvp = ((hfs_libcb_data*)in_vol->cbdata)->devvp;
		if (devvp != NULL) {
			vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
			(void)VOP_CLOSE(devvp,
			    in_vol->readonly ? FREAD : FREAD | FWRITE, NOCRED);
			VOP_UNLOCK(devvp);
		}

		free(in_vol->cbdata, M_HFSMNT);
		in_vol->cbdata = NULL;
	}
}

int
hfs_libcb_read(
	hfs_volume* vol,
	void* outbytes,
	uint64_t length,
	uint64_t offset,
	hfs_callback_args* cbargs)
{
	hfs_libcb_data *cbdata;
	hfs_libcb_argsread* argsread;
	kauth_cred_t cred;
	uint64_t physoffset; /* physical offset from start of device(?) */

	if (vol == NULL || outbytes == NULL)
		return -1;

	cbdata = (hfs_libcb_data*)vol->cbdata;

	if (cbargs != NULL
		&& (argsread = (hfs_libcb_argsread*)cbargs->read) != NULL
		&& argsread->cred != NULL)
		cred = argsread->cred;
	else
		cred = NOCRED;

	/*
	 * Since bread() only reads data in terms of integral blocks, it may have
	 * read some data before and/or after our desired offset & length. So when
	 * copying that data into the outgoing buffer, start at the actual desired
	 * offset and only copy the desired length.
	 */
	physoffset = offset + vol->offset;

	return hfs_pread(cbdata->devvp, outbytes, cbdata->devblksz, physoffset,
			length, cred);
}

/*
 * So it turns out that bread() is pretty shoddy. It not only requires the size
 * parameter to be an integral multiple of the device's block size, but also
 * requires the block number to be on a boundary of that same block size -- and
 * yet be given as an integral multiple of DEV_BSIZE! So after much toil and
 * bloodshed, hfs_pread() was written as a convenience (and a model of how sane
 * people take their bread()). Returns 0 on success.
 */
int
hfs_pread(struct vnode *vp, void *buf, size_t secsz, uint64_t off,
	uint64_t len, kauth_cred_t cred)
{
	struct buf *bp;
	uint64_t curoff; /* relative to 'start' variable */
	uint64_t start;
	int error;

	if (vp == NULL || buf == NULL)
		return EINVAL;

	if (len == 0)
		return 0;

	curoff = 0;
	error = 0;

/* align offset to highest preceding sector boundary */
#define ABSZ(x, bsz) (((x)/(bsz))*(bsz)) 

/* round size up to integral # of block sizes */
#define RBSZ(x, bsz) (((x) + (bsz) - 1) & ~((bsz) - 1))

	start = ABSZ(off, secsz);
	while (start + curoff < off + len)
	{
		bp = NULL;

		/* XXX  Does the algorithm always do what's intended here when 
		 * XXX  start != off? Need to test this. */

		error = bread(vp, (start + curoff) / DEV_BSIZE,/* no rounding involved*/
		   RBSZ(min(len - curoff + (off - start), MAXBSIZE), secsz),
		   0, &bp);

		if (error == 0)
			memcpy((uint8_t*)buf + curoff, (uint8_t*)bp->b_data +
				(off - start), min(len - curoff, MAXBSIZE - (off - start)));

		if (bp != NULL)
			brelse(bp, 0);
		if (error != 0)
			return error;

		curoff += MAXBSIZE;
	}
#undef ABSZ
#undef RBSZ

	return 0;
}

/* XXX Provide a routine to take a catalog record and return its proper BSD file
 * XXX or directory mode value */


/* Convert from HFS+ time representation to UNIX time since epoch. */
void
hfs_time_to_timespec(uint32_t hfstime, struct timespec *unixtime)
{
	/*
	 * HFS+ time is calculated in seconds since midnight, Jan 1st, 1904.
	 * struct timespec counts from midnight, Jan 1st, 1970. Thus, there is
	 * precisely a 66 year difference between them, which is equal to
	 * 2,082,844,800 seconds. No, I didn't count them by hand.
	 */

	if (hfstime < 2082844800)
		unixtime->tv_sec = 0; /* dates before 1970 are bs anyway, so use epoch*/
	else
		unixtime->tv_sec = hfstime - 2082844800;

	unixtime->tv_nsec = 0; /* we don't have nanosecond resolution */
}

/*
 * Endian conversion with automatic pointer incrementation.
 */
 
uint16_t be16tohp(void** inout_ptr)
{
	uint16_t	result;

	if (inout_ptr == NULL)
		return 0;

	memcpy(&result, *inout_ptr, sizeof(result));
	*inout_ptr = (char *)*inout_ptr + sizeof(result);
	return be16toh(result);
}

uint32_t be32tohp(void** inout_ptr)
{
	uint32_t	result;

	if (inout_ptr == NULL)
		return 0;

	memcpy(&result, *inout_ptr, sizeof(result));
	*inout_ptr = (char *)*inout_ptr + sizeof(result);
	return be32toh(result);
}

uint64_t be64tohp(void** inout_ptr)
{
	uint64_t	result;

	if (inout_ptr == NULL)
		return 0;

	memcpy(&result, *inout_ptr, sizeof(result));
	*inout_ptr = (char *)*inout_ptr + sizeof(result);
	return be64toh(result);
}

enum vtype
hfs_catalog_keyed_record_vtype(const hfs_catalog_keyed_record_t *rec)
{
	if (rec->type == HFS_REC_FILE) {
		uint32_t mode;

		mode = ((const hfs_file_record_t *)rec)->bsd.file_mode;
		if (mode != 0)
			return IFTOVT(mode);
		else
			return VREG;
	} else
		return VDIR;
}

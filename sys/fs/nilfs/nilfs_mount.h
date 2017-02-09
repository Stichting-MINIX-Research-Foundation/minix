/* $NetBSD: nilfs_mount.h,v 1.1 2009/07/18 16:31:42 reinoud Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#ifndef _FS_NILFS_NILFS_MOUNT_H_
#define _FS_NILFS_NILFS_MOUNT_H_

/*
 * Arguments to mount NILFS filingsystem.
 */

#define NILFSMNT_VERSION	1
struct nilfs_args {
	uint32_t	 version;	/* version of this structure         */
	char		*fspec;		/* mount specifier                   */
	uint32_t	 nilfsmflags;	/* mount options                     */

	int32_t		 gmtoff;	/* offset from UTC in seconds        */
	int64_t		 cpno;		/* checkpoint number                 */

	/* extendable */
	uint8_t	 reserved[32];
};


/* nilfs mount options */

#define NILFSMNT_BITS "\20"

#endif /* !_FS_NILFS_NILFS_MOUNT_H_ */


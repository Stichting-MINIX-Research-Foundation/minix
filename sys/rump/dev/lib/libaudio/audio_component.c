/*	$NetBSD: audio_component.c,v 1.2 2015/06/08 12:18:04 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: audio_component.c,v 1.2 2015/06/08 12:18:04 pooka Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/stat.h>

#include <dev/audio_if.h>

#include "ioconf.c"

#include "rump_private.h"
#include "rump_vfs_private.h"

RUMP_COMPONENT(RUMP_COMPONENT_DEV)
{
        extern const struct cdevsw audio_cdevsw;
	devmajor_t bmaj, cmaj;
	int error;

	config_init_component(cfdriver_ioconf_audio,
	    cfattach_ioconf_audio, cfdata_ioconf_audio);

	bmaj = cmaj = NODEVMAJOR;
	if ((error = devsw_attach("audio", NULL, &bmaj,
	    &audio_cdevsw, &cmaj)) != 0)
		panic("audio devsw attach failed: %d", error);
	if ((error = rump_vfs_makedevnodes(S_IFCHR, "/dev/audio", '0',
	    cmaj, AUDIO_DEVICE, 4)) !=0)
		panic("cannot create audio device nodes: %d", error);
	if ((error = rump_vfs_makesymlink("audio0", "/dev/audio")) != 0)
		panic("cannot create audio symlink: %d", error);
	if ((error = rump_vfs_makedevnodes(S_IFCHR, "/dev/sound", '0',
	    cmaj, SOUND_DEVICE, 4)) !=0)
		panic("cannot create sound device nodes: %d", error);
	if ((error = rump_vfs_makesymlink("sound0", "/dev/sound")) != 0)
		panic("cannot create sound symlink: %d", error);
	if ((error = rump_vfs_makedevnodes(S_IFCHR, "/dev/audioctl", '0',
	    cmaj, AUDIOCTL_DEVICE, 4)) !=0)
		panic("cannot create audioctl device nodes: %d", error);
	if ((error = rump_vfs_makesymlink("audioctl0", "/dev/audioctl")) != 0)
		panic("cannot create audioctl symlink: %d", error);
	if ((error = rump_vfs_makedevnodes(S_IFCHR, "/dev/mixer", '0',
	    cmaj, MIXER_DEVICE, 4)) !=0)
		panic("cannot create mixer device nodes: %d", error);
	if ((error = rump_vfs_makesymlink("mixer0", "/dev/mixer")) != 0)
		panic("cannot create mixer symlink: %d", error);
}

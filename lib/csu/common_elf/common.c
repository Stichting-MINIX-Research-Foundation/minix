/* $NetBSD: common.c,v 1.17 2005/12/24 22:53:15 perry Exp $ */

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

/*
 * NOT A STANDALONE FILE!
 */

static char *
_strrchr(char *p, int ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
	/* NOTREACHED */
}

#ifdef MCRT0
__asm ("  .text");
#ifdef EPROL_EXPORT
EPROL_EXPORT;
#endif
__asm ("_eprol:");
#endif

#ifdef DYNAMIC

void
_rtld_setup(void (*cleanup)(void), const Obj_Entry *obj)
{

	if ((obj == NULL) || (obj->magic != RTLD_MAGIC))
		_FATAL("Corrupt Obj_Entry pointer in GOT\n");
	if (obj->version != RTLD_VERSION)
		_FATAL("Dynamic linker version mismatch\n");

	atexit(cleanup);
}

#endif /* DYNAMIC */

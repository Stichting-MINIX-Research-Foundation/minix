/*****************************************************************
**
**	@(#) zone.h -- Header file for zone info
**
**	Copyright (c) Mar 2005, Holger Zuleger HZnet. All rights reserved.
**
**	This software is open source.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	Redistributions of source code must retain the above copyright notice,
**	this list of conditions and the following disclaimer.
**
**	Redistributions in binary form must reproduce the above copyright notice,
**	this list of conditions and the following disclaimer in the documentation
**	and/or other materials provided with the distribution.
**
**	Neither the name of Holger Zuleger HZnet nor the names of its contributors may
**	be used to endorse or promote products derived from this software without
**	specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
**	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
**	PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
**	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
**	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
**	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
**	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
**	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
**	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************/
#ifndef ZONE_H
# define ZONE_H

# include <sys/types.h>
# include <stdio.h>
# include <time.h>
# include "dki.h"

/* all we have to know about a zone */
typedef	struct	Zone {
	const	char	*zone;	/* domain name or label */
	const	char	*dir;	/* directory of zone data */
	const	char	*file;	/* file name (zone.db)  */
	const	char	*sfile;	/* file name of secured zone (zone.db.signed)  */
	const	zconf_t	*conf;	/* ptr to config */	/* TODO: Should this be only a ptr to a local config ? */
		dki_t	*keys;	/* ptr to keylist */
	struct	Zone	*next;		/* ptr to next entry in list */
} zone_t;

extern	void	zone_free (zone_t *zp);
extern	void	zone_freelist (zone_t **listp);
extern	zone_t	*zone_new (zone_t **zp, const char *zone, const char *dir, const char *file, const char *signed_ext, const zconf_t *cp);
extern	const	char	*zone_geterrstr ();
extern	zone_t	*zone_add (zone_t **list, zone_t *new);
extern	const zone_t	*zone_search (const zone_t *list, const char *name);
extern	int	zone_readdir (const char *dir, const char *zone, const char *zfile, zone_t **listp, const zconf_t *conf, int dyn_zone);
extern	const	char	*zone_geterrstr (void);
extern	int	zone_print (const char *mesg, const zone_t *z);

#endif

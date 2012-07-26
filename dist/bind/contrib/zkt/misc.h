/*****************************************************************
**
**	@(#) misc.h  (c) 2005 - 2007  Holger Zuleger  hznet.de
**
**	Copyright (c) 2005 - 2007, Holger Zuleger HZnet. All rights reserved.
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
#ifndef MISC_H
# define MISC_H
# include <sys/types.h>
# include <stdarg.h>
# include <stdio.h>
# include "zconf.h"

# define min(a, b)	((a) < (b) ? (a) : (b))
# define max(a, b)	((a) > (b) ? (a) : (b))

extern	const	char	*getnameappendix (const char *progname, const char *basename);
extern	const	char	*getdefconfname (const char *view);
extern	int	fileexist (const char *name);
extern	size_t	filesize (const char *name);
extern	int	file_age (const char *fname);
extern	int	touch (const char *fname, time_t sec);
extern	int	linkfile (const char *fromfile, const char *tofile);
//extern	int	copyfile (const char *fromfile, const char *tofile);
extern	int	copyfile (const char *fromfile, const char *tofile, const char *dnskeyfile);
extern	int	copyzonefile (const char *fromfile, const char *tofile, const char *dnskeyfile);
extern	int	cmpfile (const char *file1, const char *file2);
extern	char	*str_delspace (char *s);
#if 1
extern	char	*domain_canonicdup (const char *s);
#else
extern	char	*str_tolowerdup (const char *s);
#endif
extern	int	in_strarr (const char *str, char *const arr[], int cnt);
extern	const	char	*splitpath (char *path, size_t  size, const char *filename);
extern	char	*pathname (char *name, size_t size, const char *path, const char *file, const char *ext);
extern	char	*time2str (time_t sec, int precision);
extern	char	*time2isostr (time_t sec, int precision);
extern	time_t	timestr2time (const char *timestr);
extern	int	is_keyfilename (const char *name);
extern	int	is_directory (const char *name);
extern	time_t	file_mtime (const char *fname);
extern	int	is_exec_ok (const char *prog);
extern	char	*age2str (time_t sec);
extern	time_t	stop_timer (time_t start);
extern	time_t	start_timer (void);
extern	void    error (char *fmt, ...);
extern	void    fatal (char *fmt, ...);
extern	void    logmesg (char *fmt, ...);
extern	void	verbmesg (int verblvl, const zconf_t *conf, char *fmt, ...);
extern	void	logflush (void);
extern	int	gensalt (char *salt, size_t saltsize, int saltbits, unsigned int seed);
extern	char	*str_untaint (char *str);
extern	char	*str_chop (char *str, char c);
extern	int	is_dotfilename (const char *name);
extern	void	parseurl (char *url, char **proto, char **host, char **port, char **para);
#endif

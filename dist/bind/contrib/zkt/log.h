/*****************************************************************
**
**	@(#) log.h  (c) June 2008  Holger Zuleger  hznet.de
**
**	Copyright (c) June 2008, Holger Zuleger HZnet. All rights reserved.
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
#ifndef LOG_H
# define LOG_H
# include <sys/types.h>
# include <stdarg.h>
# include <stdio.h>
# include <time.h>
# include <syslog.h>

#ifndef LOG_FNAMETMPL
# define	LOG_FNAMETMPL	"/zkt-%04d-%02d-%02dT%02d%02d%02dZ+log"
#endif

#ifndef LOG_DOMAINTMPL
# define	LOG_DOMAINTMPL	"zktlog-%s"
#endif


typedef enum {
	LG_NONE = 0,
	LG_DEBUG,
	LG_INFO,
	LG_NOTICE,
	LG_WARNING,
	LG_ERROR,
	LG_FATAL
} lg_lvl_t;

extern	lg_lvl_t	lg_str2lvl (const char *name);
extern	int	lg_str2syslog (const char *facility);
extern	const	char	*lg_lvl2str (lg_lvl_t level);
extern	lg_lvl_t	lg_lvl2syslog (lg_lvl_t level);
extern	long	lg_geterrcnt (void);
extern	long	lg_seterrcnt (long value);
extern	long	lg_reseterrcnt (void);
extern	int	lg_open (const char *progname, const char *facility, const char *syslevel, const char *path, const char *file, const char *filelevel);
extern	int	lg_close (void);
extern	int	lg_zone_start (const char *dir, const char *domain);
extern	int	lg_zone_end (void);
extern	void	lg_args (lg_lvl_t level, int argc, char * const argv[]);
extern	void	lg_mesg (int level, char *fmt, ...);
#endif

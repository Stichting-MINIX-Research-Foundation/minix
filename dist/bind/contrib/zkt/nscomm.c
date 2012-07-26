/*****************************************************************
**
**	@(#) nscomm.c  (c) 2005 - 2009  Holger Zuleger  hznet.de
**
**	Copyright (c) 2005 - 2009, Holger Zuleger HZnet. All rights reserved.
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
# include <stdio.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "config_zkt.h"
#include "zconf.h"
#define extern
#include "nscomm.h"
#undef extern


/*****************************************************************
**	dyn_update_freeze ()
*****************************************************************/
int	dyn_update_freeze (const char *domain, const zconf_t *z, int freeze)
{
	char	cmdline[254+1];
	char	str[254+1];
	char	*action;
	FILE	*fp;

	assert (z != NULL);
	if ( freeze )
		action = "freeze";
	else
		action = "thaw";

	if ( z->view )
		snprintf (str, sizeof (str), "\"%s\" in view \"%s\"", domain, z->view);
	else
		snprintf (str, sizeof (str), "\"%s\"", domain);

	lg_mesg (LG_NOTICE, "%s: %s dynamic zone", str, action);
	verbmesg (1, z, "\t%s dynamic zone %s\n", action, str);

	if ( z->view )
		snprintf (cmdline, sizeof (cmdline), "%s %s %s IN %s", RELOADCMD, action, domain, z->view);
	else
		snprintf (cmdline, sizeof (cmdline), "%s %s %s", RELOADCMD, action, domain);

	verbmesg (2, z, "\t  Run cmd \"%s\"\n", cmdline);
	*str = '\0';
	if ( z->noexec == 0 )
	{
		if ( (fp = popen (cmdline, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
			return -1;
		pclose (fp);
	}

	verbmesg (2, z, "\t  rndc %s return: \"%s\"\n", action, str_chop (str, '\n'));

	return 0;
}

/*****************************************************************
**	distribute and reload a zone via "distribute_command"
**	what is
**		1 for zone distribution and relaod
**		2 for key distribution (used by dynamic zoes)
*****************************************************************/
int	dist_and_reload (const zone_t *zp, int what)
{
	char	path[MAX_PATHSIZE+1];
	char	cmdline[254+1];
	char	zone[254+1];
	char	str[254+1];
	char	*view;
	FILE	*fp;

	assert (zp != NULL);
	assert (zp->conf->dist_cmd != NULL);
	assert ( what == 1 || what == 2 );

	if ( zp->conf->dist_cmd == NULL )
		return 0;

	if ( !is_exec_ok (zp->conf->dist_cmd) )
	{
		char	*mesg;

		if ( getuid () == 0 )
			mesg = "\tDistribution command %s not run as root\n";
		else
			mesg = "\tDistribution command %s not run due to strange file mode settings\n";

		verbmesg (1, zp->conf, mesg, zp->conf->dist_cmd);
		lg_mesg (LG_ERROR, "exec of distribution command %s disabled due to security reasons", zp->conf->dist_cmd);

		return -1;
	}

	view = "";	/* default is an empty view string */
	if ( zp->conf->view )
	{
		snprintf (zone, sizeof (zone), "\"%s\" in view \"%s\"", zp->zone, zp->conf->view);
		view = zp->conf->view;
	}
	else
		snprintf (zone, sizeof (zone), "\"%s\"", zp->zone);


	if ( what == 2 )
	{
		lg_mesg (LG_NOTICE, "%s: key distribution triggered", zone);
		verbmesg (1, zp->conf, "\tDistribute keys for zone %s\n", zone);
		snprintf (cmdline, sizeof (cmdline), "%s distkeys %s %s %s",
					zp->conf->dist_cmd, zp->zone, path, view);
		*str = '\0';
		if ( zp->conf->noexec == 0 )
		{
			verbmesg (2, zp->conf, "\t  Run cmd \"%s\"\n", cmdline);
			if ( (fp = popen (cmdline, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
				return -2;
			pclose (fp);
			verbmesg (2, zp->conf, "\t  %s distribute return: \"%s\"\n", zp->conf->dist_cmd, str_chop (str, '\n'));
		}

		return 0;
	}

	pathname (path, sizeof (path), zp->dir, zp->sfile, NULL);

	lg_mesg (LG_NOTICE, "%s: distribution triggered", zone);
	verbmesg (1, zp->conf, "\tDistribute zone %s\n", zone);
	snprintf (cmdline, sizeof (cmdline), "%s distribute %s %s %s", zp->conf->dist_cmd, zp->zone, path, view);

	*str = '\0';
	if ( zp->conf->noexec == 0 )
	{
		verbmesg (2, zp->conf, "\t  Run cmd \"%s\"\n", cmdline);
		if ( (fp = popen (cmdline, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
			return -2;
		pclose (fp);
		verbmesg (2, zp->conf, "\t  %s distribute return: \"%s\"\n", zp->conf->dist_cmd, str_chop (str, '\n'));
	}


	lg_mesg (LG_NOTICE, "%s: reload triggered", zone);
	verbmesg (1, zp->conf, "\tReload zone %s\n", zone);
	snprintf (cmdline, sizeof (cmdline), "%s reload %s %s %s", zp->conf->dist_cmd, zp->zone, path, view);

	*str = '\0';
	if ( zp->conf->noexec == 0 )
	{
		verbmesg (2, zp->conf, "\t  Run cmd \"%s\"\n", cmdline);
		if ( (fp = popen (cmdline, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
			return -2;
		pclose (fp);
		verbmesg (2, zp->conf, "\t  %s reload return: \"%s\"\n", zp->conf->dist_cmd, str_chop (str, '\n'));
	}

	return 0;
}

/*****************************************************************
**	reload a zone via "rndc"
*****************************************************************/
int	reload_zone (const char *domain, const zconf_t *z)
{
	char	cmdline[254+1];
	char	str[254+1];
	FILE	*fp;

	assert (z != NULL);
	dbg_val3 ("reload_zone %d :%s: :%s:\n", z->verbosity, domain, z->view);
	if ( z->view )
		snprintf (str, sizeof (str), "\"%s\" in view \"%s\"", domain, z->view);
	else
		snprintf (str, sizeof (str), "\"%s\"", domain);

	lg_mesg (LG_NOTICE, "%s: reload triggered", str);
	verbmesg (1, z, "\tReload zone %s\n", str);

	if ( z->view )
		snprintf (cmdline, sizeof (cmdline), "%s reload %s IN %s", RELOADCMD, domain, z->view);
	else
		snprintf (cmdline, sizeof (cmdline), "%s reload %s", RELOADCMD, domain);

	*str = '\0';
	if ( z->noexec == 0 )
	{
		verbmesg (2, z, "\t  Run cmd \"%s\"\n", cmdline);
		if ( (fp = popen (cmdline, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
			return -1;
		pclose (fp);
		verbmesg (2, z, "\t  rndc reload return: \"%s\"\n", str_chop (str, '\n'));
	}

	return 0;
}

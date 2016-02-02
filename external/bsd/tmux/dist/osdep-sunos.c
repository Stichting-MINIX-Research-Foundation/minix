/* Id */

/*
 * Copyright (c) 2009 Todd Carson <toc@daybefore.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <event.h>
#include <fcntl.h>
#include <procfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

char *
osdep_get_name(int fd, char *tty)
{
	struct psinfo	 p;
	struct stat	 st;
	char		*path;
	ssize_t		 bytes;
	int		 f;
	pid_t		 pgrp;

	if ((f = open(tty, O_RDONLY)) < 0)
		return (NULL);

	if (fstat(f, &st) != 0 || ioctl(f, TIOCGPGRP, &pgrp) != 0) {
		close(f);
		return (NULL);
	}
	close(f);

	xasprintf(&path, "/proc/%u/psinfo", (u_int) pgrp);
	f = open(path, O_RDONLY);
	free(path);
	if (f < 0)
		return (NULL);

	bytes = read(f, &p, sizeof(p));
	close(f);
	if (bytes != sizeof(p))
		return (NULL);

	if (p.pr_ttydev != st.st_rdev)
		return (NULL);

	return (xstrdup(p.pr_fname));
}

char *
osdep_get_cwd(int fd)
{
	static char	 target[MAXPATHLEN + 1];
	char		*path;
	ssize_t		 n;
	pid_t		 pgrp;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	xasprintf(&path, "/proc/%u/path/cwd", (u_int) pgrp);
	n = readlink(path, target, MAXPATHLEN);
	free(path);
	if (n > 0) {
		target[n] = '\0';
		return (target);
	}
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	return (event_init());
}

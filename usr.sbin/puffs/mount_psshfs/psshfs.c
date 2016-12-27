/*	$NetBSD: psshfs.c,v 1.66 2012/11/04 22:46:08 christos Exp $	*/

/*
 * Copyright (c) 2006-2009  Antti Kantee.  All Rights Reserved.
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

/*
 * psshfs: puffs sshfs
 *
 * psshfs implements sshfs functionality on top of puffs making it
 * possible to mount a filesystme through the sftp service.
 *
 * psshfs can execute multiple operations in "parallel" by using the
 * puffs_cc framework for continuations.
 *
 * Concurrency control is handled currently by vnode locking (this
 * will change in the future).  Context switch locations are easy to
 * find by grepping for puffs_framebuf_enqueue_cc().
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: psshfs.c,v 1.66 2012/11/04 22:46:08 christos Exp $");
#endif /* !lint */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <stdio.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <mntopts.h>
#include <paths.h>
#include <poll.h>
#include <puffs.h>
#include <signal.h>
#include <stdlib.h>
#include <util.h>
#include <unistd.h>

#include "psshfs.h"

static int	pssh_connect(struct puffs_usermount *, int);
static void	psshfs_loopfn(struct puffs_usermount *);
__dead static void	usage(void);
static char *	cleanhostname(char *);
static char *	colon(char *);
static void	add_ssharg(char ***, int *, const char *);
static void	psshfs_notify(struct puffs_usermount *, int, int);

#define SSH_PATH "/usr/bin/ssh"

unsigned int max_reads;
static int sighup;

static char *
cleanhostname(char *host)
{
	if (*host == '[' && host[strlen(host) - 1] == ']') {
		host[strlen(host) - 1] = '\0';
		return (host + 1);
	} else
		return host;
}

static char *
colon(char *cp)
{
	int flag = 0;

	if (*cp == '[')
		flag = 1;

	for (; *cp; ++cp) {
		if (*cp == '@' && *(cp+1) == '[')
			flag = 1;
		if (*cp == ']' && *(cp+1) == ':' && flag)
			return (cp+1);
		if (*cp == ':' && !flag)
			return (cp);
		if (*cp == '/')
			return NULL;
	}
	return NULL;
}

static void
add_ssharg(char ***sshargs, int *nargs, const char *arg)
{
	
	*sshargs = realloc(*sshargs, (*nargs + 2) * sizeof(char*));
	if (!*sshargs)
		err(1, "realloc");
	(*sshargs)[(*nargs)++] = estrdup(arg);
	(*sshargs)[*nargs] = NULL;
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
	    "[-ceprst] [-F configfile] [-O sshopt=value] [-o opts] "
	    "user@host:path mountpath\n",
	    getprogname());
	exit(1);
}

static void
takehup(int sig)
{

	sighup = 1;
}

int
main(int argc, char *argv[])
{
	struct psshfs_ctx pctx;
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	struct psshfs_node *root = &pctx.psn_root;
	struct puffs_node *pn_root;
	puffs_framev_fdnotify_fn notfn;
	struct vattr *rva;
	mntoptparse_t mp;
	char **sshargs;
	char *user;
	char *host;
	char *path;
	int mntflags, pflags, ch;
	int detach;
	int exportfs, refreshival, numconnections;
	int nargs;

	setprogname(argv[0]);
	puffs_unmountonsignal(SIGINT, true);
	puffs_unmountonsignal(SIGTERM, true);

	if (argc < 3)
		usage();

	memset(&pctx, 0, sizeof(pctx));
	mntflags = pflags = exportfs = nargs = 0;
	numconnections = 1;
	detach = 1;
	refreshival = DEFAULTREFRESH;
	notfn = puffs_framev_unmountonclose;
	sshargs = NULL;
	add_ssharg(&sshargs, &nargs, SSH_PATH);
	add_ssharg(&sshargs, &nargs, "-axs");
	add_ssharg(&sshargs, &nargs, "-oClearAllForwardings=yes");

	while ((ch = getopt(argc, argv, "c:eF:g:o:O:pr:st:u:")) != -1) {
		switch (ch) {
		case 'c':
			numconnections = atoi(optarg);
			if (numconnections < 1 || numconnections > 2) {
				fprintf(stderr, "%s: only 1 or 2 connections "
				    "permitted currently\n", getprogname());
				usage();
				/*NOTREACHED*/
			}
			break;
		case 'e':
			exportfs = 1;
			break;
		case 'F':
			add_ssharg(&sshargs, &nargs, "-F");
			add_ssharg(&sshargs, &nargs, optarg);
			break;
		case 'g':
			pctx.domanglegid = 1;
			pctx.manglegid = atoi(optarg);
			if (pctx.manglegid == (gid_t)-1)
				errx(1, "-1 not allowed for -g");
			pctx.mygid = getegid();
			break;
		case 'O':
			add_ssharg(&sshargs, &nargs, "-o");
			add_ssharg(&sshargs, &nargs, optarg);
			break;
		case 'o':
			mp = getmntopts(optarg, puffsmopts, &mntflags, &pflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 'p':
			notfn = psshfs_notify;
			break;
		case 'r':
			max_reads = atoi(optarg);
			break;
		case 's':
			detach = 0;
			break;
		case 't':
			refreshival = atoi(optarg);
			if (refreshival < 0 && refreshival != -1)
				errx(1, "invalid timeout %d", refreshival);
			break;
		case 'u':
			pctx.domangleuid = 1;
			pctx.mangleuid = atoi(optarg);
			if (pctx.mangleuid == (uid_t)-1)
				errx(1, "-1 not allowed for -u");
			pctx.myuid = geteuid();
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (pflags & PUFFS_FLAG_OPDUMP)
		detach = 0;
	pflags |= PUFFS_FLAG_BUILDPATH;
	pflags |= PUFFS_KFLAG_WTCACHE | PUFFS_KFLAG_IAONDEMAND;

	if (argc != 2)
		usage();

	PUFFSOP_INIT(pops);

	PUFFSOP_SET(pops, psshfs, fs, unmount);
	PUFFSOP_SETFSNOP(pops, sync); /* XXX */
	PUFFSOP_SET(pops, psshfs, fs, statvfs);
	PUFFSOP_SET(pops, psshfs, fs, nodetofh);
	PUFFSOP_SET(pops, psshfs, fs, fhtonode);

	PUFFSOP_SET(pops, psshfs, node, lookup);
	PUFFSOP_SET(pops, psshfs, node, create);
	PUFFSOP_SET(pops, psshfs, node, open);
	PUFFSOP_SET(pops, psshfs, node, inactive);
	PUFFSOP_SET(pops, psshfs, node, readdir);
	PUFFSOP_SET(pops, psshfs, node, getattr);
	PUFFSOP_SET(pops, psshfs, node, setattr);
	PUFFSOP_SET(pops, psshfs, node, mkdir);
	PUFFSOP_SET(pops, psshfs, node, remove);
	PUFFSOP_SET(pops, psshfs, node, readlink);
	PUFFSOP_SET(pops, psshfs, node, rmdir);
	PUFFSOP_SET(pops, psshfs, node, symlink);
	PUFFSOP_SET(pops, psshfs, node, rename);
	PUFFSOP_SET(pops, psshfs, node, read);
	PUFFSOP_SET(pops, psshfs, node, write);
	PUFFSOP_SET(pops, psshfs, node, reclaim);

	pu = puffs_init(pops, argv[0], "psshfs", &pctx, pflags);
	if (pu == NULL)
		err(1, "puffs_init");

	pctx.mounttime = time(NULL);
	pctx.refreshival = refreshival;
	pctx.numconnections = numconnections;

	user = strdup(argv[0]);
	if ((host = strrchr(user, '@')) == NULL) {
		host = user;
	} else {
		*host++ = '\0';		/* break at the '@' */
		if (user[0] == '\0') {
			fprintf(stderr, "Missing username\n");
			usage();
		}
		add_ssharg(&sshargs, &nargs, "-l");
		add_ssharg(&sshargs, &nargs, user);
	}

	if ((path = colon(host)) != NULL) {
		*path++ = '\0';		/* break at the ':' */
		pctx.mountpath = path;
	} else {
		pctx.mountpath = ".";
	}

	host = cleanhostname(host);
	if (host[0] == '\0') {
		fprintf(stderr, "Missing hostname\n");
		usage();
	}

	add_ssharg(&sshargs, &nargs, host);
	add_ssharg(&sshargs, &nargs, "sftp");
	pctx.sshargs = sshargs;

	pctx.nextino = 2;
	memset(root, 0, sizeof(struct psshfs_node));
	TAILQ_INIT(&root->pw);
	pn_root = puffs_pn_new(pu, root);
	if (pn_root == NULL)
		return errno;
	puffs_setroot(pu, pn_root);

	puffs_framev_init(pu, psbuf_read, psbuf_write, psbuf_cmp, NULL, notfn);

	signal(SIGHUP, takehup);
	puffs_ml_setloopfn(pu, psshfs_loopfn);
	if (pssh_connect(pu, PSSHFD_META) == -1)
		err(1, "can't connect meta");
	if (puffs_framev_addfd(pu, pctx.sshfd,
	    PUFFS_FBIO_READ | PUFFS_FBIO_WRITE) == -1)
		err(1, "framebuf addfd meta");
	if (numconnections == 2) {
		if (pssh_connect(pu, PSSHFD_DATA) == -1)
			err(1, "can't connect data");
		if (puffs_framev_addfd(pu, pctx.sshfd_data,
		    PUFFS_FBIO_READ | PUFFS_FBIO_WRITE) == -1)
			err(1, "framebuf addfd data");
	} else {
		pctx.sshfd_data = pctx.sshfd;
	}

	if (exportfs)
		puffs_setfhsize(pu, sizeof(struct psshfs_fid),
		    PUFFS_FHFLAG_NFSV2 | PUFFS_FHFLAG_NFSV3);

	rva = &pn_root->pn_va;
	rva->va_fileid = pctx.nextino++;

	/*
	 * For root link count, just guess something ridiculously high.
	 * Guessing too high has no known adverse effects, but fts(3)
	 * doesn't like too low values.  This guess will be replaced
	 * with the real value when readdir is first called for
	 * the root directory.
	 */
	rva->va_nlink = 8811;

	if (detach)
		if (puffs_daemon(pu, 1, 1) == -1)
			err(1, "puffs_daemon");

	if (puffs_mount(pu, argv[1], mntflags, puffs_getroot(pu)) == -1)
		err(1, "puffs_mount");
	if (puffs_setblockingmode(pu, PUFFSDEV_NONBLOCK) == -1)
		err(1, "setblockingmode");

	if (puffs_mainloop(pu) == -1)
		err(1, "mainloop");
	puffs_exit(pu, 1);

	return 0;
}

#define RETRY_MAX 100

void
psshfs_notify(struct puffs_usermount *pu, int fd, int what)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	int nretry, which, newfd, dummy;

	if (fd == pctx->sshfd) {
		which = PSSHFD_META;
	} else {
		assert(fd == pctx->sshfd_data);
		which = PSSHFD_DATA;
	}

	if (puffs_getstate(pu) != PUFFS_STATE_RUNNING)
		return;

	if (what != (PUFFS_FBIO_READ | PUFFS_FBIO_WRITE)) {
		puffs_framev_removefd(pu, fd, ECONNRESET);
		return;
	}
	close(fd);

	/* deal with zmobies, beware of half-eaten brain */
	while (waitpid(-1, &dummy, WNOHANG) > 0)
		continue;

	for (nretry = 0;;nretry++) {
		if ((newfd = pssh_connect(pu, which)) == -1)
			goto retry2;

		if (puffs_framev_addfd(pu, newfd,
		    PUFFS_FBIO_READ | PUFFS_FBIO_WRITE) == -1)
			goto retry1;

		break;
 retry1:
		fprintf(stderr, "reconnect failed... ");
		close(newfd);
 retry2:
		if (nretry < RETRY_MAX) {
			fprintf(stderr, "retry (%d left)\n", RETRY_MAX-nretry);
			sleep(nretry);
		} else {
			fprintf(stderr, "retry count exceeded, going south\n");
			exit(1); /* XXXXXXX */
		}
	}
}

static int
pssh_connect(struct puffs_usermount *pu, int which)
{
	struct psshfs_ctx *pctx = puffs_getspecific(pu);
	char * const *sshargs = pctx->sshargs;
	int fds[2];
	pid_t pid;
	int dnfd, x;
	int *sshfd;
	pid_t *sshpid;

	if (which == PSSHFD_META) {
		sshfd = &pctx->sshfd;
		sshpid = &pctx->sshpid;
	} else {
		assert(which == PSSHFD_DATA);
		sshfd = &pctx->sshfd_data;
		sshpid = &pctx->sshpid_data;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1)
		return -1;

	pid = fork();
	switch (pid) {
	case -1:
		return -1;
		/*NOTREACHED*/
	case 0: /* child */
		if (dup2(fds[0], STDIN_FILENO) == -1)
			err(1, "child dup2");
		if (dup2(fds[0], STDOUT_FILENO) == -1)
			err(1, "child dup2");
		close(fds[0]);
		close(fds[1]);

		dnfd = open(_PATH_DEVNULL, O_RDWR);
		if (dnfd != -1)
			dup2(dnfd, STDERR_FILENO);

		execvp(sshargs[0], sshargs);
		/*NOTREACHED*/
		break;
	default:
		*sshpid = pid;
		*sshfd = fds[1];
		close(fds[0]);
		break;
	}

	if (psshfs_handshake(pu, *sshfd) != 0)
		errx(1, "handshake failed, server does not support sftp?");
	x = 1;
	if (ioctl(*sshfd, FIONBIO, &x) == -1)
		err(1, "nonblocking descriptor %d", which);

	return *sshfd;
}

static void *
invalone(struct puffs_usermount *pu, struct puffs_node *pn, void *arg)
{
	struct psshfs_node *psn = pn->pn_data;

	psn->attrread = 0;
	psn->dentread = 0;
	psn->slread = 0;

	return NULL;
}

static void
psshfs_loopfn(struct puffs_usermount *pu)
{

	if (sighup) {
		puffs_pn_nodewalk(pu, invalone, NULL);
		sighup = 0;
	}
}

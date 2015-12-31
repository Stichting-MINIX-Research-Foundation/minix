/*	$NetBSD: exf.c,v 1.8 2014/01/26 21:43:45 christos Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/cdefs.h>
#if 0
#ifndef lint
static const char sccsid[] = "Id: exf.c,v 10.72 2003/08/10 09:44:01 skimo Exp  (Berkeley) Date: 2003/08/10 09:44:01 ";
#endif /* not lint */
#else
__RCSID("$NetBSD: exf.c,v 1.8 2014/01/26 21:43:45 christos Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>		/* XXX: param.h may not have included types.h */
#include <sys/queue.h>
#include <sys/stat.h>

/*
 * We include <sys/file.h>, because the flock(2) and open(2) #defines
 * were found there on historical systems.  We also include <fcntl.h>
 * because the open(2) #defines are found there on newer systems.
 */
#include <sys/file.h>

#include <bitstring.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "dbinternal.h"

static int	file_backup __P((SCR *, const char *, const char *));
static void	file_cinit __P((SCR *));
static void	file_comment __P((SCR *));
static int	file_spath __P((SCR *, FREF *, struct stat *, int *));

/*
 * file_add --
 *	Insert a file name into the FREF list, if it doesn't already
 *	appear in it.
 *
 * !!!
 * The "if it doesn't already appear" changes vi's semantics slightly.  If
 * you do a "vi foo bar", and then execute "next bar baz", the edit of bar
 * will reflect the line/column of the previous edit session.  Historic nvi
 * did not do this.  The change is a logical extension of the change where
 * vi now remembers the last location in any file that it has ever edited,
 * not just the previously edited file.
 *
 * PUBLIC: FREF *file_add __P((SCR *, const char *));
 */
FREF *
file_add(SCR *sp, const char *name)
{
	GS *gp;
	FREF *frp, *tfrp;

	/*
	 * Return it if it already exists.  Note that we test against the
	 * user's name, whatever that happens to be, including if it's a
	 * temporary file.
	 *
	 * If the user added a file but was unable to initialize it, there
	 * can be file list entries where the name field is NULL.  Discard
	 * them the next time we see them.
	 */
	gp = sp->gp;
	if (name != NULL)
		TAILQ_FOREACH_SAFE(frp, &gp->frefq, q, tfrp) {
			if (frp->name == NULL) {
				TAILQ_REMOVE(&gp->frefq, frp, q);
				if (frp->name != NULL)
					free(frp->name);
				free(frp);
				continue;
			}
			if (!strcmp(frp->name, name))
				return (frp);
		}

	/* Allocate and initialize the FREF structure. */
	CALLOC(sp, frp, FREF *, 1, sizeof(FREF));
	if (frp == NULL)
		return (NULL);

	/*
	 * If no file name specified, or if the file name is a request
	 * for something temporary, file_init() will allocate the file
	 * name.  Temporary files are always ignored.
	 */
	if (name != NULL && strcmp(name, TEMPORARY_FILE_STRING) &&
	    (frp->name = strdup(name)) == NULL) {
		free(frp);
		msgq(sp, M_SYSERR, NULL);
		return (NULL);
	}

	/* Append into the chain of file names. */
	TAILQ_INSERT_TAIL(&gp->frefq, frp, q);

	return (frp);
}

/*
 * file_init --
 *	Start editing a file, based on the FREF structure.  If successsful,
 *	let go of any previous file.  Don't release the previous file until
 *	absolutely sure we have the new one.
 *
 * PUBLIC: int file_init __P((SCR *, FREF *, char *, int));
 */
int
file_init(SCR *sp, FREF *frp, char *rcv_name, int flags)
{
	EXF *ep;
	struct stat sb;
	size_t psize;
	int fd, exists, open_err, readonly;
	char *oname = NULL, tname[MAXPATHLEN];

	open_err = readonly = 0;

	/*
	 * If the file is a recovery file, let the recovery code handle it.
	 * Clear the FR_RECOVER flag first -- the recovery code does set up,
	 * and then calls us!  If the recovery call fails, it's probably
	 * because the named file doesn't exist.  So, move boldly forward,
	 * presuming that there's an error message the user will get to see.
	 */
	if (F_ISSET(frp, FR_RECOVER)) {
		F_CLR(frp, FR_RECOVER);
		return (rcv_read(sp, frp));
	}

	/*
	 * Required FRP initialization; the only flag we keep is the
	 * cursor information.
	 */
	F_CLR(frp, ~FR_CURSORSET);

	/*
	 * Scan the user's path to find the file that we're going to
	 * try and open.
	 */
	if (file_spath(sp, frp, &sb, &exists))
		return (1);

	/*
	 * Check whether we already have this file opened in some
	 * other screen.
	 */
	if (exists) {
		EXF *exfp;
		TAILQ_FOREACH(exfp, &sp->gp->exfq, q) {
			if (exfp->mdev == sb.st_dev &&
			    exfp->minode == sb.st_ino && 
			    (exfp != sp->ep || exfp->refcnt > 1)) {
				ep = exfp;
				oname = ep->rcv_path;
				goto postinit;
			}
		}
	}

	/*
	 * Required EXF initialization:
	 *	Flush the line caches.
	 *	Default recover mail file fd to -1.
	 *	Set initial EXF flag bits.
	 */
	CALLOC_RET(sp, ep, EXF *, 1, sizeof(EXF));
	TAILQ_INIT(&ep->scrq);
	sp->c_lno = ep->c_nlines = OOBLNO;
	ep->fd = ep->rcv_fd = ep->fcntl_fd = -1;
	F_SET(ep, F_FIRSTMODIFY);

	/*
	 * If no name or backing file, for whatever reason, create a backing
	 * temporary file, saving the temp file name so we can later unlink
	 * it.  If the user never named this file, copy the temporary file name
	 * to the real name (we display that until the user renames it).
	 */
	oname = frp->name;
	if (LF_ISSET(FS_OPENERR) || oname == NULL || !exists) {
		if (opts_empty(sp, O_TMP_DIRECTORY, 0))
			goto err;
		(void)snprintf(tname, sizeof(tname),
		    "%s/vi.XXXXXX", O_STR(sp, O_TMP_DIRECTORY));
		if ((fd = mkstemp(tname)) == -1) {
			msgq(sp, M_SYSERR,
			    "237|Unable to create temporary file");
			goto err;
		}
		(void)close(fd);

		if (frp->name == NULL)
			F_SET(frp, FR_TMPFILE);
		if ((frp->tname = strdup(tname)) == NULL ||
		    (frp->name == NULL && 
		     (frp->name = strdup(tname)) == NULL)) {
			if (frp->tname != NULL) {
				free(frp->tname);
			}
			msgq(sp, M_SYSERR, NULL);
			(void)unlink(tname);
			goto err;
		}
		oname = frp->tname;
		psize = 1024;
		if (!LF_ISSET(FS_OPENERR))
			F_SET(frp, FR_NEWFILE);

		time(&ep->mtime);
	} else {
		/*
		 * XXX
		 * A seat of the pants calculation: try to keep the file in
		 * 15 pages or less.  Don't use a page size larger than 10K
		 * (vi should have good locality) or smaller than 1K.
		 */
		psize = ((sb.st_size / 15) + 1023) / 1024;
		if (psize > 10)
			psize = 10;
		if (psize == 0)
			psize = 1;
		psize *= 1024;

		F_SET(ep, F_DEVSET);
		ep->mdev = sb.st_dev;
		ep->minode = sb.st_ino;

		ep->mtime = sb.st_mtime;

		if (!S_ISREG(sb.st_mode))
			msgq_str(sp, M_ERR, oname,
			    "238|Warning: %s is not a regular file");
	}

	/* Set up recovery. */
	if (rcv_name == NULL) {
		/* ep->rcv_path NULL if rcv_tmp fails */
		rcv_tmp(sp, ep, frp->name);
	} else {
		if ((ep->rcv_path = strdup(rcv_name)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			goto err;
		}
		F_SET(ep, F_MODIFIED);
	}

	if (db_init(sp, ep, rcv_name, oname, psize, &open_err)) {
		if (open_err && !LF_ISSET(FS_OPENERR))
			goto oerr;
		goto err;
	}

	/*
	 * Do the remaining things that can cause failure of the new file,
	 * mark and logging initialization.
	 */
	if (mark_init(sp, ep) || log_init(sp, ep))
		goto err;

postinit:
	/*
	 * Set the alternate file name to be the file we're discarding.
	 *
	 * !!!
	 * Temporary files can't become alternate files, so there's no file
	 * name.  This matches historical practice, although it could only
	 * happen in historical vi as the result of the initial command, i.e.
	 * if vi was executed without a file name.
	 */
	if (LF_ISSET(FS_SETALT))
		set_alt_name(sp, sp->frp == NULL ||
		    F_ISSET(sp->frp, FR_TMPFILE) ? NULL : sp->frp->name);

	/*
	 * Close the previous file; if that fails, close the new one and run
	 * for the border.
	 *
	 * !!!
	 * There's a nasty special case.  If the user edits a temporary file,
	 * and then does an ":e! %", we need to re-initialize the backing
	 * file, but we can't change the name.  (It's worse -- we're dealing
	 * with *names* here, we can't even detect that it happened.)  Set a
	 * flag so that the file_end routine ignores the backing information
	 * of the old file if it happens to be the same as the new one.
	 *
	 * !!!
	 * Side-effect: after the call to file_end(), sp->frp may be NULL.
	 */
	if (sp->ep != NULL) {
		F_SET(frp, FR_DONTDELETE);
		if (file_end(sp, NULL, LF_ISSET(FS_FORCE))) {
			(void)file_end(sp, ep, 1);
			goto err;
		}
		sp->ep = NULL;
		F_CLR(frp, FR_DONTDELETE);
	}

	/*
	 * Lock the file; if it's a recovery file, it should already be
	 * locked.  Note, we acquire the lock after the previous file
	 * has been ended, so that we don't get an "already locked" error
	 * for ":edit!".
	 *
	 * XXX
	 * While the user can't interrupt us between the open and here,
	 * there's a race between the dbopen() and the lock.  Not much
	 * we can do about it.
	 *
	 * XXX
	 * We don't make a big deal of not being able to lock the file.  As
	 * locking rarely works over NFS, and often fails if the file was
	 * mmap(2)'d, it's far too common to do anything like print an error
	 * message, let alone make the file readonly.  At some future time,
	 * when locking is a little more reliable, this should change to be
	 * an error.
	 */
	if (rcv_name == NULL && ep->refcnt == 0) {
		if ((ep->fd = open(oname, O_RDWR)) == -1)
		    goto no_lock;

		switch (file_lock(sp, oname, &ep->fcntl_fd, ep->fd, 1)) {
		case LOCK_FAILED:
no_lock:
			F_SET(frp, FR_UNLOCKED);
			break;
		case LOCK_UNAVAIL:
			readonly = 1;
			msgq_str(sp, M_INFO, oname,
			    "239|%s already locked, session is read-only");
			break;
		case LOCK_SUCCESS:
			break;
		}
	}

	/*
         * Historically, the readonly edit option was set per edit buffer in
         * vi, unless the -R command-line option was specified or the program
         * was executed as "view".  (Well, to be truthful, if the letter 'w'
         * occurred anywhere in the program name, but let's not get into that.)
	 * So, the persistant readonly state has to be stored in the screen
	 * structure, and the edit option value toggles with the contents of
	 * the edit buffer.  If the persistant readonly flag is set, set the
	 * readonly edit option.
	 *
	 * Otherwise, try and figure out if a file is readonly.  This is a
	 * dangerous thing to do.  The kernel is the only arbiter of whether
	 * or not a file is writeable, and the best that a user program can
	 * do is guess.  Obvious loopholes are files that are on a file system
	 * mounted readonly (access catches this one on a few systems), or
	 * alternate protection mechanisms, ACL's for example, that we can't
	 * portably check.  Lots of fun, and only here because users whined.
	 *
	 * !!!
	 * Historic vi displayed the readonly message if none of the file
	 * write bits were set, or if an an access(2) call on the path
	 * failed.  This seems reasonable.  If the file is mode 444, root
	 * users may want to know that the owner of the file did not expect
	 * it to be written.
	 *
	 * Historic vi set the readonly bit if no write bits were set for
	 * a file, even if the access call would have succeeded.  This makes
	 * the superuser force the write even when vi expects that it will
	 * succeed.  I'm less supportive of this semantic, but it's historic
	 * practice and the conservative approach to vi'ing files as root.
	 *
	 * It would be nice if there was some way to update this when the user
	 * does a "^Z; chmod ...".  The problem is that we'd first have to
	 * distinguish between readonly bits set because of file permissions
	 * and those set for other reasons.  That's not too hard, but deciding
	 * when to reevaluate the permissions is trickier.  An alternative
	 * might be to turn off the readonly bit if the user forces a write
	 * and it succeeds.
	 *
	 * XXX
	 * Access(2) doesn't consider the effective uid/gid values.  This
	 * probably isn't a problem for vi when it's running standalone.
	 */
	if (readonly || F_ISSET(sp, SC_READONLY) ||
	    (!F_ISSET(frp, FR_NEWFILE) &&
	    (!(sb.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) ||
	    access(frp->name, W_OK))))
		O_SET(sp, O_READONLY);
	else
		O_CLR(sp, O_READONLY);

	/* Switch... */
	++ep->refcnt;
	TAILQ_INSERT_HEAD(&ep->scrq, sp, eq);
	sp->ep = ep;
	sp->frp = frp;

	/* Set the initial cursor position, queue initial command. */
	file_cinit(sp);

	/* Report conversion errors again. */
	F_CLR(sp, SC_CONV_ERROR);

	/* Redraw the screen from scratch, schedule a welcome message. */
	F_SET(sp, SC_SCR_REFORMAT | SC_STATUS);

	if (frp->lno == OOBLNO)
		F_SET(sp, SC_SCR_TOP);

	/* Append into the chain of file structures. */
	if (ep->refcnt == 1)
		TAILQ_INSERT_TAIL(&sp->gp->exfq, ep, q);

	return (0);

err:	if (frp->name != NULL) {
		free(frp->name);
		frp->name = NULL;
	}
	if (frp->tname != NULL) {
		(void)unlink(frp->tname);
		free(frp->tname);
		frp->tname = NULL;
	}

oerr:	if (F_ISSET(ep, F_RCV_ON))
		(void)unlink(ep->rcv_path);
	if (ep->rcv_path != NULL) {
		free(ep->rcv_path);
		ep->rcv_path = NULL;
	}
	if (ep->db != NULL) {
		(void)db_close(ep->db);
		ep->db = NULL;
	}
	free(ep);

	return (open_err && !LF_ISSET(FS_OPENERR) ?
	    file_init(sp, frp, rcv_name, flags | FS_OPENERR) : 1);
}

/*
 * file_spath --
 *	Scan the user's path to find the file that we're going to
 *	try and open.
 */
static int
file_spath(SCR *sp, FREF *frp, struct stat *sbp, int *existsp)
{
	size_t len;
	int found;
	char *name, path[MAXPATHLEN];
	const char *p, *t;

	/*
	 * If the name is NULL or an explicit reference (i.e., the first
	 * component is . or ..) ignore the O_PATH option.
	 */
	name = frp->name;
	if (name == NULL) {
		*existsp = 0;
		return (0);
	}
	if (name[0] == '/' || (name[0] == '.' &&
	    (name[1] == '/' || (name[1] == '.' && name[2] == '/')))) {
		*existsp = !stat(name, sbp);
		return (0);
	}

	/* Try . */
	if (!stat(name, sbp)) {
		*existsp = 1;
		return (0);
	}

	/* Try the O_PATH option values. */
	for (found = 0, p = t = O_STR(sp, O_PATH);; ++p)
		if (*p == ':' || *p == '\0') {
			if (t < p - 1) {
				len = snprintf(path, sizeof(path), "%.*s/%s",
				    (int)(p - t), t, name);
				if (!stat(path, sbp)) {
					found = 1;
					break;
				}
			}
			t = p + 1;
			if (*p == '\0')
				break;
		}

	/* If we found it, build a new pathname and discard the old one. */
	if (found) {
		char *q;
		MALLOC_RET(sp, q, char *, len + 1);
		memcpy(q, path, len + 1);
		free(frp->name);
		frp->name = q;
	}
	*existsp = found;
	return (0);
}

/*
 * file_cinit --
 *	Set up the initial cursor position.
 */
static void
file_cinit(SCR *sp)
{
	GS *gp;
	MARK m;
	size_t len;
	int nb;
	const CHAR_T *wp;
	size_t wlen;

	/* Set some basic defaults. */
	sp->lno = 1;
	sp->cno = 0;

	/*
	 * Historically, initial commands (the -c option) weren't executed
	 * until a file was loaded, e.g. "vi +10 nofile", followed by an
	 * :edit or :tag command, would execute the +10 on the file loaded
	 * by the subsequent command, (assuming that it existed).  This
	 * applied as well to files loaded using the tag commands, and we
	 * follow that historic practice.  Also, all initial commands were
	 * ex commands and were always executed on the last line of the file.
	 *
	 * Otherwise, if no initial command for this file:
	 *    If in ex mode, move to the last line, first nonblank character.
	 *    If the file has previously been edited, move to the last known
	 *	  position, and check it for validity.
	 *    Otherwise, move to the first line, first nonblank.
	 *
	 * This gets called by the file init code, because we may be in a
	 * file of ex commands and we want to execute them from the right
	 * location in the file.
	 */
	nb = 0;
	gp = sp->gp;
	if (gp->c_option != NULL && !F_ISSET(sp->frp, FR_NEWFILE)) {
		if (db_last(sp, &sp->lno))
			return;
		if (sp->lno == 0) {
			sp->lno = 1;
			sp->cno = 0;
		}
		CHAR2INT(sp, gp->c_option, strlen(gp->c_option) + 1,
			 wp, wlen);
		if (ex_run_str(sp, "-c option", wp, wlen - 1, 1, 1))
			return;
		gp->c_option = NULL;
	} else if (F_ISSET(sp, SC_EX)) {
		if (db_last(sp, &sp->lno))
			return;
		if (sp->lno == 0) {
			sp->lno = 1;
			sp->cno = 0;
			return;
		}
		nb = 1;
	} else {
		if (F_ISSET(sp->frp, FR_CURSORSET)) {
			sp->lno = sp->frp->lno;
			sp->cno = sp->frp->cno;

			/* If returning to a file in vi, center the line. */
			 F_SET(sp, SC_SCR_CENTER);
		} else {
			if (O_ISSET(sp, O_COMMENT))
				file_comment(sp);
			else
				sp->lno = 1;
			nb = 1;
		}
		if (db_get(sp, sp->lno, 0, NULL, &len)) {
			sp->lno = 1;
			sp->cno = 0;
			return;
		}
		if (!nb && sp->cno > len)
			nb = 1;
	}
	if (nb) {
		sp->cno = 0;
		(void)nonblank(sp, sp->lno, &sp->cno);
	}

	/*
	 * !!!
	 * The initial column is also the most attractive column.
	 */
	sp->rcm = sp->cno;

	/*
	 * !!!
	 * Historically, vi initialized the absolute mark, but ex did not.
	 * Which meant, that if the first command in ex mode was "visual",
	 * or if an ex command was executed first (e.g. vi +10 file) vi was
	 * entered without the mark being initialized.  For consistency, if
	 * the file isn't empty, we initialize it for everyone, believing
	 * that it can't hurt, and is generally useful.  Not initializing it
	 * if the file is empty is historic practice, although it has always
	 * been possible to set (and use) marks in empty vi files.
	 */
	m.lno = sp->lno;
	m.cno = sp->cno;
	(void)mark_set(sp, ABSMARK1, &m, 0);
}

/*
 * file_end --
 *	Stop editing a file.
 *
 * PUBLIC: int file_end __P((SCR *, EXF *, int));
 */
int
file_end(SCR *sp, EXF *ep, int force)
{
	FREF *frp;

	/*
	 * !!!
	 * ep MAY NOT BE THE SAME AS sp->ep, DON'T USE THE LATTER.
	 * (If argument ep is NULL, use sp->ep.)
	 *
	 * If multiply referenced, just decrement the count and return.
	 */
	if (ep == NULL)
		ep = sp->ep;
	TAILQ_REMOVE(&ep->scrq, sp, eq);
	if (--ep->refcnt != 0)
		return (0);

	/*
	 *
	 * Clean up the FREF structure.
	 *
	 * Save the cursor location.
	 *
	 * XXX
	 * It would be cleaner to do this somewhere else, but by the time
	 * ex or vi knows that we're changing files it's already happened.
	 */
	frp = sp->frp;
	frp->lno = sp->lno;
	frp->cno = sp->cno;
	F_SET(frp, FR_CURSORSET);

	/*
	 * We may no longer need the temporary backing file, so clean it
	 * up.  We don't need the FREF structure either, if the file was
	 * never named, so lose it.
	 *
	 * !!!
	 * Re: FR_DONTDELETE, see the comment above in file_init().
	 */
	if (!F_ISSET(frp, FR_DONTDELETE) && frp->tname != NULL) {
		if (unlink(frp->tname))
			msgq_str(sp, M_SYSERR, frp->tname, "240|%s: remove");
		free(frp->tname);
		frp->tname = NULL;
		if (F_ISSET(frp, FR_TMPFILE)) {
			TAILQ_REMOVE(&sp->gp->frefq, frp, q);
			if (frp->name != NULL)
				free(frp->name);
			free(frp);
		}
		sp->frp = NULL;
	}

	/*
	 * Clean up the EXF structure.
	 *
	 * Close the db structure.
	 */
	if (ep->db->close != NULL) {
		if ((sp->db_error = db_close(ep->db)) != 0 && 
		    !force) {
			msgq_str(sp, M_DBERR, frp->name, "241|%s: close");
			TAILQ_INSERT_HEAD(&ep->scrq, sp, eq);
			++ep->refcnt;
			return (1);
		}
		ep->db = NULL;
	}

	/* COMMITTED TO THE CLOSE.  THERE'S NO GOING BACK... */

	/* Stop logging. */
	(void)log_end(sp, ep);

	/* Free up any marks. */
	(void)mark_end(sp, ep);

	if (ep->env) {
		DB_ENV *env;

		db_env_close(ep->env, 0);
		ep->env = 0;
		if ((sp->db_error = db_env_create(&env, 0)) != 0)
			msgq(sp, M_DBERR, "env_create");
		if ((sp->db_error = db_env_remove(env, ep->env_path, 0)) != 0)
			msgq(sp, M_DBERR, "env->remove");
		if (ep->env_path != NULL && rmdir(ep->env_path))
			msgq_str(sp, M_SYSERR, ep->env_path, "242|%s: remove");
	}

	/*
	 * Delete recovery files, close the open descriptor, free recovery
	 * memory.  See recover.c for a description of the protocol.
	 *
	 * XXX
	 * Unlink backup file first, we can detect that the recovery file
	 * doesn't reference anything when the user tries to recover it.
	 * There's a race, here, obviously, but it's fairly small.
	 */
	if (!F_ISSET(ep, F_RCV_NORM)) {
		if (ep->rcv_path != NULL && unlink(ep->rcv_path))
			msgq_str(sp, M_SYSERR, ep->rcv_path, "242|%s: remove");
		if (ep->rcv_mpath != NULL && unlink(ep->rcv_mpath))
			msgq_str(sp, M_SYSERR, ep->rcv_mpath, "243|%s: remove");
	}
	TAILQ_REMOVE(&sp->gp->exfq, ep, q);
	if (ep->fd != -1)
		(void)close(ep->fd);
	if (ep->fcntl_fd != -1)
		(void)close(ep->fcntl_fd);
	if (ep->rcv_fd != -1)
		(void)close(ep->rcv_fd);
	if (ep->env_path != NULL)
		free(ep->env_path);
	if (ep->rcv_path != NULL) {
		free(ep->rcv_path);
		ep->rcv_path = NULL;
	}
	if (ep->rcv_mpath != NULL)
		free(ep->rcv_mpath);

	free(ep);
	return (0);
}

/*
 * file_write --
 *	Write the file to disk.  Historic vi had fairly convoluted
 *	semantics for whether or not writes would happen.  That's
 *	why all the flags.
 *
 * PUBLIC: int file_write __P((SCR *, MARK *, MARK *, char *, int));
 */
int
file_write(SCR *sp, MARK *fm, MARK *tm, char *name, int flags)
{
	enum { NEWFILE, OLDFILE } mtype;
	struct stat sb;
	EXF *ep;
	FILE *fp;
	FREF *frp;
	MARK from, to;
	size_t len;
	u_long nlno, nch;
	int fd, nf, noname, oflags, rval;
	char *p, *s, *t, buf[MAXPATHLEN + 64];
	const char *msgstr;

	ep = sp->ep;
	frp = sp->frp;

	/*
	 * Writing '%', or naming the current file explicitly, has the
	 * same semantics as writing without a name.
	 */
	if (name == NULL || !strcmp(name, frp->name)) {
		noname = 1;
		name = frp->name;
	} else
		noname = 0;

	/* Can't write files marked read-only, unless forced. */
	if (!LF_ISSET(FS_FORCE) && noname && O_ISSET(sp, O_READONLY)) {
		msgq(sp, M_ERR, LF_ISSET(FS_POSSIBLE) ?
		    "244|Read-only file, not written; use ! to override" :
		    "245|Read-only file, not written");
		return (1);
	}

	/* If not forced, not appending, and "writeany" not set ... */
	if (!LF_ISSET(FS_FORCE | FS_APPEND) && !O_ISSET(sp, O_WRITEANY)) {
		/* Don't overwrite anything but the original file. */
		if ((!noname || F_ISSET(frp, FR_NAMECHANGE)) &&
		    !stat(name, &sb)) {
			msgq_str(sp, M_ERR, name,
			    LF_ISSET(FS_POSSIBLE) ?
			    "246|%s exists, not written; use ! to override" :
			    "247|%s exists, not written");
			return (1);
		}

		/*
		 * Don't write part of any existing file.  Only test for the
		 * original file, the previous test catches anything else.
		 */
		if (!LF_ISSET(FS_ALL) && noname && !stat(name, &sb)) {
			msgq(sp, M_ERR, LF_ISSET(FS_POSSIBLE) ?
			    "248|Partial file, not written; use ! to override" :
			    "249|Partial file, not written");
			return (1);
		}
	}

	/*
	 * Figure out if the file already exists -- if it doesn't, we display
	 * the "new file" message.  The stat might not be necessary, but we
	 * just repeat it because it's easier than hacking the previous tests.
	 * The information is only used for the user message and modification
	 * time test, so we can ignore the obvious race condition.
	 *
	 * One final test.  If we're not forcing or appending the current file,
	 * and we have a saved modification time, object if the file changed
	 * since we last edited or wrote it, and make them force it.
	 */
	if (stat(name, &sb))
		mtype = NEWFILE;
	else {
		if (noname && !LF_ISSET(FS_FORCE | FS_APPEND) &&
		    ((F_ISSET(ep, F_DEVSET) &&
		    (sb.st_dev != ep->mdev || sb.st_ino != ep->minode)) ||
		    sb.st_mtime != ep->mtime)) {
			msgq_str(sp, M_ERR, name, LF_ISSET(FS_POSSIBLE) ?
"250|%s: file modified more recently than this copy; use ! to override" :
"251|%s: file modified more recently than this copy");
			return (1);
		}

		mtype = OLDFILE;
	}

	/* Set flags to create, write, and either append or truncate. */
	oflags = O_CREAT | O_WRONLY |
	    (LF_ISSET(FS_APPEND) ? O_APPEND : O_TRUNC);

	/* Backup the file if requested. */
	if (!opts_empty(sp, O_BACKUP, 1) &&
	    file_backup(sp, name, O_STR(sp, O_BACKUP)) && !LF_ISSET(FS_FORCE))
		return (1);

	/* Open the file. */
	SIGBLOCK;
	if ((fd = open(name, oflags,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) < 0) {
		msgq_str(sp, M_SYSERR, name, "%s");
		SIGUNBLOCK;
		return (1);
	}
	SIGUNBLOCK;

	/* Try and get a lock. */
	if (!noname && file_lock(sp, NULL, NULL, fd, 0) == LOCK_UNAVAIL)
		msgq_str(sp, M_ERR, name,
		    "252|%s: write lock was unavailable");

#if __linux__
	/*
	 * XXX
	 * In libc 4.5.x, fdopen(fd, "w") clears the O_APPEND flag (if set).
	 * This bug is fixed in libc 4.6.x.
	 *
	 * This code works around this problem for libc 4.5.x users.
	 * Note that this code is harmless if you're using libc 4.6.x.
	 */
	if (LF_ISSET(FS_APPEND) && lseek(fd, (off_t)0, SEEK_END) < 0) {
		msgq(sp, M_SYSERR, "%s", name);
		return (1);
	}
#endif

	/*
	 * Use stdio for buffering.
	 *
	 * XXX
	 * SVR4.2 requires the fdopen mode exactly match the original open
	 * mode, i.e. you have to open with "a" if appending.
	 */
	if ((fp = fdopen(fd, LF_ISSET(FS_APPEND) ? "a" : "w")) == NULL) {
		msgq_str(sp, M_SYSERR, name, "%s");
		(void)close(fd);
		return (1);
	}

	/* Build fake addresses, if necessary. */
	if (fm == NULL) {
		from.lno = 1;
		from.cno = 0;
		fm = &from;
		if (db_last(sp, &to.lno))
			return (1);
		to.cno = 0;
		tm = &to;
	}

	rval = ex_writefp(sp, name, fp, fm, tm, &nlno, &nch, 0);

	/*
	 * Save the new last modification time -- even if the write fails
	 * we re-init the time.  That way the user can clean up the disk
	 * and rewrite without having to force it.
	 */
	if (noname) {
		if (stat(name, &sb))
			time(&ep->mtime);
		else {
			F_SET(ep, F_DEVSET);
			ep->mdev = sb.st_dev;
			ep->minode = sb.st_ino;

			ep->mtime = sb.st_mtime;
		}
	}

	/*
	 * If the write failed, complain loudly.  ex_writefp() has already
	 * complained about the actual error, reinforce it if data was lost.
	 */
	if (rval) {
		if (!LF_ISSET(FS_APPEND))
			msgq_str(sp, M_ERR, name,
			    "254|%s: WARNING: FILE TRUNCATED");
		return (1);
	}

	/*
	 * Once we've actually written the file, it doesn't matter that the
	 * file name was changed -- if it was, we've already whacked it.
	 */
	F_CLR(frp, FR_NAMECHANGE);

	/*
	 * If wrote the entire file, and it wasn't by appending it to a file,
	 * clear the modified bit.  If the file was written to the original
	 * file name and the file is a temporary, set the "no exit" bit.  This
	 * permits the user to write the file and use it in the context of the
	 * filesystem, but still keeps them from discarding their changes by
	 * exiting.
	 */
	if (LF_ISSET(FS_ALL) && !LF_ISSET(FS_APPEND)) {
		F_CLR(ep, F_MODIFIED);
		if (F_ISSET(frp, FR_TMPFILE)) {
			if (noname)
				F_SET(frp, FR_TMPEXIT);
			else
				F_CLR(frp, FR_TMPEXIT);
		}
	}

	p = msg_print(sp, name, &nf);
	switch (mtype) {
	case NEWFILE:
		msgstr = msg_cat(sp,
		    "256|%s: new file: %lu lines, %lu characters", NULL);
		len = snprintf(buf, sizeof(buf), msgstr, p, nlno, nch);
		break;
	case OLDFILE:
		msgstr = msg_cat(sp, LF_ISSET(FS_APPEND) ?
		    "315|%s: appended: %lu lines, %lu characters" :
		    "257|%s: %lu lines, %lu characters", NULL);
		len = snprintf(buf, sizeof(buf), msgstr, p, nlno, nch);
		break;
	default:
		abort();
	}

	/*
	 * There's a nasty problem with long path names.  Cscope and tags files
	 * can result in long paths and vi will request a continuation key from
	 * the user.  Unfortunately, the user has typed ahead, and chaos will
	 * result.  If we assume that the characters in the filenames only take
	 * a single screen column each, we can trim the filename.
	 */
	s = buf;
	if (len >= sp->cols) {
		for (s = buf, t = buf + strlen(p); s < t &&
		    (*s != '/' || len >= sp->cols - 3); ++s, --len);
		if (s == t)
			s = buf;
		else {
			*--s = '.';		/* Leading ellipses. */
			*--s = '.';
			*--s = '.';
		}
	}
	msgq(sp, M_INFO, "%s", s);
	if (nf)
		FREE_SPACE(sp, p, 0);
	return (0);
}

/*
 * file_backup --
 *	Backup the about-to-be-written file.
 *
 * XXX
 * We do the backup by copying the entire file.  It would be nice to do
 * a rename instead, but: (1) both files may not fit and we want to fail
 * before doing the rename; (2) the backup file may not be on the same
 * disk partition as the file being written; (3) there may be optional
 * file information (MACs, DACs, whatever) that we won't get right if we
 * recreate the file.  So, let's not risk it.
 */
static int
file_backup(SCR *sp, const char *name, const char *bname)
{
	struct dirent *dp;
	struct stat sb;
	DIR *dirp;
	EXCMD cmd;
	off_t off;
	size_t blen;
	int flags, maxnum, nr, num, nw, rfd, wfd, version;
	char *bp, *pct, *slash, *t, buf[8192];
	const char *p, *estr, *wfname;
	const CHAR_T *wp;
	size_t wlen;
	size_t nlen;
	char *d = NULL;

	rfd = wfd = -1;
	estr = wfname = NULL;
	bp = NULL;

	/*
	 * Open the current file for reading.  Do this first, so that
	 * we don't exec a shell before the most likely failure point.
	 * If it doesn't exist, it's okay, there's just nothing to back
	 * up.
	 */
	errno = 0;
	if ((rfd = open(name, O_RDONLY, 0)) < 0) {
		if (errno == ENOENT)
			return (0);
		estr = name;
		goto err;
	}

	/*
	 * If the name starts with an 'N' character, add a version number
	 * to the name.  Strip the leading N from the string passed to the
	 * expansion routines, for no particular reason.  It would be nice
	 * to permit users to put the version number anywhere in the backup
	 * name, but there isn't a special character that we can use in the
	 * name, and giving a new character a special meaning leads to ugly
	 * hacks both here and in the supporting ex routines.
	 *
	 * Shell and file name expand the option's value.
	 */
	ex_cinit(sp, &cmd, 0, 0, 0, 0, 0);
	if (bname[0] == 'N') {
		version = 1;
		++bname;
	} else
		version = 0;
	CHAR2INT(sp, bname, strlen(bname) + 1, wp, wlen);
	if (argv_exp2(sp, &cmd, wp, wlen - 1))
		return (1);

	/*
	 *  0 args: impossible.
	 *  1 args: use it.
	 * >1 args: object, too many args.
	 */
	if (cmd.argc != 1) {
		msgq_str(sp, M_ERR, bname,
		    "258|%s expanded into too many file names");
		(void)close(rfd);
		return (1);
	}

	/*
	 * If appending a version number, read through the directory, looking
	 * for file names that match the name followed by a number.  Make all
	 * of the other % characters in name literal, so the user doesn't get
	 * surprised and sscanf doesn't drop core indirecting through pointers
	 * that don't exist.  If any such files are found, increment its number
	 * by one.
	 */
	if (version) {
		GET_SPACE_GOTOC(sp, bp, blen, cmd.argv[0]->len * 2 + 50);
		INT2SYS(sp, cmd.argv[0]->bp, cmd.argv[0]->len + 1,
			 p, nlen); 
		d = strdup(p);
		p = d;
		for (t = bp, slash = NULL;
		     p[0] != '\0'; *t++ = *p++)
			if (p[0] == '%') {
				if (p[1] != '%')
					*t++ = '%';
			} else if (p[0] == '/')
				slash = t;
		pct = t;
		*t++ = '%';
		*t++ = 'd';
		*t = '\0';

		if (slash == NULL) {
			dirp = opendir(".");
			p = bp;
		} else {
			*slash = '\0';
			dirp = opendir(bp);
			*slash = '/';
			p = slash + 1;
		}
		if (dirp == NULL) {
			INT2SYS(sp, cmd.argv[0]->bp, cmd.argv[0]->len + 1,
				estr, nlen);
			goto err;
		}

		for (maxnum = 0; (dp = readdir(dirp)) != NULL;)
			if (sscanf(dp->d_name, p, &num) == 1 && num > maxnum)
				maxnum = num;
		(void)closedir(dirp);

		/* Format the backup file name. */
		(void)snprintf(pct, blen - (pct - bp), "%d", maxnum + 1);
		wfname = bp;
	} else {
		bp = NULL;
		INT2SYS(sp, cmd.argv[0]->bp, cmd.argv[0]->len + 1,
			wfname, nlen);
	}
	
	/* Open the backup file, avoiding lurkers. */
	if (stat(wfname, &sb) == 0) {
		if (!S_ISREG(sb.st_mode)) {
			msgq_str(sp, M_ERR, bname,
			    "259|%s: not a regular file");
			goto err;
		}
		if (sb.st_uid != getuid()) {
			msgq_str(sp, M_ERR, bname, "260|%s: not owned by you");
			goto err;
		}
		if (sb.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) {
			msgq_str(sp, M_ERR, bname,
			   "261|%s: accessible by a user other than the owner");
			goto err;
		}
		flags = O_TRUNC;
	} else
		flags = O_CREAT | O_EXCL;
	if ((wfd = open(wfname, flags | O_WRONLY, S_IRUSR | S_IWUSR)) < 0) {
		estr = bname;
		goto err;
	}

	/* Copy the file's current contents to its backup value. */
	while ((nr = read(rfd, buf, sizeof(buf))) > 0)
		for (off = 0; nr != 0; nr -= nw, off += nw)
			if ((nw = write(wfd, buf + off, nr)) < 0) {
				estr = wfname;
				goto err;
			}
	if (nr < 0) {
		estr = name;
		goto err;
	}

	if (close(rfd)) {
		estr = name;
		goto err;
	}
	if (close(wfd)) {
		estr = wfname;
		goto err;
	}
	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	if (d != NULL)
		free(d);
	return (0);

alloc_err:
err:	if (rfd != -1)
		(void)close(rfd);
	if (wfd != -1) {
		(void)unlink(wfname);
		(void)close(wfd);
	}
	if (estr)
		msgq_str(sp, M_SYSERR, estr, "%s");
	if (d != NULL)
		free(d);
	if (bp != NULL)
		FREE_SPACE(sp, bp, blen);
	return (1);
}

/*
 * file_comment --
 *	Skip the first comment.
 */
static void
file_comment(SCR *sp)
{
	db_recno_t lno;
	size_t len;
	CHAR_T *p;

	for (lno = 1; !db_get(sp, lno, 0, &p, &len) && len == 0; ++lno);
	if (p == NULL)
		return;
	if (p[0] == '#') {
		F_SET(sp, SC_SCR_TOP);
		while (!db_get(sp, ++lno, 0, &p, &len))
			if (len < 1 || p[0] != '#') {
				sp->lno = lno;
				return;
			}
	} else if (len > 1 && p[0] == '/' && p[1] == '*') {
		F_SET(sp, SC_SCR_TOP);
		do {
			for (; len > 1; --len, ++p)
				if (p[0] == '*' && p[1] == '/') {
					sp->lno = lno;
					return;
				}
		} while (!db_get(sp, ++lno, 0, &p, &len));
	} else if (len > 1 && p[0] == '/' && p[1] == '/') {
		F_SET(sp, SC_SCR_TOP);
		while (!db_get(sp, ++lno, 0, &p, &len))
			if (len < 1 || p[0] != '/' || p[1] != '/') {
				sp->lno = lno;
				return;
			}
	}
}

/*
 * file_m1 --
 * 	First modification check routine.  The :next, :prev, :rewind, :tag,
 *	:tagpush, :tagpop, ^^ modifications check.
 *
 * PUBLIC: int file_m1 __P((SCR *, int, int));
 */
int
file_m1(SCR *sp, int force, int flags)
{
	EXF *ep;

	ep = sp->ep;

	/* If no file loaded, return no modifications. */
	if (ep == NULL)
		return (0);

	/*
	 * If the file has been modified, we'll want to write it back or
	 * fail.  If autowrite is set, we'll write it back automatically,
	 * unless force is also set.  Otherwise, we fail unless forced or
	 * there's another open screen on this file.
	 */
	if (F_ISSET(ep, F_MODIFIED)) {
		if (O_ISSET(sp, O_AUTOWRITE)) {
			if (!force && file_aw(sp, flags))
				return (1);
		} else if (ep->refcnt <= 1 && !force) {
			msgq(sp, M_ERR, LF_ISSET(FS_POSSIBLE) ?
"262|File modified since last complete write; write or use ! to override" :
"263|File modified since last complete write; write or use :edit! to override");
			return (1);
		}
	}

	return (file_m3(sp, force));
}

/*
 * file_m2 --
 * 	Second modification check routine.  The :edit, :quit, :recover
 *	modifications check.
 *
 * PUBLIC: int file_m2 __P((SCR *, int));
 */
int
file_m2(SCR *sp, int force)
{
	EXF *ep;

	ep = sp->ep;

	/* If no file loaded, return no modifications. */
	if (ep == NULL)
		return (0);

	/*
	 * If the file has been modified, we'll want to fail, unless forced
	 * or there's another open screen on this file.
	 */
	if (F_ISSET(ep, F_MODIFIED) && ep->refcnt <= 1 && !force) {
		msgq(sp, M_ERR,
"264|File modified since last complete write; write or use ! to override");
		return (1);
	}

	return (file_m3(sp, force));
}

/*
 * file_m3 --
 * 	Third modification check routine.
 *
 * PUBLIC: int file_m3 __P((SCR *, int));
 */
int
file_m3(SCR *sp, int force)
{
	EXF *ep;

	ep = sp->ep;

	/* If no file loaded, return no modifications. */
	if (ep == NULL)
		return (0);

	/*
	 * Don't exit while in a temporary files if the file was ever modified.
	 * The problem is that if the user does a ":wq", we write and quit,
	 * unlinking the temporary file.  Not what the user had in mind at all.
	 * We permit writing to temporary files, so that user maps using file
	 * system names work with temporary files.
	 */
	if (F_ISSET(sp->frp, FR_TMPEXIT) && ep->refcnt <= 1 && !force) {
		msgq(sp, M_ERR,
		    "265|File is a temporary; exit will discard modifications");
		return (1);
	}
	return (0);
}

/*
 * file_aw --
 *	Autowrite routine.  If modified, autowrite is set and the readonly bit
 *	is not set, write the file.  A routine so there's a place to put the
 *	comment.
 *
 * PUBLIC: int file_aw __P((SCR *, int));
 */
int
file_aw(SCR *sp, int flags)
{
	if (!F_ISSET(sp->ep, F_MODIFIED))
		return (0);
	if (!O_ISSET(sp, O_AUTOWRITE))
		return (0);

	/*
	 * !!!
	 * Historic 4BSD vi attempted to write the file if autowrite was set,
	 * regardless of the writeability of the file (as defined by the file
	 * readonly flag).  System V changed this as some point, not attempting
	 * autowrite if the file was readonly.  This feels like a bug fix to
	 * me (e.g. the principle of least surprise is violated if readonly is
	 * set and vi writes the file), so I'm compatible with System V.
	 */
	if (O_ISSET(sp, O_READONLY)) {
		msgq(sp, M_INFO,
		    "266|File readonly, modifications not auto-written");
		return (1);
	}
	return (file_write(sp, NULL, NULL, NULL, flags));
}

/*
 * set_alt_name --
 *	Set the alternate pathname.
 *
 * Set the alternate pathname.  It's a routine because I wanted some place
 * to hang this comment.  The alternate pathname (normally referenced using
 * the special character '#' during file expansion and in the vi ^^ command)
 * is set by almost all ex commands that take file names as arguments.  The
 * rules go something like this:
 *
 *    1: If any ex command takes a file name as an argument (except for the
 *	 :next command), the alternate pathname is set to that file name.
 *	 This excludes the command ":e" and ":w !command" as no file name
 *       was specified.  Note, historically, the :source command did not set
 *	 the alternate pathname.  It does in nvi, for consistency.
 *
 *    2: However, if any ex command sets the current pathname, e.g. the
 *	 ":e file" or ":rew" commands succeed, then the alternate pathname
 *	 is set to the previous file's current pathname, if it had one.
 *	 This includes the ":file" command and excludes the ":e" command.
 *	 So, by rule #1 and rule #2, if ":edit foo" fails, the alternate
 *	 pathname will be "foo", if it succeeds, the alternate pathname will
 *	 be the previous current pathname.  The ":e" command will not set
 *       the alternate or current pathnames regardless.
 *
 *    3: However, if it's a read or write command with a file argument and
 *	 the current pathname has not yet been set, the file name becomes
 *	 the current pathname, and the alternate pathname is unchanged.
 *
 * If the user edits a temporary file, there may be times when there is no
 * alternative file name.  A name argument of NULL turns it off.
 *
 * PUBLIC: void set_alt_name __P((SCR *, const char *));
 */
void
set_alt_name(SCR *sp, const char *name)
{
	if (sp->alt_name != NULL)
		free(sp->alt_name);
	if (name == NULL)
		sp->alt_name = NULL;
	else if ((sp->alt_name = strdup(name)) == NULL)
		msgq(sp, M_SYSERR, NULL);
}

/*
 * file_lock --
 *	Get an exclusive lock on a file and set close-on-exec flag
 *
 * XXX
 * The default locking is flock(2) style, not fcntl(2).  The latter is
 * known to fail badly on some systems, and its only advantage is that
 * it occasionally works over NFS.
 *
 * Furthermore, the semantics of fcntl(2) are wrong.  The problems are
 * two-fold: you can't close any file descriptor associated with the file
 * without losing all of the locks, and you can't get an exclusive lock
 * unless you have the file open for writing.  Someone ought to be shot,
 * but it's probably too late, they may already have reproduced.  To get
 * around these problems, nvi opens the files for writing when it can and
 * acquires a second file descriptor when it can't.  The recovery files
 * are examples of the former, they're always opened for writing.  The DB
 * files can't be opened for writing because the semantics of DB are that
 * files opened for writing are flushed back to disk when the DB session
 * is ended. So, in that case we have to acquire an extra file descriptor.
 *
 * PUBLIC: lockr_t file_lock __P((SCR *, char *, int *, int, int));
 */
lockr_t
file_lock(SCR *sp, char *name, int *fdp, int fd, int iswrite)
{
	fcntl(fd, F_SETFD, 1);

	if (!O_ISSET(sp, O_LOCKFILES))
		return (LOCK_SUCCESS);
	
#ifdef HAVE_LOCK_FLOCK			/* Hurrah!  We've got flock(2). */
	/*
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Flock is documented
	 * as returning EWOULDBLOCK; add EAGAIN for good measure, and assume
	 * they are the former.  There's no portable way to do this.
	 */
	errno = 0;
	return (flock(fd, LOCK_EX | LOCK_NB) ? errno == EAGAIN
#ifdef EWOULDBLOCK
	    || errno == EWOULDBLOCK
#endif
	    ? LOCK_UNAVAIL : LOCK_FAILED : LOCK_SUCCESS);
#endif
#ifdef HAVE_LOCK_FCNTL			/* Gag me.  We've got fcntl(2). */
{
	struct flock arg;
	int didopen, sverrno;

	arg.l_type = F_WRLCK;
	arg.l_whence = 0;		/* SEEK_SET */
	arg.l_start = arg.l_len = 0;
	arg.l_pid = 0;

	/*
	 * If the file descriptor isn't opened for writing, it must fail.
	 * If we fail because we can't get a read/write file descriptor,
	 * we return LOCK_SUCCESS, believing that the file is readonly
	 * and that will be sufficient to warn the user.
	 */
	if (!iswrite) {
		if (name == NULL || fdp == NULL)
			return (LOCK_FAILED);
		if ((fd = open(name, O_RDWR, 0)) == -1)
			return (LOCK_SUCCESS);
		*fdp = fd;
		didopen = 1;
	}

	errno = 0;
	if (!fcntl(fd, F_SETLK, &arg))
		return (LOCK_SUCCESS);
	if (didopen) {
		sverrno = errno;
		(void)close(fd);
		errno = sverrno;
	}

	/*
	 * !!!
	 * We need to distinguish a lock not being available for the file
	 * from the file system not supporting locking.  Fcntl is documented
	 * as returning EACCESS and EAGAIN; add EWOULDBLOCK for good measure,
	 * and assume they are the former.  There's no portable way to do this.
	 */
	return (errno == EACCES || errno == EAGAIN
#ifdef EWOULDBLOCK
	|| errno == EWOULDBLOCK
#endif
	?  LOCK_UNAVAIL : LOCK_FAILED);
}
#endif
#if !defined(HAVE_LOCK_FLOCK) && !defined(HAVE_LOCK_FCNTL)
	return (LOCK_SUCCESS);
#endif
}

/*	cron 1.4 - clock daemon				Author: Kees J. Bot
 *								7 Dec 1996
 */

#define nil ((void*)0)
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "misc.h"
#include "tab.h"

#if __minix && !__minix_vmd
#define initgroups(name, gid)	(0)
#endif

static volatile int busy;	/* Set when something is afoot, don't sleep! */
static volatile int need_reload;/* Set if table reload required. */
static volatile int need_quit;	/* Set if cron must exit. */
static volatile int debug;	/* Debug level. */

static void run_job(cronjob_t *job)
/* Execute a cron job.  Register its pid in the job structure.  If a job's
 * crontab has an owner then its output is mailed to that owner, otherwise
 * no special provisions are made, so the output will go where cron's output
 * goes.  This keeps root's mailbox from filling up.
 */
{
	pid_t pid;
	int need_mailer;
	int mailfd[2], errfd[2];
	struct passwd *pw;
	crontab_t *tab= job->tab;

	need_mailer= (tab->user != nil);

	if (job->atjob) {
		struct stat st;

		need_mailer= 1;
		if (rename(tab->file, tab->data) < 0) {
			if (errno == ENOENT) {
				/* Normal error, job deleted. */
				need_reload= 1;
			} else {
				/* Bad error, halt processing AT jobs. */
				log(LOG_CRIT, "Can't rename %s: %s\n",
					tab->file, strerror(errno));
				tab_reschedule(job);
			}
			return;
		}
		/* Will need to determine the next AT job. */
		need_reload= 1;

		if (stat(tab->data, &st) < 0) {
			log(LOG_ERR, "Can't stat %s: %s\n",
						tab->data, strerror(errno));
			tab_reschedule(job);
			return;
		}
		if ((pw= getpwuid(st.st_uid)) == nil) {
			log(LOG_ERR, "Unknown owner for uid %lu of AT job %s\n",
				(unsigned long) st.st_uid, job->cmd);
			tab_reschedule(job);
			return;
		}
	} else {
		pw= nil;
		if (job->user != nil && (pw= getpwnam(job->user)) == nil) {
			log(LOG_ERR, "%s: Unknown user\n", job->user);
			tab_reschedule(job);
			return;
		}
	}

	if (need_mailer) {
		errfd[0]= -1;
		if (pipe(errfd) < 0 || pipe(mailfd) < 0) {
			log(LOG_ERR, "pipe() call failed: %s\n",
							strerror(errno));
			if (errfd[0] != -1) {
				close(errfd[0]);
				close(errfd[1]);
			}
			tab_reschedule(job);
			return;
		}
		(void) fcntl(errfd[1], F_SETFD,
				fcntl(errfd[1], F_GETFD) | FD_CLOEXEC);

		if ((pid= fork()) == -1) {
			log(LOG_ERR, "fork() call failed: %s\n",
							strerror(errno));
			close(errfd[0]);
			close(errfd[1]);
			close(mailfd[0]);
			close(mailfd[1]);
			tab_reschedule(job);
			return;
		}

		if (pid == 0) {
			/* Child that is to be the mailer. */
			char subject[70+20], *ps;

			close(errfd[0]);
			close(mailfd[1]);
			if (mailfd[0] != 0) {
				dup2(mailfd[0], 0);
				close(mailfd[0]);
			}

			memset(subject, 0, sizeof(subject));
			sprintf(subject,
				"Output from your %s job: %.50s",
				job->atjob ? "AT" : "cron",
				job->cmd);
			if (subject[70] != 0) {
				strcpy(subject+70-3, "...");
			}
			for (ps= subject; *ps != 0; ps++) {
				if (*ps == '\n') *ps= '%';
			}

			execl("/usr/bin/mail", "mail", "-s", subject,
						pw->pw_name, (char *) nil);
			write(errfd[1], &errno, sizeof(errno));
			_exit(1);
		}

		close(mailfd[0]);
		close(errfd[1]);
		if (read(errfd[0], &errno, sizeof(errno)) > 0) {
			log(LOG_ERR, "can't execute /usr/bin/mail: %s\n",
							strerror(errno));
			close(errfd[0]);
			close(mailfd[1]);
			tab_reschedule(job);
			return;
		}
		close(errfd[0]);
	}

	if (pipe(errfd) < 0) {
		log(LOG_ERR, "pipe() call failed: %s\n", strerror(errno));
		if (need_mailer) close(mailfd[1]);
		tab_reschedule(job);
		return;
	}
	(void) fcntl(errfd[1], F_SETFD, fcntl(errfd[1], F_GETFD) | FD_CLOEXEC);

	if ((pid= fork()) == -1) {
		log(LOG_ERR, "fork() call failed: %s\n", strerror(errno));
		close(errfd[0]);
		close(errfd[1]);
		if (need_mailer) close(mailfd[1]);
		tab_reschedule(job);
		return;
	}

	if (pid == 0) {
		/* Child that is to be the cron job. */
		close(errfd[0]);
		if (need_mailer) {
			if (mailfd[1] != 1) {
				dup2(mailfd[1], 1);
				close(mailfd[1]);
			}
			dup2(1, 2);
		}

		if (pw != nil) {
			/* Change id to the owner of the job. */
			initgroups(pw->pw_name, pw->pw_gid);
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);
			chdir(pw->pw_dir);
			if (setenv("USER", pw->pw_name, 1) < 0) goto bad;
			if (setenv("LOGNAME", pw->pw_name, 1) < 0) goto bad;
			if (setenv("HOME", pw->pw_dir, 1) < 0) goto bad;
			if (setenv("SHELL", pw->pw_shell[0] == 0 ? "/bin/sh"
						: pw->pw_shell, 1) < 0) goto bad;
		}

		if (job->atjob) {
			execl("/bin/sh", "sh", tab->data, (char *) nil);
		} else {
			execl("/bin/sh", "sh", "-c", job->cmd, (char *) nil);
		}
	    bad:
		write(errfd[1], &errno, sizeof(errno));
		_exit(1);
	}

	if (need_mailer) close(mailfd[1]);
	close(errfd[1]);
	if (read(errfd[0], &errno, sizeof(errno)) > 0) {
		log(LOG_ERR, "can't execute /bin/sh: %s\n", strerror(errno));
		close(errfd[0]);
		tab_reschedule(job);
		return;
	}
	close(errfd[0]);
	job->pid= pid;
	if (debug >= 1) fprintf(stderr, "executing >%s<, pid = %ld\n",
						job->cmd, (long) job->pid);
}

static void load_crontabs(void)
/* Load all the crontabs we like to run.  We didn't bother to make a list in
 * an array or something, this is too system specific to make nice.
 */
{
	DIR *spool;
#if __minix_vmd
	FILE *pkgs;
#endif

	tab_parse("/usr/lib/crontab", nil);
	tab_parse("/usr/local/lib/crontab", nil);
	tab_parse("/var/lib/crontab", nil);

#if __minix_vmd
	if ((pkgs= fopen("/usr/lib/packages", "r")) != nil) {
		char name[NAME_MAX+1];
		char *np;
		int c;
		char tab[sizeof("/var/opt//lib/crontab") + NAME_MAX];

		while ((c= fgetc(pkgs)) != EOF) {
			np= name;
			while (c != EOF && c != '/' && c != '\n') {
				if (np < name+NAME_MAX) *np++ = c;
				c= fgetc(pkgs);
			}
			*np= 0;
			while (c != EOF && c != '\n') c= fgetc(pkgs);

			if (name[0] == 0) continue;	/* ? */

			strcpy(tab, "/var/opt/");
			strcat(tab, name);
			strcat(tab, "/lib/crontab");
			tab_parse(tab, nil);
		}
		if (ferror(pkgs)) {
			log(LOG_CRIT, "/usr/lib/packages: %s\n",
							strerror(errno));
		}
		fclose(pkgs);
	} else {
		if (errno != ENOENT) {
			log(LOG_ERR, "/usr/lib/packages: %s\n",
							strerror(errno));
		}
	}
#endif /* Minix-vmd */

	if ((spool= opendir("/usr/spool/crontabs")) != nil) {
		struct dirent *entry;
		char tab[sizeof("/usr/spool/crontabs/") + NAME_MAX];

		while ((entry= readdir(spool)) != nil) {
			if (entry->d_name[0] == '.') continue;

			strcpy(tab, "/usr/spool/crontabs/");
			strcat(tab, entry->d_name);
			tab_parse(tab, entry->d_name);
		}
		closedir(spool);
	}

	/* Find the first to be executed AT job. */
	tab_find_atjob("/usr/spool/at");

	tab_purge();
	if (debug >= 2) {
		tab_print(stderr);
		fprintf(stderr, "%lu memory chunks in use\n",
			(unsigned long) alloc_count);
	}
}

static void handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		need_reload= 1;
		break;
	case SIGINT:
	case SIGTERM:
		need_quit= 1;
		break;
	case SIGUSR1:
		debug++;
		break;
	case SIGUSR2:
		debug= 0;
		break;
	}
	alarm(1);	/* A signal may come just before a blocking call. */
	busy= 1;
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-d[#]]\n", prog_name);
	exit(1);
}

int main(int argc, char **argv)
{
	int i;
	struct sigaction sa, osa;
	FILE *pf;
	int r;

	prog_name= strrchr(argv[0], '/');
	if (prog_name == nil) prog_name= argv[0]; else prog_name++;

	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++] + 1;

		if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

		while (*opt != 0) switch (*opt++) {
		case 'd':
			if (*opt == 0) {
				debug= 1;
			} else {
				debug= strtoul(opt, &opt, 10);
				if (*opt != 0) usage();
			}
			break;
		default:
			usage();
		}
	}
	if (i != argc) usage();

	selectlog(SYSLOG);
	openlog(prog_name, LOG_PID, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_INFO));

	/* Save process id. */
	if ((pf= fopen(PIDFILE, "w")) == NULL) {
		fprintf(stderr, "%s: %s\n", PIDFILE, strerror(errno));
		exit(1);
	}
	fprintf(pf, "%d\n", getpid());
	if (ferror(pf) || fclose(pf) == EOF) {
		fprintf(stderr, "%s: %s\n", PIDFILE, strerror(errno));
		exit(1);
	}

	sigemptyset(&sa.sa_mask);
	sa.sa_flags= 0;
	sa.sa_handler= handler;

	/* Hangup: Reload crontab files. */
	sigaction(SIGHUP, &sa, nil);

	/* User signal 1 & 2: Raise or reset debug level. */
	sigaction(SIGUSR1, &sa, nil);
	sigaction(SIGUSR2, &sa, nil);

	/* Interrupt and Terminate: Cleanup and exit. */
	if (sigaction(SIGINT, nil, &osa) == 0 && osa.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &sa, nil);
	}
	if (sigaction(SIGTERM, nil, &osa) == 0 && osa.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &sa, nil);
	}

	/* Alarm: Wake up and run a job. */
	sigaction(SIGALRM, &sa, nil);

	/* Initialize current time and time next to do something. */
	time(&now);
	next= NEVER;

	/* Table load required first time. */
	need_reload= 1;

	do {
		if (need_reload) {
			need_reload= 0;
			load_crontabs();
			busy= 1;
		}

		/* Run jobs whose time has come. */
		if (next <= now) {
			cronjob_t *job;

			if ((job= tab_nextjob()) != nil) run_job(job);
			busy= 1;
		}

		if (busy) {
			/* Did a job finish? */
			r= waitpid(-1, nil, WNOHANG);
			busy= 0;
		} else {
			/* Sleep until the next job must be started. */
			if (next == NEVER) {
				alarm(0);
			} else {
#if __minix_vmd
				struct timeval tvnext;

				tvnext.tv_sec= next;
				tvnext.tv_usec= 0;
				sysutime(UTIME_SETALARM, &tvnext);
#else
				alarm((next - now) > INT_MAX
						? INT_MAX : (next - now));
#endif
			}
			if (debug >= 1) fprintf(stderr, "%s: sleep until %s",
						prog_name, ctime(&next));

			closelog();	/* Don't keep resources open. */

			/* Wait for a job to exit or a timeout. */
			r= waitpid(-1, nil, 0);
			if (r == -1 && errno == ECHILD) pause();
			alarm(0);
			time(&now);
		}

		if (r > 0) {
			/* A job has finished, reschedule it. */
			if (debug >= 1) fprintf(stderr, "pid %d has exited\n",
									r);
			tab_reap_job((pid_t) r);
			busy= 1;
		}
	} while (!need_quit);

	/* Remove the pid file to signal that cron is gone. */
	unlink(PIDFILE);

	return 0;
}

/*
 * $PchId: cron.c,v 1.4 2000/07/17 19:00:35 philip Exp $
 */

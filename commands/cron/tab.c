/*	tab.c - process crontabs and create in-core crontab data
 *							Author: Kees J. Bot
 *								7 Dec 1996
 * Changes:
 * 17 Jul 2000 by Philip Homburg
 *	- Tab_reschedule() rewritten (and fixed).
 */
#define nil ((void*)0)
#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "misc.h"
#include "tab.h"

static int nextbit(bitmap_t map, int bit)
/* Return the next bit set in 'map' from 'bit' onwards, cyclic. */
{
	for (;;) {
		bit= (bit+1) & 63;
		if (bit_isset(map, bit)) break;
	}
	return bit;
}

void tab_reschedule(cronjob_t *job)
/* Reschedule one job.  Compute the next time to run the job in job->rtime.
 */
{
	struct tm prevtm, nexttm, tmptm;
	time_t nodst_rtime, dst_rtime;

	/* AT jobs are only run once. */
	if (job->atjob) { job->rtime= NEVER; return; }

	/* Was the job scheduled late last time? */
	if (job->late) job->rtime= now;

	prevtm= *localtime(&job->rtime);
	prevtm.tm_sec= 0;

	nexttm= prevtm;
	nexttm.tm_min++;	/* Minimal increment */

	for (;;)
	{
		if (nexttm.tm_min > 59)
		{
			nexttm.tm_min= 0;
			nexttm.tm_hour++;
		}
		if (nexttm.tm_hour > 23)
		{
			nexttm.tm_min= 0;
			nexttm.tm_hour= 0;
			nexttm.tm_mday++;
		}
		if (nexttm.tm_mday > 31)
		{
			nexttm.tm_hour= nexttm.tm_min= 0;
			nexttm.tm_mday= 1;
			nexttm.tm_mon++;
		}
		if (nexttm.tm_mon >= 12)
		{
			nexttm.tm_hour= nexttm.tm_min= 0;
			nexttm.tm_mday= 1;
			nexttm.tm_mon= 0;
			nexttm.tm_year++;
		}

		/* Verify tm_year. A crontab entry cannot restrict tm_year
		 * directly. However, certain dates (such as Feb, 29th) do
		 * not occur every year. We limit the difference between
		 * nexttm.tm_year and prevtm.tm_year to detect impossible dates
		 * (e.g, Feb, 31st). In theory every date occurs within a
		 * period of 4 years. However, some years at the end of a 
		 * century are not leap years (e.g, the year 2100). An extra
		 * factor of 2 should be enough.
		 */
		if (nexttm.tm_year-prevtm.tm_year > 2*4)
		{
			job->rtime= NEVER;
			return;			/* Impossible combination */
		}

		if (!job->do_wday)
		{
			/* Verify the mon and mday fields. If do_wday and
			 * do_mday are both true we have to merge both
			 * schedules. This is done after the call to mktime.
			 */
			if (!bit_isset(job->mon, nexttm.tm_mon))
			{
				/* Clear other fields */
				nexttm.tm_mday= 1;
				nexttm.tm_hour= nexttm.tm_min= 0;

				nexttm.tm_mon++;
				continue;
			}

			/* Verify mday */
			if (!bit_isset(job->mday, nexttm.tm_mday))
			{
				/* Clear other fields */
				nexttm.tm_hour= nexttm.tm_min= 0;

				nexttm.tm_mday++;
				continue;
			}
		}

		/* Verify hour */
		if (!bit_isset(job->hour, nexttm.tm_hour))
		{
			/* Clear tm_min field */
			nexttm.tm_min= 0;

			nexttm.tm_hour++;
			continue;
		}

		/* Verify min */
		if (!bit_isset(job->min, nexttm.tm_min))
		{
			nexttm.tm_min++;
			continue;
		}

		/* Verify that we don't have any problem with DST. Try
		 * tm_isdst=0 first. */
		tmptm= nexttm;
		tmptm.tm_isdst= 0;
#if 0
		fprintf(stderr, 
	"tab_reschedule: trying %04d-%02d-%02d %02d:%02d:%02d isdst=0\n",
				1900+nexttm.tm_year, nexttm.tm_mon+1,
				nexttm.tm_mday, nexttm.tm_hour,
				nexttm.tm_min, nexttm.tm_sec);
#endif
		nodst_rtime= job->rtime= mktime(&tmptm);
		if (job->rtime == -1) {
			/* This should not happen. */
			log(LOG_ERR,
			"mktime failed for %04d-%02d-%02d %02d:%02d:%02d",
				1900+nexttm.tm_year, nexttm.tm_mon+1,
				nexttm.tm_mday, nexttm.tm_hour,
				nexttm.tm_min, nexttm.tm_sec);
			job->rtime= NEVER;
			return;	
		}
		tmptm= *localtime(&job->rtime);
		if (tmptm.tm_hour != nexttm.tm_hour ||
			tmptm.tm_min != nexttm.tm_min)
		{
			assert(tmptm.tm_isdst);
			tmptm= nexttm;
			tmptm.tm_isdst= 1;
#if 0
			fprintf(stderr, 
	"tab_reschedule: trying %04d-%02d-%02d %02d:%02d:%02d isdst=1\n",
				1900+nexttm.tm_year, nexttm.tm_mon+1,
				nexttm.tm_mday, nexttm.tm_hour,
				nexttm.tm_min, nexttm.tm_sec);
#endif
			dst_rtime= job->rtime= mktime(&tmptm);
			if (job->rtime == -1) {
				/* This should not happen. */
				log(LOG_ERR,
			"mktime failed for %04d-%02d-%02d %02d:%02d:%02d\n",
					1900+nexttm.tm_year, nexttm.tm_mon+1,
					nexttm.tm_mday, nexttm.tm_hour,
					nexttm.tm_min, nexttm.tm_sec);
				job->rtime= NEVER;
				return;	
			}
			tmptm= *localtime(&job->rtime);
			if (tmptm.tm_hour != nexttm.tm_hour ||
				tmptm.tm_min != nexttm.tm_min)
			{
				assert(!tmptm.tm_isdst);
				/* We have a problem. This time neither
				 * exists with DST nor without DST.
				 * Use the latest time, which should be
				 * nodst_rtime.
				 */
				assert(nodst_rtime > dst_rtime);
				job->rtime= nodst_rtime;
#if 0
				fprintf(stderr,
			"During DST trans. %04d-%02d-%02d %02d:%02d:%02d\n",
					1900+nexttm.tm_year, nexttm.tm_mon+1,
					nexttm.tm_mday, nexttm.tm_hour,
					nexttm.tm_min, nexttm.tm_sec);
#endif
			}
		}

		/* Verify this the combination (tm_year, tm_mon, tm_mday). */
		if (tmptm.tm_mday != nexttm.tm_mday ||
			tmptm.tm_mon != nexttm.tm_mon ||
			tmptm.tm_year != nexttm.tm_year)
		{
			/* Wrong day */
#if 0
			fprintf(stderr, "Wrong day\n");
#endif
			nexttm.tm_hour= nexttm.tm_min= 0;
			nexttm.tm_mday++;
			continue;
		}

		/* Check tm_wday */
		if (job->do_wday && bit_isset(job->wday, tmptm.tm_wday))
		{
			/* OK, wday matched */
			break;
		}

		/* Check tm_mday */
		if (job->do_mday && bit_isset(job->mon, tmptm.tm_mon) &&
			bit_isset(job->mday, tmptm.tm_mday))
		{
			/* OK, mon and mday matched */
			break;
		}

		if (!job->do_wday && !job->do_mday)
		{
			/* No need to match wday and mday */
			break;
		}

		/* Wrong day */
#if 0
		fprintf(stderr, "Wrong mon+mday or wday\n");
#endif
		nexttm.tm_hour= nexttm.tm_min= 0;
		nexttm.tm_mday++;
	}
#if 0
	fprintf(stderr, "Using %04d-%02d-%02d %02d:%02d:%02d \n",
		1900+nexttm.tm_year, nexttm.tm_mon+1, nexttm.tm_mday,
		nexttm.tm_hour, nexttm.tm_min, nexttm.tm_sec);
	tmptm= *localtime(&job->rtime);
	fprintf(stderr, "Act. %04d-%02d-%02d %02d:%02d:%02d isdst=%d\n",
		1900+tmptm.tm_year, tmptm.tm_mon+1, tmptm.tm_mday,
		tmptm.tm_hour, tmptm.tm_min, tmptm.tm_sec,
		tmptm.tm_isdst);
#endif


	/* Is job issuing lagging behind with the progress of time? */
	job->late= (job->rtime < now);

  	/* The result is in job->rtime. */
  	if (job->rtime < next) next= job->rtime;
}

#define isdigit(c)	((unsigned) ((c) - '0') < 10)

static char *get_token(char **ptr)
/* Return a pointer to the next token in a string.  Move *ptr to the end of
 * the token, and return a pointer to the start.  If *ptr == start of token
 * then we're stuck against a newline or end of string.
 */
{
	char *start, *p;

	p= *ptr;
	while (*p == ' ' || *p == '\t') p++;

	start= p;
	while (*p != ' ' && *p != '\t' && *p != '\n' && *p != 0) p++;
	*ptr= p;
	return start;
}

static int range_parse(char *file, char *data, bitmap_t map,
	int min, int max, int *wildcard)
/* Parse a comma separated series of 'n', 'n-m' or 'n:m' numbers.  'n'
 * includes number 'n' in the bit map, 'n-m' includes all numbers between
 * 'n' and 'm' inclusive, and 'n:m' includes 'n+k*m' for any k if in range.
 * Numbers must fall between 'min' and 'max'.  A '*' means all numbers.  A
 * '?' is allowed as a synonym for the current minute, which only makes sense
 * in the minute field, i.e. max must be 59.  Return true iff parsed ok.
 */
{
	char *p;
	int end;
	int n, m;

	/* Clear all bits. */
	for (n= 0; n < 8; n++) map[n]= 0;

	p= data;
	while (*p != ' ' && *p != '\t' && *p != '\n' && *p != 0) p++;
	end= *p;
	*p= 0;
	p= data;

	if (*p == 0) {
		log(LOG_ERR, "%s: not enough time fields\n", file);
		return 0;
	}

	/* Is it a '*'? */
	if (p[0] == '*' && p[1] == 0) {
		for (n= min; n <= max; n++) bit_set(map, n);
		p[1]= end;
		*wildcard= 1;
		return 1;
	}
	*wildcard= 0;

	/* Parse a comma separated series of numbers or ranges. */
	for (;;) {
		if (*p == '?' && max == 59 && p[1] != '-') {
			n= localtime(&now)->tm_min;
			p++;
		} else {
			if (!isdigit(*p)) goto syntax;
			n= 0;
			do {
				n= 10 * n + (*p++ - '0');
				if (n > max) goto range;
			} while (isdigit(*p));
		}
		if (n < min) goto range;

		if (*p == '-') {	/* A range of the form 'n-m'? */
			p++;
			if (!isdigit(*p)) goto syntax;
			m= 0;
			do {
				m= 10 * m + (*p++ - '0');
				if (m > max) goto range;
			} while (isdigit(*p));
			if (m < n) goto range;
			do {
				bit_set(map, n);
			} while (++n <= m);
		} else
		if (*p == ':') {	/* A repeat of the form 'n:m'? */
			p++;
			if (!isdigit(*p)) goto syntax;
			m= 0;
			do {
				m= 10 * m + (*p++ - '0');
				if (m > (max-min+1)) goto range;
			} while (isdigit(*p));
			if (m == 0) goto range;
			while (n >= min) n-= m;
			while ((n+= m) <= max) bit_set(map, n);
		} else {
					/* Simply a number */
			bit_set(map, n);
		}
		if (*p == 0) break;
		if (*p++ != ',') goto syntax;
	}
	*p= end;
	return 1;
  syntax:
	log(LOG_ERR, "%s: field '%s': bad syntax for a %d-%d time field\n",
		file, data, min, max);
	return 0;
  range:
	log(LOG_ERR, "%s: field '%s': values out of the %d-%d allowed range\n",
		file, data, min, max);
	return 0;
}

void tab_parse(char *file, char *user)
/* Parse a crontab file and add its data to the tables.  Handle errors by
 * yourself.  Table is owned by 'user' if non-null.
 */
{
	crontab_t **atab, *tab;
	cronjob_t **ajob, *job;
	int fd;
	struct stat st;
	char *p, *q;
	size_t n;
	ssize_t r;
	int ok, wc;

	for (atab= &crontabs; (tab= *atab) != nil; atab= &tab->next) {
		if (strcmp(file, tab->file) == 0) break;
	}

	/* Try to open the file. */
	if ((fd= open(file, O_RDONLY)) < 0 || fstat(fd, &st) < 0) {
		if (errno != ENOENT) {
			log(LOG_ERR, "%s: %s\n", file, strerror(errno));
		}
		if (fd != -1) close(fd);
		return;
	}

	/* Forget it if the file is awfully big. */
	if (st.st_size > TAB_MAX) {
		log(LOG_ERR, "%s: %lu bytes is bigger than my %lu limit\n",
			file,
			(unsigned long) st.st_size,
			(unsigned long) TAB_MAX);
		return;
	}

	/* If the file is the same as before then don't bother. */
	if (tab != nil && st.st_mtime == tab->mtime) {
		close(fd);
		tab->current= 1;
		return;
	}

	/* Create a new table structure. */
	tab= allocate(sizeof(*tab));
	tab->file= allocate((strlen(file) + 1) * sizeof(tab->file[0]));
	strcpy(tab->file, file);
	tab->user= nil;
	if (user != nil) {
		tab->user= allocate((strlen(user) + 1) * sizeof(tab->user[0]));
		strcpy(tab->user, user);
	}
	tab->data= allocate((st.st_size + 1) * sizeof(tab->data[0]));
	tab->jobs= nil;
	tab->mtime= st.st_mtime;
	tab->current= 0;
	tab->next= *atab;
	*atab= tab;

	/* Pull a new table in core. */
	n= 0;
	while (n < st.st_size) {
		if ((r = read(fd, tab->data + n, st.st_size - n)) < 0) {
			log(LOG_CRIT, "%s: %s", file, strerror(errno));
			close(fd);
			return;
		}
		if (r == 0) break;
		n+= r;
	}
	close(fd);
	tab->data[n]= 0;
	if (strlen(tab->data) < n) {
		log(LOG_ERR, "%s contains a null character\n", file);
		return;
	}

	/* Parse the file. */
	ajob= &tab->jobs;
	p= tab->data;
	ok= 1;
	while (ok && *p != 0) {
		q= get_token(&p);
		if (*q == '#' || q == p) {
			/* Comment or empty. */
			while (*p != 0 && *p++ != '\n') {}
			continue;
		}

		/* One new job coming up. */
		*ajob= job= allocate(sizeof(*job));
		*(ajob= &job->next)= nil;
		job->tab= tab;

		if (!range_parse(file, q, job->min, 0, 59, &wc)) {
			ok= 0;
			break;
		}

		q= get_token(&p);
		if (!range_parse(file, q, job->hour, 0, 23, &wc)) {
			ok= 0;
			break;
		}

		q= get_token(&p);
		if (!range_parse(file, q, job->mday, 1, 31, &wc)) {
			ok= 0;
			break;
		}
		job->do_mday= !wc;

		q= get_token(&p);
		if (!range_parse(file, q, job->mon, 1, 12, &wc)) {
			ok= 0;
			break;
		}
		job->do_mday |= !wc;

		q= get_token(&p);
		if (!range_parse(file, q, job->wday, 0, 7, &wc)) {
			ok= 0;
			break;
		}
		job->do_wday= !wc;

		/* 7 is Sunday, but 0 is a common mistake because it is in the
		 * tm_wday range.  We allow and even prefer it internally.
		 */
		if (bit_isset(job->wday, 7)) {
			bit_clr(job->wday, 7);
			bit_set(job->wday, 0);
		}

		/* The month range is 1-12, but tm_mon likes 0-11. */
		job->mon[0] >>= 1;
		if (bit_isset(job->mon, 8)) bit_set(job->mon, 7);
		job->mon[1] >>= 1;

		/* Scan for options. */
		job->user= nil;
		while (q= get_token(&p), *q == '-') {
			q++;
			if (q[0] == '-' && q+1 == p) {
				/* -- */
				q= get_token(&p);
				break;
			}
			while (q < p) switch (*q++) {
			case 'u':
				if (q == p) q= get_token(&p);
				if (q == p) goto usage;
				memmove(q-1, q, p-q);	/* gross... */
				p[-1]= 0;
				job->user= q-1;
				q= p;
				break;
			default:
			usage:
				log(LOG_ERR,
			"%s: bad option -%c, good options are: -u username\n",
					file, q[-1]);
				ok= 0;
				goto endtab;
			}
		}

		/* A crontab owned by a user can only do things as that user. */
		if (tab->user != nil) job->user= tab->user;

		/* Inspect the first character of the command. */
		job->cmd= q;
		if (q == p || *q == '#') {
			/* Rest of the line is empty, i.e. the commands are on
			 * the following lines indented by one tab.
			 */
			while (*p != 0 && *p++ != '\n') {}
			if (*p++ != '\t') {
				log(LOG_ERR, "%s: contains an empty command\n",
					file);
				ok= 0;
				goto endtab;
			}
			while (*p != 0) {
				if ((*q = *p++) == '\n') {
					if (*p != '\t') break;
					p++;
				}
				q++;
			}
		} else {
			/* The command is on this line.  Alas we must now be
			 * backwards compatible and change %'s to newlines.
			 */
			p= q;
			while (*p != 0) {
				if ((*q = *p++) == '\n') break;
				if (*q == '%') *q= '\n';
				q++;
			}
		}
		*q= 0;
		job->rtime= now;
		job->late= 0;		/* It is on time. */
		job->atjob= 0;		/* True cron job. */
		job->pid= IDLE_PID;	/* Not running yet. */
		tab_reschedule(job);	/* Compute next time to run. */
	}
  endtab:

	if (ok) tab->current= 1;
}

void tab_find_atjob(char *atdir)
/* Find the first to be executed AT job and kludge up an crontab job for it.
 * We set tab->file to "atdir/jobname", tab->data to "atdir/past/jobname",
 * and job->cmd to "jobname".
 */
{
	DIR *spool;
	struct dirent *entry;
	time_t t0, t1;
	struct tm tmnow, tmt1;
	static char template[] = "96.365.1546.00";
	char firstjob[sizeof(template)];
	int i;
	crontab_t *tab;
	cronjob_t *job;

	if ((spool= opendir(atdir)) == nil) return;

	tmnow= *localtime(&now);
	t0= NEVER;

	while ((entry= readdir(spool)) != nil) {
		/* Check if the name fits the template. */
		for (i= 0; template[i] != 0; i++) {
			if (template[i] == '.') {
				if (entry->d_name[i] != '.') break;
			} else {
				if (!isdigit(entry->d_name[i])) break;
			}
		}
		if (template[i] != 0 || entry->d_name[i] != 0) continue;

		/* Convert the name to a time.  Careful with the century. */
		memset(&tmt1, 0, sizeof(tmt1));
		tmt1.tm_year= atoi(entry->d_name+0);
		while (tmt1.tm_year < tmnow.tm_year-10) tmt1.tm_year+= 100;
		tmt1.tm_mday= 1+atoi(entry->d_name+3);
		tmt1.tm_min= atoi(entry->d_name+7);
		tmt1.tm_hour= tmt1.tm_min / 100;
		tmt1.tm_min%= 100;
		tmt1.tm_isdst= -1;
		if ((t1= mktime(&tmt1)) == -1) {
			/* Illegal time?  Try in winter time. */
			tmt1.tm_isdst= 0;
			if ((t1= mktime(&tmt1)) == -1) continue;
		}
		if (t1 < t0) {
			t0= t1;
			strcpy(firstjob, entry->d_name);
		}
	}
	closedir(spool);

	if (t0 == NEVER) return;	/* AT job spool is empty. */

	/* Create new table and job structures. */
	tab= allocate(sizeof(*tab));
	tab->file= allocate((strlen(atdir) + 1 + sizeof(template))
						* sizeof(tab->file[0]));
	strcpy(tab->file, atdir);
	strcat(tab->file, "/");
	strcat(tab->file, firstjob);
	tab->data= allocate((strlen(atdir) + 6 + sizeof(template))
						* sizeof(tab->data[0]));
	strcpy(tab->data, atdir);
	strcat(tab->data, "/past/");
	strcat(tab->data, firstjob);
	tab->user= nil;
	tab->mtime= 0;
	tab->current= 1;
	tab->next= crontabs;
	crontabs= tab;

	tab->jobs= job= allocate(sizeof(*job));
	job->next= nil;
	job->tab= tab;
	job->user= nil;
	job->cmd= tab->data + strlen(atdir) + 6;
	job->rtime= t0;
	job->late= 0;
	job->atjob= 1;		/* One AT job disguised as a cron job. */
	job->pid= IDLE_PID;

	if (job->rtime < next) next= job->rtime;
}

void tab_purge(void)
/* Remove table data that is no longer current.  E.g. a crontab got removed.
 */
{
	crontab_t **atab, *tab;
	cronjob_t *job;

	atab= &crontabs;
	while ((tab= *atab) != nil) {
		if (tab->current) {
			/* Table is fine. */
			tab->current= 0;
			atab= &tab->next;
		} else {
			/* Table is not, or no longer valid; delete. */
			*atab= tab->next;
			while ((job= tab->jobs) != nil) {
				tab->jobs= job->next;
				deallocate(job);
			}
			deallocate(tab->data);
			deallocate(tab->file);
			deallocate(tab->user);
			deallocate(tab);
		}
	}
}

static cronjob_t *reap_or_find(pid_t pid)
/* Find a finished job or search for the next one to run. */
{
	crontab_t *tab;
	cronjob_t *job;
	cronjob_t *nextjob;

	nextjob= nil;
	next= NEVER;
	for (tab= crontabs; tab != nil; tab= tab->next) {
		for (job= tab->jobs; job != nil; job= job->next) {
			if (job->pid == pid) {
				job->pid= IDLE_PID;
				tab_reschedule(job);
			}
			if (job->pid != IDLE_PID) continue;
			if (job->rtime < next) next= job->rtime;
			if (job->rtime <= now) nextjob= job;
		}
	}
	return nextjob;
}

void tab_reap_job(pid_t pid)
/* A job has finished.  Try to find it among the crontab data and reschedule
 * it.  Recompute time next to run a job.
 */
{
	(void) reap_or_find(pid);
}

cronjob_t *tab_nextjob(void)
/* Find a job that should be run now.  If none are found return null.
 * Update 'next'.
 */
{
	return reap_or_find(NO_PID);
}

static void pr_map(FILE *fp, bitmap_t map)
{
	int last_bit= -1, bit;
	char *sep;

	sep= "";
	for (bit= 0; bit < 64; bit++) {
		if (bit_isset(map, bit)) {
			if (last_bit == -1) last_bit= bit;
		} else {
			if (last_bit != -1) {
				fprintf(fp, "%s%d", sep, last_bit);
				if (last_bit != bit-1) {
					fprintf(fp, "-%d", bit-1);
				}
				last_bit= -1;
				sep= ",";
			}
		}
	}
}

void tab_print(FILE *fp)
/* Print out a stored crontab file for debugging purposes. */
{
	crontab_t *tab;
	cronjob_t *job;
	char *p;
	struct tm tm;

	for (tab= crontabs; tab != nil; tab= tab->next) {
		fprintf(fp, "tab->file = \"%s\"\n", tab->file);
		fprintf(fp, "tab->user = \"%s\"\n",
				tab->user == nil ? "(root)" : tab->user);
		fprintf(fp, "tab->mtime = %s", ctime(&tab->mtime));

		for (job= tab->jobs; job != nil; job= job->next) {
			if (job->atjob) {
				fprintf(fp, "AT job");
			} else {
				pr_map(fp, job->min); fputc(' ', fp);
				pr_map(fp, job->hour); fputc(' ', fp);
				pr_map(fp, job->mday); fputc(' ', fp);
				pr_map(fp, job->mon); fputc(' ', fp);
				pr_map(fp, job->wday);
			}
			if (job->user != nil && job->user != tab->user) {
				fprintf(fp, " -u %s", job->user);
			}
			fprintf(fp, "\n\t");
			for (p= job->cmd; *p != 0; p++) {
				fputc(*p, fp);
				if (*p == '\n') fputc('\t', fp);
			}
			fputc('\n', fp);
			tm= *localtime(&job->rtime);
			fprintf(fp, "    rtime = %.24s%s\n", asctime(&tm),
				tm.tm_isdst ? " (DST)" : "");
			if (job->pid != IDLE_PID) {
				fprintf(fp, "    pid = %ld\n", (long) job->pid);
			}
		}
	}
}

/*
 * $PchId: tab.c,v 1.5 2000/07/25 22:07:51 philip Exp $
 */

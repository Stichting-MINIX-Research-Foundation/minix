/* utility.c
 *
 * This file is part of httpd
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 Initial Release	Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utility.h"
#include "config.h"

const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

char *logdate(t)
time_t *t;
{
time_t worktime;
struct tm *tm;
static char datebuffer[80];

  if(t == (time_t *)NULL)
	(void) time(&worktime);
  else
  	worktime = *t;

   tm = localtime(&worktime);

   sprintf(datebuffer, "%4d%02d%02d%02d%02d%02d",
		1900+tm->tm_year,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

   return(datebuffer);
}

char *httpdate(t)
time_t *t;
{
time_t worktime;
struct tm *tm;
static char datebuffer[80];

  if(t == (time_t *)NULL)
	(void) time(&worktime);
  else
  	worktime = *t;

   tm = gmtime(&worktime);

   sprintf(datebuffer, "%s, %02d %s %4d %02d:%02d:%02d GMT",
   		days[tm->tm_wday],
		tm->tm_mday, months[tm->tm_mon], 1900+tm->tm_year,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

   return(datebuffer);
}

time_t httptime(p)
char *p;
{
time_t worktime, gtime, ltime;
struct tm tm;
struct tm *tm2;
int i;

   worktime = (time_t) -1;

   tm.tm_yday = 0;
   tm.tm_isdst = -1;

   /* day of week */
   for(i = 0; i < 7; i++)
	if(!strncmp(p, days[i], 3)) break;
   if(i < 7)
	tm.tm_wday = i;
   else
	return(worktime);
   while(*p && *p != ' ') p++;
   if(!*p) return(worktime);
   while(*p && *p == ' ') p++;
   if(!*p) return(worktime);

   if(*p >= '0' && *p <= '9') {
   	/* day */
   	if(*(p+1) >= '0' && *(p+1) <= '9')
		tm.tm_mday = 10 * (*p - '0') + (*(p+1) - '0');
	else
		return(worktime);
	p += 3;
	/* month */
	for(i = 0; i < 12; i++)
		if(!strncmp(p, months[i], 3)) break;
	if(i < 12)
		tm.tm_mon = i;
	else
		return(worktime);
	p += 3;
	if(!*p++) return(worktime);
	/* year */
	tm.tm_year = atoi(p);
	while(*p && *p != ' ') p++;
	if(*p) p++;
   } else {
	/* day */
	tm.tm_mday = atoi(p);
	while(*p && *p != ' ') p++;
	while(*p && *p == ' ') p++;
	if(!*p) return(worktime);
   }

   /* hour */
   if(*p < '0' || *p > '9' || *(p+1) < '0' || *(p+1) > '9' || *(p+2) != ':') return(worktime);
   tm.tm_hour = 10 * (*p - '0') + (*(p+1) - '0');
   p += 3;

   /* minute */
   if(*p < '0' || *p > '9' || *(p+1) < '0' || *(p+1) > '9' || *(p+2) != ':') return(worktime);
   tm.tm_min  = 10 * (*p - '0') + (*(p+1) - '0');
   p += 3;

   /* second */
   if(*p < '0' || *p > '9' || *(p+1) < '0' || *(p+1) > '9' || *(p+2) != ' ') return(worktime);
   tm.tm_sec  = 10 * (*p - '0') + (*(p+1) - '0');
   p += 3;
   while(*p && *p == ' ') p++;
   if(!*p) return(worktime);

   if(*p >= '0' && *p <= '9')
   	tm.tm_year = atoi(p);
   else
   	if(*p++ != 'G' || *p++ != 'M' || *p++ != 'T')
   		return(worktime);

   if(tm.tm_year == 0)
   	return(worktime);

   if(tm.tm_year > 1900)
	tm.tm_year -= 1900;

   worktime = mktime(&tm);

   gtime = mktime(gmtime(&worktime));
   tm2 = localtime(&worktime);
   tm2->tm_isdst = 0;
   ltime = mktime(tm2);

   worktime = worktime - (gtime - ltime);

   return(worktime);
}

char *mimetype(url)
char *url;
{
char *p;
struct msufx *ps;
char *dmt;

   dmt = (char *) NULL;
   p = url;
   while(*p) {
   	if(*p != '.') {
   		p++;
   		continue;
   	}
   	for(ps = msufx; ps != NULL; ps = ps->snext)
   		if(!strcmp(ps->suffix, "") && dmt == (char *) NULL)
   			dmt = ps->mtype->mimetype;
   		else
   			if(!strcmp(p, ps->suffix))
   				return(ps->mtype->mimetype);
   	p++;
   }

   if(dmt == (char *) NULL)
   	dmt = "application/octet-stream";

   return(dmt);
}

char *decode64(p)
char *p;
{
static char decode[80];
char c[4];
int i;
int d;

   i = 0;
   d = 0;

   while(*p) {
   	if(*p >= 'A' && *p <= 'Z') c[i++] = *p++ - 'A'; else
   	if(*p >= 'a' && *p <= 'z') c[i++] = *p++ - 'a' + 26; else
   	if(*p >= '0' && *p <= '9') c[i++] = *p++ - '0' + 52; else
   	if(*p == '+') c[i++] = *p++ - '+' + 62; else
   	if(*p == '/') c[i++] = *p++ - '/' + 63; else
   	if(*p == '=') c[i++] = *p++ - '='; else
   		return("");
   	if(i < 4) continue;
   	decode[d++] = ((c[0] << 2) | (c[1] >> 4));
   	decode[d++] = ((c[1] << 4) | (c[2] >> 2));
   	decode[d++] = ((c[2] << 6) |  c[3]);
   	decode[d] = '\0';
   	i = 0;
   }

   return(decode);
}

int getparms(p, parms, maxparms)
char *p;
char *parms[];
int maxparms;
{
int np;

   np = 0;

   if(LWS(*p)) {
   	while(*p && LWS(*p)) p++;
   	if(!*p) return(0);
   	parms[np++] = (char *)NULL;
   } else
   	np = 0;

   while(np < maxparms && *p) {
   	parms[np++] = p;
   	while(*p && !LWS(*p)) p++;
   	if(*p) *p++ = '\0';
   	while(*p && LWS(*p)) p++;
   }

   return(np);
}

int mkurlaccess(p)
char *p;
{
int ua;

   ua = 0;

   while(*p) {
	if(toupper(*p) == 'R') ua |= URLA_READ; else
	if(toupper(*p) == 'W') ua |= URLA_WRITE; else
	if(toupper(*p) == 'X') ua |= URLA_EXEC; else
	if(toupper(*p) == 'H') ua |= URLA_HEADERS; else
		return(0);
	p++;
   }

   return(ua);
}

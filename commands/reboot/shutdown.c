/*
  shutdown - close down the system graciously

  Author: Edvard Tuinder  <v892231@si.hhs.NL>

  This program informs the users that the system is going
  down, when and why. After that a shutdown notice is written in
  both /usr/adm/wtmp and by syslog(3).  Then reboot(2) is called
  to really close the system.

  This actually is a ``nice'' halt(8).

  Options are supposed to be as with BSD
   -h: shutdown and halt the system
   -r: shutdown and reboot
   -k: stop an already running shutdown
   -o: obsolete: not implemented

  New Minix options:
   -C: crash check, i.e. is the last wtmp entry a shutdown entry?
   -R: reset the system
   -d: default CTRL-ALT-DEL shutdown for current bootloader
 */

#define _POSIX_SOURCE	1
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <utmp.h>
#include <errno.h>
#include <minix/reboot.h>

#include "wtmp.h"

static char SHUT_PID[] =	"/usr/run/shutdown.pid";
static char NOLOGIN[] =		"/etc/nologin";

#ifndef __STDC__
#define inform_user_time inf_time
#define inform_user      inf_user
#endif

void usage( void );
void write_pid( void );
int inform_user_time( void );
void inform_user( void );
void terminate( void );
void wall( char *when, char *extra );
int crash_check( void );
void parse_time( char *arg );
void get_message( void );
int main( int argc, char *argv[] );
char *itoa( int n );

long wait_time=0L;
char message[1024];
char info[80];
int reboot_flag='h';			/* default is halt */
int info_min, info_hour;
char *prog;

void parse_time (arg)
char *arg;
{
  char *p = arg;
  int hours, minutes;
  time_t now;
  struct tm *tm;
  int delta = 0;
  int bad = 0;
  
  if (p[0] == '+') { delta = 1; p++; }

  hours = strtoul(p, &p, 10);
  if (*p == 0 && delta) {
    minutes = hours;
    hours = 0;
  } else {
    if (*p != ':' && *p != '.')
      bad = 1;
    else
      p++;
    minutes = strtoul(p, &p, 10);
    if (*p != 0) bad = 1;
  }
  if (bad) {
    fprintf(stderr,"Invalid time specification `%s'\n",arg);
    usage();
  }

  time(&now);
  tm = localtime(&now);

  if (!delta) {
    hours -= tm->tm_hour;
    minutes -= tm->tm_min;
  }

  if (minutes < 0) {
    minutes += 60;
    hours--;
  }
  if (hours < 0) hours += 24;	/* Time after midnight. */

  tm->tm_hour += hours;
  tm->tm_min += minutes;
  (void) mktime(tm);
  info_hour = tm->tm_hour;
  info_min  = tm->tm_min;

  sprintf(info,
    "The system will shutdown in %d hour%s and %d minute%s at %02d:%02d\n\n",
    hours,hours==1?"":"s",minutes,minutes==1?"":"s",info_hour,info_min);

  wait_time += hours * 3600 + minutes * 60;
  return;
}

int main(argc,argv)
int argc;
char *argv[];
{
  int i, now = 0, nologin = 0, want_terminate = 0, want_message = 0, check = 0;
  char *opt;
  int tty;
  static char HALT1[] = "-?";
  static char *HALT[] = { "shutdown", HALT1, NULL };

  /* Parse options. */
  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    if (argv[i][1] == '-' && argv[i][2] == 0) {
      /* -- */
      i++;
      break;
    }
    for (opt = argv[i] + 1; *opt != 0; opt++) {
      switch (*opt) {
      case 'k':
	want_terminate = 1;
	break;
      case 'h':
      case 'r':
      case 'p':
      case 'd':
      case 'R':
	reboot_flag = *opt;
	break;
      case 'm':
	want_message = 1;
	break;
      case 'C':
	check = 1;
	break;
      default:
	fprintf (stderr,"shutdown: invalid option '-%c'\n",*opt);
	usage();
	break;
      }
    }
  }
  if ((argc - i) > 2) usage();

  if (check) exit(crash_check() ? 0 : 2);

  if (i == argc) {
    /* No timespec, assume "now". */
    now = 1;
  } else {
    if (!strcmp(argv[i], "now"))
      now++;
    else
      parse_time(argv[i]);
  }

  if ((argc - i) == 2) {
    /* One line message */
    strcat(message, argv[i+1]);
    strcat(message, "\n");
  }

  if (want_terminate) terminate();
  if (want_message) get_message();

  puts(info);

  prog = strrchr(*argv,'/');
  if (prog == (char *)0)
    prog = *argv;
  else
    prog++;
    
  if (!now) {
    /* Daemonize. */
    switch (fork()) {
    case 0:
      break;
    case -1:
      fprintf(stderr, "%s: can't fork\n", prog);
      exit(1);
    default:
      exit(0);
    }
    /* Detach from the terminal (if any). */
    if ((tty = open("/dev/tty", O_RDONLY)) != -1) {
      close(tty);
      setsid();
    }
    write_pid();
  }

  for (;;) {
    if (wait_time <= 5 * 60 && !nologin && !now) {
      close(creat(NOLOGIN,00644));
      nologin = 1;
    }
    if (wait_time <= 60) break;
    if(inform_user_time())
      inform_user();
    sleep (60);
    wait_time -= 60;
  }
  
  if (!now) {
    inform_user();
    sleep (30);				/* Last minute before shutdown */
    wait_time -= 30;
    inform_user();
    sleep (30);				/* Last 30 seconds before shutdown */
  }
  wait_time = 0;
  inform_user();

  unlink(SHUT_PID);			/* No way of stopping anymore */
  unlink(NOLOGIN);

  HALT[1][1] = reboot_flag;
#if __minix_vmd
  execv("/usr/sbin/halt", HALT);
#else
  execv("/usr/bin/halt", HALT);
#endif
  if (errno != ENOENT)
    fprintf(stderr, "Can't execute 'halt': %s\n", strerror(errno));

  sleep(2);
  reboot(RBT_HALT);
  fprintf(stderr, "Reboot call failed: %s\n", strerror(errno));

  return(1);
}

void usage()
{
  fputs("Usage: shutdown [-hrRpmkd] [time [message]]\n", stderr);
  fputs("       -h -> halt system after shutdown\n", stderr);
  fputs("       -r -> reboot system after shutdown\n", stderr);
  fputs("       -R -> reset system after shutdown\n", stderr);
  fputs("       -p -> power system off after shutdown\n", stderr);
  fputs("       -d -> default CTRL-ALT-DEL shutdown for current bootloader\n", stderr);
  fputs("       -m -> read a shutdown message from standard input\n", stderr);
  fputs("       -k -> stop an already running shutdown\n", stderr);
  fputs("       time -> keyword ``now'', minutes before shutdown ``+5'',\n", stderr);
  fputs("               or absolute time specification ``11:20''\n", stderr);
  fputs("       message -> short shutdown message\n", stderr);
  exit(1);
}

void terminate()
{
  FILE *in;
  pid_t pid;
  char c_pid[5];

  in = fopen(SHUT_PID,"r");
  if (in == (FILE *)0) {
    fputs ("Can't get pid of shutdown process, probably not running shutdown\n", stderr);
    exit(1);
  }
  fgets(c_pid,5,in);
  fclose(in);
  pid = atoi(c_pid);
  if (kill(pid,9) == -1)
    fputs("Can't kill the shutdown process, probably not running anymore\n",stderr);
  else
    puts("Shutdown process terminated");
  unlink(SHUT_PID);
  unlink(NOLOGIN);
  exit(0);
}

void get_message()
{
  char line[80];

  puts ("Type your message. End with ^D at an empty line");
  fputs ("shutdown> ",stdout);fflush(stdout);
  while (fgets(line,80,stdin) != (char *)0) {
    strcat (message,line);
    bzero(line,strlen(line));
    fputs ("shutdown> ",stdout);fflush(stdout);
  }
  putc('\n',stdout);fflush(stdout);
}

int inform_user_time()
{
  int min;

  min = wait_time /60;

  if (min == 60 || min == 30 || min == 15 || min == 10 || min <= 5)
    return 1;
  else
    return 0;
}

void inform_user()
{
  int hour, minute;
  char mes[80];

  hour = 0;
  minute = wait_time / 60;
  while (minute >= 60) {
    minute -= 60;
    hour++;
  }

  if (hour)
    sprintf(mes,
    "\nThe system will shutdown in %d hour%s and %d minute%s at %.02d:%.02d\n\n",
    hour,hour==1?"":"s",minute,minute==1?"":"s",info_hour,info_min);
  else
  if (minute > 1)
    sprintf(mes,
    "\nThe system will shutdown in %d minutes at %.02d:%.02d\n\n",
    minute,info_hour,info_min);
  else
  if (wait_time > 1)
    sprintf(mes,
    "\nThe system will shutdown in %ld seconds\n\n",
    wait_time);
  else
    sprintf(mes,
    "\nThe system will shutdown NOW\n\n");

  wall(mes,message);
}

void write_pid()
{
  char pid[5];
  int fd;

  fd = creat(SHUT_PID,00600);
  if (!fd)
    return;
  strncpy (pid,itoa(getpid()), sizeof(pid));
  write (fd,pid,sizeof(pid));
  close(fd);
  return;
}

int crash_check()
{
  struct utmp last;
  int fd = -1, crashed;
  struct stat st;

  if (stat(STR_ROOT_WTMP, &st) < 0 || st.st_size == 0) {
  	if (stat(STR_WTMP, &st) < 0 || st.st_size == 0) {
		return 0;
	}
	if ((fd = open(STR_WTMP, O_RDONLY)) < 0) return 0;
  } else if ((fd = open(STR_ROOT_WTMP, O_RDONLY)) < 0) return 0;

  crashed = (lseek(fd, - (off_t) sizeof(last), SEEK_END) == -1
    || read(fd, (void *) &last, sizeof(last)) != sizeof(last)
    || last.ut_line[0] != '~'
    || (strncmp(last.ut_name, "shutdown", sizeof(last.ut_name))
    && strncmp(last.ut_name, "halt", sizeof(last.ut_name)))
    );
  close(fd);
  return crashed;
}

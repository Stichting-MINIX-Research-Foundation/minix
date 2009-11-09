/* The <utmp.h> header is used by init, login, who, etc. */

#ifndef _UTMP_H
#define _UTMP_H

#define WTMP  "/usr/adm/wtmp"	/* the login history file */
#define BTMP  "/usr/adm/btmp"	/* the bad-login history file */
#define UTMP  "/etc/utmp"	/* the user accouting file */

struct utmp {
  char ut_user[8];		/* user name */
  char ut_id[4];		/* /etc/inittab ID */
  char ut_line[12];		/* terminal name */
  char ut_host[16];		/* host name, when remote */
  short ut_pid;			/* process id */
  short int ut_type;		/* type of entry */
  long ut_time;			/* login/logout time */
};

#define ut_name ut_user		/* for compatibility with other systems */

/* Definitions for ut_type. */
#define RUN_LVL            1	/* this is a RUN_LEVEL record */
#define BOOT_TIME          2	/* this is a REBOOT record */
#define INIT_PROCESS       5	/* this process was spawned by INIT */
#define LOGIN_PROCESS      6	/* this is a 'getty' process waiting */
#define USER_PROCESS       7	/* any other user process */
#define DEAD_PROCESS       8	/* this process has died (wtmp only) */

#endif /* _UTMP_H */

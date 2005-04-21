/* su - become super-user		Author: Patrick van Kleef */

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>
#if __minix_vmd
#include <sys/syslog.h>
#endif
#include <minix/minlib.h>

_PROTOTYPE(int main, (int argc, char **argv));

int main(argc, argv)
int argc;
char *argv[];
{
  register char *name, *password;
  char *shell, sh0[100];
  char from_user[8+1], from_shell[100];
  register struct passwd *pwd;
  char USER[20], LOGNAME[25], HOME[100], SHELL[100];
  char *envv[20], **envp;
  int smallenv;
  char *p;
  int super;
  int loginshell;
#if __minix_vmd
  gid_t groups[NGROUPS_MAX];
  int ngroups;
  int g;
#endif

  smallenv = 0;
  loginshell = 0;
  if (argc > 1 && (strcmp(argv[1], "-") == 0 || strcmp(argv[1], "-e") == 0)) {
	if (argv[1][1] == 0)
		loginshell= 1;		/* 'su -' reads .profile */
	argv[1] = argv[0];
	argv++;
	argc--;
	smallenv = 1;	/* Use small environment. */
  }
  if (argc > 1) {
	if (argv[1][0] == '-') {
		fprintf(stderr,
			"Usage: su [-[e]] [user [shell-arguments ...]]\n");
		exit(1);
	}
	name = argv[1];
	argv[1] = argv[0];
	argv++;
  } else {
	name = "root";
  }

  if ((pwd = getpwuid(getuid())) == 0) {
	fprintf(stderr, "You do not exist\n");
	exit(1);
  }
  strncpy(from_user, pwd->pw_name, 8);
  from_user[8]= 0;
  strncpy(from_shell, pwd->pw_shell[0] == '\0' ? "/bin/sh" : pwd->pw_shell,
						sizeof(from_shell)-1);
  from_shell[sizeof(from_shell)-1]= 0;

  if ((pwd = getpwnam(name)) == 0) {
	fprintf(stderr, "Unknown id: %s\n", name);
	exit(1);
  }
  super = 0;
  if (getgid() == 0) super = 1;
#if __minix_vmd
  ngroups = getgroups(NGROUPS_MAX, groups);
  for (g = 0; g < ngroups; g++) if (groups[g] == 0) super = 1;
#endif

  if (!super && strcmp(pwd->pw_passwd, crypt("", pwd->pw_passwd)) != 0) {
#if __minix_vmd
	openlog("su", 0, LOG_AUTH);
#endif
	password = getpass("Password:");
	if (password == 0
	  || strcmp(pwd->pw_passwd, crypt(password, pwd->pw_passwd)) != 0) {
		if (password != 0 && *password != 0) {
#if __minix_vmd
			syslog(LOG_WARNING, "su %s failed for %s",
							name, from_user);
#endif
		}
		fprintf(stderr, "Sorry\n");
		exit(2);
	}
#if __minix_vmd
	syslog(LOG_NOTICE, "su %s succeeded for %s", name, from_user);
	closelog();
#endif
  }

#if __minix_vmd
  initgroups(pwd->pw_name, pwd->pw_gid);
#endif
  setgid(pwd->pw_gid);
  setuid(pwd->pw_uid);
  if (loginshell) {
	shell = pwd->pw_shell[0] == '\0' ? "/bin/sh" : pwd->pw_shell;
  } else {
	if ((shell = getenv("SHELL")) == NULL) shell = from_shell;
  }
  if ((p= strrchr(shell, '/')) == 0) p= shell; else p++;
  sh0[0]= '-';
  strcpy(loginshell ? sh0+1 : sh0, p);
  argv[0]= sh0;

  if (smallenv) {
	envp = envv;
	*envp++ = "PATH=:/bin:/usr/bin",
	strcpy(USER, "USER=");
	strcpy(USER + 5, name);
	*envp++ = USER;
	strcpy(LOGNAME, "LOGNAME=");
	strcpy(LOGNAME + 8, name);
	*envp++ = LOGNAME;
	strcpy(SHELL, "SHELL=");
	strcpy(SHELL + 6, shell);
	*envp++ = SHELL;
	strcpy(HOME, "HOME=");
	strcpy(HOME + 5, pwd->pw_dir);
	*envp++ = HOME;
	if ((p = getenv("TERM")) != NULL) {
		*envp++ = p - 5;
	}
	if ((p = getenv("TERMCAP")) != NULL) {
		*envp++ = p - 8;
	}
	if ((p = getenv("TZ")) != NULL) {
		*envp++ = p - 3;
	}
	*envp = NULL;
	(void) chdir(pwd->pw_dir);
	execve(shell, argv, envv);
  } else {
	execv(shell, argv);
  }
  fprintf(stderr, "No shell\n");
  return(3);
}

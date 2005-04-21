/* passwd - change a passwd			Author: Adri Koppes */

/* chfn, chsh - change full name, shell		Added by: Kees J. Bot */

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <minix/minlib.h>
#include <stdio.h>

_PROTOTYPE(void report, (char *label));
_PROTOTYPE(void quit, (int ex_stat));
_PROTOTYPE(void fatal, (char *label));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(int goodchars, (char *s));
_PROTOTYPE(int main, (int argc, char **argv));

char pw_file[] = "/etc/passwd";
char sh_file[] = "/etc/shadow";
char pw_tmp[] = "/etc/ptmp";
char bad[] = "Permission denied\n";
char buf[1024];

enum action {
  PASSWD, CHFN, CHSH
} action = PASSWD;

char *arg0;

void report(label)
char *label;
{
  int e = errno;
  fprintf(stderr, "%s: ", arg0);
  fflush(stderr);
  errno = e;
  perror(label);
}

void quit(ex_stat)
int ex_stat;
{
  if (unlink(pw_tmp) < 0 && errno != ENOENT) {
	report(pw_tmp);
	ex_stat = 1;
  }
  exit(ex_stat);
}

void fatal(label)
char *label;
{
  report(label);
  quit(1);
}

void usage()
{
  static char *usages[] = {
	"passwd [user]\n",
	"chfn [user] fullname\n",
	"chsh [user] shell\n"
  };
  std_err(usages[(int) action]);
  exit(1);
}

int goodchars(s)
char *s;
{
  int c;

  while ((c = *s++) != 0) {
	if (c == ':' || c < ' ' || c >= 127) return(0);
  }
  return(1);
}

int main(argc, argv)
int argc;
char *argv[];
{
  int uid, cn, n;
  int fd_pwd, fd_tmp;
  FILE *fp_tmp;
  time_t salt;
  struct passwd *pwd;
  char *name, pwname[9], oldpwd[9], newpwd[9], newcrypted[14], sl[2];
  char *argn;
  int shadow = 0;

  if ((arg0 = strrchr(argv[0], '/')) != 0)
	arg0++;
  else
	arg0 = argv[0];

  if (strcmp(arg0, "chfn") == 0)
	action = CHFN;
  else if (strcmp(arg0, "chsh") == 0)
	action = CHSH;

  uid = getuid();

  n = action == PASSWD ? 1 : 2;

  if (argc != n && argc != n + 1) usage();

  if (argc == n) {
	pwd = getpwuid(uid);
	strcpy(pwname, pwd->pw_name);
	name = pwname;
  } else {
	name = argv[1];
	pwd = getpwnam(name);
  }
  if (pwd == NULL || ((uid != pwd->pw_uid) && uid != 0)) {
	std_err(bad);
	exit(1);
  }

  switch (action) {
      case PASSWD:
	if (pwd->pw_passwd[0] == '#' && pwd->pw_passwd[1] == '#') {
		/* The password is found in the shadow password file. */
		shadow = 1;
		strncpy(pwname, pwd->pw_passwd + 2, 8);
		pwname[8] = 0;
		name = pwname;
		setpwfile(sh_file);
		if ((pwd= getpwnam(name)) == NULL) {
			std_err(bad);
			exit(1);
		}
		printf("Changing the shadow password of %s\n", name);
	} else {
		printf("Changing the password of %s\n", name);
	}

	oldpwd[0] = 0;
	if (pwd->pw_passwd[0] != '\0' && uid != 0) {
		strcpy(oldpwd, getpass("Old password:"));
		if (strcmp(pwd->pw_passwd, crypt(oldpwd, pwd->pw_passwd)) != 0)
		{
			std_err(bad);
			exit(1);
		}
	}
	for (;;) {
		strcpy(newpwd, getpass("New password:"));

		if (newpwd[0] == '\0')
			std_err("Password cannot be null");
		else if (strcmp(newpwd, getpass("Retype password:")) != 0)
			std_err("Passwords don't match");
		else
			break;

		std_err(", try again\n");
	}
	time(&salt);
	sl[0] = (salt & 077) + '.';
	sl[1] = ((salt >> 6) & 077) + '.';
	for (cn = 0; cn < 2; cn++) {
		if (sl[cn] > '9') sl[cn] += 7;
		if (sl[cn] > 'Z') sl[cn] += 6;
	}
	strcpy(newcrypted, crypt(newpwd, sl));
	break;

      case CHFN:
      case CHSH:
	argn = argv[argc - 1];

	if (strlen(argn) > (action == CHFN ? 80 : 60) || !goodchars(argn)) {
		std_err(bad);
		exit(1);
	}
  }

  signal(SIGHUP, SIG_IGN);
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  umask(0);
  n = 10;
  while ((fd_tmp = open(pw_tmp, O_RDWR | O_CREAT | O_EXCL, 0400)) < 0) {
	if (errno != EEXIST) fatal("Can't create temporary file");

	if (n-- > 0) {
		sleep(2);
	} else {
		fprintf(stderr, "Password file busy, try again later.\n");
		exit(1);
	}
  }

  if ((fp_tmp = fdopen(fd_tmp, "w+")) == NULL) fatal(pw_tmp);

  setpwent();
  while ((pwd = getpwent()) != 0) {
	if (strcmp(name, pwd->pw_name) == 0) {
		switch (action) {
		    case PASSWD:
			pwd->pw_passwd = newcrypted;
			break;
		    case CHFN:
			pwd->pw_gecos = argn;
			break;
		    case CHSH:
		    	pwd->pw_shell = argn;
		    	break;
		}
	}
	if (strcmp(pwd->pw_shell, "/bin/sh") == 0
		|| strcmp(pwd->pw_shell, "/usr/bin/sh") == 0
	)
		pwd->pw_shell = "";

	fprintf(fp_tmp, "%s:%s:%s:",
		pwd->pw_name,
		pwd->pw_passwd,
		itoa(pwd->pw_uid)
	);
	if (ferror(fp_tmp)) fatal(pw_tmp);

	fprintf(fp_tmp, "%s:%s:%s:%s\n",
		itoa(pwd->pw_gid),
		pwd->pw_gecos,
		pwd->pw_dir,
		pwd->pw_shell
	);
	if (ferror(fp_tmp)) fatal(pw_tmp);
  }
  endpwent();
  if (fflush(fp_tmp) == EOF) fatal(pw_tmp);

  if (lseek(fd_tmp, (off_t) 0, SEEK_SET) != 0)
	fatal("Can't reread temp file");

  if ((fd_pwd = open(shadow ? sh_file : pw_file, O_WRONLY | O_TRUNC)) < 0)
	fatal("Can't recreate password file");

  while ((n = read(fd_tmp, buf, sizeof(buf))) != 0) {
	if (n < 0 || write(fd_pwd, buf, n) != n) {
		report("Error rewriting password file, tell root!");
		exit(1);
	}
  }
  close(fd_tmp);
  close(fd_pwd);
  quit(0);
}

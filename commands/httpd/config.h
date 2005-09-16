/* config.h
 *
 * This file is part of httpd.
 *
 * 02/26/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 * 07/04/2003			Al Woodhull <awoodhull@hampshire.edu>	
 * 02/08/2005 			Michael Temari <Michael@TemWare.Com>
 *
 */

#define VERSION  "Minix httpd 0.994"

struct authuser {
	char *user;
	struct authuser *next;
};

struct auth {
	char *name;
	char *desc;
	int urlaccess;
	char *passwdfile;
	struct authuser *users;
	struct auth *next;
};

struct msufx {
	char *suffix;
	struct mtype *mtype;
	struct msufx *snext;
	struct msufx *tnext;
};

struct mtype {
	char *mimetype;
	struct msufx *msufx;
	struct mtype *next;
};

struct vhost {
	char *hname;
	char *root;
	struct vhost *next;
};

struct vpath {
	char *from;
	char *to;
	struct auth *auth;
	int urlaccess;
	struct vpath *next;
};

struct dirsend {
	char *file;
	struct dirsend *next;
};

/* urlaccess bits */

#define	URLA_READ	1
#define	URLA_WRITE	2
#define	URLA_EXEC	4
#define	URLA_HEADERS	8

#define	HTTPD_CONFIG_FILE	"/etc/httpd.conf"

_PROTOTYPE(int readconfig, (char *cfg_file, int testing));

extern struct mtype *mtype;
extern struct msufx *msufx;
extern struct vhost *vhost;
extern struct vpath *vpath;
extern struct dirsend *dirsend;
extern struct auth *auth;
extern struct auth *proxyauth;
extern char *direxec;
extern char *srvrroot;
extern char *LogFile;
extern char *DbgFile;
extern char *User;
extern char *Chroot;

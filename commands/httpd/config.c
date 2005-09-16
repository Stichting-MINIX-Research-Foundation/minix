/* config.c by Michael Temari 02/26/96
 *
 * This file is part of httpd.
 *
 * 02/26/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>

#include "utility.h"
#include "config.h"

struct mtype *mtype = NULL;
struct msufx *msufx = NULL;
struct vhost *vhost = NULL;
struct vpath *vpath = NULL;
struct dirsend *dirsend = NULL;
struct auth *auth = NULL;
struct auth *proxyauth = NULL;
char *direxec = NULL;
char *srvrroot = "";
char *LogFile = NULL;
char *DbgFile = NULL;
char *User = NULL;
char *Chroot = NULL;

_PROTOTYPE(static int doconfig, (char *cfg_file));
_PROTOTYPE(static int doinclude, (char *parms[], int np));
_PROTOTYPE(static int domtype, (char *parms[], int np));
_PROTOTYPE(static struct auth *findauth, (char *name));
_PROTOTYPE(static int dovhost, (char *parms[], int np));
_PROTOTYPE(static int dovpath, (char *parms[], int np));
_PROTOTYPE(static int dosrvrroot, (char *parms[], int np));
_PROTOTYPE(static int dodirsend, (char *parms[], int np));
_PROTOTYPE(static int dodirexec, (char *parms[], int np));
_PROTOTYPE(static char *subvpath, (char *s));
_PROTOTYPE(static int dologfile, (char *parms[], int np));
_PROTOTYPE(static int dodbgfile, (char *parms[], int np));
_PROTOTYPE(static int douser, (char *parms[], int np));
_PROTOTYPE(static int dochroot, (char *parms[], int np));
_PROTOTYPE(static int adduser, (struct auth *pauth, char *user));
_PROTOTYPE(static int doauth, (char *parms[], int np));
_PROTOTYPE(static int doproxyauth, (char *parms[], int np));

int readconfig(cfg_file, testing)
char *cfg_file;
int testing;
{
int s;
char *cfg;
struct msufx *ps;
struct mtype *pt;
struct vhost *ph;
struct vpath *pv;
struct dirsend *pd;
struct auth *pa;

   cfg = HTTPD_CONFIG_FILE;
   if(cfg_file != (char *)NULL)
	if(*cfg_file)
		cfg = cfg_file;

   s = doconfig(cfg);

   if(testing) {
	printf("ServerRoot: %s\n", srvrroot);
	printf("UserName: %s\n", User == NULL ? "" : User);
	printf("Chroot: %s\n", Chroot == NULL ? "" : Chroot);
	printf("LogFile: %s\n", LogFile == NULL ? "" : LogFile);
	printf("DbgFile: %s\n", DbgFile == NULL ? "" : DbgFile);
	printf("DirSend:");
	for(pd = dirsend; pd != NULL; pd = pd->next)
   		printf(" %s", pd->file);
	printf("\n");
	printf("DirExec: %s\n", direxec == NULL ? "" : direxec);
	for(ph = vhost; ph != NULL; ph = ph->next)
		printf("VHost: %s %s\n", ph->hname, ph->root);
	for(pa = auth; pa != NULL; pa = pa->next)
   		printf("Auth: %s %s %d %s\n",
			pa->name, pa->desc, pa->urlaccess, pa->passwdfile);
	for(pa = proxyauth; pa != NULL; pa = pa->next)
   		printf("ProxyAuth: %s %s %d %s\n",
			pa->name, pa->desc, pa->urlaccess, pa->passwdfile);
	for(pv = vpath; pv != NULL; pv = pv->next)
   		printf("Vpath: %s %s %s %d\n",
			pv->from, pv->to, pv->auth->name, pv->urlaccess);
	for(pt = mtype; pt != NULL; pt = pt->next) {
   		printf("MType: %s :", pt->mimetype);
		for(ps = pt->msufx; ps != NULL; ps = ps->tnext)
		   	printf(" '%s'", ps->suffix);
		printf("\n");
	}
	for(ps = msufx; ps != NULL; ps = ps->snext)
   		printf("Suffix: %s\t%s\n", ps->suffix, ps->mtype->mimetype);
   }

   return(s);
}

static int doconfig(cfg_file)
char *cfg_file;
{
FILE *fp;
int np;
int s;
char *p;
char ltype[40];
char *parms[30];
static char buffer[2048];

   if((fp = fopen(cfg_file, "r")) == (FILE *)NULL) {
   	fprintf(stderr, "httpd: Could not read %s config file.\n", cfg_file);
	return(-1);
   }

   *ltype = '\0';

   while(fgets(buffer, sizeof(buffer), fp) != (char *)NULL) {
   	if(buffer[0] == '#') continue;	/* skip comments */
   	np = getparms(buffer, parms, sizeof(parms)/sizeof(parms[0]));
   	if(np == 0) continue;	/* blank line */
   	if(parms[0] == (char *)NULL)
   		parms[0] = ltype;
   	else {
   		p = parms[0];
   		while(*p) *p++ = tolower(*p);
   		strncpy(ltype, parms[0], sizeof(ltype));
   	}
   	s = 0;
   	if(!strcmp(parms[0], "mtype")) s = domtype(parms, np);
   	else
   	if(!strcmp(parms[0], "vhost")) s = dovhost(parms, np);
   	else
   	if(!strcmp(parms[0], "vpath")) s = dovpath(parms, np);
   	else
   	if(!strcmp(parms[0], "serverroot")) s = dosrvrroot(parms, np);
   	else
   	if(!strcmp(parms[0], "dirsend")) s = dodirsend(parms, np);
   	else
   	if(!strcmp(parms[0], "direxec")) s = dodirexec(parms, np);
   	else
   	if(!strcmp(parms[0], "logfile")) s = dologfile(parms, np);
   	else
   	if(!strcmp(parms[0], "dbgfile")) s = dodbgfile(parms, np);
   	else
   	if(!strcmp(parms[0], "user")) s = douser(parms, np);
   	else
   	if(!strcmp(parms[0], "chroot")) s = dochroot(parms, np);
   	else
   	if(!strcmp(parms[0], "auth")) s = doauth(parms, np);
   	else
   	if(!strcmp(parms[0], "proxyauth")) s = doproxyauth(parms, np);
   	else
	if(!strcmp(parms[0], "include")) s = doinclude(parms, np);
	else
   	fprintf(stderr, "httpd: Unknown directive: %s\n", parms[0]);
   	if(s) {
   		fprintf(stderr, "httpd: Error processing config file\n");
		fclose(fp);
   		return(-1);
   	}
   }

   fclose(fp);

   return(0);
}

static int doinclude(parms, np)
char *parms[];
int np;
{
char *p;

   if(np < 2) return(0);

   p = subvpath(parms[1]);

   return(doconfig(p));
}

static int domtype(parms, np)
char *parms[];
int np;
{
int i;
struct mtype *pt, *lpt, *newpt;
struct msufx *ps, *lps, *newps, *psend;

   if(np < 2) return(0);


   /* check if this mime type already exists in the list */
   for(pt = mtype, lpt = NULL; pt != NULL; lpt = pt, pt = pt->next)
   	if(!strcmp(parms[1], pt->mimetype))
   		break;

   if(pt == NULL) {		/* not there so add it */
   	newpt = malloc(sizeof(struct mtype));
   	if(newpt == NULL) {
   		fprintf(stderr, "httpd: malloc failed in domtype\n");
   		return(-1);
   	}
   	newpt->mimetype = malloc(strlen(parms[1])+1);
   	if(newpt->mimetype == NULL) {
   		fprintf(stderr, "httpd: malloc failed in domtype\n");
   		return(-1);
   	}
   	strcpy(newpt->mimetype, parms[1]);
   	newpt->msufx = NULL;
   	newpt->next = NULL;
   	if(lpt == NULL)
   		mtype = newpt;
   	else
   		lpt->next = newpt;
   } else
   	newpt = pt;

   /* find end of suffix list */
   for(ps = newpt->msufx, lps = NULL; ps != NULL; lps = ps, ps = ps->tnext) ;
   psend = lps;

   /* if no suffix given then add empty suffix for default */
   if(np == 2)
   	strcpy(parms[np++], "");

   /* add each suffix to the mime type */
   for(i = 2; i < np; i++) {
	/* a suffix can only be for a single mime type */
	for(ps = msufx, lps = NULL; ps != NULL; lps = ps, ps = ps->snext) {
		if(!strcmp(ps->suffix, parms[i])) {
			fprintf(stderr, "httpd: Suffix already found\n");
			return(-1);
		}
		if(strlen(parms[i]) > strlen(ps->suffix)) break;
	}
	newps = malloc(sizeof(struct msufx));
	if(newps == NULL) {
   		fprintf(stderr, "httpd: malloc failed in domtype\n");
   		return(-1);
   	}
	newps->suffix = malloc(strlen(parms[i])+1);
	if(newps->suffix == NULL) {
   		fprintf(stderr, "httpd: malloc failed in domtype\n");
   		return(-1);
   	}
	strcpy(newps->suffix, parms[i]);
	newps->mtype = newpt;
	newps->snext = NULL;
	newps->tnext = NULL;
	if(lps == NULL) {
		msufx = newps;
		newps->snext = ps;
	} else {
		lps->snext = newps;
		newps->snext = ps;
	}
	if(psend == NULL)
		newpt->msufx = newps;
	else
		psend->tnext = newps;
	psend = newps;
   }

   return(0);
}

static struct auth *findauth(name)
char *name;
{
char lname[80];
char *p, *p2;
struct auth *a = NULL;

   if(sizeof(lname) < (strlen(name)+1)) {
   	fprintf(stderr, "httpd: lname too small in findauth\n");
   	return(a);
   }
   p = name; p2 = lname;
   while(*p)
	*p2++ = tolower(*p++);
   *p2 = '\0';

   for(a = auth; a != NULL; a = a->next)
	if(!strcmp(a->name, lname)) break;

   return(a);
}

static int dovhost(parms, np)
char *parms[];
int np;
{
char *hname, *root;
struct vhost *ph, *lph, *newph;

   if(np < 2) return(0);

   hname = parms[1];

   if(np < 3)
   	root = "";
   else
	root = parms[2];

   for(ph = vhost, lph = NULL; ph != NULL; lph = ph, ph = ph->next)
	;

   newph = malloc(sizeof(struct vhost));
   if(newph == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dovhost\n");
   	return(-1);
   }
   newph->hname = malloc(strlen(hname)+1);
   if(newph->hname == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dovhost\n");
   	return(-1);
   }
   strcpy(newph->hname, hname);

   root = subvpath(root);

   newph->root = malloc(strlen(root)+1);
   if(newph->root == NULL) {
	fprintf(stderr, "httpd: malloc failed in dovhost\n");
   	return(-1);
   }
   strcpy(newph->root, root);

   if(np > 3)
   	if(parms[3][0] != '#') {
		fprintf(stderr, "httpd: junk at end of vhost line\n");
   		return(-1);
   	}

   newph->next = NULL;
   if(lph == NULL) {
	vhost = newph;
	newph->next = ph;
   } else {
	lph->next = newph;
	newph->next = ph;
   }

   return(0);
}

static int dovpath(parms, np)
char *parms[];
int np;
{
char *from, *to;
struct vpath *pv, *lpv, *newpv;

   if(np < 3) return(0);

   from = parms[1];
   to = parms[2];

   for(pv = vpath, lpv = NULL; pv != NULL; lpv = pv, pv = pv->next)
	;

   newpv = malloc(sizeof(struct vpath));
   if(newpv == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dovpath\n");
   	return(-1);
   }
   newpv->from = malloc(strlen(from)+1);
   if(newpv->from == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dovpath\n");
   	return(-1);
   }
   strcpy(newpv->from, from);

   to = subvpath(to);

   newpv->to = malloc(strlen(to)+1);
   if(newpv->to == NULL) {
	fprintf(stderr, "httpd: malloc failed in dovpath\n");
   	return(-1);
   }
   strcpy(newpv->to, to);

   newpv->auth = NULL;
   newpv->urlaccess = -1;

   if(np > 3)
   	if(parms[3][0] != '#') {
   		newpv->auth = findauth(parms[3]);
   		if(np > 4)
			if(parms[4][0] != '#') {
				newpv->urlaccess = mkurlaccess(parms[4]);
				if(np > 5)
   					if(parms[5][0] != '#') {
   						fprintf(stderr, "httpd: junk at end of vpath line\n");
   						return(-1);
   					}
			}
   	}

   newpv->next = NULL;
   if(lpv == NULL) {
	vpath = newpv;
	newpv->next = pv;
   } else {
	lpv->next = newpv;
	newpv->next = pv;
   }

   return(0);
}

static int dosrvrroot(parms, np)
char *parms[];
int np;
{
char *newroot;

   if(np < 2) return(0);

   newroot = subvpath(parms[1]);

   srvrroot = malloc(strlen(newroot)+1);
   if(srvrroot == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dosrvrroot\n");
   	return(-1);
   }
   strcpy(srvrroot, newroot);
   if(srvrroot[strlen(srvrroot)-1] == '/')
   	srvrroot[strlen(srvrroot)-1] = '\0';

   return(0);
}

static int dodirsend(parms, np)
char *parms[];
int np;
{
char *file;
int i;
struct dirsend *pd, *lpd, *npd;

   if(np < 2) return(0);

   /* find end of the list */
   for(pd = dirsend, lpd = NULL; pd != NULL; lpd = pd, pd = pd->next) ;

   for(i = 1; i < np; i++) {
   	file = parms[i];
   	if(file[0] == '#') break;
	npd = malloc(sizeof(struct dirsend));
	if(npd == NULL) {
   		fprintf(stderr, "httpd: malloc failed in dodirsend\n");
   		return(-1);
	}
	npd->file = malloc(strlen(file)+1);
	if(npd->file == NULL) {
		fprintf(stderr, "httpd: malloc failed in dodirsend\n");
		return(-1);
	}
	strcpy(npd->file, file);
	npd->next = NULL;
	if(lpd == NULL)
		dirsend = npd;
	else
		lpd->next = npd;
	lpd = npd;
   }

   return(0);
}

static int dodirexec(parms, np)
char *parms[];
int np;
{
char *file;

   if(np < 2) return(0);

   if(direxec != NULL) {
   	fprintf(stderr, "httpd: Error direxec line already present\n");
   	return(-1);
   }

   file = subvpath(parms[1]);

   direxec = malloc(strlen(file)+1);

   if(direxec == NULL) {
	fprintf(stderr, "httpd: malloc failed in dodirexec\n");
   	return(-1);
   }

   strcpy(direxec, file);

   if(np > 2)
   	if(parms[2][0] != '#') {
   		fprintf(stderr, "httpd: garbage on end of direxec line\n");
   		return(-1);
   	}

   return(0);
}

static char *subvpath(s)
char *s;
{
char *p, *p2;
int len;
static char buffer[1024];
char user[80];
struct passwd *pwd;

   /* replace beginning // with srvrroot */
   if(s[0] == '/' && s[1] == '/')
   	/* but not /// if we have VHOST's */
   	if(vhost == NULL || s[2] != '/') {
   		strcpy(buffer, srvrroot);
   		strncat(buffer, s+1, sizeof(buffer) - strlen(buffer));
   		buffer[sizeof(buffer)-1] = '\0';
   		return(buffer);
	}

   if(s[0] != '/' || s[1] != '~') return(s);

   /* replace beginning /~user with user home directory */
   p = s + 2;
   p2 = user;
   len = sizeof(user) - 1;
   while(*p && *p != '/' && len-- > 0) *p2++ = *p++;
   *p2 = '\0';
   if(*p != '\0' && *p != '/') return(s);
   if((pwd = getpwnam(user)) == (struct passwd *)NULL) return(s);
   strcpy(buffer, pwd->pw_dir);
   strncat(buffer, p, sizeof(buffer) - strlen(buffer));
   buffer[sizeof(buffer)-1] = '\0';

   return(buffer);
}

static int dologfile(parms, np)
char *parms[];
int np;
{
char *p;

   if(np < 2) return(0);

   p = subvpath(parms[1]);
   LogFile = malloc(strlen(p)+1);
   if(LogFile == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dologfile\n");
   	return(-1);
   }
   strcpy(LogFile, p);

   return(0);
}

static int dodbgfile(parms, np)
char *parms[];
int np;
{
char *p;

   if(np < 2) return(0);

   p = subvpath(parms[1]);
   DbgFile = malloc(strlen(p)+1);
   if(DbgFile == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dodbgfile\n");
   	return(-1);
   }
   strcpy(DbgFile, p);

   return(0);
}

static int douser(parms, np)
char *parms[];
int np;
{
   if(np < 2) return(0);

   User = malloc(strlen(parms[1])+1);
   if(User == NULL) {
   	fprintf(stderr, "httpd: malloc failed in douser\n");
   	return(-1);
   }
   strcpy(User, parms[1]);

   return(0);
}

static int dochroot(parms, np)
char *parms[];
int np;
{
char *newroot;

   if(np < 2) return(0);

   newroot = subvpath(parms[1]);

   Chroot = malloc(strlen(newroot)+1);
   if(Chroot == NULL) {
   	fprintf(stderr, "httpd: malloc failed in dochroot\n");
   	return(-1);
   }
   strcpy(Chroot, newroot);

   return(0);
}

static int adduser(pauth, user)
struct auth *pauth;
char *user;
{
struct authuser *pa, *lpa, *newpa;

   for(pa = pauth->users, lpa = NULL; pa != NULL; lpa = pa, pa = pa->next)
	;

   newpa = malloc(sizeof(struct authuser));
   if(newpa == NULL) {
   	fprintf(stderr, "httpd: malloc failed in adduser\n");
   	return(-1);
   }
   newpa->user = malloc(strlen(user)+1);
   if(newpa->user == NULL) {
   	fprintf(stderr, "httpd: malloc failed in adduser\n");
   	return(-1);
   }
   strcpy(newpa->user, user);

   newpa->next = NULL;
   if(lpa == NULL) {
	pauth->users = newpa;
	newpa->next = pa;
   } else {
	lpa->next = newpa;
	newpa->next = pa;
   }

   return(0);
}

static int doauth(parms, np)
char *parms[];
int np;
{
int i;
char *name, *desc, *pf;
char *p, *p2;
struct auth *pa, *lpa, *newpa;

   if(np < 3) return(0);

   name = parms[1];
   desc = parms[2];

   for(pa = auth, lpa = NULL; pa != NULL; lpa = pa, pa = pa->next)
	;

   newpa = malloc(sizeof(struct auth));
   if(newpa == NULL) {
   	fprintf(stderr, "httpd: malloc failed in doauth\n");
   	return(-1);
   }
   newpa->name = malloc(strlen(name)+1);
   if(newpa->name == NULL) {
   	fprintf(stderr, "httpd: malloc failed in doauth\n");
   	return(-1);
   }
   p = name; p2 = newpa->name;
   while(*p)
	*p2++ = tolower(*p++);
   *p2 = '\0';

   newpa->desc = malloc(strlen(desc)+1);
   if(newpa->desc == NULL) {
	fprintf(stderr, "httpd: malloc failed in doauth\n");
   	return(-1);
   }
   strcpy(newpa->desc, desc);

   newpa->urlaccess = mkurlaccess(parms[3]);
   newpa->passwdfile = NULL;
   newpa->users = NULL;

   if(np > 4)
   	if(parms[4][0] != '#') {
		if(!strcmp(parms[4], "."))
			pf = "/etc/passwd";
		else
			pf = subvpath(parms[4]);
   		newpa->passwdfile = malloc(strlen(pf)+1);
   		if(newpa->passwdfile == NULL) {
   			fprintf(stderr, "httpd: malloc failed in doauth\n");
   			return(-1);
   		}
   		strcpy(newpa->passwdfile, pf);
		i = 5;
		while(i < np) {
   			if(parms[i][0] == '#')
				break;
			if(adduser(newpa, parms[i]))
				return(-1);
			i++;
		}
   	}

   newpa->next = NULL;
   if(lpa == NULL) {
	auth = newpa;
	newpa->next = pa;
   } else {
	lpa->next = newpa;
	newpa->next = pa;
   }

   return(0);
}

static int doproxyauth(parms, np)
char *parms[];
int np;
{
int i;
char *name, *desc, *pf;
char *p, *p2;
struct auth *pa, *lpa, *newpa;

   if(np < 3) return(0);

   name = parms[1];
   desc = parms[2];

   if(proxyauth != (struct auth *)NULL) {
   	fprintf(stderr, "httpd: ProxyAuth defined multiple times using 1st only\n");
   	return(0);
   }

   for(pa = proxyauth, lpa = NULL; pa != NULL; lpa = pa, pa = pa->next)
	;

   newpa = malloc(sizeof(struct auth));
   if(newpa == NULL) {
   	fprintf(stderr, "httpd: malloc failed in doproxyauth\n");
   	return(-1);
   }
   newpa->name = malloc(strlen(name)+1);
   if(newpa->name == NULL) {
   	fprintf(stderr, "httpd: malloc failed in doproxyauth\n");
   	return(-1);
   }
   p = name; p2 = newpa->name;
   while(*p)
	*p2++ = tolower(*p++);
   *p2 = '\0';

   newpa->desc = malloc(strlen(desc)+1);
   if(newpa->desc == NULL) {
	fprintf(stderr, "httpd: malloc failed in doproxyauth\n");
   	return(-1);
   }
   strcpy(newpa->desc, desc);

   newpa->urlaccess = mkurlaccess(parms[3]);
   newpa->passwdfile = NULL;
   newpa->users = NULL;

   if(np > 4)
   	if(parms[4][0] != '#') {
		if(!strcmp(parms[4], "."))
			pf = "/etc/passwd";
		else
			pf = subvpath(parms[4]);
   		newpa->passwdfile = malloc(strlen(pf)+1);
   		if(newpa->passwdfile == NULL) {
   			fprintf(stderr, "httpd: malloc failed in doauth\n");
   			return(-1);
   		}
   		strcpy(newpa->passwdfile, pf);
		i = 5;
		while(i < np) {
   			if(parms[i][0] == '#')
				break;
			if(adduser(newpa, parms[i]))
				return(-1);
			i++;
		}
   	}

   newpa->next = NULL;
   if(lpa == NULL) {
	proxyauth = newpa;
	newpa->next = pa;
   } else {
	lpa->next = newpa;
	newpa->next = pa;
   }

   return(0);
}

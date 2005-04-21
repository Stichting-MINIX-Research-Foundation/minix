/*	install 1.11 - install files.			Author: Kees J. Bot
 *								21 Feb 1993
 */
#define nil 0
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <a.out.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <utime.h>
#include <signal.h>

/* First line used on a self-decompressing executable. */
char ZCAT[]=	"#!/usr/bin/zexec /usr/bin/zcat\n";
char GZCAT[]=	"#!/usr/bin/zexec /usr/bin/gzcat\n";

/* Compression filters. */
char *COMPRESS[]=	{ "compress", nil };
char *GZIP[]=		{ "gzip", "-#", nil };

int excode= 0;		/* Exit code. */

void report(char *label)
{
	if (label == nil || label[0] == 0)
		fprintf(stderr, "install: %s\n", strerror(errno));
	else
		fprintf(stderr, "install: %s: %s\n", label, strerror(errno));
	excode= 1;
}

void fatal(char *label)
{
	report(label);
	exit(1);
}

void *allocate(void *mem, size_t size)
/* Safe malloc/realloc. */
{
	mem= mem == nil ? malloc(size) : realloc(mem, size);

	if (mem == nil) fatal(nil);
	return mem;
}

void deallocate(void *mem)
{
	if (mem != nil) free(mem);
}

int lflag= 0;		/* Make a hard link if possible. */
int cflag= 0;		/* Copy if you can't link, otherwise symlink. */
int dflag= 0;		/* Create a directory. */
int strip= 0;		/* Strip the copy. */
char **compress= nil;	/* Compress utility to make a compressed executable. */
char *zcat= nil;	/* Line one to decompress. */

long stack= -1;		/* Amount of heap + stack. */
int wordpow= 1;		/* Must be multiplied with wordsize ** wordpow */
			/* So 8kb for an 8086 and 16kb for the rest. */

pid_t filter(int fd, char **command)
/* Let a command filter the output to fd. */
{
	pid_t pid;
	int pfd[2];

	if (pipe(pfd) < 0) {
		report("pipe()");
		return -1;
	}

	switch ((pid= fork())) {
	case -1:
		report("fork()");
		return -1;
	case 0:
		/* Run the filter. */
		dup2(pfd[0], 0);
		dup2(fd, 1);
		close(pfd[0]);
		close(pfd[1]);
		close(fd);
		signal(SIGPIPE, SIG_DFL);
		execvp(command[0], command);
		fatal(command[0]);
	}
	/* Connect fd to the pipe. */
	dup2(pfd[1], fd);
	close(pfd[0]);
	close(pfd[1]);
	return pid;
}

int mkdirp(char *dir, int mode, int owner, int group)
/* mkdir -p dir */
{
	int keep;
	char *sep, *pref;

	sep= dir;
	while (*sep == '/') sep++;
	
	if (*sep == 0) {
		errno= EINVAL;
		return -1;
	}

	do {
		while (*sep != '/' && *sep != 0) sep++;
		pref= sep;
		while (*sep == '/') sep++;

		keep= *pref; *pref= 0;

		if (strcmp(dir, ".") == 0 || strcmp(dir, "..") == 0) continue;

		if (mkdir(dir, mode) < 0) {
			if (errno != EEXIST || *sep == 0) {
				/* On purpose not doing: *pref= keep; */
				return -1;
			}
		} else {
			if (chown(dir, owner, group) < 0 && errno != EPERM)
				return -1;
		}
	} while (*pref= keep, *sep != 0);
	return 0;
}

void makedir(char *dir, int mode, int owner, int group)
/* Install a directory, and set it's modes. */
{
	struct stat st;

	if (stat(dir, &st) < 0) {
		if (errno != ENOENT) { report(dir); return; }

		/* The target doesn't exist, make it. */
		if (mode == -1) mode= 0755;
		if (owner == -1) owner= getuid();
		if (group == -1) group= getgid();

		if (mkdirp(dir, mode, owner, group) < 0) {
			report(dir); return;
		}
	} else {
		/* The target does exist, change mode and ownership. */
		if (mode == -1) mode= (st.st_mode & 07777) | 0555;

		if ((st.st_mode & 07777) != mode) {
			if (chmod(dir, mode) < 0) { report(dir); return; }
		}
		if (owner == -1) owner= st.st_uid;
		if (group == -1) group= st.st_gid;
		if (st.st_uid != owner || st.st_gid != group) {
			if (chown(dir, owner, group) < 0 && errno != EPERM) {
				report(dir); return;
			}
			/* Set the mode again, chown may have wrecked it. */
			(void) chmod(dir, mode);
		}
	}
}

int setstack(struct exec *hdr)
/* Set the stack size in a header.  Return true if something changed. */
{
	long total;

	total= stack;
	while (wordpow > 0) {
		total *= hdr->a_cpu == A_I8086 ? 2 : 4;
		wordpow--;
	}
	total+= hdr->a_data + hdr->a_bss;

	if (!(hdr->a_flags & A_SEP)) {
		total+= hdr->a_text;
#ifdef A_PAL
		if (hdr->a_flags & A_PAL) total+= hdr->a_hdrlen;
#endif
	}
	if (hdr->a_cpu == A_I8086 && total > 0x10000L)
		total= 0x10000L;

	if (hdr->a_total != total) {
		/* Need to change stack allocation. */
		hdr->a_total= total;

		return 1;
	}
	return 0;
}

void copylink(char *source, char *dest, int mode, int owner, int group)
{
	struct stat sst, dst;
	int sfd, dfd, n;
	int r, same= 0, change= 0, docopy= 1;
	char buf[4096];
#	define hdr ((struct exec *) buf)
	pid_t pid;
	int status;

	/* Source must exist as a plain file, dest may exist as a plain file. */

	if (stat(source, &sst) < 0) { report(source); return; }

	if (mode == -1) {
		mode= sst.st_mode & 07777;
		if (!lflag || cflag) {
			mode|= 0444;
			if (mode & 0111) mode|= 0111;
		}
	}
	if (owner == -1) owner= sst.st_uid;
	if (group == -1) group= sst.st_gid;

	if (!S_ISREG(sst.st_mode)) {
		fprintf(stderr, "install: %s is not a regular file\n", source);
		excode= 1;
		return;
	}
	r= stat(dest, &dst);
	if (r < 0) {
		if (errno != ENOENT) { report(dest); return; }
	} else {
		if (!S_ISREG(dst.st_mode)) {
			fprintf(stderr, "install: %s is not a regular file\n",
									dest);
			excode= 1;
			return;
		}

		/* Are the files the same? */
		if (sst.st_dev == dst.st_dev && sst.st_ino == dst.st_ino) {
			if (!lflag && cflag) {
				fprintf(stderr,
				"install: %s and %s are the same, can't copy\n",
					source, dest);
				excode= 1;
				return;
			}
			same= 1;
		}
	}

	if (lflag && !same) {
		/* Try to link the files. */

		if (r >= 0 && unlink(dest) < 0) {
			report(dest); return;
		}

		if (link(source, dest) >= 0) {
			docopy= 0;
		} else {
			if (!cflag || errno != EXDEV) {
				fprintf(stderr,
					"install: can't link %s to %s: %s\n",
					source, dest, strerror(errno));
				excode= 1;
				return;
			}
		}
	}

	if (docopy && !same) {
		/* Copy the files, stripping if necessary. */
		long count= LONG_MAX;
		int first= 1;

		if ((sfd= open(source, O_RDONLY)) < 0) {
			report(source); return;
		}

		/* Open for write is less simple, its mode may be 444. */
		dfd= open(dest, O_WRONLY|O_CREAT|O_TRUNC, mode | 0600);
		if (dfd < 0 && errno == EACCES) {
			(void) chmod(dest, mode | 0600);
			dfd= open(dest, O_WRONLY|O_TRUNC);
		}
		if (dfd < 0) {
			report(dest);
			close(sfd);
			return;
		}

		pid= 0;
		while (count > 0 && (n= read(sfd, buf, sizeof(buf))) > 0) {
			if (first && n >= A_MINHDR && !BADMAG(*hdr)) {
				if (strip) {
					count= hdr->a_hdrlen
						+ hdr->a_text + hdr->a_data;
#ifdef A_NSYM
					hdr->a_flags &= ~A_NSYM;
#endif
					hdr->a_syms= 0;
				}
				if (stack != -1 && setstack(hdr)) change= 1;

				if (compress != nil) {
					/* Write first #! line. */
					(void) write(dfd, zcat, strlen(zcat));

					/* Put a compressor in between. */
					if ((pid= filter(dfd, compress)) < 0) {
						close(sfd);
						close(dfd);
						return;
					}
					change= 1;
				}
			}
			if (count < n) n= count;

			if (write(dfd, buf, n) < 0) {
				report(dest);
				close(sfd);
				close(dfd);
				if (pid != 0) (void) waitpid(pid, nil, 0);
				return;
			}
			count-= n;
			first= 0;
		}
		if (n < 0) report(source);
		close(sfd);
		close(dfd);
		if (pid != 0 && waitpid(pid, &status, 0) < 0 || status != 0) {
			excode= 1;
			return;
		}
		if (n < 0) return;
	} else {
		if (stack != -1) {
			/* The file has been linked into place.  Set the
			 * stack size.
			 */
			if ((dfd= open(dest, O_RDWR)) < 0) {
				report(dest);
				return;
			}

			if ((n= read(dfd, buf, sizeof(*hdr))) < 0) {
				report(dest); return;
			}

			if (n >= A_MINHDR && !BADMAG(*hdr) && setstack(hdr)) {
				if (lseek(dfd, (off_t) 0, SEEK_SET) == -1
					|| write(dfd, buf, n) < 0
				) {
					report(dest);
					close(dfd);
					return;
				}
				change= 1;
			}
			close(dfd);
		}
	}

	if (stat(dest, &dst) < 0) { report(dest); return; }

	if ((dst.st_mode & 07777) != mode) {
		if (chmod(dest, mode) < 0) { report(dest); return; }
	}
	if (dst.st_uid != owner || dst.st_gid != group) {
		if (chown(dest, owner, group) < 0 && errno != EPERM) {
			report(dest); return;
		}
		/* Set the mode again, chown may have wrecked it. */
		(void) chmod(dest, mode);
	}
	if (!change) {
		struct utimbuf ubuf;

		ubuf.actime= dst.st_atime;
		ubuf.modtime= sst.st_mtime;

		if (utime(dest, &ubuf) < 0 && errno != EPERM) {
			report(dest); return;
		}
	}
}

void usage(void)
{
	fprintf(stderr, "\
Usage:\n\
  install [-lcsz#] [-o owner] [-g group] [-m mode] [-S stack] [file1] file2\n\
  install [-lcsz#] [-o owner] [-g group] [-m mode] [-S stack] file ... dir\n\
  install -d [-o owner] [-g group] [-m mode] directory\n");
	exit(1);
}

void main(int argc, char **argv)
{
	int i= 1;
	int mode= -1;		/* Mode of target. */
	int owner= -1;		/* Owner. */
	int group= -1;		/* Group. */
	int super = 0;
#if NGROUPS_MAX > 0
	gid_t groups[NGROUPS_MAX];
	int ngroups;
	int g;
#endif

	/* Only those in group 0 are allowed to set owner and group. */
	if (getgid() == 0) super = 1;
#if NGROUPS_MAX > 0
	ngroups= getgroups(NGROUPS_MAX, groups);
	for (g= 0; g < ngroups; g++) if (groups[g] == 0) super= 1;
#endif
	if (!super) {
		setgid(getgid());
		setuid(getuid());
	}

	/* May use a filter. */
	signal(SIGPIPE, SIG_IGN);

	while (i < argc && argv[i][0] == '-') {
		char *p= argv[i++]+1;
		char *end;
		unsigned long num;
		int wp;
		struct passwd *pw;
		struct group *gr;

		if (strcmp(p, "-") == 0) break;

		while (*p != 0) {
			switch (*p++) {
			case 'l':	lflag= 1;	break;
			case 'c':	cflag= 1;	break;
			case 's':	strip= 1;	break;
			case 'd':	dflag= 1;	break;
			case 'z':
				if (compress == nil) {
					compress= COMPRESS;
					zcat= ZCAT;
				}
				break;
			case 'o':
				if (*p == 0) {
					if (i == argc) usage();
					p= argv[i++];
					if (*p == 0) usage();
				}
				num= strtoul(p, &end, 10);
				if (*end == 0) {
					if ((uid_t) num != num) usage();
					owner= num;
				} else {
					if ((pw= getpwnam(p)) == nil) {
						fprintf(stderr,
						"install: %s: unknown user\n",
							p);
						exit(1);
					}
					owner= pw->pw_uid;
				}
				p= "";
				break;
			case 'g':
				if (*p == 0) {
					if (i == argc) usage();
					p= argv[i++];
					if (*p == 0) usage();
				}
				num= strtoul(p, &end, 10);
				if (*end == 0) {
					if ((gid_t) num != num) usage();
					group= num;
				} else {
					if ((gr= getgrnam(p)) == nil) {
						fprintf(stderr,
						"install: %s: unknown user\n",
							p);
						exit(1);
					}
					group= gr->gr_gid;
				}
				p= "";
				break;
			case 'm':
				if (*p == 0) {
					if (i == argc) usage();
					p= argv[i++];
					if (*p == 0) usage();
				}
				num= strtoul(p, &end, 010);
				if (*end != 0 || (num & 07777) != num) usage();
				mode= num;
				if ((mode & S_ISUID) && super && owner == -1) {
					/* Setuid what?  Root most likely. */
					owner= 0;
				}
				if ((mode & S_ISGID) && super && group == -1) {
					group= 0;
				}
				p= "";
				break;
			case 'S':
				if (*p == 0) {
					if (i == argc) usage();
					p= argv[i++];
					if (*p == 0) usage();
				}
				stack= strtol(p, &end, 0);
				wp= 0;
				if (end == p || stack < 0) usage();
				p= end;
				while (*p != 0) {
					switch (*p++) {
					case 'm':
					case 'M': num= 1024 * 1024L; break;
					case 'k':
					case 'K': num= 1024; break;
					case 'w':
					case 'W': num= 4; wp++; break;
					case 'b':
					case 'B': num= 1; break;
					default: usage();
					}
					if (stack > LONG_MAX / num) usage();
					stack*= num;
				}
				wordpow= 0;
				while (wp > 0) { stack /= 4; wordpow++; wp--; }
				break;
			default:
				if ((unsigned) (p[-1] - '1') <= ('9' - '1')) {
					compress= GZIP;
					GZIP[1][1]= p[-1];
					zcat= GZCAT;
					break;
				}
				usage();
			}
		}
	}
	/* Some options don't mix. */
	if (dflag && (cflag || lflag || strip)) usage();

	/* Don't let the user umask interfere. */
	umask(000);

	if (dflag) {
		/* install directory */
		if ((argc - i) != 1) usage();

		makedir(argv[i], mode, owner, group);
	} else {
		struct stat st;

		if ((argc - i) < 1) usage();
		if ((lflag || cflag) && (argc - i) == 1) usage();

		if (stat(argv[argc-1], &st) >= 0 && S_ISDIR(st.st_mode)) {
			/* install file ... dir */
			char *target= nil;
			char *base;

			if ((argc - i) == 1) usage();

			while (i < argc-1) {
				if ((base= strrchr(argv[i], '/')) == nil)
					base= argv[i];
				else
					base++;
				target= allocate(target, strlen(argv[argc-1])
						+ 1 + strlen(base) + 1);
				strcpy(target, argv[argc-1]);
				strcat(target, "/");
				strcat(target, base);

				copylink(argv[i++], target, mode, owner, group);
			}
		} else {
			/* install [file1] file2 */

			copylink(argv[i], argv[argc-1], mode, owner, group);
		}
	}
	exit(excode);
}

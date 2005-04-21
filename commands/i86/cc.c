/*	Driver for Minix compilers.
	Written june 1987 by Ceriel J.H. Jacobs, partly derived from old
	cc-driver, written by Erik Baalbergen.
	This driver is mostly table-driven, the table being in the form of
	some global initialized structures.
*/
/* $Header$ */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* Paths.  (Executables in /usr are first tried with /usr stripped off.) */
#define SHELL		"/bin/sh"
#define PP		"/usr/lib/ncpp"
#define IRREL		"/usr/lib/irrel"
#define CEM		"/usr/lib/ncem"
#define M2EM		"/usr/lib/nm2em"
#define ENCODE		"/usr/lib/em_encode"
#define OPT		"/usr/lib/nopt"
#define CG		"/usr/lib/ncg"
#define AS		"/usr/lib/as"
#define LD		"/usr/lib/ld"
#define CV		"/usr/lib/cv"
#define LIBDIR		"/usr/lib"
#define CRT		"/usr/lib/ncrtso.o"
#define PEM		"/usr/lib/npem"
#define PRT		"/usr/lib/nprtso.o"
#define M2RT		"/usr/lib/nm2rtso.o"
#define LIBC            "/usr/lib/libd.a", "/usr/lib/libc.a"
#define LIBP		"/usr/lib/libp.a", "/usr/lib/libc.a"
#define LIBM2		"/usr/lib/libm2.a", "/usr/lib/libc.a"
#define END             "/usr/lib/libe.a", "/usr/lib/end.a"
#define M2DEF		"-I/usr/lib/m2"


/*	every pass that this program knows about has associated with it
	a structure, containing such information as its name, where it
	resides, the flags it accepts, and the like.
*/
struct passinfo {
	char *p_name;		/* name of this pass */
	char *p_path;		/* where is it */
	char *p_from;		/* suffix of source (comma-separated list) */
	char *p_to;		/* suffix of destination */
	char *p_acceptflags;	/* comma separated list; format:
			   		flag
			   		flag*
			   		flag=xxx
					flag*=xxx[*]
				   where a star matches a, possibly empty, 
				   string
				*/
	int  p_flags;
#define INPUT	01		/* needs input file as argument */
#define OUTPUT	02		/* needs output file as argument */
#define LOADER	04		/* this pass is the loader */
#define STDIN	010		/* reads from standard input */
#define STDOUT	020		/* writes on standard output */
#define NOCLEAN	040		/* do not remove target if this pass fails */
#define O_OUTPUT 0100		/* -o outputfile, hack for as */
#define PREPALWAYS	0200	/* always to be preprocessed */
#define PREPCOND	0400	/* preprocessed when starting with '#' */
#define PREPNOLN	01000	/* suppress line number info (cpp -P) */
};

#define MAXHEAD	10
#define MAXTAIL	5
#define MAXPASS	7

/*	Every language handled by this program has a "compile" structure
	associated with it, describing the start-suffix, how the driver for
	this language is called, which passes must be called, which flags
	and arguments must be passed to these passes, etc.
	The language is determined by the suffix of the argument program.
	However, if this suffix does not determine a language (DEFLANG),
	the callname is used.
	Notice that the 's' suffix does not determine a language, because
	the input file could have been derived from f.i. a C-program.
	So, if you use "cc x.s", the C-runtime system will be used, but if
	you use "as x.s", it will not.
*/
struct compile {
	char *c_suffix;		/* starting suffix of this list of passes */
	char *c_callname;	/* affects runtime system loaded with program */
	struct pass {
		char *pp_name;		/* name of the pass */
		char *pp_head[MAXHEAD];	/* args in front of filename */
		char *pp_tail[MAXTAIL];	/* args after filename */
	} c_passes[MAXPASS];
	int  c_flags;
#define DEFLANG		010	/* this suffix determines a language */
};

struct passinfo passinfo[] = {
	{ "cpp", PP, "CPP", "i", "wo=o,I*,D*,U*,P", INPUT|STDOUT },
	{ "irrel", IRREL, "i", "i", "m", INPUT},
	{ "cem", CEM, "i,c", "k", "m=o,p,wa=a,wo=o,ws=s,w,T*", INPUT|OUTPUT|PREPALWAYS },
	{ "pc", PEM, "i,p", "k", "n=L,w,a,A,R", INPUT|OUTPUT|PREPCOND },
	{ "m2", M2EM, "i,mod", "k", "n=L,w*,A,R,W*,3,I*", INPUT|OUTPUT|PREPCOND },
	{ "encode", ENCODE, "i,e", "k", "", INPUT|STDOUT|PREPCOND|PREPNOLN },
	{ "opt", OPT, "k", "m", "", STDIN|STDOUT },
	{ "cg", CG, "m", "s", "O=p4", INPUT|OUTPUT },
	{ "as", AS, "i,s", "o", "T*", INPUT|O_OUTPUT|PREPCOND },
	{ "ld", LD, "o", "out", "i,s", INPUT|LOADER },	/* changed */
	{ "cv", CV, "out", 0, "", INPUT|OUTPUT|NOCLEAN },	/* must come after loader */
	{ 0}
};

#define	PREP_FLAGS	"-D_EM_WSIZE=2", "-D_EM_PSIZE=2", "-D_EM_SSIZE=2", \
			"-D_EM_LSIZE=4", "-D_EM_FSIZE=4", "-D_EM_DSIZE=8", \
			"-D__ACK__", "-D__minix", "-D__i86"

struct pass preprocessor = { "cpp",
			    { PREP_FLAGS }
			    , {0}
			    };

struct pass prepnoln = { "cpp",
			    { PREP_FLAGS, "-P" }
			    , {0}
			    };

struct pass irrel = { "irrel",
			    {0}
			};

/* The "*" in the arguments for the loader indicates the place where the
 * fp-emulation library should come.
 */
struct compile passes[] = {
{	"c", "cc", 
	{	{ "cem", {"-L"}, {0} },	/* changed */
		{ "opt", {0}, {0} },
		{ "cg", {0}, {0} },
		{ "as", {"-"}, {0} },
		{ "ld", {CRT}, /* changed */
			  {LIBC, "*",  END}},
		{ "cv", {0}, {0} }
	},
	DEFLANG
},
{	"p", "pc",
	{	{ "pc", {0}, {0} },
		{ "opt", {0}, {0} },
		{ "cg", {0}, {0} },
		{ "as", {"-"}, {0} },
		{ "ld", {PRT}, 
			  {LIBP,
			    "*", END}},
		{ "cv", {0}, {0} }
	},
	DEFLANG
},
{	"mod", "m2",
	{	{ "m2", {M2DEF}, {0} },
		{ "opt", {0}, {0} },
		{ "cg", {0}, {0} },
		{ "as", {"-"}, {0} },
		{ "ld", {M2RT}, 
			  {LIBM2,
			    "*", END}},
		{ "cv", {0}, {0} }
	},
	DEFLANG
},
{	"e", "encode",
	{	{ "encode", {0}, {0}},
		{ "opt", {0}, {0} },
		{ "cg", {0}, {0} },
		{ "as", {"-"}, {0} },
		{ "ld", {0}, {"*", END}},
		{ "cv", {0}, {0} }
	},
	DEFLANG
},
{	"s", "as",
	{	{ "as", {0}, {0}}
	},
	0
},
{	"CPP", "cpp",
	{	{ "cpp", {PREP_FLAGS}, {0}}
	},
	DEFLANG
},
{	0},
};

#define MAXARGC	150	/* maximum number of arguments allowed in a list */
#define USTR_SIZE	64	/* maximum length of string variable */

typedef char USTRING[USTR_SIZE];

struct arglist {
	int al_argc;
	char *al_argv[MAXARGC];
};

struct arglist CALLVEC;

int kids = -1;

char *o_FILE = "a.out"; /* default name for executable file */

#define init(a)		((a)->al_argc = 1)
#define cleanup(str)		(str && remove(str))

char *ProgCall = 0;

int RET_CODE = 0;

char *stopsuffix;
int v_flag = 0;
int t_flag = 0;
int noexec = 0;
int fp_lib = 1;
int E_flag = 0;
int i_flag = 1;


USTRING curfil;
USTRING newfil;
struct arglist SRCFILES;
struct arglist LDIRS;
struct arglist LDFILES;
struct arglist GEN_LDFILES;
struct arglist FLAGS;

char *tmpdir = "/tmp";
char tmpname[64];

struct compile *compbase;
struct pass *loader;
struct passinfo *loaderinfo;
char *source;
int maxLlen;

_PROTOTYPE(char *library, (char *nm ));
_PROTOTYPE(void trapcc, (int sig ));
_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(int remove, (char *str ));
_PROTOTYPE(char *alloc, (unsigned u ));
_PROTOTYPE(int append, (struct arglist *al, char *arg ));
_PROTOTYPE(int concat, (struct arglist *al1, struct arglist *al2 ));
_PROTOTYPE(char *mkstr, (char *dst, char *arg1, char *arg2, char *arg3 ));
_PROTOTYPE(int basename, (char *str, char *dst ));
_PROTOTYPE(char *extension, (char *fln ));
_PROTOTYPE(int runvec, (struct arglist *vec, struct passinfo *pass, char *in, char *out ));
_PROTOTYPE(int prnum, (unsigned x ));
_PROTOTYPE(int prs, (char *str ));
_PROTOTYPE(int panic, (char *str ));
_PROTOTYPE(int pr_vec, (struct arglist *vec ));
_PROTOTYPE(int ex_vec, (struct arglist *vec ));
_PROTOTYPE(int mktempname, (char *nm ));
_PROTOTYPE(int mkbase, (void));
_PROTOTYPE(int mkloader, (void));
_PROTOTYPE(int needsprep, (char *name ));
_PROTOTYPE(int cfile, (char *name ));
_PROTOTYPE(char *apply, (struct passinfo *pinf, struct compile *cp, char *name, int passindex, int noremove, int first, char *resultname ));
_PROTOTYPE(int applicable, (struct passinfo *pinf, char *suffix ));
_PROTOTYPE(char *process, (char *name, int noremove ));
_PROTOTYPE(int mkvec, (struct arglist *call, char *in, char *out, struct pass *pass, struct passinfo *pinf ));
_PROTOTYPE(int callld, (struct arglist *in, char *out, struct pass *pass, struct passinfo *pinf ));
_PROTOTYPE(int clean, (struct arglist *c ));
_PROTOTYPE(int scanflags, (struct arglist *call, struct passinfo *pinf ));



char *
library(nm)
	char	*nm;
{
	static char	f[512];
	int	Lcount;

	for (Lcount = 0; Lcount < LDIRS.al_argc; Lcount++) {
		mkstr(f, LDIRS.al_argv[Lcount], "/lib", nm);
		strcat(f, ".a");
		if (access(f, 0) != 0) {
			f[strlen(f)-1] = 'a';
			if (access(f, 0) != 0) continue;
		}
		return f;
	}
	mkstr(f, LIBDIR, "/lib", nm);
	strcat(f, ".a");
	if (access(f, 0) != 0) {
		int i = strlen(f) - 1;
		f[i] = 'a';
		if (access(f, 0) != 0) f[i] = 'A';
	}
	return f;
}

void trapcc(sig)
	int sig;
{
	signal(sig, SIG_IGN);
	if (kids != -1) kill(kids, sig);
	cleanup(newfil);
	cleanup(curfil);
	exit(1);
}

main(argc, argv)
	char *argv[];
{
	char *str;
	char **argvec;
	int count;
	char *file;

	maxLlen = strlen(LIBDIR);
	ProgCall = *argv++;

	mkbase();

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, trapcc);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, trapcc);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, trapcc);
	while (--argc > 0) {
		if (*(str = *argv++) != '-' || str[1] == 0) {
			append(&SRCFILES, str);
			continue;
		}

		if (strcmp(str, "-com") == 0) {
			i_flag = 0;
		} else
		if (strcmp(str, "-sep") == 0) {
			i_flag = 1;
		} else {
			switch (str[1]) {

			case 'c':
				stopsuffix = "o";
				if (str[2] == '.') stopsuffix = str + 3;
				break;
			case 'f':
				fp_lib = (strcmp(str+2, "hard") != 0);
				break;
			case 'F':
			case 'W':
				/* Ignore. */
				break;
			case 'L':
				append(&LDIRS, &str[2]);
				count = strlen(&str[2]);
				if (count > maxLlen) maxLlen = count;
				break;
			case 'l':
				append(&SRCFILES, library(&str[2]));
				break;
			case 'm':
				/* Use -m, ignore -mxxx. */
				if (str[2] == 0) append(&FLAGS, str);
				break;
			case 'o':
				if (argc-- >= 0)
					o_FILE = *argv++;
				break;
			case 'S':
				stopsuffix = "s";
				break;
			case 'E':
				E_flag = 1;
				stopsuffix = "i";
				break;
			case 'P':
				stopsuffix = "i";
				append(&FLAGS, str);
				break;
			case 'v':
				v_flag++;
				if (str[2] == 'n')
					noexec = 1;
				break;
			case 't':
				/* save temporaries */
				t_flag++;
				break;
			case '.':
				if (str[2] == 'o') {
					/* no runtime start-off */
					loader->pp_head[0] = 0;
				}
				break;
			case 'i':
				i_flag++;
				break;
			case 'T':
				tmpdir = &str[2];
				/*FALLTHROUGH*/
			default:
				append(&FLAGS, str);

			}
		}
	}

	if (i_flag) append(&FLAGS, "-i");

	mktempname(tmpname);

	count = SRCFILES.al_argc;
	argvec = &(SRCFILES.al_argv[0]);

	while (count-- > 0) {

		file = *argvec++;
		source = file;

		file = process(file, 1);
	
		if (file && ! stopsuffix) append(&LDFILES, file);
	}

	clean(&SRCFILES);

	/* loader ... */
	if (RET_CODE == 0 && LDFILES.al_argc > 0) {
		register struct passinfo *pp = passinfo;

		while (!(pp->p_flags & LOADER)) pp++;
		mkstr(newfil, tmpname, pp->p_to, "");
		callld(&LDFILES, !((pp+1)->p_name) ? o_FILE : newfil, loader, pp);
		if (RET_CODE == 0) {
			register int i = GEN_LDFILES.al_argc;

			while (i-- > 0) {
				remove(GEN_LDFILES.al_argv[i]);
				free(GEN_LDFILES.al_argv[i]);
			}
			if ((++pp)->p_name) {
				process(newfil, 0);
			}
		}
	}
	exit(RET_CODE);
}

remove(str)
	char *str;
{
	if (t_flag)
		return;
	if (v_flag) {
		prs("rm ");
		prs(str);
		prs("\n");
	}
	if (noexec)
		return;
	unlink(str);
}

char *
alloc(u)
	unsigned u;
{
	register char *p = malloc(u);

	if (p == 0) panic("no space\n");
	return p;
}

append(al, arg)
	struct arglist *al;
	char *arg;
{
	char *a = alloc((unsigned) (strlen(arg) + 1));

	strcpy(a, arg);
	if (al->al_argc >= MAXARGC)
		panic("argument list overflow\n");
	al->al_argv[(al->al_argc)++] = a;
}

concat(al1, al2)
	struct arglist *al1, *al2;
{
	register i = al2->al_argc;
	register char **p = &(al1->al_argv[al1->al_argc]);
	register char **q = &(al2->al_argv[0]);

	if ((al1->al_argc += i) >= MAXARGC)
		panic("argument list overflow\n");
	while (i-- > 0)
		*p++ = *q++;
}

char *
mkstr(dst, arg1, arg2, arg3)
	char *dst, *arg1, *arg2, *arg3;
{
	register char *p;
	register char *q = dst;

	p = arg1;
	while (*q++ = *p++);
	q--;
	p = arg2;
	while (*q++ = *p++);
	q--;
	p = arg3;
	while (*q++ = *p++);
	q--;
	return dst;
}

basename(str, dst)
	char *str;
	register char *dst;
{
	register char *p1 = str;
	register char *p2 = p1;

	while (*p1)
		if (*p1++ == '/')
			p2 = p1;
	p1--;
	while (*p1 != '.' && p1 > p2) p1--;
	if (*p1 == '.') {
		*p1 = '\0';
		while (*dst++ = *p2++);
		*p1 = '.';
	}
	else
		while (*dst++ = *p2++);
}

char *
extension(fln)
	char *fln;
{
	register char *fn = fln;

	while (*fn) fn++;
	while (fn > fln && *fn != '.') fn--;
	if (fn != fln) return fn+1;
	return (char *)0;
}

runvec(vec, pass, in, out)
	struct arglist *vec;
	struct passinfo *pass;
	char *in, *out;
{
	int pid, status;
	int shifted = 0;

	if (
		strncmp(vec->al_argv[1], "/usr/", 5) == 0
		&&
		access(vec->al_argv[1] + 4, 1) == 0
	) {
		vec->al_argv[1] += 4;
		shifted = 1;
	}

	if (v_flag) {
		pr_vec(vec);
		if (pass->p_flags & STDIN) {
			prs(" <");
			prs(in);
		}
		if (pass->p_flags & STDOUT && !E_flag) {
			prs(" >");
			prs(out);
		}
		prs("\n");
	}
	if (noexec) {
		if (shifted) vec->al_argv[1] -= 4;
		clean(vec);
		return 1;
	}
	if ((pid = fork()) == 0) {	/* start up the process */
		if (pass->p_flags & STDIN && strcmp(in, "-") != 0) {
			/* redirect standard input */
			close(0);
			if (open(in, 0) != 0)
				panic("cannot open input file\n");
		}
		if (pass->p_flags & STDOUT && !E_flag) {
			/* redirect standard output */
			close(1);
			if (creat(out, 0666) != 1)
				panic("cannot create output file\n");
		}
		ex_vec(vec);
	}
	if (pid == -1)
		panic("no more processes\n");
	kids = pid;
	wait(&status);
	if (status) switch(status & 0177) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
	case 0:
		break;
	default:
		if (E_flag && (status & 0177) == SIGPIPE) break;
		prs(vec->al_argv[1]);
		prs(" died with signal ");
		prnum(status & 0177);
		prs("\n");
	}
	if (shifted) vec->al_argv[1] -= 4;
	clean(vec);
	kids = -1;
	return status ? ((RET_CODE = 1), 0) : 1;
}

prnum(x)
	register unsigned x;
{
	static char numbuf[8];			/* though it prints at most 3 characters */
	register char *cp = numbuf + sizeof(numbuf) - 1;

	*cp = '\0';
	while (x >= 10) {
		*--cp = (x % 10) + '0';
		x /= 10;
	}
	*--cp = x + '0';
	prs(cp);

}

prs(str)
	char *str;
{
	if (str && *str)
		write(2, str, strlen(str));
}

panic(str)
	char *str;
{
	prs(str);
	trapcc(SIGINT);
}

pr_vec(vec)
	register struct arglist *vec;
{
	register char **ap = &vec->al_argv[1];
	
	vec->al_argv[vec->al_argc] = 0;
	prs(*ap);
	while (*++ap) {
		prs(" ");
		if (strlen(*ap))
			prs(*ap);
		else
			prs("(empty)");
	}
}

ex_vec(vec)
	register struct arglist *vec;
{
	extern int errno;

	vec->al_argv[vec->al_argc] = 0;
	execv(vec->al_argv[1], &(vec->al_argv[1]));
	if (errno == ENOEXEC) { /* not an a.out, try it with the SHELL */
		vec->al_argv[0] = SHELL;
		execv(SHELL, &(vec->al_argv[0]));
	}
	if (access(vec->al_argv[1], 1) == 0) {
		/* File is executable. */
		prs("Cannot execute ");
		prs(vec->al_argv[1]);
		prs(". Not enough memory.\n");
		prs("Reduce the memory use of your system and try again\n");
	} else {
		prs(vec->al_argv[1]);
		prs(" is not executable\n");
	}
	exit(1);
}

mktempname(nm)
	register char *nm;
{
	register int i;
	register int pid = getpid();

	mkstr(nm, tmpdir, "/", compbase->c_callname);
	while (*nm) nm++;

	for (i = 9; i > 3; i--) {
		*nm++ = (pid % 10) + '0';
		pid /= 10;
	}
	*nm++ = '.';
	*nm++ = '\0'; /* null termination */
}

mkbase()
{
	register struct compile *p = passes;
	USTRING callname;
	register int len;

	basename(ProgCall, callname);
	len = strlen(callname);
	while (p->c_suffix) {
		if (strcmp(p->c_callname, callname+len-strlen(p->c_callname)) == 0) {
			compbase = p;
			mkloader();
			return;
		}
		p++;
	}
	/* we should not get here */
	panic("internal error\n");
}

mkloader()
{
	register struct passinfo *p = passinfo;
	register struct pass *pass;

	while (!(p->p_flags & LOADER)) p++;
	loaderinfo = p;
	pass = &(compbase->c_passes[0]);
	while (strcmp(pass->pp_name, p->p_name)) pass++;
	loader = pass;
}

needsprep(name)
	char *name;
{
	int file;
	char fc;

	file = open(name,0);
	if (file <0) return 0;
	if (read(file, &fc, 1) != 1) fc = 0;
	close(file);
	return fc == '#';
}

cfile(name)
	char *name;
{
	while (*name != '\0' && *name != '.')
		name++;

	if (*name == '\0') return 0;
	return (*++name == 'c' && *++name == '\0');
}

char *
apply(pinf, cp, name, passindex, noremove, first, resultname)
	register struct passinfo *pinf;
	register struct compile *cp;
	char *name, *resultname;
{
	/*	Apply a pass, indicated by "pinf", with args in 
		cp->c_passes[passindex], to name "name", leaving the result
		in a file with name "resultname", concatenated with result
		suffix.
		When neccessary, the preprocessor is run first.
		If "noremove" is NOT set, the file "name" is removed.
	*/

	struct arglist *call = &CALLVEC;
	struct pass *pass = &(cp->c_passes[passindex]);
	char *outname;

	if ( /* this pass is the first pass */
	     first
	   &&
	     ( /* preprocessor always needed */
	       (pinf->p_flags & PREPALWAYS)
	     ||/* or only when "needsprep" says so */
	       ( (pinf->p_flags & PREPCOND) && needsprep(name))
	     )
	   ) {
		mkstr(newfil, tmpname, passinfo[0].p_to, "");
		mkvec(call, name, newfil,
			(pinf->p_flags & PREPNOLN) ? &prepnoln : &preprocessor,
			&passinfo[0]);
		if (! runvec(call, &passinfo[0], name, newfil)) {
			cleanup(newfil);
			return 0;
		}

		/* A .c file must always be mishandled by irrel. */
		if (cfile(name)) {
			/* newfil is OK */
			mkvec(call, newfil, newfil, &irrel, &passinfo[1]);
			if (! runvec(call, &passinfo[1], newfil, newfil)) {
				cleanup(newfil);
				return 0;
			}
		}
		strcpy(curfil, newfil);
		newfil[0] = '\0';
		name = curfil;
		noremove = 0;
	}
	if (pinf->p_to) outname = mkstr(newfil, resultname, pinf->p_to, "");
	else outname = o_FILE;
	mkvec(call, name, outname, pass, pinf);
	if (! runvec(call, pinf, name, outname)) {
		if (! (pinf->p_flags & NOCLEAN)) cleanup(outname);
		if (! noremove) cleanup(name);
		return 0;
	}
	if (! noremove) cleanup(name);
	strcpy(curfil, newfil);
	newfil[0] = '\0';
	return curfil;
}

int
applicable(pinf, suffix)
	struct passinfo *pinf;
	char *suffix;
{
	/*	Return one if the pass indicated by "pinfo" is applicable to
		a file with suffix "suffix".
	*/
	register char *sfx = pinf->p_from;
	int l;

	if (! suffix) return 0;
	l = strlen(suffix);
	while (*sfx) {
		register char *p = sfx;

		while (*p && *p != ',') p++;
		if (l == p - sfx && strncmp(sfx, suffix, l) == 0) {
			return 1;
		}
		if (*p == ',') sfx = p+1;
		else sfx = p;
	}
	return 0;
}
		
char *
process(name, noremove)
	char *name;
{
	register struct compile *cp = passes;
	char *suffix = extension(name);
	USTRING base;
	register struct pass *pass;
	register struct passinfo *pinf;

	if (E_flag) {
		/* -E uses the cpp pass. */
		suffix = "CPP";
	}

	if (! suffix) return name;

	basename(name, base);

	while (cp->c_suffix) {
		if ((cp->c_flags & DEFLANG) &&
		    strcmp(cp->c_suffix, suffix) == 0)
			break;
		cp++;
	}
	if (! cp->c_suffix) cp = compbase;
	pass = cp->c_passes;
	while (pass->pp_name) {
		int first = 1;

		for (pinf=passinfo; strcmp(pass->pp_name,pinf->p_name);pinf++)
			;
		if (! (pinf->p_flags & LOADER) && applicable(pinf, suffix)) {
			int cont = ! stopsuffix || ! pinf->p_to ||
					strcmp(stopsuffix, pinf->p_to) != 0;
			name = apply(pinf,
				     cp,
				     name,
				     (int) (pass - cp->c_passes),
				     noremove,
				     first,
				     applicable(loaderinfo, pinf->p_to) ||
				      !cont ?
					strcat(base, ".") :
					tmpname);
			first = noremove = 0;
			suffix = pinf->p_to;
			if (!cont || !name) break;
		}
		pass++;
	}
	if (!noremove && name)
		append(&GEN_LDFILES, name);
	return name;
}

mkvec(call, in, out, pass, pinf)
	struct arglist *call;
	char *in, *out;
	struct pass *pass;
	register struct passinfo *pinf;
{
	register int i;

	init(call);
	append(call, pinf->p_path);
	scanflags(call, pinf);
	if (pass) for (i = 0; i < MAXHEAD; i++)
		if (pass->pp_head[i])
			append(call, pass->pp_head[i]);
		else	break;
	if (pinf->p_flags & INPUT && strcmp(in, "-") != 0)
		append(call, in);
	if (pinf->p_flags & OUTPUT)
		append(call, out);
	if (pinf->p_flags & O_OUTPUT) {
		append(call, "-o");
		append(call, out);
	}
	if (pass) for (i = 0; i < MAXTAIL; i++)
		if (pass->pp_tail[i])
			append(call, pass->pp_tail[i]);
		else	break;
}

callld(in, out, pass, pinf)
	struct arglist *in;
	char *out;
	struct pass *pass;
	register struct passinfo *pinf;
{
	struct arglist *call = &CALLVEC;
	register int i;

	init(call);
	append(call, pinf->p_path);
	scanflags(call, pinf);
	append(call, "-o");
	append(call, out);
	for (i = 0; i < MAXHEAD; i++)
		if (pass->pp_head[i])
			append(call, pass->pp_head[i]);
		else	break;
	if (pinf->p_flags & INPUT)
		concat(call, in);
	if (pinf->p_flags & OUTPUT)
		append(call, out);
	for (i = 0; i < MAXTAIL; i++) {
		if (pass->pp_tail[i]) {
			if (pass->pp_tail[i][0] == '-' &&
			    pass->pp_tail[i][1] == 'l') {
				append(call, library(&(pass->pp_tail[i][2])));
			}
			else if (*(pass->pp_tail[i]) != '*')
				append(call, pass->pp_tail[i]);
			else if (fp_lib)
				append(call, library("fp"));
		} else	break;
	}
	if (! runvec(call, pinf, (char *) 0, out)) {
		cleanup(out);
		RET_CODE = 1;
	}
}

clean(c)
	register struct arglist *c;
{
	register int i;

	for (i = 1; i < c->al_argc; i++) {
		free(c->al_argv[i]);
		c->al_argv[i] = 0;
	}
	c->al_argc = 0;
}

scanflags(call, pinf)
	struct arglist *call;
	struct passinfo *pinf;
{
	/*	Find out which flags from FLAGS must be passed to pass "pinf",
		and how. 
		Append them to "call"
	*/
	register int i;
	USTRING flg;

	for (i = 0; i < FLAGS.al_argc; i++) {
		register char *q = pinf->p_acceptflags;

		while (*q)  {
			register char *p = FLAGS.al_argv[i] + 1;

			while (*q && *q == *p) {
				q++; p++;
			}
			if (*q == ',' || !*q) {
				if (! *p) {
					/* append literally */
					append(call, FLAGS.al_argv[i]);
				}
				break;
			}
			if (*q == '*') {
				register char *s = flg;

				if (*++q != '=') {
					/* append literally */
					append(call, FLAGS.al_argv[i]);
					break;
				}
				*s++ = '-';
				if (*q) q++;	/* skip ',' */
				while (*q && *q != ',' && *q != '*') {
					/* copy replacement flag */
					*s++ = *q++;
				}
				if (*q == '*') {
					/* copy rest */
					while (*p) *s++ = *p++;
				}
				*s = 0;
				append(call, flg);
				break;
			}
			if (*q == '=') {
				/* copy replacement */
				register char *s = flg;

				*s++ = '-';
				q++;
				while (*q && *q != ',') *s++ = *q++;
				*s = 0;
				append(call, flg);
				break;
			}
			while (*q && *q++ != ',')
				;
		}
	}
}

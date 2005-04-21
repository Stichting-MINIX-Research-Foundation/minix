#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "partition.h"
#include "vfat.h"

const char *progname;

static const struct dispatch {
	const char *cmd;
	void (*fn)(int, char **, int);
	int type;
} dispatch[] = {
	{"attrib",mattrib, 0},
	{"badblocks",mbadblocks, 0},
	{"cat",mcat, 0},
	{"cd",mcd, 0},
	{"copy",mcopy, 0},
	{"del",mdel, 0},
	{"deltree",mdel, 2},
	{"dir",mdir, 0},
	{"doctorfat",mdoctorfat, 0},
	{"du",mdu, 0},
	{"format",mformat, 0},
	{"info", minfo, 0},
	{"label",mlabel, 0},
	{"md",mmd, 0},
	{"mkdir",mmd, 0},
#ifdef OS_linux
	{"mount",mmount, 0},
#endif
	{"partition",mpartition, 0},
	{"rd",mdel, 1},
	{"rmdir",mdel, 1},
	{"read",mcopy, 0},
	{"move",mmove, 0},
	{"ren",mmove, 1},
	{"showfat", mshowfat, 0},
#ifndef NO_CONFIG
	{"toolstest", mtoolstest, 0},
#endif
	{"type",mcopy, 1},
	{"write",mcopy, 0},
#ifndef OS_Minix
	{"zip", mzip, 0}
#endif
};
#define NDISPATCH (sizeof dispatch / sizeof dispatch[0])

int main(int argc,char **argv)
{
	const char *name;
	int i;

	init_privs();
#ifdef __EMX__
	_wildcard(&argc,&argv);
#endif

/*#define PRIV_TEST*/

#ifdef PRIV_TEST
	{ 
		int euid;
		char command[100];
	
		printf("INIT: %d %d\n", getuid(), geteuid());
		drop_privs();
		printf("DROP: %d %d\n", getuid(), geteuid());
		reclaim_privs();
		printf("RECLAIM: %d %d\n", getuid(), geteuid());
		euid = geteuid();
		if(argc & 1) {
			drop_privs();
			printf("DROP: %d %d\n", getuid(), geteuid());
		}
		if(!((argc-1) & 2)) {
			destroy_privs();
			printf("DESTROY: %d %d\n", getuid(), geteuid());
		}
		sprintf(command, "a.out %d", euid);
		system(command);
		return 1;
	}
#endif


#ifdef __EMX__
       _wildcard(&argc,&argv);
#endif 


	/* check whether the compiler lays out structures in a sane way */
	if(sizeof(struct partition) != 16 ||
	   sizeof(struct directory) != 32 ||
	   sizeof(struct vfat_subentry) !=32) {
		fprintf(stderr,"Mtools has not been correctly compiled\n");
		fprintf(stderr,"Recompile it using a more recent compiler\n");
		return 137;
	}

#ifdef __EMX__
       argv[0] = _getname(argv[0]); _remext(argv[0]); name = argv[0];
#else  
	name = _basename(argv[0]);
#endif

#if 0
	/* this allows the different tools to be called as "mtools -c <command>"
	** where <command> is mdir, mdel, mcopy etcetera
	** Mainly done for the BeOS, which doesn't support links yet.
	*/

	if(argc >= 3 && 
	   !strcmp(argv[1], "-c") &&
	   !strcmp(name, "mtools")) {
		argc-=2;
		argv+=2;
		name = argv[0];
	}
#endif

	/* print the version */
	if(argc >= 2 && 
	   (strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") ==0)) {
		printf("%c%s version %s, dated %s\n", 
		       toupper(name[0]), name+1,
		       mversion, mdate);
		printf("configured with the following options: ");
#ifdef USE_XDF
		printf("enable-xdf ");
#else
		printf("disable-xdf ");
#endif
#ifdef USING_VOLD
		printf("enable-vold ");
#else
		printf("disable-vold ");
#endif
#ifdef USING_NEW_VOLD
		printf("enable-new-vold ");
#else
		printf("disable-new-vold ");
#endif
#ifdef DEBUG
		printf("enable-debug ");
#else
		printf("disable-debug ");
#endif
#ifdef USE_RAWTERM
		printf("enable-raw-term ");
#else
		printf("disable-raw-term ");
#endif
		printf("\n");
		return 0;
	}

	if (argc >= 2 && strcmp(name, "mtools") == 0) {
		/* mtools command ... */
		argc--;
		argv++;
		name = argv[0];
	}
	progname = argv[0];

	read_config();
	setup_signal();
	for (i = 0; i < NDISPATCH; i++) {
		if (!strcmp(name,dispatch[i].cmd)
		    || (name[0] == 'm' && !strcmp(name+1,dispatch[i].cmd)))
			dispatch[i].fn(argc, argv, dispatch[i].type);
	}
	if (strcmp(name,"mtools"))
		fprintf(stderr,"Unknown mtools command '%s'\n",name);
	fprintf(stderr,"Usage: mtools [-V] command [-options] arguments ...\n");
	fprintf(stderr,"Supported commands:");
	for (i = 0; i < NDISPATCH; i++) {
		fprintf(stderr, i%8 == 0 ? "\n\t" : ", ");
		fprintf(stderr, "%s", dispatch[i].cmd);
	}
	putc('\n', stderr);
	fprintf(stderr, "Use 'mtools command -?' for help per command\n");

	return 1;
}

/*	$NetBSD: main.c,v 1.1.1.7 2010/01/30 21:33:31 joerg Exp $	*/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <nbcompat.h>
#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
__RCSID("$NetBSD: main.c,v 1.1.1.7 2010/01/30 21:33:31 joerg Exp $");

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the create module.
 *
 */

#if HAVE_ERR_H
#include <err.h>
#endif
#include "lib.h"
#include "create.h"

static const char Options[] = "B:C:D:EF:I:K:L:OP:S:T:UVb:c:d:f:g:i:k:ln:p:r:s:u:v";

char   *Prefix = NULL;
char   *Comment = NULL;
char   *Desc = NULL;
char   *Display = NULL;
char   *Install = NULL;
char   *DeInstall = NULL;
char   *Contents = NULL;
char   *Pkgdeps = NULL;
char   *BuildPkgdeps = NULL;
char   *Pkgcfl = NULL;
char   *BuildVersion = NULL;
char   *BuildInfo = NULL;
char   *SizePkg = NULL;
char   *SizeAll = NULL;
char   *Preserve = NULL;
char   *DefaultOwner = NULL;
char   *DefaultGroup = NULL;
char   *realprefix = NULL;
const char *CompressionType = NULL;
int	update_pkgdb = 1;
int	create_views = 0;
int     PlistOnly = 0;
int     RelativeLinks = 0;
Boolean File2Pkg = FALSE;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: pkg_create [-ElOUVv] [-B build-info-file] [-b build-version-file]\n"
            "                  [-C cpkgs] [-D displayfile] [-F compression] \n"
	    "                  [-I realprefix] [-i iscript]\n"
            "                  [-K pkg_dbdir] [-k dscript]\n"
            "                  [-n preserve-file] [-P dpkgs] [-p prefix] [-r rscript]\n"
            "                  [-S size-all-file] [-s size-pkg-file]\n"
	    "                  [-T buildpkgs] [-u owner] [-g group]\n"
            "                  -c comment -d description -f packlist\n"
            "                  pkg-name\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int     ch;

	setprogname(argv[0]);
	while ((ch = getopt(argc, argv, Options)) != -1)
		switch (ch) {
		case 'v':
			Verbose = TRUE;
			break;

		case 'E':
			create_views = 1;
			break;

		case 'F':
			CompressionType = optarg;
			break;

		case 'I':
			realprefix = optarg;
			break;

		case 'O':
			PlistOnly = 1;
			break;

		case 'U':
			update_pkgdb = 0;
			break;

		case 'p':
			Prefix = optarg;
			break;

		case 's':
			SizePkg = optarg;
			break;

		case 'S':
			SizeAll = optarg;
			break;

		case 'f':
			Contents = optarg;
			break;

		case 'c':
			Comment = optarg;
			break;

		case 'd':
			Desc = optarg;
			break;

		case 'g':
			DefaultGroup = optarg;
			break;

		case 'i':
			Install = optarg;
			break;

		case 'K':
			pkgdb_set_dir(optarg, 3);
			break;

		case 'k':
			DeInstall = optarg;
			break;

		case 'l':
			RelativeLinks = 1;
			break;

		case 'L':
			warnx("Obsolete -L option ignored");
			break;

		case 'u':
			DefaultOwner = optarg;
			break;

		case 'D':
			Display = optarg;
			break;

		case 'n':
			Preserve = optarg;
			break;

		case 'P':
			Pkgdeps = optarg;
			break;

		case 'T':
			BuildPkgdeps = optarg;
			break;

		case 'C':
			Pkgcfl = optarg;
			break;

		case 'b':
			BuildVersion = optarg;
			break;

		case 'B':
			BuildInfo = optarg;
			break;

		case 'V':
			show_version();
			/* NOTREACHED */

		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	pkg_install_config();

	if (argc == 0) {
		warnx("missing package name");
		usage();
	}
	if (argc != 1) {
		warnx("only one package name allowed");
		usage();
	}

	if (pkg_perform(*argv))
		return 0;
	if (Verbose) {
		if (PlistOnly)
			warnx("package registration failed");
		else
			warnx("package creation failed");
	}
	return 1;
}

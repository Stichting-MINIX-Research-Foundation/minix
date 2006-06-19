/* dir2html.c by Michael Temari 3/3/96 */

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

typedef struct namelist {	/* Obviously a list of names. */
	struct namelist	*next;
	char		name[1];
} namelist_t;

_PROTOTYPE(static void sort, (namelist_t **anl));
_PROTOTYPE(static namelist_t *collect, (char *dir));
_PROTOTYPE(int main, (int argc, char *argv[]));

static void sort(anl)
namelist_t **anl;
/* A stable mergesort disguised as line noise.  Must be called like this:
 *	if (L != NULL && L->next != NULL) sort(&L);
 */
{
    /* static */ namelist_t *nl1, **mid;  /* Need not be local */
    namelist_t *nl2;

    nl1 = *(mid = &(*anl)->next);
    do {
	if ((nl1 = nl1->next) == NULL) break;
	mid = &(*mid)->next;
    } while ((nl1 = nl1->next) != NULL);

    nl2 = *mid;
    *mid = NULL;

    if ((*anl)->next != NULL) sort(anl);
    if (nl2->next != NULL) sort(&nl2);

    nl1 = *anl;
    for (;;) {
	if (strcmp(nl1->name, nl2->name) <= 0) {
	    if ((nl1 = *(anl = &nl1->next)) == NULL) {
		*anl = nl2;
		break;
	    }
	} else {
	    *anl = nl2;
	    nl2 = *(anl = &nl2->next);
	    *anl = nl1;
	    if (nl2 == NULL) break;
	}
    }
}

static namelist_t *collect(dir)
char *dir;
/* Return a sorted list of directory entries.  Returns null with errno != 0
 * on error.
 */
{
    namelist_t *names, **pn = &names;
    DIR *dp;
    struct dirent *entry;

    if ((dp = opendir(dir)) == NULL) return NULL;

    while ((entry = readdir(dp)) != NULL) {
	if (strcmp(entry->d_name, ".") == 0) continue;
	*pn = malloc(offsetof(namelist_t, name) + strlen(entry->d_name) + 1);
	if (*pn == NULL) {
	    closedir(dp);
	    errno = ENOMEM;
	    return NULL;
	}
	strcpy((*pn)->name, entry->d_name);
	pn = &(*pn)->next;
    }
    closedir(dp);
    *pn = NULL;
    if (names != NULL && names->next != NULL) sort(&names);
    errno = 0;
    return names;
}

int main(argc, argv)
int argc;
char *argv[];
{
    namelist_t *np;
    char *rpath, *vpath;
    static char cwd[1024];
    static char work[64];
    char *filename;
    struct stat st;
    struct tm *tmp;
    static char month[][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    if(argc > 1) {
	rpath = argv[1];
	if (chdir(rpath) < 0) {
	    fprintf(stderr, "dir2html: %s: %s\n", rpath, strerror(errno));
	    return(-1);
    	}
    } else {
	if(getcwd(cwd, sizeof(cwd)) == NULL) {
	    fprintf(stderr, "dir2html: getcwd(): %s", strerror(errno));
	    return(-1);
	}
	rpath = cwd;
    }

    if(argc > 2) {
	vpath = argv[2];
    } else {
	vpath = rpath;
    }

    if ((np = collect(".")) == NULL && errno != 0) {
	fprintf(stderr, "dir2html: %s: %s\n", vpath, strerror(errno));
	return(-1);
    }

    printf("<HTML><HEAD><TITLE>Index of %s</TITLE></HEAD>\n", vpath);
    printf("<BODY>\n");
    printf("<H1>Index of %s</H1>\n", vpath);

    printf("<PRE>\n");
    printf("%-22s  %-17s  %s\n", "Name", "Last modified", "Size/Type");
    printf("<HR>\n");

    while (np != NULL) {
	errno = 0;
	filename = np->name;
	np= np->next;

	if (stat(filename, &st) < 0) continue;

	printf("<A HREF=\"%s%s\">",
	    filename, S_ISDIR(st.st_mode) ? "/" : "");
	sprintf(work, "%.23s%s",
	    filename, S_ISDIR(st.st_mode) ? "/" : "");
	if (strcmp(filename, "..") == 0) strcpy(work, "Parent Directory");
	printf("%-22.22s%s</A>",
	    work, strlen(work) > 22 ? "&gt;" : " ");
	tmp = localtime(&st.st_mtime);
	printf(" %02d %s %d %02d:%02d",
	    tmp->tm_mday, month[tmp->tm_mon], 1900+tmp->tm_year,
	    tmp->tm_hour, tmp->tm_min);
	if (S_ISREG(st.st_mode)) {
	    if (st.st_size < 10240) {
		sprintf(work, "%lu ", (unsigned long) st.st_size);
	    } else
	    if (st.st_size < 10240 * 1024L) {
		sprintf(work, "%luK",
		    ((unsigned long) st.st_size - 1) / 1024 + 1);
	    } else {
		sprintf(work, "%luM",
		    ((unsigned long) st.st_size - 1) / (1024 * 1024L) + 1);
	    }
	} else {
	    strcpy(work,
		S_ISDIR(st.st_mode) ? "[dir]" :
		S_ISBLK(st.st_mode) ? "[block]" :
		S_ISCHR(st.st_mode) ? "[char]" :
		S_ISFIFO(st.st_mode) ? "[pipe]" :
					"[???]");
	}
	printf(" %8s\n", work);
    }

    printf("</PRE>\n");

    printf("<HR>\n");
    printf("<SMALL><i>Minix httpd 0.99</i></SMALL>\n");
    printf("</BODY>\n");
    printf("</HTML>\n");

    return(0);
}

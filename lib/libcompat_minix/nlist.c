/*
 * "nlist.c", Peter Valkenburg, january 1989.
 */
 
#include <lib.h>
#include <string.h>
#include <a.out.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#define fail(fp)	(fclose(fp), -1)	/* ret. exp. when nlist fails */

_PROTOTYPE( int nlist, (char *file, struct nlist nl[]));

/*
 * Nlist fills fields n_sclass and n_value of array nl with values found in
 * non-stripped executable file.  Entries that are not found have their
 * n_value/n_sclass fields set to 0.  Nl ends with a 0 or nul string n_name.
 * The return value is -1 on failure, else the number of entries not found.
 */
int nlist(file, nl)
char *file;
struct nlist nl[];
{
	int nents, nsrch, nfound, i;
	struct nlist nlent;
	FILE *fp;
	struct exec hd;

	/* open executable with namelist */
	if ((fp = fopen(file, "r")) == NULL)
		return -1;
		
	/* get header and seek to start of namelist */	
	if (fread((char *) &hd, sizeof(struct exec), 1, fp) != 1 ||
	    BADMAG(hd) || fseek(fp, A_SYMPOS(hd), SEEK_SET) != 0)
		return fail(fp);
	
	/* determine number of entries searched for & reset fields */
	nsrch = 0;
	while (nl[nsrch].n_name != NULL && *(nl[nsrch].n_name) != '\0') {
		nl[nsrch].n_sclass = 0;
		nl[nsrch].n_value = 0;
		nl[nsrch].n_type = 0;		/* for compatability */
		nsrch++;
	}

	/* loop through namelist & fill in user array */
	nfound = 0;
	for (nents = (hd.a_syms & 0xFFFF) / sizeof(struct nlist);
	     nents > 0; nents--) {
		if (nsrch == nfound)
			break;			/* no need to look further */
		if (fread((char *) &nlent, sizeof(struct nlist), 1, fp) != 1)
			return fail(fp);	  
		for (i = 0; i < nsrch; i++)
			if (nl[i].n_sclass == 0 &&
			    strncmp(nl[i].n_name, nlent.n_name,
			    	    sizeof(nlent.n_name)) == 0) {
				nl[i] = nlent;
				nfound++;
				break;
			}
	}

	(void) fclose(fp);
	
	return nsrch - nfound;
}

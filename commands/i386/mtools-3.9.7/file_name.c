#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "vfat.h"
#include "codepage.h"

/* Write a DOS name + extension into a legal unix-style name.  */
char *unix_normalize (char *ans, char *name, char *ext)
{
	char *a;
	int j;
	
	for (a=ans,j=0; (j<8) && (name[j] > ' '); ++j,++a)
		*a = name[j];
	if(*ext > ' ') {
		*a++ = '.';
		for (j=0; j<3 && ext[j] > ' '; ++j,++a)
			*a = ext[j];
	}
	*a++ = '\0';
	return ans;
}

typedef enum Case_l {
	NONE,
	UPPER,
	LOWER 
} Case_t;

static void TranslateToDos(const char *s, char *t, int count,
			   char *end, Case_t *Case, int *mangled)
{
	*Case = NONE;
	for( ;  *s && (s < end || !end); s++) {
		if(!count) {
			*mangled |= 3;
			break;
		}
		/* skip spaces & dots */
		if(*s == ' ' || *s == '.') {
			*mangled |= 3;
			continue;
		}

		/* convert to dos */
		if((*s) & 0x80) {
			*mangled |= 1;
			*t = to_dos(*s);
		}

		if ((*s & 0x7f) < ' ' ) {
			*mangled |= 3;
			*t = '_';
		} else if (islower((unsigned char)*s)) {
			*t = toupper(*s);
			if(*Case == UPPER && !mtools_no_vfat)
				*mangled |= 1;
			else
				*Case = LOWER;
		} else if (isupper((unsigned char)*s)) {
			*t = *s;
			if(*Case == LOWER && !mtools_no_vfat)
				*mangled |= 1;
			else
				*Case = UPPER;
		} else if((*s) & 0x80)
			*t = mstoupper(*t);	/* upper case */
		else
			*t = *s;
		count--;
		t++;
	}
}

/* dos_name
 *
 * Convert a Unix-style filename to a legal MSDOS name and extension.
 * Will truncate file and extension names, will substitute
 * the character '~' for any illegal character(s) in the name.
 */
char *dos_name(char *name, int verbose, int *mangled, char *ans)
{
	char *s, *ext;
	register int i;
	Case_t BaseCase, ExtCase;

	*mangled = 0;

	/* skip drive letter */
	name = skip_drive(name);

	/* zap the leading path */
	name = (char *) _basename(name);
	if ((s = strrchr(name, '\\')))
		name = s + 1;
	
	memset(ans, ' ', 11);
	ans[11]='\0';

	/* skip leading dots and spaces */
	i = strspn(name, ". ");
	if(i) {
		name += i;
		*mangled = 3;
	}
		
	ext = strrchr(name, '.');

	/* main name */
	TranslateToDos(name, ans, 8, ext, &BaseCase, mangled);
	if(ext)
		TranslateToDos(ext+1, ans+8, 3, 0, &ExtCase,  mangled);

	if(*mangled & 2)
		autorename_short(ans, 0);

	if(!*mangled) {
		if(BaseCase == LOWER)
			*mangled |= BASECASE;
		if(ExtCase == LOWER)
			*mangled |= EXTCASE;
		if((BaseCase == LOWER || ExtCase == LOWER) &&
		   !mtools_no_vfat) {
		  *mangled |= 1;
		}
	}
	return ans;
}


/*
 * Get rid of spaces in an MSDOS 'raw' name (one that has come from the
 * directory structure) so that it can be used for regular expression
 * matching with a Unix filename.  Also used to 'unfix' a name that has
 * been altered by dos_name().
 */

char *unix_name(char *name, char *ext, char Case, char *ans)
{
	char *s, tname[9], text[4];
	int i;

	strncpy(tname, (char *) name, 8);
	tname[8] = '\0';
	if ((s = strchr(tname, ' ')))
		*s = '\0';

	if(!(Case & (BASECASE | EXTCASE)) && mtools_ignore_short_case)
		Case |= BASECASE | EXTCASE;

	if(Case & BASECASE)
		for(i=0;i<8 && tname[i];i++)
			tname[i] = tolower(tname[i]);

	strncpy(text, (char *) ext, 3);
	text[3] = '\0';
	if ((s = strchr(text, ' ')))
		*s = '\0';

	if(Case & EXTCASE)
		for(i=0;i<3 && text[i];i++)
			text[i] = tolower(text[i]);

	if (*text) {
		strcpy(ans, tname);
		strcat(ans, ".");
		strcat(ans, text);
	} else
		strcpy(ans, tname);

	/* fix special characters (above 0x80) */
	to_unix(ans,11);
	return(ans);
}

/* If null encountered, set *end to 0x40 and write nulls rest of way
 * 950820: Win95 does not like this!  It complains about bad characters.
 * So, instead: If null encountered, set *end to 0x40, write the null, and
 * write 0xff the rest of the way (that is what Win95 seems to do; hopefully
 * that will make it happy)
 */
/* Always return num */
int unicode_write(char *in, struct unicode_char *out, int num, int *end_p)
{
	int j;

	for (j=0; j<num; ++j) {
		out->uchar = '\0';	/* Hard coded to ASCII */
		if (*end_p)
			/* Fill with 0xff */
			out->uchar = out->lchar = (char) 0xff;
		else {
			out->lchar = *in;
			if (! *in) {
				*end_p = VSE_LAST;
			}
		}

		++out;
		++in;
	}
	return num;
}

/*
 * Miscellaneous routines.
 */

#include "sysincludes.h"
#include "msdos.h"
#include "stream.h"
#include "vfat.h"
#include "mtools.h"


void printOom(void)
{
	fprintf(stderr, "Out of memory error");
}

char *get_homedir(void)
{
	struct passwd *pw;
	uid_t uid;
	char *homedir;
	char *username;
	
	homedir = getenv ("HOME");    
	/* 
	 * first we call getlogin. 
	 * There might be several accounts sharing one uid 
	 */
	if ( homedir )
		return homedir;
	
	pw = 0;
	
	username = getenv("LOGNAME");
	if ( !username )
		username = getlogin();
	if ( username )
		pw = getpwnam( username);
  
	if ( pw == 0 ){
		/* if we can't getlogin, look up the pwent by uid */
		uid = geteuid();
		pw = getpwuid(uid);
	}
	
	/* we might still get no entry */
	if ( pw )
		return pw->pw_dir;
	return 0;
}


static void get_mcwd_file_name(char *file)
{
	char *mcwd_path;
	char *homedir;

	mcwd_path = getenv("MCWD");
	if (mcwd_path == NULL || *mcwd_path == '\0'){
		homedir= get_homedir();
		if(!homedir)
			homedir="/tmp";
		strncpy(file, homedir, MAXPATHLEN-6);
		file[MAXPATHLEN-6]='\0';
		strcat( file, "/.mcwd");
	} else {
		strncpy(file, mcwd_path, MAXPATHLEN);
		file[MAXPATHLEN]='\0';
	}
}

void unlink_mcwd()
{
	char file[MAXPATHLEN+1];
	get_mcwd_file_name(file);
	unlink(file);
}

FILE *open_mcwd(const char *mode)
{
	struct stat sbuf;
	char file[MAXPATHLEN+1];
	time_t now;
	
	get_mcwd_file_name(file);
	if (*mode == 'r'){
		if (stat(file, &sbuf) < 0)
			return NULL;
		/*
		 * Ignore the info, if the file is more than 6 hours old
		 */
		getTimeNow(&now);
		if (now - sbuf.st_mtime > 6 * 60 * 60) {
			fprintf(stderr,
				"Warning: \"%s\" is out of date, removing it\n",
				file);
			unlink(file);
			return NULL;
		}
	}
	
	return  fopen(file, mode);
}
	

/* Fix the info in the MCWD file to be a proper directory name.
 * Always has a leading separator.  Never has a trailing separator
 * (unless it is the path itself).  */

const char *fix_mcwd(char *ans)
{
	FILE *fp;
	char *s;
	char buf[MAX_PATH];

	fp = open_mcwd("r");
	if(!fp){
		strcpy(ans, "A:/");
		return ans;
	}

	if (!fgets(buf, MAX_PATH, fp))
		return("A:/");

	buf[strlen(buf) -1] = '\0';
	fclose(fp);
					/* drive letter present? */
	s = skip_drive(buf);
	if (s > buf) {
		strncpy(ans, buf, s - buf);
		ans[s - buf] = '\0';
	} else 
		strcpy(ans, "A:");
					/* add a leading separator */
	if (*s != '/' && *s != '\\') {
		strcat(ans, "/");
		strcat(ans, s);
	} else
		strcat(ans, s);

#if 0
					/* translate to upper case */
	for (s = ans; *s; ++s) {
		*s = toupper(*s);
		if (*s == '\\')
			*s = '/';
	}
#endif
					/* if only drive, colon, & separator */
	if (strlen(ans) == 3)
		return(ans);
					/* zap the trailing separator */
	if (*--s == '/')
		*s = '\0';
	return ans;
}

void *safe_malloc(size_t size)
{
	void *p;

	p = malloc(size);
	if(!p){
		printOom();
		exit(1);
	}
	return p;
}

void print_sector(char *message, unsigned char *data, int size)
{
	int col;
	int row;

	printf("%s:\n", message);
	
	for(row = 0; row * 16 < size; row++){
		printf("%03x  ", row * 16);
		for(col = 0; col < 16; col++)			
			printf("%02x ", data [row*16+col]);
		for(col = 0; col < 16; col++) {
			if(isprint(data [row*16+col]))
				printf("%c", data [row*16+col]);
			else
				printf(".");
		}
		printf("\n");
	}
}


time_t getTimeNow(time_t *now)
{
	static int haveTime = 0;
	static time_t sharedNow;

	if(!haveTime) {
		time(&sharedNow);
		haveTime = 1;
	}
	if(now)
		*now = sharedNow;
	return sharedNow;
}

char *skip_drive(const char *filename)
{
	char *p;

	/* Skip drive name.  Return pointer just after the `:', or a pointer
	 * to the start of the file name if there is is no drive name.
	 */
	p = strchr(filename, ':');
	return (p == NULL || p == filename) ? (char *) filename : p + 1;
}

char *get_drive(const char *filename, const char *def)
{
	const char *path;
	char *drive;
	const char *rest;
	size_t len;

	/* Return the drive name part of a full filename. */

	path = filename;
	rest = skip_drive(path);
	if (rest == path) {
		if (def == NULL) def = "A:";
		path = def;
		rest = skip_drive(path);
		if (rest == path) {
			path = "A:";
			rest = path+2;
		}
	}
	len = rest - path;
	drive = safe_malloc(len * sizeof(drive[0]));
	len--;
	memcpy(drive, path, len);
	drive[len] = 0;
	if (len == 1) drive[0] = toupper(drive[0]);
	return drive;
}

#if 0

#undef free
#undef malloc

static int total=0;

void myfree(void *ptr)
{
	int *size = ((int *) ptr)-1;
	total -= *size;
	fprintf(stderr, "freeing %d bytes at %p total alloced=%d\n",
		*size, ptr, total);
	free(size);
}

void *mymalloc(size_t size)
{
	int *ptr;
	ptr = (int *)malloc(size+sizeof(int));
	if(!ptr)
		return 0;
	*ptr = size;
	ptr++;
	total += size;
	fprintf(stderr, "allocating %d bytes at %p total allocated=%d\n",
		size, ptr, total);
	return (void *) ptr;
}

void *mycalloc(size_t nmemb, size_t size)
{
	void *ptr = mymalloc(nmemb * size);
	if(!ptr)
		return 0;
	memset(ptr, 0, size);
	return ptr;
}

void *myrealloc(void *ptr, size_t size)
{
	int oldsize = ((int *)ptr) [-1];
	void *new = mymalloc(size);
	if(!new)
		return 0;
	memcpy(new, ptr, oldsize);
	myfree(ptr);
	return new;
}

char *mystrdup(char *src)
{
	char *dest;
	dest = mymalloc(strlen(src)+1);
	if(!dest)
		return 0;
	strcpy(dest, src);
	return dest;
}


#endif

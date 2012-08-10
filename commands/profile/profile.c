/* profile - profile operating system
 *
 * The profile command is used to control Statistical and Call Profiling.
 * It writes the profiling data collected by the kernel to a file.
 *
 * Changes:
 *   14 Aug, 2006   Created (Rogier Meurs)
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#undef SPROFILE
#define SPROFILE 1
#include <minix/profile.h>

#define EHELP			1
#define ESYNTAX			2
#define EMEM			3
#define EOUTFILE		4
#define EFREQ			5
#define EACTION			6

#define START			1
#define STOP			2
#define GET			3
#define RESET			4

#define SPROF			(action==START||action==STOP)
#define CPROF			(!SPROF)
#define DEF_OUTFILE_S		"profile.stat.out"
#define DEF_OUTFILE_C		"profile.call.out"
#define DEF_OUTFILE		(SPROF?DEF_OUTFILE_S:DEF_OUTFILE_C)
#define NPIPE			"/tmp/profile.npipe"
#define MIN_MEMSIZE		1
#define DEF_MEMSIZE		64
#define MIN_FREQ		3
#define MAX_FREQ		15
#define DEF_FREQ		6
#define BUFSIZE			1024
#define MB			(1024*1024)
#define SYNCING			"SYNC"
#define DEV_LOG			"/dev/log"

int action = 0;
int mem_size = 0;
int mem_used = 0;
int freq = 0;
int intr_type = PROF_RTC;
char *outfile = "";
char *mem_ptr;
int outfile_fd, npipe_fd;
struct sprof_info_s sprof_info;
struct cprof_info_s cprof_info;

#define HASH_MOD	128
struct sproc {
	endpoint_t	ep;
	char		name[8];
	struct sproc *	next;
};

static struct sproc * proc_hash[HASH_MOD];

int handle_args(int argc, char *argv[]);
int start(void);
int stop(void);
int get(void);
int reset(void);
int create_named_pipe(void);
int alloc_mem(void);
int init_outfile(void);
int write_outfile(void);
int write_outfile_cprof(void);
int write_outfile_sprof(void);
void detach(void);

int main(int argc, char *argv[])
{
  int res;

  if ((res = handle_args(argc, argv))) {
	switch(res) {
		case ESYNTAX:
			printf("Error in parameters.\n");
			return 1;
			break;
		case EACTION:
			printf("Specify one of start|stop|get|reset.\n");
			return 1;
			break;
		case EMEM:
			printf("Incorrect memory size.\n");
			return 1;
			break;
		case EFREQ:
			printf("Incorrect frequency.\n");
			return 1;
			break;
		case EOUTFILE:
			printf("Output filename missing.\n");
			return 1;
			break;
		default:
			break;
	}

	/*
	 * Check the frequency when we know the intr type. Only selected values
	 * are correct for RTC
	 */
	if (action == START && intr_type == PROF_RTC &&
			(freq < MIN_FREQ || freq > MAX_FREQ)) {
		printf("Incorrect frequency.\n");
		return 1;
	}

        printf("Statistical Profiling:\n");
	printf("  profile start [--rtc | --nmi] "
			"[-m memsize] [-o outfile] [-f frequency]\n");
        printf("  profile stop\n\n");
	printf("   --rtc is default, --nmi allows kernel profiling\n");
        printf("Call Profiling:\n");
	printf("  profile get   [-m memsize] [-o outfile]\n");
        printf("  profile reset\n\n");
	printf("   - memsize in MB, default: %u\n", DEF_MEMSIZE);
	printf("   - default output file: profile.{stat|call}.out\n");
	printf( "   - sample frequencies for --rtc (default: %u):\n", DEF_FREQ);
	printf("      3    8192 Hz          10     64 Hz\n");
	printf("      4    4096 Hz          11     32 Hz\n");
	printf("      5    2048 Hz          12     16 Hz\n");
	printf("      6    1024 Hz          13      8 Hz\n");
	printf("      7     512 Hz          14      4 Hz\n");
	printf("      8     256 Hz          15      2 Hz\n");
	printf("      9     128 Hz\n\n");
	printf("Use [sc]profalyze.pl to analyze output file.\n");
	return 1;
  }

  switch(action) {
	  case START:
	  	if (start()) return 1;
		break;
	  case STOP:
	  	if (stop()) return 1;
		break;
	  case GET:
	  	if (get()) return 1;
		break;
	  case RESET:
	  	if (reset()) return 1;
		break;
	  default:
		break;
  }
  return 0;
}


int handle_args(int argc, char *argv[])
{
  while (--argc) {
	++argv;

	if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "help") == 0 ||
		strcmp(*argv, "--help") == 0) {
		return EHELP;
	} else
	if (strcmp(*argv, "-m") == 0) {
		if (--argc == 0) return ESYNTAX;
		if (sscanf(*++argv, "%u", &mem_size) != 1 ||
			mem_size < MIN_MEMSIZE ) return EMEM;
	} else
	if (strcmp(*argv, "-f") == 0) {
		if (--argc == 0) return ESYNTAX;
		if (sscanf(*++argv, "%u", &freq) != 1)
			return EFREQ;
	} else
	if (strcmp(*argv, "-o") == 0) {
		if (--argc == 0) return ESYNTAX;
		outfile = *++argv;
	} else
	if (strcmp(*argv, "--rtc") == 0) {
		intr_type = PROF_RTC;
	} else
	if (strcmp(*argv, "--nmi") == 0) {
		intr_type = PROF_NMI;
	} else
	if (strcmp(*argv, "start") == 0) {
		if (action) return EACTION;
	       	action = START;
	} else
	if (strcmp(*argv, "stop") == 0) {
		if (action) return EACTION;
	       	action = STOP;
	} else
	if (strcmp(*argv, "get") == 0) {
		if (action) return EACTION;
	       	action = GET;
	} else
	if (strcmp(*argv, "reset") == 0) {
		if (action) return EACTION;
	       	action = RESET;
	}
  }

  /* No action specified. */
  if (!action) return EHELP;

  /* Init unspecified parameters. */
  if (action == START || action == GET) {
	if (strcmp(outfile, "") == 0) outfile = DEF_OUTFILE;
	if (mem_size == 0) mem_size = DEF_MEMSIZE;
	mem_size *= MB;				   /* mem_size in bytes */
  }
  if (action == START) {
	mem_size -= mem_size % sizeof(struct sprof_sample); /* align to sample size */
	if (freq == 0) freq = DEF_FREQ;		   /* default frequency */
  }
  return 0;
}


int start()
{
 /* This is the "starter process" for statistical profiling.
  *
  * Create output file for profiling data.  Create named pipe to
  * synchronize with stopper process.  Fork so the parent can exit.
  * Allocate memory for profiling data.  Start profiling in kernel.
  * Complete detachment from terminal.  Write known string to named
  * pipe, which blocks until read by stopper process.  Redirect
  * stdout/stderr to the named pipe.  Write profiling data to file.
  * Clean up.
  */
  int log_fd;

  if (init_outfile() || create_named_pipe()) return 1;

  printf("Starting statistical profiling.\n");

  if (fork() != 0) exit(0);

  if (alloc_mem()) return 1;

  if (sprofile(PROF_START, mem_size, freq, intr_type, &sprof_info, mem_ptr)) {
	perror("sprofile");
	fprintf(stderr, "Error starting profiling.\n");
	return 1;
  }

  detach();

  /* Temporarily redirect to system log to catch errors. */
  log_fd = open(DEV_LOG, O_WRONLY);
  dup2(log_fd, 1);
  dup2(log_fd, 2);

  if ((npipe_fd = open(NPIPE, O_WRONLY)) < 0) {
	fprintf(stderr, "Unable to open named pipe %s.\n", NPIPE);
	return 1;
  } else
	/* Synchronize with stopper process. */
	write(npipe_fd, SYNCING, strlen(SYNCING));

  /* Now redirect to named pipe. */
  dup2(npipe_fd, 1);
  dup2(npipe_fd, 2);

  mem_used = sprof_info.mem_used;

  if (mem_used == -1) {
	  fprintf(stderr, "WARNING: Profiling was stopped prematurely due to ");
	  fprintf(stderr, "insufficient memory.\n");
	  fprintf(stderr, "Try increasing available memory using the -m switch.\n");
  }

  if (write_outfile()) return 1;

  close(log_fd);
  close(npipe_fd);
  unlink(NPIPE);
  close(outfile_fd);
  free(mem_ptr);

  return 0;
}


int stop()
{
  /* This is the "stopper" process for statistical profiling.
   *
   * Stop profiling in kernel.  Read known string from named pipe
   * to synchronize with starter proces.  Read named pipe until EOF
   * and write to stdout, this allows feedback from started process
   * to be printed.
   */
  int n;
  char buf[BUFSIZE];

  if (sprofile(PROF_STOP, 0, 0, 0, 0, 0)) {
	perror("sprofile");
	fprintf(stderr, "Error stopping profiling.\n");
  	return 1;
  } else printf("Statistical profiling stopped.\n");

  if ((npipe_fd = open(NPIPE, O_RDONLY)) < 0) {
	fprintf(stderr, "Unable to open named pipe %s.\n", NPIPE);
	return 1;
  } else
	/* Synchronize with starter process. */
	read(npipe_fd, buf, strlen(SYNCING));

  while ((n = read(npipe_fd, buf, BUFSIZE)) > 0)
	write(1, buf, n);

  close(npipe_fd);

  return 0;
}


int get()
{
 /* Get function for call profiling.
  *
  * Create output file.  Allocate memory.  Perform system call to get
  * profiling table and write it to file.  Clean up.
  */
  if (init_outfile()) return 1;

  printf("Getting call profiling data.\n");

  if (alloc_mem()) return 1;

  if (cprofile(PROF_GET, mem_size, &cprof_info, mem_ptr)) {
	perror("cprofile");
	fprintf(stderr, "Error getting data.\n");
	return 1;
  }

  mem_used = cprof_info.mem_used;

  if (mem_used == -1) {
	fprintf(stderr, "ERROR: unable to get data due to insufficient memory.\n");
	fprintf(stderr, "Try increasing available memory using the -m switch.\n");
  } else
  if (cprof_info.err) {
	fprintf(stderr, "ERROR: the following error(s) happened during profiling:\n");
	if (cprof_info.err & CPROF_CPATH_OVERRUN)
		fprintf(stderr, " call path overrun\n");
	if (cprof_info.err & CPROF_STACK_OVERRUN)
		fprintf(stderr, " call stack overrun\n");
	if (cprof_info.err & CPROF_TABLE_OVERRUN)
		fprintf(stderr, " hash table overrun\n");
	fprintf(stderr, "Try changing values in /usr/src/include/minix/profile.h ");
	fprintf(stderr, "and then rebuild the system.\n");
  } else
  if (write_outfile()) return 1;

  close(outfile_fd);
  free(mem_ptr);

  return 0;
}


int reset()
{
 /* Reset function for call profiling.
  *
  * Perform system call to reset profiling table.
  */
  printf("Resetting call profiling data.\n");

  if (cprofile(PROF_RESET, 0, 0, 0)) {
	perror("cprofile");
	fprintf(stderr, "Error resetting data.\n");
	return 1;
  }

  return 0;
}


int alloc_mem()
{
  if ((mem_ptr = malloc(mem_size)) == 0) {
	fprintf(stderr, "Unable to allocate memory.\n");
	fprintf(stderr, "Used chmem to increase available proces memory?\n");
	return 1;
  } else memset(mem_ptr, '\0', mem_size);

  return 0;
}


int init_outfile()
{
  if ((outfile_fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY)) <= 0) {
	fprintf(stderr, "Unable to create outfile %s.\n", outfile);
	return 1;
  } else chmod(outfile, S_IRUSR | S_IWUSR);

  return 0;
}


int create_named_pipe()
{
  if ((mkfifo(NPIPE, S_IRUSR | S_IWUSR) == -1) && (errno != EEXIST)) {
	fprintf(stderr, "Unable to create named pipe %s.\n", NPIPE);
	return 1;
  } else
	return 0;
}


void detach()
{
  setsid();
  (void) chdir("/");
  close(0);
  close(1);
  close(2);
}

static void add_proc(struct sprof_proc * p)
{
	struct sproc * n;
	int slot = ((unsigned)(p->proc)) % HASH_MOD;

	n = malloc(sizeof(struct sproc));
	if (!n)
		abort();
	n->ep = p->proc;
	memcpy(n->name, p->name, 8);
	n->next = proc_hash[slot];
	proc_hash[slot] = n;
}

static char * get_proc_name(endpoint_t ep)
{
	struct sproc * p;

	for (p = proc_hash[((unsigned)ep) % HASH_MOD]; p; p = p->next) {
		if (p->ep == ep)
			return p->name;
	}

	return NULL;
}

int write_outfile()
{
  ssize_t n;
  int written;
  char header[80];

  printf("Writing to %s ...", outfile);

  /* Write header. */
  if (SPROF)
	sprintf(header, "stat\n%u %u %u\n",	sizeof(struct sprof_info_s),
						sizeof(struct sprof_sample),
					  	sizeof(struct sprof_proc));
  else
	sprintf(header, "call\n%u %u\n",
				CPROF_CPATH_MAX_LEN, CPROF_PROCNAME_LEN);

  n = write(outfile_fd, header, strlen(header));

  if (n < 0) {
	fprintf(stderr, "Error writing to outfile %s.\n", outfile);
	return 1;
  }

  /* for sprofile, raw data will do; cprofile is handled by a script that needs
   * some preprocessing to be done by us
   */
  if (SPROF) {
	written = write_outfile_sprof();
  } else {
	written = write_outfile_cprof();
  }
  if (written < 0) return -1;

  printf(" header %d bytes, data %d bytes.\n", strlen(header), written);
  return 0;
}

int write_outfile_cprof()
{
  int towrite, written = 0;
  struct sprof_sample *sample;

  /* Write data. */
  towrite = mem_used == -1 ? mem_size : mem_used;

  sample = (struct sprof_sample *) mem_ptr;
  while (towrite > 0) {
	  unsigned bytes;
	  char	entry[12];
	  char * name;

	  name = get_proc_name(sample->proc);
	  if (!name) {
		  add_proc((struct sprof_proc *)sample);
		  bytes = sizeof(struct sprof_proc);
		  towrite -= bytes;
		  sample = (struct sprof_sample *)(((char *) sample) + bytes);
		  continue;
	  }

	  memset(entry, 0, 12);
	  memcpy(entry, name, strlen(name));
	  memcpy(entry + 8, &sample->pc, 4);

	  if (write(outfile_fd, entry, 12) != 12) {
		  fprintf(stderr, "Error writing to outfile %s.\n", outfile);
		  return -1;
	  }
	  towrite -= sizeof(struct sprof_sample);
	  sample++;

	  written += 12;
  }

  return written;
}

int write_outfile_sprof()
{
  int towrite;
  ssize_t written, written_total = 0;

  /* write profiling totals */
  written = write(outfile_fd, &sprof_info, sizeof(sprof_info));
  if (written != sizeof(sprof_info)) goto error;
  written_total += written;

  /* write raw samples */
  towrite = mem_used == -1 ? mem_size : mem_used;
  written = write(outfile_fd, mem_ptr, towrite);
  if (written != towrite) goto error;
  written_total += written;

  return written_total;

error:
  fprintf(stderr, "Error writing to outfile %s.\n", outfile);
  return -1;
}

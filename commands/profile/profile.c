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
char *outfile = "";
char *mem_ptr;
int outfile_fd, npipe_fd;
struct sprof_info_s sprof_info;
struct cprof_info_s cprof_info;

_PROTOTYPE(int handle_args, (int argc, char *argv[]));
_PROTOTYPE(int start, (void));
_PROTOTYPE(int stop, (void));
_PROTOTYPE(int get, (void));
_PROTOTYPE(int reset, (void));
_PROTOTYPE(int create_named_pipe, (void));
_PROTOTYPE(int alloc_mem, (void));
_PROTOTYPE(int init_outfile, (void));
_PROTOTYPE(int write_outfile, (void));
_PROTOTYPE(int detach, (void));

int main(int argc, char *argv[])
{
  int res;

  if (res = handle_args(argc, argv)) {
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

        printf("Statistical Profiling:\n");
	printf("  profile start [-m memsize] [-o outfile] [-f frequency]\n");
        printf("  profile stop\n\n");
        printf("Call Profiling:\n");
	printf("  profile get   [-m memsize] [-o outfile]\n");
        printf("  profile reset\n\n");
	printf("   - memsize in MB, default: %u\n", DEF_MEMSIZE);
	printf("   - default output file: profile.{stat|call}.out\n");
	printf( "   - sample frequencies (default: %u):\n", DEF_FREQ);
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
		if (sscanf(*++argv, "%u", &freq) != 1 ||
			freq < MIN_FREQ || freq > MAX_FREQ) return EFREQ;
	} else
	if (strcmp(*argv, "-o") == 0) {
		if (--argc == 0) return ESYNTAX;
		outfile = *++argv;
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
	mem_size -= mem_size % sizeof(sprof_sample); /* align to sample size */
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

  if (sprofile(PROF_START, mem_size, freq, &sprof_info, mem_ptr)) {
	perror("sprofile");
	printf("Error starting profiling.\n");
	return 1;
  }

  detach();

  /* Temporarily redirect to system log to catch errors. */
  log_fd = open(DEV_LOG, O_WRONLY);
  dup2(log_fd, 1);
  dup2(log_fd, 2);

  if ((npipe_fd = open(NPIPE, O_WRONLY)) < 0) {
	printf("Unable to open named pipe %s.\n", NPIPE);
	return 1;
  } else
	/* Synchronize with stopper process. */
	write(npipe_fd, SYNCING, strlen(SYNCING));

  /* Now redirect to named pipe. */
  dup2(npipe_fd, 1);
  dup2(npipe_fd, 2);

  mem_used = sprof_info.mem_used;

  if (mem_used == -1) {
	  printf("WARNING: Profiling was stopped prematurely due to ");
	  printf("insufficient memory.\n");
	  printf("Try increasing available memory using the -m switch.\n");
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

  if (sprofile(PROF_STOP, 0, 0, 0, 0)) {
	perror("sprofile");
	printf("Error stopping profiling.\n");
  	return 1;
  } else printf("Statistical profiling stopped.\n");

  if ((npipe_fd = open(NPIPE, O_RDONLY)) < 0) {
	printf("Unable to open named pipe %s.\n", NPIPE);
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
	printf("Error getting data.\n");
	return 1;
  }

  mem_used = cprof_info.mem_used;

  if (mem_used == -1) {
	printf("ERROR: unable to get data due to insufficient memory.\n");
	printf("Try increasing available memory using the -m switch.\n");
  } else
  if (cprof_info.err) {
	printf("ERROR: the following error(s) happened during profiling:\n");
	if (cprof_info.err & CPROF_CPATH_OVERRUN)
		printf(" call path overrun\n");
	if (cprof_info.err & CPROF_STACK_OVERRUN)
		printf(" call stack overrun\n");
	if (cprof_info.err & CPROF_TABLE_OVERRUN)
		printf(" hash table overrun\n");
	printf("Try changing values in /usr/src/include/minix/profile.h ");
	printf("and then rebuild the system.\n");
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
	printf("Error resetting data.\n");
	return 1;
  }

  return 0;
}


int alloc_mem()
{
  if ((mem_ptr = malloc(mem_size)) == 0) {
	printf("Unable to allocate memory.\n");
	printf("Used chmem to increase available proces memory?\n");
	return 1;
  } else memset(mem_ptr, '\0', mem_size);

  return 0;
}


int init_outfile()
{
  if ((outfile_fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY)) <= 0) {
	printf("Unable to create outfile %s.\n", outfile);
	return 1;
  } else chmod(outfile, S_IRUSR | S_IWUSR);

  return 0;
}


int create_named_pipe()
{
  if ((mkfifo(NPIPE, S_IRUSR | S_IWUSR) == -1) && (errno != EEXIST)) {
	printf("Unable to create named pipe %s.\n", NPIPE);
	return 1;
  } else
	return 0;
}


int detach()
{
  setsid();
  (void) chdir("/");
  close(0);
  close(1);
  close(2);
}


int write_outfile()
{
  int n, towrite, written = 0;
  char *buf = mem_ptr;
  char header[80];

  printf("Writing to %s ...", outfile);

  /* Write header. */
  if (SPROF)
	sprintf(header, "stat\n%d %d %d %d\n",	sprof_info.total_samples,
						sprof_info.idle_samples,
					  	sprof_info.system_samples,
					  	sprof_info.user_samples);
  else
	sprintf(header, "call\n%u %u\n",
				CPROF_CPATH_MAX_LEN, CPROF_PROCNAME_LEN);

  n = write(outfile_fd, header, strlen(header));

  if (n < 0) { printf("Error writing to outfile %s.\n", outfile); return 1; }

  /* Write data. */
  towrite = mem_used == -1 ? mem_size : mem_used;

  while (towrite > 0) {

	n = write(outfile_fd, buf, towrite);

	if (n < 0)
		{ printf("Error writing to outfile %s.\n", outfile); return 1; }

	towrite -= n;
	buf += n;
	written += n;
  }

  printf(" header %d bytes, data %d bytes.\n", strlen(header), written);

  return 0;
}


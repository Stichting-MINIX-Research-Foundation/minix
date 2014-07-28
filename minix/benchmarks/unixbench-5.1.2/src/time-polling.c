/*  Programme to test how long it takes to select(2), poll(2) and poll2(2) a
    large number of file descriptors.

    Copyright 1997     Richard Gooch  rgooch@atnf.csiro.au
    Distributed under the GNU General Public License.

    To compile this programme, use  gcc -O2 -o time-polling time-polling.c

    Extra compile flags:

    Add  -DHAS_SELECT  if your operating system has the select(2) system call
    Add  -DHAS_POLL    if your operating system has the poll(2) system call
    Add  -DHAS_POLL2   if your operating system has the poll2(2) system call

    Usage:  time-polling [num_iter] [num_to_test] [num_active] [-v]

    NOTE: on many systems the default limit on file descriptors is less than
    1024. You should try to increase this limit to 1024 before doing the test.
    Something like "limit descriptors 1024" or "limit openfiles 1024" should do
    the trick. On some systems (like IRIX), doing the test on a smaller number
    gives a *much* smaller time per descriptor, which shows that time taken
    does not scale linearly with number of descriptors, which is non-optimal.
    In the tests I've done, I try to use 1024 descriptors.
    The benchmark results are available at:
    http://www.atnf.csiro.au/~rgooch/benchmarks.html
    If you want to contribute results, please email them to me. Please specify
    if you want to be acknowledged.


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Richard Gooch may be reached by email at  rgooch@atnf.csiro.au
    The postal address is:
      Richard Gooch, c/o ATNF, P. O. Box 76, Epping, N.S.W., 2121, Australia.

*/

#ifdef UNIXBENCH
	#define	OUT	stdout
#else
	#define OUT	stderr
#endif
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef HAS_POLL
#  include <sys/poll.h>
#endif
#ifdef HAS_POLL2
#  include <linux/poll2.h>
#endif
#ifdef HAS_SELECT
#  include <sys/select.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0
#ifdef UNIXBENCH
	#define MAX_ITERATIONS 1000
#else
	#define MAX_ITERATIONS 30
#endif
#define MAX_FDS 40960
#define CONST const
#define ERRSTRING strerror (errno)

typedef int flag;

    
#ifndef HAS_POLL
/*
static inline int find_first_set_bit (CONST void *array, int size)
*/
static int find_first_set_bit (CONST void *array, int size)
/*  [SUMMARY] Find the first bit set in a bitfield.
    <array> A pointer to the bitfield. This must be aligned on a long boundary.
    <size> The number of bits in the bitfield.
    [RETURNS] The index of the first set bit. If no bits are set, <<size>> + 1
    is returned.
*/
{
    int index;
    unsigned long word;
    unsigned int ul_size = 8 * sizeof (unsigned long);
    CONST unsigned long *ul_array = array;

    /*  Find first word with any bit set  */
    for (index = 0; (*ul_array == 0) && (index < size);
	 index += ul_size, ++ul_array);
    /*  Find first bit set in word  */
    for (word = *ul_array; !(word & 1) && (index < size);
	 ++index, word = word >> 1);
    return (index);
}   /*  End Function find_first_set_bit  */

/*
static inline int find_next_set_bit (CONST void *array, int size, int offset)
*/
static int find_next_set_bit (CONST void *array, int size, int offset)
/*  [SUMMARY] Find the next bit set in a bitfield.
    <array> A pointer to the bitfield. This must be aligned on a long boundary.
    <size> The number of bits in the bitfield.
    <offset> The offset of the current bit in the bitfield. The current bit is
    ignored.
    [RETURNS] The index of the next set bit. If no more bits are set,
    <<size>> + 1 is returned.
*/
{
    int index, tmp;
    unsigned long word;
    unsigned int ul_size = 8 * sizeof (unsigned long);
    CONST unsigned long *ul_array = array;

    if (++offset >= size) return (offset);
    index = offset;
    /*  Jump to the long word containing the next bit  */
    tmp = offset / ul_size;
    ul_array += tmp;
    offset -= tmp * ul_size;
    if ( (offset == 0) || (*ul_array == 0) )
	return (find_first_set_bit (ul_array, size - index) + index);
    /*  There is a bit set somewhere in this word  */
    if ( ( (word = *ul_array) != 0 ) && ( (word = word >> offset) != 0 ) )
    {
	/*  There is a bit set somewhere in this word at or after the offset
	    position  */
	for (; (word & 1) == 0; word = word >> 1, ++index);
	return (index);
    }
    /*  Have to go to subsequent word(s)  */
    index += ul_size - offset;
    return (find_first_set_bit (++ul_array, size - index) + index);
}   /*  End Function find_next_set_bit  */
#endif


struct callback_struct
{
    void (*input_func) (void *info);
    void (*output_func) (void *info);
    void (*exception_func) (void *info);
    void *info;
};

static int total_bits = 0;
struct callback_struct callbacks[MAX_FDS];


static void test_func (void *info)
{
    ++total_bits;
}

#ifdef HAS_SELECT
static void time_select (fd_set *input_fds, fd_set *output_fds,
			 fd_set *exception_fds, int max_fd, int num_iter,
			 long *times)
/*  [SUMMARY] Time how long it takes to select(2) file descriptors.
    <input_fds> The input masks.
    <output_fds> The output masks.
    <exception_fds> The exception masks.
    <max_fd> The highest file descriptor in the fd_sets.
    <num_iter> The number of iterations.
    <times> The time taken (in microseconds) for each iteration.
    [RETURNS] Nothing.
*/
{
    int fd, count, nready;
    fd_set i_fds, o_fds, e_fds;
    struct timeval time1, time2, tv;

    /*  Warm the cache a bit  */
    memcpy (&i_fds, input_fds, sizeof i_fds);
    memcpy (&o_fds, output_fds, sizeof i_fds);
    memcpy (&e_fds, exception_fds, sizeof i_fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    select (max_fd + 1, &i_fds, &o_fds, &e_fds, &tv);
    for (count = 0; count < num_iter; ++count)
    {
	total_bits = 0;
	gettimeofday (&time1, NULL);
	memcpy (&i_fds, input_fds, sizeof i_fds);
	memcpy (&o_fds, output_fds, sizeof i_fds);
	memcpy (&e_fds, exception_fds, sizeof i_fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	nready = select (max_fd + 1, &i_fds, &o_fds, &e_fds, &tv);
	if (nready == -1)
	{
	    fprintf (stderr, "Error selecting\t%s\n", ERRSTRING);
	    exit (2);
	}
	if (nready < 1)
	{
	    fprintf (stderr, "Error: nready: %d\n", nready);
	    exit (1);
	}
	/*  Scan the output  */
	for (fd = find_first_set_bit (&e_fds, sizeof e_fds * 8); fd <= max_fd;
	     fd = find_next_set_bit (&e_fds, sizeof e_fds * 8, fd) )
	{
	    (*callbacks[fd].exception_func) (callbacks[fd].info);
	}
	for (fd = find_first_set_bit (&i_fds, sizeof i_fds * 8); fd <= max_fd;
	     fd = find_next_set_bit (&i_fds, sizeof i_fds * 8, fd) )
	{
	    (*callbacks[fd].input_func) (callbacks[fd].info);
	}
	for (fd = find_first_set_bit (&o_fds, sizeof o_fds * 8); fd <= max_fd;
	     fd = find_next_set_bit (&o_fds, sizeof o_fds * 8, fd) )
	{
	    (*callbacks[fd].output_func) (callbacks[fd].info);
	}
	gettimeofday (&time2, NULL);
	times[count] = (time2.tv_sec - time1.tv_sec) * 1000000;
	times[count] += time2.tv_usec - time1.tv_usec;
    }
}   /*  End Function time_select  */
#endif  /* HAS_SELECT */

#ifdef HAS_POLL
static void time_poll (struct pollfd *pollfd_array, int start_index,
		       int num_to_test, int num_iter, long *times)
/*  [SUMMARY] Time how long it takes to poll(2) file descriptors.
    <pollfd_array> The array of pollfd structures.
    <start_index> The start index in the array of pollfd structures.
    <num_to_test> The number of file descriptors to test.
    <num_iter> The number of iterations.
    <times> The time taken (in microseconds) for each iteration.
    [RETURNS] Nothing.
*/
{
    short revents;
    int fd, count, nready;
    struct timeval time1, time2;
    struct pollfd *pollfd_ptr, *stop_pollfd;

    /*  Warm the cache a bit  */
    poll (pollfd_array + start_index, num_to_test, 0);
    for (count = 0; count < num_iter; ++count)
    {
	total_bits = 0;
	gettimeofday (&time1, NULL);
	nready = poll (pollfd_array + start_index, num_to_test, 0);
	if (nready == -1)
	{
	    fprintf (stderr, "Error polling\t%s\n", ERRSTRING);
	    exit (2);
	}
	if (nready < 1)
	{
	    fprintf (stderr, "Error: nready: %d\n", nready);
	    exit (1);
	}
	stop_pollfd = pollfd_array + start_index + num_to_test;
	for (pollfd_ptr = pollfd_array + start_index; TRUE; ++pollfd_ptr)
	{
	    if (pollfd_ptr->revents == 0) continue;
	    /*  Have an active descriptor  */
	    revents = pollfd_ptr->revents;
	    fd = pollfd_ptr->fd;
	    if (revents & POLLPRI)
		(*callbacks[fd].exception_func) (callbacks[fd].info);
	    if (revents & POLLIN)
		(*callbacks[fd].input_func) (callbacks[fd].info);
	    if (revents & POLLOUT)
		(*callbacks[fd].output_func) (callbacks[fd].info);
	    if (--nready == 0) break;
	}
	gettimeofday (&time2, NULL);
	times[count] = (time2.tv_sec - time1.tv_sec) * 1000000;
	times[count] += time2.tv_usec - time1.tv_usec;
    }
}   /*  End Function time_poll  */
#endif  /* HAS_POLL */

#ifdef HAS_POLL2
static void time_poll2 (struct poll2ifd *poll2ifd_array, int start_index,
			int num_to_test, int num_iter, long *times)
/*  [SUMMARY] Time how long it takes to poll2(2) file descriptors.
    <poll2ifd_array> The array of poll2ifd structures.
    <start_index> The start index in the array of pollfd structures.
    <num_to_test> The number of file descriptors to test.
    <num_iter> The number of iterations.
    <times> The time taken (in microseconds) for each iteration.
    [RETURNS] Nothing.
*/
{
    short revents;
    int fd, count, nready, i;
    struct timeval time1, time2;
    struct poll2ofd poll2ofd_array[MAX_FDS];

    /*  Warm the cache a bit  */
    poll2 (poll2ifd_array + start_index, poll2ofd_array, num_to_test, 0);
    for (count = 0; count < num_iter; ++count)
    {
	total_bits = 0;
	gettimeofday (&time1, NULL);
	nready = poll2 (poll2ifd_array + start_index, poll2ofd_array,
			num_to_test, 0);
	if (nready == -1)
	{
	    times[count] = -1;
	    if (errno == ENOSYS) return;  /*  Must do this first  */
	    fprintf (stderr, "Error calling poll2(2)\t%s\n", ERRSTRING);
	    exit (2);
	}
	if (nready < 1)
	{
	    fprintf (stderr, "Error: nready: %d\n", nready);
	    exit (1);
	}
	for (i = 0; i < nready; ++i)
	{
	    revents = poll2ofd_array[i].revents;
	    fd = poll2ofd_array[i].fd;
	    if (revents & POLLPRI)
		(*callbacks[fd].exception_func) (callbacks[fd].info);
	    if (revents & POLLIN)
		(*callbacks[fd].input_func) (callbacks[fd].info);
	    if (revents & POLLOUT)
		(*callbacks[fd].output_func) (callbacks[fd].info);
	}
	gettimeofday (&time2, NULL);
	times[count] = (time2.tv_sec - time1.tv_sec) * 1000000;
	times[count] += time2.tv_usec - time1.tv_usec;
    }
}   /*  End Function time_poll2  */
#endif  /* HAS_POLL2 */


int main (argc, argv)
int	argc;
char	*argv[];
{
    flag failed = FALSE;
    flag verbose = FALSE;
    int first_fd = -1;
    int fd, max_fd, count, total_fds;
    int num_to_test, num_active;
#ifdef UNIXBENCH
    int max_iter = 1000;
#else
    int max_iter = 10;
#endif
#ifdef HAS_SELECT
    long select_total = 0;
    fd_set input_fds, output_fds, exception_fds;
    long select_times[MAX_ITERATIONS];
#endif
#ifdef HAS_POLL
	int start_index;
    long poll_total = 0;
    struct pollfd pollfd_array[MAX_FDS];
    long poll_times[MAX_ITERATIONS];
#endif
#ifdef HAS_POLL2
    long poll2_total = 0;
    struct poll2ifd poll2ifd_array[MAX_FDS];
    struct poll2ofd poll2ofd_array[MAX_FDS];
    long poll2_times[MAX_ITERATIONS];
#endif
#if 0
    extern char *sys_errlist[];
#endif

#ifdef HAS_SELECT
    FD_ZERO (&input_fds);
    FD_ZERO (&output_fds);
    FD_ZERO (&exception_fds);
#endif
#ifdef HAS_POLL
    memset (pollfd_array, 0, sizeof pollfd_array);
#endif
    /*  Allocate file descriptors  */
    total_fds = 0;
    max_fd = 0;
    while (!failed)
    {
	if ( ( fd = dup (1) ) == -1 )
	{
	    if (errno != EMFILE)
	    {
		fprintf (stderr, "Error dup()ing\t%s\n", ERRSTRING);
		exit (1);
	    }
	    failed = TRUE;
	    continue;
	}
	if (fd >= MAX_FDS)
	{
	    fprintf (stderr, "File descriptor: %d larger than max: %d\n",
		     fd, MAX_FDS - 1);
	    exit (1);
	}
	callbacks[fd].input_func = test_func;
	callbacks[fd].output_func = test_func;
	callbacks[fd].exception_func = test_func;
	callbacks[fd].info = NULL;
	if (fd > max_fd) max_fd = fd;
	if (first_fd < 0) first_fd = fd;
#ifdef HAS_POLL
	pollfd_array[fd].fd = fd;
	pollfd_array[fd].events = 0;
#endif
#ifdef HAS_POLL2
	poll2ifd_array[fd].fd = fd;
	poll2ifd_array[fd].events = 0;
#endif
    }
    total_fds = max_fd + 1;
    /*  Process the command-line arguments  */
    if (argc > 5)
    {
	fputs ("Usage:\ttime-polling [num_iter] [num_to_test] [num_active] [-v]\n",
	       stderr);
	exit (1);
    }
    if (argc > 1) max_iter = atoi (argv[1]);
    if (max_iter > MAX_ITERATIONS)
    {
	fprintf (stderr, "num_iter too large\n");
	exit (1);
    }
    if (argc > 2) num_to_test = atoi (argv[2]);
    else num_to_test = total_fds - first_fd;
    if (argc > 3) num_active = atoi (argv[3]);
    else num_active = 1;
    if (argc > 4)
    {
	if (strcmp (argv[4], "-v") != 0)
	{
	    fputs ("Usage:\ttime-polling [num_iter] [num_to_test] [num_active] [-v]\n",
		   stderr);
	    exit (1);
	}
	verbose = TRUE;
    }

    /*  Sanity tests  */
    if (num_to_test > total_fds - first_fd) num_to_test = total_fds - first_fd;
    if (num_active > total_fds - first_fd) num_active = total_fds - first_fd;
    /*  Set activity monitoring flags  */
    for (fd = total_fds - num_to_test; fd < total_fds; ++fd)
    {
#ifdef HAS_SELECT
	FD_SET (fd, &exception_fds);
	FD_SET (fd, &input_fds);
#endif
#ifdef HAS_POLL
	pollfd_array[fd].events = POLLPRI | POLLIN;
#endif
#ifdef HAS_POLL2
	poll2ifd_array[fd].events = POLLPRI | POLLIN;
#endif
    }
    for (fd = total_fds - num_active; fd < total_fds; ++fd)
    {
#ifdef HAS_SELECT
	FD_SET (fd, &output_fds);
#endif
#ifdef HAS_POLL
	pollfd_array[fd].events |= POLLOUT;
#endif
#ifdef HAS_POLL2
	poll2ifd_array[fd].events |= POLLOUT;
#endif
    }
    fprintf (OUT, "Num fds: %d, polling descriptors %d-%d\n",
	     total_fds, total_fds - num_to_test, max_fd);
    /*  First do all the tests, then print the results  */
#ifdef HAS_SELECT
    time_select (&input_fds, &output_fds, &exception_fds, max_fd, max_iter,
		 select_times);
#endif
#ifdef HAS_POLL
    start_index = total_fds - num_to_test;
    time_poll (pollfd_array, start_index, num_to_test, max_iter, poll_times);
#endif
#ifdef HAS_POLL2
    start_index = total_fds - num_to_test;
    time_poll2 (poll2ifd_array, start_index, num_to_test, max_iter,
		poll2_times);
#endif
    /*  Now print out all the times  */
    fputs ("All times in microseconds\n", OUT);
    fputs ("ITERATION\t", OUT);
#ifdef HAS_SELECT
    fprintf (OUT, "%-12s", "select(2)");
#endif
#ifdef HAS_POLL
    fprintf (OUT, "%-12s", "poll(2)");
#endif
#ifdef HAS_POLL2
    if (poll2_times[0] >= 0) fprintf (OUT, "%-12s", "poll2(2)");
#endif
    for (count = 0; count < max_iter; ++count)
    {
	if (verbose) fprintf (OUT, "\n%d\t\t", count);
#ifdef HAS_SELECT
	if (verbose) fprintf (OUT, "%-12ld", select_times[count]);
	select_total += select_times[count];
#endif
#ifdef HAS_POLL
	if (verbose) fprintf (OUT, "%-12ld", poll_times[count]);
	poll_total += poll_times[count];
#endif
#ifdef HAS_POLL2
	if ( verbose && (poll2_times[0] >= 0) )
	    fprintf (OUT, "%-12ld", poll2_times[count]);
	poll2_total += poll2_times[count];
#endif
    }
    fputs ("\n\naverage\t\t", OUT);
#ifdef HAS_SELECT
    fprintf (OUT, "%-12ld", select_total / max_iter);
#endif
#ifdef HAS_POLL
    fprintf (OUT, "%-12ld", poll_total / max_iter);
#endif
#ifdef HAS_POLL2
    if (poll2_times[0] >= 0)
	fprintf (OUT, "%-12ld", poll2_total / max_iter);
#endif
    putc ('\n', OUT);
    fputs ("Per fd\t\t", OUT);
#ifdef HAS_SELECT
    fprintf (OUT, "%-12.2f",
	     (float) select_total / (float) max_iter / (float) num_to_test);
#ifdef UNIXBENCH
	fprintf (stderr, "lps\t%.2f\t%.1f\n",
		1000000 * (float) max_iter * (float) num_to_test
		 / (float) select_total, (float)select_total / 1000000);
#endif
#endif
#ifdef HAS_POLL
    fprintf (OUT, "%-12.2f",
	     (float) poll_total / (float) max_iter / (float) num_to_test);
#ifdef UNIXBENCH
	fprintf (stderr, "lps\t%.2f\t%.1f\n",
		1000000 * (float) max_iter * (float) num_to_test
		 / (float) poll_total, (float)poll_total / 1000000);
#endif
#endif
#ifdef HAS_POLL2
    if (poll2_times[0] >= 0) {
	fprintf (OUT, "%-12.2f",
		 (float) poll2_total / (float) max_iter / (float) num_to_test);
#ifdef UNIXBENCH
	fprintf (stderr, "lps\t%.2f\t%.1f\n",
		1000000 * (float) max_iter * (float) num_to_test
		 / (float) poll2_total, (float)poll2_total / 1000000);
#endif
    }

#endif
    fputs ("<- the most important value\n", OUT);

    exit(0);
}   /*  End Function main  */

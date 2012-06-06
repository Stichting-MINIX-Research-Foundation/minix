/* This header makes it possible to redefine system calls to the
 * file system. This way, minix servers can re-route the data
 * that libgcov tries to send to the file system. This is
 * necessary, because the servers can't access the file system
 * directly. Instead, they will copy the data to a helping user
 * space process, which will call the file system for them.
 * For more information, see the <minix/gcov.h> header file.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


/* These function pointers initially point to the standard system library
 * functions (fopen, etc). All calls to these system library functions are
 * then redefined to calls to these function pointers. Because the pointers
 * still point to the original functions, all functionality is unchanged.
 * Therefore, libgcov won't act differently when linked to applications.
 * But, when these pointers are redefined by code within the minix servers,
 * the file system calls get replaced by other functionality.
 */

#define fopen(...)  _gcov_fopen(__VA_ARGS__)
#define fread(...)  _gcov_fread(__VA_ARGS__)
#define fwrite(...) _gcov_fwrite(__VA_ARGS__)
#define fclose(...) _gcov_fclose(__VA_ARGS__)
#define fseek(...)  _gcov_fseek(__VA_ARGS__)
#define getenv(...) _gcov_getenv(__VA_ARGS__)


/* wrapper to make it possible to disable gcov_exit on a process exit (for mfs) */

int do_gcov_exit = 1;

void gcov_exit_wrapper(void){
	if(do_gcov_exit)
		gcov_exit();
}

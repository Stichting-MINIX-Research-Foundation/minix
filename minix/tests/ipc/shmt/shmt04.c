/*
 *
 *   Copyright (c) International Business Machines  Corp., 2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* 03/21/2003   enable ia64     Jacky.Malcles */
/* 12/20/2002   Port to LTP     robbiew@us.ibm.com */
/* 06/30/2001   Port to Linux   nsharoff@us.ibm.com */

/*
 * NAME
 *		 shmt04
 *
 * CALLS
 *		 shmctl(2) shmget(2) shmat(2)
 *
 * ALGORITHM
 * Parent process forks a child. Child pauses until parent has created
 * a shared memory segment, attached to it and written to it too. At that
 * time child gets the shared memory segment id, attaches to it and 
 * verifies that its contents are the same as the contents of the
 * parent attached segment.
 *
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

/** LTP Port **/
#include "test.h"
#include "usctest.h"


char *TCID="shmt04";            /* Test program identifier.    */
int TST_TOTAL=2;                /* Total number of test cases. */
extern int Tst_count;           /* Test Case counter for tst_* routines */
/**************/


key_t key;
sigset_t mysigset;

#define  ADDR1  (void *)0x40000000
#define  ADDR  (void *)0x80000
#define  SIZE  16*1024


int child(void);
int rm_shm(int);

int main(void)
{
 char *cp=NULL;
 int pid, pid1, shmid;
 int status;

 key = (key_t) getpid() ;

 signal(SIGUSR1, SIG_DFL);

 sigemptyset(&mysigset);
 sigaddset(&mysigset,SIGUSR1);
 sigprocmask(SIG_BLOCK,&mysigset,NULL);
 
 pid = fork();
 switch (pid) {
 case -1:
  tst_resm(TBROK,"fork failed");
  tst_exit() ;
 case 0:
  child();
 }

/*----------------------------------------------------------*/


if ((shmid = shmget(key, SIZE, IPC_CREAT|0666)) < 0 ) {
 perror("shmget");
 tst_resm(TFAIL,"Error: shmget: shmid = %d, errno = %d\n",
 shmid, errno) ;
 /*
  * kill the child if parent failed to do the attach
  */
 (void)kill(pid, SIGINT);
}
else {
#ifdef __ia64__
  cp = (char *) shmat(shmid, ADDR1, 0);
#elif defined(__ARM_ARCH_4T__)
  cp = (char *) shmat(shmid, NULL, 0);
#else
  cp = (char *) shmat(shmid, ADDR, 0);
#endif

 if (cp == (char *)-1) {
  perror("shmat");
  tst_resm(TFAIL,
           "Error: shmat: shmid = %d, errno = %d\n",
           shmid, errno) ;

/* kill the child if parent failed to do the attch */

 kill(pid, SIGINT) ;   

/* remove shared memory segment */

 rm_shm(shmid) ;  

 tst_exit() ;
} 
*cp     = 'A';
*(cp+1) = 'B';
*(cp+2) = 'C';

kill(pid, SIGUSR1);
while ( (pid1 = wait(&status)) < 0 && 
 (errno == EINTR) ) ;
 if (pid1 != pid) {
  tst_resm(TFAIL,"Waited on the wrong child") ;
  tst_resm(TFAIL,
           "Error: wait_status = %d, pid1= %d\n", status, pid1) ;
 }
}

tst_resm(TPASS,"shmget,shmat");

/*----------------------------------------------------------*/


if (shmdt(cp) < 0 ) {
 tst_resm(TFAIL,"shmdt");
}

tst_resm(TPASS,"shmdt");

/*----------------------------------------------------------*/

rm_shm(shmid) ;
tst_exit() ;

/*----------------------------------------------------------*/
return(0);
}

int child(void)
{
int  shmid, 
     chld_pid ;
char *cp;

sigemptyset(&mysigset);
sigsuspend(&mysigset);
chld_pid = getpid() ;
/*--------------------------------------------------------*/


if ((shmid = shmget(key, SIZE, 0)) < 0) {
 perror("shmget:child process");
 tst_resm(TFAIL,
          "Error: shmget: errno=%d, shmid=%d, child_pid=%d\n",
           errno, shmid, chld_pid);
}
else 
{
#ifdef __ia64__
  cp = (char *) shmat(shmid, ADDR1, 0);
#elif defined(__ARM_ARCH_4T__)
  cp = (char *) shmat(shmid, NULL, 0);
#else
  cp = (char *) shmat(shmid, ADDR, 0);
#endif
 if (cp == (char *)-1) {
  perror("shmat:child process");
  tst_resm(TFAIL,
           "Error: shmat: errno=%d, shmid=%d, child_pid=%d\n",
           errno, shmid, chld_pid);
} else {
  if (*cp != 'A') {
   tst_resm(TFAIL,"child: not A\n");
  }
  if (*(cp+1) != 'B') {
   tst_resm(TFAIL,"child: not B\n");
  }
  if (*(cp+2) != 'C') {
   tst_resm(TFAIL,"child: not C\n");
  }
  if (*(cp+8192) != 0) {
   tst_resm(TFAIL,"child: not 0\n");
  }
}

}
tst_exit() ;
return(0);
}

int rm_shm(shmid)
int shmid ;
{
        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
                perror("shmctl");
                tst_resm(TFAIL,
                "shmctl Failed to remove: shmid = %d, errno = %d\n",
                shmid, errno) ;
                tst_exit();
        }
        return(0);
}

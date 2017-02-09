/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*****************************************************************************
 * @(#) n8_OS_intf.h 1.39@(#)
 *****************************************************************************/
        
/*****************************************************************************/
/** @file n8_OS_intf.h
 *  @brief OS-dependent definitions for NSP2000 Interface
 *
 * Common header file for root definitions for NSP2000 project
 *
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 02/19/04 jpw   Added KERN_WARNING to defines for N8_PRINT
 * 07/28/03 brr   Only define N8_THREAD_INIT for user space when 
 *                SUPPORT_USER_CALLBACK_PTHREAD is defined. The allows users to
 *                enable kernel callbacks with requiring applications to link 
 *                with the pthread library.
 * 04/29/03 brr   Removed support for BSDi.
 * 03/24/03 brr   Added definition for n8_atomic_inc_and_test.
 * 03/10/03 brr   Added macros to support thread initialization.
 * 12/02/02 brr   Only define ioctl macros for user space.
 * 10/25/02 brr   Do not define N8_VirtToPhys for FreeBSD kernel.
 * 10/11/02 brr   Added support for FreeBSD.
 * 07/19/02 brr   Moved definitions of N8_VirtToPhys & N8_PhysToVirt here from
 *                n8_driver_api.h.
 * 07/02/02 brr   Added atomic operations.
 * 06/12/02 hml   Deleted BSD ATOMICLOCK ops as well as the user space
 *                N8_PhysToVirt macros. 
 * 06/11/02 hml   VX N8_AtomicLock* removed as promised.
 * 06/11/02 hml   Removed user space versions of N8_AtomicLock* interfaces.
 *                Vx will go next!
 * 06/10/02 hml   Added user space versions of the N8_AtomicLock* interfaces
 *                for Linux and BSD.  Also added n8_Lock_t for all OS's.
 * 06/06/02 brr   Moved n8_WaitQueue_t definition to helper.h.
 * 05/17/02 brr   Added n8_WaitQueue_t.
 * 04/11/02 brr   Define N8_YEILD for Linux kernel.
 * 03/14/02 brr   Removed IoctlCode, added N8_IOCTL
 * 02/25/02 brr   Added N8_PRINT.
 * 02/22/02 spm   Cleaned up debug macros so that we use the DBG macro only.
 *                Converted n8_udelay to n8_usleep.
 * 01/21/02 msz   Added N8_YIELD
 * 01/08/02 brr   Added n8_delay and other kernel specific interfaces for 2.1.
 * 11/28/01 bac   Removed #define for usleep for vxworks. Use n8_usleep
 *                instead. 
 * 10/24/01 dkm   Moved N8_DEF_MAX_NUM_PARAMS define to n8_pub_common.h
 * 10/15/01 brr   Removed misc warnings and removed semaphore macros.
 * 10/09/01 msz   Added n8_mkdir as VxWorks doesn't have mode_t (permission
 *                flags.)
 * 08/21/01 msz   Deleted COPY_IN, COPY_OUT as they are no longer used,
 *                code uses QMCopy directly.
 * 08/15/01 brr   Added VxWorks semaphores.
 * 08/13/01 msz   Don't define semun if we got _LINUX_SEM_H
 * 08/08/01 msz   Moved semun from n8_semaphore.h
 * 08/10/01 brr   Replaced IoctlCode for VxWorks.
 * 08/08/01 brr   Moved all OS kernel specific macros to helper.h.
 * 08/01/01 msz   Use _IOWR not IOWR for BSDi
 * 07/31/01 bac   Update COPY_IN and COPY_OUT to use QMCopy, renamed MIN/MAX to
 *                N8_MIN/N8_MAX.
 * 07/30/01 brr   Added definition for munmap - VxWorks Mod.
 * 07/26/01 mel   Deleted  redefinitions for MALLOC and FREE - we are not using them.
 * 07/27/01 brr   Added definision for printk & KERN_CRIT - VxWorks Mod.
 * 07/20/01 mel   Added definision for usleep - VxWorks Mod.
 ****************************************************************************/

/* DBG usage:
 * DBG(("format string...", arg1, arg2, ...));
 */
 
#ifndef N8_OS_INTF_H
#define N8_OS_INTF_H

#ifdef VX_BUILD
     #include "n8_pub_common.h"
     #include <vxWorks.h>
     #include <stdio.h>
     #include <string.h>
     #include <time.h>
     #include <sysLib.h>
     #include <taskLib.h>
     #include <usrLib.h>
     #include <netinet/in.h>
#endif

#define N8_MIN(x, y)               ((x) < (y) ? (x) : (y))
#define N8_MAX(x, y)               ((x) > (y) ? (x) : (y))


/*****************************************************************************
 * Linux specific macros
 ****************************************************************************/
#ifdef __linux

 #ifndef __KERNEL__
   #include <sched.h>         /* for sched_yield */
   #include <malloc.h>
   #include <string.h>
   #include <stdio.h>         /* for printf */
   #include <netinet/in.h>    /* for ntohs */
   #include <pthread.h>       /* for the pthread mutex operations */
   
   #ifdef N8DEBUG
        #define DBG(_args) { printf("DEBUG <%s:%d>: ", \
        __FILE__,__LINE__); printf _args; }
   #else
        #define DBG(_args)
   #endif
   #define N8_open(x,y,z) open(x,y)
   #define N8_mkdir(p,m) mkdir(p,m)
   #define N8_PRINT  printf
   #define KERN_CRIT
   #define N8_YIELD sched_yield()

   typedef int  n8_atomic_t;
   #define n8_atomic_set(var, value) (var = value)
   #define n8_atomic_read(var) (var)
   #define n8_atomic_add(var, value) (var += value)
   #define n8_atomic_sub(var, value) (var -= value)

   typedef pthread_t N8_Thread_t;

   #ifdef SUPPORT_USER_CALLBACK_PTHREAD
      #define N8_THREAD_INIT(entry, parm, thread) \
            pthread_create(&thread, NULL, (void *)entry, (void *)parm)
   #else
      #define N8_THREAD_INIT(entry, parm, thread) (thread = (N8_Thread_t)0)
   #endif
 #else
   #include <net/ip.h>
   #include <asm/atomic.h>

   #ifdef N8DEBUG
        #define DBG(_args) { printk("DEBUG: <%s:%i> ", \
        __FILE__, __LINE__); printk _args; }
   #else                                   
        #define DBG(_args)
   #endif
   #define N8_PRINT  printk
   #define N8_YIELD

   typedef atomic_t  n8_atomic_t;
   #define n8_atomic_set(var, value) atomic_set(&var, value)
   #define n8_atomic_read(var)       atomic_read(&var)
   #define n8_atomic_add(var, value) atomic_add(value, &var)
   #define n8_atomic_sub(var, value) atomic_sub(value, &var)

   typedef int N8_Thread_t;
   #define N8_THREAD_INIT(entry, parm,thread) \
              thread = kernel_thread((int (*)(void *))entry, (void *)parm, \
                                     CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
 #endif

   extern void *N8_PhysToVirt(unsigned long phys);
   extern unsigned long N8_VirtToPhys(void * virt);


   typedef char n8_Lock_t[24];

   #define N8_IOCTL ioctl
   #define WHERE " "

      /* semun defined for linux and n8_semaphore.h.  It is already defined  */
      /* in BSDi, so this is a operating system difference.                  */
      /* Furthermore semun does seem to be defined for the kernel, in        */
      /* /usr/src/linux/include/linux/sem.h, so it depends on what sem.h     */
      /* is included.                                                        */
      /***********************************************************************
      * NOTE:  Following bit of nastiness taken from the linux semctl man page 
      ***********************************************************************/
      #if ( defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED) ) \
          || defined(_LINUX_SEM_H) || defined(__KERNEL__)
      /* union semun is defined by including <sys/sem.h> */
      #else
      /* according to X/OPEN we have to define it ourselves */
      union semun {
         int val;                    /* value for SETVAL */
         struct semid_ds *buf;       /* buffer for IPC_STAT, IPC_SET */
         unsigned short int *array;  /* array for GETALL, SETALL */
         struct seminfo *__buf;      /* buffer for IPC_INFO */
      };
      #endif


/*****************************************************************************
 * VxWorks specific macros
 ****************************************************************************/
#elif VX_BUILD
   int vxProcParms(char *argv[N8_DEF_MAX_NUM_PARAMS], char *testStr);

   #define sleep(x) taskDelay(x * sysClkRateGet())
       
   #define N8_YIELD taskDelay(1)

   #define WHERE " "
   #define N8_mkdir(p,m) mkdir(p)

   #ifdef N8DEBUG
        #define DBG(_args) { printf("DEBUG <%s:%d>: ", \
        __FILE__,__LINE__); printf _args; }
   #else
        #define DBG(_args)
   #endif

   #define N8_PRINT  printf
   #define KERN_CRIT
   #define KERN_WARNING 

   typedef int  n8_atomic_t;
   #define n8_atomic_set(var, value) (var = value)
   #define n8_atomic_read(var) (var)
   #define n8_atomic_add(var, value) (var += value)
   #define n8_atomic_sub(var, value) (var -= value)

   typedef SEM_ID n8_Lock_t;

   #define N8_VirtToPhys(addr) (unsigned long)addr
   #define N8_PhysToVirt(addr) (void *)addr

   typedef int N8_Thread_t;
   #define N8_THREAD_INIT(entry, parm, thread) \
              thread = sp((FUNCPTR)entry, parm, 0,0,0,0,0,0,0,0)
/*****************************************************************************
 * FreeBSD specific macros
 ****************************************************************************/
#elif __FreeBSD__

     #include <sys/ioccom.h>     /* ioctl defines: _IOWR, IOC */
     #include <sys/param.h>
#ifndef KERNEL
     #include <stdio.h>         /* for printf */
     #include <pthread.h>
     #include <string.h>

#endif

     #ifdef N8DEBUG
          #define DBG(_args) { printf("DEBUG <%s:%d>: ", \
          __FILE__,__LINE__); printf _args; }
     #else
          #define DBG(_args)
     #endif
     #define N8_PRINT  printf
     #define KERN_CRIT
     #define KERN_WARNING 

     #define NSP2000_GROUP_NUM 0x31       /* a non-useful number */

     #define WHERE __WHERE__
     #define N8_open(x,y,z) open(x,y)
     #define N8_mkdir(p,m) mkdir(p,m)
     #define WakeUp(A) wakeup(A);

     #define N8_YIELD sched_yield()

     typedef char n8_Lock_t[24];
     typedef int  n8_atomic_t;
     #define n8_atomic_set(var, value) (var = value)
     #define n8_atomic_read(var) (var)
     #define n8_atomic_add(var, value) (var += value)
     #define n8_atomic_sub(var, value) (var -= value)
 
#ifdef KERNEL
     /* We didn't see memcpy in bsd. */
     #define memcpy(dst,src,len) bcopy((src),(dst),(len))
#else
     extern unsigned long N8_VirtToPhys(void * virt);

     typedef pthread_t N8_Thread_t;
     #define N8_THREAD_INIT(entry, parm, thread) \
           pthread_create(&thread, NULL, (void *)entry, (void *)parm)

     extern int n8_ioctl(int fd,int command, int parm);
     #define N8_IOCTL(fd,command,parm) n8_ioctl((fd),(command),(int)(parm))
#endif

/*****************************************************************************
 * NetBSD specific macros
 ****************************************************************************/
#elif __NetBSD__

     #include <sys/ioccom.h>     /* ioctl defines: _IOWR, IOC */
     #include <sys/param.h>
#ifndef KERNEL
     #include <stdio.h>         /* for printf */
     #include <pthread.h>
     #include <string.h>

#endif

     #ifdef N8DEBUG
          #define DBG(_args) { printf("DEBUG <%s:%d>: ", \
          __FILE__,__LINE__); printf _args; }
     #else
          #define DBG(_args)
     #endif
     #define N8_PRINT  printf
     #define KERN_CRIT
     #define KERN_WARNING 

     #define NSP2000_GROUP_NUM 0x31       /* a non-useful number */

     #define WHERE __WHERE__
     #define N8_open(x,y,z) open(x,y)
     #define N8_mkdir(p,m) mkdir(p,m)
     #define WakeUp(A) wakeup(A);

     #define N8_YIELD sched_yield()

     typedef char n8_Lock_t[24];
     typedef int  n8_atomic_t;
     #define n8_atomic_set(var, value) (var = value)
     #define n8_atomic_read(var) (var)
     #define n8_atomic_add(var, value) (var += value)
     #define n8_atomic_sub(var, value) (var -= value)
 
#ifdef KERNEL
#if 0
     /* We didn't see memcpy in bsd. */
     #define memcpy(dst,src,len) bcopy((src),(dst),(len))
#endif
#else
     extern unsigned long N8_VirtToPhys(void * virt);

     typedef pthread_t N8_Thread_t;
     #define N8_THREAD_INIT(entry, parm, thread) \
           pthread_create(&thread, NULL, (void *)entry, (void *)parm)

     extern int n8_ioctl(int fd,int command, int parm);
     #define N8_IOCTL(fd,command,parm) n8_ioctl((fd),(command),(int)(parm))
#endif

#define n8_delay_ms(ms)	DELAY((ms*1000))

#endif	/* __NetBSD__ */

#endif /* N8_OS_INTF_H */


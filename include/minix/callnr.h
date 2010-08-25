#define NCALLS		 113	/* number of system calls allowed */

#define EXIT		   1 
#define FORK		   2 
#define READ		   3 
#define WRITE		   4 
#define OPEN		   5 
#define CLOSE		   6 
#define WAIT		   7
#define CREAT		   8 
#define LINK		   9 
#define UNLINK		  10 
#define WAITPID		  11
#define CHDIR		  12 
#define TIME		  13
#define MKNOD		  14 
#define CHMOD		  15 
#define CHOWN		  16 
#define BRK		  17
#define STAT		  18 
#define LSEEK		  19
#define MINIX_GETPID	  20
#define MOUNT		  21 
#define UMOUNT		  22 
#define SETUID		  23
#define GETUID		  24
#define STIME		  25
#define PTRACE		  26
#define ALARM		  27
#define FSTAT		  28 
#define PAUSE		  29
#define UTIME		  30 
#define ACCESS		  33 
#define SYNC		  36 
#define KILL		  37
#define RENAME		  38
#define MKDIR		  39
#define RMDIR		  40
#define DUP		  41 
#define PIPE		  42 
#define TIMES		  43
#define SYMLINK		  45
#define SETGID		  46
#define GETGID		  47
#define SIGNAL		  48
#define RDLNK		  49
#define LSTAT		  50
#define IOCTL		  54
#define FCNTL		  55
#define FS_READY	  57
#define EXEC		  59
#define UMASK		  60 
#define CHROOT		  61 
#define SETSID		  62
#define GETPGRP		  63
#define ITIMER		  64
#define GETGROUPS	  65
#define SETGROUPS	  66
#define GETMCONTEXT       67
#define SETMCONTEXT       68

/* Posix signal handling. */
#define SIGACTION	  71
#define SIGSUSPEND	  72
#define SIGPENDING	  73
#define SIGPROCMASK	  74
#define SIGRETURN	  75

#define REBOOT		  76
#define SVRCTL		  77
#define SYSUNAME	  78
#define GETSYSINFO	  79	/* to PM, VFS, RS, or DS */
#define GETDENTS	  80	/* to FS */
#define LLSEEK		  81	/* to VFS */
#define FSTATFS	 	  82	/* to VFS */
#define STATVFS 	  83	/* to VFS */
#define FSTATVFS 	  84	/* to VFS */
#define SELECT            85	/* to VFS */
#define FCHDIR            86	/* to VFS */
#define FSYNC             87	/* to VFS */
#define GETPRIORITY       88	/* to PM */
#define SETPRIORITY       89	/* to PM */
#define GETTIMEOFDAY      90	/* to PM */
#define SETEUID		  91	/* to PM */
#define SETEGID		  92	/* to PM */
#define TRUNCATE	  93	/* to VFS */
#define FTRUNCATE	  94	/* to VFS */
#define FCHMOD		  95	/* to VFS */
#define FCHOWN		  96	/* to VFS */
#define GETSYSINFO_UP	  97	/* to PM or VFS */
#define SPROF             98    /* to PM */
#define CPROF             99    /* to PM */

/* Calls provided by PM and FS that are not part of the API */
#define EXEC_NEWMEM	100	/* from VFS or RS to PM: new memory map for
				 * exec
				 */
#define SRV_FORK  	101	/* to PM: special fork call for RS */
#define EXEC_RESTART	102	/* to PM: final part of exec for RS */
#define PROCSTAT	103	/* to PM */
#define GETPROCNR	104	/* to PM */

#define GETEPINFO	107	/* to PM: get pid/uid/gid of an endpoint */
#define ADDDMA		108	/* to PM: inform PM about a region of memory
				 * that is used for bus-master DMA
				 */
#define DELDMA		109	/* to PM: inform PM that a region of memory
				 * that is no longer used for bus-master DMA
				 */
#define GETDMA		110	/* to PM: ask PM for a region of memory
				 * that should not be used for bus-master DMA
				 * any longer
				 */
#define SRV_KILL  	111	/* to PM: special kill call for RS */

#define GCOV_FLUSH	112	/* flush gcov data from server to gcov files */

#define TASK_REPLY	121	/* to VFS: reply code from drivers, not 
				 * really a standalone call.
				 */
#define MAPDRIVER      122     /* to VFS, map a device */

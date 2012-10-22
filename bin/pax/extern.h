/*	$NetBSD: extern.h,v 1.59 2012/08/09 08:09:21 christos Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)extern.h	8.2 (Berkeley) 4/18/94
 */

/*
 * External references from each source file
 */

#include <sys/cdefs.h>
#include <err.h>

/*
 * ar_io.c
 */
extern const char *arcname;
extern int curdirfd;
extern const char *gzip_program;
extern time_t starttime;
extern int force_one_volume;
extern char *chdname;
extern int forcelocal;
extern int secure;

int ar_open(const char *);
void ar_close(void);
void ar_drain(void);
int ar_set_wr(void);
int ar_app_ok(void);
#ifdef SYS_NO_RESTART
int read_with_restart(int, void *, int);
int write_with_restart(int, void *, int);
#else
#define read_with_restart	read
#define write_with_restart	write
#endif
int xread(int, void *, int);
int xwrite(int, void *, int);
int ar_read(char *, int);
int ar_write(char *, int);
int ar_rdsync(void);
int ar_fow(off_t, off_t *);
int ar_rev(off_t );
int ar_next(void);
void ar_summary(int);
int ar_dochdir(const char *);

/*
 * ar_subs.c
 */
extern u_long flcnt;
extern ARCHD archd;
int updatepath(void);
int dochdir(const char *);
int fdochdir(int);
int domkdir(const char *, mode_t);
int list(void);
int extract(void);
int append(void);
int archive(void);
int copy(void);

/*
 * buf_subs.c
 */
extern int blksz;
extern int wrblksz;
extern int maxflt;
extern int rdblksz;
extern off_t wrlimit;
extern off_t rdcnt;
extern off_t wrcnt;
int wr_start(void);
int rd_start(void);
void cp_start(void);
int appnd_start(off_t);
int rd_sync(void);
void pback(char *, int);
int rd_skip(off_t);
void wr_fin(void);
int wr_rdbuf(char *, int);
int rd_wrbuf(char *, int);
int wr_skip(off_t);
int wr_rdfile(ARCHD *, int, off_t *);
int rd_wrfile(ARCHD *, int, off_t *);
void cp_file(ARCHD *, int, int);
int buf_fill(void);
int buf_flush(int);

/*
 * cpio.c
 */
extern int cpio_swp_head;
int cpio_strd(void);
int cpio_subtrail(ARCHD *);
int cpio_endwr(void);
int cpio_id(char *, int);
int cpio_rd(ARCHD *, char *);
off_t cpio_endrd(void);
int cpio_stwr(void);
int cpio_wr(ARCHD *);
int vcpio_id(char *, int);
int crc_id(char *, int);
int crc_strd(void);
int vcpio_rd(ARCHD *, char *);
off_t vcpio_endrd(void);
int crc_stwr(void);
int vcpio_wr(ARCHD *);
int bcpio_id(char *, int);
int bcpio_rd(ARCHD *, char *);
off_t bcpio_endrd(void);
int bcpio_wr(ARCHD *);

/*
 * file_subs.c
 */
extern char *gnu_name_string, *gnu_link_string;
extern size_t gnu_name_length, gnu_link_length;
extern char *xtmp_name;
int file_creat(ARCHD *, int);
void file_close(ARCHD *, int);
int lnk_creat(ARCHD *, int *);
int cross_lnk(ARCHD *);
int chk_same(ARCHD *);
int node_creat(ARCHD *);
int unlnk_exist(char *, int);
int chk_path(char *, uid_t, gid_t);
void set_ftime(char *fnm, time_t mtime, time_t atime, int frc, int slk);
int set_ids(char *, uid_t, gid_t);
void set_pmode(char *, mode_t);
void set_chflags(char *fnm, u_int32_t flags);
int file_write(int, char *, int, int *, int *, int, char *);
void file_flush(int, char *, int);
void rdfile_close(ARCHD *, int *);
int set_crc(ARCHD *, int);

/*
 * ftree.c
 */
int ftree_start(void);
int ftree_add(char *, int);
void ftree_sel(ARCHD *);
void ftree_chk(void);
int next_file(ARCHD *);

/*
 * gen_subs.c
 */
void ls_list(ARCHD *, time_t, FILE *);
void ls_tty(ARCHD *);
void safe_print(const char *, FILE *);
uint32_t asc_u32(char *, int, int);
int u32_asc(uintmax_t, char *, int, int);
uintmax_t asc_umax(char *, int, int);
int umax_asc(uintmax_t, char *, int, int);
int check_Aflag(void);

/*
 * getoldopt.c
 */
struct option;
int getoldopt(int, char **, const char *, struct option *, int *);

/*
 * options.c
 */
extern FSUB fsub[];
extern int ford[];
extern int sep;
extern int havechd;
void options(int, char **);
OPLIST * opt_next(void);
int bad_opt(void);
int mkpath(char *);
char *chdname;
#if !HAVE_NBTOOL_CONFIG_H
int do_chroot;
#endif

/*
 * pat_rep.c
 */
int rep_add(char *);
int pat_add(char *, char *, int);
void pat_chk(void);
int pat_sel(ARCHD *);
int pat_match(ARCHD *);
int mod_name(ARCHD *, int);
int set_dest(ARCHD *, char *, int);

/*
 * pax.c
 */
extern int act;
extern FSUB *frmt;
extern int Aflag;
extern int cflag;
extern int cwdfd;
extern int dflag;
extern int iflag;
extern int kflag;
extern int lflag;
extern int nflag;
extern int tflag;
extern int uflag;
extern int vflag;
extern int Dflag;
extern int Hflag;
extern int Lflag;
extern int Mflag;
extern int Vflag;
extern int Xflag;
extern int Yflag;
extern int Zflag;
extern int vfpart;
extern int patime;
extern int pmtime;
extern int nodirs;
extern int pfflags;
extern int pmode;
extern int pids;
extern int rmleadslash;
extern int exit_val;
extern int docrc;
extern int to_stdout;
extern char *dirptr;
extern char *ltmfrmt;
extern const char *argv0;
extern FILE *listf;
extern char *tempfile;
extern char *tempbase;

/*
 * sel_subs.c
 */
int sel_chk(ARCHD *);
int grp_add(char *);
int usr_add(char *);
int trng_add(char *);

/*
 * tables.c
 */
int lnk_start(void);
int chk_lnk(ARCHD *);
void purg_lnk(ARCHD *);
void lnk_end(void);
int ftime_start(void);
int chk_ftime(ARCHD *);
int name_start(void);
int add_name(char *, int, char *);
void sub_name(char *, int *, size_t);
int dev_start(void);
int add_dev(ARCHD *);
int map_dev(ARCHD *, u_long, u_long);
int atdir_start(void);
void atdir_end(void);
void add_atdir(char *, dev_t, ino_t, time_t, time_t);
int get_atdir(dev_t, ino_t, time_t *, time_t *);
int dir_start(void);
void add_dir(char *, int, struct stat *, int);
void proc_dir(void);
u_int st_hash(char *, int, int);

/*
 * tar.c
 */
extern int is_gnutar;
int tar_endwr(void);
off_t tar_endrd(void);
int tar_trail(char *, int, int *);
int tar_id(char *, int);
int tar_opt(void);
int tar_rd(ARCHD *, char *);
int tar_wr(ARCHD *);
int ustar_strd(void);
int ustar_stwr(void);
int ustar_id(char *, int);
int ustar_rd(ARCHD *, char *);
int ustar_wr(ARCHD *);
int tar_gnutar_X_compat(const char *);
int tar_gnutar_minus_minus_exclude(const char *);

/*
 * tty_subs.c
 */
int tty_init(void);
void tty_prnt(const char *, ...)
    __attribute__((format (printf, 1, 2)));
int tty_read(char *, int);
void tty_warn(int, const char *, ...)
    __attribute__((format (printf, 2, 3)));
void syswarn(int, int, const char *, ...)
    __attribute__((format (printf, 3, 4)));

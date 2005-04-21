#ifndef _ANSI
#include <ansi.h>
#endif

/* sbbcpy.c */
_PROTOTYPE( int bcopy, (SBMA from, SBMA to, unsigned cnt) );
_PROTOTYPE( int sbm_wcpy, (int *from, int *to, unsigned cnt) );

/* sberr.c */
_PROTOTYPE( int sbe_mem, (void) );
_PROTOTYPE( char *sbe_mvfy, (void) );
_PROTOTYPE( char *sbe_mfl, (int p) );
_PROTOTYPE( char *sbe_mlst, (int p) );
_PROTOTYPE( int sbe_smp, (struct smblk *sm, int type) );
_PROTOTYPE( char *sbe_sfl, (int p) );
_PROTOTYPE( int sbe_sds, (void) );
_PROTOTYPE( int sbe_psd, (struct sdblk *sd) );
_PROTOTYPE( char *sbe_svfy, (void) );
_PROTOTYPE( char *sbe_sdlist, (int p, int phys) );
_PROTOTYPE( int sbe_dsk, (SBFILE *sfp) );
_PROTOTYPE( char *sbe_sbvfy, (SBBUF *sbp) );
_PROTOTYPE( char *sbe_sbs, (SBBUF *sbp, int p) );

/* sbm.c */
_PROTOTYPE( struct smblk *sbm_init, (SBMA xaddr, SBMO xlen) );
_PROTOTYPE( struct smblk *sbm_nget, (void) );
_PROTOTYPE( int sbm_nfre, (struct smblk *smp) );
_PROTOTYPE( struct smblk *sbm_nmak, (SBMO elsize, unsigned flag) );
_PROTOTYPE( struct smblk *sbm_lmak, (SBMA addr, SBMO elsize, int num) );
_PROTOTYPE( int sbm_nmov, (struct smblk *smp1, struct smblk *smp2, struct smblk **begp, int elsize) );
_PROTOTYPE( struct smblk *sbm_mget, (SBMO cmin, SBMO cmax) );
_PROTOTYPE( char *sbm_brk, (unsigned size) );
_PROTOTYPE( int sbm_mfree, (struct smblk *sm) );
_PROTOTYPE( struct smblk *sbm_exp, (struct smblk *sm, SBMO size) );
_PROTOTYPE( int sbm_mmrg, (struct smblk *smp) );
_PROTOTYPE( struct smblk *sbm_split, (struct smblk *smp, SBMO coff) );
_PROTOTYPE( int sbm_scpy, (char *from, char *to, unsigned count) );
#if 0
_PROTOTYPE( struct smblk *sbm_err, (struct smblk *val, char *str, int a0, int a1, int a2, int a3) );
#else
struct smblk *sbm_err();
#endif
_PROTOTYPE( char *malloc, (unsigned size) );
_PROTOTYPE( char *alloc, (unsigned size) );
_PROTOTYPE( int free, (char *ptr) );
_PROTOTYPE( char *realloc, (char *ptr, unsigned size) );
_PROTOTYPE( char *calloc, (unsigned nelem, unsigned elsize) );
_PROTOTYPE( int sbm_ngc, (void) );
_PROTOTYPE( int sbm_xngc, (struct smblk **begp, unsigned elsize, unsigned flag) );
_PROTOTYPE( int sbm_nfor, (int flag, int nodsiz, int (*proc )(), struct sbfile *arg) );

/* sbstr.c */
_PROTOTYPE( SBSTR *sb_close, (SBBUF *sbp) );
_PROTOTYPE( int sb_setovw, (SBBUF *sbp) );
_PROTOTYPE( int sb_clrovw, (SBBUF *sbp) );
_PROTOTYPE( chroff sbx_fdlen, (int fd) );
_PROTOTYPE( SBSTR *sb_fduse, (int ifd) );
_PROTOTYPE( int sb_fdcls, (int ifd) );
_PROTOTYPE( int sbx_fcls, (struct sbfile *sfp) );
_PROTOTYPE( int sb_fdinp, (SBBUF *sb, int fd) );
_PROTOTYPE( int sb_fsave, (SBBUF *sb, int fd) );
_PROTOTYPE( int sb_sgetc, (SBBUF *sb) );
_PROTOTYPE( int sb_sputc, (SBBUF *sb, int ch) );
_PROTOTYPE( int sb_speekc, (SBBUF *sb) );
_PROTOTYPE( int sb_rgetc, (SBBUF *sb) );
_PROTOTYPE( int sb_rdelc, (SBBUF *sbp) );
_PROTOTYPE( int sb_deln, (SBBUF *sbp, chroff num) );
_PROTOTYPE( struct sdblk *sb_killn, (SBBUF *sbp, chroff num) );
_PROTOTYPE( SBSTR *sb_cpyn, (SBBUF *sbp, chroff num) );
_PROTOTYPE( int sb_sins, (SBBUF *sbp, struct sdblk *sdp) );
_PROTOTYPE( SBSTR *sbs_cpy, (SBSTR *sdp) );
_PROTOTYPE( int sbs_del, (SBSTR *sdp) );
_PROTOTYPE( SBSTR *sbs_app, (struct sdblk *sdp, struct sdblk *sdp2) );
_PROTOTYPE( chroff sbs_len, (SBSTR *sdp) );
_PROTOTYPE( int sb_seek, (SBBUF *sbp, chroff coff, int flg) );
_PROTOTYPE( int sb_rewind, (SBBUF *sbp) );
_PROTOTYPE( chroff sb_tell, (SBBUF *sbp) );
_PROTOTYPE( chroff sb_ztell, (SBBUF *sbp) );
#if 0
_PROTOTYPE( struct sdblk *sbx_ready, (SBBUF *sbp, int type, SBMO cmin, SBMO cmax) );
#else
struct sdblk *sbx_ready();
#endif
_PROTOTYPE( struct sdblk *sbx_next, (SBBUF *sbp) );
_PROTOTYPE( struct sdblk *sbx_norm, (SBBUF *sbp, int mode) );
_PROTOTYPE( struct sdblk *sbx_beg, (struct sdblk *sdp) );
_PROTOTYPE( int sbx_smdisc, (SBBUF *sbp) );
_PROTOTYPE( int sbx_sbrdy, (SBBUF *sbp) );
_PROTOTYPE( struct sdblk *sbx_scpy, (struct sdblk *sdp, struct sdblk *sdlast) );
_PROTOTYPE( struct sdblk *sbx_sdcpy, (struct sdblk *sdp) );
_PROTOTYPE( struct sdblk *sbx_xcis, (SBBUF *sbp, chroff num, struct sdblk **asd2, chroff *adot) );
_PROTOTYPE( struct sdblk *sbx_split, (struct sdblk *sdp, chroff coff) );
_PROTOTYPE( struct smblk *sbx_msplit, (struct smblk *smp, SBMO size) );
_PROTOTYPE( struct sdblk *sbx_ndel, (struct sdblk *sdp) );
_PROTOTYPE( int sbx_npdel, (struct sdblk *sdp) );
_PROTOTYPE( struct sdblk *sbx_ndget, (void) );
_PROTOTYPE( int sbx_ndfre, (struct sdblk *sdp) );
_PROTOTYPE( SBMA sbx_malloc, (unsigned size) );
_PROTOTYPE( struct smblk *sbx_mget, (SBMO cmin, SBMO cmax) );
_PROTOTYPE( int sbx_comp, (int cmin, int lev) );
_PROTOTYPE( int sbx_sdgc, (struct sdblk *sdp, int lev) );
#if 0
_PROTOTYPE( int sbx_aout, (struct sdblk *sdp, int flag, int fd) );
#else
int sbx_aout();
#endif
_PROTOTYPE( chroff sbx_qlen, (struct sdblk *sdp) );
_PROTOTYPE( int sbx_tset, (chroff loff, int align) );
_PROTOTYPE( struct sdblk *sbx_ffnd, (SBFILE *sfp, chroff size, chroff *aloc) );
_PROTOTYPE( int sbx_rdf, (int fd, char *addr, int cnt, int skflg, chroff loc) );
_PROTOTYPE( int sbx_rugpull, (int fd) );
_PROTOTYPE( int sbx_unpur, (struct sdblk *sd, struct sbfile *sf) );
#if 0
_PROTOTYPE( int sbx_err, (int val, char *str, int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12) );
#else
int sbx_err();
#endif

/* sbvall.c */
_PROTOTYPE( char *valloc, (unsigned size) );

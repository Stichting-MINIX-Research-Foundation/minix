/* $Header$
 *
 * $Log$
 * Revision 1.1  2005/04/21 14:55:11  beng
 * Initial revision
 *
 * Revision 1.1.1.1  2005/04/20 13:33:19  beng
 * Initial import of minix 2.0.4
 *
 * Revision 2.0.1.1  87/01/30  22:47:16  lwall
 * Added do_ed_script().
 * 
 * Revision 2.0  86/09/17  15:39:57  lwall
 * Baseline for netwide release.
 * 
 */

EXT FILE *pfp INIT(Nullfp);		/* patch file pointer */
#ifdef SMALL
EXT FILE *sfp INIT(Nullfp);		/* string file pointer */
#endif

_PROTOTYPE(void re_patch , (void));
_PROTOTYPE(void open_patch_file , (char *filename ));
_PROTOTYPE(void set_hunkmax , (void));
_PROTOTYPE(void grow_hunkmax , (void));
_PROTOTYPE(bool there_is_another_patch , (void));
_PROTOTYPE(int intuit_diff_type , (void));
_PROTOTYPE(void next_intuit_at , (long file_pos , long file_line ));
_PROTOTYPE(void skip_to , (long file_pos , long file_line ));
_PROTOTYPE(bool another_hunk , (void));
_PROTOTYPE(char *pgets , (char *bf , int sz , FILE *fp ));
_PROTOTYPE(bool pch_swap , (void));
_PROTOTYPE(LINENUM pch_first , (void));
_PROTOTYPE(LINENUM pch_ptrn_lines , (void));
_PROTOTYPE(LINENUM pch_newfirst , (void));
_PROTOTYPE(LINENUM pch_repl_lines , (void));
_PROTOTYPE(LINENUM pch_end , (void));
_PROTOTYPE(LINENUM pch_context , (void));
_PROTOTYPE(short pch_line_len , (LINENUM line ));
_PROTOTYPE(char pch_char , (LINENUM line ));
_PROTOTYPE(char *pfetch , (LINENUM line ));
_PROTOTYPE(LINENUM pch_hunk_beg , (void));
_PROTOTYPE(void do_ed_script , (void));
#ifdef SMALL
_PROTOTYPE(long saveStr , (char *string , short *length ));
_PROTOTYPE(void strEdit , (long pos , int from , int to ));
#endif

/* $Header$
 *
 * $Log$
 * Revision 1.1  2005/04/21 14:55:10  beng
 * Initial revision
 *
 * Revision 1.1.1.1  2005/04/20 13:33:18  beng
 * Initial import of minix 2.0.4
 *
 * Revision 2.0  86/09/17  15:37:25  lwall
 * Baseline for netwide release.
 * 
 */

EXT LINENUM input_lines INIT(0);	/* how long is input file in lines */
EXT LINENUM last_frozen_line INIT(0);	/* how many input lines have been */
					/* irretractibly output */

_PROTOTYPE(bool rev_in_string , (char *string ));
_PROTOTYPE(void scan_input , (char *filename ));
_PROTOTYPE(bool plan_a , (char *filename )); 
_PROTOTYPE(void plan_b , (char *filename ));
_PROTOTYPE(char *ifetch , (Reg1 LINENUM line , int whichbuf ));
_PROTOTYPE(void re_input , (void));

/* $NetBSD: term.h,v 1.16 2013/06/07 13:16:18 roy Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011, 2013 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TERM_H_
#define	_TERM_H_

#ifndef ERR
#define	ERR	(-1)	/* Error return */
#define	OK	(0)	/* Success return */
#endif

/* Define available terminfo flags */
enum TIFLAGS {
	TICODE_bw,
	TICODE_am,
	TICODE_bce,
	TICODE_ccc,
	TICODE_xhp,
	TICODE_xhpa,
	TICODE_cpix,
	TICODE_crxm,
	TICODE_xt,
	TICODE_xenl,
	TICODE_eo,
	TICODE_gn,
	TICODE_hc,
	TICODE_chts,
	TICODE_km,
	TICODE_daisy,
	TICODE_hs,
	TICODE_hls,
	TICODE_in,
	TICODE_lpix,
	TICODE_da,
	TICODE_db,
	TICODE_mir,
	TICODE_msgr,
	TICODE_nxon,
	TICODE_xsb,
	TICODE_npc,
	TICODE_ndscr,
	TICODE_nrrmc,
	TICODE_os,
	TICODE_mc5i,
	TICODE_xvpa,
	TICODE_sam,
	TICODE_eslok,
	TICODE_hz,
	TICODE_ul,
	TICODE_xon
};
#define TIFLAGMAX	TICODE_xon

#define t_auto_left_margin(t)		(t)->flags[TICODE_bw]
#define t_auto_right_margin(t)		(t)->flags[TICODE_am]
#define t_back_color_erase(t)		(t)->flags[TICODE_bce]
#define t_can_change(t)			(t)->flags[TICODE_ccc]
#define t_ceol_standout_glitch(t)	(t)->flags[TICODE_xhp]
#define t_col_addr_glitch(t)		(t)->flags[TICODE_xhpa]
#define t_cpi_changes_res(t)		(t)->flags[TICODE_cpix]
#define t_cr_cancels_micro_mode(t)	(t)->flags[TICODE_crxm]
#define t_dest_tabs_magic_smso(t)	(t)->flags[TICODE_xt]
#define t_eat_newline_glitch(t)		(t)->flags[TICODE_xenl]
#define t_erase_overstrike(t)		(t)->flags[TICODE_eo]
#define t_generic_type(t)		(t)->flags[TICODE_gn]
#define t_hard_copy(t)			(t)->flags[TICODE_hc]
#define t_hard_cursor(t)		(t)->flags[TICODE_chts]
#define t_has_meta_key(t)		(t)->flags[TICODE_km]
#define t_has_print_wheel(t)		(t)->flags[TICODE_daisy]
#define t_has_status_line(t)		(t)->flags[TICODE_hs]
#define t_hue_light_saturation(t)	(t)->flags[TICODE_hls]
#define t_insert_null_glitch(t)		(t)->flags[TICODE_in]
#define t_lpi_changes_yes(t)		(t)->flags[TICODE_lpix]
#define t_memory_above(t)		(t)->flags[TICODE_da]
#define t_memory_below(t)		(t)->flags[TICODE_db]
#define t_move_insert_mode(t)		(t)->flags[TICODE_mir]
#define t_move_standout_mode(t)		(t)->flags[TICODE_msgr]
#define t_needs_xon_xoff(t)		(t)->flags[TICODE_nxon]
#define t_no_esc_ctlc(t)		(t)->flags[TICODE_xsb]
#define t_no_pad_char(t)		(t)->flags[TICODE_npc]
#define t_non_dest_scroll_region(t)	(t)->flags[TICODE_ndscr]
#define t_non_rev_rmcup(t)		(t)->flags[TICODE_nrrmc]
#define t_over_strike(t)		(t)->flags[TICODE_os]
#define t_prtr_silent(t)		(t)->flags[TICODE_mc5i]
#define t_row_addr_glitch(t)		(t)->flags[TICODE_xvpa]
#define t_semi_auto_right_margin(t)	(t)->flags[TICODE_sam]
#define t_status_line_esc_ok(t)		(t)->flags[TICODE_eslok]
#define t_tilde_glitch(t)		(t)->flags[TICODE_hz]
#define t_transparent_underline(t)	(t)->flags[TICODE_ul]
#define t_xon_xoff(t)			(t)->flags[TICODE_xon]

#define auto_left_margin		t_auto_left_margin(cur_term)
#define auto_right_margin		t_auto_right_margin(cur_term)
#define back_color_erase		t_back_color_erase(cur_term)
#define can_change			t_can_change(cur_term)
#define ceol_standout_glitch		t_ceol_standout_glitch(cur_term)
#define col_addr_glitch			t_col_addr_glitch(cur_term)
#define cpi_changes_res			t_cpi_changes_res(cur_term)
#define cr_cancels_micro_mode		t_cr_cancels_micro_mode(cur_term)
#define dest_tabs_magic_smso		t_dest_tabs_magic_smso(cur_term)
#define eat_newline_glitch		t_eat_newline_glitch(cur_term)
#define erase_overstrike		t_erase_overstrike(cur_term)
#define generic_type			t_generic_type(cur_term)
#define hard_copy			t_hard_copy(cur_term)
#define hard_cursor			t_hard_cursor(cur_term)
#define has_meta_key			t_has_meta_key(cur_term)
#define has_print_wheel			t_has_print_wheel(cur_term)
#define has_status_line			t_has_status_line(cur_term)
#define hue_light_saturation		t_hue_light_saturation(cur_term)
#define insert_null_glitch		t_insert_null_glitch(cur_term)
#define lpi_changes_yes			t_lpi_changes_yes(cur_term)
#define memory_above			t_memory_above(cur_term)
#define memory_below			t_memory_below(cur_term)
#define move_insert_mode		t_move_insert_mode(cur_term)
#define move_standout_mode		t_move_standout_mode(cur_term)
#define needs_xon_xoff			t_needs_xon_xoff(cur_term)
#define no_esc_ctlc			t_no_esc_ctlc(cur_term)
#define no_pad_char			t_no_pad_char(cur_term)
#define non_dest_scroll_region		t_non_dest_scroll_region(cur_term)
#define non_rev_rmcup			t_non_rev_rmcup(cur_term)
#define over_strike			t_over_strike(cur_term)
#define prtr_silent			t_prtr_silent(cur_term)
#define row_addr_glitch			t_row_addr_glitch(cur_term)
#define semi_auto_right_margin		t_semi_auto_right_margin(cur_term)
#define status_line_esc_ok		t_status_line_esc_ok(cur_term)
#define tilde_glitch			t_tilde_glitch(cur_term)
#define transparent_underline		t_transparent_underline(cur_term)
#define xon_xoff			t_xon_xoff(cur_term)

/*
 * BOOLEAN DESCRIPTIONS
 *
 * auto_left_margin: cub1 wraps from column 0 to last column
 * auto_right_margin: Terminal has automatic margins
 * back_color_erase: Screen erased with background colour
 * can_change: Terminal can re-define existing colour
 * ceol_standout_glitch: Standout not erased by overwriting (hp)
 * col_addr_glitch: Only positive motion for hpa/mhba caps
 * cpi_changes_res: Changing character pitch changes resolution
 * cr_cancels_micro_mode: Using cr turns off micro mode
 * dest_tabs_magic_smso: Destructive tabs, magic smso char (t1061)
 * eat_newline_glitch: Newline ignored after 80 columns (Concept)
 * erase_overstrike: Can erase overstrikes with a blank line
 * generic_type: Generic line type (e.g. dialup, switch)
 * hard_copy: Hardcopy terminal
 * hard_cursor: Cursor is hard to see
 * has_meta_key: Has a meta key (shift, sets parity bit)
 * has_print_wheel: Printer needs operator to change character set
 * has_status_line: Has extra "status line"
 * hue_light_saturation: Terminal only uses HLS colour notion (Tektronix)
 * insert_null_glitch: Insert mode distinguishes nulls
 * lpi_changes_yes: Changing line pitch changes resolution
 * memory_above: Display may be retained above the screen
 * memory_below: Display may be retained below the screen
 * move_insert_mode: Safe to move while in insert mode
 * move_standout_mode: Safe to move in standout modes
 * needs_xon_xoff: Padding won't work, xon/xoff required
 * no_esc_ctlc: Beehive (f1=escape, f2=ctrl C)
 * no_pad_char: Pad character doesn't exist
 * non_dest_scroll_region: Scrolling region is nondestructive
 * non_rev_rmcup: smcup does not reverse rmcup
 * over_strike: Terminal overstrikes on hard-copy terminal
 * prtr_silent: Printer won't echo on screen
 * row_addr_glitch: Only positive motion for vpa/mvpa caps
 * semi_auto_right_margin: Printing in last column causes cr
 * status_line_esc_ok: Escape can be used on the status line
 * tilde_glitch: Hazeltine; can't print tilde (~)
 * transparent_underline: Underline character overstrikes
 * xon_xoff: Terminal uses xon/xoff handshaking
*/

/* Define available terminfo numbers */
enum TINUMS {
	TICODE_bitwin,
	TICODE_bitype,
	TICODE_bufsz,
	TICODE_btns,
	TICODE_cols,
	TICODE_spinh,
	TICODE_spinv,
	TICODE_it,
	TICODE_lh,
	TICODE_lw,
	TICODE_lines,
	TICODE_lm,
	TICODE_ma,
	TICODE_xmc,
	TICODE_colors,
	TICODE_maddr,
	TICODE_mjump,
	TICODE_pairs,
	TICODE_wnum,
	TICODE_mcs,
	TICODE_mls,
	TICODE_ncv,
	TICODE_nlab,
	TICODE_npins,
	TICODE_orc,
	TICODE_orl,
	TICODE_orhi,
	TICODE_orvi,
	TICODE_pb,
	TICODE_cps,
	TICODE_vt,
	TICODE_widcs,
	TICODE_wsl
};
#define TINUMMAX			TICODE_wsl

#define t_bit_image_entwining(t)	(t)->nums[TICODE_bitwin]
#define t_bit_image_type(t)		(t)->nums[TICODE_bitype]
#define t_buffer_capacity(t)		(t)->nums[TICODE_bufsz]
#define t_buttons(t)			(t)->nums[TICODE_btns]
#define t_columns(t)			(t)->nums[TICODE_cols]
#define t_dot_horz_spacing(t)		(t)->nums[TICODE_spinh]
#define t_dot_vert_spacing(t)		(t)->nums[TICODE_spinv]
#define t_init_tabs(t)			(t)->nums[TICODE_it]
#define t_label_height(t)		(t)->nums[TICODE_lh]
#define t_label_width(t)		(t)->nums[TICODE_lw]
#define t_lines(t)			(t)->nums[TICODE_lines]
#define t_lines_of_memory(t)		(t)->nums[TICODE_lm]
#define t_max_attributes(t)		(t)->nums[TICODE_ma]
#define t_magic_cookie_glitch(t)	(t)->nums[TICODE_xmc]
#define t_max_colors(t)			(t)->nums[TICODE_colors]
#define t_max_micro_address(t)		(t)->nums[TICODE_maddr]
#define t_max_micro_jump(t)		(t)->nums[TICODE_mjump]
#define t_max_pairs(t)			(t)->nums[TICODE_pairs]
#define t_maximum_windows(t)		(t)->nums[TICODE_wnum]
#define t_micro_col_size(t)		(t)->nums[TICODE_mcs]
#define t_micro_line_size(t)		(t)->nums[TICODE_mls]
#define t_no_color_video(t)		(t)->nums[TICODE_ncv]
#define t_num_labels(t)			(t)->nums[TICODE_nlab]
#define t_number_of_pins(t)		(t)->nums[TICODE_npins]
#define t_output_res_char(t)		(t)->nums[TICODE_orc]
#define t_output_res_line(t)		(t)->nums[TICODE_orl]
#define t_output_res_horz_inch(t)	(t)->nums[TICODE_orhi]
#define t_output_res_vert_inch(t)	(t)->nums[TICODE_orvi]
#define t_padding_baud_rate(t)		(t)->nums[TICODE_pb]
#define t_print_rate(t)			(t)->nums[TICODE_cps]
#define t_virtual_terminal(t)		(t)->nums[TICODE_vt]
#define t_wide_char_size(t)		(t)->nums[TICODE_widcs]
#define t_width_status_line(t)		(t)->nums[TICODE_wsl]

#define bit_image_entwining		 t_bit_image_entwining(cur_term)
#define bit_image_type			 t_bit_image_type(cur_term)
#define buffer_capacity			 t_buffer_capacity(cur_term)
#define buttons				 t_buttons(cur_term)
#define columns				 t_columns(cur_term)
#define dot_horz_spacing		 t_dot_horz_spacing(cur_term)
#define dot_vert_spacing		 t_dot_vert_spacing(cur_term)
#define init_tabs			 t_init_tabs(cur_term)
#define label_height			 t_label_height(cur_term)
#define label_width			 t_label_width(cur_term)
#define lines				 t_lines(cur_term)
#define lines_of_memory			 t_lines_of_memory(cur_term)
#define max_attributes			 t_max_attributes(cur_term)
#define magic_cookie_glitch		 t_magic_cookie_glitch(cur_term)
#define max_colors			 t_max_colors(cur_term)
#define max_micro_address		 t_max_micro_address(cur_term)
#define max_micro_jump			 t_max_micro_jump(cur_term)
#define max_pairs			 t_max_pairs(cur_term)
#define maximum_windows			 t_maximum_windows(cur_term)
#define micro_col_size			 t_micro_col_size(cur_term)
#define micro_line_size			 t_micro_line_size(cur_term)
#define no_color_video			 t_no_color_video(cur_term)
#define num_labels			 t_num_labels(cur_term)
#define number_of_pins			 t_number_of_pins(cur_term)
#define output_res_char			 t_output_res_char(cur_term)
#define output_res_line			 t_output_res_line(cur_term)
#define output_res_horz_inch		 t_output_res_horz_inch(cur_term)
#define output_res_vert_inch		 t_output_res_vert_inch(cur_term)
#define padding_baud_rate		 t_padding_baud_rate(cur_term)
#define print_rate			 t_print_rate(cur_term)
#define virtual_terminal		 t_virtual_terminal(cur_term)
#define wide_char_size			 t_wide_char_size(cur_term)
#define width_status_line		 t_width_status_line(cur_term)

/*
 * NUMBER DESCRIPTIONS
 *
 * bit_image_entwining: Number of passes for each bit-map row
 * bit_image_type: Type of bit image device
 * buffer_capacity: Number of bytes buffered before printing
 * buttons: Number of buttons on the mouse
 * columns: Number of columns in a line
 * dot_horz_spacing: Spacing of dots horizontally in dots per inch
 * dot_vert_spacing: Spacing of pins vertically in pins per inch
 * init_tabs: Tabs initially every #1 spaces
 * label_height: Number of rows in each label
 * label_width: Numbre of columns in each label
 * lines: Number of lines on a screen or a page
 * lines_of_memory: Lines of memory of > lines; 0 means varies
 * max_attributes: Maximum combined video attributes terminal can display
 * magic_cookie_glitch: Number of blank characters left by smso or rmso
 * max_colors: Maximum number of colours on the screen
 * max_micro_address: Maximum value in micro_..._addresss
 * max_micro_jump: Maximum value in parm_..._micro
 * max_pairs: Maximum number of colour-pairs on the screen
 * maximum_windows: Maximum number of definable windows
 * micro_col_size: Character step size when in micro mode
 * micro_line_size: Line step size when in micro mode
 * no_color_video: Video attributes that can't be used with colours
 * num_labels: Number of labels on screen (start at 1)
 * number_of_pins: Number of pins in print-head
 * output_res_char: Horizontal resolution in units per character
 * output_res_line: Vertical resolution in units per line
 * output_res_horz_inch: Horizontal resolution in units per inch
 * output_res_vert_inch: Vertical resolution in units per inch
 * padding_baud_rate: Lowest baud rate where padding needed
 * print_rate: Print rate in characters per second
 * virtual_terminal: Virtual terminal number
 * wide_char_size: Character step size when in double-wide mode
 * width_status_line: Number of columns in status line
 */

/* Define available terminfo strings */
enum TISTRS{
	TICODE_acsc,
	TICODE_scesa,
	TICODE_cbt,
	TICODE_bel,
	TICODE_bicr,
	TICODE_binel,
	TICODE_birep,
	TICODE_cr,
	TICODE_cpi,
	TICODE_lpi,
	TICODE_chr,
	TICODE_cvr,
	TICODE_csr,
	TICODE_rmp,
	TICODE_csnm,
	TICODE_tbc,
	TICODE_mgc,
	TICODE_clear,
	TICODE_el1,
	TICODE_el,
	TICODE_ed,
	TICODE_csin,
	TICODE_colornm,
	TICODE_hpa,
	TICODE_cmdch,
	TICODE_cwin,
	TICODE_cup,
	TICODE_cud1,
	TICODE_home,
	TICODE_civis,
	TICODE_cub1,
	TICODE_mrcup,
	TICODE_cnorm,
	TICODE_cuf1,
	TICODE_ll,
	TICODE_cuu1,
	TICODE_cvvis,
	TICODE_defbi,
	TICODE_defc,
	TICODE_dch1,
	TICODE_dl1,
	TICODE_devt,
	TICODE_dial,
	TICODE_dsl,
	TICODE_dclk,
	TICODE_dispc,
	TICODE_hd,
	TICODE_enacs,
	TICODE_endbi,
	TICODE_smacs,
	TICODE_smam,
	TICODE_blink,
	TICODE_bold,
	TICODE_smcup,
	TICODE_smdc,
	TICODE_dim,
	TICODE_swidm,
	TICODE_sdrfq,
	TICODE_ehhlm,
	TICODE_smir,
	TICODE_sitm,
	TICODE_elhlm,
	TICODE_slm,
	TICODE_elohlm,
	TICODE_smicm,
	TICODE_snlq,
	TICODE_snrmq,
	TICODE_smpch,
	TICODE_prot,
	TICODE_rev,
	TICODE_erhlm,
	TICODE_smsc,
	TICODE_invis,
	TICODE_sshm,
	TICODE_smso,
	TICODE_ssubm,
	TICODE_ssupm,
	TICODE_ethlm,
	TICODE_smul,
	TICODE_sum,
	TICODE_evhlm,
	TICODE_smxon,
	TICODE_ech,
	TICODE_rmacs,
	TICODE_rmam,
	TICODE_sgr0,
	TICODE_rmcup,
	TICODE_rmdc,
	TICODE_rwidm,
	TICODE_rmir,
	TICODE_ritm,
	TICODE_rlm,
	TICODE_rmicm,
	TICODE_rmpch,
	TICODE_rmsc,
	TICODE_rshm,
	TICODE_rmso,
	TICODE_rsubm,
	TICODE_rsupm,
	TICODE_rmul,
	TICODE_rum,
	TICODE_rmxon,
	TICODE_pause,
	TICODE_hook,
	TICODE_flash,
	TICODE_ff,
	TICODE_fsl,
	TICODE_getm,
	TICODE_wingo,
	TICODE_hup,
	TICODE_is1,
	TICODE_is2,
	TICODE_is3,
	TICODE_if,
	TICODE_iprog,
	TICODE_initc,
	TICODE_initp,
	TICODE_ich1,
	TICODE_il1,
	TICODE_ip,
	TICODE_ka1,
	TICODE_ka3,
	TICODE_kb2,
	TICODE_kbs,
	TICODE_kbeg,
	TICODE_kcbt,
	TICODE_kc1,
	TICODE_kc3,
	TICODE_kcan,
	TICODE_ktbc,
	TICODE_kclr,
	TICODE_kclo,
	TICODE_kcmd,
	TICODE_kcpy,
	TICODE_kcrt,
	TICODE_kctab,
	TICODE_kdch1,
	TICODE_kdl1,
	TICODE_kcud1,
	TICODE_krmir,
	TICODE_kend,
	TICODE_kent,
	TICODE_kel,
	TICODE_ked,
	TICODE_kext,
	TICODE_kf0,
	TICODE_kf1,
	TICODE_kf2,
	TICODE_kf3,
	TICODE_kf4,
	TICODE_kf5,
	TICODE_kf6,
	TICODE_kf7,
	TICODE_kf8,
	TICODE_kf9,
	TICODE_kf10,
	TICODE_kf11,
	TICODE_kf12,
	TICODE_kf13,
	TICODE_kf14,
	TICODE_kf15,
	TICODE_kf16,
	TICODE_kf17,
	TICODE_kf18,
	TICODE_kf19,
	TICODE_kf20,
	TICODE_kf21,
	TICODE_kf22,
	TICODE_kf23,
	TICODE_kf24,
	TICODE_kf25,
	TICODE_kf26,
	TICODE_kf27,
	TICODE_kf28,
	TICODE_kf29,
	TICODE_kf30,
	TICODE_kf31,
	TICODE_kf32,
	TICODE_kf33,
	TICODE_kf34,
	TICODE_kf35,
	TICODE_kf36,
	TICODE_kf37,
	TICODE_kf38,
	TICODE_kf39,
	TICODE_kf40,
	TICODE_kf41,
	TICODE_kf42,
	TICODE_kf43,
	TICODE_kf44,
	TICODE_kf45,
	TICODE_kf46,
	TICODE_kf47,
	TICODE_kf48,
	TICODE_kf49,
	TICODE_kf50,
	TICODE_kf51,
	TICODE_kf52,
	TICODE_kf53,
	TICODE_kf54,
	TICODE_kf55,
	TICODE_kf56,
	TICODE_kf57,
	TICODE_kf58,
	TICODE_kf59,
	TICODE_kf60,
	TICODE_kf61,
	TICODE_kf62,
	TICODE_kf63,
	TICODE_kfnd,
	TICODE_khlp,
	TICODE_khome,
	TICODE_kich1,
	TICODE_kil1,
	TICODE_kcub1,
	TICODE_kll,
	TICODE_kmrk,
	TICODE_kmsg,
	TICODE_kmous,
	TICODE_kmov,
	TICODE_knxt,
	TICODE_knp,
	TICODE_kopn,
	TICODE_kopt,
	TICODE_kpp,
	TICODE_kprv,
	TICODE_kprt,
	TICODE_krdo,
	TICODE_kref,
	TICODE_krfr,
	TICODE_krpl,
	TICODE_krst,
	TICODE_kres,
	TICODE_kcuf1,
	TICODE_ksav,
	TICODE_kBEG,
	TICODE_kCAN,
	TICODE_kCMD,
	TICODE_kCPY,
	TICODE_kCRT,
	TICODE_kDC,
	TICODE_kDL,
	TICODE_kslt,
	TICODE_kEND,
	TICODE_kEOL,
	TICODE_kEXT,
	TICODE_kind,
	TICODE_kFND,
	TICODE_kHLP,
	TICODE_kHOM,
	TICODE_kIC,
	TICODE_kLFT,
	TICODE_kMSG,
	TICODE_kMOV,
	TICODE_kNXT,
	TICODE_kOPT,
	TICODE_kPRV,
	TICODE_kPRT,
	TICODE_kri,
	TICODE_kRDO,
	TICODE_kRPL,
	TICODE_kRIT,
	TICODE_kRES,
	TICODE_kSAV,
	TICODE_kSPD,
	TICODE_khts,
	TICODE_kUND,
	TICODE_kspd,
	TICODE_kund,
	TICODE_kcuu1,
	TICODE_rmkx,
	TICODE_smkx,
	TICODE_lf0,
	TICODE_lf1,
	TICODE_lf2,
	TICODE_lf3,
	TICODE_lf4,
	TICODE_lf5,
	TICODE_lf6,
	TICODE_lf7,
	TICODE_lf8,
	TICODE_lf9,
	TICODE_lf10,
	TICODE_fln,
	TICODE_rmln,
	TICODE_smln,
	TICODE_rmm,
	TICODE_smm,
	TICODE_mhpa,
	TICODE_mcud1,
	TICODE_mcub1,
	TICODE_mcuf1,
	TICODE_mvpa,
	TICODE_mcuu1,
	TICODE_minfo,
	TICODE_nel,
	TICODE_porder,
	TICODE_oc,
	TICODE_op,
	TICODE_pad,
	TICODE_dch,
	TICODE_dl,
	TICODE_cud,
	TICODE_mcud,
	TICODE_ich,
	TICODE_indn,
	TICODE_il,
	TICODE_cub,
	TICODE_mcub,
	TICODE_cuf,
	TICODE_mcuf,
	TICODE_rin,
	TICODE_cuu,
	TICODE_mcuu,
	TICODE_pctrm,
	TICODE_pfkey,
	TICODE_pfloc,
	TICODE_pfxl,
	TICODE_pfx,
	TICODE_pln,
	TICODE_mc0,
	TICODE_mc5p,
	TICODE_mc4,
	TICODE_mc5,
	TICODE_pulse,
	TICODE_qdial,
	TICODE_rmclk,
	TICODE_rep,
	TICODE_rfi,
	TICODE_reqmp,
	TICODE_rs1,
	TICODE_rs2,
	TICODE_rs3,
	TICODE_rf,
	TICODE_rc,
	TICODE_vpa,
	TICODE_sc,
	TICODE_scesc,
	TICODE_ind,
	TICODE_ri,
	TICODE_scs,
	TICODE_s0ds,
	TICODE_s1ds,
	TICODE_s2ds,
	TICODE_s3ds,
	TICODE_sgr1,
	TICODE_setab,
	TICODE_setaf,
	TICODE_sgr,
	TICODE_setb,
	TICODE_smgb,
	TICODE_smgbp,
	TICODE_sclk,
	TICODE_setcolor,
	TICODE_scp,
	TICODE_setf,
	TICODE_smgl,
	TICODE_smglp,
	TICODE_smglr,
	TICODE_slines,
	TICODE_slength,
	TICODE_smgr,
	TICODE_smgrp,
	TICODE_hts,
	TICODE_smgtb,
	TICODE_smgt,
	TICODE_smgtp,
	TICODE_wind,
	TICODE_sbim,
	TICODE_scsd,
	TICODE_rbim,
	TICODE_rcsd,
	TICODE_subcs,
	TICODE_supcs,
	TICODE_ht,
	TICODE_docr,
	TICODE_tsl,
	TICODE_tone,
	TICODE_u0,
	TICODE_u1,
	TICODE_u2,
	TICODE_u3,
	TICODE_u4,
	TICODE_u5,
	TICODE_u6,
	TICODE_u7,
	TICODE_u8,
	TICODE_u9,
	TICODE_uc,
	TICODE_hu,
	TICODE_wait,
	TICODE_xoffc,
	TICODE_xonc,
	TICODE_zerom
};
#define TISTRMAX			TICODE_zerom

#define t_acs_chars(t)			(t)->strs[TICODE_acsc]
#define t_alt_scancode_esc(t)		(t)->strs[TICODE_scesa]
#define t_back_tab(t)			(t)->strs[TICODE_cbt]
#define t_bell(t)			(t)->strs[TICODE_bel]
#define t_bit_image_carriage_return(t)	(t)->strs[TICODE_bicr]
#define t_bit_image_newline(t)		(t)->strs[TICODE_binel]
#define t_bit_image_repeat(t)		(t)->strs[TICODE_birep]
#define t_carriage_return(t)		(t)->strs[TICODE_cr]
#define t_change_char_pitch(t)		(t)->strs[TICODE_cpi]
#define t_change_line_pitch(t)		(t)->strs[TICODE_lpi]
#define t_change_res_horz(t)		(t)->strs[TICODE_chr]
#define t_change_res_vert(t)		(t)->strs[TICODE_cvr]
#define t_change_scroll_region(t)	(t)->strs[TICODE_csr]
#define t_char_padding(t)		(t)->strs[TICODE_rmp]
#define t_char_set_names(t)		(t)->strs[TICODE_csnm]
#define t_clear_all_tabs(t)		(t)->strs[TICODE_tbc]
#define t_clear_margins(t)		(t)->strs[TICODE_mgc]
#define t_clear_screen(t)		(t)->strs[TICODE_clear]
#define t_clr_bol(t)			(t)->strs[TICODE_el1]
#define t_clr_eol(t)			(t)->strs[TICODE_el]
#define t_clr_eos(t)			(t)->strs[TICODE_ed]
#define t_code_set_init(t)		(t)->strs[TICODE_csin]
#define t_color_names(t)		(t)->strs[TICODE_colornm]
#define t_column_address(t)		(t)->strs[TICODE_hpa]
#define t_command_character(t)		(t)->strs[TICODE_cmdch]
#define t_create_window(t)		(t)->strs[TICODE_cwin]
#define t_cursor_address(t)		(t)->strs[TICODE_cup]
#define t_cursor_down(t)		(t)->strs[TICODE_cud1]
#define t_cursor_home(t)		(t)->strs[TICODE_home]
#define t_cursor_invisible(t)		(t)->strs[TICODE_civis]
#define t_cursor_left(t)		(t)->strs[TICODE_cub1]
#define t_cursor_mem_address(t)		(t)->strs[TICODE_mrcup]
#define t_cursor_normal(t)		(t)->strs[TICODE_cnorm]
#define t_cursor_right(t)		(t)->strs[TICODE_cuf1]
#define t_cursor_to_ll(t)		(t)->strs[TICODE_ll]
#define t_cursor_up(t)			(t)->strs[TICODE_cuu1]
#define t_cursor_visible(t)		(t)->strs[TICODE_cvvis]
#define t_define_bit_image_region(t)	(t)->strs[TICODE_defbi]
#define t_define_char(t)		(t)->strs[TICODE_defc]
#define t_delete_character(t)		(t)->strs[TICODE_dch1]
#define t_delete_line(t)		(t)->strs[TICODE_dl1]
#define t_device_type(t)		(t)->strs[TICODE_devt]
#define t_dial_phone(t)			(t)->strs[TICODE_dial]
#define t_dis_status_line(t)		(t)->strs[TICODE_dsl]
#define t_display_clock(t)		(t)->strs[TICODE_dclk]
#define t_display_pc_char(t)		(t)->strs[TICODE_dispc]
#define t_down_half_time(t)		(t)->strs[TICODE_hd]
#define t_ena_acs(t)			(t)->strs[TICODE_enacs]
#define t_end_bit_image_region(t)	(t)->strs[TICODE_endbi]
#define t_enter_alt_charset_mode(t)	(t)->strs[TICODE_smacs]
#define t_enter_am_mode(t)		(t)->strs[TICODE_smam]
#define t_enter_blink_mode(t)		(t)->strs[TICODE_blink]
#define t_enter_bold_mode(t)		(t)->strs[TICODE_bold]
#define t_enter_ca_mode(t)		(t)->strs[TICODE_smcup]
#define t_enter_delete_mode(t)		(t)->strs[TICODE_smdc]
#define t_enter_dim_mode(t)		(t)->strs[TICODE_dim]
#define t_enter_doublewide_mode(t)	(t)->strs[TICODE_swidm]
#define t_enter_draft_quality(t)	(t)->strs[TICODE_sdrfq]
#define t_enter_horizontal_hl_mode(t)	(t)->strs[TICODE_ehhlm]
#define t_enter_insert_mode(t)		(t)->strs[TICODE_smir]
#define t_enter_italics_mode(t)		(t)->strs[TICODE_sitm]
#define t_enter_left_hl_mode(t)		(t)->strs[TICODE_elhlm]
#define t_enter_leftward_mode(t)	(t)->strs[TICODE_slm]
#define t_enter_low_hl_mode(t)		(t)->strs[TICODE_elohlm]
#define t_enter_micro_mode(t)		(t)->strs[TICODE_smicm]
#define t_enter_near_quality_letter(t)	(t)->strs[TICODE_snlq]
#define t_enter_normal_quality(t)	(t)->strs[TICODE_snrmq]
#define t_enter_pc_charset_mode(t)	(t)->strs[TICODE_smpch]
#define t_enter_protected_mode(t)	(t)->strs[TICODE_prot]
#define t_enter_reverse_mode(t)		(t)->strs[TICODE_rev]
#define t_enter_right_hl_mode(t)	(t)->strs[TICODE_erhlm]
#define t_enter_scancode_mode(t)	(t)->strs[TICODE_smsc]
#define t_enter_secure_mode(t)		(t)->strs[TICODE_invis]
#define t_enter_shadow_mode(t)		(t)->strs[TICODE_sshm]
#define t_enter_standout_mode(t)	(t)->strs[TICODE_smso]
#define t_enter_subscript_mode(t)	(t)->strs[TICODE_ssubm]
#define t_enter_superscript_mode(t)	(t)->strs[TICODE_ssupm]
#define t_enter_top_hl_mode(t)		(t)->strs[TICODE_ethlm]
#define t_enter_underline_mode(t)	(t)->strs[TICODE_smul]
#define t_enter_upward_mode(t)		(t)->strs[TICODE_sum]
#define t_enter_vertical_hl_mode(t)	(t)->strs[TICODE_evhlm]
#define t_enter_xon_mode(t)		(t)->strs[TICODE_smxon]
#define t_erase_chars(t)		(t)->strs[TICODE_ech]
#define t_exit_alt_charset_mode(t)	(t)->strs[TICODE_rmacs]
#define t_exit_am_mode(t)		(t)->strs[TICODE_rmam]
#define t_exit_attribute_mode(t)	(t)->strs[TICODE_sgr0]
#define t_exit_ca_mode(t)		(t)->strs[TICODE_rmcup]
#define t_exit_delete_mode(t)		(t)->strs[TICODE_rmdc]
#define t_exit_doublewide_mode(t)	(t)->strs[TICODE_rwidm]
#define t_exit_insert_mode(t)		(t)->strs[TICODE_rmir]
#define t_exit_italics_mode(t)		(t)->strs[TICODE_ritm]
#define t_exit_leftward_mode(t)		(t)->strs[TICODE_rlm]
#define t_exit_micro_mode(t)		(t)->strs[TICODE_rmicm]
#define t_exit_pc_charset_mode(t)	(t)->strs[TICODE_rmpch]
#define t_exit_scancode_mode(t)		(t)->strs[TICODE_rmsc]
#define t_exit_shadow_mode(t)		(t)->strs[TICODE_rshm]
#define t_exit_standout_mode(t)		(t)->strs[TICODE_rmso]
#define t_exit_subscript_mode(t)	(t)->strs[TICODE_rsubm]
#define t_exit_superscript_mode(t)	(t)->strs[TICODE_rsupm]
#define t_exit_underline_mode(t)	(t)->strs[TICODE_rmul]
#define t_exit_upward_mode(t)		(t)->strs[TICODE_rum]
#define t_exit_xon_mode(t)		(t)->strs[TICODE_rmxon]
#define t_fixed_pause(t)		(t)->strs[TICODE_pause]
#define t_flash_hook(t)			(t)->strs[TICODE_hook]
#define t_flash_screen(t)		(t)->strs[TICODE_flash]
#define t_form_feed(t)			(t)->strs[TICODE_ff]
#define t_from_status_line(t)		(t)->strs[TICODE_fsl]
#define t_get_mouse(t)			(t)->strs[TICODE_getm]
#define t_goto_window(t)		(t)->strs[TICODE_wingo]
#define t_hangup(t)			(t)->strs[TICODE_hup]
#define t_init_1string(t)		(t)->strs[TICODE_is1]
#define t_init_2string(t)		(t)->strs[TICODE_is2]
#define t_init_3string(t)		(t)->strs[TICODE_is3]
#define t_init_file(t)			(t)->strs[TICODE_if]
#define t_init_prog(t)			(t)->strs[TICODE_iprog]
#define t_initialize_color(t)		(t)->strs[TICODE_initc]
#define t_initialize_pair(t)		(t)->strs[TICODE_initp]
#define t_insert_character(t)		(t)->strs[TICODE_ich1]
#define t_insert_line(t)		(t)->strs[TICODE_il1]
#define t_insert_padding(t)		(t)->strs[TICODE_ip]
#define t_key_a1(t)			(t)->strs[TICODE_ka1]
#define t_key_a3(t)			(t)->strs[TICODE_ka3]
#define t_key_b2(t)			(t)->strs[TICODE_kb2]
#define t_key_backspace(t)		(t)->strs[TICODE_kbs]
#define t_key_beg(t)			(t)->strs[TICODE_kbeg]
#define t_key_btab(t)			(t)->strs[TICODE_kcbt]
#define t_key_c1(t)			(t)->strs[TICODE_kc1]
#define t_key_c3(t)			(t)->strs[TICODE_kc3]
#define t_key_cancel(t)			(t)->strs[TICODE_kcan]
#define t_key_catab(t)			(t)->strs[TICODE_ktbc]
#define t_key_clear(t)			(t)->strs[TICODE_kclr]
#define t_key_close(t)			(t)->strs[TICODE_kclo]
#define t_key_command(t)		(t)->strs[TICODE_kcmd]
#define t_key_copy(t)			(t)->strs[TICODE_kcpy]
#define t_key_create(t)			(t)->strs[TICODE_kcrt]
#define t_key_ctab(t)			(t)->strs[TICODE_kctab]
#define t_key_dc(t)			(t)->strs[TICODE_kdch1]
#define t_key_dl(t)			(t)->strs[TICODE_kdl1]
#define t_key_down(t)			(t)->strs[TICODE_kcud1]
#define t_key_eic(t)			(t)->strs[TICODE_krmir]
#define t_key_end(t)			(t)->strs[TICODE_kend]
#define t_key_enter(t)			(t)->strs[TICODE_kent]
#define t_key_eol(t)			(t)->strs[TICODE_kel]
#define t_key_eos(t)			(t)->strs[TICODE_ked]
#define t_key_exit(t)			(t)->strs[TICODE_kext]
#define t_key_f0(t)			(t)->strs[TICODE_kf0]
#define t_key_f1(t)			(t)->strs[TICODE_kf1]
#define t_key_f2(t)			(t)->strs[TICODE_kf2]
#define t_key_f3(t)			(t)->strs[TICODE_kf3]
#define t_key_f4(t)			(t)->strs[TICODE_kf4]
#define t_key_f5(t)			(t)->strs[TICODE_kf5]
#define t_key_f6(t)			(t)->strs[TICODE_kf6]
#define t_key_f7(t)			(t)->strs[TICODE_kf7]
#define t_key_f8(t)			(t)->strs[TICODE_kf8]
#define t_key_f9(t)			(t)->strs[TICODE_kf9]
#define t_key_f10(t)			(t)->strs[TICODE_kf10]
#define t_key_f11(t)			(t)->strs[TICODE_kf11]
#define t_key_f12(t)			(t)->strs[TICODE_kf12]
#define t_key_f13(t)			(t)->strs[TICODE_kf13]
#define t_key_f14(t)			(t)->strs[TICODE_kf14]
#define t_key_f15(t)			(t)->strs[TICODE_kf15]
#define t_key_f16(t)			(t)->strs[TICODE_kf16]
#define t_key_f17(t)			(t)->strs[TICODE_kf17]
#define t_key_f18(t)			(t)->strs[TICODE_kf18]
#define t_key_f19(t)			(t)->strs[TICODE_kf19]
#define t_key_f20(t)			(t)->strs[TICODE_kf20]
#define t_key_f21(t)			(t)->strs[TICODE_kf21]
#define t_key_f22(t)			(t)->strs[TICODE_kf22]
#define t_key_f23(t)			(t)->strs[TICODE_kf23]
#define t_key_f24(t)			(t)->strs[TICODE_kf24]
#define t_key_f25(t)			(t)->strs[TICODE_kf25]
#define t_key_f26(t)			(t)->strs[TICODE_kf26]
#define t_key_f27(t)			(t)->strs[TICODE_kf27]
#define t_key_f28(t)			(t)->strs[TICODE_kf28]
#define t_key_f29(t)			(t)->strs[TICODE_kf29]
#define t_key_f30(t)			(t)->strs[TICODE_kf30]
#define t_key_f31(t)			(t)->strs[TICODE_kf31]
#define t_key_f32(t)			(t)->strs[TICODE_kf32]
#define t_key_f33(t)			(t)->strs[TICODE_kf33]
#define t_key_f34(t)			(t)->strs[TICODE_kf34]
#define t_key_f35(t)			(t)->strs[TICODE_kf35]
#define t_key_f36(t)			(t)->strs[TICODE_kf36]
#define t_key_f37(t)			(t)->strs[TICODE_kf37]
#define t_key_f38(t)			(t)->strs[TICODE_kf38]
#define t_key_f39(t)			(t)->strs[TICODE_kf39]
#define t_key_f40(t)			(t)->strs[TICODE_kf40]
#define t_key_f41(t)			(t)->strs[TICODE_kf41]
#define t_key_f42(t)			(t)->strs[TICODE_kf42]
#define t_key_f43(t)			(t)->strs[TICODE_kf43]
#define t_key_f44(t)			(t)->strs[TICODE_kf44]
#define t_key_f45(t)			(t)->strs[TICODE_kf45]
#define t_key_f46(t)			(t)->strs[TICODE_kf46]
#define t_key_f47(t)			(t)->strs[TICODE_kf47]
#define t_key_f48(t)			(t)->strs[TICODE_kf48]
#define t_key_f49(t)			(t)->strs[TICODE_kf49]
#define t_key_f50(t)			(t)->strs[TICODE_kf50]
#define t_key_f51(t)			(t)->strs[TICODE_kf51]
#define t_key_f52(t)			(t)->strs[TICODE_kf52]
#define t_key_f53(t)			(t)->strs[TICODE_kf53]
#define t_key_f54(t)			(t)->strs[TICODE_kf54]
#define t_key_f55(t)			(t)->strs[TICODE_kf55]
#define t_key_f56(t)			(t)->strs[TICODE_kf56]
#define t_key_f57(t)			(t)->strs[TICODE_kf57]
#define t_key_f58(t)			(t)->strs[TICODE_kf58]
#define t_key_f59(t)			(t)->strs[TICODE_kf59]
#define t_key_f60(t)			(t)->strs[TICODE_kf60]
#define t_key_f61(t)			(t)->strs[TICODE_kf61]
#define t_key_f62(t)			(t)->strs[TICODE_kf62]
#define t_key_f63(t)			(t)->strs[TICODE_kf63]
#define t_key_find(t)			(t)->strs[TICODE_kfnd]
#define t_key_help(t)			(t)->strs[TICODE_khlp]
#define t_key_home(t)			(t)->strs[TICODE_khome]
#define t_key_ic(t)			(t)->strs[TICODE_kich1]
#define t_key_il(t)			(t)->strs[TICODE_kil1]
#define t_key_left(t)			(t)->strs[TICODE_kcub1]
#define t_key_ll(t)			(t)->strs[TICODE_kll]
#define t_key_mark(t)			(t)->strs[TICODE_kmrk]
#define t_key_message(t)		(t)->strs[TICODE_kmsg]
#define t_key_mouse(t)			(t)->strs[TICODE_kmous]
#define t_key_move(t)			(t)->strs[TICODE_kmov]
#define t_key_next(t)			(t)->strs[TICODE_knxt]
#define t_key_npage(t)			(t)->strs[TICODE_knp]
#define t_key_open(t)			(t)->strs[TICODE_kopn]
#define t_key_options(t)		(t)->strs[TICODE_kopt]
#define t_key_ppage(t)			(t)->strs[TICODE_kpp]
#define t_key_previous(t)		(t)->strs[TICODE_kprv]
#define t_key_print(t)			(t)->strs[TICODE_kprt]
#define t_key_redo(t)			(t)->strs[TICODE_krdo]
#define t_key_reference(t)		(t)->strs[TICODE_kref]
#define t_key_refresh(t)		(t)->strs[TICODE_krfr]
#define t_key_replace(t)		(t)->strs[TICODE_krpl]
#define t_key_restart(t)		(t)->strs[TICODE_krst]
#define t_key_resume(t)			(t)->strs[TICODE_kres]
#define t_key_right(t)			(t)->strs[TICODE_kcuf1]
#define t_key_save(t)			(t)->strs[TICODE_ksav]
#define t_key_sbeg(t)			(t)->strs[TICODE_kBEG]
#define t_key_scancel(t)		(t)->strs[TICODE_kCAN]
#define t_key_scommand(t)		(t)->strs[TICODE_kCMD]
#define t_key_scopy(t)			(t)->strs[TICODE_kCPY]
#define t_key_screate(t)		(t)->strs[TICODE_kCRT]
#define t_key_sdc(t)			(t)->strs[TICODE_kDC]
#define t_key_sdl(t)			(t)->strs[TICODE_kDL]
#define t_key_select(t)			(t)->strs[TICODE_kslt]
#define t_key_send(t)			(t)->strs[TICODE_kEND]
#define t_key_seol(t)			(t)->strs[TICODE_kEOL]
#define t_key_sexit(t)			(t)->strs[TICODE_kEXT]
#define t_key_sf(t)			(t)->strs[TICODE_kind]
#define t_key_sfind(t)			(t)->strs[TICODE_kFND]
#define t_key_shelp(t)			(t)->strs[TICODE_kHLP]
#define t_key_shome(t)			(t)->strs[TICODE_kHOM]
#define t_key_sic(t)			(t)->strs[TICODE_kIC]
#define t_key_sleft(t)			(t)->strs[TICODE_kLFT]
#define t_key_smessage(t)		(t)->strs[TICODE_kMSG]
#define t_key_smove(t)			(t)->strs[TICODE_kMOV]
#define t_key_snext(t)			(t)->strs[TICODE_kNXT]
#define t_key_soptions(t)		(t)->strs[TICODE_kOPT]
#define t_key_sprevious(t)		(t)->strs[TICODE_kPRV]
#define t_key_sprint(t)			(t)->strs[TICODE_kPRT]
#define t_key_sr(t)			(t)->strs[TICODE_kri]
#define t_key_sredo(t)			(t)->strs[TICODE_kRDO]
#define t_key_sreplace(t)		(t)->strs[TICODE_kRPL]
#define t_key_sright(t)			(t)->strs[TICODE_kRIT]
#define t_key_srsume(t)			(t)->strs[TICODE_kRES]
#define t_key_ssave(t)			(t)->strs[TICODE_kSAV]
#define t_key_ssuspend(t)		(t)->strs[TICODE_kSPD]
#define t_key_stab(t)			(t)->strs[TICODE_khts]
#define t_key_sundo(t)			(t)->strs[TICODE_kUND]
#define t_key_suspend(t)		(t)->strs[TICODE_kspd]
#define t_key_undo(t)			(t)->strs[TICODE_kund]
#define t_key_up(t)			(t)->strs[TICODE_kcuu1]
#define t_keypad_local(t)		(t)->strs[TICODE_rmkx]
#define t_keypad_xmit(t)		(t)->strs[TICODE_smkx]
#define t_lab_f0(t)			(t)->strs[TICODE_lf0]
#define t_lab_f1(t)			(t)->strs[TICODE_lf1]
#define t_lab_f2(t)			(t)->strs[TICODE_lf2]
#define t_lab_f3(t)			(t)->strs[TICODE_lf3]
#define t_lab_f4(t)			(t)->strs[TICODE_lf4]
#define t_lab_f5(t)			(t)->strs[TICODE_lf5]
#define t_lab_f6(t)			(t)->strs[TICODE_lf6]
#define t_lab_f7(t)			(t)->strs[TICODE_lf7]
#define t_lab_f8(t)			(t)->strs[TICODE_lf8]
#define t_lab_f9(t)			(t)->strs[TICODE_lf9]
#define t_lab_f10(t)			(t)->strs[TICODE_lf10]
#define t_label_format(t)		(t)->strs[TICODE_fln]
#define t_label_off(t)			(t)->strs[TICODE_rmln]
#define t_label_on(t)			(t)->strs[TICODE_smln]
#define t_meta_off(t)			(t)->strs[TICODE_rmm]
#define t_meta_on(t)			(t)->strs[TICODE_smm]
#define t_micro_column_address(t)	(t)->strs[TICODE_mhpa]
#define t_micro_down(t)			(t)->strs[TICODE_mcud1]
#define t_micro_left(t)			(t)->strs[TICODE_mcub1]
#define t_micro_right(t)		(t)->strs[TICODE_mcuf1]
#define t_micro_row_address(t)		(t)->strs[TICODE_mvpa]
#define t_micro_up(t)			(t)->strs[TICODE_mcuu1]
#define t_mouse_info(t)			(t)->strs[TICODE_minfo]
#define t_newline(t)			(t)->strs[TICODE_nel]
#define t_order_of_pins(t)		(t)->strs[TICODE_porder]
#define t_orig_colors(t)		(t)->strs[TICODE_oc]
#define t_orig_pair(t)			(t)->strs[TICODE_op]
#define t_pad_char(t)			(t)->strs[TICODE_pad]
#define t_parm_dch(t)			(t)->strs[TICODE_dch]
#define t_parm_delete_line(t)		(t)->strs[TICODE_dl]
#define t_parm_down_cursor(t)		(t)->strs[TICODE_cud]
#define t_parm_down_micro(t)		(t)->strs[TICODE_mcud]
#define t_parm_ich(t)			(t)->strs[TICODE_ich]
#define t_parm_index(t)			(t)->strs[TICODE_indn]
#define t_parm_insert_line(t)		(t)->strs[TICODE_il]
#define t_parm_left_cursor(t)		(t)->strs[TICODE_cub]
#define t_parm_left_micro(t)		(t)->strs[TICODE_mcub]
#define t_parm_right_cursor(t)		(t)->strs[TICODE_cuf]
#define t_parm_right_micro(t)		(t)->strs[TICODE_mcuf]
#define t_parm_rindex(t)		(t)->strs[TICODE_rin]
#define t_parm_up_cursor(t)		(t)->strs[TICODE_cuu]
#define t_parm_up_micro(t)		(t)->strs[TICODE_mcuu]
#define t_pc_term_options(t)		(t)->strs[TICODE_pctrm]
#define t_pkey_key(t)			(t)->strs[TICODE_pfkey]
#define t_pkey_local(t)			(t)->strs[TICODE_pfloc]
#define t_pkey_plab(t)			(t)->strs[TICODE_pfxl]
#define t_pkey_xmit(t)			(t)->strs[TICODE_pfx]
#define t_pkey_norm(t)			(t)->strs[TICODE_pln]
#define t_print_screen(t)		(t)->strs[TICODE_mc0]
#define t_ptr_non(t)			(t)->strs[TICODE_mc5p]
#define t_ptr_off(t)			(t)->strs[TICODE_mc4]
#define t_ptr_on(t)			(t)->strs[TICODE_mc5]
#define t_pulse(t)			(t)->strs[TICODE_pulse]
#define t_quick_dial(t)			(t)->strs[TICODE_qdial]
#define t_remove_clock(t)		(t)->strs[TICODE_rmclk]
#define t_repeat_char(t)		(t)->strs[TICODE_rep]
#define t_req_for_input(t)		(t)->strs[TICODE_rfi]
#define t_req_mouse_pos(t)		(t)->strs[TICODE_reqmp]
#define t_reset_1string(t)		(t)->strs[TICODE_rs1]
#define t_reset_2string(t)		(t)->strs[TICODE_rs2]
#define t_reset_3string(t)		(t)->strs[TICODE_rs3]
#define t_reset_file(t)			(t)->strs[TICODE_rf]
#define t_restore_cursor(t)		(t)->strs[TICODE_rc]
#define t_row_address(t)		(t)->strs[TICODE_vpa]
#define t_save_cursor(t)		(t)->strs[TICODE_sc]
#define t_scancode_escape(t)		(t)->strs[TICODE_scesc]
#define t_scroll_forward(t)		(t)->strs[TICODE_ind]
#define t_scroll_reverse(t)		(t)->strs[TICODE_ri]
#define t_select_char_set(t)		(t)->strs[TICODE_scs]
#define t_set0_des_seq(t)		(t)->strs[TICODE_s0ds]
#define t_set1_des_seq(t)		(t)->strs[TICODE_s1ds]
#define t_set2_des_seq(t)		(t)->strs[TICODE_s2ds]
#define t_set3_des_seq(t)		(t)->strs[TICODE_s3ds]
#define t_set_a_attributes(t)		(t)->strs[TICODE_sgr1]
#define t_set_a_background(t)		(t)->strs[TICODE_setab]
#define t_set_a_foreground(t)		(t)->strs[TICODE_setaf]
#define t_set_attributes(t)		(t)->strs[TICODE_sgr]
#define t_set_background(t)		(t)->strs[TICODE_setb]
#define t_set_bottom_margin(t)		(t)->strs[TICODE_smgb]
#define t_set_bottom_margin_parm(t)	(t)->strs[TICODE_smgbp]
#define t_set_clock(t)			(t)->strs[TICODE_sclk]
#define t_set_color_band(t)		(t)->strs[TICODE_setcolor]
#define t_set_color_pair(t)		(t)->strs[TICODE_scp]
#define t_set_foreground(t)		(t)->strs[TICODE_setf]
#define t_set_left_margin(t)		(t)->strs[TICODE_smgl]
#define t_set_left_margin_parm(t)	(t)->strs[TICODE_smglp]
#define t_set_lr_margin(t)		(t)->strs[TICODE_smglr]
#define t_set_page_length(t)		(t)->strs[TICODE_slines]
#define t_set_pglen_inch(t)		(t)->strs[TICODE_slength]
#define t_set_right_margin(t)		(t)->strs[TICODE_smgr]
#define t_set_right_margin_parm(t)	(t)->strs[TICODE_smgrp]
#define t_set_tab(t)			(t)->strs[TICODE_hts]
#define t_set_tb_margin(t)		(t)->strs[TICODE_smgtb]
#define t_set_top_margin(t)		(t)->strs[TICODE_smgt]
#define t_set_top_margin_parm(t)	(t)->strs[TICODE_smgtp]
#define t_set_window(t)			(t)->strs[TICODE_wind]
#define t_start_bit_image(t)		(t)->strs[TICODE_sbim]
#define t_start_char_set_def(t)		(t)->strs[TICODE_scsd]
#define t_stop_bit_image(t)		(t)->strs[TICODE_rbim]
#define t_stop_char_set_def(t)		(t)->strs[TICODE_rcsd]
#define t_subscript_characters(t)	(t)->strs[TICODE_subcs]
#define t_superscript_characters(t)	(t)->strs[TICODE_supcs]
#define t_tab(t)			(t)->strs[TICODE_ht]
#define t_these_cause_cr(t)		(t)->strs[TICODE_docr]
#define t_to_status_line(t)		(t)->strs[TICODE_tsl]
#define t_tone(t)			(t)->strs[TICODE_tone]
#define t_user0(t)			(t)->strs[TICODE_u0]
#define t_user1(t)			(t)->strs[TICODE_u1]
#define t_user2(t)			(t)->strs[TICODE_u2]
#define t_user3(t)			(t)->strs[TICODE_u3]
#define t_user4(t)			(t)->strs[TICODE_u4]
#define t_user5(t)			(t)->strs[TICODE_u5]
#define t_user6(t)			(t)->strs[TICODE_u6]
#define t_user7(t)			(t)->strs[TICODE_u7]
#define t_user8(t)			(t)->strs[TICODE_u8]
#define t_user9(t)			(t)->strs[TICODE_u9]
#define t_underline_char(t)		(t)->strs[TICODE_uc]
#define t_up_half_line(t)		(t)->strs[TICODE_hu]
#define t_wait_tone(t)			(t)->strs[TICODE_wait]
#define t_xoff_character(t)		(t)->strs[TICODE_xoffc]
#define t_xon_character(t)		(t)->strs[TICODE_xonc]
#define t_zero_motion(t)		(t)->strs[TICODE_zerom]

#define acs_chars			 t_acs_chars(cur_term)
#define alt_scancode_esc		 t_alt_scancode_esc(cur_term)
#define back_tab			 t_back_tab(cur_term)
#define bell				 t_bell(cur_term)
#define bit_image_carriage_return	 t_bit_image_carriage_return(cur_term)
#define bit_image_newline		 t_bit_image_newline(cur_term)
#define bit_image_repeat		 t_bit_image_repeat(cur_term)
#define carriage_return			 t_carriage_return(cur_term)
#define change_char_pitch		 t_change_char_pitch(cur_term)
#define change_line_pitch		 t_change_line_pitch(cur_term)
#define change_res_horz			 t_change_res_horz(cur_term)
#define change_res_vert			 t_change_res_vert(cur_term)
#define change_scroll_region		 t_change_scroll_region(cur_term)
#define char_padding			 t_char_padding(cur_term)
#define char_set_names			 t_char_set_names(cur_term)
#define clear_all_tabs			 t_clear_all_tabs(cur_term)
#define clear_margins			 t_clear_margins(cur_term)
#define clear_screen			 t_clear_screen(cur_term)
#define clr_bol				 t_clr_bol(cur_term)
#define clr_eol				 t_clr_eol(cur_term)
#define clr_eos				 t_clr_eos(cur_term)
#define code_set_init			 t_code_set_init(cur_term)
#define color_names			 t_color_names(cur_term)
#define column_address			 t_column_address(cur_term)
#define command_character		 t_command_character(cur_term)
#define create_window			 t_create_window(cur_term)
#define cursor_address			 t_cursor_address(cur_term)
#define cursor_down			 t_cursor_down(cur_term)
#define cursor_home			 t_cursor_home(cur_term)
#define cursor_invisible		 t_cursor_invisible(cur_term)
#define cursor_left			 t_cursor_left(cur_term)
#define cursor_mem_address		 t_cursor_mem_address(cur_term)
#define cursor_normal			 t_cursor_normal(cur_term)
#define cursor_right			 t_cursor_right(cur_term)
#define cursor_to_ll			 t_cursor_to_ll(cur_term)
#define cursor_up			 t_cursor_up(cur_term)
#define cursor_visible			 t_cursor_visible(cur_term)
#define define_bit_image_region		 t_define_bit_image_region(cur_term)
#define define_char			 t_define_char(cur_term)
#define delete_character		 t_delete_character(cur_term)
#define delete_line			 t_delete_line(cur_term)
#define device_type			 t_device_type(cur_term)
#define dial_phone			 t_dial_phone(cur_term)
#define dis_status_line			 t_dis_status_line(cur_term)
#define display_clock			 t_display_clock(cur_term)
#define display_pc_char			 t_display_pc_char(cur_term)
#define down_half_time			 t_down_half_time(cur_term)
#define ena_acs				 t_ena_acs(cur_term)
#define end_bit_image_region		 t_end_bit_image_region(cur_term)
#define enter_alt_charset_mode		 t_enter_alt_charset_mode(cur_term)
#define enter_am_mode			 t_enter_am_mode(cur_term)
#define enter_blink_mode		 t_enter_blink_mode(cur_term)
#define enter_bold_mode			 t_enter_bold_mode(cur_term)
#define enter_ca_mode			 t_enter_ca_mode(cur_term)
#define enter_delete_mode		 t_enter_delete_mode(cur_term)
#define enter_dim_mode			 t_enter_dim_mode(cur_term)
#define enter_doublewide_mode		 t_enter_doublewide_mode(cur_term)
#define enter_draft_quality		 t_enter_draft_quality(cur_term)
#define enter_horizontal_hl_mode	 t_enter_horizontal_hl_mode(cur_term)
#define enter_insert_mode		 t_enter_insert_mode(cur_term)
#define enter_italics_mode		 t_enter_italics_mode(cur_term)
#define enter_left_hl_mode		 t_enter_left_hl_mode(cur_term)
#define enter_leftward_mode		 t_enter_leftward_mode(cur_term)
#define enter_low_hl_mode		 t_enter_low_hl_mode(cur_term)
#define enter_micro_mode		 t_enter_micro_mode(cur_term)
#define enter_near_quality_letter	 t_enter_near_quality_letter(cur_term)
#define enter_normal_quality		 t_enter_normal_quality(cur_term)
#define enter_pc_charset_mode		 t_enter_pc_charset_mode(cur_term)
#define enter_protected_mode		 t_enter_protected_mode(cur_term)
#define enter_reverse_mode		 t_enter_reverse_mode(cur_term)
#define enter_right_hl_mode		 t_enter_right_hl_mode(cur_term)
#define enter_scancode_mode		 t_enter_scancode_mode(cur_term)
#define enter_secure_mode		 t_enter_secure_mode(cur_term)
#define enter_shadow_mode		 t_enter_shadow_mode(cur_term)
#define enter_standout_mode		 t_enter_standout_mode(cur_term)
#define enter_subscript_mode		 t_enter_subscript_mode(cur_term)
#define enter_superscript_mode		 t_enter_superscript_mode(cur_term)
#define enter_top_hl_mode		 t_enter_top_hl_mode(cur_term)
#define enter_underline_mode		 t_enter_underline_mode(cur_term)
#define enter_upward_mode		 t_enter_upward_mode(cur_term)
#define enter_vertical_hl_mode		 t_enter_vertical_hl_mode(cur_term)
#define enter_xon_mode			 t_enter_xon_mode(cur_term)
#define erase_chars			 t_erase_chars(cur_term)
#define exit_alt_charset_mode		 t_exit_alt_charset_mode(cur_term)
#define exit_am_mode			 t_exit_am_mode(cur_term)
#define exit_attribute_mode		 t_exit_attribute_mode(cur_term)
#define exit_ca_mode			 t_exit_ca_mode(cur_term)
#define exit_delete_mode		 t_exit_delete_mode(cur_term)
#define exit_doublewide_mode		 t_exit_doublewide_mode(cur_term)
#define exit_insert_mode		 t_exit_insert_mode(cur_term)
#define exit_italics_mode		 t_exit_italics_mode(cur_term)
#define exit_leftward_mode		 t_exit_leftward_mode(cur_term)
#define exit_micro_mode			 t_exit_micro_mode(cur_term)
#define exit_pc_charset_mode		 t_exit_pc_charset_mode(cur_term)
#define exit_scancode_mode		 t_exit_scancode_mode(cur_term)
#define exit_shadow_mode		 t_exit_shadow_mode(cur_term)
#define exit_standout_mode		 t_exit_standout_mode(cur_term)
#define exit_subscript_mode		 t_exit_subscript_mode(cur_term)
#define exit_superscript_mode		 t_exit_superscript_mode(cur_term)
#define exit_underline_mode		 t_exit_underline_mode(cur_term)
#define exit_upward_mode		 t_exit_upward_mode(cur_term)
#define exit_xon_mode			 t_exit_xon_mode(cur_term)
#define fixed_pause			 t_fixed_pause(cur_term)
#define flash_hook			 t_flash_hook(cur_term)
#define flash_screen			 t_flash_screen(cur_term)
#define form_feed			 t_form_feed(cur_term)
#define from_status_line		 t_from_status_line(cur_term)
#define get_mouse			 t_get_mouse(cur_term)
#define goto_window			 t_goto_window(cur_term)
#define hangup				 t_hangup(cur_term)
#define init_1string			 t_init_1string(cur_term)
#define init_2string			 t_init_2string(cur_term)
#define init_3string			 t_init_3string(cur_term)
#define init_file			 t_init_file(cur_term)
#define init_prog			 t_init_prog(cur_term)
#define initialize_color		 t_initialize_color(cur_term)
#define initialize_pair			 t_initialize_pair(cur_term)
#define insert_character		 t_insert_character(cur_term)
#define insert_line			 t_insert_line(cur_term)
#define insert_padding			 t_insert_padding(cur_term)
#define key_a1				 t_key_a1(cur_term)
#define key_a3				 t_key_a3(cur_term)
#define key_b2				 t_key_b2(cur_term)
#define key_backspace			 t_key_backspace(cur_term)
#define key_beg				 t_key_beg(cur_term)
#define key_btab			 t_key_btab(cur_term)
#define key_c1				 t_key_c1(cur_term)
#define key_c3				 t_key_c3(cur_term)
#define key_cancel			 t_key_cancel(cur_term)
#define key_catab			 t_key_catab(cur_term)
#define key_clear			 t_key_clear(cur_term)
#define key_close			 t_key_close(cur_term)
#define key_command			 t_key_command(cur_term)
#define key_copy			 t_key_copy(cur_term)
#define key_create			 t_key_create(cur_term)
#define key_ctab			 t_key_ctab(cur_term)
#define key_dc				 t_key_dc(cur_term)
#define key_dl				 t_key_dl(cur_term)
#define key_down			 t_key_down(cur_term)
#define key_eic				 t_key_eic(cur_term)
#define key_end				 t_key_end(cur_term)
#define key_enter			 t_key_enter(cur_term)
#define key_eol				 t_key_eol(cur_term)
#define key_eos				 t_key_eos(cur_term)
#define key_exit			 t_key_exit(cur_term)
#define key_f0				 t_key_f0(cur_term)
#define key_f1				 t_key_f1(cur_term)
#define key_f2				 t_key_f2(cur_term)
#define key_f3				 t_key_f3(cur_term)
#define key_f4				 t_key_f4(cur_term)
#define key_f5				 t_key_f5(cur_term)
#define key_f6				 t_key_f6(cur_term)
#define key_f7				 t_key_f7(cur_term)
#define key_f8				 t_key_f8(cur_term)
#define key_f9				 t_key_f9(cur_term)
#define key_f10				 t_key_f10(cur_term)
#define key_f11				 t_key_f11(cur_term)
#define key_f12				 t_key_f12(cur_term)
#define key_f13				 t_key_f13(cur_term)
#define key_f14				 t_key_f14(cur_term)
#define key_f15				 t_key_f15(cur_term)
#define key_f16				 t_key_f16(cur_term)
#define key_f17				 t_key_f17(cur_term)
#define key_f18				 t_key_f18(cur_term)
#define key_f19				 t_key_f19(cur_term)
#define key_f20				 t_key_f20(cur_term)
#define key_f21				 t_key_f21(cur_term)
#define key_f22				 t_key_f22(cur_term)
#define key_f23				 t_key_f23(cur_term)
#define key_f24				 t_key_f24(cur_term)
#define key_f25				 t_key_f25(cur_term)
#define key_f26				 t_key_f26(cur_term)
#define key_f27				 t_key_f27(cur_term)
#define key_f28				 t_key_f28(cur_term)
#define key_f29				 t_key_f29(cur_term)
#define key_f30				 t_key_f30(cur_term)
#define key_f31				 t_key_f31(cur_term)
#define key_f32				 t_key_f32(cur_term)
#define key_f33				 t_key_f33(cur_term)
#define key_f34				 t_key_f34(cur_term)
#define key_f35				 t_key_f35(cur_term)
#define key_f36				 t_key_f36(cur_term)
#define key_f37				 t_key_f37(cur_term)
#define key_f38				 t_key_f38(cur_term)
#define key_f39				 t_key_f39(cur_term)
#define key_f40				 t_key_f40(cur_term)
#define key_f41				 t_key_f41(cur_term)
#define key_f42				 t_key_f42(cur_term)
#define key_f43				 t_key_f43(cur_term)
#define key_f44				 t_key_f44(cur_term)
#define key_f45				 t_key_f45(cur_term)
#define key_f46				 t_key_f46(cur_term)
#define key_f47				 t_key_f47(cur_term)
#define key_f48				 t_key_f48(cur_term)
#define key_f49				 t_key_f49(cur_term)
#define key_f50				 t_key_f50(cur_term)
#define key_f51				 t_key_f51(cur_term)
#define key_f52				 t_key_f52(cur_term)
#define key_f53				 t_key_f53(cur_term)
#define key_f54				 t_key_f54(cur_term)
#define key_f55				 t_key_f55(cur_term)
#define key_f56				 t_key_f56(cur_term)
#define key_f57				 t_key_f57(cur_term)
#define key_f58				 t_key_f58(cur_term)
#define key_f59				 t_key_f59(cur_term)
#define key_f60				 t_key_f60(cur_term)
#define key_f61				 t_key_f61(cur_term)
#define key_f62				 t_key_f62(cur_term)
#define key_f63				 t_key_f63(cur_term)
#define key_find			 t_key_find(cur_term)
#define key_help			 t_key_help(cur_term)
#define key_home			 t_key_home(cur_term)
#define key_ic				 t_key_ic(cur_term)
#define key_il				 t_key_il(cur_term)
#define key_left			 t_key_left(cur_term)
#define key_ll				 t_key_ll(cur_term)
#define key_mark			 t_key_mark(cur_term)
#define key_message			 t_key_message(cur_term)
#define key_mouse			 t_key_mouse(cur_term)
#define key_move			 t_key_move(cur_term)
#define key_next			 t_key_next(cur_term)
#define key_npage			 t_key_npage(cur_term)
#define key_open			 t_key_open(cur_term)
#define key_options			 t_key_options(cur_term)
#define key_ppage			 t_key_ppage(cur_term)
#define key_previous			 t_key_previous(cur_term)
#define key_print			 t_key_print(cur_term)
#define key_redo			 t_key_redo(cur_term)
#define key_reference			 t_key_reference(cur_term)
#define key_refresh			 t_key_refresh(cur_term)
#define key_replace			 t_key_replace(cur_term)
#define key_restart			 t_key_restart(cur_term)
#define key_resume			 t_key_resume(cur_term)
#define key_right			 t_key_right(cur_term)
#define key_save			 t_key_save(cur_term)
#define key_sbeg			 t_key_sbeg(cur_term)
#define key_scancel			 t_key_scancel(cur_term)
#define key_scommand			 t_key_scommand(cur_term)
#define key_scopy			 t_key_scopy(cur_term)
#define key_screate			 t_key_screate(cur_term)
#define key_sdc				 t_key_sdc(cur_term)
#define key_sdl				 t_key_sdl(cur_term)
#define key_select			 t_key_select(cur_term)
#define key_send			 t_key_send(cur_term)
#define key_seol			 t_key_seol(cur_term)
#define key_sexit			 t_key_sexit(cur_term)
#define key_sf				 t_key_sf(cur_term)
#define key_sfind			 t_key_sfind(cur_term)
#define key_shelp			 t_key_shelp(cur_term)
#define key_shome			 t_key_shome(cur_term)
#define key_sic				 t_key_sic(cur_term)
#define key_sleft			 t_key_sleft(cur_term)
#define key_smessage			 t_key_smessage(cur_term)
#define key_smove			 t_key_smove(cur_term)
#define key_snext			 t_key_snext(cur_term)
#define key_soptions			 t_key_soptions(cur_term)
#define key_sprevious			 t_key_sprevious(cur_term)
#define key_sprint			 t_key_sprint(cur_term)
#define key_sr				 t_key_sr(cur_term)
#define key_sredo			 t_key_sredo(cur_term)
#define key_sreplace			 t_key_sreplace(cur_term)
#define key_sright			 t_key_sright(cur_term)
#define key_srsume			 t_key_srsume(cur_term)
#define key_ssave			 t_key_ssave(cur_term)
#define key_ssuspend			 t_key_ssuspend(cur_term)
#define key_stab			 t_key_stab(cur_term)
#define key_sundo			 t_key_sundo(cur_term)
#define key_suspend			 t_key_suspend(cur_term)
#define key_undo			 t_key_undo(cur_term)
#define key_up				 t_key_up(cur_term)
#define keypad_local			 t_keypad_local(cur_term)
#define keypad_xmit			 t_keypad_xmit(cur_term)
#define lab_f0				 t_lab_f0(cur_term)
#define lab_f1				 t_lab_f1(cur_term)
#define lab_f2				 t_lab_f2(cur_term)
#define lab_f3				 t_lab_f3(cur_term)
#define lab_f4				 t_lab_f4(cur_term)
#define lab_f5				 t_lab_f5(cur_term)
#define lab_f6				 t_lab_f6(cur_term)
#define lab_f7				 t_lab_f7(cur_term)
#define lab_f8				 t_lab_f8(cur_term)
#define lab_f9				 t_lab_f9(cur_term)
#define lab_f10				 t_lab_f10(cur_term)
#define label_format			 t_label_format(cur_term)
#define label_off			 t_label_off(cur_term)
#define label_on			 t_label_on(cur_term)
#define meta_off			 t_meta_off(cur_term)
#define meta_on				 t_meta_on(cur_term)
#define micro_column_address		 t_micro_column_address(cur_term)
#define micro_down			 t_micro_down(cur_term)
#define micro_left			 t_micro_left(cur_term)
#define micro_right			 t_micro_right(cur_term)
#define micro_row_address		 t_micro_row_address(cur_term)
#define micro_up			 t_micro_up(cur_term)
#define mouse_info			 t_mouse_info(cur_term)
#define newline				 t_newline(cur_term)
#define order_of_pins			 t_order_of_pins(cur_term)
#define orig_colors			 t_orig_colors(cur_term)
#define orig_pair			 t_orig_pair(cur_term)
#define pad_char			 t_pad_char(cur_term)
#define parm_dch			 t_parm_dch(cur_term)
#define parm_delete_line		 t_parm_delete_line(cur_term)
#define parm_down_cursor		 t_parm_down_cursor(cur_term)
#define parm_down_micro			 t_parm_down_micro(cur_term)
#define parm_ich			 t_parm_ich(cur_term)
#define parm_index			 t_parm_index(cur_term)
#define parm_insert_line		 t_parm_insert_line(cur_term)
#define parm_left_cursor		 t_parm_left_cursor(cur_term)
#define parm_left_micro			 t_parm_left_micro(cur_term)
#define parm_right_cursor		 t_parm_right_cursor(cur_term)
#define parm_right_micro		 t_parm_right_micro(cur_term)
#define parm_rindex			 t_parm_rindex(cur_term)
#define parm_up_cursor			 t_parm_up_cursor(cur_term)
#define parm_up_micro			 t_parm_up_micro(cur_term)
#define pc_term_options			 t_pc_term_options(cur_term)
#define pkey_key			 t_pkey_key(cur_term)
#define pkey_local			 t_pkey_local(cur_term)
#define pkey_plab			 t_pkey_plab(cur_term)
#define pkey_xmit			 t_pkey_xmit(cur_term)
#define pkey_norm			 t_pkey_norm(cur_term)
#define print_screen			 t_print_screen(cur_term)
#define ptr_non				 t_ptr_non(cur_term)
#define ptr_off				 t_ptr_off(cur_term)
#define ptr_on				 t_ptr_on(cur_term)
#define pulse				 t_pulse(cur_term)
#define quick_dial			 t_quick_dial(cur_term)
#define remove_clock			 t_remove_clock(cur_term)
#define repeat_char			 t_repeat_char(cur_term)
#define req_for_input			 t_req_for_input(cur_term)
#define req_mouse_pos			 t_req_mouse_pos(cur_term)
#define reset_1string			 t_reset_1string(cur_term)
#define reset_2string			 t_reset_2string(cur_term)
#define reset_3string			 t_reset_3string(cur_term)
#define reset_file			 t_reset_file(cur_term)
#define restore_cursor			 t_restore_cursor(cur_term)
#define row_address			 t_row_address(cur_term)
#define save_cursor			 t_save_cursor(cur_term)
#define scancode_escape			 t_scancode_escape(cur_term)
#define scroll_forward			 t_scroll_forward(cur_term)
#define scroll_reverse			 t_scroll_reverse(cur_term)
#define select_char_set			 t_select_char_set(cur_term)
#define set0_des_seq			 t_set0_des_seq(cur_term)
#define set1_des_seq			 t_set1_des_seq(cur_term)
#define set2_des_seq			 t_set2_des_seq(cur_term)
#define set3_des_seq			 t_set3_des_seq(cur_term)
#define set_a_attributes		 t_set_a_attributes(cur_term)
#define set_a_background		 t_set_a_background(cur_term)
#define set_a_foreground		 t_set_a_foreground(cur_term)
#define set_attributes			 t_set_attributes(cur_term)
#define set_background			 t_set_background(cur_term)
#define set_bottom_margin		 t_set_bottom_margin(cur_term)
#define set_bottom_margin_parm		 t_set_bottom_margin_parm(cur_term)
#define set_clock			 t_set_clock(cur_term)
#define set_color_band			 t_set_color_band(cur_term)
#define set_color_pair			 t_set_color_pair(cur_term)
#define set_foreground			 t_set_foreground(cur_term)
#define set_left_margin			 t_set_left_margin(cur_term)
#define set_left_margin_parm		 t_set_left_margin_parm(cur_term)
#define set_lr_margin			 t_set_lr_margin(cur_term)
#define set_page_length			 t_set_page_length(cur_term)
#define set_pglen_inch			 t_set_pglen_inch(cur_term)
#define set_right_margin		 t_set_right_margin(cur_term)
#define set_right_margin_parm		 t_set_right_margin_parm(cur_term)
#define set_tab				 t_set_tab(cur_term)
#define set_tb_margin			 t_set_tb_margin(cur_term)
#define set_top_margin			 t_set_top_margin(cur_term)
#define set_top_margin_parm		 t_set_top_margin_parm(cur_term)
#define set_window			 t_set_window(cur_term)
#define start_bit_image			 t_start_bit_image(cur_term)
#define start_char_set_def		 t_start_char_set_def(cur_term)
#define stop_bit_image			 t_stop_bit_image(cur_term)
#define stop_char_set_def		 t_stop_char_set_def(cur_term)
#define subscript_characters		 t_subscript_characters(cur_term)
#define superscript_characters		 t_superscript_characters(cur_term)
#define tab				 t_tab(cur_term)
#define these_cause_cr			 t_these_cause_cr(cur_term)
#define to_status_line			 t_to_status_line(cur_term)
#define tone				 t_tone(cur_term)
#define user0				 t_user0(cur_term)
#define user1				 t_user1(cur_term)
#define user2				 t_user2(cur_term)
#define user3				 t_user3(cur_term)
#define user4				 t_user4(cur_term)
#define user5				 t_user5(cur_term)
#define user6				 t_user6(cur_term)
#define user7				 t_user7(cur_term)
#define user8				 t_user8(cur_term)
#define user9				 t_user9(cur_term)
#define underline_char			 t_underline_char(cur_term)
#define up_half_line			 t_up_half_line(cur_term)
#define wait_tone			 t_wait_tone(cur_term)
#define xoff_character			 t_xoff_character(cur_term)
#define xon_character			 t_xon_character(cur_term)
#define zero_motion			 t_zero_motion(cur_term)

/*
 * STRING DESCRIPTIONS
 *
 * acs_chars: Graphic charset pairs aAbBcC
 * alt_scancode_esc: Alternate escape for scancode emulation
 * back_tab: Back tab
 * bell: Audible signal (bell)
 * bit_image_carriage_return: Move to beginning of same row
 * bit_image_newline: Move to next row of the bit image
 * bit_image_repeat: Repeat bit-image cell #1 #2 times
 * carriage_return: Carriage return
 * change_char_pitch: Change number of characters per inch
 * change_line_pitch: Change number of lines per inch
 * change_res_horz: Change horizontal resolution
 * change_res_vert: Change vertical resolution
 * change_scroll_region: Change to lines #1 through #2 (VT100)
 * char_padding: Like ip but when in replace mode
 * char_set_names: Returns a list of character set names
 * clear_all_tabs: Clear all tab stops
 * clear_margins: Clear all margins (top, bottom and sides)
 * clear_screen: Clear screen and home cursor
 * clr_bol: Clear to beginning of line, inclusive
 * clr_eol: Clear to end of line
 * clr_eos: Clear to end of display
 * code_set_init: Init sequence for multiple codesets
 * color_names: Give name for colour #1
 * column_address: Set horizontal position to absolute #1
 * command_character: Terminal settable cmd character in prototype
 * create_window: Define win #1 to go from #2,#3 to #4,#5
 * cursor_address: Move to row #1, col #2
 * cursor_down: Down one line
 * cursor_home: Home cursor (if no cup)
 * cursor_invisible: Make cursor invisible
 * cursor_left: Move left one space
 * cursor_mem_address: Memory relative cursor addressing
 * cursor_normal: Make cursor appear normal (under vs/vi)
 * cursor_right: Non-destructive space (cursor or carriage right)
 * cursor_to_ll: Last line, first column (if no cup)
 * cursor_up: Upline (cursor up)
 * cursor_visible: Make cursor very visible
 * define_bit_image_region: Define rectangular bit-image region
 * define_char: Define a character in a character set
 * delete_character: Delete character
 * delete_line: Delete line
 * device_type: Indicate language/codeset support
 * dial_phone: Dial phone number #1
 * dis_status_line: Disable status line
 * display_clock: Display time-of-day clock
 * display_pc_char: Display PC character
 * down_half_time: Half-line down (forward 1/2 linefeed)
 * ena_acs: Enable alternate character set
 * end_bit_image_region: End a bit-image region
 * enter_alt_charset_mode: Start alternate character set
 * enter_am_mode: Turn on automatic margins
 * enter_blink_mode: Turn on blinking
 * enter_bold_mode: Turn on bold (extra bright) mode
 * enter_ca_mode: String to begin programs that use cup
 * enter_delete_mode: Delete mode (enter)
 * enter_dim_mode: Turn on half-bright mode
 * enter_doublewide_mode: Enable double wide printing
 * enter_draft_quality: Set draft qualify print
 * enter_horizontal_hl_mode: Turn on horizontal highlight mode
 * enter_insert_mode: Insert mode (enter)
 * enter_italics_mode: Enable italics
 * enter_left_hl_mode: Turn on left highlight mode
 * enter_leftward_mode: Enable leftward carriage motion
 * enter_low_hl_mode: Turn on low highlight mode
 * enter_micro_mode: Enable micro motion capabilities
 * enter_near_quality_letter: Set near-letter quality print
 * enter_normal_quality: Set normal quality print
 * enter_pc_charset_mode: Enter PC character display mode
 * enter_protected_mode: Turn on protected mode
 * enter_reverse_mode: Turn on reverse video mode
 * enter_right_hl_mode: Turn on right highlight mode
 * enter_scancode_mode: Enter PC scancode mode
 * enter_secure_mode: Turn on blank mode (characters invisible)
 * enter_shadow_mode: Enable shadow printing
 * enter_standout_mode: Begin standout mode
 * enter_subscript_mode: Enable subscript printing
 * enter_superscript_mode: Enable superscript printing
 * enter_top_hl_mode: Turn on top highlight mode
 * enter_underline_mode: Start underscore mode
 * enter_upward_mode: Enable upward carriage motion
 * enter_vertical_hl_mode: Turn on verticle highlight mode
 * enter_xon_mode: Turn on xon/xoff handshaking
 * erase_chars: Erase #1 characters
 * exit_alt_charset_mode: End alternate character set
 * exit_am_mode: Turn off automatic margins
 * exit_attribute_mode: Turn off all attributes
 * exit_ca_mode: String to end programs that use cup
 * exit_delete_mode: End delete mode
 * exit_doublewide_mode: Disable double wide printing
 * exit_insert_mode: End insert mode
 * exit_italics_mode: Disable italics
 * exit_leftward_mode: Enable rightward (normal) carriage motion
 * exit_micro_mode: Disable micro motion capabilities
 * exit_pc_charset_mode: Disable PC character display mode
 * exit_scancode_mode: Disable PC scancode mode
 * exit_shadow_mode: Disable shadow printing
 * exit_standout_mode: End standout mode
 * exit_subscript_mode: Disable subscript printing
 * exit_superscript_mode: Disable superscript printing
 * exit_underline_mode: End underscore mode
 * exit_upward_mode: Enable downward (normal) carriage motion
 * exit_xon_mode: Turn off xon/xoff handshaking
 * fixed_pause: Pause for 2-3 seconds
 * flash_hook: Flash the switch hook
 * flash_screen: Visible bell (may move cursor)
 * form_feed: Hardcopy terminal eject page
 * from_status_line: Return from status line
 * get_mouse: Curses should get button events
 * goto_window: Go to window #1
 * hangup: Hang-up phone
 * init_1string: Terminal or printer initialisation string
 * init_2string: Terminal or printer initialisation string
 * init_3string: Terminal or printer initialisation string
 * init_file: Name of initialisation file
 * init_prog: Path name of program for initialisation
 * initialize_color: Set colour #1 to RGB #2, #3, #4
 * initialize_pair: Set colour-pair #1 to fg #2, bg #3
 * insert_character: Insert character
 * insert_line: Add new blank line
 * insert_padding: Insert pad after character inserted
 * key_a1: upper left of keypad
 * key_a3: upper right of keypad
 * key_b2: center of keypad
 * key_backspace: set by backspace key
 * key_beg: 1
 * key_btab: sent by back-tab key
 * key_c1: lower left of keypad
 * key_c3: lower right of keypad
 * key_cancel: 2
 * key_catab: sent by clear-all-tabs key
 * key_clear: sent by clear-screen or erase key
 * key_close: 3
 * key_command: 4
 * key_copy: 5
 * key_create: 6
 * key_ctab: sent by clear-tab key
 * key_dc: sent by delete-character key
 * key_dl: sent by delete-line key
 * key_down: sent by terminal down-arrow key
 * key_eic: sent by rmir or smir in insert mode
 * key_end: 7
 * key_enter: 8
 * key_eol: sent by clear-to-end-of-line key
 * key_eos: sent by clear-to-end-of-screen key
 * key_exit: 9
 * key_f0: sent by function key f0
 * key_f1: sent by function key f1
 * key_f2: sent by function key f2
 * key_f3: sent by function key f3
 * key_f4: sent by function key f4
 * key_f5: sent by function key f5
 * key_f6: sent by function key f6
 * key_f7: sent by function key f7
 * key_f8: sent by function key f8
 * key_f9: sent by function key f9
 * key_f10: sent by function key f10
 * key_f11: sent by function key f11
 * key_f12: sent by function key f12
 * key_f13: sent by function key f13
 * key_f14: sent by function key f14
 * key_f15: sent by function key f15
 * key_f16: sent by function key f16
 * key_f17: sent by function key f17
 * key_f18: sent by function key f18
 * key_f19: sent by function key f19
 * key_f20: sent by function key f20
 * key_f21: sent by function key f21
 * key_f22: sent by function key f22
 * key_f23: sent by function key f23
 * key_f24: sent by function key f24
 * key_f25: sent by function key f25
 * key_f26: sent by function key f26
 * key_f27: sent by function key f27
 * key_f28: sent by function key f28
 * key_f29: sent by function key f29
 * key_f30: sent by function key f30
 * key_f31: sent by function key f31
 * key_f32: sent by function key f32
 * key_f33: sent by function key f33
 * key_f34: sent by function key f34
 * key_f35: sent by function key f35
 * key_f36: sent by function key f36
 * key_f37: sent by function key f37
 * key_f38: sent by function key f38
 * key_f39: sent by function key f39
 * key_f40: sent by function key f40
 * key_f41: sent by function key f41
 * key_f42: sent by function key f42
 * key_f43: sent by function key f43
 * key_f44: sent by function key f44
 * key_f45: sent by function key f45
 * key_f46: sent by function key f46
 * key_f47: sent by function key f47
 * key_f48: sent by function key f48
 * key_f49: sent by function key f49
 * key_f50: sent by function key f50
 * key_f51: sent by function key f51
 * key_f52: sent by function key f52
 * key_f53: sent by function key f53
 * key_f54: sent by function key f54
 * key_f55: sent by function key f55
 * key_f56: sent by function key f56
 * key_f57: sent by function key f57
 * key_f58: sent by function key f58
 * key_f59: sent by function key f59
 * key_f60: sent by function key f60
 * key_f61: sent by function key f61
 * key_f62: sent by function key f62
 * key_f63: sent by function key f63
 * key_find: 0
 * key_help: sent by help key
 * key_home: sent by home key
 * key_ic: sent by ins-char/enter ins-mode key
 * key_il: sent by insert-line key
 * key_left: sent by terminal left-arrow key
 * key_ll: sent by home-down key
 * key_mark: sent by mark key
 * key_message: sent by message key
 * key_mouse: 0631, Mouse event has occured
 * key_move: sent by move key
 * key_next: sent by next-object key
 * key_npage: sent by next-page key
 * key_open: sent by open key
 * key_options: sent by options key
 * key_ppage: sent by previous-page key
 * key_previous: sent by previous-object key
 * key_print: sent by print or copy key
 * key_redo: sent by redo key
 * key_reference: sent by ref(erence) key
 * key_refresh: sent by refresh key
 * key_replace: sent by replace key
 * key_restart: sent by restart key
 * key_resume: sent by resume key
 * key_right: sent by terminal right-arrow key
 * key_save: sent by save key
 * key_sbeg: sent by shifted beginning key
 * key_scancel: sent by shifted cancel key
 * key_scommand: sent by shifted command key
 * key_scopy: sent by shifted copy key
 * key_screate: sent by shifted create key
 * key_sdc: sent by shifted delete-char key
 * key_sdl: sent by shifted delete-line key
 * key_select: sent by select key
 * key_send: sent by shifted end key
 * key_seol: sent by shifted clear-line key
 * key_sexit: sent by shited exit key
 * key_sf: sent by scroll-forward/down key
 * key_sfind: sent by shifted find key
 * key_shelp: sent by shifted help key
 * key_shome: sent by shifted home key
 * key_sic: sent by shifted input key
 * key_sleft: sent by shifted left-arrow key
 * key_smessage: sent by shifted message key
 * key_smove: sent by shifted move key
 * key_snext: sent by shifted next key
 * key_soptions: sent by shifted options key
 * key_sprevious: sent by shifted prev key
 * key_sprint: sent by shifted print key
 * key_sr: sent by scroll-backwards/up key
 * key_sredo: sent by shifted redo key
 * key_sreplace: sent by shifted replace key
 * key_sright: sent by shifted right-arrow key
 * key_srsume: sent by shifted resume key
 * key_ssave: sent by shifted save key
 * key_ssuspend: sent by shifted suspend key
 * key_stab: sent by set-tab key
 * key_sundo: sent by shifted undo key
 * key_suspend: sent by suspend key
 * key_undo: sent by undo key
 * key_up: sent by terminal up-arrow key
 * keypad_local: Out of "keypad-transmit" mode
 * keypad_xmit: Put terminal in "keypad-transmit" mode
 * lab_f0: Labels on function key f0 if not f0
 * lab_f1: Labels on function key f1 if not f1
 * lab_f2: Labels on function key f2 if not f2 
 * lab_f3: Labels on function key f3 if not f3
 * lab_f4: Labels on function key f4 if not f4
 * lab_f5: Labels on function key f5 if not f5
 * lab_f6: Labels on function key f6 if not f6
 * lab_f7: Labels on function key f7 if not f7
 * lab_f8: Labels on function key f8 if not f8
 * lab_f9: Labels on function key f9 if not f9
 * lab_f10: Labels on function key f10 if not f10
 * label_format: Label format
 * label_off: Turn off soft labels
 * label_on: Turn on soft labels
 * meta_off: Turn off "meta mode"
 * meta_on: Turn on "meta mode" (8th bit)
 * micro_column_address: Like column_address for micro adjustment
 * micro_down: Like cursor_down for micro adjustment
 * micro_left: Like cursor_left for micro adjustment
 * micro_right: Like cursor_right for micro adjustment
 * micro_row_address: Like row_address for micro adjustment
 * micro_up: Like cursor_up for micro adjustment
 * mouse_info: Mouse status information
 * newline: Newline (behaves like cr followed by lf)
 * order_of_pins: Matches software bits to print-head pins
 * orig_colors: Set all colour(-pair)s to original ones
 * orig_pair: Set default colour-pair to the original one
 * pad_char: Pad character (rather than NULL)
 * parm_dch: Delete #1 chars
 * parm_delete_line: Delete #1 lines
 * parm_down_cursor: Move down #1 lines
 * parm_down_micro: Like parm_down_cursor for micro adjustment
 * parm_ich: Insert #1 blank chars
 * parm_index: Scroll forward #1 lines
 * parm_insert_line: Add #1 new blank lines
 * parm_left_cursor: Move cursor left #1 lines
 * parm_left_micro: Like parm_left_cursor for micro adjustment
 * parm_right_cursor: Move right #1 spaces
 * parm_right_micro: Like parm_right_cursor for micro adjustment
 * parm_rindex: Scroll backward #1 lines
 * parm_up_cursor: Move cursor up #1 lines
 * parm_up_micro: Like parm_up_cursor for micro adjustment
 * pc_term_options: PC terminal options
 * pkey_key: Prog funct key #1 to type string #2
 * pkey_local: Prog funct key #1 to execute string #2
 * pkey_plab: Prog key #1 to xmit string #2 and show string #3
 * pkey_xmit: Prog funct key #1 to xmit string #2
 * pkey_norm: Prog label #1 to show string #3
 * print_screen: Print contents of screen
 * ptr_non: Turn off printer for #1 bytes
 * ptr_off: Turn off the printer
 * ptr_on: Turn on the printer
 * pulse: Select pulse dialing
 * quick_dial: Dial phone number #1, without progress detection
 * remove_clock: Remove time-of-day clock
 * repeat_char: Repeat char #1 #2 times
 * req_for_input: Send next input char (for ptys)
 * req_mouse_pos: Request mouse position report
 * reset_1string: Reset terminal completely to sane modes
 * reset_2string: Reset terminal completely to sane modes
 * reset_3string: Reset terminal completely to sane modes
 * reset_file: Name of file containing reset string
 * restore_cursor: Restore cursor to position of last sc
 * row_address: Set vertical position to absolute #1
 * save_cursor: Save cursor position
 * scancode_escape: Escape for scancode emulation
 * scroll_forward: Scroll text up
 * scroll_reverse: Scroll text down
 * select_char_set: Select character set
 * set0_des_seq: Shift into codeset 0 (EUC set 0, ASCII)
 * set1_des_seq: Shift into codeset 1
 * set2_des_seq: Shift into codeset 2
 * set3_des_seq: Shift into codeset 3
 * set_a_attributes: Define second set of video attributes #1-#6
 * set_a_background: Set background colour to #1 using ANSI escape
 * set_a_foreground: Set foreground colour to #1 using ANSI escape
 * set_attributes: Define first set of video attributes #1-#9
 * set_background: Set background colour to #1
 * set_bottom_margin: Set bottom margin at current line
 * set_bottom_margin_parm: Set bottom margin at line #1 or #2 lines from bottom
 * set_clock: Set clock to hours (#1), minutes (#2), seconds (#3)
 * set_color_band: Change ribbon to colour #1
 * set_color_pair: Set current colour pair to #1
 * set_foreground: Set foreground colour to #1
 * set_left_margin: Set left margin at current column
 * set_left_margin_parm: Set left (right) margin at column #1 (#2)
 * set_lr_margin: Sets both left and right margins
 * set_page_length: Set page length to #1 lines
 * set_pglen_inch: Set page length to #1 hundredth of an inch
 * set_right_margin: Set right margin at current column
 * set_right_margin_parm: Set right margin at #1
 * set_tab: Set a tab in all rows, current column
 * set_tb_margin: Sets both top and bottom margins
 * set_top_margin: Set top margin at current line
 * set_top_margin_parm: Set top (bottom) margin at line #1 (#2)
 * set_window: Current window is lines #1-#2 cols #3-#4
 * start_bit_image: Start printing bit image graphics
 * start_char_set_def: Start definition of a character set
 * stop_bit_image: End printing bit image graphics
 * stop_char_set_def: End definition of a character set
 * subscript_characters: List of "subscript-able" characters
 * superscript_characters: List of "superscript-able" characters
 * tab: Tab to next 8-space hardware tab stop
 * these_cause_cr: Printing any of these characters causes cr
 * to_status_line: Go to status line, col #1
 * tone: Select tone touch dialing
 * user0: User string 0
 * user1: User string 1
 * user2: User string 2
 * user3: User string 3
 * user4: User string 4
 * user5: User string 5
 * user6: User string 6
 * user7: User string 7
 * user8: User string 8
 * user9: User string 9
 * underline_char: Underscore one char and move past it
 * up_half_line: Half-line up (reverse 1/2 linefeed)
 * wait_tone: Wait for dial tone
 * xoff_character: X-off character
 * xon_character: X-on character
 * zero_motion: No motion for the subsequent character
 */

#ifndef _TERMINFO
typedef struct {
	int fildes;
	/* We need to expose these so that the macros work */
	const char *name;
	const char *desc;
	const signed char *flags;
	const short *nums;
	const char **strs;
} TERMINAL;
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS

extern TERMINAL *cur_term;

/* setup functions */
int		setupterm(const char *, int, int *);
TERMINAL *	set_curterm(TERMINAL *);
int		del_curterm(TERMINAL *);
char *		termname(void);
char *		longname(void);

/* information functions */
int		tigetflag(const char *);
int		tigetnum(const char *);
char *		tigetstr(const char *);
/* You should note that the spec allows stuffing a char * into a long
 * if the platform allows and the %pN is followed immediately by %l or %s */
char *		tparm(const char *, long, long, long, long, long,
		long, long, long, long);

/* Non standard functions, but provide a level of thread safety */
int		ti_setupterm(TERMINAL **, const char *, int, int *);
int		ti_getflag(const TERMINAL *, const char *);
int		ti_getnum(const TERMINAL *, const char *);
const char *	ti_getstr(const TERMINAL *, const char *);
char *		ti_parm(TERMINAL *, const char *,
    long, long, long, long, long, long, long, long, long);

/* These functions do not use PC or speed, but the terminal */
int		ti_puts(const TERMINAL *, const char *, int,
    int (*)(int, void *), void *);
int		ti_putp(const TERMINAL *, const char *);

/* Using tparm can be kunkly, so provide a variadic function
 * Numbers have to be passed as int */
/* This is not standard, but ncurses also provides this */
char *		tiparm(const char *, ...);
/* And a thread safe version */
char *		ti_tiparm(TERMINAL *, const char *, ...);

#ifdef TPARM_TLPARM
/* Same as the above, but numbers have to be passed as long */
char *		tlparm(const char *, ...);
/* And a thread safe version */
char *		ti_tlparm(TERMINAL *, const char *, ...);
#endif

/* Default to X/Open tparm, but allow it to be variadic also */
#ifdef TPARM_VARARGS
#  define tparm	tiparm
#  define ti_parm ti_tiparm
#endif

/* Convert a termcap string into a terminfo string.
 * The passed string is destroyed and the return string needs to be freed. */
char *		captoinfo(char *);

/* POSIX says that term.h should also pull in our termcap definitions. */
#include <termcap.h>

__END_DECLS
#endif

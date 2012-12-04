/* $NetBSD: term.h,v 1.12 2012/05/29 00:27:59 dholland Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
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

/* Using tparm can be kunkly, so provide a variadic function */
/* This is not standard, but ncurses also provides this */
char *		tiparm(const char *, ...);
/* And a thread safe version */
char *		ti_tiparm(TERMINAL *, const char *, ...);

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

;;;
;;; ELLE Master Function Definition file - "deffun.e"
;;;
;;;	This file serves as input to the ellec program.  It defines
;;; all ELLE functions which may serve as keyboard-bound user commands.
;;;
;;; Format: (efun <Index> <Name> <Routine> <Module>)
;;;		Index - an unique index # (used only within ELLE)
;;;		Name - an unique string identifying this function to the user.
;;;		Routine - the C routine implementing the function within ELLE.
;;;		Module - the name of the C source file that the routine is in.
;;;
;;; The following definitions are roughly organized by object.
;;; All functions that emulate EMACS functions are given names identical
;;; to the EMACS function names.  For historical reasons these names
;;; are not as consistent as they could be (sigh).
;;; Those which have no exact counterpart in EMACS are identified by comments.

(undefall)	; Ensure all predefined stuff is cleared out.

; Simple Insertion
(efun   1 "Insert Self"		f_insself	eef1)
(efun   2 "Quoted Insert"	f_quotins	eef1)
(efun   3 "CRLF"		f_crlf		eef1)

; Characters
(efun   4 "Forward Character"	f_fchar		eef1)
(efun   5 "Backward Character"	f_bchar		eef1)
(efun   6 "Delete Character"	f_dchar		eef1)
(efun   7 "Backward Delete Character" f_bdchar	eef1)
(efun   8 "Delete Horizontal Space" f_delspc	eef1)
(efun   9 "Transpose Characters" f_tchars	eef1)

; Words
(efun  10 "Forward Word"	f_fword		eef1)
(efun  11 "Backward Word"	f_bword		eef1)
(efun  12 "Kill Word"		f_kword		eef1)
(efun  13 "Backward Kill Word"	f_bkword	eef1)
(efun  14 "Transpose Words"	f_twords	eef1)
(efun  15 "Uppercase Word"	f_ucword	eef1)
(efun  16 "Lowercase Word"	f_lcword	eef1)
(efun  17 "Uppercase Initial"	f_uciword	eef1)
     ; 18-19 reserved

; Lines
(efun  20 "Beginning of Line"	f_begline	eef2)
(efun  21 "End of Line"		f_endline	eef2)
(efun  22 "Next Line"		f_nxtline	eef2)
(efun  23 "Previous Line"	f_prvline	eef2)
(efun  24 "Down Real Line"	f_dnrline	eef2)
(efun  25 "Up Real Line"	f_uprline	eef2)
(efun  26 "Open Line"		f_oline		eef2)
(efun  27 "Delete Blank Lines"	f_delblines	eef2)
(efun  28 "Kill Line"		f_kline		eef2)
(efun  29 "Backward Kill Line"	f_bkline	eef2)	; not EMACS
(efun  30 "Goto Line"		f_goline	eef2)	; not EMACS
     ; 31-34 reserved

; Regions
(efun  35 "Set/Pop Mark"	f_setmark	eef2)
(efun  36 "Exchange Point and Mark" f_exchmark	eef2)
(efun  37 "Kill Region"		f_kregion	eef2)
(efun  38 "Copy Region"		f_copreg	eef2)
(efun  39 "Uppercase Region"	f_ucreg		eef2)
(efun  40 "Lowercase Region"	f_lcreg		eef2)
(efun  41 "Fill Region"		f_fillreg	eef2)
     ; 42-44 reserved

; Paragraphs
(efun  45 "Forward Paragraph"	f_fpara		eef2)
(efun  46 "Backward Paragraph"	f_bpara		eef2)
(efun  47 "Mark Paragraph"	f_mrkpara	eef2)
(efun  48 "Fill Paragraph"	f_fillpara	eef2)
     ; 49 reserved

; Buffers
(efun  50 "Select Buffer"	f_selbuffer	eebuff)
(efun  51 "Select Existing Buffer" f_selxbuffer	eebuff)	; not EMACS
(efun  52 "Kill Buffer"		f_kbuffer	eebuff)
(efun  53 "List Buffers"	f_listbufs	eebuff)
(efun  54 "Buffer Not Modified"	f_bufnotmod	eebuff)
(efun  55 "EOL CRLF Mode"	f_eolmode	eebuff)	; ELLE
(efun  56 "Goto Beginning"	f_gobeg		eebuff)
(efun  57 "Goto End"		f_goend		eebuff)
(efun  58 "What Page"		f_whatpage	eebuff)
     ; 59 reserved

; Files
(efun  60 "Find File"		f_ffile		eefile)
(efun  61 "Read File"		f_rfile		eefile)
(efun  62 "Visit File"		f_vfile		eefile)
(efun  63 "Insert File"		f_ifile		eefile)
(efun  64 "Save File"		f_sfile		eefile)
(efun  65 "Save All Files"	f_savefiles	eebuff)
(efun  66 "Write File"		f_wfile		eefile)
(efun  67 "Write Region"	f_wreg		eefile)
(efun  68 "Write Last Kill"	f_wlastkill	eefile)	; not EMACS
     ; 69 reserved

; Windows
(efun  70 "Two Windows"		f_2winds	eebuff)
(efun  71 "One Window"		f_1wind		eebuff)
(efun  72 "Other Window"	f_othwind	eebuff)
(efun  73 "Grow Window"		f_growind	eebuff)
(efun  74 "Shrink Window"	f_shrinkwind	eebuff)	; not EMACS	
(efun  75 "Delete Window"	f_delwind	eebuff)	; not EMACS
(efun  76 "Standout Window"	f_sowind	eebuff)	; ELLE
(efun  77 "Two Mode Windows"	f_2modewinds	eebuff)	; ELLE

; Window Positioning
(efun  78 "New Window"		f_newwin	eefd)
(efun  79 "Next Screen"		f_nscreen	eefd)
(efun  80 "Previous Screen"	f_pscreen	eefd)
(efun  81 "Other New Screen"	f_othnscreen	eefd)	; not EMACS
(efun  82 "Line to Window Border" f_lwindbord	eefd)	; not EMACS
(efun  83 "Scroll Window Up"	f_scupwind	eefd)	; not EMACS
(efun  84 "Scroll Window Down"	f_scdnwind	eefd)	; not EMACS
(efun  85 "Move to Window Top"	f_mvwtop	eefd)	; not EMACS
(efun  86 "Move to Window Bottom" f_mvwbot	eefd)	; not EMACS
     ; 87-89 reserved

; Command Input
(efun  90 "Set Profile"		f_setprof	eecmds)	; ELLE
(efun  91 "Prefix Meta"		f_pfxmeta	eecmds)
(efun  92 "Prefix Extend"	f_pfxext	eecmds)
(efun  93 "Universal Arg"	f_uarg		eecmds)
(efun  94 "Negative Argument"	f_negarg	eecmds)
(efun  95 "Argument Digit"	f_argdig	eecmds)
(efun  96 "VT100 Button Hack"	f_vtbuttons	eecmds)	; not EMACS

; Help
(efun  97 "Describe"		f_describe	eehelp)
     ; 98-99 reserved

; Keyboard Macros
(efun 100 "Start Kbd Macro"	f_skmac		eekmac)
(efun 101 "End Kbd Macro"	f_ekmac		eekmac)
(efun 102 "Execute Kbd Macro"	f_xkmac		eekmac)
(efun 103 "View Kbd Macro"	f_vkmac		eekmac)
    ; 104 reserved

; Killing
(efun 105 "Un-kill"		f_unkill	eef3)
(efun 106 "Un-kill Pop"		f_unkpop	eef3)
(efun 107 "Append Next Kill"	f_appnkill	eef3)
    ; 108-109 reserved

; Searching
(efun 110 "String Search"	f_srch		eesrch)
(efun 111 "Reverse String Search" f_rsrch	eesrch)
(efun 112 "Incremental Search"	f_isrch		eesrch)
(efun 113 "Reverse Search"	f_risrch	eesrch)

; Query Replace & friends
(efun 114 "Replace String"	f_repstr	eequer)
(efun 115 "Query Replace"	f_querep	eequer)
(efun 116 "Replace in Line"	f_repline	eequer)	; not EMACS

; Fill Mode
(efun 117 "Set Fill Column"	f_sfcol		eefill)
(efun 118 "Set Fill Prefix"	f_sfpref	eefill)
(efun 119 "Auto Fill Mode"	f_fillmode	eefill)
(efun 120 "Text Mode"		f_textmode	eefill)	; IMAGEN

; Indentation
(efun 121 "Indent According to Mode" f_indatm	eef3)
(efun 122 "Indent New Line"	f_indnl		eef3)
(efun 123 "Back to Indentation"	f_backind	eef3)
(efun 124 "Indent for Comment"	f_indcomm	eef3)
(efun 125 "Indent Relative"	f_indrel	eef3)
	; 126-128 reserved

; Miscellaneous
(efun 129 "Match Bracket"	f_matchbrack	eef3)	; not EMACS 

; Process Control
(efun 130 "Push to Inferior"	f_pshinf	eemain)
(efun 131 "Return to Superior"	f_retsup	eemain)
(efun 132 "Write File Exit"	f_wfexit	eemain)	; not EMACS
    ; 133-139 reserved

; ELLE Debugging
(efun 140 "Hit Breakpoint"	f_bkpt		eeerr)	; ELLE
(efun 141 "Debug Mode"		f_debug		eediag)	; ELLE
    ; 142-149 reserved
;---------------------------------------------------------------

; IMAGEN configuration only
(efun 150 "Execute Unix Command" f_xucmd	eemake)	; IMAGEN
(efun 151 "Execute Make"	f_make		eemake)	; IMAGEN
(efun 152 "Find Next Error"	f_nxterr	eemake)	; IMAGEN

; ICONOGRAPHICS-specific
(efun 153 "ICO Extend Command"	f_icoxcmd	eefico)	; ICONOGRAPHICS
(efun 154 "ICO Typeset Funs"	f_icotypfns	eefico)	; ICONOGRAPHICS
(efun 155 "ICO Spec Input Funs" f_icospifns	eefico) ; ICONOGRAPHICS

; SUN Mouse functions
(efun 156 "Stuff Selection"	f_stuffsel	eesun)	; SUN
(efun 157 "Select Region"	f_selregion	eesun)	; SUN


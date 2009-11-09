;;;
;;; ELLE Default Command Profile - "defprf.e"
;;;
;;;	This file is input to the ellec program.  It defines the default
;;; command key bindings that ELLE uses, in the absence of an individual
;;; user profile.
;;;	These defaults attempt to emulate the default EMACS command key
;;; bindings.  Differences, where known, are commented.
;;;
;;;	"ELLE" means the function is unique to ELLE.
;;;	E/G: (cmd altnam) "thisname";
;;;		"E:" refers to TOPS-20 EMACS, "G:" refers to Gnu Emacs.
;;;		(cmd) This function exists but is bound to "cmd" instead.
;;;		    (*) function exists but is not bound to any specific key.
;;;		    ()  function does not exist.
;;;		    (=) function exists, with same binding (normally omitted)
;;;		altnam  Name by which this function is known.
;;;		"thisname" - name of function bound to this command.
;;;		    -    means the command is unbound (undefined).

(keyallunbind)		; Flush any predefined bindings

(keybind ^@ "Set/Pop Mark")
(keybind ^A "Beginning of Line")
(keybind ^B "Backward Character")
; ^C not bound.  			; E: ()- G: mode-specific-command-prefix
(keybind ^D "Delete Character")
(keybind ^E "End of Line")
(keybind ^F "Forward Character")
(keybind ^H "Backward Character")	; G: (^B) help-command
(keybind ^I "Indent According to Mode")
(keybind ^J "Indent New Line")
(keybind ^K "Kill Line")
(keybind ^L "New Window")
(keybind ^M "CRLF")
(keybind ^N "Down Real Line")
(keybind ^O "Open Line")
(keybind ^P "Up Real Line")
(keybind ^Q "Quoted Insert")
(keybind ^R "Reverse Search")
(keybind ^S "Incremental Search")
(keybind ^T "Transpose Characters")
(keybind ^U "Universal Arg")
(keybind ^V "Next Screen")
(keybind ^W "Kill Region")
(keybind ^X "Prefix Extend")
(keybind ^Y "Un-kill")
; ^Z not bound			; E: Prefix Control-Meta;  G: suspend-emacs
(keybind ^[ "Prefix Meta")
(keybind "^\" "Debug Mode")	; ELLE. E: () Prefix Meta;  G: () -
; ^] not bound.			; E+G: Abort Recursive Edit
(keybind ^^ "Hit Breakpoint")	; ELLE. E: () Prefix Control;  G: () -
(keybind ^_ "Describe")		; E: (M-?) Help;  G: (^H-k) undo
(keybind " " "Insert Self")
(keybind ! "Insert Self")
(keybind """" "Insert Self")
(keybind # "Insert Self")
(keybind $ "Insert Self")
(keybind % "Insert Self")
(keybind & "Insert Self")
(keybind ' "Insert Self")
(keybind "(" "Insert Self")
(keybind ")" "Insert Self")
(keybind * "Insert Self")
(keybind + "Insert Self")
(keybind , "Insert Self")
(keybind - "Insert Self")
(keybind . "Insert Self")
(keybind / "Insert Self")
(keybind 0 "Insert Self")
(keybind 1 "Insert Self")
(keybind 2 "Insert Self")
(keybind 3 "Insert Self")
(keybind 4 "Insert Self")
(keybind 5 "Insert Self")
(keybind 6 "Insert Self")
(keybind 7 "Insert Self")
(keybind 8 "Insert Self")
(keybind 9 "Insert Self")
(keybind : "Insert Self")
(keybind ";" "Insert Self")
(keybind < "Insert Self")
(keybind = "Insert Self")
(keybind > "Insert Self")
(keybind ? "Insert Self")
(keybind @ "Insert Self")
(keybind A "Insert Self")
(keybind B "Insert Self")
(keybind C "Insert Self")
(keybind D "Insert Self")
(keybind E "Insert Self")
(keybind F "Insert Self")
(keybind G "Insert Self")
(keybind H "Insert Self")
(keybind I "Insert Self")
(keybind J "Insert Self")
(keybind K "Insert Self")
(keybind L "Insert Self")
(keybind M "Insert Self")
(keybind N "Insert Self")
(keybind O "Insert Self")
(keybind P "Insert Self")
(keybind Q "Insert Self")
(keybind R "Insert Self")
(keybind S "Insert Self")
(keybind T "Insert Self")
(keybind U "Insert Self")
(keybind V "Insert Self")
(keybind W "Insert Self")
(keybind X "Insert Self")
(keybind Y "Insert Self")
(keybind Z "Insert Self")
(keybind [ "Insert Self")
(keybind "\" "Insert Self")
(keybind ] "Insert Self")
(keybind ^ "Insert Self")
(keybind _ "Insert Self")
(keybind ` "Insert Self")
(keybind a "Insert Self")
(keybind b "Insert Self")
(keybind c "Insert Self")
(keybind d "Insert Self")
(keybind e "Insert Self")
(keybind f "Insert Self")
(keybind g "Insert Self")
(keybind h "Insert Self")
(keybind i "Insert Self")
(keybind j "Insert Self")
(keybind k "Insert Self")
(keybind l "Insert Self")
(keybind m "Insert Self")
(keybind n "Insert Self")
(keybind o "Insert Self")
(keybind p "Insert Self")
(keybind q "Insert Self")
(keybind r "Insert Self")
(keybind s "Insert Self")
(keybind t "Insert Self")
(keybind u "Insert Self")
(keybind v "Insert Self")
(keybind w "Insert Self")
(keybind x "Insert Self")
(keybind y "Insert Self")
(keybind z "Insert Self")
(keybind { "Insert Self")
(keybind | "Insert Self")
(keybind } "Insert Self")
(keybind ~ "Insert Self")
(keybind DEL "Backward Delete Character")

; Meta chars

(keybind M-^B "Move to Window Bottom")	; ELLE (ima). E+G:()-
(keybind M-^L "Goto Line")		; E:();  G:(* goto-line) -
(keybind M-^N "Scroll Window Down")	; ELLE (ima). E+G:()- forward-list
(keybind M-^P "Scroll Window Up")	; ELLE (ima). E+G:()- backward-list
(keybind M-^R "Reverse String Search")	; E:(*); G:(* search-backward) -
(keybind M-^S "String Search")		; E:(*); G:(* search-forward) isearch-forward-regexp
(keybind M-^T "Move to Window Top")	; ELLE (ima). E+G:()-
(keybind M-^W "Append Next Kill")
(keybind M-^X "Select Existing Buffer")	; ELLE (ima). E+G:()-
(keybind M-^^ "Shrink Window")		; ELLE (ima). E+G:()-
(keybind M-% "Query Replace")
(keybind M-- "Negative Argument")
(keybind M-0 "Argument Digit")
(keybind M-1 "Argument Digit")
(keybind M-2 "Argument Digit")
(keybind M-3 "Argument Digit")
(keybind M-4 "Argument Digit")
(keybind M-5 "Argument Digit")
(keybind M-6 "Argument Digit")
(keybind M-7 "Argument Digit")
(keybind M-8 "Argument Digit")
(keybind M-9 "Argument Digit")
(keybind "M-;" "Indent for Comment")
(keybind M-< "Goto Beginning")
(keybind M-> "Goto End")
(keybind M-[ "Backward Paragraph")
(keybind "M-\" "Delete Horizontal Space")
(keybind M-] "Forward Paragraph")
(keybind M-B "Backward Word")
(keybind M-C "Uppercase Initial")
(keybind M-D "Kill Word")
(keybind M-F "Forward Word")
(keybind M-G "Fill Region")
(keybind M-H "Mark Paragraph")
(keybind M-I "Indent Relative")		; E+G: (*) Tab to Tab Stop
(keybind M-L "Lowercase Word")
(keybind M-M "Back to Indentation")
(keybind M-N  "Next Line")		; E:(*); G:(* forward-line) -
(keybind M-O "VT100 button hack")	; ELLE. E+G: () -
(keybind M-P  "Previous Line")		; E:(*); G:() -
(keybind M-Q "Fill Paragraph")
(keybind M-T "Transpose Words")
(keybind M-U "Uppercase Word")
(keybind M-V "Previous Screen")
(keybind M-W "Copy Region")
(keybind M-Y "Un-kill Pop")
(keybind M-~ "Buffer Not Modified")
(keybind M-DEL "Backward Kill Word")

; Extended commands

(keybind X-^B "List Buffers")
(keybind X-^C "Write File Exit")	; ELLE (ima). E:()-; G: (= save-buffers-kill-emacs)
(keybind X-^E "Write Region")		; E:(*)-;    G:(*) eval-last-sexp
(keybind X-^F "Find File")
(keybind X-^K "Write Last Kill")	; ELLE (mnx). E+G:()-
(keybind X-^L "Lowercase Region")
(keybind X-^M "EOL CRLF Mode")		; ELLE.  E+G: ()-
(keybind X-^O "Delete Blank Lines")
(keybind X-^P "Set Profile")		; ELLE.  E+G: () Mark Page
(keybind X-^R "Read File")
(keybind X-^S "Save File")
(keybind X-^U "Uppercase Region")
(keybind X-^V "Visit File")
(keybind X-^W "Write File")
(keybind X-^X "Exchange Point and Mark")
(keybind X-^Z "Return to Superior")	; G:() suspend-emacs
(keybind X-! "Push to Inferior")	; ELLE.  E:(*)-; G:()-
(keybind X-$  "Replace in Line")	; ELLE (mnx). E+G:()-
(keybind X-% "Replace String")		; E+G: (*) -
(keybind "X-(" "Start Kbd Macro")
(keybind "X-)" "End Kbd Macro")
(keybind X-* "View Kbd Macro")		; E: (*)-; G: ()-
(keybind X-. "Set Fill Prefix")
(keybind X-0 "Delete Window")		; E: ()-
(keybind X-1 "One Window")
(keybind X-2 "Two Windows")
(keybind X-8 "Standout Window")		; ELLE.  E+G:()-
(keybind X-9 "Two Mode Windows")	; ELLE.  E+G:()-
(keybind X-= "What Page")		; E+G: (*) What Cursor Position
(keybind X-^ "Grow Window")
(keybind X-B "Select Buffer")
(keybind X-E "Execute Kbd Macro")
(keybind X-F "Set Fill Column")
(keybind X-I "Insert File")		; E: (*) Info
(keybind X-K "Kill Buffer")
(keybind X-O "Other Window")
(keybind X-S "Save All Files")		; E:(*)-; G:(= save-some-buffers)
(keybind X-T "Auto Fill Mode")		; E:(*) Transpose Regions;  G:(*)-
(keybind X-DEL "Backward Kill Line")	; ELLE(ico)  E+G:() Backward Kill Sentence

; IMAGEN-specific functions, not bound.
;(keybind ""  "Text Mode")		; IMAGEN E:(*);	G:(*)
;(keybind ""  "Execute Unix Command")	; IMAGEN E:();	G:(M-! shell-command)
;(keybind ""  "Execute Make")		; IMAGEN E:(* Compile); G:(* compile)
;(keybind ""  "Find Next Error")	; IMAGEN E:();	G:(X-` next-error)

; SUN Mouse functions, for menuitem selection.
;(menuitem "Stuff Selection")	; SUN
;(menuitem "Select Region")	; SUN

; Forget completely about these.
;(keybind ""  "ICO Extend Command")	; ICONOGRAPHICS
;(keybind ""  "ICO Typeset Funs")	; ICONOGRAPHICS
;(keybind ""  "ICO Spec Input Funs")	; ICONOGRAPHICS


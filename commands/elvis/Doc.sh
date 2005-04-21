echo x - cflags.ms
sed '/^X/s///' > cflags.ms << '/'
X.Go 9 "CFLAGS"
X.PP
X\*E uses many preprocessor symbols to control compilation.
XSome of these control the sizes of buffers and such.
XThe "-DNO_XXXX" options remove small sets of related features.
X.PP
XMost \*E users will probably want to keep all features available.
XMinix-PC users, though, will have to sacrifice some sets because otherwise
X\*E would be too bulky to compile.
XThe "asld" phase of the compiler craps out.
X.IP "-DM_SYSV, -Dbsd, -DTOS, -DCOHERENT, -Damiga"
XThese flags tell the compiler that \*E is being compiled for
XSystem-V UNIX, BSD UNIX, Atari TOS, Coherent, or AmigaDos, respectively.
XFor other systems, the config.h file can generally figure it out automatically.
X.IP -DRAINBOW
XFor MS-DOS systems, this causes support for the DEC Rainbow to be compiled
Xinto \*E.
X.IP -DS5WINSIZE
XSome versions of SysV UNIX don't support support the "winsize"
Xstyle of screen-size testing,
Xso elvis ignores window size changes by default.
X.IP
XHowever, many of the newer SysV systems defines "winsize" in the
Xfile "/usr/include/sys/ptem.h".
XIf your SysV system has "winsize" then you should add
X-DS5SWINSIZE to the CFLAGS setting.
X.IP -DTERMIOS
XPOSIX is a SysV-derived specification which uses a terminal control
Xpackage called "termios", instead of "termio".
XSome other SysV systems may also use termios.
XYou can make elvis uses termios instead of the more common termio
Xby adding -DTERMIOS to CFLAGS.
X(Note: This hasn't been tested very well.)
X.IP -DNBUFS=\fInumber\fP
X\*E keeps most of your text in a temporary file;
Xonly a small amount is actually stored in RAM.
XThis flag allows you to control how much of the file can be in RAM at any time.
XThe default is 5 blocks, and the minimum is 3 blocks.
X(See the -DBLKSIZE flag, below.)
X.IP
XMore RAM allows global changes to happen a little faster.
X f you're just making many small changes in one section of a file, though,
Xextra RAM won't help much.
X.IP -DBLKSIZE=\fInumber\fP
XThis controls the size of blocks that \*E uses internally.
XThe value of BLKSIZE must be a power of two.
XEvery time you double BLKSIZE, you quadruple the size of a text file that
X\*E can handle, but you also cause the temporary file to grow faster.
XFor MS-DOS, Coherent, and Minix-PC, the default value is 1024, which allows
Xyou to edit files up to almost 512K bytes long.
XFor all other systems, the default value is 2048, which allows you to edit
Xfiles that are nearly 2 megabytes long.
X.IP
XThe BLKSIZE also determines the maximum line length, and a few other limits.
XBLKSIZE should be either 256, 512, 1024, or 2048.
XValues other than these can lead to strange behaviour.
X.IP -DTMPDIR=\fIstring\fP
XThis sets the default value of the "directory" option, which specifies where
Xthe temporary files should reside.
XThe value of TMPDIR must be a string, so be sure your value includes the
Xquote characters on each end.
X.IP "-DEXRC=\fIstr\fP, -DHMEXRC=\fIstr\fP, -DSYSEXRC=\fIstr\fP, -DEXINIT=\fIstr\fP"
XThis lets you control the names of the initialization files.
XTheir values must be strings, so be careful about quoting.
X.IP
XEXRC is the name of the initialization file in the current directory.
XIts default value is ".exrc" on UNIX systems -- the same as the real vi.
XSince that isn't a legal DOS filename, under DOS the default is "elvis.rc".
XFor other systems, check the config.h file.
X.IP
XHMEXRC is the name of the initialization file in your home directory.
XBy default, it is the same as EXRC.
X\*E will automatically prepend the name of your home directory to HMEXRC
Xat run time, so don't give a full path name.
X.IP
XSYSEXRC is the name of a system-wide initialization file.
XIt has no default value;
Xif you don't define a value for it, then
Xthe code that supports SYSEXRC just isn't compiled.
XThe value of SYSEXRC should be a full pathname, in quotes.
X.IP
XEXINIT is the name of an environment variable that can contain initialization
Xcommands.
XNormally, its value is "EXINIT".
X.IP -DKEYWORDPRG=\fIstring\fP
XThis flag determines the default value of the "keywordprg" option.
XIts value must be a string, so be careful about quoting.
XThe default value of this flag is "ref", which is a C reference program.
X.IP "-DCC_COMMAND=\fIstring\fP -DMAKE_COMMAND=\fIstring\fP -DERRLIST=\fIstring\fP"
XThese control the names of the C compiler, the "make" utility, and the
Xerror output file, respectively.
XThey are only used if -DNO_ERRLIST is not given.
X.IP
XThe default value of CC_COMMAND depends on the Operating System and compiler
Xthat you use to compile elvis;
Xfor UNIX, the default is "cc".
XThe default values of MAKE_COMMAND and ERRLIST are "make" and "errlist",
Xrespectively.
X.IP -DMAXRCLEN=\fInumber\fP
XThis determines how large a :@ macro command can be (measured in bytes).
XThe default is 1000 bytes.
XIf you increase this value significantly,
Xthen you may need to allocate extra memory for the stack.
XSee the "CHMEM" setting in the Makefile.
X.IP -DSHELL=\fIstring\fP
XThis is the default value of the "shell" option, and hence
Xthe default shell used from within \*E.
XThis only controls the default;
Xthe value you give here may be overridden at run-time by setting
Xan environment variable named SHELL (or COMSPEC for MS-DOS).
XIts value must be a string constant, so be careful about quoting.
X.IP -DTAGS=\fIstring\fP
XThis sets the name of the "tags" file,
Xwhich is used by the :tag command.
XIts value must be a string constant, so be careful about quoting.
X.IP "-DCS_IBMPC -DCS_LATIN1 -DCS_SPECIAL"
XThe digraph table and flipcase option will normally start out empty.
XHowever, if you add -DCS_IBMPC or -DCS_LATIN1 to your CFLAGS,
Xthen they will start out filled with values that are appropriate for the
XIBM PC character set or the ISO Latin-1 character set, respectively.
X.IP
XYou can also use -DCS_IBMPC and -DCS_SPECIAL together to get digraphs
Xthat produce the PC's graphic characters.
X.IP "-DDEBUG -DEBUG2"
X-DDEBUG adds the ":debug" and ":validate" commands,
Xand also adds many internal consistency checks.
XIt increases the size of the ".text" segment by about 6K.
X.IP
X-DDEBUG2 causes a line to be appended to a file called "debug.out"
Xeverytime any change is made to the edit buffer.
X.IP -DCRUNCH
XThis flag removes some non-critical code, so that \*E is smaller.
XFor example, it removes a short-cut from the regexp package, so that
Xtext searches are slower.
XAlso, screen updates are not as efficient.
XA couple of obscure features are disabled by this, too.
X.IP -DNO_MKEXRC
XThis removes the ":mkexrc" command,
Xso you have to create any .exrc files manually.
XThe size of the .text segment will be reduced by about 600 bytes.
X.IP -DNO_CHARATTR
XPermanently disables the charattr option.
XThis reduces the size of your ".text" segment by about 850 bytes.
X.IP -DNO_RECYCLE
XNormally, \*E will recycle space (from the temporary file) which contains
Xtotally obsolete text.
XThis flag disables this recycling.
XWithout recycling, the ".text" segment is about 1K smaller
Xthan it would otherwise be,
Xbut the tmp file grows much faster.
XIf you have a lot of free space on your hard disk,
Xbut \*E is too bulky to run with recycling,
Xthen try it without recycling.
X.IP
XWhen using a version of \*E that has been compiled with -DNO_RECYCLE,
Xyou should be careful to avoid making many small changes to a file
Xbecause each individual change will cause the tmp file to grow by at least 1k.
XHitting "x" thirty times counts as thirty changes,
Xbut typing "30x" counts as one change.
XAlso, you should occasionally do a ":w" followed by a ":e" to start with a
Xfresh tmp file.
X.IP
XInterestingly, the real vi never recycles space from its temporary file.
X.IP -DNO_SENTENCE
XLeaves out the "(" and ")" visual mode commands.
XAlso, the "[[", "]]", "{", and "}" commands will not recognize *roff macros.
XThe sections and paragraphs options go away.
XThis saves about 650 bytes in the ".text" segment.
X.IP -DNO_CHARSEARCH
XLeaves out the visual commands which locate a given character
Xin the current line:
X"f", "t", "F", "T", "," and ";".
XThis saves about 900 bytes.
X.IP -DNO_EXTENSIONS
XLeaves out the "K" and "#" visual commands.
XAlso, the arrow keys will no longer work in input mode.
XRegular expressions will no longer recognize the \\{\\} operator.
X(Other extensions are either inherent in the design of \*E,
Xor are controlled by more specific flags,
Xor are too tiny to be worth removing.)
XThis saves about 250 bytes.
X.IP -DNO_MAGIC
XPermanently disables the "magic" option, so that most meta-characters
Xin a regular expression are *NOT* recognized.
XThis saves about 3k of space in the ".text" segment, because
Xthe complex regular expression code can be replaced by much simpler code.
X.IP -DNO_SHOWMODE
XPermanently disables the "showmode" option, saving about 250 bytes.
X.IP -DNO_CURSORSHAPE
XNormally, \*E tries to adjust the shape of the cursor as a reminder
Xof which mode you're in.
XThe -DNO_CURSORSHAPE flag disables this, saving about 150 bytes.
X.IP -DNO_DIGRAPH
XTo allow entry of non-ASCII characters, \*E supports digraphs.
XA digraph is a single (non-ASCII) character which is entered as a
Xcombination of two other (ASCII) characters.
XIf you don't need to input non-ASCII characters,
Xor if your keyboard supports a better way of entering non-ASCII characters,
Xthen you can disable the digraph code and save about 450 bytes.
X.IP -DNO_ERRLIST
X\*E adds a ":errlist" command, which is useful to programmers.
XIf you don't need this feature, you can disable it via the -DNO_ERRLIST flag.
XThis will reduce the .text segment by about 900 bytes, and the .bss segment
Xby about 300 bytes.
X.IP -DNO_ABBR
XThe -DNO_ABBR flag disables the ":abbr" command,
Xand reduces the size of \*E by about 250 bytes.
X.IP -DNO_OPTCOLS
XWhen \*E displays the current options settings via the ":set" command,
Xthe options are normally sorted into columns.
XThe -DNO_OPTCOLS flag causes the options to be sorted across the rows,
Xwhich is much simpler for the computer.
XThe -DNO_OPTCOLS flag will reduce the size of your .text segment by about
X500 bytes.
X.IP -DNO_MODELINES
XThis removes all support for modelines.
X.IP -DNO_TAG
XThis disables tag lookup.
XIt reduces the size of the .text segment by about 750 bytes.
X.IP "-DNO_ALT_FKEY -DNO_CTRL_FKEY -DNO_SHIFT_FKEY -DNO_FKEY"
XThese remove explicit support of function keys.
X-DNO_ALT_FKEY removes support for the <alternate> versions function keys.
X-DNO_CTRL_FKEY removes support for the <control> and <alternate> versions function keys.
X-DNO_SHIFT_FKEY removes support for the <shift>, <control>, and <alternate> versions function keys.
X-DNO_FKEY removes all support of function keys.
X.IP
X\*E's ":map" command normally allows you to use the special sequence "#<n>"
Xto map function key <n>.
XFor example, ":map #1 {!}fmt^M" will cause the <F1> key to reformat a paragraph.
X\*E checks the :k1=: field in the termcap description of your terminal
Xto figure out what code is sent by the <F1> key.
XThis is handy because it allows you to create a .exrc file which maps function
Xkeys the same way regardless of what type of terminal you use.
X.IP
XThat behaviour is standard; most implementations of the real vi supports it too.
X\*E extends this to allow you to use "#1s" to refer to <shift>+<F1>,
X"#1c" to refer to <control>+<F1>, and
X"#1a" to refer to <alt>+<F1>.
XThe termcap description for the terminal should have fields named
X:s1=:c1=:a1=: respectively, to define the code sent by these key conbinations.
X(You should also have :k2=:s2=:c2=:a2=: for the <F2> key, and so on.)
X.IP
XBut there may be problems.
XThe terminfo database doesn't support :s1=:c1=:a1=:, so no terminfo terminal
Xdescription could ever support shift/control/alt function keys;
Xso you might as well add -DNO_SHIFT_FKEY to CFLAGS if you're using terminfo.
X.IP
XNote that, even if you have -DNO_FKEYS, you can still configure \*E to use
Xyour function keys my mapping the literal character codes sent by the key.
XYou just couldn't do it in a terminal-independent way.
XTERM_925
X.IP "-DTERM_AMIGA -DTERM_VT100 -DTERM_VT52 etc."
XThe tinytcap.c file contains descriptions of several terminal types.
XFor each system that uses tinytcap, a reasonable subset of the available
Xdescriptions is actually compiled into \*E.
XIf you wish to enlarge this subset, then you can add the appropriate -DTERM_XXX
Xflag to your CFLAGS settings.
X.IP
XFor a list of the available terminal types, check the tinytcap.c file.
X.IP -DINTERNAL_TAGS
XNormally, \*E uses the "ref" program to perform tag lookup.
XThis is more powerful than the real vi's tag lookup,
Xbut it can be much slower.
X.IP
XIf you add -DINTERNAL_TAGS to your CFLAGS setting,
Xthen \* will use its own internal tag lookup code, which is faster.
X.IP -DPRSVDIR=\fIdirectory\fR
XThis controls where preserved files will be placed.
XAn appropriate default has been chosen for each Operating System,
Xso you probably don't need to worry about it.
X.IP -DFILEPERMS=\fInumber\fR
XThis affects the attributes of files that are created by \*E;
Xit is used as the second argument to the creat() function.
XThe default is 0666 which (on UNIX systems at least) means that
Xanybody can read or write the new file, but nobody can execute it.
XOn UNIX systems, the creat() call modifies this via the umask setting.
X.IP -DKEYBUFSIZE=\fInumber\fR
XThis determines the size of the type-ahead buffer that elvis uses.
XIt also limits the size of keymaps that it can handle.
XThe default is 1000 characters, which should be plenty.
/
echo x - cutbufs.ms
sed '/^X/s///' > cutbufs.ms << '/'
X.Go 6 "CUT BUFFERS"
X.PP
XWhen \*E deletes text, it stores that text in a cut buffer.
XThis happens in both visual mode and EX mode.
XThere is no practical limit to how much text a cut buffer can hold.
X.PP
XThere are 36 cut buffers:
X26 named buffers ("a through "z),
X9 anonymous buffers ("1 through "9),
Xand 1 extra cut buffer (".).
X.PP
XIn EX mode, the :move and :copy commands use a cut buffer to temporarily
Xhold the text to be moved/copied.
X.NH 2
XPutting text into a Cut Buffer
X.PP
XIn visual mode, text is copied into a cut buffer when you use the
Xd, y, c, C, s, or x commands.
XThere are also a few others.
X.PP
XBy default, the text goes into the "1 buffer.
XThe text that used to be in "1 gets shifted into "2,
X"2 gets shifted into "3, and so on.
XThe text that used to be in "9 is lost.
XThis way, the last 9 things you deleted are still accessible.
X.PP
XYou can also put the text into a named buffer -- "a through "z.
XTo do this, you should type the buffer's name
X(two keystrokes: a double-quote and a lowercase letter)
Xbefore the command that will cut the text.
XWhen you do this, "1 through "9 are not affected by the cut.
X.PP
XYou can append text to one of the named buffers.
XTo do this, type the buffer's name in uppercase
X(a double-quote and an uppercase letter)
Xbefore the d/y/c/C/s/x command.
X.PP
XThe ". buffer is special.
XIt isn't affected by the d/y/c/C/s/x command.
XInstead, it stores the text that you typed in
Xthe last time you were in input mode.
XIt is used to implement the . visual command,
Xand ^A in input mode.
X.PP
XIn EX mode (also known as colon mode),
Xthe :delete, :change, and :yank commands all copy text into a cut buffer.
XLike the visual commands, these EX commands normally use the "1 buffer,
Xbut you can use one of the named buffers by giving its name after the command.
XFor example,
X.sp 1
X.ti +0.5i
X:20,30y a
X.sp
X.LP
Xwill copy lines 20 through 30 into cut buffer "a.
X.PP
XYou can't directly put text into the ". buffer, or the "2 through "9 buffers.
X.NH 2
XPasting from a Cut Buffer
X.PP
XThere are two styles of pasting:
Xline-mode and character-mode.
XIf a cut buffer contains whole lines (from a command like "dd")
Xthen line-mode pasting is used;
Xif it contains partial lines (from a command like "dw")
Xthen character-mode pasting is used.
XThe EX commands always cut whole lines.
X.PP
XCharacter-mode pasting causes the text to be inserted into the line that
Xthe cursor is on.
X.PP
XLine-mode pasting inserts the text on a new line above or below the line
Xthat the cursor is on.
XIt doesn't affect the cursor's line at all.
X.PP
XIn visual mode, the p and P commands insert text from a cut buffer.
XUppercase P will insert it before the cursor,
Xand lowercase p will insert it after the cursor.
XNormally, these commands will paste from the "1 buffer, but you can
Xspecify any other buffer to paste from.
XJust type its name (a double-quote and another character)
Xbefore you type the P or p.
X.PP
XIn EX mode, the (pu)t command pastes text after a given line.
XTo paste from a buffer other that "1,
Xenter its name after the command.
X.NH 2
XMacros
X.PP
XThe contents of a named cut buffer can be executed as a series of
Xex/vi commands.
X.PP
XTo put the instructions into the cut buffer, you must first insert
Xthem into the file, and then delete them into a named cut buffer.
X.PP
XTo execute a cut buffer's contents as EX commands,
Xyou should give the EX command "@" and the name of the buffer.
XFor example, :@z will execute "z as a series of EX commands.
X.PP
XTo execute a cut buffer's contents as visual commands,
Xyou should give the visual command "@" and the letter of the buffer's name.
XThe visual "@" command is different from the EX "@" command.
XThey interpret the cut buffer's contents differently.
X.PP
XThe visual @ command can be rather finicky.
XEach character in the buffer is interpretted as a keystroke.
XIf you load the instructions into the cut buffer via a "zdd command,
Xthen the newline character at the end of the line will be executed just
Xlike any other character, so the cursor would be moved down 1 line.
XIf you don't want the cursor to move down 1 line at the end of each
X@z command, then you should load the cut buffer by saying 0"zD instead.
X.PP
XAlthough cut buffers can hold any amount of text,
X\*E can only \fIexecute\fR small buffers.
XThe size limit is roughly 1000 characters, for either EX macros or VI macros.
XIf a buffer is too large to execute, an error message is displayed.
X.PP
XYou can't nest :@ commands.
XYou can't run :@ commands from your .exrc file,
Xor any other :source file either.
XSimilarly, you can't run a :source command from within an @ command.
XHopefully, these restrictions will be lifted in a later version.
X.NH 2
XThe Effect of Switching Files
X.PP
XWhen \*E first starts up, all cut buffers are empty.
XWhen you switch to a different file
X(via the :n or :e commands perhaps)
Xthe 9 anonymous cut buffers are emptied again,
Xbut the other 27 buffers ("a through "z, and ".) retain their text.
/
echo x - differ.ms
sed '/^X/s///' > differ.ms << '/'
X.Go 7 "DIFFERENCES BETWEEN \*E & BSD VI/EX"
X.PP
X\*E is not 100% compatible with the real vi/ex.
X\*E has many small extensions, some omissions, and a few features which
Xare implemented in a slightly different manner.
X.NH 2
XExtensions
X.IP "Save Configuration" 1i
XThe :mkexrc command saves the current :set and :map configurations in
Xthe ".exrc" file in your current directory.
X.IP "Previous File" 1i
XThe :N or :prev command moves backwards through the args list.
X.IP "Center Current Row" 1i
XIn visual command mode, the (lowercase) "zz" command will center the current
Xline on the screen, like "z=".
X.IP "Changing Repeat Count" 1i
XThe default count value for . is the same as the previous command
Xwhich . is meant to repeat.
XHowever, you can supply a new count if you wish.
XFor example, after "3dw", "." will delete 3 words,
Xbut "5." will delete 5 words.
X.IP "Previous Text" 1i
XThe text which was most recently input
X(via a "cw" command, or something similar)
Xis saved in a cut buffer called ". (which
Xis a pretty hard name to write in an English sentence).
X.IP "Keyword Lookup" 1i
XIn visual command mode, you can move the cursor onto a word and press
Xshift-K to have \*E run a reference program to look that word up.
XThis command alone is worth the price of admission!
XSee the ctags and ref programs.
X.IP "Increment/Decrement" 1i
XIn visual command mode, you can move the cursor onto a number and
Xthen hit ## or #+ to increment that number by 1.
XTo increment it by a larger amount,
Xtype in the increment value before hitting the initial #.
XThe number can also be decremented or set by hitting #- or #=, respectively.
X.IP "Input Mode" 1i
XYou can backspace past the beginning of the line.
X.IP "" 1i
XThe arrow keys work in input mode.
X.IP "" 1i
XIf you type control-A, then the text that you input last time is inserted.
XYou will remain in input mode, so you can backspace over part of it,
Xor add more to it.
X(This is sort of like control-@ on the real vi,
Xexcept that control-A really works.)
X.IP "" 1i
XControl-P will insert the contents of the cut buffer.
X.IP "" 1i
XReal vi can only remember up to 128 characters of input,
Xbut \*E can remember any amount.
X.IP "" 1i
XThe ^T and ^D keys can adjust the indent of a line no matter where
Xthe cursor happens to be in that line.
X.IP "" 1i
XYou can save your file and exit \*E directly from input mode by hitting
Xcontrol-Z twice.
X.IP "" 1i
X\*E supports digraphs as a way to enter non-ASCII characters.
X.IP "Start in Input Mode" 1i
XIf you ":set inputmode" in your .exrc file, then \*E will start up in
Xinput mode instead of visual command mode.
X.IP "Visible Fonts" 1i
XWith ":set charattr", \*E can display "backslash-f" style character attributes on the
Xscreen as you edit.
XThe following example shows the recognized atributes:
X.sp
X.ti +0.5i
Xnormal \\fBboldface\\fR \\fIitalics\\fR \\fUunderlined\\fR normal
X.sp
XNOTE: you must compile \*E without the -DNO_CHARATTR flag for
Xthis to work.
X.IP "File Syncing" 1i
XAfter a crash, you can usually recover the altered form of the file
Xfrom the temporary file that \*E uses -- unless the temporary file was
Xcorrupted.
X.IP "" 1i
XUNIX systems use a delayed-write cache, which means that when \*E tries to
Xwrite to the temporary file, the information might still be in RAM instead
Xof on the disk.
XA power failure at that time would cause the in-RAM information to be lost.
XUNIX's sync() call will force all such information to disk.
X.IP "" 1i
XMS-DOS and Atari TOS don't write a file's length to disk until that file
Xis closed.
XConsequently, the temporary file would appear to be 0 bytes long if power
Xfailed when we were editing.
XTo avoid this problem, a sync() function has been written which will close
Xthe temporary file and then immediately reopen it.
X.IP "Cursor Shape" 1i
X\*E changes the shape of the cursor to indicate which mode you're in,
Xif your terminal's termcap entry includes the necessary capabilities.
X.IP "Hide nroff Lines" 1i
XTh ":set hideformat" option hides nroff format control lines.
X(They are displayed on the screen as blank lines.)
X.ne 7
X.IP "Compiler Interface" 1i
X\*E is clever enough to parse the error messages emitted by many compilers.
XTo use this feature,
Xyou should collect your compiler's error messages into a file called "errlist";
X\*E will read this file,
Xdetermine which source file caused the error messages,
Xstart editing that file,
Xmove the cursor to the line where the error was detected,
Xand display the error message on the status line.
XNifty!
X.IP "Visible Text Selection" 1i
XIn visual command mode, 'v' starts visibly selecting characters and
X\&'V' starts visibly selecting whole lines.
XThe character or line where the cursor is located becomes one
Xendpoint of the selection.
XYou can then use the standard cursor movement commands to move the cursor
Xto the other endpoint, and then press one of the operator commands
X(c/d/y/</>/!/=/\\).
XThe operator will then immediately be applied to the selected text.
X.IP "Pop-up Menu Operator" 1i
XThe '\\' key is a new operator,
Xsimilar in operation to the c/d/y/</>/! operators
XIt conjures up a menu, from which you can select any of the other
Xoperators plus a few other common commands.
X.IP "Preset Filter Operator" 1i
XThe '=' key is another new operator.
XIt is similar to the '!' operator, except that while
X\&'!' asks you to type in a filter command each time,
X\&'=' assumes it should always run the command stored in the \fIequalprg\fR option.
X.IP "Move to a Given Percentage" 1i
XThe '%' movement key can now accept an optional count.
XWithout a count, the '%' key still moves to a matching parenthesis
Xlike it always did.
XWith a count somewhere between 1 and 100, though, it moves the cursor to
Xapproximately a given percentage of the way through the file.
XFor example, typing "50%" will move the cursor to the middle of the file.
X.IP "Regular Expressions"
XIn regular expressions, several new forms of closure operators are supported:
X\\{\fIn\fR}, \\{\fIn\fR,\fIm\fR}, \\+, and \\?.
X.NH 2
XOmissions
X.PP
XThe replace mode is a hack.
XIt doesn't save the text that it overwrites.
X.PP
XLong lines are displayed differently -- where the real vi would
Xwrap a long line onto several rows of the screen, \*E simply
Xdisplays part of the line, and allows you to scroll the screen
Xsideways to see the rest of it.
X.PP
XThe ":preserve" and ":recover" commands are missing.
XSo is the -r flag.
XI've never had a good reason to use ":preserve",
Xand since ":recover" is used so rarely
XI decided to implement it as a separate program.
XThere's no need to load the recovery code into memory every
Xtime you edit a file, I figured.
X.PP
XLISP support is missing.
XHowever, the = key is still an operator that reformats lines of text.
XBy default, it reformats lines by sending them through the \fIfmt\fP filter,
Xbut you could write your own LISP beautifier and configure elvis to use it.
XKey mappings could take care of most other differences.
XAuto-indent is the only thing that is irrecoverably lost.
X.PP
XAutoindent mode acts a little different from the real vi, anyway.
XIt doesn't handle ^^D or 0^D correctly.
XOn the other hand, it \fIdoes\fP allow ^D and ^T to be used anywhere in the
Xline, to adjust the indentation for the whole line.
/
echo x - environ.ms
sed '/^X/s///' > environ.ms << '/'
X.Go 11 "ENVIRONMENT VARIABLES"
X.PP
X\*E examines several environment variables when it starts up.
XThe values of these variables are used internally for a variety
Xof purposes.
XYou don't need to define all of these;
Xon most systems, \*E only requires TERM to be defined.
XOn AmigaDOS, MS-DOS or TOS systems, even that is optional.
X.SH
XTERM, TERMCAP
X.PP
XTERM tells \*E the name of the termcap entry to use.
XTERMCAP may contain either the entire termcap entry,
Xor the full pathname of the termcap file to search through.
X.PP
XIf your version of \*E is using tinytcap instead of the full termcap library,
Xthen the value of TERMCAP \fIcannot\fR contain any backslash escapes (\\E, \\r, etc.)
Xor carat escapes (^[, ^M, etc.), because tinytcap doesn't understand them.
XInstead, you should embed the actual control character into the string.
X.SH
XTMP, TEMP
X.PP
XThese only work for AmigaDOS, MS-DOS and Atari TOS.
XEither of these variables may be used to set the "directory" option,
Xwhich controls where temporary files are stored.
XIf you define them both, then TMP is used, and TEMP is ignored.
X.SH
XLINES, COLUMNS
X.PP
XThe termcap entry for your terminal should specify the size of your screen.
XIf you're using a windowing interface, then there is an ioctl() call which
Xwill provide the size of the window; the ioctl() values will override the
Xvalues in the termcap entry.
XThe LINES and COLUMNS environment variables (if defined)
Xwill override either of these sources.
XThey, in turn, can be overridden by a ":set" command.
X.PP
XNormally, the LINES and COLUMNS variables shouldn't need to be defined.
X.SH
XEXINIT
X.PP
XThis variable's value may contain one or more colon-mode commands,
Xwhich will be executed after all of the ".exrc" files
Xbut before interactive editing begins.
X.PP
XTo put more than one command in EXINIT, you can separate the commands
Xwith either a newline or a '|' character.
X.SH
XSHELL, COMSPEC
X.PP
XYou can use COMSPEC in MS-DOS, or SHELL in any other system,
Xto specify which shell should be used for executing commands and
Xexpanding wildcards.
X.SH
XHOME
X.PP
XThis variable should give the full pathname of your home directory.
X\*E needs to know the name of your home directory so it can locate
Xthe ".exrc" file there.
X.SH
XTAGPATH
X.PP
XThis variable is used by the "ref" program.
XIt contains a list of directories that might contain a relevent "tags" file.
XUnder AmigaDOS, MS-DOS or Atari TOS, the names of the directories should be separated by
Xsemicolons (";").
XUnder other operating systems, the names should be separated by colons (":").
X.PP
XIf you don't define TAGPATH, then "ref" will use a default list which includes
Xthe current directory and a few other likely places.
XSee the definition of DEFTAGPATH at the start of ref.c for an accurate list.
/
echo x - ex.ms
sed '/^X/s///' > ex.ms << '/'
X.Go 3 "COLON MODE COMMANDS"
X.ID
X.ps
X.in 0.8i
X.ta 2i 3.i
X.\" NOTE: The following macro is used to output a single line of the
X.\" command chart.  Its usage is:
X.\"
X.\"		.Cm <linespecs> <name> <arguments>...
X.\"
X.de Cm
X.if "\\$1"0" \t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
X.if "\\$1"1" \s-2[line]\s+2\t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
X.if "\\$1"2" \s-2[line][,line]\s+2\t\\$2\t\\$3 \\$4 \\$5 \\$6 \\$7 \\$8 \\$9
X..
X.if t .ds Q ``
X.if t .ds U ''
X.if n .ds Q "
X.if n .ds U "
X\s+2LINES	COMMAND	ARGUMENTS\s-2
X.Cm 0 ab[br] [short] [expanded form]
X.Cm 1 a[ppend][!]
X.Cm 0 ar[gs] [files]
X.Cm 0 cc [files]
X.Cm 0 cd[!] [directory]
X.Cm 2 c[hange]
X.Cm 0 chd[ir][!] [directory]
X.Cm 2 co[py] line
X.Cm 0 col[or] [when] [[\*Qlight\*U] color] [\*Qon\*U color]
X.Cm 2 d[elete] [\*Ux]
X.Cm 0 dig[raph][!] [XX [Y]]
X.Cm 0 e[dit][!] [file]
X.Cm 0 er[rlist][!] [errlist]
X.Cm 0 f[ile] [file]
X.Cm 2 g[lobal] /regexp/ command
X.Cm 1 i[nsert]
X.Cm 2 j[oin][!]
X.Cm 2 l[ist]
X.Cm 0 mak[e] [target]
X.Cm 0 map[!] key mapped_to
X.Cm 1 ma[rk]  \*Ux
X.Cm 0 mk[exrc]
X.Cm 2 m[ove] line
X.Cm 0 n[ext][!] [files]
X.Cm 0 N[ext][!]
X.Cm 2 nu[mber]
X.Cm 2 p[rint]
X.Cm 1 pu[t] [\*Ux]
X.Cm 0 q[uit][!]
X.Cm 1 r[ead] file
X.Cm 0 rew[ind][!]
X.Cm 0 se[t] [options]
X.Cm 0 so[urce] file
X.Cm 2 s[ubstitute] /regexp/replacement/[p][g][c]
X.Cm 0 ta[g][!] tagname
X.Cm 0 una[bbr] [short]
X.Cm 0 u[ndo]
X.Cm 0 unm[ap][!] key
X.Cm 0 ve[rsion]
X.Cm 2 v[global] /regexp/ command
X.Cm 0 vi[sual] [filename]
X.Cm 0 wq 
X.Cm 2 w[rite][!] [[>>]file]
X.Cm 0 x[it][!]
X.Cm 2 y[ank] [\*Ux]
X.Cm 2 ! command
X.Cm 2 < 
X.Cm 2 = 
X.Cm 2 > 
X.Cm 2 & 
X.Cm 0 @ "" \*Ux
X.DE
X.TA
X.PP
XTo use colon mode commands, you must switch from visual command
Xmode to colon command mode.
XThe visual mode commands to do this are ":" for a single colon command,
Xor "Q" for many colon mode commands.
X.NH 2
XLine Specifiers
X.PP
XLine specifiers are always optional.
XThe first line specifier of most commands usually defaults to the current line.
XThe second line specifier usually defaults to be the same
Xas the first line specifier.
XExceptions are :write, :global, and :vglobal, which act on all lines of the
Xfile by default, and :!, which acts on no lines by default.
X.PP
XLine specifiers consist of an absolute part and a relative part.
XThe absolute part of a line specifier may be either an explicit line number,
Xa mark, a dot to denote the current line, a dollar sign to denote the last
Xline of the file, or a forward or backward search.
X.PP
XAn explicit line number is simply a decimal number, expressed as a
Xstring of digits.
X.PP
XA mark is typed in as an apostrophe followed by a letter.
XMarks must be set before they can be used.
XYou can set a mark in visual command mode by typing "m" and a letter,
Xor you can set it in colon command mode via the "mark" command.
X.PP
XA forward search is typed in as a regular expression surrounded by
Xslash characters; searching begins at the default line.
XA backward search is typed in as a regular expression surrounded by
Xquestion marks; searching begins at the line before the default line.
X.PP
XIf you omit the absolute part, then the default line is used.
X.PP
XThe relative part of a line specifier is typed as a "+" or "-" character
Xfollowed by a decimal number.
XThe number is added to or subtracted from the absolute part
Xof the line specifier to produce the final line number.
X.PP
XAs a special case, the % character may be used to specify all lines of the file.
XIt is roughly equivelent to saying 1,$.
XThis can be a handy shortcut.
X.PP
XSome examples:
X.LD
X.ps
X.ta 0.5i 1.8i
X	:p	print the current line
X	:37p	print line 37
X	:'gp	print the line which contains mark g
X	:/foo/p	print the next line that contains "foo"
X	:$p	print the last line of the file
X	:20,30p	print lines 20 through 30
X	:1,$p	print all lines of the file
X	:%p	print all lines of the file
X	:/foo/-2,+4p	print 5 lines around the next "foo"
X.TA
X.DE
X.NH 2
XText Entry Commands
X.if n .ul 0
X.ID
X.ps
X[line] append
X[line][,line] change ["x]
X[line] insert
X.DE
X.PP
XThe \fBa\fRppend command inserts text after the specified line.
X.PP
XThe \fBi\fRnsert command inserts text before the specified line.
X.PP
XThe \fBc\fRhange command copies the range of lines into a cut buffer,
Xdeletes them, and inserts new text where the old text used to be.
X.PP
XFor all of these commands, you indicate the end of the text you're
Xinserting by hitting ^D or by entering a line which contains only a
Xperiod.
X.NH 2
XCut & Paste Commands
X.if n .ul 0
X.ID
X.ps
X[line][,line] delete ["x]
X[line][,line] yank ["x]
X[line] put ["x]
X[line][,line] copy line
X[line][,line] to line
X[line][,line] move line
X.DE
X.PP
XThe \fBd\fRelete command copies the specified range of lines into a
Xcut buffer, and then deletes them.
X.PP
XThe \fBy\fRank command copies the specified range of lines into a cut
Xbuffer, but does *not* delete them.
X.PP
XThe \fBpu\fRt command inserts text from a cut buffer after the
Xspecified line.
X.PP
XThe \fBco\fRpy and \fBt\fRo commands yank the specified range of lines and
Xthen immediately paste them after some other line.
X.PP
XThe \fBm\fRove command deletes the specified range of lines and then
Ximmediately pastes them after some other line.
XIf the destination line comes after the deleted text,
Xthen it will be adjusted automatically to account for the deleted lines.
X.NH 2
XDisplay Text Commands
X.if n .ul 0
X.ID
X.ps
X[line][,line] print
X[line][,line] list
X[line][,line] number
X.DE
X.PP
XThe \fBp\fRrint command displays the specified range of lines.
X.PP
XThe \fBnu\fRmber command displays the lines, with line numbers.
X.PP
XThe \fBl\fRist command also displays them, but it is careful to make
Xcontrol characters visible.
X.NH 2
XGlobal Operations Commands
X.if n .ul 0
X.ID
X.ps
X[line][,line] global /regexp/ command
X[line][,line] vglobal /regexp/ command
X.DE
X.PP
XThe \fBg\fRlobal command searches through the lines of the specified range
X(or through the whole file if no range is specified)
Xfor lines that contain a given regular expression.
XIt then moves the cursor to each of these lines and
Xruns some other command on them.
X.PP
XThe \fBv\fRglobal command is similar, but it searches for lines that \fIdon't\fR
Xcontain the regular expression.
X.NH 2
XLine Editing Commands
X.if n .ul 0
X.ID
X.ps
X[line][,line] join[!]
X[line][,line] ! program
X[line][,line] <
X[line][,line] >
X[line][,line] substitute /regexp/replacement/[p][g][c]
X[line][,line] &
X.DE
X.PP
XThe \fBj\fRoin command catenates all lines in the specified range together
Xto form one big line.
XIf only a single line is specified, then the following line is catenated
Xonto it.
XThe normal ":join" inserts one or two spaces between the lines;
Xthe ":join!" variation (with a '!') doesn't insert spaces.
X.PP
XThe \fB!\fR command runs an external filter program,
Xand feeds the specified range of lines to it's stdin.
XThe lines are then replaced by the output of the filter.
XA typical example would be ":'a,'z!sort" to sort the lines 'a,'z.
X.PP
XThe \fB<\fR and \fB>\fR commands shift the specified range of lines left or right,
Xnormally by the width of 1 tab character.
XThe "shiftwidth" option determines the shifting amount.
X.PP
XThe \fBs\fRubstitute command finds the regular expression in each line,
Xand replaces it with the replacement text.
XThe "p" option causes the altered lines to be printed.
XThe "g" option permits all instances of the regular expression
Xto be found & replaced.
X(Without "g", only the first occurrence in each line is replaced.)
XThe "c" option asks for confirmation before each substitution.
X.PP
XThe \fB&\fR command repeats the previous substitution command.
XActually, "&" is equivelent to "s//~/" with the same options as last time.
XIt searches for the last regular expression that you specified for any purpose,
Xand replaces it with the the same text
Xthat was used in the previous substitution.
X.NH 2
XUndo Command
X.if n .ul 0
X.ID
X.ps
Xundo
X.DE
X.PP
XThe \fBu\fRndo command restores the file to the state it was in before
Xyour most recent command which changed text.
X.NH 2
XConfiguration & Status Commands
X.if n .ul 0
X.ID
X.ps
Xmap[!] [key mapped_to]
Xunmap[!] key
Xabbr [word expanded_form_of_word]
Xunabbr word
Xdigraph[!] [XX [Y]]
Xset [options]
Xmkexrc
X[line] mark "x
Xvisual
Xversion
X[line][,line] =
Xfile [file]
Xsource file
X@ "x
Xcolor [when] [["light"] color] ["on" color]
X.DE
X.PP
XThe \fBma\fRp command allows you to configure \*E to recognize your function keys,
Xand treat them as though they transmitted some other sequence of characters.
XNormally this mapping is done only when in the visual command mode,
Xbut with the [!] present it will map keys under input and replace modes as well.
XWhen this command is given with no arguments,
Xit prints a table showing all mappings currently in effect.
XWhen called with two arguments, the first is the sequence that your
Xfunction key really sends, and the second is the sequence that you want
X\*E to treat it as having sent.
XAs a special case, if the first argument is a number then \*E will map the
Xcorresponding function key;
Xfor example, ":map 7 dd" will cause the <F7> key to delete a line.
X.PP
XThe \fBunm\fRap command removes key definitions that were made via the map command.
X.PP
XThe \fBab\fRbr command is used to define/list a table of abbreviations.
XThe table contains both the abbreviated form and the fully spelled-out form.
XWhen you're in visual input mode, and you type in the abbreviated form,
X\*E will replace the abbreviated form with the fully spelled-out form.
XWhen this command is called without arguments, it lists the table;
Xwith two or more arguments, the first argument is taken as the abbreviated
Xform, and the rest of the command line is the fully-spelled out form.
X.PP
XThe \fBuna\fRbbr command deletes entries from the abbr table.
X.PP
XThe \fBdi\fRgraph command allows you to display the set of digraphs that \*E is
Xusing, or add/remove a digraph.
XTo list the set of digraphs, use the digraph command with no arguments.
XTo add a digraph, you should give the digraph command two arguments.
XThe first argument is the two ASCII characters that are to be combined;
Xthe second is the non-ASCII character that they represent.
XThe non-ASCII character's most significant bit is automatically set by the
Xdigraph command, unless to append a ! to the command name.
XRemoval of a digraph is similar to adding a digraph, except that you should
Xleave off the second argument.
X.PP
XThe \fBse\fRt command allows you examine or set various options.
XWith no arguments, it displays the values of options that have been changed.
XWith the single argument "all" it displays the values of all options,
Xregardless of whether they've been explicitly set or not.
XOtherwise, the arguments are treated as options to be set.
X.PP
XThe \fBmk\fRexrc command saves the current configuration to a file
Xcalled ".exrc" in the current directory.
X.PP
XThe mar\fBk\fR command defines a named mark to refer to a specific place
Xin the file.
XThis mark may be used later to specify lines for other commands.
X.PP
XThe \fBvi\fRsual command puts the editor into visual mode.
XInstead of emulating ex, \*E will start emulating vi.
X.PP
XThe \fBve\fRrsion command tells you that what version of \*E this is.
X.PP
XThe \fB=\fR command tells you what line you specified, or,
Xif you specified a range of lines, it will tell you both endpoints and
Xthe number of lines included in the range.
X.PP
XThe \fBf\fRile command tells you the name of the file,
Xwhether it has been modified,
Xthe number of lines in the file,
Xand the current line number.
XYou can also use it to change the name of the current file.
X.PP
XThe \fBso\fRurce command reads a sequence of colon mode commands from a file,
Xand interprets them.
X.PP
XThe \fB@\fR command executes the contents of a cut-buffer as EX commands.
X.PP
XThe \fBcol\fRor command only works under MS-DOS, or if you have an ANSI-compatible
Xcolor terminal.
XIt allows you to set the foreground and background colors
Xfor different types of text:
Xnormal, bold, italic, underlined, standout, pop-up menu, and visible selection.
XBy default, it changes the "normal" colors;
Xto change other colors, the first argument to the :color command should be
Xthe first letter of the type of text you want.
XThe syntax for the colors themselves is fairly intuitive.
XFor example, ":color light cyan on blue" causes normal text to be displayed
Xin light cyan on a blue background, and
X":color b bright white" causes bold text to be displayed in bright white on
Xa blue background.
XThe background color always defaults to the current background color of
Xnormal text.
XYour first :color command \fImust\fP specify both the foreground and background
Xfor normal text.
X.NH 2
XMultiple File Commands
X.if n .ul 0
X.ID
X.ps
Xargs [files]
Xnext[!] [files]
XNext[!]
Xprevious[!]
Xrewind[!]
X.DE
X.PP
XWhen you invoke \*E from your shell's command line,
Xany filenames that you give to \*E as arguments are stored in the args list.
XThe \fBar\fRgs command will display this list, or define a new one.
X.PP
XThe \fBn\fRext command switches from the current file to the next one
Xin the args list.
XYou may specify a new args list here, too.
X.PP
XThe \fBN\fRext and \fBpre\fRvious commands
X(they're really aliases for the same command)
Xswitch from the current file to the preceding file in the args list.
X.PP
XThe \fBrew\fRind command switches from the current file to the first file
Xin the args list.
X.NH 2
XSwitching Files
X.if n .ul 0
X.ID
X.ps
Xedit[!] [file]
Xtag[!] tagname
X.DE
X.PP
XThe \fBe\fRdit command allows to switch from the current file to some other file.
XThis has nothing to do with the args list, by the way.
X.PP
XThe \fBta\fRg command looks up a given tagname in a file called "tags".
XThis tells it which file the tag is in, and how to find it in that file.
X\*E then switches to the tag's file and finds the tag.
X.NH 2
XWorking with a Compiler
X.if n .ul 0
X.ID
X.ps
Xcc [files]
Xmake [target]
Xerrlist[!] [errlist]
X.DE
X.PP
XThe \fBcc\fR and \fBmak\fRe commands execute your compiler or "make" utility
Xand redirect any error messages into a file called "errlist".
XBy default, cc is run on the current file.
X(You should write it before running cc.)
XThe contents of the "errlist" file are then scanned for error messages.
XIf an error message is found, then the cursor is moved to the line where
Xthe error was detected,
Xand the description of the error is displayed on the status line.
X.PP
XAfter you've fixed one error, the \fBer\fRrlist command will move
Xthe cursor to the next error.
XIn visual command mode,
Xhitting `*' will do this, too.
X.PP
XYou can also create an "errlist" file from outside of \*E,
Xand use "\*E -m" to start elvis and have the cursor moved to the
Xfirst error.
XNote that you don't need to supply a filename with "\*E -m" because
Xthe error messages always say which source file an error is in.
X.PP
XNote:
XWhen you use errlist repeatedly to fix several errors in a single file,
Xit will attempt to adjust the reported line numbers to allow for lines
Xthat you have inserted or deleted.
XThese adjustments are made with the assumption that you will work though
Xthe file from the beginning to the end.
X.NH 2
XExit Commands
X.if n .ul 0
X.ID
X.ps
Xquit[!]
Xwq
Xxit
X.DE
X.PP
XThe \fBq\fRuit command exits from the editor without saving your file.
X.PP
XThe \fBwq\fR command writes your file out, then then exits.
X.PP
XThe \fBx\fRit command is similar to the \fBwq\fR command, except that
X\fBx\fRit won't bother to write your file if you haven't modified it.
X.NH 2
XFile I/O Commands
X.if n .ul 0
X.ID
X.ps
X[line] read file
X[line][,line] write[!] [[>>]file]
X.DE
X.PP
XThe \fBr\fRead command gets text from another file and inserts it
Xafter the specified line.
XIt can also read the output of a program;
Xsimply precede the program name by a '!' and use it in place of the file name.
X.PP
XThe \fBw\fRrite command writes the whole file, or just part of it,
Xto some other file.
XThe !, if present, will permit the lines to be written even if you've set
Xthe readonly option.
XIf you precede the filename by >> then the lines will be appended to the file.
XYou can send the lines to the standard input of a program by replacing the
Xfilename with a '!' followed by the command and its arguments.
X.PP
XNote: Be careful not to confuse ":w!filename" and ":w !command".
XTo write to a program, you must have at least one blank before the '!'.
X.NH 2
XDirectory Commands
X.if n .ul 0
X.ID
X.ps
Xcd [directory]
Xchdir [directory]
Xshell
X.DE
X.PP
XThe \fBcd\fR and \fBchd\fRir commands
X(really two names for one command)
Xswitch the current working directory.
X.PP
XThe \fBsh\fRell command starts an interactive shell.
X.NH 2
XDebugging Commands
X.if n .ul 0
X.ID
X.ps
X[line][,line] debug[!]
Xvalidate[!]
X.DE
X.PP
XThese commands are only available if you compile \*E with the -DDEBUG flag.
X.PP
XThe de\fBb\fRug command lists statistics for the blocks which contain
Xthe specified range of lines.
XIf the ! is present, then the contents of those blocks is displayed, too.
X.PP
XThe \fBva\fRlidate command checks certain variables for internal consistency.
XNormally it doesn't output anything unless it detects a problem.
XWith the !, though, it will always produce *some* output.
/
echo x - index.ms
sed '/^X/s///' > index.ms << '/'
X.XS 1
XINTRODUCTION
XWhat E\s-2LVIS\s+2 does,
XCopyright,
XHow to compile E\s-2LVIS\s+2,
XOverview
X.XA 2
XVISUAL MODE COMMANDS
XNormal interactive editing,
XInput mode,
XArrow keys,
XDigraphs,
XAbbreviations,
XAuto-indentation
X.XA 3
XCOLON MODE COMMANDS
XLine specifiers,
XText entry,
XCut & paste,
XDisplay text,
XGlobal operations,
XLine editing,
XUndo,
XConfiguration & status,
XMultiple files,
XSwitching files,
XWorking with a compiler,
XExiting,
XFile I/O,
XDirectory & shell,
XDebugging
X.XA 4
XREGULAR EXPRESSIONS
XSyntax,
XOptions,
XSubstitutions,
XExamples
X.XA 5
XOPTIONS
XAutoindent,
XAutoprint,
Xetc.
X.XA 6
XCUT BUFFERS
XPutting text into a cut buffer,
XPasting from a cut buffer,
XMacros,
XThe effect of switching files
X.XA 7
XDIFFERENCES BETWEEN E\s-2LVIS\s+2 AND THE REAL VI/EX
XExtensions,
XOmissions
X.XA 8
XINTERNAL
XFor programmers only,
XThe temporary file,
XImplementation of editing,
XMarks and the cursor,
XColon command interpretation,
XScreen control,
XPortability
X.XA 9
XCFLAGS
X.XA 10
XTERMCAP
X.XA 11
XENVIRONMENT VARIABLES
X.XA 12
XVERSIONS
X.XA 13
XQUESTIONS & ANSWERS
X.XE
X.PX
X.sp 0.3i
X.ce 1
XUNIX-style "man" pages appear at the end of this manual.
/
echo x - internal.ms
sed '/^X/s///' > internal.ms << '/'
X.Go 8 "INTERNAL"
X.PP
XYou don't need to know the material in this section to use \*E.
XYou only need it if you intend to modify \*E.
X.PP
XYou should also check out the CFLAGS, TERMCAP, ENVIRONMENT VARIABLES,
XVERSIONS, and QUIESTIONS & ANSWERS sections of this manual.
X.NH 2
XThe temporary file
X.PP
XThe temporary file is divided into blocks of 1024 bytes each.
XThe functions in "blk.c" maintain a cache of the five most recently used blocks,
Xto minimize file I/O.
X.PP
XWhen \*E starts up, the file is copied into the temporary file
Xby the function \fBtmpstart()\fR in "tmp.c".
XSmall amounts of extra space are inserted into the temporary file to
Xinsure that no text lines cross block boundaries.
XThis speeds up processing and simplifies storage management.
XThe extra space is filled with NUL characters.
Xthe input file must not contain any NULs, to avoid confusion.
XThis also limits lines to a length of 1023 characters or less.
X.PP
XThe data blocks aren't necessarily stored in sequence.
XFor example, it is entirely possible that the data block containing
Xthe first lines of text will be stored after the block containing the
Xlast lines of text.
X.PP
XIn RAM, \*E maintains two lists: one that describes the "proper"
Xorder of the disk blocks, and another that records the line number of
Xthe last line in each block.
XWhen \*E needs to fetch a given line of text, it uses these tables
Xto locate the data block which contains that line.
X.PP
XBefore each change is made to the file, these lists are copied.
XThe copies can be used to "undo" the change.
XAlso, the first list
X-- the one that lists the data blocks in their proper order --
Xis written to the first data block of the temp file.
XThis list can be used during file recovery.
X.PP
XWhen blocks are altered, they are rewritten to a \fIdifferent\fR block in the file,
Xand the order list is updated accordingly.
XThe original block is left intact, so that "undo" can be performed easily.
X\*E will eventually reclaim the original block, when it is no longer needed.
X.NH 2
XImplementation of Editing
X.PP
XThere are three basic operations which affect text:
X.ID
X\(bu delete text	- delete(from, to)
X\(bu add text	- add(at, text)
X\(bu yank text	- cut(from, to)
X.DE
X.PP
XTo yank text, all text between two text positions is copied into a cut buffer.
XThe original text is not changed.
XTo copy the text into a cut buffer,
Xyou need only remember which physical blocks that contain the cut text,
Xthe offset into the first block of the start of the cut,
Xthe offset into the last block of the end of the cut,
Xand what kind of cut it was.
X(Cuts may be either character cuts or line cuts;
Xthe kind of a cut affects the way it is later "put".)
XYanking is implemented in the function \fBcut()\fR,
Xand pasting is implemented in the function \fBpaste()\fR.
XThese functions are defined in "cut.c".
X.PP
XTo delete text, you must modify the first and last blocks, and
Xremove any reference to the intervening blocks in the header's list.
XThe text to be deleted is specified by two marks.
XThis is implemented in the function \fBdelete()\fR.
X.PP
XTo add text, you must specify
Xthe text to insert (as a NUL-terminated string)
Xand the place to insert it (as a mark).
XThe block into which the text is to be inserted may need to be split into
Xas many as four blocks, with new intervening blocks needed as well...
Xor it could be as simple as modifying a single block.
XThis is implemented in the function \fBadd()\fR.
X.PP
XThere is also a \fBchange()\fR function,
Xwhich generally just calls delete() and add().
XFor the special case where a single character is being replaced by another
Xsingle character, though, change() will optimize things somewhat.
XThe add(), delete(), and change() functions are all defined in "modify.c".
X.PP
XThe \fBinput()\fR function reads text from a user and inserts it into the file.
XIt makes heavy use of the add(), delete(), and change() functions.
XIt inserts characters one at a time, as they are typed.
X.PP
XWhen text is modified, an internal file-revision counter, called \fBchanges\fR,
Xis incremented.
XThis counter is used to detect when certain caches are out of date.
X(The "changes" counter is also incremented when we switch to a different file,
Xand also in one or two similar situations -- all related to invalidating caches.)
X.NH 2
XMarks and the Cursor
X.PP
XMarks are places within the text.
XThey are represented internally as 32-bit values which are split
Xinto two bitfields:
Xa line number and a character index.
XLine numbers start with 1, and character indexes start with 0.
XLines can be up to 1023 characters long, so the character index is 10 bits
Xwide and the line number fills the remaining 22 bits in the long int.
X.PP
XSince line numbers start with 1,
Xit is impossible for a valid mark to have a value of 0L.
X0L is therefore used to represent unset marks.
X.PP
XWhen you do the "delete text" change, any marks that were part of
Xthe deleted text are unset, and any marks that were set to points
Xafter it are adjusted.
XMarks are adjusted similarly after new text is inserted.
X.PP
XThe cursor is represented as a mark.
X.NH 2
XColon Command Interpretation
X.PP
XColon commands are parsed, and the command name is looked up in an array
Xof structures which also contain a pointer to the function that implements
Xthe command, and a description of the arguments that the command can take.
XIf the command is recognized and its arguments are legal,
Xthen the function is called.
X.PP
XEach function performs its task; this may cause the cursor to be
Xmoved to a different line, or whatever.
X.NH 2
XScreen Control
X.PP
XIn input mode or visual command mode,
Xthe screen is redrawn by a function called \fBredraw()\fR.
XThis function is called in the getkey() function before each keystroke is
Xread in, if necessary.
X.PP
XRedraw() write to the screen via a package which looks like the "curses"
Xlibrary, but isn't.
XIt is actually much simpler.
XMost curses operations are implemented as macros which copy characters
Xinto a large I/O buffer, which is then written with a single large
Xwrite() call as part of the refresh() operation.
X.PP
X(Note: Under MS-DOS, the pseudo-curses macros check to see whether you're
Xusing the pcbios interface.  If you are, then the macros call functions
Xin "pc.c" to implement screen updates.)
X.PP
XThe low-level functions which modify text (namely add(), delete(), and change())
Xsupply redraw() with clues to help redraw() decide which parts of the
Xscreen must be redrawn.
XThe clues are given via a function called \fBredrawrange()\fR.
X.PP
XMost EX commands use the pseudo-curses package to perform their output,
Xlike redraw().
X.PP
XThere is also a function called \fBmsg()\fR which uses the same syntax as printf().
XIn EX mode, msg() writes message to the screen and automatically adds a
Xnewline.
XIn VI mode, msg() writes the message on the bottom line of the screen
Xwith the "standout" character attribute turned on.
X.NH 2
XOptions
X.PP
XFor each option available through the ":set" command,
X\*E contains a character array variable, named "o_\fIoption\fR".
XFor example, the "lines" option uses a variable called "o_lines".
X.PP
XFor boolean options, the array has a dimension of 1.
XThe first (and only) character of the array will be NUL if the
Xvariable's value is FALSE, and some other value if it is TRUE.
XTo check the value, just by dereference the array name,
Xas in "if (*o_autoindent)".
X.PP
XFor number options, the array has a dimension of 3.
XThe array is treated as three unsigned one-byte integers.
XThe first byte is the current value of the option.
XThe second and third bytes are the lower and upper bounds of that
Xoption.
X.PP
XFor string options, the array usually has a dimension of about 60
Xbut this may vary.
XThe option's value is stored as a normal NUL-terminated string.
X.PP
XAll of the options are declared in "opts.c".
XMost are initialized to their default values;
Xthe \fBinitopts()\fR function is used to perform any environment-specific
Xinitialization.
X.NH 2
XPortability
X.PP
XTo improve portability, \*E collects as many of the system-dependent
Xdefinitions as possible into the "config.h" file.
XThis file begins with some preprocessor instructions which attempt to
Xdetermine which compiler and operating system you have.
XAfter that, it conditionally defines some macros and constants for your system.
X.PP
XOne of the more significant macros is \fBttyread()\fR.
XThis macro is used to read raw characters from the keyboard, possibly
Xwith timeout.
XFor UNIX systems, this basically reads bytes from stdin.
XFor MSDOS, TOS, and OS9, ttyread() is a function defined in curses.c.
XThere is also a \fBttywrite()\fR macro.
X.PP
XThe \fBtread()\fR and \fBtwrite()\fR macros are versions of read() and write() that are
Xused for text files.
XOn UNIX systems, these are equivelent to read() and write().
XOn MS-DOS, these are also equivelent to read() and write(),
Xsince DOS libraries are generally clever enough to convert newline characters
Xautomatically.
XFor Atari TOS, though, the MWC library is too stupid to do this,
Xso we had to do the conversion explicitly.
X.PP
XOther macros may substitute index() for strchr(), or bcopy() for memcpy(),
Xor map the "void" data type to "int", or whatever.
X.PP
XThe file "tinytcap.c" contains a set of functions that emulate the termcap
Xlibrary for a small set of terminal types.
XThe terminal-specific info is hard-coded into this file.
XIt is only used for systems that don't support real termcap.
XAnother alternative for screen control can be seen in
Xthe "curses.h" and "pc.c" files.
XHere, macros named VOIDBIOS and CHECKBIOS are used to indirectly call
Xfunctions which perform low-level screen manipulation via BIOS calls.
X.PP
XThe stat() function must be able to come up with UNIX-style major/minor/inode
Xnumbers that uniquely identify a file or directory.
X.PP
XPlease try to keep you changes localized,
Xand wrap them in #if/#endif pairs,
Xso that \*E can still be compiled on other systems.
XAnd PLEASE let me know about it, so I can incorporate your changes into
Xmy latest-and-greatest version of \*E.
/
echo x - intro.ms
sed '/^X/s///' > intro.ms << '/'
X.Go 1 "INTRODUCTION"
X.PP
X\*E is a clone of vi/ex, the standard UNIX editor.
X\*E supports nearly all of the vi/ex commands,
Xin both visual mode and colon mode.
X.PP
XLike vi/ex, \*E stores most of the text in a temporary file, instead of RAM.
XThis allows it to edit files that are too large to fit
Xin a single process' data space.
XAlso, the edit buffer can survive a power failure or crash.
X.PP
X\*E runs under BSD UNIX, AT&T SysV UNIX, Minix, MS-DOS, Atari TOS,
XCoherent, OS9/68000, VMS and AmigaDos.
XThe next version is also expected to add MS-Windows, OS/2 and MacOS.
XContact me before you start porting it to some other OS,
Xbecause somebody else may have already done it for you.
X.PP
X\*E is freely redistributable, in either source form or executable form.
XThere are no restrictions on how you may use it.
X.NH 2
XCompiling
X.PP
XSee the "Versions" section of this manual for instructions on how to compile
X\*E.
X.PP
XIf you want to port \*E to another O.S. or compiler, then
Xyou should start be reading the "Portability" part of the "Internal" section.
X.NH 2
XOverview of \*E
X.PP
XThe user interface of \*E/vi/ex is weird.
XThere are two major command modes in \*E, and a few text input modes as well.
XEach command mode has a command which allows you to switch to the other mode.
X.PP
XYou will probably use the \fIvisual command mode\fR
Xmost of the time.
XThis is the mode that \*E normally starts up in.
X.PP
XIn visual command mode, the entire screen is filled with lines of text
Xfrom your file.
XEach keystroke is interpretted as part of a visual command.
XIf you start typing text, it will \fInot\fR be inserted,
Xit will be treated as part of a command.
XTo insert text, you must first give an "insert text" command.
XThis will take some getting used to.
X(An alternative exists.
XLookup the "inputmode" option.)
X.PP
XThe \fIcolon mode\fR is quite different.
X\*E displays a ":" character on the bottom line of the screen, as a prompt.
XYou are then expected to type in a command line and hit the <Return> key.
XThe set of commands recognized in the colon mode is different
Xfrom visual mode's.
/
echo x - options.ms
sed '/^X/s///' > options.ms << '/'
X.Go 5 "OPTIONS"
X.PP
XOptions may be set or examined via the colon command "set".
XThe values of options will affect the operation of later commands.
X.PP
XFor convenience, options have both a long descriptive name and a short name
Xwhich is easy to type.
XYou may use either name interchangably.
XI like the short names, myself.
X.PP
XThere are three types of options: Boolean, string, and numeric.
XBoolean options are made TRUE by giving the name of the option as an
Xargument to the "set" command;
Xthey are made FALSE by prefixing the name with "no".
XFor example, "set autoindent" makes the autoindent option TRUE,
Xand "set noautoindent" makes it FALSE.
X\*E also allows boolean options to be toggled by prefixing the name with "neg".
XSo, ":map g :set neglist^M" will cause the <g> key to alternately toggle the
X"list" option on and off.
X(The "neg" prefix is an extension; the real vi doesn't support it.)
X.PP
XTo change the value of a string or numeric option, pass the "set" command
Xthe name of the option, followed by an "=" sign and the option's new value.
XFor example, "set tabstop=8" will give the tabstop option a value of 8.
XFor string options, you may enclose the new value in quotes.
X.LD
X.ta 1.9i 2.4i 3.8i
X.ps +2
X\fBNAMES	TYPE	DEFAULT	MEANING\fP
X.ps
Xautoindent, ai	Bool	noai	auto-indent during input
Xautoprint, ap	Bool	ap	in EX, print the current line
Xautotab, at	Bool	at	auto-indent allowed to use tabs?
Xautowrite, aw	Bool	noaw	auto-write when switching files
Xbeautify,  bf	Bool	nobf	strip control chars from file?
Xcharattr, ca	Bool	noca	interpret \\fX sequences?
Xcc, cc	Str	cc="cc -c"	name of the C compiler
Xcolumns, co	Num	co=80	width of the screen
Xdigraph, dig	Bool	nodig	recognize digraphs?
Xdirectory, dir	Str	dir="/usr/tmp"	where tmp files are kept
Xedcompatible, ed	Bool	noed	remember ":s//" options
Xequalprg, ep	Bool	ep="fmt"	program to run for = operator
Xerrorbells, eb	Bool	eb	ring bell on error
Xexrc, exrc	Bool	noexrc	read "./.exrc" file?
Xexrefresh, er	Bool	er	write lines indiviually in EX
Xflash, vbell	Bool	flash	use visible alternative to bell
Xflipcase, fc	Str	fc=""	non-ASCII chars flipped by ~
Xhideformat, hf	Bool	hf	hide text formatter commands
Xignorecase, ic	Bool	noic	upper/lowercase match in search
Xinputmode, im	Bool	noim	start vi in insert mode?
Xkeytime, kt	Num	kt=2	timeout for mapped key entry
Xkeywordprg, kp	Str	kp="ref"	full pathname of shift-K prog
Xlines, ln	Num	ln=25	number of lines on the screen
Xlist, li	Bool	noli	display lines in "list" mode
Xmagic, ma	Bool	ma	use regular expression in search
Xmake, mk	Str	mk="make"	name of the "make" program
Xmesg, ms	Bool	ms	allow messages from other users?
Xmodelines, ml	Bool	noml	are modelines processed?
Xmore, more	Bool	more	pause between messages?
Xnovice, nov	Bool	nonovice	set options for ease of use
Xparagraphs, para	Str	para="PPppIPLPQP"	names of "paragraph" nroff cmd
Xprompt, pr	Bool	pr	show ':' prompt in \fIex\fR mode
Xreadonly, ro	Bool	noro	prevent overwriting of orig file
Xremap, rem	Bool	remap	allow key maps to call key maps
Xreport, re	Num	re=5	report when 5 or more changes
Xruler, ru	Bool	noru	display line/column numbers
Xscroll, sc	Num	sc=12	scroll amount for ^U and ^D
Xsections, sect	Str	sect="NHSHSSSEse"	names of "section" nroff cmd
Xshell, sh	Str	sh="/bin/sh"	full pathname of the shell
Xshowmatch, sm	Bool	nosm	show matching ()[]{}
Xshowmode, smd	Bool	nosmd	say when we're in input mode
Xshiftwidth, sw	Num	sw=8	shift amount for < and >
Xsidescroll, ss	Num	ss=8	amount of sideways scrolling
Xsync, sy	Bool	nosy	call sync() often
Xtabstop, ts	Num	ts=8	width of tab characters
Xtaglength, tl	Num	tl=0	significant chars in tag name
Xterm, te	Str	te="$TERM"	name of the termcap entry
Xterse, tr	Bool	notr	give shorter error messages
Xtimeout, to	Bool	to	distinguish <esc> from <arrow>?
Xwarn, wa	Bool	wa	warn for ! if file modified
Xwindow, wi	Num	wi=24	lines to redraw after long move
Xwrapmargin, wm	Num	wm=0	wrap long lines in input mode
Xwrapscan, ws	Bool	ws	at EOF, searches wrap to line 1
Xwriteany, wr	Bool	nowr	allow :w to clobber files
X.DE
X.TA
X.ne 6
X.IP "autoindent, ai"
XDuring input mode, the autoindent option will cause each added line
Xto begin with the same amount of leading whitespace as the line above it.
XWithout autoindent, added lines are initially empty.
X.IP "autoprint, ap"
XThis option only affects EX mode.
XIf the autoprint option on,
Xand either the cursor has moved to a different line
Xor the previous command modified the file,
Xthen \*E will print the current line.
X.IP "autotab, at"
XThis option affects the behaviour of the autoindent mode.
XIf autoindent is turned off, then autotab has no effect.
X.IP
XWhen autotab is turned on, elvis will use a mixture of spaces and tabs
Xto create the proper amount of indentation.
XThis is the default.
X.IP
XWhen autotab is turned off, elvis will only use spaces for auto-indent.
X\*E will still insert a real tab character when you hit the <Tab> key, though;
Xthe autotab option only affects \fIautomatic\fR indentation.
X.IP "autowrite, aw"
XWhen you're editing one file and decide to switch to another
X\- via the :tag command, or :next command, perhaps \-
Xif your current file has been modified,
Xthen \*E will normally print an error message and refuse to switch.
X.IP
XHowever, if the autowrite option is on,
Xthen \*E will write the modified version of the current file
Xand successfully switch to the new file.
X.IP "beautify, bf"
XThis option causes all control characters to be deleted from the text file,
Xat the time when you start editing it.
XIf you're already editing a file when you turn on the beautify option,
Xthen that file won't be affected.
X.IP cc
XThe :cc command runs the C compiler.
XThis option should be set to the name of your compiler.
X.IP "charattr, ca"
XMany text formatting programs allow you to designate portions of
Xyour text to be underlined, italicized, or boldface by embedding
Xthe special strings \\fU, \\fI, and \\fB in your text.
XThe special string \\fP marks the end of underlined or boldface text.
X.IP
X\*E normally treats those special strings just like any other text.
X.IP
XHowever, if the charattr option is on, then \*E will interpret
Xthose special strings correctly,
Xto display underlined or boldface text on the screen.
X(This only works, of course, if your terminal can display
Xunderlined and boldface, and if the TERMCAP entry says how to do it.)
X.IP "columns, co"
XThis option shows how wide your screen is.
X.IP "digraph, dig"
XThis option is used to enable/disable recognition of digraphs.
XThe default value is nodigraph, which means that digraphs will not be
Xrecognized.
X.IP "directory, dir"
X\*E stores text in temporary files.
XThis option allows you to control which directory those temporary files will
Xappear in.
XThe default is /usr/tmp.
X.IP
XThis option can only be set in a .exrc file;
Xafter that, \*E will have already started making temporary files
Xin some other directory, so it would be too late.
X.IP "edcompatible, ed"
XThis option affects the behaviour of the ":s/regexp/text/options" command.
XIt is normally off (:se noed) which causes all of the substitution options
Xto be off unless explicitly given.
X.IP
XHowever, with edcompatible on (:se ed), the substitution command remembers
Xwhich options you used last time.
XThose same options will continue to be used until you change them.
XIn edcompatible mode, when you explicitly give the name of a
Xsubstitution option, you will toggle the state of that option.
X.IP
XThis all seems very strange to me, but its implementation was almost free
Xwhen I added the ":&" command to repeat the previous substitution,
Xso there it is.
X.IP "equalprg, ep"
XThis holds the name & arguments of the external filter program
Xused the the visual = operator.
XThe defualt value is "fmt",
Xso the = operator will adjust line breaks in text.
X.IP "errorbells, eb"
X\*E normally rings a bell when you do something wrong.
XThis option lets you disable the bell.
X.IP exrc
XThis option specifies whether a .exrc file in the current directory
Xshould be executed.
XBy default, this option is off (":set noexrc") which prevents elvis from
Xexecuting .exrc in the current directory.
XIf the .exrc file in your home directory turns this option on (":set exrc")
Xthen the \*E will attempt to execute the .exrc file in the current directory.
X.IP
XThis option exist mainly for security reasons.
XA mean-spirited person could do something like
X.br
X	echo >/tmp/.exrc '!rm -rf $HOME'
X.br
Xand then anybody who attempted to edit or view a file in the /tmp directory
Xwould lose most of their files.
XWith the exrc option turned off, this couldn't happen to you.
X.IP "exrefresh, er"
XThe EX mode of \*E writes many lines to the screen.
XYou can make \*E either write each line to the screen separately,
Xor save up many lines and write them all at once.
X.IP
XThe exrefresh option is normally on, so each line is written to the
Xscreen separately.
X.IP
XYou may wish to turn the exrefresh option off (:se noer) if the
X"write" system call is costly on your machine, or if you're using a
Xwindowing environment.
X(Windowing environments scroll text a lot faster when you write
Xmany lines at once.)
X.IP
XThis option has no effect in visual command mode or input mode.
X.IP "flash, vbell"
XIf your termcap entry describes a visible alternative to ringing
Xyour terminal's bell, then this option will say whether the visible
Xversion gets used or not.
XNormally it will be.
X.IP
XIf your termcap does NOT include a visible bell capability,
Xthen the flash option will be off, and you can't turn it on.
X.IP "flipcase, fc"
XThe flipcase option allows you to control how the non-ASCII characters are
Xaltered by the "~" command.
X.IP
XThe string is divided into pairs of characters.
XWhen "~" is applied to a non-ASCII character,
X\*E looks up the character in the flipcase string to see which pair it's in,
Xand replaces it by the other character of the pair.
X.IP "hideformat, hf"
XMany text formatters require you to embed format commands in your text,
Xon lines that start with a "." character.
X\*E normally displays these lines like any other text,
Xbut if the hideformat option is on,
Xthen format lines are displayed as blank lines.
X.IP "ignorecase, ic"
XNormally, when \*E searches for text, it treats uppercase letters
Xas being different for lowercase letters.
X.IP
XWhen the ignorecase option is on, uppercase and lowercase are treated as equal.
X.IP "inputmode, im"
XThis option allows you to have \*E start up in insert mode.
XYou can still exit insert mode at any time by hitting the ESC key, as usual.
XUsually, this option would be set in your ".exrc" file.
X.IP "keytime, kt"
XThe arrow keys of most terminals send a multi-character sequence.
XIt takes a measurable amount of time for these sequences to be transmitted.
XThe keytime option allows you to control the maximum amount of time
Xto allow for an arrow key (or other mapped key) to be received in full.
X.IP
XOn most systems, the setting is the number of tenths of a second to allow
Xbetween characters.
XOn some other systems, the setting is in whole seconds.
X.IP
XTry to avoid setting keytime=1.
XMost systems just count clock beats, so if you tried to read a character
Xshortly before a clock beat, you could allow almost no time at all for
Xreading the characters.
XFor higher keytime settings, the difference is less critical.
X.IP
XIf your system's response time is poor, you might want to increase the keytime.
XIn particular, I've found that when keystrokes must be sent through a network
X(via X windows, rlogin, or telnet, for example) the keytime should be set to
Xat least 1 second.
X.IP
XAs a special case,
Xyou can set keytime to 0 to disable this time limit stuff altogether.
XThe big problem here is:
XIf your arrow keys' sequences start with an ESC,
Xthen every time you hit your ESC key \*E will wait... and wait...
Xto see if maybe that ESC was part of an arrow key's sequence.
X.IP
XNOTE: this option is a generalization of the timeout option of the real vi.
X.IP "keywordprg, kp"
X\*E has a special keyword lookup feature.
XYou move the cursor onto a word, and hit shift-K,
Xand \*E uses another program to look up the word
Xand display information about it.
X.IP
XThis option says which program gets run.
X.IP
XThe default value of this option is "ref",
Xwhich is a program that looks up the definition of a function in C.
XIt looks up the function name in a file called "refs" which is created by ctags.
X.IP
XYou can subtitute other programs, such as an English dictionary program
Xor the online manual.
X\*E runs the program, using the keyword as its only argument.
XThe program should write information to stdout.
XThe program's exit status should be 0, unless you want \*E to print
X"<<< failed >>>".
X.IP "lines, ln"
XThis option says how many lines you screen has.
X.IP "list, li"
XIn nolist mode (the default), \*E displays text in a "normal" manner
X-- with tabs expanded to an appropriate number of spaces, etc.
X.IP
XHowever, sometimes it is useful to have tab characters displayed differently.
XIn list mode, tabs are displayed as "^I",
Xand a "$" is displayed at the end of each line.
X.IP "magic, ma"
XThe search mechanism in \*E can accept "regular expressions"
X-- strings in which certain characters have special meaning.
X.IP
XThe magic option is normally on, which causes these characters to be treated
Xspecially.
X.IP
XIf you turn the magic option off (:se noma),
Xthen all characters except ^ and $ are treated literally.
X^ and $ retain their special meanings regardless of the setting of magic.
X.IP "make, mk"
XThe :make command runs your "make" program.
XThis option defines the name of your "make" program.
X.IP mesg
XWith the real vi, running under real UNIX,
X":set nomesg" would prevent other users from sending you messages.
X\*E ignores it, though.
X.IP "modelines, ml"
X\*E supports modelines.
XModelines are lines near the beginning or end of your text file which
Xcontain "ex:yowza:",
Xwhere "yowza" is any EX command.
XA typical "yowza" would be something like "set ts=5 ca kp=spell wm=15".
XOther text may also appear on a modeline,
Xso you can place the "ex:yowza:" in a comment:
X.br
X.ID
X/* ex:set sw=4 ai: */
X.DE
X.IP
XNormally these lines are ignored, for security reasons,
Xbut if you have "set modelines" in your .exrc file
Xthen "yowza" is executed.
X.IP "novice, nov"
XThe command ":set novice" is equivelent to ":set nomagic report=1 showmode".
X.IP "paragraphs, pa"
XThe { and } commands move the cursor forward or backward in increments
Xof one paragraph.
XParagraphs may be separated by blank lines, or by a "dot" command of
Xa text formatter.
XDifferent text formatters use different "dot" commands.
XThis option allows you to configure \*E to work with your text formatter.
X.IP
XIt is assumed that your formatter uses commands that start with a
X"." character at the front of a line,
Xand then have a one- or two-character command name.
X.IP
XThe value of the paragraphs option is a string in which each pair
Xof characters is one possible form of your text formatter's paragraph
Xcommand.
X.IP "more"
XWhen \*E must display a sequence of messages at the bottom line of the screen
Xin visual mode, it normally pauses after all but the last one, so you have
Xtime to read them all.
X.IP
XIf you turn off the "more" option, then \*E will not pause.
XThis means you can only read the last message, but it is usually the most
Ximportant one anyway.
X.IP "prompt, pr"
XIf you ":set noprompt", then \*E will no longer emit a ':' when it
Xexpects you to type in an \fIex\fR command.
XThis is slightly useful if you're using an astonishingly slow UNIX machine,
Xbut the rest of us can just ignore this one.
X.IP "readonly, ro"
XNormally, \*E will let you write back any file to which you have
Xwrite permission.
XIf you don't have write permission, then you can only write the changed
Xversion of the file to a \fIdifferent\fP file.
X.IP
XIf you set the readonly option,
Xthen \*E will pretend you don't have write permission to \fIany\fP file you edit.
XIt is useful when you really only mean to use \*E to look at a file,
Xnot to change it.
XThis way you can't change it accidentally.
X.IP
XThis option is normally off, unless you use the "view" alias of \*E.
X"View" is like "vi" except that the readonly option is on.
X.IP "remap"
XThe ":map" command allows you to convert one key sequence into another.
XThe remap option allows you to specify what should happen if portions of
Xthat other sequence are also in the map table.
XIf remap is on, then those portions will also be mapped, just as if they
Xhad been typed on the keyboard.
XIf remap is off, then the matching portions will not be mapped.
X.IP
XFor example, if you enter the commands ":map A B" and ":map B C",
Xthen when remap is on, A will be converted to C.
XBut when remap is off, A will be converted only to B.
X.IP "report, re"
XCommands in \*E may affect many lines.
XFor commands that affect a lot of lines, \*E will output a message saying
Xwhat was done and how many lines were affected.
XThis option allows you to define what "a lot of lines" means.
XThe default is 5, so any command which affects 5 or more lines will cause
Xa message to be shown.
X.IP "ruler, ru"
XThis option is normally off.
XIf you turn it on, then \*E will constantly display the line/column numbers
Xof the cursor, at the bottom of the screen.
X.IP "scroll, sc"
XThe ^U and ^D keys normally scroll backward or forward by half a screenful,
Xbut this is adjustable.
XThe value of this option says how many lines those keys should scroll by.
XIf you invoke ^U or ^D with a count argument (for example, "33^D") then
Xthis option's value is set to the count.
X.IP "sections, se"
XThe [[ and ]] commands move the cursor backward or forward in increments of
X1 section.
XSections may be delimited by a { character in column 1
X(which is useful for C source code)
Xor by means of a text formatter's "dot" commands.
X.IP
XThis option allows you to configure \*E to work with your text formatter's
X"section" command, in exectly the same way that the paragraphs option makes
Xit work with the formatter's "paragraphs" command.
X.IP "shell, sh"
XWhen \*E forks a shell
X(perhaps for the :! or :shell commands)
Xthis is the program that is uses as a shell.
XThis is "/bin/sh" by default,
Xunless you have set the SHELL (or COMSPEC, for MS-DOS) environment variable,
Xit which case the default value is copied from the environment.
X.IP "shiftwidth, sw"
XThe < and > commands shift text left or right by some uniform number of columns.
XThe shiftwidth option defines that "uniform number".
XThe default is 8.
X.IP "showmatch, sm"
XWith showmatch set,
Xin input mode every time you hit one of )}],
X\*E will momentarily move the cursor to the matching ({[.
X.IP "showmode, smd"
XIn visual mode, it is easy to forget whether you're in the visual command mode
Xor input/replace mode.
XNormally, the showmode option is off, and you haven't a clue as to which mode
Xyou're in.
XIf you turn the showmode option on, though, a little message will appear in the
Xlower right-hand corner of your screen, telling you which mode you're in.
X.IP "sidescroll, ss"
XFor long lines, \*E scrolls sideways.
X(This is different from the real vi,
Xwhich wraps a single long line onto several rows of the screen.)
X.IP
XTo minimize the number of scrolls needed,
X\*E moves the screen sideways by several characters at a time.
XThe value of this option says how many characters' widths to scroll at a time.
X.IP
XGenerally, the faster your screen can be redrawn,
Xthe lower the value you will want in this option.
X.IP "sync, sy"
XIf the system crashes during an edit session, then most of your work
Xcan be recovered from the temporary file that \*E uses to store
Xchanges.
XHowever, sometimes the OS will not copy changes to the
Xhard disk immediately, so recovery might not be possible.
XThe [no]sync option lets you control this.
X.IP
XIn nosync mode (which is the default, for UNIX), \*E lets the operating system
Xcontrol when data is written to the disk.
XThis is generally faster.
X.IP
XIn sync mode (which is the default for MS-DOS, AmigaDos, and Atari TOS),
X\*E forces all changes out
Xto disk every time you make a change.
XThis is generally safer, but slower.
XIt can also be a rather rude thing to do on a multi-user system.
X.IP "tabstop, ts"
XTab characters are normally 8 characters wide,
Xbut you can change their widths by means of this option.
X.IP "taglength, tl"
XThis option allows you to specify how many characters of a tag's name
Xmust match when performing tag lookup.
XAs a special case, ":set taglength=0" means that all characters of a tag's
Xname must match.
X.IP
XNote: some configurations of \*E don't support this option.
X.IP "term, te"
XThis read-only option shows the name of the termcap entry that
X\*E is using for your terminal.
X.IP "terse, tr"
XThe real vi uses this option to select longer vs. shorter error messages.
X\*E has only one set of error messages, though, so this option has no effect.
X.IP "timeout, to"
XThe command ":set notimeout" is equivelent to ":set keytime=0",
Xand ":set timeout" is equivelent to ":set keytime=1".
XThis affects the behaviour of the <Esc> key.
XSee the discussion of the "keytime" option for more information.
X.IP "warn, wa"
XIf you have modified a file but not yet written it back to disk, then
X\*E will normally print a warning before executing a ":!cmd" command.
XHowever, in nowarn mode, this warning is not given.
X.IP
X\*E also normally prints a message after a successful search that
Xwrapped at EOF.
XThe [no]warn option can also disable this warning.
X.IP "window, wi"
XThis option controls how many lines are redrawn after a long move.
X.IP
XOn fast terminals, this is usually set to the number of rows that the
Xterminal can display, minus one.
XThis causes the entire screen to be filled with text around the cursor.
X.IP
XOn slow terminals, you may wish to reduce this value to about 7 or so.
XThat way, if you're doing something like repeatedly hitting 'n' to search
Xfor each occurrence of some string and trying to find a particular occurrence,
Xthen you don't need to wait as long for \*E to redraw the screen after each
Xsearch.
X.IP "wrapmargin, wm"
XNormally (with wrapmargin=0) \*E will let you type in extremely long
Xlines, if you wish.
X.IP
XHowever, with warpmargin set to something other that 0 (wrapmargin=10
Xis nice), \*E will automatically cause long lines to be "wrapped"
Xon a word break for lines come too close to the right-hand margin.
XFor example: On an 80-column screen, ":set wm=10" will cause lines to
Xwrap when their length exceeds 70 columns.
X.IP "wrapscan, ws"
XNormally, when you search for something, \*E will find it no matter
Xwhere it is in the file.
X\*E starts at the cursor position, and searches forward.
XIf \*E hits EOF without finding what you're looking for,
Xthen it wraps around to continue searching from line 1.
XIf you turn off the wrapscan option (:se nows),
Xthen when \*E hits EOF during a search, it will stop and say so.
X.IP "writeany, wr"
XWith "writeany" turned off, elvis will prevent you from accidentally
Xoverwriting a file.
XFor example, if "foo" exists then ":w foo" will fail.
XIf you turn on the "writeany" option, then ":w foo" will work.
X.IP
XRegardless of the setting of "writeany", though, ":w! foo" will work.
XThe '!' forces the ":w" command to write the file unless the operating system
Xwon't allow it.
/
echo x - question.ms
sed '/^X/s///' > question.ms << '/'
X.nr Qn 0 1
X.de QQ
X.sp
X.IP \fB\\n+(Qn) 0.3i
X..
X.de AA
X.IP \fR 0.75i
X..
X.Go 13 "QUESTIONS & ANSWERS"
X.QQ
XHow can I make elvis run faster under DOS?
X.AA
XThere are several things you can do.
XThe first thing to do is get a good screen driver such as NANSI.SYS.
XThis can speed up screen redrawing by as much as a factor of eight!
XThe DOS-specific part of section 12 tells you how to do this.
X.AA
XYou might also consider reducing the size of the blocks that elvis uses.
XYou'll need to recompile \*E to do this.
XThe default BLKSIZE is 1024 byte for the DOS version of \*E, which means
Xthat for each keystroke that you insert, elvis must shift an average of
Xabout 500 bytes.
XThat's a lot to ask from a little old 5MHz 8088.
XA BLKSIZE of 512 bytes might be more appropriate.
X.AA
XIf you're \fIreally\fR desperate for more speed, you might want to make
X\*E store its temporary files on a RAM disk.
XHowever, this limits the size of the file you can edit, and it eliminates any
Xchance you may have had to recover your work after a power failure
Xor system crash, but it might be worth it; you decide.
XTo do this, add ":set dir=R:\\" (or whatever your RAM disk's name is)
Xto the \fIelvis.rc\fP file.
X.AA
XNext, consider turning off the "sync" option.
XWhen the sync option is turned on, \*E will close the temporary file
Xand reopen it after every change, in order to force DOS to update
Xthe file's directory entry.
XIf you put ":set nosync" into the \fIelvis.rc\fP file, then elvis will
Xonly close the file when you start editing a different text file, or
Xwhen you're exiting \*E.
XConsequently, there is no chance that you'll be able to recover your
Xchanges after a power failure... so if you're going to this, then you
Xmight as well store the temp files on the RAM disk, too.
X.QQ
XWhere's the <Esc> key on a DEC keyboard?
X.AA
XI don't know.  Maybe the <F11> key?
XYou could always use ":map!" to make some other key act like the <Esc> key.
XIf all else fails, use <Control><[>.
X.QQ
XIs there a way to show which keys do what?
X.AA
XYes.  The command ":map" will show what each key does in command mode,
Xand ":map!" (with an exclamation mark) shows what each key does in
Xinput mode.
X.AA
XThe table is divided into three columns: the key's label, the characters
Xthat it sends, and the characters that \*E pretends you typed.
X.QQ
XHow can I make \*E display long lines like the real vi?
X.AA
XYou can't yet.
XThe next version of \*E shouldsupport this, though.
X.QQ
XI can't recover my text [under MS-DOS or Atari TOS].
XAccording to the directory listing, the temporary file is 0 bytes long.
XWhat went wrong?
X.AA
XMS-DOS and TOS only update a file's directory entry when the file is closed.
XIf the system crashes while the file is still open, then the file's length
Xis stored as 0 bytes.
XThe ":set sync" option is supposed to prevent this;
Xyou probably turned it off in the interest of speed, right?
X.AA
XUnder MS-DOS [I don't know about TOS], you should delete the empty
Xtemporary file, and then run CHKDSK/F.
XThis \fImight\fP find the data that belonged in the empty file,
Xand place it in a new file with a name like "000001.CHK" -- something like that.
XYou can then try to extract the text from that temporary file by giving the
Xcommand "elvprsv -R 000001.chk >goodnews.txt".
XIf you're lucky, then your text might be in GOODNEWS.TXT.
X.QQ
XWhat is the most current version of \*E?
X.AA
XEach version of \*E that is released to the public has a version number
Xof the form "number point number".
XAs I write this, the most current version of elvis is 1.5.
X.AA
XThe intermediate steps between one release and the next are labeled with
Xthe \fInext\fP version number, with a letter appended.
XFor example, after 1.4 was released, I started working on 1.5a.
XI am currently working on 2.0a.
XWhen \*E reaches a stable state, I'll call it 2.0 and release it.
X.AA
XSometimes a beta-test version of elvis will be available via anonymous FTP
Xfrom m2xenix.psg.com, in the directory "pub/elvis/beta".
X.QQ
XI only got executables, but now I want the source code.
XWhere can I get it?
X.AA
XIf you have access to the Internet, then you should be able to fetch it
Xfrom one of the public archives such as \fBplains.nodak.edu\fP.
XIt is accessible via anonymous FTP, or via an email server named
X"archive-server@plains.nodak.edu".
XElvis is located in the directory "/pub/Minix/all.contrib".
X.AA
XI will also offer it to the C Users' Group.
XThey sell C source code for us$8 per diskette
X(or slightly more outside  North  America).
XTheir phone number is (913) 841-1631,
Xand their address is:
X.ID
XThe C Users' Group
XPO Box 3127
XLawrence KS 66046-0127
X.DE
X.QQ
XIs this shareware, or public domain, or what?
X.AA
XIt is not public domain; it is copyrighted by me, Steve Kirkendall.
XHowever, this particular version is freely redistributable, in either
Xsource form or executable form.
X(I would prefer that you give copies away for free, complete with the
Xfull source code... but I'm not going to force you.)
X.AA
XIt is not shareware; you aren't expected to send me anything.
XYou can use it without guilt.
X.AA
XIt is not "copylefted."
XI hold a copyright, but currently I have not added any of the usual restrictions
Xthat you would find on copylefted software.
XIf people start doing really obnoxious things to \*E, then I will start
Xadding restrictions to subsequent versions, but earlier versions won't
Xbe affected.
X(So far, everybody has been pretty good about this so no restrictions
Xhave been necessary.)
X.QQ
XCan I reuse parts of your source code?
X.AA
XYes.  Please be careful, though, to make sure that the code really is mine.
XSome of the code was contributed by other people, and I don't have the
Xauthority to give you permission to use it.
XThe author's name can be found near the top of each source file.
XIf it says "Steve Kirkendall" then you may use it;
Xotherwise, you'd better contact the author first.
X.AA
XPlease don't remove my name from the source code.
XIf you modify the source, please make a note of that fact in a comment
Xnear the top of the source code.
XAnd, finally, please mention my name in your documentation.
X.QQ
XCan \*E work with non-ASCII files?
X.AA
X\*E can't edit binary files because it can't handle the NUL character,
Xand because of line-length limitations.
XHowever, it is 8-bit clean so you should be able to edit any European
Xextended ASCII file without any surprises.
X.AA
X\*E has also been modified to work with 16-bit character sets.
XYongguang Zhang (ygz@cs.purdue.edu) has created a Chinese version of \*E
Xthat uses 16-bit characters and runs under cxterm (Chinese X-term)
Xon X-windows systems.
XJunichiro Itoh (itojun@foretune.co.jp) has modified \*E to edit Japanese
Xtext under MS-DOS.
/
echo x - regexp.ms
sed '/^X/s///' > regexp.ms << '/'
X.Go 4 "REGULAR EXPRESSIONS"
X
X.PP
X\*E uses regular expressions for searching and substututions.
XA regular expression is a text string in which some characters have
Xspecial meanings.
XThis is much more powerful than simple text matching.
X.SH
XSyntax
X.PP
X\*E' regexp package treats the following one- or two-character
Xstrings (called meta-characters) in special ways:
X.IP "\\\\\\\\(\fIsubexpression\fP\\\\\\\\)" 0.8i
XThe \\( and \\) metacharacters are used to delimit subexpressions.
XWhen the regular expression matches a particular chunk of text,
X\*E will remember which portion of that chunk matched the \fIsubexpression\fP.
XThe :s/regexp/newtext/ command makes use of this feature.
X.IP "^" 0.8i
XThe ^ metacharacter matches the beginning of a line.
XIf, for example, you wanted to find "foo" at the beginning of a line,
Xyou would use a regular expression such as /^foo/.
XNote that ^ is only a metacharacter if it occurs
Xat the beginning of a regular expression;
Xanyplace else, it is treated as a normal character.
X.IP "$" 0.8i
XThe $ metacharacter matches the end of a line.
XIt is only a metacharacter when it occurs at the end of a regular expression;
Xelsewhere, it is treated as a normal character.
XFor example, the regular expression /$$/ will search for a dollar sign at
Xthe end of a line.
X.IP "\\\\\\\\<" 0.8i
XThe \\< metacharacter matches a zero-length string at the beginning of
Xa word.
XA word is considered to be a string of 1 or more letters and digits.
XA word can begin at the beginning of a line
Xor after 1 or more non-alphanumeric characters.
X.IP "\\\\\\\\>" 0.8i
XThe \\> metacharacter matches a zero-length string at the end of a word.
XA word can end at the end of the line
Xor before 1 or more non-alphanumeric characters.
XFor example, /\\<end\\>/ would find any instance of the word "end",
Xbut would ignore any instances of e-n-d inside another word
Xsuch as "calendar".
X.IP "\&." 0.8i
XThe . metacharacter matches any single character.
X.IP "[\fIcharacter-list\fP]" 0.8i
XThis matches any single character from the \fIcharacter-list\fP.
XInside the \fIcharacter-list\fP, you can denote a span of characters
Xby writing only the first and last characters, with a hyphen between
Xthem.
XIf the \fIcharacter-list\fP is preceded by a ^ character, then the
Xlist is inverted -- it will match character that \fIisn't\fP mentioned
Xin the list.
XFor example, /[a-zA-Z]/ matches any letter, and /[^ ]/ matches anything
Xother than a blank.
X.IP "\\\\\\\\{\fIn\fP\\\\\\\\}" 0.8i
XThis is a closure operator,
Xwhich means that it can only be placed after something that matches a
Xsingle character.
XIt controls the number of times that the single-character expression
Xshould be repeated.
X.IP "" 0.8i
XThe \\{\fIn\fP\\} operator, in particular, means that the preceding
Xexpression should be repeated exactly \fIn\fP times.
XFor example, /^-\\{80\\}$/ matches a line of eighty hyphens, and
X/\\<[a-zA-Z]\\{4\\}\\>/ matches any four-letter word.
X.IP "\\\\\\\\{\fIn\fP,\fIm\fP\\\\\\\\}" 0.8i
XThis is a closure operator which means that the preceding single-character
Xexpression should be repeated between \fIn\fP and \fIm\fP times, inclusive.
XIf the \fIm\fP is omitted (but the comma is present) then \fIm\fP is
Xtaken to be inifinity.
XFor example, /"[^"]\\{3,5\\}"/ matches any pair of quotes which contains
Xthree, four, or five non-quote characters.
X.IP "*" 0.8i
XThe * metacharacter is a closure operator which means that the preceding
Xsingle-character expression can be repeated zero or more times.
XIt is equivelent to \\{0,\\}.
XFor example, /.*/ matches a whole line.
X.IP "\\\\\\\\+" 0.8i
XThe \\+ metacharacter is a closure operator which means that the preceding
Xsingle-character expression can be repeated one or more times.
XIt is equivelent to \\{1,\\}.
XFor example, /.\\+/ matches a whole line, but only if the line contains
Xat least one character.
XIt doesn't match empty lines.
X.IP "\\\\\\\\?" 0.8i
XThe \\? metacharacter is a closure operator which indicates that the
Xpreceding single-character expression is optional -- that is, that it
Xcan occur 0 or 1 times.
XIt is equivelent to \\{0,1\\}.
XFor example, /no[ -]\\?one/ matches "no one", "no-one", or "noone".
X.PP
XAnything else is treated as a normal character which must exactly match
Xa character from the scanned text.
XThe special strings may all be preceded by a backslash to
Xforce them to be treated normally.
X.SH
XSubstitutions
X.PP
XThe :s command has at least two arguments: a regular expression,
Xand a substitution string.
XThe text that matched the regular expression is replaced by text
Xwhich is derived from the substitution string.
X.br
X.ne 15 \" so we don't mess up the table
X.PP
XMost characters in the substitution string are copied into the
Xtext literally but a few have special meaning:
X.LD
X.ta 0.75i 1.3i
X	&	Insert a copy of the original text
X	~	Insert a copy of the previous replacement text
X	\\1	Insert a copy of that portion of the original text which
X		matched the first set of \\( \\) parentheses
X	\\2-\\9	Do the same for the second (etc.) pair of \\( \\)
X	\\U	Convert all chars of any later & or \\# to uppercase
X	\\L	Convert all chars of any later & or \\# to lowercase
X	\\E	End the effect of \\U or \\L
X	\\u	Convert the first char of the next & or \\# to uppercase
X	\\l	Convert the first char of the next & or \\# to lowercase
X.TA
X.DE
X.PP
XThese may be preceded by a backslash to force them to be treated normally.
XIf "nomagic" mode is in effect,
Xthen & and ~ will be treated normally,
Xand you must write them as \\& and \\~ for them to have special meaning.
X.SH
XOptions
X.PP
X\*E has two options which affect the way regular expressions are used.
XThese options may be examined or set via the :set command.
X.PP
XThe first option is called "[no]magic".
XThis is a boolean option, and it is "magic" (TRUE) by default.
XWhile in magic mode, all of the meta-characters behave as described above.
XIn nomagic mode, only ^ and $ retain their special meaning.
X.PP
XThe second option is called "[no]ignorecase".
XThis is a boolean option, and it is "noignorecase" (FALSE) by default.
XWhile in ignorecase mode, the searching mechanism will not distinguish between
Xan uppercase letter and its lowercase form.
XIn noignorecase mode, uppercase and lowercase are treated as being different.
X.PP
XAlso, the "[no]wrapscan" option affects searches.
X.SH
XExamples
X.PP
XThis example changes every occurence of "utilize" to "use":
X.sp
X.ti +1i
X:%s/utilize/use/g
X.PP
XThis example deletes all whitespace that occurs at the end of a line anywhere
Xin the file.
X(The brackets contain a single space and a single tab.):
X.sp
X.ti +1i
X:%s/[   ]\\+$//
X.PP
XThis example converts the current line to uppercase:
X.sp
X.ti +1i
X:s/.*/\\U&/
X.PP
XThis example underlines each letter in the current line,
Xby changing it into an "underscore backspace letter" sequence.
X(The ^H is entered as "control-V backspace".):
X.sp
X.ti +1i
X:s/[a-zA-Z]/_^H&/g
X.PP
XThis example locates the last colon in a line,
Xand swaps the text before the colon with the text after the colon.
XThe first \\( \\) pair is used to delimit the stuff before the colon,
Xand the second pair delimit the stuff after.
XIn the substitution text, \\1 and \\2 are given in reverse order
Xto perform the swap:
X.sp
X.ti +1i
X:s/\\(.*\\):\\(.*\\)/\\2:\\1/
/
echo x - termcap.ms
sed '/^X/s///' > termcap.ms << '/'
X.Go 10 "TERMCAP"
X.PP
X\*E uses fairly standard termcap fields for most things.
XI invented the cursor shape names
Xbut other than that there should be few surprises.
X.SH
XRequired numeric fields
X.if n .ul 0
X.ID
X:co#:	number of columns on the screen (chars per line)
X:li#:	number of lines on the screen
X.DE
X.SH
XRequired string fields
X.ID
X.if n .ul 0
X:ce=:	clear to end-of-line
X:cl=:	home the cursor & clear the screen
X:cm=:	move the cursor to a given row/column
X:up=:	move the cursor up one line
X.DE
X.SH
XBoolean fields
X.if n .ul 0
X.ID
X:am:	auto margins - wrap when char is written in last column?
X:xn:	brain-damaged auto margins - newline ignored after wrap
X:pt:	physical tabs?
X.DE
X.SH
XOptional string fields
X.if n .ul 0
X.ID
X:al=:	insert a blank row on the screen
X:dl=:	delete a row from the screen
X:cd=:	clear to end of display
X:ei=:	end insert mode
X:ic=:	insert a blank character
X:im=:	start insert mode
X:dc=:	delete a character
X:sr=:	scroll reverse (insert row at top of screen)
X:vb=:	visible bell
X:ti=:	terminal initialization string, to start full-screen mode
X:te=:	terminal termination, to end full-screen mode
X:ks=:	enables the cursor keypad
X:ke=:	disables the cursor keypad
X.DE
X.SH
XOptional strings received from the keyboard
X.if n .ul 0
X.ID
X:kd=:	sequence sent by the <down arrow> key
X:kl=:	sequence sent by the <left arrow> key
X:kr=:	sequence sent by the <right arrow> key
X:ku=:	sequence sent by the <up arrow> key
X:kP=:	sequence sent by the <PgUp> key
X:kN=:	sequence sent by the <PgDn> key
X:kh=:	sequence sent by the <Home> key
X:kH=:	sequence sent by the <End> key
X:kI=:	sequence sent by the <Insert> key
X.DE
X.PP
XOriginally, termcap didn't have any names for the <PgUp>, <PgDn>, <Home>,
Xand <End> keys.
XAlthough the capability names shown in the table above are the most common,
Xthey are \fInot\fR universal.
XSCO Xenix uses :PU=:PD=:HM=:EN=: for those keys.
XAlso, if the four arrow keys happen to be part of a 3x3 keypad,
Xthen the five non-arrow keys may be named :K1=: through :K5=:,
Xso an IBM PC keyboard may be described using those names instead.
X\*E can find any of these names.
X.SH
XOptional strings sent by function keys
X.if n .ul 0
X.ID
X:k1=:...:k9=:k0=:	codes sent by <F1> through <F10> keys
X:s1=:...:s9=:s0=:	codes sent by <Shift F1> ... <Shift F10>
X:c1=:...:c9=:c0=:	codes sent by <Ctrl F1> ... <Ctrl F10>
X:a1=:...:a9=:a0=:	codes sent by <Alt F1> ... <Alt F10>
X.DE
X.PP
XNote that :k0=: is used to describe the <F10> key.
XSome termcap documents recommend :ka=: or even :k;=: for describing
Xthe <F10> key, but \*E doesn't support that.
X.PP
XAlso, the :s1=:..., :c1=:..., and :a1=:... codes are very non-standard.
XThe terminfo library doesn't support them.
X.SH
XOptional fields that describe character attributes
X.if n .ul 0
X.ID
X:so=:se=:	start/end standout mode (We don't care about :sg#:)
X:us=:ue=:	start/end underlined mode
X:md=:me=:	start/end boldface mode
X:as=:ae=:	start/end alternate character set (italics)
X:ug#:		visible gap left by :us=:ue=:md=:me=:as=:ae=:
X.DE
X.SH
XOptional fields that affect the cursor's shape
X.PP
XThe :cQ=: string is used by \*E immediately before exiting to undo
Xthe effects of the other cursor shape strings.
XIf :cQ=: is not given, then all other cursor shape strings are ignored.
X.ID
X:cQ=:	normal cursor
X:cX=:	cursor used for reading EX command
X:cV=:	cursor used for reading VI commands
X:cI=:	cursor used during VI input mode
X:cR=:	cursor used during VI replace mode
X.DE
X.PP
XIf the capabilities above aren't given, then \*E will try to use the
Xfollowing values instead.
X.ID
X:ve=:	normal cursor, used as :cQ=:cX=:cI=:cR=:
X:vs=:	gaudy cursor, used as :cV=:
X.DE
X.SH
XAn example
X.PP
XHere's the termcap entry I use on my Minix-ST system.
XSome of the fields in it have nothing to do with \*E.
XSome can only work on my system;
XI have modified my kernel's screen driver.
X.sp
X.LD
X.ne 14
Xmx|minix|minixst|ansi:\\
X	:is=\\E[0~:co#80:li#25:bs:pt:\\
X	:cm=\\E[%i%d;%dH:up=\\E[A:do=^J:nd=\\E[C:sr=\\EM:\\
X	:cd=\\E[J:ce=\\E[K:cl=\\E[H\\E[J:\\
X	:al=\\E[L:dl=\\E[M:ic=\\E[@:dc=\\E[P:im=:ei=:\\
X	:so=\\E[7m:se=\\E[m:us=\\E[4m:ue=\\E[m:\\
X	:md=\\E[1m:me=\\E[m:as=\\E[1;3m:ae=\\E[m:\\
X	:ku=\\E[A:kd=\\E[B:kr=\\E[C:kl=\\E[D:\\
X	:k1=\\E[1~:k2=\\E[2~:k3=\\E[3~:k4=\\E[4~:k5=\\E[5~:\\
X	:k6=\\E[6~:k7=\\E[17~:k8=\\E[18~:k9=\\E[19~:k0=\\E[20~:\\
X	:kU=\\E[36~:kQ=\\E[32~:kH=\\E[28~:\\
X	:GV=3:GH=D:G1=?:G2=Z:G3=@:G4=Y:GC=E:GL=4:GR=C:GU=A:GD=B:\\
X	:cQ=\\E[k:cX=\\E[2;0k:cV=\\E[16;0k:cI=\\E[k:cR=\\E[16;20k:
X.DE
/
echo x - title.ms
sed '/^X/s///' > title.ms << '/'
X.de tE
X.ps 80
X.ce 1
X\*E
X..
X.de nE
X.ce 7
X#######                                
X#        #       #    #     #     #### 
X#        #       #    #     #    #     
X#####    #       #    #     #     #### 
X#        #       #    #     #         #
X#        #        #  #      #    #    #
X#######  ######    ##       #     #### 
X..
X.sp |2i
X.if t .tE
X.if n .nE
X.ps 10
X.sp 1
X.ce 2
X- a clone of vi/ex -
Xversion \*V
X.sp |7.5i
X.IP Author: 0.9i
XSteve Kirkendall
X.br
X14407 SW Teal Blvd., Apt C
X.br
XBeaverton, OR 97005
X.IP E-Mail: 0.9i
Xkirkenda@cs.pdx.edu
X.IP Phone: 0.9i
X(503) 643-6980
/
echo x - ver.ms
sed '/^X/s///' > ver.ms << '/'
X.ds V 1.5j-betatest
X.if t .ds E E\s-2LVIS\s+2
X.if n .ds E Elvis
X.\"
X.\" usage: .Go <section#> <title>
X.de Go
X.ds LH "\\$1-\\\\n%
X.ds RH "\\$1-\\\\n%
X.ds CH "\\$2
X.NH S \\$1
X\\$2
X.\"if !\\n%=1 .bp 1
X.if n .ul 0
X..
/
echo x - versions.ms
sed '/^X/s///' > versions.ms << '/'
X.Go 12 "VERSIONS"
X.PP
X\*E currently works under BSD UNIX, AT&T System-V UNIX, SCO XENIX,
XMinix, Coherent, MS-DOS, Atari TOS, OS9/68k, VAX/VMS, and AmigaDos.
XThis section of the manual provides special information that applies to each
Xparticular version of \*E.
X.PP
XFor all versions except MS-DOS,
Xthe file "Makefile.mix" should be copied to "Makefile",
Xand then edited to select the correct set of options for your system.
XThere is more information about this embedded in the file itself.
X.NH 2
XBSD UNIX
X.PP
XTemporary files are stored in /tmp.
X.PP
XYou should modify /etc/rc so that
Xthe temp files are preserved when the system is rebooted.
XFind a line in /etc/rc which reads
X.br
X.ti +0.5i
Xex4.3preserve /tmp
X.PP
Xor something like that, and append the following line after it:
X.br
X.ti +0.5i
Xelvprsv /tmp/elv*
X.PP
XIf you do not have permission to modify /etc/rc, don't fret.
XThe above modification is only needed to allow you to recover your changes
Xafter a system crash.
XYou can still run \*E without that modification,
Xand you can still recover your changes when \*E crashes
Xor when your dialup modem looses the carrier signal, or something like that.
XOnly a system crash or power failure could hurt you.
X.PP
XBoth \*E and the real Vi
Xread initialization commands from a file called ".exrc",
Xbut the commands in that file might work on one but not the other.
XFor example, "set keywordprg=man" will work for \*E,
Xbut Vi will complain because it doesn't have a "keywordprg" option.
XIf the warning messages annoy you, then you can edit the config.h file
Xto change the name of the initialization file ".exrc" to something else,
Xsuch as ".elvisrc".
X.PP
XIf you use X windows, you may wish to add "-DCS_LATIN1" to CFLAGS.
XThis will cause the digraph table and the flipcase option to have default
Xvalues that are appropriate for the LATIN-1 character set.
XThat's the standard character set for X.
X.PP
XThe default keyboard macro time-out value is larger for BSD than it is for
Xsome other systems, because I've had trouble running \*E via rlogin or Xterm.
XI guess it takes a while for those keystokes to squirt through the net.
X.NH 2
XSystem-V UNIX
X.PP
XMost SysV UNIX systems use terminfo instead of termcap,
Xbut  the  terminfo  library  doesn't seem to have a standard name.
XAs shipped, Elvis' Makefile.mix  is  configured  with "LIBS=-lterm".
XYou may need to change it to "LIBS=-ltermcap" or "LIBS=-lterminfo"
Xor even "LIBS=-lcurses".
X.PP
XThe /etc/rc file should be modified as described for BSD systems, above.
XThe only difference is that SysV systems tend to have directories for
Xinitialization, instead of a single large /etc/rc file.
XEditor recovery is usually done somewhere in the /etc/rc2.d directory.
X.PP
XThe potential trouble with ".exrc" described above for BSD UNIX applies
Xto System-V UNIX as well.
X.PP
X\*E uses control-C as the interrupt key, not Delete.
X.NH 2
XSCO Xenix
X.PP
XFor Xenix-386, you can use the generic System-V settings.
XYou may wish to add "-DCS_IBMPC" to CFLAGS, to have the digraph table and
Xflipcase option start up in a mode that is appropriate for the console.
X
XThere is a separate group of settings for use with Xenix-286.
XIt already has "-DCS_IBMPC" in CFLAGS.
X.PP
XBecause Xenix is so similar to System-V, everything I said earlier about
XSystem-V applies to the Xenix version too, except that editor recovery
Xprobably belongs in a directory called /etc/rc.d/8.
X.NH 2
XMinix
X.PP
XThere are separate settings in Makefile.mix for Minix-PC and Minix-68k.
XThe differences between these two are that
Xthe 68k version uses ".o" for the object file extension where
Xthe PC version uses ".s", and
Xthe PC version has some extra flags in CFLAGS to reduce the size of \*E.
XThe PC version also uses tinytcap (instead of the full termcap) to make it smaller.
X.PP
XMinix-PC users should read the CFLAGS section of this manual very carefully.
XYou have some choices to make...
X.PP
XThe temporary files are stored in /usr/tmp.
XThe /usr/tmp directory must exist before you run \*E,
Xand it must be readable/writable by everybody.
XWe use /usr/tmp instead of /tmp because
Xafter a system crash or power failure,
Xyou can recover the altered version of a file from the temporary file
Xin /usr/tmp.
XIf it was stored in /tmp, though, then it would be lost because /tmp is
Xnormally located on the RAM disk.
X.PP
X\*E uses control-C as the interrupt key, not Delete.
X.NH 2
XCoherent
X.PP
X\*E was ported to Coherent by Esa Ahola.
X.PP
X\*E is too large to run under Coherent unless you eliminate some
Xfeatures via the CFLAGS setting.
XThe recommended settings, in Makefile.mix, produce a working version
Xof \*E which emulates Vi faithfully, but lacks most of the extensions.
XYou should read the CFLAGS section of this manual carefully.
X.PP
XYou can probably reduce the size of \*E by using tinytcap.c instead of -lterm.
XThis would allow you to keep most features of \*E,
Xat the expense of terminal independence.
X(Tinytcap.c has ANSI escape sequences hard-coded into it.)
XTo use tinytcap, just add "tinytcap.o" to the "EXTRA=" line in the Makefile,
Xand remove "-lterm" from the "LIBS=" line.
X.PP
XThe temporary files are stored in /tmp.
XYou should modify your /etc/rc file as described for BSD earlier.
X.NH 2
XMS-DOS
X.PP
X\*E was ported to MS-DOS by Guntram Blohm and Martin Patzel.
XWillett Kempton added support for the DEC Rainbow.
X.PP
XIdeally, \*E should be compiled with Microsoft C 5.10 and the standard
XMicrosoft Make utility,
Xvia the command "make elvis.mak".
XThis will compile \*E and all related utilities.
X.PP
XWith Microsoft C 6.00, you may have trouble compiling regexp.c.
XIf so, try compiling it without optimization.
X.PP
XThe "Makefile.mix" file contains a set of suggested settings for compiling
Xelvis with Turbo-C or Borland C.
X(If you have Turbo-C, but not the Make utility,
Xthen you can \fIalmost\fR use the "\*E.prj" file to compile \*E,
Xbut you must explicitly force Turbo-C to compile it with the "medium" memory model.
XMost of the related programs [ctags, ref, virec, refont, and wildcard] are
Xonly one file long, so you should have no trouble compiling them.)
XThe "alias.c" file is meant to be compiled once into an executable named
X"ex.exe".
XYou should then copy "ex.exe" to "vi.exe" and "view.exe".
X.PP
X\*E stores its temporary files in C:\\tmp.
XIf this is not satisfactory, then you should edit the CFLAGS line of
Xyour Makefile to change TMPDIR to something else before compiling.
XYou can also control the name of the temp directory via an environment
Xvariable named TMP or TEMP.
XThe directory must exist before you can run \*E.
X.PP
XThe TERM environment variable determines how elvis will write to the screen.
XIt can be set to any one of the following values:
X.LD
X.ta 1.5i 2.5i
X	pcbios	Use BIOS calls on an IBM-PC clone.
X	rainbow	Use DEC Rainbow interface.
X	ansi	Use ANSI.SYS driver.
X	nansi	User faster NANSI.SYS driver.
X.DE
X.PP
XIf the TERM variable isn't set, then elvis will automatically select either
Xthe "rainbow" interface (when run on a Rainbow) or "pcbios" (on an IBM clone).
X.PP
XYou may prefer to use NANSI.SYS for speed;
Xor you may NEED to use ANSI.SYS for a non-clone, such as a lap-top.
XIf so, you should
Xinstall one of these drivers by adding "driver = nansi.sys" (or whatever)
Xto your CONFIG.SYS file,
Xand then you should define TERM to be "nansi" (or whatever) by adding
X"set TERM=nansi" to your AUTOEXEC.BAT file.
XYou must then reboot for these changes to take effect.
XAfter that, \*E will notice the "TERM" setting and use the driver.
X.PP
XSince ".exrc" is not a valid DOS filename,
Xthe name of the initialization file has been changed to "elvis.rc".
XElvis will look for an "elvis.rc" file first in your home directory,
Xand then in the current directory.
XNote that you must set an environment variable named "HOME" to the
Xfull pathname of your home directory, for Elvis to check there;
Xif "HOME" isn't set, then Elvis will only look in the current directory.
XTo set "HOME", you would typically add the following line to your
XAUTOEXEC.BAT file:
X.br
X.ti +0.5i
Xset HOME c:\\
X.PP
XAn extra program, called "wildcard", is needed for MS-DOS.
XIt expands wildcard characters in file names.
XIf \*E flashes a "Bad command or filename" message when it starts,
Xthen you've probably lost the WILDCARD.EXE program somehow.
X.PP
X\*E can run under Windows, but only in full-screen mode.
XAlso, Windows uses an environment variable called TEMP which interferes with
Xelvis' usage of TEMP;
Xto work around this, you can simply set an environment variable named
XTMP (with no 'E') to the name of elvis' temporary directory.
XWhen TEMP and TMP are both set, \*E uses TMP and ignored TEMP.
X.NH 2
XAtari TOS
X.PP
X\*E was ported to Atari TOS by Guntram Blohm and Martin Patzel.
XIt is very similar to the MS-DOS version.
XIt has been tested with the Mark Williams C compiler and also GNU-C.
X.PP
XThe TERM environment variable is ignored;
Xthe ST port always assumes that TERM=vt52.
XThe SHELL (not COMSPEC!) variable should be set to
Xthe name of a line-oriented shell.
X.PP
XA simple shell in included with \*E.
XIts source is in "shell.c", and the name of the executable is "shell.ttp".
XThe file "profile.sh" should contain a set of instructions to be executed
Xwhen the shell first starts up.
XAn example of this file is included, but you will almost certainly want to
Xedit it right away to match your configuration.
X(If you already have a command-line shell,
Xthen you'll probably want to continue using it.
XThe shell that comes with \*E is very limited.)
X.PP
XCurrently, character attributes cannot be displayed on the screen.
X.PP
X\*E runs under MiNT (a free multi-tasking extension to TOS)
Xbut it can be a CPU hog because of the way that \*E reads from the
Xkeyboard with timeout.
XAlso, \*E doesn't use any of the special features of MiNT.
XI have received a set of patches that optimize \*E for MiNT,
Xbut they arrived too late to integrate into this release.
X.NH 2
XOS9/68k
X.PP
X\*E was ported to OS9/68k by Peter Reinig.
X.PP
XThe Makefile is currently configured to install \*E and the related
Xprograms in /dd/usr/cmds
XIf this this is unacceptable, then you should change the BIN setting
Xto some other directory.
XSimilarly, it expects the source code to reside in /dd/usr/src/elvis;
Xthe ODIR setting is used to control this.
X.PP
XTemporary files are stored in the /dd/tmp directory.
XYour /dd/startup file may need to be modified
Xto prevent it from deleting \*E' temporary files;
Xmake /dd/startup run the \fIelvprsv\fR program before it wipes out /dd/tmp.
X.PP
XThe program in alias.c is linked repeatedly to produce the
X"vi", "view", and "input" aliases for \*E.
XSadly, the "ex" alias is impossible to implement under OS9
Xbecause the shell has a built-in command by that name.
X.PP
XFor some purposes,
Xyou must give `make' the "-b" option.
XSpecifically, you need this for "make -b clean" and "make -b install".
X.NH 2
XVAX/VMS
X.PP
XJohn Campbell ported \*E to VAX/VMS.
X.PP
XA heavily laden VAX can take half an hour to compile elvis.
XThis is normal.
XDon't panic.
X.PP
XWhile running, elvis will create temporary files in SYS$SCRATCH.
XEnter SHOW LOGICAL SYS$SCRATCH to see what actual directory you are using.
XMany sites have SYS$SCRATCH equivalenced to SYS$LOGIN.
XThe elvis temporary files look like the following on VMS while elvis is running:
X.br
X.ti 0.75i
XELV_1123A.1;1       ELV_1123A.2;1       SO070202.;1
X.PP
XAlso, filtering commands (like !!dir and !}fmt) should work on VMS.
XThis assumes, however, that you can create temporary mailboxes and that
Xyour mailbox quota (a sysgen parameter) is at least 256 bytes for a
Xsingle write to the mailbox.
XThis is the default sysgen parameter,
Xso there should be few people who experience filter problems.
X.PP
XAdditionally, an attempt was made to support the standard terminals on VMS:
X"vt52", "vt100", "vt200", "vt300", "vt101", "vt102".
XNon-standard terminals could be supported by setting your terminal type to
XUNKNOWN (by entering SET TERM/UNKNOWN)
Xand defining the logical name ELVIS_TERM.
XWhatever ELVIS_TERM translates to, however, will have to be included in
Xtinytcap.c.
XNote that the upper/lowercase distinctions are significant,
Xand that DCL will upshift characters that are not quoted strings, so
Xenter DEFINE ELVIS_TERM "hp2621a".
XAs distributed, it would probably not be a good idea to have more than the
Xstandard terminals in tinytcap.c (else it wouldn't be tiny, would it?).
XChanges here, of course, would require a recompilation to take effect.
X.PP
XIf you have a version of the "termcap" library and database on your system,
Xthen you may wish to replace tinytcap with the real termcap.
X.NH 2
XAmigaDOS
X.PP
XMike Rieser and Dale Rahn ported \*E to AmigaDOS.
X.PP
XThe port was done using Manx Aztec C version 5.2b.
X\*E uses about as much space as it can and still be small code and data.
X\*E should also compile under DICE, though there may be a little trouble with
Xsigned versus unsigned chars.
X.PP
XThe port has been done so the same binary will run under both versions of AmigaDOS.
XUnder AmigaDOS 2.04, \*E supports all the documented features.
XIt also uses an external program ref to do tag lookup.
XSo, the accompanying programs: ref and ctags are recommended.
XUnder AmigaDOS 1.2/1.3 \*E works, buts lacks the more advanced features.
X.PP
XFor the port to AmigaDOS 2.04, we tried to use as many Native AmigaDOS
Xcalls as we could.
XThis should increase Elvis's chances at being compiled with other compilers.
XDICE seems to have a different default char type.
XYou may need to use the UCHAR() macro in tio.c.
XTo test it, try the :map command; if it looks right, things are cool.
X.PP
XFor the port to AmigaDOS 1.3, we tried to make sure the program was at
Xleast usable.
XMany features are missing, most notably running commands in subshells.
XAlso, what we could get working, we used Aztec functions to support them,
Xso this part is little more compiler dependent.
X.PP
XAztec is compatible with the SAS libcall #pragma.
XI personally prefer using the includes that come from Commodore over the ones
Xsupplied with Aztec, but for people with a straight Aztec installation,
XI went with the default names for the Aztec pragmas.
X.PP
XOne include you'll need is <sys/types.h>.
XIts a common include when porting software just make yourself one.
XIts a two line file that saves a lot of hassle especially in the elvis source.
XSo, make a directory where your includes are located called `sys'
Xand in a file below that type:
X.br
X.ti +0.8i
X/* sys/types.h */
X.br
X.ti +0.8i
X#include <exec/types.h>
X.PP
XWhen setting environment variables (either local or global) for
Xvariables that specify a directory, make sure the variable ends in `:'
Xor `/'.
XThis saved from having to change much of the way elvis works.
XThe default temporary directory (if TEMP and TMP aren't specified) is "T:".
XThe default if HOME directory (if no HOME environment variable is set) is "S:".
X.PP
XTo avoid conlict with other uses, \*E uses elvis.rc instead of .exrc or
Xwhere it looks for macros.
X.NH 2
XOther Systems
X.PP
XFor Sun workstations, use the BSD configuration.
XEarlier versions of elvis didn't link correctly due to a quirk in Sun's
Xversion of the "make" utility, but this version of elvis has a work-around
Xfor that quirk so you should have no trouble at all.
X.PP
XFor Linux, use the SysV settings.
XYou can probably just remove the "-lterm" from the "LIBS= -lterm" line,
Xsince linux keeps the termcap functions in the standard C library.
X.PP
XFor other UNIXoid systems, I suggest you start with the Minix-68k settings
Xand then grow from that.
XMinix is a nice starting point because it is a clone of Version 7 UNIX,
Xwhich was the last common ancestor of BSD UNIX and SysV UNIX.
XAny Operating System which claims any UNIX compatibility what so ever
Xwill therefore support V7/Minix code.
XYou may need to fiddle with #include directives or something, though.
XMinix-68k is a better starting point than Minix-PC because the PC compiler
Xhas some severe quirks.
/
echo x - visual.ms
sed '/^X/s///' > visual.ms << '/'
X.Go 2 "VISUAL MODE COMMANDS"
X.PP
XMost visual mode commands are one keystroke long.
XThe following table lists the operation performed by each keystroke,
Xand also denotes any options or arguments that it accepts.
XNotes at the end of the table describe the notation used in this table.
X.PP
XIn addition to the keys listed here, your keyboard's "arrow" keys
Xwill be interpretted as the appropriate cursor movement commands.
XThe same goes for <PgUp> and <PgDn>, if your keyboard has them.
XThe <Insert> key will toggle between insert mode and replace mode.
XThere is a colon mode command (":map", to be described later)
Xwhich will allow you to define other keys, such as function keys.
X.PP
XA tip: visual command mode looks a lot like text input mode.
XIf you forget which mode you're in, just hit the <Esc> key.
XIf \*E beeps, then you're in visual command mode.
XIf \*E does not beep, then you were in input mode,
Xbut by hitting <Esc> you will have switched to visual command mode.
XSo, one way or another, after <Esc> \*E will be ready for a command.
X.LD
X.ta 0.7i 1.3i
X\s+2COMMAND	DESCRIPTION\s-2
X	^A	Search for next occurence of word at cursor (MOVE)(EXT)
X	^B	Move toward the top of the file by 1 screenful
X	^C	--- (usually sends SIGINT, to interupt a command)
Xcount	^D	Scroll down <count> lines (default 1/2 screen)
Xcount	^E	Scroll up <count> lines
X	^F	Move toward the bottom of the file by 1 screenful
X	^G	Show file status, and the current line #
Xcount	^H	Move left, like h (MOVE)
X	^I	---
Xcount	^J	Move down (MOVE)
X	^K	---
X	^L	Redraw the screen
Xcount	^M	Move to the front of the next line (MOVE)
Xcount	^N	Move down (MOVE)
X	^O	---
Xcount	^P	Move up (MOVE)
X	^Q	--- (typically XON, which restarts screen updates)
X	^R	Redraw the screen
X	^S	--- (typically XOFF, which stops screen updates)
X	^T	---
Xcount	^U	Scroll up <count> lines (default 1/2 screen)
X	^V	---
X	^W	---
Xcount	^X	Move to a physical column number on the screen (MOVE) (EXT)
Xcount	^Y	Scroll down <count> lines
X	^Z	--- (sometimes sends SIGSUSP, to suspend execution)
X	ESC	---
X	^\\	--- (usually sends SIGQUIT, which is ignored)
X	^]	If the cursor is on a tag name, go to that tag
X	^^	Switch to the previous file, like ":e #"
X	^_	---
Xcount	SPC	Move right,like l (MOVE)
X	! \s-2mv\s+2	Run the selected lines thru an external filter program
X	" \s-2key\s+2	Select which cut buffer to use next
Xcount	# \s-2+\s+2	Increment a number (EDIT) (EXT)
X	$	Move to the rear of the current line (MOVE)
Xcount	%	Move to matching (){}[] or to a given % of file (MOVE) (EXT)
Xcount	&	Repeat the previous ":s//" command here (EDIT)
X	' \s-2key\s+2	Move to a marked line (MOVE)
Xcount	(	Move backward <count> sentences (MOVE)
Xcount	)	Move forward <count> sentences (MOVE)
X	*	Go to the next error in the errlist (EXT)
Xcount	+	Move to the front of the next line (MOVE)
Xcount	,	Repeat the previous [fFtT] but in the other direction (MOVE)
Xcount	-	Move to the front of the preceding line (MOVE)
Xcount	.	Repeat the previous "edit" command
X	/ \s-2text\s+2	Search forward for a given regular expression (MOVE)
X	0	If not part of count, move to 1st char of this line (MOVE)
X	1	Part of count
X	2	Part of count
X	3	Part of count
X	4	Part of count
X	5	Part of count
X	6	Part of count
X	7	Part of count
X	8	Part of count
X	9	Part of count
X	: \s-2text\s+2	Run single EX cmd
Xcount	;	Repeat the previous [fFtT] cmd (MOVE)
X	< \s-2mv\s+2	Shift text left (EDIT)
X	= \s-2mv\s+2	Reformat
X	> \s-2mv\s+2	Shift text right (EDIT)
X	? \s-2text\s+2	Search backward for a given regular expression (MOVE)
X	@ \s-2key\s+2	Execute the contents of a cut-buffer as VI commands
Xcount	A \s-2inp\s+2	Append at end of the line (EDIT)
Xcount	B	Move back Word (MOVE)
X	C \s-2inp\s+2	Change text from the cursor through the end of the line (EDIT)
X	D	Delete text from the cursor through the end of the line (EDIT)
Xcount	E	Move end of Word (MOVE)
Xcount	F \s-2key\s+2	Move leftward to a given character (MOVE)
Xcount	G	Move to line #<count> (default is the bottom line) (MOVE)
Xcount	H	Move to home row (the line at the top of the screen)
Xcount	I \s-2inp\s+2	Insert at the front of the line (after indents) (EDIT)
Xcount	J	Join lines, to form one big line (EDIT)
X	K	Look up keyword (EXT)
Xcount	L	Move to last row (the line at the bottom of the screen)
X	M	Move to middle row
X	N	Repeat previous search, but in the opposite direction (MOVE)
Xcount	O \s-2inp\s+2	Open up a new line above the current line (EDIT)
X	P	Paste text before the cursor (EDIT)
X	Q	Quit to EX mode
X	R \s-2inp\s+2	Overtype (EDIT)
Xcount	S \s-2inp\s+2	Change lines, like <count>cc
Xcount	T \s-2key\s+2	Move leftward *almost* to a given character (MOVE)
X	U	Undo all recent changes to the current line
X	V	Start marking lines for c/d/y/</>/!/\\ (EXT)
Xcount	W	Move forward <count> Words (MOVE)
Xcount	X	Delete the character(s) to the left of the cursor (EDIT)
Xcount	Y	Yank text line(s) (copy them into a cut buffer)
X	Z Z	Save the file & exit
X	[ [	Move back 1 section (MOVE)
X	\\ \s-2mv\s+2	Pop-up menu for modifying text (EXT)
X	] ]	Move forward 1 section (MOVE)
X	^	Move to the front of the current line (after indent) (MOVE)
Xcount	_	Move to the current line
X	` \s-2key\s+2	Move to a marked character (MOVE)
Xcount	a \s-2inp\s+2	Insert text after the cursor (EDIT)
Xcount	b	Move back <count> words (MOVE)
X	c \s-2mv\s+2	Change text (EDIT)
X	d \s-2mv\s+2	Delete text (EDIT)
Xcount	e	Move forward to the end of the current word (MOVE)
Xcount	f \s-2key\s+2	Move rightward to a given character (MOVE)
X	g	---
Xcount	h	Move left (MOVE)
Xcount	i \s-2inp\s+2	Insert text at the cursor (EDIT)
Xcount	j	Move down (MOVE)
Xcount	k	Move up (MOVE)
Xcount	l	Move right (MOVE)
X	m \s-2key\s+2	Mark a line or character
X	n	Repeat the previous search (MOVE)
Xcount	o \s-2inp\s+2	Open a new line below the current line (EDIT)
X	p	Paste text after the cursor (EDIT)
X	q	---
Xcount	r \s-2key\s+2	Replace <count> chars by a given character (EDIT)
Xcount	s \s-2inp\s+2	Replace <count> chars with text from the user (EDIT)
Xcount	t \s-2key\s+2	Move rightward *almost* to a given character (MOVE)
X	u	Undo the previous edit command
X	v	Start marking characters for c/d/y/</>/!/\\ (EXT)
Xcount	w	Move forward <count> words (MOVE)
Xcount	x	Delete the character that the cursor's on (EDIT)
X	y \s-2mv\s+2	Yank text (copy it into a cut buffer)
X	z \s-2key\s+2	Scroll current line to the screen's +=top -=bottom .=middle
Xcount	{	Move back <count> paragraphs (MOVE)
Xcount	|	Move to column <count> (the leftmost column is 1)
Xcount	}	Move forward <count> paragraphs (MOVE)
Xcount	~	Switch a character between uppercase & lowercase (EDIT)
X	DEL	--- (usually mapped to shift-X, so it deletes one character)
X.DE
X.IP count
XMany commands may be preceded by a count.  This is a sequence of digits
Xrepresenting a decimal number.  For most commands that use a count,
Xthe command is repeated <count> times.  The count is always optional,
Xand usually defaults to 1.
X.IP key
XSome commands require two keystrokes.  The first key always determines
Xwhich command is to be executed.  The second key is used as a parameter
Xto the command.
X.IP mv
XSome commands (! < > c d y \\ =) operate on text between the cursor and some
Xother position.
XThere are three ways that you can specifify that other position.
X.IP
XThe first way is to follow the command keystroke with a movement command.
XFor example, "dw" deletes a single word.
X"d3w" and "3dw" both delete three words.
X.IP
XThe second way is to type the command keystroke twice.
XThis causes whole lines to be acted upon.
XFor example, ">>" indents the current line.
X"3>>" indents the current line and the following two lines.
X.IP
XThe last way is to move the cursor to one end of the text,
Xtype 'v' or 'V' to start marking,
Xmove the cursor to the other end,
Xand then type the desired command key.
X.IP inp
XMany commands allow the user to interactively enter text.
XSee the discussion of "input mode" in the following section.
X.IP (EXT)
XThese commands are extensions -- the real vi doesn't have them.
X.IP (EDIT)
XThese commands affect text, and may be repeated by the "." command.
X.IP (MOVE)
XThese commands move the cursor, and may be used to specify the extent
Xof a member of the "mv" class of commands.
X.NH 2
XInput Mode
X.PP
XYou can't type text into your file directly from visual command mode.
XInstead, you must first give a command which will put you into input mode.
XThe commands to do this are A/C/I/O/R/S/a/i/o/s.
X.PP
XThe S/s/C/c commands temporarily place a $ at the end of the text that
Xthey are going to change.
X.PP
XIn input mode, all keystrokes are inserted into the text at the
Xcursor's position, except for the following:
X.ID
X^A	insert a copy of the last input text
X^D	delete one indent character
X^H	(backspace) erase the character before the cursor
X^L	redraw the screen
X^M	(carriage return) insert a newline (^J, linefeed)
X^O	execute next key as a visual command (limited!)
X^P	insert the contents of the cut buffer
X^R	redraw the screen, like ^L
X^T	insert an indent character
X^U	backspace to the beginning of the line
X^V	insert the following keystroke, even if special
X^W	backspace to the beginning of the current word
X^Z^Z	write the file & exit \*E
X^[	(ESCape) exit from input mode, back to command mode
X.DE
X.PP
XAlso, on some systems, ^S may stop output, ^Q may restart output,
Xand ^C may interupt execution.
X^@ (the NUL character) cannot be inserted.
X.PP
XThe R visual command puts you in overtype mode,
Xwhich is a slightly different form of input mode.
XIn overtype mode, each time you insert a character,
Xone of the old characters is deleted from the file.
X.NH 2
XArrow keys in Input Mode
X.PP
XThe arrow keys can be used to move the cursor in input mode.
X(This is an extension; the real Vi doesn't support arrow keys in input mode.)
XThe <PgUp>, <PgDn>, <Home>, and <End> keys work in input mode, too.
XThe <Delete> key deletes a single character in input mode.
XThe <Insert> key toggles between input mode and replace mode.
X.PP
XThe best thing about allowing arrow keys to work in input mode is that
Xas long as you're in input mode,
X\*E seems to have a fairly ordinary user interface.
XWith most other text editors, you are always in either insert mode or
Xreplace mode, and you can use the arrow keys at any time to move the cursor.
XNow, \*E can act like that, too.
XIn fact, with the new "inputmode" option and the "control-Z control-Z" input
Xcommand, you may never have to go into visual command mode for simple edit
Xsessions.
X.NH 2
XDigraphs
X.PP
X\*E supports digraphs as a way to enter non-ASCII characters.
XA digraph is a character which is composed of two other characters.
XFor example, an apostrophe and the letter i could be defined as a digraph
Xwhich is to be stored & displayed as an accented i.
X.PP
XThere is no single standard for extended ASCII character sets.
X\*E can be compiled to fill the digraph with values appropriate for
Xeither the IBM PC character set, or the LATIN-1 character set used by
XX windows, or neither.
X(See the discussions of -DCS_IBMPC and -DCS_LATIN1 in the CFLAGS section
Xof this manual.)
XYou can view or edit the digraph table via the ":digraph" colon command.
X.PP
XDigraphs will not be recognized until you've entered ":set digraph".
X.PP
XTo actually use a digraph
Xtype the first character, then hit <Backspace>, and then type the
Xsecond character.
X\*E will then substitute the non-ASCII character in their place.
X.NH 2
XAbbreviations
X.PP
X\*E can expand abbreviations for you.
XYou define an abbreviation with the :abbr command,
Xand then whenever you type in the abbreviated form while in input mode,
X\*E will immediately replace it with the long form.
XCOBOL programmers should find this useful. :-)
X.PP
X\*E doesn't perform the substitution until you type a non-alphanumeric
Xcharacter to mark the end of the word.
XIf you type a control-V before that non-alphanumeric character, then
X\*E will not perform the substitution.
X.NH 2
XAuto-Indent
X.PP
XWith the ":set autoindent" option turned on,
X\*E will automatically insert leading whitespace at the beginning of each
Xnew line that you type in.
XThe leading whitespace is copied from the preceding line.
X.PP
XTo add more leading whitespace, type control-T.
XTo remove some whitespace, type control-D.
X.PP
XIf you ":set noautotab", then the whitespace generated by control-T will
Xalways consist of spaces -- never tabs.
XSome people seem to prefer this.
X.PP
X\*E' autoindent mode isn't 100% compatible with vi's.
XIn \*E, 0^D and ^^D don't work,
X^U can wipeout all indentation, 
Xand sometimes \*E will use a different amount of indentation than vi would.
/

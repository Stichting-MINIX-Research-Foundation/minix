!
!
!
!
!

Xcursor.theme: whiteglass

#define BS \ /* cpp can be trickier than m4 */
#define NLBS \n\ /* don't remove these comments */
xlogin*login.translations: #override BS
	Ctrl<Key>R: abort-display()NLBS
	<Key>F1: set-session-argument(failsafe) finish-field()NLBS
	<Key>Delete: delete-character()NLBS
	<Key>Left: move-backward-character()NLBS
	<Key>Right: move-forward-character()NLBS
	<Key>Home: move-to-begining()NLBS
	<Key>End: move-to-end()NLBS
	Ctrl<Key>KP_Enter: set-session-argument(failsafe) finish-field()NLBS
	<Key>KP_Enter: set-session-argument() finish-field()NLBS
	Ctrl<Key>Return: set-session-argument(failsafe) finish-field()NLBS
	<Key>Return: set-session-argument() finish-field()

xlogin*greeting: Welcome to CLIENTHOST
xlogin*namePrompt: \040\040\040\040\040\040\040Login:
xlogin*fail: Login incorrect

XHASHif WIDTH > 800
xlogin*greetFont: -adobe-helvetica-bold-o-normal-*-18-*-*-*-*-*-iso8859-1
xlogin*font: -adobe-helvetica-medium-o-normal-*-14-*-*-*-*-*-iso8859-1
xlogin*promptFont: -adobe-helvetica-medium-r-normal-*-14-*-*-*-*-*-iso8859-1
xlogin*failFont: -adobe-helvetica-medium-r-normal-*-14-*-*-*-*-*-iso8859-1
xlogin*greetFace:	Serif-24:bold:italic
xlogin*face: 		Helvetica-14
xlogin*promptFace: 	Helvetica-14:bold
xlogin*failFace: 	Helvetica-14:bold
XHASHelse
xlogin*greetFont: -adobe-helvetica-bold-o-normal--17-120-100-100-p-92-iso8859-1
xlogin*font: -adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1
xlogin*promptFont: -adobe-helvetica-bold-r-normal--12-120-75-75-p-70-iso8859-1
xlogin*failFont: -adobe-helvetica-bold-o-normal--14-140-75-75-p-82-iso8859-1
xlogin*greetFace:	Serif-18:bold:italic
xlogin*face:		Helvetica-12
xlogin*promptFace:	Helvetica-12:bold
xlogin*failFace:	Helvetica-14:bold
XHASHendif

XHASHifdef COLOR
xlogin*borderWidth: 1
xlogin*frameWidth: 5
xlogin*innerFramesWidth: 2
xlogin*shdColor: grey30
xlogin*hiColor: grey90
xlogin*background: grey
!xlogin*foreground: darkgreen
xlogin*greetColor: Blue3
xlogin*failColor: red
*Foreground: black
*Background: #fffff0
XHASHelse
xlogin*borderWidth: 3
xlogin*frameWidth: 0
xlogin*innerFramesWidth: 1
xlogin*shdColor: black
xlogin*hiColor: black
XHASHendif
#ifdef XPM
XHASHif PLANES >= 8
XHASHif 1
! XDM has no support for images with alpha channel, so we precomputed a
! NetBSD logo with fixed background and use it here. If you change this
! file to use another background colour, you need to create a new logo
! xpm file. This can be done with netpbm from pkgsrc:
!
!   pngtopnm -mix -background grey NetBSD-flag.png | pnmtoxpm > NetBSD-flag.xpm
!
! (all files in BITMAPDIR)
!
xlogin*logoFileName: BITMAPDIR/**//NetBSD-flag1.xpm
xlogin*useShape: false
XHASHelse
!
! This is the stock method, using a coloured xpm file and a b&w mask xpm.
! Antialiased borders look ugly this way, but it works with arbitrary
! background colours.
!
xlogin*logoFileName: BITMAPDIR/**//XDM_PIXMAP
xlogin*useShape: true
XHASHendif
XHASHelse
xlogin*logoFileName: BITMAPDIR/**//XDM_BWPIXMAP
xlogin*useShape: true
XHASHendif
xlogin*logoPadding: 10
#endif /* XPM */

XConsole.text.geometry:	480x130
XConsole.verbose:	true
XConsole*iconic:	true
XConsole*font:		fixed

Chooser*geometry:		700x500+300+200
Chooser*allowShellResize:	false
Chooser*viewport.forceBars:	true
Chooser*label.font:		*-new century schoolbook-bold-i-normal-*-240-*
Chooser*label.label:		XDMCP Host Menu from CLIENTHOST
Chooser*list.font:		-*-*-medium-r-normal-*-*-230-*-*-c-*-iso8859-1
Chooser*Command.font:		*-new century schoolbook-bold-r-normal-*-180-*

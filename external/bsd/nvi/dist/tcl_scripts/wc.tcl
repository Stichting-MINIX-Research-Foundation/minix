#	Id: wc.tcl,v 8.2 1995/11/18 12:59:12 bostic Exp  (Berkeley) Date: 1995/11/18 12:59:12 
#
proc wc {} {
	global viScreenId
	global viStartLine
	global viStopLine

	set lines [viLastLine $viScreenId]
	set output ""
	set words 0
	for {set i $viStartLine} {$i <= $viStopLine} {incr i} {
		set outLine [split [string trim [viGetLine $viScreenId $i]]]
		set words [expr $words + [llength $outLine]]
	}
	viMsg $viScreenId "$words words"
}

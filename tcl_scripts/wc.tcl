#	$Id: wc.tcl,v 8.1 1995/11/05 10:33:41 bostic Exp $ (Berkeley) $Date: 1995/11/05 10:33:41 $
#
proc wc {} {
	set lines [viLastLine]
	set output ""
	set words 0
	for {set i 1} {$i <= $lines} {incr i} {
		set outLine [split [string trim [viGetLine $i]]]
		set words [expr $words + [llength $outLine]]
	}
	viMsg "$words words"
}

#	$Id: errors.tcl,v 8.1 1995/11/05 10:33:29 bostic Exp $ (Berkeley) $Date: 1995/11/05 10:33:29 $
#
# File: errors.tcl
#
# Author: George V. Neville-Neil
#
# Purpose: This file contains vi/tcl code that allows a vi user to parse
# compiler errors and warnings from a make.out file.

proc findErr {} {
	global errScreen
	global currFile
	global fileScreen
	set errLine [lindex [viGetCursor $errScreen] 0]
	set currLine [split [viGetLine $errScreen $errLine] :]
	set currFile [lindex $currLine 0]
	set fileScreen [viNewScreen $currFile]
	viSetCursor $fileScreen [lindex $currLine 1] 1
	viMapKey  nextErr
}

proc nextErr {} {
	global errScreen
	global fileScreen
	global currFile
	set errLine [lindex [viGetCursor $errScreen] 0]
	set currLine [split [viGetLine $errScreen $errLine] :]
	if {[string match $currFile [lindex $currLine 0]]} {
		viSetCursor $fileScreen [lindex $currLine 1] 0
		viSwitchScreen $fileScreen
	} else {
		viEndScreen $fileScreen
		set currFile [lindex $currLine 0]
		set fileScreen[viNewScreen $currFile]
		viSetCursor $fileScreen [lindex $currLine 1] 0
	}
}

proc initErr {} {
	global errScreen
	set errScreen [viNewScreen make.out]
	viMapKey  findErr
}


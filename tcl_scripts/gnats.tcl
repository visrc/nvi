#	$Id: gnats.tcl,v 8.1 1995/11/05 10:33:32 bostic Exp $ (Berkeley) $Date: 1995/11/05 10:33:32 $
#
proc init {catFile} {
	global categories
	set categories {}
        set categoriesFile [open $catFile r]
	while {[gets $categoriesFile line] >= 0} {
		lappend categories $line
	}
	close $categoriesFile
	viMsg $categories
	viMapKey  next
}

proc next {} {
	set cursor [viGetCursor]
	set lineNum [lindex $cursor 0]
	set line [viGetLine $lineNum]
	viMsg [lindex $line 0]
	if {[lindex $line 0] == ">Confidential:"} {
		confNext $lineNum $line
	} elseif {[lindex $line 0] == ">Severity:"} {
		sevNext $lineNum $line
	} elseif {[lindex $line 0] == ">Priority:"} {
		priNext $lineNum $line
	} elseif {[lindex $line 0] == ">Class:"} {
		classNext $lineNum $line
	} elseif {[lindex $line 0] == ">Category:"} {
		catNext $lineNum $line
	}
}

proc confNext {lineNum line} {
	viMsg [lindex $line 1]
	if {[lindex $line 1] == "yes"} {
		viSetLine $lineNum ">Confidential: no"
	} else {
		viSetLine $lineNum ">Confidential: yes"
	}
}

proc sevNext {lineNum line} {
	viMsg [lindex $line 1]
	if {[lindex $line 1] == "non-critical"} {
		viSetLine $lineNum ">Severity: serious"
	} elseif {[lindex $line 1] == "serious"} {
		viSetLine $lineNum ">Severity: critical"
	} elseif {[lindex $line 1] == "critical"} {
		viSetLine $lineNum ">Severity: non-critical"
	}
}

proc priNext {lineNum line} {
	viMsg [lindex $line 1]
	if {[lindex $line 1] == "low"} {
		viSetLine $lineNum ">Priority: medium"
	} elseif {[lindex $line 1] == "medium"} {
		viSetLine $lineNum ">Priority: high"
	} elseif {[lindex $line 1] == "high"} {
		viSetLine $lineNum ">Priority: low"
	}
}

proc classNext {lineNum line} {
	viMsg [lindex $line 1]
	if {[lindex $line 1] == "sw-bug"} {
		viSetLine $lineNum ">Class: doc-bug"
	} elseif {[lindex $line 1] == "doc-bug"} {
		viSetLine $lineNum ">Class: change-request"
	} elseif {[lindex $line 1] == "change-request"} {
		viSetLine $lineNum ">Class: support"
	} elseif {[lindex $line 1] == "support"} {
		viSetLine $lineNum ">Class: sw-bug"
	}
}

proc catNext {lineNum line} {
	global categories
	viMsg [lindex $line 1]
	set curr [lsearch -exact $categories [lindex $line 1]]
	if {$curr == -1} {
		set curr 0
	}
	viMsg $curr
	viSetLine $lineNum ">Class: [lindex $categories $curr]"
}

init abekas

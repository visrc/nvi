#	$Id: mailprocs.tcl,v 8.1 1995/11/05 10:33:36 bostic Exp $ (Berkeley) $Date: 1995/11/05 10:33:36 $
#
proc validLine {} {
	set line [viGetLine [lindex [viGetCursor] 0]]
	if {[string compare [lindex [split $line :] 0]	"To"] == 0} {
		set addrs [lindex [split $line :] 1]
		foreach name [split $addrs ,] {
			isValid [string trim $name]
		}
	}
}

proc valid {target} {
	set found 0
	set aliasFile [open "~/Mail/aliases" r]
	while {[gets $aliasFile line] >= 0} {
		set name [lindex [split $line :] 0]
		set address [lindex [split $line :] 1]
		if {[string compare $target $name] == 0} {
			set found 1
			break
		}
	}
	close $aliasFile
	if {$found == 1} {
		return $address
	} else {
		return $found
	}
}

proc isValid {target} {
	set address [valid $target]
	if {$address != 0} {
		viMsg "$target is [string trim $address]"
	} else {
		viMsg "$target not found"
	}
}

proc isAliasedLine {} {
	set line [viGetLine [lindex [viGetCursor] 0]]
	if {[string match [lindex [split $line :] 0] "*To"] == 0} {
		set addrs [lindex [split $line :] 1]
		foreach name [split $addrs ,] {
			isAliased [string trim $name]
		}
	}
}

proc aliased {target} {
	set found 0
	set aliasFile [open "~/Mail/aliases" r]
	while {[gets $aliasFile line] >= 0} {
		set name [lindex [split $line :] 0]
		set address [lindex [split $line :] 1]
		if {[string compare $target [string trim $address]] == 0} {
			set found 1
			break
		}
	}
	close $aliasFile

	return $found
}

proc isAliased {target} {
	set found [aliased $target]

	if {$found} {
		viMsg "$target is aliased to [string trim $name]"
	} else {
		viMsg "$target not aliased"
	}
}

proc appendAlias {target address} {
	if {![aliased $target]} {
	    set aliasFile [open "~/Mail/aliases" a]
	    puts $aliasFile "$target: $address"
	}
	close $aliasFile
}

proc expand {} {
	set row [lindex [viGetCursor] 0]]
	set column [lindex [viGetCursor] 1]]
	set line [viGetLine $row]
	while {$column < [string length $line] && \
		[string index $line $column] != ' '} {
		append $target [string index $line $column]
		incr $column
	}
	set found [isValid $target]
}

viMapKey  isAliasedLine
viMapKey  validLine

#	$Id: ex.awk,v 8.1 1994/04/17 17:28:07 bostic Exp $ (Berkeley) $Date: 1994/04/17 17:28:07 $
 
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}

#	$Id: ex.awk,v 9.1 1994/11/09 18:42:53 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:42:53 $
 
/^\/\* C_[0-9A-Z_]* \*\/$/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}

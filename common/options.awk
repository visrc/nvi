#	$Id: options.awk,v 8.1 1994/04/17 17:28:01 bostic Exp $ (Berkeley) $Date: 1994/04/17 17:28:01 $
 
/^\/\* O_[0-9A-Z_]*/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}
END {
	printf("#define O_OPTIONCOUNT %d\n", cnt);
}

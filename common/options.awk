#	$Id: options.awk,v 9.1 1994/11/09 18:45:28 bostic Exp $ (Berkeley) $Date: 1994/11/09 18:45:28 $
 
/^\/\* O_[0-9A-Z_]*/ {
	printf("#define %s %d\n", $2, cnt++);
	next;
}
END {
	printf("#define O_OPTIONCOUNT %d\n", cnt);
}

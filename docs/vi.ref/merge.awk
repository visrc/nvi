#	$Id: merge.awk,v 8.1 1994/05/25 19:37:50 bostic Exp $ (Berkeley) $Date: 1994/05/25 19:37:50 $
#
# merge index entries into one line per label
$1 == prev {
	printf ", %s", $2;
	next;
}
{
	printf "%s%s \t%s", sep, $1, $2;
	if (sep == "\t")
		sep = "\n";
	else if (sep == "\n")
		sep = "\t";
	else
		sep = "\t";
	prev = $1;
}

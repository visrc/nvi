#	$Id: merge.awk,v 8.2 1994/05/25 20:00:04 bostic Exp $ (Berkeley) $Date: 1994/05/25 20:00:04 $
#
# merge index entries into one line per label
BEGIN {
	sep = 1;
}
$1 == prev {
	printf ", %s", $2;
	next;
}
{
	if (sep == 1) {
		p = "";
		++sep;
	} else if (sep == 2 || sep == 3) {
		p = "\t";
		++sep;
	} else {
		p = "\n";
		sep = 2;
	}
	printf "%s%s \t%s", p, $1, $2;
	prev = $1;
}

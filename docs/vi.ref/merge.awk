#	$Id: merge.awk,v 8.3 1994/05/25 23:45:36 bostic Exp $ (Berkeley) $Date: 1994/05/25 23:45:36 $
#
# merge index entries into one line per label
$1 == prev {
	printf ", %s", $2;
	next;
}
{
	if (NR != 1)
		printf "\n";
	printf "%s \t%s", $1, $2;
	prev = $1;
}
END {
	printf "\n"
}

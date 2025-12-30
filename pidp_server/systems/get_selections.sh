#! /bin/sh
#
# Filter the "selections" file, printing only the lines for which the directory exists
selections="${1-`dirname -- $0`/selections}"
basedir=`dirname $selections`

if [ -r "$selections" ]; then
	while read csw dir
	do
		if [ -d "$basedir/$dir" ] ; then echo "$csw	$dir" ; fi
	done < $selections
fi

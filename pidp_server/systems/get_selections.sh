#! /bin/bash
#
# Process the "selections" file, printing its information only for
# directories that exist.
#
# -v: print in verbose (name=value) format
# -cDEFAULT: print CPU name, DEFAULT if none
# -d: print description

declare -a all_params=( csw dir cpu desc )
declare -a show_params=( csw dir )

opt_cpu=
opt_v=

while getopts "c:dv" opt
do
	case "$opt" in
	c)
		show_params[${#show_params[*]}]=cpu
		default_cpu="${OPTARG:-false}"
		;;
	d)	show_params[${#show_params[*]}]=desc
		;;
	v)	opt_v=1
		;;
	esac
done
shift $((OPTIND - 1))

selections_file="${1-`dirname -- $0`/selections}"
basedir=`dirname ${selections_file}`

if [ -r "${selections_file}" ]; then
	sed -e 's/#.*//' -e '/^[ 	]*$/d' < ${selections_file} |
	while read -a sel
	do
		declare -A value=( [cpu]="$default_cpu" )
		for item in "${sel[@]}" ; do
			item_name="${item%=*}"
			for param in ${all_params[*]} ; do
				if [ "${item_name}" = "${param}" ] ; then
					value[${item_name}]="${item#*=}"
				fi
			done
		done
		if [ -d "${basedir}/${value[dir]}" ] ; then
			for param in ${show_params[*]} ; do
				if [ -n "$opt_v" ] ; then
					printf '%s="%s" ' ${param} "${value[$param]}"
				else
					printf '%s\t' "${value[$param]}"
				fi
			done
			printf "\n"
		fi
	done
fi

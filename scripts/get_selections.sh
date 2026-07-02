#! /bin/bash

INFO_FILE=pidp_info

DESCRIPTION="
Create a list of all available selections. Each operating system directory
contains a '${INFO_FILE}' metadata file. The list of selections is created
from that metadata. The default output format is human readable, including
the octal switch value and either the description of the OS (if available)
or the name of the OS directory (if no description is available). The
verbose output includes multiple attributes and is intended for consumption
by other scripts (notably pidp.sh).

The default operating system is specified by a filesystem entry named
'default'. It may either be a symlink to the directory for the default OS
or an ordinary file containing the name of that directory.
"

#
# -cDEFAULT: print CPU name, DEFAULT if none
# -d: print description
# -s: print service command
# -v: print in verbose (name=value) format
# -wN: number of octal digits in csw (width)

declare -a all_params=( csw dir cpu desc svc )
declare -a show_params=( csw dir )

opt_cpu=
default_cpu=
csw_width=4
opt_v=

while getopts ":c:dsv" opt ; do
    case "$opt" in
    c)  show_params[${#show_params[*]}]=cpu
        default_cpu="${OPTARG:-false}"
        ;;
    d)  show_params[${#show_params[*]}]=desc
        ;;
    s)  show_params[${#show_params[*]}]=svc
        ;;
    v)  opt_v=1
        ;;
    w)  csw_width="${OPTARG:-4}"
        ;;
    ?)  echo -n "Usage: $0 "
        echo -n "[-c default_cpu] "
        echo -n "[-d] "
        echo -n "[-v] "
        echo -n "[-wN] "
        echo -n "[dir] "
        echo
        echo "${DESCRIPTION}"
        echo "Options:"
        echo "-c default_cpu    Show CPU, use 'default_cpu' if none is specified"
        echo "-d                Show description"
        echo "-s                Print service command"
        echo "-v                Print in verbose (name=value) format"
        echo "-wN               Number of octal digits in csw value (default is 4)"
        echo "dir               Directory (e.g., /opt/pidp11/systems)"
        exit 1
        ;;
    esac
done
shift $((OPTIND - 1))

basedir="${1-.}"

if [[ -d "${basedir}" ]] ; then
    cd "${basedir}"
    for dir in *; do
        real_dir="${dir}"
        if [[ "${dir}" = "default" ]]; then
            if [[ -h "${dir}" ]]; then
                # "default" is a symlink
                real_dir="$(readlink ${dir})"
            elif [[ -f "${dir}" ]]; then
                # "default" contains the name of the directory
                real_dir="$(cat ${dir})"
            fi
        fi
                
        if [[ -d "${real_dir}" && -r "${real_dir}/${INFO_FILE}" ]]; then
            declare -A values=( [dir]="${real_dir}" )

            # initialize values[cpu] to default
            if [[ -n "${default_cpu}" ]]; then
                values[cpu]="${default_cpu}"
            fi

            # read the info file and set all parameters it defines
            while read line; do
                line="${line%#*}"
                key="${line%=*}"
                if [[ -n "${key}" ]]; then values[${key}]="${line#*=}" ; fi
            done < ${real_dir}/${INFO_FILE}
            
            # special case for the "default" entry
            # it inherits the attributes of its reference
            # but the console switch setting is 0000
            if [[ "${dir}" = "default" ]]; then
                values[csw]="0000"
            fi

            # parse the original "csw" attribute (which could be octal,
            # decimal, or hexadecimal) and reformat as csw_width octal digits
            declare -i csw="${values[csw]}"
            printf -v values[csw] "%0.*o" ${csw_width} ${csw}

            if [[ -n "${values[csw]}" ]]; then
                if [[ -z "${opt_v}" ]]; then
                    printf '%s\t' ${values[csw]}
                    if [[ -n "${values[desc]}" ]]; then
                        printf '%s\n' "${values[desc]}"
                    else
                        printf '%s\n' "${values[dir]}"
                    fi
                else
                    for param in ${show_params[*]}; do
                        if [[ -n "${values[$param]}" ]]; then
                            printf '%s="%s" ' ${param} "${values[${param}]}"
                        fi
                    done
                    printf '\n'
                fi
            fi
        fi
    done | sort
fi

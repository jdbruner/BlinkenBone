#! /bin/bash
#
# Process the "selections" file, printing its information only for
# directories that exist.
#
# -cDEFAULT: print CPU name, DEFAULT if none
# -d: print description
# -v: print in verbose (name=value) format
# -wN: number of octal digits in csw (width)

INFO_FILE=pidp_info

declare -a all_params=( csw dir cpu desc )
declare -a show_params=( csw dir )

opt_cpu=
default_cpu=
csw_width=4
opt_v=

while getopts ":c:dv" opt ; do
    case "$opt" in
    c)  show_params[${#show_params[*]}]=cpu
        default_cpu="${OPTARG:-false}"
        ;;
    d)  show_params[${#show_params[*]}]=desc
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
        exit 1
        ;;
    esac
done
shift $((OPTIND - 1))

basedir="${1-.}"

if [ -d "${basedir}" ] ; then
    cd "${basedir}"
    for dir in *; do
        if [ -d ${dir} -a -r ${dir}/${INFO_FILE} ]; then
            declare -A values=( [dir]="${dir}" )

            # initialize values[cpu] to default
            if [ -n "${default_cpu}" ]; then
                values[cpu]="${default_cpu}"
            fi

            # read the info file and set all parameters it defines
            while read line; do
                line="${line%#*}"
                key="${line%=*}"
                if [ -n "${key}" ]; then values[${key}]="${line#*=}" ; fi
                #values[${line%=*}]="${line#*=}"
            done < ${dir}/${INFO_FILE}
            
            # special case the "default" symlink
            # it inherits the attributes of its reference
            # but the console switch setting is 0000
            if [ -h ${dir} -a "${dir}" = "default" ]; then
                values[csw]="0000"
                values[dir]="$(readlink ${dir})"
            fi

            # parse the original "csw" attribute (which could be octal,
            # decimal, or hexadecimal) and reformat as csw_width octal digits
            declare -i csw="${values[csw]}"
            printf -v values[csw] "%0.*o" ${csw_width} ${csw}

            if [ -n "${values[csw]}" ]; then
                if [ -z "${opt_v}" ]; then
                    printf '%s\t' ${values[csw]}
                    if [ -n "${values[desc]}" ]; then
                        printf '%s\n' "${values[desc]}"
                    else
                        printf '%s\n' "${values[dir]}"
                    fi
                else
                    for param in ${show_params[*]}; do
                        if [ -n "${values[$param]}" ]; then
                            printf '%s="%s" ' ${param} "${values[${param}]}"
                        fi
                    done
                    printf '\n'
                fi
            fi
        fi
    done | sort
fi

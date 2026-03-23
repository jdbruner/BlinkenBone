#! /bin/bash

# pidp server script
#
# This script should be run as an ordinary user (not root)
#

PIDP_DIR=
DEFAULT_CPU=
GETCSW=
BOOTFILE=
CPU_SUFFIX=
REALCONS_PANEL=

USE_REALCONS=	# default to running with no panel
USE_PIPANEL=
USE_JAVACONS=

PIDP_TYPE=11	# default to PiDP11
SINGLE_RUN=	# default to looping until simh exits with nonzero status

VERBOSE=

while getopts ":b:c:d:g:jprs:R:vx:" opt
do
	case ${opt} in
	b)	BOOTFILE="${OPTARG}" ;;
	c)	DEFAULT_CPU="${OPTARG}" ;;
	d)	PIDP_DIR="${OPTARG}" ;;
	g)	GETCSW="${OPTARG}" ;;
	j)	USE_REALCONS= USE_PIPANEL= USE_JAVACONS=1 ;;
	p)	USE_REALCONS= USE_PIPANEL=1 USE_JAVACONS= ;;
	r)	USE_REALCONS=1 USE_PIPANEL= USE_JAVACONS= ;;
	R)	USE_REALCONS=1 USE_PIPANEL= USE_JAVACONS=
		REALCONS_PANEL="${OPTARG}"  ;;
	s)	SINGLE_RUN=1 GETCSW="scansw00 -v${OPTARG}" ;;
	v)	VERBOSE=1 ;;
	x)	case "${OPTARG}" in
		10|11) PIDP_TYPE="${OPTARG}" ;;
		*) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
		esac ;;
	*)	echo -n "Usage: $0 "
		echo -n "[-b bootfile] "
		echo -n "[-c default_cpu] "
		echo -n "[-d directory] "
		echo -n "[-g getcsw_program] "
		echo -n "[-j | -p | -r | -R realcons_panel ] "
		echo -n "[-s switch_value ] "
		echo -n "[-x10 | -x11] "
		echo -n "[-v] "
		echo
		exit 1 ;;
	esac
done

# defaults per PiDP type (if not defined by an argument above)
case ${PIDP_TYPE} in
10)
	if [ -n "${USE_REALCONS}" ]; then
		: ${REALCONS_PANEL:=PDP10-KA10}
	else
		: ${REALCONS_PANEL:=PDP10-KI10}
	fi
	: ${PIDP_DIR:=/opt/pidp10}
	: ${DEFAULT_CPU:=pdp10-ka}
	;;
11)
	: ${REALCONS_PANEL:="11/70"}
	: ${PIDP_DIR:=/opt/pidp11}
	: ${DEFAULT_CPU:=pdp11}
	;;
esac

# set parameters based upon type of panel
if [ -n "${USE_REALCONS}" ] ; then
	# Use REALCONS/BlinkenBone server (assumed to have been started)
	: ${GETCSW:="getcsw -p${REALCONS_PANEL}"} ${BOOTFILE:=boot.ini}
	CPU_SUFFIX=_realcons
elif [ -n "${USE_JAVACONS}" ] ; then
	# use Java panel server (assumed to have been started)
	: ${GETCSW:=scansw00} ${BOOTFILE:=boot.ini}
	CPU_SUFFIX=_realcons
elif [ -n "${USE_PIPANEL}" ] ; then
	# Use simh with built-in access to PiDP panel via GPIO
	: ${GETCSW:=scansw10} ${BOOTFILE:=boot.pidp}
	CPU_SUFFIX=_pipanel
	REALCONS_PANEL=
else
	# Use simh with no front panel
	: ${GETCSW:=scansw00} ${BOOTFILE:=boot.ini}
	CPU_SUFFIX=
	REALCONS_PANEL=
fi
export REALCONS_PANEL

PIDP_BIN=${PIDP_DIR}/bin
PIDP_SYSTEMS=${PIDP_DIR}/systems

if [ -n "${VERBOSE}" ]; then
	for x in PIDP_DIR DEFAULT_CPU CPU_SUFFIX \
		 BOOTFILE REALCONS_PANEL GETCSW SINGLE_RUN
	do
		eval echo ${x}=$\{$x}
	done
fi

if [ -n "${USE_REALCONS}" -o -n "${USE_JAVACONS}" ]; then
	# make sure the panel server is ready
	# give up after waiting half a minute
	declare -i n=0
	until rpcinfo -T tcp localhost 99 1 > /dev/null 2>&1 ; do
		if ((++n == 30)); then
			if [ -n "${USE_REALCONS}" ]; then
				echo 'PiDP panel server is not running'
			else
				echo 'Java panel server is not running'
			fi
			exit 1
		fi
		sleep 1
	done
fi

while
	# select system using low 12 bits of data switches
	eval declare -A selections=(
	    $(bash ${PIDP_SYSTEMS}/get_selections.sh -c${DEFAULT_CPU} -d -v |
	    sed -e 's/csw="\([^"]*\)"\(.*\)/[\1]=@\2@/' \
		-e "s/@/'/g" \
		-e 's/ \([a-zA-Z]*\)=\([^ 	]*\)/ [\1]=\2/g' ))
	csw=$(${PIDP_BIN}/${GETCSW} -o4 -0 -n12)
	eval declare -A sel=(${selections[${csw:-"0000"}]})
	dir=${sel[dir]-default}
	cpu=${sel[cpu]-${DEFAULT_CPU}}${CPU_SUFFIX}
	desc=${sel[desc]:-${dir}}
	echo "*** booting ${desc} ***"
	# exit the loop on failure (non-zero exit status)
	(cd ${PIDP_SYSTEMS}/${dir} &&
	    exec ${PIDP_BIN}/${cpu} -q ${BOOTFILE}) && [ -z "${SINGLE_RUN}" ]
do
	:
done

#! /bin/bash
#
# Build everything for PiDP. Unlike the original BlinkenBone project, this
# only builds on the local machine (no cross-compiling).
#
# This script ensures the necessary (apt) package dependencies are
# present and then invokes make to do the real work.
#

OBJDIR="../BIN"
APTDONEFILE="$OBJDIR/.aptdone"
VERBOSE=
declare -A PIDP_TARGETS

PACKAGES="build-essential ed ant default-jdk net-tools rpcbind screen \
  libtirpc-dev libsdl2-dev libpcap-dev libreadline-dev \
  libpcre2-dev libedit-dev libpng-dev libvdeplug-dev"

if [[ -e /usr/bin/raspi-config ]]; then
    PACKAGES="${PACKAGES} libgpiolib-dev"
fi

while getopts ":vx:" opt
do
    case ${opt} in
    v)  VERBOSE=1 ;;
    x)  case "${OPTARG}" in
        10|11)  PIDP_TARGETS[${OPTARG}]="pidp${OPTARG}" ;;
        *) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
        esac ;;
    *)  echo -n "Usage: $0 "
        echo -n "[-x10] [-x11] "
        echo -n "[-v] "
        echo
	echo "-v	Verbose"
	echo "-x10	Build pidp10"
	echo "-x11	Build pidp11"
        exit 1
        ;;
    esac
done

# stop on error
set -e

if [[ ! -e $APTDONEFILE ]]; then
    (set -x; sudo apt-get install $PACKAGES)
    mkdir -p $OBJDIR
    touch $APTDONEFILE
fi

if [[ -z "${PIDP_TARGETS[*]}" ]]; then
    PIDP_TARGETS[11]="pidp11"
fi

if [[ -n "$VERBOSE" ]]; then set -x; fi

exec make ${PIDP_TARGETS[*]}

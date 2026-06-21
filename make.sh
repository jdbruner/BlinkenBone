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
PIDP_TYPE=11            # default to PiDP11
VERBOSE=

PACKAGES="build-essential ant default-jdk rpcbind screen \
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
        10|11)  PIDP_TYPE="${OPTARG}" ;;
        *) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
        esac ;;
    *)  echo -n "Usage: $0 "
        echo -n "[-x10 | -x11] "
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

if [ ! -e $APTDONEFILE ]; then
    (set -x; sudo apt install $PACKAGES)
    mkdir -p $OBJDIR
    touch $APTDONEFILE
fi

if [ -n "$VERBOSE" ]; then set -x; fi

exec make pidp${PIDP_TYPE}

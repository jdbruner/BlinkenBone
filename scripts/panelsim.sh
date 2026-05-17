#! /bin/bash

# Java panel simulation

DESCRIPTION="
Starts a Java panel server and then invokes the pidp.sh script
to launch a simh client that connects to that server

This should be run as an ordinary user (not root)
"

PIDP_DIR=
JAVA_APP=
JAVA_ARGS=
PIDP_ARGS=

PIDP_TYPE=11 # default to PiDP11

SWITCHES=
VERBOSE=

while getopts ":d:s:vx:" opt
do
	case ${opt} in
	d)	PIDP_DIR="${OPTARG}" ;;
	s)	SWITCHES="${OPTARG}" ;;
	v)	VERBOSE=1 ;;
	x)	case "${OPTARG}" in
		10|11) PIDP_TYPE="${OPTARG}" ;;
		*) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
		esac ;;
	*)	echo -n "Usage: $0 "
		echo -n "[-d directory] "
		echo -n "[-s switches] "
		echo -n "[-x10 | -x11] "
		echo -n "[-v] "
		echo
		echo "${DESCRIPTION}"
		echo "Options:"
		echo "-d directory	PiDP directory (e.g., /opt/pidp11)"
		echo "-s switches	Run once using 'switches' to select the OS"
		echo "-v		Verbose output"
		echo "-x10		Simulate PDP-10"
		echo "-x11		Simulate PDP-11"
		exit 1 ;;
	esac
done

# settings and defaults per PiDP type (if not defined by an argument above)
case ${PIDP_TYPE} in
10)
	JAVA_APP="blinkenbone.panelsim.panelsimKI10.PanelsimKI10_app"
	: ${PIDP_DIR:=/opt/pidp10}
	;;
11)
	JAVA_APP="blinkenbone.panelsim.panelsim1170.Panelsim1170_app"
	JAVA_ARGS="--addr_select 1 --data_select 1"
	PIDP_ARGS="-g getcsw"
	: ${PIDP_DIR:=/opt/pidp11}
	;;
esac

if [ -n "${SWITCHES}" ] ; then
	PIDP_ARGS="-s ${SWITCHES}"
fi

PIDP_BIN=${PIDP_DIR}/bin
PIDP_JAR=${PIDP_BIN}/panelsim_all.jar

if [ -n "$VERBOSE" ]; then set -x ; fi

# panel server requires X11
if [ -z "${DISPLAY}" ]; then
	echo "Panel server requires a graphical environment (X11/Wayland)"
	exit 1
fi

# check whether a panel server (either PiDP or Java) is already running
if rpcinfo -T tcp localhost 99 1 > /dev/null 2>&1 ; then
	echo "Only one panel server can be running at a time, and one is"
	echo "already active"
	exit 1
fi

# start the Java panel server
java -classpath ${PIDP_JAR} ${JAVA_APP} ${JAVA_ARGS} &
PANELSIM_PID="$!"

# invoke the PiDP service script, which will repeatedly
# start a simh simulator (until it exits with a nonzero status)
${PIDP_BIN}/pidp.sh -x${PIDP_TYPE} -d ${PIDP_DIR} -j ${PIDP_ARGS}

# clean up (kill) the Java panel server
kill ${PANELSIM_PID}

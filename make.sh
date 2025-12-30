#! /bin/bash
#
# This is a revision of the original make.sh script from the BlinkenBone project
# which only builds on the local machine (no cross-compiling) and currently
# builds the PDP-11 emulator, Java panel simulation, the PiDP11 panel server,
# and the utility to read the PiDP11 switch register.
#

export USE_REALCONS=1
if [ -e /usr/bin/raspi-config ]; then
    MAKE_TARGET_NAME=RaspberryPi
    export MAKE_TARGET_ARCH=RPI USE_PIDP11=1
 elif [ -e /usr/bin/bbb-config ]; then
    MAKE_TARGET_NAME=BeagleBoneBlack
    export MAKE_TARGET_ARCH=BBB
 else
    case $(uname -m) in
        i386|i486|i586|i686)
            MAKE_TARGET_NAME=x86
            export MAKE_TARGET_ARCH=X86
            ;;
        x86_64)
            MAKE_TARGET_NAME=x64
            export MAKE_TARGET_ARCH=X64
            ;;
        *)
            echo Unsupported machine type: $(uname -m)
            exit 1
            ;;
    esac
 fi

# stop on error
set -e

# Debugging:
# set -x

# needed packages:
PACKAGES="\
  ant default-jdk rpcbind screen \
  libgpiod-dev libtirpc-dev libsdl2-dev libpcap-dev libreadline-dev libpcre2-dev libedit-dev libpng-dev libvdeplug-dev"
(set -x; sudo apt install $PACKAGES)

# libgpiod has a breaking change with v2
if apt-cache show libgpiod-dev | grep -q '^Version: 1\.'; then
    USE_LIBGPIOD_V1=1 export USE_LIBGPIOD_V1
fi

# compile all binaries for all platforms
pwd
MAKEOPTIONS=--silent
MAKETARGETS="clean all"

# optimize all compiles, see makefiles
#export MAKE_CONFIGURATION=DEBUG
export MAKE_CONFIGURATION=RELEASE

(
    # The Blinkenlight API test client for all platforms.
    # This also builds the blinkenlight_api interface
    cd blinkenlight_test
    echo ; echo "*** blinkenlight_test for $MAKE_TARGET_NAME"
    make $MAKEOPTIONS $MAKETARGETS
)

(
    # PDP-11 simh
    echo ; echo "*** client11 - pdp11 with REALCONS ${USE_PIDP11+and PIDP11} for $MAKE_TARGET_NAME"
    cd ..
    make pdp11 && mv BIN/pdp11 BIN/client11
)

(
    # All classes and resources for all Java panels into one jar
    echo ; echo "*** Java panel server"
    cd javapanelsim
    ant -f build.xml compile jar
)

if [ $MAKE_TARGET_ARCH = RPI ]; then
    (
        # the Blinkenlight API server for Oscar Vermeulen's PiDP11
        cd pidp_server/pidp11
        echo ; echo "*** server11 - blinkenlight_server for PiDP11"
        make $MAKEOPTIONS $MAKETARGETS
    )
    (
        # utility for the PiDP11 that uses the pidp11 server
        # (alternative to Oscar Vermeulen's scansw)
        cd blinkenlight_getcsw/pidp11
        echo ; echo "*** blinkenlight_getcsw for PiDP11"
        make $MAKEOPTIONS $MAKETARGETS
    )
fi

echo
echo "All OK!"
exit 0

# the following is not (yet) updated, so don't build any of it
(
    # the Blinkenlight API server for BlinkenBus is only useful on BEAGLEBONE
    # Simulation and syntax test modes work also on desktop Linuxes
    cd blinkenlight_server
    echo ; echo "*** blinkenlight_server for $MAKE_TARGET_NAME"
    make $MAKEOPTIONS $MAKETARGETS
)

if [ $MAKE_TARGET_ARCH = BBW -o $MAKE_TARGET_ARCH = BBB ]; then
    (
        # the Blinkenlight API server for BlinkenBoard PDP-15
        cd blinkenlight_server_pdp15
        echo ; echo "*** blinkenlight_server for PDP-15 on $MAKE_TARGET_NAME"
        make $MAKEOPTIONS $MAKETARGETS
    )
fi

if [ $MAKE_TARGET_ARCH = RPI ]; then
    (
        # the Blinkenligt API server for Oscar Vermeulen's PiDP8
        cd pidp_server/pidp8
        echo ; echo "*** blinkenlight_server for PiDP8"
        make $MAKEOPTIONS $MAKETARGETS
    )
fi

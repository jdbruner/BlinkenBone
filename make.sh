#! /bin/sh
#
# Build everything. Unlike the original BlinkenBone project, this
# only builds on the local machine (no cross-compiling).
#
# This script ensures the necessary (apt) package dependencies are
# present and then invokes make to do the real work.
#

OBJDIR="../BIN"
APTDONEFILE="$OBJDIR/.aptdone"

PACKAGES="ant default-jdk rpcbind screen \
  libgpiod-dev libtirpc-dev libsdl2-dev libpcap-dev libreadline-dev \
  libpcre2-dev libedit-dev libpng-dev libvdeplug-dev"

# stop on error
set -e

if [ ! -e $APTDONEFILE ]; then
    (set -x; sudo apt install $PACKAGES) && touch $APTDONEFILE
fi

exec make 

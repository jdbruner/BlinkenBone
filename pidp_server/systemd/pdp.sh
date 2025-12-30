#! /bin/bash
# PiDP11 client - lives in client user directory, executed from .profile
if [ $(screen -ls pidp11 | egrep '[0-9]+\.pidp11' | wc -l) -ne 0 ]; then
	screen -d -r
else
	echo "PIDP11 server is not running"
fi

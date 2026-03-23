#! /bin/bash
# PiDP10 client - lives in client user directory, executed from .profile
if [ $(screen -ls pidp10 | egrep '[0-9]+\.pidp10' | wc -l) -ne 0 ]; then
	screen -d -r pidp10
else
	echo "PIDP10 server is not running"
fi

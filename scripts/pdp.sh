#! /bin/bash
# PiDP client - lives in client user directory, executed from .profile
PIDP_TYPE=11
PIDP_NAME="pidp${PIDP_TYPE}"
if [ $(screen -ls ${PIDP_NAME} | egrep "[0-9]+\.${PIDP_NAME}" | wc -l) -ne 0 ]; then
	screen -d -r ${PIDP_NAME}
else
	echo "PiDP${PIDP_TYPE} panel server is not running"
fi

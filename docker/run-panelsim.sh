#! /bin/bash
#
# Run a panel simulation in the Docker container
# Create a copy of XAUTHORITY for the container that is
# massaged to match any host
#
: ${DISPLAY?} ${XAUTHORITY?}
XAUTHTEMP=$(mktemp)
xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | xauth -f ${XAUTHTEMP} nmerge -
XAUTHORITY=${XAUTHTEMP} docker compose up -d ; rm ${XAUTHTEMP}

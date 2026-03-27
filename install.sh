#! /bin/bash
# Install PiDP10 or PiDP11
# This assumes it has been fully built with make.sh

#if [[ $(id -u) -ne 0 ]]; then
#   echo "This script must be run as root"
#   exit 1
#fi

if [[ ! -e /usr/bin/raspi-config ]]; then
    echo "PiDP can only be installed on a Raspberry Pi"
    exit 1
fi

ETHERNET_RAW=       # default to NAT/slirp or TUN/TAP
KNOB_REVERSE=       # don't reverse PDP11 knob rotation
PIDP_TYPE=11        # default to PiDP11
USE_REALCONS=       # use REALCONS - default depends upon PIDP_TYPE
VERBOSE=

while getopts ":ekrvx:" opt
do
    case ${opt} in
    e)  ETHERNET_RAW=1 ;;
    k)  KNOB_REVERSE=1 ;;
    r)  USE_REALCONS=1 ;;
    v)  VERBOSE=1 ;;
    x)  case "${OPTARG}" in
        10|11)  PIDP_TYPE="${OPTARG}" ;;
        *) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
        esac ;;
    *)  echo -n "Usage: $0 "
        echo -n "[-e] "
        echo -n "[-k] "
        echo -n "[-r] "
        echo -n "[-x10 | -x11] "
        echo -n "[-v] "
        echo
        exit 1
        ;;
    esac
done

# PiDP11 always uses REALCONS; PiDP10 can use REALCONS or PIPANEL
# KNOB_REVERSE only applies to PiDP11 (server11)
if [[ "${PIDP_TYPE}" = 11 ]]; then
    USE_REALCONS=1
else
    KNOB_REVERSE=
fi

PIDP=pidp${PIDP_TYPE}
PIDP_USER=${PIDP}
PIDP_GROUP=${PIDP}
PIDP_HOME=/home/${PIDP_USER}
PIDP_HOME_BIN=${PIDP_HOME}/bin
PIDP_OPT=/opt/${PIDP}
PIDP_OPT_BIN=${PIDP_OPT}/bin
PIDP_OPT_SYSTEMS=${PIDP_OPT}/systems

SYSTEMD_LOCAL_SYSTEM=/usr/local/lib/systemd/system

SIMH_BIN=../BIN
SRC_SERVER_DIR=pidp_server
SRC_BIN_DIR=${SRC_SERVER_DIR}/bin
SRC_SYSTEMS_DIR=${SRC_SERVER_DIR}/systems
SRC_PIDP_SYSTEMS_DIR=${SRC_SYSTEMS_DIR}/${PIDP}
SRC_SYSTEMD_DIR=${SRC_SERVER_DIR}/systemd
SRC_PIDP_SYSTEMD_DIR=${SRC_SYSTEMD_DIR}/${PIDP}

case ${PIDP_TYPE} in
10)
    if [[ -n "${USE_REALCONS}" ]]; then
        # REALCONS
        PIDP_SIMS="$(echo pdp10-k{a,i,l,s}{,_realcons})"
        PIDP_PIPANEL_SIMS=
        PIDP_EXES="${PIDP_SIMS} scansw00 server10 getcsw"
        PIDP_INSTALL_SERVICES="pidp10tv11 pidp10cbridge pidp10panel pidp10-realcons"
        PIDP_ENABLE_SERVICES="rpcbind ${PIDP_INSTALL_SERVICES}"
    else
        # PIPANEL
        PIDP_SIMS="$(echo pdp10-k{a,i,l,s}{,_pipanel})"
        PIDP_PIPANEL_SIMS="$(echo pdp10-k{a,i,l,s}_pipanel)"
        PIDP_EXES="${PIDP_SIMS} scansw00 scansw10"
        PIDP_INSTALL_SERVICES="pidp10tv11 pidp10cbridge pidp10-pipanel"
        PIDP_ENABLE_SERVICES="${PIDP_INSTALL_SERVICES}"
    fi
    ;;
11)
    # PiDP11 always uses REALCONS
    PIDP_SIMS="pdp11 pdp11_realcons"
    PIDP_PIPANEL_SIMS=
    PIDP_EXES="${PIDP_SIMS} server11 getcsw scansw00"
    PIDP_INSTALL_SERVICES="pidp11panel pidp11"
    PIDP_ENABLE_SERVICES="rpcbind ${PIDP_INSTALL_SERVICES}"
    ;;
esac

SYSCTL_TIOCSTI=dev.tty.legacy_tiocsti
if [ -n "${USE_REALCONS}" -a $(sysctl ${SYSCTL_TIOCSTI} -n) = 0 ]; then
    echo Settings ${SYSCTL_TIOCSTI}=1 to enable REALCONS injection to the simh console
    sysctl ${SYSCTL_TIOCSTI}=1
fi

# Create user ("pidp10" or "pidp11") if it does not already exist
# Create ~/bin and populate with the pdp.sh script
# exec pdp.sh as the final action in .profile
if ! id ${PIDP_USER} > /dev/null 2>&1; then
    useradd -m ${PIDP_USER}
elif [[ ! -d ${PIDP_HOME} ]]; then
    echo "User '${PIDP_USER}' exists but home directory '${PIDP_HOME}' does not"
    exit 1
fi

if [[ ! -e ${PIDP_OPT_BIN} ]]; then
    mkdir -m 755 ${PIDP_OPT_BIN} && \
        chown ${PIDP_USER}:${PIDP_GROUP} ${PIDP_OPT_BIN}
fi

if ! grep -q pdp.sh ${PIDP_HOME}/.profile ; then
    echo 'exec ${PIDP_HOME}/bin/pdp.sh' >> ${PIDP_HOME}/.profile
fi

# Create /opt/pidpxx if it does not exist
# Copy in the executables and pidp.sh script
# Edit the default PIDP_TYPE in the installed pidp.sh
#
mkdir -p -m 755 ${PIDP_OPT}
install -m 755 ${SRC_BIN_DIR}/pidp.sh ${PIDP_OPT_BIN}
install -m 755 ${SRC_BIN_DIR}/get_selections.sh ${PIDP_OPT_BIN}
for exe in ${PIDP_EXES}; do
    install -m 755 ${SIMH_BIN}/${exe} ${PIDP_OPT_BIN}
done
if [[ "${PIDP_TYPE}" != 11 ]]; then
    cp ${PIDP_OPT_BIN}/pidp.sh /tmp
    # ed ${PIDP_OPT_BIN}/pidp.sh <<-!EOF!
    ed /tmp/pidp.sh > /dev/null <<-!EOF!
        /PIDP_TYPE=11/s/11/${PIDP_TYPE}/g
        w
        q
    !EOF!
fi

# If /opt/pidpxx/systems already exists, leave it alone
# Otherwise, create an initial version (with idled as default)
#
if [[ ! -d ${PIDP_OPT_SYSTEMS} ]]; then
    cp ${SRC_SYSTEMS_DIR}/realcons.ini ${PIDP_OPT_SYSTEMS}
    cp -r ${SRC_PIDP_SYSTEMS_DIR} ${PIDP_OPT_SYSTEMS}
    ln -s idled ${PIDP_OPT_SYSTEMS}/default
fi

chown -R ${PIDP_USER}:${PIDP_GROUP} ${PIDP_OPT}

# If networking uses direct Ethernet access, then set the capabilities
if [[ -n "${ETHERNET_RAW}" ]]; then
    for sim in ${PIDP_SIMS}; do
        setcap cap_net_raw,cap_net_admin=+ep ${PIDP_OPT_BIN}/${sim}
    done
fi

# Install the systemd service files and start the services
# If KNOB_REVERSE is non-empty, invoke server11 with the -r flag
mkdir -p -m 755 ${SYSTEMD_LOCAL_SYSTEM}
for svc in ${PIDP_INSTALL_SERVICES}; do
    install -m 755 ${SRC_PIDP_SYSTEMD_DIR}/${svc}.service ${SYSTEMD_LOCAL_SYSTEM}
done

if [[ -n "${KNOB_REVERSE}" ]]; then
    ed ${SYSTEMD_LOCAL_SYSTEM}/pidp11panel.service <<-"!EOF!"
        /^ExecStart/s/server11/& -r/
        w
        q
    !EOF!
fi

# Finally, enable and start the services
for action in enable start; do
    for svc in ${PIDP_ENABLE_SERVICES}; do
        systemctl ${action} ${svc}
    done
done

#! /bin/bash
# Install PiDP10 or PiDP11
# This assumes it has been fully built with make.sh

if [[ $(id -u) -ne 0 ]]; then
    echo "This script must be run as root"
    exit 1
fi

umask 022

ETHERNET_RAW=       # default to NAT/slirp or TUN/TAP
KNOB_REVERSE=       # don't reverse PDP11 knob rotation
PIDP_TYPE=11        # default to PiDP11
PANEL_KIND=         # panel type (none, realcons, pipanel)
IS_RASPI=           # default to non-Raspberry Pi
IS_CONTAINER=       # default to non-container
VERBOSE=

if [[ -e /usr/bin/raspi-config ]]; then
    IS_RASPI=1  # this is a Raspberry Pi
fi

while getopts ":ceknprvx:" opt
do
    case ${opt} in
    c)  IS_CONTAINER=1 ;;
    e)  ETHERNET_RAW=1 ;;
    k)  KNOB_REVERSE=1 ;;
    n)  PANEL_KIND=none ;;
    p)  PANEL_KIND=pipanel ;;
    r)  PANEL_KIND=realcons ;;
    v)  VERBOSE=1 ;;
    x)  case "${OPTARG}" in
        10|11)  PIDP_TYPE="${OPTARG}" ;;
        *) echo "Unknown PiDP type (-x${OPTARG})" ; exit 1 ;;
        esac ;;
    *)  echo -n "Usage: $0 "
        echo -n "[-c] "
        echo -n "[-e] "
        echo -n "[-k] "
        echo -n "[-n] "
        echo -n "[-p] "
        echo -n "[-r] "
        echo -n "[-x10 | -x11] "
        echo -n "[-v] "
        echo
        echo "-c	Installation suitable for a container"
        echo "-e	Directly access Ethernet (vs TUN or NAT)"
        echo "-k	Reverse direction of knobs [PiDP11 only]"
        echo "-n	No panel - no switch/LED interface"
        echo "-p	PiPanel - use simh-integrated GPIO [PiDP10 only]
        echo "-r	Use REALCONS
        echo "-x10	Install PiDP10"
        echo "-x11	Install PiDP11"
        echo "-v	Verbose"
        exit 1
        ;;
    esac
done

# If this is a container install, treat as a non-Raspberry Pi
if [[ -n "${IS_CONTAINER}" ]]; then
    IS_RASPI=
fi

# Determine PANEL_KIND (use default value if not specified)
# Raspberry Pi:
#   PiDP11: defaults to realcons, pipanel is treated as realcons
#   PiDP10: all kinds supported, defaults to pipanel
# Non-Raspberry Pi: always none
# KNOB_REVERSE only applies to PiDP11 with realcons
if [[ -z "${IS_RASPI}" ]]; then
    PANEL_KIND=none
    KNOB_REVERSE=
elif [[ "${PIDP_TYPE}" = 11 ]]; then
    case ${PANEL_KIND:=realcons} in
    none)       KNOB_REVERSE= ;;
    realcons)   ;;
    pipanel)    PANEL_KIND=realcons KNOB_REVERSE= ;;
    esac
else # PiDP10
    : ${PANEL_KIND:=pipanel}
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
SRC_SCRIPTS_DIR=scripts
SRC_SYSTEMS_DIR=systems
SRC_PIDP_SYSTEMS_DIR=${SRC_SYSTEMS_DIR}/${PIDP}
SRC_SYSTEMD_DIR=systemd
SRC_PIDP_SYSTEMD_DIR=${SRC_SYSTEMD_DIR}/${PIDP}
SRC_JAVA_DIR=javapanelsim

# Determine binaries and services to install
# Always install REALCONS and non-panel clients
# No services are installed in a container
case ${PIDP_TYPE} in
10)
    case ${PANEL_KIND} in
    none)
        PIDP_SIMS="$(echo pdp10-k{a,i,l,s}{,_realcons})"
        PIDP_EXES="${PIDP_SIMS} scansw00 getcsw"
        PIDP_SERVERS=""
        PIDP_INSTALL_SERVICES="pidp10"
        PIDP_ENABLE_SERVICES="${PIDP_INSTALL_SERVICES}"
        ;;
    realcons)
        PIDP_SIMS="$(echo pdp10-k{a,i,l,s}{,_realcons})"
        PIDP_EXES="${PIDP_SIMS} scansw00 getcsw"
        PIDP_SERVERS="server10"
        PIDP_INSTALL_SERVICES="pidp10-server pidp10-client"
        PIDP_ENABLE_SERVICES="rpcbind ${PIDP_INSTALL_SERVICES}"
        ;;
    pipanel)
        # PIPANEL
        PIDP_SIMS="$(echo pdp10-k{a,i,l,s}{,_realcons,_pipanel})"
        PIDP_EXES="${PIDP_SIMS} scansw00 scansw10 getcsw"
        PIDP_SERVERS=""
        PIDP_INSTALL_SERVICES="pidp10-pipanel"
        PIDP_ENABLE_SERVICES="${PIDP_INSTALL_SERVICES}"
        ;;
    esac
    ;;
11)
    PIDP_SIMS="pdp11 pdp11_realcons"
    PIDP_PIPANEL_SIMS=
    PIDP_EXES="${PIDP_SIMS} getcsw scansw00"
    if [[ ${PANEL_KIND} = realcons ]]; then
        PIDP_SERVERS="server11"
        PIDP_INSTALL_SERVICES="pidp11-server pidp11-client"
        PIDP_ENABLE_SERVICES="rpcbind ${PIDP_INSTALL_SERVICES}"
    else
        PIDP_SERVERS=""
        PIDP_INSTALL_SERVICES="pidp11"
        PIDP_ENABLE_SERVICES="${PIDP_INSTALL_SERVICES}"
    fi
    ;;
esac
if [[ -n "${IS_CONTAINER}" ]]; then
    PIDP_SERVERS=""
    PIDP_INSTALL_SERVICES=""
    PIDP_ENABLE_SERVICES=""
fi

SYSCTL_TIOCSTI=dev.tty.legacy_tiocsti
SYSCTL_DIR=/etc/sysctl.d
SYSCTL_REALCONS_CONF=${SYSCTL_DIR}/99-realcons.conf

# stop on error, show individual commands if verbose
set -e
if [[ -n "$VERBOSE" ]]; then set -x; fi

if [[ "${PANEL_KIND}" = realcons && $(sysctl ${SYSCTL_TIOCSTI} -n) = 0 ]]; then
    echo Setting ${SYSCTL_TIOCSTI}=1 to enable REALCONS injection to the simh console
    echo "${SYSCTL_TIOCSTI} = 1" > ${SYSCTL_REALCONS_CONF}
    sysctl -p ${SYSCTL_REALCONS_CONF}
fi

# Create user ("pidp10" or "pidp11") if it does not already exist
# Add the ~/.screenrc file if not already present
# Create ~/bin and populate with the pdp.sh script
# Edit the PIDP_TYPE in the installed pdp.sh
# exec pdp.sh as the final action in .profile
if ! id ${PIDP_USER} > /dev/null 2>&1; then
    useradd -m ${PIDP_USER}
elif [[ ! -d ${PIDP_HOME} ]]; then
    echo "User '${PIDP_USER}' exists but home directory '${PIDP_HOME}' does not"
    exit 1
fi

if [[ ! -r ${PIDP_USER}/.screenrc ]]; then
    install -m 644 -o ${PIDP_USER} -g ${PIDP_GROUP} ${SRC_SCRIPTS_DIR}/screenrc -t ${PIDP_HOME}
fi

install -m 755 -o ${PIDP_USER} -g ${PIDP_GROUP} -D ${SRC_SCRIPTS_DIR}/pdp.sh -t ${PIDP_HOME}/bin
if [[ "${PIDP_TYPE}" != 11 ]]; then
    ed ${PIDP_HOME}/bin/pdp.sh <<!EOF! >& /dev/null
        /PIDP_TYPE=11/s/11/${PIDP_TYPE}/g
        w
        q
!EOF!
fi

if ! grep -q pdp.sh ${PIDP_HOME}/.profile ; then
    echo 'exec ${HOME}/bin/pdp.sh' >> ${PIDP_HOME}/.profile
fi
chown ${PIDP_USER}:${PIDP_GROUP} ${PIDP_HOME}/.profile

# Create /opt/pidpxx if it does not exist
# Copy in the executables and pidp.sh script
# Edit the default PIDP_TYPE in the installed scripts
#
install -m 755 -D ${SRC_SCRIPTS_DIR}/{pidp.sh,get_selections.sh,panelsim.sh} -t ${PIDP_OPT_BIN}
for exe in ${PIDP_EXES} ${PIDP_SERVERS}; do
    install -m 755 ${SIMH_BIN}/${exe} -t ${PIDP_OPT_BIN}
done
install -m 644 ${SRC_JAVA_DIR}/panelsim_all.jar -t ${PIDP_OPT_BIN}
if [[ "${PIDP_TYPE}" != 11 ]]; then
    for shfile in pidp.sh panelsim.sh ; do
        ed ${PIDP_OPT_BIN}/${shfile} <<!EOF! >& /dev/null
            /PIDP_TYPE=11/s/11/${PIDP_TYPE}/g
            w
            q
!EOF!
    done
fi

# If /opt/pidpxx/systems already exists, leave it alone
# Otherwise, create an initial version (with idled as default)
#
if [[ ! -d ${PIDP_OPT_SYSTEMS} ]]; then
    install -m 644 -D ${SRC_SYSTEMS_DIR}/realcons.ini -t ${PIDP_OPT_SYSTEMS}
    cp -r ${SRC_PIDP_SYSTEMS_DIR}/* ${PIDP_OPT_SYSTEMS}
    if [[ -n "${IS_CONTAINER}" ]]; then
        echo idled > ${PIDP_OPT_SYSTEMS}/default
    else
        ln -s idled ${PIDP_OPT_SYSTEMS}/default
    fi
fi

chown -R ${PIDP_USER}:${PIDP_GROUP} ${PIDP_OPT}
chmod -R go-w ${PIDP_OPT}

# If networking uses direct Ethernet access, then set the capabilities to enable this
# Otherwise, for a container install, set the capability for binding to privileged ports
if [[ -n "${ETHERNET_RAW}" ]]; then
    for sim in ${PIDP_SIMS}; do
        setcap cap_net_raw,cap_net_admin=+ep ${PIDP_OPT_BIN}/${sim}
    done
elif [[ -n "${IS_CONTAINER}" ]]; then
    for sim in ${PIDP_SIMS}; do
        setcap cap_net_bind_service=+ep ${PIDP_OPT_BIN}/${sim}
    done
fi

# Install the systemd service files
# If KNOB_REVERSE is non-empty, invoke server11 with the -r flag
for svc in ${PIDP_INSTALL_SERVICES}; do
    install -m 644 -D ${SRC_PIDP_SYSTEMD_DIR}/${svc}.service -t ${SYSTEMD_LOCAL_SYSTEM}
done

if [[ -n "${KNOB_REVERSE}" ]]; then
    ed ${SYSTEMD_LOCAL_SYSTEM}/pidp11panel.service <<"!EOF!" >& /dev/null
        /^ExecStart/s/server11/& -r/
        w
        q
!EOF!
fi

# Finally, enable and start the services (if any)
for svc in ${PIDP_ENABLE_SERVICES}; do
    for action in enable start; do
        systemctl ${action} ${svc}
    done
done

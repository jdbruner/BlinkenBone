# Debian Trixie with dependencies required to build/run SIMH
FROM debian:trixie AS simh-common
RUN <<!EOF!
	set -ex
	DEBIAN_FRONTEND=noninteractive export DEBIAN_FRONTEND
	apt-get update
	apt-get -y install --no-install-recommends ca-certificates curl \
	    libsdl2-2.0 libcap2-bin libpcap-dev default-jre procps bsdextrautils \
	    netbase net-tools rpcbind screen
!EOF!

# Get SIMH source, run make.sh and install.sh for both pidp10 and pidp11
FROM simh-common AS simh-builder
ARG GIT_REPO=https://github.com/jdbruner/simh
ENV DEBIAN_FRONTEND=noninteractive
RUN <<!EOF!
	set -ex
	echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/99dockeropts
	echo 'APT::Install-Recommends "false";' >> /etc/apt/apt.conf.d/99dockeropts
	apt-get install git sudo
!EOF!
WORKDIR /home
RUN <<!EOF!
	set -ex
	git clone --recurse-submodules ${GIT_REPO}
	cd simh/BlinkenBone
	./make.sh -v -x10 -x11
	./install.sh -c -v -x10 -e
	./install.sh -c -v -x11 -e
!EOF!

# Create image with pdp10/pdp11 executables
FROM simh-common
ARG PIDP=pidp11
ENV PIDP_SH=/opt/${PIDP}/bin/pidp.sh
RUN <<!EOF!
	set -ex
	useradd -m ${PIDP}
	mkdir -m 755 /opt/${PIDP}
	chown ${PIDP}:${PIDP} /opt/${PIDP}
!EOF!
WORKDIR /home/${PIDP}
COPY --from=simh-builder /opt/${PIDP} /opt/${PIDP}
USER ${PIDP}:${PIDP}
CMD ["/usr/bin/screen","-D","-m","-S","console","sh","-c","${PIDP_SH}"]

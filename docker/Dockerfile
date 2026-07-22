# Debian Trixie with dependencies required to build/run SIMH
FROM debian:trixie AS simh-common
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates curl \
    libsdl2-2.0 libcap2-bin libpcap-dev default-jre procps bsdextrautils \
    netbase net-tools rpcbind screen

# Get SIMH source, run make.sh and install.sh for both pidp10 and pidp11
FROM simh-common AS simh-builder
ARG GIT_REPO=https://github.com/jdbruner/simh.git
ENV DEBIAN_FRONTEND=noninteractive

RUN echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/99dockeropts
RUN echo 'APT::Install-Recommends "false";' >> /etc/apt/apt.conf.d/99dockeropts
RUN apt-get install git sudo

ADD --keep-git-dir ${GIT_REPO} /home/simh
WORKDIR /home/simh/BlinkenBone
RUN ./make.sh -v -x10 -x11
RUN ./install.sh -c -v -x10
RUN ./install.sh -c -v -x11

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

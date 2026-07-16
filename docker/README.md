## Docker

> **_Caveat_**: This is experimental.
The files in this directory are for illustration purposes
and must be edited for your specific configuration,
i.e, machine type, system directory/directories, networking.

To build Docker images ``pidp10`` and ``pidp11``,
use the command:
```bash
docker bake
```
These are Debian Trixie images, i.e., not Raspberry Pi-specific.

The PiDP10 image is configured with user ``pidp10``
and the directory ``/opt/pidp10``.
The PiDP11 image is configured with user ``pidp11``
and the directory ``/opt/pidp11``.

``/opt/pidpXX/bin`` contains the non-panel simh executables
(i.e., ``pdp11``, ``pdp10-ka``)
as well as the REALCONS clients
(i.e., ``pdp11_realcons``, ``pdp10-ka_realcons``).
``/opt/pidpXX/systems`` is a skeletal set of systems,
basically a couple of standalone idle applications.
``/opt/pidpXX/systems/default`` is a text file that contains
the name of the system to be executed.

When run in a container, the default system will be repeatedly
invoked within a ``screen`` session
unless/until simh exits with a non-zero status.

To run with a specific operating system, use a volume mount,
either to replace ``/opt/pidpXX/systems`` in its entirety
or to add a specific directory and override the default.
Execute ``screen`` within the container to access the simh console.
Add the ``CAP_NET_ADMIN`` and ``CAP_NET_RAW`` capabilities to the container.
For example:
```bash
echo rsx11mplus > $HOME/default
docker run --rm -d \
    -v$HOME/rsx11mplus:/opt/pidp11/systems/rsx11mplus \
    -v$HOME/default:/opt/pidp11/systems/default \
    --cap-add NET_ADMIN --cap-add NET_RAW \
    --name pidp11-rsx11m \
    pidp11
docker exec -it pidp11-rsx11m screen -r
```

### network
#### bridge
When using a docker ``bridge`` network
(including the ``default`` network),
configure simh networking in ``boot.ini``
to use slirp/NAT.
Expose network ports in simh
```
set xu enable
attach xu nat:tcp=2323:10.0.2.15:23,tcp=2121:10.0.2.15:21
```
and in the container
```bash
docker run --rm -d \
    -v$HOME/rsx11mplus:/opt/pidp11/systems/rsx11mplus \
    -v$HOME/default:/opt/pidp11/systems/default \
    --cap-add NET_ADMIN --cap-add NET_RAW \
    -p2323:2323 -p2121:2121 \
    --name pidp11-rsx11m \
    pidp11
```
The ``CAP_NET_ADMIN`` and ``CAP_NET_RAW`` capabilities
are not actually used,
but they must be granted to the container with ``--cap-add``
because the simh binaries have them in the filesystem.
If the ``--cap-add`` is omitted, simh won't run.

#### macvlan
The container and possibly the simh simulation share the
host's network interface,
which is set to promiscuous mode.
Create a docker network with an IP range that is not
in use by other hosts
(such as a different subnet)
and the gateway for your network, e.g.:
```bash
docker network create -d macvlan \
    --subnet '192.168.0.0/24' \
    --ip-range '192.168.0.224/27' \
    --gateway 192.168.0.1 \
    -o parent=eth0 pidpnet
```

The container will have an IP address in the specified range.
If simh exposes telnet ports for the console or serial ports,
connect to them with telnet to the container's IP address.
If ``boot.ini`` uses slirp/NAT networking,
the ports it exposes are also available at the container's IP address.

Alternatively, it is possible to put the emulated machine
directly on the local network by attaching the emulated network interface
to the container's network interface
(which shares the host's interface).
However, in order for this to work, the container's MAC address **must**
be the same as the MAC address for the emulated PiDP system.
For some systems -- such as 2.11BSD UNIX on the PDP-11 --
the MAC address is configured in the ``boot.ini`` file:
```
set xu enable
set xu mac=b8:27:eb:47:39:6f
attach xu eth0
```
and for the container by ``docker run``.
Note also that because the container and the simh system
share the same MAC address,
IP forwarding must be disabled in the container:
```bash
docker run --rm -d \
    --network pidpnet \
    -v$HOME/211bsd:/opt/pidp11/systems/211bsd \
    -v$HOME/default:/opt/pidp11/systems/default \
    --cap-add NET_ADMIN --cap-add NET_RAW \
    --mac-address b8:27:eb:47:39:6f \
    --sysctl net.ipv4.ip_forward=0 \
    --sysctl net.ipv4.conf.all.forwarding=0 \
    --name pidp11-211bsd \
    pidp11
```

Note that with DEC operating systems, such as RSX-11M+,
the address configured in ``boot.ini`` is not used.
Instead, the OS assigns the MAC address based upon its DECnet address.
(See [](https://groups.google.com/g/comp.sys.dec/c/pFIfamSmNl0).)
A DECnet address is of the form <it>area.node</it>.
The corresponding MAC address is
```
AA-00-04-00-xx-yy
```
where ``xx-yy`` are computed as follows:
```
value = (area * 1024) + node
xx = value % 0xff
yy = value >> 8
```
If the DECnet address in the OS is changed,
the container MAC address must have a corresponding change.
If you have more than one such container,
they each must have a different DECnet/MAC address
to avoid conflicts on your LAN.

The docker compose file ``compose-macvlan.yaml`` provides an example
of starting multiple containers for different operating systems.

### panelsim
When using X11 or Wayland, it is possible to run the Java panel
simulator in the container.
However, the experience is poor, due to a combination of the
thread scheduling and emulated network overhead.
For an example of how to do this,
see the ``run-panelsim.sh`` command
and ``compose-panelsim.yaml`` files.

### (No) GPIO
The docker images do not contain the REALCONS servers
(``server10``, ``server11``)
nor the simh binaries that directly access Pi GPIOs
(``pdp10-ka_pipanel``, ``pdp10-ki_pipanel``, etc.).
These rely upon memory-mapped access to ``/dev/gpiomem0``
and a Raspberry Pi-specific library (``libgpiolib``).
One option is to build the REALCONS servers with ``libgpiod``,
which is cross-platform,
and use ``/dev/gpiochip0``.
However, this approach has much higher system-call overhead
and (obviously) would still only work on a Pi.

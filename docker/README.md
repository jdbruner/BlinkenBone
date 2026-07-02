## Docker

To build Docker images for the PiDP10 and PiDP11,
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

To run with a specific system, use a volume mount,
either to replace ``/opt/pidpXX/systems`` in its entirety
or to add a specific directory and override the default.
Execute ``screen`` within the container to access the simh console.
Expose network ports as desired for connections into the container.
For example:
```bash
echo rsx11mplus > $HOME/default
docker run --rm -d \
    -v$HOME/rsx11mplus:/opt/pidp11/systems/rsx11mplus
    -v$HOME/default:/opt/pidp11/systems/default
    -p23:2323 -p21:2121
    --name pidp11-rsx11m
    pidp11
docker exec -it pidp11-rsx11m screen -r
```

### network
Currently, this only supports slirp/NAT networking in simh.
Configure port forwarding in simh (``boot.ini``)
and in ``docker run``.

### panelsim
When using X11 or Wayland, it is possible to run the Java panel
simulator in the container.
However, the experience is poor, due to a combination of the
thread scheduling and emulated network overhead.
For an example of how to do this,
see the ``run-panelsim.sh`` command
and ``compose-panelsim.yaml`` files.

### (No) GPIO
The images do not contain the REALCONS servers
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

# BlinkenBone - extend the SimH simulator with simulated console panels.

This Git repository is intended for use as a submodule to add the
BlinkenBone client, Java panel server, and PiDP server
sources to simh and open-simh.
It has been modified from its original structure to suit this purpose.

"BlinkenBone" is an architecture to connect simulators of vintage computers
with "Blinkenlight panels".
The panels can be vintage physical panels
enhanced with modern micro-Linux controllers,
graphical Java simulations of the panels,
or hardware emulated panels built around a Raspberry Pi
(i.e., the "PiDP" panels).
The interface between simulator and panel is
a network-based client/server model.

At the moment, DEC PDP-11/20, PDP-11/40, PDP-11/70, PDP-8/I, and PDP-10/KI10
and PDP-15 are available in the Java panel simulator.

PDP-11/70, PDP-10/KA10, and PDP-8 are available as PiDP servers when
running on Raspberry Pi.
The PiDP10 and PIDP11 servers are accompanied by associated control files
and support installation on the current (build) machine.
A PiDP8 server is also built, but to date it is untested.

This is derived from
(and significantly modified from)
``https://github.com/j-hoppe/BlinkenBone``.
It no longer builds for Blinkenlight panels
nor does it build cross-platform.
For those targets and builds,
consult the following:
- source: ``https://github.com/j-hoppe/BlinkenBone``
- documentation: ``https://www.retrocmp.com/projects/blinkenbone``
- j_hoppe@t-online.de

### Directory Structure

- ``3rdparty`` - software from others that are used to build REALCONS components:
  - ``3rdparty/jsap`` - Java Simple Argument Parser,
    used for command line arguments to the Java panel simulators.
  - ``3rdparty/oncrpc_win32`` - Open Network Computing Remote Procedure Call.
    used in Windows simh builds to communicate with REALCONS servers.
  - ``3rdparty/remotetea`` - Remote Tea ONC/RPC library,
    used in Java panel servers to communicate with REALCONS clients.
- ``blinkenlight_api`` - Joerge Hoppe's Blinkenlight API,
  used by clients (simh, getcsw, blinkenlight_test)
  and servers (PiDP and Java panel servers).
  - ``blinkenlight_api/java`` - used by Java panel servers
  - ``blinkenlight_api/rpcgen_java`` - not currently used
  - ``blinkenlight_api/rpcgen_linux`` - generated at build time and
    used by Linux REALCONS clients and servers
  - ``blinkenlight_api/rpcgen_win32`` - used by Windows REALCONS clients
- ``blinkenlight_test`` - Joerge Hoppe's test program,
  which connects to a specified host and can examine and modify
  the state of any REALCONS panel that is actively being served there.
- ``common`` - a set of utility source files that are shared by multiple programs.
- ``getcsw`` - Get Console Switches -
  connects to a REALCONS panel (Java or PiDP panel),
  reads the switches,
  and outputs their current value in a specified format
  (radix, field width, zero padding, ...).
- ``javapanelsim`` - Java panel simulators,
  which support REALCONS clients on Linux or Windows.
  (These can be used as an alternative to PiDP hardware panels.)
- ``scansw`` - Scan Switches -
  on a Raspberry Pi, connects to the PiDP panel,
  reads the switches,
  and outputs their current value in a specified format
  (similar to the output of ``getcsw``).
  This directory builds two targets:
  - ``scansw10`` - for PiDP10 (Raspberry Pi only)
  - ``scansw00`` - gets the switch value from a command-line argument.
    This can be used in place of ``scansw10`` (or ``getcsw``)
    to emulate a specified switch value.
- ``scripts`` - Bash shell scripts :
  - ``get_selections.sh`` - creates a description of the available operating systems
    in ``/opt/pidpXX/systems``.
    Each directory contains a ``pidp_info`` file that specifies the
    console switch setting for the O/S,
    optionally a description,
    and (for the PiDP10) the CPU used to run the system.
  - ``panelsim.sh`` - starts a Java panel server,
    then invokes the ``pidp.sh`` script to select and run an O/S
    on the emulated machine.
  - ``pdp.sh`` - intended for the home directory of the ``pidp10`` or ``pidp11`` account,
    this script connects to the ``screen`` session for the active PiDP session.
  - ``pidp.sh`` - repeatedly starts PiDP sessions:
    determines the available operating systems with ``get_selections.sh``,
    gets the current console switch settings with ``getcsw`` or ``scansw10``,
    runs the simh simulator with the boot script of the selected O/S.
    If simh exits with a zero status (success),
    then the process repeats.
    If simh exits with a non-zero status (failure),
    then ``pidp.sh`` exits.
- ``server`` - source for the PiDP servers (Raspberry Pi only):
  - ``server8`` - PDP8I panel for PiDP-8 (untested)
  - ``server10`` - PDP10-KA10 panel for PiDP-10
  - ``server11`` - 11/70 panel for PiDP-11
- ``systemd`` - service files for systemd:
  - ``systemd/pidp10`` - service files for REALCONS and PIPANEL PiDP10
  - ``systemd/pidp11`` - service files for REALCONS PiDP11
- ``systems`` - operating system skeleton directory:
  - ``systems/pidp10`` - idle LEDs standalone programs for PiDP10
  - ``systems/pidp11`` - idle LEDs standalone programs for PiDP11

### System Selections
The operating systems (and standlone programs)
reside in subdirectories of ``/opt/pidpXX/systems``.
In Oscar Vermeulen's implementation,
the file ``/opt/pidpXX/systems/selections`` acts as a directory:
for each system there is a line with
the corresponding switch setting and the subdirectory name.
This implementation uses a different approach.
Each subdirectory contains a ``pidp_info`` file with
metadata for the system:
a set of ``key=value`` pairs,
one per line:
- csw : console switch setting  
  Conventionally this is multiple digits, zero-filled,
  and is interpreted as octal.
  However, if the first digit is not ``0`` it will be
  interpreted as decimal,
  and if it begins with ``0x`` it will be interpreted as hexadecimal.
- desc : description  
  This is a short description of the system.
  If absent, the directory name is used.
- cpu : cpu  
  This specifies the simh CPU that should run this system.
  There are four different PDP10 simulators:
  ``pdp10-ka``, ``pdp10-ki``, ``pdp10-kl``, ``pdp10-ks``.
  If absent, a default CPU (normally ``pdp10-ka`` is assumed.)
  This attribute is not needed on the PiDP11
  which always uses ``pdp11``

The ``/opt/pidpXX/bin/get_selections.sh`` script examines the subdirectories of
``/opt/pidpXX/systems`` and the ``pidp_info`` files within those subdirectories.
It creates a list of all of the available systems.
There are optons to specify the default CPU
(used if none is specified in the ``pidp_info`` file)
and to display the result in a tab-separated format for human consumption
or in a ``key=value`` format
(one line per system)
for consumption by the ``/opt/pidpXX/bin/pidp.sh`` script.

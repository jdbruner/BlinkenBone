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

PDP-11/70 and PDP-8 are available as PiDP servers when
running on Raspberry Pi.
The PiDP11 server is accompanied by associated control files
and supports installation on the current (build) machine.
A PiDP8 server is also built, but to date it is untested.

This is derived from 
https://github.com/j-hoppe/BlinkenBone.
It no longer builds for Blinkenlight panels
nor does it build cross-platform.
For those targets and builds,
consult the following:
- source: https://github.com/j-hoppe/BlinkenBone  
- documentation: https://www.retrocmp.com/projects/blinkenbone
- j_hoppe@t-online.de

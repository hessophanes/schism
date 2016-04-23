SCHISM -- Static Configurator for Hypervisors Including Scenario Metadata
=========================================================================

SCHISM is (c) by Jan Nordholz.

SCHISM is licensed under a
Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You should have received a copy of the license along with this work (see
LICENSE.txt). The license is also available online at
<http://creativecommons.org/licenses/by-nc-sa/4.0/>.

=========================================================================

SCHISM is a hypervisor-implementation-agnostic build system for statically
configurable hypervisors. It provides a mechanism for specifying a desired
scenario for a given platform and architecture as an XML document. That
specification is then passed through several transformation stages, each
one adding more non-mandatory attributes to the XML document, so that
the XML is always well-formed according to the DTD.

The following list provides a high-level overview. Stages with an asterisk
are the few ones that depend on the hypervisor implementation, the CBI
(Configuration Binary Interface); these call out to the appropriate
CBI-specific tool (e.g. generate_config_phidias for the Phidias CBI).

Stage	Deps	Binary		Description
-------------------------------------------------------------------------------------

(-1*)	[]	precompilation	generate build configuration from XML data

(0)	[-1]	(COMPILER)	compile hypervisor binary

(1)	[]	expand		expand and complete XML:
(1a)	[]			* import <arch> and <board> into platform node
(1b)	[]			* add implicit attributes: <guest cpumap>...
(1c)	[]			* add missing <memreq> nodes from default <hypervisor>

(2)	[1]	reparent	reparent nodes and complete memory model:
(2a)	[]			* determine default (i.e. largest) <memory> node
(2b)	[]			* add <map> nodes for <memreq> nodes with forced mapping
(2c)	[]			* reparent all <memreq> nodes to <memory>
(2d)	[]			* validate <map> permission flags

(3*)	[2]	generate_config	generate stub (empty) configuration data
		(COMPILER)

(4)	[2]	measure		import measurable components into XML (new <memreq> and <map> nodes):
(4a)	[0]			* measure core (rx, r, rw, rws)
(4b)	[3]			* measure stub configuration (r, rw, rws)
(4c)	[]			* apply specified size / estimate size of pagetables
(4d)	[]			* measure payload files

(5)	[4]	layout_memory	layout physical space (<memory> nodes)

(6)	[5]	layout_map	layout virtual memory maps (<mmu> nodes)

(7)	[5,6]	pagetables	generate pagetables:
(7a)	[]			* generate PTINIT (choose memreq/map by XML attribute)
(7b)	[]			* generate PTCOREs
(7c)	[]			* compare total size against estimate

(8*)	[7]	generate_config	generate real configuration data
		(COMPILER)

(9)	[8]	combine		concatenate final image:
				* build ( CORE ++ CONFIG ++ PAGETABLES ++ BLOB )
				* wrap into desired format (mkimage / uefi pe / ...)

The source code includes a simple XML library and should compile effortlessly
on any recent Linux distribution - there are no dependencies except on libc.
Compiling the CBI tools for your hypervisor of choice usually requires access
to headers exported by the hypervisor - look for build instructions in the
hypervisor source code to learn how to fulfil those requirements.

The build process is expected to run out-of-tree, i.e. the target directory
is specified using the "O=" make variable. The single input file for the
whole build process is $(O)/scenario.xml. The dependency chain is linked
like this (the authoritative source is of course Makefile.output):

Stage	Inputs				Outputs
-------------------------------------------------------------------------------------
(-1)	scenario.xml			Makeconf
(0)	Makeconf			include/config.h, hypervisor ELF
(1)	scenario.xml			scenario_expanded.xml
(2)	scenario_expanded.xml		scenario_reparented.xml
(3)	scenario_reparented.xml		scenario_config.c, scenario_config.xo
(4)	scenario_reparented.xml,	scenario_measured.xml
	hypervisor ELF,
	scenario_config.xo
(5)	scenario_measured.xml		scenario_p_laidout.xml
(6)	scenario_p_laidout.xml		scenario_v_laidout.xml
(7)	scenario_v_laidout.xml		scenario_pagetables.xml, pagetables
(8)	scenario_pagetables.xml		scenario_config_real.c,
					scenario_config_real.xo
(9)	scenario_pagetables.xml,	final image
	hypervisor ELF,
	scenario_config_real.xo,
	pagetables, blob files

SDC-600 debugger libraries and example
======================================
The Arm CoreSight [SDC-600 Secure Debug Channel](https://developer.arm.com/ip-products/system-ip/coresight-debug-and-trace/coresight-components/coresight-sdc-600-secure-debug-channel), provides a dedicated path to a debugged system for authenticating debug accesses.

The SDC-600 debugger libraries and examples project contains:
* Debugger-side libraries
    * Secure Debug Manager
    * Authentication Token Provider
    * External COM Port driver
    * CSAPBCOM CoreSight interface
* Example application
* [Documentation](https://arm-software.github.io/sdc600-debugger/index.html)

Requirements
------------
* CMake v3.1 or greater.
* RDDI header files and redistributable libraries. These are available in your Arm Development Studio install directory (`<Install Directory>/sw/debugger/RDDI/`).
    * Copy header files to `<this repository>/include/`
    * Copy Redistributable and development libraries to `<this repository>/depends/<platform>/`
        * See individual README.txt located in `<this repository>/depends/<platform>/` for further details.
* [Googletest](https://github.com/google/googletest) project cloned alongside this repository. Only required if you are building tests.
* Configure build option definitions in source:
    * `libs/secure_debug_manager/secure_debug_manager.cpp`
        * `BYPASS_SDM_END` - Whether calls to SDM_End are bypassed and immediately return without error. Useful if the Secure Debug Handler does not implement the link release flags sent by SDM_End. Default: false.

An example directory structure on Linux looks like:

	/
	|- .
	|- ..
	|- sdc600-debugger (this repository)/
		|- depends/
			|- linux-x86_64/
				|- librddi-debug-rvi.so.2
			|- ...
		|- example/
		|- include/
			|- rddi.h
			|- ...
		|- libs/
	|- googletest/
Windows follows a similar structure albeit with a different naming scheme to match the operating system naming convention.

Build (Windows)
--------------
!! Run the following commands inside a Visual Studio command prompt !!
!! Visual Studio 2013 or greater required !!
```
mkdir build
cd build
cmake -G <generator> ..
msbuild /p:Configuration=Release INSTALL.vcxproj
```
`<generator>` can be any available CMake generator. Run `cmake -G` to list all available generators.
Example: `cmake -G "Visual Studio 12 2013 Win64"`

Build (Linux)
------------
```
$ mkdir build
$ cd build
$ cmake ..
$ make
$ make install
```
Build with tests - requires [Googletest](https://github.com/google/googletest)
----------------------------------------
Add the following definitions to the CMake command: `-DBUILD_TESTS=1 -DGOOGLETEST_ROOT=../googletest`

Running the example
-------------------
The example is a simple command line application that calls the SDC-600 debugger libraries to trigger the authentication process for a specified platform. The ``output/`` directory will contain the built example along with the necessary libraries and dependencies.

The example can be run with the following options:
```
$ debugger_example(.exe) <DSTREAM_ADDRESS> <SDF_PATH> <RESET_TYPE> <AP_INDEX>

	DSTREAM_ADDRESS : Address of debug vehicle, prefixed with protocol (TCP:/USB:).
	SDF_PATHFILE : Path to an SDF file describing the target system
	RESET_TYPE :
	    -1 : No reset.
	    0  : SDC-600 COM port reset.
	    >0 : nSRST. Perform system reset via the DAP. Value should be the RDDI device index (index within SDF file) of the system DAP.
	AP_INDEX : RDDI device index (index within SDF file) of the COM-AP or APBCOM device.
```

For example using the MPS3 platform in example/data:
`$ ./debugger_example TCP:<DSTREAM-ADDRESS> example/data/an535_sdc600.sdf 1 29`

:information_source: Note: If connecting to the debug vehicle via USB, additional tools are needed from the Arm Development Studio install directory. Add the following to your PATH environment variable before running the example: `<Install Directory>/sw/debughw/tools2/`.

Arm Development Studio integration
----------------------------------
The ``arm_ds/`` directory contains a manifest.xml file along with the built libraries. The manifest file is used by Arm DS to load the SDC-600 debugger libraries for debug connections. The provided ``manifest.xml`` is for the reference library implementations in this package. The schema for the manifest file is described in ``sdm_manifest.xsd``.

A demo of detecting and unlocking the Arm Secure Debug Channel using Arm Development Studio can be viewed [here](https://developer.arm.com/tools-and-software/embedded/arm-development-studio/learn/resources/media-articles/2019/04/sdc-600).

Support
-------
For enquiries contact: [support-software@arm.com](mailto:support-software@arm.com).

Licence
-------
Arm SDC-600 debugger libraries and example is provided under The University of Illinois/NCSA Open Source License. See LICENCE.txt for more information.

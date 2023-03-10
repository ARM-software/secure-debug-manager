# Secure Debug Manager (PSA-ADAC / SDC-600)

This repository contains an implementation of the [Secure Debug Manager API](https://github.com/ARM-software/sdm-api), which uses the PSA certified [Authenticated Debug Access Control (ADAC)](https://developer.arm.com/documentation/den0101/latest/) specification and the Arm CoreSight [SDC-600 Debug Channel](https://developer.arm.com/ip-products/system-ip/coresight-debug-and-trace/coresight-components/coresight-sdc-600-secure-debug-channel) to provide authenticated debug access.

The Secure Debug Manager build configuration is set up for the [Corstone-1000](https://www.arm.com/products/silicon-ip-subsystems/corstone-1000) platform running [Trusted Firmware-M](https://developer.arm.com/Tools%20and%20Software/Trusted%20Firmware-M) with the [psa-adac library](https://git.trustedfirmware.org/shared/psa-adac.git) integrated. See [build configurations](#secure-debug-manager-build-configurations) for information about porting to another platform.

This Secure Debug Manager project contains:
* The Secure Debug Manager library, which any SDM API supporting debugger can load.
* An example application, which directly drives the Secure Debug Manager library. The example uses the Arm RDDI libraries to implement debug access, supporting the DSTREAM family of debug probes.

:information_source: This repository uses git submodules. Either add the `--recurse-submodules` argument when running `git clone`, or run `git submodule update --init` after cloning.

## Requirements

* CMake v3.1 or later.
* RDDI header files and redistributable libraries. These are available in your Arm Development Studio install directory (`<Install Directory>/sw/debugger/RDDI/`). This is only required if you build the RDDI example.
 * Copy header files to `<this repository>/depends/RDDI/include/`
 * Copy Redistributable and development libraries to `<this repository>/depends/RDDI/lib/<platform>/`
  * See individual README.txt located in `<this repository>/depends/RDDI/lib/<platform>/` for further details.
* [Googletest](https://github.com/google/googletest). This is only required if you build tests.

An example directory structure on Linux:

	/
	|- .
	|- ..
	|- secure_debug_manager (this repository)/
		|- depends/
		    |- RDDI
		        |-include/
		            |- rddi.h
		            |- ...
		        |- libs
		            |- linux-x86_64/
		                |- librddi-debug-rvi.so.2
		    |- sdm-api
		    |- psa-adac
		    |- mbedtls
		|- example/
		|- test/
		|- sdm/
	|- googletest/
Windows follows a similar structure, but with a different naming scheme to match the operating system naming convention.

## Secure Debug Manager Build configurations

Build time configurations for the Secure Debug Manager library are defined in the `sdm/sdm_config.h` header file:

* `SDM_CONFIG_LOCK_ON_CLOSE` - Whether calls to SDM_Close send the Lock Debug command.
 * Type: `bool`
 * Values: `true`, `false`
* `SDM_CONFIG_RESET_ON_CLOSE` - Whether calls to SDM_Close trigger a remote reset.
 * Type: `bool`
 * Values: `true`, `false`
* `SDM_CONFIG_REMOTE_RESET_TYPE` - The remote reset type used to initialize the SDC-600 COM port andthe Secure Debug Authenticator.
 * Type: `ECPDRemoteResetType`
 * Values: `ECPD_REMOTE_RESET_NONE`, `ECPD_REMOTE_RESET_SYSTEM`, `ECPD_REMOTE_RESET_COM`
* `SDM_CONFIG_COM_HW_TX_BLOCKING` - The External COM Port Driver uses hardware blocking rather than status polling.
 * Type: `bool`
 * Values: `true`, `false`
 
The following confiurations are used to build the [`SDMDeviceDescriptor`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L386-L417) that describes the SDC-600 COM port device:
* `SDM_CONFIG_COM_DEVICE_TYPE` - The [`SDMDeviceType`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L387) of the COM port device.
 * Type: `SDMDeviceType`
 * Values: `SDMDeviceType_ArmADI_AP`, `SDMDeviceType_ArmADI_CoreSightComponent`
* `SDM_CONFIG_COM_DEVICE_DP_INDEX` - The value of [`armAp.dpIndex`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L394) or [`armCoreSightComponent.dpIndex`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L406).
 * Type: `uint8_t`
* `SDM_CONFIG_COM_DEVICE_MEMAP_ADDRESS` - The value of [`armCoreSightComponent.memAp->address`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L411). If undefined, `armCoreSightComponent.memAp` is set to `NULL`.
 * Type: `uint64_t` or undefined
* `SDM_CONFIG_COM_DEVICE_ADDRESS` - The value of [`armAp.address`](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L398) or [armCoreSightComponent.baseAddress](https://github.com/ARM-software/sdm-api/blob/0dc678d449f81d3bd4ba09551cbe9d03c209fb86/include/secure_debug_manager.h#L414).
 * Type: `uint64_t`

## Build (Windows)

:information_source: Visual Studio 2015 or later required.
:information_source: Run the following commands inside a Visual Studio command prompt:
```
cmake -S . -B build -G <generator>
cmake --build build/ --target install
```
`<generator>` can be any available CMake generator. Run `cmake -G` to list all available generators. For example:
```
cmake -G "Visual Studio 14 2015 Win64"`
```

## Build (Linux)

```
$ cmake -S . -B build
$ cmake --build build/ --target install
```

## Extra CMake options

By default, CMake will only generate build files for the Secure Debug Manager library. You can enable other components with the following CMake options:

* `-DRDDI_EXAMPLE=TRUE` - Builds the RDDI example application.
* `-DTEST=TRUE` - Builds the unit tests. "This option also requires `-DGOOGLETEST_ROOT=<path to googletest source>`.

For example:
```
cmake -S . -B build -DRDDI_EXAMPLE=TRUE -DTESTS=TRUE -DGOOGLETEST_ROOT=..\googletest
```

## Running the example

The example is a simple command line application that calls the Secure Debug Manager libraries to run the authentication process. The ``output/`` directory will contain the built example along with the necessary libraries and dependencies.

You can run the example with the following options:
```
$ debugger_example(.exe) <DSTREAM_ADDRESS> <SDF_PATHFILE> <DAP_INDEX> <COM_INDEX>

	DSTREAM_ADDRESS : Address of debug vehicle, prefixed with protocol (TCP:/USB:).
	SDF_PATHFILE : Path to an SDF file describing the target system
	DAP_INDEX : RDDI device index (index within SDF file) of the system DAP.
	COM_INDEX : RDDI device index (index within SDF file) of the COM-AP or APBCOM device.
```

For example using the [MPS3 Corstone-1000](https://developer.arm.com/documentation/dai0550/latest/) platform in `example/data/`:
```
$ ./debugger_example TCP:<DSTREAM-ADDRESS> ../example/data/sdf/MPS3_Corstone-1000.sdf 1 26
```

:information_source: If you are connecting to the debug vehicle with USB, additional tools are needed from the Arm Development Studio install directory. Before running the example, add the following to your PATH environment variable: `<Install Directory>/sw/debughw/tools2/`.

During the authentication process, you need to supply trust chain and key file paths. For example:
```
...
User input: Cretentials form

Please provide private key file path: :
<this repository>\example\data\keys\EcdsaP256Key-3.pem
Please provide trust chain file path: :
<this repository>\example\data\chains\chain.EcdsaP256-3
...
```

:information_source: These are dummy keys used for testing and SHOULD NOT be used in production.

On completion of the authentication process, the application terminates. If successful, the following message displays:
```
System is open for debug
```

## Arm Development Studio integration

Arm Development Studio 2022.2 and 2022.c adds support for the Secure Debug Manager API.

The ``arm_ds`` directory contains an Arm DS Secure Debug Manager enabled debug configuration for the MPS3 Corstone-1000 platform. The built library is installed in this directory to complete the configuration. You can import this full configuration into Arm Development Studio to create a debug connection.

For more information, see [Secure Debug Manager](https://developer.arm.com/documentation/101470/latest/Platform-Configuration/Platform-Configuration-and-the-PCE/Secure-Debug-Manager) and [Add an SDM to a debug configuration](https://developer.arm.com/documentation/101470/latest/Platform-Configuration/Platform-Configuration-and-the-PCE/Add-an-SDM-to-a-debug-configuration) in the Arm Development Studio User Guide.

## Support

For enquiries, contact: [support-software@arm.com](mailto:support-software@arm.com).

## Licence

Secure Debug Manager is provided under The University of Illinois/NCSA Open Source License. See LICENCE.txt for more information.

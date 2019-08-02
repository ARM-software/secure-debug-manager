
# SDC-600 Debugger Interfaces

![SDC-600 Debugger components](../res/debugger-components.png "SDC-600 Debugger components")

## Secure Debug Manager @ref secure_debug_manager.h

The Secure Debug Manager is responsible to open the remote debugged system for debugging
using the debugger user’s credentials. It is the library entry point for a debugger to
trigger authentication.

## Authentication Token Provider @ref auth_token_provider.h

Provides an authentication token to the Secure Debug Manager. May require communication to an
external authorization server or a simple fixed certificate.
Tokens may be certificate chain based, certificate chain based with challenge response capability,
symmetric key based, password based, etc.

## External COM Port Driver @ref ext_com_port_driver.h

SDC-600 External COM Port driver runs in the debugger platform and it owns and
manages the SDC-600 External COM port at the debugged system.
The driver serves a Secure Debug Manager component in the debugger and acts per its needs. 
The driver does not transmit or receive by itself. The driver provides some APIs to the
Secure Debug Manager.

## CSAPBCOM (I/O driver) @ref csapbcom.h

The CSAPBCOM interface an implementation of the I/O driver layer to allow reading/writing of generic
data to an ARM CoreSight&tm; COM-AP or APBCOM device, via Arm DSTREAM family debug probes.

Further to data reads/writes, system reset functionality via the debugged systems DAP is provided. The
debugger therefore does not have to provide system reset callbacks for the External COM Port Driver if
it is known that the debugged system implements secure debug certificate processing in ROM.

// sdm_config.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

/*
 * SDM build configurations.
 *
 * Current values reflect the capabilities and layout of the Arm MPS3 Corstone-1000 (AN550)
 * platform running Trusted Firmware-M.
 */

#ifndef SDM_CONFIG_H
#define SDM_CONFIG_H

#include "secure_debug_manager.h"

/*--------------------------------------------------------------*/
/* Whether calls to SDM_Close send the Lock Debug command.      */
/* If the Secure Debug Autenticator is implemented in ROM boot, */
/* it is not possible to receive the Lock Debug command once    */
/* passed boot.                                                 */
/*                                                              */
/* Type: bool                                                   */
/* Values: true, false                                          */
/*--------------------------------------------------------------*/
#define SDM_CONFIG_LOCK_ON_CLOSE false

/*--------------------------------------------------------------*/
/* Whether calls to SDM_Close trigger a remote reset.           */
/* Can be used as an alternative to the Lock Debug command,     */
/* returning permissions back to default values.                */
/*                                                              */
/* Type: bool                                                   */
/* Values: true, false                                          */
/*--------------------------------------------------------------*/
#define SDM_CONFIG_RESET_ON_CLOSE false

/*--------------------------------------------------------------*/
/* The remote reset type used to initialize the SDC-600 COM     */
/* port andthe Secure Debug Authenticator.                      */
/*                                                              */
/* If the Secure Debug Authenticator is implemented in ROM boot,*/
/* a reset will be required. Runtime authentication does not    */
/* rquire a reset.                                              */
/*                                                              */
/* Note it is IMPLEMENTATION DEFINED whther a platform supports */
/* COM port remote reset (REMRR).                               */
/*                                                              */
/* Type: ECPDRemoteResetType                                    */
/* Values: ECPD_REMOTE_RESET_NONE,                              */
/*         ECPD_REMOTE_RESET_SYSTEM,                            */
/*         ECPD_REMOTE_RESET_COM                                */
/*--------------------------------------------------------------*/
#define SDM_CONFIG_REMOTE_RESET_TYPE ECPD_REMOTE_RESET_SYSTEM

/*--------------------------------------------------------------*/
/* The External COM Port Driver uses hardware blocking, sending */
/* data via the Data Blocking Register (DBR),                   */
/* rather than the non-blocking Data Register (DR) and checking */
/* the Status Register (SR) for TX FIFO status.                 */
/* Disable if the debug vehicle is not capable of long hardware */
/* blocking periods, disabling will have a performance impact.  */
/*                                                              */
/* Type: bool                                                   */
/* Values: true, false                                          */
/*--------------------------------------------------------------*/
#define SDM_CONFIG_COM_HW_TX_BLOCKING true

/*-------------------------------------------------------------*/
/* SDMDeviceDescriptor elemnts describing the SDC-600 COM port */
/*-------------------------------------------------------------*/

/*-------------------------------------------------------------*/
/* The SDMDeviceType of the COM port device                    */
/*                                                             */
/* Type: SDMDeviceType                                         */
/* Values: SDMDeviceType_ArmADI_AP,                            */
/*         SDMDeviceType_ArmADI_CoreSightComponent             */
/*-------------------------------------------------------------*/
#define SDM_CONFIG_COM_DEVICE_TYPE SDMDeviceType_ArmADI_CoreSightComponent;

/*-------------------------------------------------------------*/
/* The value of armAp.dpIndex or armCoreSightComponent.dpIndex */
/*                                                             */
/* Type: uint8_t                                               */
/*-------------------------------------------------------------*/
#define SDM_CONFIG_COM_DEVICE_DP_INDEX 0

/*-------------------------------------------------------------*/
/* The value of armCoreSightComponent.memAp->address. If       */
/* undefined, armCoreSightComponent.memAp is set to NULL.      */
/*                                                             */
/* Type: uint64_t or undefined                                 */
/*-------------------------------------------------------------*/
//#define SDM_CONFIG_COM_DEVICE_MEMAP_ADDRESS 0x00004000

/*-------------------------------------------------------------*/
/* The value of armAp.address or                               */
/* armCoreSightComponent.baseAddress                           */
/*                                                             */
/* Type: uint64_t                                              */
/*-------------------------------------------------------------*/
#define SDM_CONFIG_COM_DEVICE_ADDRESS 0x0020000

#endif // SDM_CONFIG_H

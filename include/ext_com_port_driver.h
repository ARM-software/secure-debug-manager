// ext_com_port_driver.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

 /**
 * \file
 *
 * \brief This header file is the entry point to External COM Port Driver
 * functionality and includes all necessary definitions.
 */

#ifndef EXT_COM_PORT_DRIVER_H_
#define EXT_COM_PORT_DRIVER_H_

#include "secure_debug_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef EXT_COM_PORT_EXPORT_SYMBOLS
    #define EXT_COM_PORT_EXTERN __declspec(dllexport)
#else
    #define EXT_COM_PORT_EXTERN __declspec(dllimport)
#endif
#else
#define EXT_COM_PORT_EXTERN
#endif

/**
* \brief SDC-600 COM port protocol flag bytes
*
* Arm Advanced Communications Channel Architecture Specification (ARM IHI 0076) compliant
*/
typedef enum ECPDProtFlagBytes {
    FLAG_IDR = 0xA0, /*!< Identification request*/
    FLAG_IDA = 0xA1, /*!< Identification acknowledge */
    FLAG_LPH1RA = 0xA6, /*!< Link phase 1 request/acknowledge */
    FLAG_LPH1RL = 0xA7, /*!< Link phase 1 release/acknowledge */
    FLAG_LPH2RA = 0xA8, /*!< Link phase 2 request/acknowledge */
    FLAG_LPH2RL = 0xA9, /*!< Link phase 2 release/acknowledge */
    FLAG_LPH2RR = 0xAA, /*!< Link phase 2 reboot request */
    FLAG_LERR = 0xAB, /*!< Link error */
    FLAG_START = 0xAC, /*!< Start of Protocol Data Unit */
    FLAG_END = 0xAD, /*!< End of Protocol Data Unit*/
    FLAG_ESC = 0xAE, /*!< Escape */
    FLAG__NULL = 0xAF /*!< Null */
} ECPDProtFlagBytes;


/**
* \brief Required states for {@link EComPort_Power}
*/
EXT_COM_PORT_EXTERN typedef enum ECPDRequiredState {
    ECPD_POWER_OFF, /*!< Release COM port link power */
    ECPD_POWER_ON /*!< Setup COM port link power */
} ECPDRequiredState;

/**
* \brief Return codes for ECPD_* functionality.
*/
EXT_COM_PORT_EXTERN typedef enum ECPDReturnCode {
    ECPD_SUCCESS, /*!< Success, no error */
    ECPD_TIMEOUT,  /*!< No response, timeout */
    ECPD_UNEXPECTED_SYMBOL, /*!< Unexpected symbol received */
    ECPD_LINK_ERR, /*!< Link dropped during transmit */
    ECPD_NO_INTI, /*!< {@link EComPort_Init} was not called, or called and returned and error */
    ECPD_BUFFER_OVERFLOW, /*!< Provided buffer is too small for message received */
    ECPD_TX_FAIL, /*!< Failed to transmit data from the SDC-600 COM port device - e.g. TX FIFO doesn not drain */
    ECPD_RX_FAIL /*! Failed to receive data from the SDC-600 COM port device */
} ECPDReturnCode;

/**
 * This function is called by the Secure Debug Manager to initiate the External
 * COM port driver and the COM port link. It supports an option to remote reboot
 * the debugged system using resetType before it starts the communication (this
 * option is used when the secure debug certificate shall be introduced at early
 * boot while the remote platform is already running and the DCUs are locked at
 * ROM exit).
 *
 * Upon success, the IDResponseBuffer will hold the IDA response of the remote
 * platform (6 bytes). This is used to detect what high level protocol the
 * remote system supports. In the secure debug case the protocol is ‘TBD’.
 * IDBufferLength must be 6.
 *
 * @param[in] resetType Required reset type. Should be the same as used with
                        Secure Debug Manager {@link SDM_Init}.
 * @param[out] idResponseBuffer Client supplied buffer to receive the IDA response.
 * @param[in] idBufferLength Size in bytes of the IDA response client supplied buffer.
 * @param[in] pDebugIF The same SDMDebugIf as the Secure Debug Manger received
 *                     from the debugger at {@link SDM_Init}, with the same callbacks
 *                     and connection details.
 */
EXT_COM_PORT_EXTERN ECPDReturnCode EComPort_Init(SDMResetType resetType,
                       uint8_t* idResponseBuffer,
                       size_t idBufferLength,
                       SDMDebugIf* pDebugIF);


/**
 * This internal function is called by {@link EComPort_Init} and by {@link SDM_End}.
 * Internal CurrentPowerState should be maintained (default is off).
 *
 * @param[in] requiredState Required power state.
 */
EXT_COM_PORT_EXTERN ECPDReturnCode EComPort_Power(ECPDRequiredState requiredState);

/**
 * This function is called by {@link SDM_End} and by the {@link EComPort_Init} to remote reboot the
 * debugged system when the DCUs are locked at ROM exit.
 */
EXT_COM_PORT_EXTERN ECPDReturnCode EComPort_RReboot(void);

/**
 * The transmit side of the driver receives from the Secure Debug Manager a message
 * to transmit in the provided buffer. The transmitter is transparent to the caller
 * and the provided buffer may include any byte values (including values that are
 * SDC-600 flag bytes). The EComPort_Tx provides transparent transmit interface.
 *
 * The {@link EComPort_Tx} function is responsible to inject to the External COM Port
 * transmit FIFO HW a FLAG_START flag, followed by the message bytes from the buffer and
 * at the end it is responsible to inject to the transmit HW an FLAG_END flag. While
 * transmitting any byte from the buffer, the driver must detect if it is one of
 * the SDC-600 COM Port flag values (bytes with an upper 3 bits of b101 are classified as Flag bytes).
 * In such case the driver must inject ESC flag to the transmitter and flips the MS
 * bit of the byte and transmits the modified byte.
 *
 * In case during the transmit the driver detects timeout (debugged system does not
 * consume its RX FIFO) the transmitter stops and returns an error.
 *
 * Secure Debug Manager should only call this after a successful {@link SDM_Init},
 * meaning that the Internal COM port is powered, its link is set, its communication
 * protocol verified and its driver is ready for further communication.
 *
 * @param[in] txBuffer Transmit buffer.
 * @param[in] txBufferLength Size in bytes of the transmit buffer.
 * @param[out] actualLength Updated with the number of bytes consumed
               from the TxBuffer, it should be equal to TxBufferLength.
               Does not count the FLAG_START and FLAG_END flags.
 */
EXT_COM_PORT_EXTERN ECPDReturnCode EComPort_Tx(uint8_t* txBuffer, size_t txBufferLength, size_t* actualLength);

/**
 * At its receive side, the External COM port driver receives from the SDC-600
 * External COM port receiver a protocol message (which is stuffed by the required
 * FLAG_ESC flag bytes) that starts with FLAG_START and ends with FLAG_END.
 * The receive side of the driver strips off the FLAG_START and FLAG_END of message flags and
 * writes just the message content to the buffer it received from the Secure Debug Manager.
 * While receiving, when the receiver detects ESC flag it drops it and replace the following
 * received byte to its original value by flipping the MS bit.
 *
 * In case the receiver detects a message that does not start with FLAG_START
 * it drops it and reports an RX error.
 *
 * In case the receiver detects a LPH2RL flag it drops it and reports link dropper RX error.
 *
 * In case the receiver detects a message that starts with FLAG_START but it filled the receive
 * buffer to its end prior to the detection of FLAG_END, it drops it and reports an RX length error.
 *
 * Secure Debug Manager should only call this after a successful {@link SDM_Init},
 * meaning that the Internal COM port is powered, its link is set, its communication
 * protocol verified and its driver is ready for further communication.
 *
 * @param[out] rxBuffer Client supplied buffer to receive data.
 * @param[in] rxBufferLength Size in bytes of the receive client supplied buffer.
 * @param[in] actualLength Updated with number of bytes filled in the rxBuffer, it should be less
                           or equal to rxBufferLength. Does not count the FLAG_START, intermediate FLAG_ESC
                           and FLAG_END flags.
 */
EXT_COM_PORT_EXTERN ECPDReturnCode EComPort_Rx(uint8_t* rxBuffer, size_t rxBufferLength, size_t* actualLength);

#ifdef __cplusplus
}
#endif


#endif /* EXT_COM_PORT_DRIVER_H_ */
